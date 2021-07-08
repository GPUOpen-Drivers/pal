/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "palDevice.h"

namespace Pal
{

class      CmdStream;
class      PipelineUploader;
enum class IndexType : uint32;

namespace Gfx9
{

class Device;

// The ACQUIRE_MEM and RELEASE_MEM packets can issue one "TC cache op" in addition to their other operations. This
// behavior is controlled by CP_COHER_CNTL but only certain bit combinations are legal. To be clear what the HW can do
// and avoid illegal combinations we abstract those bit combinations using this enum.
enum class TcCacheOp : uint32
{
    Nop = 0,   // Do nothing.
    WbInvL1L2, // Flush and invalidate all TCP and TCC data.
    WbInvL2Nc, // Flush and invalidate all TCC data that used the non-coherent MTYPE.
    WbL2Nc,    // Flush all TCC data that used the non-coherent MTYPE.
    WbL2Wc,    // Flush all TCC data that used the write-combined MTYPE.
    InvL2Nc,   // Invalidate all TCC data that used the non-coherent MTYPE.
    InvL2Md,   // Invalidate the TCC's read-only metadata cache.
    InvL1,     // Invalidate all TCP data.
    InvL1Vol,  // Invalidate all volatile TCP data.
    Count
};

// GCR_CNTL bit fields for ACQUIRE_MEM and RELEASE_MEM are slightly different.
union Gfx10AcquireMemGcrCntl
{
    struct
    {
        uint32  gliInv     :  2;
        uint32  gl1Range   :  2;
        uint32  glmWb      :  1;
        uint32  glmInv     :  1;
        uint32  glkWb      :  1;
        uint32  glkInv     :  1;
        uint32  glvInv     :  1;
        uint32  gl1Inv     :  1;
        uint32  gl2Us      :  1;
        uint32  gl2Range   :  2;
        uint32  gl2Discard :  1;
        uint32  gl2Inv     :  1;
        uint32  gl2Wb      :  1;
        uint32  seq        :  2;
        uint32             : 14;
    } bits;

    uint32  u32All;
};

union Gfx10ReleaseMemGcrCntl
{
    struct
    {
        uint32  glmWb      :  1;
        uint32  glmInv     :  1;
        uint32  glvInv     :  1;
        uint32  gl1Inv     :  1;
        uint32  gl2Us      :  1;
        uint32  gl2Range   :  2;
        uint32  gl2Discard :  1;
        uint32  gl2Inv     :  1;
        uint32  gl2Wb      :  1;
        uint32  seq        :  2;
        uint32  reserved1  :  1;
        uint32             : 18;
        uint32  reserved2  :  1;
    } bits;

    uint32  u32All;
};

// This table was taken from the ACQUIRE_MEM packet spec.
static constexpr uint32 Gfx9TcCacheOpConversionTable[] =
{
    0,                                                                                                         // Nop
    Gfx09_10::CP_COHER_CNTL__TC_WB_ACTION_ENA_MASK | Gfx09_10::CP_COHER_CNTL__TC_ACTION_ENA_MASK,              // WbInvL1L2
    Gfx09_10::CP_COHER_CNTL__TC_WB_ACTION_ENA_MASK | Gfx09_10::CP_COHER_CNTL__TC_ACTION_ENA_MASK
                                                   | Gfx09_10::CP_COHER_CNTL__TC_NC_ACTION_ENA_MASK,           // WbInvL2Nc
    Gfx09_10::CP_COHER_CNTL__TC_WB_ACTION_ENA_MASK | Gfx09_10::CP_COHER_CNTL__TC_NC_ACTION_ENA_MASK,           // WbL2Nc
    Gfx09_10::CP_COHER_CNTL__TC_WB_ACTION_ENA_MASK | Gfx09_10::CP_COHER_CNTL__TC_WC_ACTION_ENA_MASK,           // WbL2Wc
    Gfx09_10::CP_COHER_CNTL__TC_ACTION_ENA_MASK    | Gfx09_10::CP_COHER_CNTL__TC_NC_ACTION_ENA_MASK,           // InvL2Nc
    Gfx09_10::CP_COHER_CNTL__TC_ACTION_ENA_MASK    | Gfx09_10::CP_COHER_CNTL__TC_INV_METADATA_ACTION_ENA_MASK, // InvL2Md
    Gfx09_10::CP_COHER_CNTL__TCL1_ACTION_ENA_MASK,                                                             // InvL1
    Gfx09_10::CP_COHER_CNTL__TCL1_ACTION_ENA_MASK  | Gfx09_10::CP_COHER_CNTL__TCL1_VOL_ACTION_ENA_MASK,        // InvL1Vol
};

// In addition to the a TC cache op, ACQUIRE_MEM can flush or invalidate additional caches independently. It is easiest
// to capture all of this information if we use an input structure for BuildAcquireMem.
struct AcquireMemInfo
{
    union
    {
        struct
        {
            uint32 usePfp      :  1; // If true the PFP will process this packet. Only valid on the universal engine.
            uint32 invSqI$     :  1; // Invalidate the SQ instruction caches.
            uint32 invSqK$     :  1; // Invalidate the SQ scalar caches.
            uint32 flushSqK$   :  1; // Flush the SQ scalar caches.
            uint32 wbInvCbData :  1; // Flush and invalidate the CB data cache. Only valid on the universal engine.
            uint32 wbInvDb     :  1; // Flush and invalidate the DB data and metadata caches. Only valid on universal.
            uint32 reserved    : 26;
        };
        uint32 u32All;
    } flags;

    EngineType          engineType;
    regCP_ME_COHER_CNTL cpMeCoherCntl; // All "dest base" bits to wait on. Only valid on the universal engine.
    TcCacheOp           tcCacheOp;     // An optional, additional cache operation to issue.

    // These define the address range being acquired. Use FullSyncBaseAddr and FullSyncSize for a global acquire.
    gpusize             baseAddress;
    gpusize             sizeBytes;
};

// Explicit version of AcquireMemInfo struct.
struct ExplicitAcquireMemInfo
{
    union
    {
        struct
        {
            uint32 usePfp           :  1; // If true the PFP will process this packet. Only valid on the universal engine.
            uint32 reservedFutureHw :  1; // Placeholder
            uint32 reserved         : 30;
        };
        uint32 u32All;
    } flags;

    EngineType engineType;
    uint32     coherCntl;
    uint32     gcrCntl;

    // These define the address range being acquired. Use FullSyncBaseAddr and FullSyncSize for a global acquire.
    gpusize    baseAddress;
    gpusize    sizeBytes;
};

// To easily see the the differences between ReleaseMem and AcquireMem, we want to use an input structure
// for BuildAcquireMem.
struct ReleaseMemInfo
{
    EngineType     engineType;
    VGT_EVENT_TYPE vgtEvent;
    TcCacheOp      tcCacheOp;  // The cache operation to issue.
    gpusize        dstAddr;
    uint32         dataSel;    // One of the data_sel_*_release_mem enumerations
    uint64         data;       // data to write, ignored except for DATA_SEL_SEND_DATA{32,64}
};

// Explicit version of ReleaseMemInfo struct.
struct ExplicitReleaseMemInfo
{
    EngineType       engineType;
    VGT_EVENT_TYPE   vgtEvent;
    uint32           coherCntl;
    uint32           gcrCntl;
    gpusize          dstAddr;
    uint32           dataSel;    // One of the data_sel_*_release_mem enumerations
    uint64           data;       // data to write, ignored except for DATA_SEL_SEND_DATA{32,64}
};

// The "official" "event-write" packet definition (see:  PM4_MEC_EVENT_WRITE) contains "extra" dwords that aren't
// necessary (and would cause problems if they existed) for event writes other than "".  Define a "plain" event-write
// packet definition here.
struct PM4_ME_NON_SAMPLE_EVENT_WRITE
{
    PM4_ME_TYPE_3_HEADER  header;
    uint32                ordinal2;
};

// Data required to perform a DMA Data transfer (aka CPDMA).
//
// Note that the "sync" flag should be set in almost all cases. The two exceptions are:
//   1. The caller will manually synchronize the CP DMA engine using another DMA.
//   2. The caller is operating under "CoherCopy/HwPipePostBlt" semantics and a CmdBarrier call will be issued. This
//      case is commonly referred to as a "CP Blt".
//
// In case #2, the caller must update the GfxCmdBufferState by calling the relevant SetGfxCmdBuf* functions.
// Furthermore, the caller must not set "disWc" because write-confirms are necessary for the barrier to guarantee
// that the CP DMA writes have made it to their destination (memory, L2, etc.).
struct DmaDataInfo
{
    PFP_DMA_DATA_dst_sel_enum     dstSel;
    gpusize                       dstAddr;        // Destination address for dstSel Addr or offset for GDS
    PFP_DMA_DATA_das_enum         dstAddrSpace;   // Destination address space
    PFP_DMA_DATA_src_sel_enum     srcSel;
    uint32                        srcData;        // Source data for srcSel data or offset for srcSel GDS
    gpusize                       srcAddr;        // Source gpu virtual address
    PFP_DMA_DATA_sas_enum         srcAddrSpace;   // Source address space
    uint32                        numBytes;       // Number of bytes to copy
    bool                          usePfp;         // true chooses PFP engine, false chooses ME
    bool                          sync;           // if true, all command processing on the selected engine
                                                  //    (see: usePfp) is halted until this packet is finished
    bool                          disWc;          // set to disable write-confirm
    bool                          rawWait;        // Read-after-write, forces the CP to wait for all previous DMA ops
                                                  //    to finish before starting this one.
    Pm4Predicate                  predicate;      // Set if currently using predication
};

// Data required to build a write_data packet. We try to set up this struct so that zero-initializing gives reasonable
// values for rarely changed members like predicate, dontWriteConfirm, etc.
struct WriteDataInfo
{
    EngineType   engineType;        // Which PAL engine will this packet be executed on?
    gpusize      dstAddr;           // Destination GPU memory address or memory mapped register offset.
    uint32       engineSel;         // Which CP engine executes this packet (see XXX_WRITE_DATA_engine_sel_enum)
                                    // Ignored on the MEC.
    uint32       dstSel;            // Where to write the data (see XXX_WRITE_DATA_dst_sel_enum)
    Pm4Predicate predicate;         // If this packet respects predication (zero defaults to disabled).
    bool         dontWriteConfirm;  // If the engine should continue immediately without waiting for a write-confirm.
    bool         dontIncrementAddr; // If the engine should write every DWORD to the same destination address.
                                    // Some memory mapped registers use this to stream in an array of data.
};

// On different hardware families, some registers have different register offsets. This structure stores the register
// offsets for some of these registers.
struct RegisterInfo
{
    uint16  mmRlcPerfmonClkCntl;
    uint16  mmRlcSpmGlobalMuxselAddr;
    uint16  mmRlcSpmGlobalMuxselData;
    uint16  mmRlcSpmSeMuxselAddr;
    uint16  mmRlcSpmSeMuxselData;
    uint16  mmEaPerfResultCntl;
    uint16  mmAtcPerfResultCntl;
    uint16  mmAtcL2PerfResultCntl;
    uint16  mmMcVmL2PerfResultCntl;
    uint16  mmRpbPerfResultCntl;
    uint16  mmSpiShaderPgmLoLs;
    uint16  mmSpiShaderPgmLoEs;
    uint16  mmVgtGsMaxPrimsPerSubGroup;
    uint16  mmDbDfsmControl;
    uint16  mmUserDataStartHsShaderStage;
    uint16  mmUserDataStartGsShaderStage;
    uint16  mmPaStereoCntl;
    uint16  mmPaStateStereoX;
    uint16  mmComputeShaderChksum;
};

// Pre-baked commands to prefetch (prime caches) for a pipeline.  This can either be done with a PRIME_UTCL2 packet,
// which will prime the UTCL2 (L2 TLB) or with a DMA_DATA packet, which will also prime GL2.
struct PipelinePrefetchPm4
{
    union
    {
        PM4_PFP_DMA_DATA    dmaData;
        PM4_PFP_PRIME_UTCL2 primeUtcl2;
    };
    uint32 spaceNeeded;
};

// =====================================================================================================================
// Utility class which provides routines to help build PM4 packets.
class CmdUtil
{
public:
    explicit CmdUtil(const Device& device);
    ~CmdUtil() {}

    // Returns the number of DWORDs that are required to chain two chunks
    static uint32 ChainSizeInDwords(EngineType engineType);
    static constexpr uint32 CondIndirectBufferSize        = PM4_PFP_COND_INDIRECT_BUFFER_SIZEDW__CORE;
    static constexpr uint32 DispatchDirectSize            = PM4_PFP_DISPATCH_DIRECT_SIZEDW__CORE;
    static constexpr uint32 DispatchIndirectMecSize       = PM4_MEC_DISPATCH_INDIRECT_SIZEDW__CORE;
    static constexpr uint32 DrawIndexAutoSize             = PM4_PFP_DRAW_INDEX_AUTO_SIZEDW__CORE;
    static constexpr uint32 DrawIndex2Size                = PM4_PFP_DRAW_INDEX_2_SIZEDW__CORE;
    static constexpr uint32 DrawIndexOffset2Size          = PM4_PFP_DRAW_INDEX_OFFSET_2_SIZEDW__CORE;
    static constexpr uint32 DispatchTaskMeshGfxSize       = PM4_ME_DISPATCH_TASKMESH_GFX_SIZEDW__GFX10COREPLUS;
    static constexpr uint32 DispatchTaskMeshDirectMecSize =
        PM4_MEC_DISPATCH_TASKMESH_DIRECT_ACE_SIZEDW__GFX10COREPLUS;
    static constexpr uint32 DispatchTaskMeshIndirectMecSize =
        PM4_MEC_DISPATCH_TASKMESH_INDIRECT_MULTI_ACE_SIZEDW__GFX10COREPLUS;
    static constexpr uint32 MinNopSizeInDwords      = 1; // all gfx9 HW supports 1-DW NOP packets

    static_assert (PM4_PFP_COND_EXEC_SIZEDW__CORE == PM4_MEC_COND_EXEC_SIZEDW__CORE,
                   "Conditional execution packet size does not match between PFP and compute engines!");
    static_assert (PM4_PFP_COND_EXEC_SIZEDW__CORE == PM4_CE_COND_EXEC_SIZEDW__HASCE,
                   "Conditional execution packet size does not match between PFP and constant engines!");
    static constexpr uint32 CondExecSizeDwords        = PM4_PFP_COND_EXEC_SIZEDW__CORE;
    static constexpr uint32 ContextRegRmwSizeDwords   = PM4_ME_CONTEXT_REG_RMW_SIZEDW__CORE;
    static constexpr uint32 RegRmwSizeDwords          = PM4_ME_REG_RMW_SIZEDW__CORE;
    static constexpr uint32 ConfigRegSizeDwords       = PM4_PFP_SET_UCONFIG_REG_SIZEDW__CORE;
    static constexpr uint32 ContextRegSizeDwords      = PM4_PFP_SET_CONTEXT_REG_SIZEDW__CORE;
    static constexpr uint32 DmaDataSizeDwords         = PM4_PFP_DMA_DATA_SIZEDW__CORE;
    static constexpr uint32 NumInstancesDwords        = PM4_PFP_NUM_INSTANCES_SIZEDW__CORE;
    static constexpr uint32 OcclusionQuerySizeDwords  = PM4_PFP_OCCLUSION_QUERY_SIZEDW__CORE;
    static constexpr uint32 ShRegSizeDwords           = PM4_ME_SET_SH_REG_SIZEDW__CORE;
    static constexpr uint32 ShRegIndexSizeDwords      = PM4_PFP_SET_SH_REG_INDEX_SIZEDW__CORE;
    static constexpr uint32 WaitRegMemSizeDwords      = PM4_ME_WAIT_REG_MEM_SIZEDW__CORE;
    static constexpr uint32 WaitRegMem64SizeDwords    = PM4_ME_WAIT_REG_MEM64_SIZEDW__CORE;
    static constexpr uint32 WriteDataSizeDwords       = PM4_ME_WRITE_DATA_SIZEDW__CORE;
    static constexpr uint32 WriteNonSampleEventDwords = (sizeof(PM4_ME_NON_SAMPLE_EVENT_WRITE) / sizeof(uint32));

    // The INDIRECT_BUFFER and COND_INDIRECT_BUFFER packet have a hard-coded IB size of 20 bits.
    static constexpr uint32 MaxIndirectBufferSizeDwords = (1 << 20) - 1;

    // The COPY_DATA src_reg_offset and dst_reg_offset fields have a bit-width of 18 bits.
    static constexpr uint32 MaxCopyDataRegOffset = (1 << 18) - 1;

    ///@note GFX10 added the ability to do ranged checks when for Flush/INV ops to GL1/GL2 (so that you can just
    ///      Flush/INV necessary lines instead of the entire cache). Since these caches are physically tagged, this
    ///      can invoke a high penalty for large surfaces so limit the surface size allowed.
    static constexpr uint64 Gfx10AcquireMemGl1Gl2RangedCheckMaxSurfaceSizeBytes = (64 * 1024);

    static bool IsContextReg(uint32 regAddr);
    static bool IsUserConfigReg(uint32 regAddr);
    static bool IsShReg(uint32 regAddr);

    static ME_WAIT_REG_MEM_function_enum WaitRegMemFunc(CompareFunc compareFunc);

    // Checks if the register offset provided can be read or written using a COPY_DATA packet.
    static bool CanUseCopyDataRegOffset(uint32 regOffset) { return (((~MaxCopyDataRegOffset) & regOffset) == 0); }

    bool CanUseCsPartialFlush(EngineType engineType) const;

    size_t BuildAcquireMem(
        const AcquireMemInfo& acquireMemInfo,
        void*                 pBuffer) const;
    size_t ExplicitBuildAcquireMem(
        const ExplicitAcquireMemInfo& acquireMemInfo,
        void*                         pBuffer) const;
    static size_t BuildAtomicMem(
        AtomicOp atomicOp,
        gpusize  dstMemAddr,
        uint64   srcData,
        void*    pBuffer);
    static size_t BuildClearState(
        PFP_CLEAR_STATE_cmd_enum command,
        void*                    pBuffer);
    static size_t BuildCondExec(
        gpusize gpuVirtAddr,
        uint32  sizeInDwords,
        void*   pBuffer);
    static size_t BuildCondIndirectBuffer(
        CompareFunc compareFunc,
        gpusize     compareGpuAddr,
        uint64      data,
        uint64      mask,
        bool        constantEngine,
        void*       pBuffer);
    static size_t BuildContextControl(
        const PM4_PFP_CONTEXT_CONTROL& contextControl,
        void*                          pBuffer);
    size_t BuildContextRegRmw(
        uint32 regAddr,
        uint32 regMask,
        uint32 regData,
        void*  pBuffer) const;
    size_t BuildRegRmw(
        uint32 regAddr,
        uint32 orMask,
        uint32 andMask,
        void*  pBuffer) const;
    static size_t BuildCopyDataGraphics(
        uint32                        engineSel,
        ME_COPY_DATA_dst_sel_enum     dstSel,
        gpusize                       dstAddr,
        ME_COPY_DATA_src_sel_enum     srcSel,
        gpusize                       srcAddr,
        ME_COPY_DATA_count_sel_enum   countSel,
        ME_COPY_DATA_wr_confirm_enum  wrConfirm,
        void*                         pBuffer);
    static size_t BuildCopyDataCompute(
        MEC_COPY_DATA_dst_sel_enum     dstSel,
        gpusize                        dstAddr,
        MEC_COPY_DATA_src_sel_enum     srcSel,
        gpusize                        srcAddr,
        MEC_COPY_DATA_count_sel_enum   countSel,
        MEC_COPY_DATA_wr_confirm_enum  wrConfirm,
        void*                          pBuffer);
    // This generic version of BuildCopyData works on graphics and compute but doesn't provide any user-friendly enums.
    // The caller must make sure that the arguments they use are legal on their engine.
    static size_t BuildCopyData(
        EngineType engineType,
        uint32     engineSel,
        uint32     dstSel,
        gpusize    dstAddr,
        uint32     srcSel,
        gpusize    srcAddr,
        uint32     countSel,
        uint32     wrConfirm,
        void*      pBuffer);
    static size_t BuildPerfmonControl(
        uint32     perfMonCtlId,
        bool       enable,
        uint32     eventSelect,
        uint32     eventUnitMask,
        void*      pBuffer);
    template <bool dimInThreads, bool forceStartAt000>
    size_t BuildDispatchDirect(
        uint32          xDim,
        uint32          yDim,
        uint32          zDim,
        Pm4Predicate    predicate,
        bool            isWave32,
        bool            useTunneling,
        bool            disablePartialPreempt,
        void*           pBuffer) const;
    static size_t BuildDispatchIndirectGfx(
        gpusize      byteOffset,
        Pm4Predicate predicate,
        bool         isWave32,
        void*        pBuffer);
    size_t BuildDispatchIndirectMec(
        gpusize         address,
        bool            isWave32,
        bool            useTunneling,
        bool            disablePartialPreempt,
        void*           pBuffer) const;
    static size_t BuildDrawIndex2(
        uint32       indexCount,
        uint32       indexBufSize,
        gpusize      indexBufAddr,
        Pm4Predicate predicate,
        void*        pBuffer);
    static size_t BuildDrawIndexOffset2(
        uint32       indexCount,
        uint32       indexBufSize,
        uint32       indexOffset,
        Pm4Predicate predicate,
        void*        pBuffer);
    size_t BuildDrawIndexIndirect(
        gpusize      offset,
        uint32       baseVtxLoc,
        uint32       startInstLoc,
        Pm4Predicate predicate,
        void*        pBuffer) const;
    size_t BuildDrawIndexIndirectMulti(
        gpusize      offset,
        uint32       baseVtxLoc,
        uint32       startInstLoc,
        uint32       drawIndexLoc,
        uint32       stride,
        uint32       count,
        gpusize      countGpuAddr,
        Pm4Predicate predicate,
        void*        pBuffer) const;
    static size_t BuildDrawIndirectMulti(
        gpusize      offset,
        uint32       baseVtxLoc,
        uint32       startInstLoc,
        uint32       drawIndexLoc,
        uint32       stride,
        uint32       count,
        gpusize      countGpuAddr,
        Pm4Predicate predicate,
        void*        pBuffer);
    static size_t BuildDrawIndexAuto(
        uint32       indexCount,
        bool         useOpaque,
        Pm4Predicate predicate,
        void*        pBuffer);
    static size_t BuildTaskStateInit(
        Pm4ShaderType shaderType,
        gpusize       controlBufferAddr,
        Pm4Predicate  predicate,
        void*         pBuffer);
    template <bool IssueSqttMarkerEvent>
    static size_t BuildDispatchTaskMeshGfx(
        uint32       tgDimOffset,
        uint32       ringEntryLoc,
        Pm4Predicate predicate,
        void*        pBuffer);
    static size_t BuildDispatchMeshIndirectMulti(
        gpusize      dataOffset,
        uint32       xyzOffset,
        uint32       drawIndexOffset,
        uint32       count,
        uint32       stride,
        gpusize      countGpuAddr,
        Pm4Predicate predicate,
        void*        pBuffer);
    static size_t BuildDispatchTaskMeshIndirectMultiAce(
        gpusize      dataOffset,
        uint32       ringEntryLoc,
        uint32       xyzDimLoc,
        uint32       dispatchIndexLoc,
        uint32       count,
        uint32       stride,
        gpusize      countGpuAddr,
        bool         isWave32,
        Pm4Predicate predicate,
        void*        pBuffer);
    static size_t BuildDispatchTaskMeshDirectAce(
        uint32          xDim,
        uint32          yDim,
        uint32          zDim,
        uint32          ringEntryLoc,
        Pm4Predicate    predicate,
        bool            isWave32,
        void*           pBuffer);
    static size_t BuildDmaData(
        DmaDataInfo&  dmaDataInfo,
        void*         pBuffer);
    static size_t BuildDumpConstRam(
        gpusize dstGpuAddr,
        uint32  ramByteOffset,
        uint32  dwordSize,
        void*   pBuffer);
    static size_t BuildDumpConstRamOffset(
        uint32  dstAddrOffset,
        uint32  ramByteOffset,
        uint32  dwordSize,
        void*   pBuffer);
    static size_t BuildNonSampleEventWrite(
        VGT_EVENT_TYPE  vgtEvent,
        EngineType      engineType,
        void*           pBuffer);
    size_t BuildSampleEventWrite(
        VGT_EVENT_TYPE                           vgtEvent,
        ME_EVENT_WRITE_event_index_enum          eventIndex,
        EngineType                               engineType,
        gpusize                                  gpuAddr,
        void*                                    pBuffer) const;
    size_t BuildExecutionMarker(
        gpusize markerAddr,
        uint32  markerVal,
        uint64  clientHandle,
        uint32  markerType,
        void*   pBuffer) const;
    static size_t BuildIncrementCeCounter(void* pBuffer);
    static size_t BuildIncrementDeCounter(void* pBuffer);
    static size_t BuildIndexAttributesIndirect(gpusize baseAddr, uint16 index, void* pBuffer);
    static size_t BuildIndexBase(gpusize baseAddr, void* pBuffer);
    static size_t BuildIndexBufferSize(uint32 indexCount, void* pBuffer);
    size_t BuildIndexType(uint32 vgtDmaIndexType, void* pBuffer) const;
    static size_t BuildIndirectBuffer(
        EngineType engineType,
        gpusize    ibAddr,
        uint32     ibSize,
        bool       chain,
        bool       constantEngine,
        bool       enablePreemption,
        void*      pBuffer);

    static size_t BuildLoadConfigRegs(
        gpusize              gpuVirtAddr,
        const RegisterRange* pRanges,
        uint32               rangeCount,
        void*                pBuffer);

    static size_t BuildLoadContextRegs(
        gpusize              gpuVirtAddr,
        const RegisterRange* pRanges,
        uint32               rangeCount,
        void*                pBuffer);
    static size_t BuildLoadContextRegs(
        gpusize gpuVirtAddr,
        uint32  startRegAddr,
        uint32  count,
        void*   pBuffer);
    template <bool directAddress>
    size_t BuildLoadContextRegsIndex(
        gpusize gpuVirtAddrOrAddrOffset,
        uint32  startRegAddr,
        uint32  count,
        void*   pBuffer) const;
    size_t BuildLoadContextRegsIndex(
        gpusize gpuVirtAddr,
        uint32  count,
        void*   pBuffer) const;

    static size_t BuildLoadConstRam(
        gpusize srcGpuAddr,
        uint32  ramByteOffset,
        uint32  dwordSize,
        void*   pBuffer);

    static size_t BuildLoadShRegs(
        gpusize              gpuVirtAddr,
        const RegisterRange* pRanges,
        uint32               rangeCount,
        Pm4ShaderType        shaderType,
        void*                pBuffer);
    static size_t BuildLoadShRegs(
        gpusize       gpuVirtAddr,
        uint32        startRegAddr,
        uint32        count,
        Pm4ShaderType shaderType,
        void*         pBuffer);
    size_t BuildLoadShRegsIndex(
        PFP_LOAD_SH_REG_INDEX_index_enum index,
        gpusize                          gpuVirtAddr,
        uint32                           count,
        Pm4ShaderType                    shaderType,
        void*                            pBuffer) const;

    static size_t BuildLoadUserConfigRegs(
        gpusize              gpuVirtAddr,
        const RegisterRange* pRanges,
        uint32               rangeCount,
        void*                pBuffer);

    static size_t BuildNop(size_t numDwords, void* pBuffer);

    size_t BuildNumInstances(uint32 instanceCount, void* pBuffer) const;

    static size_t BuildOcclusionQuery(gpusize queryMemAddr, gpusize dstMemAddr, void* pBuffer);

    static size_t BuildPfpSyncMe(void* pBuffer);

    static size_t BuildPrimeUtcL2(
        gpusize gpuAddr,
        uint32  cachePerm,
        uint32  primeMode,
        uint32  engineSel,
        size_t  requestedPages,
        void*   pBuffer);
    size_t BuildReleaseMem(
        const ReleaseMemInfo& releaseMemInfo,
        void*                 pBuffer,
        uint32                gdsAddr = 0,
        uint32                gdsSize = 0) const;
    size_t ExplicitBuildReleaseMem(
        const ExplicitReleaseMemInfo& releaseMemInfo,
        void*                         pBuffer,
        uint32                        gdsAddr = 0,
        uint32                        gdsSize = 0) const;
    size_t BuildRewind(
        bool  offloadEnable,
        bool  valid,
        void* pBuffer) const;
    static size_t BuildSetBase(
        gpusize                      address,
        PFP_SET_BASE_base_index_enum baseIndex,
        Pm4ShaderType                shaderType,
        void*                        pBuffer);
    static size_t BuildSetBaseCe(
        gpusize                      address,
        CE_SET_BASE_base_index_enum  baseIndex,
        Pm4ShaderType                shaderType,
        void*                        pBuffer);
    template <bool resetFilterCam = false>
    size_t BuildSetOneConfigReg(
        uint32                               regAddr,
        void*                                pBuffer,
        PFP_SET_UCONFIG_REG_INDEX_index_enum index = index__pfp_set_uconfig_reg_index__default) const;
    size_t BuildSetOneContextReg(
        uint32                               regAddr,
        void*                                pBuffer,
        PFP_SET_CONTEXT_REG_INDEX_index_enum index = index__pfp_set_context_reg_index__default__GFX09) const;
    size_t BuildSetOneShReg(
        uint32        regAddr,
        Pm4ShaderType shaderType,
        void*         pBuffer) const;
    size_t BuildSetOneShRegIndex(
        uint32                          regAddr,
        Pm4ShaderType                   shaderType,
        PFP_SET_SH_REG_INDEX_index_enum index,
        void*                           pBuffer) const;
    template <bool resetFilterCam = false>
    size_t BuildSetSeqConfigRegs(
        uint32                         startRegAddr,
        uint32                         endRegAddr,
        void*                          pBuffer,
        PFP_SET_UCONFIG_REG_INDEX_index_enum index = index__pfp_set_uconfig_reg_index__default) const;
    size_t BuildSetSeqContextRegs(
        uint32                               startRegAddr,
        uint32                               endRegAddr,
        void*                                pBuffer,
        PFP_SET_CONTEXT_REG_INDEX_index_enum index = index__pfp_set_context_reg_index__default__GFX09) const;
    size_t BuildSetSeqShRegs(
        uint32        startRegAddr,
        uint32        endRegAddr,
        Pm4ShaderType shaderType,
        void*         pBuffer) const;
    size_t BuildSetSeqShRegsIndex(
        uint32                          startRegAddr,
        uint32                          endRegAddr,
        Pm4ShaderType                   shaderType,
        PFP_SET_SH_REG_INDEX_index_enum index,
        void*                           pBuffer) const;
    static size_t BuildSetPredication(
        gpusize       gpuVirtAddr,
        bool          predicationBool,
        bool          occlusionHint,
        PredicateType predType,
        bool          continuePredicate,
        void*         pBuffer);
    static size_t BuildStrmoutBufferUpdate(
        uint32  bufferId,
        uint32  sourceSelect,
        uint32  explicitOffset,
        gpusize dstGpuVirtAddr,
        gpusize srcGpuVirtAddr,
        void*   pBuffer);
    size_t BuildWaitCsIdle(EngineType engineType, gpusize timestampGpuAddr, void* pBuffer) const;
    static size_t BuildWaitDmaData(void* pBuffer);
    static size_t BuildWaitOnCeCounter(bool invalidateKcache, void* pBuffer);
    static size_t BuildWaitOnDeCounterDiff(uint32 counterDiff, void* pBuffer);
    size_t BuildWaitOnReleaseMemEventTs(
        EngineType     engineType,
        VGT_EVENT_TYPE vgtEvent,
        TcCacheOp      tcCacheOp,
        gpusize        gpuAddr,
        void*          pBuffer) const;
    static size_t BuildWaitRegMem(
        EngineType engineType,
        uint32     memSpace,
        uint32     function,
        uint32     engine,
        gpusize    addr,
        uint32     reference,
        uint32     mask,
        void*      pBuffer,
        uint32     operation = static_cast<uint32>(operation__me_wait_reg_mem__wait_reg_mem));
    static size_t BuildWaitRegMem64(
        EngineType engineType,
        uint32     memSpace,
        uint32     function,
        uint32     engine,
        gpusize    addr,
        uint64     reference,
        uint64     mask,
        void*      pBuffer);
    static size_t BuildWriteConstRam(
        const void* pSrcData,
        uint32      ramByteOffset,
        uint32      dwordSize,
        void*       pBuffer);
    static size_t BuildWriteData(
        const WriteDataInfo& info,
        uint32               data,
        void*                pBuffer);
    static size_t BuildWriteData(
        const WriteDataInfo& info,
        size_t               dwordsToWrite,
        const uint32*        pData,
        void*                pBuffer);
    static size_t BuildWriteDataPeriodic(
        const WriteDataInfo& info,
        size_t               dwordsPerPeriod,
        size_t               periodsToWrite,
        const uint32*        pPeriodData,
        void*                pBuffer);

    static size_t BuildCommentString(const char* pComment, Pm4ShaderType type, void* pBuffer);

    size_t BuildNopPayload(const void* pPayload, uint32 payloadSize, void* pBuffer) const;

    void BuildPipelinePrefetchPm4(const PipelineUploader& uploader, PipelinePrefetchPm4* pOutput) const;

    size_t BuildPrimeGpuCaches(
        const PrimeGpuCacheRange& primeGpuCacheRange,
        void*                     pBuffer) const;

    // Returns the register information for registers which have differing addresses between hardware families.
    const RegisterInfo& GetRegInfo() const { return m_registerInfo; }

private:
    static size_t BuildWriteDataInternal(
        const WriteDataInfo& info,
        size_t               dwordsToWrite,
        void*                pBuffer);

    uint32 Gfx10CalcAcquireMemGcrCntl(const AcquireMemInfo&  acquireMemInfo) const;
    uint32 Gfx10CalcReleaseMemGcrCntl(const ReleaseMemInfo&  releaseMemInfo) const;

#if PAL_ENABLE_PRINTS_ASSERTS
    void CheckShadowedContextReg(uint32 regAddr) const;
    void CheckShadowedContextRegs(uint32 startRegAddr, uint32 endRegAddr) const;
    void CheckShadowedShReg(Pm4ShaderType shaderType, uint32 regAddr, bool shouldBeShadowed = true) const;
    void CheckShadowedShRegs(Pm4ShaderType shaderType, uint32 startRegAddr,
                             uint32 endRegAddr, bool shouldBeShadowed = true) const;
    void CheckShadowedUserConfigRegs(uint32 startRegAddr, uint32 endRegAddr) const;
#endif

    const Device&    m_device;
    const GfxIpLevel m_gfxIpLevel;
    const uint32     m_cpUcodeVersion;
    RegisterInfo     m_registerInfo;   // Addresses for registers whose addresses vary between hardware families.

#if PAL_ENABLE_PRINTS_ASSERTS
    // If this is set, PAL will verify that all register writes fall within the ranges which get shadowed to GPU
    // memory when mid command buffer preemption is enabled.
    const bool  m_verifyShadowedRegisters;
#endif

    PAL_DISALLOW_DEFAULT_CTOR(CmdUtil);
    PAL_DISALLOW_COPY_AND_ASSIGN(CmdUtil);
};

} // Gfx9
} // Pal
