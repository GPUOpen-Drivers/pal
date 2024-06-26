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
    const GfxBarrierMgr*       pBarrierMgr,
    Pm4::CmdStream*            pDeCmdStream,
    Pm4::CmdStream*            pCeCmdStream,
    Pm4::CmdStream*            pAceCmdStream,
    bool                       blendOptEnable,
    bool                       useUpdateUserData)
    :
    Pm4CmdBuffer(device, createInfo, pBarrierMgr),
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

    if (useUpdateUserData)
    {
        SwitchCmdSetUserDataFunc(PipelineBindPoint::Compute, &Pm4CmdBuffer::CmdUpdateUserDataCs);
        SwitchCmdSetUserDataFunc(PipelineBindPoint::Graphics, &CmdUpdateUserDataGfx);
    }
    else
    {
        SwitchCmdSetUserDataFunc(PipelineBindPoint::Compute, &Pm4CmdBuffer::CmdSetUserDataCs);
        SwitchCmdSetUserDataFunc(PipelineBindPoint::Graphics, &CmdSetUserDataGfxFiltered);
    }

    constexpr TessDistributionFactors DefaultTessDistributionFactors = { 12, 30, 24, 24, 6 };
    m_tessDistributionFactors = DefaultTessDistributionFactors;

    const PalPublicSettings* pPalSettings = m_device.Parent()->GetPublicSettings();

    m_contextStatesPerBin    = pPalSettings->binningContextStatesPerBin;
    m_persistentStatesPerBin = pPalSettings->binningPersistentStatesPerBin;
}

// =====================================================================================================================
// Resets the command buffer's previous contents and state, then puts it into a building state allowing new commands
// to be recorded.
// Also starts command buffer dumping, if it is enabled.
Result UniversalCmdBuffer::Begin(
    const CmdBufferBuildInfo& info)
{
    if (info.flags.optimizeTessDistributionFactors)
    {
        m_tessDistributionFactors = info.clientTessDistributionFactors;
    }

    // m_persistentStatesPerBin and m_contextStatesPerBin
    // need to be set before the base class Begin() is called.
    // These values are read be ResetState() in the HWL which is called by Begin().

    if (info.flags.optimizeContextStatesPerBin)
    {
        m_contextStatesPerBin = info.contextStatesPerBin;
    }

    if (info.flags.optimizePersistentStatesPerBin)
    {
        m_persistentStatesPerBin = info.persistentStatesPerBin;
    }

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

        m_graphicsState.leakFlags.u32All |= m_graphicsState.dirtyFlags.u32All;

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

    // No guard band by default
    m_graphicsState.viewportState.horzClipRatio    = 1.0f;
    m_graphicsState.viewportState.horzDiscardRatio = 1.0f;
    m_graphicsState.viewportState.vertClipRatio    = 1.0f;
    m_graphicsState.viewportState.vertDiscardRatio = 1.0f;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdBindPipeline(
    const PipelineBindParams& params)
{
    if (params.pipelineBindPoint == PipelineBindPoint::Graphics)
    {
        m_graphicsState.pipelineState.dirtyFlags.pipeline |= (m_graphicsState.pipelineState.pPipeline !=
                                                             static_cast<const Pipeline*>(params.pPipeline)) ? 1 : 0;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 842
        m_graphicsState.dynamicGraphicsInfo = params.graphics;
#else
        m_graphicsState.dynamicGraphicsInfo = params.gfxShaderInfo;
        m_graphicsState.dynamicState        = params.gfxDynState;
#endif
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
    CmdOverwriteColorExportInfoForBlits(swizzledFormat, targetIndex);
}

// =====================================================================================================================
void PAL_STDCALL UniversalCmdBuffer::CmdUpdateUserDataGfx(
    ICmdBuffer*   pCmdBuffer,
    uint32        firstEntry,
    uint32        entryCount,
    const uint32* pEntryValues)
{
    auto* const pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);
    UpdateUserData(&pThis->m_graphicsState.gfxUserDataEntries, firstEntry, entryCount, pEntryValues);
}

// =====================================================================================================================
// CmdSetUserData callback which updates the tracked user-data entries for the graphics state.
// Does redundant filtering, except for middle entries in entryCount >= 3 partially redundant ranges.
// For HWL's that definitely don't mind potentially more holes in the dirty mask, the newer UpdateUserData can be used.
void PAL_STDCALL UniversalCmdBuffer::CmdSetUserDataGfxFiltered(
    ICmdBuffer*   pCmdBuffer,
    uint32        firstEntry,
    uint32        entryCount,
    const uint32* pEntryValues)
{
    PAL_DEBUG_BUILD_ONLY_ASSERT((pCmdBuffer != nullptr) && (entryCount != 0) && (pEntryValues != nullptr));

    auto*const pSelf    = static_cast<UniversalCmdBuffer*>(pCmdBuffer);
    auto*const pEntries = &pSelf->m_graphicsState.gfxUserDataEntries;

    UserDataArgs userDataArgs;
    userDataArgs.firstEntry   = firstEntry;
    userDataArgs.entryCount   = entryCount;
    userDataArgs.pEntryValues = pEntryValues;

    if (GfxCmdBuffer::FilterSetUserData(&userDataArgs,
                                        pSelf->m_graphicsState.gfxUserDataEntries.entries,
                                        pSelf->m_graphicsState.gfxUserDataEntries.touched))
    {
        SetUserData(userDataArgs.firstEntry, userDataArgs.entryCount, pEntries, userDataArgs.pEntryValues);
    }
}

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
    m_graphicsState.iaState.indexAddr  = gpuAddr;
    m_graphicsState.iaState.indexCount = indexCount;
    m_graphicsState.iaState.indexType  = indexType;
    m_graphicsState.dirtyFlags.iaState = 1;
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
    m_graphicsState.lineStippleState            = params;
    m_graphicsState.dirtyFlags.lineStippleState = 1;
}

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
// Copies the currently bound state to m_graphicsRestoreState. This cannot be called again until
// CmdRestoreGraphicsStateInternal is called.
void UniversalCmdBuffer::CmdSaveGraphicsState()
{
    GfxCmdBuffer::CmdSaveGraphicsState();

    m_graphicsRestoreState = m_graphicsState;
    memset(&m_graphicsState.gfxUserDataEntries.touched[0], 0, sizeof(m_graphicsState.gfxUserDataEntries.touched));

    // Disable all active queries so that we don't sample internal operations in the app's query pool slots.
    // SEE: Pm4CmdBuffer::CmdSaveComputeState() for details on why we don't expect Vulkan to set this flag.
    if (m_buildFlags.disableQueryInternalOps)
    {
        DeactivateQueries();
    }
}

// =====================================================================================================================
// Restores the last saved to m_graphicsRestoreState, rebinding all objects as necessary.
void UniversalCmdBuffer::CmdRestoreGraphicsStateInternal(
    bool trackBltActiveFlags)
{
    // Note:  Vulkan does allow blits in nested command buffers, but they do not support inheriting user-data values
    // from the caller.  Therefore, simply "setting" the restored-state's user-data is sufficient, just like it is
    // in a root command buffer.  (If Vulkan decides to support user-data inheritance in a later API version, we'll
    // need to revisit this!)

    SetGraphicsState(m_graphicsRestoreState);

    GfxCmdBuffer::CmdRestoreGraphicsStateInternal(trackBltActiveFlags);

    // Reactivate all queries that we stopped in CmdSaveGraphicsState.
    if (m_buildFlags.disableQueryInternalOps)
    {
        ReactivateQueries();
    }

    // No need track blt active flags (expect trackBltActiveFlags == false) for below cases:
    //  1. CmdRestoreGraphicsState() call from PAL clients.
    //  2. CmdRestoreGraphicsState() call from CmdClearBound*Targets().
    //  3. CmdRestoreGraphicsState() call from auto sync clear case.
    if (trackBltActiveFlags)
    {
        // All RMP GFX Blts should push/pop command buffer's graphics state,
        // so this is a safe opportunity to mark that a GFX Blt is active
        SetGfxBltState(true);
        SetGfxBltWriteCacheState(true);

        UpdateGfxBltExecEopFence();
        // Set a impossible waited fence until IssueReleaseSync assigns a meaningful value when sync RB cache.
        UpdateGfxBltWbEopFence(UINT32_MAX);

#if PAL_DEVELOPER_BUILD
        Developer::RpmBltData cbData = { .pCmdBuffer = this, .bltType = Developer::RpmBltType::Draw };
        m_device.Parent()->DeveloperCb(Developer::CallbackType::RpmBlt, &cbData);
#endif
    }
}

// =====================================================================================================================
// Set all specified state on this command buffer.
// State will be set if it differs from the current graphics state.
void UniversalCmdBuffer::SetGraphicsState(
    const GraphicsState& newGraphicsState)
{
    GraphicsStateFlags setGraphicsStateFlags { };
    PipelineStateFlags setPipelineStateFlags { };
    SetGraphicsState(newGraphicsState, setGraphicsStateFlags, setPipelineStateFlags);
}

// =====================================================================================================================
// Set all specified state on this command buffer.
// State will be set if it differs from the currently graphics state or the corresponding dirty bit is set in the
// input flag parameters
void UniversalCmdBuffer::SetGraphicsState(
    const GraphicsState& newGraphicsState,
    GraphicsStateFlags   setGraphicStateFlags,
    PipelineStateFlags   setPipelineStateFlags)
{
    const auto& pipelineState = newGraphicsState.pipelineState;

    if (setPipelineStateFlags.pipeline ||
        pipelineState.pPipeline != m_graphicsState.pipelineState.pPipeline ||
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 842
        (memcmp(&newGraphicsState.dynamicGraphicsInfo.dynamicState,
                &m_graphicsState.dynamicGraphicsInfo.dynamicState,
                sizeof(DynamicGraphicsState)) != 0)
#else
        (memcmp(&newGraphicsState.dynamicState,
                &m_graphicsState.dynamicState,
                sizeof(m_graphicsState.dynamicState)) != 0) ||
        (memcmp(&newGraphicsState.dynamicGraphicsInfo,
                &m_graphicsState.dynamicGraphicsInfo,
                sizeof(m_graphicsState.dynamicGraphicsInfo)) != 0)
#endif
        )
    {
        PipelineBindParams bindParams = {};
        bindParams.pipelineBindPoint  = PipelineBindPoint::Graphics;
        bindParams.pPipeline          = pipelineState.pPipeline;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 842
        bindParams.graphics           = newGraphicsState.dynamicGraphicsInfo;
#else
        bindParams.gfxShaderInfo      = newGraphicsState.dynamicGraphicsInfo;
        bindParams.gfxDynState        = newGraphicsState.dynamicState;
#endif
        bindParams.apiPsoHash         = pipelineState.apiPsoHash;

        CmdBindPipeline(bindParams);
    }

    if (setPipelineStateFlags.borderColorPalette ||
        (pipelineState.pBorderColorPalette != m_graphicsState.pipelineState.pBorderColorPalette))
    {
        CmdBindBorderColorPalette(PipelineBindPoint::Graphics, pipelineState.pBorderColorPalette);
    }

    m_graphicsState.gfxUserDataEntries = newGraphicsState.gfxUserDataEntries;
    for (uint32 i = 0; i < NumUserDataFlagsParts; ++i)
    {
        m_graphicsState.gfxUserDataEntries.dirty[i] |= newGraphicsState.gfxUserDataEntries.touched[i];

        if (setPipelineStateFlags.pipeline)
        {
            m_graphicsState.gfxUserDataEntries.dirty[i] = 0;
            m_graphicsState.gfxUserDataEntries.dirty[i] = ~m_graphicsState.gfxUserDataEntries.dirty[i];
        }
    }

    // The target state that we would restore is invalid if this is a nested command buffer that inherits target
    // view state. The only allowed BLTs in a nested command buffer are CmdClearBoundColorTargets and
    // CmdClearBoundDepthStencilTargets, neither of which will overwrite the bound targets.
    if ((m_graphicsState.inheritedState.stateFlags.targetViewState == 0) &&
        ((setGraphicStateFlags.colorTargetView  ||
          setGraphicStateFlags.depthStencilView ||
          (memcmp(&newGraphicsState.bindTargets,
                  &m_graphicsState.bindTargets,
                  sizeof(m_graphicsState.bindTargets)) != 0))))
    {
        CmdBindTargets(newGraphicsState.bindTargets);
    }

    if (setGraphicStateFlags.iaState ||
        (newGraphicsState.iaState.indexAddr  != m_graphicsState.iaState.indexAddr)  ||
        (newGraphicsState.iaState.indexCount != m_graphicsState.iaState.indexCount) ||
        (newGraphicsState.iaState.indexType  != m_graphicsState.iaState.indexType))
    {
        CmdBindIndexData(newGraphicsState.iaState.indexAddr,
                         newGraphicsState.iaState.indexCount,
                         newGraphicsState.iaState.indexType);
    }

    if (setGraphicStateFlags.inputAssemblyState ||
        (memcmp(&newGraphicsState.inputAssemblyState,
                &m_graphicsState.inputAssemblyState,
                sizeof(m_graphicsState.inputAssemblyState)) != 0))
    {
        CmdSetInputAssemblyState(newGraphicsState.inputAssemblyState);
    }

    if (setGraphicStateFlags.colorBlendState ||
        (newGraphicsState.pColorBlendState != m_graphicsState.pColorBlendState))
    {
        CmdBindColorBlendState(newGraphicsState.pColorBlendState);
    }

    if (setGraphicStateFlags.blendConstState ||
        (memcmp(newGraphicsState.blendConstState.blendConst,
                m_graphicsState.blendConstState.blendConst,
                sizeof(m_graphicsState.blendConstState.blendConst)) != 0))
    {
        CmdSetBlendConst(newGraphicsState.blendConstState);
    }

    if (setGraphicStateFlags.stencilRefMaskState ||
        (memcmp(&newGraphicsState.stencilRefMaskState,
                &m_graphicsState.stencilRefMaskState,
                sizeof(m_graphicsState.stencilRefMaskState)) != 0))
    {
        // Setting StencilRefMaskState flags to 0xFF so that the faster command is used instead of read-modify-write
        StencilRefMaskParams stencilRefMaskState = newGraphicsState.stencilRefMaskState;
        stencilRefMaskState.flags.u8All = 0xFF;

        CmdSetStencilRefMasks(stencilRefMaskState);
    }

    if (setGraphicStateFlags.depthStencilState ||
        (newGraphicsState.pDepthStencilState != m_graphicsState.pDepthStencilState))
    {
        CmdBindDepthStencilState(newGraphicsState.pDepthStencilState);
    }

    if (setGraphicStateFlags.depthBoundsState ||
        (newGraphicsState.depthBoundsState.min != m_graphicsState.depthBoundsState.min) ||
        (newGraphicsState.depthBoundsState.max != m_graphicsState.depthBoundsState.max))
    {
        CmdSetDepthBounds(newGraphicsState.depthBoundsState);
    }

    if (setGraphicStateFlags.msaaState ||
        newGraphicsState.pMsaaState != m_graphicsState.pMsaaState)
    {
        CmdBindMsaaState(newGraphicsState.pMsaaState);
    }

    if (setGraphicStateFlags.lineStippleState ||
        (memcmp(&newGraphicsState.lineStippleState,
                &m_graphicsState.lineStippleState,
                sizeof(LineStippleStateParams)) != 0))
    {
        CmdSetLineStippleState(newGraphicsState.lineStippleState);
    }

    if (setGraphicStateFlags.quadSamplePatternState ||
        (memcmp(&newGraphicsState.quadSamplePatternState,
                &m_graphicsState.quadSamplePatternState,
                sizeof(MsaaQuadSamplePattern)) != 0))
    {
        // numSamplesPerPixel can be 0 if the client never called CmdSetMsaaQuadSamplePattern.
        if (newGraphicsState.numSamplesPerPixel != 0)
        {
            CmdSetMsaaQuadSamplePattern(newGraphicsState.numSamplesPerPixel,
                newGraphicsState.quadSamplePatternState);
        }
    }

    if (setGraphicStateFlags.triangleRasterState ||
        (memcmp(&newGraphicsState.triangleRasterState,
                &m_graphicsState.triangleRasterState,
                sizeof(m_graphicsState.triangleRasterState)) != 0))
    {
        CmdSetTriangleRasterState(newGraphicsState.triangleRasterState);
    }

    if (setGraphicStateFlags.pointLineRasterState ||
        (memcmp(&newGraphicsState.pointLineRasterState,
                &m_graphicsState.pointLineRasterState,
                sizeof(m_graphicsState.pointLineRasterState)) != 0))
    {
        CmdSetPointLineRasterState(newGraphicsState.pointLineRasterState);
    }

    const auto& restoreDepthBiasState = newGraphicsState.depthBiasState;

    if (setGraphicStateFlags.depthBiasState ||
        (restoreDepthBiasState.depthBias            != m_graphicsState.depthBiasState.depthBias)      ||
        (restoreDepthBiasState.depthBiasClamp       != m_graphicsState.depthBiasState.depthBiasClamp) ||
        (restoreDepthBiasState.slopeScaledDepthBias != m_graphicsState.depthBiasState.slopeScaledDepthBias))
    {
        CmdSetDepthBiasState(newGraphicsState.depthBiasState);
    }

    const auto& restoreViewports = newGraphicsState.viewportState;
    const auto& currentViewports = m_graphicsState.viewportState;

    if (setGraphicStateFlags.viewports ||
        (restoreViewports.count != currentViewports.count) ||
        (restoreViewports.depthRange != currentViewports.depthRange) ||
        (restoreViewports.horzDiscardRatio != currentViewports.horzDiscardRatio) ||
        (restoreViewports.vertDiscardRatio != currentViewports.vertDiscardRatio) ||
        (restoreViewports.horzClipRatio    != currentViewports.horzClipRatio)    ||
        (restoreViewports.vertClipRatio    != currentViewports.vertClipRatio)    ||
        (memcmp(&restoreViewports.viewports[0],
                &currentViewports.viewports[0],
                restoreViewports.count * sizeof(restoreViewports.viewports[0])) != 0))
    {
        CmdSetViewports(restoreViewports);
    }

    const auto& restoreScissorRects = newGraphicsState.scissorRectState;
    const auto& currentScissorRects = m_graphicsState.scissorRectState;

    if (setGraphicStateFlags.scissorRects ||
        (restoreScissorRects.count != currentScissorRects.count) ||
        (memcmp(&restoreScissorRects.scissors[0],
                &currentScissorRects.scissors[0],
                restoreScissorRects.count * sizeof(restoreScissorRects.scissors[0])) != 0))
    {
        CmdSetScissorRects(restoreScissorRects);
    }

    const auto& restoreGlobalScissor = newGraphicsState.globalScissorState.scissorRegion;
    const auto& currentGlobalScissor = m_graphicsState.globalScissorState.scissorRegion;

    if (setGraphicStateFlags.globalScissorState ||
        (restoreGlobalScissor.offset.x      != currentGlobalScissor.offset.x)     ||
        (restoreGlobalScissor.offset.y      != currentGlobalScissor.offset.y)     ||
        (restoreGlobalScissor.extent.width  != currentGlobalScissor.extent.width) ||
        (restoreGlobalScissor.extent.height != currentGlobalScissor.extent.height))
    {
        CmdSetGlobalScissor(newGraphicsState.globalScissorState);
    }

    const auto& restoreClipRects = newGraphicsState.clipRectsState;
    const auto& currentClipRects = m_graphicsState.clipRectsState;

    if (setGraphicStateFlags.clipRectsState ||
        (restoreClipRects.clipRule != currentClipRects.clipRule)   ||
        (restoreClipRects.rectCount != currentClipRects.rectCount) ||
        (memcmp(&restoreClipRects.rectList[0],
                &currentClipRects.rectList[0],
                restoreClipRects.rectCount * sizeof(Rect))))
    {
        CmdSetClipRects(newGraphicsState.clipRectsState.clipRule,
                        newGraphicsState.clipRectsState.rectCount,
                        newGraphicsState.clipRectsState.rectList);
    }

    if (setGraphicStateFlags.vrsRateParams ||
        (memcmp(&newGraphicsState.vrsRateState, &m_graphicsState.vrsRateState, sizeof(VrsRateParams)) != 0))
    {
        CmdSetPerDrawVrsRate(newGraphicsState.vrsRateState);
    }

    if (setGraphicStateFlags.vrsCenterState ||
        (memcmp(&newGraphicsState.vrsCenterState, &m_graphicsState.vrsCenterState, sizeof(VrsCenterState)) != 0))
    {
        CmdSetVrsCenterState(newGraphicsState.vrsCenterState);
    }

    if (setGraphicStateFlags.vrsImage ||
        (newGraphicsState.pVrsImage != m_graphicsState.pVrsImage))
    {
        // Restore the pointer to the client's original VRS rate image.  On GFX10 products, if the bound depth stencil
        // image has changed, this will be re-copied into hTile on the next draw.
        CmdBindSampleRateImage(newGraphicsState.pVrsImage);
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

    if (graphics.leakFlags.colorTargetView != 0)
    {
        memcpy(&m_graphicsState.bindTargets.colorTargets[0],
               &graphics.bindTargets.colorTargets[0],
               sizeof(m_graphicsState.bindTargets.colorTargets));
        m_graphicsState.bindTargets.colorTargetCount = graphics.bindTargets.colorTargetCount;
        m_graphicsState.targetExtent                 = graphics.targetExtent;
    }

    if (graphics.leakFlags.depthStencilView != 0)
    {
        m_graphicsState.bindTargets.depthTarget = graphics.bindTargets.depthTarget;
        m_graphicsState.targetExtent            = graphics.targetExtent;
    }

    if (graphics.leakFlags.streamOutTargets != 0)
    {
        m_graphicsState.bindStreamOutTargets = graphics.bindStreamOutTargets;
    }

    if (graphics.leakFlags.iaState != 0)
    {
        m_graphicsState.iaState = graphics.iaState;
    }

    if (graphics.leakFlags.inputAssemblyState != 0)
    {
        m_graphicsState.inputAssemblyState = graphics.inputAssemblyState;
    }

    if (graphics.leakFlags.blendConstState != 0)
    {
        m_graphicsState.blendConstState = graphics.blendConstState;
    }

    if (graphics.leakFlags.depthBiasState != 0)
    {
        m_graphicsState.depthBiasState = graphics.depthBiasState;
    }

    if (graphics.leakFlags.depthBoundsState != 0)
    {
        m_graphicsState.depthBoundsState = graphics.depthBoundsState;
    }

    if (graphics.leakFlags.pointLineRasterState != 0)
    {
        m_graphicsState.pointLineRasterState = graphics.pointLineRasterState;
    }

    if (graphics.leakFlags.stencilRefMaskState != 0)
    {
        m_graphicsState.stencilRefMaskState = graphics.stencilRefMaskState;
    }

    if (graphics.leakFlags.triangleRasterState != 0)
    {
        m_graphicsState.triangleRasterState = graphics.triangleRasterState;
    }

    if (graphics.leakFlags.viewports != 0)
    {
        m_graphicsState.viewportState = graphics.viewportState;
    }

    if (graphics.leakFlags.scissorRects != 0)
    {
        m_graphicsState.scissorRectState = graphics.scissorRectState;
    }

    if (graphics.leakFlags.globalScissorState != 0)
    {
        m_graphicsState.globalScissorState = graphics.globalScissorState;
    }

    if (graphics.leakFlags.clipRectsState != 0)
    {
        m_graphicsState.clipRectsState = graphics.clipRectsState;
    }

    if (graphics.leakFlags.vrsRateParams != 0)
    {
        m_graphicsState.vrsRateState = graphics.vrsRateState;
    }

    if (graphics.leakFlags.vrsCenterState != 0)
    {
        m_graphicsState.vrsCenterState = graphics.vrsCenterState;
    }

    if (graphics.leakFlags.vrsImage != 0)
    {
        m_graphicsState.pVrsImage = graphics.pVrsImage;
    }

    if (graphics.pipelineState.pPipeline != nullptr)
    {
        m_funcTable.pfnCmdDraw                      = cmdBuffer.m_funcTable.pfnCmdDraw;
        m_funcTable.pfnCmdDrawOpaque                = cmdBuffer.m_funcTable.pfnCmdDrawOpaque;
        m_funcTable.pfnCmdDrawIndexed               = cmdBuffer.m_funcTable.pfnCmdDrawIndexed;
        m_funcTable.pfnCmdDrawIndirectMulti         = cmdBuffer.m_funcTable.pfnCmdDrawIndirectMulti;
        m_funcTable.pfnCmdDrawIndexedIndirectMulti  = cmdBuffer.m_funcTable.pfnCmdDrawIndexedIndirectMulti;
        m_funcTable.pfnCmdDispatchMesh              = cmdBuffer.m_funcTable.pfnCmdDispatchMesh;
        m_funcTable.pfnCmdDispatchMeshIndirectMulti = cmdBuffer.m_funcTable.pfnCmdDispatchMeshIndirectMulti;
    }

    // It is possible that nested command buffer execute operation which affect the data in the primary buffer
    const Pm4CmdBufferStateFlags srcFlags = cmdBuffer.m_pm4CmdBufState.flags;

    m_pm4CmdBufState.flags.gfxBltActive                        = srcFlags.gfxBltActive;
    m_pm4CmdBufState.flags.csBltActive                         = srcFlags.csBltActive;
    m_pm4CmdBufState.flags.cpBltActive                         = srcFlags.cpBltActive;
    m_pm4CmdBufState.flags.gfxWriteCachesDirty                 = srcFlags.gfxWriteCachesDirty;
    m_pm4CmdBufState.flags.csWriteCachesDirty                  = srcFlags.csWriteCachesDirty;
    m_pm4CmdBufState.flags.cpWriteCachesDirty                  = srcFlags.cpWriteCachesDirty;
    m_pm4CmdBufState.flags.cpMemoryWriteL2CacheStale           = srcFlags.cpMemoryWriteL2CacheStale;
    m_pm4CmdBufState.flags.csBltDirectWriteMisalignedMdDirty   = srcFlags.csBltDirectWriteMisalignedMdDirty;
    m_pm4CmdBufState.flags.csBltIndirectWriteMisalignedMdDirty = srcFlags.csBltIndirectWriteMisalignedMdDirty;
    m_pm4CmdBufState.flags.gfxBltDirectWriteMisalignedMdDirty  = srcFlags.gfxBltDirectWriteMisalignedMdDirty;

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
    // The ACE command stream is located first so that the DE CmdStream is at NumCmdStreams() - 1
    // and the CE CmdStream remains before the DE CmdStream.
    switch (cmdStreamIdx)
    {
    case 0:
        PAL_ASSERT(ImplicitGangedSubQueueCount() <= 1); // Only one ganged ACE supported currently!
        pStream = (ImplicitGangedSubQueueCount() > 0) ? m_pAceCmdStream : nullptr;
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
    m_graphicsState.vrsRateState             = rateParams;
    m_graphicsState.dirtyFlags.vrsRateParams = 1;
}

// =====================================================================================================================
// Record the VRS center state structure so RPM has a copy for save / restore purposes.
void UniversalCmdBuffer::CmdSetVrsCenterState(
    const VrsCenterState&  centerState)
{
    // Record the state so that we can restore it after RPM operations.
    m_graphicsState.vrsCenterState            = centerState;
    m_graphicsState.dirtyFlags.vrsCenterState = 1;
}

// =====================================================================================================================
// Probably setup dirty state here...  indicate that draw time potentially has a lot to do.
void UniversalCmdBuffer::CmdBindSampleRateImage(
    const IImage*  pImage)
{
    // Binding a NULL image is always ok; otherwise, verify that the HW supports VRS images.
    PAL_ASSERT((pImage == nullptr) || (m_device.Parent()->ChipProperties().imageProperties.vrsTileSize.width != 0));

    m_graphicsState.pVrsImage = static_cast<const Image*>(pImage);
    m_graphicsState.dirtyFlags.vrsImage = 1;
}

} // Pm4
} // Pal
