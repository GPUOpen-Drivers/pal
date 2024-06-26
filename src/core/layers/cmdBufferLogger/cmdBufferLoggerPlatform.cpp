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

#include "g_platformSettings.h"
#include "core/layers/cmdBufferLogger/cmdBufferLoggerCmdBuffer.h"
#include "core/layers/cmdBufferLogger/cmdBufferLoggerDevice.h"
#include "core/layers/cmdBufferLogger/cmdBufferLoggerPlatform.h"
#include "palSysUtil.h"
#include <ctime>

using namespace Util;

namespace Pal
{
namespace CmdBufferLogger
{

// =====================================================================================================================
Platform::Platform(
    const PlatformCreateInfo&   createInfo,
    const Util::AllocCallbacks& allocCb,
    IPlatform*                  pNextPlatform,
    bool                        enabled)
    :
    // CmdBufferLogger doesn't install callback
    PlatformDecorator(createInfo, allocCb, CmdBufferLoggerCb, enabled, enabled, pNextPlatform)
{
}

// =====================================================================================================================
Result Platform::Create(
    const PlatformCreateInfo&   createInfo,
    const Util::AllocCallbacks& allocCb,
    IPlatform*                  pNextPlatform,
    bool                        enabled,
    void*                       pPlacementAddr,
    IPlatform**                 ppPlatform)
{
    auto* pPlatform = PAL_PLACEMENT_NEW(pPlacementAddr) Platform(createInfo, allocCb, pNextPlatform, enabled);
    Result result   = pPlatform->Init();
    if (result == Result::Success)
    {
        (*ppPlatform) = pPlatform;
    }
    else
    {
        pPlatform->Destroy();
    }

    return result;
}

// =====================================================================================================================
Result Platform::EnumerateDevices(
    uint32*  pDeviceCount,
    IDevice* pDevices[MaxDevices])
{
    if (m_layerEnabled)
    {
        // We must tear down our GPUs before calling EnumerateDevices() because TearDownGpus() will call Cleanup()
        // which will destroy any state set by the lower layers in EnumerateDevices().
        TearDownGpus();
    }

    Result result = m_pNextLayer->EnumerateDevices(pDeviceCount, pDevices);

    if (m_layerEnabled && (result == Result::Success))
    {
        m_deviceCount = (*pDeviceCount);
        for (uint32 i = 0; i < m_deviceCount; i++)
        {
            m_pDevices[i] = PAL_NEW(Device, this, SystemAllocType::AllocObject)(this, pDevices[i]);
            pDevices[i]->SetClientData(m_pDevices[i]);
            pDevices[i]   = m_pDevices[i];

            if (m_pDevices[i] == nullptr)
            {
                result = Result::ErrorOutOfMemory;
                break;
            }
        }
    }

    return result;
}

// =====================================================================================================================
size_t Platform::GetScreenObjectSize() const
{
    size_t screenSize;

    // We only want to wrap the screen with a decorator when the layer is enabled.  Otherwise, just pass the call
    // through.  This is a consequence of the fact that the Platform object is always wrapped, regardless of whether
    // the layer is actually enabled or not.
    if (m_layerEnabled)
    {
        screenSize = PlatformDecorator::GetScreenObjectSize();
    }
    else
    {
        screenSize = m_pNextLayer->GetScreenObjectSize();
    }

    return screenSize;
}

// =====================================================================================================================
Result Platform::GetScreens(
    uint32*  pScreenCount,
    void*    pStorage[MaxScreens],
    IScreen* pScreens[MaxScreens])
{
    Result result = Result::Success;

    // We only want to wrap the screen with a decorator when the layer is enabled.  Otherwise, just pass the call
    // through.  This is a consequence of the fact that the Platform object is always wrapped, regardless of whether
    // the layer is actually enabled or not.
    if (m_layerEnabled)
    {
        result = PlatformDecorator::GetScreens(pScreenCount, pStorage, pScreens);
    }
    else
    {
        result = m_pNextLayer->GetScreens(pScreenCount, pStorage, pScreens);
    }

    return result;
}

// =====================================================================================================================
void PAL_STDCALL Platform::CmdBufferLoggerCb(
    void*                   pPrivateData,
    const uint32            deviceIndex,
    Developer::CallbackType type,
    void*                   pCbData)
{
    PAL_ASSERT(pPrivateData != nullptr);
    Platform* pPlatform = static_cast<Platform*>(pPrivateData);

    switch (type)
    {
    case Developer::CallbackType::AllocGpuMemory:
    case Developer::CallbackType::FreeGpuMemory:
    case Developer::CallbackType::SubAllocGpuMemory:
    case Developer::CallbackType::SubFreeGpuMemory:
        TranslateGpuMemoryData(pCbData);
        break;
    case Developer::CallbackType::PresentConcluded:
    case Developer::CallbackType::CreateImage:
    case Developer::CallbackType::SurfRegData:
        break;
    case Developer::CallbackType::BarrierBegin:
        TranslateBarrierEventData(pCbData);
        break;
    case Developer::CallbackType::BarrierEnd:
        if (TranslateBarrierEventData(pCbData))
        {
            Developer::BarrierData* pData      = static_cast<Developer::BarrierData*>(pCbData);
            CmdBuffer*              pCmdBuffer = static_cast<CmdBuffer*>(pData->pCmdBuffer);

            pCmdBuffer->DescribeBarrier(pData, "BarrierEnd:");
        }
        break;
    case Developer::CallbackType::ImageBarrier:
        if (TranslateBarrierEventData(pCbData))
        {
            Developer::BarrierData* pData      = static_cast<Developer::BarrierData*>(pCbData);
            CmdBuffer*              pCmdBuffer = static_cast<CmdBuffer*>(pData->pCmdBuffer);

            pCmdBuffer->DescribeBarrier(pData, "ImageBarrier:");
        }
        break;
    case Developer::CallbackType::DrawDispatch:
        if (TranslateDrawDispatchData(pCbData))
        {
            Developer::DrawDispatchData* pData = static_cast<Developer::DrawDispatchData*>(pCbData);
            CmdBuffer* pCmdBuffer = static_cast<CmdBuffer*>(pData->pCmdBuffer);

            pCmdBuffer->AddDrawDispatchInfo(pData->cmdType);
        }
        break;
    case Developer::CallbackType::BindPipeline:
        if (TranslateBindPipelineData(pCbData))
        {
            Developer::BindPipelineData* pData      = static_cast<Developer::BindPipelineData*>(pCbData);
            CmdBuffer*                   pCmdBuffer = static_cast<CmdBuffer*>(pData->pCmdBuffer);

            pCmdBuffer->UpdateDrawDispatchInfo(pData->pPipeline, pData->bindPoint, pData->apiPsoHash);
        }
        break;
    case Developer::CallbackType::DrawDispatchValidation:
        TranslateDrawDispatchValidationData(pCbData);
        break;
    case Developer::CallbackType::BindPipelineValidation:
        TranslateBindPipelineValidationData(pCbData);
        break;
    case Developer::CallbackType::OptimizedRegisters:
        TranslateOptimizedRegistersData(pCbData);
        break;
    case Developer::CallbackType::BindGpuMemory:
        TranslateBindGpuMemoryData(pCbData);
        break;
    case Developer::CallbackType::RpmBlt:
        TranslateReportRpmBltTypeData(pCbData);
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    pPlatform->DeveloperCb(deviceIndex, type, pCbData);
}

} // CmdBufferLogger
} // Pal

#endif
