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
#include "g_platformSettings.h"
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
    m_pfnTable.pfnCreateBvhSrds            = CreateBvhSrds;
}

// =====================================================================================================================
Result Device::CommitSettingsAndInit()
{
    auto*const   pPlatform = static_cast<Platform*>(m_pPlatform);
    const bool   active    = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceCommitSettingsAndInit);
    const Result result    = DeviceDecorator::CommitSettingsAndInit();

    if (result == Result::Success)
    {
        // We must initialize logging here, now that we finally have our settings.
        // But don't fail init.
        const Result layerResult = pPlatform->CommitLoggingSettings();
        PAL_ALERT_MSG(layerResult != Result::Success, "Failed to initialize interface logger");
        if ((layerResult == Result::ErrorPermissionDenied) ||
            (layerResult == Result::NotFound))
        {
            PAL_DPINFO("Check permissions on '%s' or change logDirectory/AMD_DEBUG_DIR.",
                       pPlatform->PlatformSettings().interfaceLoggerConfig.logDirectory);
        }
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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
    auto*const   pPlatform = static_cast<Platform*>(m_pPlatform);
    const bool   active    = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceFinalize);
    const Result result    = DeviceDecorator::Finalize(finalizeInfo);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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
    auto*const   pPlatform = static_cast<Platform*>(m_pPlatform);
    const bool   active    = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceCleanup);
    const Result result    = DeviceDecorator::Cleanup();

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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
    auto*const   pPlatform = static_cast<Platform*>(m_pPlatform);
    const bool   active    = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceSetMaxQueuedFrames);
    const Result result    = DeviceDecorator::SetMaxQueuedFrames(maxFrames);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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
    auto*const   pPlatform = static_cast<Platform*>(m_pPlatform);
    const bool   active    = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceAddGpuMemoryReferences);
    const Result result    = DeviceDecorator::AddGpuMemoryReferences(gpuMemRefCount, pGpuMemoryRefs, pQueue, flags
                                );

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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
    auto*const   pPlatform = static_cast<Platform*>(m_pPlatform);
    const bool   active    = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceRemoveGpuMemoryReferences);
    const Result result    = DeviceDecorator::RemoveGpuMemoryReferences(gpuMemoryCount, ppGpuMemory, pQueue
                                );

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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
    auto*const   pPlatform = static_cast<Platform*>(m_pPlatform);
    const bool   active    = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceSetClockMode);
    const Result result    = m_pNextLayer->SetClockMode(setClockModeInput, pSetClockModeOutput);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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
    auto*const   pPlatform = static_cast<Platform*>(m_pPlatform);
    const bool   active    = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceSetMgpuMode);
    const Result result    = DeviceDecorator::SetMgpuMode(setMgpuModeInput);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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
    auto*const   pPlatform = static_cast<Platform*>(m_pPlatform);
    const bool   active    = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceResetFences);
    const Result result    = DeviceDecorator::ResetFences(fenceCount, ppFences);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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
    uint32                   fenceCount,
    const IFence*const*      ppFences,
    bool                     waitAll,
    std::chrono::nanoseconds timeout
    ) const
{
    auto*const   pPlatform = static_cast<Platform*>(m_pPlatform);
    const bool   active    = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceWaitForFences);
    const Result result    = DeviceDecorator::WaitForFences(fenceCount, ppFences, waitAll, timeout);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndBeginList("fences", false);

        for (uint32 idx = 0; idx < fenceCount; ++idx)
        {
            pLogContext->Object(ppFences[idx]);
        }

        pLogContext->EndList();
        pLogContext->KeyAndValue("waitAll", waitAll);
        pLogContext->KeyAndValue("timeout", uint64(timeout.count()));
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
    const bool active    = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceBindTrapHandler);

    DeviceDecorator::BindTrapHandler(pipelineType, pGpuMemory, offset);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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
    const bool active    = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceBindTrapBuffer);

    DeviceDecorator::BindTrapBuffer(pipelineType, pGpuMemory, offset);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceCreateQueue);
    const Result result = m_pNextLayer->CreateQueue(createInfo,
                                                    NextObjectAddr<Queue>(pPlacementAddr),
                                                    &pNextQueue);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextQueue != nullptr);
        pNextQueue->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::Queue);

        (*ppQueue) = PAL_PLACEMENT_NEW(pPlacementAddr) Queue(pNextQueue, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceCreateMultiQueue);
    const Result result = m_pNextLayer->CreateMultiQueue(queueCount,
                                                         pCreateInfo,
                                                         NextObjectAddr<Queue>(pPlacementAddr),
                                                         &pNextQueue);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextQueue != nullptr);
        pNextQueue->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::Queue);

        (*ppQueue) = PAL_PLACEMENT_NEW(pPlacementAddr) Queue(pNextQueue, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceCreateGpuMemory);
    const Result result = m_pNextLayer->CreateGpuMemory(nextCreateInfo,
                                                        NextObjectAddr<GpuMemory>(pPlacementAddr),
                                                        &pNextGpuMemory);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextGpuMemory != nullptr);
        pNextGpuMemory->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::GpuMemory);

        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pPlacementAddr) GpuMemory(pNextGpuMemory, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppGpuMemory);

        if (result == Result::Success)
        {
            PAL_ASSERT(*ppGpuMemory != nullptr);
            pLogContext->KeyAndStruct("GpuMemoryDesc", (*ppGpuMemory)->Desc());
        }

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceCreatePinnedGpuMemory);
    const Result result = m_pNextLayer->CreatePinnedGpuMemory(createInfo,
                                                              NextObjectAddr<GpuMemory>(pPlacementAddr),
                                                              &pNextMemObj);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextMemObj != nullptr);
        pNextMemObj->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::GpuMemory);

        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pPlacementAddr) GpuMemory(pNextMemObj, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppGpuMemory);

        if (result == Result::Success)
        {
            PAL_ASSERT(*ppGpuMemory != nullptr);
            pLogContext->KeyAndStruct("GpuMemoryDesc", (*ppGpuMemory)->Desc());
        }

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceCreateSvmGpuMemory);
    const Result result = m_pNextLayer->CreateSvmGpuMemory(nextCreateInfo,
                                                           NextObjectAddr<GpuMemory>(pPlacementAddr),
                                                           &pNextMemObj);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextMemObj != nullptr);
        pNextMemObj->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::GpuMemory);

        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pPlacementAddr) GpuMemory(pNextMemObj, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppGpuMemory);

        if (result == Result::Success)
        {
            PAL_ASSERT(*ppGpuMemory != nullptr);
            pLogContext->KeyAndStruct("GpuMemoryDesc", (*ppGpuMemory)->Desc());
        }

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceOpenSharedGpuMemory);
    const Result result = m_pNextLayer->OpenSharedGpuMemory(nextOpenInfo,
                                                            NextObjectAddr<GpuMemory>(pPlacementAddr),
                                                            &pNextMemObj);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextMemObj != nullptr);
        pNextMemObj->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::GpuMemory);

        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pPlacementAddr) GpuMemory(pNextMemObj, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("openInfo", openInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppGpuMemory);

        if (result == Result::Success)
        {
            PAL_ASSERT(*ppGpuMemory != nullptr);
            pLogContext->KeyAndStruct("GpuMemoryDesc", (*ppGpuMemory)->Desc());
        }

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceOpenExternalSharedGpuMemory);
    const Result result = m_pNextLayer->OpenExternalSharedGpuMemory(openInfo,
                                                                    NextObjectAddr<GpuMemory>(pPlacementAddr),
                                                                    pMemCreateInfo,
                                                                    &pNextMemObj);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextMemObj != nullptr);
        pNextMemObj->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::GpuMemory);

        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pPlacementAddr) GpuMemory(pNextMemObj, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("openInfo", openInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppGpuMemory);
        pLogContext->KeyAndStruct("memCreateInfo", *pMemCreateInfo);

        if (result == Result::Success)
        {
            PAL_ASSERT(*ppGpuMemory != nullptr);
            pLogContext->KeyAndStruct("GpuMemoryDesc", (*ppGpuMemory)->Desc());
        }

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceOpenPeerGpuMemory);
    const Result result = m_pNextLayer->OpenPeerGpuMemory(nextOpenInfo,
                                                          NextObjectAddr<GpuMemory>(pPlacementAddr),
                                                          &pNextMemObj);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextMemObj != nullptr);
        pNextMemObj->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::GpuMemory);

        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pPlacementAddr) GpuMemory(pNextMemObj, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("openInfo", openInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppGpuMemory);

        if (result == Result::Success)
        {
            PAL_ASSERT(*ppGpuMemory != nullptr);
            pLogContext->KeyAndStruct("GpuMemoryDesc", (*ppGpuMemory)->Desc());
        }

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceCreateImage);
    const Result result = m_pNextLayer->CreateImage(createInfo,
                                                    NextObjectAddr<Image>(pPlacementAddr),
                                                    &pNextImage);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextImage != nullptr);
        pNextImage->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::Image);

        (*ppImage) = PAL_PLACEMENT_NEW(pPlacementAddr) Image(pNextImage, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppImage);
        pLogContext->EndOutput();

        if (result == Result::Success)
        {
            pLogContext->KeyAndStruct("ImageMemoryLayout", (*ppImage)->GetMemoryLayout());
        }

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceCreatePresentableImage);
    const Result result = m_pNextLayer->CreatePresentableImage(nextCreateInfo,
                                                               NextObjectAddr<Image>(pImagePlacementAddr),
                                                               NextObjectAddr<GpuMemory>(pGpuMemoryPlacementAddr),
                                                               &pNextImage,
                                                               &pNextGpuMemory);

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

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdImageObj", *ppImage);
        pLogContext->KeyAndObject("createdGpuMemoryObj", *ppGpuMemory);

        if (result == Result::Success)
        {
            PAL_ASSERT(*ppGpuMemory != nullptr);
            pLogContext->KeyAndStruct("GpuMemoryDesc", (*ppGpuMemory)->Desc());
        }

        pLogContext->EndOutput();

        if (result == Result::Success)
        {
            pLogContext->KeyAndStruct("ImageMemoryLayout", (*ppImage)->GetMemoryLayout());
        }

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceOpenPeerImage);
    const Result result = m_pNextLayer->OpenPeerImage(nextOpenInfo,
                                                      NextObjectAddr<Image>(pImagePlacementAddr),
                                                      NextObjectAddr<GpuMemory>(pGpuMemoryPlacementAddr),
                                                      &pNextImage,
                                                      &pNextGpuMemory);

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

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("openInfo", openInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdImageObj", *ppImage);
        pLogContext->KeyAndObject("createdGpuMemoryObj", *ppGpuMemory);

        pLogContext->EndOutput();

        if (result == Result::Success)
        {
            PAL_ASSERT(*ppGpuMemory != nullptr);
            pLogContext->KeyAndStruct("GpuMemoryDesc", (*ppGpuMemory)->Desc());
            pLogContext->KeyAndStruct("ImageMemoryLayout", (*ppImage)->GetMemoryLayout());
        }

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceOpenExternalSharedImage);
    const Result result = m_pNextLayer->OpenExternalSharedImage(openInfo,
                                                                NextObjectAddr<Image>(pImagePlacementAddr),
                                                                NextObjectAddr<GpuMemory>(pGpuMemoryPlacementAddr),
                                                                pMemCreateInfo,
                                                                &pNextImage,
                                                                &pNextGpuMemory);

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

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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

        if (result == Result::Success)
        {
            PAL_ASSERT(*ppGpuMemory != nullptr);
            pLogContext->KeyAndStruct("GpuMemoryDesc", (*ppGpuMemory)->Desc());
        }

        pLogContext->EndOutput();

        if (result == Result::Success)
        {
            pLogContext->KeyAndStruct("ImageMemoryLayout", (*ppImage)->GetMemoryLayout());
        }

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceCreateColorTargetView);
    const Result result = m_pNextLayer->CreateColorTargetView(nextCreateInfo,
                                                              NextObjectAddr<ColorTargetView>(pPlacementAddr),
                                                              &pNextView);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextView != nullptr);
        pNextView->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::ColorTargetView);

        (*ppColorTargetView) =
            PAL_PLACEMENT_NEW(pPlacementAddr) ColorTargetView(pNextView, createInfo, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceCreateDepthStencilView);
    const Result result = m_pNextLayer->CreateDepthStencilView(nextCreateInfo,
                                                               NextObjectAddr<DepthStencilView>(pPlacementAddr),
                                                               &pNextView);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextView != nullptr);
        pNextView->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::DepthStencilView);

        (*ppDepthStencilView) =
            PAL_PLACEMENT_NEW(pPlacementAddr) DepthStencilView(pNextView, nextCreateInfo, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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
    auto*const   pPlatform = static_cast<Platform*>(m_pPlatform);
    const bool   active    = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceSetSamplePatternPalette);
    const Result result    = DeviceDecorator::SetSamplePatternPalette(palette);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceCreateBorderColorPalette);
    const Result result = m_pNextLayer->CreateBorderColorPalette(createInfo,
                                                                 NextObjectAddr<BorderColorPalette>(pPlacementAddr),
                                                                 &pNextPalette);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextPalette != nullptr);
        pNextPalette->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::BorderColorPalette);

        (*ppPalette) = PAL_PLACEMENT_NEW(pPlacementAddr) BorderColorPalette(pNextPalette, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceCreateComputePipeline);
    const Result result = m_pNextLayer->CreateComputePipeline(createInfo,
                                                              NextObjectAddr<Pipeline>(pPlacementAddr),
                                                              &pNextPipeline);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextPipeline != nullptr);
        pNextPipeline->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::Pipeline);

        (*ppPipeline) = PAL_PLACEMENT_NEW(pPlacementAddr) Pipeline(pNextPipeline, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppPipeline);

        size_t numEntries;
        GpuMemSubAllocInfo subAllocInfo{ };
        Result allocResult = pNextPipeline->QueryAllocationInfo(&numEntries, nullptr);
        if (allocResult == Result::Success)
        {
            // pipelines always return 1
            PAL_ASSERT(numEntries == 1);
            allocResult = pNextPipeline->QueryAllocationInfo(&numEntries, &subAllocInfo);
        }
        if (allocResult == Result::Success)
        {
            pLogContext->KeyAndStruct("gpuMemSubAllocInfo", subAllocInfo);
        }

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceCreateGraphicsPipeline);
    const Result result = CallNextCreateGraphicsPipeline(createInfo,
                                                         NextObjectAddr<Pipeline>(pPlacementAddr),
                                                         &pNextPipeline);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextPipeline != nullptr);
        pNextPipeline->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::Pipeline);

        (*ppPipeline) = PAL_PLACEMENT_NEW(pPlacementAddr) Pipeline(pNextPipeline, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppPipeline);

        size_t numEntries;
        GpuMemSubAllocInfo subAllocInfo{ };
        Result allocResult = pNextPipeline->QueryAllocationInfo(&numEntries, nullptr);
        if (allocResult == Result::Success)
        {
            // pipelines always return 1
            PAL_ASSERT(numEntries == 1);
            allocResult = pNextPipeline->QueryAllocationInfo(&numEntries, &subAllocInfo);
        }
        if (allocResult == Result::Success)
        {
            pLogContext->KeyAndStruct("gpuMemSubAllocInfo", subAllocInfo);
        }

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceCreateShaderLibrary);
    const Result result = m_pNextLayer->CreateShaderLibrary(createInfo,
                                                            NextObjectAddr<ShaderLibrary>(pPlacementAddr),
                                                            &pLibrary);

    if (result == Result::Success)
    {
        PAL_ASSERT(pLibrary != nullptr);
        pLibrary->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::ShaderLibrary);

        (*ppLibrary) = PAL_PLACEMENT_NEW(pPlacementAddr) ShaderLibrary(pLibrary, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("createInfo", createInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->KeyAndObject("createdObj", *ppLibrary);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 827
        if (result == Result::Success)
        {
            pLogContext->KeyAndBeginList("functions", false);
            for (uint32 i = 0; i < createInfo.funcCount; ++i)
            {
                pLogContext->Struct(createInfo.pFuncList[i]);
            }
            pLogContext->EndList();
        }
#endif
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetMsaaStateSize() const
{
    return m_pNextLayer->GetMsaaStateSize() + sizeof(MsaaState);
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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceCreateMsaaState);
    const Result result = m_pNextLayer->CreateMsaaState(createInfo,
                                                        NextObjectAddr<MsaaState>(pPlacementAddr),
                                                        &pNextState);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextState != nullptr);
        pNextState->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::MsaaState);

        (*ppMsaaState) = PAL_PLACEMENT_NEW(pPlacementAddr) MsaaState(pNextState, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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
size_t Device::GetColorBlendStateSize() const
{
    return m_pNextLayer->GetColorBlendStateSize() + sizeof(ColorBlendState);
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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceCreateColorBlendState);
    const Result result = m_pNextLayer->CreateColorBlendState(createInfo,
                                                              NextObjectAddr<ColorBlendState>(pPlacementAddr),
                                                              &pNextState);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextState != nullptr);
        pNextState->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::ColorBlendState);

        (*ppColorBlendState) = PAL_PLACEMENT_NEW(pPlacementAddr) ColorBlendState(pNextState, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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
size_t Device::GetDepthStencilStateSize() const
{
    return m_pNextLayer->GetDepthStencilStateSize() + sizeof(DepthStencilState);
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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceCreateDepthStencilState);
    const Result result = m_pNextLayer->CreateDepthStencilState(createInfo,
                                                                NextObjectAddr<DepthStencilState>(pPlacementAddr),
                                                                &pNextState);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextState != nullptr);
        pNextState->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::DepthStencilState);

        (*ppDepthStencilState) = PAL_PLACEMENT_NEW(pPlacementAddr) DepthStencilState(pNextState, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceCreateQueueSemaphore);
    const Result result = m_pNextLayer->CreateQueueSemaphore(createInfo,
                                                             NextObjectAddr<QueueSemaphore>(pPlacementAddr),
                                                             &pNextSemaphore);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextSemaphore != nullptr);
        pNextSemaphore->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::QueueSemaphore);

        (*ppQueueSemaphore) = PAL_PLACEMENT_NEW(pPlacementAddr) QueueSemaphore(pNextSemaphore, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceOpenSharedQueueSemaphore);
    const Result result = m_pNextLayer->OpenSharedQueueSemaphore(nextOpenInfo,
                                                                 NextObjectAddr<QueueSemaphore>(pPlacementAddr),
                                                                 &pNextSemaphore);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextSemaphore != nullptr);
        pNextSemaphore->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::QueueSemaphore);

        (*ppQueueSemaphore) = PAL_PLACEMENT_NEW(pPlacementAddr) QueueSemaphore(pNextSemaphore, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceOpenExternalSharedQueueSemaphore);
    const Result result = m_pNextLayer->OpenExternalSharedQueueSemaphore(openInfo,
                                                                         NextObjectAddr<QueueSemaphore>(pPlacementAddr),
                                                                         &pNextSemaphore);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextSemaphore != nullptr);
        pNextSemaphore->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::QueueSemaphore);

        (*ppQueueSemaphore) = PAL_PLACEMENT_NEW(pPlacementAddr) QueueSemaphore(pNextSemaphore, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceCreateFence);
    const Result result = m_pNextLayer->CreateFence(createInfo,
                                                    NextObjectAddr<Fence>(pPlacementAddr),
                                                    &pNextFence);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextFence != nullptr);
        pNextFence->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::Fence);

        (*ppFence) = PAL_PLACEMENT_NEW(pPlacementAddr) Fence(pNextFence, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceOpenFence);
    const Result result = m_pNextLayer->OpenFence(openInfo,
                                                  NextObjectAddr<Fence>(pPlacementAddr),
                                                  &pNextFence);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextFence != nullptr);
        pNextFence->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::Fence);

        (*ppFence) = PAL_PLACEMENT_NEW(pPlacementAddr) Fence(pNextFence, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceCreateGpuEvent);
    const Result result = m_pNextLayer->CreateGpuEvent(createInfo,
                                                       NextObjectAddr<GpuEvent>(pPlacementAddr),
                                                       &pNextGpuEvent);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextGpuEvent != nullptr);
        pNextGpuEvent->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::GpuEvent);

        (*ppGpuEvent) = PAL_PLACEMENT_NEW(pPlacementAddr) GpuEvent(pNextGpuEvent, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceCreateQueryPool);
    const Result result = m_pNextLayer->CreateQueryPool(createInfo,
                                                        NextObjectAddr<QueryPool>(pPlacementAddr),
                                                        &pNextQueryPool);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextQueryPool != nullptr);
        pNextQueryPool->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::QueryPool);

        (*ppQueryPool) = PAL_PLACEMENT_NEW(pPlacementAddr) QueryPool(pNextQueryPool, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceCreateCmdAllocator);
    const Result result = m_pNextLayer->CreateCmdAllocator(createInfo,
                                                           NextObjectAddr<CmdAllocator>(pPlacementAddr),
                                                           &pNextCmdAllocator);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextCmdAllocator != nullptr);
        pNextCmdAllocator->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::CmdAllocator);

        (*ppCmdAllocator) = PAL_PLACEMENT_NEW(pPlacementAddr) CmdAllocator(pNextCmdAllocator, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceCreateCmdBuffer);
    const Result result = m_pNextLayer->CreateCmdBuffer(nextCreateInfo,
                                                        NextObjectAddr<CmdBuffer>(pPlacementAddr),
                                                        &pNextCmdBuffer);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextCmdBuffer != nullptr);
        pNextCmdBuffer->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::CmdBuffer);

        (*ppCmdBuffer) = PAL_PLACEMENT_NEW(pPlacementAddr) CmdBuffer(pNextCmdBuffer, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceCreateIndirectCmdGenerator);
    const Result result = m_pNextLayer->CreateIndirectCmdGenerator(createInfo,
                                                                   NextObjectAddr<IndirectCmdGenerator>(pPlacementAddr),
                                                                   &pNextGenerator);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextGenerator != nullptr);
        pNextGenerator->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::IndirectCmdGenerator);

        (*ppGenerator) = PAL_PLACEMENT_NEW(pPlacementAddr) IndirectCmdGenerator(pNextGenerator, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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
    auto*const   pPlatform = static_cast<Platform*>(m_pPlatform);
    const bool   active    = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceGetPrivateScreens);
    const Result result    = DeviceDecorator::GetPrivateScreens(pNumScreens, ppScreens);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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

    auto*const   pPlatform = static_cast<Platform*>(m_pPlatform);
    const bool   active    = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceAddEmulatedPrivateScreen);
    const Result result    = DeviceDecorator::AddEmulatedPrivateScreen(createInfo, pTargetId);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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
    auto*const   pPlatform = static_cast<Platform*>(m_pPlatform);
    const bool   active    = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceRemoveEmulatedPrivateScreen);
    const Result result    = DeviceDecorator::RemoveEmulatedPrivateScreen(targetId);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceCreatePrivateScreenImage);
    const Result result = m_pNextLayer->CreatePrivateScreenImage(nextCreateInfo,
                                                                 NextObjectAddr<Image>(pImagePlacementAddr),
                                                                 NextObjectAddr<GpuMemory>(pGpuMemoryPlacementAddr),
                                                                 &pNextImage,
                                                                 &pNextGpuMemory);

    if (result == Result::Success)
    {
        pNextImage->SetClientData(pImagePlacementAddr);
        pNextGpuMemory->SetClientData(pGpuMemoryPlacementAddr);

        const uint32 imageId     = pPlatform->NewObjectId(InterfaceObject::Image);
        const uint32 gpuMemoryId = pPlatform->NewObjectId(InterfaceObject::GpuMemory);

        (*ppImage)     = PAL_PLACEMENT_NEW(pImagePlacementAddr) Image(pNextImage, this, imageId);
        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pGpuMemoryPlacementAddr) GpuMemory(pNextGpuMemory, this, gpuMemoryId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceCreateSwapChain);
    const Result result = m_pNextLayer->CreateSwapChain(createInfo,
                                                        NextObjectAddr<SwapChain>(pPlacementAddr),
                                                        &pNextSwapChain);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextSwapChain != nullptr);
        pNextSwapChain->SetClientData(pPlacementAddr);

        const uint32 objectId = pPlatform->NewObjectId(InterfaceObject::SwapChain);

        (*ppSwapChain) = PAL_PLACEMENT_NEW(pPlacementAddr) SwapChain(pNextSwapChain, this, objectId);
    }

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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
    auto*const   pPlatform = static_cast<Platform*>(m_pPlatform);
    const bool   active    = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceSetPowerProfile);
    const Result result    = DeviceDecorator::SetPowerProfile(profile, pInfo);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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
    auto*const   pPlatform = static_cast<Platform*>(m_pPlatform);
    const bool   active    = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceFlglQueryState);
    const Result result    = m_pNextLayer->FlglQueryState(pState);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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
Result Device::FlglSetSyncConfiguration(
    const GlSyncConfig& glSyncConfig)
{
    auto* const  pPlatform = static_cast<Platform*>(m_pPlatform);
    const bool   active    = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceFlglSetSyncConfiguration);
    const Result result    = m_pNextLayer->FlglSetSyncConfiguration(glSyncConfig);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("glSyncConfig", glSyncConfig);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Device::FlglGetSyncConfiguration(
    GlSyncConfig* pGlSyncConfig
    ) const
{
    auto* const  pPlatform = static_cast<Platform*>(m_pPlatform);
    const bool   active    = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceFlglGetSyncConfiguration);
    const Result result    = m_pNextLayer->FlglGetSyncConfiguration(pGlSyncConfig);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);

        if (pGlSyncConfig != nullptr)
        {
            pLogContext->KeyAndStruct("pGlSyncConfig", *pGlSyncConfig);
        }
        else
        {
            pLogContext->KeyAndNullValue("pGlSyncConfig");
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
    auto*const   pPlatform = static_cast<Platform*>(m_pPlatform);
    const bool   active    = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceFlglSetFrameLock);
    const Result result    = m_pNextLayer->FlglSetFrameLock(enable);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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
Result Device::FlglSetGenLock(
    bool enable)
{
    auto* const  pPlatform = static_cast<Platform*>(m_pPlatform);
    const bool   active    = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceFlglSetGenLock);
    const Result result    = m_pNextLayer->FlglSetGenLock(enable);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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
    auto*const   pPlatform = static_cast<Platform*>(m_pPlatform);
    const bool   active    = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceFlglResetFrameCounter);
    const Result result    = m_pNextLayer->FlglResetFrameCounter();

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Device::FlglGetFrameCounter(
    uint64* pValue,
    bool*   pReset
    ) const
{
    auto*const   pPlatform = static_cast<Platform*>(m_pPlatform);
    const bool   active    = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceFlglGetFrameCounter);
    const Result result    = m_pNextLayer->FlglGetFrameCounter(pValue, pReset);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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
Result Device::FlglGetFrameCounterResetStatus(
    bool* pReset
    ) const
{
    auto*const pPlatform = static_cast<Platform*>(m_pPlatform);

    const bool   active = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceFlglGetFrameCounterResetStatus);
    const Result result = m_pNextLayer->FlglGetFrameCounterResetStatus(pReset);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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

    const bool active = pPlatform->ActivateLogging(pThis->m_objectId, InterfaceFunc::DeviceCreateTypedBufferViewSrds);

    DeviceDecorator::DecoratorCreateTypedBufViewSrds(pDevice, count, pBufferViewInfo, pOut);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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

    const bool active = pPlatform->ActivateLogging(pThis->m_objectId, InterfaceFunc::DeviceCreateUntypedBufferViewSrds);

    DeviceDecorator::DecoratorCreateUntypedBufViewSrds(pDevice, count, pBufferViewInfo, pOut);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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
    const bool  active    = pPlatform->ActivateLogging(pThis->m_objectId, InterfaceFunc::DeviceCreateImageViewSrds);

    DeviceDecorator::DecoratorCreateImageViewSrds(pDevice, count, pImgViewInfo, pOut);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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
    const bool  active    = pPlatform->ActivateLogging(pThis->m_objectId, InterfaceFunc::DeviceCreateFmaskViewSrds);

    DeviceDecorator::DecoratorCreateFmaskViewSrds(pDevice, count, pFmaskViewInfo, pOut);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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
    const bool  active    = pPlatform->ActivateLogging(pThis->m_objectId, InterfaceFunc::DeviceCreateSamplerSrds);

    DeviceDecorator::DecoratorCreateSamplerSrds(pDevice, count, pSamplerInfo, pOut);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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
void PAL_STDCALL Device::CreateBvhSrds(
    const IDevice*  pDevice,
    uint32          count,
    const BvhInfo*  pBvhInfo,
    void*           pOut)
{
    const auto* pThis     = static_cast<const Device*>(pDevice);
    auto*const  pPlatform = static_cast<Platform*>(pThis->m_pPlatform);
    const bool  active    = pPlatform->ActivateLogging(pThis->m_objectId, InterfaceFunc::DeviceCreateBvhSrds);

    DeviceDecorator::DecoratorCreateBvhSrds(pDevice, count, pBvhInfo, pOut);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndBeginList("bvhInfo", false);

        for (uint32 idx = 0; idx < count; ++idx)
        {
            pLogContext->Struct(pBvhInfo[idx]);
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
    auto*const   pPlatform = static_cast<Platform*>(m_pPlatform);
    const bool   active    = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceCreateVirtualDisplay);
    const Result result    = m_pNextLayer->CreateVirtualDisplay(virtualDisplayInfo, pScreenTargetId);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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
    auto*const   pPlatform = static_cast<Platform*>(m_pPlatform);
    const bool   active    = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceDestroyVirtualDisplay);
    const Result result    = m_pNextLayer->DestroyVirtualDisplay(screenTargetId);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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
    auto*const   pPlatform = static_cast<Platform*>(m_pPlatform);
    const bool   active    = pPlatform->ActivateLogging(m_objectId, InterfaceFunc::DeviceGetVirtualDisplayProperties);
    const Result result    = m_pNextLayer->GetVirtualDisplayProperties(screenTargetId, pProperties);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

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
