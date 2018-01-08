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

#pragma once

#include "palDevice.h"

// Forward declarations.
namespace Pal {
struct GpuMemoryRequirements;
class  ICmdBuffer;
class  IGpuMemory;
class  IImage;
class  IPipeline;
}

namespace GpuUtil
{

/// Namespace to encapsulate all constants related to the TimeGraph Draw.
namespace TimeGraphDraw
{
constexpr Pal::uint32 LineWidth = 5;
constexpr Pal::uint32 LineHeight = 1;
}

/// Defines the colors for the line draw.
struct ColorInfo
{
    Pal::uint32 lineColor[4]; ///< Color of the line.
};

/**
***********************************************************************************************************************
* @brief TimeGraph is GPU utility which uses PAL core and Utility classes to draw non-vertical lines (horizontal and
*        sloping lines) onto a Pal::IImage object using a compute shader. The TimeGraph class manages its own objects
*        and GPU memory and can be used by clients.
***********************************************************************************************************************
*/
template <typename Allocator>
class TimeGraph
{
public:
    TimeGraph(
        Pal::IDevice* pDevice,
        Allocator*    pAllocator);
    ~TimeGraph();

    /// Initializes the TimeGraph class, creating the necessary PAL objects and allocating GPU memory.
    Pal::Result Init();

    /// Draws the line to the specified image at the XY coordinate using the specific command buffer.
    void DrawGraphLine(
        const Pal::IImage& dstImage,
        Pal::ICmdBuffer*   pCmdBuffer,
        const Pal::uint32* pTimeData,
        Pal::uint32        xPosition,
        Pal::uint32        yPosition,
        const Pal::uint32* pLineColor,
        Pal::uint32        numDataPoints) const;

private:
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
    Pal::DeviceProperties        m_deviceProps;                     // Stored copy of the device properties.
    Pal::GpuMemoryHeapProperties m_memHeapProps[Pal::GpuHeapCount]; // Stored copy of the GpuHeap properties.
    Pal::uint32                  m_maxSrdSize;                      // Maximum size needed (in bytes) to store an SRD.

    PAL_DISALLOW_COPY_AND_ASSIGN(TimeGraph);
    PAL_DISALLOW_DEFAULT_CTOR(TimeGraph);
};

} // GpuUtil
