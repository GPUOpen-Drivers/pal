/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palFile.h"
#include "core/device.h"
#include "core/g_palSettings.h"
#include "core/platform.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/rpm/rsrcProcMgr.h"
#include "palHashMapImpl.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
GfxDevice::GfxDevice(
    Device*           pDevice,
    Pal::RsrcProcMgr* pRsrcProcMgr,
    uint32            frameCountRegOffset)
    :
    m_pParent(pDevice),
    m_pRsrcProcMgr(pRsrcProcMgr),
    m_frameCountGpuMem(),
    m_frameCntReg(frameCountRegOffset),
    m_useFixedLateAllocVsLimit(false),
    m_lateAllocVsLimit(LateAllocVsInvalid),
    m_smallPrimFilter(SmallPrimFilterEnableAll),
    m_waEnableDccCacheFlushAndInvalidate(false),
    m_waTcCompatZRange(false),
    m_degeneratePrimFilter(false),
    m_pSettingsLoader(nullptr),
    m_allocator(pDevice->GetPlatform())
{
    for (uint32 i = 0; i < QueueType::QueueTypeCount; i++)
    {
        m_pFrameCountCmdBuffer[i] = nullptr;
    }
    memset(m_flglRegSeq, 0, sizeof(m_flglRegSeq));

    memset(m_fastClearImageRefs, 0, sizeof(m_fastClearImageRefs));
}

// =====================================================================================================================
GfxDevice::~GfxDevice()
{
    // Note that GfxDevice does not own the m_pRsrcProcMgr instance so it is not deleted here.

    if (m_pSettingsLoader != nullptr)
    {
        PAL_SAFE_DELETE(m_pSettingsLoader, m_pParent->GetPlatform());
    }
}

// =====================================================================================================================
// This must clean up all internal GPU memory allocations and all objects created after EarlyInit. Note that EarlyInit
// is called when the platform creates the device objects so the work it does must be preserved if we are to reuse
// this device object.
Result GfxDevice::Cleanup()
{
    Result result = Result::Success;

#if DEBUG
    if (m_debugStallGpuMem.IsBound())
    {
        result = m_pParent->MemMgr()->FreeGpuMem(m_debugStallGpuMem.Memory(), m_debugStallGpuMem.Offset());
        m_debugStallGpuMem.Update(nullptr, 0);
    }
#endif

    for (uint32 i = 0; i < QueueType::QueueTypeCount; i++)
    {
        if (m_pFrameCountCmdBuffer[i] != nullptr)
        {
            m_pFrameCountCmdBuffer[i]->DestroyInternal();
            m_pFrameCountCmdBuffer[i] = nullptr;
        }
    }

    return result;
}

// =====================================================================================================================
// Performs initialization of hardware layer settings.
Result GfxDevice::InitHwlSettings(
    PalSettings* pSettings)
{
    Result ret = Result::Success;

    // Make sure we only initialize settings once
    if (m_pSettingsLoader == nullptr)
    {
        switch (m_pParent->ChipProperties().gfxLevel)
        {
#if PAL_BUILD_GFX6
        case GfxIpLevel::GfxIp6:
        case GfxIpLevel::GfxIp7:
        case GfxIpLevel::GfxIp8:
        case GfxIpLevel::GfxIp8_1:
            m_pSettingsLoader = Gfx6::CreateSettingsLoader(&m_allocator, m_pParent);
            break;
#endif
#if PAL_BUILD_GFX9
        case GfxIpLevel::GfxIp9:
            m_pSettingsLoader = Gfx9::CreateSettingsLoader(&m_allocator, m_pParent);
            break;
#endif // PAL_BUILD_GFX9
        case GfxIpLevel::None:
        default:
            break;
        }

        if (m_pSettingsLoader == nullptr)
        {
            ret = Result::ErrorOutOfMemory;
        }
        else
        {
            ret = m_pSettingsLoader->Init();
        }
    }

    if (ret == Result::Success)
    {
        HwlOverrideDefaultSettings(pSettings);
    }

    return ret;
}

// =====================================================================================================================
const PalSettings& GfxDevice::CoreSettings() const
{
    return m_pParent->Settings();
}

// =====================================================================================================================
// Finalizes any chip properties which depend on settings being read.
void GfxDevice::FinalizeChipProperties(
    GpuChipProperties* pChipProperties
    ) const
{
    // The maximum number of supported user-data entries is controlled by a public PAL setting.
    pChipProperties->gfxip.maxUserDataEntries = m_pParent->GetPublicSettings()->maxUserDataEntries;

    // Default to supporting the full 1024 threads-per-group. If necessary, the hardware layer will reduce this.
    constexpr uint32 MaxThreadsPerGroup = 1024;
    pChipProperties->gfxip.maxThreadGroupSize             = MaxThreadsPerGroup;
    pChipProperties->gfxip.maxAsyncComputeThreadGroupSize = MaxThreadsPerGroup;
}

// =====================================================================================================================
// Peforms extra initialization which needs to be done after the parent Device is finalized.
Result GfxDevice::Finalize()
{
    Result result = Result::Success;

#if DEBUG
    if (result == Result::Success)
    {
        GpuMemoryCreateInfo memCreateInfo = { };
        memCreateInfo.alignment = sizeof(uint32);
        memCreateInfo.size      = sizeof(uint32);
        memCreateInfo.priority  = GpuMemPriority::Normal;
        memCreateInfo.vaRange   = VaRange::Default;
        memCreateInfo.heaps[0]  = GpuHeapGartUswc;
        memCreateInfo.heaps[1]  = GpuHeapGartCacheable;
        memCreateInfo.heapCount = 2;

        GpuMemoryInternalCreateInfo internalInfo = { };
        internalInfo.flags.alwaysResident = 1;

        GpuMemory* pMemObj   = nullptr;
        gpusize    memOffset = 0;
        result = m_pParent->MemMgr()->AllocateGpuMem(memCreateInfo, internalInfo, false, &pMemObj, &memOffset);
        if (result == Result::Success)
        {
            m_debugStallGpuMem.Update(pMemObj, memOffset);
        }
    }
#endif

    return result;
}

// =====================================================================================================================
// Creates an internal compute pipeline object by allocating memory then calling the usual create method.
Result GfxDevice::CreateComputePipelineInternal(
    const ComputePipelineCreateInfo& createInfo,
    ComputePipeline**                ppPipeline,
    Util::SystemAllocType            allocType)
{
    Result result = Result::ErrorOutOfMemory;

    void* pMemory = PAL_MALLOC(GetComputePipelineSize(createInfo, nullptr), GetPlatform(), allocType);

    if (pMemory != nullptr)
    {
        result = CreateComputePipeline(createInfo, pMemory, true, reinterpret_cast<IPipeline**>(ppPipeline));

        if (result != Result::Success)
        {
            PAL_SAFE_FREE(pMemory, GetPlatform());
        }
    }

    return result;
}

// =====================================================================================================================
// Creates an internal graphics pipeline object by allocating memory then calling the usual create method.
Result GfxDevice::CreateGraphicsPipelineInternal(
    const GraphicsPipelineCreateInfo&         createInfo,
    const GraphicsPipelineInternalCreateInfo& internalInfo,
    GraphicsPipeline**                        ppPipeline,
    Util::SystemAllocType                     allocType)
{
    Result result = Result::ErrorOutOfMemory;

    void* pMemory = PAL_MALLOC(GetGraphicsPipelineSize(createInfo, true, nullptr), GetPlatform(), allocType);

    if (pMemory != nullptr)
    {
        result = CreateGraphicsPipeline(createInfo,
                                        internalInfo,
                                        pMemory,
                                        true,
                                        reinterpret_cast<IPipeline**>(ppPipeline));

        if (result != Result::Success)
        {
            PAL_SAFE_FREE(pMemory, GetPlatform());
        }
    }

    return result;
}

// =====================================================================================================================
// Creates an internal color blend state object by allocating memory then calling the usual create method.
Result GfxDevice::CreateColorBlendStateInternal(
    const ColorBlendStateCreateInfo& createInfo,
    ColorBlendState**                ppBlendState,
    Util::SystemAllocType            allocType
    ) const
{
    Result result = Result::ErrorOutOfMemory;

    void* pMemory = PAL_MALLOC(GetColorBlendStateSize(createInfo, nullptr), GetPlatform(), allocType);

    if (pMemory != nullptr)
    {
        result = CreateColorBlendState(createInfo,
                                       pMemory,
                                       reinterpret_cast<IColorBlendState**>(ppBlendState));

        if (result != Result::Success)
        {
            PAL_SAFE_FREE(pMemory, GetPlatform());
        }
    }

    return result;
}

// =====================================================================================================================
// Creates an internal depth stencil state object by allocating memory then calling the usual create method.
Result GfxDevice::CreateDepthStencilStateInternal(
    const DepthStencilStateCreateInfo& createInfo,
    DepthStencilState**                ppDepthStencilState,
    Util::SystemAllocType              allocType
    ) const
{
    Result result = Result::ErrorOutOfMemory;

    void* pMemory = PAL_MALLOC(GetDepthStencilStateSize(createInfo, nullptr), GetPlatform(), allocType);

    if (pMemory != nullptr)
    {
        result = CreateDepthStencilState(createInfo,
                                         pMemory,
                                         reinterpret_cast<IDepthStencilState**>(ppDepthStencilState));

        if (result != Result::Success)
        {
            PAL_SAFE_FREE(pMemory, GetPlatform());
        }
    }

    return result;
}

// =====================================================================================================================
// Creates an internal msaa state object by allocating memory then calling the usual create method.
Result GfxDevice::CreateMsaaStateInternal(
    const MsaaStateCreateInfo& createInfo,
    MsaaState**                ppMsaaState,
    Util::SystemAllocType      allocType
    ) const
{
    Result result = Result::ErrorOutOfMemory;

    void* pMemory = PAL_MALLOC(GetMsaaStateSize(createInfo, nullptr), GetPlatform(), allocType);

    if (pMemory != nullptr)
    {
        result = CreateMsaaState(createInfo, pMemory, reinterpret_cast<IMsaaState**>(ppMsaaState));

        if (result != Result::Success)
        {
            PAL_SAFE_FREE(pMemory, GetPlatform());
        }
    }

    return result;
}

// =====================================================================================================================
Platform* GfxDevice::GetPlatform() const
{
    return m_pParent->GetPlatform();
}

// =====================================================================================================================
// Helper function that disables a specific CU mask within the UMD managed range.
uint32 GfxDevice::GetCuEnableMaskInternal(
    uint32 disabledCuMask,          // Mask of CU's to explicitly disabled.  These CU's are virtualized so that PAL
                                    // doesn't need to worry about any yield-harvested CU's.
    uint32 enabledCuMaskSetting     // Mask of CU's a shader can run on based on a setting
    ) const
{
    const uint32 cuMaskSetting = enabledCuMaskSetting;

    uint32 cuMask = ~disabledCuMask;
    if ((cuMask & cuMaskSetting) != 0)
    {
        // If the provided setting value doesn't cause all CU's to be masked-off, then apply the mask specified in
        // the setting.
        cuMask &= cuMaskSetting;
    }

#if PAL_ENABLE_PRINTS_ASSERTS
    // The mask of CU's reserved by the KMD is also virtualized.
    const uint32 reservedCuMask = Parent()->ChipProperties().gfxip.realTimeCuMask;
    PAL_ASSERT((reservedCuMask & 0xFFFF0000) == 0);

    // If this assert triggers, CUs that are currently reserved by KMD are being disabled by PAL, which is illegal.
    PAL_ASSERT((reservedCuMask & disabledCuMask) == 0);
#endif

    return cuMask;
}

// =====================================================================================================================
// Helper function that disables a specific CU mask within the UMD managed range.
uint16 GfxDevice::GetCuEnableMask(
    uint16 disabledCuMask,          // Mask of CU's to explicitly disabled.  These CU's are virtualized so that PAL
                                    // doesn't need to worry about any yield-harvested CU's.
    uint32 enabledCuMaskSetting     // Mask of CU's a shader can run on based on a setting
    ) const
{
    return GetCuEnableMaskInternal(disabledCuMask, enabledCuMaskSetting) & 0xFFFF;
}

// =====================================================================================================================
// Helper to check if this Device can support launching a CE preamble command stream with every Universal Queue
// submission.
bool GfxDevice::SupportsCePreamblePerSubmit() const
{
    // We can only submit a CE preamble stream with each submission if the Device supports at least five command
    // streams per submission.
    return (Parent()->QueueProperties().maxNumCmdStreamsPerSubmit >= 5);
}

// =====================================================================================================================
// Returns the buffer that contains command to write to frame count register and increment the GPU memory. If it`s
// called the first time, the buffer will be initialized.
Result GfxDevice::InitAndGetFrameCountCmdBuffer(
    QueueType      queueType,
    EngineType     engineType,
    GfxCmdBuffer** ppBuffer)
{
    PAL_ASSERT((queueType == QueueType::QueueTypeCompute) || (queueType == QueueType::QueueTypeUniversal));

    Result result = Result::Success;

    if ((m_pFrameCountCmdBuffer[queueType] == nullptr) && (m_frameCntReg != 0))
    {
        if(m_frameCountGpuMem.Memory() == nullptr)
        {
            GpuMemoryCreateInfo createInfo = {};
            createInfo.alignment = sizeof(uint32);
            createInfo.size      = sizeof(uint32);
            createInfo.priority  = GpuMemPriority::Normal;
            createInfo.heaps[0]  = GpuHeapLocal;
            createInfo.heaps[1]  = GpuHeapGartUswc;
            createInfo.heapCount = 2;

            GpuMemoryInternalCreateInfo internalInfo = {};
            internalInfo.flags.alwaysResident = 1;

            gpusize    memOffset    = 0;
            GpuMemory* pFrameGpuMem = nullptr;

            result = m_pParent->MemMgr()->AllocateGpuMem(createInfo,
                                                         internalInfo,
                                                         false,
                                                         &pFrameGpuMem,
                                                         &memOffset);
            m_frameCountGpuMem.Update(pFrameGpuMem, memOffset);

            if (result == Result::Success)
            {
                char* pData = nullptr;
                result = m_frameCountGpuMem.Map(reinterpret_cast<void**>(&pData));

                if (result == Result::Success)
                {
                    memset(pData, 0, sizeof(uint32));
                    result = m_frameCountGpuMem.Unmap();
                }
            }
        }

        if (result == Result::Success)
        {
            CmdBuffer** ppFrameCountCmdBuffer = reinterpret_cast<CmdBuffer**>(&m_pFrameCountCmdBuffer[queueType]);

            CmdBufferCreateInfo cmdBufferCreateInfo = {};
            cmdBufferCreateInfo.pCmdAllocator = m_pParent->InternalCmdAllocator(engineType);
            cmdBufferCreateInfo.queueType     = queueType;

            cmdBufferCreateInfo.engineType    = engineType;

            CmdBufferInternalCreateInfo cmdBufferInternalInfo = {};
            cmdBufferInternalInfo.flags.isInternal = 1;

            result = m_pParent->CreateInternalCmdBuffer(cmdBufferCreateInfo,
                                                        cmdBufferInternalInfo,
                                                        ppFrameCountCmdBuffer);
        }

        if (result == Result::Success)
        {
            CmdBufferBuildInfo buildInfo = {};
            result = m_pFrameCountCmdBuffer[queueType]->Begin(buildInfo);
        }

        if (result == Result::Success)
        {
            m_pFrameCountCmdBuffer[queueType]->AddPerPresentCommands(m_frameCountGpuMem.GpuVirtAddr(), m_frameCntReg);
            result = m_pFrameCountCmdBuffer[queueType]->End();
        }
    }
    *ppBuffer = m_pFrameCountCmdBuffer[queueType];

    return result;
}

// =====================================================================================================================
// Call back to above layers to describe a compute dispatch command
void GfxDevice::DescribeDispatch(
    GfxCmdBuffer*               pCmdBuf,
    Developer::DrawDispatchType cmdType,
    uint32                      xOffset,
    uint32                      yOffset,
    uint32                      zOffset,
    uint32                      xDim,
    uint32                      yDim,
    uint32                      zDim
    ) const
{
    Developer::DrawDispatchData data = {};

    data.pCmdBuffer                    = pCmdBuf;
    data.cmdType                       = cmdType;
    data.dispatch.groupStart[0]        = xOffset;
    data.dispatch.groupStart[1]        = yOffset;
    data.dispatch.groupStart[2]        = zOffset;
    data.dispatch.groupDims[0]         = xDim;
    data.dispatch.groupDims[1]         = yDim;
    data.dispatch.groupDims[2]         = zDim;

    m_pParent->DeveloperCb(Developer::CallbackType::DrawDispatch, &data);
}

// =====================================================================================================================
// Call back to above layers to describe a graphics draw command
void GfxDevice::DescribeDraw(
    GfxCmdBuffer*               pCmdBuf,
    Developer::DrawDispatchType cmdType,
    uint32                      firstVertexUserDataIdx,
    uint32                      instanceOffsetUserDataIdx,
    uint32                      drawIndexUserDataIdx
    ) const
{
    Developer::DrawDispatchData data = {};

    data.pCmdBuffer                       = pCmdBuf;
    data.cmdType                          = cmdType;
    data.draw.userDataRegs.firstVertex    = firstVertexUserDataIdx;
    data.draw.userDataRegs.instanceOffset = instanceOffsetUserDataIdx;
    data.draw.userDataRegs.drawIndex      = drawIndexUserDataIdx;

    m_pParent->DeveloperCb(Developer::CallbackType::DrawDispatch, &data);
}

// =====================================================================================================================
// Returns a pointer to an unused index in the fast clear ref count array for use of the image. Returns nullptr if
// allocation was unsuccessful.
uint32* GfxDevice::AllocateFceRefCount()
{
    uint32* pCounter = nullptr;

    if (m_pParent->GetPublicSettings()->disableSkipFceOptimization == false)
    {
        for (uint32 i = 0; i < MaxNumFastClearImageRefs; ++i)
        {
            if (m_fastClearImageRefs[i] == 0)
            {
                if (AtomicCompareAndSwap(&m_fastClearImageRefs[i], RefCounterState::Free, RefCounterState::InUse) == 0)
                {
                    // The index was acquired, so return a pointer.
                    pCounter = &m_fastClearImageRefs[i];
                    break;
                }
            }
        }
    }

    return pCounter;
}

} // Pal
