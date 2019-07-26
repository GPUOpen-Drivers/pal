/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx6/gfx6Device.h"

namespace Pal
{
namespace Gfx6
{

class CmdStream;
class Device;

// =====================================================================================================================
// Represents an "image" of the PM4 commands necessary to write a BlendStatePm4Img to hardware.
// The required register writes are grouped into sets based on sequential register addresses, so that we can minimize
// the amount of PM4 space needed by setting several reg's in each packet.
struct BlendStatePm4Img
{
    PM4CMDSETDATA               hdrCbBlendControl;

    // Per-MRT blend control for MRTs 0..7
    regCB_BLEND0_CONTROL        cbBlendControl[MaxColorTargets];

    // Note: This packet is only used on GFX8+ hardware with the RB+ feature, which should be last in the PM4 image to
    // eliminate any "gaps" on non-RB+ hardware.
    PM4CMDSETDATA               hdrSxMrtBlendOpt;
    regSX_MRT0_BLEND_OPT__VI    sxMrtBlendOpt[MaxColorTargets];

    // Command space needed, in DWORDs. This field must always be last in the structure to not interfere w/ the actual
    // commands contained within.
    size_t                      spaceNeeded;
};

// =====================================================================================================================
// GFX6-specific color blend  state implementation.  See IColorBlendState documentation for more details.
class ColorBlendState : public Pal::ColorBlendState
{
public:
    explicit ColorBlendState(const Device& device, const ColorBlendStateCreateInfo& createInfo);
    void Init(const Device& device, const ColorBlendStateCreateInfo& createInfo);
    static Result ValidateCreateInfo(const Device* pDevice, const ColorBlendStateCreateInfo& createInfo);

    static size_t Pm4ImgSize() { return sizeof(BlendStatePm4Img); }
    uint32* WriteCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    template <bool Pm4OptImmediate>
    uint32* WriteBlendOptimizations(
        CmdStream*                     pCmdStream,
        const SwizzledFormat*          pTargetFormats,
        const uint8*                   pTargetWriteMasks,
        bool                           enableOpts,
        GfxBlendOptimizer::BlendOpts*  pBlendOpts,
        uint32*                        pCmdSpace) const;

    bool IsBlendEnabled(uint32 slot) const { return ((m_blendEnableMask & (1 << slot)) != 0); }
    uint32 BlendEnableMask() const { return m_blendEnableMask; }

    bool IsBlendCommutative(uint32 slot) const
    {
        PAL_ASSERT(slot < MaxColorTargets);
        return (((m_blendCommutativeMask >> slot) & 0x1) != 0);
    }

    // NOTE: Part of the IDestroyable public interface.
    virtual void Destroy() override { this->~ColorBlendState(); }

protected:
    virtual ~ColorBlendState() {} // Destructor has nothing to do.

private:
    void BuildPm4Headers(const Device& device);

    void InitBlendOpts(const ColorBlendStateCreateInfo& blend, bool isDualSrcBlend);
    void InitBlendCommutativeMask(const ColorBlendStateCreateInfo& createInfo);

    static BlendOp  HwBlendOp(Blend blendOp);
    static CombFunc HwBlendFunc(BlendFunc blendFunc);
    static bool     IsDualSrcBlendOption(Blend blend);

    // Image of PM4 commands needed to write this object to hardware
    BlendStatePm4Img             m_pm4Commands;
    // Per MRT blend opts
    GfxBlendOptimizer::BlendOpts m_blendOpts[MaxColorTargets * GfxBlendOptimizer::NumChannelWriteComb];

    uint32  m_blendEnableMask;       // Indicates if blending is enabled for each target
    uint32  m_blendCommutativeMask;  // Indicates if the blend state is commutative for each target

    PAL_DISALLOW_COPY_AND_ASSIGN(ColorBlendState);
    PAL_DISALLOW_DEFAULT_CTOR(ColorBlendState);
};

} // Gfx6
} // Pal
