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

#include "core/hw/gfxip/computeCmdBuffer.h"
#include "core/hw/gfxip/gfx9/gfx9Gds.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9PrefetchMgr.h"
#include "core/hw/gfxip/gfx9/gfx9UserDataTable.h"

namespace Pal
{
namespace Gfx9
{

class Device;

// =====================================================================================================================
// GFX9 compute command buffer class: implements GFX9 specific functionality for the ComputeCmdBuffer class.
class ComputeCmdBuffer : public Pal::ComputeCmdBuffer
{
public:
    static size_t GetSize(const Device& device);

    ComputeCmdBuffer(const Device& device, const CmdBufferCreateInfo& createInfo);

    virtual Result Init(const CmdBufferInternalCreateInfo& internalInfo) override;

    virtual void CmdBindPipeline(
        const PipelineBindParams& params) override;

    virtual void CmdBarrier(const BarrierInfo& barrierInfo) override;

    virtual void CmdSetIndirectUserData(
        uint16      tableId,
        uint32      dwordOffset,
        uint32      dwordSize,
        const void* pSrcData) override;

    virtual void CmdSetIndirectUserDataWatermark(
        uint16 tableId,
        uint32 dwordLimit) override;

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
        uint32            value) override;

    virtual void CmdMemoryAtomic(
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset,
        uint64            srcData,
        AtomicOp          atomicOp) override;

    virtual void CmdWriteTimestamp(HwPipePoint pipePoint, const IGpuMemory& dstGpuMemory, gpusize dstOffset) override;

    virtual void CmdWriteImmediate(
        HwPipePoint        pipePoint,
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

    virtual void CmdExecuteNestedCmdBuffers(
        uint32            cmdBufferCount,
        ICmdBuffer*const* ppCmdBuffers) override;

    virtual void CmdCommentString(
        const char* pComment) override;

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

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 311
    virtual void CmdSetPredication(
        IQueryPool*         pQueryPool,
        uint32              slot,
        const IGpuMemory*   pGpuMemory,
        gpusize             offset,
        PredicateType       predType,
        bool                predPolarity,
        bool                waitResults,
        bool                accumulateData) override;
#else
    virtual void CmdSetPredication(
        IQueryPool*   pQueryPool,
        uint32        slot,
        gpusize       gpuVirtAddr,
        PredicateType predType,
        bool          predPolarity,
        bool          waitResults,
        bool          accumulateData) override;
#endif

    virtual void CmdInsertRgpTraceMarker(
        uint32      numDwords,
        const void* pData) override;

    virtual void AddPerPresentCommands(
        gpusize frameCountGpuAddr,
        uint32  frameCntReg) override;

protected:
    virtual ~ComputeCmdBuffer() {}

    virtual Result AddPreamble() override;
    virtual Result AddPostamble() override;

    virtual void ResetState() override;

    virtual void WriteEventCmd(const BoundGpuMemory& boundMemObj, HwPipePoint pipePoint, uint32 data) override;

    virtual void InheritStateFromCmdBuf(const GfxCmdBuffer* pCmdBuffer) override;

    void ValidateDispatch(gpusize gpuVirtAddrNumTgs);

private:
    static void PAL_STDCALL CmdSetUserDataCs(
        ICmdBuffer*   pCmdBuffer,
        uint32        firstEntry,
        uint32        entryCount,
        const uint32* pEntryValues);

    template <bool issueSqttMarkerEvent>
    static void PAL_STDCALL CmdDispatch(
        ICmdBuffer* pCmdBuffer,
        uint32      x,
        uint32      y,
        uint32      z);
    template <bool issueSqttMarkerEvent>
    static void PAL_STDCALL CmdDispatchIndirect(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset);
    template <bool issueSqttMarkerEvent>
    static void PAL_STDCALL CmdDispatchOffset(
        ICmdBuffer* pCmdBuffer,
        uint32      xOffset,
        uint32      yOffset,
        uint32      zOffset,
        uint32      xDim,
        uint32      yDim,
        uint32      zDim);

    virtual void ActivateQueryType(QueryPoolType queryPoolType) override;
    virtual void DeactivateQueryType(QueryPoolType queryPoolType) override;

    void LeakNestedCmdBufferState(
        const ComputeCmdBuffer& cmdBuffer);

    /* Helper methods for managing generic embedded-data user-data tables: */

    uint32* UpdateUserDataTableAddressses(
        uint32* pCmdSpace);

    const Device&  m_device;
    const CmdUtil& m_cmdUtil;

    // Prefetch manager is for pre-loading / warming L2 caches on behalf of the command buffer
    PrefetchMgr    m_prefetchMgr;
    CmdStream      m_cmdStream;

    // Tracks the user-data signature of the currently active compute pipeline.
    const ComputePipelineSignature*  m_pSignatureCs;

    union
    {
        struct
        {
            uint32  reserved : 32;
        } bits;

        uint32   u32All;
    } m_flags;

    struct
    {
        // Client-specified high-watermark for each indirect user-data table. This indicates how much of each table
        // is dumped from CE RAM to memory before a draw or dispatch.
        uint32  watermark;
        uint32* pData;  // Tracks the contents of each indirect user-data table.

        UserDataTableState  state;  // Tracks the state for the indirect user-data table

    }  m_indirectUserDataInfo[MaxIndirectUserDataTables];

    UserDataTableState  m_spillTableCs;  // Tracks the sate for the compute spill table

    // SET_PREDICATION is not supported on compute queue so what we work out here is an emulation using cond exec
    // Note m_gfxCmdBuff.clientPredicate and m_gfxCmdBuff.packetPredicate bits are 0 when:
    //     - app disables/resets predication
    //     - driver forces them to 0 when a new command buffer begins
    // Note m_gfxCmdBuff.packetPredicate is also temporarily overridden by the driver during some operations
    //
    gpusize  m_predGpuAddr;

    PAL_DISALLOW_DEFAULT_CTOR(ComputeCmdBuffer);
    PAL_DISALLOW_COPY_AND_ASSIGN(ComputeCmdBuffer);
};

} // Gfx9
} // Pal
