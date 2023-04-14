/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

namespace Pm4
{

class CmdStream;

// Specifies gpu addresses that are used as input to CmdGenerateIndirectCmds
struct GenerateInfo
{
    GfxCmdBuffer*                    pCmdBuffer;
    const Pipeline*                  pPipeline;
    const Pm4::IndirectCmdGenerator& generator;
    uint32                           indexBufSize; // Maximum number of indices in the bound index buffer.
    uint32                           maximumCount; // Maximum number of draw or dispatch commands.
    gpusize                          argsGpuAddr;  // Argument buffer GPU address.
    gpusize                          countGpuAddr; // GPU address of the memory containing the actual command
                                                   // count to generate.
};

// =====================================================================================================================
// Abstract class for executing basic Resource Processing Manager functionality in PM4.
class RsrcProcMgr : public Pal::RsrcProcMgr
{
public:
    void CmdGenerateIndirectCmds(
        const GenerateInfo& genInfo,
        CmdStreamChunk**    ppChunkLists[],
        uint32              NumChunkLists,
        uint32*             pNumGenChunks
        ) const;

    virtual void CmdCopyImage(
        GfxCmdBuffer*          pCmdBuffer,
        const Image&           srcImage,
        ImageLayout            srcImageLayout,
        const Image&           dstImage,
        ImageLayout            dstImageLayout,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        const Rect*            pScissorRect,
        uint32                 flags) const override;

    virtual void CmdCopyMemory(
        Pm4CmdBuffer*           pCmdBuffer,
        const GpuMemory&        srcGpuMemory,
        const GpuMemory&        dstGpuMemory,
        uint32                  regionCount,
        const MemoryCopyRegion* pRegions) const;

    virtual void CmdResolveImage(
        GfxCmdBuffer*             pCmdBuffer,
        const Image&              srcImage,
        ImageLayout               srcImageLayout,
        const Image&              dstImage,
        ImageLayout               dstImageLayout,
        ResolveMode               resolveMode,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions,
        uint32                    flags) const override;

    virtual void CmdClearColorImage(
        GfxCmdBuffer*         pCmdBuffer,
        const Image&          dstImage,
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
        const Image&       dstImage,
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

    virtual bool ExpandDepthStencil(
        Pm4CmdBuffer*                pCmdBuffer,
        const Image&                 image,
        const MsaaQuadSamplePattern* pQuadSamplePattern,
        const SubresRange&           range) const;

    void ResummarizeDepthStencil(
        Pm4CmdBuffer*                pCmdBuffer,
        const Image&                 image,
        ImageLayout                  imageLayout,
        const MsaaQuadSamplePattern* pQuadSamplePattern,
        const SubresRange&           range) const;

    virtual void HwlResummarizeHtileCompute(
        Pm4CmdBuffer*      pCmdBuffer,
        const GfxImage&    image,
        const SubresRange& range) const = 0;

protected:
    explicit RsrcProcMgr(GfxDevice* pDevice);
    virtual ~RsrcProcMgr();

    virtual ImageCopyEngine GetImageToImageCopyEngine(
        const GfxCmdBuffer*    pCmdBuffer,
        const Image&           srcImage,
        const Image&           dstImage,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        uint32                 copyFlags) const;

    virtual bool ScaledCopyImageUseGraphics(
        GfxCmdBuffer*           pCmdBuffer,
        const ScaledCopyInfo&   copyInfo) const override;

    void SlowClearGraphics(
        Pm4CmdBuffer*         pCmdBuffer,
        const Image&          dstImage,
        ImageLayout           dstImageLayout,
        const ClearColor*     pColor,
        const SwizzledFormat& clearFormat,
        const SubresRange&    clearRange,
        uint32                boxCount,
        const Box*            pBoxes) const;

    // Generating indirect commands needs to choose different shaders based on the GFXIP version.
    virtual const ComputePipeline* GetCmdGenerationPipeline(
        const Pm4::IndirectCmdGenerator& generator,
        const Pm4CmdBuffer&              cmdBuffer) const = 0;

    virtual const bool IsGfxPipelineForFormatSupported(
        SwizzledFormat format) const = 0;

    virtual bool PreferComputeForNonLocalDestCopy(
        const Pal::Image& dstImage) const
        { return false; }

    void GenericColorBlit(
        Pm4CmdBuffer*                pCmdBuffer,
        const Image&                 dstImage,
        const SubresRange&           range,
        const MsaaQuadSamplePattern* pQuadSamplePattern,
        RpmGfxPipeline               pipeline,
        const GpuMemory*             pGpuMemory,
        gpusize                      metaDataOffset
    ) const;

    const ComputePipeline* GetComputeMaskRamExpandPipeline(
        const Image& image) const;

    const ComputePipeline* GetLinearHtileClearPipeline(
        bool   expClearEnable,
        bool   tileStencilDisabled,
        uint32 hTileMask) const;

    const GraphicsPipeline* GetCopyDepthStencilPipeline(
        bool   isDepth,
        bool   isDepthStencil,
        uint32 numSamples) const;

    const GraphicsPipeline* GetScaledCopyDepthStencilPipeline(
        bool   isDepth,
        bool   isDepthStencil,
        uint32 numSamples) const;

    void PreComputeColorClearSync(
        ICmdBuffer*        pCmdBuffer,
        const IImage*      pImage,
        const SubresRange& subres,
        ImageLayout        layout) const;

    void PostComputeColorClearSync(
        ICmdBuffer*        pCmdBuffer,
        const IImage*      pImage,
        const SubresRange& subres,
        ImageLayout        layout,
        bool               csFastClear) const;

    virtual void PreComputeDepthStencilClearSync(
        ICmdBuffer*        pCmdBuffer,
        const GfxImage&    gfxImage,
        const SubresRange& subres,
        ImageLayout        layout) const;

    void PostComputeDepthStencilClearSync(
        ICmdBuffer*        pCmdBuffer,
        const GfxImage&    gfxImage,
        const SubresRange& subres,
        ImageLayout        layout,
        bool               csFastClear) const;

    const bool m_releaseAcquireSupported; // If acquire release interface is supported.

private:
    virtual void CopyImageGraphics(
        GfxCmdBuffer*          pCmdBuffer,
        const Image&           srcImage,
        ImageLayout            srcImageLayout,
        const Image&           dstImage,
        ImageLayout            dstImageLayout,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        const Rect*            pScissorRect,
        uint32                 flags) const override;

    bool SlowClearUseGraphics(
        Pm4CmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        const SubresRange& clearRange,
        ClearMethod        method) const;

    virtual void HwlImageToImageMissingPixelCopy(
        GfxCmdBuffer*          pCmdBuffer,
        const Pal::Image&      srcImage,
        const Pal::Image&      dstImage,
        const ImageCopyRegion& region) const = 0;

    virtual void ScaledCopyImageGraphics(
        GfxCmdBuffer*         pCmdBuffer,
        const ScaledCopyInfo& copyInfo) const override;

    // The next two functions should be called before and after a graphics copy. They give the gfxip layer a chance
    // to optimize the hardware for the copy operation and restore to the previous state once the copy is done.
    virtual uint32 HwlBeginGraphicsCopy(
        Pm4CmdBuffer*           pCmdBuffer,
        const GraphicsPipeline* pPipeline,
        const Image&            dstImage,
        uint32                  bpp) const = 0;

    virtual void HwlEndGraphicsCopy(CmdStream* pCmdStream, uint32 restoreMask) const = 0;

    virtual void HwlFastColorClear(
        Pm4CmdBuffer*         pCmdBuffer,
        const GfxImage&       dstImage,
        const uint32*         pConvertedColor,
        const SwizzledFormat& clearFormat,
        const SubresRange&    clearRange) const = 0;

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
        const Box*         pBox) const = 0;

    virtual void HwlHtileCopyAndFixUp(
        Pm4CmdBuffer*             pCmdBuffer,
        const Pal::Image&         srcImage,
        const Pal::Image&         dstImage,
        ImageLayout               dstImageLayout,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions,
        bool                      computeResolve) const = 0;

#if PAL_BUILD_GFX11
    virtual void HwlResolveImageGraphics(
        GfxCmdBuffer*              pCmdBuffer,
        const Pal::Image&         srcImage,
        ImageLayout               srcImageLayout,
        const Pal::Image&         dstImage,
        ImageLayout               dstImageLayout,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions,
        uint32                    flags) const { PAL_NEVER_CALLED(); };
#endif

    virtual void HwlFixupResolveDstImage(
        Pm4CmdBuffer*             pCmdBuffer,
        const GfxImage&           dstImage,
        ImageLayout               dstImageLayout,
        const ImageResolveRegion* pRegions,
        uint32                    regionCount,
        bool                      computeResolve) const = 0;

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

    void CopyDepthStencilImageGraphics(
        Pm4CmdBuffer*          pCmdBuffer,
        const Image&           srcImage,
        ImageLayout            srcImageLayout,
        const Image&           dstImage,
        ImageLayout            dstImageLayout,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        const Rect*            pScissorRect,
        uint32                 flags) const;

    void CopyColorImageGraphics(
        Pm4CmdBuffer*          pCmdBuffer,
        const Image&           srcImage,
        ImageLayout            srcImageLayout,
        const Image&           dstImage,
        ImageLayout            dstImageLayout,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        const Rect*            pScissorRect,
        uint32                 flags) const;

    void SlowClearGraphicsOneMip(
        Pm4CmdBuffer*              pCmdBuffer,
        const Image&               dstImage,
        const SubresId&            mipSubres,
        uint32                     boxCount,
        const Box*                 pBoxes,
        ColorTargetViewCreateInfo* pColorViewInfo,
        BindTargetParams*          pBindTargetsInfo,
        uint32                     xRightShift) const;

    void ClearImageOneBox(
        Pm4CmdBuffer*          pCmdBuffer,
        const SubResourceInfo& subResInfo,
        const Box*             pBox,
        bool                   hasBoxes,
        uint32                 xRightShift,
        uint32                 numInstances) const;

    void ResolveImageDepthStencilGraphics(
        Pm4CmdBuffer*             pCmdBuffer,
        const Image&              srcImage,
        ImageLayout               srcImageLayout,
        const Image&              dstImage,
        ImageLayout               dstImageLayout,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions,
        uint32                    flags) const;

    void ResolveImageFixedFunc(
        Pm4CmdBuffer*             pCmdBuffer,
        const Image&              srcImage,
        ImageLayout               srcImageLayout,
        const Image&              dstImage,
        ImageLayout               dstImageLayout,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions,
        uint32                    flags) const;

    void ResolveImageDepthStencilCopy(
        Pm4CmdBuffer*             pCmdBuffer,
        const Image&              srcImage,
        ImageLayout               srcImageLayout,
        const Image&              dstImage,
        ImageLayout               dstImageLayout,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions,
        uint32                    flags) const;

    virtual void FixupMetadataForComputeDst(
        GfxCmdBuffer*           pCmdBuffer,
        const Image&            dstImage,
        ImageLayout             dstImageLayout,
        uint32                  regionCount,
        const ImageFixupRegion* pRegions,
        bool                    beforeCopy) const override;

    virtual void FixupComputeResolveDst(
        GfxCmdBuffer*             pCmdBuffer,
        const Image&              dstImage,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions) const override;

    PAL_DISALLOW_DEFAULT_CTOR(RsrcProcMgr);
    PAL_DISALLOW_COPY_AND_ASSIGN(RsrcProcMgr);
};

} // Pm4
} // Pal
