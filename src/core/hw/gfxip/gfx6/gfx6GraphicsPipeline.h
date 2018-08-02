/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palPipelineAbiProcessor.h"

namespace Pal
{

class Platform;

namespace Gfx6
{

class ColorBlendState;
class DepthStencilState;
class DepthStencilView;

// Represents an "image" of the PM4 commands necessary to write a GFX6 graphics pipeline's non-context register render
// state to hardware. The required register writes are grouped into sets based on sequential register addresses, so
// that we can minimize the amount of PM4 space needed by setting several registers in each packet.
struct GfxPipelineStateCommonPm4Img
{
    // This should only be issued on GFX7+.
    // Common registers must be above here and all GFX7 or GFX8 only registers go below.
    PM4CMDSETDATA                       hdrSpiShaderLateAllocVs;
    regSPI_SHADER_LATE_ALLOC_VS__CI__VI spiShaderLateAllocVs;

    // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere w/ the actual
    // commands contained within.
    size_t                              spaceNeeded;
};

// Represents an "image" of the PM4 commands necessary to write GFX6 graphics pipeline's context registers to hardware.
// The required register writes are grouped into sets based on sequential register addresses, so that we can minimize
// the amount of PM4 space needed by setting several registers in each packet.
struct GfxPipelineStateContextPm4Img
{
    PM4CMDSETDATA                       hdrVgtShaderStagesEn;
    regVGT_SHADER_STAGES_EN             vgtShaderStagesEn;

    PM4CMDSETDATA                       hdrVgtGsMode;
    regVGT_GS_MODE                      vgtGsMode;

    PM4CMDSETDATA                       hdrVgtReuseOff;
    regVGT_REUSE_OFF                    vgtReuseOff;

    PM4CMDSETDATA                       hdrVgtTfParam;
    regVGT_TF_PARAM                     vgtTfParam;

    PM4CMDSETDATA                       hdrCbColorControl;
    regCB_COLOR_CONTROL                 cbColorControl;

    PM4CMDSETDATA                       hdrCbShaderTargetMask;
    regCB_TARGET_MASK                   cbTargetMask;
    regCB_SHADER_MASK                   cbShaderMask;

    PM4CMDSETDATA                       hdrPaClClipCntl;
    regPA_CL_CLIP_CNTL                  paClClipCntl;

    PM4CMDSETDATA                       hdrPaSuVtxCntl;
    regPA_SU_VTX_CNTL                   paSuVtxCntl;

    PM4CMDSETDATA                       hdrPaClVteCntl;
    regPA_CL_VTE_CNTL                   paClVteCntl;

    PM4CMDSETDATA                       hdrPaScLineCntl;
    regPA_SC_LINE_CNTL                  paScLineCntl;

    PM4CMDSETDATA                       hdrSpiInterpControl0;
    regSPI_INTERP_CONTROL_0             spiInterpControl0;

    PM4CONTEXTREGRMW                    dbAlphaToMaskRmw;

    // Common register for SI/CI/VI
    PM4CMDSETDATA                       hdrVgtVertexReuseBlockCntl;
    regVGT_VERTEX_REUSE_BLOCK_CNTL      vgtVertexReuseBlockCntl;

    // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere w/ the actual
    // commands contained within.
    size_t                              spaceNeeded;
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
// GFX6 graphics pipeline class: implements common GFX6-specific funcionality for the GraphicsPipeline class.  Details
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

    uint32* RequestPrefetch(
        const Pal::PrefetchMgr& prefetchMgr,
        uint32*                 pCmdSpace) const;

    template <bool pm4OptImmediate>
    uint32* WriteDbShaderControl(
        bool       isDepthEnabled,
        bool       usesOverRasterization,
        CmdStream* pCmdStream,
        uint32*    pCmdSpace) const;

    regPA_SC_MODE_CNTL_1 PaScModeCntl1() const { return m_paScModeCntl1; }

    regIA_MULTI_VGT_PARAM IaMultiVgtParam(bool forceWdSwitchOnEop) const
        { return m_iaMultiVgtParam[IaRegIdx(forceWdSwitchOnEop)]; }

    regVGT_LS_HS_CONFIG VgtLsHsConfig() const { return m_vgtLsHsConfig; }

    bool CanDrawPrimsOutOfOrder(const DepthStencilView*  pDsView,
                                const DepthStencilState* pDepthStencilState,
                                const ColorBlendState*   pBlendState,
                                uint32                   hasActiveQueries,
                                OutOfOrderPrimMode       gfx7EnableOutOfOrderPrimitives) const;

    bool IsOutOfOrderPrimsEnabled() const
        { return m_paScModeCntl1.bits.OUT_OF_ORDER_PRIMITIVE_ENABLE; }

    regVGT_STRMOUT_BUFFER_CONFIG VgtStrmoutBufferConfig() const { return m_chunkVsPs.VgtStrmoutBufferConfig(); }
    regVGT_STRMOUT_VTX_STRIDE_0 VgtStrmoutVtxStride(uint32 idx) const { return m_chunkVsPs.VgtStrmoutVtxStride(idx); }
    regSPI_VS_OUT_CONFIG SpiVsOutConfig() const { return m_chunkVsPs.SpiVsOutConfig(); }
    regSPI_PS_IN_CONTROL SpiPsInControl() const { return m_chunkVsPs.SpiPsInControl(); }

    regSX_PS_DOWNCONVERT__VI SxPsDownconvert() const { return m_sxPsDownconvert; }
    regSX_BLEND_OPT_EPSILON__VI SxBlendOptEpsilon() const { return m_sxBlendOptEpsilon; }
    regSX_BLEND_OPT_CONTROL__VI SxBlendOptControl() const { return m_sxBlendOptControl; }

    const GraphicsPipelineSignature& Signature() const { return m_signature; }

    bool UsesViewInstancing() const { return (m_signature.viewIdRegAddr[0] != UserDataNotMapped); }

    uint32* WriteShCommands(
        CmdStream*                        pCmdStream,
        uint32*                           pCmdSpace,
        const DynamicGraphicsShaderInfos& graphicsInfo) const;

    uint32* WriteContextCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    uint64 GetContextPm4ImgHash() const { return m_contextPm4ImgHash; }

    void OverrideRbPlusRegistersForRpm(
        SwizzledFormat               swizzledFormat,
        uint32                       slot,
        regSX_PS_DOWNCONVERT__VI*    pSxPsDownconvert,
        regSX_BLEND_OPT_EPSILON__VI* pSxBlendOptEpsilon,
        regSX_BLEND_OPT_CONTROL__VI* pSxBlendOptControl) const;

    uint32 GetVsUserDataBaseOffset() const;

protected:
    virtual ~GraphicsPipeline() { }

    virtual Result HwlInit(
        const GraphicsPipelineCreateInfo& createInfo,
        const AbiProcessor&               abiProcessor) override;

    virtual const ShaderStageInfo* GetShaderStageInfo(ShaderType shaderType) const override;

private:
    uint32 CalcMaxWavesPerSh(
        uint32 maxWavesPerCu) const;

    void CalcDynamicStageInfo(
        const DynamicGraphicsShaderInfo& shaderInfo,
        DynamicStageInfo*                pStageInfo
        ) const;
    void CalcDynamicStageInfos(
        const DynamicGraphicsShaderInfos& graphicsInfo,
        DynamicStageInfos*                pStageInfos
        ) const;

    void UpdateRingSizes(
        const AbiProcessor& abiProcessor);
    uint32 ComputeScratchMemorySize(
        const AbiProcessor& abiProcessor) const;

    void SetupSignatureFromElf(
        const AbiProcessor& abiProcessor,
        uint16*             pEsGsLdsSizeRegGs,
        uint16*             pEsGsLdsSizeRegVs);
    void SetupSignatureForStageFromElf(
        const AbiProcessor& abiProcessor,
        HwShaderStage       stage,
        uint16*             pEsGsLdsSizeReg);

    void BuildPm4Headers();
    void InitCommonStateRegisters(
        const GraphicsPipelineCreateInfo& createInfo,
        const AbiProcessor&               abiProcessor);

    void SetupIaMultiVgtParam(
        const AbiProcessor& abiProcessor);
    void FixupIaMultiVgtParamOnGfx7Plus(
        bool                   forceWdSwitchOnEop,
        regIA_MULTI_VGT_PARAM* pIaMultiVgtParam) const;

    void SetupNonShaderRegisters(
        const GraphicsPipelineCreateInfo& createInfo);
    void SetupLateAllocVs(
        const AbiProcessor& abiProcessor);
    void SetupRbPlusRegistersForSlot(
        uint32                       slot,
        uint8                        writeMask,
        SwizzledFormat               swizzledFormat,
        regSX_PS_DOWNCONVERT__VI*    pSxPsDownconvert,
        regSX_BLEND_OPT_EPSILON__VI* pSxBlendOptEpsilon,
        regSX_BLEND_OPT_CONTROL__VI* pSxBlendOptControl) const;

    Device*const  m_pDevice;

    // Images of PM4 commands needed to write this pipeline to hardware: This class contains the images needed for all
    // graphics pipelines: render state, MSAA state.
    GfxPipelineStateCommonPm4Img   m_stateCommonPm4Cmds;
    GfxPipelineStateContextPm4Img  m_stateContextPm4Cmds;
    uint64                         m_contextPm4ImgHash;

    // We need two copies of IA_MULTI_VGT_PARAM to cover all possible register combinations depending on whether or not
    // WD_SWITCH_ON_EOP is required.
    static constexpr uint32  NumIaMultiVgtParam = 2;
    regIA_MULTI_VGT_PARAM  m_iaMultiVgtParam[NumIaMultiVgtParam];

    regSX_PS_DOWNCONVERT__VI     m_sxPsDownconvert;
    regSX_BLEND_OPT_EPSILON__VI  m_sxBlendOptEpsilon;
    regSX_BLEND_OPT_CONTROL__VI  m_sxBlendOptControl;
    regVGT_LS_HS_CONFIG          m_vgtLsHsConfig;
    regPA_SC_MODE_CNTL_1         m_paScModeCntl1;

    PipelineChunkLsHs  m_chunkLsHs;
    PipelineChunkEsGs  m_chunkEsGs;
    PipelineChunkVsPs  m_chunkVsPs;

    GraphicsPipelineSignature  m_signature;

    // Stores information about the depth clamp state
    regDB_RENDER_OVERRIDE m_dbRenderOverride;

    // Private structure used to store/load a graphics pipeline object. Does not include the data from the shader.
    // Should correspond to the data members in Pal::Gfx6::GraphicsPipeline.
    struct SerializedData
    {
        GfxPipelineStateCommonPm4Img  renderStateCommonPm4Img;
        GfxPipelineStateContextPm4Img renderStateContextPm4Img;
        GraphicsPipelineSignature     signature;
        regIA_MULTI_VGT_PARAM         iaMultiVgtParam[NumIaMultiVgtParam];
        regSX_PS_DOWNCONVERT__VI      sxPsDownconvert;
        regSX_BLEND_OPT_EPSILON__VI   sxBlendOptEpsilon;
        regSX_BLEND_OPT_CONTROL__VI   sxBlendOptControl;
        regVGT_LS_HS_CONFIG           vgtLsHsConfig;
        regPA_SC_MODE_CNTL_1          paScModeCntl1;
        uint64                        contextPm4ImgHash;
        uint16                        esGsLdsSizeRegGs;
        uint16                        esGsLdsSizeRegVs;
    };

    // Used to index into the IA_MULTI_VGT_PARAM array based on dynamic state. This just constructs a flat index
    // directly from the integer representations of the bool inputs (1/0).
    static PAL_INLINE uint32 IaRegIdx(bool forceSwitchOnEop) { return static_cast<uint32>(forceSwitchOnEop); }

    // Returns the target mask of the specified CB target.
    uint8 GetTargetMask(uint32 target) const
    {
        PAL_ASSERT(target < MaxColorTargets);
        return ((m_stateContextPm4Cmds.cbTargetMask.u32All >> (target * 4)) & 0xF);
    }

    PAL_DISALLOW_DEFAULT_CTOR(GraphicsPipeline);
    PAL_DISALLOW_COPY_AND_ASSIGN(GraphicsPipeline);
};

} // Gfx6
} // Pal
