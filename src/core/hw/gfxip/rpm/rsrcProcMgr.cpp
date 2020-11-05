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

#include "core/cmdStream.h"
#include "core/platform.h"
#include "core/g_palPlatformSettings.h"
#include "core/hw/gfxip/colorBlendState.h"
#include "core/hw/gfxip/computePipeline.h"
#include "core/hw/gfxip/depthStencilState.h"
#include "core/hw/gfxip/graphicsPipeline.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/indirectCmdGenerator.h"
#include "core/hw/gfxip/msaaState.h"
#include "core/hw/gfxip/rpm/rpmUtil.h"
#include "core/hw/gfxip/rpm/rsrcProcMgr.h"
#include "core/hw/gfxip/universalCmdBuffer.h"
#include "palAutoBuffer.h"
#include "palColorBlendState.h"
#include "palColorTargetView.h"
#include "palDepthStencilState.h"
#include "palDepthStencilView.h"
#include "palFormatInfo.h"
#include "palMsaaState.h"
#include "palInlineFuncs.h"

#include <float.h>
#include <math.h>

using namespace Util;

namespace Pal
{

static void PreComputeColorClearSync(ICmdBuffer* pCmdBuffer);
static void PostComputeColorClearSync(ICmdBuffer* pCmdBuffer);

// =====================================================================================================================
// Note that this constructor is invoked before settings have been committed.
RsrcProcMgr::RsrcProcMgr(
    GfxDevice* pDevice)
    :
    m_pBlendDisableState(nullptr),
    m_pColorBlendState(nullptr),
    m_pDepthDisableState(nullptr),
    m_pDepthClearState(nullptr),
    m_pStencilClearState(nullptr),
    m_pDepthStencilClearState(nullptr),
    m_pDepthExpandState(nullptr),
    m_pDepthResummarizeState(nullptr),
    m_pDepthResolveState(nullptr),
    m_pStencilResolveState(nullptr),
    m_pDepthStencilResolveState(nullptr),
    m_pDevice(pDevice),
    m_srdAlignment(0)
{
    memset(&m_pMsaaState[0], 0, sizeof(m_pMsaaState));
    memset(&m_pComputePipelines[0], 0, sizeof(m_pComputePipelines));
    memset(&m_pGraphicsPipelines[0], 0, sizeof(m_pGraphicsPipelines));
}

// =====================================================================================================================
RsrcProcMgr::~RsrcProcMgr()
{
    // These objects must be destroyed in Cleanup().
    for (uint32 idx = 0; idx < static_cast<uint32>(RpmComputePipeline::Count); ++idx)
    {
        PAL_ASSERT(m_pComputePipelines[idx] == nullptr);
    }

    for (uint32 idx = 0; idx < RpmGfxPipelineCount; ++idx)
    {
        PAL_ASSERT(m_pGraphicsPipelines[idx] == nullptr);
    }

    for (uint32 sampleIdx = 0; sampleIdx <= MaxLog2AaSamples; ++sampleIdx)
    {
        for (uint32 fragmentIdx = 0; fragmentIdx <= MaxLog2AaFragments; ++fragmentIdx)
        {
            PAL_ASSERT(m_pMsaaState[sampleIdx][fragmentIdx] == nullptr);
        }
    }

    PAL_ASSERT(m_pBlendDisableState      == nullptr);
    PAL_ASSERT(m_pColorBlendState        == nullptr);
    PAL_ASSERT(m_pDepthDisableState      == nullptr);
    PAL_ASSERT(m_pDepthClearState        == nullptr);
    PAL_ASSERT(m_pStencilClearState      == nullptr);
    PAL_ASSERT(m_pDepthStencilClearState == nullptr);
    PAL_ASSERT(m_pDepthExpandState       == nullptr);
    PAL_ASSERT(m_pDepthResummarizeState  == nullptr);
    PAL_ASSERT(m_pDepthResolveState      == nullptr);
    PAL_ASSERT(m_pStencilResolveState    == nullptr);
}

// =====================================================================================================================
// This must clean up all internal GPU memory allocations and all objects created after EarlyInit. Note that EarlyInit
// is called when the platform creates the device objects so the work it does must be preserved if we are to reuse
// this object.
void RsrcProcMgr::Cleanup()
{
    // Destroy all compute pipeline objects.
    for (uint32 idx = 0; idx < static_cast<uint32>(RpmComputePipeline::Count); ++idx)
    {
        if (m_pComputePipelines[idx] != nullptr)
        {
            m_pComputePipelines[idx]->DestroyInternal();
            m_pComputePipelines[idx] = nullptr;
        }
    }

    // Destroy all graphics pipeline objects.
    for (uint32 idx = 0; idx < RpmGfxPipelineCount; ++idx)
    {
        if (m_pGraphicsPipelines[idx] != nullptr)
        {
            m_pGraphicsPipelines[idx]->DestroyInternal();
            m_pGraphicsPipelines[idx] = nullptr;
        }
    }

    m_pDevice->DestroyColorBlendStateInternal(m_pBlendDisableState);
    m_pBlendDisableState = nullptr;

    m_pDevice->DestroyColorBlendStateInternal(m_pColorBlendState);
    m_pColorBlendState = nullptr;

    DepthStencilState**const ppDepthStates[] =
    {
        &m_pDepthDisableState,
        &m_pDepthClearState,
        &m_pStencilClearState,
        &m_pDepthStencilClearState,
        &m_pDepthExpandState,
        &m_pDepthResummarizeState,
        &m_pDepthResolveState,
        &m_pStencilResolveState,
        &m_pDepthStencilResolveState,
    };

    for (uint32 idx = 0; idx < ArrayLen(ppDepthStates); ++idx)
    {
        m_pDevice->DestroyDepthStencilStateInternal(*ppDepthStates[idx]);
        (*ppDepthStates[idx]) = nullptr;
    }

    for (uint32 sampleIdx = 0; sampleIdx <= MaxLog2AaSamples; ++sampleIdx)
    {
        for (uint32 fragmentIdx = 0; fragmentIdx <= MaxLog2AaFragments; ++fragmentIdx)
        {
            m_pDevice->DestroyMsaaStateInternal(m_pMsaaState[sampleIdx][fragmentIdx]);
            m_pMsaaState[sampleIdx][fragmentIdx] = nullptr;
        }
    }
}

// =====================================================================================================================
// Performs early initialization of this object; this occurs when the device owning is created.
Result RsrcProcMgr::EarlyInit()
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();
    m_srdAlignment = Max(chipProps.srdSizes.bufferView,
                         chipProps.srdSizes.fmaskView,
                         chipProps.srdSizes.imageView,
                         chipProps.srdSizes.sampler);

    // Round up to the size of a DWORD.
    m_srdAlignment = Util::NumBytesToNumDwords(m_srdAlignment);

    return Result::Success;
}

// =====================================================================================================================
// Performs any late-stage initialization that can only be done after settings have been committed.
Result RsrcProcMgr::LateInit()
{
    Result result = Result::Success;

    if (m_pDevice->Parent()->GetPublicSettings()->disableResourceProcessingManager == false)
    {
        result = CreateRpmComputePipelines(m_pDevice, m_pComputePipelines);

        if (result == Result::Success)
        {
            result = CreateRpmGraphicsPipelines(m_pDevice, m_pGraphicsPipelines);
        }

        if (result == Result::Success)
        {
            result = CreateCommonStateObjects();
        }

    }

    return result;
}

// =====================================================================================================================
// Builds commands to copy one or more regions from one GPU memory location to another with a compute shader.
void RsrcProcMgr::CopyMemoryCs(
    GfxCmdBuffer*           pCmdBuffer,
    const GpuMemory&        srcGpuMemory,
    const GpuMemory&        dstGpuMemory,
    uint32                  regionCount,
    const MemoryCopyRegion* pRegions
    ) const
{
    constexpr uint32  NumGpuMemory  = 2;        // source & destination.
    constexpr gpusize CopySizeLimit = 16777216; // 16 MB.

    bool p2pBltInfoRequired = m_pDevice->Parent()->IsP2pBltWaRequired(dstGpuMemory);

    uint32 newRegionCount = 0;
    if (p2pBltInfoRequired)
    {
        m_pDevice->P2pBltWaModifyRegionListMemory(dstGpuMemory,
                                                  regionCount,
                                                  pRegions,
                                                  &newRegionCount,
                                                  nullptr,
                                                  nullptr);
    }

    AutoBuffer<MemoryCopyRegion, 32, Platform> newRegions(newRegionCount, m_pDevice->GetPlatform());
    AutoBuffer<gpusize, 32, Platform> chunkAddrs(newRegionCount, m_pDevice->GetPlatform());
    if (p2pBltInfoRequired)
    {
        if ((newRegions.Capacity() >= newRegionCount) && (chunkAddrs.Capacity() >= newRegionCount))
        {
            m_pDevice->P2pBltWaModifyRegionListMemory(dstGpuMemory,
                                                      regionCount,
                                                      pRegions,
                                                      &newRegionCount,
                                                      &newRegions[0],
                                                      &chunkAddrs[0]);
            regionCount = newRegionCount;
            pRegions    = &newRegions[0];

            pCmdBuffer->P2pBltWaCopyBegin(&dstGpuMemory, regionCount, &chunkAddrs[0]);
        }
        else
        {
            pCmdBuffer->NotifyAllocFailure();
            p2pBltInfoRequired = false;
        }
    }

    // Save current command buffer state.
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

    // Now begin processing the list of copy regions.
    for (uint32 idx = 0; idx < regionCount; ++idx)
    {
        if (p2pBltInfoRequired)
        {
            pCmdBuffer->P2pBltWaCopyNextRegion(chunkAddrs[idx]);
        }

        const gpusize srcOffset  = pRegions[idx].srcOffset;
        const gpusize dstOffset  = pRegions[idx].dstOffset;
        const gpusize copySize   = pRegions[idx].copySize;

        for(gpusize copyOffset = 0; copyOffset < copySize; copyOffset += CopySizeLimit)
        {
            const uint32 copySectionSize = static_cast<uint32>(Min(CopySizeLimit, copySize - copyOffset));

            // Create an embedded user-data table and bind it to user data. We need buffer views for the source and
            // destination.
            uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                       SrdDwordAlignment() * NumGpuMemory,
                                                                       SrdDwordAlignment(),
                                                                       PipelineBindPoint::Compute,
                                                                       0);

            // Populate the table with raw buffer views, by convention the destination is placed before the source.
            BufferViewInfo rawBufferView = {};
            RpmUtil::BuildRawBufferViewInfo(&rawBufferView,
                                            dstGpuMemory,
                                            dstOffset + copyOffset,
                                            copySectionSize);
            m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &rawBufferView, pSrdTable);
            pSrdTable += SrdDwordAlignment();

            RpmUtil::BuildRawBufferViewInfo(&rawBufferView,
                                            srcGpuMemory,
                                            srcOffset + copyOffset,
                                            copySectionSize);
            m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &rawBufferView, pSrdTable);

            const uint32 regionUserData[3] = { 0, 0, copySectionSize };
            pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 1, 3, regionUserData);

            // Get the pipeline object and number of thread groups.
            const ComputePipeline* pPipeline = nullptr;
            uint32 numThreadGroups           = 0;

            constexpr uint32 DqwordSize = 4 * sizeof(uint32);
            if (IsPow2Aligned(srcOffset + copyOffset, DqwordSize) &&
                IsPow2Aligned(dstOffset + copyOffset, DqwordSize) &&
                IsPow2Aligned(copySectionSize, DqwordSize))
            {
                // Offsets and copySectionSize are DQWORD aligned so we can use the DQWORD copy pipeline.
                pPipeline       = GetPipeline(RpmComputePipeline::CopyBufferDqword);
                numThreadGroups = RpmUtil::MinThreadGroups(copySectionSize / DqwordSize,
                                                           pPipeline->ThreadsPerGroup());
            }
            else if (IsPow2Aligned(srcOffset + copyOffset, sizeof(uint32)) &&
                     IsPow2Aligned(dstOffset + copyOffset, sizeof(uint32)) &&
                     IsPow2Aligned(copySectionSize, sizeof(uint32)))
            {
                // Offsets and copySectionSize are DWORD aligned so we can use the DWORD copy pipeline.
                pPipeline       = GetPipeline(RpmComputePipeline::CopyBufferDword);
                numThreadGroups = RpmUtil::MinThreadGroups(copySectionSize / sizeof(uint32),
                                                           pPipeline->ThreadsPerGroup());
            }
            else
            {
                // Offsets and copySectionSize are not all DWORD aligned so we have to use the byte copy pipeline.
                pPipeline       = GetPipeline(RpmComputePipeline::CopyBufferByte);
                numThreadGroups = RpmUtil::MinThreadGroups(copySectionSize, pPipeline->ThreadsPerGroup());
            }

            // Bind pipeline and dispatch.
            pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });
            pCmdBuffer->CmdDispatch(numThreadGroups, 1, 1);
        }
    }

    if (p2pBltInfoRequired)
    {
        pCmdBuffer->P2pBltWaCopyEnd();
    }

    // Restore command buffer state.
    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
// Default implementation of getting the engine to use for image-to-image copies.
ImageCopyEngine RsrcProcMgr::GetImageToImageCopyEngine(
    const GfxCmdBuffer*    pCmdBuffer,
    const Image&           srcImage,
    const Image&           dstImage,
    uint32                 regionCount,
    const ImageCopyRegion* pRegions,
    uint32                 copyFlags
    ) const
{
    const auto&      srcInfo      = srcImage.GetImageCreateInfo();
    const auto&      dstInfo      = dstImage.GetImageCreateInfo();
    const ImageType  srcImageType = srcInfo.imageType;
    const ImageType  dstImageType = dstInfo.imageType;

    const bool bothColor    = ((srcImage.IsDepthStencil() == false) && (dstImage.IsDepthStencil() == false) &&
                               (Formats::IsDepthStencilOnly(srcInfo.swizzledFormat.format) == false) &&
                               (Formats::IsDepthStencilOnly(dstInfo.swizzledFormat.format) == false));
    const bool isCompressed = (Formats::IsBlockCompressed(srcInfo.swizzledFormat.format) ||
                               Formats::IsBlockCompressed(dstInfo.swizzledFormat.format));
    const bool isYuv        = (Formats::IsYuv(srcInfo.swizzledFormat.format) ||
                               Formats::IsYuv(dstInfo.swizzledFormat.format));
    const bool p2pBltWa     = m_pDevice->Parent()->ChipProperties().p2pBltWaInfo.required &&
                              dstImage.GetBoundGpuMemory().Memory()->AccessesPeerMemory();

    const bool isSrgbWithFormatConversion = (Formats::IsSrgb(dstInfo.swizzledFormat.format) &&
                                             TestAnyFlagSet(copyFlags, CopyFormatConversion));
    const bool isMacroPixelPackedRgbOnly  = (Formats::IsMacroPixelPackedRgbOnly(srcInfo.swizzledFormat.format) ||
                                             Formats::IsMacroPixelPackedRgbOnly(dstInfo.swizzledFormat.format));

    ImageCopyEngine  engineType = ImageCopyEngine::Compute;

    // We need to decide between the graphics copy path and the compute copy path. The graphics path only supports
    // single-sampled non-compressed, non-YUV , non-MacroPixelPackedRgbOnly 2D or 2D color images for now.
    if ((Image::PreferGraphicsCopy && pCmdBuffer->IsGraphicsSupported()) &&
        (p2pBltWa == false) &&
        (dstImage.IsDepthStencil() ||
         ((srcImageType != ImageType::Tex1d)   &&
          (dstImageType != ImageType::Tex1d)   &&
          (dstInfo.samples == 1)               &&
          (isCompressed == false)              &&
          (isYuv == false)                     &&
          (isMacroPixelPackedRgbOnly == false) &&
          (bothColor == true)                  &&
          (isSrgbWithFormatConversion == false))))
    {
        engineType = ImageCopyEngine::Graphics;
    }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 603
    // Scissor-enabled blit for OGLP is only supported on graphics path.
    PAL_ASSERT((engineType == ImageCopyEngine::Graphics) ||
               (TestAnyFlagSet(copyFlags, CopyEnableScissorTest) == false));
#endif

    return engineType;
}

// =====================================================================================================================
// Builds commands to copy one or more regions from one image to another.
void RsrcProcMgr::CmdCopyImage(
    GfxCmdBuffer*          pCmdBuffer,
    const Image&           srcImage,
    ImageLayout            srcImageLayout,
    const Image&           dstImage,
    ImageLayout            dstImageLayout,
    uint32                 regionCount,
    const ImageCopyRegion* pRegions,
    const Rect*            pScissorRect,
    uint32                 flags
    ) const
{
    const auto& srcInfo = srcImage.GetImageCreateInfo();
    const auto& dstInfo = dstImage.GetImageCreateInfo();

    // MSAA source and destination images must have the same number of samples.
    PAL_ASSERT(srcInfo.samples == dstInfo.samples);

    const ImageCopyEngine copyEngine =
        GetImageToImageCopyEngine(pCmdBuffer, srcImage, dstImage, regionCount, pRegions, flags);

    if (copyEngine == ImageCopyEngine::Graphics)
    {
        if (dstImage.IsDepthStencil())
        {
            CopyDepthStencilImageGraphics(pCmdBuffer,
                                          srcImage,
                                          srcImageLayout,
                                          dstImage,
                                          dstImageLayout,
                                          regionCount,
                                          pRegions,
                                          pScissorRect,
                                          flags);
        }
        else
        {
            CopyColorImageGraphics(pCmdBuffer,
                                   srcImage,
                                   srcImageLayout,
                                   dstImage,
                                   dstImageLayout,
                                   regionCount,
                                   pRegions,
                                   pScissorRect,
                                   flags);
        }
    }
    else
    {
        AutoBuffer<ImageFixupRegion, 32, Platform> fixupRegions(regionCount, m_pDevice->GetPlatform());
        if (fixupRegions.Capacity() >= regionCount)
        {
            for (uint32 i = 0; i < regionCount; i++)
            {
                fixupRegions[i].subres    = pRegions[i].dstSubres;
                fixupRegions[i].offset    = pRegions[i].dstOffset;
                fixupRegions[i].extent    = pRegions[i].extent;
                fixupRegions[i].numSlices = pRegions[i].numSlices;
            }
            FixupMetadataForComputeDst(pCmdBuffer, dstImage, dstImageLayout, regionCount, &fixupRegions[0], true);

            CopyImageCompute(pCmdBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout,
                             regionCount, pRegions, flags);

            FixupMetadataForComputeDst(pCmdBuffer, dstImage, dstImageLayout, regionCount, &fixupRegions[0], false);

            if (((Formats::IsBlockCompressed(srcInfo.swizzledFormat.format) ||
                  Formats::IsMacroPixelPackedRgbOnly(srcInfo.swizzledFormat.format)) && (srcInfo.mipLevels > 1)) ||
                ((Formats::IsBlockCompressed(dstInfo.swizzledFormat.format) ||
                  Formats::IsMacroPixelPackedRgbOnly(dstInfo.swizzledFormat.format)) && (dstInfo.mipLevels > 1)))
            {
                // Assume the missing pixel copy will no overwritten any pixel copied in normal path.
                // Or a cs done barrier is need to be inserted here.
                for (uint32 regionIdx = 0; regionIdx < regionCount; regionIdx++)
                {
                    HwlImageToImageMissingPixelCopy(pCmdBuffer, srcImage, dstImage, pRegions[regionIdx]);
                }
            }
        }
        else
        {
            pCmdBuffer->NotifyAllocFailure();
        }
    }

}

// =====================================================================================================================
// Builds commands to copy one or more regions from one image to another using a graphics pipeline.
// This path only supports copies between single-sampled non-compressed 2D, 2D color, and 3D images for now.
void RsrcProcMgr::CopyColorImageGraphics(
    GfxCmdBuffer*          pCmdBuffer,
    const Image&           srcImage,
    ImageLayout            srcImageLayout,
    const Image&           dstImage,
    ImageLayout            dstImageLayout,
    uint32                 regionCount,
    const ImageCopyRegion* pRegions,
    const Rect*            pScissorRect,
    uint32                 flags
    ) const
{
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());
    // Don't expect GFX Blts on Nested unless targets not inherited.
    PAL_ASSERT((pCmdBuffer->IsNested() == false) || (static_cast<UniversalCmdBuffer*>(
        pCmdBuffer)->GetGraphicsState().inheritedState.stateFlags.targetViewState == 0));

    // Get some useful information about the image.
    const auto& dstCreateInfo = dstImage.GetImageCreateInfo();
    const auto& srcCreateInfo = srcImage.GetImageCreateInfo();
    const auto& device        = *m_pDevice->Parent();

    Pal::CmdStream*const pStream = pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::Graphics);
    PAL_ASSERT(pStream != nullptr);

    const StencilRefMaskParams       stencilRefMasks       = { 0xFF, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0xFF, 0x01, 0xFF };

    ViewportParams viewportInfo = { };
    viewportInfo.count                 = 1;
    viewportInfo.viewports[0].origin   = PointOrigin::UpperLeft;
    viewportInfo.viewports[0].minDepth = 0.f;
    viewportInfo.viewports[0].maxDepth = 1.f;
    viewportInfo.horzClipRatio         = FLT_MAX;
    viewportInfo.horzDiscardRatio      = 1.0f;
    viewportInfo.vertClipRatio         = FLT_MAX;
    viewportInfo.vertDiscardRatio      = 1.0f;
    viewportInfo.depthRange            = DepthRange::ZeroToOne;

    ScissorRectParams scissorInfo = { };
    scissorInfo.count = 1;

    const ColorTargetViewInternalCreateInfo colorViewInfoInternal = { };

    ColorTargetViewCreateInfo colorViewInfo = { };
    colorViewInfo.imageInfo.pImage    = &dstImage;
    colorViewInfo.imageInfo.arraySize = 1;

    if (dstCreateInfo.imageType == ImageType::Tex3d)
    {
        colorViewInfo.zRange.extent     = 1;
        colorViewInfo.flags.zRangeValid = true;
    }

    BindTargetParams bindTargetsInfo = { };
    bindTargetsInfo.colorTargets[0].imageLayout      = dstImageLayout;
    bindTargetsInfo.colorTargets[0].pColorTargetView = nullptr;

    // Save current command buffer state.
    pCmdBuffer->PushGraphicsState();
    BindCommonGraphicsState(pCmdBuffer);
    pCmdBuffer->CmdBindColorBlendState(m_pBlendDisableState);
    pCmdBuffer->CmdBindDepthStencilState(m_pDepthDisableState);
    pCmdBuffer->CmdBindMsaaState(GetMsaaState(dstCreateInfo.samples, dstCreateInfo.fragments));
    pCmdBuffer->CmdSetStencilRefMasks(stencilRefMasks);

    SubresRange viewRange = { };
    viewRange.startSubres = srcImage.GetBaseSubResource();
    viewRange.numMips     = srcCreateInfo.mipLevels;
    // Use the depth of base subresource as the number of array slices since 3D image is viewed as 2D array
    // later. Src image view is set up as a whole rather than per mip-level, using base subresource's depth
    // to cover the MAX_SLICE of all mip-level.
    viewRange.numSlices   =
        (srcCreateInfo.imageType == ImageType::Tex3d) ? srcCreateInfo.extent.depth : srcCreateInfo.arraySize;

    // Keep track of the previous graphics pipeline to reduce the pipeline switching overhead.
    const GraphicsPipeline* pPreviousPipeline = nullptr;

    // Accumulate the restore mask for each region copied.
    uint32 restoreMask = 0;

    // Each region needs to be copied individually.
    for (uint32 region = 0; region < regionCount; ++region)
    {
        // Multiply all x-dimension values in our region by the texel scale.
        ImageCopyRegion copyRegion = pRegions[region];

        // Determine which image formats to use for the copy.
        SwizzledFormat dstFormat    = { };
        SwizzledFormat srcFormat    = { };
        uint32         texelScale   = 1;
        bool           singleSubres = false;

        GetCopyImageFormats(srcImage,
                            srcImageLayout,
                            dstImage,
                            dstImageLayout,
                            copyRegion,
                            flags,
                            &srcFormat,
                            &dstFormat,
                            &texelScale,
                            &singleSubres);

        // Update the color target view format with the destination format.
        colorViewInfo.swizzledFormat = dstFormat;

        // Only switch to the appropriate graphics pipeline if it differs from the previous region's pipeline.
        const GraphicsPipeline*const pPipeline = GetGfxPipelineByTargetIndexAndFormat(Copy_32ABGR, 0, dstFormat);
        if (pPreviousPipeline != pPipeline)
        {
            pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics, pPipeline, InternalApiPsoHash, });
            pCmdBuffer->CmdOverwriteRbPlusFormatForBlits(dstFormat, 0);
            pPreviousPipeline = pPipeline;
        }

        if (singleSubres == false)
        {
            // We'll setup both 2D and 3D src images as a 2D view.
            //
            // Is it legal for the shader to view 3D images as 2D?
            ImageViewInfo imageView = {};
            RpmUtil::BuildImageViewInfo(&imageView,
                                        srcImage,
                                        viewRange,
                                        srcFormat,
                                        srcImageLayout,
                                        device.TexOptLevel());

            // Create an embedded SRD table and bind it to user data 4 for pixel work.
            uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                       SrdDwordAlignment(),
                                                                       SrdDwordAlignment(),
                                                                       PipelineBindPoint::Graphics,
                                                                       4);

            // Populate the table with an image view of the source image.
            device.CreateImageViewSrds(1, &imageView, pSrdTable);
        }

        // Give the gfxip layer a chance to optimize the hardware before we start copying.
        const uint32 bitsPerPixel = Formats::BitsPerPixel(dstFormat.format);
        restoreMask              |= HwlBeginGraphicsCopy(pCmdBuffer, pPipeline, dstImage, bitsPerPixel);

        // When copying from 3D to 3D, the number of slices should be 1. When copying from
        // 1D to 1D or 2D to 2D, depth should be 1. Therefore when the src image type is identical
        // to the dst image type, either the depth or the number of slices should be equal to 1.
        PAL_ASSERT((srcCreateInfo.imageType != dstCreateInfo.imageType) ||
                   (copyRegion.numSlices == 1) ||
                   (copyRegion.extent.depth == 1));

        // When copying from 2D to 3D or 3D to 2D, the number of slices should match the depth.
        PAL_ASSERT((srcCreateInfo.imageType == dstCreateInfo.imageType) ||
                   ((((srcCreateInfo.imageType == ImageType::Tex3d) &&
                      (dstCreateInfo.imageType == ImageType::Tex2d)) ||
                     ((srcCreateInfo.imageType == ImageType::Tex2d) &&
                      (dstCreateInfo.imageType == ImageType::Tex3d))) &&
                    (copyRegion.numSlices == copyRegion.extent.depth)));

        copyRegion.srcOffset.x  *= texelScale;
        copyRegion.dstOffset.x  *= texelScale;
        copyRegion.extent.width *= texelScale;

        // Setup the viewport and scissor to restrict rendering to the destination region being copied.
        viewportInfo.viewports[0].originX = static_cast<float>(copyRegion.dstOffset.x);
        viewportInfo.viewports[0].originY = static_cast<float>(copyRegion.dstOffset.y);
        viewportInfo.viewports[0].width   = static_cast<float>(copyRegion.extent.width);
        viewportInfo.viewports[0].height  = static_cast<float>(copyRegion.extent.height);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 603
        if (TestAnyFlagSet(flags, CopyEnableScissorTest))
        {
            scissorInfo.scissors[0].offset.x      = pScissorRect->offset.x;
            scissorInfo.scissors[0].offset.y      = pScissorRect->offset.y;
            scissorInfo.scissors[0].extent.width  = pScissorRect->extent.width;
            scissorInfo.scissors[0].extent.height = pScissorRect->extent.height;
        }
        else
#endif
        {
            scissorInfo.scissors[0].offset.x      = copyRegion.dstOffset.x;
            scissorInfo.scissors[0].offset.y      = copyRegion.dstOffset.y;
            scissorInfo.scissors[0].extent.width  = copyRegion.extent.width;
            scissorInfo.scissors[0].extent.height = copyRegion.extent.height;
        }

        pCmdBuffer->CmdSetViewports(viewportInfo);
        pCmdBuffer->CmdSetScissorRects(scissorInfo);

        const float texcoordVs[4] =
        {
            static_cast<float>(copyRegion.srcOffset.x),
            static_cast<float>(copyRegion.srcOffset.y),
            static_cast<float>(copyRegion.srcOffset.x + copyRegion.extent.width),
            static_cast<float>(copyRegion.srcOffset.y + copyRegion.extent.height)
        };

        const uint32* pUserDataVs = reinterpret_cast<const uint32*>(&texcoordVs);
        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 0, 4, pUserDataVs);

        // Copy may happen between the layers of a 2d image and the slices of a 3d image.
        const uint32 numSlices = Max(copyRegion.numSlices, copyRegion.extent.depth);

        // Each slice is copied individually, we can optimize this into fewer draw calls if it becomes a
        // performance bottleneck, but for now this is simpler.
        for (uint32 sliceOffset = 0; sliceOffset < numSlices; ++sliceOffset)
        {
            const uint32 srcSlice    = ((srcCreateInfo.imageType == ImageType::Tex3d)
                                        ? copyRegion.srcOffset.z          + sliceOffset
                                        : copyRegion.srcSubres.arraySlice + sliceOffset);

            if (singleSubres)
            {
                const bool singleArrayAccess  = (srcCreateInfo.imageType != ImageType::Tex3d);
                const bool singlezRangeAccess = (srcCreateInfo.imageType == ImageType::Tex3d) &&
                                                HwlNeedSinglezRangeAccess();

                viewRange.numMips     = 1;
                viewRange.numSlices   = 1;
                viewRange.startSubres = copyRegion.srcSubres;

                if (singleArrayAccess)
                {
                    viewRange.startSubres.arraySlice += sliceOffset;
                }

                ImageViewInfo imageView = {};
                RpmUtil::BuildImageViewInfo(&imageView,
                                            srcImage,
                                            viewRange,
                                            srcFormat,
                                            srcImageLayout,
                                            device.TexOptLevel());

                if (singlezRangeAccess)
                {
                    imageView.zRange.offset     = srcSlice;
                    imageView.zRange.extent     = 1;
                    imageView.flags.zRangeValid = 1;
                }

                // Create an embedded SRD table and bind it to user data 4 for pixel work.
                uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                           SrdDwordAlignment(),
                                                                           SrdDwordAlignment(),
                                                                           PipelineBindPoint::Graphics,
                                                                           4);

                // Populate the table with an image view of the source image.
                device.CreateImageViewSrds(1, &imageView, pSrdTable);

                const uint32 userDataPs[2] =
                {
                    (singleArrayAccess || singlezRangeAccess) ? 0 : sliceOffset,
                    0
                };

                pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 5, 2, &userDataPs[0]);
            }
            else
            {
                const uint32 userDataPs[2] =
                {
                    srcSlice,
                    copyRegion.srcSubres.mipLevel
                };
                pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 5, 2, &userDataPs[0]);
            }

            colorViewInfo.imageInfo.baseSubRes = copyRegion.dstSubres;

            if (dstCreateInfo.imageType == ImageType::Tex3d)
            {
                colorViewInfo.zRange.offset = copyRegion.dstOffset.z + sliceOffset;
            }
            else
            {
                colorViewInfo.imageInfo.baseSubRes.arraySlice = copyRegion.dstSubres.arraySlice + sliceOffset;
            }

            // Create and bind a color-target view for this slice.
            LinearAllocatorAuto<VirtualLinearAllocator> sliceAlloc(pCmdBuffer->Allocator(), false);

            IColorTargetView* pColorView = nullptr;
            void* pColorViewMem =
                PAL_MALLOC(m_pDevice->GetColorTargetViewSize(nullptr), &sliceAlloc, AllocInternalTemp);

            if (pColorViewMem == nullptr)
            {
                pCmdBuffer->NotifyAllocFailure();
            }
            else
            {
                // Since our color target view can only bind 1 slice at a time, we have to issue a separate draw for
                // each slice in extent.z. We can keep the same src image view since we pass the explicit slice to
                // read from in user data, but we'll need to create a new color target view each time.
                Result result = m_pDevice->CreateColorTargetView(colorViewInfo,
                                                                 colorViewInfoInternal,
                                                                 pColorViewMem,
                                                                 &pColorView);
                PAL_ASSERT(result == Result::Success);

                bindTargetsInfo.colorTargets[0].pColorTargetView = pColorView;
                bindTargetsInfo.colorTargetCount = 1;
                pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                // Draw a fullscreen quad.
                pCmdBuffer->CmdDraw(0, 3, 0, 1, 0);

                // Unbind the color-target view.
                bindTargetsInfo.colorTargetCount = 0;
                pCmdBuffer->CmdBindTargets(bindTargetsInfo);
                PAL_SAFE_FREE(pColorViewMem, &sliceAlloc);
            }
        }
    }

    // Call back to the gfxip layer so it can restore any state it modified previously.
    HwlEndGraphicsCopy(pStream, restoreMask);

    // Restore original command buffer state.
    pCmdBuffer->PopGraphicsState();
}

// =====================================================================================================================
// Copies multisampled depth-stencil images using a graphics pipeline.
void RsrcProcMgr::CopyDepthStencilImageGraphics(
    GfxCmdBuffer*          pCmdBuffer,
    const Image&           srcImage,
    ImageLayout            srcImageLayout,
    const Image&           dstImage,
    ImageLayout            dstImageLayout,
    uint32                 regionCount,
    const ImageCopyRegion* pRegions,
    const Rect*            pScissorRect,
    uint32                 flags
    ) const
{
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());
    // Don't expect GFX Blts on Nested unless targets not inherited.
    PAL_ASSERT((pCmdBuffer->IsNested() == false) || (static_cast<UniversalCmdBuffer*>(
        pCmdBuffer)->GetGraphicsState().inheritedState.stateFlags.targetViewState == 0));

    const auto& device        = *m_pDevice->Parent();
    const auto& texOptLevel   = device.TexOptLevel();
    const auto& dstCreateInfo = dstImage.GetImageCreateInfo();
    const auto& srcCreateInfo = srcImage.GetImageCreateInfo();

    const StencilRefMaskParams       stencilRefMasks      = { 0xFF, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0xFF, 0x01, 0xFF, };

    // Initialize some structures we will need later on.
    ViewportParams viewportInfo = { };
    viewportInfo.count                 = 1;
    viewportInfo.viewports[0].minDepth = 0.f;
    viewportInfo.viewports[0].maxDepth = 1.f;
    viewportInfo.viewports[0].origin   = PointOrigin::UpperLeft;
    viewportInfo.horzClipRatio         = FLT_MAX;
    viewportInfo.horzDiscardRatio      = 1.0f;
    viewportInfo.vertClipRatio         = FLT_MAX;
    viewportInfo.vertDiscardRatio      = 1.0f;
    viewportInfo.depthRange            = DepthRange::ZeroToOne;

    ScissorRectParams scissorInfo = { };
    scissorInfo.count = 1;

    const DepthStencilViewInternalCreateInfo noDepthViewInfoInternal = { };
    DepthStencilViewCreateInfo               depthViewInfo           = { };
    depthViewInfo.pImage    = &dstImage;
    depthViewInfo.arraySize = 1;

    // Save current command buffer state and bind graphics state which is common for all regions.
    pCmdBuffer->PushGraphicsState();
    BindCommonGraphicsState(pCmdBuffer);
    pCmdBuffer->CmdBindMsaaState(GetMsaaState(dstCreateInfo.samples, dstCreateInfo.fragments));
    pCmdBuffer->CmdSetStencilRefMasks(stencilRefMasks);

    RpmUtil::WriteVsZOut(pCmdBuffer, 1.0f);

    // Setup the viewport and scissor to restrict rendering to the destination region being copied.
    viewportInfo.viewports[0].originX = static_cast<float>(pRegions[0].dstOffset.x);
    viewportInfo.viewports[0].originY = static_cast<float>(pRegions[0].dstOffset.y);
    viewportInfo.viewports[0].width   = static_cast<float>(pRegions[0].extent.width);
    viewportInfo.viewports[0].height  = static_cast<float>(pRegions[0].extent.height);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 603
    if (TestAnyFlagSet(flags, CopyEnableScissorTest))
    {
        scissorInfo.scissors[0].offset.x      = pScissorRect->offset.x;
        scissorInfo.scissors[0].offset.y      = pScissorRect->offset.y;
        scissorInfo.scissors[0].extent.width  = pScissorRect->extent.width;
        scissorInfo.scissors[0].extent.height = pScissorRect->extent.height;
    }
    else
#endif
    {
        scissorInfo.scissors[0].offset.x      = pRegions[0].dstOffset.x;
        scissorInfo.scissors[0].offset.y      = pRegions[0].dstOffset.y;
        scissorInfo.scissors[0].extent.width  = pRegions[0].extent.width;
        scissorInfo.scissors[0].extent.height = pRegions[0].extent.height;
    }

    // The shader will calculate src coordinates by adding a delta to the dst coordinates. The user data should
    // contain those deltas which are (srcOffset-dstOffset) for X & Y.
    const int32  xOffset = (pRegions[0].srcOffset.x - pRegions[0].dstOffset.x);
    const int32  yOffset = (pRegions[0].srcOffset.y - pRegions[0].dstOffset.y);
    const uint32 userData[2] =
    {
        reinterpret_cast<const uint32&>(xOffset),
        reinterpret_cast<const uint32&>(yOffset)
    };

    pCmdBuffer->CmdSetViewports(viewportInfo);
    pCmdBuffer->CmdSetScissorRects(scissorInfo);
    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 2, 2, userData);

    // To improve performance, input src coordinates to VS, avoid using screen position in PS.
    const float texcoordVs[4] =
    {
        static_cast<float>(pRegions[0].srcOffset.x),
        static_cast<float>(pRegions[0].srcOffset.y),
        static_cast<float>(pRegions[0].srcOffset.x + pRegions[0].extent.width),
        static_cast<float>(pRegions[0].srcOffset.y + pRegions[0].extent.height)
    };

    const uint32* pUserDataVs = reinterpret_cast<const uint32*>(&texcoordVs);
    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 6, 4, pUserDataVs);

    AutoBuffer<bool, 16, Platform> isRangeProcessed(regionCount, m_pDevice->GetPlatform());
    PAL_ASSERT(isRangeProcessed.Capacity() >= regionCount);

    // Notify the command buffer that the AutoBuffer allocation has failed.
    if (isRangeProcessed.Capacity() < regionCount)
    {
        pCmdBuffer->NotifyAllocFailure();
    }
    else
    {
        memset(&isRangeProcessed[0], false, isRangeProcessed.SizeBytes());

        // Now issue fast or slow clears to all ranges, grouping identical depth/stencil pairs if possible.
        for (uint32 idx = 0; idx < regionCount; idx++)
        {
            // Same sanity checks of the region aspects.
            const bool isDepth = (pRegions[idx].dstSubres.aspect == ImageAspect::Depth);
            bool isDepthStencil = false;

            BindTargetParams bindTargetsInfo = {};

            // It's possible that SRC may be not a depth/stencil resource and it's created with X32_UINT from
            // R32_TYPELESS, use DST's format to setup SRC format correctly.
            const ChNumFormat depthFormat = dstImage.GetImageCreateInfo().swizzledFormat.format;

            bindTargetsInfo.depthTarget.depthLayout = dstImageLayout;

            // No need to clear a range twice.
            if (isRangeProcessed[idx])
            {
                continue;
            }

            uint32 secondSurface = 0;

            // Search the range list to see if there is a matching range which span the other aspect.
            for (uint32 forwardIdx = idx + 1; forwardIdx < regionCount; ++forwardIdx)
            {
                // TODO: there is unknown corruption issue if grouping depth and stencil copy together for mipmap
                //       image, disallow merging copy for mipmap image as a temp fix.
                if ((dstCreateInfo.mipLevels                   == 1)                                  &&
                    (pRegions[forwardIdx].srcSubres.aspect     != pRegions[idx].srcSubres.aspect)     &&
                    (pRegions[forwardIdx].dstSubres.aspect     != pRegions[idx].dstSubres.aspect)     &&
                    (pRegions[forwardIdx].srcSubres.mipLevel   == pRegions[idx].srcSubres.mipLevel)   &&
                    (pRegions[forwardIdx].dstSubres.mipLevel   == pRegions[idx].dstSubres.mipLevel)   &&
                    (pRegions[forwardIdx].srcSubres.arraySlice == pRegions[idx].srcSubres.arraySlice) &&
                    (pRegions[forwardIdx].dstSubres.arraySlice == pRegions[idx].dstSubres.arraySlice) &&
                    (pRegions[forwardIdx].extent.depth         == pRegions[idx].extent.depth)         &&
                    (pRegions[forwardIdx].extent.height        == pRegions[idx].extent.height)        &&
                    (pRegions[forwardIdx].extent.width         == pRegions[idx].extent.width)         &&
                    (pRegions[forwardIdx].numSlices            == pRegions[idx].numSlices))
                {
                    // We found a matching range that for the other aspect, clear them both at once.
                    isDepthStencil = true;
                    isRangeProcessed[forwardIdx] = true;
                    secondSurface = forwardIdx;
                    break;
                }
            }
            pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics,
                                          GetCopyDepthStencilPipeline(
                                              isDepth,
                                              isDepthStencil,
                                              srcImage.GetImageCreateInfo().samples),
                                          InternalApiPsoHash, });

            // Determine which format we should use to view the source image.
            SwizzledFormat srcFormat =
            {
                ChNumFormat::Undefined,
                { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
            };

            if (isDepthStencil)
            {
                // We should only be in the depth stencil case when we have a depth stencil format
                PAL_ASSERT((depthFormat == ChNumFormat::D32_Float_S8_Uint) ||
                           (depthFormat == ChNumFormat::D16_Unorm_S8_Uint));
                if (depthFormat == ChNumFormat::D32_Float_S8_Uint)
                {
                    srcFormat.format = ChNumFormat::X32_Float;
                }
                else
                {
                    srcFormat.format = ChNumFormat::X16_Unorm;
                }
                pCmdBuffer->CmdBindDepthStencilState(m_pDepthStencilResolveState);
            }
            else if (isDepth)
            {
                if ((depthFormat == ChNumFormat::D32_Float_S8_Uint) || (depthFormat == ChNumFormat::X32_Float))
                {
                    srcFormat.format = ChNumFormat::X32_Float;
                }
                else
                {
                    srcFormat.format = ChNumFormat::X16_Unorm;
                }
                pCmdBuffer->CmdBindDepthStencilState(m_pDepthResolveState);
            }
            else
            {
                srcFormat.format = ChNumFormat::X8_Uint;
                pCmdBuffer->CmdBindDepthStencilState(m_pStencilResolveState);
            }

            for (uint32 slice = 0; slice < pRegions[idx].numSlices; ++slice)
            {
                LinearAllocatorAuto<VirtualLinearAllocator> sliceAlloc(pCmdBuffer->Allocator(), false);

                // Create an embedded user-data table and bind it to user data 1. We need an image view for each aspect.
                const uint32 numSrds = isDepthStencil ? 2 : 1;
                uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                           SrdDwordAlignment() * numSrds,
                                                                           SrdDwordAlignment(),
                                                                           PipelineBindPoint::Graphics,
                                                                           1);

                if (isDepthStencil)
                {
                    // Populate the table with an image view of the source image.
                    ImageViewInfo imageView[2] = {};
                    SubresRange viewRange      = { pRegions[idx].srcSubres, 1, 1 };

                    viewRange.startSubres.arraySlice += slice;

                    RpmUtil::BuildImageViewInfo(&imageView[0],
                                                srcImage,
                                                viewRange,
                                                srcFormat,
                                                srcImageLayout,
                                                texOptLevel);

                    srcFormat.format = ChNumFormat::X8_Uint;
                    viewRange        = { pRegions[secondSurface].srcSubres, 1, 1 };

                    viewRange.startSubres.arraySlice += slice;

                    RpmUtil::BuildImageViewInfo(&imageView[1],
                                                srcImage,
                                                viewRange,
                                                srcFormat,
                                                srcImageLayout,
                                                texOptLevel);
                    device.CreateImageViewSrds(2, &imageView[0], pSrdTable);
                }
                else
                {
                    // Populate the table with an image view of the source image.
                    ImageViewInfo imageView = {};
                    SubresRange   viewRange = { pRegions[idx].srcSubres, 1, 1 };

                    viewRange.startSubres.arraySlice += slice;

                    RpmUtil::BuildImageViewInfo(&imageView,
                                                srcImage,
                                                viewRange,
                                                srcFormat,
                                                srcImageLayout,
                                                texOptLevel);
                    device.CreateImageViewSrds(1, &imageView, pSrdTable);
                }

                // Create and bind a depth stencil view of the destination region.
                depthViewInfo.baseArraySlice = pRegions[idx].dstSubres.arraySlice + slice;
                depthViewInfo.mipLevel       = pRegions[idx].dstSubres.mipLevel;

                void* pDepthStencilViewMem = PAL_MALLOC(m_pDevice->GetDepthStencilViewSize(nullptr),
                                                        &sliceAlloc,
                                                        AllocInternalTemp);
                if (pDepthStencilViewMem == nullptr)
                {
                    pCmdBuffer->NotifyAllocFailure();
                }
                else
                {
                    IDepthStencilView* pDepthView = nullptr;
                    Result result = m_pDevice->CreateDepthStencilView(depthViewInfo,
                                                                      noDepthViewInfoInternal,
                                                                      pDepthStencilViewMem,
                                                                      &pDepthView);
                    PAL_ASSERT(result == Result::Success);

                    bindTargetsInfo.depthTarget.pDepthStencilView = pDepthView;
                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                    // Draw a fullscreen quad.
                    pCmdBuffer->CmdDraw(0, 3, 0, 1, 0);

                    // Unbind the depth view and destroy it.
                    bindTargetsInfo.depthTarget.pDepthStencilView = nullptr;
                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                    PAL_SAFE_FREE(pDepthStencilViewMem, &sliceAlloc);
                }
            } // End for each slice.
        } // End for each region
    }
    // Restore original command buffer state.
    pCmdBuffer->PopGraphicsState();
}

// =====================================================================================================================
// Builds commands to copy one or more regions from one image to another using a compute shader.
// The caller should assert that the source and destination images have the same image types and sample counts.
void RsrcProcMgr::CopyImageCompute(
    GfxCmdBuffer*          pCmdBuffer,
    const Image&           srcImage,
    ImageLayout            srcImageLayout,
    const Image&           dstImage,
    ImageLayout            dstImageLayout,
    uint32                 regionCount,
    const ImageCopyRegion* pRegions,
    uint32                 flags
    ) const
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 603
    PAL_ASSERT(TestAnyFlagSet(flags, CopyEnableScissorTest) == false);
#endif

    const auto&     device        = *m_pDevice->Parent();
    const auto&     dstCreateInfo = dstImage.GetImageCreateInfo();
    const auto&     srcCreateInfo = srcImage.GetImageCreateInfo();
    const bool      isCompressed  = (Formats::IsBlockCompressed(srcCreateInfo.swizzledFormat.format) ||
                                     Formats::IsBlockCompressed(dstCreateInfo.swizzledFormat.format));
    const bool      useMipInSrd   = CopyImageUseMipLevelInSrd(isCompressed);
    const GfxImage* pSrcGfxImage  = srcImage.GetGfxImage();
    const ImageType imageType     = pSrcGfxImage->GetOverrideImageType();
    const bool      isEqaaSrc     = (srcCreateInfo.samples != srcCreateInfo.fragments);

    bool isFmaskCopy          = false;
    bool isFmaskCopyOptimized = false;
    // Get the appropriate pipeline object.
    const ComputePipeline* pPipeline = nullptr;

    // The Fmask accelerated copy should be used in all non-EQAA cases where Fmask is enabled. There is no use case
    // Fmask accelerated EQAA copy and it would require several new shaders. It can be implemented at a future
    // point if required.
    if (pSrcGfxImage->HasFmaskData() && isEqaaSrc)
    {
        PAL_NOT_IMPLEMENTED();
    }

    if (pSrcGfxImage->HasFmaskData() && (isEqaaSrc == false))
    {
        PAL_ASSERT(srcCreateInfo.fragments > 1);
        PAL_ASSERT((srcImage.IsDepthStencil() == false) && (dstImage.IsDepthStencil() == false));

        // Optimized image copies require a call to HwlFixupCopyDstImageMetaData...
        // Verify that any "update" operation performed is legal for the source and dest images.
        if (HwlUseOptimizedImageCopy(srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions))
        {
            pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskCopyImageOptimized);
            isFmaskCopyOptimized = true;
        }
        else
        {
            pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskCopyImage);
        }

        isFmaskCopy = true;
    }
    else
    {
        switch (srcCreateInfo.fragments)
        {
        case 2:
            pPipeline = GetPipeline(RpmComputePipeline::CopyImage2dms2x);
            break;

        case 4:
            pPipeline = GetPipeline(RpmComputePipeline::CopyImage2dms4x);
            break;

        case 8:
            pPipeline = GetPipeline(RpmComputePipeline::CopyImage2dms8x);
            break;

        default:
            if (useMipInSrd)
            {
                pPipeline = GetPipeline(RpmComputePipeline::CopyImage2d);
            }
            else
            {
                pPipeline = GetPipeline(RpmComputePipeline::CopyImage2dShaderMipLevel);
            }
            break;
        }
    }

    const bool isSrgbDst = (Formats::IsSrgb(dstCreateInfo.swizzledFormat.format) &&
                            (Formats::IsSrgb(srcCreateInfo.swizzledFormat.format) == false));
    // If the destination format is srgb and we will be doing format conversion copy then we need to use the pipeline
    // that will properly perform gamma correction. Note: If both src and dst are srgb then we'll do a raw copy and so
    // no need to change pipelines in that case.
    if (isSrgbDst && TestAnyFlagSet(flags, CopyFormatConversion))
    {
        pPipeline = GetPipeline(RpmComputePipeline::CopyImageGammaCorrect2d);
    }

    // Get number of threads per groups in each dimension, we will need this data later.
    uint32 threadsPerGroup[3] = {};
    pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

    bool p2pBltInfoRequired = m_pDevice->Parent()->IsP2pBltWaRequired(*dstImage.GetBoundGpuMemory().Memory());

    uint32 newRegionCount = 0;
    if (p2pBltInfoRequired)
    {
        m_pDevice->P2pBltWaModifyRegionListImage(srcImage,
                                                 dstImage,
                                                 regionCount,
                                                 pRegions,
                                                 &newRegionCount,
                                                 nullptr,
                                                 nullptr);
    }

    AutoBuffer<ImageCopyRegion, 32, Platform> newRegions(newRegionCount, m_pDevice->GetPlatform());
    AutoBuffer<gpusize, 32, Platform> chunkAddrs(newRegionCount, m_pDevice->GetPlatform());

    if (p2pBltInfoRequired)
    {
        if ((newRegions.Capacity() >= newRegionCount) && (chunkAddrs.Capacity() >= newRegionCount))
        {
            m_pDevice->P2pBltWaModifyRegionListImage(srcImage,
                                                     dstImage,
                                                     regionCount,
                                                     pRegions,
                                                     &newRegionCount,
                                                     &newRegions[0],
                                                     &chunkAddrs[0]);

            regionCount = newRegionCount;
            pRegions    = &newRegions[0];

            pCmdBuffer->P2pBltWaCopyBegin(dstImage.GetBoundGpuMemory().Memory(), regionCount, &chunkAddrs[0]);
        }
        else
        {
            pCmdBuffer->NotifyAllocFailure();
            p2pBltInfoRequired = false;
        }
    }

    // Save current command buffer state and bind the pipeline.
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    // Now begin processing the list of copy regions.
    for (uint32 idx = 0; idx < regionCount; ++idx)
    {
        ImageCopyRegion copyRegion = pRegions[idx];

        // When copying from 3D to 3D, the number of slices should be 1. When copying from
        // 1D to 1D or 2D to 2D, depth should be 1. Therefore when the src image type is identical
        // to the dst image type, either the depth or the number of slices should be equal to 1.
        PAL_ASSERT((srcCreateInfo.imageType != dstCreateInfo.imageType) ||
                   (copyRegion.numSlices == 1) ||
                   (copyRegion.extent.depth == 1));

#if PAL_ENABLE_PRINTS_ASSERTS
        // When copying from 2D to 3D or 3D to 2D, the number of slices should match the depth.
        if (((srcCreateInfo.imageType == ImageType::Tex3d) && (dstCreateInfo.imageType == ImageType::Tex2d)) ||
            ((srcCreateInfo.imageType == ImageType::Tex2d) && (dstCreateInfo.imageType == ImageType::Tex3d)))
        {
            PAL_ASSERT(copyRegion.numSlices == copyRegion.extent.depth);
        }
#endif

        if (p2pBltInfoRequired)
        {
            pCmdBuffer->P2pBltWaCopyNextRegion(chunkAddrs[idx]);
        }

        // Setup image formats per-region. This is different than the graphics path because the compute path must be
        // able to copy depth-stencil images.
        SwizzledFormat dstFormat    = {};
        SwizzledFormat srcFormat    = {};
        uint32         texelScale   = 1;
        bool           singleSubres = false;

        GetCopyImageFormats(srcImage,
                            srcImageLayout,
                            dstImage,
                            dstImageLayout,
                            copyRegion,
                            flags,
                            &srcFormat,
                            &dstFormat,
                            &texelScale,
                            &singleSubres);

        // The hardware can't handle UAV stores using SRGB num format.  The resolve shaders already contain a
        // linear-to-gamma conversion, but in order for that to work the output UAV's num format must be patched to be
        // simple unorm.
        if (Formats::IsSrgb(dstFormat.format))
        {
            dstFormat.format = Formats::ConvertToUnorm(dstFormat.format);
            PAL_ASSERT(Formats::IsUndefined(dstFormat.format) == false);
        }

        // Multiply all x-dimension values in our region by the texel scale.
        copyRegion.srcOffset.x  *= texelScale;
        copyRegion.dstOffset.x  *= texelScale;
        copyRegion.extent.width *= texelScale;

        RpmUtil::CopyImageInfo copyImageInfo    = {};
        copyImageInfo.srcOffset                 = copyRegion.srcOffset;
        copyImageInfo.dstOffset                 = copyRegion.dstOffset;
        copyImageInfo.numSamples                = dstCreateInfo.samples;
        copyImageInfo.packedMipData.srcMipLevel = copyRegion.srcSubres.mipLevel;
        copyImageInfo.packedMipData.dstMipLevel = copyRegion.dstSubres.mipLevel;
        copyImageInfo.copyRegion.width          = copyRegion.extent.width;
        copyImageInfo.copyRegion.height         = copyRegion.extent.height;

        // Create an embedded user-data table and bind it to user data 0. We need image views for the src and dst
        // subresources, as well as some inline constants for the copy offsets and extents.
        const uint8 numSlots = isFmaskCopy ? 3 : 2;
        uint32* pUserData = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                   (SrdDwordAlignment() * numSlots +
                                                                    RpmUtil::CopyImageInfoDwords),
                                                                   SrdDwordAlignment(),
                                                                   PipelineBindPoint::Compute,
                                                                   0);

        // When we treat 3D images as 2D arrays each z-slice must be treated as an array slice.
        const uint32 numSlices = (imageType == ImageType::Tex3d) ? copyRegion.extent.depth : copyRegion.numSlices;

        ImageViewInfo imageView[2] = {};
        SubresRange   viewRange    = { copyRegion.dstSubres, 1, numSlices };

        PAL_ASSERT(TestAnyFlagSet(dstImageLayout.usages, LayoutShaderWrite | LayoutCopyDst) == true);
        RpmUtil::BuildImageViewInfo(&imageView[0],
                                    dstImage,
                                    viewRange,
                                    dstFormat,
                                    dstImageLayout,
                                    device.TexOptLevel());

        viewRange.startSubres = copyRegion.srcSubres;
        RpmUtil::BuildImageViewInfo(&imageView[1],
                                    srcImage,
                                    viewRange,
                                    srcFormat,
                                    srcImageLayout,
                                    device.TexOptLevel());

        // The shader treats all images as 2D arrays which means we need to override the view type to 2D. We also used
        // to do this for 3D images but that caused test failures when the images used mipmaps because the HW expected
        // the "numSlices" to be constant for all mip levels (rather than halving at each mip as z-slices do).
        //
        // Is it legal for the shader to view 1D and 3D images as 2D?
        if (imageType == ImageType::Tex1d)
        {
            imageView[0].viewType = ImageViewType::Tex2d;
            imageView[1].viewType = ImageViewType::Tex2d;
        }

        if (useMipInSrd == false)
        {
            // The miplevel as specified in the shader instruction is actually an offset from the mip-level
            // as specified in the SRD.
            imageView[0].subresRange.startSubres.mipLevel = 0;  // dst
            imageView[1].subresRange.startSubres.mipLevel = 0;  // src

            // The mip-level from the instruction is also clamped to the "last level" as specified in the SRD.
            imageView[0].subresRange.numMips = copyRegion.dstSubres.mipLevel + viewRange.numMips;
            imageView[1].subresRange.numMips = copyRegion.srcSubres.mipLevel + viewRange.numMips;
        }

        PAL_ASSERT(singleSubres == false);

        // Turn our image views into HW SRDs here
        device.CreateImageViewSrds(2, &imageView[0], pUserData);
        pUserData += SrdDwordAlignment() * 2;

        if (isFmaskCopy)
        {
            // If this is an Fmask-accelerated Copy, create an image view of the source Image's Fmask surface.
            FmaskViewInfo fmaskView = {};
            fmaskView.pImage         = &srcImage;
            fmaskView.baseArraySlice = copyRegion.srcSubres.arraySlice;
            fmaskView.arraySize      = copyRegion.numSlices;

            m_pDevice->Parent()->CreateFmaskViewSrds(1, &fmaskView, pUserData);
            pUserData += SrdDwordAlignment();
        }

        // Copy the copy parameters into the embedded user-data space
        memcpy(pUserData, &copyImageInfo, sizeof(copyImageInfo));

        // Execute the dispatch, we need one thread per texel.
        pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroups(copyRegion.extent.width,  threadsPerGroup[0]),
                                RpmUtil::MinThreadGroups(copyRegion.extent.height, threadsPerGroup[1]),
                                RpmUtil::MinThreadGroups(numSlices,                threadsPerGroup[2]));
    }

    if (p2pBltInfoRequired)
    {
        pCmdBuffer->P2pBltWaCopyEnd();
    }

    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);

    if (isFmaskCopyOptimized
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 562
        || (dstImage.GetImageCreateInfo().flags.fullCopyDstOnly != 0)
#endif
        )
    {
        // If this is MSAA copy optimized we might have to update destination image meta data.
        // If image is created with fullCopyDstOnly=1, there will be no expand when transition to "LayoutCopyDst"; if
        // the copy isn't compressed copy, need fix up dst metadata to uncompressed state.
        const Pal::Image* pSrcImage = isFmaskCopyOptimized ? &srcImage : nullptr;
        AutoBuffer<ImageFixupRegion, 32, Platform> fixupRegions(regionCount, m_pDevice->GetPlatform());

        if (fixupRegions.Capacity() >= regionCount)
        {
            for (uint32 i = 0; i < regionCount; i++)
            {
                fixupRegions[i].subres    = pRegions[i].dstSubres;
                fixupRegions[i].offset    = pRegions[i].dstOffset;
                fixupRegions[i].extent    = pRegions[i].extent;
                fixupRegions[i].numSlices = pRegions[i].numSlices;
            }
            HwlFixupCopyDstImageMetaData(pCmdBuffer, pSrcImage, dstImage, dstImageLayout,
                                         &fixupRegions[0], regionCount, isFmaskCopyOptimized);
        }
        else
        {
            pCmdBuffer->NotifyAllocFailure();
        }
    }
}

// =====================================================================================================================
// Picks a source format and a destination format for an image-to-image copy.
template <typename CopyRegion>
void RsrcProcMgr::GetCopyImageFormats(
    const Image&      srcImage,
    ImageLayout       srcImageLayout,
    const Image&      dstImage,
    ImageLayout       dstImageLayout,
    const CopyRegion& copyRegion,
    uint32            copyFlags,
    SwizzledFormat*   pSrcFormat,     // [out] Read from the source image using this format.
    SwizzledFormat*   pDstFormat,     // [out] Read from the destination image using this format.
    uint32*           pTexelScale,    // [out] Each texel requires this many raw format texels in the X dimension.
    bool*             pSingleSubres   // [out] Format requires that you access each subres independantly.
    ) const
{
    const auto& device        = *m_pDevice->Parent();
    const auto& srcCreateInfo = srcImage.GetImageCreateInfo();
    const auto& dstCreateInfo = dstImage.GetImageCreateInfo();

    // Begin with each subresource's native format.
    SwizzledFormat srcFormat = srcImage.SubresourceInfo(copyRegion.srcSubres)->format;
    SwizzledFormat dstFormat = dstImage.SubresourceInfo(copyRegion.dstSubres)->format;

    const bool isSrcFormatReplaceable = srcImage.GetGfxImage()->IsFormatReplaceable(copyRegion.srcSubres,
                                                                                    srcImageLayout,
                                                                                    false);
    const bool isDstFormatReplaceable = dstImage.GetGfxImage()->IsFormatReplaceable(copyRegion.dstSubres,
                                                                                    dstImageLayout,
                                                                                    true);

    const bool chFmtsMatch    = Formats::ShareChFmt(srcFormat.format, dstFormat.format);
    const bool formatsMatch   = (srcFormat.format == dstFormat.format) &&
                                (srcFormat.swizzle.swizzleValue == dstFormat.swizzle.swizzleValue);
    const bool isMmFormatUsed = (Formats::IsMmFormat(srcFormat.format) || Formats::IsMmFormat(dstFormat.format));

    // Both formats must have the same pixel size.
    PAL_ASSERT(Formats::BitsPerPixel(srcFormat.format) == Formats::BitsPerPixel(dstFormat.format));

    // Initialize the texel scale to 1, it will be modified later if necessary.
    *pTexelScale = 1;

    // First, determine if we must follow conversion copy rules.
    if (TestAnyFlagSet(copyFlags, CopyFormatConversion)                            &&
        device.SupportsFormatConversionSrc(srcFormat.format, srcCreateInfo.tiling) &&
        device.SupportsFormatConversionDst(dstFormat.format, dstCreateInfo.tiling))
    {
        // Eventhough we're supposed to do a conversion copy, it will be faster if we can get away with a raw copy.
        // It will be safe to do a raw copy if the formats match and the target subresources support format replacement.
        if (formatsMatch && isSrcFormatReplaceable && isDstFormatReplaceable)
        {
            srcFormat = RpmUtil::GetRawFormat(srcFormat.format, pTexelScale, pSingleSubres);
            dstFormat = srcFormat;
        }
    }
    else
    {
        // We will be doing some sort of raw copy.
        //
        // Our copy shaders and hardware treat sRGB and UNORM nearly identically, the only difference being that the
        // hardware modifies sRGB data when reading it and can't write it, which will make it hard to do a raw copy.
        // We can avoid that problem by simply forcing sRGB to UNORM.
        if (Formats::IsSrgb(srcFormat.format))
        {
            srcFormat.format = Formats::ConvertToUnorm(srcFormat.format);
        }

        if (Formats::IsSrgb(dstFormat.format))
        {
            dstFormat.format = Formats::ConvertToUnorm(dstFormat.format);
        }

        // Due to hardware-specific compression modes, some image subresources might not support format replacement.
        // Note that the code above can force sRGB to UNORM even if format replacement is not supported because sRGB
        // values use the same bit representation as UNORM values, they just use a different color space.
        if (isSrcFormatReplaceable && isDstFormatReplaceable)
        {
            // We should do a raw copy that respects channel swizzling if the flag is set and the channel formats
            // don't match. The process is simple: keep the channel formats and try to find a single numeric format
            // that fits both of them.
            bool foundSwizzleFormats = false;

            if (TestAnyFlagSet(copyFlags, CopyRawSwizzle) && (chFmtsMatch == false))
            {
                typedef ChNumFormat(PAL_STDCALL *FormatConversion)(ChNumFormat);

                constexpr uint32 NumNumericFormats = 3;
                constexpr FormatConversion FormatConversionFuncs[NumNumericFormats] =
                {
                    &Formats::ConvertToUint,
                    &Formats::ConvertToUnorm,
                    &Formats::ConvertToFloat,
                };

                for (uint32 idx = 0; idx < NumNumericFormats; ++idx)
                {
                    ChNumFormat tempSrcFmt = srcFormat.format;
                    ChNumFormat tempDstFmt = dstFormat.format;

                    tempSrcFmt = FormatConversionFuncs[idx](tempSrcFmt);
                    tempDstFmt = FormatConversionFuncs[idx](tempDstFmt);

                    if ((Formats::IsUndefined(tempSrcFmt) == false)           &&
                        (Formats::IsUndefined(tempDstFmt) == false)           &&
                        device.SupportsCopy(tempSrcFmt, srcCreateInfo.tiling) &&
                        device.SupportsCopy(tempDstFmt, dstCreateInfo.tiling))
                    {
                        foundSwizzleFormats = true;
                        srcFormat.format    = tempSrcFmt;
                        dstFormat.format    = tempDstFmt;
                        break;
                    }
                }
            }

            // If we either didn't try to find swizzling formats or weren't able to do so, execute a true raw copy.
            if (foundSwizzleFormats == false)
            {
                srcFormat = RpmUtil::GetRawFormat(srcFormat.format, pTexelScale, pSingleSubres);
                dstFormat = srcFormat;
            }
        }
        // If one format is deemed "not replaceable" that means it may possibly be compressed. However,
        // if it is compressed, it doesn't necessarily mean it's not replaceable. If we don't do a replacement,
        // copying from one format to another may cause corruption, so we will arbitrarily choose to replace
        // the source if the channels within the format match and it is not an MM format. MM formats cannot be
        // replaced or HW will convert the data to the format's black or white which is different for MM formats.
        else if ((isSrcFormatReplaceable && (isDstFormatReplaceable == false)) ||
                 (chFmtsMatch && (isMmFormatUsed == false)))
        {
            // We can replace the source format but not the destination format. This means that we must interpret
            // the source subresource using the destination numeric format. We should keep the original source
            // channel format if a swizzle copy was requested and is possible.
            srcFormat.format = Formats::ConvertToDstNumFmt(srcFormat.format, dstFormat.format);

            if ((TestAnyFlagSet(copyFlags, CopyRawSwizzle) == false) ||
                (device.SupportsCopy(srcFormat.format, srcCreateInfo.tiling) == false))
            {
                srcFormat = dstFormat;
            }
        }
        else if ((isSrcFormatReplaceable == false) && isDstFormatReplaceable)
        {
            // We can replace the destination format but not the source format. This means that we must interpret
            // the destination subresource using the source numeric format. We should keep the original destination
            // channel format if a swizzle copy was requested and is possible.
            dstFormat.format = Formats::ConvertToDstNumFmt(dstFormat.format, srcFormat.format);

            if ((TestAnyFlagSet(copyFlags, CopyRawSwizzle) == false) ||
                (device.SupportsCopy(dstFormat.format, dstCreateInfo.tiling) == false))
            {
                dstFormat = srcFormat;
            }
        }
        else
        {
            // We can't replace either format, both formats must match. Or the channels must match in the case of
            // an MM copy.
            PAL_ASSERT(formatsMatch || (chFmtsMatch && isMmFormatUsed));
        }
    }

    // We've settled on a pair of formats, make sure that we can actually use them.
    PAL_ASSERT(device.SupportsImageRead(srcFormat.format, srcCreateInfo.tiling));
    // We have specific code to handle srgb destination by treating it as unorm and handling gamma correction
    // manually. So it's ok to ignore SRGB for this assert.
    PAL_ASSERT(Formats::IsSrgb(dstFormat.format) ||
        (device.SupportsImageWrite(dstFormat.format, dstCreateInfo.tiling)));

    *pSrcFormat = srcFormat;
    *pDstFormat = dstFormat;
}

// =====================================================================================================================
// Builds commands to copy one or more regions from a GPU memory location to an image.
void RsrcProcMgr::CmdCopyMemoryToImage(
    GfxCmdBuffer*                pCmdBuffer,
    const GpuMemory&             srcGpuMemory,
    const Image&                 dstImage,
    ImageLayout                  dstImageLayout,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions,
    bool                         includePadding
    ) const
{
    // Select the appropriate pipeline for this copy based on the destination image's properties.
    const auto& createInfo = dstImage.GetImageCreateInfo();
    const ComputePipeline* pPipeline = nullptr;

    switch (dstImage.GetGfxImage()->GetOverrideImageType())
    {
    case ImageType::Tex1d:
        pPipeline = GetPipeline(RpmComputePipeline::CopyMemToImg1d);
        break;

    case ImageType::Tex2d:
        switch (createInfo.fragments)
        {
        case 2:
            pPipeline = GetPipeline(RpmComputePipeline::CopyMemToImg2dms2x);
            break;

        case 4:
            pPipeline = GetPipeline(RpmComputePipeline::CopyMemToImg2dms4x);
            break;

        case 8:
            pPipeline = GetPipeline(RpmComputePipeline::CopyMemToImg2dms8x);
            break;

        default:
            pPipeline = GetPipeline(RpmComputePipeline::CopyMemToImg2d);
            break;
        }
        break;

    default:
        pPipeline = GetPipeline(RpmComputePipeline::CopyMemToImg3d);
        break;
    }

    // Note that we must call this helper function before and after our compute blit to fix up our image's metadata
    // if the copy isn't compatible with our layout's metadata compression level.
    AutoBuffer<ImageFixupRegion, 32, Platform> fixupRegions(regionCount, m_pDevice->GetPlatform());
    if (fixupRegions.Capacity() >= regionCount)
    {
        for (uint32 i = 0; i < regionCount; i++)
        {
            fixupRegions[i].subres    = pRegions[i].imageSubres;
            fixupRegions[i].offset    = pRegions[i].imageOffset;
            fixupRegions[i].extent    = pRegions[i].imageExtent;
            fixupRegions[i].numSlices = pRegions[i].numSlices;
        }
        FixupMetadataForComputeDst(pCmdBuffer, dstImage, dstImageLayout, regionCount, &fixupRegions[0], true);

        CopyBetweenMemoryAndImage(pCmdBuffer, pPipeline, srcGpuMemory, dstImage, dstImageLayout, true, false,
                                  regionCount, pRegions, includePadding);

        FixupMetadataForComputeDst(pCmdBuffer, dstImage, dstImageLayout, regionCount, &fixupRegions[0], false);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 562
        // If image is created with fullCopyDstOnly=1, there will be no expand when transition to "LayoutCopyDst"; if
        // the copy isn't compressed copy, need fix up dst metadata to uncompressed state.
        if (dstImage.GetImageCreateInfo().flags.fullCopyDstOnly != 0)
        {
            HwlFixupCopyDstImageMetaData(pCmdBuffer, nullptr, dstImage, dstImageLayout,
                                         &fixupRegions[0], regionCount, false);
        }
#endif
    }
    else
    {
        pCmdBuffer->NotifyAllocFailure();
    }
}

// =====================================================================================================================
// Builds commands to copy one or more regions from an image to a GPU memory location.
void RsrcProcMgr::CmdCopyImageToMemory(
    GfxCmdBuffer*                pCmdBuffer,
    const Image&                 srcImage,
    ImageLayout                  srcImageLayout,
    const GpuMemory&             dstGpuMemory,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions,
    bool                         includePadding
    ) const
{
    // Select the appropriate pipeline for this copy based on the source image's properties.
    const auto&            createInfo = srcImage.GetImageCreateInfo();
    const bool             isEqaaSrc  = (createInfo.samples != createInfo.fragments);
    const GfxImage*        pGfxImage  = srcImage.GetGfxImage();
    const ComputePipeline* pPipeline  = nullptr;

    bool isFmaskCopy = false;

    switch (pGfxImage->GetOverrideImageType())
    {
    case ImageType::Tex1d:
        pPipeline = GetPipeline(RpmComputePipeline::CopyImgToMem1d);
        break;

    case ImageType::Tex2d:
        // The Fmask accelerated copy should be used in all non-EQAA cases where Fmask is enabled. There is no use case
        // Fmask accelerated EQAA copy and it would require several new shaders. It can be implemented at a future
        // point if required.
        if (pGfxImage->HasFmaskData() && isEqaaSrc)
        {
            PAL_NOT_IMPLEMENTED();
        }
        if (pGfxImage->HasFmaskData() && (isEqaaSrc == false))
        {
            PAL_ASSERT((srcImage.IsDepthStencil() == false) && (createInfo.fragments > 1));
            pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskCopyImgToMem);
            isFmaskCopy = true;
        }
        else
        {
            switch (createInfo.fragments)
            {
            case 2:
                pPipeline = GetPipeline(RpmComputePipeline::CopyImgToMem2dms2x);
                break;

            case 4:
                pPipeline = GetPipeline(RpmComputePipeline::CopyImgToMem2dms4x);
                break;

            case 8:
                pPipeline = GetPipeline(RpmComputePipeline::CopyImgToMem2dms8x);
                break;

            default:
                pPipeline = GetPipeline(RpmComputePipeline::CopyImgToMem2d);
                break;
            }
        }
        break;

    default:
        pPipeline = GetPipeline(RpmComputePipeline::CopyImgToMem3d);
        break;
    }

    CopyBetweenMemoryAndImage(pCmdBuffer,
                              pPipeline,
                              dstGpuMemory,
                              srcImage,
                              srcImageLayout,
                              false,
                              isFmaskCopy,
                              regionCount,
                              pRegions,
                              includePadding);
}

// =====================================================================================================================
// Builds commands to copy one or more regions between an image and a GPU memory location. Which object is the source
// and which object is the destination is determined by the given pipeline. This works because the image <-> memory
// pipelines all have the same input layouts.
void RsrcProcMgr::CopyBetweenMemoryAndImage(
    GfxCmdBuffer*                pCmdBuffer,
    const ComputePipeline*       pPipeline,
    const GpuMemory&             gpuMemory,
    const Image&                 image,
    ImageLayout                  imageLayout,
    bool                         isImageDst,
    bool                         isFmaskCopy,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions,
    bool                         includePadding
    ) const
{
    const auto& imgCreateInfo = image.GetImageCreateInfo();
    const auto& device        = *m_pDevice->Parent();
    const auto& settings      = device.Settings();
    const bool  is3d          = (imgCreateInfo.imageType == ImageType::Tex3d);

    // Get number of threads per groups in each dimension, we will need this data later.
    uint32 threadsPerGroup[3] = {};
    pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

    bool p2pBltInfoRequired =
        m_pDevice->Parent()->IsP2pBltWaRequired(isImageDst ? *image.GetBoundGpuMemory().Memory() : gpuMemory);
    uint32 newRegionCount = 0;
    if (p2pBltInfoRequired)
    {
        if (isImageDst)
        {
            m_pDevice->P2pBltWaModifyRegionListMemoryToImage(gpuMemory,
                                                             image,
                                                             regionCount,
                                                             pRegions,
                                                             &newRegionCount,
                                                             nullptr,
                                                             nullptr);
        }
        else
        {
            m_pDevice->P2pBltWaModifyRegionListImageToMemory(image,
                                                             gpuMemory,
                                                             regionCount,
                                                             pRegions,
                                                             &newRegionCount,
                                                             nullptr,
                                                             nullptr);
        }
    }

    AutoBuffer<MemoryImageCopyRegion, 32, Platform> newRegions(newRegionCount, m_pDevice->GetPlatform());
    AutoBuffer<gpusize, 32, Platform> chunkAddrs(newRegionCount, m_pDevice->GetPlatform());

    if (p2pBltInfoRequired)
    {
        if ((newRegions.Capacity() >= newRegionCount) && (chunkAddrs.Capacity() >= newRegionCount))
        {
            if (isImageDst)
            {
                m_pDevice->P2pBltWaModifyRegionListMemoryToImage(gpuMemory,
                                                                 image,
                                                                 regionCount,
                                                                 pRegions,
                                                                 &newRegionCount,
                                                                 &newRegions[0],
                                                                 &chunkAddrs[0]);
            }
            else
            {
                m_pDevice->P2pBltWaModifyRegionListImageToMemory(image,
                                                                 gpuMemory,
                                                                 regionCount,
                                                                 pRegions,
                                                                 &newRegionCount,
                                                                 &newRegions[0],
                                                                 &chunkAddrs[0]);
            }

            regionCount = newRegionCount;
            pRegions    = &newRegions[0];

            const GpuMemory* pDstGpuMemory = isImageDst ? image.GetBoundGpuMemory().Memory() : &gpuMemory;
            pCmdBuffer->P2pBltWaCopyBegin(pDstGpuMemory, regionCount, &chunkAddrs[0]);
        }
        else
        {
            pCmdBuffer->NotifyAllocFailure();
            p2pBltInfoRequired = false;
        }
    }

    // Save current command buffer state and bind the pipeline.
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    // Now begin processing the list of copy regions.
    for (uint32 idx = 0; idx < regionCount; ++idx)
    {
        MemoryImageCopyRegion copyRegion = pRegions[idx];

        // 3D images can't have slices and non-3D images shouldn't specify depth > 1 so we expect at least one
        // of them to be set to 1.
        PAL_ASSERT((copyRegion.numSlices == 1) || (copyRegion.imageExtent.depth == 1));

        if (p2pBltInfoRequired)
        {
            pCmdBuffer->P2pBltWaCopyNextRegion(chunkAddrs[idx]);
        }

        // It will be faster to use a raw format, but we must stick with the base format if replacement isn't an option.
        SwizzledFormat    viewFormat = image.SubresourceInfo(copyRegion.imageSubres)->format;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 583
        if (Formats::IsUndefined(copyRegion.swizzledFormat.format) == false)
        {
            viewFormat = copyRegion.swizzledFormat;
        }
#endif

        const ImageTiling srcTiling  = (isImageDst) ? ImageTiling::Linear : imgCreateInfo.tiling;

        // Our copy shaders and hardware treat sRGB and UNORM nearly identically, the only difference being that the
        // hardware modifies sRGB data when reading it and can't write it, which will make it hard to do a raw copy.
        // We can avoid that problem by simply forcing sRGB to UNORM.
        if (Formats::IsSrgb(viewFormat.format))
        {
            viewFormat.format = Formats::ConvertToUnorm(viewFormat.format);
            PAL_ASSERT(Formats::IsUndefined(viewFormat.format) == false);
        }

        bool singleSubres = false;
        if (image.GetGfxImage()->IsFormatReplaceable(copyRegion.imageSubres, imageLayout, isImageDst) ||
            (m_pDevice->Parent()->SupportsMemoryViewRead(viewFormat.format, srcTiling) == false))
        {
            uint32 texelScale     = 1;
            uint32 pixelsPerBlock = 1;
            if (m_pDevice->IsImageFormatOverrideNeeded(imgCreateInfo, &viewFormat.format, &pixelsPerBlock))
            {
                copyRegion.imageOffset.x     /= pixelsPerBlock;
                copyRegion.imageExtent.width /= pixelsPerBlock;
            }
            else
            {
                viewFormat = RpmUtil::GetRawFormat(viewFormat.format, &texelScale, &singleSubres);
                copyRegion.imageOffset.x     *= texelScale;
                copyRegion.imageExtent.width *= texelScale;
            }
            // If the format is not supported by the buffer SRD (checked with SupportsMemoryViewRead() above)
            // and the compression state check above (i.e., IsFormatReplaceable()) returns false, the
            // format is still replaced but a corruption may occur. The corruption can occur if the format
            // replacement results in a change in the color channel width and the resource is compressed.
            // Cover this with an assert for now.
            PAL_ASSERT(image.GetGfxImage()->IsFormatReplaceable(copyRegion.imageSubres, imageLayout, isImageDst)
                       == true);
        }

        // Make sure our view format supports reads and writes.
        PAL_ASSERT(device.SupportsImageWrite(viewFormat.format, imgCreateInfo.tiling) &&
                   device.SupportsImageRead(viewFormat.format, imgCreateInfo.tiling));

        // The row and depth pitches need to be expressed in terms of view format texels.
        const uint32 viewBpp    = Formats::BytesPerPixel(viewFormat.format);
        const uint32 rowPitch   = static_cast<uint32>(copyRegion.gpuMemoryRowPitch   / viewBpp);
        const uint32 depthPitch = static_cast<uint32>(copyRegion.gpuMemoryDepthPitch / viewBpp);

        // Generally the pipeline expects the user data to be arranged as follows for each dispatch:
        // Img X offset, Img Y offset, Img Z offset (3D), row pitch
        // Copy width, Copy height, Copy depth, slice pitch
        uint32 copyData[8] =
        {
            static_cast<uint32>(copyRegion.imageOffset.x),
            static_cast<uint32>(copyRegion.imageOffset.y),
            static_cast<uint32>(copyRegion.imageOffset.z),
            rowPitch,
            copyRegion.imageExtent.width,
            copyRegion.imageExtent.height,
            copyRegion.imageExtent.depth,
            depthPitch
        };

        // For fmask accelerated copy, the pipeline expects the user data to be arranged as below,
        // Img X offset, Img Y offset, samples, row pitch
        // Copy width, Copy height, Copy depth, slice pitch
        if (isFmaskCopy)
        {
            // Img Z offset doesn't make sense for msaa image; store numSamples instead.
            copyData[2] = imgCreateInfo.samples;
        }

        // Create an embedded user-data table to contain the Image SRD's and the copy data constants. It will be bound
        // to entry 0.
        const uint32 DataDwords = NumBytesToNumDwords(sizeof(copyData));
        uint32*      pUserData  = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                         SrdDwordAlignment() * 2 + DataDwords,
                                                                         SrdDwordAlignment(),
                                                                         PipelineBindPoint::Compute,
                                                                         0);

        const Extent3d bufferBox =
        {
            copyRegion.imageExtent.width,
            copyRegion.imageExtent.height,
            is3d ? copyRegion.imageExtent.depth : copyRegion.numSlices
        };

        BufferViewInfo bufferView = {};
        bufferView.gpuAddr        = gpuMemory.Desc().gpuVirtAddr + copyRegion.gpuMemoryOffset;
        bufferView.swizzledFormat = viewFormat;
        bufferView.stride         = viewBpp;
        bufferView.range          = ComputeTypedBufferRange(bufferBox,
                                                            viewBpp * imgCreateInfo.fragments,
                                                            copyRegion.gpuMemoryRowPitch,
                                                            copyRegion.gpuMemoryDepthPitch);

        device.CreateTypedBufferViewSrds(1, &bufferView, pUserData);
        pUserData += SrdDwordAlignment();

        ImageViewInfo     imageView      = {};
        const SubresRange viewRange      = { copyRegion.imageSubres, 1, copyRegion.numSlices };
        const uint32      firstMipLevel  = copyRegion.imageSubres.mipLevel;
        const uint32      lastArraySlice = copyRegion.imageSubres.arraySlice + copyRegion.numSlices - 1;

        // If single subres is requested for the format, iterate slice-by-slice and mip-by-mip.
        if (singleSubres)
        {
            copyRegion.numSlices = 1;
        }

        if (isImageDst)
        {
            PAL_ASSERT(TestAnyFlagSet(imageLayout.usages, LayoutShaderWrite | LayoutCopyDst) == true);
        }

        for (;
            copyRegion.imageSubres.arraySlice <= lastArraySlice;
            copyRegion.imageSubres.arraySlice += copyRegion.numSlices)
        {
            copyRegion.imageSubres.mipLevel = firstMipLevel;

            RpmUtil::BuildImageViewInfo(&imageView,
                                        image,
                                        viewRange,
                                        viewFormat,
                                        imageLayout,
                                        device.TexOptLevel());
            imageView.flags.includePadding = includePadding;

            device.CreateImageViewSrds(1, &imageView, pUserData);
            pUserData += SrdDwordAlignment();

            if (isFmaskCopy)
            {
                // If this is an Fmask-accelerated Copy, create an image view of the source Image's Fmask surface.
                FmaskViewInfo fmaskView = {};
                fmaskView.pImage         = &image;
                fmaskView.baseArraySlice = copyRegion.imageSubres.arraySlice;
                fmaskView.arraySize      = copyRegion.numSlices;

                m_pDevice->Parent()->CreateFmaskViewSrds(1, &fmaskView, pUserData);
                pUserData += SrdDwordAlignment();
            }

            // Copy the copy data into the embedded user data memory.
            memcpy(pUserData, &copyData[0], sizeof(copyData));

            // Execute the dispatch, we need one thread per texel.
            pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroups(bufferBox.width,  threadsPerGroup[0]),
                                    RpmUtil::MinThreadGroups(bufferBox.height, threadsPerGroup[1]),
                                    RpmUtil::MinThreadGroups(bufferBox.depth,  threadsPerGroup[2]));
        }
    }

    if (p2pBltInfoRequired)
    {
        pCmdBuffer->P2pBltWaCopyEnd();
    }

    // Restore command buffer state.
    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
// Builds commands to copy multiple regions directly (without format conversion) from one typed buffer to another.
void RsrcProcMgr::CmdCopyTypedBuffer(
    GfxCmdBuffer*                pCmdBuffer,
    const GpuMemory&             srcGpuMemory,
    const GpuMemory&             dstGpuMemory,
    uint32                       regionCount,
    const TypedBufferCopyRegion* pRegions
    ) const
{
    const auto& device   = *m_pDevice->Parent();
    const auto& settings = device.Settings();

    // Save current command buffer state.
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

    // We may have to bind a new pipeline for each region, we can optimize out redundant binds by tracking the previous
    // pipeline and only updating the pipeline binding when it must change.
    const ComputePipeline* pPipeline          = nullptr;
    const ComputePipeline* pPrevPipeline      = nullptr;
    uint32                 threadsPerGroup[3] = {};

    // Now begin processing the list of copy regions.
    for (uint32 idx = 0; idx < regionCount; ++idx)
    {
        const TypedBufferInfo& srcInfo = pRegions[idx].srcBuffer;
        const TypedBufferInfo& dstInfo = pRegions[idx].dstBuffer;

        // Both buffers must have the same pixel size.
        PAL_ASSERT(Formats::BitsPerPixel(srcInfo.swizzledFormat.format) ==
                   Formats::BitsPerPixel(dstInfo.swizzledFormat.format));

        // Pick a raw format for the copy.
        uint32               texelScale = 1;
        const SwizzledFormat rawFormat  = RpmUtil::GetRawFormat(srcInfo.swizzledFormat.format, &texelScale, nullptr);

        // Multiply 'texelScale' into our extent to make sure we dispatch enough threads to copy the whole region.
        const Extent3d copyExtent =
        {
            pRegions[idx].extent.width * texelScale,
            pRegions[idx].extent.height,
            pRegions[idx].extent.depth
        };

        // The row and depth pitches need to be expressed in terms of raw format texels.
        const uint32 rawBpp        = Formats::BytesPerPixel(rawFormat.format);
        const uint32 dstRowPitch   = static_cast<uint32>(dstInfo.rowPitch   / rawBpp);
        const uint32 dstDepthPitch = static_cast<uint32>(dstInfo.depthPitch / rawBpp);
        const uint32 srcRowPitch   = static_cast<uint32>(srcInfo.rowPitch   / rawBpp);
        const uint32 srcDepthPitch = static_cast<uint32>(srcInfo.depthPitch / rawBpp);

        // Get the appropriate pipeline and user data based on the copy extents.
        uint32 userData[7] = {};
        uint32 numUserData = 0;

        if (copyExtent.depth > 1)
        {
            pPipeline   = GetPipeline(RpmComputePipeline::CopyTypedBuffer3d);
            userData[0] = dstRowPitch;
            userData[1] = dstDepthPitch;
            userData[2] = srcRowPitch;
            userData[3] = srcDepthPitch;
            userData[4] = copyExtent.width;
            userData[5] = copyExtent.height;
            userData[6] = copyExtent.depth;
            numUserData = 7;
        }
        else if (copyExtent.height > 1)
        {
            pPipeline   = GetPipeline(RpmComputePipeline::CopyTypedBuffer2d);
            userData[0] = dstRowPitch;
            userData[1] = srcRowPitch;
            userData[2] = copyExtent.width;
            userData[3] = copyExtent.height;
            numUserData = 4;
        }
        else
        {
            pPipeline   = GetPipeline(RpmComputePipeline::CopyTypedBuffer1d);
            userData[0] = copyExtent.width;
            numUserData = 1;
        }

        PAL_ASSERT(pPipeline != nullptr);

        // Change pipeline bindings if necessary.
        if (pPrevPipeline != pPipeline)
        {
            pPrevPipeline = pPipeline;
            pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);
            pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });
        }

        // Create an embedded user-data table and bind it to user data 0. We need buffer views for the src and dst
        // subresources, as well as some inline constants for our inline constant user data.
        uint32* pUserDataTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                        SrdDwordAlignment() * 2 + numUserData,
                                                                        SrdDwordAlignment(),
                                                                        PipelineBindPoint::Compute,
                                                                        0);

        BufferViewInfo bufferView = {};
        bufferView.gpuAddr        = dstGpuMemory.Desc().gpuVirtAddr + dstInfo.offset;
        bufferView.range          = ComputeTypedBufferRange(copyExtent, rawBpp, dstInfo.rowPitch, dstInfo.depthPitch);
        bufferView.stride         = rawBpp;
        bufferView.swizzledFormat = rawFormat;

        device.CreateTypedBufferViewSrds(1, &bufferView, pUserDataTable);
        pUserDataTable += SrdDwordAlignment();

        bufferView.gpuAddr        = srcGpuMemory.Desc().gpuVirtAddr + srcInfo.offset;
        bufferView.range          = ComputeTypedBufferRange(copyExtent, rawBpp, srcInfo.rowPitch, srcInfo.depthPitch);

        device.CreateTypedBufferViewSrds(1, &bufferView, pUserDataTable);
        pUserDataTable += SrdDwordAlignment();

        // Copy the copy parameters into the embedded user-data space.
        memcpy(pUserDataTable, userData, numUserData * sizeof(uint32));

        // Execute the dispatch, we need one thread per texel.
        pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroups(copyExtent.width,  threadsPerGroup[0]),
                                RpmUtil::MinThreadGroups(copyExtent.height, threadsPerGroup[1]),
                                RpmUtil::MinThreadGroups(copyExtent.depth,  threadsPerGroup[2]));
    }

    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
bool RsrcProcMgr::ScaledCopyImageUseGraphics(
    GfxCmdBuffer*           pCmdBuffer,
    const ScaledCopyInfo&   copyInfo) const
{
    const auto&      srcInfo      = copyInfo.pSrcImage->GetImageCreateInfo();
    const auto&      dstInfo      = copyInfo.pDstImage->GetImageCreateInfo();
    const auto*      pDstImage    = static_cast<const Image*>(copyInfo.pDstImage);
    const ImageType  srcImageType = srcInfo.imageType;
    const ImageType  dstImageType = dstInfo.imageType;

    const bool isDepth      = ((srcInfo.usageFlags.depthStencil != 0) ||
                               (dstInfo.usageFlags.depthStencil != 0) ||
                               Formats::IsDepthStencilOnly(srcInfo.swizzledFormat.format) ||
                               Formats::IsDepthStencilOnly(dstInfo.swizzledFormat.format));
    const bool isCompressed = (Formats::IsBlockCompressed(srcInfo.swizzledFormat.format) ||
                               Formats::IsBlockCompressed(dstInfo.swizzledFormat.format));
    const bool isYuv        = (Formats::IsYuv(srcInfo.swizzledFormat.format) ||
                               Formats::IsYuv(dstInfo.swizzledFormat.format));
    const bool p2pBltWa     = m_pDevice->Parent()->ChipProperties().p2pBltWaInfo.required &&
                              pDstImage->GetBoundGpuMemory().Memory()->AccessesPeerMemory();

    const bool preferGraphicsCopy = Image::PreferGraphicsCopy &&
                                    (PreferComputeForNonLocalDestCopy(*pDstImage) == false);

    // We need to decide between the graphics copy path and the compute copy path. The graphics path only supports
    // single-sampled non-compressed, non-YUV 2D or 2D color images for now.
    const bool useGraphicsCopy = ((preferGraphicsCopy && pCmdBuffer->IsGraphicsSupported()) &&
                                  ((srcImageType != ImageType::Tex1d) &&
                                   (dstImageType != ImageType::Tex1d) &&
                                   (isCompressed == false)            &&
                                   (isYuv == false)                   &&
                                   (isDepth == false)                 &&
                                   (p2pBltWa == false)));

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 603
    // Scissor-enabled blit for OGLP is only supported on graphics path.
    PAL_ASSERT(useGraphicsCopy || (copyInfo.flags.scissorTest == 0));
#endif

    return useGraphicsCopy;
}

// =====================================================================================================================
void RsrcProcMgr::CmdScaledCopyImage(
    GfxCmdBuffer*           pCmdBuffer,
    const ScaledCopyInfo&   copyInfo) const
{
    const bool useGraphicsCopy = ScaledCopyImageUseGraphics(pCmdBuffer, copyInfo);

    if (useGraphicsCopy)
    {
        // Save current command buffer state.
        pCmdBuffer->PushGraphicsState();
        ScaledCopyImageGraphics(pCmdBuffer, copyInfo);
        // Restore original command buffer state.
        pCmdBuffer->PopGraphicsState();
    }
    else
    {
        // Note that we must call this helper function before and after our compute blit to fix up our image's
        // metadata if the copy isn't compatible with our layout's metadata compression level.
        const Image& dstImage = *static_cast<const Image*>(copyInfo.pDstImage);
        AutoBuffer<ImageFixupRegion, 32, Platform> fixupRegions(copyInfo.regionCount, m_pDevice->GetPlatform());
        if (fixupRegions.Capacity() >= copyInfo.regionCount)
        {
            for (uint32 i = 0; i < copyInfo.regionCount; i++)
            {
                fixupRegions[i].subres        = copyInfo.pRegions[i].dstSubres;
                fixupRegions[i].offset        = copyInfo.pRegions[i].dstOffset;
                fixupRegions[i].extent.width  = Math::Absu(copyInfo.pRegions[i].dstExtent.width);
                fixupRegions[i].extent.height = Math::Absu(copyInfo.pRegions[i].dstExtent.height);
                fixupRegions[i].extent.depth  = Math::Absu(copyInfo.pRegions[i].dstExtent.depth);
                fixupRegions[i].numSlices     = copyInfo.pRegions[i].numSlices;
            }
            FixupMetadataForComputeDst(pCmdBuffer, dstImage, copyInfo.dstImageLayout,
                                       copyInfo.regionCount, &fixupRegions[0], true);

            // Save current command buffer state and bind the pipeline.
            pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
            ScaledCopyImageCompute(pCmdBuffer, copyInfo);
            pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);

            FixupMetadataForComputeDst(pCmdBuffer, dstImage, copyInfo.dstImageLayout,
                                       copyInfo.regionCount, &fixupRegions[0], false);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 562
            // If image is created with fullCopyDstOnly=1, there will be no expand when transition to
            // "LayoutCopyDst"; if the copy isn't compressed copy, need fix up dst metadata to uncompressed state.
            if (copyInfo.pDstImage->GetImageCreateInfo().flags.fullCopyDstOnly != 0)
            {
                HwlFixupCopyDstImageMetaData(pCmdBuffer, nullptr, dstImage, copyInfo.dstImageLayout,
                                             &fixupRegions[0], copyInfo.regionCount, false);
            }
#endif
        }
        else
        {
            pCmdBuffer->NotifyAllocFailure();
        }
    }
}

// =====================================================================================================================
void RsrcProcMgr::CmdGenerateMipmaps(
    GfxCmdBuffer*         pCmdBuffer,
    const GenMipmapsInfo& genInfo) const
{
    // The range cannot start at mip zero and cannot extend past the last mip level.
    PAL_ASSERT((genInfo.range.startSubres.mipLevel >= 1) &&
               ((genInfo.range.startSubres.mipLevel + genInfo.range.numMips) <=
                genInfo.pImage->GetImageCreateInfo().mipLevels));

    if (m_pDevice->Parent()->Settings().mipGenUseFastPath &&
        (genInfo.pImage->GetImageCreateInfo().imageType == ImageType::Tex2d))
    {
        // Use compute shader-based path that can generate up to 12 mipmaps/array slice per pass.
        GenerateMipmapsFast(pCmdBuffer, genInfo);
    }
    else
    {
        // Use multi-pass scaled copy image-based path.
        GenerateMipmapsSlow(pCmdBuffer, genInfo);
    }
}

// =====================================================================================================================
void RsrcProcMgr::GenerateMipmapsFast(
    GfxCmdBuffer*         pCmdBuffer,
    const GenMipmapsInfo& genInfo
    ) const
{
    const auto& device    = *m_pDevice->Parent();
    const auto& settings  = device.Settings();
    const auto& image     = *static_cast<const Image*>(genInfo.pImage);
    const auto& imageInfo = image.GetImageCreateInfo();

    // The shader can only generate up to 12 mips in one pass.
    constexpr uint32 MaxNumMips = 12;

    const ComputePipeline*const pPipeline = (settings.useFp16GenMips == false) ?
                                            GetPipeline(RpmComputePipeline::GenerateMipmaps) :
                                            GetPipeline(RpmComputePipeline::GenerateMipmapsLowp);

    // Save current command buffer state and bind the pipeline.
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    BarrierInfo barrier = { };
    barrier.waitPoint   = HwPipePreCs;

    constexpr HwPipePoint PostCs = HwPipePostCs;
    barrier.pipePointWaitCount   = 1;
    barrier.pPipePoints          = &PostCs;

    // If we need to generate more than MaxNumMips mip levels, then we will need to issue multiple dispatches with
    // internal barriers in between, because the src mip of a subsequent pass is the last dst mip of the previous pass.
    // Note that we don't need any barriers between per-array slice dispatches.
    BarrierTransition transition = { };
    transition.srcCacheMask = CoherShader;
    transition.dstCacheMask = CoherShader;

    // We will specify the base subresource later on.
    transition.imageInfo.pImage                = genInfo.pImage;
    transition.imageInfo.subresRange.numMips   = 1;
    transition.imageInfo.subresRange.numSlices = genInfo.range.numSlices;
    transition.imageInfo.oldLayout             = genInfo.genMipLayout;
    transition.imageInfo.newLayout             = genInfo.genMipLayout;

    barrier.transitionCount = 1;
    barrier.pTransitions    = &transition;

    barrier.reason = Developer::BarrierReasonUnknown;

    SubresId srcSubres = genInfo.range.startSubres;
    --srcSubres.mipLevel;

    uint32 samplerType = 0; // 0 = linearSampler, 1 = pointSampler

    if ((genInfo.filter.magnification == Pal::XyFilterLinear) && (genInfo.filter.minification == Pal::XyFilterLinear))
    {
        PAL_ASSERT(genInfo.filter.mipFilter == Pal::MipFilterNone);
        samplerType = 0;
    }
    else if ((genInfo.filter.magnification == Pal::XyFilterPoint)
        && (genInfo.filter.minification == Pal::XyFilterPoint))
    {
        PAL_ASSERT(genInfo.filter.mipFilter == Pal::MipFilterNone);
        samplerType = 1;
    }
    else
    {
        PAL_NOT_IMPLEMENTED();
    }

    for (uint32 start = 0; start < genInfo.range.numMips; start += MaxNumMips, srcSubres.mipLevel += MaxNumMips)
    {
        const uint32 numMipsToGenerate = Min((genInfo.range.numMips - start), MaxNumMips);

        // The shader can only handle one array slice per pass.
        for (uint32 slice = 0; slice < genInfo.range.numSlices; ++slice, ++srcSubres.arraySlice)
        {
            const SubResourceInfo& subresInfo = *image.SubresourceInfo(srcSubres);

            const SwizzledFormat srcFormat =
                (genInfo.swizzledFormat.format != ChNumFormat::Undefined) ? genInfo.swizzledFormat : subresInfo.format;
            SwizzledFormat dstFormat = srcFormat;

            const uint32 numWorkGroupsPerDim[] =
            {
                RpmUtil::MinThreadGroups(subresInfo.extentTexels.width,  64),
                RpmUtil::MinThreadGroups(subresInfo.extentTexels.height, 64),
                1
            };

            const float invInputDims[] =
            {
                (1.0f / subresInfo.extentTexels.width),
                (1.0f / subresInfo.extentTexels.height),
            };

            // Bind inline constants to user data 0+.
            const uint32 copyData[] =
            {
                numMipsToGenerate,                                               // numMips
                (numWorkGroupsPerDim[0] * numWorkGroupsPerDim[1] * numWorkGroupsPerDim[2]),
                reinterpret_cast<const uint32&>(invInputDims[0]),
                reinterpret_cast<const uint32&>(invInputDims[1]),
                samplerType,
            };
            const uint32 copyDataDwords = Util::NumBytesToNumDwords(sizeof(copyData));

            pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 0, copyDataDwords, &copyData[0]);

            // Create an embedded user-data table and bind it.  We need an image view and a sampler for the src
            // subresource, image views for MaxNumMips dst subresources, and a buffer SRD pointing to the atomic
            // counter.
            constexpr uint8  NumSlots   = 2 + MaxNumMips + 1;
            uint32*          pUserData  = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                                 SrdDwordAlignment() * NumSlots,
                                                                                 SrdDwordAlignment(),
                                                                                 PipelineBindPoint::Compute,
                                                                                 copyDataDwords);

            // The hardware can't handle UAV stores using sRGB num format.  The resolve shaders already contain a
            // linear-to-gamma conversion, but in order for that to work the output UAV's num format must be patched to
            // be simple UNORM.
            if (Formats::IsSrgb(dstFormat.format))
            {
                dstFormat.format = Formats::ConvertToUnorm(dstFormat.format);
                PAL_ASSERT(Formats::IsUndefined(dstFormat.format) == false);

                PAL_NOT_IMPLEMENTED_MSG(
                    "Gamma correction for sRGB image writes is not yet implemented in the mipgen shader.");
            }

            SubresRange viewRange = { srcSubres, 1, 1 };

            ImageViewInfo srcImageView = { };
            RpmUtil::BuildImageViewInfo(&srcImageView,
                                        image,
                                        viewRange,
                                        srcFormat,
                                        genInfo.baseMipLayout,
                                        device.TexOptLevel());

            device.CreateImageViewSrds(1, &srcImageView, pUserData);
            pUserData += SrdDwordAlignment();

            SamplerInfo samplerInfo = { };
            samplerInfo.filter      = genInfo.filter;
            samplerInfo.addressU    = TexAddressMode::Clamp;
            samplerInfo.addressV    = TexAddressMode::Clamp;
            samplerInfo.addressW    = TexAddressMode::Clamp;
            samplerInfo.compareFunc = CompareFunc::Always;
            device.CreateSamplerSrds(1, &samplerInfo, pUserData);
            pUserData += SrdDwordAlignment();

            ImageViewInfo dstImageView[MaxNumMips] = { };
            for (uint32 mip = 0; mip < MaxNumMips; ++mip)
            {
                if (mip < numMipsToGenerate)
                {
                    ++viewRange.startSubres.mipLevel;
                }

                RpmUtil::BuildImageViewInfo(&dstImageView[mip],
                                            image,
                                            viewRange,
                                            dstFormat,
                                            genInfo.genMipLayout,
                                            device.TexOptLevel());
            }

            device.CreateImageViewSrds(MaxNumMips, &dstImageView[0], pUserData);
            pUserData += (SrdDwordAlignment() * MaxNumMips);

            // Allocate scratch memory for the global atomic counter and initialize it to 0.
            const gpusize counterVa = pCmdBuffer->AllocateGpuScratchMem(1, Util::NumBytesToNumDwords(128));
            pCmdBuffer->CmdWriteImmediate(HwPipePoint::HwPipeTop, 0, ImmediateDataWidth::ImmediateData32Bit, counterVa);

            BufferViewInfo bufferView = { };
            bufferView.gpuAddr        = counterVa;
            bufferView.stride         = 0;
            bufferView.range          = sizeof(uint32);
            bufferView.swizzledFormat = UndefinedSwizzledFormat;

            device.CreateUntypedBufferViewSrds(1, &bufferView, pUserData);

            // Execute the dispatch.
            pCmdBuffer->CmdDispatch(numWorkGroupsPerDim[0], numWorkGroupsPerDim[1], numWorkGroupsPerDim[2]);
        }

        srcSubres.arraySlice = genInfo.range.startSubres.arraySlice;

        if ((start + MaxNumMips) < genInfo.range.numMips)
        {
            // If we need to do additional dispatches to handle more mip levels, issue a barrier between each pass.
            transition.imageInfo.subresRange.startSubres          = srcSubres;
            transition.imageInfo.subresRange.startSubres.mipLevel = (start + numMipsToGenerate);

            pCmdBuffer->CmdBarrier(barrier);
        }
    }

    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
void RsrcProcMgr::GenerateMipmapsSlow(
    GfxCmdBuffer*         pCmdBuffer,
    const GenMipmapsInfo& genInfo
    ) const
{
    const Pal::Image*      pImage     = static_cast<const Pal::Image*>(genInfo.pImage);
    const ImageCreateInfo& createInfo = pImage->GetImageCreateInfo();

    // We will use scaled image copies to generate each mip. Most of the copy state is identical but we must adjust the
    // copy region for each generated subresource.
    ImageScaledCopyRegion region = {};
    region.srcSubres.aspect     = genInfo.range.startSubres.aspect;
    region.srcSubres.arraySlice = genInfo.range.startSubres.arraySlice;
    region.dstSubres.aspect     = genInfo.range.startSubres.aspect;
    region.dstSubres.arraySlice = genInfo.range.startSubres.arraySlice;
    region.numSlices            = genInfo.range.numSlices;
    region.swizzledFormat       = genInfo.swizzledFormat;

    ScaledCopyInfo copyInfo = {};
    copyInfo.pSrcImage      = pImage;
    copyInfo.srcImageLayout = genInfo.baseMipLayout;
    copyInfo.pDstImage      = pImage;
    copyInfo.dstImageLayout = genInfo.genMipLayout;
    copyInfo.regionCount    = 1;
    copyInfo.pRegions       = &region;
    copyInfo.filter         = genInfo.filter;
    copyInfo.rotation       = ImageRotation::Ccw0;

    const bool useGraphicsCopy = ScaledCopyImageUseGraphics(pCmdBuffer, copyInfo);

    // We need an internal barrier between each mip-level's scaled copy because the destination of the prior copy is
    // the source of the next copy. Note that we can't use CoherCopy here because we optimize it away in the barrier
    // code but that optimization requires that we pop all state before calling CmdBarrier. That's very slow so instead
    // we use implementation dependent cache masks.
    BarrierTransition transition = {};
    transition.srcCacheMask = useGraphicsCopy ? CoherColorTarget : CoherShader;
    transition.dstCacheMask = CoherShader;

    // We will specify the base subresource later on.
    transition.imageInfo.pImage                = pImage;
    transition.imageInfo.subresRange.numMips   = 1;
    transition.imageInfo.subresRange.numSlices = genInfo.range.numSlices;
    transition.imageInfo.oldLayout             = genInfo.genMipLayout;
    transition.imageInfo.newLayout             = genInfo.genMipLayout;

    const HwPipePoint postBlt = useGraphicsCopy ? HwPipeBottom : HwPipePostCs;
    BarrierInfo       barrier = {};

    barrier.waitPoint          = HwPipePostIndexFetch;
    barrier.pipePointWaitCount = 1;
    barrier.pPipePoints        = &postBlt;
    barrier.transitionCount    = 1;
    barrier.pTransitions       = &transition;

    // Save current command buffer state.
    if (useGraphicsCopy)
    {
        pCmdBuffer->PushGraphicsState();
    }
    else
    {
        pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
    }

    // Issue one CmdScaledCopyImage for each mip in the generation range.
    uint32       destMip = genInfo.range.startSubres.mipLevel;
    const uint32 lastMip = destMip + genInfo.range.numMips - 1;

    while (destMip <= lastMip)
    {
        region.srcSubres.mipLevel = destMip - 1;
        region.dstSubres.mipLevel = destMip;

        // We want to generate all texels in the target subresource so copy the full extent from the first array slice
        // in the current source and destination mips.
        const SubResourceInfo& srcSubresInfo = *pImage->SubresourceInfo(region.srcSubres);
        const SubResourceInfo& dstSubresInfo = *pImage->SubresourceInfo(region.dstSubres);

        region.srcExtent.width  = srcSubresInfo.extentTexels.width;
        region.srcExtent.height = srcSubresInfo.extentTexels.height;
        region.srcExtent.depth  = srcSubresInfo.extentTexels.depth;
        region.dstExtent.width  = dstSubresInfo.extentTexels.width;
        region.dstExtent.height = dstSubresInfo.extentTexels.height;
        region.dstExtent.depth  = dstSubresInfo.extentTexels.depth;

        if (useGraphicsCopy)
        {
            ScaledCopyImageGraphics(pCmdBuffer, copyInfo);
        }
        else
        {
            ScaledCopyImageCompute(pCmdBuffer, copyInfo);
        }

        // If we're going to loop again...
        if (++destMip <= lastMip)
        {
            // Update the copy's source layout.
            copyInfo.srcImageLayout = genInfo.genMipLayout;

            // Issue the barrier between this iteration's writes and the next iteration's reads.
            transition.imageInfo.subresRange.startSubres = region.dstSubres;

            pCmdBuffer->CmdBarrier(barrier);
        }
    }

    // Restore original command buffer state.
    if (useGraphicsCopy)
    {
        pCmdBuffer->PopGraphicsState();
    }
    else
    {
        pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
    }
}

// =====================================================================================================================
void RsrcProcMgr::ScaledCopyImageGraphics(
    GfxCmdBuffer*           pCmdBuffer,
    const ScaledCopyInfo&   copyInfo) const
{
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());
    // Don't expect GFX Blts on Nested unless targets not inherited.
    PAL_ASSERT((pCmdBuffer->IsNested() == false) || (static_cast<UniversalCmdBuffer*>(
        pCmdBuffer)->GetGraphicsState().inheritedState.stateFlags.targetViewState == 0));

    // Get some useful information about the image.
    const auto* pSrcImage                 = static_cast<const Image*>(copyInfo.pSrcImage);
    const auto* pDstImage                 = static_cast<const Image*>(copyInfo.pDstImage);
    ImageLayout srcImageLayout            = copyInfo.srcImageLayout;
    ImageLayout dstImageLayout            = copyInfo.dstImageLayout;
    uint32 regionCount                    = copyInfo.regionCount;
    const ImageScaledCopyRegion* pRegions = copyInfo.pRegions;

    const auto& dstCreateInfo = pDstImage->GetImageCreateInfo();
    const auto& srcCreateInfo = pSrcImage->GetImageCreateInfo();
    const auto& device        = *m_pDevice->Parent();
    const bool isTex3d        = (srcCreateInfo.imageType == ImageType::Tex3d) ? 1 : 0;

    Pal::CmdStream*const pStream = pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::Graphics);
    PAL_ASSERT(pStream != nullptr);

    const StencilRefMaskParams       stencilRefMasks      = { 0xFF, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0xFF, 0x01, 0xFF };

    ViewportParams viewportInfo = {};
    viewportInfo.count                 = 1;
    viewportInfo.viewports[0].origin   = PointOrigin::UpperLeft;
    viewportInfo.viewports[0].minDepth = 0.f;
    viewportInfo.viewports[0].maxDepth = 1.f;
    viewportInfo.horzClipRatio         = FLT_MAX;
    viewportInfo.horzDiscardRatio      = 1.0f;
    viewportInfo.vertClipRatio         = FLT_MAX;
    viewportInfo.vertDiscardRatio      = 1.0f;
    viewportInfo.depthRange            = DepthRange::ZeroToOne;

    ScissorRectParams scissorInfo = {};
    scissorInfo.count = 1;

    const ColorTargetViewInternalCreateInfo colorViewInfoInternal = {};

    ColorTargetViewCreateInfo colorViewInfo = {};
    colorViewInfo.imageInfo.pImage    = copyInfo.pDstImage;
    colorViewInfo.imageInfo.arraySize = 1;

    if (dstCreateInfo.imageType == ImageType::Tex3d)
    {
        colorViewInfo.zRange.extent     = 1;
        colorViewInfo.flags.zRangeValid = true;
    }

    BindTargetParams bindTargetsInfo = {};
    bindTargetsInfo.colorTargets[0].imageLayout      = dstImageLayout;
    bindTargetsInfo.colorTargets[0].pColorTargetView = nullptr;

#if PAL_ENABLE_PRINTS_ASSERTS
    PAL_ASSERT(static_cast<const UniversalCmdBuffer*>(pCmdBuffer)->IsGraphicsStatePushed());
#endif

    BindCommonGraphicsState(pCmdBuffer);
    pCmdBuffer->CmdBindDepthStencilState(m_pDepthDisableState);
    pCmdBuffer->CmdBindMsaaState(GetMsaaState(dstCreateInfo.samples, dstCreateInfo.fragments));
    pCmdBuffer->CmdSetStencilRefMasks(stencilRefMasks);

    if (copyInfo.flags.srcAlpha)
    {
        pCmdBuffer->CmdBindColorBlendState(m_pColorBlendState);
    }
    else
    {
        pCmdBuffer->CmdBindColorBlendState(m_pBlendDisableState);
    }

    // Keep track of the previous graphics pipeline to reduce the pipeline switching overhead.
    const GraphicsPipeline* pPreviousPipeline = nullptr;

    // Accumulate the restore mask for each region copied.
    uint32 restoreMask = 0;

    uint32 colorKey[4]          = {0};
    uint32 alphaDiffMul         = 0;
    float  threshold            = 0.0f;
    uint32 colorKeyEnableMask   = 0;

    if (copyInfo.flags.srcColorKey)
    {
        colorKeyEnableMask = 1;
    }
    else if (copyInfo.flags.dstColorKey)
    {
        colorKeyEnableMask = 2;
    }

    if (colorKeyEnableMask > 0)
    {
        const bool srcColorKey = (colorKeyEnableMask == 1);

        PAL_ASSERT(copyInfo.pColorKey != nullptr);
        PAL_ASSERT(srcCreateInfo.imageType == ImageType::Tex2d);
        PAL_ASSERT(dstCreateInfo.imageType == ImageType::Tex2d);
        PAL_ASSERT(srcCreateInfo.samples <= 1);
        PAL_ASSERT(dstCreateInfo.samples <= 1);

        memcpy(&colorKey[0], &copyInfo.pColorKey->u32Color[0], sizeof(colorKey));

        // Convert uint color key to float representation
        SwizzledFormat format = srcColorKey ? srcCreateInfo.swizzledFormat : dstCreateInfo.swizzledFormat;
        RpmUtil::ConvertClearColorToNativeFormat(format, format, colorKey);
        // Only GenerateMips uses swizzledFormat in regions, color key is not available in this case.
        PAL_ASSERT(Formats::IsUndefined(copyInfo.pRegions[0].swizzledFormat.format));
        // Set constant to respect or ignore alpha channel color diff
        constexpr uint32 FloatOne = 0x3f800000;
        alphaDiffMul = Formats::HasUnusedAlpha(format) ? 0 : FloatOne;

        // Compute the threshold for comparing 2 float value
        const uint32 bitCount = Formats::MaxComponentBitCount(format.format);
        threshold = static_cast<float>(pow(2, -2.0f * bitCount) - pow(2, -2.0f * bitCount - 24.0f));
    }

    // Each region needs to be copied individually.
    for (uint32 region = 0; region < regionCount; ++region)
    {
        // Multiply all x-dimension values in our region by the texel scale.

        ImageScaledCopyRegion copyRegion = pRegions[region];

        // Calculate the absolute value of dstExtent, which will get fed to the shader.
        const int32 dstExtentW =
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 607
            (copyInfo.flags.coordsInFloat != 0) ? static_cast<int32>(copyRegion.dstExtentFloat.width + 0.5f) :
#endif
            copyRegion.dstExtent.width;
        const int32 dstExtentH =
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 607
            (copyInfo.flags.coordsInFloat != 0) ? static_cast<int32>(copyRegion.dstExtentFloat.height + 0.5f) :
#endif
            copyRegion.dstExtent.height;
        const int32 dstExtentD =
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 607
            (copyInfo.flags.coordsInFloat != 0) ? static_cast<int32>(copyRegion.dstExtentFloat.depth + 0.5f) :
#endif
            copyRegion.dstExtent.depth;

        const uint32 absDstExtentW = Math::Absu(dstExtentW);
        const uint32 absDstExtentH = Math::Absu(dstExtentH);
        const uint32 absDstExtentD = Math::Absu(dstExtentD);

        if ((absDstExtentW > 0) && (absDstExtentH > 0) && (absDstExtentD > 0))
        {
            // A negative extent means that we should do a reverse the copy.
            // We want to always use the absolute value of dstExtent.
            // If dstExtent is negative in one dimension, then we negate srcExtent in that dimension,
            // and we adjust the offsets as well.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 607
            if (copyInfo.flags.coordsInFloat != 0)
            {
                if (copyRegion.dstExtentFloat.width < 0)
                {
                    copyRegion.dstOffsetFloat.x     = copyRegion.dstOffsetFloat.x + copyRegion.dstExtentFloat.width;
                    copyRegion.srcOffsetFloat.x     = copyRegion.srcOffsetFloat.x + copyRegion.srcExtentFloat.width;
                    copyRegion.srcExtentFloat.width = -copyRegion.srcExtentFloat.width;
                    copyRegion.dstExtentFloat.width = -copyRegion.dstExtentFloat.width;
                }

                if (copyRegion.dstExtentFloat.height < 0)
                {
                    copyRegion.dstOffsetFloat.y      = copyRegion.dstOffsetFloat.y + copyRegion.dstExtentFloat.height;
                    copyRegion.srcOffsetFloat.y      = copyRegion.srcOffsetFloat.y + copyRegion.srcExtentFloat.height;
                    copyRegion.srcExtentFloat.height = -copyRegion.srcExtentFloat.height;
                    copyRegion.dstExtentFloat.height = -copyRegion.dstExtentFloat.height;
                }

                if (copyRegion.dstExtentFloat.depth < 0)
                {
                    copyRegion.dstOffsetFloat.z     = copyRegion.dstOffsetFloat.z + copyRegion.dstExtentFloat.depth;
                    copyRegion.srcOffsetFloat.z     = copyRegion.srcOffsetFloat.z + copyRegion.srcExtentFloat.depth;
                    copyRegion.srcExtentFloat.depth = -copyRegion.srcExtentFloat.depth;
                    copyRegion.dstExtentFloat.depth = -copyRegion.dstExtentFloat.depth;
                }
            }
            else
#endif
            {
                if (copyRegion.dstExtent.width < 0)
                {
                    copyRegion.dstOffset.x     = copyRegion.dstOffset.x + copyRegion.dstExtent.width;
                    copyRegion.srcOffset.x     = copyRegion.srcOffset.x + copyRegion.srcExtent.width;
                    copyRegion.srcExtent.width = -copyRegion.srcExtent.width;
                    copyRegion.dstExtent.width = -copyRegion.dstExtent.width;
                }

                if (copyRegion.dstExtent.height < 0)
                {
                    copyRegion.dstOffset.y      = copyRegion.dstOffset.y + copyRegion.dstExtent.height;
                    copyRegion.srcOffset.y      = copyRegion.srcOffset.y + copyRegion.srcExtent.height;
                    copyRegion.srcExtent.height = -copyRegion.srcExtent.height;
                    copyRegion.dstExtent.height = -copyRegion.dstExtent.height;
                }

                if (copyRegion.dstExtent.depth < 0)
                {
                    copyRegion.dstOffset.z     = copyRegion.dstOffset.z + copyRegion.dstExtent.depth;
                    copyRegion.srcOffset.z     = copyRegion.srcOffset.z + copyRegion.srcExtent.depth;
                    copyRegion.srcExtent.depth = -copyRegion.srcExtent.depth;
                    copyRegion.dstExtent.depth = -copyRegion.dstExtent.depth;
                }
            }

            // The shader expects the region data to be arranged as follows for each dispatch:
            // Src Normalized Left,  Src Normalized Top,Src Normalized Right, SrcNormalized Bottom.

            const Extent3d& srcExtent = pSrcImage->SubresourceInfo(copyRegion.srcSubres)->extentTexels;
            float srcLeft   = 0;
            float srcTop    = 0;
            float srcRight  = 0;
            float srcBottom = 0;

            float dstLeft   = 0;
            float dstTop    = 0;
            float dstRight  = 0;
            float dstBottom = 0;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 607
            if (copyInfo.flags.coordsInFloat != 0)
            {
                srcLeft   = copyRegion.srcOffsetFloat.x / srcExtent.width;
                srcTop    = copyRegion.srcOffsetFloat.y / srcExtent.height;
                srcRight  = (copyRegion.srcOffsetFloat.x + copyRegion.srcExtentFloat.width) / srcExtent.width;
                srcBottom = (copyRegion.srcOffsetFloat.y + copyRegion.srcExtentFloat.height) / srcExtent.height;

                dstLeft   = copyRegion.dstOffsetFloat.x;
                dstTop    = copyRegion.dstOffsetFloat.y;
                dstRight  = copyRegion.dstOffsetFloat.x + copyRegion.dstExtentFloat.width;
                dstBottom = copyRegion.dstOffsetFloat.y + copyRegion.dstExtentFloat.height;
            }
            else
#endif
            {
                srcLeft   = (1.f * copyRegion.srcOffset.x) / srcExtent.width;
                srcTop    = (1.f * copyRegion.srcOffset.y) / srcExtent.height;
                srcRight  = (1.f * (copyRegion.srcOffset.x + copyRegion.srcExtent.width)) / srcExtent.width;
                srcBottom = (1.f * (copyRegion.srcOffset.y + copyRegion.srcExtent.height)) / srcExtent.height;

                dstLeft   = 1.f * copyRegion.dstOffset.x;
                dstTop    = 1.f * copyRegion.dstOffset.y;
                dstRight  = 1.f * (copyRegion.dstOffset.x + copyRegion.dstExtent.width);
                dstBottom = 1.f * (copyRegion.dstOffset.y + copyRegion.dstExtent.height);
            }

            PAL_ASSERT((srcLeft   >= 0.0f)   && (srcLeft   <= 1.0f) &&
                       (srcTop    >= 0.0f)   && (srcTop    <= 1.0f) &&
                       (srcRight  >= 0.0f)   && (srcRight  <= 1.0f) &&
                       (srcBottom >= 0.0f)   && (srcBottom <= 1.0f));

            // RotationParams contains the parameters to rotate 2d texture cooridnates.
            // Given 2d texture coordinates (u, v), we use following equations to compute rotated coordinates (u', v'):
            // u' = RotationParams[0] * u + RotationParams[1] * v + RotationParams[4]
            // v' = RotationParams[2] * u + RotationParams[3] * v + RotationParams[5]
            constexpr float RotationParams[static_cast<uint32>(ImageRotation::Count)][6] =
            {
                { 1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f},
                { 0.0f, -1.0f,  1.0f,  0.0f, 1.0f, 0.0f},
                {-1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f},
                { 0.0f,  1.0f, -1.0f,  0.0f, 0.0f, 1.0f},
            };

            const uint32 rotationIndex = static_cast<const uint32>(copyInfo.rotation);

            const uint32 texcoordVs[4] =
            {
                reinterpret_cast<const uint32&>(dstLeft),
                reinterpret_cast<const uint32&>(dstTop),
                reinterpret_cast<const uint32&>(dstRight),
                reinterpret_cast<const uint32&>(dstBottom),
            };

            const uint32 userData[10] =
            {
                reinterpret_cast<const uint32&>(srcLeft),
                reinterpret_cast<const uint32&>(srcTop),
                reinterpret_cast<const uint32&>(srcRight),
                reinterpret_cast<const uint32&>(srcBottom),
                reinterpret_cast<const uint32&>(RotationParams[rotationIndex][0]),
                reinterpret_cast<const uint32&>(RotationParams[rotationIndex][1]),
                reinterpret_cast<const uint32&>(RotationParams[rotationIndex][2]),
                reinterpret_cast<const uint32&>(RotationParams[rotationIndex][3]),
                reinterpret_cast<const uint32&>(RotationParams[rotationIndex][4]),
                reinterpret_cast<const uint32&>(RotationParams[rotationIndex][5]),
            };

            if (isTex3d == true)
            {
                pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 1, 4, &userData[0]);
            }
            else
            {
                pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 1, 4, &texcoordVs[0]);
                pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 5, 10, &userData[0]);
            }
        }

        // Determine which image formats to use for the copy.
        SwizzledFormat srcFormat = pSrcImage->SubresourceInfo(copyRegion.srcSubres)->format;
        SwizzledFormat dstFormat = pDstImage->SubresourceInfo(copyRegion.dstSubres)->format;
        if (Formats::IsUndefined(copyRegion.swizzledFormat.format) == false)
        {
            srcFormat = copyRegion.swizzledFormat;
            dstFormat = copyRegion.swizzledFormat;
        }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 626
        // Non-SRGB can be treated as SRGB when copying to non-srgb image
        if (copyInfo.flags.dstAsSrgb)
        {
            dstFormat.format = Formats::ConvertToSrgb(dstFormat.format);
            PAL_ASSERT(Formats::IsUndefined(dstFormat.format) == false);
        }
#endif

        // Update the color target view format with the destination format.
        colorViewInfo.swizzledFormat = dstFormat;

        const GraphicsPipeline* pPipeline = nullptr;
        if (srcCreateInfo.imageType == ImageType::Tex2d)
        {
            if (colorKeyEnableMask)
            {
                // There is no UINT/SINT formats in DX9 and only legacy formats <= 32 bpp can be used in color key blit.
                const uint32 bpp = Formats::BytesPerPixel(srcFormat.format);
                PAL_ASSERT(bpp <= 32);
                pPipeline = GetGfxPipeline(ScaledCopyImageColorKey);
            }
            else
            {
                pPipeline = GetGfxPipelineByTargetIndexAndFormat(ScaledCopy2d_32ABGR, 0, dstFormat);
            }
        }
        else
        {
            pPipeline = GetGfxPipelineByTargetIndexAndFormat(ScaledCopy3d_32ABGR, 0, dstFormat);
        }

        // Only switch to the appropriate graphics pipeline if it differs from the previous region's pipeline.
        if (pPreviousPipeline != pPipeline)
        {
            pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics, pPipeline, InternalApiPsoHash, });
            pCmdBuffer->CmdOverwriteRbPlusFormatForBlits(dstFormat, 0);
            pPreviousPipeline = pPipeline;
        }

        uint32 sizeInDwords;
        constexpr uint32 ColorKeyDataDwords = 7;
        if (colorKeyEnableMask)
        {
            // Create an embedded SRD table and bind it to user data 0. We need image views and
            // a sampler for the src and dest subresource, as well as some inline constants for src and dest
            // color key for 2d texture copy. Only need image view and a sampler for the src subresource
            // as not support color key for 3d texture copy.
            sizeInDwords = SrdDwordAlignment() * 3 + ColorKeyDataDwords;
        }
        else
        {
            // If color Key is not enabled, the ps shader don't need to allocate memory for copydata.
            sizeInDwords = SrdDwordAlignment() * 2;
        }

        uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                   sizeInDwords,
                                                                   SrdDwordAlignment(),
                                                                   PipelineBindPoint::Graphics,
                                                                   0);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 626
        // Follow up the compute path of scaled copy that SRGB can be treated as UNORM
        // when copying from SRGB -> XX.
        if (copyInfo.flags.srcSrgbAsUnorm)
        {
            srcFormat.format = Formats::ConvertToUnorm(srcFormat.format);
        }
#endif

        ImageViewInfo imageView[2] = {};
        SubresRange   viewRange    = { copyRegion.srcSubres, 1, copyRegion.numSlices };

        RpmUtil::BuildImageViewInfo(&imageView[0],
                                    *pSrcImage,
                                    viewRange,
                                    srcFormat,
                                    srcImageLayout,
                                    device.TexOptLevel());

        if (colorKeyEnableMask)
        {
            // Note that this is a read-only view of the destination.
            viewRange.startSubres = copyRegion.dstSubres;
            RpmUtil::BuildImageViewInfo(&imageView[1],
                *pDstImage,
                viewRange,
                dstFormat,
                dstImageLayout,
                device.TexOptLevel());
             PAL_ASSERT(imageView[1].viewType == ImageViewType::Tex2d);
        }

        // Populate the table with image views of the source and dest image for 2d texture.
        // Only populate the table with an image view of the source image for 3d texutre.
        const uint32 imageCount = colorKeyEnableMask ? 2 : 1;
        device.CreateImageViewSrds(imageCount, &imageView[0], pSrdTable);
        pSrdTable += SrdDwordAlignment() * imageCount;

        SamplerInfo samplerInfo = {};
        samplerInfo.filter      = copyInfo.filter;
        samplerInfo.addressU    = TexAddressMode::Clamp;
        samplerInfo.addressV    = TexAddressMode::Clamp;
        samplerInfo.addressW    = TexAddressMode::Clamp;
        samplerInfo.compareFunc = CompareFunc::Always;
        device.CreateSamplerSrds(1, &samplerInfo, pSrdTable);
        pSrdTable += SrdDwordAlignment();

        // Copy the copy parameters into the embedded user-data space for 2d texture copy.
        if (colorKeyEnableMask)
        {
            PAL_ASSERT(isTex3d == false);
            uint32 copyData[ColorKeyDataDwords] =
            {
                colorKeyEnableMask,
                alphaDiffMul,
                Util::Math::FloatToBits(threshold),
                colorKey[0],
                colorKey[1],
                colorKey[2],
                colorKey[3],
            };

            memcpy(pSrdTable, &copyData[0], sizeof(copyData));
        }

        // Give the gfxip layer a chance to optimize the hardware before we start copying.
        const uint32 bitsPerPixel = Formats::BitsPerPixel(dstFormat.format);
        restoreMask              |= HwlBeginGraphicsCopy(pCmdBuffer, pPipeline, *pDstImage, bitsPerPixel);

        // When copying from 3D to 3D, the number of slices should be 1. When copying from
        // 1D to 1D or 2D to 2D, depth should be 1. Therefore when the src image type is identical
        // to the dst image type, either the depth or the number of slices should be equal to 1.
        PAL_ASSERT((srcCreateInfo.imageType != dstCreateInfo.imageType) ||
                   (copyRegion.numSlices == 1) ||
                   (copyRegion.srcExtent.depth == 1));

        // When copying from 2D to 3D or 3D to 2D, the number of slices should match the depth.
        PAL_ASSERT((srcCreateInfo.imageType == dstCreateInfo.imageType) ||
                   ((((srcCreateInfo.imageType == ImageType::Tex3d) &&
                      (dstCreateInfo.imageType == ImageType::Tex2d)) ||
                     ((srcCreateInfo.imageType == ImageType::Tex2d) &&
                      (dstCreateInfo.imageType == ImageType::Tex3d))) &&
                    (copyRegion.numSlices == static_cast<uint32>(copyRegion.dstExtent.depth))));

        // Setup the viewport and scissor to restrict rendering to the destination region being copied.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 607
        if (copyInfo.flags.coordsInFloat != 0)
        {
            viewportInfo.viewports[0].originX = copyRegion.dstOffsetFloat.x;
            viewportInfo.viewports[0].originY = copyRegion.dstOffsetFloat.y;
            viewportInfo.viewports[0].width   = copyRegion.dstExtentFloat.width;
            viewportInfo.viewports[0].height  = copyRegion.dstExtentFloat.height;
        }
        else
#endif
        {
            viewportInfo.viewports[0].originX = static_cast<float>(copyRegion.dstOffset.x);
            viewportInfo.viewports[0].originY = static_cast<float>(copyRegion.dstOffset.y);
            viewportInfo.viewports[0].width   = static_cast<float>(copyRegion.dstExtent.width);
            viewportInfo.viewports[0].height  = static_cast<float>(copyRegion.dstExtent.height);
        }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 603
        if (copyInfo.flags.scissorTest != 0)
        {
            scissorInfo.scissors[0].offset.x      = copyInfo.pScissorRect->offset.x;
            scissorInfo.scissors[0].offset.y      = copyInfo.pScissorRect->offset.y;
            scissorInfo.scissors[0].extent.width  = copyInfo.pScissorRect->extent.width;
            scissorInfo.scissors[0].extent.height = copyInfo.pScissorRect->extent.height;
        }
        else
#endif
        {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 607
            if (copyInfo.flags.coordsInFloat != 0)
            {
                scissorInfo.scissors[0].offset.x      = static_cast<int32>(copyRegion.dstOffsetFloat.x + 0.5f);
                scissorInfo.scissors[0].offset.y      = static_cast<int32>(copyRegion.dstOffsetFloat.y + 0.5f);
                scissorInfo.scissors[0].extent.width  = static_cast<int32>(copyRegion.dstExtentFloat.width + 0.5f);
                scissorInfo.scissors[0].extent.height = static_cast<int32>(copyRegion.dstExtentFloat.height + 0.5f);
            }
            else
#endif
            {
                scissorInfo.scissors[0].offset.x      = copyRegion.dstOffset.x;
                scissorInfo.scissors[0].offset.y      = copyRegion.dstOffset.y;
                scissorInfo.scissors[0].extent.width  = copyRegion.dstExtent.width;
                scissorInfo.scissors[0].extent.height = copyRegion.dstExtent.height;
            }
        }

        pCmdBuffer->CmdSetViewports(viewportInfo);
        pCmdBuffer->CmdSetScissorRects(scissorInfo);

        // Copy may happen between the layers of a 2d image and the slices of a 3d image.
        const uint32 numSlices = Max(copyRegion.numSlices, absDstExtentD);

        // Each slice is copied individually, we can optimize this into fewer draw calls if it becomes a
        // performance bottleneck, but for now this is simpler.
        for (uint32 sliceOffset = 0; sliceOffset < numSlices; ++sliceOffset)
        {
            const Extent3d& srcExtent = pSrcImage->SubresourceInfo(copyRegion.srcSubres)->extentTexels;

            const float src3dSliceNom = ((static_cast<float>(copyRegion.srcExtent.depth) / numSlices) *
                                         (static_cast<float>(sliceOffset) + 0.5f))
                                        + static_cast<float>(copyRegion.srcOffset.z);
            const float src3dSlice    = src3dSliceNom / srcExtent.depth;
            const float src2dSlice    = static_cast<const float>(sliceOffset);
            const uint32 srcSlice     = isTex3d
                                        ? reinterpret_cast<const uint32&>(src3dSlice)
                                        : reinterpret_cast<const uint32&>(src2dSlice);

            const uint32 userData[1] =
            {
                srcSlice
            };

            if (isTex3d == true)
            {
                pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 5, 1, &userData[0]);
            }
            else
            {
                pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 15, 1, &userData[0]);
            }

            colorViewInfo.imageInfo.baseSubRes = copyRegion.dstSubres;

            if (dstCreateInfo.imageType == ImageType::Tex3d)
            {
                colorViewInfo.zRange.offset = copyRegion.dstOffset.z + sliceOffset;
            }
            else
            {
                colorViewInfo.imageInfo.baseSubRes.arraySlice = copyRegion.dstSubres.arraySlice + sliceOffset;
            }

            // Create and bind a color-target view for this slice.
            LinearAllocatorAuto<VirtualLinearAllocator> sliceAlloc(pCmdBuffer->Allocator(), false);

            IColorTargetView* pColorView = nullptr;
            void* pColorViewMem =
                PAL_MALLOC(m_pDevice->GetColorTargetViewSize(nullptr), &sliceAlloc, AllocInternalTemp);

            if (pColorViewMem == nullptr)
            {
                pCmdBuffer->NotifyAllocFailure();
            }
            else
            {
                // Since our color target view can only bind 1 slice at a time, we have to issue a separate draw for
                // each slice in extent.z. We can keep the same src image view since we pass the explicit slice to
                // read from in user data, but we'll need to create a new color target view each time.
                Result result = m_pDevice->CreateColorTargetView(colorViewInfo,
                                                                 colorViewInfoInternal,
                                                                 pColorViewMem,
                                                                 &pColorView);
                PAL_ASSERT(result == Result::Success);

                bindTargetsInfo.colorTargets[0].pColorTargetView = pColorView;
                bindTargetsInfo.colorTargetCount = 1;
                pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                // Draw a fullscreen quad.
                pCmdBuffer->CmdDraw(0, 3, 0, 1, 0);

                // Unbind the color-target view.
                bindTargetsInfo.colorTargetCount = 0;
                pCmdBuffer->CmdBindTargets(bindTargetsInfo);
                PAL_SAFE_FREE(pColorViewMem, &sliceAlloc);
            }
        }
    }
    // Call back to the gfxip layer so it can restore any state it modified previously.
    HwlEndGraphicsCopy(pStream, restoreMask);
}

// =====================================================================================================================
void RsrcProcMgr::ScaledCopyImageCompute(
    GfxCmdBuffer*           pCmdBuffer,
    const ScaledCopyInfo&   copyInfo) const
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 603
    PAL_ASSERT(copyInfo.flags.scissorTest == 0);
#endif

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 607
    PAL_ASSERT(copyInfo.flags.coordsInFloat == 0);
#endif

    const auto& device       = *m_pDevice->Parent();
    const auto* pSrcImage    = static_cast<const Image*>(copyInfo.pSrcImage);
    const auto* pSrcGfxImage = pSrcImage->GetGfxImage();
    const auto* pDstImage    = static_cast<const Image*>(copyInfo.pDstImage);
    const auto* pDstGfxImage = pDstImage->GetGfxImage();
    const auto& srcInfo      = pSrcImage->GetImageCreateInfo();
    const auto& dstInfo      = pDstImage->GetImageCreateInfo();

    const bool imageTypeMatch = (pSrcGfxImage->GetOverrideImageType() == pDstGfxImage->GetOverrideImageType());
    const bool is3d           = (imageTypeMatch && (pSrcGfxImage->GetOverrideImageType() == ImageType::Tex3d));
    bool       isFmaskCopy    = false;

    // Get the appropriate pipeline object.
    const ComputePipeline* pPipeline = nullptr;
    if (is3d)
    {
        pPipeline = GetPipeline(RpmComputePipeline::ScaledCopyImage3d);
    }
    else
    {
        const bool isDepth = (pSrcImage->IsDepthStencil() || pDstImage->IsDepthStencil());
        if ((srcInfo.samples > 1) && (isDepth == false))
        {
            // EQAA images or MSAA images with FMask disabled are unsupported for scaled copy. There is no use case for
            // EQAA and it would require several new shaders. It can be implemented if needed at a future point.
            PAL_ASSERT((srcInfo.samples == srcInfo.fragments) && (pSrcGfxImage->HasFmaskData() == true));
            pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskScaledCopy);
            isFmaskCopy = true;
        }
        else
        {
            pPipeline = GetPipeline(RpmComputePipeline::ScaledCopyImage2d);
        }
    }

    // Get number of threads per groups in each dimension, we will need this data later.
    uint32 threadsPerGroup[3] = {0};
    pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

    PAL_ASSERT(pCmdBuffer->IsComputeStateSaved());

    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    uint32 colorKey[4]          = {0};
    uint32 alphaDiffMul         = 0;
    float  threshold            = 0.0f;
    uint32 colorKeyEnableMask   = 0;
    uint32 alphaBlendEnableMask = 0;

    if (copyInfo.flags.srcColorKey)
    {
        colorKeyEnableMask = 1;
    }
    else if (copyInfo.flags.dstColorKey)
    {
        colorKeyEnableMask = 2;
    }
    else if (copyInfo.flags.srcAlpha)
    {
        alphaBlendEnableMask = 4;
    }

    if (colorKeyEnableMask > 0)
    {
        const bool srcColorKey = (colorKeyEnableMask == 1);

        PAL_ASSERT(copyInfo.pColorKey != nullptr);
        PAL_ASSERT(srcInfo.imageType == ImageType::Tex2d);
        PAL_ASSERT(dstInfo.imageType == ImageType::Tex2d);
        PAL_ASSERT(srcInfo.samples <= 1);
        PAL_ASSERT(dstInfo.samples <= 1);
        PAL_ASSERT(pPipeline == GetPipeline(RpmComputePipeline::ScaledCopyImage2d));

        memcpy(&colorKey[0], &copyInfo.pColorKey->u32Color[0], sizeof(colorKey));

        // Convert uint color key to float representation
        SwizzledFormat format = srcColorKey ? srcInfo.swizzledFormat : dstInfo.swizzledFormat;
        RpmUtil::ConvertClearColorToNativeFormat(format, format, colorKey);
        // Only GenerateMips uses swizzledFormat in regions, color key is not available in this case.
        PAL_ASSERT(Formats::IsUndefined(copyInfo.pRegions[0].swizzledFormat.format));

        // Set constant to respect or ignore alpha channel color diff
        constexpr uint32 FloatOne = 0x3f800000;
        alphaDiffMul = Formats::HasUnusedAlpha(format) ? 0 : FloatOne;

        // Compute the threshold for comparing 2 float value
        const uint32 bitCount = Formats::MaxComponentBitCount(format.format);
        threshold = static_cast<float>(pow(2, -2.0f * bitCount) - pow(2, -2.0f * bitCount - 24.0f));
    }

    // Now begin processing the list of copy regions.
    for (uint32 idx = 0; idx < copyInfo.regionCount; ++idx)
    {
        ImageScaledCopyRegion copyRegion = copyInfo.pRegions[idx];

        // Calculate the absolute value of dstExtent, which will get fed to the shader.
        const uint32 dstExtentW = Math::Absu(copyRegion.dstExtent.width);
        const uint32 dstExtentH = Math::Absu(copyRegion.dstExtent.height);
        const uint32 dstExtentD = Math::Absu(copyRegion.dstExtent.depth);

        if ((dstExtentW > 0) && (dstExtentH > 0) && (dstExtentD > 0))
        {
            // A negative extent means that we should do a reverse the copy.
            // We want to always use the absolute value of dstExtent.
            // otherwise the compute shader can't handle it. If dstExtent is negative in one
            // dimension, then we negate srcExtent in that dimension, and we adjust the offsets
            // as well.
            if (copyRegion.dstExtent.width < 0)
            {
                copyRegion.dstOffset.x = copyRegion.dstOffset.x + copyRegion.dstExtent.width;
                copyRegion.srcOffset.x = copyRegion.srcOffset.x + copyRegion.srcExtent.width;
                copyRegion.srcExtent.width = -copyRegion.srcExtent.width;
            }

            if (copyRegion.dstExtent.height < 0)
            {
                copyRegion.dstOffset.y = copyRegion.dstOffset.y + copyRegion.dstExtent.height;
                copyRegion.srcOffset.y = copyRegion.srcOffset.y + copyRegion.srcExtent.height;
                copyRegion.srcExtent.height = -copyRegion.srcExtent.height;
            }

            if (copyRegion.dstExtent.depth < 0)
            {
                copyRegion.dstOffset.z = copyRegion.dstOffset.z + copyRegion.dstExtent.depth;
                copyRegion.srcOffset.z = copyRegion.srcOffset.z + copyRegion.srcExtent.depth;
                copyRegion.srcExtent.depth = -copyRegion.srcExtent.depth;
            }

            // The shader expects the region data to be arranged as follows for each dispatch:
            // Src Normalized Left,  Src Normalized Top,   Src Normalized Start-Z (3D) or slice (1D/2D), extent width
            // Dst Pixel X offset,   Dst Pixel Y offset,   Dst Z offset (3D) or slice (1D/2D),           extent height
            // Src Normalized Right, SrcNormalized Bottom, Src Normalized End-Z   (3D),                  extent depth

            // For 3D blts, the source Z-values are normalized as the X and Y values are for 1D, 2D, and 3D.

            const Extent3d& srcExtent = pSrcImage->SubresourceInfo(copyRegion.srcSubres)->extentTexels;
            const float srcLeft   = (1.f * copyRegion.srcOffset.x) / srcExtent.width;
            const float srcTop    = (1.f * copyRegion.srcOffset.y) / srcExtent.height;
            const float srcSlice  = (1.f * copyRegion.srcOffset.z) / srcExtent.depth;
            const float srcRight  = (1.f * (copyRegion.srcOffset.x + copyRegion.srcExtent.width))  / srcExtent.width;
            const float srcBottom = (1.f * (copyRegion.srcOffset.y + copyRegion.srcExtent.height)) / srcExtent.height;
            const float srcDepth  = (1.f * (copyRegion.srcOffset.z + copyRegion.srcExtent.depth))  / srcExtent.depth;

            PAL_ASSERT((srcLeft   >= 0.0f) && (srcLeft   <= 1.0f) &&
                       (srcTop    >= 0.0f) && (srcTop    <= 1.0f) &&
                       (srcSlice  >= 0.0f) && (srcSlice  <= 1.0f) &&
                       (srcRight  >= 0.0f) && (srcRight  <= 1.0f) &&
                       (srcBottom >= 0.0f) && (srcBottom <= 1.0f) &&
                       (srcDepth  >= 0.0f) && (srcDepth  <= 1.0f));

            SwizzledFormat dstFormat = pDstImage->SubresourceInfo(copyRegion.dstSubres)->format;
            SwizzledFormat srcFormat = pSrcImage->SubresourceInfo(copyRegion.srcSubres)->format;
            if (Formats::IsUndefined(copyRegion.swizzledFormat.format) == false)
            {
                srcFormat = copyRegion.swizzledFormat;
                dstFormat = copyRegion.swizzledFormat;
            }

            const uint32 zfilter   = copyInfo.filter.zFilter;
            const uint32 magfilter = copyInfo.filter.magnification;
            const uint32 minfilter = copyInfo.filter.minification;

            float zOffset = 0.0f;

            if (zfilter == ZFilterNone)
            {
                if ((magfilter != XyFilterPoint) || (minfilter != XyFilterPoint))
                {
                    zOffset = 0.5f;
                }
            }
            else if (zfilter != ZFilterPoint)
            {
                zOffset = 0.5f;
            }

            // RotationParams contains the parameters to rotate 2d texture cooridnates.
            // Given 2d texture coordinates (u, v), we use following equations to compute rotated coordinates (u', v'):
            // u' = RotationParams[0] * u + RotationParams[1] * v + RotationParams[4]
            // v' = RotationParams[2] * u + RotationParams[3] * v + RotationParams[5]
            constexpr float RotationParams[static_cast<uint32>(ImageRotation::Count)][6] =
            {
                { 1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f},
                { 0.0f, -1.0f,  1.0f,  0.0f, 1.0f, 0.0f},
                {-1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f},
                { 0.0f,  1.0f, -1.0f,  0.0f, 0.0f, 1.0f},
            };

            const uint32 rotationIndex = static_cast<const uint32>(copyInfo.rotation);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 626
            // Enable gamma conversion when dstFormat is Srgb or copyInfo.flags.dstAsSrgb
            const uint32 enableGammaConversion =
                (Formats::IsSrgb(dstFormat.format) || copyInfo.flags.dstAsSrgb) ? 1 : 0;
#else
            // Enable gamma conversion when dstFormat is Srgb, but only if srcFormat is not Srgb-as-Unorm.
            // Because the Srgb-as-Unorm sample is still gamma compressed and therefore no additional
            // conversion before shader export is needed.
            const uint32 enableGammaConversion =
                (Formats::IsSrgb(dstFormat.format) && (copyInfo.flags.srcSrgbAsUnorm == 0)) ? 1 : 0;
#endif

            const uint32 copyData[] =
            {
                reinterpret_cast<const uint32&>(srcLeft),
                reinterpret_cast<const uint32&>(srcTop),
                static_cast<uint32>(copyRegion.srcOffset.z),
                dstExtentW,
                static_cast<uint32>(copyRegion.dstOffset.x),
                static_cast<uint32>(copyRegion.dstOffset.y),
                static_cast<uint32>(copyRegion.dstOffset.z),
                dstExtentH,
                reinterpret_cast<const uint32&>(srcRight),
                reinterpret_cast<const uint32&>(srcBottom),
                reinterpret_cast<const uint32&>(srcDepth),
                dstExtentD,
                enableGammaConversion,
                reinterpret_cast<const uint32&>(zOffset),
                srcInfo.samples,
                (colorKeyEnableMask | alphaBlendEnableMask),
                reinterpret_cast<const uint32&>(RotationParams[rotationIndex][0]),
                reinterpret_cast<const uint32&>(RotationParams[rotationIndex][1]),
                reinterpret_cast<const uint32&>(RotationParams[rotationIndex][2]),
                reinterpret_cast<const uint32&>(RotationParams[rotationIndex][3]),
                reinterpret_cast<const uint32&>(RotationParams[rotationIndex][4]),
                reinterpret_cast<const uint32&>(RotationParams[rotationIndex][5]),
                alphaDiffMul,
                Util::Math::FloatToBits(threshold),
                colorKey[0],
                colorKey[1],
                colorKey[2],
                colorKey[3],
            };

            // Create an embedded user-data table and bind it to user data 0. We need image views for the src and dst
            // subresources, a sampler for the src subresource, as well as some inline constants for the copy offsets
            // and extents.
            const uint32 DataDwords = NumBytesToNumDwords(sizeof(copyData));
            const uint8  numSlots   = isFmaskCopy ? 4 : 3;
            uint32*      pUserData  = RpmUtil::CreateAndBindEmbeddedUserData(
                                                    pCmdBuffer,
                                                    SrdDwordAlignment() * numSlots + DataDwords,
                                                    SrdDwordAlignment(),
                                                    PipelineBindPoint::Compute,
                                                    0);

            // The hardware can't handle UAV stores using SRGB num format.  The resolve shaders already contain a
            // linear-to-gamma conversion, but in order for that to work the output UAV's num format must be patched to
            // be simple unorm.
            if (Formats::IsSrgb(dstFormat.format))
            {
                dstFormat.format = Formats::ConvertToUnorm(dstFormat.format);
                PAL_ASSERT(Formats::IsUndefined(dstFormat.format) == false);
            }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 626
            if (copyInfo.flags.srcSrgbAsUnorm)
            {
                srcFormat.format = Formats::ConvertToUnorm(srcFormat.format);
            }
#endif

            ImageViewInfo imageView[2] = {};
            SubresRange   viewRange    = { copyRegion.dstSubres, 1, copyRegion.numSlices };

            PAL_ASSERT(TestAnyFlagSet(copyInfo.dstImageLayout.usages, LayoutShaderWrite | LayoutCopyDst) == true);
            RpmUtil::BuildImageViewInfo(&imageView[0],
                                        *pDstImage,
                                        viewRange,
                                        dstFormat,
                                        copyInfo.dstImageLayout,
                                        device.TexOptLevel());
            viewRange.startSubres = copyRegion.srcSubres;
            RpmUtil::BuildImageViewInfo(&imageView[1],
                                        *pSrcImage,
                                        viewRange,
                                        srcFormat,
                                        copyInfo.srcImageLayout,
                                        device.TexOptLevel());

            if (is3d == false)
            {
                imageView[0].viewType = ImageViewType::Tex2d;
                imageView[1].viewType = ImageViewType::Tex2d;
            }

            device.CreateImageViewSrds(2, &imageView[0], pUserData);
            pUserData += SrdDwordAlignment() * 2;

            if (isFmaskCopy)
            {
                // If this is an Fmask-accelerated Copy, create an image view of the source Image's Fmask surface.
                FmaskViewInfo fmaskView = {};
                fmaskView.pImage         = pSrcImage;
                fmaskView.baseArraySlice = copyRegion.srcSubres.arraySlice;
                fmaskView.arraySize      = copyRegion.numSlices;

                m_pDevice->Parent()->CreateFmaskViewSrds(1, &fmaskView, pUserData);
                pUserData += SrdDwordAlignment();
            }

            SamplerInfo samplerInfo = {};
            samplerInfo.filter      = copyInfo.filter;
            samplerInfo.addressU    = TexAddressMode::Clamp;
            samplerInfo.addressV    = TexAddressMode::Clamp;
            samplerInfo.addressW    = TexAddressMode::Clamp;
            samplerInfo.compareFunc = CompareFunc::Always;
            device.CreateSamplerSrds(1, &samplerInfo, pUserData);
            pUserData += SrdDwordAlignment();

            // Copy the copy parameters into the embedded user-data space
            memcpy(pUserData, &copyData[0], sizeof(copyData));

            const uint32 zGroups = is3d ? dstExtentD : copyRegion.numSlices;

            // Execute the dispatch, we need one thread per texel.
            pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroups(dstExtentW, threadsPerGroup[0]),
                                    RpmUtil::MinThreadGroups(dstExtentH, threadsPerGroup[1]),
                                    RpmUtil::MinThreadGroups(zGroups,    threadsPerGroup[2]));
        }
    }

}

// =====================================================================================================================
// Builds commands to perform an out-of-place conversion between a YUV and an RGB image.
void RsrcProcMgr::CmdColorSpaceConversionCopy(
    GfxCmdBuffer*                     pCmdBuffer,
    const Image&                      srcImage,
    ImageLayout                       srcImageLayout,
    const Image&                      dstImage,
    ImageLayout                       dstImageLayout,
    uint32                            regionCount,
    const ColorSpaceConversionRegion* pRegions,
    TexFilter                         filter,
    const ColorSpaceConversionTable&  cscTable
    ) const
{
    const auto& srcImageInfo = srcImage.GetImageCreateInfo();
    const auto& dstImageInfo = dstImage.GetImageCreateInfo();
    PAL_ASSERT((srcImageInfo.imageType == ImageType::Tex2d) && (dstImageInfo.imageType == ImageType::Tex2d));

    const bool srcIsYuv = Formats::IsYuv(srcImageInfo.swizzledFormat.format);
    const bool dstIsYuv = Formats::IsYuv(dstImageInfo.swizzledFormat.format);

    SamplerInfo samplerInfo = { };
    samplerInfo.filter      = filter;
    samplerInfo.addressU    = TexAddressMode::Clamp;
    samplerInfo.addressV    = TexAddressMode::Clamp;
    samplerInfo.addressW    = TexAddressMode::Clamp;
    samplerInfo.compareFunc = CompareFunc::Always;

    if ((dstIsYuv == false) && srcIsYuv)
    {
        ConvertYuvToRgb(pCmdBuffer, srcImage, dstImage, regionCount, pRegions, samplerInfo, cscTable);
    }
    else if ((srcIsYuv == false) && dstIsYuv)
    {
        ConvertRgbToYuv(pCmdBuffer, srcImage, dstImage, regionCount, pRegions, samplerInfo, cscTable);
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }
}

// =====================================================================================================================
// Builds commands to execute a color-space-conversion copy from a YUV source to an RGB destination.
void RsrcProcMgr::ConvertYuvToRgb(
    GfxCmdBuffer*                     pCmdBuffer,
    const Image&                      srcImage,
    const Image&                      dstImage,
    uint32                            regionCount,
    const ColorSpaceConversionRegion* pRegions,
    const SamplerInfo&                sampler,
    const ColorSpaceConversionTable&  cscTable
    ) const
{
    const auto& device       = *m_pDevice->Parent();
    const auto& srcImageInfo = srcImage.GetImageCreateInfo();
    const auto& dstImageInfo = dstImage.GetImageCreateInfo();

    // Build YUV to RGB color-space-conversion table constant buffer.
    RpmUtil::YuvRgbConversionInfo copyInfo = { };
    memcpy(copyInfo.cscTable, &cscTable, sizeof(cscTable));
    const RpmUtil::ColorSpaceConversionInfo& cscInfo =
        RpmUtil::CscInfoTable[static_cast<uint32>(srcImageInfo.swizzledFormat.format) -
        static_cast<uint32>(ChNumFormat::AYUV)];

    // NOTE: Each of the YUV --> RGB conversion shaders expects the following user-data layout:
    //  o RGB destination Image
    //  o YUV source Image's Y aspect (or YCbCr aspect for RGB --> YUV-packed conversions)
    //  o YUV source Image's Cb or CbCr aspect (unused for RGB --> YUV-packed conversions)
    //  o YUV source Image's Cr aspect (unused unless converting between YV12 and RGB)
    //  o Image sampler for scaled copies
    //  o Copy Info constant buffer
    //  o Color-space Conversion Table constant buffer

    constexpr uint32 MaxImageSrds = 4;
    constexpr uint32 MaxTotalSrds = (MaxImageSrds + 1);

    const uint32 viewCount =
        (cscInfo.pipelineYuvToRgb == RpmComputePipeline::YuvToRgb) ? MaxImageSrds : (MaxImageSrds - 1);

    ImageViewInfo viewInfo[MaxImageSrds] = { };

    // Override the RGB image format to skip gamma-correction if it is required.
    SwizzledFormat dstFormat = dstImageInfo.swizzledFormat;

    if (Formats::IsSrgb(dstFormat.format))
    {
        dstFormat.format = Formats::ConvertToUnorm(dstFormat.format);
    }

    const ComputePipeline*const pPipeline = GetPipeline(cscInfo.pipelineYuvToRgb);

    uint32 threadsPerGroup[3] = { };
    pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

    pCmdBuffer->CmdSaveComputeState(ComputeStateFlags::ComputeStatePipelineAndUserData);
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    for (uint32 idx = 0; idx < regionCount; ++idx)
    {
        ColorSpaceConversionRegion region = pRegions[idx];
        if ((region.dstExtent.width == 0) || (region.dstExtent.height == 0))
        {
            continue;   // Skip empty regions.
        }

        const SubresRange dstRange = { region.rgbSubres, 1, region.sliceCount };
        RpmUtil::BuildImageViewInfo(&viewInfo[0],
                                    dstImage,
                                    dstRange,
                                    dstFormat,
                                    RpmUtil::DefaultRpmLayoutShaderWrite,
                                    device.TexOptLevel());

        for (uint32 view = 1; view < viewCount; ++view)
        {
            const auto&       cscViewInfo         = cscInfo.viewInfoYuvToRgb[view - 1];
            SwizzledFormat    imageViewInfoFormat = cscViewInfo.swizzledFormat;
            const SubresRange srcRange            =
                { { cscViewInfo.aspect, 0, region.yuvStartSlice }, 1, region.sliceCount };
            // Try to use MM formats for YUV planes
            RpmUtil::SwapForMMFormat(srcImage.GetDevice(), &imageViewInfoFormat);
            RpmUtil::BuildImageViewInfo(&viewInfo[view],
                                        srcImage,
                                        srcRange,
                                        imageViewInfoFormat,
                                        RpmUtil::DefaultRpmLayoutRead,
                                        device.TexOptLevel());
        }

        // Calculate the absolute value of dstExtent, which will get fed to the shader.
        copyInfo.dstExtent.width  = Math::Absu(region.dstExtent.width);
        copyInfo.dstExtent.height = Math::Absu(region.dstExtent.height);
        copyInfo.dstOffset.x      = region.dstOffset.x;
        copyInfo.dstOffset.y      = region.dstOffset.y;

        // A negative extent means that we should reverse the copy direction. We want to always use the absolute
        // value of dstExtent, otherwise the compute shader can't handle it. If dstExtent is negative in one
        // dimension, then we negate srcExtent in that dimension, and we adjust the offsets as well.
        if (region.dstExtent.width < 0)
        {
            copyInfo.dstOffset.x   = (region.dstOffset.x + region.dstExtent.width);
            region.srcOffset.x     = (region.srcOffset.x + region.srcExtent.width);
            region.srcExtent.width = -region.srcExtent.width;
        }

        if (region.dstExtent.height < 0)
        {
            copyInfo.dstOffset.y    = (region.dstOffset.y + region.dstExtent.height);
            region.srcOffset.y      = (region.srcOffset.y + region.srcExtent.height);
            region.srcExtent.height = -region.srcExtent.height;
        }

        // The shaders expect the source copy region to be specified in normalized texture coordinates.
        const Extent3d& srcExtent = srcImage.SubresourceInfo(0)->extentTexels;

        copyInfo.srcLeft   = (static_cast<float>(region.srcOffset.x) / srcExtent.width);
        copyInfo.srcTop    = (static_cast<float>(region.srcOffset.y) / srcExtent.height);
        copyInfo.srcRight  = (static_cast<float>(region.srcOffset.x + region.srcExtent.width) / srcExtent.width);
        copyInfo.srcBottom = (static_cast<float>(region.srcOffset.y + region.srcExtent.height) / srcExtent.height);

        PAL_ASSERT((copyInfo.srcLeft   >= 0.0f) && (copyInfo.srcLeft   <= 1.0f) &&
                   (copyInfo.srcTop    >= 0.0f) && (copyInfo.srcTop    <= 1.0f) &&
                   (copyInfo.srcRight  >= 0.0f) && (copyInfo.srcRight  <= 1.0f) &&
                   (copyInfo.srcBottom >= 0.0f) && (copyInfo.srcBottom <= 1.0f));

        // Each conversion shader requires:
        //  o Four image SRD's: one for the RGB image, one each for the Y, U and V "planes" of the YUV image
        //  o One sampler SRD
        //  o Inline constant space for copyInfo
        const uint32 sizeInDwords = (SrdDwordAlignment() * MaxTotalSrds) + RpmUtil::YuvRgbConversionInfoDwords;
        uint32* pUserData = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                   sizeInDwords,
                                                                   SrdDwordAlignment(),
                                                                   PipelineBindPoint::Compute,
                                                                   0);

        device.CreateImageViewSrds(viewCount, &viewInfo[0], pUserData);
        pUserData += (SrdDwordAlignment() * MaxImageSrds);

        device.CreateSamplerSrds(1, &sampler, pUserData);
        pUserData += SrdDwordAlignment();

        memcpy(pUserData, &copyInfo, sizeof(copyInfo));

        // Finally, issue the dispatch. The shaders need one thread per texel.
        pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroups(copyInfo.dstExtent.width,  threadsPerGroup[0]),
                                RpmUtil::MinThreadGroups(copyInfo.dstExtent.height, threadsPerGroup[1]),
                                RpmUtil::MinThreadGroups(region.sliceCount,         threadsPerGroup[2]));
    } // End loop over regions

    pCmdBuffer->CmdRestoreComputeState(ComputeStateFlags::ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
// Builds commands to execute a color-space-conversion copy from a RGB source to an YUV destination.
void RsrcProcMgr::ConvertRgbToYuv(
    GfxCmdBuffer*                     pCmdBuffer,
    const Image&                      srcImage,
    const Image&                      dstImage,
    uint32                            regionCount,
    const ColorSpaceConversionRegion* pRegions,
    const SamplerInfo&                sampler,
    const ColorSpaceConversionTable&  cscTable
    ) const
{
    const auto& device       = *m_pDevice->Parent();
    const auto& srcImageInfo = srcImage.GetImageCreateInfo();
    const auto& dstImageInfo = dstImage.GetImageCreateInfo();

    const RpmUtil::ColorSpaceConversionInfo& cscInfo =
        RpmUtil::CscInfoTable[static_cast<uint32>(dstImageInfo.swizzledFormat.format) -
                              static_cast<uint32>(ChNumFormat::AYUV)];

    // NOTE: Each of the RGB --> YUV conversion shaders expects the following user-data layout:
    //  o RGB source Image
    //  o YUV destination Image plane
    //  o Image sampler for scaled copies
    //  o Copy Info constant buffer
    //  o Color-space Conversion Table constant buffer
    //
    // The conversion is done in multiple passes for YUV planar destinations, one pass per plane. This is done so that
    // the planes can sample the source Image at different rates (because planes often have differing dimensions).
    const uint32 passCount = static_cast<uint32>(dstImage.GetImageInfo().numPlanes);

    const ComputePipeline*const pPipeline = GetPipeline(cscInfo.pipelineRgbToYuv);

    uint32 threadsPerGroup[3] = { };
    pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

    pCmdBuffer->CmdSaveComputeState(ComputeStateFlags::ComputeStatePipelineAndUserData);
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    for (uint32 idx = 0; idx < regionCount; ++idx)
    {
        ColorSpaceConversionRegion region = pRegions[idx];
        if ((region.dstExtent.width == 0) || (region.dstExtent.height == 0))
        {
            continue;   // Skip empty regions.
        }

        constexpr uint32 MaxImageSrds = 2;
        constexpr uint32 MaxTotalSrds = (MaxImageSrds + 1);

        ImageViewInfo viewInfo[MaxImageSrds] = { };

        // Override the RGB image format to skip degamma.
        SwizzledFormat srcFormat = srcImageInfo.swizzledFormat;

        if (Formats::IsSrgb(srcFormat.format))
        {
            srcFormat.format = Formats::ConvertToUnorm(srcFormat.format);
        }

        const SubresRange srcRange = { region.rgbSubres, 1, region.sliceCount };
        RpmUtil::BuildImageViewInfo(&viewInfo[0],
                                    srcImage,
                                    srcRange,
                                    srcFormat,
                                    RpmUtil::DefaultRpmLayoutRead,
                                    device.TexOptLevel());

        RpmUtil::RgbYuvConversionInfo copyInfo = { };

        // Calculate the absolute value of dstExtent, which will get fed to the shader.
        const Extent2d dstExtent = { Math::Absu(region.dstExtent.width), Math::Absu(region.dstExtent.height) };
        Offset2d dstOffset = region.dstOffset;

        // A negative extent means that we should reverse the copy direction. We want to always use the absolute
        // value of dstExtent, otherwise the compute shader can't handle it. If dstExtent is negative in one
        // dimension, then we negate srcExtent in that dimension, and we adjust the offsets as well.
        if (region.dstExtent.width < 0)
        {
            dstOffset.x            = (region.dstOffset.x + region.dstExtent.width);
            region.srcOffset.x     = (region.srcOffset.x + region.srcExtent.width);
            region.srcExtent.width = -region.srcExtent.width;
        }

        if (region.dstExtent.height < 0)
        {
            dstOffset.y             = (region.dstOffset.y + region.dstExtent.height);
            region.srcOffset.y      = (region.srcOffset.y + region.srcExtent.height);
            region.srcExtent.height = -region.srcExtent.height;
        }

        // The shaders expect the source copy region to be specified in normalized texture coordinates.
        const Extent3d& srcExtent = srcImage.SubresourceInfo(0)->extentTexels;

        copyInfo.srcLeft   = (static_cast<float>(region.srcOffset.x) / srcExtent.width);
        copyInfo.srcTop    = (static_cast<float>(region.srcOffset.y) / srcExtent.height);
        copyInfo.srcRight  = (static_cast<float>(region.srcOffset.x + region.srcExtent.width) / srcExtent.width);
        copyInfo.srcBottom = (static_cast<float>(region.srcOffset.y + region.srcExtent.height) / srcExtent.height);

        // Writing to macro-pixel YUV destinations requires the distance between the two source pixels which form
        // the destination macro-pixel (in normalized texture coordinates).
        copyInfo.srcWidthEpsilon = (1.f / srcExtent.width);

        PAL_ASSERT((copyInfo.srcLeft   >= 0.0f) && (copyInfo.srcLeft   <= 1.0f) &&
                   (copyInfo.srcTop    >= 0.0f) && (copyInfo.srcTop    <= 1.0f) &&
                   (copyInfo.srcRight  >= 0.0f) && (copyInfo.srcRight  <= 1.0f) &&
                   (copyInfo.srcBottom >= 0.0f) && (copyInfo.srcBottom <= 1.0f));

        if (cscInfo.pipelineRgbToYuv == RpmComputePipeline::RgbToYuvPacked)
        {
            // The YUY2 and YVY2 formats have the packing of components in a macro-pixel reversed compared to the
            // UYVY and VYUY formats.
            copyInfo.reversePacking = ((dstImageInfo.swizzledFormat.format == ChNumFormat::YUY2) ||
                                       (dstImageInfo.swizzledFormat.format == ChNumFormat::YVY2));
        }

        // Perform one conversion pass per plane of the YUV destination.
        for (uint32 pass = 0; pass < passCount; ++pass)
        {
            const auto&       cscViewInfo         = cscInfo.viewInfoRgbToYuv[pass];
            SwizzledFormat    imageViewInfoFormat = cscViewInfo.swizzledFormat;
            const SubresRange dstRange            =
                { { cscViewInfo.aspect, 0, region.yuvStartSlice }, 1, region.sliceCount };
            // Try to use MM formats for YUV planes
            RpmUtil::SwapForMMFormat(dstImage.GetDevice(), &imageViewInfoFormat);
            RpmUtil::BuildImageViewInfo(&viewInfo[1],
                                        dstImage,
                                        dstRange,
                                        imageViewInfoFormat,
                                        RpmUtil::DefaultRpmLayoutShaderWrite,
                                        device.TexOptLevel());

            // Build RGB to YUV color-space-conversion table constant buffer.
            RpmUtil::SetupRgbToYuvCscTable(dstImageInfo.swizzledFormat.format, pass, cscTable, &copyInfo);

            // The destination offset and extent need to be adjusted to account for differences in the dimensions of
            // the YUV image's planes.
            Extent3d log2Ratio = Formats::Log2SubsamplingRatio(dstImageInfo.swizzledFormat.format, cscViewInfo.aspect);
            if (cscInfo.pipelineRgbToYuv == RpmComputePipeline::RgbToYuvPacked)
            {
                // For YUV formats which are macro-pixel packed, we run a special shader which outputs two pixels
                // (one macro-pxiel) per thread. Therefore, we must adjust the destination region accordingly, even
                // though the planar subsampling ratio would normally be treated as 1:1.
                log2Ratio.width  = 1;
                log2Ratio.height = 0;
            }

            copyInfo.dstOffset.x      = (dstOffset.x      >> log2Ratio.width);
            copyInfo.dstOffset.y      = (dstOffset.y      >> log2Ratio.height);
            copyInfo.dstExtent.width  = (dstExtent.width  >> log2Ratio.width);
            copyInfo.dstExtent.height = (dstExtent.height >> log2Ratio.height);

            // Each codec(Mpeg-1, Mpeg-2) requires the specific chroma subsampling location.
            copyInfo.sampleLocX = cscViewInfo.sampleLocX;
            copyInfo.sampleLocY = cscViewInfo.sampleLocY;

            // Each conversion shader requires:
            //  o Two image SRD's: one for the RGB image, one for the YUV image
            //  o One sampler SRD
            //  o Inline constant space for copyInfo
            const uint32 sizeInDwords = (SrdDwordAlignment() * MaxTotalSrds) + RpmUtil::YuvRgbConversionInfoDwords;
            uint32* pUserData = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                       sizeInDwords,
                                                                       SrdDwordAlignment(),
                                                                       PipelineBindPoint::Compute,
                                                                       0);

            device.CreateImageViewSrds(MaxImageSrds, &viewInfo[0], pUserData);
            pUserData += (SrdDwordAlignment() * MaxImageSrds);

            device.CreateSamplerSrds(1, &sampler, pUserData);
            pUserData += SrdDwordAlignment();

            memcpy(pUserData, &copyInfo, sizeof(copyInfo));

            // Finally, issue the dispatch. The shaders need one thread per texel.
            pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroups(copyInfo.dstExtent.width,  threadsPerGroup[0]),
                                    RpmUtil::MinThreadGroups(copyInfo.dstExtent.height, threadsPerGroup[1]),
                                    RpmUtil::MinThreadGroups(region.sliceCount,         threadsPerGroup[2]));
        } // End loop over per-plane passes
    } // End loop over regions

    pCmdBuffer->CmdRestoreComputeState(ComputeStateFlags::ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
// Builds commands to fill every DWORD of the memory object with 'data' between dstOffset and (dstOffset + fillSize).
// The offset and fill size must be DWORD aligned.
void RsrcProcMgr::CmdFillMemory(
    GfxCmdBuffer*    pCmdBuffer,
    bool             saveRestoreComputeState,
    const GpuMemory& dstGpuMemory,
    gpusize          dstOffset,
    gpusize          fillSize,
    uint32           data
    ) const
{
    PAL_ASSERT(IsPow2Aligned(dstOffset, sizeof(uint32)));
    PAL_ASSERT(IsPow2Aligned(fillSize,  sizeof(uint32)));

    constexpr gpusize FillSizeLimit   = 268435456; // 256MB
    const auto& settings              = m_pDevice->Parent()->Settings();

    if (saveRestoreComputeState)
    {
        // Save the command buffer's state.
        pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
    }

    for (gpusize fillOffset = 0; fillOffset < fillSize; fillOffset += FillSizeLimit)
    {
        const uint32 numDwords = static_cast<uint32>(Min(FillSizeLimit, (fillSize - fillOffset)) / sizeof(uint32));

        // ((FillSizeLimit % 4) == 0) as the value stands now, ensuring fillSize is 4xOptimized too. If we change it
        // to something that doesn't satisfy this condition we would need to check ((fillSize - fillOffset) % 4) too.
        const bool is4xOptimized = ((numDwords % 4) == 0);

        const ComputePipeline* pPipeline = nullptr;

        if (is4xOptimized)
        {
            // This fill memory can be optimized to use the 4xDWORD pipeline.
            pPipeline = GetPipeline(RpmComputePipeline::FillMem4xDword);
        }
        else
        {
            // Use the fill memory DWORD pipeline since this call expects everything to be DWORD-aligned.
            pPipeline = GetPipeline(RpmComputePipeline::FillMemDword);
        }

        pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

        uint32 srd[4] = { };
        PAL_ASSERT(m_pDevice->Parent()->ChipProperties().srdSizes.bufferView == sizeof(srd));

        BufferViewInfo dstBufferView = {};
        dstBufferView.gpuAddr = dstGpuMemory.Desc().gpuVirtAddr + dstOffset + fillOffset;
        dstBufferView.range   = numDwords * sizeof(uint32);
        dstBufferView.stride  = (is4xOptimized) ? (sizeof(uint32) * 4) : sizeof(uint32);
        if (is4xOptimized)
        {
            dstBufferView.swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
            dstBufferView.swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };
        }
        else
        {
            dstBufferView.swizzledFormat.format  = ChNumFormat::X32_Uint;
            dstBufferView.swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };
        }
        m_pDevice->Parent()->CreateTypedBufferViewSrds(1, &dstBufferView, &srd[0]);

        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 0, 4, &srd[0]);
        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 4, 1, &data);

        // Issue a dispatch with one thread per DWORD.
        const uint32 minThreads   = (is4xOptimized) ? (numDwords / 4) : numDwords;
        const uint32 threadGroups = RpmUtil::MinThreadGroups(minThreads, pPipeline->ThreadsPerGroup());
        pCmdBuffer->CmdDispatch(threadGroups, 1, 1);
    }
    if (saveRestoreComputeState)
    {
        // Restore the command buffer's state.
        pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
    }
}

// =====================================================================================================================
// Builds commands to clear the specified ranges of the current depth stencil attachment views to the specified values.
void RsrcProcMgr::CmdClearBoundDepthStencilTargets(
    GfxCmdBuffer*                 pCmdBuffer,
    float                         depth,
    uint8                         stencil,
    uint8                         stencilWriteMask,
    uint32                        samples,
    uint32                        fragments,
    DepthStencilSelectFlags       flag,
    uint32                        regionCount,
    const ClearBoundTargetRegion* pClearRegions
    ) const
{
    PAL_ASSERT(regionCount > 0);

    StencilRefMaskParams stencilRefMasks = { };
    stencilRefMasks.flags.u8All    = 0xFF;
    stencilRefMasks.frontRef       = stencil;
    stencilRefMasks.frontReadMask  = 0xFF;
    stencilRefMasks.frontWriteMask = stencilWriteMask;
    stencilRefMasks.backRef        = stencil;
    stencilRefMasks.backReadMask   = 0xFF;
    stencilRefMasks.backWriteMask  = stencilWriteMask;

    ViewportParams viewportInfo = { };
    viewportInfo.count = 1;
    viewportInfo.viewports[0].originX  = 0;
    viewportInfo.viewports[0].originY  = 0;
    viewportInfo.viewports[0].minDepth = 0.f;
    viewportInfo.viewports[0].maxDepth = 1.f;
    viewportInfo.viewports[0].origin   = PointOrigin::UpperLeft;
    viewportInfo.horzClipRatio         = FLT_MAX;
    viewportInfo.horzDiscardRatio      = 1.0f;
    viewportInfo.vertClipRatio         = FLT_MAX;
    viewportInfo.vertDiscardRatio      = 1.0f;
    viewportInfo.depthRange            = DepthRange::ZeroToOne;

    ScissorRectParams scissorInfo = { };
    scissorInfo.count = 1;
    scissorInfo.scissors[0].offset.x = 0;
    scissorInfo.scissors[0].offset.y = 0;

    // Save current command buffer state and bind graphics state which is common for all mipmap levels.
    pCmdBuffer->PushGraphicsState();
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics, GetGfxPipeline(DepthSlowDraw), InternalApiPsoHash, });
    BindCommonGraphicsState(pCmdBuffer);
    pCmdBuffer->CmdBindMsaaState(GetMsaaState(samples, fragments));
    pCmdBuffer->CmdSetStencilRefMasks(stencilRefMasks);

    if ((flag.depth != 0) && (flag.stencil != 0))
    {
        pCmdBuffer->CmdBindDepthStencilState(m_pDepthStencilClearState);
    }
    else if (flag.depth != 0)
    {
        pCmdBuffer->CmdBindDepthStencilState(m_pDepthClearState);
    }
    else if (flag.stencil != 0)
    {
        pCmdBuffer->CmdBindDepthStencilState(m_pStencilClearState);
    }

    // All mip levels share the same depth export value, so only need to do it once.
    RpmUtil::WriteVsZOut(pCmdBuffer, depth);

    for (uint32 scissorIndex = 0; scissorIndex < regionCount; ++scissorIndex)
    {
        // Note: we should clear the same range of slices for depth and/or stencil attachment. If this
        // requirement needs to be relaxed, we need to separate the draws for depth clear and stencil clear.
        RpmUtil::WriteVsFirstSliceOffset(pCmdBuffer, pClearRegions[scissorIndex].startSlice);

        viewportInfo.viewports[0].originX = static_cast<float>(pClearRegions[scissorIndex].rect.offset.x);
        viewportInfo.viewports[0].originY = static_cast<float>(pClearRegions[scissorIndex].rect.offset.y);
        viewportInfo.viewports[0].width   = static_cast<float>(pClearRegions[scissorIndex].rect.extent.width);
        viewportInfo.viewports[0].height  = static_cast<float>(pClearRegions[scissorIndex].rect.extent.height);

        pCmdBuffer->CmdSetViewports(viewportInfo);

        scissorInfo.scissors[0].offset.x      = pClearRegions[scissorIndex].rect.offset.x;
        scissorInfo.scissors[0].offset.y      = pClearRegions[scissorIndex].rect.offset.y;
        scissorInfo.scissors[0].extent.width  = pClearRegions[scissorIndex].rect.extent.width;
        scissorInfo.scissors[0].extent.height = pClearRegions[scissorIndex].rect.extent.height;

        pCmdBuffer->CmdSetScissorRects(scissorInfo);

        // Draw numSlices fullscreen instanced quads.
        pCmdBuffer->CmdDraw(0, 3, 0, pClearRegions[scissorIndex].numSlices, 0);
    }

    // Restore original command buffer state and destroy the depth/stencil state.
    pCmdBuffer->PopGraphicsState();
}

// =====================================================================================================================
// Builds commands to clear the specified ranges of a depth/stencil image to the specified values.
void RsrcProcMgr::CmdClearDepthStencil(
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
    uint32             flags
    ) const
{
    const GfxImage& gfxImage   = *dstImage.GetGfxImage();
    const bool      hasRects   = (rectCount > 0);
    const auto&     createInfo = dstImage.GetImageCreateInfo();

    PAL_ASSERT((hasRects == false) || (pRects != nullptr));

    // Clear groups of ranges on "this group is fast clearable = true/false" boundaries
    uint32 rangesCleared = 0;

    // Convert the Rects to Boxes. We use an AutoBuffer instead of the virtual linear allocator because
    // we may need to allocate more boxes than will fit in the fixed virtual space.
    AutoBuffer<Box, 16, Platform> boxes(rectCount, m_pDevice->GetPlatform());

    // Notify the command buffer if AutoBuffer allocation has failed.
    if (boxes.Capacity() < rectCount)
    {
        pCmdBuffer->NotifyAllocFailure();
    }
    else
    {
        for (uint32 i = 0; i < rectCount; i++)
        {
            boxes[i].offset.x      = pRects[i].offset.x;
            boxes[i].offset.y      = pRects[i].offset.y;
            boxes[i].offset.z      = 0;
            boxes[i].extent.width  = pRects[i].extent.width;
            boxes[i].extent.height = pRects[i].extent.height;
            boxes[i].extent.depth  = 1;
        }

        const bool clearRectCoversWholeImage = ((hasRects                  == false)                  ||
                                                ((rectCount                == 1)                      &&
                                                 (pRects[0].offset.x       == 0)                      &&
                                                 (pRects[0].offset.y       == 0)                      &&
                                                 (createInfo.extent.width  == pRects[0].extent.width) &&
                                                 (createInfo.extent.height == pRects[0].extent.height)));

        while (rangesCleared < rangeCount)
        {
            const uint32 groupBegin = rangesCleared;

            // Note that fast clears don't support sub-rect clears so we skip them if we have any boxes. Further,
            // we only can store one fast clear color per mip level, and therefore can only support fast clears
            // when a range covers all slices.
            const bool groupFastClearable = (clearRectCoversWholeImage &&
                                             gfxImage.IsFastDepthStencilClearSupported(
                                                 depthLayout,
                                                 stencilLayout,
                                                 depth,
                                                 stencil,
                                                 stencilWriteMask,
                                                 pRanges[groupBegin]));

            // Find as many other ranges that also support/don't support fast clearing so that they can be grouped
            // together into a single clear operation.
            uint32 groupEnd = groupBegin + 1;

            while ((groupEnd < rangeCount)     &&
                   ((clearRectCoversWholeImage &&
                     gfxImage.IsFastDepthStencilClearSupported(depthLayout,
                                                               stencilLayout,
                                                               depth,
                                                               stencil,
                                                               stencilWriteMask,
                                                               pRanges[groupEnd]))
                    == groupFastClearable))
            {
                ++groupEnd;
            }

            // Either fast clear or slow clear this group of ranges.
            rangesCleared = groupEnd;
            const uint32 clearRangeCount = groupEnd - groupBegin; // NOTE: end equals one past the last range in group.

            HwlDepthStencilClear(pCmdBuffer,
                                 gfxImage,
                                 depthLayout,
                                 stencilLayout,
                                 depth,
                                 stencil,
                                 stencilWriteMask,
                                 clearRangeCount,
                                 &pRanges[groupBegin],
                                 groupFastClearable,
                                 TestAnyFlagSet(flags, DsClearAutoSync),
                                 rectCount,
                                 &boxes[0]);
        }
    }
}

// =====================================================================================================================
// Builds commands to clear the existing color attachment in the command buffer to the given color data.
void RsrcProcMgr::CmdClearBoundColorTargets(
    GfxCmdBuffer*                   pCmdBuffer,
    uint32                          colorTargetCount,
    const BoundColorTarget*         pBoundColorTargets,
    uint32                          regionCount,
    const ClearBoundTargetRegion*   pClearRegions
    ) const
{
    // for attachment, clear region comes from boxes. So regionCount has to be valid
    PAL_ASSERT(regionCount > 0);

    ViewportParams viewportInfo = { };
    viewportInfo.count                 = 1;
    viewportInfo.viewports[0].originX  = 0;
    viewportInfo.viewports[0].originY  = 0;
    viewportInfo.viewports[0].minDepth = 0.f;
    viewportInfo.viewports[0].maxDepth = 1.f;
    viewportInfo.viewports[0].origin   = PointOrigin::UpperLeft;
    viewportInfo.horzClipRatio         = FLT_MAX;
    viewportInfo.horzDiscardRatio      = 1.0f;
    viewportInfo.vertClipRatio         = FLT_MAX;
    viewportInfo.vertDiscardRatio      = 1.0f;
    viewportInfo.depthRange            = DepthRange::ZeroToOne;

    ScissorRectParams scissorInfo = { };
    scissorInfo.count = 1;

    // Save current command buffer state and bind graphics state which is common for all mipmap levels.
    pCmdBuffer->PushGraphicsState();
    BindCommonGraphicsState(pCmdBuffer);
    pCmdBuffer->CmdBindColorBlendState(m_pBlendDisableState);
    pCmdBuffer->CmdBindDepthStencilState(m_pDepthDisableState);

    for (uint32 colorIndex = 0; colorIndex < colorTargetCount; ++colorIndex)
    {
        uint32 convertedColor[4] = {0};

        if (pBoundColorTargets[colorIndex].clearValue.type == ClearColorType::Float)
        {
            Formats::ConvertColor(pBoundColorTargets[colorIndex].swizzledFormat,
                                  &pBoundColorTargets[colorIndex].clearValue.f32Color[0],
                                  &convertedColor[0]);
        }
        else
        {
            memcpy(&convertedColor[0], &pBoundColorTargets[colorIndex].clearValue.u32Color[0], sizeof(convertedColor));
        }

        // Get the correct slow clear state based on the view format.
        pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics,
                                      GetGfxPipelineByTargetIndexAndFormat(
                                          SlowColorClear0_32ABGR,
                                          pBoundColorTargets[colorIndex].targetIndex,
                                          pBoundColorTargets[colorIndex].swizzledFormat),
                                      InternalApiPsoHash, });
        pCmdBuffer->CmdOverwriteRbPlusFormatForBlits(pBoundColorTargets[colorIndex].swizzledFormat,
                                                     pBoundColorTargets[colorIndex].targetIndex);
        pCmdBuffer->CmdBindMsaaState(GetMsaaState(pBoundColorTargets[colorIndex].samples,
                                                  pBoundColorTargets[colorIndex].fragments));

        RpmUtil::WriteVsZOut(pCmdBuffer, 1.0f);

        RpmUtil::ConvertClearColorToNativeFormat(pBoundColorTargets[colorIndex].swizzledFormat,
                                                 pBoundColorTargets[colorIndex].swizzledFormat,
                                                 &convertedColor[0]);

        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, RpmPsClearFirstUserData, 4, &convertedColor[0]);

        for (uint32 scissorIndex = 0; scissorIndex < regionCount; ++scissorIndex)
        {
            RpmUtil::WriteVsFirstSliceOffset(pCmdBuffer, pClearRegions[scissorIndex].startSlice);

            viewportInfo.viewports[0].originX   = static_cast<float>(pClearRegions[scissorIndex].rect.offset.x);
            viewportInfo.viewports[0].originY   = static_cast<float>(pClearRegions[scissorIndex].rect.offset.y);
            viewportInfo.viewports[0].width     = static_cast<float>(pClearRegions[scissorIndex].rect.extent.width);
            viewportInfo.viewports[0].height    = static_cast<float>(pClearRegions[scissorIndex].rect.extent.height);

            pCmdBuffer->CmdSetViewports(viewportInfo);

            // Create a scissor state for this mipmap level, slice, and current scissor.
            scissorInfo.scissors[0].offset.x        = pClearRegions[scissorIndex].rect.offset.x;
            scissorInfo.scissors[0].offset.y        = pClearRegions[scissorIndex].rect.offset.y;
            scissorInfo.scissors[0].extent.width    = pClearRegions[scissorIndex].rect.extent.width;
            scissorInfo.scissors[0].extent.height   = pClearRegions[scissorIndex].rect.extent.height;

            pCmdBuffer->CmdSetScissorRects(scissorInfo);

            // Draw numSlices fullscreen instanced quads.
            pCmdBuffer->CmdDraw(0, 3, 0, pClearRegions[scissorIndex].numSlices, 0);
        }
    }

    // Restore original command buffer state.
    pCmdBuffer->PopGraphicsState();
}

// =====================================================================================================================
// Builds commands to clear the specified ranges of an image to the given color data.
void RsrcProcMgr::CmdClearColorImage(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       dstImage,
    ImageLayout        dstImageLayout,
    const ClearColor&  color,
    uint32             rangeCount,
    const SubresRange* pRanges,
    uint32             boxCount,
    const Box*         pBoxes,
    uint32             flags
    ) const
{
    Pal::GfxImage*              pGfxImage     = dstImage.GetGfxImage();
    const auto&                 createInfo    = dstImage.GetImageCreateInfo();
    const SubResourceInfo*const pStartSubRes  = dstImage.SubresourceInfo(pRanges[0].startSubres);
    const bool                  hasBoxes      = (boxCount > 0);

    const bool clearBoxCoversWholeImage = ((hasBoxes == false)                                    ||
                                           ((boxCount                 == 1)                       &&
                                            (pBoxes[0].offset.x       == 0)                       &&
                                            (pBoxes[0].offset.y       == 0)                       &&
                                            (pBoxes[0].offset.z       == 0)                       &&
                                            (createInfo.extent.width  == pBoxes[0].extent.width)  &&
                                            (createInfo.extent.height == pBoxes[0].extent.height) &&
                                            (createInfo.extent.depth  == pBoxes[0].extent.depth)));

    bool needPreComputeSync  = TestAnyFlagSet(flags, ColorClearAutoSync);
    bool needPostComputeSync = false;

    for (uint32 rangeIdx = 0; rangeIdx < rangeCount; ++rangeIdx)
    {
        SubresRange  minSlowClearRange = {};
        const auto*  pSlowClearRange   = &minSlowClearRange;
        const auto&  clearRange        = pRanges[rangeIdx];
        ClearMethod  slowClearMethod   = Image::DefaultSlowClearMethod;

        uint32 convertedColor[4] = { 0 };
        if (color.type == ClearColorType::Float)
        {
            const SwizzledFormat& baseFormat = dstImage.SubresourceInfo(clearRange.startSubres)->format;
            Formats::ConvertColor(baseFormat, &color.f32Color[0], &convertedColor[0]);
        }
        else
        {
            memcpy(&convertedColor[0], &color.u32Color[0], sizeof(convertedColor));
        }

        // Note that fast clears don't support sub-rect clears so we skip them if we have any boxes.  Futher, we only
        // can store one fast clear color per mip level, and therefore can only support fast clears when a range covers
        // all slices.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 592
        // Fast clear is only usable when all channels of the color are being written.
        if ((color.disabledChannelMask == 0) &&
             clearBoxCoversWholeImage        &&
#else
        if (clearBoxCoversWholeImage &&
#endif
            pGfxImage->IsFastColorClearSupported(pCmdBuffer,
                                                 dstImageLayout,
                                                 &convertedColor[0],
                                                 clearRange))
        {
            // Assume that all portions of the original range can be fast cleared.
            SubresRange fastClearRange = clearRange;

            // Assume that no portion of the original range needs to be slow cleared.
            minSlowClearRange.startSubres = clearRange.startSubres;
            minSlowClearRange.numSlices   = clearRange.numSlices;
            minSlowClearRange.numMips     = 0;

            for (uint32 mipIdx = 0; mipIdx < clearRange.numMips; ++mipIdx)
            {
                const SubresId    subres      = { clearRange.startSubres.aspect,
                                                  clearRange.startSubres.mipLevel + mipIdx,
                                                  0
                                                };
                const ClearMethod clearMethod = dstImage.SubresourceInfo(subres)->clearMethod;

                if (clearMethod != ClearMethod::Fast)
                {
                    fastClearRange.numMips = mipIdx;

                    minSlowClearRange.startSubres.mipLevel = subres.mipLevel;
                    minSlowClearRange.numMips              = clearRange.numMips - mipIdx;
                    slowClearMethod                        = clearMethod;
                    break;
                }
            }

            if (fastClearRange.numMips != 0)
            {
                if (needPreComputeSync)
                {
                    PreComputeColorClearSync(pCmdBuffer);

                    needPreComputeSync  = false;
                    needPostComputeSync = true;
                }

                // Hand off to the HWL to perform the fast-clear.
                PAL_ASSERT(dstImage.IsRenderTarget());

                HwlFastColorClear(pCmdBuffer, *pGfxImage, &convertedColor[0], fastClearRange);
            }
        }
        else
        {
            // Since fast clears aren't available, the slow-clear range is everything the caller asked for.
            pSlowClearRange = &clearRange;
        }

        // If we couldn't fast clear every range, then we need to slow clear whatever is left over.
        if (pSlowClearRange->numMips != 0)
        {
            const SwizzledFormat& baseFormat   = dstImage.SubresourceInfo(pSlowClearRange->startSubres)->format;
            const bool            is3dBoxClear = hasBoxes && (createInfo.imageType == ImageType::Tex3d);
            uint32                texelScale   = 1;
            const SwizzledFormat  rawFormat    = RpmUtil::GetRawFormat(baseFormat.format, &texelScale, nullptr);

            // Not surprisingly, a slow graphics clears requires a command buffer that supports graphics operations
            if (pCmdBuffer->IsGraphicsSupported() &&
                // Force clears of scaled formats to the compute engine
                (texelScale == 1)                 &&
                (slowClearMethod == ClearMethod::NormalGraphics))
            {
                SlowClearGraphics(pCmdBuffer, dstImage, dstImageLayout, &color, *pSlowClearRange, boxCount, pBoxes);
            }
            else
            {
                if (needPreComputeSync)
                {
                    PreComputeColorClearSync(pCmdBuffer);

                    needPreComputeSync  = false;
                    needPostComputeSync = true;
                }

                // Raw format clears are ok on the compute engine because these won't affect the state of DCC memory.
                SlowClearCompute(pCmdBuffer,
                                 dstImage,
                                 dstImageLayout,
                                 baseFormat,
                                 &color,
                                 *pSlowClearRange,
                                 boxCount,
                                 pBoxes);
            }
        }
    }

    if (needPostComputeSync)
    {
        PostComputeColorClearSync(pCmdBuffer);
    }
}

// =====================================================================================================================
// Builds commands to slow clear a range of an image to the given raw color data using a pixel shader. Note that this
// function can only clear color aspects.
void RsrcProcMgr::SlowClearGraphics(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       dstImage,
    ImageLayout        dstImageLayout,
    const ClearColor*  pColor,
    const SubresRange& clearRange,
    uint32             boxCount,
    const Box*         pBoxes
    ) const
{
    // Graphics slow clears only work on color aspects.
    PAL_ASSERT((clearRange.startSubres.aspect != ImageAspect::Depth) &&
               (clearRange.startSubres.aspect != ImageAspect::Stencil));

    // Get some useful information about the image.
    const auto& createInfo = dstImage.GetImageCreateInfo();
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 592
    bool rawFmtOk = dstImage.GetGfxImage()->IsFormatReplaceable(clearRange.startSubres,
                                                                dstImageLayout,
                                                                true,
                                                                pColor->disabledChannelMask);
#else
    bool rawFmtOk = dstImage.GetGfxImage()->IsFormatReplaceable(clearRange.startSubres, dstImageLayout, true);
#endif

    // Query the format of the image and determine which format to use for the color target view. If rawFmtOk is
    // set the caller has allowed us to use a slightly more efficient raw format.
    const SwizzledFormat baseFormat   = dstImage.SubresourceInfo(clearRange.startSubres)->format;
    SwizzledFormat       viewFormat   = (rawFmtOk ? RpmUtil::GetRawFormat(baseFormat.format, nullptr, nullptr)
                                                  : baseFormat);
    uint32               xRightShift  = 0;
    uint32               vpRightShift = 0;
    // For packed YUV image use X32_Uint instead of X16_Uint to fill with YUYV.
    if ((viewFormat.format == ChNumFormat::X16_Uint) && Formats::IsYuvPacked(baseFormat.format))
    {
        viewFormat.format  = ChNumFormat::X32_Uint;
        viewFormat.swizzle = { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };
        rawFmtOk           = false;
        // If clear color type isn't Yuv then the client is responsible for offset/extent adjustments.
        xRightShift        = (pColor->type == ClearColorType::Yuv) ? 1 : 0;
        // The viewport should always be adjusted regardless the clear color type, (however, since this is just clear,
        // all pixels are the same and the scissor rect will clamp the rendering area, the result is still correct
        // without this adjustment).
        vpRightShift       = 1;
    }

    ViewportParams viewportInfo = { };
    viewportInfo.count                 = 1;
    viewportInfo.viewports[0].originX  = 0;
    viewportInfo.viewports[0].originY  = 0;
    viewportInfo.viewports[0].minDepth = 0.f;
    viewportInfo.viewports[0].maxDepth = 1.f;
    viewportInfo.viewports[0].origin   = PointOrigin::UpperLeft;
    viewportInfo.horzClipRatio         = FLT_MAX;
    viewportInfo.horzDiscardRatio      = 1.0f;
    viewportInfo.vertClipRatio         = FLT_MAX;
    viewportInfo.vertDiscardRatio      = 1.0f;
    viewportInfo.depthRange            = DepthRange::ZeroToOne;

    const bool  is3dImage  = (createInfo.imageType == ImageType::Tex3d);
    ColorTargetViewCreateInfo colorViewInfo         = { };
    colorViewInfo.swizzledFormat                    = viewFormat;
    colorViewInfo.imageInfo.pImage                  = &dstImage;
    colorViewInfo.imageInfo.arraySize               = (is3dImage ? 1 : clearRange.numSlices);
    colorViewInfo.imageInfo.baseSubRes.aspect       = clearRange.startSubres.aspect;
    colorViewInfo.imageInfo.baseSubRes.arraySlice   = clearRange.startSubres.arraySlice;

    BindTargetParams bindTargetsInfo = { };
    bindTargetsInfo.colorTargets[0].imageLayout      = dstImageLayout;
    bindTargetsInfo.colorTargets[0].pColorTargetView = nullptr;

    // Save current command buffer state and bind graphics state which is common for all mipmap levels.
    pCmdBuffer->PushGraphicsState();
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics,
                                  GetGfxPipelineByTargetIndexAndFormat(SlowColorClear0_32ABGR, 0, viewFormat),
                                  InternalApiPsoHash, });
    BindCommonGraphicsState(pCmdBuffer);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 592
    if (pColor->disabledChannelMask != 0)
    {
        // Overwrite CbTargetMask for different writeMasks.
        pCmdBuffer->CmdOverrideColorWriteMaskForBlits(pColor->disabledChannelMask);
    }
#endif
    pCmdBuffer->CmdOverwriteRbPlusFormatForBlits(viewFormat, 0);
    pCmdBuffer->CmdBindColorBlendState(m_pBlendDisableState);
    pCmdBuffer->CmdBindDepthStencilState(m_pDepthDisableState);
    pCmdBuffer->CmdBindMsaaState(GetMsaaState(createInfo.samples, createInfo.fragments));

    RpmUtil::WriteVsZOut(pCmdBuffer, 1.0f);
    RpmUtil::WriteVsFirstSliceOffset(pCmdBuffer, 0);

    uint32 packedColor[4] = {0};

    if (pColor->type == ClearColorType::Yuv)
    {
        // If clear color type is Yuv, the image format should used to determine the clear color swizzling and packing
        // for planar YUV formats since the baseFormat is subresource's format which is not a YUV format.
        // NOTE: if clear color type is Uint, the client is responsible for:
        //       1. packing and swizzling clear color for packed YUV formats (e.g. packing in YUYV order for YUY2).
        //       2. passing correct clear color for this plane for planar YUV formats (e.g. two uint32s for U and V if
        //          current plane is CbCr).
        const SwizzledFormat imgFormat = createInfo.swizzledFormat;
        Formats::ConvertYuvColor(imgFormat, clearRange.startSubres.aspect, &pColor->u32Color[0], &packedColor[0]);
    }
    else
    {
        uint32 convertedColor[4] = {0};

        if (pColor->type == ClearColorType::Float)
        {
            Formats::ConvertColor(baseFormat, &pColor->f32Color[0], &convertedColor[0]);
        }
        else
        {
            memcpy(&convertedColor[0], &pColor->u32Color[0], sizeof(convertedColor));
        }

        RpmUtil::ConvertClearColorToNativeFormat(baseFormat, viewFormat, &convertedColor[0]);

        // If we can clear with raw format replacement which is more efficient, swizzle it into the order
        // required and then pack it.
        if (rawFmtOk)
        {
            uint32 swizzledColor[4] = {0};
            Formats::SwizzleColor(baseFormat, &convertedColor[0], &swizzledColor[0]);
            Formats::PackRawClearColor(baseFormat, &swizzledColor[0], &packedColor[0]);
        }
        else
        {
            memcpy(&packedColor[0], &convertedColor[0], sizeof(packedColor));
        }
    }

    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, RpmPsClearFirstUserData, 4, &packedColor[0]);

    // Each mipmap needs to be cleared individually.
    const uint32 lastMip = (clearRange.startSubres.mipLevel + clearRange.numMips - 1);

    // Boxes are only meaningful if we're clearing a single mip.
    PAL_ASSERT((boxCount == 0) || ((pBoxes != nullptr) && (clearRange.numMips == 1)));

    for (uint32 mip = clearRange.startSubres.mipLevel; mip <= lastMip; ++mip)
    {
        const SubresId mipSubres  = { clearRange.startSubres.aspect, mip, 0 };
        const auto&    subResInfo = *dstImage.SubresourceInfo(mipSubres);

        // All slices of the same mipmap level can re-use the same viewport state.
        viewportInfo.viewports[0].width  = static_cast<float>(subResInfo.extentTexels.width >> vpRightShift);
        viewportInfo.viewports[0].height = static_cast<float>(subResInfo.extentTexels.height);

        pCmdBuffer->CmdSetViewports(viewportInfo);

        colorViewInfo.imageInfo.baseSubRes.mipLevel = mip;
        SlowClearGraphicsOneMip(pCmdBuffer,
                                dstImage,
                                clearRange,
                                boxCount,
                                pBoxes,
                                mip,
                                &colorViewInfo,
                                &bindTargetsInfo,
                                xRightShift);
    }

    // Restore original command buffer state.
    pCmdBuffer->PopGraphicsState();
}

// =====================================================================================================================
// Builds commands to slow clear a range of an image for a given mip level.
void RsrcProcMgr::SlowClearGraphicsOneMip(
    GfxCmdBuffer*              pCmdBuffer,
    const Image&               dstImage,
    const SubresRange&         clearRange,
    uint32                     boxCount,
    const Box*                 pBoxes,
    uint32                     mip,
    ColorTargetViewCreateInfo* pColorViewInfo,
    BindTargetParams*          pBindTargetsInfo,
    uint32                     xRightShift
    ) const
{
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());
    // Don't expect GFX Blts on Nested unless targets not inherited.
    PAL_ASSERT((pCmdBuffer->IsNested() == false) || (static_cast<UniversalCmdBuffer*>(
        pCmdBuffer)->GetGraphicsState().inheritedState.stateFlags.targetViewState == 0));

    const auto& createInfo = dstImage.GetImageCreateInfo();
    const bool  is3dImage  = (createInfo.imageType == ImageType::Tex3d);
    ColorTargetViewInternalCreateInfo colorViewInfoInternal = {};

    const SubresId mipSubres  = { clearRange.startSubres.aspect, mip, 0 };
    const auto&    subResInfo = *dstImage.SubresourceInfo(mipSubres);

    // If rects were specified, then we'll create scissors to match the rects and do a Draw for each one. Otherwise
    // we'll use the full image scissor and a single draw.
    const bool   hasBoxes     = (boxCount > 0);
    const uint32 scissorCount = hasBoxes ? boxCount : 1;

    if (is3dImage == false)
    {
        LinearAllocatorAuto<VirtualLinearAllocator> sliceAlloc(pCmdBuffer->Allocator(), false);

        // Create and bind a color-target view for this mipmap level and slice.
        IColorTargetView* pColorView = nullptr;
        void* pColorViewMem =
            PAL_MALLOC(m_pDevice->GetColorTargetViewSize(nullptr), &sliceAlloc, AllocInternalTemp);

        if (pColorViewMem == nullptr)
        {
            pCmdBuffer->NotifyAllocFailure();
        }
        else
        {
            Result result = m_pDevice->CreateColorTargetView(*pColorViewInfo,
                                                             colorViewInfoInternal,
                                                             pColorViewMem,
                                                             &pColorView);
            PAL_ASSERT(result == Result::Success);

            pBindTargetsInfo->colorTargets[0].pColorTargetView = pColorView;
            pBindTargetsInfo->colorTargetCount = 1;
            pCmdBuffer->CmdBindTargets(*pBindTargetsInfo);

            for (uint32 i = 0; i < scissorCount; i++)
            {
                ClearImageOneBox(pCmdBuffer, subResInfo, &pBoxes[i], hasBoxes, xRightShift,
                    pColorViewInfo->imageInfo.arraySize);
            }

            // Unbind the color-target view and destroy it.
            pBindTargetsInfo->colorTargetCount = 0;
            pCmdBuffer->CmdBindTargets(*pBindTargetsInfo);

            PAL_SAFE_FREE(pColorViewMem, &sliceAlloc);
        }
    }
    else
    {
        // For 3d image, the start and end slice is based on the z offset and depth extend of the boxes.
        // The slices must be specified using the zRange because the imageInfo "slice" refers to image subresources.
        pColorViewInfo->flags.zRangeValid = 1;

        for (uint32 i = 0; i < scissorCount; i++)
        {
            LinearAllocatorAuto<VirtualLinearAllocator> sliceAlloc(pCmdBuffer->Allocator(), false);

            // Create and bind a color-target view for this mipmap level and z offset.
            IColorTargetView* pColorView = nullptr;
            void* pColorViewMem =
                PAL_MALLOC(m_pDevice->GetColorTargetViewSize(nullptr), &sliceAlloc, AllocInternalTemp);

            if (pColorViewMem == nullptr)
            {
                pCmdBuffer->NotifyAllocFailure();
            }
            else
            {

                const Box*   pBox     = hasBoxes ? &pBoxes[i]     : nullptr;
                const uint32 maxDepth = subResInfo.extentTexels.depth;

                pColorViewInfo->zRange.extent  = hasBoxes ? pBox->extent.depth : maxDepth;
                pColorViewInfo->zRange.offset  = hasBoxes ? pBox->offset.z : 0;

                PAL_ASSERT((hasBoxes == false) || (pBox->extent.depth <= maxDepth));

                Result result = m_pDevice->CreateColorTargetView(*pColorViewInfo,
                                                                 colorViewInfoInternal,
                                                                 pColorViewMem,
                                                                 &pColorView);
                PAL_ASSERT(result == Result::Success);

                pBindTargetsInfo->colorTargets[0].pColorTargetView = pColorView;
                pBindTargetsInfo->colorTargetCount = 1;
                pCmdBuffer->CmdBindTargets(*pBindTargetsInfo);

                ClearImageOneBox(pCmdBuffer, subResInfo, pBox, hasBoxes, xRightShift, pColorViewInfo->zRange.extent);

                // Unbind the color-target view and destroy it.
                pBindTargetsInfo->colorTargetCount = 0;
                pCmdBuffer->CmdBindTargets(*pBindTargetsInfo);

                PAL_SAFE_FREE(pColorViewMem, &sliceAlloc);
            }
        }
    }
}

// =====================================================================================================================
// Builds commands to clear a range of an image for a given box.
void RsrcProcMgr::ClearImageOneBox(
    GfxCmdBuffer*          pCmdBuffer,
    const SubResourceInfo& subResInfo,
    const Box*             pBox,
    bool                   hasBoxes,
    uint32                 xRightShift,
    uint32                 numInstances
    ) const
{
    // Create a scissor state for this mipmap level, slice, and current scissor.
    ScissorRectParams scissorInfo = {};
    scissorInfo.count = 1;

    if (hasBoxes)
    {
        scissorInfo.scissors[0].offset.x      = pBox->offset.x >> xRightShift;
        scissorInfo.scissors[0].offset.y      = pBox->offset.y;
        scissorInfo.scissors[0].extent.width  = pBox->extent.width >> xRightShift;
        scissorInfo.scissors[0].extent.height = pBox->extent.height;
    }
    else
    {
        scissorInfo.scissors[0].extent.width  = subResInfo.extentTexels.width >> xRightShift;
        scissorInfo.scissors[0].extent.height = subResInfo.extentTexels.height;
    }

    pCmdBuffer->CmdSetScissorRects(scissorInfo);

    // Draw a fullscreen quad.
    pCmdBuffer->CmdDraw(0, 3, 0, numInstances, 0);
}

// =====================================================================================================================
// Builds commands to slow clear a range of an image to the given raw color data using a compute shader.
void RsrcProcMgr::SlowClearCompute(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       dstImage,
    ImageLayout        dstImageLayout,
    SwizzledFormat     dstFormat,
    const ClearColor*  pColor,
    const SubresRange& clearRange,
    uint32             boxCount,
    const Box*         pBoxes
    ) const
{
    // If the image isn't in a layout that allows format replacement this clear path won't work.
    PAL_ASSERT(dstImage.GetGfxImage()->IsFormatReplaceable(clearRange.startSubres, dstImageLayout, true));

    // Get some useful information about the image.
    const auto&     createInfo   = dstImage.GetImageCreateInfo();
    const ImageType imageType    = dstImage.GetGfxImage()->GetOverrideImageType();
    uint32          texelScale   = 1;
    uint32          texelShift   = 0;
    bool            singleSubRes = false;
    bool            rawFmtOk     = dstImage.GetGfxImage()->IsFormatReplaceable(clearRange.startSubres,
                                                                               dstImageLayout,
                                                                               true);
    const auto&          subresInfo = *dstImage.SubresourceInfo(clearRange.startSubres);
    const SwizzledFormat baseFormat = subresInfo.format;
    SwizzledFormat       viewFormat = rawFmtOk ? RpmUtil::GetRawFormat(dstFormat.format, &texelScale, &singleSubRes)
                                               : baseFormat;

    // For packed YUV image use X32_Uint instead of X16_Uint to fill with YUYV.
    if ((viewFormat.format == ChNumFormat::X16_Uint) && Formats::IsYuvPacked(dstFormat.format))
    {
        viewFormat.format  = ChNumFormat::X32_Uint;
        viewFormat.swizzle = { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };
        rawFmtOk           = false;
        // The extent and offset need to be adjusted to 1/2 size.
        texelShift         = (pColor->type == ClearColorType::Yuv) ? 1 : 0;
    }

    // These are the only two supported texel scales
    PAL_ASSERT((texelScale == 1) || (texelScale == 3));

    // Get the appropriate pipeline.
    auto pipelineEnum = RpmComputePipeline::Count;
    switch (imageType)
    {
    case ImageType::Tex1d:
        pipelineEnum = ((texelScale == 1)
                        ? RpmComputePipeline::ClearImage1d
                        : RpmComputePipeline::ClearImage1dTexelScale);
        break;

    case ImageType::Tex2d:
        pipelineEnum = ((texelScale == 1)
                        ? RpmComputePipeline::ClearImage2d
                        : RpmComputePipeline::ClearImage2dTexelScale);
        break;

    default:
        pipelineEnum = ((texelScale == 1)
                        ? RpmComputePipeline::ClearImage3d
                        : RpmComputePipeline::ClearImage3dTexelScale);
        break;
    }

    const ComputePipeline*  pPipeline  = GetPipeline(pipelineEnum);

    // Get number of threads per group in each dimension.
    uint32 threadsPerGroup[3] = {0};
    pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

    // Save current command buffer state and bind the pipeline.
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    // Pack the clear color into the raw format and write it to user data 2-5.
    uint32 packedColor[4] = {0};

    uint32 convertedColor[4] = {0};
    if (pColor->type == ClearColorType::Yuv)
    {
        // If clear color type is Yuv, the image format should used to determine the clear color swizzling and packing
        // for planar YUV formats since the baseFormat is subresource's format which is not a YUV format.
        // NOTE: if clear color type is Uint, the client is responsible for:
        //       1. packing and swizzling clear color for packed YUV formats (e.g. packing in YUYV order for YUY2)
        //       2. passing correct clear color for this plane for planar YUV formats (e.g. two uint32s for U and V if
        //          current plane is CbCr).
        const SwizzledFormat imgFormat = createInfo.swizzledFormat;
        Formats::ConvertYuvColor(imgFormat, clearRange.startSubres.aspect, &pColor->u32Color[0], &packedColor[0]);
    }
    else
    {
        if (pColor->type == ClearColorType::Float)
        {
            Formats::ConvertColor(dstFormat, &pColor->f32Color[0], &convertedColor[0]);
        }
        else
        {
            memcpy(&convertedColor[0], &pColor->u32Color[0], sizeof(convertedColor));
        }

        uint32 swizzledColor[4] = {0};
        Formats::SwizzleColor(dstFormat, &convertedColor[0], &swizzledColor[0]);
        Formats::PackRawClearColor(dstFormat, &swizzledColor[0], &packedColor[0]);
    }

    // Split the clear range into sections with constant mip/array levels and loop over them.
    SubresRange  singleMipRange = { clearRange.startSubres, 1, clearRange.numSlices };
    const uint32 firstMipLevel  = clearRange.startSubres.mipLevel;
    const uint32 lastMipLevel   = clearRange.startSubres.mipLevel + clearRange.numMips - 1;
    const uint32 lastArraySlice = clearRange.startSubres.arraySlice + clearRange.numSlices - 1;

    // If single subres is requested for the format, iterate slice-by-slice and mip-by-mip.
    if (singleSubRes)
    {
        singleMipRange.numSlices = 1;
    }

    // We will do a dispatch for every box. If no boxes are specified then we will do a single full image dispatch.
    const bool   hasBoxes      = (boxCount > 0);
    const uint32 dispatchCount = hasBoxes ? boxCount : 1;

    // Boxes are only meaningful if we're clearing a single mip.
    PAL_ASSERT((hasBoxes == false) || ((pBoxes != nullptr) && (clearRange.numMips == 1)));

    // The user data will contain:
    //   [ 0 :  3] Clear color
    //   [ 4 : 10] Offset and extent
    uint32 userData[11] =
    {
        packedColor[0],
        packedColor[1],
        packedColor[2],
        packedColor[3],
        0, 0, 0, 0, 0, 0, 0
    };

    const auto& device  = *m_pDevice->Parent();

    for (;
         singleMipRange.startSubres.arraySlice <= lastArraySlice;
         singleMipRange.startSubres.arraySlice += singleMipRange.numSlices)
    {
        singleMipRange.startSubres.mipLevel = firstMipLevel;
        for (; singleMipRange.startSubres.mipLevel <= lastMipLevel; ++singleMipRange.startSubres.mipLevel)
        {
            const auto& subResInfo = *dstImage.SubresourceInfo(singleMipRange.startSubres);

            // Create an embedded SRD table and bind it to user data 0. We only need a single image view.
            // Populate the table with an image view and the embedded user data. The view should cover this
            // mip's clear range and use a raw format.
            const uint32 DataDwords = NumBytesToNumDwords(sizeof(userData));
            ImageViewInfo imageView = {};
            PAL_ASSERT(TestAnyFlagSet(dstImageLayout.usages, LayoutShaderWrite | LayoutCopyDst) == true);
            RpmUtil::BuildImageViewInfo(&imageView,
                                        dstImage,
                                        singleMipRange,
                                        viewFormat,
                                        dstImageLayout,
                                        device.TexOptLevel());

            // The default clear box is the entire subresource. This will be changed per-dispatch if boxes are enabled.
            Extent3d clearExtent = subResInfo.extentTexels;
            Offset3d clearOffset = {};

            for (uint32 i = 0; i < dispatchCount; i++)
            {
                uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(
                    pCmdBuffer,
                    SrdDwordAlignment() + DataDwords,
                    SrdDwordAlignment(),
                    PipelineBindPoint::Compute,
                    0);

                device.CreateImageViewSrds(1, &imageView, pSrdTable);
                pSrdTable += SrdDwordAlignment();

                if (hasBoxes)
                {
                    clearExtent = pBoxes[i].extent;
                    clearOffset = pBoxes[i].offset;
                }

                if (texelShift != 0)
                {
                    clearExtent.width >>= texelShift;
                    clearOffset.x     >>= texelShift;
                }

                // Compute the minimum number of threads to dispatch. Note that only 2D images can have multiple
                // samples and 3D images cannot have multiple slices.
                uint32 minThreads[3] = { clearExtent.width, 1, 1, };
                switch (imageType)
                {
                case ImageType::Tex1d:
                    // For 1d the shader expects the x offset, an unused dword, then the clear width.
                    // ClearImage1D:dcl_num_thread_per_group 64, 1, 1, Y and Z direction threads are 1
                    userData[4] = clearOffset.x;
                    userData[6] = clearExtent.width;

                    // 1D images can only have a single-sample, but they can have multiple slices.
                    minThreads[2] = singleMipRange.numSlices;
                    break;

                case ImageType::Tex2d:
                    minThreads[1] = clearExtent.height;
                    minThreads[2] = singleMipRange.numSlices * createInfo.samples;
                    // For 2d the shader expects x offset, y offset, clear width then clear height.
                    userData[4]  = clearOffset.x;
                    userData[5]  = clearOffset.y;
                    userData[6]  = clearExtent.width;
                    userData[7]  = clearExtent.height;
                    break;

                default:
                    // 3d image
                    minThreads[1] = clearExtent.height;
                    minThreads[2] = clearExtent.depth;
                    // For 3d the shader expects x, y z offsets, an unused dword then the width, height and depth.
                    userData[4]  = clearOffset.x;
                    userData[5]  = clearOffset.y;
                    userData[6]  = clearOffset.z;

                    userData[8]  = clearExtent.width;
                    userData[9]  = clearExtent.height;
                    userData[10] = clearExtent.depth;
                    break;
                }

                // Copy the user-data values into the descriptor table memory.
                memcpy(pSrdTable, &userData[0], sizeof(userData));

                // Execute the dispatch.
                pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroups(minThreads[0], threadsPerGroup[0]),
                                        RpmUtil::MinThreadGroups(minThreads[1], threadsPerGroup[1]),
                                        RpmUtil::MinThreadGroups(minThreads[2], threadsPerGroup[2]));
            }
        }
    }

    // Restore original command buffer state.
    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
// Builds commands to clear the contents of the buffer view (or the given ranges) to the given clear color.
// The simplest way to implement this is to decode the SRD's view info and reuse CmdClearColorBuffer.
void RsrcProcMgr::CmdClearBufferView(
    GfxCmdBuffer*     pCmdBuffer,
    const IGpuMemory& dstGpuMemory,
    const ClearColor& color,
    const void*       pBufferViewSrd,
    uint32            rangeCount,
    const Range*      pRanges
    ) const
{
    // Decode the buffer SRD.
    BufferViewInfo viewInfo = {};
    HwlDecodeBufferViewSrd(pBufferViewSrd, &viewInfo);

    // We need the offset and extent of the buffer wrt. the dstGpuMemory in units of texels.
    const uint32 viewStride = Formats::BytesPerPixel(viewInfo.swizzledFormat.format);
    const uint32 viewOffset = static_cast<uint32>(viewInfo.gpuAddr - dstGpuMemory.Desc().gpuVirtAddr);
    const uint32 viewExtent = static_cast<uint32>(viewInfo.range);

    // The view's offset and extent must be multiples of the view's texel stride.
    PAL_ASSERT((viewOffset % viewStride == 0) && (viewExtent % viewStride == 0));

    const uint32 offset = viewOffset / viewStride;
    const uint32 extent = viewExtent / viewStride;
    CmdClearColorBuffer(pCmdBuffer, dstGpuMemory, color, viewInfo.swizzledFormat, offset, extent, rangeCount, pRanges);
}

// =====================================================================================================================
// Builds commands to clear the contents of the buffer (or the given ranges) to the given clear color.
void RsrcProcMgr::CmdClearColorBuffer(
    GfxCmdBuffer*     pCmdBuffer,
    const IGpuMemory& dstGpuMemory,
    const ClearColor& color,
    SwizzledFormat    bufferFormat,
    uint32            bufferOffset,
    uint32            bufferExtent,
    uint32            rangeCount,
    const Range*      pRanges
    ) const
{
    const auto& settings  = m_pDevice->Parent()->Settings();

    ClearColor clearColor = color;

    uint32 convertedColor[4] = {0};

    if (clearColor.type == ClearColorType::Float)
    {
        Formats::ConvertColor(bufferFormat, &clearColor.f32Color[0], &convertedColor[0]);
    }
    else
    {
        memcpy(&convertedColor[0], &clearColor.u32Color[0], sizeof(convertedColor));
    }

    // Pack the clear color into the form it is expected to take in memory.
    constexpr uint32 PackedColorDwords = 4;
    uint32           packedColor[PackedColorDwords] = {0};
    Formats::PackRawClearColor(bufferFormat, &convertedColor[0], &packedColor[0]);

    // This is the raw format that we will be writing.
    const SwizzledFormat rawFormat = RpmUtil::GetRawFormat(bufferFormat.format, nullptr, nullptr);
    const uint32         rawStride = Formats::BytesPerPixel(rawFormat.format);

    // Build an SRD we can use to write to any texel within the buffer using our raw format.
    BufferViewInfo dstViewInfo = {};
    dstViewInfo.gpuAddr        = dstGpuMemory.Desc().gpuVirtAddr + rawStride * bufferOffset;
    dstViewInfo.range          = rawStride * bufferExtent;
    dstViewInfo.stride         = rawStride;
    dstViewInfo.swizzledFormat = rawFormat;

    uint32 dstSrd[4] = {0};
    PAL_ASSERT(m_pDevice->Parent()->ChipProperties().srdSizes.bufferView == sizeof(dstSrd));

    m_pDevice->Parent()->CreateTypedBufferViewSrds(1, &dstViewInfo, dstSrd);

    // Get the appropriate pipeline.
    const auto*const pPipeline       = GetPipeline(RpmComputePipeline::ClearBuffer);
    const uint32     threadsPerGroup = pPipeline->ThreadsPerGroup();

    // Save current command buffer state and bind the pipeline.
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    // We will do a dispatch for every range. If no ranges are specified then we will do a single full buffer dispatch.
    const Range defaultRange = { 0, bufferExtent };

    const bool   hasRanges       = (rangeCount > 0);
    const uint32 dispatchCount   = hasRanges ? rangeCount : 1;
    const Range* pDispatchRanges = hasRanges ? pRanges    : &defaultRange;

    for (uint32 i = 0; i < dispatchCount; i++)
    {
        // Create an embedded SRD table and bind it to user data 0-1. We only need a single buffer view.
        // Populate the table with a buffer view and the necessary embedded user data (clear color, offset, and extent).
        constexpr uint32 DataDwords = PackedColorDwords + 2;
        uint32*          pSrdTable  = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                             SrdDwordAlignment() + DataDwords,
                                                                             SrdDwordAlignment(),
                                                                             PipelineBindPoint::Compute,
                                                                             0);

        // Copy in the raw buffer view.
        memcpy(pSrdTable, dstSrd, sizeof(dstSrd));
        pSrdTable += SrdDwordAlignment();

        // Copy in the packed clear color.
        memcpy(pSrdTable, packedColor, sizeof(packedColor));
        pSrdTable += PackedColorDwords;

        // The final two entries in the table are the range offset and range extent.
        pSrdTable[0] = pDispatchRanges[i].offset;
        pSrdTable[1] = pDispatchRanges[i].extent;

        // Verify that the range is contained within the view.
        PAL_ASSERT((pDispatchRanges[i].offset >= 0) &&
                   (pDispatchRanges[i].offset + pDispatchRanges[i].extent <= bufferExtent));

        // Execute the dispatch.
        const uint32 numThreadGroups = RpmUtil::MinThreadGroups(pDispatchRanges[i].extent, threadsPerGroup);

        pCmdBuffer->CmdDispatch(numThreadGroups, 1, 1);
    }

    // Restore original command buffer state.
    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
// Builds commands to clear the contents of the image view (or the given boxes) to the given clear color.
// Given that the destination image is in a shader writeable layout we must do this clear using a compute slow clear.
// The simplest way to implement this is to decode the SRD's format and range and reuse SlowClearCompute.
void RsrcProcMgr::CmdClearImageView(
    GfxCmdBuffer*     pCmdBuffer,
    const Image&      dstImage,
    ImageLayout       dstImageLayout,
    const ClearColor& color,
    const void*       pImageViewSrd,
    uint32            rectCount,
    const Rect*       pRects
    ) const
{
    // Get the SRD's format and subresource range.
    SwizzledFormat srdFormat    = {};
    SubresRange    srdRange     = {};

    HwlDecodeImageViewSrd(pImageViewSrd, dstImage, &srdFormat, &srdRange);

    ClearColor  clearColor = color;
    const auto& createInfo = dstImage.GetImageCreateInfo();

    if (rectCount != 0)
    {
        Box* pBoxes = PAL_NEW_ARRAY(Box, rectCount, m_pDevice->GetPlatform(), AllocObject);

        if (pBoxes != nullptr)
        {
            for (uint32 i = 0; i < rectCount; i++)
            {
                pBoxes[i].offset.x = pRects[i].offset.x;
                pBoxes[i].offset.y = pRects[i].offset.y;
                pBoxes[i].offset.z = srdRange.startSubres.arraySlice;

                pBoxes[i].extent.width  = pRects[i].extent.width;
                pBoxes[i].extent.height = pRects[i].extent.height;
                pBoxes[i].extent.depth  = srdRange.numSlices;
            }

            SlowClearCompute(pCmdBuffer, dstImage, dstImageLayout, srdFormat, &clearColor, srdRange, rectCount, pBoxes);
            PAL_DELETE_ARRAY(pBoxes, m_pDevice->GetPlatform());
        }
        else
        {
            // Memory allocation failed.
            PAL_ASSERT_ALWAYS();
        }
    }
    else
    {
        SlowClearCompute(pCmdBuffer, dstImage, dstImageLayout, srdFormat, &clearColor, srdRange, rectCount, nullptr);
    }
}

// =====================================================================================================================
// Expand DCC/Fmask/HTile and sync before resolve image.
void RsrcProcMgr::LateExpandResolveSrc(
    GfxCmdBuffer*             pCmdBuffer,
    const Image&              srcImage,
    ImageLayout               srcImageLayout,
    const ImageResolveRegion* pRegions,
    uint32                    regionCount,
    ResolveMethod             method
    ) const
{
    const ImageLayoutUsageFlags shaderUsage =
        (method.shaderCsFmask ? Pal::LayoutShaderFmaskBasedRead : Pal::LayoutShaderRead);

    if (((method.shaderCsFmask != 0) || (method.shaderCs != 0)) &&
        (TestAnyFlagSet(srcImageLayout.usages, shaderUsage) == false))
    {
        BarrierTransition transition = { };
        transition.imageInfo.pImage            = &srcImage;
        transition.imageInfo.oldLayout.usages  = srcImageLayout.usages;
        transition.imageInfo.oldLayout.engines = srcImageLayout.engines;
        transition.imageInfo.newLayout.usages  = srcImageLayout.usages | shaderUsage;
        transition.imageInfo.newLayout.engines = srcImageLayout.engines;
        transition.srcCacheMask                = Pal::CoherResolve;
        transition.dstCacheMask                = Pal::CoherShader;

        LateExpandResolveSrcHelper(pCmdBuffer, pRegions, regionCount, transition, HwPipePreBlt);
    }
}

// =====================================================================================================================
// Inserts a barrier after a compute resolve for a color image.  Returns the image to the ResolveSrc layout after the
// internal compute shader runs.
void RsrcProcMgr::FixupLateExpandResolveSrc(
    GfxCmdBuffer*             pCmdBuffer,
    const Image&              srcImage,
    ImageLayout               srcImageLayout,
    const ImageResolveRegion* pRegions,
    uint32                    regionCount,
    ResolveMethod             method
    ) const
{
    const ImageLayoutUsageFlags shaderUsage =
        (method.shaderCsFmask ? Pal::LayoutShaderFmaskBasedRead : Pal::LayoutShaderRead);

    if (((method.shaderCsFmask != 0) || (method.shaderCs != 0)) &&
        (TestAnyFlagSet(srcImageLayout.usages, shaderUsage) == false))
    {
        BarrierTransition transition = { };
        transition.imageInfo.pImage             = &srcImage;
        transition.imageInfo.oldLayout.usages   = srcImageLayout.usages | shaderUsage;
        transition.imageInfo.oldLayout.engines  = srcImageLayout.engines;
        transition.imageInfo.newLayout.usages   = srcImageLayout.usages;
        transition.imageInfo.newLayout.engines  = srcImageLayout.engines;

        transition.srcCacheMask = Pal::CoherShader;
        transition.dstCacheMask = Pal::CoherResolve;

        LateExpandResolveSrcHelper(pCmdBuffer, pRegions, regionCount, transition, HwPipePostBlt);
    }
}

// =====================================================================================================================
// Helper function for setting up a barrier used before and after a compute shader resolve.
void RsrcProcMgr::LateExpandResolveSrcHelper(
    GfxCmdBuffer*             pCmdBuffer,
    const ImageResolveRegion* pRegions,
    uint32                    regionCount,
    const BarrierTransition&  transition,
    HwPipePoint               waitPoint
    ) const
{
    const Image& image = *static_cast<const Image*>(transition.imageInfo.pImage);

    AutoBuffer<BarrierTransition, 32, Platform> transitions(regionCount, m_pDevice->GetPlatform());

    if (transitions.Capacity() >= regionCount)
    {
        for (uint32 i = 0; i < regionCount; i++)
        {
            transitions[i].imageInfo.subresRange.startSubres.aspect     = pRegions[i].srcAspect;
            transitions[i].imageInfo.subresRange.startSubres.arraySlice = pRegions[i].srcSlice;
            transitions[i].imageInfo.subresRange.startSubres.mipLevel   = 0;
            transitions[i].imageInfo.subresRange.numMips                = 1;
            transitions[i].imageInfo.subresRange.numSlices              = pRegions[i].numSlices;

            transitions[i].imageInfo.pImage             = &image;
            transitions[i].imageInfo.oldLayout          = transition.imageInfo.oldLayout;
            transitions[i].imageInfo.newLayout          = transition.imageInfo.newLayout;
            transitions[i].imageInfo.pQuadSamplePattern = pRegions[i].pQuadSamplePattern;

            transitions[i].srcCacheMask = transition.srcCacheMask;
            transitions[i].dstCacheMask = transition.dstCacheMask;

            PAL_ASSERT((image.GetImageCreateInfo().flags.sampleLocsAlwaysKnown != 0) ==
                       (pRegions[i].pQuadSamplePattern != nullptr));
        }

        BarrierInfo barrierInfo = { };
        barrierInfo.pTransitions    = &transitions[0];
        barrierInfo.transitionCount = regionCount;
        barrierInfo.waitPoint       = waitPoint;

        const HwPipePoint releasePipePoint = Pal::HwPipeBottom;
        barrierInfo.pipePointWaitCount = 1;
        barrierInfo.pPipePoints        = &releasePipePoint;

        pCmdBuffer->CmdBarrier(barrierInfo);
    }
    else
    {
        pCmdBuffer->NotifyAllocFailure();
    }
}

// =====================================================================================================================
// This must be called before and after each compute copy. The pre-copy call will insert any required metadata
// decompresses and the post-copy call will fixup any metadata that needs updating. In practice these barriers are
// required in cases where we treat CopyDst as compressed but RPM can't actually write compressed data directly from
// the compute shader.
void RsrcProcMgr::FixupMetadataForComputeDst(
    GfxCmdBuffer*           pCmdBuffer,
    const Image&            dstImage,
    ImageLayout             dstImageLayout,
    uint32                  regionCount,
    const ImageFixupRegion* pRegions,
    bool                    beforeCopy
    ) const
{
    const GfxImage* pGfxImage = dstImage.GetGfxImage();

    // TODO: unify all RPM metadata fixup here; currently only depth image is handled.
    if (pGfxImage->HasHtileData())
    {
        // TODO: there is suspected Hiz issue on gfx10 comrpessed depth write. Suggested temporary workaround is
        // to attach layout with LayoutUncompressed, which always triggers depth expand before copy and depth
        // resummarize after copy.
        const bool enableCompressedDepthWriteTempWa = IsGfx10(*m_pDevice->Parent());

        // If enable temp workaround for comrpessed depth write, always need barriers for before and after copy.
        bool needBarrier = enableCompressedDepthWriteTempWa;
        for (uint32 i = 0; (needBarrier == false) && (i < regionCount); i++)
        {
            needBarrier = pGfxImage->ShaderWriteIncompatibleWithLayout(pRegions[i].subres, dstImageLayout);
        }

        if (needBarrier)
        {
            AutoBuffer<BarrierTransition, 32, Platform> transitions(regionCount, m_pDevice->GetPlatform());

            if (transitions.Capacity() >= regionCount)
            {
                const uint32 shaderWriteLayout =
                    (enableCompressedDepthWriteTempWa ? (LayoutShaderWrite | LayoutUncompressed) : LayoutShaderWrite);

                for (uint32 i = 0; i < regionCount; i++)
                {
                    transitions[i].imageInfo.pImage                  = &dstImage;
                    transitions[i].imageInfo.subresRange.startSubres = pRegions[i].subres;
                    transitions[i].imageInfo.subresRange.numMips     = 1;
                    transitions[i].imageInfo.subresRange.numSlices   = pRegions[i].numSlices;
                    transitions[i].imageInfo.oldLayout               = dstImageLayout;
                    transitions[i].imageInfo.newLayout               = dstImageLayout;
                    transitions[i].imageInfo.pQuadSamplePattern      = nullptr;

                    // The first barrier must prepare the image for shader writes, perhaps by decompressing metadata.
                    // The second barrier is required to undo those changes, perhaps by resummarizing the metadata.
                    if (beforeCopy)
                    {
                        // Can optimize depth expand to lighter Barrier with UninitializedTarget for full subres copy.
                        const SubResourceInfo* pSubresInfo = dstImage.SubresourceInfo(pRegions[i].subres);
                        const bool fullSubresCopy =
                            ((pRegions[i].offset.x == 0) &&
                             (pRegions[i].offset.y == 0) &&
                             (pRegions[i].offset.z == 0) &&
                             (pRegions[i].extent.width  >= pSubresInfo->extentElements.width) &&
                             (pRegions[i].extent.height >= pSubresInfo->extentElements.height) &&
                             (pRegions[i].extent.depth  >= pSubresInfo->extentElements.depth));

                        if (fullSubresCopy)
                        {
                            transitions[i].imageInfo.oldLayout.usages = LayoutUninitializedTarget;
                        }

                        transitions[i].imageInfo.newLayout.usages |= shaderWriteLayout;
                        transitions[i].srcCacheMask                = CoherCopy;
                        transitions[i].dstCacheMask                = CoherShader;
                    }
                    else // After copy
                    {
                        transitions[i].imageInfo.oldLayout.usages |= shaderWriteLayout;
                        transitions[i].srcCacheMask                = CoherShader;
                        transitions[i].dstCacheMask                = CoherCopy;
                    }
                }

                // Operations like resummarizes might read the blit's output so we can't optimize the wait point.
                BarrierInfo barrierInfo = {};
                barrierInfo.pTransitions    = &transitions[0];
                barrierInfo.transitionCount = regionCount;
                barrierInfo.waitPoint       = HwPipePreBlt;

                const HwPipePoint releasePipePoint = beforeCopy ? HwPipeBottom : HwPipePostCs;
                barrierInfo.pipePointWaitCount = 1;
                barrierInfo.pPipePoints        = &releasePipePoint;

                pCmdBuffer->CmdBarrier(barrierInfo);
            }
            else
            {
                pCmdBuffer->NotifyAllocFailure();
            }
        }
    }
}

// =====================================================================================================================
// Resolves a multisampled source Image into the single-sampled destination Image using the Image's resolve method.
void RsrcProcMgr::CmdResolveImage(
    GfxCmdBuffer*             pCmdBuffer,
    const Image&              srcImage,
    ImageLayout               srcImageLayout,
    const Image&              dstImage,
    ImageLayout               dstImageLayout,
    ResolveMode               resolveMode,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions,
    uint32                    flags
    ) const
{
    const ResolveMethod srcMethod = srcImage.GetImageInfo().resolveMethod;
    const ResolveMethod dstMethod = dstImage.GetImageInfo().resolveMethod;

    if (pCmdBuffer->GetEngineType() == EngineTypeCompute)
    {
        PAL_ASSERT((srcMethod.shaderCsFmask == 1) || (srcMethod.shaderCs == 1));
        ResolveImageCompute(pCmdBuffer,
                            srcImage,
                            srcImageLayout,
                            dstImage,
                            dstImageLayout,
                            resolveMode,
                            regionCount,
                            pRegions,
                            srcMethod,
                            flags);

        HwlFixupResolveDstImage(pCmdBuffer,
                                *dstImage.GetGfxImage(),
                                dstImageLayout,
                                pRegions,
                                regionCount,
                                true);
    }
    else
    {
        if ((srcMethod.fixedFunc == 1) && HwlCanDoFixedFuncResolve(srcImage,
                                                                   dstImage,
                                                                   resolveMode,
                                                                   regionCount,
                                                                   pRegions))
        {
            PAL_ASSERT(resolveMode == ResolveMode::Average);
            // this only support color resolves.
            ResolveImageFixedFunc(pCmdBuffer,
                                  srcImage,
                                  srcImageLayout,
                                  dstImage,
                                  dstImageLayout,
                                  regionCount,
                                  pRegions,
                                  flags);

            HwlFixupResolveDstImage(pCmdBuffer,
                                    *dstImage.GetGfxImage(),
                                    dstImageLayout,
                                    pRegions,
                                    regionCount,
                                    false);
        }
        else if ((srcMethod.depthStencilCopy == 1) && (dstMethod.depthStencilCopy == 1) &&
                 (resolveMode == ResolveMode::Average) &&
                  HwlCanDoDepthStencilCopyResolve(srcImage, dstImage, regionCount, pRegions))
        {
            ResolveImageDepthStencilCopy(pCmdBuffer,
                                         srcImage,
                                         srcImageLayout,
                                         dstImage,
                                         dstImageLayout,
                                         regionCount,
                                         pRegions,
                                         flags);

            HwlHtileCopyAndFixUp(pCmdBuffer, srcImage, dstImage, dstImageLayout, regionCount, pRegions, false);
        }
        else if (dstMethod.shaderPs && (resolveMode == ResolveMode::Average))
        {
            // this only supports Depth/Stencil resolves.
            ResolveImageGraphics(pCmdBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions, flags);
        }
        else if (pCmdBuffer->IsComputeSupported() &&
                 ((srcMethod.shaderCsFmask == 1) ||
                  (srcMethod.shaderCs == 1)))
        {
            ResolveImageCompute(pCmdBuffer,
                                srcImage,
                                srcImageLayout,
                                dstImage,
                                dstImageLayout,
                                resolveMode,
                                regionCount,
                                pRegions,
                                srcMethod,
                                flags);

            HwlFixupResolveDstImage(pCmdBuffer,
                                    *dstImage.GetGfxImage(),
                                    dstImageLayout,
                                    pRegions,
                                    regionCount,
                                    true);
        }
        else
        {
            PAL_NOT_IMPLEMENTED();
        }
    }
}

// =====================================================================================================================
// Executes a compute shader which generates a PM4 command buffer which can later be executed. If the number of indirect
// commands being generated will not fit into a single command-stream chunk, this will issue multiple dispatches, one
// for each command chunk to generate.
void RsrcProcMgr::CmdGenerateIndirectCmds(
    const GenerateInfo& genInfo,
    CmdStreamChunk**    ppChunkLists[],
    uint32              NumChunkLists,
    uint32*             pNumGenChunks
    ) const
{
    const auto&                 settings     = m_pDevice->Parent()->Settings();
    const gpusize               argsGpuAddr  = genInfo.argsGpuAddr;
    const gpusize               countGpuAddr = genInfo.countGpuAddr;
    const Pipeline*             pPipeline    = genInfo.pPipeline;
    const IndirectCmdGenerator& generator    = genInfo.generator;
    GfxCmdBuffer*               pCmdBuffer   = genInfo.pCmdBuffer;
    uint32                      indexBufSize = genInfo.indexBufSize;
    uint32                      maximumCount = genInfo.maximumCount;

    const ComputePipeline* pGenerationPipeline = GetCmdGenerationPipeline(generator, *pCmdBuffer);

    uint32 threadsPerGroup[3] = { };
    pGenerationPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pGenerationPipeline, InternalApiPsoHash, });

    // The command-generation pipelines expect the following descriptor-table layout for the resources which are the
    // same for each command-stream chunk being generated:
    //  + Raw-buffer SRD for the indirect argument data (4 DW)
    //  + Structured-buffer SRD for the command parameter data (4 DW)
    //  + Typed buffer SRD for the user-data entry mapping table for each shader stage (4 DW)
    //  + Structured-buffer SRD for the pipeline signature (4 DW)
    //  + Constant buffer SRD for the command-generator properties (4 DW)
    //  + Constant buffer SRD for the properties of the ExecuteIndirect() invocation (4 DW)
    //  + GPU address of the memory containing the count of commands to generate (2 DW)
    //  + Issue THREAD_TRACE_MARKER after draw or dispatch (1 DW)

    constexpr uint32 SrdDwords = 4;
    PAL_ASSERT(m_pDevice->Parent()->ChipProperties().srdSizes.bufferView == (sizeof(uint32) * SrdDwords));

    // The generation pipelines expect the descriptor table's GPU address to be written to user-data #0-1.
    gpusize tableGpuAddr = 0uLL;
    uint32* pTableMem = pCmdBuffer->CmdAllocateEmbeddedData(((7 * SrdDwords) + 4), 1, &tableGpuAddr);
    PAL_ASSERT(pTableMem != nullptr);

    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 0, 2, reinterpret_cast<uint32*>(&tableGpuAddr));

    // Raw-buffer SRD for the indirect-argument data:
    BufferViewInfo viewInfo = { };
    viewInfo.gpuAddr        = argsGpuAddr;
    viewInfo.swizzledFormat = UndefinedSwizzledFormat;
    viewInfo.range          = (generator.Properties().argBufStride * maximumCount);
    viewInfo.stride         = 1;
    m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &viewInfo, pTableMem);
    pTableMem += SrdDwords;

    // Structured-buffer SRD for the command parameter data:
    memcpy(pTableMem, generator.ParamBufferSrd(), (sizeof(uint32) * SrdDwords));
    pTableMem += SrdDwords;

    // Typed-buffer SRD for the user-data entry mappings:
    generator.PopulateUserDataMappingBuffer(pCmdBuffer, pPipeline, pTableMem);
    pTableMem += SrdDwords;

    // Structured buffer SRD for the pipeline signature:
    generator.PopulateSignatureBuffer(pCmdBuffer, pPipeline, pTableMem);
    pTableMem += SrdDwords;

    // Constant buffer SRD for the command-generator properties:
    memcpy(pTableMem, generator.PropertiesSrd(), (sizeof(uint32) * SrdDwords));
    pTableMem += SrdDwords;

    // Constant buffer SRD for the properties of the ExecuteIndirect() invocation:
    generator.PopulateInvocationBuffer(pCmdBuffer,
                                       pPipeline,
                                       argsGpuAddr,
                                       maximumCount,
                                       indexBufSize,
                                       pTableMem);
    pTableMem += SrdDwords;

    // GPU address of the memory containing the actual command count to generate:
    memcpy(pTableMem, &countGpuAddr, sizeof(countGpuAddr));
    pTableMem += 2;

    const auto& platformSettings = m_pDevice->Parent()->GetPlatform()->PlatformSettings();

    const bool sqttEnabled = ((platformSettings.gpuProfilerMode > GpuProfilerCounterAndTimingOnly) &&
                              Util::TestAnyFlagSet(platformSettings.gpuProfilerConfig.traceModeMask,
                                                   GpuProfilerTraceSqtt));
    const bool issueSqttMarkerEvent = (sqttEnabled |
                                       m_pDevice->Parent()->GetPlatform()->IsDevDriverProfilingEnabled());

    // Flag to decide whether to issue THREAD_TRACE_MARKER following generated draw/dispatch commands.
    pTableMem[0] = issueSqttMarkerEvent;

    uint32 commandIdOffset = 0;
    while (commandIdOffset < maximumCount)
    {
        // Obtain a command-stream chunk for generating commands into. This also sets-up the padding requirements
        // for the chunk and determines the number of commands which will safely fit. We'll need to build a raw-
        // buffer SRD so the shader can access the command buffer as a UAV.
        ChunkOutput output[2]  = {};
        const uint32 numChunks = 1;
        pCmdBuffer->GetChunkForCmdGeneration(generator,
                                             *pPipeline,
                                             (maximumCount - commandIdOffset),
                                             numChunks,
                                             output);

        ChunkOutput& mainChunk          = output[0];
        ppChunkLists[0][*pNumGenChunks] = mainChunk.pChunk;

        // The command generation pipeline also expects the following descriptor-table layout for the resources
        // which change between each command-stream chunk being generated:
        //  + Raw buffer UAV SRD for the command-stream chunk to generate (4 DW)
        //  + Raw buffer UAV SRD for the embedded data segment to use for the spill table (4 DW)
        //  + Command ID offset for the current command-stream-chunk (1 DW)
        //  + Low half of the GPU virtual address of the spill table's embedded data segment (1 DW)

        // The generation pipelines expect the descriptor table's GPU address to be written to user-data #2-3.
        pTableMem = pCmdBuffer->CmdAllocateEmbeddedData(((2 * SrdDwords) + 2), 1, &tableGpuAddr);
        PAL_ASSERT(pTableMem != nullptr);

        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 2, 2, reinterpret_cast<uint32*>(&tableGpuAddr));

        // UAV buffer SRD for the command-stream-chunk to generate:
        viewInfo.gpuAddr        = mainChunk.pChunk->GpuVirtAddr();
        viewInfo.swizzledFormat = UndefinedSwizzledFormat;
        viewInfo.range          = (mainChunk.commandsInChunk * generator.Properties().cmdBufStride);
        viewInfo.stride         = 1;
        m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &viewInfo, pTableMem);
        pTableMem += SrdDwords;

        // UAV buffer SRD for the embedded-data spill table:
        if (mainChunk.embeddedDataSize != 0)
        {
            viewInfo.gpuAddr        = mainChunk.embeddedDataAddr;
            viewInfo.swizzledFormat = UndefinedSwizzledFormat;
            viewInfo.range          = (sizeof(uint32) * mainChunk.embeddedDataSize);
            viewInfo.stride         = 1;
            m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &viewInfo, pTableMem);
        }
        else
        {
            // If we're not using the embedded-data spill table, we still need to clear the srd to 0.
            // This prevents hangs on older hardware caused by the shader attempting to read an invalid srd.
            memset(pTableMem, 0, (sizeof(uint32) * SrdDwords));
        }

        pTableMem += SrdDwords;

        // Command ID offset for the current command stream-chunk
        pTableMem[0] = commandIdOffset;
        // Low portion of the spill table's GPU virtual address
        pTableMem[1] = LowPart(mainChunk.embeddedDataAddr);

        pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroups(generator.ParameterCount(), threadsPerGroup[0]),
                                RpmUtil::MinThreadGroups(mainChunk.commandsInChunk, threadsPerGroup[1]),
                                1);

        (*pNumGenChunks)++;
        commandIdOffset += mainChunk.commandsInChunk;
    }

    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
// Resolves a multisampled depth-stencil source Image into the single-sampled destination Image using a pixel shader.
void RsrcProcMgr::ResolveImageGraphics(
    GfxCmdBuffer*             pCmdBuffer,
    const Image&              srcImage,
    ImageLayout               srcImageLayout,
    const Image&              dstImage,
    ImageLayout               dstImageLayout,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions,
    uint32                    flags
    ) const
{
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());
    // Don't expect GFX Blts on Nested unless targets not inherited.
    PAL_ASSERT((pCmdBuffer->IsNested() == false) || (static_cast<UniversalCmdBuffer*>(
        pCmdBuffer)->GetGraphicsState().inheritedState.stateFlags.targetViewState == 0));

    const auto& device        = *m_pDevice->Parent();
    const auto& dstCreateInfo = dstImage.GetImageCreateInfo();
    const auto& srcCreateInfo = srcImage.GetImageCreateInfo();
    const auto& srcImageInfo  = srcImage.GetImageInfo();

    LateExpandResolveSrc(pCmdBuffer, srcImage, srcImageLayout, pRegions, regionCount, srcImageInfo.resolveMethod);

    // This path only works on depth-stencil images.
    PAL_ASSERT((srcCreateInfo.usageFlags.depthStencil && dstCreateInfo.usageFlags.depthStencil) ||
               (Formats::IsDepthStencilOnly(srcCreateInfo.swizzledFormat.format) &&
                Formats::IsDepthStencilOnly(dstCreateInfo.swizzledFormat.format)));

    const StencilRefMaskParams       stencilRefMasks      = { 0xFF, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0xFF, 0x01, 0xFF, };

    // Initialize some structures we will need later on.
    ViewportParams viewportInfo = { };
    viewportInfo.count                 = 1;
    viewportInfo.viewports[0].minDepth = 0.f;
    viewportInfo.viewports[0].maxDepth = 1.f;
    viewportInfo.viewports[0].origin   = PointOrigin::UpperLeft;
    viewportInfo.horzClipRatio         = FLT_MAX;
    viewportInfo.horzDiscardRatio      = 1.0f;
    viewportInfo.vertClipRatio         = FLT_MAX;
    viewportInfo.vertDiscardRatio      = 1.0f;
    viewportInfo.depthRange            = DepthRange::ZeroToOne;

    ScissorRectParams scissorInfo = { };
    scissorInfo.count = 1;

    const DepthStencilViewInternalCreateInfo noDepthViewInfoInternal = { };
    DepthStencilViewCreateInfo               depthViewInfo           = { };
    depthViewInfo.pImage    = &dstImage;
    depthViewInfo.arraySize = 1;

    // Save current command buffer state and bind graphics state which is common for all regions.
    pCmdBuffer->PushGraphicsState();
    BindCommonGraphicsState(pCmdBuffer);
    pCmdBuffer->CmdBindMsaaState(GetMsaaState(dstCreateInfo.samples, dstCreateInfo.fragments));
    pCmdBuffer->CmdSetStencilRefMasks(stencilRefMasks);

    // Put ImageResolveInvertY value in user data 0 used by VS.
    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 0, 1, &flags);

    // Determine which format we should use to view the source image. The initial value is the stencil format.
    SwizzledFormat srcFormat =
    {
        ChNumFormat::Undefined,
        { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
    };

    // Each region needs to be resolved individually.
    for (uint32 idx = 0; idx < regionCount; ++idx)
    {
        // Same sanity checks of the region aspects.
        const bool isDepth = (pRegions[idx].dstAspect == ImageAspect::Depth);
        PAL_ASSERT(((pRegions[idx].srcAspect == ImageAspect::Depth) ||
                    (pRegions[idx].srcAspect == ImageAspect::Stencil)) &&
                    (pRegions[idx].srcAspect == pRegions[idx].dstAspect));

        // This path can't reinterpret the resolve format.
        const SubresId dstStartSubres =
        {
            pRegions[idx].dstAspect,
            pRegions[idx].dstMipLevel,
            pRegions[idx].dstSlice
        };

        PAL_ASSERT(Formats::IsUndefined(pRegions[idx].swizzledFormat.format) ||
                  (dstImage.SubresourceInfo(dstStartSubres)->format.format == pRegions[idx].swizzledFormat.format));

        BindTargetParams bindTargetsInfo = { };

        if (isDepth)
        {
            if ((srcCreateInfo.swizzledFormat.format == ChNumFormat::D32_Float_S8_Uint) ||
                Formats::ShareChFmt(srcCreateInfo.swizzledFormat.format, ChNumFormat::X32_Float))
            {
                srcFormat.format = ChNumFormat::X32_Float;
            }
            else
            {
                srcFormat.format = ChNumFormat::X16_Unorm;
            }

            bindTargetsInfo.depthTarget.depthLayout = dstImageLayout;
            pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics,
                                          GetGfxPipeline(ResolveDepth),
                                          InternalApiPsoHash, });
            pCmdBuffer->CmdBindDepthStencilState(m_pDepthResolveState);
        }
        else
        {
            srcFormat.format                          = ChNumFormat::X8_Uint;
            bindTargetsInfo.depthTarget.stencilLayout = dstImageLayout;
            pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics, GetGfxPipeline(ResolveStencil),
                                          InternalApiPsoHash, });
            pCmdBuffer->CmdBindDepthStencilState(m_pStencilResolveState);
        }

        // Setup the viewport and scissor to restrict rendering to the destination region being copied.
        viewportInfo.viewports[0].originX = static_cast<float>(pRegions[idx].dstOffset.x);
        viewportInfo.viewports[0].originY = static_cast<float>(pRegions[idx].dstOffset.y);
        viewportInfo.viewports[0].width   = static_cast<float>(pRegions[idx].extent.width);
        viewportInfo.viewports[0].height  = static_cast<float>(pRegions[idx].extent.height);

        scissorInfo.scissors[0].offset.x      = pRegions[idx].dstOffset.x;
        scissorInfo.scissors[0].offset.y      = pRegions[idx].dstOffset.y;
        scissorInfo.scissors[0].extent.width  = pRegions[idx].extent.width;
        scissorInfo.scissors[0].extent.height = pRegions[idx].extent.height;

        // The shader will calculate src coordinates by adding a delta to the dst coordinates. The user data should
        // contain those deltas which are (srcOffset-dstOffset) for X & Y.
        const int32  xOffset     = (pRegions[idx].srcOffset.x - pRegions[idx].dstOffset.x);
        const int32  yOffset     = (pRegions[idx].srcOffset.y - pRegions[idx].dstOffset.y);
        const uint32 userData[3] =
        {
            reinterpret_cast<const uint32&>(xOffset),
            reinterpret_cast<const uint32&>(yOffset),
        };

        pCmdBuffer->CmdSetViewports(viewportInfo);
        pCmdBuffer->CmdSetScissorRects(scissorInfo);
        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 2, 2, userData);

        for (uint32 slice = 0; slice < pRegions[idx].numSlices; ++slice)
        {
            LinearAllocatorAuto<VirtualLinearAllocator> sliceAlloc(pCmdBuffer->Allocator(), false);

            const SubresId srcSubres = { pRegions[idx].srcAspect, 0, pRegions[idx].srcSlice + slice };
            const SubresId dstSubres =
            {
                pRegions[idx].dstAspect,
                pRegions[idx].dstMipLevel,
                pRegions[idx].dstSlice + slice
            };

            // Create an embedded user-data table and bind it to user data 1. We only need one image view.
            uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                       SrdDwordAlignment(),
                                                                       SrdDwordAlignment(),
                                                                       PipelineBindPoint::Graphics,
                                                                       1);

            // Populate the table with an image view of the source image.
            ImageViewInfo     imageView = { };
            const SubresRange viewRange = { srcSubres, 1, 1 };
            RpmUtil::BuildImageViewInfo(&imageView,
                                        srcImage,
                                        viewRange,
                                        srcFormat,
                                        srcImageLayout,
                                        device.TexOptLevel());
            device.CreateImageViewSrds(1, &imageView, pSrdTable);

            // Create and bind a depth stencil view of the destination region.
            depthViewInfo.baseArraySlice = dstSubres.arraySlice;
            depthViewInfo.mipLevel       = dstSubres.mipLevel;

            void* pDepthStencilViewMem = PAL_MALLOC(m_pDevice->GetDepthStencilViewSize(nullptr),
                                                    &sliceAlloc,
                                                    AllocInternalTemp);
            if (pDepthStencilViewMem == nullptr)
            {
                pCmdBuffer->NotifyAllocFailure();
            }
            else
            {
                IDepthStencilView* pDepthView = nullptr;
                Result result = m_pDevice->CreateDepthStencilView(depthViewInfo,
                                                                  noDepthViewInfoInternal,
                                                                  pDepthStencilViewMem,
                                                                  &pDepthView);
                PAL_ASSERT(result == Result::Success);

                bindTargetsInfo.depthTarget.pDepthStencilView = pDepthView;
                pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                // Draw a fullscreen quad.
                pCmdBuffer->CmdDraw(0, 3, 0, 1, 0);

                // Unbind the depth view and destroy it.
                bindTargetsInfo.depthTarget.pDepthStencilView = nullptr;
                pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                PAL_SAFE_FREE(pDepthStencilViewMem, &sliceAlloc);
            }
        } // End for each slice.
    } // End for each region.

    // Restore original command buffer state.
    pCmdBuffer->PopGraphicsState();

    FixupLateExpandResolveSrc(pCmdBuffer, srcImage, srcImageLayout, pRegions, regionCount, srcImageInfo.resolveMethod);
}

// =====================================================================================================================
// Resolves a multisampled source Image into the single-sampled destination Image using a compute shader.
void RsrcProcMgr::ResolveImageCompute(
    GfxCmdBuffer*             pCmdBuffer,
    const Image&              srcImage,
    ImageLayout               srcImageLayout,
    const Image&              dstImage,
    ImageLayout               dstImageLayout,
    ResolveMode               resolveMode,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions,
    ResolveMethod             method,
    uint32                    flags
    ) const
{
    const auto& device   = *m_pDevice->Parent();

    LateExpandResolveSrc(pCmdBuffer, srcImage, srcImageLayout, pRegions, regionCount, method);

    // Save the command buffer's state.
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

    // Basic resolves need one slot per region per image, FMask resolves need a third slot for the source Image's FMask.
    const bool   isCsFmask = (method.shaderCsFmask == 1);
    const uint32 numSlots  = isCsFmask ? 3 : 2;

    // Execute the Resolve for each region in the specified list.
    for (uint32 idx = 0; idx < regionCount; ++idx)
    {
        // Select a Resolve shader based on the source Image's sample-count and resolve method.
        const ComputePipeline*const pPipeline = GetCsResolvePipeline(srcImage,
                                                                     pRegions[idx].srcAspect,
                                                                     resolveMode,
                                                                     method);

        uint32 threadsPerGroup[3] = {};
        pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

        // Bind the pipeline.
        pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

        // Set both subresources to the first slice of the required mip level
        const SubresId srcSubres = { pRegions[idx].srcAspect, 0, pRegions[idx].srcSlice };
        const SubresId dstSubres = { pRegions[idx].dstAspect, pRegions[idx].dstMipLevel, pRegions[idx].dstSlice };

        SwizzledFormat srcFormat = srcImage.SubresourceInfo(srcSubres)->format;
        SwizzledFormat dstFormat = dstImage.SubresourceInfo(dstSubres)->format;

        // Override the formats with the caller's "reinterpret" format.
        if ((Formats::IsUndefined(pRegions[idx].swizzledFormat.format) == false) &&
            (Formats::IsUndefined(pRegions[idx].swizzledFormat.format) == false))
        {
            // We require that the channel formats match.
            PAL_ASSERT(Formats::ShareChFmt(srcFormat.format, pRegions[idx].swizzledFormat.format));
            PAL_ASSERT(Formats::ShareChFmt(dstFormat.format, pRegions[idx].swizzledFormat.format));

            // If the specified format exactly matches the image formats the resolve will always work. Otherwise, the
            // images must support format replacement.
            PAL_ASSERT(Formats::HaveSameNumFmt(srcFormat.format, pRegions[idx].swizzledFormat.format) ||
                       srcImage.GetGfxImage()->IsFormatReplaceable(srcSubres, srcImageLayout, false));

            PAL_ASSERT(Formats::HaveSameNumFmt(dstFormat.format, pRegions[idx].swizzledFormat.format) ||
                       dstImage.GetGfxImage()->IsFormatReplaceable(dstSubres, dstImageLayout, true));

            srcFormat.format = pRegions[idx].swizzledFormat.format;
            dstFormat.format = pRegions[idx].swizzledFormat.format;
        }

        // Store the necessary region independent user data values in slots 1-4. Shader expects the following layout:
        // 1 - Num Samples
        // 2 - Gamma correction option (1 if the destination format is SRGB, 0 otherwise)
        // 3 - Copy sample 0 (single sample) flag. (1 for integer formats, 0 otherwise). For DS images this flag
        //     is 1 if resolve mode is set as average.
        // 4 - Y-invert
        const uint32 imageData[4] =
        {
            srcImage.GetImageCreateInfo().samples,
            Formats::IsSrgb(dstFormat.format),
            ((pRegions[idx].srcAspect == ImageAspect::Stencil) ? (resolveMode == ResolveMode::Average)
                                                               : (Formats::IsSint(srcFormat.format) ||
                                                                  Formats::IsUint(srcFormat.format))),
            TestAnyFlagSet(flags, ImageResolveInvertY),
        };

        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 1, 4, &imageData[0]);

        // The hardware can't handle UAV stores using SRGB num format.  The resolve shaders already contain a
        // linear-to-gamma conversion, but in order for that to work the output UAV's num format must be patched to be
        // simple unorm.
        if (Formats::IsSrgb(dstFormat.format))
        {
            dstFormat.format = Formats::ConvertToUnorm(dstFormat.format);
        }

        // The shader expects the following layout for the embedded user-data constants.
        // Src Offset X, Src Y offset, Resolve width, Resolve height
        // Dst Offset X, Dst Y offset

        const uint32 regionData[6] =
        {
            static_cast<uint32>(pRegions[idx].srcOffset.x),
            static_cast<uint32>(pRegions[idx].srcOffset.y),
            pRegions[idx].extent.width,
            pRegions[idx].extent.height,
            static_cast<uint32>(pRegions[idx].dstOffset.x),
            static_cast<uint32>(pRegions[idx].dstOffset.y)
        };

        // Create an embedded user-data table and bind it to user data 0. We need image views for the src and dst
        // subresources, as well as some inline constants for the resolve offsets and extents.
        const uint32 DataDwords = NumBytesToNumDwords(sizeof(regionData));
        uint32*      pUserData  = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                         SrdDwordAlignment() * numSlots + DataDwords,
                                                                         SrdDwordAlignment(),
                                                                         PipelineBindPoint::Compute,
                                                                         0);

        ImageViewInfo imageView[2] = {};
        SubresRange   viewRange = { dstSubres, 1, pRegions[idx].numSlices };

        PAL_ASSERT(TestAnyFlagSet(dstImageLayout.usages, LayoutResolveDst) == true);

        // ResolveDst doesn't imply ShaderWrite, but it's safe because it's always uncompressed
        ImageLayout dstLayoutCompute  = dstImageLayout;
        dstLayoutCompute.usages      |= LayoutShaderWrite;

        // Destination image is at the beginning of pUserData.
        RpmUtil::BuildImageViewInfo(&imageView[0],
                                    dstImage,
                                    viewRange,
                                    dstFormat,
                                    dstLayoutCompute,
                                    device.TexOptLevel());

        viewRange.startSubres = srcSubres;
        RpmUtil::BuildImageViewInfo(&imageView[1],
                                    srcImage,
                                    viewRange,
                                    srcFormat,
                                    srcImageLayout,
                                    device.TexOptLevel());

        device.CreateImageViewSrds(2, &imageView[0], pUserData);
        pUserData += SrdDwordAlignment() * 2;

        if (isCsFmask)
        {
            // If this is an Fmask-accelerated Resolve, create a third image view of the source Image's Fmask surface.
            FmaskViewInfo fmaskView = {};
            fmaskView.pImage         = &srcImage;
            fmaskView.baseArraySlice = pRegions[idx].srcSlice;
            fmaskView.arraySize      = pRegions[idx].numSlices;

            m_pDevice->Parent()->CreateFmaskViewSrds(1, &fmaskView, pUserData);
            pUserData += SrdDwordAlignment();
        }

        // Copy the user-data values into the descriptor table memory
        memcpy(pUserData, &regionData[0], sizeof(regionData));

        // Execute the dispatch. Resolves can only be done on 2D images so the Z dimension of the dispatch is always 1.
        pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroups(pRegions[idx].extent.width,  threadsPerGroup[0]),
                                RpmUtil::MinThreadGroups(pRegions[idx].extent.height, threadsPerGroup[1]),
                                RpmUtil::MinThreadGroups(pRegions[idx].numSlices,  threadsPerGroup[2]));
    }

    // Restore the command buffer's state.
    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);

    if (dstImage.GetGfxImage()->HasHtileData())
    {
        bool performedResummarizeHtileCompute = false;
        for (uint32 i = 0; i < regionCount; ++i)
        {
            const ImageResolveRegion& curRegion = pRegions[i];
            SubresRange subresRange = {};
            subresRange.startSubres.aspect = curRegion.dstAspect;
            subresRange.startSubres.mipLevel = curRegion.dstMipLevel;
            subresRange.startSubres.arraySlice = curRegion.dstSlice;
            subresRange.numMips = 1;
            subresRange.numSlices = curRegion.numSlices;
            HwlResummarizeHtileCompute(pCmdBuffer, *dstImage.GetGfxImage(), subresRange);

            performedResummarizeHtileCompute = true;
        }

        // Add the barrier if a ResummarizeHtileCompute was performed.
        if (performedResummarizeHtileCompute)
        {
            // There is a potential problem here because the htile is shared between
            // the depth and stencil aspects, but the APIs manage the state of those
            // aspects independently.  At this point in the code, we know the depth
            // aspect must be in a state that supports being a resolve destination,
            // but the stencil aspect may still be in a state that supports stencil
            // target rendering.  Since we are modifying HTILE asynchronously with
            // respect to the DB and through a different data path than the DB, we
            // need to ensure our CS won't overlap with subsequent stencil rendering
            // and that our HTILE updates are immediately visible to the DB.

            BarrierInfo hiZExpandBarrier = {};
            hiZExpandBarrier.waitPoint = HwPipePreCs;

            constexpr HwPipePoint PostCs = HwPipePostCs;
            hiZExpandBarrier.pipePointWaitCount = 1;
            hiZExpandBarrier.pPipePoints = &PostCs;

            BarrierTransition transition = {};
            transition.srcCacheMask = CoherShader;
            transition.dstCacheMask = CoherShader | CoherDepthStencilTarget;
            hiZExpandBarrier.pTransitions = &transition;

            pCmdBuffer->CmdBarrier(hiZExpandBarrier);
        }
    }

    FixupLateExpandResolveSrc(pCmdBuffer, srcImage, srcImageLayout, pRegions, regionCount, method);
}

// =====================================================================================================================
// Selects a compute Resolve pipeline based on the properties of the given Image and resolve method.
const ComputePipeline* RsrcProcMgr::GetCsResolvePipeline(
    const Image&  srcImage,
    ImageAspect   aspect,
    ResolveMode   mode,
    ResolveMethod method
    ) const
{
    const ComputePipeline* pPipeline = nullptr;
    const auto& createInfo = srcImage.GetImageCreateInfo();
    const bool  isStencil  = (aspect == ImageAspect::Stencil);

    // If the sample and fragment counts are different then this must be an EQAA resolve.
    if (createInfo.samples != createInfo.fragments)
    {
        PAL_ASSERT(method.shaderCsFmask == 1);

        switch (createInfo.fragments)
        {
        case 1:
            pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve1xEqaa);
            break;
        case 2:
            switch (mode)
            {
            case ResolveMode::Average:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve2xEqaa);
                break;
            case ResolveMode::Minimum:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve2xEqaaMin);
                break;
            case ResolveMode::Maximum:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve2xEqaaMax);
                break;
            default:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve2xEqaa);
                PAL_NEVER_CALLED();
                break;
            }
            break;
        case 4:
            switch (mode)
            {
            case ResolveMode::Average:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve4xEqaa);
                break;
            case ResolveMode::Minimum:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve4xEqaaMin);
                break;
            case ResolveMode::Maximum:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve4xEqaaMax);
                break;
            default:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve4xEqaa);
                PAL_NEVER_CALLED();
                break;
            }
            break;
        case 8:
            switch (mode)
            {
            case ResolveMode::Average:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve8xEqaa);
                break;
            case ResolveMode::Minimum:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve8xEqaaMin);
                break;
            case ResolveMode::Maximum:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve8xEqaaMax);
                break;
            default:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve8xEqaa);
                PAL_NEVER_CALLED();
                break;
            }
            break;
        default:
            PAL_NEVER_CALLED();
            break;
        }
    }
    else if ((method.shaderCs == 1) && (method.shaderCsFmask == 0))
    {
        // A regular MSAA color image resolve shader is used for DS resolve as well. By setting the
        // "copy sample zero" flag to 1, we force the shader to simply copy the first sample (sample 0).
        switch (createInfo.samples)
        {
        case 2:
            switch (mode)
            {
            case ResolveMode::Average:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaResolve2x);
                break;
            case ResolveMode::Minimum:
                pPipeline = isStencil ? GetPipeline(RpmComputePipeline::MsaaResolveStencil2xMin)
                                      : GetPipeline(RpmComputePipeline::MsaaResolve2xMin);
                break;
            case ResolveMode::Maximum:
                pPipeline = isStencil ? GetPipeline(RpmComputePipeline::MsaaResolveStencil2xMax)
                                      : GetPipeline(RpmComputePipeline::MsaaResolve2xMax);
                break;
            default:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaResolve2x);
                PAL_NEVER_CALLED();
                break;
            }
            break;
        case 4:
            switch (mode)
            {
            case ResolveMode::Average:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaResolve4x);
                break;
            case ResolveMode::Minimum:
                pPipeline = isStencil ? GetPipeline(RpmComputePipeline::MsaaResolveStencil4xMin)
                                      : GetPipeline(RpmComputePipeline::MsaaResolve4xMin);
                break;
            case ResolveMode::Maximum:
                pPipeline = isStencil ? GetPipeline(RpmComputePipeline::MsaaResolveStencil4xMax)
                                      : GetPipeline(RpmComputePipeline::MsaaResolve4xMax);
                break;
            default:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaResolve4x);
                PAL_NEVER_CALLED();
                break;
            }
            break;
        case 8:
            switch (mode)
            {
            case ResolveMode::Average:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaResolve8x);
                break;
            case ResolveMode::Minimum:
                pPipeline = isStencil ? GetPipeline(RpmComputePipeline::MsaaResolveStencil8xMin)
                                      : GetPipeline(RpmComputePipeline::MsaaResolve8xMin);
                break;
            case ResolveMode::Maximum:
                pPipeline = isStencil ? GetPipeline(RpmComputePipeline::MsaaResolveStencil8xMax)
                                      : GetPipeline(RpmComputePipeline::MsaaResolve8xMax);
                break;
            default:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaResolve8x);
                PAL_NEVER_CALLED();
                break;
            }
            break;
        default:
            PAL_NEVER_CALLED();
            break;
        }
    }
    else
    {
        switch (createInfo.samples)
        {
        case 2:
            switch (mode)
            {
            case ResolveMode::Average:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve2x);
                break;
            case ResolveMode::Minimum:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve2xMin);
                break;
            case ResolveMode::Maximum:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve2xMax);
                break;
            default:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve2x);
                PAL_NEVER_CALLED();
                break;
            }
            break;
        case 4:
            switch (mode)
            {
            case ResolveMode::Average:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve4x);
                break;
            case ResolveMode::Minimum:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve4xMin);
                break;
            case ResolveMode::Maximum:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve4xMax);
                break;
            default:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve4x);
                PAL_NEVER_CALLED();
                break;
            }
            break;
        case 8:
            switch (mode)
            {
            case ResolveMode::Average:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve8x);
                break;
            case ResolveMode::Minimum:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve8xMin);
                break;
            case ResolveMode::Maximum:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve8xMax);
                break;
            default:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve8x);
                PAL_NEVER_CALLED();
                break;
            }
            break;
        default:
            PAL_NEVER_CALLED();
            break;
        }
    }

    PAL_ASSERT(pPipeline != nullptr);
    return pPipeline;
}

// =====================================================================================================================
// Performs a depth/stencil expand (decompress) on the provided image.
bool RsrcProcMgr::ExpandDepthStencil(
    GfxCmdBuffer*        pCmdBuffer,
    const Image&         image,
    const IMsaaState*    pMsaaState,
    const MsaaQuadSamplePattern* pQuadSamplePattern,
    const SubresRange&   range
    ) const
{
    PAL_ASSERT(image.IsDepthStencil());
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());
    // Don't expect GFX Blts on Nested unless targets not inherited.
    PAL_ASSERT((pCmdBuffer->IsNested() == false) || (static_cast<UniversalCmdBuffer*>(
        pCmdBuffer)->GetGraphicsState().inheritedState.stateFlags.targetViewState == 0));

    const StencilRefMaskParams       stencilRefMasks      = { 0xFF, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0xFF, 0x01, 0xFF };

    ViewportParams viewportInfo = { };
    viewportInfo.count                 = 1;
    viewportInfo.viewports[0].originX  = 0;
    viewportInfo.viewports[0].originY  = 0;
    viewportInfo.viewports[0].minDepth = 0.f;
    viewportInfo.viewports[0].maxDepth = 1.f;
    viewportInfo.viewports[0].origin   = PointOrigin::UpperLeft;
    viewportInfo.horzClipRatio         = FLT_MAX;
    viewportInfo.horzDiscardRatio      = 1.0f;
    viewportInfo.vertClipRatio         = FLT_MAX;
    viewportInfo.vertDiscardRatio      = 1.0f;
    viewportInfo.depthRange            = DepthRange::ZeroToOne;

    ScissorRectParams scissorInfo      = { };
    scissorInfo.count                  = 1;
    scissorInfo.scissors[0].offset.x   = 0;
    scissorInfo.scissors[0].offset.y   = 0;

    DepthStencilViewInternalCreateInfo depthViewInfoInternal = { };
    depthViewInfoInternal.flags.isExpand = 1;

    DepthStencilViewCreateInfo depthViewInfo = { };
    depthViewInfo.pImage    = &image;
    depthViewInfo.arraySize = 1;

    // Because a subresource range can't span multiple aspects we should always mark one of them "read only".
    if (range.startSubres.aspect == ImageAspect::Depth)
    {
        depthViewInfo.flags.readOnlyStencil = 1;
    }
    else
    {
        depthViewInfo.flags.readOnlyDepth = 1;
    }

    BindTargetParams bindTargetsInfo = { };
    bindTargetsInfo.depthTarget.pDepthStencilView     = nullptr;
    bindTargetsInfo.depthTarget.depthLayout.usages    = LayoutDepthStencilTarget;
    bindTargetsInfo.depthTarget.depthLayout.engines   = LayoutUniversalEngine;
    bindTargetsInfo.depthTarget.stencilLayout.usages  = LayoutDepthStencilTarget;
    bindTargetsInfo.depthTarget.stencilLayout.engines = LayoutUniversalEngine;

    // Save current command buffer state and bind graphics state which is common for all subresources.
    pCmdBuffer->PushGraphicsState();
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics, GetGfxPipeline(DepthExpand), InternalApiPsoHash, });
    BindCommonGraphicsState(pCmdBuffer);
    pCmdBuffer->CmdBindDepthStencilState(m_pDepthExpandState);
    pCmdBuffer->CmdBindMsaaState(pMsaaState);

    if (pQuadSamplePattern != nullptr)
    {
        pCmdBuffer->CmdSetMsaaQuadSamplePattern(image.GetImageCreateInfo().samples, *pQuadSamplePattern);
    }

    pCmdBuffer->CmdSetStencilRefMasks(stencilRefMasks);

    RpmUtil::WriteVsZOut(pCmdBuffer, 1.0f);

    const uint32 lastMip   = (range.startSubres.mipLevel   + range.numMips   - 1);
    const uint32 lastSlice = (range.startSubres.arraySlice + range.numSlices - 1);

    for (depthViewInfo.mipLevel  = range.startSubres.mipLevel;
         depthViewInfo.mipLevel <= lastMip;
         ++depthViewInfo.mipLevel)
    {
        if (image.GetGfxImage()->CanMipSupportMetaData(depthViewInfo.mipLevel))
        {
            LinearAllocatorAuto<VirtualLinearAllocator> mipAlloc(pCmdBuffer->Allocator(), false);

            const SubresId mipSubres  = { range.startSubres.aspect, depthViewInfo.mipLevel, 0 };
            const auto&    subResInfo = *image.SubresourceInfo(mipSubres);

            // All slices of the same mipmap level can re-use the same viewport/scissor state.
            viewportInfo.viewports[0].width  = static_cast<float>(subResInfo.extentTexels.width);
            viewportInfo.viewports[0].height = static_cast<float>(subResInfo.extentTexels.height);

            scissorInfo.scissors[0].extent.width  = subResInfo.extentTexels.width;
            scissorInfo.scissors[0].extent.height = subResInfo.extentTexels.height;

            pCmdBuffer->CmdSetViewports(viewportInfo);
            pCmdBuffer->CmdSetScissorRects(scissorInfo);

            for (depthViewInfo.baseArraySlice  = range.startSubres.arraySlice;
                 depthViewInfo.baseArraySlice <= lastSlice;
                 ++depthViewInfo.baseArraySlice)
            {
                LinearAllocatorAuto<VirtualLinearAllocator> sliceAlloc(pCmdBuffer->Allocator(), false);

                // Create and bind a depth stencil view of the current subresource.
                IDepthStencilView* pDepthView = nullptr;
                void* pDepthViewMem =
                    PAL_MALLOC(m_pDevice->GetDepthStencilViewSize(nullptr), &sliceAlloc, AllocInternalTemp);

                if (pDepthViewMem == nullptr)
                {
                    pCmdBuffer->NotifyAllocFailure();
                }
                else
                {
                    Result result = m_pDevice->CreateDepthStencilView(depthViewInfo,
                                                                      depthViewInfoInternal,
                                                                      pDepthViewMem,
                                                                      &pDepthView);
                    PAL_ASSERT(result == Result::Success);

                    bindTargetsInfo.depthTarget.pDepthStencilView = pDepthView;
                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                    // Draw a fullscreen quad.
                    pCmdBuffer->CmdDraw(0, 3, 0, 1, 0);

                    PAL_SAFE_FREE(pDepthViewMem, &sliceAlloc);

                    // Unbind the depth view and destroy it.
                    bindTargetsInfo.depthTarget.pDepthStencilView = nullptr;
                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);
                }
            }
        }
    }

    // Restore command buffer state.
    pCmdBuffer->PopGraphicsState();

    // Compute path was not used
    return false;
}

// =====================================================================================================================
// Performs a depth/stencil resummarization on the provided image.  This operation recalculates the HiZ range in the
// htile based on the z-buffer values.
void RsrcProcMgr::ResummarizeDepthStencil(
    GfxCmdBuffer*        pCmdBuffer,
    const Image&         image,
    ImageLayout          imageLayout,
    const IMsaaState*    pMsaaState,
    const MsaaQuadSamplePattern* pQuadSamplePattern,
    const SubresRange&   range
    ) const
{
    PAL_ASSERT(image.IsDepthStencil());
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());
    // Don't expect GFX Blts on Nested unless targets not inherited.
    PAL_ASSERT((pCmdBuffer->IsNested() == false) || (static_cast<UniversalCmdBuffer*>(
        pCmdBuffer)->GetGraphicsState().inheritedState.stateFlags.targetViewState == 0));

    const StencilRefMaskParams       stencilRefMasks      = { 0xFF, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0xFF, 0x01, 0xFF };

    ViewportParams viewportInfo = { };
    viewportInfo.count                 = 1;
    viewportInfo.viewports[0].originX  = 0;
    viewportInfo.viewports[0].originY  = 0;
    viewportInfo.viewports[0].minDepth = 0.f;
    viewportInfo.viewports[0].maxDepth = 1.f;
    viewportInfo.viewports[0].origin   = PointOrigin::UpperLeft;
    viewportInfo.horzClipRatio         = FLT_MAX;
    viewportInfo.horzDiscardRatio      = 1.0f;
    viewportInfo.vertClipRatio         = FLT_MAX;
    viewportInfo.vertDiscardRatio      = 1.0f;
    viewportInfo.depthRange            = DepthRange::ZeroToOne;

    ScissorRectParams scissorInfo    = { };
    scissorInfo.count                = 1;
    scissorInfo.scissors[0].offset.x = 0;
    scissorInfo.scissors[0].offset.y = 0;

    const DepthStencilViewInternalCreateInfo depthViewInfoInternal = { };

    DepthStencilViewCreateInfo depthViewInfo = { };
    depthViewInfo.pImage    = &image;
    depthViewInfo.arraySize = 1;
    depthViewInfo.flags.resummarizeHiZ = 1;

    // Because a subresource range can't span multiple aspects we should always mark one of them "read only".
    if (range.startSubres.aspect == ImageAspect::Depth)
    {
        depthViewInfo.flags.readOnlyStencil = 1;
    }
    else
    {
        depthViewInfo.flags.readOnlyDepth = 1;
    }

    BindTargetParams bindTargetsInfo = {};
    bindTargetsInfo.depthTarget.pDepthStencilView = nullptr;
    bindTargetsInfo.depthTarget.depthLayout       = imageLayout;
    bindTargetsInfo.depthTarget.stencilLayout     = imageLayout;

    // Save current command buffer state and bind graphics state which is common for all subresources.
    pCmdBuffer->PushGraphicsState();
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics, GetGfxPipeline(DepthResummarize), InternalApiPsoHash, });
    BindCommonGraphicsState(pCmdBuffer);
    pCmdBuffer->CmdBindDepthStencilState(m_pDepthResummarizeState);
    pCmdBuffer->CmdBindMsaaState(pMsaaState);

    if (pQuadSamplePattern != nullptr)
    {
        pCmdBuffer->CmdSetMsaaQuadSamplePattern(image.GetImageCreateInfo().samples, *pQuadSamplePattern);
    }

    pCmdBuffer->CmdSetStencilRefMasks(stencilRefMasks);

    RpmUtil::WriteVsZOut(pCmdBuffer, 1.0f);

    const uint32 lastMip   = range.startSubres.mipLevel   + range.numMips   - 1;
    const uint32 lastSlice = range.startSubres.arraySlice + range.numSlices - 1;

    for (depthViewInfo.mipLevel  = range.startSubres.mipLevel;
         depthViewInfo.mipLevel <= lastMip;
         ++depthViewInfo.mipLevel)
    {
        if (image.GetGfxImage()->CanMipSupportMetaData(depthViewInfo.mipLevel))
        {
            LinearAllocatorAuto<VirtualLinearAllocator> mipAlloc(pCmdBuffer->Allocator(), false);

            const SubresId mipSubres  = { range.startSubres.aspect, depthViewInfo.mipLevel, 0 };
            const auto&    subResInfo = *image.SubresourceInfo(mipSubres);

            // All slices of the same mipmap level can re-use the same viewport/scissor state.
            viewportInfo.viewports[0].width  = static_cast<float>(subResInfo.extentTexels.width);
            viewportInfo.viewports[0].height = static_cast<float>(subResInfo.extentTexels.height);

            scissorInfo.scissors[0].extent.width  = subResInfo.extentTexels.width;
            scissorInfo.scissors[0].extent.height = subResInfo.extentTexels.height;

            pCmdBuffer->CmdSetViewports(viewportInfo);
            pCmdBuffer->CmdSetScissorRects(scissorInfo);

            for (depthViewInfo.baseArraySlice  = range.startSubres.arraySlice;
                 depthViewInfo.baseArraySlice <= lastSlice;
                 ++depthViewInfo.baseArraySlice)
            {
                LinearAllocatorAuto<VirtualLinearAllocator> sliceAlloc(pCmdBuffer->Allocator(), false);

                // Create and bind a depth stencil view of the current subresource.
                IDepthStencilView* pDepthView = nullptr;
                void* pDepthViewMem =
                    PAL_MALLOC(m_pDevice->GetDepthStencilViewSize(nullptr), &sliceAlloc, AllocInternalTemp);

                if (pDepthViewMem == nullptr)
                {
                    pCmdBuffer->NotifyAllocFailure();
                }
                else
                {
                    Result result = m_pDevice->CreateDepthStencilView(depthViewInfo,
                                                                      depthViewInfoInternal,
                                                                      pDepthViewMem,
                                                                      &pDepthView);
                    PAL_ASSERT(result == Result::Success);

                    bindTargetsInfo.depthTarget.pDepthStencilView = pDepthView;
                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                    // Draw a fullscreen quad.
                    pCmdBuffer->CmdDraw(0, 3, 0, 1, 0);

                    PAL_SAFE_FREE(pDepthViewMem, &sliceAlloc);

                    // Unbind the depth view and destroy it.
                    bindTargetsInfo.depthTarget.pDepthStencilView = nullptr;
                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);
                }
            }
        }
    }

    // Restore command buffer state.
    pCmdBuffer->PopGraphicsState();
}

// =====================================================================================================================
// Executes a generic color blit which acts upon the specified color Image. If mipCondDwordsAddr is non-zero, it is the
// GPU virtual address of an array of conditional DWORDs, one for each mip level in the image. RPM will use these
// DWORDs to conditionally execute this blit on a per-mip basis.
void RsrcProcMgr::GenericColorBlit(
    GfxCmdBuffer*        pCmdBuffer,
    const Image&         dstImage,
    const SubresRange&   range,
    const IMsaaState&    msaaState,
    const MsaaQuadSamplePattern* pQuadSamplePattern,
    RpmGfxPipeline       pipeline,
    const GpuMemory*     pGpuMemory,
    gpusize              metaDataOffset
    ) const
{
    PAL_ASSERT(dstImage.IsRenderTarget());
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());
    // Don't expect GFX Blts on Nested unless targets not inherited.
    PAL_ASSERT((pCmdBuffer->IsNested() == false) || (static_cast<UniversalCmdBuffer*>(
        pCmdBuffer)->GetGraphicsState().inheritedState.stateFlags.targetViewState == 0));

    const auto& imageCreateInfo = dstImage.GetImageCreateInfo();
    const bool  is3dImage       = (imageCreateInfo.imageType == ImageType::Tex3d);
    const bool  isDecompress    = ((pipeline == RpmGfxPipeline::DccDecompress) ||
                                   (pipeline == RpmGfxPipeline::FastClearElim) ||
                                   (pipeline == RpmGfxPipeline::FmaskDecompress));

    ViewportParams viewportInfo = { };
    viewportInfo.count                 = 1;
    viewportInfo.viewports[0].originX  = 0;
    viewportInfo.viewports[0].originY  = 0;
    viewportInfo.viewports[0].minDepth = 0.f;
    viewportInfo.viewports[0].maxDepth = 1.f;
    viewportInfo.viewports[0].origin   = PointOrigin::UpperLeft;
    viewportInfo.horzClipRatio         = FLT_MAX;
    viewportInfo.horzDiscardRatio      = 1.0f;
    viewportInfo.vertClipRatio         = FLT_MAX;
    viewportInfo.vertDiscardRatio      = 1.0f;
    viewportInfo.depthRange            = DepthRange::ZeroToOne;

    ScissorRectParams scissorInfo;
    scissorInfo.count                  = 1;
    scissorInfo.scissors[0].offset.x   = 0;
    scissorInfo.scissors[0].offset.y   = 0;

    ColorTargetViewInternalCreateInfo colorViewInfoInternal = { };
    colorViewInfoInternal.flags.dccDecompress   = (pipeline == RpmGfxPipeline::DccDecompress);
    colorViewInfoInternal.flags.fastClearElim   = (pipeline == RpmGfxPipeline::FastClearElim);
    colorViewInfoInternal.flags.fmaskDecompress = (pipeline == RpmGfxPipeline::FmaskDecompress);

    ColorTargetViewCreateInfo colorViewInfo = { };
    colorViewInfo.swizzledFormat      = imageCreateInfo.swizzledFormat;
    colorViewInfo.imageInfo.pImage    = &dstImage;
    colorViewInfo.imageInfo.arraySize = 1;
    colorViewInfo.imageInfo.baseSubRes.aspect = range.startSubres.aspect;

    if (is3dImage)
    {
        colorViewInfo.zRange.extent     = 1;
        colorViewInfo.flags.zRangeValid = 1;
    }

    BindTargetParams bindTargetsInfo = { };
    bindTargetsInfo.colorTargets[0].pColorTargetView    = nullptr;
    bindTargetsInfo.colorTargets[0].imageLayout.usages  = LayoutColorTarget;
    bindTargetsInfo.colorTargets[0].imageLayout.engines = LayoutUniversalEngine;
    bindTargetsInfo.depthTarget.pDepthStencilView       = nullptr;
    bindTargetsInfo.depthTarget.depthLayout.usages      = LayoutDepthStencilTarget;
    bindTargetsInfo.depthTarget.depthLayout.engines     = LayoutUniversalEngine;
    bindTargetsInfo.depthTarget.stencilLayout.usages    = LayoutDepthStencilTarget;
    bindTargetsInfo.depthTarget.stencilLayout.engines   = LayoutUniversalEngine;

    const StencilRefMaskParams       stencilRefMasks      = { 0xFF, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0xFF, 0x01, 0xFF };

    // Save current command buffer state and bind graphics state which is common for all mipmap levels.
    pCmdBuffer->PushGraphicsState();
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics, GetGfxPipeline(pipeline), InternalApiPsoHash, });

    BindCommonGraphicsState(pCmdBuffer);

    SwizzledFormat swizzledFormat = {};

    swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
    swizzledFormat.swizzle = { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

    pCmdBuffer->CmdOverwriteRbPlusFormatForBlits(swizzledFormat, 0);

    pCmdBuffer->CmdBindColorBlendState(m_pBlendDisableState);
    pCmdBuffer->CmdBindDepthStencilState(m_pDepthDisableState);
    pCmdBuffer->CmdBindMsaaState(&msaaState);

    if (pQuadSamplePattern != nullptr)
    {
        pCmdBuffer->CmdSetMsaaQuadSamplePattern(dstImage.GetImageCreateInfo().samples, *pQuadSamplePattern);
    }

    pCmdBuffer->CmdSetStencilRefMasks(stencilRefMasks);

    RpmUtil::WriteVsZOut(pCmdBuffer, 1.0f);

    const uint32 lastMip                = (range.startSubres.mipLevel + range.numMips - 1);
    gpusize      mipCondDwordsOffset    = metaDataOffset;
    bool         needDisablePredication = false;

    for (uint32 mip = range.startSubres.mipLevel; mip <= lastMip; ++mip)
    {
        // If this is a decompress operation of some sort, then don't bother continuing unless this
        // subresource supports expansion.
        if ((isDecompress == false) ||
            (dstImage.GetGfxImage()->CanMipSupportMetaData(mip)))
        {
            // Use predication to skip this operation based on the image's conditional dwords.
            // We can only perform this optimization if the client is not currently using predication.
            if ((pCmdBuffer->GetGfxCmdBufState().flags.clientPredicate == 0) && (pGpuMemory != nullptr))
            {
                // Set/Enable predication
                pCmdBuffer->CmdSetPredication(nullptr,
                                              0,
                                              pGpuMemory,
                                              mipCondDwordsOffset,
                                              PredicateType::Boolean64,
                                              true,
                                              false,
                                              false);
                mipCondDwordsOffset += PredicationAlign; // Advance to the next mip's conditional meta-data.

                needDisablePredication = true;
            }

            const SubresId mipSubres  = { range.startSubres.aspect, mip, 0 };
            const auto&    subResInfo = *dstImage.SubresourceInfo(mipSubres);

            // All slices of the same mipmap level can re-use the same viewport & scissor states.
            viewportInfo.viewports[0].width       = static_cast<float>(subResInfo.extentTexels.width);
            viewportInfo.viewports[0].height      = static_cast<float>(subResInfo.extentTexels.height);
            scissorInfo.scissors[0].extent.width  = subResInfo.extentTexels.width;
            scissorInfo.scissors[0].extent.height = subResInfo.extentTexels.height;

            pCmdBuffer->CmdSetViewports(viewportInfo);
            pCmdBuffer->CmdSetScissorRects(scissorInfo);

            // We need to draw each array slice individually because we cannot select which array slice to render to
            // without a Geometry Shader. If this is a 3D Image, we need to include all slices for this mipmap level.
            const uint32 baseSlice = (is3dImage ? 0                             : range.startSubres.arraySlice);
            const uint32 numSlices = (is3dImage ? subResInfo.extentTexels.depth : range.numSlices);
            const uint32 lastSlice = baseSlice + numSlices - 1;

            for (uint32 arraySlice = baseSlice; arraySlice <= lastSlice; ++arraySlice)
            {
                LinearAllocatorAuto<VirtualLinearAllocator> sliceAlloc(pCmdBuffer->Allocator(), false);

                // Create and bind a color-target view for this mipmap level and slice.
                IColorTargetView* pColorView = nullptr;
                void* pColorViewMem =
                    PAL_MALLOC(m_pDevice->GetColorTargetViewSize(nullptr), &sliceAlloc, AllocInternalTemp);

                if (pColorViewMem == nullptr)
                {
                    pCmdBuffer->NotifyAllocFailure();
                }
                else
                {
                    if (is3dImage)
                    {
                        colorViewInfo.zRange.offset = arraySlice;
                    }
                    else
                    {
                        colorViewInfo.imageInfo.baseSubRes.arraySlice = arraySlice;
                    }

                    colorViewInfo.imageInfo.baseSubRes.mipLevel   = mip;

                    Result result = m_pDevice->CreateColorTargetView(colorViewInfo,
                                                                     colorViewInfoInternal,
                                                                     pColorViewMem,
                                                                     &pColorView);
                    PAL_ASSERT(result == Result::Success);

                    bindTargetsInfo.colorTargets[0].pColorTargetView = pColorView;
                    bindTargetsInfo.colorTargetCount = 1;
                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                    // Draw a fullscreen quad.
                    pCmdBuffer->CmdDraw(0, 3, 0, 1, 0);

                    // Unbind the color-target view and destroy it.
                    bindTargetsInfo.colorTargetCount = 0;
                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                    PAL_SAFE_FREE(pColorViewMem, &sliceAlloc);
                }
            } // End for each array slice.
        }
    } // End for each mip level.

    if (needDisablePredication)
    {
        // Disable predication
        pCmdBuffer->CmdSetPredication(nullptr,
                                      0,
                                      nullptr,
                                      0,
                                      static_cast<PredicateType>(0),
                                      false,
                                      false,
                                      false);
    }

    // Restore original command buffer state.
    pCmdBuffer->PopGraphicsState();
}

// =====================================================================================================================
// Executes a CB fixed function resolve.
void RsrcProcMgr::ResolveImageFixedFunc(
    GfxCmdBuffer*             pCmdBuffer,
    const Image&              srcImage,
    ImageLayout               srcImageLayout,
    const Image&              dstImage,
    ImageLayout               dstImageLayout,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions,
    uint32                    flags
    ) const
{
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());
    // Don't expect GFX Blts on Nested unless targets not inherited.
    PAL_ASSERT((pCmdBuffer->IsNested() == false) || (static_cast<UniversalCmdBuffer*>(
        pCmdBuffer)->GetGraphicsState().inheritedState.stateFlags.targetViewState == 0));

    const auto& srcCreateInfo = srcImage.GetImageCreateInfo();
    const auto& dstCreateInfo = dstImage.GetImageCreateInfo();

    ViewportParams viewportInfo = { };
    viewportInfo.count                 = 1;
    viewportInfo.viewports[0].minDepth = 0.f;
    viewportInfo.viewports[0].maxDepth = 1.f;
    viewportInfo.viewports[0].origin   = PointOrigin::UpperLeft;
    viewportInfo.horzClipRatio         = FLT_MAX;
    viewportInfo.horzDiscardRatio      = 1.0f;
    viewportInfo.vertClipRatio         = FLT_MAX;
    viewportInfo.vertDiscardRatio      = 1.0f;
    viewportInfo.depthRange            = DepthRange::ZeroToOne;

    ScissorRectParams scissorInfo = { };
    scissorInfo.count = 1;

    const ColorTargetViewInternalCreateInfo colorViewInfoInternal = { };

    ColorTargetViewCreateInfo srcColorViewInfo = { };
    srcColorViewInfo.imageInfo.pImage            = &srcImage;
    srcColorViewInfo.imageInfo.baseSubRes.aspect = ImageAspect::Color;
    srcColorViewInfo.imageInfo.arraySize         = 1;

    ColorTargetViewCreateInfo dstColorViewInfo = { };
    dstColorViewInfo.imageInfo.pImage            = &dstImage;
    dstColorViewInfo.imageInfo.baseSubRes.aspect = ImageAspect::Color;
    dstColorViewInfo.imageInfo.arraySize         = 1;

    BindTargetParams bindTargetsInfo = {};
    bindTargetsInfo.colorTargetCount                    = 2;
    bindTargetsInfo.colorTargets[0].pColorTargetView    = nullptr;
    bindTargetsInfo.colorTargets[0].imageLayout.usages  = LayoutColorTarget;
    bindTargetsInfo.colorTargets[0].imageLayout.engines = LayoutUniversalEngine;
    bindTargetsInfo.colorTargets[1].pColorTargetView    = nullptr;
    bindTargetsInfo.colorTargets[1].imageLayout.usages  = LayoutColorTarget;
    bindTargetsInfo.colorTargets[1].imageLayout.engines = LayoutUniversalEngine;

    // Save current command buffer state and bind graphics state which is common for all regions.
    pCmdBuffer->PushGraphicsState();
    BindCommonGraphicsState(pCmdBuffer);
    pCmdBuffer->CmdBindMsaaState(GetMsaaState(srcCreateInfo.samples, srcCreateInfo.fragments));
    pCmdBuffer->CmdBindColorBlendState(m_pBlendDisableState);
    pCmdBuffer->CmdBindDepthStencilState(m_pDepthDisableState);

    const GraphicsPipeline* pPipelinePrevious      = nullptr;
    const GraphicsPipeline* pPipelineByImageFormat =
        GetGfxPipelineByTargetIndexAndFormat(ResolveFixedFunc_32ABGR, 0, srcCreateInfo.swizzledFormat);

    // Put ImageResolveInvertY value in user data 0 used by VS.
    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 0, 1, &flags);

    // Each region needs to be resolved individually.
    for (uint32 idx = 0; idx < regionCount; ++idx)
    {
        LinearAllocatorAuto<VirtualLinearAllocator> regionAlloc(pCmdBuffer->Allocator(), false);

        srcColorViewInfo.swizzledFormat                = srcCreateInfo.swizzledFormat;
        dstColorViewInfo.swizzledFormat                = dstCreateInfo.swizzledFormat;
        dstColorViewInfo.imageInfo.baseSubRes.mipLevel = pRegions[idx].dstMipLevel;

        // Override the formats with the caller's "reinterpret" format:
        if (Formats::IsUndefined(pRegions[idx].swizzledFormat.format) == false)
        {
            // We require that the channel formats match.
            PAL_ASSERT(Formats::ShareChFmt(srcColorViewInfo.swizzledFormat.format,
                                           pRegions[idx].swizzledFormat.format));
            PAL_ASSERT(Formats::ShareChFmt(dstColorViewInfo.swizzledFormat.format,
                                           pRegions[idx].swizzledFormat.format));

            const SubresId srcSubres = { pRegions[idx].srcAspect, 0, pRegions[idx].srcSlice };
            const SubresId dstSubres = { pRegions[idx].dstAspect, pRegions[idx].dstMipLevel, pRegions[idx].dstSlice };

            // If the specified format exactly matches the image formats the resolve will always work. Otherwise, the
            // images must support format replacement.
            PAL_ASSERT(Formats::HaveSameNumFmt(srcColorViewInfo.swizzledFormat.format,
                                               pRegions[idx].swizzledFormat.format) ||
                       srcImage.GetGfxImage()->IsFormatReplaceable(srcSubres, srcImageLayout, false));

            PAL_ASSERT(Formats::HaveSameNumFmt(dstColorViewInfo.swizzledFormat.format,
                                               pRegions[idx].swizzledFormat.format) ||
                       dstImage.GetGfxImage()->IsFormatReplaceable(dstSubres, dstImageLayout, true));

            srcColorViewInfo.swizzledFormat.format = pRegions[idx].swizzledFormat.format;
            dstColorViewInfo.swizzledFormat.format = pRegions[idx].swizzledFormat.format;
        }

        // Setup the viewport and scissor to restrict rendering to the destination region being copied.
        viewportInfo.viewports[0].originX = static_cast<float>(pRegions[idx].dstOffset.x);
        viewportInfo.viewports[0].originY = static_cast<float>(pRegions[idx].dstOffset.y);
        viewportInfo.viewports[0].width   = static_cast<float>(pRegions[idx].extent.width);
        viewportInfo.viewports[0].height  = static_cast<float>(pRegions[idx].extent.height);

        scissorInfo.scissors[0].offset.x      = pRegions[idx].dstOffset.x;
        scissorInfo.scissors[0].offset.y      = pRegions[idx].dstOffset.y;
        scissorInfo.scissors[0].extent.width  = pRegions[idx].extent.width;
        scissorInfo.scissors[0].extent.height = pRegions[idx].extent.height;

        const GraphicsPipeline* pPipeline =
            Formats::IsUndefined(pRegions[idx].swizzledFormat.format)
            ? pPipelineByImageFormat
            : GetGfxPipelineByTargetIndexAndFormat(ResolveFixedFunc_32ABGR, 0, pRegions[idx].swizzledFormat);

        if (pPipelinePrevious != pPipeline)
        {
            pPipelinePrevious = pPipeline;
            pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics, pPipeline, InternalApiPsoHash, });
        }

        pCmdBuffer->CmdSetViewports(viewportInfo);
        pCmdBuffer->CmdSetScissorRects(scissorInfo);

        for (uint32 slice = 0; slice < pRegions[idx].numSlices; ++slice)
        {
            srcColorViewInfo.imageInfo.baseSubRes.arraySlice = (pRegions[idx].srcSlice + slice);
            dstColorViewInfo.imageInfo.baseSubRes.arraySlice = (pRegions[idx].dstSlice + slice);

            LinearAllocatorAuto<VirtualLinearAllocator> sliceAlloc(pCmdBuffer->Allocator(), false);

            IColorTargetView* pSrcColorView = nullptr;
            IColorTargetView* pDstColorView = nullptr;

            void* pSrcColorViewMem =
                PAL_MALLOC(m_pDevice->GetColorTargetViewSize(nullptr), &sliceAlloc, AllocInternalTemp);
            void* pDstColorViewMem =
                PAL_MALLOC(m_pDevice->GetColorTargetViewSize(nullptr), &sliceAlloc, AllocInternalTemp);

            if ((pDstColorViewMem == nullptr) || (pSrcColorViewMem == nullptr))
            {
                pCmdBuffer->NotifyAllocFailure();
            }
            else
            {
                Result result = m_pDevice->CreateColorTargetView(srcColorViewInfo,
                                                                 colorViewInfoInternal,
                                                                 pSrcColorViewMem,
                                                                 &pSrcColorView);
                PAL_ASSERT(result == Result::Success);
                if (result == Result::Success)
                {
                    result = m_pDevice->CreateColorTargetView(dstColorViewInfo,
                                                              colorViewInfoInternal,
                                                              pDstColorViewMem,
                                                              &pDstColorView);
                    PAL_ASSERT(result == Result::Success);
                }

                if (result == Result::Success)
                {
                    bindTargetsInfo.colorTargets[0].pColorTargetView = pSrcColorView;
                    bindTargetsInfo.colorTargets[1].pColorTargetView = pDstColorView;
                    bindTargetsInfo.colorTargetCount = 2;
                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                    // Draw a fullscreen quad.
                    pCmdBuffer->CmdDraw(0, 3, 0, 1, 0);

                    // Unbind the color-target view and destroy it.
                    bindTargetsInfo.colorTargetCount = 0;
                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);
                }

            }

            PAL_SAFE_FREE(pSrcColorViewMem, &sliceAlloc);
            PAL_SAFE_FREE(pDstColorViewMem, &sliceAlloc);
        } // End for each slice.
    } // End for each region.

    // Restore original command buffer state.
    pCmdBuffer->PopGraphicsState();
}

// =====================================================================================================================
// Executes a image resolve by performing fixed-func depth copy or stencil copy
void RsrcProcMgr::ResolveImageDepthStencilCopy(
    GfxCmdBuffer*             pCmdBuffer,
    const Image&              srcImage,
    ImageLayout               srcImageLayout,
    const Image&              dstImage,
    ImageLayout               dstImageLayout,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions,
    uint32                    flags) const
{
    PAL_ASSERT(srcImage.IsDepthStencil() && dstImage.IsDepthStencil());
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());
    // Don't expect GFX Blts on Nested unless targets not inherited.
    PAL_ASSERT((pCmdBuffer->IsNested() == false) || (static_cast<UniversalCmdBuffer*>(
        pCmdBuffer)->GetGraphicsState().inheritedState.stateFlags.targetViewState == 0));

    const auto& srcCreateInfo = srcImage.GetImageCreateInfo();
    const auto& dstCreateInfo = dstImage.GetImageCreateInfo();

    ViewportParams viewportInfo = {};
    viewportInfo.count = 1;
    viewportInfo.viewports[0].minDepth = 0.f;
    viewportInfo.viewports[0].maxDepth = 1.f;
    viewportInfo.viewports[0].origin = PointOrigin::UpperLeft;

    viewportInfo.horzClipRatio      = FLT_MAX;
    viewportInfo.horzDiscardRatio   = 1.0f;
    viewportInfo.vertClipRatio      = FLT_MAX;
    viewportInfo.vertDiscardRatio   = 1.0f;
    viewportInfo.depthRange         = DepthRange::ZeroToOne;

    ScissorRectParams scissorInfo = {};
    scissorInfo.count = 1;

    DepthStencilViewCreateInfo srcDepthViewInfo = {};
    srcDepthViewInfo.pImage = &srcImage;
    srcDepthViewInfo.arraySize = 1;
    srcDepthViewInfo.flags.readOnlyDepth = 1;
    srcDepthViewInfo.flags.readOnlyStencil = 1;

    ColorTargetViewCreateInfo dstColorViewInfo = {};
    dstColorViewInfo.imageInfo.pImage = &dstImage;
    dstColorViewInfo.imageInfo.arraySize = 1;

    BindTargetParams bindTargetsInfo = {};
    bindTargetsInfo.colorTargetCount = 1;
    bindTargetsInfo.colorTargets[0].pColorTargetView = nullptr;
    bindTargetsInfo.colorTargets[0].imageLayout.usages = LayoutColorTarget;
    bindTargetsInfo.colorTargets[0].imageLayout.engines = LayoutUniversalEngine;

    bindTargetsInfo.depthTarget.depthLayout.usages = LayoutDepthStencilTarget;
    bindTargetsInfo.depthTarget.depthLayout.engines = LayoutUniversalEngine;

    // Save current command buffer state and bind graphics state which is common for all regions.
    pCmdBuffer->PushGraphicsState();
    BindCommonGraphicsState(pCmdBuffer);
    pCmdBuffer->CmdBindMsaaState(GetMsaaState(1u, 1u));
    pCmdBuffer->CmdBindColorBlendState(m_pBlendDisableState);
    pCmdBuffer->CmdBindDepthStencilState(m_pDepthDisableState);

    // Put ImageResolveInvertY value in user data 0 used by VS.
    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 0, 1, &flags);

    // Each region needs to be resolved individually.
    for (uint32 idx = 0; idx < regionCount; ++idx)
    {
        LinearAllocatorAuto<VirtualLinearAllocator> regionAlloc(pCmdBuffer->Allocator(), false);

        dstColorViewInfo.imageInfo.baseSubRes.mipLevel = pRegions[idx].dstMipLevel;

        // Setup the viewport and scissor to restrict rendering to the destination region being copied.
        // srcOffset and dstOffset have to be exactly same
        PAL_ASSERT((pRegions[idx].srcOffset.x == pRegions[idx].dstOffset.x) &&
                   (pRegions[idx].srcOffset.y == pRegions[idx].dstOffset.y));
        viewportInfo.viewports[0].originX = static_cast<float>(pRegions[idx].srcOffset.x);
        viewportInfo.viewports[0].originY = static_cast<float>(pRegions[idx].srcOffset.y);
        viewportInfo.viewports[0].width = static_cast<float>(pRegions[idx].extent.width);
        viewportInfo.viewports[0].height = static_cast<float>(pRegions[idx].extent.height);

        scissorInfo.scissors[0].offset.x = pRegions[idx].srcOffset.x;
        scissorInfo.scissors[0].offset.y = pRegions[idx].srcOffset.y;
        scissorInfo.scissors[0].extent.width = pRegions[idx].extent.width;
        scissorInfo.scissors[0].extent.height = pRegions[idx].extent.height;

        pCmdBuffer->CmdSetViewports(viewportInfo);
        pCmdBuffer->CmdSetScissorRects(scissorInfo);

        if (srcCreateInfo.flags.sampleLocsAlwaysKnown != 0)
        {
            PAL_ASSERT(pRegions[idx].pQuadSamplePattern != nullptr);
            pCmdBuffer->CmdSetMsaaQuadSamplePattern(srcCreateInfo.samples, *pRegions[idx].pQuadSamplePattern);
        }
        else
        {
            PAL_ASSERT(pRegions[idx].pQuadSamplePattern == nullptr);
        }

        for (uint32 slice = 0; slice < pRegions[idx].numSlices; ++slice)
        {
            DepthStencilViewInternalCreateInfo depthViewInfoInternal = {};
            ColorTargetViewInternalCreateInfo  colorViewInfoInternal = {};
            colorViewInfoInternal.flags.depthStencilCopy = 1;

            srcDepthViewInfo.baseArraySlice = (pRegions[idx].srcSlice + slice);
            dstColorViewInfo.imageInfo.baseSubRes.arraySlice = (pRegions[idx].dstSlice + slice);

            LinearAllocatorAuto<VirtualLinearAllocator> sliceAlloc(pCmdBuffer->Allocator(), false);

            IDepthStencilView* pSrcDepthView = nullptr;
            IColorTargetView* pDstColorView = nullptr;

            void* pSrcDepthViewMem =
                PAL_MALLOC(m_pDevice->GetDepthStencilViewSize(nullptr), &sliceAlloc, AllocInternalTemp);
            void* pDstColorViewMem =
                PAL_MALLOC(m_pDevice->GetColorTargetViewSize(nullptr), &sliceAlloc, AllocInternalTemp);

            if ((pDstColorViewMem == nullptr) || (pSrcDepthViewMem == nullptr))
            {
                pCmdBuffer->NotifyAllocFailure();
            }
            else
            {
                PAL_ASSERT(pRegions[idx].srcAspect == pRegions[idx].dstAspect);
                dstColorViewInfo.imageInfo.baseSubRes.aspect = pRegions[idx].srcAspect;

                SubresId dstSubresId = {};
                dstSubresId.mipLevel = pRegions[idx].dstMipLevel;
                dstSubresId.arraySlice = (pRegions[idx].dstSlice + slice);
                dstSubresId.aspect = pRegions[idx].dstAspect;

                dstColorViewInfo.swizzledFormat.format = dstImage.SubresourceInfo(dstSubresId)->format.format;

                if (pRegions[idx].dstAspect == ImageAspect::Depth)
                {
                    depthViewInfoInternal.flags.isDepthCopy = 1;

                    dstColorViewInfo.swizzledFormat.swizzle =
                        {ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One};
                    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics, GetGfxPipeline(ResolveDepthCopy),
                                                  InternalApiPsoHash, });
                }
                else if (pRegions[idx].dstAspect == ImageAspect::Stencil)
                {
                    // Fixed-func stencil copies stencil value from db to g chanenl of cb.
                    // Swizzle the stencil plance to 0X00.
                    depthViewInfoInternal.flags.isStencilCopy = 1;

                    dstColorViewInfo.swizzledFormat.swizzle =
                        { ChannelSwizzle::Zero, ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::One };
                    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics, GetGfxPipeline(ResolveStencilCopy),
                                                  InternalApiPsoHash, });
                }
                else
                {
                    PAL_ASSERT_ALWAYS();
                }

                Result result = m_pDevice->CreateDepthStencilView(srcDepthViewInfo,
                    depthViewInfoInternal,
                    pSrcDepthViewMem,
                    &pSrcDepthView);
                PAL_ASSERT(result == Result::Success);

                if (result == Result::Success)
                {
                    result = m_pDevice->CreateColorTargetView(dstColorViewInfo,
                        colorViewInfoInternal,
                        pDstColorViewMem,
                        &pDstColorView);
                    PAL_ASSERT(result == Result::Success);
                }

                if (result == Result::Success)
                {
                    bindTargetsInfo.colorTargetCount = 1;
                    bindTargetsInfo.colorTargets[0].pColorTargetView = pDstColorView;
                    bindTargetsInfo.depthTarget.pDepthStencilView = pSrcDepthView;

                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                    // Draw a fullscreen quad.
                    pCmdBuffer->CmdDraw(0, 3, 0, 1, 0);

                    // Unbind the color-target and depth-stencil target view and destroy them.
                    bindTargetsInfo.colorTargetCount = 0;
                    bindTargetsInfo.depthTarget.pDepthStencilView = nullptr;
                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);
                }
            }

            PAL_SAFE_FREE(pSrcDepthViewMem, &sliceAlloc);
            PAL_SAFE_FREE(pDstColorViewMem, &sliceAlloc);
        } // End for each slice.
    } // End for each region.

      // Restore original command buffer state.
    pCmdBuffer->PopGraphicsState();
}

// =====================================================================================================================
// Selects the appropriate Depth Stencil copy pipeline based on usage and samples
const GraphicsPipeline* RsrcProcMgr::GetCopyDepthStencilPipeline(
    bool   isDepth,
    bool   isDepthStencil,
    uint32 numSamples
    ) const
{
    RpmGfxPipeline pipelineType;

    if (isDepthStencil)
    {
        pipelineType = (numSamples > 1) ? CopyMsaaDepthStencil : CopyDepthStencil;
    }
    else
    {
        if (isDepth)
        {
            pipelineType = (numSamples > 1) ? CopyMsaaDepth : CopyDepth;
        }
        else
        {
            pipelineType = (numSamples > 1) ? CopyMsaaStencil : CopyStencil;
        }
    }

    return GetGfxPipeline(pipelineType);
}

// =====================================================================================================================
// Retrieves a pre-created MSAA state object that represents the requested number of samples.
const MsaaState* RsrcProcMgr::GetMsaaState(
    uint32 samples,
    uint32 fragments
    ) const
{
    const uint32 log2SampleRate = Log2(samples);
    const uint32 log2FragmentRate = Log2(fragments);
    PAL_ASSERT(log2SampleRate <= MaxLog2AaSamples);
    PAL_ASSERT(log2FragmentRate <= MaxLog2AaFragments);

    return m_pMsaaState[log2SampleRate][log2FragmentRate];
}

// =====================================================================================================================
// Create a number of common state objects used by the various RPM-owned GFX pipelines
Result RsrcProcMgr::CreateCommonStateObjects()
{
    // Setup a "default" depth/stencil state with depth testing: Depth writes and stencil writes all disabled.
    DepthStencilStateCreateInfo depthStencilInfo = { };
    depthStencilInfo.depthFunc                = CompareFunc::Always;
    depthStencilInfo.front.stencilFailOp      = StencilOp::Keep;
    depthStencilInfo.front.stencilPassOp      = StencilOp::Keep;
    depthStencilInfo.front.stencilDepthFailOp = StencilOp::Keep;
    depthStencilInfo.front.stencilFunc        = CompareFunc::Always;
    depthStencilInfo.back                     = depthStencilInfo.front;
    depthStencilInfo.depthEnable              = false;
    depthStencilInfo.depthWriteEnable         = false;
    depthStencilInfo.stencilEnable            = false;

    Result result = m_pDevice->CreateDepthStencilStateInternal(depthStencilInfo, &m_pDepthDisableState, AllocInternal);

    if (result == Result::Success)
    {
        // Setup depth/stencil state with depth testing disabled, depth writes enabled and stencil writes enabled.
        // This is used for depth and stencil expands.
        depthStencilInfo.depthFunc                = CompareFunc::Always;
        depthStencilInfo.front.stencilFailOp      = StencilOp::Keep;
        depthStencilInfo.front.stencilPassOp      = StencilOp::Keep;
        depthStencilInfo.front.stencilDepthFailOp = StencilOp::Keep;
        depthStencilInfo.front.stencilFunc        = CompareFunc::Always;
        depthStencilInfo.back                     = depthStencilInfo.front;
        depthStencilInfo.depthEnable              = false;
        depthStencilInfo.depthWriteEnable         = true;
        depthStencilInfo.stencilEnable            = true;

        result = m_pDevice->CreateDepthStencilStateInternal(depthStencilInfo, &m_pDepthExpandState, AllocInternal);
    }

    if (result == Result::Success)
    {
        // Setup depth/stencil state with depth testing disabled and depth/stencil writes disabled
        // This is used for depth and stencil resummarization.
        depthStencilInfo.depthFunc                = CompareFunc::Always;
        depthStencilInfo.front.stencilFailOp      = StencilOp::Keep;
        depthStencilInfo.front.stencilPassOp      = StencilOp::Keep;
        depthStencilInfo.front.stencilDepthFailOp = StencilOp::Keep;
        depthStencilInfo.front.stencilFunc        = CompareFunc::Always;
        depthStencilInfo.back                     = depthStencilInfo.front;
        depthStencilInfo.depthEnable              = false;
        depthStencilInfo.depthWriteEnable         = false;
        depthStencilInfo.stencilEnable            = false;

        result = m_pDevice->CreateDepthStencilStateInternal(depthStencilInfo, &m_pDepthResummarizeState, AllocInternal);
    }

    // Setup the depth/stencil state for depth and stencil resolves using the graphics engine.
    if (result == Result::Success)
    {
        depthStencilInfo.depthEnable       = true;
        depthStencilInfo.depthFunc         = CompareFunc::Always;
        depthStencilInfo.front.stencilFunc = CompareFunc::Always;

        // State object for depth resolves:
        depthStencilInfo.front.stencilFailOp      = StencilOp::Keep;
        depthStencilInfo.front.stencilPassOp      = StencilOp::Keep;
        depthStencilInfo.front.stencilDepthFailOp = StencilOp::Keep;
        depthStencilInfo.back                     = depthStencilInfo.front;
        depthStencilInfo.depthWriteEnable         = true;
        depthStencilInfo.stencilEnable            = false;

        result = m_pDevice->CreateDepthStencilStateInternal(depthStencilInfo, &m_pDepthResolveState, AllocInternal);

        if (result == Result::Success)
        {
            // State object for stencil resolves:
            depthStencilInfo.front.stencilFailOp      = StencilOp::Replace;
            depthStencilInfo.front.stencilPassOp      = StencilOp::Replace;
            depthStencilInfo.front.stencilDepthFailOp = StencilOp::Replace;
            depthStencilInfo.back                     = depthStencilInfo.front;
            depthStencilInfo.depthWriteEnable         = true;
            depthStencilInfo.stencilEnable            = true;

            result = m_pDevice->CreateDepthStencilStateInternal(depthStencilInfo,
                                                                &m_pDepthStencilResolveState,
                                                                AllocInternal);
        }

        if (result == Result::Success)
        {
            // State object for stencil resolves:
            depthStencilInfo.front.stencilFailOp      = StencilOp::Replace;
            depthStencilInfo.front.stencilPassOp      = StencilOp::Replace;
            depthStencilInfo.front.stencilDepthFailOp = StencilOp::Replace;
            depthStencilInfo.back                     = depthStencilInfo.front;
            depthStencilInfo.depthWriteEnable         = false;
            depthStencilInfo.stencilEnable            = true;

            result = m_pDevice->CreateDepthStencilStateInternal(depthStencilInfo,
                                                                &m_pStencilResolveState,
                                                                AllocInternal);
        }
    }

    // Setup the depth/stencil states for clearing depth and/or stencil.
    if (result == Result::Success)
    {
        depthStencilInfo.depthFunc                = CompareFunc::Always;
        depthStencilInfo.front.stencilFunc        = CompareFunc::Always;
        depthStencilInfo.front.stencilFailOp      = StencilOp::Replace;
        depthStencilInfo.front.stencilPassOp      = StencilOp::Replace;
        depthStencilInfo.front.stencilDepthFailOp = StencilOp::Replace;
        depthStencilInfo.back                     = depthStencilInfo.front;
        depthStencilInfo.depthBoundsEnable        = false;
        depthStencilInfo.depthWriteEnable         = true;
        depthStencilInfo.depthEnable              = true;
        depthStencilInfo.stencilEnable            = true;

        result =
            m_pDevice->CreateDepthStencilStateInternal(depthStencilInfo, &m_pDepthStencilClearState, AllocInternal);

        if (result == Result::Success)
        {
            depthStencilInfo.depthEnable   = true;
            depthStencilInfo.stencilEnable = false;

            result = m_pDevice->CreateDepthStencilStateInternal(depthStencilInfo, &m_pDepthClearState, AllocInternal);
        }

        if (result == Result::Success)
        {
            depthStencilInfo.depthEnable   = false;
            depthStencilInfo.stencilEnable = true;

            result = m_pDevice->CreateDepthStencilStateInternal(depthStencilInfo, &m_pStencilClearState, AllocInternal);
        }
    }

    if (result == Result::Success)
    {
        // Set up a "default" color blend state which disables all blending.
        ColorBlendStateCreateInfo blendInfo = { };
        for (uint32 idx = 0; idx < MaxColorTargets; ++idx)
        {
            blendInfo.targets[idx].srcBlendColor  = Blend::One;
            blendInfo.targets[idx].srcBlendAlpha  = Blend::One;
            blendInfo.targets[idx].dstBlendColor  = Blend::Zero;
            blendInfo.targets[idx].dstBlendAlpha  = Blend::Zero;
            blendInfo.targets[idx].blendFuncColor = BlendFunc::Add;
            blendInfo.targets[idx].blendFuncAlpha = BlendFunc::Add;
        }

        result = m_pDevice->CreateColorBlendStateInternal(blendInfo, &m_pBlendDisableState, AllocInternal);
    }

    if (result == Result::Success)
    {
        // Set up a color blend state which enable rt0 blending.
        ColorBlendStateCreateInfo blendInfo = { };
        blendInfo.targets[0].blendEnable    = 1;
        blendInfo.targets[0].srcBlendColor  = Blend::SrcColor;
        blendInfo.targets[0].srcBlendAlpha  = Blend::SrcAlpha;
        blendInfo.targets[0].dstBlendColor  = Blend::DstColor;
        blendInfo.targets[0].dstBlendAlpha  = Blend::OneMinusSrcAlpha;
        blendInfo.targets[0].blendFuncColor = BlendFunc::Add;
        blendInfo.targets[0].blendFuncAlpha = BlendFunc::Add;

        result = m_pDevice->CreateColorBlendStateInternal(blendInfo, &m_pColorBlendState, AllocInternal);
    }

    // Create all MSAA state objects.
    MsaaStateCreateInfo msaaInfo = { };
    msaaInfo.sampleMask          = USHRT_MAX;

    for (uint32 log2Samples = 0; ((log2Samples <= MaxLog2AaSamples) && (result == Result::Success)); ++log2Samples)
    {
        const uint32 coverageSamples = (1 << log2Samples);
        msaaInfo.coverageSamples         = coverageSamples;
        msaaInfo.alphaToCoverageSamples  = coverageSamples;

        for (uint32 log2Fragments = 0;
            ((log2Fragments <= MaxLog2AaFragments) && (result == Result::Success));
            ++log2Fragments)
        {
            const uint32 fragments = (1 << log2Fragments);

            // The following parameters should never be higher than the max number of msaa fragments (usually 8).
            const uint32 maxFragments        = m_pDevice->Parent()->ChipProperties().imageProperties.maxMsaaFragments;
            const uint32 clampedSamples      = Min(fragments, maxFragments);
            msaaInfo.exposedSamples          = clampedSamples;
            msaaInfo.pixelShaderSamples      = clampedSamples;
            msaaInfo.depthStencilSamples     = clampedSamples;
            msaaInfo.shaderExportMaskSamples = clampedSamples;
            msaaInfo.sampleClusters          = clampedSamples;

            result = m_pDevice->CreateMsaaStateInternal(
                msaaInfo, &m_pMsaaState[log2Samples][log2Fragments], AllocInternal);
        }
    }

    return result;
}

// =====================================================================================================================
// Returns true if the supplied aspect / mip-level supports meta-data texture fetches.  If the supplied aspect is
// invalid for the image, then fetches are not supported.
bool RsrcProcMgr::GetMetaDataTexFetchSupport(
    const Image*  pImage,
    ImageAspect   aspect,
    uint32        mipLevel)
{
    const SubresId subres = { aspect, mipLevel, 0 };

    // The SubresourceInfo() function will assert and possibly do bad things (at least return a pointer to a different
    // subresource than intended) if the aspect isn't valid, so check that first.
    return (pImage->IsAspectValid(aspect) && pImage->SubresourceInfo(subres)->flags.supportMetaDataTexFetch);
}

// =====================================================================================================================
// Returns the size of a typed buffer that contains a 3D block of elements with the given size and pitches.
// This is useful for mapping a sub-cube of a linear image into a linear buffer.
gpusize RsrcProcMgr::ComputeTypedBufferRange(
    const Extent3d& extent,
    uint32          elementSize, // The size of each element in bytes.
    gpusize         rowPitch,    // The number of bytes between successive rows.
    gpusize         depthPitch)  // The number of bytes between successive depth slices.
{
    // This function will underflow if the extents aren't fully defined.
    PAL_ASSERT((extent.width > 0) && (extent.height > 0) && (extent.depth > 0));

    // Traversing the buffer from the "top left" to "bottom right" covers (depth - 1) full depth slices, (height - 1)
    // full rows, and (width) elements in the final partial row.
    return (((extent.depth - 1) * depthPitch) + ((extent.height - 1) * rowPitch) + (extent.width * elementSize));
}

// =====================================================================================================================
// Inserts barrier needed before issuing a compute clear when the target image is currently bound as a color target.
// Only necessary when the client specifies the ColorClearAutoSync flag for a color clear.
static void PreComputeColorClearSync(
    ICmdBuffer* pCmdBuffer)
{
    BarrierInfo preBarrier        = { };
    preBarrier.waitPoint          = HwPipePreCs;

    constexpr HwPipePoint Eop     = HwPipeBottom;
    preBarrier.pipePointWaitCount = 1;
    preBarrier.pPipePoints        = &Eop;

    BarrierTransition transition  = { };
    transition.srcCacheMask       = CoherColorTarget;
    transition.dstCacheMask       = CoherShader;
    preBarrier.transitionCount    = 1;
    preBarrier.pTransitions       = &transition;
    preBarrier.reason             = Developer::BarrierReasonPreComputeColorClear;

    pCmdBuffer->CmdBarrier(preBarrier);
}

// =====================================================================================================================
// Inserts barrier needed after issuing a compute clear when the target image will be immediately re-bound as a
// color target.  Only necessary when the client specifies the ColorClearAutoSync flag for a color clear.
static void PostComputeColorClearSync(
    ICmdBuffer* pCmdBuffer)
{
    BarrierInfo postBarrier        = { };
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 577
    postBarrier.waitPoint          = HwPipePreRasterization;
#else
    postBarrier.waitPoint          = HwPipePreColorTarget;
#endif

    constexpr HwPipePoint PostCs   = HwPipePostCs;
    postBarrier.pipePointWaitCount = 1;
    postBarrier.pPipePoints        = &PostCs;

    BarrierTransition transition   = { };
    transition.srcCacheMask        = CoherShader;
    transition.dstCacheMask        = CoherColorTarget;
    postBarrier.transitionCount    = 1;
    postBarrier.pTransitions       = &transition;
    postBarrier.reason             = Developer::BarrierReasonPostComputeColorClear;

    pCmdBuffer->CmdBarrier(postBarrier);
}

// =====================================================================================================================
// Inserts barrier needed before issuing a compute clear when the target image is currently bound as a depth/stencil
// target.  Only necessary when the client specifies the DsClearAutoSync flag for a depth/stencil clear.
void PreComputeDepthStencilClearSync(
    ICmdBuffer*        pCmdBuffer,
    const GfxImage&    gfxImage,
    const SubresRange& subres,
    ImageLayout        layout)
{
    BarrierInfo preBarrier                 = { };
    preBarrier.waitPoint                   = HwPipePreCs;

    const IImage* pImage                   = gfxImage.Parent();

    preBarrier.rangeCheckedTargetWaitCount = 1;
    preBarrier.ppTargets                   = &pImage;

    BarrierTransition transition           = { };
    transition.srcCacheMask                = CoherDepthStencilTarget;
    transition.dstCacheMask                = CoherShader;
    transition.imageInfo.pImage            = pImage;
    transition.imageInfo.subresRange       = subres;
    transition.imageInfo.oldLayout         = layout;
    transition.imageInfo.newLayout         = layout;

    preBarrier.transitionCount             = 1;
    preBarrier.pTransitions                = &transition;
    preBarrier.reason                      = Developer::BarrierReasonPreComputeDepthStencilClear;

    pCmdBuffer->CmdBarrier(preBarrier);
}

// =====================================================================================================================
// Inserts barrier needed after issuing a compute clear when the target image will be immediately re-bound as a
// depth/stencil target.  Only necessary when the client specifies the DsClearAutoSync flag for a depth/stencil clear.
void PostComputeDepthStencilClearSync(
    ICmdBuffer* pCmdBuffer)
{
    BarrierInfo postBarrier        = { };
    postBarrier.waitPoint          = HwPipePreRasterization;

    constexpr HwPipePoint PostCs   = HwPipePostCs;
    postBarrier.pipePointWaitCount = 1;
    postBarrier.pPipePoints        = &PostCs;

    BarrierTransition transition   = { };
    transition.srcCacheMask        = CoherShader;
    transition.dstCacheMask        = CoherDepthStencilTarget;
    postBarrier.transitionCount    = 1;
    postBarrier.pTransitions       = &transition;
    postBarrier.reason             = Developer::BarrierReasonPostComputeDepthStencilClear;

    pCmdBuffer->CmdBarrier(postBarrier);
}

// =====================================================================================================================
// Returns a pointer to the compute pipeline used to decompress the supplied image.
const ComputePipeline* RsrcProcMgr::GetComputeMaskRamExpandPipeline(
    const Image& image
    ) const
{
    const auto&  createInfo   = image.GetImageCreateInfo();

    const auto   pipelineEnum = ((createInfo.samples == 1) ? RpmComputePipeline::ExpandMaskRam :
                                 (createInfo.samples == 2) ? RpmComputePipeline::ExpandMaskRamMs2x :
                                 (createInfo.samples == 4) ? RpmComputePipeline::ExpandMaskRamMs4x :
                                 (createInfo.samples == 8) ? RpmComputePipeline::ExpandMaskRamMs8x :
                                 RpmComputePipeline::ExpandMaskRam);

    const ComputePipeline*  pPipeline = GetPipeline(pipelineEnum);

    PAL_ASSERT(pPipeline != nullptr);

    return pPipeline;
}

// =====================================================================================================================
// Binds common graphics state.
void RsrcProcMgr::BindCommonGraphicsState(
    GfxCmdBuffer* pCmdBuffer
    ) const
{
    const InputAssemblyStateParams   inputAssemblyState   = { PrimitiveTopology::RectList };
    const DepthBiasParams            depthBias            = { 0.0f, 0.0f, 0.0f };
    const PointLineRasterStateParams pointLineRasterState = { 1.0f, 1.0f };

    const TriangleRasterStateParams  triangleRasterState =
    {
        FillMode::Solid,        // frontface fillMode
        FillMode::Solid,        // backface fillMode
        CullMode::_None,        // cullMode
        FaceOrientation::Cw,    // frontFace
        ProvokingVertex::First  // provokingVertex
    };

    GlobalScissorParams scissorParams = { };
    scissorParams.scissorRegion.extent.width  = MaxScissorExtent;
    scissorParams.scissorRegion.extent.height = MaxScissorExtent;

    pCmdBuffer->CmdSetInputAssemblyState(inputAssemblyState);
    pCmdBuffer->CmdSetDepthBiasState(depthBias);
    pCmdBuffer->CmdSetPointLineRasterState(pointLineRasterState);
    pCmdBuffer->CmdSetTriangleRasterState(triangleRasterState);
    pCmdBuffer->CmdSetClipRects(DefaultClipRectsRule, 0, nullptr);
    pCmdBuffer->CmdSetGlobalScissor(scissorParams);

}

// =====================================================================================================================
// Returns a pointer to the compute pipeline used for fast-clearing hTile data that is laid out in a linear fashion.
const ComputePipeline* RsrcProcMgr::GetLinearHtileClearPipeline(
    bool    expClearEnable,
    bool    tileStencilDisabled,
    uint32  hTileMask
    ) const
{
    // Determine which pipeline to use for this clear.
    const ComputePipeline* pPipeline = nullptr;
    if (expClearEnable)
    {
        // If Exp/Clear is enabled, fast clears require using a special Exp/Clear shader. One such shader exists for
        // depth/stencil Images and for depth-only Images.
        if (tileStencilDisabled == false)
        {
            pPipeline = GetPipeline(RpmComputePipeline::FastDepthStExpClear);
        }
        else
        {
            pPipeline = GetPipeline(RpmComputePipeline::FastDepthExpClear);
        }
    }
    else if (hTileMask == UINT_MAX)
    {
        // If the HTile mask has all bits set, we can use the standard ClearHtile path.
        // Set the pipeline to null so we don't attempt to use it.
        pPipeline = nullptr;
    }
    else
    {
        // Otherwise use the depth clear read-write shader.
        pPipeline = GetPipeline(RpmComputePipeline::FastDepthClear);
    }

    return pPipeline;
}

/// BltMonitorDesc defines a parametrized model for monitors supported by the Desktop Composition interface.
struct BltMonitorDesc
{
    uint32      numPixels;          // Number of pixels packed into a single word
    bool        isColorType;        // True if color monitor, False for monochrome
    bool        isSplitType;        // True if the packed pixels are not adjacent (on screen)
    float       scalingParams[4];   // scaling parameters which is used to convert from float to 10-bit uints
    float       grayScalingMap[12]; // Luminance constants which convert color to monochrome
    uint32      packParams[24];     // parametrized packing layout
};

/// PackPixelConstant describes a set of constants which will be passed to PackedPixelComposite shader.
///     c0       desktop sampling scale/offset for left/first pixel
///     c1       desktop sampling scale/offset for right/third pixel
///     c2       shader flow control parameters
///     c3-c5    color to grayscale conversion matrix
///     c6-c7    left pixel pack parameters
///     c8-c9    middle pixel pack parameters
///     c10-c11  right pixel packing parameters
///     c12      scaling parameters which is used to convert from float to 10-bit unsigned integers
///     c13      region.width*1.0, region.height*1.0, region.width, region.height
struct PackPixelConstant
{
    uint32 aluConstant0[4];
    uint32 aluConstant1[4];
    uint32 aluConstant2[4];
    uint32 aluConstant3[4];
    uint32 aluConstant4[4];
    uint32 aluConstant5[4];
    uint32 aluConstant6[4];
    uint32 aluConstant7[4];
    uint32 aluConstant8[4];
    uint32 aluConstant9[4];
    uint32 aluConstant10[4];
    uint32 aluConstant11[4];
    uint32 aluConstant12[4];
    uint32 aluConstant13[4];
};

static const BltMonitorDesc Desc_NotPacked =
{
    1,                                  // Number of packed pixels
    true,                               // isColorType ? (predicate)
    false,                              // isSplitType ? (predicate)
    255.0f, 1/255.0f, 0, 0,            // pixel precision (2^N-1, 1/(2^N-1))

    1.0f, 0.0f, 0.0f, 0.0f,            // grayScaling
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
};

static const BltMonitorDesc Desc_SplitG70B54_R70B10 =
{
    2,                                  // Number of packed pixels
    false,                              // isColorType ? (predicate)
    true,                               // isSplitType ? (predicate)
    1023.0f, 1/1023.0f, 0, 0,           // pixel precision (2^N-1, 1/(2^N-1))

    0.2126f, 0.7152f, 0.0722f, 0.0f,    // grayScaling
    0.2126f, 0.7152f, 0.0722f, 0.0f,
    0.2126f, 0.7152f, 0.0722f, 0.0f,

    0x00, 0xff, 0x00, 2,        // Most significant bits for the first pixel
    0x00, 0x00, 0x30, 4,        // Least significant bits for the first pixel
    0xff, 0x00, 0x00, 2,        // Most significant bits for the second pixel
    0x00, 0x00, 0x03, 0         // Least significant bits for the second pixel

};

static const BltMonitorDesc Desc_SplitB70G10_R70G76 =
{
    2,                                  // Number of packed pixels
    false,                              // isColorType ? (predicate)
    true,                               // isSplitType ? (predicate)
    1023.0f, 1/1023.0f, 0, 0,           // pixel precision (2^N-1, 1/(2^N-1))

    0.2126f, 0.7152f, 0.0722f, 0.0f,    // grayScaling
    0.2126f, 0.7152f, 0.0722f, 0.0f,
    0.2126f, 0.7152f, 0.0722f, 0.0f,

    0x00, 0x00, 0xff, 2,        // Most significant bits for the first pixel
    0x00, 0x03, 0x00, 0,        // Least significant bits for the first pixel
    0xff, 0x00, 0x00, 2,        // Most significant bits for the second pixel
    0x00, 0xc0, 0x00, 6          // Least significant bits for the second pixel

};

static const BltMonitorDesc Desc_G70B54_R70B10 =
{
    2,                                 // Number of packed pixels
    false,                             // isColorType ? (predicate)
    false,                             // isSplitType ? (predicate)
    1023.0f, 1/1023.0f, 0, 0,          // pixel precision (2^N-1, 1/(2^N-1))

    0.2126f, 0.7152f, 0.0722f, 0.0f,    // grayScaling
    0.2126f, 0.7152f, 0.0722f, 0.0f,
    0.2126f, 0.7152f, 0.0722f, 0.0f,

    0x00, 0xff, 0x00, 2,        // Most significant bits for the first pixel
    0x00, 0x00, 0x30, 4,        // Least significant bits for the first pixel
    0xff, 0x00, 0x00, 2,        // Most significant bits for the second pixel
    0x00, 0x00, 0x03, 0          // Least significant bits for the second pixel
};

static const BltMonitorDesc Desc_B70R32_G70R76 =
{
    2,                                 // Number of packed pixels
    false,                             // isColorType ? (predicate)
    false,                             // isSplitType ? (predicate)
    1023.0f, 1/1023.0f, 0, 0,          // pixel precision (2^N-1, 1/(2^N-1))

    0x00, 0x00, 0xff, 2,        // Most significant bits for the first pixel
    0x0c, 0x00, 0x00, 2,        // Least significant bits for the first pixel
    0x00, 0xff, 0x00, 2,        // Most significant bits for the second pixel
    0xc0, 0x00, 0x00, 6         // Least significant bits for the second pixel

};

static const BltMonitorDesc Desc_B70R30_G70R74 =
{
    2,                                 // Number of packed pixels
    false,                             // isColorType ? (predicate)
    false,                             // isSplitType ? (predicate)
    4095.0f, 1/4095.0f, 0, 0,          // pixel precision (2^N-1, 1/(2^N-1))

    0.2126f, 0.7152f, 0.0722f, 0.0f,   // grayScaling
    0.2126f, 0.7152f, 0.0722f, 0.0f,
    0.2126f, 0.7152f, 0.0722f, 0.0f,

    0x00, 0x00, 0xff, 4,        // Most significant bits for the first pixel
    0x0f, 0x00, 0x00, 0,        // Least significant bits for the first pixel
    0x00, 0xff, 0x00, 4,        // Most significant bits for the second pixel
    0xf0, 0x00, 0x00, 4         // Least significant bits for the second pixel
};

static const BltMonitorDesc Desc_B70_G70_R70 =
{
    3,                                // Number of packed pixels
    false,                            // isColorType ? (predicate)
    false,                            // isSplitType ? (predicate)
    255.0f, 1/255.0f, 0, 0,           // pixel precision (2^N-1, 1/(2^N-1))

    0.2126f, 0.7152f, 0.0722f, 0.0f,  // grayScaling
    0.2126f, 0.7152f, 0.0722f, 0.0f,
    0.2126f, 0.7152f, 0.0722f, 0.0f,

    0x00, 0x00, 0xff, 0,        // Most significant bits for the first pixel
    0x00, 0x00, 0x00, 0,        // Least significant bits for the first pixel
    0x00, 0xff, 0x00, 0,        // Most significant bits for the second pixel
    0x00, 0x00, 0x00, 0,        // Least significant bits for the second pixel
    0xff, 0x00, 0x00, 0,        // Most significant bits for the third pixel
    0x00, 0x00, 0x00, 0         // Least significant bits for the third pixel

};

static const BltMonitorDesc Desc_R70G76 =
{
    1,                                // Number of packed pixels
    false,                            // isColorType ? (predicate)
    false,                            // isSplitType ? (predicate)
    1023.0f, 1/1023.0f, 0, 0,         // pixel precision (2^N-1, 1/(2^N-1))

    0.2126f, 0.7152f, 0.0722f, 0.0f,  // grayScaling
    0.2126f, 0.7152f, 0.0722f, 0.0f,
    0.2126f, 0.7152f, 0.0722f, 0.0f,

    0xff, 0x00, 0x00, 2,        // Most significant bits for the first pixel
    0x00, 0xc0, 0x00, 6         // Least significant bits for the first pixel

};

static const BltMonitorDesc Desc_G70B54 =
{
    1,                                  // Number of packed pixels
    false,                              // isColorType ? (predicate)
    false,                              // isSplitType ? (predicate)
    1023.0f, 1/1023.0f, 0, 0,       // pixel precision (2^N-1, 1/(2^N-1))

    0.2126f, 0.7152f, 0.0722f, 0.0f,  // grayScaling
    0.2126f, 0.7152f, 0.0722f, 0.0f,
    0.2126f, 0.7152f, 0.0722f, 0.0f,

    0x00, 0xff, 0x00, 2,        // Most significant bits for the first pixel
    0x00, 0x00, 0x30, 4         // Least significant bits for the first pixel

};

static const BltMonitorDesc Desc_Native =
{
    1,                                  // Number of packed pixels
    true,                               // isColorType ? (predicate)
    false,                              // isSplitType ? (predicate)
    1023.0f, 1/1023.0f, 0.0f, 0.0f,     // pixel precision (2^N-1, 1/(2^N-1))

    1.0f, 0.0f, 0.0f, 0.0f,             // grayScaling
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
};

// =====================================================================================================================
// Return pointer to parametrized monitor description given the specified (input) packed pixel type.
static const BltMonitorDesc* GetMonitorDesc(
    PackedPixelType packedPixelType)  // packed pixel type
{
    const BltMonitorDesc* pDesc = nullptr;
    switch (packedPixelType)
    {
    case PackedPixelType::NotPacked:
        pDesc = &Desc_NotPacked;
        break;

    case PackedPixelType::SplitG70B54_R70B10:
        pDesc = &Desc_SplitG70B54_R70B10;
        break;

    case PackedPixelType::SplitB70G10_R70G76:
        pDesc = &Desc_SplitB70G10_R70G76;
        break;

    case PackedPixelType::G70B54_R70B10:
        pDesc = &Desc_G70B54_R70B10;
        break;

    case PackedPixelType::B70R32_G70R76:
        pDesc = &Desc_B70R32_G70R76;
        break;

    case PackedPixelType::B70R30_G70R74:
        pDesc = &Desc_B70R30_G70R74;
        break;

    case PackedPixelType::B70_G70_R70:
        pDesc = &Desc_B70_G70_R70;
        break;

    case PackedPixelType::R70G76:
        pDesc = &Desc_R70G76;
        break;

    case PackedPixelType::G70B54:
        pDesc = &Desc_G70B54;
        break;

    case PackedPixelType::Native:
        pDesc = &Desc_Native;
        break;

    default:
        break;
    }
    return pDesc;
}

// =====================================================================================================================
// return packed pixel constant scaling and offset constant based on packed pixel state
static const void ProcessPackPixelCopyConstants(
    const BltMonitorDesc&    monDesc,
    uint32                   packFactor,
    const ImageCopyRegion&   regions,
    float*                   pAluConstants)
{
    float leftOffset;
    float rightOffset;
    float scale;

    scale = (monDesc.isSplitType)? 0.5f : 1.0f;

    if (monDesc.isSplitType)
    {
        leftOffset = 0.5f * regions.srcOffset.x;
        rightOffset = 0.5f;
    }
    else
    {
        const float pixelWidth = 1.0f / static_cast<float>(regions.extent.width * monDesc.numPixels);
        const float offset     = (packFactor == 2) ? (pixelWidth / 2.0f) : pixelWidth;

        leftOffset = -offset;
        rightOffset = offset;
    }

    // c13 -> region.width*1.0, region.height*1.0, region.width, region.height
    pAluConstants[52] = 1.0f * regions.extent.width;
    pAluConstants[53] = 1.0f * regions.extent.height;

    pAluConstants[0] = scale;
    pAluConstants[1] = 1.0f;
    pAluConstants[2] = leftOffset;
    pAluConstants[3] = 0.0f;
    pAluConstants[4] = scale;
    pAluConstants[5] = 1.0f;
    pAluConstants[6] = rightOffset;
    pAluConstants[7] = 0.0f;
}

// =====================================================================================================================
// Builds commands to copy from rendered surface to an internal surface with packing 2/3 pixels to 1 R8G8B8A8 pixel.
void RsrcProcMgr::CopyImageToPackedPixelImage(
    GfxCmdBuffer*          pCmdBuffer,
    const Image&           srcImage,
    const Image&           dstImage,
    uint32                 regionCount,
    const ImageCopyRegion* pRegions,
    Pal::PackedPixelType   packPixelType
    ) const
{
    const auto&           device        = *m_pDevice->Parent();
    const auto&           dstCreateInfo = dstImage.GetImageCreateInfo();
    const auto&           srcCreateInfo = srcImage.GetImageCreateInfo();
    const bool            isCompressed  = (Formats::IsBlockCompressed(srcCreateInfo.swizzledFormat.format) ||
                                           Formats::IsBlockCompressed(dstCreateInfo.swizzledFormat.format));
    const bool            useMipInSrd   = CopyImageUseMipLevelInSrd(isCompressed);
    const BltMonitorDesc* pMonDesc      = GetMonitorDesc(packPixelType);

    // Get the appropriate pipeline object.
    const ComputePipeline* pPipeline = GetPipeline(RpmComputePipeline::PackedPixelComposite);

    // Get number of threads per groups in each dimension, we will need this data later.
    uint32 threadsPerGroup[3] = {};
    pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

    // Save current command buffer state and bind the pipeline.
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    // ALU constants assignment
    PackPixelConstant constantData    = {};
    const uint32      aluConstantSize = sizeof(constantData.aluConstant0);
    constexpr uint32 DataDwords = (sizeof(constantData) / sizeof(uint32));

    // c2 shader flow control
    constantData.aluConstant2[0] = pMonDesc->isColorType;
    constantData.aluConstant2[1] = ((pMonDesc->numPixels > 1) ? 1 : 0);
    constantData.aluConstant2[2] = ((pMonDesc->numPixels != 2) ? 1 : 0);
    constantData.aluConstant2[3] = pMonDesc->isSplitType;

    // c3 - c5 color -> color gray matrix
    memcpy(&constantData.aluConstant3[0], &pMonDesc->grayScalingMap[0], aluConstantSize);
    memcpy(&constantData.aluConstant4[0], &pMonDesc->grayScalingMap[4], aluConstantSize);
    memcpy(&constantData.aluConstant5[0], &pMonDesc->grayScalingMap[8], aluConstantSize);

    if (pMonDesc->isColorType == 0)
    {
        // c6  - c7 left pixel pack parameters (rmask, gmask, bmask, shift)
        // c8  - c9 mid pixel pack parameters
        // c10 - c11 right pixel pack parameters
        if (pMonDesc->numPixels == 1)
        {
            memcpy(&constantData.aluConstant8[0], &pMonDesc->packParams[0], aluConstantSize);
            memcpy(&constantData.aluConstant9[0], &pMonDesc->packParams[4], aluConstantSize);
        }
        else if (pMonDesc->numPixels == 2)
        {
            memcpy(&constantData.aluConstant6[0], &pMonDesc->packParams[0], aluConstantSize);
            memcpy(&constantData.aluConstant7[0], &pMonDesc->packParams[4], aluConstantSize);

            memcpy(&constantData.aluConstant10[0], &pMonDesc->packParams[8], aluConstantSize);
            memcpy(&constantData.aluConstant11[0], &pMonDesc->packParams[12], aluConstantSize);
        }
        else if (pMonDesc->numPixels == 3)
        {
            memcpy(&constantData.aluConstant6[0], &pMonDesc->packParams[0], aluConstantSize);
            memcpy(&constantData.aluConstant7[0], &pMonDesc->packParams[4], aluConstantSize);

            memcpy(&constantData.aluConstant8[0], &pMonDesc->packParams[8], aluConstantSize);
            memcpy(&constantData.aluConstant9[0], &pMonDesc->packParams[12], aluConstantSize);

            memcpy(&constantData.aluConstant10[0], &pMonDesc->packParams[16], aluConstantSize);
            memcpy(&constantData.aluConstant11[0], &pMonDesc->packParams[20], aluConstantSize);
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }
    }

    // c12 pixel scaling (2^N-1, 1/(2^N-1), unused, unused)
    memcpy(&constantData.aluConstant12[0], &pMonDesc->scalingParams[0], sizeof(pMonDesc->scalingParams[0]) * 4);

    // Now begin processing the list of copy regions.
    for (uint32 idx = 0; idx < regionCount; ++idx)
    {
        const ImageCopyRegion& region = pRegions[idx];

        PAL_ASSERT((region.numSlices == 1) || (region.extent.depth == 1));

        SwizzledFormat srcFormat = srcImage.SubresourceInfo(region.srcSubres)->format;
        SwizzledFormat dstFormat = dstImage.SubresourceInfo(region.dstSubres)->format;

        // set up c0/c1 sample scaling and offset
        ProcessPackPixelCopyConstants(*pMonDesc, pMonDesc->numPixels,
                                      region,
                                      reinterpret_cast<float*>(&constantData.aluConstant0[0]));

        // c13 -> region.width*1.0, region.height*1.0, region.width, region.height
        constantData.aluConstant13[2] = region.dstOffset.x + region.extent.width;
        constantData.aluConstant13[3] = region.dstOffset.y + region.extent.height;

        // there are 2 resources and 1 sampler
        const uint8 rsNum = 3;
        uint32* pUserData = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                   SrdDwordAlignment() * rsNum + DataDwords,
                                                                   SrdDwordAlignment(),
                                                                   PipelineBindPoint::Compute,
                                                                   0);
        ImageViewInfo imageView[2] = {};
        SubresRange   viewRange    = { region.dstSubres, 1, 1};
        RpmUtil::BuildImageViewInfo(&imageView[0],
                                    dstImage,
                                    viewRange,
                                    dstFormat,
                                    RpmUtil::DefaultRpmLayoutShaderWrite,
                                    Pal::ImageTexOptLevel::Default);

        viewRange.startSubres = region.srcSubres;
        RpmUtil::BuildImageViewInfo(&imageView[1],
                                    srcImage,
                                    viewRange,
                                    srcFormat,
                                    RpmUtil::DefaultRpmLayoutRead,
                                    Pal::ImageTexOptLevel::Default);

        if (useMipInSrd == false)
        {
            // The miplevel as specified in the shader instruction is actually an offset from the mip-level
            // as specified in the SRD.
            imageView[0].subresRange.startSubres.mipLevel = 0;  // dst
            imageView[1].subresRange.startSubres.mipLevel = 0;  // src

            // The mip-level from the instruction is also clamped to the "last level" as specified in the SRD.
            imageView[0].subresRange.numMips = region.dstSubres.mipLevel + viewRange.numMips;
            imageView[1].subresRange.numMips = region.srcSubres.mipLevel + viewRange.numMips;
        }

        // Turn our image views into HW SRDs here
        device.CreateImageViewSrds(2, &imageView[0], pUserData);
        pUserData += SrdDwordAlignment() * 2;

        Pal::SamplerInfo samplerInfo = {};

        samplerInfo.filter.magnification = Pal::XyFilterPoint;
        samplerInfo.filter.minification  = Pal::XyFilterPoint;
        samplerInfo.filter.mipFilter     = Pal::MipFilterNone;
        samplerInfo.addressU             = Pal::TexAddressMode::Clamp;
        samplerInfo.addressV             = Pal::TexAddressMode::Clamp;
        samplerInfo.addressW             = Pal::TexAddressMode::Clamp;

        device.CreateSamplerSrds(1, &samplerInfo, pUserData);
        pUserData += SrdDwordAlignment();
        // Copy the copy parameters into the embedded user-data space
        memcpy(pUserData, &constantData, sizeof(constantData));

        // Execute the dispatch, we need one thread per texel.
        pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroups(region.extent.width,  threadsPerGroup[0]),
                                RpmUtil::MinThreadGroups(region.extent.height, threadsPerGroup[1]),
                                RpmUtil::MinThreadGroups(1,  threadsPerGroup[2]));
    }
    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
void RsrcProcMgr::CmdGfxDccToDisplayDcc(
    GfxCmdBuffer*  pCmdBuffer,
    const IImage&  image
    ) const
{
    HwlGfxDccToDisplayDcc(pCmdBuffer, static_cast<const Pal::Image&>(image));
}

// =====================================================================================================================
// Put displayDCC memory itself back into a "fully decompressed" state.
void RsrcProcMgr::CmdDisplayDccFixUp(
    GfxCmdBuffer*      pCmdBuffer,
    const IImage&      image
    ) const
{
    InitDisplayDcc(pCmdBuffer, static_cast<const Pal::Image&>(image));
}

} // Pal
