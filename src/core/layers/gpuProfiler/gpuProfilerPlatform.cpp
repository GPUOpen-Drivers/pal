/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/layers/gpuProfiler/gpuProfilerCmdBuffer.h"
#include "core/layers/gpuProfiler/gpuProfilerDevice.h"
#include "core/layers/gpuProfiler/gpuProfilerPlatform.h"

#include "palSysUtil.h"

#include <cstring>

using namespace Util;

namespace Pal
{
namespace GpuProfiler
{

// =====================================================================================================================
Platform::Platform(
    const PlatformCreateInfo&   createInfo,
    const Util::AllocCallbacks& allocCb,
    IPlatform*                  pNextPlatform,
    GpuProfilerMode             mode)
    :
    PlatformDecorator(createInfo,
                      allocCb,
                      GpuProfilerCb,
                      (mode != GpuProfilerDisabled),
                      (mode != GpuProfilerDisabled),
                      pNextPlatform),
    m_profilerMode(mode),
    m_pLogger(nullptr),
    m_frameId(0),
    m_forceLogging(false),
    m_apiMajorVer(createInfo.apiMajorVer),
    m_apiMinorVer(createInfo.apiMinorVer),
    m_universalQueueSequence(0)
{
}

// =====================================================================================================================
Result Platform::Create(
    const PlatformCreateInfo&   createInfo,
    const Util::AllocCallbacks& allocCb,
    IPlatform*                  pNextPlatform,
    GpuProfilerMode             mode,
    const char*                 pTargetApp,
    void*                       pPlacementAddr,
    IPlatform**                 ppPlatform)
{
    if (strlen(pTargetApp) > 0)
    {
        char  executableNameBuffer[256] = {};
        char* pExecutableName = nullptr;

        if (GetExecutableName(executableNameBuffer, &pExecutableName, sizeof(executableNameBuffer)) == Result::Success)
        {
            if (strcmp(pExecutableName, pTargetApp) != 0)
            {
                mode = GpuProfilerDisabled;
            }
        }
        else
        {
            PAL_ASSERT_ALWAYS_MSG("Unable to retrieve executable name to match against the Gpu Profiler target "
                                  "application name.");
        }
    }

    Platform* pPlatform = PAL_PLACEMENT_NEW(pPlacementAddr) Platform(createInfo, allocCb, pNextPlatform, mode);
    Result result = pPlatform->Init();

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
Result Platform::Init()
{
    return PlatformDecorator::Init();
}

// =====================================================================================================================
// Called by device creating a universal queue to identify a unique creation order
uint32 Platform::GetUniversalQueueSequenceNumber()
{
    return AtomicIncrement(&m_universalQueueSequence) - 1;
}

// =====================================================================================================================
// Called by queue anytime a present is performed.  This is managed by the platform since a present on any queue or
// device should advance the frame count for the entire platform.
void Platform::IncrementFrameId()
{
    // NOTE: There is a potential problem here for MGPU AFR situations.  Theoretically, the app could submit all work
    // for frame N+1 before issuing a present on frame N.  If that happens, the commands issued for frame N+1 will
    // be logged as part of frame N.  This hasn't been observed in practice, and would really only affect which files
    // the commands are logged in, but it is something to be aware of.
    m_frameId++;

    // Force logging on for the next frame if the user is currently holding the trigger key (defaults to Shift-F11).
    m_forceLogging = IsKeyPressed(PlatformSettings().gpuProfilerCaptureTriggerKey);
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
            m_pDevices[i] = PAL_NEW(Device, this, SystemAllocType::AllocObject)(this, pDevices[i], i);
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
    Result result;

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
// Callback for an image barrier event from an ICmdBuffer::CmdBarrier call.
void PAL_STDCALL Platform::GpuProfilerCb(
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
    case Developer::CallbackType::BarrierEnd:
    case Developer::CallbackType::ImageBarrier:
        if (TranslateBarrierEventData(pCbData))
        {
            Developer::BarrierData* pData      = static_cast<Developer::BarrierData*>(pCbData);
            CmdBufferDecorator*     pCmdBuffer = static_cast<CmdBufferDecorator*>(pData->pCmdBuffer);

            if (pCmdBuffer != nullptr)
            {
                pCmdBuffer->UpdateCommentString(pData);
            }
        }
        break;
    case Developer::CallbackType::DrawDispatch:
        TranslateDrawDispatchData(pCbData);
        break;
    case Developer::CallbackType::BindPipeline:
        TranslateBindPipelineData(pCbData);
        break;
#if PAL_DEVELOPER_BUILD
    case Developer::CallbackType::DrawDispatchValidation:
        TranslateDrawDispatchValidationData(pCbData);
        break;
    case Developer::CallbackType::BindPipelineValidation:
        TranslateBindPipelineValidationData(pCbData);
        break;
    case Developer::CallbackType::OptimizedRegisters:
        TranslateOptimizedRegistersData(pCbData);
        break;
    case Developer::CallbackType::RpmBlt:
        TranslateReportRpmBltTypeData(pCbData);
        break;
#endif
    case Developer::CallbackType::BindGpuMemory:
        TranslateBindGpuMemoryData(pCbData);
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    pPlatform->DeveloperCb(deviceIndex, type, pCbData);
}

} // GpuProfiler
} // Pal
