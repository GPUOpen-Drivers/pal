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

#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "palDevice.h"

namespace Pal
{

class      CmdStream;
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

// The "official" "event-write" packet definition (see:  PM4_MEC_EVENT_WRITE) contains "extra" dwords that aren't
// necessary (and would cause problems if they existed) for event writes other than "".  Define a "plain" event-write
// packet definition here.
struct PM4ME_NON_SAMPLE_EVENT_WRITE
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

// On different hardware families, some registers have different register offsets. This structure stores the register
// offsets for some of these registers.
struct RegisterInfo
{
    uint16  mmCpPerfmonCntl;
    uint16  mmCpStrmoutCntl;
    uint16  mmGrbmGfxIndex;
    uint16  mmRlcPerfmonCntl;
    uint16  mmSqPerfCounterCtrl;
    uint16  mmSqThreadTraceUserData2;
    uint16  mmSqThreadTraceUserData3;
    uint16  mmEaPerfResultCntl;
    uint16  mmAtcPerfResultCntl;
    uint16  mmAtcL2PerfResultCntl;
    uint16  mmMcVmL2PerfResultCntl;
    uint16  mmRpbPerfResultCntl;
    uint16  mmSpiShaderPgmLoLs;
    uint16  mmSpiShaderPgmLoEs;
    uint16  mmVgtGsMaxPrimsPerSubGroup;
    uint16  mmDbDfsmControl;
    uint16  mmDbDepthInfo;
    uint16  mmUserDataStartHsShaderStage;
    uint16  mmUserDataStartGsShaderStage;
    uint16  mmSpiConfigCntl;
    uint16  mmPaStereoCntl;
    uint16  mmPaStateStereoX;
    uint16  mmComputeShaderChksum;
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
    static constexpr uint32 CondIndirectBufferSize  = (sizeof(PM4PFP_COND_INDIRECT_BUFFER) / sizeof(uint32));
    static constexpr uint32 DispatchDirectSize      = (sizeof(PM4PFP_DISPATCH_DIRECT) / sizeof(uint32));
    static constexpr uint32 DispatchIndirectMecSize = (sizeof(PM4MEC_DISPATCH_INDIRECT) / sizeof(uint32));
    static constexpr uint32 DrawIndexAutoSize       = (sizeof(PM4_PFP_DRAW_INDEX_AUTO) / sizeof(uint32));
    static constexpr uint32 DrawIndex2Size          = (sizeof(PM4_PFP_DRAW_INDEX_2) / sizeof(uint32));
    static constexpr uint32 DrawIndexOffset2Size    = (sizeof(PM4_PFP_DRAW_INDEX_OFFSET_2) / sizeof(uint32));
    static constexpr uint32 MinNopSizeInDwords      = 1; // all gfx9 HW supports 1-DW NOP packets

    static_assert (sizeof(PM4PFP_COND_EXEC) == sizeof(PM4MEC_COND_EXEC),
                   "Conditional execution packet size does not match between PFP and compute engines!");
    static_assert (sizeof(PM4PFP_COND_EXEC) == sizeof(PM4CE_COND_EXEC),
                   "Conditional execution packet size does not match between PFP and constant engines!");
    static constexpr uint32 CondExecSizeDwords        = (sizeof(PM4PFP_COND_EXEC) / sizeof(uint32));
    static constexpr uint32 ContextRegRmwSizeDwords   = (sizeof(PM4ME_CONTEXT_REG_RMW) / sizeof(uint32));
    static constexpr uint32 RegRmwSizeDwords          = (sizeof(PM4_ME_REG_RMW) / sizeof(uint32));
    static constexpr uint32 ConfigRegSizeDwords       = (sizeof(PM4PFP_SET_UCONFIG_REG) / sizeof(uint32));
    static constexpr uint32 ContextRegSizeDwords      = (sizeof(PM4PFP_SET_CONTEXT_REG) / sizeof(uint32));
    static constexpr uint32 DmaDataSizeDwords         = (sizeof(PM4PFP_DMA_DATA) / sizeof(uint32));
    static constexpr uint32 NumInstancesDwords        = (sizeof(PM4_PFP_NUM_INSTANCES) / sizeof(uint32));
    static constexpr uint32 OcclusionQuerySizeDwords  = (sizeof(PM4PFP_OCCLUSION_QUERY) / sizeof(uint32));
    static constexpr uint32 ShRegSizeDwords           = (sizeof(PM4ME_SET_SH_REG) / sizeof(uint32));
    static constexpr uint32 ShRegIndexSizeDwords      = (sizeof(PM4_PFP_SET_SH_REG_INDEX) / sizeof(uint32));
    static constexpr uint32 WaitRegMemSizeDwords      = (sizeof(PM4ME_WAIT_REG_MEM) / sizeof(uint32));
    static constexpr uint32 WaitRegMem64SizeDwords    = (sizeof(PM4ME_WAIT_REG_MEM64) / sizeof(uint32));
    static constexpr uint32 WriteDataSizeDwords       = (sizeof(PM4ME_WRITE_DATA) / sizeof(uint32));
    static constexpr uint32 WriteNonSampleEventDwords = (sizeof(PM4ME_NON_SAMPLE_EVENT_WRITE) / sizeof(uint32));

    // The INDIRECT_BUFFER and COND_INDIRECT_BUFFER packet have a hard-coded IB size of 20 bits.
    static constexpr uint32 MaxIndirectBufferSizeDwords = (1 << 20) - 1;

    static bool                          IsContextReg(uint32 regAddr);
    static bool                          IsPrivilegedConfigReg(uint32 regAddr);
    static bool                          IsShReg(uint32 regAddr);
    static ME_WAIT_REG_MEM_function_enum WaitRegMemFunc(CompareFunc compareFunc);

    size_t BuildAcquireMem(
        const AcquireMemInfo& acquireMemInfo,
        void*                 pBuffer) const;
    size_t BuildAtomicMem(
        AtomicOp atomicOp,
        gpusize  dstMemAddr,
        uint64   srcData,
        void*    pBuffer) const;
    size_t BuildClearState(
        PFP_CLEAR_STATE_cmd_enum command,
        void*                    pBuffer) const;
    size_t BuildCondExec(
        gpusize gpuVirtAddr,
        uint32  sizeInDwords,
        void*   pBuffer) const;
    size_t BuildCondIndirectBuffer(
        CompareFunc compareFunc,
        gpusize     compareGpuAddr,
        uint64      data,
        uint64      mask,
        bool        constantEngine,
        void*       pBuffer) const;
    size_t BuildContextControl(
        const PM4PFP_CONTEXT_CONTROL& contextControl,
        void*                         pBuffer) const;
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
    size_t BuildCopyDataGraphics(
        uint32                        engineSel,
        ME_COPY_DATA_dst_sel_enum     dstSel,
        gpusize                       dstAddr,
        ME_COPY_DATA_src_sel_enum     srcSel,
        gpusize                       srcAddr,
        ME_COPY_DATA_count_sel_enum   countSel,
        ME_COPY_DATA_wr_confirm_enum  wrConfirm,
        void*                         pBuffer) const;
    size_t BuildCopyDataCompute(
        MEC_COPY_DATA_dst_sel_enum     dstSel,
        gpusize                        dstAddr,
        MEC_COPY_DATA_src_sel_enum     srcSel,
        gpusize                        srcAddr,
        MEC_COPY_DATA_count_sel_enum   countSel,
        MEC_COPY_DATA_wr_confirm_enum  wrConfirm,
        void*                          pBuffer) const;
    template <bool dimInThreads, bool forceStartAt000>
    size_t BuildDispatchDirect(
        uint32       xDim,
        uint32       yDim,
        uint32       zDim,
        Pm4Predicate predicate,
        void*        pBuffer) const;
    size_t BuildDispatchIndirectGfx(
        gpusize      byteOffset,
        Pm4Predicate predicate,
        void*        pBuffer) const;
    size_t BuildDispatchIndirectMec(
        gpusize address,
        void*   pBuffer) const;
    size_t BuildDrawIndex2(
        uint32       indexCount,
        uint32       indexBufSize,
        gpusize      indexBufAddr,
        Pm4Predicate predicate,
        void*        pBuffer) const;
    size_t BuildDrawIndexOffset2(
        uint32       indexCount,
        uint32       indexBufSize,
        uint32       indexOffset,
        Pm4Predicate predicate,
        void*        pBuffer) const;
    size_t BuildDrawIndexIndirect(
        gpusize      offset,
        uint32       baseVtxLoc,
        uint32       startInstLoc,
        uint32       startIndexLoc,
        Pm4Predicate predicate,
        void*        pBuffer) const;
    size_t BuildDrawIndexIndirectMulti(
        gpusize      offset,
        uint32       baseVtxLoc,
        uint32       startInstLoc,
        uint32       drawIndexLoc,
        uint32       startIndexLoc,
        uint32       stride,
        uint32       count,
        gpusize      countGpuAddr,
        Pm4Predicate predicate,
        void*        pBuffer) const;
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
    size_t BuildDrawIndexAuto(
        uint32       indexCount,
        bool         useOpaque,
        Pm4Predicate predicate,
        void*        pBuffer) const;
    size_t BuildDmaData(
        DmaDataInfo&  dmaDataInfo,
        void*         pBuffer) const;
    size_t BuildDumpConstRam(
        gpusize dstGpuAddr,
        uint32  ramByteOffset,
        uint32  dwordSize,
        void*   pBuffer) const;
    size_t BuildDumpConstRamOffset(
        uint32  dstAddrOffset,
        uint32  ramByteOffset,
        uint32  dwordSize,
        void*   pBuffer) const;
    size_t BuildNonSampleEventWrite(
        VGT_EVENT_TYPE  vgtEvent,
        EngineType      engineType,
        void*           pBuffer) const;
    size_t BuildSampleEventWrite(
        VGT_EVENT_TYPE  vgtEvent,
        EngineType      engineType,
        gpusize         gpuAddr,
        void*           pBuffer) const;
    size_t BuildIncrementCeCounter(void* pBuffer) const;
    size_t BuildIncrementDeCounter(void* pBuffer) const;
    size_t BuildIndexAttributesIndirect(gpusize baseAddr, uint16 index, void* pBuffer) const;
    size_t BuildIndexBase(gpusize baseAddr, void* pBuffer) const;
    size_t BuildIndexBufferSize(uint32 indexCount, void* pBuffer) const;
    size_t BuildIndexType(uint32 vgtDmaIndexType, void* pBuffer) const;
    size_t BuildIndirectBuffer(
        EngineType engineType,
        gpusize    ibAddr,
        uint32     ibSize,
        bool       chain,
        bool       constantEngine,
        bool       enablePreemption,
        void*      pBuffer) const;

    size_t BuildLoadConfigRegs(
        gpusize              gpuVirtAddr,
        const RegisterRange* pRanges,
        uint32               rangeCount,
        void*                pBuffer) const;

    size_t BuildLoadContextRegs(
        gpusize              gpuVirtAddr,
        const RegisterRange* pRanges,
        uint32               rangeCount,
        void*                pBuffer) const;
    size_t BuildLoadContextRegs(
        gpusize gpuVirtAddr,
        uint32  startRegAddr,
        uint32  count,
        void*   pBuffer) const;
    template <bool directAddress>
    size_t BuildLoadContextRegsIndex(
        gpusize gpuVirtAddrOrAddrOffset,
        uint32  startRegAddr,
        uint32  count,
        void*   pBuffer) const;

    size_t BuildLoadConstRam(
        gpusize srcGpuAddr,
        uint32  ramByteOffset,
        uint32  dwordSize,
        void*   pBuffer) const;

    size_t BuildLoadShRegs(
        gpusize              gpuVirtAddr,
        const RegisterRange* pRanges,
        uint32               rangeCount,
        uint32               maxRangeCount,
        Pm4ShaderType        shaderType,
        void*                pBuffer) const;
    size_t BuildLoadShRegs(
        gpusize       gpuVirtAddr,
        uint32        startRegAddr,
        uint32        count,
        Pm4ShaderType shaderType,
        void*         pBuffer) const;
    template <bool directAddress>
    size_t BuildLoadShRegsIndex(
        gpusize       gpuVirtAddrOrAddrOffset,
        uint32        startRegAddr,
        uint32        count,
        Pm4ShaderType shaderType,
        void*         pBuffer) const;

    size_t BuildLoadUserConfigRegs(
        gpusize              gpuVirtAddr,
        const RegisterRange* pRanges,
        uint32               rangeCount,
        uint32               maxRangeCount,
        void*                pBuffer) const;

    size_t BuildNop(size_t numDwords, void* pBuffer) const;

    size_t BuildNumInstances(uint32 instanceCount, void* pBuffer) const;

    size_t BuildOcclusionQuery(gpusize queryMemAddr, gpusize dstMemAddr, void* pBuffer) const;

    size_t BuildPfpSyncMe(void* pBuffer) const;

    size_t BuildPreambleCntl(ME_PREAMBLE_CNTL_command_enum command, void* pBuffer) const;

    size_t BuildPrimeUtcL2(
        gpusize gpuAddr,
        uint32  cachePerm,
        uint32  primeMode,
        uint32  engineSel,
        size_t  requestedPages,
        void*   pBuffer) const;
    size_t BuildReleaseMem(
        const ReleaseMemInfo& releaseMemInfo,
        void*                 pBuffer,
        uint32                gdsAddr = 0,
        uint32                gdsSize = 0) const;
    size_t BuildRewind(
        bool  offloadEnable,
        bool  valid,
        void* pBuffer) const;
    size_t BuildSetBase(
        gpusize                      address,
        PFP_SET_BASE_base_index_enum baseIndex,
        Pm4ShaderType                shaderType,
        void*                        pBuffer) const;
    size_t BuildSetBaseCe(
        gpusize                      address,
        CE_SET_BASE_base_index_enum  baseIndex,
        Pm4ShaderType                shaderType,
        void*                        pBuffer) const;
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
    size_t BuildSetShRegDataOffset(
        uint32                           regAddr,
        uint32                           dataOffset,
        Pm4ShaderType                    shaderType,
        void*                            pBuffer,
        PFP_SET_SH_REG_OFFSET_index_enum index = index__pfp_set_sh_reg_offset__data_indirect_1dw) const;
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
    size_t BuildSetPredication(
        gpusize       gpuVirtAddr,
        bool          predicationBool,
        bool          occlusionHint,
        PredicateType predType,
        bool          continuePredicate,
        void*         pBuffer) const;
    size_t BuildStrmoutBufferUpdate(
        uint32  bufferId,
        uint32  sourceSelect,
        uint32  explicitOffset,
        gpusize dstGpuVirtAddr,
        gpusize srcGpuVirtAddr,
        void*   pBuffer) const;
    size_t BuildWaitDmaData(void* pBuffer) const;
    size_t BuildWaitOnCeCounter(bool invalidateKcache, void* pBuffer) const;
    size_t BuildWaitOnDeCounterDiff(uint32 counterDiff, void* pBuffer) const;
    size_t BuildWaitOnReleaseMemEvent(
        EngineType     engineType,
        VGT_EVENT_TYPE vgtEvent,
        TcCacheOp      tcCacheOp,
        gpusize        gpuAddr,
        void*          pBuffer) const;
    size_t BuildWaitRegMem(
        uint32  memSpace,
        uint32  function,
        uint32  engine,
        gpusize addr,
        uint32  reference,
        uint32  mask,
        void*   pBuffer) const;
    size_t BuildWaitRegMem64(
        uint32  memSpace,
        uint32  function,
        uint32  engine,
        gpusize addr,
        uint64  reference,
        uint64  mask,
        void*   pBuffer) const;
    size_t BuildWriteConstRam(
        const void* pSrcData,
        uint32      ramByteOffset,
        uint32      dwordSize,
        void*       pBuffer) const;
    size_t BuildWriteData(
        EngineType      engineType,
        gpusize         dstAddr,
        size_t          dwordsToWrite,
        uint32          engineSel,
        uint32          dstSel,
        uint32          wrConfirm,
        const uint32*   pData,
        Pm4Predicate    predicate,
        void*           pBuffer) const;
    size_t BuildWriteDataPeriodic(
        EngineType    engineType,
        gpusize       dstAddr,
        size_t        dwordsPerPeriod,
        size_t        periodsToWrite,
        uint32        engineSel,
        uint32        dstSel,
        bool          wrConfirm,
        const uint32* pPeriodData,
        Pm4Predicate  predicate,
        void*         pBuffer) const;

    size_t BuildCommentString(const char* pComment, void* pBuffer) const;

    // Returns the register information for registers which have differing addresses between hardware families.
    const RegisterInfo& GetRegInfo() const { return m_registerInfo; }

private:
    size_t BuildWriteDataInternal(
        EngineType     engineType,
        gpusize        dstAddr,
        size_t         dwordsToWrite,
        uint32         engineSel,
        uint32         dstSel,
        uint32         wrConfirm,
        Pm4Predicate   predicate,
        void*          pBuffer) const;
    size_t BuildCopyDataInternal(
        EngineType engineType,
        uint32     engineSel,
        uint32     dstSel,
        gpusize    dstAddr,
        uint32     srcSel,
        gpusize    srcAddr,
        uint32     countSel,
        uint32     wrConfirm,
        void*      pBuffer) const;

    template <typename AcquireMemPacketType>
    uint32 BuildAcquireMemInternal(
        const AcquireMemInfo&  acquireMemInfo,
        AcquireMemPacketType*  pBuffer) const;

    template <typename ReleaseMemPacketType>
    size_t BuildReleaseMemInternal(
        const ReleaseMemInfo&  releaseMemInfo,
        ReleaseMemPacketType*  pPacket,
        uint32                 gdsAddr,
        uint32                 gdsSize) const;

#if PAL_ENABLE_PRINTS_ASSERTS
    void CheckShadowedContextReg(uint32 regAddr) const;
    void CheckShadowedContextRegs(uint32 startRegAddr, uint32 endRegAddr) const;
    void CheckShadowedShReg(Pm4ShaderType shaderType, uint32 regAddr) const;
    void CheckShadowedShRegs(Pm4ShaderType shaderType, uint32 startRegAddr, uint32 endRegAddr) const;
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
