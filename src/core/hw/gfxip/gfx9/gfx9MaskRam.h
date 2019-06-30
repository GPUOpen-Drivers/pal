/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9MetaEq.h"
#include "core/addrMgr/addrMgr2/addrMgr2.h"

namespace Pal
{

namespace Gfx9
{

// Forward decl's:
class Image;
class Device;

// Represents an "image" of the fast-clear metadata used by Depth/Stencil Images.
struct Gfx9FastDepthClearMetaData
{
    regDB_STENCIL_CLEAR  dbStencilClear;    // Stencil clear value
    regDB_DEPTH_CLEAR    dbDepthClear;      // Depth clear value
};

// Represents an "image" of the HiSPretests metadata used by Depth/Stencil Images.
struct Gfx9HiSPretestsMetaData
{
    regDB_SRESULTS_COMPARE_STATE0 dbSResultCompare0;
    regDB_SRESULTS_COMPARE_STATE1 dbSResultCompare1;
};

// A structure that defines the dimensions of a "block", either meta blocks or compressed blocks
struct Gfx9MaskRamBlockSize
{
    uint32  width;
    uint32  height;
    uint32  depth;
};

// A structure for defining the location and size of a meta-equation in GPU accessible memory
struct MetaEqGpuAccess
{
    gpusize  offset;
    gpusize  size;
};

// Some operations need an easy way to specify which HTile aspects they will read or write to.
enum HtileAspectMask : uint32
{
    HtileAspectDepth   = 0x1,
    HtileAspectStencil = 0x2
};

// GFX9 hw has three metadata types cmask is metadata for fmask, Dcc for color surface and
// Htile for depth surfaces.
enum MetaDataType : uint32
{
    MetaDataCmask,
    MetaDataDcc,
    MetaDataHtile,
    MetaDataNumTypes,
};

union HtileUsageFlags
{
    struct
    {
        uint32  dsMetadata       :  1; // hTile reflects data stored in the parent depth/stencil image.
        uint32  reservedFutureHw :  1;
        uint32  reserved         : 30;
    };
    uint32  value;
};

// =====================================================================================================================
// Anything that affects all GFX9 mask ram types goes here.  Most importantly, this class provides functions for
// calculating the meta data addressing equation -- i.e., how to turn an x,y,z coordinate into an offset into a
// mask-ram allocation.
//
class Gfx9MaskRam : public MaskRam
{
public:
    Gfx9MaskRam(
        const Image&  image,
        int32         metaDataSizeLog2,
        uint32        firstUploadBit);
    // Destructor has nothing to do.
    virtual ~Gfx9MaskRam() {}

    void BuildEqBufferView(
        BufferViewInfo*  pBufferView) const;
    void BuildSurfBufferView(
        BufferViewInfo*  pViewInfo) const;
    uint32 CalcPipeXorMask(
        ImageAspect   aspect) const;
    const MetaDataAddrEquation&  GetMetaEquation() const { return m_meta; }
    const MetaEquationParam& GetMetaEquationParam() const { return m_metaEqParam; }
    virtual uint32  GetPipeBankXor(ImageAspect   aspect) const;
    void CpuUploadEq(void*  pCpuMem) const;
    void UploadEq(CmdBuffer*  pCmdBuffer) const;
    bool HasEqGpuAccess() const { return m_eqGpuAccess.offset != 0; }
    static bool SupportFastColorClear(
        const Pal::Device& device,
        const Image&       image,
        AddrSwizzleMode    swizzleMode);

    const ADDR2_META_MIP_INFO&  GetAddrMipInfo(uint32  mipLevel) const { return m_addrMipOutput[mipLevel]; }
    uint32  GetFirstBit() const { return m_firstUploadBit; }

    // Returns the number of samples that actually matter when it comes to processing the meta equation associated
    // with this mask ram.  This can be different than the number of samples associated with the image this mask-ram
    // belongs to.  I don't understand that either.
    uint32  GetNumEffectiveSamples() const { return m_effectiveSamples; }

    virtual void GetXyzInc(
        uint32*  pXinc,
        uint32*  pYinc,
        uint32*  pZinc) const;

    static bool IsRbAligned(const Image*  pImage);
    static bool IsPipeAligned(const Image*  pImage);
    bool IsMetaEquationValid() const { return m_metaEquationValid; }

protected:
    void                    InitEqGpuAccess(gpusize*  pGpuSize);
    virtual void            CalcMetaEquation();
    virtual uint32          GetBytesPerPixelLog2() const;
    virtual uint32          GetNumSamplesLog2() const = 0;
    virtual AddrSwizzleMode GetSwizzleMode() const;
    bool                    IsThick() const;
    uint32                  AdjustPipeBankXorForSwizzle(uint32  pipeBankXor) const;
    void                    FinalizeMetaEquation(gpusize  addressableSizeBytes);

    // Of the three types of mask-ram surfaces (hTile, dcc and cMask), only DCC is really associated with
    // a color image.  hTile is associated with depth, and cMask is the meta surface for fMask, so, for
    // two-outta-three, the associated surface is not a color image.
    virtual bool  IsColor() const { return false; }

    const Image&          m_image;
    const Device*         m_pGfxDevice;

    // Equations used for calculating locations within this meta-surface
    MetaDataAddrEquation  m_meta;
    MetaDataAddrEquation  m_pipe;

    ADDR2_META_MIP_INFO   m_addrMipOutput[MaxImageMipLevels];
    const int32           m_metaDataWordSizeLog2;

private:
    void   CalcDataOffsetEquation();
    void   CalcPipeEquation(uint32  numPipesLog2);
    uint32 CapPipe() const;
    void   CalcRbEquation(uint32  numSesLog2, uint32  numRbsPerSeLog2);
    void   MergePipeAndRbEq();
    uint32 RemoveSmallRbBits();

    uint32 GetRbAppendedBit(uint32  bitPos) const;
    void   SetRbAppendedBit(uint32  bitPos, uint32  bitVal);

    virtual void   CalcCompBlkSizeLog2(Gfx9MaskRamBlockSize*  pBlockSize) const = 0;
    virtual void   CalcMetaBlkSizeLog2(Gfx9MaskRamBlockSize*  pBlockSize) const = 0;

    MetaDataAddrEquation  m_dataOffset;
    MetaDataAddrEquation  m_rb;
    MetaEquationParam     m_metaEqParam;

    const uint32          m_firstUploadBit;
    bool                  m_metaEquationValid;
    uint32                m_effectiveSamples;
    MetaEqGpuAccess       m_eqGpuAccess;
    uint32                m_rbAppendedWithPipeBits;

    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx9MaskRam);
};

// =====================================================================================================================
// Manages the HTile state for all slices of a single mipmap level of an Image resource.
class Gfx9Htile : public Gfx9MaskRam
{
public:
    Gfx9Htile(const Image&  image, HtileUsageFlags  htileUsage);
    // Destructor has nothing to do.
    virtual ~Gfx9Htile() {}

    static HtileUsageFlags UseHtileForImage(const Pal::Device& device, const Image& image);

    uint32 GetClearValue(float  depthValue) const;

    uint32 GetAspectMask(
        uint32   aspectFlags) const;

    uint32 ComputeResummarizeData() const;

    uint32 GetInitialValue() const;

    virtual uint32  GetPipeBankXor(ImageAspect   aspect) const override;

    Result Init(
        gpusize*  pGpuOffset,
        bool      hasEqGpuAccess);

    bool DepthCompressed() const { return m_flags.compressZ; }
    bool StencilCompressed() const { return m_flags.compressS; }
    bool TileStencilDisabled() const { return m_flags.tileStencilDisable; }
    bool ZRangePrecision() const { return m_flags.zrangePrecision; }
    const regDB_HTILE_SURFACE& DbHtileSurface(uint32  mipLevel) const { return m_dbHtileSurface[mipLevel]; }
    const regDB_PRELOAD_CONTROL& DbPreloadControl(uint32  mipLevel) const { return m_dbPreloadControl[mipLevel]; }
    const ADDR2_COMPUTE_HTILE_INFO_OUTPUT&  GetAddrOutput() const { return m_addrOutput; }
    HtileUsageFlags  GetHtileUsage() const { return m_hTileUsage; }

protected:
    virtual uint32  GetNumSamplesLog2() const override;

private:
    Result ComputeHtileInfo(const SubResourceInfo* pSubResInfo);

    void SetupHtilePreload(uint32 mipLevel);

    virtual void   CalcCompBlkSizeLog2(Gfx9MaskRamBlockSize*  pBlockSize) const override;
    virtual void   CalcMetaBlkSizeLog2(Gfx9MaskRamBlockSize*  pBlockSize) const override;

    union Gfx9HtileFlags
    {
        struct
        {
            uint32 zrangePrecision    :  1; // Should zMin (0.f) or zMax (1.f) get more precision?
            uint32 compressZ          :  1; // Is depth compression enabled?
            uint32 compressS          :  1; // Is stencil compression enabled?
            uint32 tileStencilDisable :  1; // Allows more Hi-Z precision for non-stencil formats
            uint32 reserved           : 28; // Reserved for future use
        };
        uint32 value; // Value of the flags bitmask
    };

    ADDR2_COMPUTE_HTILE_INFO_OUTPUT  m_addrOutput;
    Gfx9HtileFlags                   m_flags;                                // HTile properties flags
    HtileUsageFlags                  m_hTileUsage;
    regDB_HTILE_SURFACE              m_dbHtileSurface[MaxImageMipLevels];    // DB_HTILE_SURFACE register value
    regDB_PRELOAD_CONTROL            m_dbPreloadControl[MaxImageMipLevels];  // DB_PRELOAD_CONTROL register value

    // Each DB's HTile cache can fit 8K DWORDs. Each DWORD of HTILE data covers 64 pixels.
    static constexpr uint32 DbHtileCacheSizeInPixels = (8 * 1024 * 64);

    // Mask of HTile bits used for stencil.
    static constexpr uint32 Gfx9HtileStencilMask = 0x000003F0;
    // Mask of HTile bits used for depth.
    static constexpr uint32 Gfx9HtileDepthMask   = 0xFFFFFC0F;

    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx9Htile);
};

// Enum for CB_COLOR0_DCC_CONTROL.MAX_UN/COMPRESSED_BLOCK_SIZE
enum class Gfx9DccMaxBlockSize: uint32
{
    BlockSize64B  = 0,
    BlockSize128B = 1,
    BlockSize256B = 2
};

// Enum for CB_COLOR0_DCC_CONTROL.MIN_COMPRESSED_BLOCK_SIZE
enum class Gfx9DccMinBlockSize : uint32
{
    BlockSize32B = 0,
    BlockSize64B = 1
};

// Enum represents the purpose of clearing on DCC memory
enum class DccClearPurpose : uint32
{
    Init      = 0x0,  // This indicates a DCC initialization before it can be really used.
    FastClear = 0x1,  // This indicates a fast-clear based on DCC clear.
};

// These values correspond to the various fast-clear codes for DCC memory
enum class Gfx9DccClearColor : uint8
{
    ClearColor0000         = 0x00,
    ClearColor0001         = 0x40,
    ClearColor1110         = 0x80,
    ClearColor1111         = 0xC0,
    ClearColorReg          = 0x20,
};

// Represents an "image" of the fast-clear metadata used by Color Target Images.
struct Gfx9FastColorClearMetaData
{
    regCB_COLOR0_CLEAR_WORD0  cbColorClearWord0;    // Packed fast-clear color bits [31:0]
    regCB_COLOR0_CLEAR_WORD1  cbColorClearWord1;    // Packet fast-clear color bits [63:32]
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
// Manages the DCC state for all slices / miplevels of an Image resource.
class Gfx9Dcc : public Gfx9MaskRam
{
public:
    Gfx9Dcc(const Image&  image);
    // Destructor has nothing to do.
    virtual ~Gfx9Dcc() {}

    Result Init(gpusize*  pSize, bool  hasEqGpuAccess);
    static bool UseDccForImage(const Image& image, bool metaDataTexFetchSupported);

    // Returns the value of the DCC control register for this DCC surface
    const regCB_COLOR0_DCC_CONTROL& GetControlReg() const { return m_dccControl; }

    const ADDR2_COMPUTE_DCCINFO_OUTPUT&  GetAddrOutput() const { return m_addrOutput; }

    static uint8 GetFastClearCode(
        const Image&            image,
        const Pal::SubresRange& clearRange,
        const uint32*           pConvertedColor,
        bool*                   pNeedFastClearElim);

    // Initial value for a DCC allocation.
    static constexpr uint8 InitialValue = 0xFF;

    uint32  GetNumEffectiveSamples(DccClearPurpose  clearPurpose) const;

    virtual void GetXyzInc(
        uint32*  pXinc,
        uint32*  pYinc,
        uint32*  pZinc) const override;

protected:
    virtual uint32  GetNumSamplesLog2() const override;

private:
    ADDR2_COMPUTE_DCCINFO_OUTPUT  m_addrOutput;
    regCB_COLOR0_DCC_CONTROL      m_dccControl;

    Result ComputeDccInfo();
    void   SetControlReg();
    uint32 GetMinCompressedBlockSize() const;

    // The surface associated with a DCC surface is guaranteed to be a color image
    virtual bool  IsColor() const override { return true; }

    virtual void   CalcCompBlkSizeLog2(Gfx9MaskRamBlockSize*  pBlockSize) const override;
    virtual void   CalcMetaBlkSizeLog2(Gfx9MaskRamBlockSize*  pBlockSize) const override;

    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx9Dcc);
};

// =====================================================================================================================
// Manages the cMask state for all slices and miplevels of an Image resource.
class Gfx9Cmask : public Gfx9MaskRam
{
public:
    Gfx9Cmask(const Image&  image);
    // Destructor has nothing to do.
    virtual ~Gfx9Cmask() {}

    Result Init(
        gpusize*  pGpuOffset,
        bool      hasEqGpuAccess);
    static bool UseCmaskForImage(
        const Pal::Device& device,
        const Image&       image);
    const ADDR2_COMPUTE_CMASK_INFO_OUTPUT&  GetAddrOutput() const { return m_addrOutput; }
    virtual uint32  GetPipeBankXor(ImageAspect   aspect) const override;

    // Initial value for a cMask allocation (MSAA associated cMask only)
    static constexpr uint8 InitialValue = 0xCC;

protected:
    virtual uint32  GetBytesPerPixelLog2() const override;

    // FMASK always treated as 1xAA for Cmask addressing
    virtual uint32  GetNumSamplesLog2() const override { return 0; }

    // Returns the swizzle mode of the associated fmask surface
    AddrSwizzleMode GetSwizzleMode() const override;

private:
    ADDR2_COMPUTE_CMASK_INFO_OUTPUT  m_addrOutput;

    void   CalcCompBlkSizeLog2(Gfx9MaskRamBlockSize*  pBlockSize) const override;
    void   CalcMetaBlkSizeLog2(Gfx9MaskRamBlockSize*  pBlockSize) const override;

    Result ComputeCmaskInfo();

    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx9Cmask);
};

// =====================================================================================================================
// Manages the fMask state for all slices and miplevels of an Image resource.  Note that fMask is not considered
// a meta-equation capable surface, so it derives from MaskRam, not Gfx9MaskRam.
class Gfx9Fmask : public MaskRam
{
public:
    Gfx9Fmask(const Image&  image);
    // Destructor has nothing to do.
    virtual ~Gfx9Fmask() {}

    Result Init(gpusize*  pGpuOffset);
    regSQ_IMG_RSRC_WORD1 Gfx9FmaskFormat(uint32  samples, uint32  fragments, bool isUav) const;
    const ADDR2_COMPUTE_FMASK_INFO_OUTPUT&  GetAddrOutput() const { return m_addrOutput; }
    AddrSwizzleMode GetSwizzleMode() const { return m_surfSettings.swizzleMode; }
    uint32 GetPipeBankXor() const { return m_pipeBankXor; }

    // Initial value for a fMask allocation
    static constexpr uint32 InitialValue = 0;

    static uint32 GetPackedExpandedValue(const Image& image);

private:
    ADDR2_COMPUTE_FMASK_INFO_OUTPUT          m_addrOutput;
    ADDR2_GET_PREFERRED_SURF_SETTING_OUTPUT  m_surfSettings;
    uint32                                   m_pipeBankXor;
    const Image&                             m_image;

    Result ComputeFmaskInfo();

    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx9Fmask);
};

} // Gfx9
} // Pal
