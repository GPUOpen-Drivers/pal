/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/rpm/pm4RsrcProcMgr.h"

namespace Pal
{

class  GfxCmdBuffer;
class  QueryPool;
struct DepthStencilViewCreateInfo;

namespace Gfx9
{

class      CmdUtil;
class      Device;
class      Gfx9Htile;
class      Gfx9MaskRam;
class      Image;
struct     SyncReqs;
enum class DccClearPurpose : uint32;

// =====================================================================================================================
// GFX9+10 common hardware layer implementation of the Resource Processing Manager. It is most known for handling
// GFX9+10-specific resource operations like DCC decompression.
class RsrcProcMgr : public Pm4::RsrcProcMgr
{
public:

    static constexpr bool ForceGraphicsFillMemoryPath = false;

    virtual Result LateInit() override;
    virtual void Cleanup() override;

    void CmdCloneImageData(
        GfxCmdBuffer* pCmdBuffer,
        const Image&  srcImage,
        const Image&  dstImage) const;

    void CmdUpdateMemory(
        GfxCmdBuffer*    pCmdBuffer,
        const GpuMemory& dstMem,
        gpusize          dstOffset,
        gpusize          dataSize,
        const uint32*    pData) const;

    void CmdResolveQuery(
        GfxCmdBuffer*         pCmdBuffer,
        const Pal::QueryPool& queryPool,
        QueryResultFlags      flags,
        QueryType             queryType,
        uint32                startQuery,
        uint32                queryCount,
        const GpuMemory&      dstGpuMemory,
        gpusize               dstOffset,
        gpusize               dstStride) const;

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

    void BuildHtileLookupTable(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        const SubresRange& range) const;

    virtual void BuildDccLookupTable(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       srcImage,
        const SubresRange& range) const {};

    void FastClearEliminate(
        GfxCmdBuffer*                pCmdBuffer,
        Pal::CmdStream*              pCmdStream,
        const Image&                 image,
        const MsaaQuadSamplePattern* pQuadSamplePattern,
        const SubresRange&           range) const;

    virtual bool ExpandDepthStencil(
        Pm4CmdBuffer*                pCmdBuffer,
        const Pal::Image&            image,
        const MsaaQuadSamplePattern* pQuadSamplePattern,
        const SubresRange&           range) const override;

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

    bool WillDecompressWithCompute(
        const GfxCmdBuffer* pCmdBuffer,
        const Image&        gfxImage,
        const SubresRange&  range) const;

    void EchoGlobalInternalTableAddr(
        GfxCmdBuffer* pCmdBuffer,
        gpusize       dstAddr) const;

protected:
    explicit RsrcProcMgr(Device* pDevice);
    virtual ~RsrcProcMgr();

    uint32  GetInitHtileClearMask(
        const Image&       dstImage,
        const SubresRange& clearRange) const;

    virtual bool CopyDstBoundStencilNeedsWa(
        const GfxCmdBuffer* pCmdBuffer,
        const Pal::Image&   dstImage) const override;

    virtual const Pal::GraphicsPipeline* GetGfxPipelineByTargetIndexAndFormat(
        RpmGfxPipeline basePipeline,
        uint32         targetIndex,
        SwizzledFormat format) const override;

    virtual const bool IsGfxPipelineForFormatSupported(
        SwizzledFormat format) const override;

    virtual bool CopyImageUseMipLevelInSrd(bool isCompressed) const override
        { return (RsrcProcMgr::UseMipLevelInSrd && (isCompressed == false)); }

    bool ClearDcc(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       dstImage,
        const SubresRange& clearRange,
        uint8              clearCode,
        DccClearPurpose    clearPurpose,
        const uint32*      pPackedClearColor = nullptr) const;

    virtual void ClearDccCompute(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       dstImage,
        const SubresRange& clearRange,
        uint8              clearCode,
        DccClearPurpose    clearPurpose,
        const uint32*      pPackedClearColor = nullptr) const = 0;

    uint32 DecodeImageViewSrdPlane(
        const Pal::Image&  image,
        gpusize            srdBaseAddr,
        uint32             slice) const;

    virtual void FastDepthStencilClearCompute(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        const SubresRange& range,
        uint32             htileValue,
        uint32             clearMask,
        uint8              stencil) const = 0;

    void FastDepthStencilClearComputeCommon(
        GfxCmdBuffer*      pCmdBuffer,
        const Pal::Image*  pPalImage,
        uint32             clearMask) const;

    uint32 GetClearDepth(
        const Image& dstImage,
        uint32       plane,
        uint32       numSlices,
        uint32       mipLevel) const;

    virtual bool HwlUseOptimizedImageCopy(
        const Pal::Image&      srcImage,
        ImageLayout            srcImageLayout,
        const Pal::Image&      dstImage,
        ImageLayout            dstImageLayout,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions) const override;

    // On gfx9/gfx10, we need to use single z range for single subresource view.
    virtual bool HwlNeedSinglezRangeAccess() const override { return true; }

    virtual void HwlFixupCopyDstImageMetaData(
        GfxCmdBuffer*           pCmdBuffer,
        const Pal::Image*       pSrcImage,
        const Pal::Image&       dstImage,
        ImageLayout             dstImageLayout,
        const ImageFixupRegion* pRegions,
        uint32                  regionCount,
        bool                    isFmaskCopyOptimized) const override;

    virtual void HwlFixupResolveDstImage(
        Pm4CmdBuffer*             pCmdBuffer,
        const GfxImage&           dstImage,
        ImageLayout               dstImageLayout,
        const ImageResolveRegion* pRegions,
        uint32                    regionCount,
        bool                      computeResolve
        ) const override;

#if PAL_BUILD_GFX11
    virtual void HwlResolveImageGraphics(
        GfxCmdBuffer*             pCmdBuffer,
        const Pal::Image&         srcImage,
        ImageLayout               srcImageLayout,
        const Pal::Image&         dstImage,
        ImageLayout               dstImageLayout,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions,
        uint32                    flags) const override;
#endif

    virtual void HwlImageToImageMissingPixelCopy(
        GfxCmdBuffer*          pCmdBuffer,
        const Pal::Image&      srcImage,
        const Pal::Image&      dstImage,
        const ImageCopyRegion& region) const override;

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

    virtual void PreComputeDepthStencilClearSync(
        ICmdBuffer*        pCmdBuffer,
        const GfxImage&    gfxImage,
        const SubresRange& subres,
        ImageLayout        layout) const override;

    Device*const   m_pDevice;
    const CmdUtil& m_cmdUtil;

private:
    void CmdCopyMemoryFromToImageViaPixels(
        Pm4CmdBuffer*                 pCmdBuffer,
        const Pal::Image&             image,
        const GpuMemory&              memory,
        const MemoryImageCopyRegion&  region,
        bool                          includePadding,
        bool                          imageIsSrc) const;

    void CmdCopyImageToImageViaPixels(
        Pm4CmdBuffer*          pCmdBuffer,
        const Pal::Image&      srcImage,
        const Pal::Image&      dstImage,
        const ImageCopyRegion& region) const;

    static Extent3d GetCopyViaSrdCopyDims(
        const Pal::Image&  image,
        const SubresId&    subResId,
        bool               includePadding);

    static bool UsePixelCopy(
        const Pal::Image&             image,
        const MemoryImageCopyRegion&  region);

    static bool UsePixelCopyForCmdCopyImage(
        const Pal::Image&      srcImage,
        const Pal::Image&      dstImage,
        const ImageCopyRegion& region);

    virtual void HwlFastColorClear(
        Pm4CmdBuffer*         pCmdBuffer,
        const GfxImage&       dstImage,
        const uint32*         pConvertedColor,
        const SwizzledFormat& clearFormat,
        const SubresRange&    clearRange) const override;

    virtual void HwlDepthStencilClear(
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
        bool               needComputeSync,
        uint32             boxCnt,
        const Box*         pBox) const override;

    virtual bool HwlCanDoFixedFuncResolve(
        const Pal::Image&         srcImage,
        const Pal::Image&         dstImage,
        ResolveMode               resolveMode,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions) const override;

    virtual bool HwlCanDoDepthStencilCopyResolve(
        const Pal::Image&         srcImage,
        const Pal::Image&         dstImage,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions) const override;

    void ClearFmask(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        const SubresRange& clearRange,
        uint64             clearValue) const;

    virtual void InitCmask(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       image,
        const SubresRange& range,
        const uint8        initValue) const = 0;

    virtual void InitHtile(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       dstImage,
        const SubresRange& clearRange) const = 0;

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

    Pal::ComputePipeline* m_pEchoGlobalTablePipeline;

    PAL_DISALLOW_DEFAULT_CTOR(RsrcProcMgr);
    PAL_DISALLOW_COPY_AND_ASSIGN(RsrcProcMgr);
};

// =====================================================================================================================
// GFX9 specific implementation of RPM.
class Gfx9RsrcProcMgr final : public Pal::Gfx9::RsrcProcMgr
{
public:
    explicit Gfx9RsrcProcMgr(Device* pDevice) : Pal::Gfx9::RsrcProcMgr(pDevice) {}
    virtual ~Gfx9RsrcProcMgr() {}

    void HwlResummarizeHtileCompute(
        Pm4CmdBuffer*      pCmdBuffer,
        const GfxImage&    image,
        const SubresRange& range) const override;

    virtual void CmdCopyMemory(
        Pm4CmdBuffer*           pCmdBuffer,
        const GpuMemory&        srcGpuMemory,
        const GpuMemory&        dstGpuMemory,
        uint32                  regionCount,
        const MemoryCopyRegion* pRegions) const override;

protected:
    void ClearDccCompute(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       dstImage,
        const SubresRange& clearRange,
        uint8              clearCode,
        DccClearPurpose    clearPurpose,
        const uint32*      pPackedClearColor = nullptr) const override;

    void FastDepthStencilClearCompute(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        const SubresRange& range,
        uint32             htileValue,
        uint32             clearMask,
        uint8              stencil) const override;

    virtual void CopyImageCompute(
        GfxCmdBuffer*          pCmdBuffer,
        const Pal::Image&      srcImage,
        ImageLayout            srcImageLayout,
        const Pal::Image&      dstImage,
        ImageLayout            dstImageLayout,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        uint32                 flags) const override;

    virtual void CopyMemoryCs(
        GfxCmdBuffer*           pCmdBuffer,
        const GpuMemory&        srcGpuMemory,
        const GpuMemory&        dstGpuMemory,
        uint32                  regionCount,
        const MemoryCopyRegion* pRegions) const override;

    virtual ImageCopyEngine GetImageToImageCopyEngine(
        const GfxCmdBuffer*    pCmdBuffer,
        const Pal::Image&      srcImage,
        const Pal::Image&      dstImage,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        uint32                 copyFlags) const override;

    virtual const Pal::ComputePipeline* GetCmdGenerationPipeline(
        const Pm4::IndirectCmdGenerator& generator,
        const Pm4CmdBuffer&              cmdBuffer) const override;

    void HwlDecodeBufferViewSrd(
        const void*     pBufferViewSrd,
        BufferViewInfo* pViewInfo) const override;

    void HwlDecodeImageViewSrd(
        const void*       pImageViewSrd,
        const Pal::Image& dstImage,
        SwizzledFormat*   pSwizzledFormat,
        SubresRange*      pSubresRange) const override;

    void InitCmask(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       image,
        const SubresRange& range,
        const uint8        initValue) const override;

    void InitHtile(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       dstImage,
        const SubresRange& clearRange) const override;

private:
    void ClearHtileAllBytes(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        const SubresRange& range,
        uint32             htileValue) const;

    void ClearHtileSelectedBytes(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        const SubresRange& range,
        uint32             htileValue,
        uint32             htileMask) const;

    void DoFastClear(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       dstImage,
        const SubresRange& clearRange,
        uint8              clearCode,
        DccClearPurpose    clearPurpose) const;

    void DoOptimizedCmaskInit(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       image,
        const SubresRange& range,
        uint8              initValue
        ) const;

    void DoOptimizedFastClear(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       dstImage,
        const SubresRange& clearRange,
        uint8              clearCode,
        DccClearPurpose    clearPurpose) const;

    void DoOptimizedHtileFastClear(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        const SubresRange& range,
        uint32             htileValue,
        uint32             htileMask) const;

    void ExecuteHtileEquation(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        const SubresRange& range,
        uint32             htileValue,
        uint32             htileMask) const;

    virtual void CopyBetweenMemoryAndImage(
        GfxCmdBuffer*                pCmdBuffer,
        const Pal::ComputePipeline*  pPipeline,
        const GpuMemory&             gpuMemory,
        const Pal::Image&            image,
        ImageLayout                  imageLayout,
        bool                         isImageDst,
        bool                         isFmaskCopy,
        uint32                       regionCount,
        const MemoryImageCopyRegion* pRegions,
        bool                         includePadding) const override;

    void HwlHtileCopyAndFixUp(
        Pm4CmdBuffer*             pCmdBuffer,
        const Pal::Image&         srcImage,
        const Pal::Image&         dstImage,
        ImageLayout               dstImageLayout,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions,
        bool                      computeResolve) const override;

    virtual void HwlFixupCopyDstImageMetaData(
        GfxCmdBuffer*           pCmdBuffer,
        const Pal::Image*       pSrcImage,
        const Pal::Image&       dstImage,
        ImageLayout             dstImageLayout,
        const ImageFixupRegion* pRegions,
        uint32                  regionCount,
        bool                    isFmaskCopyOptimized) const override;

    virtual uint32 HwlBeginGraphicsCopy(
        Pm4CmdBuffer*                pCmdBuffer,
        const Pal::GraphicsPipeline* pPipeline,
        const Pal::Image&            dstImage,
        uint32                       bpp) const override;

    virtual void HwlEndGraphicsCopy(
        Pm4::CmdStream* pCmdStream,
        uint32          restoreMask) const override;

    PAL_DISALLOW_DEFAULT_CTOR(Gfx9RsrcProcMgr);
    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx9RsrcProcMgr);
};

class Gfx10DepthStencilView;

// =====================================================================================================================
// GFX10 specific implementation of RPM.
class Gfx10RsrcProcMgr final : public Pal::Gfx9::RsrcProcMgr
{
public:
    explicit Gfx10RsrcProcMgr(Device* pDevice) : Pal::Gfx9::RsrcProcMgr(pDevice) {}
    virtual ~Gfx10RsrcProcMgr() {}

    void BuildDccLookupTable(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       srcImage,
        const SubresRange& range) const override;

    virtual void HwlResummarizeHtileCompute(
        Pm4CmdBuffer*      pCmdBuffer,
        const GfxImage&    image,
        const SubresRange& range) const override;

    void CmdResolvePrtPlusImage(
        GfxCmdBuffer*                    pCmdBuffer,
        const IImage&                    srcImage,
        ImageLayout                      srcImageLayout,
        const IImage&                    dstImage,
        ImageLayout                      dstImageLayout,
        PrtPlusResolveType               resolveType,
        uint32                           regionCount,
        const PrtPlusImageResolveRegion* pRegions) const override;

    void LaunchOptimizedVrsCopyShader(
        GfxCmdBuffer*                  pCmdBuffer,
        const Gfx10DepthStencilView*   pDsView,
        const Extent3d&                depthExtent,
        const Pal::Image*              pSrcVrsImg,
        const Gfx9Htile*const          pHtile) const;

    void CopyVrsIntoHtile(
        GfxCmdBuffer*                pCmdBuffer,        // cmd buffer to receive copy commands, must support compute
        const Gfx10DepthStencilView* pDsView,           // depth view that contains image that owns dest hTile buffer
        const Extent3d&              depthExtent,       // extent of the depth buffers' mip level
        const Pal::Image*            pSrcVrsImg) const; // source VRS data (can be NULL to imply 1x1)

protected:
    virtual void ClearDccCompute(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       dstImage,
        const SubresRange& clearRange,
        uint8              clearCode,
        DccClearPurpose    clearPurpose,
        const uint32*      pPackedClearColor = nullptr) const override;

    virtual void FastDepthStencilClearCompute(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        const SubresRange& range,
        uint32             htileValue,
        uint32             clearMask,
        uint8              stencil) const override;

    virtual const Pal::ComputePipeline* GetCmdGenerationPipeline(
        const Pm4::IndirectCmdGenerator& generator,
        const Pm4CmdBuffer&              cmdBuffer) const override;

    virtual void HwlDecodeBufferViewSrd(
        const void*     pBufferViewSrd,
        BufferViewInfo* pViewInfo) const override;

    virtual void HwlDecodeImageViewSrd(
        const void*       pImageViewSrd,
        const Pal::Image& dstImage,
        SwizzledFormat*   pSwizzledFormat,
        SubresRange*      pSubresRange) const override;

    virtual void InitCmask(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       image,
        const SubresRange& range,
        const uint8        initValue) const override;

    virtual void InitHtile(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       dstImage,
        const SubresRange& clearRange) const override;

    virtual ImageCopyEngine GetImageToImageCopyEngine(
        const GfxCmdBuffer*    pCmdBuffer,
        const Pal::Image&      srcImage,
        const Pal::Image&      dstImage,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        uint32                 copyFlags) const override;

    virtual bool PreferComputeForNonLocalDestCopy(const Pal::Image& dstImage) const override;
    virtual bool CopyImageCsUseMsaaMorton(const Pal::Image& dstImage) const override;

private:
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
        uint8              stencil) const;

    void ClearDccComputeSetFirstPixelOfBlock(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        uint32             plane,
        uint32             absMipLevel,
        uint32             startSlice,
        uint32             numSlices,
        uint32             bytesPerPixel,
        const uint32*      pPackedClearColor) const;

    virtual bool HwlCanDoDepthStencilCopyResolve(
        const Pal::Image&         srcImage,
        const Pal::Image&         dstImage,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions) const override;

    virtual void HwlHtileCopyAndFixUp(
        Pm4CmdBuffer*             pCmdBuffer,
        const Pal::Image&         srcImage,
        const Pal::Image&         dstImage,
        ImageLayout               dstImageLayout,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions,
        bool                      computeResolve) const override;

    virtual void HwlFixupCopyDstImageMetaData(
        GfxCmdBuffer*           pCmdBuffer,
        const Pal::Image*       pSrcImage,
        const Pal::Image&       dstImage,
        ImageLayout             dstImageLayout,
        const ImageFixupRegion* pRegions,
        uint32                  regionCount,
        bool                    isFmaskCopyOptimized) const override;

    virtual uint32 HwlBeginGraphicsCopy(
        Pm4CmdBuffer*                pCmdBuffer,
        const Pal::GraphicsPipeline* pPipeline,
        const Pal::Image&            dstImage,
        uint32                       bpp) const override;

    virtual void HwlEndGraphicsCopy(
        Pm4::CmdStream* pCmdStream,
        uint32          restoreMask) const override;

    virtual void HwlGfxDccToDisplayDcc(
        GfxCmdBuffer*     pCmdBuffer,
        const Pal::Image& image) const override;

    virtual void InitDisplayDcc(
        GfxCmdBuffer*      pCmdBuffer,
        const Pal::Image&  image) const override;

    PAL_DISALLOW_DEFAULT_CTOR(Gfx10RsrcProcMgr);
    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx10RsrcProcMgr);
};

} // Gfx9
} // Pal
