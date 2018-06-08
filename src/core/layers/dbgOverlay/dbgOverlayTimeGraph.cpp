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

#include "core/layers/decorators.h"
#include "core/layers/dbgOverlay/dbgOverlayDevice.h"
#include "core/layers/dbgOverlay/dbgOverlayFpsMgr.h"
#include "core/layers/dbgOverlay/dbgOverlayImage.h"
#include "core/layers/dbgOverlay/dbgOverlayPlatform.h"
#include "core/layers/dbgOverlay/dbgOverlayTextWriter.h"
#include "core/layers/dbgOverlay/dbgOverlayTimeGraph.h"
#include "core/platform.h"
#include "palAutoBuffer.h"
#include "palFormatInfo.h"
#include "palSysUtil.h"
#include "palTimeGraphImpl.h"

#include "core/os/lnx/lnxHeaders.h"

using namespace Util;

namespace Pal
{
namespace DbgOverlay
{

// =====================================================================================================================
TimeGraph::TimeGraph(
    Device* pDevice)
    :
    m_pDevice(pDevice),
    m_timegraph(pDevice, static_cast<Platform*>(pDevice->GetPlatform()))
{
}

// =====================================================================================================================
TimeGraph::~TimeGraph()
{
}

// =====================================================================================================================
Result TimeGraph::Init()
{
    Result result = m_timegraph.Init();

    return result;
}

// =====================================================================================================================
// Draws the Time Graph to the specified image.
void TimeGraph::DrawVisualConfirm(
    const Image& dstImage,    // Image to write visual confirm into.
    ICmdBuffer*  pCmdBuffer   // Command buffer to write commands into.
    ) const
{
    auto*const       pFpsMgr           = static_cast<Platform*>(m_pDevice->GetPlatform())->GetFpsMgr();
    const auto&      settings          = m_pDevice->OverlaySettings();
    const auto&      gpuProps          = m_pDevice->GpuProps();
    const auto*const pCreateInfo       = dstImage.GetCreateInfo();
    const bool       combinedNonLocal  = (settings.overlayCombineNonLocal);

    const uint32 textLength = combinedNonLocal ? MaxTextLengthComb : MaxTextLength;

    const uint32 textWidth  = (GpuUtil::TextWriterFont::LetterWidth * textLength);
    const uint32 graphWidth = (GpuUtil::TimeGraphDraw::LineWidth * TimeCount);

    const uint32 timeCount = TimeCount;

    if (!((settings.visualConfirmEnabled == true) && (pCreateInfo->extent.width) < (graphWidth + textWidth)))
    {
        // Pack the raw draw colors into the destination format.
        const Pal::SwizzledFormat imgFormat = dstImage.GetImageCreateInfo().swizzledFormat;

        Pal::uint32 gridLineColor[4] = {};
        Pal::uint32 cpuLineColor[4]  = {};
        Pal::uint32 gpuLineColor[4]  = {};

        // Convert the raw color into the destination format.
        if (Pal::Formats::IsUnorm(imgFormat.format)   || Pal::Formats::IsSnorm(imgFormat.format)   ||
            Pal::Formats::IsUscaled(imgFormat.format) || Pal::Formats::IsSscaled(imgFormat.format) ||
            Pal::Formats::IsFloat(imgFormat.format)   || Pal::Formats::IsSrgb(imgFormat.format))
        {
            constexpr float ColorTable[][4] =
            {
                { 0.0f, 0.0f, 0.0f, 1.0f }, // Black
                { 1.0f, 0.0f, 0.0f, 1.0f }, // Red
                { 0.0f, 1.0f, 0.0f, 1.0f }, // Green
                { 0.0f, 0.0f, 1.0f, 1.0f }, // Blue
                { 1.0f, 1.0f, 0.0f, 1.0f }, // Yellow
                { 0.0f, 1.0f, 1.0f, 1.0f }, // Cyan
                { 1.0f, 0.0f, 1.0f, 1.0f }, // Magenta
                { 1.0f, 1.0f, 1.0f, 1.0f }, // White
            };

            Pal::Formats::ConvertColor(imgFormat, &ColorTable[settings.timeGraphGridLineColor][0], &gridLineColor[0]);
            Pal::Formats::ConvertColor(imgFormat, &ColorTable[settings.timeGraphCpuLineColor][0], &cpuLineColor[0]);
            Pal::Formats::ConvertColor(imgFormat, &ColorTable[settings.timeGraphGpuLineColor][0], &gpuLineColor[0]);
        }
        else if (Pal::Formats::IsSint(imgFormat.format))
        {
            constexpr Pal::uint32 ColorTable[][4] =
            {
                { 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF },     // Black
                { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x7FFFFFFF },     // Red
                { 0x00000000, 0xFFFFFFFF, 0x00000000, 0x7FFFFFFF },     // Green
                { 0x00000000, 0x00000000, 0xFFFFFFFF, 0x7FFFFFFF },     // Blue
                { 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x7FFFFFFF },     // Yellow
                { 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0x7FFFFFFF },     // Cyan
                { 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x7FFFFFFF },     // Magenta
                { 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF },     // White
            };
            memcpy(&gridLineColor[0], &ColorTable[settings.timeGraphGridLineColor][0], sizeof(gridLineColor));
            memcpy(&cpuLineColor[0], &ColorTable[settings.timeGraphCpuLineColor][0], sizeof(cpuLineColor));
            memcpy(&gpuLineColor[0], &ColorTable[settings.timeGraphGpuLineColor][0], sizeof(gpuLineColor));
        }
        else
        {
            PAL_ASSERT(Pal::Formats::IsUint(imgFormat.format));

            Pal::uint32 ColorTable[][4] =
            {
                { 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF },     // Black
                { 0xFFFFFFFF, 0x00000000, 0x00000000, 0xFFFFFFFF },     // Red
                { 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF },     // Green
                { 0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF },     // Blue
                { 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF },     // Yellow
                { 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF },     // Cyan
                { 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF },     // Magenta
                { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF },     // White
            };

            memcpy(&gridLineColor[0], &ColorTable[settings.timeGraphGridLineColor][0], sizeof(gridLineColor));
            memcpy(&cpuLineColor[0], &ColorTable[settings.timeGraphCpuLineColor][0], sizeof(cpuLineColor));
            memcpy(&gpuLineColor[0], &ColorTable[settings.timeGraphGpuLineColor][0], sizeof(gpuLineColor));
        }

        // Get the starting pixel position.
        uint32 x = 0;
        uint32 y = 0;
        uint32 yOffset = 10;
        uint32 xOffset = 10;

        switch (pFpsMgr->GetTimeGraphLocation())
        {
        case DebugOverlayLowerLeft:
            x = xOffset;
            y = pCreateInfo->extent.height - yOffset;
            break;
        default:
        case DebugOverlayLowerRight:
            x = pCreateInfo->extent.width - (GpuUtil::TimeGraphDraw::LineWidth * timeCount);
            y = pCreateInfo->extent.height - yOffset;
            break;
        }

        // Storing data values
        Util::AutoBuffer<uint32, 100, PlatformDecorator> dataValues(timeCount,
                                                                    static_cast<Platform*>(m_pDevice->GetPlatform()));

        for (uint32 i = 0; i < timeCount; i++)
        {
            dataValues[i] = 0;
        }

        // Draw reference line for x-axis at Y = 0
        m_timegraph.DrawGraphLine(dstImage, pCmdBuffer, &dataValues[0], x, y, gridLineColor, timeCount);

        for (uint32 i = 0; i < timeCount; i++)
        {
            dataValues[i] = 100;
        }

        // Draw refernce marker parallel to the x-axis at Y = 100
        m_timegraph.DrawGraphLine(dstImage, pCmdBuffer, &dataValues[0], x, y, gridLineColor, timeCount);

        for (uint32 i = 0; i < timeCount; i++)
        {
            dataValues[i] = 200;
        }

        // Draw refernce marker parallel to the x-axis at Y = 200
        m_timegraph.DrawGraphLine(dstImage, pCmdBuffer, &dataValues[0], x, y, gridLineColor, timeCount);

        // Issue a barrier to ensure the line drawn via CS is complete.
        BarrierInfo gridBarrier = {};
        gridBarrier.waitPoint   = HwPipePreCs;

        const HwPipePoint gridPostCs   = HwPipePostCs;
        gridBarrier.pipePointWaitCount = 1;
        gridBarrier.pPipePoints        = &gridPostCs;

        BarrierTransition gridTransition = {};
        gridTransition.srcCacheMask      = CoherShader;
        gridTransition.dstCacheMask      = CoherShader;

        gridBarrier.transitionCount = 1;
        gridBarrier.pTransitions    = &gridTransition;

        gridBarrier.reason          = Developer::BarrierReasonTimeGraphGrid;

        pCmdBuffer->CmdBarrier(gridBarrier);

        // Storing GpuTime Values from newest to oldest
        for (uint32 i = timeCount; i > 0; i--)
        {
            dataValues[i - 1] = pFpsMgr->GetScaledGpuTime(timeCount - i);
        }

        // Draw gpu line graph
        m_timegraph.DrawGraphLine(dstImage, pCmdBuffer, &dataValues[0], x, y, gpuLineColor, timeCount);

        // Issue a barrier to ensure the line drawn via CS is complete.
        BarrierInfo gpuBarrier = {};
        gpuBarrier.waitPoint   = HwPipePreCs;

        const HwPipePoint gpuPostCs   = HwPipePostCs;
        gpuBarrier.pipePointWaitCount = 1;
        gpuBarrier.pPipePoints        = &gpuPostCs;

        BarrierTransition gpuTransition = {};
        gpuTransition.srcCacheMask      = CoherShader;
        gpuTransition.dstCacheMask      = CoherShader;

        gpuBarrier.transitionCount = 1;
        gpuBarrier.pTransitions    = &gpuTransition;

        gpuBarrier.reason          = Developer::BarrierReasonTimeGraphGpuLine;

        pCmdBuffer->CmdBarrier(gpuBarrier);

        // Storing CpuTime Values from newest to oldest
        for (uint32 i = timeCount; i > 0; i--)
        {
            dataValues[i - 1] = pFpsMgr->GetScaledCpuTime(timeCount - i);
        }

        // Draw cpu line graph
        m_timegraph.DrawGraphLine(dstImage, pCmdBuffer, &dataValues[0], x, y, cpuLineColor, timeCount);
    }
}

} // DbgOverlay
} // Pal
