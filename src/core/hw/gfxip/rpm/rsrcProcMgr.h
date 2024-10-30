/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
class  Pipeline;
struct ImageCopyRegion;
struct ImageResolveRegion;
struct MemoryCopyRegion;
struct MemoryImageCopyRegion;
class  MsaaState;
class  QueryPool;

static constexpr uint32 MaxLog2AaSamples   = 4; // Max AA sample rate is 16x.
static constexpr uint32 MaxLog2AaFragments = 3; // Max fragments is 8.

// Specifies image region for metadata fixup pre/post all RPM copies.
struct ImageFixupRegion
{
    SubresId subres;
    Offset3d offset;
    Extent3d extent;
    uint32   numSlices;
};

// Which engine should be used for RPM copies into images
enum class ImageCopyEngine : uint32
{
    Graphics        = 0x1,
    Compute         = 0x2,
    ComputeVrsDirty = 0x3,
};

// Specifies the shader-specific info used by CopyImageCs. Filled out by GetCopyImageCsInfo.
struct CopyImageCsInfo
{
    const ComputePipeline* pPipeline;
    DispatchDims           texelsPerGroup;
    bool                   isFmaskCopy;
    bool                   isFmaskCopyOptimized;
    bool                   useMipInSrd;
};

typedef void (*ClearImageCsCreateSrd)(
    const GfxDevice&   device,
    const Pal::Image&  image,
    const SubresRange& viewRange,
    const void*        pContext,
    void*              pSrd,
    Extent3d*          pExtent);

struct ClearImageCsInfo
{
    RpmComputePipeline pipelineEnum;   // Must be one of the "ClearImage" shader variants.
    DispatchDims       groupShape;     // Any combination of powers of two such that X*Y*Z = 64 (e.g. {8, 8, 1}).
    uint32             clearFragments; // This may not match the image's fragment count (e.g. FMask metadata clears).
    uint32             packedColor[4]; // The caller's packed (and pre-disable-masked) raw clear color.
    uint32             disableMask[4]; // A bitmask of packed texel bits that should not be cleared (disables writes).
    bool               hasDisableMask; // Set this to true if disableMask has any non-zero bits.
    bool               singleSubRes;   // If we can't clear multiple slices in one dispatch.
    uint32             texelShift;     // If non-zero, right shift the X-dimension offset and extent by this value.

    // ClearImageCs will call this function pointer with pSrdContext to build the image SRDs for its dispatches.
    ClearImageCsCreateSrd pSrdCallback;
    const void*           pSrdContext;
};

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
    virtual Result LateInit();
    virtual void Cleanup();

    void CopyMemoryCs(
        GfxCmdBuffer*           pCmdBuffer,
        gpusize                 srcGpuVirtAddr,
        const Device&           srcDevice,
        gpusize                 dstGpuVirtAddr,
        const Device&           dstDevice,
        uint32                  regionCount,
        const MemoryCopyRegion* pRegions,
        bool                    preferWideFormatCopy,
        bool                    srcIsCompressed,
        bool                    dstIsCompressed) const;

    void CopyMemoryCs(
        GfxCmdBuffer*           pCmdBuffer,
        const GpuMemory&        srcGpuMemory,
        const GpuMemory&        dstGpuMemory,
        uint32                  regionCount,
        const MemoryCopyRegion* pRegions) const;

    virtual void CmdCopyImage(
        GfxCmdBuffer*          pCmdBuffer,
        const Image&           srcImage,
        ImageLayout            srcImageLayout,
        const Image&           dstImage,
        ImageLayout            dstImageLayout,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        const Rect*            pScissorRect,
        uint32                 flags) const;

    virtual void CmdCopyMemoryToImage(
        GfxCmdBuffer*                pCmdBuffer,
        const GpuMemory&             srcGpuMemory,
        const Image&                 dstImage,
        ImageLayout                  dstImageLayout,
        uint32                       regionCount,
        const MemoryImageCopyRegion* pRegions,
        bool                         includePadding) const;

    virtual void CmdCopyImageToMemory(
        GfxCmdBuffer*                pCmdBuffer,
        const Image&                 srcImage,
        ImageLayout                  srcImageLayout,
        const GpuMemory&             dstGpuMemory,
        uint32                       regionCount,
        const MemoryImageCopyRegion* pRegions,
        bool                         includePadding) const;

    virtual void CmdResolveQuery(
        GfxCmdBuffer*         pCmdBuffer,
        const Pal::QueryPool& queryPool,
        QueryResultFlags      flags,
        QueryType             queryType,
        uint32                startQuery,
        uint32                queryCount,
        const GpuMemory&      dstGpuMemory,
        gpusize               dstOffset,
        gpusize               dstStride) const = 0;

    void CmdCopyTypedBuffer(
        GfxCmdBuffer*                pCmdBuffer,
        const GpuMemory&             srcGpuMemory,
        const GpuMemory&             dstGpuMemory,
        uint32                       regionCount,
        const TypedBufferCopyRegion* pRegions) const;

    void CmdScaledCopyTypedBufferToImage(
        GfxCmdBuffer*                           pCmdBuffer,
        const GpuMemory&                        srcGpuMemory,
        const Image&                            dstImage,
        ImageLayout                             dstImageLayout,
        uint32                                  regionCount,
        const TypedBufferImageScaledCopyRegion* pRegions) const;

    void CmdScaledCopyImage(
        GfxCmdBuffer*           pCmdBuffer,
        const ScaledCopyInfo&   copyInfo) const;

    void CmdGenerateMipmaps(
        GfxCmdBuffer*         pCmdBuffer,
        const GenMipmapsInfo& genInfo) const;

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

    void CmdFillMemory(
        GfxCmdBuffer*   pCmdBuffer,
        bool            saveRestoreComputeState,
        bool            trackBltActiveFlags,
        gpusize         dstGpuVirtAddr,
        gpusize         fillSize,
        uint32          data
        ) const;

    void CmdClearBoundDepthStencilTargets(
        GfxCmdBuffer*                 pCmdBuffer,
        float                         depth,
        uint8                         stencil,
        uint8                         stencilWriteMask,
        uint32                        samples,
        uint32                        fragments,
        DepthStencilSelectFlags       flag,
        uint32                        regionCount,
        const ClearBoundTargetRegion* pClearRegions) const;

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
        uint32                flags) const;

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

    virtual void CmdResolveImage(
        GfxCmdBuffer*             pCmdBuffer,
        const Image&              srcImage,
        ImageLayout               srcImageLayout,
        const Image&              dstImage,
        ImageLayout               dstImageLayout,
        ResolveMode               resolveMode,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions,
        uint32                    flags) const
        { PAL_NEVER_CALLED(); }

    virtual void CmdResolvePrtPlusImage(
        GfxCmdBuffer*                    pCmdBuffer,
        const IImage&                    srcImage,
        ImageLayout                      srcImageLayout,
        const IImage&                    dstImage,
        ImageLayout                      dstImageLayout,
        PrtPlusResolveType               resolveType,
        uint32                           regionCount,
        const PrtPlusImageResolveRegion* pRegions) const
        { PAL_NEVER_CALLED(); }

    virtual void CmdGfxDccToDisplayDcc(
        GfxCmdBuffer* pCmdBuffer,
        const IImage& image) const;

    virtual void CmdDisplayDccFixUp(
        GfxCmdBuffer*      pCmdBuffer,
        const IImage&      image) const;

protected:
    // When constructing SRD tables, all SRDs must be size and offset aligned to this many DWORDs.
    uint32 SrdDwordAlignment() const { return m_srdAlignment; }

    virtual bool CopyImageUseMipLevelInSrd(bool isCompressed) const { return UseMipLevelInSrd; }
    virtual bool CopyImageCsUseMsaaMorton(const Image& dstImage) const;

    // Assume optimized copies won't work
    virtual bool HwlUseOptimizedImageCopy(
        const Pal::Image&      srcImage,
        ImageLayout            srcImageLayout,
        const Pal::Image&      dstImage,
        ImageLayout            dstImageLayout,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions) const
        { return false; }

    virtual bool CopyDstBoundStencilNeedsWa(
        const GfxCmdBuffer* pCmdBuffer,
        const Pal::Image&   dstImage) const
        { return false; }

    //  If need to access single zRange for each subres independantly.
    virtual bool HwlNeedSinglezRangeAccess() const { return false; }

    virtual bool ScaledCopyImageUseGraphics(
        GfxCmdBuffer*           pCmdBuffer,
        const ScaledCopyInfo&   copyInfo) const
        { return false; }

    // Some blts need to use GFXIP-specific algorithms to pick the proper state. The baseState is the first
    // graphics state in a series of states that vary only on target format.
    virtual const GraphicsPipeline* GetGfxPipelineByFormat(
        RpmGfxPipeline basePipeline,
        SwizzledFormat format) const = 0;

    const ComputePipeline* GetPipeline(RpmComputePipeline pipeline) const
        { return m_pComputePipelines[static_cast<size_t>(pipeline)]; }

    const GraphicsPipeline* GetGfxPipeline(RpmGfxPipeline pipeline) const
        { return m_pGraphicsPipelines[pipeline]; }

    const MsaaState* GetMsaaState(uint32 samples, uint32 fragments) const;

    static gpusize ComputeTypedBufferRange(
        const Extent3d& extent,
        uint32          elementSize,
        gpusize         rowPitch,
        gpusize         depthPitch);

    void FillMem32Bit(
        GfxCmdBuffer*   pCmdBuffer,
        gpusize         dstGpuVirtAddr,
        gpusize         fillSize,
        uint32          data) const;

    void SlowClearCompute(
        GfxCmdBuffer*         pCmdBuffer,
        const Image&          dstImage,
        ImageLayout           dstImageLayout,
        const ClearColor&     color,
        const SwizzledFormat& clearFormat,
        const SubresRange&    clearRange,
        bool                  trackBltActiveFlags,
        uint32                boxCount,
        const Box*            pBoxes) const;

    void ClearImageCs(
        GfxCmdBuffer*           pCmdBuffer,
        const ClearImageCsInfo& info,
        const Image&            dstImage,
        const SubresRange&      clearRange,
        uint32                  boxCount,
        const Box*              pBoxes) const;

    const ComputePipeline* GetScaledCopyImageComputePipeline(
        const Image& srcImage,
        const Image& dstImage,
        TexFilter    filter,
        bool         is3d,
        bool*        pIsFmaskCopy) const;

    virtual void CopyImageCompute(
        GfxCmdBuffer*          pCmdBuffer,
        const Image&           srcImage,
        ImageLayout            srcImageLayout,
        const Image&           dstImage,
        ImageLayout            dstImageLayout,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        uint32                 flags) const;

    void CopyImageCs(
        GfxCmdBuffer*          pCmdBuffer,
        const Image&           srcImage,
        ImageLayout            srcImageLayout,
        const Image&           dstImage,
        ImageLayout            dstImageLayout,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        uint32                 flags) const;

    void GetCopyImageCsInfo(
        const Image&           srcImage,
        ImageLayout            srcImageLayout,
        const Image&           dstImage,
        ImageLayout            dstImageLayout,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        uint32                 flags,
        CopyImageCsInfo*       pInfo) const;

    void CopyBetweenMemoryAndImageCs(
        GfxCmdBuffer*                pCmdBuffer,
        const ComputePipeline*       pPipeline,
        const GpuMemory&             gpuMemory,
        const Image&                 image,
        ImageLayout                  imageLayout,
        bool                         isImageDst,
        bool                         isFmaskCopy,
        uint32                       regionCount,
        const MemoryImageCopyRegion* pRegions,
        bool                         includePadding) const;

    void ResolveImageCompute(
        GfxCmdBuffer*             pCmdBuffer,
        const Image&              srcImage,
        ImageLayout               srcImageLayout,
        const Image&              dstImage,
        ImageLayout               dstImageLayout,
        ResolveMode               resolveMode,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions,
        ResolveMethod             method,
        uint32                    flags) const;

    void LateExpandShaderResolveSrc(
        GfxCmdBuffer*             pCmdBuffer,
        const Image&              srcImage,
        ImageLayout               srcImageLayout,
        const ImageResolveRegion* pRegions,
        uint32                    regionCount,
        ResolveMethod             method,
        bool                      isCsResolve) const;

    void FixupLateExpandShaderResolveSrc(
        GfxCmdBuffer*             pCmdBuffer,
        const Image&              srcImage,
        ImageLayout               srcImageLayout,
        const ImageResolveRegion* pRegions,
        uint32                    regionCount,
        ResolveMethod             method,
        bool                      isCsResolve) const;

    void BindCommonGraphicsState(
        GfxCmdBuffer*  pCmdBuffer,
        VrsShadingRate vrsRate=VrsShadingRate::_1x1) const;

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
        uint32*                pTexelScale,
        bool*                  pSingleSubres) const;

    GfxDevice*const     m_pDevice;
    ColorBlendState*    m_pBlendDisableState;               // Blend state object with all blending disabled.
    ColorBlendState*    m_pColorBlendState;                 // Blend state object with rt0 blending enabled.
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

    virtual void HwlFixupCopyDstImageMetaData(
        GfxCmdBuffer*           pCmdBuffer,
        const Pal::Image*       pSrcImage,
        const Pal::Image&       dstImage,
        ImageLayout             dstImageLayout,
        const ImageFixupRegion* pRegions,
        uint32                  regionCount,
        bool                    isFmaskCopyOptimized) const = 0;

    virtual void HwlGfxDccToDisplayDcc(
        GfxCmdBuffer*     pCmdBuffer,
        const Pal::Image& image) const
        { PAL_NEVER_CALLED(); }

    virtual void InitDisplayDcc(
        GfxCmdBuffer*      pCmdBuffer,
        const Pal::Image&  image) const
        { PAL_NEVER_CALLED(); }

    virtual void CopyImageGraphics(
        GfxCmdBuffer*          pCmdBuffer,
        const Image&           srcImage,
        ImageLayout            srcImageLayout,
        const Image&           dstImage,
        ImageLayout            dstImageLayout,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        const Rect*            pScissorRect,
        uint32                 flags) const
        { PAL_NEVER_CALLED(); }

    virtual void ScaledCopyImageGraphics(
        GfxCmdBuffer*         pCmdBuffer,
        const ScaledCopyInfo& copyInfo) const
        { PAL_NEVER_CALLED(); }

    void ScaledCopyImageCompute(
        GfxCmdBuffer*           pCmdBuffer,
        const ScaledCopyInfo&   copyInfo) const;

    virtual void CopyBetweenMemoryAndImage(
        GfxCmdBuffer*                pCmdBuffer,
        const ComputePipeline*       pPipeline,
        const GpuMemory&             gpuMemory,
        const Image&                 image,
        ImageLayout                  imageLayout,
        bool                         isImageDst,
        bool                         isFmaskCopy,
        uint32                       regionCount,
        const MemoryImageCopyRegion* pRegions,
        bool                         includePadding) const;

    virtual void CopyBetweenTypedBufferAndImage(
        GfxCmdBuffer*                           pCmdBuffer,
        const ComputePipeline*                  pPipeline,
        const GpuMemory&                        gpuMemory,
        const Image&                            image,
        ImageLayout                             imageLayout,
        bool                                    isImageDst,
        uint32                                  regionCount,
        const TypedBufferImageScaledCopyRegion* pRegions) const;

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
        uint32        plane,
        ResolveMode   mode,
        ResolveMethod method) const;

    void LateExpandShaderResolveSrcHelper(
        GfxCmdBuffer*             pCmdBuffer,
        const ImageResolveRegion* pRegions,
        uint32                    regionCount,
        const ImgBarrier&         imgBarrier) const;

    virtual void FixupMetadataForComputeDst(
        GfxCmdBuffer*           pCmdBuffer,
        const Image&            dstImage,
        ImageLayout             dstImageLayout,
        uint32                  regionCount,
        const ImageFixupRegion* pRegions,
        bool                    beforeCopy) const = 0;

    virtual void FixupComputeResolveDst(
        GfxCmdBuffer*             pCmdBuffer,
        const Image&              dstImage,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions) const = 0;

    void GenerateMipmapsFast(
        GfxCmdBuffer*         pCmdBuffer,
        const GenMipmapsInfo& genInfo) const;

    void GenerateMipmapsSlow(
        GfxCmdBuffer*         pCmdBuffer,
        const GenMipmapsInfo& genInfo) const;

    uint32             m_srdAlignment;  // All SRDs must be offset and size aligned to this many DWORDs.

    // All internal RPM pipelines are stored here.
    ComputePipeline*   m_pComputePipelines[static_cast<size_t>(RpmComputePipeline::Count)];
    GraphicsPipeline*  m_pGraphicsPipelines[RpmGfxPipelineCount];

    PAL_DISALLOW_DEFAULT_CTOR(RsrcProcMgr);
    PAL_DISALLOW_COPY_AND_ASSIGN(RsrcProcMgr);
};

} // Pal
