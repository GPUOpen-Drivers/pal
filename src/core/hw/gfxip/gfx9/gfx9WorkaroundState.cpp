/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
uint32* WorkaroundState::SwitchToNggPipeline(
    bool        isFirstDraw,
    bool        prevPipeIsNgg,
    bool        prevPipeUsesTess,
    bool        prevPipeUsesGs,
    bool        usesOffchipPc,
    CmdStream*  pDeCmdStream,
    uint32*     pCmdSpace
    ) const
{
    // When a transition from a legacy tessellation pipeline (GS disabled) to an NGG pipeline, the broadcast logic
    // to update the VGTs can be triggered at different times. This, coupled with back pressure in the SPI, can cause
    // delays in the RESET_TO_LOWEST_VGT and ENABLE_NGG_PIPELINE events from being seen. This will cause a hang.
    // NOTE: For non-nested command buffers, there is the potential that we could chain multiple command buffers
    //       together. In this scenario, we have no method of detecting what the previous command buffer's last bound
    //       pipeline is, so we have to assume the worst and insert this event.
    if (isFirstDraw || (prevPipeUsesTess && !prevPipeUsesGs))
    {
        // If we see hangs, this alert will draw our attention to this possible workaround.
        PAL_ALERT_ALWAYS();

        if (m_settings.waLegacyTessToNggVgtFlush)
        {
            pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(VGT_FLUSH, EngineTypeUniversal, pCmdSpace);
        }
    }

    // When we transition from a legacy pipeline to an NGG pipeline we need to send a VS_PARTIAL_FLUSH to avoid a hang.
    if (m_settings.waLegacyToNggVsPartialFlush && !usesOffchipPc && (isFirstDraw || !prevPipeIsNgg))
    {
        pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(VS_PARTIAL_FLUSH, EngineTypeUniversal, pCmdSpace);
    }

    return pCmdSpace;
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
    const auto& dirtyFlags    = gfxState.dirtyFlags;

    const auto*const pBlendState      = static_cast<const ColorBlendState*>(gfxState.pColorBlendState);
    const auto*const pMsaaState       = static_cast<const MsaaState*>(gfxState.pMsaaState);
    const auto*const pDepthTargetView =
        static_cast<const DepthStencilView*>(gfxState.bindTargets.depthTarget.pDepthStencilView);
    const auto*const pPipeline        = static_cast<const GraphicsPipeline*>(gfxState.pipelineState.pPipeline);

    if (m_settings.waColorCacheControllerInvalidEviction &&
        (stateDirty && (dirtyFlags.validationBits.colorTargetView || dirtyFlags.validationBits.colorBlendState)))
    {
        for (uint32  cbIdx = 0; cbIdx < gfxState.bindTargets.colorTargetCount; cbIdx++)
        {
            const auto& colorTarget = gfxState.bindTargets.colorTargets[cbIdx];
            const auto* pView       = static_cast<const ColorTargetView*>(colorTarget.pColorTargetView);

            if (pView != nullptr)
            {
                const auto* pGfxImage = pView->GetImage();

                if ((pGfxImage != nullptr) && (pGfxImage->HasDccData()))
                {
                    regCB_COLOR0_DCC_CONTROL cbColorDccControl = {};

                    if ((pBlendState != nullptr) && pBlendState->IsBlendEnabled(cbIdx))
                    {
                        const auto& createInfo = pGfxImage->Parent()->GetImageCreateInfo();

                        cbColorDccControl.bits.OVERWRITE_COMBINER_DISABLE = ((createInfo.fragments > 1) ? 1 : 0);
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
            DB_DFSM_CONTROL__POPS_DRAIN_PS_ON_OVERLAP_MASK,
            dbDfsmControl.u32All,
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
