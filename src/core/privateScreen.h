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

#include "palPrivateScreen.h"

namespace Pal
{

// Maximum number of format supported by a PrivateScreen, which is needed to define m_formatCaps array.
constexpr uint32 MaxPrivateScreenFormats = 8;

// Forward decl's
class Device;
class Image;

// Specifies the callback function that is provided by PAL client for topology change notification
typedef void (PAL_STDCALL *PrivateScreenTopologyChangedFunc)(
    Device*  pDevice,
    void*    pClientData);

// =====================================================================================================================
// Represents a private screen that can be used for presenting rendered images to the end user.
class PrivateScreen : public IPrivateScreen
{
public:
    virtual ~PrivateScreen();

    PrivateScreen(Device* pDevice, const PrivateScreenCreateInfo& createInfo);

    virtual void BindOwner(void* pOwner) override { m_pOwner = pOwner; }

    virtual Result GetProperties(
        size_t*                  pNumFormats,
        size_t*                  pEdidSize,
        PrivateScreenProperties* pInfo) const override;

    virtual Result GetPresentStats(
        PrivateScreenPresentStats* pStats) override;

    virtual Result Enable(
        const PrivateScreenEnableInfo& info) override;

    virtual Result Disable() override;

    virtual Result Blank() override;

    virtual Result Present(
        const PrivateScreenPresentInfo& presentInfo) override;

    virtual Result GetScanLine(int32* pScanLine) override;

    virtual Result GetConnectorProperties(
        uint32*                           pDataSize,
        PrivateScreenConnectorProperties* pConnectorProperties) override;

    virtual Result GetDisplayMode(PrivateDisplayMode* pMode) override;

    virtual Result SetGammaRamp(const GammaRamp* pGammaRamp) override;

    virtual Result SetPowerMode(PrivateDisplayPowerState powerMode) override;

    virtual Result SetDisplayMode(const PrivateDisplayMode& displayMode) override;

    virtual Result SetColorMatrix(const ColorTransform& matrix) override;

    virtual Result SetEventAfterVsync(
        OsExternalHandle hEvent,
        uint32           delayInUs,
        bool             repeated) override;

    virtual Result GetHdcpStatus(HdcpStatus* pStatus) override;

    virtual Result EnableAudio(bool enable) override;

    Result InitPhysical();
    Result InitEmulated();
    uint32 Index() const { return m_index; }
    uint32 NumFormats() const { return m_properties.numFormats; }
    uint64 Hash() const { return m_properties.hash; }
    bool   Removed() const { return m_removed; }
    bool   Enabled() const { return m_enabled; }

    Result ObtainImageId(uint32* pImageId);
    void   ReturnImageId(uint32 imageId);
    void   SetImageSlot(uint32 imageId, Image* pImage);
    bool   FormatSupported(SwizzledFormat format) const;

private:

    Result OsInitProperties();

    Result OsGetPresentStats(
        PrivateScreenPresentStats* pStats);

    Result OsEnable(
        const PrivateScreenEnableInfo& info);

    Result OsDisable();

    Result OsBlank();

    Result OsPresent(
        const PrivateScreenPresentInfo& presentInfo);

    Result OsGetScanLine(int32* pScanLine);

    Result OsGetConnectorProperties(
        uint32*                           pDataSize,
        PrivateScreenConnectorProperties* pConnectorProperties);

    Result OsSetGammaRamp(const GammaRamp* pGammaRamp);

    Result OsSetColorMatrix(const ColorTransform& matrix);

    Result OsSetPowerMode(PrivateDisplayPowerState powerMode);

    Result OsSetDisplayMode(const PrivateDisplayMode& displayMode);

    Result OsSetEventAfterVsync(
        OsExternalHandle hEvent,
        uint32           delayInUs,
        bool             repeated);

    Result OsGetHdcpStatus(HdcpStatus* pStatus);

    Result OsEnableAudio(bool enable);

    void UpdateSupportedFormats(SwizzledFormat format);
    void FinalizeSupportedFormats() { m_properties.numFormats = m_reportedFormats; }

    Device*const            m_pDevice;
    uint32                  m_index;        // Display index of this private screen.
    uint64                  m_dummyHandle;  // Dummy display handle for emulated private screen.
    PrivateScreenProperties m_properties;   // Extent, refresh rate, EDID and supported format info.

    void*                   m_pOwner;       // Owner object bound by the client, used to OnDestroy notification.

    // A flag indicates the private screen is removed. A removd private screen cannot be used for presenting etc.
    // This flag might be set when KMD detects a removal and before UMD handle can handle the event.
    bool                    m_removed;
    // A flag indicates the private screen is enabled. A private screen can only be used when it is enabled. Note this
    // can only protect double enable within the same process. Another process trying to enable an already-enabled
    // private screen should be failed by KMD.
    bool                    m_enabled;
    // A mask of currenly created image slots. A private screen supports 16 image (actually its GPU physical addresses).
    // And images can be created and destroyed, so the mask is used to get an available image slot (id).
    uint32                  m_imageMask;
    // Stored image pointers, this is used to clear the image's private screen info to avoid accessing invalid private
    // screen pointers when this image is destroyed.
    Image*                  m_pImages[MaxPrivateScreenImages];

    // Format of last present, which is initially "undefined". When this format changes during present time, the format
    // needs to be passed to KMD. The format is of ATI_FORMAT for Windows implementation.
    int32                   m_lastPresentFormat;

    // A bitmask indicates the format in the global format table is supported. The bit offset is the same as the index
    // in the format table.
    uint32                  m_formatCaps;

    // The actual number of formats that the private screen supports.
    uint32                  m_reportedFormats;

    // m_bridgeCount is used to decide the size of buffer for escape call. OsGetConnectorProperties will record the
    // count of bridge after the first time call.
    uint32                  m_bridgeCount;

    // The m_displayMode is used to cache the private display mode, it is assigned when calling PrivateScreen::Enable
    // or PrivateScreen::SetDisplayMode, and it can be retreved by calling PrivateScreen::GetDisplayMode.
    PrivateDisplayMode      m_displayMode;

    PAL_DISALLOW_DEFAULT_CTOR(PrivateScreen);
    PAL_DISALLOW_COPY_AND_ASSIGN(PrivateScreen);
};

} // Pal
