/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9ComputePipeline.h"
#include "palAutoBuffer.h"

namespace Pal
{
namespace Gfx9
{

class Device;
class WorkGraph;

// =====================================================================================================================
// GFX9 compute command buffer class: implements GFX9 specific functionality for the ComputeCmdBuffer class.
class ComputeCmdBuffer final : public Pal::ComputeCmdBuffer
{
public:
    ComputeCmdBuffer(const Device& device, const CmdBufferCreateInfo& createInfo);

    static void WritePreambleCommands(const CmdUtil& cmdUtil, CmdStream* pCmdStream);
    static void WritePostambleCommands(
        const CmdUtil&     cmdUtil,
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

    virtual void AddQuery(QueryPoolType queryPoolType, QueryControlFlags flags) override;
    virtual void RemoveQuery(QueryPoolType queryPoolType) override;

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

    virtual void CmdWaitBusAddressableMemoryMarker(
        const IGpuMemory& gpuMemory,
        uint32            data,
        uint32            mask,
        CompareFunc       compareFunc) override;

    virtual void CmdExecuteNestedCmdBuffers(
        uint32            cmdBufferCount,
        ICmdBuffer*const* ppCmdBuffers) override;
    virtual void CmdExecuteIndirectCmds(
        const IIndirectCmdGenerator& generator,
        gpusize                      gpuVirtAddr,
        uint32                       maximumCount,
        gpusize                      countGpuAddr) override;

    virtual void CmdPrimeGpuCaches(
        uint32                    rangeCount,
        const PrimeGpuCacheRange* pRanges) override;
    virtual void CmdCommentString(
        const char* pComment) override;
    virtual void CmdNop(
        const void* pPayload,
        uint32      payloadSize) override;

    virtual void GetChunkForCmdGeneration(
        const Pal::IndirectCmdGenerator& generator,
        const Pal::Pipeline&             pipeline,
        uint32                           maxCommands,
        uint32                           numChunkOutputs,
        ChunkOutput*                     pChunkOutputs) override;

    virtual void CmdSetPredication(
        IQueryPool*         pQueryPool,
        uint32              slot,
        const IGpuMemory*   pGpuMemory,
        gpusize             offset,
        PredicateType       predType,
        bool                predPolarity,
        bool                waitResults,
        bool                accumulateData) override;

    virtual void CmdInsertTraceMarker(PerfTraceMarkerType markerType, uint32 markerData) override;
    virtual void CmdInsertRgpTraceMarker(
        RgpMarkerSubQueueFlags subQueueFlags,
        uint32                 numDwords,
        const void*            pData) override;

    virtual void CmdUpdateSqttTokenMask(const ThreadTraceTokenConfig& sqttTokenConfig) override;

    virtual void CopyMemoryCp(gpusize dstAddr, gpusize srcAddr, gpusize numBytes) override;

    virtual uint32* WriteWaitEop(WriteWaitEopInfo info, uint32* pCmdSpace) override;
    virtual uint32* WriteWaitCsIdle(uint32* pCmdSpace) override;

    //Gets ringSizes from cmdBuffer.
    size_t GetRingSizeComputeScratch() const { return m_ringSizeComputeScratch; }

protected:
    virtual ~ComputeCmdBuffer() {}

    virtual void AddPreamble() override;
    virtual void AddPostamble() override;

    virtual void ResetState() override;

    virtual void WriteEventCmd(const BoundGpuMemory& boundMemObj, uint32 stageMask, uint32 data) override;

private:
    template <bool HsaAbi, bool IssueSqttMarkerEvent, bool DescribeCallback>
    void SetDispatchFunctions();
    void SetDispatchFunctions(bool hsaAbi);

    template <bool HsaAbi, bool IssueSqttMarkerEvent, bool DescribeCallback>
    static void PAL_STDCALL CmdDispatch(
        ICmdBuffer*       pCmdBuffer,
        DispatchDims      size,
        DispatchInfoFlags infoFlags);
    template <bool IssueSqttMarkerEvent, bool DescribeCallback>
    static void PAL_STDCALL CmdDispatchIndirect(
        ICmdBuffer* pCmdBuffer,
        gpusize     gpuVirtAddr
    );
    template <bool HsaAbi, bool IssueSqttMarkerEvent, bool DescribeCallback>
    static void PAL_STDCALL CmdDispatchOffset(
        ICmdBuffer*  pCmdBuffer,
        DispatchDims offset,
        DispatchDims launchSize,
        DispatchDims logicalSize);

    virtual size_t BuildWriteToZero(
        gpusize       dstAddr,
        uint32        numDwords,
        const uint32* pZeros,
        uint32*       pCmdSpace) const override;

    virtual void ActivateQueryType(QueryPoolType queryPoolType) override;
    virtual void DeactivateQueryType(QueryPoolType queryPoolType) override;

    uint32* ValidateDispatchPalAbi(
        gpusize      indirectGpuVirtAddr,
        DispatchDims logicalSize,
        uint32*      pCmdSpace);

    uint32* ValidateDispatchHsaAbi(
        DispatchDims        offset,
        const DispatchDims& logicalSize,
        uint32*             pCmdSpace);

    template <bool HasPipelineChanged>
    uint32* ValidateUserData(
        const ComputePipelineSignature* pPrevSignature,
        UserDataEntries*                pUserData,
        UserDataTableState*             pSpillTable,
        uint32*                         pCmdSpace);

    bool FixupUserSgprsOnPipelineSwitch(
        const UserDataEntries&          userData,
        const ComputePipelineSignature* pPrevSignature,
        uint32**                        ppCmdSpace);

    void LeakNestedCmdBufferState(
        const ComputeCmdBuffer& cmdBuffer);

    bool DisablePartialPreempt() const
    {
        return static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline)->DisablePartialPreempt();
    }

    const Device&   m_device;
    const CmdUtil&  m_cmdUtil;
    const bool      m_issueSqttMarkerEvent;
    bool            m_describeDispatch;
    CmdStream       m_cmdStream;

    // Tracks the user-data signature of the currently active compute pipeline.
    const ComputePipelineSignature*  m_pSignatureCs;

    size_t  m_ringSizeComputeScratch;

    PAL_DISALLOW_DEFAULT_CTOR(ComputeCmdBuffer);
    PAL_DISALLOW_COPY_AND_ASSIGN(ComputeCmdBuffer);
};

} // Gfx9
} // Pal
