/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/layers/dbgOverlay/dbgOverlayCmdBuffer.h"
#include "core/layers/dbgOverlay/dbgOverlayDevice.h"
#include "core/layers/dbgOverlay/dbgOverlayFpsMgr.h"
#include "core/layers/dbgOverlay/dbgOverlayImage.h"
#include "core/layers/dbgOverlay/dbgOverlayPlatform.h"
#include "core/layers/dbgOverlay/dbgOverlayQueue.h"
#include "core/layers/dbgOverlay/dbgOverlayTextWriter.h"
#include "core/layers/dbgOverlay/dbgOverlayTimeGraph.h"
#include "core/g_palPlatformSettings.h"
#include "palAutoBuffer.h"
#include "palDequeImpl.h"

using namespace Util;

namespace Pal
{
namespace DbgOverlay
{

// =====================================================================================================================
Queue::Queue(
    IQueue*    pNextQueue,
    Device*    pDevice,
    uint32     queueCount)
    :
    QueueDecorator(pNextQueue, pDevice),
    m_pDevice(pDevice),
    m_queueCount(queueCount),
    m_pSubQueueInfos(nullptr),
    m_supportAnyTimestamp(false)
{
    PAL_ASSERT(m_queueCount > 0);
}

// =====================================================================================================================
Queue::~Queue()
{
    Platform*const pPlatform = static_cast<Platform*>(m_pDevice->GetPlatform());

    pPlatform->GetFpsMgr()->NotifyQueueDestroyed(this);

    for (uint32 qIdx = 0; qIdx < m_queueCount; qIdx++)
    {
        SubQueueInfo* pSubQueueInfo = &m_pSubQueueInfos[qIdx];
        PAL_ASSERT(pSubQueueInfo->pGpuTimestamps != nullptr);

        while (pSubQueueInfo->pGpuTimestamps->NumElements() > 0)
        {
            GpuTimestampPair* pTimestamp = nullptr;
            pSubQueueInfo->pGpuTimestamps->PopFront(&pTimestamp);
            DestroyGpuTimestampPair(pTimestamp);
        }
        PAL_SAFE_DELETE(pSubQueueInfo->pGpuTimestamps, pPlatform);

        if (pSubQueueInfo->pTimestampMemory != nullptr)
        {
            pSubQueueInfo->pTimestampMemory->Destroy();
            PAL_SAFE_FREE(pSubQueueInfo->pTimestampMemory, pPlatform);
        }
    }

    PAL_SAFE_DELETE_ARRAY(m_pSubQueueInfos, pPlatform);
}

// =====================================================================================================================
Result Queue::Init(
    const QueueCreateInfo* pCreateInfo)
{
    PAL_ASSERT(pCreateInfo != nullptr);

    Platform* pPlatform = static_cast<Platform*>(m_pDevice->GetPlatform());
    Result    result    = Result::Success;
    m_pSubQueueInfos = PAL_NEW_ARRAY(SubQueueInfo, m_queueCount, pPlatform, AllocInternal);

    if (m_pSubQueueInfos == nullptr)
    {
        result = Result::ErrorOutOfMemory;
    }
    const auto& engineProps = m_pDevice->GpuProps().engineProperties;

    if (result == Result::Success)
    {
        memset(&m_pSubQueueInfos[0], 0, sizeof(SubQueueInfo) * m_queueCount);
    }

    for (uint32 i = 0; ((result == Result::Success) && (i < m_queueCount)); ++i)
    {
        const auto& subQueueEngineProps = engineProps[pCreateInfo[i].engineType];

        m_pSubQueueInfos[i].engineType          = pCreateInfo[i].engineType;
        m_pSubQueueInfos[i].engineIndex         = pCreateInfo[i].engineIndex;
        m_pSubQueueInfos[i].queueType           = pCreateInfo[i].queueType;
        m_pSubQueueInfos[i].supportTimestamps   = subQueueEngineProps.flags.supportsTimestamps;
        m_pSubQueueInfos[i].timestampAlignment  = subQueueEngineProps.minTimestampAlignment;
        m_pSubQueueInfos[i].timestampMemorySize =
            2 * MaxGpuTimestampPairCount * subQueueEngineProps.minTimestampAlignment;

        m_pSubQueueInfos[i].pGpuTimestamps = PAL_NEW(GpuTimestampDeque, pPlatform, AllocInternal)(pPlatform);
        if (m_pSubQueueInfos[i].pGpuTimestamps == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }

        if (result == Result::Success)
        {
            m_supportAnyTimestamp |= m_pSubQueueInfos[i].supportTimestamps;

            if (m_pSubQueueInfos[i].supportTimestamps)
            {
                result = CreateGpuTimestampPairMemory(&m_pSubQueueInfos[i]);
            }
        }
    }

    return result;
}

// =====================================================================================================================
Result Queue::CreateCmdBuffer(
    const CmdBufferCreateInfo& createInfo,
    ICmdBuffer**               ppCmdBuffer)
{
    Result result = Result::ErrorOutOfMemory;

    void*const pMemory = PAL_MALLOC(m_pDevice->GetCmdBufferSize(createInfo, nullptr),
                                    m_pDevice->GetPlatform(),
                                    AllocInternal);

    if (pMemory != nullptr)
    {
        result = m_pDevice->CreateCmdBuffer(createInfo, pMemory, ppCmdBuffer);
    }

    return result;
}

// =====================================================================================================================
Result Queue::CreateFence(
    const FenceCreateInfo& createInfo,
    IFence**               ppFence)
{
    Result result = Result::ErrorOutOfMemory;

    void*const pMemory = PAL_MALLOC(m_pDevice->GetFenceSize(nullptr), m_pDevice->GetPlatform(), AllocInternal);

    if (pMemory != nullptr)
    {
        result = m_pDevice->CreateFence(createInfo, pMemory, ppFence);
    }

    return result;
}

// =====================================================================================================================
// Allocates Gpu Memory for GpuTimestampPair structs
Result Queue::CreateGpuTimestampPairMemory(
    SubQueueInfo* pSubQueueInfo)
{
    Result result = Result::Success;

    GpuMemoryCreateInfo gpuMemoryCreateInfo = {};

    gpuMemoryCreateInfo.size           = pSubQueueInfo->timestampMemorySize;
    gpuMemoryCreateInfo.vaRange        = VaRange::Default;
    gpuMemoryCreateInfo.heapCount      = 1;
    gpuMemoryCreateInfo.priority       = GpuMemPriority::Normal;
    gpuMemoryCreateInfo.priorityOffset = GpuMemPriorityOffset::Offset0;
    gpuMemoryCreateInfo.heaps[0]       = GpuHeapGartUswc;

    void*const pMemory = PAL_MALLOC(m_pDevice->GetGpuMemorySize(gpuMemoryCreateInfo, nullptr),
                                    m_pDevice->GetPlatform(),
                                    AllocInternal);

    if (pMemory != nullptr)
    {
        result = m_pDevice->CreateGpuMemory(gpuMemoryCreateInfo, pMemory, &pSubQueueInfo->pTimestampMemory);
    }
    else
    {
        result = Result::ErrorOutOfMemory;
    }

    GpuMemoryRef gpuMemoryRef = {};
    gpuMemoryRef.pGpuMemory = pSubQueueInfo->pTimestampMemory;

    if (result == Result::Success)
    {
        result = m_pDevice->AddGpuMemoryReferences(1, &gpuMemoryRef, this, GpuMemoryRefCantTrim);
    }

    if (result == Result::Success)
    {
        result = pSubQueueInfo->pTimestampMemory->Map(&pSubQueueInfo->pMappedTimestampData);
    }

    return result;
}

// =====================================================================================================================
Result Queue::PresentDirect(
    const PresentDirectInfo& presentInfo)
{
    Result result = Result::Success;

    const Result presentResult = QueueDecorator::PresentDirect(presentInfo);
    result = CollapseResults(presentResult, result);

    if (result == Result::Success)
    {
        Platform*const pPlatform = static_cast<Platform*>(m_pDevice->GetPlatform());

        pPlatform->GetFpsMgr()->IncrementFrameCount();
        pPlatform->ResetGpuWork();
    }

    return result;
}

// =====================================================================================================================
Result Queue::PresentSwapChain(
    const PresentSwapChainInfo& presentInfo)
{
    Result result = Result::Success;

    // Note: We must always call down to the next layer because we must release ownership of the image index.
    const Result presentResult = QueueDecorator::PresentSwapChain(presentInfo);
    result = CollapseResults(presentResult, result);

    if (result == Result::Success)
    {
        Platform*const pPlatform = static_cast<Platform*>(m_pDevice->GetPlatform());

        pPlatform->GetFpsMgr()->IncrementFrameCount();
        pPlatform->ResetGpuWork();
    }

    return result;
}

// =====================================================================================================================
Result Queue::Submit(
    const MultiSubmitInfo& submitInfo)
{
    PAL_ASSERT_MSG((submitInfo.perSubQueueInfoCount <= 1),
                   "Multi-Queue support has not yet been tested in DbgOverlay!");
    PAL_ASSERT((submitInfo.perSubQueueInfoCount <= 1) || (submitInfo.perSubQueueInfoCount == m_queueCount));

    const auto& gpuProps = m_pDevice->GpuProps();
    Platform* pPlatform = static_cast<Platform*>(m_pDevice->GetPlatform());
    pPlatform->SetGpuWork(gpuProps.gpuIndex, true);

    // Determine if we should add timestamps to this submission.
    bool addTimestamps = m_supportAnyTimestamp                    &&
                         (submitInfo.pPerSubQueueInfo != nullptr) &&
                         (submitInfo.pPerSubQueueInfo[0].cmdBufferCount > 0);
    if (addTimestamps)
    {
        // Other PAL layers assume that CmdPresent can only be in the last command buffer in a submission.
        // If we were to timestamp submissions with presents we would break those layers.
        // We don't timestamp IQueue's present calls either so this should be OK.
        auto*const pLastCmdBuffer = static_cast<CmdBuffer*>(
            submitInfo.pPerSubQueueInfo[0].ppCmdBuffers[submitInfo.pPerSubQueueInfo[0].cmdBufferCount - 1]);

        addTimestamps = (pLastCmdBuffer->ContainsPresent() == false);
    }

    Result result = Result::Success;

    if (addTimestamps)
    {
        // Try to reuse an existing GpuTimestampPair, otherwise create a new one if we still have space for it.
        AutoBuffer<GpuTimestampPair*, 8, Platform> gpuTimestamps(submitInfo.perSubQueueInfoCount, pPlatform);

        if (gpuTimestamps.Capacity() >= submitInfo.perSubQueueInfoCount)
        {
            memset(gpuTimestamps.Data(), 0, gpuTimestamps.SizeBytes());

            uint32 i = 0;
            for (; i < submitInfo.perSubQueueInfoCount; ++i)
            {
                SubQueueInfo*     pSubQueueInfo = &m_pSubQueueInfos[i];
                GpuTimestampPair* pTimestamp    = nullptr;

                if (pSubQueueInfo->supportTimestamps)
                {
                    if ((pSubQueueInfo->pGpuTimestamps->NumElements() > 0) &&
                        (pSubQueueInfo->pGpuTimestamps->Front()->numActiveSubmissions == 0))
                    {
                        result = pSubQueueInfo->pGpuTimestamps->PopFront(&pTimestamp);

                        if (result == Result::Success)
                        {
                            result = m_pDevice->ResetFences(1, &pTimestamp->pFence);
                        }
                    }
                    else if (pSubQueueInfo->nextTimestampOffset < pSubQueueInfo->timestampMemorySize)
                    {
                        result = CreateGpuTimestampPair(pSubQueueInfo, &pTimestamp);
                    }

                    // Immediately push it onto the back of the deque to avoid leaking memory if something fails.
                    if (pTimestamp != nullptr)
                    {
                        // The timestamp should be null if any error occured.
                        PAL_ASSERT(result == Result::Success);

                        result = pSubQueueInfo->pGpuTimestamps->PushBack(pTimestamp);

                        if (result != Result::Success)
                        {
                            // We failed to push the timestamp onto the deque. To avoid leaking memory we must delete it.
                            DestroyGpuTimestampPair(pTimestamp);
                            pTimestamp    = nullptr;
                            addTimestamps = false;
                            break;
                        }

                        gpuTimestamps[i] = pTimestamp;
                    }
                    else
                    {
                        addTimestamps = false;
                        break;
                    }
                }
            }

            if (addTimestamps == false)
            {
                for (i = 0; ((result == Result::Success) && (i < submitInfo.perSubQueueInfoCount)); i++)
                {
                    if (gpuTimestamps[i] != nullptr)
                    {
                        SubQueueInfo* pSubQueueInfo = &m_pSubQueueInfos[i];
                        result =  pSubQueueInfo->pGpuTimestamps->PushFront(gpuTimestamps[i]);
                    }
                }
            }
        }
        else
        {
            result        = Result::ErrorOutOfMemory;
            addTimestamps = false;
        }

        // Submit to the next layer. We should do this even if a failure occured to avoid crashing the application.
        if ((result == Result::Success) && addTimestamps)
        {
            // The timestamp should be null if any error occured.
            PAL_ASSERT(result == Result::Success);

            result = SubmitWithGpuTimestampPair(submitInfo, gpuTimestamps.Data());
        }
        else
        {
            const Result submitResult = QueueDecorator::Submit(submitInfo);
            result = CollapseResults(submitResult, result);

            // Notify the FPS manager that we failed to timestamp this submit (the overlay text will reflect this).
            pPlatform->GetFpsMgr()->NotifySubmitWithoutTimestamp();
        }
    }
    else
    {
        result = QueueDecorator::Submit(submitInfo);
    }

    return result;
}

// =====================================================================================================================
Result Queue::SubmitWithGpuTimestampPair(
    const MultiSubmitInfo& submitInfo,
    GpuTimestampPair**     ppTimestamp)
{
    // Caller should have made sure that there was at least one command buffer in here.
    PAL_ASSERT((submitInfo.pPerSubQueueInfo != nullptr) && (submitInfo.pPerSubQueueInfo[0].cmdBufferCount > 0));
    Result result = Result::Success;

    Platform* pPlatform = static_cast<Platform*>(m_pDevice->GetPlatform());

    uint32 cmdBufferCapacity = submitInfo.perSubQueueInfoCount * 2;

    for (uint32 i = 0; i < submitInfo.perSubQueueInfoCount; i++)
    {
        cmdBufferCapacity += submitInfo.pPerSubQueueInfo[i].cmdBufferCount;
    }

    AutoBuffer<PerSubQueueSubmitInfo, 16,  PlatformDecorator> perSubQueueInfos(submitInfo.perSubQueueInfoCount,
                                                                               pPlatform);
    AutoBuffer<ICmdBuffer*,           256, PlatformDecorator> cmdBuffers(cmdBufferCapacity,     pPlatform);
    AutoBuffer<CmdBufInfo,            256, PlatformDecorator> cmdBufInfoList(cmdBufferCapacity, pPlatform);

    if ((perSubQueueInfos.Capacity() < submitInfo.perSubQueueInfoCount) ||
        (cmdBuffers.Capacity()       < cmdBufferCapacity)               ||
        (cmdBufInfoList.Capacity()   < cmdBufferCapacity))
    {
        result = Result::ErrorOutOfMemory;
    }
    else
    {
        uint32 cmdBufferIndex  = 0;
        uint32 cmdBufInfoIndex = 0;

        memset(cmdBufInfoList.Data(), 0, cmdBufInfoList.SizeBytes());

        for (uint32 queueIdx = 0; queueIdx < submitInfo.perSubQueueInfoCount; ++queueIdx)
        {
            const PerSubQueueSubmitInfo& origPerSubQueueInfo = submitInfo.pPerSubQueueInfo[queueIdx];
            PerSubQueueSubmitInfo*       pNewPerSubQueueInfo = &perSubQueueInfos[queueIdx];

            const bool hasCmdBufInfo      = (origPerSubQueueInfo.pCmdBufInfoList != nullptr);
            const bool supportsTimestamps = m_pSubQueueInfos[queueIdx].supportTimestamps;

            (*pNewPerSubQueueInfo) = origPerSubQueueInfo;

            if (pNewPerSubQueueInfo->cmdBufferCount > 0)
            {
                GpuTimestampPair* pTimestamp = ppTimestamp[queueIdx];
                PAL_ASSERT((pTimestamp != nullptr) || (supportsTimestamps == false));

                pNewPerSubQueueInfo->cmdBufferCount += (supportsTimestamps) ? 2 : 0;
                pNewPerSubQueueInfo->ppCmdBuffers    = &cmdBuffers[cmdBufferIndex];
                pNewPerSubQueueInfo->pCmdBufInfoList = (hasCmdBufInfo) ? &cmdBufInfoList[cmdBufInfoIndex] : nullptr;

                if (supportsTimestamps)
                {
                    cmdBuffers[cmdBufferIndex++] = pTimestamp->pBeginCmdBuffer;
                    cmdBufInfoIndex += (hasCmdBufInfo) ? 1 : 0;
                }

                for (uint32 cmdBufIdx = 0; cmdBufIdx < origPerSubQueueInfo.cmdBufferCount; ++cmdBufIdx)
                {
                    cmdBuffers[cmdBufferIndex++] = origPerSubQueueInfo.ppCmdBuffers[cmdBufIdx];

                    if (hasCmdBufInfo)
                    {
                        cmdBufInfoList[cmdBufInfoIndex++] = origPerSubQueueInfo.pCmdBufInfoList[cmdBufIdx];
                    }
                }

                if (supportsTimestamps)
                {
                    cmdBuffers[cmdBufferIndex++] = pTimestamp->pEndCmdBuffer;
                    cmdBufInfoIndex += (hasCmdBufInfo) ? 1 : 0;
                }
            }

            PAL_ASSERT(cmdBufferIndex  <= cmdBufferCapacity);
            PAL_ASSERT(cmdBufInfoIndex <= cmdBufferCapacity);
        }

        MultiSubmitInfo finalSubmitInfo  = submitInfo;
        finalSubmitInfo.pPerSubQueueInfo = perSubQueueInfos.Data();

        result = QueueDecorator::Submit(finalSubmitInfo);

        for (uint32 i = 0; ((result == Result::Success) && (i < submitInfo.perSubQueueInfoCount)); ++i)
        {
            if (ppTimestamp[i] != nullptr)
            {
                result = AssociateFenceWithLastSubmit(ppTimestamp[i]->pFence);
            }
        }

        if (result == Result::Success)
        {
            pPlatform->GetFpsMgr()->UpdateSubmitTimelist(submitInfo.perSubQueueInfoCount, ppTimestamp);
        }
    }
    return result;
}

// =====================================================================================================================
// Creates and initializes a new GpuTimestampPair
Result Queue::CreateGpuTimestampPair(
    SubQueueInfo*       pSubQueueInfo,
     GpuTimestampPair** ppTimestamp)
{
    Result result = Result::Success;

    GpuTimestampPair* pTimestamp = PAL_NEW(GpuTimestampPair, m_pDevice->GetPlatform(), AllocInternal);

    if (pTimestamp == nullptr)
    {
        result = Result::ErrorOutOfMemory;
    }

    if (result == Result::Success)
    {
        memset(pTimestamp, 0, sizeof(*pTimestamp));

        pTimestamp->pOwner             = this;
        pTimestamp->timestampFrequency = m_pDevice->GpuProps().timestampFrequency;
        Pal::FenceCreateInfo createInfo = {};
        result = CreateFence(createInfo, &pTimestamp->pFence);
    }

    if (result == Result::Success)
    {
        CmdBufferCreateInfo beginCmdBufferCreateInfo = {};
        beginCmdBufferCreateInfo.pCmdAllocator = m_pDevice->InternalCmdAllocator();
        beginCmdBufferCreateInfo.queueType     = pSubQueueInfo->queueType;
        beginCmdBufferCreateInfo.engineType    = pSubQueueInfo->engineType;

        result = CreateCmdBuffer(beginCmdBufferCreateInfo, &pTimestamp->pBeginCmdBuffer);
    }

    if (result == Result::Success)
    {
        CmdBufferCreateInfo endCmdBufferCreateInfo = {};
        endCmdBufferCreateInfo.pCmdAllocator = m_pDevice->InternalCmdAllocator();
        endCmdBufferCreateInfo.queueType     = pSubQueueInfo->queueType;
        endCmdBufferCreateInfo.engineType    = pSubQueueInfo->engineType;

        result = CreateCmdBuffer(endCmdBufferCreateInfo, &pTimestamp->pEndCmdBuffer);
    }

    if (result == Result::Success)
    {
        CmdBufferBuildInfo cmdBufferBuildInfo = {};
        cmdBufferBuildInfo.flags.optimizeExclusiveSubmit = 1;

        result = pTimestamp->pBeginCmdBuffer->Begin(NextCmdBufferBuildInfo(cmdBufferBuildInfo));
    }

    if (result == Result::Success)
    {
        pTimestamp->pBeginCmdBuffer->CmdWriteTimestamp(HwPipeBottom,
                                                       *pSubQueueInfo->pTimestampMemory,
                                                       pSubQueueInfo->nextTimestampOffset);
        result = pTimestamp->pBeginCmdBuffer->End();
    }

    if (result == Result::Success)
    {
        pTimestamp->pBeginTimestamp = static_cast<uint64*>(Util::VoidPtrInc(pSubQueueInfo->pMappedTimestampData,
                                                                            pSubQueueInfo->nextTimestampOffset));
        pSubQueueInfo->nextTimestampOffset += pSubQueueInfo->timestampAlignment;

        CmdBufferBuildInfo cmdBufferBuildInfo = {};
        cmdBufferBuildInfo.flags.optimizeExclusiveSubmit = 1;

        result = pTimestamp->pEndCmdBuffer->Begin(NextCmdBufferBuildInfo(cmdBufferBuildInfo));
    }

    if (result == Result::Success)
    {
        pTimestamp->pEndCmdBuffer->CmdWriteTimestamp(HwPipeBottom,
                                                     *pSubQueueInfo->pTimestampMemory,
                                                     pSubQueueInfo->nextTimestampOffset);
        result = pTimestamp->pEndCmdBuffer->End();
    }

    if (result == Result::Success)
    {
        pTimestamp->pEndTimestamp = static_cast<uint64*>(Util::VoidPtrInc(pSubQueueInfo->pMappedTimestampData,
                                                                          pSubQueueInfo->nextTimestampOffset));
        pSubQueueInfo->nextTimestampOffset += pSubQueueInfo->timestampAlignment;
    }

    if (result == Result::Success)
    {
        *ppTimestamp = pTimestamp;
    }
    else
    {
        DestroyGpuTimestampPair(pTimestamp);
    }

    return result;
}

// =====================================================================================================================
void Queue::DestroyGpuTimestampPair(
    GpuTimestampPair* pTimestamp)
{
    if (pTimestamp->pBeginCmdBuffer != nullptr)
    {
        pTimestamp->pBeginCmdBuffer->Destroy();
        PAL_SAFE_FREE(pTimestamp->pBeginCmdBuffer, m_pDevice->GetPlatform());
    }

    if (pTimestamp->pEndCmdBuffer != nullptr)
    {
        pTimestamp->pEndCmdBuffer->Destroy();
        PAL_SAFE_FREE(pTimestamp->pEndCmdBuffer, m_pDevice->GetPlatform());
    }

    if (pTimestamp->pFence != nullptr)
    {
        pTimestamp->pFence->Destroy();
        PAL_SAFE_FREE(pTimestamp->pFence, m_pDevice->GetPlatform());
    }

    PAL_SAFE_DELETE(pTimestamp, m_pDevice->GetPlatform());
}

} // DbgOverlay
} // Pal
