/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/universalCmdBuffer.h"
#include "core/hw/gfxip/gfx9/g_gfx9PalSettings.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9ColorBlendState.h"
#include "core/hw/gfxip/gfx9/gfx9ColorTargetView.h"
#include "core/hw/gfxip/gfx9/gfx9DepthStencilState.h"
#include "core/hw/gfxip/gfx9/gfx9DepthStencilView.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9GraphicsPipeline.h"
#include "core/hw/gfxip/gfx9/gfx9Image.h"
#include "core/hw/gfxip/gfx9/gfx9MsaaState.h"
#include "core/hw/gfxip/gfx9/gfx9UniversalCmdBuffer.h"
#include "core/hw/gfxip/gfx9/gfx9WorkaroundState.h"

#include <limits.h>

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
WorkaroundState::WorkaroundState(
    const Device*                  pDevice,
    bool                           isNested,
    const UniversalCmdBufferState& universalState,
    const CachedSettings&          cachedSettings)
    :
    m_device(*pDevice),
    m_cmdUtil(pDevice->CmdUtil()),
    m_cachedSettings(cachedSettings),
    m_isNested(isNested),
    m_universalState(universalState)
{
}

// =====================================================================================================================
uint32* WorkaroundState::SwitchFromNggPipelineToLegacy(
    bool    nextPipelineUsesGs,
    uint32* pCmdSpace
    ) const
{
    if (m_cachedSettings.waVgtFlushNggToLegacy || (m_cachedSettings.waVgtFlushNggToLegacyGs && nextPipelineUsesGs))
    {
        //  GE has a bug where a legacy GS draw following an NGG draw can cause the legacy GS draw to interfere with
        //  pending NGG primitives, causing the GE to drop the pending NGG primitives and eventually lead to a hang.
        //  The suggested workaround is to create a bubble for the GE. Since determining the necessary size of this
        //  bubble is workload dependent, it is safer to issue a VGT_FLUSH between this transition.
        //  GE has a second bug with the same software workaround. A legacy draw following an NGG draw will cause GE to
        //  internally transition from NGG to legacy prematurely. This leads to GE sending the enable legacy event to
        //  only some PAs on legacy path, and SC is left waiting for events from the others. Issuing a VGT_FLUSH
        //  prevents this from happening.
        pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(VGT_FLUSH, EngineTypeUniversal, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
uint32* WorkaroundState::SwitchBetweenLegacyPipelines(
    bool                    oldPipelineUsesGs,
    uint32                  oldCutMode,
    const GraphicsPipeline* pNewPipeline,
    uint32*                 pCmdSpace
    ) const
{
    if (m_cachedSettings.waLegacyGsCutModeFlush            &&
        (oldPipelineUsesGs && pNewPipeline->IsGsEnabled()) &&
        (oldCutMode != pNewPipeline->VgtGsMode().bits.CUT_MODE))
    {
        pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(VGT_FLUSH, EngineTypeUniversal, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
void WorkaroundState::HandleZeroIndexBuffer(
    UniversalCmdBuffer* pCmdBuffer,
    gpusize*            pIndexBufferAddr,
    uint32*             pIndexCount)
{
    if (m_cachedSettings.waIndexBufferZeroSize && (*pIndexCount == 0))
    {
        // The GE has a bug where attempting to use an index buffer of size zero can cause a hang.
        // The workaround is to bind an internal index buffer of a single entry and force the index buffer size to one.
        uint32* pNewIndexBuffer = pCmdBuffer->CmdAllocateEmbeddedData(1, 1, pIndexBufferAddr);
        pNewIndexBuffer[0]      = 0;
        *pIndexCount            = 1;
    }
}

// =====================================================================================================================
// Performs pre-draw validation specifically for hardware workarounds which must be evaluated at draw-time.
// Returns the next unused DWORD in pCmdSpace.
template <bool PipelineDirty, bool StateDirty, bool Pm4OptImmediate>
uint32* WorkaroundState::PreDraw(
    const GraphicsState&    gfxState,        // Currently-active Command Buffer Graphics state
    CmdStream*              pDeCmdStream,    // DE command stream
    UniversalCmdBuffer*     pCmdBuffer,
    uint32*                 pCmdSpace)
{
    const auto&      dirtyFlags       = gfxState.dirtyFlags;
    const auto*const pBlendState      = static_cast<const ColorBlendState*>(gfxState.pColorBlendState);
    const auto*const pMsaaState       = static_cast<const MsaaState*>(gfxState.pMsaaState);
    const auto*const pDepthTargetView =
        static_cast<const DepthStencilView*>(gfxState.bindTargets.depthTarget.pDepthStencilView);
    const auto*const pPipeline        = static_cast<const GraphicsPipeline*>(gfxState.pipelineState.pPipeline);

    // the pipeline is only dirty if it is in fact dirty and the setting that is affected by a dirty
    // pipeline is active.
    const bool pipelineDirty = m_cachedSettings.waLogicOpDisablesOverwriteCombiner &&
                               PipelineDirty                                       &&
                               gfxState.pipelineState.dirtyFlags.pipelineDirty;

    // colorBlendWorkaoundsActive will be true if the state of the view and / or blend state
    // is important.
    const bool colorBlendWorkaroundsActive = m_cachedSettings.waColorCacheControllerInvalidEviction ||
                                             m_cachedSettings.waRotatedSwizzleDisablesOverwriteCombiner;

    const bool targetsDirty = dirtyFlags.validationBits.colorTargetView ||
                              dirtyFlags.validationBits.colorBlendState;

    // If the pipeline is dirty and it matters, then we have to look at all the bound targets
    if (pipelineDirty  ||
        // Otherwise, if the view and/or blend states are important, look at all the bound targets
        (colorBlendWorkaroundsActive && StateDirty && targetsDirty))
    {
        for (uint32  cbIdx = 0; cbIdx < gfxState.bindTargets.colorTargetCount; cbIdx++)
        {
            const auto& colorTarget = gfxState.bindTargets.colorTargets[cbIdx];
            const auto* pView       = static_cast<const ColorTargetView*>(colorTarget.pColorTargetView);

            if (pView != nullptr)
            {
                const bool viewCanNeedWa =
                    (pView->HasMultipleFragments() && pView->HasDcc()) ||
                    pView->IsRotatedSwizzleOverwriteCombinerDisabled();

                // Macro check if the view can possibly need the WA so we avoid it in many cases
                if (viewCanNeedWa)
                {
                    const bool rop3Enabled     = (m_cachedSettings.waLogicOpDisablesOverwriteCombiner &&
                                                  (pPipeline->GetLogicOp() != LogicOp::Copy));
                    const bool blendingEnabled = ((pBlendState != nullptr) && pBlendState->IsBlendEnabled(cbIdx));

                    regCB_COLOR0_DCC_CONTROL cbColorDccControl = {};

                    // if ( (blending or rop3) && (MSAA or EQAA) && dcc_enabled )
                    //     CB_COLOR<n>_DCC_CONTROL.OVERWRITE_COMBINER_DISABLE = 1;
                    if ((rop3Enabled || blendingEnabled) &&
                        pView->HasMultipleFragments()    &&
                        pView->HasDcc())
                    {
                        cbColorDccControl.gfx09_10.OVERWRITE_COMBINER_DISABLE = 1;
                    }
                    else if (pView->IsRotatedSwizzleOverwriteCombinerDisabled())
                    {
                        cbColorDccControl.gfx09_10.OVERWRITE_COMBINER_DISABLE = 1;
                    }

                    pCmdSpace = pDeCmdStream->WriteContextRegRmw<Pm4OptImmediate>(
                        mmCB_COLOR0_DCC_CONTROL + (cbIdx * CbRegsPerSlot),
                        Gfx09_10::CB_COLOR0_DCC_CONTROL__OVERWRITE_COMBINER_DISABLE_MASK,
                        cbColorDccControl.u32All,
                        pCmdSpace);
                }
            }
        }
    }

    if (m_cachedSettings.waMiscPopsMissedOverlap)
    {
        // This should be unreachable since waStalledPopsMode applies to GFX10 only and waMiscPopsMissedOverlap
        // applies to GFX9 only.
        PAL_ASSERT(m_device.Settings().waStalledPopsMode == false);

        if ((PipelineDirty && gfxState.pipelineState.dirtyFlags.pipelineDirty) ||
            (StateDirty && (dirtyFlags.validationBits.msaaState ||
                            dirtyFlags.validationBits.depthStencilView)))
        {
            bool setPopsDrainPsOnOverlap = false;

            if (pPipeline->PsUsesRovs())
            {
                if ((pMsaaState != nullptr) && (pMsaaState->Log2NumSamples() >= 3))
                {
                    setPopsDrainPsOnOverlap = true;
                }

                if ((pDepthTargetView != nullptr) && (pDepthTargetView->GetImage() != nullptr))
                {
                    const auto& imageCreateInfo = pDepthTargetView->GetImage()->Parent()->GetImageCreateInfo();

                    if (imageCreateInfo.samples >= 8)
                    {
                        setPopsDrainPsOnOverlap = true;
                    }
                }
            }

            // This WA is rarely needed - once it is applied during a CmdBuffer we need to start validating it.
            if (setPopsDrainPsOnOverlap || pCmdBuffer->HasWaMiscPopsMissedOverlapBeenApplied())
            {
                regDB_DFSM_CONTROL* pPrevDbDfsmControl = pCmdBuffer->GetDbDfsmControl();
                regDB_DFSM_CONTROL  dbDfsmControl      = *pPrevDbDfsmControl;

                dbDfsmControl.most.POPS_DRAIN_PS_ON_OVERLAP = ((setPopsDrainPsOnOverlap) ? 1 : 0);

                if (dbDfsmControl.u32All != pPrevDbDfsmControl->u32All)
                {
                    pCmdSpace = pDeCmdStream->WriteSetOneContextRegNoOpt(m_cmdUtil.GetRegInfo().mmDbDfsmControl,
                                                                         dbDfsmControl.u32All,
                                                                         pCmdSpace);

                    pPrevDbDfsmControl->u32All = dbDfsmControl.u32All;
                }

                // Mark CmdBuffer bool that this WA has been applied, so we re-validate it for the remainder of the
                // CmdBuffer disabling it as necessary.
                pCmdBuffer->SetWaMiscPopsMissedOverlapHasBeenApplied();
            }
        }
    }

    // If legacy tessellation is active and the fillmode is set to wireframe, the workaround requires that vertex reuse
    // is disabled to avoid corruption. It is expected that we should rarely hit this case.
    // Since we should rarely hit this and to keep this "simple," we won't handle the case where a legacy tessellation
    // pipeline is bound and fillMode goes from Wireframe to NOT wireframe.
    if ((StateDirty || PipelineDirty)                       &&
        m_cachedSettings.waTessIncorrectRelativeIndex       &&
        (gfxState.pipelineState.dirtyFlags.pipelineDirty ||
         dirtyFlags.validationBits.triangleRasterState)     &&
        pPipeline->IsTessEnabled()                          &&
        (pPipeline->IsNgg() == false)                       &&
        ((gfxState.triangleRasterState.frontFillMode == FillMode::Wireframe) ||
         (gfxState.triangleRasterState.backFillMode == FillMode::Wireframe)))
    {
        pCmdSpace = pDeCmdStream->WriteSetOneContextReg<Pm4OptImmediate>(mmVGT_REUSE_OFF,
                                                                         VGT_REUSE_OFF__REUSE_OFF_MASK,
                                                                         pCmdSpace);
    }

    // This must go last in order to validate that no other context rolls can occur before the draw.
    if ((StateDirty && dirtyFlags.validationBits.scissorRects) ||
        pCmdBuffer->NeedsToValidateScissorRectsWa(Pm4OptImmediate))
    {
        pCmdSpace = pCmdBuffer->ValidateScissorRects(pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Disable instance packing while query pipeline statistics with *_ADJACENCY primitive and instance > 1.
template <bool Indirect>
bool WorkaroundState::DisableInstancePacking(
    PrimitiveTopology   topology,
    uint32              instanceCount,
    uint32              numActiveQueries
    ) const
{
    bool disableInstancePacking = false;

    if (m_cachedSettings.waDisableInstancePacking           &&
        (numActiveQueries != 0)                             &&
        ((instanceCount > 1) || Indirect)                   &&
        ((topology == PrimitiveTopology::LineListAdj)       ||
         (topology == PrimitiveTopology::LineStripAdj)      ||
         (topology == PrimitiveTopology::TriangleListAdj)   ||
         (topology == PrimitiveTopology::TriangleStripAdj)))
    {
        disableInstancePacking = true;
    }

    return disableInstancePacking;
}

template
bool WorkaroundState::DisableInstancePacking<true>(
    PrimitiveTopology   topology,
    uint32              instanceCount,
    uint32              numActiveQueries) const;

template
bool WorkaroundState::DisableInstancePacking <false>(
    PrimitiveTopology   topology,
    uint32              instanceCount,
    uint32              numActiveQueries) const;

// Instantiate the template for the linker.
template
uint32* WorkaroundState::PreDraw<false, false, false>(
    const GraphicsState&    gfxState,        // Currently-active Command Buffer Graphics state
    CmdStream*              pDeCmdStream,    // DE command stream
    UniversalCmdBuffer*     pCmdBuffer,
    uint32*                 pCmdSpace);

template
uint32* WorkaroundState::PreDraw<false, false, true>(
    const GraphicsState&    gfxState,        // Currently-active Command Buffer Graphics state
    CmdStream*              pDeCmdStream,    // DE command stream
    UniversalCmdBuffer*     pCmdBuffer,
    uint32*                 pCmdSpace);

template
uint32* WorkaroundState::PreDraw<false, true, false>(
    const GraphicsState&    gfxState,        // Currently-active Command Buffer Graphics state
    CmdStream*              pDeCmdStream,    // DE command stream
    UniversalCmdBuffer*     pCmdBuffer,
    uint32*                 pCmdSpace);

template
uint32* WorkaroundState::PreDraw<false, true, true>(
    const GraphicsState&    gfxState,        // Currently-active Command Buffer Graphics state
    CmdStream*              pDeCmdStream,    // DE command stream
    UniversalCmdBuffer*     pCmdBuffer,
    uint32*                 pCmdSpace);

template
uint32* WorkaroundState::PreDraw<true, false, false>(
    const GraphicsState&    gfxState,        // Currently-active Command Buffer Graphics state
    CmdStream*              pDeCmdStream,    // DE command stream
    UniversalCmdBuffer*     pCmdBuffer,
    uint32*                 pCmdSpace);

template
uint32* WorkaroundState::PreDraw<true, false, true>(
    const GraphicsState&    gfxState,        // Currently-active Command Buffer Graphics state
    CmdStream*              pDeCmdStream,    // DE command stream
    UniversalCmdBuffer*     pCmdBuffer,
    uint32*                 pCmdSpace);

template
uint32* WorkaroundState::PreDraw<true, true, false>(
    const GraphicsState&    gfxState,        // Currently-active Command Buffer Graphics state
    CmdStream*              pDeCmdStream,    // DE command stream
    UniversalCmdBuffer*     pCmdBuffer,
    uint32*                 pCmdSpace);

template
uint32* WorkaroundState::PreDraw<true, true, true>(
    const GraphicsState&    gfxState,        // Currently-active Command Buffer Graphics state
    CmdStream*              pDeCmdStream,    // DE command stream
    UniversalCmdBuffer*     pCmdBuffer,
    uint32*                 pCmdSpace);

} // Gfx9
} // Pal
