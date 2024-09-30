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

#include "core/layers/dbgOverlay/dbgOverlayCmdBuffer.h"
#include "core/layers/dbgOverlay/dbgOverlayDevice.h"
#include "core/layers/dbgOverlay/dbgOverlayFpsMgr.h"
#include "core/layers/dbgOverlay/dbgOverlayImage.h"
#include "core/layers/dbgOverlay/dbgOverlayPlatform.h"
#include "core/layers/dbgOverlay/dbgOverlayTextWriter.h"
#include "core/layers/dbgOverlay/dbgOverlayTimeGraph.h"
#include "g_platformSettings.h"

namespace Pal
{
namespace DbgOverlay
{

// =====================================================================================================================
CmdBuffer::CmdBuffer(
    ICmdBuffer*   pNextCmdBuffer,
    Device*const  pDevice,
    QueueType     queueType)
    :
    CmdBufferFwdDecorator(pNextCmdBuffer, pDevice),
    m_pDevice(pDevice),
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
            CmdPostProcessDebugOverlayInfo debugOverlayInfo = {};
            debugOverlayInfo.presentMode = PresentMode::Unknown;
            m_pDevice->GetTextWriter().WriteVisualConfirm(static_cast<const Image&>(srcImage),
                                                          this,
                                                          debugOverlayInfo);

            // Issue a barrier to ensure the text written via CS is complete and dst cache is flushed out.
            AcquireReleaseInfo acqRelInfo = {};
            acqRelInfo.srcGlobalStageMask  = PipelineStageCs;
            acqRelInfo.dstGlobalStageMask  = PipelineStageCs;
            acqRelInfo.srcGlobalAccessMask = CoherShader;
            acqRelInfo.dstGlobalAccessMask = CoherShader;
            acqRelInfo.reason              = Developer::BarrierReasonDebugOverlayText;
            CmdReleaseThenAcquire(acqRelInfo);
        }

        auto*const pFpsMgr = static_cast<Platform*>(m_pDevice->GetPlatform())->GetFpsMgr();

        pFpsMgr->IncrementFrameCount();

        // Call UpdateFps and UpdateBenchmark now instead of at presentation time.
        pFpsMgr->UpdateFps();
        pFpsMgr->UpdateGpuFps();
        pFpsMgr->UpdateBenchmark();
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
    const IImage*                         pSrcImage,
    const CmdPostProcessDebugOverlayInfo& debugOverlayInfo,
    ImageLayout                           srcImageLayout)
{
    const auto&  settings = m_pDevice->GetPlatform()->PlatformSettings();
    const uint32 engines  = (m_queueType == QueueTypeUniversal) ? LayoutUniversalEngine : LayoutComputeEngine;

    // Issue a barrier to ensure the text written via CS is complete and dst cache is flushed out.
    ImgBarrier imgBarrier    = {};
    imgBarrier.pImage        = pSrcImage;
    imgBarrier.srcStageMask  = PipelineStageCs;
    imgBarrier.dstStageMask  = PipelineStageCs;
    imgBarrier.srcAccessMask = CoherShader;
    imgBarrier.oldLayout     = srcImageLayout;
    imgBarrier.newLayout     = { .engines = engines };

    pSrcImage->GetFullSubresourceRange(&imgBarrier.subresRange);

    AcquireReleaseInfo acqRelInfo = {};
    acqRelInfo.imageBarrierCount  = 1;
    acqRelInfo.pImageBarriers     = &imgBarrier;

    if (settings.debugOverlayConfig.visualConfirmEnabled == true)
    {
        const PlatformProperties& properties   = static_cast<Platform*>(m_pDevice->GetPlatform())->Properties();

        CmdPostProcessDebugOverlayInfo expectedDebugOverlayInfo = debugOverlayInfo;
        if (properties.explicitPresentModes == 0)
        {
            expectedDebugOverlayInfo.presentMode = PresentMode::Unknown;
        }

        // Draw the debug overlay using this command buffer. Note that the DX runtime controls whether the
        // present will be windowed or fullscreen. We have no reliable way to detect the chosen present mode.
        m_pDevice->GetTextWriter().WriteVisualConfirm(static_cast<const Image&>(*pSrcImage),
                                                      this,
                                                      expectedDebugOverlayInfo);

        if (settings.debugOverlayConfig.timeGraphEnabled == true)
        {
            imgBarrier.dstAccessMask    = CoherShader;
            imgBarrier.newLayout.usages = LayoutShaderRead | LayoutShaderWrite;
        }
        else
        {
            imgBarrier.dstAccessMask    = CoherPresent;
            imgBarrier.newLayout.usages = LayoutPresentWindowed | LayoutPresentFullscreen;
        }

        acqRelInfo.reason = Developer::BarrierReasonDebugOverlayText;
        CmdReleaseThenAcquire(acqRelInfo);
    }

    if (settings.debugOverlayConfig.timeGraphEnabled == true)
    {
        // Draw the time graph using this command buffer.
        m_pDevice->GetTimeGraph().DrawVisualConfirm(static_cast<const Image&>(*pSrcImage),
                                                    this,
                                                    debugOverlayInfo.presentKey);

        imgBarrier.dstAccessMask    = CoherPresent;
        imgBarrier.newLayout.usages = LayoutPresentWindowed | LayoutPresentFullscreen;
        acqRelInfo.reason           = Developer::BarrierReasonDebugOverlayGraph;
        CmdReleaseThenAcquire(acqRelInfo);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdPostProcessFrame(
    const CmdPostProcessFrameInfo& postProcessInfo,
    bool*                          pAddedGpuWork)
{
    // Only an Image supports visual confirm
    if ((postProcessInfo.flags.srcIsTypedBuffer == 0) &&
        (m_pDevice->GetSettings()->disableDebugOverlayVisualConfirm == false) &&
        Device::DetermineDbgOverlaySupport(m_queueType))
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 836
        DrawOverlay(postProcessInfo.pSrcImage, postProcessInfo.debugOverlay, postProcessInfo.srcImageLayout);
#else
        // layout is unknown, make an assumption here.
        const uint32 engines  = (m_queueType == QueueTypeUniversal) ? LayoutUniversalEngine : LayoutComputeEngine;
        const ImageLayout srcImageLayout = { .usages = LayoutShaderRead | LayoutShaderWrite, .engines = engines };

        DrawOverlay(postProcessInfo.pSrcImage, postProcessInfo.debugOverlay, srcImageLayout);
#endif

        if (pAddedGpuWork != nullptr)
        {
            *pAddedGpuWork = true;
        }
    }

    CmdBufferFwdDecorator::CmdPostProcessFrame(postProcessInfo, pAddedGpuWork);
}

} // DbgOverlay
} // Pal
