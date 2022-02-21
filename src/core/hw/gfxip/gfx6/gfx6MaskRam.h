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

#pragma once

#include "pal.h"
#include "core/hw/gfxip/maskRam.h"
#include "core/hw/gfxip/gfx6/gfx6Chip.h"

namespace Pal
{
namespace Gfx6
{

// Forward decl's:
class Image;

// Represents an "image" of the fast-clear metadata used by Depth/Stencil Images.
struct Gfx6FastDepthClearMetaData
{
    regDB_STENCIL_CLEAR  dbStencilClear;    // Stencil clear value
    regDB_DEPTH_CLEAR    dbDepthClear;      // Depth clear value
};

// Represents an "image" of the HiSPretests metadata used by Depth/Stencil Images.
struct Gfx6HiSPretestsMetaData
{
    regDB_SRESULTS_COMPARE_STATE0 dbSResultCompare0;
    regDB_SRESULTS_COMPARE_STATE1 dbSResultCompare1;
};

// Contains the Mask RAM information for a single mipmap level of an Image.
struct MaskRamInfo
{
    gpusize  maskSize;   // Size of mask memory in bytes.
    gpusize  sliceSize;  // Slice size, in bytes.
    gpusize  baseAlign;  // Base alignment needed for mask memory.
    uint32   blockSize;  // Block size:
                         //  For CMask blockSize = (pitch*height)/128/128-1
                         //  For FMask blockSize = (pitch*height)/NumPixelsPerTile - 1;
};

// Contains the FMASK information for a single mipmap level of an Image.
struct FmaskInfo : public MaskRamInfo
{
    uint32  bankHeight;
    int32   tileIndex;      // Tile index (-1 if unused)
    uint32  bpp;            // Bits per pixel in mask
    uint32  pitch;          // Pitch in pixels
    uint32  height;         // Height in pixels
};

// Contains the DCC information for a single mipmap level of an Image.
struct DccInfo : public MaskRamInfo
{
    gpusize fastClearSize; // Size, in bytes, of any fast clears done against this DCC surface
    bool    sizeAligned;   // Indicates that DCC memory size is aligned, necessary to be fast-cleared
};

// Contains the HTILE information for a single mipmap level of an Image.
struct HtileInfo : public MaskRamInfo
{
    bool    slicesInterleaved;        // If slices are interleaved, they cannot be fast cleared seperately with compute
    bool    nextMipLevelCompressible; // Whether htile of next mip level is compressible. If not, memset fast clear is
                                      // not allowed on cur mip level since mip interleave ocurred. This only counts
                                      // for tc-compatible HTILE.
};

// Some operations need an easy way to specify which HTile plane they will read or write to.
enum HtilePlaneMask : uint32
{
    HtilePlaneDepth   = 0x1,
    HtilePlaneStencil = 0x2
};

// Enumerates all operations that may view HTile memory as a buffer.
enum class HtileBufferUsage : uint32
{
    Init  = 0x0, // Used to set Htile memory to its initial value.
    Clear = 0x1, // Used to set Htile memory to some non-initial value (e.g., a fast-clear).
};

// Specifies which HTile planes contain meaningful data, because the image and HTile may not have the same planes.
// For example, a depth-only image can still have the combined depth/stencil HTile (tileStencilDisable = 0), but the
// HTile stencil data will not be used.
enum class HtileContents : uint32
{
    DepthOnly    = 0,
    StencilOnly  = 1,
    DepthStencil = 2
};

// =====================================================================================================================
// Manages the HTile state for all slices of a single mipmap level of an Image resource.
class Gfx6Htile final : public MaskRam
{
public:
    Gfx6Htile();
    // Destructor has nothing to do.
    virtual ~Gfx6Htile() {}

    static bool UseHtileForImage(const Pal::Device& device, const Image& image, bool metaDataTexFetchSupported);

    uint32 GetInitialValue() const;
    uint32 GetClearValue(float depthValue) const;
    uint32 GetPlaneMask(uint32 planeFlags) const;
    uint32 GetPlaneMask(const Image& image, const SubresRange& range) const;

    Result Init(
        const Pal::Device& device,
        const Image&       image,
        uint32             mipLevel,
        gpusize*           pGpuOffset);

    bool DepthCompressed() const { return m_flags.compressZ; }
    bool StencilCompressed() const { return m_flags.compressS; }
    bool TileStencilDisabled() const { return m_flags.tileStencilDisable; }
    bool ZRangePrecision() const { return m_flags.zrangePrecision; }
    bool SlicesInterleaved() const { return m_flags.slicesInterleaved; }
    bool FirstInterleavedMip() const { return m_flags.firstInterleavedMip;  }
    const regDB_HTILE_SURFACE& DbHtileSurface() const { return m_dbHtileSurface; }
    const regDB_PRELOAD_CONTROL& DbPreloadControl() const { return m_dbPreloadControl; }
    HtileContents GetHtileContents() const { return m_htileContents; }

private:

    Result ComputeHtileInfo(
        const Pal::Device&     device,
        const Image&           image,
        const SubResourceInfo& subResInfo,
        bool                   isLinear,
        bool                   mipInterleavedChildMip,
        HtileInfo*             pHtileInfo) const;

    static HtileContents ExpectedHtileContents(
        const Pal::Device& device,
        const Image&       image);

    union Gfx6HtileFlags
    {
        struct
        {
            uint32 zrangePrecision          :  1; // Should zMin (0.f) or zMax (1.f) get more precision?
            uint32 compressZ                :  1; // Is depth compression enabled?
            uint32 compressS                :  1; // Is stencil compression enabled?
            uint32 tileStencilDisable       :  1; // Allows more Hi-Z precision for non-stencil formats
            uint32 slicesInterleaved        :  1; // If true, compute fast clears cannot be used
            uint32 firstInterleavedMip      :  1; // Whether the mip is the first mip-interleaved mip. If
                                                  // true, all following mips are not allowed to be tc-compatible
                                                  // and  memset fast clear is not allowed on cur mip.
            uint32 reserved                 : 26; // Reserved for future use
        };
        uint32 value; // Value of the flags bitmask
    };

    Gfx6HtileFlags         m_flags;             // HTile properties flags
    regDB_HTILE_SURFACE    m_dbHtileSurface;    // DB_HTILE_SURFACE register value
    regDB_PRELOAD_CONTROL  m_dbPreloadControl;  // DB_PRELOAD_CONTROL register value
    HtileContents          m_htileContents;     // Htile planes which contain meaningful data

    // Each DB's HTile cache can fit 8K DWORDs. Each DWORD of HTILE data covers 64 pixels.
    static constexpr uint32 DbHtileCacheSizeInPixels = (8 * 1024 * 64);

    // Mask of HTile bits used for stencil.
    static constexpr uint32 Gfx6HtileStencilMask = 0x000003F0;
    // Mask of HTile bits used for depth.
    static constexpr uint32 Gfx6HtileDepthMask   = 0xFFFFFC0F;

    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx6Htile);
};

// Represents an "image" of the fast-clear metadata used by Color Target Images.
struct Gfx6FastColorClearMetaData
{
    regCB_COLOR0_CLEAR_WORD0  cbColorClearWord0;    // Packed fast-clear color bits [31:0]
    regCB_COLOR0_CLEAR_WORD1  cbColorClearWord1;    // Packet fast-clear color bits [63:32]
};

// =====================================================================================================================
// Manages the CMask state for all slices of a single mipmap level of an Image resource.
class Gfx6Cmask final : public MaskRam
{
public:
    Gfx6Cmask();
    // Destructor has nothing to do.
    virtual ~Gfx6Cmask() {}

    static bool UseCmaskForImage(
        const Pal::Device& device,
        const Image&       image,
        bool               useDcc);

    Result Init(
        const Pal::Device& device,
        const Image&       image,
        uint32             mipLevel,
        gpusize*           pGpuOffset);

    // Returns true if the CMask buffer is linear.
    bool IsLinear() const { return m_flags.linear; }

    // Returns true if the CMask buffer supports fast color clears.
    bool UseFastClear() const { return m_flags.fastClear; }

    static uint32 GetInitialValue(const Image& image);

    static uint32 GetFastClearCode(const Image& image);

    // Returns the CB_COLOR*_CMASK_SLICE register value.
    const regCB_COLOR0_CMASK_SLICE& CbColorCmaskSlice() const { return m_cbColorCmaskSlice; }

private:
    static bool SupportFastColorClear(
        const Pal::Device& device,
        const Image&       image,
        AddrTileMode       tileMode,
        AddrTileType       tileType);

    Result ComputeCmaskInfo(
        const Pal::Device&     device,
        const Image&           image,
        const SubResourceInfo& subResInfo,
        MaskRamInfo*           pCmaskInfo) const;

    union Gfx6CmaskFlags
    {
        struct
        {
            uint32 linear    :  1;    // Indicates that CMask is linear
            uint32 fastClear :  1;    // Indicates fast color clears are supported
            uint32 reserved  : 30;    // Reserved for future use
        };
        uint32 value; // Value of the flags bitmask
    };

    // CMask value which represents fast-cleared for images that don't also have DCC memory.
    static constexpr uint32 FastClearValue = 0;

    // CMask value which represents fast-cleared for images that have DCC memory. Bits 3:2 should be 2'b11 to indicate
    // 'not fast cleared' and bits 1:0 being 2'b00 to mean all FMask pointers are zero for the entire tile.
    static constexpr uint32 FastClearValueDcc = 0xCCCCCCCC;

    // CMask value which represents fully expanded for single-sampled images.
    static constexpr uint32 FullyExpanded = 0xFFFFFFFF;

    Gfx6CmaskFlags            m_flags;              // CMask properties flags
    regCB_COLOR0_CMASK_SLICE  m_cbColorCmaskSlice;  // CB_COLOR*_CMASK_SLICE register value

    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx6Cmask);
};

// =====================================================================================================================
// Manages the FMask state for all slices of a single mipmap level of an Image resource.
class Gfx6Fmask final : public MaskRam
{
public:
    Gfx6Fmask();
    // Destructor has nothing to do.
    virtual ~Gfx6Fmask() {}

    static bool UseFmaskForImage(const Pal::Device& device, const Image& image);

    IMG_DATA_FORMAT FmaskFormat(uint32 samples, uint32 fragments, bool isUav) const;

    Result Init(
        const Pal::Device& device,
        const Image&       image,
        uint32             mipLevel,
        gpusize*           pGpuOffset);

    // Returns true if the FMask bufer supports MSAA compression.
    bool UseCompression() const { return m_flags.compression; }

    // Returns the tile index used for FMask.
    int32 TileIndex() const { return m_tileIndex; }

    // Returns the bank height for the FMask surface.
    uint32 BankHeight() const { return m_bankHeight; }

    // Returns the pitch in pixels for the FMask surface.
    uint32 Pitch() const { return m_pitch; }

    // Returns the bits of FMask data needed for each pixel.
    uint32 BitsPerPixel() const { return m_bitsPerPixel; }

    // Returns the CB_COLOR*_FMASK_SLICE register value.
    const regCB_COLOR0_FMASK_SLICE& CbColorFmaskSlice() const { return m_cbColorFmaskSlice; }

    // Initial value for an FMask allocation.
    static uint32 GetInitialValue(const Image& image);

    static uint32 GetPackedExpandedValue(const Image& image);

private:
    Result ComputeFmaskInfo(
        const Pal::Device&     device,
        const Image&           image,
        const SubResourceInfo& subResInfo,
        uint32                 numSamples,
        uint32                 numFragments,
        FmaskInfo*             pFmaskInfo) const;

    union Gfx6FmaskFlags
    {
        struct
        {
            uint32 compression :  1;    // Indicates fmask compression is supported
            uint32 reserved    : 31;    // Reserved for future use
        };
        uint32 value; // Value of the flags bitmask
    };

    Gfx6FmaskFlags            m_flags;              // CMask properties flags
    int32                     m_tileIndex;          // Tile index (-1 if unused)
    uint32                    m_bankHeight;         // Bank height
    uint32                    m_pitch;              // Pitch of FMask, in pixels
    uint32                    m_bitsPerPixel;       // Number of FMask bits per pixel
    regCB_COLOR0_FMASK_SLICE  m_cbColorFmaskSlice;  // CB_COLOR*_FMASK_SLICE register value

    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx6Fmask);
};

// Enum represents the purpose of clearing on DCC memory
enum class DccClearPurpose : uint32
{
    Init      = 0x0,  // This indicates a DCC initialization before it can be really used.
    FastClear = 0x1,  // This indicates a fast-clear based on DCC clear.
};

// UBM_DCC_DEFAULT_CLEAR_COLOR enum.
enum class Gfx8DccClearColor : uint32
{
    ClearColor0000  = 0x00,
    ClearColor0001  = 0x40,
    ClearColor1110  = 0x80,
    ClearColor1111  = 0xC0,
    ClearColorReg   = 0x20,
};

// Enum for VI CB_COLOR0_DCC_CONTROL.MAX_UN/COMPRESSED_BLOCK_SIZE
enum class Gfx8DccMaxBlockSize: uint32
{
    BlockSize64B  = 0,
    BlockSize128B = 1,
    BlockSize256B = 2
};

// Enum for VI CB_COLOR0_DCC_CONTROL.MIN_COMPRESSED_BLOCK_SIZE
enum class Gfx8DccMinBlockSize : uint32
{
    BlockSize32B = 0,
    BlockSize64B = 1
};

// Represents an "image" of the FCE state metadata used by all Images with DCC memory. Each image has one copy of this
// metadata for each of its mip levels.
struct MipFceStateMetaData
{
    uint64 fceRequired; // 64bit integer interpreted by the CP as a boolean (0 = false, !0 = true)
    uint64 padding;     // Padding for SET_PREDICATION alignment requirements
};

// Represents an "image" of the DCC state metadata used by all Images with DCC memory. Each image has one copy of this
// metatdata for each of its mip levels.
struct MipDccStateMetaData
{
    uint64 isCompressed; // 64bit integer interpreted by the CP as a boolean (0 = false, !0 = true)
    uint64 padding;      // Padding for SET_PREDICATION alignment requirements
};

// =====================================================================================================================
// Manages the DCC state for all slices of a single mipmap level of an Image resource.
class Gfx6Dcc final : public MaskRam
{
public:
    Gfx6Dcc();

    // Destructor has nothing to do.
    virtual ~Gfx6Dcc() {}

    Result Init(
        const Pal::Device& device,
        const Image&       image,
        uint32             mipLevel,
        gpusize*           pSizeAvail,
        gpusize*           pGpuOffset,
        bool*              pCanUseDcc);

    static Result InitTotal(
        const Pal::Device& device,
        const Image&       image,
        gpusize            totalMipSize,
        gpusize*           pGpuOffset,
        gpusize*           pTotalSize);

    // Returns the value of the DCC control register for this DCC surface
    const regCB_COLOR0_DCC_CONTROL__VI& GetControlReg() const { return m_dccControl; }

    // Returns the number of bytes of DCC memory that should be fast cleared
    gpusize GetFastClearSize() const { return m_fastClearSize; }

    static uint32 GetFastClearCode(
        const Image&            image,
        const Pal::SubresRange& clearRange,
        const uint32*           pConvertedColor,
        bool*                   pNeedFastClearElim);

    static bool UseDccForImage(
        const Pal::Device& device,
        const Image&       image,
        AddrTileMode       tileMode,
        AddrTileType       tileType,
        bool               metaDataTexFetchSupported);

    // Returns true if this DCC memory can actually be used or if it's just placeholder
    // memory that the HW requires we allocate anyway.
    bool IsCompressionEnabled() const { return m_flags.enableCompression; }

    // Returns true if the DCC buffer supports fast color clears.
    bool UseFastClear() const { return m_flags.enableFastClear; }

    // Returns true if Subres memory is contiguous.
    bool ContiguousSubresMem() const { return m_flags.contiguousSubresMem; }

    void SetEnableCompression(uint32 val);

    static constexpr uint8 DecompressedValue = 0xFF;
    uint8 GetInitialValue(const Image& image, SubresId subresId, ImageLayout layout) const;

private:
    static Result ComputeDccInfo(
        const Pal::Device& device,
        const Image&       image,
        const SubresId&    subResource,
        gpusize            colorSurfSize,
        DccInfo*           pDccInfo,
        bool*              pNextMipCanUseDcc);

    static uint32 GetMinCompressedBlockSize(const Image&  image);

    union Gfx6DccFlags
    {
        struct
        {
            uint32 enableCompression   :  1; // Indicates that this DCC surface is useful
            uint32 enableFastClear     :  1; // Does this DCC surface support fast clears?
            uint32 contiguousSubresMem :  1; // Indicates Subres memory is contiguous
            uint32 reserved            : 29; // Reserved for future use
        };
        uint32 value; // Value of the flags bitmask
    };

    Gfx6DccFlags              m_flags;          // DCC properties flags
    // Number of bytes of DCC memory that should be written for a fast-clear operation
    gpusize                   m_fastClearSize;
    regCB_COLOR0_DCC_CONTROL__VI  m_dccControl; // the DCC control reg for this DCC memory

    DccInitialClearKind       m_clearKind;

    void SetControlReg(const Image& image, const SubResourceInfo& subResInfo);

    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx6Dcc);
};

} // Gfx6
} // Pal
