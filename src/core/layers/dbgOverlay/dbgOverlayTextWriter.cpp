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

#include "core/layers/decorators.h"
#include "core/layers/dbgOverlay/dbgOverlayDevice.h"
#include "core/layers/dbgOverlay/dbgOverlayFpsMgr.h"
#include "core/layers/dbgOverlay/dbgOverlayImage.h"
#include "core/layers/dbgOverlay/dbgOverlayPlatform.h"
#include "core/layers/dbgOverlay/dbgOverlayTextWriter.h"
#include "core/platform.h"
#include "palFormatInfo.h"
#include "palSysUtil.h"
#include "palTextWriterImpl.h"

#if PAL_AMDGPU_BUILD
#include "core/os/amdgpu/amdgpuHeaders.h"
#endif

#include <ctime>

using namespace Util;

namespace Pal
{
namespace DbgOverlay
{

constexpr float OneMb = 1048576.0f;

// =====================================================================================================================
TextWriter::TextWriter(
    Device* pDevice)
    :
    m_pDevice(pDevice),
    m_textWriter(pDevice, static_cast<Platform*>(pDevice->GetPlatform()))
{
}

// =====================================================================================================================
TextWriter::~TextWriter()
{
}

// =====================================================================================================================
Result TextWriter::Init()
{
    Result result = m_textWriter.Init();

    return result;
}

// =====================================================================================================================
static void PrintMemoryInfo(
    const Device& device,
    const char*   pHeaderFormatString,
    const char*   pLineTitle,
    AllocType     allocType,
    bool          combinedNonLocal,
    char*         pOutputText)
{
    // Report amount of GPU memory allocated via the specified source.
    const float localGpuMem    = device.GetVidMemTotal(allocType, GpuHeapLocal)         / OneMb;
    const float invisGpuMem    = device.GetVidMemTotal(allocType, GpuHeapInvisible)     / OneMb;
    const float sysUswcGpuMem  = device.GetVidMemTotal(allocType, GpuHeapGartUswc)      / OneMb;
    const float sysCacheGpuMem = device.GetVidMemTotal(allocType, GpuHeapGartCacheable) / OneMb;
    const float sysCombGpuMem  = sysUswcGpuMem + sysCacheGpuMem;

    Util::Snprintf(pOutputText, BufSize,
                   pHeaderFormatString,
                   pLineTitle, localGpuMem, invisGpuMem,
                   combinedNonLocal ? sysCombGpuMem : sysUswcGpuMem,
                   sysCacheGpuMem);
}

// =====================================================================================================================
// Writes the Visual Confirm ("Rendered by <Your API Here>") to the specified image.
void TextWriter::WriteVisualConfirm(
    const Image&                          dstImage,    // Image to write visual confirm into.
    ICmdBuffer*                           pCmdBuffer,  // Command buffer to write commands into.
    const CmdPostProcessDebugOverlayInfo& debugOverlayInfo
    ) const
{
    auto*const               pFpsMgr         = static_cast<Platform*>(m_pDevice->GetPlatform())->GetFpsMgr();
    const auto&              settings        = m_pDevice->GetPlatform()->PlatformSettings();
    const PalPublicSettings* pPublicSettings = m_pDevice->GetPublicSettings();
    const auto&              gpuProps        = m_pDevice->GpuProps();

    char overlayText[MaxTextLines][BufSize];
    memset(overlayText, 0, sizeof(overlayText));

    uint32 textLines = 0;

    if (strlen(pPublicSettings->miscellaneousDebugString) > 0)
    {
        Util::Snprintf(&overlayText[textLines++][0], BufSize, pPublicSettings->miscellaneousDebugString);
    }

    if (strlen(pPublicSettings->renderedByString) > 0)
    {
        Util::Snprintf(&overlayText[textLines++][0], BufSize, pPublicSettings->renderedByString);
    }
    else
    {
        const char* pClientApiStr = m_pDevice->GetPlatform()->GetClientApiStr();
        Util::Snprintf(&overlayText[textLines++][0], BufSize, "Rendered by %s", pClientApiStr);
    }

    Platform* pPlatform = static_cast<Platform*>(m_pDevice->GetPlatform());
    if (pPlatform->HasRayTracingBeenUsed())
    {
        Util::Snprintf(&overlayText[textLines++][0], BufSize, "Powered by Ray Tracing");
    }

    Util::Snprintf(&overlayText[textLines++][0], BufSize, "GPU: %s", gpuProps.gpuName);

    // Optionally include command processor subversion info
    if (settings.debugOverlayConfig.cpVersionEnabled)
    {
        Util::Snprintf(&overlayText[textLines++][0],
                       BufSize,
                       "CP Feature %d, PFP Firmware 0x%x",
                       gpuProps.gfxipProperties.cpUcodeVersion,
                       gpuProps.gfxipProperties.pfpUcodeVersion);
    }

    // Add the index of the GPU that is presenting. This is formatted like this:
    // Presenting GPU:    1    2    3    4
    constexpr uint32 NumSpacing     = 5;

    Util::Snprintf(&overlayText[textLines][0], BufSize, "Presenting GPU: ");

    for (uint32 index = 0; index < MaxLdaChainLength; index++)
    {
        size_t indexOffset = strlen("Presenting GPU: ") + (NumSpacing * index);
        if (gpuProps.gpuIndex == index)
        {
            Util::Snprintf(&overlayText[textLines][indexOffset],
                           (BufSize - indexOffset), "  %d  ", index);
        }
        else
        {
            Util::Snprintf(&overlayText[textLines][indexOffset],
                           (BufSize - indexOffset), "     ");
        }
    }
    textLines++;

    Util::Snprintf(&overlayText[textLines][0], BufSize, "GPU Work: ");
    for (uint32 index = 0; index < MaxDevices; index++)
    {
        size_t indexOffset = strlen("GPU Work: ") + (NumSpacing * index);
        if (pPlatform->GetGpuWork(index))
        {
            Util::Snprintf(&overlayText[textLines][indexOffset],
                (BufSize - indexOffset), "  %d  ", index);
        }
    }
    textLines++;

    if (settings.debugOverlayConfig.dateTimeEnabled)
    {
        time_t now = time(0);
        struct tm tstruct = *localtime(&now);
        strftime(&overlayText[textLines++][0], BufSize, "Date: %Y-%m-%d (YYYY-MM-DD)", &tstruct);
        strftime(&overlayText[textLines++][0], BufSize, "Time: %H:%M:%S   (HH:MM:SS)",   &tstruct);
    }

    // Blank line.
    textLines++;

    if (settings.debugOverlayConfig.printFrameNumber)
    {
        Util::Snprintf(&overlayText[textLines++][0], BufSize, "Frame #: %u", pFpsMgr->FrameCount());
    }

    const float framerate = pFpsMgr->GetFramesPerSecond();

    const char* const PresentModeStrings[] =
    {
        "Unknown",    // 0
        "Windowed",   // 1
        "Fullscreen", // 2
    };

    static_assert(ArrayLen(PresentModeStrings) == uint32(PresentMode::Count), "PresentModeStrings is out of date.");

    const uint32 presentModeIdx = uint32(debugOverlayInfo.presentMode);
    PAL_ASSERT(presentModeIdx < uint32(PresentMode::Count));

    const char* const WsiPlatformStrings[] =
    {
        "Win32",         // 0x00000001,
        "Xcb",           // 0x00000002,
        "Xlib",          // 0x00000004,
        "Wayland",       // 0x00000008,
        "Mir",           // 0x00000010,
        "DirectDisplay", // 0x00000020,
        "Android",       // 0x00000040,
        "Dxgi",          // 0x00000080,
    };

    const uint32 wsiPlatformIdx = Log2(debugOverlayInfo.wsiPlatform);
    PAL_ASSERT(wsiPlatformIdx < 0xFF);

    if (debugOverlayInfo.wsiPlatform != 0)
    {
        // We know our WSI platform and may know our present mode (if not, we'll print Unknown).
        Util::Snprintf(&overlayText[textLines++][0], BufSize, "CPU Frame Rate:    %7.2f FPS (%s | %s)",
            framerate, PresentModeStrings[presentModeIdx], WsiPlatformStrings[wsiPlatformIdx]);
    }
    else if (debugOverlayInfo.presentMode != PresentMode::Unknown)
    {
        // We don't have a WSI platform but we do know our present mode.
        Util::Snprintf(&overlayText[textLines++][0], BufSize, "CPU Frame Rate:    %7.2f FPS (%s)",
            framerate, PresentModeStrings[presentModeIdx]);
    }
    else
    {
        // If we don't know what mode will be used, don't write a mode at all.
        Util::Snprintf(&overlayText[textLines++][0], BufSize, "CPU Frame Rate:    %7.2f FPS", framerate);
    }

    // Add benchmark string.
    pFpsMgr->GetBenchmarkString(&overlayText[textLines][0], BufSize);
    textLines++;

    // Blank line.
    textLines++;

    const float cpuTime = (pFpsMgr->GetCpuTime() * 1000);
    const float gpuTime = (pFpsMgr->GetGpuTime() * 1000);

    Util::Snprintf(&overlayText[textLines++][0], BufSize, "CPU Frame Time:    %7.2f ms", cpuTime);

    if (pFpsMgr->PartialGpuTime())
    {
        Util::Snprintf(&overlayText[textLines++][0], BufSize, "GPU Frame Time:    %7.2f ms (Partial)", gpuTime);
    }
    else
    {
        Util::Snprintf(&overlayText[textLines++][0], BufSize, "GPU Frame Time:    %7.2f ms", gpuTime);
    }

    // Blank line.
    textLines++;

    const bool combinedNonLocal = (settings.overlayMemoryInfoConfig.combineNonLocal);

    const char* pHeaderFormatString = combinedNonLocal ? "%11s %10s | %10s | %10s" : "%11s %10s | %10s | %10s | %10s";

    // Header text for GPU memory allocation report.
    Util::Snprintf(&overlayText[textLines++][0], BufSize,
                   pHeaderFormatString,
                   "GpuMem (MB)", "LocalVis", "LocalInvis",
                   combinedNonLocal ? "System" : "SysUswc",
                   "SysCache");

    const char* pMemFormatString =
        combinedNonLocal ? "%10s: %10.2f | %10.2f | %10.2f" : "%10s: %10.2f | %10.2f | %10.2f | %10.2f";

    if (settings.overlayMemoryInfoConfig.reportExternal)
    {
        PrintMemoryInfo(*m_pDevice, pMemFormatString, "External",
                        AllocTypeExternal, combinedNonLocal, &overlayText[textLines++][0]);
    }

    if (settings.overlayMemoryInfoConfig.reportInternal)
    {
        PrintMemoryInfo(*m_pDevice, pMemFormatString, "Internal",
                        AllocTypeInternal, combinedNonLocal, &overlayText[textLines++][0]);
    }

    if (settings.overlayMemoryInfoConfig.reportCmdAllocator)
    {
        PrintMemoryInfo(*m_pDevice, pMemFormatString, "CmdAlloc",
                        AllocTypeCmdAlloc, combinedNonLocal, &overlayText[textLines++][0]);
    }

    m_pDevice->SumVidMemAllocations();

    // Report the total used GPU memory.
    const float totalLocalGpuMem    = m_pDevice->GetVidMemTotalSum(GpuHeapLocal)         / OneMb;
    const float totalInvisGpuMem    = m_pDevice->GetVidMemTotalSum(GpuHeapInvisible)     / OneMb;
    const float totalSysUswcGpuMem  = m_pDevice->GetVidMemTotalSum(GpuHeapGartUswc)      / OneMb;
    const float totalSysCacheGpuMem = m_pDevice->GetVidMemTotalSum(GpuHeapGartCacheable) / OneMb;
    const float totalSysCombGpuMem  = totalSysUswcGpuMem + totalSysCacheGpuMem;

    Util::Snprintf(&overlayText[textLines++][0], BufSize,
                   pMemFormatString,
                   "Total Used", totalLocalGpuMem, totalInvisGpuMem,
                   combinedNonLocal ? totalSysCombGpuMem : totalSysUswcGpuMem,
                   totalSysCacheGpuMem);

    if (settings.overlayMemoryInfoConfig.displayPeakMemUsage)
    {
        const float peakLocalGpuMem    = m_pDevice->GetPeakMemTotal(GpuHeapLocal)         / OneMb;
        const float peakInvisGpuMem    = m_pDevice->GetPeakMemTotal(GpuHeapInvisible)     / OneMb;
        const float peakSysUswcGpuMem  = m_pDevice->GetPeakMemTotal(GpuHeapGartUswc)      / OneMb;
        const float peakSysCacheGpuMem = m_pDevice->GetPeakMemTotal(GpuHeapGartCacheable) / OneMb;
        const float peakSysCombGpuMem  = peakSysUswcGpuMem + peakSysCacheGpuMem;

        Util::Snprintf(&overlayText[textLines++][0], BufSize,
                       pMemFormatString,
                       "Peak Used", peakLocalGpuMem, peakInvisGpuMem,
                       combinedNonLocal ? peakSysCombGpuMem : peakSysUswcGpuMem, peakSysCacheGpuMem);
    }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 766
    const float localHeapSize    = m_pDevice->GetMemHeapProps(GpuHeapLocal).logicalSize         / OneMb;
    const float invisHeapSize    = m_pDevice->GetMemHeapProps(GpuHeapInvisible).logicalSize     / OneMb;
    const float sysUswcHeapSize  = m_pDevice->GetMemHeapProps(GpuHeapGartUswc).logicalSize      / OneMb;
    const float sysCacheHeapSize = m_pDevice->GetMemHeapProps(GpuHeapGartCacheable).logicalSize / OneMb;
#else
    const float localHeapSize    = m_pDevice->GetMemHeapProps(GpuHeapLocal).heapSize            / OneMb;
    const float invisHeapSize    = m_pDevice->GetMemHeapProps(GpuHeapInvisible).heapSize        / OneMb;
    const float sysUswcHeapSize  = m_pDevice->GetMemHeapProps(GpuHeapGartUswc).heapSize         / OneMb;
    const float sysCacheHeapSize = m_pDevice->GetMemHeapProps(GpuHeapGartCacheable).heapSize    / OneMb;
#endif
    const float sysCombHeapSize  = sysUswcHeapSize + sysCacheHeapSize;

    Util::Snprintf(&overlayText[textLines++][0], BufSize,
                   pMemFormatString,
                   "Heap Size", localHeapSize, invisHeapSize,
                   combinedNonLocal ? sysCombHeapSize : sysUswcHeapSize,
                   sysCacheHeapSize);

    PAL_ASSERT(textLines <= MaxTextLines);

    // Get the starting pixel position.
    uint32 x = 0;
    uint32 y = 0;

    const uint32 textLength = combinedNonLocal ? MaxTextLengthComb : MaxTextLength;

    const auto*const pCreateInfo = dstImage.GetCreateInfo();
    switch (pFpsMgr->GetDebugOverlayLocation())
    {
    case DebugOverlayUpperLeft:
        x = 0;
        y = 0;
        break;
    case DebugOverlayUpperRight:
        x = pCreateInfo->extent.width - (GpuUtil::TextWriterFont::LetterWidth * textLength);
        y = 0;
        break;
    case DebugOverlayLowerRight:
        x = pCreateInfo->extent.width - (GpuUtil::TextWriterFont::LetterWidth * textLength);
        y = pCreateInfo->extent.height - (GpuUtil::TextWriterFont::LetterHeight * textLines);
        break;
    default:
    case DebugOverlayLowerLeft:
        x = 0;
        y = pCreateInfo->extent.height - (GpuUtil::TextWriterFont::LetterHeight * textLines);
        break;
    }

    // Draw each line of text.
    for (uint32 i = 0; i < textLines; i++)
    {
        m_textWriter.DrawDebugText(dstImage, pCmdBuffer, &overlayText[i][0], x, y);
        y += GpuUtil::TextWriterFont::LetterHeight;
    }
}

} // DbgOverlay
} // Pal
