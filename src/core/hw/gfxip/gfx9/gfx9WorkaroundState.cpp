/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
    const UniversalCmdBufferState& universalState)
    :
    m_device(*pDevice),
    m_cmdUtil(pDevice->CmdUtil()),
    m_settings(pDevice->Settings()),
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
    if (m_settings.waVgtFlushNggToLegacyGs && nextPipelineUsesGs)
    {
        //  GE has a bug where a legacy GS draw following an NGG draw can cause the legacy GS draw to interfere with
        //  pending NGG primitives, causing the GE to drop the pending NGG primitives and eventually lead to a hang.
        //  The suggested workaround is to create a bubble for the GE. Since determining the necessary size of this
        //  bubble is workload dependent, it is safer to issue a VGT_FLUSH between this transition.
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
    if (m_settings.waIndexBufferZeroSize && (*pIndexCount == 0))
    {
        // The GE has a bug where attempting to use an index buffer of size zero can cause a hang.
        // The workaround is to bind an internal index buffer of a single entry and force the index buffer size to one.
        uint32* pNewIndexBuffer = pCmdBuffer->CmdAllocateEmbeddedData(1, 1, pIndexBufferAddr);
        pNewIndexBuffer[0]      = 0;
        *pIndexCount            = 1;
    }
}

// =====================================================================================================================
void WorkaroundState::HandleFirstIndexSmallerThanIndexCount(
    uint32*         pFirstIndex,
    const uint32    indexCount
    ) const
{
    if (*pFirstIndex >= indexCount)
    {
        // The caller (UniversalCmdBuffer::CmdDrawIndexed) request pFirstIndex to be no greater than indexCount.
        if (m_settings.waIndexBufferZeroSize)
        {
            // In Gfx10 there is a hardware bug (see settings.waIndexBufferZeroSize),
            // In the event that this workaround is active, we need to modify "pFirstIndex" as "indexCount - 1",
            // so the caller can set the maxSize / validIndexCount to 1.
            *pFirstIndex = indexCount - 1;
        }
        else
        {
            // Modify the "pFirstIndex" to be "indexCount", so the caller can clamp the "validIndexCount" to 0.
            *pFirstIndex = indexCount;
        }
    }
}

// =====================================================================================================================
// Performs pre-draw validation specifically for hardware workarounds which must be evaluated at draw-time.
// Returns the next unused DWORD in pCmdSpace.
template <bool indirect, bool stateDirty, bool pm4OptImmediate>
uint32* WorkaroundState::PreDraw(
    const GraphicsState&    gfxState,        // Currently-active Command Buffer Graphics state
    CmdStream*              pDeCmdStream,    // DE command stream
    UniversalCmdBuffer*     pCmdBuffer,
    const ValidateDrawInfo& drawInfo,        // Draw info
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
    const bool pipelineDirty = m_settings.waLogicOpDisablesOverwriteCombiner &&
                               stateDirty                                    &&
                               gfxState.pipelineState.dirtyFlags.pipelineDirty;

    // colorBlendWorkaoundsActive will be true if the state of the view and / or blend state
    // is important.
    const bool colorBlendWorkaroundsActive = m_settings.waColorCacheControllerInvalidEviction ||
                                             m_settings.waRotatedSwizzleDisablesOverwriteCombiner;

    const bool targetsDirty = dirtyFlags.validationBits.colorTargetView ||
                              dirtyFlags.validationBits.colorBlendState;

    // If the pipeline is dirty and it matters, then we have to look at all the bound targets
    if (pipelineDirty  ||
        // Otherwise, if the view and/or blend states are important, look at all the bound targets
        (colorBlendWorkaroundsActive && stateDirty && targetsDirty))
    {
        for (uint32  cbIdx = 0; cbIdx < gfxState.bindTargets.colorTargetCount; cbIdx++)
        {
            const auto& colorTarget = gfxState.bindTargets.colorTargets[cbIdx];
            const auto* pView       = static_cast<const ColorTargetView*>(colorTarget.pColorTargetView);

            if (pView != nullptr)
            {
                const auto* pGfxImage = pView->GetImage();

                // pGfxImage will be NULL for buffer views...
                if (pGfxImage != nullptr)
                {
                    const auto* pPalImage       = pGfxImage->Parent();
                    const auto& createInfo      = pPalImage->GetImageCreateInfo();
                    const bool  rop3Enabled     = (m_settings.waLogicOpDisablesOverwriteCombiner &&
                                                   (pPipeline->GetLogicOp() != LogicOp::Copy));
                    const bool  blendingEnabled = ((pBlendState != nullptr) && pBlendState->IsBlendEnabled(cbIdx));

                    regCB_COLOR0_DCC_CONTROL cbColorDccControl = {};

                    // if ( (blending or rop3) && (MSAA or EQAA) && dcc_enabled )
                    //     CB_COLOR<n>_DCC_CONTROL.OVERWRITE_COMBINER_DISABLE = 1;
                    if ((rop3Enabled || blendingEnabled) &&
                        (createInfo.fragments > 1)       &&
                        pGfxImage->HasDccData())
                    {
                        cbColorDccControl.bits.OVERWRITE_COMBINER_DISABLE = 1;
                    }
                    else if (pView->IsRotatedSwizzleOverwriteCombinerDisabled())
                    {
                        cbColorDccControl.bits.OVERWRITE_COMBINER_DISABLE = 1;
                    }

                    pCmdSpace = pDeCmdStream->WriteContextRegRmw<pm4OptImmediate>(
                        mmCB_COLOR0_DCC_CONTROL + (cbIdx * CbRegsPerSlot),
                        CB_COLOR0_DCC_CONTROL__OVERWRITE_COMBINER_DISABLE_MASK,
                        cbColorDccControl.u32All,
                        pCmdSpace);
                }
            }
        }
    }

    bool setPopsDrainPsOnOverlap = false;

    if (m_settings.waMiscPopsMissedOverlap && stateDirty && pPipeline->PsUsesRovs())
    {
        setPopsDrainPsOnOverlap = ((pMsaaState != nullptr) &&
                                   (pMsaaState->Log2NumSamples() >= 3));

        if ((pDepthTargetView != nullptr) && (pDepthTargetView->GetImage() != nullptr))
        {
            const auto& imageCreateInfo = pDepthTargetView->GetImage()->Parent()->GetImageCreateInfo();

            if (imageCreateInfo.samples >= 8)
            {
                setPopsDrainPsOnOverlap = true;
            }
        }
    }

    if (setPopsDrainPsOnOverlap)
    {
        regDB_DFSM_CONTROL dbDfsmControl = {};
        dbDfsmControl.bits.POPS_DRAIN_PS_ON_OVERLAP = 1;

        pCmdSpace = pDeCmdStream->WriteContextRegRmw<pm4OptImmediate>(
            m_device.CmdUtil().GetRegInfo().mmDbDfsmControl,
            Core::DB_DFSM_CONTROL__POPS_DRAIN_PS_ON_OVERLAP_MASK,
            dbDfsmControl.u32All,
            pCmdSpace);
    }

    // setPopsDrainPsOnOverlap should effectively be false for all Gfx10 products, but adding it here just in case.
    if (m_settings.waStalledPopsMode &&
        stateDirty                   &&
        pPipeline->PsUsesRovs()      &&
        (setPopsDrainPsOnOverlap || m_settings.drainPsOnOverlap))
    {
        regDB_RENDER_OVERRIDE2 dbRenderOverride2 = {};
        dbRenderOverride2.bits.PARTIAL_SQUAD_LAUNCH_CONTROL = PSLC_ON_HANG_ONLY;

        pCmdSpace = pDeCmdStream->WriteContextRegRmw<pm4OptImmediate>(
            mmDB_RENDER_OVERRIDE2,
            DB_RENDER_OVERRIDE2__PARTIAL_SQUAD_LAUNCH_CONTROL_MASK,
            dbRenderOverride2.u32All,
            pCmdSpace);
    }

    // If legacy tessellation is active and the fillmode is set to wireframe, the workaround requires that vertex reuse
    // is disabled to avoid corruption. It is expected that we should rarely hit this case.
    // Since we should rarely hit this and to keep this "simple," we won't handle the case where a legacy tessellation
    // pipeline is bound and fillMode goes from Wireframe to NOT wireframe.
    if (stateDirty                                               &&
        m_settings.waTessIncorrectRelativeIndex                  &&
        (gfxState.pipelineState.dirtyFlags.pipelineDirty ||
         gfxState.dirtyFlags.validationBits.triangleRasterState) &&
        pPipeline->IsTessEnabled()                               &&
        (pPipeline->IsNgg() == false)                            &&
        ((gfxState.triangleRasterState.frontFillMode == FillMode::Wireframe) ||
         (gfxState.triangleRasterState.backFillMode == FillMode::Wireframe)))
    {
        pCmdSpace = pDeCmdStream->WriteSetOneContextReg<pm4OptImmediate>(mmVGT_REUSE_OFF,
                                                                         VGT_REUSE_OFF__REUSE_OFF_MASK,
                                                                         pCmdSpace);
    }

    if (pPipeline->IsNggFastLaunch())
    {
        //  The IA has a mode which enables ping-pong algorithm at EOP distribution to balance for small draws.
        //  Unfortunately this mode does not support fast-launch draws of any kind. We must reset to the lowest VGT
        //  to prevent hangs.
        pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(RESET_TO_LOWEST_VGT, EngineTypeUniversal, pCmdSpace);
    }

    // This must go last in order to validate that no other context rolls can occur before the draw.
    if (pCmdBuffer->NeedsToValidateScissorRects(pm4OptImmediate))
    {
        pCmdSpace = pCmdBuffer->ValidateScissorRects(pCmdSpace);
    }

    return pCmdSpace;
}

// Instantiate the template for the linker.
template
uint32* WorkaroundState::PreDraw<true, false, false>(
    const GraphicsState&    gfxState,        // Currently-active Command Buffer Graphics state
    CmdStream*              pDeCmdStream,    // DE command stream
    UniversalCmdBuffer*     pCmdBuffer,
    const ValidateDrawInfo& drawInfo,        // Draw info
    uint32*                 pCmdSpace);

template
uint32* WorkaroundState::PreDraw<true, false, true>(
    const GraphicsState&    gfxState,        // Currently-active Command Buffer Graphics state
    CmdStream*              pDeCmdStream,    // DE command stream
    UniversalCmdBuffer*     pCmdBuffer,
    const ValidateDrawInfo& drawInfo,        // Draw info
    uint32*                 pCmdSpace);

template
uint32* WorkaroundState::PreDraw<false, false, false>(
    const GraphicsState&    gfxState,        // Currently-active Command Buffer Graphics state
    CmdStream*              pDeCmdStream,    // DE command stream
    UniversalCmdBuffer*     pCmdBuffer,
    const ValidateDrawInfo& drawInfo,        // Draw info
    uint32*                 pCmdSpace);

template
uint32* WorkaroundState::PreDraw<false, false, true>(
    const GraphicsState&    gfxState,        // Currently-active Command Buffer Graphics state
    CmdStream*              pDeCmdStream,    // DE command stream
    UniversalCmdBuffer*     pCmdBuffer,
    const ValidateDrawInfo& drawInfo,        // Draw info
    uint32*                 pCmdSpace);

template
uint32* WorkaroundState::PreDraw<true, true, false>(
    const GraphicsState&    gfxState,        // Currently-active Command Buffer Graphics state
    CmdStream*              pDeCmdStream,    // DE command stream
    UniversalCmdBuffer*     pCmdBuffer,
    const ValidateDrawInfo& drawInfo,        // Draw info
    uint32*                 pCmdSpace);

template
uint32* WorkaroundState::PreDraw<true, true, true>(
    const GraphicsState&    gfxState,        // Currently-active Command Buffer Graphics state
    CmdStream*              pDeCmdStream,    // DE command stream
    UniversalCmdBuffer*     pCmdBuffer,
    const ValidateDrawInfo& drawInfo,        // Draw info
    uint32*                 pCmdSpace);

template
uint32* WorkaroundState::PreDraw<false, true, false>(
    const GraphicsState&    gfxState,        // Currently-active Command Buffer Graphics state
    CmdStream*              pDeCmdStream,    // DE command stream
    UniversalCmdBuffer*     pCmdBuffer,
    const ValidateDrawInfo& drawInfo,        // Draw info
    uint32*                 pCmdSpace);

template
uint32* WorkaroundState::PreDraw<false, true, true>(
    const GraphicsState&    gfxState,        // Currently-active Command Buffer Graphics state
    CmdStream*              pDeCmdStream,    // DE command stream
    UniversalCmdBuffer*     pCmdBuffer,
    const ValidateDrawInfo& drawInfo,        // Draw info
    uint32*                 pCmdSpace);

} // Gfx9
} // Pal
