/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/addrMgr/addrMgr3/addrMgr3.h"
#include "core/hw/gfxip/gfxImage.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"

namespace Pal
{
namespace Gfx12
{

// Forward decl's:
class HiSZ;

// Specifies the HiZ/HiS state of a depth/stencil image.
enum DepthStencilHiSZState : uint32
{
    DepthStencilNoHiSZ   = 0, // HiZ/HiS incompatible state.
    DepthStencilWithHiSZ = 1, // HiZ/HiS compatible state.
};

// =====================================================================================================================
// Returns the image's HiZ/HiS state based on provided layout info.
inline DepthStencilHiSZState ImageLayoutToDepthStencilHiSZState(
    ImageLayout hiSZValidLayout,
    ImageLayout imageLayout)
{
    DepthStencilHiSZState state = DepthStencilNoHiSZ;

    if ((imageLayout.engines != 0)                                                   &&
        (Util::TestAnyFlagSet(imageLayout.usages, ~hiSZValidLayout.usages) == false) &&
        (Util::TestAnyFlagSet(imageLayout.engines, ~hiSZValidLayout.engines) == false))
    {
        state = DepthStencilWithHiSZ;
    }

    return state;
}

// =====================================================================================================================
// GFX12-specific Image derived class, responsible for hardware-specific functionality like HW-specific addressing
// and metadata.
class Image final : public GfxImage
{
public:
    Image(Pal::Image* pParentImage, ImageInfo* pImageInfo, const Pal::Device& device);
    virtual ~Image();

    const ADDR3_COMPUTE_SURFACE_INFO_OUTPUT& GetAddrOutput(SubresId subresId) const
        { return m_addrSurfOutput[subresId.plane]; }

    const ADDR3_MIP_INFO& GetAddrMipOutput(SubresId subresId) const
        { return m_addrMipOutput[subresId.plane][subresId.mipLevel]; }

    const Addr3SwizzleMode GetFinalSwizzleMode(SubresId subresId) const
        { return m_finalSwizzleModes[subresId.plane]; }

    virtual void Addr3InitSubResInfo(
        const SubResIterator&  subResIt,
        SubResourceInfo*       pSubResInfoList,
        void*                  pSubResTileInfoList,
        gpusize*               pGpuMemSize) override;

    virtual Result Addr3FinalizePlane(
        SubResourceInfo*                         pBaseSubRes,
        void*                                    pBaseTileInfo,
        Addr3SwizzleMode                         swizzleMode,
        const ADDR3_COMPUTE_SURFACE_INFO_OUTPUT& surfaceInfo) override;

    virtual void Addr3FinalizeSubresource(
        SubResourceInfo*                              pSubResInfo,
        Addr3SwizzleMode                              swizzleMode) const override;

    virtual void PadYuvPlanarViewActualExtent(
        SubresId  subresource,
        Extent3d* pActualExtent // In: Original actualExtent of subresource. Out: padded actualExtent
        ) const override;

    Result ComputePipeBankXor(
        uint32            plane,
        Addr3SwizzleMode  swizzleMode,
        uint32*           pPipeBankXor) const;

    virtual Result Finalize(
        bool               dccUnsupported,
        SubResourceInfo*   pSubResInfoList,
        ImageMemoryLayout* pGpuMemLayout,
        gpusize*           pGpuMemSize,
        gpusize*           pGpuMemAlignment) override;

    virtual gpusize GetPlaneBaseAddr(uint32 plane, uint32 arraySlice = 0) const override
        { return GetMipAddr(Subres(plane, 0, arraySlice), true); }

    gpusize GetMipAddr(SubresId subresId, bool includeXor) const;

    virtual gpusize GetSubresourceAddr(SubresId subresId) const override;

    virtual uint32 GetSwTileMode(const SubResourceInfo*  pSubResInfo) const override
        { return static_cast<uint32>(GetSwTileMode(pSubResInfo->subresId)); }

    Addr3SwizzleMode GetSwTileMode(SubresId subresId) const
        { return AddrMgr3::GetTileInfo(Parent(), subresId)->swizzleMode; }

    virtual uint32 GetTileSwizzle(SubresId subresId) const override;

    virtual uint32 GetHwSwizzleMode(const SubResourceInfo* pSubResInfo) const override;

    virtual bool IsSubResourceLinear(SubresId subresId) const override
        { return AddrMgr3::IsLinearSwizzleMode(GetSwTileMode(subresId)); }

    virtual bool IsFormatReplaceable(
        SubresId    subresId,
        ImageLayout layout,
        bool        isDst,
        uint8       disabledChannelMask = 0) const override;

    virtual bool HasFmaskData() const override { return false; }

    virtual void GetSharedMetadataInfo(SharedMetadataInfo* pMetadataInfo) const override;

    virtual void InitMetadataFill(
        Pal::CmdBuffer*    pCmdBuffer,
        const SubresRange& range,
        ImageLayout        layout) const override;

    virtual bool ShaderWriteIncompatibleWithLayout(
        SubresId    subresId,
        ImageLayout layout) const override
    {
        return false;
    }

    bool HasHiSZ() const { return (m_pHiSZ != nullptr); }

    // Returns a pointer to the HiSZ object associated with this image
    const HiSZ* GetHiSZ() const { return m_pHiSZ; }

    ImageLayout GetHiSZValidLayout(uint32 plane) const
    {
        PAL_ASSERT(plane < MaxNumPlanes);
        return m_hiSZValidLayout[plane];
    }

    ImageLayout GetHiSZValidLayout(const SubresRange& subresRange) const;

    gpusize ComputeNonBlockCompressedView(
        const SubResourceInfo* pBaseSubResInfo,
        const SubResourceInfo* pMipSubResInfo,
        uint32*                pMipLevels,
        uint32*                pMipId,
        Extent3d*              pExtent) const;

    virtual bool HasHtileData() const override { return false; }

    virtual bool IsFastColorClearSupported(
        GfxCmdBuffer*      pCmdBuffer,
        ImageLayout        colorLayout,
        const uint32*      pColor,
        const SubresRange& range) override { return false; }

    virtual bool IsFastDepthStencilClearSupported(
        ImageLayout        depthLayout,
        ImageLayout        stencilLayout,
        float              depth,
        uint8              stencil,
        uint8              stencilWriteMask,
        const SubresRange& range) const override { return false; }

    virtual void GetDccControlBlockSize(DccControlBlockSize* pBlockSize) const override { *pBlockSize = m_dccControl; }

    uint32 GetMaxUncompressedSize(uint32 plane) const
    {
        return (plane == 0) ? m_dccControl.maxUncompressedBlockSizePlane0 : m_dccControl.maxUncompressedBlockSizePlane1;
    }

    uint32 GetMaxCompressedSize(uint32 plane) const
    {
        return (plane == 0) ? m_dccControl.maxCompressedBlockSizePlane0 : m_dccControl.maxCompressedBlockSizePlane1;
    }

    bool HasHiSZStateMetaData() const { return m_hiSZStateMetaDataOffset != 0; }

    gpusize HiSZStateMetaDataAddr(uint32 mipLevel) const;
    gpusize HiSZStateMetaDataOffset(uint32 mipLevel) const;

    uint32* UpdateHiSZStateMetaData(
        const SubresRange& range,
        bool               enable,  // If allow enabling HiZ/HiS
        Pm4Predicate       predicate,
        EngineType         engineType,
        uint32*            pCmdSpace) const;

private:
    void SetupPlaneOffsets(uint32 numPlanes, bool isYuvPlanar);
    void InitHiSZStateMetaData(ImageMemoryLayout* pGpuMemLayout, gpusize* pGpuMemSize);

    void InitLayoutStateMasks();

    const Device& m_gfxDevice;
    gpusize       m_gpuMemSyncSize; // Total size of the image and metadata before any allocation padding

    // Address dimensions are calculated on a per-plane basis
    ADDR3_COMPUTE_SURFACE_INFO_OUTPUT       m_addrSurfOutput[MaxNumPlanes];
    ADDR3_MIP_INFO                          m_addrMipOutput[MaxNumPlanes][MaxImageMipLevels];
    Addr3SwizzleMode                        m_finalSwizzleModes[MaxNumPlanes];
    DccControlBlockSize                     m_dccControl;

    // The byte offset of where each plane begins, relative to the image's bound memory.
    gpusize m_planeOffset[MaxNumPlanes];

    // For YUV planar surfaces, this is the size of one slice worth of data across all planes.  For other surfaces,
    // this is the image size.
    gpusize m_totalPlaneSize;

    HiSZ*       m_pHiSZ;
    ImageLayout m_hiSZValidLayout[MaxNumPlanes]; // Both for depth and stencil enabled case.

    gpusize  m_hiSZStateMetaDataOffset;       // Offset to beginning of HiSZ state metadata, tracked with one DWORD
                                              // per each miplevel.

    static constexpr uint32 HiSZStateMetaDataSizePerMip = sizeof(uint32);

    // These static variables ensure that we are assigning a rotating set of swizzle indices for each new image.
    static uint32 s_cbSwizzleIdx;
    static uint32 s_txSwizzleIdx;

    PAL_DISALLOW_DEFAULT_CTOR(Image);
    PAL_DISALLOW_COPY_AND_ASSIGN(Image);
};

// Helper function to get a Gfx12::Image from an IImage.
extern const Image* GetGfx12Image(const IImage* pImage);

} // Gfx12
} // Pal
