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

#include "core/hw/gfxip/pm4CmdStream.h"
#include "core/hw/gfxip/pm4CmdBuffer.h"

namespace Pal
{

namespace Pm4
{
class BarrierMgr;

// =====================================================================================================================
// Class for executing basic hardware-specific functionality common to all pm4 based compute command buffers.
class ComputeCmdBuffer : public Pm4CmdBuffer
{
public:
    virtual Result Init(const CmdBufferInternalCreateInfo& internalInfo) override;

    virtual Result End() override;
    virtual Result Reset(ICmdAllocator* pCmdAllocator, bool returnGpuMemory) override;

    // This function allows us to dump the contents of this command buffer to a file at submission time.
    virtual void DumpCmdStreamsToFile(Util::File* pFile, CmdBufDumpFormat mode) const override;

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

    virtual void CmdOverwriteColorExportInfoForBlits(
        SwizzledFormat format,
        uint32         targetIndex) override { PAL_NEVER_CALLED(); }

    // Returns a pointer to the command stream associated with the specified engine type
    virtual CmdStream* GetCmdStreamByEngine(CmdBufferEngineSupport engineType) override;

    // Increments the submit-count of the command stream(s) contained in this command buffer.
    virtual void IncrementSubmitCount() override
        { m_pCmdStream->IncrementSubmitCount(); }

    virtual uint32 GetUsedSize(CmdAllocType type) const override;

protected:
    ComputeCmdBuffer(
        const GfxDevice&           device,
        const CmdBufferCreateInfo& createInfo,
        const GfxBarrierMgr*       pBarrierMgr,
        Pm4::CmdStream*            pCmdStream,
        bool                       useUpdateUserData);

    virtual ~ComputeCmdBuffer() {}

    virtual Result BeginCommandStreams(CmdStreamBeginFlags cmdStreamFlags, bool doReset) override;

    virtual void ResetState() override;

    void LeakNestedCmdBufferState(
        const ComputeCmdBuffer& cmdBuffer);

    virtual uint32* WriteNops(uint32* pCmdSpace, uint32 numDwords) const override
        { return pCmdSpace + m_pCmdStream->BuildNop(numDwords, pCmdSpace); }

    struct
    {
        UserDataTableState  stateCs;  // Tracks the state of the compute spill table
    }  m_spillTable;

private:
    const GfxDevice&     m_device;
    Pm4::CmdStream*const m_pCmdStream;

    PAL_DISALLOW_COPY_AND_ASSIGN(ComputeCmdBuffer);
    PAL_DISALLOW_DEFAULT_CTOR(ComputeCmdBuffer);
};

} // Pm4
} // Pal
