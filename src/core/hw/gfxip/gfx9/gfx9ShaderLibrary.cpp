/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 **********************************************************************************************************************/

#include "core/hw/gfxip/gfx9/gfx9ShaderLibrary.h"
#include "core/hw/gfxip/gfx9/g_gfx9PalSettings.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "palMsgPack.h"
#include "palMsgPackImpl.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{
// =====================================================================================================================
ShaderLibrary::ShaderLibrary(Device* pDevice)
    :
    Pal::ShaderLibrary(pDevice->Parent()),
    m_pClientData(nullptr),
    m_pDevice(pDevice),
    m_chunkCs(*pDevice),
    m_pFunctionList(nullptr),
    m_funcCount(0)
{
}

// =====================================================================================================================
ShaderLibrary::~ShaderLibrary()
{
    if (m_pFunctionList != nullptr)
    {
        for (uint32 i = 0; i < m_funcCount; ++i)
        {
            PAL_FREE(m_pFunctionList[i].pSymbolName, m_pDevice->GetPlatform());
        }
        PAL_SAFE_DELETE_ARRAY(m_pFunctionList, m_pDevice->GetPlatform());
    }
}

// =====================================================================================================================
// Check wavefront size and set the m_hwInfo.flags.isWave32 flag
void ShaderLibrary::SetIsWave32(
    const CodeObjectMetadata& metadata)
{
    // We don't bother checking the wavefront size for pre-Gfx10 GPU's since it is implicitly 64 before Gfx10. Any ELF
    // which doesn't specify a wavefront size is assumed to use 64, even on Gfx10 and newer.
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();
    if (IsGfx10Plus(chipProps.gfxLevel))
    {
        const auto& csMetadata = metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Cs)];
        if (csMetadata.hasEntry.wavefrontSize != 0)
        {
            PAL_ASSERT((csMetadata.wavefrontSize == 64) || (csMetadata.wavefrontSize == 32));
            m_hwInfo.flags.isWave32 = (csMetadata.wavefrontSize == 32);
        }
    }
}

// =====================================================================================================================
// Initializes HW-specific state related to this shader library object (register values, user-data mapping, etc.)
// using the specified library ABI processor.
Result ShaderLibrary::HwlInit(
    const ShaderLibraryCreateInfo& createInfo,
    const AbiReader&               abiReader,
    const CodeObjectMetadata&      metadata,
    Util::MsgPackReader*           pMetadataReader)
{
    const Gfx9PalSettings&   settings  = m_pDevice->Settings();
    const CmdUtil&           cmdUtil   = m_pDevice->CmdUtil();
    const auto&              regInfo   = cmdUtil.GetRegInfo();
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    RegisterVector registers(m_pDevice->GetPlatform());
    Result result = pMetadataReader->Seek(metadata.pipeline.registers);

    if (result == Result::Success)
    {
        result = pMetadataReader->Unpack(&registers);
    }

    ShaderLibraryUploader uploader(m_pDevice, abiReader);

    if (result == Result::Success)
    {
        // Next, handle relocations and upload the library code & data to GPU memory.
        result = PerformRelocationsAndUploadToGpuMemory(
            metadata,
            (createInfo.flags.overrideGpuHeap == 1) ? createInfo.preferredHeap : GpuHeapInvisible,
            &uploader);
        SetIsWave32(metadata);
    }

    if (result == Result::Success)
    {
        const uint32 wavefrontSize = IsWave32() ? 32 : 64;

        m_chunkCs.LateInit<ShaderLibraryUploader>(abiReader,
                                                  registers,
                                                  wavefrontSize,
                                                  createInfo.pFuncList,
                                                  createInfo.funcCount,
                                                  &uploader);

        UpdateHwInfo();
        PAL_ASSERT(m_uploadFenceToken == 0);
        result = uploader.End(&m_uploadFenceToken);
    }

    if (result == Result::Success)
    {
        ResourceDescriptionShaderLibrary desc = {};
        desc.pLibrarynfo   = &GetInfo();
        desc.pCreateFlags  = &createInfo.flags;
        ResourceCreateEventData data = {};
        data.type = ResourceType::Pipeline;
        data.pResourceDescData = &desc;
        data.resourceDescSize = sizeof(ResourceDescriptionShaderLibrary);
        data.pObj = this;
        m_pDevice->GetPlatform()->GetEventProvider()->LogGpuMemoryResourceCreateEvent(data);

        GpuMemoryResourceBindEventData bindData = {};
        bindData.pObj = this;
        bindData.pGpuMemory = m_gpuMem.Memory();
        bindData.requiredGpuMemSize = m_gpuMemSize;
        bindData.offset = m_gpuMem.Offset();
        m_pDevice->GetPlatform()->GetEventProvider()->LogGpuMemoryResourceBindEvent(bindData);
    }

    if (result == Result::Success)
    {
        m_funcCount = createInfo.funcCount;
        m_pFunctionList = PAL_NEW_ARRAY(ShaderLibraryFunctionInfo,
                                        m_funcCount,
                                        m_pDevice->GetPlatform(),
                                        AllocInternal);
        if (m_pFunctionList == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }

        if (result == Result::Success)
        {
            for (uint32 i = 0; i < m_funcCount; ++i)
            {
                memset(&m_pFunctionList[i], 0, sizeof(ShaderLibraryFunctionInfo));

                // GPU VA should never be 0
                PAL_ASSERT(createInfo.pFuncList[i].gpuVirtAddr != 0);

                m_pFunctionList[i].gpuVirtAddr = createInfo.pFuncList[i].gpuVirtAddr;

                size_t nameLength = strlen(createInfo.pFuncList[i].pSymbolName) + 1;
                char* pSymbolName =
                    static_cast<char*>(PAL_MALLOC(nameLength, m_pDevice->GetPlatform(), AllocInternal));
                if (pSymbolName != nullptr)
                {
                    strncpy(pSymbolName, createInfo.pFuncList[i].pSymbolName, nameLength);
                    m_pFunctionList[i].pSymbolName = pSymbolName;
                }
                else
                {
                    result = Result::ErrorOutOfMemory;
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Update local HwInfo struct, in case later during LinkLibrary phase need to read these value out and update
// the main shader register values.
void ShaderLibrary::UpdateHwInfo()
{
    m_hwInfo.libRegs.computePgmRsrc1 = m_chunkCs.LibHWInfo().computePgmRsrc1;
    m_hwInfo.libRegs.computePgmRsrc2 = m_chunkCs.LibHWInfo().dynamic.computePgmRsrc2;
    m_hwInfo.libRegs.computePgmRsrc3 = m_chunkCs.LibHWInfo().computePgmRsrc3;
}

// =====================================================================================================================
// Obtains the compiled shader ISA code for the shader specified.
Result ShaderLibrary::GetShaderFunctionCode(
    const char*  pShaderExportName,
    size_t*      pSize,
    void*        pBuffer
    ) const
{
    // To extract the shader code, we can re-parse the saved ELF binary and lookup the shader's program
    // instructions by examining the symbol table entry for that shader's entrypoint.
    AbiReader abiReader(m_pDevice->GetPlatform(), m_pCodeObjectBinary);
    Result result = abiReader.Init();
    if (result == Result::Success)
    {
        const Elf::SymbolTableEntry* pSymbol = abiReader.GetGenericSymbol(pShaderExportName);
        if (pSymbol != nullptr)
        {
            result = abiReader.GetElfReader().CopySymbol(*pSymbol, pSize, pBuffer);
        }
        else
        {
            result = Result::ErrorUnavailable;
        }
    }

    return result;
}

// =====================================================================================================================
// Obtains the shader pre and post compilation stats/params for the specified shader.
Result ShaderLibrary::GetShaderFunctionStats(
    const char*      pShaderExportName,
    ShaderLibStats*  pShaderStats) const
{
    Result result = Result::Success;

    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    PAL_ASSERT(pShaderStats != nullptr);
    memset(pShaderStats, 0, sizeof(ShaderLibStats));

    const auto&  gpuInfo       = m_pDevice->Parent()->ChipProperties();

    pShaderStats->common.numUsedSgprs          = m_hwInfo.libRegs.computePgmRsrc1.bits.SGPRS;
    pShaderStats->common.numUsedVgprs          = m_hwInfo.libRegs.computePgmRsrc1.bits.VGPRS;
    pShaderStats->common.ldsUsageSizeInBytes   = m_hwInfo.libRegs.computePgmRsrc2.bits.LDS_SIZE;
    pShaderStats->palInternalLibraryHash       = m_info.internalLibraryHash;
    pShaderStats->common.ldsSizePerThreadGroup = chipProps.gfxip.ldsSizePerThreadGroup;
    pShaderStats->common.flags.isWave32        = m_hwInfo.flags.isWave32;

    // We can re-parse the saved pipeline ELF binary to extract shader statistics.
    AbiReader abiReader(m_pDevice->GetPlatform(), m_pCodeObjectBinary);
    result = abiReader.Init();
    if (result == Result::Success)
    {
        const Elf::SymbolTableEntry* pSymbol = abiReader.GetGenericSymbol(pShaderExportName);
        if (pSymbol != nullptr)
        {
            pShaderStats->isaSizeInBytes = static_cast<size_t>(pSymbol->st_size);
        }
    }

    MsgPackReader      metadataReader;
    CodeObjectMetadata metadata;

    if (result == Result::Success)
    {
        result = abiReader.GetMetadata(&metadataReader, &metadata);
    }

    if (result == Result::Success)
    {
        const auto&  stageMetadata = metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Cs)];

        pShaderStats->numAvailableSgprs = (stageMetadata.hasEntry.sgprLimit != 0)
                                            ? stageMetadata.sgprLimit
                                            : gpuInfo.gfx9.numShaderVisibleSgprs;
        pShaderStats->numAvailableVgprs = (stageMetadata.hasEntry.vgprLimit != 0)
                                               ? stageMetadata.vgprLimit
                                               : MaxVgprPerShader;

        pShaderStats->common.scratchMemUsageInBytes = stageMetadata.scratchMemorySize;
    }

    if (result == Result::Success)
    {
        result = UnpackStackFrameSize(pShaderExportName,
                                      metadata,
                                      &metadataReader,
                                      &pShaderStats->stackFrameSizeInBytes);
    }

    return result;
}

// =====================================================================================================================
// Obtains the shader function stack frame size
Result ShaderLibrary::UnpackStackFrameSize(
    const char*               pShaderExportName,
    const CodeObjectMetadata& metadata,
    Util::MsgPackReader*      pMetadataReader,
    uint32*                   pStackFrameSizeInBytes
    ) const
{
    Result result = pMetadataReader->Seek(metadata.pipeline.shaderFunctions);

    if (result == Result::Success)
    {
        result    = (pMetadataReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;
        const auto& item = pMetadataReader->Get().as;

        for (uint32 i = item.map.size; ((result == Result::Success) && (i > 0)); --i)
        {
            ShaderFuncStats stats;

            result = pMetadataReader->Next(CWP_ITEM_STR);
            if (result == Result::Success)
            {
                stats.symbolNameLength = item.str.length;
                stats.pSymbolName      = static_cast<const char*>(item.str.start);
            }

            if (result == Result::Success)
            {
                result = pMetadataReader->Next(CWP_ITEM_MAP);
            }

            for (uint32 j = item.map.size; ((result == Result::Success) && (j > 0)); --j)
            {
                result = pMetadataReader->Next(CWP_ITEM_STR);

                if (result == Result::Success)
                {
                    switch (HashString(static_cast<const char*>(item.str.start), item.str.length))
                    {
                    case HashLiteralString(".stack_frame_size_in_bytes"):
                    {
                        result = pMetadataReader->UnpackNext(&stats.stackFrameSizeInBytes);
                        if ((result == Result::Success) &&
                            (strncmp(pShaderExportName, stats.pSymbolName, stats.symbolNameLength) == 0) &&
                            (strlen(pShaderExportName) == stats.symbolNameLength))
                        {
                            *pStackFrameSizeInBytes = stats.stackFrameSizeInBytes;
                        }
                    }
                    break;

                    default:
                        result = pMetadataReader->Skip(1);
                       break;
                    }
                }
            }

        }
    }

    return result;
}

} // namespace Gfx9

} // namespace Pal
