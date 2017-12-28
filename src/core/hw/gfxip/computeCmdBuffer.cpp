/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palShader.h"
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
    PrefetchMgr*               pPrefetchMgr,
    GfxCmdStream*              pCmdStream)
    :
    GfxCmdBuffer(device, createInfo, pPrefetchMgr, pCmdStream),
    m_device(device),
    m_pCmdStream(pCmdStream)
{
    PAL_ASSERT(createInfo.queueType == QueueTypeCompute);

    memset(&m_computeState,        0, sizeof(m_computeState));
    memset(&m_computeRestoreState, 0, sizeof(m_computeRestoreState));

    SwitchCmdSetUserDataFunc(PipelineBindPoint::Compute,  &GfxCmdBuffer::CmdSetUserDataCs);
    SwitchCmdSetUserDataFunc(PipelineBindPoint::Graphics, &DummyCmdSetUserDataGfx);
}

// =====================================================================================================================
// Resets the command buffer's previous contents and state, then puts it into a building state allowing new commands
// to be recorded.
// Also starts command buffer dumping, if it is enabled.
Result ComputeCmdBuffer::Begin(
    const CmdBufferBuildInfo& info)
{
    const Result result = GfxCmdBuffer::Begin(info);

#if PAL_ENABLE_PRINTS_ASSERTS
    if ((result == Result::Success) && (IsDumpingEnabled()))
    {
        char filename[MaxFilenameLength] = {};

        // filename is:  computexx_yyyyy, where "xx" is the number of compute command buffers that have been created so
        //               far (one based) and "yyyyy" is the number of times this command buffer has been begun (also
        //               one based).
        //
        // All streams associated with this command buffer are included in this one file.
        Snprintf(filename, MaxFilenameLength, "compute%02d_%05d", UniqueId(), NumBegun());
        OpenCmdBufDumpFile(&filename[0]);
    }
#endif

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
#if PAL_ENABLE_PRINTS_ASSERTS
        if (IsDumpingEnabled() && DumpFile()->IsOpen())
        {
            if (m_device.Parent()->Settings().submitTimeCmdBufDumpMode == CmdBufDumpModeBinaryHeaders)
            {
                const CmdBufferDumpFileHeader fileHeader =
                {
                    static_cast<uint32>(sizeof(CmdBufferDumpFileHeader)), // Structure size
                    1,                                                    // Header version
                    m_device.Parent()->ChipProperties().familyId,         // ASIC family
                    m_device.Parent()->ChipProperties().deviceId,         // Reserved, but use for PCI device ID
                    0                                                     // Reserved
                };
                DumpFile()->Write(&fileHeader, sizeof(fileHeader));

                CmdBufferListHeader listHeader =
                {
                    static_cast<uint32>(sizeof(CmdBufferListHeader)),   // Structure size
                    0,                                                  // Engine index
                    0                                                   // Number of command buffer chunks
                };

                listHeader.count = m_pCmdStream->GetNumChunks();

                DumpFile()->Write(&listHeader, sizeof(listHeader));
            }

            DumpCmdStreamsToFile(DumpFile(), m_device.Parent()->Settings().submitTimeCmdBufDumpMode);
            DumpFile()->Close();
        }
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
    Result result = GfxCmdBuffer::Reset(pCmdAllocator, returnGpuMemory);

    m_pCmdStream->Reset(static_cast<CmdAllocator*>(pCmdAllocator), returnGpuMemory);

    return result;
}

// =====================================================================================================================
// Resets all of the command buffer state tracked. After a reset there should be no state bound.
void ComputeCmdBuffer::ResetState()
{
    GfxCmdBuffer::ResetState();

    memset(&m_computeState,        0, sizeof(m_computeState));
    memset(&m_computeRestoreState, 0, sizeof(m_computeRestoreState));
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdBindPipeline(
    const PipelineBindParams& params)
{
    PAL_ASSERT(params.pipelineBindPoint == PipelineBindPoint::Compute);

    m_computeState.pipelineState.pPipeline = static_cast<const Pipeline*>(params.pPipeline);
    m_computeState.pipelineState.dirtyFlags.pipelineDirty = 1;

    m_computeState.dynamicCsInfo = params.cs;
}

#if PAL_ENABLE_PRINTS_ASSERTS
// =====================================================================================================================
// Dumps this command buffer's single command stream to the given file with an appropriate header.
void ComputeCmdBuffer::DumpCmdStreamsToFile(
    File*          pFile,
    CmdBufDumpMode mode
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

} // Pal
