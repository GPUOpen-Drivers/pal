/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/palToScpcWrapper.h"
#include "palShader.h"
#include "palShaderCache.h"
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
    m_pShaderCache(nullptr),
    m_frameCountGpuMem(),
    m_frameCntReg(frameCountRegOffset),
    m_useFixedLateAllocVsLimit(false),
    m_lateAllocVsLimit(LateAllocVsInvalid),
    m_smallPrimFilter(SmallPrimFilterEnableAll),
    m_waEnableDccCacheFlushAndInvalidate(false),
    m_waTcCompatZRange(false),
    m_degeneratePrimFilter(false)
{
    for (uint32 i = 0; i < QueueType::QueueTypeCount; i++)
    {
        m_pFrameCountCmdBuffer[i] = nullptr;
    }
    memset(m_flglRegSeq, 0, sizeof(m_flglRegSeq));
}

// =====================================================================================================================
GfxDevice::~GfxDevice()
{
    // Note that GfxDevice does not own the m_pRsrcProcMgr instance so it is not deleted here.

    // This object must be destroyed in Cleanup().
    PAL_ASSERT(m_pShaderCache == nullptr);
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

    if (m_pShaderCache != nullptr)
    {
        m_pShaderCache->Destroy();
        PAL_SAFE_FREE(m_pShaderCache, GetPlatform());
    }

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
// Performs any late-stage initialization that can only be done after settings have been committed.
Result GfxDevice::LateInit()
{
    Result result = Result::Success;
    auto shaderCacheMode = Parent()->GetPublicSettings()->shaderCacheMode;

    if (shaderCacheMode != ShaderCacheDisabled)
    {
        const size_t shaderCacheSize = GetShaderCacheSize();

        if (shaderCacheSize > 0)
        {
            void* pPlacementMem = PAL_MALLOC(shaderCacheSize, GetPlatform(), AllocInternal);
            if (pPlacementMem != nullptr)
            {
                const bool enableDiskCache = (shaderCacheMode == ShaderCacheOnDisk);

                m_pShaderCache = PAL_PLACEMENT_NEW(pPlacementMem) ShaderCache(*Parent());

                const ShaderCacheCreateInfo createInfo = { };
                result = m_pShaderCache->Init(createInfo, enableDiskCache);

                if (result != Result::Success)
                {
                    // NOTE: If for some reason the global shader cache failed to initialize, we should clean it up and
                    // proceed as though the cache were disabled.
                    PAL_ALERT_ALWAYS();

                    m_pShaderCache->Destroy();
                    PAL_SAFE_FREE(m_pShaderCache, GetPlatform());

                    result = Result::Success;
                }
            }
            else
            {
                // NOTE: If for some reason the global shader cache failed to initialize, we should clean it up and
                // proceeed as though the cache were disabled.
                PAL_ALERT_ALWAYS();
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Finalizes any chip properties which depend on settings being read.
void GfxDevice::FinalizeChipProperties(
    GpuChipProperties* pChipProperties
    ) const
{
    const auto& settings = Parent()->Settings();

    // The maximum number of supported user-data entries is controlled by a public PAL setting.
    pChipProperties->gfxip.maxUserDataEntries = m_pParent->GetPublicSettings()->maxUserDataEntries;

    // The effective number of fast user-data registers can be overridden by a PAL setting.
    for (uint32 i = 0; i < NumShaderTypes; ++i)
    {
        pChipProperties->gfxip.fastUserDataEntries[i] = Min(pChipProperties->gfxip.fastUserDataEntries[i],
                                                            settings.forcedUserDataSpillThreshold);
    }

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
// Allocates GPU memory which backs the ring buffers used for dumping CE RAM before draws and/or dispatches.
Result GfxDevice::AllocateCeRingBufferGpuMem(
    gpusize sizeInBytes,  // required size, in bytes, for any HWL-specific data (all ring entries)
    gpusize alignment)
{
    const PalSettings&       settings  = m_pParent->Settings();
    const GpuChipProperties& chipProps = m_pParent->ChipProperties();

    GpuMemoryCreateInfo memCreateInfo = { };
    memCreateInfo.alignment = alignment;
    memCreateInfo.priority  = GpuMemPriority::Normal;
    memCreateInfo.vaRange   = VaRange::DescriptorTable;

    if (chipProps.gpuType == GpuType::Integrated)
    {
        memCreateInfo.heapCount = 2;
        memCreateInfo.heaps[0]  = GpuHeap::GpuHeapGartUswc;
        memCreateInfo.heaps[1]  = GpuHeap::GpuHeapGartCacheable;
    }
    else
    {
        memCreateInfo.heapCount = 1;
        memCreateInfo.heaps[0]  = GpuHeap::GpuHeapInvisible;
    }

    // User-data spill table and stream-output table sizes:
    memCreateInfo.size = sizeInBytes;

    Result result = Result::Success;
    if (memCreateInfo.size > 0)
    {
        GpuMemoryInternalCreateInfo memInternalInfo = { };
        memInternalInfo.flags.alwaysResident = 1;

        // We need enough space to store two independent copies of these ring buffers: one each for root-level and
        // for nested command buffers.
        for (uint32 i = 0; i < 2; ++i)
        {
            GpuMemory* pGpuMemory = nullptr;
            gpusize    offset     = 0uLL;
            result = m_pParent->MemMgr()->AllocateGpuMem(memCreateInfo,
                                                         memInternalInfo,
                                                         false,
                                                         &pGpuMemory,
                                                         &offset);
            if (result == Result::Success)
            {
                m_ceRingBufferGpuMem[i].Update(pGpuMemory, offset);
            }
        }
    }

    return result;
}

// =====================================================================================================================
size_t GfxDevice::GetShaderSize(
    const ShaderCreateInfo& createInfo,
    Result*                 pResult
    ) const
{
    if (pResult != nullptr)
    {
        if (createInfo.pCode == nullptr)
        {
            *pResult = Result::ErrorInvalidPointer;
        }
        else if (createInfo.codeSize == 0)
        {
            *pResult = Result::ErrorInvalidValue;
        }
        else
        {
            *pResult = Result::Success;
        }
    }

    // NOTE: the shader object and its IL code are packed into the same memory allocation.
    return Shader::GetSize(*Parent(), createInfo, pResult);
}

// =====================================================================================================================
// Creates & initializes a new shader object.  Handles storage of the IL code buffer in the same allocation as the
// shader object itself on behalf of the caller.
Result GfxDevice::CreateShader(
    const ShaderCreateInfo& createInfo,
    void*                   pPlacementAddr,
    IShader**               ppShader)
{
    Shader* pShader = PAL_PLACEMENT_NEW(pPlacementAddr) Shader(*Parent());

    Result result = pShader->Init(createInfo);

    if (result != Result::Success)
    {
        pShader->Destroy();
    }
    else
    {
        *ppShader = pShader;
    }

    return result;
}

// =====================================================================================================================
size_t GfxDevice::GetShaderCacheSize() const
{
    constexpr ShaderCacheCreateInfo DummyCreateInfo = { };
    return ShaderCache::GetSize(*Parent(), DummyCreateInfo, nullptr);
}

// =====================================================================================================================
// Creates & initializes a new shader cache object.
Result GfxDevice::CreateShaderCache(
    const ShaderCacheCreateInfo& createInfo,
    void*                        pPlacementAddr,
    IShaderCache**               ppShaderCache)
{
    ShaderCache* pShaderCache = PAL_PLACEMENT_NEW(pPlacementAddr) ShaderCache(*Parent());

    Result result = pShaderCache->Init(createInfo, false);

    if (result != Result::Success)
    {
        pShaderCache->Destroy();
    }
    else
    {
        *ppShaderCache = static_cast<IShaderCache*>(pShaderCache);
    }

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
// Returns the Device object that owns this GFXIP-specific "sub device".
Pal::Device* GfxDevice::Parent() const
{
    return m_pParent;
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

} // Pal
