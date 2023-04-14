/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6PipelineChunkEsGs.h"
#include "core/hw/gfxip/gfx6/gfx6PipelineChunkLsHs.h"
#include "core/hw/gfxip/gfx6/gfx6PipelineChunkVsPs.h"

namespace Pal
{

class Platform;

namespace Gfx6
{

class ColorBlendState;
class DepthStencilState;
class DepthStencilView;

// Contains information about the pipeline which needs to be passed to the Init methods or between the multiple Init
// phases.
struct GraphicsPipelineLoadInfo
{
    bool    usesOnchipTess;     // Set if the pipeline has a HS and uses on-chip HS.
    bool    usesGs;             // Set if the pipeline has a GS.
    bool    usesOnChipGs;       // Set if the pipeline has a GS and uses on-chip GS.
    uint16  esGsLdsSizeRegGs;   // User-SGPR where the ES/GS ring size in LDS is passed to the GS stage
    uint16  esGsLdsSizeRegVs;   // User-SGPR where the ES/GS ring size in LDS is passed to the VS stage
};

// Contains graphics stage information calculated at pipeline bind time.
struct DynamicStageInfos
{
    DynamicStageInfo ps;
    DynamicStageInfo vs;
    DynamicStageInfo ls;
    DynamicStageInfo hs;
    DynamicStageInfo es;
    DynamicStageInfo gs;
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
// GFX6 graphics pipeline class: implements common GFX6-specific funcionality for the GraphicsPipeline class.  Details
// specific to a particular pipeline configuration (GS-enabled, tessellation-enabled, etc) are offloaded to appropriate
// subclasses.
class GraphicsPipeline final : public Pal::GraphicsPipeline
{
public:
    GraphicsPipeline(Device* pDevice, bool isInternal);

    virtual Result GetShaderStats(
        ShaderType   shaderType,
        ShaderStats* pShaderStats,
        bool         getDisassemblySize) const override;

    uint32* Prefetch(uint32* pCmdSpace) const;

    regPA_SC_MODE_CNTL_1 PaScModeCntl1() const { return m_regs.other.paScModeCntl1; }
    regCB_TARGET_MASK CbTargetMask() const { return m_regs.other.cbTargetMask; }
    regDB_RENDER_OVERRIDE DbRenderOverride() const { return m_regs.other.dbRenderOverride; }
    regPA_CL_CLIP_CNTL PaClClipCntl() const { return m_regs.other.paClClipCntl; }
    regPA_SC_LINE_CNTL PaScLineCntl() const { return m_regs.other.paScLineCntl; }
    regVGT_TF_PARAM VgtTfParam() const { return m_regs.other.vgtTfParam; }
    regCB_COLOR_CONTROL CbColorControl() const { return m_regs.other.cbColorControl; }
    regDB_SHADER_CONTROL DbShaderControl() const { return m_regs.other.dbShaderControl; }
    regIA_MULTI_VGT_PARAM IaMultiVgtParam(bool forceWdSwitchOnEop) const
        { return m_regs.other.iaMultiVgtParam[static_cast<uint32>(forceWdSwitchOnEop)]; }

    regVGT_LS_HS_CONFIG VgtLsHsConfig() const { return m_regs.other.vgtLsHsConfig; }

    bool CanDrawPrimsOutOfOrder(const DepthStencilView*  pDsView,
                                const DepthStencilState* pDepthStencilState,
                                const ColorBlendState*   pBlendState,
                                uint32                   hasActiveQueries,
                                OutOfOrderPrimMode       gfx7EnableOutOfOrderPrimitives) const;

    bool IsOutOfOrderPrimsEnabled() const
        { return m_regs.other.paScModeCntl1.bits.OUT_OF_ORDER_PRIMITIVE_ENABLE; }

    bool UsesStreamOut() const { return m_chunkVsPs.UsesStreamOut(); }

    regVGT_STRMOUT_BUFFER_CONFIG VgtStrmoutBufferConfig() const { return m_chunkVsPs.VgtStrmoutBufferConfig(); }
    regVGT_STRMOUT_VTX_STRIDE_0 VgtStrmoutVtxStride(uint32 idx) const { return m_chunkVsPs.VgtStrmoutVtxStride(idx); }
    regSPI_VS_OUT_CONFIG SpiVsOutConfig() const { return m_chunkVsPs.SpiVsOutConfig(); }
    regSPI_PS_IN_CONTROL SpiPsInControl() const { return m_chunkVsPs.SpiPsInControl(); }

    const GraphicsPipelineSignature& Signature() const { return m_signature; }

    bool UsesViewInstancing() const { return (m_signature.viewIdRegAddr[0] != UserDataNotMapped); }
    bool IsLineStippleTexEnabled() const { return m_chunkVsPs.SpiPsInputEna().bits.LINE_STIPPLE_TEX_ENA != 0; }

    uint32* WriteShCommands(
        CmdStream*                        pCmdStream,
        uint32*                           pCmdSpace,
        const DynamicGraphicsShaderInfos& graphicsInfo) const;

    uint32* WriteContextCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    uint32 GetContextRegHash() const { return m_contextRegHash; }

    void OverrideRbPlusRegistersForRpm(
        SwizzledFormat               swizzledFormat,
        uint32                       slot,
        regSX_PS_DOWNCONVERT__VI*    pSxPsDownconvert,
        regSX_BLEND_OPT_EPSILON__VI* pSxBlendOptEpsilon,
        regSX_BLEND_OPT_CONTROL__VI* pSxBlendOptControl) const;

    void GetRbPlusRegisters(
        bool                         dualSourceBlendEnable,
        regSX_PS_DOWNCONVERT__VI*    pSxPsDownconvert,
        regSX_BLEND_OPT_EPSILON__VI* pSxBlendOptEpsilon,
        regSX_BLEND_OPT_CONTROL__VI* pSxBlendOptControl) const;

    uint32 GetVsUserDataBaseOffset() const;

protected:
    virtual ~GraphicsPipeline() { }

    virtual Result HwlInit(
        const GraphicsPipelineCreateInfo&       createInfo,
        const AbiReader&                        abiReader,
        const Util::PalAbi::CodeObjectMetadata& metadata,
        Util::MsgPackReader*                    pMetadataReader) override;

    virtual const ShaderStageInfo* GetShaderStageInfo(ShaderType shaderType) const override;

private:
    void EarlyInit(
        const Util::PalAbi::CodeObjectMetadata& metadata,
        const RegisterVector&                   registers,
        GraphicsPipelineLoadInfo*               pInfo);

    uint32 CalcMaxWavesPerSh(float maxWavesPerCu) const;

    void CalcDynamicStageInfo(
        const DynamicGraphicsShaderInfo& shaderInfo,
        DynamicStageInfo*                pStageInfo) const;
    void CalcDynamicStageInfos(
        const DynamicGraphicsShaderInfos& graphicsInfo,
        DynamicStageInfos*                pStageInfos) const;

    uint32* WriteContextCommandsSetPath(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    void UpdateRingSizes(
        const Util::PalAbi::CodeObjectMetadata& metadata);
    uint32 ComputeScratchMemorySize(
        const Util::PalAbi::CodeObjectMetadata& metadata) const;

    void SetupSignatureFromElf(
        const Util::PalAbi::CodeObjectMetadata& metadata,
        const RegisterVector&                   registers,
        uint16*                                 pEsGsLdsSizeRegGs,
        uint16*                                 pEsGsLdsSizeRegVs);
    void SetupSignatureForStageFromElf(
        const Util::PalAbi::CodeObjectMetadata& metadata,
        const RegisterVector&                   registers,
        HwShaderStage                           stage,
        uint16*                                 pEsGsLdsSizeReg);

    void SetupCommonRegisters(
        const GraphicsPipelineCreateInfo& createInfo,
        const RegisterVector&             registers,
        PipelineUploader*                 pUploader);
    void SetupNonShaderRegisters(
        const GraphicsPipelineCreateInfo& createInfo,
        const RegisterVector&             registers,
        PipelineUploader*                 pUploader);

    void SetupIaMultiVgtParam(
        const RegisterVector& registers);
    void FixupIaMultiVgtParamOnGfx7Plus(
        bool                   forceWdSwitchOnEop,
        regIA_MULTI_VGT_PARAM* pIaMultiVgtParam) const;

    void SetupLateAllocVs(
        const RegisterVector& registers);

    void SetupRbPlusRegistersForSlot(
        uint32                       slot,
        uint8                        writeMask,
        SwizzledFormat               swizzledFormat,
        regSX_PS_DOWNCONVERT__VI*    pSxPsDownconvert,
        regSX_BLEND_OPT_EPSILON__VI* pSxBlendOptEpsilon,
        regSX_BLEND_OPT_CONTROL__VI* pSxBlendOptControl) const;

    Device*const  m_pDevice;
    uint32        m_contextRegHash;

    // We need two copies of IA_MULTI_VGT_PARAM to cover all possible register combinations depending on whether or not
    // WD_SWITCH_ON_EOP is required.
    static constexpr uint32  NumIaMultiVgtParam = 2;

    PipelineChunkLsHs  m_chunkLsHs;
    PipelineChunkEsGs  m_chunkEsGs;
    PipelineChunkVsPs  m_chunkVsPs;

    struct
    {
        struct
        {
            regSPI_SHADER_LATE_ALLOC_VS__CI__VI  spiShaderLateAllocVs;
        } sh;

        struct
        {
            regVGT_SHADER_STAGES_EN         vgtShaderStagesEn;
            regVGT_GS_MODE                  vgtGsMode;
            regVGT_REUSE_OFF                vgtReuseOff;
            regCB_SHADER_MASK               cbShaderMask;
            regPA_SU_VTX_CNTL               paSuVtxCntl;
            regPA_CL_VTE_CNTL               paClVteCntl;
            regPA_SC_EDGERULE               paScEdgerule;
            regSPI_INTERP_CONTROL_0         spiInterpControl0;
            regVGT_VERTEX_REUSE_BLOCK_CNTL  vgtVertexReuseBlockCntl;
            regDB_ALPHA_TO_MASK             dbAlphaToMask;
        } context;

        struct
        {
            // The registers below are written by the command buffer during draw-time validation, so they are not
            // written in WriteContextCommandsSetPath nor uploaded as part of the LOAD_INDEX path.
            regSX_PS_DOWNCONVERT__VI     sxPsDownconvert;
            regSX_BLEND_OPT_EPSILON__VI  sxBlendOptEpsilon;
            regSX_BLEND_OPT_CONTROL__VI  sxBlendOptControl;

            // Additional RbPlus register set for enable dual source blend dynamically.
            regSX_PS_DOWNCONVERT__VI     sxPsDownconvertDual;
            regSX_BLEND_OPT_EPSILON__VI  sxBlendOptEpsilonDual;
            regSX_BLEND_OPT_CONTROL__VI  sxBlendOptControlDual;

            regVGT_LS_HS_CONFIG          vgtLsHsConfig;
            regPA_SC_MODE_CNTL_1         paScModeCntl1;
            regIA_MULTI_VGT_PARAM        iaMultiVgtParam[NumIaMultiVgtParam];

            // This register is written by the command buffer at draw-time validation. Only some fields are used.
            regDB_RENDER_OVERRIDE        dbRenderOverride;

            // These registers may be modified by pipeline dynamic state and are written at draw-time validation.
            regVGT_TF_PARAM               vgtTfParam;
            regCB_COLOR_CONTROL           cbColorControl;
            regCB_TARGET_MASK             cbTargetMask;
            regPA_CL_CLIP_CNTL            paClClipCntl;
            regPA_SC_LINE_CNTL            paScLineCntl;
            regDB_SHADER_CONTROL          dbShaderControl;
        } other;
    }  m_regs;

    PipelinePrefetchPm4        m_prefetch;
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

} // Gfx6
} // Pal
