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
#include "core/hw/gfxip/borderColorPalette.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/graphicsPipeline.h"
#include "core/hw/gfxip/pipeline.h"
#include "core/hw/gfxip/pm4UniversalCmdBuffer.h"
#include "core/gpuMemory.h"
#include "core/perfExperiment.h"
#include "core/platform.h"
#include "palDequeImpl.h"
#include "palMath.h"
#include <limits.h>

using namespace Util;

namespace Pal
{

namespace Pm4
{

// =====================================================================================================================
UniversalCmdBuffer::UniversalCmdBuffer(
    const GfxDevice&           device,
    const CmdBufferCreateInfo& createInfo,
    Pm4::CmdStream*            pDeCmdStream,
    Pm4::CmdStream*            pCeCmdStream,
    Pm4::CmdStream*            pAceCmdStream,
    bool                       blendOptEnable)
    :
    Pm4CmdBuffer(device, createInfo),
    m_graphicsState{},
    m_graphicsRestoreState{},
    m_blendOpts{},
    m_pAceCmdStream(pAceCmdStream),
    m_device(device),
    m_pDeCmdStream(pDeCmdStream),
    m_pCeCmdStream(pCeCmdStream),
    m_blendOptEnable(blendOptEnable),
    m_contextStatesPerBin(1),
    m_persistentStatesPerBin(1)
{
    PAL_ASSERT(createInfo.queueType == QueueTypeUniversal);

    SwitchCmdSetUserDataFunc(PipelineBindPoint::Compute,  &Pm4CmdBuffer::CmdSetUserDataCs);
    SwitchCmdSetUserDataFunc(PipelineBindPoint::Graphics, &CmdSetUserDataGfx<true>);

    const PalPublicSettings* pPalSettings = m_device.Parent()->GetPublicSettings();

#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 755)
    constexpr TessDistributionFactors DefaultTessDistributionFactors = { 12, 30, 24, 24, 6 };
    m_tessDistributionFactors = DefaultTessDistributionFactors;
#else
    m_tessDistributionFactors = { pPalSettings->isolineDistributionFactor,
                                  pPalSettings->triDistributionFactor,
                                  pPalSettings->quadDistributionFactor,
                                  pPalSettings->donutDistributionFactor,
                                  pPalSettings->trapezoidDistributionFactor };
#endif

#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 744)
    m_contextStatesPerBin    = pPalSettings->binningContextStatesPerBin;
    m_persistentStatesPerBin = pPalSettings->binningPersistentStatesPerBin;
#endif
}

// =====================================================================================================================
// Resets the command buffer's previous contents and state, then puts it into a building state allowing new commands
// to be recorded.
// Also starts command buffer dumping, if it is enabled.
Result UniversalCmdBuffer::Begin(
    const CmdBufferBuildInfo& info)
{
#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 755)
    if (m_buildFlags.optimizeTessDistributionFactors)
    {
        m_tessDistributionFactors = info.clientTessDistributionFactors;
    }
#endif
    // m_persistentStatesPerBin and m_contextStatesPerBin
    // need to be set before the base class Begin() is called.
    // These values are read be ResetState() in the HWL which is called by Begin().

#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 763)
    if (m_buildFlags.optimizeContextStatesPerBin)
    {
        m_contextStatesPerBin = info.contextStatesPerBin;
    }
    if (m_buildFlags.optimizePersistentStatesPerBin)
    {
        m_persistentStatesPerBin = info.persistentStatesPerBin;
    }
#endif

    Result result = Pm4CmdBuffer::Begin(info);

    if (info.pInheritedState != nullptr)
    {
        m_graphicsState.inheritedState = *(info.pInheritedState);
    }

    return result;
}

// =====================================================================================================================
// Puts the command streams into a state that is ready for command building.
Result UniversalCmdBuffer::BeginCommandStreams(
    CmdStreamBeginFlags cmdStreamFlags,
    bool                doReset)
{
    Result result = Pm4CmdBuffer::BeginCommandStreams(cmdStreamFlags, doReset);

    if (doReset)
    {
        m_pDeCmdStream->Reset(nullptr, true);

        if (m_pCeCmdStream != nullptr)
        {
            m_pCeCmdStream->Reset(nullptr, true);
        }

        if (m_pAceCmdStream != nullptr)
        {
            m_pAceCmdStream->Reset(nullptr, true);
        }
    }

    if (result == Result::Success)
    {
        result = m_pDeCmdStream->Begin(cmdStreamFlags, m_pMemAllocator);
    }

    if ((result == Result::Success) && (m_pCeCmdStream != nullptr))
    {
        result = m_pCeCmdStream->Begin(cmdStreamFlags, m_pMemAllocator);
    }

    if ((result == Result::Success) && (m_pAceCmdStream != nullptr))
    {
        result = m_pAceCmdStream->Begin(cmdStreamFlags, m_pMemAllocator);
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
    Result result = Pm4CmdBuffer::End();

    if (result == Result::Success)
    {
        result = m_pDeCmdStream->End();
    }

    if ((result == Result::Success) && (m_pCeCmdStream != nullptr))
    {
        result = m_pCeCmdStream->End();
    }

    if ((result == Result::Success) && (m_pAceCmdStream != nullptr))
    {
        result = m_pAceCmdStream->End();
    }

    if (result == Result::Success)
    {

        m_graphicsState.leakFlags.u64All |= m_graphicsState.dirtyFlags.u64All;

        const Pal::CmdStream* cmdStreams[] = { m_pDeCmdStream, m_pCeCmdStream, m_pAceCmdStream };
        EndCmdBufferDump(cmdStreams, 3);
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
    Result result = Pm4CmdBuffer::Reset(pCmdAllocator, returnGpuMemory);

    if (result == Result::Success)
    {
        m_pDeCmdStream->Reset(static_cast<CmdAllocator*>(pCmdAllocator), returnGpuMemory);

        if (m_pCeCmdStream != nullptr)
        {
            m_pCeCmdStream->Reset(static_cast<CmdAllocator*>(pCmdAllocator), returnGpuMemory);
        }

        if (m_pAceCmdStream != nullptr)
        {
            m_pAceCmdStream->Reset(static_cast<CmdAllocator*>(pCmdAllocator), returnGpuMemory);
        }
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
    Pm4CmdBuffer::ResetState();

    memset(&m_graphicsState, 0, sizeof(m_graphicsState));

    // Clear the pointer to the performance experiment object currently used by this command buffer.
    m_pCurrentExperiment = nullptr;

    // NULL color target will only be bound if the slot was not NULL and is being set to NULL. Use a value of all 1s
    // so NULL color targets will be bound when BuildNullColorTargets() is called for the first time.
    m_graphicsState.boundColorTargetMask = NoNullColorTargetMask;

    if (IsNested() == false)
    {
        // Fully open scissor by default
        m_graphicsState.targetExtent.width  = MaxScissorExtent;
        m_graphicsState.targetExtent.height = MaxScissorExtent;
    }
    else
    {
        // For nested case, default to an invalid value to trigger validation if BindTarget called.
        static_assert(static_cast<uint16>(USHRT_MAX) > static_cast<uint16>(MaxScissorExtent), "Check Scissor logic");
        m_graphicsState.targetExtent.width  = USHRT_MAX;
        m_graphicsState.targetExtent.height = USHRT_MAX;
    }

    m_graphicsState.clipRectsState.clipRule = DefaultClipRectsRule;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdBindPipeline(
    const PipelineBindParams& params)
{
    if (params.pipelineBindPoint == PipelineBindPoint::Graphics)
    {
        m_graphicsState.pipelineState.dirtyFlags.pipeline |= (m_graphicsState.pipelineState.pPipeline !=
                                                             static_cast<const Pipeline*>(params.pPipeline)) ? 1 : 0;
        m_graphicsState.dynamicGraphicsInfo      = params.graphics;
        m_graphicsState.pipelineState.pPipeline  = static_cast<const Pipeline*>(params.pPipeline);
        m_graphicsState.pipelineState.apiPsoHash = params.apiPsoHash;
    }

    // Compute state and some additional generic support is handled by the Pm4CmdBuffer.
    Pm4CmdBuffer::CmdBindPipeline(params);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdBindPipelineWithOverrides(
    const PipelineBindParams& params,
    SwizzledFormat            swizzledFormat,
    uint32                    targetIndex)
{
    CmdBindPipeline(params);
    CmdOverwriteRbPlusFormatForBlits(swizzledFormat, targetIndex);
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

    if ((filterRedundantUserData == false) ||
        GfxCmdBuffer::FilterSetUserData(&userDataArgs,
                                        pSelf->m_graphicsState.gfxUserDataEntries.entries,
                                        pSelf->m_graphicsState.gfxUserDataEntries.touched))
    {
        SetUserData(userDataArgs.firstEntry, userDataArgs.entryCount, pEntries, userDataArgs.pEntryValues);
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
bool UniversalCmdBuffer::IsAnyGfxUserDataDirty() const
{
    return IsAnyUserDataDirty(&m_graphicsState.gfxUserDataEntries);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdDuplicateUserData(
    PipelineBindPoint source,
    PipelineBindPoint dest)
{
    PAL_ASSERT(source != dest);

    const UserDataEntries& sourceEntries = (source == PipelineBindPoint::Compute)
        ? m_computeState.csUserDataEntries
        : m_graphicsState.gfxUserDataEntries;

    CmdSetUserData(dest, 0, MaxUserDataEntries, sourceEntries.entries);
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

// =====================================================================================================================
// Sets parameters controlling line stippling.
void UniversalCmdBuffer::CmdSetLineStippleState(
    const LineStippleStateParams& params)
{
    m_graphicsState.lineStippleState = params;
    m_graphicsState.dirtyFlags.validationBits.lineStippleState = 1;
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 778
// =====================================================================================================================
// Sets color write mask params
void UniversalCmdBuffer::CmdSetColorWriteMask(
    const ColorWriteMaskParams& params)
{
    const auto*const pPipeline = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);

    if (pPipeline != nullptr)
    {
        uint32 updatedColorWriteMask      = 0;
        const uint8*const targetWriteMask = pPipeline->TargetWriteMasks();
        const uint32 maskShift            = 0x4;

        for (uint32 i = 0; i < pPipeline->NumColorTargets(); ++i)
        {
            if (i < params.count)
            {
                // The new color write mask must be a subset of the currently bound pipeline's color write mask.  Use
                // bitwise & to clear any bits not set in the pipeline's original mask.
                updatedColorWriteMask |= (params.colorWriteMask[i] & targetWriteMask[i]) << (i * maskShift);
            }
            else
            {
                // Enable any targets of the pipeline that are not specified in params.
                updatedColorWriteMask |= targetWriteMask[i] << (i * maskShift);
            }
        }

        PipelineBindParams bindParams = {};
        bindParams.pipelineBindPoint = PipelineBindPoint::Graphics;
        bindParams.pPipeline         = pPipeline;
        bindParams.apiPsoHash        = m_graphicsState.pipelineState.apiPsoHash;
        bindParams.graphics          = m_graphicsState.dynamicGraphicsInfo;
        bindParams.graphics.dynamicState.enable.colorWriteMask = 1;
        bindParams.graphics.dynamicState.colorWriteMask        = updatedColorWriteMask;

        CmdBindPipeline(bindParams);
    }
}

// =====================================================================================================================
// Sets dynamic rasterizer discard enable bit
void UniversalCmdBuffer::CmdSetRasterizerDiscardEnable(
    bool rasterizerDiscardEnable)
{
    const auto*const pPipeline = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);

    if (pPipeline != nullptr)
    {
        const TossPointMode tossPointMode = static_cast<TossPointMode>(m_device.Parent()->Settings().tossPointMode);

        PipelineBindParams bindParams = {};
        bindParams.pipelineBindPoint = PipelineBindPoint::Graphics;
        bindParams.pPipeline         = pPipeline;
        bindParams.apiPsoHash        = m_graphicsState.pipelineState.apiPsoHash;
        bindParams.graphics          = m_graphicsState.dynamicGraphicsInfo;
        bindParams.graphics.dynamicState.enable.rasterizerDiscardEnable = 1;
        bindParams.graphics.dynamicState.rasterizerDiscardEnable        =
            rasterizerDiscardEnable || (tossPointMode == TossPointAfterRaster);

        CmdBindPipeline(bindParams);
    }
}
#endif

// =====================================================================================================================
// Dumps this command buffer's DE and CE command streams to the given file with an appropriate header.
void UniversalCmdBuffer::DumpCmdStreamsToFile(
    File*            pFile,
    CmdBufDumpFormat mode
    ) const
{
    m_pDeCmdStream->DumpCommands(pFile, "# Universal Queue - DE Command length = ", mode);

    if (m_pCeCmdStream != nullptr)
    {
        m_pCeCmdStream->DumpCommands(pFile, "# Universal Queue - CE Command length = ", mode);
    }

    if (m_pAceCmdStream != nullptr)
    {
        m_pAceCmdStream->DumpCommands(pFile, "# Universal Queue - ACE Command length = ", mode);
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::EndCmdBufferDump(
    const Pal::CmdStream** ppCmdStreams,
    uint32                 cmdStreamsNum)
{
    if (IsDumpingEnabled() && DumpFile()->IsOpen())
    {
        if (m_device.Parent()->Settings().cmdBufDumpFormat == CmdBufDumpFormatBinaryHeaders)
        {
            CmdBufferDumpFileHeader fileHeader =
            {
                static_cast<uint32>(sizeof(CmdBufferDumpFileHeader)), // Structure size
                1,                                                    // Header version
                m_device.Parent()->ChipProperties().familyId,         // ASIC family
                m_device.Parent()->ChipProperties().eRevId,           // ASIC revision
                0                                                     // IB2 start index
            };

            CmdBufferListHeader listHeader =
            {
                static_cast<uint32>(sizeof(CmdBufferListHeader)),   // Structure size
                0,                                                  // Engine index
                0,                                                  // Number of command buffer chunks
            };

            for (uint32 i = 0; i < cmdStreamsNum && ppCmdStreams[i] != nullptr; i++)
            {
                listHeader.count += ppCmdStreams[i]->GetNumChunks();
            }

            fileHeader.ib2Start = (m_ib2DumpInfos.size() > 0) ? listHeader.count : 0;

            DumpFile()->Write(&fileHeader, sizeof(fileHeader));
            DumpFile()->Write(&listHeader, sizeof(listHeader));
        }

        DumpCmdStreamsToFile(DumpFile(), m_device.Parent()->Settings().cmdBufDumpFormat);
        DumpIb2s(DumpFile(), m_device.Parent()->Settings().cmdBufDumpFormat);
        DumpFile()->Close();
    }
}

// =====================================================================================================================
// Copies the currently bound state to m_graphicsRestoreState. This cannot be called again until CmdRestoreGraphicsState
// is called.
void UniversalCmdBuffer::CmdSaveGraphicsState()
{
    GfxCmdBuffer::CmdSaveGraphicsState();

    m_graphicsRestoreState = m_graphicsState;
    memset(&m_graphicsState.gfxUserDataEntries.touched[0], 0, sizeof(m_graphicsState.gfxUserDataEntries.touched));

    // Disable all active queries so that we don't sample internal operations in the app's query pool slots.
    // SEE: Pm4CmdBuffer::CmdSaveComputeState() for details on why this is not done for the Vulkan client.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 757
    if (m_buildFlags.disableQueryInternalOps)
    {
        DeactivateQueries();
    }
#endif
}

// =====================================================================================================================
// Restores the last saved to m_graphicsRestoreState, rebinding all objects as necessary.
void UniversalCmdBuffer::CmdRestoreGraphicsState()
{
    // Note:  Vulkan does allow blits in nested command buffers, but they do not support inheriting user-data values
    // from the caller.  Therefore, simply "setting" the restored-state's user-data is sufficient, just like it is
    // in a root command buffer.  (If Vulkan decides to support user-data inheritance in a later API version, we'll
    // need to revisit this!)

    SetGraphicsState(m_graphicsRestoreState);

    GfxCmdBuffer::CmdRestoreGraphicsState();

    // Reactivate all queries that we stopped in CmdSaveGraphicsState.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 757
    if (m_buildFlags.disableQueryInternalOps)
    {
        ReactivateQueries();
    }
#endif

    // All RMP GFX Blts should push/pop command buffer's graphics state,
    // so this is a safe opportunity to mark that a GFX Blt is active
    SetPm4CmdBufGfxBltState(true);
    SetPm4CmdBufGfxBltWriteCacheState(true);

    UpdatePm4CmdBufGfxBltExecEopFence();
    // Set a impossible waited fence until IssueReleaseSync assigns a meaningful value when sync RB cache.
    UpdatePm4CmdBufGfxBltWbEopFence(UINT32_MAX);
}

// =====================================================================================================================
// Set all specified state on this command buffer.
void UniversalCmdBuffer::SetGraphicsState(
    const GraphicsState& newGraphicsState)
{
    const auto& pipelineState = newGraphicsState.pipelineState;

    if (pipelineState.pPipeline != m_graphicsState.pipelineState.pPipeline ||
        (memcmp(&newGraphicsState.dynamicGraphicsInfo.dynamicState,
                &m_graphicsState.dynamicGraphicsInfo.dynamicState,
                sizeof(DynamicGraphicsState)) != 0))
    {
        PipelineBindParams bindParams = {};
        bindParams.pipelineBindPoint  = PipelineBindPoint::Graphics;
        bindParams.pPipeline          = pipelineState.pPipeline;
        bindParams.graphics           = newGraphicsState.dynamicGraphicsInfo;
        bindParams.apiPsoHash         = pipelineState.apiPsoHash;

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
        m_graphicsState.depthClampMode         = graphics.depthClampMode;
    }

    if (graphics.leakFlags.validationBits.colorTargetView != 0)
    {
        memcpy(&m_graphicsState.bindTargets.colorTargets[0],
               &graphics.bindTargets.colorTargets[0],
               sizeof(m_graphicsState.bindTargets.colorTargets));
        m_graphicsState.bindTargets.colorTargetCount = graphics.bindTargets.colorTargetCount;
        m_graphicsState.targetExtent.value           = graphics.targetExtent.value;
    }

    if (graphics.leakFlags.validationBits.depthStencilView != 0)
    {
        m_graphicsState.bindTargets.depthTarget = graphics.bindTargets.depthTarget;
        m_graphicsState.targetExtent.value      = graphics.targetExtent.value;
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

    if (graphics.leakFlags.nonValidationBits.clipRectsState != 0)
    {
        m_graphicsState.clipRectsState = graphics.clipRectsState;
    }

    if (graphics.leakFlags.validationBits.vrsRateParams != 0)
    {
        m_graphicsState.vrsRateState = graphics.vrsRateState;
    }

    if (graphics.leakFlags.validationBits.vrsCenterState != 0)
    {
        m_graphicsState.vrsCenterState = graphics.vrsCenterState;
    }

    if (graphics.leakFlags.validationBits.vrsImage != 0)
    {
        m_graphicsState.pVrsImage = graphics.pVrsImage;
    }

    m_graphicsState.viewInstanceMask = graphics.viewInstanceMask;

    m_graphicsState.dirtyFlags.u64All |= graphics.leakFlags.u64All;

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
    // The ACE command stream is located first so that the DE CmdStream is at NumCmdStreams() - 1
    // and the CE CmdStream remains before the DE CmdStream.
    switch (cmdStreamIdx)
    {
    case 0:
        pStream = m_pAceCmdStream;
        break;
    case 1:
        pStream = m_pCeCmdStream;
        break;
    case 2:
        pStream = m_pDeCmdStream;
        break;
    }

    return pStream;
}

// =====================================================================================================================
// Returns the number of command streams associated with this command buffer, for the specified ganged sub-queue index.
uint32 UniversalCmdBuffer::NumCmdStreamsInSubQueue(
    int32 subQueueIndex
    ) const
{
    PAL_ASSERT(subQueueIndex < int32(AceStreamCount));

    // The main sub-queue has two streams (DE and CE), other ganged sub-queues have one stream (ACE).
    return (subQueueIndex == MainSubQueueIdx) ? 2 : 1;
}

// =====================================================================================================================
// Returns a pointer to the command stream specified by the given ganged sub-queue index and command stream index.
const CmdStream* UniversalCmdBuffer::GetCmdStreamInSubQueue(
    int32  subQueueIndex,
    uint32 cmdStreamIndex
    ) const
{
    PAL_ASSERT(cmdStreamIndex < NumCmdStreamsInSubQueue(subQueueIndex));

    const CmdStream* pStream = nullptr;
    if (subQueueIndex == MainSubQueueIdx)
    {
        // For the "main" sub-queue, CE always comes first.
        pStream = (cmdStreamIndex == 0) ? m_pCeCmdStream : m_pDeCmdStream;
    }
    else
    {
        PAL_ASSERT(subQueueIndex == 0); // Only one ganged sub-queue currently supported.
        pStream = m_pAceCmdStream; // Ganged sub-queues are always ACE queues.
    }

    return pStream;
}

// =====================================================================================================================
uint32 UniversalCmdBuffer::GetUsedSize(
    CmdAllocType type
    ) const
{
    uint32 sizeInBytes = GfxCmdBuffer::GetUsedSize(type);

    if (type == CommandDataAlloc)
    {
        sizeInBytes += (m_pDeCmdStream->GetUsedCmdMemorySize() +
                       ((m_pCeCmdStream != nullptr) ? m_pCeCmdStream->GetUsedCmdMemorySize() : 0));
    }

    return sizeInBytes;
}

// =====================================================================================================================
// Record the VRS rate structure so RPM has a copy for save / restore purposes.
void UniversalCmdBuffer::CmdSetPerDrawVrsRate(
    const VrsRateParams&  rateParams)
{
    // Record the state so that we can restore it after RPM operations
    m_graphicsState.vrsRateState = rateParams;
    m_graphicsState.dirtyFlags.validationBits.vrsRateParams = 1;
}

// =====================================================================================================================
// Record the VRS center state structure so RPM has a copy for save / restore purposes.
void UniversalCmdBuffer::CmdSetVrsCenterState(
    const VrsCenterState&  centerState)
{
    // Record the state so that we can restore it after RPM operations.
    m_graphicsState.vrsCenterState = centerState;
    m_graphicsState.dirtyFlags.validationBits.vrsCenterState = 1;
}

// =====================================================================================================================
// Probably setup dirty state here...  indicate that draw time potentially has a lot to do.
void UniversalCmdBuffer::CmdBindSampleRateImage(
    const IImage*  pImage)
{
    // Binding a NULL image is always ok; otherwise, verify that the HW supports VRS images.
    PAL_ASSERT((pImage == nullptr) || (m_device.Parent()->ChipProperties().imageProperties.vrsTileSize.width != 0));

    m_graphicsState.pVrsImage = static_cast<const Image*>(pImage);
    m_graphicsState.dirtyFlags.validationBits.vrsImage = 1;
}

} // Pm4
} // Pal
