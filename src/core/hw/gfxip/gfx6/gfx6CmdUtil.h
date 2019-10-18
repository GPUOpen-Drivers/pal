/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/device.h"
#include "core/hw/gfxip/gfx6/gfx6Chip.h"
#include "palCmdBuffer.h"

namespace Pal
{

class CmdStream;
class Device;
class PipelineUploader;
enum class PredicateType : uint32;

namespace Gfx6
{

// Data required to perform a DMA Data transfer (aka CPDMA).
//
// Note that the "sync" flag should be set in almost all cases. The two exceptions are:
//   1. The caller will manually synchronize the CP DMA engine using another DMA.
//   2. The caller is operating under "CoherCopy/HwPipePostBlt" semantics and a CmdBarrier call will be issued. This
//      case is commonly referred to as a "CP Blt".
//
// In case #2, the caller must update the GfxCmdBufferState by calling the relevant SetGfxCmdBuf* functions.
// Furthermore, the caller must not set "disableWc" because write-confirms are necessary for the barrier to guarantee
// that the CP DMA writes have made it to their destination (memory, L2, etc.).
struct DmaDataInfo
{
    CPDMA_DST_SEL     dstSel;         // Destination select - 0 dstAddrSpace, 1 GDS
    gpusize           dstAddr;        // Destination address for dstSel Addr or offset for GDS
    CPDMA_ADDR_SPACE  dstAddrSpace;   // Destination address space - 0 memory, 1 register
    CPDMA_SRC_SEL     srcSel;         // Src select - 0 srcAddrSpace, 1 GDS
    uint32            srcData;        // Source data for srcSel data or offset for srcSel GDS
    gpusize           srcAddr;        // Source gpu virtual address
    CPDMA_ADDR_SPACE  srcAddrSpace;   // Source address space - 0 memory, 1 register
    uint32            numBytes;       // Number of bytes to copy
    bool              sync;           // Synchronize the transfer
    bool              usePfp;         // true chooses PFP engine, false chooses ME
    bool              disableWc;      // true disables WRITE_CONFIRM
    PM4Predicate      predicate;      // Set if currently using predication
};

// Data required to build a write_data packet. We try to set up this struct so that zero-initializing gives reasonable
// values for rarely changed members like predicate, dontWriteConfirm, etc.
struct WriteDataInfo
{
    gpusize      dstAddr;           // Destination GPU memory address or memory mapped register offset.
    uint32       engineSel;         // Which CP engine executes this packet (see WRITE_DATA_ENGINE_*).
                                    // Ignored on the MEC.
    uint32       dstSel;            // Where to write the data (see WRITE_DATA_DST_SEL_*)
    PM4Predicate predicate;         // If this packet respects predication (zero defaults to disabled).
    bool         dontWriteConfirm;  // If the engine should continue immediately without waiting for a write-confirm.
    bool         dontIncrementAddr; // If the engine should write every DWORD to the same destination address.
                                    // Some memory mapped registers use this to stream in an array of data.
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
    uint16  mmSqThreadTraceBase;
    uint16  mmSqThreadTraceBase2;
    uint16  mmSqThreadTraceSize;
    uint16  mmSqThreadTraceMask;
    uint16  mmSqThreadTraceTokenMask;
    uint16  mmSqThreadTracePerfMask;
    uint16  mmSqThreadTraceCtrl;
    uint16  mmSqThreadTraceMode;
    uint16  mmSqThreadTraceWptr;
    uint16  mmSqThreadTraceStatus;
    uint16  mmSqThreadTraceHiWater;
    uint16  mmSrbmPerfmonCntl;
};

// Pre-baked commands to prefetch (prime caches) for a pipeline.  This will be done with a CPDMA operation that will
// prime GL2.
struct PipelinePrefetchPm4
{
    PM4DMADATA dmaData;
    uint32     spaceNeeded;
};

// =====================================================================================================================
// Utility class which provides routines to help build PM4 packets.
class CmdUtil
{
public:
    explicit CmdUtil(const Pal::Device& device);
    ~CmdUtil() {}

    // These return the number of DWORDs required to build various packets.

    static constexpr uint32 GetChainSizeInDwords()       { return PM4_CMD_INDIRECT_BUFFER_DWORDS; }
    static constexpr uint32 GetCondIndirectBufferSize()  { return PM4_CMD_COND_INDIRECT_BUFFER_DWORDS; }
    static constexpr uint32 GetContextRegRmwSize()       { return PM4_CONTEXT_REG_RMW_DWORDS; }
    static constexpr uint32 GetCopyDataSize()            { return PM4_CMD_COPY_DATA_DWORDS; }
    static constexpr uint32 GetDispatchDirectSize()      { return PM4_CMD_DISPATCH_DIRECT_DWORDS; }
    static constexpr uint32 GetDispatchIndirectMecSize() { return PM4_CMD_DISPATCH_INDIRECT_MEC_DWORDS; }
    static constexpr uint32 GetDispatchIndirectSize()    { return PM4_CMD_DISPATCH_INDIRECT_DWORDS; }
    static constexpr uint32 GetDrawIndexAutoSize()       { return PM4_CMD_DRAW_INDEX_AUTO_DWORDS; }
    static constexpr uint32 GetDrawIndex2Size()          { return PM4_CMD_DRAW_INDEX_2_DWORDS; }
    static constexpr uint32 GetDrawIndexOffset2Size()    { return PM4_CMD_DRAW_INDEX_OFFSET_2_DWORDS; }
    static constexpr uint32 GetIndexTypeSize()           { return PM4_CMD_DRAW_INDEX_TYPE_DWORDS; }
    static constexpr uint32 GetNumInstancesSize()        { return PM4_CMD_DRAW_NUM_INSTANCES_DWORDS; }
    static constexpr uint32 GetOcclusionQuerySize()      { return PM4_CMD_OCCLUSION_QUERY_DWORDS; }
    static constexpr uint32 GetSetBaseSize()             { return PM4_CMD_DRAW_SET_BASE_DWORDS; }
    static constexpr uint32 GetSetDataHeaderSize()       { return PM4_CMD_SET_DATA_DWORDS; }
    static constexpr uint32 GetWaitRegMemSize()          { return PM4_CMD_WAIT_REG_MEM_DWORDS; }
    static constexpr uint32 GetWriteDataHeaderSize()     { return PM4_CMD_WRITE_DATA_DWORDS; }
    static constexpr uint32 GetWriteEventWriteSize()     { return PM4_CMD_WAIT_EVENT_WRITE_DWORDS; }

    // The INDIRECT_BUFFER and COND_INDIRECT_BUFFER packet have a hard-coded IB size of 20 bits (in units of DWORDS).
    static constexpr uint32 GetMaxIndirectBufferSize()   { return (1 << 20) - 1; }

    static uint32 WaitRegMemFuncFromCompareType(CompareFunc compareFunc);

    uint32 GetCondExecSizeInDwords() const;
    uint32 GetDmaDataWorstCaseSize() const;
    uint32 GetDmaDataSizeInDwords(const DmaDataInfo& dmaData) const;
    uint32 GetMinNopSizeInDwords() const;

    // Returns the register information for registers which have differing addresses between hardware families.
    const RegisterInfo& GetRegInfo() const { return m_registerInfo; }

    bool IsPrivilegedConfigReg(uint32 regAddr) const;

    // Packet building functions in alphabetical order.

    size_t BuildAcquireMem(regCP_COHER_CNTL cpCoherCntl, gpusize baseAddress, gpusize sizeBytes, void* pPacket) const;
    size_t BuildAtomicMem(AtomicOp atomicOp, gpusize dstMemAddr, uint64 srcData, void* pBuffer) const;

    size_t BuildClearState(void* pBuffer) const;

    size_t BuildCondExec(gpusize gpuVirtAddr, uint32 sizeInDwords, void* pBuffer) const;

    size_t BuildCondIndirectBuffer(
        CompareFunc compareFunc,
        gpusize     compareGpuAddr,
        uint64      reference,
        uint64      mask,
        bool        constantEngine,
        void*       pBuffer) const;

    size_t BuildContextControl(CONTEXT_CONTROL_ENABLE loadBits, CONTEXT_CONTROL_ENABLE shadowBits, void* pBuffer) const;
    size_t BuildContextRegRmw(uint32 regAddr, uint32 regMask, uint32 regData, void* pBuffer) const;
    size_t BuildRegRmw(uint32 regAddr, uint32 orMask, uint32 andMask, void*  pBuffer) const;

    size_t BuildCopyData(
        uint32  dstSel,
        gpusize dstAddr,
        uint32  srcSel,
        gpusize srcAddr,
        uint32  countSel,
        uint32  engineSel,
        uint32  wrConfirm,
        void*   pBuffer) const;

    size_t BuildDispatchDirect(
        uint32       xDim,
        uint32       yDim,
        uint32       zDim,
        bool         dimInThreads,
        bool         forceStartAt000,
        PM4Predicate predicate,
        void*        pBuffer) const;
    size_t BuildDispatchIndirect(gpusize offset, PM4Predicate predicate, void* pBuffer) const;
    size_t BuildDispatchIndirectMec(gpusize address, void* pBuffer) const;

    size_t BuildDmaData(const DmaDataInfo& dmaData, void* pBuffer) const;

    size_t BuildDrawIndex2(
        uint32       indexCount,
        uint32       indexBufSize,
        gpusize      indexBufAddr,
        PM4Predicate predicate,
        void*        pBuffer) const;
    size_t BuildDrawIndexAuto(
        uint32       indexCount,
        bool         useOpaque,
        PM4Predicate predicate,
        void*        pBuffer) const;
    size_t BuildDrawIndexIndirect(
        gpusize      offset,
        uint32       baseVtxLoc,
        uint32       startInstLoc,
        PM4Predicate predicate,
        void*        pBuffer) const;
    size_t BuildDrawIndexIndirectMulti(
        gpusize      offset,
        uint32       baseVtxLoc,
        uint32       startInstLoc,
        uint32  drawIndexLoc,
        uint32       stride,
        uint32       count,
        gpusize      countGpuAddr,
        PM4Predicate predicate,
        void*        pBuffer) const;
    size_t BuildDrawIndexOffset2(
        uint32       indexCount,
        uint32       indexBufSize,
        uint32       indexOffset,
        PM4Predicate predicate,
        void*        pBuffer) const;
    size_t BuildDrawIndirectMulti(
        gpusize      offset,
        uint32       baseVtxLoc,
        uint32       startInstLoc,
        uint32  drawIndexLoc,
        uint32       stride,
        uint32       count,
        gpusize      countGpuAddr,
        PM4Predicate predicate,
        void*        pBuffer) const;

    size_t BuildDumpConstRam(gpusize dstGpuAddr, uint32 ramByteOffset, uint32 dwordSize, void* pBuffer) const;
    size_t BuildDumpConstRamOffset(uint32 addrOffset, uint32 ramByteOffset, uint32 dwordSize, void* pBuffer) const;

    size_t BuildEventWrite(VGT_EVENT_TYPE eventType, void* pBuffer) const;
    size_t BuildEventWriteEop(
        VGT_EVENT_TYPE eventType,
        gpusize        gpuAddress,
        uint32         dataSel,
        uint64         data,
        bool           flushInvL2,
        void*          pBuffer) const;

    size_t BuildEventWriteEos(
        VGT_EVENT_TYPE eventType,
        gpusize        dstMemAddr,
        uint32         command,
        uint32         data,
        uint32         gdsIndex,
        uint32         gdsSize,
        void*          pBuffer) const;

    size_t BuildEventWriteQuery(VGT_EVENT_TYPE eventType, gpusize address, void* pBuffer) const;

    size_t BuildExecutionMarker(
        gpusize markerAddr,
        uint32  markerVal,
        uint64  clientHandle,
        uint32  markerType,
        void*   pBuffer) const;

    size_t BuildGenericSync(
        regCP_COHER_CNTL cpCoherCntl,
        uint32           syncEngine,
        gpusize          baseAddress,
        gpusize          sizeBytes,
        bool             forComputeEngine,
        void*            pBuffer) const;

    size_t BuildGenericEopEvent(
        VGT_EVENT_TYPE eventType,
        gpusize        gpuAddress,
        uint32         dataSel,
        uint64         data,
        bool           forComputeEngine,
        bool           flushInvL2,
        void*          pBuffer) const;

    size_t BuildGenericEosEvent(
        VGT_EVENT_TYPE eventType,
        gpusize        dstMemAddr,
        uint32         command,
        uint32         data,
        uint32         gdsIndex,
        uint32         gdsSize,
        bool           forComputeEngine,
        void*          pBuffer) const;

    size_t BuildIncrementCeCounter(void* pBuffer) const;
    size_t BuildIncrementDeCounter(void* pBuffer) const;

    size_t BuildIndexAttributesIndirect(gpusize baseAddr, uint16 index, void* pBuffer) const;
    size_t BuildIndexBase(gpusize baseAddr, void* pBuffer) const;
    size_t BuildIndexBufferSize(uint32 indexCount, void* pBuffer) const;
    size_t BuildIndexType(regVGT_DMA_INDEX_TYPE__VI vgtDmaIndexType, void* pBuffer) const;

    size_t BuildIndirectBuffer(
        gpusize gpuAddr,
        size_t  sizeInDwords,
        bool    chain,
        bool    constantEngine,
        bool    enablePreemption,
        void*   pBuffer) const;

    size_t BuildLoadConfigRegs(
        gpusize              gpuVirtAddr,
        const RegisterRange* pRanges,
        uint32               rangeCount,
        void*                pBuffer) const;

    size_t BuildLoadConstRam(
        gpusize srcGpuAddr,
        uint32  ramByteOffset,
        uint32  dwordSize,
        void*   pBuffer) const;

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
    size_t BuildLoadContextRegsIndex(
        gpusize gpuVirtAddr,
        uint32  count,
        void*   pBuffer) const;

    size_t BuildLoadShRegs(
        gpusize              gpuVirtAddr,
        const RegisterRange* pRanges,
        uint32               rangeCount,
        PM4ShaderType        shaderType,
        void*                pBuffer) const;
    size_t BuildLoadShRegs(
        gpusize       gpuVirtAddr,
        uint32        startRegAddr,
        uint32        count,
        PM4ShaderType shaderType,
        void*         pBuffer) const;
    size_t BuildLoadShRegsIndex(
        uint32        addrOffset,
        uint32        startRegAddr,
        uint32        count,
        PM4ShaderType shaderType,
        void*         pBuffer) const;
    size_t BuildLoadShRegsIndex(
        gpusize       gpuVirtAddr,
        uint32        count,
        PM4ShaderType shaderType,
        void*         pBuffer) const;

    size_t BuildLoadUserConfigRegs(
        gpusize              gpuVirtAddr,
        const RegisterRange* pRanges,
        uint32               rangeCount,
        void*                pBuffer) const;

    size_t BuildMemSemaphore(
        gpusize gpuVirtAddr,
        uint32  semaphoreOp,
        uint32  semaphoreClient,
        bool    useMailbox,
        bool    isBinary,
        void*   pBuffer) const;

    size_t BuildNop(size_t numDwords, void* pBuffer) const;

    size_t BuildNumInstances(uint32 instanceCount, void* pBuffer) const;

    size_t BuildOcclusionQuery(gpusize queryMemAddr, gpusize dstMemAddr, void* pBuffer) const;

    size_t BuildPfpSyncMe(void* pBuffer) const;

    size_t BuildPreambleCntl(uint32 command, void* pBuffer) const;

    size_t BuildReleaseMem(
        VGT_EVENT_TYPE eventType,
        gpusize        gpuAddress,
        uint32         dataSel,
        uint64         data,
        uint32         gdsAddr,
        uint32         gdsSize,
        bool           flushInvL2,
        void*          pBuffer) const;

    size_t BuildRewind(
        bool  offloadEnable,
        bool  valid,
        void* pBuffer) const;

    size_t BuildSetBase(PM4ShaderType shaderType, uint32 baseIndex, gpusize baseAddr, void* pBuffer) const;

    size_t BuildSetOneConfigReg(uint32 regAddr, void* pBuffer, uint32 index = 0) const;
    size_t BuildSetOneContextReg(uint32 regAddr, void* pBuffer, uint32 index = 0) const;
    size_t BuildSetOneShReg(uint32 regAddr, PM4ShaderType shaderType, void* pBuffer) const;
    size_t BuildSetOneShRegIndex(uint32 regAddr, PM4ShaderType shaderType, uint32 index, void* pBuffer) const;

    size_t BuildSetSeqConfigRegs(uint32 startRegAddr, uint32 endRegAddr, void* pBuffer) const;
    size_t BuildSetSeqContextRegs(uint32 startRegAddr, uint32 endRegAddr, void* pBuffer) const;
    size_t BuildSetSeqShRegs(uint32 startRegAddr, uint32 endRegAddr, PM4ShaderType shaderType, void* pBuffer) const;
    size_t BuildSetSeqShRegsIndex(
        uint32        startRegAddr,
        uint32        endRegAddr,
        PM4ShaderType shaderType,
        uint32        index,
        void*         pBuffer) const;

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

    size_t BuildSurfaceSync(
        regCP_COHER_CNTL cpCoherCntl,
        uint32           syncEngine,
        gpusize          baseAddress,
        gpusize          sizeBytes,
        void*            pBuffer) const;

    size_t BuildWaitDmaData(void* pBuffer) const;

    size_t BuildWaitOnCeCounter(bool invalidateKcache, void* pBuffer) const;
    size_t BuildWaitOnDeCounterDiff(uint32 counterDiff, void* pBuffer) const;

    size_t BuildWaitOnEopEvent(VGT_EVENT_TYPE eventType, gpusize gpuAddr, void* pBuffer) const;
    size_t BuildWaitOnGenericEopEvent(
        VGT_EVENT_TYPE eventType,
        gpusize        gpuAddr,
        bool           forComputeEngine,
        void*          pBuffer) const;

    size_t BuildWaitOnEosEvent(VGT_EVENT_TYPE eventType, gpusize gpuAddr, void* pBuffer) const;
    size_t BuildWaitOnGenericEosEvent(
        VGT_EVENT_TYPE eventType,
        gpusize        gpuAddr,
        bool           forComputeEngine,
        void*          pBuffer) const;

    size_t BuildWaitRegMem(
        uint32  memSpace,
        uint32  function,
        uint32  engine,
        gpusize addr,
        uint32  reference,
        uint32  mask,
        bool    isSdi,
        void*   pBuffer) const;

    size_t BuildWriteConstRam(const void* pSrcData, uint32 ramByteOffset, uint32 dwordSize, void* pBuffer) const;

    size_t BuildWriteData(
        const WriteDataInfo& info,
        uint32               data,
        void*                pBuffer) const;

    size_t BuildWriteData(
        const WriteDataInfo& info,
        size_t               dwordsToWrite,
        const uint32*        pData,
        void*                pBuffer) const;

    size_t BuildWriteDataPeriodic(
        const WriteDataInfo& info,
        size_t               dwordsPerPeriod,
        size_t               periodsToWrite,
        const uint32*        pPeriodData,
        void*                pBuffer) const;

    size_t BuildCommentString(const char* pComment, void* pBuffer) const;
    size_t BuildNopPayload(const void* pPayload, uint32 payloadSize, void* pBuffer) const;

    void BuildPipelinePrefetchPm4(const PipelineUploader& uploader, PipelinePrefetchPm4* pOutput) const;

    GfxIpLevel IpLevel() const { return m_chipFamily; }

private:
    // These packet building functions need to be hidden from users of CmdUtil.
    size_t BuildCpDmaInternal(const DmaDataInfo& dmaData, void* pBuffer) const;
    size_t BuildDmaDataInternal(const DmaDataInfo& dmaData, void* pBuffer) const;
    size_t BuildDmaDataSizeFixup(uint32 sizeInBytes, void* pBuffer) const;

    // Whenever CmdUtil needs to issue a DMA request it should call this method.
    size_t BuildGenericDmaDataInternal(const DmaDataInfo& dmaData, void* pBuffer) const
    {
        return (m_chipFamily == GfxIpLevel::GfxIp6) ? BuildCpDmaInternal(dmaData, pBuffer)
                                                    : BuildDmaDataInternal(dmaData, pBuffer);
    }

    template <IT_OpCodeType opCode>
    PAL_INLINE size_t BuildLoadRegsOne(
        gpusize       gpuVirtAddr,
        uint32        startRegOffset,
        uint32        count,
        PM4ShaderType shaderType,
        void*         pBuffer) const;

    template <IT_OpCodeType opCode>
    PAL_INLINE size_t BuildLoadRegsMulti(
        gpusize              gpuVirtAddr,
        const RegisterRange* pRanges,
        uint32               rangeCount,
        PM4ShaderType        shaderType,
        void*                pBuffer) const;

    template <IT_OpCodeType opCode, bool directAddress, uint32 dataFormat>
    PAL_INLINE size_t BuildLoadRegsIndex(
        gpusize       gpuVirtAddrOrAddrOffset,
        uint32        startRegOffset,
        uint32        count,
        PM4ShaderType shaderType,
        void*         pBuffer) const;

    bool IsConfigReg(uint32 regAddr) const;
    bool IsUserConfigReg(uint32 regAddr) const;
    bool IsContextReg(uint32 regAddr) const;
    bool IsShReg(uint32 regAddr) const;

    bool Is32BitAtomicOp(AtomicOp atomicOp) const;
    TC_OP TranslateAtomicOp(AtomicOp atomicOp) const;

    // Helper method that sets bitfields in a Type 3 PM4 packet, returns the PM4 header as a uint32.
    // The shaderType argument doesn't matter (can be left at its default) for all packets except the following:
    // - load_sh_reg
    // - set_base
    // - set_sh_reg
    // - set_sh_reg_offset
    // - write_gds
    PAL_INLINE uint32 Type3Header(
        IT_OpCodeType opCode,
        size_t        packetSize,
        PM4ShaderType shaderType = ShaderGraphics,
        PM4Predicate  predicate  = PredDisable) const
        { return PM4_TYPE_3_HDR(opCode, static_cast<uint32>(packetSize), shaderType, predicate); }

    // Helper method to generate the 2nd ordinal of a PM4CMDSETDATA packet:
    // union
    // {
    //     struct
    //     {
    //         unsigned int    regOffset : 16;  ///< offset in DWords from the register base address
    //         unsigned int    reserved1 : 12;  ///< Program to zero
    //         unsigned int    index     : 4;   ///< Index for UCONFIG/CONTEXT on CI+
    //                                          ///< Program to zero for other opcodes and on SI
    //     };
    //     unsigned int        ordinal2;
    // };
    PAL_INLINE uint32 SetDataOrdinal2(
        uint32 regOffset,
        uint32 index = 0) const
    {
        return regOffset |
               ((m_chipFamily == GfxIpLevel::GfxIp6) ? 0 : (index << SET_CONTEXT_INDEX_SHIFT));
    }

#if PAL_ENABLE_PRINTS_ASSERTS
    void CheckShadowedContextReg(uint32 regAddr) const;
    void CheckShadowedContextRegs(uint32 startRegAddr, uint32 endRegAddr) const;
    void CheckShadowedShReg(PM4ShaderType shaderType, uint32 regAddr) const;
    void CheckShadowedShRegs(PM4ShaderType shaderType, uint32 startRegAddr, uint32 endRegAddr) const;
    void CheckShadowedUserConfigReg(uint32 regAddr) const;
    void CheckShadowedUserConfigRegs(uint32 startRegAddr, uint32 endRegAddr) const;
#endif

    static uint32 EventIndexFromEventType(VGT_EVENT_TYPE eventType);
    static uint32 CondIbFuncFromCompareType(CompareFunc compareFunc);

    const Pal::Device& m_device;
    const GfxIpLevel   m_chipFamily;
    RegisterInfo       m_registerInfo; // Addresses for registers whose addresses vary between hardware families.

#if PAL_ENABLE_PRINTS_ASSERTS
    // If this is set, PAL will verify that all register writes fall within the ranges which get shadowed to GPU
    // memory when mid command buffer preemption is enabled.
    const bool  m_verifyShadowedRegisters;
#endif

    PAL_DISALLOW_DEFAULT_CTOR(CmdUtil);
    PAL_DISALLOW_COPY_AND_ASSIGN(CmdUtil);
};

} // Gfx6
} // Pal
