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

#include "core/device.h"
#include "core/hw/gfxip/depthStencilState.h"
#include "core/hw/gfxip/msaaState.h"
#include "core/hw/gfxip/graphicsPipeline.h"
#include "core/hw/gfxip/computePipeline.h"
#include "core/hw/gfxip/queryPool.h"
#include "core/hw/gfxip/gfx12/gfx12UniversalCmdBuffer.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"
#include "core/hw/gfxip/gfx12/gfx12GraphicsPipeline.h"
#include "core/hw/gfxip/gfx12/gfx12Image.h"
#include "core/hw/gfxip/gfx12/gfx12Metadata.h"
#include "core/hw/gfxip/rpm/gfx12/gfx12RsrcProcMgr.h"
#include "core/hw/gfxip/rpm/rpmUtil.h"
#include "palAutoBuffer.h"
#include "palLiterals.h"

#include <float.h>

using namespace Pal::Formats::Gfx12;
using namespace Util;
using namespace Util::Literals;

namespace Pal
{
namespace Gfx12
{

// Maps export formats to graphics state enum offsets. The offsets are relative to
// RpmGfxPipeline::SlowColorClear(X)_32ABGR. The offset -1 indicates that there is no pipeline for a given format.
constexpr int32 ExportStateMapping[] =
{
    -1, // SPI_SHADER_ZERO is not supported.
    static_cast<int32>(SlowColorClear_32R)     - static_cast<int32>(SlowColorClear_32ABGR),
    static_cast<int32>(SlowColorClear_32GR)    - static_cast<int32>(SlowColorClear_32ABGR),
    -1, // SPI_SHADER_32_AR is not supported.
    static_cast<int32>(SlowColorClear_FP16)    - static_cast<int32>(SlowColorClear_32ABGR),
    static_cast<int32>(SlowColorClear_UNORM16) - static_cast<int32>(SlowColorClear_32ABGR),
    static_cast<int32>(SlowColorClear_SNORM16) - static_cast<int32>(SlowColorClear_32ABGR),
    static_cast<int32>(SlowColorClear_UINT16)  - static_cast<int32>(SlowColorClear_32ABGR),
    static_cast<int32>(SlowColorClear_SINT16)  - static_cast<int32>(SlowColorClear_32ABGR),
    static_cast<int32>(SlowColorClear_32ABGR)  - static_cast<int32>(SlowColorClear_32ABGR),
};

// Specify which planes the DepthStencil clear operation will write to.
enum DsClearMask : uint32
{
    ClearDepth   = 0x1,
    ClearStencil = 0x2
};

// =====================================================================================================================
RsrcProcMgr::RsrcProcMgr(
    Device* pDevice)
    :
    Pal::RsrcProcMgr(pDevice)
{
}

// =====================================================================================================================
void RsrcProcMgr::ExpandHiSZWithFullRange(
    GfxCmdBuffer*      pCmdBuffer,
    const Pal::IImage& image,
    const SubresRange& range,
    bool               trackBltActiveFlags
    ) const
{
    const auto&        palImage   = static_cast<const Pal::Image&>(image);
    const Image&       gfx12Image = static_cast<const Image&>(*palImage.GetGfxImage());
    const ChNumFormat& format     = image.GetImageCreateInfo().swizzledFormat.format;
    const HiSZ*        pHiSZ      = gfx12Image.GetHiSZ();

    PAL_ASSERT(pHiSZ != nullptr);
    PAL_ASSERT(range.numPlanes == 1);

    if ((range.startSubres.plane == 0) && m_pDevice->Parent()->SupportsDepth(format, ImageTiling::Optimal))
    {
        ClearHiSZ(pCmdBuffer, gfx12Image, range, HiSZType::HiZ, pHiSZ->GetHiZInitialValue(), trackBltActiveFlags);
    }
    else
    {
        ClearHiSZ(pCmdBuffer, gfx12Image, range, HiSZType::HiS, pHiSZ->GetHiSInitialValue(), trackBltActiveFlags);
    }
}

// =====================================================================================================================
void RsrcProcMgr::FixupHiSZWithClearValue(
    GfxCmdBuffer*      pCmdBuffer,
    const Pal::IImage& image,
    const SubresRange& range,
    float              depth,
    uint8              stencil,
    bool               trackBltActiveFlags
    ) const
{
    const auto&        palImage   = static_cast<const Pal::Image&>(image);
    const Image&       gfx12Image = static_cast<const Image&>(*palImage.GetGfxImage());
    const ChNumFormat& format     = image.GetImageCreateInfo().swizzledFormat.format;
    const HiSZ*        pHiSZ      = gfx12Image.GetHiSZ();

    PAL_ASSERT(pHiSZ != nullptr);

    if ((range.startSubres.plane == 0) && m_pDevice->Parent()->SupportsDepth(format, ImageTiling::Optimal))
    {
        ClearHiSZ(pCmdBuffer, gfx12Image, range, HiSZType::HiZ, pHiSZ->GetHiZClearValue(depth), trackBltActiveFlags);
    }
    else
    {
        ClearHiSZ(pCmdBuffer, gfx12Image, range, HiSZType::HiS, pHiSZ->GetHiSClearValue(stencil), trackBltActiveFlags);
    }
}

// =====================================================================================================================
// The function checks HW specific conditions to see if allow clone copy,
//   - For both image with metadata case, if source image's layout is compatible with dst image's layout.
bool RsrcProcMgr::UseImageCloneCopy(
    GfxCmdBuffer*          pCmdBuffer,
    const Pal::Image&      srcImage,
    ImageLayout            srcImageLayout,
    const Pal::Image&      dstImage,
    ImageLayout            dstImageLayout,
    uint32                 regionCount,
    const ImageCopyRegion* pRegions,
    uint32                 flags
    ) const
{
    bool useCloneCopy = Pal::RsrcProcMgr::UseImageCloneCopy(pCmdBuffer, srcImage, srcImageLayout, dstImage,
                                                            dstImageLayout, regionCount, pRegions, flags);

    // Check src image is enough as both images should have the same metadata info if useCloneCopy == true.
    if (useCloneCopy && srcImage.HasMetadata())
    {
        const auto&  gfx12SrcImage = static_cast<const Image&>(*srcImage.GetGfxImage());
        const auto&  gfx12DstImage = static_cast<const Image&>(*dstImage.GetGfxImage());
        const uint32 numPlanes     = srcImage.GetImageInfo().numPlanes;

        // DepthStencilLayoutToState may change with different plane but not mipLevel or slice.
        // Currently clone copy only supports full copy, so loop all planes here.
        for (uint32 plane = 0; useCloneCopy && (plane < numPlanes); plane++)
        {
            const ImageLayout srcHiSZValidLayout = gfx12SrcImage.GetHiSZValidLayout(plane);
            const ImageLayout dstHiSZValidLayout = gfx12DstImage.GetHiSZValidLayout(plane);

            const DepthStencilHiSZState srcState = ImageLayoutToDepthStencilHiSZState(srcHiSZValidLayout,
                                                                                      srcImageLayout);
            const DepthStencilHiSZState dstState = ImageLayoutToDepthStencilHiSZState(dstHiSZValidLayout,
                                                                                      dstImageLayout);

            // Only support clone copy if source layout is compatible with destination layout.
            if (srcState == DepthStencilNoHiSZ)
            {
                useCloneCopy &= (srcState == dstState);
            }
            // else if (srcState == DepthStencilWithHiSZ), always support clone copy.
        }
    }

    return useCloneCopy;
}

// =====================================================================================================================
// Clones the image data from the source image while preserving its state and avoiding decompressing.
void RsrcProcMgr::CmdCloneImageData(
    GfxCmdBuffer*     pCmdBuffer,
    const Pal::Image& srcImage,
    const Pal::Image& dstImage
    ) const
{
    // Check our assumptions:
    // 1. Both images need to be cloneable.
    // 2. Both images must have been created with identical create info.
    // 3. Both images must have been created with identical memory layout.
    PAL_ASSERT(srcImage.IsCloneable() && dstImage.IsCloneable());
    PAL_ASSERT(srcImage.GetImageCreateInfo() == dstImage.GetImageCreateInfo());
    PAL_ASSERT(srcImage.GetGpuMemSize() == dstImage.GetGpuMemSize());

    const ImageMemoryLayout& srcImgMemLayout   = srcImage.GetMemoryLayout();
    const bool               hasMetadataHeader = (srcImgMemLayout.metadataHeaderSize != 0);

    if (hasMetadataHeader)
    {
        // First copy header by PFP
        // We always read and write the metadata header using the PFP so the copy must also use the PFP and no need
        // call SetCpMemoryWriteL2CacheStaleState(true) here to track cache coherency.
        Pal::CmdStream* pCmdStream = pCmdBuffer->GetMainCmdStream();

        DmaDataInfo dmaDataInfo = {};
        dmaDataInfo.dstSel      = dst_sel__pfp_dma_data__dst_addr_using_l2;
        dmaDataInfo.srcSel      = src_sel__pfp_dma_data__src_addr_using_l2;
        dmaDataInfo.sync        = true;
        dmaDataInfo.usePfp      = true;
        dmaDataInfo.predicate   = Pm4Predicate(pCmdBuffer->GetPacketPredicate());
        dmaDataInfo.dstAddr     = dstImage.GetBoundGpuMemory().GpuVirtAddr() + srcImgMemLayout.metadataHeaderOffset;
        dmaDataInfo.srcAddr     = srcImage.GetBoundGpuMemory().GpuVirtAddr() + srcImgMemLayout.metadataHeaderOffset;
        dmaDataInfo.numBytes    = uint32(srcImgMemLayout.metadataHeaderSize);

        uint32* pCmdSpace = pCmdStream->ReserveCommands();
        pCmdSpace += CmdUtil::BuildDmaData<false>(dmaDataInfo, pCmdSpace);
        pCmdStream->CommitCommands(pCmdSpace);
    }

    // Do the rest copy
    // If has metadata header, copy all of the source image excluding metadata header to the dest image; otherwise
    // copy the whole memory.
    Pal::MemoryCopyRegion copyRegion = {};

    copyRegion.srcOffset = srcImage.GetBoundGpuMemory().Offset();
    copyRegion.dstOffset = dstImage.GetBoundGpuMemory().Offset();
    copyRegion.copySize  = hasMetadataHeader ? srcImgMemLayout.metadataHeaderOffset : dstImage.GetGpuMemSize();

    CopyMemoryCs(pCmdBuffer,
                 *srcImage.GetBoundGpuMemory().Memory(),
                 *dstImage.GetBoundGpuMemory().Memory(),
                 1,
                 &copyRegion);
}

// =====================================================================================================================
// The queue preamble streams set COMPUTE_USER_DATA_0 to the address of the global internal table, as required by the
// PAL compute pipeline ABI. If we overwrite that register in a command buffer we need some way to restore it the next
// time we bind a compute pipeline. We don't know the address of the internal table at the time we build command
// buffers so we must query it dynamically on the GPU. Unfortunately the CP can't read USER_DATA registers so we must
// use a special pipeline to simply read the table address from user data and write it to a known GPU address.
//
// This function binds and executes that special compute pipeline. It will write the low 32-bits of the global internal
// table address to dstAddr. Later on, we can tell the CP to read those bits and write them to COMPUTE_USER_DATA_0.
void RsrcProcMgr::EchoGlobalInternalTableAddr(
    GfxCmdBuffer* pCmdBuffer,
    gpusize       dstAddr
    ) const
{
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
    const Pal::ComputePipeline*const pPipeline = GetPipeline(RpmComputePipeline::Gfx12EchoGlobalTable);
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash });

    // Note we start at userdata2 here because the pipeline is special and userdata0/1 are marked unused but
    // overlap the global table.
    const uint32 userData[2] = { LowPart(dstAddr), HighPart(dstAddr) };
    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 2, 2, userData);
    pCmdBuffer->CmdDispatch({ 1, 1, 1 }, {});
    pCmdBuffer->CmdRestoreComputeStateInternal(ComputeStatePipelineAndUserData);

    // We need a CS wait-for-idle before we try to restore the global internal table user data. There are a few ways
    // we could accomplish that, but the most simple way is to just do a wait for idle right here. We only need to
    // call this function once per command buffer (and only if we use a non-PAL ABI pipeline) so it should be fine.
    Pal::CmdStream* pCmdStream = pCmdBuffer->GetMainCmdStream();
    uint32*         pCmdSpace  = pCmdStream->ReserveCommands();

    pCmdSpace += CmdUtil::BuildNonSampleEventWrite(CS_PARTIAL_FLUSH, pCmdBuffer->GetEngineType(), pCmdSpace);

    if (pCmdBuffer->IsGraphicsSupported())
    {
        // Note that we also need a PFP_SYNC_ME on any graphics queues because the PFP loads from this memory.
        CmdUtil::BuildPfpSyncMe(pCmdSpace);
    }

    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void RsrcProcMgr::CmdUpdateMemory(
    GfxCmdBuffer*    pCmdBuffer,
    const GpuMemory& dstMem,
    gpusize          dstOffset,
    gpusize          dataSize,
    const uint32*    pData
    ) const
{
    // Prepare to issue one or more DmaCopyMemory packets. Start the dstAddr at the beginning of the dst buffer. The
    // srcAddr and numBytes will be set in the loop.
    const uint32     embeddedDataLimit = pCmdBuffer->GetEmbeddedDataLimit() * sizeof(uint32);
    constexpr uint32 embeddedDataAlign = 1;
    gpusize          dstAddr           = dstMem.Desc().gpuVirtAddr + dstOffset;

    // Loop until we've submitted enough DmaCopyMemory packets to upload the whole src buffer.
    const void* pRemainingSrcData = pData;
    uint32      remainingDataSize = static_cast<uint32>(dataSize);
    while (remainingDataSize > 0)
    {
        // Create the embedded video memory space for the next section of the src buffer.
        uint32 numBytes = Min(remainingDataSize, embeddedDataLimit);
        gpusize srcAddr;

        uint32* pBufStart = pCmdBuffer->CmdAllocateEmbeddedData(numBytes / sizeof(uint32),
                                                                embeddedDataAlign,
                                                                &srcAddr);

        memcpy(pBufStart, pRemainingSrcData, numBytes);

        // Write the DmaCopyMemory packet to the command stream.
        pCmdBuffer->CopyMemoryCp(dstAddr, srcAddr, numBytes);

        // Update all variable addresses and sizes except for srcAddr and numBytes which will be reset above.
        pRemainingSrcData =  VoidPtrInc(pRemainingSrcData, numBytes);
        remainingDataSize -= numBytes;
        dstAddr           += numBytes;
    }

    pCmdBuffer->SetCpBltState(true);
    pCmdBuffer->SetCpMemoryWriteL2CacheStaleState(true);

#if PAL_DEVELOPER_BUILD
    Developer::RpmBltData cbData = { .pCmdBuffer = pCmdBuffer, .bltType = Developer::RpmBltType::CpDmaUpdate };
    m_pDevice->Parent()->DeveloperCb(Developer::CallbackType::RpmBlt, &cbData);
#endif
}

// =====================================================================================================================
// Resolves a multisampled source Image into the single-sampled destination Image using the Image's resolve method.
void RsrcProcMgr::CmdResolveImage(
    GfxCmdBuffer*             pCmdBuffer,
    const Pal::Image&         srcImage,
    ImageLayout               srcImageLayout,
    const Pal::Image&         dstImage,
    ImageLayout               dstImageLayout,
    ResolveMode               resolveMode,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions,
    uint32                    flags
    ) const
{
    const ResolveMethod srcMethod = srcImage.GetImageInfo().resolveMethod;

    PAL_ASSERT(srcMethod.shaderCs == 1);
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
}

// =====================================================================================================================
void RsrcProcMgr::CmdResolvePrtPlusImage(
    GfxCmdBuffer*                    pCmdBuffer,
    const IImage&                    srcImage,
    ImageLayout                      srcImageLayout,
    const IImage&                    dstImage,
    ImageLayout                      dstImageLayout,
    PrtPlusResolveType               resolveType,
    uint32                           regionCount,
    const PrtPlusImageResolveRegion* pRegions
    ) const
{
    const auto*  pPalDevice    = m_pDevice->Parent();
    const auto&  srcPalImage   = static_cast<const Pal::Image&>(srcImage);
    const auto&  dstPalImage   = static_cast<const Pal::Image&>(dstImage);
    const auto&  srcCreateInfo = srcImage.GetImageCreateInfo();
    const auto&  dstCreateInfo = dstImage.GetImageCreateInfo();
    const auto   pipeline      = ((resolveType == PrtPlusResolveType::Decode)
                                  ? ((srcCreateInfo.prtPlus.mapType == PrtMapType::SamplingStatus)
                                     ? RpmComputePipeline::Gfx10PrtPlusResolveSamplingStatusMap
                                     : RpmComputePipeline::Gfx10PrtPlusResolveResidencyMapDecode)
                                  : ((dstCreateInfo.prtPlus.mapType == PrtMapType::SamplingStatus)
                                     ? RpmComputePipeline::Gfx10PrtPlusResolveSamplingStatusMap
                                     : RpmComputePipeline::Gfx10PrtPlusResolveResidencyMapEncode));
    const auto*  pPipeline     = GetPipeline(pipeline);

    // DX spec requires that resolve source and destinations be 8bpp
    PAL_ASSERT((Formats::BitsPerPixel(dstCreateInfo.swizzledFormat.format) == 8) &&
               (Formats::BitsPerPixel(srcCreateInfo.swizzledFormat.format) == 8));

    // What are we even doing here?
    PAL_ASSERT(TestAnyFlagSet(pPalDevice->ChipProperties().imageProperties.prtFeatures,
                              PrtFeatureFlags::PrtFeaturePrtPlus));

    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

    const DispatchDims threadsPerGroup = pPipeline->ThreadsPerGroupXyz();

    // Bind compute pipeline used for the resolve.
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    for (uint32 regionIdx = 0; regionIdx < regionCount; regionIdx++)
    {
        const auto& resolveRegion = pRegions[regionIdx];

        const uint32 constData[] =
        {
            // start cb0[0]
            uint32(resolveRegion.srcOffset.x),
            uint32(resolveRegion.srcOffset.y),
            uint32(resolveRegion.srcOffset.z),
            0u,
            // start cb0[1]
            uint32(resolveRegion.dstOffset.x),
            uint32(resolveRegion.dstOffset.y),
            uint32(resolveRegion.dstOffset.z),
            0u,
            // start cb0[2]
            resolveRegion.extent.width,
            resolveRegion.extent.height,
            ((srcCreateInfo.imageType == Pal::ImageType::Tex2d) ? resolveRegion.numSlices : resolveRegion.extent.depth),
            // cb0[2].w is ignored for residency maps
            ((resolveType == PrtPlusResolveType::Decode) ? 0xFFu : 0x01u),
        };

        // Create an embedded user-data table and bind it to user data 0.
        constexpr uint32 SizeConstDataDwords = NumBytesToNumDwords(sizeof(constData));
        uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                   SrdDwordAlignment() * 2 + SizeConstDataDwords,
                                                                   SrdDwordAlignment(),
                                                                   PipelineBindPoint::Compute,
                                                                   0);

        const SubresRange srcRange = SubresourceRange(
            Subres(0, resolveRegion.srcMipLevel, resolveRegion.srcSlice), 1u, 1u, resolveRegion.numSlices);
        const SubresRange dstRange = SubresourceRange(
            Subres(0, resolveRegion.dstMipLevel, resolveRegion.dstSlice), 1u, 1u, resolveRegion.numSlices);

        // For the sampling status shader, the format doesn't matter that much as it's just doing a "0" or "1"
        // comparison, but the residency map shader is decoding the bits, so we need the raw unfiltered data.
        constexpr SwizzledFormat X8Uint =
        {
            ChNumFormat::X8_Uint,
            {
                ChannelSwizzle::X,
                ChannelSwizzle::Zero,
                ChannelSwizzle::Zero,
                ChannelSwizzle::One
            }
        };

        ImageViewInfo imageView[2] = {};
        const SwizzledFormat srcFormat = ((resolveType == PrtPlusResolveType::Decode)
                                          ? X8Uint
                                          : srcCreateInfo.swizzledFormat);
        const SwizzledFormat dstFormat = ((resolveType == PrtPlusResolveType::Decode)
                                          ? dstCreateInfo.swizzledFormat
                                          : X8Uint);
        RpmUtil::BuildImageViewInfo(&imageView[0],
                                    srcPalImage,
                                    srcRange,
                                    srcFormat,
                                    srcImageLayout,
                                    pPalDevice->TexOptLevel(),
                                    false);

        RpmUtil::BuildImageViewInfo(&imageView[1],
                                    dstPalImage,
                                    dstRange,
                                    dstFormat,
                                    dstImageLayout,
                                    pPalDevice->TexOptLevel(),
                                    true);

        pPalDevice->CreateImageViewSrds(2, &imageView[0], pSrdTable);
        pSrdTable += SrdDwordAlignment() * 2;

        // And give the shader all kinds of useful dimension info
        memcpy(pSrdTable, &constData[0], sizeof(constData));

        const DispatchDims threads =
        {
            resolveRegion.extent.width,
            resolveRegion.extent.height,
            ((srcCreateInfo.imageType == Pal::ImageType::Tex2d) ? resolveRegion.numSlices : resolveRegion.extent.depth)
        };

        pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroupsXyz(threads, threadsPerGroup), {});
    }

    pCmdBuffer->CmdRestoreComputeStateInternal(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
// Resolve the query with compute shader.
void RsrcProcMgr::CmdResolveQuery(
    GfxCmdBuffer*         pCmdBuffer,
    const Pal::QueryPool& queryPool,
    QueryResultFlags      flags,
    QueryType             queryType,
    uint32                startQuery,
    uint32                queryCount,
    const GpuMemory&      dstGpuMemory,
    gpusize               dstOffset,
    gpusize               dstStride
    ) const
{
    auto*const pStream = static_cast<CmdStream*>(pCmdBuffer->GetMainCmdStream());
    PAL_ASSERT(pStream != nullptr);

    if (TestAnyFlagSet(flags, QueryResultWait) && queryPool.HasTimestamps())
    {
        // Wait for the query data to get to memory if it was requested.
        // The shader is required to implement the wait if the query pool doesn't have timestamps.
        queryPool.WaitForSlots(pCmdBuffer, pStream, startQuery, queryCount);
    }

    const Pal::ComputePipeline* pPipeline = nullptr;

    // The resolve query shaders have their own control flags that are based on QueryResultFlags.
    union ResolveQueryControl
    {
        struct
        {
            uint32 resultsAre64Bit   : 1;
            uint32 availability      : 1;
            uint32 partialResults    : 1;
            uint32 accumulateResults : 1;
            uint32 booleanResults    : 1;
            uint32 noWait            : 1;
            uint32 onlyPrimNeeded    : 1;
            uint32 reserved          : 25;
        };
        uint32     value;
    };

    // Translate the result flags and query type into the flags that the shader expects.
    ResolveQueryControl controlFlags;
    controlFlags.value             = 0;
    controlFlags.resultsAre64Bit   = TestAnyFlagSet(flags, QueryResult64Bit);
    controlFlags.availability      = TestAnyFlagSet(flags, QueryResultAvailability);
    controlFlags.partialResults    = TestAnyFlagSet(flags, QueryResultPartial);
    controlFlags.accumulateResults = TestAnyFlagSet(flags, QueryResultAccumulate);
    controlFlags.booleanResults    = (queryType == QueryType::BinaryOcclusion);
    // We should only use shader-based wait if the query pool doesn't already use timestamps.
    controlFlags.noWait            = ((TestAnyFlagSet(flags, QueryResultWait) == false) || queryPool.HasTimestamps());
    controlFlags.onlyPrimNeeded    = TestAnyFlagSet(flags, QueryResultOnlyPrimNeeded);

    uint32 constData[4]    = { controlFlags.value, queryCount, static_cast<uint32>(dstStride), 0 };
    uint32 constEntryCount = 0;

    switch (queryPool.CreateInfo().queryPoolType)
    {
    case QueryPoolType::Occlusion:
        // The occlusion query shader needs the stride of a set of zPass counters.
        pPipeline       = GetPipeline(RpmComputePipeline::Gfx12ResolveOcclusionQuery);
        constData[3]    = static_cast<uint32>(queryPool.GetGpuResultSizeInBytes(1));
        constEntryCount = 4;

        PAL_ASSERT((queryType == QueryType::Occlusion) || (queryType == QueryType::BinaryOcclusion));
        break;

    case QueryPoolType::PipelineStats:
        // The pipeline stats query shader needs the mask of enabled pipeline stats.
        pPipeline       = GetPipeline(RpmComputePipeline::Gfx12ResolvePipelineStatsQuery);
        constData[3]    = queryPool.CreateInfo().enabledStats;
        constEntryCount = 4;

        // Note that accumulation was not implemented for this query pool type because no clients support it.
        PAL_ASSERT(TestAnyFlagSet(flags, QueryResultAccumulate) == false);
        PAL_ASSERT(queryType == QueryType::PipelineStats);

        // Pipeline stats query doesn't implement shader-based wait.
        PAL_ASSERT(controlFlags.noWait == 1);
        break;

    case QueryPoolType::StreamoutStats:
        PAL_ASSERT((flags & QueryResultWait) != 0);

        pPipeline       = GetPipeline(RpmComputePipeline::Gfx12ResolveStreamoutStatsQuery);
        constEntryCount = 3;

        PAL_ASSERT((queryType == QueryType::StreamoutStats)  ||
                   (queryType == QueryType::StreamoutStats1) ||
                   (queryType == QueryType::StreamoutStats2) ||
                   (queryType == QueryType::StreamoutStats3));

        // Streamout stats query doesn't implement shader-based wait.
        PAL_ASSERT(controlFlags.noWait == 1);
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    PAL_ASSERT(pPipeline != nullptr);

    // Save current command buffer state and bind the pipeline.
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    // Create an embedded user-data table and bind it to user data 0-1. We need buffer views for the source and dest.
    uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                               SrdDwordAlignment() * 2,
                                                               SrdDwordAlignment(),
                                                               PipelineBindPoint::Compute,
                                                               0);

    // Populate the table with raw buffer views, by convention the destination is placed before the source.
    BufferViewInfo rawBufferView = {};
    RpmUtil::BuildRawBufferViewInfo(&rawBufferView, dstGpuMemory, dstOffset);
    m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &rawBufferView, pSrdTable);
    pSrdTable += SrdDwordAlignment();

    RpmUtil::BuildRawBufferViewInfo(&rawBufferView, queryPool.GpuMemory(), queryPool.GetQueryOffset(startQuery));
    m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &rawBufferView, pSrdTable);

    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 1, constEntryCount, constData);

    // Issue a dispatch with one thread per query slot.
    const uint32 threadGroups = RpmUtil::MinThreadGroups(queryCount, pPipeline->ThreadsPerGroup());
    pCmdBuffer->CmdDispatch({threadGroups, 1, 1}, {});

    // Restore the command buffer's state.
    pCmdBuffer->CmdRestoreComputeStateInternal(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
// Some blts need to use GFXIP-specific algorithms to pick the proper state. The baseState is the first
// graphics state in a series of states that vary only on target format.
const Pal::GraphicsPipeline* RsrcProcMgr::GetGfxPipelineByFormat(
    RpmGfxPipeline basePipeline,
    SwizzledFormat format
    ) const
{
    // There is only one range of pipelines that vary by export format and this is the base.
    PAL_ASSERT(basePipeline == SlowColorClear_32ABGR);

    const SPI_SHADER_EX_FORMAT exportFormat = DeterminePsExportFmt(format,
                                                                   false,  // Blend disabled
                                                                   true,   // Alpha is exported
                                                                   false,  // Blend Source Alpha disabled
                                                                   false); // Alpha-to-Coverage disabled

    const int32 pipelineOffset = ExportStateMapping[exportFormat];
    PAL_ASSERT(pipelineOffset >= 0);

    const Pal::GraphicsPipeline* selectedPipeline = nullptr;

    // When the input format's Pixel format is 'Undefined', then the pipelineOffset maybe -1. It can indicate that no
    // color/depth target will be attached when creating a graphics pipeline. In this case, we don't need a pipeline.
    if (pipelineOffset >= 0)
    {
        selectedPipeline = GetGfxPipeline(static_cast<RpmGfxPipeline>(basePipeline + pipelineOffset));
    }

    return selectedPipeline;
}

// =====================================================================================================================
// Builds commands to clear the specified ranges of an image to the given color data.
void RsrcProcMgr::CmdClearColorImage(
    GfxCmdBuffer*         pCmdBuffer,
    const Pal::Image&     dstImage,
    ImageLayout           dstImageLayout,
    const ClearColor&     color,
    const SwizzledFormat& clearFormat,
    uint32                rangeCount,
    const SubresRange*    pRanges,
    uint32                boxCount,
    const Box*            pBoxes,
    uint32                flags
    ) const
{
    // By definition, all gfx12 clears are "slow clears" so return immediately if this flag was specified.
    if (TestAnyFlagSet(flags, ColorClearSkipIfSlow))
    {
        return;
    }

    // At least one range is always required.
    PAL_ASSERT((pRanges != nullptr) && (rangeCount >= 1));

    // We'd like to know if the faster linear clear path is available when we decide between GFX and CS clears.
    const bool linearClearSupportsImage = LinearClearSupportsImage(dstImage, color, pRanges[0], boxCount, pBoxes);

    // AutoSync means the image was put into the color target state by a previous barrier. If we use a compute clear
    // we must first insert a barrier from target usage to compute usage. Once the clears are done we'll need a final
    // barrier to go back to color target usage. Normal non-AutoSync clears are already ready for CS usage.
    //
    // Note that the "blt active" flags are used to optimize the client's non-AutoSync clear barriers. We don't want
    // AutoSync clears to change these flags because, from an interface perspective, AutoSync looks fully pipelined
    // with the client's draw commands.
    const bool clearAutoSync       = TestAnyFlagSet(flags, ColorClearAutoSync);
    const bool trackBltActiveFlags = (clearAutoSync == false);
    bool       needPreCsSync       = clearAutoSync;
    bool       needPostCsSync      = false;

    const ImageCreateInfo&  createInfo = dstImage.GetImageCreateInfo();
    const Gfx12PalSettings& settings   = GetGfx12Settings(m_pDevice->Parent());

    for (uint32 rangeIdx = 0; rangeIdx < rangeCount; ++rangeIdx)
    {
        const SubresRange range = pRanges[rangeIdx];

        // We assume that multi-plane ranges are split before the command buffer calls RPM. This means all subresources
        // in this range share the same plane format.
        PAL_ASSERT((range.numPlanes == 1) && (range.numMips >= 1) && (range.numSlices >= 1));

        SwizzledFormat baseFormat = clearFormat;

        // If the caller wants us to pick the format we should just use the plane's format.
        if (clearFormat.format == ChNumFormat::Undefined)
        {
            baseFormat = dstImage.SubresourceInfo(range.startSubres)->format;
        }

        LinearClearDesc desc = {};

        // If this is true then it's legal to call TryLinearImageClear.
        const bool linearClearSupported = (linearClearSupportsImage &&
                                           FillLinearClearDesc(dstImage, range, baseFormat, &desc));

        // Call GetDefaultSlowClearMethod() first to see if graphics clear is supported or not, then judge if graphics
        // clear shall be really used when it is preferred.
        const bool canUseGfx = (pCmdBuffer->IsGraphicsSupported() &&
                                (m_pDevice->GetDefaultSlowClearMethod(createInfo, baseFormat) ==
                                 ClearMethod::NormalGraphics));

        if (canUseGfx && IsColorGfxClearPreferred(settings, desc, linearClearSupported, clearAutoSync))
        {
            SlowClearGraphics(pCmdBuffer, dstImage, dstImageLayout, color, baseFormat,
                              range, trackBltActiveFlags, boxCount, pBoxes);
        }
        else
        {
            if (needPreCsSync)
            {
                AcquireReleaseInfo acqRelInfo  = {};
                acqRelInfo.srcGlobalStageMask  = PipelineStageColorTarget;
                acqRelInfo.dstGlobalStageMask  = PipelineStageCs;
                acqRelInfo.srcGlobalAccessMask = CoherColorTarget;
                acqRelInfo.dstGlobalAccessMask = CoherShader;
                acqRelInfo.reason              = Developer::BarrierReasonPreComputeColorClear;

                pCmdBuffer->CmdReleaseThenAcquire(acqRelInfo);

                // The pre-cs sync is a global stall and cache flush so it covers all ranges, we only need it once.
                // If we needed a pre-cs sync then we'll also need a post-cs sync at the end.
                needPreCsSync  = false;
                needPostCsSync = true;
            }

            bool mustFallBack = true;

            if (linearClearSupported)
            {
                mustFallBack = TryLinearImageClear(pCmdBuffer, dstImage, settings, desc, color, trackBltActiveFlags);
            }

            if (mustFallBack)
            {
                SlowClearCompute(pCmdBuffer, dstImage, dstImageLayout, color, baseFormat, range, trackBltActiveFlags,
                                 boxCount, pBoxes);
            }
        }
    }

    if (needPostCsSync)
    {
        AcquireReleaseInfo acqRelInfo  = {};
        acqRelInfo.srcGlobalStageMask  = PipelineStageCs;
        acqRelInfo.dstGlobalStageMask  = PipelineStageColorTarget;
        acqRelInfo.srcGlobalAccessMask = CoherShader;
        acqRelInfo.dstGlobalAccessMask = CoherColorTarget;
        acqRelInfo.reason              = Developer::BarrierReasonPostComputeColorClear;

        pCmdBuffer->CmdReleaseThenAcquire(acqRelInfo);
    }
}

// =====================================================================================================================
// Builds commands to clear the specified ranges of a depth/stencil image to the specified values.
void RsrcProcMgr::CmdClearDepthStencil(
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
    uint32             flags
    ) const
{
    PAL_ASSERT((rectCount == 0) || (pRects != nullptr));

    // Convert the Rects to Boxes. We use an AutoBuffer instead of the virtual linear allocator because
    // we may need to allocate more boxes than will fit in the fixed virtual space.
    AutoBuffer<Box, 16, Platform> boxes(rectCount, m_pDevice->GetPlatform());

    if (boxes.Capacity() < rectCount)
    {
        // Notify the command buffer if AutoBuffer allocation has failed.
        pCmdBuffer->NotifyAllocFailure();
        return;
    }

    for (uint32 i = 0; i < rectCount; i++)
    {
        boxes[i].offset.x      = pRects[i].offset.x;
        boxes[i].offset.y      = pRects[i].offset.y;
        boxes[i].offset.z      = 0;
        boxes[i].extent.width  = pRects[i].extent.width;
        boxes[i].extent.height = pRects[i].extent.height;
        boxes[i].extent.depth  = 1;
    }

    const bool        clearAutoSync   = TestAnyFlagSet(flags, DsClearAutoSync);
    const bool        useGfxClear     = pCmdBuffer->IsGraphicsSupported() &&
                                        dstImage.IsDepthStencilTarget()   &&
                                        IsDepthStencilGfxClearPreferred(clearAutoSync);
    const bool        needComputeSync = ((useGfxClear == false) && clearAutoSync);
    const auto&       createInfo      = dstImage.GetImageCreateInfo();
    const auto&       gfx12Image      = *static_cast<const Image*>(dstImage.GetGfxImage());
    const HiSZ*       pHiSZ           = gfx12Image.GetHiSZ();
    const ChNumFormat imageFormat     = createInfo.swizzledFormat.format;
    const bool        supportsDepth   = m_pDevice->Parent()->SupportsDepth(imageFormat, ImageTiling::Optimal);
    const bool        fullBoxClear    = BoxesCoverWholeExtent(createInfo.extent, rectCount, boxes.Data());

    // Check if need pre/post sync for potential CS expand HiSZ range in gfx clear path.
    bool needHiSZExpandSyncForGfxClear = false;
    if (useGfxClear && fullBoxClear && gfx12Image.HasHiSZStateMetaData())
    {
        for (uint32 rangeIdx = 0; rangeIdx < rangeCount; rangeIdx++)
        {
            if ((pRanges[rangeIdx].numPlanes == 1) && dstImage.IsRangeFullSlices(pRanges[rangeIdx]))
            {
                needHiSZExpandSyncForGfxClear = true;
                break;
            }
        }
    }

    if (needComputeSync || needHiSZExpandSyncForGfxClear)
    {
        AcquireReleaseInfo acqRelInfo  = {};
        acqRelInfo.srcGlobalStageMask  = PipelineStageDsTarget;
        acqRelInfo.dstGlobalStageMask  = PipelineStageCs;
        acqRelInfo.srcGlobalAccessMask = CoherDepthStencilTarget;
        acqRelInfo.dstGlobalAccessMask = CoherShader;
        acqRelInfo.reason              = Developer::BarrierReasonPreComputeDepthStencilClear;

        pCmdBuffer->CmdReleaseThenAcquire(acqRelInfo);
    }

    const bool trackBltActiveFlags = (clearAutoSync == false);

    for (uint32 rangeIdx = 0; rangeIdx < rangeCount; rangeIdx++)
    {
        // Update HiSZ state metadata to allow enable HiZ/HiS after subresource full resummarization.
        const bool setHiSZStateMetadata =
            gfx12Image.HasHiSZStateMetaData()        &&
            // partial Clear is fine for compute path as ExpandHiSZWithFullRange() always handles full box.
            ((useGfxClear == false) || fullBoxClear) &&
            dstImage.IsRangeFullSlices(pRanges[rangeIdx]);

        if (useGfxClear)
        {
            PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());

            uint32 clearMask = 0;

            if (pRanges[rangeIdx].numPlanes == 2)
            {
                PAL_ASSERT(supportsDepth);
                clearMask = ClearDepth | ClearStencil;
            }
            else if ((pRanges[rangeIdx].startSubres.plane == 0) && supportsDepth)
            {
                clearMask = ClearDepth;
            }
            else
            {
                clearMask = ClearStencil;
            }

            DepthStencilClearGraphics(pCmdBuffer,
                                      gfx12Image,
                                      pRanges[rangeIdx],
                                      depth,
                                      stencil,
                                      stencilWriteMask,
                                      clearMask,
                                      depthLayout,
                                      stencilLayout,
                                      rectCount,
                                      boxes.Data());
        }
        else
        {
            // Compute Clear path
            for (uint32 plane = 0; plane < pRanges[rangeIdx].numPlanes; plane++)
            {
                SubresRange range = pRanges[rangeIdx];

                range.startSubres.plane += plane;
                range.numPlanes          = 1;

                const bool           isDepth      = (range.startSubres.plane == 0) && supportsDepth;
                const SwizzledFormat subresFormat = dstImage.SubresourceInfo(range.startSubres)->format;

                ClearColor clearColor = {};

                if (isDepth)
                {
                    // For Depth slow clears, we use a float clear color.
                    clearColor.type        = ClearColorType::Float;
                    clearColor.f32Color[0] = depth;
                }
                else
                {
                    PAL_ASSERT(m_pDevice->Parent()->SupportsStencil(imageFormat, ImageTiling::Optimal));

                    // For Stencil plane we use the stencil value directly.
                    clearColor.type                = ClearColorType::Uint;
                    clearColor.u32Color[0]         = stencil;
                    clearColor.disabledChannelMask = ~stencilWriteMask;
                }

                LinearClearDesc desc = {};
                bool mustFallBack = true;

                if (LinearClearSupportsImage(dstImage, clearColor, range, rectCount, boxes.Data()) &&
                    FillLinearClearDesc(dstImage, range, subresFormat, &desc))
                {
                    const Gfx12PalSettings& settings = GetGfx12Settings(m_pDevice->Parent());

                    mustFallBack = TryLinearImageClear(pCmdBuffer, dstImage, settings, desc, clearColor,
                                                       trackBltActiveFlags);
                }

                if (mustFallBack)
                {
                    SlowClearCompute(pCmdBuffer,
                                     dstImage,
                                     isDepth ? depthLayout : stencilLayout,
                                     clearColor,
                                     subresFormat,
                                     range,
                                     trackBltActiveFlags,
                                     rectCount,
                                     boxes.Data());
                }

                const ImageLayout           hiszValidLayout = gfx12Image.GetHiSZValidLayout(range.startSubres.plane);
                const DepthStencilHiSZState hiszState       =
                    ImageLayoutToDepthStencilHiSZState(hiszValidLayout, isDepth ? depthLayout : stencilLayout);

                if (gfx12Image.HasHiSZ() &&
                    ((isDepth && pHiSZ->HiZEnabled()) || ((isDepth == false) && pHiSZ->HiSEnabled())) &&
                    // Force expand HiSZ range if setHiSZStateMetadata is true.
                    ((hiszState == DepthStencilWithHiSZ) || setHiSZStateMetadata))
                {
                    constexpr uint8_t StencilWriteMaskFull = 0xFF;

                    if (fullBoxClear && (stencilWriteMask == StencilWriteMaskFull))
                    {
                        // If full clear, fix up HiZ/HiS based on clear value.
                        FixupHiSZWithClearValue(pCmdBuffer, dstImage, range, depth, stencil, trackBltActiveFlags);
                    }
                    else
                    {
                        // If partial clear, fix up HiZ/HiS with full range.
                        ExpandHiSZWithFullRange(pCmdBuffer, dstImage, range, trackBltActiveFlags);
                    }
                }
            }
        }

        if (setHiSZStateMetadata)
        {
            // Expand the other plane so can safely re-enable HiSZ.
            if (pRanges[rangeIdx].numPlanes == 1)
            {
                SubresRange range = pRanges[rangeIdx];
                range.startSubres.plane = (range.startSubres.plane == 0) ? 1 : 0;

                // Note: This is only necessary if both HiZ and HiS are enabled.
                if (((range.startSubres.plane == 0) && pHiSZ->HiZEnabled()) ||
                    ((range.startSubres.plane == 1) && pHiSZ->HiSEnabled()))
                {
                    ExpandHiSZWithFullRange(pCmdBuffer, dstImage, range, trackBltActiveFlags);
                }
            }

            Pm4Predicate pktPredicate = Pm4Predicate(pCmdBuffer->GetPacketPredicate());
            auto* const  pCmdStream   = static_cast<CmdStream*>(pCmdBuffer->GetMainCmdStream());
            uint32*      pCmdSpace    = pCmdStream->ReserveCommands();

            pCmdSpace = gfx12Image.UpdateHiSZStateMetaData(pRanges[rangeIdx], true, pktPredicate,
                                                           pCmdBuffer->GetEngineType(), pCmdSpace);

            pCmdStream->CommitCommands(pCmdSpace);
        }
    }

    if (needComputeSync || needHiSZExpandSyncForGfxClear)
    {
        AcquireReleaseInfo acqRelInfo  = {};
        acqRelInfo.srcGlobalStageMask  = PipelineStageCs;
        acqRelInfo.dstGlobalStageMask  = PipelineStageDsTarget;
        acqRelInfo.srcGlobalAccessMask = CoherShader;
        acqRelInfo.dstGlobalAccessMask = CoherDepthStencilTarget;
        acqRelInfo.reason              = Developer::BarrierReasonPostComputeDepthStencilClear;

        pCmdBuffer->CmdReleaseThenAcquire(acqRelInfo);
    }
}

// Stuff ClearHiSZ knows but ClearImageCs doesn't know. We need to pass it through to the callback below.
struct ClearHiSZSrdContext
{
    HiSZType hiSZType;
};

// =====================================================================================================================
// Make a special writeable FMask image SRD which covers the entire clear range.
static void ClearHiSZCreateSrdCallback(
    const GfxDevice&   device,
    const Pal::Image&  image,
    const SubresRange& viewRange,
    const void*        pContext,
    void*              pSrd,      // [out] Place the image SRD here.
    Extent3d*          pExtent)   // [out] Fill this out with the maximum extent of the start subresource.
{
    PAL_ASSERT(pContext != nullptr);
    const auto& context     = *static_cast<const ClearHiSZSrdContext*>(pContext);
    const auto& gfx12Image  = *static_cast<const Image*>(image.GetGfxImage());
    const auto& gfx12Device = static_cast<const Device&>(device);

    // The ClearImageCs shaders always use "raw" formats, where PAL treats the image as a bit-packed unit format.
    // For example, HiZ normally has two 16-bit components (min, max) but we'd use X32_Unit for raw copies.
    // It should be legal to always do raw clears of the HiZ/HiS surfaces due to distributed compression.
    const SwizzledFormat rawFormat =
    {
        (context.hiSZType == HiSZType::HiZ) ? ChNumFormat::X32_Uint : ChNumFormat::X16_Uint,
        {{{ ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One }}}
    };

    gfx12Device.CreateHiSZViewSrds(gfx12Image, viewRange, rawFormat, context.hiSZType, pSrd);

    *pExtent = gfx12Image.GetHiSZ()->GetUnalignedExtent(viewRange.startSubres.mipLevel);
}

// =====================================================================================================================
// Reference from SlowClearCompute.
// Builds commands to clear a range of HiSZ surface to the given clear value using a compute shader.
void RsrcProcMgr::ClearHiSZ(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       image,
    const SubresRange& clearRange,
    HiSZType           hiSZType,
    uint32             clearValue,
    bool               trackBltActiveFlags
    ) const
{
    PAL_ASSERT(clearRange.numPlanes == 1);

    const uint32 fragments = image.Parent()->GetImageCreateInfo().fragments;

    // Ask for a typical 2D image slow clear with a 8x8 thread pattern. The only odd parts are that it must use HiS/Z
    // views and that HiS/Z can be MSAA but the samples map to different abstract pixel locations!
    ClearHiSZSrdContext context;
    context.hiSZType = hiSZType;

    ClearImageCsInfo info = {};
    info.clearFragments = fragments;
    info.pipelineEnum   = (fragments == 1) ? RpmComputePipeline::ClearImage
                                           : RpmComputePipeline::ClearImageMsaaSampleMajor;

    // See RsrcProcMgr::SlowClearCompute for the full details on why the SampleMajor shader requires different shapes.
    switch (fragments)
    {
    case 1:
        info.groupShape = {8, 8, 1};
        break;
    case 2:
        info.groupShape = {8, 4, 1};
        break;
    case 4:
        info.groupShape = {4, 4, 1};
        break;
    case 8:
        info.groupShape = {4, 2, 1};
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    info.packedColor[0] = clearValue; // HiZ/HiS will be cleared with raw formats X32_Uint/X16_Uint.
    info.pSrdCallback   = ClearHiSZCreateSrdCallback;
    info.pSrdContext    = &context;

    // Wrap the clear dispatches with a save/restore pair since ClearImageCs doesn't do that itself.
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
    ClearImageCs(pCmdBuffer, info, *image.Parent(), clearRange, 0, nullptr);
    pCmdBuffer->CmdRestoreComputeStateInternal(ComputeStatePipelineAndUserData, trackBltActiveFlags);
}

// CompSetting is a "helper" enum used in the CB's algorithm for deriving an ideal SPI_SHADER_EX_FORMAT
enum class CompSetting : uint32
{
    Invalid,
    OneCompRed,
    OneCompAlpha,
    TwoCompAlphaRed,
    TwoCompGreenRed
};

// =====================================================================================================================
// This method implements the helper function called CompSetting() for the shader export mode derivation algorithm
static CompSetting ComputeCompSetting(
    ColorFormat    hwColorFmt,
    SwizzledFormat format)
{
    CompSetting       compSetting = CompSetting::Invalid;
    const SurfaceSwap surfSwap    = Formats::Gfx12::ColorCompSwap(format);

    switch (hwColorFmt)
    {
    case COLOR_8:
    case COLOR_16:
    case COLOR_32:
        if (surfSwap == SWAP_STD)
        {
            compSetting = CompSetting::OneCompRed;
        }
        else if (surfSwap == SWAP_ALT_REV)
        {
            compSetting = CompSetting::OneCompAlpha;
        }
        break;
    case COLOR_8_8:
    case COLOR_16_16:
    case COLOR_32_32:
        if ((surfSwap == SWAP_STD) || (surfSwap == SWAP_STD_REV))
        {
            compSetting = CompSetting::TwoCompGreenRed;
        }
        else if ((surfSwap == SWAP_ALT) || (surfSwap == SWAP_ALT_REV))
        {
            compSetting = CompSetting::TwoCompAlphaRed;
        }
        break;
    default:
        compSetting = CompSetting::Invalid;
        break;
    }

    return compSetting;
}

// =====================================================================================================================
// Derives the hardware pixel shader export format for a particular RT view slot.  Value should be used to determine
// programming for SPI_SHADER_COL_FORMAT.
//
//
// Currently, we always use the default setting as specified in the spreadsheet, ignoring the optional settings.
const SPI_SHADER_EX_FORMAT RsrcProcMgr::DeterminePsExportFmt(
    SwizzledFormat format,
    bool           blendEnabled,
    bool           shaderExportsAlpha,
    bool           blendSrcAlphaToColor,
    bool           enableAlphaToCoverage
    ) const
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    const bool isUnorm = Formats::IsUnorm(format.format);
    const bool isSnorm = Formats::IsSnorm(format.format);
    const bool isFloat = Formats::IsFloat(format.format);
    const bool isUint  = Formats::IsUint(format.format);
    const bool isSint  = Formats::IsSint(format.format);
    const bool isSrgb  = Formats::IsSrgb(format.format);

    const uint32 maxCompSize = Formats::MaxComponentBitCount(format.format);

    const ColorFormat hwColorFmt  = HwColorFmt(format.format);
    const CompSetting compSetting = ComputeCompSetting(hwColorFmt, format);

    const bool hasAlpha = Formats::HasAlpha(format);
    const bool isDepth  = ((hwColorFmt == COLOR_8_24) ||
                           (hwColorFmt == COLOR_24_8) ||
                           (hwColorFmt == COLOR_X24_8_32_FLOAT));

    const bool alphaExport = (shaderExportsAlpha && (hasAlpha || blendSrcAlphaToColor || enableAlphaToCoverage));

    // Start by assuming SPI_FORMAT_ZERO (no exports).
    SPI_SHADER_EX_FORMAT spiShaderExFormat = SPI_SHADER_ZERO;

    if ((compSetting == CompSetting::OneCompRed) &&
        (alphaExport == false)                   &&
        (isSrgb == false)                        &&
        ((chipProps.gfx9.rbPlus == 0) || (maxCompSize == 32)))
    {
        // When RBPlus is enalbed, R8-UNORM and R16 UNORM shouldn't use SPI_SHADER_32_R, instead SPI_SHADER_FP16_ABGR
        // and SPI_SHADER_UNORM16_ABGR should be used for 2X exporting performance.
        // This setting is invalid in some cases when CB_COLOR_CONTROL.DEGAMMA_ENABLE is set, but PAL never uses that
        // legacy bit.
        spiShaderExFormat = SPI_SHADER_32_R;
    }
    else if (((isUnorm || isSnorm) && (maxCompSize <= 10)) ||
             ((isFloat           ) && (maxCompSize <= 16)) ||
             ((isSrgb            ) && (maxCompSize == 8)))
    {
        spiShaderExFormat = SPI_SHADER_FP16_ABGR;
    }
    else if (isSint && (maxCompSize <= 16) && (enableAlphaToCoverage == false))
    {
        // 8bpp SINT is supposed to be use SPI_SHADER_SINT16_ABGR per HW document
        spiShaderExFormat = SPI_SHADER_SINT16_ABGR;
    }
    else if (isSnorm && (maxCompSize == 16) && (blendEnabled == false))
    {
        spiShaderExFormat = SPI_SHADER_SNORM16_ABGR;
    }
    else if (isUint && (maxCompSize <= 16) && (enableAlphaToCoverage == false))
    {
        // 8bpp UINT is supposed to be use SPI_SHADER_UINT16_ABGR per HW document
        spiShaderExFormat = SPI_SHADER_UINT16_ABGR;
    }
    else if (isUnorm && (maxCompSize == 16) && (blendEnabled == false))
    {
        spiShaderExFormat = SPI_SHADER_UNORM16_ABGR;
    }
    else if ((((isUint  || isSint )                       ) ||
              ((isFloat           ) && (maxCompSize >  16)) ||
              ((isUnorm || isSnorm) && (maxCompSize == 16)))  &&
             ((compSetting == CompSetting::OneCompRed) ||
              (compSetting == CompSetting::OneCompAlpha) ||
              (compSetting == CompSetting::TwoCompAlphaRed)))
    {
        spiShaderExFormat = SPI_SHADER_32_AR;
    }
    else if ((((isUint  || isSint )                       ) ||
              ((isFloat           ) && (maxCompSize >  16)) ||
              ((isUnorm || isSnorm) && (maxCompSize == 16)))  &&
             (compSetting == CompSetting::TwoCompGreenRed) && (alphaExport == false))
    {
        spiShaderExFormat = SPI_SHADER_32_GR;
    }
    else if (((isUnorm || isSnorm) && (maxCompSize == 16)) ||
             ((isUint  || isSint )                       ) ||
             ((isFloat           ) && (maxCompSize >  16)) ||
             (isDepth))
    {
        spiShaderExFormat = SPI_SHADER_32_ABGR;
    }

    PAL_ASSERT(spiShaderExFormat != SPI_SHADER_ZERO);
    return spiShaderExFormat;
}

// =====================================================================================================================
// Returns true if there is graphics pipeline that can copy specified format.
const bool RsrcProcMgr::IsGfxPipelineForFormatSupported(
    SwizzledFormat format
    ) const
{
    const SPI_SHADER_EX_FORMAT exportFormat = DeterminePsExportFmt(format,
                                                                   false,  // Blend disabled
                                                                   true,   // Alpha is exported
                                                                   false,  // Blend Source Alpha disabled
                                                                   false); // Alpha-to-Coverage disabled

    return ExportStateMapping[exportFormat] >= 0;
}

// =====================================================================================================================
// Performs depth stencil clear using the graphics engine.
void RsrcProcMgr::DepthStencilClearGraphics(
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
    const Box*         pBox
    ) const
{
    PAL_ASSERT(dstImage.Parent()->IsDepthStencilTarget());

    const bool clearDepth   = TestAnyFlagSet(clearMask, ClearDepth);
    const bool clearStencil = TestAnyFlagSet(clearMask, ClearStencil);
    PAL_ASSERT(clearDepth || clearStencil); // How did we get here if there's nothing to clear!?

    const StencilRefMaskParams stencilRefMasks =
        { stencil, 0xFF, stencilWriteMask, 0x01, stencil, 0xFF, stencilWriteMask, 0x01, 0xFF };

    ViewportParams viewportInfo        = {};
    viewportInfo.count                 = 1;
    viewportInfo.viewports[0].origin   = PointOrigin::UpperLeft;
    viewportInfo.viewports[0].originX  = 0;
    viewportInfo.viewports[0].originY  = 0;
    viewportInfo.viewports[0].minDepth = 0.f;
    viewportInfo.viewports[0].maxDepth = 1.f;
    viewportInfo.horzClipRatio         = FLT_MAX;
    viewportInfo.horzDiscardRatio      = 1.0f;
    viewportInfo.vertClipRatio         = FLT_MAX;
    viewportInfo.vertDiscardRatio      = 1.0f;
    viewportInfo.depthRange            = DepthRange::ZeroToOne;

    ScissorRectParams scissorInfo    = {};
    scissorInfo.count                = 1;
    scissorInfo.scissors[0].offset.x = 0;
    scissorInfo.scissors[0].offset.y = 0;

    const ImageCreateInfo& createInfo = dstImage.Parent()->GetImageCreateInfo();

    DepthStencilViewInternalCreateInfo depthViewInfoInternal = {};
    depthViewInfoInternal.depthClearValue       = depth;
    depthViewInfoInternal.stencilClearValue     = stencil;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 876
    depthViewInfoInternal.flags.disableClientCompression =
        (createInfo.clientCompressionMode == ClientCompressionMode::Disable) ||
        (createInfo.clientCompressionMode == ClientCompressionMode::DisableClearOnly);
#endif

    DepthStencilViewCreateInfo depthViewInfo = {};
    depthViewInfo.pImage                     = dstImage.Parent();
    depthViewInfo.arraySize                  = 1;
    depthViewInfo.compressionMode            = CompressionMode::Default;
    depthViewInfo.flags.imageVaLocked        = 1;

    // Depth-stencil targets must be used on the universal engine.
    PAL_ASSERT((clearDepth   == false) || TestAnyFlagSet(depthLayout.engines,   LayoutUniversalEngine));
    PAL_ASSERT((clearStencil == false) || TestAnyFlagSet(stencilLayout.engines, LayoutUniversalEngine));

    BindTargetParams bindTargetsInfo          = {};
    bindTargetsInfo.depthTarget.depthLayout   = depthLayout;
    bindTargetsInfo.depthTarget.stencilLayout = stencilLayout;

    pCmdBuffer->CmdSaveGraphicsState();

    // Bind the depth expand state because it's just a full image quad and a zero PS (with no internal flags) which
    // is also what we need for the clear.
    PipelineBindParams bindParams = { PipelineBindPoint::Graphics, GetGfxPipeline(DepthExpand), InternalApiPsoHash, };
    if (clearDepth)
    {
        // Enable viewport clamping if depth values are in the [0, 1] range. This avoids writing expanded depth
        // when using a float depth format. DepthExpand pipeline disables clamping by default.
        const bool disableClamp = ((depth < 0.0f) || (depth > 1.0f));

        bindParams.gfxDynState.enable.depthClampMode = 1;
        bindParams.gfxDynState.depthClampMode        = disableClamp ? DepthClampMode::_None : DepthClampMode::Viewport;
    }
    pCmdBuffer->CmdBindPipeline(bindParams);
    pCmdBuffer->CmdBindMsaaState(GetMsaaState(dstImage.Parent()->GetImageCreateInfo().samples,
                                              dstImage.Parent()->GetImageCreateInfo().fragments));
    BindCommonGraphicsState(pCmdBuffer);
    pCmdBuffer->CmdSetStencilRefMasks(stencilRefMasks);

    // Select a depth/stencil state object for this clear:
    if (clearDepth && clearStencil)
    {
        pCmdBuffer->CmdBindDepthStencilState(m_pDepthStencilClearState);
    }
    else if (clearDepth)
    {
        pCmdBuffer->CmdBindDepthStencilState(m_pDepthClearState);
    }
    else if (clearStencil)
    {
        pCmdBuffer->CmdBindDepthStencilState(m_pStencilClearState);
    }

    // All mip levels share the same depth export value, so only need to do it once.
    RpmUtil::WriteVsZOut(pCmdBuffer, depth);

    // Box of partial clear is only valid when number of mip-map is equal to 1.
    PAL_ASSERT((boxCnt == 0) || ((pBox != nullptr) && (range.numMips == 1)));
    uint32 scissorCnt = (boxCnt > 0) ? boxCnt : 1;

    LinearAllocatorAuto<VirtualLinearAllocator> sliceAllocator(pCmdBuffer->Allocator(), false);
    const size_t depthStencilViewSize = m_pDevice->GetDepthStencilViewSize(nullptr);

    // Allocate two copies of DepthStencilViewSize and use it in ping-pong mode in the below loop.
    void* pDepthViewMem = PAL_MALLOC(depthStencilViewSize * 2, &sliceAllocator, AllocInternalTemp);

    if (pDepthViewMem == nullptr)
    {
        pCmdBuffer->NotifyAllocFailure();
    }
    else
    {
        void* pCurrDepthViewMem = pDepthViewMem;

        // Each mipmap level has to be fast-cleared individually because a depth target view can only be tied to a
        // single mipmap level of the destination Image.
        const uint32 lastMip = (range.startSubres.mipLevel + range.numMips - 1);
        for (depthViewInfo.mipLevel  = range.startSubres.mipLevel;
             depthViewInfo.mipLevel <= lastMip;
             ++depthViewInfo.mipLevel)
        {
            const SubresId subres = Subres(range.startSubres.plane, depthViewInfo.mipLevel, 0);
            const SubResourceInfo& subResInfo = *dstImage.Parent()->SubresourceInfo(subres);

            // All slices of the same mipmap level can re-use the same viewport and scissor state.
            viewportInfo.viewports[0].width  = static_cast<float>(subResInfo.extentTexels.width);
            viewportInfo.viewports[0].height = static_cast<float>(subResInfo.extentTexels.height);

            scissorInfo.scissors[0].extent.width  = subResInfo.extentTexels.width;
            scissorInfo.scissors[0].extent.height = subResInfo.extentTexels.height;

            pCmdBuffer->CmdSetViewports(viewportInfo);

            // Issue a clear draw for each slice of the current mip level.
            const uint32 lastSlice = (range.startSubres.arraySlice + range.numSlices - 1);
            for (depthViewInfo.baseArraySlice  = range.startSubres.arraySlice;
                 depthViewInfo.baseArraySlice <= lastSlice;
                 ++depthViewInfo.baseArraySlice)
            {
                IDepthStencilView* pDepthView = nullptr;

                Result result = m_pDevice->CreateDepthStencilView(depthViewInfo,
                                                                  depthViewInfoInternal,
                                                                  pCurrDepthViewMem,
                                                                  &pDepthView);
                PAL_ASSERT(result == Result::Success);

                // Bind the depth view for this mip and slice.
                bindTargetsInfo.depthTarget.pDepthStencilView = pDepthView;
                pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                for (uint32 i = 0; i < scissorCnt; i++)
                {
                    if (boxCnt > 0)
                    {
                        scissorInfo.scissors[0].offset.x      = pBox[i].offset.x;
                        scissorInfo.scissors[0].offset.y      = pBox[i].offset.y;
                        scissorInfo.scissors[0].extent.width  = pBox[i].extent.width;
                        scissorInfo.scissors[0].extent.height = pBox[i].extent.height;
                    }

                    pCmdBuffer->CmdSetScissorRects(scissorInfo);

                    // Draw a fullscreen quad.
                    pCmdBuffer->CmdDraw(0, 3, 0, 1, 0);
                }

                // Switch to the other copy of pDepthViewMem for next loop
                pCurrDepthViewMem = (pCurrDepthViewMem == pDepthViewMem)
                                    ? Util::VoidPtrInc(pDepthViewMem, depthStencilViewSize)
                                    : pDepthViewMem;
            } // End for each slice.
        } // End for each mip.
    }

    // Restore original command buffer state and destroy the depth/stencil state.
    pCmdBuffer->CmdRestoreGraphicsStateInternal(false);

    // Delete pDepthViewMem after restored to original DepthStencilView.
    PAL_SAFE_FREE(pDepthViewMem, &sliceAllocator);
}

// =====================================================================================================================
bool RsrcProcMgr::IsColorGfxClearPreferred(
    const Gfx12PalSettings& settings,
    const LinearClearDesc&  desc, // Only valid if linearClearSupported.
    bool                    linearClearSupported,
    bool                    clearAutoSync
    ) const
{
    // If AutoSync wasn't specified our barrier implementation assumes we'll always use a CS clear.
    if (clearAutoSync == false)
    {
        return false;
    }

    const AutoSyncClearPreferEngineOverride setting = settings.autoSyncClearPreferEngine;

    // Select CS clears if PreferEngineCompute, otherwise default to GFX clears.
    bool preferGfxClear = (setting != AutoSyncClearPreferEngineCompute);

    // This heuristic assumes that the linear clear path is available. If it isn't, assume CS is too slow.
    // Note that we punt on multi-mip clears for now. We need more data before we can update this heuristic.
    if (linearClearSupported && (setting == AutoSyncClearPreferEngineDefault) && (desc.clearRange.numMips == 1))
    {
        const gpusize scaledSize = desc.planeSize;

        if ((desc.swizzleMode == ADDR3_LINEAR) || (desc.swizzleMode == ADDR3_256B_2D))
        {
            // GFX clears are really slow with linear images and the 256B swizzle mode.
            preferGfxClear = false;
        }
        else if (desc.baseFormatBpp <= 32)
        {
            if ((desc.swizzleMode == ADDR3_256KB_2D) && (desc.compressedWrites == false))
            {
                // Once 256KB is bottlenecked by uncompressed DF traffic, GFX clears seem to be slightly better.
                preferGfxClear = (scaledSize > 80_MiB);
            }
            else
            {
                // In all other small BPP cases, CS is just as good as GFX or faster.
                preferGfxClear = false;
            }
        }
        else if (desc.baseFormatBpp == 64)
        {
            if ((desc.swizzleMode == ADDR3_4KB_2D))
            {
                // 64bpp images with 4KB tiles runs faster on CS.
                preferGfxClear = false;
            }
            else if ((desc.swizzleMode == ADDR3_64KB_2D) || (desc.swizzleMode == ADDR3_256KB_2D))
            {
                // GFX is much better at large tiles and tend to be faster than CS if the plane size is large enough.
                if (desc.compressedWrites == false)
                {
                    // Once we're bottlenecked by uncompressed DF traffic, GFX clears seem to be slightly better.
                    preferGfxClear = (scaledSize > 80_MiB);
                }
                else
                {
                    // Client compression is great at MSAA images but CS still wins if the image is single sampled.
                    preferGfxClear = (scaledSize > 4_MiB) && (desc.samples > 1);
                }
            }
        }
        else if (desc.baseFormatBpp == 128)
        {
            if (desc.swizzleMode == ADDR3_4KB_2D)
            {
                // 128bpp images with 4KB tiles runs faster on CS except for compressed 8xaa with a size over 4MB.
                preferGfxClear = desc.compressedWrites && (scaledSize > 4_MiB) && (desc.samples == 8);
            }
            else if ((desc.swizzleMode == ADDR3_64KB_2D) || (desc.swizzleMode == ADDR3_256KB_2D))
            {
                // GFX is much better at large tiles and tend to be faster than CS if the plane size is large enough.
                if (desc.compressedWrites == false)
                {
                    // Once we're bottlenecked by uncompressed DF traffic, GFX clears seem to be slightly better.
                    preferGfxClear = (scaledSize > 80_MiB);
                }
                else if (desc.samples <= 2)
                {
                    // GFX clears are pretty bad at 128bpp clears when samples <= 2.
                    preferGfxClear = false;
                }
                else if (desc.samples == 4)
                {
                    // Client compression is great at 4xaa but CS still wins if the image is small enough.
                    preferGfxClear = (scaledSize > 8_MiB);
                }
                else
                {
                    PAL_ASSERT(desc.samples == 8);

                    // Client compression is great at 8xaa but CS still wins if the image is small enough.
                    preferGfxClear = (scaledSize > 4_MiB);
                }
            }
        }
    }

    return preferGfxClear;
}

// =====================================================================================================================
bool RsrcProcMgr::IsDepthStencilGfxClearPreferred(
    bool clearAutoSync
    ) const
{
    bool preferGfxClear = false;

    if (clearAutoSync)
    {
        const Gfx12PalSettings& settings = GetGfx12Settings(m_pDevice->Parent());

        switch (settings.autoSyncClearPreferEngine)
        {
        case AutoSyncClearPreferEngineDefault:
            // TODO: Add PAL heuristic for default case
        case AutoSyncClearPreferEngineGraphics:
            preferGfxClear = true;
            break;
        case AutoSyncClearPreferEngineCompute:
            preferGfxClear = false;
            break;
        default:
            PAL_NEVER_CALLED();
            break;
        }
    }

    return preferGfxClear;
}

// =====================================================================================================================
// Given a subresource dimensions in blocks and a padding cutoff table, return the appropriate cutoff for that subres.
//
// If the subresource is too large its coordinates will be outside of the table. We just clamp to the boundary of the
// table, assuming that really large images behave similarly to the largest images that were profiled to make the table.
//
// Read the huge comment on the following function for background on this function (like why it uses CeilLog2).
template<uint32 N>
static uint8 LookupCutoff(
    uint32      blocksX,
    uint32      blocksY,
    const uint8 (*table)[N])
{
    const uint32 tableX = Min(CeilLog2(blocksX), N - 1);
    const uint32 tableY = Min(CeilLog2(blocksY), N - 1);

    return table[tableY][tableX];
}

#if PAL_BUILD_NAVI48
// =====================================================================================================================
// Returns true if the linear image clear shader should run faster than the fallback SlowClearCompute path on Navi48.
// Note that I will refer to SlowClearCompute as "the image-view clear" in this function because it uses an image SRD.
//
// The linear clear shader's strength is that it always fully utilizes all cache/DF bandwidth. In theory this makes it
// the fastest shader in all cases. However, it knows nothing about swizzle modes so it can't skip any padding bytes
// on the edges of the image. This means the linear shader must write extra memory in most cases! This extra work adds
// up and can seriously slow down the clear.
//
// The image-view clear shader's strength is that it understands swizzle modes. It writes exactly as much memory as
// the caller requested; all padding is ignored. Its weakness is that it writes using the image's native BPP.
// For example a clear of a X8_UINT image will write just 8 bits per thread. This causes performance to drop off
// significantly as the image BPP decreases.
//
// Both clear shaders were profiled on a square grid of 256x256 image sizes. Each image's extent (in texels) was picked
// to keep byte size of each clear constant across different BPPs. For example, the smallest image was always 4KB which
// is 64x64 at 8bpp or 32x32 at 32bpp. The largest image was always 256MB. Profiling covered all combinations of:
// pow2 BPPs, 2D image swizzle modes (including linear), sample count, distributed compression on/off, color or DS.
//
// The profiling data confirmed the core tradeoffs mentioned earlier. For most images, the primary BPP and padding
// tradeoffs dominate. At first glance we might expect that we should always use the linear shader if BPP < 128 but
// it's not quite that simple. For example the image-view shader runs twice as fast as the linear shader on a 32x2048,
// 32bpp, 1xaa, SW_256KB_2D image because 87% of that image is padding. Assuming both shaders use a constant amount of
// cache bandwidth, we can always find some padding threshold past which the linear shader runs slower than the
// image-view shader no matter how small the BPP is. Thus the next step in our heuristic is to compute the clear's
// padding percentage and compare that against a threshold derived from profiling data; if we're under the padding
// threshold we expect that the linear shader will be faster.
//
// Testing shows that linear, 256B, and 4KB swizzles have so little padding that they don't need a padding threshold.
// For these swizzles, linear clears are always faster. That leaves just the 64KB and 256KB modes for consideration and
// they have roughly two cases:
//   1. The clear paths have very different performance so a simple, global padding threshold is sufficient.
//      For example, all 32 BPP, 256KB images should use the linear shader if they have less than 74% padding [A].
//   2. The clear paths have areas of significant performance overlap. Complex real-world interactions cause the actual
//      performance of both shaders to change depending on the image's width and height values. We must select unique
//      padding thresholds depending on our image's BPP, swizzle mode, width, and height.
//
// Note that there is one exception to both cases: small images. Both shaders are very competitive at small sizes but
// the linear clear shader tends to be slightly faster. This is especially true for non-DSV images, where we can ignore
// the overhead of clearing HiS/HiZ metadata. From the data, we should only apply the above rules if:
//  - 64KB:  the image is a DSV or has more than 5 swizzle macroblocks [B].
//  - 256KB: the image is a DSV or has more than 1 swizzle macroblocks [C].
//
// Solving case 2 is fairly difficult. What worked best in the end was dividing the space of image extents up into bins
// of similar extents where the image-view shader performs similarly. Practical testing shows that setting the bin size
// equal to the image swizzle macroblock size works well. This makes sense because adding another row or column of
// macroblocks will change the way the image-view shader walks though memory. In other words images with the same memory
// layout should get the same performance characteristics out of the image-view shader.
//
// However, a simple grid of bins requires a lot of cutoffs! To cover all 128-BPP image sizes up to 4096x4096 (256MB)
// the ADDR3_64KB_2D swizzle mode requires a grid of 64x64 cutoffs. Even if we use one byte per cutoff PAL will still
// need a few KBs of constants for this heuristic. That's not too bad, but we can do better without reducing accuracy.
// I realized that once the image sizes get big enough the cutoffs are very similar between neighboring bins. This
// makes some sense, going from 128x128 to 256x128 is a huge change compared to 1024x1024 to 1152x1024. We can capture
// this mathematically if we toss a CeilLog2() into the bin indexing logic. For example, the width/X dimension maps to:
//   binX = 0: All images with width = 1 block
//   binX = 1: All images with width = 2 blocks
//   binX = 2: All images with width = 4 blocks
//   binX = 3: All images with width = 8 blocks
//   binX = 4: All images with width = 16 blocks
//   ...
// This brings the ADDR3_64KB_2D cutoff tables down to just 7x7 = 49 cutoffs!
//
// This log2 table heuristic works well on single-sampled images and MSAA images. In fact, for any given swizzle mode
// and BPP value we can use a cutoff table derived from single-sampled profiling and just use it with any sample count!
// When we convert the subresource size into macroblock counts we're implicitly accounting for the samples so the
// heuristic never even needs to look at the sample count.
//
// Finally, we must account for distributed compression because enabling or disabling compression has a huge impact
// on clear performance. With small BPPs or small block sizes the previously mentioned large difference in shader
// performance is still the dominant factor; these padding cutoffs do not depend on our compression mode. However,
// with 64KB or 256KB macrotiles and BPP >= 64 there is a significant performance difference. We handle this by simply
// picking the padding cutoffs twice: once with compression enabled and once with it disabled.
//
// Note that all cutoffs mentioned here were automatically fit by python scripts. The lower-bound size cutoff is fairly
// easy to eyeball manually but the percentage cutoffs are too sensitive to pick by hand.
bool RsrcProcMgr::ExpectLinearIsFasterNavi48(
    const LinearClearDesc& desc,
    const SubResourceInfo& subresInfo)
{
    if ((desc.swizzleMode != ADDR3_64KB_2D) && (desc.swizzleMode != ADDR3_256KB_2D))
    {
        // Testing shows that linear, 256B, and 4KB swizzles have so little padding that they don't need a padding
        // threshold. In other words, if this image doesn't use a 64KB or 256KB mode we can immediately return true.
        // (This is really just here to avoid triggering the block size asserts on linear images...)
        return true;
    }

    // Compute the ideal clear size: the number of bytes a perfect clear shader would write.
    // Note that bitsPerTexel is the size of a single element for a single sample, not a texel (yep...)
    const Extent3d& extentElems    = subresInfo.extentElements;
    const gpusize  clearWindowSize = (gpusize(extentElems.width) * extentElems.height * extentElems.depth *
                                      desc.samples * (subresInfo.bitsPerTexel / 8));

    // If this triggers there's a bug in the clearWindowSize calculation. Maybe a units issue with bitsPerTexel.
    PAL_ASSERT(clearWindowSize <= desc.planeSize);

    // The linear shader writes planeSize bytes so this gives our linear shader performance rating.
    const double paddingPct = (double(desc.planeSize - clearWindowSize) / desc.planeSize) * 100.0;

    // To simplify the implementation we assume we always want the linear shader by default. Setting the padding cutoff
    // to 100% here is basically saying: "run the linear shader no matter how much padding it writes!"
    uint8 paddingCutoffPct = 100;

    // For large block sizes (64KB+), the padding gets large enough that we actually need to use the image-view path or
    // we'll regress some cases. The following if-statements implement paths #2 and #3 from the header comment.
    const uint32 blocksX     = subresInfo.actualExtentElements.width  / subresInfo.blockSize.width;
    const uint32 blocksY     = subresInfo.actualExtentElements.height / subresInfo.blockSize.height;
    const uint32 totalBlocks = blocksX * blocksY;

     // Addrlib should guarantee that the actual subresource size is block-aligned.
    PAL_ASSERT(subresInfo.actualExtentElements.width  % subresInfo.blockSize.width  == 0);
    PAL_ASSERT(subresInfo.actualExtentElements.height % subresInfo.blockSize.height == 0);

    if ((desc.swizzleMode == ADDR3_64KB_2D) &&
        (desc.isDepthStencil || (totalBlocks > 5))) // See reference [B] above.
    {
        // The bottom right corners of these tables go out to a max image size of 256 MiB, which is the first square
        // power of two over Navi48's MALL size of 64 MiB. We don't have a compile-time log2 function so this static
        // assert verifies that the hand-written bin count is correct.
        constexpr uint32 NumBinsSw64Kb = 7;
        static_assert((1u << (NumBinsSw64Kb - 1)) * (1u << (NumBinsSw64Kb - 1)) * 64_KiB == 256_MiB);

        constexpr uint8 CutoffTableSw64Kb64BppCmprOff[NumBinsSw64Kb][NumBinsSw64Kb] =
        {
            { 100, 100, 100, 57,  31,  28,  26 },
            { 100, 100, 49,  100, 100, 100, 38 },
            { 100, 47,  100, 100, 100, 100, 18 },
            { 53,  100, 100, 100, 100, 100, 11 },
            { 31,  100, 100, 100, 100, 12,  4  },
            { 28,  100, 100, 18,  12,  8,   2  },
            { 26,  38,  17,  8,   4,   2,   1  }
        };
        constexpr uint8 CutoffTableSw64Kb128BppCmprOn[NumBinsSw64Kb][NumBinsSw64Kb] =
        {
            { 100, 100, 100, 36, 31, 28, 26  },
            { 100, 100, 31,  21, 25, 27, 26  },
            { 100, 37,  25,  16, 19, 27, 100 },
            { 33,  23,  14,  11, 13, 18, 100 },
            { 31,  19,  12,  9,  10, 12, 100 },
            { 28,  16,  11,  8,  5,  5,  100 },
            { 2,   14,  10,  6,  3,  3,  100 }
        };
        constexpr uint8 CutoffTableSw64Kb128BppCmprOff[NumBinsSw64Kb][NumBinsSw64Kb] =
        {
            { 100, 100, 100, 36, 31, 28, 26 },
            { 100, 100, 28,  23, 27, 28, 26 },
            { 100, 37,  19,  16, 19, 26, 12 },
            { 36,  23,  15,  11, 13, 8,  6  },
            { 31,  19,  11,  9,  10, 4,  3  },
            { 28,  16,  11,  10, 6,  1,  1  },
            { 2,   14,  10,  5,  3,  1,  1  }
        };

        switch (desc.baseFormatBpp)
        {
        case 8:
            paddingCutoffPct = 100;
            break;
        case 16:
            paddingCutoffPct = 87;
            break;
        case 32:
            paddingCutoffPct = 75;
            break;
        case 64:
            if (desc.compressedWrites)
            {
                paddingCutoffPct = 45;
            }
            else
            {
                paddingCutoffPct = LookupCutoff(blocksX, blocksY, CutoffTableSw64Kb64BppCmprOff);
            }
            break;
        case 128:
            paddingCutoffPct = LookupCutoff(blocksX, blocksY, desc.compressedWrites ? CutoffTableSw64Kb128BppCmprOn
                                                                                    : CutoffTableSw64Kb128BppCmprOff);
            break;
        default:
            PAL_ASSERT_ALWAYS();
            break;
        }
    }
    else if ((desc.swizzleMode == ADDR3_256KB_2D) &&
             (desc.isDepthStencil || (totalBlocks > 1))) // See reference [C] above.
    {
        // The bottom right corners of these tables go out to a max image size of 256 MiB, which is the first square
        // power of two over Navi48's MALL size of 64 MiB. We don't have a compile-time log2 function so this static
        // assert verifies that the hand-written bin count is correct.
        constexpr uint32 NumBinsSw256Kb = 6;
        static_assert((1u << (NumBinsSw256Kb - 1)) * (1u << (NumBinsSw256Kb - 1)) * 256_KiB == 256_MiB);

        constexpr uint8 CutoffTableSw256Kb64BppCmprOff[NumBinsSw256Kb][NumBinsSw256Kb] =
        {
            { 100, 49, 46, 43, 42, 40 },
            { 49,  44, 42, 41, 40, 17 },
            { 43,  42, 41, 39, 17, 8  },
            { 44,  41, 39, 31, 9,  4  },
            { 41,  39, 16, 8,  4,  3  },
            { 40,  11, 8,  3,  2,  3  }
        };
        constexpr uint8 CutoffTableSw256Kb128BppCmprOn[NumBinsSw256Kb][NumBinsSw256Kb] =
        {
            { 100, 34, 21, 22, 30, 28  },
            { 28,  12, 15, 16, 26, 32  },
            { 23,  14, 11, 13, 20, 27  },
            { 23,  9,  8,  10, 13, 21  },
            { 19,  10, 9,  5,  7,  100 },
            { 14,  11, 6,  4,  5,  100 }
        };
        constexpr uint8 CutoffTableSw256Kb128BppCmprOff[NumBinsSw256Kb][NumBinsSw256Kb] =
        {
            { 100, 31, 19, 25, 30, 28 },
            { 28,  17, 15, 16, 22, 12 },
            { 23,  14, 10, 11, 8,  6  },
            { 23,  10, 8,  5,  3,  2  },
            { 19,  9,  6,  3,  1,  3  },
            { 13,  5,  5,  2,  2,  3  }
        };

        switch (desc.baseFormatBpp)
        {
        case 8:
            paddingCutoffPct = 93;
            break;
        case 16:
            paddingCutoffPct = 87;
            break;
        case 32:
            paddingCutoffPct = 74; // See reference [A] above.
            break;
        case 64:
            if (desc.compressedWrites)
            {
                paddingCutoffPct = 44;
            }
            else
            {
                paddingCutoffPct = LookupCutoff(blocksX, blocksY, CutoffTableSw256Kb64BppCmprOff);
            }
            break;
        case 128:
            paddingCutoffPct = LookupCutoff(blocksX, blocksY, desc.compressedWrites ? CutoffTableSw256Kb128BppCmprOn
                                                                                    : CutoffTableSw256Kb128BppCmprOff);
            break;
        default:
            PAL_ASSERT_ALWAYS();
            break;
        }
    }

    return (paddingPct < double(paddingCutoffPct));
}
#endif

// =====================================================================================================================
// Returns true if it's legal to call TryLinearImageClear on this image. Note that you only need to call this on the
// first SubresRange in the client's array, that makes this a whole-image check and not a per-range check.
bool RsrcProcMgr::LinearClearSupportsImage(
    const Pal::Image& dstImage,
    const ClearColor& color,
    SubresRange       firstRange,
    uint32            boxCount,
    const Box*        pBoxes)
{
    // The clear functions say this in their interface comments:
    //   "If any Boxes have been specified, all subresource ranges must contain a single, identical mip level."
    // Thus if boxes were specified we have a guarantee that all cleared subresources have the same mip level and thus
    // the same x/y/z extent. If no boxes were specified then by definition we're clearing whole mips. Thus we only
    // need to call BoxesCoverWholeExtent once using the first SubresRange.
    const ImageCreateInfo& createInfo = dstImage.GetImageCreateInfo();
    const SubResourceInfo& subresInfo = *dstImage.SubresourceInfo(firstRange.startSubres);
    const bool clearBoxCoversWholeMip = RsrcProcMgr::BoxesCoverWholeExtent(subresInfo.extentTexels, boxCount, pBoxes);

    // This path has many restrictions.
    // - This path can't handle boxes. We can only continue if the only box covers the entire mip level.
    // - This path can't handle disabled channels either. Doing read-modify-writes will kill performance.
    // - YuvPlanar scares me...
    // These will be relaxed in future commits:
    // - Only support one array subresource per plane. We need more info from addrlib to relax this.
    // - Only support 2D images, other types haven't been profiled and tuned yet.
    return (clearBoxCoversWholeMip                                            &&
            (color.disabledChannelMask == 0)                                  &&
            (Formats::IsYuvPlanar(createInfo.swizzledFormat.format) == false) &&
            (createInfo.arraySize == 1)                                       &&
            (createInfo.imageType == ImageType::Tex2d));
}

// =====================================================================================================================
// Returns true if it's legal to call TryLinearImageClear with the filled out pDesc.
bool RsrcProcMgr::FillLinearClearDesc(
    const Pal::Image& dstImage,
    SubresRange       clearRange,
    SwizzledFormat    baseFormat,
    LinearClearDesc*  pDesc
    ) const
{
    uint32 baseFormatBpp = Formats::BitsPerPixel(baseFormat.format);

    if ((baseFormatBpp == 16) && Formats::IsYuvPacked(baseFormat.format))
    {
        baseFormatBpp = 32;
    }

    // Additional correctness requirements now that we know our clearRange and baseFormatBpp:
    // - Every thread in FillMem128Bit writes 128 bits so the clear pattern size (bpp) must be a power of two.
    // - Currently we only support clearing full mip chains.
    const ImageCreateInfo& createInfo = dstImage.GetImageCreateInfo();

    const bool supported = IsPowerOfTwo(baseFormatBpp) && (clearRange.numMips == createInfo.mipLevels);

    if (supported)
    {
        // Note that the "arraySize == 1" check in LinearClearSupportsImage and the "numMips == mipLevels" check just
        // above imply that clearRange must start at mip 0 and slice 0, it would be illegal to pass us anything else.
        // We require this for our full-plane clears but there should be no need to check it directly.
        PAL_ASSERT((clearRange.startSubres.mipLevel == 0) && (clearRange.startSubres.arraySlice == 0));

        pDesc->clearRange    = clearRange;
        pDesc->baseFormat    = baseFormat;
        pDesc->baseFormatBpp = baseFormatBpp;

        const Image& gfx12Image = static_cast<const Image&>(*dstImage.GetGfxImage());

        pDesc->samples     = createInfo.samples;
        pDesc->planeAddr   = gfx12Image.GetMipAddr(clearRange.startSubres, false);
        pDesc->planeSize   = gfx12Image.GetAddrOutput(clearRange.startSubres).surfSize;
        pDesc->swizzleMode = gfx12Image.GetFinalSwizzleMode(clearRange.startSubres);

        // Distributed compression has a huge impact on clear speed. This uses the same logic as CreateImageViewSrds
        // to determine if the clear should use compressed writes. The destination memory is an image, the fact that
        // we're using a buffer view to write to it is irrelevant. It must follow the image compression logic.
        const Device& gfx12Device     = static_cast<const Device&>(*m_pDevice);
        auto          compressionMode = static_cast<CompressionMode>(gfx12Device.Settings().imageViewCompressionMode);

        if (compressionMode == CompressionMode::Default)
        {
            compressionMode = gfx12Device.GetImageViewCompressionMode(CompressionMode::Default,
                                                                      createInfo.compressionMode,
                                                                      dstImage.GetBoundGpuMemory().Memory());
        }

        // CreateImageViewSrds enables write compression for Default and ReadEnableWriteEnable.
        pDesc->compressionMode  = compressionMode;
        pDesc->compressedWrites = ((compressionMode == CompressionMode::Default) ||
                                   (compressionMode == CompressionMode::ReadEnableWriteEnable));
        pDesc->isDepthStencil   = dstImage.IsDepthStencilTarget();
    }

    return supported;
}

// =====================================================================================================================
// Think of this as an alternative to SlowClearCompute which runs at maximum bandwidth no matter what your format is.
// The catch is that it just blasts your entire image memory using linear buffer writes. This breaks a number of basic
// assumptions about how RPM's blits work so it can be tricky to understand when it's safe to run this shader and also
// when this shader will be slower than SlowClearCompute.
//
// For instance, this path must always write full "macrotiles" because the tiled swizzle modes XOR a hash into the
// addressing math. This scrambles the locations of texels within the macrotile. If we want a fast shader we have to
// ignore the complex addressing logic and blast the whole macrotile, padding included. This is why this shader can't
// support arbitrary boxes and why it seems slow for oddly shaped images (images with tons of padding).
//
// So you must call both LinearClearSupportsImage and FillLinearClearDesc and verify that they return true before
// you can call this function. Not doing so may result in corruption.
//
// This function implements performance heuristics which may skip the clear. If this function returns true, the caller
// must forward the clearRange to a generic clear fallback path like SlowClearCompute. If this function returns false
// then the full clearRange was cleared.
bool RsrcProcMgr::TryLinearImageClear(
    GfxCmdBuffer*           pCmdBuffer,
    const Pal::Image&       dstImage,
    const Gfx12PalSettings& gfx12Settings,
    const LinearClearDesc&  desc,
    const ClearColor&       color,
    bool                    trackBltActiveFlags
    ) const
{
    // PAL still splits ranges at the top level before we call into RPM. We rely on this behavior.
    PAL_ASSERT(desc.clearRange.numPlanes == 1);

    // Many cases are actually slower using the linear clear path; make sure this clear is actually faster.
    const RpmLinearClearMode clearMode = gfx12Settings.rpmLinearClearMode;
    bool                     doClear   = (clearMode == RpmLinearClearForceOn);
#if PAL_BUILD_NAVI48
    const bool               isNavi48  = IsNavi48(*m_pDevice->Parent());
#endif

    if (clearMode == RpmLinearClearDefault)
    {
        const SubResourceInfo& subresInfo = *dstImage.SubresourceInfo(desc.clearRange.startSubres);

#if PAL_BUILD_NAVI48
        if (isNavi48)
        {
            doClear = ExpectLinearIsFasterNavi48(desc, subresInfo);
        }
#endif
    }

    if (doClear)
    {
        const ImageCreateInfo& createInfo = dstImage.GetImageCreateInfo();

        // Pack the client's abstract clear color into the intended bit pattern using the same logic that we use in a
        // normal image view slow clear.
        uint32 packedColor[4] = {};
        RpmUtil::ConvertAndPackClearColor(color,
                                          createInfo.swizzledFormat,
                                          desc.baseFormat,
                                          nullptr,
                                          desc.clearRange.startSubres.plane,
                                          true,
                                          packedColor);

        // Now replicate the packed color until it fills the 128-bit (16-byte) clear pattern. All bpp value should be
        // a multiple of 8 so we can use byte addressing here, no need for bit manipulation.
        PAL_ASSERT(IsPow2Aligned(desc.baseFormatBpp, 8));
        const uint32 bytesPerPixel = desc.baseFormatBpp / 8;

        for (uint32 byteOffset = bytesPerPixel; byteOffset < sizeof(packedColor); byteOffset += bytesPerPixel)
        {
            memcpy(VoidPtrInc(packedColor, byteOffset), packedColor, bytesPerPixel);
        }

        // Doing the save and restore in this function will make more sense in the future when we'll loop over a subres
        // range and launch multiple memory fills.
        pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

        // For decently large images (>1 MiB), the best fill shader is determined by whether our clear color will get a
        // clear-to-constant encoding or a clear-to-single encoding. For clear-to-constant, all 16-byte-per-thread fill
        // shaders run at essentially the same speed and sustain max DF bandwidth out to at least 1 GiB. However, if we
        // get a clear-to-single encoding, fill speed will stick close to max DF bandwidth until the clear size hits 4x
        // the MALL size; as the size continues to increase perf quickly drops to about half the DF bandwidth. We can
        // partially fix this by setting the LLCNOALLOC flag which tells the MALL to commit our writes straight to
        // memory which gives us about 2/3s of the max DF bandwidth (because memory write efficiency is improved).
        //
        // To make things simple we'll tell all very large clears to use the NOALLOC flag, regardless of what kind of
        // clear code they get. Note that we don't want to check for exactly 4x the MALL size because the drop isn't
        // instant; fitting almost all of the image in the MALL is still better than NOALLOCing the whole image. The
        // cutoff size was determined by sweeping the clear size and plotting the intersection of both shaders.
        bool mallNoAlloc = false;

#if PAL_BUILD_NAVI48
        if (isNavi48)
        {
            mallNoAlloc = (desc.planeSize > 273_MiB);
        }
#endif

        // Clear-to-single fills will run very, very slightly faster at small image sizes (<1 MiB) if we go down the
        // 32-bit fill path instead of the 128-bit fill path. The perf difference is extremely small (nanoseconds)
        // so I wouldn't mind if someone wants to remove the CompressionMode params from CmdFillMemory but as long as
        // it's there we should use it. Clear-to-constant doesn't benefit but it's simpler to let it use 32-bit fills.
        //
        // There are two cases where we can safely use 32-bit fills:
        // 1. The image's bpp is <= 32 so the fill pattern always fits in 32 bits.
        // 2. The caller's clear color is repetitive. For example clears to all black/zero should use the 32-bit
        //    fill path. Even more interesting clear patterns can still go down here sometimes, imagine clearing to
        //    (255, 0, 255, 0) on a R16G16B16A16_UNORM format; both 32-bit halves of each texel use the same pattern.
        // Comparing the four components of the full 128-bit pattern detects all of these cases in one go.
        //
        // Note that CmdFillMemory does not implement the LLCNOALLOC optimization because those shaders are generic.
        // Rather than risk hurting performance on other hardware we just force the 128-bit fill path.
        if ((mallNoAlloc    == false)          &&
            (packedColor[0] == packedColor[1]) &&
            (packedColor[0] == packedColor[2]) &&
            (packedColor[0] == packedColor[3]))
        {
            CmdFillMemory(pCmdBuffer, false, false, desc.planeAddr, desc.planeSize, packedColor[0],
                          desc.compressionMode);
        }
        else
        {
            FillMem128Bit(pCmdBuffer, desc.compressionMode, desc.planeAddr, desc.planeSize, packedColor, mallNoAlloc);
        }

        pCmdBuffer->CmdRestoreComputeStateInternal(ComputeStatePipelineAndUserData, trackBltActiveFlags);
    }

    // We also need to return true if the caller needs to call their fallback path.
    return (doClear == false);
}

// =====================================================================================================================
// Builds commands to write a repeating 128-bit pattern to GPU memory. dstGpuVirtAddr must be 4-byte aligned and
// fillSize must be 16-byte aligned.
//
// This function does not save or restore the Command Buffer's state, that responsibility lies with the caller!
void RsrcProcMgr::FillMem128Bit(
    GfxCmdBuffer*   pCmdBuffer,
    CompressionMode compressionMode,
    gpusize         dstGpuVirtAddr,
    gpusize         fillSize,
    const uint32    data[4],
    bool            mallNoAlloc
    ) const
{
    constexpr uint32 PatternSize = sizeof(uint32) * 4;

    // The caller must align these values. In practice both conditions should always be true when filling image
    // subresources because all swizzle mode tiles must be aligned to at least 128 bits.
    PAL_ASSERT(IsPow2Aligned(dstGpuVirtAddr, sizeof(uint32)));
    PAL_ASSERT(IsPow2Aligned(fillSize,       PatternSize));

    BufferViewInfo bufferView = {};
    bufferView.stride                 = PatternSize;
    bufferView.swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
    bufferView.swizzledFormat.swizzle = { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };
    bufferView.compressionMode        = compressionMode;

    const Pal::ComputePipeline*const pPipeline = GetPipeline(mallNoAlloc ? RpmComputePipeline::Gfx12FillMem128bNoalloc
                                                                         : RpmComputePipeline::Gfx12FillMem128b);
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    const Pal::Device& device = *m_pDevice->Parent();

    // We split big fills up into multiple dispatches based on this limit. The hope is that this will improve
    // preemption QoS without hurting performance.
    constexpr gpusize FillSizeLimit = 256_MiB;

    for (gpusize fillOffset = 0; fillOffset < fillSize; fillOffset += FillSizeLimit)
    {
        const uint32 numBytes = uint32(Min(FillSizeLimit, (fillSize - fillOffset)));

        bufferView.gpuAddr = dstGpuVirtAddr + fillOffset;
        bufferView.range   = numBytes;

        // Gfx12FillMem128b has this optimized user-data layout:
        // [0-1]: The first half of the fill pattern.
        // [2-5]: The buffer view, gfx12 HW has 4-DW buffer views.
        // [6-7]: The second half of the fill pattern.
        PAL_ASSERT(device.ChipProperties().srdSizes.typedBufferView == (4 * sizeof(uint32)));

        constexpr uint32 NumUserData = 8;
        uint32 userData[NumUserData] = { data[0], data[1], 0, 0, 0, 0, data[2], data[3] };
        device.CreateTypedBufferViewSrds(1, &bufferView, &userData[2]);
        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 0, NumUserData, userData);

        // Issue a dispatch with one 128-bit write per thread.
        const uint32 minThreads   = numBytes / PatternSize;
        const uint32 threadGroups = RpmUtil::MinThreadGroups(minThreads, pPipeline->ThreadsPerGroup());
        pCmdBuffer->CmdDispatch({threadGroups, 1, 1}, {});
    }
}

} // namespace Gfx12
} // namespace Pal
