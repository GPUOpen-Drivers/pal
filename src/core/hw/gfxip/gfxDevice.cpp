/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palFormatInfo.h"
#include "core/device.h"
#include "g_coreSettings.h"
#include "core/platform.h"
#include "core/hw/gfxip/colorBlendState.h"
#include "core/hw/gfxip/depthStencilState.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/msaaState.h"
#include "core/hw/gfxip/rpm/rpmUtil.h"
#include "core/hw/gfxip/rpm/rsrcProcMgr.h"
#include "palHashMapImpl.h"
#include "addrinterface.h"
#include "dd_settings_base.h"
#include "dd_settings_service.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
// Detects dual-source blend modes.
static bool IsDualSrcBlendOption(
    Blend blend)
{
    bool isDualSrcBlendOption = false;

    switch (blend)
    {
    case Blend::Src1Color:
    case Blend::OneMinusSrc1Color:
    case Blend::Src1Alpha:
    case Blend::OneMinusSrc1Alpha:
        isDualSrcBlendOption = true;
        break;
    default:
        break;
    }

    return isDualSrcBlendOption;
}

// =====================================================================================================================
GfxDevice::GfxDevice(
    Device*           pDevice,
    Pal::RsrcProcMgr* pRsrcProcMgr)
    :
    m_pParent(pDevice),
    m_pRsrcProcMgr(pRsrcProcMgr),
    m_pSettingsLoader(nullptr),
    m_pDdSettingsLoader(nullptr),
    m_queueContextUpdateCounter(0),
    m_pipelineLoader(pDevice)
{
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

    if (m_pDdSettingsLoader != nullptr)
    {
        PAL_SAFE_DELETE(m_pDdSettingsLoader, m_pParent->GetPlatform());
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

        if ((m_pParent->GetPlatform() != nullptr) && (m_pParent->GetPlatform()->GetGpuMemoryEventProvider() != nullptr))
        {
            ResourceDestroyEventData destroyData = {};
            destroyData.pObj = &m_debugStallGpuMem;
            m_pParent->GetPlatform()->GetGpuMemoryEventProvider()->LogGpuMemoryResourceDestroyEvent(destroyData);
        }
    }
#endif

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
        case GfxIpLevel::GfxIp10_1:
        case GfxIpLevel::GfxIp10_3:
        case GfxIpLevel::GfxIp11_0:
            m_pDdSettingsLoader = Gfx9::CreateSettingsLoader(m_pParent);
            break;
        case GfxIpLevel::None:
        default:
            break;
        }

        if ((m_pSettingsLoader == nullptr) && (m_pDdSettingsLoader == nullptr))
        {
            ret = Result::ErrorOutOfMemory;
        }
        else
        {
            ret = InitSettings();
            if (ret == Result::Success)
            {
                HwlOverrideDefaultSettings(pSettings);

                HwlReadSettings();

                // Register this component to receive setting user-overrides via DevDriver network.
                DevDriver::SettingsRpcService* pSettingsRpcService
                    = m_pParent->GetPlatform()->GetSettingsRpcService();
                if ((pSettingsRpcService != nullptr) && (m_pDdSettingsLoader != nullptr))
                {
                    pSettingsRpcService->RegisterSettingsComponent(m_pDdSettingsLoader);
                }
            }
        }
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
    pChipProperties->gfxip.maxUserDataEntries = MaxUserDataEntries;

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
        internalInfo.mtype = MType::Uncached;

        GpuMemory* pMemObj   = nullptr;
        gpusize    memOffset = 0;
        result = m_pParent->MemMgr()->AllocateGpuMem(memCreateInfo, internalInfo, false, &pMemObj, &memOffset);
        if (result == Result::Success)
        {
            m_debugStallGpuMem.Update(pMemObj, memOffset);

            if ((m_pParent->GetPlatform() != nullptr) && (m_pParent->GetPlatform()->GetGpuMemoryEventProvider() != nullptr))
            {
                ResourceDescriptionMiscInternal desc;
                desc.type = MiscInternalAllocType::DummyChunk;

                ResourceCreateEventData createData = {};
                createData.type = ResourceType::MiscInternal;
                createData.pObj = &m_debugStallGpuMem;
                createData.pResourceDescData = &desc;
                createData.resourceDescSize = sizeof(ResourceDescriptionMiscInternal);

                m_pParent->GetPlatform()->GetGpuMemoryEventProvider()->LogGpuMemoryResourceCreateEvent(createData);

                GpuMemoryResourceBindEventData bindData = {};
                bindData.pGpuMemory = pMemObj;
                bindData.pObj = &m_debugStallGpuMem;
                bindData.offset = memOffset;
                bindData.requiredGpuMemSize = memCreateInfo.size;
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
    Result result  = Result::ErrorOutOfMemory;
    void*  pMemory = PAL_MALLOC(GetColorBlendStateSize(), GetPlatform(), allocType);

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
// Destroys an internal ColorBlendState object
void GfxDevice::DestroyColorBlendStateInternal(
    ColorBlendState* pColorBlendState
    ) const
{
    if (pColorBlendState != nullptr)
    {
        pColorBlendState->Destroy();
        PAL_FREE(pColorBlendState, GetPlatform());
    }
}

// =====================================================================================================================
// Creates an internal depth stencil state object by allocating memory then calling the usual create method.
Result GfxDevice::CreateDepthStencilStateInternal(
    const DepthStencilStateCreateInfo& createInfo,
    DepthStencilState**                ppDepthStencilState,
    Util::SystemAllocType              allocType
    ) const
{
    Result result  = Result::ErrorOutOfMemory;
    void*  pMemory = PAL_MALLOC(GetDepthStencilStateSize(), GetPlatform(), allocType);

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
// Destroys an internal DepthStencilState object
void GfxDevice::DestroyDepthStencilStateInternal(
    DepthStencilState* pDepthStencilState
    ) const
{
    if (pDepthStencilState != nullptr)
    {
        pDepthStencilState->Destroy();
        PAL_FREE(pDepthStencilState, GetPlatform());
    }
}

// =====================================================================================================================
// Creates an internal msaa state object by allocating memory then calling the usual create method.
Result GfxDevice::CreateMsaaStateInternal(
    const MsaaStateCreateInfo& createInfo,
    MsaaState**                ppMsaaState,
    Util::SystemAllocType      allocType
    ) const
{
    Result result  = Result::ErrorOutOfMemory;
    void*  pMemory = PAL_MALLOC(GetMsaaStateSize(), GetPlatform(), allocType);

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
// Destroys an internal MsaaState object
void GfxDevice::DestroyMsaaStateInternal(
    MsaaState* pMsaaState
    ) const
{
    if (pMsaaState != nullptr)
    {
        pMsaaState->Destroy();
        PAL_FREE(pMsaaState, GetPlatform());
    }
}

// =====================================================================================================================
Platform* GfxDevice::GetPlatform() const
{
    return m_pParent->GetPlatform();
}

// =====================================================================================================================
uint32 GfxDevice::QueueContextUpdateCounter()
{
    const MutexAuto lock(&m_queueContextUpdateLock);
    return m_queueContextUpdateCounter;
}

// =====================================================================================================================
// Updates the GPU memory bound for use as a trap handler for either compute or graphics pipelines.  Updates the queue
// context update counter so that the next submission on each queue will properly process this update.
void GfxDevice::BindTrapHandler(
    PipelineBindPoint pipelineType,
    IGpuMemory*       pGpuMemory,
    gpusize           offset)
{
    const MutexAuto lock(&m_queueContextUpdateLock);

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
void GfxDevice::BindTrapBuffer(
    PipelineBindPoint pipelineType,
    IGpuMemory*       pGpuMemory,
    gpusize           offset)
{
    const MutexAuto lock(&m_queueContextUpdateLock);

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

    // The mask of CU's reserved by the KMD is also virtualized.
    // If this assert triggers, CUs that are currently reserved by KMD are being disabled by PAL, which is illegal.
    PAL_ASSERT((Parent()->ChipProperties().gfxip.realTimeCuMask & disabledCuMask) == 0);

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
// Call back to above layers to describe a compute dispatch command
void GfxDevice::DescribeDispatch(
    GfxCmdBuffer*               pCmdBuf,
    RgpMarkerSubQueueFlags      subQueueFlags,
    Developer::DrawDispatchType cmdType,
    DispatchDims                offset,
    DispatchDims                launchSize,
    DispatchDims                logicalSize
    ) const
{
    Developer::DrawDispatchData data { };

    data.pCmdBuffer           = pCmdBuf;
    data.subQueueFlags        = subQueueFlags;
    data.cmdType              = cmdType;
    data.dispatch.groupStart  = offset;
    data.dispatch.groupDims   = launchSize;
    data.dispatch.logicalSize = logicalSize;

    m_pParent->DeveloperCb(Developer::CallbackType::DrawDispatch, &data);
}

// =====================================================================================================================
// Call back to above layers to describe a graphics draw command
void GfxDevice::DescribeDraw(
    GfxCmdBuffer*               pCmdBuf,
    RgpMarkerSubQueueFlags      subQueueFlags,
    Developer::DrawDispatchType cmdType,
    uint32                      firstVertexUserDataIdx,
    uint32                      instanceOffsetUserDataIdx,
    uint32                      drawIndexUserDataIdx
    ) const
{
    Developer::DrawDispatchData data { };

    data.pCmdBuffer                       = pCmdBuf;
    data.subQueueFlags                    = subQueueFlags;
    data.cmdType                          = cmdType;
    data.draw.userDataRegs.firstVertex    = firstVertexUserDataIdx;
    data.draw.userDataRegs.instanceOffset = instanceOffsetUserDataIdx;
    data.draw.userDataRegs.drawIndex      = drawIndexUserDataIdx;

    m_pParent->DeveloperCb(Developer::CallbackType::DrawDispatch, &data);
}

// =====================================================================================================================
// Call back to above layers to describe a bind pipeline command
void GfxDevice::DescribeBindPipeline(
    GfxCmdBuffer*     pCmdBuf,
    const IPipeline*  pPipeline,
    uint64            apiPsoHash,
    PipelineBindPoint bindPoint
    ) const
{
    Developer::BindPipelineData data = {};

    data.pPipeline  = pPipeline;
    data.pCmdBuffer = pCmdBuf;
    data.apiPsoHash = apiPsoHash;
    data.bindPoint  = bindPoint;

    m_pParent->DeveloperCb(Developer::CallbackType::BindPipeline, &data);
}

#if PAL_DEVELOPER_BUILD
// =====================================================================================================================
// Call back to above layers to describe a draw- or dispatch-time validation.
void GfxDevice::DescribeDrawDispatchValidation(
    GfxCmdBuffer* pCmdBuf,
    size_t        userDataCmdSize,
    size_t        miscCmdSize
    ) const
{
    Developer::DrawDispatchValidationData data = { };
    data.pCmdBuffer      = pCmdBuf;
    data.userDataCmdSize = static_cast<uint32>(userDataCmdSize);
    data.miscCmdSize     = static_cast<uint32>(miscCmdSize);

    m_pParent->DeveloperCb(Developer::CallbackType::DrawDispatchValidation, &data);
}

// =====================================================================================================================
// Call back to above layers to describe a bound pipeline.
void GfxDevice::DescribeBindPipelineValidation(
    GfxCmdBuffer* pCmdBuf,
    size_t        pipelineCmdSize
    ) const
{
    Developer::BindPipelineValidationData data = { };
    data.pCmdBuffer      = pCmdBuf;
    data.pipelineCmdSize = static_cast<uint32>(pipelineCmdSize);;

    m_pParent->DeveloperCb(Developer::CallbackType::BindPipelineValidation, &data);
}

// =====================================================================================================================
// Call back to above layers to describe the writes to registers seen using SET or RMW packets.
void GfxDevice::DescribeHotRegisters(
    GfxCmdBuffer* pCmdBuf,
    const uint32* pShRegSeenSets,
    const uint32* pShRegKeptSets,
    uint32        shRegCount,
    uint16        shRegBase,
    const uint32* pCtxRegSeenSets,
    const uint32* pCtxRegKeptSets,
    uint32        ctxRegCount,
    uint16        ctxRegBase
    ) const
{
    Developer::OptimizedRegistersData data = { };
    data.pCmdBuffer      = pCmdBuf;
    data.pShRegSeenSets  = pShRegSeenSets;
    data.pShRegKeptSets  = pShRegKeptSets;
    data.shRegCount      = shRegCount;
    data.shRegBase       = shRegBase;
    data.pCtxRegSeenSets = pCtxRegSeenSets;
    data.pCtxRegKeptSets = pCtxRegKeptSets;
    data.ctxRegCount     = ctxRegCount;
    data.ctxRegBase      = ctxRegBase;

    m_pParent->DeveloperCb(Developer::CallbackType::OptimizedRegisters, &data);
}
#endif

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

// =====================================================================================================================
void GfxDevice::InitAddrLibChipId(
    ADDR_CREATE_INPUT*  pInput
    ) const
{
    const GpuChipProperties& chipProps = Parent()->ChipProperties();

    pInput->chipEngine   = chipProps.gfxEngineId;
    pInput->chipRevision = chipProps.eRevId;
    pInput->chipFamily   = chipProps.familyId;
}

// =====================================================================================================================
// Tells whether our dual-source blend ops will be overwritten
bool GfxDevice::CanEnableDualSourceBlend(
    const ColorBlendStateCreateInfo& createInfo) const
{
    bool dualSourceBlendEnable = (IsDualSrcBlendOption(createInfo.targets[0].srcBlendColor) |
                                  IsDualSrcBlendOption(createInfo.targets[0].dstBlendColor) |
                                  IsDualSrcBlendOption(createInfo.targets[0].srcBlendAlpha) |
                                  IsDualSrcBlendOption(createInfo.targets[0].dstBlendAlpha));

    bool funcColorIsMinMax = (createInfo.targets[0].blendFuncColor == BlendFunc::Min ||
                              createInfo.targets[0].blendFuncColor == BlendFunc::Max);
    bool funcAlphaIsMinMax = (createInfo.targets[0].blendFuncAlpha == BlendFunc::Min ||
                              createInfo.targets[0].blendFuncAlpha == BlendFunc::Max);

    if (createInfo.targets[0].blendEnable == false)
    {
        dualSourceBlendEnable = false;
    }
    else if (funcColorIsMinMax && funcAlphaIsMinMax)
    {
        dualSourceBlendEnable = false;
    }
    else if ((IsDualSrcBlendOption(createInfo.targets[0].srcBlendAlpha) == false) &&
             (IsDualSrcBlendOption(createInfo.targets[0].dstBlendAlpha) == false) &&
             funcColorIsMinMax)
    {
        dualSourceBlendEnable = false;
    }
    else if ((IsDualSrcBlendOption(createInfo.targets[0].srcBlendColor) == false) &&
             (IsDualSrcBlendOption(createInfo.targets[0].dstBlendColor) == false) &&
             funcAlphaIsMinMax)
    {
        dualSourceBlendEnable = false;
    }

    return dualSourceBlendEnable;
}

// =====================================================================================================================
uint32 GfxDevice::VertsPerPrimitive(
    PrimitiveTopology topology,
    uint32            patchControlPoints)
{
    uint32 vertsPerPrimitive = 1;
    switch (topology)
    {
    case PrimitiveTopology::PointList:
        vertsPerPrimitive = 1;
        break;
    case PrimitiveTopology::LineList:
    case PrimitiveTopology::LineStrip:
    case PrimitiveTopology::LineLoop:
        vertsPerPrimitive = 2;
        break;

    case PrimitiveTopology::TriangleList:
    case PrimitiveTopology::TriangleStrip:
    case PrimitiveTopology::RectList:
    case PrimitiveTopology::TriangleFan:
    case PrimitiveTopology::Polygon:
    case PrimitiveTopology::TwoDRectList:
        vertsPerPrimitive = 3;
        break;

    case PrimitiveTopology::QuadList:
    case PrimitiveTopology::QuadStrip:
        vertsPerPrimitive = 4;
        break;

    case PrimitiveTopology::LineListAdj:
    case PrimitiveTopology::LineStripAdj:
        vertsPerPrimitive = 4;
        break;

    case PrimitiveTopology::TriangleListAdj:
    case PrimitiveTopology::TriangleStripAdj:
        vertsPerPrimitive = 6;
        break;

    case PrimitiveTopology::Patch:
        PAL_ASSERT(patchControlPoints > 0);
        vertsPerPrimitive = patchControlPoints;
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return vertsPerPrimitive;
}

// =====================================================================================================================
uint32 GfxDevice::VertsPerPrimitive(
    PrimitiveTopology topology)
{
    uint32 vertsPerPrimitive = 1;
    switch (topology)
    {
    case PrimitiveTopology::PointList:
        vertsPerPrimitive = 1;
        break;
    case PrimitiveTopology::LineList:
    case PrimitiveTopology::LineStrip:
    case PrimitiveTopology::LineLoop:
    case PrimitiveTopology::LineListAdj:
    case PrimitiveTopology::LineStripAdj:
        vertsPerPrimitive = 2;
        break;
    case PrimitiveTopology::TriangleList:
    case PrimitiveTopology::TriangleStrip:
    case PrimitiveTopology::RectList:
    case PrimitiveTopology::TriangleFan:
    case PrimitiveTopology::Polygon:
    case PrimitiveTopology::TwoDRectList:
    case PrimitiveTopology::QuadList:
    case PrimitiveTopology::QuadStrip:
    case PrimitiveTopology::TriangleListAdj:
    case PrimitiveTopology::TriangleStripAdj:
        vertsPerPrimitive = 3;
        break;

    default:
        break;
    }

    return vertsPerPrimitive;
}

// =====================================================================================================================
bool GfxDevice::IsValidTypedBufferView(
    const BufferViewInfo& view)
{
    bool isValid = true;

    const uint32 bpp = Pal::Formats::BytesPerPixel(view.swizzledFormat.format);

    // Typed buffer loads require element size is min(DWORD, ElementsSize).
    const gpusize requiredAlignment = Util::Min(bpp, uint32(sizeof(uint32)));

    if (view.gpuAddr == 0)
    {
        isValid = false;
    }
    if (Formats::IsUndefined(view.swizzledFormat.format))
    {
        isValid = false;
    }
    if ((view.gpuAddr % requiredAlignment) != 0)
    {
        isValid = false;
    }
    if ((view.stride % requiredAlignment) != 0)
    {
        isValid = false;
    }

    return isValid;
}

// Default sample positions, indexed via Log2(numSamples).
const MsaaQuadSamplePattern GfxDevice::DefaultSamplePattern[] = {
    // 1x
    {
        { 0, 0, },
        { 0, 0, },
        { 0, 0, },
        { 0, 0, },
    },
    // 2x
    {
        { { 4, 4, }, { -4, -4, }, },
        { { 4, 4, }, { -4, -4, }, },
        { { 4, 4, }, { -4, -4, }, },
        { { 4, 4, }, { -4, -4, }, },
    },
    // 4x
    {
        { { -2, -6, }, { 6, -2, }, { -6, 2, }, { 2, 6, }, },
        { { -2, -6, }, { 6, -2, }, { -6, 2, }, { 2, 6, }, },
        { { -2, -6, }, { 6, -2, }, { -6, 2, }, { 2, 6, }, },
        { { -2, -6, }, { 6, -2, }, { -6, 2, }, { 2, 6, }, },
    },
    // 8x
    {
        { { 1, -3, }, { -1, 3, }, { 5, 1, }, { -3, -5, }, { -5, 5, }, { -7, -1, }, { 3, 7, }, { 7, -7, }, },
        { { 1, -3, }, { -1, 3, }, { 5, 1, }, { -3, -5, }, { -5, 5, }, { -7, -1, }, { 3, 7, }, { 7, -7, }, },
        { { 1, -3, }, { -1, 3, }, { 5, 1, }, { -3, -5, }, { -5, 5, }, { -7, -1, }, { 3, 7, }, { 7, -7, }, },
        { { 1, -3, }, { -1, 3, }, { 5, 1, }, { -3, -5, }, { -5, 5, }, { -7, -1, }, { 3, 7, }, { 7, -7, }, },
    },
    // 16x
    {
        {
            {  1, 1, }, { -1, -1, }, { -3,  2, }, {  4, -1, }, { -5, -2, }, { 2,  5, }, { 5, 3, }, {  3, -5, },
            { -2, 6, }, {  0, -7, }, { -4, -6, }, { -6,  4, }, { -8,  0, }, { 7, -4, }, { 6, 7, }, { -7, -8, },
        },
        {
            {  1, 1, }, { -1, -1, }, { -3,  2, }, {  4, -1, }, { -5, -2, }, { 2,  5, }, { 5, 3, }, {  3, -5, },
            { -2, 6, }, {  0, -7, }, { -4, -6, }, { -6,  4, }, { -8,  0, }, { 7, -4, }, { 6, 7, }, { -7, -8, },
        },
        {
            {  1, 1, }, { -1, -1, }, { -3,  2, }, {  4, -1, }, { -5, -2, }, { 2,  5, }, { 5, 3, }, {  3, -5, },
            { -2, 6, }, {  0, -7, }, { -4, -6, }, { -6,  4, }, { -8,  0, }, { 7, -4, }, { 6, 7, }, { -7, -8, },
        },
        {
            {  1, 1, }, { -1, -1, }, { -3,  2, }, {  4, -1, }, { -5, -2, }, { 2,  5, }, { 5, 3, }, {  3, -5, },
            { -2, 6, }, {  0, -7, }, { -4, -6, }, { -6,  4, }, { -8,  0, }, { 7, -4, }, { 6, 7, }, { -7, -8, },
        },
    },
};

// =====================================================================================================================
// Describes the image barrier to the above layers but only if we're a developer build. Clears the BarrierOperations
// passed in after calling back in case of layout transitions. This function is expected to be called only on layout
// transitions.
void GfxDevice::DescribeBarrier(
    GfxCmdBuffer*                 pCmdBuf,
    const BarrierTransition*      pTransition,
    Developer::BarrierOperations* pOperations
    ) const
{
    constexpr BarrierTransition NullTransition = {};
    Developer::BarrierData data                = {};

    data.pCmdBuffer    = pCmdBuf;
    data.transition    = (pTransition != nullptr) ? (*pTransition) : NullTransition;
    data.hasTransition = (pTransition != nullptr);

    PAL_ASSERT(pOperations != nullptr);
    // The callback is expected to be made only on layout transitions.
    memcpy(&data.operations, pOperations, sizeof(Developer::BarrierOperations));

    // Callback to the above layers if there is a transition and clear the BarrierOperations.
    m_pParent->DeveloperCb(Developer::CallbackType::ImageBarrier, &data);
    memset(pOperations, 0, sizeof(Developer::BarrierOperations));
}

// =====================================================================================================================
// Call back to above layers before starting the barrier execution.
void GfxDevice::DescribeBarrierStart(
    GfxCmdBuffer*          pCmdBuf,
    uint32                 reason,
    Developer::BarrierType type
    ) const
{
    Developer::BarrierData data = {};

    data.pCmdBuffer = pCmdBuf;

    // Make sure we have an acceptable barrier reason.
    PAL_ALERT_MSG((GetPlatform()->IsDevDriverProfilingEnabled() && (reason == Developer::BarrierReasonInvalid)),
                  "Invalid barrier reason codes are not allowed!");

    data.reason = reason;
    data.type   = type;

    m_pParent->DeveloperCb(Developer::CallbackType::BarrierBegin, &data);
}

// =====================================================================================================================
// Callback to above layers with summary information at end of barrier execution.
void GfxDevice::DescribeBarrierEnd(
    GfxCmdBuffer*                 pCmdBuf,
    Developer::BarrierOperations* pOperations
    ) const
{
    Developer::BarrierData data  = {};

    // Set the barrier type to an invalid type.
    data.pCmdBuffer    = pCmdBuf;

    PAL_ASSERT(pOperations != nullptr);
    memcpy(&data.operations, pOperations, sizeof(Developer::BarrierOperations));

    m_pParent->DeveloperCb(Developer::CallbackType::BarrierEnd, &data);
}

// =====================================================================================================================
ClearMethod GfxDevice::GetDefaultSlowClearMethod(
    const ImageCreateInfo&  createInfo,
    const SwizzledFormat&   clearFormat
    ) const
{
    uint32 texelScale = 1;
    RpmUtil::GetRawFormat(clearFormat.format, &texelScale, nullptr);

    // Force clears of scaled formats to the compute engine
    return (texelScale > 1) ? ClearMethod::NormalCompute : ClearMethod::NormalGraphics;
}

// =====================================================================================================================
// This function checks to see if an override is needed for the image format. The TC hardware treats YUV422 formats as
// 32bpp memory addressing instead of 16bpp. The HW scales the SRD width and x-coordinate accordingly for these formats.
bool GfxDevice::IsImageFormatOverrideNeeded(
    ChNumFormat* pFormat,
    uint32*      pPixelsPerBlock)
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

} // Pal
