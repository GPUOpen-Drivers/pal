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

#include "core/hw/gfxip/pm4UniversalCmdBuffer.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9ComputeCmdBuffer.h"
#include "core/hw/gfxip/gfx9/gfx9WorkaroundState.h"
#include "core/hw/gfxip/gfx9/gfx9ColorTargetView.h"
#include "core/hw/gfxip/gfx9/gfx9DepthStencilView.h"
#include "g_gfx9Settings.h"
#include "palAutoBuffer.h"
#include "palIntervalTree.h"
#include "palPipelineAbi.h"
#include "palVector.h"

#ifdef max
#undef max
#endif

#include <algorithm>
#include <type_traits>

namespace Pal
{
namespace Gfx9
{

class GraphicsPipeline;
class Gfx10DepthStencilView;
class UniversalCmdBuffer;
class IndirectCmdGenerator;

// Structure to track the state of internal command buffer operations.
struct UniversalCmdBufferState
{
    union
    {
        struct
        {
            // Tracks whether or not *ANY* piece of ring memory being dumped-to by the CE (by PAL or the client) has
            // wrapped back to the beginning within this command buffer. If no ring has wrapped yet, there is no need
            // to ever stall the CE from getting too far ahead or to ask the DE to invalidate the Kcache for us.
            uint32 ceHasAnyRingWrapped   :  1;
            // CE memory dumps go through the L2 cache, but not the L1 cache! In order for the shader cores to read
            // correct data out of piece of ring memory, we need to occasionally invalidate the Kcache when waiting
            // for the CE to finish dumping its memory. If set, the next INCREMENT_CE_COUNTER inserted into the DE
            // stream should also invalidate the Kcache.
            uint32 ceInvalidateKcache    :  1;
            uint32 ceWaitOnDeCounterDiff :  1;
            uint32 deCounterDirty        :  1;
            uint32 containsDrawIndirect  :  1;
            uint32 optimizeLinearGfxCpy  :  1;
            uint32 firstDrawExecuted     :  1;
            uint32 meshShaderEnabled     :  1;
            uint32 taskShaderEnabled     :  1;
            uint32 fastLaunchMode        :  2;
            uint32 cbTargetMaskChanged   :  1; // Flag setup at Pipeline bind-time informing the draw-time set
                                               // that the CB_TARGET_MASK has been changed.
            uint32 occlusionQueriesActive : 1; // Indicates if the current validated cmd buf state has occulsion
                                               // queries enabled.
            uint32 reserved0             :  3;
            uint32 cbColorInfoDirtyRtv   :  8; // Per-MRT dirty mask for CB_COLORx_INFO as a result of RTV
            uint32 reserved1             :  8;
        };
        uint32 u32All;
    } flags;
    // According to the UDX implementation, CP uCode and CE programming guide, the ideal DE counter diff amount we
    // should ask the CE to wait for is 1/4 the minimum size (in entries!) of all pieces of memory being ringed.
    // Thus we only need to track this minimum diff amount. If ceWaitOnDeCounterDiff flag is also set, the CE will
    // be asked to wait for a DE counter diff at the next Draw or Dispatch.
    uint32  minCounterDiff;

    // If non-null, points to the most recent DUMP_CONST_RAM or DUMP_CONST_RAM_OFFSET packet written into the CE cmd
    // stream.  If null, then no DUMP_CONST_RAM_* packets have been written since the previous Draw or Dispatch.
    uint32*               pLastDumpCeRam;
    // Stores the 2nd ordinal of the most-recent DUMP_CONST_RAM_* packet to avoid a read-modify-write when updating
    // that packet to set the increment_ce bit.
    DumpConstRamOrdinal2  lastDumpCeRamOrdinal2;

    // Copy of what will be written into CE RAM for NGG culling pipelines.
    Util::Abi::PrimShaderCullingCb primShaderCullingCb;

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
            uint32 instanceOffset         :  1; // Set when instanceOffset matches the HW value.
            uint32 vertexOffset           :  1; // Set when vertexOffset matches the HW value.
            uint32 drawIndex              :  1; // Set when drawIndex matches the HW value.
            uint32 numInstances           :  1; // Set when numInstances matches the HW value.
            uint32 paScModeCntl1          :  1; // Set when paScModeCntl1 matches the HW value.
            uint32 vgtMultiPrimIbResetEn  :  1; // Set when vgtMultiPrimIbResetEn matches the HW value.
            uint32 reserved               : 26; // Reserved bits
        };
        uint32     u32All;                // The flags as a single integer.
    } valid;                              // Draw state valid flags.

    union
    {
        struct
        {
            uint32 indexType        :  1; // Set when the index type is dirty
            uint32 indexBufferBase  :  1; // Set when the index buffer base address is dirty
            uint32 indexBufferSize  :  1; // Set when the index buffer size is dirty
            uint32 indexedIndexType :  1; // Set when the index type is dirty and needs to be rewritten for the next
                                          // indexed draw.
            uint32 reserved         : 28; // Reserved bits
        };
        uint32 u32All;                   // The flags as a single integer.
    } dirty;                             // Draw state dirty flags. If any of these are set, the next call to
                                         // ValidateDrawTimeHwState needs to write them.

    uint32                        instanceOffset;            // Current value of the instance offset user data.
    uint32                        vertexOffset;              // Current value of the vertex offset user data.
    uint32                        numInstances;              // Current value of the NUM_INSTANCES state.
    uint32                        drawIndex;                 // Current value of the draw index user data.
    regPA_SC_MODE_CNTL_1          paScModeCntl1;             // Current value of the PA_SC_MODE_CNTL1 register.
    regVGT_MULTI_PRIM_IB_RESET_EN vgtMultiPrimIbResetEn;     // Current value of the VGT_MULTI_PRIM_IB_RESET_EN
                                                             // register.
    gpusize                       nggIndexBufferPfStartAddr; // Start address of last IndexBuffer prefetch for NGG.
    gpusize                       nggIndexBufferPfEndAddr;   // End address of last IndexBuffer prefetch for NGG.
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

typedef regPA_SU_HARDWARE_SCREEN_OFFSET HwScreenOffsetPm4Img;

struct VportRegs
{
    VportScaleOffsetPm4Img scaleOffsetImgs[MaxViewports];
    VportZMinMaxPm4Img     zMinMaxImgs[MaxViewports];
    GuardbandPm4Img        guardbandImg;
    HwScreenOffsetPm4Img   hwScreenOffset;

    static constexpr uint32 NumScaleOffsetRegsPerVport = sizeof(VportScaleOffsetPm4Img) / sizeof(uint32_t);
    static constexpr uint32 NumZMinMaxRegsPerVport     = sizeof(VportZMinMaxPm4Img)     / sizeof(uint32_t);
    static constexpr uint32 NumGuardbandRegs           = sizeof(GuardbandPm4Img)        / sizeof(uint32_t);
    static constexpr uint32 NumHwScreenOffsetRegs      = sizeof(HwScreenOffsetPm4Img)   / sizeof(uint32_t);
};

// Register state for a single scissor rect.
struct ScissorRectPm4Img
{
    regPA_SC_VPORT_SCISSOR_0_TL tl;
    regPA_SC_VPORT_SCISSOR_0_BR br;
};

// Register state for PA SC Binner Cntl
struct PaScBinnerCntlRegs
{
    regPA_SC_BINNER_CNTL_0 paScBinnerCntl0;
    regPA_SC_BINNER_CNTL_1 paScBinnerCntl1;
};

// All NGG related state tracking.
struct NggState
{
    struct
    {
        uint8 hasPrimShaderWorkload : 1;
        uint8 dirty                 : 1;
        uint8 reserved              : 6;
    } flags;

    uint32 numSamples;  // Number of active MSAA samples.
};

// Cached settings used to speed up access to settings/info referenced at draw-time. Shared with Workaround class.
union CachedSettings
{
    struct
    {
        uint64 tossPointMode              :  3; // The currently enabled "TossPointMode" global setting
        uint64 hiDepthDisabled            :  1; // True if Hi-Depth is disabled by settings
        uint64 hiStencilDisabled          :  1; // True if Hi-Stencil is disabled by settings
        uint64 ignoreCsBorderColorPalette :  1; // True if compute border-color palettes should be ignored
        uint64 blendOptimizationsEnable   :  1; // A copy of the blendOptimizationsEnable setting.
        uint64 outOfOrderPrimsEnable      :  2; // The out-of-order primitive rendering mode allowed by settings
        uint64 scissorChangeWa            :  1; // True if the scissor register workaround is enabled
        uint64 issueSqttMarkerEvent       :  1; // True if settings are such that we need to issue SQ thread trace
                                                // marker events on draw.
        uint64 enablePm4Instrumentation   :  1; // True if settings are such that we should enable detailed PM4
                                                // instrumentation.
        uint64 batchBreakOnNewPs          :  1; // True if a BREAK_BATCH should be inserted when switching pixel
                                                // shaders.
        uint64 padParamCacheSpace         :  1; // True if this command buffer should pad used param-cache space to
                                                // reduce context rolls.
        uint64 describeDrawDispatch       :  1; // True if draws/dispatch shader IDs should be specified within the
                                                // command stream for parsing by PktTools
        uint64 rbPlusSupported            :  1; // True if RBPlus is supported by the device
        uint64 disableVertGrouping        :  1; // Disable VertexGrouping.
        uint64 prefetchIndexBufferForNgg  :  1; // Prefetch index buffers to workaround misses in UTCL2 with NGG
        uint64 waCeDisableIb2             :  1; // Disable IB2's on the constant engine to workaround HW bug
        uint64 supportsMall               :  1; // True if this device supports the MALL
        uint64 waDisableInstancePacking   :  1;
        uint64 reserved3                  :  1;
        uint64 pbbMoreThanOneCtxState     :  1;
        uint64 waUtcL0InconsistentBigPage :  1;
        uint64 waClampGeCntlVertGrpSize   :  1;
        uint64 reserved4                  :  1;
        uint64 reserved11                 :  2;
        uint64 ignoreDepthForBinSize      :  1; // Ignore depth when calculating Bin Size (unless no color bound)
        uint64 pbbDisableBinMode          :  2; // BINNING_MODE value to use when PBB is disabled

        uint64 waLogicOpDisablesOverwriteCombiner        :  1;
        uint64 waMiscPopsMissedOverlap                   :  1;
        uint64 waColorCacheControllerInvalidEviction     :  1;
        uint64 waRotatedSwizzleDisablesOverwriteCombiner :  1;
        uint64 waTessIncorrectRelativeIndex              :  1;
        uint64 waVgtFlushNggToLegacy                     :  1;
        uint64 waVgtFlushNggToLegacyGs                   :  1;
        uint64 waIndexBufferZeroSize                     :  1;
        uint64 waLegacyGsCutModeFlush                    :  1;
        uint64 waClampQuadDistributionFactor             :  1;

        uint64 supportsVrs                               :  1;
        uint64 vrsForceRateFine                          :  1;
#if (PAL_BUILD_GFX11)
        uint64 supportsSwStrmout                         :  1;
#else
        uint64 reserved7                                 :  1;
#endif
        uint64 supportAceOffload                         :  1;
        uint64 useExecuteIndirectPacket                  :  2;
        uint64 disablePreamblePipelineStats              :  1;
#if PAL_BUILD_GFX11
        uint64 primGrpSize                 :  9; // For programming GE_CNTL::PRIM_GRP_SIZE
        uint64 geCntlGcrMode               :  2; // For programming GE_CNTL::GCR_DISABLE
        uint64 useLegacyDbZInfo            :  1;
        uint64 waLineStippleReset          :  1;
        uint64 disableRbPlusWithBlending   :  1;
        uint64 waEnableIntrinsicRateEnable :  1;
        uint64 supportsShPairsPacket       :  1;
        uint64 supportsShPairsPacketCs     :  1;
#else
        uint64 reserved8                   : 17;
#endif
#if PAL_BUILD_NAVI3X
        uint64 waAddPostambleEvent        :  1;
#else
        uint64 reserved10                 :  1;
#endif

        uint64 optimizeDepthOnlyFmt       :  1;
        uint64 has32bPred                 :  1;
        uint64 optimizeNullSourceImage    :  1;
        uint64 waitAfterCbFlush           :  1;
        uint64 waitAfterDbFlush           :  1;
        uint64 rbHarvesting               :  1;
        uint64 reserved                   : 57;
    };
    uint64 u64All[3];
};

// Tracks a prior VRS rate image to HTile copy so that we can skip redundant rate image copies.
struct VrsCopyMapping
{
    const Pal::Image* pRateImage;  // The source VRS rate image.
    const Pal::Image* pDepthImage; // Contains the destination HTile.

    // The original destination is always a depth stencil view but we cannot keep a pointer to it because it's legal
    // to create it on the stack and destroy it after the view is unbound. Instead we must copy the view's mip level
    // and slice range.
    uint32            mipLevel;
    uint32            baseSlice;
    uint32            endSlice;
};

// =====================================================================================================================
// GFX9 universal command buffer class: implements GFX9 specific functionality for the UniversalCmdBuffer class.
class UniversalCmdBuffer final : public Pal::Pm4::UniversalCmdBuffer
{
    // Shorthand for function pointers which validate graphics user-data at Draw-time.
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
    virtual void CmdPrimeGpuCaches(uint32                    rangeCount,
                                   const PrimeGpuCacheRange* pRanges) override;
    virtual void CmdSetClipRects(uint16      clipRule,
                                 uint32      rectCount,
                                 const Rect* pRectList) override;
    void CmdAceWaitDe();
    void CmdDeWaitAce();

    virtual void CmdBarrier(const BarrierInfo& barrierInfo) override;

    virtual uint32 CmdRelease(
        const AcquireReleaseInfo& releaseInfo) override;
    virtual void CmdAcquire(
        const AcquireReleaseInfo& acquireInfo,
        uint32                    syncTokenCount,
        const uint32*             pSyncTokens) override;

    virtual void CmdReleaseEvent(
        const AcquireReleaseInfo& releaseInfo,
        const IGpuEvent*          pGpuEvent) override;
    virtual void CmdAcquireEvent(
        const AcquireReleaseInfo& acquireInfo,
        uint32                    gpuEventCount,
        const IGpuEvent* const*   ppGpuEvents) override;

    virtual void CmdReleaseThenAcquire(const AcquireReleaseInfo& barrierInfo) override;

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

    virtual CmdStream* GetCmdStreamByEngine(CmdBufferEngineSupport engineType) override;

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

    virtual void CmdUpdateHiSPretests(
        const IImage*      pImage,
        const HiSPretests& pretests,
        uint32             firstMip,
        uint32             numMips) override;

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

    virtual void CmdCommentString(const char* pComment) override;

    virtual void CmdNop(
        const void* pPayload,
        uint32      payloadSize) override;

    uint32 ComputeSpillTableInstanceCnt(
        uint32 spillTableDwords,
        uint32 vertexBufTableDwords,
        uint32 maxCmdCnt
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 803
        , bool* pUseLargeEmbeddedData
#endif
        ) const;

    uint32 BuildExecuteIndirectIb2Packets(
        const IndirectCmdGenerator& gfx9Generator,
        const GraphicsPipeline&     gfxPipeline,
        const ComputePipeline&      csPipeline,
        const bool                  isGfx,
        uint32*                     pDeCmdIb2Space);

    gpusize ConstructExecuteIndirectIb2(
        const IndirectCmdGenerator& gfx9Generator,
        PipelineBindPoint           bindPoint,
        const uint32                maximumCount,
        const GraphicsPipeline*     pGfxPipeline,
        const ComputePipeline*      pCsPipeline,
        gpusize*                    pIb2GpuSize,
        gpusize*                    pSpillTableAddress,
        uint32*                     pSpillTableInstCnt,
        uint32*                     pSpillTableStride,
        uint32*                     pVbTableRegOffset,
        uint32*                     pVbTableSize);

    void ExecuteIndirectPacket(
        const IIndirectCmdGenerator& generator,
        const IGpuMemory&            gpuMemory,
        gpusize                      offset,
        uint32                       maximumCount,
        gpusize                      countGpuAddr);

    void ExecuteIndirectShader(
        const IIndirectCmdGenerator& generator,
        const IGpuMemory&            gpuMemory,
        gpusize                      offset,
        uint32                       maximumCount,
        gpusize                      countGpuAddr);

    virtual void CmdExecuteIndirectCmds(
        const IIndirectCmdGenerator& generator,
        const IGpuMemory&            gpuMemory,
        gpusize                      offset,
        uint32                       maximumCount,
        gpusize                      countGpuAddr) override;

    virtual void CmdDispatchAce(DispatchDims size) override;

    virtual void GetChunkForCmdGeneration(
        const Pm4::IndirectCmdGenerator& generator,
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

    void SetPrimShaderWorkload() { m_nggState.flags.hasPrimShaderWorkload = 1; }
    bool HasPrimShaderWorkload() const { return m_nggState.flags.hasPrimShaderWorkload; }

    uint32 BuildScissorRectImage(
        bool               multipleViewports,
        ScissorRectPm4Img* pScissorRectImg) const;

    template <bool pm4OptImmediate>
    uint32* ValidateScissorRects(uint32* pDeCmdSpace);
    uint32* ValidateScissorRects(uint32* pDeCmdSpace);

    bool NeedsToValidateScissorRectsWa(bool pm4OptImmediate) const;

    uint32* ValidatePaScAaConfig(uint32* pDeCmdSpace);

    virtual void CpCopyMemory(gpusize dstAddr, gpusize srcAddr, gpusize numBytes) override;

    virtual void CmdSetPerDrawVrsRate(const VrsRateParams&  rateParams) override;
    virtual void CmdSetVrsCenterState(const VrsCenterState&  centerState) override;
    virtual void CmdBindSampleRateImage(const IImage*  pImage) override;

    // See gfxCmdBuffer.h for a full description of this function.
    virtual void DirtyVrsDepthImage(const IImage* pDepthImage) override;

    void CallNestedCmdBuffer(UniversalCmdBuffer* pCmdBuf);

    regDB_DFSM_CONTROL* GetDbDfsmControl() { return &m_dbDfsmControl; }
    bool HasWaMiscPopsMissedOverlapBeenApplied() const { return m_hasWaMiscPopsMissedOverlapBeenApplied; }
    void SetWaMiscPopsMissedOverlapHasBeenApplied() { m_hasWaMiscPopsMissedOverlapBeenApplied = true; }

    virtual gpusize GetMeshPipeStatsGpuAddr() const override { return m_meshPipeStatsGpuAddr; }

    // checks if the entire command buffer can be preempted or not
    virtual bool IsPreemptable() const override;

    virtual uint32* WriteWaitEop(HwPipePoint waitPoint, uint32 hwGlxSync, uint32 hwRbSync, uint32* pCmdSpace) override;
    virtual uint32* WriteWaitCsIdle(uint32* pCmdSpace) override;

protected:
    virtual ~UniversalCmdBuffer();

    virtual Result AddPreamble() override;
    virtual Result AddPostamble() override;

    virtual void ResetState() override;

    virtual void WriteEventCmd(const BoundGpuMemory& boundMemObj, HwPipePoint pipePoint, uint32 data) override;

    virtual void CmdXdmaWaitFlipPending() override;

    virtual void InheritStateFromCmdBuf(const Pm4CmdBuffer* pCmdBuffer) override;

    template <bool Pm4OptImmediate, bool IsNgg, bool Indirect>
    uint32* ValidateBinSizes(uint32* pDeCmdSpace);

    template <bool Indexed, bool Indirect>
    void ValidateDraw(const Pm4::ValidateDrawInfo& drawInfo);

    template <bool Indexed, bool Indirect, bool Pm4OptImmediate>
    void ValidateDraw(const Pm4::ValidateDrawInfo& drawInfopDeCmdSpace);

    template <bool Indexed, bool Indirect, bool Pm4OptImmediate, bool PipelineDirty>
    uint32* ValidateDraw(
        const Pm4::ValidateDrawInfo& drawInfo,
        uint32*                      pDeCmdSpace);

    template <bool Indexed, bool Indirect, bool Pm4OptImmediate, bool PipelineDirty, bool StateDirty>
    uint32* ValidateDraw(
        const Pm4::ValidateDrawInfo& drawInfo,
        uint32*                      pDeCmdSpace);

    template <bool Indexed, bool Indirect, bool Pm4OptImmediate, bool PipelineDirty, bool StateDirty, bool IsNgg>
    uint32* ValidateDraw(
        const Pm4::ValidateDrawInfo& drawInfo,
        uint32*                      pDeCmdSpace);

    template <bool Indexed, bool Indirect, bool Pm4OptImmediate>
    uint32* ValidateDrawTimeHwState(
        regPA_SC_MODE_CNTL_1         paScModeCntl1,
        const Pm4::ValidateDrawInfo& drawInfo,
        uint32*                      pDeCmdSpace);

    // Gets vertex offset register address
    uint16 GetVertexOffsetRegAddr() const { return m_vertexOffsetReg; }

    // Gets instance offset register address. It always immediately follows the vertex offset register.
    uint16 GetInstanceOffsetRegAddr() const { return m_vertexOffsetReg + 1; }

    // Gets draw index register address.
    uint16 GetDrawIndexRegAddr()  const { return m_drawIndexReg; }

    virtual void P2pBltWaCopyBegin(
        const GpuMemory* pDstMemory,
        uint32           regionCount,
        const gpusize*   pChunkAddrs) override;
    virtual void P2pBltWaCopyNextRegion(gpusize chunkAddr) override;
    virtual void P2pBltWaCopyEnd() override;

private:
    template <bool IssueSqttMarkerEvent,
              bool HasUavExport,
              bool ViewInstancingEnable,
              bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDraw(
        ICmdBuffer* pCmdBuffer,
        uint32      firstVertex,
        uint32      vertexCount,
        uint32      firstInstance,
        uint32      instanceCount,
        uint32      drawId);

    template <bool IssueSqttMarkerEvent,
              bool HasUavExport,
              bool ViewInstancingEnable,
              bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDrawOpaque(
        ICmdBuffer* pCmdBuffer,
        gpusize streamOutFilledSizeVa,
        uint32  streamOutOffset,
        uint32  stride,
        uint32  firstInstance,
        uint32  instanceCount);

    template <bool IssueSqttMarkerEvent,
              bool HasUavExport,
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

    template <bool IssueSqttMarkerEvent, bool ViewInstancingEnable, bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDrawIndirectMulti(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint32            stride,
        uint32            maximumCount,
        gpusize           countGpuAddr);

    template <bool IssueSqttMarkerEvent, bool ViewInstancingEnable, bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDrawIndexedIndirectMulti(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint32            stride,
        uint32            maximumCount,
        gpusize           countGpuAddr);

    template <bool HsaAbi, bool IssueSqttMarkerEvent, bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDispatch(
        ICmdBuffer*  pCmdBuffer,
        DispatchDims size);
    template <bool IssueSqttMarkerEvent, bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDispatchIndirect(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset);
    template <bool HsaAbi, bool IssueSqttMarkerEvent, bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDispatchOffset(
        ICmdBuffer*  pCmdBuffer,
        DispatchDims offset,
        DispatchDims launchSize,
        DispatchDims logicalSize);
    template <bool IssueSqttMarkerEvent, bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDispatchDynamic(
        ICmdBuffer*  pCmdBuffer,
        gpusize      gpuVa,
        DispatchDims size);
#if PAL_BUILD_GFX11
    template <bool IssueSqttMarkerEvent,
              bool HasUavExport,
              bool ViewInstancingEnable,
              bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDispatchMeshNative(
        ICmdBuffer*  pCmdBuffer,
        DispatchDims size);
#endif
    template <bool IssueSqttMarkerEvent,
              bool HasUavExport,
              bool ViewInstancingEnable,
              bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDispatchMeshAmpFastLaunch(
        ICmdBuffer*  pCmdBuffer,
        DispatchDims size);
    template <bool IssueSqttMarkerEvent,
              bool ViewInstancingEnable,
              bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDispatchMeshIndirectMulti(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint32            stride,
        uint32            maximumCount,
        gpusize           countGpuAddr);
    template <bool IssueSqttMarkerEvent,
              bool HasUavExport,
              bool ViewInstancingEnable,
              bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDispatchMeshTask(
        ICmdBuffer*  pCmdBuffer,
        DispatchDims size);
    template <bool IssueSqttMarkerEvent,
              bool ViewInstancingEnable,
              bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDispatchMeshIndirectMultiTask(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint32            stride,
        uint32            maximumCount,
        gpusize           countGpuAddr);

    template <bool IsNgg>
    uint32 CalcGeCntl(
        bool                  usesLineStipple,
        regIA_MULTI_VGT_PARAM iaMultiVgtParam) const;

    template <bool PipelineDirty, bool StateDirty>
    uint32* ValidateTriangleRasterState(
        const GraphicsPipeline*  pPipeline,
        uint32*                  pDeCmdSpace);

    template <bool Pm4OptImmediate, bool PipelineDirty, bool StateDirty>
    uint32* ValidateCbColorInfo(
        uint32* pDeCmdSpace);
    uint32* ValidateDbRenderOverride(
        uint32* pDeCmdSpace);

    uint32* WriteTessDistributionFactors(
        uint32* pDeCmdSpace);

    static Offset2d GetHwShadingRate(VrsShadingRate  shadingRate);

    static uint32 GetHwVrsCombinerState(VrsCombiner  combinerMode);

    static uint32 GetHwVrsCombinerState(
        const VrsRateParams&  rateParams,
        VrsCombinerStage      combinerStage);

    void ValidateVrsState();

    void BarrierMightDirtyVrsRateImage(const IImage* pRateImage);

    // See m_validVrsCopies for more information on what these do.
    bool IsVrsCopyRedundant(const Gfx10DepthStencilView* pDsView, const Pal::Image* pRateImage);
    void AddVrsCopyMapping(const Gfx10DepthStencilView* pDsView, const Pal::Image* pRateImage);
    void EraseVrsCopiesFromRateImage(const Pal::Image* pRateImage);
    void EraseVrsCopiesToDepthImage(const Pal::Image* pDepthImage);

    virtual void DeactivateQueryType(QueryPoolType queryPoolType) override;
    virtual void ActivateQueryType(QueryPoolType queryPoolType) override;

    template <bool pm4OptImmediate>
    uint32* UpdateDbCountControl(uint32 log2SampleRate, uint32* pDeCmdSpace);

    bool ForceWdSwitchOnEop(const GraphicsPipeline& pipeline, const Pm4::ValidateDrawInfo& drawInfo) const;

    VportCenterRect GetViewportsCenterAndScale() const;

    template <bool pm4OptImmediate>
    uint32* ValidateViewports(uint32* pDeCmdSpace);
    uint32* ValidateViewports(uint32* pDeCmdSpace);

    void WriteNullColorTargets(
        uint32  newColorTargetMask,
        uint32  oldColorTargetMask);
    uint32* WriteNullDepthTarget(uint32* pCmdSpace);

    uint32* FlushStreamOut(uint32* pDeCmdSpace);

    bool HasStreamOutBeenSet() const;

    uint32* WaitOnCeCounter(uint32* pDeCmdSpace);
    uint32* IncrementDeCounter(uint32* pDeCmdSpace);

    Pm4Predicate PacketPredicate() const { return static_cast<Pm4Predicate>(m_pm4CmdBufState.flags.packetPredicate); }

    template <bool HsaAbi, bool IssueSqttMarkerEvent, bool DescribeDrawDispatch>
    void SetDispatchFunctions();
    void SetDispatchFunctions(bool hsaAbi);

    template <bool TessEnabled, bool GsEnabled, bool VsEnabled>
    void SetUserDataValidationFunctions();
    void SetUserDataValidationFunctions(bool tessEnabled, bool gsEnabled, bool isNgg);

    void ValidateDispatchPalAbi(
        ComputeState* pComputeState,
        CmdStream*    pCmdStream,
        gpusize       indirectGpuVirtAddr,
        gpusize       launchDescGpuVirtAddr,
        DispatchDims  logicalSize);

    void ValidateDispatchHsaAbi(
        ComputeState* pComputeState,
        CmdStream*    pCmdStream,
        DispatchDims  offset,
        DispatchDims  logicalSize);

    uint32* SwitchGraphicsPipeline(
        const GraphicsPipelineSignature* pPrevSignature,
        const GraphicsPipeline*          pCurrPipeline,
        uint32*                          pDeCmdSpace);

    template <bool HasPipelineChanged, bool TessEnabled, bool GsEnabled, bool VsEnabled>
    uint32* ValidateGraphicsUserData(
        const GraphicsPipelineSignature* pPrevSignature,
        uint32*                          pDeCmdSpace);

    template <bool HasPipelineChanged>
    uint32* ValidateComputeUserData(
        ICmdBuffer*                     pCmdBuffer,
        UserDataTableState*             pSpillTable,
        UserDataEntries*                pUserData,
        CmdStream*                      pCmdStream,
        const ComputePipelineSignature* pPrevSignature,
        const ComputePipelineSignature* pCurrSignature,
        uint32*                         pCmdSpace);

    template <bool TessEnabled, bool GsEnabled, bool VsEnabled>
    uint32* WriteDirtyUserDataEntriesToSgprsGfx(
        const GraphicsPipelineSignature* pPrevSignature,
        uint8                            alreadyWrittenStageMask,
        uint32*                          pDeCmdSpace);

    template <bool TessEnabled, bool GsEnabled, bool VsEnabled>
    uint8 FixupUserSgprsOnPipelineSwitch(
        const GraphicsPipelineSignature* pPrevSignature,
        uint32**                         ppDeCmdSpace);

    bool FixupUserSgprsOnPipelineSwitchCs(
        const UserDataEntries&          userData,
        const ComputePipelineSignature* pCurrSignature,
        const ComputePipelineSignature* pPrevSignature,
#if PAL_BUILD_GFX11
        const bool                      onAce,
#endif
        uint32**                        ppDeCmdSpace);

    void LeakNestedCmdBufferState(
        const UniversalCmdBuffer& cmdBuffer);

    uint8 CheckStreamOutBufferStridesOnPipelineSwitch();

    void Gfx9GetColorBinSize(Extent2d* pBinSize) const;
    void Gfx9GetDepthBinSize(Extent2d* pBinSize) const;
    void Gfx10GetColorBinSize(Extent2d* pBinSize) const;
    void Gfx10GetDepthBinSize(Extent2d* pBinSize) const;
    template <bool IsNgg>
    bool SetPaScBinnerCntl01(
                             const Extent2d* pBinSize);

    void DescribeDraw(Developer::DrawDispatchType cmdType, bool includedGangedAce = false);

    void P2pBltWaSync();

    uint32* UpdateNggCullingDataBufferWithCpu(
        uint32* pDeCmdSpace);

    uint32* BuildWriteViewId(
        uint32  viewId,
        uint32* pCmdSpace);

    void UpdateUavExportTable();

    void SwitchDrawFunctions(
        bool hasUavExport,
        bool viewInstancingEnable,
#if PAL_BUILD_GFX11
        bool nativeMsEnable,
#endif
        bool hasTaskShader);

    template <bool IssueSqtt,
              bool DescribeDrawDispatch>
    void SwitchDrawFunctionsInternal(
        bool hasUavExport,
        bool viewInstancingEnable,
#if PAL_BUILD_GFX11
        bool nativeMsEnable,
#endif
        bool hasTaskShader);

    template <bool ViewInstancing,
              bool IssueSqtt,
              bool DescribeDrawDispatch>
    void SwitchDrawFunctionsInternal(
        bool hasUavExport,
#if PAL_BUILD_GFX11
        bool nativeMsEnable,
#endif
        bool hasTaskShader);

    template <bool ViewInstancing,
              bool HasUavExport,
              bool IssueSqtt,
              bool DescribeDrawDispatch>
    void SwitchDrawFunctionsInternal(
#if PAL_BUILD_GFX11
        bool nativeMsEnable,
#endif
        bool hasTaskShader);

    CmdStream* GetAceCmdStream();
    gpusize    GangedCmdStreamSemAddr();
    void       IssueGangedBarrierAceWaitDeIncr();
    void       IssueGangedBarrierDeWaitAceIncr();
    void       UpdateTaskMeshRingSize();
    void       ValidateTaskMeshDispatch(gpusize indirectGpuVirtAddr, DispatchDims size);

    void        ValidateExecuteNestedCmdBuffer();

#if PAL_BUILD_GFX11
    gpusize    SwStreamoutDataAddr();
#endif

    bool IsTessEnabled() const
    {
#if PAL_ENABLE_PRINTS_ASSERTS
        PAL_ASSERT(m_pipelineStateValid == true);
#endif
        return (m_pipelineState.flags.usesTess != 0);
    }

    bool IsGsEnabled() const
    {
#if PAL_ENABLE_PRINTS_ASSERTS
        PAL_ASSERT(m_pipelineStateValid == true);
#endif
        return (m_pipelineState.flags.usesGs != 0);
    }

    bool IsNggEnabled() const
    {
#if PAL_ENABLE_PRINTS_ASSERTS
        PAL_ASSERT(m_pipelineStateValid == true);
#endif
        return (m_pipelineState.flags.isNgg != 0);
    }

#if (PAL_BUILD_GFX11)
    bool SupportsSwStrmout() const { return m_cachedSettings.supportsSwStrmout; }
#endif

    void WritePerDrawVrsRate(const VrsRateParams&  rateParams);

#if PAL_BUILD_GFX11
    template <Pm4ShaderType ShaderType, bool Pm4OptImmediate>
    uint32* WritePackedUserDataEntriesToSgprs(uint32* pDeCmdSpace);
    template <Pm4ShaderType ShaderType>
    uint32* WritePackedUserDataEntriesToSgprs(uint32* pDeCmdSpace);
#endif
    template <Pm4ShaderType ShaderType>
    uint32* SetUserSgprReg(
        uint16  regAddr,
        uint32  regValue,
#if PAL_BUILD_GFX11
        bool    onAce,
#endif
        uint32* pDeCmdSpace);

    template <Pm4ShaderType ShaderType>
    uint32* SetSeqUserSgprRegs(
        uint16      startAddr,
        uint16      endAddr,
        const void* pValues,
    #if PAL_BUILD_GFX11
        bool        onAce,
    #endif
        uint32*     pDeCmdSpace);

    bool UpdateNggPrimCb(
        const GraphicsPipeline*         pCurrentPipeline,
        Util::Abi::PrimShaderCullingCb* pPrimShaderCb) const;

    template <typename... Tn>
    using ViewStorage = typename std::aligned_storage<
        std::max({ sizeof(Tn)... }),
        std::max({ alignof(Tn)... })>::type;

#if PAL_BUILD_GFX11
    using ColorTargetViewStorage  = ViewStorage<Gfx9ColorTargetView, Gfx10ColorTargetView, Gfx11ColorTargetView>;
#else
    using ColorTargetViewStorage  = ViewStorage<Gfx9ColorTargetView, Gfx10ColorTargetView>;
#endif
    using DepthStencilViewStorage = ViewStorage<Gfx9DepthStencilView, Gfx10DepthStencilView>;

    IColorTargetView* StoreColorTargetView(uint32 slot, const BindTargetParams& params);

    void CopyColorTargetViewStorage(
        ColorTargetViewStorage*       pColorTargetViewStorageDst,
        const ColorTargetViewStorage* pColorTargetViewStorageSrc,
        Pm4::GraphicsState*           pGraphicsStateDst);

    IDepthStencilView* StoreDepthStencilView(const BindTargetParams& params);

    void CopyDepthStencilViewStorage(
        DepthStencilViewStorage*       pDepthStencilViewStorageDst,
        const DepthStencilViewStorage* pDepthStencilViewStorageSrc,
        Pm4::GraphicsState*            pGraphicsStateDst);

    const Device&   m_device;
    const CmdUtil&  m_cmdUtil;
    CmdStream       m_deCmdStream;
    CmdStream       m_ceCmdStream;

    // Tracks the user-data signature of the currently active compute & graphics pipelines.
    const ComputePipelineSignature*   m_pSignatureCs;
    const GraphicsPipelineSignature*  m_pSignatureGfx;

    // Hash of current rb+ related registers(m_sxPsDownconvert, m_sxBlendOptEpsilon, m_sxBlendOptControl).
    uint32      m_rbplusRegHash;        // Hash of current pipeline's rb+ registers.
    uint32      m_pipelineCtxRegHash;   // Hash of current pipeline's context registers.
    uint32      m_pipelineCfgRegHash;   // Hash of current pipeline's config registers.
    ShaderHash  m_pipelinePsHash;       // Hash of current pipeline's pixel shader program.

    struct
    {
        union
        {
            struct
            {
                uint32  usesTess  :  1;
                uint32  usesGs    :  1;
                uint32  isNgg     :  1;
                uint32  gsCutMode :  2;
                uint32  reserved  : 27;
            };
            uint32 u32All;
        } flags; // Flags describing the currently active pipeline stages.
    }  m_pipelineState;

#if PAL_ENABLE_PRINTS_ASSERTS
    bool m_pipelineStateValid; /// Debug flag for knowing when m_pipelineState is valid (most of draw-time).
#endif

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

        UserDataTableState  state;  // Tracks the state for the indirect user-data table

    }  m_vbTable;

    struct
    {
        UserDataTableState  state; // Tracks the state of the NGG state table
    }  m_nggTable;

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

    struct
    {
        UserDataTableState  state;         // Tracks the state of the SRD table
        ImageSrd            srd[MaxColorTargets];
        uint32              tableSizeDwords; // Size of the srd table in dwords, omitting unbound targets at the end
        uint32              maxColorTargets; // Maximum color targets bound by the shader
    }  m_uavExportTable;

    // DX12 requires that the command-stream chunks generated by indirect command generators honor the command
    // buffer's predication state. Since we cannot predicate the chain packet used to launch the indirect command
    // chunks, we need to save the predicate values to a location in embedded data to check when executing a call
    // to CmdExecuteIndirectCommands().
    gpusize  m_predGpuAddr;

    WorkaroundState          m_workaroundState;
    UniversalCmdBufferState  m_state; // State tracking for internal cmd buffer operations

    regSX_PS_DOWNCONVERT                     m_sxPsDownconvert;
    regSX_BLEND_OPT_EPSILON                  m_sxBlendOptEpsilon;
    regSX_BLEND_OPT_CONTROL                  m_sxBlendOptControl;
    regVGT_DMA_INDEX_TYPE                    m_vgtDmaIndexType;   // Register setting for VGT_DMA_INDEX_TYPE
    regSPI_VS_OUT_CONFIG                     m_spiVsOutConfig;    // Register setting for VS_OUT_CONFIG
    regSPI_PS_IN_CONTROL                     m_spiPsInControl;    // Register setting for PS_IN_CONTROL
    regPA_SC_CONSERVATIVE_RASTERIZATION_CNTL m_paScConsRastCntl;  // Register setting for PA_SC_CONSERV_RAST_CNTL
    regVGT_LS_HS_CONFIG                      m_vgtLsHsConfig;     // Register setting for VGT_LS_HS_CONFIG
    regGE_CNTL                               m_geCntl;            // Register setting for GE_CNTL
    regDB_SHADER_CONTROL                     m_dbShaderControl;   // Register setting for DB_SHADER_CONTROL
    regCB_COLOR_CONTROL                      m_cbColorControl;    // Register setting for CB_COLOR_CONTROL
    regPA_CL_CLIP_CNTL                       m_paClClipCntl;      // Register setting for PA_CL_CLIP_CNTL
    regCB_TARGET_MASK                        m_cbTargetMask;      // Register setting for CB_TARGET_MASK
    regVGT_TF_PARAM                          m_vgtTfParam;        // Register setting for VGT_TF_PARAM
    regPA_SC_LINE_CNTL                       m_paScLineCntl;      // Register setting for PA_SC_LINE_CNTL
    uint16                                   m_vertexOffsetReg;   // Register where the vertex start offset is written
    uint16                                   m_drawIndexReg;      // Register where the draw index is written
    DepthClampMode                           m_depthClampMode;    // Depth clamping behavior
    regCB_COLOR0_INFO m_cbColorInfo[MaxColorTargets]; // Final CB_COLOR_INFO register values. Impacted by RTV and
                                                      // (Pipeline || Blend) state.

    const uint32  m_log2NumSes;
    const uint32  m_log2NumRbPerSe;

    uint32  m_depthBinSizeTagPart;    // Constant used in Depth PBB bin size formulas
    uint32  m_colorBinSizeTagPart;    // Constant used in Color PBB bin size formulas
    uint32  m_fmaskBinSizeTagPart;    // Constant used in Fmask PBB bin size formulas
    uint16  m_minBinSizeX;            // Minimum bin size(width) for PBB.
    uint16  m_minBinSizeY;            // Minimum bin size(height) for PBB.
    uint32  m_totalNumRbs;            // RB number

    regCB_RMI_GL2_CACHE_CONTROL m_cbRmiGl2CacheControl; // Control CB cache policy and big page

    union
    {
        struct
        {
            uint64 maxAllocCountNgg       : 16;
            uint64 maxAllocCountLegacy    : 16;
            uint64 persistentStatesPerBin : 16;
            uint64 maxPrimsPerBatch       : 16;
        };
        uint64 u64All;
    } m_cachedPbbSettings;

    PaScBinnerCntlRegs      m_pbbCntlRegs;

    regDB_DFSM_CONTROL      m_dbDfsmControl;
    regDB_RENDER_OVERRIDE   m_dbRenderOverride;     // Current value of DB_RENDER_OVERRIDE.
    regDB_RENDER_OVERRIDE   m_prevDbRenderOverride; // Prev value of DB_RENDER_OVERRIDE - only used on primary CmdBuf.
    regVGT_MULTI_PRIM_IB_RESET_EN m_vgtMultiPrimIbResetEn; // Last written value of VGT_MULTI_PRIM_IB_RESET_EN register.

    regPA_SC_AA_CONFIG m_paScAaConfigNew;  // PA_SC_AA_CONFIG state that will be written on the next draw.
    regPA_SC_AA_CONFIG m_paScAaConfigLast; // Last written value of PA_SC_AA_CONFIG

    regPA_SU_LINE_STIPPLE_CNTL  m_paSuLineStippleCntl; // Last written value of PA_SU_LINE_STIPPLE_CNTL
    regPA_SC_LINE_STIPPLE       m_paScLineStipple;     // Last written value of PA_SC_LINE_STIPPLE

    static constexpr uint32     InvalidPaSuScModeCntlVal = (7 << PA_SU_SC_MODE_CNTL__POLYMODE_BACK_PTYPE__SHIFT);
    regPA_SU_SC_MODE_CNTL       m_paSuScModeCntl;      // Current written value of PA_SU_SC_MODE_CNTL

    bool             m_hasWaMiscPopsMissedOverlapBeenApplied;
    bool             m_enabledPbb;       // PBB is currently enabled or disabled.
    uint16           m_customBinSizeX;   // Custom bin sizes for PBB.  Zero indicates PBB is not using
    uint16           m_customBinSizeY;   // a custom bin size.

    CachedSettings   m_cachedSettings;   // Cached settings values referenced at draw-time

    DrawTimeHwState  m_drawTimeHwState;  // Tracks certain bits of HW-state that might need to be updated per draw.
    NggState         m_nggState;

    uint8 m_leakCbColorInfoRtv;   // Sticky per-MRT dirty mask of CB_COLORx_INFO state written due to RTV

    // This "list" remembers draw-time VRS rate image to HTile copies that occured in this command buffer and which are
    // still valid. We can skip future copies with the same source and destination until an external write clears a
    // copy mapping (e.g., a CmdBarrier call on the VRS rate image).
    Util::Vector<VrsCopyMapping, 16, Platform> m_validVrsCopies;

    // In order to prevent invalid query results if an app does Begin()/End(), Reset()/Begin()/End(), Resolve() on a
    // query slot in a command buffer (the first End() might overwrite values written by the Reset()), we have to
    // insert an idle before performing the Reset().  This has a high performance penalty.  This structure is used
    // to track memory ranges affected by outstanding End() calls in this command buffer so we can avoid the idle
    // during Reset() if the reset doesn't affect any pending queries.
    Util::IntervalTree<gpusize, bool, Platform>  m_activeOcclusionQueryWriteRanges;

    // Used to sync the ACE and DE in a ganged submit.
    gpusize m_gangedCmdStreamSemAddr;
    uint32  m_semCountAceWaitDe;
    uint32  m_semCountDeWaitAce;

#if PAL_BUILD_GFX11
    gpusize m_swStreamoutDataAddr;
#endif

    uint16 m_baseUserDataReg[HwShaderStage::Last];

#if PAL_BUILD_GFX11
    // Array of valid packed register pairs holding user entries to be written into SGPRs.
    PackedRegisterPair     m_validUserEntryRegPairs[Gfx11MaxPackedUserEntryCountGfx];
    PackedRegisterPair     m_validUserEntryRegPairsCs[Gfx11MaxPackedUserEntryCountCs];
    // A lookup of registers written into m_validUserEntryRegPairs where each index in the lookup maps to each supported
    // shader stage's SGPRs. The value at each index divided by 2 serves as an index into m_validUserEntryRegPairs.
    uint8                  m_validUserEntryRegPairsLookup[Gfx11MaxUserDataIndexCountGfx];
    uint8                  m_validUserEntryRegPairsLookupCs[Gfx11MaxUserDataIndexCountCs];
    // Total number of registers packed into m_validUserEntryRegPairs.
    uint32                 m_numValidUserEntries;
    uint32                 m_numValidUserEntriesCs;
#endif

    // MS/TS pipeline stats query is emulated by shader. A 6-DWORD scratch memory chunk is needed to store for shader
    // to store the three counter values.
    gpusize m_meshPipeStatsGpuAddr;

    gpusize m_globalInternalTableAddr; // If non-zero, the low 32-bits of the global internal table were written here.

    ColorTargetViewStorage  m_colorTargetViewStorage[MaxColorTargets];
    ColorTargetViewStorage  m_colorTargetViewRestoreStorage[MaxColorTargets];
    DepthStencilViewStorage m_depthStencilViewStorage;
    DepthStencilViewStorage m_depthStencilViewRestoreStorage;

    PAL_DISALLOW_DEFAULT_CTOR(UniversalCmdBuffer);
    PAL_DISALLOW_COPY_AND_ASSIGN(UniversalCmdBuffer);
};

// Helper function for managing the logic controlling when to do CE/DE synchronization and invalidating the Kcache.
extern bool HandleCeRinging(
    UniversalCmdBufferState* pState,
    uint32                   currRingPos,
    uint32                   ringInstances,
    uint32                   ringSize);

} // Gfx9
} // Pal
