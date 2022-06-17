/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/pipeline.h"
#include "core/hw/gfxip/universalCmdBuffer.h"
#include "core/hw/gfxip/gfx6/gfx6Chip.h"
#include "core/hw/gfxip/gfx6/gfx6CmdStream.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6WorkaroundState.h"
#include "palAutoBuffer.h"
#include "palIntervalTree.h"

namespace Pal
{
namespace Gfx6
{

class Device;
class GraphicsPipeline;
class UniversalCmdBuffer;

// Structure to track the state of internal command buffer operations.
struct UniversalCmdBufferState
{
    union
    {
        struct
        {
            uint32 ceStreamDirty        :  1; // A CE RAM Dump command was added to the CE stream since the last Draw
                                              // requires increment & wait on CE counter commands to be added.
            // Tracks whether or not *ANY* piece of ring memory being dumped-to by the CE (by PAL or the client) has
            // wrapped back to the beginning within this command buffer. If no ring has wrapped yet, there is no need
            // to ever stall the CE from getting too far ahead or to ask the DE to invalidate the Kcache for us.
            uint32 ceHasAnyRingWrapped  :  1;
            // CE memory dumps go through the L2 cache, but not the L1 cache! In order for the shader cores to read
            // correct data out of piece of ring memory, we need to occasionally invalidate the Kcache when waiting
            // for the CE to finish dumping its memory. If set, the next INCREMENT_CE_COUNTER inserted into the DE
            // stream should also invalidate the Kcache.
            uint32 ceInvalidateKcache   :  1;
            uint32 ceWaitOnDeCounterDiff:  1;
            uint32 deCounterDirty       :  1;
            uint32 containsDrawIndirect :  1;
            uint32 optimizeLinearGfxCpy :  1;
            uint32 occlusionQueriesActive : 1; // Indicates if the current validated cmd buf state has occulsion
                                               // queries enabled.
            uint32 reserved             : 24;
        };
        uint32 u32All;
    } flags;

    // According to the UDX implementation, CP uCode and CE programming guide, the ideal DE counter diff amount we
    // should ask the CE to wait for is 1/4 the minimum size (in entries!) of all pieces of memory being ringed.
    // Thus we only need to track this minimum diff amount. If ceWaitOnDeCounterDiff flag is also set, the CE will be
    // asked to wait for a DE counter diff at the next Draw or Dispatch.
    uint32  minCounterDiff;
};

// Structure used by UniversalCmdBuffer to track particular bits of hardware state that might need to be updated
// per-draw. Note that the 'valid' flags exist to indicate when we don't know the actual value of certain state. For
// example, we don't know what NUM_INSTANCES is set to at the beginning of a command buffer or after an indirect draw.
// WARNING: If you change anything in here please update ValidateDrawTimeHwState.
struct DrawTimeHwState
{
    union
    {
        struct
        {
            uint32 instanceOffset   :  1; // Set when instanceOffset matches the HW value.
            uint32 vertexOffset     :  1; // Set when vertexOffset matches the HW value.
            uint32 drawIndex        :  1; // Set when drawIndex matches the HW value.
            uint32 numInstances     :  1; // Set when numInstances matches the HW value.
            uint32 vgtLsHsConfig    :  1; // Set when vgtLsHsConfig matches the HW value.
            uint32 iaMultiVgtParam  :  1; // Set when iaMultiVgtParam matches the HW value.
            uint32 paScModeCntl1    :  1; // Set when paScModeCntl1 matches the HW value.
            uint32 reserved         : 25; // Reserved bits
        };
        uint32     u32All;                // The flags as a single integer.
    } valid;                              // Draw state valid flags.

    union
    {
        struct
        {
            uint32 indexType       :  1; // Set when the index type is dirty
            uint32 indexBufferBase :  1; // Set when the index buffer base address is dirty
            uint32 indexBufferSize :  1; // Set when the index buffer size is dirty
            uint32 reserved        : 29; // Reserved bits
        };
        uint32 u32All;                   // The flags as a single integer.
    } dirty;                             // Draw state dirty flags. If any of these are set, the next call to
                                         // ValidateDrawTimeHwState needs to write them.

    uint32                instanceOffset;   // The current value of the instance offset user data.
    uint32                vertexOffset;     // The current value of the vertex offset user data.
    uint32                numInstances;     // The current value of the NUM_INSTANCES state.
    uint32                drawIndex;        // The current value of the draw index user data
    regVGT_LS_HS_CONFIG   vgtLsHsConfig;    // The current value of the VGT_LS_HS_CONFIG register.
    regIA_MULTI_VGT_PARAM iaMultiVgtParam;  // The current value of the IA_MULTI_VGT_PARAM register.
    regPA_SC_MODE_CNTL_1  paScModeCntl1;    // The current value of the PA_SC_MODE_CNTL1 register.
};

// Register state for a single viewport's X,Y,Z scales and offsets.
struct VportScaleOffsetPm4Img
{
    regPA_CL_VPORT_XSCALE  xScale;
    regPA_CL_VPORT_XOFFSET xOffset;
    regPA_CL_VPORT_YSCALE  yScale;
    regPA_CL_VPORT_YOFFSET yOffset;
    regPA_CL_VPORT_ZSCALE  zScale;
    regPA_CL_VPORT_ZOFFSET zOffset;
};

// Register state for a single viewport's Z min and max bounds.
struct VportZMinMaxPm4Img
{
    regPA_SC_VPORT_ZMIN_0 zMin;
    regPA_SC_VPORT_ZMAX_0 zMax;
};

// Register state for the clip guardband.
struct GuardbandPm4Img
{
    regPA_CL_GB_VERT_CLIP_ADJ paClGbVertClipAdj;
    regPA_CL_GB_VERT_DISC_ADJ paClGbVertDiscAdj;
    regPA_CL_GB_HORZ_CLIP_ADJ paClGbHorzClipAdj;
    regPA_CL_GB_HORZ_DISC_ADJ paClGbHorzDiscAdj;
};

// Register state for a single scissor rect.
struct ScissorRectPm4Img
{
    regPA_SC_VPORT_SCISSOR_0_TL tl;
    regPA_SC_VPORT_SCISSOR_0_BR br;
};

// =====================================================================================================================
// GFX6 universal command buffer class: implements GFX6 specific functionality for the UniversalCmdBuffer class.
class UniversalCmdBuffer final : public Pal::UniversalCmdBuffer
{
    // Shorthand for function pointers which validate graphics user-data at draw-time.
    typedef uint32* (UniversalCmdBuffer::*ValidateUserDataGfxFunc)(const GraphicsPipelineSignature*, uint32*);

public:
    static size_t GetSize(const Device& device);

    UniversalCmdBuffer(const Device& device, const CmdBufferCreateInfo& createInfo);

    virtual Result Init(const CmdBufferInternalCreateInfo& internalInfo) override;

    virtual void CmdBindPipeline(
        const PipelineBindParams& params) override;

    virtual void CmdBindIndexData(gpusize gpuAddr, uint32 indexCount, IndexType indexType) override;
    virtual void CmdBindMsaaState(const IMsaaState* pMsaaState) override;
    virtual void CmdSaveGraphicsState() override;
    virtual void CmdRestoreGraphicsState() override;
    virtual void CmdBindColorBlendState(const IColorBlendState* pColorBlendState) override;
    virtual void CmdBindDepthStencilState(const IDepthStencilState* pDepthStencilState) override;

    virtual void CmdSetBlendConst(const BlendConstParams& params) override;
    virtual void CmdSetInputAssemblyState(const InputAssemblyStateParams& params) override;
    virtual void CmdSetStencilRefMasks(const StencilRefMaskParams& params) override;
    virtual void CmdSetDepthBounds(const DepthBoundsParams& params) override;
    virtual void CmdSetTriangleRasterState(const TriangleRasterStateParams& params) override;
    virtual void CmdSetDepthBiasState(const DepthBiasParams& params) override;
    virtual void CmdSetPointLineRasterState(const PointLineRasterStateParams& params) override;
    virtual void CmdSetMsaaQuadSamplePattern(uint32                       numSamplesPerPixel,
                                             const MsaaQuadSamplePattern& quadSamplePattern) override;

    virtual void CmdSetViewports(const ViewportParams& params) override;
    virtual void CmdSetScissorRects(const ScissorRectParams& params) override;
    virtual void CmdSetGlobalScissor(const GlobalScissorParams& params) override;
    virtual void CmdSetUserClipPlanes(uint32               firstPlane,
                                      uint32               planeCount,
                                      const UserClipPlane* pPlanes) override;
    virtual void CmdSetClipRects(uint16      clipRule,
                                 uint32      rectCount,
                                 const Rect* pRectList) override;

    virtual void CmdBarrier(const BarrierInfo& barrierInfo) override;

    virtual void CmdSetVertexBuffers(
        uint32                firstBuffer,
        uint32                bufferCount,
        const BufferViewInfo* pBuffers) override;

    virtual void CmdBindTargets(const BindTargetParams& params) override;
    virtual void CmdBindStreamOutTargets(const BindStreamOutTargetParams& params) override;

    virtual void CmdCloneImageData(const IImage& srcImage, const IImage& dstImage) override;

    virtual void CmdCopyRegisterToMemory(
        uint32            srcRegisterOffset,
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset) override;

    virtual void CmdCopyMemory(
        const IGpuMemory&       srcGpuMemory,
        const IGpuMemory&       dstGpuMemory,
        uint32                  regionCount,
        const MemoryCopyRegion* pRegions) override;

    virtual void CmdUpdateMemory(
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset,
        gpusize           dataSize,
        const uint32*     pData) override;

    virtual void CmdUpdateBusAddressableMemoryMarker(
        const IGpuMemory& dstGpuMemory,
        gpusize           offset,
        uint32            value) override;

    virtual void CmdMemoryAtomic(
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset,
        uint64            srcData,
        AtomicOp          atomicOp) override;

    virtual void CmdWriteTimestamp(HwPipePoint pipePoint, const IGpuMemory& dstGpuMemory, gpusize dstOffset) override;

    virtual void CmdWriteImmediate(
        HwPipePoint        pipePoint,
        uint64             data,
        ImmediateDataWidth dataSize,
        gpusize            address) override;

    virtual void CmdBindBorderColorPalette(
        PipelineBindPoint          pipelineBindPoint,
        const IBorderColorPalette* pPalette) override;

    virtual void CmdInsertTraceMarker(PerfTraceMarkerType markerType, uint32 markerData) override;
    virtual void CmdInsertRgpTraceMarker(
        RgpMarkerSubQueueFlags subQueueFlags,
        uint32                 numDwords,
        const void*            pData) override;

    virtual void AddQuery(QueryPoolType queryPoolType, QueryControlFlags flags) override;
    virtual void RemoveQuery(QueryPoolType queryPoolType) override;

    virtual void CmdLoadBufferFilledSizes(
        const gpusize (&gpuVirtAddr)[MaxStreamOutTargets]) override;

    virtual void CmdSaveBufferFilledSizes(
        const gpusize (&gpuVirtAddr)[MaxStreamOutTargets]) override;

    virtual void CmdSetBufferFilledSize(
        uint32  bufferId,
        uint32  offset) override;

    virtual void CmdBeginQuery(
        const IQueryPool& queryPool,
        QueryType         queryType,
        uint32            slot,
        QueryControlFlags flags) override;

    virtual void CmdEndQuery(const IQueryPool& queryPool, QueryType queryType, uint32 slot) override;

    virtual void CmdResolveQuery(
        const IQueryPool& queryPool,
        QueryResultFlags  flags,
        QueryType         queryType,
        uint32            startQuery,
        uint32            queryCount,
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset,
        gpusize           dstStride) override;

    virtual void CmdResetQueryPool(
        const IQueryPool& queryPool,
        uint32            startQuery,
        uint32            queryCount) override;

    virtual CmdStream* GetCmdStreamByEngine(uint32 engineType) override;

    virtual void CmdUpdateSqttTokenMask(const ThreadTraceTokenConfig& sqttTokenConfig) override;

    virtual void CmdLoadCeRam(
        const IGpuMemory& srcGpuMemory,
        gpusize           memOffset,
        uint32            ramOffset,
        uint32            dwordSize) override;

    virtual void CmdDumpCeRam(
        const IGpuMemory& dstGpuMemory,
        gpusize           memOffset,
        uint32            ramOffset,
        uint32            dwordSize,
        uint32            currRingPos,
        uint32            ringSize) override;

    virtual void CmdWriteCeRam(
        const void* pSrcData,
        uint32      ramOffset,
        uint32      dwordSize) override;

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

    virtual void CmdWaitRegisterValue(
        uint32      registerOffset,
        uint32      data,
        uint32      mask,
        CompareFunc compareFunc) override;

    virtual void CmdWaitMemoryValue(
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint32            data,
        uint32            mask,
        CompareFunc       compareFunc) override;

    virtual void CmdWaitBusAddressableMemoryMarker(
        const IGpuMemory& gpuMemory,
        uint32            data,
        uint32            mask,
        CompareFunc       compareFunc) override;

    virtual void CmdSetPredication(
        IQueryPool*         pQueryPool,
        uint32              slot,
        const IGpuMemory*   pGpuMemory,
        gpusize             offset,
        PredicateType       predType,
        bool                predPolarity,
        bool                waitResults,
        bool                accumulateData) override;

    virtual void CmdExecuteNestedCmdBuffers(
        uint32            cmdBufferCount,
        ICmdBuffer*const* ppCmdBuffers) override;

    virtual void CmdExecuteIndirectCmds(
        const IIndirectCmdGenerator& generator,
        const IGpuMemory&            gpuMemory,
        gpusize                      offset,
        uint32                       maximumCount,
        gpusize                      countGpuAddr) override;

    virtual void CmdCommentString(
        const char* pComment) override;
    virtual void CmdNop(
        const void* pPayload,
        uint32      payloadSize) override;

    virtual uint32 CmdInsertExecutionMarker() override;

    virtual void GetChunkForCmdGeneration(
        const Pal::IndirectCmdGenerator& generator,
        const Pal::Pipeline&             pipeline,
        uint32                           maxCommands,
        uint32                           numChunkOutputs,
        ChunkOutput*                     pChunkOutputs) override;

    Util::IntervalTree<gpusize, bool, Platform>* ActiveOcclusionQueryWriteRanges()
        { return &m_activeOcclusionQueryWriteRanges; }

    void CmdSetTriangleRasterStateInternal(
        const TriangleRasterStateParams& params,
        bool                             optimizeLinearDestGfxCopy);

    virtual void AddPerPresentCommands(
        gpusize frameCountGpuAddr,
        uint32  frameCntReg) override;

    virtual void CmdOverwriteRbPlusFormatForBlits(
        SwizzledFormat format,
        uint32         targetIndex) override;

    virtual void CmdUpdateHiSPretests(
        const IImage*      pImage,
        const HiSPretests& pretests,
        uint32             firstMip,
        uint32             numMips) override;

    virtual void CpCopyMemory(gpusize dstAddr, gpusize srcAddr, gpusize numBytes) override;

protected:
    virtual ~UniversalCmdBuffer() {}

    virtual Result AddPreamble() override;
    virtual Result AddPostamble() override;

    virtual void BeginExecutionMarker(uint64 clientHandle) override;
    virtual void EndExecutionMarker() override;

    virtual void ResetState() override;

    virtual void WriteEventCmd(const BoundGpuMemory& boundMemObj, HwPipePoint pipePoint, uint32 data) override;

    virtual void CmdXdmaWaitFlipPending() override;

    virtual void SetGraphicsState(const GraphicsState& graphicsState) override;

    virtual void InheritStateFromCmdBuf(const GfxCmdBuffer* pCmdBuffer) override;

    template <bool indexed, bool indirect>
    void ValidateDraw(const ValidateDrawInfo& drawInfo);

    template <bool indexed, bool indirect, bool pm4OptImmediate>
    void ValidateDraw(const ValidateDrawInfo& drawInfo);

    template <bool indexed, bool indirect, bool pm4OptImmediate, bool pipelineDirty>
    uint32* ValidateDraw(
        const ValidateDrawInfo& drawInfo,
        uint32*                 pDeCmdSpace);

    template <bool indexed, bool indirect, bool pm4OptImmediate, bool pipelineDirty, bool stateDirty>
    uint32* ValidateDraw(
        const ValidateDrawInfo& drawInfo,
        uint32*                 pDeCmdSpace);

    template <bool indexed, bool indirect, bool pm4OptImmediate>
    uint32* ValidateDrawTimeHwState(
        regIA_MULTI_VGT_PARAM   iaMultiVgtParam,
        regVGT_LS_HS_CONFIG     vgtLsHsConfig,
        regPA_SC_MODE_CNTL_1    paScModeCntl1,
        const ValidateDrawInfo& drawInfo,
        uint32*                 pDeCmdSpace);

    // Gets vertex offset register address
    uint16 GetVertexOffsetRegAddr() const { return m_vertexOffsetReg; }

    // Gets instance offset register address. It always immediately follows the vertex offset register.
    uint16 GetInstanceOffsetRegAddr() const { return m_vertexOffsetReg + 1; }

    // Gets draw index register address.
    uint16 GetDrawIndexRegAddr()  const { return m_drawIndexReg; }

private:
    template <GfxIpLevel gfxLevel, bool issueSqttMarkerEvent, bool viewInstancingEnable, bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDraw(
        ICmdBuffer* pCmdBuffer,
        uint32      firstVertex,
        uint32      vertexCount,
        uint32      firstInstance,
        uint32      instanceCount,
        uint32      drawId);
    template <GfxIpLevel gfxLevel, bool issueSqttMarkerEvent, bool viewInstancingEnable, bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDrawOpaque(
        ICmdBuffer* pCmdBuffer,
        gpusize     streamOutFilledSizeVa,
        uint32      streamOutOffset,
        uint32      stride,
        uint32      firstInstance,
        uint32      instanceCount);
    template <GfxIpLevel gfxLevel, bool issueSqttMarkerEvent, bool viewInstancingEnable, bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDrawIndexed(
        ICmdBuffer* pCmdBuffer,
        uint32      firstIndex,
        uint32      indexCount,
        int32       vertexOffset,
        uint32      firstInstance,
        uint32      instanceCount,
        uint32      drawId);
    template <GfxIpLevel gfxLevel, bool issueSqttMarkerEvent, bool viewInstancingEnable, bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDrawIndirectMulti(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint32            stride,
        uint32            maximumCount,
        gpusize           countGpuAddr);
    template <GfxIpLevel gfxLevel, bool issueSqttMarkerEvent, bool viewInstancingEnable, bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDrawIndexedIndirectMulti(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint32            stride,
        uint32            maximumCount,
        gpusize           countGpuAddr);

    template <bool IssueSqttMarkerEvent, bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDispatch(
        ICmdBuffer* pCmdBuffer,
        uint32      x,
        uint32      y,
        uint32      z);
    template <bool IssueSqttMarkerEvent, bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDispatchIndirect(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset);
    template <bool IssueSqttMarkerEvent, bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDispatchOffset(
        ICmdBuffer* pCmdBuffer,
        uint32      xOffset,
        uint32      yOffset,
        uint32      zOffset,
        uint32      xDim,
        uint32      yDim,
        uint32      zDim);

    virtual void DeactivateQueryType(QueryPoolType queryPoolType) override;
    virtual void ActivateQueryType(QueryPoolType queryPoolType) override;

    template <bool pm4OptImmediate>
    uint32* UpdateDbCountControl(uint32 log2SampleRate, uint32* pDeCmdSpace);

    void UpdatePrimGroupOpt(uint32 vxtIdxCount);
    void DisablePrimGroupOpt();

    bool ForceWdSwitchOnEop(const GraphicsPipeline& pipeline, const ValidateDrawInfo& drawInfo) const;

    template <bool pm4OptImmediate>
    uint32* ValidateViewports(uint32* pDeCmdSpace);
    uint32* ValidateViewports(uint32* pDeCmdSpace);
    template <bool pm4OptImmediate>
    uint32* ValidateScissorRects(uint32* pDeCmdSpace);
    uint32* ValidateScissorRects(uint32* pDeCmdSpace);
    uint32* ValidateTriangleRasterState(uint32* pDeCmdSpace);

    uint32* WriteNullColorTargets(
        uint32* pCmdSpace,
        uint32  newColorTargetMask,
        uint32  oldColorTargetMask);
    uint32* WriteNullDepthTarget(uint32* pCmdSpace);

    uint32* FlushStreamOut(uint32* pDeCmdSpace);

    // Stream-out target GPU addresses must be DWORD-aligned, so we can use the LSB of the address to know if
    // a stream-out target has ever been set for this command buffer.
    bool HasStreamOutBeenSet() const { return ((m_streamOut.srd[0].word0.bits.BASE_ADDRESS & 1) == 0); }

    uint32* WaitOnCeCounter(uint32* pDeCmdSpace);
    uint32* IncrementDeCounter(uint32* pDeCmdSpace);

    PM4Predicate PacketPredicate() const { return static_cast<PM4Predicate>(m_gfxCmdBufState.flags.packetPredicate); }

    template <bool IssueSqttMarkerEventt, bool DescribeDrawDispatch>
    void SetDispatchFunctions();

    template <bool TessEnabled, bool GsEnabled>
    void SetUserDataValidationFunctions();
    void SetUserDataValidationFunctions(bool tessEnabled, bool gsEnabled);

    uint32* ValidateDispatch(
        gpusize indirectGpuVirtAddr,
        uint32  xDim,
        uint32  yDim,
        uint32  zDim,
        uint32* pDeCmdSpace);

    uint32* SwitchGraphicsPipeline(
        const GraphicsPipelineSignature* pPrevSignature,
        const GraphicsPipeline*          pCurrPipeline,
        uint32*                          pDeCmdSpace);

    template <bool HasPipelineChanged, bool TessEnabled, bool GsEnabled>
    uint32* ValidateGraphicsUserData(
        const GraphicsPipelineSignature* pPrevSignature,
        uint32*                          pDeCmdSpace);

    template <bool HasPipelineChanged>
    uint32* ValidateComputeUserData(
        const ComputePipelineSignature* pPrevSignature,
        uint32*                         pDeCmdSpace);

    template <bool TessEnabled, bool GsEnabled>
    uint32* WriteDirtyUserDataEntriesToSgprsGfx(
        const GraphicsPipelineSignature* pPrevSignature,
        uint8                            alreadyWrittenStageMask,
        uint32*                          pDeCmdSpace);

    template <bool TessEnabled, bool GsEnabled>
    uint8 FixupUserSgprsOnPipelineSwitch(
        const GraphicsPipelineSignature* pPrevSignature,
        uint32**                         ppDeCmdSpace);

    bool FixupUserSgprsOnPipelineSwitchCs(
        const ComputePipelineSignature* pPrevSignature,
        uint32**                        ppDeCmdSpace);

    void LeakNestedCmdBufferState(
        const UniversalCmdBuffer& cmdBuffer);

    uint8 CheckStreamOutBufferStridesOnPipelineSwitch();

    void DescribeDraw(Developer::DrawDispatchType cmdType);

    uint32* BuildWriteViewId(
        uint32  viewId,
        uint32* pCmdSpace);

    void SwitchDrawFunctions(
        bool viewInstancingEnable);

    const Device&   m_device;
    const CmdUtil&  m_cmdUtil;
    CmdStream       m_deCmdStream;
    CmdStream       m_ceCmdStream;

    // Tracks the user-data signature of the currently active compute & graphics pipelines.
    const ComputePipelineSignature*   m_pSignatureCs;
    const GraphicsPipelineSignature*  m_pSignatureGfx;

    uint32  m_pipelineCtxRegHash; // Hash of current pipeline's context registers.

    // Function pointers which validate all graphics user-data at Draw-time for the cases where the pipeline is
    // changing and cases where it is not.
    ValidateUserDataGfxFunc  m_pfnValidateUserDataGfx;
    ValidateUserDataGfxFunc  m_pfnValidateUserDataGfxPipelineSwitch;

    struct
    {
        // Per-pipeline watermark of the size of the vertex buffer table needed per draw (in DWORDs).
        uint32      watermark : 31;
        // Tracks whether or not the vertex buffer table was modified somewhere in the command buffer.
        uint32      modified  :  1;
        BufferSrd*  pSrds;  // Tracks the contents of the vertex buffer table.

        UserDataTableState  state;  // Tracks the state for the vertex buffer table.
    }  m_vbTable;

    struct
    {
        UserDataTableState  stateCs;  // Tracks the state of the compute spill table
        UserDataTableState  stateGfx; // Tracks the state of the graphics spill table
    }  m_spillTable;

    struct
    {
        UserDataTableState  state;  // Tracks the state of the stream-out SRD table

        BufferSrd  srd[MaxStreamOutTargets];    // Current stream-out target SRD's
    }  m_streamOut;

    UniversalCmdBufferState  m_state; // State tracking for internal cmd buffer operations

    regSX_PS_DOWNCONVERT__VI     m_sxPsDownconvert;
    regSX_BLEND_OPT_EPSILON__VI  m_sxBlendOptEpsilon;
    regSX_BLEND_OPT_CONTROL__VI  m_sxBlendOptControl;
    regVGT_DMA_INDEX_TYPE__VI    m_vgtDmaIndexType;     // Register setting for VGT_DMA_INDEX_TYPE
    regSPI_VS_OUT_CONFIG         m_spiVsOutConfig;      // Register setting for VS_OUT_CONFIG
    regSPI_PS_IN_CONTROL         m_spiPsInControl;      // Register setting for PS_IN_CONTROL
    uint16                       m_vertexOffsetReg;     // Register where the vertex start offset is written
    uint16                       m_drawIndexReg;        // Register where the draw index is written

    regDB_RENDER_OVERRIDE        m_dbRenderOverride;    // Last written value of the pipeline-owned part of
                                                        // DB_RENDER_OVERRIDE register.
    regPA_SU_LINE_STIPPLE_CNTL  m_paSuLineStippleCntl;  // Last written value of PA_SU_LINE_STIPPLE_CNTL
    regPA_SC_LINE_STIPPLE       m_paScLineStipple;      // Last written value of PA_SC_LINE_STIPPLE

    static constexpr uint32     InvalidPaSuScModeCntlVal = (7 << PA_SU_SC_MODE_CNTL__POLYMODE_BACK_PTYPE__SHIFT);
    regPA_SU_SC_MODE_CNTL       m_paSuScModeCntl;      // Current written value of PA_SU_SC_MODE_CNTL

    WorkaroundState  m_workaroundState;  // Manages several hardware workarounds whose states change between draws.
    DrawTimeHwState  m_drawTimeHwState;  // Tracks certain bits of HW-state that might need to be updated per draw.

    union
    {
        struct
        {
            uint32 tossPointMode              :  3; // The currently enabled "TossPointMode" global setting
            uint32 hiDepthDisabled            :  1; // True if Hi-Depth is disabled by settings
            uint32 hiStencilDisabled          :  1; // True if Hi-Stencil is disabled by settings
            uint32 ignoreCsBorderColorPalette :  1; // True if compute border-color palettes should be ignored
            uint32 blendOptimizationsEnable   :  1; // A copy of the blendOptimizationsEnable setting.
            uint32 outOfOrderPrimsEnable      :  2; // The out-of-order primitive rendering mode allowed by settings
            uint32 issueSqttMarkerEvent       :  1; // True if settings are such that we need to issue SQ thread trace
                                                    // marker events on draw.
            uint32 enablePm4Instrumentation   :  1; // True if settings are such that we should enable detailed PM4
                                                    // instrumentation.
            uint32 padParamCacheSpace         :  1; // True if this command buffer should pad used param-cache space to
                                                    // reduce context rolls.
            uint32 gfx7AvoidNullPrims         :  1; // True if settings indicate that we should avoid null primitive
                                                    // groups on GFX7.
            uint32 describeDrawDispatch       :  1; // True if draws/dispatch shader IDs should be specified within the
                                                    // command stream for parsing by PktTools
            uint32 rbPlusSupported            :  1; // True if RBPlus is supported by the device
            uint32 has32bPred                 :  1; // True if the CP supports 32-bit predicate flags
            uint32 reserved                   : 16;
        };
        uint32 u32All;
    } m_cachedSettings;

    // All state required by the dynamic primitive group size optimization. This optimization will track the number
    // of primitives per draw over a given window and issue a new IA_MULTI_VGT_PARAM with an optimal primgroup size
    // if those draws are small enough that they would benefit from a smaller primgroup size.
    struct
    {
        uint32 windowSize;  // Number of draws between updates to the dynamic primgroup size.  Will be set to zero if
                            // the optimization is disabled for this entire command buffer.
        uint32 step;        // Granularity of the dynamic primgroup sizes we'll choose
        uint32 minSize;     // Minimum primgroup size for the dynamic primgroup optimization
        uint32 maxSize;     // Maximum primgroup size for the dynamic primgroup optimization

        uint64 vtxIdxTotal; // Total number of vertices/indices drawn during the current window.
        uint32 drawCount;   // The number of draws processed during the current window.
        uint32 optimalSize; // If non-zero, this value was written to IA_MULTI_VGT_PARAM.PRIMGROUP_SIZE.
        bool   enabled;     // The optimization is disabled in certain conditions (e.g. tess, indirect draws).
    } m_primGroupOpt;

    // In order to prevent invalid query results if an app does Begin()/End(), Reset()/Begin()/End(), Resolve() on a
    // query slot in a command buffer (the first End() might overwrite values written by the Reset()), we have to
    // insert an idle before performing the Reset().  This has a high performance penalty.  This structure is used
    // to track memory ranges affected by outstanding End() calls in this command buffer so we can avoid the idle
    // during Reset() if the reset doesn't affect any pending queries.
    Util::IntervalTree<gpusize, bool, Platform>  m_activeOcclusionQueryWriteRanges;

    PAL_DISALLOW_DEFAULT_CTOR(UniversalCmdBuffer);
    PAL_DISALLOW_COPY_AND_ASSIGN(UniversalCmdBuffer);
};

// Helper function for managing the logic controlling when to do CE/DE synchronization and invalidating the Kcache.
extern bool HandleCeRinging(
    UniversalCmdBufferState* pState,
    uint32                   currRingPos,
    uint32                   ringInstances,
    uint32                   ringSize);

} // Gfx6
} // Pal
