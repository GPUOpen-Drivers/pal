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

#include "core/hw/gfxip/rpm/rsrcProcMgr.h"
#include "core/hw/gfxip/gfx12/gfx12FormatInfo.h"

namespace Pal
{

class QueryPool;

namespace Gfx12
{

class Device;
class Image;
enum  HiSZType : uint32;

// =====================================================================================================================
// Gfx12 hardware layer implementation of the Resource Processing Manager.
class RsrcProcMgr final : public Pal::RsrcProcMgr
{
public:
    explicit RsrcProcMgr(Device* pDevice);

    void ExpandHiSZWithFullRange(
        GfxCmdBuffer*      pCmdBuffer,
        const Pal::IImage& image,
        const SubresRange& range,
        bool               trackBltActiveFlags) const;

    void FixupHiSZWithClearValue(
        GfxCmdBuffer*      pCmdBuffer,
        const Pal::IImage& image,
        const SubresRange& range,
        float              depth,
        uint8              stencil,
        bool               trackBltActiveFlags) const;

    virtual bool UseImageCloneCopy(
        GfxCmdBuffer*          pCmdBuffer,
        const Pal::Image&      srcImage,
        ImageLayout            srcImageLayout,
        const Pal::Image&      dstImage,
        ImageLayout            dstImageLayout,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        uint32                 flags) const override;

    virtual void CmdCloneImageData(
        GfxCmdBuffer*     pCmdBuffer,
        const Pal::Image& srcImage,
        const Pal::Image& dstImage) const override;

    void EchoGlobalInternalTableAddr(
        GfxCmdBuffer*           pCmdBuffer,
        gpusize                 dstAddr) const;

    virtual void CmdUpdateMemory(
        GfxCmdBuffer*    pCmdBuffer,
        const GpuMemory& dstMem,
        gpusize          dstOffset,
        gpusize          dataSize,
        const uint32*    pData) const override;

    virtual void CmdResolveQuery(
        GfxCmdBuffer*         pCmdBuffer,
        const Pal::QueryPool& queryPool,
        QueryResultFlags      flags,
        QueryType             queryType,
        uint32                startQuery,
        uint32                queryCount,
        const GpuMemory&      dstGpuMemory,
        gpusize               dstOffset,
        gpusize               dstStride) const override;

    virtual void CmdResolveImage(
        GfxCmdBuffer*             pCmdBuffer,
        const Pal::Image&         srcImage,
        ImageLayout               srcImageLayout,
        const Pal::Image&         dstImage,
        ImageLayout               dstImageLayout,
        ResolveMode               resolveMode,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions,
        uint32                    flags) const override;

    virtual void CmdResolvePrtPlusImage(
        GfxCmdBuffer*                    pCmdBuffer,
        const IImage&                    srcImage,
        ImageLayout                      srcImageLayout,
        const IImage&                    dstImage,
        ImageLayout                      dstImageLayout,
        PrtPlusResolveType               resolveType,
        uint32                           regionCount,
        const PrtPlusImageResolveRegion* pRegions) const override;

    virtual void CmdClearColorImage(
        GfxCmdBuffer*         pCmdBuffer,
        const Pal::Image&     dstImage,
        ImageLayout           dstImageLayout,
        const ClearColor&     color,
        const SwizzledFormat& clearFormat,
        uint32                rangeCount,
        const SubresRange*    pRanges,
        uint32                boxCount,
        const Box*            pBoxes,
        uint32                flags) const override;

    virtual void CmdClearDepthStencil(
        GfxCmdBuffer*      pCmdBuffer,
        const Pal::Image&  dstImage,
        ImageLayout        depthLayout,
        ImageLayout        stencilLayout,
        float              depth,
        uint8              stencil,
        uint8              stencilWriteMask,
        uint32             rangeCount,
        const SubresRange* pRanges,
        uint32             rectCount,
        const Rect*        pRects,
        uint32             flags) const override;

    virtual void HwlResummarizeHtileCompute(
        GfxCmdBuffer*      pCmdBuffer,
        const GfxImage&    image,
        const SubresRange& range) const override
    {
        PAL_NOT_IMPLEMENTED();
    }

    virtual const bool IsGfxPipelineForFormatSupported(
        SwizzledFormat format) const override;

    virtual uint32 HwlBeginGraphicsCopy(
        GfxCmdBuffer*                pCmdBuffer,
        const Pal::GraphicsPipeline* pPipeline,
        const Pal::Image&            dstImage,
        uint32                       bpp) const override
    {
        PAL_NOT_IMPLEMENTED();
        return 0;
    }

    virtual void HwlEndGraphicsCopy(GfxCmdStream* pCmdStream, uint32 restoreMask) const override
    {
        PAL_NOT_IMPLEMENTED();
    }

    virtual bool IsAc01ColorClearCode(
        const GfxImage&       dstImage,
        const uint32*         pConvertedColor,
        const SwizzledFormat& clearFormat,
        const SubresRange&    clearRange) const override
    {
        PAL_NOT_IMPLEMENTED();
        return false;
    }

protected:
    virtual void FixupMetadataForComputeCopyDst(
        GfxCmdBuffer*           pCmdBuffer,
        const Pal::Image&       dstImage,
        ImageLayout             dstImageLayout,
        uint32                  regionCount,
        const ImageFixupRegion* pRegions,
        bool                    beforeCopy,
        const Pal::Image*       pFmaskOptimizedCopySrcImage = nullptr) const override { }

    virtual const Pal::GraphicsPipeline* GetGfxPipelineByFormat(
        RpmGfxPipeline basePipeline,
        SwizzledFormat format) const override;

    virtual ImageCopyEngine GetImageToImageCopyEngine(
        const GfxCmdBuffer*    pCmdBuffer,
        const Pal::Image&      srcImage,
        const Pal::Image&      dstImage,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        uint32                 copyFlags) const override
    {
        return ImageCopyEngine::Compute;
    }

    virtual bool ScaledCopyImageUseGraphics(
        GfxCmdBuffer*           pCmdBuffer,
        const ScaledCopyInfo&   copyInfo) const override { return false; }

    // In gfx12, all MSAA swizzle modes were made identical to gfx10's "Z" swizzle modes. That means all gfx12
    // MSAA images store their samples sequentially and store pixels in micro-tiles in Morton/Z order.
    virtual bool CopyImageCsUseMsaaMorton(const Pal::Image& dstImage) const override { return true; };

private:
    // No need to implement it for GFX12 since srd bit no_edge_clamp could cover such corner case.
    virtual void HwlImageToImageMissingPixelCopy(
        GfxCmdBuffer*          pCmdBuffer,
        const Pal::Image&      srcImage,
        const Pal::Image&      dstImage,
        const ImageCopyRegion& region) const override { }

    virtual void FixupMetadataForComputeResolveDst(
        GfxCmdBuffer*             pCmdBuffer,
        const Pal::Image&         dstImage,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions) const override { }

    void ClearHiSZ(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       image,
        const SubresRange& clearRange,
        HiSZType           hiSZType,
        uint32             clearValue,
        bool               trackBltActiveFlags) const;

    const SPI_SHADER_EX_FORMAT DeterminePsExportFmt(
        SwizzledFormat format,
        bool           blendEnabled,
        bool           shaderExportsAlpha,
        bool           blendSrcAlphaToColor,
        bool           enableAlphaToCoverage) const;

    void DepthStencilClearGraphics(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        const SubresRange& range,
        float              depth,
        uint8              stencil,
        uint8              stencilWriteMask,
        uint32             clearMask,
        ImageLayout        depthLayout,
        ImageLayout        stencilLayout,
        uint32             boxCnt,
        const Box*         pBox) const;

    // Describes some overall characteristics of a linear clear in a single plane. This will be used in multiple clear
    // path selection heuristics and in the linear clear implementation.
    struct LinearClearDesc
    {
        SubresRange      clearRange;       // The plane and mips/slices to clear.
        SwizzledFormat   baseFormat;       // the clear's intended format (after default format selection).
        uint32           baseFormatBpp;    // The BPP of the clear's intended format.
        uint32           samples;          // The number of samples in the image.
        gpusize          planeAddr;        // The plane's base GPU VA.
        gpusize          planeSize;        // The plane's size (including padding).
        Addr3SwizzleMode swizzleMode;      // The plane's swizzle mode.
        CompressionMode  compressionMode;  // The compression mode the clear should use when writing.
        bool             compressedWrites; // If writes will be compressed.
        bool             isDepthStencil;   // If the image is a depth-stencil target.
    };

    static bool LinearClearSupportsImage(
        const Pal::Image& dstImage,
        const ClearColor& color,
        SubresRange       firstRange,
        uint32            boxCount,
        const Box*        pBoxes);
    bool FillLinearClearDesc(
        const Pal::Image& dstImage,
        SubresRange       clearRange,
        SwizzledFormat    baseFormat,
        LinearClearDesc*  pDesc) const;
    bool TryLinearImageClear(
        GfxCmdBuffer*           pCmdBuffer,
        const Pal::Image&       dstImage,
        const Gfx12PalSettings& gfx12Settings,
        const LinearClearDesc&  desc,
        const ClearColor&       color,
        bool                    trackBltActiveFlags) const;
    void FillMem128Bit(
        GfxCmdBuffer*   pCmdBuffer,
        CompressionMode compressionMode,
        gpusize         dstGpuVirtAddr,
        gpusize         fillSize,
        const uint32    data[4],
        bool            mallNoAlloc) const;
#if PAL_BUILD_NAVI48
    static bool ExpectLinearIsFasterNavi48(
        const LinearClearDesc& desc,
        const SubResourceInfo& subresInfo);
#endif

    bool IsColorGfxClearPreferred(
        const Gfx12PalSettings& settings,
        const LinearClearDesc&  desc,
        bool                    linearClearSupported,
        bool                    clearAutoSync) const;

    bool IsDepthStencilGfxClearPreferred(bool clearAutoSync) const;

    PAL_DISALLOW_DEFAULT_CTOR(RsrcProcMgr);
    PAL_DISALLOW_COPY_AND_ASSIGN(RsrcProcMgr);
};

} // namespace Gfx12
} // namespace Pal
