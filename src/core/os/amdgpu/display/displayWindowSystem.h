/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/os/amdgpu/amdgpuWindowSystem.h"
#if PAL_HAVE_DRI3_PLATFORM
#include "core/os/amdgpu/dri3/g_dri3Loader.h"
#endif
#include "core/os/amdgpu/g_drmLoader.h"
#include "palEvent.h"
#include "palMutex.h"
#include "palSemaphore.h"
#include "palThread.h"
#include "palUtil.h"

#if PAL_HAVE_DRI3_PLATFORM
#include <xcb/xcb.h>
#include <xcb/present.h>
#endif

namespace Pal
{
namespace Amdgpu
{

class Device;
class DisplayWindowSystem;

/// PresentState is used to represent the state of presentable images.
enum class PresentState : uint32
{
    Idle,   ///< Indicates the image is idle.
    Flip,   ///< Indicates the image is flipped or is being scan-out.
};
// =====================================================================================================================
// The definition of Present fence for DirectDisplay platform. The fence will be unsignaled when call WaitForCompletion
// to wait, and will be signaled when the next Vsync happened.
class DisplayPresentFence final : public PresentFence
{
public:
    static size_t GetSize() { return sizeof(DisplayPresentFence); }

    static Result Create(
        const DisplayWindowSystem& windowSystem,
        bool                    initiallySignaled,
        void*                   pPlacementAddr,
        PresentFence**          ppPresentFence);

    virtual void Reset() override;

    virtual Result Trigger() override;

    virtual Result WaitForCompletion(bool doWait) override;

    virtual Result AssociatePriorRenderFence(IQueue* pQueue) override { return Result::Success; }

    void SetPresentState(PresentState state) { m_presentState = state; }
    PresentState GetPresentState() const { return m_presentState; }

private:
    DisplayPresentFence(const DisplayWindowSystem& windowSystem);
    virtual ~DisplayPresentFence();

    Result Init(bool initiallySignaled);

    const DisplayWindowSystem& m_windowSystem;
    volatile PresentState      m_presentState;

    // Todo: Sean suggested we should use Util::Event instead.
    Util::Semaphore            m_imageIdle;

    PAL_DISALLOW_DEFAULT_CTOR(DisplayPresentFence);
    PAL_DISALLOW_COPY_AND_ASSIGN(DisplayPresentFence);
};

// =====================================================================================================================
// DisplayWindowSystem can directly render to a display without using intermediate window system (X or Wayland), and it
// can directly manipulate DRM commands/interfaces. It's most useful for console, embedded and virtual reality
// applications.
class DisplayWindowSystem final : public WindowSystem
{
public:
    // The WindowSystem class is designed to be placed into other PAL objects which requires the Create/Destroy pattern.
    static size_t GetSize() { return sizeof(DisplayWindowSystem); }

    static Result Create(
        const Device&                 device,
        const WindowSystemCreateInfo& createInfo,
        void*                         pPlacementAddr,
        WindowSystem**                ppWindowSystem);

    // Helper functions to describe the properties of a window system we will create in the future.
    static Result DeterminePresentationSupported(
        Device*             pDevice,
        OsDisplayHandle     hDisplay,
        int64               visualId);

    static Result GetWindowProperties(
        Device*              pDevice,
        OsDisplayHandle      hDisplay,
        OsWindowHandle       hWindow,
        SwapChainProperties* pSwapChainProperties);

    virtual Result CreatePresentableImage(
        SwapChain* pSwapChain,
        Image*     pImage,
        int32      sharedBufferFd) override;

    virtual void DestroyPresentableImage(
        WindowSystemImageHandle hImage) override;

    virtual Result Present(
        const PresentSwapChainInfo& presentInfo,
        PresentFence*               pRenderFence,
        PresentFence*               pIdleFence) override;

    virtual Result WaitForLastImagePresented() override;

private:
    static void DisplayVblankCb(
        int32  fd,
        uint32 frame,
        uint32 sec,
        uint32 usec,
        void*  pUserData);

    static void DisplayPageFlipCb(
        int32  fd,
        uint32 frame,
        uint32 sec,
        uint32 usec,
        void*  pUserData);

    static void DisplayPageFlip2Cb(
        int32  fd,
        uint32 frame,
        uint32 sec,
        uint32 usec,
        uint32 crtcId,
        void*  pUserData);

    static void EventPolling(
        void* pData);

    Result FindCrtc();

private:
    DisplayWindowSystem(const Device& device, const WindowSystemCreateInfo& createInfo);
    virtual ~DisplayWindowSystem();

    Result Init();

    Result ModeSet(Image* pImage);

    uint32 GetCrtcId()   const { return m_crtcId; }
    int32  GetMasterFd() const { return m_drmMasterFd; }
    uint32 GetExitThreadEventFd() const { return m_exitThreadEvent.GetHandle(); }

    const Device&          m_device;
    const DrmLoader&       m_drmLoader;
    const DrmLoaderFuncs&  m_drmProcs;

    uint32  m_crtcId;
    int32   m_drmMasterFd;
    uint32  m_connectorId;

    Util::Thread  m_waitEventThread;

    // Todo: Sean suggested we should use Util::Event instead.
    // This Semaphore will be signaled if vsync happened, this means a presentable image is going to be scan-out.
    Util::Semaphore  m_flipSemaphore;
    Util::Event      m_exitThreadEvent;

    friend DisplayPresentFence;

    PAL_DISALLOW_DEFAULT_CTOR(DisplayWindowSystem);
    PAL_DISALLOW_COPY_AND_ASSIGN(DisplayWindowSystem);
};

} // Amdgpu
} // Pal
