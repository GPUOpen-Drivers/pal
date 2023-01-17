/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/pm4ComputeCmdBuffer.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/pipeline.h"
#include <limits.h>

using namespace Util;

namespace Pal
{

namespace Pm4
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
    Pm4::CmdStream*            pCmdStream)
    :
    Pm4CmdBuffer(device, createInfo),
    m_spillTable{},
    m_device(device),
    m_pCmdStream(pCmdStream)
{
    PAL_ASSERT(createInfo.queueType == QueueTypeCompute);

    SwitchCmdSetUserDataFunc(PipelineBindPoint::Compute,  &Pm4CmdBuffer::CmdSetUserDataCs);
    SwitchCmdSetUserDataFunc(PipelineBindPoint::Graphics, &DummyCmdSetUserDataGfx);
}

// =====================================================================================================================
Result ComputeCmdBuffer::Init(
    const CmdBufferInternalCreateInfo& internalInfo)
{
    Result result = Pm4CmdBuffer::Init(internalInfo);

    // Initialize the states for the embedded-data GPU memory table for spilling.
    if (result == Result::Success)
    {
        const auto& chipProps = m_device.Parent()->ChipProperties();

        m_spillTable.stateCs.sizeInDwords = chipProps.gfxip.maxUserDataEntries;
    }

    return result;
}

// =====================================================================================================================
// Resets the command buffer's previous contents and state, then puts it into a building state allowing new commands
// to be recorded.
// Also starts command buffer dumping, if it is enabled.
Result ComputeCmdBuffer::Begin(
    const CmdBufferBuildInfo& info)
{
    const Result result = Pm4CmdBuffer::Begin(info);

    return result;
}

// =====================================================================================================================
// Puts the command stream into a state that is ready for command building.
Result ComputeCmdBuffer::BeginCommandStreams(
    CmdStreamBeginFlags cmdStreamFlags,
    bool                doReset)
{
    Result result = Pm4CmdBuffer::BeginCommandStreams(cmdStreamFlags, doReset);

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
    Result result = Pm4CmdBuffer::End();

    if (result == Result::Success)
    {
        result = m_pCmdStream->End();
    }

    if (result == Result::Success)
    {
#if PAL_ENABLE_PRINTS_ASSERTS
        const Pal::CmdStream* cmdStreams[] = { m_pCmdStream };
        EndCmdBufferDump(cmdStreams, 1);
#endif
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
    Result result = Pm4CmdBuffer::Reset(pCmdAllocator, returnGpuMemory);

    m_pCmdStream->Reset(static_cast<CmdAllocator*>(pCmdAllocator), returnGpuMemory);

    return result;
}

// =====================================================================================================================
// Resets all of the command buffer state tracked. After a reset there should be no state bound.
void ComputeCmdBuffer::ResetState()
{
    Pm4CmdBuffer::ResetState();

    ResetUserDataTable(&m_spillTable.stateCs);
}

#if PAL_ENABLE_PRINTS_ASSERTS
// =====================================================================================================================
// Dumps this command buffer's single command stream to the given file with an appropriate header.
void ComputeCmdBuffer::DumpCmdStreamsToFile(
    File*            pFile,
    CmdBufDumpFormat mode
    ) const
{
    m_pCmdStream->DumpCommands(pFile, "# Compute Queue - Command length = ", mode);
}
#endif

// =====================================================================================================================
CmdStream* ComputeCmdBuffer::GetCmdStreamByEngine(
    uint32 engineType)
{
    return TestAnyFlagSet(m_engineSupport, engineType) ? m_pCmdStream : nullptr;
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

} // Pm4
} // Pal
