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

#include "core/hw/gfxip/gfx9/gfx9FormatInfo.h"
#include "core/hw/gfxip/gfx9/g_gfx9MergedDataFormats.h"
#include "core/hw/gfxip/gfx9/g_gfx9PalSettings.h"
#include "palDevice.h"

using namespace Util;

namespace Pal
{
namespace Formats
{
namespace Gfx9
{
static_assert(ArrayLen(Gfx9MergedFormatPropertiesTable.features) == static_cast<size_t>(ChNumFormat::Count),
              "Size of Gfx9MergedFormatPropertiesTable mismatches the number of declared ChNumFormat enums");
static_assert(ArrayLen(Gfx9MergedChannelFmtInfoTbl) == static_cast<size_t>(ChNumFormat::Count),
              "Size of Gfx9MergedChannelFmtInfoTbl mismatches the number of declared ChNumFormat enums");

// =====================================================================================================================
// Returns the format info table for the specific GfxIpLevel.
const MergedFmtInfo* MergedChannelFmtInfoTbl(
    GfxIpLevel                        gfxIpLevel,
    const Pal::Gfx9::Gfx9PalSettings* pSettings)
{
    const MergedFmtInfo* pFmtInfo = nullptr;

    switch (gfxIpLevel)
    {
    case GfxIpLevel::GfxIp9:
        pFmtInfo = Gfx9MergedChannelFmtInfoTbl;
        break;
    case GfxIpLevel::GfxIp10_1:
        // Do *not* return "Gfx10MergedChannelFmtInfoTbl" here!  That table is of a different type; GFX10
        // users need to call "Gfx10MergedChannelFmtInfoTbl" instead.

        // break intentionally removed
    default:
        // What is this?
        PAL_ASSERT_ALWAYS();
    }

    return pFmtInfo;
}

// =====================================================================================================================
// Returns the format info table for the specific GfxIpLevel.
const MergedFlatFmtInfo* MergedChannelFlatFmtInfoTbl(
    GfxIpLevel                        gfxIpLevel,
    const Pal::Gfx9::Gfx9PalSettings* pSettings)
{
    PAL_ASSERT(IsGfx10(gfxIpLevel));
    const MergedFlatFmtInfo*  pFlatFmtInfo = nullptr;

    {
        pFlatFmtInfo = Gfx10MergedChannelFmtInfoTbl;
    }

    return pFlatFmtInfo;
}

// =====================================================================================================================
// Retrieves the hardware color-buffer format for a given PAL format type.
// This is specifically for exports and is intended to be called at compile time
ColorFormat HwColorFormatForExport(
    GfxIpLevel       gfxLevel,
    Pal::ChNumFormat format)
{
    ColorFormat hwColorFmt = COLOR_INVALID;

    if (gfxLevel == GfxIpLevel::GfxIp9)
    {
        hwColorFmt = HwColorFmt(MergedChannelFmtInfoTbl(gfxLevel, nullptr), format);
    }
    else
    {
        PAL_ASSERT(IsGfx10(gfxLevel));
        hwColorFmt = HwColorFmt(MergedChannelFlatFmtInfoTbl(gfxLevel, nullptr), format);
    }

    PAL_ASSERT(hwColorFmt != COLOR_INVALID);
    return hwColorFmt;
}

// Lookup table for converting PAL swizzle types to HW enums.
constexpr SQ_SEL_XYZW01 ChannelSwizzleTbl[] =
{
    SQ_SEL_0,
    SQ_SEL_1,
    SQ_SEL_X,
    SQ_SEL_Y,
    SQ_SEL_Z,
    SQ_SEL_W,
};

// Lookup table for converting HW swizzle enums to PAL types.
constexpr ChannelSwizzle HwSwizzleTbl[] =
{
    ChannelSwizzle::Zero,  // SQ_SEL_0
    ChannelSwizzle::One,   // SQ_SEL_1
    ChannelSwizzle::Count, // SQ_SEL_RESERVED_0
    ChannelSwizzle::Count, // SQ_SEL_RESERVED_1
    ChannelSwizzle::X,     // SQ_SEL_X
    ChannelSwizzle::Y,     // SQ_SEL_Y
    ChannelSwizzle::Z,     // SQ_SEL_Z
    ChannelSwizzle::W,     // SQ_SEL_W
};

// =====================================================================================================================
// Returns the SQ_SEL_XYZW01 enum corresponding to the specified PAL channel swizzle. This enum is used when programming
// the texture block.
SQ_SEL_XYZW01 HwSwizzle(
    ChannelSwizzle swizzle)
{
    PAL_ASSERT(swizzle != ChannelSwizzle::Count);
    return ChannelSwizzleTbl[static_cast<uint32>(swizzle)];
}

// =====================================================================================================================
// Returns the ChannelSwizzle corresponding to the specified SQ_SEL_XYZW01 enum.
ChannelSwizzle ChannelSwizzleFromHwSwizzle(
    SQ_SEL_XYZW01 hwSwizzle)
{
    PAL_ASSERT((hwSwizzle <= SQ_SEL_W) && (HwSwizzleTbl[hwSwizzle] != ChannelSwizzle::Count));
    return HwSwizzleTbl[hwSwizzle];
}

// =====================================================================================================================
// Returns the IMG_DATA_FORMAT enum corresponding to the specified PAL channel format.  This enum is used when
// programming the texture block.
IMG_DATA_FORMAT HwImgDataFmt(
    const MergedFmtInfo fmtInfo[],
    ChNumFormat         format)
{
    // Assert if we're looking at the wrong table entry.
    PAL_ASSERT(fmtInfo[static_cast<uint32>(format)].format == format);
    return fmtInfo[static_cast<uint32>(format)].hwImgDataFmt;
}

// =====================================================================================================================
// Returns the IMG_NUM_FORMAT enum corresponding to the specified PAL numeric format.  This enum is used when
// programming the texture block.
IMG_NUM_FORMAT HwImgNumFmt(
    const MergedFmtInfo fmtInfo[],
    ChNumFormat         format)
{
    // Assert if we're looking at the wrong table entry.
    PAL_ASSERT(fmtInfo[static_cast<uint32>(format)].format == format);
    return static_cast<IMG_NUM_FORMAT>(fmtInfo[static_cast<uint32>(format)].hwImgNumFmt);
}

// =====================================================================================================================
// Returns the IMG_FMT enum corresponding to the specified PAL channel format.  This enum is used when programming the
// texture block.
IMG_FMT HwImgFmt(
    const MergedFlatFmtInfo fmtInfo[],
    ChNumFormat             format)
{
    // Assert if we're looking at the wrong table entry.
    PAL_ASSERT(fmtInfo[static_cast<uint32>(format)].format == format);
    return fmtInfo[static_cast<uint32>(format)].hwImgFmt;
}

// =====================================================================================================================
// Returns the PAL channel format corresponding to the specified IMG_DATA_FORMAT enum or Undefined if an error occurred.
ChNumFormat FmtFromHwImgFmt(
    IMG_DATA_FORMAT imgDataFmt,
    IMG_NUM_FORMAT  imgNumFmt,
    GfxIpLevel      gfxIpLevel)
{
    // Get the right table for our GFXIP level.
    const MergedImgDataFmtInfo* pImgDataFmtTbl  = nullptr;
    uint32                      imgDataFmtCount = 0;

    switch (gfxIpLevel)
    {
    case GfxIpLevel::GfxIp9:
        pImgDataFmtTbl  = Gfx9MergedImgDataFmtTbl;
        imgDataFmtCount = Gfx9MergedImgDataFmtCount;
        break;
    default:
        // What is this?
        PAL_ASSERT_ALWAYS();
    }

    ChNumFormat format = ChNumFormat::Undefined;

    if (imgDataFmt < imgDataFmtCount)
    {
        // Assert if we're looking at the wrong table entry.
        PAL_ASSERT(pImgDataFmtTbl[imgDataFmt].imgDataFmt == imgDataFmt);

        // TODO: We should see if we even need the L8/P8/L16/L8A8... formats to be available here.
        format = pImgDataFmtTbl[imgDataFmt].mappings[imgNumFmt][0];
    }

    return format;
}

// =====================================================================================================================
// Returns the PAL channel format corresponding to the specified IMG_FMT enum or Undefined if an error occurred.
ChNumFormat FmtFromHwImgFmt(
    IMG_FMT     imgFmt,
    GfxIpLevel  gfxIpLevel)
{
    PAL_ASSERT(IsGfx10(gfxIpLevel));

    ChNumFormat format = ChNumFormat::Undefined;

    if (imgFmt < Gfx10MergedImgDataFmtCount)
    {
        format = Gfx10MergedImgDataFmtTbl[imgFmt];
    }

    return format;
}

// =====================================================================================================================
// Returns the BUF_DATA_FORMAT enum corresponding to the specified PAL channel format.  This enum is used when
// programming the texture block.
BUF_DATA_FORMAT HwBufDataFmt(
    const MergedFmtInfo fmtInfo[],
    ChNumFormat         format)
{
    // Assert if we're looking at the wrong table entry.
    PAL_ASSERT(fmtInfo[static_cast<uint32>(format)].format == format);
    return fmtInfo[static_cast<uint32>(format)].hwBufDataFmt;
}

// =====================================================================================================================
// Returns the BUF_NUM_FORMAT enum corresponding to the specified PAL numeric format.  This enum is used when
// programming the texture block.
BUF_NUM_FORMAT HwBufNumFmt(
    const MergedFmtInfo fmtInfo[],
    ChNumFormat         format)
{
    // Assert if we're looking at the wrong table entry.
    PAL_ASSERT(fmtInfo[static_cast<uint32>(format)].format == format);
    return fmtInfo[static_cast<uint32>(format)].hwBufNumFmt;
}

// =====================================================================================================================
// Returns the BUF_FMT enum corresponding to the specified PAL channel format.  This enum is used when programming the
// texture block.
BUF_FMT HwBufFmt(
    const MergedFlatFmtInfo fmtInfo[],
    ChNumFormat             format)
{
    // Assert if we're looking at the wrong table entry.
    PAL_ASSERT(fmtInfo[static_cast<uint32>(format)].format == format);
    return fmtInfo[static_cast<uint32>(format)].hwBufFmt;
}

// =====================================================================================================================
// Returns the PAL channel format corresponding to the specified BUF_DATA_FORMAT enum or Undefined if an error occurred.
ChNumFormat FmtFromHwBufFmt(
    BUF_DATA_FORMAT bufDataFmt,
    BUF_NUM_FORMAT  bufNumFmt,
    GfxIpLevel      gfxIpLevel)
{
    // Get the right table for our GFXIP level.
    const MergedBufDataFmtInfo* pBufDataFmtTbl  = nullptr;
    uint32                      bufDataFmtCount = 0;

    switch (gfxIpLevel)
    {
    case GfxIpLevel::GfxIp9:
        pBufDataFmtTbl  = Gfx9MergedBufDataFmtTbl;
        bufDataFmtCount = Gfx9MergedBufDataFmtCount;
        break;
    default:
        // What is this?
        PAL_ASSERT_ALWAYS();
    }

    ChNumFormat format = ChNumFormat::Undefined;

    if (bufDataFmt < bufDataFmtCount)
    {
        // Assert if we're looking at the wrong table entry.
        PAL_ASSERT(pBufDataFmtTbl[bufDataFmt].bufDataFmt == bufDataFmt);

        // TODO: We should see if we even need the A8/L8/P8/L16/L8A8... formats to be available here.
        format = pBufDataFmtTbl[bufDataFmt].mappings[bufNumFmt][0];
    }

    return format;
}

// =====================================================================================================================
// Returns the PAL channel format corresponding to the specified BUF_FMT enum or Undefined if an error occurred.
ChNumFormat FmtFromHwBufFmt(
    BUF_FMT     bufFmt,
    GfxIpLevel  gfxIpLevel)
{
    PAL_ASSERT(IsGfx10(gfxIpLevel));

    ChNumFormat format = ChNumFormat::Undefined;

    if (bufFmt < Gfx10MergedBufDataFmtCount)
    {
        format = Gfx10MergedBufDataFmtTbl[bufFmt];
    }

    return format;
}

// =====================================================================================================================
// Returns the ColorFormat enum corresponding to the specified PAL channel format.  This enum is used when programming
// the CB block.
ColorFormat HwColorFmt(
    const MergedFmtInfo fmtInfo[],
    ChNumFormat         format)
{
    // Assert if we're looking at the wrong table entry.
    PAL_ASSERT(fmtInfo[static_cast<uint32>(format)].format == format);
    return fmtInfo[static_cast<uint32>(format)].hwColorFmt;
}

// =====================================================================================================================
// Returns the ColorFormat enum corresponding to the specified PAL channel format.  This enum is used when programming
// the CB block.
ColorFormat HwColorFmt(
    const MergedFlatFmtInfo fmtInfo[],
    ChNumFormat             format)
{
    // Assert if we're looking at the wrong table entry.
    PAL_ASSERT(fmtInfo[static_cast<uint32>(format)].format == format);
    return fmtInfo[static_cast<uint32>(format)].hwColorFmt;
}

// =====================================================================================================================
// Determines the CB component swap mode for the given channel format.
SurfaceSwap ColorCompSwap(
    SwizzledFormat swizzledFormat)
{
    SurfaceSwap surfSwap = SWAP_STD;

    const uint32 numComponents = NumComponents(swizzledFormat.format);
    const auto&  swizzle       = swizzledFormat.swizzle;
    if (numComponents == 1)
    {
        if (swizzle.r == ChannelSwizzle::X)
        {
            surfSwap = SWAP_STD;
        }
        else if (swizzle.g == ChannelSwizzle::X)
        {
            surfSwap = SWAP_ALT;
        }
        else if (swizzle.a == ChannelSwizzle::X)
        {
            surfSwap = SWAP_ALT_REV;
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }
    }
    else if (numComponents == 2)
    {
        if ((swizzle.r == ChannelSwizzle::X) && (swizzle.g == ChannelSwizzle::Y))
        {
            surfSwap = SWAP_STD;
        }
        else if ((swizzle.r == ChannelSwizzle::X) && (swizzle.a == ChannelSwizzle::Y))
        {
            surfSwap = SWAP_ALT;
        }
        else if ((swizzle.g == ChannelSwizzle::X) && (swizzle.r == ChannelSwizzle::Y))
        {
            surfSwap = SWAP_STD_REV;
        }
        else if ((swizzle.a == ChannelSwizzle::X) && (swizzle.r == ChannelSwizzle::Y))
        {
            surfSwap = SWAP_ALT_REV;
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }
    }
    else if (numComponents == 3)
    {
        if ((swizzle.r == ChannelSwizzle::X) &&
            (swizzle.g == ChannelSwizzle::Y) &&
            (swizzle.b == ChannelSwizzle::Z))
        {
            surfSwap = SWAP_STD;
        }
        else if ((swizzle.r == ChannelSwizzle::X) &&
                 (swizzle.g == ChannelSwizzle::Y) &&
                 (swizzle.a == ChannelSwizzle::Z))
        {
            surfSwap = SWAP_ALT;
        }
        else if ((swizzle.b == ChannelSwizzle::X) &&
                 (swizzle.g == ChannelSwizzle::Y) &&
                 (swizzle.r == ChannelSwizzle::Z))
        {
            surfSwap = SWAP_STD_REV;
        }
        else if ((swizzle.a == ChannelSwizzle::X) &&
                 (swizzle.g == ChannelSwizzle::Y) &&
                 (swizzle.r == ChannelSwizzle::Z))
        {
            surfSwap = SWAP_ALT_REV;
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }
    }
    else if (numComponents == 4)
    {
        if ((swizzle.r == ChannelSwizzle::X) &&
            (swizzle.g == ChannelSwizzle::Y) &&
            (swizzle.b == ChannelSwizzle::Z) &&
            ((swizzle.a == ChannelSwizzle::W) || (swizzle.a == ChannelSwizzle::One)))
        {
            surfSwap = SWAP_STD;
        }
        else if ((swizzle.b == ChannelSwizzle::X) &&
                 (swizzle.g == ChannelSwizzle::Y) &&
                 (swizzle.r == ChannelSwizzle::Z) &&
                 ((swizzle.a == ChannelSwizzle::W) || (swizzle.a == ChannelSwizzle::One)))
        {
            surfSwap = SWAP_ALT;
        }
        else if ((swizzle.a == ChannelSwizzle::X) &&
                 (swizzle.b == ChannelSwizzle::Y) &&
                 (swizzle.g == ChannelSwizzle::Z) &&
                 (swizzle.r == ChannelSwizzle::W))
        {
            surfSwap = SWAP_STD_REV;
        }
        else if ((swizzle.a == ChannelSwizzle::X) &&
                 (swizzle.r == ChannelSwizzle::Y) &&
                 (swizzle.g == ChannelSwizzle::Z) &&
                 (swizzle.b == ChannelSwizzle::W))
        {
            surfSwap = SWAP_ALT_REV;
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    return surfSwap;
}

// =====================================================================================================================
// Returns the SurfaceNumber enum corresponding to the specified PAL numeric format.  This enum is used when
// programming the CB block.
SurfaceNumber ColorSurfNum(
    const MergedFmtInfo fmtInfo[],
    ChNumFormat         format)
{
    // Assert if we're looking at the wrong table entry.
    PAL_ASSERT(fmtInfo[static_cast<uint32>(format)].format == format);
    return fmtInfo[static_cast<uint32>(format)].hwColorNumFmt;
}

// =====================================================================================================================
// Returns the SurfaceNumber enum corresponding to the specified PAL numeric format.  This enum is used when
// programming the CB block.
SurfaceNumber ColorSurfNum(
    const MergedFlatFmtInfo fmtInfo[],
    ChNumFormat             format)
{
    // Assert if we're looking at the wrong table entry.
    PAL_ASSERT(fmtInfo[static_cast<uint32>(format)].format == format);
    return fmtInfo[static_cast<uint32>(format)].hwColorNumFmt;
}

// =====================================================================================================================
// Returns the ZFormat enum corresponding to the specified PAL channel format.  This enum is used when programming
// the DB block.
ZFormat HwZFmt(
    const MergedFmtInfo fmtInfo[],
    ChNumFormat         format)
{
    // Assert if we're looking at the wrong table entry.
    PAL_ASSERT(fmtInfo[static_cast<uint32>(format)].format == format);
    return fmtInfo[static_cast<uint32>(format)].hwZFmt;
}

// =====================================================================================================================
// Returns the ZFormat enum corresponding to the specified PAL channel format.  This enum is used when programming
// the DB block.
ZFormat HwZFmt(
    const MergedFlatFmtInfo fmtInfo[],
    ChNumFormat             format)
{
    // Assert if we're looking at the wrong table entry.
    PAL_ASSERT(fmtInfo[static_cast<uint32>(format)].format == format);
    return fmtInfo[static_cast<uint32>(format)].hwZFmt;
}

// =====================================================================================================================
// Returns the StencilFormat enum corresponding to the specified PAL channel format.  This enum is used when programming
// the DB block.
StencilFormat HwStencilFmt(
    const MergedFmtInfo fmtInfo[],
    ChNumFormat         format)
{
    // Assert if we're looking at the wrong table entry.
    PAL_ASSERT(fmtInfo[static_cast<uint32>(format)].format == format);
    return fmtInfo[static_cast<uint32>(format)].hwStencilFmt;
}

// =====================================================================================================================
// Returns the StencilFormat enum corresponding to the specified PAL channel format.  This enum is used when programming
// the DB block.
StencilFormat HwStencilFmt(
    const MergedFlatFmtInfo fmtInfo[],
    ChNumFormat             format)
{
    // Assert if we're looking at the wrong table entry.
    PAL_ASSERT(fmtInfo[static_cast<uint32>(format)].format == format);
    return fmtInfo[static_cast<uint32>(format)].hwStencilFmt;
}

// =====================================================================================================================
// Returns true if this channel format can support fast color clears.
bool SupportsFastColorClear(
    ChNumFormat format)
{
    return ((IsYuv(format) == false) && (BitsPerPixel(format) <= FastColorClearBppLimit));
}

} // Gfx9
} // Formats
} // Pal
