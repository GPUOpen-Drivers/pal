/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palAssert.h"
#include "palDevice.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"

namespace Pal
{
namespace GfxBlendOptimizer
{

static constexpr uint32 AlphaEnabled          = 0x01; // Alpha channel is written to
static constexpr uint32 ColorEnabled          = 0x02; // Color channel is written to
// Alpha/Color channel is written to
static constexpr uint32 AlphaColorEnabledMask = (AlphaEnabled | ColorEnabled);
// Number of valid combinations of Alpha/Color channel writes
// note: Valid combinations are Alpha Only, Color Only, or Both. Have neither is invalid.
static constexpr uint32 NumChannelWriteComb   = 3;

// Contains the color/alpha masks for blend optimizations.
enum ColorWriteEnable : uint32
{
    Red   = 1, // Red channel is written to
    Green = 2, // Green channel is written to
    Blue  = 4, // Blue channel is written to
    Alpha = 8, // Alpha channel is written to
};

enum class BlendOp : uint32
{
    // This enum doesn`t have to match the gfx level BlendOp enum. It can be any order/ value.
    BlendZero                  = 0x1000,
    BlendOne                   = 0x1001,
    BlendSrcColor              = 0x1002,
    BlendOneMinusSrcColor      = 0x1003,
    BlendSrcAlpha              = 0x1004,
    BlendOneMinusSrcAlpha      = 0x1005,
    BlendDstAlpha              = 0x1006,
    BlendOneMinusDstAlpha      = 0x1007,
    BlendDstColor              = 0x1008,
    BlendOneMinusDstColor      = 0x1009,
    BlendSrcAlphaSaturate      = 0x100A,
    BlendBothSrcAlpha          = 0x100B,
    BlendBothInvSrcAlpha       = 0x100C,
    BlendConstantColor         = 0x100D,
    BlendOneMinusConstantColor = 0x100E,
    BlendSrc1Color             = 0x100F,
    BlendInvSrc1Color          = 0x1010,
    BlendSrc1Alpha             = 0x1011,
    BlendInvSrc1Alpha          = 0x1012,
    BlendConstantAlpha         = 0x1013,
    BlendOneMinusConstantAlpha = 0x1014,
};

enum class BlendOpt : uint32
{
    ForceOptAuto               = 0x00,
    ForceOptDisable            = 0x01,
    ForceOptEnableIfSrcA0      = 0x02,
    ForceOptEnableIfSrcRgb0    = 0x03,
    ForceOptEnableIfSrcArgb0   = 0x04,
    ForceOptEnableIfSrcA1      = 0x05,
    ForceOptEnableIfSrcRgb1    = 0x06,
    ForceOptEnableIfSrcArgb1   = 0x07,
};

// Contains the blend optimization setting for a single MRT
struct BlendOpts
{
    // Per-MRT blend optimization controls for destination read.
    BlendOpt dontRdDst;
    // Per-MRT blend optimization controls for pixel discard.
    BlendOpt discardPixel;
};

// Contains state information for deriving applicable blend optimizations.
struct Input
{
    BlendOp srcBlend;       // Source color blend factor
    BlendOp destBlend;      // Destination color blend factor
    BlendOp alphaSrcBlend;  // Source alpha blend factor
    BlendOp alphaDestBlend; // Destination alpha blend factor
    bool    colorWrite;     // Color channel write flag
    bool    alphaWrite;     // Alpha channel write flag
};

// Various optimization equations for pixel discard (in the order of priority)
extern BlendOpt OptimizePixDiscard1(const Input& state);
extern BlendOpt OptimizePixDiscard2(const Input& state);

} // GfxBlendOptimizer
} // Pal
