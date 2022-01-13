/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "msaaImageCopy/g_msaaImageCopyComputePipelineInit.h"

/// Forward declarations.
namespace Pal
{
struct SubresId;
class  ICmdBuffer;
class  IImage;
class  IPipeline;
}

namespace GpuUtil
{

///Namespace to encapsulate all constants related to MSAAIMAGECOPY.
namespace MsaaImageCopy
{
constexpr Pal::uint32 ThreadsPerGroupX = 8;
constexpr Pal::uint32 ThreadsPerGroupY = 8;
}

/**
 ***********************************************************************************************************************
 * @class MsaaImageCopyUtil
 * @brief MsaaImageCopyUtil is GPU utility which uses PAL core and Utility classes to perform different sample count
 *        resource's copy using compute shader.  The MsaaImageCopyUtil class can be used by clients.
 ***********************************************************************************************************************
 */
template <typename Allocator>
class MsaaImageCopyUtil
{
public:
    MsaaImageCopyUtil(
        Pal::IDevice*  pDevice,
        Allocator*     pAllocator);
    ~MsaaImageCopyUtil();

    /// Initializes the MsaaImageCopyUtil class, creating the necessary PAL objects and allocating GPU memory.
    ///
    /// @returns Success if successful, ErrorOutOfMemory if ran out of system memory, ErrorOutOfGpuMemory if ran out of
    ///          GPU memory for pipeline uploads.
    Pal::Result Init();

    /// Different sample count resource's blit from source image to
    /// destination image using the specified command buffer.
    ///
    /// The added commands are equivalent to a CmdDispatch for synchronization purposes.
    /// Only different sample count MSAA image are allowed.
    ///
    /// @param pCmdBuffer  The command buffer to add commands to. Command buffer must support compute shader dispatch.
    /// @param srcImage    Src image for the msaa image copy.
    /// @param dstImage    Dst image for the msaa image copy.
    /// @param regionCount Msaa image copy region count.
    /// @param pRegions    Msaa image copy regions array.
    void MsaaImageCopy(
        Pal::ICmdBuffer*            pCmdBuffer,
        const Pal::IImage&          srcImage,
        const Pal::IImage&          dstImage,
        uint32                      regionCount,
        const Pal::ImageCopyRegion* pRegions) const;

private:
    // Allocates and binds embedded user data.
    Pal::uint32* CreateAndBindEmbeddedUserData(
        Pal::ICmdBuffer*  pCmdBuffer,
        Pal::uint32       sizeInDwords,
        Pal::uint32       entryToBind) const;

    // Populates an ImageViewInfo that wraps the given subres of the provided image object.
    void BuildImageViewInfo(
        Pal::ImageViewInfo*   pInfo,
        const Pal::IImage*    pImage,
        const Pal::SubresId&  subresId,
        Pal::SwizzledFormat   swizzledFormat,
        bool                  isShaderWriteable) const;

    // Device associated with this MsaaImageCopyUtil.
    Pal::IDevice*const  m_pDevice;
    // The system-memory allocator to use.
    Allocator*          m_pAllocator;
    // Pipeline objects.
    Pal::IPipeline*     m_pPipelines[static_cast<Pal::uint32>(MsaaImageCopy::MsaaImageCopyComputePipeline::Count)];
    // Maximum size needed (in DWORDs) to store an SRD.
    Pal::uint32         m_maxSrdSizeInDwords;

    PAL_DISALLOW_COPY_AND_ASSIGN(MsaaImageCopyUtil);
    PAL_DISALLOW_DEFAULT_CTOR(MsaaImageCopyUtil);
};

} // GpuUtil
