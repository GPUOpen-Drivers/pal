/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfxImage.h"

namespace Pal
{

// Forward declarations
class      GfxCmdBuffer;
enum class ClearMethod : uint32;

// Compute expand methods
enum UseComputeExpand : uint32
{
    UseComputeExpandDepth        = 0x00000001,
    UseComputeExpandMsaaDepth    = 0x00000002,
    UseComputeExpandDcc          = 0x00000004,
    UseComputeExpandDccWithFmask = 0x00000008,
    UseComputeExpandAlways       = 0x00000010,
};

// =====================================================================================================================
class Pm4Image : public GfxImage
{
public:
    static constexpr uint32 UseComputeExpand = UseComputeExpandDepth | UseComputeExpandDcc;

    virtual ~Pm4Image() {}

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

    bool HasFastClearMetaData(uint32 plane) const
        { return m_fastClearMetaDataOffset[GetFastClearIndex(plane)] != 0; }
    bool HasFastClearMetaData(const SubresRange& range) const;

    gpusize FastClearMetaDataAddr(const SubresId&  subResId) const;
    gpusize FastClearMetaDataOffset(const SubresId&  subResId) const;
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

    // Helper function for AddrMgr1 to initialize the AddrLib surface info strucutre for a subresource.
    virtual Result Addr1InitSurfaceInfo(
        uint32                           subResIdx,
        ADDR_COMPUTE_SURFACE_INFO_INPUT* pSurfInfo) { return Result::ErrorUnavailable; }

    // Helper function for AddrMgr1 to finalize the subresource and tiling info for a subresource after
    // calling AddrLib.
    virtual void Addr1FinalizeSubresource(
        uint32                                  subResIdx,
        SubResourceInfo*                        pSubResInfoList,
        void*                                   pTileInfoList,
        const ADDR_COMPUTE_SURFACE_INFO_OUTPUT& surfInfo) { PAL_NEVER_CALLED(); }

protected:
    Pm4Image(
        Image*        pParentImage,
        ImageInfo*    pImageInfo,
        const Device& device);

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

    virtual void Destroy() override;

    gpusize  m_fastClearMetaDataOffset[MaxNumPlanes];      // Offset to beginning of fast-clear metadata.
    gpusize  m_fastClearMetaDataSizePerMip[MaxNumPlanes];  // Size of fast-clear metadata per mip level.

    gpusize  m_hiSPretestsMetaDataOffset;     // Offset to beginning of HiSPretest metadata
    gpusize  m_hiSPretestsMetaDataSizePerMip; // Size of HiSPretest metadata per mip level.

    bool     m_hasSeenNonTcCompatClearColor;  // True if this image has been cleared with non TC-compatible color.

    uint32*  m_pNumSkippedFceCounter;

private:
    PAL_DISALLOW_DEFAULT_CTOR(Pm4Image);
    PAL_DISALLOW_COPY_AND_ASSIGN(Pm4Image);
};

} // Pal
