/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/layers/dbgOverlay/dbgOverlayCmdBuffer.h"
#include "core/layers/dbgOverlay/dbgOverlayDevice.h"
#include "core/layers/dbgOverlay/dbgOverlayFpsMgr.h"
#include "core/layers/dbgOverlay/dbgOverlayImage.h"
#include "core/layers/dbgOverlay/dbgOverlayPlatform.h"
#include "core/layers/dbgOverlay/dbgOverlayTextWriter.h"
#include "core/layers/dbgOverlay/dbgOverlayTimeGraph.h"
#include "core/g_palPlatformSettings.h"

namespace Pal
{
namespace DbgOverlay
{

// =====================================================================================================================
CmdBuffer::CmdBuffer(
    ICmdBuffer*   pNextCmdBuffer,
    const Device* pDevice,
    QueueType     queueType)
    :
    CmdBufferFwdDecorator(pNextCmdBuffer, pDevice),
    m_device(*pDevice),
    m_queueType(queueType),
    m_containsPresent(false)
{
}

// =====================================================================================================================
Result CmdBuffer::Begin(
    const CmdBufferBuildInfo& info)
{
    m_containsPresent = false;

    return m_pNextLayer->Begin(NextCmdBufferBuildInfo(info));
}

// =====================================================================================================================
void CmdBuffer::CmdColorSpaceConversionCopy(
    const IImage&                     srcImage,
    ImageLayout                       srcImageLayout,
    const IImage&                     dstImage,
    ImageLayout                       dstImageLayout,
    uint32                            regionCount,
    const ColorSpaceConversionRegion* pRegions,
    TexFilter                         filter,
    const ColorSpaceConversionTable&  cscTable)
{
    const auto& settings = m_pDevice->GetPlatform()->PlatformSettings();

    // Draw the debug overlay before color conversion.
    if (settings.debugOverlayConfig.useDebugOverlayOnColorSpaceConversionCopy == true)
    {
        // Only an image supports visual confirm.
        if (Device::DetermineDbgOverlaySupport(m_queueType))
        {
            // Issue a barrier to ensure the text written via CS is complete and flushed out of L2.
            BarrierInfo barrier = {};
            barrier.waitPoint = HwPipePreCs;

            const HwPipePoint postCs = HwPipePostCs;
            barrier.pipePointWaitCount = 1;
            barrier.pPipePoints = &postCs;

            BarrierTransition transition = {};
            transition.srcCacheMask = CoherShader;
            transition.dstCacheMask = CoherShader;

            barrier.transitionCount = 1;
            barrier.pTransitions = &transition;

            m_device.GetTextWriter().WriteVisualConfirm(static_cast<const Image&>(srcImage),
                                                        this, PresentMode::Unknown);

            barrier.reason = Developer::BarrierReasonDebugOverlayText;

            CmdBarrier(barrier);
        }

        auto*const pFpsMgr = static_cast<Platform*>(m_device.GetPlatform())->GetFpsMgr();

        pFpsMgr->IncrementFrameCount();

        // Call UpdateFps and UpdateBenchmark now instead of at presentation time.
        pFpsMgr->UpdateFps();
        pFpsMgr->UpdateGpuFps();
        pFpsMgr->UpdateBenchmark();

        Platform* pPlatform = static_cast<Platform*>(m_pDevice->GetPlatform());
        pPlatform->ResetGpuWork();
    }

    m_pNextLayer->CmdColorSpaceConversionCopy(*NextImage(&srcImage),
        srcImageLayout,
        *NextImage(&dstImage),
        dstImageLayout,
        regionCount,
        pRegions,
        filter,
        cscTable);
}

// =====================================================================================================================
void CmdBuffer::DrawOverlay(
    const IImage* pSrcImage,
    PresentMode   presentMode)
{
    const auto& settings = m_pDevice->GetPlatform()->PlatformSettings();

    // Issue a barrier to ensure the text written via CS is complete and flushed out of L2.
    BarrierInfo barrier = {};
    barrier.waitPoint   = HwPipePreCs;

    const HwPipePoint postCs   = HwPipePostCs;
    barrier.pipePointWaitCount = 1;
    barrier.pPipePoints        = &postCs;

    BarrierTransition transition = {};
    transition.srcCacheMask      = CoherShader;
    transition.dstCacheMask      = CoherShader;

    barrier.transitionCount = 1;
    barrier.pTransitions    = &transition;

    if (settings.debugOverlayConfig.visualConfirmEnabled == true)
    {
        const PlatformProperties& properties   = static_cast<Platform*>(m_pDevice->GetPlatform())->Properties();
        const PresentMode expectedMode         =
            ((properties.explicitPresentModes == 0) ? PresentMode::Unknown : presentMode);

        // Draw the debug overlay using this command buffer. Note that the DX runtime controls whether the
        // present will be windowed or fullscreen. We have no reliable way to detect the chosen present mode.
        m_device.GetTextWriter().WriteVisualConfirm(static_cast<const Image&>(*pSrcImage),
                                                    this,
                                                    expectedMode);

        barrier.reason = Developer::BarrierReasonDebugOverlayText;

        CmdBarrier(barrier);
    }

    if (settings.debugOverlayConfig.timeGraphEnabled == true)
    {
        // Draw the time graph using this command buffer.
        m_device.GetTimeGraph().DrawVisualConfirm(static_cast<const Image&>(*pSrcImage), this);

        barrier.reason = Developer::BarrierReasonDebugOverlayGraph;

        CmdBarrier(barrier);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdPostProcessFrame(
    const CmdPostProcessFrameInfo& postProcessInfo,
    bool*                          pAddedGpuWork)
{
    // Only an Image supports visual confirm
    if ((postProcessInfo.flags.srcIsTypedBuffer == 0) &&
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 662
        (m_device.GetSettings()->disableDebugOverlayVisualConfirm == false) &&
#endif
        Device::DetermineDbgOverlaySupport(m_queueType))
    {
        DrawOverlay(postProcessInfo.pSrcImage, postProcessInfo.presentMode);

        if (pAddedGpuWork != nullptr)
        {
            *pAddedGpuWork = true;
        }
    }

    CmdBufferFwdDecorator::CmdPostProcessFrame(postProcessInfo, pAddedGpuWork);
}

} // DbgOverlay
} // Pal
