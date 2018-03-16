/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/os/lnx/dri3/dri3Loader.h"
#include "core/os/lnx/drmLoader.h"
#include "core/os/lnx/lnxHeaders.h"
namespace Pal
{

struct PlatformCreateInfo;
namespace Linux
{

// =====================================================================================================================
// Linux flavor of the Platform singleton. The responsibilities of the OS-specific Platform classes are interacting
// with the OS and kernel-mode drivers. On Linux, this includes managing access to the X server powered by the GPU's
// in the system, as well as interfacing with the components of Linux driver stack.
class Platform : public Pal::Platform
{
public:
    Platform(
        const PlatformCreateInfo&   createInfo,
        const Util::AllocCallbacks& allocCb);

    virtual ~Platform() {}

    virtual void Destroy() override;

    virtual size_t GetScreenObjectSize() const override;

    const Dri3Loader& GetDri3Loader();
    const DrmLoader&  GetDrmLoader();

    virtual Result GetPrimaryLayout(
        uint32 vidPnSourceId,
        GetPrimaryLayoutOutput* pPrimaryLayoutOutput) override {return Result::ErrorUnavailable;}

    virtual Result TurboSyncControl(
        const TurboSyncControlInput& turboSyncControlInput) override {return Result::ErrorUnavailable;}

    virtual bool      IsDtifEnabled() const override { return m_features.dtifEnabled == 1; }
    bool              CheckDtifStatus();

    bool  IsQueuePrioritySupported() const { return m_features.supportQueuePriority == 1; }
    bool  IsProSemaphoreSupported()  const { return m_features.supportProSemaphore  == 1; }
    bool  IsSyncObjectSupported()    const { return m_features.supportSyncObj       == 1; }
    bool  IsCreateSignaledSyncObjectSupported() const { return m_features.supportCreateSignaledSyncobj == 1; }
    bool  IsSyncobjFenceSupported()  const { return m_features.supportSyncobjFence  == 1; }
protected:
    virtual Result InitProperties() override;
    virtual Result ConnectToOsInterface() override;
    virtual Result ReQueryDevices() override;

    virtual Result ReQueryScreens(
        uint32*  pScreenCount,
        void*    pStorage[MaxScreens],
        IScreen* pScreens[MaxScreens]) override;

    Dri3Loader   m_dri3Loader;
    DrmLoader    m_drmLoader;
#if defined(PAL_DEBUG_PRINTS)
    static constexpr size_t LogPathLength = 256;
    char         m_logPath[LogPathLength];
#endif

    union
    {
        struct
        {
            uint32 dtifEnabled                  :  1;    // Whether DTIF is enabled
            uint32 supportProSemaphore          :  1;    // Support Pro stack Semaphore
            uint32 supportSyncObj               :  1;    // Support Sync Object Interface
            uint32 supportRawSubmitRoutine      :  1;    // Support raw submit routine
            uint32 supportQueuePriority         :  1;    // Support creating queue with priority
            uint32 supportCreateSignaledSyncobj :  1;    // support creating initial signaled syncobj.
            uint32 supportSyncobjFence          :  1;    // support fence based on sync object.
            uint32 reserved                     : 25;
        };
        uint32 u32All;
    } m_features;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(Platform);
};

// Converts an AMDGPU format enumeration to a PAL format.
extern SwizzledFormat AmdgpuFormatToPalFormat(
        AMDGPU_PIXEL_FORMAT format,
        bool* pFormatChange,
        bool* pDepthStencilUsage);

}
}
