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

#pragma once

#include "core/hw/gfxip/colorBlendState.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfx12/gfx12RegPairHandler.h"

namespace Pal
{
namespace Gfx12
{

class Device;

// =====================================================================================================================
// GFX12 ColorBlendState object.  Translates PAL interface blending controls to Gfx12.  Hardware independent.
class ColorBlendState final : public Pal::ColorBlendState
{
public:
    ColorBlendState(const Device& device, const ColorBlendStateCreateInfo& createInfo);

    uint32* WriteCommands(uint32* pCmdSpace) const;

    // sxMrt0BlendOpt is pending to the draw validation time.
    SX_MRT0_BLEND_OPT SxMrt0BlendOpt() const { return m_regs.sxMrtBlendOpt[0]; }

    uint32 BlendReadsDstPerformanceHeuristicMrtMask() const { return m_flags.blendReadsDstPerformanceHeuristic; }

private:
    virtual ~ColorBlendState() { }

    static BlendOp  HwBlendOp(Blend blendOp);
    static CombFunc HwBlendFunc(BlendFunc blendFunc);
    void InitSxBlendOpts(const ColorBlendStateCreateInfo& createInfo);
    void InitBlendMasks(const ColorBlendStateCreateInfo& createInfo);

    struct ColorBlendStateRegs
    {
        SX_MRT0_BLEND_OPT sxMrtBlendOpt[MaxColorTargets];
        CB_BLEND0_CONTROL cbBlendControl[MaxColorTargets];
    };

    ColorBlendStateRegs m_regs;

    union
    {
        struct
        {
            uint32 blendReadsDstPerformanceHeuristic :  8; // blending reads dst for performance heuristic purposes
            uint32 reserved                          : 24;
        };
        uint32  u32All;
    } m_flags;

    PAL_DISALLOW_COPY_AND_ASSIGN(ColorBlendState);
    PAL_DISALLOW_DEFAULT_CTOR(ColorBlendState);
};

} // namespace Gfx12
} // namespace Pal
