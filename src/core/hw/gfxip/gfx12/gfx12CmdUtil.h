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

#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/gfx12/gfx12Chip.h"
#include "core/hw/gfxip/gfx12/gfx12ExecuteIndirectCmdUtil.h"
#include "core/hw/gfxip/gfx12/gfx12UserDataLayout.h"

namespace Pal
{

namespace Gfx12
{
class Device;
class UserDataLayout;

// Describes the core acquire_mem functionality common to ACE and GFX engines.
struct AcquireMemCore
{
    SyncGlxFlags cacheSync; // Multiple acquire_mems may be issued on gfx9 to handle some cache combinations.
};

// In practice, we also need to know your runtime engine type to implement a generic acquire_mem. This isn't an
// abstract requirement of acquire_mem so it's not in AcquireMemCore.
struct AcquireMemGeneric : AcquireMemCore
{
    EngineType engineType;
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

// Modeled after the GCR bits. Multiple release_mems may be issued on gfx12 to handle some cache combinations.
// Caches can only be synced by EOP release_mems.
union ReleaseMemCaches
{
    uint8 u8All;

    struct
    {
        uint8 gl2Inv      : 1; // Invalidate the GL2 cache.
        uint8 gl2Wb       : 1; // Flush the GL2 cache.
        uint8 glvInv      : 1; // Invalidate the L0 vector cache.
        uint8 glkInv      : 1; // Invalidate the L0 scalar cache.
        uint8 glkWb       : 1; // Flush the L0 scalar cache.
        uint8 reserved    : 3;
    };
};

// Describes the core release_mem functionality.
struct ReleaseMemGeneric
{
    VGT_EVENT_TYPE   vgtEvent;    // Use this event. It must be an EOP TS event or an EOS event.
    uint32           dataSel;     // One of the {ME,MEC}_RELEASE_MEM_data_sel_enum values.
    uint64           data;        // data to write, ignored except for *_send_32_bit_low or *_send_64_bit_data.
    gpusize          dstAddr;     // The the selected data here, must be aligned to the data byte size.
    ReleaseMemCaches cacheSync;   // Caches can only be synced by EOP release_mems.
    bool             usePws;      // This event should increment the PWS counters.
    bool             waitCpDma;   // If wait CP DMA to be idle, only available with supported PFP version; clients must
                                  // query EnableReleaseMemWaitCpDma() to make sure ReleaseMem packet supports waiting
                                  // CP DMA before setting it true.
    bool             noConfirmWr; // Disable confirmation of data write after EOP
};

// Data required to perform a copy using the CP's COPY_DATA.
struct CopyDataInfo
{
    EngineType engineType;
    uint32     engineSel;
    uint32     dstSel;
    gpusize    dstAddr;
    uint32     srcSel;
    gpusize    srcAddr;
    uint32     countSel;
    uint32     wrConfirm;
};

// Data used to query gpu/soc clock counter.
struct TimestampInfo
{
    bool          enableBottom;
    uint32        clkSel;
    gpusize       dstAddr;
    Pm4ShaderType shaderType;
};

// Data required to perform a DMA Data transfer (aka CPDMA).
//
// Note that the "sync" flag should be set in almost all cases. The two exceptions are:
//   1. The caller will manually synchronize the CP DMA engine using another DMA.
//   2. The caller is operating under "CoherCopy/PipelineStageBlt" semantics and a barrier call will be issued. This
//      case is commonly referred to as a "CP Blt".
//
// In case #2, the caller must update the GfxCmdBufferState by calling the relevant SetGfxCmdBuf* functions.
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

// Parameters for building an EXECUTE_INDIRECT PM4 packet.
struct ExecuteIndirectPacketInfo
{
    gpusize               argumentBufferAddr;        // GPU virtual address of the indirect arguments buffer, which
                                                     // layout shall be determined by the param from app.
    gpusize               countBufferAddr;           // GPU virtual address of buffer that indicates the actual number
                                                     // of times the generated indirect commands are to be executed.
                                                     // If this is a nullptr or the count is 0, then actual execution
                                                     // count is equal to maxCount.
    gpusize               spillTableAddr;            // GPU virtual address of the PAL allocated single copy of
                                                     // VB+UserData spill table buffer. Not Global SpillBuffer.
    uint32                maxCount;                  // The maximum number of indirect commands to generate and execute
    uint32                argumentBufferStrideBytes; // ArgBuffer stride provided by client.
    uint32                spillTableStrideBytes;     // Calculated stride of the SpillTable.
    const UserDataLayout* pUserDataLayout;           // UserDataLayout specified by pipeline.
    uint32                vbTableRegOffset;          // Offset to VBTable if it exists.
    uint32                vbTableSizeDwords;         // Size of VBTable.
    uint32                xyzDimLoc;                 // Dispatch dims reg address offset.
};

class CmdUtil
{
public:
    explicit CmdUtil(const Device& device);
    ~CmdUtil() {}

    // Compile-time packet size helpers.
    static constexpr uint32 AtomicMemSizeDwords             = PM4_ME_ATOMIC_MEM_SIZEDW__CORE;
    static constexpr uint32 CondExecMecSize                 = PM4_MEC_COND_EXEC_SIZEDW__CORE;
    static constexpr uint32 CondIndirectBufferSize          = PM4_PFP_COND_INDIRECT_BUFFER_SIZEDW__CORE;
    static constexpr uint32 CopyDataSizeDwords              = PM4_ME_COPY_DATA_SIZEDW__CORE;
    static constexpr uint32 DispatchDirectSize              = PM4_PFP_DISPATCH_DIRECT_SIZEDW__CORE;
    static constexpr uint32 DispatchIndirectMecSize         = PM4_MEC_DISPATCH_INDIRECT_SIZEDW__CORE;
    static constexpr uint32 DispatchTaskMeshDirectMecSize   = PM4_MEC_DISPATCH_TASKMESH_DIRECT_ACE_SIZEDW__CORE;
    static constexpr uint32 DispatchTaskMeshIndirectMecSize = PM4_MEC_DISPATCH_TASKMESH_INDIRECT_MULTI_ACE_SIZEDW__CORE;
    static constexpr uint32 DmaDataSizeDwords               = PM4_PFP_DMA_DATA_SIZEDW__CORE;
    static constexpr uint32 IndexTypeSizeDwords             = PM4_PFP_SET_UCONFIG_REG_INDEX_SIZEDW__CORE + 1;
    static constexpr uint32 LoadShRegsIndexSizeDwords       = PM4_PFP_LOAD_SH_REG_INDEX_SIZEDW__CORE;
    static constexpr uint32 PfpSyncMeSizeDwords             = PM4_PFP_PFP_SYNC_ME_SIZEDW__CORE;
    static constexpr uint32 ReleaseMemSizeDwords            = PM4_ME_RELEASE_MEM_SIZEDW__CORE;
    static constexpr uint32 SetContextRegHeaderSizeDwords   = PM4_PFP_SET_CONTEXT_REG_SIZEDW__CORE;
    static constexpr uint32 SetShRegHeaderSizeDwords        = PM4_PFP_SET_SH_REG_SIZEDW__CORE;
    static constexpr uint32 SetUConfigRegHeaderSizeDwords   = PM4_PFP_SET_UCONFIG_REG_SIZEDW__CORE;
    static constexpr uint32 SetOneUConfigRegSizeDwords      = SetUConfigRegHeaderSizeDwords + 1;
    static constexpr uint32 WaitRegMemSizeDwords            = PM4_ME_WAIT_REG_MEM_SIZEDW__CORE;

    // All gfx12 HW supports 1-DW NOP packets.
    static constexpr uint32 MinNopSizeInDwords = 1;

    // The INDIRECT_BUFFER and COND_INDIRECT_BUFFER packet have a hard-coded IB size of 20 bits.
    static constexpr uint32 MaxIndirectBufferSizeDwords = (1 << 20) - 1;

    // DMA_DATA's byte_count is only 26 bits so the max count is (1 << 26) - 1. However, I really just don't
    // like splitting the copies on an alignment of one byte. It just feels... wrong, and might hurt performance too!
    static constexpr uint32 MaxDmaDataByteCount = 1u << 25;

    // Compute the size of a NOP packet with an arbitrary binary payload.
    static constexpr uint32 NopPayloadSizeDwords(uint32 payloadSize)
        { return PM4_PFP_NOP_SIZEDW__CORE + payloadSize; }

    // Compute the size of a SET_CONTEXT_REG_PAIRS packet, in dwords. (The packet constant contains the first pair.)
    static constexpr uint32 SetContextPairsSizeDwords(uint32 numPairs)
        { return PM4_PFP_SET_CONTEXT_REG_PAIRS_SIZEDW__CORE + ((numPairs - 1) * 2); }

    // Compute the size of a SET_CONTEXT_REG packet with multiple registers, in dwords.
    static constexpr uint32 SetSeqContextRegsSizeDwords(uint32 startReg, uint32 endReg)
        { return SetContextRegHeaderSizeDwords + endReg - startReg + 1; }

    // Note that a SET_CONTEXT_REG_PAIRS is expected to be faster than a SET_CONTEXT_REG with only one register.
    // Make sure you build the right packet when using this constant.
    static constexpr uint32 SetOneContextRegSizeDwords = PM4_PFP_SET_CONTEXT_REG_PAIRS_SIZEDW__CORE;

    // Compute the size of a SET_SH_REG packet with multiple registers, in dwords.
    static constexpr uint32 SetSeqShRegsSizeDwords(uint32 startReg, uint32 endReg)
        { return SetShRegHeaderSizeDwords + endReg - startReg + 1; }

    // Compute the size of a SET_UCONFIG_REG packet with multiple registers, in dwords.
    static constexpr uint32 SetSeqUConfigRegsSizeDwords(uint32 startReg, uint32 endReg)
        { return SetUConfigRegHeaderSizeDwords + endReg - startReg + 1; }

    // Compute the size of a WRITE_DATA packet.
    static constexpr uint32 WriteDataSizeDwords(uint32 dwordsToWrite)
        { return PM4_ME_WRITE_DATA_SIZEDW__CORE + dwordsToWrite; }

    static size_t BuildContextControl(
        const PM4_PFP_CONTEXT_CONTROL& contextControl,
        void*                          pBuffer);

    static size_t BuildNop(uint32 numDwords, void* pBuffer);
    static size_t BuildNopPayload(const void* pPayload, uint32 payloadSize, void* pBuffer);
    static size_t BuildCommentString(const char* pComment, Pm4ShaderType type, void* pBuffer);

    template <Pm4ShaderType ShaderType = ShaderGraphics, bool ResetFilterCam = false>
    static size_t BuildSetShPairs(
        const RegisterValuePair* pPairs,
        uint32                   numPairs,
        void*                    pBuffer);
    template <Pm4ShaderType ShaderType = ShaderGraphics, bool ResetFilterCam = false>
    static size_t BuildSetShPairsHeader(
        uint32                   numPairsTotal,
        void**                   ppDataStart,
        void*                    pBuffer);
    static size_t BuildSetContextPairs(
        const RegisterValuePair* pPairs,
        uint32                   numPairs,
        void*                    pBuffer);
    static size_t BuildSetContextPairsHeader(
        uint32                   numPairsTotal,
        void**                   ppDataStart,
        void*                    pBuffer);

    static size_t BuildContextRegRmw(
        uint32  regAddr,
        uint32  regMask,
        uint32  regData,
        void*   pBuffer);

    static bool IsUserConfigReg(uint32 regAddr);

    template <bool ResetFilterCam = false>
    static size_t BuildSetOneUConfigReg(
        uint32 offset,
        uint32 value,
        void*  pBuffer);
    static size_t BuildSetUConfigPairs(
        const RegisterValuePair* pPairs,
        uint32                   numPairs,
        void*                    pBuffer);
    static size_t BuildSetUConfigPairsHeader(
        uint32                   numPairsTotal,
        void**                   ppDataStart,
        void*                    pBuffer);

    template <bool ResetFilterCam = false>
    static size_t BuildSetSeqUConfigRegs(
        uint32 startRegAddr,
        uint32 endRegAddr,
        void*  pBuffer);

    static constexpr uint32 ShRegIndexSizeDwords = PM4_PFP_SET_SH_REG_INDEX_SIZEDW__CORE;

    template <Pm4ShaderType ShaderType = ShaderGraphics>
    static size_t BuildSetSeqShRegsIndex(
        uint32                          startRegAddr,
        uint32                          endRegAddr,
        PFP_SET_SH_REG_INDEX_index_enum index,
        void*                           pBuffer);

    static size_t BuildLoadContextRegsIndex(
        gpusize gpuVirtAddr,
        uint32  startRegAddr,
        uint32  count,
        void*   pBuffer);

    static size_t BuildLoadShRegsIndex(
        PFP_LOAD_SH_REG_INDEX_index_enum       index,
        PFP_LOAD_SH_REG_INDEX_data_format_enum dataFormat,
        gpusize                                gpuVirtAddr,
        uint32                                 startRegAddr,
        uint32                                 count,
        Pm4ShaderType                          shaderType,
        void*                                  pBuffer);

    static size_t BuildSetPredication(
        gpusize       gpuVirtAddr,
        bool          predicationBool,
        bool          occlusionHint,
        PredicateType predType,
        bool          continuePredicate,
        void*         pBuffer);

    static size_t BuildNumInstances(uint32 instanceCount, void* pBuffer);
    static size_t BuildIndexBase(gpusize baseAddr, void* pBuffer);
    static size_t BuildIndexBufferSize(uint32 indexCount, void* pBuffer);
    static size_t BuildIndexType(uint32 vgtDmaIndexType, void* pBuffer);

    static size_t BuildDrawIndexAuto(
        uint32       indexCount,
        bool         useOpaque,
        Pm4Predicate predicate,
        void*        pBuffer);
    static size_t BuildDrawIndex2(
        uint32       indexCount,
        uint32       indexBufSize,
        gpusize      indexBufAddr,
        Pm4Predicate predicate,
        void*        pBuffer);
    static size_t BuildDrawIndirect(
        gpusize      offset,
        uint32       baseVtxLoc,
        uint32       startInstLoc,
        Pm4Predicate predicate,
        void*        pBuffer);
    static size_t BuildDrawIndexIndirect(
        gpusize      offset,
        uint32       baseVtxLoc,
        uint32       startInstLoc,
        Pm4Predicate predicate,
        void*        pBuffer);
    static size_t BuildDrawIndirectMulti(
        gpusize      offset,
        uint16       baseVtxLoc,
        uint16       startInstLoc,
        uint16       drawIndexLoc,
        uint32       stride,
        uint32       count,
        gpusize      countGpuAddr,
        Pm4Predicate predicate,
        bool         issueSqttMarker,
        void*        pBuffer);
    static size_t BuildDrawIndexIndirectMulti(
        gpusize      offset,
        uint16       baseVtxLoc,
        uint16       startInstLoc,
        uint16       drawIndexLoc,
        uint32       stride,
        uint32       count,
        gpusize      countGpuAddr,
        Pm4Predicate predicate,
        bool         issueSqttMarker,
        void*        pBuffer);
    template <bool DimInThreads, bool ForceStartAt000>
    static size_t BuildDispatchDirect(
        DispatchDims size,
        Pm4Predicate predicate,
        bool         isWave32,
        bool         useTunneling,
        bool         disablePartialPreempt,
        bool         pingPongEn,
        bool         is2dDispatchInterleave,
        void*        pBuffer);
    static size_t BuildDispatchIndirectGfx(
        gpusize      offset,
        Pm4Predicate predicate,
        bool         isWave32,
        bool         pingPongEn,
        bool         is2dDispatchInterleave,
        void*        pBuffer);
    static size_t BuildDispatchMeshDirect(
        DispatchDims size,
        Pm4Predicate predicate,
        void*        pBuffer);
    static size_t BuildDispatchMeshIndirectMulti(
        gpusize      dataOffset,
        uint16       xyzOffset,
        uint16       drawIndexLoc,
        uint32       count,
        uint32       stride,
        gpusize      countGpuAddr,
        Pm4Predicate predicate,
        bool         issueSqttMarker,
        void*        pBuffer);
    static size_t BuildDispatchTaskMeshGfx(
        uint16       xyzDimLoc,
        uint16       ringEntryLoc,
        Pm4Predicate predicate,
        bool         issueSqttMarker,
        bool         linearDispatch,
        void*        pBuffer);
    static size_t BuildDispatchTaskMeshDirectMec(
        DispatchDims size,
        uint16       ringEntryLoc,
        Pm4Predicate predicate,
        bool         isWave32,
        void*        pBuffer);
    static size_t BuildDispatchTaskMeshIndirectMultiMec(
        gpusize      dataOffset,
        uint16       ringEntryLoc,
        uint16       xyzDimLoc,
        uint16       dispatchIndexLoc,
        uint32       count,
        uint32       stride,
        gpusize      countGpuAddr,
        bool         isWave32,
        Pm4Predicate predicate,
        bool         issueSqttMarker,
        void*        pBuffer);
    static size_t BuildDispatchIndirectMec(
        gpusize         address,
        bool            isWave32,
        bool            useTunneling,
        bool            disablePartialPreempt,
        void*           pBuffer);

    // Returns the number of DWORDs that are required to chain two chunks
    static uint32 ChainSizeInDwords(EngineType engineType);

    static size_t BuildIndirectBuffer(
        EngineType engineType,
        gpusize    ibAddr,
        uint32     ibSize,
        bool       chain,
        bool       enablePreemption,
        void*      pBuffer);

    // The "official" "event-write" packet definition (see:  PM4_MEC_EVENT_WRITE) contains "extra" dwords that aren't
    // necessary (and would cause problems if they existed) for event writes other than "".  Define a "plain"
    // event-write packet definition here.
    struct PM4_ME_NON_SAMPLE_EVENT_WRITE
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32               ordinal2;
    };

    static constexpr uint32 NonSampleEventWriteSizeDwords = (sizeof(PM4_ME_NON_SAMPLE_EVENT_WRITE) / sizeof(uint32));

    // Also, PIXEL_PIPE_STAT_DUMP with pixel_pipe_stat_control_or_dump requires a special EVENT_WRITE_ZPASS packet.
    // BuildSampleEventWrite can generate both of these sizes so be careful!
    static constexpr uint32 SampleEventWriteSizeDwords      = PM4_ME_EVENT_WRITE_SIZEDW__CORE;
    static constexpr uint32 SampleEventWriteZpassSizeDwords = PM4_ME_EVENT_WRITE_ZPASS_SIZEDW__CORE;

    static size_t BuildNonSampleEventWrite(
        VGT_EVENT_TYPE  vgtEvent,
        EngineType      engineType,
        void*           pBuffer);
    static size_t BuildNonSampleEventWrite(
        VGT_EVENT_TYPE  vgtEvent,
        EngineType      engineType,
        Pm4Predicate    predicate,
        void*           pBuffer);
    static size_t BuildSampleEventWrite(
        VGT_EVENT_TYPE                           vgtEvent,
        ME_EVENT_WRITE_event_index_enum          eventIndex,
        EngineType                               engineType,
        MEC_EVENT_WRITE_samp_plst_cntr_mode_enum counterMode,
        gpusize                                  gpuAddr,
        void*                                    pBuffer);
    static VGT_EVENT_TYPE SelectEopEvent(
        SyncRbFlags      rbSync);
    static ReleaseMemCaches SelectReleaseMemCaches(
        SyncGlxFlags*    pGlxSync);

    size_t BuildReleaseMemGeneric(
        const ReleaseMemGeneric& info,
        void*                    pBuffer) const;
    static size_t BuildAcquireMemGeneric(
        const AcquireMemGeneric& info,
        void*                    pBuffer);
    static size_t BuildAcquireMemGfxPws(
        const AcquireMemGfxPws& info,
        void*                   pBuffer);
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
        uint32     operation = uint32(operation__me_wait_reg_mem__wait_reg_mem));
    static size_t BuildPfpSyncMe(void* pBuffer);

    static size_t BuildWriteData(
        const WriteDataInfo& info,
        uint32               data,
        void*                pBuffer);
    static size_t BuildWriteData(
        const WriteDataInfo& info,
        size_t               dwordsToWrite,
        const uint32*        pData,
        void*                pBuffer);

    // This generic version of BuildCopyData works on graphics and compute but doesn't provide any user-friendly enums.
    // The caller must make sure that the arguments they use are legal on their engine.
    static size_t BuildCopyData(
        const CopyDataInfo& info,
        void*               pBuffer);

    static size_t BuildWriteTimestamp(
        const TimestampInfo& info,
        void*                pBuffer);

    template<bool indirectAddress>
    static size_t BuildDmaData(
        const DmaDataInfo& dmaDataInfo,
        void*              pBuffer);

    static size_t BuildWaitDmaData(void* pBuffer);

    static size_t BuildSetSeqContextRegs(
        uint32 startRegAddr,
        uint32 endRegAddr,
        void*  pBuffer);

    template <Pm4ShaderType ShaderType = ShaderGraphics>
    static size_t BuildSetSeqShRegs(
        uint32 startRegAddr,
        uint32 endRegAddr,
        void*  pBuffer);

    static ME_WAIT_REG_MEM_function_enum WaitRegMemFunc(CompareFunc compareFunc);
    static size_t BuildRewind(
        bool  offloadEnable,
        bool  valid,
        void* pBuffer);

    template <Pm4ShaderType ShaderType = ShaderGraphics>
    static size_t BuildSetBase(
        gpusize                      address,
        PFP_SET_BASE_base_index_enum baseIndex,
        void*                        pBuffer);
    static size_t BuildAtomicMem(
        AtomicOp atomicOp,
        gpusize  dstMemAddr,
        uint64   srcData,
        void*    pBuffer);

    static size_t BuildCondIndirectBuffer(
        CompareFunc compareFunc,
        gpusize     compareGpuAddr,
        uint64      data,
        uint64      mask,
        void*       pBuffer);

    static size_t BuildCondExec(
        gpusize gpuVirtAddr,
        uint32  sizeInDwords,
        void*   pBuffer);

    static size_t BuildPrimeGpuCaches(
        const PrimeGpuCacheRange& primeGpuCacheRange,
        gpusize                   clampSize,
        EngineType                engineType,
        void*                     pBuffer);

    static size_t BuildPrimeUtcL2(
        gpusize gpuAddr,
        uint32  cachePerm,
        uint32  primeMode,
        uint32  engineSel,
        size_t  requestedPages, // Number of 4KB pages to prefetch.
        void*   pBuffer);

    static size_t BuildOcclusionQuery(gpusize queryMemAddr, gpusize dstMemAddr, void* pBuffer);

    // Checks if the register offset provided can be read or written using a COPY_DATA packet.

    static size_t BuildExecuteIndirectV2Gfx(
        Pm4Predicate                     predicate,
        const bool                       isGfx,
        const ExecuteIndirectPacketInfo& packetInfo,
        ExecuteIndirectMeta*             pMeta,
        void*                            pBuffer);

    static size_t BuildExecuteIndirectV2Ace(
        Pm4Predicate                     predicate,
        const ExecuteIndirectPacketInfo& packetInfo,
        ExecuteIndirectMeta*             pMeta,
        void*                            pBuffer);

    static size_t BuildPerfmonControl(
        uint32 perfMonCtlId,
        bool   enable,
        uint32 eventSelect,
        uint32 eventUnitMask,
        void*  pBuffer);

    static size_t BuildLoadBufferFilledSizes(
        const gpusize  streamoutCtrlBuf,
        const gpusize* pStreamoutTargets,
        void*          pBuffer);

    static size_t BuildSetBufferFilledSize(
        const gpusize streamoutCtrlBuf,
        const uint32  bufferId,
        const uint32  bufferOffset,
        void*         pBuffer);

    static size_t BuildSaveBufferFilledSizes(
        const gpusize  streamoutCtrlBuf,
        const gpusize* pStreamoutTargets,
        void*          pBuffer);

    static size_t BuildStreamoutStatsQuery(
        const gpusize streamoutCtrlBuf,
        const uint32  streamIndex,
        const gpusize streamoutDst,
        void*         pBuffer);

    static size_t BuildTaskStateInit(
        gpusize       controlBufferAddr,
        Pm4Predicate  predicate,
        Pm4ShaderType shaderType,
        void*         pBuffer);

    size_t BuildPerfCounterWindow(
        EngineType engineType,
        bool       enableWindow,
        void*      pBuffer) const;

    static constexpr uint32 PerfCounterWindowSizeDwords = PM4_PFP_PERF_COUNTER_WINDOW_SIZEDW__CORE;
    size_t BuildHdpFlush(void* pBuffer) const;

    static size_t BuildUpdateDbSummarizerTimeouts(uint32 timeout, void* pBuffer);

    static bool IsIndexedRegister(uint32 addr);

    const Device&            m_device;
    const GpuChipProperties& m_chipProps;
};

} // namespace Gfx12
} // namespace Pal
