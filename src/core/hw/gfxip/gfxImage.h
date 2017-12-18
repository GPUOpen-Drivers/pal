/*
 *******************************************************************************
 *
 * Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

#include "core/gpuMemory.h"
#include "addrinterface.h"
#include "addrtypes.h"
#include "palCmdBuffer.h"

namespace Pal
{

// Forward declarations
class      CmdBuffer;
class      Device;
class      Image;
class      GfxCmdBuffer;
class      SubResIterator;
struct     GpuMemoryRequirements;
struct     ImageInfo;
struct     SubresId;
struct     SubResourceInfo;
enum class ImageAspect : uint32;
enum class ClearMethod : uint32;

// Mask of all image usage layout flags which are valid to use on depth/stencil Images.
constexpr uint32 AllDepthImageLayoutFlags = LayoutUninitializedTarget |
                                            LayoutDepthStencilTarget  |
                                            LayoutShaderRead          |
                                            LayoutShaderWrite         |
                                            LayoutCopySrc             |
                                            LayoutCopyDst             |
                                            LayoutResolveSrc          |
                                            LayoutResolveDst;

enum UseComputeExpand : uint32
{
    UseComputeExpandDepth = 0x00000001,
    UseComputeExpandMsaaDepth = 0x00000002,
    UseComputeExpandDcc = 0x00000004,
    UseComputeExpandMsaaDcc = 0x00000008,
    UseComputeExpandAlways = 0x00000010,

};

// =====================================================================================================================
class GfxImage
{
public:
    static constexpr uint32 UseComputeExpand = UseComputeExpandDepth | UseComputeExpandDcc;

    virtual ~GfxImage() {}

    Image* Parent() const { return m_pParent; }

    virtual ImageType GetOverrideImageType() const;

    virtual bool HasFmaskData() const = 0;

    virtual bool HasHtileData() const = 0;

    virtual bool IsFastColorClearSupported(GfxCmdBuffer*      pCmdBuffer,
                                           ImageLayout        colorLayout,
                                           const uint32*      pColor,
                                           const SubresRange& range) const = 0;

    virtual bool IsFastDepthStencilClearSupported(ImageLayout        depthLayout,
                                                  ImageLayout        stencilLayout,
                                                  float              depth,
                                                  uint8              stencil,
                                                  const SubresRange& range) const = 0;

    virtual bool IsFormatReplaceable(const SubresId& subresId, ImageLayout layout) const = 0;
    virtual bool IsSubResourceLinear(const SubresId& subresource) const = 0;

    virtual void OverrideGpuMemHeaps(GpuMemoryRequirements* pMemReqs) const { }

    virtual bool IsRestrictedTiledMultiMediaSurface() const;

    bool HasFastClearMetaData() const { return m_fastClearMetaDataOffset != 0; }
    gpusize FastClearMetaDataAddr(uint32 mipLevel) const;
    gpusize FastClearMetaDataOffset(uint32 mipLevel) const;
    gpusize FastClearMetaDataSize(uint32 numMips) const;

    virtual gpusize GetAspectBaseAddr(ImageAspect  aspect) const { PAL_NEVER_CALLED(); return 0; }

    uint32 TranslateClearCodeOneToNativeFmt(uint32 cmpIdx) const;

    // Returns an integer that represents the tiling mode associated with the specified subresource.
    virtual uint32 GetSwTileMode(const SubResourceInfo* pSubResInfo) const = 0;

    // Initializes the metadata in the given subresource range using CmdFillMemory calls. It may not be possible
    // for some gfxip layers to implement this function.
    virtual void InitMetadataFill(CmdBuffer* pCmdBuffer, const SubresRange& range) const = 0;

    // Helper function for AddrMgr1 to initialize the AddrLib surface info strucutre for a subresource.
    virtual Result Addr1InitSurfaceInfo(
        uint32                           subResIdx,
        ADDR_COMPUTE_SURFACE_INFO_INPUT* pSurfInfo) { return Result::ErrorUnavailable; }

    // Helper function for AddrMgr1 to finalize the subresource and tiling info for a subresource after
    // calling AddrLib.
    virtual void Addr1FinalizeSubresource(
        uint32                                  subResIdx,
        SubResourceInfo*                        pSubResInfoList,
        void*                                   pTileInfoList,
        const ADDR_COMPUTE_SURFACE_INFO_OUTPUT& surfInfo) { PAL_NEVER_CALLED(); }

    virtual void Addr2InitSubResInfo(
        const SubResIterator&  subResIt,
        SubResourceInfo*       pSubResInfoList,
        void*                  pSubResTileInfoList,
        gpusize*               pGpuMemSize) { PAL_NEVER_CALLED(); }

    // Helper function for AddrMgr2 to finalize the addressing information for an aspect plane.
    virtual Result Addr2FinalizePlane(
        SubResourceInfo*                               pBaseSubRes,
        void*                                          pBaseTileInfo,
        const ADDR2_GET_PREFERRED_SURF_SETTING_OUTPUT& surfaceSetting,
        const ADDR2_COMPUTE_SURFACE_INFO_OUTPUT&       surfaceInfo) { return Result::ErrorUnavailable; }

    // Helper function for AddrMgr2 to finalize the subresource info for a subresource after calling AddrLib.
    virtual void Addr2FinalizeSubresource(
        SubResourceInfo*                               pSubResInfo,
        const ADDR2_GET_PREFERRED_SURF_SETTING_OUTPUT& surfaceSetting) const { PAL_NEVER_CALLED(); }

    virtual Result Finalize(
        bool               dccUnsupported,
        SubResourceInfo*   pSubResInfoList,
        void*              pTileInfoList,
        ImageMemoryLayout* pGpuMemLayout,
        gpusize*           pGpuMemSize,
        gpusize*           pGpuMemAlignment) = 0;

    void PadYuvPlanarViewActualExtent(
        SubresId  subresource,
        Extent3d* pActualExtent) const;

protected:
    GfxImage(
        Image*        pParentImage,
        ImageInfo*    pImageInfo,
        const Device& device);

    static uint32 GetDepthStencilStateIndex(ImageAspect dsAspect)
    {
        PAL_ASSERT(dsAspect == ImageAspect::Depth || dsAspect == ImageAspect::Stencil);
        return static_cast<uint32>(dsAspect == ImageAspect::Stencil);
    }

    static void UpdateMetaDataLayout(
        ImageMemoryLayout* pGpuMemLayout,
        gpusize            offset,
        gpusize            alignment);
    static void UpdateMetaDataHeaderLayout(
        ImageMemoryLayout* pGpuMemLayout,
        gpusize            offset,
        gpusize            alignment);
    void InitFastClearMetaData(
        ImageMemoryLayout* pGpuMemLayout,
        gpusize*           pGpuMemSize,
        size_t             sizePerMipLevel,
        gpusize            alignment);

    void UpdateClearMethod(
        SubResourceInfo* pSubResInfoList,
        ImageAspect      aspect,
        uint32           mipLevel,
        ClearMethod      method);

    Image*const            m_pParent;
    const Device&          m_device;
    const ImageCreateInfo& m_createInfo;
    ImageInfo*const        m_pImageInfo;

    gpusize  m_fastClearMetaDataOffset;      // Offset to beginning of fast-clear metadata
    gpusize  m_fastClearMetaDataSizePerMip;  // Size of fast-clear metadata per mip level.

private:
    PAL_DISALLOW_DEFAULT_CTOR(GfxImage);
    PAL_DISALLOW_COPY_AND_ASSIGN(GfxImage);
};

} // Pal
