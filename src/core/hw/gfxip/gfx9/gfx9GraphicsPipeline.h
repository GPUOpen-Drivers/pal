/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
struct RbPlusPm4Img;

// Contains information about the pipeline which needs to be passed to the Init methods or between the multiple Init
// phases.
struct GraphicsPipelineLoadInfo
{
    bool    enableNgg;          // Set if the pipeline is using NGG mode.
    bool    usesOnChipGs;       // Set if the pipeline has a GS and uses on-chip GS.
    uint16  esGsLdsSizeRegGs;   // User-SGPR where the ES/GS ring size in LDS is passed to the GS stage
    uint16  esGsLdsSizeRegVs;   // User-SGPR where the ES/GS ring size in LDS is passed to the VS stage
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

    regPA_SC_MODE_CNTL_1 PaScModeCntl1() const { return m_regs.other.paScModeCntl1; }

    void UpdateNggPrimCb(Util::Abi::PrimShaderCullingCb* pPrimShaderCb) const;

    regIA_MULTI_VGT_PARAM IaMultiVgtParam(bool forceWdSwitchOnEop) const
        { return m_regs.other.iaMultiVgtParam[static_cast<uint32>(forceWdSwitchOnEop)]; }

    regVGT_LS_HS_CONFIG VgtLsHsConfig()   const { return m_regs.other.vgtLsHsConfig;  }
    regSPI_VS_OUT_CONFIG SpiVsOutConfig() const { return m_regs.other.spiVsOutConfig; }
    regSPI_PS_IN_CONTROL SpiPsInControl() const { return m_regs.other.spiPsInControl; }
    regSX_PS_DOWNCONVERT SxPsDownconvert() const { return m_regs.other.sxPsDownconvert; }
    regSX_BLEND_OPT_EPSILON SxBlendOptEpsilon() const { return m_regs.other.sxBlendOptEpsilon; }
    regSX_BLEND_OPT_CONTROL SxBlendOptControl() const { return m_regs.other.sxBlendOptControl; }
    regCB_TARGET_MASK CbTargetMask() const { return m_regs.context.cbTargetMask; }
    regDB_RENDER_OVERRIDE DbRenderOverride() const { return m_regs.other.dbRenderOverride; }

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
        { return m_regs.other.paScModeCntl1.bits.OUT_OF_ORDER_PRIMITIVE_ENABLE; }

    const GraphicsPipelineSignature& Signature() const { return m_signature; }

    regVGT_STRMOUT_CONFIG VgtStrmoutConfig() const { return m_chunkVsPs.VgtStrmoutConfig(); }
    regVGT_STRMOUT_BUFFER_CONFIG VgtStrmoutBufferConfig() const { return m_chunkVsPs.VgtStrmoutBufferConfig(); }
    regVGT_STRMOUT_VTX_STRIDE_0 VgtStrmoutVtxStride(uint32 idx) const { return m_chunkVsPs.VgtStrmoutVtxStride(idx); }
    regPA_SC_AA_CONFIG PaScAaConfig() const { return m_chunkVsPs.PaScAaConfig(); }

    regVGT_GS_ONCHIP_CNTL VgtGsOnchipCntl() const { return m_regs.context.vgtGsOnchipCntl; }
    regVGT_GS_MODE VgtGsMode() const { return m_regs.context.vgtGsMode; }

    bool   IsNgg() const { return (m_regs.context.vgtShaderStagesEn.bits.PRIMGEN_EN != 0); }
    bool   IsNggFastLaunch() const { return m_isNggFastLaunch; }
    uint32 NggSubgroupSize() const { return m_nggSubgroupSize; }

    bool UsesInnerCoverage() const { return m_chunkVsPs.UsesInnerCoverage(); }
    bool UsesOffchipParamCache() const { return (m_regs.other.spiPsInControl.bits.OFFCHIP_PARAM_EN != 0); }
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
    uint32* WriteConfigCommandsGfx10(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    uint32 GetContextRegHash() const { return m_contextRegHash; }
    uint32 GetConfigRegHash() const { return m_configRegHash; }

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

    bool IsRasterizationKilled() const { return (m_regs.context.paClClipCntl.bits.DX_RASTERIZATION_KILL != 0); }

protected:
    virtual ~GraphicsPipeline() { }

    virtual Result HwlInit(
        const GraphicsPipelineCreateInfo& createInfo,
        const AbiReader&                  abiReader,
        const CodeObjectMetadata&         metadata,
        Util::MsgPackReader*              pMetadataReader) override;

    virtual const ShaderStageInfo* GetShaderStageInfo(ShaderType shaderType) const override;
    void EarlyInit(const CodeObjectMetadata& metadata,
                   const RegisterVector&     registers,
                   GraphicsPipelineLoadInfo* pInfo);
    void LateInit(
        const GraphicsPipelineCreateInfo& createInfo,
        const AbiReader&                  abiReader,
        const CodeObjectMetadata&         metadata,
        const RegisterVector&             registers,
        const GraphicsPipelineLoadInfo&   loadInfo,
        GraphicsPipelineUploader*         pUploader);

    Device*const m_pDevice;

private:

    uint32 CalcMaxWavesPerSh(
        float maxWavesPerCu1,
        float maxWavesPerCu2) const;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 604
    uint32 CalcMaxWavesPerSh(
        uint32 maxWavesPerCu1,
        uint32 maxWavesPerCu2) const
    {
        return CalcMaxWavesPerSh(static_cast<float>(maxWavesPerCu1),
                                 static_cast<float>(maxWavesPerCu2));
    }
#endif

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

    uint32* WriteContextCommandsSetPath(CmdStream* pCmdStream, uint32* pCmdSpace) const;

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

    void SetupCommonRegisters(
        const GraphicsPipelineCreateInfo& createInfo,
        const RegisterVector&             registers,
        GraphicsPipelineUploader*         pUploader);
    void SetupNonShaderRegisters(
        const GraphicsPipelineCreateInfo& createInfo,
        const RegisterVector&             registers,
        GraphicsPipelineUploader*         pUploader);
    void SetupStereoRegisters();

    void SetupFetchShaderInfo(const GraphicsPipelineUploader* pUploader);

    uint32* WriteFsShCommands(
        CmdStream* pCmdStream,
        uint32*    pCmdSpace,
        uint32     fetchShaderRegAddr) const;

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

    const GfxIpLevel  m_gfxLevel;
    uint32            m_contextRegHash;
    uint32            m_configRegHash;
    bool              m_isNggFastLaunch; ///< Is NGG fast launch enabled?
    uint32            m_nggSubgroupSize;
    bool              m_uavExportRequiresFlush; // If false, must flush after each draw when UAV export is enabled

    uint16            m_fetchShaderRegAddr; // The user data register which fetch shader address will be writen to.
    gpusize           m_fetchShaderPgm;     // The GPU virtual address of fetch shader entry.

    // We need two copies of IA_MULTI_VGT_PARAM to cover all possible register combinations depending on whether or not
    // WD_SWITCH_ON_EOP is required.
    static constexpr uint32  NumIaMultiVgtParam = 2;

    // Each pipeline object contains all possibly pipeline chunk sub-objects, even though not every pipeline will
    // actually end up needing them.
    PipelineChunkHs    m_chunkHs;
    PipelineChunkGs    m_chunkGs;
    PipelineChunkVsPs  m_chunkVsPs;

    struct
    {
        struct
        {
            regSPI_SHADER_LATE_ALLOC_VS  spiShaderLateAllocVs;
        } sh;

        struct
        {
            regVGT_SHADER_STAGES_EN         vgtShaderStagesEn;
            regVGT_GS_MODE                  vgtGsMode;
            regVGT_REUSE_OFF                vgtReuseOff;
            regVGT_TF_PARAM                 vgtTfParam;
            regCB_COLOR_CONTROL             cbColorControl;
            regCB_TARGET_MASK               cbTargetMask;
            regCB_SHADER_MASK               cbShaderMask;
            regPA_CL_CLIP_CNTL              paClClipCntl;
            regPA_SU_VTX_CNTL               paSuVtxCntl;
            regPA_CL_VTE_CNTL               paClVteCntl;
            regPA_SC_LINE_CNTL              paScLineCntl;
            regPA_STEREO_CNTL               paStereoCntl;
            regSPI_INTERP_CONTROL_0         spiInterpControl0;
            regVGT_VERTEX_REUSE_BLOCK_CNTL  vgtVertexReuseBlockCntl;
            regCB_COVERAGE_OUT_CONTROL      cbCoverageOutCntl;
            regVGT_GS_ONCHIP_CNTL           vgtGsOnchipCntl;
            regVGT_DRAW_PAYLOAD_CNTL        vgtDrawPayloadCntl;
        } context;

        struct
        {
            // The registers below are written by the command buffer during draw-time validation, so they are not
            // written in WriteContextCommandsSetPath nor uploaded as part of the LOAD_INDEX path.
            regSX_PS_DOWNCONVERT     sxPsDownconvert;
            regSX_BLEND_OPT_EPSILON  sxBlendOptEpsilon;
            regSX_BLEND_OPT_CONTROL  sxBlendOptControl;
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
        } other;

        struct
        {
            regGE_STEREO_CNTL   geStereoCntl;
            regGE_PC_ALLOC      gePcAlloc;
            regGE_USER_VGPR_EN  geUserVgprEn;
        } uconfig;
    }  m_regs;

    struct
    {
        gpusize  gpuVirtAddrCtx;
        gpusize  gpuVirtAddrSh;
        uint32   countCtx;
        uint32   countSh;
    }  m_loadPath;

    PipelinePrefetchPm4        m_prefetch;
    GraphicsPipelineSignature  m_signature;

    // Returns the target mask of the specified CB target.
    uint8 GetTargetMask(uint32 target) const
    {
        PAL_ASSERT(target < MaxColorTargets);
        return ((m_regs.context.cbTargetMask.u32All >> (target * 4)) & 0xF);
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
        Device*          pDevice,
        const AbiReader& abiReader,
        uint32           ctxRegisterCount,
        uint32           shRegisterCount)
        :
        PipelineUploader(pDevice->Parent(), abiReader, ctxRegisterCount, shRegisterCount)
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
