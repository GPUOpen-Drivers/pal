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

#include "core/hw/gfxip/universalCmdBuffer.h"
#include "core/hw/gfxip/gfx9/gfx9Gds.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9PrefetchMgr.h"
#include "core/hw/gfxip/gfx9/gfx9WorkaroundState.h"
#include "core/hw/gfxip/gfx9/g_gfx9PalSettings.h"
#include "palIntervalTree.h"

#include "palPipelineAbi.h"

namespace Pal
{
namespace Gfx9
{

class GraphicsPipeline;
class UniversalCmdBuffer;

// Structure to track the state of internal command buffer operations.
struct UniversalCmdBufferState
{
    union
    {
        struct
        {
            uint32 isPrecisionOn         :  1; // Whether occlusion query active during execution uses precise data
            uint32 ceStreamDirty         :  1; // A CE RAM Dump command was added to the CE stream since the last Draw
                                               // requires increment & wait on CE counter commands to be added.
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
            uint32 paScAaConfigUpdated   :  1;
            uint32 containsDrawIndirect  :  1;
            uint32 optimizeLinearGfxCpy  :  1;
            uint32 firstDrawExecuted     :  1;
            uint32 reserved              : 22;
        };
        uint32 u32All;
    } flags;
    // According to the UDX implementation, CP uCode and CE programming guide, the ideal DE counter diff amount we
    // should ask the CE to wait for is 1/4 the minimum size (in entries!) of all pieces of memory being ringed.
    // Thus we only need to track this minimum diff amount. If ceWaitOnDeCounterDiff flag is also set, the CE will
    // be asked to wait for a DE counter diff at the next Draw or Dispatch.
    uint32  minCounterDiff;

    // Copy of what will be written into CE RAM for NGG pipelines.
    Util::Abi::PrimShaderCbLayout  primShaderCbLayout;
};

// Represents an "image" of the PM4 headers necessary to write NULL depth-stencil state to hardware. The required
// register writes are grouped into sets based on sequential register addresses, so that we can minimize the amount
// of PM4 space needed by setting several reg's in each packet.
struct NullDepthStencilPm4Img
{
    PM4PFP_SET_CONTEXT_REG       hdrPaScScreenScissor;
    regPA_SC_SCREEN_SCISSOR_TL   paScScreenScissorTl;
    regPA_SC_SCREEN_SCISSOR_BR   paScScreenScissorBr;

    PM4PFP_SET_CONTEXT_REG       hdrDbHtileDataBase;
    regDB_HTILE_DATA_BASE        dbHtileDataBase;

    // Note:  this must be last, because the size of this union is a constant, but the number of registers
    //        written to the GPU is of a variable length.  This struct is copied into the PM4 stream "as is",
    //        so any blank / unused spaces need to be at the end.
    PM4PFP_SET_CONTEXT_REG       hdrDbInfo;
    union
    {
        struct
        {
            regDB_Z_INFO__GFX09          dbZInfo;
            regDB_STENCIL_INFO__GFX09    dbStencilInfo;
            regDB_Z_READ_BASE            dbZReadBase;
            regDB_Z_READ_BASE_HI         dbZReadBaseHi;
            regDB_STENCIL_READ_BASE      dbStencilReadBase;
            regDB_STENCIL_READ_BASE_HI   dbStencilReadBaseHi;
            regDB_Z_WRITE_BASE           dbZWriteBase;
            regDB_Z_WRITE_BASE_HI        dbZWriteBaseHi;
            regDB_STENCIL_WRITE_BASE     dbStencilWriteBase;
            regDB_STENCIL_WRITE_BASE_HI  dbStencilWriteBaseHi;
            regDB_DFSM_CONTROL           dbDfsmControl;
        } gfx9;

    };
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
            uint32 indexOffset            :  1; // Set when startIndex matches the HW value.
            uint32 log2IndexSize          :  1; // Set when log2IndexSize matches the HW value.
            uint32 numInstances           :  1; // Set when numInstances matches the HW value.
            uint32 paScModeCntl1          :  1; // Set when paScModeCntl1 matches the HW value.
            uint32 dbCountControl         :  1; // Set when dbCountControl matches the HW value.
            uint32 vgtMultiPrimIbResetEn  :  1; // Set when vgtMultiPrimIbResetEn matches the HW value.
            uint32 nggIndexBufferBaseAddr :  1; // Set when nggIndexBufferBaseAddr matches the HW value.
            uint32 reserved               : 22; // Reserved bits
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

    uint32                        instanceOffset;         // Current value of the instance offset user data.
    uint32                        vertexOffset;           // Current value of the vertex offset user data.
    uint32                        startIndex;             // Current value of the start index user data.
    uint32                        log2IndexSize;          // Current value of the Log2(sizeof(indexType)) user data.
    uint32                        numInstances;           // Current value of the NUM_INSTANCES state.
    regPA_SC_MODE_CNTL_1          paScModeCntl1;          // Current value of the PA_SC_MODE_CNTL1 register.
    regDB_COUNT_CONTROL           dbCountControl;         // Current value of the DB_COUNT_CONTROL register.
    regVGT_MULTI_PRIM_IB_RESET_EN vgtMultiPrimIbResetEn;  // Current value of the VGT_MULTI_PRIM_IB_RESET_EN register.
    gpusize                       nggIndexBufferBaseAddr; // Current value of the IndexBufferBaseAddr for NGG.
};

struct ColorInfoReg
{
    PM4PFP_SET_CONTEXT_REG header;
    regCB_COLOR0_INFO      cbColorInfo;
};

struct GenericScissorReg
{
    PM4PFP_SET_CONTEXT_REG      header;
    regPA_SC_GENERIC_SCISSOR_TL paScGenericScissorTl;
    regPA_SC_GENERIC_SCISSOR_BR paScGenericScissorBr;
};

constexpr size_t MaxNullColorTargetPm4ImgSize = sizeof(ColorInfoReg) * MaxColorTargets + sizeof(GenericScissorReg);

struct BlendConstReg
{
    PM4_PFP_SET_CONTEXT_REG header;
    regCB_BLEND_RED         red;
    regCB_BLEND_GREEN       green;
    regCB_BLEND_BLUE        blue;
    regCB_BLEND_ALPHA       alpha;
};

struct InputAssemblyStatePm4Img
{
    PM4_PFP_SET_UCONFIG_REG             hdrPrimType;
    regVGT_PRIMITIVE_TYPE               primType;

    PM4_PFP_SET_CONTEXT_REG             hdrVgtMultiPrimIbResetIndex;
    regVGT_MULTI_PRIM_IB_RESET_INDX     vgtMultiPrimIbResetIndex;
};

struct StencilRefMasksReg
{
    PM4_PFP_SET_CONTEXT_REG header;
    regDB_STENCILREFMASK    dbStencilRefMaskFront;
    regDB_STENCILREFMASK_BF dbStencilRefMaskBack;
};

struct StencilRefMaskRmwReg
{
    PM4_ME_REG_RMW dbStencilRefMaskFront;
    PM4_ME_REG_RMW dbStencilRefMaskBack;
};

constexpr size_t MaxStencilSetPm4ImgSize = sizeof(StencilRefMasksReg) > sizeof(StencilRefMaskRmwReg) ?
                                           sizeof(StencilRefMasksReg) : sizeof(StencilRefMaskRmwReg);
struct DepthBoundsStateReg
{
    PM4_PFP_SET_CONTEXT_REG header;
    regDB_DEPTH_BOUNDS_MIN  dbDepthBoundsMin;
    regDB_DEPTH_BOUNDS_MAX  dbDepthBoundsMax;
};

struct TriangleRasterStateReg
{
    PM4_PFP_SET_CONTEXT_REG header;
    regPA_SU_SC_MODE_CNTL   paSuScModeCntl;
};

struct DepthBiasStateReg
{
    PM4_PFP_SET_CONTEXT_REG           header;
    regPA_SU_POLY_OFFSET_CLAMP        paSuPolyOffsetClamp;       // Poly offset clamp value
    regPA_SU_POLY_OFFSET_FRONT_SCALE  paSuPolyOffsetFrontScale;  // Front-facing poly scale
    regPA_SU_POLY_OFFSET_FRONT_OFFSET paSuPolyOffsetFrontOffset; // Front-facing poly offset
    regPA_SU_POLY_OFFSET_BACK_SCALE   paSuPolyOffsetBackScale;   // Back-facing poly scale
    regPA_SU_POLY_OFFSET_BACK_OFFSET  paSuPolyOffsetBackOffset;  // Back-facing poly offset
};

struct PointLineRasterStateReg
{
    PM4_PFP_SET_CONTEXT_REG paSuHeader;
    regPA_SU_POINT_SIZE     paSuPointSize;
    regPA_SU_POINT_MINMAX   paSuPointMinMax;
    regPA_SU_LINE_CNTL      paSuLineCntl;
};

struct GlobalScissorReg
{
    PM4_PFP_SET_CONTEXT_REG    header;
    regPA_SC_WINDOW_SCISSOR_TL topLeft;
    regPA_SC_WINDOW_SCISSOR_BR bottomRight;
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

// Register state for a single plane's x y z and w coordinates.
struct UserClipPlaneStateReg
{
    regPA_CL_UCP_0_X            paClUcpX;
    regPA_CL_UCP_0_Y            paClUcpY;
    regPA_CL_UCP_0_Z            paClUcpZ;
    regPA_CL_UCP_0_W            paClUcpW;
};

// Command for setting up user clip planes.
struct UserClipPlaneStatePm4Img
{
    PM4_PFP_SET_CONTEXT_REG     header;
    UserClipPlaneStateReg       plane[6];
};

// PM4 image for loading context registers from memory
struct LoadDataIndexPm4Img
{
    // PM4 load context regs packet to load the register data from memory
    union
    {
        PM4PFP_LOAD_CONTEXT_REG       loadData;
        PM4PFP_LOAD_CONTEXT_REG_INDEX loadDataIndex;
    };

    // Command space needed, in DWORDs. This field must always be last in the structure to not
    // interfere w/ the actual commands contained within.
    size_t                            spaceNeeded;

};

// Represents an image of the PM4 commands necessary to write RB-plus related info to hardware.
struct RbPlusPm4Img
{
    PM4_PFP_SET_CONTEXT_REG  header;
    regSX_PS_DOWNCONVERT     sxPsDownconvert;
    regSX_BLEND_OPT_EPSILON  sxBlendOptEpsilon;
    regSX_BLEND_OPT_CONTROL  sxBlendOptControl;

    size_t  spaceNeeded;
};

// All NGG related state tracking.
struct NggState
{
    union
    {
        struct
        {
            union
            {
                struct
                {
                    uint8 hasPrimShaderWorkload : 1;
                    uint8 reserved              : 7;
                };
                uint8 u8All;
            } state;

            union
            {
                struct
                {
                    uint8 viewports           : 1;
                    uint8 scissorRects        : 1;
                    uint8 triangleRasterState : 1;
                    uint8 inputAssemblyState  : 1;
                    uint8 reserved            : 4;
                };
                uint8 u8All;
            } dirty;
        };
        uint16 u16All;
    } flags;

    uint16 startIndexReg;       // Register where the index start offset is written
    uint16 log2IndexSizeReg;    // Register where the Log2(sizeof(indexType)) is written
};

// =====================================================================================================================
// GFX9 universal command buffer class: implements GFX9 specific functionality for the UniversalCmdBuffer class.
class UniversalCmdBuffer : public Pal::UniversalCmdBuffer
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
    virtual void CmdFlglSync() override;
    virtual void CmdFlglEnable() override;
    virtual void CmdFlglDisable() override;

    static uint32* BuildSetBlendConst(const BlendConstParams& params, const CmdUtil& cmdUtil, uint32* pCmdSpace);
    static uint32* BuildSetInputAssemblyState(
        const InputAssemblyStateParams& params,
        const Device&                   device,
        uint32*                         pCmdSpace);
    static uint32* BuildSetStencilRefMasks(
        const StencilRefMaskParams& params,
        const CmdUtil&              cmdUtil,
        uint32*                     pCmdSpace);
    static uint32* BuildSetDepthBounds(const DepthBoundsParams& params, const CmdUtil& cmdUtil, uint32* pCmdSpace);
    static uint32* BuildSetDepthBiasState(const DepthBiasParams& params, const CmdUtil& cmdUtil, uint32* pCmdSpace);
    static uint32* BuildSetPointLineRasterState(
        const PointLineRasterStateParams& params,
        const CmdUtil&                    cmdUtil,
        uint32*                           pCmdSpace);
    static uint32* BuildSetGlobalScissor(
        const GlobalScissorParams& params,
        const CmdUtil&             cmdUtil,
        uint32*                    pCmdSpace);
    static uint32* BuildSetUserClipPlane(uint32               firstPlane,
                                         uint32               count,
                                         const UserClipPlane* pPlanes,
                                         const CmdUtil&       cmdUtil,
                                         uint32*              pCmdSpace);

    virtual void CmdBarrier(const BarrierInfo& barrierInfo) override;

    virtual void CmdSetIndirectUserData(
        uint16      tableId,
        uint32      dwordOffset,
        uint32      dwordSize,
        const void* pSrcData) override;

    virtual void CmdSetIndirectUserDataWatermark(
        uint16 tableId,
        uint32 dwordLimit) override;

    void CmdBindTargetsMetadata(const BindTargetParams& params);

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
    virtual void CmdInsertRgpTraceMarker(uint32 numDwords, const void* pData) override;

    virtual void AddQuery(QueryPoolType queryPoolType, QueryControlFlags flags) override;
    virtual void RemoveQuery(QueryPoolType queryPoolType) override;

    virtual void CmdLoadGds(
        HwPipePoint       pipePoint,
        uint32            dstGdsOffset,
        const IGpuMemory& srcGpuMemory,
        gpusize           srcMemOffset,
        uint32            size) override;

    virtual void CmdStoreGds(
        HwPipePoint       pipePoint,
        uint32            srcGdsOffset,
        const IGpuMemory& dstGpuMemory,
        gpusize           dstMemOffset,
        uint32            size,
        bool              waitForWC) override;

    virtual void CmdUpdateGds(
        HwPipePoint       pipePoint,
        uint32            gdsOffset,
        uint32            dataSize,
        const uint32*     pData) override;

    virtual void CmdFillGds(
        HwPipePoint       pipePoint,
        uint32            gdsOffset,
        uint32            fillSize,
        uint32            data) override;

    virtual void CmdLoadBufferFilledSizes(
        const gpusize (&gpuVirtAddr)[MaxStreamOutTargets]) override;

    virtual void CmdSaveBufferFilledSizes(
        const gpusize (&gpuVirtAddr)[MaxStreamOutTargets]) override;

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

    virtual void PushGraphicsState() override;
    virtual void PopGraphicsState() override;

    virtual CmdStream* GetCmdStreamByEngine(uint32 engineType) override;

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

    virtual void CmdSetHiSCompareState0(
        CompareFunc compFunc,
        uint32      compMask,
        uint32      compValue,
        bool        enable) override;

    virtual void CmdSetHiSCompareState1(
        CompareFunc compFunc,
        uint32      compMask,
        uint32      compValue,
        bool        enable) override;

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

    virtual void CmdSaveComputeState(uint32 stateFlags) override;
    virtual void CmdRestoreComputeState(uint32 stateFlags) override;

    virtual void CmdCommentString(const char* pComment) override;

    virtual void CmdExecuteIndirectCmds(
        const IIndirectCmdGenerator& generator,
        const IGpuMemory&            gpuMemory,
        gpusize                      offset,
        uint32                       maximumCount,
        gpusize                      countGpuAddr) override;

    virtual CmdStreamChunk* GetChunkForCmdGeneration(
        const Pal::IndirectCmdGenerator& generator,
        const Pal::Pipeline&             pipeline,
        uint32                           maxCommands,
        uint32*                          pCommandsInChunk,
        gpusize*                         pEmbeddedDataAddr,
        uint32*                          pEmbeddedDataSize) override;

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

    void SetPrimShaderWorkload() { m_nggState.flags.state.hasPrimShaderWorkload = 1; }
    virtual bool HasPrimShaderWorkload() const override { return m_nggState.flags.state.hasPrimShaderWorkload; }

    uint32 BuildScissorRectImage(
        bool               multipleViewports,
        ScissorRectPm4Img* pScissorRectImg) const;

    template <bool pm4OptImmediate>
    uint32* ValidateScissorRects(uint32* pDeCmdSpace);
    uint32* ValidateScissorRects(uint32* pDeCmdSpace);

    bool NeedsToValidateScissorRects(const bool pm4OptImmediate) const;
    bool NeedsToValidateScissorRects() const;

    virtual void CpCopyMemory(gpusize dstAddr, gpusize srcAddr, gpusize numBytes) override;

protected:
    virtual ~UniversalCmdBuffer() {}

    virtual Result AddPreamble() override;
    virtual Result AddPostamble() override;

    virtual void ResetState() override;

    virtual void WriteEventCmd(const BoundGpuMemory& boundMemObj, HwPipePoint pipePoint, uint32 data) override;

    virtual void CmdXdmaWaitFlipPending() override;

    virtual void SetGraphicsState(const GraphicsState& newGraphicsState) override;

    virtual void InheritStateFromCmdBuf(const GfxCmdBuffer* pCmdBuffer) override;

    template <bool pm4OptImmediate>
    uint32* ValidateBinSizes(
        const GraphicsPipeline&  pipeline,
        const ColorBlendState*   pColorBlendState,
        bool                     disableDfsm,
        uint32*                  pDeCmdSpace);

    bool ShouldEnablePbb(
        const GraphicsPipeline&  pipeline,
        const ColorBlendState*   pColorBlendState,
        const DepthStencilState* pDepthStencilState,
        const MsaaState*         pMsaaState) const;

    template <bool indexed, bool indirect>
    void ValidateDraw(const ValidateDrawInfo& drawInfo);

    template <bool indexed, bool indirect, bool pm4OptImmediate>
    void ValidateDraw(const ValidateDrawInfo& drawInfopDeCmdSpace);

    template <bool indexed, bool indirect, bool pm4OptImmediate, bool pipelineDirty>
    uint32* ValidateDraw(
        const ValidateDrawInfo& drawInfo,
        uint32*                 pDeCmdSpace);

    template <bool indexed, bool indirect, bool pm4OptImmediate, bool pipelineDirty, bool stateDirty>
    uint32* ValidateDraw(
        const ValidateDrawInfo& drawInfo,
        uint32*                 pDeCmdSpace);

    template <bool indexed, bool indirect, bool pm4OptImmediate, bool pipelineDirty, bool stateDirty, bool isNgg>
    uint32* ValidateDraw(
        const ValidateDrawInfo& drawInfo,
        uint32*                 pDeCmdSpace);

    template <bool indexed,
              bool indirect,
              bool pm4OptImmediate,
              bool pipelineDirty,
              bool stateDirty,
              bool isNgg,
              bool isNggFastLaunch>
    uint32* ValidateDraw(
        const ValidateDrawInfo& drawInfo,
        uint32*                 pDeCmdSpace);

    template <bool indexed, bool indirect, bool isNgg, bool isNggFastLaunch, bool pm4OptImmediate>
    uint32* ValidateDrawTimeHwState(
        regPA_SC_MODE_CNTL_1          paScModeCntl1,
        regDB_COUNT_CONTROL           dbCountControl,
        regVGT_MULTI_PRIM_IB_RESET_EN vgtMultiPrimIbResetEn,
        const ValidateDrawInfo&       drawInfo,
        uint32*                       pDeCmdSpace);

    template <bool indexed, bool indirect, bool pm4OptImmediate>
    uint32* ValidateDrawTimeNggFastLaunchState(
        const ValidateDrawInfo& drawInfo,
        uint32*                 pDeCmdSpace);

    void ValidateExecuteNestedCmdBuffers(const UniversalCmdBuffer& cmdBuffer);

    // Gets vertex offset register address
    uint16 GetVertexOffsetRegAddr() const { return m_vertexOffsetReg; }

    // Gets instance offset register address. It always immediately follows the vertex offset register.
    uint16 GetInstanceOffsetRegAddr() const { return m_vertexOffsetReg + 1; }

    // Gets the start index offset register address
    uint16 GetStartIndexRegAddr() const { return m_nggState.startIndexReg; }

    virtual void P2pBltWaCopyBegin(
        const GpuMemory* pDstMemory,
        uint32           regionCount,
        const gpusize*   pChunkAddrs) override;
    virtual void P2pBltWaCopyNextRegion(gpusize chunkAddr) override;
    virtual void P2pBltWaCopyEnd() override;

private:
    template <bool issueSqttMarkerEvent, bool viewInstancingEnable>
    static void PAL_STDCALL CmdDraw(
        ICmdBuffer* pCmdBuffer,
        uint32      firstVertex,
        uint32      vertexCount,
        uint32      firstInstance,
        uint32      instanceCount);
    template <bool issueSqttMarkerEvent, bool isNggFastLaunch, bool viewInstancingEnable>
    static void PAL_STDCALL CmdDrawIndexed(
        ICmdBuffer* pCmdBuffer,
        uint32      firstIndex,
        uint32      indexCount,
        int32       vertexOffset,
        uint32      firstInstance,
        uint32      instanceCount);
    template <bool issueSqttMarkerEvent, bool viewInstancingEnable>
    static void PAL_STDCALL CmdDrawIndirectMulti(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint32            stride,
        uint32            maximumCount,
        gpusize           countGpuAddr);
    template <bool issueSqttMarkerEvent, bool isNggFastLaunch, bool viewInstancingEnable>
    static void PAL_STDCALL CmdDrawIndexedIndirectMulti(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint32            stride,
        uint32            maximumCount,
        gpusize           countGpuAddr);

    template <bool IssueSqttMarkerEvent>
    static void PAL_STDCALL CmdDispatch(
        ICmdBuffer* pCmdBuffer,
        uint32      x,
        uint32      y,
        uint32      z);
    template <bool IssueSqttMarkerEvent>
    static void PAL_STDCALL CmdDispatchIndirect(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset);
    template <bool IssueSqttMarkerEvent>
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
    uint32* UpdateDbCountControl(uint32               log2SampleRate,
                                 regDB_COUNT_CONTROL* pDbCountControl,
                                 uint32*              pDeCmdSpace);

    bool ForceWdSwitchOnEop(const GraphicsPipeline& pipeline, const ValidateDrawInfo& drawInfo) const;

    template <bool pm4OptImmediate>
    uint32* ValidateViewports(uint32* pDeCmdSpace);
    uint32* ValidateViewports(uint32* pDeCmdSpace);

    uint32* WriteNullColorTargets(
        uint32* pCmdSpace,
        uint32  newColorTargetMask,
        uint32  oldColorTargetMask);
    uint32* WriteNullDepthTarget(uint32* pCmdSpace);

    uint32* FlushStreamOut(uint32* pDeCmdSpace);

    bool HasStreamOutBeenSet() const;

    void SynchronizeCeDeCounters(
        uint32** ppDeCmdSpace,
        uint32** ppCeCmdSpace);

    uint32* IncrementDeCounter(uint32* pDeCmdSpace);

    Pm4Predicate PacketPredicate() const { return static_cast<Pm4Predicate>(m_gfxCmdBufState.packetPredicate); }

    template <bool IssueSqttMarkerEvent>
    void SetDispatchFunctions();
    void SetUserDataValidationFunctions(bool tessEnabled, bool gsEnabled, bool isNgg);

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

    template <uint32 AlignmentInDwords>
    void RelocateUserDataTable(
        CeRamUserDataTableState* pTable,
        uint32                   offsetInDwords,
        uint32                   dwordsNeeded);
    uint32* UploadToUserDataTable(
        CeRamUserDataTableState* pTable,
        uint32                   offsetInDwords,
        uint32                   dwordsNeeded,
        const uint32*            pSrcData,
        uint32                   highWatermark,
        uint32*                  pCeCmdSpace);
    uint32* DumpUserDataTable(
        CeRamUserDataTableState* pTable,
        uint32                   offsetInDwords,
        uint32                   dwordsNeeded,
        uint32*                  pCeCmdSpace);

    template <bool HasPipelineChanged, bool TessEnabled, bool GsEnabled, bool VsEnabled>
    uint32* ValidateGraphicsUserData(
        const GraphicsPipelineSignature* pPrevSignature,
        uint32*                          pDeCmdSpace);

    template <bool HasPipelineChanged>
    uint32* ValidateComputeUserData(
        const ComputePipelineSignature* pPrevSignature,
        uint32*                         pDeCmdSpace);

    template <bool TessEnabled, bool GsEnabled, bool VsEnabled>
    uint32* WriteDirtyUserDataEntriesToSgprsGfx(
        const GraphicsPipelineSignature* pPrevSignature,
        uint8                            alreadyWrittenStageMask,
        uint32*                          pDeCmdSpace);

    uint32* WriteDirtyUserDataEntriesToUserSgprsCs(
        uint32* pDeCmdSpace);

    template <typename PipelineSignature>
    uint32* WriteDirtyUserDataEntriesToCeRam(
        const PipelineSignature* pPrevSignature,
        const PipelineSignature* pCurrSignature,
        uint32*                  pCeCmdSpace);

    template <bool TessEnabled, bool GsEnabled, bool VsEnabled>
    uint8 FixupUserSgprsOnPipelineSwitch(
        const GraphicsPipelineSignature* pPrevSignature,
        uint32**                         ppDeCmdSpace);

    template <typename PipelineSignature>
    void FixupSpillTableOnPipelineSwitch(
        const PipelineSignature* pPrevSignature,
        const PipelineSignature* pCurrSignature);

    void LeakNestedCmdBufferState(
        const UniversalCmdBuffer& cmdBuffer);

    uint8 CheckStreamOutBufferStridesOnPipelineSwitch();
    uint32* UploadStreamOutBufferStridesToCeRam(
        uint8   dirtyStrideMask,
        uint32* pCeCmdSpace);

    Extent2d GetColorBinSize() const;
    Extent2d GetDepthBinSize() const;
    void SetPaScBinnerCntl0(const GraphicsPipeline&  pipeline,
                            const ColorBlendState*   pColorBlendState,
                            Extent2d                 size,
                            bool                     disableDfsm);

    void SendFlglSyncCommands(FlglRegSeqType type);

    void DescribeDraw(Developer::DrawDispatchType cmdType);

    void P2pBltWaSync();

    uint32* UpdateNggRingData(
        uint32* pDeCmdSpace);

    uint32* BuildWriteViewId(
        uint32  viewId,
        uint32* pCmdSpace);

    void SwitchDrawFunctions(
        bool viewInstancingEnable,
        bool nggFastLuanch);

    const Device&   m_device;
    const CmdUtil&  m_cmdUtil;

    // Prefetch manager is for pre-loading / warming L2 caches on behalf of the command buffer
    PrefetchMgr  m_prefetchMgr;
    CmdStream    m_deCmdStream;
    CmdStream    m_ceCmdStream;

    // Tracks the user-data signature of the currently active compute & graphics pipelines.
    const ComputePipelineSignature*   m_pSignatureCs;
    const GraphicsPipelineSignature*  m_pSignatureGfx;

    uint64      m_pipelineCtxPm4Hash;   // Hash of current pipeline's PM4 image for context registers.
    ShaderHash  m_pipelinePsHash;       // Hash of current pipeline's pixel shader program.
    union
    {
        struct
        {
            uint32  usesTess :  1;
            uint32  usesGs   :  1;
            uint32  isNgg    :  1;
            uint32  reserved : 29;
        };
        uint32 u32All;
    }  m_pipelineFlags;  // Flags describing the currently active pipeline stages.

    // Function pointers which validate all graphics user-data at Draw-time for the cases where the pipeline is
    // changing and cases where it is not.
    ValidateUserDataGfxFunc  m_pfnValidateUserDataGfx;
    ValidateUserDataGfxFunc  m_pfnValidateUserDataGfxPipelineSwitch;

    struct
    {
        // Client-specified high-watermark for each indirect user-data table. This indicates how much of each table
        // is dumped from CE RAM to memory before a draw or dispatch.
        uint32              watermark : 31;
        // Tracks whether or not this indirect user-data table was modified somewhere in the command buffer.
        uint32              modified  :  1;
        uint32*             pData;  // Tracks the contents of each indirect user-data table.

        CeRamUserDataTableState  state;  // Tracks the state for the indirect user-data table

    }  m_indirectUserDataInfo[MaxIndirectUserDataTables];

    struct
    {
        CeRamUserDataTableState  state; // Tracks the state of the NGG state table
    }  m_nggTable;

    struct
    {
        CeRamUserDataTableState  stateCs;  // Tracks the state of the compute spill table
        CeRamUserDataTableState  stateGfx; // Tracks the state of the graphics spill table
    }  m_spillTable;

    struct
    {
        CeRamUserDataTableState  state;  // Tracks the state of the stream-out SRD table

        BufferSrd  srd[MaxStreamOutTargets];    // Current stream-out target SRD's
    }  m_streamOut;

    WorkaroundState              m_workaroundState;
    UniversalCmdBufferState      m_state; // State tracking for internal cmd buffer operations

    regVGT_DMA_INDEX_TYPE__GFX09 m_vgtDmaIndexType;     // Register setting for VGT_DMA_INDEX_TYPE
    regSPI_VS_OUT_CONFIG         m_spiVsOutConfig;      // Register setting for VS_OUT_CONFIG
    regSPI_PS_IN_CONTROL         m_spiPsInControl;      // Register setting for PS_IN_CONTROL
    uint16                       m_vertexOffsetReg;     // Register where the vertex start offset is written
    uint16                       m_drawIndexReg;        // Register where the draw index is written
    RbPlusPm4Img                 m_rbPlusPm4Img;        // PM4 image for RB Plus register state

    const uint32                 m_log2NumSes;
    const uint32                 m_log2NumRbPerSe;

    regPA_SC_BINNER_CNTL_0       m_paScBinnerCntl0;
    regPA_SC_BINNER_CNTL_0       m_savedPaScBinnerCntl0; // Value of PA_SC_BINNER_CNTL0 selected by settings
    uint32                       m_log2NumSamples;       // Last written value of PA_SC_AA_CONFIG.MSAA_NUM_SAMPLES.

    BinningMode      m_binningMode;                     // Last value programmed into paScBinnerCntl0.BINNING_MODE
    BinningOverride  m_pbbStateOverride;                // Sets PBB on/off as per dictated by the new bound pipeline.
    bool             m_enabledPbb;                      // PBB is currently enabled or disabled.
    uint16           m_customBinSizeX;                  // Custom bin sizes for PBB.  Zero indicates PBB is not using
    uint16           m_customBinSizeY;                  // a custom bin size.

    union
    {
        struct
        {
            uint32 tossPointMode              :  3; // The currently enabled "TossPointMode" global setting
            uint32 hiDepthDisabled            :  1; // True if Hi-Depth is disabled by settings
            uint32 hiStencilDisabled          :  1; // True if Hi-Stencil is disabled by settings
            uint32 disableDfsm                :  1; // A copy of the disableDfsm setting.
            uint32 disableDfsmPsUav           :  1; // A copy of the disableDfsmPsUav setting.
            uint32 disableBatchBinning        :  1; // True if binningMode is disabled.
            uint32 disablePbbPsKill           :  1; // True if PBB should be disabled for pipelines using PS Kill
            uint32 disablePbbNoDb             :  1; // True if PBB should be disabled for pipelines with no DB
            uint32 disablePbbBlendingOff      :  1; // True if PBB should be disabled for pipelines with no blending
            uint32 disablePbbAppendConsume    :  1; // True if PBB should be disabled for pipelines with append/consume
            uint32 disableWdLoadBalancing     :  1; // True if wdLoadBalancingMode is disabled.
            uint32 ignoreCsBorderColorPalette :  1; // True if compute border-color palettes should be ignored
            uint32 blendOptimizationsEnable   :  1; // A copy of the blendOptimizationsEnable setting.
            uint32 outOfOrderPrimsEnable      :  2; // The out-of-order primitive rendering mode allowed by settings
            uint32 nggWdPageFaultWa           :  1; // True if the NGG Work Distributer pagefault workaround is enabled
            uint32 checkDfsmEqaaWa            :  1; // True if settings are such that the DFSM + EQAA workaround is on.
            uint32 scissorChangeWa            :  1; // True if the scissor register workaround is enabled
            uint32 issueSqttMarkerEvent       :  1; // True if settings are such that we need to issue SQ thread trace
                                                    // marker events on draw.
            uint32 batchBreakOnNewPs          :  1; // True if a BREAK_BATCH should be inserted when switching pixel
                                                    // shaders.
            uint32 padParamCacheSpace         :  1; // True if this command buffer should pad used param-cache space to
                                                    // reduce context rolls.
            uint32 reserved                   :  9;
        };
        uint32 u32All;
    } m_cachedSettings;

    DrawTimeHwState  m_drawTimeHwState;  // Tracks certain bits of HW-state that might need to be updated per draw.

    NggState m_nggState;

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

} // Gfx9
} // Pal
