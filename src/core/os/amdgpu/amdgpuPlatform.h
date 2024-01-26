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

#include "core/platform.h"
#include "core/os/amdgpu/amdgpuHeaders.h"
#if PAL_HAVE_DRI3_PLATFORM
#include "core/os/amdgpu/dri3/g_dri3Loader.h"
#endif
#include "core/os/amdgpu/g_drmLoader.h"
#if PAL_HAVE_WAYLAND_PLATFORM
#include "core/os/amdgpu/wayland/g_waylandLoader.h"
#endif
#include "palLib.h"

namespace Pal
{
namespace Amdgpu
{

// =====================================================================================================================
// AMDGPU flavor of the Platform singleton. The responsibilities of the OS-specific Platform classes are interacting
// with the OS and kernel-mode drivers. On Linux/AMDGPU, this includes managing access to the X server powered by the
// GPU's in the system, as well as interfacing with the components of Linux driver stack.
class Platform final : public Pal::Platform
{
public:
    Platform(
        const PlatformCreateInfo&   createInfo,
        const Util::AllocCallbacks& allocCb);

    virtual ~Platform() {}

    virtual void Destroy() override;

    virtual size_t GetScreenObjectSize() const override;
#if PAL_HAVE_DRI3_PLATFORM
    const Dri3Loader&    GetDri3Loader();
#endif
    const DrmLoader&     GetDrmLoader();
#if PAL_HAVE_WAYLAND_PLATFORM
    const WaylandLoader& GetWaylandLoader();
#endif

    virtual Result GetPrimaryLayout(
        uint32 vidPnSourceId,
        GetPrimaryLayoutOutput* pPrimaryLayoutOutput) override {return Result::ErrorUnavailable;}

    virtual Result TurboSyncControl(
        const TurboSyncControlInput& turboSyncControlInput) override {return Result::ErrorUnavailable;}

    bool  IsQueuePrioritySupported() const { return m_features.supportQueuePriority == 1; }
    bool  IsQueueIfhKmdSupported()   const { return m_features.supportQueueIfhKmd   == 1; }
    bool  IsProSemaphoreSupported()  const { return m_features.supportProSemaphore  == 1; }
    bool  IsSyncObjectSupported()    const { return m_features.supportSyncObj       == 1; }
    bool  IsRaw2SubmitSupported()    const { return m_features.supportRaw2SubmitRoutine == 1; }
    bool  IsCreateSignaledSyncObjectSupported() const { return m_features.supportCreateSignaledSyncobj == 1; }
    bool  IsSyncobjFenceSupported()  const { return m_features.supportSyncobjFence  == 1; }
    bool  IsHostMappedForeignMemorySupported() const { return m_features.suportHostMappedForeignMemory == 1; }
protected:
    virtual Result InitProperties() override;
    virtual Result ConnectToOsInterface() override;
    virtual Result ReQueryDevices() override;

    virtual Result ReQueryScreens(
        uint32*  pScreenCount,
        void*    pStorage[MaxScreens],
        IScreen* pScreens[MaxScreens]) override;
#if PAL_HAVE_DRI3_PLATFORM
    Dri3Loader    m_dri3Loader;
#endif
    DrmLoader     m_drmLoader;
#if PAL_HAVE_WAYLAND_PLATFORM
    WaylandLoader m_waylandLoader;
#endif

#if defined(PAL_DEBUG_PRINTS)
    static constexpr size_t LogPathLength = 256;
    char         m_logPath[LogPathLength];
#endif

    union
    {
        struct
        {
            uint32 supportProSemaphore          :  1;    // Support Pro stack Semaphore
            uint32 supportSyncObj               :  1;    // Support Sync Object Interface
            uint32 supportRaw2SubmitRoutine     :  1;    // Support raw2 submit routine
            uint32 supportQueuePriority         :  1;    // Support creating queue with priority
            uint32 supportCreateSignaledSyncobj :  1;    // Support creating initial signaled syncobj.
            uint32 supportSyncobjFence          :  1;    // Support fence based on sync object.
            uint32 suportHostMappedForeignMemory:  1;    // Support pin memory which is host-mapped from foreign device.
            uint32 supportQueueIfhKmd           :  1;    // Support IFH KMD mode.
            uint32 reserved                     : 24;
        };
        uint32 u32All;
    } m_features;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(Platform);
};

// Converts an AMDGPU format enumeration to a PAL format.
extern SwizzledFormat AmdgpuFormatToPalFormat(
    AMDGPU_PIXEL_FORMAT format,
    bool*               pFormatChange,
    bool*               pDepthStencilUsage);

extern AMDGPU_PIXEL_FORMAT PalToAmdGpuFormatConversion(
    SwizzledFormat format);
} // Amdgpu
} // Pal
