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

#include "core/hw/gfxip/graphicsPipeline.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkGs.h"
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkHs.h"
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkVsPs.h"

namespace Pal
{

class Platform;

namespace Gfx9
{

class  ColorBlendState;
class  DepthStencilState;
class  DepthStencilView;
class  GraphicsPipelineUploader;

// Contains information about the pipeline which needs to be passed to the Init methods or between the multiple Init
// phases.
struct GraphicsPipelineLoadInfo
{
    bool    enableNgg;          // Set if the pipeline is using NGG mode.
    bool    usesOnChipGs;       // Set if the pipeline has a GS and uses on-chip GS.
    uint16  esGsLdsSizeRegGs;   // User-SGPR where the ES/GS ring size in LDS is passed to the GS stage
    uint16  esGsLdsSizeRegVs;   // User-SGPR where the ES/GS ring size in LDS is passed to the VS stage
};

// Contains graphics stage information calculated at pipeline bind time.
struct DynamicStageInfos
{
    DynamicStageInfo ps;
    DynamicStageInfo vs;
    DynamicStageInfo gs;
    DynamicStageInfo hs;
};

// We need two copies of IA_MULTI_VGT_PARAM to cover all possible register combinations depending on whether or not
// WD_SWITCH_ON_EOP is required.
constexpr uint32 NumIaMultiVgtParam = 2;

struct GfxPipelineRegs
{
    struct Sh
    {
        regSPI_SHADER_LATE_ALLOC_VS  spiShaderLateAllocVs;
    } sh;

    struct Context
    {
        regVGT_SHADER_STAGES_EN         vgtShaderStagesEn;
        regVGT_GS_MODE                  vgtGsMode;
        regVGT_REUSE_OFF                vgtReuseOff;
        regCB_SHADER_MASK               cbShaderMask;
        regPA_SU_VTX_CNTL               paSuVtxCntl;
        regPA_CL_VTE_CNTL               paClVteCntl;
        regPA_SC_EDGERULE               paScEdgerule;
        regPA_STEREO_CNTL               paStereoCntl;
        regSPI_INTERP_CONTROL_0         spiInterpControl0;
        regVGT_VERTEX_REUSE_BLOCK_CNTL  vgtVertexReuseBlockCntl;
        regCB_COVERAGE_OUT_CONTROL      cbCoverageOutCntl;
        regVGT_GS_ONCHIP_CNTL           vgtGsOnchipCntl;
        regVGT_DRAW_PAYLOAD_CNTL        vgtDrawPayloadCntl;
        regSPI_SHADER_IDX_FORMAT        spiShaderIdxFormat;
        regSPI_SHADER_POS_FORMAT        spiShaderPosFormat;
        regSPI_SHADER_Z_FORMAT          spiShaderZFormat;
        regSPI_SHADER_COL_FORMAT        spiShaderColFormat;
    } context;

    struct
    {
        // The registers below are written by the command buffer during draw-time validation, so they are not
        // written in WriteContextCommandsSetPath nor uploaded as part of the LOAD_INDEX path.
        regSX_PS_DOWNCONVERT     sxPsDownconvert;
        regSX_BLEND_OPT_EPSILON  sxBlendOptEpsilon;
        regSX_BLEND_OPT_CONTROL  sxBlendOptControl;

        // Additional RbPlus register set for enable dual source blend dynamically.
        regSX_PS_DOWNCONVERT     sxPsDownconvertDual;
        regSX_BLEND_OPT_EPSILON  sxBlendOptEpsilonDual;
        regSX_BLEND_OPT_CONTROL  sxBlendOptControlDual;

        regVGT_LS_HS_CONFIG      vgtLsHsConfig;
        regPA_SC_MODE_CNTL_1     paScModeCntl1;
        regIA_MULTI_VGT_PARAM    iaMultiVgtParam[NumIaMultiVgtParam];

        // This register is written by the command buffer at draw-time validation. Only some fields are used.
        regDB_RENDER_OVERRIDE    dbRenderOverride;

        // Note that SPI_VS_OUT_CONFIG and SPI_PS_IN_CONTROL are not written in WriteContextCommands nor
        // uploaded as part of the LOAD_INDEX path.  The reason for this is that the command buffer performs
        // an optimization to avoid context rolls by sometimes sacrificing param-cache space to avoid cases
        // where these two registers' values change at a high frequency between draws.
        regSPI_VS_OUT_CONFIG  spiVsOutConfig;
        regSPI_PS_IN_CONTROL  spiPsInControl;

        // These registers may be modified by pipeline dynamic state and are written at draw-time validation.
        regVGT_TF_PARAM          vgtTfParam;
        regCB_COLOR_CONTROL      cbColorControl;
        regCB_TARGET_MASK        cbTargetMask;
        regPA_CL_CLIP_CNTL       paClClipCntl;
        regPA_SC_LINE_CNTL       paScLineCntl;
    } other;

    struct
    {
        regGE_STEREO_CNTL        geStereoCntl;
        regGE_PC_ALLOC           gePcAlloc;
        regGE_USER_VGPR_EN       geUserVgprEn;
        regVGT_GS_OUT_PRIM_TYPE  vgtGsOutPrimType;
    } uconfig;

    static constexpr uint32 NumContextReg = sizeof(Context) / sizeof(uint32_t);
    static constexpr uint32 NumShReg      = sizeof(Sh)      / sizeof(uint32_t);
};

// =====================================================================================================================
// Converts the specified logic op enum into a ROP3 code (for programming CB_COLOR_CONTROL).
inline uint8 Rop3(
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
// GFX9 graphics pipeline class: implements common GFX9-specific functionality for the GraphicsPipeline class.  Details
// specific to a particular pipeline configuration (GS-enabled, tessellation-enabled, etc) are offloaded to appropriate
// subclasses.
class GraphicsPipeline : public Pal::GraphicsPipeline
{
public:
    GraphicsPipeline(Device* pDevice, bool isInternal);

    virtual Result GetShaderStats(
        ShaderType   shaderType,
        ShaderStats* pShaderStats,
        bool         getDisassemblySize) const override;

    uint32* Prefetch(uint32* pCmdSpace) const;

    regPA_SC_MODE_CNTL_1 PaScModeCntl1() const { return m_regs.other.paScModeCntl1; }

    regIA_MULTI_VGT_PARAM IaMultiVgtParam(bool forceWdSwitchOnEop) const
        { return m_regs.other.iaMultiVgtParam[static_cast<uint32>(forceWdSwitchOnEop)]; }

    regVGT_LS_HS_CONFIG VgtLsHsConfig()   const { return m_regs.other.vgtLsHsConfig;  }
    regSPI_VS_OUT_CONFIG SpiVsOutConfig() const { return m_regs.other.spiVsOutConfig; }
    regSPI_PS_IN_CONTROL SpiPsInControl() const { return m_regs.other.spiPsInControl; }
    regCB_TARGET_MASK CbTargetMask() const { return m_regs.other.cbTargetMask; }
    regCB_COLOR_CONTROL CbColorControl() const { return m_regs.other.cbColorControl; }
    regDB_RENDER_OVERRIDE DbRenderOverride() const { return m_regs.other.dbRenderOverride; }
    regPA_SU_VTX_CNTL PaSuVtxCntl() const { return m_regs.context.paSuVtxCntl; }
    regPA_SC_LINE_CNTL PaScLineCntl() const { return m_regs.other.paScLineCntl; }
    regPA_CL_VTE_CNTL PaClVteCntl() const { return m_regs.context.paClVteCntl; }
    regSPI_PS_INPUT_ENA SpiPsInputEna() const { return m_chunkVsPs.SpiPsInputEna(); }
    regSPI_BARYC_CNTL SpiBarycCntl() const { return m_chunkVsPs.SpiBarycCntl(); }
    regDB_SHADER_CONTROL DbShaderControl() const { return m_chunkVsPs.DbShaderControl(); }
    regVGT_TF_PARAM   VgtTfParam()const { return m_regs.other.vgtTfParam; }
    bool CanDrawPrimsOutOfOrder(const DepthStencilView*  pDsView,
                                const DepthStencilState* pDepthStencilState,
                                const ColorBlendState*   pBlendState,
                                uint32                   hasActiveQueries,
                                OutOfOrderPrimMode       gfx9EnableOutOfOrderPrimitives) const;
    bool PsAllowsPunchout() const;

    bool IsOutOfOrderPrimsEnabled() const
        { return m_regs.other.paScModeCntl1.bits.OUT_OF_ORDER_PRIMITIVE_ENABLE; }

    const GraphicsPipelineSignature& Signature() const { return m_signature; }

    bool UsesStreamout() const { return (m_signature.streamOutTableRegAddr != UserDataNotMapped); }
    bool UsesHwStreamout() const { return m_chunkVsPs.UsesHwStreamout(); }
    uint32 StrmoutVtxStrideDw(uint32 idx) const;
    regPA_SC_AA_CONFIG PaScAaConfig() const { return m_chunkVsPs.PaScAaConfig(); }

    regVGT_GS_ONCHIP_CNTL VgtGsOnchipCntl() const { return m_regs.context.vgtGsOnchipCntl; }
    regVGT_GS_MODE VgtGsMode() const { return m_regs.context.vgtGsMode; }
    regPA_CL_CLIP_CNTL PaClClipCntl() const { return m_regs.other.paClClipCntl; }

    bool   IsNgg() const { return (m_regs.context.vgtShaderStagesEn.bits.PRIMGEN_EN != 0); }
    GsFastLaunchMode FastLaunchMode() const { return m_fastLaunchMode; }
    uint32 NggSubgroupSize() const { return m_nggSubgroupSize; }

    bool UsesInnerCoverage() const { return m_chunkVsPs.UsesInnerCoverage(); }
    bool UsesOffchipParamCache() const { return (m_regs.other.spiPsInControl.bits.OFFCHIP_PARAM_EN != 0); }
    bool HwStereoRenderingEnabled() const;
    bool HwStereoRenderingUsesMultipleViewports() const;
    bool UsesMultipleViewports() const { return UsesViewportArrayIndex() || HwStereoRenderingUsesMultipleViewports(); }
    bool UsesViewInstancing() const { return (m_signature.viewIdRegAddr[0] != UserDataNotMapped); }
    bool UsesUavExport() const { return (m_signature.uavExportTableAddr != UserDataNotMapped); }
    bool NeedsUavExportFlush() const { return m_flags.uavExportRequiresFlush; }
    bool IsLineStippleTexEnabled() const { return m_chunkVsPs.SpiPsInputEna().bits.LINE_STIPPLE_TEX_ENA != 0; }
    uint32* WriteShCommands(
        CmdStream*                        pCmdStream,
        uint32*                           pCmdSpace,
        const DynamicGraphicsShaderInfos& graphicsInfo) const;

    uint32* WriteContextCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const;
    uint32* WriteConfigCommandsGfx10(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    uint32 GetContextRegHash() const { return m_contextRegHash; }
    uint32 GetRbplusRegHash(bool dual) const { return dual ? m_rbplusRegHashDual : m_rbplusRegHash; }
    uint32 GetConfigRegHash() const { return m_configRegHash; }

    void OverrideRbPlusRegistersForRpm(
        SwizzledFormat           swizzledFormat,
        uint32                   slot,
        regSX_PS_DOWNCONVERT*    pSxPsDownconvert,
        regSX_BLEND_OPT_EPSILON* pSxBlendOptEpsilon,
        regSX_BLEND_OPT_CONTROL* pSxBlendOptControl) const;

    void GetRbPlusRegisters(
        bool                     dualSourceBlendEnable,
        regSX_PS_DOWNCONVERT*    pSxPsDownconvert,
        regSX_BLEND_OPT_EPSILON* pSxBlendOptEpsilon,
        regSX_BLEND_OPT_CONTROL* pSxBlendOptControl) const;

    uint32 GetVsUserDataBaseOffset() const;

    static uint32 CalcMaxLateAllocLimit(
        const Device& device,
        uint32        vsNumVgpr,
        uint32        vsNumSgpr,
        uint32        vsWaveSize,
        bool          vsScratchEn,
        bool          psScratchEn,
        uint32        targetLateAllocLimit);

    bool BinningAllowed() const { return m_flags.binningAllowed; }

    uint32 GetPrimAmpFactor() const { return m_primAmpFactor; }

    bool CanRbPlusOptimizeDepthOnly() const;

protected:
    virtual ~GraphicsPipeline() { }

    virtual Result HwlInit(
        const GraphicsPipelineCreateInfo&       createInfo,
        const AbiReader&                        abiReader,
        const Util::PalAbi::CodeObjectMetadata& metadata,
        Util::MsgPackReader*                    pMetadataReader) override;

    virtual const ShaderStageInfo* GetShaderStageInfo(ShaderType shaderType) const override;
    void EarlyInit(const Util::PalAbi::CodeObjectMetadata& metadata,
                   GraphicsPipelineLoadInfo*               pInfo);
    void LateInit(
        const GraphicsPipelineCreateInfo&       createInfo,
        const AbiReader&                        abiReader,
        const Util::PalAbi::CodeObjectMetadata& metadata,
        const GraphicsPipelineLoadInfo&         loadInfo,
        PipelineUploader*                       pUploader);

    Device*const m_pDevice;

private:
    uint32 CalcMaxWavesPerSe(
        float maxWavesPerCu1,
        float maxWavesPerCu2) const;

    uint32 CalcMaxWavesPerSh(
        float maxWavesPerCu1,
        float maxWavesPerCu2) const;

    void CalcDynamicStageInfo(
        const DynamicGraphicsShaderInfo& shaderInfo,
        DynamicStageInfo*                pStageInfo) const;
    void CalcDynamicStageInfo(
        const DynamicGraphicsShaderInfo& shaderInfo1,
        const DynamicGraphicsShaderInfo& shaderInfo2,
        DynamicStageInfo*                pStageInfo) const;
    void CalcDynamicStageInfos(
        const DynamicGraphicsShaderInfos& graphicsInfo,
        DynamicStageInfos*                pStageInfos) const;

    uint32* WriteDynamicRegisters(
        CmdStream*                        pCmdStream,
        uint32*                           pCmdSpace,
        const DynamicGraphicsShaderInfos& graphicsInfo) const;
    uint32* WriteContextCommandsSetPath(CmdStream* pCmdStream, uint32* pCmdSpace) const;
#if PAL_BUILD_GFX11
    void AccumulateContextRegisters(PackedRegisterPair* pRegPairs, uint32* pNumRegs) const;
#endif

    void UpdateRingSizes(
        const Util::PalAbi::CodeObjectMetadata& metadata);
    uint32 ComputeScratchMemorySize(
        const Util::PalAbi::CodeObjectMetadata& metadata) const;

    void SetupSignatureFromElf(
        const Util::PalAbi::CodeObjectMetadata& metadata,
        uint16*                                 pEsGsLdsSizeRegGs,
        uint16*                                 pEsGsLdsSizeRegVs);
    void SetupSignatureForStageFromElf(
        const Util::PalAbi::CodeObjectMetadata& metadata,
        HwShaderStage                           stage,
        uint16*                                 pEsGsLdsSizeReg);

    void SetupCommonRegisters(
        const GraphicsPipelineCreateInfo&       createInfo,
        const Util::PalAbi::CodeObjectMetadata& metadata);
    void SetupNonShaderRegisters(
        const GraphicsPipelineCreateInfo&       createInfo,
        const Util::PalAbi::CodeObjectMetadata& metadata);
    void SetupStereoRegisters();

    void SetupIaMultiVgtParam(
        const Util::PalAbi::CodeObjectMetadata& metadata);
    void FixupIaMultiVgtParam(
        bool                   forceWdSwitchOnEop,
        regIA_MULTI_VGT_PARAM* pIaMultiVgtParam) const;

    void SetupRbPlusRegistersForSlot(
        uint32                   slot,
        uint8                    writeMask,
        SwizzledFormat           swizzledFormat,
        regSX_PS_DOWNCONVERT*    pSxPsDownconvert,
        regSX_BLEND_OPT_EPSILON* pSxBlendOptEpsilon,
        regSX_BLEND_OPT_CONTROL* pSxBlendOptControl) const;

    SX_DOWNCONVERT_FORMAT SxDownConvertFormat(SwizzledFormat swizzledFormat) const;
    void DetermineBinningOnOff();

    const GfxIpLevel  m_gfxLevel;
    uint32            m_contextRegHash;
    uint32            m_rbplusRegHash;
    uint32            m_rbplusRegHashDual;
    uint32            m_configRegHash;
    GsFastLaunchMode  m_fastLaunchMode;
    uint32            m_nggSubgroupSize;
    union
    {
        struct
        {
            uint8 uavExportRequiresFlush      : 1; // If false, must flush after each draw when UAV export is enabled
            uint8 binningAllowed              : 1;
#if PAL_BUILD_GFX11
            uint8 contextPairsPacketSupported : 1;
            uint8 shPairsPacketSupported      : 1;
#else
            uint8 placeholder                 : 2;
#endif
            uint8 reserved                    : 4;
        };
        uint8 u8All;
    } m_flags;
#if PAL_BUILD_GFX11
    uint32            m_strmoutVtxStride[MaxStreamOutTargets];
#endif

    uint32            m_primAmpFactor;      // Only valid on GFX10 and later with NGG enabled.

    // Each pipeline object contains all possibly pipeline chunk sub-objects, even though not every pipeline will
    // actually end up needing them.
    PipelineChunkHs    m_chunkHs;
    PipelineChunkGs    m_chunkGs;
    PipelineChunkVsPs  m_chunkVsPs;

    GfxPipelineRegs m_regs;

    PrimeGpuCacheRange         m_prefetch;
    GraphicsPipelineSignature  m_signature;

    // Returns the target mask of the specified CB target.
    uint8 GetTargetMask(uint32 target) const
    {
        PAL_ASSERT(target < MaxColorTargets);
        return ((m_regs.other.cbTargetMask.u32All >> (target * 4)) & 0xF);
    }

    PAL_DISALLOW_DEFAULT_CTOR(GraphicsPipeline);
    PAL_DISALLOW_COPY_AND_ASSIGN(GraphicsPipeline);
};

} // Gfx9
} // Pal
