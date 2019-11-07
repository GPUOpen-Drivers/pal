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

#pragma once

#include "core/hw/gfxip/graphicsPipeline.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkGs.h"
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkHs.h"
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkVsPs.h"
#include "palPipelineAbiProcessor.h"

namespace Pal
{

class Platform;

namespace Gfx9
{

class  ColorBlendState;
class  DepthStencilState;
class  DepthStencilView;
class  GraphicsPipelineUploader;
struct RbPlusPm4Img;

// Contains information about the pipeline which needs to be passed to the Init methods or between the multiple Init
// phases.
struct GraphicsPipelineLoadInfo
{
    bool    enableNgg;          // Set if the pipeline is using NGG mode.
    bool    usesOnChipGs;       // Set if the pipeline has a GS and uses on-chip GS.
    uint16  esGsLdsSizeRegGs;   // User-SGPR where the ES/GS ring size in LDS is passed to the GS stage
    uint16  esGsLdsSizeRegVs;   // User-SGPR where the ES/GS ring size in LDS is passed to the VS stage
    uint16  interpolatorCount;  // Number of PS interpolators
    uint32  loadedShRegCount;   // Number of SH registers to load using LOAD_SH_REG_INDEX.  If zero, the LOAD_INDEX
                                // path for pipeline binds is not supported.
    uint32  loadedCtxRegCount;  // Number of constext registers to load using LOAD_CONTEXT_REG_INDEX.  If zero, the
                                // LOAD_INDEX path for pipeline binds is not supported.
};
// Contains graphics stage information calculated at pipeline bind time.
struct DynamicStageInfos
{
    DynamicStageInfo ps;
    DynamicStageInfo vs;
    DynamicStageInfo gs;
    DynamicStageInfo hs;
};

// =====================================================================================================================
// GFX9 graphics pipeline class: implements common GFX9-specific funcionality for the GraphicsPipeline class.  Details
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

    regPA_SC_MODE_CNTL_1 PaScModeCntl1() const { return m_paScModeCntl1; }

    void UpdateNggPrimCb(Util::Abi::PrimShaderPsoCb* pPrimShaderCb) const;

    regIA_MULTI_VGT_PARAM IaMultiVgtParam(bool forceWdSwitchOnEop) const
        { return m_iaMultiVgtParam[IaRegIdx(forceWdSwitchOnEop)]; }

    regVGT_LS_HS_CONFIG VgtLsHsConfig()   const { return m_vgtLsHsConfig;  }
    regSPI_VS_OUT_CONFIG SpiVsOutConfig() const { return m_spiVsOutConfig; }
    regSPI_PS_IN_CONTROL SpiPsInControl() const { return m_spiPsInControl; }
    regSX_PS_DOWNCONVERT SxPsDownconvert() const { return m_sxPsDownconvert; }
    regSX_BLEND_OPT_EPSILON SxBlendOptEpsilon() const { return m_sxBlendOptEpsilon; }
    regSX_BLEND_OPT_CONTROL SxBlendOptControl() const { return m_sxBlendOptControl; }
    regCB_TARGET_MASK CbTargetMask() const { return m_commands.set.context.cbTargetMask; }

    bool CanDrawPrimsOutOfOrder(const DepthStencilView*  pDsView,
                                const DepthStencilState* pDepthStencilState,
                                const ColorBlendState*   pBlendState,
                                uint32                   hasActiveQueries,
                                OutOfOrderPrimMode       gfx9EnableOutOfOrderPrimitives) const;
    bool PsTexKill() const;
    bool IsAlphaToMaskEnable() const;
    bool PsCanTriviallyReject() const;
    bool PsAllowsPunchout() const;

    bool IsOutOfOrderPrimsEnabled() const
        { return m_paScModeCntl1.bits.OUT_OF_ORDER_PRIMITIVE_ENABLE; }

    const GraphicsPipelineSignature& Signature() const { return m_signature; }

    regVGT_STRMOUT_CONFIG VgtStrmoutConfig() const { return m_chunkVsPs.VgtStrmoutConfig(); }
    regVGT_STRMOUT_BUFFER_CONFIG VgtStrmoutBufferConfig() const { return m_chunkVsPs.VgtStrmoutBufferConfig(); }
    regVGT_STRMOUT_VTX_STRIDE_0 VgtStrmoutVtxStride(uint32 idx) const { return m_chunkVsPs.VgtStrmoutVtxStride(idx); }

    regVGT_GS_ONCHIP_CNTL VgtGsOnchipCntl() const { return m_commands.set.context.vgtGsOnchipCntl; }

    bool IsNgg() const { return (m_commands.set.context.vgtShaderStagesEn.bits.PRIMGEN_EN != 0); }
    bool IsNggFastLaunch() const { return m_isNggFastLaunch; }

    bool UsesInnerCoverage() const { return m_chunkVsPs.UsesInnerCoverage(); }
    bool UsesOffchipParamCache() const { return (m_spiPsInControl.bits.OFFCHIP_PARAM_EN != 0); }
    bool HwStereoRenderingEnabled() const;
    bool HwStereoRenderingUsesMultipleViewports() const;
    bool UsesMultipleViewports() const { return UsesViewportArrayIndex() || HwStereoRenderingUsesMultipleViewports(); }
    bool UsesViewInstancing() const { return (m_signature.viewIdRegAddr[0] != UserDataNotMapped); }
    bool UsesUavExport() const { return (m_signature.uavExportTableAddr != UserDataNotMapped); }
    bool NeedsUavExportFlush() const { return m_uavExportRequiresFlush; }

    uint32* WriteShCommands(
        CmdStream*                        pCmdStream,
        uint32*                           pCmdSpace,
        const DynamicGraphicsShaderInfos& graphicsInfo) const;

    uint32* WriteContextCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    uint64 GetContextPm4ImgHash() const { return m_contextRegHash; }

    void OverrideRbPlusRegistersForRpm(
        SwizzledFormat           swizzledFormat,
        uint32                   slot,
        regSX_PS_DOWNCONVERT*    pSxPsDownconvert,
        regSX_BLEND_OPT_EPSILON* pSxBlendOptEpsilon,
        regSX_BLEND_OPT_CONTROL* pSxBlendOptControl) const;

    uint32 GetVsUserDataBaseOffset() const;

    static uint32 CalcMaxLateAllocLimit(
        const Device&          device,
        const RegisterVector&  registers,
        uint32                 numVgprs,
        uint32                 numSgprs,
        uint32                 scratchEn,
        uint32                 targetLateAllocLimit);

    bool IsRasterizationKilled() const { return (m_commands.set.context.paClClipCntl.bits.DX_RASTERIZATION_KILL != 0); }

protected:
    virtual ~GraphicsPipeline() { }

    virtual Result HwlInit(
        const GraphicsPipelineCreateInfo& createInfo,
        const AbiProcessor&               abiProcessor,
        const CodeObjectMetadata&         metadata,
        Util::MsgPackReader*              pMetadataReader) override;

    virtual const ShaderStageInfo* GetShaderStageInfo(ShaderType shaderType) const override;

private:
    void EarlyInit(
        const CodeObjectMetadata& metadata,
        const RegisterVector&     registers,
        GraphicsPipelineLoadInfo* pInfo);

    uint32 CalcMaxWavesPerSh(
        uint32 maxWavesPerCu1,
        uint32 maxWavesPerCu2) const;

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

    void UpdateRingSizes(
        const CodeObjectMetadata& metadata);
    uint32 ComputeScratchMemorySize(
        const CodeObjectMetadata& metadata) const;

    void SetupSignatureFromElf(
        const CodeObjectMetadata& metadata,
        const RegisterVector&     registers,
        uint16*                   pEsGsLdsSizeRegGs,
        uint16*                   pEsGsLdsSizeRegVs);
    void SetupSignatureForStageFromElf(
        const CodeObjectMetadata& metadata,
        const RegisterVector&     registers,
        HwShaderStage             stage,
        uint16*                   pEsGsLdsSizeReg);

    void BuildPm4Headers(
        const GraphicsPipelineUploader& uploader);

    void SetupCommonRegisters(
        const GraphicsPipelineCreateInfo& createInfo,
        const RegisterVector&             registers,
        GraphicsPipelineUploader*         pUploader);
    void SetupNonShaderRegisters(
        const GraphicsPipelineCreateInfo& createInfo,
        const RegisterVector&             registers,
        GraphicsPipelineUploader*         pUploader);
    void SetupStereoRegisters();

    void SetupIaMultiVgtParam(
        const RegisterVector& registers);
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

    SX_DOWNCONVERT_FORMAT SxDownConvertFormat(ChNumFormat format) const;

    // Pre-assembled "images" of the PM4 packets used for binding this pipeline to a command buffer.
    struct Pm4Commands
    {
        struct
        {
            PM4_ME_CONTEXT_REG_RMW        dbRenderOverride;
            PM4ME_NON_SAMPLE_EVENT_WRITE  flushDfsm;

            PM4PFP_SET_UCONFIG_REG        hdrGeStereoCntlGePcAlloc;
            regGE_STEREO_CNTL             geStereoCntl;
            regGE_PC_ALLOC                gePcAlloc;

            // The following is only available on Gfx10.1+.
            PM4PFP_SET_UCONFIG_REG        hdrGeUserVgprEn;
            regGE_USER_VGPR_EN            geUserVgprEn;
            // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere
            // w/ the actual commands contained above.
            size_t  spaceNeeded;
        } common; // Packets which are common to both the SET and LOAD_INDEX paths (such as read-modify-writes).

        struct
        {
            struct
            {
                PM4_ME_LOAD_SH_REG_INDEX  loadShRegIndex;
            } sh;

            struct
            {
                PM4_ME_LOAD_CONTEXT_REG_INDEX  loadCtxRegIndex;
            } context;
        } loadIndex; // LOAD_INDEX path, used for GPU's which support the updated microcode.

        PipelinePrefetchPm4 prefetch;

        struct
        {
            struct
            {
                PM4_ME_SET_SH_REG            hdrSpiShaderLateAllocVs;
                regSPI_SHADER_LATE_ALLOC_VS  spiShaderLateAllocVs;
            } sh;

            struct
            {
                PM4_PFP_SET_CONTEXT_REG  hdrVgtShaderStagesEn;
                regVGT_SHADER_STAGES_EN  vgtShaderStagesEn;

                PM4_PFP_SET_CONTEXT_REG  hdrVgtDrawPayloadCntl;
                regVGT_DRAW_PAYLOAD_CNTL vgtDrawPayloadCntl;

                PM4_PFP_SET_CONTEXT_REG  hdrVgtGsMode;
                regVGT_GS_MODE           vgtGsMode;

                PM4_PFP_SET_CONTEXT_REG  hdrVgtReuseOff;
                regVGT_REUSE_OFF         vgtReuseOff;

                PM4_PFP_SET_CONTEXT_REG  hdrVgtTfParam;
                regVGT_TF_PARAM          vgtTfParam;

                PM4_PFP_SET_CONTEXT_REG  hdrCbColorControl;
                regCB_COLOR_CONTROL      cbColorControl;

                PM4_PFP_SET_CONTEXT_REG  hdrCbShaderTargetMask;
                regCB_TARGET_MASK        cbTargetMask;
                regCB_SHADER_MASK        cbShaderMask;

                PM4_PFP_SET_CONTEXT_REG  hdrPaClClipCntl;
                regPA_CL_CLIP_CNTL       paClClipCntl;

                PM4_PFP_SET_CONTEXT_REG  hdrPaSuVtxCntl;
                regPA_SU_VTX_CNTL        paSuVtxCntl;

                PM4_PFP_SET_CONTEXT_REG  hdrPaClVteCntl;
                regPA_CL_VTE_CNTL        paClVteCntl;

                PM4_PFP_SET_CONTEXT_REG  hdrPaScLineCntl;
                regPA_SC_LINE_CNTL       paScLineCntl;

                PM4_PFP_SET_CONTEXT_REG  hdrPaStereoCntl;
                regPA_STEREO_CNTL        paStereoCntl;

                PM4_PFP_SET_CONTEXT_REG  hdrSpiInterpControl0;
                regSPI_INTERP_CONTROL_0  spiInterpControl0;

                PM4_PFP_SET_CONTEXT_REG         hdrVgtVertexReuseBlockCntl;
                regVGT_VERTEX_REUSE_BLOCK_CNTL  vgtVertexReuseBlockCntl;

                PM4PFP_SET_CONTEXT_REG      hdrCbCoverageOutCntl;
                regCB_COVERAGE_OUT_CONTROL  cbCoverageOutCntl;
                PM4_PFP_SET_CONTEXT_REG  hdrVgtGsOnchipCntl;
                regVGT_GS_ONCHIP_CNTL    vgtGsOnchipCntl;

                // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere
                // w/ the actual commands contained above.
                size_t  spaceNeeded;
            } context;
        } set; // SET path, used for GPU's which don't support the updated microcode.
    };

    const GfxIpLevel  m_gfxLevel;
    Device*const      m_pDevice;
    uint64            m_contextRegHash;
    bool              m_isNggFastLaunch; ///< Is NGG fast launch enabled?
    bool              m_uavExportRequiresFlush; // If false, must flush after each draw when UAV export is enabled

    // We need two copies of IA_MULTI_VGT_PARAM to cover all possible register combinations depending on whether or not
    // WD_SWITCH_ON_EOP is required.
    static constexpr uint32  NumIaMultiVgtParam = 2;
    regIA_MULTI_VGT_PARAM    m_iaMultiVgtParam[NumIaMultiVgtParam];

    regSX_PS_DOWNCONVERT     m_sxPsDownconvert;
    regSX_BLEND_OPT_EPSILON  m_sxBlendOptEpsilon;
    regSX_BLEND_OPT_CONTROL  m_sxBlendOptControl;
    regVGT_LS_HS_CONFIG      m_vgtLsHsConfig;
    regSPI_VS_OUT_CONFIG     m_spiVsOutConfig;
    regSPI_PS_IN_CONTROL     m_spiPsInControl;
    regPA_SC_MODE_CNTL_1     m_paScModeCntl1;

    // Each pipeline object contains all possibly pipeline chunk sub-objects, even though not every pipeline will
    // actually end up needing them.
    PipelineChunkHs    m_chunkHs;
    PipelineChunkGs    m_chunkGs;
    PipelineChunkVsPs  m_chunkVsPs;

    GraphicsPipelineSignature  m_signature;
    Pm4Commands                m_commands;

    // Used to index into the IA_MULTI_VGT_PARAM array based on dynamic state. This just constructs a flat index
    // directly from the integer representations of the bool inputs (1/0).
    static PAL_INLINE uint32 IaRegIdx(bool forceSwitchOnEop) { return static_cast<uint32>(forceSwitchOnEop); }

    // Returns the target mask of the specified CB target.
    uint8 GetTargetMask(uint32 target) const
    {
        PAL_ASSERT(target < MaxColorTargets);
        return ((m_commands.set.context.cbTargetMask.u32All >> (target * 4)) & 0xF);
    }

    PAL_DISALLOW_DEFAULT_CTOR(GraphicsPipeline);
    PAL_DISALLOW_COPY_AND_ASSIGN(GraphicsPipeline);
};

// =====================================================================================================================
// Extension of the PipelineUploader helper class for Gfx9+ graphics pipelines.
class GraphicsPipelineUploader : public Pal::PipelineUploader
{
public:
    explicit GraphicsPipelineUploader(
        Device* pDevice,
        uint32  ctxRegisterCount,
        uint32  shRegisterCount)
        :
        PipelineUploader(pDevice->Parent(), ctxRegisterCount, shRegisterCount)
        { }
    virtual ~GraphicsPipelineUploader() { }

    // Add a context register to GPU memory for use with LOAD_CONTEXT_REG_INDEX.
    PAL_INLINE void AddCtxReg(uint16 address, uint32 value)
    {
        PAL_ASSERT(Gfx9::CmdUtil::IsContextReg(address));
        Pal::PipelineUploader::AddCtxRegister(address - CONTEXT_SPACE_START, value);
    }
    template <typename Register_t>
    PAL_INLINE void AddCtxReg(uint16 address, Register_t reg)
        { AddCtxReg(address, reg.u32All); }

    // Add a SH register to GPU memory for use with LOAD_SH_REG_INDEX.
    PAL_INLINE void AddShReg(uint16 address, uint32 value)
    {
        PAL_ASSERT(CmdUtil::IsShReg(address));
        Pal::PipelineUploader::AddShRegister(address - PERSISTENT_SPACE_START, value);
    }
    template <typename Register_t>
    PAL_INLINE void AddShReg(uint16 address, Register_t reg)
        { AddShReg(address, reg.u32All); }

private:
    PAL_DISALLOW_DEFAULT_CTOR(GraphicsPipelineUploader);
    PAL_DISALLOW_COPY_AND_ASSIGN(GraphicsPipelineUploader);
};

} // Gfx9
} // Pal
