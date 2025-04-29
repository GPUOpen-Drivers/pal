/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

namespace Pal
{

class  GfxCmdBuffer;
class  QueryPool;
struct DepthStencilViewCreateInfo;

namespace Gfx9
{

class      CmdUtil;
class      DepthStencilView;
class      Device;
class      Gfx9Htile;
class      Gfx9MaskRam;
class      Image;
enum class DccClearPurpose : uint32;

// =====================================================================================================================
// GFX9 common hardware layer implementation of the Resource Processing Manager. It is most known for handling
// GFX9-specific resource operations like DCC decompression.
class RsrcProcMgr : public Pal::RsrcProcMgr
{
public:

    static constexpr bool ForceGraphicsFillMemoryPath = false;

    explicit RsrcProcMgr(Device* pDevice);
    virtual ~RsrcProcMgr() {}

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

    void DccDecompress(
        GfxCmdBuffer*                pCmdBuffer,
        Pal::CmdStream*              pCmdStream,
        const Image&                 image,
        const MsaaQuadSamplePattern* pQuadSamplePattern,
        const SubresRange&           range) const;

    void FmaskColorExpand(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       image,
        const SubresRange& range) const;

    void FmaskDecompress(
        GfxCmdBuffer*                pCmdBuffer,
        Pal::CmdStream*              pCmdStream,
        const Image&                 image,
        const MsaaQuadSamplePattern* pQuadSamplePattern,
        const SubresRange&           range) const;

    bool InitMaskRam(
        GfxCmdBuffer*                 pCmdBuffer,
        Pal::CmdStream*               pCmdStream,
        const Image&                  dstImage,
        const SubresRange&            range,
        ImageLayout                   layout) const;

    void BuildDccLookupTable(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       srcImage,
        const SubresRange& range) const;

    void FastClearEliminate(
        GfxCmdBuffer*                pCmdBuffer,
        Pal::CmdStream*              pCmdStream,
        const Image&                 image,
        const MsaaQuadSamplePattern* pQuadSamplePattern,
        const SubresRange&           range) const;

    bool ExpandDepthStencil(
        GfxCmdBuffer*                pCmdBuffer,
        const Pal::Image&            image,
        const MsaaQuadSamplePattern* pQuadSamplePattern,
        const SubresRange&           range) const;

    virtual void CmdCopyMemoryToImage(
        GfxCmdBuffer*                pCmdBuffer,
        const GpuMemory&             srcGpuMemory,
        const Pal::Image&            dstImage,
        ImageLayout                  dstImageLayout,
        uint32                       regionCount,
        const MemoryImageCopyRegion* pRegions,
        bool                         includePadding) const override;

    virtual void CmdCopyImageToMemory(
        GfxCmdBuffer*                pCmdBuffer,
        const Pal::Image&            srcImage,
        ImageLayout                  srcImageLayout,
        const GpuMemory&             dstGpuMemory,
        uint32                       regionCount,
        const MemoryImageCopyRegion* pRegions,
        bool                         includePadding) const override;

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
        const SubresRange& range) const override;

    bool WillDecompressColorWithCompute(
        const GfxCmdBuffer* pCmdBuffer,
        const Image&        gfxImage,
        const SubresRange&  range) const;

    bool WillDecompressDepthStencilWithCompute(
        const GfxCmdBuffer* pCmdBuffer,
        const Image&        gfxImage,
        const SubresRange&  range) const;

    bool WillResummarizeWithCompute(
        const GfxCmdBuffer* pCmdBuffer,
        const Pal::Image&   image) const;

    void EchoGlobalInternalTableAddr(
        GfxCmdBuffer* pCmdBuffer,
        gpusize       dstAddr) const;

    void CopyVrsIntoHtile(
        GfxCmdBuffer*           pCmdBuffer,        // cmd buffer to receive copy commands, must support compute
        const DepthStencilView* pDsView,           // depth view that contains image that owns dest hTile buffer
        bool                    isClientDsv,
        const Extent3d&         depthExtent,       // extent of the depth buffers' mip level
        const Pal::Image*       pSrcVrsImg) const; // source VRS data (can be NULL to imply 1x1)

    void CmdDisplayDccFixUp(
        GfxCmdBuffer*      pCmdBuffer,
        const Pal::Image&  image) const;

    void CmdGfxDccToDisplayDcc(
        GfxCmdBuffer*     pCmdBuffer,
        const Pal::Image& image) const;

    virtual bool CopyImageCsUseMsaaMorton(const Pal::Image& dstImage) const override;

    void CmdGenerateIndirectCmds(
        const IndirectCmdGenerateInfo& genInfo,
        CmdStreamChunk**               ppChunkLists[],
        uint32*                        pNumGenChunks) const;

protected:
    uint32  GetInitHtileClearMask(
        const Image&       dstImage,
        const SubresRange& clearRange) const;

    virtual bool CopyDstBoundStencilNeedsWa(
        const GfxCmdBuffer* pCmdBuffer,
        const Pal::Image&   dstImage) const override;

    virtual const Pal::GraphicsPipeline* GetGfxPipelineByFormat(
        RpmGfxPipeline basePipeline,
        SwizzledFormat format) const override;

    virtual const bool IsGfxPipelineForFormatSupported(
        SwizzledFormat format) const override;

    virtual bool CopyImageUseMipLevelInSrd(bool isCompressed) const override
        { return (RsrcProcMgr::UseMipLevelInSrd && (isCompressed == false)); }

    const Pal::ComputePipeline* GetCmdGenerationPipeline(
        const Pal::IndirectCmdGenerator& generator,
        const GfxCmdBuffer&              cmdBuffer) const;

    bool ClearDcc(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       dstImage,
        const SubresRange& clearRange,
        uint8              clearCode,
        DccClearPurpose    clearPurpose,
        bool               trackBltActiveFlags,
        const uint32*      pPackedClearColor = nullptr) const;

    void ClearDccCompute(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       dstImage,
        const SubresRange& clearRange,
        uint8              clearCode,
        DccClearPurpose    clearPurpose,
        bool               trackBltActiveFlags,
        const uint32*      pPackedClearColor = nullptr) const;

    void FastDepthStencilClearCompute(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        const SubresRange& range,
        uint32             htileValue,
        uint32             clearMask,
        uint8              stencil,
        bool               trackBltActiveFlags) const;

    void FastDepthStencilClearComputeCommon(
        GfxCmdBuffer*      pCmdBuffer,
        const Pal::Image*  pPalImage,
        uint32             clearMask) const;

    uint32 GetClearDepth(
        const Image& dstImage,
        uint32       plane,
        uint32       numSlices,
        uint32       mipLevel) const;

    virtual bool HwlUseFMaskOptimizedImageCopy(
        const Pal::Image&      srcImage,
        ImageLayout            srcImageLayout,
        const Pal::Image&      dstImage,
        ImageLayout            dstImageLayout,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions) const override;

    virtual void HwlFixupCopyDstImageMetadata(
        GfxCmdBuffer*           pCmdBuffer,
        const Pal::Image*       pSrcImage,
        const Pal::Image&       dstImage,
        ImageLayout             dstImageLayout,
        const ImageFixupRegion* pRegions,
        uint32                  regionCount,
        bool                    isFmaskCopyOptimized) const;

    virtual void FixupMetadataForComputeCopyDst(
        GfxCmdBuffer*           pCmdBuffer,
        const Pal::Image&       dstImage,
        ImageLayout             dstImageLayout,
        uint32                  regionCount,
        const ImageFixupRegion* pRegions,
        bool                    beforeCopy,
        const Pal::Image*       pFmaskOptimizedCopySrcImage = nullptr) const override;

    void ResolveImageDepthStencilCopy(
        GfxCmdBuffer*             pCmdBuffer,
        const Pal::Image&         srcImage,
        ImageLayout               srcImageLayout,
        const Pal::Image&         dstImage,
        ImageLayout               dstImageLayout,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions,
        uint32                    flags) const;

    bool HwlCanDoFixedFuncResolve(
        const Pal::Image&         srcImage,
        const Pal::Image&         dstImage,
        ResolveMode               resolveMode,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions) const;

    void HwlFixupResolveDstImage(
        GfxCmdBuffer*             pCmdBuffer,
        const GfxImage&           dstImage,
        ImageLayout               dstImageLayout,
        const ImageResolveRegion* pRegions,
        uint32                    regionCount,
        bool                      computeResolve) const;

    void HwlResolveImageGraphics(
        GfxCmdBuffer*             pCmdBuffer,
        const Pal::Image&         srcImage,
        ImageLayout               srcImageLayout,
        const Pal::Image&         dstImage,
        ImageLayout               dstImageLayout,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions,
        uint32                    flags) const;

    virtual bool NeedPixelCopyForCmdCopyImage(
        const Pal::Image&      srcImage,
        const Pal::Image&      dstImage,
        const ImageCopyRegion* pRegions,
        uint32                 regionCount) const override;

    virtual void HwlImageToImageMissingPixelCopy(
        GfxCmdBuffer*          pCmdBuffer,
        const Pal::Image&      srcImage,
        const Pal::Image&      dstImage,
        const ImageCopyRegion& region) const override;

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

    virtual ImageCopyEngine GetImageToImageCopyEngine(
        const GfxCmdBuffer*    pCmdBuffer,
        const Pal::Image&      srcImage,
        const Pal::Image&      dstImage,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        uint32                 copyFlags) const override;

    virtual bool ScaledCopyImageUseGraphics(
        GfxCmdBuffer*         pCmdBuffer,
        const ScaledCopyInfo& copyInfo) const override;

    virtual bool PreferComputeForNonLocalDestCopy(const Pal::Image& dstImage) const override;

    static void MetaDataDispatch(
        GfxCmdBuffer*       pCmdBuffer,
        const Gfx9MaskRam*  pMaskRam,
        uint32              width,
        uint32              height,
        uint32              depth,
        DispatchDims        threadsPerGroup);

    void CommitBeginEndGfxCopy(
        Pal::CmdStream*  pCmdStream,
        uint32           paScTileSteeringOverride) const;

    Device*const   m_pDevice;
    const CmdUtil& m_cmdUtil;

private:
    void CmdCopyMemoryFromToImageViaPixels(
        GfxCmdBuffer*                 pCmdBuffer,
        const Pal::Image&             image,
        const GpuMemory&              memory,
        const MemoryImageCopyRegion&  region,
        bool                          includePadding,
        bool                          imageIsSrc) const;

    void CmdCopyImageToImageViaPixels(
        GfxCmdBuffer*          pCmdBuffer,
        const Pal::Image&      srcImage,
        const Pal::Image&      dstImage,
        const ImageCopyRegion& region) const;

    static Extent3d GetCopyViaSrdCopyDims(
        const Pal::Image&  image,
        SubresId           subresId,
        bool               includePadding);

    static bool UsePixelCopy(
        const Pal::Image&             image,
        const MemoryImageCopyRegion&  region);

    static bool UsePixelCopyForCmdCopyImage(
        const Pal::Image&      srcImage,
        const Pal::Image&      dstImage,
        const ImageCopyRegion& region);

    static void PreComputeColorClearSync(
        ICmdBuffer*        pCmdBuffer,
        const IImage*      pImage,
        const SubresRange& subres,
        ImageLayout        layout);

    static void PostComputeColorClearSync(
        ICmdBuffer*        pCmdBuffer,
        const IImage*      pImage,
        const SubresRange& subres,
        ImageLayout        layout,
        bool               csFastClear);

    static void PreComputeDepthStencilClearSync(
        ICmdBuffer*        pCmdBuffer,
        const GfxImage&    gfxImage,
        const SubresRange& subres,
        ImageLayout        layout);

    static void PostComputeDepthStencilClearSync(
        ICmdBuffer*        pCmdBuffer,
        const GfxImage&    gfxImage,
        const SubresRange& subres,
        ImageLayout        layout,
        bool               csFastClear);

    void HwlFastColorClear(
        GfxCmdBuffer*         pCmdBuffer,
        const GfxImage&       dstImage,
        const uint32*         pConvertedColor,
        const SwizzledFormat& clearFormat,
        const SubresRange&    clearRange,
        bool                  trackBltActiveFlags) const;

    virtual bool IsAc01ColorClearCode(
        const GfxImage&       dstImage,
        const uint32*         pConvertedColor,
        const SwizzledFormat& clearFormat,
        const SubresRange&    clearRange) const override;

    void HwlDepthStencilClear(
        GfxCmdBuffer*      pCmdBuffer,
        const GfxImage&    dstImage,
        ImageLayout        depthLayout,
        ImageLayout        stencilLayout,
        float              depth,
        uint8              stencil,
        uint8              stencilWriteMask,
        uint32             rangeCount,
        const SubresRange* pRanges,
        bool               fastClear,
        bool               clearAutoSync,
        uint32             boxCnt,
        const Box*         pBox) const;

    virtual uint32 HwlBeginGraphicsCopy(
        GfxCmdBuffer*                pCmdBuffer,
        const Pal::GraphicsPipeline* pPipeline,
        const Pal::Image&            dstImage,
        uint32                       bpp) const override;

    virtual void HwlEndGraphicsCopy(
        GfxCmdStream* pCmdStream,
        uint32        restoreMask) const override;

    void ClearFmask(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        const SubresRange& clearRange,
        uint64             clearValue) const;

    void InitCmask(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       image,
        const SubresRange& range,
        const uint8        initValue,
        bool               trackBltActiveFlags) const;

    void InitHtile(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       dstImage,
        const SubresRange& clearRange) const;

    void ClearHiSPretestsMetaData(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       dstImage,
        const SubresRange& range) const;

    void InitDepthClearMetaData(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       dstImage,
        const SubresRange& range) const;

    void InitColorClearMetaData(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       dstImage,
        const SubresRange& range) const;

    void DepthStencilClearGraphics(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        const SubresRange& range,
        float              depth,
        uint8              stencil,
        uint8              stencilWriteMask,
        uint32             clearMask,
        bool               fastClear,
        ImageLayout        depthLayout,
        ImageLayout        stencilLayout,
        bool               trackBltActiveFlags,
        uint32             boxCnt,
        const Box*         pBox) const;

    uint32* UpdateBoundFastClearColor(
        GfxCmdBuffer*   pCmdBuffer,
        const GfxImage& dstImage,
        uint32          startMip,
        uint32          numMips,
        const uint32    color[4],
        CmdStream*      pStream,
        uint32*         pCmdSpace) const;

    void UpdateBoundFastClearDepthStencil(
        GfxCmdBuffer*      pCmdBuffer,
        const GfxImage&    dstImage,
        const SubresRange& range,
        uint32             metaDataClearFlags,
        float              depth,
        uint8              stencil) const;

    void DccDecompressOnCompute(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       image,
        const SubresRange& range) const;

    void CmdResolveQueryComputeShader(
        GfxCmdBuffer*         pCmdBuffer,
        const Pal::QueryPool& queryPool,
        QueryResultFlags      flags,
        QueryType             queryType,
        uint32                startQuery,
        uint32                queryCount,
        const GpuMemory&      dstGpuMemory,
        gpusize               dstOffset,
        gpusize               dstStride) const;

    const SPI_SHADER_EX_FORMAT DeterminePsExportFmt(
        SwizzledFormat format,
        bool           blendEnabled,
        bool           shaderExportsAlpha,
        bool           blendSrcAlphaToColor,
        bool           enableAlphaToCoverage) const;

    void PfpCopyMetadataHeader(
        GfxCmdBuffer* pCmdBuffer,
        gpusize       dstAddr,
        gpusize       srcAddr,
        uint32        size,
        bool          hasDccLookupTable) const;

    void LaunchOptimizedVrsCopyShader(
        GfxCmdBuffer*           pCmdBuffer,
        const DepthStencilView* pDsView,
        bool                    isClientDsv,
        const Extent3d&         depthExtent,
        const Pal::Image*       pSrcVrsImg,
        const Gfx9Htile*const   pHtile) const;

    void InitHtileData(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        const SubresRange& range,
        uint32             hTileValue,
        uint32             hTileMask) const;

    void WriteHtileData(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        const SubresRange& range,
        uint32             hTileValue,
        uint32             hTileMask,
        uint8              stencil,
        bool               trackBltActiveFlags) const;

    void ClearDccComputeSetFirstPixelOfBlock(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        uint32             plane,
        uint32             absMipLevel,
        uint32             startSlice,
        uint32             numSlices,
        uint32             bytesPerPixel,
        const uint32*      pPackedClearColor) const;

    bool HwlCanDoDepthStencilCopyResolve(
        const Pal::Image&         srcImage,
        const Pal::Image&         dstImage,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions) const;

    void HwlHtileCopyAndFixUp(
        GfxCmdBuffer*             pCmdBuffer,
        const Pal::Image&         srcImage,
        const Pal::Image&         dstImage,
        ImageLayout               dstImageLayout,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions,
        bool                      computeResolve) const;

    PAL_DISALLOW_DEFAULT_CTOR(RsrcProcMgr);
    PAL_DISALLOW_COPY_AND_ASSIGN(RsrcProcMgr);
};

} // Gfx9
} // Pal
