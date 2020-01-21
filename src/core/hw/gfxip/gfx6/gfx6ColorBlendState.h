/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx6/gfx6Chip.h"

namespace Pal
{
namespace Gfx6
{

class CmdStream;
class Device;

// =====================================================================================================================
// GFX6-specific color blend  state implementation.  See IColorBlendState documentation for more details.
class ColorBlendState : public Pal::ColorBlendState
{
public:
    ColorBlendState(const Device& device, const ColorBlendStateCreateInfo& createInfo);

    uint32* WriteCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    template <bool Pm4OptImmediate>
    uint32* WriteBlendOptimizations(
        CmdStream*                     pCmdStream,
        const SwizzledFormat*          pTargetFormats,
        const uint8*                   pTargetWriteMasks,
        bool                           enableOpts,
        GfxBlendOptimizer::BlendOpts*  pBlendOpts,
        uint32*                        pCmdSpace) const;

    bool IsBlendEnabled(uint32 slot) const { return ((m_flags.blendEnable & (1 << slot)) != 0); }
    uint32 BlendEnableMask() const { return m_flags.blendEnable; }

    bool IsBlendCommutative(uint32 slot) const
    {
        PAL_ASSERT(slot < MaxColorTargets);
        return (((m_flags.blendCommutative >> slot) & 0x1) != 0);
    }

private:
    virtual ~ColorBlendState() { }

    void Init(const ColorBlendStateCreateInfo& createInfo);
    void InitBlendOpts(const ColorBlendStateCreateInfo& createInfo);
    void InitBlendCommutativeMask(const ColorBlendStateCreateInfo& createInfo);

    static BlendOp  HwBlendOp(Blend blendOp);
    static CombFunc HwBlendFunc(BlendFunc blendFunc);
    static bool     IsDualSrcBlendOption(Blend blend);

    union
    {
        struct
        {
            uint32  blendEnable      :  8; // Indicates if blending is enabled for each target
            uint32  blendCommutative :  8; // Indicates if blending is commutative for each target
            uint32  dualSourceBlend  :  1; // Indicates if dual-source blending is enabled
            uint32  rbPlus           :  1; // Indicates if RBPlus is enabled
            uint32  reserved         : 14;
        };
        uint32  u32All;
    } m_flags;

    regCB_BLEND0_CONTROL      m_cbBlendControl[MaxColorTargets];
    regSX_MRT0_BLEND_OPT__VI  m_sxMrtBlendOpt[MaxColorTargets];

    GfxBlendOptimizer::BlendOpts  m_blendOpts[MaxColorTargets * GfxBlendOptimizer::NumChannelWriteComb];

    PAL_DISALLOW_COPY_AND_ASSIGN(ColorBlendState);
    PAL_DISALLOW_DEFAULT_CTOR(ColorBlendState);
};

} // Gfx6
} // Pal
