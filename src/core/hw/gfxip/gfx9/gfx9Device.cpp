/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/queue.h"
#include "core/hw/amdgpu_asic.h"
#include "core/hw/gfxip/gfx9/g_gfx9MergedDataFormats.h"
#include "core/hw/gfxip/gfx9/gfx9BorderColorPalette.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUploadRing.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9ColorBlendState.h"
#include "core/hw/gfxip/gfx9/gfx9ColorTargetView.h"
#include "core/hw/gfxip/gfx9/gfx9ComputeCmdBuffer.h"
#include "core/hw/gfxip/gfx9/gfx9ComputeEngine.h"
#include "core/hw/gfxip/gfx9/gfx9ComputePipeline.h"
#include "core/hw/gfxip/gfx9/gfx9DepthStencilState.h"
#include "core/hw/gfxip/gfx9/gfx9DepthStencilView.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9FormatInfo.h"
#include "core/hw/gfxip/gfx9/gfx9GraphicsPipeline.h"
#include "core/hw/gfxip/gfx9/gfx9HybridGraphicsPipeline.h"
#include "core/hw/gfxip/gfx9/gfx9Image.h"
#include "core/hw/gfxip/gfx9/gfx9IndirectCmdGenerator.h"
#include "core/hw/gfxip/gfx9/gfx9MsaaState.h"
#include "core/hw/gfxip/gfx9/gfx9OcclusionQueryPool.h"
#include "core/hw/gfxip/gfx9/gfx9PerfCtrInfo.h"
#include "core/hw/gfxip/gfx9/gfx9PerfExperiment.h"
#include "core/hw/gfxip/gfx9/gfx9PipelineStatsQueryPool.h"
#include "core/hw/gfxip/gfx9/gfx9QueueContexts.h"
#include "core/hw/gfxip/gfx9/gfx9SettingsLoader.h"
#include "core/hw/gfxip/gfx9/gfx9ShaderLibrary.h"
#include "core/hw/gfxip/gfx9/gfx9ShadowedRegisters.h"
#include "core/hw/gfxip/gfx9/gfx9StreamoutStatsQueryPool.h"
#include "core/hw/gfxip/gfx9/gfx9UniversalCmdBuffer.h"
#include "core/hw/gfxip/gfx9/gfx9UniversalEngine.h"
#include "core/hw/gfxip/sdma/gfx10/gfx10DmaCmdBuffer.h"
#include "palAssert.h"
#include "palAutoBuffer.h"
#include "palDequeImpl.h"
#include "palFormatInfo.h"
#include "palLiterals.h"

using namespace Util;
using namespace Util::Literals;
using namespace Pal::Formats::Gfx9;

namespace Pal
{
namespace Gfx9
{
static BufferSrd     nullBufferView = {};
static ImageSrd      nullImageView  = {};
constexpr SamplerSrd NullSampler    = {};

// Microcode version for CE dump offset support
constexpr uint32 UcodeVersionWithDumpOffsetSupport = 30;

// Microcode version for SET_SH_REG_OFFSET with 256B alignment.
constexpr uint32 Gfx9UcodeVersionSetShRegOffset256B  = 42;
constexpr uint32 Gfx10UcodeVersionSetShRegOffset256B = 27;

// =====================================================================================================================
size_t GetDeviceSize(
    GfxIpLevel  gfxLevel)
{
    size_t  rpmSize = sizeof(Gfx9RsrcProcMgr);

    if (IsGfx10Plus(gfxLevel))
    {
        rpmSize = sizeof(Gfx10RsrcProcMgr);
    }

    return (sizeof(Device) + rpmSize);
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

        switch (pDevice->ChipProperties().gfxLevel)
        {
        case GfxIpLevel::GfxIp9:
            pPfnTable->pfnCreateTypedBufViewSrds   = &Device::Gfx9CreateTypedBufferViewSrds;
            pPfnTable->pfnCreateUntypedBufViewSrds = &Device::Gfx9CreateUntypedBufferViewSrds;
            pPfnTable->pfnCreateImageViewSrds      = &Device::Gfx9CreateImageViewSrds;
            pPfnTable->pfnCreateSamplerSrds        = &Device::Gfx9CreateSamplerSrds;
            break;

        case GfxIpLevel::GfxIp10_1:
        case GfxIpLevel::GfxIp10_3:
#if PAL_BUILD_GFX11
        case GfxIpLevel::GfxIp11_0:
#endif
            pPfnTable->pfnCreateTypedBufViewSrds   = &Device::Gfx10CreateTypedBufferViewSrds;
            pPfnTable->pfnCreateUntypedBufViewSrds = &Device::Gfx10CreateUntypedBufferViewSrds;
            pPfnTable->pfnCreateImageViewSrds      = &Device::Gfx10CreateImageViewSrds;
            pPfnTable->pfnCreateSamplerSrds        = &Device::Gfx10CreateSamplerSrds;
            break;

        default:
            PAL_ASSERT_ALWAYS();
            break;
        }

        pPfnTable->pfnCreateFmaskViewSrds = &Device::CreateFmaskViewSrds;
        pPfnTable->pfnCreateBvhSrds       = &Device::CreateBvhSrds;
    }

    return result;
}

// =====================================================================================================================
// Returns the offset of the frame counter register (mmMP1_SMN_FPS_CNT) for the specified GPU.  Returns zero if
// the current GPU doesn't support frame-counts.
static uint32 GetFrameCountRegister(
    const Pal::Device*  pDevice)
{
    uint32       frameCountRegister = 0;

    //@todo:  different parts have different offsets for the frame-counter register.  Instead of hard-coding
    //        the offset of the different registers for APUs vs. GPUs, we're ultimately going to get this info
    //        from the KMD.  For now, play nice with Vega10 since that one is known.
    // Skip setting the fps cnt register from UMD if KMD sets the smnFpsCntRegWrittenByKmd bit,
    // which indicates that it will be written by KMD.
    if (IsVega10(*pDevice) && pDevice->ShouldWriteFrameCounterRegister())
    {
        if (pDevice->ChipProperties().cpUcodeVersion >= 31)
        {
            frameCountRegister = Vg10_Vg12_Rn::mmMP1_SMN_FPS_CNT;
        }
    }

    return frameCountRegister;
}

// =====================================================================================================================
Device::Device(
    Pal::Device* pDevice)
    :
    GfxDevice(pDevice,
              nullptr, // RPM, we don't know it's address until earlyInit timeframe
              GetFrameCountRegister(pDevice)),
    m_cmdUtil(*this),
    m_queueContextUpdateCounter(0),
    // The default value of MSAA rate is 1xMSAA.
    m_msaaRate(1),
    m_presentResolution({ 0,0 }),
    m_pVrsDepthView(nullptr),
    m_vrsDepthViewMayBeNeeded(false),
    m_gbAddrConfig(m_pParent->ChipProperties().gfx9.gbAddrConfig),
    m_gfxIpLevel(pDevice->ChipProperties().gfxLevel),
    m_varBlockSize(0)
{
    PAL_ASSERT(((GetGbAddrConfig().bits.NUM_PIPES - GetGbAddrConfig().bits.NUM_RB_PER_SE) < 2) ||
               IsGfx10Plus(m_gfxIpLevel));

    for (uint32  shaderStage = 0; shaderStage < HwShaderStage::Last; shaderStage++)
    {
        m_firstUserDataReg[shaderStage] = GetBaseUserDataReg(static_cast<HwShaderStage>(shaderStage)) +
                                          FastUserDataStartReg;
    }
    memset(const_cast<uint32*>(&m_msaaHistogram[0]), 0, sizeof(m_msaaHistogram));

#if PAL_BUILD_GFX11
    memset(const_cast<BoundGpuMemory*>(&m_vertexAttributesMem[0]), 0, sizeof(m_vertexAttributesMem));
#endif

    if (IsGfx103PlusExclusive(*Parent())
#if PAL_BUILD_GFX11
        && (IsGfx11(*Parent()) == false)
#endif
       )
    {
#if PAL_ENABLE_PRINTS_ASSERTS
        // The packer-based number of SA's can be less than the physical number of SA's, but it better not be more.
        const auto&  chipProps     = m_pParent->ChipProperties().gfx9;
        const uint32 chipPropNumSa = chipProps.numShaderArrays * chipProps.numShaderEngines;

        PAL_ASSERT((1u << Gfx103PlusExclusiveGetNumActiveShaderArraysLog2()) <= chipPropNumSa);
#endif
        // Var block size = number of total pipes * 16KB
        // This fields is filled out for all Gfx10.2+, but only used
        // for Gfx10.2 and Gfx10.3
        m_varBlockSize = 16384u << GetGbAddrConfig().bits.NUM_PIPES;
    }
}

// =====================================================================================================================
// This must clean up all internal GPU memory allocations and all objects created after EarlyInit. Note that EarlyInit
// is called when the platform creates the device objects so the work it does must be preserved if we are to reuse
// this device object.
Result Device::Cleanup()
{
    // RsrcProcMgr::Cleanup must be called before GfxDevice::Cleanup because the ShaderCache object referenced by
    // RsrcProcMgr is owned by GfxDevice and gets reset on GfxDevice::Cleanup.
    m_pRsrcProcMgr->Cleanup();

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

    if (m_dummyZpassDoneMem.IsBound())
    {
        result = m_pParent->MemMgr()->FreeGpuMem(m_dummyZpassDoneMem.Memory(), m_dummyZpassDoneMem.Offset());
        m_dummyZpassDoneMem.Update(nullptr, 0);
    }

#if PAL_BUILD_GFX11
    for (uint32 i = 0; i < ArrayLen(m_vertexAttributesMem); i++)
    {
        if (m_vertexAttributesMem[i].IsBound())
        {
            result = m_pParent->MemMgr()->FreeGpuMem(m_vertexAttributesMem[i].Memory(), m_vertexAttributesMem[i].Offset());
            m_vertexAttributesMem[i].Update(nullptr, 0);
        }
    }
#endif

    if (m_pVrsDepthView != nullptr)
    {
        DestroyVrsDepthImage(m_pVrsDepthView->GetImage()->Parent());
        m_pVrsDepthView = nullptr;
    }

    if (result == Result::Success)
    {
        result = GfxDevice::Cleanup();
    }

    // We don't need to free the NestedCmdBufNggMem or NestedCmdBufInheritGpuMem or the CeRingBufferGpuMem because they
    // are allocated via the internal memory manager
    return result;
}

// =====================================================================================================================
// Performs early initialization of this device; this occurs when the device is created.
Result Device::EarlyInit()
{
    // The shader cache is a constant size and RPM is not, so to simplify allocation, we will allocate RPM in space
    // following the shader cache in memory, even if the shader cache ends up not being created.
    void*const pRpmPlacementAddr = (this + 1);

    if (IsGfx9(*m_pParent))
    {
        m_pRsrcProcMgr = PAL_PLACEMENT_NEW(pRpmPlacementAddr) Pal::Gfx9::Gfx9RsrcProcMgr(this);
    }
    else if (IsGfx10Plus(m_gfxIpLevel))
    {
        m_pRsrcProcMgr = PAL_PLACEMENT_NEW(pRpmPlacementAddr) Pal::Gfx9::Gfx10RsrcProcMgr(this);
    }
    else
    {
        // No RPM, you're not going to get very far...
        PAL_ASSERT_ALWAYS();
    }

    Result result = m_pRsrcProcMgr->EarlyInit();

    SetupWorkarounds();

    return result;
}

// =====================================================================================================================
// Sets up the hardware workaround/support flags based on the current ASIC
void Device::SetupWorkarounds()
{
    const auto& gfx9Props = m_pParent->ChipProperties().gfx9;
    // The LBPW feature uses a fixed late alloc VS limit based off of the available CUs.
    if (gfx9Props.lbpwEnabled || IsGfx10(*m_pParent))
    {
        m_useFixedLateAllocVsLimit = true;
    }

    if (gfx9Props.numCuPerSh > 2)
    {
        if (m_useFixedLateAllocVsLimit)
        {
            if (IsGfx9(*m_pParent))
            {
                // Use a fixed value for the late alloc VS limit based on the number of available CUs
                // on the GPU. The computation is late_alloc_waves = 4 * (Available_CUs - 1)
                m_lateAllocVsLimit = 4 * (gfx9Props.numCuPerSh - 1);
            }
            else
            {
                PAL_ASSERT(IsGfx10(*m_pParent));
                // On Gfx10, a limit of 4 * (NumCUs/SA - 1) has been found to be optimal
                m_lateAllocVsLimit = 4 * (gfx9Props.numCuPerSh - 1);
            }
        }
        else if (m_lateAllocVsLimit == LateAllocVsInvalid)
        {
            // 4 * (numCu - 2), enable Late Alloc VS feature for GFX9 asics that have over 2 CUs
            // per shader array (SH). Note that the final ShaderLateAllocVs.bits.LIMIT will be
            // adjusted later in GraphicsPipeline::InitLateAllocVs
            m_lateAllocVsLimit = ((gfx9Props.numCuPerSh - 2) << 2);
        }
    }

    if (IsGfx9(*m_pParent))
    {
        m_waEnableDccCacheFlushAndInvalidate = true;

        m_waTcCompatZRange = true;
    }
    else if (IsGfx10(*m_pParent))
    {
        m_waEnableDccCacheFlushAndInvalidate = true;

    }
#if PAL_BUILD_GFX11
    else if (IsGfx11(*m_pParent))
    {
        m_waEnableDccCacheFlushAndInvalidate = true;
    }
#endif
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
    const Gfx9PalSettings& settings = GetGfx9Settings(*Parent());

    GfxDevice::FinalizeChipProperties(pChipProperties);

    pChipProperties->gfx9.supportImplicitPrimitiveShader = settings.nggSupported;

    switch (settings.offchipLdsBufferSize)
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

    pChipProperties->gfxip.numOffchipTessBuffers = settings.numOffchipLdsBuffers;

    pChipProperties->gfxip.maxPrimgroupSize = 253;

    pChipProperties->gfxip.tessFactorBufferSizePerSe = settings.tessFactorBufferSizePerSe;

    pChipProperties->gfx9.gfx10.supportVrsWithDsExports = settings.waDisableVrsWithDsExports ? false : true;
}

// =====================================================================================================================
// Peforms extra initialization which needs to be done after the parent Device is finalized.
Result Device::Finalize()
{
    Result result = GfxDevice::Finalize();

    if (result == Result::Success)
    {
        result = m_pRsrcProcMgr->LateInit();
    }

    if (result == Result::Success)
    {
        result = InitOcclusionResetMem();
    }

#if PAL_BUILD_GFX11
    if (result == Result::Success)
    {
        // Start off by allocating the non-TMZ version.
        result = AllocateVertexAttributesMem(false);
    }
#endif

    // CreateVrsDepthView dependents on GetImageSize, which isn't supported on NullDevice. Since VrsDepthView isn't used
    // on NullDevice, so we skip it now.
    if ((result == Result::Success) && (m_pParent->IsNull() == false))
    {
        const Pal::Device*  pParent  = Parent();
        const auto&         settings = GetGfx9Settings(*pParent);

        if (pParent->ChipProperties().gfxip.supportsVrs                      &&
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 713
            (pParent->GetPublicSettings()->disableInternalVrsImage == false) &&
#endif
            (settings.vrsImageSize != 0))
        {
            if (IsGfx10(*pParent))
            {
                // GFX10 era-devices require a stand-alone hTile buffer to store the image-rate data when a client
                // hTile buffer isn't bound. Defer allocation to on demand.
                m_vrsDepthViewMayBeNeeded = true;
            }
#if PAL_BUILD_GFX11
            else if (IsGfx11(*Parent()))
            {
                // GFX11 supports VRS, but it won't utilize the hTile buffer to do so, so the CreateVrsDepthView
                // function is useless on that platform.  GFX11 VRS implementation hasn't been contemplated yet.
            }
#endif
        }

    }

    return result;
}

// =====================================================================================================================
// As a performance optimization, we have a small piece of video memory which contains the reset values for each slot in
// an occlusion query pool. This initializes that memory for future use.
Result Device::InitOcclusionResetMem()
{
    Result result = Result::Success;

    const GpuChipProperties& chipProps = m_pParent->ChipProperties();

    // First, we initialize our copy of the reset data for a single query slot.
    memset(&m_occlusionSlotResetValues[0], 0, sizeof(m_occlusionSlotResetValues));

    // For GFX9+, rbs pack the results of active rbs in-order.
    for (uint32 rb = chipProps.gfx9.numActiveRbs; rb < chipProps.gfx9.numTotalRbs; rb++)
    {
        m_occlusionSlotResetValues[rb].begin.bits.valid = 1;
        m_occlusionSlotResetValues[rb].end.bits.valid   = 1;
    }

    const Gfx9PalSettings& gfx9Settings = GetGfx9Settings(*m_pParent);

    const size_t slotSize = chipProps.gfx9.numTotalRbs * sizeof(OcclusionQueryResultPair);

    PAL_ALERT(slotSize > sizeof(m_occlusionSlotResetValues));

    // Second, if the DMA optimization is enabled, we allocate a buffer of local memory to accelerate large
    // resets using DMA.
    GpuMemoryCreateInfo srcMemCreateInfo = { };
    srcMemCreateInfo.alignment = sizeof(uint32);
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

    if (gfx9Settings.waDummyZpassDoneBeforeTs)
    {
        // This unprotected memory will be written under TMZ state. So align to 64 bytes for Tmz Write Block Size.
        constexpr uint32 TmzWriteBlockSize = 64;

        // We need enough space for a full occlusion query slot because the DBs write to every other result location.
        // According to the packet spec it must be QWORD-aligned. We prefer the local heap to avoid impacting timestamp
        // performance and expect to get suballocated out of the same raft as the occlusion reset memory above.
        const uint32 gpuMemSize = chipProps.gfx9.numTotalRbs * sizeof(OcclusionQueryResultPair);
        GpuMemoryCreateInfo zPassDoneCreateInfo = {};
        zPassDoneCreateInfo.alignment = TmzWriteBlockSize;
        zPassDoneCreateInfo.size      = Util::Pow2Align(gpuMemSize, TmzWriteBlockSize);
        zPassDoneCreateInfo.priority  = GpuMemPriority::Normal;
        zPassDoneCreateInfo.heaps[0]  = GpuHeapLocal;
        zPassDoneCreateInfo.heaps[1]  = GpuHeapGartUswc;
        zPassDoneCreateInfo.heapCount = 2;

        pMemObj   = nullptr;
        memOffset = 0;

        result = m_pParent->MemMgr()->AllocateGpuMem(zPassDoneCreateInfo, internalInfo, false, &pMemObj, &memOffset);

        if (result == Result::Success)
        {
            m_dummyZpassDoneMem.Update(pMemObj, memOffset);
        }
    }

    return result;
}

#if PAL_BUILD_GFX11
// =====================================================================================================================
// Allocate the ring buffer for attributes through memory.
Result Device::AllocateVertexAttributesMem(
    bool isTmz)
{
    const MutexAuto lock(&m_ringSizesLock);
    Result result = Result::Success;
    // Create the attributes through memory ring buffer
    const Gfx9PalSettings& settings = GetGfx9Settings(*Parent());

    if ((settings.gfx11VertexAttributesRingBufferSizePerSe != 0) &&
        (m_vertexAttributesMem[isTmz].IsBound() == false))
    {
        const auto& chipProps = m_pParent->ChipProperties().gfx9;

        GpuMemoryCreateInfo createInfo = { };
        createInfo.size      = (settings.gfx11VertexAttributesRingBufferSizePerSe * chipProps.numShaderEngines);
        createInfo.alignment = Gfx11VertexAttributeRingAlignmentBytes;
        createInfo.priority  = GpuMemPriority::Normal;

        if (m_pParent->HeapLogicalSize(GpuHeapInvisible) > 0)
        {
            createInfo.heapCount = 3;
            createInfo.heaps[0]  = GpuHeapInvisible;
            createInfo.heaps[1]  = GpuHeapLocal;
            createInfo.heaps[2]  = GpuHeapGartUswc;
        }
        else
        {
            createInfo.heapCount = 2;
            createInfo.heaps[0]  = GpuHeapLocal;
            createInfo.heaps[1]  = GpuHeapGartUswc;
        }

        createInfo.flags.tmzProtected = isTmz;

        GpuMemoryInternalCreateInfo internalInfo = {};
        internalInfo.flags.alwaysResident = 1;

        GpuMemory* pGpuMemory = nullptr;
        gpusize    memOffset  = 0;
        gpusize*   pMemOffset = &memOffset;

        InternalMemMgr*const pMemMgr = Parent()->MemMgr();
        result = pMemMgr->AllocateGpuMem(createInfo, internalInfo, 0, &pGpuMemory, pMemOffset);
        if (result == Result::Success)
        {
            m_vertexAttributesMem[isTmz].Update(pGpuMemory, memOffset);
        }
    }

    return result;
}
#endif

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
        // According to the addressing doc, we simply have to align everything to the SW_LINEAR block size (256 bytes).
        constexpr uint16 LinearBlkSize = 256;

        pAlignments->baseAddress = LinearBlkSize;
        pAlignments->rowPitch    = LinearBlkSize;
        pAlignments->depthPitch  = LinearBlkSize;
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

// =====================================================================================================================
// Returns whether or not state shadowing should be enabled.
bool Device::UseStateShadowing(
    EngineType engineType
    ) const
{
    return ForceStateShadowing ||
#if PAL_BUILD_GFX11
           // Context register shadowing should always be enabled for gfx11+
           (Pal::Device::EngineSupportsGraphics(engineType) &&
            IsGfx11(*Parent()))                             ||
#endif
           Parent()->IsPreemptionSupported(engineType);
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
    return (pCmdSpace + m_cmdUtil.BuildWaitRegMem(engineType,
                                                  mem_space__me_wait_reg_mem__memory_space,
                                                  function__me_wait_reg_mem__equal_to_the_reference_value,
                                                  engine_sel__me_wait_reg_mem__micro_engine,
                                                  m_debugStallGpuMem.GpuVirtAddr(),
                                                  number,
                                                  UINT_MAX,
                                                  pCmdSpace));
}
#endif

// =====================================================================================================================
Result Device::CreateEngine(
    EngineType engineType,
    uint32     engineIndex,
    Engine**   ppEngine)
{
    Result  result = Result::ErrorOutOfMemory;
    Engine* pEngine = nullptr;

    switch (engineType)
    {
    case EngineTypeUniversal:
        pEngine = PAL_NEW(UniversalEngine, GetPlatform(), AllocInternalShader)(this, engineType, engineIndex);
        break;
    case EngineTypeCompute:
        pEngine = PAL_NEW(ComputeEngine, GetPlatform(), AllocInternal)(this, engineType, engineIndex);
        break;
    case EngineTypeDma:
        // Gfx10+ has the DMA engine on the GFX level, not the OSS level
        if (IsGfx10Plus(m_gfxIpLevel))
        {
            pEngine = PAL_NEW(Engine, GetPlatform(), AllocInternal)(*Parent(), engineType, engineIndex);
        }
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
    else if (pEngine != nullptr)
    {
        PAL_DELETE(pEngine, GetPlatform());
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
        if (engineType == EngineTypeDma)
        {
            pCmdSpace = DmaCmdBuffer::BuildNops(pCmdSpace, pCmdStream->GetSizeAlignDwords());
        }
        else
        {
            pCmdSpace += m_cmdUtil.BuildNop(CmdUtil::MinNopSizeInDwords, pCmdSpace);
        }

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
// Determines the size of the QueueContext object needed for GFXIP9+ hardware. Only supported on Universal and
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
    case QueueTypeDma:
        PAL_ASSERT(IsGfx10Plus(m_gfxIpLevel));

        size = sizeof(QueueContext);
        break;
    default:
        break;
    }

    return size;
}

// =====================================================================================================================
// Creates the QueueContext object for the specified Queue in preallocated memory. Only supported on Universal and
// Compute Queues. The createInfo here is not the orignally createInfo passed by Pal client. It's an updated one after
// execution of queue's contructor.
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
            const bool useStateShadowing = UseStateShadowing(createInfo.engineType);
            UniversalQueueContext* pContext =
                PAL_PLACEMENT_NEW(pPlacementAddr) UniversalQueueContext(
                this,
                useStateShadowing,
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
    case QueueTypeDma:
        // All GFX10+ parts implement the DMA queue on the GFX engine.
        PAL_ASSERT(IsGfx10Plus(m_gfxIpLevel));

        (*ppQueueContext) = PAL_PLACEMENT_NEW(pPlacementAddr) QueueContext(Parent());

        result = Result::Success;
        break;
    default:
        result = Result::ErrorUnavailable;
        break;
    }

    return result;
}

// =====================================================================================================================
static Result ConvertAbiRegistersToMetadata(
    Device*                     pDevice,
    PalAbi::CodeObjectMetadata* pMetadata,
    MsgPackReader*              pMetadataReader)
{
    const Pal::Device& palDevice = *(pDevice->Parent());

    RegisterVector registers(pDevice->GetPlatform());

    Result result = Result::Success;
    if (pMetadata->pipeline.hasEntry.registers != 0)
    {
        result = pMetadataReader->Seek(pMetadata->pipeline.registers);
        if (result == Result::Success)
        {
            result = pMetadataReader->Unpack(&registers);
        }
    }

    if ((result == Result::Success) && (registers.NumElements() > 0))
    {
        const GpuChipProperties& chipProps  = palDevice.ChipProperties();
        const RegisterInfo&      regInfo    = pDevice->CmdUtil().GetRegInfo();
        const bool               hasHwVs    = chipProps.gfxip.supportsHwVs;
        const bool               supportSpp = chipProps.gfx9.supportSpp;

        // If there are any registers, we need to convert them into the metadata.
        PalAbi::PipelineMetadata*         pPipeline         = &pMetadata->pipeline;
        PalAbi::ComputeRegisterMetadata*  pComputeRegisters = &pPipeline->computeRegister;
        PalAbi::GraphicsRegisterMetadata* pGfxRegisters     = &pPipeline->graphicsRegister;

        // Temporary macros to make some of this nightmare easier to read/more compact.
        #define PAL_SET_ABI_FIELD(reg, field, value) \
            reg->field          = value;             \
            reg->hasEntry.field = 1;

        #define PAL_SET_ABI_FIELD_IDX(reg, field, idx, value) \
            reg->field[idx]     = value;                      \
            reg->hasEntry.field = 1;

        #define PAL_SET_ABI_FLAG(reg, field, value) \
            reg->flags.field    = value;            \
            reg->hasEntry.field = 1;

        // All graphics pipelines have VGT_SHADER_STAGES_EN.
        if (registers.HasEntry(mmVGT_SHADER_STAGES_EN))
        {
            // VGT_SHADER_STAGES_EN
            {
                PalAbi::VgtShaderStagesEnMetadata* pVgtShaderStagesEn = &pGfxRegisters->vgtShaderStagesEn;

                VGT_SHADER_STAGES_EN vgtShaderStagesEn;
                vgtShaderStagesEn.u32All = registers.At(mmVGT_SHADER_STAGES_EN);

                PAL_SET_ABI_FIELD(pVgtShaderStagesEn, esStageEn,          vgtShaderStagesEn.bits.ES_EN);
                PAL_SET_ABI_FIELD(pVgtShaderStagesEn, vsStageEn,          vgtShaderStagesEn.bits.VS_EN);
                PAL_SET_ABI_FIELD(pVgtShaderStagesEn, maxPrimgroupInWave, vgtShaderStagesEn.bits.MAX_PRIMGRP_IN_WAVE);

                PAL_SET_ABI_FLAG(pVgtShaderStagesEn, lsStageEn,     vgtShaderStagesEn.bits.LS_EN);
                PAL_SET_ABI_FLAG(pVgtShaderStagesEn, hsStageEn,     vgtShaderStagesEn.bits.HS_EN);
                PAL_SET_ABI_FLAG(pVgtShaderStagesEn, gsStageEn,     vgtShaderStagesEn.bits.GS_EN);
                PAL_SET_ABI_FLAG(pVgtShaderStagesEn, primgenEn,     vgtShaderStagesEn.bits.PRIMGEN_EN);
                PAL_SET_ABI_FLAG(pVgtShaderStagesEn, orderedIdMode, vgtShaderStagesEn.bits.ORDERED_ID_MODE);

                if (IsGfx10Plus(palDevice))
                {
                    PAL_SET_ABI_FIELD(pVgtShaderStagesEn, gsFastLaunch, vgtShaderStagesEn.gfx09_1xPlus.GS_FAST_LAUNCH);

                    PAL_SET_ABI_FLAG(pVgtShaderStagesEn,  dynamicHs,    vgtShaderStagesEn.gfx10Plus.DYNAMIC_HS);
                    PAL_SET_ABI_FLAG(pVgtShaderStagesEn,  nggWaveIdEn,  vgtShaderStagesEn.gfx10Plus.NGG_WAVE_ID_EN);
                    PAL_SET_ABI_FLAG(pVgtShaderStagesEn,
                                     primgenPassthruEn,
                                     vgtShaderStagesEn.gfx10Plus.PRIMGEN_PASSTHRU_EN);

                    auto* pHsHwStage = &pMetadata->pipeline.hardwareStage[uint32(Abi::HardwareStage::Hs)];
                    if (pHsHwStage->hasEntry.wavefrontSize == 0)
                    {
                        PAL_SET_ABI_FIELD(pHsHwStage, wavefrontSize, (vgtShaderStagesEn.gfx10Plus.HS_W32_EN) ? 32 : 64);
                    }

                    auto* pGsHwStage = &pMetadata->pipeline.hardwareStage[uint32(Abi::HardwareStage::Gs)];
                    if (pGsHwStage->hasEntry.wavefrontSize == 0)
                    {
                        PAL_SET_ABI_FIELD(pGsHwStage, wavefrontSize, (vgtShaderStagesEn.gfx10Plus.GS_W32_EN) ? 32 : 64);
                    }

                    auto* pVsHwStage = &pMetadata->pipeline.hardwareStage[uint32(Abi::HardwareStage::Vs)];
                    if (pVsHwStage->hasEntry.wavefrontSize == 0)
                    {
                        PAL_SET_ABI_FIELD(pVsHwStage, wavefrontSize, (vgtShaderStagesEn.gfx10Plus.VS_W32_EN) ? 32 : 64);
                    }
                }

#if  PAL_BUILD_GFX11
                if (IsGfx104Plus(palDevice))
                {
                    PAL_SET_ABI_FLAG(pVgtShaderStagesEn,
                                     primgenPassthruNoMsg,
                                     vgtShaderStagesEn.gfx104Plus.PRIMGEN_PASSTHRU_NO_MSG);
                }
#endif
            }

            // VGT_GS_MODE
            {
                VGT_GS_MODE vgtGsMode;
                if (hasHwVs && registers.HasEntry(HasHwVs::mmVGT_GS_MODE, &vgtGsMode.u32All))
                {
                    PalAbi::VgtGsModeMetadata* pVgtGsMode = &pGfxRegisters->vgtGsMode;

                    PAL_SET_ABI_FIELD(pVgtGsMode, mode,    vgtGsMode.bits.MODE);
                    PAL_SET_ABI_FIELD(pVgtGsMode, onchip,  vgtGsMode.bits.ONCHIP);
                    PAL_SET_ABI_FIELD(pVgtGsMode, cutMode, vgtGsMode.bits.CUT_MODE);

                    PAL_SET_ABI_FLAG(pVgtGsMode, esWriteOptimize, vgtGsMode.bits.ES_WRITE_OPTIMIZE);
                    PAL_SET_ABI_FLAG(pVgtGsMode, gsWriteOptimize, vgtGsMode.bits.GS_WRITE_OPTIMIZE);
                }
            }

            // PA_CL_CLIP_CNTL
            {
                PalAbi::PaClClipCntlMetadata* pPaClClipCntl = &pGfxRegisters->paClClipCntl;

                PA_CL_CLIP_CNTL paClClipCntl;
                paClClipCntl.u32All = registers.At(mmPA_CL_CLIP_CNTL);

                PAL_SET_ABI_FLAG(pPaClClipCntl, userClipPlane0Ena,   paClClipCntl.bits.UCP_ENA_0);
                PAL_SET_ABI_FLAG(pPaClClipCntl, userClipPlane1Ena,   paClClipCntl.bits.UCP_ENA_1);
                PAL_SET_ABI_FLAG(pPaClClipCntl, userClipPlane2Ena,   paClClipCntl.bits.UCP_ENA_2);
                PAL_SET_ABI_FLAG(pPaClClipCntl, userClipPlane3Ena,   paClClipCntl.bits.UCP_ENA_3);
                PAL_SET_ABI_FLAG(pPaClClipCntl, userClipPlane4Ena,   paClClipCntl.bits.UCP_ENA_4);
                PAL_SET_ABI_FLAG(pPaClClipCntl, userClipPlane5Ena,   paClClipCntl.bits.UCP_ENA_5);
                PAL_SET_ABI_FLAG(pPaClClipCntl, dxLinearAttrClipEna, paClClipCntl.bits.DX_LINEAR_ATTR_CLIP_ENA);
                PAL_SET_ABI_FLAG(pPaClClipCntl, zclipNearDisable,    paClClipCntl.bits.ZCLIP_NEAR_DISABLE);
                PAL_SET_ABI_FLAG(pPaClClipCntl, zclipFarDisable,     paClClipCntl.bits.ZCLIP_FAR_DISABLE);
                PAL_SET_ABI_FLAG(pPaClClipCntl, rasterizationKill,   paClClipCntl.bits.DX_RASTERIZATION_KILL);
                PAL_SET_ABI_FLAG(pPaClClipCntl, clipDisable,         paClClipCntl.bits.CLIP_DISABLE);
            }

            // PA_CL_VTE_CNTL
            {
                PalAbi::PaClVteCntlMetadata* pPaClVteCntl = &pGfxRegisters->paClVteCntl;

                PA_CL_VTE_CNTL paClVteCntl;
                paClVteCntl.u32All = registers.At(mmPA_CL_VTE_CNTL);

                PAL_SET_ABI_FLAG(pPaClVteCntl, vtxXyFmt,   paClVteCntl.bits.VTX_XY_FMT);
                PAL_SET_ABI_FLAG(pPaClVteCntl, vtxZFmt,    paClVteCntl.bits.VTX_Z_FMT);
                PAL_SET_ABI_FLAG(pPaClVteCntl, xScaleEna,  paClVteCntl.bits.VPORT_X_SCALE_ENA);
                PAL_SET_ABI_FLAG(pPaClVteCntl, xOffsetEna, paClVteCntl.bits.VPORT_X_OFFSET_ENA);
                PAL_SET_ABI_FLAG(pPaClVteCntl, yScaleEna,  paClVteCntl.bits.VPORT_Y_SCALE_ENA);
                PAL_SET_ABI_FLAG(pPaClVteCntl, yOffsetEna, paClVteCntl.bits.VPORT_Y_OFFSET_ENA);
                PAL_SET_ABI_FLAG(pPaClVteCntl, zScaleEna,  paClVteCntl.bits.VPORT_Z_SCALE_ENA);
                PAL_SET_ABI_FLAG(pPaClVteCntl, zOffsetEna, paClVteCntl.bits.VPORT_Z_OFFSET_ENA);
                PAL_SET_ABI_FLAG(pPaClVteCntl, vtxW0Fmt,   paClVteCntl.bits.VTX_W0_FMT);
            }

            // PA_SU_VTX_CNTL
            {
                PalAbi::PaSuVtxCntlMetadata* pPaSuVtxCntl = &pGfxRegisters->paSuVtxCntl;

                PA_SU_VTX_CNTL paSuVtxCntl;
                paSuVtxCntl.u32All = registers.At(mmPA_SU_VTX_CNTL);

                PAL_SET_ABI_FIELD(pPaSuVtxCntl, roundMode, paSuVtxCntl.bits.ROUND_MODE);
                PAL_SET_ABI_FIELD(pPaSuVtxCntl, quantMode, paSuVtxCntl.bits.QUANT_MODE);

                PAL_SET_ABI_FLAG(pPaSuVtxCntl, pixCenter, paSuVtxCntl.bits.PIX_CENTER);
            }

            // PA_SC_MODE_CNTL_1
            {
                PA_SC_MODE_CNTL_1 paScModeCntl1;
                paScModeCntl1.u32All = registers.At(mmPA_SC_MODE_CNTL_1);

                PAL_SET_ABI_FLAG(pGfxRegisters, psIterSample, paScModeCntl1.bits.PS_ITER_SAMPLE);
            }

            // SPI_SHADER_IDX_FORMAT
            {
                SPI_SHADER_IDX_FORMAT spiShaderIdxFormat;
                if (registers.HasEntry(Gfx10Plus::mmSPI_SHADER_IDX_FORMAT, &spiShaderIdxFormat.u32All))
                {
                    PAL_SET_ABI_FIELD(pGfxRegisters, spiShaderIdxFormat, spiShaderIdxFormat.bits.IDX0_EXPORT_FORMAT);
                }
            }

            // SPI_SHADER_COL_FORMAT
            {
                PalAbi::SpiShaderColFormatMetadata* pSpiShaderColFormat = &pGfxRegisters->spiShaderColFormat;

                SPI_SHADER_COL_FORMAT spiShaderColFormat;
                spiShaderColFormat.u32All = registers.At(mmSPI_SHADER_COL_FORMAT);

                PAL_SET_ABI_FIELD(pSpiShaderColFormat, col_0ExportFormat, spiShaderColFormat.bits.COL0_EXPORT_FORMAT);
                PAL_SET_ABI_FIELD(pSpiShaderColFormat, col_1ExportFormat, spiShaderColFormat.bits.COL1_EXPORT_FORMAT);
                PAL_SET_ABI_FIELD(pSpiShaderColFormat, col_2ExportFormat, spiShaderColFormat.bits.COL2_EXPORT_FORMAT);
                PAL_SET_ABI_FIELD(pSpiShaderColFormat, col_3ExportFormat, spiShaderColFormat.bits.COL3_EXPORT_FORMAT);
                PAL_SET_ABI_FIELD(pSpiShaderColFormat, col_4ExportFormat, spiShaderColFormat.bits.COL4_EXPORT_FORMAT);
                PAL_SET_ABI_FIELD(pSpiShaderColFormat, col_5ExportFormat, spiShaderColFormat.bits.COL5_EXPORT_FORMAT);
                PAL_SET_ABI_FIELD(pSpiShaderColFormat, col_6ExportFormat, spiShaderColFormat.bits.COL6_EXPORT_FORMAT);
                PAL_SET_ABI_FIELD(pSpiShaderColFormat, col_7ExportFormat, spiShaderColFormat.bits.COL7_EXPORT_FORMAT);
            }

            // SPI_SHADER_POS_FORMAT
            {
                SPI_SHADER_POS_FORMAT spiShaderPosFormat = {};
                if (registers.HasEntry(mmSPI_SHADER_POS_FORMAT, &spiShaderPosFormat.u32All))
                {
                    PAL_SET_ABI_FIELD_IDX(pGfxRegisters,
                                          spiShaderPosFormat,
                                          0,
                                          spiShaderPosFormat.bits.POS0_EXPORT_FORMAT);
                    PAL_SET_ABI_FIELD_IDX(pGfxRegisters,
                                          spiShaderPosFormat,
                                          1,
                                          spiShaderPosFormat.bits.POS1_EXPORT_FORMAT);
                    PAL_SET_ABI_FIELD_IDX(pGfxRegisters,
                                          spiShaderPosFormat,
                                          2,
                                          spiShaderPosFormat.bits.POS2_EXPORT_FORMAT);
                    PAL_SET_ABI_FIELD_IDX(pGfxRegisters,
                                          spiShaderPosFormat,
                                          3,
                                          spiShaderPosFormat.bits.POS3_EXPORT_FORMAT);

                    if (IsGfx10Plus(palDevice))
                    {
                        PAL_SET_ABI_FIELD_IDX(pGfxRegisters,
                                              spiShaderPosFormat,
                                              4,
                                              spiShaderPosFormat.gfx10Plus.POS4_EXPORT_FORMAT);
                    }
                }
            }

            // SPI_SHADER_Z_FORMAT
            {
                SPI_SHADER_Z_FORMAT spiShaderZFormat;
                spiShaderZFormat.u32All = registers.At(mmSPI_SHADER_Z_FORMAT);

                PAL_SET_ABI_FIELD(pGfxRegisters, spiShaderZFormat, spiShaderZFormat.bits.Z_EXPORT_FORMAT);
            }

            // VGT_GS_ONCHIP_CNTL
            {
                PalAbi::VgtGsOnchipCntlMetadata* pVgtGsOnchipCntl = &pGfxRegisters->vgtGsOnchipCntl;

                VGT_GS_ONCHIP_CNTL vgtGsOnchipCntl;
                if (registers.HasEntry(Gfx09_10::mmVGT_GS_ONCHIP_CNTL, &vgtGsOnchipCntl.u32All))
                {
                    PAL_SET_ABI_FIELD(pVgtGsOnchipCntl, esVertsPerSubgroup,   vgtGsOnchipCntl.bits.ES_VERTS_PER_SUBGRP);
                    PAL_SET_ABI_FIELD(pVgtGsOnchipCntl, gsPrimsPerSubgroup,   vgtGsOnchipCntl.bits.GS_PRIMS_PER_SUBGRP);
                    PAL_SET_ABI_FIELD(pVgtGsOnchipCntl,
                                      gsInstPrimsPerSubgrp,
                                      vgtGsOnchipCntl.bits.GS_INST_PRIMS_IN_SUBGRP);
                }
            }

            // VGT_GS_OUT_PRIM_TYPE
            {
                PalAbi::VgtGsOutPrimTypeMetadata* pVgtGsOutPrimType = &pGfxRegisters->vgtGsOutPrimType;

                bool foundReg = false;
                VGT_GS_OUT_PRIM_TYPE vgtGsOutPrimType;
#if PAL_BUILD_GFX11
                if (IsGfx11(palDevice))
                {
                    foundReg = registers.HasEntry(Gfx11::mmVGT_GS_OUT_PRIM_TYPE, &vgtGsOutPrimType.u32All);
                }
                else
#endif
                {
                    foundReg = registers.HasEntry(Gfx09_10::mmVGT_GS_OUT_PRIM_TYPE, &vgtGsOutPrimType.u32All);
                }

                if (foundReg)
                {
                    if (IsGfx9(palDevice))
                    {
                        // Gfx9 has a different value from the ABI for RectList.
                        // Hack it into the register for the translation logic below.
                        if (vgtGsOutPrimType.bits.OUTPRIM_TYPE == VGT_GS_OUTPRIM_TYPE::RECTLIST__GFX09)
                        {
                            vgtGsOutPrimType.bits.OUTPRIM_TYPE = VGT_GS_OUTPRIM_TYPE::RECTLIST__GFX10PLUS;
                        }
                        if (vgtGsOutPrimType.gfx09_10.OUTPRIM_TYPE_1 == VGT_GS_OUTPRIM_TYPE::RECTLIST__GFX09)
                        {
                            vgtGsOutPrimType.gfx09_10.OUTPRIM_TYPE_1 = VGT_GS_OUTPRIM_TYPE::RECTLIST__GFX10PLUS;
                        }
                        if (vgtGsOutPrimType.gfx09_10.OUTPRIM_TYPE_2 == VGT_GS_OUTPRIM_TYPE::RECTLIST__GFX09)
                        {
                            vgtGsOutPrimType.gfx09_10.OUTPRIM_TYPE_2 = VGT_GS_OUTPRIM_TYPE::RECTLIST__GFX10PLUS;
                        }
                        if (vgtGsOutPrimType.gfx09_10.OUTPRIM_TYPE_3 == VGT_GS_OUTPRIM_TYPE::RECTLIST__GFX09)
                        {
                            vgtGsOutPrimType.gfx09_10.OUTPRIM_TYPE_3 = VGT_GS_OUTPRIM_TYPE::RECTLIST__GFX10PLUS;
                        }
                    }
                    PAL_SET_ABI_FIELD(pVgtGsOutPrimType,
                                      outprimType,
                                      Abi::GsOutPrimType(vgtGsOutPrimType.bits.OUTPRIM_TYPE));

                    if (IsGfx9(palDevice) || IsGfx10(palDevice))
                    {
                        PAL_SET_ABI_FIELD(pVgtGsOutPrimType,
                                          outprimType_1,
                                          Abi::GsOutPrimType(vgtGsOutPrimType.gfx09_10.OUTPRIM_TYPE_1));
                        PAL_SET_ABI_FIELD(pVgtGsOutPrimType,
                                          outprimType_2,
                                          Abi::GsOutPrimType(vgtGsOutPrimType.gfx09_10.OUTPRIM_TYPE_2));
                        PAL_SET_ABI_FIELD(pVgtGsOutPrimType,
                                          outprimType_3,
                                          Abi::GsOutPrimType(vgtGsOutPrimType.gfx09_10.OUTPRIM_TYPE_3));

                        PAL_SET_ABI_FLAG(pVgtGsOutPrimType,
                                         uniqueTypePerStream,
                                         vgtGsOutPrimType.gfx09_10.UNIQUE_TYPE_PER_STREAM);
                    }
                }
            }

            // VGT_REUSE_OFF
            {
                VGT_REUSE_OFF vgtReuseOff;
                vgtReuseOff.u32All = registers.At(mmVGT_REUSE_OFF);

                PAL_SET_ABI_FLAG(pGfxRegisters, vgtReuseOff, vgtReuseOff.bits.REUSE_OFF);
            }

            // SPI_PS_IN_CONTROL
            {
                PalAbi::SpiPsInControlMetadata* pSpiPsInControl = &pGfxRegisters->spiPsInControl;

                SPI_PS_IN_CONTROL spiPsInControl;
                spiPsInControl.u32All = registers.At(mmSPI_PS_IN_CONTROL);

                PAL_SET_ABI_FIELD(pSpiPsInControl, numInterps, spiPsInControl.bits.NUM_INTERP);

                PAL_SET_ABI_FLAG(pSpiPsInControl, paramGen,          spiPsInControl.bits.PARAM_GEN);
                PAL_SET_ABI_FLAG(pSpiPsInControl, offchipParamEn,    spiPsInControl.bits.OFFCHIP_PARAM_EN);
                PAL_SET_ABI_FLAG(pSpiPsInControl, latePcDealloc,     spiPsInControl.bits.LATE_PC_DEALLOC);
                PAL_SET_ABI_FLAG(pSpiPsInControl, bcOptimizeDisable, spiPsInControl.bits.BC_OPTIMIZE_DISABLE);

                auto* pPsHwStage = &pMetadata->pipeline.hardwareStage[uint32(Abi::HardwareStage::Ps)];

                if (IsGfx10Plus(palDevice) && (pPsHwStage->hasEntry.wavefrontSize == 0))
                {
                    PAL_SET_ABI_FIELD(pPsHwStage, wavefrontSize, (spiPsInControl.gfx10Plus.PS_W32_EN) ? 32 : 64);
                }

                if (IsGfx103PlusExclusive(palDevice))
                {
                    PAL_SET_ABI_FIELD(pSpiPsInControl,
                                      numPrimInterp,
                                      spiPsInControl.gfx103PlusExclusive.NUM_PRIM_INTERP);
                }
            }

            // SPI_VS_OUT_CONFIG
            {
                PalAbi::SpiVsOutConfigMetadata* pSpiVsOutConfig = &pGfxRegisters->spiVsOutConfig;

                SPI_VS_OUT_CONFIG spiVsOutConfig = {};
                if (registers.HasEntry(mmSPI_VS_OUT_CONFIG, &spiVsOutConfig.u32All))
                {
                    PAL_SET_ABI_FIELD(pSpiVsOutConfig, vsExportCount, spiVsOutConfig.bits.VS_EXPORT_COUNT);

                    if (IsGfx10Plus(palDevice))
                    {
                        PAL_SET_ABI_FLAG(pSpiVsOutConfig, noPcExport, spiVsOutConfig.gfx10Plus.NO_PC_EXPORT);
                    }

                    if (IsGfx103PlusExclusive(palDevice))
                    {
                        PAL_SET_ABI_FIELD(pSpiVsOutConfig,
                                          primExportCount,
                                          spiVsOutConfig.gfx103PlusExclusive.PRIM_EXPORT_COUNT);
                    }
                }
            }

            // VGT_TF_PARAM
            {
                PalAbi::VgtTfParamMetadata* pVgtTfParam = &pGfxRegisters->vgtTfParam;

                VGT_TF_PARAM vgtTfParam;
                if (registers.HasEntry(mmVGT_TF_PARAM, &vgtTfParam.u32All))
                {
                    PAL_SET_ABI_FIELD(pVgtTfParam, type,              vgtTfParam.bits.TYPE);
                    PAL_SET_ABI_FIELD(pVgtTfParam, partitioning,      vgtTfParam.bits.PARTITIONING);
                    PAL_SET_ABI_FIELD(pVgtTfParam, topology,          vgtTfParam.bits.TOPOLOGY);
                    PAL_SET_ABI_FIELD(pVgtTfParam, distributionMode,  vgtTfParam.bits.DISTRIBUTION_MODE);

                    PAL_SET_ABI_FLAG(pVgtTfParam, disableDonuts, vgtTfParam.bits.DISABLE_DONUTS);

                    if (IsGfx10Plus(palDevice))
                    {
                        PAL_SET_ABI_FIELD(pVgtTfParam, numDsWavesPerSimd, vgtTfParam.gfx10Plus.NUM_DS_WAVES_PER_SIMD);
                    }
                }
            }

            // VGT_LS_HS_CONFIG
            {
                PalAbi::VgtLsHsConfigMetadata* pVgtLsHsConfig = &pGfxRegisters->vgtLsHsConfig;

                VGT_LS_HS_CONFIG vgtLsHsConfig;
                if (registers.HasEntry(mmVGT_LS_HS_CONFIG, &vgtLsHsConfig.u32All))
                {
                    PAL_SET_ABI_FIELD(pVgtLsHsConfig, numPatches,    vgtLsHsConfig.bits.NUM_PATCHES);
                    PAL_SET_ABI_FIELD(pVgtLsHsConfig, hsNumInputCp,  vgtLsHsConfig.bits.HS_NUM_INPUT_CP);
                    PAL_SET_ABI_FIELD(pVgtLsHsConfig, hsNumOutputCp, vgtLsHsConfig.bits.HS_NUM_OUTPUT_CP);
                }
            }

            // SPI_INTERP_CONTROL_0
            {
                PalAbi::SpiInterpControlMetadata* pSpiInterpControl = &pGfxRegisters->spiInterpControl;

                SPI_INTERP_CONTROL_0 spiInterpControl;
                if (registers.HasEntry(mmSPI_INTERP_CONTROL_0, &spiInterpControl.u32All))
                {
                    PAL_SET_ABI_FIELD(pSpiInterpControl,
                                      pointSpriteOverrideX,
                                      Abi::PointSpriteSelect(spiInterpControl.bits.PNT_SPRITE_OVRD_X));
                    PAL_SET_ABI_FIELD(pSpiInterpControl,
                                      pointSpriteOverrideY,
                                      Abi::PointSpriteSelect(spiInterpControl.bits.PNT_SPRITE_OVRD_Y));
                    PAL_SET_ABI_FIELD(pSpiInterpControl,
                                      pointSpriteOverrideZ,
                                      Abi::PointSpriteSelect(spiInterpControl.bits.PNT_SPRITE_OVRD_Z));
                    PAL_SET_ABI_FIELD(pSpiInterpControl,
                                      pointSpriteOverrideW,
                                      Abi::PointSpriteSelect(spiInterpControl.bits.PNT_SPRITE_OVRD_W));

                    PAL_SET_ABI_FLAG(pSpiInterpControl, pointSpriteEna, spiInterpControl.bits.PNT_SPRITE_ENA);
                }
            }

            // VGT_DRAW_PAYLOAD_CNTL
            {
                VGT_DRAW_PAYLOAD_CNTL vgtDrawPayloadCntl;
                if (IsGfx10Plus(palDevice) && registers.HasEntry(mmVGT_DRAW_PAYLOAD_CNTL, &vgtDrawPayloadCntl.u32All))
                {
                    PAL_SET_ABI_FLAG(pGfxRegisters, vgtDrawPrimPayloadEn, vgtDrawPayloadCntl.gfx10Plus.EN_PRIM_PAYLOAD);
                }
            }

            // IA_MULTI_VGT_PARAM
            {
                PalAbi::IaMultiVgtParamMetadata* pIaMultiVgtParam = &pGfxRegisters->iaMultiVgtParam;

                regIA_MULTI_VGT_PARAM iaMultiVgtParam;
                if (registers.HasEntry(Gfx09::mmIA_MULTI_VGT_PARAM, &iaMultiVgtParam.u32All))
                {
                    PAL_SET_ABI_FIELD(pIaMultiVgtParam, primgroupSize, iaMultiVgtParam.bits.PRIMGROUP_SIZE);

                    PAL_SET_ABI_FLAG(pIaMultiVgtParam, partialVsWaveOn, iaMultiVgtParam.bits.PARTIAL_VS_WAVE_ON);
                    PAL_SET_ABI_FLAG(pIaMultiVgtParam, partialEsWaveOn, iaMultiVgtParam.bits.PARTIAL_ES_WAVE_ON);
                    PAL_SET_ABI_FLAG(pIaMultiVgtParam, switchOnEop,     iaMultiVgtParam.bits.SWITCH_ON_EOP);
                    PAL_SET_ABI_FLAG(pIaMultiVgtParam, switchOnEoi,     iaMultiVgtParam.bits.SWITCH_ON_EOI);
                }
            }

            // CB_SHADER_MASK
            {
                PalAbi::CbShaderMaskMetadata* pCbShaderMask = &pGfxRegisters->cbShaderMask;

                CB_SHADER_MASK cbShaderMask;
                cbShaderMask.u32All = registers.At(mmCB_SHADER_MASK);

                PAL_SET_ABI_FIELD(pCbShaderMask, output0Enable, cbShaderMask.bits.OUTPUT0_ENABLE);
                PAL_SET_ABI_FIELD(pCbShaderMask, output1Enable, cbShaderMask.bits.OUTPUT1_ENABLE);
                PAL_SET_ABI_FIELD(pCbShaderMask, output2Enable, cbShaderMask.bits.OUTPUT2_ENABLE);
                PAL_SET_ABI_FIELD(pCbShaderMask, output3Enable, cbShaderMask.bits.OUTPUT3_ENABLE);
                PAL_SET_ABI_FIELD(pCbShaderMask, output4Enable, cbShaderMask.bits.OUTPUT4_ENABLE);
                PAL_SET_ABI_FIELD(pCbShaderMask, output5Enable, cbShaderMask.bits.OUTPUT5_ENABLE);
                PAL_SET_ABI_FIELD(pCbShaderMask, output6Enable, cbShaderMask.bits.OUTPUT6_ENABLE);
                PAL_SET_ABI_FIELD(pCbShaderMask, output7Enable, cbShaderMask.bits.OUTPUT7_ENABLE);
            }

            // SPI_SHADER_PGM_LO_GS
            {
                uint32 value = 0;
                if (registers.HasEntry(mmSPI_SHADER_PGM_LO_GS, &value))
                {
                    // This register is only used for the NGG culling data buffer register.
                    PAL_SET_ABI_FIELD(pGfxRegisters, nggCullingDataReg, mmSPI_SHADER_PGM_LO_GS);
                }
            }

#if PAL_BUILD_GFX11
            // SPI_SHADER_GS_MESHLET_DIM
            {
                PalAbi::SpiShaderGsMeshletDimMetadata* pSpiShaderGsMeshletDim = &pGfxRegisters->spiShaderGsMeshletDim;

                SPI_SHADER_GS_MESHLET_DIM spiShaderGsMeshletDim;
                if (IsGfx11(palDevice) &&
                    registers.HasEntry(Gfx11::mmSPI_SHADER_GS_MESHLET_DIM, &spiShaderGsMeshletDim.u32All))
                {
                    PAL_SET_ABI_FIELD(pSpiShaderGsMeshletDim,
                                      numThreadX,
                                      spiShaderGsMeshletDim.bits.MESHLET_NUM_THREAD_X);
                    PAL_SET_ABI_FIELD(pSpiShaderGsMeshletDim,
                                      numThreadY,
                                      spiShaderGsMeshletDim.bits.MESHLET_NUM_THREAD_Y);
                    PAL_SET_ABI_FIELD(pSpiShaderGsMeshletDim,
                                      numThreadZ,
                                      spiShaderGsMeshletDim.bits.MESHLET_NUM_THREAD_Z);
                    PAL_SET_ABI_FIELD(pSpiShaderGsMeshletDim,
                                      threadgroupSize,
                                      spiShaderGsMeshletDim.bits.MESHLET_THREADGROUP_SIZE);
                }
            }

            // SPI_SHADER_GS_MESHLET_EXP_ALLOC
            {
                PalAbi::SpiShaderGsMeshletExpAllocMetadata* pSpiShaderGsMeshletExpAlloc =
                    &pGfxRegisters->spiShaderGsMeshletExpAlloc;

                SPI_SHADER_GS_MESHLET_EXP_ALLOC spiShaderGsMeshletExpAlloc;
                if (IsGfx11(palDevice) &&
                    registers.HasEntry(Gfx11::mmSPI_SHADER_GS_MESHLET_EXP_ALLOC, &spiShaderGsMeshletExpAlloc.u32All))
                {
                    PAL_SET_ABI_FIELD(pSpiShaderGsMeshletExpAlloc,
                                      maxExpVerts,
                                      spiShaderGsMeshletExpAlloc.bits.MAX_EXP_VERTS);
                    PAL_SET_ABI_FIELD(pSpiShaderGsMeshletExpAlloc,
                                      maxExpPrims,
                                      spiShaderGsMeshletExpAlloc.bits.MAX_EXP_PRIMS);
                }
            }
#endif

            // VGT_GS_INSTANCE_CNT
            {
                PalAbi::VgtGsInstanceCntMetadata* pVgtGsInstanceCnt = &pGfxRegisters->vgtGsInstanceCnt;

                VGT_GS_INSTANCE_CNT vgtGsInstanceCnt;
                if (registers.HasEntry(mmVGT_GS_INSTANCE_CNT, &vgtGsInstanceCnt.u32All))
                {
                    PAL_SET_ABI_FIELD(pVgtGsInstanceCnt, count, vgtGsInstanceCnt.bits.CNT);

                    PAL_SET_ABI_FLAG(pVgtGsInstanceCnt, enable, vgtGsInstanceCnt.bits.ENABLE);

                    if (IsGfx10Plus(palDevice))
                    {
                        PAL_SET_ABI_FLAG(pVgtGsInstanceCnt,
                                         enMaxVertOutPerGsInstance,
                                         vgtGsInstanceCnt.gfx10Plus.EN_MAX_VERT_OUT_PER_GS_INSTANCE);
                    }
                }
            }

            // VGT_GS_PER_VS
            {
                VGT_GS_PER_VS vgtGsPerVs;
                if (hasHwVs && registers.HasEntry(HasHwVs::mmVGT_GS_PER_VS, &vgtGsPerVs.u32All))
                {
                    PAL_SET_ABI_FIELD(pGfxRegisters, vgtGsPerVs, vgtGsPerVs.bits.GS_PER_VS);
                }
            }

            // VGT_GS_VERT_ITEMSIZE .. _3
            {
                VGT_GS_VERT_ITEMSIZE vgtGsVertItemsize;
                if (hasHwVs && registers.HasEntry(HasHwVs::mmVGT_GS_VERT_ITEMSIZE, &vgtGsVertItemsize.u32All))
                {
                    PAL_SET_ABI_FIELD_IDX(pGfxRegisters, vgtGsVertItemsize, 0, vgtGsVertItemsize.bits.ITEMSIZE);
                }
                VGT_GS_VERT_ITEMSIZE_1 vgtGsVertItemsize1;
                if (hasHwVs && registers.HasEntry(HasHwVs::mmVGT_GS_VERT_ITEMSIZE_1, &vgtGsVertItemsize1.u32All))
                {
                    PAL_SET_ABI_FIELD_IDX(pGfxRegisters, vgtGsVertItemsize, 1, vgtGsVertItemsize1.bits.ITEMSIZE);
                }
                VGT_GS_VERT_ITEMSIZE_2 vgtGsVertItemsize2;
                if (hasHwVs && registers.HasEntry(HasHwVs::mmVGT_GS_VERT_ITEMSIZE_2, &vgtGsVertItemsize2.u32All))
                {
                    PAL_SET_ABI_FIELD_IDX(pGfxRegisters, vgtGsVertItemsize, 2, vgtGsVertItemsize2.bits.ITEMSIZE);
                }
                VGT_GS_VERT_ITEMSIZE_3 vgtGsVertItemsize3;
                if (hasHwVs && registers.HasEntry(HasHwVs::mmVGT_GS_VERT_ITEMSIZE_3, &vgtGsVertItemsize3.u32All))
                {
                    PAL_SET_ABI_FIELD_IDX(pGfxRegisters, vgtGsVertItemsize, 3, vgtGsVertItemsize3.bits.ITEMSIZE);
                }
            }

            // VGT_GSVS_RING_ITEMSIZE
            {
                VGT_GSVS_RING_ITEMSIZE vgtGsVsRingItemsize;
                if (hasHwVs && registers.HasEntry(HasHwVs::mmVGT_GSVS_RING_ITEMSIZE, &vgtGsVsRingItemsize.u32All))
                {
                    PAL_SET_ABI_FIELD(pGfxRegisters, vgtGsvsRingItemsize, vgtGsVsRingItemsize.bits.ITEMSIZE);
                }
            }

            // VGT_GSVS_RING_OFFSET_1 .. _3
            {
                VGT_GSVS_RING_OFFSET_1 vgtGsVsRingOffset1;
                if (hasHwVs && registers.HasEntry(HasHwVs::mmVGT_GSVS_RING_OFFSET_1, &vgtGsVsRingOffset1.u32All))
                {
                    PAL_SET_ABI_FIELD_IDX(pGfxRegisters, vgtGsvsRingOffset, 0, vgtGsVsRingOffset1.bits.OFFSET);
                }
                VGT_GSVS_RING_OFFSET_2 vgtGsVsRingOffset2;
                if (hasHwVs && registers.HasEntry(HasHwVs::mmVGT_GSVS_RING_OFFSET_2, &vgtGsVsRingOffset2.u32All))
                {
                    PAL_SET_ABI_FIELD_IDX(pGfxRegisters, vgtGsvsRingOffset, 1, vgtGsVsRingOffset2.bits.OFFSET);
                }
                VGT_GSVS_RING_OFFSET_3 vgtGsVsRingOffset3;
                if (hasHwVs && registers.HasEntry(HasHwVs::mmVGT_GSVS_RING_OFFSET_3, &vgtGsVsRingOffset3.u32All))
                {
                    PAL_SET_ABI_FIELD_IDX(pGfxRegisters, vgtGsvsRingOffset, 2, vgtGsVsRingOffset3.bits.OFFSET);
                }
            }

            // VGT_ESGS_RING_ITEMSIZE
            {
                VGT_ESGS_RING_ITEMSIZE vgtEsGsRingItemsize;
                if (registers.HasEntry(mmVGT_ESGS_RING_ITEMSIZE, &vgtEsGsRingItemsize.u32All))
                {
                    PAL_SET_ABI_FIELD(pGfxRegisters, vgtEsgsRingItemsize, vgtEsGsRingItemsize.bits.ITEMSIZE);
                }
            }

            // VGT_GS_MAX_VERT_OUT
            {
                VGT_GS_MAX_VERT_OUT vgtGsMaxVertOut;
                if (registers.HasEntry(mmVGT_GS_MAX_VERT_OUT, &vgtGsMaxVertOut.u32All))
                {
                    PAL_SET_ABI_FIELD(pGfxRegisters, vgtGsMaxVertOut, vgtGsMaxVertOut.bits.MAX_VERT_OUT);
                }
            }

            // VGT_GS_MAX_PRIMS_PER_SUBGROUP
            {
                VGT_GS_MAX_PRIMS_PER_SUBGROUP vgtGsMaxPrimsPerSubgroup;
                if (IsGfx9(palDevice) &&
                    registers.HasEntry(Gfx09::mmVGT_GS_MAX_PRIMS_PER_SUBGROUP, &vgtGsMaxPrimsPerSubgroup.u32All))
                {
                    PAL_SET_ABI_FIELD(pGfxRegisters,
                                      maxVertsPerSubgroup,
                                      vgtGsMaxPrimsPerSubgroup.bits.MAX_PRIMS_PER_SUBGROUP);
                }
            }

            // GE_MAX_OUTPUT_PER_SUBGROUP
            {
                GE_MAX_OUTPUT_PER_SUBGROUP geMaxOutputPerSubgroup;
                if (IsGfx10Plus(palDevice) &&
                    registers.HasEntry(Gfx10Plus::mmGE_MAX_OUTPUT_PER_SUBGROUP, &geMaxOutputPerSubgroup.u32All))
                {
                    PAL_SET_ABI_FIELD(pGfxRegisters,
                                      maxVertsPerSubgroup,
                                      geMaxOutputPerSubgroup.bits.MAX_VERTS_PER_SUBGROUP);
                }
            }

            // GE_NGG_SUBGRP_CNTL
            {
                PalAbi::GeNggSubgrpCntlMetadata* pGeNggSubgrpCntl = &pGfxRegisters->geNggSubgrpCntl;

                GE_NGG_SUBGRP_CNTL geNggSubgrpCntl;
                if (IsGfx10Plus(palDevice) &&
                    registers.HasEntry(Gfx10Plus::mmGE_NGG_SUBGRP_CNTL, &geNggSubgrpCntl.u32All))
                {
                    PAL_SET_ABI_FIELD(pGeNggSubgrpCntl, primAmpFactor,      geNggSubgrpCntl.bits.PRIM_AMP_FACTOR);
                    PAL_SET_ABI_FIELD(pGeNggSubgrpCntl, threadsPerSubgroup, geNggSubgrpCntl.bits.THDS_PER_SUBGRP);
                }
            }

            // VGT_HOS_MIN_TESS_LEVEL
            {
                VGT_HOS_MIN_TESS_LEVEL vgtHosMinTessLevel;
                if (registers.HasEntry(mmVGT_HOS_MIN_TESS_LEVEL, &vgtHosMinTessLevel.u32All))
                {
                    PAL_SET_ABI_FIELD(pGfxRegisters, vgtHosMinTessLevel, vgtHosMinTessLevel.f32All);
                }
            }

            // VGT_HOS_MAX_TESS_LEVEL
            {
                VGT_HOS_MAX_TESS_LEVEL vgtHosMaxTessLevel;
                if (registers.HasEntry(mmVGT_HOS_MAX_TESS_LEVEL, &vgtHosMaxTessLevel.u32All))
                {
                    PAL_SET_ABI_FIELD(pGfxRegisters, vgtHosMaxTessLevel, vgtHosMaxTessLevel.f32All);
                }
            }

            // VGT_STRMOUT_CONFIG
            {
                PalAbi::VgtStrmoutConfigMetadata* pVgtStrmoutConfig = &pGfxRegisters->vgtStrmoutConfig;

                VGT_STRMOUT_CONFIG vgtStrmoutConfig;
                if (hasHwVs && registers.HasEntry(HasHwVs::mmVGT_STRMOUT_CONFIG, &vgtStrmoutConfig.u32All))
                {
                    PAL_SET_ABI_FIELD(pVgtStrmoutConfig, rastStream,     vgtStrmoutConfig.bits.RAST_STREAM);
                    PAL_SET_ABI_FIELD(pVgtStrmoutConfig, rastStreamMask, vgtStrmoutConfig.bits.RAST_STREAM_MASK);

                    PAL_SET_ABI_FLAG(pVgtStrmoutConfig, streamout_0En,     vgtStrmoutConfig.bits.STREAMOUT_0_EN);
                    PAL_SET_ABI_FLAG(pVgtStrmoutConfig, streamout_1En,     vgtStrmoutConfig.bits.STREAMOUT_1_EN);
                    PAL_SET_ABI_FLAG(pVgtStrmoutConfig, streamout_2En,     vgtStrmoutConfig.bits.STREAMOUT_2_EN);
                    PAL_SET_ABI_FLAG(pVgtStrmoutConfig, streamout_3En,     vgtStrmoutConfig.bits.STREAMOUT_3_EN);
                    PAL_SET_ABI_FLAG(pVgtStrmoutConfig, primsNeededCntEn,  vgtStrmoutConfig.bits.EN_PRIMS_NEEDED_CNT);
                    PAL_SET_ABI_FLAG(pVgtStrmoutConfig, useRastStreamMask, vgtStrmoutConfig.bits.USE_RAST_STREAM_MASK);
                }
            }

            // VGT_STRMOUT_BUFFER_CONFIG
            {
                PalAbi::VgtStrmoutBufferConfigMetadata* pVgtStrmoutBufferConfig =
                    &pGfxRegisters->vgtStrmoutBufferConfig;

                VGT_STRMOUT_BUFFER_CONFIG vgtStrmoutBufferConfig;
                if (hasHwVs && registers.HasEntry(HasHwVs::mmVGT_STRMOUT_BUFFER_CONFIG, &vgtStrmoutBufferConfig.u32All))
                {
                    PAL_SET_ABI_FIELD(pVgtStrmoutBufferConfig,
                                      stream_0BufferEn,
                                      vgtStrmoutBufferConfig.bits.STREAM_0_BUFFER_EN);
                    PAL_SET_ABI_FIELD(pVgtStrmoutBufferConfig,
                                      stream_1BufferEn,
                                      vgtStrmoutBufferConfig.bits.STREAM_1_BUFFER_EN);
                    PAL_SET_ABI_FIELD(pVgtStrmoutBufferConfig,
                                      stream_2BufferEn,
                                      vgtStrmoutBufferConfig.bits.STREAM_2_BUFFER_EN);
                    PAL_SET_ABI_FIELD(pVgtStrmoutBufferConfig,
                                      stream_3BufferEn,
                                      vgtStrmoutBufferConfig.bits.STREAM_3_BUFFER_EN);
                }
            }

            // VGT_STRMOUT_VTX_STRIDE_0
            {
                VGT_STRMOUT_VTX_STRIDE_0 vgtStrmoutVtxStride;
                if (hasHwVs && registers.HasEntry(HasHwVs::mmVGT_STRMOUT_VTX_STRIDE_0, &vgtStrmoutVtxStride.u32All))
                {
                    PAL_SET_ABI_FIELD_IDX(pPipeline, streamoutVertexStrides, 0, vgtStrmoutVtxStride.bits.STRIDE);
                }
            }

            // VGT_STRMOUT_VTX_STRIDE_1
            {
                VGT_STRMOUT_VTX_STRIDE_1 vgtStrmoutVtxStride;
                if (hasHwVs && registers.HasEntry(HasHwVs::mmVGT_STRMOUT_VTX_STRIDE_1, &vgtStrmoutVtxStride.u32All))
                {
                    PAL_SET_ABI_FIELD_IDX(pPipeline, streamoutVertexStrides, 1, vgtStrmoutVtxStride.bits.STRIDE);
                }
            }

            // VGT_STRMOUT_VTX_STRIDE_2
            {
                VGT_STRMOUT_VTX_STRIDE_2 vgtStrmoutVtxStride;
                if (hasHwVs && registers.HasEntry(HasHwVs::mmVGT_STRMOUT_VTX_STRIDE_2, &vgtStrmoutVtxStride.u32All))
                {
                    PAL_SET_ABI_FIELD_IDX(pPipeline, streamoutVertexStrides, 2, vgtStrmoutVtxStride.bits.STRIDE);
                }
            }

            // VGT_STRMOUT_VTX_STRIDE_3
            {
                VGT_STRMOUT_VTX_STRIDE_3 vgtStrmoutVtxStride;
                if (hasHwVs && registers.HasEntry(HasHwVs::mmVGT_STRMOUT_VTX_STRIDE_3, &vgtStrmoutVtxStride.u32All))
                {
                    PAL_SET_ABI_FIELD_IDX(pPipeline, streamoutVertexStrides, 3, vgtStrmoutVtxStride.bits.STRIDE);
                }
            }

            // DB_SHADER_CONTROL
            {
                PalAbi::DbShaderControlMetadata* pDbShaderControl = &pGfxRegisters->dbShaderControl;

                DB_SHADER_CONTROL dbShaderControl;
                dbShaderControl.u32All = registers.At(mmDB_SHADER_CONTROL);

                PAL_SET_ABI_FIELD(pDbShaderControl, zOrder,              dbShaderControl.bits.Z_ORDER);
                PAL_SET_ABI_FIELD(pDbShaderControl, conservativeZExport, dbShaderControl.bits.CONSERVATIVE_Z_EXPORT);

                PAL_SET_ABI_FLAG(pDbShaderControl, zExportEnable, dbShaderControl.bits.Z_EXPORT_ENABLE);
                PAL_SET_ABI_FLAG(pDbShaderControl,
                                 stencilTestValExportEnable,
                                 dbShaderControl.bits.STENCIL_TEST_VAL_EXPORT_ENABLE);
                PAL_SET_ABI_FLAG(pDbShaderControl,
                                 stencilOpValExportEnable,
                                 dbShaderControl.bits.STENCIL_OP_VAL_EXPORT_ENABLE);
                PAL_SET_ABI_FLAG(pDbShaderControl, killEnable,          dbShaderControl.bits.KILL_ENABLE);
                PAL_SET_ABI_FLAG(pDbShaderControl, coverageToMaskEn,    dbShaderControl.bits.COVERAGE_TO_MASK_ENABLE);
                PAL_SET_ABI_FLAG(pDbShaderControl, maskExportEnable,    dbShaderControl.bits.MASK_EXPORT_ENABLE);
                PAL_SET_ABI_FLAG(pDbShaderControl, execOnHierFail,      dbShaderControl.bits.EXEC_ON_HIER_FAIL);
                PAL_SET_ABI_FLAG(pDbShaderControl, execOnNoop,          dbShaderControl.bits.EXEC_ON_NOOP);
                PAL_SET_ABI_FLAG(pDbShaderControl, alphaToMaskDisable,  dbShaderControl.bits.ALPHA_TO_MASK_DISABLE);
                PAL_SET_ABI_FLAG(pDbShaderControl, depthBeforeShader,   dbShaderControl.bits.DEPTH_BEFORE_SHADER);
                PAL_SET_ABI_FLAG(pDbShaderControl,
                                 primitiveOrderedPixelShader,
                                 dbShaderControl.bits.PRIMITIVE_ORDERED_PIXEL_SHADER);

                if (IsGfx10Plus(palDevice))
                {
                    PAL_SET_ABI_FLAG(pDbShaderControl,
                                     preShaderDepthCoverageEnable,
                                     dbShaderControl.gfx10Plus.PRE_SHADER_DEPTH_COVERAGE_ENABLE);
                }
            }

            // SPI_BARYC_CNTL
            {
                PalAbi::SpiBarycCntlMetadata* pSpiBarycCntl = &pGfxRegisters->spiBarycCntl;

                SPI_BARYC_CNTL spiBarycCntl;
                spiBarycCntl.u32All = registers.At(mmSPI_BARYC_CNTL);

                PAL_SET_ABI_FIELD(pSpiBarycCntl, posFloatLocation, spiBarycCntl.bits.POS_FLOAT_LOCATION);

                PAL_SET_ABI_FLAG(pSpiBarycCntl, frontFaceAllBits, spiBarycCntl.bits.FRONT_FACE_ALL_BITS);

            }

            // SPI_PS_INPUT_ADDR
            {
                PalAbi::SpiPsInputAddrMetadata* pSpiPsInput = &pGfxRegisters->spiPsInputAddr;

                SPI_PS_INPUT_ADDR spiPsInputAddr;
                spiPsInputAddr.u32All = registers.At(mmSPI_PS_INPUT_ADDR);

                PAL_SET_ABI_FLAG(pSpiPsInput, perspSampleEna,    spiPsInputAddr.bits.PERSP_SAMPLE_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, perspCenterEna,    spiPsInputAddr.bits.PERSP_CENTER_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, perspCentroidEna,  spiPsInputAddr.bits.PERSP_CENTROID_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, perspPullModelEna, spiPsInputAddr.bits.PERSP_PULL_MODEL_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, linearSampleEna,   spiPsInputAddr.bits.LINEAR_SAMPLE_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, linearCenterEna,   spiPsInputAddr.bits.LINEAR_CENTER_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, linearCentroidEna, spiPsInputAddr.bits.LINEAR_CENTROID_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, lineStippleTexEna, spiPsInputAddr.bits.LINE_STIPPLE_TEX_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, posXFloatEna,      spiPsInputAddr.bits.POS_X_FLOAT_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, posYFloatEna,      spiPsInputAddr.bits.POS_Y_FLOAT_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, posZFloatEna,      spiPsInputAddr.bits.POS_Z_FLOAT_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, posWFloatEna,      spiPsInputAddr.bits.POS_W_FLOAT_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, frontFaceEna,      spiPsInputAddr.bits.FRONT_FACE_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, ancillaryEna,      spiPsInputAddr.bits.ANCILLARY_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, sampleCoverageEna, spiPsInputAddr.bits.SAMPLE_COVERAGE_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, posFixedPtEna,     spiPsInputAddr.bits.POS_FIXED_PT_ENA);
            }

            // SPI_PS_INPUT_ENA
            {
                PalAbi::SpiPsInputEnaMetadata* pSpiPsInput = &pGfxRegisters->spiPsInputEna;

                SPI_PS_INPUT_ENA spiPsInputEna;
                spiPsInputEna.u32All = registers.At(mmSPI_PS_INPUT_ENA);

                PAL_SET_ABI_FLAG(pSpiPsInput, perspSampleEna,    spiPsInputEna.bits.PERSP_SAMPLE_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, perspCenterEna,    spiPsInputEna.bits.PERSP_CENTER_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, perspCentroidEna,  spiPsInputEna.bits.PERSP_CENTROID_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, perspPullModelEna, spiPsInputEna.bits.PERSP_PULL_MODEL_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, linearSampleEna,   spiPsInputEna.bits.LINEAR_SAMPLE_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, linearCenterEna,   spiPsInputEna.bits.LINEAR_CENTER_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, linearCentroidEna, spiPsInputEna.bits.LINEAR_CENTROID_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, lineStippleTexEna, spiPsInputEna.bits.LINE_STIPPLE_TEX_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, posXFloatEna,      spiPsInputEna.bits.POS_X_FLOAT_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, posYFloatEna,      spiPsInputEna.bits.POS_Y_FLOAT_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, posZFloatEna,      spiPsInputEna.bits.POS_Z_FLOAT_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, posWFloatEna,      spiPsInputEna.bits.POS_W_FLOAT_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, frontFaceEna,      spiPsInputEna.bits.FRONT_FACE_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, ancillaryEna,      spiPsInputEna.bits.ANCILLARY_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, sampleCoverageEna, spiPsInputEna.bits.SAMPLE_COVERAGE_ENA);
                PAL_SET_ABI_FLAG(pSpiPsInput, posFixedPtEna,     spiPsInputEna.bits.POS_FIXED_PT_ENA);
            }

            // PA_CL_VS_OUT_CNTL
            {
                PalAbi::PaClVsOutCntlMetadata* pPaClVsOutCntl = &pGfxRegisters->paClVsOutCntl;

                PA_CL_VS_OUT_CNTL paClVsOutCntl = {};
                if (registers.HasEntry(mmPA_CL_VS_OUT_CNTL, &paClVsOutCntl.u32All))
                {
                    PAL_SET_ABI_FLAG(pPaClVsOutCntl, clipDistEna_0,          paClVsOutCntl.bits.CLIP_DIST_ENA_0);
                    PAL_SET_ABI_FLAG(pPaClVsOutCntl, clipDistEna_1,          paClVsOutCntl.bits.CLIP_DIST_ENA_1);
                    PAL_SET_ABI_FLAG(pPaClVsOutCntl, clipDistEna_2,          paClVsOutCntl.bits.CLIP_DIST_ENA_2);
                    PAL_SET_ABI_FLAG(pPaClVsOutCntl, clipDistEna_3,          paClVsOutCntl.bits.CLIP_DIST_ENA_3);
                    PAL_SET_ABI_FLAG(pPaClVsOutCntl, clipDistEna_4,          paClVsOutCntl.bits.CLIP_DIST_ENA_4);
                    PAL_SET_ABI_FLAG(pPaClVsOutCntl, clipDistEna_5,          paClVsOutCntl.bits.CLIP_DIST_ENA_5);
                    PAL_SET_ABI_FLAG(pPaClVsOutCntl, clipDistEna_6,          paClVsOutCntl.bits.CLIP_DIST_ENA_6);
                    PAL_SET_ABI_FLAG(pPaClVsOutCntl, clipDistEna_7,          paClVsOutCntl.bits.CLIP_DIST_ENA_7);
                    PAL_SET_ABI_FLAG(pPaClVsOutCntl, cullDistEna_0,          paClVsOutCntl.bits.CULL_DIST_ENA_0);
                    PAL_SET_ABI_FLAG(pPaClVsOutCntl, cullDistEna_1,          paClVsOutCntl.bits.CULL_DIST_ENA_1);
                    PAL_SET_ABI_FLAG(pPaClVsOutCntl, cullDistEna_2,          paClVsOutCntl.bits.CULL_DIST_ENA_2);
                    PAL_SET_ABI_FLAG(pPaClVsOutCntl, cullDistEna_3,          paClVsOutCntl.bits.CULL_DIST_ENA_3);
                    PAL_SET_ABI_FLAG(pPaClVsOutCntl, cullDistEna_4,          paClVsOutCntl.bits.CULL_DIST_ENA_4);
                    PAL_SET_ABI_FLAG(pPaClVsOutCntl, cullDistEna_5,          paClVsOutCntl.bits.CULL_DIST_ENA_5);
                    PAL_SET_ABI_FLAG(pPaClVsOutCntl, cullDistEna_6,          paClVsOutCntl.bits.CULL_DIST_ENA_6);
                    PAL_SET_ABI_FLAG(pPaClVsOutCntl, cullDistEna_7,          paClVsOutCntl.bits.CULL_DIST_ENA_7);
                    PAL_SET_ABI_FLAG(pPaClVsOutCntl, useVtxPointSize,        paClVsOutCntl.bits.USE_VTX_POINT_SIZE);
                    PAL_SET_ABI_FLAG(pPaClVsOutCntl, useVtxEdgeFlag,         paClVsOutCntl.bits.USE_VTX_EDGE_FLAG);
                    PAL_SET_ABI_FLAG(pPaClVsOutCntl, useVtxRenderTargetIndx,
                                     paClVsOutCntl.bits.USE_VTX_RENDER_TARGET_INDX);
                    PAL_SET_ABI_FLAG(pPaClVsOutCntl, useVtxViewportIndx,     paClVsOutCntl.bits.USE_VTX_VIEWPORT_INDX);
                    PAL_SET_ABI_FLAG(pPaClVsOutCntl, useVtxKillFlag,         paClVsOutCntl.bits.USE_VTX_KILL_FLAG);
                    PAL_SET_ABI_FLAG(pPaClVsOutCntl, vsOutMiscVecEna,        paClVsOutCntl.bits.VS_OUT_MISC_VEC_ENA);
                    PAL_SET_ABI_FLAG(pPaClVsOutCntl, vsOutCcDist0VecEna,     paClVsOutCntl.bits.VS_OUT_CCDIST0_VEC_ENA);
                    PAL_SET_ABI_FLAG(pPaClVsOutCntl, vsOutCcDist1VecEna,     paClVsOutCntl.bits.VS_OUT_CCDIST1_VEC_ENA);
                    PAL_SET_ABI_FLAG(pPaClVsOutCntl, vsOutMiscSideBusEna,
                                     paClVsOutCntl.bits.VS_OUT_MISC_SIDE_BUS_ENA);

                    if (IsGfx9(palDevice))
                    {
                        PAL_SET_ABI_FLAG(pPaClVsOutCntl, useVtxLineWidth, paClVsOutCntl.gfx09.USE_VTX_LINE_WIDTH);
                    }

                    if (IsGfx9(palDevice) || IsGfx10(palDevice))
                    {
                        PAL_SET_ABI_FLAG(pPaClVsOutCntl, useVtxGsCutFlag, paClVsOutCntl.gfx09_10.USE_VTX_GS_CUT_FLAG);
                    }

                    if (IsGfx10Plus(palDevice))
                    {
                        PAL_SET_ABI_FLAG(pPaClVsOutCntl, useVtxLineWidth, paClVsOutCntl.gfx10Plus.USE_VTX_LINE_WIDTH);
                    }

                    if (IsGfx103Plus(palDevice))
                    {
                        PAL_SET_ABI_FLAG(pPaClVsOutCntl, useVtxVrsRate, paClVsOutCntl.gfx103Plus.USE_VTX_VRS_RATE);
                        PAL_SET_ABI_FLAG(pPaClVsOutCntl,
                                         bypassVtxRateCombiner,
                                         paClVsOutCntl.gfx103Plus.BYPASS_VTX_RATE_COMBINER);
                        PAL_SET_ABI_FLAG(pPaClVsOutCntl,
                                         bypassPrimRateCombiner,
                                         paClVsOutCntl.gfx103Plus.BYPASS_PRIM_RATE_COMBINER);
                    }

#if PAL_BUILD_GFX11
                    if (IsGfx11(palDevice)
                        )
                    {
                        PAL_SET_ABI_FLAG(pPaClVsOutCntl, useVtxFsrSelect, paClVsOutCntl.gfx110.USE_VTX_FSR_SELECT);
                    }
#endif
                }
            }

            // VGT_PRIMITIVEID_EN
            {
                VGT_PRIMITIVEID_EN vgtPrimitiveIdEn;
                vgtPrimitiveIdEn.u32All = registers.At(mmVGT_PRIMITIVEID_EN);

                PAL_SET_ABI_FLAG(pGfxRegisters, vgtPrimitiveIdEn,      vgtPrimitiveIdEn.bits.PRIMITIVEID_EN);
                PAL_SET_ABI_FLAG(pGfxRegisters, nggDisableProvokReuse, vgtPrimitiveIdEn.bits.NGG_DISABLE_PROVOK_REUSE);
            }

            // PA_SC_SHADER_CONTROL
            {
                PalAbi::PaScShaderControlMetadata* pPaScShaderControl = &pGfxRegisters->paScShaderControl;

                PA_SC_SHADER_CONTROL paScShaderControl;
                paScShaderControl.u32All = registers.At(mmPA_SC_SHADER_CONTROL);

                {
                    PAL_SET_ABI_FLAG(pPaScShaderControl,
                                     loadCollisionWaveid,
                                     paScShaderControl.core.LOAD_COLLISION_WAVEID);
                    PAL_SET_ABI_FLAG(pPaScShaderControl,
                                     loadIntrawaveCollision,
                                     paScShaderControl.core.LOAD_INTRAWAVE_COLLISION);
                }

                if (IsGfx10Plus(palDevice))
                {
                    PAL_SET_ABI_FIELD(pPaScShaderControl,
                                      waveBreakRegionSize,
                                      paScShaderControl.gfx10Plus.WAVE_BREAK_REGION_SIZE);
                }
            }

            // PA_SC_AA_CONFIG
            {
                PA_SC_AA_CONFIG paScAaConfig;
                paScAaConfig.u32All = registers.At(mmPA_SC_AA_CONFIG);

                static_assert((uint32(Abi::CoverageToShaderSel::InputCoverage)      == INPUT_COVERAGE)       &&
                              (uint32(Abi::CoverageToShaderSel::InputInnerCoverage) == INPUT_INNER_COVERAGE) &&
                              (uint32(Abi::CoverageToShaderSel::InputDepthCoverage) == INPUT_DEPTH_COVERAGE),
                              "ABI and HW enum values do not match!");

                PAL_SET_ABI_FIELD(pGfxRegisters,
                                  aaCoverageToShaderSelect,
                                  Abi::CoverageToShaderSel(paScAaConfig.bits.COVERAGE_TO_SHADER_SELECT));
            }

            // SPI_PS_INPUT_CNTL[0..32]
            {
                uint32 numInterpolants = 0;
                for (uint32 i = 0; i < MaxPsInputSemantics; i++)
                {
                    PalAbi::SpiPsInputCntlMetadata* pSpiPsInputCntl = &pGfxRegisters->spiPsInputCntl[i];

                    SPI_PS_INPUT_CNTL_0 spiPsInputCntl;
                    if (registers.HasEntry(mmSPI_PS_INPUT_CNTL_0 + i, &spiPsInputCntl.u32All))
                    {
                        ++numInterpolants;

                        PAL_SET_ABI_FIELD(pSpiPsInputCntl, offset,     spiPsInputCntl.bits.OFFSET);
                        PAL_SET_ABI_FIELD(pSpiPsInputCntl, defaultVal, spiPsInputCntl.bits.DEFAULT_VAL);

                        PAL_SET_ABI_FLAG(pSpiPsInputCntl, flatShade,      spiPsInputCntl.bits.FLAT_SHADE);
                        PAL_SET_ABI_FLAG(pSpiPsInputCntl, ptSpriteTex,    spiPsInputCntl.bits.PT_SPRITE_TEX);
                        PAL_SET_ABI_FLAG(pSpiPsInputCntl, fp16InterpMode, spiPsInputCntl.bits.FP16_INTERP_MODE);
                        PAL_SET_ABI_FLAG(pSpiPsInputCntl, attr0Valid,     spiPsInputCntl.bits.ATTR0_VALID);
                        PAL_SET_ABI_FLAG(pSpiPsInputCntl, attr1Valid,     spiPsInputCntl.bits.ATTR1_VALID);

                        if (IsGfx9(palDevice) || IsGfx10(palDevice))
                        {
                            PAL_SET_ABI_FIELD(pSpiPsInputCntl, cylWrap, spiPsInputCntl.gfx09_10.CYL_WRAP);
                        }

                        if (IsGfx103PlusExclusive(palDevice))
                        {
                            PAL_SET_ABI_FLAG(pSpiPsInputCntl,
                                             rotatePcPtr,
                                             spiPsInputCntl.gfx103PlusExclusive.ROTATE_PC_PTR);
                        }

#if PAL_BUILD_GFX11
                        if (IsGfx11(palDevice))
                        {
                            PAL_SET_ABI_FLAG(pSpiPsInputCntl, primAttr, spiPsInputCntl.gfx11.PRIM_ATTR);
                        }
#endif
                    }
                    else
                    {
                        break;
                    }
                }

                // Some compilers don't provide numInterpolants, so lets just set it directly.
                PAL_SET_ABI_FIELD(pPipeline, numInterpolants, numInterpolants);
            }

            // HwHs - Rsrc1..3 and Checksum
            {
                PalAbi::HardwareStageMetadata* pHwHs = &pPipeline->hardwareStage[uint32(Abi::HardwareStage::Hs)];

                SPI_SHADER_PGM_RSRC1_HS rsrc1;
                if (registers.HasEntry(mmSPI_SHADER_PGM_RSRC1_HS, &rsrc1.u32All))
                {
                    const bool isWave32 = pHwHs->hasEntry.wavefrontSize && (pHwHs->wavefrontSize == 32);
                    const uint32 allocVgprs = (rsrc1.bits.VGPRS + 1) * ((isWave32) ? 8 : 4);
                    const uint32 calcVgprs  = (pHwHs->vgprCount == 0)
                                              ? 0
                                              : ((pHwHs->vgprCount - 1) / ((isWave32) ? 8 : 4));

                    PAL_ASSERT(pHwHs->hasEntry.vgprCount        &&
                               (pHwHs->vgprCount <= allocVgprs) &&
                               (rsrc1.bits.VGPRS == calcVgprs));

                    const uint32 calcSgprs  = (pHwHs->sgprCount == 0) ? 0 : ((pHwHs->sgprCount - 1) / 8);
                    PAL_ASSERT(pHwHs->hasEntry.sgprCount && (rsrc1.bits.SGPRS == calcSgprs));

                    PAL_SET_ABI_FIELD(pHwHs, floatMode, rsrc1.bits.FLOAT_MODE);

                    PAL_SET_ABI_FLAG(pHwHs, fp16Overflow, rsrc1.bits.FP16_OVFL);
                    PAL_SET_ABI_FLAG(pHwHs, ieeeMode,     rsrc1.bits.IEEE_MODE);
                    PAL_SET_ABI_FLAG(pHwHs, debugMode,    rsrc1.bits.DEBUG_MODE);

                    if (IsGfx10Plus(palDevice))
                    {
                        PAL_SET_ABI_FLAG(pHwHs, wgpMode,         rsrc1.gfx10Plus.WGP_MODE);
                        PAL_SET_ABI_FLAG(pHwHs, memOrdered,      rsrc1.gfx10Plus.MEM_ORDERED);
                        PAL_SET_ABI_FLAG(pHwHs, forwardProgress, rsrc1.gfx10Plus.FWD_PROGRESS);
                    }

                    PAL_SET_ABI_FIELD(pGfxRegisters, lsVgprCompCnt, rsrc1.bits.LS_VGPR_COMP_CNT);
                }

                SPI_SHADER_PGM_RSRC2_HS rsrc2;
                if (registers.HasEntry(mmSPI_SHADER_PGM_RSRC2_HS, &rsrc2.u32All))
                {
                    uint32 regLdsSize = 0;

                    uint32 userSgprCount = rsrc2.bits.USER_SGPR;
                    if (IsGfx9(palDevice))
                    {
                        regLdsSize     = rsrc2.gfx09.LDS_SIZE;
                        userSgprCount |= (rsrc2.gfx09.USER_SGPR_MSB << 5);

                        PAL_SET_ABI_FIELD(pHwHs, excpEn, rsrc2.gfx09.EXCP_EN);
                    }
                    if (IsGfx10Plus(palDevice))
                    {
                        regLdsSize     = rsrc2.gfx10Plus.LDS_SIZE;
                        userSgprCount |= (rsrc2.gfx10Plus.USER_SGPR_MSB << 5);

                        PAL_SET_ABI_FIELD(pHwHs, excpEn,        rsrc2.gfx10Plus.EXCP_EN);
                        PAL_SET_ABI_FIELD(pHwHs, sharedVgprCnt, rsrc2.gfx10Plus.SHARED_VGPR_CNT);

                        PAL_SET_ABI_FLAG(pHwHs,         offchipLdsEn, rsrc2.gfx10Plus.OC_LDS_EN);
                        PAL_SET_ABI_FLAG(pGfxRegisters, hsTgSizeEn,   rsrc2.gfx10Plus.TG_SIZE_EN);
                    }

                    PAL_SET_ABI_FIELD(pHwHs, userSgprs, userSgprCount);

                    PAL_SET_ABI_FLAG(pHwHs, scratchEn,   rsrc2.bits.SCRATCH_EN);
                    PAL_SET_ABI_FLAG(pHwHs, trapPresent, rsrc2.bits.TRAP_PRESENT);

                    const uint32 allocLdsSize = regLdsSize * Gfx9LdsDwGranularity * sizeof(uint32);
                    if (pHwHs->hasEntry.ldsSize)
                    {
                        const uint32 calcLdsSize = Pow2Align(pHwHs->ldsSize / uint32(sizeof(uint32)),
                                                             Gfx9LdsDwGranularity) >> Gfx9LdsDwGranularityShift;
                        PAL_DPF(DbgPrintCatInfoMsg, DbgPrintStyleDefault,
                            "LDS_SIZE: 0x%0x, Calc: 0x%08x, Alloc: 0x%08x",
                            regLdsSize,
                            calcLdsSize,
                            allocLdsSize);
                        PAL_ASSERT(pHwHs->ldsSize <= allocLdsSize);
                        PAL_ASSERT(regLdsSize == calcLdsSize);
                    }

                    // We forcibly overwrite this because some compilers don't put the proper LDS size
                    // into the metadata, if it is there at all.
                    PAL_SET_ABI_FIELD(pHwHs, ldsSize, allocLdsSize);
                }

                SPI_SHADER_PGM_RSRC3_HS rsrc3;
                if (registers.HasEntry(mmSPI_SHADER_PGM_RSRC3_HS, &rsrc3.u32All))
                {
                    if (IsGfx10Plus(palDevice))
                    {
                        PAL_SET_ABI_FIELD(pHwHs, wavesPerSe, rsrc3.bits.WAVE_LIMIT);
                    }
                    else
                    {
                        const uint32 numSaPerSe = chipProps.gfx9.numShaderArrays;
                        PAL_SET_ABI_FIELD(pHwHs, wavesPerSe, rsrc3.bits.WAVE_LIMIT * numSaPerSe);
                    }
                }

                SPI_SHADER_PGM_CHKSUM_HS chksum;
                if (supportSpp && registers.HasEntry(Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_HS, &chksum.u32All))
                {
                    PAL_SET_ABI_FIELD(pHwHs, checksumValue, chksum.bits.CHECKSUM);
                }
            }

            // HwGs - Rsrc1..3 and Checksum
            {
                PalAbi::HardwareStageMetadata* pHwGs = &pPipeline->hardwareStage[uint32(Abi::HardwareStage::Gs)];

                SPI_SHADER_PGM_RSRC1_GS rsrc1;
                if (registers.HasEntry(mmSPI_SHADER_PGM_RSRC1_GS, &rsrc1.u32All))
                {
                    const bool isWave32 = pHwGs->hasEntry.wavefrontSize && (pHwGs->wavefrontSize == 32);
                    const uint32 allocVgprs = (rsrc1.bits.VGPRS + 1) * ((isWave32) ? 8 : 4);
                    const uint32 calcVgprs  = (pHwGs->vgprCount == 0)
                                              ? 0
                                              : ((pHwGs->vgprCount - 1) / ((isWave32) ? 8 : 4));

                    PAL_ASSERT(pHwGs->hasEntry.vgprCount        &&
                               (pHwGs->vgprCount <= allocVgprs) &&
                               (rsrc1.bits.VGPRS == calcVgprs));

                    const uint32 calcSgprs  = (pHwGs->sgprCount == 0) ? 0 : ((pHwGs->sgprCount - 1) / 8);
                    PAL_ASSERT(pHwGs->hasEntry.sgprCount && (rsrc1.bits.SGPRS == calcSgprs));

                    PAL_SET_ABI_FIELD(pHwGs, floatMode, rsrc1.bits.FLOAT_MODE);

                    PAL_SET_ABI_FLAG(pHwGs, fp16Overflow, rsrc1.bits.FP16_OVFL);
                    PAL_SET_ABI_FLAG(pHwGs, ieeeMode,     rsrc1.bits.IEEE_MODE);
                    PAL_SET_ABI_FLAG(pHwGs, debugMode,    rsrc1.bits.DEBUG_MODE);

                    if (IsGfx10Plus(palDevice))
                    {
                        PAL_SET_ABI_FLAG(pHwGs, wgpMode,         rsrc1.gfx10Plus.WGP_MODE);
                        PAL_SET_ABI_FLAG(pHwGs, memOrdered,      rsrc1.gfx10Plus.MEM_ORDERED);
                        PAL_SET_ABI_FLAG(pHwGs, forwardProgress, rsrc1.gfx10Plus.FWD_PROGRESS);
                    }

                    PAL_SET_ABI_FIELD(pGfxRegisters, gsVgprCompCnt, rsrc1.bits.GS_VGPR_COMP_CNT);
                }

                SPI_SHADER_PGM_RSRC2_GS rsrc2;
                if (registers.HasEntry(mmSPI_SHADER_PGM_RSRC2_GS, &rsrc2.u32All))
                {
                    uint32 userSgprCount = rsrc2.bits.USER_SGPR;
                    if (IsGfx9(palDevice))
                    {
                        userSgprCount |= (rsrc2.gfx09.USER_SGPR_MSB << 5);
                    }
                    if (IsGfx10Plus(palDevice))
                    {
                        userSgprCount |= (rsrc2.gfx10Plus.USER_SGPR_MSB << 5);

                        PAL_SET_ABI_FIELD(pHwGs, sharedVgprCnt, rsrc2.gfx10Plus.SHARED_VGPR_CNT);
                    }

                    PAL_SET_ABI_FIELD(pGfxRegisters, esVgprCompCnt, rsrc2.bits.ES_VGPR_COMP_CNT);

                    PAL_SET_ABI_FIELD(pHwGs, userSgprs, userSgprCount);
                    PAL_SET_ABI_FIELD(pHwGs, excpEn,    rsrc2.bits.EXCP_EN);

                    PAL_SET_ABI_FLAG(pHwGs, scratchEn,    rsrc2.bits.SCRATCH_EN);
                    PAL_SET_ABI_FLAG(pHwGs, trapPresent,  rsrc2.bits.TRAP_PRESENT);
                    PAL_SET_ABI_FLAG(pHwGs, offchipLdsEn, rsrc2.bits.OC_LDS_EN);

                    const uint32 allocLdsSize = rsrc2.bits.LDS_SIZE * Gfx9LdsDwGranularity * sizeof(uint32);
                    if (pHwGs->hasEntry.ldsSize)
                    {
                        const uint32 calcLdsSize = Pow2Align(pHwGs->ldsSize / uint32(sizeof(uint32)),
                                                             Gfx9LdsDwGranularity) >> Gfx9LdsDwGranularityShift;
                        PAL_ASSERT(pHwGs->ldsSize <= allocLdsSize);
                        PAL_ASSERT(rsrc2.bits.LDS_SIZE == calcLdsSize);
                    }

                    // We forcibly overwrite this because some compilers don't put the proper LDS size
                    // into the metadata, if it is there at all.
                    PAL_SET_ABI_FIELD(pHwGs, ldsSize, allocLdsSize);
                }

                SPI_SHADER_PGM_RSRC3_GS rsrc3;
                if (registers.HasEntry(mmSPI_SHADER_PGM_RSRC3_GS, &rsrc3.u32All))
                {
                    if (IsGfx10Plus(palDevice))
                    {
                        PAL_SET_ABI_FIELD(pHwGs, wavesPerSe, rsrc3.bits.WAVE_LIMIT);
                    }
                    else
                    {
                        const uint32 numSaPerSe = chipProps.gfx9.numShaderArrays;
                        PAL_SET_ABI_FIELD(pHwGs, wavesPerSe, rsrc3.bits.WAVE_LIMIT * numSaPerSe);
                    }
                }

                SPI_SHADER_PGM_CHKSUM_GS chksum;
                if (supportSpp && registers.HasEntry(Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_GS, &chksum.u32All))
                {
                    PAL_SET_ABI_FIELD(pHwGs, checksumValue, chksum.bits.CHECKSUM);
                }
            }

            // HwVs - Rsrc1..3 and Checksum
            {
                PalAbi::HardwareStageMetadata* pHwVs = &pPipeline->hardwareStage[uint32(Abi::HardwareStage::Vs)];

                SPI_SHADER_PGM_RSRC1_VS rsrc1;
                if (hasHwVs && registers.HasEntry(HasHwVs::mmSPI_SHADER_PGM_RSRC1_VS, &rsrc1.u32All))
                {
                    const bool isWave32 = pHwVs->hasEntry.wavefrontSize && (pHwVs->wavefrontSize == 32);
                    const uint32 allocVgprs = (rsrc1.bits.VGPRS + 1) * ((isWave32) ? 8 : 4);
                    const uint32 calcVgprs  = (pHwVs->vgprCount == 0)
                                              ? 0
                                              : ((pHwVs->vgprCount - 1) / ((isWave32) ? 8 : 4));

                    PAL_ASSERT(pHwVs->hasEntry.vgprCount        &&
                               (pHwVs->vgprCount <= allocVgprs) &&
                               (rsrc1.bits.VGPRS == calcVgprs));

                    const uint32 calcSgprs  = (pHwVs->sgprCount == 0) ? 0 : ((pHwVs->sgprCount - 1) / 8);
                    PAL_ASSERT(pHwVs->hasEntry.sgprCount && (rsrc1.bits.SGPRS == calcSgprs));

                    PAL_SET_ABI_FIELD(pHwVs, floatMode, rsrc1.bits.FLOAT_MODE);

                    PAL_SET_ABI_FLAG(pHwVs, fp16Overflow, rsrc1.bits.FP16_OVFL);
                    PAL_SET_ABI_FLAG(pHwVs, ieeeMode,     rsrc1.bits.IEEE_MODE);
                    PAL_SET_ABI_FLAG(pHwVs, debugMode,    rsrc1.bits.DEBUG_MODE);

                    if (IsGfx10(palDevice))
                    {
                        PAL_SET_ABI_FLAG(pHwVs, memOrdered,      rsrc1.gfx10.MEM_ORDERED);
                        PAL_SET_ABI_FLAG(pHwVs, forwardProgress, rsrc1.gfx10.FWD_PROGRESS);
                    }

                    PAL_SET_ABI_FIELD(pGfxRegisters, vsVgprCompCnt, rsrc1.bits.VGPR_COMP_CNT);
                }

                SPI_SHADER_PGM_RSRC2_VS rsrc2;
                if (hasHwVs && registers.HasEntry(HasHwVs::mmSPI_SHADER_PGM_RSRC2_VS, &rsrc2.u32All))
                {
                    uint32 userSgprCount = rsrc2.bits.USER_SGPR;
                    if (IsGfx9(palDevice))
                    {
                        userSgprCount |= (rsrc2.gfx09.USER_SGPR_MSB << 5);
                    }
                    if (IsGfx10(palDevice))
                    {
                        userSgprCount |= (rsrc2.gfx10.USER_SGPR_MSB << 5);

                        PAL_SET_ABI_FIELD(pHwVs, sharedVgprCnt, rsrc2.gfx10.SHARED_VGPR_CNT);
                    }

                    PAL_SET_ABI_FIELD(pHwVs, userSgprs, userSgprCount);
                    PAL_SET_ABI_FIELD(pHwVs, excpEn,    rsrc2.bits.EXCP_EN);

                    PAL_SET_ABI_FLAG(pHwVs, scratchEn,    rsrc2.bits.SCRATCH_EN);
                    PAL_SET_ABI_FLAG(pHwVs, trapPresent,  rsrc2.bits.TRAP_PRESENT);
                    PAL_SET_ABI_FLAG(pHwVs, offchipLdsEn, rsrc2.bits.OC_LDS_EN);

                    PAL_SET_ABI_FLAG(pGfxRegisters, vsSoBase0En,   rsrc2.bits.SO_BASE0_EN);
                    PAL_SET_ABI_FLAG(pGfxRegisters, vsSoBase1En,   rsrc2.bits.SO_BASE1_EN);
                    PAL_SET_ABI_FLAG(pGfxRegisters, vsSoBase2En,   rsrc2.bits.SO_BASE2_EN);
                    PAL_SET_ABI_FLAG(pGfxRegisters, vsSoBase3En,   rsrc2.bits.SO_BASE3_EN);
                    PAL_SET_ABI_FLAG(pGfxRegisters, vsStreamoutEn, rsrc2.bits.SO_EN);
                    PAL_SET_ABI_FLAG(pGfxRegisters, vsPcBaseEn,    rsrc2.bits.PC_BASE_EN);
                }

                SPI_SHADER_PGM_RSRC3_VS rsrc3;
                if (hasHwVs && registers.HasEntry(HasHwVs::mmSPI_SHADER_PGM_RSRC3_VS, &rsrc3.u32All))
                {
                    if (IsGfx10(palDevice))
                    {
                        PAL_SET_ABI_FIELD(pHwVs, wavesPerSe, rsrc3.bits.WAVE_LIMIT);
                    }
                    else
                    {
                        const uint32 numSaPerSe = chipProps.gfx9.numShaderArrays;
                        PAL_SET_ABI_FIELD(pHwVs, wavesPerSe, rsrc3.bits.WAVE_LIMIT * numSaPerSe);
                    }
                }

                SPI_SHADER_PGM_CHKSUM_VS chksum;
                if (supportSpp         &&
                    IsGfx10(palDevice) &&
                    registers.HasEntry(Gfx10::mmSPI_SHADER_PGM_CHKSUM_VS, &chksum.u32All))
                {
                    PAL_SET_ABI_FIELD(pHwVs, checksumValue, chksum.most.CHECKSUM);
                }
            }

            // HwPs - Rsrc1..3 and Checksum
            {
                PalAbi::HardwareStageMetadata* pHwPs = &pPipeline->hardwareStage[uint32(Abi::HardwareStage::Ps)];

                SPI_SHADER_PGM_RSRC1_PS rsrc1;
                if (registers.HasEntry(mmSPI_SHADER_PGM_RSRC1_PS, &rsrc1.u32All))
                {
                    const bool isWave32 = pHwPs->hasEntry.wavefrontSize && (pHwPs->wavefrontSize == 32);
                    const uint32 allocVgprs = (rsrc1.bits.VGPRS + 1) * ((isWave32) ? 8 : 4);
                    const uint32 calcVgprs  = (pHwPs->vgprCount == 0)
                                              ? 0
                                              : ((pHwPs->vgprCount - 1) / ((isWave32) ? 8 : 4));

                    PAL_ASSERT(pHwPs->hasEntry.vgprCount        &&
                               (pHwPs->vgprCount <= allocVgprs) &&
                               (rsrc1.bits.VGPRS == calcVgprs));

                    const uint32 calcSgprs  = (pHwPs->sgprCount == 0) ? 0 : ((pHwPs->sgprCount - 1) / 8);
                    PAL_ASSERT(pHwPs->hasEntry.sgprCount && (rsrc1.bits.SGPRS == calcSgprs));

                    PAL_SET_ABI_FIELD(pHwPs, floatMode, rsrc1.bits.FLOAT_MODE);

                    PAL_SET_ABI_FLAG(pHwPs, fp16Overflow, rsrc1.bits.FP16_OVFL);
                    PAL_SET_ABI_FLAG(pHwPs, ieeeMode,     rsrc1.bits.IEEE_MODE);
                    PAL_SET_ABI_FLAG(pHwPs, debugMode,    rsrc1.bits.DEBUG_MODE);

                    if (IsGfx10Plus(palDevice))
                    {
                        PAL_SET_ABI_FLAG(pHwPs, memOrdered,      rsrc1.gfx10Plus.MEM_ORDERED);
                        PAL_SET_ABI_FLAG(pHwPs, forwardProgress, rsrc1.gfx10Plus.FWD_PROGRESS);
                    }

                    if (IsGfx103PlusExclusive(palDevice))
                    {
                        PAL_SET_ABI_FLAG(pGfxRegisters,
                                         psLoadProvokingVtx,
                                         rsrc1.gfx103PlusExclusive.LOAD_PROVOKING_VTX);
                    }
                }

                SPI_SHADER_PGM_RSRC2_PS rsrc2;
                if (registers.HasEntry(mmSPI_SHADER_PGM_RSRC2_PS, &rsrc2.u32All))
                {
                    uint32 userSgprCount = rsrc2.bits.USER_SGPR;
                    if (IsGfx9(palDevice))
                    {
                        userSgprCount |= (rsrc2.gfx09.USER_SGPR_MSB << 5);
                    }
                    if (IsGfx10Plus(palDevice))
                    {
                        userSgprCount |= (rsrc2.gfx10Plus.USER_SGPR_MSB << 5);

                        PAL_SET_ABI_FIELD(pHwPs, sharedVgprCnt, rsrc2.gfx10Plus.SHARED_VGPR_CNT);
                    }

                    PAL_SET_ABI_FIELD(pHwPs, userSgprs, userSgprCount);
                    PAL_SET_ABI_FIELD(pHwPs, excpEn,    rsrc2.bits.EXCP_EN);

                    PAL_SET_ABI_FLAG(pHwPs, scratchEn,    rsrc2.bits.SCRATCH_EN);
                    PAL_SET_ABI_FLAG(pHwPs, trapPresent,  rsrc2.bits.TRAP_PRESENT);

#if PAL_BUILD_GFX11
                    const uint32 psExtraLdsDwGranularity = (IsGfx11(palDevice)) ? Gfx11PsExtraLdsDwGranularity :
                                                                                  Gfx9PsExtraLdsDwGranularity;
#else
                    const uint32 psExtraLdsDwGranularity = Gfx9PsExtraLdsDwGranularity;
#endif

                    PAL_SET_ABI_FIELD(pGfxRegisters,
                                      psExtraLdsSize,
                                      rsrc2.bits.EXTRA_LDS_SIZE * psExtraLdsDwGranularity * sizeof(uint32));

                    PAL_SET_ABI_FLAG(pGfxRegisters, psWaveCntEn, rsrc2.bits.WAVE_CNT_EN);

                    // These two bits are duplicated in RSRC2 and PA_SC_SHADER_CONTROL.
                    // We'll write them only if they don't already exist.
                    PalAbi::PaScShaderControlMetadata* pPaScShaderControl = &pGfxRegisters->paScShaderControl;
                    if (pPaScShaderControl->hasEntry.loadCollisionWaveid == 0)
                    {
                        PAL_SET_ABI_FLAG(pPaScShaderControl, loadCollisionWaveid, rsrc2.bits.LOAD_COLLISION_WAVEID);
                    }
                    if (pPaScShaderControl->hasEntry.loadIntrawaveCollision == 0)
                    {
                        PAL_SET_ABI_FLAG(pPaScShaderControl,
                                         loadIntrawaveCollision,
                                         rsrc2.bits.LOAD_INTRAWAVE_COLLISION);
                    }
                }

                SPI_SHADER_PGM_RSRC3_PS rsrc3;
                if (registers.HasEntry(mmSPI_SHADER_PGM_RSRC3_PS, &rsrc3.u32All))
                {
                    if (IsGfx10Plus(palDevice))
                    {
                        PAL_SET_ABI_FIELD(pHwPs, wavesPerSe, rsrc3.bits.WAVE_LIMIT);
                    }
                    else
                    {
                        const uint32 numSaPerSe = chipProps.gfx9.numShaderArrays;
                        PAL_SET_ABI_FIELD(pHwPs, wavesPerSe, rsrc3.bits.WAVE_LIMIT * numSaPerSe);
                    }
                }

                SPI_SHADER_PGM_CHKSUM_PS chksum;
                if (supportSpp && registers.HasEntry(Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_PS, &chksum.u32All))
                {
                    PAL_SET_ABI_FIELD(pHwPs, checksumValue, chksum.bits.CHECKSUM);
                }
            }
        }

        // Compute pipelines (or hybrid graphics pipelines) will have a COMPUTE_PGM_RSRC1
        if (registers.HasEntry(mmCOMPUTE_PGM_RSRC1))
        {
            PalAbi::HardwareStageMetadata* pHwCs = &pPipeline->hardwareStage[uint32(Abi::HardwareStage::Cs)];

            // COMPUTE_NUM_THREAD_X
            {
                COMPUTE_NUM_THREAD_X computeNumThreadX;
                if (registers.HasEntry(mmCOMPUTE_NUM_THREAD_X, &computeNumThreadX.u32All))
                {
                    PAL_ASSERT((pHwCs->hasEntry.threadgroupDimensions == 0) ||
                               (pHwCs->threadgroupDimensions[0] == computeNumThreadX.bits.NUM_THREAD_FULL));

                    PAL_SET_ABI_FIELD_IDX(pHwCs, threadgroupDimensions, 0, computeNumThreadX.bits.NUM_THREAD_FULL);
                }
            }

            // COMPUTE_NUM_THREAD_Y
            {
                COMPUTE_NUM_THREAD_Y computeNumThreadY;
                if (registers.HasEntry(mmCOMPUTE_NUM_THREAD_Y, &computeNumThreadY.u32All))
                {
                    PAL_ASSERT((pHwCs->hasEntry.threadgroupDimensions == 0) ||
                               (pHwCs->threadgroupDimensions[1] == computeNumThreadY.bits.NUM_THREAD_FULL));

                    PAL_SET_ABI_FIELD_IDX(pHwCs, threadgroupDimensions, 1, computeNumThreadY.bits.NUM_THREAD_FULL);
                }
            }

            // COMPUTE_NUM_THREAD_Z
            {
                COMPUTE_NUM_THREAD_Z computeNumThreadZ;
                if (registers.HasEntry(mmCOMPUTE_NUM_THREAD_Z, &computeNumThreadZ.u32All))
                {
                    PAL_ASSERT((pHwCs->hasEntry.threadgroupDimensions == 0) ||
                               (pHwCs->threadgroupDimensions[2] == computeNumThreadZ.bits.NUM_THREAD_FULL));

                    PAL_SET_ABI_FIELD_IDX(pHwCs, threadgroupDimensions, 2, computeNumThreadZ.bits.NUM_THREAD_FULL);
                }
            }

            // COMPUTE_RESOURCE_LIMITS
            {
                COMPUTE_RESOURCE_LIMITS computeResourceLimits;
                if (registers.HasEntry(mmCOMPUTE_RESOURCE_LIMITS, &computeResourceLimits.u32All))
                {
                    if (IsGfx10Plus(palDevice))
                    {
                        PAL_SET_ABI_FIELD(pHwCs, wavesPerSe, computeResourceLimits.bits.WAVES_PER_SH);
                    }
                    else
                    {
                        const uint32 numSaPerSe = chipProps.gfx9.numShaderArrays;
                        PAL_SET_ABI_FIELD(pHwCs, wavesPerSe, computeResourceLimits.bits.WAVES_PER_SH * numSaPerSe);
                    }
                }
            }

            // HwCs - Rsrc1..3 and Checksum
            {
                COMPUTE_PGM_RSRC1 rsrc1;
                if (registers.HasEntry(mmCOMPUTE_PGM_RSRC1, &rsrc1.u32All))
                {
                    const bool isWave32 = pHwCs->hasEntry.wavefrontSize && (pHwCs->wavefrontSize == 32);
                    const uint32 allocVgprs = (rsrc1.bits.VGPRS + 1) * ((isWave32) ? 8 : 4);
                    const uint32 calcVgprs  = (pHwCs->vgprCount == 0)
                                              ? 0
                                              : ((pHwCs->vgprCount - 1) / ((isWave32) ? 8 : 4));

                    PAL_ASSERT((pHwCs->hasEntry.vgprCount == 0) ||
                               (pHwCs->hasEntry.vgprCount       &&
                               (pHwCs->vgprCount <= allocVgprs) &&
                               (rsrc1.bits.VGPRS == calcVgprs)));

                    const uint32 calcSgprs  = (pHwCs->sgprCount == 0) ? 0 : ((pHwCs->sgprCount - 1) / 8);
                    PAL_ASSERT((pHwCs->hasEntry.sgprCount == 0) ||
                               (pHwCs->hasEntry.sgprCount && (rsrc1.bits.SGPRS == calcSgprs)));

                    // only hit here via ShaderLibrary so far
                    if ((pHwCs->hasEntry.vgprCount == 0) && (rsrc1.bits.VGPRS != 0))
                    {
                        PAL_SET_ABI_FIELD(pHwCs, vgprCount, allocVgprs);
                    }

                    if ((pHwCs->hasEntry.sgprCount == 0) && (rsrc1.bits.SGPRS != 0))
                    {
                        const uint32 allocSgprs = (rsrc1.bits.SGPRS + 1) * 8;
                        PAL_SET_ABI_FIELD(pHwCs, sgprCount, allocSgprs);
                    }

                    PAL_SET_ABI_FIELD(pHwCs, floatMode, rsrc1.bits.FLOAT_MODE);

                    PAL_SET_ABI_FLAG(pHwCs, fp16Overflow, rsrc1.bits.FP16_OVFL);
                    PAL_SET_ABI_FLAG(pHwCs, ieeeMode,     rsrc1.bits.IEEE_MODE);
                    PAL_SET_ABI_FLAG(pHwCs, debugMode,    rsrc1.bits.DEBUG_MODE);

                    if (IsGfx10Plus(palDevice))
                    {
                        PAL_SET_ABI_FLAG(pHwCs, wgpMode,         rsrc1.gfx10Plus.WGP_MODE);
                        PAL_SET_ABI_FLAG(pHwCs, memOrdered,      rsrc1.gfx10Plus.MEM_ORDERED);
                        PAL_SET_ABI_FLAG(pHwCs, forwardProgress, rsrc1.gfx10Plus.FWD_PROGRESS);
                    }
                }

                COMPUTE_PGM_RSRC2 rsrc2;
                if (registers.HasEntry(mmCOMPUTE_PGM_RSRC2, &rsrc2.u32All))
                {
                    PAL_SET_ABI_FIELD(pHwCs, userSgprs, rsrc2.bits.USER_SGPR);

                    const uint32 excpEn = rsrc2.bits.EXCP_EN |
                                          (rsrc2.bits.EXCP_EN_MSB << CountSetBits(COMPUTE_PGM_RSRC2__EXCP_EN_MASK));
                    PAL_SET_ABI_FIELD(pHwCs, excpEn, excpEn);

                    PAL_SET_ABI_FLAG(pHwCs, scratchEn,   rsrc2.bits.SCRATCH_EN);
                    PAL_SET_ABI_FLAG(pHwCs, trapPresent, rsrc2.bits.TRAP_PRESENT);

                    PAL_SET_ABI_FIELD(pComputeRegisters, tidigCompCnt, rsrc2.bits.TIDIG_COMP_CNT);

                    PAL_SET_ABI_FLAG(pComputeRegisters, tgidXEn,  rsrc2.bits.TGID_X_EN);
                    PAL_SET_ABI_FLAG(pComputeRegisters, tgidYEn,  rsrc2.bits.TGID_Y_EN);
                    PAL_SET_ABI_FLAG(pComputeRegisters, tgidZEn,  rsrc2.bits.TGID_Z_EN);
                    PAL_SET_ABI_FLAG(pComputeRegisters, tgSizeEn, rsrc2.bits.TG_SIZE_EN);

                    const uint32 allocLdsSize = rsrc2.bits.LDS_SIZE * Gfx9LdsDwGranularity * sizeof(uint32);
                    if (pHwCs->hasEntry.ldsSize)
                    {
                        const uint32 calcLdsSize = Pow2Align(pHwCs->ldsSize / uint32(sizeof(uint32)),
                                                             Gfx9LdsDwGranularity) >> Gfx9LdsDwGranularityShift;
                        PAL_ASSERT(pHwCs->ldsSize <= allocLdsSize);
                        PAL_ASSERT(rsrc2.bits.LDS_SIZE == calcLdsSize);
                    }

                    // We forcibly overwrite this because some compilers don't put the proper LDS size
                    // into the metadata, if it is there at all.
                    PAL_SET_ABI_FIELD(pHwCs, ldsSize, allocLdsSize);
                }

                COMPUTE_PGM_RSRC3 rsrc3;
                if (registers.HasEntry(Gfx10Plus::mmCOMPUTE_PGM_RSRC3, &rsrc3.u32All))
                {
                    if (IsGfx10Plus(palDevice))
                    {
                        PAL_SET_ABI_FIELD(pHwCs, sharedVgprCnt, rsrc3.bits.SHARED_VGPR_CNT * 8);
                    }
                }

                COMPUTE_SHADER_CHKSUM chksum;
                if (supportSpp)
                {
                    // We appear to write this register into the ELF actually based on the ASIC support for it.
                    if (IsGfx9(palDevice) && registers.HasEntry(Gfx09_1x::mmCOMPUTE_SHADER_CHKSUM, &chksum.u32All))
                    {
                        PAL_SET_ABI_FIELD(pHwCs, checksumValue, chksum.bits.CHECKSUM);
                    }
                    else if (IsGfx10Plus(palDevice) &&
                             registers.HasEntry(Gfx10Plus::mmCOMPUTE_SHADER_CHKSUM, &chksum.u32All))
                    {
                        PAL_SET_ABI_FIELD(pHwCs, checksumValue, chksum.bits.CHECKSUM);
                    }
                }
            }
        }

        // Setup the userDataRegMap
        {
            for (uint32 hwStageIdx = 0; hwStageIdx < uint32(HwShaderStage::Last); hwStageIdx++)
            {
                const HwShaderStage hwStage       = HwShaderStage(hwStageIdx);
                const uint32        abiHwStageIdx = uint32(PalToAbiHwShaderStage[hwStageIdx]);

                PalAbi::HardwareStageMetadata* pHwStage = &pMetadata->pipeline.hardwareStage[abiHwStageIdx];

                // If we already have a mapping here, don't bother going over them.
                if (pHwStage->hasEntry.userDataRegMap == 0)
                {
                    const uint16 baseRegAddr = pDevice->GetBaseUserDataReg(hwStage);
                    const uint16 lastRegAddr = baseRegAddr + ((hwStage != HwShaderStage::Cs) ? 32 : 16);

                    for (uint16 offset = baseRegAddr; offset < lastRegAddr; ++offset)
                    {
                        uint32 value = 0;
                        if (registers.HasEntry(offset, &value))
                        {
                            PAL_SET_ABI_FIELD_IDX(pHwStage, userDataRegMap, offset - baseRegAddr, value);
                        }
                        else
                        {
                            PAL_SET_ABI_FIELD_IDX(pHwStage,
                                                  userDataRegMap,
                                                  offset - baseRegAddr,
                                                  uint32(Abi::UserDataMapping::NotMapped));
                        }
                    }
                }
            }

            // For compute, just mark the extra 16 entries as not mapped.
            PalAbi::HardwareStageMetadata* pHwCs = &pMetadata->pipeline.hardwareStage[uint32(Abi::HardwareStage::Cs)];
            for (uint32 userDataReg = 16; userDataReg < 32; userDataReg++)
            {
                pHwCs->userDataRegMap[userDataReg] = uint32(Abi::UserDataMapping::NotMapped);
            }
        }
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
    PAL_ASSERT(createInfo.pPipelineBinary != nullptr);
    PAL_ASSERT(pPlacementAddr != nullptr);

    AbiReader abiReader(GetPlatform(), createInfo.pPipelineBinary);
    Result result = abiReader.Init();

    if (result == Result::Success)
    {
        MsgPackReader metadataReader;
        PalAbi::CodeObjectMetadata metadata = {};

        const uint8 abi = abiReader.GetOsAbi();
        if (abi == Abi::ElfOsAbiAmdgpuPal)
        {
            result = abiReader.GetMetadata(&metadataReader, &metadata);

            if (result == Result::Success)
            {
                result = ConvertAbiRegistersToMetadata(this, &metadata, &metadataReader);
            }
        }

        auto* pPipeline = PAL_PLACEMENT_NEW(pPlacementAddr) ComputePipeline(this, isInternal);
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
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetShaderLibrarySize(
    const ShaderLibraryCreateInfo& createInfo,
    Result*                        pResult
    ) const
{
    if (pResult != nullptr)
    {
        (*pResult) = Result::Success;
    }

    return sizeof(ShaderLibrary);
}

// =====================================================================================================================
Result Device::CreateShaderLibrary(
    const ShaderLibraryCreateInfo&  createInfo,
    void*                           pPlacementAddr,
    bool                            isInternal,
    IShaderLibrary**                ppPipeline)
{
    PAL_ASSERT(createInfo.pCodeObject != nullptr);
    PAL_ASSERT(pPlacementAddr != nullptr);
    AbiReader abiReader(GetPlatform(), createInfo.pCodeObject);
    Result result = abiReader.Init();

    MsgPackReader              metadataReader;
    PalAbi::CodeObjectMetadata metadata;

    if (result == Result::Success)
    {
        result = abiReader.GetMetadata(&metadataReader, &metadata);
    }

    if (result == Result::Success)
    {
        result = ConvertAbiRegistersToMetadata(this, &metadata, &metadataReader);
    }

    auto* pShaderLib = PAL_PLACEMENT_NEW(pPlacementAddr) ShaderLibrary(this);

    if (result == Result::Success)
    {
        result = pShaderLib->Initialize(createInfo, abiReader, metadata, &metadataReader);
    }

    if (result != Result::Success)
    {
        pShaderLib->Destroy();
        pShaderLib = nullptr;
    }

    *ppPipeline = pShaderLib;

    return result;
}

// =====================================================================================================================
size_t Device::GetGraphicsPipelineSize(
    const GraphicsPipelineCreateInfo& createInfo,
    bool                              isInternal,
    Result*                           pResult
    ) const
{
    const size_t pipelineSize = Max(sizeof(GraphicsPipeline), sizeof(HybridGraphicsPipeline));

    if (pResult != nullptr)
    {
        (*pResult) = Result::Success;
    }

    return pipelineSize;
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

    MsgPackReader metadataReader;
    PalAbi::CodeObjectMetadata metadata = {};

    if (result == Result::Success)
    {
        result = abiReader.GetMetadata(&metadataReader, &metadata);
    }

    if (result == Result::Success)
    {
        result = ConvertAbiRegistersToMetadata(this, &metadata, &metadataReader);
    }

    if (result == Result::Success)
    {
        const auto& shaderMetadata = metadata.pipeline.shader[static_cast<uint32>(Abi::ApiShaderType::Task)];
        if (ShaderHashIsNonzero({ shaderMetadata.apiShaderHash[0], shaderMetadata.apiShaderHash[1] }))
        {
            PAL_PLACEMENT_NEW(pPlacementAddr) HybridGraphicsPipeline(this);
        }
        else
        {
            PAL_PLACEMENT_NEW(pPlacementAddr) GraphicsPipeline(this, isInternal);
        }
    }

    if (result == Result::Success)
    {
        auto* pPipeline = static_cast<GraphicsPipeline*>(pPlacementAddr);
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
bool Device::DetermineHwStereoRenderingSupported(
    const GraphicPipelineViewInstancingInfo& viewInstancingInfo
    ) const
{
    bool hwStereoRenderingSupported = false;

    if ((viewInstancingInfo.pViewInstancingDesc != nullptr) &&
        (viewInstancingInfo.pViewInstancingDesc->enableMasking == false) &&
        (viewInstancingInfo.pViewInstancingDesc->viewInstanceCount == 2))
    {
        if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
        {
            hwStereoRenderingSupported = IsVega12(*Parent()) || IsVega20(*Parent());

            if (hwStereoRenderingSupported)
            {
                // The bits number of RT_SLICE_OFFSET in PA_STEREO_CNTL.
                constexpr uint32 RightEyeSliceOffsetBits = 2;

                if (viewInstancingInfo.shaderUseViewId)
                {
                    // TODO: Hardware can also supports the case that view id is only used by VS/GS/DS to export
                    // x cooridnate of position, but this requires SC changes to add semantic for view id and
                    // export second position in sp3 codes.
                    hwStereoRenderingSupported = false;
                }
                if (viewInstancingInfo.pViewInstancingDesc->viewportArrayIdx[0] != 0)
                {
                    hwStereoRenderingSupported = false;
                }
                else if (viewInstancingInfo.pViewInstancingDesc->renderTargetArrayIdx[0] != 0)
                {
                    hwStereoRenderingSupported = false;
                }
                else if (viewInstancingInfo.pViewInstancingDesc->renderTargetArrayIdx[1] >=
                         (1 << RightEyeSliceOffsetBits))
                {
                    hwStereoRenderingSupported = false;
                }
            }
        }
        else
        {
            hwStereoRenderingSupported = true;

            // The bits number of RT_SLICE in GE_STEREO_CNTL
            constexpr uint32 LeftEyeSliceIdBits      = 3;

            // The bits number of RT_SLICE_OFFSET in PA_STEREO_CNTL.
            constexpr uint32 RightEyeSliceOffsetBits = 4;

            if (viewInstancingInfo.shaderUseViewId)
            {
                // TODO: Hardware can also supports the case that view id is only used by VS/GS/DS to export
                // position, but this requires SC changes to add semantic for view id and
                // export second position in sp3 codes.
                hwStereoRenderingSupported = false;
            }
            else if (viewInstancingInfo.pViewInstancingDesc->viewportArrayIdx[0] >
                     viewInstancingInfo.pViewInstancingDesc->viewportArrayIdx[1])
            {
                hwStereoRenderingSupported = false;
            }
            else if (viewInstancingInfo.pViewInstancingDesc->renderTargetArrayIdx[0] >=
                     (1 << LeftEyeSliceIdBits))
            {
                hwStereoRenderingSupported = false;
            }
            else if (viewInstancingInfo.pViewInstancingDesc->renderTargetArrayIdx[0] >
                     viewInstancingInfo.pViewInstancingDesc->renderTargetArrayIdx[1])
            {
                hwStereoRenderingSupported = false;
            }
            else if ((viewInstancingInfo.pViewInstancingDesc->renderTargetArrayIdx[1] -
                      viewInstancingInfo.pViewInstancingDesc->renderTargetArrayIdx[0]) >=
                     (1 << RightEyeSliceOffsetBits))
            {
                hwStereoRenderingSupported = false;
            }
            else if ((viewInstancingInfo.gsExportViewportArrayIndex != 0) &&
                     (viewInstancingInfo.pViewInstancingDesc->viewportArrayIdx[0] != 0))
            {
                hwStereoRenderingSupported = false;
            }
            else if ((viewInstancingInfo.gsExportRendertargetArrayIndex != 0) &&
                     (viewInstancingInfo.pViewInstancingDesc->renderTargetArrayIdx[0] != 0))
            {
                hwStereoRenderingSupported = false;
            }
        }
    }

    return hwStereoRenderingSupported;
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
// Creates a concrete Gfx9 GfxImage object
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
    else if (IsGfx10Plus(m_gfxIpLevel) && (createInfo.queueType == QueueTypeDma))
    {
        cmdBufferSize = DmaCmdBuffer::GetSize(*this);
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
    else if ((createInfo.queueType == QueueTypeDma) && IsGfx10Plus(m_gfxIpLevel))
    {
        result = Result::Success;

        // As of GFX10, DMA operations have become part of the graphics engine...
        *ppCmdBuffer = PAL_PLACEMENT_NEW(pPlacementAddr) DmaCmdBuffer(*this, createInfo);
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

    size_t  viewSize = sizeof(Gfx9ColorTargetView);

    if (IsGfx10(m_gfxIpLevel))
    {
        viewSize = sizeof(Gfx10ColorTargetView);
    }
#if PAL_BUILD_GFX11
    else if (IsGfx11(m_gfxIpLevel))
    {
        viewSize = sizeof(Gfx11ColorTargetView);
    }
#endif

    return viewSize;
}

// =====================================================================================================================
// Creates a Gfx9 implementation of Pal::IColorTargetView
Result Device::CreateColorTargetView(
    const ColorTargetViewCreateInfo&  createInfo,
    ColorTargetViewInternalCreateInfo internalInfo,
    void*                             pPlacementAddr,
    IColorTargetView**                ppColorTargetView
    ) const
{
    const uint32 viewId = m_nextColorTargetViewId++;

    if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
    {
        (*ppColorTargetView) =
            PAL_PLACEMENT_NEW(pPlacementAddr) Gfx9ColorTargetView(this, createInfo, internalInfo, viewId);
    }
    else if (IsGfx10(m_gfxIpLevel))
    {
        (*ppColorTargetView) =
            PAL_PLACEMENT_NEW(pPlacementAddr) Gfx10ColorTargetView(this, createInfo, internalInfo, viewId);
    }
#if PAL_BUILD_GFX11
    else if (IsGfx11(m_gfxIpLevel))
    {
        (*ppColorTargetView) =
            PAL_PLACEMENT_NEW(pPlacementAddr) Gfx11ColorTargetView(this, createInfo, internalInfo, viewId);
    }
#endif

    return Result::Success;
}

// =====================================================================================================================
size_t Device::GetDepthStencilViewSize(
    Result* pResult
    ) const
{
    if (pResult != nullptr)
    {
        (*pResult) = Result::Success;
    }

    size_t  viewSize = sizeof(Gfx9DepthStencilView);

    if (IsGfx10Plus(m_gfxIpLevel))
    {
        viewSize = sizeof(Gfx10DepthStencilView);
    }

    return viewSize;
}

// =====================================================================================================================
// Creates a Gfx9 implementation of Pal::IDepthStencilView
Result Device::CreateDepthStencilView(
    const DepthStencilViewCreateInfo&         createInfo,
    const DepthStencilViewInternalCreateInfo& internalInfo,
    void*                                     pPlacementAddr,
    IDepthStencilView**                       ppDepthStencilView
    ) const
{
    const uint32 viewId = m_nextDepthStencilViewId++;

    if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
    {
        (*ppDepthStencilView) =
            PAL_PLACEMENT_NEW(pPlacementAddr) Gfx9DepthStencilView(this, createInfo, internalInfo, viewId);
    }
    else if (IsGfx10Plus(m_gfxIpLevel))
    {
        (*ppDepthStencilView) =
            PAL_PLACEMENT_NEW(pPlacementAddr) Gfx10DepthStencilView(this, createInfo, internalInfo, viewId);
    }
    else
    {
        PAL_ALERT_ALWAYS();
    }

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
bool Device::SupportsIterate256() const
{
    // ITERATE_256 is only supported on Gfx10 products.
    return IsGfx10(m_gfxIpLevel)                       &&
        // Emulation cannot suport iterate256 = 0 since the frame buffer is really just system memory where the
        // page size is unknown.
        (GetPlatform()->IsEmulationEnabled() == false) &&
        // In cases where our VRAM bus width is not a power of two, we need to have iterate256 enabled at all times
        IsPowerOfTwo(m_pParent->MemoryProperties().vramBusBitWidth);
}

// =====================================================================================================================
Result Device::CreateCmdUploadRingInternal(
    const CmdUploadRingCreateInfo& createInfo,
    Pal::CmdUploadRing**           ppCmdUploadRing)
{
    return CmdUploadRing::CreateInternal(createInfo, this, ppCmdUploadRing);
}

// =====================================================================================================================
// Calculates the value of a buffer SRD's NUM_RECORDS field.
uint32 Device::CalcNumRecords(
    size_t      sizeInBytes,
    uint32      stride)
{
    // According to the regspec, the units for NUM_RECORDS are:
    //    Bytes if:  const_stride == 0 ||  or const_swizzle_enable == false
    //    Otherwise,  in units of "stride".
    //
    // According to the SQ team, the units for NUM_RECORDS are instead:
    //    Bytes if: Shader instruction doesn't include a structured buffer
    //    Otherwise, in units of "stride".
    //
    //    We can simplify NUM_RECORDS to actually be:
    //    Bytes if: Buffer SRD is for raw buffer access (which we define as Undefined format and Stride of 1).
    //    Otherwise, in units of "stride".
    // Which can be simplified to divide by stride if the stride is greater than 1
    uint32 numRecords = static_cast<uint32>(sizeInBytes);

    if (stride > 1)
    {
        numRecords /= stride;
    }

    return numRecords;
}

// =====================================================================================================================
// Fills in the AddrLib create input fields based on chip specific properties. Note: at this point during init, settings
// have only been partially intialized. Only settings and member variables that are not impacted by validation or
// the client driver may be used.
Result Device::InitAddrLibCreateInput(
    ADDR_CREATE_FLAGS*   pCreateFlags, // [out] Creation Flags
    ADDR_REGISTER_VALUE* pRegValue     // [out] Register Value
    ) const
{
    const Gfx9PalSettings& settings = GetGfx9Settings(*Parent());
    if (settings.addrLibGbAddrConfigOverride == 0)
    {
        pRegValue->gbAddrConfig = m_pParent->ChipProperties().gfx9.gbAddrConfig;
    }
    else
    {
        pRegValue->gbAddrConfig = settings.addrLibGbAddrConfigOverride;
    }

    pCreateFlags->nonPower2MemConfig = (IsPowerOfTwo(m_pParent->MemoryProperties().vramBusBitWidth) == false);

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
    const auto&       palDevice         = *Parent();
    DccFormatEncoding dccFormatEncoding = DccFormatEncoding::Optimal;

    if (viewFormatCount == AllCompatibleFormats)
    {
#if PAL_BUILD_GFX11
        const auto& palSettings = palDevice.Settings();

        if (IsGfx11(palDevice) && TestAnyFlagSet(palSettings.useDcc, UseDcc::UseDccForAllCompatibleFormats))
        {
            // Allow DCC for "all compatible formats" on GFX11 products only.
            dccFormatEncoding = DccFormatEncoding::Optimal;
        }
        else
#endif
        {
            // If all compatible formats are allowed as view formats then the image is not DCC compatible as none of
            // the format compatibility classes comprise only of formats that are DCC compatible.
            dccFormatEncoding = DccFormatEncoding::Incompatible;
        }
    }
#if PAL_BUILD_GFX11
    // If this is not a GFX11 or if we have disabled DCC format replacement, then we need to check if the various
    // formats in use by this surface work with DCC compression.
    else if ((IsGfx11(palDevice) == false) || (Settings().gfx11AlwaysAllowDccFormatReplacement == false))
#else
    else
#endif
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
        const bool baseFormatIsSigned = Formats::IsSnorm(swizzledFormat.format)   ||
                                        Formats::IsSint(swizzledFormat.format)    ||
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
            const bool viewFormatIsSigned = Formats::IsSnorm(pFormats[i].format)   ||
                                            Formats::IsSint(pFormats[i].format)    ||
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
    constexpr SQ_TEX_CLAMP PalTexAddrToHwTbl[] =
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
    Pal::TexFilter texfilter)
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
    const SamplerInfo& info)
{
    SQ_TEX_ANISO_RATIO anisoRatio = SQ_TEX_ANISO_RATIO_1;

    if (IsAnisoEnabled(info.filter))
    {
        if (info.maxAnisotropy < 2)
        {
            // Nothing to do.
        }
        else if (info.maxAnisotropy < 4)
        {
            anisoRatio = SQ_TEX_ANISO_RATIO_2;
        }
        else if (info.maxAnisotropy < 8)
        {
            anisoRatio = SQ_TEX_ANISO_RATIO_4;
        }
        else if (info.maxAnisotropy < 16)
        {
            anisoRatio = SQ_TEX_ANISO_RATIO_8;
        }
        else if (info.maxAnisotropy == 16)
        {
            anisoRatio = SQ_TEX_ANISO_RATIO_16;
        }
    }

    return anisoRatio;
}

// =====================================================================================================================
// Gfx9+ helper function for patching a pipeline's shader internal SRD table.
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

        const gpusize patchedGpuVa = (GetBaseAddress(&srd) + dataGpuVirtAddr);
        SetBaseAddress(&srd, patchedGpuVa);

        // Note: The entire unpatched SRD table has already been copied to GPU memory wholesale.  We just need to
        // modify the first quadword of the SRD to patch the addresses.
        memcpy((pDstSrd + i), &srd, sizeof(uint64));
    }
}

// =====================================================================================================================
// Helper function for calculating an SRD's "llc_noalloc" field (last level cache, aka the mall).
static uint32 CalcLlcNoalloc(
    uint32  bypassOnRead,
    uint32  bypassOnWrite)
{
    //    0 : use the LLC for read/write if enabled in Mtype (see specified GpuMemMallPolicy for underlying alloc).
    //    1 : use the LLC for read, bypass for write / atomics (write / atomics probe - invalidate)
    //    2 : use the LLC for write / atomics, bypass for read
    //    3 : bypass the LLC for all ops
    return (bypassOnRead << 1) | bypassOnWrite;
}

// =====================================================================================================================
// Gfx9 specific function for creating typed buffer view SRDs. Installed in the function pointer table of the parent
// device during initialization.
void PAL_STDCALL Device::Gfx9CreateTypedBufferViewSrds(
    const IDevice*        pDevice,
    uint32                count,
    const BufferViewInfo* pBufferViewInfo,
    void*                 pOut)
{
    PAL_ASSERT((pDevice != nullptr) && (pOut != nullptr) && (pBufferViewInfo != nullptr) && (count > 0));
    const auto*const pGfxDevice = static_cast<const Device*>(static_cast<const Pal::Device*>(pDevice)->GetGfxDevice());
    const auto*const pFmtInfo   = MergedChannelFmtInfoTbl(pGfxDevice->Parent()->ChipProperties().gfxLevel,
                                                          &pGfxDevice->GetPlatform()->PlatformSettings());

    Gfx9BufferSrd* pOutSrd = static_cast<Gfx9BufferSrd*>(pOut);

    for (uint32 idx = 0; idx < count; ++idx)
    {
        PAL_ASSERT(IsValidTypedBufferView(*pBufferViewInfo));

        pOutSrd->word0.bits.BASE_ADDRESS = LowPart(pBufferViewInfo->gpuAddr);
        pOutSrd->word1.u32All =
            ((HighPart(pBufferViewInfo->gpuAddr)           << Gfx09::SQ_BUF_RSRC_WORD1__BASE_ADDRESS_HI__SHIFT) |
             (static_cast<uint32>(pBufferViewInfo->stride) << Gfx09::SQ_BUF_RSRC_WORD1__STRIDE__SHIFT));

        pOutSrd->word2.bits.NUM_RECORDS = pGfxDevice->CalcNumRecords(static_cast<size_t>(pBufferViewInfo->range),
                                                                     static_cast<uint32>(pBufferViewInfo->stride));

        const SQ_SEL_XYZW01 SqSelX = Formats::Gfx9::HwSwizzle(pBufferViewInfo->swizzledFormat.swizzle.r);
        const SQ_SEL_XYZW01 SqSelY = Formats::Gfx9::HwSwizzle(pBufferViewInfo->swizzledFormat.swizzle.g);
        const SQ_SEL_XYZW01 SqSelZ = Formats::Gfx9::HwSwizzle(pBufferViewInfo->swizzledFormat.swizzle.b);
        const SQ_SEL_XYZW01 SqSelW = Formats::Gfx9::HwSwizzle(pBufferViewInfo->swizzledFormat.swizzle.a);

        // Get the HW format enumeration corresponding to the view-specified format.
        const BUF_DATA_FORMAT hwBufDataFmt = Formats::Gfx9::HwBufDataFmt(pFmtInfo,
                                                                         pBufferViewInfo->swizzledFormat.format);
        const BUF_NUM_FORMAT  hwBufNumFmt  = Formats::Gfx9::HwBufNumFmt(pFmtInfo,
                                                                        pBufferViewInfo->swizzledFormat.format);

        // If we get an invalid format in the buffer SRD, then the memory operation involving this SRD will be dropped
        PAL_ASSERT(hwBufDataFmt != BUF_DATA_FORMAT_INVALID);

        pOutSrd->word3.u32All = ((SQ_RSRC_BUF  << Gfx09::SQ_BUF_RSRC_WORD3__TYPE__SHIFT)        |
                                 (SqSelX       << Gfx09::SQ_BUF_RSRC_WORD3__DST_SEL_X__SHIFT)   |
                                 (SqSelY       << Gfx09::SQ_BUF_RSRC_WORD3__DST_SEL_Y__SHIFT)   |
                                 (SqSelZ       << Gfx09::SQ_BUF_RSRC_WORD3__DST_SEL_Z__SHIFT)   |
                                 (SqSelW       << Gfx09::SQ_BUF_RSRC_WORD3__DST_SEL_W__SHIFT)   |
                                 (hwBufDataFmt << Gfx09::SQ_BUF_RSRC_WORD3__DATA_FORMAT__SHIFT) |
                                 (hwBufNumFmt  << Gfx09::SQ_BUF_RSRC_WORD3__NUM_FORMAT__SHIFT));

        pOutSrd++;
        pBufferViewInfo++;
    }
}

// =====================================================================================================================
uint32 Device::BufferSrdResourceLevel() const
{
    const Pal::Device& device = *(Parent());

    uint32  resourceLevel = 1;

#if PAL_BUILD_GFX11
    // GFX11 parts don't have a "resource level" bit in their buffer SRDs.
    if (IsGfx11(device))
    {
        resourceLevel = 0;
    }
#endif

    return resourceLevel;
}

// =====================================================================================================================
// Gfx10 specific function for creating typed buffer view SRDs.
void PAL_STDCALL Device::Gfx10CreateTypedBufferViewSrds(
    const IDevice*        pDevice,
    uint32                count,
    const BufferViewInfo* pBufferViewInfo,
    void*                 pOut)
{
#if PAL_BUILD_GFX11
    static_assert((Gfx10CoreSqBufRsrcTWord3FormatShift == Gfx104PlusSqBufRsrcTWord3FormatShift),
                  "LSB of sq_buf_rsrc_t.format field is different between GFX10 and GFX11.");
#endif

    PAL_ASSERT((pDevice != nullptr) && (pOut != nullptr) && (pBufferViewInfo != nullptr) && (count > 0));
    const auto*const pPalDevice = static_cast<const Pal::Device*>(pDevice);
    const auto*const pGfxDevice = static_cast<const Device*>(pPalDevice->GetGfxDevice());
    const auto*const pFmtInfo   = MergedChannelFlatFmtInfoTbl(pPalDevice->ChipProperties().gfxLevel,
                                                              &pGfxDevice->GetPlatform()->PlatformSettings());

    sq_buf_rsrc_t* pOutSrd = static_cast<sq_buf_rsrc_t*>(pOut);

    // This means "(index >= NumRecords)" is out-of-bounds.
    constexpr uint32 OobSelect = SQ_OOB_INDEX_ONLY;

    for (uint32 idx = 0; idx < count; ++idx)
    {
        PAL_ASSERT(IsValidTypedBufferView(*pBufferViewInfo));

        pOutSrd->u32All[0] = LowPart(pBufferViewInfo->gpuAddr);
        pOutSrd->u32All[1] =
            (HighPart(pBufferViewInfo->gpuAddr) |
             (static_cast<uint32>(pBufferViewInfo->stride) << SqBufRsrcTWord1StrideShift));

        pOutSrd->u32All[2] = pGfxDevice->CalcNumRecords(static_cast<size_t>(pBufferViewInfo->range),
                                                        static_cast<uint32>(pBufferViewInfo->stride));

        uint32 llcNoalloc = 0;

        if (pPalDevice->MemoryProperties().flags.supportsMall != 0)
        {
            // The SRD has a two-bit field where the high-bit is the control for "read" operations
            // and the low bit is the control for bypassing the MALL on write operations.
            llcNoalloc = CalcLlcNoalloc(pBufferViewInfo->flags.bypassMallRead,
                                        pBufferViewInfo->flags.bypassMallWrite);
        }

        const SQ_SEL_XYZW01 SqSelX = Formats::Gfx9::HwSwizzle(pBufferViewInfo->swizzledFormat.swizzle.r);
        const SQ_SEL_XYZW01 SqSelY = Formats::Gfx9::HwSwizzle(pBufferViewInfo->swizzledFormat.swizzle.g);
        const SQ_SEL_XYZW01 SqSelZ = Formats::Gfx9::HwSwizzle(pBufferViewInfo->swizzledFormat.swizzle.b);
        const SQ_SEL_XYZW01 SqSelW = Formats::Gfx9::HwSwizzle(pBufferViewInfo->swizzledFormat.swizzle.a);

        // Get the HW format enumeration corresponding to the view-specified format.
        const BUF_FMT hwBufFmt = Formats::Gfx9::HwBufFmt(pFmtInfo, pBufferViewInfo->swizzledFormat.format);

        // If we get an invalid format in the buffer SRD, then the memory operation involving this SRD will be dropped
        PAL_ASSERT(hwBufFmt != BUF_FMT_INVALID);
        pOutSrd->u32All[3] = ((SqSelX                         << SqBufRsrcTWord3DstSelXShift)                       |
                              (SqSelY                         << SqBufRsrcTWord3DstSelYShift)                       |
                              (SqSelZ                         << SqBufRsrcTWord3DstSelZShift)                       |
                              (SqSelW                         << SqBufRsrcTWord3DstSelWShift)                       |
                              (hwBufFmt                       << Gfx10CoreSqBufRsrcTWord3FormatShift)               |
                              (pGfxDevice->BufferSrdResourceLevel() << Gfx10CoreSqBufRsrcTWord3ResourceLevelShift)  |
                              (OobSelect                      << SqBufRsrcTWord3OobSelectShift)                     |
                              (llcNoalloc                     << Gfx103PlusExclusiveSqBufRsrcTWord3LlcNoallocShift) |
                              (SQ_RSRC_BUF                    << SqBufRsrcTWord3TypeShift));

        pOutSrd++;
        pBufferViewInfo++;
    }
}

// =====================================================================================================================
// Gfx9 specific function for creating untyped buffer view SRDs.
void PAL_STDCALL Device::Gfx9CreateUntypedBufferViewSrds(
    const IDevice*        pDevice,
    uint32                count,
    const BufferViewInfo* pBufferViewInfo,
    void*                 pOut)
{
    PAL_ASSERT((pDevice != nullptr) && (pOut != nullptr) && (pBufferViewInfo != nullptr) && (count > 0));
    const auto*const pGfxDevice = static_cast<const Device*>(static_cast<const Pal::Device*>(pDevice)->GetGfxDevice());

    Gfx9BufferSrd* pOutSrd = static_cast<Gfx9BufferSrd*>(pOut);

    for (uint32 idx = 0; idx < count; ++idx)
    {
        PAL_ASSERT((pBufferViewInfo->gpuAddr != 0) || (pBufferViewInfo->range == 0));

        pOutSrd->word0.bits.BASE_ADDRESS = LowPart(pBufferViewInfo->gpuAddr);

        pOutSrd->word1.u32All =
            ((HighPart(pBufferViewInfo->gpuAddr)           << Gfx09::SQ_BUF_RSRC_WORD1__BASE_ADDRESS_HI__SHIFT) |
             (static_cast<uint32>(pBufferViewInfo->stride) << Gfx09::SQ_BUF_RSRC_WORD1__STRIDE__SHIFT));

        pOutSrd->word2.bits.NUM_RECORDS = pGfxDevice->CalcNumRecords(static_cast<size_t>(pBufferViewInfo->range),
                                                                     static_cast<uint32>(pBufferViewInfo->stride));

        PAL_ASSERT(Formats::IsUndefined(pBufferViewInfo->swizzledFormat.format));

        if (pBufferViewInfo->gpuAddr != 0)
        {
            pOutSrd->word3.u32All  = ((SQ_RSRC_BUF         << Gfx09::SQ_BUF_RSRC_WORD3__TYPE__SHIFT)        |
                                      (SQ_SEL_X            << Gfx09::SQ_BUF_RSRC_WORD3__DST_SEL_X__SHIFT)   |
                                      (SQ_SEL_Y            << Gfx09::SQ_BUF_RSRC_WORD3__DST_SEL_Y__SHIFT)   |
                                      (SQ_SEL_Z            << Gfx09::SQ_BUF_RSRC_WORD3__DST_SEL_Z__SHIFT)   |
                                      (SQ_SEL_W            << Gfx09::SQ_BUF_RSRC_WORD3__DST_SEL_W__SHIFT)   |
                                      (BUF_DATA_FORMAT_32  << Gfx09::SQ_BUF_RSRC_WORD3__DATA_FORMAT__SHIFT) |
                                      (BUF_NUM_FORMAT_UINT << Gfx09::SQ_BUF_RSRC_WORD3__NUM_FORMAT__SHIFT));
        }
        else
        {
            pOutSrd->word3.u32All = 0;
        }

        pOutSrd++;
        pBufferViewInfo++;
    }
}

// =====================================================================================================================
// Gfx10 specific function for creating untyped buffer view SRDs.
void PAL_STDCALL Device::Gfx10CreateUntypedBufferViewSrds(
    const IDevice*        pDevice,
    uint32                count,
    const BufferViewInfo* pBufferViewInfo,
    void*                 pOut)
{
    PAL_ASSERT((pDevice != nullptr) && (pOut != nullptr) && (pBufferViewInfo != nullptr) && (count > 0));
    const auto*const pPalDevice = static_cast<const Pal::Device*>(pDevice);
    const auto*const pGfxDevice = static_cast<const Device*>(pPalDevice->GetGfxDevice());

    sq_buf_rsrc_t* pOutSrd = static_cast<sq_buf_rsrc_t*>(pOut);

    for (uint32 idx = 0; idx < count; ++idx)
    {
        PAL_ASSERT((pBufferViewInfo->gpuAddr != 0) || (pBufferViewInfo->range == 0));

        pOutSrd->u32All[0] = LowPart(pBufferViewInfo->gpuAddr);

        pOutSrd->u32All[1] =
            (HighPart(pBufferViewInfo->gpuAddr) |
             (static_cast<uint32>(pBufferViewInfo->stride) << SqBufRsrcTWord1StrideShift));

        pOutSrd->u32All[2] = pGfxDevice->CalcNumRecords(static_cast<size_t>(pBufferViewInfo->range),
                                                        static_cast<uint32>(pBufferViewInfo->stride));

        PAL_ASSERT(Formats::IsUndefined(pBufferViewInfo->swizzledFormat.format));

        uint32 llcNoalloc = 0;

        if (pPalDevice->MemoryProperties().flags.supportsMall != 0)
        {
            // The SRD has a two-bit field where the high-bit is the control for "read" operations
            // and the low bit is the control for bypassing the MALL on write operations.
            llcNoalloc = CalcLlcNoalloc(pBufferViewInfo->flags.bypassMallRead,
                                        pBufferViewInfo->flags.bypassMallWrite);
        }

        if (pBufferViewInfo->gpuAddr != 0)
        {
            const uint32 oobSelect = ((pBufferViewInfo->stride == 1) ||
                                      (pBufferViewInfo->stride == 0)) ? SQ_OOB_COMPLETE : SQ_OOB_INDEX_ONLY;

            PAL_ASSERT((llcNoalloc == 0) || (IsGfx103PlusExclusive(*pPalDevice)));

            pOutSrd->u32All[3] = ((SQ_SEL_X                       << SqBufRsrcTWord3DstSelXShift)                       |
                                  (SQ_SEL_Y                       << SqBufRsrcTWord3DstSelYShift)                       |
                                  (SQ_SEL_Z                       << SqBufRsrcTWord3DstSelZShift)                       |
                                  (SQ_SEL_W                       << SqBufRsrcTWord3DstSelWShift)                       |
                                  (BUF_FMT_32_UINT                << Gfx10CoreSqBufRsrcTWord3FormatShift)               |
                                  (pGfxDevice->BufferSrdResourceLevel() << Gfx10CoreSqBufRsrcTWord3ResourceLevelShift)  |
                                  (oobSelect                      << SqBufRsrcTWord3OobSelectShift)                     |
                                  (llcNoalloc                     << Gfx103PlusExclusiveSqBufRsrcTWord3LlcNoallocShift) |
                                  (SQ_RSRC_BUF                    << SqBufRsrcTWord3TypeShift));
        }
        else
        {
            pOutSrd->u32All[3] = 0;
        }

        pOutSrd++;
        pBufferViewInfo++;
    }
}

// =====================================================================================================================
// Add this function to avoid register redefintion in gfx9chip.h that was concerned the 4-bit may change
// from one asic to another moving forward.
static void PAL_STDCALL SetImageViewSamplePatternIdx(
    Gfx9ImageSrd*  pSrd,
    uint32         samplePatternIdx)
{
    // If this 4-bit used for samplePatternIdx is somehow different,
    // we can redefine it based on asic Id.
    struct Gfx9ImageSrdWord6
    {
#if defined(LITTLEENDIAN_CPU)
        uint32 samplePatternIdx : 4;
        uint32 reserved : 28;
#elif defined(BIGENDIAN_CPU)
        uint32 reserved : 28;
        uint32 samplePatternIdx : 4;
#endif
    };

    reinterpret_cast<Gfx9ImageSrdWord6*>(&(pSrd->word6))->samplePatternIdx = samplePatternIdx;
}

// =====================================================================================================================
// Returns the value for SQ_IMG_RSRC_WORD4.BC_SWIZZLE
static TEX_BC_SWIZZLE GetBcSwizzle(
    const ImageCreateInfo& imageCreateInfo)
{
    // GFX9+ applies image view swizzle to border color in hardware.
    // The only thing we have to do is to apply swizzle to border color, which is specified as image format swizzle
    // relative to RGBA format e.g. RAGB image format has a swizzle of XWYZ relative to RGBA.
    const ChannelMapping&  swizzle   = imageCreateInfo.swizzledFormat.swizzle;
    TEX_BC_SWIZZLE         bcSwizzle = TEX_BC_Swizzle_XYZW;

    const uint32 numComponents = Formats::NumComponents(imageCreateInfo.swizzledFormat.format);

    // If the format has 3 or 4 components there is only one possible combination that matches
    if (numComponents >= 3)
    {
        if ((swizzle.r == ChannelSwizzle::X) &&
            (swizzle.g == ChannelSwizzle::Y) &&
            (swizzle.b == ChannelSwizzle::Z))
        {
            // RGBA
            bcSwizzle = TEX_BC_Swizzle_XYZW;
        }
        else if ((swizzle.r == ChannelSwizzle::X) &&
                 (swizzle.a == ChannelSwizzle::Y) &&
                 (swizzle.g == ChannelSwizzle::Z))
        {
            // RAGB
            bcSwizzle = TEX_BC_Swizzle_XWYZ;
        }
        else if ((swizzle.a == ChannelSwizzle::X) &&
                 (swizzle.b == ChannelSwizzle::Y) &&
                 (swizzle.g == ChannelSwizzle::Z))
        {
            // ABGR
            bcSwizzle = TEX_BC_Swizzle_WZYX;
        }
        else if ((swizzle.a == ChannelSwizzle::X) &&
                 (swizzle.r == ChannelSwizzle::Y) &&
                 (swizzle.g == ChannelSwizzle::Z))
        {
            // ARGB
            bcSwizzle = TEX_BC_Swizzle_WXYZ;
        }
        else if ((swizzle.b == ChannelSwizzle::X) &&
                 (swizzle.g == ChannelSwizzle::Y) &&
                 (swizzle.r == ChannelSwizzle::Z))
        {
            // BGRA
            bcSwizzle = TEX_BC_Swizzle_ZYXW;
        }
        else if ((swizzle.g == ChannelSwizzle::X) &&
                 (swizzle.r == ChannelSwizzle::Y) &&
                 (swizzle.a == ChannelSwizzle::Z))
        {
            // GRAB
            bcSwizzle = TEX_BC_Swizzle_YXWZ;
        }
    }
    // If the format has 2 components we have to match them and the remaining 2 can be in any order
    else if (numComponents == 2)
    {
        if ((swizzle.r == ChannelSwizzle::X) &&
            (swizzle.g == ChannelSwizzle::Y))
        {
            // RGBA
            bcSwizzle = TEX_BC_Swizzle_XYZW;
        }
        else if ((swizzle.r == ChannelSwizzle::X) &&
                 (swizzle.a == ChannelSwizzle::Y))
        {
            // RAGB
            bcSwizzle = TEX_BC_Swizzle_XWYZ;
        }
        else if ((swizzle.a == ChannelSwizzle::X) &&
                 (swizzle.b == ChannelSwizzle::Y))
        {
            // ABGR
            bcSwizzle = TEX_BC_Swizzle_WZYX;
        }
        else if ((swizzle.a == ChannelSwizzle::X) &&
                 (swizzle.r == ChannelSwizzle::Y))
        {
            // ARGB
            bcSwizzle = TEX_BC_Swizzle_WXYZ;
        }
        else if ((swizzle.b == ChannelSwizzle::X) &&
                 (swizzle.g == ChannelSwizzle::Y))
        {
            // BGRA
            bcSwizzle = TEX_BC_Swizzle_ZYXW;
        }
        else if ((swizzle.g == ChannelSwizzle::X) &&
                 (swizzle.r == ChannelSwizzle::Y))
        {
            // GRAB
            bcSwizzle = TEX_BC_Swizzle_YXWZ;
        }
    }
    // If the format has 1 component we have to match it and the remaining 3 can be in any order
    else
    {
        if (swizzle.r == ChannelSwizzle::X)
        {
            // RGBA or RAGB
            bcSwizzle = TEX_BC_Swizzle_XYZW;
        }
        else if (swizzle.g == ChannelSwizzle::X)
        {
            // GRAB
            bcSwizzle = TEX_BC_Swizzle_YXWZ;
        }
        else if (swizzle.b == ChannelSwizzle::X)
        {
            // BGRA
            bcSwizzle = TEX_BC_Swizzle_ZYXW;
        }
        else if (swizzle.a == ChannelSwizzle::X)
        {
            // ABGR or ARGB
            bcSwizzle = TEX_BC_Swizzle_WXYZ;
        }
    }

    return bcSwizzle;
}

// =====================================================================================================================
static ImageViewType GetViewType(
    const ImageViewInfo&   viewInfo)
{
    const auto*    pPalImage  = static_cast<const Pal::Image*>(viewInfo.pImage);
    const auto*    pGfxImage  = pPalImage->GetGfxImage();
    const auto&    createInfo = pPalImage->GetImageCreateInfo();
    ImageViewType  viewType   = viewInfo.viewType;

    if ((viewType == ImageViewType::Tex1d)         &&            // requesting a 1D view
        (createInfo.imageType == ImageType::Tex1d) &&            // image that was created by app to be 1D
        (pGfxImage->GetOverrideImageType() == ImageType::Tex2d)) // image has been overridden to be 2D
    {
        viewType = ImageViewType::Tex2d;
    }

    return viewType;
}

// =====================================================================================================================
// Function for checking to see if an override is needed of the image format to workaround a gfx9 hardware issue.
// Special handling is needed for X8Y8_Z8Y8_*, Y8X8_Y8Z8_* resources. gfx9 hardware does not calculate the
// dimensions of all mipmaps correctly. All mips must have dimensions with an even width but hardware does
// not do this. To workaround this issue, the driver needs to change the bpp from 16 to 32, use the aligned
// (i.e., actual) dimensions, and the copy each mip as if it were an individual resource. For mip levels
// not in the mip tail, each mip level is copied as a non-mipmapped, non-array resource. For mip levels in
// the mip tail, all mip levels in the mip tail are copied as a single mipmapped, non-array resource. Because
// the driver is overriding the normal gfx9 copy, the driver must apply the slice Xor directly to the address
// so that the mip level is placed correctly in memory.
static bool IsGfx9ImageFormatWorkaroundNeeded(
    const ImageCreateInfo& imageCreateInfo,
    ChNumFormat*           pFormat,
    uint32*                pPixelsPerBlock)
{
    bool isOverrideNeeded = false;

    if ((imageCreateInfo.imageType != ImageType::Tex3d) &&
        (imageCreateInfo.mipLevels > 1) &&
        Formats::IsMacroPixelPacked(*pFormat) &&
        (Formats::IsYuvPacked(*pFormat) == false))
    {
        isOverrideNeeded = true;
        *pFormat         = Pal::ChNumFormat::X32_Uint;
        *pPixelsPerBlock = 2;
    }
    return isOverrideNeeded;
}

// =====================================================================================================================
// This function checks to see if an override is needed for the image format in gfx10. In gfx10, YUV422 formats were
// changed in TC hardware to use 32bpp memory addressing instead of 16bpp in Vega HW. The Gfx10 HW scales the SRD width
// and x-coordinate accordingly for these formats. Using 32bpp was the intended behavior in Vega also. It's a bug that
// TC uses 16bpp in Vega.
static bool IsGfx10ImageFormatOverrideNeeded(
    const ImageCreateInfo& imageCreateInfo,
    ChNumFormat*           pFormat,
    uint32*                pPixelsPerBlock)
{
    bool isOverrideNeeded = false;

    if (Formats::IsMacroPixelPacked(*pFormat))
    {
        isOverrideNeeded = true;
        *pFormat         = Pal::ChNumFormat::X32_Uint;
        *pPixelsPerBlock = 2;
    }

    return isOverrideNeeded;
}

// =====================================================================================================================
// Checks if an image format override is needed.
bool Device::IsImageFormatOverrideNeeded(
    const ImageCreateInfo& imageCreateInfo,
    ChNumFormat*           pFormat,
    uint32*                pPixelsPerBlock
    ) const
{
    return IsGfx10Plus(m_gfxIpLevel) ?
        IsGfx10ImageFormatOverrideNeeded(imageCreateInfo, pFormat, pPixelsPerBlock) :
        IsGfx9ImageFormatWorkaroundNeeded(imageCreateInfo, pFormat, pPixelsPerBlock);
}

// =====================================================================================================================
static void GetSliceAddressOffsets(
    const Image& image,
    SubresId     subResId,
    uint32       arraySlice,
    uint32*      pSliceXor,
    gpusize*     pSliceOffset)
{
    ADDR2_COMPUTE_SLICE_PIPEBANKXOR_INPUT  inSliceXor      = { 0 };
    ADDR2_COMPUTE_SLICE_PIPEBANKXOR_OUTPUT outSliceXor     = { 0 };
    const auto*const                       pParent         = image.Parent();
    const ImageCreateInfo&                 imageCreateInfo = pParent->GetImageCreateInfo();
    Pal::Device*                           pDevice         = pParent->GetDevice();
    const SubResourceInfo*const            pSubResInfo     = pParent->SubresourceInfo(subResId);
    const auto*const                       pAddrOutput     = image.GetAddrOutput(pSubResInfo);
    const auto&                            surfSetting     = image.GetAddrSettings(pSubResInfo);
    const AddrMgr2::TileInfo*              pTileInfo       = AddrMgr2::GetTileInfo(pParent, subResId);

    inSliceXor.size            = sizeof(ADDR2_COMPUTE_SLICE_PIPEBANKXOR_INPUT);
    inSliceXor.swizzleMode     = surfSetting.swizzleMode;
    inSliceXor.resourceType    = surfSetting.resourceType;
    inSliceXor.basePipeBankXor = pTileInfo->pipeBankXor;
    inSliceXor.slice           = arraySlice;
    inSliceXor.numSamples      = imageCreateInfo.samples;

    // To place the mip correctly, obtain the slice Xor from AddrLib.
    ADDR_E_RETURNCODE addrRetCode = Addr2ComputeSlicePipeBankXor(pDevice->AddrLibHandle(),
                                                                 &inSliceXor,
                                                                 &outSliceXor);
    PAL_ASSERT(addrRetCode == ADDR_OK);
    if (addrRetCode == ADDR_OK)
    {
        *pSliceXor = outSliceXor.pipeBankXor;
    }
    *pSliceOffset = pAddrOutput->sliceSize * arraySlice;
}

// =====================================================================================================================
// Gfx9+ specific function for creating image view SRDs. Installed in the function pointer table of the parent device
// during initialization.
void PAL_STDCALL Device::Gfx9CreateImageViewSrds(
    const IDevice*       pDevice,
    uint32               count,
    const ImageViewInfo* pImgViewInfo,
    void*                pOut)
{
    PAL_ASSERT((pDevice != nullptr) && (pOut != nullptr) && (pImgViewInfo != nullptr) && (count > 0));
    const auto*const pPalDevice = static_cast<const Pal::Device*>(pDevice);
    const auto*const pGfxDevice = static_cast<const Device*>(pPalDevice->GetGfxDevice());
    const auto&      chipProp   = pPalDevice->ChipProperties();
    const auto*      pAddrMgr   = static_cast<const AddrMgr2::AddrMgr2*>(pPalDevice->GetAddrMgr());
    const auto*const pFmtInfo   = MergedChannelFmtInfoTbl(chipProp.gfxLevel,
                                                          &pGfxDevice->GetPlatform()->PlatformSettings());

    ImageSrd* pSrds = static_cast<ImageSrd*>(pOut);

    for (uint32 i = 0; i < count; ++i)
    {
        const ImageViewInfo&   viewInfo        = pImgViewInfo[i];
        PAL_ASSERT(viewInfo.subresRange.numPlanes == 1);

        const Image&           image           = *GetGfx9Image(viewInfo.pImage);
        const Gfx9MaskRam*     pMaskRam        = image.GetPrimaryMaskRam(viewInfo.subresRange.startSubres.plane);
        const auto*const       pParent         = static_cast<const Pal::Image*>(viewInfo.pImage);
        const ImageInfo&       imageInfo       = pParent->GetImageInfo();
        const ImageCreateInfo& imageCreateInfo = pParent->GetImageCreateInfo();
        const bool             imgIsBc         = Formats::IsBlockCompressed(imageCreateInfo.swizzledFormat.format);
        const bool             imgIsYuvPlanar  = Formats::IsYuvPlanar(imageCreateInfo.swizzledFormat.format);
        const bool             imgIsMacroPixelPacked =
            Formats::IsMacroPixelPacked(imageCreateInfo.swizzledFormat.format);

        Gfx9ImageSrd srd    = {};
        ChNumFormat  format = viewInfo.swizzledFormat.format;

        SubresId     baseSubResId   = { viewInfo.subresRange.startSubres.plane, 0, 0 };
        uint32       baseArraySlice = viewInfo.subresRange.startSubres.arraySlice;
        uint32       firstMipLevel  = viewInfo.subresRange.startSubres.mipLevel;
        uint32       mipLevels      = imageCreateInfo.mipLevels;

        if ((viewInfo.flags.zRangeValid == 1) && (imageCreateInfo.imageType == ImageType::Tex3d))
        {
            baseArraySlice = viewInfo.zRange.offset;
        }
        else if (imgIsYuvPlanar && (viewInfo.subresRange.numSlices == 1))
        {
            baseSubResId.arraySlice = baseArraySlice;
            baseArraySlice = 0;
        }
        PAL_ASSERT((viewInfo.possibleLayouts.engines != 0) && (viewInfo.possibleLayouts.usages != 0 ));

        bool                        overrideBaseResource       = false;
        bool                        overrideBaseResource96bpp  = false;
        bool                        overrideZRangeOffset       = false;
        uint32                      widthScaleFactor           = 1;
        uint32                      workaroundWidthScaleFactor = 1;
        bool                        includePadding             = (viewInfo.flags.includePadding != 0);
        gpusize                     sliceOffset                = 0;
        uint32                      sliceXor                   = 0;
        const SubResourceInfo*const pSubResInfo                = pParent->SubresourceInfo(baseSubResId);
        const auto*const            pAddrOutput                = image.GetAddrOutput(pSubResInfo);
        const auto&                 surfSetting                = image.GetAddrSettings(pSubResInfo);
        ChNumFormat                 imageFormat                = imageCreateInfo.swizzledFormat.format;

        if (IsGfx9ImageFormatWorkaroundNeeded(imageCreateInfo, &imageFormat, &workaroundWidthScaleFactor) &&
            (viewInfo.swizzledFormat.format == imageFormat))
        {
            overrideBaseResource = true;
            widthScaleFactor     = workaroundWidthScaleFactor;
            includePadding       = true;

            GetSliceAddressOffsets(image,
                                   baseSubResId,
                                   baseArraySlice,
                                   &sliceXor,
                                   &sliceOffset);

            baseArraySlice = 0;

            if (firstMipLevel < pAddrOutput->firstMipIdInTail)
            {
                // copy mip level as individual resource
                mipLevels             = 1;
                baseSubResId.mipLevel = firstMipLevel;
                firstMipLevel         = 0;
            }
            else
            {
                // copy whole mip tail as single resource
                mipLevels            -= pAddrOutput->firstMipIdInTail;
                baseSubResId.mipLevel = pAddrOutput->firstMipIdInTail;
                firstMipLevel        -= pAddrOutput->firstMipIdInTail;
            }
        }

        // Validate subresource ranges
        const SubResourceInfo* pBaseSubResInfo = pParent->SubresourceInfo(baseSubResId);

        Extent3d extent       = pBaseSubResInfo->extentTexels;
        Extent3d actualExtent = pBaseSubResInfo->actualExtentTexels;

        extent.width       /= widthScaleFactor;
        actualExtent.width /= widthScaleFactor;

        // The view should be in terms of texels except in four special cases when we're operating in terms of elements:
        // 1. Viewing a compressed image in terms of blocks. For BC images elements are blocks, so if the caller gave
        //    us an uncompressed view format we assume they want to view blocks.
        // 2. Copying to an "expanded" format (e.g., R32G32B32). In this case we can't do native format writes so we're
        //    going to write each element independently. The trigger for this case is a mismatched bpp.
        // 3. Viewing a YUV-packed image with a non-YUV-packed format when the view format is allowed for view formats
        //    with twice the bpp. In this case, the effective width of the view is half that of the base image.
        // 4. Viewing a YUV-planar Image which has multiple array slices. In this case, the texture hardware has no way
        //    to know about the padding in between array slices of the same plane (due to the other plane's slices being
        //    interleaved). In this case, we pad out the actual height of the view to span all planes (so that the view
        //    can access each array slice).
        //    This has the unfortunate side-effect of making normalized texture coordinates inaccurate.
        //    However, this is required for access to multiple slices.
        if (overrideBaseResource == false)
        {
            if (imgIsBc && (Formats::IsBlockCompressed(format) == false))
            {
                // If we have the following image:
                //              Uncompressed pixels   Compressed block sizes (4x4)
                //      mip0:       22 x 22                   6 x 6
                //      mip1:       11 x 11                   3 x 3
                //      mip2:        5 x  5                   2 x 2
                //      mip3:        2 x  2                   1 x 1
                //      mip4:        1 x  1                   1 x 1
                //
                // On GFX9 the SRD is always programmed with the WIDTH and HEIGHT of the base level and the HW is
                // calculating the degradation of the block sizes down the mip-chain as follows (straight-up
                // divide-by-two integer math):
                //      mip0:  6x6
                //      mip1:  3x3
                //      mip2:  1x1
                //      mip3:  1x1
                //
                // This means that mip2 will be missing texels.
                //
                // Fix this by calculating the start mip's ceil(texels/blocks) width and height and then go up the chain
                // to pad the base mip's width and height to account for this.  A result lower than the base mip's
                // indicates a non-power-of-two texture, and the result should be clamped to its extentElements.
                // Otherwise, if the mip is aligned to block multiples, the result will be equal to extentElements.  If
                // there is no suitable width or height, the actualExtentElements is chosen.  The application is in
                // charge of making sure the math works out properly if they do this (allowed by Vulkan), otherwise we
                // assume it's an internal view and the copy shaders will prevent accessing out-of-bounds pixels.
                SubresId               mipSubResId    = { viewInfo.subresRange.startSubres.plane, firstMipLevel, 0 };
                const SubResourceInfo* pMipSubResInfo = pParent->SubresourceInfo(mipSubResId);

                extent.width  = Util::Clamp((pMipSubResInfo->extentElements.width  << firstMipLevel),
                                            pBaseSubResInfo->extentElements.width,
                                            pBaseSubResInfo->actualExtentElements.width);
                extent.height = Util::Clamp((pMipSubResInfo->extentElements.height << firstMipLevel),
                                            pBaseSubResInfo->extentElements.height,
                                            pBaseSubResInfo->actualExtentElements.height);

                actualExtent = pBaseSubResInfo->actualExtentElements;

                // It would appear that HW needs the actual extents to calculate the mip addresses correctly when
                // viewing more than 1 mip especially in the case of non power of two textures.
                if (viewInfo.subresRange.numMips > 1)
                {
                    includePadding = true;
                }
            }
            else if (pBaseSubResInfo->bitsPerTexel != Formats::BitsPerPixel(format))
            {
                extent       = pBaseSubResInfo->extentElements;
                actualExtent = pBaseSubResInfo->actualExtentElements;

                includePadding = true;

                // For 96 bit bpp formats(X32Y32Z32_Uint/X32Y32Z32_Sint/X32Y32Z32_Float), X32_Uint formated image view
                // srd might be created upon the image for image copy operation. Extent of mipmaped level of X32_Uint
                // and mipmaped level of the original X32Y32Z32_* format might mismatch, especially on the last several
                // mips. Thus, it could be problematic to use 256b address of zero-th mip + mip level mode. Instead we
                // shall adopt 256b address of startsubres's miplevel/arrayLevel.
                if (pBaseSubResInfo->bitsPerTexel == 96)
                {
                    PAL_ASSERT(viewInfo.subresRange.numMips == 1);
                    baseSubResId.mipLevel = firstMipLevel;
                    firstMipLevel         = 0;

                    // For gfx9 the baseSubResId should point to the baseArraySlice instead of setting the base_array
                    // SRD. When baseSubResId is used to calculate the baseAddress value, the current array slice will
                    // will be included in the equation.
                    PAL_ASSERT(viewInfo.subresRange.numSlices == 1);

                    // For gfx9 3d texture, we need to access per z slice instead subresource.
                    // Z slices are interleaved for mipmapped 3d texture. (each DepthPitch contains all the miplevels)
                    // example: the memory layout for a 3 miplevel WxHxD 3d texture:
                    // baseAddress(mip0) + DepthPitch * 0: subresource(mip0)'s 0 slice
                    // baseAddress(mip1) + DepthPitch * 0: subresource(mip1)'s 0 slice
                    // baseAddress(mip2) + DepthPitch * 0: subresource(mip2)'s 0 slice
                    // baseAddress(mip0) + DepthPitch * 1: subresource(mip0)'s 1 slice
                    // baseAddress(mip1) + DepthPitch * 1: subresource(mip1)'s 1 slice
                    // baseAddress(mip2) + DepthPitch * 1: subresource(mip2)'s 1 slice
                    // ...
                    // baseAddress(mip0) + DepthPitch * (D-1): subresource(mip0)'s D-1 slice
                    // baseAddress(mip1) + DepthPitch * (D-1): subresource(mip1)'s D-1 slice
                    // baseAddress(mip2) + DepthPitch * (D-1): subresource(mip2)'s D-1 slice
                    // When we try to view each subresource as 1 miplevel, we can't use srd.word5.bits.BASE_ARRAY to
                    // access each z slices since the srd for hardware can't compute the correct z slice stride.
                    // Instead we need a view to each slice.
                    if (imageCreateInfo.imageType == ImageType::Tex3d)
                    {
                        PAL_ASSERT((viewInfo.flags.zRangeValid == 1) && (viewInfo.zRange.extent == 1));
                        PAL_ASSERT(image.IsSubResourceLinear(baseSubResId));

                        baseSubResId.arraySlice = 0;
                        overrideZRangeOffset    = viewInfo.flags.zRangeValid;
                    }
                    else
                    {
                        baseSubResId.arraySlice = baseArraySlice;
                    }

                    baseArraySlice             = 0;
                    overrideBaseResource96bpp  = true;

                    pBaseSubResInfo = pParent->SubresourceInfo(baseSubResId);
                    extent          = pBaseSubResInfo->extentElements;
                    actualExtent    = pBaseSubResInfo->actualExtentElements;
                }
            }
            else if (imgIsMacroPixelPacked                          &&
                     (Formats::IsMacroPixelPacked(format) == false) &&
                     (imageCreateInfo.mipLevels > 1))
            {
                // For MacroPixelPacked pixel (sub-sampled format like X8Y8_Z8Y8_Unorm),
                // it must have a size that is a multiple of 2 in the x dimension.
                //              orignal size           replaced copy format size (Uint16)
                //      mip0:       18 x 17                   18 x 17
                //      mip1:       10 x  8                    9 x  8
                //      mip2:        4 x  4                    4 x  4
                //      mip3:        2 x  2                    2 x  2
                //      mip4:        2 x  1                    1 x  1
                // The last pixel will be skipped in some miplevel when using replaced format srv used as copy dst.
                includePadding = true;
            }
        }

        bool modifiedYuvExtents = false;

        if (Formats::IsYuvPacked(pBaseSubResInfo->format.format) &&
            (Formats::IsYuvPacked(format) == false)              &&
            ((pBaseSubResInfo->bitsPerTexel << 1) == Formats::BitsPerPixel(format)))
        {
            // Changing how we interpret the bits-per-pixel of the subresource wreaks havoc with any tile swizzle
            // pattern used. This will only work for linear-tiled Images.
            PAL_ASSERT(image.IsSubResourceLinear(baseSubResId));

            extent.width       >>= 1;
            actualExtent.width >>= 1;
            modifiedYuvExtents = true;
        }
        else if (Formats::IsYuvPlanar(imageCreateInfo.swizzledFormat.format))
        {
            if (viewInfo.subresRange.numSlices > 1)
            {
                image.PadYuvPlanarViewActualExtent(baseSubResId, &actualExtent);
                includePadding     = true;
                modifiedYuvExtents = true;
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

        constexpr uint32 Gfx9MinLodIntBits  = 4;
        constexpr uint32 Gfx9MinLodFracBits = 8;

        srd.word0.u32All = 0;
        // IMG RSRC MIN_LOD field is unsigned
        srd.word1.bits.MIN_LOD     = Math::FloatToUFixed(viewInfo.minLod, Gfx9MinLodIntBits, Gfx9MinLodFracBits, true);
        srd.word1.bits.DATA_FORMAT = Formats::Gfx9::HwImgDataFmt(pFmtInfo, format);
        srd.word1.bits.NUM_FORMAT  = Formats::Gfx9::HwImgNumFmt(pFmtInfo, format);

        // GFX9 does not support native 24-bit surfaces...  Clients promote 24-bit depth surfaces to 32-bit depth on
        // image creation.  However, they can request that border color data be clamped appropriately for the original
        // 24-bit depth.  Don't check for explicit depth surfaces here, as that only pertains to bound depth surfaces,
        // not to purely texture surfaces.
        //
        if ((imageCreateInfo.usageFlags.depthAsZ24 != 0) &&
            (Formats::ShareChFmt(format, ChNumFormat::X32_Uint)) &&
            ((pBaseSubResInfo->flags.supportMetaDataTexFetch == 0) ||
             (pGfxDevice->Settings().waDisable24BitHWFormatForTCCompatibleDepth == false)))
        {
            srd.word1.bits.DATA_FORMAT = IMG_DATA_FORMAT_8_24;
            srd.word1.bits.NUM_FORMAT  = IMG_NUM_FORMAT_FLOAT;
        }
        else if ((Formats::BytesPerPixel(format) == 1) &&
                 pParent->HasDepthPlane()              &&
                 image.HasDsMetadata())
        {
            // If they're requesting the stencil plane (i.e., an 8bpp view)       -and-
            // this surface also has Z data (i.e., is not a stencil-only surface) -and-
            // this surface has hTile data
            //
            // then we have to program the data-format of the stencil surface to match the bpp of the Z surface.
            // i.e., if we setup the stencil plane with an 8bpp format, then the HW will address into hTile
            // data as if it was laid out as 8bpp, when it reality, it's laid out with the bpp of the associated
            // Z surface.
            //
            const uint32  zBitCount = Formats::ComponentBitCounts(imageCreateInfo.swizzledFormat.format)[0];

            srd.word1.bits.DATA_FORMAT = ((zBitCount == 16)
                                          ? IMG_DATA_FORMAT_S8_16__GFX09
                                          : IMG_DATA_FORMAT_S8_32__GFX09);
        }

        const Extent3d programmedExtent = (includePadding) ? actualExtent : extent;
        srd.word2.bits.WIDTH  = (programmedExtent.width - 1);
        srd.word2.bits.HEIGHT = (programmedExtent.height - 1);

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
            texOptLevel = static_cast<const Pal::Device*>(pDevice)->Settings().textureOptLevel;
            break;
        }

        PAL_ASSERT(texOptLevel < ArrayLen(PanelToTexPerfMod));

        TexPerfModulation perfMod = PanelToTexPerfMod[texOptLevel];

        srd.word2.bits.PERF_MOD = static_cast<uint32>(perfMod);

        // Destination swizzles come from the view creation info, rather than the format of the view.
        srd.word3.bits.DST_SEL_X = Formats::Gfx9::HwSwizzle(viewInfo.swizzledFormat.swizzle.r);
        srd.word3.bits.DST_SEL_Y = Formats::Gfx9::HwSwizzle(viewInfo.swizzledFormat.swizzle.g);
        srd.word3.bits.DST_SEL_Z = Formats::Gfx9::HwSwizzle(viewInfo.swizzledFormat.swizzle.b);
        srd.word3.bits.DST_SEL_W = Formats::Gfx9::HwSwizzle(viewInfo.swizzledFormat.swizzle.a);

        // We need to use D swizzle mode for writing an image with view3dAs2dArray feature enabled.
        // But when reading from it, we need to use S mode.
        // In AddrSwizzleMode, S mode is always right before D mode, so we simply do a "-1" here.
        if ((viewInfo.viewType == ImageViewType::Tex2d) && (imageCreateInfo.flags.view3dAs2dArray))
        {
            const AddrSwizzleMode view3dAs2dReadSwizzleMode = static_cast<AddrSwizzleMode>(surfSetting.swizzleMode - 1);
            PAL_ASSERT(AddrMgr2::IsStandardSwzzle(view3dAs2dReadSwizzleMode));

            srd.word3.bits.SW_MODE = pAddrMgr->GetHwSwizzleMode(view3dAs2dReadSwizzleMode);
        }
        else
        {
            srd.word3.bits.SW_MODE = pAddrMgr->GetHwSwizzleMode(surfSetting.swizzleMode);
        }

        const bool isMultiSampled = (imageCreateInfo.samples > 1);

        // NOTE: Where possible, we always assume an array view type because we don't know how the shader will
        // attempt to access the resource.
        const ImageViewType  viewType = GetViewType(viewInfo);
        switch (viewType)
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

        if (isMultiSampled)
        {
            // MSAA textures cannot be mipmapped; the LAST_LEVEL and MAX_MIP fields indicate the texture's
            // sample count.  According to the docs, these are samples.  According to reality, this is
            // fragments.  I'm going with reality.
            srd.word3.bits.BASE_LEVEL = 0;
            srd.word3.bits.LAST_LEVEL = Log2(imageCreateInfo.fragments);
            srd.word5.bits.MAX_MIP    = Log2(imageCreateInfo.fragments);
        }
        else
        {
            srd.word3.bits.BASE_LEVEL = firstMipLevel;
            srd.word3.bits.LAST_LEVEL = firstMipLevel + viewInfo.subresRange.numMips - 1;
            srd.word5.bits.MAX_MIP    = mipLevels - 1;
        }

        uint32 depth = 0;

        // From reg spec: Units are "depth - 1", so 0 = 1 slice, 1= 2 slices.
        // If the image type is 3D, then the DEPTH field is the image's depth - 1.
        // Otherwise, the DEPTH field replaces the old "last_array" field.

        // Note that we can't use viewInfo.viewType here since 3D image may be viewed as 2D (array).
        if (imageCreateInfo.imageType == ImageType::Tex3d)
        {
            if (viewInfo.flags.zRangeValid == 1)
            {
                // If the client is specifying a valid Z range, the depth of the SRD must include the range's offset
                // and extent. Furthermore, the Z range is specified in terms of the view's first mip level, not the
                // Image's base mip level. The hardware, however, requires the SRD depth to be in terms of the base
                // mip level.
                // could be broken as gfx9 does not support zRange on 3d images
                const uint32 firstMip = viewInfo.subresRange.startSubres.mipLevel;
                depth = (((viewInfo.zRange.offset + viewInfo.zRange.extent) << firstMip) - 1);
            }
            else
            {
                depth = (pBaseSubResInfo->extentTexels.depth - 1);
            }
        }
        else
        {

            // For gfx9, there is no longer a separate last_array parameter  for arrays. Instead the "depth" input is
            // used as the last_array parameter. For cubemaps, depth is no longer interpreted as the number of full cube
            // maps (6 faces), but strictly as the number of array slices. It is up to driver to make sure depth-base is
            // modulo 6 for cube maps.
            depth = (viewInfo.subresRange.startSubres.arraySlice + viewInfo.subresRange.numSlices - 1);
        }

        srd.word4.bits.DEPTH      = depth;
        srd.word4.bits.BC_SWIZZLE = GetBcSwizzle(imageCreateInfo);

        if (modifiedYuvExtents == false)
        {
            srd.word4.bits.PITCH = AddrMgr2::CalcEpitch(pAddrOutput);
            if (overrideBaseResource && (pAddrOutput->epitchIsHeight == false))
            {
                srd.word4.bits.PITCH = ((srd.word4.bits.PITCH + 1) / 2) - 1;
            }
        }
        else
        {
            srd.word4.bits.PITCH =
                ((pAddrOutput->epitchIsHeight ? programmedExtent.height : programmedExtent.width) - 1);
        }

        srd.word5.bits.BASE_ARRAY        = baseArraySlice;
        if (pMaskRam != nullptr)
        {
            srd.word5.bits.META_PIPE_ALIGNED = pMaskRam->PipeAligned();
            srd.word5.bits.META_RB_ALIGNED   = pGfxDevice->IsRbAligned();
        }

        // Depth images obviously don't have an alpha component, so don't bother...
        if ((pParent->IsDepthStencilTarget() == false) && pBaseSubResInfo->flags.supportMetaDataTexFetch)
        {
            // The setup of the compression-related fields requires knowing the bound memory and the expected
            // usage of the memory (read or write), so defer most of the setup to "WriteDescriptorSlot".

            // For single-channel FORMAT cases, ALPHA_IS_ON_MSB(AIOM) = 0 indicates the channel is color.
            // while ALPHA_IS_ON_MSB (AIOM) = 1 indicates the channel is alpha.

            // Theratically, ALPHA_IS_ON_MSB should be set to 1 for all single-channel formats only if
            // swap is SWAP_ALT_REV as gfx6 implementation; however, there is a new CB feature - to compress to AC01
            // during CB rendering/draw on gfx9.2, which requires special handling.
            const SurfaceSwap surfSwap = Formats::Gfx9::ColorCompSwap(imageCreateInfo.swizzledFormat);

            if (Formats::NumComponents(imageCreateInfo.swizzledFormat.format) == 1)
            {
                srd.word6.bits.ALPHA_IS_ON_MSB =
                    (surfSwap == SWAP_ALT_REV) != (IsRaven2(*pPalDevice) || IsRenoir(*pPalDevice));
            }
            else if ((surfSwap != SWAP_STD_REV) && (surfSwap != SWAP_ALT_REV))
            {
                srd.word6.bits.ALPHA_IS_ON_MSB = 1;
            }
        }

        if (pParent->GetBoundGpuMemory().IsBound())
        {
            if ((imgIsYuvPlanar && (viewInfo.subresRange.numSlices == 1)) || overrideBaseResource96bpp)
            {
                gpusize gpuVirtAddress = pParent->GetSubresourceBaseAddr(baseSubResId);

                if (overrideZRangeOffset)
                {
                    gpuVirtAddress += viewInfo.zRange.offset * pBaseSubResInfo->depthPitch;
                }

                srd.word0.bits.BASE_ADDRESS    = Get256BAddrLo(gpuVirtAddress);
                srd.word1.bits.BASE_ADDRESS_HI = Get256BAddrHi(gpuVirtAddress);
            }
            else
            {
                if (overrideBaseResource)
                {
                    const gpusize gpuVirtAddress = image.GetMipAddr(baseSubResId);
                    srd.word0.bits.BASE_ADDRESS  = Get256BAddrLo(gpuVirtAddress + sliceOffset) | sliceXor;
                }
                else
                {
                    srd.word0.bits.BASE_ADDRESS = LowPart(image.GetSubresource256BAddr(baseSubResId));
                }
                // Usually, we'll never have an image address that extends into 40 bits.
                // However, when svm is enabled, The bit 39 of an image address is 1 if the address is gpuvm.
                srd.word1.bits.BASE_ADDRESS_HI = HighPart(image.GetSubresource256BAddr(baseSubResId));
            }

            if (pBaseSubResInfo->flags.supportMetaDataTexFetch)
            {
                if (image.Parent()->IsDepthStencilTarget())
                {
                    if (TestAnyFlagSet(viewInfo.possibleLayouts.usages, LayoutShaderWrite) == false)
                    {
                        srd.word6.bits.COMPRESSION_EN = 1;
                        const gpusize htile256BAddrSwizzled = image.GetHtile256BAddrSwizzled();
                        srd.word7.bits.META_DATA_ADDRESS = LowPart(htile256BAddrSwizzled);
                        // This generation of HW didn't support > 40 bit addressing.
                        PAL_ASSERT(HighPart(htile256BAddrSwizzled) == 0);
                    }
                }
                else
                {
                    if (TestAnyFlagSet(viewInfo.possibleLayouts.usages, LayoutShaderWrite) == false)
                    {
                        srd.word6.bits.COMPRESSION_EN = 1;
                        // The color image's meta-data always points at the DCC surface.  Any existing cMask or fMask
                        // meta-data is only required for compressed texture fetches of MSAA surfaces, and that feature
                        // requires enabling an extension and use of an fMask image view.
                        const gpusize dcc256BAddrSwizzled = image.GetDcc256BAddrSwizzled(baseSubResId);
                        srd.word7.bits.META_DATA_ADDRESS  = LowPart(dcc256BAddrSwizzled);
                        // This generation of HW didn't support > 40 bit addressing.
                        PAL_ASSERT(HighPart(dcc256BAddrSwizzled) == 0);
                    }
                }
            } // end check for image supporting meta-data tex fetches
        }

        // Fill the unused 4 bits of word6 with sample pattern index
        SetImageViewSamplePatternIdx(&srd, viewInfo.samplePatternIdx);

        memcpy(&pSrds[i], &srd, sizeof(srd));
    }
}

// =====================================================================================================================
void Device::Gfx10SetImageSrdDims(
    sq_img_rsrc_t*  pSrd,
    uint32          width,
    uint32          height
    ) const
{
    constexpr uint32  WidthLowSize = 2;

    if (IsGfx10(*Parent()))
    {
        pSrd->gfx10.width_lo = (width - 1) & ((1 << WidthLowSize) - 1);
        pSrd->gfx10.width_hi = (width - 1) >> WidthLowSize;
        pSrd->gfx10.height   = (height - 1);
    }
#if PAL_BUILD_GFX11
    else
    {
        pSrd->gfx11.width_lo = (width - 1) & ((1 << WidthLowSize) - 1);
        pSrd->gfx11.width_hi = (width - 1) >> WidthLowSize;
        pSrd->gfx11.height   = (height - 1);
    }
#endif
}

// =====================================================================================================================
// Returns true if the supplied meta-data dimension (either width, height or depth) is compatible with the supplied
// parent image dimension of the same type.
static bool IsPrtPlusDimensionValid(
    uint32  parentDim,
    uint32  metaDataDim,
    uint32  requiredLodDim)
{
    const uint32  quotient = parentDim / metaDataDim;

    // Is the parent image an exact multiple larger than the meta-data image?
    return (((parentDim % metaDataDim) == 0) &&
            // Is the meta-data image an exact power of two smaller?
            Util::IsPowerOfTwo(quotient)     &&
            // Will the divisor size fit in four bits? (The available size in the SRD)
            (Util::Log2(quotient) < 4)       &&
            // Do the image dimensions match the size specified when the map image was created?
            ((requiredLodDim == 0) || (quotient == requiredLodDim)));
}

// =====================================================================================================================
// Error checks ImageViewInfo parameters for an image view SRD.
Result Device::HwlValidateImageViewInfo(
    const ImageViewInfo& info
    ) const
{
    const auto&  palDevice  = *Parent();
    const auto&  imageProps = palDevice.ChipProperties().imageProperties;
    const auto*  pImage     = static_cast<const Pal::Image*>(info.pImage);
    const auto&  createInfo = pImage->GetImageCreateInfo();
    const auto&  prtPlus    = createInfo.prtPlus;
    Result       result     = Result::Success;

    // Note that the Image::ValidateCreateInfo should have failed if this image doesn't support PRT+ features.
    if ((prtPlus.mapType == PrtMapType::None) && (info.mapAccess != PrtMapAccessType::Raw))
    {
        // If the image is not a PRT+ meta-data, then the map access has to be "raw".
        result = Result::ErrorInvalidValue;
    }
    else if ((TestAnyFlagSet(imageProps.prtFeatures, PrtFeatureFlags::PrtFeaturePrtPlus) == false) &&
             (info.mapAccess != PrtMapAccessType::Raw))
    {
        // If this device doesn't support PRT+, then the access must be set to raw.
        result = Result::ErrorInvalidValue;
    }
    else if (prtPlus.mapType != PrtMapType::None)
    {
        const auto* pPrtParentImg = static_cast<const Pal::Image*>(info.pPrtParentImg);

        // Ok, the supplied image is a PRT+ map image.
        if (info.mapAccess == PrtMapAccessType::Raw)
        {
            // If they're requesting raw access, then they should not have provided a parent image.
            if (pPrtParentImg != nullptr)
            {
                result = Result::ErrorInvalidImage;
            }
        }
        else if (pPrtParentImg != nullptr)
        {
            // They're requesting special access and we have a parent image.
            const auto&  parentCreateInfo = pPrtParentImg->GetImageCreateInfo();

            // Make sure the parent image is *not* another PRT+ meta-data surface
            if (parentCreateInfo.prtPlus.mapType == PrtMapType::None)
            {
                const auto&  mapExtent    = createInfo.extent;
                const auto&  parentExtent = parentCreateInfo.extent;
                const auto&  lodRegion    = createInfo.prtPlus.lodRegion;

                // The dimensions of the meta-data image need to be a power-of-two multiple of the parent image.
                // Verify that equivalency here.
                if (IsPrtPlusDimensionValid(parentExtent.width, mapExtent.width, lodRegion.width) == false)
                {
                    result = Result::ErrorInvalidImageWidth;
                }
                else if (IsPrtPlusDimensionValid(parentExtent.height, mapExtent.height, lodRegion.height) == false)
                {
                    result = Result::ErrorInvalidImageHeight;
                }
                else if (IsPrtPlusDimensionValid(parentExtent.depth, mapExtent.depth, lodRegion.depth) == false)
                {
                    result = Result::ErrorInvalidImageDepth;
                }
                else if ((createInfo.prtPlus.mapType == PrtMapType::SamplingStatus) &&
                         (info.mapAccess != PrtMapAccessType::WriteSamplingStatus))
                {
                    // Sampling status images can only be accessed via "raw" (checked above) or by the
                    // samspling-status specific access type.
                    result = Result::ErrorInvalidValue;
                }
                else if ((createInfo.prtPlus.mapType == PrtMapType::Residency) &&
                         (info.mapAccess == PrtMapAccessType::WriteSamplingStatus))
                {
                    // Likewise, residency map images can not be accessed via the sampling-status access type.
                    result = Result::ErrorInvalidValue;
                }
            }
            else
            {
                result = Result::ErrorInvalidImage;
            }
        }
    }

    return result;
}

// =====================================================================================================================
static Result VerifySlopeOffsetPair(
    int32  slope,
    int32  offset)
{
    // Valid offsets are 1/4 to 1/64.
    constexpr int32  LowValidOffset  = 2; // Log2(4)
    constexpr int32  HighValidOffset = 6; // Log2(64);

    // Assume bad parameters
    Result  result = Result::ErrorInvalidValue;

    if (((offset >= LowValidOffset) && (offset <= HighValidOffset)) &&
        // There are only 8 valid slope values.
        ((slope >= 0) && (slope <= 7)))
    {
        constexpr int32  Log2Sixteen = 4; // Log2(16) == 4
        constexpr int32  Log2Eight   = 3; // Log2(8) == 3

        // Ok, the supplied slope and offset values are valid, but note that some combinations of small slope
        // values with big offset values might bring discontinuity in interpolated LOD value as this combination
        // might prevent filtering weight to reach value of 1.0 at texel sampling center. The problem-free
        // combinations are:
        //      Slope value     Offset value
        //      2.5             <= 1/16
        //      3               <= 1/8
        //      4 or above      Any supported
        if (((slope == 0) && (offset >= Log2Sixteen)) ||  // 2.5 degrees
            ((slope == 1) && (offset >= Log2Eight))   ||  // 3 degrees
            (slope >= 2))                                 // 4 degrees or above, all offsets are valid
        {
            result = Result::Success;
        }
    }

    return result;
}

// =====================================================================================================================
Result Device::HwlValidateSamplerInfo(
    const SamplerInfo& samplerInfo
    )  const
{
    const auto*  pPalDevice      = Parent();
    const auto&  imageProperties = pPalDevice->ChipProperties().imageProperties;

    Result  result = Result::Success;

    // Residency map samplers have some specific restrictions; check those here.
    if (samplerInfo.flags.forResidencyMap != 0)
    {
        // Fail if the app tries to create a residency map sampler on a device that doesn't support residency maps.
        if (TestAnyFlagSet(Parent()->ChipProperties().imageProperties.prtFeatures,
                           PrtFeatureFlags::PrtFeaturePrtPlus) == false)
        {
            result = Result::ErrorUnavailable;
        }
        else if (samplerInfo.borderColorType == BorderColorType::PaletteIndex)
        {
            // Residency map samplers override the bits used for palete-index, so if both are specified then fail.
            result = Result::ErrorUnavailable;
        }

        if (result == Result::Success)
        {
            result = VerifySlopeOffsetPair(samplerInfo.uvSlope.x, samplerInfo.uvOffset.x);
        }

        if (result == Result::Success)
        {
            result = VerifySlopeOffsetPair(samplerInfo.uvSlope.y, samplerInfo.uvOffset.y);
        }
    }

    return result;
}

// =====================================================================================================================
// Update the supplied SRD to instead reflect certain parameters that are different between the "map" image and its
// parent image.
static void Gfx10UpdateLinkedResourceViewSrd(
    const Pal::Image* pParentImage, // Can be NULL for read access type
    const Image&      mapImage,
    const SubresId&   subResId,
    PrtMapAccessType  accessType,
    sq_img_rsrc_t*    pSrd)
{
    const auto& mapCreateInfo  = mapImage.Parent()->GetImageCreateInfo();

    sq_img_rsrc_linked_rsrc_t*  pLinkedRsrc      = reinterpret_cast<sq_img_rsrc_linked_rsrc_t*>(pSrd);

    // Without this, the other fields setup here have very different meanings.
    pLinkedRsrc->linked_resource = 1;

    // Sanity check that our sq_img_rsrc_linked_rsrc_t and sq_img_rsrc_t definitions line up.
    PAL_ASSERT(pSrd->gfx103CorePlus.linked_resource == 1);

    // "linked_resource_type" lines up with the "bc_swizzle" field of the sq_img_rsrc_t structure.
    // There are no enums for these values
    if (mapCreateInfo.prtPlus.mapType == PrtMapType::Residency)
    {
        switch (accessType)
        {
        case PrtMapAccessType::Read:
            pLinkedRsrc->linked_resource_type = 4;
            break;
        case PrtMapAccessType::WriteMin:
            pLinkedRsrc->linked_resource_type = 2;
            break;
        case PrtMapAccessType::WriteMax:
            pLinkedRsrc->linked_resource_type = 3;
            break;
        default:
            // What is this?
            PAL_ASSERT_ALWAYS();
            break;
        }
    }
    else if (mapCreateInfo.prtPlus.mapType == PrtMapType::SamplingStatus)
    {
        pLinkedRsrc->linked_resource_type = 1;
    }
    else
    {
        // What is this?
        PAL_ASSERT_ALWAYS();
    }

    if (pParentImage != nullptr)
    {
        const auto* pPalDevice          = pParentImage->GetDevice();
        const auto* pAddrMgr            = static_cast<const AddrMgr2::AddrMgr2*>(pPalDevice->GetAddrMgr());
        const auto& parentCreateInfo    = pParentImage->GetImageCreateInfo();
        const auto* pMapSubResInfo      = mapImage.Parent()->SubresourceInfo(subResId);
        const auto& parentExtent        = parentCreateInfo.extent;
        const auto& mapExtent           = mapCreateInfo.extent;
        const auto& mapSurfSetting      = mapImage.GetAddrSettings(pMapSubResInfo);
        constexpr uint32  BigPageShaderMask = Gfx10AllowBigPageShaderWrite | Gfx10AllowBigPageShaderRead;
        const bool  isBigPage           = IsImageBigPageCompatible(mapImage, BigPageShaderMask);

#if PAL_BUILD_GFX11
        if (IsGfx11(*pPalDevice))
        {
            // "big_page" was originally setup to reflect the big-page settings of the parent image, but it
            // needs to reflect the big-page setup of the map image instead.
            pLinkedRsrc->gfx11.big_page = isBigPage;

            // The "max_mip" field reflects the number of mip levels in the map image
            pLinkedRsrc->gfx11.max_mip  = mapCreateInfo.mipLevels - 1;
        }
        else
#endif
        {
            // "big_page" was originally setup to reflect the big-page settings of the parent image, but it
            // needs to reflect the big-page setup of the map image instead.
            pLinkedRsrc->gfx103.big_page = isBigPage;

            // The "max_mip" field reflects the number of mip levels in the map image
            pLinkedRsrc->gfx103.max_mip  = mapCreateInfo.mipLevels - 1;
        }

        // "xxx_scale" lines up with the "min_lod_warn" field of the sq_img_rsrc_t structure.
        pLinkedRsrc->width_scale  = Log2(parentExtent.width  / mapExtent.width);
        pLinkedRsrc->height_scale = Log2(parentExtent.height / mapExtent.height);
        pLinkedRsrc->depth_scale  = Log2(parentExtent.depth  / mapExtent.depth);

        // Most importantly, the base address points to the map image, not the parent image.
        pLinkedRsrc->base_address = mapImage.GetSubresource256BAddr(subResId);

        // As the linked resource image's memory is the one that is actually being accesed, the swizzle
        // mode needs to reflect that image, not the parent.
        pLinkedRsrc->sw_mode   = pAddrMgr->GetHwSwizzleMode(mapSurfSetting.swizzleMode);

        // Map images do support DCC, but for now...  no.  The map images are anticpated to be fairly small.
        PAL_ASSERT(mapImage.HasDccData() == false);

        // Note that the "compression_en" field was originally setup above based on the DCC status of the
        // parent image, so we need to force it off here to reflect that the map image won't have DCC.
        pLinkedRsrc->compression_en = 0;
    }
}

// =====================================================================================================================
void PAL_STDCALL Device::Gfx10CreateImageViewSrds(
    const IDevice*       pDevice,
    uint32               count,
    const ImageViewInfo* pImgViewInfo,
    void*                pOut)
{
    PAL_ASSERT((pDevice != nullptr) && (pOut != nullptr) && (pImgViewInfo != nullptr) && (count > 0));
    const auto*const pPalDevice = static_cast<const Pal::Device*>(pDevice);
    const auto*const pGfxDevice = static_cast<const Device*>(pPalDevice->GetGfxDevice());
    const auto*      pAddrMgr   = static_cast<const AddrMgr2::AddrMgr2*>(pPalDevice->GetAddrMgr());
    const auto&      chipProps  = pPalDevice->ChipProperties();
    const auto*const pFmtInfo   = MergedChannelFlatFmtInfoTbl(chipProps.gfxLevel,
                                                              &pPalDevice->GetPlatform()->PlatformSettings());
    const auto&      settings   = GetGfx9Settings(*pPalDevice);

    ImageSrd* pSrds = static_cast<ImageSrd*>(pOut);

    for (uint32 i = 0; i < count; ++i)
    {
        const ImageViewInfo&   viewInfo        = pImgViewInfo[i];
        PAL_ASSERT(viewInfo.subresRange.numPlanes == 1);

        // If the "image" is really a PRT+ mapping image, then we want to set up the majority of this
        // SRD off of the parent image, unless the client is indicating they want raw access to the
        // map image.
        const auto*const       pParent         = ((viewInfo.mapAccess == PrtMapAccessType::Raw)
                                                  ? static_cast<const Pal::Image*>(viewInfo.pImage)
                                                  : static_cast<const Pal::Image*>(viewInfo.pPrtParentImg));
        const Image&           image           = static_cast<const Image&>(*(pParent->GetGfxImage()));
        const Gfx9MaskRam*     pMaskRam        = image.GetPrimaryMaskRam(viewInfo.subresRange.startSubres.plane);
        const ImageInfo&       imageInfo       = pParent->GetImageInfo();
        const ImageCreateInfo& imageCreateInfo = pParent->GetImageCreateInfo();
        const ImageUsageFlags& imageUsageFlags = imageCreateInfo.usageFlags;
        const bool             imgIsBc         = Formats::IsBlockCompressed(imageCreateInfo.swizzledFormat.format);
        const bool             imgIsYuvPlanar  = Formats::IsYuvPlanar(imageCreateInfo.swizzledFormat.format);
        const auto             gfxLevel        = pPalDevice->ChipProperties().gfxLevel;
        sq_img_rsrc_t          srd             = {};
        const auto&            boundMem        = pParent->GetBoundGpuMemory();
        ChNumFormat            format          = viewInfo.swizzledFormat.format;

        SubresId baseSubResId   = { viewInfo.subresRange.startSubres.plane, 0, 0 };
        uint32   baseArraySlice = viewInfo.subresRange.startSubres.arraySlice;
        uint32   firstMipLevel  = viewInfo.subresRange.startSubres.mipLevel;
        uint32   mipLevels      = imageCreateInfo.mipLevels;

        PAL_ASSERT((viewInfo.possibleLayouts.engines != 0) && (viewInfo.possibleLayouts.usages != 0));

        if (imgIsYuvPlanar && (viewInfo.subresRange.numSlices == 1))
        {
            baseSubResId.arraySlice = baseArraySlice;
            baseArraySlice = 0;
        }

        bool  overrideBaseResource              = false;
        bool  overrideZRangeOffset              = false;
        bool  viewMipAsFullTexture              = false;
        bool  includePadding                    = (viewInfo.flags.includePadding != 0);
        const SubResourceInfo*const pSubResInfo = pParent->SubresourceInfo(baseSubResId);
        const auto&                 surfSetting = image.GetAddrSettings(pSubResInfo);

        // Validate subresource ranges
        const SubResourceInfo* pBaseSubResInfo  = pParent->SubresourceInfo(baseSubResId);

        Extent3d extent       = pBaseSubResInfo->extentTexels;
        Extent3d actualExtent = pBaseSubResInfo->actualExtentTexels;

        // The view should be in terms of texels except in four special cases when we're operating in terms of elements:
        // 1. Viewing a compressed image in terms of blocks. For BC images elements are blocks, so if the caller gave
        //    us an uncompressed view format we assume they want to view blocks.
        // 2. Copying to an "expanded" format (e.g., R32G32B32). In this case we can't do native format writes so we're
        //    going to write each element independently. The trigger for this case is a mismatched bpp.
        // 3. Viewing a YUV-packed image with a non-YUV-packed format when the view format is allowed for view formats
        //    with twice the bpp. In this case, the effective width of the view is half that of the base image.
        // 4. Viewing a YUV-planar Image which has multiple array slices. In this case, the texture hardware has no way
        //    to know about the padding in between array slices of the same plane (due to the other plane's slices being
        //    interleaved). In this case, we pad out the actual height of the view to span all planes (so that the view
        //    can access each array slice).
        //    This has the unfortunate side-effect of making normalized texture coordinates inaccurate.
        //    However, this is required for access to multiple slices.
        if (imgIsBc && (Formats::IsBlockCompressed(format) == false) &&
            (viewInfo.pImage->GetImageCreateInfo().prtPlus.mapType == PrtMapType::None))
        {
            // If we have the following image:
            //              Uncompressed pixels   Compressed block sizes (4x4)
            //      mip0:       22 x 22                   6 x 6
            //      mip1:       11 x 11                   3 x 3
            //      mip2:        5 x  5                   2 x 2
            //      mip3:        2 x  2                   1 x 1
            //      mip4:        1 x  1                   1 x 1
            //
            // On GFX10 the SRD is always programmed with the WIDTH and HEIGHT of the base level and the HW is
            // calculating the degradation of the block sizes down the mip-chain as follows (straight-up
            // divide-by-two integer math):
            //      mip0:  6x6
            //      mip1:  3x3
            //      mip2:  1x1
            //      mip3:  1x1
            //
            // This means that mip2 will be missing texels.
            //
            // Fix this by calculating the start mip's ceil(texels/blocks) width and height and then go up the chain
            // to pad the base mip's width and height to account for this.  A result lower than the base mip's
            // indicates a non-power-of-two texture, and the result should be clamped to its extentElements.
            // Otherwise, if the mip is aligned to block multiples, the result will be equal to extentElements.  If
            // there is no suitable width or height, the actualExtentElements is chosen.  The application is in
            // charge of making sure the math works out properly if they do this (allowed by Vulkan), otherwise we
            // assume it's an internal view and the copy shaders will prevent accessing out-of-bounds pixels.
            SubresId               mipSubResId    = { viewInfo.subresRange.startSubres.plane,
                                                      firstMipLevel,
                                                      baseArraySlice };
            const SubResourceInfo* pMipSubResInfo = pParent->SubresourceInfo(mipSubResId);

            extent.width  = Clamp((pMipSubResInfo->extentElements.width  << firstMipLevel),
                                  pBaseSubResInfo->extentElements.width,
                                  pBaseSubResInfo->actualExtentElements.width);
            extent.height = Clamp((pMipSubResInfo->extentElements.height << firstMipLevel),
                                  pBaseSubResInfo->extentElements.height,
                                  pBaseSubResInfo->actualExtentElements.height);
            if (AddrMgr2::IsNonBcViewCompatible(surfSetting.swizzleMode, imageCreateInfo.imageType)     &&
                (viewInfo.subresRange.numMips == 1)                                                     &&
                (viewInfo.subresRange.numSlices == 1)                                                   &&
                ((Max(1u, extent.width >> firstMipLevel) < pMipSubResInfo->extentElements.width) ||
                 (Max(1u, extent.height >> firstMipLevel) < pMipSubResInfo->extentElements.height)))

            {
                srd.base_address = image.ComputeNonBlockCompressedView(pBaseSubResInfo,
                                                                       pMipSubResInfo,
                                                                       &mipLevels,
                                                                       &firstMipLevel,
                                                                       &extent);
                baseArraySlice       = 0;
                viewMipAsFullTexture = true;
            }
            else
            {
                actualExtent = pBaseSubResInfo->actualExtentElements;
            }

            // It would appear that HW needs the actual extents to calculate the mip addresses correctly when
            // viewing more than 1 mip especially in the case of non power of two textures.
            if (viewInfo.subresRange.numMips > 1)
            {
                includePadding = true;
            }
        }
        else if ((pBaseSubResInfo->bitsPerTexel != Formats::BitsPerPixel(format)) &&
                 // For PRT+ map images, the format of the view is expected to be different
                 // from the format of the image itself.  Don't adjust the extents for PRT+ map images!
                 (viewInfo.pImage->GetImageCreateInfo().prtPlus.mapType == PrtMapType::None))
        {
            // The mismatched bpp checked is intended to catch the 2nd scenario in the above comment. However, YUV422
            // format also hit this. For YUV422 case, we need to apply widthScaleFactor to extent and acutalExtent.
            uint32 widthScaleFactor = 1;
            ChNumFormat imageFormat = imageCreateInfo.swizzledFormat.format;

            if (IsGfx10ImageFormatOverrideNeeded(imageCreateInfo, &imageFormat, &widthScaleFactor))
            {
                extent.width       /= widthScaleFactor;
                actualExtent.width /= widthScaleFactor;
            }
            else
            {
                extent       = pBaseSubResInfo->extentElements;
                actualExtent = pBaseSubResInfo->actualExtentElements;

                // For 96 bit bpp formats(X32Y32Z32_Uint/X32Y32Z32_Sint/X32Y32Z32_Float), X32_Uint formated image view
                // srd might be created upon the image for image copy operation. Extent of mipmaped level of X32_Uint
                // and mipmaped level of the original X32Y32Z32_* format might mismatch, especially on the last several
                // mips. Thus, it could be problematic to use 256b address of zero-th mip + mip level mode. Instead we
                // shall adopt 256b address of startsubres's miplevel/arrayLevel.
                if (pBaseSubResInfo->bitsPerTexel == 96)
                {
                    PAL_ASSERT(viewInfo.subresRange.numMips == 1);
                    mipLevels             = 1;
                    baseSubResId.mipLevel = firstMipLevel;
                    firstMipLevel         = 0;

                    // For gfx10 the baseSubResId should point to the baseArraySlice instead of setting the base_array
                    // SRD. When baseSubResId is used to calculate the baseAddress value, the current array slice will
                    // will be included in the equation.
                    PAL_ASSERT(viewInfo.subresRange.numSlices == 1);

                    // For gfx10 3d texture, we need to access per z slice instead subresource.
                    // Z slices are interleaved for mipmapped 3d texture. (each DepthPitch contains all the miplevels)
                    // example: the memory layout for a 3 miplevel WxHxD 3d texture:
                    // baseAddress(mip2) + DepthPitch * 0: subresource(mip2)'s 0 slice
                    // baseAddress(mip1) + DepthPitch * 0: subresource(mip1)'s 0 slice
                    // baseAddress(mip0) + DepthPitch * 0: subresource(mip0)'s 0 slice
                    // baseAddress(mip2) + DepthPitch * 1: subresource(mip2)'s 1 slice
                    // baseAddress(mip1) + DepthPitch * 1: subresource(mip1)'s 1 slice
                    // baseAddress(mip0) + DepthPitch * 1: subresource(mip0)'s 1 slice
                    // ...
                    // baseAddress(mip2) + DepthPitch * (D-1): subresource(mip2)'s D-1 slice
                    // baseAddress(mip1) + DepthPitch * (D-1): subresource(mip1)'s D-1 slice
                    // baseAddress(mip0) + DepthPitch * (D-1): subresource(mip0)'s D-1 slice
                    // When we try to view each subresource as 1 miplevel, we can't use srd.word5.bits.BASE_ARRAY to
                    // access each z slices since the srd for hardware can't compute the correct z slice stride.
                    // Instead we need a view to each slice.
                    if (imageCreateInfo.imageType == ImageType::Tex3d)
                    {
                        PAL_ASSERT((viewInfo.flags.zRangeValid == 1) && (viewInfo.zRange.extent == 1));
                        PAL_ASSERT(image.IsSubResourceLinear(baseSubResId));

                        overrideZRangeOffset    = viewInfo.flags.zRangeValid;
                    }
                    else
                    {
                        baseSubResId.arraySlice = baseArraySlice;
                    }

                    baseArraySlice       = 0;
                    overrideBaseResource = true;

                    pBaseSubResInfo = pParent->SubresourceInfo(baseSubResId);
                    extent          = pBaseSubResInfo->extentElements;
                    actualExtent    = pBaseSubResInfo->actualExtentElements;
                }
            }

            // When there is mismatched bpp and more than 1 mipLevels, it's possible to have missing texels like it
            // is to block compressed format. To compensate that, we set includePadding to true.
            if (imageCreateInfo.mipLevels > 1)
            {
                includePadding = true;
            }
        }
        else if (Formats::IsYuvPacked(pBaseSubResInfo->format.format) &&
                 (Formats::IsYuvPacked(format) == false)              &&
                 ((pBaseSubResInfo->bitsPerTexel << 1) == Formats::BitsPerPixel(format)))
        {
            // Changing how we interpret the bits-per-pixel of the subresource wreaks havoc with any tile swizzle
            // pattern used. This will only work for linear-tiled Images.
            PAL_ASSERT(image.IsSubResourceLinear(baseSubResId));

            extent.width       >>= 1;
            actualExtent.width >>= 1;
        }
        else if (Formats::IsYuvPlanar(imageCreateInfo.swizzledFormat.format))
        {
            if (viewInfo.subresRange.numSlices > 1)
            {
                image.PadYuvPlanarViewActualExtent(baseSubResId, &actualExtent);
                includePadding     = true;
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
        else if (Formats::IsMacroPixelPackedRgbOnly(imageCreateInfo.swizzledFormat.format) &&
                 (Formats::IsMacroPixelPackedRgbOnly(format) == false) &&
                 (imageCreateInfo.mipLevels > 1))
        {
            // If we have view format as X16 for MacroPixelPackedRgbOnly format.
            // We need a padding view for width need to be padding to even.
            //      mip0:  100x800
            //      mip1:  50x400
            //      mip2:  26x200
            //      mip3:  12x100
            //      mip4:  6x50
            //      mip5:  4x25
            //      mip6:  2x12
            //      mip7:  2x6
            //      mip8:  2x3
            //      mip9:  2x1   (may missing pixel if actual base extent.width < 2**10)
            // If we have missing pixel, we will do another following on copy by HwlImageToImageMissingPixelCopy()
            includePadding = true;
        }

        {
            // MIN_LOD field is unsigned
            constexpr uint32 Gfx9MinLodIntBits  = 4;
            constexpr uint32 Gfx9MinLodFracBits = 8;
            const     uint32 minLod             = Math::FloatToUFixed(viewInfo.minLod,
                                                                      Gfx9MinLodIntBits,
                                                                      Gfx9MinLodFracBits,
                                                                      true);

#if PAL_BUILD_GFX11
            if (IsGfx11(*pPalDevice))
            {
                srd.gfx11.min_lod_lo  = minLod & ((1 << 5) - 1);
                srd.gfx11.min_lod_hi  = minLod >> 5;
                srd.gfx104Plus.format = Formats::Gfx9::HwImgFmt(pFmtInfo, format);
            }
            else
#endif
            {
                srd.gfx10Core.min_lod = minLod;
                srd.gfx10Core.format  = Formats::Gfx9::HwImgFmt(pFmtInfo, format);
            }
        }

        // GFX10 does not support native 24-bit surfaces...  Clients promote 24-bit depth surfaces to 32-bit depth on
        // image creation.  However, they can request that border color data be clamped appropriately for the original
        // 24-bit depth.  Don't check for explicit depth surfaces here, as that only pertains to bound depth surfaces,
        // not to purely texture surfaces.
        //
        if ((imageCreateInfo.usageFlags.depthAsZ24 != 0) &&
            (Formats::ShareChFmt(format, ChNumFormat::X32_Uint)))
        {
            // This special format indicates to HW that this is a promoted 24-bit surface, so sample_c and border color
            // can be treated differently.
#if PAL_BUILD_GFX11
            if (false
#if PAL_BUILD_GFX11
                || IsGfx11(*pPalDevice)
#endif
               )
            {
                srd.gfx104Plus.format = IMG_FMT_32_FLOAT_CLAMP__GFX104PLUS;
            }
            else
#endif
            {
                srd.gfx10Core.format = IMG_FMT_32_FLOAT_CLAMP__GFX10CORE;
            }
        }

        const Extent3d programmedExtent = (includePadding) ? actualExtent : extent;
        pGfxDevice->Gfx10SetImageSrdDims(&srd, programmedExtent.width, programmedExtent.height);

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
            texOptLevel = static_cast<const Pal::Device*>(pDevice)->Settings().textureOptLevel;
            break;
        }

        PAL_ASSERT(texOptLevel < ArrayLen(PanelToTexPerfMod));

        TexPerfModulation perfMod = PanelToTexPerfMod[texOptLevel];

        srd.perf_mod = static_cast<uint32>(perfMod);

        // Destination swizzles come from the view creation info, rather than the format of the view.
        srd.dst_sel_x = Formats::Gfx9::HwSwizzle(viewInfo.swizzledFormat.swizzle.r);
        srd.dst_sel_y = Formats::Gfx9::HwSwizzle(viewInfo.swizzledFormat.swizzle.g);
        srd.dst_sel_z = Formats::Gfx9::HwSwizzle(viewInfo.swizzledFormat.swizzle.b);
        srd.dst_sel_w = Formats::Gfx9::HwSwizzle(viewInfo.swizzledFormat.swizzle.a);

        // When view3dAs2dArray is enabled for 3d image, we'll use the same mode for writing and viewing
        // according to the doc, so we don't need to change it here.
        srd.sw_mode   = pAddrMgr->GetHwSwizzleMode(surfSetting.swizzleMode);

        const bool isMultiSampled = (imageCreateInfo.samples > 1);

        // NOTE: Where possible, we always assume an array view type because we don't know how the shader will
        // attempt to access the resource.
        const ImageViewType  viewType = GetViewType(viewInfo);
        switch (viewType)
        {
        case ImageViewType::Tex1d:
            srd.type = ((imageCreateInfo.arraySize == 1) ? SQ_RSRC_IMG_1D : SQ_RSRC_IMG_1D_ARRAY);
            break;
        case ImageViewType::Tex2d:
            // A 3D image with view3dAs2dArray enabled can be accessed via 2D image view too, it needs 2D_ARRAY type.
            srd.type = (((imageCreateInfo.arraySize == 1) && (imageCreateInfo.imageType != ImageType::Tex3d))
                        ? (isMultiSampled ? SQ_RSRC_IMG_2D_MSAA       : SQ_RSRC_IMG_2D)
                        : (isMultiSampled ? SQ_RSRC_IMG_2D_MSAA_ARRAY : SQ_RSRC_IMG_2D_ARRAY));
            break;
        case ImageViewType::Tex3d:
            srd.type = SQ_RSRC_IMG_3D;
            break;
        case ImageViewType::TexCube:
            srd.type = SQ_RSRC_IMG_CUBE;
            break;
        default:
            PAL_ASSERT_ALWAYS();
            break;
        }

        uint32  maxMipField = 0;
        if (isMultiSampled)
        {
            // MSAA textures cannot be mipmapped; the LAST_LEVEL and MAX_MIP fields indicate the texture's
            // sample count.  According to the docs, these are samples.  According to reality, this is
            // fragments.  I'm going with reality.
            srd.base_level = 0;
            srd.last_level = Log2(imageCreateInfo.fragments);
            maxMipField    = Log2(imageCreateInfo.fragments);
        }
        else
        {
            srd.base_level = firstMipLevel;
            srd.last_level = firstMipLevel + viewInfo.subresRange.numMips - 1;
            maxMipField    = mipLevels - 1;
        }

#if PAL_BUILD_GFX11
        if (false
#if PAL_BUILD_GFX11
             || IsGfx11(*pPalDevice)
#endif
            )
        {
            srd.gfx104Plus.max_mip = maxMipField;
        }
        else
#endif
        {
            srd.gfx10Core.max_mip = maxMipField;
        }

        uint32 depth = 0;

        // From reg spec: Units are "depth - 1", so 0 = 1 slice, 1= 2 slices.
        // If the image type is 3D, then the DEPTH field is the image's depth - 1.
        // Otherwise, the DEPTH field replaces the old "last_array" field.

        // Note that we can't use viewInfo.viewType here since 3D image may be viewed as 2D (array).
        if (imageCreateInfo.imageType == ImageType::Tex3d)
        {
            if (viewInfo.flags.zRangeValid == 1)
            {
                // For 3D, bit 0 indicates SRV or UAV:
                //   0: SRV (base_array ignored, depth w.r.t. base map)
                //   1: UAV (base_array and depth are first and last layer in view, and w.r.t. mip level specified)
                //
                // "base_array" and "depth" specify the range of 3D slices that can be read from.  Both of these fields
                // are setup (below) to what the client expects based on the "viewInfo" paramters, so we always want the
                // HW to obey them, so we always set the LSB of "array_pitch".
                //
                // For non-3D images, the "array_pitch" field is only meaningful for quilts, which we do not support.
                // Since zRange is set valid here, UAV should be used.
                srd.gfx10CorePlus.array_pitch = 1;
                baseArraySlice = viewInfo.zRange.offset;
                // If the client is specifying a valid Z range, the depth of the SRD must include the range's offset
                // and extent. Furthermore, the Z range is specified in terms of the view's first mip level, not the
                // Image's base mip level. Since it is UAV, so the hardware accepts depth in the current
                // mip level.
                depth = ((viewInfo.zRange.offset + viewInfo.zRange.extent) - 1);
                // if the image is a 96-bit image, since we treat it as a 32-bit image with three times the width and
                // compute the depth pitch in memory ourselves, we force the view to view one slice at a time, and set
                // baseArraySlice to 0 for the computation of depth pitch.
                if (overrideZRangeOffset)
                {
                    baseArraySlice = 0;
                    depth = 0;
                }
            }
            else
            {
                // Since zRange is not enabled, SRV should be used.
                srd.gfx10CorePlus.array_pitch = 0;
                // Using the depth of the base mip level for SRV according to the documentation.
                depth = (pBaseSubResInfo ->extentTexels.depth - 1);
            }
        }
        else
        {

            // For gfx9, there is no longer a separate last_array parameter  for arrays. Instead the "depth" input is
            // used as the last_array parameter. For cubemaps, depth is no longer interpreted as the number of full cube
            // maps (6 faces), but strictly as the number of array slices. It is up to driver to make sure depth-base is
            // modulo 6 for cube maps.
            depth = (viewInfo.subresRange.startSubres.arraySlice + viewInfo.subresRange.numSlices - 1);
        }

#if PAL_BUILD_GFX11
        if (IsGfx11(*pPalDevice))
        {
            srd.gfx11.depth = depth;
        }
        else
#endif
        {
            srd.gfx10.depth = depth;
        }

        // (pitch-1)[12:0] of mip 0 for 1D, 2D and 2D MSAA in GFX10.3+, if pitch > width,
        // we aren't treating mip1+ as the base image, and TA_CNTL_AUX.DEPTH_AS_WIDTH_DIS = 0
        const uint32  bytesPerPixel = Formats::BytesPerPixel(format);
        const uint32  pitchInPixels = imageCreateInfo.rowPitch / bytesPerPixel;
        if ((pitchInPixels > programmedExtent.width) &&
            (viewMipAsFullTexture == false) &&
            ((srd.type == SQ_RSRC_IMG_1D) ||
             (srd.type == SQ_RSRC_IMG_2D) ||
             (srd.type == SQ_RSRC_IMG_2D_MSAA)))
        {
            if (IsGfx103(*pPalDevice)
                )
            {
                srd.gfx10.depth = pitchInPixels - 1;
            }
#if PAL_BUILD_GFX11
            else if (IsGfx11(*pPalDevice))
            {
                srd.gfx11.depth = (pitchInPixels - 1);
                srd.gfx11.pitch_13 = (pitchInPixels - 1) >> 13;
            }
#endif
        }

        if (pPalDevice->MemoryProperties().flags.supportsMall != 0)
        {
            const uint32  llcNoAlloc = CalcLlcNoalloc(viewInfo.flags.bypassMallRead,
                                                      viewInfo.flags.bypassMallWrite);
#if PAL_BUILD_GFX11
            if (IsGfx11(*pPalDevice))
            {
                srd.gfx104Plus.llc_noalloc = llcNoAlloc;
            }
            else
#endif
            {
                // The SRD has a two-bit field where the high-bit is the control for "read" operations
                // and the low bit is the control for bypassing the MALL on write operations.
                srd.gfx103.llc_noalloc = llcNoAlloc;
            }
        }

        srd.bc_swizzle = GetBcSwizzle(imageCreateInfo);

        if (IsGfx10(*pPalDevice))
        {
            srd.gfx10.base_array   = baseArraySlice;
        }
#if PAL_BUILD_GFX11
        else if (IsGfx11(*pPalDevice))
        {
            srd.gfx11.base_array   = baseArraySlice;
        }
#endif

        srd.meta_pipe_aligned  = ((pMaskRam != nullptr) ? pMaskRam->PipeAligned() : 0);
        srd.corner_samples     = imageCreateInfo.usageFlags.cornerSampling;
        srd.iterate_256        = image.GetIterate256(pSubResInfo);

        // Depth images obviously don't have an alpha component, so don't bother...
        if ((pParent->IsDepthStencilTarget() == false) && pBaseSubResInfo->flags.supportMetaDataTexFetch)
        {
            // The setup of the compression-related fields requires knowing the bound memory and the expected
            // usage of the memory (read or write), so defer most of the setup to "WriteDescriptorSlot".
            const SurfaceSwap surfSwap = Formats::Gfx9::ColorCompSwap(imageCreateInfo.swizzledFormat);

            // If single-component color format such as COLOR_8/16/32
            //    set AoMSB=1 when comp_swap=11
            //    set AoMSB=0 when comp_swap=others
            // Follow the legacy way of setting AoMSB for other color formats
            if (Formats::NumComponents(imageCreateInfo.swizzledFormat.format) == 1)
            {
                srd.alpha_is_on_msb = ((surfSwap == SWAP_ALT_REV) ? 1 : 0);
            }
            else if ((surfSwap != SWAP_STD_REV) && (surfSwap != SWAP_ALT_REV))
            {
                srd.alpha_is_on_msb = 1;
            }
        }

        if (boundMem.IsBound())
        {
            const Gfx10AllowBigPage bigPageUsage  = imageCreateInfo.usageFlags.shaderWrite
                                                           ? Gfx10AllowBigPageShaderWrite
                                                           : Gfx10AllowBigPageShaderRead;
            const uint32            bigPageCompat = IsImageBigPageCompatible(image, bigPageUsage);

#if PAL_BUILD_GFX11
            if (false
#if PAL_BUILD_GFX11
                || IsGfx11(*pPalDevice)
#endif
                )
            {
                srd.gfx104Plus.big_page = bigPageCompat;
            }
            else
#endif
            {
                srd.gfx10Core.big_page  = bigPageCompat;
            }

            // When overrideBaseResource = true (96bpp images), compute baseAddress using the mip/slice in
            // baseSubResId.
            if ((imgIsYuvPlanar && (viewInfo.subresRange.numSlices == 1)) || overrideBaseResource)
            {
                const gpusize gpuVirtAddress = pParent->GetSubresourceBaseAddr(baseSubResId);
                const auto*   pTileInfo      = AddrMgr2::GetTileInfo(pParent, baseSubResId);
                const gpusize pipeBankXor    = pTileInfo->pipeBankXor;
                gpusize addrWithXor          = gpuVirtAddress | (pipeBankXor << 8);

                if (overrideZRangeOffset)
                {
                    addrWithXor += viewInfo.zRange.offset * pBaseSubResInfo->depthPitch;
                }

                srd.base_address = addrWithXor >> 8;
            }
            else if (srd.base_address == 0)
            {
                srd.base_address = image.GetSubresource256BAddr(baseSubResId);
            }

            if (pBaseSubResInfo->flags.supportMetaDataTexFetch)
            {
                srd.compression_en = 1;

                if (image.Parent()->IsDepthStencilTarget())
                {
                    srd.meta_data_address = image.GetHtile256BAddr();
                }
                else
                {
                    const auto& dccControl = image.GetDcc(viewInfo.subresRange.startSubres.plane)->GetControlReg();

                    // The color image's meta-data always points at the DCC surface.  Any existing cMask or fMask
                    // meta-data is only required for compressed texture fetches of MSAA surfaces, and that feature
                    // requires enabling an extension and use of an fMask image view.
#if PAL_BUILD_GFX11
                    //
                    // GFX11 changes the rules as to whether or not the "swizzle" value associated with this surface
                    // applies to the SRD or not.
                    if (IsGfx11(*pPalDevice))
                    {
                        srd.meta_data_address = image.GetDcc256BAddrSwizzled(baseSubResId);
                    }
                    else
#endif
                    {
                        srd.meta_data_address = image.GetDcc256BAddr(baseSubResId);
                    }

                    srd.max_compressed_block_size   = dccControl.bits.MAX_COMPRESSED_BLOCK_SIZE;

                    srd.max_uncompressed_block_size = dccControl.bits.MAX_UNCOMPRESSED_BLOCK_SIZE;

                    // In GFX10, there is a feature called compress-to-constant which automatically enocde A0/1
                    // C0/1 in DCC key if it detected the whole 256Byte of data are all 0s or 1s for both alpha
                    // channel and color channel. However, this does not work well with format replacement in PAL.
                    // When a format changes from with-alpha-format to without-alpha-format, HW may incorrectly
                    // encode DCC key if compress-to-constant is triggered. In PAL, format is only replaceable
                    // when DCC is in decompressed state.  Therefore, we have the choice to not enable compressed
                    // write and simply write the surface and allow it to stay in expanded state.
                    // Additionally, HW will encode the DCC key in a manner that is incompatible with the app's
                    // understanding of the surface if the format for the SRD differs from the surface's format.
                    // If the format isn't DCC compatible, we need to disable compressed writes.
                    const DccFormatEncoding encoding =
                        pGfxDevice->ComputeDccFormatEncoding(imageCreateInfo.swizzledFormat,
                                                             &viewInfo.swizzledFormat,
                                                             1);
                    if ((encoding != DccFormatEncoding::Incompatible) &&
                        ImageLayoutCanCompressColorData(image.LayoutToColorCompressionState(),
                                                        viewInfo.possibleLayouts))
                    {
                        srd.color_transform       = dccControl.bits.COLOR_TRANSFORM;
                        srd.write_compress_enable = 1;
                    }
                }
            } // end check for image supporting meta-data tex fetches
        }

        {
            if (IsGfx10(*pPalDevice))
            {
                srd.gfx10Core.resource_level = 1;

                // Fill the unused 4 bits of word6 with sample pattern index
                srd.gfx10._reserved_206_203  = viewInfo.samplePatternIdx;
            }
#if PAL_BUILD_GFX11
            else if (IsGfx11(*pPalDevice))
            {
                srd.gfx11.sample_pattern_offset = viewInfo.samplePatternIdx;
            }
#endif

            //   PRT unmapped returns 0.0 or 1.0 if this bit is 0 or 1 respectively
            //   Only used with image ops (sample/load)
            srd.gfx10CorePlus.prt_default = 0;
        }

        if (viewInfo.mapAccess != PrtMapAccessType::Raw)
        {
            Gfx10UpdateLinkedResourceViewSrd(static_cast<const Pal::Image*>(viewInfo.pPrtParentImg),
                                             *GetGfx9Image(viewInfo.pImage),
                                             baseSubResId,
                                             viewInfo.mapAccess,
                                             &srd);
        }

        memcpy(&pSrds[i], &srd, sizeof(srd));
    }
}

// =====================================================================================================================
// Gfx9+ specific function for creating fmask view SRDs. Installed in the function pointer table of the parent device
// during initialization.
void PAL_STDCALL Device::CreateFmaskViewSrds(
    const IDevice*        pDevice,
    uint32                count,
    const FmaskViewInfo*  pFmaskViewInfo,
    void*                 pOut)
{
    PAL_ASSERT((pDevice != nullptr) && (pOut != nullptr) && (pFmaskViewInfo != nullptr) && (count > 0));

    const Pal::Device*  pPalDevice = static_cast<const Pal::Device*>(pDevice);

    if (pPalDevice->ChipProperties().srdSizes.fmaskView != 0)
    {
        const Device* pGfxDevice = static_cast<const Device*>(pPalDevice->GetGfxDevice());

        pGfxDevice->CreateFmaskViewSrdsInternal(count, pFmaskViewInfo, nullptr, pOut);
    }
    else
    {
        // Why are we trying to get a fMask SRD on a device that doesn't support fMask?
        PAL_ASSERT_ALWAYS();
    }
}

// =====================================================================================================================
// GFX9-specific function to create an fmask-specific SRD.  If internal info is not required pFmaskViewInternalInfo can
// be set to null, otherwise it must be a pointer to a valid internal-info structure.
void Device::Gfx9CreateFmaskViewSrdsInternal(
    const FmaskViewInfo&          viewInfo,
    const FmaskViewInternalInfo*  pFmaskViewInternalInfo,
    Gfx9ImageSrd*                 pSrd
    ) const
{
    const bool             hasInternalInfo = (pFmaskViewInternalInfo != nullptr);
    const SubresId         slice0Id        = {};
    const Image&           image           = *GetGfx9Image(viewInfo.pImage);
    const Gfx9Fmask*const  pFmask          = image.GetFmask();
    const auto*const       pParent         = static_cast<const Pal::Image*>(viewInfo.pImage);
    const auto*            pPalDevice      = pParent->GetDevice();
    const Device*          pGfxDevice      = static_cast<const Device*>(pPalDevice->GetGfxDevice());
    const auto*            pAddrMgr        = static_cast<const AddrMgr2::AddrMgr2*>(pPalDevice->GetAddrMgr());
    const ImageCreateInfo& createInfo      = pParent->GetImageCreateInfo();
    const bool             isUav           = (hasInternalInfo && (pFmaskViewInternalInfo->flags.fmaskAsUav == 1));
    const SubResourceInfo& subresInfo      = *pParent->SubresourceInfo(slice0Id);
    const auto*            pAddrOutput     = image.GetAddrOutput(&subresInfo);
    const Gfx9Fmask&       fmask           = *image.GetFmask();
    const Gfx9Cmask*       pCmask          = image.GetCmask();
    const auto&            fMaskAddrOut    = fmask.GetAddrOutput();

    PAL_ASSERT(createInfo.extent.depth == 1);
    PAL_ASSERT(image.HasFmaskData());

    // For Fmask views, the format is based on the sample and fragment counts.
    pSrd->word1                 = fmask.Gfx9FmaskFormat(createInfo.samples, createInfo.fragments, isUav);
    pSrd->word1.bits.MIN_LOD    = 0;

    pSrd->word2.bits.WIDTH    = (subresInfo.extentTexels.width  - 1);
    pSrd->word2.bits.HEIGHT   = (subresInfo.extentTexels.height - 1);
    pSrd->word2.bits.PERF_MOD = 0;

    // For Fmask views, destination swizzles are based on the bit depth of the Fmask buffer.
    pSrd->word3.bits.DST_SEL_X    = SQ_SEL_X;
    pSrd->word3.bits.DST_SEL_Y    = (fMaskAddrOut.bpp == 64) ? SQ_SEL_Y : SQ_SEL_0;
    pSrd->word3.bits.DST_SEL_Z    = SQ_SEL_0;
    pSrd->word3.bits.DST_SEL_W    = SQ_SEL_0;
    // Program "type" based on the image's physical dimensions, not the dimensions of the view
    pSrd->word3.bits.TYPE         = ((createInfo.arraySize > 1) ? SQ_RSRC_IMG_2D_ARRAY : SQ_RSRC_IMG_2D);
    pSrd->word3.bits.BASE_LEVEL   = 0;
    pSrd->word3.bits.LAST_LEVEL   = 0;
    pSrd->word3.bits.SW_MODE      = pAddrMgr->GetHwSwizzleMode(pFmask->GetSwizzleMode());

    // On GFX9, "depth" replaces the deprecated "last_array" from pre-GFX9 ASICs.
    pSrd->word4.bits.DEPTH = (viewInfo.baseArraySlice + viewInfo.arraySize - 1);
    pSrd->word4.bits.PITCH = fMaskAddrOut.pitch - 1;

    pSrd->word5.bits.BASE_ARRAY        = viewInfo.baseArraySlice;
    pSrd->word5.bits.ARRAY_PITCH       = 0; // msaa surfaces don't support texture quilting
    pSrd->word5.bits.META_LINEAR       = 0; // linear meta-surfaces aren't supported in gfx9
    pSrd->word5.bits.META_PIPE_ALIGNED = pCmask->PipeAligned();
    pSrd->word5.bits.META_RB_ALIGNED   = pGfxDevice->IsRbAligned();
    pSrd->word5.bits.MAX_MIP           = 0;

    if (image.Parent()->GetBoundGpuMemory().IsBound())
    {
        // Need to grab the most up-to-date GPU virtual address for the underlying FMask object.
        const gpusize fmask256BAddrSwizzled = image.GetFmask256BAddrSwizzled();
        pSrd->word0.bits.BASE_ADDRESS       = LowPart(fmask256BAddrSwizzled);
        pSrd->word1.bits.BASE_ADDRESS_HI    = HighPart(fmask256BAddrSwizzled);

        // Does this image has an associated FMask which is shader Readable? if FMask needs to be
        // read in the shader CMask has to be read as FMask meta data
        if (image.IsComprFmaskShaderReadable(slice0Id))
        {
            pSrd->word6.bits.COMPRESSION_EN = (viewInfo.flags.shaderWritable == 0);

            if (viewInfo.flags.shaderWritable == 0)
            {
                // word7 contains bits 8-39 of the meta-data surface.  For fMask,the meta-surface is cMask.
                // word5 contains bits 40-47.
                const gpusize cmask256BAddrSwizzled = image.GetCmask256BAddrSwizzled();
                pSrd->word7.bits.META_DATA_ADDRESS  = LowPart(cmask256BAddrSwizzled);
                pSrd->word5.bits.META_DATA_ADDRESS  = HighPart(cmask256BAddrSwizzled);
            }
        }
    }
}

// =====================================================================================================================
// GFX10-specific function to create an fmask-specific SRD.  If internal info is not required pFmaskViewInternalInfo
// can be set to null, otherwise it must be a pointer to a valid internal-info structure.
void Device::Gfx10CreateFmaskViewSrdsInternal(
    const FmaskViewInfo&          viewInfo,
    const FmaskViewInternalInfo*  pFmaskViewInternalInfo,
    sq_img_rsrc_t*                pSrd
    ) const
{
    const bool             hasInternalInfo = (pFmaskViewInternalInfo != nullptr);
    const SubresId         slice0Id        = {};
    const Image&           image           = *GetGfx9Image(viewInfo.pImage);
    const Gfx9Fmask*const  pFmask          = image.GetFmask();
    const Gfx9Cmask*       pCmask          = image.GetCmask();
    const auto*const       pParent         = static_cast<const Pal::Image*>(viewInfo.pImage);
    const auto*            pPalDevice      = pParent->GetDevice();
    const auto*            pAddrMgr        = static_cast<const AddrMgr2::AddrMgr2*>(pPalDevice->GetAddrMgr());
    const ImageCreateInfo& createInfo      = pParent->GetImageCreateInfo();
    const bool             isUav           = (hasInternalInfo && (pFmaskViewInternalInfo->flags.fmaskAsUav == 1));
    const SubResourceInfo& subresInfo      = *pParent->SubresourceInfo(slice0Id);
    const auto*            pAddrOutput     = image.GetAddrOutput(&subresInfo);
    const Gfx9Fmask&       fmask           = *image.GetFmask();
    const auto&            fMaskAddrOut    = fmask.GetAddrOutput();
    const uint32           bigPageCompat   = IsFmaskBigPageCompatible(image, Gfx10AllowBigPageShaderRead);

    PAL_ASSERT(createInfo.extent.depth == 1);
    PAL_ASSERT(image.HasFmaskData());

    // For Fmask views, the format is based on the sample and fragment counts.
    {
        pSrd->gfx10Core.format  = fmask.Gfx10FmaskFormat(createInfo.samples, createInfo.fragments, isUav);

        pSrd->gfx10Core.min_lod = 0;
        pSrd->gfx10Core.max_mip = 0;

        pSrd->gfx10Core.resource_level = 1;

        pSrd->gfx10Core.big_page       = bigPageCompat;
    }

    Gfx10SetImageSrdDims(pSrd, subresInfo.extentTexels.width, subresInfo.extentTexels.height);
    pSrd->perf_mod = 0;

    // For Fmask views, destination swizzles are based on the bit depth of the Fmask buffer.
    pSrd->dst_sel_x    = SQ_SEL_X;
    pSrd->dst_sel_y    = (fMaskAddrOut.bpp == 64) ? SQ_SEL_Y : SQ_SEL_0;
    pSrd->dst_sel_z    = SQ_SEL_0;
    pSrd->dst_sel_w    = SQ_SEL_0;
    // Program "type" based on the image's physical dimensions, not the dimensions of the view
    pSrd->type         = ((createInfo.arraySize > 1) ? SQ_RSRC_IMG_2D_ARRAY : SQ_RSRC_IMG_2D);
    pSrd->base_level   = 0;
    pSrd->last_level   = 0;
    pSrd->sw_mode      = pAddrMgr->GetHwSwizzleMode(pFmask->GetSwizzleMode());

    // On GFX10, "depth" replaces the deprecated "last_array" from pre-GFX9 ASICs.
    pSrd->gfx10.depth = (viewInfo.baseArraySlice + viewInfo.arraySize - 1);

    pSrd->gfx10.base_array  = viewInfo.baseArraySlice;
    pSrd->meta_pipe_aligned = pCmask->PipeAligned();

    if (image.Parent()->GetBoundGpuMemory().IsBound())
    {
        // Need to grab the most up-to-date GPU virtual address for the underlying FMask object.
        pSrd->base_address = image.GetFmask256BAddr();

        // Does this image has an associated FMask which is shader Readable? if FMask needs to be
        // read in the shader CMask has to be read as FMask meta data
        if ((image.IsComprFmaskShaderReadable(slice0Id)) &&
            // The "isUav" flag is basically used to indicate that RPM is going to write into the fMask surface by
            // itself.  If "compression_en=1", then the HW will try to update the cMask memory to "uncompressed
            // state", which is NOT what we want.  We want fmask updated and cmask left alone:
            (isUav == false))
        {
            // Does this image has an associated FMask which is shader Readable? if FMask needs to be
            // read in the shader CMask has to be read as FMask meta data
            pSrd->compression_en = 1;

            // For fMask,the meta-surface is cMask.
            pSrd->meta_data_address = image.GetCmask256BAddr();
        }
    }

}

// =====================================================================================================================
// Creates 'count' fmask view SRDs. If internal info is not required pFmaskViewInternalInfo can be set to null,
// otherwise it must be an array of 'count' internal info structures.
void Device::CreateFmaskViewSrdsInternal(
    uint32                       count,
    const FmaskViewInfo*         pFmaskViewInfo,
    const FmaskViewInternalInfo* pFmaskViewInternalInfo,
    void*                        pOut
    ) const
{
    ImageSrd*  pSrds = static_cast<ImageSrd*>(pOut);

    for (uint32 i = 0; i < count; ++i)
    {
        const FmaskViewInternalInfo* pInternalInfo = ((pFmaskViewInternalInfo != nullptr)
                                                      ? &pFmaskViewInternalInfo[i]
                                                      : nullptr);
        const FmaskViewInfo&         viewInfo      = pFmaskViewInfo[i];
        const Image&                 image         = *GetGfx9Image(viewInfo.pImage);
        const Gfx9Fmask* const       pFmask        = image.GetFmask();

        if (pFmask != nullptr)
        {
            ImageSrd srd = {};

            if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
            {
                Gfx9CreateFmaskViewSrdsInternal(viewInfo, pInternalInfo, &srd.gfx9);
            }
            else if (IsGfx10(m_gfxIpLevel))
            {
                Gfx10CreateFmaskViewSrdsInternal(viewInfo, pInternalInfo, &srd.gfx10);
            }
            else
            {
                PAL_ASSERT_ALWAYS();
            }

            pSrds[i] = srd;
        }
        else
        {
            memcpy(pSrds + i, Parent()->ChipProperties().nullSrds.pNullFmaskView, sizeof(ImageSrd));
        }
    }
}

// =====================================================================================================================
// Gfx9 specific function for creating sampler SRDs. Installed in the function pointer table of the parent device
// during initialization.
void PAL_STDCALL Device::Gfx9CreateSamplerSrds(
    const IDevice*     pDevice,
    uint32             count,
    const SamplerInfo* pSamplerInfo,
    void*              pOut)
{
    PAL_ASSERT((pDevice != nullptr) && (pOut != nullptr) && (pSamplerInfo != nullptr) && (count > 0));
    const Device* pGfxDevice = static_cast<const Device*>(static_cast<const Pal::Device*>(pDevice)->GetGfxDevice());

    const Gfx9PalSettings& settings       = GetGfx9Settings(*(pGfxDevice->Parent()));
    constexpr uint32       SamplerSrdSize = sizeof(SamplerSrd);

    constexpr uint32 NumTemporarySamplerSrds                  = 32;
    SamplerSrd       tempSamplerSrds[NumTemporarySamplerSrds] = {};
    uint32           srdsBuilt                                = 0;

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
            Gfx9SamplerSrd*    pSrd  = &tempSamplerSrds[currentSrdIdx].gfx9;

            const SQ_TEX_ANISO_RATIO maxAnisoRatio = GetAnisoRatio(*pInfo);

            pSrd->word0.bits.CLAMP_X            = GetAddressClamp(pInfo->addressU);
            pSrd->word0.bits.CLAMP_Y            = GetAddressClamp(pInfo->addressV);
            pSrd->word0.bits.CLAMP_Z            = GetAddressClamp(pInfo->addressW);
            pSrd->word0.bits.MAX_ANISO_RATIO    = maxAnisoRatio;
            pSrd->word0.bits.DEPTH_COMPARE_FUNC = static_cast<uint32>(pInfo->compareFunc);
            pSrd->word0.bits.FORCE_UNNORMALIZED = pInfo->flags.unnormalizedCoords;
            pSrd->word0.bits.TRUNC_COORD        = pInfo->flags.truncateCoords;
            pSrd->word0.bits.DISABLE_CUBE_WRAP  = (pInfo->flags.seamlessCubeMapFiltering == 1) ? 0 : 1;

            constexpr uint32 Gfx9SamplerLodMinMaxIntBits  = 4;
            constexpr uint32 Gfx9SamplerLodMinMaxFracBits = 8;
            pSrd->word1.bits.MIN_LOD = Math::FloatToUFixed(pInfo->minLod,
                                                           Gfx9SamplerLodMinMaxIntBits,
                                                           Gfx9SamplerLodMinMaxFracBits);
            pSrd->word1.bits.MAX_LOD = Math::FloatToUFixed(pInfo->maxLod,
                                                           Gfx9SamplerLodMinMaxIntBits,
                                                           Gfx9SamplerLodMinMaxFracBits);

            constexpr uint32 Gfx9SamplerLodBiasIntBits  = 6;
            constexpr uint32 Gfx9SamplerLodBiasFracBits = 8;

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
                                                                 Gfx9SamplerLodBiasIntBits,
                                                                 Gfx9SamplerLodBiasFracBits);

            pSrd->word2.bits.BLEND_ZERO_PRT     = pInfo->flags.prtBlendZeroMode;
            pSrd->word2.bits.MIP_POINT_PRECLAMP = 0;
            pSrd->word2.bits.FILTER_PREC_FIX    = settings.samplerPrecisionFixEnabled;

            // Ensure useAnisoThreshold is only set when preciseAniso is disabled
            PAL_ASSERT((pInfo->flags.preciseAniso == 0) ||
                       ((pInfo->flags.preciseAniso == 1) && (pInfo->flags.useAnisoThreshold == 0)));

            if (pInfo->flags.preciseAniso == 0)
            {
                // Setup filtering optimization levels: these will be modulated by the global filter
                // optimization aggressiveness, which is controlled by the "TFQ" public setting.
                // NOTE: Aggressiveness of optimizations is influenced by the max anisotropy level.
                constexpr uint32 Gfx9PerfMipOffset = 6;

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
                    pSrd->word1.bits.PERF_MIP = (maxAnisoRatio + Gfx9PerfMipOffset);
                }

                constexpr uint32 Gfx9NumAnisoThresholdValues = 8;

                if (pInfo->flags.useAnisoThreshold == 1)
                {
                    // ANISO_THRESHOLD is a 3 bit number representing adjustments of 0/8 through 7/8
                    // so we quantize and clamp anisoThreshold into that range here.
                    pSrd->word0.bits.ANISO_THRESHOLD = Util::Clamp(static_cast<uint32>(
                        static_cast<float>(Gfx9NumAnisoThresholdValues) * pInfo->anisoThreshold),
                        0U, Gfx9NumAnisoThresholdValues - 1U);
                }
                else
                {
                    //  The code below does the following calculation.
                    //  if maxAnisotropy < 4   ANISO_THRESHOLD = 0 (0.0 adjust)
                    //  if maxAnisotropy < 16  ANISO_THRESHOLD = 1 (0.125 adjust)
                    //  if maxAnisotropy == 16 ANISO_THRESHOLD = 2 (0.25 adjust)
                    constexpr uint32 Gfx9AnisoRatioShift = 1;
                    pSrd->word0.bits.ANISO_THRESHOLD = (settings.samplerAnisoThreshold == 0)
                                                        ? (maxAnisoRatio >> Gfx9AnisoRatioShift)
                                                        : settings.samplerAnisoThreshold;
                }

                pSrd->word0.bits.ANISO_BIAS = (settings.samplerAnisoBias == 0) ? maxAnisoRatio :
                                                                                 settings.samplerAnisoBias;
                pSrd->word2.bits.LOD_BIAS_SEC = settings.samplerSecAnisoBias;
            }

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
            //       register which controls the address of the palette is a config register.
            //
            //
            //       In the event that this setting (disableBorderColorPaletteBinds) should be set to TRUE, we need to
            //       make sure that any samplers created do not reference a border color palette and instead
            //       just select transparent black.
            if (settings.disableBorderColorPaletteBinds)
            {
                pSrd->word3.bits.BORDER_COLOR_TYPE = SQ_TEX_BORDER_COLOR_TRANS_BLACK;
                pSrd->word3.bits.BORDER_COLOR_PTR  = 0;
            }

            // This is an enhancement for anisotropic texture filtering, which should be disabled if we need to match
            // image quality between ASICs in an MGPU configuration.
            pSrd->word0.bits.COMPAT_MODE = (pInfo->flags.mgpuIqMatch == 0);

            // This allows the sampler to override anisotropic filtering when the resource view contains a single
            // mipmap level.
            pSrd->word2.bits.ANISO_OVERRIDE = !pInfo->flags.disableSingleMipAnisoOverride;
        }

        memcpy(pSrdOutput, &tempSamplerSrds[0], (currentSrdIdx * sizeof(SamplerSrd)));
    }
}

// =====================================================================================================================
void Device::SetSrdBorderColorPtr(
    sq_img_samp_t*  pSrd,
    uint32          borderColorPtr
    ) const
{
    const Pal::Device&  device = *Parent();

    if (IsGfx10(device))
    {
        pSrd->gfx10Core.border_color_ptr = borderColorPtr;
    }
#if PAL_BUILD_GFX11
    else
    {
        pSrd->gfx104Plus.border_color_ptr = borderColorPtr;
    }
#endif
}

// =====================================================================================================================
// Gfx10 specific function for creating sampler SRDs. Installed in the function pointer table of the parent device
// during initialization.
void PAL_STDCALL Device::Gfx10CreateSamplerSrds(
    const IDevice*     pDevice,
    uint32             count,
    const SamplerInfo* pSamplerInfo,
    void*              pOut)
{
    PAL_ASSERT((pDevice != nullptr) && (pOut != nullptr) && (pSamplerInfo != nullptr) && (count > 0));
    const Pal::Device*     pPalDevice = static_cast<const Pal::Device*>(pDevice);
    const Device*          pGfxDevice = static_cast<const Device*>(pPalDevice->GetGfxDevice());
    const Gfx9PalSettings& settings   = GetGfx9Settings(*pPalDevice);
    constexpr uint32       SamplerSrdSize = sizeof(SamplerSrd);

    constexpr uint32 NumTemporarySamplerSrds                  = 32;
    SamplerSrd       tempSamplerSrds[NumTemporarySamplerSrds] = {};
    uint32           srdsBuilt                                = 0;

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
            auto*              pSrd  = &tempSamplerSrds[currentSrdIdx].gfx10;

            const SQ_TEX_ANISO_RATIO maxAnisoRatio = GetAnisoRatio(*pInfo);

            pSrd->clamp_x            = GetAddressClamp(pInfo->addressU);
            pSrd->clamp_y            = GetAddressClamp(pInfo->addressV);
            pSrd->clamp_z            = GetAddressClamp(pInfo->addressW);
            pSrd->max_aniso_ratio    = maxAnisoRatio;
            pSrd->depth_compare_func = static_cast<uint32>(pInfo->compareFunc);
            pSrd->force_unnormalized = pInfo->flags.unnormalizedCoords;
            pSrd->trunc_coord        = pInfo->flags.truncateCoords;
            pSrd->disable_cube_wrap  = (pInfo->flags.seamlessCubeMapFiltering == 1) ? 0 : 1;

            constexpr uint32 Gfx10SamplerLodMinMaxIntBits  = 4;
            constexpr uint32 Gfx10SamplerLodMinMaxFracBits = 8;
            pSrd->min_lod = Math::FloatToUFixed(pInfo->minLod,
                                                Gfx10SamplerLodMinMaxIntBits,
                                                Gfx10SamplerLodMinMaxFracBits);
            pSrd->max_lod = Math::FloatToUFixed(pInfo->maxLod,
                                                Gfx10SamplerLodMinMaxIntBits,
                                                Gfx10SamplerLodMinMaxFracBits);

            constexpr uint32 Gfx10SamplerLodBiasIntBits  = 6;
            constexpr uint32 Gfx10SamplerLodBiasFracBits = 8;

            // Setup XY and Mip filters.  Encoding of the API enumerations is:  xxyyzzww, where:
            //     ww : mag filter bits
            //     zz : min filter bits
            //     yy : z filter bits
            //     xx : mip filter bits
            pSrd->xy_mag_filter = static_cast<uint32>(pInfo->filter.magnification);
            pSrd->xy_min_filter = static_cast<uint32>(pInfo->filter.minification);
            pSrd->z_filter      = static_cast<uint32>(pInfo->filter.zFilter);
            pSrd->mip_filter    = static_cast<uint32>(pInfo->filter.mipFilter);
            pSrd->lod_bias      = Math::FloatToSFixed(pInfo->mipLodBias,
                                                      Gfx10SamplerLodBiasIntBits,
                                                      Gfx10SamplerLodBiasFracBits);

            {
                pSrd->gfx10CorePlus.blend_prt      = pInfo->flags.prtBlendZeroMode;
                pSrd->gfx10Core.mip_point_preclamp = 0;
            }

            // Ensure useAnisoThreshold is only set when preciseAniso is disabled
            PAL_ASSERT((pInfo->flags.preciseAniso == 0) ||
                        ((pInfo->flags.preciseAniso == 1) && (pInfo->flags.useAnisoThreshold == 0)));

            if (pInfo->flags.preciseAniso == 0)
            {
                // Setup filtering optimization levels: these will be modulated by the global filter
                // optimization aggressiveness, which is controlled by the "TFQ" public setting.
                // NOTE: Aggressiveness of optimizations is influenced by the max anisotropy level.
                constexpr uint32 Gfx10PerfMipOffset = 6;

                if (settings.samplerPerfMip)
                {
                    pSrd->perf_mip = settings.samplerPerfMip;
                }
                else if (pInfo->perfMip)
                {
                    pSrd->perf_mip = pInfo->perfMip;
                }
                else
                {
                    pSrd->perf_mip = (maxAnisoRatio + Gfx10PerfMipOffset);
                }

                constexpr uint32 Gfx10NumAnisoThresholdValues = 8;

                if (pInfo->flags.useAnisoThreshold == 1)
                {
                    // ANISO_THRESHOLD is a 3 bit number representing adjustments of 0/8 through 7/8
                    // so we quantize and clamp anisoThreshold into that range here.
                    pSrd->aniso_threshold = Util::Clamp(static_cast<uint32>(
                        static_cast<float>(Gfx10NumAnisoThresholdValues) * pInfo->anisoThreshold),
                        0U, Gfx10NumAnisoThresholdValues - 1U);
                }
                else
                {
                    //  The code below does the following calculation.
                    //  if maxAnisotropy < 4   ANISO_THRESHOLD = 0 (0.0 adjust)
                    //  if maxAnisotropy < 16  ANISO_THRESHOLD = 1 (0.125 adjust)
                    //  if maxAnisotropy == 16 ANISO_THRESHOLD = 2 (0.25 adjust)
                    constexpr uint32 Gfx10AnisoRatioShift = 1;
                    pSrd->aniso_threshold = (settings.samplerAnisoThreshold == 0)
                                             ? (maxAnisoRatio >> Gfx10AnisoRatioShift)
                                             : settings.samplerAnisoThreshold;
                }

                pSrd->aniso_bias   = (settings.samplerAnisoBias == 0) ? maxAnisoRatio : settings.samplerAnisoBias;
                pSrd->lod_bias_sec = settings.samplerSecAnisoBias;
            }

            {
                constexpr SQ_IMG_FILTER_TYPE  HwFilterMode[]=
                {
                    SQ_IMG_FILTER_MODE_BLEND, // TexFilterMode::Blend
                    SQ_IMG_FILTER_MODE_MIN,   // TexFilterMode::Min
                    SQ_IMG_FILTER_MODE_MAX,   // TexFilterMode::Max
                };

                PAL_ASSERT(static_cast<uint32>(pInfo->filterMode) < (Util::ArrayLen(HwFilterMode)));
                pSrd->gfx10CorePlus.filter_mode = HwFilterMode[static_cast<uint32>(pInfo->filterMode)];
            }

            // The BORDER_COLOR_PTR field is only used by the HW for the SQ_TEX_BORDER_COLOR_REGISTER case
            pGfxDevice->SetSrdBorderColorPtr(pSrd, 0);

            // And setup the HW-supported border colors appropriately
            switch (pInfo->borderColorType)
            {
            case BorderColorType::White:
                pSrd->border_color_type = SQ_TEX_BORDER_COLOR_OPAQUE_WHITE;
                break;
            case BorderColorType::TransparentBlack:
                pSrd->border_color_type = SQ_TEX_BORDER_COLOR_TRANS_BLACK;
                break;
            case BorderColorType::OpaqueBlack:
                pSrd->border_color_type = SQ_TEX_BORDER_COLOR_OPAQUE_BLACK;
                break;
            case BorderColorType::PaletteIndex:
                pSrd->border_color_type     = SQ_TEX_BORDER_COLOR_REGISTER;
                pGfxDevice->SetSrdBorderColorPtr(pSrd, pInfo->borderColorPaletteIndex);
                break;
            default:
                PAL_ALERT_ALWAYS();
                break;
            }

            // NOTE: The hardware fundamentally does not support multiple border color palettes for compute as the
            //       register which controls the address of the palette is a config register.
            //
            //
            //       In the event that this setting (disableBorderColorPaletteBinds) should be set to TRUE, we need to
            //       make sure that any samplers created do not reference a border color palette and instead
            //       just select transparent black.
            if (settings.disableBorderColorPaletteBinds)
            {
                pSrd->border_color_type     = SQ_TEX_BORDER_COLOR_TRANS_BLACK;
                pGfxDevice->SetSrdBorderColorPtr(pSrd, 0);
            }

            // This allows the sampler to override anisotropic filtering when the resource view contains a single
            // mipmap level.
            pSrd->aniso_override = (pInfo->flags.disableSingleMipAnisoOverride == 0);

            if (pInfo->flags.forResidencyMap)
            {
                // The u/v slope / offset fields are in the same location as the border_color_ptr field
                // used by PaletteIndex.  Verify that both residencymap and palette-index are not set.
                PAL_ASSERT(pInfo->borderColorType != BorderColorType::PaletteIndex);

                sq_img_samp_linked_resource_res_map_t*  pLinkedRsrcSrd =
                        reinterpret_cast<sq_img_samp_linked_resource_res_map_t*>(pSrd);

                //  if (T#.linked_resource != 0)
                //      11:9 - v_offset(w_offset for 3D texture) value selector
                //       8:6 - v_slope(w_slope for 3D texture) value selector
                //       5:3 - u_offset value selector
                //       2:0 - u_slope value selector
                //
                // Offset values as specified by the client start at 1 / (1 << 0) = 1.  However,
                // HW considers a programmed value of zero to represent an offset of 1/4th.  Bias
                // the supplied value here.
                constexpr uint32  LowValidOffset = 2; // Log2(4);

                const uint32  biasedOffsetX  = pInfo->uvOffset.x - LowValidOffset;
                const uint32  biasedOffsetY  = pInfo->uvOffset.y - LowValidOffset;

                if (IsGfx103(*pPalDevice))
                {
                    pLinkedRsrcSrd->gfx103.linked_resource_slopes = (((pInfo->uvSlope.x  & 0x7) << 0) |
                                                                     ((biasedOffsetX     & 0x7) << 3) |
                                                                     ((pInfo->uvSlope.y  & 0x7) << 6) |
                                                                     ((biasedOffsetY     & 0x7) << 9));

                    // Verify that the "linked_resource_slopes" lines up with the "border_color_ptr" field.
                    PAL_ASSERT(pSrd->gfx10Core.border_color_ptr == pLinkedRsrcSrd->gfx103.linked_resource_slopes);

                }
#if PAL_BUILD_GFX11
                else
                {
                    pLinkedRsrcSrd->gfx11.linked_resource_slopes = (((pInfo->uvSlope.x  & 0x7) << 0) |
                                                                    ((biasedOffsetX     & 0x7) << 3) |
                                                                    ((pInfo->uvSlope.y  & 0x7) << 6) |
                                                                    ((biasedOffsetY     & 0x7) << 9));
                }
#endif
            }
        } // end loop through temp SRDs

        memcpy(pSrdOutput, &tempSamplerSrds[0], (currentSrdIdx * sizeof(SamplerSrd)));
    } // end loop through SRDs
}

static_assert(static_cast<uint32>(Pal::BoxSortHeuristic::ClosestFirst) == 0,
    "HW value is not identical to Pal::BoxSortHeuristic::ClosestFirst enum value.");
static_assert(static_cast<uint32>(Pal::BoxSortHeuristic::LargestFirst) == 1,
    "HW value is not identical to Pal::BoxSortHeuristic::LargestFirst enum value.");
static_assert(static_cast<uint32>(Pal::BoxSortHeuristic::ClosestMidPoint) == 2,
    "HW value is not identical to Pal::BoxSortHeuristic::ClosestMidPoint enum value.");

// =====================================================================================================================
// Gfx9+ specific function for creating ray trace SRDs. Installed in the function pointer table of the parent device
// during initialization.
void PAL_STDCALL Device::CreateBvhSrds(
    const IDevice*  pDevice,
    uint32          count,
    const BvhInfo*  pBvhInfo,
    void*           pOut)
{
    PAL_ASSERT((pDevice != nullptr) && (pOut != nullptr) && (pBvhInfo != nullptr) && (count > 0));

    const Pal::Device*      pPalDevice = static_cast<const Pal::Device*>(pDevice);
    const Device*           pGfxDevice = static_cast<const Device*>(pPalDevice->GetGfxDevice());
    const GpuChipProperties chipProperties = pPalDevice->ChipProperties();

    // If this trips, then this hardware doesn't support ray-trace.  Why are we being called?
    PAL_ASSERT(chipProperties.srdSizes.bvh != 0);

    for (uint32 idx = 0; idx < count; idx++)
    {
        sq_bvh_rsrc_t     bvhSrd  = {}; // create the SRD in local memory to avoid potential thrashing of GPU memory
        const auto&       bvhInfo = pBvhInfo[idx];
        const GpuMemory*  pMemory = static_cast<const GpuMemory*>(bvhInfo.pMemory);

        // Ok, there are two modes of operation here:
        //    1) raw VA.  The node_address is a tagged VA pointer, instead of a relative offset.  However, the
        //                HW still needs a BVH T# to tell it to run in raw VA mode and to configure the
        //                watertightness, box sorting, and cache behavior.
        //    2) BVH addressing:
        if (bvhInfo.flags.useZeroOffset == 0)
        {
            PAL_ASSERT(pMemory != nullptr);
            const auto& memDesc = pMemory->Desc();

            const gpusize  gpuVirtAddr = memDesc.gpuVirtAddr + bvhInfo.offset;

            // Make sure the supplied memory pointer is aligned.
            PAL_ASSERT((gpuVirtAddr & 0xFF) == 0);

            bvhSrd.base_address = gpuVirtAddr >> 8;
        }
        else
        {
            // Node_pointer comes from the VGPRs when the instruction is issued (vgpr_a[0] for image_bvh*,
            // vgpr_a[0:1] for image_bvh64*)
            bvhSrd.base_address = 0;
        }

        // Setup common srd fields here
        bvhSrd.size = bvhInfo.numNodes - 1;

        //    Number of ULPs to be added during ray-box test, encoded as unsigned integer

        // HW only has eight bits available for this field
        PAL_ASSERT((bvhInfo.boxGrowValue & ~0xFF) == 0);

        bvhSrd.box_grow_value = bvhInfo.boxGrowValue;

        if (pPalDevice->MemoryProperties().flags.supportsMall != 0)
        {
            bvhSrd.gfx103PlusExclusive.llc_noalloc = CalcLlcNoalloc(bvhInfo.flags.bypassMallRead,
                                                                    bvhInfo.flags.bypassMallWrite);
        }

        //    0: Return data for triangle tests are
        //    { 0: t_num, 1 : t_denom, 2 : triangle_id, 3 : hit_status}
        //    1: Return data for triangle tests are
        //    { 0: t_num, 1 : t_denom, 2 : I_num, 3 : J_num }
        // This should only be set if HW supports the ray intersection mode that returns triangle barycentrics.
        PAL_ASSERT((chipProperties.gfx9.supportIntersectRayBarycentrics == 1) ||
                   (bvhInfo.flags.returnBarycentrics == 0));

        bvhSrd.triangle_return_mode = bvhInfo.flags.returnBarycentrics;

        bvhSrd.box_sort_en = (bvhInfo.boxSortHeuristic == BoxSortHeuristic::Disabled) ? false : true;

        //    MSB must be set-- 0x8
        bvhSrd.type         = 0x8;

#if PAL_BUILD_GFX11
        if (chipProperties.gfx9.rayTracingIp >= RayTracingIpLevel::RtIp2_0)
        {
            bvhSrd.rtIp2Plus.pointer_flags = bvhInfo.flags.pointerFlags;
        }

        // bvhSrd.rtIp2Plus.box_sorting_heuristic to specify which heuristic should be utilized
        if ((chipProperties.gfx9.rayTracingIp >= RayTracingIpLevel::RtIp2_0) &&
            (bvhInfo.boxSortHeuristic != BoxSortHeuristic::Disabled))
        {
            bvhSrd.rtIp2Plus.box_sorting_heuristic = static_cast<uint32>(bvhInfo.boxSortHeuristic);
        }
#endif

        memcpy(VoidPtrInc(pOut, idx * sizeof(sq_bvh_rsrc_t)),
               &bvhSrd,
               sizeof(bvhSrd));
    }
}

// =====================================================================================================================
// Determines the GFXIP level of a GPU supported by the GFX9 hardware layer. The return value will be GfxIpLevel::None
// if the GPU is unsupported by this HWL.
// PAL relies on a specific set of functionality from the CP microcode, so the GPU is only supported if the microcode
// version is new enough (this varies by hardware family).
GfxIpLevel DetermineIpLevel(
    uint32 familyId, // Hardware Family ID.
    uint32 eRevId,   // Software Revision ID.
    uint32 microcodeVersion)
{
    GfxIpLevel level = GfxIpLevel::None;

    switch (familyId)
    {
    // GFX 9 Discrete GPU's (Arctic Islands):
    case FAMILY_AI:
    case FAMILY_RV:
        level = GfxIpLevel::GfxIp9;
        break;

    // GFX10 GPU's (Navi family)
    case FAMILY_NV:
        if (AMDGPU_IS_NAVI10(familyId, eRevId)
            || AMDGPU_IS_NAVI12(familyId, eRevId)
            || AMDGPU_IS_NAVI14(familyId, eRevId)
            )
        {
            level = GfxIpLevel::GfxIp10_1;
        }
        else if (AMDGPU_IS_NAVI21(familyId, eRevId)
                 )
        {
            level = GfxIpLevel::GfxIp10_3;
        }
        else if (AMDGPU_IS_NAVI22(familyId, eRevId))
        {
            level = GfxIpLevel::GfxIp10_3;
        }
        else if (AMDGPU_IS_NAVI23(familyId, eRevId))
        {
            level = GfxIpLevel::GfxIp10_3;
        }
        else if (AMDGPU_IS_NAVI24(familyId, eRevId))
        {
            level = GfxIpLevel::GfxIp10_3;
        }
        else
        {
            PAL_NOT_IMPLEMENTED_MSG("NV_FAMILY Revision %d unsupported", eRevId);
        }
        break;

    case FAMILY_RMB:
        if (AMDGPU_IS_REMBRANDT(familyId, eRevId))
        {
            level = GfxIpLevel::GfxIp10_3;
        }
        else
        {
            PAL_NOT_IMPLEMENTED_MSG("RMB_FAMILY Revision %d unsupported", eRevId);
        }
        break;

#if PAL_BUILD_NAVI3X
    case FAMILY_NV3:
#if PAL_BUILD_NAVI31
        if (AMDGPU_IS_NAVI31(familyId, eRevId))
        {
            level = GfxIpLevel::GfxIp11_0;
        }
        else
#endif
        {
            PAL_NOT_IMPLEMENTED_MSG("FAMILY_NV3 Revision %d unsupported", eRevId);
        }
        break;
#endif

    case FAMILY_RPL:
        if (AMDGPU_IS_RAPHAEL(familyId, eRevId))
        {
            level = GfxIpLevel::GfxIp10_3;
        }
        else
        {
            PAL_NOT_IMPLEMENTED_MSG("RPL_FAMILY Revision %d unsupported", eRevId);
        }
        break;
    case FAMILY_MDN:
        if (AMDGPU_IS_MENDOCINO(familyId, eRevId))
        {
            level = GfxIpLevel::GfxIp10_3;
        }
        else
        {
            PAL_NOT_IMPLEMENTED_MSG("MDN_FAMILY Revision %d unsupported", eRevId);
        }
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return level;
}

// =====================================================================================================================
// Gets the static format support info table for GFXIP 9 hardware.
const MergedFormatPropertiesTable* GetFormatPropertiesTable(
    GfxIpLevel                 gfxIpLevel,
    const PalPlatformSettings& settings)
{
    const MergedFormatPropertiesTable* pTable = nullptr;

    switch (gfxIpLevel)
    {
    case GfxIpLevel::GfxIp9:
        pTable = &Gfx9MergedFormatPropertiesTable;
        break;
    case GfxIpLevel::GfxIp10_1:
        pTable = &Gfx10MergedFormatPropertiesTable;
        break;
    case GfxIpLevel::GfxIp10_3:
        pTable = &Gfx10_3MergedFormatPropertiesTable;
        break;
#if PAL_BUILD_GFX11
    case GfxIpLevel::GfxIp11_0:
        pTable = &Gfx11MergedFormatPropertiesTable;
        break;
#endif

    default:
        // What is this?
        PAL_ASSERT_ALWAYS();
    }

    return pTable;
}

// =====================================================================================================================
// Initializes the GPU chip properties for a Device object, specifically for the GFX9 hardware layer. Returns an error
// if an unsupported chip revision is detected.
void InitializeGpuChipProperties(
    const Platform*    pPlatform,
    uint32             cpUcodeVersion,
    GpuChipProperties* pInfo)
{
    pInfo->imageProperties.flags.u32All = 0;

    // All GFXIP9 hardware has the same max image dimensions.
    pInfo->imageProperties.maxImageDimension.width  = MaxImageWidth;
    pInfo->imageProperties.maxImageDimension.height = MaxImageHeight;
    pInfo->imageProperties.maxImageDimension.depth  = MaxImageDepth;

    pInfo->imageProperties.flags.supportsAqbsStereoMode = 1;

    // GFX9 core ASICs support all MSAA modes (up to S16F8)
    pInfo->imageProperties.msaaSupport      = MsaaAll;
    pInfo->imageProperties.maxMsaaFragments = 8;

    pInfo->imageProperties.tilingSupported[static_cast<uint32>(ImageTiling::Linear)]           = true;
    pInfo->imageProperties.tilingSupported[static_cast<uint32>(ImageTiling::Optimal)]          = true;
    if (ASICREV_IS_VEGA12_P(pInfo->eRevId))
    {
        pInfo->imageProperties.tilingSupported[static_cast<uint32>(ImageTiling::Standard64Kb)] = false;
    }
    else
    {
        pInfo->imageProperties.tilingSupported[static_cast<uint32>(ImageTiling::Standard64Kb)] = true;
    }

    pInfo->gfx9.numScPerSe     = 1;
    pInfo->gfx9.numPackerPerSc = 2; // unless overridden below

    // TODO:  Should find a way to get this info from the ADAPTERINFOEX structure.  Steal these values from
    //        the GFX6 implementation for the time being.
    pInfo->gfx9.numSimdPerCu = 4;

    // All Gfx9+ GPUs have 16 SQC barrier resources per CU.  One barrier is allocated to every compute threadgroup
    // which has >1 wavefront per group.
    pInfo->gfx9.numSqcBarriersPerCu = 16;

    // The maximum amount of LDS space that can be shared by a group of threads (wave/ threadgroup) in bytes.
    pInfo->gfxip.ldsSizePerThreadGroup = 64_KiB;
    pInfo->gfxip.ldsSizePerCu          = 65536;
    pInfo->gfxip.mallSizeInBytes       = 0_MiB; // overridden if device supports MALL.
    pInfo->gfxip.tccSizeInBytes        = 4_MiB;
    pInfo->gfxip.tcpSizeInBytes        = 16_KiB;
    pInfo->gfxip.maxLateAllocVsLimit   = 64;

    pInfo->gfxip.gl1cSizePerSa         = 0_KiB;  // overwritten if device includes GL1 cache.
    pInfo->gfxip.instCacheSizePerCu    = 32_KiB; // INST_CACHE_BANK_SIZE_KBYTES * GC__NUM_BANK or INST_NUM_BANKS
    pInfo->gfxip.scalarCacheSizePerCu  = 16_KiB; // DATA_CACHE_BANK_SIZE_KBYTES * GC__NUM_BANK or DATA_NUM_BANKS

    // The LDS_SIZE registers are programmed in granularity of 128DW, but on 10.3+ HW, allocating 128DW
    // actually allocates 256 DW.  i.e., HW allocates in units of 256.  So let the client know what the
    // actual allocating size is.
    pInfo->gfxip.ldsGranularity        = (IsGfx103Plus(pInfo->gfxLevel) ? 256 : 128) * sizeof(uint32);

    pInfo->gfxip.supportGl2Uncached      = 1;
    pInfo->gfxip.gl2UncachedCpuCoherency = (CoherCpu | CoherShader | CoherIndirectArgs | CoherIndexData |
                                            CoherQueueAtomic | CoherTimestamp | CoherCeLoad | CoherCeDump |
                                            CoherStreamOut | CoherMemory | CoherCp | CoherSampleRate);

    pInfo->gfxip.supportCaptureReplay    = 1;

    pInfo->gfxip.maxUserDataEntries      = MaxUserDataEntries;
    pInfo->gfxip.supportsHwVs            = 1;

    // Max supported by HW is 2^32-1 for all counters.  However limit Y and Z to keep total threads < 2^64 to avoid
    // potentially overflowing 64 bit counters in HW
    pInfo->gfxip.maxComputeThreadGroupCountX = UINT32_MAX;
    pInfo->gfxip.maxComputeThreadGroupCountY = UINT16_MAX;
    pInfo->gfxip.maxComputeThreadGroupCountZ = UINT16_MAX;

#if PAL_BUILD_GFX11
    if (IsGfx11(pInfo->gfxLevel))
    {
        // NGG is required on GFX11, so the max verts/prim per subgroup is 256.
        pInfo->gfxip.maxGsOutputVert            = 256;
        pInfo->gfxip.maxGsTotalOutputComponents = 1024;
    }
    else
#endif
    {
        pInfo->gfxip.maxGsOutputVert            = 1023; // power of two minus one
        pInfo->gfxip.maxGsTotalOutputComponents = 4095; // power of two minus one
    }

    if (IsGfx10Plus(pInfo->gfxLevel))
    {
        pInfo->gfxip.maxGsInvocations = 126;
    }
    else
    {
        pInfo->gfxip.maxGsInvocations = 127;
    }

    if (IsGfx103Plus(pInfo->gfxLevel))
    {
        pInfo->imageProperties.prtFeatures = Gfx102PlusPrtFeatures;
        pInfo->imageProperties.prtTileSize = PrtTileSize;
    }
    else
    {
        pInfo->imageProperties.prtFeatures = Gfx9PrtFeatures;
        pInfo->imageProperties.prtTileSize = PrtTileSize;
    }

    if (IsGfx103Plus(pInfo->gfxLevel))
    {
        // On GFX10, VRS tiles are stored in hTile memory which always represents an 8x8 block
        pInfo->imageProperties.vrsTileSize.width  = 8;
        pInfo->imageProperties.vrsTileSize.height = 8;

        pInfo->gfxip.supportsVrs = 1;

        pInfo->gfx9.gfx10.supportedVrsRates = ((1 << static_cast<uint32>(VrsShadingRate::_16xSsaa)) |
                                               (1 << static_cast<uint32>(VrsShadingRate::_8xSsaa))  |
                                               (1 << static_cast<uint32>(VrsShadingRate::_4xSsaa))  |
                                               (1 << static_cast<uint32>(VrsShadingRate::_2xSsaa))  |
                                               (1 << static_cast<uint32>(VrsShadingRate::_1x1))     |
                                               (1 << static_cast<uint32>(VrsShadingRate::_1x2))     |
                                               (1 << static_cast<uint32>(VrsShadingRate::_2x1))     |
                                               (1 << static_cast<uint32>(VrsShadingRate::_2x2)));

#if PAL_BUILD_GFX11
        if (IsGfx11(pInfo->gfxLevel))
        {
            // On Gfx11, Dynamiclaunch feature should be enabled by default.
            pInfo->gfxip.dynamicLaunchDescSize = sizeof(DynamicCsLaunchDescLayout);
        }
        else
#endif
        if (cpUcodeVersion >= Gfx103UcodeVersionLoadShRegIndexIndirectAddr)
        {
            pInfo->gfxip.dynamicLaunchDescSize = sizeof(DynamicCsLaunchDescLayout);
        }
    }

    if ((IsGfx103(pInfo->gfxLevel) && (cpUcodeVersion >= Gfx103UcodeVersionLoadShRegIndexIndirectAddr))
#if PAL_BUILD_GFX11
        || IsGfx11(pInfo->gfxLevel)
#endif
       )
    {
        // PAL implements almost all of its HSA ABI support in a generic gfx9-10.3 way but we require LOAD_SH_REG_INDEX
        // packet support on compute queues. That was only implemented on gfx10.3+ for dynamic launch support.
        // If CP ever expands support to earlier HW we can expand HSA support too.

        pInfo->gfxip.supportHsaAbi = 1;
    }

    // When per-channel min/max filter operations are supported, make it clear that single channel always are as well.
    pInfo->gfx9.supportSingleChannelMinMaxFilter = 1;

    pInfo->gfx9.supports2BitSignedValues           = 1;
    pInfo->gfx9.supportConservativeRasterization   = 1;
    pInfo->gfx9.supportPrtBlendZeroMode            = 1;
    pInfo->gfx9.supportPrimitiveOrderedPs          = 1;
    pInfo->gfx9.supportImplicitPrimitiveShader     = 1;
    pInfo->gfx9.supportFp16Fetch                   = 1;
    pInfo->gfx9.support16BitInstructions           = 1;
    pInfo->gfx9.support64BitInstructions           = 1;
    pInfo->gfx9.supportBorderColorSwizzle          = 1;
    pInfo->gfx9.supportDoubleRate16BitInstructions = 1;
    pInfo->gfxip.supportFloat64BufferAtomicMinMax  = 1;
    pInfo->gfxip.supportFloat64SharedAtomicMinMax  = 1;

    {
        pInfo->gfx9.supportImageViewMinLod         = 1;
    }

    // Support PrimitiveTopology::TwoDRectList for GfxIp9 and onwards.
    pInfo->gfx9.support2DRectList                  = 1;

    // All gfx9+ hardware can support subgroup/device clocks
    pInfo->gfx9.supportShaderSubgroupClock = 1;
    pInfo->gfx9.supportShaderDeviceClock   = 1;

    if (IsGfx10(pInfo->gfxLevel))
    {
        pInfo->gfx9.supportAddrOffsetDumpAndSetShPkt = 1;
        pInfo->gfx9.supportAddrOffsetSetSh256Pkt     = (cpUcodeVersion >= Gfx10UcodeVersionSetShRegOffset256B);
        pInfo->gfx9.supportPostDepthCoverage         = 1;
        pInfo->gfx9.supportTextureGatherBiasLod      = 1;
        pInfo->gfxip.supportFloat32BufferAtomics     = 1;
        pInfo->gfxip.supportFloat32ImageAtomics      = 1;
        pInfo->gfxip.supportFloat32ImageAtomicMinMax = 1;
        pInfo->gfxip.supportFloat32BufferAtomicAdd   = 0;
        pInfo->gfxip.supportFloat32ImageAtomicAdd    = 0;
        pInfo->gfx9.supportFloat64Atomics            = 1;

        pInfo->gfx9.numShaderArrays         = 2;
        pInfo->gfx9.numSimdPerCu            = Gfx10NumSimdPerCu;
        pInfo->gfx9.numWavesPerSimd         = IsGfx103Plus(pInfo->gfxLevel) ? 16 : 20;
        pInfo->gfx9.nativeWavefrontSize     = 32;
        pInfo->gfx9.minWavefrontSize        = 32;
        pInfo->gfx9.maxWavefrontSize        = 64;
        pInfo->gfx9.numShaderVisibleSgprs   = MaxSgprsAvailable;
        pInfo->gfx9.numPhysicalSgprs        = pInfo->gfx9.numWavesPerSimd * Gfx10NumSgprsPerWave;
        pInfo->gfx9.sgprAllocGranularity    = Gfx10NumSgprsPerWave;
        pInfo->gfx9.minSgprAlloc            = pInfo->gfx9.sgprAllocGranularity;
        pInfo->gfx9.numPhysicalVgprs        = 1024;
        pInfo->gfx9.vgprAllocGranularity    = IsGfx103Plus(pInfo->gfxLevel) ? 16 : 8;
        pInfo->gfx9.minVgprAlloc            = pInfo->gfx9.vgprAllocGranularity;
        pInfo->gfxip.shaderPrefetchBytes    = 3 * ShaderICacheLineSize;
        pInfo->gfxip.gl1cSizePerSa          = 128_KiB;

    }
    else if (pInfo->gfxLevel == GfxIpLevel::GfxIp9)
    {
        pInfo->gfx9.supportAddrOffsetDumpAndSetShPkt  = (cpUcodeVersion >= UcodeVersionWithDumpOffsetSupport);
        pInfo->gfx9.supportAddrOffsetSetSh256Pkt      = (cpUcodeVersion >= Gfx9UcodeVersionSetShRegOffset256B);
        pInfo->gfx9.supportTextureGatherBiasLod       = 1;
        pInfo->gfxip.supportFloat32BufferAtomics      = 1;
        pInfo->gfxip.supportFloat32ImageAtomics       = 1;
        pInfo->gfxip.supportFloat32BufferAtomicAdd    = 0;
        pInfo->gfxip.supportFloat32ImageAtomicAdd     = 0;
        pInfo->gfxip.supportFloat32ImageAtomicMinMax  = 0;
        pInfo->gfxip.supportFloat64BufferAtomicMinMax = 0;
        pInfo->gfxip.supportFloat64SharedAtomicMinMax = 0;
        pInfo->gfx9.supportFloat64Atomics             = 1;

        pInfo->gfx9.numShaderArrays         = 1;
        pInfo->gfx9.numSimdPerCu            = Gfx9NumSimdPerCu;
        pInfo->gfx9.numWavesPerSimd         = Gfx9NumWavesPerSimd;
        pInfo->gfx9.nativeWavefrontSize     = 64;
        pInfo->gfx9.minWavefrontSize        = 64;
        pInfo->gfx9.maxWavefrontSize        = 64;
        pInfo->gfx9.numShaderVisibleSgprs   = MaxSgprsAvailable;
        pInfo->gfx9.numPhysicalSgprs        = Gfx9PhysicalSgprsPerSimd;
        pInfo->gfx9.sgprAllocGranularity    = 16;
        pInfo->gfx9.minSgprAlloc            = pInfo->gfx9.sgprAllocGranularity;
        pInfo->gfx9.numPhysicalVgprs        = 256;
        pInfo->gfx9.vgprAllocGranularity    = 4;
        pInfo->gfx9.minVgprAlloc            = pInfo->gfx9.vgprAllocGranularity;
        pInfo->gfxip.shaderPrefetchBytes    = 2 * ShaderICacheLineSize;
    }
#if PAL_BUILD_GFX11
    else if (IsGfx11(pInfo->gfxLevel))
    {
        pInfo->gfx9.supportAddrOffsetDumpAndSetShPkt = 1;
        pInfo->gfx9.supportAddrOffsetSetSh256Pkt     = (cpUcodeVersion >= Gfx10UcodeVersionSetShRegOffset256B);
        pInfo->gfx9.supportPostDepthCoverage         = 1;

        //       FP32 image add/min/max atomic operations are removed in Gfx11, though atomic exch op is enabled
        pInfo->gfxip.supportFloat32BufferAtomics      = 1;
        pInfo->gfxip.supportFloat32ImageAtomics       = 1;
        pInfo->gfxip.supportFloat32BufferAtomicAdd    = 1;
        pInfo->gfxip.supportFloat32ImageAtomicAdd     = 0;
        pInfo->gfxip.supportFloat32ImageAtomicMinMax  = 0;

        //       FP64 atomic add/min/max operations are removed from GL2 in Gfx11, though atomic exch op is enabled
        pInfo->gfx9.supportFloat64Atomics             = 1;

        // NAVI3x hw for min/max support has been removed (comparing to NAVI2x)
        pInfo->gfxip.supportFloat64BufferAtomicMinMax = 0;
        pInfo->gfxip.supportFloat64SharedAtomicMinMax = 0;

        pInfo->gfx9.numShaderArrays         = 2;
        pInfo->gfx9.numSimdPerCu            = Gfx10NumSimdPerCu;
        pInfo->gfx9.numWavesPerSimd         = 16;
        pInfo->gfx9.nativeWavefrontSize     = 32;
        pInfo->gfx9.minWavefrontSize        = 32;
        pInfo->gfx9.maxWavefrontSize        = 64;
        pInfo->gfx9.numShaderVisibleSgprs   = MaxSgprsAvailable;
        pInfo->gfx9.numPhysicalSgprs        = pInfo->gfx9.numWavesPerSimd * Gfx10NumSgprsPerWave;
        pInfo->gfx9.sgprAllocGranularity    = Gfx10NumSgprsPerWave;
        pInfo->gfx9.minSgprAlloc            = pInfo->gfx9.sgprAllocGranularity;
#if PAL_BUILD_NAVI31
        if (AMDGPU_IS_NAVI31(pInfo->familyId, pInfo->eRevId))
        {
            // Navi31 supports 1.5x VGPR
            pInfo->gfx9.numPhysicalVgprs     = 1536;
            pInfo->gfx9.vgprAllocGranularity = 24;
        }
        else
#endif
        {
            pInfo->gfx9.numPhysicalVgprs     = 1024;
            pInfo->gfx9.vgprAllocGranularity = 16;
        }
        pInfo->gfx9.minVgprAlloc            = pInfo->gfx9.vgprAllocGranularity;
        pInfo->gfxip.shaderPrefetchBytes    = 3 * ShaderICacheLineSize;
        pInfo->gfxip.supportsSwStrmout      = 1;
        pInfo->gfxip.supportsHwVs           = 0;

        pInfo->gfxip.gl1cSizePerSa          = 256_KiB;

    }
#endif

    pInfo->gfx9.gsVgtTableDepth         = 32;
    pInfo->gfx9.gsPrimBufferDepth       = 1792;
    pInfo->gfx9.doubleOffchipLdsBuffers = 1;

    pInfo->gfxip.vaRangeNumBits   = 48;
    pInfo->gfxip.hardwareContexts = 8;

    // Gfx9 HW supports all tessellation distribution modes.
    pInfo->gfx9.supportPatchTessDistribution     = 1;
    pInfo->gfx9.supportDonutTessDistribution     = 1;
    pInfo->gfx9.supportTrapezoidTessDistribution = 1;

    pInfo->gfx9.supportReleaseAcquireInterface = 1;

    if (IsGfx10Plus(pInfo->gfxLevel))
    {
        pInfo->gfx9.supportSplitReleaseAcquire = 1;

        // Only supported on GFX10+ because GFX9 ignores the BASE_ARRAY SRD field for 3D image views.
        pInfo->gfx9.support3dUavZRange = 1;
    }

    switch (pInfo->familyId)
    {
    // Gfx 9 APU's (Raven):
    case FAMILY_RV:
        pInfo->gpuType  = GpuType::Integrated;
        pInfo->gfx9.numShaderEngines               = 1;
        pInfo->gfx9.maxGsWavesPerVgt               = 16;
        pInfo->gfx9.parameterCacheLines            = 1024;
        pInfo->gfx9.rbPlus                         = 1;
        pInfo->gfx9.numSdpInterfaces               = 2;
        pInfo->gfxip.supportCaptureReplay          = 0;

        if (ASICREV_IS_RAVEN(pInfo->eRevId))
        {
            pInfo->revision                  = AsicRevision::Raven;
            pInfo->gfxStepping               = Abi::GfxIpSteppingRaven;
            pInfo->gfx9.numTccBlocks         = 4;
            pInfo->gfx9.maxNumCuPerSh        = 11;
            pInfo->gfx9.maxNumRbPerSe        = 2;
            pInfo->gfx9.timestampResetOnIdle = 1;
        }
        else if (ASICREV_IS_RAVEN2(pInfo->eRevId))
        {
            pInfo->revision                  = AsicRevision::Raven2;
            pInfo->gfxStepping               = Abi::GfxIpSteppingRaven2;
            pInfo->gfx9.numTccBlocks         = 2;
            pInfo->gfx9.maxNumCuPerSh        = 3;
            pInfo->gfx9.maxNumRbPerSe        = 1;
            pInfo->gfx9.supportSpp           = 1;
            pInfo->gfx9.timestampResetOnIdle = 1;
        }
        else if (ASICREV_IS_RENOIR(pInfo->eRevId))
        {
            pInfo->revision                  = AsicRevision::Renoir;
            pInfo->gfxStepping               = Abi::GfxIpSteppingRenoir;
            pInfo->gfx9.numTccBlocks         = 4;
            pInfo->gfx9.maxNumCuPerSh        = 8;
            pInfo->gfx9.maxNumRbPerSe        = 2;
            pInfo->gfx9.supportSpp           = 1;
            pInfo->gfx9.timestampResetOnIdle = 1;
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }
        break;
    // Gfx 9 Discrete GPU's (Vega):
    case FAMILY_AI:
        pInfo->gpuType = GpuType::Discrete;
        pInfo->gfx9.numShaderEngines               = 4;
        pInfo->gfx9.maxGsWavesPerVgt               = 32;
        pInfo->gfx9.parameterCacheLines            = 2048;

        if (ASICREV_IS_VEGA10_P(pInfo->eRevId))
        {
            pInfo->revision              = AsicRevision::Vega10;
            pInfo->gfxStepping           = Abi::GfxIpSteppingVega10;
            pInfo->gfx9.numTccBlocks     = 16;
            pInfo->gfx9.maxNumCuPerSh    = 16;
            pInfo->gfx9.maxNumRbPerSe    = 4;
            pInfo->gfx9.numSdpInterfaces = 16;
        }
        else if (ASICREV_IS_VEGA12_P(pInfo->eRevId))
        {
            pInfo->revision                  = AsicRevision::Vega12;
            pInfo->gfxStepping               = Abi::GfxIpSteppingVega12;
            pInfo->gfx9.numTccBlocks         = 8;
            pInfo->gfx9.maxNumCuPerSh        = 5;
            pInfo->gfx9.maxNumRbPerSe        = 2;
            pInfo->gfx9.rbPlus               = 1;
            pInfo->gfx9.timestampResetOnIdle = 1;
            pInfo->gfx9.numSdpInterfaces     = 8;
        }
        else if (ASICREV_IS_VEGA20_P(pInfo->eRevId))
        {
            pInfo->revision                  = AsicRevision::Vega20;
            pInfo->gfxStepping               = Abi::GfxIpSteppingVega20;
            pInfo->gfx9.numTccBlocks         = 16;
            pInfo->gfx9.maxNumCuPerSh        = 16;
            pInfo->gfx9.maxNumRbPerSe        = 4;
            pInfo->gfx9.numSdpInterfaces     = 32;
            pInfo->gfx9.eccProtectedGprs     = 1;
            pInfo->gfx9.supportFp16Dot2      = 1;
            pInfo->gfx9.supportInt8Dot       = 1;
            pInfo->gfx9.supportInt4Dot       = 1;
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }
        break;

    case FAMILY_NV:
        pInfo->gfx9.numShaderArrays                = 2;
        pInfo->gfx9.maxGsWavesPerVgt               = 32;
        pInfo->gfx9.parameterCacheLines            = 1024;
        pInfo->gfx9.supportSpp                     = 1;
        pInfo->gfx9.supportMsaaCoverageOut         = 1;

        // GFX10-specific image properties go here
        pInfo->imageProperties.flags.supportsCornerSampling = 1;

        // This is the common gl2 config for most gfx10 ASICs.
        pInfo->gfx9.gfx10.numGl2a = 4;
        pInfo->gfx9.gfx10.numGl2c = 16;

        // Similarly, this the most common WGP config from the same sources.
        pInfo->gfx9.gfx10.numWgpAboveSpi = 3; // GPU__GC__NUM_WGP0_PER_SA
        pInfo->gfx9.gfx10.numWgpBelowSpi = 2; // GPU__GC__NUM_WGP1_PER_SA

        if (AMDGPU_IS_NAVI10(pInfo->familyId, pInfo->eRevId))
        {
            pInfo->gpuType               = GpuType::Discrete;
            pInfo->revision              = AsicRevision::Navi10;
            pInfo->gfxStepping           = Abi::GfxIpSteppingNavi10;
            pInfo->gfx9.numShaderEngines = 2;
            pInfo->gfx9.maxNumCuPerSh    = 10;
            pInfo->gfx9.maxNumRbPerSe    = 8;
            pInfo->gfx9.numSdpInterfaces = 16;
        }
        else if (AMDGPU_IS_NAVI12(pInfo->familyId, pInfo->eRevId))
        {
            pInfo->gpuType               = GpuType::Discrete;
            pInfo->revision              = AsicRevision::Navi12;
            pInfo->gfxStepping           = Abi::GfxIpSteppingNavi12;
            pInfo->gfx9.numShaderEngines = 2;
            pInfo->gfx9.maxNumCuPerSh    = 10;
            pInfo->gfx9.maxNumRbPerSe    = 8;
            pInfo->gfx9.numSdpInterfaces = 16;
            pInfo->gfx9.supportFp16Dot2  = 1;
            pInfo->gfx9.supportInt8Dot   = 1;
            pInfo->gfx9.supportInt4Dot   = 1;
        }
        else if (AMDGPU_IS_NAVI14(pInfo->familyId, pInfo->eRevId))
        {
            pInfo->gpuType                   = GpuType::Discrete;
            {
                pInfo->revision              = AsicRevision::Navi14;
                pInfo->gfxStepping           = Abi::GfxIpSteppingNavi14;
            }

            pInfo->gfx9.numShaderEngines     = 1;
            pInfo->gfx9.maxNumCuPerSh        = 12;
            pInfo->gfx9.maxNumRbPerSe        = 8;
            pInfo->gfx9.numSdpInterfaces     = 8;
            pInfo->gfx9.parameterCacheLines  = 512;
            pInfo->gfx9.supportFp16Dot2      = 1;
            pInfo->gfx9.gfx10.numGl2a        = 2;
            pInfo->gfx9.gfx10.numGl2c        = 8;
            pInfo->gfx9.gfx10.numWgpAboveSpi = 3; // GPU__GC__NUM_WGP0_PER_SA
            pInfo->gfx9.gfx10.numWgpBelowSpi = 3; // GPU__GC__NUM_WGP1_PER_SA
            pInfo->gfx9.supportInt8Dot       = 1;
            pInfo->gfx9.supportInt4Dot       = 1;
        }
        else if (AMDGPU_IS_NAVI21(pInfo->familyId, pInfo->eRevId))
        {
            pInfo->gpuType                       = GpuType::Discrete;
            pInfo->revision                      = AsicRevision::Navi21;
            pInfo->gfxStepping                   = Abi::GfxIpSteppingNavi21;
            pInfo->gfx9.numShaderEngines         = 4;
            pInfo->gfx9.rbPlus                   = 1;
            pInfo->gfx9.numSdpInterfaces         = 16;
            pInfo->gfx9.maxNumCuPerSh            = 10;
            pInfo->gfx9.maxNumRbPerSe            = 4;
            pInfo->gfx9.numPackerPerSc           = 4;
            pInfo->gfx9.supportFp16Dot2          = 1;
            pInfo->gfx9.gfx10.numWgpAboveSpi     = 5; // GPU__GC__NUM_WGP0_PER_SA
            pInfo->gfx9.gfx10.numWgpBelowSpi     = 0; // GPU__GC__NUM_WGP1_PER_SA
            pInfo->gfx9.supportInt8Dot           = 1;
            pInfo->gfx9.supportInt4Dot           = 1;
            pInfo->gfxip.mallSizeInBytes         = 128_MiB;
        }
        else if (AMDGPU_IS_NAVI22(pInfo->familyId, pInfo->eRevId))
        {
            pInfo->gpuType                       = GpuType::Discrete;
            pInfo->revision                      = AsicRevision::Navi22;
            pInfo->gfxStepping                   = Abi::GfxIpSteppingNavi22;
            pInfo->gfx9.numShaderEngines         = 2;
            pInfo->gfx9.rbPlus                   = 1;
            pInfo->gfx9.numSdpInterfaces         = 16;
            pInfo->gfx9.maxNumCuPerSh            = 10;
            pInfo->gfx9.maxNumRbPerSe            = 4;
            pInfo->gfx9.numPackerPerSc           = 4;
            pInfo->gfx9.supportFp16Dot2          = 1;
            pInfo->gfx9.gfx10.numGl2a            = 2;
            pInfo->gfx9.gfx10.numGl2c            = 12;
            pInfo->gfx9.gfx10.numWgpAboveSpi     = 5; // GPU__GC__NUM_WGP0_PER_SA
            pInfo->gfx9.gfx10.numWgpBelowSpi     = 0; // GPU__GC__NUM_WGP1_PER_SA
            pInfo->gfx9.supportInt8Dot           = 1;
            pInfo->gfx9.supportInt4Dot           = 1;
            pInfo->gfxip.mallSizeInBytes         = 96_MiB;
        }
        else if (AMDGPU_IS_NAVI23(pInfo->familyId, pInfo->eRevId))
        {
            pInfo->gpuType                       = GpuType::Discrete;
            pInfo->revision                      = AsicRevision::Navi23;
            pInfo->gfxStepping                   = Abi::GfxIpSteppingNavi23;
            pInfo->gfx9.numShaderEngines         = 2;
            pInfo->gfx9.rbPlus                   = 1;
            pInfo->gfx9.numSdpInterfaces         = 8;
            pInfo->gfx9.maxNumCuPerSh            = 8;
            pInfo->gfx9.maxNumRbPerSe            = 4;
            pInfo->gfx9.numPackerPerSc           = 4;
            pInfo->gfx9.supportFp16Dot2          = 1;
            pInfo->gfx9.gfx10.numGl2a            = 2;
            pInfo->gfx9.gfx10.numGl2c            = 8;
            pInfo->gfx9.gfx10.numWgpAboveSpi     = 4; // GPU__GC__NUM_WGP0_PER_SA
            pInfo->gfx9.gfx10.numWgpBelowSpi     = 0; // GPU__GC__NUM_WGP1_PER_SA
            pInfo->gfx9.supportInt8Dot           = 1;
            pInfo->gfx9.supportInt4Dot           = 1;
            pInfo->gfxip.mallSizeInBytes         = 32_MiB;
        }
        else if (AMDGPU_IS_NAVI24(pInfo->familyId, pInfo->eRevId))
        {
            pInfo->gpuType                       = GpuType::Discrete;
            pInfo->revision                      = AsicRevision::Navi24;
            pInfo->gfxStepping                   = Abi::GfxIpSteppingNavi24;
            pInfo->gfx9.numShaderEngines         = 1;
            pInfo->gfx9.rbPlus                   = 1;
            pInfo->gfx9.numSdpInterfaces         = 8;
            pInfo->gfx9.maxNumCuPerSh            = 8;
            pInfo->gfx9.maxNumRbPerSe            = 2;
            pInfo->gfx9.numPackerPerSc           = 4;
            pInfo->gfx9.supportFp16Dot2          = 1;
            pInfo->gfx9.parameterCacheLines      = 512; //Navi24 differs from the rest of Navi2x family
            pInfo->gfx9.gfx10.numGl2a            = 2;
            pInfo->gfx9.gfx10.numGl2c            = 8;
            pInfo->gfx9.gfx10.numWgpAboveSpi     = 5; // GPU__GC__NUM_WGP0_PER_SA
            pInfo->gfx9.gfx10.numWgpBelowSpi     = 0; // GPU__GC__NUM_WGP1_PER_SA
            pInfo->gfx9.supportInt8Dot           = 1;
            pInfo->gfx9.supportInt4Dot           = 1;
            pInfo->gfxip.mallSizeInBytes         = 16_MiB;
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }

        // The GL2C is the TCC.
        pInfo->gfx9.numTccBlocks = pInfo->gfx9.gfx10.numGl2c;
        break;

    case FAMILY_RMB:
        if (AMDGPU_IS_REMBRANDT(pInfo->familyId, pInfo->eRevId))
        {
            pInfo->gpuType                             = GpuType::Integrated;
            pInfo->revision                            = AsicRevision::Rembrandt;
            pInfo->gfxStepping                         = Abi::GfxIpSteppingRembrandt;
            pInfo->gfx9.numShaderEngines               = 1; // GC__NUM_SE
            pInfo->gfx9.rbPlus                         = 1; // GC__RB_PLUS_ADDRESSING == 1
            pInfo->gfx9.numSdpInterfaces               = 4; // GC__NUM_SDP
            pInfo->gfx9.maxNumCuPerSh                  = 6; // (GC__NUM_WGP0_PER_SA (3) + GC__NUM_WGP1_PER_SA (0)) * 2
            pInfo->gfx9.maxNumRbPerSe                  = 4; // GC__NUM_RB_PER_SA (2) * NUM_SA (2) (may eventually be 3)
            pInfo->gfx9.numPackerPerSc                 = 4;
            pInfo->gfx9.parameterCacheLines            = 256;
            pInfo->gfx9.gfx10.numGl2a                  = 4; // GC__NUM_GL2A
            pInfo->gfx9.gfx10.numGl2c                  = 4; // GC__NUM_GL2C
            pInfo->gfx9.gfx10.numWgpAboveSpi           = 3; // GPU__GC__NUM_WGP0_PER_SA
            pInfo->gfx9.gfx10.numWgpBelowSpi           = 0; // GPU__GC__NUM_WGP1_PER_SA
            pInfo->gfxip.supportCaptureReplay          = 0;
            pInfo->gfx9.supportInt8Dot                 = 1;
            pInfo->gfx9.supportInt4Dot                 = 1;
        }
        else
        {
            PAL_ASSERT_ALWAYS_MSG("Unknown RMB Revision %d", pInfo->eRevId);
        }

        // The GL2C is the TCC.
        pInfo->gfx9.numTccBlocks = pInfo->gfx9.gfx10.numGl2c;
        break;

#if PAL_BUILD_NAVI3X
    case FAMILY_NV3:
        pInfo->gpuType = GpuType::Discrete;

        pInfo->gfx9.rbPlus          = 1;
        pInfo->gfx9.supportSpp      = 1;
        pInfo->gfx9.supportFp16Dot2 = 1;
        pInfo->gfx9.supportInt8Dot  = 1;
        pInfo->gfx9.supportInt4Dot  = 1;

        pInfo->imageProperties.flags.supportsCornerSampling = 1;

        //  Navi3x products don't support EQAA
        pInfo->imageProperties.msaaSupport = static_cast<MsaaFlags>(MsaaS1F1 | MsaaS2F2 | MsaaS4F4 | MsaaS8F8);

#if PAL_BUILD_NAVI31
        if (AMDGPU_IS_NAVI31(pInfo->familyId, pInfo->eRevId))
        {
            pInfo->revision              = AsicRevision::Navi31;
            pInfo->gfxStepping           = Abi::GfxIpSteppingNavi31;
            pInfo->gfx9.numShaderEngines = 6;
            pInfo->gfx9.numSdpInterfaces = 16;
            pInfo->gfx9.maxNumCuPerSh    = 8;
            pInfo->gfx9.maxNumRbPerSe    = 4;
            pInfo->gfx9.numPackerPerSc   = 4;
            pInfo->gfx9.gfx10.numGl2a    = 4;  // GC__NUM_GL2A
            pInfo->gfx9.gfx10.numGl2c    = 24; // GC__NUM_GL2C
            pInfo->gfxip.mallSizeInBytes = 96_MiB;

            pInfo->gfx9.gfx10.numWgpAboveSpi = 4; // GPU__GC__NUM_WGP0_PER_SA
            pInfo->gfx9.gfx10.numWgpBelowSpi = 0; // GPU__GC__NUM_WGP1_PER_SA
        }
        else
#endif
        {
            PAL_ASSERT_ALWAYS_MSG("Unknown NV3 Revision %d", pInfo->eRevId);
        }

        // The GL2C is the TCC.
        pInfo->gfx9.numTccBlocks = pInfo->gfx9.gfx10.numGl2c;
        break;
#endif

    case FAMILY_RPL:
        if (AMDGPU_IS_RAPHAEL(pInfo->familyId, pInfo->eRevId))
        {
            pInfo->gpuType                             = GpuType::Integrated;
            pInfo->revision                            = AsicRevision::Raphael;
            pInfo->gfxStepping                         = Abi::GfxIpSteppingRaphael;
            pInfo->gfx9.numShaderEngines               = 1; // GC__NUM_SE
            pInfo->gfx9.rbPlus                         = 1; // GC__RB_PLUS_ADDRESSING == 1
            pInfo->gfx9.numSdpInterfaces               = 2; // GC__NUM_SDP
            pInfo->gfx9.maxNumCuPerSh                  = 2;
            pInfo->gfx9.maxNumRbPerSe                  = 1;
            pInfo->gfx9.parameterCacheLines            = 256;

            pInfo->gfx9.gfx10.numGl2a                  = 2; // GC__NUM_GL2A
            pInfo->gfx9.gfx10.numGl2c                  = 2; // GC__NUM_GL2C
            pInfo->gfx9.gfx10.numWgpAboveSpi           = 1; // GPU__GC__NUM_WGP0_PER_SA
            pInfo->gfx9.gfx10.numWgpBelowSpi           = 0; // GPU__GC__NUM_WGP1_PER_SA

            pInfo->gfxip.supportCaptureReplay          = 0;
            pInfo->gfx9.supportInt8Dot                 = 1;
            pInfo->gfx9.supportInt4Dot                 = 1;
        }
        else
        {
            PAL_ASSERT_ALWAYS_MSG("Unknown RPL Revision %d", pInfo->eRevId);
        }

        // The GL2C is the TCC.
        pInfo->gfx9.numTccBlocks = pInfo->gfx9.gfx10.numGl2c;
        break;

    case FAMILY_MDN:
        if (AMDGPU_IS_MENDOCINO(pInfo->familyId, pInfo->eRevId))
        {
            pInfo->gpuType                             = GpuType::Integrated;
            pInfo->revision                            = AsicRevision::Raphael;
            pInfo->gfxStepping                         = Abi::GfxIpSteppingRaphael;
            pInfo->gfx9.numShaderEngines               = 1; // GC__NUM_SE
            pInfo->gfx9.rbPlus                         = 1; // GC__RB_PLUS_ADDRESSING == 1
            pInfo->gfx9.numSdpInterfaces               = 2; // GC__NUM_SDP
            pInfo->gfx9.maxNumCuPerSh                  = 2;
            pInfo->gfx9.maxNumRbPerSe                  = 1;
            pInfo->gfx9.parameterCacheLines            = 256;

            pInfo->gfx9.gfx10.numGl2a                  = 2; // GC__NUM_GL2A
            pInfo->gfx9.gfx10.numGl2c                  = 2; // GC__NUM_GL2C
            pInfo->gfx9.gfx10.numWgpAboveSpi           = 1; // GPU__GC__NUM_WGP0_PER_SA
            pInfo->gfx9.gfx10.numWgpBelowSpi           = 0; // GPU__GC__NUM_WGP1_PER_SA

            pInfo->gfxip.supportCaptureReplay          = 0;
            pInfo->gfx9.supportInt8Dot                 = 1;
            pInfo->gfx9.supportInt4Dot                 = 1;
        }
        else
        {
            PAL_ASSERT_ALWAYS_MSG("Unknown MDN Revision %d", pInfo->eRevId);
        }

        // The GL2C is the TCC.
        pInfo->gfx9.numTccBlocks = pInfo->gfx9.gfx10.numGl2c;
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    pInfo->gfx9.numActiveShaderEngines = pInfo->gfx9.numShaderEngines;

    pInfo->srdSizes.bufferView = sizeof(BufferSrd);
    pInfo->srdSizes.imageView  = sizeof(ImageSrd);
    pInfo->srdSizes.fmaskView  = sizeof(ImageSrd);
    pInfo->srdSizes.sampler    = sizeof(SamplerSrd);

    pInfo->nullSrds.pNullBufferView = &nullBufferView;
    pInfo->nullSrds.pNullImageView  = &nullImageView;
    pInfo->nullSrds.pNullFmaskView  = &nullImageView;
    pInfo->nullSrds.pNullSampler    = &NullSampler;

    // Setup anything specific to a given GFXIP level here
    pInfo->gfx9.rayTracingIp = RayTracingIpLevel::_None;
    if (pInfo->gfxLevel == GfxIpLevel::GfxIp9)
    {
        nullBufferView.gfx9.word3.bits.TYPE = SQ_RSRC_BUF;
        nullImageView.gfx9.word3.bits.TYPE  = SQ_RSRC_IMG_2D_ARRAY;

        pInfo->imageProperties.maxImageArraySize = Gfx9MaxImageArraySlices;

        pInfo->gfx9.supportOutOfOrderPrimitives = 1;
    }
    else if (IsGfx10(pInfo->gfxLevel))
    {
        if (IsGfx103Plus(pInfo->gfxLevel)
            )
        {
            pInfo->srdSizes.bvh = sizeof(sq_bvh_rsrc_t);

            {
                pInfo->gfx9.supportIntersectRayBarycentrics = 1;
            }
        }
        if (IsGfx103PlusExclusive(pInfo->gfxLevel))
        {
            pInfo->gfx9.supportSortAgnosticBarycentrics = 1;
        }

        nullBufferView.gfx10.type = SQ_RSRC_BUF;
        nullImageView.gfx10.type  = SQ_RSRC_IMG_2D_ARRAY;

        pInfo->imageProperties.maxImageArraySize  = Gfx10MaxImageArraySlices;

        if (IsGfx103Plus(pInfo->gfxLevel))
        {
            pInfo->imageProperties.flags.supportDisplayDcc = 1;
        }

        // Programming of the various wave-size parameters started with GFX10 parts
        pInfo->gfx9.supportPerShaderStageWaveSize = 1;
        pInfo->gfx9.supportCustomWaveBreakSize    = 1;
        pInfo->gfx9.support1xMsaaSampleLocations  = 1;
        pInfo->gfx9.supportSpiPrefPriority        = 1;

        if (IsGfx103Plus(pInfo->gfxLevel))
        {
            pInfo->gfx9.rayTracingIp = RayTracingIpLevel::RtIp1_1;

        }
    }
#if PAL_BUILD_GFX11
    else if (IsGfx11(pInfo->gfxLevel))
    {
        // GFX11 doesn't support cMask or fMask
        pInfo->srdSizes.fmaskView      = 0;
        pInfo->nullSrds.pNullFmaskView = nullptr;

        // BVH used for ray-tracing is supported though.
        pInfo->srdSizes.bvh        = sizeof(sq_bvh_rsrc_t);
        pInfo->gfx9.supportIntersectRayBarycentrics = 1;

        nullBufferView.gfx10.type = SQ_RSRC_BUF;
        nullImageView.gfx10.type  = SQ_RSRC_IMG_2D_ARRAY;

        pInfo->imageProperties.maxImageArraySize       = Gfx10MaxImageArraySlices;
        pInfo->imageProperties.flags.supportDisplayDcc = 1;

        pInfo->gfx9.supportPerShaderStageWaveSize   = 1;
        pInfo->gfx9.supportCustomWaveBreakSize      = 1;
        pInfo->gfx9.support1xMsaaSampleLocations    = 1;
        pInfo->gfx9.supportSpiPrefPriority          = 1;
        pInfo->gfx9.supportSortAgnosticBarycentrics = 1;
        pInfo->gfx9.supportRayTraversalStack        = 1;
        pInfo->gfx9.supportPointerFlags             = 1;
        pInfo->gfx9.supportMsFullRangeRtai          = 1;

        pInfo->gfx9.rayTracingIp = RayTracingIpLevel::RtIp2_0;
    }
#endif

}

// =====================================================================================================================
// Finalizes the GPU chip properties for a Device object, specifically for the GFX9 hardware layer. Intended to be
// called after InitializeGpuChipProperties().
void FinalizeGpuChipProperties(
    const Pal::Device& device,
    GpuChipProperties* pInfo)
{
    // Setup some GPU properties which can be derived from other properties:

    // Total number of physical CU's (before harvesting)
    pInfo->gfx9.numPhysicalCus = (pInfo->gfx9.numShaderEngines *
                                  pInfo->gfx9.numShaderArrays  *
                                  pInfo->gfx9.maxNumCuPerSh);

    // GPU__GC__NUM_SE * GPU__GC__NUM_RB_PER_SE
    pInfo->gfx9.numTotalRbs = (pInfo->gfx9.numShaderEngines * pInfo->gfx9.maxNumRbPerSe);

    // We need to increase MaxNumRbs if this assert triggers.
    PAL_ASSERT(pInfo->gfx9.numTotalRbs <= MaxNumRbs);

    // Active RB counts will be overridden if any RBs are disabled.
    pInfo->gfx9.numActiveRbs     = pInfo->gfx9.numTotalRbs;
    pInfo->gfx9.activeNumRbPerSe = pInfo->gfx9.maxNumRbPerSe;

    // GPU__GC__NUM_SE
    pInfo->primsPerClock = pInfo->gfx9.numShaderEngines;

    // Loop over each shader array and shader engine to determine actual number of active CU's (total and per SA/SE).
    uint32 numActiveCus   = 0;
    uint32 numAlwaysOnCus = 0;
    for (uint32 se = 0; se < pInfo->gfx9.numShaderEngines; ++se)
    {
        bool seActive = false;
        for (uint32 sa = 0; sa < pInfo->gfx9.numShaderArrays; ++sa)
        {
            const uint32 cuActiveMask    = pInfo->gfx9.activeCuMask[se][sa];
            const uint32 cuActiveCount   = CountSetBits(cuActiveMask);
            numActiveCus += cuActiveCount;

            const uint32 cuAlwaysOnMask  = pInfo->gfx9.alwaysOnCuMask[se][sa];
            const uint32 cuAlwaysOnCount = CountSetBits(cuAlwaysOnMask);
            numAlwaysOnCus += cuAlwaysOnCount;

            // For gfx9 it is expected that all SA's/SE's have the same number of CU's.
            PAL_ASSERT((pInfo->gfxLevel != GfxIpLevel::GfxIp9) ||
                       (pInfo->gfx9.numCuPerSh == 0)           ||
                       (pInfo->gfx9.numCuPerSh == cuActiveCount));
            pInfo->gfx9.numCuPerSh = Max(pInfo->gfx9.numCuPerSh, cuActiveCount);

            if (cuActiveCount != 0)
            {
                seActive = true;
            }
        }
        if (seActive)
        {
            pInfo->gfx9.activeSeMask |= (1 << se);
        }
    }
    pInfo->gfx9.numActiveShaderEngines = CountSetBits(pInfo->gfx9.activeSeMask);
    PAL_ASSERT((pInfo->gfx9.numCuPerSh > 0) && (pInfo->gfx9.numCuPerSh <= pInfo->gfx9.maxNumCuPerSh));
    pInfo->gfx9.numActiveCus   = numActiveCus;
    pInfo->gfx9.numAlwaysOnCus = numAlwaysOnCus;
    PAL_ASSERT((pInfo->gfx9.numActiveCus > 0) && (pInfo->gfx9.numActiveCus <= pInfo->gfx9.numPhysicalCus));
    PAL_ASSERT((pInfo->gfx9.numAlwaysOnCus > 0) && (pInfo->gfx9.numAlwaysOnCus <= pInfo->gfx9.numPhysicalCus));
    if (IsGfx10Plus(pInfo->gfxLevel))
    {
        pInfo->gfx9.nativeWavefrontSize = 32;
    }

    memset(pInfo->gfxip.activePixelPackerMask, 0, sizeof(pInfo->gfxip.activePixelPackerMask));
    const uint32 numPixelPackersPerSe = pInfo->gfx9.numScPerSe * pInfo->gfx9.numPackerPerSc;
    PAL_ASSERT(numPixelPackersPerSe <= MaxPixelPackerPerSe);
    // By default, set all pixel packers to active based on the number of packers in a SE on a particular ASIC.
    // eg. if an ASIC has 2 pixel packers per SE with 4 shader engines, then packerMask = ... 0011 0011 0011 0011
    for (uint32 se = 0; se < pInfo->gfx9.numShaderEngines; ++se)
    {
        if (TestAnyFlagSet(pInfo->gfx9.activeSeMask, 1u << se))
        {
            for (uint32 packer = 0; packer < numPixelPackersPerSe; ++packer)
            {
                WideBitfieldSetBit(pInfo->gfxip.activePixelPackerMask, packer + (MaxPixelPackerPerSe * se));
            }
        }
    }

    // Initialize the performance counter info.  Perf counter info is reliant on a finalized GpuChipProperties
    // structure, so wait until the pInfo->gfx9 structure is "good to go".
    InitPerfCtrInfo(device, pInfo);
}

// =====================================================================================================================
// Initializes the performance experiment properties for this GPU.
void InitializePerfExperimentProperties(
    const GpuChipProperties&  chipProps,
    PerfExperimentProperties* pProperties)  // out
{
    const Gfx9PerfCounterInfo& perfCounterInfo = chipProps.gfx9.perfCounterInfo;

    pProperties->features.u32All       = perfCounterInfo.features.u32All;
    pProperties->maxSqttSeBufferSize   = static_cast<size_t>(SqttMaximumBufferSize);
    pProperties->sqttSeBufferAlignment = static_cast<size_t>(SqttBufferAlignment);
    pProperties->shaderEngineCount     = chipProps.gfx9.numActiveShaderEngines;

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

            if (blockIdx == static_cast<uint32>(GpuBlock::DfMall))
            {
                // For DF SPM, the max number of counters is equal to the number of global counters
                pBlock->maxSpmCounters = blockInfo.numGlobalOnlyCounters;
            }

            // Note that the current interface says the shared count includes all global counters. This seems
            // to be contradictory, how can something be shared and global-only? Regardless, we cannot change this
            // without a major interface change so we must compute the total number of global counters here.
            pBlock->maxGlobalSharedCounters = blockInfo.numGlobalSharedCounters + blockInfo.numGlobalOnlyCounters;
        }
    }
}

// =====================================================================================================================
// Initialize default values for the GPU engine properties.
void InitializeGpuEngineProperties(
    const GpuChipProperties&  chipProps,
    GpuEngineProperties*      pInfo)
{
    const GfxIpLevel gfxIpLevel = chipProps.gfxLevel;

    auto*const  pUniversal = &pInfo->perEngine[EngineTypeUniversal];

    // We support If/Else/While on the universal and compute queues; the command stream controls the max nesting depth.
    pUniversal->flags.timestampSupport                = 1;
    pUniversal->flags.borderColorPaletteSupport       = 1;
    pUniversal->flags.queryPredicationSupport         = 1;
    pUniversal->flags.memory32bPredicationEmulated    = 1; // Emulated by embedding a 64-bit predicate in the cmdbuf and copying from the 32-bit source.
    pUniversal->flags.memory64bPredicationSupport     = 1;
    pUniversal->flags.conditionalExecutionSupport     = 1;
    pUniversal->flags.loopExecutionSupport            = 1;
    pUniversal->flags.constantEngineSupport           = (chipProps.gfxip.ceRamSize != 0);
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

    if ((IsGfx9(gfxIpLevel)      && (chipProps.cpUcodeVersion >= 52)) ||
        (IsGfx10Plus(gfxIpLevel) && (chipProps.cpUcodeVersion >= 32))
#if  PAL_BUILD_GFX11
        || IsGfx104Plus(gfxIpLevel)
#endif
        )
    {
        pUniversal->flags.memory32bPredicationSupport = 1;
    }

#if PAL_BUILD_GFX11
    if (IsGfx11(gfxIpLevel))
    {
        pUniversal->flags.supportsPws = 1;
    }
    else
    {
        pUniversal->flags.supportsPws = 0;
    }
#endif

    auto*const pCompute = &pInfo->perEngine[EngineTypeCompute];

    pCompute->flags.timestampSupport                = 1;
    pCompute->flags.borderColorPaletteSupport       = 1;
    pCompute->flags.queryPredicationSupport         = 1;
    pCompute->flags.memory32bPredicationSupport     = 1;
    pCompute->flags.memory64bPredicationSupport     = 1;
    pCompute->flags.conditionalExecutionSupport     = 1;
    pCompute->flags.loopExecutionSupport            = 1;
    pCompute->flags.regMemAccessSupport             = 1;
    pCompute->flags.indirectBufferSupport           = 1;
    pCompute->flags.supportsMismatchedTileTokenCopy = 1;
    pCompute->flags.supportsImageInitBarrier        = 1;
    pCompute->flags.supportsImageInitPerSubresource = 1;
    pCompute->flags.supportsUnmappedPrtPageAccess   = 1;
    pCompute->maxControlFlowNestingDepth            = CmdStream::CntlFlowNestingLimit;
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

    if (IsGfx10Plus(gfxIpLevel)
    )
    {
        // SDMA engine is part of GFXIP for GFX10+, so set that up here
        auto*const pDma = &pInfo->perEngine[EngineTypeDma];

        pDma->flags.timestampSupport               = 1;
        pDma->flags.memory32bPredicationSupport    = 0;
        pDma->flags.memory64bPredicationSupport    = 1;
        pDma->minTiledImageCopyAlignment.width     = 16;
        pDma->minTiledImageCopyAlignment.height    = 16;
        pDma->minTiledImageCopyAlignment.depth     = 8;
        pDma->minTiledImageMemCopyAlignment.width  = 1;
        pDma->minTiledImageMemCopyAlignment.height = 1;
        pDma->minTiledImageMemCopyAlignment.depth  = 1;
        pDma->minLinearMemCopyAlignment.width      = 4;
        pDma->minLinearMemCopyAlignment.height     = 1;
        pDma->minLinearMemCopyAlignment.depth      = 1;
        pDma->minTimestampAlignment                = 8; // The OSSIP 5.0 spec requires 64-bit alignment.
        pDma->queueSupport                         = SupportQueueTypeDma;
    }

    // Note that we set this DMA state in the GFXIP layer because it deals with GFXIP features that the OSSIP layer
    // doesn't need to understand. Gfx9 can't support per-subresource initialization on DMA because the metadata
    // is interleaved.
    pInfo->perEngine[EngineTypeDma].flags.supportsImageInitBarrier        = 1;
    pInfo->perEngine[EngineTypeDma].flags.supportsMismatchedTileTokenCopy = 1;
    pInfo->perEngine[EngineTypeDma].flags.supportsUnmappedPrtPageAccess   = 1;
}

// =====================================================================================================================
// Returns the value for the DB_DFSM_CONTROL register
uint32 Device::GetDbDfsmControl() const
{
#if PAL_BUILD_GFX11
    PAL_ASSERT(IsGfx11(m_gfxIpLevel) == false);
#endif

    regDB_DFSM_CONTROL dbDfsmControl = {};

    // Force off DFSM.
    dbDfsmControl.most.PUNCHOUT_MODE = DfsmPunchoutModeForceOff;

    return dbDfsmControl.u32All;
}

// =====================================================================================================================
// Returns the hardware's maximum possible value for HW shader stage WAVE_LIMIT/WAVES_PER_SH register settings.
uint32 Device::GetMaxWavesPerSh(
    const GpuChipProperties& chipProps,
    bool                     isCompute)
{
    const uint32 numWavefrontsPerCu    = (chipProps.gfx9.numSimdPerCu * chipProps.gfx9.numWavesPerSimd);
    const uint32 maxWavesPerShUnitSize = isCompute ? 1 : Gfx9MaxWavesPerShGraphicsUnitSize;
    return (numWavefrontsPerCu * chipProps.gfx9.maxNumCuPerSh) / maxWavesPerShUnitSize;
}

// =====================================================================================================================
// Returns the GB_ADDR_CONFIG register associated with this device which contains all kinds of useful info.
const regGB_ADDR_CONFIG& Device::GetGbAddrConfig() const
{
    return *(reinterpret_cast<const regGB_ADDR_CONFIG*>(&m_gbAddrConfig));
}

// =====================================================================================================================
// Returns the value of GB_ADDR_CONFIG.PIPE_INTERLEAVE_SIZE associated with this device.
uint32 Device::GetPipeInterleaveLog2() const
{
    // The possible values for the pipe-interleave are:
    //    Value               Enum name                    Log2
    //      0         ADDR_CONFIG_PIPE_INTERLEAVE_256B      8
    //      1         ADDR_CONFIG_PIPE_INTERLEAVE_512B      9
    //      2         ADDR_CONFIG_PIPE_INTERLEAVE_1KB       10
    //      3         ADDR_CONFIG_PIPE_INTERLEAVE_2KB       11
    return (8 + GetGbAddrConfig().bits.PIPE_INTERLEAVE_SIZE);
}

// =====================================================================================================================
// Creates a GFX9 specific settings loader object
Pal::ISettingsLoader* CreateSettingsLoader(
    Pal::Device* pDevice)
{
    return PAL_NEW(Gfx9::SettingsLoader, pDevice->GetPlatform(), AllocInternal)(pDevice);
 }

// =====================================================================================================================
// Returns one of the BinSizeExtend enumerations that correspond to the specified bin-size.  Doesn't work for a bin
// size of 16 as that's controlled by a separate register field.
uint32 Device::GetBinSizeEnum(
    uint32  binSize)
{
    uint32 binSizeEnum = 0;

    PAL_ASSERT ((binSize >= 32) && (binSize <= 512));
    PAL_ASSERT (IsPowerOfTwo(binSize));

    switch (binSize)
    {
    case 32:
        binSizeEnum = BIN_SIZE_32_PIXELS;
        break;
    case 64:
        binSizeEnum = BIN_SIZE_64_PIXELS;
        break;
    case 128:
        binSizeEnum = BIN_SIZE_128_PIXELS;
        break;
    case 256:
        binSizeEnum = BIN_SIZE_256_PIXELS;
        break;
    case 512:
        binSizeEnum = BIN_SIZE_512_PIXELS;
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return binSizeEnum;
}

// =====================================================================================================================
// Calculates the value of IA_MULTI_VGT_PARAM.PRIMGROUP_SIZE when tessellation is inactive.
uint32 Device::ComputeNoTessPrimGroupSize(
    uint32 targetPrimGroupSize
    ) const
{
    // When non-patch primitives are used without tessellation enabled, PRIMGROUP_SIZE must be at least 4, and must be
    // even if there are more than 2 shader engines on the GPU.
    uint32 primGroupSize = Max(4u, targetPrimGroupSize);
    if (Parent()->ChipProperties().gfx9.numShaderEngines > 2)
    {
        primGroupSize = Pow2Align(primGroupSize, 2);
    }

    // The register specification says that values larger than 256 may cause decreased performance.  This alert serves
    // as a warning to developers that we are risking reduced performance in order to meet the programming requirements
    // of this register field.
    PAL_ALERT(primGroupSize > 256);

    return (primGroupSize - 1); // The hardware adds 1 to the value we specify, so pre-subtract 1 here.
}

// =====================================================================================================================
// Calculates the value of IA_MULTI_VGT_PARAM.PRIMGROUP_SIZE when tessellation is inactive but the input primitive
// topology type is patch primitives.
uint32 Device::ComputeNoTessPatchPrimGroupSize(
    uint32 patchControlPoints
    ) const
{
    // When patch input primitives are used without tessellation enabled, PRIMGROUP_SIZE must never exceed
    // (256 / patchControlPoints).
    uint32 primGroupSize = (256 / patchControlPoints);

    // ...however, the minimum value of PRIMGROUP_SIZE is 4, and for > 2 shader engine GPU's, PRIMGROUP_SIZE must also
    // be even.  Since the maximum supported number of patch control points is 32, this value is guaranteed to always
    // meet the minimum size requirement.
    PAL_ASSERT(primGroupSize >= 4);
    // We must also reduce the prim group size by one if it is odd and we have more than 2 shader engines so that the
    // upper bound of (256 / patchControlPoints) is not exceeded.
    if (Parent()->ChipProperties().gfx9.numShaderEngines > 2)
    {
        primGroupSize = Pow2AlignDown(primGroupSize, 2);
    }

    // The register specification says that values larger than 256 may cause decreased performance.  This alert serves
    // as a warning to developers that we are risking reduced performance in order to meet the programming requirements
    // of this register field.
    PAL_ALERT(primGroupSize > 256);

    return (primGroupSize - 1); // The hardware adds 1 to the value we specify, so pre-subtract 1 here.
}

// =====================================================================================================================
// Calculates the value of IA_MULTI_VGT_PARAM.PRIMGROUP_SIZE when tessellation is active.
uint32 Device::ComputeTessPrimGroupSize(
    uint32 numPatchesPerThreadGroup
    ) const
{
    // When tessellation is enabled, PRIMGROUP_SIZE must be an integer multiple of the number of patches per thread-
    // group.  The recommended multiple is 1.
    uint32 primGroupSize = numPatchesPerThreadGroup;

    // ...however, the minimum value of PRIMGROUP_SIZE is 4, and for > 2 shader engine GPU's, PRIMGROUP_SIZE must also
    // be even.  The following loop will ensure that these requirements are met while still keeping PRIMGROUP_SIZE an
    // integer multiple of the patches-per-thread-group.
    const bool mustBeEven = (Parent()->ChipProperties().gfx9.numShaderEngines > 2);
    while ((primGroupSize < 4) || (mustBeEven && ((primGroupSize & 1) != 0)))
    {
        primGroupSize += numPatchesPerThreadGroup;
    }

    // The register specification says that values larger than 256 may cause decreased performance.  This alert serves
    // as a warning to developers that we are risking reduced performance in order to meet the programming requirements
    // of this register field.
    PAL_ALERT(primGroupSize > 256);

    return (primGroupSize - 1); // The hardware adds 1 to the value we specify, so pre-subtract 1 here.
}

// =====================================================================================================================
// When creating a image used as color target, we increment the corresponding MSAA histogram pile by 1.
void Device::IncreaseMsaaHistogram(
    uint32 samples)
{
    Util::AtomicIncrement(&m_msaaHistogram[Log2(samples)]);
}

// =====================================================================================================================
// When destroying a image being used color target, we decrease the corresponding MSAA histogram pile by 1.
void Device::DecreaseMsaaHistogram(
    uint32 samples)
{
    Util::AtomicDecrement(&m_msaaHistogram[Log2(samples)]);
}

// =====================================================================================================================
// Update MSAA rate and presentable image resolution.
// Return true if the MSAA rate or presentable image resolution gets updated.
// Return false if neither of the spp states has to be updated.
bool Device::UpdateSppState(
    const IImage& presentableImage)
{
    bool updated = false;

    const uint32 resolutionHeight = presentableImage.GetImageCreateInfo().extent.height;
    const uint32 resolutionWidth  = presentableImage.GetImageCreateInfo().extent.width;
    const uint32 preHeight        = Util::AtomicExchange(&m_presentResolution.height, resolutionHeight);
    const uint32 preWidth         = Util::AtomicExchange(&m_presentResolution.width, resolutionWidth);
    if ((preHeight != m_presentResolution.height) || (preWidth != m_presentResolution.width))
    {
        updated = true;
    }

    // We anticipate that every application will have more Msaa1 render targets than any other sampel rate.
    // To properly determine the MSAA rate of the application, we skip Msaa1 and start from Msaa2.
    // If m_msaaHistogram[1], m_msaaHistogram[2], m_msaaHistogram[3] and m_msaaHistogram[4] are all 0,
    // lastestMsaaRate will be 1 << 0.
    uint32 maxMsaaImgCount = 0;
    uint32 latestMsaaRate  = 1 << 0;
    for (uint32 i = 1; i < MsaaLevelCount; i++)
    {
        if (m_msaaHistogram[i] > maxMsaaImgCount)
        {
            latestMsaaRate = 1 << i;
            maxMsaaImgCount = m_msaaHistogram[i];
        }
    }
    if (m_msaaRate != latestMsaaRate)
    {
        m_msaaRate = latestMsaaRate;
        updated = true;
    }

    return updated;
}

// =====================================================================================================================
uint16 Device::GetBaseUserDataReg(
    HwShaderStage  shaderStage
    ) const
{
    uint16  baseUserDataReg = 0;

    switch (shaderStage)
    {
    case HwShaderStage::Hs:
        baseUserDataReg = CmdUtil().GetRegInfo().mmUserDataStartHsShaderStage;
        break;
    case HwShaderStage::Gs:
        baseUserDataReg = CmdUtil().GetRegInfo().mmUserDataStartGsShaderStage;
        break;
    case HwShaderStage::Vs:
        baseUserDataReg = HasHwVs::mmSPI_SHADER_USER_DATA_VS_0;
        break;
    case HwShaderStage::Ps:
        baseUserDataReg = mmSPI_SHADER_USER_DATA_PS_0;
        break;
    case HwShaderStage::Cs:
        baseUserDataReg = mmCOMPUTE_USER_DATA_0;
        break;
    default:
        // What is this?
        PAL_ASSERT_ALWAYS();
        break;
    }

    PAL_ASSERT(baseUserDataReg != 0);

    return baseUserDataReg;
}

// =====================================================================================================================
gpusize Device::GetBaseAddress(
    const BufferSrd*  pBufferSrd
    ) const
{
    gpusize  gpuVirtAddr = 0;

    if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
    {
        gpuVirtAddr = pBufferSrd->gfx9.word1.bits.BASE_ADDRESS_HI;
        gpuVirtAddr = (gpuVirtAddr << 32) + pBufferSrd->gfx9.word0.bits.BASE_ADDRESS;
    }
    else if (IsGfx10Plus(m_gfxIpLevel))
    {
        gpuVirtAddr = pBufferSrd->gfx10.base_address;
    }

    return gpuVirtAddr;
}

// =====================================================================================================================
void Device::SetBaseAddress(
    BufferSrd*  pBufferSrd,
    gpusize     baseAddress
    ) const
{
    if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
    {
        auto*  pSrd = &pBufferSrd->gfx9;

        pSrd->word0.bits.BASE_ADDRESS    = LowPart(baseAddress);
        pSrd->word1.bits.BASE_ADDRESS_HI = HighPart(baseAddress);
    }
    else if (IsGfx10Plus(m_gfxIpLevel))
    {
        pBufferSrd->gfx10.base_address = baseAddress;
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }
}

// =====================================================================================================================
void Device::InitBufferSrd(
    BufferSrd*  pBufferSrd,
    gpusize     gpuVirtAddr,
    gpusize     stride
    ) const
{
    if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
    {
        auto*  pSrd = &pBufferSrd->gfx9;

        pSrd->word0.bits.BASE_ADDRESS    = LowPart(gpuVirtAddr);
        pSrd->word1.bits.BASE_ADDRESS_HI = HighPart(gpuVirtAddr);
        pSrd->word1.bits.STRIDE          = stride;
        pSrd->word1.bits.CACHE_SWIZZLE   = 0;
        pSrd->word1.bits.SWIZZLE_ENABLE  = 0;
        pSrd->word3.bits.DST_SEL_X       = SQ_SEL_X;
        pSrd->word3.bits.DST_SEL_Y       = SQ_SEL_Y;
        pSrd->word3.bits.DST_SEL_Z       = SQ_SEL_Z;
        pSrd->word3.bits.DST_SEL_W       = SQ_SEL_W;
        pSrd->word3.bits.TYPE            = SQ_RSRC_BUF;
        pSrd->word3.bits.NUM_FORMAT      = BUF_NUM_FORMAT_FLOAT;
        pSrd->word3.bits.DATA_FORMAT     = BUF_DATA_FORMAT_32;
        pSrd->word3.bits.ADD_TID_ENABLE  = 0;
    }
    else if (IsGfx10Plus(m_gfxIpLevel))
    {
        auto*  pSrd = &pBufferSrd->gfx10;

        pSrd->base_address   = gpuVirtAddr;
        pSrd->stride         = stride;
        pSrd->dst_sel_x      = SQ_SEL_X;
        pSrd->dst_sel_y      = SQ_SEL_Y;
        pSrd->dst_sel_z      = SQ_SEL_Z;
        pSrd->dst_sel_w      = SQ_SEL_W;
        pSrd->type           = SQ_RSRC_BUF;
        pSrd->add_tid_enable = 0;
        pSrd->oob_select     = SQ_OOB_NUM_RECORDS_0; // never check out-of-bounds

        if (IsGfx10(m_gfxIpLevel))
        {
            pSrd->gfx10Core.resource_level = 1;
            pSrd->gfx10Core.format         = BUF_FMT_32_FLOAT;
            pSrd->gfx10.cache_swizzle      = 0;
            pSrd->gfx10.swizzle_enable     = 0;
        }
#if PAL_BUILD_GFX11
        else if (IsGfx11(m_gfxIpLevel))
        {
            pSrd->gfx104Plus.format    = BUF_FMT_32_FLOAT;
            pSrd->gfx11.swizzle_enable = 0;
        }
#endif
        else
        {
            PAL_ASSERT_ALWAYS();
        }
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }
}

// =====================================================================================================================
void Device::SetNumRecords(
    BufferSrd*  pBufferSrd,
    gpusize     numRecords
    ) const
{
    if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
    {
        pBufferSrd->gfx9.word2.bits.NUM_RECORDS = numRecords;
    }
    else if (IsGfx10Plus(m_gfxIpLevel))
    {
        pBufferSrd->gfx10.num_records = numRecords;
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }
}

// =====================================================================================================================
// Returns the HW color format associated with this image based on the specified format
ColorFormat Device::GetHwColorFmt(
    SwizzledFormat  format
    ) const
{
    const GfxIpLevel  gfxLevel   = Parent()->ChipProperties().gfxLevel;
    ColorFormat       hwColorFmt = COLOR_INVALID;

    if (gfxLevel == GfxIpLevel::GfxIp9)
    {
        const MergedFmtInfo*const pFmtInfo = MergedChannelFmtInfoTbl(gfxLevel, &GetPlatform()->PlatformSettings());
        hwColorFmt = HwColorFmt(pFmtInfo, format.format);
    }
    else if (IsGfx10Plus(m_gfxIpLevel))
    {
        const MergedFlatFmtInfo*const pFmtInfo =
            MergedChannelFlatFmtInfoTbl(gfxLevel, &GetPlatform()->PlatformSettings());
        hwColorFmt = HwColorFmt(pFmtInfo, format.format);
    }

    return hwColorFmt;
}

// =====================================================================================================================
// Returns the HW stencil format associated with this image based on the specified format
StencilFormat Device::GetHwStencilFmt(
    ChNumFormat  format
    ) const
{
    const GfxIpLevel  gfxLevel     = Parent()->ChipProperties().gfxLevel;
    StencilFormat     hwStencilFmt = STENCIL_INVALID;

    if (gfxLevel == GfxIpLevel::GfxIp9)
    {
        const MergedFmtInfo*const pFmtInfo = MergedChannelFmtInfoTbl(gfxLevel, &GetPlatform()->PlatformSettings());
        hwStencilFmt = HwStencilFmt(pFmtInfo, format);
    }
    else if (IsGfx10Plus(m_gfxIpLevel))
    {
        const MergedFlatFmtInfo*const pFmtInfo =
            MergedChannelFlatFmtInfoTbl(gfxLevel, &GetPlatform()->PlatformSettings());
        hwStencilFmt = HwStencilFmt(pFmtInfo, format);
    }

    return hwStencilFmt;
}

// =====================================================================================================================
// Returns the HW Z format associated with this image based on the specified format
ZFormat Device::GetHwZFmt(
    ChNumFormat  format
    ) const
{
    const GfxIpLevel gfxLevel = Parent()->ChipProperties().gfxLevel;
    ZFormat          zFmt     = Z_INVALID;

    if (gfxLevel == GfxIpLevel::GfxIp9)
    {
        const MergedFmtInfo*const pFmtInfo = MergedChannelFmtInfoTbl(gfxLevel, &GetPlatform()->PlatformSettings());

        zFmt = HwZFmt(pFmtInfo, format);
    }
    else if (IsGfx10Plus(m_gfxIpLevel))
    {
        const MergedFlatFmtInfo*const pFmtInfo =
            MergedChannelFlatFmtInfoTbl(gfxLevel, &GetPlatform()->PlatformSettings());

        zFmt = HwZFmt(pFmtInfo, format);
    }

    return zFmt;
}

// =====================================================================================================================
const RegisterRange* Device::GetRegisterRange(
    RegisterRangeType  rangeType,
    uint32*            pRangeEntries
    ) const
{
    const RegisterRange*  pRange  = nullptr;

    if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
    {
        switch (rangeType)
        {
        case RegRangeUserConfig:
            pRange         = Gfx9UserConfigShadowRange;
            *pRangeEntries = Gfx9NumUserConfigShadowRanges;
            break;

        case RegRangeContext:
            pRange         = Gfx9ContextShadowRange;
            *pRangeEntries = Gfx9NumContextShadowRanges;
            break;

        case RegRangeSh:
            if (IsRaven2(*Parent()) || IsRenoir(*Parent()))
            {
                pRange         = Gfx9ShShadowRangeRaven2;
                *pRangeEntries = Gfx9NumShShadowRangesRaven2;
            }
            else
            {
                pRange         = Gfx9ShShadowRange;
                *pRangeEntries = Gfx9NumShShadowRanges;
            }
            break;

        case RegRangeCsSh:
            if (IsRaven2(*Parent()) || IsRenoir(*Parent()))
            {
                pRange         = Gfx9CsShShadowRangeRaven2;
                *pRangeEntries = Gfx9NumCsShShadowRangesRaven2;
            }
            else
            {
                pRange         = Gfx9CsShShadowRange;
                *pRangeEntries = Gfx9NumCsShShadowRanges;
            }
            break;

#if PAL_ENABLE_PRINTS_ASSERTS
        case RegRangeNonShadowed:
            if (IsVega10(*Parent()) || IsRaven(*Parent()))
            {
                pRange         = Gfx90NonShadowedRanges;
                *pRangeEntries = Gfx90NumNonShadowedRanges;
            }
            else
            {
                pRange         = Gfx91NonShadowedRanges;
                *pRangeEntries = Gfx91NumNonShadowedRanges;
            }
            break;
#endif

        default:
            // What is this?
            PAL_ASSERT_ALWAYS();
            break;
        }
    }
    else if (IsGfx10(m_gfxIpLevel))
    {
        switch (rangeType)
        {
        case RegRangeUserConfig:
            if (IsGfx101(*Parent()))
            {
                pRange         = Nv10UserConfigShadowRange;
                *pRangeEntries = Nv10NumUserConfigShadowRanges;
            }
            else if (IsGfx103(*Parent()))
            {
                pRange         = Gfx103UserConfigShadowRange;
                *pRangeEntries = Gfx103NumUserConfigShadowRanges;
            }
            else
            {
                // Need to add UserConfigShadowRange for new ASIC here.
                PAL_ASSERT_ALWAYS();
            }
            break;

        case RegRangeContext:
            if (IsGfx101(*Parent()))
            {
                pRange         = Nv10ContextShadowRange;
                *pRangeEntries = Nv10NumContextShadowRanges;
            }
            else if (IsGfx103(*Parent()))
            {
                pRange         = Gfx103ContextShadowRange;
                *pRangeEntries = Gfx103NumContextShadowRanges;
            }
            else
            {
                // Need to add ContextShadowRange for new ASIC here.
                PAL_ASSERT_ALWAYS();
            }
            break;

        case RegRangeSh:
            pRange         = Gfx10ShShadowRange;
            *pRangeEntries = Gfx10NumShShadowRanges;
            break;

        case RegRangeCsSh:
            pRange         = Gfx10CsShShadowRange;
            *pRangeEntries = Gfx10NumCsShShadowRanges;
            break;

#if PAL_ENABLE_PRINTS_ASSERTS
        case RegRangeNonShadowed:
            if (IsGfx101(*Parent()))
            {
                pRange         = Navi10NonShadowedRanges;
                *pRangeEntries = Navi10NumNonShadowedRanges;
            }
            else if (IsGfx103(*Parent()))
            {
                pRange         = Gfx103NonShadowedRanges;
                *pRangeEntries = Gfx103NumNonShadowedRanges;
            }
            else
            {
                // Need to add NonShadowedRanges for new ASIC here.
                PAL_ASSERT_ALWAYS();
            }
            break;
#endif

        default:
            // What is this?
            PAL_ASSERT_ALWAYS();
            break;
        }
    }
#if PAL_BUILD_GFX11
    else if (IsGfx11(m_gfxIpLevel))
    {
        switch (rangeType)
        {
        case RegRangeUserConfig:
            pRange         = Gfx11UserConfigShadowRange;
            *pRangeEntries = Gfx11NumUserConfigShadowRanges;
            break;

        case RegRangeContext:
            {
                pRange         = Gfx11ContextShadowRange;
                *pRangeEntries = Gfx11NumContextShadowRanges;
            }
            break;

        case RegRangeSh:
             pRange        = Gfx11ShShadowRange;
            *pRangeEntries = Gfx11NumShShadowRanges;
            break;

        case RegRangeCsSh:
            pRange         = Gfx11CsShShadowRange;
            *pRangeEntries = Gfx11NumCsShShadowRanges;
            break;

#if PAL_ENABLE_PRINTS_ASSERTS
        case RegRangeNonShadowed:
            pRange         = Gfx11NonShadowedRanges;
            *pRangeEntries = Gfx11NumNonShadowedRanges;
            break;
#endif

        case RegRangeCpRs64InitSh:
            pRange = Gfx11CpRs64InitShRanges;
            *pRangeEntries = Gfx11NumCpRs64InitShRanges;
            break;

        case RegRangeCpRs64InitCsSh:
            pRange = Gfx11CpRs64InitCsShRanges;
            *pRangeEntries = Gfx11NumCpRs64InitCsShRanges;
            break;

        case RegRangeCpRs64InitUserConfig:
            pRange = Gfx11CpRs64InitUserConfigRanges;
            *pRangeEntries = Gfx11NumCpRs64InitUserConfigRanges;
            break;

        default:
            // What is this?
            PAL_ASSERT_ALWAYS();
            break;
        }
    }
#endif

    PAL_ASSERT(pRange != nullptr);

    return pRange;
}

// =====================================================================================================================
// Computes the CONTEXT_CONTROL value that should be used for universal engine submissions.  This will vary based on
// whether preemption is enabled or not, and the gfx ip level.  This exists as a helper function since there are cases
// where the command buffer may want to temporarily override the default value written by the queue context, and it
// needs to be able to restore it to the proper original value.
PM4_PFP_CONTEXT_CONTROL Device::GetContextControl() const
{
    PM4_PFP_CONTEXT_CONTROL contextControl = { };

    // Since PAL doesn't preserve GPU state across command buffer boundaries, we always need to enable loading
    // context and SH registers.
    contextControl.ordinal2.bitfields.update_load_enables    = 1;
    contextControl.ordinal2.bitfields.load_per_context_state = 1;
    contextControl.ordinal2.bitfields.load_cs_sh_regs        = 1;
    contextControl.ordinal2.bitfields.load_gfx_sh_regs       = 1;
    contextControl.ordinal3.bitfields.update_shadow_enables  = 1;

    if (UseStateShadowing(EngineType::EngineTypeUniversal))
    {
        // If state shadowing is enabled, then we enable shadowing and loading for all register types,
        // because if preempted the GPU state needs to be properly restored when the Queue resumes.
        // (Config registers are exempted because we don't write config registers in PAL.)
        contextControl.ordinal2.bitfields.load_global_uconfig      = 1;
        contextControl.ordinal2.bitfields.core.load_ce_ram         = 1;
        contextControl.ordinal3.bitfields.shadow_per_context_state = 1;
        contextControl.ordinal3.bitfields.shadow_cs_sh_regs        = 1;
        contextControl.ordinal3.bitfields.shadow_gfx_sh_regs       = 1;
        contextControl.ordinal3.bitfields.shadow_global_config     = 1;
        contextControl.ordinal3.bitfields.shadow_global_uconfig    = 1;
    }

#if PAL_BUILD_GFX11
    if (IsGfx11(*Parent()))
    {
        // No CE RAM on GFX11 devices.
        contextControl.ordinal2.bitfields.core.load_ce_ram = 0;

    }
#endif

    return contextControl;
}

// =====================================================================================================================
// Returns bits [31..16] of the CU_EN fields
uint32 Device::GetCuEnableMaskHi(
    uint32 disabledCuMask,          // Mask of CU's to explicitly disabled.  These CU's are virtualized so that PAL
                                    // doesn't need to worry about any yield-harvested CU's.
    uint32 enabledCuMaskSetting     // Mask of CU's a shader can run on based on a setting
    ) const
{
    // GFX10 and newer parts have expanded the CU_EN fields to 32-bits, no other GPU should be calling this
    PAL_ASSERT(IsGfx10Plus(m_gfxIpLevel));

    return (GetCuEnableMaskInternal(disabledCuMask, enabledCuMaskSetting) >> 16);
}

// =====================================================================================================================
// Implements a portion of the Vega10 P2P BLT workaround by modifying a list of memory copy regions so that it is
// composed of multiple, small chunks as required by the workaround.  For each modified region, a chunkAddr is reported
// that is the VA where the region begins in memory.
Result Device::P2pBltWaModifyRegionListMemory(
    const IGpuMemory&       dstGpuMemory,
    uint32                  regionCount,
    const MemoryCopyRegion* pRegions,
    uint32*                 pNewRegionCount,
    MemoryCopyRegion*       pNewRegions,
    gpusize*                pChunkAddrs
    ) const
{
    Result result = Result::Success;

    const gpusize maxChunkSize = Parent()->ChipProperties().p2pBltWaInfo.maxCopyChunkSize;
    const gpusize baseVa       = dstGpuMemory.Desc().gpuVirtAddr;

    struct LookupItem
    {
        MemoryCopyRegion region;
        gpusize          chunkAddr;
    };
    Deque<LookupItem, Platform> lookupList(GetPlatform());

    bool    needBiggerRegionList = false;
    gpusize chunkVa              = 0;

    for (uint32 i = 0; ((i < regionCount) && (result == Result::Success)); i++)
    {
        if (pRegions[i].copySize > maxChunkSize)
        {
            // Need to split the region to chunks of maxChunkSize size.
            needBiggerRegionList = true;

            const gpusize numChunks = RoundUpQuotient(pRegions[i].copySize, maxChunkSize);
            MemoryCopyRegion region = {};

            for (uint32 j = 0; ((j < numChunks) && (result == Result::Success)); j++)
            {
                const gpusize transferredSize = j * maxChunkSize;
                const gpusize currentCopySize = (j < (numChunks - 1)) ? maxChunkSize :
                                                                        (pRegions[i].copySize - transferredSize);
                region.srcOffset              = pRegions[i].srcOffset + transferredSize;
                region.dstOffset              = pRegions[i].dstOffset + transferredSize;
                region.copySize               = currentCopySize;

                LookupItem regionItem         = {};
                regionItem.region             = region;
                regionItem.chunkAddr          = baseVa + region.dstOffset;
                result                        = lookupList.PushBack(regionItem);
            }
        }
        else
        {
            // No need to split the region
            const gpusize startVa = baseVa + pRegions[i].dstOffset;
            const gpusize endVa   = startVa + pRegions[i].copySize;

            // If current region cannot fit in previous chunk, we need to update chunkVa associating with a new VCOP.
            if ((startVa < chunkVa) || (endVa > (chunkVa + maxChunkSize)))
            {
                chunkVa = startVa;
            }

            LookupItem regionItem = {};
            regionItem.region     = pRegions[i];
            regionItem.chunkAddr  = chunkVa;
            result                = lookupList.PushBack(regionItem);
        }
    }

    const uint32 newRegionCount = static_cast<uint32>(lookupList.NumElements());

    if (result == Result::Success)
    {
        if (pNewRegions == nullptr)
        {
            // Query size required for new region list.
            if (needBiggerRegionList)
            {
                PAL_ASSERT(newRegionCount > regionCount);
            }
            else
            {
                PAL_ASSERT(newRegionCount == regionCount);
            }

            *pNewRegionCount = newRegionCount;
        }
        else
        {
            // Fill new regions into region list.
            PAL_ASSERT(newRegionCount == *pNewRegionCount);

            auto iter = lookupList.Begin();
            for (uint32 i = 0; i < newRegionCount; i++)
            {
                pNewRegions[i] = iter.Get()->region;
                pChunkAddrs[i] = iter.Get()->chunkAddr;
                iter.Next();
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Implements a portion of the Vega10 P2P BLT workaround by modifying a list of image copy regions so that it is
// composed of multiple, small chunks as required by the workaround.  For each modified region, a chunkAddr is reported
// that is the VA where the region begins in memory.
Result Device::P2pBltWaModifyRegionListImage(
    const Pal::Image&      srcImage,
    const Pal::Image&      dstImage,
    uint32                 regionCount,
    const ImageCopyRegion* pRegions,
    uint32*                pNewRegionCount,
    ImageCopyRegion*       pNewRegions,
    gpusize*               pChunkAddrs
    ) const
{
    Result result = Result::Success;

    const gpusize maxChunkSize = Parent()->ChipProperties().p2pBltWaInfo.maxCopyChunkSize;
    const gpusize baseVa       = dstImage.GetBoundGpuMemory().GpuVirtAddr();

    struct LookupItem
    {
        ImageCopyRegion region;
        gpusize         chunkAddr;
    };
    Deque<LookupItem, Platform> lookupList(GetPlatform());

    gpusize chunkVa = 0;

    for (uint32 i = 0; ((i < regionCount) && (result == Result::Success)); i++)
    {
        const SubResourceInfo* pDstSubresInfo = dstImage.SubresourceInfo(pRegions[i].dstSubres);

        const Image*const pDstGfx9Image = static_cast<const Image*>(dstImage.GetGfxImage());
        const ADDR2_COMPUTE_SURFACE_INFO_OUTPUT*const  pAddrOutput = pDstGfx9Image->GetAddrOutput(pDstSubresInfo);
        const gpusize macroBlockOffset  = pAddrOutput->pMipInfo[pDstSubresInfo->subresId.mipLevel].macroBlockOffset;

        const SwizzledFormat dstViewFormat = pDstSubresInfo->format;
        const uint32         bytesPerPixel = Formats::BytesPerPixel(dstViewFormat.format);

        const uint32 subResWidth        = pDstSubresInfo->extentElements.width;
        const uint32 subResHeight       = pDstSubresInfo->extentElements.height;
        const uint32 subResDepth        = pDstSubresInfo->extentElements.depth;
        const uint32 paddedSubresWidth  = pDstSubresInfo->actualExtentElements.width;
        const uint32 paddedSubresHeight = pDstSubresInfo->actualExtentElements.height;
        const uint32 paddedSubresDepth  = pDstSubresInfo->actualExtentElements.depth;

        const gpusize rowPitchInBytes   = pDstSubresInfo->rowPitch;
        const gpusize depthPitchInBytes = pDstSubresInfo->depthPitch;

        uint32 transferWidth  = pRegions[i].extent.width;
        uint32 transferHeight = pRegions[i].extent.height;
        uint32 transferDepth  = pRegions[i].extent.depth;

        const bool is3d = transferDepth > 1;

        PAL_ASSERT((pRegions[i].dstOffset.x >= 0) && (pRegions[i].dstOffset.y >= 0) && (pRegions[i].dstOffset.z >= 0));

        LookupItem stackedRegionItem = { }; // It stacks consecutive slices of an image as long as they still
                                            // fit in one chunk.

        if (dstImage.IsSubResourceLinear(pRegions[i].dstSubres))
        {
            // Linear image's depth can be treated as slice.
            const uint32 loopCount = (is3d) ? transferDepth : pRegions[i].numSlices;

            // Go through each slice separately.
            for (uint32 j = 0; ((j < loopCount) && (result == Result::Success)); j++)
            {
                ImageCopyRegion region = {};
                memcpy(&region, &pRegions[i], sizeof(ImageCopyRegion));
                region.numSlices = 1;
                if (is3d)
                {
                    region.srcOffset.z = pRegions[i].srcOffset.z + j;
                    region.dstOffset.z = pRegions[i].dstOffset.z + j;
                    region.extent.depth = 1;
                }
                else
                {
                    region.srcSubres.arraySlice = pRegions[i].srcSubres.arraySlice + j;
                    region.dstSubres.arraySlice = pRegions[i].dstSubres.arraySlice + j;
                }

                const gpusize sliceBaseVa   = (is3d) ? (baseVa + (depthPitchInBytes * region.dstOffset.z))
                                                     : (baseVa + (depthPitchInBytes * region.dstSubres.arraySlice));

                const gpusize regionPixelRowSize = transferWidth * bytesPerPixel;
                const gpusize vaSpanEntireRegion = rowPitchInBytes * transferHeight;

                // Need to split to 1d (per-row)?
                if (maxChunkSize < vaSpanEntireRegion)
                {
                    if (maxChunkSize < regionPixelRowSize)
                    {
                        // Worst case for one line is 16384 pixels times 16 bytes (R32G32B32A32), which would be 256KB.
                        // Chunk size is not expected to be smaller than 256KB.
                        PAL_ASSERT_ALWAYS();
                    }
                    else
                    {
                        // Each chunk can hold at least one row.
                        const uint32 rowsPerChunk = static_cast<uint32>(maxChunkSize / rowPitchInBytes);
                        const uint32 numChunks    = RoundUpQuotient(transferHeight, rowsPerChunk);

                        // Register each splitted chunk in lookupList for current region.
                        region.dstOffset.x  = pRegions[i].dstOffset.x;
                        region.srcOffset.x  = pRegions[i].srcOffset.x;
                        region.extent.width = pRegions[i].extent.width;
                        PAL_ASSERT(region.dstOffset.x  == pRegions[i].dstOffset.x);
                        PAL_ASSERT(region.srcOffset.x  == pRegions[i].srcOffset.x);
                        PAL_ASSERT(region.extent.width == pRegions[i].extent.width);

                        for (uint32 m = 0; ((m < numChunks) && (result == Result::Success)); m++)
                        {
                            region.dstOffset.y = pRegions[i].dstOffset.y + (rowsPerChunk * m);
                            region.srcOffset.y = pRegions[i].srcOffset.y + (rowsPerChunk * m);

                            if (m == (numChunks - 1))
                            {
                                // Last chunk gets what's leftover.
                                region.extent.height = transferHeight - (rowsPerChunk * m);
                                PAL_ASSERT(region.extent.height > 0);
                            }
                            else
                            {
                                region.extent.height = rowsPerChunk;
                            }

                            // Use the beginning of pixel row to improve VCOP share rate.
                            chunkVa = sliceBaseVa +
                                      macroBlockOffset +
                                      (region.dstOffset.y * rowPitchInBytes);

                            LookupItem regionItem = {};
                            regionItem.region     = region;
                            regionItem.chunkAddr  = chunkVa;
                            result = lookupList.PushBack(regionItem);
                        }
                    }
                }
                else
                {
                    // Entering this path means one chunk can cover the whole slice.  If current region cannot fit in
                    // previous chunk, we need to update chunkVa associating with a new VCOP, otherwise keep using last
                    // chunkVa to avoid creating unnecessary VCOP.
                    const gpusize startVa = sliceBaseVa +
                                            macroBlockOffset +
                                            (region.dstOffset.x * bytesPerPixel) +
                                            (region.dstOffset.y * rowPitchInBytes);
                    const gpusize endVa   = sliceBaseVa +
                                            macroBlockOffset +
                                            ((region.dstOffset.x + region.extent.width) * bytesPerPixel) +
                                            ((region.dstOffset.y + region.extent.height) * rowPitchInBytes);

                    // Update chunkVa if necessary; otherwise the previous chunkVa can cover current region.
                    if ((chunkVa == 0)      ||
                        (startVa < chunkVa) ||
                        (endVa > (chunkVa + maxChunkSize)))
                    {
                        chunkVa = startVa;
                    }

                    // Update regionList.
                    if (j == 0)
                    {
                        PAL_ASSERT(stackedRegionItem.region.numSlices == 0);
                        PAL_ASSERT(stackedRegionItem.chunkAddr == 0);
                        stackedRegionItem.region    = region;
                        stackedRegionItem.chunkAddr = chunkVa;
                    }
                    else
                    {
                        PAL_ASSERT(stackedRegionItem.region.numSlices > 0);
                        PAL_ASSERT(stackedRegionItem.chunkAddr != 0);
                        PAL_ASSERT((stackedRegionItem.region.extent.depth == 1) ||
                                   (stackedRegionItem.region.numSlices == 1));

                        if (chunkVa != stackedRegionItem.chunkAddr)
                        {
                            // chunkVa cannot cover current region, update stackedRegionItem.
                            result = lookupList.PushBack(stackedRegionItem);
                            stackedRegionItem.region    = region;
                            stackedRegionItem.chunkAddr = chunkVa;
                        }
                        else
                        {
                            PAL_ASSERT(stackedRegionItem.region.dstOffset.x == region.dstOffset.x);
                            PAL_ASSERT(stackedRegionItem.region.dstOffset.y == region.dstOffset.y);
                            PAL_ASSERT(stackedRegionItem.region.extent.width  == region.extent.width);
                            PAL_ASSERT(stackedRegionItem.region.extent.height == region.extent.height);
                            if (is3d)
                            {
                                PAL_ASSERT(stackedRegionItem.region.numSlices == region.numSlices);
                                PAL_ASSERT(stackedRegionItem.region.numSlices == 1);
                                stackedRegionItem.region.extent.depth++;
                            }
                            else
                            {
                                PAL_ASSERT(stackedRegionItem.region.dstOffset.z == region.dstOffset.z);
                                PAL_ASSERT(stackedRegionItem.region.extent.depth == region.extent.depth);
                                stackedRegionItem.region.numSlices++;
                            }
                        }
                    }

                    if (j == (loopCount - 1))
                    {
                        // This region cannot take more slice if reaching the end of slice array.
                        result = lookupList.PushBack(stackedRegionItem);
                        memset(&stackedRegionItem, 0, sizeof(LookupItem));
                    }
                }
            }
        }
        else
        {
            // The image is tiled.
            // Go through each slice separately.
            const uint32 loopCount = pRegions[i].numSlices;
            for (uint32 j = 0; ((j < loopCount) && (result == Result::Success)); j++)
            {
                ImageCopyRegion region = {};
                memcpy(&region, &pRegions[i], sizeof(ImageCopyRegion));
                region.srcSubres.arraySlice = pRegions[i].srcSubres.arraySlice + j;
                region.dstSubres.arraySlice = pRegions[i].dstSubres.arraySlice + j;
                region.numSlices = 1;

                // Note: 3D surface only has single slice. So 3D always have sliceBaseVa==baseVa.
                const gpusize sliceBaseVa = baseVa + (depthPitchInBytes * region.dstSubres.arraySlice);

                if ((region.srcOffset.x   == 0)            &&
                    (region.srcOffset.y   == 0)            &&
                    (region.srcOffset.z   == 0)            &&
                    (region.dstOffset.x   == 0)            &&
                    (region.dstOffset.y   == 0)            &&
                    (region.dstOffset.z   == 0)            &&
                    (region.extent.width  == subResWidth)  &&
                    (region.extent.height == subResHeight) &&
                    (region.extent.depth  == subResDepth))
                {
                    transferWidth  = paddedSubresWidth;
                    transferHeight = paddedSubresHeight;
                    transferDepth  = paddedSubresDepth;
                }

                // Get surface info.
                const uint32  blockWidth            = pAddrOutput->blockWidth;
                const uint32  blockHeight           = pAddrOutput->blockHeight;
                const uint32  blockDepth            = pAddrOutput->blockSlices; // For 3D-support only.
                const uint32  mipChainPitch         = pAddrOutput->mipChainPitch;
                const uint32  mipChainHeight        = pAddrOutput->mipChainHeight;
                const uint32  numBlocksSurfWidth    = mipChainPitch / blockWidth;
                const uint32  numBlocksSurfHeight   = mipChainHeight / blockHeight;
                const gpusize blockSize             = blockWidth * blockHeight * blockDepth * bytesPerPixel;
                const gpusize blockRowSizeInBytes   = mipChainPitch * blockHeight * blockDepth * bytesPerPixel;
                const gpusize blockLayerSizeInBytes = mipChainPitch * mipChainHeight * blockDepth * bytesPerPixel;

                PAL_ASSERT((mipChainPitch % blockWidth) == 0);
                PAL_ASSERT((mipChainPitch * bytesPerPixel) == rowPitchInBytes);

                const uint32 copyRegionPaddedHeightInBlocks =
                    (((pRegions[i].dstOffset.y) + transferHeight - 1) / blockHeight) -
                    pRegions[i].dstOffset.y / blockHeight + 1;

                const uint32 copyRegionPaddedDepthInBlockLayers =
                    (((pRegions[i].dstOffset.z) + transferDepth - 1) / blockDepth) -
                    pRegions[i].dstOffset.z / blockDepth + 1;

                // For simplicity, 1d/2d is based on a block row across the whole mipchain;
                // 3d is based on a block layer of x,y coordinates covering the whole mipchain.
                const gpusize vaSpanEntireRegion = is3d ? (blockLayerSizeInBytes * copyRegionPaddedDepthInBlockLayers) :
                                                          (blockRowSizeInBytes * copyRegionPaddedHeightInBlocks);

                if (maxChunkSize < blockLayerSizeInBytes)
                {
                    // Each 2d-layer of tile blocks needs at least one chunk.
                    for (uint32 m = 0; m < copyRegionPaddedDepthInBlockLayers; m++)
                    {
                        // TODO: Wrap into a function.
                        {
                            struct Dim1d
                            {
                                uint32 begin;
                                uint32 end;
                            };
                            Dim1d zSrc = {};
                            Dim1d zDst = {};
                            if (m == 0)
                            {
                                zSrc.begin = pRegions[i].dstOffset.z;
                                zDst.begin = pRegions[i].srcOffset.z;
                            }
                            else
                            {
                                zSrc.begin = RoundDownToMultiple((pRegions[i].srcOffset.z + (blockDepth * m)),
                                                                 blockDepth);
                                zDst.begin = RoundDownToMultiple((pRegions[i].dstOffset.z + (blockDepth * m)),
                                                                 blockDepth);
                            }

                            if (m == copyRegionPaddedDepthInBlockLayers - 1)
                            {
                                zSrc.end = pRegions[i].srcOffset.z + transferDepth - 1;
                                zDst.end = pRegions[i].dstOffset.z + transferDepth - 1;
                            }
                            else
                            {
                                zSrc.end =
                                    RoundDownToMultiple((pRegions[i].srcOffset.z + (blockDepth * (m + 1))), blockDepth)
                                    - 1;
                                zDst.end =
                                    RoundDownToMultiple((pRegions[i].dstOffset.z + (blockDepth * (m + 1))), blockDepth)
                                    - 1;
                            }
                            region.srcOffset.z  = zSrc.begin;
                            region.dstOffset.z  = zDst.begin;
                            region.extent.depth = zDst.end - zDst.begin + 1;
                        }

                        if (maxChunkSize < blockRowSizeInBytes)
                        {
                            // Extreme case, doesn't seem ever happen in apps. Pending support.
                            PAL_NOT_IMPLEMENTED();
                        }
                        else
                        {
                            // Each chunk can hold at lease one row of tile blocks.
                            const uint32 chunkMaxHeightInBlocks = static_cast<uint32>
                                                                    (maxChunkSize / blockRowSizeInBytes);
                            const uint32 chunkHeightInBlocks    = Min(copyRegionPaddedHeightInBlocks,
                                                                      chunkMaxHeightInBlocks);
                            const uint32 chunkHeight            = chunkHeightInBlocks * blockHeight;
                            const uint32 numChunks              = RoundUpQuotient(copyRegionPaddedHeightInBlocks,
                                                                                  chunkHeightInBlocks);

                            region.dstOffset.x  = pRegions[i].dstOffset.x;
                            region.srcOffset.x  = pRegions[i].srcOffset.x;
                            region.extent.width = transferWidth;
                            PAL_ASSERT((transferWidth == pRegions[i].extent.width) ||
                                       (transferWidth == paddedSubresWidth));

                            for (uint32 n = 0; ((n < numChunks) && (result == Result::Success)); n++)
                            {
                                // TODO: Wrap into a function.
                                {
                                    struct Dim1d
                                    {
                                        uint32 begin;
                                        uint32 end;
                                    };
                                    Dim1d ySrc = {};
                                    Dim1d yDst = {};

                                    if (n == 0)
                                    {
                                        ySrc.begin = pRegions[i].srcOffset.y;
                                        yDst.begin = pRegions[i].dstOffset.y;
                                    }
                                    else
                                    {
                                        ySrc.begin = RoundDownToMultiple((pRegions[i].srcOffset.y + (chunkHeight * n)),
                                                                         chunkHeight);
                                        yDst.begin = RoundDownToMultiple((pRegions[i].dstOffset.y + (chunkHeight * n)),
                                                                         chunkHeight);
                                    }

                                    if (n == numChunks - 1)
                                    {
                                        ySrc.end = pRegions[i].srcOffset.y + transferHeight - 1;
                                        yDst.end = pRegions[i].dstOffset.y + transferHeight - 1;
                                    }
                                    else
                                    {
                                        ySrc.end = RoundDownToMultiple(
                                            (pRegions[i].srcOffset.y + (chunkHeight * (n + 1))), chunkHeight) - 1;
                                        yDst.end = RoundDownToMultiple(
                                            (pRegions[i].dstOffset.y + (chunkHeight * (n + 1))), chunkHeight) - 1;
                                    }

                                    region.srcOffset.y   = ySrc.begin;
                                    region.dstOffset.y   = yDst.begin;
                                    region.extent.height = yDst.end - yDst.begin + 1;
                                }

                                const uint32 startBlockX = 0; // Use zero for simplicity.
                                const uint32 startBlockY = region.dstOffset.y / blockHeight;
                                const uint32 startBlockZ = region.dstOffset.z / blockDepth;
                                chunkVa = sliceBaseVa +
                                          macroBlockOffset +
                                          ((startBlockX + ((startBlockY + (startBlockZ * numBlocksSurfHeight))
                                              * numBlocksSurfWidth)) * blockSize);

                                LookupItem regionItem = {};
                                regionItem.region     = region;
                                regionItem.chunkAddr  = chunkVa;
                                result = lookupList.PushBack(regionItem);
                            }
                        }
                    }
                }
                else
                {
                    // Each chunk can hold at lease one 1d/2d/2d*blockDepth layer of tile blocks.
                    const uint32 chunkMaxDepthInBlockLayers = static_cast<uint32>(maxChunkSize / blockLayerSizeInBytes);
                    const uint32 chunkDepthInBlockLayers    = Min(copyRegionPaddedDepthInBlockLayers,
                                                                  chunkMaxDepthInBlockLayers);
                    const uint32 chunkDepth                 = chunkDepthInBlockLayers * blockDepth;
                    const uint32 numChunks                  = RoundUpQuotient(copyRegionPaddedDepthInBlockLayers,
                                                                              chunkDepthInBlockLayers);

                    region.dstOffset.x   = pRegions[i].dstOffset.x;
                    region.srcOffset.x   = pRegions[i].srcOffset.x;
                    region.extent.width  = transferWidth;
                    region.dstOffset.y   = pRegions[i].dstOffset.y;
                    region.srcOffset.y   = pRegions[i].srcOffset.y;
                    region.extent.height = transferHeight;

                    for (uint32 m = 0; ((m < numChunks) && (result == Result::Success)); m++)
                    {
                        // TODO: Wrap into a function.
                        {
                            struct Dim1d
                            {
                                uint32 begin;
                                uint32 end;
                            };
                            Dim1d zSrc = {};
                            Dim1d zDst = {};
                            if (m == 0)
                            {
                                zSrc.begin = pRegions[i].dstOffset.z;
                                zDst.begin = pRegions[i].srcOffset.z;
                            }
                            else
                            {
                                zSrc.begin = RoundDownToMultiple((pRegions[i].srcOffset.z + (chunkDepth * m)),
                                                                 chunkDepth);
                                zDst.begin = RoundDownToMultiple((pRegions[i].dstOffset.z + (chunkDepth * m)),
                                                                 chunkDepth);
                            }

                            if (m == numChunks - 1)
                            {
                                zSrc.end = pRegions[i].srcOffset.z + transferDepth - 1;
                                zDst.end = pRegions[i].dstOffset.z + transferDepth - 1;
                            }
                            else
                            {
                                zSrc.end = RoundDownToMultiple(
                                    (pRegions[i].srcOffset.z + (chunkDepth * (m + 1))), chunkDepth) - 1;
                                zDst.end = RoundDownToMultiple(
                                    (pRegions[i].dstOffset.z + (chunkDepth * (m + 1))), chunkDepth) - 1;
                            }
                            region.srcOffset.z  = zSrc.begin;
                            region.dstOffset.z  = zDst.begin;
                            region.extent.depth = zDst.end - zDst.begin + 1;
                        }

                        if (numChunks == 1)
                        {
                            // Optimization that stacks multi-slice copy-region. (2D image specific, because 3d doesn't
                            // allow multi-slice)
                            const uint32 startBlockX = 0; // Use the beginning of pixel row to improve VCOP share rate.
                            const uint32 startBlockY = region.dstOffset.y / blockHeight;
                            const uint32 startBlockZ = region.dstOffset.z / blockDepth;
                            const gpusize startVa    = sliceBaseVa +
                                                       macroBlockOffset +
                                                       ((startBlockX + ((startBlockY + (startBlockZ *
                                                           numBlocksSurfHeight)) * numBlocksSurfWidth)) * blockSize);
                            const gpusize endVa      = startVa + vaSpanEntireRegion;

                            // Update chunkVa if necessary; otherwise the previous chunkVa can cover current region.
                            if ((chunkVa == 0)      ||
                                (startVa < chunkVa) ||
                                (endVa > (chunkVa + maxChunkSize)))
                            {
                                chunkVa = startVa;
                            }

                            // Update regionList.
                            if (j == 0)
                            {
                                PAL_ASSERT(stackedRegionItem.region.numSlices == 0);
                                PAL_ASSERT(stackedRegionItem.chunkAddr == 0);
                                stackedRegionItem.region    = region;
                                stackedRegionItem.chunkAddr = chunkVa;
                            }
                            else
                            {
                                PAL_ASSERT(stackedRegionItem.region.numSlices != 0);
                                PAL_ASSERT(stackedRegionItem.chunkAddr != 0);

                                if (chunkVa != stackedRegionItem.chunkAddr)
                                {
                                    // chunkVa cannot cover current region, update stackedRegionItem.
                                    result = lookupList.PushBack(stackedRegionItem);
                                    stackedRegionItem.region    = region;
                                    stackedRegionItem.chunkAddr = chunkVa;
                                }
                                else
                                {
                                    PAL_ASSERT(stackedRegionItem.region.dstOffset.x   == region.dstOffset.x);
                                    PAL_ASSERT(stackedRegionItem.region.dstOffset.y   == region.dstOffset.y);
                                    PAL_ASSERT(stackedRegionItem.region.dstOffset.z   == region.dstOffset.z);
                                    PAL_ASSERT(stackedRegionItem.region.extent.width  == region.extent.width);
                                    PAL_ASSERT(stackedRegionItem.region.extent.height == region.extent.height);
                                    PAL_ASSERT(stackedRegionItem.region.extent.depth  == region.extent.depth);
                                    stackedRegionItem.region.numSlices++;
                                }
                            }

                            if (j == (pRegions[i].numSlices - 1))
                            {
                                // This region cannot take more slice if reaching the end of slice array.
                                result = lookupList.PushBack(stackedRegionItem);
                                memset(&stackedRegionItem, 0, sizeof(LookupItem));
                            }
                        }
                        else
                        {
                            PAL_ASSERT(numChunks > 1);
                            const uint32 startBlockX = 0; // Use the beginning of pixel row to improve VCOP share rate.
                            const uint32 startBlockY = region.dstOffset.y / blockHeight;
                            const uint32 startBlockZ = region.dstOffset.z / blockDepth;
                            chunkVa                  = sliceBaseVa +
                                                       macroBlockOffset +
                                                       ((startBlockX + ((startBlockY + (startBlockZ *
                                                           numBlocksSurfHeight)) * numBlocksSurfWidth)) * blockSize);

                            LookupItem regionItem = {};
                            regionItem.region     = region;
                            regionItem.chunkAddr  = chunkVa;
                            result = lookupList.PushBack(regionItem);
                        }
                    }
                }
            }
        }
    } // region loop done

    const uint32 newRegionCount = static_cast<uint32>(lookupList.NumElements());

    if (result == Result::Success)
    {
        if (pNewRegions == nullptr)
        {
            *pNewRegionCount = newRegionCount;
        }
        else
        {
            // Record new region list.
            PAL_ASSERT(newRegionCount == *pNewRegionCount);
            auto iter = lookupList.Begin();
            for (uint32 i = 0; i < newRegionCount; i++)
            {
                pNewRegions[i] = iter.Get()->region;
                pChunkAddrs[i] = iter.Get()->chunkAddr;
                iter.Next();
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Implements a portion of the Vega10 P2P BLT workaround by modifying a list of image to memory copy regions so that it
// is composed of multiple, small chunks as required by the workaround.  For each modified region, a chunkAddr is
// reported that is the VA where the region begins in memory.
Result Device::P2pBltWaModifyRegionListImageToMemory(
    const Pal::Image&            srcImage,
    const IGpuMemory&            dstGpuMemory,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions,
    uint32*                      pNewRegionCount,
    MemoryImageCopyRegion*       pNewRegions,
    gpusize*                     pChunkAddrs
    ) const
{
    PAL_NOT_TESTED();
    Result result = Result::Success;

    const gpusize maxChunkSize = Parent()->ChipProperties().p2pBltWaInfo.maxCopyChunkSize;
    const gpusize baseVa       = dstGpuMemory.Desc().gpuVirtAddr;
    bool needBiggerRegionList  = false;
    gpusize chunkVa            = 0;

    // SplitChunk implementation
    struct LookupItem
    {
        MemoryImageCopyRegion region;
        gpusize               chunkAddr;
    };
    Deque<LookupItem, Platform> lookupList(GetPlatform());

    for (uint32 i = 0; ((i < regionCount) && (result == Result::Success)); i++)
    {
        const SubResourceInfo* pSrcSubresInfo = srcImage.SubresourceInfo(pRegions[i].imageSubres);
        const SwizzledFormat srcViewFormat    = pSrcSubresInfo->format;
        const uint32 bytesPerPixel            = Formats::BytesPerPixel(srcViewFormat.format);

        const uint32 transferWidth  = pRegions[i].imageExtent.width;
        const uint32 transferHeight = pRegions[i].imageExtent.height;
        const uint32 transferDepth  = pRegions[i].imageExtent.depth;
        PAL_ASSERT(transferDepth == 1); // For now we only support buffer, 1D, 2D, 2D slices.

        uint32 numChunks = 1; // Default needs one chunk for this region.

        // Go through each slice separately. If numSlices=N, the region will be split to at least N chunks.
        if (pRegions[i].numSlices > 1)
        {
            needBiggerRegionList = true;
        }

        for (uint32 j = 0; ((j < pRegions[i].numSlices) && (result == Result::Success)); j++)
        {
            const gpusize rowPitchInBytes    = pRegions[i].gpuMemoryRowPitch;
            const gpusize regionPixelRowSize = transferWidth * bytesPerPixel;
            const gpusize vaSpanEntireRegion = rowPitchInBytes * transferHeight;

            MemoryImageCopyRegion region = { };
            memcpy(&region, &pRegions[i], sizeof(MemoryImageCopyRegion));
            region.numSlices = 1;
            region.imageSubres.arraySlice = pRegions[i].imageSubres.arraySlice + j;

            // Need to split this slice region?
            if (maxChunkSize < vaSpanEntireRegion)
            {
                needBiggerRegionList = true;

                if (maxChunkSize < rowPitchInBytes)
                {
                    // Each pixel row needs more than one chunk. (Won't happen because maxChunkSize should always larger
                    // than a pixel row.)
                    PAL_ASSERT_ALWAYS();
                    region.imageExtent.height = 1;

                    const uint32 chunksPerRow       = static_cast<uint32>(RoundUpQuotient(regionPixelRowSize,
                                                                                          maxChunkSize));
                    const uint32 chunkStrideInPixel = static_cast<uint32>(maxChunkSize / bytesPerPixel);

                    // Register each splitted chunk in lookupList for current region.
                    for (uint32 m = 0; ((m < transferHeight) && (result == Result::Success)); m++)
                    {
                        region.imageOffset.y = pRegions[i].imageOffset.y + m;
                        for (uint32 n = 0; ((n < chunksPerRow) && (result == Result::Success)); n++)
                        {
                            region.imageOffset.x = pRegions[i].imageOffset.x + chunkStrideInPixel*n;

                            if (n == (chunksPerRow - 1))
                            {
                                // Last chunk gets what's leftover.
                                region.imageExtent.width = transferWidth - chunkStrideInPixel*n;
                                PAL_ASSERT(transferWidth > chunkStrideInPixel*n);
                            }
                            else
                            {
                                region.imageExtent.width = chunkStrideInPixel;
                            }

                            region.gpuMemoryOffset =
                                pRegions[i].gpuMemoryOffset +
                                ((region.imageOffset.x - pRegions[i].imageOffset.x) * bytesPerPixel) +
                                ((region.imageOffset.y - pRegions[i].imageOffset.y) * region.gpuMemoryRowPitch);

                            chunkVa               = baseVa + region.gpuMemoryOffset;

                            LookupItem regionItem = {};
                            regionItem.region     = region;
                            regionItem.chunkAddr  = chunkVa;
                            result = lookupList.PushBack(regionItem);
                        }
                    }
                }
                else
                {
                    // Each chunk can hold at lease one pixel row.
                    const uint32 rowsPerChunk = static_cast<uint32>(maxChunkSize / rowPitchInBytes);
                    numChunks = RoundUpQuotient(transferHeight, rowsPerChunk); // round-up

                    // Register each splitted chunk in lookupList for current region.
                    for (uint32 m = 0; ((m < numChunks) && (result == Result::Success)); m++)
                    {
                        region.imageOffset.y = pRegions[i].imageOffset.y + (rowsPerChunk * m);

                        if (m != (numChunks - 1))
                        {
                            region.imageExtent.height = rowsPerChunk;
                        }
                        else
                        {
                            // Last chunk gets what's leftover.
                            region.imageExtent.height = transferHeight - rowsPerChunk*m;
                            PAL_ASSERT(region.imageExtent.height > 0);
                        }

                        region.gpuMemoryOffset =
                            pRegions[i].gpuMemoryOffset +
                            ((region.imageOffset.x - pRegions[i].imageOffset.x) * bytesPerPixel) +
                            ((region.imageOffset.y - pRegions[i].imageOffset.y) * region.gpuMemoryRowPitch);

                        chunkVa               = baseVa + region.gpuMemoryOffset;

                        LookupItem regionItem = {};
                        regionItem.region     = region;
                        regionItem.chunkAddr  = chunkVa;
                        result = lookupList.PushBack(regionItem);
                    }
                }
            }
            else
            {
                // Entering this path means one chunk can cover the whole region.
                const gpusize startVa = baseVa + region.gpuMemoryOffset;
                const gpusize endVa   = baseVa + region.gpuMemoryOffset +
                                        (region.imageExtent.width * bytesPerPixel) +
                                        (region.imageExtent.height * region.gpuMemoryRowPitch);
                if ((startVa < chunkVa) || (endVa >(chunkVa + maxChunkSize)))
                {
                    chunkVa = startVa;
                }

                LookupItem regionItem = {};
                regionItem.region     = pRegions[i];
                regionItem.chunkAddr  = chunkVa;
                result = lookupList.PushBack(regionItem);
            }
        }
    }

    uint32 const newRegionCount = static_cast<uint32>(lookupList.NumElements());

    if (result == Result::Success)
    {
        if (pNewRegions == nullptr)
        {
            // Query size required for new region list.
            if (needBiggerRegionList)
            {
                PAL_ASSERT(newRegionCount > regionCount);
            }
            else
            {
                PAL_ASSERT(newRegionCount == regionCount);
            }
            *pNewRegionCount = newRegionCount;
        }
        else
        {
            // Record new region list.
            PAL_ASSERT(newRegionCount == *pNewRegionCount);
            auto iter = lookupList.Begin();
            for (uint32 i = 0; i < newRegionCount; i++)
            {
                pNewRegions[i] = iter.Get()->region;
                pChunkAddrs[i] = iter.Get()->chunkAddr;
                iter.Next();
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Implements a portion of the Vega10 P2P BLT workaround by modifying a list of copy memory to imageregions so that it
// is composed of multiple, small chunks as required by the workaround.  For each modified region, a chunkAddr is
// reported that is the VA where the region begins in memory.
Result Device::P2pBltWaModifyRegionListMemoryToImage(
    const IGpuMemory&            srcGpuMemory,
    const Pal::Image&            dstImage,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions,
    uint32*                      pNewRegionCount,
    MemoryImageCopyRegion*       pNewRegions,
    gpusize*                     pChunkAddrs
    ) const
{
    PAL_NOT_TESTED();
    Result result = Result::Success;

    const gpusize maxChunkSize = Parent()->ChipProperties().p2pBltWaInfo.maxCopyChunkSize;
    const gpusize baseVa       = dstImage.GetBoundGpuMemory().GpuVirtAddr();

    // SplitChunk implementation
    struct LookupItem
    {
        MemoryImageCopyRegion region;
        gpusize               chunkAddr;
    };
    Deque<LookupItem, Platform> lookupList(GetPlatform());

    bool    needBiggerRegionList = false;
    gpusize chunkVa              = 0;

    for (uint32 i = 0; ((i < regionCount) && (result == Result::Success)); i++)
    {
        const uint32 transferWidth  = pRegions[i].imageExtent.width;
        const uint32 transferHeight = pRegions[i].imageExtent.height;
        const uint32 transferDepth  = pRegions[i].imageExtent.depth;
        PAL_ASSERT(transferDepth == 1); // For now we only support buffer, 1D, 2D, 2D slices.

        const SubResourceInfo* pDstSubResInfo = dstImage.SubresourceInfo(pRegions[i].imageSubres);
        const SwizzledFormat   dstViewFormat  = pDstSubResInfo->format;
        const uint32           bytesPerPixel  = Formats::BytesPerPixel(dstViewFormat.format);

        uint32 numChunks = 1; // Default needs one chunk for this region.

        // Go through each slice separately. If numSlices=N, the region will be split to at least N chunks.
        if (pRegions[i].numSlices > 1)
        {
            needBiggerRegionList = true;
        }
        for (uint32 j = 0; ((j < pRegions[i].numSlices) && (result == Result::Success)); j++)
        {
            const gpusize rowPitchInByte   = pDstSubResInfo->rowPitch;
            const gpusize depthPitchInByte = pDstSubResInfo->depthPitch;

            PAL_ASSERT(pDstSubResInfo->actualExtentElements.width*bytesPerPixel == rowPitchInByte);

            MemoryImageCopyRegion region = {};
            memcpy(&region, &pRegions[i], sizeof(MemoryImageCopyRegion));
            region.numSlices = 1;
            region.imageSubres.arraySlice = pRegions[i].imageSubres.arraySlice + j;

            const gpusize sliceBaseVa = baseVa + (depthPitchInByte * region.imageSubres.arraySlice);

            if (dstImage.IsSubResourceLinear(pRegions[i].imageSubres))
            {
                const gpusize regionPixelRowSize = transferWidth * bytesPerPixel;
                const gpusize vaSpanEntireRegion = rowPitchInByte * transferHeight;

                // Need to split to 1d (per-row)?
                if (maxChunkSize < vaSpanEntireRegion)
                {
                    needBiggerRegionList = true;

                    if (maxChunkSize < regionPixelRowSize)
                    {
                        // Each pixel row needs more than one chunk. (Won't happen because maxChunkSize should always
                        // larger than a pixel row.)
                        PAL_ASSERT_ALWAYS();
                        region.imageExtent.height = 1;

                        const uint32 chunksPerRow       = static_cast<uint32>(RoundUpQuotient(regionPixelRowSize,
                                                                                              maxChunkSize));
                        const uint32 chunkStrideInPixel = static_cast<uint32>(maxChunkSize / bytesPerPixel);

                        // Register each splitted chunk in lookupList for current region.
                        for (uint32 m = 0; ((m < transferHeight) && (result == Result::Success)); m++)
                        {
                            region.imageOffset.y = pRegions[i].imageOffset.y + m;
                            for (uint32 n = 0; ((n < chunksPerRow) && (result == Result::Success)); n++)
                            {
                                region.imageOffset.x = pRegions[i].imageOffset.x + (chunkStrideInPixel * n);

                                if (n != (chunksPerRow - 1))
                                {
                                    region.imageExtent.width = chunkStrideInPixel;
                                }
                                else
                                {
                                    // Last chunk gets what's leftover.
                                    region.imageExtent.width = transferWidth - (chunkStrideInPixel * n);
                                    PAL_ASSERT(region.imageExtent.width > 0);
                                }

                                region.gpuMemoryOffset =
                                    pRegions[i].gpuMemoryOffset +
                                    ((region.imageOffset.x - pRegions[i].imageOffset.x) * bytesPerPixel) +
                                    ((region.imageOffset.y - pRegions[i].imageOffset.y) * region.gpuMemoryRowPitch);

                                chunkVa               = sliceBaseVa +
                                                        (region.imageOffset.x * bytesPerPixel) +
                                                        (region.imageOffset.y * rowPitchInByte);

                                LookupItem regionItem = {};
                                regionItem.region     = region;
                                regionItem.chunkAddr  = chunkVa;
                                result = lookupList.PushBack(regionItem);
                            }
                        }
                    }
                    else
                    {
                        // Each chunk can hold at least one row.
                        const uint32 rowsPerChunk = static_cast<uint32>(maxChunkSize / rowPitchInByte);
                        numChunks = RoundUpQuotient(transferHeight, rowsPerChunk);

                        // Register each splitted chunk in lookupList for current region.
                        for (uint32 m = 0; ((m < numChunks) && (result == Result::Success)); m++)
                        {
                            region.imageOffset.y = pRegions[i].imageOffset.y + (rowsPerChunk * m);

                            if (m != (numChunks - 1))
                            {
                                region.imageExtent.height = rowsPerChunk;
                            }
                            else
                            {
                                // Last chunk gets what's leftover.
                                region.imageExtent.height = transferHeight - (rowsPerChunk * m);
                                PAL_ASSERT(region.imageExtent.height > 0);
                            }

                            region.gpuMemoryOffset =
                                pRegions[i].gpuMemoryOffset +
                                ((region.imageOffset.x - pRegions[i].imageOffset.x) * bytesPerPixel) +
                                ((region.imageOffset.y - pRegions[i].imageOffset.y) * region.gpuMemoryRowPitch);

                            // Use the beginning of pixel row to improve VCOP share rate.
                            chunkVa              = sliceBaseVa + (region.imageOffset.y * rowPitchInByte);

                            LookupItem regionItem = {};
                            regionItem.region     = region;
                            regionItem.chunkAddr  = chunkVa;
                            result = lookupList.PushBack(regionItem);
                        }
                    }
                }
                else
                {
                    // Entering this path means one chunk can cover the whole region.  If current region cannot fit in
                    // previous chunk, we need to update chunkVa associating with a new VCOP, otherwise keep using last
                    // chunkVa to avoid creating unnecessary VCOP.
                    const gpusize startVa = sliceBaseVa +
                                            (region.imageOffset.x * bytesPerPixel) +
                                            (region.imageOffset.y * rowPitchInByte);
                    const gpusize endVa   = sliceBaseVa +
                                            ((region.imageOffset.x + region.imageExtent.width) * bytesPerPixel) +
                                            ((region.imageOffset.y + region.imageExtent.height) * rowPitchInByte);

                    if ((startVa < chunkVa) || (endVa > (chunkVa + maxChunkSize)))
                    {
                        // Use the beginning of pixel row to improve VCOP share rate.
                        chunkVa = sliceBaseVa + (region.imageOffset.y * rowPitchInByte);
                    }

                    LookupItem regionItem = {};
                    regionItem.region     = pRegions[i];
                    regionItem.chunkAddr  = chunkVa;
                    result = lookupList.PushBack(regionItem);
                }
            }
            else
            {
                // The image is tiled.
                const Image* pGfxImage = static_cast<const Image*>(dstImage.GetGfxImage());
                const ADDR2_COMPUTE_SURFACE_INFO_OUTPUT* pSurfInfoOut = pGfxImage->GetAddrOutput(pDstSubResInfo);

                // Split tiled resource
                const uint32  blockWidth          = pSurfInfoOut->blockWidth;
                const uint32  blockHeight         = pSurfInfoOut->blockHeight;
                const gpusize blockSize           = blockWidth * blockHeight * bytesPerPixel;
                const uint32  mipChainPitch       = pSurfInfoOut->mipChainPitch; // unit of pixels
                const uint32  numBlocksPerRow     = mipChainPitch / blockWidth;
                const gpusize blockRowSizeInBytes = mipChainPitch * blockHeight * bytesPerPixel; // mip-chain row
                PAL_ASSERT((mipChainPitch % blockWidth) == 0);

                const uint32 extendRegionHeight =
                    RoundUpToMultiple(static_cast<uint32>(pRegions[i].imageOffset.y) + transferHeight, blockHeight) -
                    RoundDownToMultiple(static_cast<uint32>(pRegions[i].imageOffset.y), blockHeight);

                const uint32  numBlockRows       = extendRegionHeight / blockHeight;
                const gpusize vaSpanEntireRegion = blockRowSizeInBytes * numBlockRows;

                if (maxChunkSize < vaSpanEntireRegion)
                {
                    needBiggerRegionList = true;

                    if (maxChunkSize < blockRowSizeInBytes)
                    {
                        PAL_ASSERT_ALWAYS();
                        // Each row of tile blocks needs more than one chunk.
                        const uint32 numBlocksPerChunk = static_cast<uint32>(maxChunkSize / blockSize);
                        const uint32 numChunksPerRow   = RoundUpQuotient(numBlocksPerRow, numBlocksPerChunk);
                        const uint32 chunkWidth        = blockWidth * numBlocksPerChunk;

                        numChunks = numChunksPerRow * numBlockRows;
                        PAL_ASSERT(numChunks >= 1);

                        for (uint32 m = 0; ((m < numBlockRows) && (result == Result::Success)); m++)
                        {
                            struct Rect
                            {
                                uint32 xBegin;
                                uint32 xEnd;
                                uint32 yBegin;
                                uint32 yEnd;
                            };
                            Rect dst = {};

                            if (m == 0)
                            {
                                dst.yBegin = pRegions[i].imageOffset.y;
                            }
                            else
                            {
                                dst.yBegin = RoundDownToMultiple((pRegions[i].imageOffset.y + (blockHeight * m)),
                                                                 blockHeight);
                            }

                            if (m == numBlockRows - 1)
                            {
                                dst.yEnd = pRegions[i].imageOffset.y + transferHeight - 1;
                            }
                            else
                            {
                                dst.yEnd = RoundDownToMultiple((pRegions[i].imageOffset.y + (blockHeight * (m + 1))),
                                                               blockHeight) - 1;
                            }

                            region.imageOffset.y      = dst.yBegin;
                            region.imageExtent.height = dst.yEnd - dst.yBegin + 1;

                            for (uint32 n = 0; ((n < numChunksPerRow) && (result == Result::Success)); n++)
                            {
                                if (n == 0)
                                {
                                    dst.xBegin = pRegions[i].imageOffset.x;
                                }
                                else
                                {
                                    dst.xBegin = RoundDownToMultiple((pRegions[i].imageOffset.x + (chunkWidth * n)),
                                                                     chunkWidth);
                                }

                                if (n == numChunksPerRow - 1)
                                {
                                    dst.xEnd = pRegions[i].imageOffset.x + transferWidth - 1;
                                }
                                else
                                {
                                    dst.xEnd = RoundDownToMultiple((pRegions[i].imageOffset.x + (chunkWidth * (n + 1))),
                                                                   chunkWidth) - 1;
                                }
                                region.imageOffset.x     = dst.xBegin;
                                region.imageExtent.width = dst.xEnd - dst.xBegin + 1;

                                region.gpuMemoryOffset =
                                    pRegions[i].gpuMemoryOffset +
                                    ((region.imageOffset.x - pRegions[i].imageOffset.x) * bytesPerPixel) +
                                    ((region.imageOffset.y - pRegions[i].imageOffset.y) * region.gpuMemoryRowPitch);

                                const uint32 startBlockX = region.imageOffset.x / blockWidth;
                                const uint32 startBlockY = region.imageOffset.y / blockHeight;
                                chunkVa                  = sliceBaseVa +
                                                           ((startBlockX + startBlockY * numBlocksPerRow) * blockSize);

                                LookupItem regionItem = {};
                                regionItem.region     = region;
                                regionItem.chunkAddr  = chunkVa;
                                result = lookupList.PushBack(regionItem);
                            }
                        }
                    }
                    else
                    {
                        // Each chunk can hold at lease one effective row of tile blocks.  Trade of accuracy for
                        // simplicity, leads to small waste of chunk space.  Need to trim off blocks on the left of
                        // starting block, and right of ending block in its row.
                        const uint32 chunkHeightInBlocks = static_cast<uint32>(maxChunkSize / blockRowSizeInBytes);
                        const uint32 chunkHeight         = chunkHeightInBlocks * blockHeight;

                        numChunks = RoundUpQuotient(numBlockRows, chunkHeightInBlocks);
                        PAL_ASSERT(numChunks >= 1);

                        region.imageOffset.x = pRegions[i].imageOffset.x;
                        region.imageExtent.width = transferWidth;
                        PAL_ASSERT(transferWidth == pRegions[i].imageExtent.width);

                        for (uint32 m = 0; ((m < numChunks) && (result == Result::Success)); m++)
                        {
                            struct Line
                            {
                                uint32 yBegin;
                                uint32 yEnd;
                            };
                            Line dst = {};

                            if (m == 0)
                            {
                                dst.yBegin = pRegions[i].imageOffset.y;
                            }
                            else
                            {
                                dst.yBegin = RoundDownToMultiple((pRegions[i].imageOffset.y + (chunkHeight * m)),
                                                                 chunkHeight);
                            }

                            if (m == numChunks - 1)
                            {
                                dst.yEnd = pRegions[i].imageOffset.y + transferHeight - 1;
                            }
                            else
                            {
                                dst.yEnd = RoundDownToMultiple((pRegions[i].imageOffset.y + (chunkHeight * (m + 1))),
                                                               chunkHeight) - 1;
                            }
                            region.imageOffset.y      = dst.yBegin;
                            region.imageExtent.height = dst.yEnd - dst.yBegin + 1;

                            region.gpuMemoryOffset =
                                pRegions[i].gpuMemoryOffset +
                                ((region.imageOffset.x - pRegions[i].imageOffset.x) * bytesPerPixel) +
                                ((region.imageOffset.y - pRegions[i].imageOffset.y) * region.gpuMemoryRowPitch);

                            const uint32 startBlockX = 0;
                            const uint32 startBlockY = region.imageOffset.y / blockHeight;
                            chunkVa                  = sliceBaseVa +
                                                       ((startBlockX + startBlockY * numBlocksPerRow) * blockSize);

                            LookupItem regionItem = {};
                            regionItem.region     = region;
                            regionItem.chunkAddr  = chunkVa;
                            result = lookupList.PushBack(regionItem);
                        }
                    }
                }
                else
                {
                    // Entering this path means one chunk can cover the whole region.  If current region cannot fit in
                    // previous chunk, we need to update chunkVa associating with a new VCOP, otherwise keep using last
                    // chunkVa to avoid creating unnecessary VCOP.
                    const uint32 startBlockX = 0;
                    const uint32 startBlockY = region.imageOffset.y / blockHeight;
                    const uint32 endBlockY   = (region.imageOffset.y + region.imageExtent.height - 1) / blockHeight;
                    const gpusize startVa    = sliceBaseVa +
                                               ((startBlockX + (startBlockY * numBlocksPerRow)) * blockSize);
                    const gpusize endVa      = startVa + vaSpanEntireRegion;
                    PAL_ASSERT(vaSpanEntireRegion == ((endBlockY - startBlockY + 1) * numBlocksPerRow * blockSize));
                    if ((startVa < chunkVa) || (endVa >(chunkVa + maxChunkSize)))
                    {
                        chunkVa = startVa;
                    }

                    LookupItem regionItem = {};
                    regionItem.region     = pRegions[i];
                    regionItem.chunkAddr  = chunkVa;
                    result = lookupList.PushBack(regionItem);
                }
            }
        }
    }

    const uint32 newRegionCount = static_cast<uint32>(lookupList.NumElements());

    if (result == Result::Success)
    {
        if (pNewRegions == nullptr)
        {
            // Query size required for new region list.
            if (needBiggerRegionList)
            {
                PAL_ASSERT(newRegionCount > regionCount);
            }
            else
            {
                PAL_ASSERT(newRegionCount == regionCount);
            }
            *pNewRegionCount = newRegionCount;
        }
        else
        {
            // Record new region list.
            PAL_ASSERT(newRegionCount == *pNewRegionCount);
            auto iter = lookupList.Begin();
            for (uint32 i = 0; i < newRegionCount; i++)
            {
                pNewRegions[i] = iter.Get()->region;
                pChunkAddrs[i]  = iter.Get()->chunkAddr;
                iter.Next();
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Reports if the specified buffer (or GpuMemory range) should enable the CB, DB, and/or TCP BIG_PAGE feature.  This
// feature will reduce traffic between those blocks and their UTCL0s, but can only be enabled if the UMD can guarantee
// that the memory is backed by pages that are >= 64KiB (e.g., not 4KiB pages in system memory).  Further, there is a
// hardware bug on Navi10/Navi14 that requires there are no shared 64KiB regions that might be accessed without the
// BIG_PAGE bit set (i.e., the range must consume an integral number of 64KiB blocks).  PAL also supports
// enabling/disabling this feature with panel settings per use case, which must be passed in the bigPageUsageMask
// argument.
bool IsBufferBigPageCompatible(
    const GpuMemory& gpuMemory,
    gpusize          offset,
    gpusize          extent,
    uint32           bigPageUsageMask)  // Mask of Gfx10AllowBigPage values
{
    const Gfx9PalSettings& settings = GetGfx9Settings(*gpuMemory.GetDevice());
    bool bigPageCompatibility       = false;

    // Minimum allocation size required to support BigPage optimization supplied by KMD.
    gpusize bigPageAlignment = gpuMemory.GetDevice()->MemoryProperties().bigPageMinAlignment;

    // The hardware BIG_PAGE optimization always requires allocation >= bigPageMinAlignment.
    // Also if bigPageMinAlignment == 0, BigPage optimization is not supported
    if ((TestAllFlagsSet(settings.allowBigPage, bigPageUsageMask)) &&
        (bigPageAlignment > 0) && gpuMemory.IsLocalOnly() &&
        (gpuMemory.Desc().size >= bigPageAlignment))
    {
        gpusize bigPageLargeAlignment = gpuMemory.GetDevice()->MemoryProperties().bigPageLargeAlignment;

        // Increase alignment requirements to bigPageLargeAlignment if the buffer's allocation is larger and KMD
        // supports it.
        if ((bigPageLargeAlignment > 0) &&
            (gpuMemory.Desc().size >= bigPageLargeAlignment))
        {
            bigPageAlignment = bigPageLargeAlignment;
        }

        // KMD defined alignment requirements for BIG_PAGE optimization.
        bigPageCompatibility = IsPow2Aligned(gpuMemory.Desc().alignment, bigPageAlignment) &&
                               IsPow2Aligned(gpuMemory.Desc().size, bigPageAlignment) &&
                               IsPow2Aligned(gpuMemory.GetPhysicalAddressAlignment(), bigPageAlignment) &&
                               ((settings.waUtcL0InconsistentBigPage == false) ||
                                (IsPow2Aligned(offset, bigPageAlignment) && IsPow2Aligned(extent, bigPageAlignment)));
    }
    return bigPageCompatibility;
}

// =====================================================================================================================
// Reports if the specified image should enable the CB, DB, and/or TCP BIG_PAGE feature.  This feature will reduce
// traffic between those blocks and their UTCL0s.
bool IsImageBigPageCompatible(
    const Image& image,
    uint32       bigPageUsageMask)
{
    bool bigPage = false;

    const BoundGpuMemory& boundMem = image.Parent()->GetBoundGpuMemory();

    if (boundMem.IsBound()
       )
    {
        const GpuMemory&         gpuMemory = *boundMem.Memory();
        const ImageMemoryLayout& layout    = image.Parent()->GetMemoryLayout();

        bigPage = IsBufferBigPageCompatible(gpuMemory, boundMem.Offset(), layout.dataSize, bigPageUsageMask);
    }

    return bigPage;
}

// =====================================================================================================================
// Reports if the fmask owned by the specified image should enable the CB and/or TCP BIG_PAGE feature.  This feature
// will reduce traffic between those blocks and their UTCL0s.
bool IsFmaskBigPageCompatible(
    const Image& image,
    uint32       bigPageUsageMask)  // Mask of Gfx10AllowBigPage values
{
    bool bigPage = false;

    const BoundGpuMemory& boundMem = image.Parent()->GetBoundGpuMemory();

    if (boundMem.IsBound() && image.HasFmaskData())
    {
        const Gfx9Fmask& fmask = *image.GetFmask();

        bigPage = IsBufferBigPageCompatible(*boundMem.Memory(),
                                            boundMem.Offset() + fmask.MemoryOffset(),
                                            fmask.TotalSize(),
                                            bigPageUsageMask);
    }

    return bigPage;
}

// =====================================================================================================================
// Returns the number of shader-arrays based on the NUM_PKRS field in GB_ADDR_CONFIG
uint32 Device::Gfx103PlusExclusiveGetNumActiveShaderArraysLog2() const
{
    const auto&  gbAddrConfig  = GetGbAddrConfig();
    const uint32 numPkrLog2    = gbAddrConfig.gfx103PlusExclusive.NUM_PKRS;

    // Packers is a 10.3+ concept.
    PAL_ASSERT(IsGfx103PlusExclusive(*Parent()));

    // See Gfx10Lib::HwlInitGlobalParams (address library) for where this bit of non-intuitiveness comes from
    const uint32  numSaLog2FromPkr = ((numPkrLog2 > 0) ? (numPkrLog2 - 1) : 0);

    return numSaLog2FromPkr;
}

// =====================================================================================================================
// Getter for the VRS Depth Stencil View.  Creates the allocation on demand on first use.
const Gfx10DepthStencilView* Device::GetVrsDepthStencilView()
{
    // Alloc on demand to avoid creating this for apps which don't use VRS.
    if ((m_pVrsDepthView == nullptr) && m_vrsDepthViewMayBeNeeded)
    {
        // The caller is responsible to handle this failing.
        Result result = CreateVrsDepthView();
        PAL_ASSERT(result == Result::Success);
        PAL_ASSERT((result != Result::Success) ? (m_pVrsDepthView == nullptr) : true);
    }

    return m_pVrsDepthView;
}

// =====================================================================================================================
// Undoes CreateVrsDepthView.  The supplied image pointer is the VRS image belonging to this device; the view (if it
// was ever actually created) is implicitily destroyed as well.  It is the caller's responsibility to NULL out any
// remaining view pointer.
void Device::DestroyVrsDepthImage(
    Pal::Image*  pDsImage)
{
    if (pDsImage != nullptr)
    {
        auto*       pPalDevice  = Parent();
        auto*       pMemMgr     = pPalDevice->MemMgr();
        const auto& imageGpuMem = pDsImage->GetBoundGpuMemory();

        // Destroy the backing GPU memory associated with this image.
        if (imageGpuMem.IsBound())
        {
            pMemMgr->FreeGpuMem(imageGpuMem.Memory(), imageGpuMem.Offset());
        }

        // Unbind this memory from the image.
        pDsImage->BindGpuMemory(nullptr, 0);

        // Destroy the image
        pDsImage->Destroy();

        // And destroy the CPU allocation.
        PAL_SAFE_FREE(pDsImage, pPalDevice->GetPlatform());
    }
}

// =====================================================================================================================
// If the application has not bound a depth image and they bind a NULL source image via CmdBindSampleRateImage then
// we need a way to insert a 1x1 shading rate into VRS pipeline via an image.  Create a 1x1 depth buffer here that
// consists only of hTile data.
Result Device::CreateVrsDepthView()
{
    // Just re-using an already existing mutex. This call should only ever be hit once per device instance.
    const MutexAuto lock(&m_ringSizesLock);
    Result          result = Result::Success;

    // Double check in case multiple threads got past the caller's check to ensure we get one allocation.
    if (m_pVrsDepthView == nullptr)
    {
        Pal::Device*       pPalDevice = static_cast<Pal::Device*>(Parent());
        const auto&        settings   = GetGfx9Settings(*pPalDevice);
        IDepthStencilView* pVrsDsView = nullptr;

        PAL_ASSERT(IsGfx103Plus(*pPalDevice));

        // Create a stencil only image that can support VRS up to the size set in vrsImageSize. The worst-case size is
        // 16k by 16k (the largest possible target size) and we expect to use that size by default. In general, clients
        // don't know how big their render targets will be so we're more or less forced into the max size. 16k by 16k
        // seems huge, but the prior limit of 4k by 4k was too small, you can reach that threshold by enabling super
        // sampling on a 4K monitor.
        //
        // Note that the image doesn't actually contain any stencil data. We also do not need to initialize this
        // image's metadata in any way because the app's draws won't read or write stencil and the VRS copy shader
        // doesn't use meta equations.
        ImageCreateInfo  imageCreateInfo = {};

        imageCreateInfo.usageFlags.u32All        = 0;
        imageCreateInfo.usageFlags.vrsDepth      = 1;   // indicate hTile needs to support VRS
        imageCreateInfo.usageFlags.depthStencil  = 1;
        imageCreateInfo.imageType                = ImageType::Tex2d;
        imageCreateInfo.extent.width             = (settings.vrsImageSize & 0xFFFF);
        imageCreateInfo.extent.height            = (settings.vrsImageSize >> 16);
        imageCreateInfo.extent.depth             = 1;
        imageCreateInfo.swizzledFormat.format    = ChNumFormat::X8_Uint;
        imageCreateInfo.swizzledFormat.swizzle.r = ChannelSwizzle::X;
        imageCreateInfo.swizzledFormat.swizzle.g = ChannelSwizzle::Zero;
        imageCreateInfo.swizzledFormat.swizzle.b = ChannelSwizzle::Zero;
        imageCreateInfo.swizzledFormat.swizzle.a = ChannelSwizzle::Zero;
        imageCreateInfo.mipLevels                = 1;
        imageCreateInfo.arraySize                = 1;
        imageCreateInfo.samples                  = 1;
        imageCreateInfo.fragments                = 1;
        imageCreateInfo.tiling                   = ImageTiling::Optimal;

        const size_t imageSize  = pPalDevice->GetImageSize(imageCreateInfo, &result);
        size_t       dsViewSize = 0;

        if (result == Result::Success)
        {
            dsViewSize = pPalDevice->GetDepthStencilViewSize(&result);
        }

        if (result == Result::Success)
        {
            // Combine the allocation for the image and DS view
            void* pPlacementAddr = PAL_MALLOC_BASE(imageSize + dsViewSize,
                                                    Pow2Pad(imageSize),
                                                    pPalDevice->GetPlatform(),
                                                    SystemAllocType::AllocInternal,
                                                    MemBlkType::Malloc);

            if (pPlacementAddr != nullptr)
            {
                Pal::Image*              pVrsDepth          = nullptr;
                ImageInternalCreateInfo  internalCreateInfo = {};
                internalCreateInfo.flags.vrsOnlyDepth = (settings.privateDepthIsHtileOnly ? 1 : 0);

                result = pPalDevice->CreateInternalImage(imageCreateInfo,
                                                         internalCreateInfo,
                                                         pPlacementAddr,
                                                         &pVrsDepth);
                if (result != Result::Success)
                {
                    PAL_SAFE_FREE(pPlacementAddr, pPalDevice->GetPlatform());
                }
                else
                {
                    GpuMemoryRequirements  vrsDepthMemReqs = {};
                    pVrsDepth->GetGpuMemoryRequirements(&vrsDepthMemReqs);

                    // Allocate GPU backing memory for this image object.
                    GpuMemoryCreateInfo srcMemCreateInfo = { };
                    srcMemCreateInfo.alignment = vrsDepthMemReqs.alignment;
                    srcMemCreateInfo.size      = vrsDepthMemReqs.size;
                    srcMemCreateInfo.priority  = GpuMemPriority::Normal;

                    if (m_pParent->HeapLogicalSize(GpuHeapInvisible) > 0)
                    {
                        srcMemCreateInfo.heapCount = 3;
                        srcMemCreateInfo.heaps[0]  = GpuHeapInvisible;
                        srcMemCreateInfo.heaps[1]  = GpuHeapLocal;
                        srcMemCreateInfo.heaps[2]  = GpuHeapGartUswc;
                    }
                    else
                    {
                        srcMemCreateInfo.heapCount = 2;
                        srcMemCreateInfo.heaps[0]  = GpuHeapLocal;
                        srcMemCreateInfo.heaps[1]  = GpuHeapGartUswc;
                    }

                    GpuMemoryInternalCreateInfo internalInfo = { };
                    internalInfo.flags.alwaysResident = 1;

                    GpuMemory* pMemObj   = nullptr;
                    gpusize    memOffset = 0;

                    result = pPalDevice->MemMgr()->AllocateGpuMem(srcMemCreateInfo,
                                                                 internalInfo,
                                                                 false,    // data is written via RPM
                                                                 &pMemObj,
                                                                 &memOffset);

                    if (result == Result::Success)
                    {
                        result = pVrsDepth->BindGpuMemory(pMemObj, memOffset);
                    } // end check for GPU memory allocation
                } // end check for internal image creation

                // If we've succeeded in creating a hTile-only "depth" buffer, then create the view as well.
                if (result == Result::Success)
                {
                    DepthStencilViewCreateInfo  dsCreateInfo = {};
                    dsCreateInfo.flags.readOnlyDepth         = 1; // Our non-existent depth and stencil buffers will never
                    dsCreateInfo.flags.readOnlyStencil       = 1; //   be written...  or read for that matter.
                    dsCreateInfo.flags.imageVaLocked         = 1; // image memory is never going to move
                    dsCreateInfo.arraySize                   = imageCreateInfo.arraySize;
                    dsCreateInfo.pImage                      = pVrsDepth;

                    // Ok, we have our image, create a depth-stencil view for this image as well so we can bind our
                    // hTile memory at draw time.
                    result = pPalDevice->CreateDepthStencilView(dsCreateInfo,
                                                                VoidPtrInc(pPlacementAddr, imageSize),
                                                                &pVrsDsView);
                }

                if (result != Result::Success)
                {
                    // Ok, something went wrong and since m_pVrsDepthView was possibly never set, the "cleanup"
                    // function might not do anything with respect to cleaning up our image.  We still need to
                    // destroy whatever exists of our VRS image though to prevent memory leaks.
                    DestroyVrsDepthImage(pVrsDepth);

                    pVrsDepth       = nullptr;
                    m_pVrsDepthView = nullptr;
                }
                else
                {
                    // Assign member last as the allocation check is key'd off this.
                    m_pVrsDepthView = static_cast<Pal::Gfx9::Gfx10DepthStencilView*>(pVrsDsView);
                }
            }
            else
            {
                result = Result::ErrorOutOfMemory;
            }
        } // end check for getting the image size
    }

    return result;
}

#if  PAL_BUILD_GFX11
// =====================================================================================================================
// Calculate the value for the various INST_PREF_SIZE fields.  Default behavior is to prefetch the entire shader.
uint32 Device::GetShaderPrefetchSize(
    size_t  shaderSizeBytes
    ) const
{
    // Get the maximum number of bits the register has for setting the prefetch.  It's conceivable that a large shader
    // could overflow this.  All the shader stages have the same number of bits; the choice of PS here is arbitrary.
    PAL_ASSERT(IsGfx104Plus(*Parent()));
    constexpr uint32 MaxPrefetchSize = Gfx104Plus::SPI_SHADER_PGM_RSRC4_PS__INST_PREF_SIZE_MASK >>
                                       Gfx104Plus::SPI_SHADER_PGM_RSRC4_PS__INST_PREF_SIZE__SHIFT;

    constexpr  uint32 CachelineSizeBytes = 128;

    // Don't prefetch more bytes than the panel setting allows
    const uint32 prefetchSizeBytes       = Min(static_cast<uint32>(shaderSizeBytes),
                                               Settings().shaderPrefetchSizeBytes);

    // Align to the nearest multiple of a cachline
    const uint32 prefetchSizeAligned     = Pow2Align(prefetchSizeBytes, CachelineSizeBytes);

    // And return in terms of regsiter units.  Don't allow a value larger than what the register supports.
    return Min(MaxPrefetchSize, (prefetchSizeAligned / CachelineSizeBytes));
}
#endif

} // Gfx9
} // Pal
