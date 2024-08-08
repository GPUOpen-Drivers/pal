/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "mlaa/g_mlaaComputePipelineInit.h"

// Forward declarations.
namespace Pal
{
    struct SubresId;
    class  ICmdBuffer;
    class  IImage;
    class  IPipeline;
}

namespace GpuUtil
{

/// Namespace to encapsulate all constants related to the mlaa resolve.
namespace Mlaa
{
constexpr Pal::uint32 ThreadsPerGroupX = 8;
constexpr Pal::uint32 ThreadsPerGroupY = 8;
}

/**
 ***********************************************************************************************************************
 * @brief MlaaUtil is GPU utility which uses PAL core and Utility classes to perform an MLAA resolve from source image
 *        to destination image using compute shader. The MlaaUtil class manages its own objects and GPU memory and can
 *        be used by clients.
 ***********************************************************************************************************************
 */
template <typename Allocator>
class MlaaUtil
{
public:
    MlaaUtil(
        Pal::IDevice* pDevice,
        Allocator*    pAllocator,
        bool          fastPath);
    ~MlaaUtil();

    /// Initializes the MlaaUtil class, creating the necessary PAL objects and allocating GPU memory.
    Pal::Result Init();

    /// MLAA resolve from source image to destination image using the specific command buffer.
    void ResolveImage(
        Pal::ICmdBuffer*      pCmdBuffer,
        const Pal::IImage&    srcImage,
        Pal::SubresId         srcSubres,
        const Pal::IImage&    dstImage,
        Pal::SubresId         dstSubres);

private:

    // Enumerates auxiliary images used by mlaa resolve
    enum class MlaaAuxImage : Pal::uint32
    {
        SepEdge = 0,    // Separate edge image
        HorzEdgeCountA, // Horizontal edge count ping pong image A
        HorzEdgeCountB, // Horizontal edge Count ping pong image B
        VertEdgeCountA, // Vertical edge count ping pong image A
        VertEdgeCountB, // Vertical edge count ping pong image B
        EdgeCountFast,  // Edge count image for fast path use
        Count,          // Auxiliary image count
    };

    // Setup auxiliary image objects.
    Pal::Result SetupAuxImages(
        Pal::uint32  srcWidth,
        Pal::uint32  srcHeight);

    // Cleanup auxiliary image objects.
    void CleanupAuxImages();

    // Allocates and binds embedded user data.
    Pal::uint32* CreateAndBindEmbeddedUserData(
        Pal::ICmdBuffer*  pCmdBuffer,
        Pal::uint32       sizeInDwords);

    // Populates an ImageViewInfo that wraps the given subres of the provided image object.
    void BuildImageViewInfo(
        Pal::ImageViewInfo*   pInfo,
        const Pal::IImage*    pImage,
        Pal::SubresId         subresId,
        bool                  isShaderWriteable);

    // Creates the GPU memory object and binds it to the provided image
    Pal::Result CreateImageMemoryObject(
        Pal::IImage*      pImage,
        Pal::IGpuMemory** ppGpuMemory);

    // 1st stage of MLAA resolve: Find the separating edges
    void FindSepEdge(
        Pal::ICmdBuffer*     pCmdBuffer,
        const Pal::IImage&   srcImage,
        Pal::SubresId        srcSubres);

    // 2nd stage of MLAA resolve: Calculate the separating edge length (recursive doubling path)
    void CalcSepEdgeLength(
        Pal::ICmdBuffer* pCmdBuffer,
        Pal::uint32      iterationDepth);

    // 2nd stage of MLAA resolve: Calculate the separating edge length (fast path)
    void CalcSepEdgeLengthFast(
        Pal::ICmdBuffer* pCmdBuffer);

    // Final stage of MLAA resolve: Blend the pixels along the separating edge
    void FinalBlend(
        Pal::ICmdBuffer*     pCmdBuffer,
        const Pal::IImage&   srcImage,
        Pal::SubresId        srcSubres,
        const Pal::IImage&   dstImage,
        Pal::SubresId        dstSubres,
        Pal::uint32          maxIterationDepth);

    // Final stage of MLAA resolve: Blend the pixels along the separating edge (fast path)
    void FinalBlendFast(
        Pal::ICmdBuffer*     pCmdBuffer,
        const Pal::IImage&   srcImage,
        Pal::SubresId        srcSubres,
        const Pal::IImage&   dstImage,
        Pal::SubresId        dstSubres);

    // Device associated with this MlaaUtil.
    Pal::IDevice*const           m_pDevice;
    // The system-memory allocator to use.
    Allocator*                   m_pAllocator;
    // Fast path or not.
    bool                         m_fastPath;
    // Pipeline objects.
    Pal::IPipeline*              m_pPipelines[static_cast<Pal::uint32>(Mlaa::MlaaComputePipeline::Count)];
    // Auxiliary image objects.
    Pal::IImage*                 m_pAuxImages[static_cast<Pal::uint32>(MlaaAuxImage::Count)];
    // GpuMemory objects for auxiliary images.
    Pal::IGpuMemory*             m_pAuxGpuMem[static_cast<Pal::uint32>(MlaaAuxImage::Count)];
    // Current image width.
    Pal::uint32                  m_width;
    // Current image height.
    Pal::uint32                  m_height;
    // Stored copy of the device properties.
    Pal::DeviceProperties        m_deviceProps;
    // Maximum size needed (in DWORDs) to store an SRD.
    Pal::uint32                  m_maxSrdSizeInDwords;

    PAL_DISALLOW_COPY_AND_ASSIGN(MlaaUtil);
    PAL_DISALLOW_DEFAULT_CTOR(MlaaUtil);
};

} // GpuUtil
