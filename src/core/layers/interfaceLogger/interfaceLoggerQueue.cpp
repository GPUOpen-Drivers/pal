/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_DEVELOPER_BUILD

#include "core/layers/interfaceLogger/interfaceLoggerCmdBuffer.h"
#include "core/layers/interfaceLogger/interfaceLoggerDevice.h"
#include "core/layers/interfaceLogger/interfaceLoggerFence.h"
#include "core/layers/interfaceLogger/interfaceLoggerGpuMemory.h"
#include "core/layers/interfaceLogger/interfaceLoggerImage.h"
#include "core/layers/interfaceLogger/interfaceLoggerPlatform.h"
#include "core/layers/interfaceLogger/interfaceLoggerQueue.h"
#include "core/layers/interfaceLogger/interfaceLoggerQueueSemaphore.h"
#include "core/layers/interfaceLogger/interfaceLoggerSwapChain.h"

using namespace Util;
using namespace std::chrono;

namespace Pal
{
namespace InterfaceLogger
{

// =====================================================================================================================
Queue::Queue(
    IQueue*  pNextQueue,
    Device*  pDevice,
    uint32   objectId)
    :
    QueueDecorator(pNextQueue, pDevice),
    m_pPlatform(static_cast<Platform*>(pDevice->GetPlatform())),
    m_objectId(objectId)
{
}

// =====================================================================================================================
Result Queue::Submit(
    const MultiSubmitInfo& submitInfo)
{
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::QueueSubmit);
    const Result result = QueueDecorator::Submit(submitInfo);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndBeginMap("submitInfo", false);

        pLogContext->KeyAndBeginList("perSubQueueInfos", false);
        for (uint32 queueIdx = 0; queueIdx < submitInfo.perSubQueueInfoCount; queueIdx++)
        {
            pLogContext->Struct(submitInfo.pPerSubQueueInfo[queueIdx]);
        }
        pLogContext->EndList();

        pLogContext->KeyAndBeginList("gpuMemoryRefs", false);
        for (uint32 idx = 0; idx < submitInfo.gpuMemRefCount; ++idx)
        {
            pLogContext->Struct(submitInfo.pGpuMemoryRefs[idx]);
        }
        pLogContext->EndList();

        pLogContext->KeyAndBeginList("doppRefs", false);
        for (uint32 idx = 0; idx < submitInfo.doppRefCount; ++idx)
        {
            pLogContext->Struct(submitInfo.pDoppRefs[idx]);
        }
        pLogContext->EndList();

        pLogContext->KeyAndBeginList("blockIfFlipping", false);
        for (uint32 idx = 0; idx < submitInfo.blockIfFlippingCount; ++idx)
        {
            pLogContext->Object(submitInfo.ppBlockIfFlipping[idx]);
        }
        pLogContext->EndList();

        pLogContext->KeyAndBeginList("fences", false);

        for (uint32 idx = 0; idx < submitInfo.fenceCount; ++idx)
        {
            pLogContext->Object(submitInfo.ppFences[idx]);
        }

        pLogContext->EndList();

        pLogContext->KeyAndValue("stackSizeInDwords", submitInfo.stackSizeInDwords);

        pLogContext->KeyAndObject("freeMuxMemory", submitInfo.pFreeMuxMemory);

        pLogContext->EndMap();
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Queue::WaitIdle()
{
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::QueueWaitIdle);
    const Result result = QueueDecorator::WaitIdle();

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Queue::SignalQueueSemaphore(
    IQueueSemaphore* pQueueSemaphore,
    uint64           value)
{
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::QueueSignalQueueSemaphore);
    const Result result = QueueDecorator::SignalQueueSemaphore(pQueueSemaphore, value);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("queueSemaphore", pQueueSemaphore);
        pLogContext->KeyAndValue("value", value);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Queue::WaitQueueSemaphore(
    IQueueSemaphore* pQueueSemaphore,
    uint64           value)
{
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::QueueWaitQueueSemaphore);
    const Result result = QueueDecorator::WaitQueueSemaphore(pQueueSemaphore, value);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("queueSemaphore", pQueueSemaphore);
        pLogContext->KeyAndValue("value", value);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Queue::PresentDirect(
    const PresentDirectInfo& presentInfo)
{
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::QueuePresentDirect);
    const Result result = QueueDecorator::PresentDirect(presentInfo);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("presentInfo", presentInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    m_pPlatform->UpdatePresentState();

    return result;
}

// =====================================================================================================================
Result Queue::PresentSwapChain(
    const PresentSwapChainInfo& presentInfo)
{
    // Note: We must always call down to the next layer because we must release ownership of the image index.
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::QueuePresentSwapChain);
    const Result result = QueueDecorator::PresentSwapChain(presentInfo);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("presentInfo", presentInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    m_pPlatform->UpdatePresentState();

    return result;
}

// =====================================================================================================================
Result Queue::Delay(
    Util::fmilliseconds delay)
{
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::QueueDelay);
    const Result result = QueueDecorator::Delay(delay);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndValue("delay", delay.count());
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Queue::DelayAfterVsync(
    Util::fmicroseconds   delay,
    const IPrivateScreen* pScreen)
{
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::QueueDelayAfterVsync);
    const Result result = QueueDecorator::DelayAfterVsync(delay, pScreen);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndValue("delay", delay.count());
        pLogContext->KeyAndObject("screen", pScreen);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Queue::RemapVirtualMemoryPages(
    uint32                         rangeCount,
    const VirtualMemoryRemapRange* pRanges,
    bool                           doNotWait,
    IFence*                        pFence)
{
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::QueueRemapVirtualMemoryPages);
    const Result result = QueueDecorator::RemapVirtualMemoryPages(rangeCount, pRanges, doNotWait, pFence);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndBeginList("ranges", false);

        for (uint32 idx = 0; idx < rangeCount; ++idx)
        {
            pLogContext->Struct(pRanges[idx]);
        }

        pLogContext->EndList();
        pLogContext->KeyAndValue("doNotWait", doNotWait);
        pLogContext->KeyAndObject("fence", pFence);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Queue::CopyVirtualMemoryPageMappings(
    uint32                                    rangeCount,
    const VirtualMemoryCopyPageMappingsRange* pRanges,
    bool                                      doNotWait)
{
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::QueueCopyVirtualMemoryPageMappings);
    const Result result = QueueDecorator::CopyVirtualMemoryPageMappings(rangeCount, pRanges, doNotWait);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndBeginList("ranges", false);

        for (uint32 idx = 0; idx < rangeCount; ++idx)
        {
            pLogContext->Struct(pRanges[idx]);
        }

        pLogContext->EndList();
        pLogContext->KeyAndValue("doNotWait", doNotWait);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Queue::AssociateFenceWithLastSubmit(
    IFence* pFence)
{
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::QueueAssociateFenceWithLastSubmit);
    const Result result = QueueDecorator::AssociateFenceWithLastSubmit(pFence);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("fence", pFence);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
void Queue::SetExecutionPriority(
    QueuePriority priority)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::QueueSetExecutionPriority);

    QueueDecorator::SetExecutionPriority(priority);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndEnum("priority", priority);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void Queue::Destroy()
{
    // Note that we can't time Destroy calls nor track their callbacks.
    if (m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::QueueDestroy))
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    QueueDecorator::Destroy();
}

} // InterfaceLogger
} // Pal

#endif
