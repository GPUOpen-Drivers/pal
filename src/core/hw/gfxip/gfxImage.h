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
class      SubResIterator;
struct     GpuMemoryRequirements;
struct     ImageInfo;
struct     SubresId;
struct     SubResourceInfo;

// Mask of all image usage layout flags which are valid to use on depth/stencil Images.
constexpr uint32 AllDepthImageLayoutFlags = LayoutUninitializedTarget |
                                            LayoutDepthStencilTarget  |
                                            LayoutShaderRead          |
                                            LayoutShaderWrite         |
                                            LayoutCopySrc             |
                                            LayoutCopyDst             |
                                            LayoutResolveSrc          |
                                            LayoutResolveDst          |
                                            LayoutSampleRate;

// Internal flags set for opening shared metadata path.
union SharedMetadataFlags
{
    struct
    {
        uint32 shaderFetchable      : 1; // Main metadata is shader fetchable.
        uint32 shaderFetchableFmask : 1; // In case the FMASK shader-fetchable is different from main metadata. - TBD
        uint32 hasEqGpuAccess       : 1; // Metadata equation for GPU access following main metadata (DCC or HTILE).
                                         // CS-based fast-clear is disabled w/o this on GFX9.
        uint32 hasHtileLookupTable  : 1; // Htile look-up table for each mip and slice - DB fixed-func resolve is
                                         // disabled w/o this.
        uint32 htileHasDsMetadata   : 1; // Whether htile has depth/stencil metadata.
        uint32 hasCmaskEqGpuAccess  : 1; // Metadata equation for GPU access following cmask metadata.
        uint32 reserved             : 26;
    };
    uint32 value;
};

// Shared metadata info to be used for opened optimally shared image.
struct SharedMetadataInfo
{
    SharedMetadataFlags flags;
    uint32              numPlanes; // the number of valid indices into the various "MaxNumPlanes" arrays.
    gpusize             dccOffset[MaxNumPlanes];
    gpusize             displayDccOffset[MaxNumPlanes];
    uint32              pipeAligned[MaxNumPlanes];
    gpusize             cmaskOffset;
    gpusize             fmaskOffset;
    gpusize             fmaskXor;
    gpusize             htileOffset;
    gpusize             dccStateMetaDataOffset[MaxNumPlanes];
    gpusize             fastClearMetaDataOffset[MaxNumPlanes];
    gpusize             fastClearEliminateMetaDataOffset[MaxNumPlanes];
    gpusize             hisPretestMetaDataOffset; // Offset for HiSPrest meta data.
    gpusize             htileLookupTableOffset;
    uint64              resourceId; // This id is a unique name for the cross-process shared memory used to pass extra
                                    // information. Currently it's composed by the image object pointer and process id.
    AddrSwizzleMode     fmaskSwizzleMode;
    gpusize             hiZOffset;
    gpusize             hiSOffset;
};

// Display Dcc state for a plane
struct DccState
{
    gpusize primaryOffset;   // Byte offset in the allocation
    gpusize secondaryOffset; // Byte offset from the beginning of the first display surface
    uint32  size;            // Size of dcc key.
    uint32  pitch;           // In pixels
    struct
    {
        uint32 maxUncompressedBlockSize :2;
        uint32 maxCompressedBlockSize   :2;
        uint32 independentBlk64B        :1;
        uint32 independentBlk128B       :1;
        uint32 isDccForceEnabled        :1; // Whether dcc is force-enabled or force-disabled.
        uint32 reserved                 :25;
    };
};

// =====================================================================================================================
class GfxImage
{
public:
    virtual ~GfxImage() {}

    Image* Parent() const { return m_pParent; }

    virtual bool HasFmaskData() const = 0;

    virtual bool HasDisplayDccData() const { return false; }

    virtual bool IsFormatReplaceable(
        const SubresId& subresId,
        ImageLayout     layout,
        bool            isDst,
        uint8           disabledChannelMask = 0) const = 0;

    virtual bool IsSubResourceLinear(const SubresId& subresource) const = 0;

    virtual bool IsIterate256Meaningful(const SubResourceInfo* subResInfo) const { return false; }

    virtual void OverrideGpuMemHeaps(GpuMemoryRequirements* pMemReqs) const { }

    virtual bool IsRestrictedTiledMultiMediaSurface() const;

    virtual bool IsNv12OrP010FormatSurface() const;

    // Answers the question: "If I do shader writes in this layout, will it break my metadata?". For example, this
    // would return true if we promised that CopyDst would be compressed but tried to use a compute copy path.
    virtual bool ShaderWriteIncompatibleWithLayout(const SubresId& subresId, ImageLayout layout) const = 0;

    virtual void GetSharedMetadataInfo(SharedMetadataInfo* pMetadataInfo) const = 0;
    virtual void GetDisplayDccState(DccState* pState) const { PAL_NEVER_CALLED(); }
    virtual void GetDccState(DccState* pState) const { PAL_NEVER_CALLED(); }

    // Mall only exists on Gfx9+ hardware, so base functions should do nothing
    virtual void SetMallCursorCacheSize(uint32 cursorSize) { }
    virtual gpusize GetMallCursorCacheOffset() { return 0; }

    virtual gpusize GetSubresourceAddr(SubresId  subResId) const = 0;
    gpusize GetSubresource256BAddr(SubresId  subResId) const
        { return GetSubresourceAddr(subResId) >> 8; }
    virtual gpusize GetPlaneBaseAddr(uint32 plane, uint32 arraySlice = 0) const { PAL_NEVER_CALLED(); return 0; }

    // Returns an integer that represents the tiling mode associated with the specified subresource.
    virtual uint32 GetSwTileMode(const SubResourceInfo* pSubResInfo) const = 0;

    virtual uint32 GetTileSwizzle(const SubresId& subResId) const = 0;
    virtual uint32 GetHwSwizzleMode(const SubResourceInfo* pSubResInfo) const = 0;

    // Returns true if this subresource is effectively swizzled as a 2D image.
    virtual bool   IsSwizzleThin(const SubresId& subResId) const;

    uint32 GetStencilPlane() const;

    virtual void PadYuvPlanarViewActualExtent(SubresId subresource, Extent3d* pActualExtent) const;

    // Initializes the metadata in the given subresource range using CmdFillMemory calls. It may not be possible
    // for some gfxip layers to implement this function.
    virtual void InitMetadataFill(
        CmdBuffer*         pCmdBuffer,
        const SubresRange& range,
        ImageLayout        layout) const = 0;

    virtual void Addr2InitSubResInfo(
        const SubResIterator&  subResIt,
        SubResourceInfo*       pSubResInfoList,
        void*                  pSubResTileInfoList,
        gpusize*               pGpuMemSize) { PAL_NEVER_CALLED(); }

    // Helper function for AddrMgr2 to finalize the addressing information for a plane.
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
        ImageMemoryLayout* pGpuMemLayout,
        gpusize*           pGpuMemSize,
        gpusize*           pGpuMemAlignment) = 0;

    virtual Result GetDefaultGfxLayout(SubresId subresId, ImageLayout* pLayout) const = 0;

    bool HasMisalignedMetadata() const { return m_hasMisalignedMetadata; }

protected:
    GfxImage(
        Image*        pParentImage,
        ImageInfo*    pImageInfo,
        const Device& device);

    static void UpdateMetaDataLayout(
        ImageMemoryLayout* pGpuMemLayout,
        gpusize            offset,
        gpusize            alignment);

    virtual void Destroy() {}

    Image*const            m_pParent;
    const Device&          m_device;
    const ImageCreateInfo& m_createInfo;
    ImageInfo*const        m_pImageInfo;
    bool                   m_hasMisalignedMetadata;

private:
    PAL_DISALLOW_DEFAULT_CTOR(GfxImage);
    PAL_DISALLOW_COPY_AND_ASSIGN(GfxImage);
};

} // Pal
