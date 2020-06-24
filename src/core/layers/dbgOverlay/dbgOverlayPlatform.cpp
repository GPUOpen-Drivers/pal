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

#include "core/layers/dbgOverlay/dbgOverlayDevice.h"
#include "core/layers/dbgOverlay/dbgOverlayFpsMgr.h"
#include "core/layers/dbgOverlay/dbgOverlayPlatform.h"

using namespace Util;

namespace Pal
{
namespace DbgOverlay
{

// =====================================================================================================================
Result Platform::Create(
    const Util::AllocCallbacks& allocCb,
    IPlatform*                  pNextPlatform,
    bool                        enabled,
    void*                       pPlacementAddr,
    IPlatform**                 ppPlatform)
{
    Result result   = Result::ErrorInitializationFailed;
    auto* pPlatform = PAL_PLACEMENT_NEW(pPlacementAddr) Platform(allocCb, pNextPlatform, enabled);

    if (pPlatform != nullptr)
    {
        result = pPlatform->Init();
    }

    if (result == Result::Success)
    {
        (*ppPlatform) = pPlatform;
    }

    return result;
}

// =====================================================================================================================
Result Platform::EnumerateDevices(
    uint32*    pDeviceCount,
    IDevice*   pDevices[MaxDevices])
{
    if (m_layerEnabled)
    {
        // We must tear down our GPUs before calling EnumerateDevices() because TearDownGpus() will call Cleanup()
        // which will destroy any state set by the lower layers in EnumerateDevices().
        TearDownGpus();
    }

    Result result = m_pNextLayer->EnumerateDevices(pDeviceCount, pDevices);

    if (m_layerEnabled)
    {
        // Update our local copy of the platform's properties.
        if (result == Result::Success)
        {
            result = m_pNextLayer->GetProperties(&m_properties);
        }

        if (result == Result::Success)
        {
            m_deviceCount = (*pDeviceCount);
            for (uint32 gpu = 0; gpu < m_deviceCount; gpu++)
            {
                m_pDevices[gpu] = PAL_NEW(Device, this, SystemAllocType::AllocObject)(this, pDevices[gpu]);
                pDevices[gpu]->SetClientData(m_pDevices[gpu]);
                pDevices[gpu]   = m_pDevices[gpu];

                if (m_pDevices[gpu] == nullptr)
                {
                    result = Result::ErrorOutOfMemory;
                    break;
                }
            }
        }

        if ((result == Result::Success) && (m_deviceCount > 0))
        {
            // Create an FPS manager if we don't have one, otherwise just update the settings of the existing manager.
            if (m_pFpsMgr == nullptr)
            {
                m_pFpsMgr = PAL_NEW(FpsMgr,
                                    this,
                                    SystemAllocType::AllocInternal)(this, static_cast<Device*>(pDevices[0]));
                if (m_pFpsMgr == nullptr)
                {
                    result = Result::ErrorOutOfMemory;
                }
                else
                {
                    result = m_pFpsMgr->Init();
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
size_t Platform::GetScreenObjectSize() const
{
    size_t screenSize;

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
Platform::~Platform()
{
    if (m_pFpsMgr != nullptr)
    {
        PAL_SAFE_DELETE(m_pFpsMgr, this);
    }
}

// =====================================================================================================================
Device* Platform::GetDevice(
    uint32 deviceIndex)
{
    return static_cast<Device*>(m_pDevices[deviceIndex]);
}

// =====================================================================================================================
// Determines the allocation type for a GpuMemory-related event.
static AllocType PAL_INLINE DetermineAllocType(
    const Developer::GpuMemoryData& eventData)
{
    return eventData.flags.isVirtual                                  ? AllocTypeCount    :
           (eventData.flags.isClient || eventData.flags.isFlippable)  ? AllocTypeExternal :
           eventData.flags.isCmdAllocator                             ? AllocTypeCmdAlloc :
                                                                        AllocTypeInternal;
};

// =====================================================================================================================
// Callback for when a present is complete, or memory has been allocated/freed.
void PAL_STDCALL Platform::DbgOverlayCb(
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
    {
        PAL_ASSERT(pCbData != nullptr);
        const Developer::GpuMemoryData& data      = *static_cast<const Developer::GpuMemoryData*>(pCbData);
        AllocType                       allocType = DetermineAllocType(data);

        if (allocType != AllocTypeCount)
        {
            pPlatform->GetDevice(deviceIndex)->AddAllocatedVidMem(allocType, data.heap, data.size);
        }
        break;
    }
    case Developer::CallbackType::FreeGpuMemory:
    {
        PAL_ASSERT(pCbData != nullptr);
        const Developer::GpuMemoryData& data      = *static_cast<const Developer::GpuMemoryData*>(pCbData);
        AllocType                       allocType = DetermineAllocType(data);

        if (allocType != AllocTypeCount)
        {
            pPlatform->GetDevice(deviceIndex)->SubFreedVidMem(allocType, data.heap, data.size);
        }
        break;
    }
    case Developer::CallbackType::PresentConcluded:
    {
        FpsMgr* pFpsMgr = pPlatform->GetFpsMgr();

        if (pFpsMgr != nullptr)
        {
            pFpsMgr->UpdateFps();
            pFpsMgr->UpdateGpuFps();
            pFpsMgr->UpdateBenchmark();
        }
        break;
    }
    case Developer::CallbackType::ImageBarrier:
    case Developer::CallbackType::BarrierBegin:
    case Developer::CallbackType::BarrierEnd:
        PAL_ASSERT(pCbData != nullptr);
        TranslateBarrierEventData(pCbData);
        break;
    case Developer::CallbackType::CreateImage:
        break;
    case Developer::CallbackType::DrawDispatch:
        PAL_ASSERT(pCbData != nullptr);
        TranslateDrawDispatchData(pCbData);
        break;
    case Developer::CallbackType::BindPipeline:
        PAL_ASSERT(pCbData != nullptr);
        TranslateBindPipelineData(pCbData);
        break;
#if PAL_BUILD_PM4_INSTRUMENTOR
    case Developer::CallbackType::DrawDispatchValidation:
        PAL_ASSERT(pCbData != nullptr);
        TranslateDrawDispatchValidationData(pCbData);
        break;
    case Developer::CallbackType::OptimizedRegisters:
        PAL_ASSERT(pCbData != nullptr);
        TranslateOptimizedRegistersData(pCbData);
        break;
#endif
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    pPlatform->DeveloperCb(deviceIndex, type, pCbData);
}

// =====================================================================================================================
// Returns if this GPU is busy or idle.
bool Platform::GetGpuWork(
    uint32 deviceIndex)
{
    const MutexAuto lock(&m_gpuWorkLock);
    return m_gpuWork[deviceIndex];
}

// =====================================================================================================================
// Sets the GPU to busy or idle.
void Platform::SetGpuWork(
    uint32 deviceIndex,
    bool   isBusy)
{
    const MutexAuto lock(&m_gpuWorkLock);
    m_gpuWork[deviceIndex] = isBusy;
}

// =====================================================================================================================
// Resets all the gpuWork flag to idle (false).
void Platform::ResetGpuWork()
{
    const MutexAuto lock(&m_gpuWorkLock);
    memset(const_cast<bool*>(&m_gpuWork[0]), 0, sizeof(m_gpuWork));
}

} // DbgOverlay
} // Pal
