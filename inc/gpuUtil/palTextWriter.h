/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#pragma once

#include "palCmdBuffer.h"
#include "palDevice.h"
#include "palTextWriterFont.h"

// Forward declarations.
namespace Pal {
    struct GpuMemoryRequirements;
    class  IGpuMemory;
    class  IImage;
    class  IPipeline;
}

namespace GpuUtil
{

/// Defines the XY offset and colors for the debug text draw.
struct TextDrawShaderInfo
{
    Pal::uint32 startX;             ///< X offset on the image for the beginning of the text.
    Pal::uint32 startY;             ///< Y offset on the image for the beginning of the text.
    Pal::uint32 scale;              ///< Text scaling factor.
    Pal::uint32 foregroundColor[4]; ///< Color of the letters.
    Pal::uint32 backgroundColor[4]; ///< Color of the letter outlines.
};

/**
 ***********************************************************************************************************************
 * @brief TextWriter is GPU utility which uses PAL core and Utility classes to draw text onto a Pal::IImage object using
 *        a compute shader. The TextWriter class manages its own objects and GPU memory and can be used by clients.
 ***********************************************************************************************************************
 */
template <typename Allocator>
class TextWriter
{
public:
    TextWriter(
        Pal::IDevice* pDevice,
        Allocator*    pAllocator);
    ~TextWriter();

    /// Initializes the TextWriter class, creating the necessary PAL objects and allocating GPU memory.
    Pal::Result Init();

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 909
    /// Draws the text to the specified image at the XY coordinate using the specific command buffer.
    inline void DrawDebugText(
        const Pal::IImage& dstImage,
        Pal::ICmdBuffer*   pCmdBuffer,
        const char*        pText,
        Pal::uint32        x,
        Pal::uint32        y) const
        { DrawDebugText(dstImage, pCmdBuffer, pText, x, y, 1, {}); }

    /// Draws the text to the specified image at the XY coordinate using the specific command buffer.
    inline void DrawDebugText(
        const Pal::IImage& dstImage,
        Pal::ICmdBuffer*   pCmdBuffer,
        const char*        pText,
        Pal::uint32        x,
        Pal::uint32        y,
        Pal::uint32        pixelScale) const
        { DrawDebugText(dstImage, pCmdBuffer, pText, x, y, pixelScale, {}); }
#endif

    /// Draws the text to the specified image at the XY coordinate using the specific command buffer.
    void DrawDebugText(
        const Pal::IImage&     dstImage,
        Pal::ICmdBuffer*       pCmdBuffer,
        const char*            pText,
        Pal::uint32            x,
        Pal::uint32            y,
        Pal::DispatchInfoFlags infoFlags) const
        { DrawDebugText(dstImage, pCmdBuffer, pText, x, y, 1, infoFlags); }

    /// Draws the text to the specified image at the XY coordinate using the specific command buffer.
    void DrawDebugText(
        const Pal::IImage&     dstImage,
        Pal::ICmdBuffer*       pCmdBuffer,
        const char*            pText,
        Pal::uint32            x,
        Pal::uint32            y,
        Pal::uint32            pixelScale,
        Pal::DispatchInfoFlags infoFlags) const;

private:
    // Creates the GPU memory for the constant font data binary.
    Pal::Result CreateDrawFontData();

    // Helper function to create GPU memory for the TextWriter.
    Pal::Result CreateGpuMemory(
        Pal::GpuMemoryRequirements* pMemReqs,
        Pal::IGpuMemory**           ppGpuMemory,
        Pal::gpusize*               pOffset);

    // Helper function to create a basic image view for the destination image.
    void CreateImageView(const Pal::IImage* pImage, void* pOut) const;

    // Helper function to determine the raw format of the specified format.
    static Pal::SwizzledFormat GetRawFormat(Pal::ChNumFormat oldFmt);

    Pal::IDevice*const           m_pDevice;                         // Device associated with this TextWriter.
    Allocator*                   m_pAllocator;                      // The system-memory allocator to use.
    Pal::IPipeline*              m_pPipeline;                       // Pipeline object for drawing text.
    Pal::IGpuMemory*             m_pFontData;                       // GPU memory for constant font data binary.
    Pal::uint32                  m_fontSrd[4];                      // SRD for the font data.
    Pal::DeviceProperties        m_deviceProps;                     // Stored copy of the device properties.
    Pal::GpuMemoryHeapProperties m_memHeapProps[Pal::GpuHeapCount]; // Stored copy of the GpuHeap properties.
    Pal::uint32                  m_maxSrdSize;                      // Maximum size needed (in bytes) to store an SRD.

    PAL_DISALLOW_COPY_AND_ASSIGN(TextWriter);
    PAL_DISALLOW_DEFAULT_CTOR(TextWriter);
};

} // GpuUtil
