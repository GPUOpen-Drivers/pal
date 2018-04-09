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

#include "core/cmdAllocator.h"
#include "core/device.h"
#include "core/dmaCmdBuffer.h"
#include "core/image.h"
#include "palFormatInfo.h"
#include "palAutoBuffer.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
// Dummy function for catching illegal attempts to set user-data entries on a DMA command buffer.
static void PAL_STDCALL DummyCmdSetUserData(
    ICmdBuffer*,
    uint32,
    uint32,
    const uint32*)
{
    PAL_ASSERT_ALWAYS();
}

// =====================================================================================================================
DmaCmdBuffer::DmaCmdBuffer(
    Device*                    pDevice,
    const CmdBufferCreateInfo& createInfo,
    bool                       copyOverlapHazardSyncs)
    :
    CmdBuffer(*pDevice, createInfo, &m_cmdStream),
    m_pDevice(pDevice),
    m_cmdStream(pDevice, createInfo.pCmdAllocator, EngineTypeDma, SubQueueType::Primary, 0, 0, IsNested(), false),
    m_predMemEnabled(false),
    m_copyOverlapHazardSyncs(copyOverlapHazardSyncs),
    m_predMemAddress(0),
    m_pT2tEmbeddedGpuMemory(nullptr),
    m_t2tEmbeddedMemOffset(0)
{
    PAL_ASSERT(createInfo.queueType == QueueTypeDma);

    SwitchCmdSetUserDataFunc(PipelineBindPoint::Compute,  &DummyCmdSetUserData);
    SwitchCmdSetUserDataFunc(PipelineBindPoint::Graphics, &DummyCmdSetUserData);
}

// =====================================================================================================================
ImageType DmaCmdBuffer::GetImageType(
    const IImage&  image)
{
    const Image&    palImage  = static_cast<const Image&>(image);
    const GfxImage* pGfxImage = palImage.GetGfxImage();

    return pGfxImage->GetOverrideImageType();
}

// =====================================================================================================================
Result DmaCmdBuffer::Init(
    const CmdBufferInternalCreateInfo& internalInfo)
{
    Result result = CmdBuffer::Init(internalInfo);

    if (result == Result::Success)
    {
        result = m_cmdStream.Init();
    }

    return result;
}

// =====================================================================================================================
// Resets the command buffer's previous contents and state, then puts it into a building state allowing new commands
// to be recorded.
// Also starts command buffer dumping, if it is enabled.
Result DmaCmdBuffer::Begin(
    const CmdBufferBuildInfo& info)
{
    Result result = CmdBuffer::Begin(info);

#if PAL_ENABLE_PRINTS_ASSERTS
    if ((result == Result::Success) && IsDumpingEnabled())
    {
        char filename[MaxFilenameLength] = {};

        // filename is:  dmaxx_yyyyy, where "xx" is the number of universal command buffers that have been created so
        //               far (one based) and "yyyyy" is the number of times this command buffer has been begun (also
        //               one based).
        //
        // All streams associated with this command buffer are included in this one file.
        Snprintf(filename, MaxFilenameLength, "dma%02d_%05d", UniqueId(), NumBegun());
        OpenCmdBufDumpFile(&filename[0]);
    }
#endif

    return result;
}

// =====================================================================================================================
// Puts the command stream into a state that is ready for command building.
Result DmaCmdBuffer::BeginCommandStreams(
    CmdStreamBeginFlags cmdStreamFlags,
    bool                doReset)
{
    Result result = CmdBuffer::BeginCommandStreams(cmdStreamFlags, doReset);

    if (doReset)
    {
        m_cmdStream.Reset(nullptr, true);
    }

    if (result == Result::Success)
    {
        result = m_cmdStream.Begin(cmdStreamFlags, m_pMemAllocator);
    }

    return result;
}

// =====================================================================================================================
// Completes recording of a command buffer in the building state, making it executable.
// Also ends command buffer dumping, if it is enabled.
Result DmaCmdBuffer::End()
{
    Result result = CmdBuffer::End();

    if (result == Result::Success)
    {
        result = m_cmdStream.End();
    }

    if (result == Result::Success)
    {

#if PAL_ENABLE_PRINTS_ASSERTS
        if (IsDumpingEnabled() && DumpFile()->IsOpen())
        {
            if (m_pDevice->Settings().cmdBufDumpFormat == CmdBufDumpFormatBinaryHeaders)
            {
                const CmdBufferDumpFileHeader fileHeader =
                {
                    static_cast<uint32>(sizeof(CmdBufferDumpFileHeader)), // Structure size
                    1,                                                    // Header version
                    m_pDevice->ChipProperties().familyId,                 // ASIC family
                    m_pDevice->ChipProperties().deviceId,                 // Reserved, but use for PCI device ID
                    0                                                     // Reserved
                };
                DumpFile()->Write(&fileHeader, sizeof(fileHeader));

                CmdBufferListHeader listHeader =
                {
                    static_cast<uint32>(sizeof(CmdBufferListHeader)),   // Structure size
                    0,                                                  // Engine index
                    0                                                   // Number of command buffer chunks
                };

                listHeader.count = m_cmdStream.GetNumChunks();

                DumpFile()->Write(&listHeader, sizeof(listHeader));
            }

            DumpCmdStreamsToFile(DumpFile(), m_pDevice->Settings().cmdBufDumpFormat);
            DumpFile()->Close();
        }
#endif
    }

    return result;
}

// =====================================================================================================================
// Explicitly resets a command buffer, releasing any internal resources associated with it and putting it in the reset
// state.
Result DmaCmdBuffer::Reset(
    ICmdAllocator* pCmdAllocator,
    bool           returnGpuMemory)
{
    Result result = CmdBuffer::Reset(pCmdAllocator, returnGpuMemory);

    // The next scanline-based tile-to-tile copy will need to allocate a new embedded memory object
    m_pT2tEmbeddedGpuMemory = nullptr;

    m_cmdStream.Reset(static_cast<CmdAllocator*>(pCmdAllocator), returnGpuMemory);

    CmdSetPredication(nullptr,
                      0,
                      nullptr,
                      0,
                      static_cast<PredicateType>(0),
                      false,
                      false,
                      false);

    return result;
}

// =====================================================================================================================
// Inserts a barrier in the current command stream that can stall GPU execution, flush/invalidate caches, or decompress
// images before further, dependent work can continue in this command buffer.
//
// Note: the DMA engines execute strictly in order and don't use any caches so most barrier operations are meaningless.
void DmaCmdBuffer::CmdBarrier(
    const BarrierInfo& barrier)
{
    CmdBuffer::CmdBarrier(barrier);

    // Wait for the provided GPU events to be set.
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    // For certain versions of SDMA, some copy/write execution happens asynchronously and the driver is responsible
    // for synchronizing hazards when such copies overlap by inserting a NOP packet, which acts as a fence command.
    if (m_copyOverlapHazardSyncs && (barrier.pipePointWaitCount > 0))
    {
        pCmdSpace = WriteNops(pCmdSpace, 1);
    }

    for (uint32 i = 0; i < barrier.gpuEventWaitCount; i++)
    {
        PAL_ASSERT(barrier.ppGpuEvents[i] != nullptr);
        pCmdSpace = WriteWaitEventSet(static_cast<const GpuEvent&>(*barrier.ppGpuEvents[i]), pCmdSpace);
    }

    m_cmdStream.CommitCommands(pCmdSpace);

    bool initRequested = false;

    for (uint32 i = 0; i < barrier.transitionCount; i++)
    {
        const auto& imageInfo = barrier.pTransitions[i].imageInfo;

        if (imageInfo.pImage != nullptr)
        {
            // At least one usage must be specified for the old and new layouts.
            PAL_ASSERT((imageInfo.oldLayout.usages != 0) && (imageInfo.newLayout.usages != 0));

            // With the exception of a transition out of the uninitialized state, at least one queue type must be valid
            // for every layout.

            PAL_ASSERT(((imageInfo.oldLayout.usages == LayoutUninitializedTarget) ||
                        (imageInfo.oldLayout.engines != 0)) &&
                       (imageInfo.newLayout.engines != 0));

            // DMA supports metadata initialization transitions via GfxImage's InitMetadataFill function.
            if (TestAnyFlagSet(imageInfo.oldLayout.usages, LayoutUninitializedTarget))
            {
                const auto*  pImage = static_cast<const Pal::Image*>(imageInfo.pImage);

                // If the image is uninitialized, no other usages should be set.
                PAL_ASSERT(TestAnyFlagSet(imageInfo.oldLayout.usages, ~LayoutUninitializedTarget) == false);

#if PAL_ENABLE_PRINTS_ASSERTS
                const auto& engineProps  = m_pDevice->EngineProperties().perEngine[EngineTypeDma];
                const auto& createInfo   = imageInfo.pImage->GetImageCreateInfo();
                const bool  isWholeImage = pImage->IsFullSubResRange(imageInfo.subresRange);

                // DMA must support this barrier transition.
                PAL_ASSERT(engineProps.flags.supportsImageInitBarrier == 1);

                // By default, the entire image must be initialized in one go. Per-subres support can be requested using
                // an image flag as long as the queue supports it.
                PAL_ASSERT(isWholeImage || ((engineProps.flags.supportsImageInitPerSubresource == 1) &&
                                            (createInfo.flags.perSubresInit == 1)));
#endif

                const auto*const pGfxImage = pImage->GetGfxImage();

                if (pGfxImage != nullptr)
                {
                    pGfxImage->InitMetadataFill(this, imageInfo.subresRange);
                    initRequested = true;
                }
            }
        }
    }

    // If an initialization BLT occurred, an additional fence command is necessary to synchronize read/write hazards.
    if (m_copyOverlapHazardSyncs && initRequested)
    {
        pCmdSpace = m_cmdStream.ReserveCommands();
        pCmdSpace = WriteNops(pCmdSpace, 1);
        m_cmdStream.CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Executes one region's worth of memory-memory copy.
void DmaCmdBuffer::CopyMemoryRegion(
    const IGpuMemory&       srcGpuMemory,
    const IGpuMemory&       dstGpuMemory,
    const MemoryCopyRegion& region)
{
    gpusize srcGpuAddr      = srcGpuMemory.Desc().gpuVirtAddr + region.srcOffset;
    gpusize dstGpuAddr      = dstGpuMemory.Desc().gpuVirtAddr + region.dstOffset;
    gpusize bytesJustCopied = 0;
    gpusize bytesLeftToCopy = region.copySize;

    const DmaCopyFlags flags = DmaCopyFlags::None;

    while (bytesLeftToCopy > 0)
    {
        uint32* pCmdSpace = m_cmdStream.ReserveCommands();
        pCmdSpace = WriteCopyGpuMemoryCmd(srcGpuAddr,
                                          dstGpuAddr,
                                          bytesLeftToCopy,
                                          flags,
                                          pCmdSpace,
                                          &bytesJustCopied);
        m_cmdStream.CommitCommands(pCmdSpace);

        bytesLeftToCopy -= bytesJustCopied;
        srcGpuAddr      += bytesJustCopied;
        dstGpuAddr      += bytesJustCopied;
    }
}

// =====================================================================================================================
void DmaCmdBuffer::CmdCopyMemory(
    const IGpuMemory&       srcGpuMemory,
    const IGpuMemory&       dstGpuMemory,
    uint32                  regionCount,
    const MemoryCopyRegion* pRegions)
{
    uint32* pCmdSpace = nullptr;
    uint32* pPredCmd  = nullptr;

    if (m_predMemEnabled)
    {
        // Write the predication command, we will patch its predication size later.
        pCmdSpace = m_cmdStream.ReserveCommands();

        pPredCmd  = pCmdSpace;
        pCmdSpace = WritePredicateCmd(0, pCmdSpace);

        m_cmdStream.CommitCommands(pCmdSpace);
    }

    const GpuMemory& dstMemory = static_cast<const GpuMemory&>(dstGpuMemory);
    bool p2pBltInfoRequired    = m_pDevice->IsP2pBltWaRequired(dstMemory);

    uint32 newRegionCount = 0;
    if (p2pBltInfoRequired)
    {
        m_pDevice->P2pBltWaModifyRegionListMemory(dstMemory,
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
            m_pDevice->P2pBltWaModifyRegionListMemory(dstMemory,
                                                      regionCount,
                                                      pRegions,
                                                      &newRegionCount,
                                                      &newRegions[0],
                                                      &chunkAddrs[0]);
            regionCount = newRegionCount;
            pRegions    = &newRegions[0];

            P2pBltWaCopyBegin(&dstMemory, regionCount, &chunkAddrs[0]);
        }
        else
        {
            NotifyAllocFailure();
            p2pBltInfoRequired = false;
        }
    }

    // Splits up each region's copy size into chunks that the specific hardware can handle.
    for (uint32 rgnIdx = 0; rgnIdx < regionCount; rgnIdx++)
    {
        if (p2pBltInfoRequired)
        {
            P2pBltWaCopyNextRegion(chunkAddrs[rgnIdx]);
        }

        CopyMemoryRegion(srcGpuMemory, dstGpuMemory, pRegions[rgnIdx]);
    }

    if (p2pBltInfoRequired)
    {
        P2pBltWaCopyEnd();
    }

    if (m_predMemEnabled)
    {
        // We're done writing commands, patch the predicate command.
        PatchPredicateCmd(static_cast<size_t>(pCmdSpace - pPredCmd), pPredCmd);
    }
}

// =====================================================================================================================
void DmaCmdBuffer::CmdCopyTypedBuffer(
    const IGpuMemory&            srcGpuMemory,
    const IGpuMemory&            dstGpuMemory,
    uint32                       regionCount,
    const TypedBufferCopyRegion* pRegions)
{
    uint32* pCmdSpace = nullptr;
    uint32* pPredCmd = nullptr;

    if (m_predMemEnabled)
    {
        // Write the predication command, we will patch its predication size later.
        pCmdSpace = m_cmdStream.ReserveCommands();

        pPredCmd = pCmdSpace;
        pCmdSpace = WritePredicateCmd(0, pCmdSpace);

        m_cmdStream.CommitCommands(pCmdSpace);
    }

    for (uint32 rgnIdx = 0; rgnIdx < regionCount; rgnIdx++)
    {
        const auto& region = pRegions[rgnIdx];
        // Create a struct with info needed to write packet (cmd to be used is linear sub-window copy)
        DmaTypedBufferCopyInfo copyInfo = {};
        uint32 srcTexelScale = 1;
        uint32 dstTexelScale = 1;

        SetupDmaTypedBufferCopyInfo(srcGpuMemory, region.srcBuffer, &copyInfo.src, &srcTexelScale);
        SetupDmaTypedBufferCopyInfo(dstGpuMemory, region.dstBuffer, &copyInfo.dst, &dstTexelScale);

        // Perform checks b/w src and dst regions
        PAL_ASSERT(copyInfo.src.bytesPerElement == copyInfo.dst.bytesPerElement);
        PAL_ASSERT(srcTexelScale == dstTexelScale);

        // Set the rect dimensions
        copyInfo.copyExtent.width   = region.extent.width * srcTexelScale;
        copyInfo.copyExtent.height  = region.extent.height;
        copyInfo.copyExtent.depth   = region.extent.depth;

        // Write packet
        pCmdSpace = m_cmdStream.ReserveCommands();

        pCmdSpace = WriteCopyTypedBuffer(copyInfo, pCmdSpace);

        m_cmdStream.CommitCommands(pCmdSpace);
    }

    if (m_predMemEnabled)
    {
        // We're done writing commands, patch the predicate command.
        PatchPredicateCmd(static_cast<size_t>(pCmdSpace - pPredCmd), pPredCmd);
    }
}

// =====================================================================================================================
// Returns true if all the parameters specified by "appData" meet the specified alignment requirements
bool DmaCmdBuffer::IsAlignedForT2t(
    const Extent3d&  appData,
    const Extent3d&  alignment)
{
    return (IsPow2Aligned(appData.width,  alignment.width)  &&
            IsPow2Aligned(appData.height, alignment.height) &&
            IsPow2Aligned(appData.depth,  alignment.depth));
}

// =====================================================================================================================
// Returns true if all the parameters specified by "appData" meet the specified alignment requirements
bool DmaCmdBuffer::IsAlignedForT2t(
    const Offset3d&  appData,
    const Extent3d&  alignment)
{
    return (IsPow2Aligned(appData.x, alignment.width)  &&
            IsPow2Aligned(appData.y, alignment.height) &&
            IsPow2Aligned(appData.z, alignment.depth));
}

// =====================================================================================================================
// Allocates the embedded GPU memory chunk reserved for doing unaligned workarounds of mem-image and image-image
// copies.
void DmaCmdBuffer::AllocateEmbeddedT2tMemory()
{
    PAL_ASSERT(m_pT2tEmbeddedGpuMemory == nullptr);

    const uint32 embeddedDataLimit = GetEmbeddedDataLimit();

    CmdAllocateEmbeddedData(embeddedDataLimit,
        1, // SDMA can access dword aligned linear data.
        &m_pT2tEmbeddedGpuMemory,
        &m_t2tEmbeddedMemOffset);

    PAL_ASSERT(m_pT2tEmbeddedGpuMemory != nullptr);
}

// =====================================================================================================================
// Tiled image to tiled image copy, slice by slice, scanline by scanline.
void DmaCmdBuffer::WriteCopyImageTiledToTiledCmdScanlineCopy(
    const DmaImageCopyInfo& imageCopyInfo)
{
    DmaImageInfo src = imageCopyInfo.src;
    DmaImageInfo dst = imageCopyInfo.dst;

    SubResourceInfo srcSubResInfo = *src.pSubresInfo;
    SubResourceInfo dstSubResInfo = *dst.pSubresInfo;

    src.pSubresInfo = &srcSubResInfo;
    dst.pSubresInfo = &dstSubResInfo;

    // Calculate the maximum number of pixels we can copy per pass in the below loop
    const uint32  embeddedDataLimit = GetEmbeddedDataLimit();
    const uint32  copySizeDwords    = Min(NumBytesToNumDwords(imageCopyInfo.copyExtent.width * src.bytesPerPixel),
                                          embeddedDataLimit);
    const uint32  copySizeBytes     = copySizeDwords * sizeof(uint32);
    const uint32  copySizePixels    = copySizeBytes / src.bytesPerPixel;

    // We only need one instance of this memory for the entire life of this command buffer.  Allocate it on an
    // as-needed basis.
    if (m_pT2tEmbeddedGpuMemory == nullptr)
    {
        AllocateEmbeddedT2tMemory();

        PAL_ASSERT(m_pT2tEmbeddedGpuMemory != nullptr);
    }

    // A lot of the parameters are a constant for each scanline, so set those up here.
    MemoryImageCopyRegion  linearDstCopyRgn = {};
    linearDstCopyRgn.imageSubres         = src.pSubresInfo->subresId;
    linearDstCopyRgn.imageExtent.width   = copySizePixels;
    linearDstCopyRgn.imageExtent.height  = 1;
    linearDstCopyRgn.imageExtent.depth   = 1;
    linearDstCopyRgn.numSlices           = 1;
    linearDstCopyRgn.gpuMemoryRowPitch   = copySizeBytes;
    linearDstCopyRgn.gpuMemoryDepthPitch = linearDstCopyRgn.gpuMemoryRowPitch * imageCopyInfo.copyExtent.height;
    linearDstCopyRgn.gpuMemoryOffset     = m_t2tEmbeddedMemOffset;

    MemoryImageCopyRegion  tiledDstCopyRgn = linearDstCopyRgn;
    tiledDstCopyRgn.imageSubres            = dst.pSubresInfo->subresId;

    // tiled to tiled copies have been determined to not work for this case, so a dual-stage copy is required.
    // Because we have a limit on the amount of embedded data, we're going to do the copy slice-by-slice and
    // scan-line by scan-line.
    Pal::HwPipePoint  pipePoints   = HwPipePoint::HwPipeBottom;
    Pal::BarrierInfo  barrierInfo  = {};
    barrierInfo.pipePointWaitCount = 1;
    barrierInfo.pPipePoints        = &pipePoints;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 360
    barrierInfo.reason             = Developer::BarrierReasonDmaImgScanlineCopySync;
#endif

    uint32*  pCmdSpace = nullptr;

    for (uint32  sliceIdx = 0; sliceIdx < imageCopyInfo.copyExtent.depth; sliceIdx++)
    {
        if (GetImageType(*src.pImage) == ImageType::Tex3d)
        {
            linearDstCopyRgn.imageOffset.z = src.offset.z + sliceIdx;
        }
        else if (sliceIdx > 0)
        {
            srcSubResInfo.subresId.arraySlice++;
        }

        if (GetImageType(*dst.pImage) == ImageType::Tex3d)
        {
            tiledDstCopyRgn.imageOffset.z = dst.offset.z + sliceIdx;
        }
        else if (sliceIdx > 0)
        {
            dstSubResInfo.subresId.arraySlice++;
        }

        for (uint32  yIdx = 0; yIdx < imageCopyInfo.copyExtent.height; yIdx++)
        {
            linearDstCopyRgn.imageOffset.y = src.offset.y + yIdx;
            tiledDstCopyRgn.imageOffset.y  = dst.offset.y + yIdx;

            for (uint32  xIdx = 0; xIdx < imageCopyInfo.copyExtent.width; xIdx += copySizePixels)
            {
                linearDstCopyRgn.imageOffset.x = src.offset.x + xIdx;
                tiledDstCopyRgn.imageOffset.x  = dst.offset.x + xIdx;

                pCmdSpace  = m_cmdStream.ReserveCommands();
                pCmdSpace = WriteCopyTiledImageToMemCmd(src, *m_pT2tEmbeddedGpuMemory, linearDstCopyRgn, pCmdSpace);
                m_cmdStream.CommitCommands(pCmdSpace);

                // Potentially have to wait for the copy to finish before we transfer out of that memory
                CmdBarrier(barrierInfo);

                pCmdSpace  = m_cmdStream.ReserveCommands();
                pCmdSpace = WriteCopyMemToTiledImageCmd(*m_pT2tEmbeddedGpuMemory, dst, tiledDstCopyRgn, pCmdSpace);
                m_cmdStream.CommitCommands(pCmdSpace);

                // Wait for this copy to finish before we re-use the temp-linear buffer above.
                CmdBarrier(barrierInfo);
            }
        }
    }
}

// =====================================================================================================================
void DmaCmdBuffer::CmdCopyImage(
    const IImage&          srcImage,
    ImageLayout            srcImageLayout,
    const IImage&          dstImage,
    ImageLayout            dstImageLayout,
    uint32                 regionCount,
    const ImageCopyRegion* pRegions,
    uint32                 flags)
{
    uint32* pCmdSpace = nullptr;
    uint32* pPredCmd  = nullptr;

    if (m_predMemEnabled)
    {
        // Write the predication command, we will patch its predication size later.
        pCmdSpace = m_cmdStream.ReserveCommands();

        pPredCmd  = pCmdSpace;
        pCmdSpace = WritePredicateCmd(0, pCmdSpace);

        m_cmdStream.CommitCommands(pCmdSpace);
    }

    // Both images need to use the same image type, so it dosen't matter where we get it from
    const ImageType  imageType = GetImageType(srcImage);
    const Image&     srcImg    = static_cast<const Image&>(srcImage);
    const Image&     dstImg    = static_cast<const Image&>(dstImage);

    bool p2pBltInfoRequired = m_pDevice->IsP2pBltWaRequired(*dstImg.GetBoundGpuMemory().Memory());

    uint32 newRegionCount = 0;
    if (p2pBltInfoRequired)
    {
        m_pDevice->P2pBltWaModifyRegionListImage(srcImg,
                                                 dstImg,
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
            m_pDevice->P2pBltWaModifyRegionListImage(srcImg,
                                                     dstImg,
                                                     regionCount,
                                                     pRegions,
                                                     &newRegionCount,
                                                     &newRegions[0],
                                                     &chunkAddrs[0]);

            regionCount = newRegionCount;
            pRegions    = &newRegions[0];

            P2pBltWaCopyBegin(dstImg.GetBoundGpuMemory().Memory(), regionCount, &chunkAddrs[0]);
        }
        else
        {
            NotifyAllocFailure();
            p2pBltInfoRequired = false;
        }
    }

    for (uint32 rgnIdx = 0; rgnIdx < regionCount; rgnIdx++)
    {
        const auto& region = pRegions[rgnIdx];

        DmaImageCopyInfo imageCopyInfo = {};
        uint32           srcTexelScale = 1;
        uint32           dstTexelScale = 1;

        if (p2pBltInfoRequired)
        {
            P2pBltWaCopyNextRegion(chunkAddrs[rgnIdx]);
        }

        SetupDmaInfoSurface(srcImage, region.srcSubres, region.srcOffset, &imageCopyInfo.src, &srcTexelScale);
        SetupDmaInfoSurface(dstImage, region.dstSubres, region.dstOffset, &imageCopyInfo.dst, &dstTexelScale);

        // Both images must have the same BPP and texel scales, otherwise nothing will line up.
        PAL_ASSERT(imageCopyInfo.src.bytesPerPixel == imageCopyInfo.dst.bytesPerPixel);
        PAL_ASSERT(srcTexelScale == dstTexelScale);

        // Multiply the copy width by the texel scale to keep our units in sync.
        imageCopyInfo.copyExtent.width  = region.extent.width * srcTexelScale;
        imageCopyInfo.copyExtent.height = region.extent.height;
        imageCopyInfo.copyExtent.depth  = ((imageType == ImageType::Tex3d) ? region.extent.depth : region.numSlices);

        // Determine if this copy covers the whole subresource.
        if ((region.srcOffset.x   == 0)                               &&
            (region.srcOffset.y   == 0)                               &&
            (region.srcOffset.z   == 0)                               &&
            (region.dstOffset.x   == 0)                               &&
            (region.dstOffset.y   == 0)                               &&
            (region.dstOffset.z   == 0)                               &&
            (region.extent.width  == imageCopyInfo.src.extent.width)  &&
            (region.extent.height == imageCopyInfo.src.extent.height) &&
            (region.extent.depth  == imageCopyInfo.src.extent.depth))
        {
            // We're copying the whole subresouce; hide the alignment requirements by copying parts of the padding. We
            // can copy no more than the intersection between the two "actual" rectangles.
            //
            // TODO: See if we can optimize this at all. We might only need to do this for tiled copies and can probably
            //       clamp the final width/height to something smaller than the whole padded image size.
            const uint32 minWidth  = Min(imageCopyInfo.src.actualExtent.width,  imageCopyInfo.dst.actualExtent.width);
            const uint32 minHeight = Min(imageCopyInfo.src.actualExtent.height, imageCopyInfo.dst.actualExtent.height);

            imageCopyInfo.src.extent.width  = minWidth;
            imageCopyInfo.src.extent.height = minHeight;

            imageCopyInfo.dst.extent.width  = minWidth;
            imageCopyInfo.dst.extent.height = minHeight;

            imageCopyInfo.copyExtent.width  = minWidth;
            imageCopyInfo.copyExtent.height = minHeight;
        }

        if (srcImg.IsSubResourceLinear(region.srcSubres))
        {
            if (dstImg.IsSubResourceLinear(region.dstSubres))
            {
                WriteCopyImageLinearToLinearCmd(imageCopyInfo);
            }
            else
            {
                WriteCopyImageLinearToTiledCmd(imageCopyInfo);
            }
        }
        else
        {
            if (dstImg.IsSubResourceLinear(region.dstSubres))
            {
                WriteCopyImageTiledToLinearCmd(imageCopyInfo);
            }
            else
            {
                // The built-in packets for scanline copies have some restrictions on their use.  Determine if this
                // copy is natively supported or if it needs to be done piecemeal.
                if (UseT2tScanlineCopy(imageCopyInfo) == false)
                {
                    WriteCopyImageTiledToTiledCmd(imageCopyInfo);
                }
                else
                {
                    WriteCopyImageTiledToTiledCmdScanlineCopy(imageCopyInfo);
                }
            }
        }
    }

    if (p2pBltInfoRequired)
    {
        P2pBltWaCopyEnd();
    }

    if (m_predMemEnabled)
    {
        // We're done writing commands, patch the predicate command.
        PatchPredicateCmd(static_cast<size_t>(pCmdSpace - pPredCmd), pPredCmd);
    }
}

// =====================================================================================================================
void DmaCmdBuffer::CmdCopyMemoryToImage(
    const IGpuMemory&            srcGpuMemory,
    const IImage&                dstImage,
    ImageLayout                  dstImageLayout,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions)
{
    uint32* pCmdSpace = nullptr;
    uint32* pPredCmd  = nullptr;

    if (m_predMemEnabled)
    {
        // Write the predication command, we will patch its predication size later.
        pCmdSpace = m_cmdStream.ReserveCommands();

        pPredCmd  = pCmdSpace;
        pCmdSpace = WritePredicateCmd(0, pCmdSpace);

        m_cmdStream.CommitCommands(pCmdSpace);
    }

    const GpuMemory& srcMemory = static_cast<const GpuMemory&>(srcGpuMemory);
    const Image&     dstImg    = static_cast<const Image&>(dstImage);
    const ImageType  imageType = GetImageType(dstImage);

    bool p2pBltInfoRequired = m_pDevice->IsP2pBltWaRequired(*dstImg.GetBoundGpuMemory().Memory());

    uint32 newRegionCount = 0;
    if (p2pBltInfoRequired)
    {
        m_pDevice->P2pBltWaModifyRegionListMemoryToImage(srcMemory,
                                                         dstImg,
                                                         regionCount,
                                                         pRegions,
                                                         &newRegionCount,
                                                         nullptr,
                                                         nullptr);
    }

    AutoBuffer<MemoryImageCopyRegion, 32, Platform> newRegions(newRegionCount, m_pDevice->GetPlatform());
    AutoBuffer<gpusize, 32, Platform> chunkAddrs(newRegionCount, m_pDevice->GetPlatform());
    if (p2pBltInfoRequired)
    {
        if ((newRegions.Capacity() >= newRegionCount) && (chunkAddrs.Capacity() >= newRegionCount))
        {
            m_pDevice->P2pBltWaModifyRegionListMemoryToImage(srcMemory,
                                                             dstImg,
                                                             regionCount,
                                                             pRegions,
                                                             &newRegionCount,
                                                             &newRegions[0],
                                                             &chunkAddrs[0]);
            regionCount = newRegionCount;
            pRegions    = &newRegions[0];

            P2pBltWaCopyBegin(dstImg.GetBoundGpuMemory().Memory(), regionCount, &chunkAddrs[0]);
        }
        else
        {
            NotifyAllocFailure();
            p2pBltInfoRequired = false;
        }
    }

    // For each region, determine which specific hardware copy type (memory-to-tiled or memory-to-linear) is necessary.
    for (uint32 rgnIdx = 0; rgnIdx < regionCount ; rgnIdx++)
    {
        MemoryImageCopyRegion region     = pRegions[rgnIdx];
        DmaImageInfo          imageInfo  = {};
        uint32                texelScale = 1;

        if (p2pBltInfoRequired)
        {
            P2pBltWaCopyNextRegion(chunkAddrs[rgnIdx]);
        }

        SetupDmaInfoSurface(dstImage, region.imageSubres, region.imageOffset, &imageInfo, &texelScale);

        // Multiply the region's offset and extent by the texel scale to keep our units in sync.
        region.imageOffset.x     *= texelScale;
        region.imageExtent.width *= texelScale;

        // For the purposes of the "WriteCopyMem" functions, "depth" is the number of slices to copy
        // which can come from different places in the original "region".
        region.imageExtent.depth  = ((imageType == ImageType::Tex3d) ? region.imageExtent.depth : region.numSlices);

        // Figure out whether we can copy using native DMA packets or need to punt to a workaround path
        const bool isLinearImg  = dstImg.IsSubResourceLinear(region.imageSubres);
        const auto copyMethod   = GetMemImageCopyMethod(isLinearImg, imageInfo, region);

        // Native copy path
        if (copyMethod == DmaMemImageCopyMethod::Native)
        {
            pCmdSpace = m_cmdStream.ReserveCommands();

            if (isLinearImg)
            {
                pCmdSpace = WriteCopyMemToLinearImageCmd(srcMemory, imageInfo, region, pCmdSpace);
            }
            else
            {
                pCmdSpace = WriteCopyMemToTiledImageCmd(srcMemory, imageInfo, region, pCmdSpace);
            }

            m_cmdStream.CommitCommands(pCmdSpace);
        }
        // Workaround path where the x-extents are not properly dword-aligned (slow)
        else
        {
            PAL_ASSERT(copyMethod == DmaMemImageCopyMethod::DwordUnaligned);

            WriteCopyMemImageDwordUnalignedCmd(true, isLinearImg, srcMemory, imageInfo, region);
        }
    }

    if (p2pBltInfoRequired)
    {
        P2pBltWaCopyEnd();
    }

    if (m_predMemEnabled)
    {
        // We're done writing commands, patch the predicate command.
        PatchPredicateCmd(static_cast<size_t>(pCmdSpace - pPredCmd), pPredCmd);
    }
}

// =====================================================================================================================
void DmaCmdBuffer::CmdCopyImageToMemory(
    const IImage&                srcImage,
    ImageLayout                  srcImageLayout,
    const IGpuMemory&            dstGpuMemory,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions)
{
    uint32* pCmdSpace = nullptr;
    uint32* pPredCmd  = nullptr;

    if (m_predMemEnabled)
    {
        // Write the predication command, we will patch its predication size later.
        pCmdSpace = m_cmdStream.ReserveCommands();

        pPredCmd  = pCmdSpace;
        pCmdSpace = WritePredicateCmd(0, pCmdSpace);

        m_cmdStream.CommitCommands(pCmdSpace);
    }

    // For each region, determine which specific hardware copy type (tiled-to-memory or linear-to-memory) is necessary.
    const GpuMemory& dstMemory = static_cast<const GpuMemory&>(dstGpuMemory);
    const Image&     srcImg    = static_cast<const Image&>(srcImage);
    const ImageType  imageType = GetImageType(srcImage);

    bool p2pBltInfoRequired = m_pDevice->IsP2pBltWaRequired(dstMemory);

    uint32 newRegionCount = 0;
    if (p2pBltInfoRequired)
    {
        m_pDevice->P2pBltWaModifyRegionListImageToMemory(srcImg,
                                                         dstMemory,
                                                         regionCount,
                                                         pRegions,
                                                         &newRegionCount,
                                                         nullptr,
                                                         nullptr);
    }

    AutoBuffer<MemoryImageCopyRegion, 32, Platform> newRegions(newRegionCount, m_pDevice->GetPlatform());
    AutoBuffer<gpusize, 32, Platform> chunkAddrs(newRegionCount, m_pDevice->GetPlatform());
    if (p2pBltInfoRequired)
    {
        if ((newRegions.Capacity() >= newRegionCount) && (chunkAddrs.Capacity() >= newRegionCount))
        {
            m_pDevice->P2pBltWaModifyRegionListImageToMemory(srcImg,
                                                             dstMemory,
                                                             regionCount,
                                                             pRegions,
                                                             &newRegionCount,
                                                             &newRegions[0],
                                                             &chunkAddrs[0]);
            regionCount = newRegionCount;
            pRegions    = &newRegions[0];

            P2pBltWaCopyBegin(&dstMemory, regionCount, &chunkAddrs[0]);
        }
        else
        {
            NotifyAllocFailure();
            p2pBltInfoRequired = false;
        }
    }

    for (uint32 rgnIdx = 0; rgnIdx < regionCount ; rgnIdx++)
    {
        MemoryImageCopyRegion region     = pRegions[rgnIdx];
        DmaImageInfo          imageInfo  = {};
        uint32                texelScale = 1;

        if (p2pBltInfoRequired)
        {
            P2pBltWaCopyNextRegion(chunkAddrs[rgnIdx]);
        }

        SetupDmaInfoSurface(srcImage, region.imageSubres, region.imageOffset, &imageInfo, &texelScale);

        // Multiply the region's offset and extent by the texel scale to keep our units in sync.
        region.imageOffset.x     *= texelScale;
        region.imageExtent.width *= texelScale;

        // For the purposes of the "WriteCopy..." functions, "depth" is the number of slices to copy
        // which can come from different places in the original "region".
        region.imageExtent.depth  = ((imageType == ImageType::Tex3d) ? region.imageExtent.depth : region.numSlices);

        // Figure out whether we can use native SDMA copy or need to punt to a workaround path
        const bool isLinearImg = srcImg.IsSubResourceLinear(region.imageSubres);
        const auto copyMethod  = GetMemImageCopyMethod(isLinearImg, imageInfo, region);

        // Native copy with SDMA
        if (copyMethod == DmaMemImageCopyMethod::Native)
        {
            pCmdSpace = m_cmdStream.ReserveCommands();

            if (isLinearImg)
            {
                pCmdSpace = WriteCopyLinearImageToMemCmd(imageInfo, dstMemory, region, pCmdSpace);
            }
            else
            {
                pCmdSpace = WriteCopyTiledImageToMemCmd(imageInfo, dstMemory, region, pCmdSpace);
            }

            m_cmdStream.CommitCommands(pCmdSpace);
        }
        // Workaround path where the x-extents are not properly dword-aligned (slow)
        else
        {
            PAL_ASSERT(copyMethod == DmaMemImageCopyMethod::DwordUnaligned);

            WriteCopyMemImageDwordUnalignedCmd(false, isLinearImg, dstMemory, imageInfo, region);
        }
    }

    if (p2pBltInfoRequired)
    {
        P2pBltWaCopyEnd();
    }

    if (m_predMemEnabled)
    {
        // We're done writing commands, patch the predicate command.
        PatchPredicateCmd(static_cast<size_t>(pCmdSpace - pPredCmd), pPredCmd);
    }
}

// =====================================================================================================================
void DmaCmdBuffer::CmdCopyMemoryToTiledImage(
    const IGpuMemory&                 srcGpuMemory,
    const IImage&                     dstImage,
    ImageLayout                       dstImageLayout,
    uint32                            regionCount,
    const MemoryTiledImageCopyRegion* pRegions)
{
    AutoBuffer<MemoryImageCopyRegion, 8, Platform> copyRegions(regionCount, m_pDevice->GetPlatform());

    if (copyRegions.Capacity() < regionCount)
    {
        NotifyAllocFailure();
    }
    else
    {
        const ImageMemoryLayout& imgMemLayout = static_cast<const Image&>(dstImage).GetMemoryLayout();
        const Extent2d tileSize = { imgMemLayout.prtTileWidth, imgMemLayout.prtTileHeight };

        for (uint32 i = 0; i < regionCount; ++i)
        {
            copyRegions[i].imageSubres         = pRegions[i].imageSubres;
            copyRegions[i].imageOffset.x       = pRegions[i].imageOffset.x * static_cast<int32>(tileSize.width);
            copyRegions[i].imageOffset.y       = pRegions[i].imageOffset.y * static_cast<int32>(tileSize.height);
            copyRegions[i].imageOffset.z       = pRegions[i].imageOffset.z;
            copyRegions[i].imageExtent.width   = pRegions[i].imageExtent.width * tileSize.width;
            copyRegions[i].imageExtent.height  = pRegions[i].imageExtent.height * tileSize.height;
            copyRegions[i].imageExtent.depth   = pRegions[i].imageExtent.depth;
            copyRegions[i].numSlices           = pRegions[i].numSlices;
            copyRegions[i].gpuMemoryOffset     = pRegions[i].gpuMemoryOffset;
            copyRegions[i].gpuMemoryRowPitch   = pRegions[i].gpuMemoryRowPitch;
            copyRegions[i].gpuMemoryDepthPitch = pRegions[i].gpuMemoryDepthPitch;
        }

        CmdCopyMemoryToImage(srcGpuMemory, dstImage, dstImageLayout, regionCount, &copyRegions[0]);
    }
}

// =====================================================================================================================
void DmaCmdBuffer::CmdCopyTiledImageToMemory(
    const IImage&                     srcImage,
    ImageLayout                       srcImageLayout,
    const IGpuMemory&                 dstGpuMemory,
    uint32                            regionCount,
    const MemoryTiledImageCopyRegion* pRegions)
{
    AutoBuffer<MemoryImageCopyRegion, 8, Platform> copyRegions(regionCount, m_pDevice->GetPlatform());

    if (copyRegions.Capacity() < regionCount)
    {
        NotifyAllocFailure();
    }
    else
    {
        const ImageMemoryLayout& imgMemLayout = static_cast<const Image&>(srcImage).GetMemoryLayout();
        const Extent2d tileSize = { imgMemLayout.prtTileWidth, imgMemLayout.prtTileHeight };

        for (uint32 i = 0; i < regionCount; ++i)
        {
            copyRegions[i].imageSubres         = pRegions[i].imageSubres;
            copyRegions[i].imageOffset.x       = pRegions[i].imageOffset.x * static_cast<int32>(tileSize.width);
            copyRegions[i].imageOffset.y       = pRegions[i].imageOffset.y * static_cast<int32>(tileSize.height);
            copyRegions[i].imageOffset.z       = pRegions[i].imageOffset.z;
            copyRegions[i].imageExtent.width   = pRegions[i].imageExtent.width * tileSize.width;
            copyRegions[i].imageExtent.height  = pRegions[i].imageExtent.height * tileSize.height;
            copyRegions[i].imageExtent.depth   = pRegions[i].imageExtent.depth;
            copyRegions[i].numSlices           = pRegions[i].numSlices;
            copyRegions[i].gpuMemoryOffset     = pRegions[i].gpuMemoryOffset;
            copyRegions[i].gpuMemoryRowPitch   = pRegions[i].gpuMemoryRowPitch;
            copyRegions[i].gpuMemoryDepthPitch = pRegions[i].gpuMemoryDepthPitch;
        }

        CmdCopyImageToMemory(srcImage, srcImageLayout, dstGpuMemory, regionCount, &copyRegions[0]);
    }
}

// =====================================================================================================================
void DmaCmdBuffer::CmdFillMemory(
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    gpusize           fillSize,
    uint32            data)
{
    gpusize dstAddr = dstGpuMemory.Desc().gpuVirtAddr + dstOffset;

    // Both the destination address and the fillSize need to be dword aligned, so verify that here.
    PAL_ASSERT(IsPow2Aligned(dstAddr,  sizeof(uint32)));
    PAL_ASSERT(IsPow2Aligned(fillSize, sizeof(uint32)));

    uint32* pCmdSpace = nullptr;

    gpusize bytesJustCopied = 0;
    gpusize bytesRemaining  = fillSize;

    while (bytesRemaining > 0)
    {
        pCmdSpace = m_cmdStream.ReserveCommands();

        pCmdSpace = WriteFillMemoryCmd(dstAddr, bytesRemaining, data, pCmdSpace, &bytesJustCopied);

        m_cmdStream.CommitCommands(pCmdSpace);

        bytesRemaining -= bytesJustCopied;
        dstAddr        += bytesJustCopied;
    }
}

// =====================================================================================================================
void DmaCmdBuffer::CmdSetPredication(
    IQueryPool*         pQueryPool,
    uint32              slot,
    const IGpuMemory*   pGpuMemory,
    gpusize             offset,
    PredicateType       predType,
    bool                predPolarity,
    bool                waitResults,
    bool                accumulateData)
{
    PAL_ASSERT(pQueryPool == nullptr);

    // On DMA queue, this is the only supported predication
    PAL_ASSERT((pGpuMemory == nullptr) || (predType == PredicateType::Boolean));

    m_predMemAddress = 0;
    if (pGpuMemory != nullptr)
    {
        m_predMemAddress = pGpuMemory->Desc().gpuVirtAddr + offset;
    }

    m_predMemEnabled = ((pQueryPool == nullptr) && (pGpuMemory == nullptr)) ? false : true;
}

// =====================================================================================================================
void DmaCmdBuffer::CmdExecuteNestedCmdBuffers(
    uint32            cmdBufferCount,
    ICmdBuffer*const* ppCmdBuffers)
{
    for (uint32 buf = 0; buf < cmdBufferCount; ++buf)
    {
        auto& cmdBuffer = *static_cast<DmaCmdBuffer*>(ppCmdBuffers[buf]);
        PAL_ASSERT(cmdBuffer.IsNested());

        const bool exclusiveSubmit = cmdBuffer.IsExclusiveSubmit();

        m_cmdStream.TrackNestedEmbeddedData(cmdBuffer.m_embeddedData.chunkList);
        m_cmdStream.TrackNestedCommands(cmdBuffer.m_cmdStream);

        m_cmdStream.Call(cmdBuffer.m_cmdStream, exclusiveSubmit, false);
    }
}

// =====================================================================================================================
// Populate the "extent" and "actualExtent" members of the pImageInfo structure with the dimensions of the subresource
// stored within the pImageInfo structure.
void DmaCmdBuffer::SetupDmaInfoExtent(
    DmaImageInfo*  pImageInfo
    ) const
{
    const auto*   pSubresInfo   = pImageInfo->pSubresInfo;
    const uint32  bytesPerPixel = pSubresInfo->bitsPerTexel / 8;
    const bool    nonPow2Bpp    = (IsPowerOfTwo(bytesPerPixel) == false);

    // We will work in terms of texels except when our BPP isn't a power of two or when our format is block compressed.
    if (nonPow2Bpp || Formats::IsBlockCompressed(pImageInfo->pSubresInfo->format.format))
    {
        pImageInfo->extent       = pSubresInfo->extentElements;
        pImageInfo->actualExtent = pSubresInfo->actualExtentElements;
    }
    else
    {
        pImageInfo->extent       = pSubresInfo->extentTexels;
        pImageInfo->actualExtent = pSubresInfo->actualExtentTexels;
    }
}

// =====================================================================================================================
void DmaCmdBuffer::SetupDmaInfoSurface(
    const IImage&   image,
    const SubresId& subresource,
    const Offset3d& offset,
    DmaImageInfo*   pImageInfo,  // [out] A completed DmaImageInfo struct.
    uint32*         pTexelScale  // [out] Scale all texel offsets/extents by this factor.
    ) const
{
    const auto& srcImg      = static_cast<const Image&>(image);
    const auto* pSubresInfo = srcImg.SubresourceInfo(subresource);

    // The DMA engine expects power-of-two BPPs, otherwise we must scale our texel dimensions and BPP to make it work.
    // Note that we must use a texelScale of one for block compressed textures because the caller must pass in offsets
    // and extents in terms of blocks.
    uint32     texelScale    = 1;
    uint32     bytesPerPixel = pSubresInfo->bitsPerTexel / 8;
    const bool nonPow2Bpp    = (IsPowerOfTwo(bytesPerPixel) == false);

    if (nonPow2Bpp)
    {
        // Fix-up the BPP by copying each channel as its own pixel; this only works for linear subresources.
        PAL_ASSERT(srcImg.IsSubResourceLinear(subresource));

        switch(bytesPerPixel)
        {
        case 12:
            // This is a 96-bit format (R32G32B32). Each texel contains three 32-bit elements.
            texelScale    = 3;
            bytesPerPixel = 4;
            break;
        default:
            PAL_ASSERT_ALWAYS();
            break;
        }
    }

    // Fill out the image information struct, taking care to scale the offset by the texelScale.
    pImageInfo->pImage        = &image;
    pImageInfo->pSubresInfo   = pSubresInfo;
    pImageInfo->baseAddr      = GetSubresourceBaseAddr(srcImg, subresource);
    pImageInfo->offset.x      = offset.x * texelScale;
    pImageInfo->offset.y      = offset.y;
    pImageInfo->offset.z      = offset.z;
    pImageInfo->bytesPerPixel = bytesPerPixel;

    SetupDmaInfoExtent(pImageInfo);

    // Return the texel scale back to the caller so that it can scale other values (e.g., the copy extent).
    *pTexelScale = texelScale;
}

// =====================================================================================================================
// Sets up a DmaTypedBufferRegion struct with info needed for writing packet for CmdCopyTypedBuffer
// Also adjusts 'texel scale' for non-power-of-two bytes per pixel formats
void DmaCmdBuffer::SetupDmaTypedBufferCopyInfo(
    const IGpuMemory&       baseAddr,
    const TypedBufferInfo&  region,
    DmaTypedBufferRegion*   pBuffer,    // [out] A completed DmaTypedBufferRegion struct.
    uint32*                 pTexelScale // [out] Texel scale for the region.
    ) const
{
    // Using the address of the region as the base address
    pBuffer->baseAddr = baseAddr.Desc().gpuVirtAddr + region.offset;

    // Bytes per texel OR bytes per block for block compressed images
    uint32 bytesPerPixel = Formats::BytesPerPixel(region.swizzledFormat.format);
    uint32 texelScale    = 1;

    if (IsPowerOfTwo(bytesPerPixel) == false)
    {
        switch (bytesPerPixel)
        {
        case 12:
            // This is a 96-bit format (R32G32B32). Each texel contains three 32-bit elements.
            texelScale    = 3;
            bytesPerPixel = 4;
            break;
        default:
            PAL_ASSERT_ALWAYS();
            break;
        }
    }

    pBuffer->bytesPerElement = bytesPerPixel;

    PAL_ASSERT(IsPow2Aligned(region.rowPitch, bytesPerPixel));
    PAL_ASSERT(IsPow2Aligned(region.depthPitch, bytesPerPixel));

    // Pre-calculating the linear pitches in the corresponding units for use in the packet info.
    pBuffer->linearRowPitch     = static_cast<uint32>(region.rowPitch / bytesPerPixel);
    pBuffer->linearDepthPitch   = static_cast<uint32>(region.depthPitch / bytesPerPixel);

    *pTexelScale = texelScale;
}

#if PAL_ENABLE_PRINTS_ASSERTS
// =====================================================================================================================
// Dumps this command buffer's single command stream to the given file with an appropriate header.
void DmaCmdBuffer::DumpCmdStreamsToFile(
    File*          pFile,
    CmdBufDumpFormat mode
    ) const
{
    m_cmdStream.DumpCommands(pFile, "# DMA Queue - Command length = ", mode);
}
#endif

// =====================================================================================================================
// Helper function for a number of OSS versions to ensure that various memory-image copy region values dependent on
// the X-axis are dword-aligned when expressed in units of bytes, as per HW requirements.
bool DmaCmdBuffer::AreMemImageXParamsDwordAligned(
    const DmaImageInfo&          imageInfo,
    const MemoryImageCopyRegion& region)
{
    bool aligned = true;

    // The requirement applies to the the x, rect_x, src/dst_pitch and src/dst_slice_pitch fields of L2T and potentially
    // L2L copy packets.
    if ((((region.imageOffset.x       * imageInfo.bytesPerPixel) & 0x3) != 0) ||
        (((region.imageExtent.width   * imageInfo.bytesPerPixel) & 0x3) != 0) ||
        (((region.gpuMemoryRowPitch   * imageInfo.bytesPerPixel) & 0x3) != 0) ||
        (((region.gpuMemoryDepthPitch * imageInfo.bytesPerPixel) & 0x3) != 0))
    {
        aligned = false;
    }

    return aligned;
}

// =====================================================================================================================
// Helper function used by unaligned mem-image copy workaround paths to pad the X-extents of a copy region to
// dword-alignment requirements when those extents are expressed in units of bytes.
static void AlignMemImgCopyRegionToDword(
    const DmaImageInfo&    image,
    MemoryImageCopyRegion* pRgn)
{
    // The x-offset and x-width values, when represented in units of bytes, must be dword-aligned
    constexpr uint32 XAlign = sizeof(uint32);

    const uint32 bpp   = image.bytesPerPixel;
    const uint32 origX = pRgn->imageOffset.x;

    pRgn->imageOffset.x     = Util::Pow2AlignDown(pRgn->imageOffset.x * bpp, XAlign) / bpp;
    pRgn->imageExtent.width += (origX - pRgn->imageOffset.x);
    pRgn->imageExtent.width = Util::Pow2Align(pRgn->imageExtent.width * bpp, XAlign) / bpp;

    PAL_ASSERT(pRgn->imageExtent.width <= image.actualExtent.width);
}

// =====================================================================================================================
// Workaround for some mem-image copy rectangles:
//
// Copies (slowly) a rectangle whose x byte offset/width is not dword aligned between linear memory and a
// linear/tiled image (both to and from memory).
//
// The copy is done (at best) one scanline at a time:
//
// 1. Copy a larger, correctly-aligned scanline from image to temporary embedded memory (T2L subwindow).
// 2. For memory -> image copies:
//    2a. Copy source memory scanline on top of aligned image scanline in embedded memory (byte copy).
//    2b. Copy (modified) aligned image scanline back to image (L2T subwindow).
// 3. For image -> memory copies:
//    3a. Copy original unaligned portion from embedded image scanline to destination memory (byte copy).
//
// Copies between src/dst memory and embedded memory are done using byte-copies that are not subject to the dword-
// alignment restrictions.  Copies between image and embedded memory are done exclusively using correctly-aligned
// rectangles using L2L/L2T/T2L subwindow copies.
void DmaCmdBuffer::WriteCopyMemImageDwordUnalignedCmd(
    bool                         memToImg,
    bool                         isLinearImg,
    const GpuMemory&             gpuMemory,
    const DmaImageInfo&          image,
    const MemoryImageCopyRegion& rgn)
{
    // Duplicate copy information because we're going to change parts of it in the logic below to conform to
    // alignment requirements and to split the copy volume into multiple pieces
    MemoryImageCopyRegion alignedRgn  = rgn;
    DmaImageInfo alignedImage         = image;
    SubResourceInfo alignedSubResInfo = *image.pSubresInfo;

    alignedImage.pSubresInfo = &alignedSubResInfo;

    // Calculate a correctly aligned region of the image to copy to/from the embedded intermediate.
    AlignMemImgCopyRegionToDword(image, &alignedRgn);

    // The aligned region must be within the (actual i.e. padded) bounds of the subresource
    PAL_ASSERT(alignedRgn.imageExtent.width <= image.actualExtent.width);

    // Calculate the scanline and slice sizes of the aligned region in bytes
    const uint32 scanlineBytes = alignedRgn.imageExtent.width * image.bytesPerPixel;
    const uint32 sliceBytes    = scanlineBytes * alignedRgn.imageExtent.height;

    // This region should already be dword-aligned
    PAL_ASSERT((scanlineBytes % sizeof(uint32)) == 0);

    // How many bytes of embedded data do we have available.  This is used as temp memory to store the aligned
    // region of the image that we are modifying.
    const uint32 embeddedDataBytes = GetEmbeddedDataLimit() * sizeof(uint32);

    // We only need one instance of this memory for the entire life of this command buffer.  Allocate it on an
    // as-needed basis.
    if (m_pT2tEmbeddedGpuMemory == nullptr)
    {
        AllocateEmbeddedT2tMemory();

        PAL_ASSERT(m_pT2tEmbeddedGpuMemory != nullptr);
    }

    // Figure out if we can copy a whole slice at a time between image and embedded (per-scanline is always done
    // between memory and embedded).  This is an optimization for small subresources
    bool wholeSliceInEmbedded = (sliceBytes <= embeddedDataBytes);

    // Figure out at much how many pixels we can copy per scanline between embedded and memory (and image and
    // embedded for scanline copies).  This may actually be less than a scanline in which case even the scanline
    // copy is split into pieces.
    const uint32 copySizeBytes  = Min(scanlineBytes, embeddedDataBytes);
    const uint32 copySizePixels = copySizeBytes / image.bytesPerPixel;

    // Region to copy a scanline (or piece of a scanline) between memory and embedded data, and for non-whole-slice
    // copies between image and embedded memory
    MemoryImageCopyRegion passRgn = {};

    passRgn.imageSubres         = rgn.imageSubres;
    passRgn.imageOffset         = alignedRgn.imageOffset;
    passRgn.imageExtent.width   = copySizePixels;
    passRgn.imageExtent.height  = 1;
    passRgn.imageExtent.depth   = 1;
    passRgn.numSlices           = 1;
    passRgn.gpuMemoryRowPitch   = copySizeBytes;
    passRgn.gpuMemoryDepthPitch = wholeSliceInEmbedded ? sliceBytes : copySizeBytes;
    passRgn.gpuMemoryOffset     = m_t2tEmbeddedMemOffset;

    Pal::HwPipePoint pipePoints = HwPipePoint::HwPipeBottom;
    Pal::BarrierInfo barrierInfo = {};

    barrierInfo.waitPoint          = HwPipeTop;
    barrierInfo.pipePointWaitCount = 1;
    barrierInfo.pPipePoints        = &pipePoints;

    MemoryImageCopyRegion sliceRgn;

    uint32_t skip = 0;

    for (uint32 zIdx = 0; zIdx < alignedRgn.imageExtent.depth; zIdx++)
    {
        if (zIdx > 0)
        {
            if (GetImageType(*image.pImage) == ImageType::Tex3d)
            {
                passRgn.imageOffset.z++;
            }
            else
            {
                passRgn.imageSubres.arraySlice++;
            }
        }

        // Attempt to copy the whole slice from image to embedded if we can.  This simplifies the inner loop
        // below.
        if (wholeSliceInEmbedded)
        {
            // Copy whole slice from image to embedded
            sliceRgn                    = passRgn;
            sliceRgn.imageOffset.x      = alignedRgn.imageOffset.x;
            sliceRgn.imageOffset.y      = alignedRgn.imageOffset.y;
            sliceRgn.imageExtent.width  = alignedRgn.imageExtent.width;
            sliceRgn.imageExtent.height = alignedRgn.imageExtent.height;

            alignedImage.offset = sliceRgn.imageOffset;

            // Copy scanline-piece from image to embedded.
            uint32* pCmdSpace = m_cmdStream.ReserveCommands();

            if (isLinearImg)
            {
                pCmdSpace = WriteCopyLinearImageToMemCmd(alignedImage, *m_pT2tEmbeddedGpuMemory, sliceRgn, pCmdSpace);
            }
            else
            {
                pCmdSpace = WriteCopyTiledImageToMemCmd(alignedImage, *m_pT2tEmbeddedGpuMemory, sliceRgn, pCmdSpace);
            }

            m_cmdStream.CommitCommands(pCmdSpace);

            CmdBarrier(barrierInfo);
        }

        for (uint32 yIdx = 0; yIdx < alignedRgn.imageExtent.height; yIdx++)
        {
            passRgn.imageOffset.y = alignedRgn.imageOffset.y + yIdx;

            // Copy the scanline in contiguous pieces, as much as we can fit in embedded data at once
            for (uint32 xIdx = 0; xIdx < alignedRgn.imageExtent.width; xIdx += copySizePixels)
            {
                passRgn.imageOffset.x = alignedRgn.imageOffset.x + xIdx;

                // If this pass's piece of the scanline intersects the true copy region
                if (((rgn.imageOffset.x >= passRgn.imageOffset.x) &&
                     (rgn.imageOffset.x < passRgn.imageOffset.x + static_cast<int32>(passRgn.imageExtent.width))) ||
                    ((passRgn.imageOffset.x >= rgn.imageOffset.x) &&
                     (passRgn.imageOffset.x < rgn.imageOffset.x + static_cast<int32>(rgn.imageExtent.width))))
                {
                    uint32* pCmdSpace = nullptr;

                    // Copy from image to embedded per scanline if we did not already do a whole slice
                    if (wholeSliceInEmbedded == false)
                    {
                        // Propagate the copy offset to the other struct
                        alignedImage.offset = passRgn.imageOffset;

                        // Copy scanline-piece from image to embedded.
                        pCmdSpace = m_cmdStream.ReserveCommands();

                        if (isLinearImg)
                        {
                            pCmdSpace = WriteCopyLinearImageToMemCmd(alignedImage,
                                                                     *m_pT2tEmbeddedGpuMemory,
                                                                     passRgn,
                                                                     pCmdSpace);
                        }
                        else
                        {
                            pCmdSpace = WriteCopyTiledImageToMemCmd(alignedImage,
                                                                    *m_pT2tEmbeddedGpuMemory,
                                                                    passRgn,
                                                                    pCmdSpace);
                        }

                        m_cmdStream.CommitCommands(pCmdSpace);

                        CmdBarrier(barrierInfo);
                    }

                    // Calculate start/end X-extents of the piece of the copy rectangle that intersects this
                    // scanline
                    const uint32 rectXStart  = Util::Max(rgn.imageOffset.x, passRgn.imageOffset.x);
                    const uint32 rectXEnd    = Util::Min(rgn.imageOffset.x + rgn.imageExtent.width,
                                                         passRgn.imageOffset.x + passRgn.imageExtent.width);

                    // X-offset to start of copy rectangle border within the memory buffer and the embedded region,
                    // respectively.
                    const uint32 memXStart      = rectXStart - rgn.imageOffset.x;
                    const uint32 embeddedXStart = rectXStart - passRgn.imageOffset.x;

                    // Calculate linear byte offset for this scanline-piece within src/dst memory
                    const gpusize memOffset = rgn.gpuMemoryOffset +               // Start of data
                                              zIdx * rgn.gpuMemoryDepthPitch +    // Start of slice
                                              yIdx * rgn.gpuMemoryRowPitch +      // Start of scanline
                                              memXStart * image.bytesPerPixel;    // Start of scanline-piece

                    // Calculate same byte offset for this scanline-piece within the embedded memory
                    gpusize embeddedOffset = passRgn.gpuMemoryOffset +              // Start of data
                                             embeddedXStart * image.bytesPerPixel;  // Start of scanline-piece

                    // If the whole slice is in embedded, offset to the start of the y-th scanline
                    if (wholeSliceInEmbedded)
                    {
                        embeddedOffset += yIdx * passRgn.gpuMemoryRowPitch;
                    }

                    // Number of bytes to copy during this pass to/from memory to embedded
                    const gpusize byteCopySize = (rectXEnd - rectXStart) * image.bytesPerPixel;

                    if (memToImg)
                    {
                        // Copy from memory to embedded region
                        MemoryCopyRegion memToEmbeddedRgn = {};

                        memToEmbeddedRgn.copySize  = byteCopySize;
                        memToEmbeddedRgn.srcOffset = memOffset;
                        memToEmbeddedRgn.dstOffset = embeddedOffset;

                        CopyMemoryRegion(gpuMemory, *m_pT2tEmbeddedGpuMemory, memToEmbeddedRgn);

                        skip++;

                        // Copy from embedded back to the image
                        if (wholeSliceInEmbedded == false)
                        {
                            CmdBarrier(barrierInfo);

                            pCmdSpace = m_cmdStream.ReserveCommands();

                            if (isLinearImg)
                            {
                                pCmdSpace = WriteCopyMemToLinearImageCmd(*m_pT2tEmbeddedGpuMemory, alignedImage,
                                                                         passRgn, pCmdSpace);
                            }
                            else
                            {
                                pCmdSpace = WriteCopyMemToTiledImageCmd(*m_pT2tEmbeddedGpuMemory, alignedImage,
                                                                        passRgn, pCmdSpace);
                            }

                            m_cmdStream.CommitCommands(pCmdSpace);
                        }
                    }
                    else
                    {
                        // Copy from embedded region to memory
                        MemoryCopyRegion embeddedToMemRgn = {};

                        embeddedToMemRgn.copySize  = byteCopySize;
                        embeddedToMemRgn.srcOffset = embeddedOffset;
                        embeddedToMemRgn.dstOffset = memOffset;

                        // Copy from embedded region to memory
                        CopyMemoryRegion(*m_pT2tEmbeddedGpuMemory, gpuMemory, embeddedToMemRgn);
                    }

                    CmdBarrier(barrierInfo);
                }
            } // X
        } // Y

        // Copy from embedded back to the image
        if (memToImg && wholeSliceInEmbedded)
        {
            // Note that sliceRgn has already been set-up at the top of this z-iteration
            CmdBarrier(barrierInfo);

            uint32* pCmdSpace = m_cmdStream.ReserveCommands();

            if (isLinearImg)
            {
                pCmdSpace = WriteCopyMemToLinearImageCmd(*m_pT2tEmbeddedGpuMemory,
                                                         alignedImage,
                                                         sliceRgn,
                                                         pCmdSpace);
            }
            else
            {
                pCmdSpace = WriteCopyMemToTiledImageCmd(*m_pT2tEmbeddedGpuMemory,
                                                        alignedImage,
                                                        sliceRgn,
                                                        pCmdSpace);
            }

            m_cmdStream.CommitCommands(pCmdSpace);

            CmdBarrier(barrierInfo);
        }
    } // Z
}

// =====================================================================================================================
void DmaCmdBuffer::CmdUpdateBusAddressableMemoryMarker(
    const IGpuMemory& dstGpuMemory,
    gpusize           offset,
    uint32            value)
{
    CmdWriteImmediate(Pal::HwPipePoint::HwPipeBottom,
                      value,
                      Pal::ImmediateDataWidth::ImmediateData32Bit,
                      static_cast<const GpuMemory*>(&dstGpuMemory)->GetBusAddrMarkerVa() + offset);
}

} // Pal
