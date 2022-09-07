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

    virtual void CmdCopyMemory(
        GfxCmdBuffer*           pCmdBuffer,
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

    virtual bool ExpandDepthStencil(
        GfxCmdBuffer*                pCmdBuffer,
        const Image&                 image,
        const MsaaQuadSamplePattern* pQuadSamplePattern,
        const SubresRange&           range) const;

    void ResummarizeDepthStencil(
        GfxCmdBuffer*                pCmdBuffer,
        const Image&                 image,
        ImageLayout                  imageLayout,
        const MsaaQuadSamplePattern* pQuadSamplePattern,
        const SubresRange&           range) const;

    virtual void HwlResummarizeHtileCompute(
        GfxCmdBuffer*      pCmdBuffer,
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
        uint32                 copyFlags) const override;

    virtual bool ScaledCopyImageUseGraphics(
        GfxCmdBuffer*           pCmdBuffer,
        const ScaledCopyInfo&   copyInfo) const override;

    // Generating indirect commands needs to choose different shaders based on the GFXIP version.
    virtual const ComputePipeline* GetCmdGenerationPipeline(
        const Pm4::IndirectCmdGenerator& generator,
        const CmdBuffer&                 cmdBuffer) const = 0;

    virtual const bool IsGfxPipelineForFormatSupported(
        SwizzledFormat format) const = 0;

    virtual bool PreferComputeForNonLocalDestCopy(
        const Pal::Image& dstImage) const
        { return false; }

    void GenericColorBlit(
        GfxCmdBuffer*                pCmdBuffer,
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

    virtual void ScaledCopyImageGraphics(
        GfxCmdBuffer*         pCmdBuffer,
        const ScaledCopyInfo& copyInfo) const override;

    // The next two functions should be called before and after a graphics copy. They give the gfxip layer a chance
    // to optimize the hardware for the copy operation and restore to the previous state once the copy is done.
    virtual uint32 HwlBeginGraphicsCopy(
        GfxCmdBuffer*           pCmdBuffer,
        const GraphicsPipeline* pPipeline,
        const Image&            dstImage,
        uint32                  bpp) const = 0;

    virtual void HwlEndGraphicsCopy(CmdStream* pCmdStream, uint32 restoreMask) const = 0;

    virtual void HwlHtileCopyAndFixUp(
        GfxCmdBuffer*             pCmdBuffer,
        const Pal::Image&         srcImage,
        const Pal::Image&         dstImage,
        ImageLayout               dstImageLayout,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions,
        bool                      computeResolve) const = 0;

    virtual void HwlFixupResolveDstImage(
        GfxCmdBuffer*             pCmdBuffer,
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
        GfxCmdBuffer*          pCmdBuffer,
        const Image&           srcImage,
        ImageLayout            srcImageLayout,
        const Image&           dstImage,
        ImageLayout            dstImageLayout,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        const Rect*            pScissorRect,
        uint32                 flags) const;

    void CopyColorImageGraphics(
        GfxCmdBuffer*          pCmdBuffer,
        const Image&           srcImage,
        ImageLayout            srcImageLayout,
        const Image&           dstImage,
        ImageLayout            dstImageLayout,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        const Rect*            pScissorRect,
        uint32                 flags) const;

    void ResolveImageDepthStencilGraphics(
        GfxCmdBuffer*             pCmdBuffer,
        const Image&              srcImage,
        ImageLayout               srcImageLayout,
        const Image&              dstImage,
        ImageLayout               dstImageLayout,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions,
        uint32                    flags) const;

    void ResolveImageFixedFunc(
        GfxCmdBuffer*             pCmdBuffer,
        const Image&              srcImage,
        ImageLayout               srcImageLayout,
        const Image&              dstImage,
        ImageLayout               dstImageLayout,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions,
        uint32                    flags) const;

    void ResolveImageDepthStencilCopy(
        GfxCmdBuffer*             pCmdBuffer,
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
