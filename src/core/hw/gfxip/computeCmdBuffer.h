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

#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfxCmdStream.h"

namespace Pal
{

// Tracks the state of a user-data table stored in GPU memory.  The table's contents are managed using embedded data
// and the CPU.
struct EmbeddedUserDataTableState
{
    gpusize  gpuVirtAddr;   // GPU virtual address where the current copy of the table data is stored.
    // CPU address of the embedded-data allocation storing the current copy of the table data.  This can be null if
    // the table has not yet been uploaded to embedded data.
    uint32*  pCpuVirtAddr;
    struct
    {
        uint32  sizeInDwords : 31; // Size of one full instance of the user-data table, in DWORD's.
        uint32  dirty        :  1; // Indicates that the CPU copy of the user-data table is more up to date than the
                                   // copy currently in GPU memory and should be updated before the next dispatch.
    };
};

// =====================================================================================================================
// Class for executing basic hardware-specific functionality common to all compute command buffers.
class ComputeCmdBuffer : public GfxCmdBuffer
{
public:
    virtual Result Init(const CmdBufferInternalCreateInfo& internalInfo) override;

    virtual Result Begin(const CmdBufferBuildInfo& info) override;
    virtual Result End() override;
    virtual Result Reset(ICmdAllocator* pCmdAllocator, bool returnGpuMemory) override;

    virtual void CmdBindPipeline(
        const PipelineBindParams& params) override;

    virtual void CmdSetIndirectUserData(
        uint16      tableId,
        uint32      dwordOffset,
        uint32      dwordSize,
        const void* pSrcData) override;
    virtual void CmdSetIndirectUserDataWatermark(
        uint16 tableId,
        uint32 dwordLimit) override;

#if PAL_ENABLE_PRINTS_ASSERTS
    // This function allows us to dump the contents of this command buffer to a file at submission time.
    virtual void DumpCmdStreamsToFile(Util::File* pFile, CmdBufDumpFormat mode) const override;
#endif

    // Returns the number of command streams associated with this command buffer.
    // Compute command buffers will only ever have one command stream.
    virtual uint32 NumCmdStreams() const override { return 1; }

    // Returns a pointer to the command stream specified by "cmdStreamIdx".
    virtual const CmdStream* GetCmdStream(uint32 cmdStreamIdx) const override
    {
        PAL_ASSERT(cmdStreamIdx < NumCmdStreams());
        return m_pCmdStream;
    }

    virtual bool IsQueryAllowed(QueryPoolType queryPoolType) const override
        { return (queryPoolType == QueryPoolType::PipelineStats); }

    // Push/Pop Graphics is never called for compute command buffers.
    virtual void PushGraphicsState() override { PAL_NEVER_CALLED(); }
    virtual void PopGraphicsState()  override { PAL_NEVER_CALLED(); }

    virtual void CmdOverwriteRbPlusFormatForBlits(
        SwizzledFormat format,
        uint32         targetIndex) override { PAL_NEVER_CALLED(); }

    // Returns a pointer to the command stream associated with the specified engine type
    virtual CmdStream* GetCmdStreamByEngine(uint32 engineType) override;

    // Increments the submit-count of the command stream(s) contained in this command buffer.
    virtual void IncrementSubmitCount() override
        { m_pCmdStream->IncrementSubmitCount(); }

protected:
    ComputeCmdBuffer(
        const GfxDevice&           device,
        const CmdBufferCreateInfo& createInfo,
        PrefetchMgr*               pPrefetchMgr,
        GfxCmdStream*              pCmdStream);

    virtual ~ComputeCmdBuffer() {}

    virtual Pal::PipelineState* PipelineState(PipelineBindPoint bindPoint) override
    {
        PAL_ASSERT(bindPoint == PipelineBindPoint::Compute);
        return &m_computeState.pipelineState;
    }

    virtual Result BeginCommandStreams(CmdStreamBeginFlags cmdStreamFlags, bool doReset) override;

    virtual void ResetState() override;

    void UpdateUserDataTable(
        EmbeddedUserDataTableState* pTable,
        uint32                      dwordsNeeded,
        uint32                      offsetInDwords,
        const uint32*               pSrcData);

    void LeakNestedCmdBufferState(
        const ComputeCmdBuffer& cmdBuffer);

    virtual void P2pBltWaCopyNextRegion(gpusize chunkAddr) override
        { CmdBuffer::P2pBltWaCopyNextRegion(m_pCmdStream, chunkAddr); }
    virtual uint32* WriteNops(uint32* pCmdSpace, uint32 numDwords) const override
        { return pCmdSpace + m_pCmdStream->BuildNop(numDwords, pCmdSpace); }

    struct
    {
        // Client-specified high-watermark for each indirect user-data table.  This indicates how much of each
        // table is uploaded to embedded data before a dispatch.
        uint32                      watermark;
        uint32*                     pData; // Tracks the contents of each indirect user-data table.
        EmbeddedUserDataTableState  state; // Tracks the state of the indirect user-date table in GPU memory.
    }  m_indirectUserDataInfo[MaxIndirectUserDataTables];

    EmbeddedUserDataTableState  m_spillTableCs; // Tracks the state of the compute user-data spill table.

private:
    const GfxDevice&    m_device;
    GfxCmdStream*const  m_pCmdStream;

    PAL_DISALLOW_COPY_AND_ASSIGN(ComputeCmdBuffer);
    PAL_DISALLOW_DEFAULT_CTOR(ComputeCmdBuffer);
};

} // Pal
