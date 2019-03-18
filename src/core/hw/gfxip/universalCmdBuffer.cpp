/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/borderColorPalette.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/graphicsPipeline.h"
#include "core/hw/gfxip/pipeline.h"
#include "core/hw/gfxip/universalCmdBuffer.h"
#include "core/gpuMemory.h"
#include "core/perfExperiment.h"
#include "core/platform.h"
#include "palDequeImpl.h"
#include "palMath.h"
#include <limits.h>

using namespace Util;

namespace Pal
{

// =====================================================================================================================
UniversalCmdBuffer::UniversalCmdBuffer(
    const GfxDevice&           device,
    const CmdBufferCreateInfo& createInfo,
    GfxCmdStream*              pDeCmdStream,
    GfxCmdStream*              pCeCmdStream,
    bool                       blendOptEnable)
    :
    GfxCmdBuffer(device, createInfo),
    m_device(device),
    m_pDeCmdStream(pDeCmdStream),
    m_pCeCmdStream(pCeCmdStream),
    m_blendOptEnable(blendOptEnable)
#if PAL_ENABLE_PRINTS_ASSERTS
    , m_graphicsStateIsPushed(false)
#endif
{
    PAL_ASSERT(createInfo.queueType == QueueTypeUniversal);

    memset(&m_computeState,         0, sizeof(m_computeState));
    memset(&m_computeRestoreState,  0, sizeof(m_computeRestoreState));
    memset(&m_graphicsState,        0, sizeof(m_graphicsState));
    memset(&m_graphicsRestoreState, 0, sizeof(m_graphicsRestoreState));
    memset(&m_blendOpts[0],         0, sizeof(m_blendOpts));

    SwitchCmdSetUserDataFunc(PipelineBindPoint::Compute,  &GfxCmdBuffer::CmdSetUserDataCs);
    SwitchCmdSetUserDataFunc(PipelineBindPoint::Graphics, &CmdSetUserDataGfx<true>);
}

// =====================================================================================================================
// Resets the command buffer's previous contents and state, then puts it into a building state allowing new commands
// to be recorded.
// Also starts command buffer dumping, if it is enabled.
Result UniversalCmdBuffer::Begin(
    const CmdBufferBuildInfo& info)
{
    Result result = GfxCmdBuffer::Begin(info);

    if (info.pInheritedState != nullptr)
    {
        m_graphicsState.inheritedState = *(info.pInheritedState);
    }

#if PAL_ENABLE_PRINTS_ASSERTS
    if ((result == Result::Success) && (IsDumpingEnabled()))
    {
        char filename[MaxFilenameLength] = {};

        // filename is:  computexx_yyyyy, where "xx" is the number of universal command buffers that have been created
        //               so far (one based) and "yyyyy" is the number of times this command buffer has been begun (also
        //               one based).
        //
        // All streams associated with this command buffer are included in this one file.
        Snprintf(filename, MaxFilenameLength, "universal%02d_%05d", UniqueId(), NumBegun());
        OpenCmdBufDumpFile(&filename[0]);
    }
#endif

    return result;
}

// =====================================================================================================================
// Puts the command streams into a state that is ready for command building.
Result UniversalCmdBuffer::BeginCommandStreams(
    CmdStreamBeginFlags cmdStreamFlags,
    bool                doReset)
{
    Result result = GfxCmdBuffer::BeginCommandStreams(cmdStreamFlags, doReset);

    if (doReset)
    {
        m_pDeCmdStream->Reset(nullptr, true);
        m_pCeCmdStream->Reset(nullptr, true);
    }

    if (result == Result::Success)
    {
        result = m_pDeCmdStream->Begin(cmdStreamFlags, m_pMemAllocator);
    }

    if (result == Result::Success)
    {
        result = m_pCeCmdStream->Begin(cmdStreamFlags, m_pMemAllocator);
    }

    return result;
}

// =====================================================================================================================
// Completes recording of a command buffer in the building state, making it executable.
// Also ends command buffer dumping, if it is enabled.
Result UniversalCmdBuffer::End()
{
    // Amoung other things, this will add the postamble.  Be sure to add this before ending the command streams so that
    // they get padded correctly.
    Result result = GfxCmdBuffer::End();

    if (result == Result::Success)
    {
        result = m_pDeCmdStream->End();
    }

    if (result == Result::Success)
    {
        result = m_pCeCmdStream->End();
    }

    if (result == Result::Success)
    {

        m_graphicsState.leakFlags.u32All |= m_graphicsState.dirtyFlags.u32All;

#if PAL_ENABLE_PRINTS_ASSERTS
        if (IsDumpingEnabled() && DumpFile()->IsOpen())
        {
            if (m_device.Parent()->Settings().cmdBufDumpFormat == CmdBufDumpFormatBinaryHeaders)
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

                listHeader.count = m_pDeCmdStream->GetNumChunks() + m_pCeCmdStream->GetNumChunks();

                DumpFile()->Write(&listHeader, sizeof(listHeader));
            }

            DumpCmdStreamsToFile(DumpFile(), m_device.Parent()->Settings().cmdBufDumpFormat);
            DumpFile()->Close();
        }
#endif
    }

    return result;
}

// =====================================================================================================================
// Explicitly resets a command buffer, releasing any internal resources associated with it and putting it in the reset
// state.
Result UniversalCmdBuffer::Reset(
    ICmdAllocator* pCmdAllocator,
    bool           returnGpuMemory)
{
    Result result = GfxCmdBuffer::Reset(pCmdAllocator, returnGpuMemory);

    if (result == Result::Success)
    {
        m_pDeCmdStream->Reset(static_cast<CmdAllocator*>(pCmdAllocator), returnGpuMemory);
        m_pCeCmdStream->Reset(static_cast<CmdAllocator*>(pCmdAllocator), returnGpuMemory);
    }

    // Command buffers initialize blend opts to default based on setting.
    // This must match default settings in Pal::ColorTargetView
    for (uint32 idx = 0; idx < MaxColorTargets; ++idx)
    {
        if (m_blendOptEnable)
        {
            m_blendOpts[idx].dontRdDst    = GfxBlendOptimizer::BlendOpt::ForceOptAuto;
            m_blendOpts[idx].discardPixel = GfxBlendOptimizer::BlendOpt::ForceOptAuto;
        }
        else
        {
            m_blendOpts[idx].dontRdDst    = GfxBlendOptimizer::BlendOpt::ForceOptDisable;
            m_blendOpts[idx].discardPixel = GfxBlendOptimizer::BlendOpt::ForceOptDisable;
        }
    }

    PAL_ASSERT(result == Result::Success);
    return result;
}

// =====================================================================================================================
// Resets all of the state tracked by this command buffer
void UniversalCmdBuffer::ResetState()
{
    GfxCmdBuffer::ResetState();

    memset(&m_computeState,  0, sizeof(m_computeState));
    memset(&m_graphicsState, 0, sizeof(m_graphicsState));

    // Clear the pointer to the performance experiment object currently used by this command buffer.
    m_pCurrentExperiment = nullptr;

    // NULL color target will only be bound if the slot was not NULL and is being set to NULL. Use a value of all 1s
    // so NULL color targets will be bound when BuildNullColorTargets() is called for the first time.
    m_graphicsState.boundColorTargetMask = NoNullColorTargetMask;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdBindPipeline(
    const PipelineBindParams& params)
{
    if (params.pipelineBindPoint == PipelineBindPoint::Compute)
    {
        m_computeState.dynamicCsInfo           = params.cs;
        m_computeState.pipelineState.pPipeline = static_cast<const Pipeline*>(params.pPipeline);
        m_computeState.pipelineState.dirtyFlags.pipelineDirty = 1;
    }
    else
    {
        m_graphicsState.dynamicGraphicsInfo     = params.graphics;
        m_graphicsState.pipelineState.pPipeline = static_cast<const Pipeline*>(params.pPipeline);
        m_graphicsState.pipelineState.dirtyFlags.pipelineDirty = 1;
    }
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 471
    m_device.DescribeBindPipeline(this, params.apiPsoHash, params.pipelineBindPoint);
#endif
}

// =====================================================================================================================
// CmdSetUserData callback which updates the tracked user-data entries for the graphics state.
template <bool filterRedundantUserData>
void PAL_STDCALL UniversalCmdBuffer::CmdSetUserDataGfx(
    ICmdBuffer*   pCmdBuffer,
    uint32        firstEntry,
    uint32        entryCount,
    const uint32* pEntryValues)
{
    PAL_ASSERT((pCmdBuffer != nullptr) && (entryCount != 0) && (pEntryValues != nullptr));

    auto*const pSelf    = static_cast<UniversalCmdBuffer*>(pCmdBuffer);
    auto*const pEntries = &pSelf->m_graphicsState.gfxUserDataEntries;

    UserDataArgs userDataArgs;
    userDataArgs.firstEntry   = firstEntry;
    userDataArgs.entryCount   = entryCount;
    userDataArgs.pEntryValues = pEntryValues;

    if ((filterRedundantUserData == false) || pSelf->FilterSetUserDataGfx(&userDataArgs))
    {
        if (userDataArgs.entryCount == 1)
        {
            WideBitfieldSetBit(pEntries->touched, userDataArgs.firstEntry);
            WideBitfieldSetBit(pEntries->dirty,   userDataArgs.firstEntry);

            pEntries->entries[userDataArgs.firstEntry] = userDataArgs.pEntryValues[0];
        }
        else
        {
            const uint32 entryLimit = (userDataArgs.firstEntry + userDataArgs.entryCount);
            for (uint32 e = userDataArgs.firstEntry; e < entryLimit ; ++e)
            {
                WideBitfieldSetBit(pEntries->touched, e);
                WideBitfieldSetBit(pEntries->dirty,   e);
            }

            memcpy(&pEntries->entries[userDataArgs.firstEntry],
                   userDataArgs.pEntryValues,
                   (sizeof(uint32) * userDataArgs.entryCount));
        }
    } // if (filtering is disabled OR user data not redundant)
}

// Instantiate templates for the linker.
template
void PAL_STDCALL UniversalCmdBuffer::CmdSetUserDataGfx<false>(
    ICmdBuffer*   pCmdBuffer,
    uint32        firstEntry,
    uint32        entryCount,
    const uint32* pEntryValues);
template
void PAL_STDCALL UniversalCmdBuffer::CmdSetUserDataGfx<true>(
    ICmdBuffer*   pCmdBuffer,
    uint32        firstEntry,
    uint32        entryCount,
    const uint32* pEntryValues);

// =====================================================================================================================
// Compares the client-specified user data update parameters against the current user data values, and filters any
// redundant updates at the beginning of ending of the range.  Filtering redundant values in the middle of the range
// would involve significant updates to the rest of PAL, and we typically expect a good hit rate for redundant updates
// at the beginning or end.  The most common updates are setting 2-dword addresses (best hit rate on high bits) and
// 4-dword buffer SRDs (best hit rate on last dword).
//
// Returns true if there are still entries that should be processed after filtering.  False means that the entire set
// is redundant.
bool UniversalCmdBuffer::FilterSetUserDataGfx(
    UserDataArgs* pUserDataArgs)
{
    uint32        firstEntry   = pUserDataArgs->firstEntry;
    uint32        entryCount   = pUserDataArgs->entryCount;
    const uint32* pEntryValues = pUserDataArgs->pEntryValues;

    // Adjust the start entry and entry value pointer for any redundant entries found at the beginning of the range.
    while ((entryCount > 0) &&
           (*pEntryValues == m_graphicsState.gfxUserDataEntries.entries[firstEntry]) &&
           WideBitfieldIsSet(m_graphicsState.gfxUserDataEntries.touched, firstEntry))
    {
        firstEntry++;
        pEntryValues++;
        entryCount--;
    }

    bool result = false;
    if (entryCount > 0)
    {
        // Search from the end of the range for the last non-redundant entry.  We are guaranteed to find one since the
        // earlier loop found at least one non-redundant entry.
        uint32 idx = entryCount - 1;
        while ((pEntryValues[idx] == m_graphicsState.gfxUserDataEntries.entries[firstEntry + idx]) &&
               WideBitfieldIsSet(m_graphicsState.gfxUserDataEntries.touched, firstEntry + idx))
        {
            idx--;
        }

        // Update the caller's values.
        pUserDataArgs->firstEntry   = firstEntry;
        pUserDataArgs->entryCount   = idx + 1;
        pUserDataArgs->pEntryValues = pEntryValues;

        result = true;
    }

    return result;
}

// =====================================================================================================================
// Updates the given stencil state ref and masks params based on the flags set in StencilRefMaskParams
void UniversalCmdBuffer::SetStencilRefMasksState(
    const StencilRefMaskParams& updatedRefMaskState,    // [in]  Updated state
    StencilRefMaskParams*       pStencilRefMaskState)   // [out] State to be set
{
    if (updatedRefMaskState.flags.u8All == 0xFF)
    {
        *pStencilRefMaskState = updatedRefMaskState;
    }
    else
    {
        if (updatedRefMaskState.flags.updateFrontOpValue)
        {
            pStencilRefMaskState->flags.updateFrontOpValue = 1;
            pStencilRefMaskState->frontOpValue = updatedRefMaskState.frontOpValue;
        }
        if (updatedRefMaskState.flags.updateFrontRef)
        {
            pStencilRefMaskState->flags.updateFrontRef = 1;
            pStencilRefMaskState->frontRef = updatedRefMaskState.frontRef;
        }
        if (updatedRefMaskState.flags.updateFrontReadMask)
        {
            pStencilRefMaskState->flags.updateFrontReadMask = 1;
            pStencilRefMaskState->frontReadMask = updatedRefMaskState.frontReadMask;
        }
        if (updatedRefMaskState.flags.updateFrontWriteMask)
        {
            pStencilRefMaskState->flags.updateFrontWriteMask = 1;
            pStencilRefMaskState->frontWriteMask = updatedRefMaskState.frontWriteMask;
        }

        if (updatedRefMaskState.flags.updateBackOpValue)
        {
            pStencilRefMaskState->flags.updateBackOpValue = 1;
            pStencilRefMaskState->backOpValue = updatedRefMaskState.backOpValue;
        }
        if (updatedRefMaskState.flags.updateBackRef)
        {
            pStencilRefMaskState->flags.updateBackRef = 1;
            pStencilRefMaskState->backRef = updatedRefMaskState.backRef;
        }
        if (updatedRefMaskState.flags.updateBackReadMask)
        {
            pStencilRefMaskState->flags.updateBackReadMask = 1;
            pStencilRefMaskState->backReadMask = updatedRefMaskState.backReadMask;
        }
        if (updatedRefMaskState.flags.updateBackWriteMask)
        {
            pStencilRefMaskState->flags.updateBackWriteMask = 1;
            pStencilRefMaskState->backWriteMask = updatedRefMaskState.backWriteMask;
        }
    }
}

// =====================================================================================================================
// Binds an index buffer to this command buffer for use.
void UniversalCmdBuffer::CmdBindIndexData(
    gpusize   gpuAddr,
    uint32    indexCount,
    IndexType indexType)
{
    PAL_ASSERT(IsPow2Aligned(gpuAddr, (1ull << static_cast<uint64>(indexType))));
    PAL_ASSERT((indexType == IndexType::Idx8)  || (indexType == IndexType::Idx16) || (indexType == IndexType::Idx32));

    // Update the currently active index buffer state.
    m_graphicsState.iaState.indexAddr                    = gpuAddr;
    m_graphicsState.iaState.indexCount                   = indexCount;
    m_graphicsState.iaState.indexType                    = indexType;
    m_graphicsState.dirtyFlags.nonValidationBits.iaState = 1;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetViewInstanceMask(
    uint32 mask)
{
    m_graphicsState.viewInstanceMask = mask;
}

#if PAL_ENABLE_PRINTS_ASSERTS
// =====================================================================================================================
// Dumps this command buffer's DE and CE command streams to the given file with an appropriate header.
void UniversalCmdBuffer::DumpCmdStreamsToFile(
    File*          pFile,
    CmdBufDumpFormat mode
    ) const
{
    m_pDeCmdStream->DumpCommands(pFile, "# Universal Queue - DE Command length = ", mode);
    m_pCeCmdStream->DumpCommands(pFile, "# Universal Queue - CE Command length = ", mode);
}
#endif

// =====================================================================================================================
// Copies the currently bound state to m_graphicsRestoreState. This cannot be called again until PopGraphicsState is
// called.
void UniversalCmdBuffer::PushGraphicsState()
{
#if PAL_ENABLE_PRINTS_ASSERTS
    PAL_ASSERT(m_graphicsStateIsPushed == false);
    m_graphicsStateIsPushed = true;
#endif

    m_graphicsRestoreState = m_graphicsState;
    memset(&m_graphicsState.gfxUserDataEntries.touched[0], 0, sizeof(m_graphicsState.gfxUserDataEntries.touched));

    if (m_pCurrentExperiment != nullptr)
    {
        // Inform the performance experiment that we're starting some internal operations.
        m_pCurrentExperiment->BeginInternalOps(m_pDeCmdStream);
    }
}

// =====================================================================================================================
// Restores the last saved to m_graphicsRestoreState, rebinding all objects as necessary.
void UniversalCmdBuffer::PopGraphicsState()
{
#if PAL_ENABLE_PRINTS_ASSERTS
    PAL_ASSERT(m_graphicsStateIsPushed);
    m_graphicsStateIsPushed = false;
#endif

    // Note:  Vulkan does allow blits in nested command buffers, but they do not support inheriting user-data values
    // from the caller.  Therefore, simply "setting" the restored-state's user-data is sufficient, just like it is
    // in a root command buffer.  (If Vulkan decides to support user-data inheritance in a later API version, we'll
    // need to revisit this!)

    SetGraphicsState(m_graphicsRestoreState);

    // All RMP GFX Blts should push/pop command buffer's graphics state,
    // so this is a safe opprotunity to mark that a GFX Blt is active
    SetGfxCmdBufGfxBltState(true);
    SetGfxCmdBufGfxBltWriteCacheState(true);

    if (m_pCurrentExperiment != nullptr)
    {
        // Inform the performance experiment that we've finished some internal operations.
        m_pCurrentExperiment->EndInternalOps(m_pDeCmdStream);
    }
}

// =====================================================================================================================
// Set all specified state on this command buffer.
void UniversalCmdBuffer::SetGraphicsState(
    const GraphicsState& newGraphicsState)
{
    const auto& pipelineState = newGraphicsState.pipelineState;

    if (pipelineState.pPipeline != m_graphicsState.pipelineState.pPipeline)
    {
        PipelineBindParams bindParams = {};
        bindParams.pipelineBindPoint  = PipelineBindPoint::Graphics;
        bindParams.pPipeline          = pipelineState.pPipeline;
        bindParams.graphics           = newGraphicsState.dynamicGraphicsInfo;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 476
        bindParams.apiPsoHash         = pipelineState.apiPsoHash;
#endif

        CmdBindPipeline(bindParams);
    }

    if (pipelineState.pBorderColorPalette != m_graphicsState.pipelineState.pBorderColorPalette)
    {
        CmdBindBorderColorPalette(PipelineBindPoint::Graphics, pipelineState.pBorderColorPalette);
    }

    m_graphicsState.gfxUserDataEntries = newGraphicsState.gfxUserDataEntries;
    for (uint32 i = 0; i < NumUserDataFlagsParts; ++i)
    {
        m_graphicsState.gfxUserDataEntries.dirty[i] |= newGraphicsState.gfxUserDataEntries.touched[i];
    }
}

// =====================================================================================================================
Pal::PipelineState* UniversalCmdBuffer::PipelineState(
    PipelineBindPoint bindPoint)
{
    PAL_ASSERT((bindPoint == PipelineBindPoint::Compute) || (bindPoint == PipelineBindPoint::Graphics));
    return (bindPoint == PipelineBindPoint::Compute) ? &m_computeState.pipelineState : &m_graphicsState.pipelineState;
}

// =====================================================================================================================
// Helper method for handling the state "leakage" from a nested command buffer back to its caller. Since the callee has
// tracked its own state during the building phase, we can access the final state of the command buffer since its stored
// in the UniversalCmdBuffer object itself.
void UniversalCmdBuffer::LeakNestedCmdBufferState(
    const UniversalCmdBuffer& cmdBuffer)    // [in] Nested command buffer whose state we're absorbing.
{
    LeakPerPipelineStateChanges(cmdBuffer.m_computeState.pipelineState,
                                cmdBuffer.m_computeState.csUserDataEntries,
                                &m_computeState.pipelineState,
                                &m_computeState.csUserDataEntries);

    LeakPerPipelineStateChanges(cmdBuffer.m_graphicsState.pipelineState,
                                cmdBuffer.m_graphicsState.gfxUserDataEntries,
                                &m_graphicsState.pipelineState,
                                &m_graphicsState.gfxUserDataEntries);

    const GraphicsState& graphics = cmdBuffer.m_graphicsState;

    if (graphics.pColorBlendState != nullptr)
    {
        m_graphicsState.pColorBlendState = graphics.pColorBlendState;
    }

    if (graphics.pDepthStencilState != nullptr)
    {
        m_graphicsState.pDepthStencilState = graphics.pDepthStencilState;
    }

    if (graphics.pMsaaState != nullptr)
    {
        m_graphicsState.pMsaaState = graphics.pMsaaState;
    }

    if (graphics.pipelineState.pPipeline != nullptr)
    {
        m_graphicsState.enableMultiViewport    = graphics.enableMultiViewport;
        m_graphicsState.everUsedMultiViewport |= graphics.everUsedMultiViewport;
    }

    if (graphics.leakFlags.validationBits.colorTargetView != 0)
    {
        memcpy(&m_graphicsState.bindTargets.colorTargets[0],
               &graphics.bindTargets.colorTargets[0],
               sizeof(m_graphicsState.bindTargets.colorTargets));
        m_graphicsState.bindTargets.colorTargetCount = graphics.bindTargets.colorTargetCount;
    }

    if (graphics.leakFlags.validationBits.depthStencilView != 0)
    {
        m_graphicsState.bindTargets.depthTarget = graphics.bindTargets.depthTarget;
    }

    if (graphics.leakFlags.nonValidationBits.streamOutTargets != 0)
    {
        m_graphicsState.bindStreamOutTargets = graphics.bindStreamOutTargets;
    }

    if (graphics.leakFlags.nonValidationBits.iaState != 0)
    {
        m_graphicsState.iaState = graphics.iaState;
    }

    if (graphics.leakFlags.validationBits.inputAssemblyState != 0)
    {
        m_graphicsState.inputAssemblyState = graphics.inputAssemblyState;
    }

    if (graphics.leakFlags.nonValidationBits.blendConstState != 0)
    {
        m_graphicsState.blendConstState = graphics.blendConstState;
    }

    if (graphics.leakFlags.nonValidationBits.depthBiasState != 0)
    {
        m_graphicsState.depthBiasState = graphics.depthBiasState;
    }

    if (graphics.leakFlags.nonValidationBits.depthBoundsState != 0)
    {
        m_graphicsState.depthBoundsState = graphics.depthBoundsState;
    }

    if (graphics.leakFlags.nonValidationBits.pointLineRasterState != 0)
    {
        m_graphicsState.pointLineRasterState = graphics.pointLineRasterState;
    }

    if (graphics.leakFlags.nonValidationBits.stencilRefMaskState != 0)
    {
        m_graphicsState.stencilRefMaskState = graphics.stencilRefMaskState;
    }

    if (graphics.leakFlags.validationBits.triangleRasterState != 0)
    {
        m_graphicsState.triangleRasterState = graphics.triangleRasterState;
    }

    if (graphics.leakFlags.validationBits.viewports != 0)
    {
        m_graphicsState.viewportState = graphics.viewportState;
    }

    if (graphics.leakFlags.validationBits.scissorRects != 0)
    {
        m_graphicsState.scissorRectState = graphics.scissorRectState;
    }

    if (graphics.leakFlags.nonValidationBits.globalScissorState != 0)
    {
        m_graphicsState.globalScissorState = graphics.globalScissorState;
    }

    m_graphicsState.viewInstanceMask = graphics.viewInstanceMask;

    m_graphicsState.dirtyFlags.u32All |= graphics.leakFlags.u32All;

    memcpy(&m_blendOpts[0], &cmdBuffer.m_blendOpts[0], sizeof(m_blendOpts));

    // It is not expected that nested command buffers will use performance experiments.
    PAL_ASSERT(cmdBuffer.m_pCurrentExperiment == nullptr);
}

// =====================================================================================================================
// Returns a pointer to the command stream specified by "cmdStreamIdx".
const CmdStream* UniversalCmdBuffer::GetCmdStream(
    uint32 cmdStreamIdx
    ) const
{
    PAL_ASSERT(cmdStreamIdx < NumCmdStreams());

    CmdStream* pStream = nullptr;

    // CE command stream index < DE command stream index so CE will be launched before the DE.
    // DE cmd stream index > all others because CmdBuffer::End() uses
    // GetCmdStream(NumCmdStreams() - 1) to get a "root" chunk.
    switch (cmdStreamIdx)
    {
    case 0:
        pStream = m_pCeCmdStream;
        break;

    case 1:
        pStream = m_pDeCmdStream;
        break;
    }

    PAL_ASSERT(pStream != nullptr);

    return pStream;
}

} // Pal
