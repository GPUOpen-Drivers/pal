/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfxImage.h"
#include "palFormatInfo.h"
#include "palImage.h"
#include "palSysMemory.h"

namespace Pal
{

// Forward declarations
class      Device;
class      PrivateScreen;
struct     PrivateScreenCreateInfo;
struct     SubresId;
enum class ImageAspect : uint32;

// Shift the 64-bit wide address by 8 to get 256 byte-aligned address, and return the low DWORD of that shifted
// address.
//
// The maximum number of address bits which GFXIP 6+ supports is 48. Some parts are limited to 40 bits.
PAL_INLINE uint32 Get256BAddrLo(
    gpusize virtAddr)
{
    PAL_ASSERT((virtAddr & 0xff) == 0);
    return static_cast<uint32>(virtAddr >> 8);
}

// Shift the 64-bit wide address by 8 to get 256 byte-aligned address, and return the high DWORD of that shifted
// address.
//
// The maximum number of address bits which GFXIP 6+ supports is 48. Some parts are limited to 40 bits.
PAL_INLINE uint32 Get256BAddrHi(
    gpusize virtAddr)
{
    PAL_ASSERT((virtAddr & 0xFF) == 0);
    return static_cast<uint32>(virtAddr >> 40);
}

// Shifts and combines the low and high DWORD's of a 256 byte-aligned address to get the original GPU virtual
// address.
PAL_INLINE gpusize GetOriginalAddress(
    uint32 virtAddr256BLo,
    uint32 virtAddr256BHi)
{
    return ((static_cast<gpusize>(virtAddr256BHi) << 40) | (static_cast<gpusize>(virtAddr256BLo) << 8));
}

// Shift the 64-bit wide address by 8 to get 256 byte-aligned address, and bitwise-OR a bank/pipe swizzle then return
// a 32-bit base address.
//
// The maximum number of address bits which GFXIP 6+ supports is 48. Some parts are limited to 40 bits.
PAL_INLINE uint32 Get256BAddrSwizzled(
    gpusize virtAddr,
    uint32  swizzle)
{
    return Get256BAddrLo(virtAddr) | swizzle;
}

// Enumerates the various types of supported clear methods.
enum class ClearMethod : uint32
{
   NormalCompute     = 0,   // Normal, "slow" clear using a compute pipeline
   NormalGraphics    = 1,   // Normal, "slow" clear using a graphics pipeline
   Fast              = 2,   // "Fast" clear using CMask or HTile
   DepthFastGraphics = 3,   // "Fast" depth clear using graphics pipeline
};

// Enumerates the various types of supported resolve methods.
union ResolveMethod
{
    struct
    {
        uint32 fixedFunc        :  1;   // Fixed function CB resolve
        uint32 shaderPs         :  1;   // Generic shader-based resolve, using draws
        uint32 shaderCs         :  1;   // Generic shader-based resolve, using dispatch
        uint32 shaderCsFmask    :  1;   // Shader-based resolve, using dispatch (Fmask-accelerated)
        uint32 depthStencilCopy :  1;   // Fixed function Depth_Copy/Stencil_Copy resolve
        uint32 reserved         : 27;
    };
    uint32 u32All;
};

// Creation flags for an internal image object.
union InternalImageFlags
{
    struct
    {
        uint32 hwRotationEnabled           :  1;  // If the DCE will scan vertically to achieve hw rotation
        uint32 primarySupportsNonLocalHeap :  1;  // The resource is primary (flippable) and can be in a non-local heap
        uint32 privateScreenPresent        :  1;  // The image is created for private screen present
        uint32 stereo                      :  1;  // Image supports stereoscopic rendering and display.  Implies an
                                                  // array size of 2.
        uint32 useSharedTilingOverrides    :  1;  // Enables the shared image tiling parameters in
                                                  // ImageInternalCreateInfo to override PAL's normal
                                                  // addressing calculations.  Should be set when opening
                                                  // external shared images.
        uint32 turbosync                   :  1;  // Image supports turbosync flip.
        uint32 useSharedMetadata           :  1;  // Indicate SharedMetadataInfo will be used.
        uint32 placeholder0                :  1;  // Placeholder.
        uint32 placeholder1                :  1;  // Placeholder.
        uint32 vrsOnlyDepth                :  1;  // Setting this causes an image to allocate memory only for its hTile.
                                                  // Meant for use with VRS when the client hasn't bound a depth buffer.
        uint32 useSharedDccState           :  1;  // Use the shared dcc block sizing
        uint32 reserved                    : 21;
    };
    uint32 value;
};

// Display Dcc capabilities
union DisplayDccCaps
{
    struct
    {
        uint32 enabled                   : 1;
        uint32 rbAligned                 : 1;  // allow RB aligned
        uint32 pipeAligned               : 1;  // allow Pipe aligned

        // MaxUncompressedBlockSize_MaxCompressedBlockSize_IndependentBlockControl
        uint32 dcc_256_256_unconstrained : 1;
        uint32 dcc_256_128_128           : 1;
        uint32 dcc_128_128_unconstrained : 1;
        uint32 dcc_256_64_64             : 1;
        uint32 reserved                  : 25;
    };
    uint32 value;
};

// PAL internal-only creation info for an image object.
struct ImageInternalCreateInfo
{
    DisplayDccCaps     displayDcc;            // DisplayDcc parameters
    uint32             primaryTilingCaps;     // tiling caps for primaries(flippable images)
    uint32             mallCursorCacheSize;   // Size of the MALL cursor cache in bytes
    union
    {
        struct
        {
            AddrTileMode       sharedTileMode;     // Tile mode for shared image
            AddrTileType       sharedTileType;     // Tile type for shared image
            uint32             sharedTileSwizzle;  // Tile swizzle for shared image
            int32              sharedTileIndex;    // Tile index for shared image
        } gfx6;

        struct
        {
            AddrSwizzleMode    sharedSwizzleMode;       // Swizzle mdoe for shared iamge
            uint32             sharedPipeBankXor;       // Pipe-bank-xor setting for shared image
            uint32             sharedPipeBankXorFmask;  // Pipe-bank-xor setting for fmask
            DccState           sharedDccState;          // DCC state shared
        } gfx9;
    };

    const Image*       pOriginalImage;       // Original image (peer image)
    InternalImageFlags flags;                // Flags to create an internal image object
    SharedMetadataInfo sharedMetadata;       // Shared metadata info
};

// Contains the information describing a image's sub-resource.
struct SubResourceInfo
{
    // General information about this subresource:
    SubresId       subresId;             // This subresource's plane, mip level, and array slice.
    SwizzledFormat format;               // This subresource's texel format.
    uint32         bitsPerTexel;         // The number of bits per texel in the above format.
    uint8          swizzleEqIndex;       // This subresource's swizzle equation (or InvalidSwizzleEqIndex).
    ClearMethod    clearMethod;          // Clear method RPM will use for this subresource.

    // The extent of this subresource in terms of both texels and elements from the perspective of the caller (unpadded)
    // and the hardware (padded). For most formats elements and texels are equivalent; for BC formats an element is a
    // block and for certain "expanded" formats an element is a single color channel (e.g., R32G32B32).
    Extent3d       extentTexels;         // Width, height, and depth in units of texels.
    Extent3d       extentElements;       // Width, height, and depth in elements (e.g., blocks for BC formats).
    Extent3d       actualExtentTexels;   // Padded width, height, and depth in units of texels.
    Extent3d       actualExtentElements; // Padded width, height, and depth in elements (e.g., blocks for BC formats).
    uint32         actualArraySize;      // Padded array size. (possibly pow2-padded for GFX6-8).

    // Information about how the subresource is laid out in memory.
    gpusize        size;                 // Size of the subresource in bytes.
    gpusize        offset;               // Byte offset from start of image GPU memory.
    gpusize        swizzleOffset;        // Offset used for supporting parameterized swizzle
    gpusize        baseAlign;            // Base address alignment in bytes.
    gpusize        rowPitch;             // Row pitch in bytes.
    gpusize        depthPitch;           // Depth pitch in bytes.

    uint32         tileToken;            // This subresource's tiling token.

    gpusize        stereoOffset;         // Mem offset to the right eye data, in bytes
    uint32         stereoLineOffset;     // Y offset to the right eye data, in texels

    // Dimensions in pixels for a tile block - micro tile for 1D tiling and macro tile for 2D tiling.
    Extent3d       blockSize;
    Offset3d       mipTailCoord;         // coords of the subresource within the mip tail

    union
    {
        struct
        {
            uint32 supportMetaDataTexFetch : 1;  // Set if TC compatability on this sub resource is allowed.
            uint32 reserved                : 31;
        };
        uint32     u32All;
    } flags;

    ImageLayout defaultGfxLayout;
};

enum class DccFormatEncoding : uint32
{
    Incompatible     = 0, // uncompatible view formats.
    SignIndependent  = 1, // Use sign independednt encoding because clearing to a clear color when there
                          // is 1 in any of the channels ((1,1,1,1), (0,0,0,1), (1,1,1,0) and when
                          // signed-unsigned view formats are mixed
                          // (R8G8B8A8 UNORM surface viewed as an R8G8B8A8 SNORM SRV) will be messed up
    Optimal          = 2, // Indicates that all possible view formats are DCC compatible

};

// Contains the information describing an image resource, including the creation info and other information not tied
// to a particular sub-resource.
struct ImageInfo
{
    ImageInternalCreateInfo internalCreateInfo;    // Internal create info
    size_t                  numPlanes;             // Number of planes in the image
    size_t                  numSubresources;       // Total number of subresources in the image
    ResolveMethod           resolveMethod;         // Resolve method RPM will use for this Image
    DccFormatEncoding       dccFormatEncoding;     // Indicates how possible view formats will be encoded
};

// =====================================================================================================================
// Base class for Image resources. Contains sub-objects for each hardware IP block which has HW-specific behavior
// related to Images. Also owns the lists of SubResourceInfo and AddrMgr-specific tiling info structures.
class Image : public IImage
{
public:

    static constexpr ClearMethod DefaultSlowClearMethod = ClearMethod::NormalGraphics;
    static constexpr bool PreferGraphicsCopy = true;
    static constexpr bool ForceExpandHiZRangeForResummarize = false;
    static constexpr bool UseCpPacketOcclusionQuery = true;

    static Result ValidateCreateInfo(
        const Device*                  pDevice,
        const ImageCreateInfo&         imageInfo,
        const ImageInternalCreateInfo& internalCreateInfo);
    static Result ValidatePresentableCreateInfo(
        const Device*                     pDevice,
        const PresentableImageCreateInfo& createInfo);
    static Result ValidatePrivateCreateInfo(
        const Device*                       pDevice,
        const PrivateScreenImageCreateInfo& createInfo);

    // Helper function which computes the size of all data needed by an Image for storing data describing its
    // subresources. This *does not* include the size of the Image object!
    static size_t GetTotalSubresourceSize(
        const Device&          device,
        const ImageCreateInfo& createInfo);

    virtual ~Image();

    virtual Result Init();

    virtual void Destroy() override;
    void DestroyInternal();

    virtual Result GetSubresourceLayout(SubresId subresId, SubresLayout* pLayout) const override;
    virtual Result BindGpuMemory(IGpuMemory* pGpuMemory, gpusize offset) override;

    Device* GetDevice() const { return m_pDevice; }

    virtual void GetGpuMemoryRequirements(GpuMemoryRequirements* pGpuMemReqs) const override;

    const ImageInfo& GetImageInfo() const { return m_imageInfo; }

    const SubResourceInfo* SubresourceInfo(const SubresId& subres) const
        { return SubresourceInfo(CalcSubresourceId(subres)); }

    const SubResourceInfo* SubresourceInfo(uint32 subresId) const
        { return (m_pSubResInfoList + subresId); }

    const void* SubresourceTileInfo(uint32 subResId) const
        { return Util::VoidPtrInc(m_pTileInfoList, (subResId * m_tileInfoBytes)); }

    virtual const ImageMemoryLayout& GetMemoryLayout() const override
        { return m_gpuMemLayout; }

    // Gets base address of a subresource
    gpusize GetSubresourceBaseAddr(const SubresId& subresource) const
        { return GetSubresourceBaseAddr(CalcSubresourceId(subresource)); }

    gpusize GetSubresourceBaseAddr(uint32 subresId) const
        { return (m_vidMem.GpuVirtAddr() + m_pSubResInfoList[subresId].offset); }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    // Determine which subresource plane is tied to the specified image aspect.
    uint32 GetPlaneFromAspect(ImageAspect aspect) const;
#endif

    bool IsHwRotated() const { return m_imageInfo.internalCreateInfo.flags.hwRotationEnabled; }

    void ValidateSubresRange(const SubresRange& range) const;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    void GetFullSubresourceRange(ImageAspect  aspect, SubresRange* pRange) const;
#else
    virtual Result GetFullSubresourceRange(SubresRange* pRange) const override;
#endif

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    // Returns true if the image aspect is valid for this image.
    bool IsAspectValid(ImageAspect aspect) const;
#endif

    // Returns true if the range covers all of the mips and slices of the image (regardless of planes).
    bool IsRangeFullPlane(const SubresRange&  range) const
    {
        return ((range.numMips == m_createInfo.mipLevels)   &&
                (range.numSlices == m_createInfo.arraySize) &&
                (range.startSubres.arraySlice == 0)         &&
                (range.startSubres.mipLevel == 0));
    }

    bool IsSubresourceValid(const SubresId& subresource) const
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
        return ((IsAspectValid(subresource.aspect)) &&
#else
        return ((subresource.plane      < m_imageInfo.numPlanes)  &&
#endif
                (subresource.mipLevel   < m_createInfo.mipLevels) &&
                (subresource.arraySlice < m_createInfo.arraySize));
    }

    // Returns whether or not this Image can be used as a render target.
    bool IsRenderTarget() const
        { return (m_createInfo.usageFlags.colorTarget != 0); }

    // Returns whether or not this Image can be used as a depth/stencil target.
    bool IsDepthStencilTarget() const
        { return (m_createInfo.usageFlags.depthStencil != 0); }

    // Returns whether or not this Image has a depth plane.
    bool HasDepthPlane() const
    {
        return ((IsDepthStencilTarget() || Formats::IsDepthStencilOnly(m_createInfo.swizzledFormat.format)) &&
                (m_createInfo.swizzledFormat.format != ChNumFormat::X8_Uint));
    }

    // Returns whether or not this Image has depth data in the specified plane.
    bool IsDepthPlane(uint32 plane) const
        { return (HasDepthPlane() && (plane == 0)); }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    // Returns whether or not this Image has a depth plane in the specified range.
    bool HasDepthPlane(const SubresRange& range) const
        { return IsDepthPlane(range.startSubres.plane); }
#endif

    // Returns whether or not this Image has stencil data in the specified plane.
    bool IsStencilPlane(uint32 plane) const
    {
        return ((IsDepthStencilTarget() || Formats::IsDepthStencilOnly(m_createInfo.swizzledFormat.format)) &&
                ((plane == 1) ||
                ((plane == 0) && (m_createInfo.swizzledFormat.format == ChNumFormat::X8_Uint))));
    }

    // Returns whether or not this Image has a stencil plane.
    bool HasStencilPlane() const
    {
        return (IsDepthStencilTarget() &&
                ((m_imageInfo.numPlanes == 2) ||
                (m_createInfo.swizzledFormat.format == ChNumFormat::X8_Uint)));
    }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    // Returns whether or not this Image has a stencil plane in the specified range.
    bool HasStencilPlane(const SubresRange& range) const
        { return (IsStencilPlane(range.startSubres.plane) || (IsDepthStencilTarget() && (range.numPlanes == 2))); }
#endif

    // Returns whether or not this Image has color data that is not YUV in the specified plane.
    bool IsColorPlane(uint32 plane) const
    {
        return ((plane == 0) &&
                (m_imageInfo.numPlanes == 1) &&
                (IsDepthStencilTarget() == false) &&
                (Formats::IsYuv(m_createInfo.swizzledFormat.format) == false));
    }

    // Returns whether or not this Image can be used for shader read access.
    bool IsShaderReadable() const
        { return (m_createInfo.usageFlags.shaderRead != 0); }

    // Returns whether or not this Image can be used for shader write access.
    bool IsShaderWritable() const
        { return (m_createInfo.usageFlags.shaderWrite != 0); }

    // Returns whether or not this image can be used as resolve source access
    bool IsResolveSrc() const
        { return (m_createInfo.usageFlags.resolveSrc != 0); }

    // Returns whether or not this image can be used as resolve destination access
    bool IsResolveDst() const
        { return (m_createInfo.usageFlags.resolveDst != 0); }

    // Returns whether or not this image will use 24-bit format for HW programming
    bool IsDepthAsZ24() const
        { return (m_createInfo.usageFlags.depthAsZ24 != 0); }

    // Returns the dcc encoding for possible view formats
    DccFormatEncoding GetDccFormatEncoding() const
        { return m_imageInfo.dccFormatEncoding; }

    // Returns the first Mip which is shader writable if this value is 0 then entire
    // image is shader writable. Only relevent when shaderwritable flag is set.
    uint32 FirstShaderWritableMip() const
        { return m_createInfo.usageFlags.firstShaderWritableMip; }

    // Returns whether or not this Image is data invariant (e.g., tile swizzle disabled).
    bool IsDataInvariant() const
        { return (m_createInfo.flags.invariant != 0); }

    // Returns whether or not this Image can be used for cloning.
    bool IsCloneable() const
        { return (m_createInfo.flags.cloneable != 0); }

    // Returns whether or not this Image is a shareable or opened image.
    bool IsShared() const
        { return (m_createInfo.flags.shareable != 0); }

    // Return whether or not this Image is tmz protected.
    bool IsTmz() const
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 616
        return (m_createInfo.flags.tmzProtected);
#else
        return false;
#endif
    }

    // Returns whether or not this Image had metadata disabled by the client. This does NOT
    // tell you if Metadata does exist or not (PAL may still disable Metadata for other reasons).
    bool IsMetadataDisabledByClient() const
        { return (m_createInfo.metadataMode == MetadataMode::Disabled); }

    // Returns whether or not this Image is an opened peer image.
    bool IsPeer() const { return (m_imageInfo.internalCreateInfo.pOriginalImage != nullptr); }

    // Returns whether or not this Image is a presentable image.
    bool IsPresentable() const
        { return (m_createInfo.flags.presentable != 0); }

    // Returns whether or not this Image is a flippable image.
    bool IsFlippable() const
        { return (m_createInfo.flags.flippable != 0); }

    // Returns true if this is a private screen present image
    bool IsPrivateScreenPresent() const
        { return m_imageInfo.internalCreateInfo.flags.privateScreenPresent; }

    // Returns true if TurboSync surface
    bool IsTurboSyncSurface() const
        { return m_imageInfo.internalCreateInfo.flags.turbosync; }

    // Returns true if this is an EQAA image (i.e., fragment and sample counts differ).
    bool IsEqaa() const
        { return (m_createInfo.samples != m_createInfo.fragments); }

    const ImageInternalCreateInfo& GetInternalCreateInfo() const { return m_imageInfo.internalCreateInfo; }

    // Returns true if the specified sub-resource is linear, false if it's tiled
    bool IsSubResourceLinear(const SubresId& subresource) const
        { return (m_pGfxImage == nullptr) ? false : m_pGfxImage->IsSubResourceLinear(subresource); }

    // Returns whether or not the format of views created from the image can be different than the base format.
    // We can simply use the viewFormatCount parameter provided at image creation time as that's expected to
    // be zero in case the base format is the only valid view format.
    bool CanChangeFormat() const
        { return (m_createInfo.viewFormatCount != 0); }

    // Returns the memory object that's bound to this surface.  It's the callers responsibility to verify that the
    // returned object is valid.
    const BoundGpuMemory& GetBoundGpuMemory() const { return m_vidMem; }

    // Calculates the subresource ID based on provided subresource.
    uint32 CalcSubresourceId(const SubresId& subresource) const;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    SubresId GetBaseSubResource() const;
#endif

    // Gets base address of the image
    gpusize GetGpuVirtualAddr() const { return m_vidMem.GpuVirtAddr(); }

    // Gets the size of the memory for the base image
    gpusize GetGpuMemSize() const { return m_gpuMemSize; }

    GfxImage* GetGfxImage() const { return m_pGfxImage; }
    const Image* OriginalImage() const { return m_imageInfo.internalCreateInfo.pOriginalImage; }

    const PrivateScreen* GetPrivateScreen() const { return m_pPrivateScreen; }
    uint32 GetPrivateScreenImageId() const { return m_privateScreenImageId; }
    uint32 GetPrivateScreenIndex() const { return m_privateScreenIndex; }

    void SetPrivateScreen(PrivateScreen* pPrivateScreen);
    void SetPrivateScreenImageId(uint32 imageId) { m_privateScreenImageId = imageId; }

    static AddrFormat GetAddrFormat(const ChNumFormat format);

    static Result CreatePrivateScreenImage(
        Device*                             pDevice,
        const PrivateScreenImageCreateInfo& createInfo,
        void*                               pImagePlacementAddr,
        void*                               pGpuMemoryPlacementAddr,
        IImage**                            ppImage,
        IGpuMemory**                        ppGpuMemory);

    void InvalidatePrivateScreen() { m_pPrivateScreen = nullptr; }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    void DetermineFormatAndAspectForPlane(
        SwizzledFormat* pFormat,
        ImageAspect*    pAspect,
        uint32          plane) const;
#else
    void DetermineFormatForPlane(
        SwizzledFormat* pFormat,
        uint32          plane) const;
#endif

    // Returns whether or not this image prefers CB fixed function resolve
    bool PreferCbResolve() const
        { return (m_createInfo.flags.repetitiveResolve != 0); }

    bool PreferGraphicsScaledCopy() const { return m_preferGraphicsScaledCopy; };
    void SetPreferGraphicsScaledCopy(bool val) { m_preferGraphicsScaledCopy = val; };

protected:
    Image(Device*                        pDevice,
          void*                          pGfxImagePlacementAddr,
          void*                          pSubresInfoPlacementAddr,
          const ImageCreateInfo&         createInfo,
          const ImageInternalCreateInfo& internalCreateInfo);

    virtual void UpdateMetaDataInfo(IGpuMemory* pGpuMemory) { }

    Device*const    m_pDevice;
    ImageInfo       m_imageInfo;
    BoundGpuMemory  m_vidMem;

    GfxImage*       m_pGfxImage;

    SubResourceInfo*const  m_pSubResInfoList;   // Array of SubResourceInfo structures for each subresource
    void*const             m_pTileInfoList;     // Array of tile info structures for each subresource
    const size_t           m_tileInfoBytes;     // Size of each tile info structure, in bytes

private:
    uint32 DegradeMipDimension(uint32  mipDimension) const;

    static Result CreatePrivateScreenImageMemoryObject(
        Device*      pDevice,
        IImage*      pImage,
        void*        pGpuMemoryPlacementAddr,
        IGpuMemory** ppGpuMemOut);

    gpusize            m_gpuMemSize;        // Required GPU memory size in bytes.
    gpusize            m_gpuMemAlignment;   // Required GPU memory alignment in bytes.
    ImageMemoryLayout  m_gpuMemLayout;      // High-level layout of this image in memory.

    // The private screen object if this image is created on it, thus this image can be used for presenting on this
    // private screen.
    PrivateScreen*  m_pPrivateScreen;
    // A private screen can only use a limited number of images for presenting. For example: on Windows, KMD only stores
    // MaxPrivateScreenImages (16) flip addresses (of presentable images). So a uint32 id from 0 to 15 is used to
    // represent the presentable images and also their associated slots of flip addresses.
    uint32          m_privateScreenImageId;
    // A cached index of private display index, this is to avoid a race condition between submission and hotplug.
    uint32          m_privateScreenIndex;
    // Whether we should use graphic engine when doing scaled copy. By default, use CS. If the image
    // has DCC and the hardware does not support compressed shader writes(i.e., GFX6 - 9), use GFX.
    bool            m_preferGraphicsScaledCopy;

    PAL_DISALLOW_DEFAULT_CTOR(Image);
    PAL_DISALLOW_COPY_AND_ASSIGN(Image);
};

extern void ConvertPrivateScreenImageCreateInfo(
    const PrivateScreenImageCreateInfo& privateImageCreateInfo,
    ImageCreateInfo*                    pImageInfo);

} // Pal
