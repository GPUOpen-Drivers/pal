/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/pm4UniversalCmdBuffer.h"
#include "g_gfx6Settings.h"
#include "core/hw/gfxip/gfx6/gfx6CmdStream.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6ColorBlendState.h"
#include "core/hw/gfxip/gfx6/gfx6ColorTargetView.h"
#include "core/hw/gfxip/gfx6/gfx6ComputePipeline.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6GraphicsPipeline.h"
#include "core/hw/gfxip/gfx6/gfx6MsaaState.h"
#include "core/hw/gfxip/gfx6/gfx6WorkaroundState.h"

#include <limits.h>

using namespace Util;

namespace Pal
{
namespace Gfx6
{

// =====================================================================================================================
WorkaroundState::WorkaroundState(
    const Device* pDevice,
    bool          isNested)
    :
    m_device(*pDevice),
    m_cmdUtil(pDevice->CmdUtil()),
    m_settings(pDevice->Settings()),
    m_isNested(isNested),
    m_dccOverwriteCombinerDisableMask(0)
    , m_multiPrimRestartIndexType(IndexType::Count)
{
}

// =====================================================================================================================
// Performs pre-draw validation specifically for hardware workarounds which must be evaluated at draw-time.
// Returns the next unused DWORD in pCmdSpace.
template <bool indirect, bool stateDirty>
uint32* WorkaroundState::PreDraw(
    const Pm4::GraphicsState&    gfxState,        // Currently-active Command Buffer Graphics state
    CmdStream&                   deCmdStream,     // DE command stream
    regIA_MULTI_VGT_PARAM        iaMultiVgtParam, // The value of IA_MULTI_VGT_PARAM that this draw will use.
    const Pm4::ValidateDrawInfo& drawInfo,        // Draw info
    uint32*                      pCmdSpace)
{
    const auto*const pPipeline   = static_cast<const GraphicsPipeline*>(gfxState.pipelineState.pPipeline);
    const auto*const pBlendState = static_cast<const ColorBlendState*>(gfxState.pColorBlendState);
    const auto*const pMsaaState  = static_cast<const MsaaState*>(gfxState.pMsaaState);

    // We better have a pipeline bound if we're doing pre-draw workarounds.
    PAL_ASSERT(pPipeline != nullptr);

    if (m_device.WaMiscGsRingOverflow())
    {
        // Apply the "GS Ring Overflow" workaround:
        // 4-SE Sea Islands parts (i.e., Hawaii) have a VGT timing problem where the hardware counters for the
        // ES/GS and GS/VS ring pointers can overflow causing the rings to read and write from/to whatever is in
        // memory following the ring allocations. This overflow scenario is rare and only occurs when the following
        // is true:
        //   o Offchip GS rendering is enabled for this Pipeline;
        //   o Draw is multi-instanced with a single primitive per-instance;
        //   o IA_MULTI_VGT_PARAM::SWITCH_ON_EOI is enabled.
        //
        //  The workaround suggested by the VGT folks is to issue a VGT_FLUSH event before any draw which could
        //  trigger the overflow scenario. Unfortunately this also includes indirect draws because we cannot know
        //  the vertex and instance counts and have to err on the safe side.

        const uint32 patchControlPoints = pPipeline->VgtLsHsConfig().bits.HS_NUM_INPUT_CP;
        const uint32 vertsPerPrim    = GfxDevice::VertsPerPrimitive(gfxState.inputAssemblyState.topology,
                                                                    patchControlPoints);
        const bool   singlePrimitive = (drawInfo.vtxIdxCount <= vertsPerPrim);
        const bool   multiInstance   = (drawInfo.instanceCount > 1);

        if (pPipeline->IsGsEnabled() &&
            (iaMultiVgtParam.bits.SWITCH_ON_EOI == 1) &&
            (indirect || (singlePrimitive && multiInstance)))
        {
            pCmdSpace += m_cmdUtil.BuildEventWrite(VGT_FLUSH, pCmdSpace);
        }
    }

    const bool targetsDirty = gfxState.dirtyFlags.validationBits.colorTargetView ||
                              gfxState.dirtyFlags.validationBits.colorBlendState;

    const bool ocDisableWorkaroundsActive = m_device.WaMiscDccOverwriteComb() ||
                                            m_settings.waRotatedSwizzleDisablesOverwriteCombiner;

    // the pipeline is only dirty if it is in fact dirty and the setting that is affected by a dirty
    // pipeline is active.
    const bool ocDisableLogicOp = m_settings.waLogicOpDisablesOverwriteCombiner;
    const bool pipelineDirty    =  ocDisableLogicOp &&
                                   stateDirty       &&
                                   gfxState.pipelineState.dirtyFlags.pipeline;

    if (pipelineDirty  ||
        (stateDirty && targetsDirty && ocDisableWorkaroundsActive))
    {
        // Apply the "Color Cache Controller Can Evict Invalid Sectors" workaround:
        // When MSAA and blending are enabled with DCC, the overwrite combiner marks something as overwritten
        // even if there are ensuing quads to the same sector that need dest. The workaround is to disable the
        // overwrite combiner.
        // HW team suggested WA:
        //  For MRT in 0 to 7:
        //   if( CB_COLOR[MRT]_ATTRIB.NUM_FRAGMENTS>0 && CB_BLEND[MRT]_CONTROL.ENABLE==1 )
        //    CB_COLOR[MRT]_DCC_CONTROL.OVERWRITE_COMBINER_DISABLE=1
        //   else
        //    CB_COLOR[MRT]_DCC_CONTROL.OVERWRITE_COMBINER_DISABLE=0

        PAL_ASSERT(m_device.Parent()->ChipProperties().gfxLevel >= GfxIpLevel::GfxIp8);

        // PAL requires pMsaaState->NumShaderExportMaskSamples() to be larger or equal to the max fragment count in all
        // the render targets. Therefore using pMsaaState->NumShaderExportMaskSamples() for this WA is safe,
        // though it may overly apply the WA if pMsaaState->NumShaderExportMaskSamples() > 1 but fragment count is 1.
        const bool   isMsaaTarget    = ((pMsaaState != nullptr) && (pMsaaState->NumShaderExportMaskSamples() > 1));
        const uint32 blendEnableMask = ((pBlendState != nullptr) && pBlendState->BlendEnableMask());

        if (m_isNested == false)
        {
            const BindTargetParams& bindInfo = gfxState.bindTargets;
            for (uint32 slot = 0; slot < bindInfo.colorTargetCount; ++slot)
            {
                const auto*const pView =
                    static_cast<const ColorTargetView*>(bindInfo.colorTargets[slot].pColorTargetView);
                regCB_COLOR0_DCC_CONTROL__VI cbDccControl = { };

                if (pView != nullptr)
                {
                    const bool  rop3Enabled     = (ocDisableLogicOp && (pPipeline->GetLogicOp() != LogicOp::Copy));
                    const bool  blendingEnabled = (((blendEnableMask >> slot) & 1) != 0);
                    if (isMsaaTarget &&
                        (rop3Enabled || blendingEnabled)  &&
                        (pView->IsDccEnabled(bindInfo.colorTargets[slot].imageLayout)))
                    {
                        cbDccControl.bits.OVERWRITE_COMBINER_DISABLE = 1;
                    }
                    else if (pView->IsRotatedSwizzleOverwriteCombinerDisabled() == 1)
                    {
                        cbDccControl.bits.OVERWRITE_COMBINER_DISABLE = 1;
                    }
                }

                if (cbDccControl.bits.OVERWRITE_COMBINER_DISABLE !=
                    ((m_dccOverwriteCombinerDisableMask >> slot) & 1))
                {
                    pCmdSpace += m_cmdUtil.BuildContextRegRmw(
                        mmCB_COLOR0_DCC_CONTROL__VI + (slot * CbRegsPerSlot),
                        CB_COLOR0_DCC_CONTROL__OVERWRITE_COMBINER_DISABLE_MASK__VI,
                        cbDccControl.u32All,
                        pCmdSpace);

                    m_dccOverwriteCombinerDisableMask |= (1 << slot);
                }
            }
        }
        else
        {
            // In nested command buffer, if client set the colorTargetView in inheritedStateMask, client
            // must have provided valid target view information about target count and per target sample count
            // that can be used for this WA
            if (gfxState.inheritedState.stateFlags.targetViewState == 1)
            {
                for (uint32 slot = 0; slot < gfxState.inheritedState.colorTargetCount; ++slot)
                {
                    const bool isMsaaSurface   = (gfxState.inheritedState.sampleCount[slot] > 1);
                    const bool blendingEnabled = (((blendEnableMask >> slot) & 1) != 0);
                    const bool rop3Enabled     = (ocDisableLogicOp && (pPipeline->GetLogicOp() != LogicOp::Copy));

                    regCB_COLOR0_DCC_CONTROL__VI cbDccControl = {};
                    if (isMsaaTarget && (blendingEnabled || rop3Enabled))
                    {
                        cbDccControl.bits.OVERWRITE_COMBINER_DISABLE = 1;
                    }
                    else if (m_settings.waRotatedSwizzleDisablesOverwriteCombiner)
                    {
                        // When a nested command buffer inherits the bound color-targets from the caller,
                        // the command buffer itself doesn't know whether the active targets use Rotated swizzle
                        // or not. This means we need to be conservative and disable the DCC overwrite combiner
                        // just to be safe.
                        cbDccControl.bits.OVERWRITE_COMBINER_DISABLE = 1;
                    }

                    if (cbDccControl.bits.OVERWRITE_COMBINER_DISABLE !=
                        ((m_dccOverwriteCombinerDisableMask >> slot) & 1))
                    {
                        pCmdSpace += m_cmdUtil.BuildContextRegRmw(
                            mmCB_COLOR0_DCC_CONTROL__VI + (slot * CbRegsPerSlot),
                            CB_COLOR0_DCC_CONTROL__OVERWRITE_COMBINER_DISABLE_MASK__VI,
                            cbDccControl.u32All,
                            pCmdSpace);
                        m_dccOverwriteCombinerDisableMask |= (1 << slot);
                    }
                }
            }
            else
            {
                // Nested command buffers aren't guaranteed to know the state of the actively bound color-target
                // views, so we need to be conservative and assume that all bound views are susceptible to the
                // hardware issue.
                const bool  rop3Enabled          = (ocDisableLogicOp && (pPipeline->GetLogicOp() != LogicOp::Copy));
                const uint32 combinerDisableMask = (isMsaaTarget ? (blendEnableMask || rop3Enabled) : 0);

                for (uint32 slot = 0; slot < MaxColorTargets; ++slot)
                {
                    regCB_COLOR0_DCC_CONTROL__VI cbDccControl = {};
                    cbDccControl.bits.OVERWRITE_COMBINER_DISABLE =
                        ((combinerDisableMask >> slot) & 1) ||
                        m_settings.waRotatedSwizzleDisablesOverwriteCombiner;

                    if (cbDccControl.bits.OVERWRITE_COMBINER_DISABLE !=
                        ((m_dccOverwriteCombinerDisableMask >> slot) & 1))
                    {
                        pCmdSpace += m_cmdUtil.BuildContextRegRmw(
                            mmCB_COLOR0_DCC_CONTROL__VI + (slot * CbRegsPerSlot),
                            CB_COLOR0_DCC_CONTROL__OVERWRITE_COMBINER_DISABLE_MASK__VI,
                            cbDccControl.u32All,
                            pCmdSpace);
                    }
                }
                m_dccOverwriteCombinerDisableMask = combinerDisableMask;
            }
        }
    }

    // On Gfx6/7, VGT compares the value of VGT_MULTI_PRIM_IB_RESET_INDX directly with the vertex index. For 16-bit
    // indices, the high 16-bits will always be 0s which means that comparing it against a primitive restart index
    // of 0xffffffff will never succeed.  Whenever the primitive restart value or the index type changes, we need
    // to patch the value of this register by masking out the bits outside of the range of possible index values.
    //
    // DX12 doesn't need to employ this workaround because their spec requires that the pipeline's index buffer
    // reset index always matches the active index buffer type. Mantle doesn't need the workaround either, because
    // Mantle doesn't support the primitive restart index feature.
    if (m_device.WaVgtPrimResetIndxMaskByType() &&
        gfxState.inputAssemblyState.primitiveRestartEnable &&
        (gfxState.pipelineState.dirtyFlags.pipeline ||                 // Primitive restart value has changed
         (gfxState.iaState.indexType != m_multiPrimRestartIndexType))) // Index type has changed
    {
        static_assert(
            static_cast<uint32>(IndexType::Count) == 3 &&
            static_cast<uint32>(IndexType::Idx8)  == 0 &&
            static_cast<uint32>(IndexType::Idx16) == 1 &&
            static_cast<uint32>(IndexType::Idx32) == 2,
            "Index type enum has changed; make sure to update the lookup table below");

        constexpr uint32 IndexTypeValidMask[] =
        {
            UCHAR_MAX,  // IndexType::Idx8
            USHRT_MAX,  // IndexType::Idx16
            UINT_MAX,   // IndexType::Idx32
        };

        m_multiPrimRestartIndexType = gfxState.iaState.indexType;

        regVGT_MULTI_PRIM_IB_RESET_INDX primIdx;
        primIdx.u32All = 0;
        primIdx.bits.RESET_INDX = gfxState.inputAssemblyState.primitiveRestartIndex &
                                  IndexTypeValidMask[static_cast<uint32>(m_multiPrimRestartIndexType)];

        pCmdSpace = deCmdStream.WriteSetOneContextReg(mmVGT_MULTI_PRIM_IB_RESET_INDX,
                                                      primIdx.u32All,
                                                      pCmdSpace);
    }

    return pCmdSpace;
}

// Instantiate the template for the linker.
template
uint32* WorkaroundState::PreDraw<true, false>(
    const Pm4::GraphicsState&    gfxState,        // Currently-active Command Buffer Graphics state
    CmdStream&                   deCmdStream,     // DE command stream
    Chip::regIA_MULTI_VGT_PARAM  iaMultiVgtParam, // The value of IA_MULTI_VGT_PARAM that this draw will use.
    const Pm4::ValidateDrawInfo& drawInfo,        // Draw info
    uint32*                      pCmdSpace);

template
uint32* WorkaroundState::PreDraw<false, false>(
    const Pm4::GraphicsState&    gfxState,        // Currently-active Command Buffer Graphics state
    CmdStream&                   deCmdStream,     // DE command stream
    Chip::regIA_MULTI_VGT_PARAM  iaMultiVgtParam, // The value of IA_MULTI_VGT_PARAM that this draw will use.
    const Pm4::ValidateDrawInfo& drawInfo,        // Draw info
    uint32*                      pCmdSpace);

template
uint32* WorkaroundState::PreDraw<true, true>(
    const Pm4::GraphicsState&    gfxState,        // Currently-active Command Buffer Graphics state
    CmdStream&                   deCmdStream,     // DE command stream
    Chip::regIA_MULTI_VGT_PARAM  iaMultiVgtParam, // The value of IA_MULTI_VGT_PARAM that this draw will use.
    const Pm4::ValidateDrawInfo& drawInfo,        // Draw info
    uint32*                      pCmdSpace);

template
uint32* WorkaroundState::PreDraw<false, true>(
    const Pm4::GraphicsState&    gfxState,        // Currently-active Command Buffer Graphics state
    CmdStream&                   deCmdStream,     // DE command stream
    Chip::regIA_MULTI_VGT_PARAM  iaMultiVgtParam, // The value of IA_MULTI_VGT_PARAM that this draw will use.
    const Pm4::ValidateDrawInfo& drawInfo,        // Draw info
    uint32*                      pCmdSpace);

// =====================================================================================================================
// Performs post-draw validation specifically for hardware workarounds which must be evaluated immediately following a
// draw.  Returns the next unused DWORD in pCmdSpace.
uint32* WorkaroundState::PostDraw(
    const Pm4::GraphicsState& gfxState,     // Currently-active Command Buffer Graphics state
    uint32*                   pCmdSpace)
{
    const auto*const pPipeline = static_cast<const GraphicsPipeline*>(gfxState.pipelineState.pPipeline);

    // We better have a pipeline bound if we're doing post-draw workarounds.
    PAL_ASSERT(pPipeline != nullptr);

    if (m_device.WaMiscVsBackPressure())
    {
        // Apply the "VS Back Pressure" workaround:
        // 4-SE Gfx 7/8 parts (i.e., Hawaii, Tonga, etc.) have a potential hang condition following a draw packet with
        // stream-output enabled: all VGT's will hang, waiting for a streamout interface transfer signal. The workaround
        // is to send a VGT_STREAMOUT_SYNC event after any draw in which stream-output is enabled.
        if (pPipeline->UsesStreamOut())
        {
            pCmdSpace += m_cmdUtil.BuildEventWrite(VGT_STREAMOUT_SYNC, pCmdSpace);
        }
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Resets the workaround state (to be called by the parent command buffer when a new command buffer is begun).
void WorkaroundState::Reset()
{
    m_dccOverwriteCombinerDisableMask = 0;
    m_multiPrimRestartIndexType = IndexType::Count;
}

// =====================================================================================================================
// Leaks nested command buffer state from a given child command buffer to this workaround state.
void WorkaroundState::LeakNestedCmdBufferState(const WorkaroundState& workaroundState)
{
    m_dccOverwriteCombinerDisableMask = workaroundState.m_dccOverwriteCombinerDisableMask;
    m_multiPrimRestartIndexType = workaroundState.m_multiPrimRestartIndexType;
}

} // Gfx6
} // Pal
