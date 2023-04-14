
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

#include "core/device.h"
#include "core/gpuMemory.h"
#include "core/platform.h"
#include "core/queue.h"
#include "core/hw/gfxip/gfx6/g_gfx6MergedDataFormats.h"
#include "core/hw/gfxip/gfx6/gfx6BorderColorPalette.h"
#include "core/hw/gfxip/gfx6/gfx6CmdStream.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUploadRing.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6ColorBlendState.h"
#include "core/hw/gfxip/gfx6/gfx6ColorTargetView.h"
#include "core/hw/gfxip/gfx6/gfx6ComputeCmdBuffer.h"
#include "core/hw/gfxip/gfx6/gfx6ComputeEngine.h"
#include "core/hw/gfxip/gfx6/gfx6ComputePipeline.h"
#include "core/hw/gfxip/gfx6/gfx6DepthStencilState.h"
#include "core/hw/gfxip/gfx6/gfx6DepthStencilView.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6FormatInfo.h"
#include "core/hw/gfxip/gfx6/gfx6GraphicsPipeline.h"
#include "core/hw/gfxip/gfx6/gfx6Image.h"
#include "core/hw/gfxip/gfx6/gfx6IndirectCmdGenerator.h"
#include "core/hw/gfxip/gfx6/gfx6MsaaState.h"
#include "core/hw/gfxip/gfx6/gfx6OcclusionQueryPool.h"
#include "core/hw/gfxip/gfx6/gfx6PerfCtrInfo.h"
#include "core/hw/gfxip/gfx6/gfx6PerfExperiment.h"
#include "core/hw/gfxip/gfx6/gfx6PipelineStatsQueryPool.h"
#include "core/hw/gfxip/gfx6/gfx6QueueContexts.h"
#include "core/hw/gfxip/gfx6/gfx6UniversalCmdBuffer.h"
#include "core/hw/gfxip/gfx6/gfx6UniversalEngine.h"
#include "core/hw/gfxip/gfx6/gfx6StreamoutStatsQueryPool.h"
#include "core/hw/gfxip/rpm/gfx6/gfx6RsrcProcMgr.h"
#include "palAssert.h"
#include "palInlineFuncs.h"
#include "palMath.h"
#include "palLiterals.h"
#include "core/hw/amdgpu_asic.h"

#include <limits.h>

using namespace Util;
using namespace Util::Literals;
using namespace Pal::Formats::Gfx6;

namespace Pal
{
namespace Gfx6
{
constexpr uint32 NullBufferView[4] = { 0, 0, 0, SQ_RSRC_BUF << SQ_BUF_RSRC_WORD3__TYPE__SHIFT };
constexpr uint32 NullImageView[8] =
{
    0, 0, 0,
    static_cast<uint32>(SQ_RSRC_IMG_2D_ARRAY) << static_cast<uint32>(SQ_IMG_RSRC_WORD3__TYPE__SHIFT),
    0, 0, 0, 0
};
constexpr uint32 NullSampler[4] = { 0, 0, 0, 0 };

// =====================================================================================================================
size_t GetDeviceSize()
{
    return sizeof(Device);
}

// =====================================================================================================================
Result CreateDevice(
    Pal::Device*                pDevice,
    void*                       pPlacementAddr,
    DeviceInterfacePfnTable*    pPfnTable,
    GfxDevice**                 ppGfxDevice)
{
    PAL_ASSERT((pDevice != nullptr) && (pPlacementAddr != nullptr) && (ppGfxDevice != nullptr));

    Device* pGfxDevice = PAL_PLACEMENT_NEW(pPlacementAddr) Device(pDevice);

    Result result = pGfxDevice->EarlyInit();

    if (result == Result::Success)
    {
        (*ppGfxDevice) = pGfxDevice;

        pPfnTable->pfnCreateTypedBufViewSrds   = &Device::CreateTypedBufferViewSrds;
        pPfnTable->pfnCreateUntypedBufViewSrds = &Device::CreateUntypedBufferViewSrds;
        pPfnTable->pfnCreateImageViewSrds      = &Device::CreateImageViewSrds;
        pPfnTable->pfnCreateFmaskViewSrds      = &Device::CreateFmaskViewSrds;
        pPfnTable->pfnCreateSamplerSrds        = &Device::CreateSamplerSrds;
        pPfnTable->pfnCreateBvhSrds            = &Device::CreateBvhSrds;
    }

    return result;
}

// =====================================================================================================================
// Helper function to return the offset of frame count register.
uint32 GetFrameCounterReg(
    const GpuChipProperties& chipProperties)
{
    uint32 offset = 0;

    // Setup the register offset to write the frame count.
    if ((chipProperties.gfxLevel == GfxIpLevel::GfxIp8) || (chipProperties.gfxLevel == GfxIpLevel::GfxIp8_1))
    {
        if (FAMILY_IS_CZ(chipProperties.familyId))
        {
            // For Carrizo we need to use mmMP_FPS_CNT instead of mmSMC_MSG_ARG_11__VI.
            // according to: website register spec, mp_fps_cnt is at 0x235, but according to the carrizo-specific chip
            // headers , it is at 0x1F5.
            offset = 0x1F5;
        }
        else
        {
            offset = mmSMC_MSG_ARG_11__VI;
        }
    }
    else if (chipProperties.gfxLevel == GfxIpLevel::GfxIp7)
    {
        offset = mmSMC_MSG_ARG_11__CI;
    }

    return offset;
}

// =====================================================================================================================
Device::Device(
    Pal::Device* pDevice)
    :
    GfxDevice(pDevice, &m_rsrcProcMgr, GetFrameCounterReg(pDevice->ChipProperties())),
    m_cmdUtil(*pDevice),
    m_rsrcProcMgr(this),
    m_queueContextUpdateCounter(0),
    m_waDbTcCompatFlush(Gfx8TcCompatDbFlushWaNever)
{
    memset(&m_supportFlags, 0, sizeof(m_supportFlags));
}

// =====================================================================================================================
// This must clean up all internal GPU memory allocations and all objects created after EarlyInit. Note that EarlyInit
// is called when the platform creates the device objects so the work it does must be preserved if we are to reuse
// this device object.
Result Device::Cleanup()
{
    // RsrcProcMgr::Cleanup must be called before GfxDevice::Cleanup because the ShaderCache object referenced by
    // RsrcProcMgr is owned by GfxDevice and gets reset on GfxDevice::Cleanup.
    m_rsrcProcMgr.Cleanup();

    Result result = Result::Success;

    if (m_occlusionSrcMem.IsBound())
    {
        result = m_pParent->MemMgr()->FreeGpuMem(m_occlusionSrcMem.Memory(), m_occlusionSrcMem.Offset());
        m_occlusionSrcMem.Update(nullptr, 0);

        if ((m_pParent->GetPlatform() != nullptr) && (m_pParent->GetPlatform()->GetGpuMemoryEventProvider() != nullptr))
        {
            ResourceDestroyEventData destroyData = {};
            destroyData.pObj = &m_occlusionSrcMem;
            m_pParent->GetPlatform()->GetGpuMemoryEventProvider()->LogGpuMemoryResourceDestroyEvent(destroyData);
        }
    }

    if (m_cpDmaPatchMem.IsBound() && (result == Result::Success))
    {
        result = m_pParent->MemMgr()->FreeGpuMem(m_cpDmaPatchMem.Memory(), m_cpDmaPatchMem.Offset());
        m_cpDmaPatchMem.Update(nullptr, 0);

        if ((m_pParent->GetPlatform() != nullptr) && (m_pParent->GetPlatform()->GetGpuMemoryEventProvider() != nullptr))
        {
            ResourceDestroyEventData destroyData = {};
            destroyData.pObj = &m_cpDmaPatchMem;
            m_pParent->GetPlatform()->GetGpuMemoryEventProvider()->LogGpuMemoryResourceDestroyEvent(destroyData);
        }
    }

    if (result == Result::Success)
    {
        result = GfxDevice::Cleanup();
    }

    return result;
}

// =====================================================================================================================
// Performs early initialization of this device; this occurs when the device is created.
Result Device::EarlyInit()
{
    Result result = m_rsrcProcMgr.EarlyInit();

    const auto& chipProperties = m_pParent->ChipProperties();
    // The LBPW feature uses a fixed late alloc VS limit based off of the available CUs
    if (chipProperties.gfx6.lbpwEnabled)
    {
        m_useFixedLateAllocVsLimit = true;
    }

    if (chipProperties.gfxLevel >= GfxIpLevel::GfxIp7)
    {
        // DXX discovered a potential hang situation on Kalindi and Godavari with the VS "late alloc" feature enabled.
        // DXX's solution is to disable the feature on these parts. It should be noted that since these parts have so
        // few CU's, the feature would likely not improve performance for them.
        if (IsKalindi(*m_pParent) || IsGodavari(*m_pParent))
        {
            m_lateAllocVsLimit = 0;
        }
        else
        {
            if (m_useFixedLateAllocVsLimit)
            {
                m_lateAllocVsLimit = (chipProperties.gfx6.numCuPerSh > 2)
                                      ? ((chipProperties.gfx6.numCuPerSh - 1) << 2)
                                      : 0;
            }
            else
            {
                // Follow DXX to enable Late Alloc VS feature for all CI and VI asics that have over 2 CUs per
                // shader array (SH)
                m_lateAllocVsLimit = (chipProperties.gfx6.numCuPerSh > 2)
                                      ? ((chipProperties.gfx6.numCuPerSh - 2) << 2)
                                      : 0;
            }
        }
    }

    SetupWorkarounds();

    return result;
}

// =====================================================================================================================
// Sets up the hardware workaround/support flags based on the current ASIC
void Device::SetupWorkarounds()
{
    const auto eRevId = m_pParent->ChipProperties().eRevId;

    // Clamp the max border color palette size to the max supported by the hardware.
    Parent()->GetPublicSettings()->borderColorPaletteSizeLimit =
        (Min<uint32>(SQ_IMG_SAMP_WORD3__BORDER_COLOR_PTR_MASK + 1,
         Parent()->GetPublicSettings()->borderColorPaletteSizeLimit));

    if (IsGfx6(*m_pParent))
    {
        m_supportFlags.waDbReZStencilCorruption = 1;
        m_supportFlags.waDbOverRasterization = 1;
        m_supportFlags.waAlignCpDma = 1;
        m_supportFlags.waVgtPrimResetIndxMaskByType = 1;
        m_supportFlags.waCpIb2ChainingUnsupported = 1;

        // On Gfx6 hardware, the CB does not properly clamp its input if the shader export format is UINT16/SINT16 and
        // the CB format is less than 16 bits per channel.
        m_supportFlags.waCbNoLt16BitIntClamp = 1;

        m_supportFlags.waMiscNullIb = 1;
    }
    else if (IsGfx7(*m_pParent))
    {
        m_supportFlags.waAlignCpDma = 1;
        m_supportFlags.waVgtPrimResetIndxMaskByType = 1;
        m_supportFlags.waWaitIdleBeforeSpiConfigCntl = 1;

        m_supportFlags.waCpDmaHangMcTcAckDrop = 1;

        m_supportFlags.waEventWriteEopPrematureL2Inv = 1;

        if (IsHawaii(*m_pParent))
        {
            m_supportFlags.waMiscOffchipLdsBufferLimit = 1;
            m_supportFlags.waMiscGsRingOverflow = 1;
            m_supportFlags.waMiscVgtNullPrim = 1;
            m_supportFlags.waMiscVsBackPressure = 1;
        }
        else if (IsBonaire(*m_pParent))
        {
            if (eRevId == CI_BONAIRE_M_A0)
            {
                m_supportFlags.waShaderSpiBarrierMgmt = 1;
            }
            m_supportFlags.waShaderSpiWriteShaderPgmRsrc2Ls = 1;
            m_supportFlags.waCbNoLt16BitIntClamp = 1;
        }
        else if (IsSpectre(*m_pParent) || IsSpooky(*m_pParent))
        {
            m_supportFlags.waShaderSpiWriteShaderPgmRsrc2Ls = 1;
            m_supportFlags.waForceToWriteNonRlcRestoredRegs = 1;
            m_supportFlags.waCbNoLt16BitIntClamp = 1;
        }
        else if (IsGodavari(*m_pParent))
        {
            m_supportFlags.waShaderSpiBarrierMgmt = 1;
            m_supportFlags.waShaderSpiWriteShaderPgmRsrc2Ls = 1;
            m_supportFlags.waForceToWriteNonRlcRestoredRegs = 1;
            m_supportFlags.waCbNoLt16BitIntClamp = 1;
        }
        else if (IsKalindi(*m_pParent))
        {
            m_supportFlags.waShaderSpiBarrierMgmt = 1;
            m_supportFlags.waShaderSpiWriteShaderPgmRsrc2Ls = 1;
            m_supportFlags.waForceToWriteNonRlcRestoredRegs = 1;
            m_supportFlags.waCbNoLt16BitIntClamp = 1;
        }
    }
    else if (IsGfx8(*m_pParent))
    {
        m_waEnableDccCacheFlushAndInvalidate = true;
        m_supportFlags.waMiscDccOverwriteComb = 1;
        m_supportFlags.waWaitIdleBeforeSpiConfigCntl = 1;

        m_supportFlags.waEnableDccXthickUse = 1;

        m_supportFlags.waNoFastClearWithDcc = 1;

        m_supportFlags.waEventWriteEopPrematureL2Inv = 1;

        // ZRANGE not TC-compatible for clear surfaces
        m_waTcCompatZRange = true;

        if (IsIceland(*m_pParent))
        {
            if (eRevId == VI_ICELAND_M_A0)
            {
                m_supportFlags.waDbDecompressOnPlanesFor4xMsaa = 1;
            }

            m_supportFlags.waAlignCpDma = 1;
            m_supportFlags.waAsyncComputeMoreThan4096ThreadGroups = 1;
            m_waDbTcCompatFlush = Gfx8TcCompatDbFlushWaNormal;
        }
        else if (IsTonga(*m_pParent))
        {
            m_supportFlags.waAlignCpDma = 1;
            m_supportFlags.waMiscVsBackPressure = 1;
            m_supportFlags.waAsyncComputeMoreThan4096ThreadGroups = 1;
            m_supportFlags.waShaderOffChipGsHang = 1;
            m_waDbTcCompatFlush = Gfx8TcCompatDbFlushWaNormal;
        }
        else if (IsCarrizo(*m_pParent))
        {
            m_supportFlags.waAlignCpDma = 1;
            m_supportFlags.waMiscMixedHeapFlips = 1;
            m_supportFlags.waForceToWriteNonRlcRestoredRegs = 1;
        }
        else if (IsFiji(*m_pParent))
        {
            m_supportFlags.waMiscVsBackPressure = 1;
            // NOTE: The CP DMA unaligned performance bug is fixed in Fiji and Polaris10.
            m_supportFlags.waShaderOffChipGsHang = 1;

            // Fiji can avoid poor decompress blt performance.
            m_supportFlags.waDbDecompressPerformance = 1;
        }
        else if (IsPolaris10(*m_pParent) ||
                 IsPolaris11(*m_pParent) ||
                 IsPolaris12(*m_pParent))
        {
            m_supportFlags.waShaderOffChipGsHang = 1;

            // Polaris10 and Polaris11 branched after Fiji so they have the fix too.
            m_supportFlags.waDbDecompressPerformance = 1;

            // Enable degenerate primitive filtering for Polaris
            m_degeneratePrimFilter = true;

            // Enable small primitive filter control
            // PA: Lines incorrectly dropped by the small primitive filter
            m_smallPrimFilter = (SmallPrimFilterEnablePoint    |
                                 SmallPrimFilterEnableTriangle |
                                 SmallPrimFilterEnableRectangle);
        }
        else if (IsStoney(*m_pParent))
        {
            // gfx8.1 variants can avoid poor decompress blt performance.
            m_supportFlags.waDbDecompressPerformance = 1;
        }
    }
}

// =====================================================================================================================
// Performs any late-stage initialization that can only be done after settings have been committed.
Result Device::LateInit()
{
    // If this device has been used before it will need this state zeroed.
    memset(const_cast<ShaderRingItemSizes*>(&m_largestRingSizes), 0, sizeof(m_largestRingSizes));
    m_queueContextUpdateCounter = 0;

    return Result::Success;
}

// =====================================================================================================================
// Finalizes any chip properties which depend on settings being read.
void Device::FinalizeChipProperties(
    GpuChipProperties* pChipProperties
    ) const
{
    const Gfx6PalSettings& settings = GetGfx6Settings(*Parent());

    GfxDevice::FinalizeChipProperties(pChipProperties);

    switch (settings.gfx7OffchipLdsBufferSize)
    {
    case OffchipLdsBufferSize1024:
        pChipProperties->gfxip.offChipTessBufferSize = 1024 * sizeof(uint32);
        break;
    case OffchipLdsBufferSize2048:
        pChipProperties->gfxip.offChipTessBufferSize = 2048 * sizeof(uint32);
        break;
    case OffchipLdsBufferSize4096:
        pChipProperties->gfxip.offChipTessBufferSize = 4096 * sizeof(uint32);
        break;
    case OffchipLdsBufferSize8192:
        pChipProperties->gfxip.offChipTessBufferSize = 8192 * sizeof(uint32);
        break;
    default:
        PAL_NEVER_CALLED();
        break;
    }

    pChipProperties->gfxip.numOffchipTessBuffers     = settings.numOffchipLdsBuffers;
    pChipProperties->gfxip.maxPrimgroupSize          = 256;
    pChipProperties->gfxip.tessFactorBufferSizePerSe = settings.tessFactorBufferSizePerSe;
}

// =====================================================================================================================
// Peforms extra initialization which needs to be done after the parent Device is finalized.
Result Device::Finalize()
{
    Result result = GfxDevice::Finalize();

    if (result == Result::Success)
    {
        result = m_rsrcProcMgr.LateInit();
    }

    if (result == Result::Success)
    {
        const GpuChipProperties& chipProps = m_pParent->ChipProperties();

        // First, we initialize our copy of the reset data for a single query slot.
        memset(&m_occlusionSlotResetValues[0], 0, sizeof(m_occlusionSlotResetValues));

        // Because the reset data was initialized to zero, we only need to fill in the valid bits for the disabled RBs.
        if (chipProps.gfx6.numActiveRbs < chipProps.gfx6.numTotalRbs)
        {
            for (uint32 rb = 0; rb < chipProps.gfx6.numTotalRbs; rb++)
            {
                if ((chipProps.gfx6.backendDisableMask & (1 << rb)) != 0)
                {
                    m_occlusionSlotResetValues[rb].begin.bits.valid = 1;
                    m_occlusionSlotResetValues[rb].end.bits.valid   = 1;
                }
            }
        }

        const Gfx6PalSettings& gfx6Settings = GetGfx6Settings(*m_pParent);

        const size_t slotSize = chipProps.gfx6.numTotalRbs * sizeof(OcclusionQueryResultPair);

        PAL_ALERT(slotSize > sizeof(m_occlusionSlotResetValues));

        // Second, if the DMA optimization is enabled, we allocate a buffer of local memory to accelerate large
        // resets using DMA.
        GpuMemoryCreateInfo srcMemCreateInfo = { };
        srcMemCreateInfo.alignment = gfx6Settings.cpDmaSrcAlignment;
        srcMemCreateInfo.size      = Pal::Device::OcclusionQueryDmaBufferSlots * slotSize;
        srcMemCreateInfo.priority  = GpuMemPriority::Normal;
        srcMemCreateInfo.heaps[0]  = GpuHeapLocal;
        srcMemCreateInfo.heaps[1]  = GpuHeapGartUswc;
        srcMemCreateInfo.heapCount = 2;

        GpuMemoryInternalCreateInfo internalInfo = { };
        internalInfo.flags.alwaysResident = 1;

        GpuMemory* pMemObj   = nullptr;
        gpusize    memOffset = 0;

        result = m_pParent->MemMgr()->AllocateGpuMem(srcMemCreateInfo, internalInfo, false, &pMemObj, &memOffset);

        char* pData = nullptr;
        if (result == Result::Success)
        {
            m_occlusionSrcMem.Update(pMemObj, memOffset);

            if ((m_pParent->GetPlatform() != nullptr) && (m_pParent->GetPlatform()->GetGpuMemoryEventProvider() != nullptr))
            {
                ResourceDescriptionMiscInternal desc;
                desc.type = MiscInternalAllocType::OcclusionQueryResetData;

                ResourceCreateEventData createData = {};
                createData.type = ResourceType::MiscInternal;
                createData.pObj = &m_occlusionSrcMem;
                createData.pResourceDescData = &desc;
                createData.resourceDescSize = sizeof(ResourceDescriptionMiscInternal);

                m_pParent->GetPlatform()->GetGpuMemoryEventProvider()->LogGpuMemoryResourceCreateEvent(createData);

                GpuMemoryResourceBindEventData bindData = {};
                bindData.pGpuMemory = pMemObj;
                bindData.pObj = &m_occlusionSrcMem;
                bindData.offset = memOffset;
                bindData.requiredGpuMemSize = srcMemCreateInfo.size;
                m_pParent->GetPlatform()->GetGpuMemoryEventProvider()->LogGpuMemoryResourceBindEvent(bindData);

                Developer::BindGpuMemoryData callbackData = {};
                callbackData.pObj               = bindData.pObj;
                callbackData.requiredGpuMemSize = bindData.requiredGpuMemSize;
                callbackData.pGpuMemory         = bindData.pGpuMemory;
                callbackData.offset             = bindData.offset;
                callbackData.isSystemMemory     = bindData.isSystemMemory;
                m_pParent->DeveloperCb(Developer::CallbackType::BindGpuMemory, &callbackData);
            }

            result = m_occlusionSrcMem.Map(reinterpret_cast<void**>(&pData));
        }

        // Populate the buffer with occlusion query reset data.
        if (result == Result::Success)
        {
            for (uint32 slot = 0; slot < Pal::Device::OcclusionQueryDmaBufferSlots; ++slot)
            {
                memcpy(pData, m_occlusionSlotResetValues, slotSize);
                pData += slotSize;
            }

            result = m_occlusionSrcMem.Unmap();
        }

        if (gfx6Settings.cpDmaSrcAlignment != CpDmaAlignmentDefault)
        {
            GpuMemoryCreateInfo patchMemCreateInfo = { };
            patchMemCreateInfo.alignment = gfx6Settings.cpDmaSrcAlignment;
            patchMemCreateInfo.size      = patchMemCreateInfo.alignment;
            patchMemCreateInfo.priority  = GpuMemPriority::Normal;
            patchMemCreateInfo.heaps[0]  = GpuHeapInvisible;
            patchMemCreateInfo.heaps[1]  = GpuHeapLocal;
            patchMemCreateInfo.heaps[2]  = GpuHeapGartUswc;
            patchMemCreateInfo.heapCount = 3;

            pMemObj   = nullptr;
            memOffset = 0;

            result = m_pParent->MemMgr()->AllocateGpuMem(patchMemCreateInfo, internalInfo, false, &pMemObj, &memOffset);

            if (result == Result::Success)
            {
                m_cpDmaPatchMem.Update(pMemObj, memOffset);

                if ((m_pParent->GetPlatform() != nullptr) && (m_pParent->GetPlatform()->GetGpuMemoryEventProvider() != nullptr))
                {
                    ResourceDescriptionMiscInternal desc;
                    desc.type = MiscInternalAllocType::Cpdmapatch;

                    ResourceCreateEventData createData = {};
                    createData.type = ResourceType::MiscInternal;
                    createData.pObj = &m_cpDmaPatchMem;
                    createData.pResourceDescData = &desc;
                    createData.resourceDescSize = sizeof(ResourceDescriptionMiscInternal);

                    m_pParent->GetPlatform()->GetGpuMemoryEventProvider()->LogGpuMemoryResourceCreateEvent(createData);

                    GpuMemoryResourceBindEventData bindData = {};
                    bindData.pGpuMemory = pMemObj;
                    bindData.pObj = &m_cpDmaPatchMem;
                    bindData.offset = memOffset;
                    bindData.requiredGpuMemSize = srcMemCreateInfo.size;
                    m_pParent->GetPlatform()->GetGpuMemoryEventProvider()->LogGpuMemoryResourceBindEvent(bindData);

                    Developer::BindGpuMemoryData callbackData = {};
                    callbackData.pObj               = bindData.pObj;
                    callbackData.requiredGpuMemSize = bindData.requiredGpuMemSize;
                    callbackData.pGpuMemory         = bindData.pGpuMemory;
                    callbackData.offset             = bindData.offset;
                    callbackData.isSystemMemory     = bindData.isSystemMemory;
                    m_pParent->DeveloperCb(Developer::CallbackType::BindGpuMemory, &callbackData);
                }
            }
        }
    }

    if (result == Result::Success)
    {
        // Initialize an array for finding cb index which is compatible to specified db tileIndex.
        memset(m_overridedTileIndexForDepthStencilCopy, 0, sizeof(m_overridedTileIndexForDepthStencilCopy));

        for (int32 tileIndex = 0; tileIndex < 8; ++tileIndex)
        {
            const GpuChipProperties& chipProps = m_pParent->ChipProperties();
            regGB_TILE_MODE0 regTileMode;
            regTileMode.u32All = chipProps.gfx6.gbTileMode[tileIndex];

            int32 overrideTileIndex = -1;

            for (int32 i = 0; i < 32; ++i)
            {
                regGB_TILE_MODE0 regTileModeOther;

                regTileModeOther.u32All = chipProps.gfx6.gbTileMode[i];

                if ((chipProps.gfxLevel > GfxIpLevel::GfxIp6) &&
                    (regTileModeOther.bits.ARRAY_MODE == regTileMode.bits.ARRAY_MODE) &&
                    (regTileModeOther.bits.MICRO_TILE_MODE_NEW__CI__VI == ADDR_NON_DISPLAYABLE))
                {
                    // On Gfx7/Gfx8, only non-split depth-only surface might go through fixed-func depth-stencil copy,
                    // so just find the suitable tile mode index with respect to array mode.
                    overrideTileIndex = i;
                    break;
                }
                else if ((chipProps.gfxLevel == GfxIpLevel::GfxIp6) &&
                         (regTileModeOther.bits.MICRO_TILE_MODE__SI == ADDR_NON_DISPLAYABLE) &&
                         (regTileModeOther.bits.ARRAY_MODE == regTileMode.bits.ARRAY_MODE) &&
                         ((regTileModeOther.bits.ARRAY_MODE == ADDR_TM_1D_TILED_THIN1) ||
                         ((regTileModeOther.bits.TILE_SPLIT == regTileMode.bits.TILE_SPLIT) &&
                          (regTileModeOther.bits.BANK_WIDTH__SI == regTileMode.bits.BANK_WIDTH__SI) &&
                          (regTileModeOther.bits.BANK_HEIGHT__SI == regTileMode.bits.BANK_HEIGHT__SI) &&
                          (regTileModeOther.bits.NUM_BANKS__SI == regTileMode.bits.NUM_BANKS__SI) &&
                          (regTileModeOther.bits.MACRO_TILE_ASPECT__SI == regTileMode.bits.MACRO_TILE_ASPECT__SI))))
                {
                    overrideTileIndex = i;
                    break;
                }
            }

            m_overridedTileIndexForDepthStencilCopy[tileIndex] = overrideTileIndex;
        }
    }

    return result;
}

// =====================================================================================================================
// Gets the maximum alignments for images created with a linear tiling mode assuming the images' elements are no larger
// than pAlignments->maxElementSize.
Result Device::GetLinearImageAlignments(
    LinearImageAlignments* pAlignments
    ) const
{
    Result result = Result::Success;

    if (pAlignments == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }
    else if (pAlignments->maxElementSize == 0)
    {
        result = Result::ErrorInvalidValue;
    }
    else
    {
        // Use the GB_ADDR_CONFIG register to determine our pipe interleave config.
        constexpr uint32 PipeInterleaveSizesCount = 2;
        constexpr uint16 PipeInterleaveSizes[PipeInterleaveSizesCount] =
        {
            256, // ADDR_CONFIG_PIPE_INTERLEAVE_256B
            512  // ADDR_CONFIG_PIPE_INTERLEAVE_512B
        };

        GB_ADDR_CONFIG gbAddrConfig;
        gbAddrConfig.u32All = m_pParent->ChipProperties().gfx6.gbAddrConfig;
        PAL_ASSERT(gbAddrConfig.bits.PIPE_INTERLEAVE_SIZE < PipeInterleaveSizesCount);

        const uint16 pipeInterleaveSize = PipeInterleaveSizes[gbAddrConfig.bits.PIPE_INTERLEAVE_SIZE];

        pAlignments->baseAddress = pipeInterleaveSize;
        pAlignments->rowPitch    = Max<uint16>(8  * pAlignments->maxElementSize, 64);
        pAlignments->depthPitch  = Max<uint16>(64 * pAlignments->maxElementSize, pipeInterleaveSize);
    }

    return result;
}

// =====================================================================================================================
// Updates the GPU memory bound for use as a trap handler for either compute or graphics pipelines.  Updates the queue
// context update counter so that the next submission on each queue will properly process this update.
void Device::BindTrapHandler(
    PipelineBindPoint pipelineType,
    IGpuMemory*       pGpuMemory,
    gpusize           offset)
{
    PAL_ASSERT(IsPow2Aligned(offset, 256));

    if (pipelineType == PipelineBindPoint::Graphics)
    {
        m_graphicsTrapHandler.Update(pGpuMemory, offset);
    }
    else
    {
        PAL_ASSERT(pipelineType == PipelineBindPoint::Compute);
        m_computeTrapHandler.Update(pGpuMemory, offset);
    }

    m_queueContextUpdateCounter++;
}

// =====================================================================================================================
// Updates the GPU memory bound for use as a trap buffer for either compute or graphics pipelines.  Updates the queue
// context update counter so that the next submission on each queue will properly process this update.
void Device::BindTrapBuffer(
    PipelineBindPoint pipelineType,
    IGpuMemory*       pGpuMemory,
    gpusize           offset)
{
    PAL_ASSERT(IsPow2Aligned(offset, 256));

    if (pipelineType == PipelineBindPoint::Graphics)
    {
        m_graphicsTrapBuffer.Update(pGpuMemory, offset);
    }
    else
    {
        PAL_ASSERT(pipelineType == PipelineBindPoint::Compute);
        m_computeTrapBuffer.Update(pGpuMemory, offset);
    }

    m_queueContextUpdateCounter++;
}

#if DEBUG
// =====================================================================================================================
// Useful helper function for debugging command buffers on the GPU. This adds a WAIT_REG_MEM command to the specified
// command buffer space which waits until the device's dummy memory location contains the provided 'number' value. This
// lets engineers temporarily hang the GPU so they can inspect hardware state and command buffer contents in WinDbg, and
// then when they're finished, they can "un-hang" the GPU by modifying the memory location being waited on to contain
// the provided value.
uint32* Device::TemporarilyHangTheGpu(
    EngineType engineType,
    uint32     number,
    uint32*    pCmdSpace
    ) const
{
    return (pCmdSpace + m_cmdUtil.BuildWaitRegMem(WAIT_REG_MEM_SPACE_MEMORY,
                                                  WAIT_REG_MEM_FUNC_EQUAL,
                                                  WAIT_REG_MEM_ENGINE_ME,
                                                  m_debugStallGpuMem.GpuVirtAddr(),
                                                  number,
                                                  UINT_MAX,
                                                  false,
                                                  pCmdSpace));
}
#endif

// =====================================================================================================================
Result Device::CreateEngine(
    EngineType engineType,
    uint32     engineIndex,
    Engine**   ppEngine)
{
    Result  result  = Result::ErrorOutOfMemory;
    Engine* pEngine = nullptr;

    switch (engineType)
    {
    case EngineTypeUniversal:
        pEngine = PAL_NEW(UniversalEngine, GetPlatform(), AllocInternal)(this, engineType, engineIndex);
        break;
    case EngineTypeCompute:
        pEngine = PAL_NEW(ComputeEngine, GetPlatform(), AllocInternal)(this, engineType, engineIndex);
        break;
    default:
        // What is this?
        PAL_ASSERT_ALWAYS();
        result = Result::ErrorInvalidValue;
        break;
    }

    if (pEngine != nullptr)
    {
        result = pEngine->Init();
    }

    if (result == Result::Success)
    {
        (*ppEngine) = pEngine;
    }
    else
    {
        PAL_SAFE_DELETE(pEngine, GetPlatform());
    }

    return result;
}

// =====================================================================================================================
Result Device::CreateDummyCommandStream(
    EngineType       engineType,
    Pal::CmdStream** ppCmdStream
    ) const
{
    Result          result     = Result::ErrorOutOfMemory;
    Pal::CmdStream* pCmdStream = PAL_NEW(CmdStream, GetPlatform(), AllocInternal)(*this,
                                     Parent()->InternalUntrackedCmdAllocator(),
                                     engineType,
                                     SubEngineType::Primary,
                                     CmdStreamUsage::Workload,
                                     false);
    if (pCmdStream != nullptr)
    {
        result = pCmdStream->Init();
    }

    if (result == Result::Success)
    {
        constexpr CmdStreamBeginFlags beginFlags = {};
        pCmdStream->Reset(nullptr, true);
        pCmdStream->Begin(beginFlags, nullptr);

        uint32* pCmdSpace = pCmdStream->ReserveCommands();

        pCmdSpace += m_cmdUtil.BuildNop(m_cmdUtil.GetMinNopSizeInDwords(), pCmdSpace);

        pCmdStream->CommitCommands(pCmdSpace);

        result = pCmdStream->End();
    }

    if (result == Result::Success)
    {
        (*ppCmdStream) = pCmdStream;
    }
    else
    {
        PAL_SAFE_DELETE(pCmdStream, GetPlatform());
    }

    return result;
}

// =====================================================================================================================
// Determines the size of the QueueContext object needed for GFXIP6+ hardware. Only supported on Universal and
// Compute Queues.
size_t Device::GetQueueContextSize(
    const QueueCreateInfo& createInfo
    ) const
{
    size_t size = 0;

    switch (createInfo.queueType)
    {
    case QueueTypeCompute:
        {
            size = sizeof(ComputeQueueContext);
        }
        break;
    case QueueTypeUniversal:
        size = sizeof(UniversalQueueContext);
        break;
    default:
        break;
    }

    return size;
}

// =====================================================================================================================
// Creates the QueueContext object for the specified Queue in preallocated memory. Only supported on Universal and
// Compute Queues.
Result Device::CreateQueueContext(
    const QueueCreateInfo& createInfo,
    Engine*                pEngine,
    void*                  pPlacementAddr,
    QueueContext**         ppQueueContext)
{
    PAL_ASSERT((pPlacementAddr != nullptr) && (ppQueueContext != nullptr));

    Result result = Result::Success;

    const uint32 engineId = createInfo.engineIndex;
    switch (createInfo.queueType)
    {
    case QueueTypeCompute:
        {
            {
                ComputeQueueContext* pContext =
                    PAL_PLACEMENT_NEW(pPlacementAddr) ComputeQueueContext(this, pEngine, engineId, createInfo.tmzOnly);

                result = pContext->Init();

                if (result == Result::Success)
                {
                    (*ppQueueContext) = pContext;
                }
                else
                {
                    pContext->Destroy();
                }
            }
        }
        break;
    case QueueTypeUniversal:
        {
            const bool isPreemptionSupported = Parent()->IsPreemptionSupported(createInfo.engineType);
            UniversalQueueContext* pContext =
                PAL_PLACEMENT_NEW(pPlacementAddr) UniversalQueueContext(
                                                    this,
                                                    isPreemptionSupported,
                                                    createInfo.persistentCeRamOffset,
                                                    createInfo.persistentCeRamSize,
                                                    pEngine,
                                                    engineId);

            result = pContext->Init();

            if (result == Result::Success)
            {
                (*ppQueueContext) = pContext;
            }
            else
            {
                pContext->Destroy();
            }
        }
        break;
    default:
        result = Result::ErrorUnavailable;
        break;
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetComputePipelineSize(
    const ComputePipelineCreateInfo& createInfo,
    Result*                          pResult
    ) const
{
    if (pResult != nullptr)
    {
        (*pResult) = Result::Success;
    }

    return sizeof(ComputePipeline);
}

// =====================================================================================================================
Result Device::CreateComputePipeline(
    const ComputePipelineCreateInfo& createInfo,
    void*                            pPlacementAddr,
    bool                             isInternal,
    IPipeline**                      ppPipeline)
{
    auto* pPipeline = PAL_PLACEMENT_NEW(pPlacementAddr) ComputePipeline(this, isInternal);

    AbiReader abiReader(GetPlatform(), createInfo.pPipelineBinary);
    Result result = abiReader.Init();

    MsgPackReader metadataReader;
    PalAbi::CodeObjectMetadata metadata = {};

    if (result == Result::Success)
    {
        result = abiReader.GetMetadata(&metadataReader, &metadata);
    }

    if (result == Result::Success)
    {
        result = pPipeline->Init(createInfo, abiReader, metadata, &metadataReader);
    }

    if (result != Result::Success)
    {
        pPipeline->Destroy();
        pPipeline = nullptr;
    }

    *ppPipeline = pPipeline;

    return result;
}

// =====================================================================================================================
size_t Device::GetShaderLibrarySize(
    const ShaderLibraryCreateInfo&  createInfo,
    Result*                         pResult
    ) const
{
    // Not supported in gfx6
    return 0;
}

// =====================================================================================================================
Result Device::CreateShaderLibrary(
    const ShaderLibraryCreateInfo&  createInfo,
    void*                           pPlacementAddr,
    bool                            isInternal,
    IShaderLibrary**                ppPipeline)
{
    // Not supported in gfx6
    return Result::Unsupported;
}

// =====================================================================================================================
size_t Device::GetGraphicsPipelineSize(
    const GraphicsPipelineCreateInfo& createInfo,
    bool                              isInternal,
    Result*                           pResult
) const
{
    if (pResult != nullptr)
    {
        (*pResult) = Result::Success;
    }

    return sizeof(GraphicsPipeline);
}

// =====================================================================================================================
Result Device::CreateGraphicsPipeline(
    const GraphicsPipelineCreateInfo&         createInfo,
    const GraphicsPipelineInternalCreateInfo& internalInfo,
    void*                                     pPlacementAddr,
    bool                                      isInternal,
    IPipeline**                               ppPipeline)
{
    PAL_ASSERT(createInfo.pPipelineBinary != nullptr);
    PAL_ASSERT(pPlacementAddr != nullptr);
    AbiReader abiReader(GetPlatform(), createInfo.pPipelineBinary);
    Result result = abiReader.Init();

    auto* pPipeline = PAL_PLACEMENT_NEW(pPlacementAddr) GraphicsPipeline(this, isInternal);

    MsgPackReader metadataReader;
    PalAbi::CodeObjectMetadata metadata = {};

    if (result == Result::Success)
    {
        result = abiReader.GetMetadata(&metadataReader, &metadata);
    }

    if (result == Result::Success)
    {
        result = pPipeline->Init(createInfo, internalInfo, abiReader, metadata, &metadataReader);

        if (result != Result::Success)
        {
            pPipeline->Destroy();
        }
        else
        {
            *ppPipeline = pPipeline;
        }
    }

    return result;
}

// =====================================================================================================================
// Client drivers should be responsible for not repeatedly set the pallete table with the same data, PAL
// doesn't check if the udpated contents are identical to last time.
Result Device::SetSamplePatternPalette(
    const SamplePatternPalette& palette)
{
    const MutexAuto lock(&m_ringSizesLock);

    // Update SamplePos shader ring item size to create sample pattern paletter video memory during validation.
    m_largestRingSizes.itemSize[static_cast<size_t>(ShaderRingType::SamplePos)] = MaxSamplePatternPaletteEntries;
    memcpy(const_cast<SamplePatternPalette*>(&m_samplePatternPalette), palette, sizeof(m_samplePatternPalette));

    // Increment counter to trigger later sample pattern palette update during submission
    m_queueContextUpdateCounter++;

    return Result::Success;
}

// =====================================================================================================================
// Copy stored sample position pallete table to caller's ouput buffer so they know what to validate/update
void Device::GetSamplePatternPalette(
    SamplePatternPalette* pSamplePatternPallete)
{
    PAL_ASSERT(pSamplePatternPallete != nullptr);

    const MutexAuto lock(&m_ringSizesLock);
    memcpy(pSamplePatternPallete,
           const_cast<const SamplePatternPalette*>(&m_samplePatternPalette),
           sizeof(m_samplePatternPalette));
}

// =====================================================================================================================
// Called during pipeline creation to notify that item-size requirements for each shader ring have changed. These
// 'largest ring sizes' will be validated at Queue submission time.
//
// NOTE: Since this is called at pipeline-create-time, it can be invoked by multiple threads simultaneously.
void Device::UpdateLargestRingSizes(
    const ShaderRingItemSizes* pRingSizesNeeded)
{
    const MutexAuto lock(&m_ringSizesLock);

    // Loop over all ring sizes and check if the ring sizes need to grow at all.
    bool ringSizesDirty = false;
    for (size_t ring = 0; ring < static_cast<size_t>(ShaderRingType::NumUniversal); ++ring)
    {
        if (pRingSizesNeeded->itemSize[ring] > m_largestRingSizes.itemSize[ring])
        {
            m_largestRingSizes.itemSize[ring] = pRingSizesNeeded->itemSize[ring];
            ringSizesDirty = true;
        }
    }

    // If the ring sizes are dirty, update the queue context counter so that all queue contexts will be rebuilt before
    // their next submission.
    if (ringSizesDirty)
    {
        m_queueContextUpdateCounter++;
    }
}

// =====================================================================================================================
// Copy our largest ring item-sizes to the caller's output buffer so they know what to validate against.
void Device::GetLargestRingSizes(
    ShaderRingItemSizes* pRingSizesNeeded)
{
    const MutexAuto lock(&m_ringSizesLock);

    // Note that the const_cast is required because m_largestRingSizes is marked as volatile.
    memcpy(pRingSizesNeeded,
           const_cast<const ShaderRingItemSizes*>(&m_largestRingSizes),
           sizeof(m_largestRingSizes));
}

// =====================================================================================================================
size_t Device::GetColorBlendStateSize(
    const ColorBlendStateCreateInfo& createInfo,
    Result*                          pResult
    ) const
{
    if (pResult != nullptr)
    {
        *pResult = Result::Success;
    }

    return sizeof(ColorBlendState);
}

// =====================================================================================================================
Result Device::CreateColorBlendState(
    const ColorBlendStateCreateInfo& createInfo,
    void*                            pPlacementAddr,
    IColorBlendState**               ppColorBlendState
    ) const
{
    *ppColorBlendState = PAL_PLACEMENT_NEW(pPlacementAddr) ColorBlendState(*this, createInfo);
    PAL_ASSERT(*ppColorBlendState != nullptr);

    return Result::Success;
}

// =====================================================================================================================
size_t Device::GetDepthStencilStateSize(
    const DepthStencilStateCreateInfo& createInfo,
    Result*                            pResult
    ) const
{
    if (pResult != nullptr)
    {
        (*pResult) = Result::Success;
    }

    return sizeof(DepthStencilState);
}

// =====================================================================================================================
Result Device::CreateDepthStencilState(
    const DepthStencilStateCreateInfo& createInfo,
    void*                              pPlacementAddr,
    IDepthStencilState**               ppDepthStencilState
    ) const
{
    *ppDepthStencilState = PAL_PLACEMENT_NEW(pPlacementAddr) DepthStencilState(createInfo);
    PAL_ASSERT(*ppDepthStencilState != nullptr);

    return Result::Success;
}

// =====================================================================================================================
size_t Device::GetMsaaStateSize(
    const MsaaStateCreateInfo& createInfo,
    Result*                    pResult
    ) const
{
    if (pResult != nullptr)
    {
        (*pResult) = Result::Success;
    }

    return sizeof(MsaaState);
}

// =====================================================================================================================
Result Device::CreateMsaaState(
    const MsaaStateCreateInfo& createInfo,
    void*                      pPlacementAddr,
    IMsaaState**               ppMsaaState
    ) const
{
    *ppMsaaState = PAL_PLACEMENT_NEW(pPlacementAddr) MsaaState(*this, createInfo);
    PAL_ASSERT(*ppMsaaState != nullptr);

    return Result::Success;
}

// =====================================================================================================================
size_t Device::GetImageSize(
    const ImageCreateInfo& createInfo) const
{
    return sizeof(Image);
}

// =====================================================================================================================
// Creates a concrete Gfx6 GfxImage object
void Device::CreateImage(
    Pal::Image* pParentImage,
    ImageInfo*  pImageInfo,
    void*       pPlacementAddr,
    GfxImage**  ppImage
    ) const
{
    (*ppImage) = PAL_PLACEMENT_NEW(pPlacementAddr) Image(pParentImage, pImageInfo, *m_pParent);
}

// =====================================================================================================================
size_t Device::GetBorderColorPaletteSize(
    const BorderColorPaletteCreateInfo& createInfo,
    Result*                             pResult
    ) const
{
    if (pResult != nullptr)
    {
        if ((createInfo.paletteSize == 0) ||
            (createInfo.paletteSize > Parent()->GetPublicSettings()->borderColorPaletteSizeLimit))
        {
            *pResult = Result::ErrorInvalidValue;
        }
        else
        {
            *pResult = Result::Success;
        }
    }

    return sizeof(BorderColorPalette);
}

// =====================================================================================================================
Result Device::CreateBorderColorPalette(
    const BorderColorPaletteCreateInfo& createInfo,
    void*                               pPlacementAddr,
    IBorderColorPalette**               ppBorderColorPalette
    ) const
{
    *ppBorderColorPalette = PAL_PLACEMENT_NEW(pPlacementAddr) BorderColorPalette(*this, createInfo);

    return Result::Success;
}
// =====================================================================================================================
size_t Device::GetQueryPoolSize(
    const QueryPoolCreateInfo& createInfo,
    Result*                    pResult
    ) const
{
    size_t queryPoolSize = 0;

    if (pResult != nullptr)
    {
        if (((createInfo.queryPoolType != QueryPoolType::Occlusion)     &&
             (createInfo.queryPoolType != QueryPoolType::PipelineStats) &&
             (createInfo.queryPoolType != QueryPoolType::StreamoutStats)) ||
            (createInfo.numSlots == 0))
        {
            *pResult = Result::ErrorInvalidValue;
        }
        else
        {
            *pResult = Result::Success;
        }
    }

    if (createInfo.queryPoolType == QueryPoolType::Occlusion)
    {
        queryPoolSize = sizeof(OcclusionQueryPool);
    }
    else if (createInfo.queryPoolType == QueryPoolType::PipelineStats)
    {
        queryPoolSize = sizeof(PipelineStatsQueryPool);
    }
    else if (createInfo.queryPoolType == QueryPoolType::StreamoutStats)
    {
        queryPoolSize = sizeof(StreamoutStatsQueryPool);
    }

    return queryPoolSize;
}

// =====================================================================================================================
Result Device::CreateQueryPool(
    const QueryPoolCreateInfo& createInfo,
    void*                      pPlacementAddr,
    IQueryPool**               ppQueryPool
    ) const
{
    if (createInfo.queryPoolType == QueryPoolType::Occlusion)
    {
        *ppQueryPool = PAL_PLACEMENT_NEW(pPlacementAddr) OcclusionQueryPool(*this, createInfo);
    }
    else if (createInfo.queryPoolType == QueryPoolType::PipelineStats)
    {
        *ppQueryPool = PAL_PLACEMENT_NEW(pPlacementAddr) PipelineStatsQueryPool(*this, createInfo);
    }
    else if (createInfo.queryPoolType == QueryPoolType::StreamoutStats)
    {
        *ppQueryPool = PAL_PLACEMENT_NEW(pPlacementAddr) StreamoutStatsQueryPool(*this, createInfo);
    }

    return Result::Success;
}

// =====================================================================================================================
size_t Device::GetCmdBufferSize(
    const CmdBufferCreateInfo& createInfo
    ) const
{
    size_t cmdBufferSize = 0;

    if (createInfo.queueType == QueueTypeCompute)
    {
        cmdBufferSize = sizeof(ComputeCmdBuffer);
    }
    else if (createInfo.queueType == QueueTypeUniversal)
    {
        cmdBufferSize = UniversalCmdBuffer::GetSize(*this);
    }

    return cmdBufferSize;
}

// =====================================================================================================================
Result Device::CreateCmdBuffer(
    const CmdBufferCreateInfo& createInfo,
    void*                      pPlacementAddr,
    CmdBuffer**                ppCmdBuffer)
{
    Result result = Result::ErrorInvalidQueueType;

    if (createInfo.queueType == QueueTypeCompute)
    {
        result = Result::Success;

        *ppCmdBuffer = PAL_PLACEMENT_NEW(pPlacementAddr) ComputeCmdBuffer(*this, createInfo);
    }
    else if (createInfo.queueType == QueueTypeUniversal)
    {
        result = Result::Success;

        *ppCmdBuffer = PAL_PLACEMENT_NEW(pPlacementAddr) UniversalCmdBuffer(*this, createInfo);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetIndirectCmdGeneratorSize(
    const IndirectCmdGeneratorCreateInfo& createInfo,
    Result*                               pResult
    ) const
{
    if (pResult != nullptr)
    {
        (*pResult) = Pm4::IndirectCmdGenerator::ValidateCreateInfo(createInfo);
    }

    return IndirectCmdGenerator::GetSize(createInfo);
}

// =====================================================================================================================
Result Device::CreateIndirectCmdGenerator(
    const IndirectCmdGeneratorCreateInfo& createInfo,
    void*                                 pPlacementAddr,
    IIndirectCmdGenerator**               ppGenerator
    ) const
{
    PAL_ASSERT((pPlacementAddr != nullptr) && (ppGenerator != nullptr));
#if PAL_ENABLE_PRINTS_ASSERTS
    PAL_ASSERT(Pm4::IndirectCmdGenerator::ValidateCreateInfo(createInfo) == Result::Success);
#endif

    (*ppGenerator) = PAL_PLACEMENT_NEW(pPlacementAddr) IndirectCmdGenerator(*this, createInfo);
    return Result::Success;
}

// =====================================================================================================================
size_t Device::GetColorTargetViewSize(
    Result* pResult
    ) const
{
    if (pResult != nullptr)
    {
        (*pResult) = Result::Success;
    }

    return sizeof(ColorTargetView);
}

// =====================================================================================================================
// Creates a Gfx6 implementation of Pal::IColorTargetView
Result Device::CreateColorTargetView(
    const ColorTargetViewCreateInfo&  createInfo,
    ColorTargetViewInternalCreateInfo internalInfo,
    void*                             pPlacementAddr,
    IColorTargetView**                ppColorTargetView
    ) const
{
    (*ppColorTargetView) = PAL_PLACEMENT_NEW(pPlacementAddr) ColorTargetView(*this, createInfo, internalInfo);

    return Result::Success;
}

// =====================================================================================================================
size_t Device::GetDepthStencilViewSize(
    Result* pResult) const
{
    if (pResult != nullptr)
    {
        (*pResult) = Result::Success;
    }

    return sizeof(DepthStencilView);
}

// =====================================================================================================================
// Creates a Gfx6 implementation of Pal::IDepthStencilView
Result Device::CreateDepthStencilView(
    const DepthStencilViewCreateInfo&         createInfo,
    const DepthStencilViewInternalCreateInfo& internalInfo,
    void*                                     pPlacementAddr,
    IDepthStencilView**                       ppDepthStencilView
    ) const
{
    (*ppDepthStencilView) = PAL_PLACEMENT_NEW(pPlacementAddr) DepthStencilView(this, createInfo, internalInfo);

    return Result::Success;
}

// =====================================================================================================================
size_t Device::GetPerfExperimentSize(
    const PerfExperimentCreateInfo& createInfo,
    Result*                         pResult
    ) const
{
    if (pResult != nullptr)
    {
        (*pResult) = Result::Success;
    }

    return sizeof(PerfExperiment);
}

// =====================================================================================================================
Result Device::CreatePerfExperiment(
    const PerfExperimentCreateInfo& createInfo,
    void*                           pPlacementAddr,
    IPerfExperiment**               ppPerfExperiment
    ) const
{
    PerfExperiment* pPerfExperiment = PAL_PLACEMENT_NEW(pPlacementAddr) PerfExperiment(this, createInfo);
    Result          result          = pPerfExperiment->Init();

    if (result == Result::Success)
    {
        (*ppPerfExperiment) = pPerfExperiment;
    }
    else
    {
        pPerfExperiment->Destroy();
    }

    return result;
}

// =====================================================================================================================
Result Device::CreateCmdUploadRingInternal(
    const CmdUploadRingCreateInfo& createInfo,
    Pal::CmdUploadRing**           ppCmdUploadRing)
{
    return CmdUploadRing::CreateInternal(createInfo, this, ppCmdUploadRing);
}

// =====================================================================================================================
// Returns the value of SQ_BUF_RSRC_WORD2.NUM_RECORDS based on this device's GFXIP level.
gpusize Device::CalcNumRecords(
    gpusize range,  // Size in bytes of the buffer.
    gpusize stride  // Size in bytes of one element in the buffer.
    ) const
{
    // On GFX8+ GPUs, the units of the "num_records" field is always in terms of bytes.
    gpusize numRecords = range;

    if (Parent()->ChipProperties().gfxLevel <= GfxIpLevel::GfxIp7)
    {
        // On GFX6 and GFX7 GPUs, the units of the "num_records" field is in terms of the stride.
        numRecords = (stride <= 1) ? range : (range / stride);
    }
    else
    {
        // On GFX8+ GPUs, the units of the "num_records" field is always in terms of bytes.
        // We need to round down to a multiple of stride.  This happens as a side effect of dividing by stride for
        // GFX6 and GFX7.
        if (stride > 1)
        {
            numRecords = RoundDownToMultiple(range, stride);
        }
    }

    return numRecords;
}

// =====================================================================================================================
// Returns the memory range covered by a buffer SRD.
gpusize Device::CalcBufferSrdRange(
    const BufferSrd& srd
    ) const
{
    // On GFX8+ GPUs, the units of the "num_records" field is always in terms of bytes.
    gpusize range = srd.word2.bits.NUM_RECORDS;

    // On GFX and GFX6 GPUs, the units of the "num_records" field is in terms of the stride when the stride is nonzero.
    if (((Parent()->ChipProperties().gfxLevel == GfxIpLevel::GfxIp6) ||
         (Parent()->ChipProperties().gfxLevel == GfxIpLevel::GfxIp7)) &&
        (srd.word1.bits.STRIDE != 0))
    {
        range = (srd.word2.bits.NUM_RECORDS * srd.word1.bits.STRIDE);
    }

    return range;
}

// =====================================================================================================================
// Returns the proper alignment in bytes according to the alignment of CP DMA
gpusize Device::CpDmaCompatAlignment(
    const Device& device,
    gpusize       alignment)
{
    const Gfx6PalSettings& settings = GetGfx6Settings(*(device.Parent()));

    return Max(alignment, static_cast<gpusize>(settings.cpDmaSrcAlignment));
}

// =====================================================================================================================
// Fills in the AddrLib create input fields based on chip specific properties. Note: this function must not use any
// settings or member variables that depend on settings because AddrLib is initialized before settings are committed.
Result Device::InitAddrLibCreateInput(
    ADDR_CREATE_FLAGS*   pCreateFlags, // [out] Creation Flags
    ADDR_REGISTER_VALUE* pRegValue     // [out] Register Value
    ) const
{
    const GpuChipProperties& chipProps = m_pParent->ChipProperties();

    // Init create flags
    pCreateFlags->useTileIndex = 1;

    // This flag must be set for the Swizzled Mip-Map bug workaround.
    //
    // Normally a workaround like this would be tied to a setting, but this workaround must be enabled before
    // settings have been committed so we simply enable it for all GfxIp6 devices.
    pCreateFlags->checkLast2DLevel = (chipProps.gfxLevel == GfxIpLevel::GfxIp6);

    // Copy register values required by AddrLib
    pRegValue->gbAddrConfig    = chipProps.gfx6.gbAddrConfig;
    pRegValue->backendDisables = chipProps.gfx6.backendDisableMask;

    if (chipProps.gfxLevel >= GfxIpLevel::GfxIp7)
    {
        regMC_ARB_RAMCFG__CI__VI mcArbRamCfg;
        mcArbRamCfg.u32All = chipProps.gfx6.mcArbRamcfg;

        pRegValue->noOfBanks = mcArbRamCfg.bits.NOOFBANK;
        pRegValue->noOfRanks = mcArbRamCfg.bits.NOOFRANKS;
    }
    else if (chipProps.gfxLevel == GfxIpLevel::GfxIp6)
    {
        regMC_ARB_RAMCFG__SI mcArbRamCfg;
        mcArbRamCfg.u32All = chipProps.gfx6.mcArbRamcfg;

        pRegValue->noOfBanks = mcArbRamCfg.bits.NOOFBANK;
        pRegValue->noOfRanks = mcArbRamCfg.bits.NOOFRANKS;
    }
    else
    {
        // Unrecognized chip family
        PAL_ASSERT_ALWAYS();
    }

    pRegValue->pTileConfig      = &chipProps.gfx6.gbTileMode[0];
    pRegValue->noOfEntries      = sizeof(chipProps.gfx6.gbTileMode) / sizeof(uint32);
    pRegValue->pMacroTileConfig = &chipProps.gfx6.gbMacroTileMode[0];
    pRegValue->noOfMacroEntries = sizeof(chipProps.gfx6.gbMacroTileMode) / sizeof(uint32);

    return Result::Success;
}

// =====================================================================================================================
// Helper function telling what kind of DCC format encoding an image created with
// the specified creation image and all of its potential view formats will end up with
DccFormatEncoding Device::ComputeDccFormatEncoding(
    const SwizzledFormat& swizzledFormat,
    const SwizzledFormat* pViewFormats,
    uint32                viewFormatCount
    ) const
{
    DccFormatEncoding dccFormatEncoding = DccFormatEncoding::Optimal;

    if (viewFormatCount == AllCompatibleFormats)
    {
        // If all compatible formats are allowed as view formats then the image is not DCC compatible as none of
        // the format compatibility classes comprise only of formats that are DCC compatible.
        dccFormatEncoding = DccFormatEncoding::Incompatible;
    }
    else
    {
        // If an array of possible view formats is specified at image creation time we can check whether all of
        // those are DCC compatible with each other or not.
        // The channel format has to match for all of these formats, but otherwise the number format may change
        // as long as all formats are from within one of the following compatible buckets:
        // (1) Unorm, Uint, Uscaled, and Srgb
        // (2) Snorm, Sint, and Sscaled
        const bool baseFormatIsUnsigned = Formats::IsUnorm(swizzledFormat.format)   ||
                                          Formats::IsUint(swizzledFormat.format)    ||
                                          Formats::IsUscaled(swizzledFormat.format) ||
                                          Formats::IsSrgb(swizzledFormat.format);

        const bool baseFormatIsSigned = Formats::IsSnorm(swizzledFormat.format) ||
                                        Formats::IsSint(swizzledFormat.format)  ||
                                        Formats::IsSscaled(swizzledFormat.format);

        const bool baseFormatIsFloat = Formats::IsFloat(swizzledFormat.format);

        // If viewFormatCount is not zero then pViewFormats must point to a valid array.
        PAL_ASSERT((viewFormatCount == 0) || (pViewFormats != nullptr));

        const SwizzledFormat* pFormats = pViewFormats;

        for (uint32 i = 0; i < viewFormatCount; ++i)
        {
            const bool viewFormatIsUnsigned = Formats::IsUnorm(pFormats[i].format)   ||
                                              Formats::IsUint(pFormats[i].format)    ||
                                              Formats::IsUscaled(pFormats[i].format) ||
                                              Formats::IsSrgb(pFormats[i].format);

            const bool viewFormatIsSigned = Formats::IsSnorm(pFormats[i].format) ||
                                            Formats::IsSint(pFormats[i].format)  ||
                                            Formats::IsSscaled(pFormats[i].format);

            const bool viewFormatIsFloat = Formats::IsFloat(pFormats[i].format);

            if ((baseFormatIsFloat != viewFormatIsFloat) ||
                (Formats::ShareChFmt(swizzledFormat.format, pFormats[i].format) == false) ||
                (swizzledFormat.swizzle.swizzleValue != pFormats[i].swizzle.swizzleValue))
            {
                // If any format is incompatible fallback to non DCC.
                dccFormatEncoding = DccFormatEncoding::Incompatible;
                break;
            }
            else if ((baseFormatIsUnsigned != viewFormatIsUnsigned) ||
                     (baseFormatIsSigned != viewFormatIsSigned))
            {
                //dont have to turn off DCC entirely only Constant Encoding
                dccFormatEncoding = DccFormatEncoding::SignIndependent;
            }
        }
    }

    return dccFormatEncoding;
}

// =====================================================================================================================
// Computes the image view SRD DEPTH field based on image view parameters
static uint32 ComputeImageViewDepth(
    const ImageViewInfo&   viewInfo,
    const ImageInfo&       imageInfo,
    const SubResourceInfo& subresInfo)
{
    constexpr uint32 NumCubemapFaces = 6;

    uint32 depth = 0;

    const ImageCreateInfo& imageCreateInfo = viewInfo.pImage->GetImageCreateInfo();

    // From reg spec: Units are "depth - 1", so 0 = 1 slice, 1= 2 slices.
    // If the image type is 3D, then the DEPTH field is the image's depth - 1.
    // If the view type is CUBE, the DEPTH filed is the image's number of array slices / 6 - 1.
    // Otherwise, the DEPTH field is the image's number of array slices - 1.

    // Note that we can't use viewInfo.viewType here since 3D image may be viewed as 2D (array).
    if (imageCreateInfo.imageType == ImageType::Tex3d)
    {
        depth = (subresInfo.extentTexels.depth - 1);
    }
    else if (viewInfo.viewType == ImageViewType::TexCube)
    {
        // Cube is special as the array size is divided by 6. If an array of 9-slice with mipmap is viewed as cube,
        // AddrLib does a power-of-two pad in number of slices so the padded array size is 16. If 9 is still used here,
        // HW would read (0 + 1) * 6 then power-of-two pad 6 to 8 which results wrong slices for mip levels.
        // Note that 3D and array still work because HW always does the power-of-two pad.
        depth = ((subresInfo.actualArraySize / NumCubemapFaces) - 1);
    }
    else
    {
        depth = (imageCreateInfo.arraySize - 1);
    }

    return depth;
}

// These compile-time assertions verify the assumption that Pal compare function enums are identical to the HW values.
static_assert(SQ_TEX_DEPTH_COMPARE_NEVER == static_cast<uint32>(CompareFunc::Never),
              "HW value is not identical to Pal::CompareFunc enum value.");
static_assert(SQ_TEX_DEPTH_COMPARE_LESS == static_cast<uint32>(CompareFunc::Less),
              "HW value is not identical to Pal::CompareFunc enum value.");
static_assert(SQ_TEX_DEPTH_COMPARE_EQUAL == static_cast<uint32>(CompareFunc::Equal),
              "HW value is not identical to Pal::CompareFunc enum value.");
static_assert(SQ_TEX_DEPTH_COMPARE_LESSEQUAL == static_cast<uint32>(CompareFunc::LessEqual),
              "HW value is not identical to Pal::CompareFunc enum value.");
static_assert(SQ_TEX_DEPTH_COMPARE_GREATER == static_cast<uint32>(CompareFunc::Greater),
              "HW value is not identical to Pal::CompareFunc enum value.");
static_assert(SQ_TEX_DEPTH_COMPARE_NOTEQUAL == static_cast<uint32>(CompareFunc::NotEqual),
              "HW value is not identical to Pal::CompareFunc enum value.");
static_assert(SQ_TEX_DEPTH_COMPARE_GREATEREQUAL == static_cast<uint32>(CompareFunc::GreaterEqual),
              "HW value is not identical to Pal::CompareFunc enum value.");
static_assert(SQ_TEX_DEPTH_COMPARE_ALWAYS == static_cast<uint32>(CompareFunc::Always),
              "HW value is not identical to Pal::CompareFunc enum value.");

// Converts HW enumerations (mag, min, mipfilter) to their equivalent Pal::TexFilter enumeration value.
#define GET_PAL_TEX_FILTER_VALUE(magFilter, minFilter, mipFilter)           \
                (((magFilter << SQ_IMG_SAMP_WORD2__XY_MAG_FILTER__SHIFT) |  \
                  (minFilter << SQ_IMG_SAMP_WORD2__XY_MIN_FILTER__SHIFT) |  \
                  (mipFilter << SQ_IMG_SAMP_WORD2__MIP_FILTER__SHIFT))   >> \
                 SQ_IMG_SAMP_WORD2__XY_MAG_FILTER__SHIFT)

// The TexFilter enumerations are encoded to match the HW enumeration values. Make sure the two sets of enumerations
// match up.
static_assert(static_cast<uint32>(XyFilterPoint) == SQ_TEX_XY_FILTER_POINT,
                                                         "HW value should be identical to Pal::XyFilter enum value.");
static_assert(static_cast<uint32>(XyFilterLinear) == SQ_TEX_XY_FILTER_BILINEAR,
                                                         "HW value should be identical to Pal::XyFilter enum value.");
static_assert(static_cast<uint32>(XyFilterAnisotropicPoint) == SQ_TEX_XY_FILTER_ANISO_POINT,
                                                         "HW value should be identical to Pal::XyFilter enum value.");
static_assert(static_cast<uint32>(XyFilterAnisotropicLinear) == SQ_TEX_XY_FILTER_ANISO_BILINEAR,
                                                         "HW value should be identical to Pal::XyFilter enum value.");
static_assert(static_cast<uint32>(ZFilterNone) == SQ_TEX_Z_FILTER_NONE,
                                                         "HW value should be identical to Pal::ZFilter enum value.");
static_assert(static_cast<uint32>(ZFilterPoint) == SQ_TEX_Z_FILTER_POINT,
                                                         "HW value should be identical to Pal::ZFilter enum value.");
static_assert(static_cast<uint32>(ZFilterLinear) == SQ_TEX_Z_FILTER_LINEAR,
                                                         "HW value should be identical to Pal::ZFilter enum value.");
static_assert(static_cast<uint32>(MipFilterNone) == SQ_TEX_MIP_FILTER_NONE,
                                                         "HW value should be identical to Pal::MipFilter enum value.");
static_assert(static_cast<uint32>(MipFilterPoint) == SQ_TEX_MIP_FILTER_POINT,
                                                         "HW value should be identical to Pal::MipFilter enum value.");
static_assert(static_cast<uint32>(MipFilterLinear) == SQ_TEX_MIP_FILTER_LINEAR,
                                                         "HW value should be identical to Pal::MipFilter enum value.");
static_assert(static_cast<uint32>(XyFilterCount) <= 4,
                                  "Only 2 bits allocated to magnification and minification members of Pal::TexFilter");
static_assert(static_cast<uint32>(ZFilterCount) <= 4,
                                  "Only 2 bits allocated to zFilter member of Pal::TexFilter");
static_assert(static_cast<uint32>(MipFilterCount) <= 4,
                                  "Only 2 bits allocated to mipFilter member of Pal::TexFilter");

// =====================================================================================================================
// Determine the appropriate SQ clamp mode based on the given TexAddressMode enum value.
static SQ_TEX_CLAMP GetAddressClamp(
    TexAddressMode texAddress)
{
    static const SQ_TEX_CLAMP PalTexAddrToHwTbl[] =
    {
        SQ_TEX_WRAP,                    // TexAddressMode::Wrap
        SQ_TEX_MIRROR,                  // TexAddressMode::Mirror
        SQ_TEX_CLAMP_LAST_TEXEL,        // TexAddressMode::Clamp
        SQ_TEX_MIRROR_ONCE_LAST_TEXEL,  // TexAddressMode::MirrorOnce
        SQ_TEX_CLAMP_BORDER,            // TexAddressMode::ClampBorder
        SQ_TEX_MIRROR_ONCE_HALF_BORDER, // TexAddressMode::MirrorClampHalfBorder
        SQ_TEX_CLAMP_HALF_BORDER,       // TexAddressMode::ClampHalfBorder
        SQ_TEX_MIRROR_ONCE_BORDER,      // TexAddressMode::MirrorClampBorder
    };

    static_assert((ArrayLen(PalTexAddrToHwTbl) == static_cast<size_t>(TexAddressMode::Count)),
                  "Hardware table for Texture Address Mode does not match Pal::TexAddressMode enum.");

    return PalTexAddrToHwTbl[static_cast<uint32>(texAddress)];
}

// =====================================================================================================================
// Determine if anisotropic filtering is enabled
constexpr bool IsAnisoEnabled(
    TexFilter texfilter)
{
    return ((texfilter.magnification == XyFilterAnisotropicPoint)  ||
            (texfilter.magnification == XyFilterAnisotropicLinear) ||
            (texfilter.minification  == XyFilterAnisotropicPoint)  ||
            (texfilter.minification  == XyFilterAnisotropicLinear));
}

// =====================================================================================================================
// Determine the appropriate Anisotropic filtering mode.
// NOTE: For values of anisotropy not natively supported by HW, we clamp to the closest value less than what was
//       requested.
static SQ_TEX_ANISO_RATIO GetAnisoRatio(
    const SamplerInfo* pInfo)
{
    SQ_TEX_ANISO_RATIO anisoRatio = SQ_TEX_ANISO_RATIO_1;

    if (IsAnisoEnabled(pInfo->filter))
    {
        if (pInfo->maxAnisotropy < 2)
        {
            // Nothing to do.
        }
        else if (pInfo->maxAnisotropy < 4)
        {
            anisoRatio = SQ_TEX_ANISO_RATIO_2;
        }
        else if (pInfo->maxAnisotropy < 8)
        {
            anisoRatio = SQ_TEX_ANISO_RATIO_4;
        }
        else if (pInfo->maxAnisotropy < 16)
        {
            anisoRatio = SQ_TEX_ANISO_RATIO_8;
        }
        else if (pInfo->maxAnisotropy == 16)
        {
            anisoRatio = SQ_TEX_ANISO_RATIO_16;
        }
    }

    return anisoRatio;
}

// =====================================================================================================================
// Gfx6/7/8 helper function for patching a pipeline's shader internal SRD table.
void Device::PatchPipelineInternalSrdTable(
    void*       pDstSrdTable,   // Out: Patched SRD table in mapped GPU memory
    const void* pSrcSrdTable,   // In: Unpatched SRD table from ELF binary
    size_t      tableBytes,
    gpusize     dataGpuVirtAddr
    ) const
{
    // See Pipeline::PerformRelocationsAndUploadToGpuMemory() for more information.

    auto*const pDstSrd = static_cast<BufferSrd*>(pDstSrdTable);

    for (uint32 i = 0; i < (tableBytes / sizeof(BufferSrd)); ++i)
    {
        // pSrcSrdTable may be unaligned, so do unaligned memcpy's rather than direct (aligned) pointer accesses.
        BufferSrd srd;
        memcpy(&srd, VoidPtrInc(pSrcSrdTable, (i * sizeof(BufferSrd))), sizeof(BufferSrd));

        const gpusize patchedGpuVa =
            ((static_cast<gpusize>(srd.word1.bits.BASE_ADDRESS_HI) << 32) | srd.word0.bits.BASE_ADDRESS) +
            dataGpuVirtAddr;

        srd.word0.bits.BASE_ADDRESS    = LowPart(patchedGpuVa);
        srd.word1.bits.BASE_ADDRESS_HI = HighPart(patchedGpuVa);

        // Note: The entire unpatched SRD table has already been copied to GPU memory wholesale.  We just need to
        // modify the first quadword of the SRD to patch the addresses.
        memcpy((pDstSrd + i), &srd, sizeof(uint64));
    }
}

// =====================================================================================================================
// Gfx6+ specific function for creating typed buffer view SRDs. Installed in the function pointer table of the parent
// device during initialization.
void PAL_STDCALL Device::CreateTypedBufferViewSrds(
    const IDevice*        pDevice,
    uint32                count,
    const BufferViewInfo* pBufferViewInfo,
    void*                 pOut)
{
    PAL_ASSERT((pDevice != nullptr) && (pOut != nullptr) && (pBufferViewInfo != nullptr) && (count > 0));
    const auto*const pGfxDevice = static_cast<const Device*>(static_cast<const Pal::Device*>(pDevice)->GetGfxDevice());
    const auto*const pFmtInfo   = MergedChannelFmtInfoTbl(pGfxDevice->Parent()->ChipProperties().gfxLevel);

    for (uint32 idx = 0; idx < count; ++idx)
    {
        const auto& view = pBufferViewInfo[idx];

        PAL_ASSERT(IsValidTypedBufferView(view));

        BufferSrd srd = { };
        srd.word0.bits.BASE_ADDRESS    = LowPart(view.gpuAddr);
        srd.word1.bits.BASE_ADDRESS_HI = HighPart(view.gpuAddr);
        srd.word1.bits.STRIDE          = view.stride;
        srd.word2.bits.NUM_RECORDS     = pGfxDevice->CalcNumRecords(view.range, view.stride);
        srd.word3.bits.TYPE            = SQ_RSRC_BUF;
        if (static_cast<const Pal::Device*>(pDevice)->MemoryProperties().flags.iommuv2Support)
        {
            srd.word3.bits.ATC__CI__VI = ((HighPart(view.gpuAddr) >> 0x10) != 0)
                ? 0 : ((LowPart(view.gpuAddr) != 0) || ((HighPart(view.gpuAddr) & 0xFFFF) != 0));
        }

        srd.word3.bits.DST_SEL_X   = Formats::Gfx6::HwSwizzle(view.swizzledFormat.swizzle.r);
        srd.word3.bits.DST_SEL_Y   = Formats::Gfx6::HwSwizzle(view.swizzledFormat.swizzle.g);
        srd.word3.bits.DST_SEL_Z   = Formats::Gfx6::HwSwizzle(view.swizzledFormat.swizzle.b);
        srd.word3.bits.DST_SEL_W   = Formats::Gfx6::HwSwizzle(view.swizzledFormat.swizzle.a);
        srd.word3.bits.DATA_FORMAT = Formats::Gfx6::HwBufDataFmt(pFmtInfo, view.swizzledFormat.format);
        srd.word3.bits.NUM_FORMAT  = Formats::Gfx6::HwBufNumFmt(pFmtInfo, view.swizzledFormat.format);

        memcpy(pOut, &srd, sizeof(srd));
        pOut = VoidPtrInc(pOut, sizeof(srd));
    }
}

// =====================================================================================================================
// Gfx6+ specific function for creating untyped buffer view SRDs. Installed in the function pointer table of the parent
// device during initialization.
void PAL_STDCALL Device::CreateUntypedBufferViewSrds(
    const IDevice*        pDevice,
    uint32                count,
    const BufferViewInfo* pBufferViewInfo,
    void*                 pOut)
{
    PAL_ASSERT((pDevice != nullptr) && (pOut != nullptr) && (pBufferViewInfo != nullptr) && (count > 0));
    const auto*const pGfxDevice = static_cast<const Device*>(static_cast<const Pal::Device*>(pDevice)->GetGfxDevice());

    BufferSrd* pOutSrd = static_cast<BufferSrd*>(pOut);

    for (uint32 idx = 0; idx < count; ++idx, ++pBufferViewInfo)
    {
        PAL_ASSERT((pBufferViewInfo->gpuAddr != 0) || (pBufferViewInfo->range == 0));

        pOutSrd->word0.bits.BASE_ADDRESS = LowPart(pBufferViewInfo->gpuAddr);

        pOutSrd->word1.u32All =
            ((HighPart(pBufferViewInfo->gpuAddr) << SQ_BUF_RSRC_WORD1__BASE_ADDRESS_HI__SHIFT) |
             (static_cast<uint32>(pBufferViewInfo->stride) << SQ_BUF_RSRC_WORD1__STRIDE__SHIFT));

        pOutSrd->word2.bits.NUM_RECORDS = pGfxDevice->CalcNumRecords(pBufferViewInfo->range, pBufferViewInfo->stride);

        PAL_ASSERT(Formats::IsUndefined(pBufferViewInfo->swizzledFormat.format));

        if (pBufferViewInfo->gpuAddr != 0)
        {
            uint32 word3Atc = 0;
            if (static_cast<const Pal::Device*>(pDevice)->MemoryProperties().flags.iommuv2Support)
            {
                word3Atc = ((HighPart(pBufferViewInfo->gpuAddr) >> 0x10) != 0) ?
                           0 : ((LowPart(pBufferViewInfo->gpuAddr) != 0) ||
                                ((HighPart(pBufferViewInfo->gpuAddr) & 0xFFFF) != 0));
            }

            pOutSrd->word3.u32All = ((SQ_RSRC_BUF << SQ_BUF_RSRC_WORD3__TYPE__SHIFT)     |
                                     (word3Atc << SQ_BUF_RSRC_WORD3__ATC__SHIFT__CI__VI) |
                                     (SQ_SEL_X << SQ_BUF_RSRC_WORD3__DST_SEL_X__SHIFT)   |
                                     (SQ_SEL_Y << SQ_BUF_RSRC_WORD3__DST_SEL_Y__SHIFT)   |
                                     (SQ_SEL_Z << SQ_BUF_RSRC_WORD3__DST_SEL_Z__SHIFT)   |
                                     (SQ_SEL_W << SQ_BUF_RSRC_WORD3__DST_SEL_W__SHIFT)   |
                                     (BUF_DATA_FORMAT_32 << SQ_BUF_RSRC_WORD3__DATA_FORMAT__SHIFT) |
                                     (BUF_NUM_FORMAT_UINT << SQ_BUF_RSRC_WORD3__NUM_FORMAT__SHIFT));
        }
        else
        {
            pOutSrd->word3.u32All = 0;
        }

        pOutSrd++;
    }
}

// =====================================================================================================================
// Gfx6+ specific function for creating image view SRDs. Installed in the function pointer table of the parent device
// during initialization.
void PAL_STDCALL Device::CreateImageViewSrds(
    const IDevice*       pDevice,
    uint32               count,
    const ImageViewInfo* pImgViewInfo,
    void*                pOut)
{
    PAL_ASSERT((pDevice != nullptr) && (pOut != nullptr) && (pImgViewInfo != nullptr) && (count > 0));
    const auto*const pGfxDevice = static_cast<const Device*>(static_cast<const Pal::Device*>(pDevice)->GetGfxDevice());
    const auto*const pFmtInfo   = MergedChannelFmtInfoTbl(pGfxDevice->Parent()->ChipProperties().gfxLevel);

    ImageSrd* pSrds = static_cast<ImageSrd*>(pOut);

    for (uint32 i = 0; i < count; ++i)
    {
        const ImageViewInfo&   viewInfo        = pImgViewInfo[i];
        PAL_ASSERT(viewInfo.subresRange.numPlanes == 1);

        const Image&           image           = *GetGfx6Image(viewInfo.pImage);
        const auto*const       pParent         = image.Parent();
        const ImageInfo&       imageInfo       = pParent->GetImageInfo();
        const ImageCreateInfo& imageCreateInfo = pParent->GetImageCreateInfo();
        const bool             imgIsBc         = Formats::IsBlockCompressed(imageCreateInfo.swizzledFormat.format);
        PAL_ASSERT((viewInfo.possibleLayouts.engines != 0) && (viewInfo.possibleLayouts.usages != 0 ));

        ImageSrd srd = {};

        // Calculate the subresource ID of the first subresource in this image view.
        SubresId subresource = {};
        subresource.plane = viewInfo.subresRange.startSubres.plane;

        uint32 baseArraySlice = viewInfo.subresRange.startSubres.arraySlice;
        uint32 baseMipLevel   = viewInfo.subresRange.startSubres.mipLevel;

        const ChNumFormat imageViewFormat = viewInfo.swizzledFormat.format;

        const SubResourceInfo& startSubresInfo =
            *image.Parent()->SubresourceInfo(viewInfo.subresRange.startSubres);

        // There are some cases where the view must be setup with base level 0:
        // 1. RPM wants to BLT to the tail of a compressed texture.  When setting up a view where each "pixel"
        //    corresponds to a 4x4 block, the texture unit thinks that the 4x4 level is really 1x1, and there is no way
        //    to address the 2x2 and 1x1 levels.
        // 2. Creating a view of the depth plane of a depth stencil surface. Depth slices may be padded to match the
        //    alignment requirement of the stencil slices. This extra padding prevents the depth plane from being
        //    viewed as a mip chain.
        // 3. RPM wants to BLT to the smaller mips of a macro-pixel-packed texture.  When setting up a view where each
        //    "pixel" corresponds to half a 2x1 macro-pixel, the texture unit cannot be used to compute the dimensions
        //    of each smaller mipmap level. In this case, we need to treat each mip as an individual resource and pad
        //    the width dimension up to the next even number.
        // 4. For 96 bit bpp formats(X32Y32Z32_Uint/X32Y32Z32_Sint/X32Y32Z32_Float), X32_Uint formated image view srd
        //    might be created upon the image for image copy operation. Extent of mipmaped level of X32_Uint and
        //    mipmaped level of the original X32Y32Z32_* format might mismatch, especially on the last several mips.
        //    Thus, it could be problemtic to use 256b address of zero-th mip + mip level mode. Instead we shall
        //    adopt 256b address of startsubres's miplevel.
        bool forceBaseMip   = false;
        bool padToEvenWidth = false;

        if (Formats::IsDepthStencilOnly(imageCreateInfo.swizzledFormat.format) &&
            (viewInfo.subresRange.startSubres.plane == 0)                      &&
            (viewInfo.subresRange.numMips > 1))
        {
            PAL_ASSERT_ALWAYS_MSG("See above comment#2");
        }

        if (viewInfo.subresRange.numMips == 1)
        {
            if (imgIsBc || pParent->IsDepthPlane(viewInfo.subresRange.startSubres.plane))
            {
                forceBaseMip = true;
            }
            else if (Formats::IsMacroPixelPacked(imageCreateInfo.swizzledFormat.format) &&
                     (Formats::IsMacroPixelPacked(viewInfo.swizzledFormat.format) == false))
            {
                forceBaseMip   = true;
                padToEvenWidth = true;
            }
            else if (Formats::IsYuvPlanar(imageCreateInfo.swizzledFormat.format) &&
                     (viewInfo.subresRange.numSlices == 1))
            {
                subresource.arraySlice = baseArraySlice;
                baseArraySlice = 0;
            }
            else if ((startSubresInfo.bitsPerTexel != Formats::BitsPerPixel(imageViewFormat)) &&
                     (startSubresInfo.bitsPerTexel == 96))
            {
                forceBaseMip = true;
            }
        }

        if (forceBaseMip)
        {
            subresource.mipLevel = baseMipLevel;

            baseMipLevel = 0;
        }

        const SubResourceInfo&         subresInfo = *image.Parent()->SubresourceInfo(subresource);
        const AddrMgr1::TileInfo*const pTileInfo  = AddrMgr1::GetTileInfo(image.Parent(), subresource);

        bool includePadding = (viewInfo.flags.includePadding != 0);

        Extent3d extent       = subresInfo.extentTexels;
        Extent3d actualExtent = subresInfo.actualExtentTexels;

        if (padToEvenWidth)
        {
            extent.width       += (extent.width       & 1);
            actualExtent.width += (actualExtent.width & 1);
        }

        // The view should be in terms of texels except in two special cases when we're operating in terms of elements:
        // 1. Viewing a compressed image in terms of blocks. For BC images elements are blocks, so if the caller gave
        //    us an uncompressed view format we assume they want to view blocks.
        // 2. Copying to an "expanded" format (e.g., R32G32B32). In this case we can't do native format writes so we're
        //    going to write each element independently. The trigger for this case is a mismatched bpp.
        // 3. Viewing a YUV-packed image with a non-YUV-packed format when the view format is allowed for view formats
        //    with twice the bpp. In this case, the effective width of the view is half that of the base image.
        // 4. Viewing a YUV-planar Image which has multiple array slices. In this case, the texture hardware has no way
        //    to know about the padding in between array slices of the same plane (due to the other plane's slices being
        //    interleaved). In this case, we pad out the actual height of the view to span all planes (so that the view
        //    can access each array slice). This has the unfortunate side-effect of making normalized texture
        //    coordinates inaccurate. However, this is required for access to multiple slices (a feature required by D3D
        //    conformance tests).
        if ((imgIsBc && (Formats::IsBlockCompressed(imageViewFormat) == false)) ||
            (subresInfo.bitsPerTexel != Formats::BitsPerPixel(imageViewFormat)))
        {
            extent       = subresInfo.extentElements;
            actualExtent = subresInfo.actualExtentElements;
        }

        if (Formats::IsYuvPacked(subresInfo.format.format)   &&
            (Formats::IsYuvPacked(imageViewFormat) == false) &&
            ((subresInfo.bitsPerTexel << 1) == Formats::BitsPerPixel(imageViewFormat)))
        {
            // Changing how we interpret the bits-per-pixel of the subresource wreaks havoc with any tile swizzle
            // pattern used. This will only work for linear-tiled Images.
            PAL_ASSERT(image.IsSubResourceLinear(subresource));

            extent.width       >>= 1;
            actualExtent.width >>= 1;
        }
        else if (Formats::IsYuvPlanar(imageCreateInfo.swizzledFormat.format))
        {
            if (viewInfo.subresRange.numSlices > 1)
            {
                image.PadYuvPlanarViewActualExtent(subresource, &actualExtent);
                includePadding = true;
                // Sampling using this view will not work correctly, but direct image loads will work.
                // This path is only expected to be used by RPM operations.
                PAL_ALERT_ALWAYS();
            }
            else
            {
                // We must use base slice 0 for correct normalized coordinates on a YUV planar surface.
                PAL_ASSERT(baseArraySlice == 0);
            }
        }

        constexpr uint32 Gfx6MinLodIntBits  = 4;
        constexpr uint32 Gfx6MinLodFracBits = 8;

        srd.word0.u32All = 0;
        // IMG RSRC MIN_LOD field is unsigned
        srd.word1.bits.MIN_LOD     = Math::FloatToUFixed(viewInfo.minLod, Gfx6MinLodIntBits, Gfx6MinLodFracBits, true);
        srd.word1.bits.DATA_FORMAT = Formats::Gfx6::HwImgDataFmt(pFmtInfo, imageViewFormat);
        srd.word1.bits.NUM_FORMAT  = Formats::Gfx6::HwImgNumFmt(pFmtInfo, imageViewFormat);

        if (includePadding)
        {
            srd.word2.bits.WIDTH  = (actualExtent.width - 1);
            srd.word2.bits.HEIGHT = (actualExtent.height - 1);
        }
        else
        {
            srd.word2.bits.WIDTH  = (extent.width  - 1);
            srd.word2.bits.HEIGHT = (extent.height - 1);
        }

        // Setup CCC filtering optimizations: GCN uses a simple scheme which relies solely on the optimization
        // setting from the CCC rather than checking the render target resolution.
        static_assert(TextureFilterOptimizationsDisabled   == 0, "TextureOptLevel lookup table mismatch");
        static_assert(TextureFilterOptimizationsEnabled    == 1, "TextureOptLevel lookup table mismatch");
        static_assert(TextureFilterOptimizationsAggressive == 2, "TextureOptLevel lookup table mismatch");

        constexpr TexPerfModulation PanelToTexPerfMod[] =
        {
            TexPerfModulation::None,     // TextureFilterOptimizationsDisabled
            TexPerfModulation::Default,  // TextureFilterOptimizationsEnabled
            TexPerfModulation::Max       // TextureFilterOptimizationsAggressive
        };

        PAL_ASSERT(viewInfo.texOptLevel < ImageTexOptLevel::Count);

        uint32 texOptLevel;
        switch (viewInfo.texOptLevel)
        {
        case ImageTexOptLevel::Disabled:
            texOptLevel = TextureFilterOptimizationsDisabled;
            break;
        case ImageTexOptLevel::Enabled:
            texOptLevel = TextureFilterOptimizationsEnabled;
            break;
        case ImageTexOptLevel::Maximum:
            texOptLevel = TextureFilterOptimizationsAggressive;
            break;
        case ImageTexOptLevel::Default:
        default:
            texOptLevel = pGfxDevice->Parent()->Settings().textureOptLevel;
            break;
        }

        PAL_ASSERT(texOptLevel < ArrayLen(PanelToTexPerfMod));

        TexPerfModulation perfMod = PanelToTexPerfMod[texOptLevel];

        if (pGfxDevice->Settings().anisoFilterOptEnabled)
        {
            // If the Anisotropic Filter Optimization is enabled, force the texture perf modulation to maximum.
            perfMod = TexPerfModulation::Max;
        }

        srd.word2.bits.PERF_MOD = static_cast<uint32>(perfMod);

        // Destination swizzles come from the view creation info, rather than the format of the view.
        srd.word3.bits.DST_SEL_X    = Formats::Gfx6::HwSwizzle(viewInfo.swizzledFormat.swizzle.r);
        srd.word3.bits.DST_SEL_Y    = Formats::Gfx6::HwSwizzle(viewInfo.swizzledFormat.swizzle.g);
        srd.word3.bits.DST_SEL_Z    = Formats::Gfx6::HwSwizzle(viewInfo.swizzledFormat.swizzle.b);
        srd.word3.bits.DST_SEL_W    = Formats::Gfx6::HwSwizzle(viewInfo.swizzledFormat.swizzle.a);
        srd.word3.bits.TILING_INDEX = pTileInfo->tileIndex;

        const bool isMultiSampled = (imageCreateInfo.samples > 1);

        // NOTE: Where possible, we always assume an array view type because we don't know how the shader will
        // attempt to access the resource.
        switch (viewInfo.viewType)
        {
        case ImageViewType::Tex1d:
            srd.word3.bits.TYPE = SQ_RSRC_IMG_1D_ARRAY;
            break;
        case ImageViewType::Tex2d:
            srd.word3.bits.TYPE = (isMultiSampled) ? SQ_RSRC_IMG_2D_MSAA_ARRAY : SQ_RSRC_IMG_2D_ARRAY;
            break;
        case ImageViewType::Tex3d:
            srd.word3.bits.TYPE = SQ_RSRC_IMG_3D;
            break;
        case ImageViewType::TexCube:
            srd.word3.bits.TYPE = SQ_RSRC_IMG_CUBE;
            break;
        default:
            PAL_ASSERT_ALWAYS();
            break;
        }

        if (imageCreateInfo.mipLevels > 1)
        {
            // AddrLib should pow2-pad all mipmapped textures. Note that actual width and actual height will not be
            // powers of two for some view formats (e.g., R32G32B32) but the byte pitches should.
            PAL_ASSERT(IsPowerOfTwo(subresInfo.rowPitch));
            PAL_ASSERT(IsPowerOfTwo(subresInfo.depthPitch));

            srd.word3.bits.POW2_PAD = 1;
        }

        if (isMultiSampled)
        {
            // MSAA textures cannot be mipmapped; the BASE_LEVEL and LAST_LEVEL fields indicate the texture's
            // sample count.
            srd.word3.bits.BASE_LEVEL = 0;
            srd.word3.bits.LAST_LEVEL = Log2(imageCreateInfo.fragments);
        }
        else
        {
            srd.word3.bits.BASE_LEVEL = baseMipLevel;
            srd.word3.bits.LAST_LEVEL = (baseMipLevel + viewInfo.subresRange.numMips - 1);
        }

        srd.word4.bits.DEPTH = ComputeImageViewDepth(viewInfo, imageInfo, subresInfo);
        srd.word4.bits.PITCH = (actualExtent.width - 1);

        // Fill the unused 4 bits of word4 with sample pattern index
        srd.word4.bits.samplePatternIdx = viewInfo.samplePatternIdx;

        if ((viewInfo.flags.zRangeValid == 1) && (imageCreateInfo.imageType == ImageType::Tex3d))
        {
            srd.word5.bits.BASE_ARRAY = viewInfo.zRange.offset;
            srd.word5.bits.LAST_ARRAY = (viewInfo.zRange.offset + viewInfo.zRange.extent - 1);
        }
        else
        {
            srd.word5.bits.BASE_ARRAY = baseArraySlice;
            srd.word5.bits.LAST_ARRAY = (baseArraySlice + viewInfo.subresRange.numSlices - 1);
        }

        // Depth images obviously don't have an alpha component, so don't bother...
        if ((pParent->IsDepthStencilTarget() == false) && (subresInfo.flags.supportMetaDataTexFetch != 0))
        {
            // The setup of the compression-related fields requires knowing the bound memory and the expected
            // usage of the memory (read or write), so defer most of the setup to "WriteDescriptorSlot".

            // For the single component FORMAT cases, ALPHA_IS_ON_MSB (AIOM)=0 indicates the channel is color,
            // while ALPHA_IS_ON_MSB (AIOM)=1 indicates the channel is alpha.
            // ALPHA_IS_ON_MSB should be set to 1 for all single-component formats only if swap is SWAP_ALT_REV
            const SurfaceSwap surfSwap = Formats::Gfx6::ColorCompSwap(viewInfo.swizzledFormat);
            const uint32 numComponents = Formats::NumComponents(viewInfo.swizzledFormat.format);

            if  (((numComponents == 1) && (surfSwap == SWAP_ALT_REV)) ||
                 ((numComponents != 1) && (surfSwap != SWAP_STD_REV) && (surfSwap != SWAP_ALT_REV)))
            {
                srd.word6.bits.ALPHA_IS_ON_MSB__VI = 1;
            }
        }

        if (pParent->GetBoundGpuMemory().IsBound())
        {
            // Need to grab the most up-to-date GPU virtual address for the underlying image object.
            const gpusize gpuVirtAddress = pParent->GetSubresourceBaseAddr(subresource);

            const uint32 swizzle = pTileInfo->tileSwizzle;

            srd.word0.bits.BASE_ADDRESS    = Get256BAddrLoSwizzled(gpuVirtAddress, swizzle);
            srd.word1.bits.BASE_ADDRESS_HI = Get256BAddrHi(gpuVirtAddress);
            if (static_cast<const Pal::Device*>(pDevice)->MemoryProperties().flags.iommuv2Support)
            {
                srd.word3.bits.ATC__CI__VI = ((HighPart(gpuVirtAddress) >> 0x10) != 0)
                    ? 0 : ((LowPart(gpuVirtAddress) != 0) || ((HighPart(gpuVirtAddress) & 0xFFFF) != 0));
            }
            if (subresInfo.flags.supportMetaDataTexFetch)
            {
                // We decide whether meta data fetch should be enabled based on start mip in view range rather than
                // zero-th mip in imageViewSrd creation. If mip level in view range starts from non-zero-th mip,
                // meta data of zero-th mip might have not been intialized when perSubResInit=1. It is generally
                // safe but when mip interleave exists, child mips might be non-tc-compatible and just fetching
                // 'expanded' meta data value interleaved in zero-th mip. It's safe to enable meta data fetch based
                // on start mip, since start mip must be in valid shader read state thus meta data already intialized,
                // no matter startMip=0 or startMip>0. On the other hand, whether zero-th mip supports meta data fetch
                // is pre-condition of whether start mip supports meta data fetch.
                const uint32 settingsCheckFromStartMip = pGfxDevice->Settings().gfx8CheckMetaDataFetchFromStartMip;

                if (pParent->IsDepthStencilTarget())
                {
                    if ((TestAnyFlagSet(settingsCheckFromStartMip, Gfx8CheckMetaDataFetchFromStartMipDepthStencil) ||
                         (startSubresInfo.flags.supportMetaDataTexFetch == true)) &&
                        (TestAnyFlagSet(viewInfo.possibleLayouts.usages, LayoutShaderWrite) == false))
                    {
                        // Theoretically, the htile address here should have the tile-swizzle OR'd in, but in
                        // SetTileSwizzle, the tile swizzle for texture-fetchable depth images is always set to zero,
                        // so we should be all set with the base address.
                        PAL_ASSERT(swizzle == 0);
                        srd.word7.bits.META_DATA_ADDRESS__VI = image.GetHtile256BAddr(subresource);
                        srd.word6.bits.COMPRESSION_EN__VI    = 1;
                    }
                }
                else if ((TestAnyFlagSet(settingsCheckFromStartMip, Gfx8CheckMetaDataFetchFromStartMipColorTarget) ||
                          (startSubresInfo.flags.supportMetaDataTexFetch == true)) &&
                         (TestAnyFlagSet(viewInfo.possibleLayouts.usages, LayoutShaderWrite) == false))
                {
                    PAL_ASSERT(pParent->IsRenderTarget());
                    // The color image's meta-data always points at the DCC surface.  Any existing cMask or fMask
                    // meta-data is only required for compressed texture fetches of MSAA surfaces, and that feature
                    // requires enabling an extension and use of an fMask image view.
                    srd.word7.bits.META_DATA_ADDRESS__VI = image.GetDcc256BAddr(subresource);
                    srd.word6.bits.COMPRESSION_EN__VI    = 1;
                }
            } // end check for image supporting meta-data tex fetches
        }

        pSrds[i] = srd;
    }
}

// =====================================================================================================================
// Gfx6+ specific function for creating fmask view SRDs. Installed in the function pointer table of the parent device
// during initialization.
void PAL_STDCALL Device::CreateFmaskViewSrds(
    const IDevice*        pDevice,
    uint32                count,
    const FmaskViewInfo*  pFmaskViewInfo,
    void*                 pOut)
{
    PAL_ASSERT((pDevice != nullptr) && (pOut != nullptr) && (pFmaskViewInfo != nullptr) && (count > 0));
    const Device* pGfxDevice = static_cast<const Device*>(static_cast<const Pal::Device*>(pDevice)->GetGfxDevice());

    pGfxDevice->CreateFmaskViewSrds(count, pFmaskViewInfo, nullptr, pOut);
}

// =====================================================================================================================
// Creates 'count' fmask view SRDs. If internal info is not required pFmaskViewInternalInfo can be set to null,
// otherwise it must be an array of 'count' internal info structures.
void Device::CreateFmaskViewSrds(
    uint32                       count,
    const FmaskViewInfo*         pFmaskViewInfo,
    const FmaskViewInternalInfo* pFmaskViewInternalInfo,
    void*                        pOut
    ) const
{
    const bool hasInternalInfo = (pFmaskViewInternalInfo != nullptr);

    ImageSrd* pSrds = static_cast<ImageSrd*>(pOut);

    for (uint32 i = 0; i < count; ++i)
    {
        const auto&            viewInfo   = pFmaskViewInfo[i];
        const Image&           image      = *GetGfx6Image(viewInfo.pImage);
        const Pal::Image*const pParent    = image.Parent();
        const ImageCreateInfo& createInfo = pParent->GetImageCreateInfo();
        const bool             isUav      = (hasInternalInfo && (pFmaskViewInternalInfo[i].flags.fmaskAsUav == 1));

        ImageSrd srd = {};

        constexpr SubresId slice0Id = {};

        const SubResourceInfo*         pSubresInfo = image.Parent()->SubresourceInfo(slice0Id);
        const AddrMgr1::TileInfo*const pTileInfo   = AddrMgr1::GetTileInfo(image.Parent(), slice0Id);
        const Gfx6Fmask&               fmask       = *image.GetFmask(slice0Id);

        // For Fmask views, the format is based on the sample and fragment counts.
        srd.word1.bits.DATA_FORMAT = fmask.FmaskFormat(createInfo.samples, createInfo.fragments, isUav);
        srd.word1.bits.NUM_FORMAT  = IMG_NUM_FORMAT_UINT;
        srd.word1.bits.MIN_LOD     = 0;

        srd.word2.bits.WIDTH    = (pSubresInfo->extentTexels.width - 1);
        srd.word2.bits.HEIGHT   = (pSubresInfo->extentTexels.height - 1);
        srd.word2.bits.PERF_MOD = 0;

        // For Fmask views, destination swizzles are based on the bit depth of the Fmask buffer.
        srd.word3.bits.DST_SEL_X    = SQ_SEL_X;
        srd.word3.bits.DST_SEL_Y    = (fmask.BitsPerPixel() == 64) ? SQ_SEL_Y : SQ_SEL_0;
        srd.word3.bits.DST_SEL_Z    = SQ_SEL_0;
        srd.word3.bits.DST_SEL_W    = SQ_SEL_0;
        srd.word3.bits.TILING_INDEX = fmask.TileIndex();
        srd.word3.bits.TYPE         = SQ_RSRC_IMG_2D_ARRAY;
        srd.word3.bits.BASE_LEVEL   = 0;
        srd.word3.bits.LAST_LEVEL   = 0;

        srd.word4.bits.DEPTH = (viewInfo.baseArraySlice + viewInfo.arraySize - 1);
        srd.word4.bits.PITCH = (pSubresInfo->actualExtentTexels.width - 1);

        srd.word5.bits.BASE_ARRAY = viewInfo.baseArraySlice;
        srd.word5.bits.LAST_ARRAY = (viewInfo.baseArraySlice + viewInfo.arraySize - 1);

        if (image.Parent()->GetBoundGpuMemory().IsBound())
        {
            // Need to grab the most up-to-date GPU virtual address for the underlying FMask object.
            const gpusize gpuVirtAddress = image.GetFmaskBaseAddr(slice0Id);
            const uint32 swizzle         = pTileInfo->tileSwizzle;

            srd.word0.bits.BASE_ADDRESS    = Get256BAddrLoSwizzled(gpuVirtAddress, swizzle);
            srd.word1.bits.BASE_ADDRESS_HI = Get256BAddrHi(gpuVirtAddress);

            // Does this image has an associated FMask which is shader Readable? if FMask needs to be
            // read in the shader CMask has to be read as FMask meta data
            if (image.IsComprFmaskShaderReadable(pSubresInfo))
            {
                srd.word6.bits.COMPRESSION_EN__VI = (viewInfo.flags.shaderWritable == 0);

                if (viewInfo.flags.shaderWritable == 0)
                {
                    srd.word7.bits.META_DATA_ADDRESS__VI = image.GetCmask256BAddr(slice0Id);
                }
            }
        }

        pSrds[i] = srd;
    }
}

// =====================================================================================================================
// Gfx6+ specific function for creating sampler SRDs. Installed in the function pointer table of the parent device
// during initialization.
void PAL_STDCALL Device::CreateSamplerSrds(
    const IDevice*     pDevice,
    uint32             count,
    const SamplerInfo* pSamplerInfo,
    void*              pOut)
{
    PAL_ASSERT((pDevice != nullptr) && (pOut != nullptr) && (pSamplerInfo != nullptr) && (count > 0));
    const Device* pGfxDevice = static_cast<const Device*>(static_cast<const Pal::Device*>(pDevice)->GetGfxDevice());

    const Gfx6PalSettings& settings       = GetGfx6Settings(*(pGfxDevice->Parent()));
    constexpr uint32       SamplerSrdSize = sizeof(SamplerSrd);

    constexpr uint32 NumTemporarySamplerSrds = 32;
    SamplerSrd tempSamplerSrds[NumTemporarySamplerSrds] = {};
    uint32     srdsBuilt = 0;

    while (srdsBuilt < count)
    {
        void* pSrdOutput = VoidPtrInc(pOut, (srdsBuilt * SamplerSrdSize));
        memset(&tempSamplerSrds[0], 0, sizeof(tempSamplerSrds));

        uint32 currentSrdIdx = 0;
        for (currentSrdIdx = 0;
             ((currentSrdIdx < NumTemporarySamplerSrds) && (srdsBuilt < count));
             currentSrdIdx++, srdsBuilt++)
        {
            const SamplerInfo* pInfo = &pSamplerInfo[srdsBuilt];
            SamplerSrd*        pSrd  = &tempSamplerSrds[currentSrdIdx];

            const SQ_TEX_ANISO_RATIO maxAnisoRatio = GetAnisoRatio(pInfo);

            pSrd->word0.bits.CLAMP_X            = GetAddressClamp(pInfo->addressU);
            pSrd->word0.bits.CLAMP_Y            = GetAddressClamp(pInfo->addressV);
            pSrd->word0.bits.CLAMP_Z            = GetAddressClamp(pInfo->addressW);
            pSrd->word0.bits.MAX_ANISO_RATIO    = maxAnisoRatio;
            pSrd->word0.bits.DEPTH_COMPARE_FUNC = static_cast<uint32>(pInfo->compareFunc);
            pSrd->word0.bits.FORCE_UNNORMALIZED = pInfo->flags.unnormalizedCoords;
            pSrd->word0.bits.TRUNC_COORD        = pInfo->flags.truncateCoords;
            pSrd->word0.bits.DISABLE_CUBE_WRAP  = (pInfo->flags.seamlessCubeMapFiltering == 1) ? 0 : 1;
            constexpr uint32 Gfx6SamplerLodMinMaxIntBits  = 4;
            constexpr uint32 Gfx6SamplerLodMinMaxFracBits = 8;
            pSrd->word1.bits.MIN_LOD = Math::FloatToUFixed(pInfo->minLod,
                                                           Gfx6SamplerLodMinMaxIntBits,
                                                           Gfx6SamplerLodMinMaxFracBits);
            pSrd->word1.bits.MAX_LOD = Math::FloatToUFixed(pInfo->maxLod,
                                                           Gfx6SamplerLodMinMaxIntBits,
                                                           Gfx6SamplerLodMinMaxFracBits);

            constexpr uint32 Gfx6SamplerLodBiasIntBits  = 6;
            constexpr uint32 Gfx6SamplerLodBiasFracBits = 8;

            // Setup XY and Mip filters.  Encoding of the API enumerations is:  xxyyzzww, where:
            //     ww : mag filter bits
            //     zz : min filter bits
            //     yy : z filter bits
            //     xx : mip filter bits
            pSrd->word2.bits.XY_MAG_FILTER = static_cast<uint32>(pInfo->filter.magnification);
            pSrd->word2.bits.XY_MIN_FILTER = static_cast<uint32>(pInfo->filter.minification);
            pSrd->word2.bits.Z_FILTER      = static_cast<uint32>(pInfo->filter.zFilter);
            pSrd->word2.bits.MIP_FILTER    = static_cast<uint32>(pInfo->filter.mipFilter);
            pSrd->word2.bits.LOD_BIAS      = Math::FloatToSFixed(pInfo->mipLodBias,
                                                                 Gfx6SamplerLodBiasIntBits,
                                                                 Gfx6SamplerLodBiasFracBits);

            pSrd->word2.bits.MIP_POINT_PRECLAMP = 0;
            pSrd->word2.bits.DISABLE_LSB_CEIL   = (settings.samplerCeilingLogicEnabled == false);
            pSrd->word2.bits.FILTER_PREC_FIX    = settings.samplerPrecisionFixEnabled;

            // Ensure useAnisoThreshold is only set when preciseAniso is disabled
            PAL_ASSERT((pInfo->flags.preciseAniso == 0) ||
                       ((pInfo->flags.preciseAniso == 1) && (pInfo->flags.useAnisoThreshold == 0)));

            if (pInfo->flags.preciseAniso == 0)
            {
                // Setup filtering optimization levels: these will be modulated by the global filter
                // optimization aggressiveness, which is controlled by the "TFQ" public setting.
                // NOTE: Aggressiveness of optimizations is influenced by the max anisotropy level.
                constexpr uint32 Gfx6PerfMipOffset = 6;

                if (settings.samplerPerfMip)
                {
                    pSrd->word1.bits.PERF_MIP = settings.samplerPerfMip;
                }
                else if (pInfo->perfMip)
                {
                    pSrd->word1.bits.PERF_MIP = pInfo->perfMip;
                }
                else
                {
                    pSrd->word1.bits.PERF_MIP = (maxAnisoRatio + Gfx6PerfMipOffset);
                }

                constexpr uint32 Gfx6NumAnisoThresholdValues = 8;

                if (pInfo->flags.useAnisoThreshold == 1)
                {
                    // ANISO_THRESHOLD is a 3 bit number representing adjustments of 0/8 through 7/8
                    // so we quantize and clamp anisoThreshold into that range here.
                    pSrd->word0.bits.ANISO_THRESHOLD = Util::Clamp(static_cast<uint32>(
                        static_cast<float>(Gfx6NumAnisoThresholdValues) * pInfo->anisoThreshold),
                        0U, Gfx6NumAnisoThresholdValues - 1U);
                }
                else
                {
                    //  The code below does the following calculation.
                    //  if maxAnisotropy < 4   ANISO_THRESHOLD = 0 (0.0 adjust)
                    //  if maxAnisotropy < 16  ANISO_THRESHOLD = 1 (0.125 adjust)
                    //  if maxAnisotropy == 16 ANISO_THRESHOLD = 2 (0.25 adjust)
                    constexpr uint32 Gfx6AnisoRatioShift = 1;
                    pSrd->word0.bits.ANISO_THRESHOLD = (settings.samplerAnisoThreshold == 0)
                        ? (maxAnisoRatio >> Gfx6AnisoRatioShift) : settings.samplerAnisoThreshold;
                }

                pSrd->word0.bits.ANISO_BIAS = (settings.samplerAnisoBias == 0) ? maxAnisoRatio :
                                                                                 settings.samplerAnisoBias;
                pSrd->word2.bits.LOD_BIAS_SEC = settings.samplerSecAnisoBias;
            }

            // First version that supported this interface was 65
            constexpr SQ_IMG_FILTER_TYPE  HwFilterMode[]=
            {
                SQ_IMG_FILTER_MODE_BLEND, // TexFilterMode::Blend
                SQ_IMG_FILTER_MODE_MIN,   // TexFilterMode::Min
                SQ_IMG_FILTER_MODE_MAX,   // TexFilterMode::Max
            };

            PAL_ASSERT (static_cast<uint32>(pInfo->filterMode) < (sizeof(HwFilterMode) / sizeof(SQ_IMG_FILTER_TYPE)));
            pSrd->word0.bitfields.FILTER_MODE = HwFilterMode[static_cast<uint32>(pInfo->filterMode)];

            // The BORDER_COLOR_PTR field is only used by the HW for the SQ_TEX_BORDER_COLOR_REGISTER case
            pSrd->word3.bits.BORDER_COLOR_PTR  = 0;

            // And setup the HW-supported border colors appropriately
            switch (pInfo->borderColorType)
            {
            case BorderColorType::White:
                pSrd->word3.bits.BORDER_COLOR_TYPE = SQ_TEX_BORDER_COLOR_OPAQUE_WHITE;
                break;
            case BorderColorType::TransparentBlack:
                pSrd->word3.bits.BORDER_COLOR_TYPE = SQ_TEX_BORDER_COLOR_TRANS_BLACK;
                break;
            case BorderColorType::OpaqueBlack:
                pSrd->word3.bits.BORDER_COLOR_TYPE = SQ_TEX_BORDER_COLOR_OPAQUE_BLACK;
                break;
            case BorderColorType::PaletteIndex:
                pSrd->word3.bits.BORDER_COLOR_TYPE = SQ_TEX_BORDER_COLOR_REGISTER;
                pSrd->word3.bits.BORDER_COLOR_PTR  = pInfo->borderColorPaletteIndex;
                break;
            default:
                PAL_ALERT_ALWAYS();
                break;
            }

            // NOTE: The hardware fundamentally does not support multiple border color palettes for compute as the
            //       register which controls the address of the palette is a config register. We need to support this
            //       for our clients, but it should not be considered a correct implementation. As a result we may see
            //       arbitrary hangs that do not reproduce easily. In the event that this setting
            //       (disableBorderColorPaletteBinds) should be set to TRUE, we need to make sure that any samplers
            //       created do not reference a border color palette and instead just select transparent black.
            if (settings.disableBorderColorPaletteBinds)
            {
                pSrd->word3.bits.BORDER_COLOR_TYPE = SQ_TEX_BORDER_COLOR_TRANS_BLACK;
                pSrd->word3.bits.BORDER_COLOR_PTR  = 0;
            }

            if (pGfxDevice->Parent()->ChipProperties().gfxLevel >= GfxIpLevel::GfxIp8)
            {
                // The new VI COMPAT_MODE feature is an enhancement for anisotropic texture filtering, which should be
                // disabled if we need to match image quality between ASICs in an MGPU configuration.
                pSrd->word0.bits.COMPAT_MODE__VI = (pInfo->flags.mgpuIqMatch == 0);

                // This allows the sampler to override anisotropic filtering when the resource view contains a single
                // mipmap level. On SI/CI hardware, SC had to add extra shader instructions to accomplish the same
                // functionality.
                pSrd->word2.bits.ANISO_OVERRIDE__VI = !pInfo->flags.disableSingleMipAnisoOverride;
            }
        }

        memcpy(pSrdOutput, &tempSamplerSrds[0], (currentSrdIdx * sizeof(SamplerSrd)));
    }
}

// =====================================================================================================================
// Gfx6+ specific function for creating BVH SRDs. Installed in the function pointer table of the parent device
// during initialization.
void PAL_STDCALL Device::CreateBvhSrds(
    const IDevice*  pDevice,
    uint32          count,
    const BvhInfo*  pBvhInfo,
    void*           pOut)
{
    // Ray trace isn't supported until GFX10; the client should never be trying this.  Function provided only to
    // prevent null-pointer calls (and crashes).
    PAL_NEVER_CALLED();
}

// The minimum microcode versions for all supported GFX 6-8 GPUs. These constants are expressed in decimal rather than
// hexidecimal because the compiled microcode headers use decimal constants. Note that these values were taken from the
// 15.20 driver which added support for command buffer chaining on the constant engine.
constexpr uint32 MinMicrocodeVersionSi          = 25;
constexpr uint32 MinMicrocodeVersionCi          = 25;
constexpr uint32 MinMicrocodeVersionKv          = 25;
constexpr uint32 MinMicrocodeVersionViPolarisCz = 29;

// =====================================================================================================================
// Determines the GFXIP level of a GPU supported by the GFX6 hardware layer. The return value will be GfxIpLevel::None
// if the GPU is unsupported by this HWL.
// PAL relies on a specific set of functionality from the CP microcode, so the GPU is only supported if the microcode
// version is new enough (this varies by hardware family).
//
GfxIpLevel DetermineIpLevel(
    uint32 familyId, // Hardware Family ID.
    uint32 eRevId,   // Software Revision ID.
    uint32 microcodeVersion)
{
    GfxIpLevel level = GfxIpLevel::None;

    if (FAMILY_IS_POLARIS(familyId))
    {
        level = GfxIpLevel::GfxIp8;
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    return level;
}

// =====================================================================================================================
// Gets the static format support info table for GFXIP 6/7/8 hardware.
const MergedFormatPropertiesTable* GetFormatPropertiesTable(
    GfxIpLevel gfxIpLevel)
{
    const MergedFormatPropertiesTable* pTable = nullptr;

    switch (gfxIpLevel)
    {
    case GfxIpLevel::GfxIp6:
        pTable = &Gfx6MergedFormatPropertiesTable;
        break;
    case GfxIpLevel::GfxIp7:
        pTable = &Gfx7MergedFormatPropertiesTable;
        break;
    case GfxIpLevel::GfxIp8:
        pTable = &Gfx8MergedFormatPropertiesTable;
        break;
    case GfxIpLevel::GfxIp8_1:
        pTable = &Gfx8_1MergedFormatPropertiesTable;
        break;
    default:
        // What is this?
        PAL_ASSERT_ALWAYS();
    }

    return pTable;
}

// =====================================================================================================================
// Initializes the GPU chip properties for a Device object, specifically for the GFX6 hardware layer. Returns an error
// if an unsupported chip revision is detected.
void InitializeGpuChipProperties(
    uint32             cpUcodeVersion,
    GpuChipProperties* pInfo)
{
    pInfo->imageProperties.flags.u32All = 0;

    // All current GFXIP 6+ harware has the same max image dimensions.
    pInfo->imageProperties.maxImageDimension.width  = MaxImageWidth;
    pInfo->imageProperties.maxImageDimension.height = MaxImageHeight;
    pInfo->imageProperties.maxImageDimension.depth  = MaxImageDepth;
    pInfo->imageProperties.maxImageArraySize        = MaxImageArraySlices;
    pInfo->imageProperties.prtTileSize              = PrtTileSize;
    pInfo->imageProperties.msaaSupport              = MsaaAll;
    pInfo->imageProperties.maxMsaaFragments         = 8;

    // GFX6 ASICs support creating AQBS stereo images.
    pInfo->imageProperties.flags.supportsAqbsStereoMode = 1;

    // GFXIP 6+ hardware doesn't support standard swizzle tiling.
    pInfo->imageProperties.tilingSupported[static_cast<uint32>(ImageTiling::Linear)]       = true;
    pInfo->imageProperties.tilingSupported[static_cast<uint32>(ImageTiling::Optimal)]      = true;
    pInfo->imageProperties.tilingSupported[static_cast<uint32>(ImageTiling::Standard64Kb)] = false;

    pInfo->gfx6.numScPerSe              = 1;
    pInfo->gfx6.numPackerPerSc          = 2;

    // NOTE: GFXIP 6+ hardware has the same wavefront size, VGPR count, TCA block count, SRD sizes and number of
    // user-data entries.
    pInfo->gfxip.hardwareContexts       = 8;
    pInfo->gfx6.numSimdPerCu            = NumSimdPerCu;
    pInfo->gfx6.numWavesPerSimd         = NumWavesPerSimd;
    pInfo->gfx6.nativeWavefrontSize     = 64;
    pInfo->gfx6.numShaderVisibleSgprs   = MaxSgprsAvailable;
    pInfo->gfx6.numPhysicalVgprsPerSimd = 256;
    pInfo->gfx6.numPhysicalVgprs        = 256;
    pInfo->gfx6.vgprAllocGranularity    = 4;
    pInfo->gfx6.minVgprAlloc            = 4;
    pInfo->gfx6.numTcaBlocks            = 2;

    pInfo->gfxip.maxUserDataEntries = MaxUserDataEntries;

    pInfo->gfxip.maxGsOutputVert            = 1023;
    pInfo->gfxip.maxGsTotalOutputComponents = 4095;
    pInfo->gfxip.maxGsInvocations           = 127;

    // Max supported by HW is 2^32-1 for all counters.  However limit Y and Z to keep total threads < 2^64 to avoid
    // potentially overflowing 64 bit counters in HW
    pInfo->gfxip.maxComputeThreadGroupCountX = UINT32_MAX;
    pInfo->gfxip.maxComputeThreadGroupCountY = UINT16_MAX;
    pInfo->gfxip.maxComputeThreadGroupCountZ = UINT16_MAX;

    // The maximum amount of LDS space that can be shared by a group of threads (wave/ threadgroup) in bytes.
    pInfo->gfxip.ldsSizePerCu = 65536;
    if (pInfo->gfxLevel == GfxIpLevel::GfxIp6)
    {
        pInfo->gfxip.ldsSizePerThreadGroup = 32_KiB;
        pInfo->gfxip.ldsGranularity        = Gfx6LdsDwGranularity * sizeof(uint32);
    }
    else
    {
        pInfo->gfxip.ldsSizePerThreadGroup = 64_KiB;
        pInfo->gfxip.ldsGranularity        = Gfx7LdsDwGranularity * sizeof(uint32);
    }

    // GFXIP 6-8 do not support MALL
    pInfo->gfxip.mallSizeInBytes  = 0_MiB;

    // Overwritten if the device supports the following
    pInfo->gfxip.gl1cSizePerSa         =  0_KiB;
    pInfo->gfxip.instCacheSizePerCu    =  0_KiB;
    pInfo->gfxip.scalarCacheSizePerCu  =  0_KiB;

    // All GFXIP 6-8 hardware share the same SRD sizes.
    pInfo->srdSizes.bufferView = sizeof(BufferSrd);
    pInfo->srdSizes.imageView  = sizeof(ImageSrd);
    pInfo->srdSizes.fmaskView  = sizeof(ImageSrd);
    pInfo->srdSizes.sampler    = sizeof(SamplerSrd);

    pInfo->nullSrds.pNullBufferView = &NullBufferView;
    pInfo->nullSrds.pNullImageView  = &NullImageView;
    pInfo->nullSrds.pNullFmaskView  = &NullImageView;
    pInfo->nullSrds.pNullSampler    = &NullSampler;

    // All GFXIP 6-8 hardware cannot support 2-bit signed values.
    pInfo->gfx6.supports2BitSignedValues          = 0;
    pInfo->gfx6.support64BitInstructions          = 1;
    pInfo->gfxip.supportFloat32BufferAtomics      = 1;
    pInfo->gfxip.supportFloat32ImageAtomics       = 1;
    pInfo->gfxip.supportFloat32BufferAtomicAdd    = 0;
    pInfo->gfxip.supportFloat32ImageAtomicAdd     = 0;
    pInfo->gfxip.supportFloat32ImageAtomicMinMax  = 0;
    pInfo->gfx6.supportFloat64Atomics             = 1;
    pInfo->gfx6.supportBorderColorSwizzle         = 0;
    pInfo->gfx6.supportImageViewMinLod            = 1;
    pInfo->gfxip.supportFloat64BufferAtomicMinMax = 1;
    pInfo->gfxip.supportFloat64SharedAtomicMinMax = 1;

    // Out of order primitives was added in Hawaii, but it has a hardware bug where the hardware can hang when a
    // multi-cycle primitive is processed when out of order is enabled. This is fixed for GFXIP 8.
    if (pInfo->gfxLevel >= GfxIpLevel::GfxIp8)
    {
        pInfo->gfx6.supportOutOfOrderPrimitives = 1;
    }

    // All hardware can support subgroup shader clock
    // GFX8+ can only support the device clock
    pInfo->gfx6.supportShaderSubgroupClock = 1;
    pInfo->gfx6.supportShaderDeviceClock   = (pInfo->gfxLevel >= GfxIpLevel::GfxIp8) ? 1 : 0;

    pInfo->gfxip.supportsHwVs = 1;

    switch (pInfo->familyId)
    {
    // GFX 6 Discrete GPU's (Southern Islands):
    case FAMILY_SI:
        pInfo->gpuType = GpuType::Discrete;

        pInfo->gfx6.gsVgtTableDepth         = 32;
        pInfo->gfx6.gsPrimBufferDepth       = 1792;
        pInfo->gfx6.maxGsWavesPerVgt        = 32;
        pInfo->gfx6.doubleOffchipLdsBuffers = 0;
        pInfo->gfx6.support8bitIndices      = 0;
        pInfo->gfx6.numPhysicalSgprs        = 512;
        pInfo->gfx6.sgprAllocGranularity    = 8;
        pInfo->gfx6.minSgprAlloc            = 8;
        pInfo->gfx6.supportRgpTraces        = 0;

        pInfo->gfxip.vaRangeNumBits = 40;

        pInfo->imageProperties.prtFeatures = Gfx6PrtFeatures;
        pInfo->gfxip.tcpSizeInBytes        = 16384;

        if (ASICREV_IS_TAHITI_P(pInfo->eRevId))
        {
            pInfo->gfx6.numShaderEngines  = 2;
            pInfo->gfx6.numShaderArrays   = 2;
            pInfo->gfx6.maxNumCuPerSh     = 8;
            pInfo->gfx6.maxNumRbPerSe     = 4;
            pInfo->gfx6.numMcdTiles       = 6;
            pInfo->gfx6.numTccBlocks      = 12;
            pInfo->revision               = AsicRevision::Tahiti;
            pInfo->gfxStepping            = Abi::GfxIpSteppingTahiti;
            pInfo->gfxip.tccSizeInBytes   = 768_KiB;
        }
        else if (ASICREV_IS_PITCAIRN_PM(pInfo->eRevId))
        {
            pInfo->gfx6.numShaderEngines  = 2;
            pInfo->gfx6.numShaderArrays   = 2;
            pInfo->gfx6.maxNumCuPerSh     = 5;
            pInfo->gfx6.maxNumRbPerSe     = 4;
            pInfo->gfx6.numMcdTiles       = 4;
            pInfo->gfx6.numTccBlocks      = 8;
            pInfo->revision               = AsicRevision::Pitcairn;
            pInfo->gfxStepping            = Abi::GfxIpSteppingPitcairn;
            pInfo->gfxip.tccSizeInBytes   = 512_KiB;
        }
        else if (ASICREV_IS_CAPEVERDE_M(pInfo->eRevId))
        {
            pInfo->gfx6.numShaderEngines  = 1;
            pInfo->gfx6.numShaderArrays   = 2;
            pInfo->gfx6.maxNumCuPerSh     = 5;
            pInfo->gfx6.maxNumRbPerSe     = 4;
            pInfo->gfx6.numMcdTiles       = 2;
            pInfo->gfx6.numTccBlocks      = 4;
            pInfo->revision               = AsicRevision::Capeverde;
            pInfo->gfxStepping            = Abi::GfxIpSteppingCapeVerde;
            pInfo->gfxip.tccSizeInBytes   = 512_KiB;
        }
        else if (ASICREV_IS_OLAND_M(pInfo->eRevId))
        {
            pInfo->gfx6.numShaderEngines  = 1;
            pInfo->gfx6.numShaderArrays   = 1;
            pInfo->gfx6.maxNumCuPerSh     = 6;
            pInfo->gfx6.maxNumRbPerSe     = 2;
            // NOTE: Oland comes in both 64b and 128b variants, so we cannot accurately know the number of MCD
            // tiles here. Caller should override based on what the KMD reports.
            pInfo->gfx6.numMcdTiles       = 0;
            pInfo->gfx6.numTccBlocks      = 4;
            pInfo->gfx6.gsVgtTableDepth   = 16;
            pInfo->gfx6.gsPrimBufferDepth = 768;
            pInfo->gfx6.maxGsWavesPerVgt  = 16;
            pInfo->revision               = AsicRevision::Oland;
            pInfo->gfxStepping            = Abi::GfxIpSteppingOland;
            pInfo->gfxip.tccSizeInBytes   = 256_KiB;
        }
        else if (ASICREV_IS_HAINAN_V(pInfo->eRevId))
        {
            pInfo->gfx6.numShaderEngines  = 1;
            pInfo->gfx6.numShaderArrays   = 1;
            pInfo->gfx6.maxNumCuPerSh     = 5;
            pInfo->gfx6.maxNumRbPerSe     = 1;
            pInfo->gfx6.numMcdTiles       = 1;
            pInfo->gfx6.numTccBlocks      = 2;
            pInfo->gfx6.gsVgtTableDepth   = 16;
            pInfo->gfx6.gsPrimBufferDepth = 768;
            pInfo->gfx6.maxGsWavesPerVgt  = 16;
            pInfo->revision               = AsicRevision::Hainan;
            pInfo->gfxStepping            = Abi::GfxIpSteppingHainan;
            pInfo->gfxip.tccSizeInBytes   = 256_KiB;
        }
        break;
    // GFXIP 7 Discrete GPU's (Sea Islands):
    case FAMILY_CI:
        pInfo->gpuType = GpuType::Discrete;

        pInfo->gfx6.numShaderArrays         = 1;
        pInfo->gfx6.gsVgtTableDepth         = 32;
        pInfo->gfx6.gsPrimBufferDepth       = 1792;
        pInfo->gfx6.maxGsWavesPerVgt        = 32;
        pInfo->gfx6.doubleOffchipLdsBuffers = 1;
        pInfo->gfx6.support8bitIndices      = 0;
        pInfo->gfx6.numPhysicalSgprs        = 512;
        pInfo->gfx6.sgprAllocGranularity    = 8;
        pInfo->gfx6.minSgprAlloc            = 8;
        pInfo->gfx6.supportRgpTraces        = 0;

        // Support for IT_INDEX_ATTRIB_INDIRECT pkt has been enabled from microcode feature version 28 onwards for Gfx7.
        pInfo->gfx6.supportIndexAttribIndirectPkt = (cpUcodeVersion >= 28);

        pInfo->gfxip.vaRangeNumBits = 40;

        pInfo->imageProperties.prtFeatures = Gfx7PrtFeatures;
        pInfo->gfxip.tcpSizeInBytes        = 16384;

        if (ASICREV_IS_BONAIRE_M(pInfo->eRevId))
        {
            pInfo->gfx6.numShaderEngines = 2;
            pInfo->gfx6.maxNumCuPerSh    = 7;
            pInfo->gfx6.maxNumRbPerSe    = 2;
            pInfo->gfx6.numMcdTiles      = 2;
            pInfo->gfx6.numTccBlocks     = 4;
            pInfo->revision              = AsicRevision::Bonaire;
            pInfo->gfxStepping           = Abi::GfxIpSteppingBonaire;
            pInfo->gfxip.tccSizeInBytes  = 512_KiB;
        }
        else if (ASICREV_IS_HAWAII_P(pInfo->eRevId))
        {
            pInfo->gfx6.numShaderEngines = 4;
            pInfo->gfx6.maxNumCuPerSh    = 11;
            pInfo->gfx6.maxNumRbPerSe    = 4;
            pInfo->gfx6.numMcdTiles      = 8;
            pInfo->gfx6.numTccBlocks     = 16;
            if ((pInfo->deviceId == DEVICE_ID_CI_HAWAII_P_67A0) || (pInfo->deviceId == DEVICE_ID_CI_HAWAII_P_67A1))
            {
                pInfo->revision          = AsicRevision::HawaiiPro;
                pInfo->gfxStepping       = Abi::GfxIpSteppingHawaiiPro;
            }
            else
            {
                pInfo->revision          = AsicRevision::Hawaii;
                pInfo->gfxStepping       = Abi::GfxIpSteppingHawaii;
            }
            pInfo->gfxip.tccSizeInBytes  = 1_MiB;

            // Support for IT_SET_SH_REG_INDEX added from CP feature version 29 onwards.
            pInfo->gfx6.supportSetShIndexPkt = (cpUcodeVersion >= 29);
        }
        break;
    // GFXIP 7 Kaveri APU's:
    case FAMILY_KV:
        pInfo->gpuType = GpuType::Integrated;

        pInfo->gfx6.numShaderEngines        = 1;
        pInfo->gfx6.numShaderArrays         = 1;
        pInfo->gfx6.gsVgtTableDepth         = 16;
        pInfo->gfx6.maxGsWavesPerVgt        = 16;
        pInfo->gfx6.doubleOffchipLdsBuffers = 1;
        pInfo->gfx6.support8bitIndices      = 0;
        pInfo->gfx6.numPhysicalSgprs        = 512;
        pInfo->gfx6.sgprAllocGranularity    = 8;
        pInfo->gfx6.minSgprAlloc            = 8;
        pInfo->gfx6.supportRgpTraces        = 0;

        // Support for IT_INDEX_ATTRIB_INDIRECT pkt has been enabled from microcode feature version 28 onwards for Gfx7.
        pInfo->gfx6.supportIndexAttribIndirectPkt = (cpUcodeVersion >= 28);

        pInfo->imageProperties.prtFeatures = Gfx7PrtFeatures;
        pInfo->gfxip.tcpSizeInBytes        = 16384;

        pInfo->gfxip.supportCaptureReplay  = 0;

        if (ASICREV_IS_KALINDI(pInfo->eRevId) || ASICREV_IS_KALINDI_GODAVARI(pInfo->eRevId))
        {
            pInfo->gfx6.maxNumCuPerSh     = 2;
            pInfo->gfx6.maxNumRbPerSe     = 1;
            pInfo->gfx6.numMcdTiles       = 1;
            pInfo->gfx6.numTccBlocks      = 2;
            pInfo->gfx6.gsPrimBufferDepth = 256;

            pInfo->gfxip.vaRangeNumBits = 40;

            pInfo->gfxip.tccSizeInBytes = 128_KiB;

            pInfo->requiresOnionAccess = true;

            pInfo->revision             = (ASICREV_IS_KALINDI_GODAVARI(pInfo->eRevId)
                                           ? AsicRevision::Godavari
                                           : AsicRevision::Kalindi);
            pInfo->gfxStepping          = (ASICREV_IS_KALINDI_GODAVARI(pInfo->eRevId)
                                           ? Abi::GfxIpSteppingGodavari
                                           : Abi::GfxIpSteppingKalindi);
        }
        else if (ASICREV_IS_SPECTRE(pInfo->eRevId) || ASICREV_IS_SPOOKY(pInfo->eRevId))
        {
            pInfo->gfx6.maxNumCuPerSh     = 8;
            pInfo->gfx6.maxNumRbPerSe     = 2;
            pInfo->gfx6.numMcdTiles       = 2;
            pInfo->gfx6.numTccBlocks      = 4;
            pInfo->gfx6.gsPrimBufferDepth = 768;

            pInfo->gfxip.vaRangeNumBits = 48;

            pInfo->gfxip.tccSizeInBytes = 512_KiB;

            pInfo->revision             = (ASICREV_IS_SPECTRE(pInfo->eRevId)
                                           ? AsicRevision::Spectre
                                           : AsicRevision::Spooky);
            pInfo->gfxStepping          = Abi::GfxIpSteppingKaveri;
        }
        break;
    // GFXIP 8 Discrete GPU's (Volcanic Islands):
    case FAMILY_VI:
        pInfo->gpuType = GpuType::Discrete;

        pInfo->gfx6.numShaderArrays                = 1;
        pInfo->gfx6.gsVgtTableDepth                = 32;
        pInfo->gfx6.gsPrimBufferDepth              = 1792;
        pInfo->gfx6.maxGsWavesPerVgt               = 32;
        pInfo->gfx6.doubleOffchipLdsBuffers        = 1;
        pInfo->gfx6.support8bitIndices             = 1;
        pInfo->gfx6.support16BitInstructions       = 1;
        pInfo->gfx6.numPhysicalSgprs               = 800;
        pInfo->gfx6.sgprAllocGranularity           = 16;
        pInfo->gfx6.minSgprAlloc                   = 16;
        pInfo->gfx6.supportRgpTraces               = 1;

        // Support for IT_SET_SH_REG_INDEX and IT_INDEX_ATTRIB_INDIRECT pkts has been enabled from microcode feature
        // version 36 onwards for Gfx8.
        pInfo->gfx6.supportSetShIndexPkt          = (cpUcodeVersion >= 36);
        pInfo->gfx6.supportIndexAttribIndirectPkt = (cpUcodeVersion >= 36);

        // Support for IT_LOAD_CONTEXT/SH_REG_INDEX has been enabled from microcode feature version 41 onwards for Gfx8.
        pInfo->gfx6.supportLoadRegIndexPkt = (cpUcodeVersion >= 41);

        // Support for IT_DUMP_CONST_RAM_OFFSET and IT_SET_SH_REF_OFFSET indexed mode has been enabled from microcode
        // feature version 45 onwards for gfx-8
        pInfo->gfx6.supportAddrOffsetDumpAndSetShPkt = (cpUcodeVersion >= 45);

        // Support for preemption within chained indirect buffers has been fixed starting with microcode feature version
        // 46 and onwards.
        pInfo->gfx6.supportPreemptionWithChaining = (cpUcodeVersion >= 46);

        pInfo->gfxip.vaRangeNumBits = 40;
        pInfo->gfxip.tcpSizeInBytes = 16384;

        pInfo->gfxip.instCacheSizePerCu   = 32_KiB;
        pInfo->gfxip.scalarCacheSizePerCu = 16_KiB;

        pInfo->imageProperties.prtFeatures = Gfx8PrtFeatures;

        pInfo->gfx6.supportPatchTessDistribution = 1;
        pInfo->gfx6.supportDonutTessDistribution = 1;

        if (ASICREV_IS_ICELAND_M(pInfo->eRevId))
        {
            pInfo->gfx6.numShaderEngines      = 1;
            pInfo->gfx6.numWavesPerSimd       = 8;
            pInfo->gfx6.maxNumCuPerSh         = 6;
            pInfo->gfx6.maxNumRbPerSe         = 2;
            pInfo->gfx6.numMcdTiles           = 1;
            pInfo->gfx6.numTccBlocks          = 2;
            pInfo->gfx6.gsVgtTableDepth       = 16;
            pInfo->gfx6.gsPrimBufferDepth     = 768;
            pInfo->gfx6.maxGsWavesPerVgt      = 16;
            pInfo->gfx6.numShaderVisibleSgprs = MaxSgprsAvailableWithSpiBug;
            pInfo->revision                   = AsicRevision::Iceland;
            pInfo->gfxStepping                = Abi::GfxIpSteppingIceland;
            pInfo->gfxip.tccSizeInBytes       = 256 * 1024;
        }
        else if (ASICREV_IS_TONGA_P(pInfo->eRevId))
        {
            pInfo->gfx6.numShaderEngines = 4;
            pInfo->gfx6.numWavesPerSimd  = 8;
            pInfo->gfx6.maxNumCuPerSh    = 8;
            pInfo->gfx6.maxNumRbPerSe    = 2;
            // NOTE: Tonga comes in both 256b and 384b variants, so we cannot accurately know the number of MCD
            // tiles here. Caller should override based on what the KMD reports.
            pInfo->gfx6.numMcdTiles           = 0;
            pInfo->gfx6.numTccBlocks          = 12;
            pInfo->gfx6.numShaderVisibleSgprs = MaxSgprsAvailableWithSpiBug;
            pInfo->revision                   = AsicRevision::Tonga;
            switch (pInfo->deviceId)
            {
            case DEVICE_ID_VI_TONGA_P_6929:
            case DEVICE_ID_VI_TONGA_P_692B:
            case DEVICE_ID_VI_TONGA_P_692F:
                pInfo->gfxStepping                = Abi::GfxIpSteppingTongaPro;
                break;
            default:
                pInfo->gfxStepping                = Abi::GfxIpSteppingTonga;
                break;
            }
            pInfo->gfxip.tccSizeInBytes       = 768_KiB;
        }
        else if (ASICREV_IS_FIJI_P(pInfo->eRevId))
        {
            pInfo->gfx6.numShaderEngines = 4;
            pInfo->gfx6.maxNumCuPerSh    = 16;
            pInfo->gfx6.maxNumRbPerSe    = 4;
            pInfo->gfx6.numMcdTiles      = 8;
            pInfo->gfx6.numTccBlocks     = 16;
            pInfo->revision              = AsicRevision::Fiji;
            pInfo->gfxip.tccSizeInBytes  = 2_MiB;
            pInfo->gfxStepping           = Abi::GfxIpSteppingFiji;

            pInfo->gfx6.supportTrapezoidTessDistribution = 1;
        }
        else if (ASICREV_IS_POLARIS10_P(pInfo->eRevId))
        {
            pInfo->gfx6.numShaderEngines = 4;
            pInfo->gfx6.numWavesPerSimd  = 8;
            pInfo->gfx6.maxNumCuPerSh    = 9;
            pInfo->gfx6.maxNumRbPerSe    = 2;
            pInfo->gfx6.numMcdTiles      = 4;
            pInfo->gfx6.numTccBlocks     = 8;
            pInfo->revision              = AsicRevision::Polaris10;
            pInfo->gfxStepping           = Abi::GfxIpSteppingPolaris;
            pInfo->gfxip.tccSizeInBytes  = 2_MiB;

            pInfo->gfxip.shaderPrefetchBytes = 2 * ShaderICacheLineSize;
            pInfo->gfx6.supportTrapezoidTessDistribution = 1;
        }
        else if (ASICREV_IS_POLARIS11_M(pInfo->eRevId))
        {
            pInfo->gfx6.numShaderEngines = 2;
            pInfo->gfx6.numWavesPerSimd  = 8;
            pInfo->gfx6.maxNumCuPerSh    = 8;
            pInfo->gfx6.maxNumRbPerSe    = 2;
            pInfo->gfx6.numMcdTiles      = 2;
            pInfo->gfx6.numTccBlocks     = 4;
            pInfo->revision              = AsicRevision::Polaris11;
            pInfo->gfxStepping           = Abi::GfxIpSteppingPolaris;
            pInfo->gfxip.tccSizeInBytes  = 1_MiB;

            pInfo->gfxip.shaderPrefetchBytes = 2 * ShaderICacheLineSize;
            pInfo->gfx6.supportTrapezoidTessDistribution = 1;
        }
        else if (ASICREV_IS_POLARIS12_V(pInfo->eRevId))
        {
            pInfo->gfx6.numShaderEngines = 2;
            pInfo->gfx6.numWavesPerSimd  = 8;
            pInfo->gfx6.maxNumCuPerSh    = 5;
            pInfo->gfx6.maxNumRbPerSe    = 2;
            pInfo->gfx6.numMcdTiles      = 2;
            pInfo->gfx6.numTccBlocks     = 4;
            pInfo->gfxip.tccSizeInBytes  = 512_KiB;
            pInfo->revision              = AsicRevision::Polaris12;
            pInfo->gfxStepping           = Abi::GfxIpSteppingPolaris;

            pInfo->gfxip.shaderPrefetchBytes = 2 * ShaderICacheLineSize;
            pInfo->gfx6.supportTrapezoidTessDistribution = 1;
        }
        break;
    // GFXIP 8.x APU's (Carrizo):
    case FAMILY_CZ:
        pInfo->gpuType = GpuType::Integrated;

        pInfo->gfx6.numShaderEngines         = 1;
        pInfo->gfx6.numShaderArrays          = 1;
        pInfo->gfx6.gsVgtTableDepth          = 16;
        pInfo->gfx6.maxGsWavesPerVgt         = 16;
        pInfo->gfx6.doubleOffchipLdsBuffers  = 1;
        pInfo->gfx6.support8bitIndices       = 1;
        pInfo->gfx6.support16BitInstructions = 1;
        pInfo->gfx6.numPhysicalSgprs         = 800;
        pInfo->gfx6.sgprAllocGranularity     = 16;
        pInfo->gfx6.minSgprAlloc             = 16;
        pInfo->gfx6.supportRgpTraces         = 1;

        // Support for IT_INDEX_ATTRIB_INDIRECT pkt has been enabled from microcode feature version 36 onwards for Gfx8
        pInfo->gfx6.supportIndexAttribIndirectPkt = (cpUcodeVersion >= 36);

        // Support for IT_SET_SH_REG_INDEX pkt has been enabled from microcode feature version 35 onwards for gfx-8.x
        pInfo->gfx6.supportSetShIndexPkt = (cpUcodeVersion >= 35);

        // Support for IT_LOAD_CONTEXT/SH_REG_INDEX has been enabled from microcode feature version 41
        // onwards for gfx-8.x.
        pInfo->gfx6.supportLoadRegIndexPkt = (cpUcodeVersion >= 41);

        // Support for IT_DUMP_CONST_RAM_OFFSET and IT_SET_SH_REF_OFFSET indexed mode has been enabled from microcode
        // feature version 45 onwards for gfx-8.x
        pInfo->gfx6.supportAddrOffsetDumpAndSetShPkt = (cpUcodeVersion >= 45);

        // Support for preemption within chained indirect buffers has been fixed starting with microcode feature version
        // 46 and onwards.
        pInfo->gfx6.supportPreemptionWithChaining = (cpUcodeVersion >= 46);

        pInfo->gfxip.vaRangeNumBits      = 48;
        pInfo->gfxip.tcpSizeInBytes      = 16384;
        pInfo->gfxip.maxLateAllocVsLimit = 64;

        pInfo->gfxip.instCacheSizePerCu   = 32_KiB;
        pInfo->gfxip.scalarCacheSizePerCu = 16_KiB;

        pInfo->gfxip.supportGl2Uncached      = 0;
        pInfo->gfxip.gl2UncachedCpuCoherency = 0;
        pInfo->gfxip.supportCaptureReplay    = 0;

        pInfo->imageProperties.prtFeatures = Gfx8PrtFeatures;
        pInfo->gfx6.supportPatchTessDistribution = 1;
        pInfo->gfx6.supportDonutTessDistribution = 1;

        if (ASICREV_IS_CARRIZO(pInfo->eRevId))
        {
            pInfo->gfx6.maxNumCuPerSh     = 8;
            pInfo->gfx6.maxNumRbPerSe     = 2;
            pInfo->gfx6.numMcdTiles       = 2;
            pInfo->gfx6.numTccBlocks      = 4;
            pInfo->gfx6.gsPrimBufferDepth = 768;
            pInfo->revision               = (ASICREV_IS_CARRIZO_BRISTOL(pInfo->eRevId)
                                             ? AsicRevision::Bristol
                                             : AsicRevision::Carrizo);
            pInfo->gfxStepping            = Abi::GfxIpSteppingCarrizo;
            pInfo->gfxip.tccSizeInBytes   = 512_KiB;
        }
        else if (ASICREV_IS_STONEY(pInfo->eRevId))
        {
            pInfo->gfx6.maxNumCuPerSh     = 3;
            pInfo->gfx6.maxNumRbPerSe     = 1;
            pInfo->gfx6.numMcdTiles       = 1;
            pInfo->gfx6.numTccBlocks      = 2;
            pInfo->gfx6.gsPrimBufferDepth = 256;
            pInfo->gfx6.rbPlus            = 1;
            pInfo->revision               = AsicRevision::Stoney;
            pInfo->gfxStepping            = Abi::GfxIpSteppingStoney;
            pInfo->gfxip.tccSizeInBytes   = 128_KiB;
        }
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }
}

// =====================================================================================================================
// Finalizes the GPU chip properties for a Device object, specifically for the GFX6 hardware layer. Intended to be
// called after InitializeGpuChipProperties().
void FinalizeGpuChipProperties(
    const Pal::Device& device,
    GpuChipProperties* pInfo)
{
    // Setup some GPU properties which can be derived from other properties:

    // GPU__GC__NUM_SE * GPU__GC__NUM_RB_PER_SE
    pInfo->gfx6.numTotalRbs = (pInfo->gfx6.numShaderEngines * pInfo->gfx6.maxNumRbPerSe);

    // We need to increase GcnMaxNumRbs if this assert triggers.
    PAL_ASSERT(pInfo->gfx6.numTotalRbs <= MaxNumRbs);

    // This will be overridden if any RBs are disabled.
    pInfo->gfx6.numActiveRbs = pInfo->gfx6.numTotalRbs;

    // GPU__GC__NUM_SE * GPU__GC__NUM_CU_PER_SE
    pInfo->alusPerClock = (pInfo->gfx6.numShaderEngines * pInfo->gfx6.numShaderArrays * pInfo->gfx6.maxNumCuPerSh);

    // Pixels per clock follows the following calculation:
    // GPU__GC__NUM_SE * GPU__GC__NUM_RB_PER_SE * (RBPlus ? 8 : 4)
    pInfo->pixelsPerClock = (pInfo->gfx6.numShaderEngines * pInfo->gfx6.maxNumRbPerSe * (pInfo->gfx6.rbPlus ? 8 : 4));

    // GPU__GC__NUM_SE
    pInfo->primsPerClock = pInfo->gfx6.numShaderEngines;

    // Texels per clock follows the following calculation:
    // GPU__GC__NUM_SE * GPU__GC__NUM_CU_PER_SE * (Number of Texture Pipes per CU).
    // Currently, the number of Texture Pipes per CU is always 1.
    pInfo->texelsPerClock = (pInfo->gfx6.numShaderEngines * pInfo->gfx6.numShaderArrays * pInfo->gfx6.maxNumCuPerSh);

    // GFXIP 7+ hardware only has one shader array per shader engine!
    PAL_ASSERT(pInfo->gfxLevel < GfxIpLevel::GfxIp7 || pInfo->gfx6.numShaderArrays == 1);

    // Loop over each shader engine and shader array to determine the actual number of active CU's per SE/SH.
    for (uint32 se = 0; se < pInfo->gfx6.numShaderEngines; ++se)
    {
        for (uint32 sh = 0; sh < pInfo->gfx6.numShaderArrays; ++sh)
        {
            uint32 cuMask          = 0;
            uint32 cuAlwaysOnMask  = 0;

            if (pInfo->gfxLevel == GfxIpLevel::GfxIp6)
            {
                cuMask         = pInfo->gfx6.activeCuMaskGfx6[se][sh];
                cuAlwaysOnMask = pInfo->gfx6.alwaysOnCuMaskGfx6[se][sh];
            }
            else
            {
                cuMask         = pInfo->gfx6.activeCuMaskGfx7[se];
                cuAlwaysOnMask = pInfo->gfx6.alwaysOnCuMaskGfx7[se];
            }

            const uint32 cuCount         = CountSetBits(cuMask);
            const uint32 cuAlwaysOnCount = CountSetBits(cuAlwaysOnMask);

            // It is expected that all SE's/SH's have the same number of CU's.
            PAL_ASSERT((pInfo->gfx6.numCuPerSh == 0) || (pInfo->gfx6.numCuPerSh == cuCount));
            pInfo->gfx6.numCuPerSh = Max(pInfo->gfx6.numCuPerSh, cuCount);

            // It is expected that all SE's/SH's have the same number of always-on CU's, or we need to
            // re-visit Max/Min below I assume
            PAL_ASSERT((pInfo->gfx6.numCuAlwaysOnPerSh == 0) || (pInfo->gfx6.numCuAlwaysOnPerSh == cuAlwaysOnCount));
            pInfo->gfx6.numCuAlwaysOnPerSh = Max(pInfo->gfx6.numCuAlwaysOnPerSh, cuAlwaysOnCount);
        }
    }

    PAL_ASSERT((pInfo->gfx6.numCuPerSh > 0) && (pInfo->gfx6.numCuPerSh <= pInfo->gfx6.maxNumCuPerSh));
    PAL_ASSERT((pInfo->gfx6.numCuAlwaysOnPerSh > 0) && (pInfo->gfx6.numCuAlwaysOnPerSh <= pInfo->gfx6.maxNumCuPerSh));

    memset(pInfo->gfxip.activePixelPackerMask, 0, sizeof(pInfo->gfxip.activePixelPackerMask));
    const uint32 numPixelPackersPerSe = pInfo->gfx6.numScPerSe * pInfo->gfx6.numPackerPerSc;
    PAL_ASSERT(numPixelPackersPerSe <= MaxPixelPackerPerSe);
    // By default, set all pixel packers to active based on the number of packers in a SE on a particular ASIC.
    // eg. if an ASIC has 2 pixel packers per SE with 4 shader engines, then packerMask = ... 0011 0011 0011 0011
    for (uint32 se = 0; se < pInfo->gfx6.numShaderEngines; ++se)
    {
        for (uint32 packer = 0; packer < numPixelPackersPerSe; ++packer)
        {
            WideBitfieldSetBit(pInfo->gfxip.activePixelPackerMask, packer + (MaxPixelPackerPerSe * se));
        }
    }

    // Initialize the performance counter info.  Perf counter info is reliant on a finalized GpuChipProperties
    // structure, so wait until the pInfo->gfx6 structure is "good to go".
    InitPerfCtrInfo(device, pInfo);
}

// =====================================================================================================================
// Initializes the performance experiment properties for this GPU.
void InitializePerfExperimentProperties(
    const GpuChipProperties&  chipProps,
    PerfExperimentProperties* pProperties)
{
    const Gfx6PerfCounterInfo& perfCounterInfo = chipProps.gfx6.perfCounterInfo;

    pProperties->features.u32All       = perfCounterInfo.features.u32All;
    pProperties->maxSqttSeBufferSize   = static_cast<size_t>(SqttMaximumBufferSize);
    pProperties->sqttSeBufferAlignment = static_cast<size_t>(SqttBufferAlignment);
    pProperties->shaderEngineCount     = chipProps.gfx6.numShaderEngines;

    for (uint32 blockIdx = 0; blockIdx < static_cast<uint32>(GpuBlock::Count); blockIdx++)
    {
        const PerfCounterBlockInfo&  blockInfo = perfCounterInfo.block[blockIdx];
        GpuBlockPerfProperties*const pBlock    = &pProperties->blocks[blockIdx];

        pBlock->available = (blockInfo.distribution != PerfCounterDistribution::Unavailable);

        if (pBlock->available)
        {
            pBlock->instanceCount             = blockInfo.numGlobalInstances;
            pBlock->maxEventId                = blockInfo.maxEventId;
            pBlock->maxGlobalOnlyCounters     = blockInfo.numGlobalOnlyCounters;
            pBlock->maxSpmCounters            = blockInfo.num16BitSpmCounters;
            pBlock->instanceGroupSize         = blockInfo.instanceGroupSize;

            // Note that the current interface says the shared count includes all global counters. This seems
            // to be contradictory, how can something be shared and global-only? Regardless, we cannot change this
            // without a major interface change so we must compute the total number of global counters here.
            pBlock->maxGlobalSharedCounters   = blockInfo.numGlobalSharedCounters + blockInfo.numGlobalOnlyCounters;
        }
    }
}

// =====================================================================================================================
// Initialize default values for the GPU engine properties for GFXIP 6/7/8 hardware.
void InitializeGpuEngineProperties(
    const GpuChipProperties& chipProps,
    GpuEngineProperties* pInfo)
{
    auto*const pUniversal = &pInfo->perEngine[EngineTypeUniversal];

    // We support If/Else/While on the universal queue; the command stream controls the max nesting depth.
    pUniversal->flags.timestampSupport                = 1;
    pUniversal->flags.borderColorPaletteSupport       = 1;
    pUniversal->flags.queryPredicationSupport         = 1;
    pUniversal->flags.memory32bPredicationEmulated    = 1; // Emulated by embedding a 64-bit predicate in the cmdbuf and copying from the 32-bit source.
    pUniversal->flags.memory64bPredicationSupport     = 1;
    pUniversal->flags.conditionalExecutionSupport     = 1;
    pUniversal->flags.loopExecutionSupport            = 1;
    pUniversal->flags.constantEngineSupport           = 1;
    pUniversal->flags.regMemAccessSupport             = 1;
    pUniversal->flags.indirectBufferSupport           = 1;
    pUniversal->flags.supportsMismatchedTileTokenCopy = 1;
    pUniversal->flags.supportsImageInitBarrier        = 1;
    pUniversal->flags.supportsImageInitPerSubresource = 1;
    pUniversal->flags.supportsUnmappedPrtPageAccess   = 1;
    pUniversal->flags.supportsClearCopyMsaaDsDst      = 1;
    pUniversal->maxControlFlowNestingDepth            = CmdStream::CntlFlowNestingLimit;
    pUniversal->minTiledImageCopyAlignment.width      = 1;
    pUniversal->minTiledImageCopyAlignment.height     = 1;
    pUniversal->minTiledImageCopyAlignment.depth      = 1;
    pUniversal->minTiledImageMemCopyAlignment.width   = 1;
    pUniversal->minTiledImageMemCopyAlignment.height  = 1;
    pUniversal->minTiledImageMemCopyAlignment.depth   = 1;
    pUniversal->minLinearMemCopyAlignment.width       = 1;
    pUniversal->minLinearMemCopyAlignment.height      = 1;
    pUniversal->minLinearMemCopyAlignment.depth       = 1;
    pUniversal->minTimestampAlignment                 = 8; // The CP spec requires 8-byte alignment.
    pUniversal->queueSupport                          = SupportQueueTypeUniversal;

    auto*const pCompute = &pInfo->perEngine[EngineTypeCompute];

    // GFX6 supports compute queue control flow for free because it doesn't have ACEs.
    // GFX7 supports it starting with feature version 27.
    // GFX8 and GFX8.1 support it starting with feature version 32.
    if ((chipProps.gfxLevel == GfxIpLevel::GfxIp6) ||
        ((chipProps.gfxLevel == GfxIpLevel::GfxIp7) && (chipProps.cpUcodeVersion >= 27)) ||
        ((chipProps.gfxLevel >= GfxIpLevel::GfxIp8) && (chipProps.cpUcodeVersion >= 32)))
    {
        pCompute->flags.conditionalExecutionSupport = 1;
        pCompute->flags.loopExecutionSupport        = 1;
        pCompute->maxControlFlowNestingDepth        = CmdStream::CntlFlowNestingLimit;
    }

    pCompute->flags.timestampSupport                = 1;
    pCompute->flags.borderColorPaletteSupport       = 1;
    pCompute->flags.queryPredicationSupport         = 1;
    pCompute->flags.memory32bPredicationSupport     = 1;
    pCompute->flags.memory64bPredicationSupport     = 1;
    pCompute->flags.regMemAccessSupport             = 1;
    pCompute->flags.indirectBufferSupport           = 1;
    pCompute->flags.supportsMismatchedTileTokenCopy = 1;
    pCompute->flags.supportsImageInitBarrier        = 1;
    pCompute->flags.supportsImageInitPerSubresource = 1;
    pCompute->flags.supportsUnmappedPrtPageAccess   = 1;
    pCompute->minTiledImageCopyAlignment.width      = 1;
    pCompute->minTiledImageCopyAlignment.height     = 1;
    pCompute->minTiledImageCopyAlignment.depth      = 1;
    pCompute->minTiledImageMemCopyAlignment.width   = 1;
    pCompute->minTiledImageMemCopyAlignment.height  = 1;
    pCompute->minTiledImageMemCopyAlignment.depth   = 1;
    pCompute->minLinearMemCopyAlignment.width       = 1;
    pCompute->minLinearMemCopyAlignment.height      = 1;
    pCompute->minLinearMemCopyAlignment.depth       = 1;
    pCompute->minTimestampAlignment                 = 8; // The CP spec requires 8-byte alignment.
    pCompute->queueSupport                          = SupportQueueTypeCompute;

    // Note that we set this DMA state in the GFXIP layer because it deals with GFXIP features that the OSSIP layer
    // doesn't need to understand.
    auto*const pDma = &pInfo->perEngine[EngineTypeDma];

    pDma->flags.memory32bPredicationSupport     = 1;
    pDma->flags.supportsImageInitBarrier        = 1;
    pDma->flags.supportsImageInitPerSubresource = 1;
    pDma->flags.supportsMismatchedTileTokenCopy = 1;
    pDma->flags.supportsUnmappedPrtPageAccess   = 0; // SDMA can't ignore VM faults for PRT resources on GFXIP 6/7/8.
}

// =====================================================================================================================
// Creates a GFX6 specific settings loader object
Pal::ISettingsLoader* CreateSettingsLoader(
    Pal::Device* pDevice)
{
    return PAL_NEW(Gfx6::SettingsLoader, pDevice->GetPlatform(), AllocInternal)(pDevice);
}

} // Gfx6
} // Pal
