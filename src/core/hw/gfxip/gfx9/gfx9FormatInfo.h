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

#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "palFormatInfo.h"

namespace Pal
{
namespace Formats
{
namespace Gfx9
{

// The maximum number of channel/swizzle pairs that can correspond to a HW image format and a HW buffer format.
constexpr uint32 MaxImgDataMappings = 4;
constexpr uint32 MaxBufDataMappings = 4;

constexpr uint32 MaxImgNumFormats = 14;
constexpr uint32 MaxBufNumFormats = 8;

// Stores each channel format and its corresponding HW enums.  Format for ChannelFmtInfoTbl .
struct MergedFmtInfo
{
    ChNumFormat     format;        // PAL channel format enum.
    ColorFormat     hwColorFmt;    // Corresponding HW color block (CB) format enum.
    SurfaceNumber   hwColorNumFmt; // Corresponding HW color block (CB) numeric format enum.
    IMG_DATA_FORMAT hwImgDataFmt;  // Corresponding HW texture block image descriptor format enum.
    uint32          hwImgNumFmt;   // Corresponding HW texture block image descriptor numeric format enum.
    BUF_DATA_FORMAT hwBufDataFmt;  // Corresponding HW texture block buffer descriptor format enum.
    BUF_NUM_FORMAT  hwBufNumFmt;   // Corresponding HW texture block buffer descriptor numeric format enum.
    ZFormat         hwZFmt;        // Corresponding HW depth block (DB) Z format enum.
    StencilFormat   hwStencilFmt;  // Corresponding HW depth block (DB) stencil format enum.
};

// Stores each HW texture block image descriptor format and its corresponding formats.
struct MergedImgDataFmtInfo
{
    IMG_DATA_FORMAT imgDataFmt; // HW texture block image descriptor format enum.
    // All valid channel formats for imgDataFmt (or Undefined).
    ChNumFormat     mappings[MaxImgNumFormats][MaxImgDataMappings];
};

// Stores each HW texture block buffer descriptor format and its corresponding formats.
struct MergedBufDataFmtInfo
{
    BUF_DATA_FORMAT bufDataFmt; // HW texture block buffer descriptor format enum.
    // All valid channel formats for imgDataFmt (or Undefined).
    ChNumFormat     mappings[MaxBufNumFormats][MaxImgDataMappings];
};

extern const MergedFmtInfo*      MergedChannelFmtInfoTbl(GfxIpLevel gfxIpLevel);

extern SQ_SEL_XYZW01 HwSwizzle(ChannelSwizzle swizzle);
extern ChannelSwizzle ChannelSwizzleFromHwSwizzle(SQ_SEL_XYZW01 hwSwizzle);

extern IMG_DATA_FORMAT HwImgDataFmt(const MergedFmtInfo fmtInfo[], ChNumFormat format);
extern IMG_NUM_FORMAT  HwImgNumFmt(const MergedFmtInfo fmtInfo[], ChNumFormat format);

extern ChNumFormat FmtFromHwImgFmt(IMG_DATA_FORMAT imgDataFmt, IMG_NUM_FORMAT imgNumFmt, GfxIpLevel gfxIpLevel);

extern BUF_DATA_FORMAT HwBufDataFmt(const MergedFmtInfo fmtInfo[], ChNumFormat format);
extern BUF_NUM_FORMAT  HwBufNumFmt(const MergedFmtInfo fmtInfo[], ChNumFormat format);

extern ChNumFormat FmtFromHwBufFmt(BUF_DATA_FORMAT bufDataFmt, BUF_NUM_FORMAT bufNumFmt, GfxIpLevel gfxIpLevel);

extern ColorFormat HwColorFmt(const MergedFmtInfo fmtInfo[], ChNumFormat format);

extern SurfaceNumber ColorSurfNum(const MergedFmtInfo fmtInfo[], ChNumFormat format);

extern SurfaceSwap ColorCompSwap(SwizzledFormat swizzledFormat);

extern ZFormat HwZFmt(const MergedFmtInfo fmtInfo[], ChNumFormat format);

extern StencilFormat HwStencilFmt(const MergedFmtInfo fmtInfo[], ChNumFormat format);

constexpr uint32 FastColorClearBppLimit = 64;
extern bool SupportsFastColorClear(ChNumFormat format);

} // Gfx9
} // Formats
} // Pal
