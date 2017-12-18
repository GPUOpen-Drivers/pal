/*
 *******************************************************************************
 *
 * Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "core/layers/gpuProfiler/gpuProfilerCmdBuffer.h"
#include "core/layers/gpuProfiler/gpuProfilerDevice.h"
#include "core/layers/gpuProfiler/gpuProfilerPlatform.h"
#include "palSysUtil.h"
#include <ctime>

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
    PlatformDecorator(allocCb,
                      GpuProfilerCb,
                      (mode != GpuProfilerDisabled),
                      (mode != GpuProfilerDisabled),
                      pNextPlatform),
    m_profilerMode(mode),
    m_frameId(0),
    m_forceLogging(false),
    m_apiMajorVer(createInfo.apiMajorVer),
    m_apiMinorVer(createInfo.apiMinorVer),
    m_pipelinePerfDataLock()
{
    // Create a directory name that will hold any dumped logs this session.  The name will be composed of the executable
    // name and current data/time, looking something like this: app.exe_2015-08-26_07.49.20.
    char  executableNameBuffer[256];
    char* pExecutableName;
    GetExecutableName(&executableNameBuffer[0], &pExecutableName, sizeof(executableNameBuffer));

    time_t rawTime;
    time(&rawTime);

    struct tm* pTimeInfo = localtime(&rawTime);
    m_time = *pTimeInfo;

    char dateTimeBuffer[64];
    strftime(&dateTimeBuffer[0], sizeof(dateTimeBuffer), "%Y-%m-%d_%H.%M.%S", pTimeInfo);

    Snprintf(&m_logDirName[0], sizeof(m_logDirName), "%s_%s", pExecutableName, &dateTimeBuffer[0]);
}

// =====================================================================================================================
Result Platform::Create(
    const PlatformCreateInfo&   createInfo,
    const Util::AllocCallbacks& allocCb,
    IPlatform*                  pNextPlatform,
    GpuProfilerMode             mode,
    void*                       pPlacementAddr,
    IPlatform**                 ppPlatform)
{
    Result result   = Result::ErrorInitializationFailed;
    auto* pPlatform = PAL_PLACEMENT_NEW(pPlacementAddr) Platform(createInfo, allocCb, pNextPlatform, mode);

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
Result Platform::Init()
{
    Result result = PlatformDecorator::Init();

    if (result == Result::Success)
    {
        result = m_pipelinePerfDataLock.Init();
    }

    if (m_layerEnabled && (result == Result::Success))
    {
        // Initialize the settings
        result = UpdateSettings();
    }

    return result;
}

// =====================================================================================================================
Result Platform::UpdateSettings()
{
    memset(m_profilerSettings.gpuProfilerLogDirectory, 0, 512);
    strncpy(m_profilerSettings.gpuProfilerLogDirectory, "/tmp/amdpal/", 512);
    m_profilerSettings.gpuProfilerStartFrame = 0;
    m_profilerSettings.gpuProfilerFrameCount = 0;
    m_profilerSettings.gpuProfilerRecordPipelineStats = false;
    memset(m_profilerSettings.gpuProfilerGlobalPerfCounterConfigFile, 0, 256);
    strncpy(m_profilerSettings.gpuProfilerGlobalPerfCounterConfigFile, "", 256);
    m_profilerSettings.gpuProfilerGlobalPerfCounterPerInstance = false;
    m_profilerSettings.gpuProfilerBreakSubmitBatches = false;
    m_profilerSettings.gpuProfilerCacheFlushOnCounterCollection = false;
    m_profilerSettings.gpuProfilerGranularity = GpuProfilerGranularityDraw;
    m_profilerSettings.gpuProfilerSqThreadTraceTokenMask = 0xFFFF;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION <= 297
    m_profilerSettings.gpuProfilerSqttPipelineHashHi = 0;
#endif
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION <= 297
    m_profilerSettings.gpuProfilerSqttPipelineHashLo = 0;
#endif
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 298
    m_profilerSettings.gpuProfilerSqttPipelineHash = 0;
#endif
    m_profilerSettings.gpuProfilerSqttVsHashHi = 0;
    m_profilerSettings.gpuProfilerSqttVsHashLo = 0;
    m_profilerSettings.gpuProfilerSqttHsHashHi = 0;
    m_profilerSettings.gpuProfilerSqttHsHashLo = 0;
    m_profilerSettings.gpuProfilerSqttDsHashHi = 0;
    m_profilerSettings.gpuProfilerSqttDsHashLo = 0;
    m_profilerSettings.gpuProfilerSqttGsHashHi = 0;
    m_profilerSettings.gpuProfilerSqttGsHashLo = 0;
    m_profilerSettings.gpuProfilerSqttPsHashHi = 0;
    m_profilerSettings.gpuProfilerSqttPsHashLo = 0;
    m_profilerSettings.gpuProfilerSqttCsHashHi = 0;
    m_profilerSettings.gpuProfilerSqttCsHashLo = 0;
    m_profilerSettings.gpuProfilerSqttMaxDraws = 0;
    m_profilerSettings.gpuProfilerSqttBufferSize = 1048576;

    return Result::Success;
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

    // Force logging on for the next frame if the user is currently holding Shift-F11.
    m_forceLogging = (IsKeyPressed(KeyCode::Shift) && IsKeyPressed(KeyCode::F11));
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
    case Developer::CallbackType::PresentConcluded:
    case Developer::CallbackType::CreateImage:
        break;
    case Developer::CallbackType::BarrierBegin:
    case Developer::CallbackType::BarrierEnd:
    case Developer::CallbackType::ImageBarrier:
    {
        PAL_ASSERT(pCbData != nullptr);
        const bool hasValidData = TranslateBarrierEventData(pCbData);

        if (hasValidData)
        {
            Developer::BarrierData* pData      = static_cast<Developer::BarrierData*>(pCbData);
            TargetCmdBuffer*        pCmdBuffer = static_cast<TargetCmdBuffer*>(pData->pCmdBuffer);

            if (pCmdBuffer != nullptr)
            {
                pCmdBuffer->UpdateCommentString(pData);
            }
        }
        break;
    }
    case Developer::CallbackType::DrawDispatch:
        PAL_ASSERT(pCbData != nullptr);
        TranslateDrawDispatchData(pCbData);
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    pPlatform->DeveloperCb(deviceIndex, type, pCbData);
}

} // GpuProfiler
} // Pal
