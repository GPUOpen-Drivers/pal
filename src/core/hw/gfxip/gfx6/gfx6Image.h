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
#include "core/hw/gfxip/gfx6/gfx6Chip.h"
#include "core/hw/gfxip/gfx6/gfx6MaskRam.h"
#include "core/hw/gfxip/gfxImage.h"
#include "core/addrMgr/addrMgr1/addrMgr1.h"
#include "palCmdBuffer.h"

namespace Pal
{

struct BufferViewInfo;
class  Device;

namespace Gfx6
{

class CmdUtil;

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

// Enumeration of depth-stencil TC Compatibility support modes.
enum Gfx8TcCompatibleResolveDst : uint32
{
    Gfx8TcCompatibleResolveDstDepthOnly = 0x00000001,
    Gfx8TcCompatibleResolveDstStencilOnly = 0x00000002,
    Gfx8TcCompatibleResolveDstDepthAndStencil = 0x00000004,
};

// Information used to determine the color compression state for an Image layout.
struct ColorLayoutToState
{
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
// This is the Gfx6 Image class which is derived from GfxImage.  It is responsible for hardware specific Image
// functionality such as setting up mask ram, metadata, tile info, etc.
class Image : public GfxImage
{
public:
    static constexpr uint32 TcCompatibleResolveDst = 0;

    Image(Pal::Image* pParentImage, ImageInfo* pImageInfo, const Pal::Device& device);
    virtual ~Image();

    // Returns true if the specified sub-resource is linear, false if it's tiled
    virtual bool IsSubResourceLinear(const SubresId& subresource) const override
    {
        const AddrTileMode  tileMode = GetSubResourceTileMode(subresource);

        return (((tileMode == ADDR_TM_LINEAR_ALIGNED) || (tileMode == ADDR_TM_LINEAR_GENERAL)) ? true : false);
    }

    bool SupportsMetaDataTextureFetch(
        AddrTileMode    tileMode,
        AddrTileType    tileType,
        ChNumFormat     format,
        const SubresId& subResource) const;

    virtual bool IsFastColorClearSupported(GfxCmdBuffer*      pCmdBuffer,
                                           ImageLayout        colorLayout,
                                           const uint32*      pColor,
                                           const SubresRange& range) override;

    virtual bool IsFastDepthStencilClearSupported(ImageLayout        depthLayout,
                                                  ImageLayout        stencilLayout,
                                                  float              depth,
                                                  uint8              stencil,
                                                  const SubresRange& range) const override;

    virtual bool IsFormatReplaceable(const SubresId& subresId, ImageLayout layout, bool isDst) const override;

    virtual bool ShaderWriteIncompatibleWithLayout(const SubresId& subresId, ImageLayout layout) const override;

    virtual void OverrideGpuMemHeaps(GpuMemoryRequirements* pMemReqs) const override;

    // Returns true if this Image has associated HTile data.
    virtual bool HasHtileData() const override { return (m_pHtile == nullptr) ? false : true; }

    bool IsComprFmaskShaderReadable(const SubResourceInfo*  pSubResInfo) const;

    // Returns a pointer to the Gfx6Htile object associated with a particular sub-Resource.
    const Gfx6Htile* GetHtile(SubresId subres) const
        { return (HasHtileData() ? &m_pHtile[subres.mipLevel] : nullptr); }

    uint32 GetHtile256BAddr(SubresId subresource) const;

    void GetHtileBufferInfo(
        uint32           mipLevel,
        uint32           firstSlice,
        uint32           numSlices,
        HtileBufferUsage htileUsage,
        GpuMemory**      ppGpuMemory,
        gpusize*         pOffset,
        gpusize*         pDataSize) const;

    bool RequiresSeparateAspectInit() const;

    // Returns true if this Image has associated CMask data.
    bool HasCmaskData() const { return (m_pCmask == nullptr) ? false : true; }

    // Returns a pointer to the Gfx6Cmask object associated with a particular sub-Resource.
    const Gfx6Cmask* GetCmask(SubresId subres) const
        { return (HasCmaskData() ? &m_pCmask[subres.mipLevel] : nullptr); }

    uint32 GetCmask256BAddr(SubresId subresource) const;

    void GetCmaskBufferInfo(
        uint32      mipLevel,
        uint32      firstSlice,
        uint32      numSlices,
        GpuMemory** ppGpuMemory,
        gpusize*    pOffset,
        gpusize*    pDataSize) const;

    // Returns true if this Image has associated DCC data.
    virtual bool HasDccData() const { return (m_pDcc == nullptr) ? false : true; }

    AddrTileMode GetSubResourceTileMode(SubresId subresource) const;
    AddrTileType GetSubResourceTileType(SubresId subresource) const;

    // Returns a pointer to the Gfx6Dcc object associated with a particular sub-Resource.
    const Gfx6Dcc* GetDcc(SubresId subres) const
        { return (HasDccData() ? &m_pDcc[subres.mipLevel] : nullptr); }

    uint32 GetDcc256BAddr(SubresId subresource) const;

    void GetDccBufferInfo(
        uint32          mipLevel,
        uint32          firstSlice,
        uint32          numSlices,
        DccClearPurpose clearPurpose,
        GpuMemory**     ppGpuMemory,
        gpusize*        pOffset,
        gpusize*        pDataSize) const;

    bool CanMergeClearDccSlices(uint32 mipLevel) const;

    // Returns true if this Image has associated FMask data.
    virtual bool HasFmaskData() const override { return (m_pFmask == nullptr) ? false : true; }

    // Returns a pointer to the Gfx6Fmask object associated with a particular sub-Resource.
    const Gfx6Fmask* GetFmask(SubresId subres) const
        { return (HasFmaskData() ? &m_pFmask[subres.mipLevel] : nullptr); }

    gpusize GetFmaskBaseAddr(SubresId subresource) const;
    uint32  GetFmask256BAddrSwizzled(SubresId subresource) const;

    void GetFmaskBufferInfo(
        uint32      firstSlice,
        uint32      numSlices,
        GpuMemory** ppGpuMemory,
        gpusize*    pOffset,
        gpusize*    pDataSize) const;

    bool HasDccStateMetaData() const { return m_dccStateMetaDataOffset != 0; }

    bool HasFastClearEliminateMetaData() const { return m_fastClearEliminateMetaDataOffset != 0; }

    gpusize GetDccStateMetaDataAddr(uint32 mipLevel) const;
    gpusize GetDccStateMetaDataOffset(uint32 mipLevel) const;
    gpusize GetFastClearEliminateMetaDataAddr(uint32 mipLevel) const;
    gpusize GetFastClearEliminateMetaDataOffset(uint32 mipLevel) const;
    gpusize GetWaTcCompatZRangeMetaDataAddr(uint32 mipLevel) const;

    bool HasWaTcCompatZRangeMetaData() const { return m_waTcCompatZRangeMetaDataOffset != 0; }

    uint32* UpdateDepthClearMetaData(
        const SubresRange& range,
        uint32             writeMask,
        float              depthValue,
        uint8              stencilValue,
        PM4Predicate       predicate,
        uint32*            pCmdSpace) const;
    uint32* UpdateHiSPretestsMetaData(
        const SubresRange& range,
        const HiSPretests& pretests,
        PM4Predicate       predicate,
        uint32*            pCmdSpace) const;
    uint32* UpdateWaTcCompatZRangeMetaData(
        const SubresRange& range,
        float              depthValue,
        PM4Predicate       predicate,
        uint32*            pCmdSpace) const;
    uint32* UpdateColorClearMetaData(
        uint32       startMip,
        uint32       numMips,
        const uint32 packedColor[4],
        PM4Predicate predicate,
        uint32*      pCmdSpace) const;
    uint32* UpdateDccStateMetaData(
        const SubresRange& range,
        bool               isCompressed,
        PM4Predicate       predicate,
        uint32*            pCmdSpace) const;
    uint32* UpdateFastClearEliminateMetaData(
        const SubresRange& range,
        uint32             value,
        PM4Predicate       predicate,
        uint32*            pCmdSpace) const;

    const ColorLayoutToState& LayoutToColorCompressionState(const SubresId& subresId) const
        { return m_layoutToState[subresId.mipLevel].color; }
    const DepthStencilLayoutToState& LayoutToDepthCompressionState(const SubresId& subresId) const;

    // Returns true if this Image has associated Cmask, Fmask, or DCC data.
    virtual bool HasColorMetaData() const
        { return HasFmaskData() || HasCmaskData() || HasDccData(); }

    // Returns true if this Image can use DCC-based fast-clear (clearing the DCC memory directly).
    bool UseDccFastClear(SubresId subres) const
        {  return m_pDcc[subres.mipLevel].UseFastClear(); }

    bool ColorImageSupportsAllFastClears() const;

    bool IsMacroTiled(const SubResourceInfo* pSubResInfo) const;
    static bool IsMacroTiled(AddrTileMode tileMode);

    uint32 GetSubresource256BAddrSwizzled(SubresId subresource) const;

    // Returns one of the AddrTileMode enumerations
    virtual uint32 GetSwTileMode(const SubResourceInfo*  pSubResInfo) const override
        { return GetSubResourceTileMode(pSubResInfo->subresId); }

    virtual void InitMetadataFill(Pal::CmdBuffer* pCmdBuffer, const SubresRange& range) const override;

    bool SupportsComputeDecompress(const SubresId& subresId) const;

    virtual Result Addr1InitSurfaceInfo(
        uint32                           subResIdx,
        ADDR_COMPUTE_SURFACE_INFO_INPUT* pSurfInfo) override;

    virtual void Addr1FinalizeSubresource(
        uint32                                  subResIdx,
        SubResourceInfo*                        pSubResInfoList,
        void*                                   pTileInfoList,
        const ADDR_COMPUTE_SURFACE_INFO_OUTPUT& surfInfo) override;

    virtual Result Finalize(
        bool               dccUnsupported,
        SubResourceInfo*   pSubResInfoList,
        void*              pTileInfoList,
        ImageMemoryLayout* pGpuMemLayout,
        gpusize*           pGpuMemSize,
        gpusize*           pGpuMemAlignment) override;

    // Fills metadata info to be used in memory export
    virtual void GetSharedMetadataInfo(SharedMetadataInfo* pMetadataInfo) const override;

    virtual Result GetDefaultGfxLayout(SubresId subresId, ImageLayout* pLayout) const override
    {
        // Gfx6 doesn't support Release-acquire based barrier.
        return Result::Success;
    }

private:
    PAL_INLINE bool IsFastClearColorMetaFetchable(const uint32* pColor) const;
    PAL_INLINE bool IsFastClearDepthMetaFetchable(float depth) const;
    PAL_INLINE bool IsFastClearStencilMetaFetchable(uint8 stencil) const;

    void InitLayoutStateMasks(bool allowComputeDecompress);
    void InitLayoutStateMasksOneMip(bool allowComputeDecompress, const SubresId& subresId);

    Gfx6Htile*  m_pHtile;   // Array of HTile objects, one for each mip level.
    Gfx6Cmask*  m_pCmask;   // Array of CMask objects, one for each mip level.
    Gfx6Fmask*  m_pFmask;   // Array of FMask objects, one for each mip level.
    Gfx6Dcc*    m_pDcc;     // Array of DCC objects, one for each mip level

    gpusize  m_dccStateMetaDataOffset;  // Offset to beginning of DCC state metadata
    gpusize  m_dccStateMetaDataSize;    // Size of the DCC state metadata

    gpusize  m_fastClearEliminateMetaDataOffset;    // Offset to start of FCE metadata
    gpusize  m_fastClearEliminateMetaDataSize;      // Size of the FCE metadata

    gpusize m_waTcCompatZRangeMetaDataOffset;       // Offset to start of waTcCompatZRange metadata
    gpusize m_waTcCompatZRangeMetaDataSizePerMip;   // Size of the waTcCompatZRange metadata per mip level

    union
    {
        // For color images and their compression states
       ColorLayoutToState  color;

        // For depth-stencil images and their compression states (one each for depth and stencil aspects).
        DepthStencilLayoutToState  depthStencil[2];
    }  m_layoutToState[MaxImageMipLevels];

    void InitDccStateMetaData(
        ImageMemoryLayout* pGpuMemLayout,
        gpusize*           pGpuMemSize);
    void InitFastClearEliminateMetaData(
        ImageMemoryLayout* pGpuMemLayout,
        gpusize*           pGpuMemSize);
    void InitWaTcCompatZRangeMetaData(
        ImageMemoryLayout* pGpuMemLayout,
        gpusize*           pGpuMemSize);

    bool DoesTileInfoMatch(const SubresId& subresId) const;
    bool DepthMetaDataTexFetchIsZValid(ChNumFormat  format) const;

    bool ColorImageSupportsMetaDataTextureFetch(
        AddrTileMode tileMode,
        AddrTileType tileType) const;
    bool DepthImageSupportsMetaDataTextureFetch(
        ChNumFormat     format,
        const SubresId& subResource) const;

    Result ComputeSubResourceInfo(
        uint32           baseTileMode,
        uint32           baseTileType,
        uint32           baseActualWidth,
        SubResourceInfo* pSubResInfo,
        bool*            pDccUnsupported,
        int32*           pStencilTileIdx);

    Result ComputeAddrTileMode(uint32 subResIdx, AddrTileMode* pTileMode) const;

    void SetupBankAndPipeSwizzle(
        uint32                                  subResIdx,
        void*                                   pTileInfoList,
        const ADDR_COMPUTE_SURFACE_INFO_OUTPUT& surfInfo) const;

    uint32 ComputeBaseTileSwizzle(
        const ADDR_COMPUTE_SURFACE_INFO_OUTPUT& surfOut,
        const SubResourceInfo&                  subResInfo) const;

    bool ApplyXthickDccWorkaround(AddrTileMode tileMode) const;

    static uint32 HwMicroTileModeFromAddrTileType(AddrTileType addrType);
    static uint32 HwArrayModeFromAddrTileMode(AddrTileMode addrMode);

    // These static variables ensure that we are assigning a rotating set of swizzle indices for each new image.
    static uint32  s_cbSwizzleIdx;
    static uint32  s_txSwizzleIdx;

    PAL_DISALLOW_DEFAULT_CTOR(Image);
    PAL_DISALLOW_COPY_AND_ASSIGN(Image);
};

// Helper functions to get a Gfx6::Image from an IImage.
extern Image* GetGfx6Image(const IImage* pImage);
extern const Image& GetGfx6Image(const IImage& image);

} // Gfx6
} // Pal
