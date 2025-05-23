/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx12/gfx12FormatInfo.h"
#include "core/hw/gfxip/gfx12/g_gfx12DataFormats.h"
#include "palDevice.h"

using namespace Util;
using namespace Pal::Gfx12::Chip;

namespace Pal
{
namespace Formats
{
namespace Gfx12
{
static_assert(ArrayLen(Gfx12MergedFormatPropertiesTable.features) == static_cast<size_t>(ChNumFormat::Count),
              "Size of Gfx12MergedFormatPropertiesTable mismatches the number of declared ChNumFormat enums");
static_assert(ArrayLen(Gfx12MergedChannelFmtInfoTbl) == static_cast<size_t>(ChNumFormat::Count),
              "Size of Gfx12MergedChannelFmtInfoTbl mismatches the number of declared ChNumFormat enums");

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
// Returns the IMG_FMT enum corresponding to the specified PAL channel format.  This enum is used when programming the
// texture block.
IMG_FMT HwImgFmt(
    ChNumFormat format)
{
    // Assert if we're looking at the wrong table entry.
    PAL_ASSERT(Gfx12MergedChannelFmtInfoTbl[static_cast<uint32>(format)].format == format);
    return Gfx12MergedChannelFmtInfoTbl[static_cast<uint32>(format)].hwImgFmt;
}

// =====================================================================================================================
// Returns the PAL channel format corresponding to the specified IMG_FMT enum or Undefined if an error occurred.
ChNumFormat FmtFromHwImgFmt(
    IMG_FMT     imgFmt)
{
    const ChNumFormat format = Gfx12MergedImgDataFmtTbl[imgFmt];
    return format;
}

// =====================================================================================================================
// Returns the BUF_FMT enum corresponding to the specified PAL channel format.  This enum is used when programming the
// texture block.
BUF_FMT HwBufFmt(
    ChNumFormat format)
{
    // Assert if we're looking at the wrong table entry.
    PAL_ASSERT(Gfx12MergedChannelFmtInfoTbl[static_cast<uint32>(format)].format == format);
    return Gfx12MergedChannelFmtInfoTbl[static_cast<uint32>(format)].hwBufFmt;
}

// =====================================================================================================================
// Returns the PAL channel format corresponding to the specified BUF_FMT enum or Undefined if an error occurred.
ChNumFormat FmtFromHwBufFmt(
    BUF_FMT bufFmt)
{
    const ChNumFormat format = Gfx12MergedBufDataFmtTbl[bufFmt];
    return format;
}

// =====================================================================================================================
// Returns the format info table for the specific GfxIpLevel.
const MergedFlatFmtInfo* MergedChannelFlatFmtInfoTbl(
    GfxIpLevel                      gfxIpLevel,
    const Pal::PalPlatformSettings* pSettings)
{
    return Gfx12MergedChannelFmtInfoTbl;
}

// =====================================================================================================================
// Returns the ColorFormat enum corresponding to the specified PAL channel format.  This enum is used when programming
// the CB block.
ColorFormat HwColorFmt(
    ChNumFormat            format)
{
    // Assert if we're looking at the wrong table entry.
    PAL_ASSERT(Gfx12MergedChannelFmtInfoTbl[static_cast<uint32>(format)].format == format);
    return Gfx12MergedChannelFmtInfoTbl[static_cast<uint32>(format)].hwColorFmt;
}

// =====================================================================================================================
// Returns the SurfaceNumber enum corresponding to the specified PAL numeric format.  This enum is used when
// programming the CB block.
SurfaceNumber ColorSurfNum(
    ChNumFormat             format)
{
    // Assert if we're looking at the wrong table entry.
    PAL_ASSERT(Gfx12MergedChannelFmtInfoTbl[static_cast<uint32>(format)].format == format);
    return Gfx12MergedChannelFmtInfoTbl[static_cast<uint32>(format)].hwColorNumFmt;
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

} // Gfx12
} // Formats
} // Pal
