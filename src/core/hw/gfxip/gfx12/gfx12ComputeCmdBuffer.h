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

#include "core/hw/gfxip/computeCmdBuffer.h"
#include "core/hw/gfxip/gfx12/gfx12Chip.h"
#include "core/hw/gfxip/gfx12/gfx12CmdStream.h"
#include "core/hw/gfxip/gfx12/gfx12ComputePipeline.h"
#include "core/hw/gfxip/gfx12/gfx12UserDataLayout.h"

namespace Pal
{
namespace Gfx12
{

class CmdUtil;
class ComputeUserDataLayout;
class Device;
class IndirectCmdGenerator;
class RsrcProcMgr;

struct ComputeCmdBufferDeviceConfig
{
    uint32 disableBorderColorPaletteBinds :  1;
    uint32 enablePreamblePipelineStats    :  1;
#if PAL_DEVELOPER_BUILD
    uint32 enablePm4Instrumentation       :  1; // If detailed PM4 instrumentation is enabled.
#else
    uint32 reserved0                      :  1;
#endif
    uint32 issueSqttMarkerEvent           :  1;
    uint32 enableReleaseMemWaitCpDma      :  1;
    uint32 reserved                       : 27;

    gpusize prefetchClampSize;
};

// =====================================================================================================================
// PM4-based compute command buffer class.  Translates PAL command buffer calls into lower-level PM4 packets.  Common
// implementation is shared by all supported hardware.
class ComputeCmdBuffer final : public Pal::ComputeCmdBuffer
{
public:
    ComputeCmdBuffer(
        const Device&                       device,
        const CmdBufferCreateInfo&          createInfo,
        const ComputeCmdBufferDeviceConfig& deviceConfig);

    static Result WritePreambleCommands(
        const ComputeCmdBufferDeviceConfig& deviceConfig,
        CmdStream*                          pCmdStream);
    static void WritePostambleCommands(
        GfxCmdBuffer*const pCmdBuffer,
        CmdStream*         pCmdStream);

    virtual Result Init(const CmdBufferInternalCreateInfo& internalInfo) override;

    virtual void CmdBindPipeline(const PipelineBindParams& params) override;

    virtual void CmdUpdateBusAddressableMemoryMarker(
        const IGpuMemory& dstGpuMemory,
        gpusize           offset,
        uint32            value) override;

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
        uint64             value,
        ImmediateDataWidth dataSize,
        gpusize            address) override;

    virtual void CmdBindBorderColorPalette(
        PipelineBindPoint          pipelineBindPoint,
        const IBorderColorPalette* pPalette) override;

    virtual void CmdBeginQuery(
        const IQueryPool& queryPool,
        QueryType         queryType,
        uint32            slot,
        QueryControlFlags flags) override;

    virtual void CmdEndQuery(const IQueryPool& queryPool, QueryType queryType, uint32 slot) override;

    virtual void CmdResetQueryPool(
        const IQueryPool& queryPool,
        uint32            startQuery,
        uint32            queryCount) override;

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

    virtual void CmdCopyRegisterToMemory(
        uint32            srcRegisterOffset,
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset) override;

    virtual void CmdWaitMemoryValue(
        gpusize     gpuVirtAddr,
        uint32      data,
        uint32      mask,
        CompareFunc compareFunc) override;

    virtual void CmdWaitBusAddressableMemoryMarker(
        const IGpuMemory& gpuMemory,
        uint32            data,
        uint32            mask,
        CompareFunc       compareFunc) override;

    virtual void CmdExecuteNestedCmdBuffers(
        uint32             cmdBufferCount,
        ICmdBuffer* const* ppCmdBuffers) override;

    void ValidateExecuteIndirect(
        const IndirectCmdGenerator& gfx12Generator);

    void PreprocessExecuteIndirect(
        const IndirectCmdGenerator& generator,
        const ComputePipeline*      pCsPipeline,
        ExecuteIndirectPacketInfo*  pPacketInfo,
        ExecuteIndirectMeta*        pMeta,
        const EiDispatchOptions&    options);

    void ExecuteIndirectPacket(
        const IIndirectCmdGenerator& generator,
        const gpusize                gpuVirtAddr,
        const uint32                 maximumCount,
        const gpusize                countGpuAddr);

    virtual void CmdExecuteIndirectCmds(
        const IIndirectCmdGenerator& generator,
        gpusize                      gpuVirtAddr,
        uint32                       maximumCount,
        gpusize                      countGpuAddr) override;

    virtual void CmdPrimeGpuCaches(
        uint32                    rangeCount,
        const PrimeGpuCacheRange* pRanges) override;

    virtual void CmdCommentString(const char* pComment) override;

    virtual void CmdSetPredication(
        IQueryPool*       pQueryPool,
        uint32            slot,
        const IGpuMemory* pGpuMemory,
        gpusize           offset,
        PredicateType     predType,
        bool              predPolarity,
        bool              waitResults,
        bool              accumulateData) override;

    virtual void CmdInsertTraceMarker(PerfTraceMarkerType markerType, uint32 markerData) override;

    virtual void CmdInsertRgpTraceMarker(
        RgpMarkerSubQueueFlags subQueueFlags,
        uint32                 numDwords,
        const void*            pData) override;

    virtual void CmdUpdateSqttTokenMask(const ThreadTraceTokenConfig& sqttTokenConfig) override;

    // This function allows us to dump the contents of this command buffer to a file at submission time.
    virtual void DumpCmdStreamsToFile(Util::File* pFile, CmdBufDumpFormat mode) const override;

    virtual void AddQuery(QueryPoolType queryPoolType, QueryControlFlags flags) override;

    virtual void RemoveQuery(QueryPoolType queryPoolType) override;

    virtual void CopyMemoryCp(gpusize dstAddr, gpusize srcAddr, gpusize numBytes) override;

    virtual void CmdNop(const void* pPayload, uint32 payloadSize) override;

    virtual uint32* WriteWaitEop(WriteWaitEopInfo info, uint32* pCmdSpace) override;
    virtual uint32* WriteWaitCsIdle(uint32* pCmdSpace) override;

    size_t GetRingSizeComputeScratch() const { return m_ringSizeComputeScratch; }

protected:
    virtual ~ComputeCmdBuffer() {}
    virtual void ResetState() override;

    virtual void AddPreamble() override;
    virtual void AddPostamble() override;

    virtual void WriteEventCmd(const BoundGpuMemory& boundMemObj, uint32 stageMask, uint32 data) override;

private:

    template<bool HsaAbi,
             bool IssueSqtt,
             bool DescribeCallback>
    void SetDispatchFunctions();

    void SetDispatchFunctions(bool hsaAbi);

    template<bool HsaAbi, bool IssueSqtt, bool DescribeCallback>
    static void PAL_STDCALL CmdDispatch(
        ICmdBuffer*       pCmdBuffer,
        DispatchDims      size,
        DispatchInfoFlags infoFlags);

    template<bool IssueSqtt, bool DescribeCallback>
    static void PAL_STDCALL CmdDispatchIndirect(
        ICmdBuffer* pCmdBuffer,
        gpusize     gpuVirtAddr);

    template<bool HsaAbi, bool IssueSqtt, bool DescribeCallback>
    static void PAL_STDCALL CmdDispatchOffset(
        ICmdBuffer* pCmdBuffer,
        DispatchDims offset,
        DispatchDims launchSize,
        DispatchDims logicalSize);

    virtual void ActivateQueryType(QueryPoolType queryPoolType) override;
    virtual void DeactivateQueryType(QueryPoolType queryPoolType) override;

    const ComputeCmdBufferDeviceConfig m_deviceConfig;
    const Device&                      m_device;
    const CmdUtil&                     m_cmdUtil;
    const RsrcProcMgr&                 m_rsrcProcMgr;
    const ComputeUserDataLayout*       m_pPrevComputeUserDataLayoutValidatedWith;
    CmdStream                          m_cmdStream;
    bool                               m_describeDispatch;

    template <bool HasPipelineChanged>
    uint32* ValidateComputeUserData(
        UserDataEntries*             pUserData,
        UserDataTableState*          pSpillTable,
        const ComputeUserDataLayout* pCurrentComputeUserDataLayout,
        const ComputeUserDataLayout* pPrevComputeUserDataLayout,
        gpusize                      indirectGpuVirtAddr,
        DispatchDims                 logicalSize,
        uint32*                      pCmdSpace);

    uint32* ValidateDispatchPalAbi(
        gpusize      indirectGpuVirtAddr,
        DispatchDims logicalSize,
        uint32*      pCmdSpace);

    uint32* ValidateDispatchHsaAbi(
        DispatchDims        offset,
        const DispatchDims& logicalSize,
        uint32*             pCmdSpace);

    void LeakNestedCmdBufferState(const ComputeCmdBuffer& cmdBuffer);

    size_t m_ringSizeComputeScratch;

    PAL_DISALLOW_DEFAULT_CTOR(ComputeCmdBuffer);
    PAL_DISALLOW_COPY_AND_ASSIGN(ComputeCmdBuffer);
};

} // Gfx12
} // Pal
