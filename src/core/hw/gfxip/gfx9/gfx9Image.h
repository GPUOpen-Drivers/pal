/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/image.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9MaskRam.h"
#include "core/hw/gfxip/gfxImage.h"
#include "core/addrMgr/addrMgr2/addrMgr2.h"
#include "palCmdBuffer.h"

namespace Pal
{

class  CmdStream;
class  Device;
class  GfxCmdBuffer;

namespace Gfx9
{
// metadata addressing pattern can be thought of as divided into two schemes:-
// A.Metablock[Hi], Sample[Hi], CombinedOffset[Hi], Metablock[Lo], CombinedOffset[Lo] :- When addressing is both Pipe
// and RB Aligned.
// B.Metablock[all], CombinedOffset[Hi], Sample[Hi], CombinedOffset[Lo] :- When addressing is only RB Aligned.
// In our implementation it is mostly A since we request meta data to be both Pipe and RB aligned.
// These constants are passed to compute shader which does meta data clear.
struct MetaDataClearConst
{
    uint32 metablockSizeLog2;          // Sum of CombinedOffset[Hi] and CombinedOffset[L0]
    uint32 metablockSizeLog2BitMask;   // Combined offset full bit mask
    uint32 combinedOffsetLowBits;      // Combined offset low bits
    uint32 combinedOffsetLowBitsMask;  // Combined offset low bit mask
    uint32 metaBlockLsb;               // metablock index lsbs
    uint32 metaBlockLsbBitMask;        // metablock index lsb bit mask
    uint32 metaBlockHighBitShift;      // Shift of Metablock index MSBs
    uint32 combinedOffsetHighBitShift; // Shift of Combined offset MSBs
    bool   metaInterleaved;            // If 2 metablocks are interleaved/mixed in memory
};

// Specifies the compressions state of a color image.
enum ColorCompressionState : uint32
{
    ColorDecompressed      = 0,  // Fully expanded, cmask/fmask/dcc all indicate that color data should be read directly
                                 // from the base image.
    ColorFmaskDecompressed = 1,  // Valid for MSAA images only.  Color data remains compressed, but FMask is fully
                                 // decompressed so that it can be read by the texture unit.
    ColorCompressed        = 2,  // Fully compressed.  May or may not be readable by the texture unit, depending on the
                                 // GFXIP level.
    ColorCompressionStateCount
};

// Specifies the compressions state of a depth/stencil image.
enum DepthStencilCompressionState : uint32
{
    DepthStencilDecomprNoHiZ     = 0, // Fully expanded z-planes.  HiZ/HiS incompatible.  Compatible with uncompressed
                                      // depth rendering, shader reads and shader writes on all queues.
    DepthStencilDecomprWithHiZ   = 1, // Fully expanded z-planes.  HiZ/HiS compatible.  Compatible with semi-compressed
                                      // depth rendering (DB uses HiZ; keeps planes expanded), shader reads on univeral
                                      // queues.  Not compatible with shader writes (would change depth without
                                      // updating HiZ).
    DepthStencilCompressed       = 2, // Compressed depth.  HiZ/HiS compatible.  Compatible with compressed depth
                                      // rendering, potentially shader reads on universal queue if gfxip supports
                                      // TC-compatible htile.  Not compatible with shader writes.
    DepthStencilCompressionStateCount
};

enum FastClearTcCompatSurfs : uint32
{
    FastClearAllTcCompatColorSurfsNever = 0x00000000,
    FastClearAllTcCompatColorSurfsNoAa = 0x00000001,
    FastClearAllTcCompatColorSurfsMsaa = 0x00000002,

};

// Information used to determine the color compression state for an Image layout.
struct ColorLayoutToState
{
    ImageLayout compressedWrite;   // Mask of layouts compatible with the ColorCompressed state which also will
                                   // write compressed data (e.g., making the Image data compressed even if it
                                   // wasn't previously compressed).
    ImageLayout compressed;        // Mask of layouts compatible with the ColorCompressed state.
    ImageLayout fmaskDecompressed; // Mask of layouts compatible with the ColorFmaskDecompressed state.
};

// =====================================================================================================================
// Returns the best color hardware compression state based on a set of allowed usages and queues. Images with metadata
// are always compressed if they are only used on the universal queue and only support the color target usage.
// Otherwise, depending on the GFXIP support, additional usages may be available that avoid a full decompress.
PAL_INLINE ColorCompressionState ImageLayoutToColorCompressionState(
    const ColorLayoutToState& layoutToState,
    ImageLayout               imageLayout)
{
    // A color target view might also be created on a depth stencil image to perform a depth-stencil copy operation.
    // So this function might also be accessed upon a depth-stencil image.
    ColorCompressionState state = ColorDecompressed;

    if ((Util::TestAnyFlagSet(imageLayout.usages, ~layoutToState.compressed.usages) == false) &&
        (Util::TestAnyFlagSet(imageLayout.engines, ~layoutToState.compressed.engines) == false))
    {
        state = ColorCompressed;
    }
    else if ((Util::TestAnyFlagSet(imageLayout.usages, ~layoutToState.fmaskDecompressed.usages) == false) &&
             (Util::TestAnyFlagSet(imageLayout.engines, ~layoutToState.fmaskDecompressed.engines) == false))
    {
        state = ColorFmaskDecompressed;
    }

    return state;
}

// =====================================================================================================================
// Determines if a particular set of allowed usages and queues is one which is not only compatible with the
// ColorCompressed state, but also can write compressed data from the GPU back to the image.
PAL_INLINE bool ImageLayoutCanCompressColorData(
    const ColorLayoutToState& layoutToState,
    ImageLayout               imageLayout)
{
    return ((Util::TestAnyFlagSet(imageLayout.usages, ~layoutToState.compressed.usages)   == false) &&
            (Util::TestAnyFlagSet(imageLayout.engines, ~layoutToState.compressed.engines) == false) &&
            Util::TestAnyFlagSet(imageLayout.usages, layoutToState.compressedWrite.usages)          &&
            Util::TestAnyFlagSet(imageLayout.engines, layoutToState.compressedWrite.engines));
}

// Information used to determine the depth or stencil compression state for an Image layout.
struct DepthStencilLayoutToState
{
    ImageLayout compressed;      // Mask of layouts compatible with DepthStencilCompressed state.
    ImageLayout decomprWithHiZ;  // Mask of layouts compatible with DepthStencilDecomprWithHtile state.
};

// =====================================================================================================================
// Returns the best hardware depth/stencil compression state for the specified subresource based on a set of allowed
// usages and queues. Images with htile are always compressed if they are only used on the universal queue and only
// support the depth/stencil target usage.  Otherwise, depending on the GFXIP support, additional usages may be
// available whithout decompressing.
PAL_INLINE DepthStencilCompressionState ImageLayoutToDepthCompressionState(
    const DepthStencilLayoutToState& layoutToState,
    ImageLayout                      imageLayout)
{
    // Start with most aggressive decompression
    DepthStencilCompressionState state = DepthStencilDecomprNoHiZ;

    if (imageLayout.engines != 0)
    {
        // If there is an htile, test if the given layout supports full compression.  Otherwise, try partial
        // decompression that still uses HiZ.
        if ((Util::TestAnyFlagSet(imageLayout.usages, ~layoutToState.compressed.usages) == false) &&
            (Util::TestAnyFlagSet(imageLayout.engines, ~layoutToState.compressed.engines) == false))
        {
            state = DepthStencilCompressed;
        }
        else if ((Util::TestAnyFlagSet(imageLayout.usages, ~layoutToState.decomprWithHiZ.usages) == false) &&
                 (Util::TestAnyFlagSet(imageLayout.engines, ~layoutToState.decomprWithHiZ.engines) == false))
        {
            state = DepthStencilDecomprWithHiZ;
        }
    }

    return state;
}

// =====================================================================================================================
// This is the Gfx9 Image class which is derived from GfxImage.  It is responsible for hardware specific Image
// functionality such as setting up mask ram, metadata, tile info, etc.
class Image : public GfxImage
{
public:
    static constexpr uint32 FastClearAllTcCompatColorSurfs = (FastClearAllTcCompatColorSurfsNoAa |
                                                              FastClearAllTcCompatColorSurfsMsaa);

    Image(Pal::Image* pParentImage, ImageInfo* pImageInfo, const Pal::Device& device);
    virtual ~Image();

    Result ComputePipeBankXor(
        ImageAspect                                     aspect,
        const ADDR2_GET_PREFERRED_SURF_SETTING_OUTPUT*  pSurfSetting,
        uint32*                                         pPipeBankXor) const;

    const ADDR2_COMPUTE_SURFACE_INFO_OUTPUT* GetAddrOutput(const SubResourceInfo* pSubResInfo) const;
    const ADDR2_GET_PREFERRED_SURF_SETTING_OUTPUT& GetAddrSettings(const SubResourceInfo* pSubResInfo) const
        { return m_addrSurfSetting[GetAspectIndex(pSubResInfo->subresId.aspect)]; }

    uint32 GetSubresource256BAddrSwizzled(SubresId subresource) const;
    uint32 GetSubresource256BAddrSwizzledHi(SubresId subresource) const;

    virtual bool IsFastColorClearSupported(GfxCmdBuffer*      pCmdBuffer,
                                           ImageLayout        colorLayout,
                                           const uint32*      pColor,
                                           const SubresRange& range) override;

    virtual bool IsFastDepthStencilClearSupported(ImageLayout        depthLayout,
                                                  ImageLayout        stencilLayout,
                                                  float              depth,
                                                  uint8              stencil,
                                                  uint8              stencilWriteMask,
                                                  const SubresRange& range) const override;

    virtual bool IsFormatReplaceable(const SubresId& subresId,
                                     ImageLayout     layout,
                                     bool            isDst,
                                     uint8           disabledChannelMask = 0) const override;

    virtual bool IsSubResourceLinear(const SubresId& subresource) const override;

    virtual bool IsIterate256Meaningful(const SubResourceInfo* subResInfo) const override;

    virtual bool ShaderWriteIncompatibleWithLayout(const SubresId& subresId, ImageLayout layout) const override;

    // Returns true if this Image has associated HTile data.
    virtual bool HasHtileData() const override { return (m_pHtile == nullptr) ? false : true; }

    virtual bool HasDsMetadata() const { return (GetHtileUsage().dsMetadata != 0); }

    virtual bool HasDisplayDccData() const override { return (m_pDispDcc[0] != nullptr); }

    // Returns a pointer to the hTile object associated with this image
    const Gfx9Htile* GetHtile() const
        { return (HasHtileData() ? m_pHtile : nullptr); }

    // Returns metaData constants needed for optimized clear
    const MetaDataClearConst& GetMetaDataClearConst(MetaDataType metaDataType) const
        { return m_metaDataClearConst[metaDataType]; }

    uint32 GetHtile256BAddr() const;

    uint32* UpdateHiSPretestsMetaData(
        const SubresRange& range,
        const HiSPretests& pretests,
        Pm4Predicate       predicate,
        uint32*            pCmdSpace) const;
    uint32* UpdateDepthClearMetaData(
        const SubresRange&  range,
        uint32              writeMask,
        float               depthValue,
        uint8               stencilValue,
        Pm4Predicate        predicate,
        uint32*             pCmdSpace) const;
    uint32* UpdateColorClearMetaData(
        const SubresRange& clearRange,
        const uint32       packedColor[4],
        Pm4Predicate       predicate,
        uint32*            pCmdSpace) const;
    void UpdateDccStateMetaData(
        Pal::CmdStream*     pCmdStream,
        const SubresRange&  range,
        bool                isCompressed,
        EngineType          engineType,
        Pm4Predicate        predicate) const;
    uint32* UpdateFastClearEliminateMetaData(
        const GfxCmdBuffer*  pCmdBuffer,
        const SubresRange&   range,
        uint32               value,
        Pm4Predicate         predicate,
        uint32*              pCmdSpace) const;
    uint32* UpdateWaTcCompatZRangeMetaData(
        const SubresRange&   range,
        float                depthValue,
        Pm4Predicate         predicate,
        uint32*              pCmdSpace) const;

    gpusize GetFastClearEliminateMetaDataAddr(const SubresId&  subResId) const;
    gpusize GetFastClearEliminateMetaDataOffset(const SubresId&  subResId) const;

    bool HasWaTcCompatZRangeMetaData() const { return m_waTcCompatZRangeMetaDataOffset != 0; }
    gpusize GetWaTcCompatZRangeMetaDataAddr(uint32 mipLevel) const;
    gpusize WaTcCompatZRangeMetaDataOffset(uint32 mipLevel) const;
    gpusize WaTcCompatZRangeMetaDataSize(uint32 numMips) const;

    uint32 GetDcc256BAddr(
        const SubresId&  subResId) const
        { return GetMaskRam256BAddr(GetDcc(subResId.aspect), subResId); }

    uint32 GetCmask256BAddr() const;
    uint32 GetFmask256BAddr() const;

    bool HasDccStateMetaData(ImageAspect  aspect) const
        { return (m_dccStateMetaDataOffset[GetAspectIndex(aspect)] != 0); }

    bool HasFastClearEliminateMetaData(ImageAspect  aspect) const
        { return m_fastClearEliminateMetaDataOffset[GetAspectIndex(aspect)] != 0; }

    gpusize GetDccStateMetaDataAddr(const SubresId&  subResId) const;
    gpusize GetDccStateMetaDataOffset(const SubresId&  subResId) const;
    gpusize GetDccStateMetaDataSize(const SubresId&  subResId, uint32 numMips) const;

    // Returns true if this Image has associated mask-ram data.
    bool HasColorMetaData() const { return HasFmaskData() || HasDccData(); }

    // Returns true if this Image has associated DCC data.
    bool HasDccData() const { return (m_numDccPlanes != 0); }
    virtual bool HasFmaskData() const override;

    bool HasHtileLookupTable() const
        { return (HasHtileData() && (m_metaDataLookupTableOffsets[0] != 0u)); }

    bool SupportsCompToReg(ImageLayout layout, const SubresId& subResId) const;

    // Returns a pointer to the Gfx9Dcc object associated with a particular sub-Resource.
    const  Gfx9Dcc*     GetDcc(ImageAspect  aspect) const { return m_pDcc[GetAspectIndex(aspect)]; }
    const  Gfx9Dcc*     GetDisplayDcc(ImageAspect aspect) const { return m_pDispDcc[GetAspectIndex(aspect)]; }
    const  Gfx9Cmask*   GetCmask() const { return m_pCmask; }
    const  Gfx9Fmask*   GetFmask() const { return m_pFmask; }
    const Gfx9MaskRam*  GetColorMaskRam(ImageAspect  aspect) const
    {
        return ((HasDccData())
            ? static_cast<const Gfx9MaskRam*>(GetDcc(aspect))
            : static_cast<const Gfx9MaskRam*>(GetCmask()));
    }
    const Gfx9MaskRam*  GetPrimaryMaskRam(ImageAspect  aspect) const
    {
        return (HasDccData()
                ? static_cast<const Gfx9MaskRam*>(GetDcc(aspect))
                : static_cast<const Gfx9MaskRam*>(GetHtile()));
    }

    gpusize GetMaskRamBaseAddr(const MaskRam*   pMaskRam, const SubresId&  subResId) const;

    const ColorLayoutToState& LayoutToColorCompressionState() const { return m_layoutToState.color; }
    const DepthStencilLayoutToState& LayoutToDepthCompressionState(const SubresId& subresId) const;

    bool ColorImageSupportsAllFastClears() const;

    virtual uint32 GetSwTileMode(const SubResourceInfo*  pSubResInfo) const override
        { return GetAddrSettings(pSubResInfo).swizzleMode; }

    virtual void InitMetadataFill(
        Pal::CmdBuffer*    pCmdBuffer,
        const SubresRange& range,
        ImageLayout        layout) const override;

    virtual void Addr2InitSubResInfo(
        const SubResIterator&  subResIt,
        SubResourceInfo*       pSubResInfoList,
        void*                  pSubResTileInfoList,
        gpusize*               pGpuMemSize) override;

    virtual Result Addr2FinalizePlane(
        SubResourceInfo*                               pBaseSubRes,
        void*                                          pBaseTileInfo,
        const ADDR2_GET_PREFERRED_SURF_SETTING_OUTPUT& surfaceSetting,
        const ADDR2_COMPUTE_SURFACE_INFO_OUTPUT&       surfaceInfo) override;

    virtual void Addr2FinalizeSubresource(
        SubResourceInfo*                               pSubResInfo,
        const ADDR2_GET_PREFERRED_SURF_SETTING_OUTPUT& surfaceSetting) const override;

    virtual Result Finalize(
        bool               dccUnsupported,
        SubResourceInfo*   pSubResInfoList,
        void*              pTileInfoList,
        ImageMemoryLayout* pGpuMemLayout,
        gpusize*           pGpuMemSize,
        gpusize*           pGpuMemAlignment) override;

    bool SupportsComputeDecompress(const SubresId& subresId) const;

    bool IsComprFmaskShaderReadable(const SubresId& subresource) const;

    virtual ImageType GetOverrideImageType() const override;

    virtual gpusize GetAspectBaseAddr(ImageAspect  aspect) const override;

    virtual void GetSharedMetadataInfo(SharedMetadataInfo* pMetadataInfo) const override;
    virtual void GetDisplayDccState(DccState* pState) const override;
    virtual void GetDccState(DccState* pState) const override;

    gpusize GetMipAddr(SubresId subresId) const;

    void BuildMetadataLookupTableBufferView(BufferViewInfo* pViewInfo, uint32 mipLevel) const;

    bool IsInMetadataMipTail(const SubresId&  subResId) const;
    bool CanMipSupportMetaData(uint32 mip) const override;

    uint32 GetIterate256(const SubResourceInfo*  pSubResInfo) const;
    bool Gfx10UseCompToSingleFastClears() const { return m_useCompToSingleForFastClears; };

    gpusize GetGpuMemSyncSize() const { return m_gpuMemSyncSize; }

    bool IsColorDataZeroOrOne(const uint32*  pColor, uint32 compIdx) const;

    virtual Result GetDefaultGfxLayout(SubresId subresId, ImageLayout* pLayout) const override;

    bool IsHtileDepthOnly() const;

    bool ImageSupportsShaderReadsAndWrites() const;

    bool NeedFlushForMetadataPipeMisalignment(const SubresRange& range) const;

private:
    // Address dimensions are calculated on a per-plane (aspect) basis
    ADDR2_COMPUTE_SURFACE_INFO_OUTPUT        m_addrSurfOutput[MaxNumPlanes];
    ADDR2_MIP_INFO                           m_addrMipOutput[MaxNumPlanes][MaxImageMipLevels];
    ADDR2_GET_PREFERRED_SURF_SETTING_OUTPUT  m_addrSurfSetting[MaxNumPlanes];

    // The byte offset of where each aspect begins, relative to the image's bound memory.
    gpusize                                  m_aspectOffset[MaxNumPlanes];

    // For YUV planar surfaces, this is the size of one slice worth of data across all aspects.
    // For other surfaces, this is the image size.
    gpusize                                  m_totalAspectSize;

    const Device&  m_gfxDevice;
    Gfx9Htile*     m_pHtile;
    Gfx9Dcc*       m_pDcc[MaxNumPlanes];
    uint32         m_numDccPlanes;
    Gfx9Dcc*       m_pDispDcc[MaxNumPlanes];
    Gfx9Cmask*     m_pCmask;
    Gfx9Fmask*     m_pFmask;

    MetaDataClearConst  m_metaDataClearConst[MetaDataNumTypes];

    gpusize  m_dccStateMetaDataOffset[MaxNumPlanes]; // Offset to beginning of DCC state metadata
    gpusize  m_dccStateMetaDataSize[MaxNumPlanes];   // Size of the DCC state metadata

    gpusize  m_fastClearEliminateMetaDataOffset[MaxNumPlanes]; // Offset to start of FCE metadata
    gpusize  m_fastClearEliminateMetaDataSize[MaxNumPlanes];   // Size of the FCE metadata

    gpusize m_waTcCompatZRangeMetaDataOffset;       // Offset to start of waTcCompatZRange MetaData
    gpusize m_waTcCompatZRangeMetaDataSizePerMip;   // Size of the waTcCompatZRange MetaData per mip level

    gpusize m_metaDataLookupTableOffsets[MaxImageMipLevels]; // Offset to lookup table for htile or cmask of each mip
    gpusize m_metaDataLookupTableSizes[MaxImageMipLevels];   // Size of lookup table for htile or cmask of each mip

    gpusize m_gpuMemSyncSize; // Total size of the the image and metadata before any required allocation padding

    union
    {
        // For color images and their compression states
        ColorLayoutToState  color;

        // For depth-stencil images and their compression states (one each for depth and stencil aspects).
        DepthStencilLayoutToState  depthStencil[2];
    } m_layoutToState;

    union
    {
        ImageLayout color;
        ImageLayout depthStencil[2];
    } m_defaultGfxLayout;

    bool m_useCompToSingleForFastClears;

    // Tracks the first mip level which needs an L2 flush & invalidation under certain circumstances due to metadata
    // not being pipe-aligned all the time in hardware.  A value of UINT_MAX means no mip levels require this
    // workaround, a value of zero means all mips require it.  See InitPipeMisalignedMetadataFirstMip() for details.
    uint32  m_firstMipMetadataPipeMisaligned[MaxNumPlanes];

    uint32 GetAspectIndex(ImageAspect  aspect) const;

    void InitDccStateMetaData(
        uint32             planeIdx,
        ImageMemoryLayout* pGpuMemLayout,
        gpusize*           pGpuMemSize);
    void InitFastClearEliminateMetaData(
        ImageAspect        aspect,
        ImageMemoryLayout* pGpuMemLayout,
        gpusize*           pGpuMemSize);
    void InitWaTcCompatZRangeMetaData(
        ImageMemoryLayout* pGpuMemLayout,
        gpusize*           pGpuMemSize);

    void InitHtileLookupTable(
        ImageMemoryLayout* pGpuMemLayout,
        gpusize*           pGpuOffset,
        gpusize*           pGpuMemAlignment);

    bool IsFastClearColorMetaFetchable(const uint32* pColor) const;
    PAL_INLINE bool IsFastClearDepthMetaFetchable(float depth) const;
    PAL_INLINE bool IsFastClearStencilMetaFetchable(uint8 stencil) const;
    void SetupAspectOffsets();

    void Addr2InitSubResInfoGfx9(
        const SubResIterator&  subResIt,
        SubResourceInfo*       pSubResInfoList,
        void*                  pSubResTileInfoList,
        gpusize*               pGpuMemSize);

    void CheckCompToSingle();

    ImageAspect GetAspectFromPlane(uint32  planeIdx) const;

    Result CreateDccObject(
        SubResourceInfo*   pSubResInfoList,
        ImageMemoryLayout* pGpuMemLayout,
        gpusize*           pGpuMemSize,
        gpusize*           pGpuMemAlignment);

    void Addr2InitSubResInfoGfx10(
        const SubResIterator&  subResIt,
        SubResourceInfo*       pSubResInfoList,
        void*                  pSubResTileInfoList,
        gpusize*               pGpuMemSize);

    // Returns a bitfield indicating what the possible uses of the hTile surface are.  No bits will
    // be set if hTile does not exist.
    HtileUsageFlags GetHtileUsage() const;

    void GetMetaEquationConstParam(
         MetaDataClearConst* pParam,
         const uint32        metaBlkFastClearSize,
         bool                cMaskMetaData = false) const;

    void InitLayoutStateMasks();
    void InitPipeMisalignedMetadataFirstMip();
    uint32 GetPipeMisalignedMetadataFirstMip(
        const ImageCreateInfo& createInfo,
        const SubResourceInfo& baseSubRes) const;

    uint32  GetMaskRam256BAddr(
        const Gfx9MaskRam*  pMaskRam,
        const SubresId&     subResId) const;

    bool SupportsMetaDataTextureFetch(AddrSwizzleMode tileMode, ChNumFormat format, const SubresId& subResource) const;
    bool ColorImageSupportsMetaDataTextureFetch() const;
    bool DepthMetaDataTexFetchIsZValid(ChNumFormat format) const;
    bool DepthImageSupportsMetaDataTextureFetch(ChNumFormat format, const SubresId& subResource) const;
    bool DoesImageSupportCopySrcCompression() const;

    // These static variables ensure that we are assigning a rotating set of swizzle indices for each new image.
    static uint32  s_cbSwizzleIdx;
    static uint32  s_txSwizzleIdx;
    static uint32  s_fMaskSwizzleIdx;

    PAL_DISALLOW_DEFAULT_CTOR(Image);
    PAL_DISALLOW_COPY_AND_ASSIGN(Image);
};

// Helper functions to get a Gfx9::Image from an IImage.
extern Image* GetGfx9Image(const IImage* pImage);
extern const Image& GetGfx9Image(const IImage& image);

} // Gfx9
} // Pal
