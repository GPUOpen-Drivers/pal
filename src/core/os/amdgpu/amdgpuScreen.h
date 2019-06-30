/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/os/amdgpu/amdgpuPlatform.h"
#include "core/os/amdgpu/amdgpuDevice.h"
#include "palScreen.h"
#include "palVector.h"

namespace Pal
{
namespace Amdgpu
{

// =====================================================================================================================
// Following definition should be part of libdrm, however, it's not up-streamed yet since libdrm doesn't support HDR
// yet. It should be removed once libdrm is updated.

enum HdmiMetadataType : uint8
{
    HDMI_STATIC_METADATA_TYPE1 = 1,
};

enum HdmiEotf : uint8
{
    HDMI_EOTF_TRADITIONAL_GAMMA_SDR,
    HDMI_EOTF_TRADITIONAL_GAMMA_HDR,
    HDMI_EOTF_SMPTE_ST2084,
};

/* HDR Metadata Infoframe as per 861.G spec */
struct HdrMetadataInfoFrame
{
    HdmiEotf         eotf;
    HdmiMetadataType metadataType;

    uint16 chromaticityRedX;
    uint16 chromaticityRedY;
    uint16 chromaticityGreenX;
    uint16 chromaticityGreenY;
    uint16 chromaticityBlueX;
    uint16 chromaticityBlueY;
    uint16 chromaticityWhitePointX;
    uint16 chromaticityWhitePointY;

    uint16 maxLuminance;
    uint16 minLuminance;
    uint16 maxContentLightLevel;
    uint16 maxFrameAverageLightLevel;
};

struct HdrOutputMetadata
{
    uint32 metadataType;
    struct HdrMetadataInfoFrame metadata;
};

// =====================================================================================================================
// Represents a screen (typically a physical monitor) that can be used for presenting rendered images to the end user.
class Screen : public Pal::IScreen
{
public:
    Screen(Device*  pDevice,
           Extent2d physicalDimension,
           Extent2d physicalResolution,
           uint32   connectorId);
    virtual ~Screen() { }

    virtual void Destroy() override;

    virtual Result GetProperties(ScreenProperties* pInfo) const override;

    virtual Result GetScreenModeList(
            uint32*     pScreenModeCount,
            ScreenMode* pScreenModeList) const override;

    virtual Result GetFormats(
        uint32*          pFormatCount,
        SwizzledFormat*  pFormatList) override;

    virtual Result GetColorCapabilities(
        ScreenColorCapabilities* pCapabilities) override;

    virtual Result SetColorConfiguration(
        const ScreenColorConfig* pColorConfig) override;

    virtual Result RegisterWindow(Pal::OsWindowHandle) override { return Result::Unsupported; }
    virtual Result IsImplicitFullscreenOwnershipSafe(
        OsDisplayHandle hDisplay,
        OsWindowHandle  hWindow,
        Extent2d        imageExtent) const override { return Result::Unsupported; }
    virtual Result TakeFullscreenOwnership(const Pal::IImage&) override { return Result::Unsupported; }
    virtual Result ReleaseFullscreenOwnership() override { return Result::Unsupported; }
    virtual Result SetGammaRamp(const Pal::GammaRamp&) override { return Result::Unsupported; }
    virtual Result WaitForVerticalBlank() const override { return Result::Unsupported; }
    virtual Result GetScanLine(Pal::int32*) const override { return Result::Unsupported; }

    virtual Result AcquireScreenAccess(
        OsDisplayHandle hDisplay,
        WsiPlatform     wsiPlatform) override;
    virtual Result ReleaseScreenAccess() override;
    virtual Result GetRandrOutput(
        OsDisplayHandle hDisplay,
        uint32*         pRandrOutput) override;
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

    HdrOutputMetadata m_nativeColorGamut;
    HdrOutputMetadata m_userColorGamut;

    PAL_DISALLOW_DEFAULT_CTOR(Screen);
    PAL_DISALLOW_COPY_AND_ASSIGN(Screen);
};

} // Amdgpu
} // Pal

