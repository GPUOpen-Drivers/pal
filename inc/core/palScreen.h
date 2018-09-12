/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
/**
 ***********************************************************************************************************************
 * @file  palScreen.h
 * @brief Defines the Platform Abstraction Library (PAL) IScreen interface and related types.
 ***********************************************************************************************************************
 */

#pragma once

#include "palPlatform.h"

namespace Pal
{

// Forward declarations.
class IDevice;
class IImage;
class IScreen;

/// Maximum string length for display names.  @see ScreenProperties.
constexpr uint32 MaxDisplayName = 256;

/// Maximum number of possible control points on a programmable gamma curve.
constexpr uint32 MaxGammaRampControlPoints = 1025;

/// Specifies the rotation of a specific screen.  Resolutions are always reported for an unrotated screen, and it is up
/// to the client to properly account for the rotation.
enum class ScreenRotation : uint32
{
    Rot0   = 0x0,
    Rot90  = 0x1,
    Rot180 = 0x2,
    Rot270 = 0x3,
    Count
};

/// Specifies color gamut as reported by the display panel EDID interface.
struct ColorGamut
{
    uint32   chromaticityRedX;           ///< Chromaticity Red X coordinate; in units of 0.00001
    uint32   chromaticityRedY;           ///< Chromaticity Red Y coordinate; in units of 0.00001
    uint32   chromaticityGreenX;         ///< Chromaticity Green X coordinate; in units of 0.00001
    uint32   chromaticityGreenY;         ///< Chromaticity Green Y coordinate; in units of 0.00001
    uint32   chromaticityBlueX;          ///< Chromaticity Blue X coordinate; in units of 0.00001
    uint32   chromaticityBlueY;          ///< Chromaticity Blue X coordinate; in units of 0.00001
    uint32   chromaticityWhitePointX;    ///< Chromaticity White point X coordinate; in units of 0.00001
    uint32   chromaticityWhitePointY;    ///< Chromaticity White point Y coordinate; in units of 0.00001
    uint32   minLuminance;               ///< Minimum luminance; in units of 0.0001 (1/10,000) Cd/m2.
    uint32   maxLuminance;               ///< Maximum luminance; in units of 0.0001 (1/10,000) Cd/m2.
    uint32   avgLuminance;               ///< Average luminance; in units of 0.0001 (1/10,000) Cd/m2.
};

/// Specifies color space and transfer functions as reported by the display panel EDID interface.
enum ScreenColorSpace : uint32
{
    TfUndefined      = 0x0000,

    /// Transfer function flags - defines how the input signal has been encoded.
    TfSrgb           = 0x00001, ///< sRGB non-linear format (IEC 61966-2-1:1999)
    TfBt709          = 0x00002, ///< BT.709 standard (HDTV)
    TfPq2084         = 0x00004, ///< HDR10 Media Profile, SMPTE ST 2084 (CEA - 861.3)
    TfLinear0_1      = 0x00008, ///< Linear 0.0 -> 1.0
    TfLinear0_125    = 0x00010, ///< Linear 0.0 -> 125
    TfDolbyVision    = 0x00020, ///< Propriety Dolby Vision transform
    TfGamma22        = 0x00040, ///< Gamma 2.2 (almost the same as sRGB transform)
    TfHlg            = 0x00080, ///< Hybrid Log Gamma (BBC \ NHK Ref)

    /// Color space flags - defines the domain of the input signal.
    CsSrgb           = 0x001000,  ///< SDR standard: sRGB non-linear format (IEC 61966-2-1:1999)
    CsBt709          = 0x002000,  ///< SDR standard: BT.709 standard (HDTV)
    CsBt2020         = 0x004000,  ///< HDR standard: BT.2020 standard (UHDTV)
    CsDolbyVision    = 0x008000,  ///< HDR standard: Propriety Dolby Vision
    CsAdobe          = 0x010000,  ///< HDR standard: Adobe
    CsDciP3          = 0x020000,  ///< HDR standard: DCI-P3 film industry standard
    CsScrgb          = 0x040000,  ///< HDR standard: scRGB non-linear format (Microsoft)
    CsUserDefined    = 0x080000,  ///< HDR standard: User defined
    CsNative         = 0x100000,  ///< HDR standard: Panel Native
    CsFreeSync2      = 0x200000,  ///< HDR standard: AMD FreeSync 2
};

/// Specifies properties for use with IScreen::GetColorCapabilties()
struct ScreenColorCapabilities
{
    union
    {
        struct
        {
            /// support flags
            uint32 dolbyVisionSupported     :  1; ///< True if DolbyVision is supported
            uint32 hdr10Supported           :  1; ///< True if HDR10 is supported
            uint32 freeSyncHdrSupported     :  1; ///< True if FreeSync2 is supported

            /// feature flags
            uint32 freeSyncBacklightSupport :  1; ///< True if FreeSync2 backlight control is supported

            uint32 reserved                 : 28; ///< Reserved for future use.
        };
        uint32 u32All;                            ///< Flags packed as 32-bit uint.
    };                                            ///< ScreenColorCapabilities property flags.

    ScreenColorSpace    supportedColorSpaces;     ///< Flags that specify the supported color spaces
    ColorGamut          nativeColorGamut;         ///< Native color gamut as reported by EDID
};

/// Specifies Client specified properties for use with IScreen::SetColorConfiguration()
struct ScreenColorConfig
{
    union
    {
        struct
        {
            uint32 localDimmingDisable :  1;    ///< Local dimming disable. See freeSyncBacklightSupport.
            uint32 reserved            : 31;    ///< Reserved for future use.
        };
        uint32 u32All;                          ///< Flags packed as 32-bit uint.
    };

    ChNumFormat         format;                 ///< Color format to Present
    ScreenColorSpace    colorSpace;             ///< Color space encoding to Present
    ColorGamut          userDefinedColorGamut;  ///< Color gamut to Present used with ScreenColorSpace::CS_userDefined
};

/// Specifies window system screen properties for use with IScreen::GetProperties()
struct WsiScreenProperties
{
    uint32  crtcId;         ///< The ID of CRTC. CRTC stands for CRT Controller, though it's not only related to CRT
                            ///  display, it supports HDMI, DP, VGA and DVI etc.. It can be used to set display
                            ///  timings, display resolution. And it can scan a frame buffer content to one or more
                            ///  displays.
    uint32  randrOutput;    ///< A handle to RandR output object. The output represents the underlying display hardware
                            ///  which include encoder and connector.

    uint32  connectorId;    ///< Connector ID. Connector represents a display connector (HDMI, DP, VGA, DVI...).
    int32   drmMasterFd;    ///< A file descriptor of DRM-master, it's used to hold/control the leased objects. DRM
                            ///  exposes an API that user-space programs can use to send commands and data to the GPU.
                            ///  If a process owns the fd of DRM-master, it has the highest privilege of the DRM.

    char    displayName[MaxDisplayName]; ///< The display name of the screen.
};

/// Reports properties of a screen (typically corresponds to one monitor attached to the desktop).  Output structure of
/// IScreen::GetProperties().
struct ScreenProperties
{
    OsDisplayHandle hDisplay;                     ///< OS-native handle to the display.  On Windows, displays and
                                                  ///  screens have a 1:1 relationship.  On Linux, a single display may
                                                  ///  host several screens.
    char            displayName[MaxDisplayName];  ///< String name of the display.
    uint32          screen;                       ///< Index reporting which of the display's screens this is.  On
                                                  ///  Windows, this value is VidPn target id.
    ScreenRotation  rotation;                     ///< Rotation of the screen (i.e., portrait or landscape mode).
    Rect            desktopCoordinates;           ///< Rectangle defining the region of the desktop occupied by this
                                                  ///  screen.  Will be all 0s if this info is not available on a
                                                  ///  platform.

    const IDevice*  pMainDevice;        ///< GPU which this screen is directly connected to. This may be null in the
                                        ///  case where the screen is associated with a GPU which is not PAL-capable.
    /// Set of GPU's which can perform cross-display presents to this screen. Typically this means that these GPU's
    /// are connected to the main GPU in some form of linked-adapter chain. This list does not include the main GPU.
    const IDevice*  pOtherDevice[MaxDevices - 1];
    uint32          otherDeviceCount; ///< Number of other GPU's in the system which can perform cross-diplay
                                      ///  presents to this screen. If this is zero, then no cross-display presents
                                      ///  can be done to this screen at all.

    bool  supportWindowedWaitForVerticalBlank;  ///< Supports waiting for a vertical blank event in windowed mode
    bool  supportWindowedGetScanLine;           ///< Supports retrieving the current scan-line in windowed mode

    struct
    {
        bool   supportScaleAndOffset;  ///< Post-conversion scale and offset is supported.
        float  minConvertedValue;      ///< Minimum supported output value.
        float  maxConvertedValue;      ///< Maximum supported output value.
        uint32 controlPointCount;      ///< Number of valid entries in controlPointPositions[].

        /// Array of floating point values describing the X-position of each control point.
        float controlPointPositions[MaxGammaRampControlPoints];
    } gammaRampCaps;                   ///< Gamma ramp capabilities.

    uint32          vidPnSourceId;     ///< Video present source identifier for Windows

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 402
    Extent2d        physicalDimension; ///< The physical dimension of screen in millimeters
    Extent2d        physicalResolution;///< The preferred or native resolution of screen
#endif
    WsiScreenProperties wsiScreenProp; ///< Window system screen properties.
};

/// Reports properties of a particular screen mode including resolution, pixel format, and other capabilities.  Output
/// structure of IScreen::GetScreenModeList().
struct ScreenMode
{
    union
    {
        struct
        {
            uint32 stereo              :  1;  ///< This mode supports stereoscopic rendering and display.
            uint32 crossDisplayPresent :  1;  ///< This mode supports cross-display presentation to the display via
                                              ///  hardware compositor.
            uint32 reserved            : 30;  ///< Reserved for future use.
        };
        uint32 u32All;                        ///< Flags packed as 32-bit uint.
    } flags;                                  ///< Screen mode flags.

    Extent2d       extent;       ///< Width and height of the resolution.
    SwizzledFormat format;       ///< Pixel format and channel swizzle of the display mode.
    uint32         refreshRate;  ///< Refresh rate in Hz.
};

/// 3-component floating point vector describing the red, green, and blue channels of a color.
struct RgbFloat
{
    float r;  ///< Red value.
    float g;  ///< Green value.
    float b;  ///< Blue value.
};

/// Specifies a custom gamma conversion ramp.
struct GammaRamp
{
    RgbFloat scale;   ///< RGB float scale value.  Scaling is performed after gamma conversion, but before the offset is
                      ///  added.
    RgbFloat offset;  ///< RGB float offset value.  Added after scaling.

    /// RGB float values corresponding to output value per control point. Gamma curve conversion is performed before any
    /// scale or offset are applied. Gamma curve defined by approximation across control points, including the end
    /// points. The actual number of curve control point used is retrieved in the gamma ramp capabilities of
    /// @ref ScreenProperties.
    RgbFloat gammaCurve[MaxGammaRampControlPoints];
};

/**
 ***********************************************************************************************************************
 * @interface IScreen
 * @brief     Represents a screen (typically a physical monitor) that can be used for presenting rendered images to the
 *            end user.
 *
 * @see Pal::GetScreens()
 ***********************************************************************************************************************
 */
class IScreen : public IDestroyable
{
public:
    /// Reports properties of this screen (name, rotation, location on the desktop, etc.).
    ///
    /// @param [out] pInfo Output properties. Must not be null.
    ///
    /// @returns Success if the properties were successfully queried.
    virtual Result GetProperties(
        ScreenProperties* pInfo) const = 0;

    /// Returns a list of supported display modes for this screen.
    ///
    /// @param [in,out] pScreenModeCount Input value specifies the maximum number of display modes to enumerate, and the
    ///                                  output value specifies the total number of display modes that were enumerated
    ///                                  in pScreenModeList.  The input value is ignored if pScreenModeList is null.
    ///                                  This pointer must not be null.
    /// @param [out]    pScreenModeList  Output list of display modes.  Can be null, in which case the total number of
    ///                                  available modes will be written to pScreenModeCount.
    ///
    /// @returns Success if the display modes were successfully queried and the results were reported in
    ///          pScreenModeCount/pScreenModeList.  Otherwise, one of the following errors may be returned:
    ///          + InvalidMemorySize if pScreenModeList is not null, but the input value to pScreenModeCount is smaller
    ///            than the number of available modes.
    virtual Result GetScreenModeList(
        uint32*     pScreenModeCount,
        ScreenMode* pScreenModeList) const = 0;

    /// Registers the specified OS window as belonging to this screen. The previously-registered Window is unregistered
    /// automatically.
    ///
    /// This is required on some operating systems before presenting an image using IQueue::Present().  The client
    /// can check the registerWindowRequired flag in PlatformProperties to determine if this call is required.  Calling
    /// this function on a platform where it is not required will not cause an error.
    ///
    /// @param [in] hWindow The OS-specific handle to the window. If the value of this parameter is NullWindowHandle,
    ///                     the previously-registered window is unregistered and no new window will be registered.
    ///
    /// @returns Success if the window was successfully register for this screen.  Otherwise, one of the following
    ///          errors may be returned:
    ///          + ErrorOutOfMemory if the call failed due to an internal failure to allocate system memory.
    ///          + ErrorUnknown if the call failed due to an unexpected OS call failure (possible due to a bad hWindow
    ///            parameter).
    virtual Result RegisterWindow(
        OsWindowHandle hWindow) = 0;

    /// Takes fullscreen ownership of this screen.  Application enters exclusive fullscreen mode.
    ///
    /// This function must be called before fullscreen presents (i.e., flip presents) can be performed on this screen.
    ///
    /// @param [in] image One of the images in the swap chain that will own the screen.  This image must support
    ///                   presents, and must match the current width, height, and format of this screen's current
    ///                   display mode.
    ///
    /// @returns Success if fullscreen exclusive mode was successfully entered.  Otherwise, one of the following errors
    ///           may be returned:
    ///           + ErrorUnavailable if the screen is already in fullscreen mode.
    ///           + ErrorInvalidResolution if the presentable image resolution does not match the current display mode.
    virtual Result TakeFullscreenOwnership(
        const IImage& image) = 0;

    /// Releases fullscreen owndership of this screen.  Application leaves exclusive fullscreen mode.
    ///
    /// @returns Success if fullscreen exclusive mode was successfully exited.  Otherwise, one of the following errors
    ///          may be returned:
    ///          + ErrorUnavailable if the screen is not currently in fullscreen mode.
    virtual Result ReleaseFullscreenOwnership() = 0;

    /// Sets the specified custom gamma ramp for this screen.
    ///
    /// The screen must be in fullscreen exclusive mode.
    ///
    /// @param [in] gammaRamp Specifies a scale, offset, and RGB float per control point on the gamma curve.
    ///
    /// @returns Success if the gamma ramp was successfully updated.  Otherwise, one of the following errors may be
    ///          returned:
    ///          + ErrorInvalidValue if any of the gamma ramp parameters are not in a valid range.
    ///          + ErrorUnavailable if the screen is not in fullscreen exclusive mode.
    virtual Result SetGammaRamp(
        const GammaRamp& gammaRamp) = 0;

    /// Returns a list of all formats supported in fullscreen mode by this screen.
    ///
    /// @param [in,out] pFormatCount     Input value specifies the maximum number of formats modes to enumerate, and
    ///                                  the output value specifies the total number of formats that were enumerated
    ///                                  in pFormats.  The input value is ignored if pScreenModeList is null.
    ///                                  This pointer must not be null.
    /// @param [out]    pFormatList      Output list of formats.  Can be null, in which case the total number of
    ///                                  available formats will be written to pFormatCount.
    ///
    /// @returns Success if this screen formats were successfully returned in pFormatList.  Otherwise, one of the
    ///          following errors may be returned:
    ///          + ErrorUnavailable if the screen is not in fullscreen exclusive mode.
    ///          + ErrorIncompleteResults if not all available formats were returned in pFormatList
    virtual Result GetFormats(
        uint32*          pFormatCount,
        SwizzledFormat*  pFormatList) = 0;

    /// Returns the colorspace and other related properties for this screen.
    /// This will return the current properties for the screen which may be modified using IScreen::SetColorSpace.
    /// Note that not all properties may be reported for a given screen since support can be dependent on:
    ///   display features, port, and drivers.
    ///
    /// @param [out] pCapabilities    Specifies the color capabilities of the given screen.
    ///
    /// @returns Success if the display's properties were successfully read into pCapabilities.
    ///          Otherwise, one of the following errors may be returned:
    ///          + ErrorInvalidValue if color space is not valid.
    ///          + ErrorUnknown if color space query is unsupported by the screen.
    virtual Result GetColorCapabilities(
        ScreenColorCapabilities* pCapabilities) = 0;

    /// Modifies the colorspace and/or other related properties for this screen.
    /// The caller should set the appropriate "Valid" flags in ScreenColorSpaceProperties for all properties
    /// that are to be modified.
    /// Note that not all properties may be modified for a given screen since support can be dependent on:
    ///   display features, port, and drivers.
    /// A call to IScreen::GetColorCapabilities can help to determine supported properties.
    ///
    /// @param [in] pColorConfig    Specifies the color configuration to set for the given screen.
    ///
    /// @returns Success if the display's properties were successfully updated to the requested values.
    ///          Otherwise, one of the following errors may be returned:
    ///          + ErrorInvalidValue if some property is not valid.
    ///          + ErrorUnsupported if changing properties unsupported by the screen.
    virtual Result SetColorConfiguration(
        const ScreenColorConfig* pColorConfig) = 0;

    /// Blocks until the start of this screen's next vertical blank period.
    ///
    /// @returns Success if the function successfully waited for the next vertical blank.  Otherwise, one of the
    ///          following errors may be returned:
    ///          + ErrorUnavailable if the screen is not in fullscreen exclusive mode.
    virtual Result WaitForVerticalBlank() const = 0;

    /// Returnes the current scanline for this screen.
    ///
    /// @param [out] pScanLine Current scanline of this screen or -1 if in vertical blank. Must not be null.
    ///
    /// @returns Success if the current scanline was successfully returned in pScanLine.  Otherwise, one of the
    ///          following errors may be returned:
    ///          + ErrorUnavailable if the screen is not in fullscreen exclusive mode.
    virtual Result GetScanLine(
        int32* pScanLine) const = 0;

    /// Acquires exclusive access to the screen. AcquireScreenAccess will lease crtcs, encoders and connectors from
    /// window system, and a new DRM master will be created to hold and control those lease objects. Once leased, those
    /// resources cannot be controlled by the window system, such as XServer, Wayland, unless the new DRM master is
    /// closed.
    /// AcquireScreenAccess can be called after SetRandrOutput is called.
    ///
    /// @param [in]   hDisplay        OS-native handle to the display.
    /// @param [in]   wsiPlatform     WSI platform.
    ///
    /// @returns Success if the call succeeded.
    virtual Result AcquireScreenAccess(
        OsDisplayHandle hDisplay,
        WsiPlatform     wsiPlatform) = 0;

    /// Closes the lease DRM master. It will return leased resources to window system and release exclusive access to
    /// the screen.
    ///
    /// @returns Success if the call succeeded.
    virtual Result ReleaseScreenAccess() = 0;

    /// Set RandR output object, which will be used to lease resources from XServer.
    ///
    /// @param [in] randrOutput  RandR output object.
    ///
    /// @returns Success if the call succeeded.
    virtual Result SetRandrOutput(
        uint32 randrOutput) = 0;

    /// Returns the value of the associated arbitrary client data pointer.
    /// Can be used to associate arbitrary data with a particular PAL object.
    ///
    /// @returns Pointer to client data.
    PAL_INLINE void* GetClientData() const
    {
        return m_pClientData;
    }

    /// Sets the value of the associated arbitrary client data pointer.
    /// Can be used to associate arbitrary data with a particular PAL object.
    ///
    /// @param  [in]    pClientData     A pointer to arbitrary client data.
    PAL_INLINE void SetClientData(
        void* pClientData)
    {
        m_pClientData = pClientData;
    }

protected:
    /// @internal Constructor. Prevent use of new operator on this interface. Client must create objects by explicitly
    /// called the proper create method.
    IScreen() : m_pClientData(nullptr) {}

    /// @internal Destructor.  Prevent use of delete operator on this interface.  Screens will be destroyed via
    /// IDestroyable::Destroy().
    virtual ~IScreen() { }

private:
    /// @internal Client data pointer. This can have an arbitrary value and can be returned by calling GetClientData()
    /// and set via SetClientData().
    /// For non-top-layer objects, this will point to the layer above the current object.
    void* m_pClientData;
};

} // Pal
