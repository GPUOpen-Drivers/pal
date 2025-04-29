/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfxBlendOptimizer.h"
#include "core/hw/gfxip/gfx12/gfx12CmdStream.h"
#include "core/hw/gfxip/gfx12/gfx12ColorBlendState.h"
#include "palInlineFuncs.h"

using namespace Util;
using namespace Pal::Gfx12::Chip;

namespace Pal
{
namespace Gfx12
{

// =====================================================================================================================
// Returns true for dual-source blend factors.
static bool IsDualSrcBlendFactor(
    Blend blend)
{
    bool isDualSrcBlendOption = false;

    switch (blend)
    {
    case Blend::Src1Color:
    case Blend::OneMinusSrc1Color:
    case Blend::Src1Alpha:
    case Blend::OneMinusSrc1Alpha:
        isDualSrcBlendOption = true;
        break;
    default:
        break;
    }

    return isDualSrcBlendOption;
}

// =====================================================================================================================
// Returns true for a blend factor in the given field (color or alpha) that uses the dst image.
static bool UsesDst(
    Blend blend,
    bool  isAlphaField)
{
    bool usesDst = false;
    switch (blend)
    {
    case Blend::DstColor:           // means DstAlpha when in alpha field
    case Blend::OneMinusDstColor:   // means OneMinusDstAlpha when in alpha field
    case Blend::DstAlpha:
    case Blend::OneMinusDstAlpha:
        usesDst = true;
        break;
    case Blend::SrcAlphaSaturate:
        usesDst = (isAlphaField == false); // (f,f,f,1); f = min(1 - dst.a, src.a)
        break;
    default:
        break;
    }
    return usesDst;
}

// =====================================================================================================================
// Converts a Pal::Blend value to a Gfx12 hardware BlendOp
BlendOp ColorBlendState::HwBlendOp(
    Blend blendOp)   // Pal::Blend enum value to convert
{
    constexpr BlendOp BlendOpTbl[] =
    {
        BLEND_ZERO,                            // Zero
        BLEND_ONE,                             // One
        BLEND_SRC_COLOR,                       // SrcColor
        BLEND_ONE_MINUS_SRC_COLOR,             // OneMinusSrcColor
        BLEND_DST_COLOR,                       // DstColor
        BLEND_ONE_MINUS_DST_COLOR,             // OneMinusDstColor
        BLEND_SRC_ALPHA,                       // SrcAlpha
        BLEND_ONE_MINUS_SRC_ALPHA,             // OneMinusSrcAlpha
        BLEND_DST_ALPHA,                       // DstAlpha
        BLEND_ONE_MINUS_DST_ALPHA,             // OneMinusDstAlpha
        BLEND_CONSTANT_COLOR,                  // ConstantColor
        BLEND_ONE_MINUS_CONSTANT_COLOR,        // OneMinusConstantColor
        BLEND_CONSTANT_ALPHA,                  // ConstantAlpha
        BLEND_ONE_MINUS_CONSTANT_ALPHA,        // OneMinusConstantAlpha
        BLEND_SRC_ALPHA_SATURATE,              // SrcAlphaSaturate
        BLEND_SRC1_COLOR,                      // Src1Color
        BLEND_INV_SRC1_COLOR,                  // OneMinusSrc1Color
        BLEND_SRC1_ALPHA,                      // Src1Alpha
        BLEND_INV_SRC1_ALPHA,                  // OneMinusSrc1Alpha
    };

    BlendOp hwOp = BlendOpTbl[static_cast<size_t>(blendOp)];

    return hwOp;
}

// =====================================================================================================================
// Converts a Pal::BlendFunc value to a Gfx12 hardware CombFunc enum.
CombFunc ColorBlendState::HwBlendFunc(
    BlendFunc blendFunc)    // Pal::BlendFunc value to convert
{
    constexpr CombFunc BlendFuncTbl[] =
    {
        COMB_DST_PLUS_SRC,    // Add              = 0,
        COMB_SRC_MINUS_DST,   // Subtract         = 1,
        COMB_DST_MINUS_SRC,   // ReverseSubtract  = 2,
        COMB_MIN_DST_SRC,     // Min              = 3,
        COMB_MAX_DST_SRC,     // Max              = 4,
        COMB_MIN_DST_SRC,     // ScaledMin        = 5, use the same hw value with Min.
        COMB_MAX_DST_SRC,     // ScaledMax        = 6, use the same hw value with Max.
    };

    return BlendFuncTbl[static_cast<size_t>(blendFunc)];
}

// =====================================================================================================================
// Get the sx-blend-opt with respect to the blend opt
// This feature is identical to the gfx8.1 implementation.
SX_BLEND_OPT SxBlendOptColor(
    Blend blendOpt)    // Pal::Blend opt
{
    SX_BLEND_OPT sxBlendOpt = BLEND_OPT_PRESERVE_NONE_IGNORE_NONE;

    switch (blendOpt)
    {
    case Blend::Zero:
        sxBlendOpt = BLEND_OPT_PRESERVE_NONE_IGNORE_ALL;
        break;
    case Blend::One:
        sxBlendOpt = BLEND_OPT_PRESERVE_ALL_IGNORE_NONE;
        break;
    case Blend::SrcColor:
        sxBlendOpt = BLEND_OPT_PRESERVE_C1_IGNORE_C0;
        break;
    case Blend::OneMinusSrcColor:
        sxBlendOpt = BLEND_OPT_PRESERVE_C0_IGNORE_C1;
        break;
    case Blend::SrcAlpha:
        sxBlendOpt = BLEND_OPT_PRESERVE_A1_IGNORE_A0;
        break;
    case Blend::OneMinusSrcAlpha:
        sxBlendOpt = BLEND_OPT_PRESERVE_A0_IGNORE_A1;
        break;
    case Blend::SrcAlphaSaturate:
        sxBlendOpt = BLEND_OPT_PRESERVE_NONE_IGNORE_A0;
        break;
    default:
        break;
    }

    return sxBlendOpt;
}

// =====================================================================================================================
// Get the sx-blend-opt with respect to the blend opt
// This method is for RbPlus feature which is identical to the gfx8.1 implementation.
SX_BLEND_OPT SxBlendOptAlpha(
    Blend blendOpt)    // Pal::Blend opt
{
    SX_BLEND_OPT sxBlendOpt = BLEND_OPT_PRESERVE_NONE_IGNORE_NONE;

    switch (blendOpt)
    {
    case Blend::Zero:
        sxBlendOpt = BLEND_OPT_PRESERVE_NONE_IGNORE_ALL;
        break;
    case Blend::One:
        sxBlendOpt = BLEND_OPT_PRESERVE_ALL_IGNORE_NONE;
        break;
    case Blend::SrcColor:
        sxBlendOpt = BLEND_OPT_PRESERVE_A1_IGNORE_A0;
        break;
    case Blend::OneMinusSrcColor:
        sxBlendOpt = BLEND_OPT_PRESERVE_A0_IGNORE_A1;
        break;
    case Blend::SrcAlpha:
        sxBlendOpt = BLEND_OPT_PRESERVE_A1_IGNORE_A0;
        break;
    case Blend::OneMinusSrcAlpha:
        sxBlendOpt = BLEND_OPT_PRESERVE_A0_IGNORE_A1;
        break;
    case Blend::SrcAlphaSaturate:
        sxBlendOpt = BLEND_OPT_PRESERVE_ALL_IGNORE_NONE;
        break;
    default:
        break;
    }

    return sxBlendOpt;
}

// =====================================================================================================================
// Get the sx-blend-fcn with respect to the Pal blend function.
// This feature is identical to the gfx8.1 implementation.
SX_OPT_COMB_FCN SxBlendFcn(
    BlendFunc blendFcn)    // Pal::Blend function
{
    constexpr SX_OPT_COMB_FCN SxBlendFcnTbl[] =
    {
        OPT_COMB_ADD,         // Add
        OPT_COMB_SUBTRACT,    // Subtract
        OPT_COMB_REVSUBTRACT, // ReverseSubtract
        OPT_COMB_MIN,         // Min
        OPT_COMB_MAX,         // Max
    };

    return SxBlendFcnTbl[static_cast<int>(blendFcn)];
}

// =====================================================================================================================
void ColorBlendState::InitSxBlendOpts(
    const ColorBlendStateCreateInfo& createInfo)
{
    for (uint32 mrtIdx = 0; mrtIdx < MaxColorTargets; mrtIdx++)
    {
        const auto& mrtInfo     = createInfo.targets[mrtIdx];
        auto*       pSxBlendOpt = &(m_regs.sxMrtBlendOpt[mrtIdx]);

        if (mrtInfo.blendEnable)
        {
            pSxBlendOpt->bits.COLOR_SRC_OPT = SxBlendOptColor(mrtInfo.srcBlendColor);

            // If src color factor contains dst, don't optimize color DST.
            if (UsesDst(mrtInfo.srcBlendColor, false))
            {
                pSxBlendOpt->bits.COLOR_DST_OPT = BLEND_OPT_PRESERVE_NONE_IGNORE_NONE;
            }
            else
            {
                pSxBlendOpt->bits.COLOR_DST_OPT = SxBlendOptColor(mrtInfo.dstBlendColor);
            }

            pSxBlendOpt->bits.ALPHA_SRC_OPT = SxBlendOptAlpha(mrtInfo.srcBlendAlpha);

            // If src alpha factor contains DST, don't optimize alpha DST.
            if (UsesDst(mrtInfo.srcBlendAlpha, true))
            {
                pSxBlendOpt->bits.ALPHA_DST_OPT = BLEND_OPT_PRESERVE_NONE_IGNORE_NONE;
            }
            else
            {
                pSxBlendOpt->bits.ALPHA_DST_OPT = SxBlendOptAlpha(mrtInfo.dstBlendAlpha);
            }

            pSxBlendOpt->bits.COLOR_COMB_FCN = SxBlendFcn(mrtInfo.blendFuncColor);
            pSxBlendOpt->bits.ALPHA_COMB_FCN = SxBlendFcn(mrtInfo.blendFuncAlpha);

            // BlendOpts are forced to ONE for MIN/MAX blend fcns
            if ((pSxBlendOpt->bits.COLOR_COMB_FCN == OPT_COMB_MIN) ||
                (pSxBlendOpt->bits.COLOR_COMB_FCN == OPT_COMB_MAX))
            {
                pSxBlendOpt->bits.COLOR_SRC_OPT = BLEND_OPT_PRESERVE_ALL_IGNORE_NONE;
                pSxBlendOpt->bits.COLOR_DST_OPT = BLEND_OPT_PRESERVE_ALL_IGNORE_NONE;
            }
            if ((pSxBlendOpt->bits.ALPHA_COMB_FCN == OPT_COMB_MIN) ||
                (pSxBlendOpt->bits.ALPHA_COMB_FCN == OPT_COMB_MAX))
            {
                pSxBlendOpt->bits.ALPHA_SRC_OPT = BLEND_OPT_PRESERVE_ALL_IGNORE_NONE;
                pSxBlendOpt->bits.ALPHA_DST_OPT = BLEND_OPT_PRESERVE_ALL_IGNORE_NONE;
            }
        }
        else
        {
            pSxBlendOpt->bits.COLOR_COMB_FCN = OPT_COMB_BLEND_DISABLED;
            pSxBlendOpt->bits.ALPHA_COMB_FCN = OPT_COMB_BLEND_DISABLED;
        }
    }
}

// =====================================================================================================================
ColorBlendState::ColorBlendState(
    const Device&                    device,
    const ColorBlendStateCreateInfo& createInfo)
    :
    Pal::ColorBlendState(createInfo),
    m_regs{}
{
    m_flags.u32All = 0;

    for (uint32 mrtIdx = 0; mrtIdx < MaxColorTargets; mrtIdx++)
    {
        const auto& mrtInfo         = createInfo.targets[mrtIdx];
        auto*       pCbBlendControl = &(m_regs.cbBlendControl[mrtIdx]);

        pCbBlendControl->bits.ENABLE               = mrtInfo.blendEnable;
        pCbBlendControl->bits.SEPARATE_ALPHA_BLEND = 1;
        pCbBlendControl->bits.COLOR_SRCBLEND       = HwBlendOp(mrtInfo.srcBlendColor);
        pCbBlendControl->bits.COLOR_DESTBLEND      = HwBlendOp(mrtInfo.dstBlendColor);
        pCbBlendControl->bits.ALPHA_SRCBLEND       = HwBlendOp(mrtInfo.srcBlendAlpha);
        pCbBlendControl->bits.ALPHA_DESTBLEND      = HwBlendOp(mrtInfo.dstBlendAlpha);
        pCbBlendControl->bits.COLOR_COMB_FCN       = HwBlendFunc(mrtInfo.blendFuncColor);
        pCbBlendControl->bits.ALPHA_COMB_FCN       = HwBlendFunc(mrtInfo.blendFuncAlpha);
        pCbBlendControl->bits.DISABLE_ROP3         = mrtInfo.disableLogicOp;

        // BlendOps are forced to ONE for MIN/MAX blend funcs
        if ((mrtInfo.blendFuncColor == Pal::BlendFunc::Min) || (mrtInfo.blendFuncColor == Pal::BlendFunc::Max))
        {
            pCbBlendControl->bits.COLOR_SRCBLEND  = BLEND_ONE;
            pCbBlendControl->bits.COLOR_DESTBLEND = BLEND_ONE;
        }

        if ((mrtInfo.blendFuncAlpha == Pal::BlendFunc::Min) || (mrtInfo.blendFuncAlpha == Pal::BlendFunc::Max))
        {
            pCbBlendControl->bits.ALPHA_SRCBLEND  = BLEND_ONE;
            pCbBlendControl->bits.ALPHA_DESTBLEND = BLEND_ONE;
        }
    }

    const auto mrt0Info = createInfo.targets[0];

    const bool isDualSource = IsDualSrcBlendFactor(mrt0Info.srcBlendColor) ||
                              IsDualSrcBlendFactor(mrt0Info.dstBlendColor) ||
                              IsDualSrcBlendFactor(mrt0Info.srcBlendAlpha) ||
                              IsDualSrcBlendFactor(mrt0Info.dstBlendAlpha);

    // MRT1 blending must be enabled for dual source blending.
    m_regs.cbBlendControl[1].bits.ENABLE |= isDualSource;

    // Per discussions with HW engineers, RTL has issues with blend optimization for dual source blending.  HW is
    // already turning it off for that case.  Thus, driver must not turn it on as well for dual source blending.
    if (isDualSource == false)
    {
        InitSxBlendOpts(createInfo);
    }

    InitBlendMasks(createInfo);
}

// =====================================================================================================================
// Examines the blend state for each target to determine if the state allows the destination to be read
// and sets the appropriate bit in m_flags.blendReadsDstPerformanceHeuristic.
void ColorBlendState::InitBlendMasks(
    const ColorBlendStateCreateInfo& createInfo)
{
    for (uint32 rtIdx = 0; rtIdx < MaxColorTargets; rtIdx++)
    {
        if (createInfo.targets[rtIdx].blendEnable)
        {
            const Blend srcBlends[] = {
                createInfo.targets[rtIdx].srcBlendColor,
                createInfo.targets[rtIdx].srcBlendAlpha
            };
            const Blend dstBlends[] = {
                createInfo.targets[rtIdx].dstBlendColor,
                createInfo.targets[rtIdx].dstBlendAlpha
            };
            const BlendFunc funcs[] = {
                createInfo.targets[rtIdx].blendFuncColor,
                createInfo.targets[rtIdx].blendFuncAlpha,
            };
            for (uint32 k = 0; k < 2; k++)
            {
                const Blend srcBlend = srcBlends[k];
                const Blend dstBlend = dstBlends[k];
                const BlendFunc func = funcs[k];
                // Min and Max ignore blend factors.
                if ((func == BlendFunc::Min) ||
                    (func == BlendFunc::Max) ||
                    (dstBlend != Blend::Zero) ||
                    UsesDst(srcBlend, k != 0))
                {
                    // Should differ from (1*dst + 0*src) so we don't interfere with client experiments.
                    if ((dstBlend != Blend::One) ||
                        (srcBlend != Blend::Zero) ||
                        (func != BlendFunc::Add))
                    {
                        m_flags.blendReadsDstPerformanceHeuristic |= (1 << rtIdx);
                    }
                }
            }
        }
    }
}

// =====================================================================================================================
uint32* ColorBlendState::WriteCommands(
    uint32* pCmdSpace
    ) const
{
    static_assert(Util::CheckSequential({ mmSX_MRT0_BLEND_OPT,
                                          mmSX_MRT1_BLEND_OPT,
                                          mmSX_MRT2_BLEND_OPT,
                                          mmSX_MRT3_BLEND_OPT,
                                          mmSX_MRT4_BLEND_OPT,
                                          mmSX_MRT5_BLEND_OPT,
                                          mmSX_MRT6_BLEND_OPT,
                                          mmSX_MRT7_BLEND_OPT,
                                          mmCB_BLEND0_CONTROL,
                                          mmCB_BLEND1_CONTROL,
                                          mmCB_BLEND2_CONTROL,
                                          mmCB_BLEND3_CONTROL,
                                          mmCB_BLEND4_CONTROL,
                                          mmCB_BLEND5_CONTROL,
                                          mmCB_BLEND6_CONTROL,
                                          mmCB_BLEND7_CONTROL, }),
                  "mmSX_MRT#_BLEND_OPT/mmCB_BLEND#_CONTROL registers are not sequential!");
    static_assert(Util::CheckSequential({ offsetof(ColorBlendStateRegs, sxMrtBlendOpt),
                                          offsetof(ColorBlendStateRegs, cbBlendControl), },
                                        sizeof(uint32) * MaxColorTargets),
                  "Storage order of ColorBlendStateRegs is important!");
    // mmSX_MRT0_BLEND_OPT is not written here which is pending to draw validation time.
    return CmdStream::WriteSetSeqContextRegs(mmSX_MRT1_BLEND_OPT,
                                             mmCB_BLEND7_CONTROL,
                                             &m_regs.sxMrtBlendOpt[1],
                                             pCmdSpace);
}

} // namespace Gfx12
} // namespace Pal
