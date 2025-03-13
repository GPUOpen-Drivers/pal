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

#include "core/hw/gfxip/universalCmdBuffer.h"
#include "core/hw/gfxip/gfx12/gfx12Chip.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"
#include "core/hw/gfxip/gfx12/gfx12CmdStream.h"
#include "core/hw/gfxip/gfx12/gfx12DepthStencilView.h"
#include "core/hw/gfxip/gfx12/gfx12ColorTargetView.h"
#include "core/hw/gfxip/gfx12/gfx12UserDataLayout.h"
#include "g_gfx12Settings.h"
#include "palIntervalTree.h"
#include "palQueryPool.h"

#include <algorithm>

namespace Pal
{
namespace Gfx12
{

class HybridGraphicsPipeline;
class IndirectCmdGenerator;
class RsrcProcMgr;

// Stride between viewport reg ranges
constexpr uint32 ViewportStride = mmPA_CL_VPORT_XSCALE_1 - mmPA_CL_VPORT_XSCALE;

// Struct which tracks metadata for a pass (CmdBindTargets call)
struct TargetsMetadata
{
    uint8         numMrtsBound;            // Number of MRTs bound during this target bind.
    bool          patchedAlready;          // Has this target bind been patched already?
    uint32*       pCbMemInfoPairsCmdSpace; // Pointer to where the CB_MEMx_INFO reg pairs are for this target bind.
                                           // Bounded by numMrtsBounds pairs (2x DWs).
    const IImage* pImage[MaxColorTargets]; // Per-slot underlying image pointer for each MRT of this target bind.
                                           // Bounded by numMrtsBounds. nullptr for unbound or Buffer type.
};

// Struct which tracks metadata on the currently bound GFX state on the CPU. See m_gfxState declaration for more detail.
struct GfxState
{
    union
    {
        struct
        {
            uint32 firstVertex               :  1; // If set, the firstVertex value in drawArgs is valid.
            uint32 firstInstance             :  1; // If set, the firstInstance value in drawArgs is valid.
            uint32 instanceCount             :  1; // If set, the instanceCount value in drawArgs is valid.
            uint32 drawIndex                 :  1; // If set, the drawIndex value in drawArgs is valid.
            uint32 meshDispatchDims          :  1; // If set, the meshDispatchDims value in drawArgs is valid.
            uint32 indirectDrawArgsHi        :  1; // If set, the indirectDrawArgsHi value in drawArgs is valid.
            uint32 pipelineCtxLowHash        :  1; // If set, the pipeline ctx low hash value is valid.
            uint32 pipelineCtxMedHash        :  1; // If set, the pipeline ctx med hash value is valid.
            uint32 pipelineCtxHighHash       :  1; // If set, the pipeline ctx high hash value is valid.
            uint32 batchBinnerState          :  1; // If set, the batchBinnerState is valid.
            uint32 paScModeCntl1             :  1; // Set when paScModeCntl1 matches the HW value.
            uint32 paSuLineStippleCntl       :  1; // Set when paSuLineStippleCntl matches the HW value.
            uint32 interpCount               :  6; // How many psInterpolants values are valid - [0, 32] range.
            uint32 indexType                 :  1; // If set, m_graphicsState.iaState.indexType is valid.
            uint32 indexIndirectBuffer       :  1; // Set when current index base and buffer size are valid.
            uint32 computeDispatchInterleave :  1; // Set when computeDispatchInterleave is valid.
            uint32 inputAssemblyCtxState     :  1; // Is IAState valid (vgtMultiPrimIbResetIndx/paScLineStippleReset)?
            uint32 paClVrsCntl               :  1; // If set, paClVrsCntl is valid.
            uint32 hiszWorkaround            :  1;
            uint32 cbColor0Info              :  1; // If set, cbColor0Info is valid.
            uint32 dbRenderOverride          :  1; // If set, dbRenderOverride is valid.
            uint32 reserved                  :  6;
        } ;
        uint32 u32All;
    } validBits; // Tracking cases where we're doing redundancy filtering.

    uint16           vertexOffsetReg;     // Register where the vertex start offset is written
    uint16           drawIndexReg;        // Register where the draw index is written
    uint16           meshDispatchDimsReg; // Register where the mesh shader dimension is written
    uint16           nggCullingDataReg;   // Register where the ngg culling data is written.
    MultiUserDataReg viewIdsReg;          // Registers where the view ids are written.

    uint64 pipelineCtxLowPktHash;  // Hash value for the pipeline low frequency context state
    uint64 pipelineCtxMedPktHash;  // Hash value for the pipeline med frequency context state
    uint64 pipelineCtxHighPktHash; // Hash value for the pipeline high frequency context state

    // The drawArgs struct tracks the draw arguments sent with the previous draw to avoid sending
    // them with each draw if they are redundant.  validBits separately tracks if each of those args is known
    // and valid for filtering, and should be reset on a fresh command buffer, when binding a new user data
    // layout or when executing packets that can overwrite these parameters on the GPU (e.g., indirect draws).
    struct
    {
        uint32       firstVertex;
        uint32       firstInstance;
        uint32       instanceCount;
        uint32       drawIndex;
        DispatchDims meshDispatchDims;
        uint32       indirectDrawArgsHi; // Tracks the last GPU address set with SET_BASE to filter redundant packets
    } drawArgs;

    struct
    {
        Chip::BinSizeExtend       binSizeX;
        Chip::BinSizeExtend       binSizeY;
        Chip::PA_SC_BINNER_CNTL_0 paScBinnerCntl0;
    } batchBinnerState;

    Chip::PA_SC_MODE_CNTL_1           paScModeCntl1;
    Chip::PA_SU_LINE_STIPPLE_CNTL     paSuLineStippleCntl;
    Chip::CB_TARGET_MASK              cbTargetMask;
    Chip::COMPUTE_DISPATCH_INTERLEAVE computeDispatchInterleave;
    Chip::DB_STENCIL_WRITE_MASK       dbStencilWriteMask;
    Chip::DB_RENDER_OVERRIDE          dbRenderOverride;
    Chip::DB_STENCIL_CONTROL          dbStencilControl;
    uint8                             dsLog2NumSamples;
    bool                              szValid;

    ShaderHash     pipelinePsHash; // Hash of current pipeline's pixel shader program.

    // This union/structure tracks all states that impact programming of register bitfield
    // PA_SC_MODE_CNTL_1.WALK_ALIGNMENT and PA_SC_MODE_CNTL_1.WALK_ALIGN8_PRIM_FITS_ST.
    union
    {
        struct
        {
            uint32 globalScissorIn64K :  1; // If global scissor is in 64K mode.
            uint32 scissorRectsIn64K  :  1; // If any scissor rect is in 64K mode.
            uint32 targetIn64K        :  1; // If color/depth stencil target is in 64K mode.
            uint32 hasHiSZ            :  1; // If HiZ or HiS is enabled.
            uint32 hasVrsImage        :  1; // If there is VRS image bound to pipeline.
            uint32 dirty              :  1; // If paScModeCntl1 needs to be invalidated.
            uint32 reserved           : 26;
        };
        uint32 u32All;
    } paScWalkAlignState;

    VGT_MULTI_PRIM_IB_RESET_INDX vgtMultiPrimIbResetIndx;
    PA_SC_LINE_STIPPLE_RESET     paScLineStippleReset;

    PA_CL_VRS_CNTL               paClVrsCntl;

    CB_COLOR0_INFO               cbColor0Info;

    // HiZS workaround tracking. We don't need to track whether the register value matches the hardware one.
    bool                              noForceReZ;
    Chip::DB_SHADER_CONTROL           dbShaderControl;

    // Tracks the current known state of PS interpolants for filtering
    SPI_PS_INPUT_CNTL_0 psInterpolants[MaxPsInputSemantics];

    // Primitive shader constant buffer relative with ngg culling.
    Util::Abi::PrimShaderCullingCb primShaderCullingCb;
};

// Distilled, local copy of information passed from the Device object that impacts Gfx12 command buffer generation. This
// includes HW capabilities, panel settings, etc.  The HWL uses this as a creation parameter to ensure the
// HW-independent Gfx12 command buffer is configured appropriately for the targeted hardware and also encourages
// efficient data access patterns during command buffer recording.
struct UniversalCmdBufferDeviceConfig
{
    TossPointMode tossPointMode                  :  3; // TossPointMode panel setting
    uint32        blendOptimizationsEnable       :  1;
    uint32        has32bitPredication            :  1;
    uint32        enablePreamblePipelineStats    :  1;
#if PAL_DEVELOPER_BUILD
    uint32        enablePm4Instrumentation       :  1; // If detailed PM4 instrumentation is enabled.
#else
    uint32        reserved0                      :  1;
#endif
    uint32        disableBorderColorPaletteBinds :  1;
    uint32        issueSqttMarkerEvent           :  1;
    uint32        describeDrawDispatch           :  1;
    uint32        batchBreakOnNewPs              :  1;
    uint32        pwsEnabled                     :  1; // If pixel wait sync is enabled.
    uint32        pwsLateAcquirePointEnabled     :  1; // If use pixel wait sync late acquire point.
    uint32        enableReleaseMemWaitCpDma      :  1;
    uint32        optimizeDepthOnlyFmt           :  1;
    uint32        reserved                       : 17;

    struct
    {
        uint32 walkAlign64kScreenSpace    :  1;
        uint32 drawOpaqueSqNonEvents      :  1;
        uint32 hiszEventBasedWar          :  1;
        uint32 forceReZWhenHiZsDisabledWa :  1;
        uint32 waDbForceStencilValid      :  1;
        uint32 reserved                   : 27;
    } workarounds;

    struct
    {
        // Chip-specific numerators of a quotient used to calculate DPBB bin sizes.  See CalculatePbbBinSizes() for
        // algorithm.
        uint32        colorBinSizeNumerator;
        uint32        depthBinSizeNumerator;
        Extent2d      minBinSize; // Minimum PBB bin size
        Extent2d      maxBinSize; // Maximum PBB bin size
    } pbb;

    uint32                         maxScissorSize;   // Max size for scissors
    Gfx12RedundantStateFilter      stateFilterFlags; // Redundant state filter flags
    uint32                         maxHwScreenOffset;
    uint32                         maxVrsRateCoord;
    gpusize                        prefetchClampSize;
    uint32                         binningMaxPrimPerBatch;
    uint32                         customBatchBinSize;
    DeferredBatchBinMode           binningMode;
    CsDispatchPingPongMode         overrideCsDispatchPingPongMode;
    uint32                         dispatchInterleaveSize2DMinX;
    uint32                         dispatchInterleaveSize2DMinY;
    bool                           allow2dDispatchInterleaveOnIndirectDispatch;
    uint32                         cpPfpVersion;
    Gfx12TemporalHintsIbRead       temporalHintsIbRead;
    Gfx12DynamicCbTemporalHints    dynCbTemporalHints;

    Gfx12TemporalHintsRead     gfx12TemporalHintsMrtRead;
    Gfx12TemporalHintsWrite    gfx12TemporalHintsMrtWrite;
    Gfx12TemporalHintsRead     gfx12TemporalHintsMrtReadBlendReadsDst;
    Gfx12TemporalHintsWrite    gfx12TemporalHintsMrtWriteBlendReadsDst;
    Gfx12TemporalHintsRead     gfx12TemporalHintsMrtReadRaw;
    Gfx12TemporalHintsWrite    gfx12TemporalHintsMrtWriteRaw;
};

// Structure used to store values relating to viewport centering, specifically relevant values of an accumulated
// rectangle surrounding all viewports which aids in efficiently centering viewports in a guardband.
struct VportCenterRect
{
    float centerX; // Center X coordinate
    float centerY; // Center Y coordinate

    float xClipFactor;   // Clip adjust factor, X axis
    float yClipFactor;   // Clip adjust factor, Y axis
};

// =====================================================================================================================
// GFX12 universal command buffer class: implements GFX12 specific functionality for the UniversalCmdBuffer class.
class UniversalCmdBuffer final : public Pal::UniversalCmdBuffer
{
public:
    UniversalCmdBuffer(
        const Device&                         device,
        const CmdBufferCreateInfo&            createInfo,
        const UniversalCmdBufferDeviceConfig& deviceConfig);

    virtual Result Init(const CmdBufferInternalCreateInfo& internalInfo) override;

    virtual void CmdBindPipeline(const PipelineBindParams& params) override;
    virtual void CmdBindPipelineWithOverrides(const PipelineBindParams& params,
                                              SwizzledFormat            swizzledFormat,
                                              uint32                    targetIndex) override;

    virtual void CmdBindMsaaState(const IMsaaState* pMsaaState) override;
    virtual void CmdBindDepthStencilState(const IDepthStencilState* pDepthStencilState) override;
    virtual void CmdBindColorBlendState(const IColorBlendState* pColorBlendState)  override;
    virtual void CmdSetPerDrawVrsRate(const VrsRateParams& rateParams) override;
    virtual void CmdSetVrsCenterState(const VrsCenterState& centerState) override;
    virtual void CmdBindSampleRateImage(const IImage* pImage) override;
    virtual void CmdSetViewports(const ViewportParams& params) override;
    virtual void CmdSetDepthBounds(const DepthBoundsParams& params) override;
    virtual void CmdSetDepthBiasState(const DepthBiasParams& params) override;
    virtual void CmdSetScissorRects(const ScissorRectParams& params) override;
    virtual void CmdSetGlobalScissor(const GlobalScissorParams& params) override;
    virtual void CmdSetInputAssemblyState(const InputAssemblyStateParams& params) override;
    virtual void CmdSetTriangleRasterState(const TriangleRasterStateParams& params) override;
    virtual void CmdBindTargets(const BindTargetParams& params) override;
    virtual void CmdBindIndexData(gpusize gpuAddr, uint32 indexCount, Pal::IndexType indexType) override;
    virtual void CmdSetVertexBuffers(const VertexBufferViews& bufferViews) override;
    virtual void CmdBindBorderColorPalette(
        PipelineBindPoint          pipelineBindPoint,
        const IBorderColorPalette* pPalette) override;
    virtual void CmdSetMsaaQuadSamplePattern(uint32 numSamplesPerPixel,
                                             const MsaaQuadSamplePattern& quadSamplePattern) override;

    virtual void CmdSetBlendConst(const BlendConstParams& params) override;
    virtual void CmdSetPointLineRasterState(const PointLineRasterStateParams& params) override;
    virtual void CmdSetLineStippleState(const LineStippleStateParams& params) override;
    virtual void CmdSetStencilRefMasks(const StencilRefMaskParams& params) override;
    virtual void CmdSetClipRects(uint16 clipRule, uint32 rectCount, const Rect* pRectList) override;
    virtual void CmdSetUserClipPlanes(uint32 firstPlane, uint32 planeCount, const UserClipPlane* pPlanes) override;

    virtual void CmdInsertTraceMarker(PerfTraceMarkerType markerType, uint32 markerData) override;
    virtual void CmdInsertRgpTraceMarker(
        RgpMarkerSubQueueFlags subQueueFlags,
        uint32                 numDwords,
        const void*            pData) override;

    virtual void CmdUpdateSqttTokenMask(const ThreadTraceTokenConfig& sqttTokenConfig) override;

    virtual void CmdUpdateHiSPretests(
        const IImage*      pImage,
        const HiSPretests& pretests,
        uint32             firstMip,
        uint32             numMips) override { }

    virtual void CmdMemoryAtomic(
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset,
        uint64            srcData,
        AtomicOp          atomicOp) override;

    virtual void CmdWriteTimestamp(
        uint32            stageMask,
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset) override;

    virtual void CmdWriteImmediate(
        uint32             stageMask,
        uint64             data,
        ImmediateDataWidth dataSize,
        gpusize            address) override;

    virtual void CmdSaveGraphicsState() override;
    virtual void CmdRestoreGraphicsStateInternal(bool trackBltActiveFlags = true) override;

    virtual void CmdCommentString(const char* pComment) override;
    virtual void CmdNop(
        const void* pPayload,
        uint32      payloadSize) override;

    // This function allows us to dump the contents of this command buffer to a file at submission time.
    virtual void DumpCmdStreamsToFile(Util::File* pFile, CmdBufDumpFormat mode) const override;

    // Universal command buffers support every type of query
    virtual bool IsQueryAllowed(QueryPoolType queryPoolType) const override { return true; }

    virtual void CmdBeginQuery(
        const IQueryPool& queryPool,
        QueryType         queryType,
        uint32            slot,
        QueryControlFlags flags) override;

    virtual void CmdEndQuery(const IQueryPool& queryPool, QueryType queryType, uint32 slot) override;

    virtual void CmdResolveQuery(
        const IQueryPool&     queryPool,
        Pal::QueryResultFlags flags,
        QueryType             queryType,
        uint32                startQuery,
        uint32                queryCount,
        const IGpuMemory&     dstGpuMemory,
        gpusize               dstOffset,
        gpusize               dstStride) override;

    virtual void CmdResetQueryPool(
        const IQueryPool& queryPool,
        uint32            startQuery,
        uint32            queryCount) override;

    virtual void AddQuery(QueryPoolType queryPoolType, QueryControlFlags flags) override;
    virtual void RemoveQuery(QueryPoolType queryPoolType) override;

    void WriteBeginEndOcclusionQueryCmds(gpusize dstAddr);

    Util::IntervalTree<gpusize, bool, Platform>* ActiveOcclusionQueryWriteRanges()
        { return &m_activeOcclusionQueryWriteRanges; }

    virtual void CmdBindStreamOutTargets(const BindStreamOutTargetParams& params) override;
    virtual void CmdLoadBufferFilledSizes(const gpusize(&gpuVirtAddr)[MaxStreamOutTargets]) override;
    virtual void CmdSetBufferFilledSize(uint32  bufferId, uint32  offset) override;
    virtual void CmdSaveBufferFilledSizes(const gpusize(&gpuVirtAddr)[MaxStreamOutTargets]) override;

    virtual void CmdSetPredication(
        IQueryPool*       pQueryPool,
        uint32            slot,
        const IGpuMemory* pGpuMemory,
        gpusize           offset,
        PredicateType     predType,
        bool              predPolarity,
        bool              waitResults,
        bool              accumulateData) override;

    virtual void CmdExecuteNestedCmdBuffers(uint32 cmdBufferCount, ICmdBuffer* const* ppCmdBuffers) override;

    void ValidateExecuteIndirect(
        const IndirectCmdGenerator& generator,
        const bool                  isGfx,
        const uint32                maximumCount,
        const gpusize               countGpuAddr,
        const bool                  isTaskEnabled,
        bool*                       pEnable2dDispatchInterleave);

    void VbUserDataSpillTableHelper(
        const IndirectCmdGenerator& generator,
        const UserDataLayout*       pUserDataLayout,
        const uint32                vertexBufTableDwords,
        const bool                  isGfx,
        gpusize*                    pSpillTableAddress,
        uint32*                     pSpillTableStrideBytes);

    void PreprocessExecuteIndirect(
        const IndirectCmdGenerator& generator,
        const bool                  isGfx,
        const bool                  isTaskEnabled,
        const IPipeline*            pPipeline,
        ExecuteIndirectPacketInfo*  pPacketInfo,
        ExecuteIndirectMeta*        pMeta,
        const EiDispatchOptions&    options,
        const EiUserDataRegs&       regs);

    void ExecuteIndirectPacket(
        const IIndirectCmdGenerator& generator,
        const gpusize                gpuVirtAddr,
        const uint32                 maximumCount,
        const gpusize                countGpuAddr,
        const bool                   isTaskEnabled);

    virtual void CmdExecuteIndirectCmds(
        const IIndirectCmdGenerator& generator,
        gpusize                      gpuVirtAddr,
        uint32                       maximumCount,
        gpusize                      countGpuAddr) override;

    virtual void CmdPrimeGpuCaches(
        uint32                    rangeCount,
        const PrimeGpuCacheRange* pRanges) override;

    virtual void CmdCopyRegisterToMemory(
        uint32            srcRegisterOffset,
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset) override;

    virtual void CmdWaitRegisterValue(
        uint32      registerOffset,
        uint32      data,
        uint32      mask,
        CompareFunc compareFunc) override;

    virtual void CmdWaitMemoryValue(
        gpusize     gpuVirtAddr,
        uint32      data,
        uint32      mask,
        CompareFunc compareFunc) override;

    virtual void CmdIf(
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint64            data,
        uint64            mask,
        CompareFunc       compareFunc) override;

    virtual void CmdElse() override;

    virtual void CmdEndIf() override;

    virtual void CmdWhile(
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint64            data,
        uint64            mask,
        CompareFunc       compareFunc) override;

    virtual void CmdEndWhile() override;

    virtual void CmdBarrier(const BarrierInfo& barrierInfo) override;

    virtual void CmdAcquire(
        const AcquireReleaseInfo& acquireInfo,
        uint32                    syncTokenCount,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 885
        const uint32*             pSyncTokens) override;
#else
        const ReleaseToken*       pSyncTokens) override;
#endif

    virtual void CmdAcquireEvent(
        const AcquireReleaseInfo& acquireInfo,
        uint32                    gpuEventCount,
        const IGpuEvent* const*   ppGpuEvents) override;

    virtual void CmdReleaseThenAcquire(const AcquireReleaseInfo& barrierInfo) override;

    virtual void CopyMemoryCp(gpusize dstAddr, gpusize srcAddr, gpusize numBytes) override;

    // checks if the entire command buffer can be preempted or not
    virtual bool IsPreemptable() const override;

    virtual uint32* WriteWaitEop(WriteWaitEopInfo info, uint32* pCmdSpace) override;
    virtual uint32* WriteWaitCsIdle(uint32* pCmdSpace) override;

    uint32* VerifyStreamoutCtrlBuf(uint32* pCmdSpace);
    gpusize GetStreamoutCtrlBufAddr() const { return m_streamoutCtrlBuf; }

    const ShaderRingItemSizes& GetShaderRingSize() const { return m_ringSizes; }

    size_t GetAceScratchSize() const;

protected:
    CmdStream* GetAceCmdStream();

    virtual void ResetState() override;

    virtual void AddPreamble() override;
    virtual void AddPostamble() override;

    virtual void WriteEventCmd(const BoundGpuMemory& boundMemObj, uint32 stageMask, uint32 data) override;

    virtual void CmdOverwriteColorExportInfoForBlits(
        SwizzledFormat format,
        uint32         targetIndex) override;

    virtual void InheritStateFromCmdBuf(const GfxCmdBuffer* pCmdBuffer) override;

private:
    virtual ~UniversalCmdBuffer();

    // User data function pointers.

    void SwitchDrawFunctions(bool viewInstancingEnable, bool hasTaskShader);

    template <bool IssueSqtt, bool DescribeDrawDispatch>
    void SwitchDrawFunctionsInternal(bool viewInstancingEnable, bool hasTaskShader);

    void SetDispatchFunctions(bool hsaAbi);

    uint32* BuildWriteViewId(uint32 viewId, uint32* pCmdSpace) const;

    // Draw function pointers.
    template <bool IssueSqtt,
              bool ViewInstancingEnable,
              bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDraw(
        ICmdBuffer* pCmdBuffer,
        uint32      firstVertex,
        uint32      vertexCount,
        uint32      firstInstance,
        uint32      instanceCount,
        uint32      drawId);
    template <bool IssueSqtt,
              bool ViewInstancingEnable,
              bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDrawOpaque(
        ICmdBuffer* pCmdBuffer,
        gpusize     streamOutFilledSizeVa,
        uint32      streamOutOffset,
        uint32      stride,
        uint32      firstInstance,
        uint32      instanceCount);
    template <bool IssueSqtt,
              bool ViewInstancingEnable,
              bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDrawIndexed(
        ICmdBuffer* pCmdBuffer,
        uint32      firstIndex,
        uint32      indexCount,
        int32       vertexOffset,
        uint32      firstInstance,
        uint32      instanceCount,
        uint32      drawId);
    template <bool IssueSqtt,
              bool ViewInstancingEnable,
              bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDrawIndirectMulti(
        ICmdBuffer*          pCmdBuffer,
        GpuVirtAddrAndStride gpuVirtAddrAndStride,
        uint32               maximumCount,
        gpusize              countGpuAddr);
    template <bool IssueSqtt,
              bool ViewInstancingEnable,
              bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDrawIndexedIndirectMulti(
        ICmdBuffer*          pCmdBuffer,
        GpuVirtAddrAndStride gpuVirtAddrAndStride,
        uint32               maximumCount,
        gpusize              countGpuAddr);
    template <bool HsaAbi, bool IssueSqtt,
              bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDispatch(
        ICmdBuffer*       pCmdBuffer,
        DispatchDims      size,
        DispatchInfoFlags infoFlags);
    template <bool IssueSqtt,
              bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDispatchIndirect(
        ICmdBuffer* pCmdBuffer,
        gpusize     gpuVirtAddr);
    template <bool HsaAbi, bool IssueSqtt,
              bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDispatchOffset(
        ICmdBuffer*  pCmdBuffer,
        DispatchDims offset,
        DispatchDims launchSize,
        DispatchDims logicalSize);
    template <bool IssueSqtt,
              bool ViewInstancingEnable,
              bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDispatchMesh(
        ICmdBuffer*  pCmdBuffer,
        DispatchDims size);
    template <bool IssueSqtt,
              bool ViewInstancingEnable,
              bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDispatchMeshTask(
        ICmdBuffer*  pCmdBuffer,
        DispatchDims size);
    template <bool IssueSqtt,
              bool ViewInstancingEnable,
              bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDispatchMeshIndirectMulti(
        ICmdBuffer*          pCmdBuffer,
        GpuVirtAddrAndStride gpuVirtAddrAndStride,
        uint32               maximumCount,
        gpusize              countGpuAddr);
    template <bool IssueSqtt,
              bool ViewInstancingEnable,
              bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDispatchMeshIndirectMultiTask(
        ICmdBuffer*          pCmdBuffer,
        GpuVirtAddrAndStride gpuVirtAddrAndStride,
        uint32               maximumCount,
        gpusize              countGpuAddr);

    template <bool IssueSqtt,
              bool ViewInstancingEnable,
              bool DescribeDrawDispatch>
    void CmdDispatchMeshTaskAce(const DispatchDims& size);
    template <bool IssueSqtt,
              bool ViewInstancingEnable,
              bool DescribeDrawDispatch>
    void CmdDispatchMeshIndirectMultiTaskAce(
        GpuVirtAddrAndStride gpuVirtAddrAndStride,
        uint32               maximumCount,
        gpusize              countGpuAddr);
    template <bool IssueSqtt,
              bool ViewInstancingEnable,
              bool DescribeDrawDispatch>
    void CmdDispatchMeshTaskGfx();

    virtual void DeactivateQueryType(QueryPoolType queryPoolType) override;
    virtual void ActivateQueryType(QueryPoolType queryPoolType) override;
    uint32* UpdateDbCountControl (uint32* pDeCmdSpace);

    Pm4Predicate PacketPredicate() const { return static_cast<Pm4Predicate>(m_cmdBufState.flags.packetPredicate); }

    void WriteViewports(uint32 viewportCount);

    uint32* WritePaScModeCntl1(uint32* pDeCmdSpace);

    uint32* WriteSpiPsInputEna(uint32* pDeCmdSpace);

    uint32* UpdateBatchBinnerState(
        Chip::BinningMode   binningMode,
        Chip::BinSizeExtend binSizeX,
        Chip::BinSizeExtend binSizeY,
        uint32*             pCmdSpace);

    template <bool HasPipelineChanged, bool Indirect>
    uint32* ValidateGraphicsPersistentState(
        const ValidateDrawInfo& drawInfo,
        uint32*                 pCmdSpace);

    template <bool HasPipelineChanged
        >
uint32* ValidateGraphicsUserData(
        const GraphicsUserDataLayout* pPrevGfxUserDataLayout,
        const GraphicsUserDataLayout* pCurrentGfxUserDataLayout,
        uint32*                       pCmdSpace);

    template <bool HasPipelineChanged>
    uint32* ValidateComputeUserData(
        UserDataEntries*             pUserDataEntries,
        UserDataTableState*          pUserDataTable,
        const ComputeUserDataLayout* pCurrentComputeUserDataLayout,
        const ComputeUserDataLayout* pPrevComputeUserDataLayout,
        const DispatchDims*          pLogicalSize,
        gpusize                      indirectGpuVirtAddr,
        uint32*                      pCmdSpace);

    void BindTaskShader(
        const GraphicsPipeline*          pNewPipeline,
        const DynamicGraphicsShaderInfo& dynamicInfo,
        uint64                           apiPsoHash);

    void ValidateExecuteNestedCmdBuffer(
        );
    void LeakNestedCmdBufferState(const UniversalCmdBuffer& cmdBuffer);
    void CallNestedCmdBuffer(const UniversalCmdBuffer* pCallee);
    uint32* ValidateDepthOnlyOpt(uint32* pCmdSpace);

    VportCenterRect GetViewportsCenterAndScale() const;

    UniversalCmdBufferDeviceConfig m_deviceConfig;
    const CmdUtil&                 m_cmdUtil;
    const RsrcProcMgr&             m_rsrcProcMgr;
    CmdStream                      m_deCmdStream;
    gpusize                        m_streamoutCtrlBuf;

    TargetsMetadata m_currentTargetsMetadata;
    TargetsMetadata m_previousTargetsMetadata;

    struct
    {
        // Number of VB entries needed by the bound GFX pipeline in DWs.
        uint32              watermarkInDwords : 31;

        // Tracks wether or not the vertex buffer table was modified somewhere in the command buffer.
        // PAL tracks other dirty and modified states using the PM4 layer's GraphicsStateFlags,
        // but vbTable state tracking is currently implemented as part of the hardware layers.
        uint32              modified          :  1;

        // Tracks the contents of the vertex buffer table on the CPU.
        union
        {
            VertexBufferView bufferViews[MaxVertexBuffers];
            sq_buf_rsrc_t    srds[MaxVertexBuffers];
        };

        // Tracks the state for the VB table on the GPU. This must be instanced if the VBs are updated by the app or
        // if a new pipeline is bound which references more VBs.
        UserDataTableState  gpuState;
    } m_vbTable;

    struct
    {
        UserDataTableState  stateGfx;     // Tracks the state of the graphics spill table
        UserDataTableState  stateCompute; // Tracks the state of the compute spill table
        UserDataTableState  stateWg;
    } m_spillTable;

    struct
    {
        sq_buf_rsrc_t       srd[MaxStreamOutTargets]; // Current stream-out target SRD's
        UserDataTableState  state;                    // Tracks the state of the stream-out SRD table
    } m_streamOut;

    struct
    {
        uint32             numSamples; // Number of active MSAA samples.
        UserDataTableState state;      // Tracks the state of the NGG state table
    } m_nggTable;

    struct
    {
        // Used to sync the ACE and DE in a ganged submit.
        gpusize cmdStreamSemAddr;
        uint32  semCountAceWaitDe;
        uint32  semCountDeWaitAce;

    } m_gangSubmitState;

    // In order to prevent invalid query results if an app does Begin()/End(), Reset()/Begin()/End(), Resolve() on a
    // query slot in a command buffer (the first End() might overwrite values written by the Reset()), we have to
    // insert an idle before performing the Reset().  This has a high performance penalty.  This structure is used
    // to track memory ranges affected by outstanding End() calls in this command buffer so we can avoid the idle
    // during Reset() if the reset doesn't affect any pending queries.
    Util::IntervalTree<gpusize, bool, Platform> m_activeOcclusionQueryWriteRanges;

    // The following members track bound state on the host side per-cmdbuf. Any such tracked state becomes invalid
    // after launching GPU-generated work that may change state.
    GfxState m_gfxState;

    // Gets vertex offset register address
    uint16 GetVertexOffsetRegAddr() const { return m_gfxState.vertexOffsetReg; }

    // Gets instance offset register address. It always immediately follows the vertex offset register.
    uint16 GetInstanceOffsetRegAddr() const { return m_gfxState.vertexOffsetReg + 1; }

    // Gets draw index register address.
    uint16 GetDrawIndexRegAddr() const { return m_gfxState.drawIndexReg; }

    // Gets mesh shader dispatch dimension register address.
    uint16 GetMeshDispatchDimRegAddr() const { return m_gfxState.meshDispatchDimsReg; }

    // In CmdBindPipeline, these do _not_ represent the last pipeline passed to CmdBindPipeline,
    // but rather the user data layout of the pipeline used in the last draw or dispatch, respectively.
    const GraphicsUserDataLayout* m_pPrevGfxUserDataLayoutValidatedWith;
    const ComputeUserDataLayout*  m_pPrevComputeUserDataLayoutValidatedWith;

    bool GetDispatchPingPongEn();

    bool m_dispatchPingPongEn;

    bool    m_indirectDispatchArgsValid;
    gpusize m_indirectDispatchArgsAddrHi;

    bool    m_writeCbDbHighBaseRegs;

    template <bool Indirect>
    void ValidateDraw(const ValidateDrawInfo& drawInfo);

    template <bool Indirect, bool IsAce>
    uint32* ValidateDispatchPalAbi(
        uint32*                       pCmdSpace,
        ComputeState*                 pComputeState,
        UserDataTableState*           pUserDataTable,
        const ComputeUserDataLayout*  pCurrentComputeUserDataLayout,
        const ComputeUserDataLayout** ppPrevComputeUserDataLayout,
        const DispatchDims*           pLogicalSize,
        gpusize                       indirectGpuVirtAddr,
        bool                          allow2dDispatchInterleave,
        bool*                         pEnable2dDispatchInterleave);

    uint32* ValidateDispatchHsaAbi(
        DispatchDims        offset,
        const DispatchDims& logicalSize,
        uint32*             pCmdSpace);

    template <bool Indirect>
    uint32* ValidateTaskDispatch(
        uint32*             pCmdSpace,
        const DispatchDims* pLogicalSize,
        gpusize             indirectAddr);

    template <typename... Tn>
    using ViewStorage = typename std::aligned_storage<
        std::max({ sizeof(Tn)... }),
        std::max({ alignof(Tn)... })>::type;

    using DepthStencilViewStorage = ViewStorage<DepthStencilView>;
    using ColorTargetViewStorage  = ViewStorage<ColorTargetView>;

    IDepthStencilView* StoreDepthStencilView(const BindTargetParams& params);
    void CopyDepthStencilViewStorage(DepthStencilViewStorage*       pDepthStencilViewStorageDst,
                                     const DepthStencilViewStorage* pDepthStencilViewStorageSrc,
                                     GraphicsState*                 pGraphicsStateDst);

    IColorTargetView* StoreColorTargetView(uint32 slot, const BindTargetParams& params);
    void CopyColorTargetViewStorage(ColorTargetViewStorage*       pColorTargetViewStorageDst,
                                    const ColorTargetViewStorage* pColorTargetViewStorageSrc,
                                    GraphicsState*                pGraphicsStateDst);

    void    InitAceCmdStream();
    void    AllocGangedCmdStreamSemaphore();
    void    IssueGangedBarrierAceWaitDeIncr();
    void    IssueGangedBarrierDeWaitAceIncr();
    void    TryInitAceGangedSubmitResources();
    uint32* CmdAceWaitDe(uint32* pCmdSpace);
    uint32* CmdDeWaitAce(uint32* pCmdSpace);

    void DescribeDraw(Developer::DrawDispatchType cmdType, bool includedGangedAce = false);
    void AddDrawSqttMarkers(const ValidateDrawInfo& drawInfo);

    void SetShaderRingSize(const ShaderRingItemSizes& ringSizes);

    bool UpdateNggPrimCb(const GraphicsPipeline* pCurrentPipeline,
                         Util::Abi::PrimShaderCullingCb* pPrimShaderCb) const;

    void UpdateNggCullingDataBufferWithCpu();

    uint32* ValidateHiZsWriteWa(const bool              depthAndStencilEn,
                                const bool              depthWriteEn,
                                const bool              stencilWriteEn,
                                const bool              pipelineNoForceReZ,
                                const DB_SHADER_CONTROL dbShaderControl,
                                const DepthStencilView* pDepthStencilView,
                                uint32*                 pDeCmdSpace);

    bool    DepthAndStencilEnabled(bool* pDepthWriteEn, bool* pStencilWriteEn) const;
    uint32* IssueHiSZWarEvent(uint32* pCmdSpace);

    ColorTargetViewStorage  m_colorTargetViewStorage[MaxColorTargets];
    ColorTargetViewStorage  m_colorTargetViewRestoreStorage[MaxColorTargets];
    DepthStencilViewStorage m_depthStencilViewStorage;
    DepthStencilViewStorage m_depthStencilViewRestoreStorage;

    ComputeState* m_pComputeStateAce;

    ShaderRingItemSizes m_ringSizes;

    // When dVGPRs are used in ACE compute queues, we need additional scratch memory.
    // This tracks the extra memory.
    size_t m_dvgprExtraAceScratch;

    // Tracks if there were active occlusion queries the last time DB_COUNT_CONTROL was updated.
    bool m_hasOcclusionQueryActive;

    // This list tracks the set of active pipeline-stats queries which need to have some of their Begin() operations
    // done on the ganged ACE queue.  We generally don't want to initialize that queue whenever a pipeline-stats query
    // begun, so track all such queries which have begun but not yet ended.
    struct ActiveQueryState
    {
        const QueryPool*  pQueryPool;
        uint32            slot;
    };
    Util::Vector<ActiveQueryState, 4, Platform>  m_deferredPipelineStatsQueries;

    PAL_DISALLOW_DEFAULT_CTOR(UniversalCmdBuffer);
    PAL_DISALLOW_COPY_AND_ASSIGN(UniversalCmdBuffer);
};

} // namespace Gfx12
} // namespace Pal
