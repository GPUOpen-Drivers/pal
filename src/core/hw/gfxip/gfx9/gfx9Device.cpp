/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/gfx9Image.h"
#include "core/hw/gfxip/gfx9/gfx9IndirectCmdGenerator.h"
#include "core/hw/gfxip/gfx9/gfx9MsaaState.h"
#include "core/hw/gfxip/gfx9/gfx9OcclusionQueryPool.h"
#include "core/hw/gfxip/gfx9/gfx9PerfCtrInfo.h"
#include "core/hw/gfxip/gfx9/gfx9PerfExperiment.h"
#include "core/hw/gfxip/gfx9/gfx9PipelineStatsQueryPool.h"
#include "core/hw/gfxip/gfx9/gfx9QueueContexts.h"
#include "core/hw/gfxip/gfx9/gfx9SettingsLoader.h"
#include "core/hw/gfxip/gfx9/gfx9ShadowedRegisters.h"
#include "core/hw/gfxip/gfx9/gfx9StreamoutStatsQueryPool.h"
#include "core/hw/gfxip/gfx9/gfx9UniversalCmdBuffer.h"
#include "core/hw/gfxip/gfx9/gfx9UniversalEngine.h"
#include "palAssert.h"
#include "palAutoBuffer.h"
#include "palDequeImpl.h"

#include "palFormatInfo.h"

#include "core/hw/amdgpu_asic.h"

using namespace Util;
using namespace Pal::Formats::Gfx9;

namespace Pal
{
namespace Gfx9
{
BufferSrd                nullBufferView = {};
ImageSrd                 nullImageView  = {};
static const SamplerSrd  NullSampler    = {};

// Microcode version for CE dump offset support
constexpr uint32 UcodeVersionWithDumpOffsetSupport = 30;

// Microcode version for SET_SH_REG_OFFSET with 256B alignment.
constexpr uint32 Gfx9UcodeVersionSetShRegOffset256B  = 42;

static PAL_INLINE uint32 ComputeImageViewDepth(
    const ImageViewInfo&   viewInfo,
    const ImageInfo&       imageInfo,
    const SubResourceInfo& subresInfo);

// =====================================================================================================================
size_t GetDeviceSize(
    GfxIpLevel  gfxLevel)
{
    size_t  rpmSize = sizeof(Gfx9RsrcProcMgr);

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

        default:
            PAL_ASSERT_ALWAYS();
            break;
        }

        pPfnTable->pfnCreateFmaskViewSrds = &Device::CreateFmaskViewSrds;
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
        const auto&  engineProps = pDevice->EngineProperties();

        if (engineProps.cpUcodeVersion >= 31)
        {
            static constexpr uint32 Mp1SmnFpsCnt = 0x162C4;

            frameCountRegister = Mp1SmnFpsCnt;
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
    m_gbAddrConfig(m_pParent->ChipProperties().gfx9.gbAddrConfig),
    m_gfxIpLevel(pDevice->ChipProperties().gfxLevel)
{
    PAL_ASSERT(((GetGbAddrConfig().bits.NUM_PIPES - GetGbAddrConfig().bits.NUM_RB_PER_SE) < 2) );

    for (uint32  shaderStage = 0; shaderStage < HwShaderStage::Last; shaderStage++)
    {
        m_firstUserDataReg[shaderStage] = GetBaseUserDataReg(static_cast<HwShaderStage>(shaderStage)) +
                                          FastUserDataStartReg;
    }
    memset(const_cast<uint32*>(&m_msaaHistogram[0]), 0, sizeof(m_msaaHistogram));
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
    }

    if (m_dummyZpassDoneMem.IsBound())
    {
        result = m_pParent->MemMgr()->FreeGpuMem(m_dummyZpassDoneMem.Memory(), m_dummyZpassDoneMem.Offset());
        m_dummyZpassDoneMem.Update(nullptr, 0);
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
    else
    {
        // No RPM, you're not going to get very far...
        PAL_ASSERT_ALWAYS();
    }

    Result result = m_ringSizesLock.Init();

    if (result == Result::Success)
    {
        result = m_pRsrcProcMgr->EarlyInit();
    }

    SetupWorkarounds();

    return result;
}

// =====================================================================================================================
// Sets up the hardware workaround/support flags based on the current ASIC
void Device::SetupWorkarounds()
{
    const auto& gfx9Props = m_pParent->ChipProperties().gfx9;
    // The LBPW feature uses a fixed late alloc VS limit based off of the available CUs.
    if (gfx9Props.lbpwEnabled
        )
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

    if (settings.nggEnableMode == NggPipelineTypeDisabled)
    {
        pChipProperties->gfx9.supportImplicitPrimitiveShader = 0;
    }

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

    pChipProperties->gfxip.tessFactorBufferSizePerSe = settings.tessFactorBufferSizePerSe;
}

// =====================================================================================================================
// Peforms extra initialization which needs to be done after the parent Device is finalized.
Result Device::Finalize()
{
    const auto& settings     = Settings();

    Result result = GfxDevice::Finalize();

    if (result == Result::Success)
    {
        result = m_pRsrcProcMgr->LateInit();
    }

    if (result == Result::Success)
    {
        result = InitOcclusionResetMem();
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
        // We need enough space for a full occlusion query slot because the DBs write to every other result location.
        // According to the packet spec it must be QWORD-aligned. We prefer the local heap to avoid impacting timestamp
        // performance and expect to get suballocated out of the same raft as the occlusion reset memory above.
        GpuMemoryCreateInfo zPassDoneCreateInfo = {};
        zPassDoneCreateInfo.alignment = sizeof(uint64);
        zPassDoneCreateInfo.size      = chipProps.gfx9.numTotalRbs * sizeof(OcclusionQueryResultPair);
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

#if DEBUG
// =====================================================================================================================
// Useful helper function for debugging command buffers on the GPU. This adds a WAIT_REG_MEM command to the specified
// command buffer space which waits until the device's dummy memory location contains the provided 'number' value. This
// lets engineers temporarily hang the GPU so they can inspect hardware state and command buffer contents in WinDbg, and
// then when they're finished, they can "un-hang" the GPU by modifying the memory location being waited on to contain
// the provided value.
uint32* Device::TemporarilyHangTheGpu(
    uint32  number,
    uint32* pCmdSpace
    ) const
{
    return (pCmdSpace + m_cmdUtil.BuildWaitRegMem(mem_space__me_wait_reg_mem__memory_space,
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
        // Assume (for now) that the UniversalEngine will work for the purposes of high-priority gfx engines as well
    case EngineTypeHighPriorityUniversal:
    case EngineTypeUniversal:
        pEngine = PAL_NEW(UniversalEngine, GetPlatform(), AllocInternal)(this, engineType, engineIndex);
        break;
    case EngineTypeCompute:
    case EngineTypeExclusiveCompute:
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
        {
            pCmdSpace += m_cmdUtil.BuildNop(CmdUtil::MinNopSizeInDwords, pCmdSpace);
        }

        pCmdStream->CommitCommands(pCmdSpace);
        pCmdStream->End();
    }
    else
    {
        PAL_SAFE_DELETE(pCmdStream, GetPlatform());
    }

    if (result == Result::Success)
    {
        (*ppCmdStream) = pCmdStream;
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
    default:
        break;
    }

    return size;
}

// =====================================================================================================================
// Creates the QueueContext object for the specified Queue in preallocated memory. Only supported on Universal and
// Compute Queues.
Result Device::CreateQueueContext(
    Queue*         pQueue,
    Engine*        pEngine,
    void*          pPlacementAddr,
    QueueContext** ppQueueContext)
{
    PAL_ASSERT((pPlacementAddr != nullptr) && (ppQueueContext != nullptr));

    Result result = Result::Success;

    const uint32 engineId = pQueue->EngineId();
    switch (pQueue->Type())
    {
    case QueueTypeCompute:
        {
            {
                ComputeQueueContext* pContext =
                    PAL_PLACEMENT_NEW(pPlacementAddr) ComputeQueueContext(this, pQueue, pEngine, engineId);

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
            UniversalQueueContext* pContext =
                PAL_PLACEMENT_NEW(pPlacementAddr) UniversalQueueContext(this, pQueue, pEngine, engineId);

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

    Result result = pPipeline->Init(createInfo);
    if (result != Result::Success)
    {
        pPipeline->Destroy();
        pPipeline = nullptr;
    }

    *ppPipeline = pPipeline;
    return result;
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
    auto* pPipeline = PAL_PLACEMENT_NEW(pPlacementAddr) GraphicsPipeline(this, isInternal);

    Result result = pPipeline->Init(createInfo, internalInfo);
    if (result != Result::Success)
    {
        pPipeline->Destroy();
    }
    else
    {
        *ppPipeline = pPipeline;
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
            hwStereoRenderingSupported |= IsVega12(*Parent());
            hwStereoRenderingSupported |= IsVega20(*Parent());
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
// Get the valid FormatFeatureFlags for the provided ChNumFormat, ImageAspect, and ImageTiling
uint32 Device::GetValidFormatFeatureFlags(
    const ChNumFormat format,
    const ImageAspect aspect,
    const ImageTiling tiling) const
{
    uint32 validFormatFeatureFlags = m_pParent->FeatureSupportFlags(format, tiling);
    constexpr uint32 InvalidDSFormatFeatureFlags       = FormatFeatureColorTargetWrite |
                                                         FormatFeatureColorTargetBlend |
                                                         FormatFeatureWindowedPresent;

    constexpr uint32 InvalidDepthFormatFeatureFlags    = InvalidDSFormatFeatureFlags   |
                                                         FormatFeatureStencilTarget;

    constexpr uint32 InvalidStencilFormatFeatureFlags  = InvalidDSFormatFeatureFlags   |
                                                         FormatFeatureDepthTarget;

    constexpr uint32 InvalidColorYUVFormatFeatureFlags = FormatFeatureStencilTarget    |
                                                         FormatFeatureDepthTarget;

    switch (aspect)
    {
    case ImageAspect::Depth:
        validFormatFeatureFlags = (tiling == ImageTiling::Optimal) ?
                                  (validFormatFeatureFlags & ~InvalidDepthFormatFeatureFlags) : 0;
        break;
    case ImageAspect::Stencil:
        validFormatFeatureFlags = (tiling == ImageTiling::Optimal) ?
                                  (validFormatFeatureFlags & ~InvalidStencilFormatFeatureFlags) : 0;
        break;
    case ImageAspect::Color:
    case ImageAspect::Y:
    case ImageAspect::CbCr:
    case ImageAspect::Cb:
    case ImageAspect::Cr:
    case ImageAspect::YCbCr:
        validFormatFeatureFlags = validFormatFeatureFlags & ~InvalidColorYUVFormatFeatureFlags;
        break;
    case ImageAspect::Fmask:
    default:
        PAL_NEVER_CALLED();
        break;
    }
    return validFormatFeatureFlags;
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
        *pResult = ColorBlendState::ValidateCreateInfo(this, createInfo);
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
    ColorBlendState* pColorBlendState = PAL_PLACEMENT_NEW(pPlacementAddr) ColorBlendState(*this, createInfo);

    PAL_ASSERT(pColorBlendState != nullptr);

    *ppColorBlendState = pColorBlendState;

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
    DepthStencilState* pDepthStencilState = PAL_PLACEMENT_NEW(pPlacementAddr) DepthStencilState(*this);

    Result result = pDepthStencilState->Init(createInfo);

    if (result != Result::Success)
    {
        pDepthStencilState->Destroy();
    }
    else
    {
        *ppDepthStencilState = pDepthStencilState;
    }

    return result;
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
    MsaaState* pMsaaState = PAL_PLACEMENT_NEW(pPlacementAddr) MsaaState(*this);

    Result result = pMsaaState->Init(createInfo);

    if (result != Result::Success)
    {
        pMsaaState->Destroy();
    }
    else
    {
        *ppMsaaState = pMsaaState;
    }

    return result;
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
        (*pResult) = Pal::IndirectCmdGenerator::ValidateCreateInfo(createInfo);
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
    PAL_ASSERT(Pal::IndirectCmdGenerator::ValidateCreateInfo(createInfo) == Result::Success);
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

    return viewSize;
}

// =====================================================================================================================
// Creates a Gfx9 implementation of Pal::IColorTargetView
Result Device::CreateColorTargetView(
    const ColorTargetViewCreateInfo&         createInfo,
    const ColorTargetViewInternalCreateInfo& internalInfo,
    void*                                    pPlacementAddr,
    IColorTargetView**                       ppColorTargetView
    ) const
{
    if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
    {
        (*ppColorTargetView) = PAL_PLACEMENT_NEW(pPlacementAddr) Gfx9ColorTargetView(this, createInfo, internalInfo);
    }

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
    if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
    {
        (*ppDepthStencilView) = PAL_PLACEMENT_NEW(pPlacementAddr) Gfx9DepthStencilView(this, createInfo, internalInfo);
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
// Fills in the AddrLib create input fields based on chip specific properties. Note: this function must not use any
// settings or member variables that depend on settings because AddrLib is initialized before settings are committed.
Result Device::InitAddrLibCreateInput(
    ADDR_CREATE_FLAGS*   pCreateFlags, // [out] Creation Flags
    ADDR_REGISTER_VALUE* pRegValue     // [out] Register Value
    ) const
{
    const GpuChipProperties& chipProps = m_pParent->ChipProperties();

    pRegValue->gbAddrConfig = chipProps.gfx9.gbAddrConfig;

    // addrlib asserts unless the varSize is >= 17 and <= 20.  Doesn't really matter what specific value we choose
    // (for now...) because the Image::ComputeAddrSwizzleMode() function disallows use of VAR swizzle modes anyway.
    pRegValue->blockVarSizeLog2 = 17;

    return Result::Success;
}

// =====================================================================================================================
// Helper function telling what kind of DCC format encoding an image created with
// the specified creation image and all of its potential view formats will end up with
DccFormatEncoding Device::ComputeDccFormatEncoding(
    const ImageCreateInfo& imageCreateInfo
    ) const
{
    DccFormatEncoding dccFormatEncoding = DccFormatEncoding::Optimal;

    if (imageCreateInfo.viewFormatCount == AllCompatibleFormats)
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
        const bool baseFormatIsUnsigned = Formats::IsUnorm(imageCreateInfo.swizzledFormat.format)   ||
                                          Formats::IsUint(imageCreateInfo.swizzledFormat.format)    ||
                                          Formats::IsUscaled(imageCreateInfo.swizzledFormat.format) ||
                                          Formats::IsSrgb(imageCreateInfo.swizzledFormat.format);
        const bool baseFormatIsSigned = Formats::IsSnorm(imageCreateInfo.swizzledFormat.format)   ||
                                        Formats::IsSint(imageCreateInfo.swizzledFormat.format)    ||
                                        Formats::IsSscaled(imageCreateInfo.swizzledFormat.format);

        const bool baseFormatIsFloat = Formats::IsFloat(imageCreateInfo.swizzledFormat.format);

        // If viewFormatCount is not zero then pViewFormats must point to a valid array.
        PAL_ASSERT((imageCreateInfo.viewFormatCount == 0) || (imageCreateInfo.pViewFormats != nullptr));

        const SwizzledFormat* pFormats = imageCreateInfo.pViewFormats;

        for (uint32 i = 0; i < imageCreateInfo.viewFormatCount; ++i)
        {
            // The pViewFormats array should not contain the base format of the image.
            PAL_ASSERT(memcmp(&imageCreateInfo.swizzledFormat, &pFormats[i], sizeof(SwizzledFormat)) != 0);

            const bool viewFormatIsUnsigned = Formats::IsUnorm(pFormats[i].format)   ||
                                              Formats::IsUint(pFormats[i].format)    ||
                                              Formats::IsUscaled(pFormats[i].format) ||
                                              Formats::IsSrgb(pFormats[i].format);
            const bool viewFormatIsSigned = Formats::IsSnorm(pFormats[i].format)   ||
                                            Formats::IsSint(pFormats[i].format)    ||
                                            Formats::IsSscaled(pFormats[i].format);

            const bool viewFormatIsFloat = Formats::IsFloat(pFormats[i].format);

            if (baseFormatIsFloat != viewFormatIsFloat)
            {
                dccFormatEncoding = DccFormatEncoding::Incompatible;
                break;
            }
            else if ((Formats::ShareChFmt(imageCreateInfo.swizzledFormat.format, pFormats[i].format) == false) ||
                     (baseFormatIsUnsigned != viewFormatIsUnsigned) ||
                     (baseFormatIsSigned != viewFormatIsSigned))
            {
                //dont have to turn off DCC entirely only Constant Encoding
                dccFormatEncoding = DccFormatEncoding::SignIndependent;
                break;
            }
        }
    }

    return dccFormatEncoding;
}

// =====================================================================================================================
// Computes the image view SRD DEPTH field based on image view parameters
static PAL_INLINE uint32 ComputeImageViewDepth(
    const ImageViewInfo&   viewInfo,
    const ImageInfo&       imageInfo,
    const SubResourceInfo& subresInfo)
{
    uint32 depth = 0;

    const ImageCreateInfo& imageCreateInfo = viewInfo.pImage->GetImageCreateInfo();

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
            const uint32 firstMip = viewInfo.subresRange.startSubres.mipLevel;
            depth = (((viewInfo.zRange.offset + viewInfo.zRange.extent) << firstMip) - 1);
        }
        else
        {
            depth = (subresInfo.extentTexels.depth - 1);
        }
    }
    else
    {

        // For gfx9, there is no longer a separate last_array parameter  for arrays. Instead the "depth" input is used
        // as the last_array parameter. For cubemaps, depth is no longer interpreted as the number of full cube maps
        // (6 faces), but strictly as the number of array slices. It is up to driver to make sure depth-base is
        // modulo 6 for cube maps.
        depth = (viewInfo.subresRange.startSubres.arraySlice + viewInfo.subresRange.numSlices - 1);
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
static PAL_INLINE SQ_TEX_CLAMP GetAddressClamp(
    TexAddressMode texAddress)
{
    constexpr SQ_TEX_CLAMP PalTexAddrToHwTbl[] =
    {
        SQ_TEX_WRAP,                    // TexAddressMode::Wrap
        SQ_TEX_MIRROR,                  // TexAddressMode::Mirror
        SQ_TEX_CLAMP_LAST_TEXEL,        // TexAddressMode::Clamp
        SQ_TEX_MIRROR_ONCE_LAST_TEXEL,  // TexAddressMode::MirrorOnce
        SQ_TEX_CLAMP_BORDER,            // TexAddressMode::ClampBorder
    };

    static_assert((ArrayLen(PalTexAddrToHwTbl) == static_cast<size_t>(TexAddressMode::Count)),
                  "Hardware table for Texture Address Mode does not match Pal::TexAddressMode enum.");

    return PalTexAddrToHwTbl[static_cast<uint32>(texAddress)];
}

// =====================================================================================================================
// Determine if anisotropic filtering is enabled
static PAL_INLINE bool IsAnisoEnabled(
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
static PAL_INLINE SQ_TEX_ANISO_RATIO GetAnisoRatio(
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

    auto*const pSrcSrd = static_cast<const BufferSrd*>(pSrcSrdTable);
    auto*const pDstSrd = static_cast<BufferSrd*>(pDstSrdTable);

    for (uint32 i = 0; i < (tableBytes / sizeof(BufferSrd)); ++i)
    {
        BufferSrd srd = pSrcSrd[i];

        const gpusize patchedGpuVa = (GetBaseAddress(&srd) + dataGpuVirtAddr);
        SetBaseAddress(&srd, patchedGpuVa);

        // Note: The entire unpatched SRD table has already been copied to GPU memory wholesale.  We just need to
        // modify the first quadword of the SRD to patch the addresses.
        memcpy((pDstSrd + i), &srd, sizeof(uint64));
    }
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
    const auto*const pFmtInfo   = MergedChannelFmtInfoTbl(pGfxDevice->Parent()->ChipProperties().gfxLevel);

    for (uint32 idx = 0; idx < count; ++idx)
    {
        const auto& view = pBufferViewInfo[idx];
        PAL_ASSERT(view.gpuAddr != 0);
        PAL_ASSERT((view.stride == 0) || ((view.gpuAddr % Min<gpusize>(sizeof(uint32), view.stride)) == 0));

        Gfx9BufferSrd srd = { };

        srd.word0.bits.BASE_ADDRESS    = LowPart(view.gpuAddr);
        srd.word1.bits.BASE_ADDRESS_HI = HighPart(view.gpuAddr);
        srd.word1.bits.STRIDE          = view.stride;
        srd.word2.bits.NUM_RECORDS     = pGfxDevice->CalcNumRecords(static_cast<size_t>(view.range),
                                                                    srd.word1.bits.STRIDE);
        srd.word3.bits.TYPE            = SQ_RSRC_BUF;

        PAL_ASSERT(Formats::IsUndefined(view.swizzledFormat.format) == false);
        PAL_ASSERT(Formats::BytesPerPixel(view.swizzledFormat.format) == view.stride);

        srd.word3.bits.DST_SEL_X   = Formats::Gfx9::HwSwizzle(view.swizzledFormat.swizzle.r);
        srd.word3.bits.DST_SEL_Y   = Formats::Gfx9::HwSwizzle(view.swizzledFormat.swizzle.g);
        srd.word3.bits.DST_SEL_Z   = Formats::Gfx9::HwSwizzle(view.swizzledFormat.swizzle.b);
        srd.word3.bits.DST_SEL_W   = Formats::Gfx9::HwSwizzle(view.swizzledFormat.swizzle.a);
        srd.word3.bits.DATA_FORMAT = Formats::Gfx9::HwBufDataFmt(pFmtInfo, view.swizzledFormat.format);
        srd.word3.bits.NUM_FORMAT  = Formats::Gfx9::HwBufNumFmt(pFmtInfo, view.swizzledFormat.format);

        // If we get an invalid format in the buffer SRD, then the memory operation involving this SRD will be dropped
        PAL_ASSERT(srd.word3.bits.DATA_FORMAT != BUF_DATA_FORMAT_INVALID);

        memcpy(pOut, &srd, sizeof(srd));
        pOut = VoidPtrInc(pOut, sizeof(srd));
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

    for (uint32 idx = 0; idx < count; ++idx, ++pBufferViewInfo)
    {
        PAL_ASSERT((pBufferViewInfo->gpuAddr != 0) ||
                   ((pBufferViewInfo->range == 0) && (pBufferViewInfo->stride == 0)));

        pOutSrd->word0.bits.BASE_ADDRESS = LowPart(pBufferViewInfo->gpuAddr);

        pOutSrd->word1.u32All =
            ((HighPart(pBufferViewInfo->gpuAddr) << Gfx09::SQ_BUF_RSRC_WORD1__BASE_ADDRESS_HI__SHIFT) |
             (static_cast<uint32>(pBufferViewInfo->stride) << Gfx09::SQ_BUF_RSRC_WORD1__STRIDE__SHIFT));

        pOutSrd->word2.bits.NUM_RECORDS = pGfxDevice->CalcNumRecords(static_cast<size_t>(pBufferViewInfo->range),
                                                                     static_cast<uint32>(pBufferViewInfo->stride));

        PAL_ASSERT(Formats::IsUndefined(pBufferViewInfo->swizzledFormat.format));

        if (pBufferViewInfo->gpuAddr != 0)
        {
            pOutSrd->word3.u32All  = ((SQ_RSRC_BUF << Gfx09::SQ_BUF_RSRC_WORD3__TYPE__SHIFT)   |
                                      (SQ_SEL_X << Gfx09::SQ_BUF_RSRC_WORD3__DST_SEL_X__SHIFT) |
                                      (SQ_SEL_Y << Gfx09::SQ_BUF_RSRC_WORD3__DST_SEL_Y__SHIFT) |
                                      (SQ_SEL_Z << Gfx09::SQ_BUF_RSRC_WORD3__DST_SEL_Z__SHIFT) |
                                      (SQ_SEL_W << Gfx09::SQ_BUF_RSRC_WORD3__DST_SEL_W__SHIFT) |
                                      (BUF_DATA_FORMAT_32 << Gfx09::SQ_BUF_RSRC_WORD3__DATA_FORMAT__SHIFT) |
                                      (BUF_NUM_FORMAT_UINT << Gfx09::SQ_BUF_RSRC_WORD3__NUM_FORMAT__SHIFT));
        }
        else
        {
            pOutSrd->word3.u32All = 0;
        }

        pOutSrd++;
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
    const ImageViewInfo&  imageViewInfo)
{
    const ChannelMapping&  swizzle   = imageViewInfo.swizzledFormat.swizzle;
    TEX_BC_SWIZZLE         bcSwizzle = TEX_BC_Swizzle_XYZW;

    if (swizzle.a == ChannelSwizzle::X)
    {
        // Have to use either TEX_BC_Swizzle_WZYX or TEX_BC_Swizzle_WXYZ
        //
        // For the pre-defined border color values (white, opaque black, transparent black), the only thing that
        // matters is that the alpha channel winds up in the correct place (because the RGB channels are all the same)
        // so either of these TEX_BC_Swizzle enumerations will work.  Not sure what happens with border color palettes.
        if (swizzle.b == ChannelSwizzle::Y)
        {
            // ABGR
            bcSwizzle = TEX_BC_Swizzle_WZYX;
        }
        else
        {
            // ARGB
            bcSwizzle = TEX_BC_Swizzle_WXYZ;
        }
    }
    else if (swizzle.r == ChannelSwizzle::X)
    {
        // Have to use either TEX_BC_Swizzle_XYZW or TEX_BC_Swizzle_XWYZ
        if (swizzle.g == ChannelSwizzle::Y)
        {
            // RGBA
            bcSwizzle = TEX_BC_Swizzle_XYZW;
        }
        else
        {
            // RAGB
            bcSwizzle = TEX_BC_Swizzle_XWYZ;
        }
    }
    else if (swizzle.g == ChannelSwizzle::X)
    {
        // GRAB, have to use TEX_BC_Swizzle_YXWZ
        bcSwizzle = TEX_BC_Swizzle_YXWZ;
    }
    else if (swizzle.b == ChannelSwizzle::X)
    {
        // BGRA, have to use TEX_BC_Swizzle_ZYXW
        bcSwizzle = TEX_BC_Swizzle_ZYXW;
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
// Checks if an image format override is needed.
bool Device::IsImageFormatOverrideNeeded(
    const ImageCreateInfo& imageCreateInfo,
    ChNumFormat*           pFormat,
    uint32*                pPixelsPerBlock
    ) const
{
    return (IsGfx9ImageFormatWorkaroundNeeded(imageCreateInfo, pFormat, pPixelsPerBlock));
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
    const auto*const pGfxDevice = static_cast<const Device*>(static_cast<const Pal::Device*>(pDevice)->GetGfxDevice());
    const auto&      chipProp   = pGfxDevice->Parent()->ChipProperties();
    const auto*const pFmtInfo   = MergedChannelFmtInfoTbl(chipProp.gfxLevel);

    ImageSrd* pSrds = static_cast<ImageSrd*>(pOut);

    for (uint32 i = 0; i < count; ++i)
    {
        const ImageViewInfo&   viewInfo        = pImgViewInfo[i];
        const Image&           image           = *GetGfx9Image(viewInfo.pImage);
        const auto*const       pParent         = static_cast<const Pal::Image*>(viewInfo.pImage);
        const ImageInfo&       imageInfo       = pParent->GetImageInfo();
        const ImageCreateInfo& imageCreateInfo = pParent->GetImageCreateInfo();
        const bool             imgIsBc         = Formats::IsBlockCompressed(imageCreateInfo.swizzledFormat.format);
        const bool             imgIsYuvPlanar  = Formats::IsYuvPlanar(imageCreateInfo.swizzledFormat.format);

        Gfx9ImageSrd srd    = {};
        ChNumFormat  format = viewInfo.swizzledFormat.format;

        SubresId     baseSubResId   = { viewInfo.subresRange.startSubres.aspect, 0, 0 };
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 478
        PAL_ASSERT((viewInfo.possibleLayouts.engines != 0) && (viewInfo.possibleLayouts.usages != 0 ));
#endif

        bool                        overrideBaseResource       = false;
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
        const SubResourceInfo*const pBaseSubResInfo = pParent->SubresourceInfo(baseSubResId);

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
                SubresId               mipSubResId    = { viewInfo.subresRange.startSubres.aspect, firstMipLevel, 0 };
                const SubResourceInfo* pMipSubResInfo = pParent->SubresourceInfo(mipSubResId);

                extent.width  = Util::Clamp((pMipSubResInfo->extentElements.width  << firstMipLevel),
                                            pBaseSubResInfo->extentElements.width,
                                            pBaseSubResInfo->actualExtentElements.width);
                extent.height = Util::Clamp((pMipSubResInfo->extentElements.height << firstMipLevel),
                                            pBaseSubResInfo->extentElements.height,
                                            pBaseSubResInfo->actualExtentElements.height);

                actualExtent = pBaseSubResInfo->actualExtentElements;
            }
            else if (pBaseSubResInfo->bitsPerTexel != Formats::BitsPerPixel(format))
            {
                extent       = pBaseSubResInfo->extentElements;
                actualExtent = pBaseSubResInfo->actualExtentElements;

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
        else if ((Formats::BytesPerPixel(format) == 1)      &&
                 pParent->IsAspectValid(ImageAspect::Depth) &&
                 image.HasDsMetadata())
        {
            // If they're requesting the stencil plane (i.e., an 8bpp view)       -and-
            // this surface also has Z data (i.e., is not a stencil-only surface) -and-
            // this surface has hTile data
            //
            // then we have to program the data-format of the stencil surface to match the bpp of the Z surface.
            // i.e., if we setup the stencil aspect with an 8bpp format, then the HW will address into hTile
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
#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION < 446)
        // We need to use D swizzle mode for writing an image with view3dAs2dArray feature enabled.
        // But when reading from it, we need to use S mode.
        // In AddrSwizzleMode, S mode is always right before D mode, so we simply do a "-1" here.
        if ((viewInfo.viewType == ImageViewType::Tex2d) && (imageCreateInfo.flags.view3dAs2dArray))
        {
            const AddrSwizzleMode view3dAs2dReadSwizzleMode = static_cast<AddrSwizzleMode>(surfSetting.swizzleMode - 1);
            PAL_ASSERT(AddrMgr2::IsStandardSwzzle(view3dAs2dReadSwizzleMode));

            srd.word3.bits.SW_MODE = AddrMgr2::GetHwSwizzleMode(view3dAs2dReadSwizzleMode);
        }
        else
#endif
        {
            srd.word3.bits.SW_MODE = AddrMgr2::GetHwSwizzleMode(surfSetting.swizzleMode);
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
        case ImageViewType::TexQuilt: // quilted textures must be 2D
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

        srd.word4.bits.DEPTH      = ComputeImageViewDepth(viewInfo, imageInfo, *pBaseSubResInfo);
        srd.word4.bits.BC_SWIZZLE = GetBcSwizzle(viewInfo);

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

        //   The array_pitch resource field is defined so that setting it to zero disables quilting and behavior
        //   reverts back to a texture array
        uint32  arrayPitch = 0;
        if (viewInfo.viewType == ImageViewType::TexQuilt)
        {
            PAL_ASSERT(isMultiSampled == false); // quilted images must be single sampled
            PAL_ASSERT(IsPowerOfTwo(viewInfo.quiltWidthInSlices));

            //    Encoded as trunc(log2(# horizontal  slices)) + 1
            arrayPitch = Log2(viewInfo.quiltWidthInSlices) + 1;
        }

        srd.word5.bits.BASE_ARRAY        = baseArraySlice;
        srd.word5.bits.ARRAY_PITCH       = arrayPitch;
        srd.word5.bits.META_PIPE_ALIGNED = Gfx9MaskRam::IsPipeAligned(&image);
        srd.word5.bits.META_RB_ALIGNED   = Gfx9MaskRam::IsRbAligned(&image);

        // Depth images obviously don't have an alpha component, so don't bother...
        if ((pParent->IsDepthStencil() == false) && pBaseSubResInfo->flags.supportMetaDataTexFetch)
        {
            // The setup of the compression-related fields requires knowing the bound memory and the expected
            // usage of the memory (read or write), so defer most of the setup to "WriteDescriptorSlot".

            // For single-channel FORMAT cases, ALPHA_IS_ON_MSB(AIOM) = 0 indicates the channel is color.
            // while ALPHA_IS_ON_MSB (AIOM) = 1 indicates the channel is alpha.

            // Theratically, ALPHA_IS_ON_MSB should be set to 1 for all single-channel formats only if
            // swap is SWAP_ALT_REV as gfx6 implementation; however, there is a new CB feature - to compress to AC01
            // during CB rendering/draw on gfx9.2, which requires special handling.

            const SurfaceSwap surfSwap = Formats::Gfx9::ColorCompSwap(viewInfo.swizzledFormat);

            if ((surfSwap != SWAP_STD_REV) && (surfSwap != SWAP_ALT_REV))
            {
                srd.word6.bits.ALPHA_IS_ON_MSB = 1;
            }
        }

        if (pParent->GetBoundGpuMemory().IsBound())
        {
            if (imgIsYuvPlanar && (viewInfo.subresRange.numSlices == 1))
            {
                gpusize gpuVirtAddress         = pParent->GetSubresourceBaseAddr(baseSubResId);
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
                    srd.word0.bits.BASE_ADDRESS  = image.GetSubresource256BAddrSwizzled(baseSubResId);
                }
                // Usually, we'll never have an image address that extends into 40 bits.
                // However, when svm is enabled, The bit 39 of an image address is 1 if the address is gpuvm.
                srd.word1.bits.BASE_ADDRESS_HI = image.GetSubresource256BAddrSwizzledHi(baseSubResId);
            }

            if (pBaseSubResInfo->flags.supportMetaDataTexFetch)
            {
                if (image.Parent()->IsDepthStencil())
                {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 478
                    if (TestAnyFlagSet(viewInfo.possibleLayouts.usages, LayoutShaderWrite | LayoutCopyDst) == false)
#else
                    if (viewInfo.flags.shaderWritable == false)
#endif
                    {
                        srd.word6.bits.COMPRESSION_EN = 1;
                        srd.word7.bits.META_DATA_ADDRESS = image.GetHtile256BAddr();
                    }
                }
                else
                {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 478
                    if (TestAnyFlagSet(viewInfo.possibleLayouts.usages, LayoutShaderWrite | LayoutCopyDst) == false)
#else
                    if (viewInfo.flags.shaderWritable == false)
#endif
                    {
                        srd.word6.bits.COMPRESSION_EN = 1;
                        // The color image's meta-data always points at the DCC surface.  Any existing cMask or fMask
                        // meta-data is only required for compressed texture fetches of MSAA surfaces, and that feature
                        // requires enabling an extension and use of an fMask image view.
                        srd.word7.bits.META_DATA_ADDRESS = image.GetDcc256BAddr();
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
// Gfx9+ specific function for creating fmask view SRDs. Installed in the function pointer table of the parent device
// during initialization.
void PAL_STDCALL Device::CreateFmaskViewSrds(
    const IDevice*        pDevice,
    uint32                count,
    const FmaskViewInfo*  pFmaskViewInfo,
    void*                 pOut)
{
    PAL_ASSERT((pDevice != nullptr) && (pOut != nullptr) && (pFmaskViewInfo != nullptr) && (count > 0));
    const Device* pGfxDevice = static_cast<const Device*>(static_cast<const Pal::Device*>(pDevice)->GetGfxDevice());

    pGfxDevice->CreateFmaskViewSrdsInternal(count, pFmaskViewInfo, nullptr, pOut);
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
    const SubresId         slice0Id        = { ImageAspect::Fmask, 0, 0 };
    const Image&           image           = *GetGfx9Image(viewInfo.pImage);
    const Gfx9Fmask*const  pFmask          = image.GetFmask();
    const auto*const       pParent         = static_cast<const Pal::Image*>(viewInfo.pImage);
    const ImageCreateInfo& createInfo      = pParent->GetImageCreateInfo();
    const bool             isUav           = (hasInternalInfo && (pFmaskViewInternalInfo->flags.fmaskAsUav == 1));
    const SubResourceInfo& subresInfo      = *pParent->SubresourceInfo(slice0Id);
    const auto*            pAddrOutput     = image.GetAddrOutput(&subresInfo);
    const Gfx9Fmask&       fmask           = *image.GetFmask();
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
    pSrd->word3.bits.SW_MODE      = AddrMgr2::GetHwSwizzleMode(pFmask->GetSwizzleMode());

    // On GFX9, "depth" replaces the deprecated "last_array" from pre-GFX9 ASICs.
    pSrd->word4.bits.DEPTH = (viewInfo.baseArraySlice + viewInfo.arraySize - 1);
    pSrd->word4.bits.PITCH = fMaskAddrOut.pitch - 1;

    pSrd->word5.bits.BASE_ARRAY        = viewInfo.baseArraySlice;
    pSrd->word5.bits.ARRAY_PITCH       = 0; // msaa surfaces don't support texture quilting
    pSrd->word5.bits.META_LINEAR       = 0; // linear meta-surfaces aren't supported in gfx9
    pSrd->word5.bits.META_PIPE_ALIGNED = Gfx9MaskRam::IsPipeAligned(&image);
    pSrd->word5.bits.META_RB_ALIGNED   = Gfx9MaskRam::IsRbAligned(&image);
    pSrd->word5.bits.MAX_MIP           = 0;

    if (image.Parent()->GetBoundGpuMemory().IsBound())
    {
        // Need to grab the most up-to-date GPU virtual address for the underlying FMask object.
        pSrd->word0.bits.BASE_ADDRESS    = image.GetFmask256BAddr();
        pSrd->word1.bits.BASE_ADDRESS_HI = 0; // base_addr is bits 8-39, we'll never have a bit 40

        // Does this image has an associated FMask which is shader Readable? if FMask needs to be
        // read in the shader CMask has to be read as FMask meta data
        if (image.IsComprFmaskShaderReadable(slice0Id))
        {
            pSrd->word6.bits.COMPRESSION_EN = (viewInfo.flags.shaderWritable == 0);

            if (viewInfo.flags.shaderWritable == 0)
            {
                // word7 contains bits 8-39 of the meta-data surface.  For fMask,the meta-surface is cMask.
                // We'll never have bits 40-47 set as we limit th possible VA addresses.
                pSrd->word7.bits.META_DATA_ADDRESS = image.GetCmask256BAddr();
                pSrd->word5.bits.META_DATA_ADDRESS = 0;
            }
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
            pSrd->word2.bits.LOD_BIAS = Math::FloatToSFixed(pInfo->mipLodBias,
                                                            Gfx9SamplerLodBiasIntBits,
                                                            Gfx9SamplerLodBiasFracBits);

            pSrd->word2.bits.BLEND_ZERO_PRT     = pInfo->flags.prtBlendZeroMode;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 444
            pSrd->word2.bits.MIP_POINT_PRECLAMP = (pInfo->flags.dx9Mipclamping == 1) ? 0 : 1;
#else
            pSrd->word2.bits.MIP_POINT_PRECLAMP = 0;
#endif
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 448
            pSrd->word2.bits.ANISO_OVERRIDE = !pInfo->flags.disableSingleMipAnisoOverride;
#else
            pSrd->word2.bits.ANISO_OVERRIDE = 1;
#endif
        }

        memcpy(pSrdOutput, &tempSamplerSrds[0], (currentSrdIdx * sizeof(SamplerSrd)));
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
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return level;
}

// =====================================================================================================================
// Gets the static format support info table for GFXIP 9 hardware.
const MergedFormatPropertiesTable* GetFormatPropertiesTable(
    GfxIpLevel gfxIpLevel)
{
    const MergedFormatPropertiesTable* pTable = nullptr;

    switch (gfxIpLevel)
    {
    case GfxIpLevel::GfxIp9:
        pTable = &Gfx9MergedFormatPropertiesTable;
        break;

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

    // GFX9 ASICs support texture quilting on single-sample surfaces.
    pInfo->imageProperties.flags.supportsSingleSampleQuilting = 1;

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

    // TODO:  Should find a way to get this info from the ADAPTERINFOEX structure.  Steal these values from
    //        the GFX6 implementation for the time being.
    pInfo->gfx9.numSimdPerCu = 4;

    // The maximum amount of LDS space that can be shared by a group of threads (wave/ threadgroup) in bytes.
    pInfo->gfxip.ldsSizePerThreadGroup = 64 * 1024;
    pInfo->gfxip.ldsSizePerCu          = 65536;
    pInfo->gfxip.ldsGranularity        = Gfx9LdsDwGranularity * sizeof(uint32);
    pInfo->gfxip.tccSizeInBytes        = 4096 * 1024;
    pInfo->gfxip.tcpSizeInBytes        = 16384;
    pInfo->gfxip.maxLateAllocVsLimit   = 64;

    pInfo->gfxip.supportGl2Uncached      = 1;
    pInfo->gfxip.gl2UncachedCpuCoherency = (CoherCpu | CoherShader | CoherIndirectArgs | CoherIndexData |
                                            CoherQueueAtomic | CoherTimestamp | CoherCeLoad | CoherCeDump |
                                            CoherStreamOut | CoherMemory);

    pInfo->gfxip.maxUserDataEntries = MaxUserDataEntries;
    memcpy(&pInfo->gfxip.fastUserDataEntries[0], &FastUserDataEntriesByStage[0], sizeof(FastUserDataEntriesByStage));

    {
        pInfo->imageProperties.prtFeatures = Gfx9PrtFeatures;
        pInfo->imageProperties.prtTileSize = PrtTileSize;
    }

    pInfo->gfx9.supports2BitSignedValues           = 1;
    pInfo->gfx9.supportConservativeRasterization   = 1;
    pInfo->gfx9.supportPrtBlendZeroMode            = 1;
    pInfo->gfx9.supportPrimitiveOrderedPs          = 1;
    pInfo->gfx9.supportImplicitPrimitiveShader     = 1;
    pInfo->gfx9.supportFp16Fetch                   = 1;
    pInfo->gfx9.support16BitInstructions           = 1;
    pInfo->gfx9.supportDoubleRate16BitInstructions = 1;

    if (
        (cpUcodeVersion  >= UcodeVersionWithDumpOffsetSupport))
    {
        pInfo->gfx9.supportAddrOffsetDumpAndSetShPkt = 1;
    }

    {
        pInfo->gfx9.supportAddrOffsetDumpAndSetShPkt = (cpUcodeVersion >= UcodeVersionWithDumpOffsetSupport);
        pInfo->gfx9.supportAddrOffsetSetSh256Pkt     = (cpUcodeVersion >= Gfx9UcodeVersionSetShRegOffset256B);

        pInfo->gfx9.numShaderArrays         = 1;
        pInfo->gfx9.numSimdPerCu            = Gfx9NumSimdPerCu;
        pInfo->gfx9.numWavesPerSimd         = Gfx9NumWavesPerSimd;
        pInfo->gfx9.nativeWavefrontSize     = 64;
        pInfo->gfx9.minWavefrontSize        = 64;
        pInfo->gfx9.maxWavefrontSize        = 64;
        pInfo->gfx9.numShaderVisibleSgprs   = MaxSgprsAvailable;
        pInfo->gfx9.numPhysicalSgprs        = Gfx9PhysicalSgprsPerSimd;
        pInfo->gfx9.sgprAllocGranularity    = 16;
        pInfo->gfx9.minSgprAlloc            = 16;
        pInfo->gfx9.numPhysicalVgprs        = 256;
        pInfo->gfx9.vgprAllocGranularity    = 4;
        pInfo->gfx9.minVgprAlloc            = 4;
        pInfo->gfxip.shaderPrefetchBytes    = 2 * ShaderICacheLineSize;
    }

    pInfo->gfx9.gsVgtTableDepth         = 32;
    pInfo->gfx9.gsPrimBufferDepth       = 1792;
    pInfo->gfx9.doubleOffchipLdsBuffers = 1;

    pInfo->gfxip.vaRangeNumBits   = 48;
    pInfo->gfxip.gdsSize          = 65536;
    pInfo->gfxip.hardwareContexts = 8;

    // Gfx9 HW supports all tessellation distribution modes.
    pInfo->gfx9.supportPatchTessDistribution     = 1;
    pInfo->gfx9.supportDonutTessDistribution     = 1;
    pInfo->gfx9.supportTrapezoidTessDistribution = 1;

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
        pInfo->gfx9.supportReleaseAcquireInterface = 1;
        pInfo->gfx9.supportSplitReleaseAcquire     = 0;

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
        pInfo->gfx9.supportReleaseAcquireInterface = 1;
        pInfo->gfx9.supportSplitReleaseAcquire     = 0;

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
            pInfo->gfx9.timestampResetOnIdle = 1;
            pInfo->gfx9.numSdpInterfaces     = 32;
            pInfo->gfx9.eccProtectedGprs     = 1;
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    pInfo->srdSizes.bufferView = sizeof(BufferSrd);
    pInfo->srdSizes.imageView  = sizeof(ImageSrd);
    pInfo->srdSizes.fmaskView  = sizeof(ImageSrd);
    pInfo->srdSizes.sampler    = sizeof(SamplerSrd);

    // Setup anything specific to a given GFXIP level here
    if (pInfo->gfxLevel == GfxIpLevel::GfxIp9)
    {
        nullBufferView.gfx9.word3.bits.TYPE = SQ_RSRC_BUF;
        nullImageView.gfx9.word3.bits.TYPE  = SQ_RSRC_IMG_2D_ARRAY;

        pInfo->imageProperties.maxImageArraySize = Gfx9MaxImageArraySlices;

        pInfo->gfx9.supportOutOfOrderPrimitives = 1;
    }

    pInfo->nullSrds.pNullBufferView = &nullBufferView;
    pInfo->nullSrds.pNullImageView  = &nullImageView;
    pInfo->nullSrds.pNullFmaskView  = &nullImageView;
    pInfo->nullSrds.pNullSampler    = &NullSampler;

    if (pInfo->gfx9.supportReleaseAcquireInterface == 1)
    {
        pInfo->gfxip.numSlotsPerEvent = MaxSlotsPerEvent;
    }
    else
    {
        pInfo->gfxip.numSlotsPerEvent = 1;
    }
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
    for (uint32 sa = 0; sa < pInfo->gfx9.numShaderArrays; ++sa)
    {
        for (uint32 se = 0; se < pInfo->gfx9.numShaderEngines; ++se)
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
        }
    }
    PAL_ASSERT((pInfo->gfx9.numCuPerSh > 0) && (pInfo->gfx9.numCuPerSh <= pInfo->gfx9.maxNumCuPerSh));
    pInfo->gfx9.numActiveCus   = numActiveCus;
    pInfo->gfx9.numAlwaysOnCus = numAlwaysOnCus;
    PAL_ASSERT((pInfo->gfx9.numActiveCus > 0) && (pInfo->gfx9.numActiveCus <= pInfo->gfx9.numPhysicalCus));
    PAL_ASSERT((pInfo->gfx9.numAlwaysOnCus > 0) && (pInfo->gfx9.numAlwaysOnCus <= pInfo->gfx9.numPhysicalCus));

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
    pProperties->shaderEngineCount     = chipProps.gfx9.numShaderEngines;

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
    GfxIpLevel           gfxIpLevel,
    uint32               familyId,
    uint32               eRevId,
    GpuEngineProperties* pInfo)
{
    auto*const  pUniversal = &pInfo->perEngine[EngineTypeUniversal];

    // We support If/Else/While on the universal and compute queues; the command stream controls the max nesting depth.
    pUniversal->flags.timestampSupport                = 1;
    pUniversal->flags.borderColorPaletteSupport       = 1;
    pUniversal->flags.queryPredicationSupport         = 1;
    pUniversal->flags.memoryPredicationSupport        = 1;
    pUniversal->flags.conditionalExecutionSupport     = 1;
    pUniversal->flags.loopExecutionSupport            = 1;
    pUniversal->flags.constantEngineSupport           = 1;
    pUniversal->flags.regMemAccessSupport             = 1;
    pUniversal->flags.indirectBufferSupport           = 1;
    pUniversal->flags.supportsMismatchedTileTokenCopy = 1;
    pUniversal->flags.supportsImageInitBarrier        = 1;
    pUniversal->flags.supportsImageInitPerSubresource = 1;
    pUniversal->flags.supportsUnmappedPrtPageAccess   = 1;
    pUniversal->maxControlFlowNestingDepth            = CmdStream::CntlFlowNestingLimit;
    pUniversal->reservedCeRamSize                     = ReservedCeRamBytes;
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

    pCompute->flags.timestampSupport                = 1;
    pCompute->flags.borderColorPaletteSupport       = 1;
    pCompute->flags.queryPredicationSupport         = 1;
    pCompute->flags.memoryPredicationSupport        = 1;
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

    // Note that we set this DMA state in the GFXIP layer because it deals with GFXIP features that the OSSIP layer
    // doesn't need to understand. Gfx9 can't support per-subresource initialization on DMA because the metadata
    // is interleaved.
    pInfo->perEngine[EngineTypeDma].flags.supportsImageInitBarrier        = 1;
    pInfo->perEngine[EngineTypeDma].flags.supportsMismatchedTileTokenCopy = 1;
    pInfo->perEngine[EngineTypeDma].flags.supportsUnmappedPrtPageAccess   = 1;

    // TODO: Get these from KMD once the information is reported by it

    // NOTE: NGG operates on the last few DWORDs of GDS thus the last 16 DWORDs are reserved.
    pUniversal->availableGdsSize = 0xFC0;
    pUniversal->gdsSizePerEngine = 0xFC0;

    pCompute->availableGdsSize   = 0xFC0;
    pCompute->gdsSizePerEngine   = 0xFC0;

    // Copy the compute properties into the exclusive compute engine properties
    auto*const pExclusiveCompute = &pInfo->perEngine[EngineTypeExclusiveCompute];
    memcpy(pExclusiveCompute, pCompute, sizeof(pInfo->perEngine[EngineTypeExclusiveCompute]));
}

// =====================================================================================================================
// Returns the value for the DB_DFSM_CONTROL register
uint32  Device::GetDbDfsmControl() const
{
    const Gfx9PalSettings& gfx9Settings  = GetGfx9Settings(*m_pParent);
    regDB_DFSM_CONTROL     dbDfsmControl = {};

    const bool disableDfsm = gfx9Settings.disableDfsm;

    // Force off DFSM if requested by the settings
    dbDfsmControl.bits.PUNCHOUT_MODE = (disableDfsm ? DfsmPunchoutModeDisable : DfsmPunchoutModeEnable);

    // Setup POPS as requested by the settings as well.
    dbDfsmControl.bits.POPS_DRAIN_PS_ON_OVERLAP = gfx9Settings.drainPsOnOverlap;

    return dbDfsmControl.u32All;
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
        baseUserDataReg = mmSPI_SHADER_USER_DATA_VS_0;
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
        const MergedFmtInfo*const pFmtInfo = MergedChannelFmtInfoTbl(gfxLevel);
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
        const MergedFmtInfo*const pFmtInfo = MergedChannelFmtInfoTbl(gfxLevel);
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
        const MergedFmtInfo*const pFmtInfo = MergedChannelFmtInfoTbl(gfxLevel);

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
            if (IsRaven2(*Parent()))
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
            if (IsRaven2(*Parent()))
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
#endif // PAL_ENABLE_PRINTS_ASSERTS

        default:
            // What is this?
            PAL_ASSERT_ALWAYS();
            break;
        }
    }

    PAL_ASSERT(pRange != nullptr);

    return pRange;
}

// =====================================================================================================================
// Computes the CONTEXT_CONTROL value that should be used for universal engine submissions.  This will vary based on
// whether preemption is enabled or not.  This exists as a helper function since there are cases where the command
// buffer may want to temporarily override the default value written by the queue context, and it needs to be able
// to restore it to the proper original value.
PM4PFP_CONTEXT_CONTROL Device::GetContextControl() const
{
    PM4PFP_CONTEXT_CONTROL contextControl = { };

    // Since PAL doesn't preserve GPU state across command buffer boundaries, we don't need to enable state shadowing
    // unless mid command buffer preemption is enabled, but we always need to enable loading context and SH registers.
    contextControl.bitfields2.update_load_enables    = 1;
    contextControl.bitfields2.load_per_context_state = 1;
    contextControl.bitfields2.load_cs_sh_regs        = 1;
    contextControl.bitfields2.load_gfx_sh_regs       = 1;
    contextControl.bitfields3.update_shadow_enables  = 1;

    if (ForceStateShadowing || Parent()->IsPreemptionSupported(EngineType::EngineTypeUniversal))
    {
        // If mid command buffer preemption is enabled, shadowing and loading must be enabled for all register types,
        // because the GPU state needs to be properly restored when this Queue resumes execution after being preempted.
        // (Config registers are exempted because we don't write config registers in PAL.)
        contextControl.bitfields2.load_global_uconfig      = 1;
        contextControl.bitfields2.load_ce_ram              = 1;
        contextControl.bitfields3.shadow_per_context_state = 1;
        contextControl.bitfields3.shadow_cs_sh_regs        = 1;
        contextControl.bitfields3.shadow_gfx_sh_regs       = 1;
        contextControl.bitfields3.shadow_global_config     = 1;
        contextControl.bitfields3.shadow_global_uconfig    = 1;
    }

    return contextControl;
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
// Returns the TcCacheOp that can satisfy the most cacheFlags without over-syncing. Note that the flags for the
// selected cache op are set to zero.
TcCacheOp Device::SelectTcCacheOp(
    uint32* pCacheFlags // [in/out]
    ) const
{
    TcCacheOp cacheOp = TcCacheOp::Nop;

    if (TestAllFlagsSet(*pCacheFlags, CacheSyncInvTcp | CacheSyncInvTcc | CacheSyncFlushTcc))
    {
        *pCacheFlags &= ~(CacheSyncInvTcp | CacheSyncInvTcc | CacheSyncFlushTcc | CacheSyncInvTccMd);
        cacheOp      = TcCacheOp::WbInvL1L2;
    }
    else if (TestAllFlagsSet(*pCacheFlags , CacheSyncInvTcc | CacheSyncFlushTcc))
    {
        *pCacheFlags &= ~(CacheSyncInvTcc | CacheSyncFlushTcc | CacheSyncInvTccMd);
        cacheOp      = TcCacheOp::WbInvL2Nc;
    }
    else if (TestAnyFlagSet(*pCacheFlags , CacheSyncFlushTcc))
    {
        *pCacheFlags &= ~CacheSyncFlushTcc;
        cacheOp      = TcCacheOp::WbL2Nc;
    }
    else if (TestAnyFlagSet(*pCacheFlags , CacheSyncInvTcc))
    {
        *pCacheFlags &= ~(CacheSyncInvTcc | CacheSyncInvTccMd);
        cacheOp      = TcCacheOp::InvL2Nc;
    }
    else if (TestAnyFlagSet(*pCacheFlags , CacheSyncInvTcp))
    {
        *pCacheFlags &= ~CacheSyncInvTcp;
        cacheOp      = TcCacheOp::InvL1;
    }
    else if (TestAnyFlagSet(*pCacheFlags , CacheSyncInvTccMd))
    {
        *pCacheFlags &= ~CacheSyncInvTccMd;
        cacheOp      = TcCacheOp::InvL2Md;
    }

    return cacheOp;
}

} // Gfx9
} // Pal
