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

#include "core/hw/gfxip/gfx12/gfx12CmdUtil.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"
#include "core/hw/gfxip/gfx12/gfx12GraphicsPipeline.h"
#include "core/hw/gfxip/gfx12/gfx12GraphicsShaderLibrary.h"
#include "core/hw/gfxip/gfx12/gfx12ComputePipeline.h"
#include "core/hw/gfxip/gfx12/gfx12UniversalCmdBuffer.h"
#include "palInlineFuncs.h"

using namespace Util;
using namespace Pal::Gfx12::Chip;

namespace Pal
{
namespace Gfx12
{

constexpr uint32 HsWaveLimitMax =
    SPI_SHADER_PGM_RSRC4_HS__WAVE_LIMIT_MASK >> SPI_SHADER_PGM_RSRC4_HS__WAVE_LIMIT__SHIFT;
constexpr uint32 GsWaveLimitMax =
    SPI_SHADER_PGM_RSRC4_GS__WAVE_LIMIT_MASK >> SPI_SHADER_PGM_RSRC4_GS__WAVE_LIMIT__SHIFT;
constexpr uint32 PsWaveLimitMax =
    SPI_SHADER_PGM_RSRC4_PS__WAVE_LIMIT_MASK >> SPI_SHADER_PGM_RSRC4_PS__WAVE_LIMIT__SHIFT;

// =====================================================================================================================
// Converts the specified logic op enum into a ROP3 code.
static uint8 Rop3(
    LogicOp logicOp)
{
    constexpr uint8 Rop3Codes[] =
    {
        0xCC, // Copy (S)
        0x00, // Clear (clear to 0)
        0x88, // And (S & D)
        0x44, // AndReverse (S & (~D))
        0x22, // AndInverted ((~S) & D)
        0xAA, // Noop (D)
        0x66, // Xor (S ^ D)
        0xEE, // Or (S | D)
        0x11, // Nor (~(S | D))
        0x99, // Equiv (~(S ^ D))
        0x55, // Invert (~D)
        0xDD, // OrReverse (S | (~D))
        0x33, // CopyInverted (~S)
        0xBB, // OrInverted ((~S) | D)
        0x77, // Nand (~(S & D))
        0xFF  // Set (set to 1)
    };

    return Rop3Codes[static_cast<uint32>(logicOp)];
}

// =====================================================================================================================
// Returns the SX "downconvert" format with respect to the channel format of the color buffer target.
// This method is for the RbPlus feature which is identical to the gfx8.1 implementation.
static SX_DOWNCONVERT_FORMAT SxDownConvertFormat(
    SwizzledFormat swizzledFormat)
{
    SX_DOWNCONVERT_FORMAT sxDownConvertFormat = SX_RT_EXPORT_NO_CONVERSION;

    switch (swizzledFormat.format)
    {
    case ChNumFormat::X4Y4Z4W4_Unorm:
    case ChNumFormat::X4Y4Z4W4_Uscaled:
        sxDownConvertFormat = SX_RT_EXPORT_4_4_4_4;
        break;
    case ChNumFormat::X5Y6Z5_Unorm:
    case ChNumFormat::X5Y6Z5_Uscaled:
        sxDownConvertFormat = SX_RT_EXPORT_5_6_5;
        break;
    case ChNumFormat::X5Y5Z5W1_Unorm:
    case ChNumFormat::X5Y5Z5W1_Uscaled:
        sxDownConvertFormat = SX_RT_EXPORT_1_5_5_5;
        break;
    case ChNumFormat::X8_Unorm:
    case ChNumFormat::X8_Snorm:
    case ChNumFormat::X8_Uscaled:
    case ChNumFormat::X8_Sscaled:
    case ChNumFormat::X8_Uint:
    case ChNumFormat::X8_Sint:
    case ChNumFormat::X8_Srgb:
    case ChNumFormat::L8_Unorm:
    case ChNumFormat::P8_Unorm:
    case ChNumFormat::X8Y8_Unorm:
    case ChNumFormat::X8Y8_Snorm:
    case ChNumFormat::X8Y8_Uscaled:
    case ChNumFormat::X8Y8_Sscaled:
    case ChNumFormat::X8Y8_Uint:
    case ChNumFormat::X8Y8_Sint:
    case ChNumFormat::X8Y8_Srgb:
    case ChNumFormat::L8A8_Unorm:
    case ChNumFormat::X8Y8Z8W8_Unorm:
    case ChNumFormat::X8Y8Z8W8_Snorm:
    case ChNumFormat::X8Y8Z8W8_Uscaled:
    case ChNumFormat::X8Y8Z8W8_Sscaled:
    case ChNumFormat::X8Y8Z8W8_Uint:
    case ChNumFormat::X8Y8Z8W8_Sint:
    case ChNumFormat::X8Y8Z8W8_Srgb:
        sxDownConvertFormat = SX_RT_EXPORT_8_8_8_8;
        break;
    case ChNumFormat::X11Y11Z10_Float:
        sxDownConvertFormat = SX_RT_EXPORT_10_11_11;
        break;
    case ChNumFormat::X10Y10Z10W2_Unorm:
    case ChNumFormat::X10Y10Z10W2_Uscaled:
        sxDownConvertFormat = SX_RT_EXPORT_2_10_10_10;
        break;
    case ChNumFormat::X16_Unorm:
    case ChNumFormat::X16_Snorm:
    case ChNumFormat::X16_Uscaled:
    case ChNumFormat::X16_Sscaled:
    case ChNumFormat::X16_Uint:
    case ChNumFormat::X16_Sint:
    case ChNumFormat::X16_Float:
    case ChNumFormat::L16_Unorm:
        sxDownConvertFormat = SX_RT_EXPORT_16_16_AR;
        break;
    case ChNumFormat::X16Y16_Unorm:
    case ChNumFormat::X16Y16_Snorm:
    case ChNumFormat::X16Y16_Uscaled:
    case ChNumFormat::X16Y16_Sscaled:
    case ChNumFormat::X16Y16_Uint:
    case ChNumFormat::X16Y16_Sint:
    case ChNumFormat::X16Y16_Float:
        sxDownConvertFormat =
            (swizzledFormat.swizzle.a == ChannelSwizzle::Y) ? SX_RT_EXPORT_16_16_AR : SX_RT_EXPORT_16_16_GR;
        break;
    case ChNumFormat::X32_Uint:
    case ChNumFormat::X32_Sint:
    case ChNumFormat::X32_Float:
        sxDownConvertFormat =
            (swizzledFormat.swizzle.a == ChannelSwizzle::X) ? SX_RT_EXPORT_32_A : SX_RT_EXPORT_32_R;
        break;
    case ChNumFormat::X9Y9Z9E5_Float:
        //  When doing 8 pixels per clock transfers (in RB+ mode) on a render target using the 999e5 format, the
        //  SX must convert the exported data to 999e5

        sxDownConvertFormat = SX_RT_EXPORT_9_9_9_E5;
        break;
    default:
        break;
    }

    return sxDownConvertFormat;
}

// =====================================================================================================================
// Get the SX blend opt control with respect to the specified writemask.
// This method is for the RbPlus feature which is identical to the gfx8.1 implementation.
static uint32 SxBlendOptControl(
    uint32 writeMask)
{
    uint32 sxBlendOptControl = 0;

    // In order to determine if alpha or color channels are meaningful to the blender, the blend equations and
    // coefficients need to be examined for any interdependency. Instead, rely on the SX optimization result except for
    // the trivial cases: write disabled here and blend disabled using COMB_FCN of SX_MRTx_BLEND_OPT.
    if (writeMask == 0)
    {
        sxBlendOptControl = SX_BLEND_OPT_CONTROL__MRT0_COLOR_OPT_DISABLE_MASK |
                            SX_BLEND_OPT_CONTROL__MRT0_ALPHA_OPT_DISABLE_MASK;
    }

    return sxBlendOptControl;
}

// =====================================================================================================================
// Get the sx-blend-opt-epsilon with respect to SX "downconvert" format.
// This method is for the RbPlus feature which is identical to the gfx8.1 implementation.
static uint32 SxBlendOptEpsilon(
    SX_DOWNCONVERT_FORMAT sxDownConvertFormat)
{
    uint32 sxBlendOptEpsilon = 0;

    switch (sxDownConvertFormat)
    {
    case SX_RT_EXPORT_NO_CONVERSION: // Don't care, just use 0.
    case SX_RT_EXPORT_32_R:
    case SX_RT_EXPORT_32_A:
    case SX_RT_EXPORT_16_16_GR:
    case SX_RT_EXPORT_16_16_AR:
    case SX_RT_EXPORT_10_11_11: // 1 is recommended, but doesn't provide sufficient precision
    case SX_RT_EXPORT_9_9_9_E5:
        sxBlendOptEpsilon = 0;
        break;
    case SX_RT_EXPORT_2_10_10_10:
        sxBlendOptEpsilon = 3;
        break;
    case SX_RT_EXPORT_8_8_8_8:  // 7 is recommended, but doesn't provide sufficient precision
        sxBlendOptEpsilon = 6;
        break;
    case SX_RT_EXPORT_5_6_5:
        sxBlendOptEpsilon = 11;
        break;
    case SX_RT_EXPORT_1_5_5_5:
        sxBlendOptEpsilon = 13;
        break;
    case SX_RT_EXPORT_4_4_4_4:
        sxBlendOptEpsilon = 15;
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return sxBlendOptEpsilon;
}

// =====================================================================================================================
GraphicsPipeline::GraphicsPipeline(
    Device* pDevice,
    bool    isInternal)
    :
    Pal::GraphicsPipeline(pDevice->Parent(), isInternal),
    m_strmoutVtxStride{},
    m_lowFreqCtxRegHash(0),
    m_medFreqCtxRegHash(0),
    m_highFreqCtxRegHash(0),
    m_numInterpolants(0),
    m_hiZRoundVal(pDevice->Settings().hiDepthRound),
    m_isBinningDisabled(false),
    m_pUserDataLayout(nullptr),
    m_disableGroupLaunchGuarantee(true),
    m_isAlphaToCoverage(false),
    m_noForceReZ(false),
    m_prefetch{},
    m_prefetchRangeCount(0),
    m_lowFreqRegs{},
    m_medFreqRegs{},
    m_highFreqRegs{},
    m_hullShaderRegs{},
    m_geomShaderRegs{},
    m_esGsLdsSize{},
    m_pixelShaderRegs{},
    m_hsStageInfo{},
    m_gsStageInfo{},
    m_psStageInfo{},
    m_semanticInfo{},
    m_semanticCount(0),
    m_ringSizes{},
    m_colorExportAddr{},
    m_depthOnlyOptMetadata{}
{
    LowFreq::Init(m_lowFreqRegs);
    MedFreq::Init(m_medFreqRegs);
    HighFreq::Init(m_highFreqRegs.pairs);
    HullShader::Init(m_hullShaderRegs);
    GeomShader::Init(m_geomShaderRegs);
    PixelShader::Init(m_pixelShaderRegs);

    HullShader::Get<mmSPI_SHADER_USER_DATA_HS_1, SPI_SHADER_USER_DATA_HS_1>(m_hullShaderRegs)->u32All =
        InvalidUserDataInternalTable;

    GeomShader::Get<mmSPI_SHADER_USER_DATA_GS_1, SPI_SHADER_USER_DATA_GS_1>(m_geomShaderRegs)->u32All =
        InvalidUserDataInternalTable;

    PixelShader::Get<mmSPI_SHADER_USER_DATA_PS_1, SPI_SHADER_USER_DATA_PS_1>(m_pixelShaderRegs)->u32All =
        InvalidUserDataInternalTable;

    PAL_ASSERT(m_esGsLdsSize.offset == UserDataNotMapped);

    static_assert((DynamicStateOverrideCtx::Exist(mmSX_PS_DOWNCONVERT)    == false) &&
                  (DynamicStateOverrideCtx::Exist(mmSX_BLEND_OPT_EPSILON) == false) &&
                  (DynamicStateOverrideCtx::Exist(mmSX_BLEND_OPT_CONTROL) == false) &&
                  (DynamicStateOverrideCtx::Exist(mmCB_SHADER_MASK)       == false),
                  "OverrideColorExportRegistersForRpm and DynamicState cannot overlap!");
    // Note: CB_TARGET_MASK *does* overlap between those two states, but we explicitly handle that one case.
}

// =====================================================================================================================
GraphicsPipeline::~GraphicsPipeline()
{
    if (m_pUserDataLayout != nullptr)
    {
        m_pUserDataLayout->Destroy();
        m_pUserDataLayout = nullptr;
    }
}

// =====================================================================================================================
void GraphicsPipeline::OverrideColorExportRegistersForRpm(
    SwizzledFormat          swizzledFormat,
    uint32                  slot,
    DynamicRpmOverrideRegs* pRegs
    ) const
{
    constexpr uint32 BitsPerRegField = (sizeof(uint32) * 8) / MaxColorTargets;
    // These registers all split up their 32b values into 8MRT*4b.
    const uint32 bitShift = slot * BitsPerRegField;
    PAL_ASSERT(slot < MaxColorTargets);

    if (slot != 0)
    {
        static_assert(CheckSequential({
            CB_SHADER_MASK__OUTPUT0_ENABLE__SHIFT,
            CB_SHADER_MASK__OUTPUT1_ENABLE__SHIFT,
            CB_SHADER_MASK__OUTPUT2_ENABLE__SHIFT,
            CB_SHADER_MASK__OUTPUT3_ENABLE__SHIFT,
            CB_SHADER_MASK__OUTPUT4_ENABLE__SHIFT,
            CB_SHADER_MASK__OUTPUT5_ENABLE__SHIFT,
            CB_SHADER_MASK__OUTPUT6_ENABLE__SHIFT,
            CB_SHADER_MASK__OUTPUT7_ENABLE__SHIFT,
        }, BitsPerRegField), "CB_SHADER_MASK layout has changed");
        static_assert(CheckSequential({
            CB_TARGET_MASK__TARGET0_ENABLE__SHIFT,
            CB_TARGET_MASK__TARGET1_ENABLE__SHIFT,
            CB_TARGET_MASK__TARGET2_ENABLE__SHIFT,
            CB_TARGET_MASK__TARGET3_ENABLE__SHIFT,
            CB_TARGET_MASK__TARGET4_ENABLE__SHIFT,
            CB_TARGET_MASK__TARGET5_ENABLE__SHIFT,
            CB_TARGET_MASK__TARGET6_ENABLE__SHIFT,
            CB_TARGET_MASK__TARGET7_ENABLE__SHIFT,
        }, BitsPerRegField), "CB_TARGET_MASK layout has changed");
        static_assert(CheckSequential({
            SX_PS_DOWNCONVERT__MRT0__SHIFT,
            SX_PS_DOWNCONVERT__MRT1__SHIFT,
            SX_PS_DOWNCONVERT__MRT2__SHIFT,
            SX_PS_DOWNCONVERT__MRT3__SHIFT,
            SX_PS_DOWNCONVERT__MRT4__SHIFT,
            SX_PS_DOWNCONVERT__MRT5__SHIFT,
            SX_PS_DOWNCONVERT__MRT6__SHIFT,
            SX_PS_DOWNCONVERT__MRT7__SHIFT,
        }, BitsPerRegField), "SX_PS_DOWNCONVERT layout has changed");
        static_assert(CheckSequential({
            SX_BLEND_OPT_CONTROL__MRT0_COLOR_OPT_DISABLE__SHIFT,
            SX_BLEND_OPT_CONTROL__MRT1_COLOR_OPT_DISABLE__SHIFT,
            SX_BLEND_OPT_CONTROL__MRT2_COLOR_OPT_DISABLE__SHIFT,
            SX_BLEND_OPT_CONTROL__MRT3_COLOR_OPT_DISABLE__SHIFT,
            SX_BLEND_OPT_CONTROL__MRT4_COLOR_OPT_DISABLE__SHIFT,
            SX_BLEND_OPT_CONTROL__MRT5_COLOR_OPT_DISABLE__SHIFT,
            SX_BLEND_OPT_CONTROL__MRT6_COLOR_OPT_DISABLE__SHIFT,
            SX_BLEND_OPT_CONTROL__MRT7_COLOR_OPT_DISABLE__SHIFT,
        }, BitsPerRegField), "SX_BLEND_OPT_CONTROL layout has changed");
        static_assert(CheckSequential({
            SX_BLEND_OPT_EPSILON__MRT0_EPSILON__SHIFT,
            SX_BLEND_OPT_EPSILON__MRT1_EPSILON__SHIFT,
            SX_BLEND_OPT_EPSILON__MRT2_EPSILON__SHIFT,
            SX_BLEND_OPT_EPSILON__MRT3_EPSILON__SHIFT,
            SX_BLEND_OPT_EPSILON__MRT4_EPSILON__SHIFT,
            SX_BLEND_OPT_EPSILON__MRT5_EPSILON__SHIFT,
            SX_BLEND_OPT_EPSILON__MRT6_EPSILON__SHIFT,
            SX_BLEND_OPT_EPSILON__MRT7_EPSILON__SHIFT,
        }, BitsPerRegField), "SX_BLEND_OPT_EPSILON layout has changed");
        pRegs->sxPsDownconvert.u32All   <<= bitShift;
        pRegs->sxBlendOptEpsilon.u32All <<= bitShift;
        pRegs->sxBlendOptControl.u32All <<= bitShift;
        pRegs->cbShaderMask.u32All      <<= bitShift;
        pRegs->cbTargetMask.u32All      <<= bitShift;
    }

    PAL_ASSERT(m_pDevice->ChipProperties().gfx9.rbPlus == 1); ///< All known GFX12 chips are RB+.

    const SX_DOWNCONVERT_FORMAT downConvertFormat = SxDownConvertFormat(swizzledFormat);

    const uint32 blendOptControl =
        SxBlendOptControl(static_cast<uint8>(Formats::ComponentMask(swizzledFormat.format)));

    const uint32 blendOptEpsilon = SxBlendOptEpsilon(downConvertFormat);

    pRegs->sxPsDownconvert.u32All &= ~(SX_PS_DOWNCONVERT__MRT0_MASK << bitShift);
    pRegs->sxPsDownconvert.u32All |= (static_cast<uint32>(downConvertFormat) << bitShift);

    pRegs->sxBlendOptEpsilon.u32All &= ~(SX_BLEND_OPT_EPSILON__MRT0_EPSILON_MASK << bitShift);
    pRegs->sxBlendOptEpsilon.u32All |= (blendOptEpsilon << bitShift);

    pRegs->sxBlendOptControl.u32All &=
        ~((SX_BLEND_OPT_CONTROL__MRT0_COLOR_OPT_DISABLE_MASK |
           SX_BLEND_OPT_CONTROL__MRT0_ALPHA_OPT_DISABLE_MASK) << bitShift);
    pRegs->sxBlendOptControl.u32All |= (blendOptControl << bitShift);
}

// =====================================================================================================================
uint32* GraphicsPipeline::UpdateMrtSlotAndRbPlusFormatState(
    SwizzledFormat  swizzledFormat,
    uint32          targetIndex,
    CB_TARGET_MASK* pCbTargetMask,
    uint32*         pCmdSpace
    ) const
{
    // These two paths both touch SX_PS_DOWNCONVERT but they should not overlap since one is modifying when color is
    // bound while the other is modifying when color is not bound.
    PAL_ASSERT(m_depthOnlyOptMetadata.isCandidate == 0);

    DynamicRpmOverrideRegs dynRpmRegs;

    // Initialize from member copy
    dynRpmRegs.sxPsDownconvert.u32All   = m_highFreqRegs.pairs[HighFreq::Index(mmSX_PS_DOWNCONVERT)].value;
    dynRpmRegs.sxBlendOptEpsilon.u32All = m_highFreqRegs.pairs[HighFreq::Index(mmSX_BLEND_OPT_EPSILON)].value;
    dynRpmRegs.sxBlendOptControl.u32All = m_highFreqRegs.pairs[HighFreq::Index(mmSX_BLEND_OPT_CONTROL)].value;
    dynRpmRegs.cbTargetMask             = *pCbTargetMask;
    dynRpmRegs.cbShaderMask.u32All      = m_highFreqRegs.pairs[HighFreq::Index(mmCB_SHADER_MASK)].value;

    // Update local copy of registers
    OverrideColorExportRegistersForRpm(swizzledFormat, targetIndex, &dynRpmRegs);

    static_assert(((mmSX_PS_DOWNCONVERT    + 1) == mmSX_BLEND_OPT_EPSILON)                      &&
                  ((mmSX_BLEND_OPT_EPSILON + 1) == mmSX_BLEND_OPT_CONTROL)                      &&
                  (offsetof(DynamicRpmOverrideRegs, sxPsDownconvert)   == (0 * sizeof(uint32))) &&
                  (offsetof(DynamicRpmOverrideRegs, sxBlendOptEpsilon) == (1 * sizeof(uint32))) &&
                  (offsetof(DynamicRpmOverrideRegs, sxBlendOptControl) == (2 * sizeof(uint32))),
                  "Dynamic RPM regs are expected to be sequential!");

    // Simply re-write this state in this rare case stomping on the value (possibly) written above.
    // Since this is an uncommon path, we've decided to take the CP overhead here instead of the CPU
    // overhead for the common path.
    pCmdSpace = CmdStream::WriteSetSeqContextRegs(mmSX_PS_DOWNCONVERT,
                                                  mmSX_BLEND_OPT_CONTROL,
                                                  &dynRpmRegs.sxPsDownconvert,
                                                  pCmdSpace);

    static_assert(CheckSequentialRegs({
        {mmCB_TARGET_MASK, offsetof(DynamicRpmOverrideRegs, cbTargetMask)},
        {mmCB_SHADER_MASK, offsetof(DynamicRpmOverrideRegs, cbShaderMask)}
    }), "Dynamic RPM regs are expected to be sequential!");

    if (targetIndex > 0)
    {
        // Also rewrite this state, understanding that remapping the target is even more rare.
        pCmdSpace = CmdStream::WriteSetSeqContextRegs(mmCB_TARGET_MASK,
                                                      mmCB_SHADER_MASK,
                                                      &dynRpmRegs.cbTargetMask,
                                                      pCmdSpace);

        // Copy this back because it is also used in other places
        *pCbTargetMask = dynRpmRegs.cbTargetMask;
    }

    return pCmdSpace;
}

// =====================================================================================================================
static inline uint32* CopyShRegPairs(
    uint32*                  pCmdSpace,
    const RegisterValuePair* pRegValuePairs,
    uint32                   numRegPairs)
{
    if (numRegPairs > 0)
    {
        memcpy(pCmdSpace, pRegValuePairs, numRegPairs * sizeof(RegisterValuePair));
        pCmdSpace += numRegPairs * 2;
    }

    return pCmdSpace;
}

// =====================================================================================================================
uint32* GraphicsPipeline::CopyShRegPairsToCmdSpace(
    const DynamicGraphicsShaderInfos& dynamicInfo,
    uint32*                           pCmdSpace
    ) const
{
    const bool   isTess        =
        (MedFreq::GetC<mmVGT_SHADER_STAGES_EN, VGT_SHADER_STAGES_EN>(m_medFreqRegs).bits.HS_EN != 0);
    const uint32 psNumRegPairs =
        (PixelShader::GetC<mmSPI_SHADER_USER_DATA_PS_1, SPI_SHADER_USER_DATA_PS_1>(m_pixelShaderRegs).u32All ==
         InvalidUserDataInternalTable) ? (PixelShader::Size() - 1) : PixelShader::Size();
    const uint32 hsNumRegPairs = isTess ?
        ((HullShader::GetC<mmSPI_SHADER_USER_DATA_HS_1, SPI_SHADER_USER_DATA_HS_1>(m_hullShaderRegs).u32All ==
          InvalidUserDataInternalTable) ? (HullShader::Size() - 1) : HullShader::Size()) : 0;

    // Skip write the first mesh shader special registers SPI_SHADER_GS_MESHLET_* if mesh shader is disabled.
    const RegisterValuePair* pGsRegStart   = HasMeshShader() ? m_geomShaderRegs   : &m_geomShaderRegs[NumGsMeshRegs];
    uint32                   gsNumRegPairs = HasMeshShader() ? GeomShader::Size() : GeomShader::Size() - NumGsMeshRegs;

    if (GeomShader::GetC<mmSPI_SHADER_USER_DATA_GS_1, SPI_SHADER_USER_DATA_GS_1>(m_geomShaderRegs).u32All ==
        InvalidUserDataInternalTable)
    {
        gsNumRegPairs--;
    }

    const uint32 esGsLdsSizeNumRegPairs = (m_esGsLdsSize.offset != UserDataNotMapped) ? 1 : 0;

    pCmdSpace = CopyShRegPairs(pCmdSpace, pGsRegStart,       gsNumRegPairs);
    pCmdSpace = CopyShRegPairs(pCmdSpace, m_pixelShaderRegs, psNumRegPairs);
    pCmdSpace = CopyShRegPairs(pCmdSpace, m_hullShaderRegs,  hsNumRegPairs);
    pCmdSpace = CopyShRegPairs(pCmdSpace, &m_esGsLdsSize,    esGsLdsSizeNumRegPairs);

    uint32 numDynShRegPairs = 0;

    if (dynamicInfo.enable.u8All != 0)
    {
        RegisterValuePair dynShRegs[DynamicStateOverrideSh::Size()];

        // Copy immutable copy from init-time to local copy.
        dynShRegs[DynamicStateOverrideSh::Index(mmSPI_SHADER_PGM_RSRC4_GS)] =
            m_geomShaderRegs[GeomShader::Index(mmSPI_SHADER_PGM_RSRC4_GS)];
        dynShRegs[DynamicStateOverrideSh::Index(mmSPI_SHADER_PGM_RSRC4_HS)] =
            m_hullShaderRegs[HullShader::Index(mmSPI_SHADER_PGM_RSRC4_HS)];
        dynShRegs[DynamicStateOverrideSh::Index(mmSPI_SHADER_PGM_RSRC4_PS)] =
            m_pixelShaderRegs[PixelShader::Index(mmSPI_SHADER_PGM_RSRC4_PS)];

        // Update local copy if necessary and flush to command stream.
        const bool anyShRegsUpdated = HandleDynamicWavesPerCu(dynamicInfo, dynShRegs);

        if (anyShRegsUpdated)
        {
            numDynShRegPairs = DynamicStateOverrideSh::Size();
            pCmdSpace = CopyShRegPairs(pCmdSpace, dynShRegs, numDynShRegPairs);
        }
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Writes context and uconfig commands to bind this graphics pipeline. Persistant state is updated separately!
uint32* GraphicsPipeline::WriteContextAndUConfigCommands(
    const DynamicGraphicsState& dynamicGraphicsInfo,
    GfxState*                   pGfxState,
    SwizzledFormat              swizzledFormat,
    uint32                      targetIndex,
    Gfx12RedundantStateFilter   filterFlags,
    DepthClampMode*             pDepthClampMode,
    regPA_CL_CLIP_CNTL*         pPaClClipCntl,
    uint32*                     pCmdSpace
    ) const
{
    PAL_ASSERT(pGfxState != nullptr);

    bool writeLowFreq  = false;
    bool writeMedFreq  = false;
    bool writeHighFreq = false;

    // Check if we need to write low freq ctx state
    if ((pGfxState->validBits.pipelineCtxLowHash == false)        ||
        (pGfxState->pipelineCtxLowPktHash != m_lowFreqCtxRegHash) ||
        ((filterFlags & Gfx12RedundantStateFilterPipelineCtxLow) == 0))
    {
        writeLowFreq = true;

        // Update hash in cmdbuffer
        pGfxState->pipelineCtxLowPktHash = m_lowFreqCtxRegHash;
    }

    // Check if we need to write med freq ctx state
    if ((pGfxState->validBits.pipelineCtxMedHash == false)        ||
        (pGfxState->pipelineCtxMedPktHash != m_medFreqCtxRegHash) ||
        ((filterFlags & Gfx12RedundantStateFilterPipelineCtxMed) == 0))
    {
        writeMedFreq = true;

        // Update hash in cmdbuffer
        pGfxState->pipelineCtxMedPktHash = m_medFreqCtxRegHash;
    }

    // Check if we need to write high freq ctx state
    if ((pGfxState->validBits.pipelineCtxHighHash == false)             ||
        (pGfxState->pipelineCtxHighPktHash != m_highFreqCtxRegHash)     ||
        ((filterFlags & Gfx12RedundantStateFilterPipelineCtxHigh) == 0) ||

        // If Interps are increasing - must write!
        ((pGfxState->validBits.interpCount < m_numInterpolants)         ||

        // The PS Interpolants are NOT included in m_highFreqCtxRegHash!
        // For GFX12, SPI_VS_OUT_CONFIG and SPI_PS_IN_CONTROL moved from context to persistent state regs to help avoid
        // context rolls. In order to get the full benefit of this change, we keep track of the PS Interpolants state
        // on the cmdbuffer and manually compare what this pipeline wants to set to the known state to allow filtering
        // in cases where the previous pipeline and this pipeline do not have the same count of interpolants.
        (memcmp(m_highFreqRegs.spiPsInputCntl, pGfxState->psInterpolants, sizeof(uint32) * m_numInterpolants) != 0)))
    {
        writeHighFreq = true;

        pCmdSpace = CmdStream::WriteSetSeqContextRegs(mmSPI_PS_IN_CONTROL,
                                                      mmSPI_PS_INPUT_CNTL_0 + m_numInterpolants - 1,
                                                      &(m_highFreqRegs.spiPsInControl.u32All),
                                                      pCmdSpace);

        // Update hash in cmdbuffer
        pGfxState->pipelineCtxHighPktHash = m_highFreqCtxRegHash;

        // Update cmdBuffer copy of the current PS Interpolant state
        pGfxState->validBits.interpCount = Max(pGfxState->validBits.interpCount, m_numInterpolants);
        memcpy(pGfxState->psInterpolants, m_highFreqRegs.spiPsInputCntl, m_numInterpolants * sizeof(uint32));
    }

    if (writeHighFreq || writeMedFreq || writeLowFreq)
    {
        static_assert(MedFreq::Exist(mmVGT_SHADER_STAGES_EN) &&
            (MedFreq::Index(mmVGT_TF_PARAM)           == MedFreq::FirstContextIdx() + MedFreq::NumContext() - 4) &&
            (MedFreq::Index(mmVGT_LS_HS_CONFIG)       == MedFreq::FirstContextIdx() + MedFreq::NumContext() - 3) &&
            (MedFreq::Index(mmVGT_HOS_MAX_TESS_LEVEL) == MedFreq::FirstContextIdx() + MedFreq::NumContext() - 2) &&
            (MedFreq::Index(mmVGT_HOS_MIN_TESS_LEVEL) == MedFreq::FirstContextIdx() + MedFreq::NumContext() - 1),
            "Tess Reg optimization relies on the above assumptions!");

        const bool   isTess             =
            (MedFreq::GetC<mmVGT_SHADER_STAGES_EN, VGT_SHADER_STAGES_EN>(m_medFreqRegs).bits.HS_EN != 0);
        const uint32 numLowFreqCtxRegs  = (writeLowFreq  ? LowFreq::NumContext()  : 0);
        const uint32 numMedFreqCtxRegs  = (writeMedFreq  ?
            ((isTess || ((filterFlags & Gfx12RedundantStateFilterPipelineCtxTessRegsWhenTessIsOff) == 0)) ?
                MedFreq::NumContext() : MedFreq::NumContext() - 4) : 0);
        const uint32 numHighFreqCtxRegs = (writeHighFreq ? HighFreq::NumContext() : 0);
        const uint32 totalCtxRegs       = numLowFreqCtxRegs + numMedFreqCtxRegs + numHighFreqCtxRegs;

        pCmdSpace = CmdStream::WriteSetContextPairGroups(pCmdSpace,
                                                         totalCtxRegs,
                                                         &m_lowFreqRegs[LowFreq::FirstContextIdx()],
                                                         numLowFreqCtxRegs,
                                                         &m_medFreqRegs[MedFreq::FirstContextIdx()],
                                                         numMedFreqCtxRegs,
                                                         &m_highFreqRegs.pairs[HighFreq::FirstContextIdx()],
                                                         numHighFreqCtxRegs);

        const uint32 numLowFreqUcRegs  = (writeLowFreq  ? LowFreq::NumOther()  : 0);
        const uint32 numMedFreqUcRegs  = (writeMedFreq  ? MedFreq::NumOther()  : 0);
        const uint32 numHighFreqUcRegs = (writeHighFreq ? HighFreq::NumOther() : 0);
        const uint32 totalUcRegs       = numLowFreqUcRegs + numMedFreqUcRegs + numHighFreqUcRegs;
        pCmdSpace = CmdStream::WriteSetUConfigPairGroups(pCmdSpace,
                                                         totalUcRegs,
                                                         &m_lowFreqRegs[LowFreq::FirstOtherIdx()],
                                                         numLowFreqUcRegs,
                                                         &m_medFreqRegs[MedFreq::FirstOtherIdx()],
                                                         numMedFreqUcRegs,
                                                         &m_highFreqRegs.pairs[HighFreq::FirstOtherIdx()],
                                                         numHighFreqUcRegs);
    }

    bool ctxHighHashIsValid = true;
    bool ctxMedHashIsValid  = true;
    bool ctxLowHashIsValid  = true;

    // Init based on create-time value - OverrideDynamicState may override this below!
    pGfxState->cbTargetMask.u32All = GetColorWriteMask();
    *pDepthClampMode               = GetDepthClampMode();
    pGfxState->pipelinePsHash      = GetInfo().shader[static_cast<uint32>(ShaderType::Pixel)].hash;

    PAL_ASSERT(pPaClClipCntl != nullptr);

    // Check if any dynamic state is enabled.
    if (dynamicGraphicsInfo.enable.u32All != 0)
    {
        RegisterValuePair dynCtxRegs[DynamicStateOverrideCtx::Size()];
        RegisterValuePair depthOnlyOptCtxRegs[DepthOnlyOptRegsCtx::Size()];

        uint32 numDepthOnlyCtxRegs = 0;

        // Copy immutable copy from init-time to local copy.
        dynCtxRegs[DynamicStateOverrideCtx::Index(mmVGT_TF_PARAM)] =
            m_medFreqRegs[MedFreq::Index(mmVGT_TF_PARAM)];
        dynCtxRegs[DynamicStateOverrideCtx::Index(mmPA_CL_CLIP_CNTL)] =
            m_medFreqRegs[MedFreq::Index(mmPA_CL_CLIP_CNTL)];
        dynCtxRegs[DynamicStateOverrideCtx::Index(mmPA_SC_LINE_CNTL)] =
            m_medFreqRegs[MedFreq::Index(mmPA_SC_LINE_CNTL)];
        dynCtxRegs[DynamicStateOverrideCtx::Index(mmDB_VIEWPORT_CONTROL)] =
            m_medFreqRegs[MedFreq::Index(mmDB_VIEWPORT_CONTROL)];
        dynCtxRegs[DynamicStateOverrideCtx::Index(mmCB_TARGET_MASK)] =
            m_highFreqRegs.pairs[HighFreq::Index(mmCB_TARGET_MASK)];
        dynCtxRegs[DynamicStateOverrideCtx::Index(mmCB_COLOR_CONTROL)] =
            m_highFreqRegs.pairs[HighFreq::Index(mmCB_COLOR_CONTROL)];
        dynCtxRegs[DynamicStateOverrideCtx::Index(mmDB_SHADER_CONTROL)] =
            m_highFreqRegs.pairs[HighFreq::Index(mmDB_SHADER_CONTROL)];

        // Override any necessary fields for dynamic info.
        OverrideDynamicState(dynamicGraphicsInfo,
                             dynCtxRegs,
                             &pGfxState->cbTargetMask.u32All,
                             pDepthClampMode);

        pPaClClipCntl->u32All = dynCtxRegs[DynamicStateOverrideCtx::Index(mmPA_CL_CLIP_CNTL)].value;

        // If this pipeline is a candidate for depth only opt but the dynamic state made us disable it, then restore
        // some register values changed at init-time. Values stored in the object for candidate pipelines are set
        // such that the optimization is enabled as that is the common scenario.
        if ((m_depthOnlyOptMetadata.isCandidate) &&
            (CanRbPlusOptimizeDepthOnly(&dynamicGraphicsInfo) == false))
        {
            // Setup pairs offsets
            DepthOnlyOptRegsCtx::Init(depthOnlyOptCtxRegs);

            auto* pSxDownConvert      =
                DepthOnlyOptRegsCtx::Get<mmSX_PS_DOWNCONVERT, SX_PS_DOWNCONVERT>(depthOnlyOptCtxRegs);
            auto* pSpiShaderColFormat =
                DepthOnlyOptRegsCtx::Get<mmSPI_SHADER_COL_FORMAT, SPI_SHADER_COL_FORMAT>(depthOnlyOptCtxRegs);

            // Initialize from immutable init-time copy which is assuming depth only opt is on.
            *pSxDownConvert      = HighFreq::GetC<mmSX_PS_DOWNCONVERT, SX_PS_DOWNCONVERT>(m_highFreqRegs.pairs);
            *pSpiShaderColFormat = m_highFreqRegs.spiShaderColFormat;

            // Roll back these fields to the values associated with the optimization being disabled.
            pSxDownConvert->bits.MRT0                    = m_depthOnlyOptMetadata.origSxDownConvertMrt0;
            pSpiShaderColFormat->bits.COL0_EXPORT_FORMAT = m_depthOnlyOptMetadata.origSpiShaderCol0Format;

            numDepthOnlyCtxRegs = DepthOnlyOptRegsCtx::Size();
        }

        // Override the state we (possibly) wrote above. Since this is an uncommon path, we've decided
        // to take the CP overhead here instead of the CPU overhead for the common path.
        pCmdSpace = CmdStream::WriteSetContextPairGroups(pCmdSpace,
                                                         DynamicStateOverrideCtx::Size() + numDepthOnlyCtxRegs,
                                                         dynCtxRegs,
                                                         DynamicStateOverrideCtx::Size(),
                                                         depthOnlyOptCtxRegs,
                                                         numDepthOnlyCtxRegs);

        // Hashes are not valid since we changed context state!
        ctxMedHashIsValid  = false;
        ctxHighHashIsValid = false;
    }
    else
    {
        pPaClClipCntl->u32All = m_medFreqRegs[MedFreq::Index(mmPA_CL_CLIP_CNTL)].value;
    }

    // Check if we need to update state for RPM
    if ((targetIndex != UINT_MAX) &&
        ((targetIndex != 0) || (TargetFormats()[targetIndex].format != swizzledFormat.format)))
    {
        pCmdSpace = UpdateMrtSlotAndRbPlusFormatState(swizzledFormat,
                                                      targetIndex,
                                                      &pGfxState->cbTargetMask,
                                                      pCmdSpace);

        // Hash is not valid for future binds since we changed state!
        ctxHighHashIsValid = false;
    }

    // The hashes we stored in the command buffer are invalid if we skipped filtering because our
    // pre-determined hashes did not represent the state we actually wrote out.
    pGfxState->validBits.pipelineCtxHighHash = ctxHighHashIsValid;
    pGfxState->validBits.pipelineCtxMedHash  = ctxMedHashIsValid;
    pGfxState->validBits.pipelineCtxLowHash  = ctxLowHashIsValid;

    return pCmdSpace;
}

// =====================================================================================================================
// Initializes HW-specific state related to this graphics pipeline (register values, user-data mapping, etc.) using the
// specified Pipeline ABI processor and create info.
Result GraphicsPipeline::HwlInit(
    const GraphicsPipelineCreateInfo& createInfo,
    const AbiReader&                  abiReader,
    const PalAbi::CodeObjectMetadata& metadata,
    Util::MsgPackReader*              pMetadataReader)
{
    const Device* pDevice  = static_cast<const Device*>(m_pDevice->GetGfxDevice());
    const auto&   settings = pDevice->Settings();

    CodeObjectUploader uploader(m_pDevice, abiReader);

    const GpuHeap heap = IsInternal() ? GpuHeapLocal : m_pDevice->GetPublicSettings()->pipelinePreferredHeap;
    Result result = PerformRelocationsAndUploadToGpuMemory(metadata, heap, &uploader);

    // Set up user-data layout first because it may be needed by subsequent Init calls.
    if (result == Result::Success)
    {
        result = GraphicsUserDataLayout::Create(*m_pDevice, metadata.pipeline, &m_pUserDataLayout);

        // We do not expect MeshShaders to have Vertex or Instance Offset mapped.
        PAL_ASSERT((result == Result::Success) &&
                   ((HasMeshShader() == false) ||
                    ((m_pUserDataLayout->GetVertexBase().u32All   == UserDataNotMapped) &&
                     (m_pUserDataLayout->GetInstanceBase().u32All == UserDataNotMapped))));
    }

    if (result == Result::Success)
    {
        m_numInterpolants = metadata.pipeline.numInterpolants;

        if (metadata.pipeline.hasEntry.streamoutVertexStrides)
        {
            static_assert(sizeof(m_strmoutVtxStride) == sizeof(metadata.pipeline.streamoutVertexStrides),
                "Unexpected mismatch of size.");

            memcpy(m_strmoutVtxStride,
                   metadata.pipeline.streamoutVertexStrides,
                   sizeof(m_strmoutVtxStride));
        }

        if (createInfo.groupLaunchGuarantee != TriState::Disable)
        {
            m_disableGroupLaunchGuarantee = false;
        }

        m_noForceReZ = createInfo.noForceReZ;

        const bool isTess = (metadata.pipeline.graphicsRegister.vgtShaderStagesEn.flags.hsStageEn != 0);
        if (isTess)
        {
            result = InitHullShaderState(metadata, uploader, abiReader);
        }
    }

    if (result == Result::Success)
    {
        result = InitGeometryShaderState(metadata, uploader, abiReader, settings.gfx12GsWaveThrottleCntl);
    }

    if (result == Result::Success)
    {
        result = InitPixelShaderState(metadata, uploader, abiReader);
    }

    if (result == Result::Success)
    {
        result = InitDerivedState(createInfo, metadata, uploader, abiReader);
    }

    if (result == Result::Success)
    {
        InitPixelInterpolants(metadata);
        InitContextState(metadata);
        UpdateContextState(createInfo);
        InitGeCntl(metadata); // This must come after InitContextState/UpdateContextState!

        HandleWorkarounds(); // This must come after any register initialization!
    }

    if (result == Result::Success)
    {
        result = uploader.End(&m_uploadFenceToken);
    }

    if (result == Result::Success)
    {
        UpdateRingSizes(metadata);
        UpdateBinningStatus();
    }

    if (result == Result::Success)
    {
        GenerateHashes();
    }

    if (pDevice->CoreSettings().pipelinePrefetchEnable &&
        (settings.shaderPrefetchMethodGfx != PrefetchDisabled))
    {
        m_prefetch[0].gpuVirtAddr         = uploader.PrefetchAddr();
        m_prefetch[0].size                = uploader.PrefetchSize();
        m_prefetch[0].usageMask           = CoherShaderRead;
        m_prefetch[0].addrTranslationOnly = (settings.shaderPrefetchMethodGfx == PrefetchPrimeUtcL2);
        m_prefetchRangeCount              = 1;
    }

    return result;
}

// =====================================================================================================================
void GraphicsPipeline::GenerateHashes()
{
    MetroHash64     hasher;
    MetroHash::Hash hash = {};

    hasher.Update(m_lowFreqRegs);
    hasher.Finalize(hash.bytes);
    m_lowFreqCtxRegHash = MetroHash::Compact64(&hash);

    hasher.Update(m_medFreqRegs);
    hasher.Finalize(hash.bytes);
    m_medFreqCtxRegHash = MetroHash::Compact64(&hash);

    hasher.Initialize();
    // PS Interpolants are NOT included!
    hasher.Update(reinterpret_cast<const uint8*>(&m_highFreqRegs),
                  offsetof(HighFreqRegs, spiPsInputCntl));
    hasher.Finalize(hash.bytes);
    m_highFreqCtxRegHash = MetroHash::Compact64(&hash);
}

// =====================================================================================================================
const ShaderStageInfo* GraphicsPipeline::GetShaderStageInfo(
    ShaderType shaderType
    ) const
{
    const ShaderStageInfo* pInfo = nullptr;

    switch (shaderType)
    {
    case ShaderType::Mesh:
        pInfo = (HasMeshShader() ? &m_gsStageInfo : nullptr);
        break;
    case ShaderType::Vertex:
        pInfo = (IsTessEnabled() ? &m_hsStageInfo : &m_gsStageInfo);
        break;
    case ShaderType::Hull:
        pInfo = (IsTessEnabled() ? &m_hsStageInfo : nullptr);
        break;
    case ShaderType::Domain:
        pInfo = (IsTessEnabled() ? &m_gsStageInfo : nullptr);
        break;
    case ShaderType::Geometry:
        pInfo = (IsGsEnabled() ? &m_gsStageInfo : nullptr);
        break;
    case ShaderType::Pixel:
        pInfo = &m_psStageInfo;
        break;
    default:
        break;
    }

    return pInfo;
}

// =====================================================================================================================
// Internal function used to obtain shader stats using the given shader mem image.
Result GraphicsPipeline::GetShaderStats(
    ShaderType   shaderType,
    ShaderStats* pShaderStats,
    bool         getDisassemblySize
    ) const
{
    const GpuChipProperties& chipProps = m_pDevice->ChipProperties();

    PAL_ASSERT(pShaderStats != nullptr);
    Result result = Result::ErrorUnavailable;

    const ShaderStageInfo*const pStageInfo = GetShaderStageInfo(shaderType);

    if (pStageInfo != nullptr)
    {
        result = GetShaderStatsForStage(shaderType, *pStageInfo, nullptr, pShaderStats);

        if (result == Result::Success)
        {
            pShaderStats->shaderStageMask = (1 << static_cast<uint32>(shaderType));
            pShaderStats->palShaderHash   = m_info.shader[static_cast<uint32>(shaderType)].hash;
            pShaderStats->shaderOperations.writesUAV =
                m_shaderMetaData.flags[static_cast<uint32>(shaderType)].writesUav;

            pShaderStats->common.ldsSizePerThreadGroup = chipProps.gfxip.ldsSizePerThreadGroup;

            switch (pStageInfo->stageId)
            {
            case Abi::HardwareStage::Hs:
                pShaderStats->shaderStageMask       = (ApiShaderStageHull | ApiShaderStageVertex);
                pShaderStats->common.gpuVirtAddress = GetOriginalAddress(
                    HullShader::GetC<mmSPI_SHADER_PGM_LO_LS, SPI_SHADER_PGM_LO_LS>(m_hullShaderRegs).bits.MEM_BASE, 0);
                break;
            case Abi::HardwareStage::Gs:
                pShaderStats->shaderStageMask       = (IsTessEnabled() ? ApiShaderStageDomain : ApiShaderStageVertex);
                if (IsGsEnabled())
                {
                    pShaderStats->shaderStageMask  |= ApiShaderStageGeometry;
                }

                if (HasMeshShader())
                {
                    pShaderStats->shaderStageMask  |= ApiShaderStageMesh;
                }

                pShaderStats->common.gpuVirtAddress = GetOriginalAddress(
                    GeomShader::GetC<mmSPI_SHADER_PGM_LO_ES, SPI_SHADER_PGM_LO_ES>(m_geomShaderRegs).bits.MEM_BASE, 0);
                break;
            case Abi::HardwareStage::Ps:
                pShaderStats->shaderStageMask       = ApiShaderStagePixel;
                pShaderStats->common.gpuVirtAddress = GetOriginalAddress(
                    PixelShader::GetC<mmSPI_SHADER_PGM_LO_PS, SPI_SHADER_PGM_LO_PS>(m_pixelShaderRegs).bits.MEM_BASE, 0);
                break;
            default:
                break;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Initialized graphics pipeline hull shader state.
Result GraphicsPipeline::InitHullShaderState(
    const PalAbi::CodeObjectMetadata& metadata,
    const CodeObjectUploader&         uploader,
    const AbiReader&                  abiReader)
{
    const Device* pDevice = static_cast<const Device*>(m_pDevice->GetGfxDevice());

    m_hsStageInfo.stageId = Abi::HardwareStage::Hs;

    GpuSymbol symbol = { };
    Result result = uploader.GetGpuSymbol(Abi::PipelineSymbolType::HsMainEntry, &symbol);

    if (result == Result::Success)
    {
        m_hsStageInfo.codeLength     = static_cast<size_t>(symbol.size);
    }

    const Elf::SymbolTableEntry* pElfSymbol = abiReader.GetSymbolHeader(Abi::PipelineSymbolType::HsDisassembly);
    if (pElfSymbol != nullptr)
    {
        m_hsStageInfo.disassemblyLength = static_cast<size_t>(pElfSymbol->st_size);
    }

    auto* pRsrc4 = HullShader::Get<mmSPI_SHADER_PGM_RSRC4_HS, SPI_SHADER_PGM_RSRC4_HS>(m_hullShaderRegs);

    if (result == Result::Success)
    {
        PAL_ASSERT(IsPow2Aligned(symbol.gpuVirtAddr, 256));
        HullShader::Get<mmSPI_SHADER_PGM_LO_LS, SPI_SHADER_PGM_LO_LS>(m_hullShaderRegs)->bits.MEM_BASE =
            Get256BAddrLo(symbol.gpuVirtAddr);
        pRsrc4->bits.INST_PREF_SIZE = pDevice->GetShaderPrefetchSize(symbol.size);
    }

    if (uploader.GetGpuSymbol(Abi::PipelineSymbolType::HsShdrIntrlTblPtr, &symbol) == Result::Success)
    {
        HullShader::Get<mmSPI_SHADER_USER_DATA_HS_1, SPI_SHADER_USER_DATA_HS_1>(m_hullShaderRegs)->bits.DATA =
            LowPart(symbol.gpuVirtAddr);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Hs = static_cast<uint32>(Abi::HardwareStage::Hs);

        const auto& hwHs = metadata.pipeline.hardwareStage[Hs];

        HullShader::Get<mmSPI_SHADER_PGM_CHKSUM_HS, SPI_SHADER_PGM_CHKSUM_HS>(m_hullShaderRegs)->bits.CHECKSUM =
            hwHs.checksumValue;

        auto* pRsrc1 = HullShader::Get<mmSPI_SHADER_PGM_RSRC1_HS, SPI_SHADER_PGM_RSRC1_HS>(m_hullShaderRegs);
        pRsrc1->bits.VGPRS            = CalcNumVgprs(hwHs.vgprCount, (hwHs.wavefrontSize == 32));
        pRsrc1->bits.FLOAT_MODE       = hwHs.floatMode;
        pRsrc1->bits.WG_RR_EN         = hwHs.flags.wgRoundRobin;
        pRsrc1->bits.DEBUG_MODE       = hwHs.flags.debugMode;
        pRsrc1->bits.DISABLE_PERF     = 0; // TODO: Get from metadata.
        pRsrc1->bits.FWD_PROGRESS     = hwHs.flags.forwardProgress;
        pRsrc1->bits.WGP_MODE         = hwHs.flags.wgpMode;
        pRsrc1->bits.FP16_OVFL        = hwHs.flags.fp16Overflow;
        pRsrc1->bits.LS_VGPR_COMP_CNT = metadata.pipeline.graphicsRegister.lsVgprCompCnt;

        auto* pRsrc2 = HullShader::Get<mmSPI_SHADER_PGM_RSRC2_HS, SPI_SHADER_PGM_RSRC2_HS>(m_hullShaderRegs);
        pRsrc2->bits.SCRATCH_EN      = hwHs.flags.scratchEn;
        pRsrc2->bits.USER_SGPR       = hwHs.userSgprs & 0x1F;
        pRsrc2->bits.OC_LDS_EN       = hwHs.flags.offchipLdsEn;
        pRsrc2->bits.LDS_SIZE        = Pow2Align(hwHs.ldsSize >> 2, LdsDwGranularity) / LdsDwGranularity;
        pRsrc2->bits.USER_SGPR_MSB   = ((hwHs.userSgprs & 0x20) != 0);
        pRsrc2->bits.SHARED_VGPR_CNT = hwHs.sharedVgprCnt;

        pRsrc4->bits.WAVE_LIMIT        = Min(HsWaveLimitMax, hwHs.wavesPerSe);
        pRsrc4->bits.GLG_FORCE_DISABLE = m_disableGroupLaunchGuarantee;

        // PWS+ only support PreShader/PrePs waits if the IMAGE_OP bit is set. Theoretically we only set it for shaders
        // that do an image operation. However that would mean that our use of the pre-shader PWS+ wait is dependent
        // on us only waiting on image resources, which we don't know in our interface. For now always set the IMAGE_OP
        // bit for corresponding shaders, making the PreShader/PrePs waits global.
        pRsrc4->bits.IMAGE_OP = 1;
    }

    return result;
}

// =====================================================================================================================
// Initializes graphics pipeline geometry shader state.
Result GraphicsPipeline::InitGeometryShaderState(
    const PalAbi::CodeObjectMetadata& metadata,
    const CodeObjectUploader&         uploader,
    const AbiReader&                  abiReader,
    GsWaveThrottleCntl                waveThrottleCntl)
{
    const Device* pDevice = static_cast<const Device*>(m_pDevice->GetGfxDevice());

    m_gsStageInfo.stageId = Abi::HardwareStage::Gs;

    GpuSymbol symbol = { };
    Result result = uploader.GetGpuSymbol(Abi::PipelineSymbolType::GsMainEntry, &symbol);

    const auto& gfxReg = metadata.pipeline.graphicsRegister;
    const auto& hwGs   = metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Gs)];

    if (result == Result::Success)
    {
        m_gsStageInfo.codeLength = static_cast<size_t>(symbol.size);

        const Elf::SymbolTableEntry* pElfSymbol = abiReader.GetSymbolHeader(Abi::PipelineSymbolType::GsDisassembly);
        if (pElfSymbol != nullptr)
        {
            m_gsStageInfo.disassemblyLength = static_cast<size_t>(pElfSymbol->st_size);
        }

        PAL_ASSERT(IsPow2Aligned(symbol.gpuVirtAddr, 256));
        GeomShader::Get<mmSPI_SHADER_PGM_LO_ES, SPI_SHADER_PGM_LO_ES>(m_geomShaderRegs)->bits.MEM_BASE =
            Get256BAddrLo(symbol.gpuVirtAddr);

        if (uploader.GetGpuSymbol(Abi::PipelineSymbolType::GsShdrIntrlTblPtr, &symbol) == Result::Success)
        {
            GeomShader::Get<mmSPI_SHADER_USER_DATA_GS_1, SPI_SHADER_USER_DATA_GS_1>(m_geomShaderRegs)->bits.DATA =
                LowPart(symbol.gpuVirtAddr);
        }

        if (hwGs.hasEntry.checksumValue)
        {
            GeomShader::Get<mmSPI_SHADER_PGM_CHKSUM_GS, SPI_SHADER_PGM_CHKSUM_GS>(m_geomShaderRegs)->bits.CHECKSUM =
                hwGs.checksumValue;
        }

        m_esGsLdsSize.offset = m_pUserDataLayout->EsGsLdsSizeRegOffset();
        if (m_pUserDataLayout->EsGsLdsSizeRegOffset() != UserDataNotMapped)
        {
            m_esGsLdsSize.value = metadata.pipeline.esGsLdsSize;
        }

        auto* pRsrc1 = GeomShader::Get<mmSPI_SHADER_PGM_RSRC1_GS, SPI_SHADER_PGM_RSRC1_GS>(m_geomShaderRegs);
        pRsrc1->bits.VGPRS            = CalcNumVgprs(hwGs.vgprCount, (hwGs.wavefrontSize == 32));
        pRsrc1->bits.FLOAT_MODE       = hwGs.floatMode;
        pRsrc1->bits.WG_RR_EN         = hwGs.flags.wgRoundRobin;
        pRsrc1->bits.DEBUG_MODE       = hwGs.flags.debugMode;
        pRsrc1->bits.DISABLE_PERF     = 0; // TODO: Get from metadata.
        pRsrc1->bits.FWD_PROGRESS     = hwGs.flags.forwardProgress;
        pRsrc1->bits.WGP_MODE         = hwGs.flags.wgpMode;
        pRsrc1->bits.GS_VGPR_COMP_CNT = gfxReg.gsVgprCompCnt;
        pRsrc1->bits.FP16_OVFL        = hwGs.flags.fp16Overflow;
        pRsrc1->bits.CU_GROUP_ENABLE  = 0;

        auto* pRsrc2 = GeomShader::Get<mmSPI_SHADER_PGM_RSRC2_GS, SPI_SHADER_PGM_RSRC2_GS>(m_geomShaderRegs);
        pRsrc2->bits.SCRATCH_EN       = hwGs.flags.scratchEn;
        pRsrc2->bits.USER_SGPR        = hwGs.userSgprs & 0x1F;
        pRsrc2->bits.ES_VGPR_COMP_CNT = gfxReg.esVgprCompCnt;
        pRsrc2->bits.OC_LDS_EN        = hwGs.flags.offchipLdsEn;
        pRsrc2->bits.USER_SGPR_MSB    = ((hwGs.userSgprs & 0x20) != 0);
        pRsrc2->bits.SHARED_VGPR_CNT  = hwGs.sharedVgprCnt;
        pRsrc2->bits.LDS_SIZE         = Pow2Align(hwGs.ldsSize >> 2, LdsDwGranularity) / LdsDwGranularity;

        auto* pRsrc4 = GeomShader::Get<mmSPI_SHADER_PGM_RSRC4_GS, SPI_SHADER_PGM_RSRC4_GS>(m_geomShaderRegs);
        pRsrc4->bits.INST_PREF_SIZE           = pDevice->GetShaderPrefetchSize(symbol.size);
        pRsrc4->bits.WAVE_LIMIT               = Min(GsWaveLimitMax, hwGs.wavesPerSe);
        pRsrc4->bits.GLG_FORCE_DISABLE        = m_disableGroupLaunchGuarantee;
        pRsrc4->bits.PH_THROTTLE_EN           = (waveThrottleCntl & GsWaveThrottleCntlPhThrottleEn)  ? 1 : 0;
        pRsrc4->bits.SPI_THROTTLE_EN          = (waveThrottleCntl & GsWaveThrottleCntlSpiThrottleEn) ? 1 : 0;
        pRsrc4->bits.SPI_SHADER_LATE_ALLOC_GS = 127;
        // PWS+ only support PreShader/PrePs waits if the IMAGE_OP bit is set. Theoretically we only set it for shaders
        // that do an image operation. However that would mean that our use of the pre-shader PWS+ wait is dependent on
        // us only waiting on image resources, which we don't know in our interface. For now always set the IMAGE_OP bit
        // for corresponding shaders, making the PreShader/PrePs waits global.
        pRsrc4->bits.IMAGE_OP                 = 1;
    }

    if (gfxReg.hasEntry.spiVsOutConfig)
    {
        auto* pGsOutConfigPs =
            GeomShader::Get<mmSPI_SHADER_GS_OUT_CONFIG_PS, SPI_SHADER_GS_OUT_CONFIG_PS>(m_geomShaderRegs);
        pGsOutConfigPs->bits.VS_EXPORT_COUNT   = gfxReg.spiVsOutConfig.vsExportCount;
        pGsOutConfigPs->bits.NO_PC_EXPORT      = gfxReg.spiVsOutConfig.flags.noPcExport;
        pGsOutConfigPs->bits.PRIM_EXPORT_COUNT = gfxReg.spiVsOutConfig.primExportCount;
    }

    if (gfxReg.hasEntry.spiShaderGsMeshletDim)
    {
        auto* pGsMeshletDim = GeomShader::Get<mmSPI_SHADER_GS_MESHLET_DIM, SPI_SHADER_GS_MESHLET_DIM>(m_geomShaderRegs);
        pGsMeshletDim->bits.MESHLET_NUM_THREAD_X      = gfxReg.spiShaderGsMeshletDim.numThreadX;
        pGsMeshletDim->bits.MESHLET_NUM_THREAD_Y      = gfxReg.spiShaderGsMeshletDim.numThreadY;
        pGsMeshletDim->bits.MESHLET_NUM_THREAD_Z      = gfxReg.spiShaderGsMeshletDim.numThreadZ;
        pGsMeshletDim->bits.MESHLET_THREADGROUP_SIZE  = gfxReg.spiShaderGsMeshletDim.threadgroupSize;
    }

    if (gfxReg.hasEntry.spiShaderGsMeshletExpAlloc)
    {
        auto* pGsMeshletExpAlloc =
            GeomShader::Get<mmSPI_SHADER_GS_MESHLET_EXP_ALLOC, SPI_SHADER_GS_MESHLET_EXP_ALLOC>(m_geomShaderRegs);
        pGsMeshletExpAlloc->bits.MAX_EXP_VERTS = gfxReg.spiShaderGsMeshletExpAlloc.maxExpVerts;
        pGsMeshletExpAlloc->bits.MAX_EXP_PRIMS = gfxReg.spiShaderGsMeshletExpAlloc.maxExpPrims;
    }

    if (gfxReg.hasEntry.spiShaderGsMeshletCtrl)
    {
        auto* pGsMeshletCtrl =
            GeomShader::Get<mmSPI_SHADER_GS_MESHLET_CTRL, SPI_SHADER_GS_MESHLET_CTRL>(m_geomShaderRegs);
        pGsMeshletCtrl->bits.INTERLEAVE_BITS_X = gfxReg.spiShaderGsMeshletCtrl.interleaveBitsX;
        pGsMeshletCtrl->bits.INTERLEAVE_BITS_Y = gfxReg.spiShaderGsMeshletCtrl.interleaveBitsY;
    }

    if (gfxReg.hasEntry.spiPsInputCntl)
    {
        auto* pGsOutConfigPs =
            GeomShader::Get<mmSPI_SHADER_GS_OUT_CONFIG_PS, SPI_SHADER_GS_OUT_CONFIG_PS>(m_geomShaderRegs);
        pGsOutConfigPs->bits.NUM_INTERP      = gfxReg.spiPsInControl.numInterps;
        pGsOutConfigPs->bits.NUM_PRIM_INTERP = gfxReg.spiPsInControl.numPrimInterp;
    }

    return (result == Result::NotFound) && IsPartialPipeline() ? Result::Success : result;
}

// =====================================================================================================================
// Initializes graphics pipeline pixel shader state.
Result GraphicsPipeline::InitPixelShaderState(
    const PalAbi::CodeObjectMetadata& metadata,
    const CodeObjectUploader&         uploader,
    const AbiReader&                  abiReader)
{
    const PalAbi::GraphicsRegisterMetadata& gfxReg    = metadata.pipeline.graphicsRegister;
    const Device*                           pDevice   = static_cast<const Device*>(m_pDevice->GetGfxDevice());
    const GpuChipProperties&                chipProps = m_pDevice->ChipProperties();

    m_psStageInfo.stageId = Abi::HardwareStage::Ps;

    GpuSymbol symbol = { };
    Result    result = uploader.GetGpuSymbol(Abi::PipelineSymbolType::PsMainEntry, &symbol);

    if (result == Result::Success)
    {
        m_psStageInfo.codeLength     = static_cast<size_t>(symbol.size);
    }

    const Elf::SymbolTableEntry* pElfSymbol = abiReader.GetSymbolHeader(Abi::PipelineSymbolType::PsDisassembly);
    if (pElfSymbol != nullptr)
    {
        m_psStageInfo.disassemblyLength = static_cast<size_t>(pElfSymbol->st_size);
    }

    auto* pRsrc4 = PixelShader::Get<mmSPI_SHADER_PGM_RSRC4_PS, SPI_SHADER_PGM_RSRC4_PS>(m_pixelShaderRegs);

    if (result == Result::Success)
    {
        PAL_ASSERT(IsPow2Aligned(symbol.gpuVirtAddr, 256));
        PixelShader::Get<mmSPI_SHADER_PGM_LO_PS, SPI_SHADER_PGM_LO_PS>(m_pixelShaderRegs)->bits.MEM_BASE =
            Get256BAddrLo(symbol.gpuVirtAddr);
        pRsrc4->bits.INST_PREF_SIZE = pDevice->GetShaderPrefetchSize(symbol.size);
    }

    if (uploader.GetGpuSymbol(Abi::PipelineSymbolType::PsShdrIntrlTblPtr, &symbol) == Result::Success)
    {
        PixelShader::Get<mmSPI_SHADER_USER_DATA_PS_1, SPI_SHADER_USER_DATA_PS_1>(m_pixelShaderRegs)->bits.DATA =
            LowPart(symbol.gpuVirtAddr);
    }

    // PsColorExportEntry will always exist, while PsColorExportDualSourceEntry is always created.
    // So it needs to initialize the m_colorExportAddr[Default] and m_colorExportAddr[DualSourceBlendEnable]
    // with the same default value, then update m_colorExportAddr[DualSourceBlendEnable] if
    // PsColorExportDualSourceEntry created.
    if (uploader.GetGpuSymbol(Abi::PipelineSymbolType::PsColorExportEntry, &symbol) == Result::Success)
    {
        m_colorExportAddr[static_cast<uint32>(ColorExportShaderType::Default)] = symbol.gpuVirtAddr;
        m_colorExportAddr[static_cast<uint32>(ColorExportShaderType::DualSourceBlendEnable)] = LowPart(symbol.gpuVirtAddr);
    }

    if (uploader.GetGpuSymbol(Abi::PipelineSymbolType::PsColorExportDualSourceEntry, &symbol) ==
        Result::Success)
    {
        m_colorExportAddr[static_cast<uint32>(ColorExportShaderType::DualSourceBlendEnable)] = symbol.gpuVirtAddr;
    }

    if (result == Result::Success)
    {
        constexpr uint32 Ps = static_cast<uint32>(Abi::HardwareStage::Ps);
        const auto& hwPs = metadata.pipeline.hardwareStage[Ps];

        PixelShader::Get<mmSPI_SHADER_PGM_CHKSUM_PS, SPI_SHADER_PGM_CHKSUM_PS>(m_pixelShaderRegs)->bits.CHECKSUM =
            hwPs.checksumValue;

        auto* pRsrc1 = PixelShader::Get<mmSPI_SHADER_PGM_RSRC1_PS, SPI_SHADER_PGM_RSRC1_PS>(m_pixelShaderRegs);
        pRsrc1->bits.VGPRS              = CalcNumVgprs(hwPs.vgprCount, (hwPs.wavefrontSize == 32));
        pRsrc1->bits.FLOAT_MODE         = hwPs.floatMode;
        pRsrc1->bits.WG_RR_EN           = hwPs.flags.wgRoundRobin;
        pRsrc1->bits.DEBUG_MODE         = hwPs.flags.debugMode;
        pRsrc1->bits.DISABLE_PERF       = 0; // TODO: Get from metadata.
        pRsrc1->bits.FWD_PROGRESS       = hwPs.flags.forwardProgress;
        pRsrc1->bits.LOAD_PROVOKING_VTX = gfxReg.flags.psLoadProvokingVtx;
        pRsrc1->bits.FP16_OVFL          = hwPs.flags.fp16Overflow;
        pRsrc1->bits.CU_GROUP_DISABLE   = 0;

        auto* pRsrc2 = PixelShader::Get<mmSPI_SHADER_PGM_RSRC2_PS, SPI_SHADER_PGM_RSRC2_PS>(m_pixelShaderRegs);
        pRsrc2->bits.SCRATCH_EN               = hwPs.flags.scratchEn;
        pRsrc2->bits.USER_SGPR                = hwPs.userSgprs & 0x1F;
        pRsrc2->bits.WAVE_CNT_EN              = gfxReg.flags.psWaveCntEn;
        pRsrc2->bits.EXTRA_LDS_SIZE           = RoundUpQuotient(gfxReg.psExtraLdsSize, ExtraLdsSizeGranularity);
        pRsrc2->bits.LOAD_COLLISION_WAVEID    = gfxReg.paScShaderControl.flags.loadCollisionWaveid;
        pRsrc2->bits.LOAD_INTRAWAVE_COLLISION = gfxReg.paScShaderControl.flags.loadIntrawaveCollision;
        pRsrc2->bits.USER_SGPR_MSB            = ((hwPs.userSgprs & 0x20) != 0);
        pRsrc2->bits.SHARED_VGPR_CNT          = hwPs.sharedVgprCnt;

        const uint32 numPackerPerSe = chipProps.gfx9.numScPerSe * chipProps.gfx9.numPackerPerSc;
        PAL_ASSERT(numPackerPerSe != 0);

        // PS is programmed per packer per SE instead of just per SE like the other shader stages!
        pRsrc4->bits.WAVE_LIMIT     = Min(PsWaveLimitMax, (hwPs.wavesPerSe / numPackerPerSe));
        pRsrc4->bits.LDS_GROUP_SIZE = 1;

        // PWS+ only support PreShader/PrePs waits if the IMAGE_OP bit is set. Theoretically we only set it for shaders
        // that do an image operation. However that would mean that our use of the pre-shader PWS+ wait is dependent on
        // us only waiting on image resources, which we don't know in our interface. For now always set the IMAGE_OP bit
        // for corresponding shaders, making the PreShader/PrePs waits global.
        pRsrc4->bits.IMAGE_OP       = 1;
    }

    return (result == Result::NotFound) && IsPartialPipeline() ? Result::Success : result;
}

// =====================================================================================================================
// Initializes graphics pipeline pixel interpolants state.
void GraphicsPipeline::InitPixelInterpolants(
    const Util::PalAbi::CodeObjectMetadata& metadata)
{
    // ================================================================================================================
    // High Frequency State below.
    for (uint32 i = 0; i < m_numInterpolants; i++)
    {
        const auto& interpolant = metadata.pipeline.graphicsRegister.spiPsInputCntl[i];

        m_highFreqRegs.spiPsInputCntl[i].bits.OFFSET              = interpolant.offset;
        m_highFreqRegs.spiPsInputCntl[i].bits.DEFAULT_VAL         = interpolant.defaultVal;
        m_highFreqRegs.spiPsInputCntl[i].bits.FLAT_SHADE          = interpolant.flags.flatShade;
        m_highFreqRegs.spiPsInputCntl[i].bits.ROTATE_PC_PTR       = interpolant.flags.rotatePcPtr;
        m_highFreqRegs.spiPsInputCntl[i].bits.PRIM_ATTR           = interpolant.flags.primAttr;
        m_highFreqRegs.spiPsInputCntl[i].bits.PT_SPRITE_TEX       = interpolant.flags.ptSpriteTex;
        m_highFreqRegs.spiPsInputCntl[i].bits.DUP                 = 0;
        m_highFreqRegs.spiPsInputCntl[i].bits.FP16_INTERP_MODE    = interpolant.flags.fp16InterpMode;
        m_highFreqRegs.spiPsInputCntl[i].bits.USE_DEFAULT_ATTR1   = 0;
        m_highFreqRegs.spiPsInputCntl[i].bits.DEFAULT_VAL_ATTR1   = 0;
        m_highFreqRegs.spiPsInputCntl[i].bits.PT_SPRITE_TEX_ATTR1 = 0;
        m_highFreqRegs.spiPsInputCntl[i].bits.ATTR0_VALID         = interpolant.flags.attr0Valid;
        m_highFreqRegs.spiPsInputCntl[i].bits.ATTR1_VALID         = interpolant.flags.attr1Valid;
    }

    m_semanticCount = 0;
    if (metadata.pipeline.prerasterOutputSemantic[0].hasEntry.semantic)
    {
        for (uint32 i = 0; i < ArrayLen32(metadata.pipeline.prerasterOutputSemantic); i++)
        {
            if (metadata.pipeline.prerasterOutputSemantic[i].hasEntry.semantic)
            {
                m_semanticCount++;
                m_semanticInfo[i].semantic = metadata.pipeline.prerasterOutputSemantic[i].semantic;
                m_semanticInfo[i].index = metadata.pipeline.prerasterOutputSemantic[i].index;
            }
            else
            {
                break;
            }
        }
    }
    else if (metadata.pipeline.psInputSemantic[0].hasEntry.semantic)
    {
        m_semanticCount = m_numInterpolants;
        for (uint32 i = 0; i < m_semanticCount; i++)
        {
            m_semanticInfo[i].semantic = metadata.pipeline.psInputSemantic[i].semantic;
        }
    }
}

// =====================================================================================================================
// Initializes graphics pipeline context state.  Mostly corresponds to HW 8-state context registers and tends to
// correspond to fixed function hardware that interfaces with the shader core.
void GraphicsPipeline::InitContextState(
    const PalAbi::CodeObjectMetadata& metadata)
{
    const PalAbi::GraphicsRegisterMetadata& gfxReg = metadata.pipeline.graphicsRegister;

    m_highFreqRegs.spiShaderPosFormat.bits.POS0_EXPORT_FORMAT = gfxReg.spiShaderPosFormat[0];
    m_highFreqRegs.spiShaderPosFormat.bits.POS1_EXPORT_FORMAT = gfxReg.spiShaderPosFormat[1];
    m_highFreqRegs.spiShaderPosFormat.bits.POS2_EXPORT_FORMAT = gfxReg.spiShaderPosFormat[2];
    m_highFreqRegs.spiShaderPosFormat.bits.POS3_EXPORT_FORMAT = gfxReg.spiShaderPosFormat[3];
    m_highFreqRegs.spiShaderPosFormat.bits.POS4_EXPORT_FORMAT = gfxReg.spiShaderPosFormat[4];

    m_highFreqRegs.spiShaderZFormat.bits.Z_EXPORT_FORMAT = gfxReg.spiShaderZFormat;

    m_highFreqRegs.spiShaderColFormat.bits.COL0_EXPORT_FORMAT = gfxReg.spiShaderColFormat.col_0ExportFormat;
    m_highFreqRegs.spiShaderColFormat.bits.COL1_EXPORT_FORMAT = gfxReg.spiShaderColFormat.col_1ExportFormat;
    m_highFreqRegs.spiShaderColFormat.bits.COL2_EXPORT_FORMAT = gfxReg.spiShaderColFormat.col_2ExportFormat;
    m_highFreqRegs.spiShaderColFormat.bits.COL3_EXPORT_FORMAT = gfxReg.spiShaderColFormat.col_3ExportFormat;
    m_highFreqRegs.spiShaderColFormat.bits.COL4_EXPORT_FORMAT = gfxReg.spiShaderColFormat.col_4ExportFormat;
    m_highFreqRegs.spiShaderColFormat.bits.COL5_EXPORT_FORMAT = gfxReg.spiShaderColFormat.col_5ExportFormat;
    m_highFreqRegs.spiShaderColFormat.bits.COL6_EXPORT_FORMAT = gfxReg.spiShaderColFormat.col_6ExportFormat;
    m_highFreqRegs.spiShaderColFormat.bits.COL7_EXPORT_FORMAT = gfxReg.spiShaderColFormat.col_7ExportFormat;

    m_highFreqRegs.spiBarycCntl.bits.POS_FLOAT_LOCATION  = gfxReg.spiBarycCntl.posFloatLocation;
    m_highFreqRegs.spiBarycCntl.bits.FRONT_FACE_ALL_BITS = gfxReg.spiBarycCntl.flags.frontFaceAllBits;

    m_highFreqRegs.spiPsInputEna.bits.PERSP_SAMPLE_ENA          = gfxReg.spiPsInputEna.flags.perspSampleEna;
    m_highFreqRegs.spiPsInputEna.bits.PERSP_CENTER_ENA          = gfxReg.spiPsInputEna.flags.perspCenterEna;
    m_highFreqRegs.spiPsInputEna.bits.PERSP_CENTROID_ENA        = gfxReg.spiPsInputEna.flags.perspCentroidEna;
    m_highFreqRegs.spiPsInputEna.bits.PERSP_PULL_MODEL_ENA      = gfxReg.spiPsInputEna.flags.perspPullModelEna;
    m_highFreqRegs.spiPsInputEna.bits.LINEAR_SAMPLE_ENA         = gfxReg.spiPsInputEna.flags.linearSampleEna;
    m_highFreqRegs.spiPsInputEna.bits.LINEAR_CENTER_ENA         = gfxReg.spiPsInputEna.flags.linearCenterEna;
    m_highFreqRegs.spiPsInputEna.bits.LINEAR_CENTROID_ENA       = gfxReg.spiPsInputEna.flags.linearCentroidEna;
    m_highFreqRegs.spiPsInputEna.bits.LINE_STIPPLE_TEX_ENA      = gfxReg.spiPsInputEna.flags.lineStippleTexEna;
    m_highFreqRegs.spiPsInputEna.bits.POS_X_FLOAT_ENA           = gfxReg.spiPsInputEna.flags.posXFloatEna;
    m_highFreqRegs.spiPsInputEna.bits.POS_Y_FLOAT_ENA           = gfxReg.spiPsInputEna.flags.posYFloatEna;
    m_highFreqRegs.spiPsInputEna.bits.POS_Z_FLOAT_ENA           = gfxReg.spiPsInputEna.flags.posZFloatEna;
    m_highFreqRegs.spiPsInputEna.bits.POS_W_FLOAT_ENA           = gfxReg.spiPsInputEna.flags.posWFloatEna;
    m_highFreqRegs.spiPsInputEna.bits.FRONT_FACE_ENA            = gfxReg.spiPsInputEna.flags.frontFaceEna;
    m_highFreqRegs.spiPsInputEna.bits.ANCILLARY_ENA             = gfxReg.spiPsInputEna.flags.ancillaryEna;
    m_highFreqRegs.spiPsInputEna.bits.SAMPLE_COVERAGE_ENA       = gfxReg.spiPsInputEna.flags.sampleCoverageEna;
    m_highFreqRegs.spiPsInputEna.bits.POS_FIXED_PT_ENA          = gfxReg.spiPsInputEna.flags.posFixedPtEna;
    m_highFreqRegs.spiPsInputEna.bits.COVERAGE_TO_SHADER_SELECT = uint32(gfxReg.aaCoverageToShaderSelect);

    m_highFreqRegs.spiPsInputAddr.bits.PERSP_SAMPLE_ENA     = gfxReg.spiPsInputAddr.flags.perspSampleEna;
    m_highFreqRegs.spiPsInputAddr.bits.PERSP_CENTER_ENA     = gfxReg.spiPsInputAddr.flags.perspCenterEna;
    m_highFreqRegs.spiPsInputAddr.bits.PERSP_CENTROID_ENA   = gfxReg.spiPsInputAddr.flags.perspCentroidEna;
    m_highFreqRegs.spiPsInputAddr.bits.PERSP_PULL_MODEL_ENA = gfxReg.spiPsInputAddr.flags.perspPullModelEna;
    m_highFreqRegs.spiPsInputAddr.bits.LINEAR_SAMPLE_ENA    = gfxReg.spiPsInputAddr.flags.linearSampleEna;
    m_highFreqRegs.spiPsInputAddr.bits.LINEAR_CENTER_ENA    = gfxReg.spiPsInputAddr.flags.linearCenterEna;
    m_highFreqRegs.spiPsInputAddr.bits.LINEAR_CENTROID_ENA  = gfxReg.spiPsInputAddr.flags.linearCentroidEna;
    m_highFreqRegs.spiPsInputAddr.bits.LINE_STIPPLE_TEX_ENA = gfxReg.spiPsInputAddr.flags.lineStippleTexEna;
    m_highFreqRegs.spiPsInputAddr.bits.POS_X_FLOAT_ENA      = gfxReg.spiPsInputAddr.flags.posXFloatEna;
    m_highFreqRegs.spiPsInputAddr.bits.POS_Y_FLOAT_ENA      = gfxReg.spiPsInputAddr.flags.posYFloatEna;
    m_highFreqRegs.spiPsInputAddr.bits.POS_Z_FLOAT_ENA      = gfxReg.spiPsInputAddr.flags.posZFloatEna;
    m_highFreqRegs.spiPsInputAddr.bits.POS_W_FLOAT_ENA      = gfxReg.spiPsInputAddr.flags.posWFloatEna;
    m_highFreqRegs.spiPsInputAddr.bits.FRONT_FACE_ENA       = gfxReg.spiPsInputAddr.flags.frontFaceEna;
    m_highFreqRegs.spiPsInputAddr.bits.ANCILLARY_ENA        = gfxReg.spiPsInputAddr.flags.ancillaryEna;
    m_highFreqRegs.spiPsInputAddr.bits.SAMPLE_COVERAGE_ENA  = gfxReg.spiPsInputAddr.flags.sampleCoverageEna;
    m_highFreqRegs.spiPsInputAddr.bits.POS_FIXED_PT_ENA     = gfxReg.spiPsInputAddr.flags.posFixedPtEna;

    auto* pDbShaderControl = HighFreq::Get<mmDB_SHADER_CONTROL, DB_SHADER_CONTROL>(m_highFreqRegs.pairs);
    pDbShaderControl->bits.Z_EXPORT_ENABLE                  = gfxReg.dbShaderControl.flags.zExportEnable;
    pDbShaderControl->bits.STENCIL_TEST_VAL_EXPORT_ENABLE   = gfxReg.dbShaderControl.flags.stencilTestValExportEnable;
    pDbShaderControl->bits.STENCIL_OP_VAL_EXPORT_ENABLE     = gfxReg.dbShaderControl.flags.stencilOpValExportEnable;
    pDbShaderControl->bits.Z_ORDER                          = gfxReg.dbShaderControl.zOrder;
    pDbShaderControl->bits.KILL_ENABLE                      = gfxReg.dbShaderControl.flags.killEnable;
    pDbShaderControl->bits.COVERAGE_TO_MASK_ENABLE          = gfxReg.dbShaderControl.flags.coverageToMaskEn;
    pDbShaderControl->bits.MASK_EXPORT_ENABLE               = gfxReg.dbShaderControl.flags.maskExportEnable;
    pDbShaderControl->bits.EXEC_ON_HIER_FAIL                = gfxReg.dbShaderControl.flags.execOnHierFail;
    pDbShaderControl->bits.EXEC_ON_NOOP                     = gfxReg.dbShaderControl.flags.execOnNoop;
    pDbShaderControl->bits.ALPHA_TO_MASK_DISABLE            = gfxReg.dbShaderControl.flags.alphaToMaskDisable;
    pDbShaderControl->bits.DEPTH_BEFORE_SHADER              = gfxReg.dbShaderControl.flags.depthBeforeShader;
    pDbShaderControl->bits.CONSERVATIVE_Z_EXPORT            = gfxReg.dbShaderControl.conservativeZExport;
    pDbShaderControl->bits.DUAL_QUAD_DISABLE                = 0;
    pDbShaderControl->bits.PRIMITIVE_ORDERED_PIXEL_SHADER   = gfxReg.dbShaderControl.flags.primitiveOrderedPixelShader;
    pDbShaderControl->bits.PRE_SHADER_DEPTH_COVERAGE_ENABLE = gfxReg.dbShaderControl.flags.preShaderDepthCoverageEnable;
    pDbShaderControl->bits.OREO_BLEND_ENABLE                = 0;

    if (gfxReg.dbShaderControl.flags.primitiveOrderedPixelShader)
    {
        //    This must be enabled and OVERRIDE_INTRINSIC_RATE set to 0 (1xaa) in POPS mode
        //    with super-sampling disabled
        pDbShaderControl->bits.OVERRIDE_INTRINSIC_RATE_ENABLE = 1;
        pDbShaderControl->bits.OVERRIDE_INTRINSIC_RATE        = 0;

        // Mark POPS enablement.
        m_info.ps.flags.enablePops = 1;
    }

    m_highFreqRegs.spiShaderIdxFormat.bits.IDX0_EXPORT_FORMAT = gfxReg.spiShaderIdxFormat;

    auto* pCbShaderMask = HighFreq::Get<mmCB_SHADER_MASK, CB_SHADER_MASK>(m_highFreqRegs.pairs);
    pCbShaderMask->bits.OUTPUT0_ENABLE = gfxReg.cbShaderMask.output0Enable;
    pCbShaderMask->bits.OUTPUT1_ENABLE = gfxReg.cbShaderMask.output1Enable;
    pCbShaderMask->bits.OUTPUT2_ENABLE = gfxReg.cbShaderMask.output2Enable;
    pCbShaderMask->bits.OUTPUT3_ENABLE = gfxReg.cbShaderMask.output3Enable;
    pCbShaderMask->bits.OUTPUT4_ENABLE = gfxReg.cbShaderMask.output4Enable;
    pCbShaderMask->bits.OUTPUT5_ENABLE = gfxReg.cbShaderMask.output5Enable;
    pCbShaderMask->bits.OUTPUT6_ENABLE = gfxReg.cbShaderMask.output6Enable;
    pCbShaderMask->bits.OUTPUT7_ENABLE = gfxReg.cbShaderMask.output7Enable;

    m_highFreqRegs.spiInterpControl0.bits.PNT_SPRITE_ENA    = gfxReg.spiInterpControl.flags.pointSpriteEna;
    m_highFreqRegs.spiInterpControl0.bits.PNT_SPRITE_OVRD_X = uint32(gfxReg.spiInterpControl.pointSpriteOverrideX);
    m_highFreqRegs.spiInterpControl0.bits.PNT_SPRITE_OVRD_Y = uint32(gfxReg.spiInterpControl.pointSpriteOverrideY);
    m_highFreqRegs.spiInterpControl0.bits.PNT_SPRITE_OVRD_Z = uint32(gfxReg.spiInterpControl.pointSpriteOverrideZ);
    m_highFreqRegs.spiInterpControl0.bits.PNT_SPRITE_OVRD_W = uint32(gfxReg.spiInterpControl.pointSpriteOverrideW);

    m_highFreqRegs.spiPsInControl.bits.PARAM_GEN           = gfxReg.spiPsInControl.flags.paramGen;
    m_highFreqRegs.spiPsInControl.bits.BC_OPTIMIZE_DISABLE = gfxReg.spiPsInControl.flags.bcOptimizeDisable;
    m_highFreqRegs.spiPsInControl.bits.PS_W32_EN           =
        (metadata.pipeline.hardwareStage[uint32(Abi::HardwareStage::Ps)].wavefrontSize == 32);

    // High Frequency State above.
    // ================================================================================================================
    // Low Frequency State below.

    auto* pPaScShaderControl = MedFreq::Get<mmPA_SC_SHADER_CONTROL, PA_SC_SHADER_CONTROL>(m_medFreqRegs);
    pPaScShaderControl->bits.LOAD_COLLISION_WAVEID        = gfxReg.paScShaderControl.flags.loadCollisionWaveid;
    pPaScShaderControl->bits.LOAD_INTRAWAVE_COLLISION     = gfxReg.paScShaderControl.flags.loadIntrawaveCollision;
    pPaScShaderControl->bits.WAVE_BREAK_REGION_SIZE       = gfxReg.paScShaderControl.waveBreakRegionSize;
    pPaScShaderControl->bits.PS_ITER_SAMPLE               = gfxReg.flags.psIterSample;

    pPaScShaderControl->bits.REALIGN_DQUADS_AFTER_N_WAVES = 1;

    auto* pPaScHiSZControl = MedFreq::Get<mmPA_SC_HISZ_CONTROL, PA_SC_HISZ_CONTROL>(m_medFreqRegs);
    pPaScHiSZControl->bits.ROUND                 = m_hiZRoundVal;
    pPaScHiSZControl->bits.CONSERVATIVE_Z_EXPORT = gfxReg.dbShaderControl.conservativeZExport;

    auto* pPaClVsOutCntl = MedFreq::Get<mmPA_CL_VS_OUT_CNTL, PA_CL_VS_OUT_CNTL>(m_medFreqRegs);
    pPaClVsOutCntl->bits.CLIP_DIST_ENA_0            = gfxReg.paClVsOutCntl.flags.clipDistEna_0;
    pPaClVsOutCntl->bits.CLIP_DIST_ENA_1            = gfxReg.paClVsOutCntl.flags.clipDistEna_1;
    pPaClVsOutCntl->bits.CLIP_DIST_ENA_2            = gfxReg.paClVsOutCntl.flags.clipDistEna_2;
    pPaClVsOutCntl->bits.CLIP_DIST_ENA_3            = gfxReg.paClVsOutCntl.flags.clipDistEna_3;
    pPaClVsOutCntl->bits.CLIP_DIST_ENA_4            = gfxReg.paClVsOutCntl.flags.clipDistEna_4;
    pPaClVsOutCntl->bits.CLIP_DIST_ENA_5            = gfxReg.paClVsOutCntl.flags.clipDistEna_5;
    pPaClVsOutCntl->bits.CLIP_DIST_ENA_6            = gfxReg.paClVsOutCntl.flags.clipDistEna_6;
    pPaClVsOutCntl->bits.CLIP_DIST_ENA_7            = gfxReg.paClVsOutCntl.flags.clipDistEna_7;
    pPaClVsOutCntl->bits.CULL_DIST_ENA_0            = gfxReg.paClVsOutCntl.flags.cullDistEna_0;
    pPaClVsOutCntl->bits.CULL_DIST_ENA_1            = gfxReg.paClVsOutCntl.flags.cullDistEna_1;
    pPaClVsOutCntl->bits.CULL_DIST_ENA_2            = gfxReg.paClVsOutCntl.flags.cullDistEna_2;
    pPaClVsOutCntl->bits.CULL_DIST_ENA_3            = gfxReg.paClVsOutCntl.flags.cullDistEna_3;
    pPaClVsOutCntl->bits.CULL_DIST_ENA_4            = gfxReg.paClVsOutCntl.flags.cullDistEna_4;
    pPaClVsOutCntl->bits.CULL_DIST_ENA_5            = gfxReg.paClVsOutCntl.flags.cullDistEna_5;
    pPaClVsOutCntl->bits.CULL_DIST_ENA_6            = gfxReg.paClVsOutCntl.flags.cullDistEna_6;
    pPaClVsOutCntl->bits.CULL_DIST_ENA_7            = gfxReg.paClVsOutCntl.flags.cullDistEna_7;
    pPaClVsOutCntl->bits.USE_VTX_POINT_SIZE         = gfxReg.paClVsOutCntl.flags.useVtxPointSize;
    pPaClVsOutCntl->bits.USE_VTX_EDGE_FLAG          = gfxReg.paClVsOutCntl.flags.useVtxEdgeFlag;
    pPaClVsOutCntl->bits.USE_VTX_RENDER_TARGET_INDX = gfxReg.paClVsOutCntl.flags.useVtxRenderTargetIndx;
    pPaClVsOutCntl->bits.USE_VTX_VIEWPORT_INDX      = gfxReg.paClVsOutCntl.flags.useVtxViewportIndx;
    pPaClVsOutCntl->bits.USE_VTX_KILL_FLAG          = gfxReg.paClVsOutCntl.flags.useVtxKillFlag;
    pPaClVsOutCntl->bits.VS_OUT_MISC_VEC_ENA        = gfxReg.paClVsOutCntl.flags.vsOutMiscVecEna;
    pPaClVsOutCntl->bits.VS_OUT_CCDIST0_VEC_ENA     = gfxReg.paClVsOutCntl.flags.vsOutCcDist0VecEna;
    pPaClVsOutCntl->bits.VS_OUT_CCDIST1_VEC_ENA     = gfxReg.paClVsOutCntl.flags.vsOutCcDist1VecEna;
    pPaClVsOutCntl->bits.VS_OUT_MISC_SIDE_BUS_ENA   = gfxReg.paClVsOutCntl.flags.vsOutMiscSideBusEna;
    pPaClVsOutCntl->bits.USE_VTX_LINE_WIDTH         = gfxReg.paClVsOutCntl.flags.useVtxLineWidth;
    pPaClVsOutCntl->bits.USE_VTX_VRS_RATE           = gfxReg.paClVsOutCntl.flags.useVtxVrsRate;
    pPaClVsOutCntl->bits.BYPASS_VTX_RATE_COMBINER   = gfxReg.paClVsOutCntl.flags.bypassVtxRateCombiner;
    pPaClVsOutCntl->bits.BYPASS_PRIM_RATE_COMBINER  = gfxReg.paClVsOutCntl.flags.bypassPrimRateCombiner;

    // Unlike our hardware, DX12 does not have separate vertex and primitive combiners.  A mesh shader is the only
    // shader that can export a primitive rate so if there is no mesh shader then we should bypass the prim rate
    // combiner.
    if (metadata.pipeline.shader[static_cast<uint32>(Abi::ApiShaderType::Mesh)].hasEntry.uAll != 0)
    {
        pPaClVsOutCntl->bits.BYPASS_VTX_RATE_COMBINER = 1;
    }
    else
    {
        pPaClVsOutCntl->bits.BYPASS_PRIM_RATE_COMBINER = 1;
    }

    auto* pVgtPrimitiveIdEn = LowFreq::Get<mmVGT_PRIMITIVEID_EN, VGT_PRIMITIVEID_EN>(m_lowFreqRegs);
    pVgtPrimitiveIdEn->bits.NGG_DISABLE_PROVOK_REUSE = gfxReg.flags.nggDisableProvokReuse;

    auto* pGeMaxOutputPerSubgroup =
        MedFreq::Get<mmGE_MAX_OUTPUT_PER_SUBGROUP, GE_MAX_OUTPUT_PER_SUBGROUP>(m_medFreqRegs);
    pGeMaxOutputPerSubgroup->bits.MAX_VERTS_PER_SUBGROUP = gfxReg.maxVertsPerSubgroup;

    auto* pGeNggSubgrpCntl = LowFreq::Get<mmGE_NGG_SUBGRP_CNTL, GE_NGG_SUBGRP_CNTL>(m_lowFreqRegs);
    pGeNggSubgrpCntl->bits.PRIM_AMP_FACTOR = gfxReg.geNggSubgrpCntl.primAmpFactor;
    pGeNggSubgrpCntl->bits.THDS_PER_SUBGRP = gfxReg.geNggSubgrpCntl.threadsPerSubgroup;

    auto* pVgtGsMaxVertOut = MedFreq::Get<mmVGT_GS_MAX_VERT_OUT, VGT_GS_MAX_VERT_OUT>(m_medFreqRegs);
    pVgtGsMaxVertOut->bits.MAX_VERT_OUT = gfxReg.vgtGsMaxVertOut;

    auto* pVgtGsInstanceCnt = LowFreq::Get<mmVGT_GS_INSTANCE_CNT, VGT_GS_INSTANCE_CNT>(m_lowFreqRegs);
    pVgtGsInstanceCnt->bits.ENABLE                          = gfxReg.vgtGsInstanceCnt.flags.enable;
    pVgtGsInstanceCnt->bits.CNT                             = gfxReg.vgtGsInstanceCnt.count;
    pVgtGsInstanceCnt->bits.EN_MAX_VERT_OUT_PER_GS_INSTANCE = gfxReg.vgtGsInstanceCnt.flags.enMaxVertOutPerGsInstance;

    // This bit field has shrunk compared to legacy - ensure we haven't overflowed!
    PAL_ASSERT(pVgtGsInstanceCnt->bits.CNT == gfxReg.vgtGsInstanceCnt.count);

    auto* pVgtGsOutPrimType = MedFreq::Get<mmVGT_GS_OUT_PRIM_TYPE, VGT_GS_OUT_PRIM_TYPE>(m_medFreqRegs);
    pVgtGsOutPrimType->bits.OUTPRIM_TYPE = uint32(gfxReg.vgtGsOutPrimType.outprimType);

    auto* pVgtShaderStagesEn = MedFreq::Get<mmVGT_SHADER_STAGES_EN, VGT_SHADER_STAGES_EN>(m_medFreqRegs);
    pVgtShaderStagesEn->bits.HS_EN                   = gfxReg.vgtShaderStagesEn.flags.hsStageEn;
    pVgtShaderStagesEn->bits.GS_EN                   = gfxReg.vgtShaderStagesEn.flags.gsStageEn;
    pVgtShaderStagesEn->bits.GS_FAST_LAUNCH          = gfxReg.vgtShaderStagesEn.gsFastLaunch;
    pVgtShaderStagesEn->bits.HS_W32_EN               =
        (metadata.pipeline.hardwareStage[uint32(Abi::HardwareStage::Hs)].wavefrontSize == 32);
    pVgtShaderStagesEn->bits.GS_W32_EN               =
        (metadata.pipeline.hardwareStage[uint32(Abi::HardwareStage::Gs)].wavefrontSize == 32);
    pVgtShaderStagesEn->bits.NGG_WAVE_ID_EN          = gfxReg.vgtShaderStagesEn.flags.nggWaveIdEn;
    pVgtShaderStagesEn->bits.PRIMGEN_PASSTHRU_NO_MSG = gfxReg.vgtShaderStagesEn.flags.primgenPassthruNoMsg;

    PAL_ASSERT((gfxReg.vgtShaderStagesEn.vsStageEn           == 0) &&
               (gfxReg.vgtShaderStagesEn.flags.dynamicHs     == 0) &&
               (gfxReg.vgtShaderStagesEn.flags.orderedIdMode == 0) &&
               (gfxReg.vgtShaderStagesEn.gsFastLaunch        == pVgtShaderStagesEn->bits.GS_FAST_LAUNCH));

    auto* pVgtReuseOff = LowFreq::Get<mmVGT_REUSE_OFF, VGT_REUSE_OFF>(m_lowFreqRegs);
    pVgtReuseOff->bits.REUSE_OFF = gfxReg.flags.vgtReuseOff;

    auto* pVgtTfParam = MedFreq::Get<mmVGT_TF_PARAM, VGT_TF_PARAM>(m_medFreqRegs);
    if (gfxReg.vgtTfParam.hasEntry.uAll != 0)
    {
        pVgtTfParam->bits.TYPE              = gfxReg.vgtTfParam.type;
        pVgtTfParam->bits.PARTITIONING      = gfxReg.vgtTfParam.partitioning;
        pVgtTfParam->bits.TOPOLOGY          = gfxReg.vgtTfParam.topology;
        pVgtTfParam->bits.DISABLE_DONUTS    = gfxReg.vgtTfParam.flags.disableDonuts;
        pVgtTfParam->bits.TEMPORAL          = VGT_TEMPORAL_DISCARD;
        pVgtTfParam->bits.DISTRIBUTION_MODE = gfxReg.vgtTfParam.distributionMode;
        pVgtTfParam->bits.DETECT_ONE        = 0;
        pVgtTfParam->bits.DETECT_ZERO       = 0;
        pVgtTfParam->bits.MTYPE             = 0;
    }

    auto* pVgtDrawPayloadCntl = MedFreq::Get<mmVGT_DRAW_PAYLOAD_CNTL, VGT_DRAW_PAYLOAD_CNTL>(m_medFreqRegs);
    pVgtDrawPayloadCntl->bits.EN_PRIM_PAYLOAD = gfxReg.flags.vgtDrawPrimPayloadEn;
    pVgtDrawPayloadCntl->bits.EN_VRS_RATE     = 1;

    auto* pPaClClipCntl = MedFreq::Get<mmPA_CL_CLIP_CNTL, PA_CL_CLIP_CNTL>(m_medFreqRegs);
    pPaClClipCntl->bits.UCP_ENA_0                 = gfxReg.paClClipCntl.flags.userClipPlane0Ena;
    pPaClClipCntl->bits.UCP_ENA_1                 = gfxReg.paClClipCntl.flags.userClipPlane1Ena;
    pPaClClipCntl->bits.UCP_ENA_2                 = gfxReg.paClClipCntl.flags.userClipPlane2Ena;
    pPaClClipCntl->bits.UCP_ENA_3                 = gfxReg.paClClipCntl.flags.userClipPlane3Ena;
    pPaClClipCntl->bits.UCP_ENA_4                 = gfxReg.paClClipCntl.flags.userClipPlane4Ena;
    pPaClClipCntl->bits.UCP_ENA_5                 = gfxReg.paClClipCntl.flags.userClipPlane5Ena;
    pPaClClipCntl->bits.PS_UCP_Y_SCALE_NEG        = 0;
    pPaClClipCntl->bits.PS_UCP_MODE               = 0;
    pPaClClipCntl->bits.CLIP_DISABLE              = gfxReg.paClClipCntl.flags.clipDisable;
    pPaClClipCntl->bits.UCP_CULL_ONLY_ENA         = 0;
    pPaClClipCntl->bits.BOUNDARY_EDGE_FLAG_ENA    = 0;
    pPaClClipCntl->bits.DIS_CLIP_ERR_DETECT       = 0;
    pPaClClipCntl->bits.VTX_KILL_OR               = 0;
    pPaClClipCntl->bits.DX_RASTERIZATION_KILL     = gfxReg.paClClipCntl.flags.rasterizationKill;
    pPaClClipCntl->bits.DX_LINEAR_ATTR_CLIP_ENA   = gfxReg.paClClipCntl.flags.dxLinearAttrClipEna;
    pPaClClipCntl->bits.VTE_VPORT_PROVOKE_DISABLE = 0;
    pPaClClipCntl->bits.ZCLIP_NEAR_DISABLE        = gfxReg.paClClipCntl.flags.zclipNearDisable;
    pPaClClipCntl->bits.ZCLIP_FAR_DISABLE         = gfxReg.paClClipCntl.flags.zclipFarDisable;
    pPaClClipCntl->bits.ZCLIP_PROG_NEAR_ENA       = 0;

    auto* pPaSuVtxCntl = LowFreq::Get<mmPA_SU_VTX_CNTL, PA_SU_VTX_CNTL>(m_lowFreqRegs);
    pPaSuVtxCntl->bits.PIX_CENTER = gfxReg.paSuVtxCntl.flags.pixCenter;
    pPaSuVtxCntl->bits.ROUND_MODE = gfxReg.paSuVtxCntl.roundMode;
    pPaSuVtxCntl->bits.QUANT_MODE = gfxReg.paSuVtxCntl.quantMode;

    auto* pPaClVteCntl = LowFreq::Get<mmPA_CL_VTE_CNTL, PA_CL_VTE_CNTL>(m_lowFreqRegs);
    pPaClVteCntl->bits.VPORT_X_SCALE_ENA  = gfxReg.paClVteCntl.flags.xScaleEna;
    pPaClVteCntl->bits.VPORT_X_OFFSET_ENA = gfxReg.paClVteCntl.flags.xOffsetEna;
    pPaClVteCntl->bits.VPORT_Y_SCALE_ENA  = gfxReg.paClVteCntl.flags.yScaleEna;
    pPaClVteCntl->bits.VPORT_Y_OFFSET_ENA = gfxReg.paClVteCntl.flags.yOffsetEna;
    pPaClVteCntl->bits.VPORT_Z_SCALE_ENA  = gfxReg.paClVteCntl.flags.zScaleEna;
    pPaClVteCntl->bits.VPORT_Z_OFFSET_ENA = gfxReg.paClVteCntl.flags.zOffsetEna;
    pPaClVteCntl->bits.VTX_XY_FMT         = gfxReg.paClVteCntl.flags.vtxXyFmt;
    pPaClVteCntl->bits.VTX_Z_FMT          = gfxReg.paClVteCntl.flags.vtxZFmt;
    pPaClVteCntl->bits.VTX_W0_FMT         = gfxReg.paClVteCntl.flags.vtxW0Fmt;
    pPaClVteCntl->bits.PERFCOUNTER_REF    = 0;

    auto* pVgtLsHsConfig = MedFreq::Get<mmVGT_LS_HS_CONFIG, VGT_LS_HS_CONFIG>(m_medFreqRegs);
    pVgtLsHsConfig->bits.NUM_PATCHES      = gfxReg.vgtLsHsConfig.numPatches;
    pVgtLsHsConfig->bits.HS_NUM_OUTPUT_CP = gfxReg.vgtLsHsConfig.hsNumOutputCp;

    MedFreq::Get<mmVGT_HOS_MIN_TESS_LEVEL, VGT_HOS_MIN_TESS_LEVEL>(m_medFreqRegs)->f32All = gfxReg.vgtHosMinTessLevel;
    MedFreq::Get<mmVGT_HOS_MAX_TESS_LEVEL, VGT_HOS_MAX_TESS_LEVEL>(m_medFreqRegs)->f32All = gfxReg.vgtHosMaxTessLevel;

    InitGeCntl(metadata);
}

// =====================================================================================================================
// Initializes the graphics pipeline state related to various GE controls.
void GraphicsPipeline::InitGeCntl(
    const PalAbi::CodeObjectMetadata& metadata)
{
    const PalAbi::GraphicsRegisterMetadata& gfxReg = metadata.pipeline.graphicsRegister;

    const bool isNggFastLaunch = (gfxReg.vgtShaderStagesEn.gsFastLaunch != 0);
    const bool isTess = (gfxReg.vgtShaderStagesEn.flags.hsStageEn != 0);
    const bool nggSubgroupSize = metadata.pipeline.nggSubgroupSize;
    const bool disableVertGrouping = (isNggFastLaunch == false) && (nggSubgroupSize == 0);

    constexpr uint32 VertGroupingDisabled = 256;

    // There is no need for a separate path for tessellation.
    uint32 primsPerSubgrp = gfxReg.vgtGsOnchipCntl.gsPrimsPerSubgroup;
    uint32 vertsPerSubgrp = disableVertGrouping ? VertGroupingDisabled : gfxReg.vgtGsOnchipCntl.esVertsPerSubgroup;

    auto* pGeCntl = HighFreq::Get<mmGE_CNTL, GE_CNTL>(m_highFreqRegs.pairs);
    pGeCntl->bits.PRIMS_PER_SUBGRP     = primsPerSubgrp;
    pGeCntl->bits.VERTS_PER_SUBGRP     = vertsPerSubgrp;
    // We could try 256/primAmpFactor for GFX12 since PH FIFOs no longer exist.
    if (gfxReg.geNggSubgrpCntl.primAmpFactor > 0)
    {
        pGeCntl->bits.PRIM_GRP_SIZE = Clamp(uint32(256 / gfxReg.geNggSubgrpCntl.primAmpFactor), 1u, 256u);
    }
    else
    {
        PAL_ASSERT(IsPartialPipeline());
    }

    pGeCntl->bits.BREAK_PRIMGRP_AT_EOI = isTess;

}

// =====================================================================================================================
// Initializes graphics pipeline state related to color exports.
void GraphicsPipeline::UpdateColorExportState(
    const GraphicsPipelineCreateInfo& createInfo)
{
    CB_TARGET_MASK*    pCbTargetMask   = HighFreq::Get<mmCB_TARGET_MASK, CB_TARGET_MASK>(m_highFreqRegs.pairs);
    CB_COLOR_CONTROL*  pCbColorControl = HighFreq::Get<mmCB_COLOR_CONTROL, CB_COLOR_CONTROL>(m_highFreqRegs.pairs);
    CB_SHADER_MASK*    pCbShaderMask   = HighFreq::Get<mmCB_SHADER_MASK, CB_SHADER_MASK>(m_highFreqRegs.pairs);
    SX_PS_DOWNCONVERT* pSxDownConvert  = HighFreq::Get<mmSX_PS_DOWNCONVERT, SX_PS_DOWNCONVERT>(m_highFreqRegs.pairs);

    for (uint32 slot = 0; slot < MaxColorTargets; slot++)
    {
        // Each iteration of the loop loads values into MRT7 then they are shifted down.
        pCbTargetMask->u32All >>= 4;

        pCbTargetMask->bits.TARGET7_ENABLE = createInfo.cbState.target[slot].channelWriteMask;
    }

    PAL_ASSERT((IsFmaskDecompress() == false) && (IsResolveFixedFunc() == false));
    PAL_ASSERT((IsFastClearEliminate() == false) && (IsDccDecompress() == false));

    if ((pCbShaderMask->u32All == 0) || (pCbTargetMask->u32All == 0))
    {
        pCbColorControl->bits.MODE = CB_DISABLE;
    }
    else
    {
        pCbColorControl->bits.MODE = CB_NORMAL;
        pCbColorControl->bits.ROP3 = Rop3(createInfo.cbState.logicOp);
    }

    if (createInfo.cbState.dualSourceBlendEnable)
    {
        // Disable RB+ is dual source blending is enabled.
        pCbColorControl->bits.DISABLE_DUAL_QUAD = 1;

        // If dual-source blending is enabled and the PS doesn't export to both RT0 and RT1, the hardware might hang.
        // To avoid the hang, just disable CB writes.
        if ((pCbShaderMask->bits.OUTPUT0_ENABLE == 0) || (pCbShaderMask->bits.OUTPUT1_ENABLE == 0))
        {
            PAL_ALERT_ALWAYS();
            pCbColorControl->bits.MODE = CB_DISABLE;
        }
    }
    else
    {
        auto* pSxBlendOptEpsilon = HighFreq::Get<mmSX_BLEND_OPT_EPSILON, SX_BLEND_OPT_EPSILON>(m_highFreqRegs.pairs);
        auto* pSxBlendOptControl = HighFreq::Get<mmSX_BLEND_OPT_CONTROL, SX_BLEND_OPT_CONTROL>(m_highFreqRegs.pairs);

        for (uint32 slot = 0; slot < MaxColorTargets; slot++)
        {
            // Each iteration of the loop loads values into MRT7 then they are shifted down.
            pSxDownConvert->u32All     >>= 4;
            pSxBlendOptEpsilon->u32All >>= 4;
            pSxBlendOptControl->u32All >>= 4;

            const auto& targetInfo = createInfo.cbState.target[slot];

            const SX_DOWNCONVERT_FORMAT sxDownConvertFmt = SxDownConvertFormat(targetInfo.swizzledFormat);
            pSxDownConvert->bits.MRT7             = sxDownConvertFmt;
            pSxBlendOptEpsilon->bits.MRT7_EPSILON = SxBlendOptEpsilon(sxDownConvertFmt);

            // In order to determine if alpha or color channels are meaningful to the blender, the blend equations and
            // coefficients would need to be examined for any interdependency. Instead, rely on the SX optimization
            // result except for the trivial case where writes are disabled by the write mask.
            if (targetInfo.channelWriteMask == 0)
            {
                pSxBlendOptControl->bits.MRT7_COLOR_OPT_DISABLE = 1;
                pSxBlendOptControl->bits.MRT7_ALPHA_OPT_DISABLE = 1;
            }
        }
    }

    // Implement the "AfterPs" toss point by forcing the CB target mask to 0 regardless of the app programming.
    if ((IsInternal() == false) && (m_pDevice->Settings().tossPointMode == TossPointAfterPs))
    {
        // This toss point is used to disable all color buffer writes.
        pCbTargetMask->u32All = 0;
    }

    PAL_ASSERT(m_pDevice->ChipProperties().gfx9.rbPlus); ///< All known GFX12 chips are RB+.

    // Assume dynamic state is not used most of the time.
    if (CanRbPlusOptimizeDepthOnly(nullptr) &&
        m_pDevice->GetPublicSettings()->optDepthOnlyExportRate)
    {
        // Save these off incase we need to disable optDepthOnlyExportRate due to dynamic state.
        m_depthOnlyOptMetadata.isCandidate             = 1;
        m_depthOnlyOptMetadata.origSxDownConvertMrt0   = pSxDownConvert->bits.MRT0;
        m_depthOnlyOptMetadata.origSpiShaderCol0Format = m_highFreqRegs.spiShaderColFormat.bits.COL0_EXPORT_FORMAT;

        pSxDownConvert->bits.MRT0                                 = SX_RT_EXPORT_32_R;
        m_highFreqRegs.spiShaderColFormat.bits.COL0_EXPORT_FORMAT = SPI_SHADER_32_R;
    }
}

// =====================================================================================================================
// Initializes graphics pipeline state related to stereo rendering.
void GraphicsPipeline::UpdateStereoState(
    const GraphicsPipelineCreateInfo& createInfo)
{
    auto* pPaStereoCntl = LowFreq::Get<mmPA_STEREO_CNTL, PA_STEREO_CNTL>(m_lowFreqRegs);
    pPaStereoCntl->bits.STEREO_MODE     = 1;
    pPaStereoCntl->bits.RT_SLICE_MODE   = 0;
    pPaStereoCntl->bits.RT_SLICE_OFFSET = 0;
    pPaStereoCntl->bits.VP_ID_MODE      = 0;
    pPaStereoCntl->bits.VP_ID_OFFSET    = 0;

    auto* pGeStereoCntl = LowFreq::Get<mmGE_STEREO_CNTL, GE_STEREO_CNTL>(m_lowFreqRegs);
    pGeStereoCntl->u32All = 0;

    const auto& viewInstancingDesc = createInfo.viewInstancingDesc;
    if (viewInstancingDesc.viewInstanceCount > 1)
    {
        if (m_pUserDataLayout->ViewInstancingEnable() == false)
        {
            PAL_ASSERT(viewInstancingDesc.viewInstanceCount == 2);
            PAL_ASSERT(viewInstancingDesc.enableMasking == false);

            const uint32 vpIdOffset    = viewInstancingDesc.viewportArrayIdx[1] -
                                         viewInstancingDesc.viewportArrayIdx[0];
            const uint32 rtSliceOffset = viewInstancingDesc.renderTargetArrayIdx[1] -
                                         viewInstancingDesc.renderTargetArrayIdx[0];

            pPaStereoCntl->bits.VP_ID_OFFSET    = vpIdOffset;
            pPaStereoCntl->bits.RT_SLICE_OFFSET = rtSliceOffset;

            if ((vpIdOffset != 0) || (rtSliceOffset != 0))
            {
                pGeStereoCntl->bits.EN_STEREO = 1;
            }

            pGeStereoCntl->bits.VIEWPORT = viewInstancingDesc.viewportArrayIdx[0];
            pGeStereoCntl->bits.RT_SLICE = viewInstancingDesc.renderTargetArrayIdx[0];

            auto* pVgtDrawPayloadCntl = MedFreq::Get<mmVGT_DRAW_PAYLOAD_CNTL, VGT_DRAW_PAYLOAD_CNTL>(m_medFreqRegs);
            if (pGeStereoCntl->bits.VIEWPORT != 0)
            {
                pVgtDrawPayloadCntl->bits.EN_DRAW_VP = 1;
            }

            if (pGeStereoCntl->bits.RT_SLICE != 0)
            {
                pVgtDrawPayloadCntl->bits.EN_REG_RT_INDEX = 1;
            }
        }
    }
}

// =====================================================================================================================
void GraphicsPipeline::HandleWorkarounds()
{
    Device*     pGfx12Device = static_cast<Device*>(m_pDevice->GetGfxDevice());
    const auto& settings     = pGfx12Device->Settings();

    if (settings.waNoDistTessPacketToOnePa)
    {
        const auto& geCntl            = HighFreq::GetC<mmGE_CNTL, GE_CNTL>(m_highFreqRegs.pairs);
        const auto& vgtShaderStagesEn = MedFreq::GetC<mmVGT_SHADER_STAGES_EN, VGT_SHADER_STAGES_EN>(m_medFreqRegs);

        if ((vgtShaderStagesEn.bits.HS_EN != 0) && (geCntl.bits.PACKET_TO_ONE_PA != 0))
        {
            auto* pVgtTfParam = MedFreq::Get<mmVGT_TF_PARAM, VGT_TF_PARAM>(m_medFreqRegs);
            pVgtTfParam->bits.DISTRIBUTION_MODE = NO_DIST;
        }
    }
}

// =====================================================================================================================
bool GraphicsPipeline::HandleDynamicWavesPerCu(
    const DynamicGraphicsShaderInfos& input,
    RegisterValuePair                 shRegs[DynamicStateOverrideSh::Size()]
    ) const
{
    bool                     anyRegsUpdated = false;
    const GpuChipProperties& chipProps      = m_pDevice->ChipProperties();
    const uint32             cusPerSe       = chipProps.gfx9.numCuPerSh * chipProps.gfx9.numShaderArrays;

    PAL_ASSERT(cusPerSe != 0);

    float nonGsMaxWavesPerCu = 0;

    if (IsTessEnabled())
    {
        nonGsMaxWavesPerCu = input.ds.maxWavesPerCu;

        const uint32 hwHsMaxWavesPerCu = (input.vs.maxWavesPerCu == 0) ? input.hs.maxWavesPerCu :
                                         (input.hs.maxWavesPerCu == 0) ? input.vs.maxWavesPerCu :
                                         Min(input.vs.maxWavesPerCu, input.hs.maxWavesPerCu);
        if (hwHsMaxWavesPerCu > 0)
        {
            const uint32 hsWaveLimitPerSe = static_cast<uint32>(round(hwHsMaxWavesPerCu * cusPerSe));

            auto* pSpiShaderPgmRsrc4Hs =
                DynamicStateOverrideSh::Get<mmSPI_SHADER_PGM_RSRC4_HS, SPI_SHADER_PGM_RSRC4_HS>(shRegs);

            // The hsWaveLimit should less than 1024.
            pSpiShaderPgmRsrc4Hs->bits.WAVE_LIMIT = Min(HsWaveLimitMax, Max(1u, hsWaveLimitPerSe));

            anyRegsUpdated = true;
        }
    }
    else if (HasMeshShader())
    {
        nonGsMaxWavesPerCu = input.ms.maxWavesPerCu;
    }
    else
    {
        nonGsMaxWavesPerCu = input.vs.maxWavesPerCu;
    }

    // Overload the HW GS wave limit if a non-zero limit was specified by the client.
    uint32 hwGsMaxWavesPerCu = (nonGsMaxWavesPerCu == 0)     ? input.gs.maxWavesPerCu :
                               (input.gs.maxWavesPerCu == 0) ? nonGsMaxWavesPerCu     :
                               Min(nonGsMaxWavesPerCu, input.gs.maxWavesPerCu);
    if (hwGsMaxWavesPerCu > 0)
    {
        const uint32 gsWaveLimitPerSe = static_cast<uint32>(round(hwGsMaxWavesPerCu * cusPerSe));

        auto* pSpiShaderPgmRsrc4Gs =
            DynamicStateOverrideSh::Get<mmSPI_SHADER_PGM_RSRC4_GS, SPI_SHADER_PGM_RSRC4_GS>(shRegs);

        // The gsWaveLimit should less than 1024.
        pSpiShaderPgmRsrc4Gs->bits.WAVE_LIMIT = Min(GsWaveLimitMax, Max(1u, gsWaveLimitPerSe));

        anyRegsUpdated = true;
    }

    // Overload the HW GS wave limit if a non-zero limit was specified by the client.
    if (input.ps.maxWavesPerCu > 0)
    {
        const uint32 numPackersPerSe           = chipProps.gfx9.numScPerSe * chipProps.gfx9.numPackerPerSc;
        const uint32 psWaveLimitPerPackerPerSe =
            static_cast<uint32>(round(input.ps.maxWavesPerCu * cusPerSe)) / numPackersPerSe;

        auto* pSpiShaderPgmRsrc4Ps =
            DynamicStateOverrideSh::Get<mmSPI_SHADER_PGM_RSRC4_PS, SPI_SHADER_PGM_RSRC4_PS>(shRegs);

        // The psWaveLimit is specified per packer per SE and should less than 1024.
        pSpiShaderPgmRsrc4Ps->bits.WAVE_LIMIT = Min(PsWaveLimitMax, Max(1u, psWaveLimitPerPackerPerSe));

        anyRegsUpdated = true;
    }

    return anyRegsUpdated;
}

// =====================================================================================================================
void GraphicsPipeline::OverrideDynamicState(
    const DynamicGraphicsState& dynamicState,
    RegisterValuePair           ctxRegs[DynamicStateOverrideCtx::Size()],
    uint32*                     pGfxStateCbTargetMask,
    DepthClampMode*             pGfxStateDepthClampMode
    ) const
{
    PAL_ASSERT(dynamicState.enable.u32All != 0); // Assuming the caller checked this!

    if ((dynamicState.enable.switchWinding != 0) && (dynamicState.switchWinding != 0))
    {
        auto* pVgtTfParam = DynamicStateOverrideCtx::Get<mmVGT_TF_PARAM, VGT_TF_PARAM>(ctxRegs);

        if (pVgtTfParam->bits.TOPOLOGY == OUTPUT_TRIANGLE_CW)
        {
            pVgtTfParam->bits.TOPOLOGY = OUTPUT_TRIANGLE_CCW;
        }
        else if (pVgtTfParam->bits.TOPOLOGY == OUTPUT_TRIANGLE_CCW)
        {
            pVgtTfParam->bits.TOPOLOGY = OUTPUT_TRIANGLE_CW;
        }
    }

    auto* pPaClClipCntl = DynamicStateOverrideCtx::Get<mmPA_CL_CLIP_CNTL, PA_CL_CLIP_CNTL>(ctxRegs);

    if (dynamicState.enable.rasterizerDiscardEnable != 0)
    {
        pPaClClipCntl->bits.DX_RASTERIZATION_KILL = dynamicState.rasterizerDiscardEnable;
    }

    if (dynamicState.enable.depthClipMode != 0)
    {
        pPaClClipCntl->bits.ZCLIP_NEAR_DISABLE = dynamicState.depthClipNearEnable ? 0 : 1;
        pPaClClipCntl->bits.ZCLIP_FAR_DISABLE  = dynamicState.depthClipFarEnable ? 0 : 1;
    }

    if (dynamicState.enable.depthRange != 0)
    {
        pPaClClipCntl->bits.DX_CLIP_SPACE_DEF = (dynamicState.depthRange == DepthRange::ZeroToOne);
    }

    if (dynamicState.enable.perpLineEndCapsEnable != 0)
    {
        auto* pPaScLineCntl = DynamicStateOverrideCtx::Get<mmPA_SC_LINE_CNTL, PA_SC_LINE_CNTL>(ctxRegs);
        pPaScLineCntl->bits.PERPENDICULAR_ENDCAP_ENA = dynamicState.perpLineEndCapsEnable;
    }

    auto* pCbColorControl = DynamicStateOverrideCtx::Get<mmCB_COLOR_CONTROL, CB_COLOR_CONTROL>(ctxRegs);
    if (dynamicState.enable.logicOp != 0)
    {
        pCbColorControl->bits.ROP3 = Rop3(dynamicState.logicOp);
    }

    if (dynamicState.enable.dualSourceBlendEnable)
    {
        pCbColorControl->bits.DISABLE_DUAL_QUAD = dynamicState.dualSourceBlendEnable ? 1 : 0;
    }

    if (dynamicState.enable.colorWriteMask != 0)
    {
        auto* pCbTargetMask = DynamicStateOverrideCtx::Get<mmCB_TARGET_MASK, CB_TARGET_MASK>(ctxRegs);

        pCbTargetMask->u32All  = (*pGfxStateCbTargetMask) & dynamicState.colorWriteMask;
        *pGfxStateCbTargetMask = pCbTargetMask->u32All;
    }

    if (dynamicState.enable.alphaToCoverageEnable != 0)
    {
        auto* pDbShaderControl = DynamicStateOverrideCtx::Get<mmDB_SHADER_CONTROL, DB_SHADER_CONTROL>(ctxRegs);
        pDbShaderControl->bits.ALPHA_TO_MASK_DISABLE = dynamicState.alphaToCoverageEnable ? 0 : 1;
    }

    if (dynamicState.enable.depthClampMode != 0)
    {
        auto* pDbViewportControl = DynamicStateOverrideCtx::Get<mmDB_VIEWPORT_CONTROL, DB_VIEWPORT_CONTROL>(ctxRegs);
        auto* pDbShaderControl   = DynamicStateOverrideCtx::Get<mmDB_SHADER_CONTROL, DB_SHADER_CONTROL>(ctxRegs);

        pDbViewportControl->bits.DISABLE_VIEWPORT_CLAMP = (dynamicState.depthClampMode == Pal::DepthClampMode::None);

        // For internal RPM pipelines, we want to always disable depth clamp based on depthClampMode
        // without honor setting of depthClampBasedOnZExport.
        if ((IsInternal() == false) && m_pDevice->GetPublicSettings()->depthClampBasedOnZExport)
        {
            pDbViewportControl->bits.DISABLE_VIEWPORT_CLAMP &= pDbShaderControl->bits.Z_EXPORT_ENABLE;
        }

        *pGfxStateDepthClampMode = static_cast<DepthClampMode>(dynamicState.depthClampMode);
    }
}

// =====================================================================================================================
// Updates the device that this pipeline has some new ring-size requirements.
void GraphicsPipeline::UpdateRingSizes(
    const PalAbi::CodeObjectMetadata& metadata)
{
    Device* pGfx12Device = static_cast<Device*>(m_pDevice->GetGfxDevice());

    m_ringSizes.itemSize[size_t(ShaderRingType::VertexAttributes)] =
        pGfx12Device->Settings().gfx12VertexAttributesRingBufferSizePerSe;

    // We only need to specify any nonzero item-size for Prim and Pos buffers because they're fixed-size rings
    // whose size doesn't depend on the item-size at all.
    m_ringSizes.itemSize[static_cast<size_t>(ShaderRingType::PrimBuffer)] = 1;
    m_ringSizes.itemSize[static_cast<size_t>(ShaderRingType::PosBuffer)]  = 1;

    if (IsTessEnabled())
    {
        // NOTE: the TF buffer is special: we only need to specify any nonzero item-size because its a fixed-size ring
        // whose size doesn't depend on the item-size at all.
        m_ringSizes.itemSize[size_t(ShaderRingType::TfBuffer)] = 1;

        // NOTE: the off-chip LDS buffer's item-size refers to the "number of buffers" that the hardware uses (i.e.,
        // VGT_HS_OFFCHIP_PARAM::OFFCHIP_BUFFERING).
        m_ringSizes.itemSize[size_t(ShaderRingType::OffChipLds)] = m_pDevice->Settings().numOffchipLdsBuffers;
    }

    m_ringSizes.itemSize[size_t(ShaderRingType::GfxScratch)] = ComputeScratchMemorySize(metadata);

    m_ringSizes.itemSize[size_t(ShaderRingType::ComputeScratch)] = ComputePipeline::CalcScratchMemSize(metadata);

    if (metadata.pipeline.hasEntry.meshScratchMemorySize != 0)
    {
        m_ringSizes.itemSize[static_cast<size_t>(ShaderRingType::MeshScratch)] = metadata.pipeline.meshScratchMemorySize;
    }
}

// =====================================================================================================================
// Calculates the maximum scratch memory in dwords necessary by checking the scratch memory needed for each shader.
uint32 GraphicsPipeline::ComputeScratchMemorySize(
    const PalAbi::CodeObjectMetadata& metadata
    ) const
{
    const auto& vgtShaderStagesEn = MedFreq::GetC<mmVGT_SHADER_STAGES_EN, VGT_SHADER_STAGES_EN>(m_medFreqRegs);

    const bool isWave32Tbl[] = {
        (vgtShaderStagesEn.bits.HS_W32_EN             != 0),
        (vgtShaderStagesEn.bits.HS_W32_EN             != 0),
        (vgtShaderStagesEn.bits.GS_W32_EN             != 0),
        (vgtShaderStagesEn.bits.GS_W32_EN             != 0),
        (vgtShaderStagesEn.bits.GS_W32_EN             != 0),
        (m_highFreqRegs.spiPsInControl.bits.PS_W32_EN != 0),
        false,
    };
    static_assert(ArrayLen(isWave32Tbl) == static_cast<size_t>(Abi::HardwareStage::Count),
                  "IsWave32Tbl is no longer appropriately sized!");

    uint32 scratchMemorySizeBytes = 0;
    for (uint32 i = 0; i < static_cast<uint32>(Abi::HardwareStage::Count); ++i)
    {
        if (static_cast<Abi::HardwareStage>(i) == Abi::HardwareStage::Cs)
        {
            // We don't handle compute-scratch in this function.
            continue;
        }

        const auto& stageMetadata = metadata.pipeline.hardwareStage[i];
        if (stageMetadata.hasEntry.scratchMemorySize != 0)
        {
            uint32 stageScratchMemorySize = stageMetadata.scratchMemorySize;

            if (isWave32Tbl[i] == false)
            {
                // We allocate scratch memory based on the minimum wave size for the chip, which for Gfx10+ ASICs will
                // be Wave32. In order to appropriately size the scratch memory (reported in the ELF as per-thread) for
                // a Wave64, we need to multiply by 2.
                stageScratchMemorySize *= 2;
            }

            scratchMemorySizeBytes = Max(scratchMemorySizeBytes, stageScratchMemorySize);
        }
    }

    return scratchMemorySizeBytes / sizeof(uint32);
}

// =====================================================================================================================
void GraphicsPipeline::UpdateBinningStatus()
{
    const DB_SHADER_CONTROL& dbShaderControl =
        HighFreq::GetC<mmDB_SHADER_CONTROL, DB_SHADER_CONTROL>(m_highFreqRegs.pairs);

    const bool disableBinningAppendConsume = true;

    bool disableBinning = false;

    const bool canKill = dbShaderControl.bits.KILL_ENABLE             ||
                         dbShaderControl.bits.MASK_EXPORT_ENABLE      ||
                         dbShaderControl.bits.COVERAGE_TO_MASK_ENABLE ||
                         (dbShaderControl.bits.ALPHA_TO_MASK_DISABLE == 0);

    const bool canReject = (dbShaderControl.bits.Z_EXPORT_ENABLE == 0) ||
                           (dbShaderControl.bits.CONSERVATIVE_Z_EXPORT > 0);

    // Disable binning when the pixels can be rejected before the PS and the PS can kill the pixel.
    // This is an optimization for cases where early Z accepts are not allowed (because the shader may kill) and early
    // Z rejects are allowed (PS does not output depth).
    // In such cases the binner orders pixel traffic in a suboptimal way.
    disableBinning |= canKill && canReject && (m_pDevice->GetPublicSettings()->disableBinningPsKill ==
                                               OverrideMode::Enabled);

    // Disable binning when the PS uses append/consume.
    // In such cases, binning changes the ordering of append/consume opeartions. This re-ordering can be suboptimal.
    disableBinning |= PsUsesAppendConsume() && disableBinningAppendConsume;

    // Overriding binning mode
    if (GetBinningOverride() == BinningOverride::Enable)
    {
        m_isBinningDisabled = false;
    }
    else if (GetBinningOverride() == BinningOverride::Disable)
    {
        m_isBinningDisabled = true;
    }
    else
    {
        m_isBinningDisabled = disableBinning;
    }
}

// =====================================================================================================================
uint32* GraphicsPipeline::Prefetch(
    uint32  prefetchClampSize,
    uint32* pCmdSpace
    ) const
{
    for (uint32 i = 0; i < m_prefetchRangeCount; i++)
    {
        pCmdSpace += CmdUtil::BuildPrimeGpuCaches(m_prefetch[i],
                                                  prefetchClampSize,
                                                  EngineTypeUniversal,
                                                  pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Return if hardware stereo rendering is enabled.
bool GraphicsPipeline::HwStereoRenderingEnabled() const
{
    const GE_STEREO_CNTL& geStereoCntl = LowFreq::GetC<mmGE_STEREO_CNTL, GE_STEREO_CNTL>(m_lowFreqRegs);

    const uint32 enStereo = geStereoCntl.bits.EN_STEREO;

    return (enStereo != 0);
}

// =====================================================================================================================
// Return if hardware stereo rendering uses multiple viewports.
bool GraphicsPipeline::HwStereoRenderingUsesMultipleViewports() const
{
    const PA_STEREO_CNTL& paStereoCntl = LowFreq::GetC<mmPA_STEREO_CNTL, PA_STEREO_CNTL>(m_lowFreqRegs);

    const uint32 vpIdOffset = paStereoCntl.bits.VP_ID_OFFSET;

    return (vpIdOffset != 0);
}

// =====================================================================================================================
// Link graphics pipeline from graphics shader libraries.
Result GraphicsPipeline::LinkGraphicsLibraries(
    const GraphicsPipelineCreateInfo& createInfo)
{
    const GraphicsPipeline* pPreRasterLib = nullptr;
    const GraphicsPipeline* pPsLib        = nullptr;
    const GraphicsPipeline* pExpLib       = nullptr;
    const GraphicsShaderLibrary* pPsShaderLibrary = nullptr;
    const GraphicsShaderLibrary* pExpShaderLibrary = nullptr;
    ColorExportProperty colorExportProperty = {};

    for (uint32 i = 0; i < NumGfxShaderLibraries(); i++)
    {
        const GraphicsShaderLibrary* pLib = reinterpret_cast<const GraphicsShaderLibrary*>(GetGraphicsShaderLibrary(i));
        uint32 apiShaderMask = pLib->GetApiShaderMask();
        if (pLib->IsColorExportShader())
        {
            PAL_ASSERT(pExpLib == nullptr);
            pExpLib = reinterpret_cast<const GraphicsPipeline*>(pLib->GetPartialPipeline());
            pExpShaderLibrary = pLib;
            pExpShaderLibrary->GetColorExportProperty(&colorExportProperty);
        }
        else if (Util::TestAnyFlagSet(apiShaderMask, 1 << static_cast<uint32>(ShaderType::Pixel)))
        {
            PAL_ASSERT(pPsLib == nullptr);
            pPsLib = reinterpret_cast<const GraphicsPipeline*>(pLib->GetPartialPipeline());
            pPsShaderLibrary = pLib;
        }
        else
        {
            PAL_ASSERT(Util::TestAnyFlagSet(apiShaderMask,
                (1 << static_cast<uint32>(ShaderType::Vertex) | (1 << static_cast<uint32>(ShaderType::Mesh)))));
            PAL_ASSERT(pPreRasterLib == nullptr);
            pPreRasterLib = reinterpret_cast<const GraphicsPipeline*>(pLib->GetPartialPipeline());
        }
    }
    PAL_ASSERT((pPreRasterLib != nullptr) && (pPsLib != nullptr));
    if (pExpLib == nullptr)
    {
        pExpLib = pPsLib;
        pExpShaderLibrary = pPsShaderLibrary;
    }

    // Set up user-data layout first because it may be needed by subsequent Init calls.
    Result result = GraphicsUserDataLayout::Create(
        *m_pDevice, *pPreRasterLib->m_pUserDataLayout, *pPsLib->m_pUserDataLayout, &m_pUserDataLayout);

    // We do not expect MeshShaders to have Vertex or Instance Offset mapped.
    PAL_ASSERT((result == Result::Success) &&
               ((HasMeshShader() == false) ||
                ((m_pUserDataLayout->GetVertexBase().u32All == UserDataNotMapped) &&
                 (m_pUserDataLayout->GetInstanceBase().u32All == UserDataNotMapped))));

    if (result == Result::Success)
    {
        m_numInterpolants = pPsLib->m_numInterpolants;
        m_semanticCount   = pPsLib->m_semanticCount;
        memcpy(m_colorExportAddr, pExpLib->m_colorExportAddr, sizeof(m_colorExportAddr));

        memcpy(m_strmoutVtxStride,
               pPreRasterLib->m_strmoutVtxStride,
               sizeof(m_strmoutVtxStride));

        if (createInfo.groupLaunchGuarantee != TriState::Disable)
        {
            m_disableGroupLaunchGuarantee = false;
        }

        if (IsTessEnabled())
        {
            m_hsStageInfo = pPreRasterLib->m_hsStageInfo;
            memcpy(m_hullShaderRegs, pPreRasterLib->m_hullShaderRegs, sizeof(m_hullShaderRegs));
        }

        m_gsStageInfo = pPreRasterLib->m_gsStageInfo;
        memcpy(m_geomShaderRegs, pPreRasterLib->m_geomShaderRegs, sizeof(m_geomShaderRegs));
        memcpy(&m_esGsLdsSize,   &(pPreRasterLib->m_esGsLdsSize), sizeof(m_esGsLdsSize));

        // SPI_SHADER_GS_OUT_CONFIG_PS is special. Its NUM_INTERP and NUM_PRIM_INTERP come from pixel shader library.
        const SPI_SHADER_GS_OUT_CONFIG_PS& gsOutConfigPsSrc =
            GeomShader::GetC<mmSPI_SHADER_GS_OUT_CONFIG_PS, SPI_SHADER_GS_OUT_CONFIG_PS>(pPsLib->m_geomShaderRegs);
        SPI_SHADER_GS_OUT_CONFIG_PS* pGsOutConfigPsDst =
            GeomShader::Get<mmSPI_SHADER_GS_OUT_CONFIG_PS, SPI_SHADER_GS_OUT_CONFIG_PS>(m_geomShaderRegs);
        pGsOutConfigPsDst->bits.NUM_INTERP      = gsOutConfigPsSrc.bits.NUM_INTERP;
        pGsOutConfigPsDst->bits.NUM_PRIM_INTERP = gsOutConfigPsSrc.bits.NUM_PRIM_INTERP;

        m_psStageInfo = pPsLib->m_psStageInfo;
        memcpy(m_pixelShaderRegs, pPsLib->m_pixelShaderRegs, sizeof(m_pixelShaderRegs));
        if (pExpShaderLibrary->IsColorExportShader())
        {
            const bool isWave32 = m_highFreqRegs.spiPsInControl.bits.PS_W32_EN != 0;
            auto* pRsrc1 = PixelShader::Get<mmSPI_SHADER_PGM_RSRC1_PS, SPI_SHADER_PGM_RSRC1_PS>(m_pixelShaderRegs);
            uint32 expVgprNum = CalcNumVgprs(colorExportProperty.vgprCount, isWave32);
            pRsrc1->bits.VGPRS = Max(expVgprNum, pRsrc1->bits.VGPRS);
        }

        // Link Ps input interpolants
        memcpy(m_highFreqRegs.spiPsInputCntl,
               pPsLib->m_highFreqRegs.spiPsInputCntl,
               sizeof(SPI_PS_INPUT_CNTL_0) * m_numInterpolants);
        if ((pPsLib->m_semanticCount > 0) && (pPreRasterLib->m_semanticCount > 0))
        {
            constexpr uint32 DefaultValOffset = (1 << 5);
            constexpr uint32 ValOffsetMask = ((1 << 5) - 1);
            for (uint32 i = 0; i < m_semanticCount; i++)
            {
                uint32 index = DefaultValOffset;
                for (uint32 j = 0; j < pPreRasterLib->m_semanticCount; j++)
                {
                    if (pPsLib->m_semanticInfo[i].semantic == pPreRasterLib->m_semanticInfo[j].semantic)
                    {
                        index = pPreRasterLib->m_semanticInfo[j].index;
                    }
                }
                m_highFreqRegs.spiPsInputCntl[i].bits.OFFSET &= ~ValOffsetMask;
                m_highFreqRegs.spiPsInputCntl[i].bits.OFFSET |= index;
            }
        }

        LinkContextState(pPreRasterLib, pPsLib, pExpLib);
        UpdateContextState(createInfo);

        // This must come after any register initialization!
        HandleWorkarounds();

        UpdateBinningStatus();

        GenerateHashes();
    }

    // Update scratch size
    m_ringSizes = pPreRasterLib->m_ringSizes;
    m_ringSizes.itemSize[uint32(ShaderRingType::GfxScratch)] =
        Util::Max(m_ringSizes.itemSize[uint32(ShaderRingType::GfxScratch)],
            pPsLib->m_ringSizes.itemSize[uint32(ShaderRingType::GfxScratch)]);
    if (pExpShaderLibrary->IsColorExportShader())
    {
        const bool isWave32 = m_highFreqRegs.spiPsInControl.bits.PS_W32_EN != 0;

        size_t scratchMemorySize = isWave32 ?
            colorExportProperty.scratchMemorySize :
            colorExportProperty.scratchMemorySize * 2;

        m_ringSizes.itemSize[uint32(ShaderRingType::GfxScratch)] =
            Util::Max(m_ringSizes.itemSize[uint32(ShaderRingType::GfxScratch)], scratchMemorySize);
    }

    // Update prefetch ranges
    m_prefetchRangeCount = 0;
    if (pPreRasterLib->m_prefetchRangeCount > 0)
    {
        m_prefetch[m_prefetchRangeCount++] = pPreRasterLib->m_prefetch[0];
    }
    if (pPsLib->m_prefetchRangeCount > 0)
    {
        m_prefetch[m_prefetchRangeCount++] = pPsLib->m_prefetch[0];
    }
    if ((pExpLib != nullptr) && (pExpLib->m_prefetchRangeCount > 0))
    {
        m_prefetch[m_prefetchRangeCount++] = pExpLib->m_prefetch[0];
    }

    return result;
}

// =====================================================================================================================
// Initializes graphics pipeline context state for graphics shader libraries.
void GraphicsPipeline::LinkContextState(
    const GraphicsPipeline*           pPreRasterLib,
    const GraphicsPipeline*           pPsLib,
    const GraphicsPipeline*           pExpLib)
{
    // Pre-raster
    m_highFreqRegs.spiShaderPosFormat = pPreRasterLib->m_highFreqRegs.spiShaderPosFormat;
    m_highFreqRegs.spiShaderIdxFormat = pPreRasterLib->m_highFreqRegs.spiShaderIdxFormat;
    HighFreq::Get<mmGE_CNTL, GE_CNTL>(m_highFreqRegs.pairs)->u32All =
        HighFreq::GetC<mmGE_CNTL, GE_CNTL>(pPreRasterLib->m_highFreqRegs.pairs).u32All;

    // Ps
    m_highFreqRegs.spiBarycCntl      = pPsLib->m_highFreqRegs.spiBarycCntl;
    m_highFreqRegs.spiPsInputEna     = pPsLib->m_highFreqRegs.spiPsInputEna;
    m_highFreqRegs.spiPsInputAddr    = pPsLib->m_highFreqRegs.spiPsInputAddr;
    m_highFreqRegs.spiInterpControl0 = pPsLib->m_highFreqRegs.spiInterpControl0;
    m_highFreqRegs.spiPsInControl    = pPsLib->m_highFreqRegs.spiPsInControl;
    HighFreq::Get<mmDB_SHADER_CONTROL, DB_SHADER_CONTROL>(m_highFreqRegs.pairs)->u32All =
        HighFreq::GetC<mmDB_SHADER_CONTROL, DB_SHADER_CONTROL>(pPsLib->m_highFreqRegs.pairs).u32All;

    // Override ALPHA_TO_MASK_DISABLE based on export shader.
    HighFreq::Get< mmDB_SHADER_CONTROL, DB_SHADER_CONTROL>(m_highFreqRegs.pairs)->bits.ALPHA_TO_MASK_DISABLE &=
        HighFreq::GetC<mmDB_SHADER_CONTROL, DB_SHADER_CONTROL>(
            pExpLib->m_highFreqRegs.pairs).bits.ALPHA_TO_MASK_DISABLE;

    // Color Export
    m_highFreqRegs.spiShaderZFormat   = pExpLib->m_highFreqRegs.spiShaderZFormat;
    m_highFreqRegs.spiShaderColFormat = pExpLib->m_highFreqRegs.spiShaderColFormat;
    HighFreq::Get<mmCB_SHADER_MASK, CB_SHADER_MASK>(m_highFreqRegs.pairs)->u32All =
        HighFreq::GetC<mmCB_SHADER_MASK, CB_SHADER_MASK>(pExpLib->m_highFreqRegs.pairs).u32All;

    // Low and Medium Frequency State below.
    memcpy(m_lowFreqRegs, pPreRasterLib->m_lowFreqRegs, sizeof(m_lowFreqRegs));
    memcpy(m_medFreqRegs, pPreRasterLib->m_medFreqRegs, sizeof(m_medFreqRegs));
    MedFreq::Get<mmPA_SC_SHADER_CONTROL, PA_SC_SHADER_CONTROL>(m_medFreqRegs)->u32All =
        MedFreq::GetC<mmPA_SC_SHADER_CONTROL, PA_SC_SHADER_CONTROL>(pPsLib->m_medFreqRegs).u32All;
    MedFreq::Get<mmPA_SC_HISZ_CONTROL, PA_SC_HISZ_CONTROL>(m_medFreqRegs)->u32All =
        MedFreq::GetC<mmPA_SC_HISZ_CONTROL, PA_SC_HISZ_CONTROL>(pPsLib->m_medFreqRegs).u32All;
}

// =====================================================================================================================
// Update graphics pipeline context state according to create info and settings.
void GraphicsPipeline::UpdateContextState(
    const GraphicsPipelineCreateInfo& createInfo)
{
    m_highFreqRegs.spiInterpControl0.bits.FLAT_SHADE_ENA = (createInfo.rsState.shadeMode == ShadeMode::Flat);
    m_highFreqRegs.spiInterpControl0.bits.PNT_SPRITE_TOP_1 =
        (m_highFreqRegs.spiInterpControl0.bits.PNT_SPRITE_ENA != 0) &&
        (createInfo.rsState.pointCoordOrigin != PointOrigin::UpperLeft);

    // Overwrite PS related state
    auto* pPaScShaderControl = MedFreq::Get<mmPA_SC_SHADER_CONTROL, PA_SC_SHADER_CONTROL>(m_medFreqRegs);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 869
    switch (createInfo.rsState.forcedShadingRate)
    {
    case PsShadingRate::SampleRate:
        pPaScShaderControl->bits.PS_ITER_SAMPLE = 1;
        break;
    case PsShadingRate::PixelRate:
        pPaScShaderControl->bits.PS_ITER_SAMPLE = 0;
        break;
    default:
        break;
    }
#endif

    // Mark if this PS uses per sample shading (either declared in PS or forced by createInfo) in our public
    // info struct.
    m_info.ps.flags.perSampleShading = pPaScShaderControl->bits.PS_ITER_SAMPLE;

    // Overwrite pre-raster registers
    auto* pPaClVsOutCntl = MedFreq::Get<mmPA_CL_VS_OUT_CNTL, PA_CL_VS_OUT_CNTL>(m_medFreqRegs);

    if (createInfo.rsState.flags.cullDistMaskValid != 0)
    {
        pPaClVsOutCntl->bits.CULL_DIST_ENA_0 &= ((createInfo.rsState.cullDistMask & 0x01) != 0);
        pPaClVsOutCntl->bits.CULL_DIST_ENA_1 &= ((createInfo.rsState.cullDistMask & 0x02) != 0);
        pPaClVsOutCntl->bits.CULL_DIST_ENA_2 &= ((createInfo.rsState.cullDistMask & 0x04) != 0);
        pPaClVsOutCntl->bits.CULL_DIST_ENA_3 &= ((createInfo.rsState.cullDistMask & 0x08) != 0);
        pPaClVsOutCntl->bits.CULL_DIST_ENA_4 &= ((createInfo.rsState.cullDistMask & 0x10) != 0);
        pPaClVsOutCntl->bits.CULL_DIST_ENA_5 &= ((createInfo.rsState.cullDistMask & 0x20) != 0);
        pPaClVsOutCntl->bits.CULL_DIST_ENA_6 &= ((createInfo.rsState.cullDistMask & 0x40) != 0);
        pPaClVsOutCntl->bits.CULL_DIST_ENA_7 &= ((createInfo.rsState.cullDistMask & 0x80) != 0);
    }

    if (createInfo.rsState.flags.clipDistMaskValid != 0)
    {
        pPaClVsOutCntl->bits.CLIP_DIST_ENA_0 &= ((createInfo.rsState.clipDistMask & 0x01) != 0);
        pPaClVsOutCntl->bits.CLIP_DIST_ENA_1 &= ((createInfo.rsState.clipDistMask & 0x02) != 0);
        pPaClVsOutCntl->bits.CLIP_DIST_ENA_2 &= ((createInfo.rsState.clipDistMask & 0x04) != 0);
        pPaClVsOutCntl->bits.CLIP_DIST_ENA_3 &= ((createInfo.rsState.clipDistMask & 0x08) != 0);
        pPaClVsOutCntl->bits.CLIP_DIST_ENA_4 &= ((createInfo.rsState.clipDistMask & 0x10) != 0);
        pPaClVsOutCntl->bits.CLIP_DIST_ENA_5 &= ((createInfo.rsState.clipDistMask & 0x20) != 0);
        pPaClVsOutCntl->bits.CLIP_DIST_ENA_6 &= ((createInfo.rsState.clipDistMask & 0x40) != 0);
        pPaClVsOutCntl->bits.CLIP_DIST_ENA_7 &= ((createInfo.rsState.clipDistMask & 0x80) != 0);
    }

    auto* pPaClNggCntl = LowFreq::Get<mmPA_CL_NGG_CNTL, PA_CL_NGG_CNTL>(m_lowFreqRegs);
    pPaClNggCntl->bits.VERTEX_REUSE_DEPTH = 30;
    pPaClNggCntl->bits.INDEX_BUF_EDGE_FLAG_ENA =
        (createInfo.iaState.topologyInfo.topologyIsPolygon ||
        (createInfo.iaState.topologyInfo.primitiveType == Pal::PrimitiveType::Quad));

    auto* pPaScEdgeRuleControl = LowFreq::Get<mmPA_SC_EDGERULE, PA_SC_EDGERULE>(m_lowFreqRegs);

    switch (createInfo.rsState.edgeRule)
    {
    case EdgeRuleMode::D3dCompliant:
        if (createInfo.rsState.pointCoordOrigin == Pal::PointOrigin::UpperLeft)
        {
            pPaScEdgeRuleControl->bits.ER_TRI     = 0xa;
            pPaScEdgeRuleControl->bits.ER_POINT   = 0xa;
            pPaScEdgeRuleControl->bits.ER_RECT    = 0xa;
            pPaScEdgeRuleControl->bits.ER_LINE_LR = 0x1a;
            pPaScEdgeRuleControl->bits.ER_LINE_RL = 0x26;
            pPaScEdgeRuleControl->bits.ER_LINE_TB = 0xa;
            pPaScEdgeRuleControl->bits.ER_LINE_BT = 0xa;
        }
        else
        {
            pPaScEdgeRuleControl->bits.ER_TRI     = 0xa;
            pPaScEdgeRuleControl->bits.ER_POINT   = 0x5;
            pPaScEdgeRuleControl->bits.ER_RECT    = 0x9;
            pPaScEdgeRuleControl->bits.ER_LINE_LR = 0x29;
            pPaScEdgeRuleControl->bits.ER_LINE_RL = 0x29;
            pPaScEdgeRuleControl->bits.ER_LINE_TB = 0xa;
            pPaScEdgeRuleControl->bits.ER_LINE_BT = 0xa;
        }
        break;
    case EdgeRuleMode::OpenGlDefault:
        if (createInfo.rsState.pointCoordOrigin == Pal::PointOrigin::UpperLeft)
        {
            pPaScEdgeRuleControl->bits.ER_TRI     = 0xa;
            pPaScEdgeRuleControl->bits.ER_POINT   = 0x6;
            pPaScEdgeRuleControl->bits.ER_RECT    = 0xa;
            pPaScEdgeRuleControl->bits.ER_LINE_LR = 0x19;
            pPaScEdgeRuleControl->bits.ER_LINE_RL = 0x25;
            pPaScEdgeRuleControl->bits.ER_LINE_TB = 0xa;
            pPaScEdgeRuleControl->bits.ER_LINE_BT = 0xa;
        }
        else
        {
            pPaScEdgeRuleControl->bits.ER_TRI     = 0xa;
            pPaScEdgeRuleControl->bits.ER_POINT   = 0x5;
            pPaScEdgeRuleControl->bits.ER_RECT    = 0x9;
            pPaScEdgeRuleControl->bits.ER_LINE_LR = 0x2a;
            pPaScEdgeRuleControl->bits.ER_LINE_RL = 0x2a;
            pPaScEdgeRuleControl->bits.ER_LINE_TB = 0xa;
            pPaScEdgeRuleControl->bits.ER_LINE_BT = 0xa;
        }
        break;
    default:
        break;
    }

    auto* pPaClClipCntl = MedFreq::Get<mmPA_CL_CLIP_CNTL, PA_CL_CLIP_CNTL>(m_medFreqRegs);

    pPaClClipCntl->bits.DX_CLIP_SPACE_DEF = (createInfo.viewportInfo.depthRange == DepthRange::ZeroToOne);
    if (createInfo.viewportInfo.depthClipNearEnable == false)
    {
        pPaClClipCntl->bits.ZCLIP_NEAR_DISABLE = 1;
    }
    if (createInfo.viewportInfo.depthClipFarEnable == false)
    {
        pPaClClipCntl->bits.ZCLIP_FAR_DISABLE = 1;
    }
    if (static_cast<TossPointMode>(m_pDevice->Settings().tossPointMode) == TossPointAfterRaster)
    {
        pPaClClipCntl->bits.DX_RASTERIZATION_KILL = 1;
    }

    auto* pPaScLineCntl = MedFreq::Get<mmPA_SC_LINE_CNTL, PA_SC_LINE_CNTL>(m_medFreqRegs);
    pPaScLineCntl->bits.EXPAND_LINE_WIDTH        = createInfo.rsState.expandLineWidth;
    pPaScLineCntl->bits.DX10_DIAMOND_TEST_ENA    = createInfo.rsState.dx10DiamondTestDisable ? 0 : 1;
    pPaScLineCntl->bits.LAST_PIXEL               = createInfo.rsState.rasterizeLastLinePixel;
    pPaScLineCntl->bits.PERPENDICULAR_ENDCAP_ENA = createInfo.rsState.perpLineEndCapsEnable;

    auto* pDbViewportControl = MedFreq::Get<mmDB_VIEWPORT_CONTROL, DB_VIEWPORT_CONTROL>(m_medFreqRegs);
    pDbViewportControl->bits.DISABLE_VIEWPORT_CLAMP = (createInfo.rsState.depthClampMode == Pal::DepthClampMode::None);
    if (m_pDevice->GetPublicSettings()->depthClampBasedOnZExport)
    {
        auto* pDbShaderControl = HighFreq::Get<mmDB_SHADER_CONTROL, DB_SHADER_CONTROL>(m_highFreqRegs.pairs);
        pDbViewportControl->bits.DISABLE_VIEWPORT_CLAMP &= pDbShaderControl->bits.Z_EXPORT_ENABLE;
    }

    m_isAlphaToCoverage = createInfo.cbState.alphaToCoverageEnable;

    UpdateColorExportState(createInfo);
    UpdateStereoState(createInfo);
}

// ==================================================================================================================== =
// Precompute the number of vertex of output primitive
void GraphicsPipeline::CalculateOutputNumVertices()
{
    bool hasGS = IsGsEnabled();
    bool hasTes = IsTessEnabled();
    bool hasMs = HasMeshShader();
    if (hasGS || hasMs)
    {
        const auto& vgtGsOutPrimType = MedFreq::GetC<mmVGT_GS_OUT_PRIM_TYPE, VGT_GS_OUT_PRIM_TYPE>(m_medFreqRegs);
        switch (vgtGsOutPrimType.bits.OUTPRIM_TYPE)
        {
        case POINTLIST:
            m_outputNumVertices = 1;
            break;
        case LINESTRIP:
            m_outputNumVertices = 2;
            break;
        case TRISTRIP:
            m_outputNumVertices = 3;
            break;
        default:
            PAL_ASSERT_ALWAYS();
            break;
        }
    }
    else if (hasTes)
    {
        const auto& vgtTfParam = MedFreq::GetC<mmVGT_TF_PARAM, VGT_TF_PARAM>(m_medFreqRegs);
        switch (vgtTfParam.bits.TOPOLOGY)
        {
        case OUTPUT_POINT:
            m_outputNumVertices = 1;
            break;
        case OUTPUT_LINE:
            m_outputNumVertices = 2;
            break;
        case OUTPUT_TRIANGLE_CW:
        case OUTPUT_TRIANGLE_CCW:
            m_outputNumVertices = 3;
            break;
        default:
            PAL_ASSERT_ALWAYS();
            break;
        }
    }
}

// =====================================================================================================================
// Returns true if no color buffers and no PS UAVs and AlphaToCoverage is disabled.
bool GraphicsPipeline::CanRbPlusOptimizeDepthOnly(
    const DynamicGraphicsState* pDynamicState
    ) const
{
    bool canEnableDepthOnlyOpt =
        (NumColorTargets() == 0) &&
        (HighFreq::GetC<mmCB_COLOR_CONTROL, CB_COLOR_CONTROL>(m_highFreqRegs.pairs).bits.MODE == CB_DISABLE) &&
        // NOTE! DB_SHADER_CONTROL.ALPHA_TO_MASK_DISABLE can change with dynamic state at bind time!
        (HighFreq::GetC<mmDB_SHADER_CONTROL, DB_SHADER_CONTROL>(m_highFreqRegs.pairs).bits.ALPHA_TO_MASK_DISABLE == 1) &&
        (PsWritesUavs() == false);

    // Don't bother trying to support this optimization when dynamic state is used for now.
    if ((pDynamicState != nullptr) && (pDynamicState->enable.alphaToCoverageEnable != 0))
    {
        canEnableDepthOnlyOpt = false;
    }

    return canEnableDepthOnlyOpt;
}

} // namespace Gfx12
} // namespace Pal
