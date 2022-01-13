/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
/**
 ***********************************************************************************************************************
 * @file  palImage.h
 * @brief Defines the Platform Abstraction Library (PAL) IImage interface and related types.
 ***********************************************************************************************************************
 */

#pragma once

#include "pal.h"
#include "palGpuMemoryBindable.h"

namespace Pal
{

// Forward declarations.
class      IImage;
class      IPrivateScreen;
class      IScreen;
class      ISwapChain;

/// When used as the value of the viewFormatCount parameter of image creation it indicates that all compatible formats
/// can be used for views of the created image.
constexpr uint32 AllCompatibleFormats = UINT32_MAX;

/// Specifies dimensionality of an image (i.e., 1D, 2D, or 3D).
enum class ImageType : uint32
{
    Tex1d = 0x0,
    Tex2d = 0x1,
    Tex3d = 0x2,
    Count
};

/// Specifies the tiling (address swizzling) to use for an image. When a linear tiled image is mapped its contents will
/// be laid out in row-major ordering. All other tiling modes require the use of swizzles equation to locate texels.
enum class ImageTiling : uint32
{
    Linear       = 0x0,  ///< Image is laid out in scan-line (row-major) order.
    Optimal      = 0x1,  ///< Image is laid out in a GPU-optimal order.
    Standard64Kb = 0x2,  ///< Image is laid out in the cross-IHV, 64KB, standard swizzle tiling.
    Count
};

/// Hints to pal to identify a preference for how this image is organized. This is a preference setting, and may be
/// ignored if pal believes better options exist.
enum class ImageTilingPattern : uint32
{
    Default     = 0x0,  ///< No swizzle mode is preferred.
    Standard    = 0x1,  ///< Prefer standard swizzle modes.
    XMajor      = 0x2,  ///< Prefer x-coordinate major swizzle modes.
    YMajor      = 0x3,  ///< Prefer y-coordinate major swizzle modes.
    Interleaved = 0x4,  ///< Prefer interleaved coordinate swizzle modes.
    Count
};

/// Hints to pal to select the appropriate tiling mode for a optimization target.
enum class TilingOptMode : uint32
{
    Balanced     = 0x0,  ///< Balance memory foorprint and rendering performance.
    OptForSpace  = 0x1,  ///< Optimize tiling mode for saving memory footprint
    OptForSpeed  = 0x2,  ///< Optimize tiling mode for rendering performance.
    Count
};

/// Image metadata modes.
enum class MetadataMode : uint16
{
    Default = 0,        ///< Default behavior.  PAL chooses if metadata should be present or not.
    ForceEnabled,       ///< Optimization Hint:  Tells PAL that the client would prefer Metadata if possible.
                        ///  useful for scenarios where metadata isn't an obvious win and clients can enable based
                        ///  on some hueristic or app-detect.
    Disabled,           ///< The Image will not contain any compression metadata.
    FmaskOnly,          ///< The color msaa Image will only contain Cmask/Fmask metadata; this mode is only valid for
                        ///  color msaa Image.
    Count,
};

/// Image metadata TC compat modes.
enum class MetadataTcCompatMode : uint16
{
    Default = 0,        ///< Default behavior.  PAL chooses if TC compat should be enabled (if compressed).
    ForceEnabled,       ///< Optimization Hint:  Tells PAL that the client would prefer Metadata is TC compat.
    Disabled,           ///< Optimization Hint:  Tells PAL that the client would prefer Metadata is not TC compat.
    Count,
};

/// Image shared metadata support level
enum class MetadataSharingLevel : uint32
{
    FullExpand  = 0,    ///< The metadata need to be fully expanded at ownership transition time.
    ReadOnly    = 1,    ///< The metadata are expected to have read-only usage after the ownership is transitioned.
    FullOptimal = 2,    ///< The metadata can remain as-is if possible at ownership transition time.
};

/// Specifies the type of PRT map image being created.
enum class PrtMapType : uint32
{
    None            = 0, ///< This is not an auxillary image used for PRT plus functionality.
    Residency       = 1, ///< Image data is really a low-resolution map containing the finest populated LOD
                         ///  for a particular UV space region.
    SamplingStatus  = 2, ///< Indicates the validity of a given tile on a per-mip level basis.
    Count,
};

/// Specifies how to interpret a clear color.
enum class ClearColorType : uint32
{
    Uint  = 0, ///< The color is stored as an unsigned integer in RGBA order in u32Color. It will be swizzled and
               ///  compacted before it is written to memory.
    Sint  = 1, ///< The color is stored as a signed integer in RGBA order in i32Color. It will be swizzled and
               ///  compacted before it is written to memory.
    Float = 2, ///< The color is stored as floating point in RGBA order. It will be swizzled and converted to the
               ///  appropriate numeric format before it is written to memory.
    Yuv   = 3, ///< The color is stored as an unsigned integer in YUVA order in u32Color. It will be swizzled and
               ///  compacted before it is written to memory. The client must clamp the clear color within the
               ///  valid range, e.g. [0, 255] for 8-bit.
};

/// Contains everything necessary to store and interpret a clear color.
struct ClearColor
{
    ClearColorType type;                   ///< How to interpret this clear color.
    uint8 disabledChannelMask;             ///< This 4 bits are used to selectively disable the A,B,G,R channels
                                           ///  from being written. 0 means write ABRG. 0xF means write nothing.
                                           ///  0x8 means write Blue, Green, Red. 0x7 means write Alpha. etc...

    union
    {
        uint32 u32Color[4]; ///< The clear color, interpreted as four unsigned integers.
        float  f32Color[4]; ///< The clear color, interpreted as four floating point values.
    };
};

/// Specifies a set of image creation flags.
union ImageCreateFlags
{
    struct
    {
        uint32 invariant               :  1; ///< Images with this flag set and all other creation identical are
                                             ///  guaranteed to have a consistent data layout.
        uint32 cloneable               :  1; ///< Image is valid as a source or destination of a clone operation.
        uint32 shareable               :  1; ///< Image can be shared between compatible devices.
        uint32 presentable             :  1; ///< Indicates this image can be used in presents.
        uint32 flippable               :  1; ///< Image can be used for flip presents.
        uint32 stereo                  :  1; ///< Indicates AMD quad buffer stereo extension (AQBS extension) image
        uint32 dxgiStereo              :  1; ///< Indicates DXGI stereo (Win8 stereo) image
        uint32 cubemap                 :  1; ///< Image will be used as a cubemap.
        uint32 prt                     :  1; ///< Image is a partially resident texture (aka, sparse image or tiled
                                             ///  resource)
        uint32 needSwizzleEqs          :  1; ///< Image requires valid swizzle equations.
        uint32 perSubresInit           :  1; ///< The image may have its subresources initialized independently using
                                             ///  CmdBarrier calls out of the uninitialized layout.
        uint32 separateDepthPlaneInit  :  1; ///< If set, the caller may transition the stencil and depth planes from
                                             ///  "Uninitialized" state at any time.  Otherwise, both planes must be
                                             ///  transitioned in the same barrier call.  Only meaningful if
                                             /// "perSubresInit" is set.
        uint32 repetitiveResolve       :  1; ///< Optimization: Is this image resolved multiple times to an image which
                                             ///  is mostly similar to this image?
        uint32 preferSwizzleEqs        :  1; ///< Image prefers valid swizzle equations, but an invalid swizzle
                                             ///  equation is also acceptable.
        uint32 fixedTileSwizzle        :  1; ///< Fix this image's tile swizzle to ImageCreateInfo::tileSwizzle. This
                                             ///  is only supported for single-sampled color images.
        uint32 videoReferenceOnly      :  1; ///< Image is used by video hardware for reference buffer only.
                                             ///  It uses a different tiling format than the decoder output buffer.
        uint32 optimalShareable        :  1; ///< Indicates metadata information is to be added into private data on
                                             ///  creation time and honored on open time.
        uint32 sampleLocsAlwaysKnown   :  1; ///< Sample pattern is always known in client driver for MSAA depth image.
        uint32 fullResolveDstOnly      :  1; ///< Indicates any ICmdBuffer::CmdResolveImage using this image as a
                                             ///  desination will overwrite the entire image (width and height of
                                             ///  resolve region is same as width and height of resolve dst).
        uint32 fullCopyDstOnly         :  1; ///< Indicates any copy to this image will overwrite the entire image.
                                             ///  A perf optimization of using post-copy metadata fixup to replace heavy
                                             ///  expand at barrier to LayoutCopyDst. Unsafe to enable it if there is
                                             ///  potential partial copy to the image.
        uint32 pipSwapChain            :  1; ///< Indicates this image is PIP swap-chain. It is only supported on
                                             ///  Windows platforms.
        uint32 view3dAs2dArray         :  1; ///< If set client can view 3D image as 2D with its depth as array slices.
                                             ///  Note that not all 3D images supports it. The image creation will
                                             ///  return error if we fail to create a compatible image.

        uint32 tmzProtected            :  1; ///< Indicate this image is protected or not.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 681
        uint32 sharedWithMesa          :  1; ///< Indicate this Image was opened from a Mesa shared Image
        uint32 reserved                :  8; ///< Reserved for future use.
#else
        uint32 reserved                :  9; ///< Reserved for future use.
#endif
    };
    uint32 u32All;                           ///< Flags packed as 32-bit uint.
};

/// Specifies a set of ways an image might be used by the GPU (color target, shader read, etc.).
union ImageUsageFlags
{
    struct
    {
        uint32 shaderRead             :  1; ///< Image will be read from shader (i.e., texture).
        uint32 shaderWrite            :  1; ///< Image will be written from a shader (i.e., UAV).
        uint32 resolveSrc             :  1; ///< Image will be used as resolve source image
        uint32 resolveDst             :  1; ///< Image will be used as resolve dst image
        uint32 colorTarget            :  1; ///< Image will be bound as a color target.
        uint32 depthStencil           :  1; ///< Image will be bound as a depth/stencil target.
        uint32 noStencilShaderRead    :  1; ///< Image will be neither read as stencil nor resolved on stencil plane.
                                            ///  Note that if resolveSrc bit has been set to indicate that the image
                                            ///  could be adopted as a resolveSrc image and there could be stencil
                                            ///  resolve, noStencilShaderRead must be set to 0, since shader-read
                                            ///  based stencil resolve might be performed.
        uint32 hiZNeverInvalid        :  1; ///< Hint to PAL indicating the client will guarantee that no operations
                                            ///  performed on this Image while it is in a decompressed state will cause
                                            ///  Hi-Z metadata to become invalid. This allows PAL to avoid an expensive
                                            ///  resummarization blit in some resource barriers.
        uint32 depthAsZ24             :  1; ///< Use a 24-bit format for HW programming of a native 32-bit surface.
                                            ///  If set, border color and Z-reference values are treated as Z-24.
        uint32 firstShaderWritableMip :  4; ///< Only relevant if the shaderWrite flag is set. Typically set to 0 so
                                            ///  entire image is writable. If non0, such as an image where only level0
                                            ///  is used as a color target and compute is used to generate mipmaps,PAL
                                            ///  may be able to enable additional compression on the baseLevels which
                                            ///  are used exclusively as color target and shader read.
        uint32 cornerSampling         :  1; ///< Set if this image will use corner sampling in image-read scenarios.
                                            ///  With corner sampling, the extent refers to the number of pixel corners
                                            ///  which will be one more than the number of pixels.  Border color is
                                            ///  ignored when corner sampling is enabled.

        uint32 vrsDepth               :  1; ///< Set if this depth image will be bound when VRS rendering is enabled.
        uint32 disableOptimizedDisplay:  1; ///< Do not create Display Dcc
        uint32 useLossy               :  1; ///< Set if this image may use lossy compression.
        uint32 stencilOnlyTarget      :  1; ///< This must be set if a stencil-only IDepthStencilView will be created
                                            ///< for this image.
        uint32 vrsRateImage           :  1; ///< This image is potentially used with CmdBindSampleRateImage
        uint32 reserved               : 13; ///< Reserved for future use.
    };
    uint32 u32All;                          ///< Flags packed as 32-bit uint.
};

/// Specifies properties for @ref IImage creation.  Input structure to IDevice::CreateImage().
///
/// Note that by default PAL may instruct the hardware to swizzle the contents of an image in memory; if this occurs
/// two images created with identical properties will not map their texels to the same offsets in GPU memory and may
/// even have different sizes. At the expense of performance this behavior can be limited by setting the invariant flag,
/// which guarantees that images with identical properties will have identical GPU memory layouts.
///
/// For single-sampled color images, there is a middle ground between these two modes. If the fixedTileSwizzle flag is
/// set, PAL will use the tileSwizzle property instead of generating its own swizzle value. The tileSwizzle value must
/// be obtained from the base subresource of a single-sampled color image with identical properties (excluding
/// fixedTileSwizzle and tileSwizzle). This allows the client to force certain similar images to share the same GPU
/// memory layouts without forcing all similar images to a single GPU memory layout.
struct ImageCreateInfo
{
    ImageCreateFlags   flags;             ///< Image creation flags.
    ImageUsageFlags    usageFlags;        ///< Image usage flags.

    ImageType          imageType;         ///< Dimensionality of image (1D/2D/3D).
    SwizzledFormat     swizzledFormat;    ///< Pixel format and channel swizzle.
    Extent3d           extent;            ///< Dimensions in pixels WxHxD.
    uint32             mipLevels;         ///< Number of mipmap levels.  Cannot be 0.
    uint32             arraySize;         ///< Number of slices.  Set to 1 for non-array images.
    uint32             samples;           ///< Number of coverage samples.  Set to 1 for single sample images.  Must be
                                          ///  greater than or equal to the number of fragments.
    uint32             fragments;         ///< Number of color/depth fragments.  Set to 1 for single sample images.
    ImageTiling        tiling;            ///< Controls layout of pixels in the image.
    ImageTilingPattern tilingPreference;  ///< Controls preferred tile swizzle organization for this image.
    TilingOptMode      tilingOptMode;     ///< Hints to pal to select the appropriate tiling mode.
    uint32             tileSwizzle;       ///< If fixedTileSwizzle is set, use this value for the image's base tile
                                          ///  swizzle.
    MetadataMode       metadataMode;      ///< Metadata behavior mode for this image.
    MetadataTcCompatMode metadataTcCompatMode; ///< TC compat mode for this image.
    uint32             maxBaseAlign;      ///< Maximum address alignment for this image or zero for an unbounded
                                          ///  alignment.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 694
    float              imageMemoryBudget; ///< The memoryBudget value used in SW addrlib to determine the minSizeBlk for
                                          ///  textures. It must be >= 0.0.
                                          ///  When in [0.0, 1.0), addrlib uses legacy logic to decide minSizeBlk.
                                          ///  When == 1.0, addrlib uses minimizeAlign.
                                          ///  When > 1.0, addrlib applies memory budget algorithm.
                                          ///  Despite 1.5 in tests show significant texture allocation size reduction,
                                          ///  default value 0.0 (legacy behavior) is recommended if not specified by
                                          ///  client.
#endif

    struct
    {
        PrtMapType         mapType;       ///< Indicates what sort of PRT meta-data is stored in this image   If
                                          ///  this image is PRT meta-data, then it can only be associated with
                                          ///  an image that is a power-of-two multiple bigger (or the same size).
                                          ///  Image properties needs to include "PrtFeaturePrtPlus" to create
                                          ///  PRT map images.  Format must be set to X8_Unorm for residency map and
                                          ///  sampling-status map types.
        Extent3d           lodRegion;     ///< Useful only if mapType is not "none".  Defines the region size of the
                                          ///  parent image that one pixel of this image matches with.  The map
                                          ///  image can only be paired with a parent image of matching dimensions.
                                          ///  This parameter can be left at zero.
    } prtPlus;

    /// The following members must be set to zero unless the client is creating a @ref ImageTiling::Linear image and
    /// wishes to directly specify the image's row and depth pitches.  In that case, they must be integer multiples of
    /// the alignments given by @ref IDevice::GetLinearImageAlignments, called with an appropriate maxElementSize.
    uint32             rowPitch;          ///< The image must have this row pitch for the first mip level (in bytes).
    uint32             depthPitch;        ///< The image must have this depth pitch for the first mip level (in bytes).

    Rational           refreshRate;       ///< The expected refresh rate when presenting this flippable or stereo image.

    uint32             viewFormatCount;   ///< Number of additional image formats views of this image can be used with
                                          ///  or the special value AllCompatibleFormats to indicate that all
                                          ///  compatible formats can be used as a view format.
    const SwizzledFormat* pViewFormats;   ///< Array of viewFormatCount number of additional image formats views of
                                          ///  this image can be used with. If viewFormatCount is AllCompatibleFormats,
                                          ///  this must be nullptr. The array should not contain the base format of
                                          ///  the image, as that's always assumed to be a supported view format,
                                          ///  so if the image is only expected to be used with the base format of
                                          ///  the image then viewFormatCount and pViewFormats should be left with
                                          ///  the default values of zero and nullptr, respectively.
                                          ///  Note that this array is consumed at image creation time and should
                                          ///  not be accessed afterwards through GetImageCreateInfo().

};

/// Specifies properties for presentable @ref IImage creation.  Input structure to IDevice::CreatePresentableImage().
struct PresentableImageCreateInfo
{
    union
    {
        struct
        {
            uint32 fullscreen   :  1;   ///< Image supports fullscreen presentation.
            uint32 stereo       :  1;   ///< Image supports stereoscopic rendering and display.
                                        ///  Implies an array size of 2. Fullscreen must be set.
            uint32 turbosync    :  1;   ///< Image supports turbosync flip
            uint32 peerWritable :  1;   ///< Indicates if the memory allocated will be writable by other devices
            uint32 tmzProtected :  1;   ///< Indicates this presenatble image's memory is tmz Protected.
            uint32 reserved     : 27;   ///< Reserved for future use.
        };
        uint32 u32All;                  ///< Flags packed as 32-bit uint.
    } flags;                            ///< Presentable image creation flags.

    SwizzledFormat      swizzledFormat; ///< Pixel format and channel swizzle.
    ImageUsageFlags     usage;          ///< Image usage flags.
    Extent2d            extent;         ///< Width/height of the image.
    const IScreen*      pScreen;        ///< Target screen for fullscreen presentable images.  Can be null if the
                                        ///  fullscreen flag is 0.
    OsDisplayHandle     hDisplay;       ///< Display handle of the local display system only for WSI.
    OsWindowHandle      hWindow;        ///< Window handle only for WSI.
    ISwapChain*         pSwapChain;     ///< SwapChain object which the presentable image belongs to.
    uint32              viewFormatCount;///< Number of additional image formats views of this image can be used with
                                        ///  or the special value AllCompatibleFormats to indicate that all
                                        ///  compatible formats can be used as a view format.
    const SwizzledFormat* pViewFormats; ///< Array of viewFormatCount number of additional image formats views of
                                        ///  this image can be used with. If viewFormatCount is AllCompatibleFormats,
                                        ///  this must be nullptr. The array should not contain the base format of
                                        ///  the image, as that's always assumed to be a supported view format,
                                        ///  so if the image is only expected to be used with the base format of
                                        ///  the image then viewFormatCount and pViewFormats should be left with
                                        ///  the default values of zero and nullptr, respectively.
                                        ///  Note that this array is consumed at image creation time and should
                                        ///  not be accessed afterwards through GetImageCreateInfo().
};

/// Specifies properties for private screen @ref IImage image creation.  Input structure to
/// IDevice::CreatePrivateScreenImage().
struct PrivateScreenImageCreateInfo
{
    union
    {
        struct
        {
            uint32 invariant       :  1; ///< Images with this flag set and all other creation identical are guaranteed
                                         ///  to have a consistent data layout.
            uint32 reserved        : 31; ///< Reserved for future use.
        };
        uint32 u32All;                 ///< Flags packed as 32-bit uint.
    } flags;                           ///< Private screen image creation flags.

    SwizzledFormat  swizzledFormat; ///< Pixel format and channel swizzle.
    ImageUsageFlags usage;          ///< Image usage flags.
    Extent2d        extent;         ///< Width/height of the image.
    IPrivateScreen* pScreen;        ///< Private screen this image is created on (then this image can be used to be
                                    ///  presented on this private screen).
    uint32             viewFormatCount;   ///< Number of additional image formats views of this image can be used with
                                          ///  or the special value AllCompatibleFormats to indicate that all
                                          ///  compatible formats can be used as a view format.
    const SwizzledFormat* pViewFormats;   ///< Array of viewFormatCount number of additional image formats views of
                                          ///  this image can be used with. If viewFormatCount is AllCompatibleFormats,
                                          ///  this must be nullptr. The array should not contain the base format of
                                          ///  the image, as that's always assumed to be a supported view format,
                                          ///  so if the image is only expected to be used with the base format of
                                          ///  the image then viewFormatCount and pViewFormats should be left with
                                          ///  the default values of zero and nullptr, respectively.
                                          ///  Note that this array is consumed at image creation time and should
                                          ///  not be accessed afterwards through GetImageCreateInfo().
};

/// Specifies parameters for opening another device's image for peer access from this device.  Input structure to
/// IDevice::OpenPeerImage().
struct PeerImageOpenInfo
{
    const IImage* pOriginalImage;  ///< Other device's image to be opened for peer access.
};

/// Specifies parameters for opening another non-PAL device's image for access from this device.  Input structure to
/// IDevice::OpenExternalSharedImage().
struct ExternalImageOpenInfo
{
    ExternalResourceOpenInfo resourceInfo;   ///< Information describing the external image.
    Extent3d                 extent;         ///< Expected extent for the external image. This reference value would be
                                             ///  ignored and use extents from shared metadata if any dimension of the
                                             ///  reference extent is zero.
    SwizzledFormat           swizzledFormat; ///< Pixel format and channel swizzle. Or UndefinedFormat to infer the
                                             ///  format internally.
    ImageCreateFlags         flags;          ///< Image Creation flags.
    ImageUsageFlags          usage;          ///< Image usage flags.
    IPrivateScreen*          pScreen;        ///< Private screen this image is created on, or null.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 681
    gpusize                  gpuMemOffset;   ///< GpuMemory offset
#endif
};

/// Reports the overall GPU memory layout of the entire image.  Output structure for IImage::GetMemoryLayout(). Unused
/// sections will have a size of zero, an offset of zero, and an alignment of one. The layout is split into:
///       + Image Data: The raw texel values for all subresources of the image.
///       + Image Metadata: Additional data that will be used to optimize GPU operations that access the image.
///       + Image Metadata Header: A special subsection of the metadata for small bits of data with weaker alignment.
struct ImageMemoryLayout
{
    gpusize     dataSize;                    ///< The size, in bytes, of the image's core data section.
    gpusize     dataAlignment;               ///< The alignment, in bytes, of the image's core data section.

    gpusize     metadataOffset;              ///< The offset, in bytes, of the image's metadata section.
    gpusize     metadataSize;                ///< The size, in bytes, of the image's metadata section.
    gpusize     metadataAlignment;           ///< The alignment, in bytes, of the image's metadata section.

    gpusize     metadataHeaderOffset;        ///< The offset, in bytes, of the image's metadata header.
    gpusize     metadataHeaderSize;          ///< The size, in bytes, of the image's metadata header.
    gpusize     metadataHeaderAlignment;     ///< The alignment, in bytes, of the image's metadata header.

    uint8       swizzleEqIndices[2];         ///< Which swizzle equations this image uses or InvalidSwizzleEqIndex if
                                             ///  there are no swizzle equations for this image's layout.
    uint8       swizzleEqTransitionMip;      ///< Before this mip level, the image uses swizzleEqIndices[0]; from this
                                             ///  mip level onwards, the image uses swizzleEqIndices[1].
    uint8       swizzleEqTransitionPlane;    ///< Before this mip plane, the image uses swizzleEqIndices[0]; from this
                                             ///  plane onward, the image uses swizzleEqIndices[1].

    uint32      prtTileWidth;                ///< Width, in texels, of a PRT tile
    uint32      prtTileHeight;               ///< Height, in texels, of a PRT tile
    uint32      prtTileDepth;                ///< Depth, in texels, of a PRT tile
    uint32      prtMinPackedLod;             ///< First mip level that is packed into the PRT mip tail.
    uint32      prtMipTailTileCount;         ///< Number of tiles in the packed mip tail. This may either indicate the
                                             ///  size per slice or per image depending on the support for
                                             ///  PrtFeaturePerLayerMipTail (@see PrtFeatureFlags)
    uint32      stereoLineOffset;            ///< Y offset to the right eye data, in texels
};

/// Collection of bitmasks specifying which operations are currently allowed on an image, and which queues are allowed
/// to perform those operations.  Based on this information, PAL can determine the best compression state of the image.
struct ImageLayout
{
    uint32 usages  : 24;  ///< Bitmask of @ref ImageLayoutUsageFlags values.
    uint32 engines :  8;  ///< Bitmask of @ref ImageLayoutEngineFlags values.
};

/// Reports position and memory layout information for a specific subresource in an image.  Output structure for
/// IImage::GetSubresourceLayout().
struct SubresLayout
{
    uint32   elementBytes;  ///< size of each element in bytes
    gpusize  offset;        ///< Offset in bytes from the base of the image's GPU memory where the subresource starts.
    gpusize  swizzleOffset; ///< Offset in bytes used for supporting parameterized swizzle
    gpusize  size;          ///< Size of the subresource in bytes.
    gpusize  rowPitch;      ///< Offset in bytes between the same X position on two consecutive lines of the subresource.
    gpusize  depthPitch;    ///< Offset in bytes between the same X,Y position of two consecutive slices.
    uint32   tileToken;     ///< Token representing various tiling information necessary for determining compatible
                            ///  optimally tiled copies.
    uint32   tileSwizzle;   ///< Bank/Pipe swizzle bits for macro-tiling modes.
    Extent3d blockSize;     ///< Size of a tile block in texels - micro tile for 1D tiling and macro tile for 2D tiling.
    Offset3d mipTailCoord;  ///< coords of the subresource within the mip tail

    /// Extent of the subresource in texels, including all internal padding for this subresource.
    Extent3d paddedExtent;

    /// Reports supported engines and usages for this subresource while it can remain in its optimal compression state.
    /// Clients using CmdRelease()/CmdAcquire() without complete knowledge of the application's next usage during
    /// CmdRelease() or its previous usage at CmdAcquire() can treat this layout as a performant target for an
    /// intermediate state that will avoid unnecessary decompressions.
    ///
    /// This value is only valid if supportSplitReleaseAcquire is set in @ref DeviceProperties.
    ImageLayout defaultGfxLayout;

    SwizzledFormat planeFormat; ///< Swizzled format for plane. Planar resource like D32-S8
                                /// will have different swizzled format per plane.

};

/// Selects a specific subresource of an image resource.
///
/// Most images only have a single data plane but in some cases conceptually related data will be stored in physically
/// separate locations which we call planes.  If an image only has a single plane it will always be plane 0.
/// We define the following fixed mappings for all multi-plane formats.
///       + Depth-stencil: if the image format contains depth and stencil data, plane 0 is depth and plane 1 is stencil.
///       + YUV-planar: if the image format is @ref YuvPlanar it has either two or three planes.  The luma plane
///         is always plane 0. If the format is @ref ChNumFormat::YV12 it has three planes where plane 1 is the
///         red-difference chrominance plane and plane 2 is the blue-difference chrominance plane. Otherwise, plane 1
///         interleaves blue-difference and red-difference chrominance values.
struct SubresId
{
    uint32 plane;      ///< Selects a data plane.
    uint32 mipLevel;   ///< Selects a mip level.
    uint32 arraySlice; ///< Selects an array slice.
};

/// Defines a range of subresources.
struct SubresRange
{
    SubresId startSubres;  ///< First subresource in the range.
    uint32   numPlanes;    ///< Number of planes in the range.
    uint32   numMips;      ///< Number of mip levels in the range.
    uint32   numSlices;    ///< Number of slices in the range.
};

/**
 ***********************************************************************************************************************
 * @interface IImage
 * @brief     Represents an image resource that can be accessed by the GPU.
 *
 * @see IDevice::CreateImage()
 * @see IDevice::OpenPeerImage()
 ***********************************************************************************************************************
 */
class IImage : public IGpuMemoryBindable
{
public:
    /// Reports information on the layout of the image in memory such as core data size and metadata alignment.
    ///
    /// @returns the reference to ImageCreateInfo
    virtual const ImageMemoryLayout& GetMemoryLayout() const = 0;

    /// Reports information on the full range of the image's subresources.
    ///
    /// @param [out] pRange  Reports info on the full range of the image's subresources such as number of mips and
    ///                      planes.
    ///
    /// @returns Success if the layout was successfully reported.  Otherwise, one of the following error codes may be
    ///          returned:
    ///          + ErrorInvalidPointer if pRange is null.
    virtual Result GetFullSubresourceRange(SubresRange* pRange) const = 0;

    /// Reports information on the layout of the specified subresource in memory.
    ///
    /// @param [in]  subresId Selects a subresource from the image (aspect/mip/slice).
    /// @param [out] pLayout  Reports info on the subresource layout such as size and pitch.
    ///
    /// @returns Success if the layout was successfully reported.  Otherwise, one of the following error codes may be
    ///          returned:
    ///          + ErrorInvalidPointer if pLayout is null.
    ///          + ErrorInvalidValue is the subresId is out of range for this image.
    virtual Result GetSubresourceLayout(
        SubresId      subresId,
        SubresLayout* pLayout) const = 0;

    /// Reports the create info of image.
    ///
    /// @returns the reference to ImageCreateInfo
    const ImageCreateInfo& GetImageCreateInfo() const { return m_createInfo; }

    /// Returns the value of the associated arbitrary client data pointer.
    /// Can be used to associate arbitrary data with a particular PAL object.
    ///
    /// @returns Pointer to client data.
    void* GetClientData() const
    {
        return m_pClientData;
    }

    /// Sets the value of the associated arbitrary client data pointer.
    /// Can be used to associate arbitrary data with a particular PAL object.
    ///
    /// @param  [in]    pClientData     A pointer to arbitrary client data.
    void SetClientData(
        void* pClientData)
    {
        m_pClientData = pClientData;
    }

    /// Sets level of optimal sharing by opening APIs using this optimal sharable image and pass this information to the
    /// creator. This function is supposed to be called by openers only. The call by creator is ignored.
    ///
    /// @param  [in]    level        Level to be set to specified client API.
    virtual void SetOptimalSharingLevel(
        MetadataSharingLevel level) = 0;

    /// Returns support level set by all possible opening APIs.
    ///
    /// @returns A summarized supporting level.
    virtual MetadataSharingLevel GetOptimalSharingLevel() const = 0;

    /// Gives the client access to the resource ID used for internal Pal events.
    /// EX: Resource Create, Resource Bind, Resource Destroy.
    ///
    /// @returns The Resource ID.
    virtual const void* GetResourceId() const = 0;

protected:
    /// @internal Constructor.
    ///
    /// @param [in] createInfo App-specified parameters describing the desired image properties.
    IImage(const ImageCreateInfo& createInfo) : m_createInfo(createInfo), m_pClientData(nullptr) { }

    /// @internal Destructor.  Prevent use of delete operator on this interface.  Client must destroy objects by
    /// explicitly calling IDestroyable::Destroy() and is responsible for freeing the system memory allocated for the
    /// object on their own.
    virtual ~IImage() { }

    /// Retained Image create info
    const ImageCreateInfo m_createInfo;

private:
    /// @internal Client data pointer. This can have an arbitrary value and can be returned by calling GetClientData()
    /// and set via SetClientData().
    /// For non-top-layer objects, this will point to the layer above the current object.
    void* m_pClientData;
};

} // Pal
