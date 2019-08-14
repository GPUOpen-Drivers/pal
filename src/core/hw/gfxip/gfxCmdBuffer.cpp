/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/gpuMemory.h"
#include "core/queue.h"
#include "core/perfExperiment.h"
#include "core/hw/gfxip/borderColorPalette.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/pipeline.h"
#include "core/hw/gfxip/rpm/rsrcProcMgr.h"
#include "palAutoBuffer.h"
#include "palDequeImpl.h"
#include "palFormatInfo.h"
#include "palImage.h"
#include "palIntrusiveListImpl.h"
#include "palQueryPool.h"
#include "palVectorImpl.h"

#include <limits.h>

using namespace Util;

namespace Pal
{

// =====================================================================================================================
GfxCmdBuffer::GfxCmdBuffer(
    const GfxDevice&           device,
    const CmdBufferCreateInfo& createInfo)
    :
    CmdBuffer(*device.Parent(), createInfo),
    m_engineSupport(0),
    m_generatedChunkList(device.GetPlatform()),
    m_retainedGeneratedChunkList(device.GetPlatform()),
    m_pCurrentExperiment(nullptr),
    m_gfxIpLevel(device.Parent()->ChipProperties().gfxLevel),
    m_device(device),
    m_pInternalEvent(nullptr),
    m_timestampGpuVa(0),
    m_computeStateFlags(0),
    m_spmTraceEnabled(false),
    m_fceRefCountVec(device.GetPlatform())
{
    PAL_ASSERT((createInfo.queueType == QueueTypeUniversal) || (createInfo.queueType == QueueTypeCompute));

    for (uint32 i = 0; i < static_cast<uint32>(QueryPoolType::Count); i++)
    {
        // Marks the specific query as "active," as in it is available to be used.
        // When we need to push state, the queries are no longer active (we deactivate them), but we want to reactivate
        // all of them after we pop state.
        m_queriesActive[i]    = true;
        m_numActiveQueries[i] = 0;
    }

    m_gfxCmdBufState.flags.u32All = 0;

}

// =====================================================================================================================
GfxCmdBuffer::~GfxCmdBuffer()
{
    ReturnGeneratedCommandChunks(true);

    Device* device = m_device.Parent();

    if (m_pInternalEvent != nullptr)
    {
        m_pInternalEvent->Destroy();
        PAL_SAFE_FREE(m_pInternalEvent, device->GetPlatform());
    }
}

// =====================================================================================================================
Result GfxCmdBuffer::Init(
    const CmdBufferInternalCreateInfo& internalInfo)
{
    Result result = CmdBuffer::Init(internalInfo);

    Device* pDevice = m_device.Parent();

    if (result == Result::Success)
    {
        // Create gpuEvent for this CmdBuffer.
        GpuEventCreateInfo createInfo  = {};
        createInfo.flags.gpuAccessOnly = 1;

        const size_t eventSize = pDevice->GetGpuEventSize(createInfo, &result);

        if (result == Result::Success)
        {
            result = Result::ErrorOutOfMemory;
            void* pMemory = PAL_MALLOC(eventSize, pDevice->GetPlatform(), Util::SystemAllocType::AllocObject);

            if (pMemory != nullptr)
            {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 474
                result = GpuEvent::CreateInternal(pDevice, createInfo, pMemory, &m_pInternalEvent);
#else
                result = pDevice->CreateGpuEvent(createInfo,
                                                 pMemory,
                                                 reinterpret_cast<IGpuEvent**>(&m_pInternalEvent));
#endif
                if (result != Result::Success)
                {
                    PAL_SAFE_FREE(pMemory, pDevice->GetPlatform());
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Resets the command buffer's previous contents and state, then puts it into a building state allowing new commands
// to be recorded.
// Also starts command buffer dumping, if it is enabled.
Result GfxCmdBuffer::Begin(
    const CmdBufferBuildInfo& info)
{
    const Result result = CmdBuffer::Begin(info);

    if (result == Result::Success)
    {
        if (info.pStateInheritCmdBuffer != nullptr)
        {
            InheritStateFromCmdBuf(static_cast<const GfxCmdBuffer*>(info.pStateInheritCmdBuffer));
        }

        // If this is a nested command buffer execution, this value should be set to 1
        // pipePoint on nested command buffer cannot be optimized using the state from primary
        if (IsNested() == true)
        {
            SetGfxCmdBufCpBltState(true);
        }
    }

    return result;
}

// =====================================================================================================================
// Completes recording of a command buffer in the building state, making it executable.
// Also ends command buffer dumping, if it is enabled.
Result GfxCmdBuffer::End()
{
    Result result = CmdBuffer::End();

    // NOTE: The root chunk comes from the last command stream in this command buffer because for universal command
    // buffers, the order of command streams is CE, DE. We always want the "DE" to be the root since the CE may not
    // have any commands, depending on what operations get recorded to the command buffer.
    CmdStreamChunk* pRootChunk = GetCmdStream(NumCmdStreams() - 1)->GetFirstChunk();

    // Finalize all generated command chunks.
    for (auto iter = m_generatedChunkList.Begin(); iter.IsValid(); iter.Next())
    {
        CmdStreamChunk* pChunk = iter.Get();
        pChunk->UpdateRootInfo(pRootChunk);
        pChunk->FinalizeCommands();
    }

    for (uint32 i = 0; i < static_cast<uint32>(QueryPoolType::Count); i++)
    {
        PAL_ASSERT(NumActiveQueries(static_cast<QueryPoolType>(i)) == 0);
    };

    return result;
}

// =====================================================================================================================
Result GfxCmdBuffer::Reset(
    ICmdAllocator* pCmdAllocator,
    bool           returnGpuMemory)
{
    // Do this before our parent class changes the allocator.
    ReturnGeneratedCommandChunks(returnGpuMemory);

    ResetFastClearReferenceCounts();

    return CmdBuffer::Reset(pCmdAllocator, returnGpuMemory);
}

// =====================================================================================================================
// Decrements the ref count of images stored in the Fast clear eliminate ref count array.
void GfxCmdBuffer::ResetFastClearReferenceCounts()
{
    if (m_fceRefCountVec.NumElements() > 0)
    {
        uint32* pCounter = nullptr;

        while (m_fceRefCountVec.NumElements() > 0)
        {
            m_fceRefCountVec.PopBack(&pCounter);
            Util::AtomicDecrement(pCounter);
        }
    }
}

// =====================================================================================================================
void GfxCmdBuffer::ResetState()
{
    CmdBuffer::ResetState();

    m_gfxCmdBufState.flags.u32All           = 0;
    m_gfxCmdBufState.flags.prevCmdBufActive = 1;

    // It's possible that another of our command buffers still has blts in flight, except for CP blts which must be
    // flushed in each command buffer postamble.
    m_gfxCmdBufState.flags.gfxBltActive        = 1;
    m_gfxCmdBufState.flags.gfxWriteCachesDirty = 1;
    m_gfxCmdBufState.flags.csBltActive         = 1;
    m_gfxCmdBufState.flags.csWriteCachesDirty  = 1;

    // A previous, chained command buffer could have used a CP blt which may have accessed L2 or the memory directly.
    // By convention, our CP blts will only use L2 if the HW supports it so we only need to set one bit here.
    if (m_device.Parent()->ChipProperties().gfxLevel > GfxIpLevel::GfxIp6)
    {
        m_gfxCmdBufState.flags.cpWriteCachesDirty = 1;
    }
    else
    {
        m_gfxCmdBufState.flags.cpMemoryWriteL2CacheStale = 1;
    }

}

// =====================================================================================================================
// Puts command stream related objects into a state ready for command building.
Result GfxCmdBuffer::BeginCommandStreams(
    CmdStreamBeginFlags cmdStreamFlags,
    bool                doReset)
{
    if (doReset)
    {
        ReturnGeneratedCommandChunks(true);
    }

    Result result = CmdBuffer::BeginCommandStreams(cmdStreamFlags, doReset);

    if (result == Result::Success)
    {
        // Allocate GPU memory for the internal event from the command allocator.
        result = AllocateAndBindGpuMemToEvent(m_pInternalEvent);
    }

    if (result == Result::Success)
    {
        // Allocate timestamp GPU memory from the command allocator.
        // AllocateGpuScratchMem() always returns a valid GPU address, even if we fail to obtain memory from the
        // allocator.  In that scenario, the allocator returns a dummy chunk so we can always have a valid object
        // to access, and sets m_status to a failure code.
        m_timestampGpuVa = AllocateGpuScratchMem(sizeof(uint32), sizeof(uint32));
        result = m_status;
    }

    return result;
}

// =====================================================================================================================
// Helper method which returns all generated chunks to the parent allocator, removing our references to those chunks.
void GfxCmdBuffer::ReturnGeneratedCommandChunks(
    bool returnGpuMemory)
{
    if (returnGpuMemory)
    {
        // The client requested that we return all chunks, add any remaining retained chunks to the chunk list so they
        // can be returned to the allocator with the rest.
        while (m_retainedGeneratedChunkList.IsEmpty() == false)
        {
            CmdStreamChunk* pChunk = nullptr;
            m_retainedGeneratedChunkList.PopBack(&pChunk);
            m_generatedChunkList.PushBack(pChunk);
        }

        // Return all chunks containing GPU-generated commands to the allocator.
        if (m_generatedChunkList.IsEmpty() == false)
        {
            for (auto iter = m_generatedChunkList.Begin(); iter.IsValid(); iter.Next())
            {
                iter.Get()->RemoveCommandStreamReference();
            }

            m_pCmdAllocator->ReuseChunks(EmbeddedDataAlloc, false, m_generatedChunkList.Begin());
        }
    }
    else
    {
        // Reset the chunks to be retained and add them to the retained list.
        for (auto iter = m_generatedChunkList.Begin(); iter.IsValid(); iter.Next())
        {
            iter.Get()->Reset(false);
            m_retainedGeneratedChunkList.PushBack(iter.Get());
        }
    }

    m_generatedChunkList.Clear();
}

// =====================================================================================================================
HwPipePoint GfxCmdBuffer::OptimizeHwPipePostBlit() const
{
    // If there are no BLTs in flight at this point, we will set the pipe point to HwPipeTop. This will optimize any
    // redundant stalls when called from the barrier implementation. Otherwise, this function remaps the pipe point
    // based on the gfx block that performed the BLT operation.

    HwPipePoint pipePoint = HwPipeTop;

    // Check xxxBltActive states in order
    const GfxCmdBufferState cmdBufState = GetGfxCmdBufState();
    if (cmdBufState.flags.gfxBltActive)
    {
        pipePoint = HwPipeBottom;
    }
    else if (cmdBufState.flags.csBltActive)
    {
        pipePoint = HwPipePostCs;
    }
    else if (cmdBufState.flags.cpBltActive)
    {
        // Note that we set this to post index fetch, which is earlier in the pipeline than our CP blts, because the
        // barrier code will handle CP DMA syncronization for us. This pipe point is still necessary to catch cases
        // when the caller wishes to sync up to the top of the pipeline.
        pipePoint = HwPipePostIndexFetch;
    }

    return pipePoint;
}

// =====================================================================================================================
// If PipelineStageBlt is set, convert it to more specific internal pipeline stages based on GfxCmdBufferState flags.
// Return the processed stageMask.
uint32 GfxCmdBuffer::ConvertToInternalPipelineStageMask(
    uint32 stageMask // [in] A representation of PipelineStageFlag.
    ) const
{
    if (TestAnyFlagSet(stageMask, PipelineStageBlt))
    {
        // If there are no BLTs in flight at this point, we will use the earliest pipe stage TopOfPipe. This will
        // optimize any redundant stalls when called from the barrier implementation. Otherwise, this function remaps
        // the BLT pipe stage based on the gfx block that performed the BLT operation.
        stageMask &= ~PipelineStageBlt;

        // Check xxxBltActive states in order.
        const GfxCmdBufferState cmdBufState = GetGfxCmdBufState();
        if (cmdBufState.flags.gfxBltActive)
        {
            stageMask |= PipelineStageBottomOfPipe;
        }
        else if (cmdBufState.flags.csBltActive)
        {
            stageMask |= PipelineStageCs;
        }
        else
        {
            stageMask |= PipelineStageTopOfPipe;
        }
    }

    return stageMask;
}

// =====================================================================================================================
void GfxCmdBuffer::CmdCopyImage(
    const IImage&          srcImage,
    ImageLayout            srcImageLayout,
    const IImage&          dstImage,
    ImageLayout            dstImageLayout,
    uint32                 regionCount,
    const ImageCopyRegion* pRegions,
    uint32                 flags)
{
    PAL_ASSERT(pRegions != nullptr);
    m_device.RsrcProcMgr().CmdCopyImage(this,
                                        static_cast<const Image&>(srcImage),
                                        srcImageLayout,
                                        static_cast<const Image&>(dstImage),
                                        dstImageLayout,
                                        regionCount,
                                        pRegions,
                                        flags);
}

// =====================================================================================================================
void GfxCmdBuffer::CmdCopyMemoryToImage(
    const IGpuMemory&            srcGpuMemory,
    const IImage&                dstImage,
    ImageLayout                  dstImageLayout,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions)
{
    PAL_ASSERT(pRegions != nullptr);
    m_device.RsrcProcMgr().CmdCopyMemoryToImage(this,
                                                static_cast<const GpuMemory&>(srcGpuMemory),
                                                static_cast<const Image&>(dstImage),
                                                dstImageLayout,
                                                regionCount,
                                                pRegions,
                                                false);
}

// =====================================================================================================================
void GfxCmdBuffer::CmdCopyImageToMemory(
    const IImage&                srcImage,
    ImageLayout                  srcImageLayout,
    const IGpuMemory&            dstGpuMemory,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions)
{
    PAL_ASSERT(pRegions != nullptr);
    m_device.RsrcProcMgr().CmdCopyImageToMemory(this,
                                                static_cast<const Image&>(srcImage),
                                                srcImageLayout,
                                                static_cast<const GpuMemory&>(dstGpuMemory),
                                                regionCount,
                                                pRegions,
                                                false);
}

// =====================================================================================================================
void GfxCmdBuffer::CmdCopyMemoryToTiledImage(
    const IGpuMemory&                 srcGpuMemory,
    const IImage&                     dstImage,
    ImageLayout                       dstImageLayout,
    uint32                            regionCount,
    const MemoryTiledImageCopyRegion* pRegions)
{
    PAL_ASSERT(pRegions != nullptr);

    AutoBuffer<MemoryImageCopyRegion, 8, Platform> copyRegions(regionCount, m_device.GetPlatform());

    if (copyRegions.Capacity() < regionCount)
    {
        NotifyAllocFailure();
    }
    else
    {
        const ImageMemoryLayout& imgMemLayout = static_cast<const Image&>(dstImage).GetMemoryLayout();
        const Extent3d tileSize = { imgMemLayout.prtTileWidth, imgMemLayout.prtTileHeight, imgMemLayout.prtTileDepth };

        for (uint32 i = 0; i < regionCount; ++i)
        {
            copyRegions[i].imageSubres         = pRegions[i].imageSubres;
            copyRegions[i].imageOffset.x       = pRegions[i].imageOffset.x * static_cast<int32>(tileSize.width);
            copyRegions[i].imageOffset.y       = pRegions[i].imageOffset.y * static_cast<int32>(tileSize.height);
            copyRegions[i].imageOffset.z       = pRegions[i].imageOffset.z * static_cast<int32>(tileSize.depth);
            copyRegions[i].imageExtent.width   = pRegions[i].imageExtent.width * tileSize.width;
            copyRegions[i].imageExtent.height  = pRegions[i].imageExtent.height * tileSize.height;
            copyRegions[i].imageExtent.depth   = pRegions[i].imageExtent.depth * tileSize.depth;
            copyRegions[i].numSlices           = pRegions[i].numSlices;
            copyRegions[i].gpuMemoryOffset     = pRegions[i].gpuMemoryOffset;
            copyRegions[i].gpuMemoryRowPitch   = pRegions[i].gpuMemoryRowPitch;
            copyRegions[i].gpuMemoryDepthPitch = pRegions[i].gpuMemoryDepthPitch;
        }

        m_device.RsrcProcMgr().CmdCopyMemoryToImage(this,
                                                    static_cast<const GpuMemory&>(srcGpuMemory),
                                                    static_cast<const Image&>(dstImage),
                                                    dstImageLayout,
                                                    regionCount,
                                                    &copyRegions[0],
                                                    true);
    }
}

// =====================================================================================================================
void GfxCmdBuffer::CmdCopyTiledImageToMemory(
    const IImage&                     srcImage,
    ImageLayout                       srcImageLayout,
    const IGpuMemory&                 dstGpuMemory,
    uint32                            regionCount,
    const MemoryTiledImageCopyRegion* pRegions)
{
    PAL_ASSERT(pRegions != nullptr);

    AutoBuffer<MemoryImageCopyRegion, 8, Platform> copyRegions(regionCount, m_device.GetPlatform());

    if (copyRegions.Capacity() < regionCount)
    {
        NotifyAllocFailure();
    }
    else
    {
        const ImageMemoryLayout& imgMemLayout = static_cast<const Image&>(srcImage).GetMemoryLayout();
        const Extent3d tileSize = { imgMemLayout.prtTileWidth, imgMemLayout.prtTileHeight, imgMemLayout.prtTileDepth };

        for (uint32 i = 0; i < regionCount; ++i)
        {
            copyRegions[i].imageSubres         = pRegions[i].imageSubres;
            copyRegions[i].imageOffset.x       = pRegions[i].imageOffset.x * static_cast<int32>(tileSize.width);
            copyRegions[i].imageOffset.y       = pRegions[i].imageOffset.y * static_cast<int32>(tileSize.height);
            copyRegions[i].imageOffset.z       = pRegions[i].imageOffset.z * static_cast<int32>(tileSize.depth);
            copyRegions[i].imageExtent.width   = pRegions[i].imageExtent.width * tileSize.width;
            copyRegions[i].imageExtent.height  = pRegions[i].imageExtent.height * tileSize.height;
            copyRegions[i].imageExtent.depth   = pRegions[i].imageExtent.depth * tileSize.depth;
            copyRegions[i].numSlices           = pRegions[i].numSlices;
            copyRegions[i].gpuMemoryOffset     = pRegions[i].gpuMemoryOffset;
            copyRegions[i].gpuMemoryRowPitch   = pRegions[i].gpuMemoryRowPitch;
            copyRegions[i].gpuMemoryDepthPitch = pRegions[i].gpuMemoryDepthPitch;
        }

        m_device.RsrcProcMgr().CmdCopyImageToMemory(this,
                                                    static_cast<const Image&>(srcImage),
                                                    srcImageLayout,
                                                    static_cast<const GpuMemory&>(dstGpuMemory),
                                                    regionCount,
                                                    &copyRegions[0],
                                                    true);
    }
}

// =====================================================================================================================
void GfxCmdBuffer::CmdCopyTypedBuffer(
    const IGpuMemory&            srcGpuMemory,
    const IGpuMemory&            dstGpuMemory,
    uint32                       regionCount,
    const TypedBufferCopyRegion* pRegions)
{
    PAL_ASSERT(pRegions != nullptr);
    m_device.RsrcProcMgr().CmdCopyTypedBuffer(this,
                                              static_cast<const GpuMemory&>(srcGpuMemory),
                                              static_cast<const GpuMemory&>(dstGpuMemory),
                                              regionCount,
                                              pRegions);
}

// =====================================================================================================================
void GfxCmdBuffer::CmdScaledCopyImage(
    const ScaledCopyInfo&        copyInfo)
{
    constexpr ScaledCopyInternalFlags NullInternalFlags = {};

    PAL_ASSERT(copyInfo.pRegions != nullptr);
    m_device.RsrcProcMgr().CmdScaledCopyImage(this, copyInfo, NullInternalFlags);
}

// =====================================================================================================================
void GfxCmdBuffer::CmdColorSpaceConversionCopy(
    const IImage&                     srcImage,
    ImageLayout                       srcImageLayout,
    const IImage&                     dstImage,
    ImageLayout                       dstImageLayout,
    uint32                            regionCount,
    const ColorSpaceConversionRegion* pRegions,
    TexFilter                         filter,
    const ColorSpaceConversionTable&  cscTable)
{
    PAL_ASSERT(pRegions != nullptr);
    m_device.RsrcProcMgr().CmdColorSpaceConversionCopy(this,
                                                       static_cast<const Image&>(srcImage),
                                                       srcImageLayout,
                                                       static_cast<const Image&>(dstImage),
                                                       dstImageLayout,
                                                       regionCount,
                                                       pRegions,
                                                       filter,
                                                       cscTable);
}

// =====================================================================================================================
void GfxCmdBuffer::CmdPostProcessFrame(
    const CmdPostProcessFrameInfo& postProcessInfo,
    bool*                          pAddedGpuWork)
{
    bool addedGpuWork = false;

    if (postProcessInfo.flags.srcIsTypedBuffer == 0)
    {
        const auto& image          = static_cast<const Image&>(*postProcessInfo.pSrcImage);
        const auto& presentedImage =
            image;

        // If developer mode is enabled, we need to apply the developer overlay.
        if (m_device.GetPlatform()->ShowDevDriverOverlay())
        {
            m_device.Parent()->ApplyDevOverlay(presentedImage, this);
            addedGpuWork = true;
        }
    }

    if (addedGpuWork && (pAddedGpuWork != nullptr))
    {
        *pAddedGpuWork = true;
    }
}

// =====================================================================================================================
// For BLT presents, this function on GfxCmdBuffer will perform whatever operations are necessary to copy the image data
// from the source image to the destination image.
void GfxCmdBuffer::CmdPresentBlt(
    const IImage&   srcImage,
    const IImage&   dstImage,
    const Offset3d& dstOffset)
{
    constexpr SubresId subres = { ImageAspect::Color, 0, 0, };
    const auto& srcImageInfo  = srcImage.GetImageCreateInfo();

    ImageScaledCopyRegion region = {};
    region.srcExtent.width  = srcImageInfo.extent.width;
    region.srcExtent.height = srcImageInfo.extent.height;
    region.srcExtent.depth  = 1;
    region.dstExtent        = region.srcExtent;
    region.dstOffset        = dstOffset;
    region.srcSubres        = subres;
    region.dstSubres        = subres;
    region.numSlices        = 1;

    ScaledCopyInternalFlags internalFlags = {};
    internalFlags.srcSrgbAsUnorm          = (Formats::IsSrgb(srcImageInfo.swizzledFormat.format)) ? 1 : 0;

    const ImageLayout srcLayout =
    {
        LayoutPresentWindowed,
        (GetEngineType() == EngineTypeUniversal) ? LayoutUniversalEngine : LayoutComputeEngine
    };

    const ImageLayout dstLayout =
    {
        LayoutCopyDst,
        (GetEngineType() == EngineTypeUniversal) ? LayoutUniversalEngine : LayoutComputeEngine
    };

    ScaledCopyInfo      copyInfo         = {};
    constexpr TexFilter DefaultTexFilter = {};

    copyInfo.pSrcImage         = &srcImage;
    copyInfo.srcImageLayout    = srcLayout;
    copyInfo.pDstImage         = &dstImage;
    copyInfo.dstImageLayout    = dstLayout;
    copyInfo.regionCount       = 1;
    copyInfo.pRegions          = &region;
    copyInfo.filter            = DefaultTexFilter;
    copyInfo.rotation          = ImageRotation::Ccw0;
    copyInfo.pColorKey         = nullptr;
    copyInfo.flags.srcColorKey = false;

    m_device.RsrcProcMgr().CmdScaledCopyImage(this, copyInfo, internalFlags);
}

// =====================================================================================================================
void GfxCmdBuffer::CmdFillMemory(
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    gpusize           fillSize,
    uint32            data)
{
    m_device.RsrcProcMgr().CmdFillMemory(this,
                                         (m_computeStateFlags == 0),
                                         static_cast<const GpuMemory&>(dstGpuMemory),
                                         dstOffset,
                                         fillSize,
                                         data);
}

// =====================================================================================================================
void GfxCmdBuffer::CmdClearColorBuffer(
    const IGpuMemory& gpuMemory,
    const ClearColor& color,
    SwizzledFormat    bufferFormat,
    uint32            bufferOffset,
    uint32            bufferExtent,
    uint32            rangeCount,
    const Range*      pRanges)
{
    m_device.RsrcProcMgr().CmdClearColorBuffer(this,
                                               gpuMemory,
                                               color,
                                               bufferFormat,
                                               bufferOffset,
                                               bufferExtent,
                                               rangeCount,
                                               pRanges);
}

// =====================================================================================================================
void GfxCmdBuffer::CmdClearBoundColorTargets(
    uint32                          colorTargetCount,
    const BoundColorTarget*         pBoundColorTargets,
    uint32                          regionCount,
    const ClearBoundTargetRegion*   pClearRegions)
{
    m_device.RsrcProcMgr().CmdClearBoundColorTargets(this,
                                                     colorTargetCount,
                                                     pBoundColorTargets,
                                                     regionCount,
                                                     pClearRegions);
}

// =====================================================================================================================
void GfxCmdBuffer::CmdClearColorImage(
    const IImage&      image,
    ImageLayout        imageLayout,
    const ClearColor&  color,
    uint32             rangeCount,
    const SubresRange* pRanges,
    uint32             boxCount,
    const Box*         pBoxes,
    uint32             flags)
{
    PAL_ASSERT(pRanges != nullptr);
    m_device.RsrcProcMgr().CmdClearColorImage(this,
                                              static_cast<const Image&>(image),
                                              imageLayout,
                                              color,
                                              rangeCount,
                                              pRanges,
                                              boxCount,
                                              pBoxes,
                                              flags);
}

// =====================================================================================================================
void GfxCmdBuffer::CmdClearBoundDepthStencilTargets(
    float                           depth,
    uint8                           stencil,
    uint32                          samples,
    uint32                          fragments,
    DepthStencilSelectFlags         flag,
    uint32                          regionCount,
    const ClearBoundTargetRegion*   pClearRegions)
{
    m_device.RsrcProcMgr().CmdClearBoundDepthStencilTargets(this,
                                                            depth,
                                                            stencil,
                                                            samples,
                                                            fragments,
                                                            flag,
                                                            regionCount,
                                                            pClearRegions);
}

// =====================================================================================================================
void GfxCmdBuffer::CmdClearDepthStencil(
    const IImage&      image,
    ImageLayout        depthLayout,
    ImageLayout        stencilLayout,
    float              depth,
    uint8              stencil,
    uint32             rangeCount,
    const SubresRange* pRanges,
    uint32             rectCount,
    const Rect*        pRects,
    uint32             flags)
{
    PAL_ASSERT(pRanges != nullptr);
    m_device.RsrcProcMgr().CmdClearDepthStencil(this,
                                                static_cast<const Image&>(image),
                                                depthLayout,
                                                stencilLayout,
                                                depth,
                                                stencil,
                                                rangeCount,
                                                pRanges,
                                                rectCount,
                                                pRects,
                                                flags);
}

// =====================================================================================================================
void GfxCmdBuffer::CmdClearBufferView(
    const IGpuMemory& gpuMemory,
    const ClearColor& color,
    const void*       pBufferViewSrd,
    uint32            rangeCount,
    const Range*      pRanges)
{
    PAL_ASSERT(pBufferViewSrd != nullptr);
    m_device.RsrcProcMgr().CmdClearBufferView(this, gpuMemory, color, pBufferViewSrd, rangeCount, pRanges);
}

// =====================================================================================================================
void GfxCmdBuffer::CmdClearImageView(
    const IImage&     image,
    ImageLayout       imageLayout,
    const ClearColor& color,
    const void*       pImageViewSrd,
    uint32            rectCount,
    const Rect*       pRects)
{
     PAL_ASSERT(pImageViewSrd != nullptr);
     m_device.RsrcProcMgr().CmdClearImageView(this,
                                              static_cast<const Image&>(image),
                                              imageLayout,
                                              color,
                                              pImageViewSrd,
                                              rectCount,
                                              pRects);
}

// =====================================================================================================================
void GfxCmdBuffer::CmdResolveImage(
    const IImage&             srcImage,
    ImageLayout               srcImageLayout,
    const IImage&             dstImage,
    ImageLayout               dstImageLayout,
    ResolveMode               resolveMode,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions)
{
    PAL_ASSERT(pRegions != nullptr);
    m_device.RsrcProcMgr().CmdResolveImage(this,
                                           static_cast<const Image&>(srcImage),
                                           srcImageLayout,
                                           static_cast<const Image&>(dstImage),
                                           dstImageLayout,
                                           resolveMode,
                                           regionCount,
                                           pRegions);
}

// =====================================================================================================================
// Copies the requested portion of the currently bound compute state to m_computeRestoreState. All active queries will
// be disabled. This cannot be called again until CmdRestoreComputeState is called.
void GfxCmdBuffer::CmdSaveComputeState(
    uint32 stateFlags)
{
    PAL_ASSERT(m_computeStateFlags == 0);
    m_computeStateFlags = stateFlags;

    if (TestAnyFlagSet(stateFlags, ComputeStatePipelineAndUserData))
    {
        // Copy over the bound pipeline and all non-indirect user-data state.
        m_computeRestoreState = m_computeState;
    }

    if (TestAnyFlagSet(stateFlags, ComputeStateBorderColorPalette))
    {
        // Copy over the bound border color palette.
        m_computeRestoreState.pipelineState.pBorderColorPalette = m_computeState.pipelineState.pBorderColorPalette;
    }

    if (m_pCurrentExperiment != nullptr)
    {
        // Inform the performance experiment that we're starting some internal operations.
        m_pCurrentExperiment->BeginInternalOps(GetCmdStreamByEngine(GetPerfExperimentEngine()));
    }

}

// =====================================================================================================================
// Restores the requested portion of the last saved compute state in m_computeRestoreState, rebinding all objects as
// necessary. All previously disabled queries will be reactivated.
void GfxCmdBuffer::CmdRestoreComputeState(
    uint32 stateFlags)
{
    PAL_ASSERT(TestAllFlagsSet(m_computeStateFlags, stateFlags));
    m_computeStateFlags = 0;

    // Vulkan does allow blits in nested command buffers, but they do not support inheriting user-data values from
    // the caller. Therefore, simply "setting" the restored-state's user-data is sufficient, just like it is in a
    // root command buffer. (If Vulkan decides to support user-data inheritance in a later API version, we'll need
    // to revisit this!)

    SetComputeState(m_computeRestoreState, stateFlags);

    if (m_pCurrentExperiment != nullptr)
    {
        // Inform the performance experiment that we've finished some internal operations.
        m_pCurrentExperiment->EndInternalOps(GetCmdStreamByEngine(GetPerfExperimentEngine()));
    }

    // The caller has just executed one or more CS blts.
    SetGfxCmdBufCsBltState(true);
    SetGfxCmdBufCsBltWriteCacheState(true);
}

// =====================================================================================================================
// Set all specified state on this command buffer.
void GfxCmdBuffer::SetComputeState(
    const ComputeState& newComputeState,
    uint32              stateFlags)
{
    if (TestAnyFlagSet(stateFlags, ComputeStatePipelineAndUserData))
    {
        if (newComputeState.pipelineState.pPipeline != m_computeState.pipelineState.pPipeline)
        {
            PipelineBindParams bindParams = {};
            bindParams.pipelineBindPoint  = PipelineBindPoint::Compute;
            bindParams.pPipeline          = newComputeState.pipelineState.pPipeline;
            bindParams.cs                 = newComputeState.dynamicCsInfo;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 471
            bindParams.apiPsoHash         = newComputeState.pipelineState.apiPsoHash;
#endif

            CmdBindPipeline(bindParams);
        }

        CmdSetUserData(PipelineBindPoint::Compute,
                       0,
                       m_device.Parent()->ChipProperties().gfxip.maxUserDataEntries,
                       &newComputeState.csUserDataEntries.entries[0]);
    }

    if (TestAnyFlagSet(stateFlags, ComputeStateBorderColorPalette) &&
        (newComputeState.pipelineState.pBorderColorPalette != m_computeState.pipelineState.pBorderColorPalette))
    {
        CmdBindBorderColorPalette(PipelineBindPoint::Compute, newComputeState.pipelineState.pBorderColorPalette);
    }
}

// =====================================================================================================================
// Disables all queries on this command buffer, stopping them and marking them as unavailable.
void GfxCmdBuffer::DeactivateQueries()
{
    for (uint32 i = 0; i < static_cast<uint32>(QueryPoolType::Count); i++)
    {
        const QueryPoolType queryPoolType = static_cast<QueryPoolType>(i);

        if (NumActiveQueries(queryPoolType) != 0)
        {
            DeactivateQueryType(queryPoolType);
        }
    }
}

// =====================================================================================================================
// Re-enables all previously active queries on this command buffer, starting them and marking them as available.
void GfxCmdBuffer::ReactivateQueries()
{
    for (uint32 i = 0; i < static_cast<uint32>(QueryPoolType::Count); i++)
    {
        const QueryPoolType queryPoolType = static_cast<QueryPoolType>(i);

        if (NumActiveQueries(queryPoolType) != 0)
        {
            ActivateQueryType(queryPoolType);
        }
    }
}

// =====================================================================================================================
// Updates a user-data table managed by embedded data & CPU updates.
void GfxCmdBuffer::UpdateUserDataTableCpu(
    UserDataTableState* pTable,
    uint32              dwordsNeeded,
    uint32              offsetInDwords,
    const uint32*       pSrcData)       // In: Data representing the *full* contents of the table, not just the part
                                        // between offsetInDwords and dwordsNeeded.
{
    // The dwordsNeeded and offsetInDwords parameters together specify a "window" of the table which is relevant to
    // the active pipeline.  To save memory as well as cycles spent copying data, this will only allocate and populate
    // the portion of the user-data table inside that window.
    PAL_ASSERT((dwordsNeeded + offsetInDwords) <= pTable->sizeInDwords);

    gpusize gpuVirtAddr  = 0uLL;
    pTable->pCpuVirtAddr = (CmdAllocateEmbeddedData(dwordsNeeded, 1, &gpuVirtAddr) - offsetInDwords);
    pTable->gpuVirtAddr  = (gpuVirtAddr - (sizeof(uint32) * offsetInDwords));

    uint32* pDstData = (pTable->pCpuVirtAddr + offsetInDwords);
    pSrcData += offsetInDwords;
    for (uint32 i = 0; i < dwordsNeeded; ++i)
    {
        *pDstData = *pSrcData;
        ++pDstData;
        ++pSrcData;
    }

    // Mark that the latest contents of the user-data table have been uploaded to the current embedded data chunk.
    pTable->dirty = 0;
}

// =====================================================================================================================
// CmdSetUserData callback which updates the tracked user-data entries for the compute state.
void PAL_STDCALL GfxCmdBuffer::CmdSetUserDataCs(
    ICmdBuffer*   pCmdBuffer,
    uint32        firstEntry,
    uint32        entryCount,
    const uint32* pEntryValues)
{
    PAL_ASSERT((pCmdBuffer != nullptr) && (entryCount != 0) && (pEntryValues != nullptr));

    auto*const pEntries = &static_cast<GfxCmdBuffer*>(pCmdBuffer)->m_computeState.csUserDataEntries;

    // NOTE: Compute operations are expected to be far rarer than graphics ones, so at the moment it is not expected
    // that filtering-out redundant compute user-data updates is worthwhile.
    for (uint32 e = firstEntry; e < (firstEntry + entryCount); ++e)
    {
        WideBitfieldSetBit(pEntries->touched, e);
        WideBitfieldSetBit(pEntries->dirty,   e);
    }
    memcpy(&pEntries->entries[firstEntry], pEntryValues, entryCount * sizeof(uint32));
}

// =====================================================================================================================
// Helper function which handles "leaking" a nested command buffer's per-pipeline state after being executed by a root
// command buffer.
void GfxCmdBuffer::LeakPerPipelineStateChanges(
    const Pal::PipelineState& leakedPipelineState,
    const UserDataEntries&    leakedUserDataEntries,
    Pal::PipelineState*       pDestPipelineState,
    UserDataEntries*          pDestUserDataEntries)
{
    if (leakedPipelineState.pBorderColorPalette != nullptr)
    {
        pDestPipelineState->pBorderColorPalette = leakedPipelineState.pBorderColorPalette;
        pDestPipelineState->dirtyFlags.borderColorPaletteDirty = 1;
    }

    if (leakedPipelineState.pPipeline != nullptr)
    {
        pDestPipelineState->pPipeline = leakedPipelineState.pPipeline;
        pDestPipelineState->dirtyFlags.pipelineDirty = 1;
    }

    for (uint32 index = 0; index < NumUserDataFlagsParts; ++index)
    {
        pDestUserDataEntries->dirty[index]   |= leakedUserDataEntries.dirty[index];
        pDestUserDataEntries->touched[index] |= leakedUserDataEntries.touched[index];

        auto mask = leakedUserDataEntries.touched[index];
        while (mask != 0)
        {
            // There is no need to check if the bit-scan found a set bit because the loop condition already does that.
            uint32 bit;
            BitMaskScanForward(&bit, mask);

            const uint32 entry = (bit + (UserDataEntriesPerMask * index));
            pDestUserDataEntries->entries[entry] = leakedUserDataEntries.entries[entry];

            mask &= ~(1 << bit);
        }
    }
}

// =====================================================================================================================
// Returns a new chunk by first searching the retained chunk list for a valid chunk then querying the command allocator
// if there are no retained chunks available.
CmdStreamChunk* GfxCmdBuffer::GetNextGeneratedChunk()
{
    CmdStreamChunk* pChunk = nullptr;

    // First search the retained chunk list
    if (m_retainedGeneratedChunkList.NumElements() > 0)
    {
        // When the chunk was retained the reference count was not modified so no need to add a reference here.
        m_retainedGeneratedChunkList.PopBack(&pChunk);
    }

    // If a retained chunk could not be found then allocate a new chunk and put it on our list. The allocator adds a
    // reference for us automatically. Embedded data chunks cannot be root chunks.
    if (pChunk == nullptr)
    {
        m_status = m_pCmdAllocator->GetNewChunk(EmbeddedDataAlloc, false, &pChunk);

        // If we fail to get a new Chunk from GPU memory either because we ran out of GPU memory or DeviceLost, get a
        // dummy chunk to allow the program to proceed until the error is progagated back to the client.
        if (m_status != Result::Success)
        {
            pChunk = m_pCmdAllocator->GetDummyChunk();
        }
    }

    PAL_ASSERT(pChunk != nullptr);

    const Result result = m_generatedChunkList.PushBack(pChunk);
    PAL_ASSERT(result == Result::Success);

    // Generated chunks shouldn't be allocating their own busy trackers!
    PAL_ASSERT(pChunk->DwordsRemaining() == pChunk->SizeDwords());

    return pChunk;
}

// =====================================================================================================================
// Begins recording performance data using the specified Experiment object.
void GfxCmdBuffer::CmdBeginPerfExperiment(
    IPerfExperiment* pPerfExperiment)
{
    const PerfExperiment*const pExperiment = static_cast<PerfExperiment*>(pPerfExperiment);
    PAL_ASSERT(pExperiment != nullptr);
    CmdStream* pCmdStream = GetCmdStreamByEngine(GetPerfExperimentEngine());

    // Indicates that this command buffer is used for enabling a perf experiment. This is used to write any VCOPs that
    // may be needed during submit time.
    EnableSpmTrace();

    pExperiment->IssueBegin(pCmdStream);
    m_pCurrentExperiment = pExperiment;
}

// =====================================================================================================================
// Updates the sqtt token mask on the specified Experiment object.
void GfxCmdBuffer::CmdUpdatePerfExperimentSqttTokenMask(
    IPerfExperiment*              pPerfExperiment,
    const ThreadTraceTokenConfig& sqttTokenConfig)
{
    const PerfExperiment*const pExperiment = static_cast<PerfExperiment*>(pPerfExperiment);
    PAL_ASSERT(pExperiment != nullptr);
    CmdStream* pCmdStream = GetCmdStreamByEngine(GetPerfExperimentEngine());
    pExperiment->UpdateSqttTokenMask(pCmdStream, sqttTokenConfig);
}

// =====================================================================================================================
void GfxCmdBuffer::CmdEndPerfExperiment(
    IPerfExperiment* pPerfExperiment)
{
    const PerfExperiment*const pExperiment = static_cast<PerfExperiment*>(pPerfExperiment);
    PAL_ASSERT(pPerfExperiment != nullptr);
    CmdStream* pCmdStream = GetCmdStreamByEngine(GetPerfExperimentEngine());
    // Normally, we should only be ending the currently bound perf experiment opened in this command buffer.  However,
    // when gathering full-frame SQ thread traces, an experiment could be opened in one command buffer and ended in
    // another.
    PAL_ASSERT((pPerfExperiment == m_pCurrentExperiment) || (m_pCurrentExperiment == nullptr));

    pExperiment->IssueEnd(pCmdStream);
}

// =====================================================================================================================
CmdBufferEngineSupport GfxCmdBuffer::GetPerfExperimentEngine() const
{
    return (TestAnyFlagSet(m_engineSupport, CmdBufferEngineSupport::Graphics)
            ? CmdBufferEngineSupport::Graphics
            : CmdBufferEngineSupport::Compute);
}

// =====================================================================================================================
void GfxCmdBuffer::CmdCopyImageToPackedPixelImage(
        const IImage&          srcImage,
        const IImage&          dstImage,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        Pal::PackedPixelType   packPixelType)
{
    PAL_ASSERT(pRegions != nullptr);
    m_device.RsrcProcMgr().CopyImageToPackedPixelImage(
        this,
        static_cast<const Image&>(srcImage),
        static_cast<const Image&>(dstImage),
        regionCount,
        pRegions,
        packPixelType);
}

// =====================================================================================================================
// Adds the gfxImage for which a fast clear eliminate was skipped to this command buffers list for tracking and
// increments the ref counter associated with the image.
// Note: The fast clear eliminate optimization aims to remove the unnecessary CPU work that is done for fast clear
//       eliminates for certain barrier transistions (compressed old state to compressed new state). If the clear color
//       was TC-compatible, the corresponding fast clear eliminate operation need not be done as it is predicated by the
//       GPU anyway. We accomplish this by allowing the fast clear eliminate, for this specific transition, only
//       when the image had been cleared with a non-TC-compatible clear color in the past, else we update a counter
//       and skip the fast clear eliminate. During command buffer reset, this counter is decremented for each command
//       buffer and for each time the fast clear eliminate was skipped. This cost of looping through the list is
//       outweighed by all the work that was skipped for setting up the FCE.
Result GfxCmdBuffer::AddFceSkippedImageCounter(
    GfxImage* pGfxImage)
{
    Result result = Result::Success;

    PAL_ASSERT(pGfxImage != nullptr);

    uint32* pCounter = pGfxImage->GetFceRefCounter();

    if (pCounter != nullptr)
    {
        result = m_fceRefCountVec.PushBack(pCounter);
        pGfxImage->IncrementFceRefCount();
    }

    return result;
}

// =====================================================================================================================
uint32 GfxCmdBuffer::GetUsedSize(
    CmdAllocType type
    ) const
{
    uint32 sizeInBytes = CmdBuffer::GetUsedSize(type);

    if (type == CommandDataAlloc)
    {
        uint32 cmdDataSizeInDwords = 0;
        for (auto iter = m_generatedChunkList.Begin(); iter.IsValid(); iter.Next())
        {
            cmdDataSizeInDwords += iter.Get()->DwordsAllocated();
        }

        sizeInBytes += cmdDataSizeInDwords * sizeof(uint32);
    }

    return sizeInBytes;
}

} // Pal
