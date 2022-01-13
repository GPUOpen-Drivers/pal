/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/g_palPlatformSettings.h"
#include "core/layers/pm4Instrumentor/pm4InstrumentorCmdBuffer.h"
#include "core/layers/pm4Instrumentor/pm4InstrumentorDevice.h"
#include "core/layers/pm4Instrumentor/pm4InstrumentorPlatform.h"
#include "palSysUtil.h"

using namespace Util;

namespace Pal
{
namespace Pm4Instrumentor
{

// =====================================================================================================================
static void PAL_STDCALL Pm4InstrumentorCb(
    void*                   pPrivateData,
    uint32                  deviceIndex,
    Developer::CallbackType type,
    void*                   pCbData)
{
    PAL_ASSERT(pPrivateData != nullptr);
    auto*const pThis = static_cast<Platform*>(pPrivateData);

    switch (type)
    {
    case Developer::CallbackType::AllocGpuMemory:
    case Developer::CallbackType::FreeGpuMemory:
    case Developer::CallbackType::SurfRegData:
        break;
    case Developer::CallbackType::PresentConcluded:
        pThis->NotifyPresentOcurred();
        break;
    case Developer::CallbackType::CreateImage:
        break;
    case Developer::CallbackType::BarrierBegin:
    case Developer::CallbackType::BarrierEnd:
    case Developer::CallbackType::ImageBarrier:
        PAL_ASSERT(pCbData != nullptr);
        TranslateBarrierEventData(pCbData);
        break;
    case Developer::CallbackType::DrawDispatch:
        PAL_ASSERT(pCbData != nullptr);
        TranslateDrawDispatchData(pCbData);
        break;
    case Developer::CallbackType::BindPipeline:
        PAL_ASSERT(pCbData != nullptr);
        TranslateBindPipelineData(pCbData);
        break;
    case Developer::CallbackType::DrawDispatchValidation:
        PAL_ASSERT(pCbData != nullptr);
        if (TranslateDrawDispatchValidationData(pCbData))
        {
            const auto& data    = *static_cast<Developer::DrawDispatchValidationData*>(pCbData);
            auto*const  pCmdBuf = static_cast<CmdBuffer*>(data.pCmdBuffer);

            pCmdBuf->NotifyDrawDispatchValidation(data);
        }
        break;
    case Developer::CallbackType::OptimizedRegisters:
        PAL_ASSERT(pCbData != nullptr);
        if (TranslateOptimizedRegistersData(pCbData))
        {
            const auto& data    = *static_cast<Developer::OptimizedRegistersData*>(pCbData);
            auto*const  pCmdBuf = static_cast<CmdBuffer*>(data.pCmdBuffer);

            pCmdBuf->UpdateOptimizedRegisters(data);
        }
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    pThis->DeveloperCb(deviceIndex, type, pCbData);
}

// =====================================================================================================================
Platform::Platform(
    const PlatformCreateInfo&   createInfo,
    const Util::AllocCallbacks& allocCb,
    IPlatform*                  pNextPlatform,
    bool                        enabled)
    :
    PlatformDecorator(createInfo, allocCb, Pm4InstrumentorCb, enabled, enabled, pNextPlatform),
    m_frameCount(0)
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
    auto*const pPlatform = PAL_PLACEMENT_NEW(pPlacementAddr) Platform(createInfo, allocCb, pNextPlatform, enabled);
    Result result        = pPlatform->Init();

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
    // We only want to wrap the screen with a decorator when the layer is enabled.  Otherwise, just pass the call
    // through.  This is a consequence of the fact that the Platform object is always wrapped, regardless of whether
    // the layer is actually enabled or not.
    return (m_layerEnabled) ? PlatformDecorator::GetScreenObjectSize() : m_pNextLayer->GetScreenObjectSize();
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

} // Pm4Instrumentor
} // Pal

#endif
