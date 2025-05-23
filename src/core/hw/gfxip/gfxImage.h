/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
class      GfxCmdBuffer;
class      Image;
class      SubResIterator;
struct     GpuMemoryRequirements;
struct     ImageInfo;
struct     SubresId;
struct     SubResourceInfo;
enum class ClearMethod : uint32;

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

// Compute expand methods
enum UseComputeExpand : uint32
{
    UseComputeExpandDepth        = 0x00000001,
    UseComputeExpandMsaaDepth    = 0x00000002,
    UseComputeExpandDcc          = 0x00000004,
    UseComputeExpandDccWithFmask = 0x00000008,
    UseComputeExpandAlways       = 0x00000010,
};

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
    union {
        AddrSwizzleMode  v2;
    } fmaskSwizzleMode;

    gpusize             hiZOffset;
    gpusize             hiSOffset;
#if PAL_BUILD_GFX12
    Addr3SwizzleMode    hiZSwizzleMode;
    Addr3SwizzleMode    hiSSwizzleMode;
#endif
    gpusize             dccSize[MaxNumPlanes];
    gpusize             dccAlignment[MaxNumPlanes];
    gpusize             displayDccSize[MaxNumPlanes];
    gpusize             displayDccAlignment[MaxNumPlanes];
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

#if PAL_BUILD_GFX12
// DCC compressed/uncompressed block size settings
struct DccControlBlockSize
{
    uint32 maxUncompressedBlockSizePlane0 :  2;
    uint32 maxCompressedBlockSizePlane0   :  2;
    uint32 maxUncompressedBlockSizePlane1 :  2;
    uint32 maxCompressedBlockSizePlane1   :  2;
    uint32 reserved                       : 24;
};
#endif

// =====================================================================================================================
class GfxImage
{
public:
    virtual ~GfxImage() {}

    static constexpr uint32 UseComputeExpand = UseComputeExpandDepth | UseComputeExpandDcc;

    Image* Parent() const { return m_pParent; }

    virtual bool HasFmaskData() const = 0;

    virtual bool HasDisplayDccData() const { return false; }

    virtual bool IsFormatReplaceable(
        SubresId    subresId,
        ImageLayout layout,
        bool        isDst,
        uint8       disabledChannelMask = 0) const = 0;

    virtual bool IsSubResourceLinear(SubresId subresource) const = 0;

    virtual bool IsIterate256Meaningful(const SubResourceInfo* subResInfo) const { return false; }

    virtual void OverrideGpuMemHeaps(GpuMemoryRequirements* pMemReqs) const { }

    virtual bool IsRestrictedTiledMultiMediaSurface() const;

    virtual bool IsNv12OrP010FormatSurface() const;

    // Answers the question: "If I do shader writes in this layout, will it break my metadata?". For example, this
    // would return true if we promised that CopyDst would be compressed but tried to use a compute copy path.
    virtual bool ShaderWriteIncompatibleWithLayout(SubresId subresId, ImageLayout layout) const = 0;

    virtual void GetSharedMetadataInfo(SharedMetadataInfo* pMetadataInfo) const = 0;
    virtual void GetDisplayDccState(DccState* pState) const { PAL_NEVER_CALLED(); }
    virtual void GetDccState(DccState* pState) const { PAL_NEVER_CALLED(); }
#if PAL_BUILD_GFX12
    virtual void GetDccControlBlockSize(DccControlBlockSize* pBlockSize) const { PAL_NEVER_CALLED(); }
#endif

    // Mall only exists on Gfx9+ hardware, so base functions should do nothing
    virtual void SetMallCursorCacheSize(uint32 cursorSize) { }
    virtual gpusize GetMallCursorCacheOffset() { return 0; }

    virtual gpusize GetSubresourceAddr(SubresId  subresId) const = 0;
    gpusize GetSubresource256BAddr(SubresId  subresId) const
        { return GetSubresourceAddr(subresId) >> 8; }
    virtual gpusize GetPlaneBaseAddr(uint32 plane, uint32 arraySlice = 0) const { PAL_NEVER_CALLED(); return 0; }

    // Returns an integer that represents the tiling mode associated with the specified subresource.
    virtual uint32 GetSwTileMode(const SubResourceInfo* pSubResInfo) const = 0;

    virtual uint32 GetTileSwizzle(SubresId subresId) const = 0;
    virtual uint32 GetHwSwizzleMode(const SubResourceInfo* pSubResInfo) const = 0;

    // Returns true if this subresource is effectively swizzled as a 2D image.
    virtual bool   IsSwizzleThin(SubresId subresId) const;

    uint8 GetStencilPlane() const;

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

#if PAL_BUILD_GFX12
    virtual void Addr3InitSubResInfo(
        const SubResIterator&  subResIt,
        SubResourceInfo*       pSubResInfoList,
        void*                  pSubResTileInfoList,
        gpusize*               pGpuMemSize) { PAL_NEVER_CALLED(); }

    // Helper function for AddrMgr3 to finalize the addressing information for a plane.
    virtual Result Addr3FinalizePlane(
        SubResourceInfo*                         pBaseSubRes,
        void*                                    pBaseTileInfo,
        Addr3SwizzleMode                         swizzleMode,
        const ADDR3_COMPUTE_SURFACE_INFO_OUTPUT& surfaceInfo) { return Result::ErrorUnavailable; }

    // Helper function for AddrMgr3 to finalize the subresource info for a subresource after calling AddrLib.
    virtual void Addr3FinalizeSubresource(
        SubResourceInfo*  pSubResInfo,
        Addr3SwizzleMode  swizzleMode) const { PAL_NEVER_CALLED(); }
#endif

    virtual Result Finalize(
        bool               dccUnsupported,
        SubResourceInfo*   pSubResInfoList,
        ImageMemoryLayout* pGpuMemLayout,
        gpusize*           pGpuMemSize,
        gpusize*           pGpuMemAlignment) = 0;

    virtual bool HasHtileData() const = 0;

    virtual bool IsFastColorClearSupported(
        GfxCmdBuffer*      pCmdBuffer,
        ImageLayout        colorLayout,
        const uint32*      pColor,
        const SubresRange& range) = 0;

    virtual bool IsFastDepthStencilClearSupported(
        ImageLayout        depthLayout,
        ImageLayout        stencilLayout,
        float              depth,
        uint8              stencil,
        uint8              stencilWriteMask,
        const SubresRange& range) const = 0;

    bool HasMisalignedMetadata() const { return m_hasMisalignedMetadata; }

    bool HasFastClearMetaData(uint32 plane) const { return m_fastClearMetaDataOffset[GetFastClearIndex(plane)] != 0; }
    bool HasFastClearMetaData(const SubresRange& range) const;

    gpusize FastClearMetaDataAddr(SubresId subresId) const;
    gpusize FastClearMetaDataOffset(SubresId subresId) const;
    gpusize FastClearMetaDataSize(uint32 plane, uint32 numMips) const;

    uint32 TranslateClearCodeOneToNativeFmt(uint32 cmpIdx) const;

    // Returns true if the specified mip level supports having a meta-data surface for the given mip level
    virtual bool CanMipSupportMetaData(uint32  mip) const { return true; }

    bool HasHiSPretestsMetaData() const { return m_hiSPretestsMetaDataOffset != 0; }
    gpusize HiSPretestsMetaDataAddr(uint32 mipLevel) const;
    gpusize HiSPretestsMetaDataOffset(uint32 mipLevel) const;
    gpusize HiSPretestsMetaDataSize(uint32 numMips) const;

    // Returns true if a clear operation was ever performed with a non-TC compatible clear color.
    bool    HasSeenNonTcCompatibleClearColor() const { return (m_hasSeenNonTcCompatClearColor == true); }
    void    SetNonTcCompatClearFlag(bool value) { m_hasSeenNonTcCompatClearColor = value; }
    bool    IsFceOptimizationEnabled() const { return (m_pNumSkippedFceCounter!= nullptr); };
    uint32* GetFceRefCounter() const { return m_pNumSkippedFceCounter; }
    uint32  GetFceRefCount() const;
    void    IncrementFceRefCount();

#if PAL_BUILD_GFX12
    bool EnableClientCompression(bool disableClientCompression) const;
#endif

protected:
    GfxImage(
        Image*        pParentImage,
        ImageInfo*    pImageInfo,
        const Device& device);

    static void UpdateMetaDataLayout(
        ImageMemoryLayout* pGpuMemLayout,
        gpusize            offset,
        gpusize            alignment);

    static void UpdateMetaDataHeaderLayout(
        ImageMemoryLayout* pGpuMemLayout,
        gpusize            offset,
        gpusize            alignment);

    void InitHiSPretestsMetaData(
        ImageMemoryLayout* pGpuMemLayout,
        gpusize*           pGpuMemSize,
        size_t             sizePerMipLevel,
        gpusize            alignment);

    void InitFastClearMetaData(
        ImageMemoryLayout* pGpuMemLayout,
        gpusize*           pGpuMemSize,
        size_t             sizePerMipLevel,
        gpusize            alignment,
        uint32             planeIndex = 0);

    void UpdateClearMethod(
        SubResourceInfo* pSubResInfoList,
        uint32           plane,
        uint32           mipLevel,
        ClearMethod      method);

    uint32 GetFastClearIndex(uint32 plane) const;

    virtual void Destroy();

    Image*const            m_pParent;
    const Device&          m_device;
    const ImageCreateInfo& m_createInfo;
    ImageInfo*const        m_pImageInfo;
    bool                   m_hasMisalignedMetadata;

    gpusize  m_fastClearMetaDataOffset[MaxNumPlanes];      // Offset to beginning of fast-clear metadata.
    gpusize  m_fastClearMetaDataSizePerMip[MaxNumPlanes];  // Size of fast-clear metadata per mip level.

    gpusize  m_hiSPretestsMetaDataOffset;     // Offset to beginning of HiSPretest metadata
    gpusize  m_hiSPretestsMetaDataSizePerMip; // Size of HiSPretest metadata per mip level.

    bool     m_hasSeenNonTcCompatClearColor;  // True if this image has been cleared with non TC-compatible color.

    uint32*  m_pNumSkippedFceCounter;

private:
    PAL_DISALLOW_DEFAULT_CTOR(GfxImage);
    PAL_DISALLOW_COPY_AND_ASSIGN(GfxImage);
};

} // Pal
