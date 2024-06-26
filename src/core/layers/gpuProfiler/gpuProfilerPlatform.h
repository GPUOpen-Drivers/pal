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

#pragma once

#include "core/layers/decorators.h"
#include "palDbgLogger.h"
#include "palMutex.h"
#include "palSysMemory.h"

#include <atomic>

using namespace Util;

namespace Pal
{
namespace GpuProfiler
{

enum CmdAllocResidencyFlags : uint32
{
    CmdAllocResWaitOnSubmitCommandData       = (1 << CommandDataAlloc),
    CmdAllocResWaitOnSubmitEmbeddedData      = (1 << EmbeddedDataAlloc),
    CmdAllocResWaitOnSubmitLargeEmbeddedData = (1 << LargeEmbeddedDataAlloc),
    CmdAllocResWaitOnSubmitGpuScratchMem     = (1 << GpuScratchMemAlloc),
};

/// GpuProfiler error logging
#define GPUPROFILER_ERROR(pFormat,...) \
    DbgLog(SeverityLevel::Error, OriginationType::GpuProfiler, "GPUProfiler", pFormat " (%s:%d:%s)",  ##__VA_ARGS__, \
           __FILE__, __LINE__, __func__);

/// GpuProfiler warning logging
#define GPUPROFILER_WARN(pFormat,...) \
    DbgLog(SeverityLevel::Warning, OriginationType::GpuProfiler, "GPUProfiler", pFormat " (%s:%d:%s)",  ##__VA_ARGS__, \
           __FILE__, __LINE__, __func__);

/// GpuProfiler info logging
#define GPUPROFILER_INFO(pFormat,...) \
    DbgLog(SeverityLevel::Info, OriginationType::GpuProfiler, "GPUProfiler", pFormat " (%s:%d:%s)",  ##__VA_ARGS__, \
           __FILE__, __LINE__, __func__);

// =====================================================================================================================
class Platform final : public PlatformDecorator
{
public:
    static Result Create(
        const PlatformCreateInfo&   createInfo,
        const Util::AllocCallbacks& allocCb,
        IPlatform*                  pNextPlatform,
        GpuProfilerMode             mode,
        const char*                 pTargetApp,
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

    uint32 GetUniversalQueueSequenceNumber();

    bool IsLoggingForced() const { return m_forceLogging; }

    // Public IPlatform interface methods:
    virtual Result EnumerateDevices(uint32* pDeviceCount, IDevice* pDevices[MaxDevices]) override;
    virtual size_t GetScreenObjectSize() const override;
    virtual Result GetScreens(
        uint32*  pScreenCount,
        void*    pStorage[MaxScreens],
        IScreen* pScreens[MaxScreens]) override;

    /// Create file logger for GPUProfiler message logging
    void CreateLogger()
    {
        if (m_pLogger == nullptr)
        {
            DbgLoggerFileSettings settings = {};

            settings.severityLevel     = SeverityLevel::Debug;
            settings.origTypeMask      = OriginationTypeFlagGpuProfiler;
            settings.fileSettingsFlags = FileSettings::ForceFlush;
            settings.fileAccessFlags   = FileAccessWrite;
            settings.pLogDirectory     = LogDirPath();

            DbgLoggerFile::CreateFileLogger<ForwardAllocator>(settings, "GPUProfiler", &m_allocator, &m_pLogger);
        }
    }

    static void PAL_STDCALL GpuProfilerCb(
        void*                   pPrivateData,
        const uint32            deviceIndex,
        Developer::CallbackType type,
        void*                   pCbData);

    const uint16 ApiMajorVer() const { return m_apiMajorVer; }
    const uint16 ApiMinorVer() const { return m_apiMinorVer; }

    Mutex* PipelinePerfDataLock() { return &m_pipelinePerfDataLock;  }
    GpuProfilerMode GetProfilerMode() const { return m_profilerMode; }

    void SetEndOfRecreateSeen(bool state) { m_recreationDone = state; }
    bool GetEndOfRecreateSeen() const { return m_recreationDone; }

private:
    virtual ~Platform() { DbgLoggerFile::DestroyFileLogger<ForwardAllocator>(m_pLogger, &m_allocator); }

    GpuProfilerMode m_profilerMode;
    uint32          m_frameId;                // ID incremented on every present call.
    uint32          m_universalQueueSequence; // Sequence number of next universal queue
    bool            m_forceLogging;           // Indicates logging has been enabled by the user hitting Shift-F11.
    uint16          m_apiMajorVer;            // API major version, used in RGP dumps.
    uint16          m_apiMinorVer;            // API minor version, used in RGP dumps.
    Util::Mutex     m_pipelinePerfDataLock;
    DbgLoggerFile*  m_pLogger;

    std::atomic<bool> m_recreationDone;

    PAL_DISALLOW_DEFAULT_CTOR(Platform);
    PAL_DISALLOW_COPY_AND_ASSIGN(Platform);
};

} // GpuProfiler
} // Pal
