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

#include "core/cmdAllocator.h"
#include "core/device.h"
#include "core/hw/gfxip/computeCmdBuffer.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/pipeline.h"
#include <limits.h>

using namespace Util;

namespace Pal
{

// =====================================================================================================================
// Dummy function for catching illegal attempts to set graphics user-data entries on a Compute command buffer.
static void PAL_STDCALL DummyCmdSetUserDataGfx(
    ICmdBuffer*   pCmdBuffer,
    uint32        firstEntry,
    uint32        entryCount,
    const uint32* pEntryValues)
{
    PAL_ASSERT_ALWAYS();
}

// =====================================================================================================================
ComputeCmdBuffer::ComputeCmdBuffer(
    const GfxDevice&           device,
    const CmdBufferCreateInfo& createInfo,
    const GfxBarrierMgr&       barrierMgr,
    GfxCmdStream*              pCmdStream,
    bool                       useUpdateUserData)
    :
    GfxCmdBuffer(device, createInfo, pCmdStream, barrierMgr, false),
    m_spillTable{}
{
    PAL_ASSERT(createInfo.queueType == QueueTypeCompute);

    SwitchCmdSetUserDataFunc(PipelineBindPoint::Compute, useUpdateUserData ? &GfxCmdBuffer::CmdUpdateUserDataCs
                                                                           : &GfxCmdBuffer::CmdSetUserDataCs);
    SwitchCmdSetUserDataFunc(PipelineBindPoint::Graphics, &DummyCmdSetUserDataGfx);
}

// =====================================================================================================================
Result ComputeCmdBuffer::Init(
    const CmdBufferInternalCreateInfo& internalInfo)
{
    Result result = GfxCmdBuffer::Init(internalInfo);

    // Initialize the states for the embedded-data GPU memory table for spilling.
    if (result == Result::Success)
    {
        const auto& chipProps = m_device.Parent()->ChipProperties();

        m_spillTable.stateCs.sizeInDwords = chipProps.gfxip.maxUserDataEntries;
    }

    return result;
}

// =====================================================================================================================
// Puts the command stream into a state that is ready for command building.
Result ComputeCmdBuffer::BeginCommandStreams(
    CmdStreamBeginFlags cmdStreamFlags,
    bool                doReset)
{
    Result result = GfxCmdBuffer::BeginCommandStreams(cmdStreamFlags, doReset);

    if (doReset)
    {
        m_pCmdStream->Reset(nullptr, true);
    }

    if (result == Result::Success)
    {
        result = m_pCmdStream->Begin(cmdStreamFlags, m_pMemAllocator);
    }

    return result;
}

// =====================================================================================================================
// Completes recording of a command buffer in the building state, making it executable.
// Also ends command buffer dumping, if it is enabled.
Result ComputeCmdBuffer::End()
{
    Result result = GfxCmdBuffer::End();

    if (result == Result::Success)
    {
        result = m_pCmdStream->End();
    }

    if (result == Result::Success)
    {
        const Pal::CmdStream* cmdStreams[] = { m_pCmdStream };
        EndCmdBufferDump(cmdStreams, 1);
    }

    return result;
}

// =====================================================================================================================
// Explicitly resets a command buffer, releasing any internal resources associated with it and putting it in the reset
// state.
Result ComputeCmdBuffer::Reset(
    ICmdAllocator* pCmdAllocator,
    bool           returnGpuMemory)
{
    Result result = GfxCmdBuffer::Reset(pCmdAllocator, returnGpuMemory);

    m_pCmdStream->Reset(static_cast<CmdAllocator*>(pCmdAllocator), returnGpuMemory);

    return result;
}

// =====================================================================================================================
// Resets all of the command buffer state tracked. After a reset there should be no state bound.
void ComputeCmdBuffer::ResetState()
{
    GfxCmdBuffer::ResetState();

    ResetUserDataTable(&m_spillTable.stateCs);
}

// =====================================================================================================================
// Dumps this command buffer's single command stream to the given file with an appropriate header.
void ComputeCmdBuffer::DumpCmdStreamsToFile(
    File*            pFile,
    CmdBufDumpFormat mode
    ) const
{
    m_pCmdStream->DumpCommands(pFile, "# Compute Queue - Command length = ", mode);
}

// =====================================================================================================================
// Helper method for handling the state "leakage" from a nested command buffer back to its caller. Since the callee has
// tracked its own state during the building phase, we can access the final state of the command buffer since its stored
// in the ComputeCmdBuffer object itself.
void ComputeCmdBuffer::LeakNestedCmdBufferState(
    const ComputeCmdBuffer& cmdBuffer) // [in] Nested command buffer whose state we're absorbing.
{
    LeakPerPipelineStateChanges(cmdBuffer.m_computeState.pipelineState,
                                cmdBuffer.m_computeState.csUserDataEntries,
                                &m_computeState.pipelineState,
                                &m_computeState.csUserDataEntries);

    // It is possible that nested command buffer execute operation which affect the data in the primary buffer
    m_cmdBufState.flags.csBltActive               = cmdBuffer.m_cmdBufState.flags.csBltActive;
    m_cmdBufState.flags.cpBltActive               = cmdBuffer.m_cmdBufState.flags.cpBltActive;
    m_cmdBufState.flags.csWriteCachesDirty        = cmdBuffer.m_cmdBufState.flags.csWriteCachesDirty;
    m_cmdBufState.flags.cpWriteCachesDirty        = cmdBuffer.m_cmdBufState.flags.cpWriteCachesDirty;
    m_cmdBufState.flags.cpMemoryWriteL2CacheStale = cmdBuffer.m_cmdBufState.flags.cpMemoryWriteL2CacheStale;

    // NOTE: Compute command buffers shouldn't have changed either of their CmdSetUserData callbacks.
    PAL_ASSERT(memcmp(&m_funcTable, &cmdBuffer.m_funcTable, sizeof(m_funcTable)) == 0);
}

// =====================================================================================================================
uint32 ComputeCmdBuffer::GetUsedSize(
    CmdAllocType type
    ) const
{
    uint32 sizeInBytes = GfxCmdBuffer::GetUsedSize(type);

    if (type == CommandDataAlloc)
    {
        sizeInBytes += m_pCmdStream->GetUsedCmdMemorySize();
    }

    return sizeInBytes;
}

} // Pal
