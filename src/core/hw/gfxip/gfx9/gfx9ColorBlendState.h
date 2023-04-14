/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfxBlendOptimizer.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"

namespace Pal
{
namespace Gfx9
{

class CmdStream;
class Device;

// =====================================================================================================================
// GFX9-specific color blend  state implementation.  See IColorBlendState documentation for more details.
class ColorBlendState final : public Pal::ColorBlendState
{
public:
    ColorBlendState(const Device& device, const ColorBlendStateCreateInfo& createInfo);

    uint32* WriteCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    bool IsBlendEnabled(uint32 slot) const { return ((m_flags.blendEnable & (1 << slot)) != 0); }

    uint32 BlendEnableMask() const { return m_flags.blendEnable; }
    uint32 BlendReadsDestMask() const { return m_flags.blendReadsDst; }

    uint8 WriteBlendOptimizations(
        CmdStream*                     pCmdStream,
        const SwizzledFormat*          pTargetFormats,
        const uint8*                   pTargetWriteMasks,
        uint32                         numRenderTargets,
        bool                           enableOpts,
        GfxBlendOptimizer::BlendOpts*  pBlendOpts,
        regCB_COLOR0_INFO*             pCbColorInfoRegs) const;

    bool IsBlendCommutative(uint32 slot) const
    {
        PAL_ASSERT(slot < MaxColorTargets);
        return (((m_flags.blendCommutative >> slot) & 0x1) != 0);
    }

private:
    virtual ~ColorBlendState() { }

    void Init(const ColorBlendStateCreateInfo& createInfo);
    void InitBlendOpts(const ColorBlendStateCreateInfo& createInfo);
    void InitBlendMasks(const ColorBlendStateCreateInfo& createInfo);

    BlendOp  HwBlendOp(Blend blendOp) const;
    static CombFunc HwBlendFunc(BlendFunc blendFunc);
    static bool     IsDualSrcBlendOption(Blend blend);

    union
    {
        struct
        {
            uint32  blendEnable      :  8; // Indicates if blending is enabled for each target
            uint32  blendCommutative :  8; // Indicates if blending is commutative for each target
            uint32  blendReadsDst    :  8; // Indicates if blending will read the destination
            uint32  dualSourceBlend  :  1; // Indicates if dual-source blending is enabled
            uint32  rbPlus           :  1; // Indicates if RBPlus is enabled
            uint32  reserved         :  6;
        };
        uint32  u32All;
    } m_flags;

    const Device& m_device;

    struct Regs
    {
        regSX_MRT0_BLEND_OPT sxMrtBlendOpt[MaxColorTargets];
        regCB_BLEND0_CONTROL cbBlendControl[MaxColorTargets];
    };

    Regs m_regs;

    GfxBlendOptimizer::BlendOpts  m_blendOpts[MaxColorTargets * GfxBlendOptimizer::NumChannelWriteComb];

    PAL_DISALLOW_COPY_AND_ASSIGN(ColorBlendState);
    PAL_DISALLOW_DEFAULT_CTOR(ColorBlendState);
};

} // Gfx9
} // Pal
