/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_BUILD_INTERFACE_LOGGER

#include "core/layers/interfaceLogger/interfaceLoggerBorderColorPalette.h"
#include "core/layers/interfaceLogger/interfaceLoggerCmdAllocator.h"
#include "core/layers/interfaceLogger/interfaceLoggerCmdBuffer.h"
#include "core/layers/interfaceLogger/interfaceLoggerColorBlendState.h"
#include "core/layers/interfaceLogger/interfaceLoggerColorTargetView.h"
#include "core/layers/interfaceLogger/interfaceLoggerDepthStencilState.h"
#include "core/layers/interfaceLogger/interfaceLoggerDepthStencilView.h"
#include "core/layers/interfaceLogger/interfaceLoggerDevice.h"
#include "core/layers/interfaceLogger/interfaceLoggerFence.h"
#include "core/layers/interfaceLogger/interfaceLoggerGpuEvent.h"
#include "core/layers/interfaceLogger/interfaceLoggerGpuMemory.h"
#include "core/layers/interfaceLogger/interfaceLoggerImage.h"
#include "core/layers/interfaceLogger/interfaceLoggerIndirectCmdGenerator.h"
#include "core/layers/interfaceLogger/interfaceLoggerMsaaState.h"
#include "core/layers/interfaceLogger/interfaceLoggerPipeline.h"
#include "core/layers/interfaceLogger/interfaceLoggerPlatform.h"
#include "core/layers/interfaceLogger/interfaceLoggerPrivateScreen.h"
#include "core/layers/interfaceLogger/interfaceLoggerQueryPool.h"
#include "core/layers/interfaceLogger/interfaceLoggerQueue.h"
#include "core/layers/interfaceLogger/interfaceLoggerQueueSemaphore.h"
#include "core/layers/interfaceLogger/interfaceLoggerScreen.h"
#include "core/layers/interfaceLogger/interfaceLoggerShaderLibrary.h"
#include "core/layers/interfaceLogger/interfaceLoggerSwapChain.h"
#include "palSysUtil.h"

using namespace Util;

namespace Pal
{
namespace InterfaceLogger
{

// =====================================================================================================================
Device::Device(
    PlatformDecorator* pPlatform,
    IDevice*           pNextDevice,
    uint32             objectId)
    :
    DeviceDecorator(pPlatform, pNextDevice),
    m_objectId(objectId)
{
    m_pfnTable.pfnCreateTypedBufViewSrds   = CreateTypedBufferViewSrds;
    m_pfnTable.pfnCreateUntypedBufViewSrds = CreateUntypedBufferViewSrds;
    m_pfnTable.pfnCreateImageViewSrds      = CreateImageViewSrds;
    m_pfnTable.pfnCreateFmaskViewSrds      = CreateFmaskViewSrds;
    m_pfnTable.pfnCreateSamplerSrds        = CreateSamplerSrds;
}

// =====================================================================================================================
Result Device::CommitSettingsAndInit()
{
    auto*const pPlatform = static_cast<Platform*>(m_pPlatform);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCommitSettingsAndInit;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    Result result         = DeviceDecorator::CommitSettingsAndInit();
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        // We must initialize logging here, now that we finally have our settings.
        result = pPlatform->CommitLoggingSettings();
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Device::Finalize(
    const DeviceFinalizeInfo& finalizeInfo)
{
    auto*const pPlatform = static_cast<Platform*>(m_pPlatform);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceFinalize;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = DeviceDecorator::Finalize(finalizeInfo);
    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("finalizeInfo", finalizeInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Device::Cleanup()
{
    auto*const pPlatform = static_cast<Platform*>(m_pPlatform);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCleanup;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = DeviceDecorator::Cleanup();
    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Device::SetMaxQueuedFrames(
    uint32 maxFrames)
{
    auto*const pPlatform = static_cast<Platform*>(m_pPlatform);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceSetMaxQueuedFrames;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = DeviceDecorator::SetMaxQueuedFrames(maxFrames);
    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndValue("maxFrames", maxFrames);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Device::AddGpuMemoryReferences(
    uint32              gpuMemRefCount,
    const GpuMemoryRef* pGpuMemoryRefs,
    IQueue*             pQueue,
    uint32              flags
    )
{
    auto*const pPlatform = static_cast<Platform*>(m_pPlatform);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceAddGpuMemoryReferences;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = DeviceDecorator::AddGpuMemoryReferences(gpuMemRefCount, pGpuMemoryRefs, pQueue, flags);
    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndGpuMemoryRefFlags("flags", flags);
        pLogContext->KeyAndBeginList("gpuMemoryRefs", false);

        for (uint32 idx = 0; idx < gpuMemRefCount; ++idx)
        {
            pLogContext->Struct(pGpuMemoryRefs[idx]);
        }

        pLogContext->EndList();
        pLogContext->KeyAndObject("queue", pQueue);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Device::RemoveGpuMemoryReferences(
    uint32            gpuMemoryCount,
    IGpuMemory*const* ppGpuMemory,
    IQueue*           pQueue
    )
{
    auto*const pPlatform = static_cast<Platform*>(m_pPlatform);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceRemoveGpuMemoryReferences;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = DeviceDecorator::RemoveGpuMemoryReferences(gpuMemoryCount, ppGpuMemory, pQueue);
    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndBeginList("gpuMemoryList", false);

        for (uint32 idx = 0; idx < gpuMemoryCount; ++idx)
        {
            pLogContext->Object(ppGpuMemory[idx]);
        }

        pLogContext->EndList();
        pLogContext->KeyAndObject("queue", pQueue);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Device::SetClockMode(
    const SetClockModeInput& setClockModeInput,
    SetClockModeOutput*      pSetClockModeOutput)
{
    auto*const pPlatform = static_cast<Platform*>(m_pPlatform);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceSetClockMode;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    Result result         = m_pNextLayer->SetClockMode(setClockModeInput, pSetClockModeOutput);
    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("setClockModeInput", setClockModeInput);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);

        if (pSetClockModeOutput != nullptr)
        {
            pLogContext->KeyAndStruct("setClockModeOutput", *pSetClockModeOutput);
        }
        else
        {
            pLogContext->KeyAndNullValue("setClockModeOutput");
        }
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Device::SetMgpuMode(
    const SetMgpuModeInput& setMgpuModeInput
    ) const
{
    auto*const pPlatform = static_cast<Platform*>(m_pPlatform);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceSetMgpuMode;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = DeviceDecorator::SetMgpuMode(setMgpuModeInput);
    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("setMgpuModeInput", setMgpuModeInput);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Device::ResetFences(
    uint32        fenceCount,
    IFence*const* ppFences
    ) const
{
    auto*const pPlatform = static_cast<Platform*>(m_pPlatform);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceResetFences;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = DeviceDecorator::ResetFences(fenceCount, ppFences);
    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndBeginList("fences", false);

        for (uint32 idx = 0; idx < fenceCount; ++idx)
        {
            pLogContext->Object(ppFences[idx]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Device::WaitForFences(
    uint32              fenceCount,
    const IFence*const* ppFences,
    bool                waitAll,
    uint64              timeout
    ) const
{
    auto*const pPlatform = static_cast<Platform*>(m_pPlatform);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceWaitForFences;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = DeviceDecorator::WaitForFences(fenceCount, ppFences, waitAll, timeout);
    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndBeginList("fences", false);

        for (uint32 idx = 0; idx < fenceCount; ++idx)
        {
            pLogContext->Object(ppFences[idx]);
        }

        pLogContext->EndList();
        pLogContext->KeyAndValue("waitAll", waitAll);
        pLogContext->KeyAndValue("timeout", timeout);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
void Device::BindTrapHandler(
    PipelineBindPoint pipelineType,
    IGpuMemory*       pGpuMemory,
    gpusize           offset)
{
    auto*const pPlatform = static_cast<Platform*>(m_pPlatform);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceBindTrapHandler;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    DeviceDecorator::BindTrapHandler(pipelineType, pGpuMemory, offset);
    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndEnum("pipelineType", pipelineType);
        pLogContext->KeyAndObject("gpuMemory", pGpuMemory);
        pLogContext->KeyAndValue("offset", offset);
        pLogContext->EndInput();

        pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void Device::BindTrapBuffer(
    PipelineBindPoint pipelineType,
    IGpuMemory*       pGpuMemory,
    gpusize           offset)
{
    auto*const pPlatform = static_cast<Platform*>(m_pPlatform);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceBindTrapBuffer;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    DeviceDecorator::BindTrapBuffer(pipelineType, pGpuMemory, offset);
    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndEnum("pipelineType", pipelineType);
        pLogContext->KeyAndObject("gpuMemory", pGpuMemory);
        pLogContext->KeyAndValue("offset", offset);
        pLogContext->EndInput();

        pPlatform->LogEndFunc(pLogContext);
    }
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
    auto*const pPlatform  = static_cast<Platform*>(m_pPlatform);
    IQueue*    pNextQueue = nullptr;

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreateQueue;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = m_pNextLayer->CreateQueue(createInfo, NextObjectAddr<Queue>(pPlacementAddr), &pNextQueue);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextQueue != nullptr);
        pNextQueue->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::Queue);

        (*ppQueue) = PAL_PLACEMENT_NEW(pPlacementAddr) Queue(pNextQueue, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppQueue);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetMultiQueueSize(
    uint32                 queueCount,
    const QueueCreateInfo* pCreateInfo,
    Result*                pResult
    ) const
{
    return m_pNextLayer->GetMultiQueueSize(queueCount, pCreateInfo, pResult) + sizeof(Queue);
}

// =====================================================================================================================
Result Device::CreateMultiQueue(
    uint32                 queueCount,
    const QueueCreateInfo* pCreateInfo,
    void*                  pPlacementAddr,
    IQueue**               ppQueue)
{
    auto*const pPlatform  = static_cast<Platform*>(m_pPlatform);
    IQueue*    pNextQueue = nullptr;

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreateMultiQueue;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = m_pNextLayer->CreateMultiQueue(queueCount,
                                                           pCreateInfo,
                                                           NextObjectAddr<Queue>(pPlacementAddr),
                                                           &pNextQueue);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextQueue != nullptr);
        pNextQueue->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::Queue);

        (*ppQueue) = PAL_PLACEMENT_NEW(pPlacementAddr) Queue(pNextQueue, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndBeginList("createInfo", false);

        for (uint32 i = 0; i < queueCount; i++)
        {
            pLogContext->Struct(pCreateInfo[i]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppQueue);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetGpuMemorySize(
    const GpuMemoryCreateInfo& createInfo,
    Result*                    pResult
    ) const
{
    GpuMemoryCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pImage = NextImage(nextCreateInfo.pImage);

    return m_pNextLayer->GetGpuMemorySize(nextCreateInfo, pResult) + sizeof(GpuMemory);
}

// =====================================================================================================================
Result Device::CreateGpuMemory(
    const GpuMemoryCreateInfo& createInfo,
    void*                      pPlacementAddr,
    IGpuMemory**               ppGpuMemory)
{
    auto*const  pPlatform      = static_cast<Platform*>(m_pPlatform);
    IGpuMemory* pNextGpuMemory = nullptr;

    GpuMemoryCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pImage = NextImage(nextCreateInfo.pImage);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreateGpuMemory;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = m_pNextLayer->CreateGpuMemory(nextCreateInfo,
                                                          NextObjectAddr<GpuMemory>(pPlacementAddr),
                                                          &pNextGpuMemory);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextGpuMemory != nullptr);
        pNextGpuMemory->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::GpuMemory);

        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pPlacementAddr) GpuMemory(pNextGpuMemory, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppGpuMemory);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetPinnedGpuMemorySize(
    const PinnedGpuMemoryCreateInfo& createInfo,
    Result*                          pResult
    ) const
{
    return m_pNextLayer->GetPinnedGpuMemorySize(createInfo, pResult) + sizeof(GpuMemory);
}

// =====================================================================================================================
Result Device::CreatePinnedGpuMemory(
    const PinnedGpuMemoryCreateInfo& createInfo,
    void*                            pPlacementAddr,
    IGpuMemory**                     ppGpuMemory)
{
    auto*const  pPlatform   = static_cast<Platform*>(m_pPlatform);
    IGpuMemory* pNextMemObj = nullptr;

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreatePinnedGpuMemory;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = m_pNextLayer->CreatePinnedGpuMemory(createInfo,
                                                                NextObjectAddr<GpuMemory>(pPlacementAddr),
                                                                &pNextMemObj);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextMemObj != nullptr);
        pNextMemObj->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::GpuMemory);

        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pPlacementAddr) GpuMemory(pNextMemObj, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppGpuMemory);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetSvmGpuMemorySize(
    const SvmGpuMemoryCreateInfo& createInfo,
    Result*                       pResult
    ) const
{
    SvmGpuMemoryCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pReservedGpuVaOwner = NextGpuMemory(createInfo.pReservedGpuVaOwner);
    return m_pNextLayer->GetSvmGpuMemorySize(nextCreateInfo, pResult) + sizeof(GpuMemory);
}

// =====================================================================================================================
Result Device::CreateSvmGpuMemory(
    const SvmGpuMemoryCreateInfo& createInfo,
    void*                         pPlacementAddr,
    IGpuMemory**                  ppGpuMemory)
{
    auto*const  pPlatform   = static_cast<Platform*>(m_pPlatform);
    IGpuMemory* pNextMemObj = nullptr;

    SvmGpuMemoryCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pReservedGpuVaOwner = NextGpuMemory(createInfo.pReservedGpuVaOwner);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreateSvmGpuMemory;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = m_pNextLayer->CreateSvmGpuMemory(nextCreateInfo,
                                                             NextObjectAddr<GpuMemory>(pPlacementAddr),
                                                             &pNextMemObj);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextMemObj != nullptr);
        pNextMemObj->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::GpuMemory);

        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pPlacementAddr) GpuMemory(pNextMemObj, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppGpuMemory);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetSharedGpuMemorySize(
    const GpuMemoryOpenInfo& openInfo,
    Result*                  pResult
    ) const
{
    GpuMemoryOpenInfo nextOpenInfo = openInfo;
    nextOpenInfo.pSharedMem = NextGpuMemory(openInfo.pSharedMem);

    return m_pNextLayer->GetSharedGpuMemorySize(nextOpenInfo, pResult) + sizeof(GpuMemory);
}

// =====================================================================================================================
Result Device::OpenSharedGpuMemory(
    const GpuMemoryOpenInfo& openInfo,
    void*                    pPlacementAddr,
    IGpuMemory**             ppGpuMemory)
{
    auto*const  pPlatform   = static_cast<Platform*>(m_pPlatform);
    IGpuMemory* pNextMemObj = nullptr;

    GpuMemoryOpenInfo nextOpenInfo = openInfo;
    nextOpenInfo.pSharedMem = NextGpuMemory(openInfo.pSharedMem);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceOpenSharedGpuMemory;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = m_pNextLayer->OpenSharedGpuMemory(nextOpenInfo,
                                                              NextObjectAddr<GpuMemory>(pPlacementAddr),
                                                              &pNextMemObj);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextMemObj != nullptr);
        pNextMemObj->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::GpuMemory);

        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pPlacementAddr) GpuMemory(pNextMemObj, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("openInfo", openInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppGpuMemory);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetExternalSharedGpuMemorySize(
    Result* pResult
    ) const
{
    return m_pNextLayer->GetExternalSharedGpuMemorySize(pResult) + sizeof(GpuMemory);
}

// =====================================================================================================================
Result Device::OpenExternalSharedGpuMemory(
    const ExternalGpuMemoryOpenInfo& openInfo,
    void*                            pPlacementAddr,
    GpuMemoryCreateInfo*             pMemCreateInfo,
    IGpuMemory**                     ppGpuMemory)
{
    auto*const  pPlatform   = static_cast<Platform*>(m_pPlatform);
    IGpuMemory* pNextMemObj = nullptr;

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceOpenExternalSharedGpuMemory;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = m_pNextLayer->OpenExternalSharedGpuMemory(openInfo,
                                                                      NextObjectAddr<GpuMemory>(pPlacementAddr),
                                                                      pMemCreateInfo,
                                                                      &pNextMemObj);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextMemObj != nullptr);
        pNextMemObj->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::GpuMemory);

        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pPlacementAddr) GpuMemory(pNextMemObj, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("openInfo", openInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppGpuMemory);
        pLogContext->KeyAndStruct("memCreateInfo", *pMemCreateInfo);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetPeerGpuMemorySize(
    const PeerGpuMemoryOpenInfo& openInfo,
    Result*                      pResult
    ) const
{
    PeerGpuMemoryOpenInfo nextOpenInfo = openInfo;
    nextOpenInfo.pOriginalMem = NextGpuMemory(openInfo.pOriginalMem);

    return m_pNextLayer->GetPeerGpuMemorySize(nextOpenInfo, pResult) + sizeof(GpuMemory);
}

// =====================================================================================================================
Result Device::OpenPeerGpuMemory(
    const PeerGpuMemoryOpenInfo& openInfo,
    void*                        pPlacementAddr,
    IGpuMemory**                 ppGpuMemory)
{
    auto*const  pPlatform   = static_cast<Platform*>(m_pPlatform);
    IGpuMemory* pNextMemObj = nullptr;

    PeerGpuMemoryOpenInfo nextOpenInfo = openInfo;
    nextOpenInfo.pOriginalMem = NextGpuMemory(openInfo.pOriginalMem);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceOpenPeerGpuMemory;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = m_pNextLayer->OpenPeerGpuMemory(nextOpenInfo,
                                                            NextObjectAddr<GpuMemory>(pPlacementAddr),
                                                            &pNextMemObj);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextMemObj != nullptr);
        pNextMemObj->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::GpuMemory);

        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pPlacementAddr) GpuMemory(pNextMemObj, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("openInfo", openInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppGpuMemory);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetImageSize(
    const ImageCreateInfo& createInfo,
    Result*                pResult
    ) const
{
    return m_pNextLayer->GetImageSize(createInfo, pResult) + sizeof(Image);
}

// =====================================================================================================================
Result Device::CreateImage(
    const ImageCreateInfo& createInfo,
    void*                  pPlacementAddr,
    IImage**               ppImage)
{
    auto*const pPlatform  = static_cast<Platform*>(m_pPlatform);
    IImage*    pNextImage = nullptr;

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreateImage;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = m_pNextLayer->CreateImage(createInfo,
                                                      NextObjectAddr<Image>(pPlacementAddr),
                                                      &pNextImage);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextImage != nullptr);
        pNextImage->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::Image);

        (*ppImage) = PAL_PLACEMENT_NEW(pPlacementAddr) Image(pNextImage, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppImage);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
void Device::GetPresentableImageSizes(
    const PresentableImageCreateInfo& createInfo,
    size_t*                           pImageSize,
    size_t*                           pGpuMemorySize,
    Result*                           pResult
    ) const
{
    PresentableImageCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pScreen    = NextScreen(createInfo.pScreen);
    nextCreateInfo.pSwapChain = NextSwapChain(createInfo.pSwapChain);

    m_pNextLayer->GetPresentableImageSizes(nextCreateInfo, pImageSize, pGpuMemorySize, pResult);

    (*pImageSize)     += sizeof(Image);
    (*pGpuMemorySize) += sizeof(GpuMemory);
}

// =====================================================================================================================
Result Device::CreatePresentableImage(
    const PresentableImageCreateInfo& createInfo,
    void*                             pImagePlacementAddr,
    void*                             pGpuMemoryPlacementAddr,
    IImage**                          ppImage,
    IGpuMemory**                      ppGpuMemory)
{
    auto*const  pPlatform      = static_cast<Platform*>(m_pPlatform);
    IImage*     pNextImage     = nullptr;
    IGpuMemory* pNextGpuMemory = nullptr;

    PresentableImageCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pScreen    = NextScreen(createInfo.pScreen);
    nextCreateInfo.pSwapChain = NextSwapChain(createInfo.pSwapChain);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreatePresentableImage;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = m_pNextLayer->CreatePresentableImage(nextCreateInfo,
                                                                 NextObjectAddr<Image>(pImagePlacementAddr),
                                                                 NextObjectAddr<GpuMemory>(pGpuMemoryPlacementAddr),
                                                                 &pNextImage,
                                                                 &pNextGpuMemory);
    funcInfo.postCallTime = pPlatform->GetTime();

    if ((result == Result::Success) || (result == Result::TooManyFlippableAllocations))
    {
        PAL_ASSERT((pNextImage != nullptr) && (pNextGpuMemory != nullptr));
        pNextImage->SetClientData(pImagePlacementAddr);
        pNextGpuMemory->SetClientData(pGpuMemoryPlacementAddr);

        const uint32 imageId     = pPlatform->NewObjectId(InterfaceObject::Image);
        const uint32 gpuMemoryId = pPlatform->NewObjectId(InterfaceObject::GpuMemory);

        (*ppImage)     = PAL_PLACEMENT_NEW(pImagePlacementAddr)     Image(pNextImage, this, imageId);
        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pGpuMemoryPlacementAddr) GpuMemory(pNextGpuMemory, this, gpuMemoryId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdImageObj", *ppImage);
        pLogContext->KeyAndObject("createdGpuMemoryObj", *ppGpuMemory);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
void Device::GetPeerImageSizes(
    const PeerImageOpenInfo& openInfo,
    size_t*                  pPeerImageSize,
    size_t*                  pPeerGpuMemorySize,
    Result*                  pResult
    ) const
{
    PeerImageOpenInfo nextOpenInfo = openInfo;
    nextOpenInfo.pOriginalImage = NextImage(openInfo.pOriginalImage);

    m_pNextLayer->GetPeerImageSizes(nextOpenInfo, pPeerImageSize, pPeerGpuMemorySize, pResult);

    (*pPeerImageSize)     += sizeof(Image);
    (*pPeerGpuMemorySize) += sizeof(GpuMemory);
}

// =====================================================================================================================
Result Device::OpenPeerImage(
    const PeerImageOpenInfo& openInfo,
    void*                    pImagePlacementAddr,
    void*                    pGpuMemoryPlacementAddr,
    IImage**                 ppImage,
    IGpuMemory**             ppGpuMemory)
{
    auto*const  pPlatform      = static_cast<Platform*>(m_pPlatform);
    IImage*     pNextImage     = nullptr;
    IGpuMemory* pNextGpuMemory = nullptr;

    PeerImageOpenInfo nextOpenInfo = openInfo;
    nextOpenInfo.pOriginalImage = NextImage(openInfo.pOriginalImage);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceOpenPeerImage;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = m_pNextLayer->OpenPeerImage(nextOpenInfo,
                                                        NextObjectAddr<Image>(pImagePlacementAddr),
                                                        NextObjectAddr<GpuMemory>(pGpuMemoryPlacementAddr),
                                                        &pNextImage,
                                                        &pNextGpuMemory);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT((pNextImage != nullptr) && (pNextGpuMemory != nullptr));

        pNextImage->SetClientData(pImagePlacementAddr);
        pNextGpuMemory->SetClientData(pGpuMemoryPlacementAddr);

        const uint32 imageId     = pPlatform->NewObjectId(InterfaceObject::Image);
        const uint32 gpuMemoryId = pPlatform->NewObjectId(InterfaceObject::GpuMemory);

        (*ppImage)     = PAL_PLACEMENT_NEW(pImagePlacementAddr)     Image(pNextImage, this, imageId);
        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pGpuMemoryPlacementAddr) GpuMemory(pNextGpuMemory, this, gpuMemoryId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("openInfo", openInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdImageObj", *ppImage);
        pLogContext->KeyAndObject("createdGpuMemoryObj", *ppGpuMemory);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Device::GetExternalSharedImageSizes(
    const ExternalImageOpenInfo& openInfo,
    size_t*                      pImageSize,
    size_t*                      pGpuMemorySize,
    ImageCreateInfo*             pImgCreateInfo
    ) const
{
    Result result = m_pNextLayer->GetExternalSharedImageSizes(openInfo, pImageSize, pGpuMemorySize, pImgCreateInfo);

    (*pImageSize)     += sizeof(Image);
    (*pGpuMemorySize) += sizeof(GpuMemory);

    return result;
}

// =====================================================================================================================
Result Device::OpenExternalSharedImage(
    const ExternalImageOpenInfo& openInfo,
    void*                        pImagePlacementAddr,
    void*                        pGpuMemoryPlacementAddr,
    GpuMemoryCreateInfo*         pMemCreateInfo,
    IImage**                     ppImage,
    IGpuMemory**                 ppGpuMemory)
{
    auto*const  pPlatform      = static_cast<Platform*>(m_pPlatform);
    IImage*     pNextImage     = nullptr;
    IGpuMemory* pNextGpuMemory = nullptr;

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceOpenExternalSharedImage;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = m_pNextLayer->OpenExternalSharedImage(openInfo,
                                                                  NextObjectAddr<Image>(pImagePlacementAddr),
                                                                  NextObjectAddr<GpuMemory>(pGpuMemoryPlacementAddr),
                                                                  pMemCreateInfo,
                                                                  &pNextImage,
                                                                  &pNextGpuMemory);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT((pNextImage != nullptr) && (pNextGpuMemory != nullptr));

        pNextImage->SetClientData(pImagePlacementAddr);
        pNextGpuMemory->SetClientData(pGpuMemoryPlacementAddr);

        const uint32 imageId     = pPlatform->NewObjectId(InterfaceObject::Image);
        const uint32 gpuMemoryId = pPlatform->NewObjectId(InterfaceObject::GpuMemory);

        (*ppImage)     = PAL_PLACEMENT_NEW(pImagePlacementAddr)     Image(pNextImage, this, imageId);
        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pGpuMemoryPlacementAddr) GpuMemory(pNextGpuMemory, this, gpuMemoryId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("openInfo", openInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdImageObj", *ppImage);
        pLogContext->KeyAndObject("createdGpuMemoryObj", *ppGpuMemory);
        if (pMemCreateInfo != nullptr)
        {
            pLogContext->KeyAndStruct("memCreateInfo", *pMemCreateInfo);
        }
        else
        {
            pLogContext->KeyAndNullValue("memCreateInfo");
        }
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetColorTargetViewSize(
    Result* pResult
    ) const
{
    return m_pNextLayer->GetColorTargetViewSize(pResult) + sizeof(ColorTargetView);
}

// =====================================================================================================================
Result Device::CreateColorTargetView(
    const ColorTargetViewCreateInfo& createInfo,
    void*                            pPlacementAddr,
    IColorTargetView**               ppColorTargetView
    ) const
{
    auto*const        pPlatform = static_cast<Platform*>(m_pPlatform);
    IColorTargetView* pNextView = nullptr;

    ColorTargetViewCreateInfo nextCreateInfo = createInfo;

    if (createInfo.flags.isBufferView)
    {
        nextCreateInfo.bufferInfo.pGpuMemory = NextGpuMemory(createInfo.bufferInfo.pGpuMemory);
    }
    else
    {
        nextCreateInfo.imageInfo.pImage      = NextImage(createInfo.imageInfo.pImage);
    }

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreateColorTargetView;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = m_pNextLayer->CreateColorTargetView(nextCreateInfo,
                                                                NextObjectAddr<ColorTargetView>(pPlacementAddr),
                                                                &pNextView);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextView != nullptr);
        pNextView->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::ColorTargetView);

        (*ppColorTargetView) = PAL_PLACEMENT_NEW(pPlacementAddr) ColorTargetView(pNextView, createInfo, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppColorTargetView);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetDepthStencilViewSize(
    Result* pResult
    ) const
{
    return m_pNextLayer->GetDepthStencilViewSize(pResult) + sizeof(DepthStencilView);
}

// =====================================================================================================================
Result Device::CreateDepthStencilView(
    const DepthStencilViewCreateInfo& createInfo,
    void*                             pPlacementAddr,
    IDepthStencilView**               ppDepthStencilView
    ) const
{
    auto*const         pPlatform = static_cast<Platform*>(m_pPlatform);
    IDepthStencilView* pNextView = nullptr;

    DepthStencilViewCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pImage                     = NextImage(createInfo.pImage);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreateDepthStencilView;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = m_pNextLayer->CreateDepthStencilView(nextCreateInfo,
                                                                 NextObjectAddr<DepthStencilView>(pPlacementAddr),
                                                                 &pNextView);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextView != nullptr);
        pNextView->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::DepthStencilView);

        (*ppDepthStencilView) = PAL_PLACEMENT_NEW(pPlacementAddr) DepthStencilView(pNextView, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppDepthStencilView);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Device::SetSamplePatternPalette(
    const SamplePatternPalette& palette)
{
    auto*const pPlatform = static_cast<Platform*>(m_pPlatform);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceSetSamplePatternPalette;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = DeviceDecorator::SetSamplePatternPalette(palette);
    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("palette", palette);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetBorderColorPaletteSize(
    const BorderColorPaletteCreateInfo& createInfo,
    Result*                             pResult
    ) const
{
    return m_pNextLayer->GetBorderColorPaletteSize(createInfo, pResult) + sizeof(BorderColorPalette);
}

// =====================================================================================================================
Result Device::CreateBorderColorPalette(
    const BorderColorPaletteCreateInfo& createInfo,
    void*                               pPlacementAddr,
    IBorderColorPalette**               ppPalette
    ) const
{
    auto*const           pPlatform    = static_cast<Platform*>(m_pPlatform);
    IBorderColorPalette* pNextPalette = nullptr;

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreateBorderColorPalette;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = m_pNextLayer->CreateBorderColorPalette(createInfo,
                                                                   NextObjectAddr<BorderColorPalette>(pPlacementAddr),
                                                                   &pNextPalette);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextPalette != nullptr);
        pNextPalette->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::BorderColorPalette);

        (*ppPalette) = PAL_PLACEMENT_NEW(pPlacementAddr) BorderColorPalette(pNextPalette, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppPalette);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
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
    auto*const pPlatform     = static_cast<Platform*>(m_pPlatform);
    IPipeline* pNextPipeline = nullptr;

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreateComputePipeline;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = m_pNextLayer->CreateComputePipeline(createInfo,
                                                                NextObjectAddr<Pipeline>(pPlacementAddr),
                                                                &pNextPipeline);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextPipeline != nullptr);
        pNextPipeline->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::Pipeline);

        (*ppPipeline) = PAL_PLACEMENT_NEW(pPlacementAddr) Pipeline(pNextPipeline, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppPipeline);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
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
    auto*const pPlatform     = static_cast<Platform*>(m_pPlatform);
    IPipeline* pNextPipeline = nullptr;

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreateGraphicsPipeline;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = m_pNextLayer->CreateGraphicsPipeline(createInfo,
                                                                 NextObjectAddr<Pipeline>(pPlacementAddr),
                                                                 &pNextPipeline);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextPipeline != nullptr);
        pNextPipeline->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::Pipeline);

        (*ppPipeline) = PAL_PLACEMENT_NEW(pPlacementAddr) Pipeline(pNextPipeline, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppPipeline);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetShaderLibrarySize(
    const ShaderLibraryCreateInfo& createInfo,
    Result*                        pResult
    ) const
{
    return m_pNextLayer->GetShaderLibrarySize(createInfo, pResult) + sizeof(ShaderLibrary);
}

// =====================================================================================================================
Result Device::CreateShaderLibrary(
    const ShaderLibraryCreateInfo& createInfo,
    void*                          pPlacementAddr,
    IShaderLibrary**               ppLibrary)
{
    auto*const      pPlatform = static_cast<Platform*>(m_pPlatform);
    IShaderLibrary* pLibrary  = nullptr;

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreateShaderLibrary;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    Result result = m_pNextLayer->CreateShaderLibrary(createInfo,
                                                      NextObjectAddr<ShaderLibrary>(pPlacementAddr),
                                                      &pLibrary);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pLibrary != nullptr);
        pLibrary->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::ShaderLibrary);

        (*ppLibrary) = PAL_PLACEMENT_NEW(pPlacementAddr) ShaderLibrary(pLibrary, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppLibrary);
        if (result == Result::Success)
        {
            pLogContext->KeyAndBeginList("functions", false);
            for (uint32 i = 0; i < createInfo.funcCount; ++i)
            {
                pLogContext->Struct(createInfo.pFuncList[i]);
            }
            pLogContext->EndList();
        }
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetMsaaStateSize(
    const MsaaStateCreateInfo& createInfo,
    Result*                    pResult
    ) const
{
    return m_pNextLayer->GetMsaaStateSize(createInfo, pResult) + sizeof(MsaaState);
}

// =====================================================================================================================
Result Device::CreateMsaaState(
    const MsaaStateCreateInfo& createInfo,
    void*                      pPlacementAddr,
    IMsaaState**               ppMsaaState
    ) const
{
    auto*const  pPlatform  = static_cast<Platform*>(m_pPlatform);
    IMsaaState* pNextState = nullptr;

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreateMsaaState;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = m_pNextLayer->CreateMsaaState(createInfo,
                                                          NextObjectAddr<MsaaState>(pPlacementAddr),
                                                          &pNextState);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextState != nullptr);
        pNextState->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::MsaaState);

        (*ppMsaaState) = PAL_PLACEMENT_NEW(pPlacementAddr) MsaaState(pNextState, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppMsaaState);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetColorBlendStateSize(
    const ColorBlendStateCreateInfo& createInfo,
    Result*                          pResult
    ) const
{
    return m_pNextLayer->GetColorBlendStateSize(createInfo, pResult) + sizeof(ColorBlendState);
}

// =====================================================================================================================
Result Device::CreateColorBlendState(
    const ColorBlendStateCreateInfo& createInfo,
    void*                            pPlacementAddr,
    IColorBlendState**               ppColorBlendState
    ) const
{
    auto*const        pPlatform  = static_cast<Platform*>(m_pPlatform);
    IColorBlendState* pNextState = nullptr;

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreateColorBlendState;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = m_pNextLayer->CreateColorBlendState(createInfo,
                                                                NextObjectAddr<ColorBlendState>(pPlacementAddr),
                                                                &pNextState);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextState != nullptr);
        pNextState->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::ColorBlendState);

        (*ppColorBlendState) = PAL_PLACEMENT_NEW(pPlacementAddr) ColorBlendState(pNextState, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppColorBlendState);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetDepthStencilStateSize(
    const DepthStencilStateCreateInfo& createInfo,
    Result*                            pResult
    ) const
{
    return m_pNextLayer->GetDepthStencilStateSize(createInfo, pResult) + sizeof(DepthStencilState);
}

// =====================================================================================================================
Result Device::CreateDepthStencilState(
    const DepthStencilStateCreateInfo& createInfo,
    void*                              pPlacementAddr,
    IDepthStencilState**               ppDepthStencilState
    ) const
{
    auto*const          pPlatform  = static_cast<Platform*>(m_pPlatform);
    IDepthStencilState* pNextState = nullptr;

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreateDepthStencilState;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = m_pNextLayer->CreateDepthStencilState(createInfo,
                                                                  NextObjectAddr<DepthStencilState>(pPlacementAddr),
                                                                  &pNextState);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextState != nullptr);
        pNextState->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::DepthStencilState);

        (*ppDepthStencilState) = PAL_PLACEMENT_NEW(pPlacementAddr) DepthStencilState(pNextState, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppDepthStencilState);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetQueueSemaphoreSize(
    const QueueSemaphoreCreateInfo& createInfo,
    Result*                         pResult
    ) const
{
    return m_pNextLayer->GetQueueSemaphoreSize(createInfo, pResult) + sizeof(QueueSemaphore);
}

// =====================================================================================================================
Result Device::CreateQueueSemaphore(
    const QueueSemaphoreCreateInfo& createInfo,
    void*                           pPlacementAddr,
    IQueueSemaphore**               ppQueueSemaphore)
{
    auto*const       pPlatform      = static_cast<Platform*>(m_pPlatform);
    IQueueSemaphore* pNextSemaphore = nullptr;

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreateQueueSemaphore;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = m_pNextLayer->CreateQueueSemaphore(createInfo,
                                                               NextObjectAddr<QueueSemaphore>(pPlacementAddr),
                                                               &pNextSemaphore);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextSemaphore != nullptr);
        pNextSemaphore->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::QueueSemaphore);

        (*ppQueueSemaphore) = PAL_PLACEMENT_NEW(pPlacementAddr) QueueSemaphore(pNextSemaphore, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppQueueSemaphore);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetSharedQueueSemaphoreSize(
    const QueueSemaphoreOpenInfo& openInfo,
    Result*                       pResult
    ) const
{
    QueueSemaphoreOpenInfo nextOpenInfo = openInfo;
    nextOpenInfo.pSharedQueueSemaphore  = NextQueueSemaphore(openInfo.pSharedQueueSemaphore);

    return m_pNextLayer->GetSharedQueueSemaphoreSize(nextOpenInfo, pResult) + sizeof(QueueSemaphore);
}

// =====================================================================================================================
Result Device::OpenSharedQueueSemaphore(
    const QueueSemaphoreOpenInfo& openInfo,
    void*                         pPlacementAddr,
    IQueueSemaphore**             ppQueueSemaphore)
{
    auto*const       pPlatform      = static_cast<Platform*>(m_pPlatform);
    IQueueSemaphore* pNextSemaphore = nullptr;

    QueueSemaphoreOpenInfo nextOpenInfo = openInfo;
    nextOpenInfo.pSharedQueueSemaphore  = NextQueueSemaphore(openInfo.pSharedQueueSemaphore);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceOpenSharedQueueSemaphore;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = m_pNextLayer->OpenSharedQueueSemaphore(nextOpenInfo,
                                                                   NextObjectAddr<QueueSemaphore>(pPlacementAddr),
                                                                   &pNextSemaphore);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextSemaphore != nullptr);
        pNextSemaphore->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::QueueSemaphore);

        (*ppQueueSemaphore) = PAL_PLACEMENT_NEW(pPlacementAddr) QueueSemaphore(pNextSemaphore, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("openInfo", openInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppQueueSemaphore);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetExternalSharedQueueSemaphoreSize(
    const ExternalQueueSemaphoreOpenInfo& openInfo,
    Result*                               pResult
    ) const
{
    return m_pNextLayer->GetExternalSharedQueueSemaphoreSize(openInfo, pResult) +
           sizeof(QueueSemaphore);
}

// =====================================================================================================================
Result Device::OpenExternalSharedQueueSemaphore(
    const ExternalQueueSemaphoreOpenInfo& openInfo,
    void*                                 pPlacementAddr,
    IQueueSemaphore**                     ppQueueSemaphore)
{
    auto*const       pPlatform      = static_cast<Platform*>(m_pPlatform);
    IQueueSemaphore* pNextSemaphore = nullptr;

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceOpenExternalSharedQueueSemaphore;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result = m_pNextLayer->OpenExternalSharedQueueSemaphore(openInfo,
                                                                         NextObjectAddr<QueueSemaphore>(pPlacementAddr),
                                                                         &pNextSemaphore);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextSemaphore != nullptr);
        pNextSemaphore->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::QueueSemaphore);

        (*ppQueueSemaphore) = PAL_PLACEMENT_NEW(pPlacementAddr) QueueSemaphore(pNextSemaphore, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("openInfo", openInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppQueueSemaphore);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetFenceSize(
    Result* pResult
    ) const
{
    return m_pNextLayer->GetFenceSize(pResult) + sizeof(Fence);
}

// =====================================================================================================================
Result Device::CreateFence(
    const FenceCreateInfo& createInfo,
    void*                  pPlacementAddr,
    IFence**               ppFence
    ) const
{
    auto*const pPlatform  = static_cast<Platform*>(m_pPlatform);
    IFence*    pNextFence = nullptr;

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreateFence;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();

    const Result result   = m_pNextLayer->CreateFence(createInfo,
                                                      NextObjectAddr<Fence>(pPlacementAddr),
                                                      &pNextFence);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextFence != nullptr);
        pNextFence->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::Fence);

        (*ppFence) = PAL_PLACEMENT_NEW(pPlacementAddr) Fence(pNextFence, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppFence);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Device::OpenFence(
    const FenceOpenInfo& openInfo,
    void*                pPlacementAddr,
    IFence**             ppFence
    ) const
{
    auto*const pPlatform  = static_cast<Platform*>(m_pPlatform);
    IFence*    pNextFence = nullptr;

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceOpenFence;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();

    const Result result   = m_pNextLayer->OpenFence(openInfo,
                                                    NextObjectAddr<Fence>(pPlacementAddr),
                                                    &pNextFence);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextFence != nullptr);
        pNextFence->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::Fence);

        (*ppFence) = PAL_PLACEMENT_NEW(pPlacementAddr) Fence(pNextFence, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("openInfo", openInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("openedObj", *ppFence);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetGpuEventSize(
    const GpuEventCreateInfo& createInfo,
    Result*                   pResult
    ) const
{
    return m_pNextLayer->GetGpuEventSize(createInfo, pResult) + sizeof(GpuEvent);
}

// =====================================================================================================================
Result Device::CreateGpuEvent(
    const GpuEventCreateInfo& createInfo,
    void*                     pPlacementAddr,
    IGpuEvent**               ppGpuEvent)
{
    auto*const pPlatform     = static_cast<Platform*>(m_pPlatform);
    IGpuEvent* pNextGpuEvent = nullptr;

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreateGpuEvent;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = m_pNextLayer->CreateGpuEvent(createInfo,
                                                         NextObjectAddr<GpuEvent>(pPlacementAddr),
                                                         &pNextGpuEvent);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextGpuEvent != nullptr);
        pNextGpuEvent->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::GpuEvent);

        (*ppGpuEvent) = PAL_PLACEMENT_NEW(pPlacementAddr) GpuEvent(pNextGpuEvent, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();
        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppGpuEvent);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetQueryPoolSize(
    const QueryPoolCreateInfo& createInfo,
    Result*                    pResult
    ) const
{
    return m_pNextLayer->GetQueryPoolSize(createInfo, pResult) + sizeof(QueryPool);
}

// =====================================================================================================================
Result Device::CreateQueryPool(
    const QueryPoolCreateInfo& createInfo,
    void*                      pPlacementAddr,
    IQueryPool**               ppQueryPool
    ) const
{
    auto*const  pPlatform      = static_cast<Platform*>(m_pPlatform);
    IQueryPool* pNextQueryPool = nullptr;

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreateQueryPool;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = m_pNextLayer->CreateQueryPool(createInfo,
                                                          NextObjectAddr<QueryPool>(pPlacementAddr),
                                                          &pNextQueryPool);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextQueryPool != nullptr);
        pNextQueryPool->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::QueryPool);

        (*ppQueryPool) = PAL_PLACEMENT_NEW(pPlacementAddr) QueryPool(pNextQueryPool, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppQueryPool);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetCmdAllocatorSize(
    const CmdAllocatorCreateInfo& createInfo,
    Result*                       pResult
    ) const
{
    return m_pNextLayer->GetCmdAllocatorSize(createInfo, pResult) + sizeof(CmdAllocator);
}

// =====================================================================================================================
Result Device::CreateCmdAllocator(
    const CmdAllocatorCreateInfo& createInfo,
    void*                         pPlacementAddr,
    ICmdAllocator**               ppCmdAllocator)
{
    auto*const     pPlatform         = static_cast<Platform*>(m_pPlatform);
    ICmdAllocator* pNextCmdAllocator = nullptr;

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreateCmdAllocator;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = m_pNextLayer->CreateCmdAllocator(createInfo,
                                                             NextObjectAddr<CmdAllocator>(pPlacementAddr),
                                                             &pNextCmdAllocator);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextCmdAllocator != nullptr);
        pNextCmdAllocator->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::CmdAllocator);

        (*ppCmdAllocator) = PAL_PLACEMENT_NEW(pPlacementAddr) CmdAllocator(pNextCmdAllocator, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppCmdAllocator);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetCmdBufferSize(
    const CmdBufferCreateInfo& createInfo,
    Result*                    pResult
    ) const
{
    CmdBufferCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pCmdAllocator       = NextCmdAllocator(createInfo.pCmdAllocator);

    return m_pNextLayer->GetCmdBufferSize(nextCreateInfo, pResult) + sizeof(CmdBuffer);
}

// =====================================================================================================================
Result Device::CreateCmdBuffer(
    const CmdBufferCreateInfo& createInfo,
    void*                      pPlacementAddr,
    ICmdBuffer**               ppCmdBuffer)
{
    auto*const  pPlatform      = static_cast<Platform*>(m_pPlatform);
    ICmdBuffer* pNextCmdBuffer = nullptr;

    CmdBufferCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pCmdAllocator       = NextCmdAllocator(createInfo.pCmdAllocator);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreateCmdBuffer;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = m_pNextLayer->CreateCmdBuffer(nextCreateInfo,
                                                          NextObjectAddr<CmdBuffer>(pPlacementAddr),
                                                          &pNextCmdBuffer);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextCmdBuffer != nullptr);
        pNextCmdBuffer->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::CmdBuffer);

        (*ppCmdBuffer) = PAL_PLACEMENT_NEW(pPlacementAddr) CmdBuffer(pNextCmdBuffer, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppCmdBuffer);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetIndirectCmdGeneratorSize(
    const IndirectCmdGeneratorCreateInfo& createInfo,
    Result*                               pResult) const
{
    return m_pNextLayer->GetIndirectCmdGeneratorSize(createInfo, pResult) +
           sizeof(IndirectCmdGenerator);
}

// =====================================================================================================================
Result Device::CreateIndirectCmdGenerator(
    const IndirectCmdGeneratorCreateInfo& createInfo,
    void*                                 pPlacementAddr,
    IIndirectCmdGenerator**               ppGenerator) const
{
    auto*const             pPlatform      = static_cast<Platform*>(m_pPlatform);
    IIndirectCmdGenerator* pNextGenerator = nullptr;

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreateIndirectCmdGenerator;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   =
        m_pNextLayer->CreateIndirectCmdGenerator(createInfo,
                                                 NextObjectAddr<IndirectCmdGenerator>(pPlacementAddr),
                                                 &pNextGenerator);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextGenerator != nullptr);
        pNextGenerator->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::IndirectCmdGenerator);

        (*ppGenerator) = PAL_PLACEMENT_NEW(pPlacementAddr) IndirectCmdGenerator(pNextGenerator, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppGenerator);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Device::GetPrivateScreens(
    uint32*          pNumScreens,
    IPrivateScreen** ppScreens)
{
    auto*const pPlatform = static_cast<Platform*>(m_pPlatform);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceGetPrivateScreens;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = DeviceDecorator::GetPrivateScreens(pNumScreens, ppScreens);
    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndBeginList("screens", false);

        for (uint32 idx = 0; idx < MaxPrivateScreens; ++idx)
        {
            // ppScreens can be null and can have null pointers so we always write MaxPrivateScreens values.
            if ((ppScreens == nullptr) || (ppScreens[idx] == nullptr))
            {
                pLogContext->NullValue();
            }
            else
            {
                pLogContext->Object(static_cast<PrivateScreen*>(ppScreens[idx]));
            }
        }

        pLogContext->EndList();
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Device::AddEmulatedPrivateScreen(
    const PrivateScreenCreateInfo& createInfo,
    uint32*                        pTargetId)
{
    PAL_ASSERT(pTargetId != nullptr);

    auto*const pPlatform = static_cast<Platform*>(m_pPlatform);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceAddEmulatedPrivateScreen;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = DeviceDecorator::AddEmulatedPrivateScreen(createInfo, pTargetId);
    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndValue("targetId", *pTargetId);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Device::RemoveEmulatedPrivateScreen(
    uint32 targetId)
{
    auto*const pPlatform = static_cast<Platform*>(m_pPlatform);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceRemoveEmulatedPrivateScreen;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = DeviceDecorator::RemoveEmulatedPrivateScreen(targetId);
    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndValue("targetId", targetId);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
void Device::GetPrivateScreenImageSizes(
    const PrivateScreenImageCreateInfo& createInfo,
    size_t*                             pImageSize,
    size_t*                             pGpuMemorySize,
    Result*                             pResult
    ) const
{
    PrivateScreenImageCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pScreen = NextPrivateScreen(createInfo.pScreen);

    m_pNextLayer->GetPrivateScreenImageSizes(nextCreateInfo, pImageSize, pGpuMemorySize, pResult);

    (*pImageSize)     += sizeof(Image);
    (*pGpuMemorySize) += sizeof(GpuMemory);
}

// =====================================================================================================================
Result Device::CreatePrivateScreenImage(
    const PrivateScreenImageCreateInfo& createInfo,
    void*                               pImagePlacementAddr,
    void*                               pGpuMemoryPlacementAddr,
    IImage**                            ppImage,
    IGpuMemory**                        ppGpuMemory)
{
    auto*const  pPlatform      = static_cast<Platform*>(m_pPlatform);
    IImage*     pNextImage     = nullptr;
    IGpuMemory* pNextGpuMemory = nullptr;

    PrivateScreenImageCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pScreen = NextPrivateScreen(createInfo.pScreen);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreatePrivateScreenImage;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    Result result = m_pNextLayer->CreatePrivateScreenImage(nextCreateInfo,
                                                           NextObjectAddr<Image>(pImagePlacementAddr),
                                                           NextObjectAddr<GpuMemory>(pGpuMemoryPlacementAddr),
                                                           &pNextImage,
                                                           &pNextGpuMemory);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        pNextImage->SetClientData(pImagePlacementAddr);
        pNextGpuMemory->SetClientData(pGpuMemoryPlacementAddr);

        const uint32 imageId     = pPlatform->NewObjectId(InterfaceObject::Image);
        const uint32 gpuMemoryId = pPlatform->NewObjectId(InterfaceObject::GpuMemory);

        (*ppImage)     = PAL_PLACEMENT_NEW(pImagePlacementAddr) Image(pNextImage, this, imageId);
        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pGpuMemoryPlacementAddr) GpuMemory(pNextGpuMemory, this, gpuMemoryId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdImageObj", *ppImage);
        pLogContext->KeyAndObject("createdGpuMemoryObj", *ppGpuMemory);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetSwapChainSize(
    const SwapChainCreateInfo& createInfo,
    Result*                    pResult
    ) const
{
    return m_pNextLayer->GetSwapChainSize(createInfo, pResult) + sizeof(SwapChain);
}

// =====================================================================================================================
Result Device::CreateSwapChain(
    const SwapChainCreateInfo& createInfo,
    void*                      pPlacementAddr,
    ISwapChain**               ppSwapChain)
{
    auto*const  pPlatform      = static_cast<Platform*>(m_pPlatform);
    ISwapChain* pNextSwapChain = nullptr;

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreateSwapChain;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = m_pNextLayer->CreateSwapChain(createInfo,
                                                          NextObjectAddr<SwapChain>(pPlacementAddr),
                                                          &pNextSwapChain);
    funcInfo.postCallTime = pPlatform->GetTime();

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextSwapChain != nullptr);
        pNextSwapChain->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::SwapChain);

        (*ppSwapChain) = PAL_PLACEMENT_NEW(pPlacementAddr) SwapChain(pNextSwapChain, this, objectId);
    }

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppSwapChain);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Device::SetPowerProfile(
    PowerProfile        profile,
    CustomPowerProfile* pInfo)
{
    auto*const pPlatform = static_cast<Platform*>(m_pPlatform);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceSetPowerProfile;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    const Result result   = DeviceDecorator::SetPowerProfile(profile, pInfo);
    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndEnum("profile", profile);

        if (pInfo != nullptr)
        {
            pLogContext->KeyAndBeginMap("info", false);
            pLogContext->KeyAndObject("screen", pInfo->pScreen);
            pLogContext->KeyAndBeginList("switchInfo", false);

            for (uint32 idx = 0; idx < pInfo->numSwitchInfo; ++idx)
            {
                pLogContext->Struct(pInfo->switchInfo[idx]);
            }

            pLogContext->EndList();
            pLogContext->EndMap();
        }
        else
        {
            pLogContext->KeyAndNullValue("info");
        }

        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);

        if (pInfo != nullptr)
        {
            pLogContext->KeyAndBeginMap("info", false);
            pLogContext->KeyAndBeginList("actualSwitchInfo", false);

            for (uint32 idx = 0; idx < pInfo->numSwitchInfo; ++idx)
            {
                pLogContext->Struct(pInfo->actualSwitchInfo[idx]);
            }

            pLogContext->EndList();
            pLogContext->EndMap();
        }
        else
        {
            pLogContext->KeyAndNullValue("info");
        }

        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Device::FlglQueryState(
    FlglState* pState)
{
    auto*const  pPlatform = static_cast<Platform*>(m_pPlatform);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceFlglQueryState;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    Result result         = m_pNextLayer->FlglQueryState(pState);
    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);

        if (pState != nullptr)
        {
            pLogContext->KeyAndStruct("pState", *pState);
        }
        else
        {
            pLogContext->KeyAndNullValue("pState");
        }
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }
    return result;
}

// =====================================================================================================================
Result Device::FlglSetFrameLock(
    bool enable)
{
    auto*const  pPlatform = static_cast<Platform*>(m_pPlatform);
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceFlglSetFrameLock;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    Result result         = m_pNextLayer->FlglSetFrameLock(enable);
    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndValue("enable", enable);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }
    return result;
}

// =====================================================================================================================
Result Device::FlglResetFrameCounter() const
{
    auto*const  pPlatform = static_cast<Platform*>(m_pPlatform);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceFlglResetFrameCounter;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    Result result         = m_pNextLayer->FlglResetFrameCounter();
    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }
    return result;
}

// =====================================================================================================================
Result Device::FlglGetFrameCounter(
    uint64* pValue
    ) const
{
    auto*const  pPlatform = static_cast<Platform*>(m_pPlatform);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceFlglGetFrameCounter;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    Result result         = m_pNextLayer->FlglGetFrameCounter(pValue);
    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);

        if (pValue != nullptr)
        {
            pLogContext->KeyAndValue("value", *pValue);
        }
        else
        {
            pLogContext->KeyAndNullValue("value");
        }
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }
    return result;
}

// =====================================================================================================================
Result Device::FlglGetFrameCounterResetStatus(
    bool* pReset
    ) const
{
    auto*const  pPlatform = static_cast<Platform*>(m_pPlatform);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceFlglGetFrameCounterResetStatus;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    Result result         = m_pNextLayer->FlglGetFrameCounterResetStatus(pReset);
    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);

        if (pReset != nullptr)
        {
            pLogContext->KeyAndValue("reset", *pReset);
        }
        else
        {
            pLogContext->KeyAndNullValue("reset");
        }
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }
    return result;
}

// =====================================================================================================================
PrivateScreenDecorator* Device::NewPrivateScreenDecorator(
    IPrivateScreen* pNextScreen,
    uint32          deviceIdx)
{
    constexpr size_t Size = sizeof(PrivateScreen);

    PrivateScreenDecorator* pDecorator     = nullptr;
    void*                   pPlacementAddr = PAL_MALLOC(Size, m_pPlatform, AllocInternal);

    if (pPlacementAddr != nullptr)
    {
        pNextScreen->SetClientData(pPlacementAddr);

        const uint32 objectId = static_cast<Platform*>(m_pPlatform)->NewObjectId(InterfaceObject::PrivateScreen);

        pDecorator = PAL_PLACEMENT_NEW(pPlacementAddr) PrivateScreen(pNextScreen, this, deviceIdx, objectId);
    }

    return pDecorator;
}

// =====================================================================================================================
void PAL_STDCALL Device::CreateTypedBufferViewSrds(
    const IDevice*        pDevice,
    uint32                count,
    const BufferViewInfo* pBufferViewInfo,
    void*                 pOut)
{
    const auto* pThis     = static_cast<const Device*>(pDevice);
    auto*const  pPlatform = static_cast<Platform*>(pThis->m_pPlatform);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreateTypedBufferViewSrds;
    funcInfo.objectId     = pThis->m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    DeviceDecorator::DecoratorCreateTypedBufViewSrds(pDevice, count, pBufferViewInfo, pOut);
    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndBeginList("bufferViewInfo", false);

        for (uint32 idx = 0; idx < count; ++idx)
        {
            pLogContext->Struct(pBufferViewInfo[idx]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void PAL_STDCALL Device::CreateUntypedBufferViewSrds(
    const IDevice*        pDevice,
    uint32                count,
    const BufferViewInfo* pBufferViewInfo,
    void*                 pOut)
{
    const auto* pThis     = static_cast<const Device*>(pDevice);
    auto*const  pPlatform = static_cast<Platform*>(pThis->m_pPlatform);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreateUntypedBufferViewSrds;
    funcInfo.objectId     = pThis->m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    DeviceDecorator::DecoratorCreateUntypedBufViewSrds(pDevice, count, pBufferViewInfo, pOut);
    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndBeginList("bufferViewInfo", false);

        for (uint32 idx = 0; idx < count; ++idx)
        {
            pLogContext->Struct(pBufferViewInfo[idx]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void PAL_STDCALL Device::CreateImageViewSrds(
    const IDevice*       pDevice,
    uint32               count,
    const ImageViewInfo* pImgViewInfo,
    void*                pOut)
{
    const auto* pThis     = static_cast<const Device*>(pDevice);
    auto*const  pPlatform = static_cast<Platform*>(pThis->m_pPlatform);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreateImageViewSrds;
    funcInfo.objectId     = pThis->m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    DeviceDecorator::DecoratorCreateImageViewSrds(pDevice, count, pImgViewInfo, pOut);
    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndBeginList("imageViewInfo", false);

        for (uint32 idx = 0; idx < count; ++idx)
        {
            pLogContext->Struct(pImgViewInfo[idx]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void PAL_STDCALL Device::CreateFmaskViewSrds(
    const IDevice*       pDevice,
    uint32               count,
    const FmaskViewInfo* pFmaskViewInfo,
    void*                pOut)
{
    const auto* pThis     = static_cast<const Device*>(pDevice);
    auto*const  pPlatform = static_cast<Platform*>(pThis->m_pPlatform);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreateFmaskViewSrds;
    funcInfo.objectId     = pThis->m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    DeviceDecorator::DecoratorCreateFmaskViewSrds(pDevice, count, pFmaskViewInfo, pOut);
    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndBeginList("fmaskViewInfo", false);

        for (uint32 idx = 0; idx < count; ++idx)
        {
            pLogContext->Struct(pFmaskViewInfo[idx]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void PAL_STDCALL Device::CreateSamplerSrds(
    const IDevice*     pDevice,
    uint32             count,
    const SamplerInfo* pSamplerInfo,
    void*              pOut)
{
    const auto* pThis     = static_cast<const Device*>(pDevice);
    auto*const  pPlatform = static_cast<Platform*>(pThis->m_pPlatform);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::DeviceCreateSamplerSrds;
    funcInfo.objectId     = pThis->m_objectId;
    funcInfo.preCallTime  = pPlatform->GetTime();
    DeviceDecorator::DecoratorCreateSamplerSrds(pDevice, count, pSamplerInfo, pOut);
    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndBeginList("samplerInfo", false);

        for (uint32 idx = 0; idx < count; ++idx)
        {
            pLogContext->Struct(pSamplerInfo[idx]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
Result Device::CreateVirtualDisplay(
    const VirtualDisplayInfo& virtualDisplayInfo,
    uint32*                   pScreenTargetId)
{
    auto*const  pPlatform = static_cast<Platform*>(m_pPlatform);
    BeginFuncInfo funcInfo;
    funcInfo.funcId      = InterfaceFunc::DeviceCreateVirtualDisplay;
    funcInfo.objectId    = m_objectId;
    funcInfo.preCallTime = pPlatform->GetTime();

    Result result = m_pNextLayer->CreateVirtualDisplay(virtualDisplayInfo, pScreenTargetId);

    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("virtualDisplayInfo", virtualDisplayInfo);
        pLogContext->EndOutput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndValue("screenTargetId", *pScreenTargetId);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }
    return result;
}

// =====================================================================================================================
Result Device::DestroyVirtualDisplay(
    uint32 screenTargetId)
{
    auto*const  pPlatform = static_cast<Platform*>(m_pPlatform);
    BeginFuncInfo funcInfo;
    funcInfo.funcId      = InterfaceFunc::DeviceDestroyVirtualDisplay;
    funcInfo.objectId    = m_objectId;
    funcInfo.preCallTime = pPlatform->GetTime();

    Result result = m_pNextLayer->DestroyVirtualDisplay(screenTargetId);

    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndValue("screenTargetId", screenTargetId);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }
    return result;
}

// =====================================================================================================================
Result Device::GetVirtualDisplayProperties(
    uint32                    screenTargetId,
    VirtualDisplayProperties* pProperties)
{
    auto*const  pPlatform = static_cast<Platform*>(m_pPlatform);
    BeginFuncInfo funcInfo;
    funcInfo.funcId      = InterfaceFunc::DeviceGetVirtualDisplayProperties;
    funcInfo.objectId    = m_objectId;
    funcInfo.preCallTime = pPlatform->GetTime();

    Result result = m_pNextLayer->GetVirtualDisplayProperties(screenTargetId, pProperties);

    funcInfo.postCallTime = pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndValue("screenTargetId", screenTargetId);
        pLogContext->EndOutput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndStruct("VirtualDisplayProperties", *pProperties);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }
    return result;
}

} // InterfaceLogger
} // Pal

#endif
