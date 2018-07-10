/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palFile.h"
#include "core/os/lnx/lnxWindowSystem.h"
#include "wsa.h"

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 392
namespace Pal
{
namespace Linux
{

class Device;
class WaylandWindowSystem;

// =====================================================================================================================
// The definition of Present fence for wayland platform.
class WaylandPresentFence : public PresentFence
{
public:
    static size_t GetSize() { return sizeof(WaylandPresentFence); }

    static Result Create(
        const WaylandWindowSystem& windowSystem,
        bool                       initiallySignaled,
        void*                      pPlacementAddr,
        PresentFence**             ppPresentFence);

    virtual void Reset() override;

    virtual Result Trigger() override;

    virtual Result WaitForCompletion(bool doWait) override;

    void SetImage(int32 hImage) { m_hImage = hImage; }

private:
    explicit WaylandPresentFence(const WaylandWindowSystem& windowSystem);
    virtual ~WaylandPresentFence();

    Result Init(bool initiallySignaled);

    const WaylandWindowSystem& m_windowSystem;
    int32                      m_hImage;

    PAL_DISALLOW_DEFAULT_CTOR(WaylandPresentFence);
    PAL_DISALLOW_COPY_AND_ASSIGN(WaylandPresentFence);
};

// =====================================================================================================================
// Represent a window system with wayland extension.
class WaylandWindowSystem : public WindowSystem
{
public:
    // The WindowSystem class is designed to be placed into other PAL objects which requires the Create/Destroy pattern.
    static size_t GetSize() { return sizeof(WaylandWindowSystem); }

    static Result Create(
        const Device&                 device,
        const WindowSystemCreateInfo& createInfo,
        void*                         pPlacementAddr,
        WindowSystem**                ppWindowSystem);

    // Helper functions to describe the properties of a window system we will create in the future.
    static Result GetWindowGeometry(
        Device*             pDevice,
        OsDisplayHandle     hDisplay,
        OsWindowHandle      hWindow,
        Extent2d*           pExtents);

    static Result DeterminePresentationSupported(
        Device*             pDevice,
        OsDisplayHandle     hDisplay,
        int64               visualId);

    static Result LoadWaylandWsa();

    virtual Result CreatePresentableImage(
        Image*              pImage,
        int32               sharedBufferFd) override;

    virtual void DestroyPresentableImage(
        uint32              image) override;

    virtual Result Present(
        const PresentSwapChainInfo& presentInfo,
        PresentFence*               pRenderFence,
        PresentFence*               pIdleFence) override;

    virtual Result WaitForLastImagePresented() override;

private:
    WaylandWindowSystem(const Device& device, const WindowSystemCreateInfo& createInfo);
    virtual ~WaylandWindowSystem();

    Result Init();

    const Device&        m_device;
    void*                m_pDisplay;         // wayland display created by App
    void*                m_pSurface;         // wayland surface created by App
    int32                m_hWsa;
    static WsaInterface* s_pWsaInterface;

    friend WaylandPresentFence;

    PAL_DISALLOW_DEFAULT_CTOR(WaylandWindowSystem);
    PAL_DISALLOW_COPY_AND_ASSIGN(WaylandWindowSystem);
};

} // Linux
} // Pal
#endif
