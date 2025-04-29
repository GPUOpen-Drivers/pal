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

#include "core/cmdAllocator.h"
#include "core/device.h"
#include "core/dmaCmdBuffer.h"
#include "core/image.h"
#include "palFormatInfo.h"
#include "palAutoBuffer.h"
#include "palTypeTraits.h"

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
    PAL_NEVER_CALLED_MSG("CmdSetUserData not supported for this bind point on this command buffer!");
}

// =====================================================================================================================
DmaCmdBuffer::DmaCmdBuffer(
    Device*                    pDevice,
    const CmdBufferCreateInfo& createInfo,
    uint32                     copyOverlapHazardSyncs)
    :
    CmdBuffer(*pDevice, createInfo),
    m_pDevice(pDevice),
    m_cmdStream(pDevice,
                createInfo.pCmdAllocator,
                EngineTypeDma,
                SubEngineType::Primary,
                CmdStreamUsage::Workload,
                0,
                0,
                IsNested()),
    m_predMemEnabled(false),
    m_copyOverlapHazardSyncs(copyOverlapHazardSyncs),
    m_predInternalAddr(0),
    m_pT2tEmbeddedGpuMemory(nullptr),
    m_t2tEmbeddedMemOffset(0)
{
    PAL_ASSERT(createInfo.queueType == QueueTypeDma);

    SwitchCmdSetUserDataFunc(PipelineBindPoint::Compute,   &DummyCmdSetUserData);
    SwitchCmdSetUserDataFunc(PipelineBindPoint::Graphics,  &DummyCmdSetUserData);
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
        m_pT2tEmbeddedGpuMemory = nullptr;
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

        const CmdStream* cmdStreams[] = { &m_cmdStream };
        EndCmdBufferDump(cmdStreams, 1);
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
// Do any work needed for a single image within a barrier. In practice, this only handles metadata init.
//
// Returns true if any work was done.
bool DmaCmdBuffer::HandleImageTransition(
    const IImage* pImage,
    ImageLayout    oldLayout,
    ImageLayout    newLayout,
    SubresRange    subresRange)
{
    PAL_ASSERT(pImage != nullptr);

    bool didTransition = false;

#if PAL_ENABLE_PRINTS_ASSERTS || PAL_ENABLE_LOGGING
    // With the exception of a transition uninitialized state, at least one queue type must be valid for every layout.
    if (oldLayout.usages != 0)
    {
        PAL_ASSERT((oldLayout.usages == LayoutUninitializedTarget) || (oldLayout.engines != 0));
    }
    if (newLayout.usages != 0)
    {
        PAL_ASSERT((newLayout.usages == LayoutUninitializedTarget) || (newLayout.engines != 0));
    }
#endif

    // At least one usage must be specified for the old and new layouts.
    PAL_ASSERT((oldLayout.usages != 0) && (newLayout.usages != 0));

    // With the exception of a transition out of the uninitialized state, at least one queue type must be valid
    // for every layout.

    PAL_ASSERT(((oldLayout.usages == LayoutUninitializedTarget) ||
                (oldLayout.engines != 0)) &&
                (newLayout.engines != 0));

    // DMA supports metadata initialization transitions via GfxImage's InitMetadataFill function.
    if (TestAnyFlagSet(oldLayout.usages, LayoutUninitializedTarget))
    {
        const auto*  pPalImage = static_cast<const Pal::Image*>(pImage);

        // If the image is uninitialized, no other usages should be set.
        PAL_ASSERT(TestAnyFlagSet(oldLayout.usages, ~LayoutUninitializedTarget) == false);

#if PAL_ENABLE_PRINTS_ASSERTS || PAL_ENABLE_LOGGING
        const auto& engineProps = m_pDevice->EngineProperties().perEngine[EngineTypeDma];
        const bool  isFullPlane = pPalImage->IsRangeFullPlane(subresRange);

        // DMA must support this barrier transition.
        PAL_ASSERT(engineProps.flags.supportsImageInitBarrier == 1);

        // By default, the entire plane must be initialized in one go. Per-subres support can be requested using
        // an image flag as long as the queue supports it.
        const auto& createInfo = pImage->GetImageCreateInfo();
        PAL_ASSERT(isFullPlane || ((engineProps.flags.supportsImageInitPerSubresource == 1) &&
                                   (createInfo.flags.perSubresInit == 1)));
#endif

        const auto*const pGfxImage = pPalImage->GetGfxImage();

        if (pGfxImage != nullptr)
        {
            pGfxImage->InitMetadataFill(this, subresRange, newLayout);
            didTransition = true;
        }
    }
    return didTransition;
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

    bool imageTypeRequiresCopyOverlapHazardSyncs = false;

    if (m_copyOverlapHazardSyncs == ((1u << ImageType::Count) - 1))
    {
        imageTypeRequiresCopyOverlapHazardSyncs = true;
    }

    bool didTransition = false;

    for (uint32 i = 0; i < barrier.transitionCount; i++)
    {
        const auto& imageInfo = barrier.pTransitions[i].imageInfo;

        if (imageInfo.pImage != nullptr)
        {
            const uint32 imageType = static_cast<uint32>(imageInfo.pImage->GetImageCreateInfo().imageType);
            imageTypeRequiresCopyOverlapHazardSyncs |= (TestAnyFlagSet(m_copyOverlapHazardSyncs, (1 << imageType)));

            didTransition |= HandleImageTransition(imageInfo.pImage,
                                                   imageInfo.oldLayout,
                                                   imageInfo.newLayout,
                                                   imageInfo.subresRange);
        }
    }

    // Wait for the provided GPU events to be set.
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    // For certain versions of SDMA, some copy/write execution happens asynchronously and the driver is responsible
    // for synchronizing hazards when such copies overlap by inserting a NOP packet, which acts as a fence command.
    if (imageTypeRequiresCopyOverlapHazardSyncs && (barrier.pipePointWaitCount > 0))
    {
        pCmdSpace = WriteNops(pCmdSpace, 1);
    }

    for (uint32 i = 0; i < barrier.gpuEventWaitCount; i++)
    {
        PAL_ASSERT(barrier.ppGpuEvents[i] != nullptr);
        pCmdSpace = WriteWaitEventSet(static_cast<const GpuEvent&>(*barrier.ppGpuEvents[i]), pCmdSpace);
    }

    m_cmdStream.CommitCommands(pCmdSpace);

    // If a BLT occurred, an additional fence command is necessary to synchronize read/write hazards.
    if (imageTypeRequiresCopyOverlapHazardSyncs && didTransition)
    {
        pCmdSpace = m_cmdStream.ReserveCommands();
        pCmdSpace = WriteNops(pCmdSpace, 1);
        m_cmdStream.CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Inserts a barrier in the current command stream that can pipeline a stall in GPU execution, flush/invalidate caches,
// or decompress images before further, dependent work can continue in this command buffer.
//
// There's no real benefit to splitting up barriers on the DMA engine. Ergo, this is a thin wrapper over full barriers.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 885
uint32 DmaCmdBuffer::CmdRelease(
#else
ReleaseToken DmaCmdBuffer::CmdRelease(
#endif
    const AcquireReleaseInfo& releaseInfo)
{
    PAL_NOT_TESTED();
    CmdReleaseThenAcquire(releaseInfo);

    return {};
}

// =====================================================================================================================
// Inserts a barrier in the current command stream that can wait on a pipelined stall, flush/invalidate caches,
// or decompress images before further, dependent work can continue in this command buffer.
//
// There's no real benefit to splitting up barriers on the DMA engine. Ergo, this is a thin wrapper over full barriers.
void DmaCmdBuffer::CmdAcquire(
    const AcquireReleaseInfo& acquireInfo,
    uint32                    syncTokenCount,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 885
    const uint32*             pSyncTokens)
#else
    const ReleaseToken*       pSyncTokens)
#endif
{
    PAL_NOT_TESTED();

    CmdReleaseThenAcquire(acquireInfo);
}

// =====================================================================================================================
// Inserts a barrier in the current command stream that can pipeline a stall in GPU execution, flush/invalidate caches,
// or decompress images before further, dependent work can continue in this command buffer.
//
// There's no real benefit to splitting up barriers on the DMA engine. Ergo, this is a thin wrapper over full barriers.
void DmaCmdBuffer::CmdReleaseEvent(
    const AcquireReleaseInfo& releaseInfo,
    const IGpuEvent*          pGpuEvent)
{
    PAL_NOT_TESTED();
    CmdReleaseThenAcquire(releaseInfo);
    if (pGpuEvent != nullptr)
    {
        CmdSetEvent(*pGpuEvent, PipelineStageBottomOfPipe);
    }
}

// =====================================================================================================================
// Inserts a barrier in the current command stream that can wait on a pipelined stall, flush/invalidate caches,
// or decompress images before further, dependent work can continue in this command buffer.
//
// There's no real benefit to splitting up barriers on the DMA engine. Ergo, this is a thin wrapper over full barriers.
void DmaCmdBuffer::CmdAcquireEvent(
    const AcquireReleaseInfo& acquireInfo,
    uint32                    gpuEventCount,
    const IGpuEvent* const*   ppGpuEvents)
{
    PAL_NOT_TESTED();
    if (gpuEventCount != 0)
    {
        uint32* pCmdSpace = m_cmdStream.ReserveCommands();
        for (uint32 idx = 0; idx < gpuEventCount; idx++)
        {
            pCmdSpace = WriteWaitEventSet(static_cast<const GpuEvent&>(*ppGpuEvents[idx]), pCmdSpace);
        }
        m_cmdStream.CommitCommands(pCmdSpace);
    }
    CmdReleaseThenAcquire(acquireInfo);
}

// =====================================================================================================================
// Inserts a barrier in the current command stream that can stall GPU execution, flush/invalidate caches, or decompress
// images before further, dependent work can continue in this command buffer.
//
//
// Note: the DMA engines execute strictly in order and don't use any caches so most barrier operations are meaningless.
void DmaCmdBuffer::CmdReleaseThenAcquire(
    const AcquireReleaseInfo& barrierInfo)
{
    bool imageTypeRequiresCopyOverlapHazardSyncs = false;
    if (m_copyOverlapHazardSyncs == ((1 << static_cast<uint32>(ImageType::Count)) - 1))
    {
        imageTypeRequiresCopyOverlapHazardSyncs = true;
    }

    bool didTransition = false;

    for (uint32 i = 0; i < barrierInfo.imageBarrierCount; i++)
    {
        const auto& imageInfo = barrierInfo.pImageBarriers[i];

        if (imageInfo.pImage != nullptr)
        {
            const uint32 imageType = static_cast<uint32>(imageInfo.pImage->GetImageCreateInfo().imageType);
            imageTypeRequiresCopyOverlapHazardSyncs |= (TestAnyFlagSet(m_copyOverlapHazardSyncs, (1 << imageType)));

            didTransition |= HandleImageTransition(imageInfo.pImage,
                                                   imageInfo.oldLayout,
                                                   imageInfo.newLayout,
                                                   imageInfo.subresRange);
        }
    }

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    // For certain versions of SDMA, some copy/write execution happens asynchronously and the driver is responsible
    // for synchronizing hazards when such copies overlap by inserting a NOP packet, which acts as a fence command.
    uint32 srcStageMask = barrierInfo.srcGlobalStageMask;

    for (uint32 i = 0; i < barrierInfo.memoryBarrierCount; i++)
    {
        srcStageMask |= barrierInfo.pMemoryBarriers[i].srcStageMask;
    }

    for (uint32 i = 0; i < barrierInfo.imageBarrierCount; i++)
    {
        srcStageMask |= barrierInfo.pImageBarriers[i].srcStageMask;
    }

    if (imageTypeRequiresCopyOverlapHazardSyncs && (srcStageMask != 0))
    {
        pCmdSpace = WriteNops(pCmdSpace, 1);
    }

    m_cmdStream.CommitCommands(pCmdSpace);

    // If a BLT occurred, an additional fence command is necessary to synchronize read/write hazards.
    if (imageTypeRequiresCopyOverlapHazardSyncs && didTransition)
    {
        pCmdSpace = m_cmdStream.ReserveCommands();
        pCmdSpace = WriteNops(pCmdSpace, 1);
        m_cmdStream.CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Executes one region's worth of memory-memory copy.
void DmaCmdBuffer::CopyMemoryRegion(
    gpusize                 srcGpuVirtAddr,
    gpusize                 dstGpuVirtAddr,
    DmaCopyFlags            flags,
    const MemoryCopyRegion& region)
{
    gpusize srcGpuAddr      = srcGpuVirtAddr + region.srcOffset;
    gpusize dstGpuAddr      = dstGpuVirtAddr + region.dstOffset;
    gpusize bytesJustCopied = 0;
    gpusize bytesLeftToCopy = region.copySize;

    while (bytesLeftToCopy > 0)
    {
        uint32* pCmdSpace = m_cmdStream.ReserveCommands();
        uint32* pPredCmd  = pCmdSpace;

        pCmdSpace = WritePredicateCmd(pPredCmd);

        pCmdSpace = WriteCopyGpuMemoryCmd(srcGpuAddr,
                                          dstGpuAddr,
                                          bytesLeftToCopy,
                                          flags,
                                          pCmdSpace,
                                          &bytesJustCopied);

        PatchPredicateCmd(pPredCmd, pCmdSpace);
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
    const GpuMemory& srcMemory = static_cast<const GpuMemory&>(srcGpuMemory);
    const GpuMemory& dstMemory = static_cast<const GpuMemory&>(dstGpuMemory);

    DmaCopyFlags flags = srcMemory.IsTmzProtected() ? DmaCopyFlags::TmzCopy : DmaCopyFlags::None;

#if PAL_BUILD_GFX12
    if (srcMemory.MaybeCompressed())
    {
        flags |= DmaCopyFlags::CompressedCopySrc;
    }
    if (dstMemory.MaybeCompressed())
    {
        flags |= DmaCopyFlags::CompressedCopyDst;
    }
#endif

    // Splits up each region's copy size into chunks that the specific hardware can handle.
    for (uint32 rgnIdx = 0; rgnIdx < regionCount; rgnIdx++)
    {
        CopyMemoryRegion(srcGpuMemory.Desc().gpuVirtAddr, dstGpuMemory.Desc().gpuVirtAddr, flags, pRegions[rgnIdx]);
    }
}

// =====================================================================================================================
void DmaCmdBuffer::CmdCopyMemoryByGpuVa(
    gpusize                 srcGpuVirtAddr,
    gpusize                 dstGpuVirtAddr,
    uint32                  regionCount,
    const MemoryCopyRegion* pRegions)
{

    // Splits up each region's copy size into chunks that the specific hardware can handle.
#if PAL_BUILD_GFX12
    constexpr DmaCopyFlags Flags = (DmaCopyFlags::CompressedCopySrc | DmaCopyFlags::CompressedCopyDst);
#else
    constexpr DmaCopyFlags Flags = DmaCopyFlags::None;
#endif
    for (uint32 rgnIdx = 0; rgnIdx < regionCount; rgnIdx++)
    {
        CopyMemoryRegion(srcGpuVirtAddr, dstGpuVirtAddr, Flags, pRegions[rgnIdx]);
    }
}

// =====================================================================================================================
void DmaCmdBuffer::CmdCopyTypedBuffer(
    const IGpuMemory&            srcGpuMemory,
    const IGpuMemory&            dstGpuMemory,
    uint32                       regionCount,
    const TypedBufferCopyRegion* pRegions)
{
    const GpuMemory& srcMemory = static_cast<const GpuMemory&>(srcGpuMemory);
    const GpuMemory& dstMemory = static_cast<const GpuMemory&>(dstGpuMemory);

    uint32* pCmdSpace = nullptr;
    uint32* pPredCmd = nullptr;

    for (uint32 rgnIdx = 0; rgnIdx < regionCount; rgnIdx++)
    {
        const TypedBufferCopyRegion& region = pRegions[rgnIdx];
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

        if (srcMemory.IsTmzProtected())
        {
            copyInfo.flags = DmaCopyFlags::TmzCopy;
        }

#if PAL_BUILD_GFX12
        if (srcMemory.MaybeCompressed())
        {
            copyInfo.flags |= DmaCopyFlags::CompressedCopySrc;
        }
        if (dstMemory.MaybeCompressed())
        {
            copyInfo.flags |= DmaCopyFlags::CompressedCopyDst;
        }
#endif

        // Write packet
        pCmdSpace = m_cmdStream.ReserveCommands();
        pPredCmd  = pCmdSpace;
        pCmdSpace = WritePredicateCmd(pPredCmd);

        pCmdSpace = WriteCopyTypedBuffer(copyInfo, pCmdSpace);

        PatchPredicateCmd(pPredCmd, pCmdSpace);
        m_cmdStream.CommitCommands(pCmdSpace);
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
// Linear image to tiled image copy
uint32* DmaCmdBuffer::WriteCopyImageLinearToTiledCmd(
    const DmaImageCopyInfo& imageCopyInfo,
    uint32* pCmdSpace)
{
    return CopyImageLinearTiledTransform(imageCopyInfo, imageCopyInfo.src, imageCopyInfo.dst, false, pCmdSpace);
}

// =====================================================================================================================
// Tiled image to linear image copy
uint32* DmaCmdBuffer::WriteCopyImageTiledToLinearCmd(
    const DmaImageCopyInfo& imageCopyInfo,
    uint32*                 pCmdSpace)
{
    return CopyImageLinearTiledTransform(imageCopyInfo, imageCopyInfo.dst, imageCopyInfo.src, true, pCmdSpace);
}

// =====================================================================================================================
// Tiled image to tiled image copy, chunk by chunk.
void DmaCmdBuffer::WriteCopyImageTiledToTiledCmdChunkCopy(
    const DmaImageCopyInfo& imageCopyInfo)
{
    DmaImageInfo src = imageCopyInfo.src;
    DmaImageInfo dst = imageCopyInfo.dst;

    SubResourceInfo srcSubResInfo = *src.pSubresInfo;
    SubResourceInfo dstSubResInfo = *dst.pSubresInfo;

    src.pSubresInfo = &srcSubResInfo;
    dst.pSubresInfo = &dstSubResInfo;

    const ImageType srcImageType = src.pImage->GetImageCreateInfo().imageType;
    const ImageType dstImageType = dst.pImage->GetImageCreateInfo().imageType;

    // Calculate the maximum number of pixels we can copy per pass in the below loop
    const uint32 embeddedDataLimitBytes = GetEmbeddedDataLimit() * sizeof(uint32);

    // How big a window can we copy given our linear data limit?
    uint32 widthToCopyPixels      = 1;
    uint32 heightToCopyPixels     = 1;
    uint32 depthToCopyPixels      = 1;
    uint32 copySizeBytes          = src.bytesPerPixel;
    uint32 gpuMemRowPitchPixels   = 1;
    uint32 gpuMemDepthPitchPixels = 1;
    PAL_ASSERT(copySizeBytes <= embeddedDataLimitBytes); //If we can't fit one pixel... then what?
    if (embeddedDataLimitBytes > copySizeBytes)
    {
        //Widen the copy area to possibly fit more texels.
        widthToCopyPixels = Min((embeddedDataLimitBytes / copySizeBytes), imageCopyInfo.copyExtent.width);
        // row pitch must align with hardware requirements
        gpuMemRowPitchPixels = Util::RoundUpToMultiple(widthToCopyPixels, GetLinearRowPitchAlignment(src.bytesPerPixel));
        copySizeBytes = gpuMemRowPitchPixels * src.bytesPerPixel;
        if (embeddedDataLimitBytes > copySizeBytes)
        {
            //Heighten the copy area to possibly fit more rows.
            heightToCopyPixels = Min((embeddedDataLimitBytes / copySizeBytes), imageCopyInfo.copyExtent.height);
            // depth pitch must align with hardware requirements
            gpuMemDepthPitchPixels = gpuMemRowPitchPixels * heightToCopyPixels;
            copySizeBytes = gpuMemDepthPitchPixels * src.bytesPerPixel;
            if (embeddedDataLimitBytes > copySizeBytes)
            {
                //Deepen the copy area to possibly fit more slices- but only if we're copying 3D textures.
                //If either the input or output is a texture array, we have to do it one slice at a time.
                if ((ImageType::Tex3d == srcImageType) && (ImageType::Tex3d == dstImageType))
                {
                    depthToCopyPixels = Min((embeddedDataLimitBytes / copySizeBytes), imageCopyInfo.copyExtent.depth);
                    copySizeBytes = gpuMemDepthPitchPixels * depthToCopyPixels * src.bytesPerPixel;
                }
            }
        }
    }
    PAL_ASSERT(copySizeBytes <= embeddedDataLimitBytes);

    // We only need one instance of this memory for the entire life of this command buffer.  Allocate it on an
    // as-needed basis.
    if (m_pT2tEmbeddedGpuMemory == nullptr)
    {
        AllocateEmbeddedT2tMemory();

        PAL_ASSERT(m_pT2tEmbeddedGpuMemory != nullptr);
    }

    // A lot of the parameters are a constant for each copy region, so set those up here.
    MemoryImageCopyRegion  linearDstCopyRgn = {};
    linearDstCopyRgn.imageSubres         = src.pSubresInfo->subresId;
    linearDstCopyRgn.gpuMemoryRowPitch   = gpuMemRowPitchPixels * src.bytesPerPixel;
    linearDstCopyRgn.gpuMemoryDepthPitch = gpuMemDepthPitchPixels * src.bytesPerPixel;
    linearDstCopyRgn.gpuMemoryOffset     = m_t2tEmbeddedMemOffset;

    MemoryImageCopyRegion  tiledDstCopyRgn = linearDstCopyRgn;
    tiledDstCopyRgn.imageSubres            = dst.pSubresInfo->subresId;

    // Tiled to tiled copies have been determined to not work for this case, so a dual-stage copy is required.
    // Because we have a limit on the amount of embedded data, we're going to do the copy chunk by chunk.
    // First by trying to go scanline by scanline, then groups of scanlines, then groups of [slices|depth].
    constexpr AcquireReleaseInfo AcqRelInfo =
    {
        .srcGlobalStageMask = PipelineStageBottomOfPipe,
        .dstGlobalStageMask = PipelineStageTopOfPipe,
        .reason             = Developer::BarrierReasonDmaImgScanlineCopySync
    };

    uint32*  pCmdSpace = nullptr;
    uint32*  pPredCmd  = nullptr;

    uint32 cappedDepthToCopy = depthToCopyPixels;
    for (uint32 sliceIdx = 0; sliceIdx < imageCopyInfo.copyExtent.depth; sliceIdx += cappedDepthToCopy)
    {
        if ((sliceIdx + cappedDepthToCopy) > imageCopyInfo.copyExtent.depth)
        {
            cappedDepthToCopy = (imageCopyInfo.copyExtent.depth - sliceIdx);
        }

        //Update the source params
        if (ImageType::Tex3d == srcImageType)
        {
            linearDstCopyRgn.imageExtent.depth = cappedDepthToCopy;
            linearDstCopyRgn.numSlices = 1;

            linearDstCopyRgn.imageOffset.z = src.offset.z + sliceIdx;
        }
        else
        {
            linearDstCopyRgn.imageExtent.depth = 1;
            linearDstCopyRgn.numSlices = cappedDepthToCopy;

            if (sliceIdx > 0)
            {
                srcSubResInfo.subresId.arraySlice += cappedDepthToCopy;
                linearDstCopyRgn.imageOffset.z = sliceIdx;
            }
        }

        //Update the destination params
        if (ImageType::Tex3d == dstImageType)
        {
            tiledDstCopyRgn.imageExtent.depth = cappedDepthToCopy;
            tiledDstCopyRgn.numSlices = 1;

            tiledDstCopyRgn.imageOffset.z = dst.offset.z + sliceIdx;
        }
        else
        {
            tiledDstCopyRgn.imageExtent.depth = 1;
            tiledDstCopyRgn.numSlices = cappedDepthToCopy;

            if (sliceIdx > 0)
            {
                dstSubResInfo.subresId.arraySlice += cappedDepthToCopy;
                tiledDstCopyRgn.imageOffset.z = sliceIdx;
            }
        }

        uint32 cappedHeightToCopy = heightToCopyPixels;
        for (uint32  yIdx = 0; yIdx < imageCopyInfo.copyExtent.height; yIdx += cappedHeightToCopy)
        {
            if ((yIdx + cappedHeightToCopy) > imageCopyInfo.copyExtent.height)
            {
                cappedHeightToCopy = (imageCopyInfo.copyExtent.height - yIdx);
            }
            linearDstCopyRgn.imageExtent.height = cappedHeightToCopy;
            tiledDstCopyRgn.imageExtent.height  = cappedHeightToCopy;

            linearDstCopyRgn.imageOffset.y = src.offset.y + yIdx;
            tiledDstCopyRgn.imageOffset.y  = dst.offset.y + yIdx;

            uint32 cappedWidthToCopy = widthToCopyPixels;
            for (uint32  xIdx = 0; xIdx < imageCopyInfo.copyExtent.width; xIdx += cappedWidthToCopy)
            {
                if ((xIdx + cappedWidthToCopy) > imageCopyInfo.copyExtent.width)
                {
                    cappedWidthToCopy = (imageCopyInfo.copyExtent.width - xIdx);
                }
                linearDstCopyRgn.imageExtent.width = cappedWidthToCopy;
                tiledDstCopyRgn.imageExtent.width  = cappedWidthToCopy;

                linearDstCopyRgn.imageOffset.x = src.offset.x + xIdx;
                tiledDstCopyRgn.imageOffset.x  = dst.offset.x + xIdx;

                pCmdSpace = m_cmdStream.ReserveCommands();
                pPredCmd  = pCmdSpace;
                pCmdSpace = WritePredicateCmd(pPredCmd);

                pCmdSpace = WriteCopyTiledImageToMemCmd(src, *m_pT2tEmbeddedGpuMemory, linearDstCopyRgn, pCmdSpace);

                PatchPredicateCmd(pPredCmd, pCmdSpace);
                m_cmdStream.CommitCommands(pCmdSpace);

                // Potentially have to wait for the copy to finish before we transfer out of that memory
                CmdReleaseThenAcquire(AcqRelInfo);

                pCmdSpace  = m_cmdStream.ReserveCommands();
                pPredCmd  = pCmdSpace;
                pCmdSpace = WritePredicateCmd(pPredCmd);

                pCmdSpace = WriteCopyMemToTiledImageCmd(*m_pT2tEmbeddedGpuMemory, dst, tiledDstCopyRgn, pCmdSpace);

                PatchPredicateCmd(pPredCmd, pCmdSpace);
                m_cmdStream.CommitCommands(pCmdSpace);

                // Wait for this copy to finish before we re-use the temp-linear buffer above.
                CmdReleaseThenAcquire(AcqRelInfo);
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
    const Rect*            pScissorRect,
    uint32                 flags)
{
    PAL_ASSERT(TestAnyFlagSet(flags, CopyEnableScissorTest) == false);

    // Both images need to use the same image type, so it dosen't matter where we get it from
    const ImageType  imageType = srcImage.GetImageCreateInfo().imageType;
    const Image&     srcImg    = static_cast<const Image&>(srcImage);
    const Image&     dstImg    = static_cast<const Image&>(dstImage);

    uint32* pCmdSpace;
    uint32* pPredCmd;

    for (uint32 rgnIdx = 0; rgnIdx < regionCount; rgnIdx++)
    {
        const auto& region = pRegions[rgnIdx];

        DmaImageCopyInfo imageCopyInfo = {};
        uint32           srcTexelScale = 1;
        uint32           dstTexelScale = 1;

        SetupDmaInfoSurface(srcImage,
                            region.srcSubres,
                            region.srcOffset,
                            srcImageLayout,
                            &imageCopyInfo.src,
                            &srcTexelScale);
        SetupDmaInfoSurface(dstImage,
                            region.dstSubres,
                            region.dstOffset,
                            dstImageLayout,
                            &imageCopyInfo.dst,
                            &dstTexelScale);

        // Both images must have the same BPP and texel scales, otherwise nothing will line up.
        PAL_ASSERT(imageCopyInfo.src.bytesPerPixel == imageCopyInfo.dst.bytesPerPixel);
        PAL_ASSERT(srcTexelScale == dstTexelScale);

        // Multiply the copy width by the texel scale to keep our units in sync.
        imageCopyInfo.copyExtent.width  = region.extent.width * srcTexelScale;
        imageCopyInfo.copyExtent.height = region.extent.height;
        imageCopyInfo.copyExtent.depth  = ((imageType == ImageType::Tex3d) ? region.extent.depth : region.numSlices);

        // Determine if this copy covers the whole subresource and the layouts are identical.
        const SubResourceInfo* const pSrcSubresInfo = imageCopyInfo.src.pSubresInfo;
        const SubResourceInfo* const pDstSubresInfo = imageCopyInfo.dst.pSubresInfo;
        if ((region.srcOffset.x   == 0)                                                      &&
            (region.srcOffset.y   == 0)                                                      &&
            (region.srcOffset.z   == 0)                                                      &&
            (region.dstOffset.x   == 0)                                                      &&
            (region.dstOffset.y   == 0)                                                      &&
            (region.dstOffset.z   == 0)                                                      &&
            (region.extent.width  == pSrcSubresInfo->extentElements.width)                   &&
            (region.extent.height == pSrcSubresInfo->extentElements.height)                  &&
            (region.extent.depth  == pSrcSubresInfo->extentElements.depth)                   &&
            (imageCopyInfo.src.extent.width  == imageCopyInfo.dst.extent.width)              &&
            (imageCopyInfo.src.extent.height == imageCopyInfo.dst.extent.height)             &&
            (imageCopyInfo.src.extent.depth  == imageCopyInfo.dst.extent.depth)              &&
            (pSrcSubresInfo->extentElements.width  == pDstSubresInfo->extentElements.width)  &&
            (pSrcSubresInfo->extentElements.height == pDstSubresInfo->extentElements.height) &&
            (pSrcSubresInfo->extentElements.depth  == pDstSubresInfo->extentElements.depth)  &&
            (pSrcSubresInfo->subresId.plane == pDstSubresInfo->subresId.plane)               &&
            (pSrcSubresInfo->subresId.mipLevel == pDstSubresInfo->subresId.mipLevel)         &&
            (pSrcSubresInfo->subresId.arraySlice == pDstSubresInfo->subresId.arraySlice))
        {
            // We're copying the whole subresouce; hide the alignment requirements by copying parts of the padding. We
            // can copy no more than the intersection between the two "actual" rectangles.
            //
            // TODO: See if we can optimize this at all. We might only need to do this for tiled copies and can probably
            //       clamp the final width/height to something smaller than the whole padded image size.
            const uint32 minWidth  = Min(imageCopyInfo.src.actualExtent.width,  imageCopyInfo.dst.actualExtent.width);
            const uint32 minHeight = Min(imageCopyInfo.src.actualExtent.height, imageCopyInfo.dst.actualExtent.height);

            const uint32 minSubResourceWidth  = Min(pSrcSubresInfo->actualExtentElements.width,
                                                    pDstSubresInfo->actualExtentElements.width);
            const uint32 minSubResourceHeight = Min(pSrcSubresInfo->actualExtentElements.height,
                                                    pDstSubresInfo->actualExtentElements.height);

            imageCopyInfo.src.extent.width  = minWidth;
            imageCopyInfo.src.extent.height = minHeight;

            imageCopyInfo.dst.extent.width  = minWidth;
            imageCopyInfo.dst.extent.height = minHeight;

            imageCopyInfo.copyExtent.width  = minSubResourceWidth;
            imageCopyInfo.copyExtent.height = minSubResourceHeight;
        }

        if (srcImg.IsSubResourceLinear(region.srcSubres))
        {
            pCmdSpace = m_cmdStream.ReserveCommands();
            pPredCmd  = pCmdSpace;
            pCmdSpace = WritePredicateCmd(pPredCmd);

            if (dstImg.IsSubResourceLinear(region.dstSubres))
            {
                pCmdSpace = WriteCopyImageLinearToLinearCmd(imageCopyInfo, pCmdSpace);
            }
            else
            {
                pCmdSpace = WriteCopyImageLinearToTiledCmd(imageCopyInfo, pCmdSpace);
            }

            PatchPredicateCmd(pPredCmd, pCmdSpace);
            m_cmdStream.CommitCommands(pCmdSpace);
        }
        else
        {
            if (dstImg.IsSubResourceLinear(region.dstSubres))
            {
                pCmdSpace = m_cmdStream.ReserveCommands();
                pPredCmd  = pCmdSpace;
                pCmdSpace = WritePredicateCmd(pPredCmd);

                pCmdSpace = WriteCopyImageTiledToLinearCmd(imageCopyInfo, pCmdSpace);

                PatchPredicateCmd(pPredCmd, pCmdSpace);
                m_cmdStream.CommitCommands(pCmdSpace);
            }
            else
            {
                // The built-in packets for tiled copies have some restrictions on their use.  Determine if this
                // copy is natively supported or if it needs to be done piecemeal. First check to see if there is
                // a DXC Panel setting that force all transfers to use scanline copy. Very useful in diagnosing sDMA issues
                if (m_pDevice->Settings().forceT2tScanlineCopies || UseT2tScanlineCopy(imageCopyInfo))
                {
                    WriteCopyImageTiledToTiledCmdChunkCopy(imageCopyInfo);
                }
                else
                {
                    pCmdSpace = m_cmdStream.ReserveCommands();
                    pPredCmd  = pCmdSpace;
                    pCmdSpace = WritePredicateCmd(pPredCmd);

                    pCmdSpace = WriteCopyImageTiledToTiledCmd(imageCopyInfo, pCmdSpace);

                    PatchPredicateCmd(pPredCmd, pCmdSpace);
                    m_cmdStream.CommitCommands(pCmdSpace);
                }
            }
        }
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
    const GpuMemory& srcMemory = static_cast<const GpuMemory&>(srcGpuMemory);
    const Image&     dstImg    = static_cast<const Image&>(dstImage);
    const ImageType  imageType = dstImage.GetImageCreateInfo().imageType;

    // For each region, determine which specific hardware copy type (memory-to-tiled or memory-to-linear) is necessary.
    for (uint32 rgnIdx = 0; rgnIdx < regionCount ; rgnIdx++)
    {
        MemoryImageCopyRegion region     = pRegions[rgnIdx];
        DmaImageInfo          imageInfo  = {};
        uint32                texelScale = 1;

        SetupDmaInfoSurface(dstImage, region.imageSubres, region.imageOffset, dstImageLayout, &imageInfo, &texelScale);

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
            uint32*       pCmdSpace = m_cmdStream.ReserveCommands();
            uint32* const pPredCmd  = pCmdSpace;

            pCmdSpace = WritePredicateCmd(pPredCmd);

            if (isLinearImg)
            {
                pCmdSpace = WriteCopyMemToLinearImageCmd(srcMemory, imageInfo, region, pCmdSpace);
            }
            else
            {
                pCmdSpace = WriteCopyMemToTiledImageCmd(srcMemory, imageInfo, region, pCmdSpace);
            }

            PatchPredicateCmd(pPredCmd, pCmdSpace);
            m_cmdStream.CommitCommands(pCmdSpace);
        }
        // Workaround path where the x-extents are not properly dword-aligned (slow)
        else
        {
            PAL_ASSERT(copyMethod == DmaMemImageCopyMethod::DwordUnaligned);

            WriteCopyMemImageDwordUnalignedCmd(true, isLinearImg, srcMemory, imageInfo, region);
        }
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
    // For each region, determine which specific hardware copy type (tiled-to-memory or linear-to-memory) is necessary.
    const GpuMemory& dstMemory = static_cast<const GpuMemory&>(dstGpuMemory);
    const Image&     srcImg    = static_cast<const Image&>(srcImage);
    const ImageType  imageType = srcImage.GetImageCreateInfo().imageType;

    for (uint32 rgnIdx = 0; rgnIdx < regionCount ; rgnIdx++)
    {
        MemoryImageCopyRegion region     = pRegions[rgnIdx];
        DmaImageInfo          imageInfo  = {};
        uint32                texelScale = 1;

        SetupDmaInfoSurface(srcImage, region.imageSubres, region.imageOffset, srcImageLayout, &imageInfo, &texelScale);

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
            uint32*       pCmdSpace = m_cmdStream.ReserveCommands();
            uint32* const pPredCmd  = pCmdSpace;

            pCmdSpace = WritePredicateCmd(pPredCmd);

            if (isLinearImg)
            {
                pCmdSpace = WriteCopyLinearImageToMemCmd(imageInfo, dstMemory, region, pCmdSpace);
            }
            else
            {
                pCmdSpace = WriteCopyTiledImageToMemCmd(imageInfo, dstMemory, region, pCmdSpace);
            }

            PatchPredicateCmd(pPredCmd, pCmdSpace);
            m_cmdStream.CommitCommands(pCmdSpace);
        }
        // Workaround path where the x-extents are not properly dword-aligned (slow)
        else
        {
            PAL_ASSERT(copyMethod == DmaMemImageCopyMethod::DwordUnaligned);

            WriteCopyMemImageDwordUnalignedCmd(false, isLinearImg, dstMemory, imageInfo, region);
        }
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
    const GpuMemory& dstMemory = static_cast<const GpuMemory&>(dstGpuMemory);
    gpusize          dstAddr   = dstGpuMemory.Desc().gpuVirtAddr + dstOffset;

    // Both the destination address and the fillSize need to be dword aligned, so verify that here.
    PAL_ASSERT(IsPow2Aligned(dstAddr,  sizeof(uint32)));
    PAL_ASSERT(IsPow2Aligned(fillSize, sizeof(uint32)));

    uint32* pCmdSpace = nullptr;

    gpusize bytesJustCopied = 0;
    gpusize bytesRemaining  = fillSize;

    while (bytesRemaining > 0)
    {
        pCmdSpace = m_cmdStream.ReserveCommands();

        pCmdSpace = WriteFillMemoryCmd(dstAddr,
                                       bytesRemaining,
                                       data,
#if PAL_BUILD_GFX12
                                       dstMemory.MaybeCompressed(),
#endif
                                       pCmdSpace,
                                       &bytesJustCopied);

        m_cmdStream.CommitCommands(pCmdSpace);

        bytesRemaining -= bytesJustCopied;
        dstAddr        += bytesJustCopied;
    }
}

// =====================================================================================================================
// Writes the current GPU timestamp value into the specified memory.
void DmaCmdBuffer::CmdWriteTimestamp(
    uint32            stageMask,    // Bitmask of PipelineStageFlag
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset)
{
    const GpuMemory& gpuMemory = static_cast<const GpuMemory&>(dstGpuMemory);
    const gpusize    dstAddr   = gpuMemory.Desc().gpuVirtAddr + dstOffset;

    WriteTimestampCmd(dstAddr);
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

    // Check if predication type is one of the supported ones
    const auto& engineCaps = m_pDevice->EngineProperties().perEngine[EngineTypeDma].flags;
    PAL_ASSERT((pGpuMemory == nullptr) ||
        ((predType == PredicateType::Boolean32) && (engineCaps.memory32bPredicationSupport)) ||
        ((predType == PredicateType::Boolean64) && (engineCaps.memory64bPredicationSupport)));

    m_predInternalAddr = 0;
    if (pGpuMemory != nullptr)
    {
        const gpusize predMemAddress = pGpuMemory->Desc().gpuVirtAddr + offset;
        const uint32  predCopyData   = (predPolarity == true);

        uint32* pPredCpuAddr = CmdAllocateEmbeddedData(2, 1, &m_predInternalAddr);

        // Execute if 64-bit value in memory are all 0 when predPolarity is false,
        // or Execute if one or more bits of 64-bit value in memory are not 0 when predPolarity is true.
        *pPredCpuAddr  = (predPolarity == false);

        // Copy and convert predication value from outer predication memory to internal predication memory
        // for easier adding CON_EXEC packet.
        uint32* pCmdSpace = m_cmdStream.ReserveCommands();

        pCmdSpace = WriteSetupInternalPredicateMemoryCmd(predMemAddress, predCopyData, pCmdSpace);

        m_cmdStream.CommitCommands(pCmdSpace);
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
    const IImage&     image,
    SubresId          subresId,
    const Offset3d&   offset,
    const ImageLayout imageLayout,
    DmaImageInfo*     pImageInfo,  // [out] A completed DmaImageInfo struct.
    uint32*           pTexelScale  // [out] Scale all texel offsets/extents by this factor.
    ) const
{
    const auto& srcImg      = static_cast<const Image&>(image);
    const auto* pSubresInfo = srcImg.SubresourceInfo(subresId);

    // The DMA engine expects power-of-two BPPs, otherwise we must scale our texel dimensions and BPP to make it work.
    // Note that we must use a texelScale of one for block compressed textures because the caller must pass in offsets
    // and extents in terms of blocks.
    uint32     texelScale    = 1;
    uint32     bytesPerPixel = pSubresInfo->bitsPerTexel / 8;
    const bool nonPow2Bpp    = (IsPowerOfTwo(bytesPerPixel) == false);

    if (nonPow2Bpp)
    {
        // Fix-up the BPP by copying each channel as its own pixel; this only works for linear subresources.
        PAL_ASSERT(srcImg.IsSubResourceLinear(subresId));

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
    pImageInfo->baseAddr      = GetSubresourceBaseAddr(srcImg, subresId);
    pImageInfo->offset.x      = offset.x * texelScale;
    pImageInfo->offset.y      = offset.y;
    pImageInfo->offset.z      = offset.z;
    pImageInfo->bytesPerPixel = bytesPerPixel;
    pImageInfo->imageLayout   = imageLayout;

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

// =====================================================================================================================
// Dumps this command buffer's single command stream to the given file with an appropriate header.
void DmaCmdBuffer::DumpCmdStreamsToFile(
    File*            pFile,
    CmdBufDumpFormat mode
    ) const
{
    m_cmdStream.DumpCommands(pFile, "# DMA Queue - Command length = ", mode);
}

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
        ((region.gpuMemoryRowPitch   & 0x3) != 0)                             ||
        ((region.gpuMemoryDepthPitch & 0x3) != 0))
    {
        aligned = false;
    }

    return aligned;
}

// =====================================================================================================================
// Helper function used by unaligned mem-image copy workaround paths to pad the X-extents of a copy region to
// dword-alignment requirements when those extents are expressed in units of bytes.
static void AlignMemImgCopyRegion(
    const DmaImageInfo&    image,
    MemoryImageCopyRegion* pRgn,
    const uint32           xAlign)
{
    const uint32 bpp   = image.bytesPerPixel;
    const uint32 origX = pRgn->imageOffset.x;

    pRgn->imageOffset.x     = Util::Pow2AlignDown(pRgn->imageOffset.x * bpp, xAlign) / bpp;
    pRgn->imageExtent.width += (origX - pRgn->imageOffset.x);
    pRgn->imageExtent.width = Util::Pow2Align(pRgn->imageExtent.width * bpp, xAlign) / bpp;

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

    // Don't always need to align the x-offset and width, so only do that when we need to.
    bool didAlignMemory = false;
    const auto& engineProps = m_pDevice->EngineProperties().perEngine[EngineTypeDma];

    const uint32 widthInBytes = alignedRgn.imageExtent.width * image.bytesPerPixel;
    const uint32 widthAlignment = engineProps.minTiledImageMemCopyAlignment.width;
    if (widthInBytes % widthAlignment != 0)
    {
        // Calculate a correctly aligned region of the image to copy to/from the embedded intermediate.
        AlignMemImgCopyRegion(image, &alignedRgn, widthAlignment);
        didAlignMemory = true;
    }

    // The aligned region must be within the (actual i.e. padded) bounds of the subresource
    PAL_ASSERT(alignedRgn.imageExtent.width <= image.actualExtent.width);

    // Calculate the scanline and slice sizes of the aligned region in bytes
    const uint32 scanlineBytes = alignedRgn.imageExtent.width * image.bytesPerPixel;
    const uint32 sliceBytes    = scanlineBytes * alignedRgn.imageExtent.height;

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

    // These may not already be aligned, so force align them.
    const gpusize gpuMemoryRowPitch   = Pow2Align(copySizeBytes, sizeof(uint32));
    const gpusize gpuMemoryDepthPitch = Pow2Align((wholeSliceInEmbedded ? sliceBytes : copySizeBytes), sizeof(uint32));

    // Region to copy a scanline (or piece of a scanline) between memory and embedded data, and for non-whole-slice
    // copies between image and embedded memory
    MemoryImageCopyRegion passRgn = {};

    passRgn.imageSubres         = rgn.imageSubres;
    passRgn.imageOffset         = alignedRgn.imageOffset;
    passRgn.imageExtent.width   = copySizePixels;
    passRgn.imageExtent.height  = 1;
    passRgn.imageExtent.depth   = 1;
    passRgn.numSlices           = 1;
    passRgn.gpuMemoryRowPitch   = gpuMemoryRowPitch;
    passRgn.gpuMemoryDepthPitch = gpuMemoryDepthPitch;
    passRgn.gpuMemoryOffset     = m_t2tEmbeddedMemOffset;

    constexpr AcquireReleaseInfo AcqRelInfo =
    {
        .srcGlobalStageMask = PipelineStageBottomOfPipe,
        .dstGlobalStageMask = PipelineStageTopOfPipe,
        .reason             = Developer::BarrierReasonUnknown
    };

    MemoryImageCopyRegion sliceRgn;
    uint32* pCmdSpace;
    uint32* pPredCmd;

    for (uint32 zIdx = 0; zIdx < alignedRgn.imageExtent.depth; zIdx++)
    {
        if (zIdx > 0)
        {
            if (image.pImage->GetImageCreateInfo().imageType == ImageType::Tex3d)
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
        if (((memToImg == false) || didAlignMemory) && wholeSliceInEmbedded)
        {
            // Copy whole slice from image to embedded
            sliceRgn                    = passRgn;
            sliceRgn.imageOffset.x      = alignedRgn.imageOffset.x;
            sliceRgn.imageOffset.y      = alignedRgn.imageOffset.y;
            sliceRgn.imageExtent.width  = alignedRgn.imageExtent.width;
            sliceRgn.imageExtent.height = alignedRgn.imageExtent.height;

            alignedImage.offset = sliceRgn.imageOffset;

            // Copy scanline-piece from image to embedded.
            pCmdSpace = m_cmdStream.ReserveCommands();
            pPredCmd  = pCmdSpace;
            pCmdSpace = WritePredicateCmd(pPredCmd);

            if (isLinearImg)
            {
                pCmdSpace = WriteCopyLinearImageToMemCmd(alignedImage, *m_pT2tEmbeddedGpuMemory, sliceRgn, pCmdSpace);
            }
            else
            {
                pCmdSpace = WriteCopyTiledImageToMemCmd(alignedImage, *m_pT2tEmbeddedGpuMemory, sliceRgn, pCmdSpace);
            }

            PatchPredicateCmd(pPredCmd, pCmdSpace);
            m_cmdStream.CommitCommands(pCmdSpace);

            CmdReleaseThenAcquire(AcqRelInfo);
        }

        for (uint32 yIdx = 0; yIdx < alignedRgn.imageExtent.height; yIdx++)
        {
            passRgn.imageOffset.y = alignedRgn.imageOffset.y + yIdx;

            // Copy the scanline in contiguous pieces, as much as we can fit in embedded data at once
            uint32 cappedWidthToCopy = copySizePixels;
            for (uint32 xIdx = 0; xIdx < alignedRgn.imageExtent.width; xIdx += cappedWidthToCopy)
            {
                if ((xIdx + cappedWidthToCopy) > alignedRgn.imageExtent.width)
                {
                    cappedWidthToCopy = (alignedRgn.imageExtent.width - xIdx);
                }

                passRgn.imageExtent.width = cappedWidthToCopy;
                passRgn.imageOffset.x = alignedRgn.imageOffset.x + xIdx;

                // If this pass's piece of the scanline intersects the true copy region
                if (((rgn.imageOffset.x >= passRgn.imageOffset.x) &&
                     (rgn.imageOffset.x < passRgn.imageOffset.x + static_cast<int32>(passRgn.imageExtent.width))) ||
                    ((passRgn.imageOffset.x >= rgn.imageOffset.x) &&
                     (passRgn.imageOffset.x < rgn.imageOffset.x + static_cast<int32>(rgn.imageExtent.width))))
                {
                    // Copy from image to embedded per scanline if we did not already do a whole slice
                    if (((memToImg == false) || didAlignMemory) && (wholeSliceInEmbedded == false))
                    {
                        // Propagate the copy offset to the other struct
                        alignedImage.offset = passRgn.imageOffset;

                        // Copy scanline-piece from image to embedded.
                        pCmdSpace = m_cmdStream.ReserveCommands();
                        pPredCmd  = pCmdSpace;
                        pCmdSpace = WritePredicateCmd(pPredCmd);

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

                        PatchPredicateCmd(pPredCmd, pCmdSpace);
                        m_cmdStream.CommitCommands(pCmdSpace);

                        CmdReleaseThenAcquire(AcqRelInfo);
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

                        DmaCopyFlags flags = gpuMemory.IsTmzProtected() ? DmaCopyFlags::TmzCopy : DmaCopyFlags::None;
#if PAL_BUILD_GFX12
                        if (gpuMemory.MaybeCompressed())
                        {
                            flags |= DmaCopyFlags::CompressedCopySrc;
                        }
                        if (m_pT2tEmbeddedGpuMemory->MaybeCompressed())
                        {
                            flags |= DmaCopyFlags::CompressedCopyDst;
                        }
#endif

                        CopyMemoryRegion(gpuMemory.Desc().gpuVirtAddr,
                                         m_pT2tEmbeddedGpuMemory->Desc().gpuVirtAddr,
                                         flags,
                                         memToEmbeddedRgn);

                        // Copy from embedded back to the image
                        if (wholeSliceInEmbedded == false)
                        {
                            CmdReleaseThenAcquire(AcqRelInfo);

                            pCmdSpace = m_cmdStream.ReserveCommands();
                            pPredCmd  = pCmdSpace;
                            pCmdSpace = WritePredicateCmd(pPredCmd);

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

                            PatchPredicateCmd(pPredCmd, pCmdSpace);
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

                        DmaCopyFlags flags =
                            m_pT2tEmbeddedGpuMemory->IsTmzProtected() ? DmaCopyFlags::TmzCopy : DmaCopyFlags::None;
#if PAL_BUILD_GFX12
                        if (m_pT2tEmbeddedGpuMemory->MaybeCompressed())
                        {
                            flags |= DmaCopyFlags::CompressedCopySrc;
                        }
                        if (gpuMemory.MaybeCompressed())
                        {
                            flags |= DmaCopyFlags::CompressedCopyDst;
                        }
#endif

                        // Copy from embedded region to memory
                        CopyMemoryRegion(m_pT2tEmbeddedGpuMemory->Desc().gpuVirtAddr,
                                         gpuMemory.Desc().gpuVirtAddr,
                                         flags,
                                         embeddedToMemRgn);
                    }

                    CmdReleaseThenAcquire(AcqRelInfo);
                }
            } // X
        } // Y

        // Copy from embedded back to the image
        if (memToImg && wholeSliceInEmbedded)
        {
            CmdReleaseThenAcquire(AcqRelInfo);

            sliceRgn                    = passRgn;
            sliceRgn.imageOffset.x      = alignedRgn.imageOffset.x;
            sliceRgn.imageOffset.y      = alignedRgn.imageOffset.y;
            sliceRgn.imageExtent.width  = alignedRgn.imageExtent.width;
            sliceRgn.imageExtent.height = alignedRgn.imageExtent.height;

            alignedImage.offset = sliceRgn.imageOffset;

            pCmdSpace = m_cmdStream.ReserveCommands();
            pPredCmd  = pCmdSpace;
            pCmdSpace = WritePredicateCmd(pPredCmd);

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

            PatchPredicateCmd(pPredCmd, pCmdSpace);
            m_cmdStream.CommitCommands(pCmdSpace);

            CmdReleaseThenAcquire(AcqRelInfo);
        }
    } // Z
}

// =====================================================================================================================
void DmaCmdBuffer::CmdUpdateBusAddressableMemoryMarker(
    const IGpuMemory& dstGpuMemory,
    gpusize           offset,
    uint32            value)
{
    CmdWriteImmediate(PipelineStageBottomOfPipe,
                      value,
                      Pal::ImmediateDataWidth::ImmediateData32Bit,
                      static_cast<const GpuMemory*>(&dstGpuMemory)->GetBusAddrMarkerVa() + offset);
}

// =====================================================================================================================
uint32 DmaCmdBuffer::GetUsedSize(
    CmdAllocType type
    ) const
{
    uint32 sizeInBytes = CmdBuffer::GetUsedSize(type);

    if (type == CommandDataAlloc)
    {
        sizeInBytes += m_cmdStream.GetUsedCmdMemorySize();
    }

    return sizeInBytes;
}

// =====================================================================================================================
// On GFX10 parts, we always program the base address to point at slice 0.  This means the "z" coordinate (for images
// that have slices) needs to specify the starting slice number.
uint32 DmaCmdBuffer::GetImageZ(
    const DmaImageInfo&  dmaImageInfo,
    uint32               offsetZ
    ) const
{
    const ImageType imageType = dmaImageInfo.pImage->GetImageCreateInfo().imageType;
    uint32          imageZ    = 0;

    if (imageType == ImageType::Tex3d)
    {
        // 3D images can't have array slices, so just return the "z" offset.
        PAL_ASSERT(dmaImageInfo.pSubresInfo->subresId.arraySlice == 0);

        imageZ = offsetZ;
    }
    else
    {
        // For 2D image array, just ignore offsetZ and adopt the start sliceIndex counted from "0".
        const Pal::Image& dmaImg = static_cast<const Pal::Image&>(*dmaImageInfo.pImage);
        imageZ = dmaImg.IsYuvPlanarArray() ? 0 : dmaImageInfo.pSubresInfo->subresId.arraySlice;
    }

    return imageZ;
}

} // Pal
