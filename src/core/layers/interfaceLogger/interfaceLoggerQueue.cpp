/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
    const SubmitInfo& submitInfo)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::QueueSubmit;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = QueueDecorator::Submit(submitInfo);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndBeginMap("submitInfo", false);
        pLogContext->KeyAndBeginMap("cmdBuffers", false);

        for (uint32 idx = 0; idx < submitInfo.cmdBufferCount; ++idx)
        {
            pLogContext->KeyAndObject("object", submitInfo.ppCmdBuffers[idx]);

            if ((submitInfo.pCmdBufInfoList != nullptr) && submitInfo.pCmdBufInfoList[idx].isValid)
            {
                pLogContext->KeyAndStruct("info", submitInfo.pCmdBufInfoList[idx]);
            }
            else
            {
                pLogContext->KeyAndNullValue("info");
            }
        }

        pLogContext->EndMap();
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
        pLogContext->KeyAndObject("fence", submitInfo.pFence);
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::QueueWaitIdle;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = QueueDecorator::WaitIdle();
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Queue::SignalQueueSemaphore(
    IQueueSemaphore* pQueueSemaphore)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::QueueSignalQueueSemaphore;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = QueueDecorator::SignalQueueSemaphore(pQueueSemaphore);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndObject("queueSemaphore", pQueueSemaphore);
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
    IQueueSemaphore* pQueueSemaphore)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::QueueWaitQueueSemaphore;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = QueueDecorator::WaitQueueSemaphore(pQueueSemaphore);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndObject("queueSemaphore", pQueueSemaphore);
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::QueuePresentDirect;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = QueueDecorator::PresentDirect(presentInfo);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("presentInfo", presentInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    m_pPlatform->NotifyPresent();

    return result;
}

// =====================================================================================================================
Result Queue::PresentSwapChain(
    const PresentSwapChainInfo& presentInfo)
{
    // Note: We must always call down to the next layer because we must release ownership of the image index.
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::QueuePresentSwapChain;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = QueueDecorator::PresentSwapChain(presentInfo);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("presentInfo", presentInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    m_pPlatform->NotifyPresent();

    return result;
}

// =====================================================================================================================
Result Queue::Delay(
    float delay)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::QueueDelay;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = QueueDecorator::Delay(delay);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndValue("delay", delay);
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
    float                 delayInUs,
    const IPrivateScreen* pScreen)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::QueueDelayAfterVsync;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = QueueDecorator::DelayAfterVsync(delayInUs, pScreen);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndValue("delayInUs", delayInUs);
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::QueueRemapVirtualMemoryPages;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = QueueDecorator::RemapVirtualMemoryPages(rangeCount, pRanges, doNotWait, pFence);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::QueueCopyVirtualMemoryPageMappings;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = QueueDecorator::CopyVirtualMemoryPageMappings(rangeCount, pRanges, doNotWait);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::QueueAssociateFenceWithLastSubmit;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = QueueDecorator::AssociateFenceWithLastSubmit(pFence);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::QueueSetExecutionPriority;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    QueueDecorator::SetExecutionPriority(priority);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndEnum("priority", priority);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void Queue::Destroy()
{
    // Note that we can't time a Destroy call.
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::QueueDestroy;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    funcInfo.postCallTime = funcInfo.preCallTime;

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        m_pPlatform->LogEndFunc(pLogContext);
    }

    QueueDecorator::Destroy();
}

} // InterfaceLogger
} // Pal
