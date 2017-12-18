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

#pragma once

#include "core/layers/decorators.h"
#include "palMutex.h"
#include <ctime>

namespace Pal
{
namespace GpuProfiler
{

enum GpuProfilerGranularity : uint32
{
    GpuProfilerGranularityDraw = 0,
    GpuProfilerGranularityCmdBuf = 1,
    GpuProfilerGranularityFrame = 2,

};

enum CmdAllocResidencyFlags : uint32
{
    CmdAllocResWaitOnSubmitCommandData = 0x00000001,
    CmdAllocResWaitOnSubmitEmbeddedData = 0x00000002,
    CmdAllocResWaitOnSubmitMarkerData = 0x00000004,

};

struct GpuProfilerSettings
{
    char                      gpuProfilerLogDirectory[512];
    uint32                    gpuProfilerStartFrame;
    uint32                    gpuProfilerFrameCount;
    bool                      gpuProfilerRecordPipelineStats;
    char                      gpuProfilerGlobalPerfCounterConfigFile[256];
    bool                      gpuProfilerGlobalPerfCounterPerInstance;
    bool                      gpuProfilerBreakSubmitBatches;
    bool                      gpuProfilerCacheFlushOnCounterCollection;
    GpuProfilerGranularity    gpuProfilerGranularity;
    uint32                    gpuProfilerSqThreadTraceTokenMask;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION <= 297
    uint32                    gpuProfilerSqttPipelineHashHi;
#endif
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION <= 297
    uint32                    gpuProfilerSqttPipelineHashLo;
#endif
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 298
    uint64                     gpuProfilerSqttPipelineHash;
#endif
    uint32                     gpuProfilerSqttVsHashHi;
    uint32                     gpuProfilerSqttVsHashLo;
    uint32                     gpuProfilerSqttHsHashHi;
    uint32                     gpuProfilerSqttHsHashLo;
    uint32                     gpuProfilerSqttDsHashHi;
    uint32                     gpuProfilerSqttDsHashLo;
    uint32                     gpuProfilerSqttGsHashHi;
    uint32                     gpuProfilerSqttGsHashLo;
    uint32                     gpuProfilerSqttPsHashHi;
    uint32                     gpuProfilerSqttPsHashLo;
    uint32                     gpuProfilerSqttCsHashHi;
    uint32                     gpuProfilerSqttCsHashLo;
    uint32                     gpuProfilerSqttMaxDraws;
    size_t                     gpuProfilerSqttBufferSize;
};

// =====================================================================================================================
class Platform : public PlatformDecorator
{
public:
    static Result Create(
        const PlatformCreateInfo&   createInfo,
        const Util::AllocCallbacks& allocCb,
        IPlatform*                  pNextPlatform,
        GpuProfilerMode             mode,
        void*                       pPlacementAddr,
        IPlatform**                 ppPlatform);

    Platform(
        const PlatformCreateInfo&   createInfo,
        const Util::AllocCallbacks& allocCb,
        IPlatform*                  pNextPlatform,
        GpuProfilerMode             mode);

    virtual Result Init() override;

    uint32 FrameId() const { return m_frameId; }
    void IncrementFrameId();

    const char* LogDirName() const { return &m_logDirName[0]; }
    bool IsLoggingForced() const { return m_forceLogging; }

    // Public IPlatform interface methods:
    virtual Result EnumerateDevices(uint32* pDeviceCount, IDevice* pDevices[MaxDevices]) override;
    virtual size_t GetScreenObjectSize() const override;
    virtual Result GetScreens(
        uint32*  pScreenCount,
        void*    pStorage[MaxScreens],
        IScreen* pScreens[MaxScreens]) override;

    static void PAL_STDCALL GpuProfilerCb(
        void*                   pPrivateData,
        const uint32            deviceIndex,
        Developer::CallbackType type,
        void*                   pCbData);

    const struct tm& Time() const { return m_time; }
    const uint16 ApiMajorVer() const { return m_apiMajorVer; }
    const uint16 ApiMinorVer() const { return m_apiMinorVer; }

    Util::Mutex* PipelinePerfDataLock() { return &m_pipelinePerfDataLock;  }
    const GpuProfilerSettings& ProfilerSettings() const { return m_profilerSettings; }
    GpuProfilerMode GetProfilerMode() const { return m_profilerMode; }

private:
    virtual ~Platform() { }
    Result UpdateSettings();

    GpuProfilerMode     m_profilerMode;
    GpuProfilerSettings m_profilerSettings;

    uint32     m_frameId;       // ID incremented on every present call.
    bool       m_forceLogging;  // Indicates logging has been enabled by the user hitting Shift-F11.
    struct tm  m_time;          // Copy of the date/time info for this run of the application.
    uint16     m_apiMajorVer;   // API major version, used in RGP dumps.
    uint16     m_apiMinorVer;   // API minor version, used in RGP dumps.

    // Storage for a unique log directory per session based on the executable name and current date/time.
    static constexpr size_t LogDirNameLength = 256;
    char m_logDirName[LogDirNameLength];

    Util::Mutex m_pipelinePerfDataLock;

    PAL_DISALLOW_DEFAULT_CTOR(Platform);
    PAL_DISALLOW_COPY_AND_ASSIGN(Platform);
};

} // GpuProfiler
} // Pal
