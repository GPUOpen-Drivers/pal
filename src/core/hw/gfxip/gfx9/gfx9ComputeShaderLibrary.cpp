/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx9/gfx9ComputeShaderLibrary.h"
#include "g_gfx9Settings.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "palMsgPackImpl.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

static_assert(
    ShaderSubType(Abi::ApiShaderSubType::Unknown)              == ShaderSubType::Unknown              &&
    ShaderSubType(Abi::ApiShaderSubType::Traversal)            == ShaderSubType::Traversal            &&
    ShaderSubType(Abi::ApiShaderSubType::RayGeneration)        == ShaderSubType::RayGeneration        &&
    ShaderSubType(Abi::ApiShaderSubType::Intersection)         == ShaderSubType::Intersection         &&
    ShaderSubType(Abi::ApiShaderSubType::AnyHit)               == ShaderSubType::AnyHit               &&
    ShaderSubType(Abi::ApiShaderSubType::ClosestHit)           == ShaderSubType::ClosestHit           &&
    ShaderSubType(Abi::ApiShaderSubType::Miss)                 == ShaderSubType::Miss                 &&
    ShaderSubType(Abi::ApiShaderSubType::Callable)             == ShaderSubType::Callable             &&
    ShaderSubType(Abi::ApiShaderSubType::LaunchKernel)         == ShaderSubType::LaunchKernel         &&
    ShaderSubType(Abi::ApiShaderSubType::Count)                == ShaderSubType::Count,
    "Mismatch in member/s between Abi::ApiShaderSubType and Pal::ShaderSubType!");

// =====================================================================================================================
ComputeShaderLibrary::ComputeShaderLibrary(
    Device* pDevice)
    :
    Pal::ComputeShaderLibrary(pDevice->Parent()),
    m_pDevice(pDevice),
    m_signature(NullCsSignature),
    m_chunkCs(*pDevice, &m_stageInfoCs, nullptr),
    m_stageInfoCs{ }
{
    m_stageInfoCs.stageId = Abi::HardwareStage::Cs;
}

// =====================================================================================================================
// Initializes HW-specific state related to this shader library object (register values, user-data mapping, etc.)
// using the specified library ABI processor.
Result ComputeShaderLibrary::HwlInit(
    const ShaderLibraryCreateInfo&    createInfo,
    const AbiReader&                  abiReader,
    const PalAbi::CodeObjectMetadata& metadata,
    Util::MsgPackReader*              pMetadataReader)
{
    PipelineUploader uploader(m_pDevice->Parent(), abiReader);

    // Handle relocations and upload the library code & data to GPU memory.
    Result result = PerformRelocationsAndUploadToGpuMemory(
        metadata,
        m_pDevice->Parent()->GetPublicSettings()->pipelinePreferredHeap, // ShaderLibrary is never internal
        &uploader);

    if (result == Result::Success)
    {
        result = InitFunctionListFromMetadata(metadata, pMetadataReader);
    }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 827
    if (result == Result::Success)
    {
        // NOTE: To preserve the behavior before interface change, which is clients get what they pass in through
        // createInfo.pFuncList when calling GetShaderLibFunctionList(), fill function list with given one here.
        m_functionList.Clear();
        for (uint32 i = 0; ((i < createInfo.funcCount) && (result == Result::Success)); i++)
        {
            size_t nameLength = strlen(createInfo.pFuncList[i].pSymbolName) + 1;
            char* pSymbolName = static_cast<char*>(PAL_MALLOC(nameLength, m_pDevice->GetPlatform(), AllocInternal));
            if (pSymbolName != nullptr)
            {
                Util::Strncpy(pSymbolName, createInfo.pFuncList[i].pSymbolName, nameLength);
                ShaderLibraryFunctionInfo info = { pSymbolName, 0 };
                result = m_functionList.PushBack(info);
            }
            else
            {
                result = Result::ErrorOutOfMemory;
            }
        }
    }
#endif

    if (result == Result::Success)
    {
        // Update the pipeline signature with user-mapping data contained in the ELF:
        m_chunkCs.SetupSignatureFromElf(&m_signature, metadata);

        DispatchDims threadsPerTg;
        m_chunkCs.LateInit(metadata,
                           (IsWave32() ? 32 : 64),
                           &threadsPerTg,
#if PAL_BUILD_GFX11
                           DispatchInterleaveSize::Default,
#endif
                           &uploader);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 827
        GetFunctionGpuVirtAddrs(uploader, createInfo.pFuncList, createInfo.funcCount);
#endif
        // Must be called after InitFunctionListFromMetadata!
        GetFunctionGpuVirtAddrs(uploader, m_functionList.Data(), m_functionList.NumElements());

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
        m_pDevice->GetPlatform()->GetGpuMemoryEventProvider()->LogGpuMemoryResourceCreateEvent(data);

        GpuMemoryResourceBindEventData bindData = {};
        bindData.pObj = this;
        bindData.pGpuMemory = m_gpuMem.Memory();
        bindData.requiredGpuMemSize = m_gpuMemSize;
        bindData.offset = m_gpuMem.Offset();
        m_pDevice->GetPlatform()->GetGpuMemoryEventProvider()->LogGpuMemoryResourceBindEvent(bindData);

        Developer::BindGpuMemoryData callbackData = {};
        callbackData.pObj               = bindData.pObj;
        callbackData.requiredGpuMemSize = bindData.requiredGpuMemSize;
        callbackData.pGpuMemory         = bindData.pGpuMemory;
        callbackData.offset             = bindData.offset;
        callbackData.isSystemMemory     = bindData.isSystemMemory;
        m_pDevice->Parent()->DeveloperCb(Developer::CallbackType::BindGpuMemory, &callbackData);
    }

    return result;
}

// =====================================================================================================================
// Update local HwInfo struct, in case later during LinkLibrary phase need to read these value out and update
// the main shader register values.
void ComputeShaderLibrary::UpdateHwInfo()
{
    m_hwInfo.libRegs.computePgmRsrc1 = m_chunkCs.HwInfo().computePgmRsrc1;
    m_hwInfo.libRegs.computePgmRsrc2 = m_chunkCs.HwInfo().dynamic.computePgmRsrc2;
    m_hwInfo.libRegs.computePgmRsrc3 = m_chunkCs.HwInfo().computePgmRsrc3;
}

// =====================================================================================================================
// Obtains the compiled shader ISA code for the shader specified.
Result ComputeShaderLibrary::GetShaderFunctionCode(
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
Result ComputeShaderLibrary::GetShaderFunctionStats(
    const char*      pShaderExportName,
    ShaderLibStats*  pShaderStats) const
{
    Result result = Result::Success;

    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    PAL_ASSERT(pShaderStats != nullptr);
    memset(pShaderStats, 0, sizeof(ShaderLibStats));

    pShaderStats->palInternalLibraryHash       = m_info.internalLibraryHash;
    pShaderStats->common.ldsSizePerThreadGroup = chipProps.gfxip.ldsSizePerThreadGroup;
    pShaderStats->common.flags.isWave32        = IsWave32();

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

    MsgPackReader              metadataReader;
    PalAbi::CodeObjectMetadata metadata;

    if (result == Result::Success)
    {
        result = abiReader.GetMetadata(&metadataReader, &metadata);
    }

    if (result == Result::Success)
    {
        const auto&  stageMetadata = metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Cs)];

        const auto& gpuInfo = m_pDevice->Parent()->ChipProperties();

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
        result = UnpackShaderFunctionStats(pShaderExportName,
                                           metadata,
                                           &metadataReader,
                                           pShaderStats);
    }

    return result;
}

// =====================================================================================================================
// Obtains the shader function stack frame size
Result ComputeShaderLibrary::UnpackShaderFunctionStats(
    const char*                       pShaderExportName,
    const PalAbi::CodeObjectMetadata& metadata,
    Util::MsgPackReader*              pMetadataReader,
    ShaderLibStats*                   pShaderStats
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
                    if ((strncmp(pShaderExportName, stats.pSymbolName, stats.symbolNameLength) == 0) &&
                        (strlen(pShaderExportName) == stats.symbolNameLength))
                    {
                        switch (HashString(static_cast<const char*>(item.str.start), item.str.length))
                        {
                        case HashLiteralString(".stack_frame_size_in_bytes"):
                        {
                            result = pMetadataReader->UnpackNext(&stats.stackFrameSizeInBytes);
                            pShaderStats->stackFrameSizeInBytes = stats.stackFrameSizeInBytes;
                        }
                        break;
                        case HashLiteralString(PalAbi::ShaderMetadataKey::ShaderSubtype):
                        {
                            Abi::ApiShaderSubType shaderSubType;
                            result = PalAbi::Metadata::DeserializeEnum(pMetadataReader, &shaderSubType);
                            pShaderStats->shaderSubType = ShaderSubType(shaderSubType);
                        }
                        break;
                        case HashLiteralString(PalAbi::HardwareStageMetadataKey::VgprCount):
                        {
                            result = pMetadataReader->UnpackNext(&pShaderStats->common.numUsedVgprs);
                        }
                        break;
                        case HashLiteralString(PalAbi::HardwareStageMetadataKey::SgprCount):
                        {
                            result = pMetadataReader->UnpackNext(&pShaderStats->common.numUsedSgprs);
                        }
                        break;
                        case HashLiteralString(PalAbi::HardwareStageMetadataKey::LdsSize):
                        {
                            result = pMetadataReader->UnpackNext(&pShaderStats->common.ldsUsageSizeInBytes);
                        }
                        break;
                        case HashLiteralString(PalAbi::ShaderMetadataKey::ApiShaderHash):
                        {
                            uint64 shaderHash[2] = {};
                            result = pMetadataReader->UnpackNext(&shaderHash);
                            pShaderStats->shaderHash = { shaderHash[0], shaderHash[1] };
                        }
                        break;
                        case HashLiteralString(PalAbi::HardwareStageMetadataKey::FrontendStackSize):
                        {
                            result = pMetadataReader->UnpackNext(&pShaderStats->cpsStackSizes.frontendSize);
                        }
                        break;
                        case HashLiteralString(PalAbi::HardwareStageMetadataKey::BackendStackSize):
                        {
                            result = pMetadataReader->UnpackNext(&pShaderStats->cpsStackSizes.backendSize);
                        }
                        break;
                        default:
                            result = pMetadataReader->Skip(1);
                            break;
                        }
                    }
                    else
                    {
                        result = pMetadataReader->Skip(1);
                    }
                }
            }

        }
    }

    return result;
}

} // namespace Gfx9
} // namespace Pal
