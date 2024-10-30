/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/gfx9ExecuteIndirectCmdUtil.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "palDevice.h"
#include "palLiterals.h"

namespace Pal
{

class      CmdStream;
enum class IndexType : uint32;

namespace Gfx9
{

class Device;

// Describes the core acquire_mem functionality common to ACE and GFX engines.
struct AcquireMemCore
{
    SyncGlxFlags cacheSync;

    // Acquires can apply to the full GPU address space or to a particular range. If these values are both zero
    // this is a full address space acquire. If both are non-zero then it's a ranged acquire. Depending on your
    // perspective, a full range acquire may be thought of as a rangeless acquire. Defining it this way means that
    // doing a "= {}" on the AcquireMem structs will give a full range acquire by default.
    gpusize      rangeBase; // The start of this acquire's address range or zero for a full range acquire.
    gpusize      rangeSize; // The length of the address range in bytes or zero for a full range acquire.
};

// In practice, we also need to know your runtime engine type to implement a generic acquire_mem. This isn't an
// abstract requirement of acquire_mem so it's not in AcquireMemCore.
struct AcquireMemGeneric : AcquireMemCore
{
    EngineType engineType;
};

// A collection of flags for AcquireMemGfxSurfSync and the internal implementation functions. The "target stall" flags
// tell the CP to compare active render target contexts against the AcquireMemCore GPU memory range. The RB cache
// flush and invalidate flags trigger after the target stalls. The Glx cache syncs trigger after the RB cache syncs.
union SurfSyncFlags
{
    uint8 u8All;

    struct
    {
        uint8 pfpWait          : 1; // Execute the acquire_mem at the PFP instead of the ME.
        uint8 cbTargetStall    : 1; // Do a range-checked stall on the active color targets.
        uint8 dbTargetStall    : 1; // Do a range-checked stall on the active depth and stencil targets.
        uint8 gfx10CbDataWbInv : 1; // Flush and invalidate the CB data cache. Only supported on gfx10.
        uint8 gfx10DbWbInv     : 1; // Flush and invalidate all DB caches. Only supported on gfx10.
        uint8 reserved         : 3;
    };
};

// If we know we're building an acquire_mem for a graphics engine we can expose extra features.
// This version programs the CP's legacy "surf sync" functionality to wait on gfx contexts and flush/inv caches.
struct AcquireMemGfxSurfSync : AcquireMemCore
{
    SurfSyncFlags flags;
};

// This version programs the CP's new PWS functionality, which can do a wait further down the gfx pipeline.
// It's only supported on gfx11+.
struct AcquireMemGfxPws : AcquireMemCore
{
    ME_ACQUIRE_MEM_pws_stage_sel_enum   stageSel;   // Where the acquire's wait occurs.
    ME_ACQUIRE_MEM_pws_counter_sel_enum counterSel; // Which delta counter to wait on.

    // The number of selected events minus 1 to synchronize on. (A value of 0 indicates 1 event ago.)
    // This field can be any value from 0 to 63. This works just like the SQ's s_waitcnt instructions.
    uint32 syncCount;
};

// Modeled after the GCR bits. Caches can only be synced by EOP release_mems.
union ReleaseMemCaches
{
    uint8 u8All;

    struct
    {
        uint8 gl2Inv      : 1; // Invalidate the GL2 cache.
        uint8 gl2Wb       : 1; // Flush the GL2 cache.
        uint8 glmInv      : 1; // Invalidate the GL2 metadata cache.
        uint8 gl1Inv      : 1; // Invalidate the GL1 cache.
        uint8 glvInv      : 1; // Invalidate the L0 vector cache.
        uint8 gfx11GlkInv : 1; // Invalidate the L0 scalar cache. Gfx11+ only.
        uint8 gfx11GlkWb  : 1; // Flush the L0 scalar cache. Gfx11+ only.
        uint8 reserved    : 1;
    };
};

// Describes the core release_mem functionality common to ACE and GFX engines.
struct ReleaseMemGeneric
{
    ReleaseMemCaches cacheSync;      // Caches can only be synced by EOP release_mems.
    uint32           dataSel;        // One of the {ME,MEC}_RELEASE_MEM_data_sel_enum values.
    gpusize          dstAddr;        // The the selected data here, must be aligned to the data byte size.
    uint64           data;           // data to write, ignored except for *_send_32_bit_low or *_send_64_bit_data.
    bool             gfx11WaitCpDma; // If wait CP DMA to be idle, only available on gfx11 with supported PFP version.
                                     // Clients must query EnableReleaseMemWaitCpDma() to make sure ReleaseMem packet
                                     // supports waiting CP DMA before setting it true.
};

// If we know we're building a release_mem for a graphics engine we can expose extra features.
struct ReleaseMemGfx : ReleaseMemGeneric
{
    VGT_EVENT_TYPE vgtEvent;  // Use this event. It must be an EOP TS event or an EOS event.
    bool           usePws;    // This event should increment the PWS counters.
    bool           waitCpDma; // If wait CP DMA to be idle.
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
//   2. The caller is operating under "CoherCopy/PipelineStageBlt" semantics and a barrier call will be issued. This
//      case is commonly referred to as a "CP Blt".
//
// In case #2, the caller must update the Pm4CmdBufferState by calling the relevant SetGfxCmdBuf* functions.
// Furthermore, the caller must not set "disWc" because write-confirms are necessary for the barrier to guarantee
// that the CP DMA writes have made it to their destination (memory, L2, etc.).
struct DmaDataInfo
{
    PFP_DMA_DATA_dst_sel_enum     dstSel;
    uint32                        dstOffset;
    gpusize                       dstAddr;        // Destination address for dstSel Addr or offset for GDS
    PFP_DMA_DATA_das_enum         dstAddrSpace;   // Destination address space
    PFP_DMA_DATA_src_sel_enum     srcSel;
    uint32                        srcOffset;
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
    uint16  mmDbDfsmControl;
};

// Parameters for building an EXECUTE_INDIRECT PM4 packet.
struct ExecuteIndirectPacketInfo
{
    gpusize      commandBufferAddr;
    gpusize      argumentBufferAddr;
    gpusize      countBufferAddr;
    gpusize      spillTableAddr;
    gpusize      incConstBufferAddr;
    uint32       spillTableInstanceCnt;
    uint32       maxCount;
    uint32       commandBufferSizeBytes;
    uint32       argumentBufferStrideBytes;
    uint32       spillTableStrideBytes;
    union
    {
        const GraphicsPipelineSignature*  pSignatureGfx;
        const ComputePipelineSignature*   pSignatureCs;
    } pipelineSignature;
    uint32       vbTableRegOffset;
    uint32       vbTableSizeDwords;
    uint32       xyzDimLoc;
};

struct BuildUntypedSrdInfo
{
    gpusize srcGpuVirtAddress;
    uint32  srcGpuVirtAddressOffset;
    gpusize dstGpuVirtAddress;
    uint32  dstGpuVirtAddressOffset;
    uint32  srdDword3;
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
    static constexpr uint32 CondIndirectBufferSize          = PM4_PFP_COND_INDIRECT_BUFFER_SIZEDW__CORE;
    static constexpr uint32 DispatchDirectSize              = PM4_PFP_DISPATCH_DIRECT_SIZEDW__CORE;
    static constexpr uint32 DispatchIndirectGfxSize         = PM4_ME_DISPATCH_INDIRECT_SIZEDW__CORE;
    static constexpr uint32 DispatchIndirectMecSize         = PM4_MEC_DISPATCH_INDIRECT_SIZEDW__CORE;
    static constexpr uint32 DrawIndirectSize                = PM4_PFP_DRAW_INDIRECT_SIZEDW__CORE;
    static constexpr uint32 SetIndexAttributesSize          = PM4_PFP_INDEX_ATTRIBUTES_INDIRECT_SIZEDW__CORE;
    static constexpr uint32 LoadShRegIndexSize              = PM4_PFP_LOAD_SH_REG_INDEX_SIZEDW__CORE;
    static constexpr uint32 BuildUntypedSrdSize             = PM4_PFP_BUILD_UNTYPED_SRD_SIZEDW__CORE;
    static constexpr uint32 DrawIndexAutoSize               = PM4_PFP_DRAW_INDEX_AUTO_SIZEDW__CORE;
    static constexpr uint32 DrawIndex2Size                  = PM4_PFP_DRAW_INDEX_2_SIZEDW__CORE;
    static constexpr uint32 DrawIndexOffset2Size            = PM4_PFP_DRAW_INDEX_OFFSET_2_SIZEDW__CORE;
    static constexpr uint32 DispatchMeshDirectSize          = PM4_ME_DISPATCH_MESH_DIRECT_SIZEDW__GFX11;
    static constexpr uint32 DispatchMeshIndirectMulti       = PM4_ME_DISPATCH_MESH_INDIRECT_MULTI_SIZEDW__CORE;
    static constexpr uint32 DispatchTaskMeshGfxSize         = PM4_ME_DISPATCH_TASKMESH_GFX_SIZEDW__CORE;
    static constexpr uint32 DispatchTaskMeshDirectMecSize   = PM4_MEC_DISPATCH_TASKMESH_DIRECT_ACE_SIZEDW__CORE;
    static constexpr uint32 DispatchTaskMeshIndirectMecSize = PM4_MEC_DISPATCH_TASKMESH_INDIRECT_MULTI_ACE_SIZEDW__CORE;
    static constexpr uint32 MinNopSizeInDwords              = 1; // all gfx9 HW supports 1-DW NOP packets

    static_assert (PM4_PFP_COND_EXEC_SIZEDW__CORE == PM4_MEC_COND_EXEC_SIZEDW__CORE,
                   "Conditional execution packet size does not match between PFP and compute engines!");
    static_assert (PM4_PFP_COND_EXEC_SIZEDW__CORE == PM4_CE_COND_EXEC_SIZEDW__GFX10,
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
    static constexpr uint32 AtomicMemSizeDwords       = PM4_ME_ATOMIC_MEM_SIZEDW__CORE;
    static constexpr uint32 PfpSyncMeSizeDwords       = PM4_PFP_PFP_SYNC_ME_SIZEDW__CORE;

    // This can't be a precomputed constant, we have to look at some device state.
    uint32 DrawIndexIndirectSize() const;

    // The INDIRECT_BUFFER and COND_INDIRECT_BUFFER packet have a hard-coded IB size of 20 bits.
    static constexpr uint32 MaxIndirectBufferSizeDwords = (1 << 20) - 1;

    // DMA_DATA's byte_count is only 26 bits so the max count is (1 << 26) - 1. However, I really just don't
    // like splitting the copies on an alignment of one byte. It just feels... wrong, and might hurt performance too!
    static constexpr uint32 MaxDmaDataByteCount = 1u << 25;

    // The COPY_DATA src_reg_offset and dst_reg_offset fields have a bit-width of 18 bits.
    static constexpr uint32 MaxCopyDataRegOffset = (1 << 18) - 1;

    ///@note GFX10 added the ability to do ranged checks when for Flush/INV ops to GL1/GL2 (so that you can just
    ///      Flush/INV necessary lines instead of the entire cache). Since these caches are physically tagged, this
    ///      can invoke a high penalty for large surfaces so limit the surface size allowed.
    static constexpr uint64 Gfx10AcquireMemGl1Gl2RangedCheckMaxSurfaceSizeBytes = (64 * Util::OneKibibyte);

    static bool IsContextReg(uint32 regAddr);
    static bool IsUserConfigReg(uint32 regAddr);
    static bool IsShReg(uint32 regAddr);

    static ME_WAIT_REG_MEM_function_enum WaitRegMemFunc(CompareFunc compareFunc);

    // Checks if the register offset provided can be read or written using a COPY_DATA packet.
    static bool CanUseCopyDataRegOffset(uint32 regOffset) { return (((~MaxCopyDataRegOffset) & regOffset) == 0); }

    bool CanUseCsPartialFlush(EngineType engineType) const;
    bool CanUseAcquireMem(SyncRbFlags rbSync) const;

    // Helper functions for building ReleaseMem info structs.
    VGT_EVENT_TYPE SelectEopEvent(SyncRbFlags rbSync) const;
    ReleaseMemCaches SelectReleaseMemCaches(SyncGlxFlags* pGlxSync) const;

    // If we have support for the indirect_addr index and compute engines.
    bool HasEnhancedLoadShRegIndex() const;

    static uint16 ShRegOffset(uint16 regAddr) { return (regAddr == 0) ? 0 : (regAddr - PERSISTENT_SPACE_START); }

    size_t BuildAcquireMemGeneric(const AcquireMemGeneric& info, void* pBuffer) const;
    size_t BuildAcquireMemGfxSurfSync(const AcquireMemGfxSurfSync& info, void* pBuffer) const;
    size_t BuildAcquireMemGfxPws(const AcquireMemGfxPws& info, void* pBuffer) const;

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
    // This generic version of BuildCopyData works on graphics and compute but doesn't provide any user-friendly enums.
    // The caller must make sure that the arguments they use are legal on their engine.
    size_t BuildCopyData(
        EngineType engineType,
        uint32     engineSel,
        uint32     dstSel,
        gpusize    dstAddr,
        uint32     srcSel,
        gpusize    srcAddr,
        uint32     countSel,
        uint32     wrConfirm,
        void*      pBuffer) const;
    static size_t BuildPerfmonControl(
        uint32     perfMonCtlId,
        bool       enable,
        uint32     eventSelect,
        uint32     eventUnitMask,
        void*      pBuffer);
    template <bool dimInThreads, bool forceStartAt000>
    size_t BuildDispatchDirect(
        DispatchDims size,
        Pm4Predicate predicate,
        bool         isWave32,
        bool         useTunneling,
        bool         disablePartialPreempt,
        void*        pBuffer) const;
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
    static size_t BuildExecuteIndirect(
        Pm4Predicate                     predicate,
        const bool                       isGfx,
        const ExecuteIndirectPacketInfo& packetInfo,
        const bool                       resetPktFilter,
        void*                            pBuffer);
    static size_t BuildExecuteIndirectV2(
        Pm4Predicate                     predicate,
        const bool                       isGfx,
        const ExecuteIndirectPacketInfo& packetInfo,
        const bool                       resetPktFilter,
        ExecuteIndirectV2Op*             pPacketOp,
        ExecuteIndirectV2Meta*           pMeta,
        void*                            pBuffer);
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
    static size_t BuildDrawIndirect(
        gpusize      offset,
        uint32       baseVtxLoc,
        uint32       startInstLoc,
        Pm4Predicate predicate,
        void*        pBuffer);
    size_t BuildDrawIndexIndirect(
        gpusize      offset,
        uint32       baseVtxLoc,
        uint32       startInstLoc,
        Pm4Predicate predicate,
        void*        pBuffer) const;
    template <bool IssueSqttMarkerEvent>
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
    template <bool IssueSqttMarkerEvent>
    size_t BuildDrawIndirectMulti(
        gpusize      offset,
        uint32       baseVtxLoc,
        uint32       startInstLoc,
        uint32       drawIndexLoc,
        uint32       stride,
        uint32       count,
        gpusize      countGpuAddr,
        Pm4Predicate predicate,
        void*        pBuffer) const;
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
    size_t BuildDispatchTaskMeshGfx(
        uint32       tgDimOffset,
        uint32       ringEntryLoc,
        Pm4Predicate predicate,
        bool         usesLegacyMsFastLaunch,
        bool         linearDispatch,
        void*        pBuffer) const;
    static size_t BuildDispatchMeshDirect(
        DispatchDims size,
        Pm4Predicate predicate,
        void*        pBuffer);
    template <bool IssueSqttMarkerEvent>
    size_t BuildDispatchMeshIndirectMulti(
        gpusize      dataOffset,
        uint32       xyzOffset,
        uint32       drawIndexOffset,
        uint32       count,
        uint32       stride,
        gpusize      countGpuAddr,
        Pm4Predicate predicate,
        bool         usesLegacyMsFastLaunch,
        void*        pBuffer) const;
    template <bool IssueSqttMarkerEvent>
    size_t BuildDispatchTaskMeshIndirectMultiAce(
        gpusize      dataOffset,
        uint32       ringEntryLoc,
        uint32       xyzDimLoc,
        uint32       dispatchIndexLoc,
        uint32       count,
        uint32       stride,
        gpusize      countGpuAddr,
        bool         isWave32,
        Pm4Predicate predicate,
        void*        pBuffer) const;
    static size_t BuildDispatchTaskMeshDirectAce(
        DispatchDims size,
        uint32       ringEntryLoc,
        Pm4Predicate predicate,
        bool         isWave32,
        void*        pBuffer);

    template<bool srcIndirectAddress, bool dstIndirectAddress>
    static size_t BuildDmaData(
        DmaDataInfo&  dmaDataInfo,
        void*         pBuffer);
    static size_t BuildUntypedSrd(
        Pm4Predicate               predicate,
        const BuildUntypedSrdInfo* pSrdInfo,
        Pm4ShaderType              shaderType,
        void* pBuffer);
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
    size_t BuildNonSampleEventWrite(
        VGT_EVENT_TYPE  vgtEvent,
        EngineType      engineType,
        void*           pBuffer) const;
    size_t BuildSampleEventWrite(
        VGT_EVENT_TYPE                           vgtEvent,
        ME_EVENT_WRITE_event_index_enum          eventIndex,
        EngineType                               engineType,
        MEC_EVENT_WRITE_samp_plst_cntr_mode_enum counterMode,
        gpusize                                  gpuAddr,
        void*                                    pBuffer) const;
    size_t BuildNonSampleEventWrite(
        VGT_EVENT_TYPE  vgtEvent,
        EngineType      engineType,
        Pm4Predicate    predicate,
        void*           pBuffer) const;
    static size_t BuildIncrementCeCounter(void* pBuffer);
    static size_t BuildIncrementDeCounter(void* pBuffer);
    static size_t BuildIndexAttributesIndirect(gpusize baseAddr, uint16 index, bool hasIndirectAddress, void* pBuffer);
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
        PFP_LOAD_SH_REG_INDEX_index_enum       index,
        PFP_LOAD_SH_REG_INDEX_data_format_enum dataFormat,
        gpusize                                gpuVirtAddr,
        uint32                                 startRegAddr,
        uint32                                 count,
        Pm4ShaderType                          shaderType,
        void*                                  pBuffer) const;

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

    size_t BuildNativeFenceRaiseInterrupt(
        void* pBuffer,
        gpusize monitoredValueGpuVa,
        uint64  signaledVal,
        uint32  intCtxId) const;

    size_t BuildReleaseMemGeneric(const ReleaseMemGeneric& info, void* pBuffer) const;
    size_t BuildReleaseMemGfx(const ReleaseMemGfx& info, void* pBuffer) const;
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
        uint32 regAddr,
        void*  pBuffer) const;
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
        uint32 startRegAddr,
        uint32 endRegAddr,
        void*  pBuffer) const;
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
    template <Pm4ShaderType ShaderType, size_t N>
    size_t BuildSetMaskedPackedRegPairs(
        const PackedRegisterPair* pRegPairs,
        uint32                    (&validMask)[N],
        bool                      isShReg,
        void*                     pBuffer) const;
    template <Pm4ShaderType ShaderType>
    size_t BuildSetPackedRegPairs(
        PackedRegisterPair* pRegPairs,
        uint32              numRegs,
        bool                isShReg,
        void*               pBuffer) const;
    template <Pm4ShaderType ShaderType>
    size_t BuildSetShRegPairsPacked(
        PackedRegisterPair* pRegPairs,
        uint32              numRegs,
        void*               pBuffer) const;
    size_t BuildSetContextRegPairsPacked(
        PackedRegisterPair* pRegPairs,
        uint32              numRegs,
        void*               pBuffer) const;
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
        gpusize controlBufAddr,
        void*   pBuffer);
    size_t BuildWaitCsIdle(EngineType engineType, gpusize timestampGpuAddr, void* pBuffer) const;
    static size_t BuildWaitDmaData(void* pBuffer);
    static size_t BuildWaitOnCeCounter(bool invalidateKcache, void* pBuffer);
    static size_t BuildWaitOnDeCounterDiff(uint32 counterDiff, void* pBuffer);
    size_t BuildWaitEopPws(
        AcquirePoint waitPoint,
        bool         waitCpDma,
        SyncGlxFlags glxSync,
        SyncRbFlags  rbSync,
        void*        pBuffer) const;
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
    static size_t BuildWriteDataInternal(
        const WriteDataInfo& info,
        size_t               dwordsToWrite,
        void*                pBuffer);

    static size_t BuildCommentString(const char* pComment, Pm4ShaderType type, void* pBuffer);

    size_t BuildNopPayload(const void* pPayload, uint32 payloadSize, void* pBuffer) const;

    size_t BuildPrimeGpuCaches(
        const PrimeGpuCacheRange& primeGpuCacheRange,
        EngineType                engineType,
        void*                     pBuffer) const;

    size_t BuildPerfCounterWindow(
        EngineType engineType,
        bool       enableWindow,
        void*      pBuffer) const;

    static bool IsIndexedRegister(uint32 regAddr);

    // Returns the register information for registers which have differing addresses between hardware families.
    const RegisterInfo& GetRegInfo() const { return m_registerInfo; }

    size_t BuildHdpFlush(void* pBuffer) const;

private:
    size_t BuildAcquireMemInternal(
        const AcquireMemCore& info,
        EngineType            engineType,
        SurfSyncFlags         surfSyncFlags,
        void*                 pBuffer) const;
    size_t BuildReleaseMemInternal(
        const ReleaseMemGeneric& info,
        VGT_EVENT_TYPE           vgtEvent,
        bool                     usePws,
        void*                    pBuffer) const;

    template <Pm4ShaderType ShaderType>
    uint32* FillPackedRegPairsHeaderAndCount(
        uint32  numRegs,
        bool    isShReg,
        size_t* pPacketSize,
        uint32* pPacket) const;

#if PAL_ENABLE_PRINTS_ASSERTS
    void CheckShadowedContextReg(uint32 regAddr) const;
    void CheckShadowedContextRegs(uint32 startRegAddr, uint32 endRegAddr) const;
    void CheckShadowedShReg(Pm4ShaderType shaderType, uint32 regAddr, bool shouldBeShadowed = true) const;
    void CheckShadowedShRegs(Pm4ShaderType shaderType, uint32 startRegAddr,
                             uint32 endRegAddr, bool shouldBeShadowed = true) const;
    void CheckShadowedUserConfigRegs(uint32 startRegAddr, uint32 endRegAddr) const;
#endif

    const Device&            m_device;
    const GpuChipProperties& m_chipProps;
    RegisterInfo             m_registerInfo;    // Addresses for registers whose addresses vary between hardware families.

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
