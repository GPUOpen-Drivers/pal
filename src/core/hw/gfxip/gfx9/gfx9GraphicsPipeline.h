/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkPs.h"
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkVs.h"
#include "palPipelineAbiProcessor.h"

namespace Pal
{

class Platform;

namespace Gfx9
{

class ColorBlendState;
class DepthStencilState;
class DepthStencilView;

struct RbPlusPm4Img;

// Represents an "image" of the PM4 commands necessary to write a GFX9 graphics pipeline's non-shader render state to
// hardware.  The required register writes are grouped into sets based on sequential register addresses, so that we can
// minimize the amount of PM4 space needed by setting several registers in each packet.
struct GfxPipelineStateCommonPm4ImgSh
{
    union
    {
        struct
        {
            PM4_ME_SET_SH_REG            hdrSpiShaderLateAllocVs;
            regSPI_SHADER_LATE_ALLOC_VS  spiShaderLateAllocVs;
        } gfx9;

    } lateAlloc;
    // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere w/ the actual
    // commands contained within.
    size_t                              spaceNeeded;
};

// Represents an "image" of the PM4 commands necessary to write a GFX9 graphics pipeline's non-shader render state to
// hardware.  The required register writes are grouped into sets based on sequential register addresses, so that we can
// minimize the amount of PM4 space needed by setting several registers in each packet.
struct GfxPipelineStateCommonPm4ImgContext
{
    PM4_PFP_SET_CONTEXT_REG             hdrVgtShaderStagesEn;
    regVGT_SHADER_STAGES_EN             vgtShaderStagesEn;

    PM4_PFP_SET_CONTEXT_REG             hdrVgtGsMode;
    regVGT_GS_MODE                      vgtGsMode;

    PM4_PFP_SET_CONTEXT_REG             hdrVgtReuseOff;
    regVGT_REUSE_OFF                    vgtReuseOff;

    PM4_PFP_SET_CONTEXT_REG             hdrVgtTfParam;
    regVGT_TF_PARAM                     vgtTfParam;

    PM4_PFP_SET_CONTEXT_REG             hdrCbColorControl;
    regCB_COLOR_CONTROL                 cbColorControl;

    PM4_PFP_SET_CONTEXT_REG             hdrCbShaderTargetMask;
    regCB_TARGET_MASK                   cbTargetMask;
    regCB_SHADER_MASK                   cbShaderMask;

    PM4_PFP_SET_CONTEXT_REG             hdrPaClClipCntl;
    regPA_CL_CLIP_CNTL                  paClClipCntl;

    PM4_PFP_SET_CONTEXT_REG             hdrPaSuVtxCntl;
    regPA_SU_VTX_CNTL                   paSuVtxCntl;

    PM4_PFP_SET_CONTEXT_REG             hdrPaClVteCntl;
    regPA_CL_VTE_CNTL                   paClVteCntl;

    PM4_PFP_SET_CONTEXT_REG             hdrPaScLineCntl;
    regPA_SC_LINE_CNTL                  paScLineCntl;

    PM4_PFP_SET_CONTEXT_REG             hdrSpiInterpControl0;
    regSPI_INTERP_CONTROL_0             spiInterpControl0;

    PM4_ME_CONTEXT_REG_RMW              dbRenderOverrideRmw;

    PM4_PFP_SET_CONTEXT_REG             hdrVgtVertexReuseBlockCntl;
    regVGT_VERTEX_REUSE_BLOCK_CNTL      vgtVertexReuseBlockCntl;

    PM4ME_NON_SAMPLE_EVENT_WRITE        flushDfsm;

    // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere w/ the actual
    // commands contained within.
    size_t                              spaceNeeded;
};

// Represents an "image" of PM4 commands necessary to write streamout information.
struct Pm4ImageStrmout
{
    PM4_PFP_SET_CONTEXT_REG      headerStrmoutCfg;
    regVGT_STRMOUT_CONFIG        vgtStrmoutConfig;
    regVGT_STRMOUT_BUFFER_CONFIG vgtStrmoutBufferConfig;

    struct
    {
        PM4_PFP_SET_CONTEXT_REG      header;
        regVGT_STRMOUT_VTX_STRIDE_0  vgtStrmoutVtxStride;
    } stride[MaxStreamOutTargets];

    // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere w/ the
    // actual commands contained within.
    size_t                       spaceNeeded;
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

    uint32* RequestPrefetch(
        const Pal::PrefetchMgr& prefetchMgr,
        uint32*                 pCmdSpace) const;

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

    regPA_SC_SHADER_CONTROL  PaScShaderControl(uint32  numIndices) const
        { return m_chunkPs.PaScShaderControl(numIndices); }
    regVGT_STRMOUT_CONFIG VgtStrmoutConfig() const { return m_streamoutPm4Cmds.vgtStrmoutConfig; }
    regVGT_STRMOUT_BUFFER_CONFIG VgtStrmoutBufferConfig() const { return m_streamoutPm4Cmds.vgtStrmoutBufferConfig; }
    regVGT_STRMOUT_VTX_STRIDE_0 VgtStrmoutVtxStride(uint32 idx) const
        { return m_streamoutPm4Cmds.stride[idx].vgtStrmoutVtxStride; }

    uint32 EsGsRingItemSize() const { return m_chunkGs.EsGsRingItemSize(); }
    regVGT_GS_ONCHIP_CNTL VgtGsOnchipCntl() const { return m_chunkGs.VgtGsOnchipCntl(); }

    bool IsNgg() const { return (m_statePm4CmdsContext.vgtShaderStagesEn.bits.PRIMGEN_EN != 0); }
    bool IsNggFastLaunch() const;

    bool UsesInnerCoverage() const { return m_chunkPs.UsesInnerCoverage(); }
    bool UsesOffchipParamCache() const { return (m_spiPsInControl.bits.OFFCHIP_PARAM_EN != 0); }
    bool HwStereoRenderingEnabled() const;
    bool HwStereoRenderingUsesMultipleViewports() const;
    bool UsesMultipleViewports() const { return UsesViewportArrayIndex() || HwStereoRenderingUsesMultipleViewports(); }
    bool UsesViewInstancing() const { return (m_signature.viewIdRegAddr[0] != UserDataNotMapped); }

    uint32* WriteShCommands(
        CmdStream*                        pCmdStream,
        uint32*                           pCmdSpace,
        const DynamicGraphicsShaderInfos& graphicsInfo) const;

    uint32* WriteContextCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    uint64 GetContextPm4ImgHash() const { return m_contextPm4ImgHash; }

    void OverrideRbPlusRegistersForRpm(
        SwizzledFormat           swizzledFormat,
        uint32                   slot,
        regSX_PS_DOWNCONVERT*    pSxPsDownconvert,
        regSX_BLEND_OPT_EPSILON* pSxBlendOptEpsilon,
        regSX_BLEND_OPT_CONTROL* pSxBlendOptControl) const;

    uint32 GetVsUserDataBaseOffset() const;

protected:
    virtual ~GraphicsPipeline() { }

    virtual Result HwlInit(
        const GraphicsPipelineCreateInfo& createInfo,
        const AbiProcessor&               abiProcessor) override;

    virtual const ShaderStageInfo* GetShaderStageInfo(ShaderType shaderType) const override;

private:
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
        const AbiProcessor& abiProcessor);
    uint32 ComputeScratchMemorySize(
        const AbiProcessor& abiProcessor) const;

    void SetupSignatureFromElf(
        const AbiProcessor& abiProcessor);
    void SetupSignatureForStageFromElf(
        const AbiProcessor& abiProcessor,
        HwShaderStage       stage);

    // Helper methods used to initialize portions of the pipeline register state:
    void BuildPm4Headers(bool useStreamOutput);
    void InitCommonStateRegisters(
        const GraphicsPipelineCreateInfo& createInfo,
        const AbiProcessor&               abiProcessor);
    void SetupStereoRegisters();

    void SetupIaMultiVgtParam(
        const AbiProcessor& abiProcessor);
    void FixupIaMultiVgtParam(
        bool                   forceWdSwitchOnEop,
        regIA_MULTI_VGT_PARAM* pIaMultiVgtParam) const;

    void SetupNonShaderRegisters(
        const GraphicsPipelineCreateInfo& createInfo,
        const AbiProcessor&               abiProcessor);
    void SetupLateAllocVs(
        const AbiProcessor& abiProcessor);
    void SetupRbPlusRegistersForSlot(
        uint32                   slot,
        uint8                    writeMask,
        SwizzledFormat           swizzledFormat,
        regSX_PS_DOWNCONVERT*    pSxPsDownconvert,
        regSX_BLEND_OPT_EPSILON* pSxBlendOptEpsilon,
        regSX_BLEND_OPT_CONTROL* pSxBlendOptControl) const;

    const GfxIpLevel  m_gfxLevel;
    Device*const      m_pDevice;

    // Images of PM4 commands needed to write this pipeline to hardware: This class contains the images needed for all
    // graphics pipelines: render state, MSAA state.
    GfxPipelineStateCommonPm4ImgSh      m_statePm4CmdsSh;
    GfxPipelineStateCommonPm4ImgContext m_statePm4CmdsContext;
    Pm4ImageStrmout                     m_streamoutPm4Cmds;
    uint64                              m_contextPm4ImgHash;

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
    PipelineChunkHs  m_chunkHs;
    PipelineChunkGs  m_chunkGs;
    PipelineChunkVs  m_chunkVs;
    PipelineChunkPs  m_chunkPs;

    GraphicsPipelineSignature  m_signature;

    // Private structure used to store/load a graphics pipeline object. Does not include the data from the shader.
    // Should correspond to the data members in Pal::Gfx9::GraphicsPipeline.
    struct SerializedData
    {
        GfxPipelineStateCommonPm4ImgSh      renderStatePm4ImgSh;
        GfxPipelineStateCommonPm4ImgContext renderStatePm4ImgContext;
        Pm4ImageStrmout                     streamoutPm4Img;
        uint32                              scratchSize;
        GraphicsPipelineSignature           signature;
        regIA_MULTI_VGT_PARAM               iaMultiVgtParam[NumIaMultiVgtParam];
        regSX_PS_DOWNCONVERT                sxPsDownconvert;
        regSX_BLEND_OPT_EPSILON             sxBlendOptEpsilon;
        regSX_BLEND_OPT_CONTROL             sxBlendOptControl;
        regVGT_LS_HS_CONFIG                 vgtLsHsConfig;
        regSPI_PS_IN_CONTROL                spiPsInControl;
        regSPI_VS_OUT_CONFIG                spiVsOutConfig;
        regPA_SC_MODE_CNTL_1                paScModeCntl1;
        uint64                              contextPm4ImgHash;
    };

    // Used to index into the IA_MULTI_VGT_PARAM array based on dynamic state. This just constructs a flat index
    // directly from the integer representations of the bool inputs (1/0).
    static PAL_INLINE uint32 IaRegIdx(bool forceSwitchOnEop) { return static_cast<uint32>(forceSwitchOnEop); }

    // Returns the target mask of the specified CB target.
    uint8 GetTargetMask(uint32 target) const
    {
        PAL_ASSERT(target < MaxColorTargets);
        return ((m_statePm4CmdsContext.cbTargetMask.u32All >> (target * 4)) & 0xF);
    }

    PAL_DISALLOW_DEFAULT_CTOR(GraphicsPipeline);
    PAL_DISALLOW_COPY_AND_ASSIGN(GraphicsPipeline);
};

} // Gfx9
} // Pal
