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

#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/graphicsPipeline.h"
#include "core/hw/gfxip/gfx12/gfx12Chip.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"
#include "core/hw/gfxip/gfx12/gfx12RegPairHandler.h"
#include "core/hw/gfxip/gfx12/gfx12UserDataLayout.h"
#include <cstddef>

namespace Pal
{
namespace Gfx12
{

class CmdStream;

// Contains the semantic info for interface match
struct SemanticInfo
{
    uint16 semantic;
    uint16 index;
};

// Enumerates the different color-export shader permutations a pipeline can have.
enum class ColorExportShaderType : uint32
{
    Default = 0,            // Default color-export shader.
                            // Most pipelines with color-export shaders will have only this one.
    DualSourceBlendEnable,  // Color-export shader which enables dual-source blending. Only used when the pipeline can
                            // have dual-source blending dynamically enabled or disabled at draw-time.
    Count,                  // Number of color-export shader permutations.
};

// =====================================================================================================================
// GFX12 graphics pipeline class: implements common GFX12-specific functionality for the GraphicsPipeline class. Details
// specific to a particular pipeline configuration (GS-enabled, tessellation-enabled, etc) are offloaded to appropriate
// subclasses.
class GraphicsPipeline : public Pal::GraphicsPipeline
{
public:
    GraphicsPipeline(Device* pDevice, bool isInternal);
    virtual ~GraphicsPipeline();

    uint32* WriteContextAndUConfigCommands(
        const DynamicGraphicsState& dynamicGraphicsInfo,
        GfxState*                   pGfxState,
        SwizzledFormat              swizzledFormat,
        uint32                      targetIndex,
        Gfx12RedundantStateFilter   filterFlags,
        DepthClampMode*             pDepthClampMode,
        regPA_CL_CLIP_CNTL*         pPaClClipCntl,
        uint32*                     pCmdSpace) const;

    uint32* CopyShRegPairsToCmdSpace(
        const DynamicGraphicsShaderInfos& dynamicInfo,
        uint32*                           pCmdSpace) const;

    uint32 GetColorWriteMask() const
    {
        return m_highFreqRegs.pairs[HighFreq::Index(Chip::mmCB_TARGET_MASK)].value;
    }

    regPA_CL_VTE_CNTL PaClVteCntl() const { return LowFreq::GetC<mmPA_CL_VTE_CNTL, PA_CL_VTE_CNTL>(m_lowFreqRegs); }

    regPA_SU_VTX_CNTL PaSuVtxCntl() const { return LowFreq::GetC<mmPA_SU_VTX_CNTL, PA_SU_VTX_CNTL>(m_lowFreqRegs); }

    bool UsesViewInstancing() const { return m_pUserDataLayout->ViewInstancingEnable(); }

    uint16 StrmoutVtxStrideDw(const uint32 idx) const { return m_strmoutVtxStride[idx]; }

    bool IsBinningDisabled() const { return m_isBinningDisabled; }

    bool IsAlphaToCoverage() const { return m_isAlphaToCoverage; }

    bool IsLineStippleTexEnabled() const { return m_highFreqRegs.spiPsInputEna.bits.LINE_STIPPLE_TEX_ENA; }

    const GraphicsUserDataLayout* UserDataLayout() const { return m_pUserDataLayout; }

    gpusize ColorExportGpuVa(
        ColorExportShaderType shaderType = ColorExportShaderType::Default) const
    {
        return m_colorExportAddr[static_cast<uint32>(shaderType)];
    }

    uint32* Prefetch(uint32  prefetchClampSize, uint32* pCmdSpace) const;

    regSPI_PS_INPUT_ENA SpiPsInputEna() const { return m_highFreqRegs.spiPsInputEna; }

    bool UsesInnerCoverage() const
        { return (m_highFreqRegs.spiPsInputEna.bits.COVERAGE_TO_SHADER_SELECT == INPUT_INNER_COVERAGE); }

    bool HwStereoRenderingEnabled() const;
    bool HwStereoRenderingUsesMultipleViewports() const;
    bool UsesMultipleViewports() const { return UsesViewportArrayIndex() || HwStereoRenderingUsesMultipleViewports(); }

    uint32* UpdateMrtSlotAndRbPlusFormatState(
        SwizzledFormat     swizzledFormat,
        uint32             targetIndex,
        regCB_TARGET_MASK* pCbTargetMask,
        uint32*            pCmdSpace) const;

    virtual Result GetShaderStats(
        ShaderType   shaderType,
        ShaderStats* pShaderStats,
        bool         getDisassemblySize) const override;

    const ShaderRingItemSizes& GetShaderRingSize() const { return m_ringSizes; }

    bool CanRbPlusOptimizeDepthOnly(const DynamicGraphicsState* pDynamicState) const;

    DB_SHADER_CONTROL DbShaderControl() const
        { return HighFreq::GetC<mmDB_SHADER_CONTROL, DB_SHADER_CONTROL>(m_highFreqRegs.pairs); }
    bool NoForceReZ() const { return m_noForceReZ; }

protected:
    virtual const ShaderStageInfo* GetShaderStageInfo(
        ShaderType shaderType) const override;

    virtual Result InitDerivedState(
        const GraphicsPipelineCreateInfo&       createInfo,
        const Util::PalAbi::CodeObjectMetadata& metadata,
        const CodeObjectUploader&               uploader,
        const AbiReader&                        abiReader) { return Result::Success; }

    virtual Result LinkGraphicsLibraries(
        const GraphicsPipelineCreateInfo& createInfo) override;

private:
    virtual Result HwlInit(
        const GraphicsPipelineCreateInfo&       createInfo,
        const AbiReader&                        abiReader,
        const Util::PalAbi::CodeObjectMetadata& metadata,
        Util::MsgPackReader*                    pMetadataReader) override;

    Result InitHullShaderState(
        const Util::PalAbi::CodeObjectMetadata& metadata,
        const CodeObjectUploader&               uploader,
        const AbiReader&                        abiReader);
    Result InitGeometryShaderState(
        const Util::PalAbi::CodeObjectMetadata& metadata,
        const CodeObjectUploader&               uploader,
        const AbiReader&                        abiReader,
        GsWaveThrottleCntl                      waveThrottleCntl);
    Result InitPixelShaderState(
        const Util::PalAbi::CodeObjectMetadata& metadata,
        const CodeObjectUploader&               uploader,
        const AbiReader&                        abiReader);
    void InitPixelInterpolants(
        const Util::PalAbi::CodeObjectMetadata& metadata);
    void InitContextState(
        const Util::PalAbi::CodeObjectMetadata& metadata);
    void InitGeCntl(
        const Util::PalAbi::CodeObjectMetadata& metadata);
    void UpdateColorExportState(const GraphicsPipelineCreateInfo& createInfo);
    void UpdateStereoState(const GraphicsPipelineCreateInfo& createInfo);
    void HandleWorkarounds();
    virtual void CalculateOutputNumVertices() override;

    void GenerateHashes();

    void UpdateRingSizes(const Util::PalAbi::CodeObjectMetadata& metadata);
    uint32 ComputeScratchMemorySize(const Util::PalAbi::CodeObjectMetadata& metadata) const;

    void UpdateBinningStatus();
    void LinkContextState(
        const GraphicsPipeline*           pPreRasterLib,
        const GraphicsPipeline*           pPsLib,
        const GraphicsPipeline*           pExpLib);

    void UpdateContextState(const GraphicsPipelineCreateInfo& createInfo);

    uint16 m_strmoutVtxStride[MaxStreamOutTargets];

    uint64 m_lowFreqCtxRegHash;
    uint64 m_medFreqCtxRegHash;
    uint64 m_highFreqCtxRegHash; // Note - This does NOT include PS interpolants!
    uint32 m_numInterpolants;

    uint32 m_hiZRoundVal; // ROUND value that is added (for maxZ) or subtracted (for minZ) when determining Zrange.
                          // This value is added as a chicken bit, in case of precision issues. The value of round is
                          // derived as follows: round = (1 << ROUND) - 1.

    bool m_isBinningDisabled;
    bool m_disableGroupLaunchGuarantee;
    bool m_isAlphaToCoverage;
    bool m_noForceReZ;

    constexpr static uint32  MaxPreFetchRangeCount = 3;
    PrimeGpuCacheRange       m_prefetch[MaxPreFetchRangeCount];
    uint32                   m_prefetchRangeCount;

    // PAL doesn't yet have a public, interface-level user data layout object.  For now, create that Gfx12 object
    // implicitly with each pipeline.
    GraphicsUserDataLayout* m_pUserDataLayout;

    // Shader Stage info for HS/ GS/ PS.
    ShaderStageInfo m_hsStageInfo;
    ShaderStageInfo m_gsStageInfo;
    ShaderStageInfo m_psStageInfo;

    static constexpr uint32 LowFreqRegs[] =
    {
        // Context Registers
        mmGE_NGG_SUBGRP_CNTL,
        mmVGT_GS_INSTANCE_CNT,
        mmPA_CL_VTE_CNTL,
        mmPA_CL_NGG_CNTL,
        mmVGT_REUSE_OFF,
        mmPA_SU_VTX_CNTL,
        mmPA_STEREO_CNTL,
        mmPA_SC_EDGERULE,

        // UConfig Registers
        mmGE_STEREO_CNTL,
        mmVGT_PRIMITIVEID_EN,
    };
    using LowFreq = RegPairHandler<decltype(LowFreqRegs), LowFreqRegs>;

    RegisterValuePair m_lowFreqRegs[LowFreq::Size()];

    static_assert((LowFreq::FirstContextIdx() < LowFreq::FirstOtherIdx()) &&
                  (LowFreq::FirstContextIdx() + LowFreq::NumContext() == LowFreq::FirstOtherIdx()) &&
                  (LowFreq::FirstOtherIdx() + LowFreq::NumOther() == LowFreq::Size()),
                  "LowFreq - expecting Ctx before Uconfig and each range is separate!");

    static constexpr uint32 MedFreqRegs[] =
    {
        // Context Registers
        mmVGT_GS_MAX_VERT_OUT,
        mmPA_SC_SHADER_CONTROL,
        mmVGT_SHADER_STAGES_EN,
        mmPA_SC_LINE_CNTL,
        mmPA_CL_VS_OUT_CNTL,
        mmPA_CL_CLIP_CNTL,
        mmDB_VIEWPORT_CONTROL,
        mmGE_MAX_OUTPUT_PER_SUBGROUP,
        mmVGT_DRAW_PAYLOAD_CNTL,
        mmPA_SC_HISZ_CONTROL,
        mmVGT_TF_PARAM,
        mmVGT_LS_HS_CONFIG,
        mmVGT_HOS_MAX_TESS_LEVEL,
        mmVGT_HOS_MIN_TESS_LEVEL,

        // UConfig Registers
        mmVGT_GS_OUT_PRIM_TYPE,
    };
    using MedFreq = RegPairHandler<decltype(MedFreqRegs), MedFreqRegs>;

    RegisterValuePair m_medFreqRegs[MedFreq::Size()];

    static_assert((MedFreq::FirstContextIdx() < MedFreq::FirstOtherIdx()) &&
                  (MedFreq::FirstContextIdx() + MedFreq::NumContext() == MedFreq::FirstOtherIdx()) &&
                  (MedFreq::FirstOtherIdx() + MedFreq::NumOther() == MedFreq::Size()),
                  "MedFreq - expecting Ctx before Uconfig and each range is separate!");

    static constexpr uint32 HighFreqSetPairsRegs[] =
    {
        // UCONFIG
        mmGE_CNTL,

        // CONTEXT
        mmDB_SHADER_CONTROL,
        mmCB_TARGET_MASK,
        mmCB_SHADER_MASK,
        mmCB_COLOR_CONTROL,
        mmSX_PS_DOWNCONVERT,
        mmSX_BLEND_OPT_EPSILON,
        mmSX_BLEND_OPT_CONTROL,
    };
    using HighFreq = RegPairHandler<decltype(HighFreqSetPairsRegs), HighFreqSetPairsRegs>;

    static_assert((HighFreq::FirstOtherIdx() < HighFreq::FirstContextIdx()) &&
                  (HighFreq::FirstOtherIdx() + HighFreq::NumOther() == HighFreq::FirstContextIdx()) &&
                  (HighFreq::FirstContextIdx() + HighFreq::NumContext() == HighFreq::Size()),
                  "HighFreqSetPairsRegs - expecting UConfig before Ctx and each range is separate!");

    struct HighFreqRegs
    {
        RegisterValuePair     pairs[HighFreq::Size()];

        // The following are written with a single SetSeq packet!
        SPI_PS_IN_CONTROL     spiPsInControl;
        SPI_INTERP_CONTROL_0  spiInterpControl0;
        SPI_SHADER_IDX_FORMAT spiShaderIdxFormat;
        SPI_SHADER_POS_FORMAT spiShaderPosFormat;
        SPI_SHADER_Z_FORMAT   spiShaderZFormat;
        SPI_SHADER_COL_FORMAT spiShaderColFormat;
        SPI_BARYC_CNTL        spiBarycCntl;
        SPI_PS_INPUT_ENA      spiPsInputEna;
        SPI_PS_INPUT_ADDR     spiPsInputAddr;
        SPI_PS_INPUT_CNTL_0   spiPsInputCntl[MaxPsInputSemantics];

        // DO NOT add anything after spiPsInputCntl! Only a portion of this range may be valid and the hashing
        // logic relies on this being last!
    };

    HighFreqRegs m_highFreqRegs;

    static_assert(offsetof(HighFreqRegs, spiPsInputCntl) ==
                  sizeof(HighFreqRegs) - (sizeof(uint32) * MaxPsInputSemantics),
                  "Structure is not laid out properly! Dynamic portion must come at end for hashing!");

    static_assert(Util::CheckSequential({ mmSPI_PS_IN_CONTROL,
                                          mmSPI_INTERP_CONTROL_0,
                                          mmSPI_SHADER_IDX_FORMAT,
                                          mmSPI_SHADER_POS_FORMAT,
                                          mmSPI_SHADER_Z_FORMAT,
                                          mmSPI_SHADER_COL_FORMAT,
                                          mmSPI_BARYC_CNTL,
                                          mmSPI_PS_INPUT_ENA,
                                          mmSPI_PS_INPUT_ADDR,
                                          mmSPI_PS_INPUT_CNTL_0, }) &&
                  (MaxPsInputSemantics == 32) &&
                  (mmSPI_PS_INPUT_CNTL_31 - mmSPI_PS_INPUT_CNTL_0 + 1 == MaxPsInputSemantics),
                  "SPI  regs are not sequential!");
    static_assert(Util::CheckSequential({ offsetof(HighFreqRegs, spiPsInControl),
                                          offsetof(HighFreqRegs, spiInterpControl0),
                                          offsetof(HighFreqRegs, spiShaderIdxFormat),
                                          offsetof(HighFreqRegs, spiShaderPosFormat),
                                          offsetof(HighFreqRegs, spiShaderZFormat),
                                          offsetof(HighFreqRegs, spiShaderColFormat),
                                          offsetof(HighFreqRegs, spiBarycCntl),
                                          offsetof(HighFreqRegs, spiPsInputEna),
                                          offsetof(HighFreqRegs, spiPsInputAddr),
                                          offsetof(HighFreqRegs, spiPsInputCntl), }, sizeof(uint32)),
                  "Storage order of these in HighFreqRegs is important!");

    static constexpr uint32 HullShaderRegs[] =
    {
        mmSPI_SHADER_PGM_RSRC4_HS,
        mmSPI_SHADER_PGM_LO_LS,
        mmSPI_SHADER_PGM_RSRC1_HS,
        mmSPI_SHADER_PGM_RSRC2_HS,
        mmSPI_SHADER_PGM_CHKSUM_HS,
        mmSPI_SHADER_USER_DATA_HS_1,
    };
    using HullShader = RegPairHandler<decltype(HullShaderRegs), HullShaderRegs>;
    static_assert(HullShader::Size() == HullShader::NumSh(), "Only SH regs expected.");
    static_assert(HullShaderRegs[HullShader::Size() - 1] == mmSPI_SHADER_USER_DATA_HS_1,
                  "Expect mmSPI_SHADER_USER_DATA_HS_1 in the end of HullShader.");
    RegisterValuePair m_hullShaderRegs[HullShader::Size()];

    static constexpr uint32 GeomShaderRegs[] =
    {
        mmSPI_SHADER_GS_MESHLET_DIM,
        mmSPI_SHADER_GS_MESHLET_EXP_ALLOC,
        mmSPI_SHADER_GS_MESHLET_CTRL,
        mmSPI_SHADER_PGM_CHKSUM_GS,
        mmSPI_SHADER_PGM_RSRC4_GS,
        mmSPI_SHADER_PGM_LO_ES,
        mmSPI_SHADER_PGM_RSRC1_GS,
        mmSPI_SHADER_PGM_RSRC2_GS,
        mmSPI_SHADER_GS_OUT_CONFIG_PS,
        mmSPI_SHADER_USER_DATA_GS_1,
    };

    using GeomShader = RegPairHandler<decltype(GeomShaderRegs), GeomShaderRegs>;
    static_assert(GeomShader::Size() == GeomShader::NumSh(), "Only SH regs expected.");
    static_assert(GeomShaderRegs[GeomShader::Size() - 1] == mmSPI_SHADER_USER_DATA_GS_1,
                  "Expect mmSPI_SHADER_USER_DATA_GS_1 in the end of GeomShader.");
    RegisterValuePair m_geomShaderRegs[GeomShader::Size()];

    RegisterValuePair m_esGsLdsSize;

    static constexpr uint32 NumGsMeshRegs =
        GeomShader::Index(mmSPI_SHADER_GS_MESHLET_CTRL) - GeomShader::Index(mmSPI_SHADER_GS_MESHLET_DIM) + 1;
    // Please don't move the location of below 3 mesh shader special registers in above table otherwise
    // it impacts the correctness of register write in WriteCommands().
    static_assert(GeomShader::Index(mmSPI_SHADER_GS_MESHLET_DIM)       == 0, "Mesh reg location changed!");
    static_assert(GeomShader::Index(mmSPI_SHADER_GS_MESHLET_EXP_ALLOC) == 1, "Mesh reg location changed!");
    static_assert(GeomShader::Index(mmSPI_SHADER_GS_MESHLET_CTRL)      == 2, "Mesh reg location changed!");

    static constexpr uint32 PixelShaderRegs[] =
    {
        mmSPI_SHADER_PGM_RSRC4_PS,
        mmSPI_SHADER_PGM_LO_PS,
        mmSPI_SHADER_PGM_CHKSUM_PS,
        mmSPI_SHADER_PGM_RSRC1_PS,
        mmSPI_SHADER_PGM_RSRC2_PS,
        mmSPI_SHADER_USER_DATA_PS_1,
    };
    using PixelShader = RegPairHandler<decltype(PixelShaderRegs), PixelShaderRegs>;
    static_assert(PixelShader::Size() == PixelShader::NumSh(), "Only SH regs expected.");
    static_assert(PixelShaderRegs[PixelShader::Size() - 1] == mmSPI_SHADER_USER_DATA_PS_1,
                  "Expect mmSPI_SHADER_USER_DATA_PS_1 in the end of PixelShader.");
    RegisterValuePair m_pixelShaderRegs[PixelShader::Size()];

    // The following registers are written from m_highFreqRegs and m_lowFreqRegs. In some cases, we may override their
    // values in OverrideDynamicState().
    // SH Regs
    static constexpr uint32 DynamicStateOverrideShRegs[] =
    {
        mmSPI_SHADER_PGM_RSRC4_GS,
        mmSPI_SHADER_PGM_RSRC4_PS,
        mmSPI_SHADER_PGM_RSRC4_HS
    };

    // Context Regs
    static constexpr uint32 DynamicStateOverrideCtxRegs[] =
    {
        // MedFreq
        mmPA_SC_LINE_CNTL,
        mmPA_CL_CLIP_CNTL,
        mmVGT_TF_PARAM,
        mmDB_VIEWPORT_CONTROL,

        // HighFreq
        mmCB_TARGET_MASK,
        mmCB_COLOR_CONTROL,
        mmDB_SHADER_CONTROL
    };

    using DynamicStateOverrideSh  = RegPairHandler<decltype(DynamicStateOverrideShRegs), DynamicStateOverrideShRegs>;
    using DynamicStateOverrideCtx = RegPairHandler<decltype(DynamicStateOverrideCtxRegs), DynamicStateOverrideCtxRegs>;

    static_assert(DynamicStateOverrideSh::Size() == DynamicStateOverrideSh::NumSh(),
        "Only SH regs expected (currently).");
    static_assert(DynamicStateOverrideCtx::Size() == DynamicStateOverrideCtx::NumContext(),
        "Only Context regs expected (currently).");

    bool HandleDynamicWavesPerCu(
        const DynamicGraphicsShaderInfos& input,
        RegisterValuePair                 shRegs[DynamicStateOverrideSh::Size()]) const;

    void OverrideDynamicState(
        const DynamicGraphicsState& dynamicState,
        RegisterValuePair           ctxRegs[DynamicStateOverrideCtx::Size()],
        uint32*                     pGfxStateCbTargetMask,
        DepthClampMode*             pGfxStateDepthClampMode) const;

    // These registers are written with the HighFreqRegs but we may re-write (override)
    // their values in some RPM cases during OverrideColorExportRegistersForRpm.
    struct DynamicRpmOverrideRegs
    {
        SX_PS_DOWNCONVERT    sxPsDownconvert;
        SX_BLEND_OPT_EPSILON sxBlendOptEpsilon;
        SX_BLEND_OPT_CONTROL sxBlendOptControl;
        CB_TARGET_MASK       cbTargetMask;
        CB_SHADER_MASK       cbShaderMask;
    };

    void OverrideColorExportRegistersForRpm(SwizzledFormat          swizzledFormat,
                                            uint32                  slot,
                                            DynamicRpmOverrideRegs* pRegs) const;

    SemanticInfo m_semanticInfo[MaxPsInputSemantics];
    uint32       m_semanticCount;

    ShaderRingItemSizes m_ringSizes;

    gpusize m_colorExportAddr[static_cast<uint32>(ColorExportShaderType::Count)];

    // These regs can be impacted by the depth only optimization.
    static constexpr uint32 DepthOnlyOptRegs[] =
    {
        // HighFreq (context regs)
        mmSX_PS_DOWNCONVERT,
        mmSPI_SHADER_COL_FORMAT,
    };

    using DepthOnlyOptRegsCtx = RegPairHandler<decltype(DepthOnlyOptRegs), DepthOnlyOptRegs>;
    static_assert(DepthOnlyOptRegsCtx::Size() == DepthOnlyOptRegsCtx::NumContext(),
        "Only Context regs expected (currently).");

    struct DepthOnlyOptMetadata
    {
        uint32 isCandidate             :  1; // Is this pipeline compatible with depth only opt?
        uint32 origSxDownConvertMrt0   :  4; // SX_DOWN_CONVERT.MRT0 if disabled.
        uint32 origSpiShaderCol0Format :  4; // SPI_SHADER_COL_FORMAT.COL0_EXPORT_FORMAT if disabled.
        uint32 reserved                : 23; // Reserved.
    };

    DepthOnlyOptMetadata m_depthOnlyOptMetadata;

    PAL_DISALLOW_COPY_AND_ASSIGN(GraphicsPipeline);
};

} // namespace Gfx12
} // namespace Pal
