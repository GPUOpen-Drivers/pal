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

#include "pal.h"
#include "core/os/amdgpu/amdgpuQueue.h"

namespace Pal
{

enum class PresentMode   : uint32;
enum class SwapChainMode : uint32;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 881
enum class WsiPlatform : uint32;
#else
enum       WsiPlatform : uint32;
#endif

struct ScreenInfo;
struct PresentableImageCreateInfo;
struct PresentSwapChainInfo;

namespace Amdgpu
{

class Device;
class Image;
class WindowSystem;
class SwapChain;

union WindowSystemImageHandle
{
    void*  pBuffer;  // Native buffer handle in Wayland is a pointer.
    uint32 hPixmap;  // Native pixmap handle in X is a 32-bit integer.
    uint32 hBuffer;  // Native buffer handle in drm is a 32-bit integer.
};

constexpr WindowSystemImageHandle NullImageHandle = { 0 }; // Value representing a null or invalid image handle.

struct WindowSystemCreateInfo
{
    WsiPlatform     platform;
    SwapChainMode   swapChainMode;

    union
    {
        /// Properties of desktop window platform.
        struct
        {
            OsDisplayHandle hDisplay;
            OsWindowHandle  hWindow;
            SwizzledFormat  format;
        };

        /// Properties of DirectDisplay platform.
        struct
        {
            int32           drmMasterFd;
            uint32          connectorId;
        };
    };
};

union WindowSystemProperties
{
    struct
    {
        uint64 supportFreeSyncExtension : 1;   // the window system support extension call to enable free sync.
        uint64 reserved                 : 63;
    };
    uint64 u64All;
};

// =====================================================================================================================
// A special Linux-specific fence used to synchronize presentation between PAL and the WindowSystem.
class PresentFence
{
public:
    static size_t GetSize(WsiPlatform platform);

    static Result Create(
        const WindowSystem& windowSystem,
        bool                initiallySignaled,
        void*               pPlacementAddr,
        PresentFence**      ppPresentFence);

    void Destroy() { this->~PresentFence(); }

    // Reset the fence.
    virtual void Reset() = 0;

    // After rendering is completed, trigger the fence.
    virtual Result Trigger() = 0;

    // Wait for the window system to complete the present.
    virtual Result WaitForCompletion(bool doWait) = 0;

    virtual Result AssociatePriorRenderFence(IQueue* pQueue) = 0;

protected:
    PresentFence() { }
    virtual ~PresentFence() { }

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(PresentFence);
};

// =====================================================================================================================
// This class is responsible for creating presentable images by some extension protocals, such as DRI3, DRI2, asking
// window system to present a image,and waiting for window system finishing to present image.
class WindowSystem
{
public:
    // The WindowSystem class is designed to be placed into other PAL objects which requires the Create/Destroy pattern.
    static size_t GetSize(WsiPlatform platform);

    static Result Create(
        const Device&                 device,
        const WindowSystemCreateInfo& createInfo,
        void*                         pPlacementAddr,
        WindowSystem**                ppWindowSystem);

    void Destroy() { this->~WindowSystem(); }

    // Helper functions to describe the properties of a window system we will create in the future.
    static Result GetWindowProperties(
        Device*              pDevice,
        WsiPlatform          platform,
        OsDisplayHandle      hDisplay,
        OsWindowHandle       hWindow,
        SwapChainProperties* pSwapChainProperties);

    static Result DeterminePresentationSupported(
        Device*             pDevice,
        OsDisplayHandle     hDisplay,
        WsiPlatform         platform,
        int64               visualId);

    static Result AcquireScreenAccess(
        Device*         pDevice,
        OsDisplayHandle hDisplay,
        WsiPlatform     wsiPlatform,
        uint32          connector,
        uint32*         pRandrOutput,
        int32*          pDrmMasterFd);

    static Result GetOutputFromConnector(
        OsDisplayHandle hDisplay,
        Device*         pDevice,
        WsiPlatform     wsiPlatform,
        uint32          connector,
        uint32*         pOutput);

    // Create a presentable image or pixmap from a buffer. This function is only meaningful for Dri3.
    virtual Result CreatePresentableImage(
        SwapChain* pSwapChain,
        Image*     pImage,
        int32      sharedBufferFd) = 0;

    virtual void DestroyPresentableImage(
        WindowSystemImageHandle hImage) = 0;

    // Ask window system to present. For Dri3, the pixmap will be presented. For Dri2, pixmap is useless and only a
    // swap buffer request will be sent to X Server.
    virtual Result Present(
        const PresentSwapChainInfo& presentInfo,
        PresentFence*               pRenderFence,
        PresentFence*               pIdleFence) = 0;

    virtual Result WaitForLastImagePresented() = 0;

    virtual bool NeedWindowSizeChangedCheck() const { return true; }

    WsiPlatform PlatformType() const { return m_platform; }
    const WindowSystemProperties& GetWindowSystemProperties() const { return m_windowSystemProperties; }
    bool PresentOnSameGpu() const { return m_presentOnSameGpu; }
    virtual void GoThroughEvent() { return; }
    virtual void WaitOnIdleEvent(WindowSystemImageHandle* pImage) { return; }
    virtual bool SupportIdleEvent() { return false; }
    virtual bool CheckIdleImage(
        WindowSystemImageHandle* pIdleImage,
        PresentFence*            pFence) { return false; }

protected:
    WindowSystem(WsiPlatform platform);
    virtual ~WindowSystem() { }

    const WsiPlatform      m_platform;
    WindowSystemProperties m_windowSystemProperties;

    bool m_presentOnSameGpu;

private:
    PAL_DISALLOW_DEFAULT_CTOR(WindowSystem);
    PAL_DISALLOW_COPY_AND_ASSIGN(WindowSystem);
};

} // Amdgpu
} // Pal
