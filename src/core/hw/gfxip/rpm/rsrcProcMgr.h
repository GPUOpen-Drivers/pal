/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/rpm/g_rpmComputePipelineInit.h"
#include "core/hw/gfxip/rpm/g_rpmGfxPipelineInit.h"
#include "palCmdBuffer.h"

namespace Pal
{

class  CmdStream;
class  ColorBlendState;
enum   CopyImageFlags : uint32;
class  DepthStencilState;
class  GfxCmdBuffer;
class  GfxImage;
class  GpuMemory;
class  Image;
class  IndirectCmdGenerator;
class  Pipeline;
struct ImageCopyRegion;
struct ImageResolveRegion;
struct MemoryCopyRegion;
struct MemoryImageCopyRegion;
class  MsaaState;
union  ScaledCopyInternalFlags;

static constexpr uint32 MaxLog2AaSamples   = 4; // Max AA sample rate is 16x.
static constexpr uint32 MaxLog2AaFragments = 3; // Max fragments is 8.

// =====================================================================================================================
// Resource Processing Manager: Contains resource modification and preparation logic. RPM and its subclasses issue
// draws, dispatches, and other operations to manipulate resource contents and hardware state.
class RsrcProcMgr
{
public:
    static constexpr bool UseMipLevelInSrd = true;
    static constexpr bool OptimizeLinearDestGraphicsCopy = true;

    explicit RsrcProcMgr(GfxDevice* pDevice);
    virtual ~RsrcProcMgr();

    Result EarlyInit();
    Result LateInit();
    void Cleanup();

    void CmdCopyImage(
        GfxCmdBuffer*          pCmdBuffer,
        const Image&           srcImage,
        ImageLayout            srcImageLayout,
        const Image&           dstImage,
        ImageLayout            dstImageLayout,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        uint32                 flags) const;

    void CmdCopyMemoryToImage(
        GfxCmdBuffer*                pCmdBuffer,
        const GpuMemory&             srcGpuMemory,
        const Image&                 dstImage,
        ImageLayout                  dstImageLayout,
        uint32                       regionCount,
        const MemoryImageCopyRegion* pRegions,
        bool                         includePadding) const;

    void CmdCopyImageToMemory(
        GfxCmdBuffer*                pCmdBuffer,
        const Image&                 srcImage,
        ImageLayout                  srcImageLayout,
        const GpuMemory&             dstGpuMemory,
        uint32                       regionCount,
        const MemoryImageCopyRegion* pRegions,
        bool                         includePadding) const;

    void CmdCopyTypedBuffer(
        GfxCmdBuffer*                pCmdBuffer,
        const GpuMemory&             srcGpuMemory,
        const GpuMemory&             dstGpuMemory,
        uint32                       regionCount,
        const TypedBufferCopyRegion* pRegions) const;

    virtual void CmdScaledCopyImage(
        GfxCmdBuffer*           pCmdBuffer,
        const ScaledCopyInfo&   copyInfo,
        ScaledCopyInternalFlags flags) const;

    void CmdColorSpaceConversionCopy(
        GfxCmdBuffer*                     pCmdBuffer,
        const Image&                      srcImage,
        ImageLayout                       srcImageLayout,
        const Image&                      dstImage,
        ImageLayout                       dstImageLayout,
        uint32                            regionCount,
        const ColorSpaceConversionRegion* pRegions,
        TexFilter                         filter,
        const ColorSpaceConversionTable&  cscTable) const;

    virtual void CmdFillMemory(
        GfxCmdBuffer*    pCmdBuffer,
        bool             saveRestoreComputeState,
        const GpuMemory& dstGpuMemory,
        gpusize          dstOffset,
        gpusize          fillSize,
        uint32           data) const;

    void CmdClearBoundDepthStencilTargets(
        GfxCmdBuffer*                   pCmdBuffer,
        float                           depth,
        uint8                           stencil,
        uint32                          samples,
        uint32                          fragments,
        DepthStencilSelectFlags         flag,
        uint32                          regionCount,
        const ClearBoundTargetRegion*   pClearRegions) const;

    void CmdClearDepthStencil(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        ImageLayout        depthLayout,
        ImageLayout        stencilLayout,
        float              depth,
        uint8              stencil,
        uint32             rangeCount,
        const SubresRange* pRanges,
        uint32             rectCount,
        const Rect*        pRects,
        uint32             flags) const;

    void CmdClearColorBuffer(
        GfxCmdBuffer*     pCmdBuffer,
        const IGpuMemory& dstGpuMemory,
        const ClearColor& color,
        SwizzledFormat    bufferFormat,
        uint32            bufferOffset,
        uint32            bufferExtent,
        uint32            rangeCount = 0,
        const Range*      pRanges    = nullptr) const;

    void CmdClearBoundColorTargets(
        GfxCmdBuffer*                   pCmdBuffer,
        uint32                          colorTargetCount,
        const BoundColorTarget*         pBoundColorTargets,
        uint32                          regionCount,
        const ClearBoundTargetRegion*   pBoxes) const;

    void CmdClearColorImage(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        ImageLayout        dstImageLayout,
        const ClearColor&  color,
        uint32             rangeCount,
        const SubresRange* pRanges,
        uint32             boxCount,
        const Box*         pBoxes,
        uint32             flags) const;

    virtual void CmdClearBufferView(
        GfxCmdBuffer*     pCmdBuffer,
        const IGpuMemory& dstGpuMemory,
        const ClearColor& color,
        const void*       pBufferViewSrd,
        uint32            rangeCount = 0,
        const Range*      pRanges    = nullptr) const;

    virtual void CmdClearImageView(
        GfxCmdBuffer*     pCmdBuffer,
        const Image&      dstImage,
        ImageLayout       dstImageLayout,
        const ClearColor& color,
        const void*       pImageViewSrd,
        uint32            rectCount = 0,
        const Rect*       pRects    = nullptr) const;

    void CmdResolveImage(
        GfxCmdBuffer*             pCmdBuffer,
        const Image&              srcImage,
        ImageLayout               srcImageLayout,
        const Image&              dstImage,
        ImageLayout               dstImageLayout,
        ResolveMode               resolveMode,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions) const;

    void CmdGenerateIndirectCmds(
        GfxCmdBuffer*               pCmdBuffer,
        const Pipeline*             pPipeline,
        const IndirectCmdGenerator& generator,
        gpusize                     argsGpuAddr,
        gpusize                     countGpuAddr,
        uint32                      indexBufSize,
        uint32                      maximumCount
        ) const;

    virtual void ExpandDepthStencil(
        GfxCmdBuffer*        pCmdBuffer,
        const Image&         image,
        const IMsaaState*    pMsaaState,
        const MsaaQuadSamplePattern* pQuadSamplePattern,
        const SubresRange&   range
    ) const;

    void ResummarizeDepthStencil(
        GfxCmdBuffer*        pCmdBuffer,
        const Image&         image,
        ImageLayout          imageLayout,
        const IMsaaState*    pMsaaState,
        const MsaaQuadSamplePattern* pQuadSamplePattern,
        const SubresRange&   range
    ) const;

    virtual void HwlExpandHtileHiZRange(
        GfxCmdBuffer*      pCmdBuffer,
        const GfxImage&    image,
        const SubresRange& range) const = 0;

    void CopyImageToPackedPixelImage(
        GfxCmdBuffer*          pCmdBuffer,
        const Image&           srcImage,
        const Image&           dstImage,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        Pal::PackedPixelType   packPixelType) const;

protected:
    // When constructing SRD tables, all SRDs must be size and offset aligned to this many DWORDs.
    uint32 SrdDwordAlignment() const { return m_srdAlignment; }

    virtual bool CopyImageUseMipLevelInSrd(bool isCompressed) const { return UseMipLevelInSrd; }

    const ComputePipeline* GetLinearHtileClearPipeline(
        bool    expClearEnable,
        bool    tileStencilDisabled,
        uint32  hTileMask) const;

    // Some blts need to use GFXIP-specific algorithms to pick the proper state. The baseState is the first
    // graphics state in a series of states that vary only on target format and target index.
    virtual const GraphicsPipeline* GetGfxPipelineByTargetIndexAndFormat(
        RpmGfxPipeline basePipeline,
        uint32         targetIndex,
        SwizzledFormat format) const = 0;

    // Generating indirect commands needs to choose different shaders based on the GFXIP version.
    virtual const ComputePipeline* GetCmdGenerationPipeline(
        const IndirectCmdGenerator& generator,
        const CmdBuffer&            cmdBuffer) const = 0;

    const ComputePipeline* GetPipeline(RpmComputePipeline pipeline) const
        { return m_pComputePipelines[static_cast<size_t>(pipeline)]; }

    const GraphicsPipeline* GetGfxPipeline(RpmGfxPipeline pipeline) const
        { return m_pGraphicsPipelines[pipeline]; }

    const MsaaState* GetMsaaState(uint32 samples, uint32 fragments) const;

    const GraphicsPipeline* GetCopyDepthStencilMsaaPipeline(bool isDepth,
                                                            bool isDepthStencil,
                                                            uint32 numSamples) const;

    void GenericColorBlit(
        GfxCmdBuffer*        pCmdBuffer,
        const Image&         dstImage,
        const SubresRange&   range,
        const IMsaaState&    msaaState,
        const MsaaQuadSamplePattern* pQuadSamplePattern,
        RpmGfxPipeline       pipeline,
        const GpuMemory*     pGpuMemory,
        gpusize              metaDataOffset
    ) const;

    static bool GetMetaDataTexFetchSupport(const Image*  pImage, ImageAspect aspect, uint32 mipLevel);

    static gpusize ComputeTypedBufferRange(
        const Extent3d& extent,
        uint32          elementSize,
        gpusize         rowPitch,
        gpusize         depthPitch);

    void SlowClearGraphics(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        ImageLayout        dstImageLayout,
        const ClearColor*  pColor,
        const SubresRange& clearRange,
        uint32             boxCount,
        const Box*         pBoxes) const;

    void SlowClearCompute(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        ImageLayout        dstImageLayout,
        SwizzledFormat     dstFormat,
        const ClearColor*  pColor,
        const SubresRange& clearRange,
        uint32             boxCount,
        const Box*         pBoxes) const;

    void CopyMemoryCs(
        GfxCmdBuffer*           pCmdBuffer,
        const GpuMemory&        srcGpuMemory,
        const GpuMemory&        dstGpuMemory,
        uint32                  regionCount,
        const MemoryCopyRegion* pRegions) const;

    const ComputePipeline* GetComputeMaskRamExpandPipeline(
        const Image& image) const;

    ColorBlendState*    m_pBlendDisableState;               // Blend state object with all blending disabled.
    DepthStencilState*  m_pDepthDisableState;               // DS state object with all depth disabled.
    DepthStencilState*  m_pDepthClearState;                 // Ds state object for depth-only clears.
    DepthStencilState*  m_pStencilClearState;               // Ds state object for stencil-only clears.
    DepthStencilState*  m_pDepthStencilClearState;          // Ds state object for depth & stencil clears.
    DepthStencilState*  m_pDepthExpandState;                // DS state object for expand.
    DepthStencilState*  m_pDepthResummarizeState;           // DS state object for resummarization.
    DepthStencilState*  m_pDepthResolveState;               // DS state object for depth resolves.
    DepthStencilState*  m_pStencilResolveState;             // DS state object for stencil resolves.
    MsaaState*          m_pMsaaState[MaxLog2AaSamples + 1]
                                    [MaxLog2AaFragments + 1]; // MSAA state objects for all AA sample rates.
    DepthStencilState*  m_pDepthStencilResolveState;        // DS state object for depth/stencil resolves.

private:
    virtual Result CreateCommonStateObjects();

    virtual void HwlFastColorClear(
        GfxCmdBuffer*      pCmdBuffer,
        const GfxImage&    dstImage,
        const uint32*      pConvertedColor,
        const SubresRange& clearRange) const = 0;

    virtual void HwlUpdateDstImageMetaData(
        GfxCmdBuffer*          pCmdBuffer,
        const Pal::Image&      srcImage,
        const Pal::Image&      dstImage,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        uint32                 flags) const = 0;

    virtual void HwlHtileCopyAndFixUp(
        GfxCmdBuffer*             pCmdBuffer,
        const Pal::Image&         srcImage,
        const Pal::Image&         dstImage,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions) const = 0;

    virtual void HwlDepthStencilClear(
        GfxCmdBuffer*      pCmdBuffer,
        const GfxImage&    dstImage,
        ImageLayout        depthLayout,
        ImageLayout        stencilLayout,
        float              depth,
        uint8              stencil,
        uint32             rangeCount,
        const SubresRange* pRanges,
        bool               fastClear,
        bool               needComputeSync,
        uint32             boxCnt,
        const Box*         pBox) const = 0;

    // The next two functions should be called before and after a graphics copy. They give the gfxip layer a chance
    // to optimize the hardware for the copy operation and restore to the previous state once the copy is done.
    virtual uint32 HwlBeginGraphicsCopy(
        GfxCmdBuffer*           pCmdBuffer,
        const GraphicsPipeline* pPipeline,
        const Image&            dstImage,
        uint32                  bpp) const = 0;

    virtual void HwlEndGraphicsCopy(CmdStream* pCmdStream, uint32 restoreMask) const = 0;

    virtual void HwlDecodeImageViewSrd(
        const void*     pImageViewSrd,
        const Image&    dstImage,
        SwizzledFormat* pSwizzledFormat,
        SubresRange*    pSubresRange) const = 0;

    virtual void HwlDecodeBufferViewSrd(
        const void*     pBufferViewSrd,
        BufferViewInfo* pViewInfo) const = 0;

    virtual bool HwlCanDoFixedFuncResolve(
        const Pal::Image&         srcImage,
        const Pal::Image&         dstImage,
        ResolveMode               resolveMode,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions) const = 0;

    virtual bool HwlCanDoDepthStencilCopyResolve(
        const Pal::Image&         srcImage,
        const Pal::Image&         dstImage,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions) const = 0;

    void CopyColorImageGraphics(
        GfxCmdBuffer*          pCmdBuffer,
        const Image&           srcImage,
        ImageLayout            srcImageLayout,
        const Image&           dstImage,
        ImageLayout            dstImageLayout,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        uint32                 flags) const;

    void CopyDepthStencilImageGraphics(
        GfxCmdBuffer*          pCmdBuffer,
        const Image&           srcImage,
        ImageLayout            srcImageLayout,
        const Image&           dstImage,
        ImageLayout            dstImageLayout,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        uint32                 flags) const;

    void CopyImageCompute(
        GfxCmdBuffer*          pCmdBuffer,
        const Image&           srcImage,
        ImageLayout            srcImageLayout,
        const Image&           dstImage,
        ImageLayout            dstImageLayout,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        uint32                 flags) const;

    template <typename CopyRegion>
    void GetCopyImageFormats(
        const Image&           srcImage,
        ImageLayout            srcImageLayout,
        const Image&           dstImage,
        ImageLayout            dstImageLayout,
        const CopyRegion&      copyRegion,
        uint32                 copyFlags,
        SwizzledFormat*        pSrcFormat,
        SwizzledFormat*        pDstFormat,
        uint32*                pTexelScale) const;

    void CopyBetweenMemoryAndImage(
        GfxCmdBuffer*                pCmdBuffer,
        const ComputePipeline*       pPipeline,
        const GpuMemory&             gpuMemory,
        const Image&                 image,
        ImageLayout                  imageLayout,
        bool                         isImageDst,
        uint32                       regionCount,
        const MemoryImageCopyRegion* pRegions,
        bool                         includePadding) const;

    void ResolveImageGraphics(
        GfxCmdBuffer*             pCmdBuffer,
        const Image&              srcImage,
        ImageLayout               srcImageLayout,
        const Image&              dstImage,
        ImageLayout               dstImageLayout,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions) const;

    void ResolveImageCompute(
        GfxCmdBuffer*             pCmdBuffer,
        const Image&              srcImage,
        ImageLayout               srcImageLayout,
        const Image&              dstImage,
        ImageLayout               dstImageLayout,
        ResolveMode               resolveMode,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions,
        ResolveMethod             method) const;

    void ResolveImageFixedFunc(
        GfxCmdBuffer*             pCmdBuffer,
        const Image&              srcImage,
        ImageLayout               srcImageLayout,
        const Image&              dstImage,
        ImageLayout               dstImageLayout,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions) const;

    void ResolveImageDepthStencilCopy(
        GfxCmdBuffer*             pCmdBuffer,
        const Image&              srcImage,
        ImageLayout               srcImageLayout,
        const Image&              dstImage,
        ImageLayout               dstImageLayout,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions) const;

    void SlowClearGraphicsOneMip(
        GfxCmdBuffer*              pCmdBuffer,
        const Image&               dstImage,
        const SubresRange&         clearRange,
        uint32                     boxCount,
        const Box*                 pBoxes,
        uint32                     mip,
        ColorTargetViewCreateInfo* pColorViewInfo,
        BindTargetParams*          pBindTargetsInfo) const;

    void ClearImageOneBox(
        GfxCmdBuffer*          pCmdBuffer,
        const SubResourceInfo& subResInfo,
        const Box*             pBox,
        bool                   hasBoxes) const;

    void ConvertYuvToRgb(
        GfxCmdBuffer*                     pCmdBuffer,
        const Image&                      srcImage,
        const Image&                      dstImage,
        uint32                            regionCount,
        const ColorSpaceConversionRegion* pRegions,
        const SamplerInfo&                sampler,
        const ColorSpaceConversionTable&  cscTable) const;

    void ConvertRgbToYuv(
        GfxCmdBuffer*                     pCmdBuffer,
        const Image&                      srcImage,
        const Image&                      dstImage,
        uint32                            regionCount,
        const ColorSpaceConversionRegion* pRegions,
        const SamplerInfo&                sampler,
        const ColorSpaceConversionTable&  cscTable) const;

    const ComputePipeline* GetCsResolvePipeline(
        const Image&  srcImage,
        ResolveMode   mode,
        ResolveMethod method) const;

    void LateExpandResolveSrc(
        GfxCmdBuffer*             pCmdBuffer,
        const Image&              srcImage,
        ImageLayout               srcImageLayout,
        const ImageResolveRegion* pRegions,
        uint32                    regionCount,
        ResolveMethod             method) const;

    GfxDevice*const  m_pDevice;
    uint32           m_srdAlignment; // All SRDs must be offset and size aligned to this many DWORDs.

    // All internal RPM pipelines are stored here.
    ComputePipeline*   m_pComputePipelines[static_cast<size_t>(RpmComputePipeline::Count)];
    GraphicsPipeline*  m_pGraphicsPipelines[RpmGfxPipelineCount];

    PAL_DISALLOW_DEFAULT_CTOR(RsrcProcMgr);
    PAL_DISALLOW_COPY_AND_ASSIGN(RsrcProcMgr);
};

extern void PreComputeDepthStencilClearSync(ICmdBuffer*        pCmdBuffer,
                                            const GfxImage&    gfxImage,
                                            const SubresRange& subres,
                                            ImageLayout        layout);
extern void PostComputeDepthStencilClearSync(ICmdBuffer* pCmdBuffer);

} // Pal
