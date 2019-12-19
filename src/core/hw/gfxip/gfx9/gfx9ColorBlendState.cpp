/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx9/g_gfx9PalSettings.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9ColorBlendState.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "palFormatInfo.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
ColorBlendState::ColorBlendState(
    const Device&                    device,
    const ColorBlendStateCreateInfo& createInfo)
    :
    Pal::ColorBlendState()
{
    m_flags.u32All = 0;
    m_flags.rbPlus = device.Settings().gfx9RbPlusEnable;

    memset(&m_blendOpts[0],      0, sizeof(m_blendOpts));
    memset(&m_cbBlendControl[0], 0, sizeof(m_cbBlendControl));
    memset(&m_sxMrtBlendOpt[0],  0, sizeof(m_sxMrtBlendOpt));

    Init(createInfo);
}

// =====================================================================================================================
// Converts a Pal::Blend value to a Gfx9 hardware BlendOp
BlendOp ColorBlendState::HwBlendOp(
    Blend blendOp)    // Pal::Blend enum value to convert
{
    constexpr BlendOp BlendOpTbl[] =
    {
        BLEND_ZERO,                     // Zero
        BLEND_ONE,                      // One
        BLEND_SRC_COLOR,                // SrcColor
        BLEND_ONE_MINUS_SRC_COLOR,      // OneMinusSrcColor
        BLEND_DST_COLOR,                // DstColor
        BLEND_ONE_MINUS_DST_COLOR,      // OneMinusDstColor
        BLEND_SRC_ALPHA,                // SrcAlpha
        BLEND_ONE_MINUS_SRC_ALPHA,      // OneMinusSrcAlpha
        BLEND_DST_ALPHA,                // DstAlpha
        BLEND_ONE_MINUS_DST_ALPHA,      // OneMinusDstAlpha
        BLEND_CONSTANT_COLOR,           // ConstantColor
        BLEND_ONE_MINUS_CONSTANT_COLOR, // OneMinusConstantColor
        BLEND_CONSTANT_ALPHA,           // ConstantAlpha
        BLEND_ONE_MINUS_CONSTANT_ALPHA, // OneMinusConstantAlpha
        BLEND_SRC_ALPHA_SATURATE,       // SrcAlphaSaturate
        BLEND_SRC1_COLOR,               // Src1Color
        BLEND_INV_SRC1_COLOR,           // OneMinusSrc1Color
        BLEND_SRC1_ALPHA,               // Src1Alpha
        BLEND_INV_SRC1_ALPHA,           // OneMinusSrc1Alpha
    };

    return BlendOpTbl[static_cast<size_t>(blendOp)];
}

// =====================================================================================================================
// Converts a Pal::BlendFunc value to a Gfx9 hardware CombFunc enum.
CombFunc ColorBlendState::HwBlendFunc(
    BlendFunc blendFunc)    // Pal::BlendFunc value to convert
{
    constexpr CombFunc BlendFuncTbl[] =
    {
        COMB_DST_PLUS_SRC,  // Add              = 0,
        COMB_SRC_MINUS_DST, // Subtract         = 1,
        COMB_DST_MINUS_SRC, // ReverseSubtract  = 2,
        COMB_MIN_DST_SRC,   // Min              = 3,
        COMB_MAX_DST_SRC,   // Max              = 4,
    };

    return BlendFuncTbl[static_cast<size_t>(blendFunc)];
}

// =====================================================================================================================
// Detects dual-source blend modes.
bool ColorBlendState::IsDualSrcBlendOption(
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
// Get the sx-blend-opt with respect to the blend opt
// This feature is identical to the gfx8.1 implementation.
SX_BLEND_OPT GetSxBlendOptColor(
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
    default:
        break;
    }

    return sxBlendOpt;
}

// =====================================================================================================================
// Get the sx-blend-opt with respect to the blend opt
// This method is for RbPlus feature which is identical to the gfx8.1 implementation.
SX_BLEND_OPT GetSxBlendOptAlpha(
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
    default:
        break;
    }

    return sxBlendOpt;
}

// =====================================================================================================================
// Get the sx-blend-fcn with respect to the Pal blend function.
// This feature is identical to the gfx8.1 implementation.
SX_OPT_COMB_FCN GetSxBlendFcn(
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
static GfxBlendOptimizer::BlendOp HwEnumToBlendOp(
    uint32 hwEnum)
{
    using namespace GfxBlendOptimizer;

    constexpr BlendOp ConversionTable[] =
    {
        BlendOp::BlendZero,
        BlendOp::BlendOne,
        BlendOp::BlendSrcColor,
        BlendOp::BlendOneMinusSrcColor,
        BlendOp::BlendSrcAlpha,
        BlendOp::BlendOneMinusSrcAlpha,
        BlendOp::BlendDstAlpha,
        BlendOp::BlendOneMinusDstAlpha,
        BlendOp::BlendDstColor,
        BlendOp::BlendOneMinusDstColor,
        BlendOp::BlendSrcAlphaSaturate,
        BlendOp::BlendBothSrcAlpha,
        BlendOp::BlendBothInvSrcAlpha,
        BlendOp::BlendConstantColor,
        BlendOp::BlendOneMinusConstantColor,
        BlendOp::BlendSrc1Color,
        BlendOp::BlendInvSrc1Color,
        BlendOp::BlendSrc1Alpha,
        BlendOp::BlendInvSrc1Alpha,
        BlendOp::BlendConstantAlpha,
        BlendOp::BlendOneMinusConstantAlpha
    };

    constexpr uint32 ConversionTableSize = sizeof(ConversionTable) / sizeof(BlendOp);

    static_assert(BLEND_ZERO == 0, "Conversion table needs to start with zero");
    static_assert(ConversionTableSize == BLEND_ONE_MINUS_CONSTANT_ALPHA + 1,
                  "Conversion table does not include all HW enumerations");

    PAL_ASSERT(ConversionTable[BLEND_ZERO] == BlendOp::BlendZero);
    PAL_ASSERT(hwEnum < ConversionTableSize);

    return ConversionTable[hwEnum];
}

// =====================================================================================================================
static uint32 BlendOptToHw(
    GfxBlendOptimizer::BlendOpt op)
{
    // PAL's enum == HW enum. Static_cast is safe w/o lookup table.
    static_assert((static_cast<uint32>(GfxBlendOptimizer::BlendOpt::ForceOptAuto) ==
        FORCE_OPT_AUTO), "BlendOpt != GfxBlendOptimizer::BlendOpts");
    static_assert((static_cast<uint32>(GfxBlendOptimizer::BlendOpt::ForceOptDisable) ==
        FORCE_OPT_DISABLE), "BlendOpt != GfxBlendOptimizer::BlendOpts");
    static_assert((static_cast<uint32>(GfxBlendOptimizer::BlendOpt::ForceOptEnableIfSrcA0) ==
        FORCE_OPT_ENABLE_IF_SRC_A_0), "BlendOpt != GfxBlendOptimizer::BlendOpts");
    static_assert((static_cast<uint32>(GfxBlendOptimizer::BlendOpt::ForceOptEnableIfSrcRgb0) ==
        FORCE_OPT_ENABLE_IF_SRC_RGB_0), "BlendOpt != GfxBlendOptimizer::BlendOpts");
    static_assert((static_cast<uint32>(GfxBlendOptimizer::BlendOpt::ForceOptEnableIfSrcArgb0) ==
        FORCE_OPT_ENABLE_IF_SRC_ARGB_0), "BlendOpt != GfxBlendOptimizer::BlendOpts");
    static_assert((static_cast<uint32>(GfxBlendOptimizer::BlendOpt::ForceOptEnableIfSrcA1) ==
        FORCE_OPT_ENABLE_IF_SRC_A_1), "BlendOpt != GfxBlendOptimizer::BlendOpts");
    static_assert((static_cast<uint32>(GfxBlendOptimizer::BlendOpt::ForceOptEnableIfSrcRgb1) ==
        FORCE_OPT_ENABLE_IF_SRC_RGB_1), "BlendOpt != GfxBlendOptimizer::BlendOpts");
    static_assert((static_cast<uint32>(GfxBlendOptimizer::BlendOpt::ForceOptEnableIfSrcArgb1) ==
        FORCE_OPT_ENABLE_IF_SRC_ARGB_1), "BlendOpt != GfxBlendOptimizer::BlendOpts");

    return static_cast<uint32>(op);
}

// =====================================================================================================================
// Performs Gfx9 hardware-specific initialization for a color blend state object, including:
// Set up the image of PM4 commands used to write the pipeline to HW.
void ColorBlendState::Init(
    const ColorBlendStateCreateInfo& blend)
{
    for (uint32 i = 0; i < MaxColorTargets; i++)
    {
        if (blend.targets[i].blendEnable)
        {
            m_flags.blendEnable |= (1 << i);
            m_cbBlendControl[i].bits.ENABLE = 1;
        }
        m_cbBlendControl[i].bits.SEPARATE_ALPHA_BLEND = 1;
        m_cbBlendControl[i].bits.COLOR_SRCBLEND       = HwBlendOp(blend.targets[i].srcBlendColor);
        m_cbBlendControl[i].bits.COLOR_DESTBLEND      = HwBlendOp(blend.targets[i].dstBlendColor);
        m_cbBlendControl[i].bits.ALPHA_SRCBLEND       = HwBlendOp(blend.targets[i].srcBlendAlpha);
        m_cbBlendControl[i].bits.ALPHA_DESTBLEND      = HwBlendOp(blend.targets[i].dstBlendAlpha);
        m_cbBlendControl[i].bits.COLOR_COMB_FCN       = HwBlendFunc(blend.targets[i].blendFuncColor);
        m_cbBlendControl[i].bits.ALPHA_COMB_FCN       = HwBlendFunc(blend.targets[i].blendFuncAlpha);

        // BlendOps are forced to ONE for MIN/MAX blend funcs
        if ((m_cbBlendControl[i].bits.COLOR_COMB_FCN == COMB_MIN_DST_SRC) ||
            (m_cbBlendControl[i].bits.COLOR_COMB_FCN == COMB_MAX_DST_SRC))
        {
            m_cbBlendControl[i].bits.COLOR_SRCBLEND  = BLEND_ONE;
            m_cbBlendControl[i].bits.COLOR_DESTBLEND = BLEND_ONE;
        }

        if ((m_cbBlendControl[i].bits.ALPHA_COMB_FCN == COMB_MIN_DST_SRC) ||
            (m_cbBlendControl[i].bits.ALPHA_COMB_FCN == COMB_MAX_DST_SRC))
        {
            m_cbBlendControl[i].bits.ALPHA_SRCBLEND  = BLEND_ONE;
            m_cbBlendControl[i].bits.ALPHA_DESTBLEND = BLEND_ONE;
        }
    }

    m_flags.dualSourceBlend = (IsDualSrcBlendOption(blend.targets[0].srcBlendColor) |
                               IsDualSrcBlendOption(blend.targets[0].dstBlendColor) |
                               IsDualSrcBlendOption(blend.targets[0].srcBlendAlpha) |
                               IsDualSrcBlendOption(blend.targets[0].dstBlendAlpha));

    // CB_BLEND1_CONTROL.ENABLE must be 1 for dual source blending.
    m_cbBlendControl[1].bits.ENABLE |= m_flags.dualSourceBlend;

    InitBlendOpts(blend);

    // SX blend optimizations must be disabled when RB+ is disabled or when dual-source blending is enabled.
    if ((m_flags.dualSourceBlend == 0) && (m_flags.rbPlus != 0))
    {
        for (uint32 i = 0; i < MaxColorTargets; i++)
        {
            if (blend.targets[i].blendEnable == true)
            {
                m_sxMrtBlendOpt[i].bits.COLOR_SRC_OPT = GetSxBlendOptColor(blend.targets[i].srcBlendColor);

                // If src color factor constains Dst, don't optimize color DST. It was said blend factor
                // SrcAlphaSaturate contains DST in RGB channels only.
                if ((blend.targets[i].srcBlendColor == Blend::DstColor) ||
                    (blend.targets[i].srcBlendColor == Blend::OneMinusDstColor) ||
                    (blend.targets[i].srcBlendColor == Blend::DstAlpha) ||
                    (blend.targets[i].srcBlendColor == Blend::OneMinusDstAlpha) ||
                    (blend.targets[i].srcBlendColor == Blend::SrcAlphaSaturate))
                {
                    m_sxMrtBlendOpt[i].bits.COLOR_DST_OPT = BLEND_OPT_PRESERVE_NONE_IGNORE_NONE;
                }
                else
                {
                    m_sxMrtBlendOpt[i].bits.COLOR_DST_OPT = GetSxBlendOptColor(blend.targets[i].dstBlendColor);
                }

                m_sxMrtBlendOpt[i].bits.ALPHA_SRC_OPT = GetSxBlendOptAlpha(blend.targets[i].srcBlendAlpha);

                // If src alpha factor contains DST, don't optimize alpha DST.
                if ((blend.targets[i].srcBlendAlpha == Blend::DstColor) ||
                    (blend.targets[i].srcBlendAlpha == Blend::OneMinusDstColor) ||
                    (blend.targets[i].srcBlendAlpha == Blend::DstAlpha) ||
                    (blend.targets[i].srcBlendAlpha == Blend::OneMinusDstAlpha))
                {
                    m_sxMrtBlendOpt[i].bits.ALPHA_DST_OPT = BLEND_OPT_PRESERVE_NONE_IGNORE_NONE;
                }
                else
                {
                    m_sxMrtBlendOpt[i].bits.ALPHA_DST_OPT = GetSxBlendOptAlpha(blend.targets[i].dstBlendAlpha);
                }

                m_sxMrtBlendOpt[i].bits.COLOR_COMB_FCN = GetSxBlendFcn(blend.targets[i].blendFuncColor);
                m_sxMrtBlendOpt[i].bits.ALPHA_COMB_FCN = GetSxBlendFcn(blend.targets[i].blendFuncAlpha);

                // BlendOpts are forced to ONE for MIN/MAX blend fcns
                if ((m_sxMrtBlendOpt[i].bits.COLOR_COMB_FCN == OPT_COMB_MIN) ||
                    (m_sxMrtBlendOpt[i].bits.COLOR_COMB_FCN == OPT_COMB_MAX))
                {
                    m_sxMrtBlendOpt[i].bits.COLOR_SRC_OPT = BLEND_OPT_PRESERVE_ALL_IGNORE_NONE;
                    m_sxMrtBlendOpt[i].bits.COLOR_DST_OPT = BLEND_OPT_PRESERVE_ALL_IGNORE_NONE;
                }

                if ((m_sxMrtBlendOpt[i].bits.ALPHA_COMB_FCN == OPT_COMB_MIN) ||
                    (m_sxMrtBlendOpt[i].bits.ALPHA_COMB_FCN == OPT_COMB_MAX))
                {
                    m_sxMrtBlendOpt[i].bits.ALPHA_SRC_OPT = BLEND_OPT_PRESERVE_ALL_IGNORE_NONE;
                    m_sxMrtBlendOpt[i].bits.ALPHA_DST_OPT = BLEND_OPT_PRESERVE_ALL_IGNORE_NONE;
                }
            }
            else
            {
                m_sxMrtBlendOpt[i].bits.COLOR_COMB_FCN = OPT_COMB_BLEND_DISABLED;
                m_sxMrtBlendOpt[i].bits.ALPHA_COMB_FCN = OPT_COMB_BLEND_DISABLED;
            }
        }
    }

    InitBlendMasks(blend);
}

// =====================================================================================================================
//  Initializes the different blend optimizations for different configurations of color buffer state per MRT.
//
//  This creates three optimizations for every color target:
//      + Writing to Alpha channel only.
//      + Writing to Color channel only.
//      + Writing to both Alpha and Color channels.
void ColorBlendState::InitBlendOpts(
    const ColorBlendStateCreateInfo& blend)
{
    using namespace GfxBlendOptimizer;

    for (uint32 ct = 0; ct < Pal::MaxColorTargets; ct++)
    {
        // The logic assumes the separate alpha blend is always on
        PAL_ASSERT(m_cbBlendControl[ct].bits.SEPARATE_ALPHA_BLEND == 1);

        Input optInput = { };
        optInput.srcBlend       = HwEnumToBlendOp(m_cbBlendControl[ct].bits.COLOR_SRCBLEND);
        optInput.destBlend      = HwEnumToBlendOp(m_cbBlendControl[ct].bits.COLOR_DESTBLEND);
        optInput.alphaSrcBlend  = HwEnumToBlendOp(m_cbBlendControl[ct].bits.ALPHA_SRCBLEND);
        optInput.alphaDestBlend = HwEnumToBlendOp(m_cbBlendControl[ct].bits.ALPHA_DESTBLEND);

        const uint32 colorCombFcn = m_cbBlendControl[ct].bits.COLOR_COMB_FCN;
        const uint32 alphaCombFcn = m_cbBlendControl[ct].bits.ALPHA_COMB_FCN;

        for (uint32 idx = 0; idx < NumChannelWriteComb; idx++)
        {
            const uint32 optIndex = (ct * NumChannelWriteComb) + idx;

            // Start with AUTO settings for all optimizations
            m_blendOpts[optIndex].discardPixel = BlendOpt::ForceOptAuto;
            // TODO: Consider explicitly overriding destination read optimization.
            m_blendOpts[optIndex].dontRdDst    = BlendOpt::ForceOptAuto;

            // Use explicit optimization settings only when blending is enabled, since HW doesn't check for blending and
            // would blindly apply optimizations even in cases when they shouldn't be applied.

            // Per discussions with HW engineers, RTL has issues with blend optimization for dual source blending.  HW
            // is already turning it off for that case.  Thus, driver must not turn it on as well for dual source
            // blending.
            if ((blend.targets[ct].blendEnable == true) && (m_flags.dualSourceBlend == 0))
            {
                // The three valid alpha/color combinations are:
                //  - AlphaEnabled      = 0x01
                //  - ColorEnabled      = 0x02
                //  - AlphaColorEnabled = (AlphaEnabled | ColorEnabled)
                // The current array index plus one gives us the correct combination.
                const uint32 colorAlphaMask = idx + 1;

                // Color and alpha write masks will determine value requirements for corresponding parts of the blend
                // equation
                optInput.colorWrite = TestAnyFlagSet(colorAlphaMask, ColorEnabled);
                optInput.alphaWrite = TestAnyFlagSet(colorAlphaMask, AlphaEnabled);

                // Try optimizing using the first pixel discard equation
                if (((colorCombFcn == COMB_DST_PLUS_SRC)   ||
                     (colorCombFcn == COMB_DST_MINUS_SRC)) &&
                    ((alphaCombFcn == COMB_DST_PLUS_SRC)   ||
                     (alphaCombFcn == COMB_DST_MINUS_SRC)))
                {
                    m_blendOpts[optIndex].discardPixel = OptimizePixDiscard1(optInput);
                }

                // If couldn't optimize, try another pixel discard equation
                if ((m_blendOpts[optIndex].discardPixel == BlendOpt::ForceOptAuto) &&
                    (colorCombFcn == COMB_DST_PLUS_SRC)                            &&
                    (alphaCombFcn == COMB_DST_PLUS_SRC))
                {
                    m_blendOpts[optIndex].discardPixel = OptimizePixDiscard2(optInput);
                }
            }
        } // for each color/alpha combination
    } // for each MRT
}

// =====================================================================================================================
//  Writes the PM4 commands required to bind the state object to the specified bind point. Returns the next unused DWORD
// in pCmdSpace.
uint32* ColorBlendState::WriteCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmCB_BLEND0_CONTROL,
                                                   mmCB_BLEND7_CONTROL,
                                                   &m_cbBlendControl[0],
                                                   pCmdSpace);
    return pCmdStream->WriteSetSeqContextRegs(mmSX_MRT0_BLEND_OPT,
                                              mmSX_MRT7_BLEND_OPT,
                                              &m_sxMrtBlendOpt[0],
                                              pCmdSpace);
}

// =====================================================================================================================
// Writes the PM4 commands required to enable or disable blending opts. Returns the next unused DWORD in pCmdSpace.
template <bool Pm4OptImmediate>
uint32* ColorBlendState::WriteBlendOptimizations(
    CmdStream*                     pCmdStream,
    const SwizzledFormat*          pTargetFormats,     // [in] Array of pixel formats per target.
    const uint8*                   pTargetWriteMasks,  // [in] Array of 4-bit write masks for each target.
    bool                           enableOpts,
    GfxBlendOptimizer::BlendOpts*  pBlendOpts,         // [in/out] Blend optimizations
    uint32*                        pCmdSpace
    ) const
{
    using namespace GfxBlendOptimizer;

    for (uint32 idx = 0; idx < MaxColorTargets; idx++)
    {
        if ((Formats::IsUndefined(pTargetFormats[idx].format) == false) && (pTargetWriteMasks[idx] != 0))
        {
            BlendOpt dontRdDst    = BlendOpt::ForceOptDisable;
            BlendOpt discardPixel = BlendOpt::ForceOptDisable;

            if (enableOpts)
            {
                constexpr uint32 AlphaMask = ColorWriteEnable::Alpha;
                constexpr uint32 ColorMask = ColorWriteEnable::Red | ColorWriteEnable::Green | ColorWriteEnable::Blue;

                const uint32 channelWriteMask     = pTargetWriteMasks[idx];
                const uint32 colorEnabled         = TestAnyFlagSet(channelWriteMask, ColorMask) ?  ColorEnabled : 0;
                const uint32 alphaEnabled         = TestAnyFlagSet(channelWriteMask, AlphaMask) ?  AlphaEnabled : 0;
                const uint32 channelWritesEnabled = colorEnabled + alphaEnabled;

                // Shouldn't have CB with no writable channels.
                PAL_ASSERT(channelWritesEnabled != 0);

                const uint32 optIndex = (idx * NumChannelWriteComb) + (channelWritesEnabled - 1);

                dontRdDst    = m_blendOpts[optIndex].dontRdDst;
                discardPixel = m_blendOpts[optIndex].discardPixel;
            }

            // Update blend optimizations if changed
            if ((pBlendOpts[idx].dontRdDst != dontRdDst) || (pBlendOpts[idx].discardPixel != discardPixel))
            {
                constexpr uint32 BlendOptRegMask = (CB_COLOR0_INFO__BLEND_OPT_DONT_RD_DST_MASK |
                                                    CB_COLOR0_INFO__BLEND_OPT_DISCARD_PIXEL_MASK);
                regCB_COLOR0_INFO regValue = {};
                regValue.bits.BLEND_OPT_DONT_RD_DST   = BlendOptToHw(dontRdDst);
                regValue.bits.BLEND_OPT_DISCARD_PIXEL = BlendOptToHw(discardPixel);

                pCmdSpace = pCmdStream->WriteContextRegRmw<Pm4OptImmediate>(mmCB_COLOR0_INFO + idx * CbRegsPerSlot,
                                                                            BlendOptRegMask,
                                                                            regValue.u32All,
                                                                            pCmdSpace);

                pBlendOpts[idx].dontRdDst    = dontRdDst;
                pBlendOpts[idx].discardPixel = discardPixel;
            }
        }
    }

    return pCmdSpace;
}

template
uint32* ColorBlendState::WriteBlendOptimizations<true>(
    CmdStream*                     pCmdStream,
    const SwizzledFormat*          pTargetFormats,
    const uint8*                   pTargetWriteMasks,
    bool                           enableOpts,
    GfxBlendOptimizer::BlendOpts*  pBlendOpts,
    uint32*                        pCmdSpace
    ) const;
template
uint32* ColorBlendState::WriteBlendOptimizations<false>(
    CmdStream*                     pCmdStream,
    const SwizzledFormat*          pTargetFormats,
    const uint8*                   pTargetWriteMasks,
    bool                           enableOpts,
    GfxBlendOptimizer::BlendOpts*  pBlendOpts,
    uint32*                        pCmdSpace
    ) const;

// =====================================================================================================================
// Examines the blend state for each target to determine if the state is commutative and sets/clears the appropriate bit
// in m_blendCommutativeMask, or if the state allows the destination to be read and sets/clears the appropriate
// bit in m_blendReadsDestMask.
void ColorBlendState::InitBlendMasks(
    const ColorBlendStateCreateInfo& createInfo)
{
    for (uint32 rtIdx = 0; rtIdx < MaxColorTargets; rtIdx++)
    {
        Blend srcBlends[2];
        srcBlends[0] = createInfo.targets[rtIdx].srcBlendColor;
        srcBlends[1] = createInfo.targets[rtIdx].srcBlendAlpha;

        Blend dstBlends[2];
        dstBlends[0] = createInfo.targets[rtIdx].dstBlendColor;
        dstBlends[1] = createInfo.targets[rtIdx].dstBlendAlpha;

        BlendFunc blendOps[2];
        blendOps[0] = createInfo.targets[rtIdx].blendFuncColor;
        blendOps[1] = createInfo.targets[rtIdx].blendFuncAlpha;

        bool isCommutative[2] = { false, false };

        for (uint32 k = 0; k < 2; k++)
        {
            if ((dstBlends[k] != Blend::Zero)             ||
                (srcBlends[k] == Blend::DstAlpha)         ||
                (srcBlends[k] == Blend::OneMinusDstAlpha) ||
                (srcBlends[k] == Blend::DstColor)         ||
                (srcBlends[k] == Blend::OneMinusDstColor))
            {
                m_flags.blendReadsDst |= (1 << rtIdx);
            }

            // Min and max blend ops are always commutative as they ignore the blend multiplier and operate directly on
            // the PS output and the current value in the render target.
            if ((blendOps[k] == BlendFunc::Min) || (blendOps[k] == BlendFunc::Max))
            {
                isCommutative[k] = true;
            }

            // Check for commutative additive/subtractive blending.
            // Dst = Dst + S1 + S2 + ...  or
            // Dst = Dst - S1 - S2 - ...
            if ((dstBlends[k] == Blend::One) &&
                ((srcBlends[k] == Blend::Zero) ||
                 (srcBlends[k] == Blend::One)  ||
                 (srcBlends[k] == Blend::SrcColor) ||
                 (srcBlends[k] == Blend::OneMinusSrcColor) ||
                 (srcBlends[k] == Blend::SrcAlpha) ||
                 (srcBlends[k] == Blend::OneMinusSrcAlpha) ||
                 (srcBlends[k] == Blend::ConstantColor) ||
                 (srcBlends[k] == Blend::OneMinusConstantColor) ||
                 (srcBlends[k] == Blend::Src1Color) ||
                 (srcBlends[k] == Blend::OneMinusSrc1Color) ||
                 (srcBlends[k] == Blend::Src1Alpha) ||
                 (srcBlends[k] == Blend::OneMinusSrc1Alpha)) &&
                ((blendOps[k] == BlendFunc::Add) ||
                 (blendOps[k] == BlendFunc::ReverseSubtract)))
            {
                isCommutative[k] = true;
            }

            const Blend colorOrAlphaSrcBlend = (k == 0) ? Blend::SrcColor : Blend::SrcAlpha;
            const Blend colorOrAlphaDstBlend = (k == 0) ? Blend::DstColor : Blend::DstAlpha;

            // Check for commutative multiplicitive blending. Dst = Dst * S1 * S2 * ...  The last two cases are unusual
            // because they use destination data as the source multiplier.  In those cases we must be sure that the dst
            // data is being multiplied by the source as that is the only multiplicative commutative case when using a
            // srcBlend.
            if ((srcBlends[k] == Blend::Zero) &&
                ((blendOps[k] == BlendFunc::Add) ||
                 (blendOps[k] == BlendFunc::ReverseSubtract)) &&
                ((dstBlends[k] == Blend::Zero) ||
                 (dstBlends[k] == Blend::One) ||
                 (dstBlends[k] == Blend::SrcColor) ||
                 (dstBlends[k] == Blend::OneMinusSrcColor) ||
                 (dstBlends[k] == Blend::SrcAlpha) ||
                 (dstBlends[k] == Blend::OneMinusSrcAlpha) ||
                 (dstBlends[k] == Blend::ConstantColor) ||
                 (dstBlends[k] == Blend::OneMinusConstantColor)))
            {
                isCommutative[k] = true;
            }
            else if ((dstBlends[k] == Blend::Zero) &&
                     ((blendOps[k] == BlendFunc::Add) ||
                      (blendOps[k] == BlendFunc::Subtract)) &&
                     ((srcBlends[k] == Blend::Zero) ||
                      (srcBlends[k] == colorOrAlphaDstBlend)))
            {
                isCommutative[k] = true;
            }
            else if ((blendOps[k] == BlendFunc::Add)        &&
                     (dstBlends[k] == colorOrAlphaSrcBlend) &&
                     (srcBlends[k] == colorOrAlphaDstBlend))
            {
                // This is the Dst = (Dst * Src) + (Src * Dst) case.
                isCommutative[k] = true;
            }
        }

        if (createInfo.targets[rtIdx].blendEnable && isCommutative[0] && isCommutative[1])
        {
            m_flags.blendCommutative |= (1 << rtIdx);
        }
    }
}

} // Gfx9
} // Pal
