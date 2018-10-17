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

#include "core/os/lnx/lnxPlatform.h"
#include "core/os/lnx/lnxDevice.h"
#include "palScreen.h"
#include "palVector.h"

namespace Pal
{
namespace Linux
{
// =====================================================================================================================
// Represents a screen (typically a physical monitor) that can be used for presenting rendered images to the end user.
class Screen : public Pal::IScreen
{
public:
    Screen(
        Device*             pDevice,
        Extent2d            physicalDimension,
        Extent2d            physicalResolution,
        uint32              connectorId);

    virtual ~Screen() override { }

    virtual void Destroy() override;

    virtual Result GetProperties(ScreenProperties* pInfo) const override;

    virtual Result GetScreenModeList(
            uint32*     pScreenModeCount,
            ScreenMode* pScreenModeList) const override;

    virtual Result RegisterWindow(Pal::OsWindowHandle) override { return Result::Unsupported; }
    virtual Result IsImplicitFullscreenOwnershipSafe(
        OsDisplayHandle hDisplay,
        OsWindowHandle  hWindow,
        Extent2d        imageExtent) const override { return Result::Unsupported; }
    virtual Result TakeFullscreenOwnership(const Pal::IImage&) override { return Result::Unsupported; }
    virtual Result ReleaseFullscreenOwnership() override { return Result::Unsupported; }
    virtual Result SetGammaRamp(const Pal::GammaRamp&) override { return Result::Unsupported; }
    virtual Result GetFormats(Pal::uint32*, Pal::SwizzledFormat*) override { return Result::Unsupported; }
    virtual Result GetColorCapabilities(Pal::ScreenColorCapabilities*) override { return Result::Unsupported; }
    virtual Result SetColorConfiguration(const Pal::ScreenColorConfig*) override { return Result::Unsupported; }
    virtual Result WaitForVerticalBlank() const override { return Result::Unsupported; }
    virtual Result GetScanLine(Pal::int32*) const override { return Result::Unsupported; }

    virtual Result AcquireScreenAccess(
        OsDisplayHandle hDisplay,
        WsiPlatform     wsiPlatform) override;
    virtual Result ReleaseScreenAccess() override;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 435
    virtual Result GetRandrOutput(
        OsDisplayHandle hDisplay,
        uint32*         pRandrOutput) override;
#endif
    virtual Result SetRandrOutput(
        uint32 randrOutput) override;

    Result Init();

    uint32 GetConnectorId() const { return m_connectorId; }
    int32  GetDrmMasterFd() const { return m_drmMasterFd; }

private:
    Device*const  m_pDevice;

    Extent2d   m_physicalDimension;
    Extent2d   m_physicalResolution;

    uint32     m_connectorId;
    int32      m_drmMasterFd;
    uint32     m_randrOutput;

    PAL_DISALLOW_DEFAULT_CTOR(Screen);
    PAL_DISALLOW_COPY_AND_ASSIGN(Screen);
};

} // Linux
} // Pal
