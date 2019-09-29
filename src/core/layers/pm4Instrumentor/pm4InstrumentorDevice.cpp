/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/layers/pm4Instrumentor/pm4InstrumentorCmdBuffer.h"
#include "core/layers/pm4Instrumentor/pm4InstrumentorDevice.h"
#include "core/layers/pm4Instrumentor/pm4InstrumentorPlatform.h"
#include "core/layers/pm4Instrumentor/pm4InstrumentorQueue.h"
#include "palSysUtil.h"

using namespace Util;

namespace Pal
{
namespace Pm4Instrumentor
{

// =====================================================================================================================
// Helper function for determining if a Queue type supports instrumentation through this layer.  Currently, only the
// Compute and Universal Queues are supported.
PAL_INLINE bool SupportsInstrumentation(
    QueueType queueType)
{
    return ((queueType == QueueTypeUniversal) || (queueType == QueueTypeCompute));
}

// =====================================================================================================================
Device::Device(
    PlatformDecorator* pPlatform,
    IDevice*           pNextDevice)
    :
    DeviceDecorator(pPlatform, pNextDevice),
    m_pPublicSettings(nullptr)
{
    memset(&m_deviceProperties, 0, sizeof(m_deviceProperties));
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

    return result;
}

// =====================================================================================================================
size_t Device::GetCmdBufferSize(
    const CmdBufferCreateInfo& createInfo,
    Result*                    pResult
    ) const
{
    const bool enableLayer = SupportsInstrumentation(createInfo.queueType);

    CmdBufferCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pCmdAllocator = NextCmdAllocator(createInfo.pCmdAllocator);

    return (m_pNextLayer->GetCmdBufferSize(nextCreateInfo, pResult) +
            (enableLayer ? sizeof(CmdBuffer) : sizeof(CmdBufferFwdDecorator)));
}

// =====================================================================================================================
Result Device::CreateCmdBuffer(
    const CmdBufferCreateInfo& createInfo,
    void*                      pPlacementAddr,
    ICmdBuffer**               ppCmdBuffer)
{
    const bool   enableLayer = SupportsInstrumentation(createInfo.queueType);
    const size_t offset      = (enableLayer ? sizeof(CmdBuffer) : sizeof(CmdBufferFwdDecorator));

    ICmdBuffer* pNextCmdBuffer = nullptr;
    ICmdBuffer* pCmdBuffer     = nullptr;

    CmdBufferCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pCmdAllocator = NextCmdAllocator(createInfo.pCmdAllocator);

    Result result = m_pNextLayer->CreateCmdBuffer(nextCreateInfo,
                                                  VoidPtrInc(pPlacementAddr, offset),
                                                  &pNextCmdBuffer);
    if (result == Result::Success)
    {
        PAL_ASSERT(pNextCmdBuffer != nullptr);

        if (enableLayer)
        {
            pCmdBuffer = PAL_PLACEMENT_NEW(pPlacementAddr) CmdBuffer(pNextCmdBuffer, this, createInfo);
        }
        else
        {
            pCmdBuffer = PAL_PLACEMENT_NEW(pPlacementAddr) CmdBufferFwdDecorator(pNextCmdBuffer, this);
        }
    }

    if (result == Result::Success)
    {
        pNextCmdBuffer->SetClientData(pPlacementAddr);
        (*ppCmdBuffer) = pCmdBuffer;
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetQueueSize(
    const QueueCreateInfo& createInfo,
    Result*                pResult
    ) const
{
    const bool enableLayer = SupportsInstrumentation(createInfo.queueType);

    return (m_pNextLayer->GetQueueSize(createInfo, pResult) +
            (enableLayer ? sizeof(Queue) : sizeof(QueueDecorator)));
}

// =====================================================================================================================
Result Device::CreateQueue(
    const QueueCreateInfo& createInfo,
    void*                  pPlacementAddr,
    IQueue**               ppQueue)
{
    const bool   enableLayer = SupportsInstrumentation(createInfo.queueType);
    const size_t offset      = (enableLayer ? sizeof(Queue) : sizeof(QueueDecorator));

    IQueue* pNextQueue = nullptr;
    IQueue* pQueue     = nullptr;

    Result result = m_pNextLayer->CreateQueue(createInfo,
                                              VoidPtrInc(pPlacementAddr, offset),
                                              &pNextQueue);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextQueue != nullptr);

        if (enableLayer)
        {
            pQueue = PAL_PLACEMENT_NEW(pPlacementAddr) Queue(pNextQueue, this);
            result = static_cast<Queue*>(pQueue)->Init();
        }
        else if (result == Result::Success)
        {
            pQueue = PAL_PLACEMENT_NEW(pPlacementAddr) QueueDecorator(pNextQueue, this);
        }
    }

    if (result == Result::Success)
    {
        pNextQueue->SetClientData(pPlacementAddr);
        (*ppQueue) = pQueue;
    }

    return result;
}

} // Pm4Instrumentor
} // Pal
