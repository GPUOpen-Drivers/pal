/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palInlineFuncs.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx10ShadowedRegisters.h"
#include "core/hw/gfxip/gfx9/gfx11ShadowedRegisters.h"

namespace Pal
{
namespace Gfx9
{
constexpr uint32 Gfx10NumShShadowRanges    = Util::ArrayLen32(Gfx10ShShadowRange);
constexpr uint32 Gfx10NumCsShShadowRanges  = Util::ArrayLen32(Gfx10CsShShadowRange);

constexpr uint32 Gfx10NumUserConfigShadowRanges = Util::Max(Nv10NumUserConfigShadowRanges,
                                                            Gfx103NumUserConfigShadowRanges);
constexpr uint32 Gfx10NumContextShadowRanges    = Util::Max(Nv10NumContextShadowRanges,
                                                            Gfx103NumContextShadowRanges);

constexpr uint32 MaxNumUserConfigRanges  = Gfx10NumUserConfigShadowRanges;
constexpr uint32 MaxNumContextRanges     = Gfx10NumContextShadowRanges;
constexpr uint32 MaxNumShRanges          = Gfx10NumShShadowRanges;
constexpr uint32 MaxNumCsShRanges        = Gfx10NumCsShShadowRanges;
} // Gfx9
} // Pal
