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

#include "core/hw/gfxip/gfx9/gfx9Chip.h"

// Forward decl
namespace Pal
{
struct PalPlatformSettings;
}

namespace Pal
{
namespace Formats
{
namespace Gfx9
{
namespace Chip = Pal::Gfx9::Chip;

// The maximum number of channel/swizzle pairs that can correspond to a HW image format and a HW buffer format.
constexpr uint32 MaxImgDataMappings = 4;
constexpr uint32 MaxBufDataMappings = 4;

constexpr uint32 MaxImgNumFormats = 14;
constexpr uint32 MaxBufNumFormats = 8;

// Stores each channel format and its corresponding HW enums for Gfxip's with flattened image and buffer formats.
struct MergedFlatFmtInfo
{
    ChNumFormat           format;         // PAL channel format enum.
    Chip::ColorFormat     hwColorFmt;     // Corresponding HW color block (CB) format enum.
    Chip::SurfaceNumber   hwColorNumFmt;  // Corresponding HW color block (CB) numeric format enum.
    Chip::IMG_FMT         hwImgFmt;       // Corresponding HW texture block image descriptor format enum.
    Chip::BUF_FMT         hwBufFmt;       // Corresponding HW texture block buffer descriptor format enum.
    Chip::ZFormat         hwZFmt;         // Corresponding HW depth block (DB) Z format enum.
    Chip::StencilFormat   hwStencilFmt;   // Corresponding HW depth block (DB) stencil format enum.
};

extern const MergedFlatFmtInfo*  MergedChannelFlatFmtInfoTbl(
    GfxIpLevel                      gfxIpLevel,
    const Pal::PalPlatformSettings* pSettings);

extern Chip::BUF_FMT             HwBufFmt(const MergedFlatFmtInfo fmtInfo[], ChNumFormat format);
extern Chip::IMG_FMT             HwImgFmt(const MergedFlatFmtInfo fmtInfo[], ChNumFormat format);
extern ChNumFormat               FmtFromHwBufFmt(Chip::BUF_FMT imgFmt, GfxIpLevel gfxIpLevel);
extern ChNumFormat               FmtFromHwImgFmt(Chip::IMG_FMT imgFmt, GfxIpLevel gfxIpLevel);
extern Chip::ColorFormat         HwColorFmt(const MergedFlatFmtInfo fmtInfo[], ChNumFormat format);
extern Chip::SurfaceNumber       ColorSurfNum(const MergedFlatFmtInfo fmtInfo[], ChNumFormat format);
extern Chip::StencilFormat       HwStencilFmt(const MergedFlatFmtInfo fmtInfo[], ChNumFormat format);
extern Chip::ZFormat             HwZFmt(const MergedFlatFmtInfo fmtInfo[], ChNumFormat format);

// Stores each HW texture block image descriptor format and its corresponding formats.
struct MergedImgDataFmtInfo
{
    Chip::IMG_DATA_FORMAT imgDataFmt; // HW texture block image descriptor format enum.
    // All valid channel formats for imgDataFmt (or Undefined).
    ChNumFormat           mappings[MaxImgNumFormats][MaxImgDataMappings];
};

// Stores each HW texture block buffer descriptor format and its corresponding formats.
struct MergedBufDataFmtInfo
{
    Chip::BUF_DATA_FORMAT bufDataFmt; // HW texture block buffer descriptor format enum.
    // All valid channel formats for imgDataFmt (or Undefined).
    ChNumFormat           mappings[MaxBufNumFormats][MaxImgDataMappings];
};

extern Chip::ColorFormat HwColorFormatForExport(GfxIpLevel gfxLevel, Pal::ChNumFormat format);
extern Chip::SQ_SEL_XYZW01 HwSwizzle(ChannelSwizzle swizzle);
extern Chip::SurfaceSwap ColorCompSwap(SwizzledFormat swizzledFormat);

extern ChannelSwizzle ChannelSwizzleFromHwSwizzle(Chip::SQ_SEL_XYZW01 hwSwizzle);

constexpr uint32 FastColorClearBppLimit = 64;
extern bool SupportsFastColorClear(ChNumFormat format);

} // Gfx9
} // Formats
} // Pal
