/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
class Gfx9MetaEqGenerator;

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

// Some operations need an easy way to specify which HTile planes they will read or write to.
enum HtilePlaneMask : uint32
{
    HtilePlaneDepth   = 0x1,
    HtilePlaneStencil = 0x2
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
        uint32  vrs              :  1; // hTile contains VRS shading-rate data
        uint32  reserved         : 30;
    };
    uint32  value;
};

// A collection of parameters needed to calculate the pipe equation for gfxip with rbPlus
struct Data2dParamsNew
{
    bool    skipY3;
    uint32  yBias;
    int32   flipPipeFill;
    uint32  pipeRotateAmount;
    uint32  pipeRotateBit0;
    uint32  pipeRotateBit1;
    uint32  restart;
    uint32  pipeAnchorWidthLog2;
    uint32  pipeAnchorHeightLog2;
    uint32  upperSampleBits;
    uint32  tileSplitBits;
};

// A collection of parameters needed to calculate the pipe equation for no rbPlus gfxip
struct Data2dParams
{
    bool    flipPipeXY;
    bool    flipX3Y3;
    bool    flipX4Y4;
    bool    flipYBias;
    bool    flipY1Y2;
    uint32  flipPipeFill;
    uint32  xRestart;
    uint32  yRestart;
    uint32  pipeAnchorWidthLog2;
    uint32  pipeAnchorHeightLog2;
    int32   upperSampleBits;
    int32   tileSplitBits;
};

enum PipeDist : uint32
{
    PipeDist8x8,
    PipeDist16x16,
};

// =====================================================================================================================
// Anything that affects all GFX9 mask ram types goes here.
class Gfx9MaskRam : public MaskRam
{
public:
    Gfx9MaskRam(
        const Image&  image,
        void*         pPlacementAddr,
        int32         metaDataSizeLog2,
        uint32        firstUploadBit);
    // Destructor has nothing to do.
    virtual ~Gfx9MaskRam() {}

    void BuildSurfBufferView(
        BufferViewInfo*  pViewInfo) const;
    virtual uint32 PipeAligned() const { return 1; }
    virtual gpusize  SliceOffset(uint32  arraySlice) const;

    virtual AddrSwizzleMode GetSwizzleMode() const;
    ADDR2_META_FLAGS GetMetaFlags() const;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    virtual uint32 GetPipeBankXor(ImageAspect aspect) const;
#else
    virtual uint32 GetPipeBankXor(uint32 plane) const;
#endif
    virtual uint32 GetBytesPerPixelLog2() const;
    virtual uint32 GetMetaBlockSize(Gfx9MaskRamBlockSize* pExtent) const;
    virtual uint32 GetNumSamplesLog2() const = 0;
    virtual uint32 GetMetaCachelineSize() const = 0;
    virtual void   CalcCompBlkSizeLog2(Gfx9MaskRamBlockSize*  pBlockSize) const = 0;
    virtual void   CalcMetaBlkSizeLog2(Gfx9MaskRamBlockSize*  pBlockSize) const = 0;

    // Of the three types of mask-ram surfaces (hTile, dcc and cMask), only DCC is really associated with
    // a color image.  hTile is associated with depth, and cMask is the meta surface for fMask, so, for
    // two-outta-three, the associated surface is not a color image.
    virtual bool  IsColor() const { return false; }

    // Only hTile is associated with depth, So if one returns false with IsColor()||IsDepth(),
    // then it should be CMask.
    virtual bool  IsDepth() const { return false; }

    virtual void GetXyzInc(
        uint32*  pXinc,
        uint32*  pYinc,
        uint32*  pZinc) const;

    bool                       HasMetaEqGenerator() const { return m_pEqGenerator != nullptr; }
    const Image&               GetImage()           const { return m_image; }
    const Device*              GetGfxDevice()       const { return m_pGfxDevice; }
    const Gfx9MetaEqGenerator* GetMetaEqGenerator() const { return m_pEqGenerator; }

protected:
    uint32 AdjustPipeBankXorForSwizzle(uint32 pipeBankXor) const;

    Gfx9MetaEqGenerator* m_pEqGenerator;

    const Image&  m_image;
    const Device* m_pGfxDevice;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx9MaskRam);
};

// =====================================================================================================================
// This class provides functions for calculating the meta data addressing equation -- i.e., how to turn an x,y,z
// coordinate into an offset into a mask-ram allocation.
//
class Gfx9MetaEqGenerator
{
public:
    Gfx9MetaEqGenerator(
        const Gfx9MaskRam* pParent,
        int32              metaDataSizeLog2,
        uint32             firstUploadBit);
    // Destructor has nothing to do.
    ~Gfx9MetaEqGenerator() {}

    void BuildEqBufferView(
        BufferViewInfo*  pBufferView) const;
    uint32 CalcPipeXorMask(
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
        ImageAspect   aspect) const;
#else
        uint32 plane) const;
#endif
    const MetaDataAddrEquation&  GetMetaEquation() const { return m_meta; }
    const MetaEquationParam& GetMetaEquationParam() const { return m_metaEqParam; }
    void CpuUploadEq(void*  pCpuMem) const;
    void UploadEq(CmdBuffer*  pCmdBuffer) const;
    bool HasEqGpuAccess() const { return m_eqGpuAccess.offset != 0; }

    uint32 GetFirstBit() const { return m_firstUploadBit; }
    uint32 CapPipe() const;

    // Returns the number of samples that actually matter when it comes to processing the meta equation associated
    // with this mask ram.  This can be different than the number of samples associated with the image this mask-ram
    // belongs to.  I don't understand that either.
    uint32  GetNumEffectiveSamples() const { return m_effectiveSamples; }

    bool IsMetaEquationValid() const { return m_metaEquationValid; }

    void   CalcMetaEquation();
    void   InitEqGpuAccess(gpusize*  pGpuSize);
    uint32 GetEffectiveNumPipes() const;
    int32  GetMetaOverlap() const;
    uint32 GetPipeRotateAmount() const;
    uint32 GetNumShaderArrayLog2() const;

    PipeDist GetPipeDist()             const { return m_pipeDist; }
    int32    GetMetaDataWordSizeLog2() const { return m_metaDataWordSizeLog2; }

protected:
    bool   IsThick() const;
    void   FinalizeMetaEquation(gpusize  addressableSizeBytes);

    void   AddMetaPipeBits(MetaDataAddrEquation* pPipe, int32 offset);
    void   AddRbBits(MetaDataAddrEquation* pPipe, int32 offset);
    void   GetData2DParams(Data2dParams* pParams) const;
    void   GetData2DParamsNew(Data2dParamsNew* pParams) const;
    void   GetMetaPipeAnchorSize(Extent2d* pAnchorSize) const;
    void   GetMicroBlockSize(Gfx9MaskRamBlockSize* pMicroBlockSize) const;
    void   GetPipeAnchorSize(Extent2d* pAnchorSize) const;
    void   GetPixelBlockSize(Gfx9MaskRamBlockSize* pBlockSize) const;
    uint32 GetPipeBlockSize() const;

    const PipeDist        m_pipeDist;
    // Equations used for calculating locations within this meta-surface
    MetaDataAddrEquation  m_meta;

    const int32           m_metaDataWordSizeLog2;

private:
    void   CalcMetaEquationGfx9();
    void   CalcMetaEquationGfx10();
    void   CalcDataOffsetEquation(MetaDataAddrEquation* pDataOffset);
    void   CalcPipeEquation(MetaDataAddrEquation* pPipe, MetaDataAddrEquation* pDataOffset, uint32  numPipesLog2);
    void   CalcRbEquation(MetaDataAddrEquation* pRb, uint32  numSesLog2, uint32  numRbsPerSeLog2);
    void   MergePipeAndRbEq(MetaDataAddrEquation* pRb, MetaDataAddrEquation* pPipe);
    uint32 RemoveSmallRbBits(MetaDataAddrEquation* pRb);

    const Gfx9MaskRam*    m_pParent;

    uint32 GetRbAppendedBit(uint32  bitPos) const;
    void   SetRbAppendedBit(uint32  bitPos, uint32  bitVal);
    MetaEquationParam     m_metaEqParam;

    const uint32          m_firstUploadBit;
    bool                  m_metaEquationValid;
    uint32                m_effectiveSamples;
    MetaEqGpuAccess       m_eqGpuAccess;
    uint32                m_rbAppendedWithPipeBits;

    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx9MetaEqGenerator);
};

// =====================================================================================================================
// Manages the HTile state for all slices of a single mipmap level of an Image resource.
class Gfx9Htile final : public Gfx9MaskRam
{
public:
    Gfx9Htile(const Image&  image, void*  pPlacementAddr, HtileUsageFlags  htileUsage);
    // Destructor has nothing to do.
    virtual ~Gfx9Htile() {}

    static HtileUsageFlags UseHtileForImage(const Pal::Device& device, const Image& image);

    uint32 GetInitialValue() const;
    uint32 GetClearValue(float depthValue) const;
    uint32 GetPlaneMask(uint32 planeFlags) const;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    uint32 GetPlaneMask(ImageAspect aspect) const;
#else
    uint32 GetPlaneMask(const SubresRange& range) const;
#endif

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

    const ADDR2_META_MIP_INFO& GetAddrMipInfo(uint32 mipLevel) const { return m_addrMipOutput[mipLevel]; }

    static constexpr uint32 Sr1Mask = (3u << 6);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    uint32 GetPipeBankXor(ImageAspect aspect) const override;
#else
    uint32 GetPipeBankXor(uint32 plane) const override;
#endif
    uint32 GetMetaBlockSize(Gfx9MaskRamBlockSize* pExtent) const override;
    uint32 GetNumSamplesLog2() const override;
    uint32 GetMetaCachelineSize() const override { return 8; }
    void   CalcCompBlkSizeLog2(Gfx9MaskRamBlockSize*  pBlockSize) const override;
    void   CalcMetaBlkSizeLog2(Gfx9MaskRamBlockSize*  pBlockSize) const override;

private:
    Result ComputeHtileInfo(const SubResourceInfo* pSubResInfo);

    void SetupHtilePreload(uint32 mipLevel);

    virtual bool   IsDepth() const override { return true; }

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

    ADDR2_META_MIP_INFO              m_addrMipOutput[MaxImageMipLevels];
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
    ClearColorCompToReg         = 0x20,
    // Used for GFX10 GPUs during fast clears where the actual clear color is written
    // into the first pixel of each DCC block in the image data itself.
    Gfx10ClearColorCompToSingle = 0x10,
    ClearColorInvalid           = 0xFF,
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
class Gfx9Dcc final : public Gfx9MaskRam
{
public:
    Gfx9Dcc(const Image&  image, void*  pPlacementAddr, bool  displayDcc);
    // Destructor has nothing to do.
    virtual ~Gfx9Dcc() {}

    Result Init(const SubresId& subResId, gpusize*  pSize, bool  hasEqGpuAccess);
    static bool UseDccForImage(const Image& image, bool metaDataTexFetchSupported);

    static bool SupportFastColorClear(
        const Pal::Device& device,
        const Image&       image,
        AddrSwizzleMode    swizzleMode);

    // Returns the value of the DCC control register for this DCC surface
    const regCB_COLOR0_DCC_CONTROL& GetControlReg() const { return m_dccControl; }

    const ADDR2_COMPUTE_DCCINFO_OUTPUT&  GetAddrOutput() const { return m_addrOutput; }

    static uint8 GetFastClearCode(
        const Image&       image,
        const SubresRange& clearRange,
        const uint32*      pConvertedColor,
        bool*              pNeedFastClearElim,
        bool*              pBlackOrWhite = nullptr);

    const ADDR2_META_MIP_INFO& GetAddrMipInfo(uint32 mipLevel) const { return m_addrMipOutput[mipLevel]; }

    // Initial value for a DCC allocation.
    static constexpr uint8 InitialValue = 0xFF;

    uint32  GetNumEffectiveSamples(DccClearPurpose  clearPurpose) const;

    virtual void GetXyzInc(
        uint32*  pXinc,
        uint32*  pYinc,
        uint32*  pZinc) const override;

    virtual uint32 PipeAligned() const override;

    virtual gpusize  SliceOffset(uint32  arraySlice) const override;

    uint32 GetMetaBlockSize(Gfx9MaskRamBlockSize* pExtent) const override;
    uint32 GetNumSamplesLog2() const override;
    uint32 GetMetaCachelineSize() const override { return 6; }
    void   CalcCompBlkSizeLog2(Gfx9MaskRamBlockSize*  pBlockSize) const override;
    void   CalcMetaBlkSizeLog2(Gfx9MaskRamBlockSize*  pBlockSize) const override;

    void GetState(DccState* pState) const;

private:

    ADDR2_META_MIP_INFO           m_addrMipOutput[MaxImageMipLevels];
    ADDR2_COMPUTE_DCCINFO_OUTPUT  m_addrOutput;
    regCB_COLOR0_DCC_CONTROL      m_dccControl;

    const bool m_displayDcc;

    Result ComputeDccInfo(const SubresId&  subResId);
    void   SetControlReg(const SubresId&  subResId);
    uint32 GetMinCompressedBlockSize() const;

    static void GetBlackOrWhiteClearCode(
        const Pal::Image*  pImage,
        const uint32       color[],
        const uint32       ones[],
        uint8*             pClearCode);

    // The surface associated with a DCC surface is guaranteed to be a color image
    virtual bool  IsColor() const override { return true; }

    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx9Dcc);
};

// =====================================================================================================================
// Manages the cMask state for all slices and miplevels of an Image resource.
class Gfx9Cmask final : public Gfx9MaskRam
{
public:
    Gfx9Cmask(const Image&  image, void*  pPlacementAddr);
    // Destructor has nothing to do.
    virtual ~Gfx9Cmask() {}

    Result Init(
        gpusize*  pGpuOffset,
        bool      hasEqGpuAccess);
    static bool UseCmaskForImage(
        const Pal::Device& device,
        const Image&       image);
    const ADDR2_COMPUTE_CMASK_INFO_OUTPUT&  GetAddrOutput() const { return m_addrOutput; }

    // Initial value for a cMask allocation (MSAA associated cMask only)
    uint8 GetInitialValue() const;

    // CMask value which represents fast-cleared for images that have DCC memory. Bits 3:2 should be 2'b11 to indicate
    // 'not fast cleared' and bits 1:0 being 2'b00 to mean all FMask pointers are zero for the entire tile.
    static constexpr uint8 FastClearValueDcc = 0xCC;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    uint32 GetPipeBankXor(ImageAspect aspect) const override;
#else
    uint32 GetPipeBankXor(uint32 plane) const override;
#endif
    uint32 GetBytesPerPixelLog2() const override;
    // FMASK always treated as 1xAA for Cmask addressing
    uint32 GetNumSamplesLog2() const override { return 0; }
    uint32 GetMetaCachelineSize() const override { return 8; }
    void   CalcCompBlkSizeLog2(Gfx9MaskRamBlockSize*  pBlockSize) const override;
    void   CalcMetaBlkSizeLog2(Gfx9MaskRamBlockSize*  pBlockSize) const override;

protected:
    // Returns the swizzle mode of the associated fmask surface
    AddrSwizzleMode GetSwizzleMode() const override;
private:

    ADDR2_COMPUTE_CMASK_INFO_OUTPUT  m_addrOutput;

    Result ComputeCmaskInfo();

    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx9Cmask);
};

// =====================================================================================================================
// Manages the fMask state for all slices and miplevels of an Image resource.  Note that fMask is not considered
// a meta-equation capable surface, so it derives from MaskRam, not Gfx9MaskRam.
class Gfx9Fmask final : public MaskRam
{
public:
    Gfx9Fmask();
    // Destructor has nothing to do.
    virtual ~Gfx9Fmask() {}

    Result Init(const Image& image, gpusize*  pGpuOffset);
    regSQ_IMG_RSRC_WORD1 Gfx9FmaskFormat(uint32  samples, uint32  fragments, bool isUav) const;
    const ADDR2_COMPUTE_FMASK_INFO_OUTPUT&  GetAddrOutput() const { return m_addrOutput; }
    AddrSwizzleMode GetSwizzleMode() const { return m_surfSettings.swizzleMode; }
    uint32 GetPipeBankXor() const { return m_pipeBankXor; }

    // Initial value for a fMask allocation
    static constexpr uint32 InitialValue = 0;

    static uint32 GetPackedExpandedValue(const Image& image);

    IMG_FMT Gfx10FmaskFormat(uint32  samples, uint32  fragments, bool isUav) const;

private:
    ADDR2_COMPUTE_FMASK_INFO_OUTPUT          m_addrOutput;
    ADDR2_GET_PREFERRED_SURF_SETTING_OUTPUT  m_surfSettings;
    uint32                                   m_pipeBankXor;

    Result ComputeFmaskInfo(const Image& image);

    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx9Fmask);
};

} // Gfx9
} // Pal
