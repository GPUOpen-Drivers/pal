/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/device.h"
#include "core/image.h"
#include "core/platform.h"
#include "core/privateScreen.h"
#include "palFormatInfo.h"

using namespace Util;
using namespace Pal::Formats;

namespace Pal
{
// A table contains all possible private screen formats
const SwizzledFormat AllPrivateScreenFormats[MaxPrivateScreenFormats] =
{
    {
        ChNumFormat::X5Y6Z5_Unorm,
        { ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::One },
    },
    {
        ChNumFormat::X8Y8Z8W8_Unorm,
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W   },
    },
    {
        ChNumFormat::X8Y8Z8W8_Srgb,
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W   },
    },
    {
        ChNumFormat::X8Y8Z8W8_Unorm,
        { ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::W   },
    },
    {
        ChNumFormat::X8Y8Z8W8_Srgb,
        { ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::W   },
    },
    {
        ChNumFormat::X10Y10Z10W2_Unorm,
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W   },
    },
    {
        ChNumFormat::X10Y10Z10W2_Unorm,
        { ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::W   },
    },
    {
        ChNumFormat::X16Y16Z16W16_Float,
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W   },
    },
};

static_assert(MaxPrivateScreenFormats <= 32, "MaxPrivateScreenFormats exceeds 32, a uint32 bitmask cannot hold them!");

// =====================================================================================================================
// Helper function to find the index of specified format and update the m_formatCaps table.
void PrivateScreen::UpdateSupportedFormats(
    SwizzledFormat format)
{
    uint32 idx = 0;
    for (; idx < MaxPrivateScreenFormats; idx++)
    {
        if (memcmp(&format, &AllPrivateScreenFormats[idx], sizeof(format)) == 0)
        {
            break;
        }
    }

    // idx == MaxPrivateScreenFormats means this format isn't supported by PAL.
    if (idx != MaxPrivateScreenFormats)
    {
        if ((m_formatCaps & (1 << idx)) == 0)
        {
            m_formatCaps |= (1 << idx);
            // As two formats reported by KMD may be mapped the same Pal format, this ensures we report unique formats.
            m_properties.pFormats[m_reportedFormats++] = AllPrivateScreenFormats[idx];

            // If the unorm format is supported, add the srgb format as KMD does not report this.
            if (format.format == ChNumFormat::X8Y8Z8W8_Unorm)
            {
                m_formatCaps |= (1 << (idx + 1));
                PAL_ASSERT(AllPrivateScreenFormats[idx+1].format == ChNumFormat::X8Y8Z8W8_Srgb);

                m_properties.pFormats[m_reportedFormats++] = AllPrivateScreenFormats[idx+1];
            }
        }
    }
}

// =====================================================================================================================
PrivateScreen::PrivateScreen(
    Device*                        pDevice,
    const PrivateScreenCreateInfo& createInfo)
    :
    m_pDevice(pDevice),
    m_index(createInfo.index),
    m_dummyHandle(0),
    m_properties(createInfo.props),
    m_pOwner(nullptr),
    m_removed(false),
    m_enabled(false),
    m_imageMask(0),
    m_lastPresentFormat(),
    m_formatCaps(0),
    m_reportedFormats(0),
    m_bridgeCount(0)
{
    memset(m_pImages, 0, sizeof(m_pImages));
    memset(m_properties.pFormats, 0, m_properties.numFormats * sizeof(SwizzledFormat));
    memset(&m_displayMode, 0, sizeof(PrivateDisplayMode));
}

// =====================================================================================================================
PrivateScreen::~PrivateScreen()
{
    // m_pOwner shouldn't be nullptr for any usable private screens, but if the destructor is caused by a failure
    // of initializing, the mutex is a nullptr.
    if (m_pOwner != nullptr)
    {
        m_pDevice->PrivateScreenDestroyNotication(m_pOwner);
    }

    // m_properties.pFormats is monolithically allocated with PrivateScreen object.
    m_properties.pFormats = nullptr;
    for (uint32 i = 0; i < MaxPrivateScreenImages; i++)
    {
        if (m_pImages[i] != nullptr)
        {
            m_pImages[i]->InvalidatePrivateScreen();
        }
    }
}

// =====================================================================================================================
Result PrivateScreen::InitPhysical()
{
    PAL_ASSERT(m_properties.type != PrivateScreenType::Emulated);

    return OsInitProperties();
}

// =====================================================================================================================
Result PrivateScreen::InitEmulated()
{
    PAL_ASSERT(m_properties.type == PrivateScreenType::Emulated);

    // Assuming that an emulated private screen supports all formats.
    for (uint32 i = 0; i < m_properties.numFormats; i++)
    {
        m_properties.pFormats[i] = AllPrivateScreenFormats[i];
    }

    return Result::Success;
}

// =====================================================================================================================
// Gets extent, refresh rate, target id, supported formats, EDID etc. of this private screen.
Result PrivateScreen::GetProperties(
    size_t*                  pNumFormats,
    size_t*                  pEdidSize,
    PrivateScreenProperties* pInfo
    ) const
{
    Result result = m_removed ? Result::ErrorPrivateScreenRemoved : Result::Success;

    if (m_removed == false)
    {
        if (pInfo == nullptr)
        {
            // This is to report sizes so both pointers must be valid.
            if ((pNumFormats != nullptr) && (pEdidSize != nullptr))
            {
                *pNumFormats = m_properties.numFormats;
                *pEdidSize   = m_properties.edidSize;
            }
            else
            {
                result = Result::ErrorInvalidPointer;
            }
        }
        else
        {
            if (((pNumFormats != nullptr) && (*pNumFormats < m_properties.numFormats)) ||
                ((pEdidSize != nullptr) && (*pEdidSize < m_properties.edidSize)))
            {
                result = Result::ErrorInvalidMemorySize;
            }
            else
            {
                pInfo->extent      = m_properties.extent;
                pInfo->targetId    = m_properties.targetId;
                pInfo->type        = m_properties.type;
                pInfo->refreshRate = m_properties.refreshRate;
                pInfo->hash        = m_properties.hash;
                pInfo->edidSize    = m_properties.edidSize;
                if (pEdidSize != nullptr)
                {
                    memcpy(pInfo->edid, m_properties.edid, sizeof(m_properties.edid));
                }
                pInfo->numFormats  = m_properties.numFormats;
                if (pNumFormats != nullptr)
                {
                    memcpy(pInfo->pFormats, m_properties.pFormats, sizeof(SwizzledFormat) * m_properties.numFormats);
                }
                pInfo->maxNumPowerSwitches = m_properties.maxNumPowerSwitches;
                pInfo->powerSwitchLatency  = m_properties.powerSwitchLatency;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Get present statistics of this private screen. This is for users to measure the performance data of present.
Result PrivateScreen::GetPresentStats(
    PrivateScreenPresentStats* pStats)
{
    Result result = m_removed ? Result::ErrorPrivateScreenRemoved : Result::ErrorInvalidPointer;

    if ((m_removed == false) && (pStats != nullptr))
    {
        result = OsGetPresentStats(pStats);

        if (result == Result::ErrorPrivateScreenRemoved)
        {
            m_removed = true;
        }
    }

    return result;
}

// =====================================================================================================================
// Enables the private screen (provides exclusive ownership of the screen).
Result PrivateScreen::Enable(
    const PrivateScreenEnableInfo& info)
{
    Result result = Result::ErrorPrivateScreenRemoved;

    if (m_enabled == true)
    {
        result = Result::ErrorPrivateScreenUsed;
    }
    else if (m_removed == false)
    {
        result = OsEnable(info);

        if (result == Result::Success)
        {
            m_enabled = true;
        }
        else if (result == Result::ErrorPrivateScreenRemoved)
        {
            m_removed = true;
        }
    }

    return result;
}

// =====================================================================================================================
// Disables the private screen (releases exclusive ownership of the screen).
Result PrivateScreen::Disable()
{
    Result result = Result::ErrorPrivateScreenRemoved;

    if (m_enabled == false)
    {
        result = Result::ErrorPrivateScreenNotEnabled;
    }
    else if (m_removed == false)
    {
        result = OsDisable();

        if (result == Result::Success)
        {
            m_enabled = false;
        }
        else if (result == Result::ErrorPrivateScreenRemoved)
        {
            m_removed = true;
        }
    }

    return result;
}

// =====================================================================================================================
// Blanks the private screen (turns the display off). This is for application to do some power management and screen
// saver features.
Result PrivateScreen::Blank()
{
    PAL_ASSERT(m_properties.type != PrivateScreenType::Emulated);

    Result result = Result::ErrorPrivateScreenRemoved;

    if (m_enabled == false)
    {
        result = Result::ErrorPrivateScreenNotEnabled;
    }
    else if (m_removed == false)
    {
        result = OsBlank();

        if (result == Result::ErrorPrivateScreenRemoved)
        {
            m_removed = true;
        }
    }

    return result;
}

// =====================================================================================================================
// Presents on the private screen. Presentation operations for private screens have "flip" semantics. The application
// is responsible for implementation of a swap chain from a required number of presentable images.
// The private screen must be enabled before presentation and the first present call lights up the screen before
// displaying an image.
Result PrivateScreen::Present(
    const PrivateScreenPresentInfo& presentInfo)
{
    Result result = Result::ErrorPrivateScreenRemoved;

    if (m_enabled == false)
    {
        result = Result::ErrorPrivateScreenNotEnabled;
    }
    else if (m_removed == false)
    {
        if (presentInfo.pSrcImg == nullptr)
        {
            result = Result::ErrorInvalidPointer;
        }
        else
        {
            Image* pImage = static_cast<Image*>(presentInfo.pSrcImg);
            // If this is not a private screen image, GetPrivateScreen() should return nullptr.
            if (pImage->GetPrivateScreen() == this)
            {
                PAL_ASSERT(pImage->IsPrivateScreenPresent());
                result = OsPresent(presentInfo);
            }
            else // This private screen image is not created on this private screen.
            {
                result = Result::ErrorInvalidImage;
            }
        }

        if (result == Result::ErrorPrivateScreenRemoved)
        {
            m_removed = true;
        }
    }

    return result;
}

// =====================================================================================================================
Result PrivateScreen::GetScanLine(
    int32* pScanLine)
{
    PAL_ASSERT(m_properties.type != PrivateScreenType::Emulated);
    PAL_ASSERT(pScanLine != nullptr);

    Result result = Result::ErrorPrivateScreenRemoved;

    if (m_enabled == false)
    {
        result = Result::ErrorPrivateScreenNotEnabled;
    }
    else if (m_removed == false)
    {
        result = OsGetScanLine(pScanLine);
    }
    return result;
}

// =====================================================================================================================
// GetConnectorProperties should be call twice. In the first call, the size of buffer (pDataSize) will be reported
// which will be used by caller to allocate the buffer. In the second call, the caller pass in the buffer and retrieve
// the connector properties.
Result PrivateScreen::GetConnectorProperties(
    uint32*                           pDataSize,
    PrivateScreenConnectorProperties* pConnectorProperties)
{
    PAL_ASSERT(m_properties.type != PrivateScreenType::Emulated);
    PAL_ASSERT(pDataSize != nullptr);

    Result result = Result::ErrorPrivateScreenRemoved;

    if (m_removed == false)
    {
        result = OsGetConnectorProperties(pDataSize, pConnectorProperties);
    }

    return result;
}

// =====================================================================================================================
// Reports the cached data of PrivateDisplayMode, it is set when calling PrivateScreen::Enable or
// PrivateScreen::SetDisplayMode.
Result PrivateScreen::GetDisplayMode(
    PrivateDisplayMode* pMode)
{
    PAL_ASSERT(m_properties.type != PrivateScreenType::Emulated);
    PAL_ASSERT(pMode != nullptr);

    Result result = Result::ErrorPrivateScreenRemoved;

    if (m_enabled == false)
    {
        result = Result::ErrorPrivateScreenNotEnabled;
    }
    else if (m_removed == false)
    {
        memcpy(pMode, &m_displayMode, sizeof(PrivateDisplayMode));
        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Set and cached the display mode.
Result PrivateScreen::SetDisplayMode(
    const PrivateDisplayMode& displayMode)
{
    PAL_ASSERT(m_properties.type != PrivateScreenType::Emulated);

    Result result = Result::ErrorPrivateScreenRemoved;

    if (m_enabled == false)
    {
        result = Result::ErrorPrivateScreenNotEnabled;
    }
    else if (m_removed == false)
    {
        result = OsSetDisplayMode(displayMode);

        if (result == Result::Success)
        {
            memcpy(&m_displayMode, &displayMode, sizeof(PrivateDisplayMode));
        }
    }
    return result;
}

// =====================================================================================================================
// Set the gamma ramp for private screen, note that the scale and offset fields in GAMMA_RAMP_FLOAT are not used
// by KMD.
Result PrivateScreen::SetGammaRamp(
    const GammaRamp* pGammaRamp)
{
    PAL_ASSERT(m_properties.type != PrivateScreenType::Emulated);

    Result result = Result::ErrorPrivateScreenRemoved;

    if (m_enabled == false)
    {
        result = Result::ErrorPrivateScreenNotEnabled;
    }
    else if (m_removed == false)
    {
        result = OsSetGammaRamp(pGammaRamp);
    }
    return result;
}

// =====================================================================================================================
// Set power mode (turn power on or power off).
Result PrivateScreen::SetPowerMode(
    PrivateDisplayPowerState poweMode)
{
    PAL_ASSERT(m_properties.type != PrivateScreenType::Emulated);

    Result result = Result::ErrorPrivateScreenRemoved;

    if (m_enabled == false)
    {
        result = Result::ErrorPrivateScreenNotEnabled;
    }
    else if (m_removed == false)
    {
        result = OsSetPowerMode(poweMode);
    }
    return result;
}

// =====================================================================================================================
// Set color transform matrix, the maxtrix (include scaling, bias) will multiply with the output color.
Result PrivateScreen::SetColorMatrix(
    const ColorTransform& matrix)
{
    PAL_ASSERT(m_properties.type != PrivateScreenType::Emulated);

    Result result = Result::ErrorPrivateScreenRemoved;

    if (m_enabled == false)
    {
        result = Result::ErrorPrivateScreenNotEnabled;
    }
    else if (m_removed == false)
    {
        result = OsSetColorMatrix(matrix);
    }
    return result;
}

// =====================================================================================================================
// Sets an event handle to be signalled by KMD after a vsync occurs with a specified delay time in microseconds.
Result PrivateScreen::SetEventAfterVsync(
    OsExternalHandle hEvent,
    uint32           delayInUs,
    bool             repeated)
{
    Result result = Result::ErrorPrivateScreenRemoved;

    if (m_enabled == false)
    {
        result = Result::ErrorPrivateScreenNotEnabled;
    }
    else if (m_removed == false)
    {
        result = OsSetEventAfterVsync(hEvent, delayInUs, repeated);
    }

    return result;
}

// =====================================================================================================================
Result PrivateScreen::GetHdcpStatus(
    HdcpStatus* pStatus)
{
    Result result = Result::ErrorPrivateScreenRemoved;

    if (m_enabled == false)
    {
        result = Result::ErrorPrivateScreenNotEnabled;
    }
    else if (m_removed == false)
    {
        result = OsGetHdcpStatus(pStatus);
    }

    return result;
}

// =====================================================================================================================
Result PrivateScreen::EnableAudio(
    bool enable)
{
    Result result = Result::ErrorPrivateScreenRemoved;

    if (m_removed == false)
    {
        result = OsEnableAudio(enable);
    }

    return result;
}

// =====================================================================================================================
// The private screen only support limited number (16) of presentable images as in KMD, it only stores up to 16 flip
// addresses so an integer is used to index in the array of 16 flip addresses. We call this index "image id". The image
// image is generated by scanning a unused bit in m_imageMask.
Result PrivateScreen::ObtainImageId(
    uint32* pImageId)
{
    PAL_ASSERT(pImageId != nullptr);

    Result result = Result::Success;
    *pImageId = MaxPrivateScreenImages;

    for (uint32 i = 0; i < MaxPrivateScreenImages; i++)
    {
        if ((m_imageMask & (1 << i)) == 0)
        {
            *pImageId = i;
            m_imageMask |= (1 << i);
            break;
        }
    }

    if (*pImageId == MaxPrivateScreenImages)
    {
        result = Result::ErrorTooManyPrivateDisplayImages;
    }

    return result;
}

// =====================================================================================================================
// Once applicaiton destroys a private screen presentable image, the used bit in m_imageMask should be cleared, next
// creation private screen presentable image may use that slot.
void PrivateScreen::ReturnImageId(
    uint32 imageId)
{
    if (imageId < MaxPrivateScreenImages)
    {
        m_imageMask &= ~(1 << imageId);
        m_pImages[imageId] = nullptr;
    }
}

// =====================================================================================================================
// Sets a image pointer into the specified slots inside private screen. This is to clear the image's private screen info
// when associated private screen is destroyed (and before application clears those images).
void PrivateScreen::SetImageSlot(
    uint32 imageId,
    Image* pImage)
{
    if (imageId < MaxPrivateScreenImages)
    {
        m_pImages[imageId] = pImage;
    }
}

// =====================================================================================================================
bool PrivateScreen::FormatSupported(
    SwizzledFormat format
    ) const
{
    bool supported = false;

    for (uint32 i = 0; i < m_properties.numFormats; i++)
    {
        const auto& cachedFormat = m_properties.pFormats[i];
        if (memcmp(&format, &cachedFormat, sizeof(format)) == 0)
        {
            supported = true;
            break;
        }
    }

    return supported;
}

} // Pal
