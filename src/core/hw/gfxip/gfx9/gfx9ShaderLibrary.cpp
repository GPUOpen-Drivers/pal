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
    m_chunkCs(*pDevice)
{
}

// =====================================================================================================================
// Check wavefront size and set the m_hwInfo.flags.isWave32 flag
void ShaderLibrary::SetIsWave32(
    const CodeObjectMetadata& metadata)
{
    // We don't bother checking the wavefront size for pre-Gfx10 GPU's since it is implicitly 64 before Gfx10. Any ELF
    // which doesn't specify a wavefront size is assumed to use 64, even on Gfx10 and newer.
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();
    if (IsGfx10(chipProps.gfxLevel))
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
    const AbiProcessor&            abiProcessor,
    const CodeObjectMetadata&      metadata,
    Util::MsgPackReader*           pMetadataReader)
{
    // ToDo -- move down to gfx9ShaderLibrary impl
    const Gfx9PalSettings&   settings  = m_pDevice->Settings();
    const CmdUtil&           cmdUtil   = m_pDevice->CmdUtil();
    const auto&              regInfo   = cmdUtil.GetRegInfo();
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    RegisterVector registers(m_pDevice->GetPlatform());
    Result result = pMetadataReader->Unpack(&registers);

    ShaderLibraryUploader uploader(m_pDevice);

    if (result == Result::Success)
    {
        // Next, handle relocations and upload the library code & data to GPU memory.
        result = PerformRelocationsAndUploadToGpuMemory(
            abiProcessor,
            metadata,
            (createInfo.flags.overrideGpuHeap == 1) ? createInfo.preferredHeap : GpuHeapInvisible,
            &uploader);
    }

    if (result ==  Result::Success)
    {
        const uint32 wavefrontSize = IsWave32() ? 32 : 64;

        m_chunkCs.LateInit<ShaderLibraryUploader>(abiProcessor,
                                                  registers,
                                                  wavefrontSize,
                                                  createInfo.pFuncList,
                                                  createInfo.funcCount,
                                                  &uploader);

        UpdateHwInfo();

        result = uploader.End();
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

} // namespace Gfx9

} // namespace Pal
