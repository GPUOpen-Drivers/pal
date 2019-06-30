/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/layers/shaderDbg/shaderDbgCmdBuffer.h"
#include "core/layers/shaderDbg/shaderDbgDevice.h"
#include "core/layers/shaderDbg/shaderDbgPipeline.h"
#include "core/layers/shaderDbg/shaderDbgPlatform.h"
#include "core/layers/shaderDbg/shaderDbgQueue.h"
#include "core/g_palPlatformSettings.h"
#include "palListImpl.h"
#include "palSysUtil.h"

using namespace Util;

namespace Pal
{
namespace ShaderDbg
{

// =====================================================================================================================
Device::Device(
    PlatformDecorator* pPlatform,
    IDevice*           pNextDevice)
    :
    DeviceDecorator(pPlatform, pNextDevice),
    m_pPublicSettings(nullptr),
    m_initialized(false),
    m_gpuMemoryLock(),
    m_freeGpuMemory(pPlatform),
    m_usedGpuMemory(pPlatform)
{
    memset(&m_deviceProperties, 0, sizeof(m_deviceProperties));
}

// =====================================================================================================================
Device::~Device()
{
}

// =====================================================================================================================
Result Device::CommitSettingsAndInit()
{
    Result result = DeviceDecorator::CommitSettingsAndInit();

    m_pPublicSettings = GetNextLayer()->GetPublicSettings();

    return result;
}

// =====================================================================================================================
Result Device::Finalize(
    const DeviceFinalizeInfo& finalizeInfo)
{
    Result result = DeviceDecorator::Finalize(finalizeInfo);

    if (result == Result::Success)
    {
        result = GetProperties(&m_deviceProperties);
    }

    if (result == Result::Success)
    {
        result = m_gpuMemoryLock.Init();
    }

    if (result == Result::Success)
    {
        m_initialized = true;
    }

    return result;
}

// =====================================================================================================================
Result Device::Cleanup()
{
    if (m_initialized)
    {
        Util::MutexAuto lock(&m_gpuMemoryLock);
        if (m_usedGpuMemory.NumElements() != 0)
        {
            auto it = m_usedGpuMemory.Begin();

            while (it.Get() != nullptr)
            {
                IGpuMemory* pUsed = (*it.Get());
                pUsed->Destroy();
                PAL_SAFE_FREE(pUsed, m_pPlatform);

                m_usedGpuMemory.Erase(&it);
            }
        }

        if (m_freeGpuMemory.NumElements() != 0)
        {
            auto it = m_freeGpuMemory.Begin();

            while (it.Get() != nullptr)
            {
                IGpuMemory* pFree = (*it.Get());
                pFree->Destroy();
                PAL_SAFE_FREE(pFree, m_pPlatform);

                m_freeGpuMemory.Erase(&it);
            }
        }
    }

    return DeviceDecorator::Cleanup();
}

// =====================================================================================================================
size_t Device::GetCmdBufferSize(
    const CmdBufferCreateInfo& createInfo,
    Result*                    pResult
    ) const
{
    CmdBufferCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pCmdAllocator = NextCmdAllocator(createInfo.pCmdAllocator);

    return m_pNextLayer->GetCmdBufferSize(nextCreateInfo, pResult) + sizeof(CmdBuffer);
}

// =====================================================================================================================
Result Device::CreateCmdBuffer(
    const CmdBufferCreateInfo& createInfo,
    void*                      pPlacementAddr,
    ICmdBuffer**               ppCmdBuffer)
{
    ICmdBuffer* pNextCmdBuffer = nullptr;
    ICmdBuffer* pCmdBuffer     = nullptr;

    CmdBufferCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pCmdAllocator = NextCmdAllocator(createInfo.pCmdAllocator);

    Result result = m_pNextLayer->CreateCmdBuffer(nextCreateInfo,
                                                  VoidPtrInc(pPlacementAddr, sizeof(CmdBuffer)),
                                                  &pNextCmdBuffer);

    // The only command buffers which accept shaders are compute and universal ones.
    if (result == Result::Success)
    {
        PAL_ASSERT(pNextCmdBuffer != nullptr);

        pCmdBuffer = PAL_PLACEMENT_NEW(pPlacementAddr) CmdBuffer(pNextCmdBuffer, this, createInfo);
        pNextCmdBuffer->SetClientData(pPlacementAddr);
        (*ppCmdBuffer) = pCmdBuffer;
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetGraphicsPipelineSize(
    const GraphicsPipelineCreateInfo& createInfo,
    Result*                           pResult
) const
{
    return m_pNextLayer->GetGraphicsPipelineSize(createInfo, pResult) + sizeof(Pipeline);
}

// =====================================================================================================================
Result Device::CreateGraphicsPipeline(
    const GraphicsPipelineCreateInfo& createInfo,
    void*                             pPlacementAddr,
    IPipeline**                       ppPipeline)
{
    IPipeline* pNextPipeline = nullptr;
    Pipeline* pPipeline = nullptr;

    Result result = m_pNextLayer->CreateGraphicsPipeline(createInfo,
        NextObjectAddr<Pipeline>(pPlacementAddr),
        &pNextPipeline);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextPipeline != nullptr);
        pNextPipeline->SetClientData(pPlacementAddr);

        pPipeline = PAL_PLACEMENT_NEW(pPlacementAddr) Pipeline(pNextPipeline, this);
        result = pPipeline->Init(createInfo.pPipelineBinary, createInfo.pipelineBinarySize);
    }

    if (result == Result::Success)
    {
        (*ppPipeline) = pPipeline;
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetComputePipelineSize(
    const ComputePipelineCreateInfo& createInfo,
    Result*                          pResult
) const
{
    return m_pNextLayer->GetComputePipelineSize(createInfo, pResult) + sizeof(Pipeline);
}

// =====================================================================================================================
Result Device::CreateComputePipeline(
    const ComputePipelineCreateInfo& createInfo,
    void*                            pPlacementAddr,
    IPipeline**                      ppPipeline)
{
    IPipeline* pNextPipeline = nullptr;
    Pipeline* pPipeline = nullptr;

    Result result = m_pNextLayer->CreateComputePipeline(createInfo,
        NextObjectAddr<Pipeline>(pPlacementAddr),
        &pNextPipeline);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextPipeline != nullptr);
        pNextPipeline->SetClientData(pPlacementAddr);

        pPipeline = PAL_PLACEMENT_NEW(pPlacementAddr) Pipeline(pNextPipeline, this);
        result = pPipeline->Init(createInfo.pPipelineBinary, createInfo.pipelineBinarySize);
    }

    if (result == Result::Success)
    {
        (*ppPipeline) = pPipeline;
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetQueueSize(
    const QueueCreateInfo& createInfo,
    Result*                pResult
) const
{
    return m_pNextLayer->GetQueueSize(createInfo, pResult) + sizeof(Queue);
}

// =====================================================================================================================
Result Device::CreateQueue(
    const QueueCreateInfo& createInfo,
    void*                  pPlacementAddr,
    IQueue**               ppQueue)
{
    IQueue* pNextQueue = nullptr;
    Queue*  pQueue = nullptr;

    Result result = m_pNextLayer->CreateQueue(createInfo,
                                              NextObjectAddr<Queue>(pPlacementAddr),
                                              &pNextQueue);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextQueue != nullptr);
        pNextQueue->SetClientData(pPlacementAddr);

        pQueue = PAL_PLACEMENT_NEW(pPlacementAddr) Queue(pNextQueue, this);

        result = pQueue->Init();
    }

    if (result == Result::Success)
    {
        (*ppQueue) = pQueue;
    }

    return result;
}

// =====================================================================================================================
Result Device::GetMemoryChunk(
    IGpuMemory** ppGpuMemory)
{
    IGpuMemory* pGpuMemory = nullptr;

    {
        Util::MutexAuto lock(&m_gpuMemoryLock);

        if (m_freeGpuMemory.NumElements() > 0)
        {
            pGpuMemory = (*m_freeGpuMemory.Begin().Get());
            m_freeGpuMemory.Erase(&m_freeGpuMemory.Begin());
        }
    }

    Result result = Result::Success;
    if (pGpuMemory == nullptr)
    {
        GpuMemoryCreateInfo createInfo = {};
        createInfo.size           = GetPlatform()->PlatformSettings().shaderDbgConfig.shaderDbgChunkSize;
        createInfo.alignment      = 0;
        createInfo.vaRange        = VaRange::Default;
        createInfo.heaps[0]       = GpuHeapLocal;
        createInfo.heaps[1]       = GpuHeapGartUswc;
        createInfo.heapCount      = 2;
        createInfo.priority       = GpuMemPriority::Normal;
        createInfo.priorityOffset = GpuMemPriorityOffset::Offset0;

        pGpuMemory = static_cast<IGpuMemory*>(PAL_MALLOC(m_pNextLayer->GetGpuMemorySize(createInfo, &result),
                                                         m_pPlatform,
                                                         AllocInternal));

        if ((result == Result::Success) && (pGpuMemory != nullptr))
        {
            result = m_pNextLayer->CreateGpuMemory(createInfo, pGpuMemory, &pGpuMemory);
        }
    }

    PAL_ASSERT(pGpuMemory != nullptr);

    if (result == Result::Success)
    {
        // Permanently add a reference to this memory chunk to the device.
        GpuMemoryRef memRef = {};
        memRef.pGpuMemory   = pGpuMemory;
        result = m_pNextLayer->AddGpuMemoryReferences(1, &memRef, nullptr, GpuMemoryRefCantTrim);
    }

    if (result == Result::Success)
    {
        Util::MutexAuto lock(&m_gpuMemoryLock);
        result = m_usedGpuMemory.PushBack(pGpuMemory);

        (*ppGpuMemory) = pGpuMemory;
    }

    return result;
}

// =====================================================================================================================
Result Device::ReleaseMemoryChunk(
    IGpuMemory* pGpuMemory)
{
    Result result = Result::Success;

    if (pGpuMemory != nullptr)
    {
        Util::MutexAuto lock(&m_gpuMemoryLock);
        if (m_usedGpuMemory.NumElements() != 0)
        {
            auto it = m_usedGpuMemory.Begin();

            while (it.Get() != nullptr)
            {
                const IGpuMemory* pUsed = (*it.Get());
                if (pUsed->Desc().gpuVirtAddr == pGpuMemory->Desc().gpuVirtAddr)
                {
                    m_usedGpuMemory.Erase(&it);
                    break;
                }
                it.Next();
            }
        }

        result = m_freeGpuMemory.PushBack(pGpuMemory);
    }

    return result;
}

} // ShaderDbg
} // Pal
