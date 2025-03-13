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
#include "core/gpuMemory.h"
#include "core/perfExperiment.h"
#include "core/queue.h"
#include "core/hw/gfxip/borderColorPalette.h"
#include "core/hw/gfxip/computePipeline.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/gfxImage.h"
#include "core/hw/gfxip/indirectCmdGenerator.h"
#include "core/hw/gfxip/pipeline.h"
#include "core/hw/gfxip/queryPool.h"
#include "core/hw/gfxip/rpm/rsrcProcMgr.h"
#include "palAutoBuffer.h"
#include "palDequeImpl.h"
#include "palFormatInfo.h"
#include "palHsaAbiMetadata.h"
#include "palHashMapImpl.h"
#include "palImage.h"
#include "palIntrusiveListImpl.h"
#include "palIterator.h"
#include "palQueryPool.h"
#include "palVectorImpl.h"

#include <limits.h>

using namespace Util;

namespace Pal
{

// =====================================================================================================================
GfxCmdBuffer::GfxCmdBuffer(
    const GfxDevice&           device,
    const CmdBufferCreateInfo& createInfo,
    GfxCmdStream*              pCmdStream,
    const GfxBarrierMgr&       barrierMgr,
    bool                       isGfxSupported)
    :
    CmdBuffer(*device.Parent(), createInfo),
    m_device(device),
    m_isGfxSupported(isGfxSupported),
    m_generatedChunkList(device.GetPlatform()),
    m_retainedGeneratedChunkList(device.GetPlatform()),
    m_pCurrentExperiment(nullptr),
    m_gfxIpLevel(device.Parent()->ChipProperties().gfxLevel),
    m_maxUploadFenceToken(0),
    m_pInternalEvent(nullptr),
    m_computeStateFlags(0),
    m_pDfSpmPerfmonInfo(nullptr),
    m_cmdBufPerfExptFlags{},
#if PAL_BUILD_GFX12
    m_splitBarriers(IsGfx12Plus(m_gfxIpLevel) == false),
#else
    m_splitBarriers(true),
#endif
    m_timestampGpuVa(0),
    m_fceRefCountVec(device.GetPlatform()),
    m_cmdBufState{},
    m_computeState{},
    m_computeRestoreState{},
    m_predGpuAddr(0),
    m_inheritedPredGpuAddr(0),
    m_pCmdStream(pCmdStream),
    m_barrierMgr(barrierMgr),
    m_numActiveQueries{},
    m_acqRelFenceValGpuVa(0),
    m_acqRelFenceVals{},
    m_retiredAcqRelFenceVals{},
    m_executeIndirectV2GlobalSpill(NoExecuteIndirectV2),
    m_globalInternalTableAddr(0)
{
    PAL_ASSERT((createInfo.queueType == QueueTypeUniversal) || (createInfo.queueType == QueueTypeCompute));

    for (uint32 i = 0; i < static_cast<uint32>(QueryPoolType::Count); i++)
    {
        // Marks the specific query as "active," as in it is available to be used.
        // When we need to push state, the queries are no longer active (we deactivate them), but we want to reactivate
        // all of them after we pop state.
        m_queriesActive[i] = true;
    }
}

// =====================================================================================================================
GfxCmdBuffer::~GfxCmdBuffer()
{
    ResetFastClearReferenceCounts();

    PAL_SAFE_FREE(m_computeState.pKernelArguments, m_device.GetPlatform());
    PAL_SAFE_FREE(m_computeRestoreState.pKernelArguments, m_device.GetPlatform());

    ReturnGeneratedCommandChunks(true);

    Device* pDevice = m_device.Parent();

    if (m_pInternalEvent != nullptr)
    {
        m_pInternalEvent->Destroy();
        PAL_SAFE_FREE(m_pInternalEvent, pDevice->GetPlatform());
    }
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
        if (info.pInheritedState != nullptr)
        {
            m_cmdBufState.flags.clientPredicate = info.pInheritedState->stateFlags.predication;
        }
    }

    if (result == Result::Success)
    {
        if (info.pStateInheritCmdBuffer != nullptr)
        {
            InheritStateFromCmdBuf(static_cast<const GfxCmdBuffer*>(info.pStateInheritCmdBuffer));
        }

        if (info.pInheritedState != nullptr)
        {
            m_cmdBufState.flags.packetPredicate = info.pInheritedState->stateFlags.predication;

            if (info.pInheritedState->stateFlags.predication)
            {
                // Allocate the SET_PREDICATION emulation/COND_EXEC memory to be populated by the root-level cmdbuffer
                uint32* pPredCpuAddr = CmdAllocateEmbeddedData(1, 1, &m_predGpuAddr);
                // Save nested cmdbuff pred address in case of future allocations which may rewrite m_predGpuAddr
                m_inheritedPredGpuAddr = m_predGpuAddr;

                // Initialize the COND_EXEC command memory to non-zero, i.e. always execute
                *pPredCpuAddr = 1;
            }
        }

        // If this is a nested command buffer execution, this value should be set to 1
        // pipePoint on nested command buffer cannot be optimized using the state from primary
        if (IsNested() == true)
        {
            SetCpBltState(true);
        }
    }

    return result;
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
                result = pDevice->CreateGpuEvent(createInfo,
                                                 pMemory,
                                                 reinterpret_cast<IGpuEvent**>(&m_pInternalEvent));
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
Result GfxCmdBuffer::Reset(
    ICmdAllocator* pCmdAllocator,
    bool           returnGpuMemory)
{
    ResetFastClearReferenceCounts();

    m_acqRelFenceValGpuVa = 0;
    m_timestampGpuVa      = 0;

    // Do this before our parent class changes the allocator.
    ReturnGeneratedCommandChunks(returnGpuMemory);

    return CmdBuffer::Reset(pCmdAllocator, returnGpuMemory);
}

// =====================================================================================================================
// Completes recording of a command buffer in the building state, making it executable.
// Also ends command buffer dumping, if it is enabled.
Result GfxCmdBuffer::End()
{
    Result result = CmdBuffer::End();

    // NOTE: The root chunk comes from the last command stream in this command buffer because for universal command
    // buffers, the order of command streams is ACE, DE. We always want the "DE" to be the root since the ACE may not
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
void GfxCmdBuffer::ResetState()
{
    CmdBuffer::ResetState();

    m_maxUploadFenceToken        = 0;
    m_cmdBufPerfExptFlags.u32All = 0;
    m_globalInternalTableAddr    = 0;

    m_cmdBufState.flags.u32All           = 0;
    m_cmdBufState.flags.prevCmdBufActive = 1;

    // It's possible that another of our command buffers still has blts in flight, except for CP blts which must be
    // flushed in each command buffer postamble.
    if (IsGraphicsSupported())
    {
        m_cmdBufState.flags.gfxBltActive                       = 1;
        m_cmdBufState.flags.gfxWriteCachesDirty                = 1;
        m_cmdBufState.flags.gfxBltDirectWriteMisalignedMdDirty = 1;
    }

    // It's possible that another of our command buffers still has blts in flight, except for CP blts which must be
    // flushed in each command buffer postamble.
    m_cmdBufState.flags.csBltActive                         = 1;
    m_cmdBufState.flags.csWriteCachesDirty                  = 1;
    m_cmdBufState.flags.csBltDirectWriteMisalignedMdDirty   = 1;
    m_cmdBufState.flags.csBltIndirectWriteMisalignedMdDirty = 1;

#if PAL_BUILD_GFX12
    if (IsGfx12(m_gfxIpLevel))
    {
        // On GFX12, CP is connected to mall/memory directly. CP writes to memory may require a GL2 invalidation.
        m_cmdBufState.flags.cpMemoryWriteL2CacheStale = 1;
    }
    else
#endif
    {
        // PAL sends CP reads and writes through the GL2 by default, we'll need GL2 flushes.
        m_cmdBufState.flags.cpWriteCachesDirty = 1;
    }

    // Command buffers start without a valid predicate GPU address.
    m_predGpuAddr          = 0;
    m_inheritedPredGpuAddr = 0;

    memset(m_acqRelFenceVals, 0, sizeof(m_acqRelFenceVals));
    memset(m_retiredAcqRelFenceVals, 0, sizeof(m_retiredAcqRelFenceVals));

    UpdateGfxBltExecEopFence();
    // Set a impossible waited fence until IssueReleaseSync assigns a meaningful value when sync RB cache.
    UpdateGfxBltWbEopFence(UINT32_MAX);
    UpdateCsBltExecFence();

    PAL_SAFE_FREE(m_computeState.pKernelArguments, m_device.GetPlatform());
    memset(&m_computeState, 0, sizeof(m_computeState));

    PAL_SAFE_FREE(m_computeRestoreState.pKernelArguments, m_device.GetPlatform());
    memset(&m_computeRestoreState, 0, sizeof(m_computeRestoreState));
}

// =====================================================================================================================
void GfxCmdBuffer::DescribeDraw(
    Developer::DrawDispatchType cmdType,
    bool                        includedGangedAce)
{
    RgpMarkerSubQueueFlags subQueueFlags { };
    subQueueFlags.includeMainSubQueue    = 1;
    subQueueFlags.includeGangedSubQueues = includedGangedAce;

    m_device.DescribeDraw(this, subQueueFlags, cmdType, 0, 0, 0);
}

// =====================================================================================================================
void GfxCmdBuffer::DescribeExecuteIndirectCmds(
    GfxCmdBuffer* pCmdBuf,
    uint32        genType)
{
    GeneratorType type = static_cast<GeneratorType>(genType);

    switch (type)
    {
    case GeneratorType::Draw:
        pCmdBuf->DescribeDraw(Developer::DrawDispatchType::CmdGenExecuteIndirectDraw);
        break;
    case GeneratorType::DrawIndexed:
        pCmdBuf->DescribeDraw(Developer::DrawDispatchType::CmdGenExecuteIndirectDrawIndexed);
        break;
    case GeneratorType::Dispatch:
        DescribeDispatch(Developer::DrawDispatchType::CmdGenExecuteIndirectDispatch, {}, {});
        break;
    case GeneratorType::DispatchMesh:
        pCmdBuf->DescribeDraw(Developer::DrawDispatchType::CmdGenExecuteIndirectDispatchMesh);
        break;
    default:
        break;
    }
}

// =====================================================================================================================
void GfxCmdBuffer::DescribeDispatch(
    Developer::DrawDispatchType cmdType,
    DispatchDims                size,
    DispatchInfoFlags           infoFlags)
{
    PAL_ASSERT((cmdType == Developer::DrawDispatchType::CmdDispatch) ||
               (cmdType == Developer::DrawDispatchType::CmdDispatchAce));

    RgpMarkerSubQueueFlags subQueueFlags { };
    if (cmdType == Developer::DrawDispatchType::CmdDispatchAce)
    {
        subQueueFlags.includeGangedSubQueues = 1;
    }
    else
    {
        subQueueFlags.includeMainSubQueue = 1;
    }

    m_device.DescribeDispatch(this, subQueueFlags, cmdType, {}, size, size, infoFlags);
}

// =====================================================================================================================
void GfxCmdBuffer::DescribeDispatchOffset(
    DispatchDims offset,
    DispatchDims launchSize,
    DispatchDims logicalSize)
{
    RgpMarkerSubQueueFlags subQueueFlags { };
    subQueueFlags.includeMainSubQueue = 1;

    m_device.DescribeDispatch(this, subQueueFlags, Developer::DrawDispatchType::CmdDispatchOffset,
                              offset, launchSize, logicalSize, {});
}

// =====================================================================================================================
void GfxCmdBuffer::DescribeDispatchIndirect()
{
    RgpMarkerSubQueueFlags subQueueFlags { };
    subQueueFlags.includeMainSubQueue = 1;

    m_device.DescribeDispatch(this, subQueueFlags, Developer::DrawDispatchType::CmdDispatchIndirect, {}, {}, {}, {});
}

// =====================================================================================================================
bool GfxCmdBuffer::IsAnyUserDataDirty(
    const UserDataEntries* pUserDataEntries)
{
    const size_t* pDirtyMask = &pUserDataEntries->dirty[0];

    size_t dirty = 0;
    for (uint32 i = 0; i < NumUserDataFlagsParts; i++)
    {
        dirty |= pDirtyMask[i];
    }

    return (dirty != 0);
}

// =====================================================================================================================
// Puts command stream related objects into a state ready for command building.
Result GfxCmdBuffer::BeginCommandStreams(
    CmdStreamBeginFlags cmdStreamFlags,
    bool                doReset)
{
    if (doReset)
    {
        ResetFastClearReferenceCounts();
        ReturnGeneratedCommandChunks(true);
    }

    Result result = CmdBuffer::BeginCommandStreams(cmdStreamFlags, doReset);

    if (result == Result::Success)
    {
        // Allocate GPU memory for the internal event from the command allocator.
        result = AllocateAndBindGpuMemToEvent(m_pInternalEvent);
    }

    return result;
}

// =====================================================================================================================
gpusize GfxCmdBuffer::TimestampGpuVirtAddr()
{
    if (m_timestampGpuVa == 0)
    {
        // Allocate timestamp GPU memory from the command allocator.
        // AllocateGpuScratchMem() always returns a valid GPU address, even if we fail to obtain memory from the
        // allocator.  In that scenario, the allocator returns a dummy chunk so we can always have a valid object
        // to access, and sets m_status to a failure code.
        m_timestampGpuVa = AllocateGpuScratchMem(2, 2);
    }

    return m_timestampGpuVa;
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
        if ((m_generatedChunkList.IsEmpty() == false) && (m_flags.autoMemoryReuse == true))
        {
            m_pCmdAllocator->ReuseChunks(LargeEmbeddedDataAlloc, false, m_generatedChunkList.Begin());
        }
    }
    else
    {
        // Reset the chunks to be retained and add them to the retained list. We can only reset them here because
        // of the interface requirement that the client guarantee that no one is using this command stream anymore.
        for (auto iter = m_generatedChunkList.Begin(); iter.IsValid(); iter.Next())
        {
            iter.Get()->Reset();
            m_retainedGeneratedChunkList.PushBack(iter.Get());
        }
    }

    m_generatedChunkList.Clear();
}

// =====================================================================================================================
void GfxCmdBuffer::CmdCopyMemoryByGpuVa(
    gpusize                 srcGpuVirtAddr,
    gpusize                 dstGpuVirtAddr,
    uint32                  regionCount,
    const MemoryCopyRegion* pRegions)
{
    PAL_ASSERT(pRegions != nullptr);

    m_device.RsrcProcMgr().CopyMemoryCs(this,
                                        srcGpuVirtAddr,
                                        *m_device.Parent(),
                                        dstGpuVirtAddr,
                                        *m_device.Parent(),
                                        regionCount,
                                        pRegions,
                                        false,
                                        true,
                                        true);
}

// =====================================================================================================================
void GfxCmdBuffer::CmdCopyMemory(
    const IGpuMemory&       srcGpuMemory,
    const IGpuMemory&       dstGpuMemory,
    uint32                  regionCount,
    const MemoryCopyRegion* pRegions)
{
    m_device.RsrcProcMgr().CmdCopyMemory(this,
                                         static_cast<const GpuMemory&>(srcGpuMemory),
                                         static_cast<const GpuMemory&>(dstGpuMemory),
                                         regionCount,
                                         pRegions);
}

// =====================================================================================================================
void GfxCmdBuffer::CmdUpdateMemory(
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    gpusize           dataSize,
    const uint32*     pData)
{
    PAL_ASSERT(pData != nullptr);
    m_device.RsrcProcMgr().CmdUpdateMemory(this,
                                           static_cast<const GpuMemory&>(dstGpuMemory),
                                           dstOffset,
                                           dataSize,
                                           pData);
}

// =====================================================================================================================
void GfxCmdBuffer::CmdCopyImage(
    const IImage&          srcImage,
    ImageLayout            srcImageLayout,
    const IImage&          dstImage,
    ImageLayout            dstImageLayout,
    uint32                 regionCount,
    const ImageCopyRegion* pRegions,
    const Rect*            pScissorRect,
    uint32                 flags)
{
    PAL_ASSERT(pRegions != nullptr);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 913
    if (m_device.RsrcProcMgr().UseImageCloneCopy(this,
                                                 static_cast<const Image&>(srcImage),
                                                 srcImageLayout,
                                                 static_cast<const Image&>(dstImage),
                                                 dstImageLayout,
                                                 regionCount,
                                                 pRegions,
                                                 flags))
    {
        m_device.RsrcProcMgr().CmdCloneImageData(this,
                                                 static_cast<const Image&>(srcImage),
                                                 static_cast<const Image&>(dstImage));
    }
    else
#endif
    {
        m_device.RsrcProcMgr().CmdCopyImage(this,
                                            static_cast<const Image&>(srcImage),
                                            srcImageLayout,
                                            static_cast<const Image&>(dstImage),
                                            dstImageLayout,
                                            regionCount,
                                            pRegions,
                                            pScissorRect,
                                            flags);
    }
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
            copyRegions[i].swizzledFormat      = {};
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
            copyRegions[i].swizzledFormat      = {};
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
void GfxCmdBuffer::CmdScaledCopyTypedBufferToImage(
    const IGpuMemory&                       srcGpuMemory,
    const IImage&                           dstImage,
    ImageLayout                             dstImageLayout,
    uint32                                  regionCount,
    const TypedBufferImageScaledCopyRegion* pRegions)
{
    PAL_ASSERT(pRegions != nullptr);
    m_device.RsrcProcMgr().CmdScaledCopyTypedBufferToImage(this,
                                                           static_cast<const GpuMemory&>(srcGpuMemory),
                                                           static_cast<const Image&>(dstImage),
                                                           dstImageLayout,
                                                           regionCount,
                                                           pRegions);
}

// =====================================================================================================================
void GfxCmdBuffer::CmdScaledCopyImage(
    const ScaledCopyInfo& copyInfo)
{
    PAL_ASSERT(copyInfo.pRegions != nullptr);
    m_device.RsrcProcMgr().CmdScaledCopyImage(this, copyInfo);
}

// =====================================================================================================================
void GfxCmdBuffer::CmdGenerateMipmaps(
    const GenMipmapsInfo& genInfo)
{
    m_device.RsrcProcMgr().CmdGenerateMipmaps(this, genInfo);
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

        if (image.GetGfxImage()->HasDisplayDccData())
        {
            m_device.UpdateDisplayDcc(this, postProcessInfo, pAddedGpuWork);
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
    const auto& srcImageInfo  = srcImage.GetImageCreateInfo();

    ImageScaledCopyRegion region = {};
    region.srcExtent.width  = srcImageInfo.extent.width;
    region.srcExtent.height = srcImageInfo.extent.height;
    region.srcExtent.depth  = 1;
    region.dstExtent        = region.srcExtent;
    region.dstOffset        = dstOffset;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 887
    region.numSlices        = 1;
#else
    region.srcSlices        = 1;
    region.dstSlices        = 1;
#endif

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

    copyInfo.pSrcImage              = &srcImage;
    copyInfo.srcImageLayout         = srcLayout;
    copyInfo.pDstImage              = &dstImage;
    copyInfo.dstImageLayout         = dstLayout;
    copyInfo.regionCount            = 1;
    copyInfo.pRegions               = &region;
    copyInfo.filter                 = DefaultTexFilter;
    copyInfo.rotation               = ImageRotation::Ccw0;
    copyInfo.pColorKey              = nullptr;
    copyInfo.flags.srcColorKey      = false;
    copyInfo.flags.dstAsSrgb        = false;

    m_device.RsrcProcMgr().CmdScaledCopyImage(this, copyInfo);
}

// =====================================================================================================================
void GfxCmdBuffer::CmdFillMemory(
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    gpusize           fillSize,
    uint32            data)
{
    m_device.RsrcProcMgr().CmdFillMemory(this,
                                         (IsComputeStateSaved() == false),
                                         true,
                                         dstGpuMemory.Desc().gpuVirtAddr + dstOffset,
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
    const IImage&         image,
    ImageLayout           imageLayout,
    const ClearColor&     color,
    const SwizzledFormat& clearFormat,
    uint32                rangeCount,
    const SubresRange*    pRanges,
    uint32                boxCount,
    const Box*            pBoxes,
    uint32                flags)
{
    PAL_ASSERT(pRanges != nullptr);

    uint32 splitRangeCount;
    bool splitMemAllocated          = false;
    const SubresRange* pSplitRanges = nullptr;
    Result result = m_device.Parent()->SplitSubresRanges(rangeCount,
                                                         pRanges,
                                                         &splitRangeCount,
                                                         &pSplitRanges,
                                                         &splitMemAllocated);

    if (result == Result::ErrorOutOfMemory)
    {
        NotifyAllocFailure();
    }
    else if (result == Result::Success)
    {
        m_device.RsrcProcMgr().CmdClearColorImage(this,
                                                  static_cast<const Image&>(image),
                                                  imageLayout,
                                                  color,
                                                  clearFormat,
                                                  splitRangeCount,
                                                  pSplitRanges,
                                                  boxCount,
                                                  pBoxes,
                                                  flags);
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    // Delete memory allocated for splitting the BarrierTransitions if necessary.
    if (splitMemAllocated)
    {
        PAL_SAFE_DELETE_ARRAY(pSplitRanges, m_device.GetPlatform());
    }
}

// =====================================================================================================================
void GfxCmdBuffer::CmdClearBoundDepthStencilTargets(
    float                         depth,
    uint8                         stencil,
    uint8                         stencilWriteMask,
    uint32                        samples,
    uint32                        fragments,
    DepthStencilSelectFlags       flag,
    uint32                        regionCount,
    const ClearBoundTargetRegion* pClearRegions)
{
    m_device.RsrcProcMgr().CmdClearBoundDepthStencilTargets(this,
                                                            depth,
                                                            stencil,
                                                            stencilWriteMask,
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
    uint8              stencilWriteMask,
    uint32             rangeCount,
    const SubresRange* pRanges,
    uint32             rectCount,
    const Rect*        pRects,
    uint32             flags)
{
    PAL_ASSERT((pRanges != nullptr) || (rangeCount == 0));

    uint32 splitRangeCount;
    bool splitMemAllocated          = false;
    const SubresRange* pSplitRanges = nullptr;
    Result result = Result::Success;

#if PAL_BUILD_GFX12
    if (m_gfxIpLevel == GfxIpLevel::GfxIp12)
    {
        splitRangeCount = rangeCount;
        pSplitRanges    = pRanges;
    }
    else
#endif
    {
        result = m_device.Parent()->SplitSubresRanges(rangeCount,
                                                      pRanges,
                                                      &splitRangeCount,
                                                      &pSplitRanges,
                                                      &splitMemAllocated);
    }

    if (result == Result::ErrorOutOfMemory)
    {
        NotifyAllocFailure();
    }
    else if (result == Result::Success)
    {
        m_device.RsrcProcMgr().CmdClearDepthStencil(this,
                                                    static_cast<const Image&>(image),
                                                    depthLayout,
                                                    stencilLayout,
                                                    depth,
                                                    stencil,
                                                    stencilWriteMask,
                                                    splitRangeCount,
                                                    pSplitRanges,
                                                    rectCount,
                                                    pRects,
                                                    flags);
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    // Delete memory allocated for splitting the BarrierTransitions if necessary.
    if (splitMemAllocated)
    {
        PAL_SAFE_DELETE_ARRAY(pSplitRanges, m_device.GetPlatform());
    }
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 910
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
#endif

// =====================================================================================================================
void GfxCmdBuffer::CmdResolveImage(
    const IImage&             srcImage,
    ImageLayout               srcImageLayout,
    const IImage&             dstImage,
    ImageLayout               dstImageLayout,
    ResolveMode               resolveMode,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions,
    uint32                    flags)
{
    PAL_ASSERT(pRegions != nullptr);
    m_device.RsrcProcMgr().CmdResolveImage(this,
                                           static_cast<const Image&>(srcImage),
                                           srcImageLayout,
                                           static_cast<const Image&>(dstImage),
                                           dstImageLayout,
                                           resolveMode,
                                           regionCount,
                                           pRegions,
                                           flags);
}

// =====================================================================================================================
void GfxCmdBuffer::CmdResolvePrtPlusImage(
    const IImage&                    srcImage,
    ImageLayout                      srcImageLayout,
    const IImage&                    dstImage,
    ImageLayout                      dstImageLayout,
    PrtPlusResolveType               resolveType,
    uint32                           regionCount,
    const PrtPlusImageResolveRegion* pRegions)
{
    PAL_ASSERT(pRegions != nullptr);

    const auto&  dstCreateInfo = dstImage.GetImageCreateInfo();
    const auto&  srcCreateInfo = srcImage.GetImageCreateInfo();

    // Either the source or destination image has to be a PRT map image
    if (((resolveType == PrtPlusResolveType::Decode) && (srcCreateInfo.prtPlus.mapType != PrtMapType::None)) ||
        ((resolveType == PrtPlusResolveType::Encode) && (dstCreateInfo.prtPlus.mapType != PrtMapType::None)))
    {
        m_device.RsrcProcMgr().CmdResolvePrtPlusImage(this,
                                                      static_cast<const Image&>(srcImage),
                                                      srcImageLayout,
                                                      static_cast<const Image&>(dstImage),
                                                      dstImageLayout,
                                                      resolveType,
                                                      regionCount,
                                                      pRegions);
    }
}

// =====================================================================================================================
// This cannot be called again until CmdRestoreGraphicsStateInternal is called.
void GfxCmdBuffer::CmdSaveGraphicsState()
{
    PAL_ASSERT(m_cmdBufState.flags.isGfxStatePushed == 0);
    m_cmdBufState.flags.isGfxStatePushed = 1;

    if (m_pCurrentExperiment != nullptr)
    {
        // Inform the performance experiment that we're starting some internal operations.
        m_pCurrentExperiment->BeginInternalOps(m_pCmdStream);
    }
}

// =====================================================================================================================
void GfxCmdBuffer::CmdRestoreGraphicsStateInternal(
    bool trackBltActiveFlags)
{
    PAL_ASSERT(m_cmdBufState.flags.isGfxStatePushed != 0);
    m_cmdBufState.flags.isGfxStatePushed = 0;

    if (m_pCurrentExperiment != nullptr)
    {
        // Inform the performance experiment that we've finished some internal operations.
        m_pCurrentExperiment->EndInternalOps(m_pCmdStream);
    }
}

// =====================================================================================================================
// This cannot be called again until CmdRestoreComputeStateInternal is called.
void GfxCmdBuffer::CmdSaveComputeState(
    uint32 stateFlags)
{
    PAL_ASSERT(IsComputeStateSaved() == false);
    m_computeStateFlags = stateFlags;

    if (m_pCurrentExperiment != nullptr)
    {
        // Inform the performance experiment that we're starting some internal operations.
        m_pCurrentExperiment->BeginInternalOps(m_pCmdStream);
    }

    if (TestAnyFlagSet(stateFlags, ComputeStatePipelineAndUserData))
    {
        // It should be impossible to already have this allocated because we null it out on restore.
        PAL_ASSERT(m_computeRestoreState.pKernelArguments == nullptr);

        // Copy over the bound pipeline and all non-indirect user-data state.
        m_computeRestoreState = m_computeState;

        // In HSA mode we must also duplicate the dynamically allocated current kernel argument buffer.
        if (m_computeState.hsaAbiMode)
        {
            // It's impossible to be in HSA mode without a pipeline.
            PAL_ASSERT(m_computeState.pipelineState.pPipeline != nullptr);

            const auto& pipeline = *static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);
            const uint32 size = pipeline.HsaMetadata().KernargSegmentSize();

            m_computeRestoreState.pKernelArguments =
                static_cast<uint8*>(PAL_MALLOC(size, m_device.GetPlatform(), AllocInternal));

            if (m_computeRestoreState.pKernelArguments == nullptr)
            {
                // Allocation failure, mark buffer as faulty.
                NotifyAllocFailure();
            }
            else
            {
                memcpy(m_computeRestoreState.pKernelArguments, m_computeState.pKernelArguments, size);
            }
        }
    }

    if (TestAnyFlagSet(stateFlags, ComputeStateBorderColorPalette))
    {
        // Copy over the bound border color palette.
        m_computeRestoreState.pipelineState.pBorderColorPalette = m_computeState.pipelineState.pBorderColorPalette;
    }

    // Disable all active queries so that we don't sample internal operations in the app's query pool slots.
    //
    // NOTE: We expect Vulkan won't set this flag because Vulkan allows blits to occur inside nested command buffers.
    // In a nested command buffer, we don't know what value of DB_COUNT_CONTROL to restore because the query state may
    // have been inherited from the calling command buffer. Luckily, Vulkan also states that whether blit or barrier
    // operations affect the results of queries is implementation-defined. So, for symmetry, they should not disable
    // active queries for blits.
    if (m_buildFlags.disableQueryInternalOps)
    {
        DeactivateQueries();
    }
}

// =====================================================================================================================
void GfxCmdBuffer::CmdRestoreComputeStateInternal(
    uint32 stateFlags,
    bool   trackBltActiveFlags)
{
    // Vulkan does allow blits in nested command buffers, but they do not support inheriting user-data values from
    // the caller. Therefore, simply "setting" the restored-state's user-data is sufficient, just like it is in a
    // root command buffer. (If Vulkan decides to support user-data inheritance in a later API version, we'll need
    // to revisit this!)

    SetComputeState(m_computeRestoreState, stateFlags);

    // We may have allocated this if we saved while in HSA mode. It makes things simpler if we just free it now.
    PAL_SAFE_FREE(m_computeRestoreState.pKernelArguments, m_device.GetPlatform());

    PAL_ASSERT(TestAllFlagsSet(m_computeStateFlags, stateFlags));
    m_computeStateFlags = 0;

    if (m_pCurrentExperiment != nullptr)
    {
        // Inform the performance experiment that we've finished some internal operations.
        m_pCurrentExperiment->EndInternalOps(m_pCmdStream);
    }

     // Reactivate all queries that we stopped in CmdSaveComputeState.
    if (m_buildFlags.disableQueryInternalOps)
    {
        ReactivateQueries();
    }

    // No need track blt active flags (expect trackBltActiveFlags == false) for below cases:
    //  1. CmdRestoreComputeState() call from PAL clients.
    //  2. CmdRestoreComputeState() call from auto sync clear case.
    if (trackBltActiveFlags)
    {
        // The caller has just executed one or more CS blts.
        SetCsBltState(true);
        SetCsBltWriteCacheState(true);

        UpdateCsBltExecFence();

#if PAL_DEVELOPER_BUILD
        Developer::RpmBltData cbData = { .pCmdBuffer = this, .bltType = Developer::RpmBltType::Dispatch };
        m_device.Parent()->DeveloperCb(Developer::CallbackType::RpmBlt, &cbData);
#endif
    }
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

// =====================================================================================================================
// Compares the client-specified user data update parameters against the current user data values, and filters any
// redundant updates at the beginning or ending of the range.
//
// Redundant values in the middle of a partially redundant range are not filtered;
// HWL's that definitely don't mind potentially more holes in the dirty mask can use the newer UpdateUserData functions.
//
// The most common redundant filtered updates are setting 2-dword addresses (best hit rate on high bits) and
// 4-dword buffer SRDs (best hit rate on last dword).
//
// Returns true if there are still entries that should be processed after filtering.  False means that the entire set
// is redundant.
bool GfxCmdBuffer::FilterSetUserData(
    UserDataArgs*        pUserDataArgs,
    const uint32*        pEntries,
    const UserDataFlags& userDataFlags)
{
    uint32        firstEntry   = pUserDataArgs->firstEntry;
    uint32        entryCount   = pUserDataArgs->entryCount;
    const uint32* pEntryValues = pUserDataArgs->pEntryValues;

    // Adjust the start entry and entry value pointer for any redundant entries found at the beginning of the range.
    while ((entryCount > 0) &&
           (*pEntryValues == pEntries[firstEntry]) &&
           WideBitfieldIsSet(userDataFlags, firstEntry))
    {
        firstEntry++;
        pEntryValues++;
        entryCount--;
    }

    bool result = false;
    if (entryCount > 0)
    {
        // Search from the end of the range for the last non-redundant entry.  We are guaranteed to find one since the
        // earlier loop found at least one non-redundant entry.
        uint32 idx = entryCount - 1;
        while ((pEntryValues[idx] == pEntries[firstEntry + idx]) &&
               WideBitfieldIsSet(userDataFlags, firstEntry + idx))
        {
            idx--;
        }

        // Update the caller's values.
        pUserDataArgs->firstEntry   = firstEntry;
        pUserDataArgs->entryCount   = idx + 1;
        pUserDataArgs->pEntryValues = pEntryValues;

        result = true;
    }

    return result;
}

// =====================================================================================================================
void GfxCmdBuffer::CmdBindPipeline(
    const PipelineBindParams& params)
{
    // Try to catch users who try to bind graphics pipelines to compute command buffers.
    PAL_DEBUG_BUILD_ONLY_ASSERT((params.pipelineBindPoint == PipelineBindPoint::Compute) || IsGraphicsSupported());

    const auto* const pPipeline = static_cast<const Pipeline*>(params.pPipeline);

    if (params.pipelineBindPoint == PipelineBindPoint::Compute)
    {
        m_computeState.pipelineState.pPipeline  = pPipeline;
        m_computeState.pipelineState.apiPsoHash = params.apiPsoHash;
        m_computeState.pipelineState.dirtyFlags.pipeline = 1;

        m_computeState.dynamicCsInfo = params.cs;
        m_computeState.hsaAbiMode    = (pPipeline != nullptr) && (pPipeline->GetInfo().flags.hsaAbi == 1);

        // It's simplest to always free the kernel args buffer and allocate a new one with the proper size if needed.
        PAL_SAFE_FREE(m_computeState.pKernelArguments, m_device.GetPlatform());

        if (m_computeState.hsaAbiMode)
        {
            // HSA mode overwrites the user-data SGPRs. The easiest way to force user-data validation when we return
            // to PAL mode is to mark all user-data values that have ever been set as dirty.
            memcpy(m_computeState.csUserDataEntries.dirty,
                   m_computeState.csUserDataEntries.touched,
                   sizeof(m_computeState.csUserDataEntries.dirty));

            const ComputePipeline& pipeline            = *static_cast<const ComputePipeline*>(pPipeline);
            const HsaAbi::CodeObjectMetadata& metadata = pipeline.HsaMetadata();

            // We're callocing here on purpose because some HSA ABI arguments need to use zero by default.
            m_computeState.pKernelArguments = static_cast<uint8*>(PAL_CALLOC(metadata.KernargSegmentSize(),
                                                                             m_device.GetPlatform(),
                                                                             AllocInternal));

            if (m_computeState.pKernelArguments == nullptr)
            {
                // Allocation failure, mark buffer as faulty.
                NotifyAllocFailure();
            }
        }
    }

    m_device.DescribeBindPipeline(this, pPipeline, params.apiPsoHash, params.pipelineBindPoint);

    if (pPipeline != nullptr)
    {
        m_maxUploadFenceToken = Max(m_maxUploadFenceToken, pPipeline->GetUploadFenceToken());
        m_lastPagingFence     = Max(m_lastPagingFence, pPipeline->GetPagingFenceVal());
    }
}

// =====================================================================================================================
void GfxCmdBuffer::CmdDuplicateUserData(
    PipelineBindPoint source,
    PipelineBindPoint dest)
{
    PAL_ASSERT(source != PipelineBindPoint::Graphics);
    PAL_ASSERT(source != dest);

    const UserDataEntries& sourceEntries = m_computeState.csUserDataEntries;

    CmdSetUserData(dest, 0, MaxUserDataEntries, sourceEntries.entries);
}

// =====================================================================================================================
void GfxCmdBuffer::CmdBarrier(
    const BarrierInfo& barrierInfo)
{
    CmdBuffer::CmdBarrier(barrierInfo);

    // Barriers do not honor predication.
    const uint32 packetPredicate = m_cmdBufState.flags.packetPredicate;
    m_cmdBufState.flags.packetPredicate = 0;

    // Mark these as traditional barriers in RGP
    m_barrierMgr.DescribeBarrierStart(this, barrierInfo.reason, Developer::BarrierType::Full);

    Developer::BarrierOperations barrierOps = {};

    if (m_splitBarriers)
    {
        bool splitMemAllocated;
        BarrierInfo splitBarrierInfo = barrierInfo;
        Result result = GfxBarrierMgr::SplitBarrierTransitions(m_device.GetPlatform(),
                                                               &splitBarrierInfo,
                                                               &splitMemAllocated);

        if (result == Result::Success)
        {
            m_barrierMgr.Barrier(this, splitBarrierInfo, &barrierOps);
        }
        else if (result == Result::ErrorOutOfMemory)
        {
            NotifyAllocFailure();
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }

        // Delete memory allocated for splitting the BarrierTransitions if necessary.
        if (splitMemAllocated)
        {
            PAL_SAFE_DELETE_ARRAY(splitBarrierInfo.pTransitions, m_device.GetPlatform());
        }
    }
    else
    {
        m_barrierMgr.Barrier(this, barrierInfo, &barrierOps);
    }

    m_barrierMgr.DescribeBarrierEnd(this, &barrierOps);

    m_cmdBufState.flags.packetPredicate = packetPredicate;
}

// =====================================================================================================================
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 885
uint32 GfxCmdBuffer::CmdRelease(
#else
ReleaseToken GfxCmdBuffer::CmdRelease(
#endif
    const AcquireReleaseInfo& releaseInfo)
{
    CmdBuffer::CmdRelease(releaseInfo);

    // Barriers do not honor predication.
    const uint32 packetPredicate = m_cmdBufState.flags.packetPredicate;
    m_cmdBufState.flags.packetPredicate = 0;

    // Mark these as traditional barriers in RGP
    m_barrierMgr.DescribeBarrierStart(this, releaseInfo.reason, Developer::BarrierType::Release);

    Developer::BarrierOperations barrierOps = {};
    ReleaseToken                 syncToken  = {};

    if (m_splitBarriers)
    {
        bool splitMemAllocated;
        AcquireReleaseInfo splitReleaseInfo = releaseInfo;
        Result result = GfxBarrierMgr::SplitImgBarriers(m_device.GetPlatform(), &splitReleaseInfo, &splitMemAllocated);

        if (result == Result::Success)
        {
            syncToken = m_barrierMgr.Release(this, splitReleaseInfo, &barrierOps);
        }
        else if (result == Result::ErrorOutOfMemory)
        {
            NotifyAllocFailure();
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }

        // Delete memory allocated for splitting ImgBarriers if necessary.
        if (splitMemAllocated)
        {
            PAL_SAFE_DELETE_ARRAY(splitReleaseInfo.pImageBarriers, m_device.GetPlatform());
        }
    }
    else
    {
        syncToken = m_barrierMgr.Release(this, releaseInfo, &barrierOps);
    }

    m_barrierMgr.DescribeBarrierEnd(this, &barrierOps);

    m_cmdBufState.flags.packetPredicate = packetPredicate;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 885
    return syncToken.u32All;
#else
    return syncToken;
#endif
}

// =====================================================================================================================
void GfxCmdBuffer::CmdAcquire(
    const AcquireReleaseInfo& acquireInfo,
    uint32                    syncTokenCount,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 885
    const uint32*             pSyncTokens)
#else
    const ReleaseToken*       pSyncTokens)
#endif
{
    CmdBuffer::CmdAcquire(acquireInfo, syncTokenCount, pSyncTokens);

    // Barriers do not honor predication.
    const uint32 packetPredicate = m_cmdBufState.flags.packetPredicate;
    m_cmdBufState.flags.packetPredicate = 0;

    // Mark these as traditional barriers in RGP
    m_barrierMgr.DescribeBarrierStart(this, acquireInfo.reason, Developer::BarrierType::Acquire);

    Developer::BarrierOperations barrierOps = {};

    if (m_splitBarriers)
    {
        bool splitMemAllocated;
        AcquireReleaseInfo splitAcquireInfo = acquireInfo;
        Result result = GfxBarrierMgr::SplitImgBarriers(m_device.GetPlatform(), &splitAcquireInfo, &splitMemAllocated);

        if (result == Result::Success)
        {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 885
            m_barrierMgr.Acquire(this, splitAcquireInfo, syncTokenCount,
                                   reinterpret_cast<const ReleaseToken*>(pSyncTokens), &barrierOps);
#else
            m_barrierMgr.Acquire(this, splitAcquireInfo, syncTokenCount, pSyncTokens, &barrierOps);
#endif
        }
        else if (result == Result::ErrorOutOfMemory)
        {
            NotifyAllocFailure();
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }

        // Delete memory allocated for splitting ImgBarriers if necessary.
        if (splitMemAllocated)
        {
            PAL_SAFE_DELETE_ARRAY(splitAcquireInfo.pImageBarriers, m_device.GetPlatform());
        }
    }
    else
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 885
        m_barrierMgr.Acquire(this, acquireInfo, syncTokenCount,
                                reinterpret_cast<const ReleaseToken*>(pSyncTokens), &barrierOps);
#else
        m_barrierMgr.Acquire(this, acquireInfo, syncTokenCount, pSyncTokens, &barrierOps);
#endif
    }

    m_barrierMgr.DescribeBarrierEnd(this, &barrierOps);

    m_cmdBufState.flags.packetPredicate = packetPredicate;
}

// =====================================================================================================================
void GfxCmdBuffer::CmdReleaseEvent(
    const AcquireReleaseInfo& releaseInfo,
    const IGpuEvent*          pGpuEvent)
{
    CmdBuffer::CmdReleaseEvent(releaseInfo, pGpuEvent);

    // Barriers do not honor predication.
    const uint32 packetPredicate = m_cmdBufState.flags.packetPredicate;
    m_cmdBufState.flags.packetPredicate = 0;

    // Mark these as traditional barriers in RGP
    m_barrierMgr.DescribeBarrierStart(this, releaseInfo.reason, Developer::BarrierType::Release);

    Developer::BarrierOperations barrierOps = {};

    if (m_splitBarriers)
    {
        bool splitMemAllocated;
        AcquireReleaseInfo splitReleaseInfo = releaseInfo;
        Result result = GfxBarrierMgr::SplitImgBarriers(m_device.GetPlatform(), &splitReleaseInfo, &splitMemAllocated);

        if (result == Result::Success)
        {
            m_barrierMgr.ReleaseEvent(this, splitReleaseInfo, pGpuEvent, &barrierOps);
        }
        else if (result == Result::ErrorOutOfMemory)
        {
            NotifyAllocFailure();
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }

        // Delete memory allocated for splitting ImgBarriers if necessary.
        if (splitMemAllocated)
        {
            PAL_SAFE_DELETE_ARRAY(splitReleaseInfo.pImageBarriers, m_device.GetPlatform());
        }
    }
    else
    {
        m_barrierMgr.ReleaseEvent(this, releaseInfo, pGpuEvent, &barrierOps);
    }

    m_barrierMgr.DescribeBarrierEnd(this, &barrierOps);

    m_cmdBufState.flags.packetPredicate = packetPredicate;
}

// =====================================================================================================================
void GfxCmdBuffer::CmdAcquireEvent(
    const AcquireReleaseInfo& acquireInfo,
    uint32                    gpuEventCount,
    const IGpuEvent* const*   ppGpuEvents)
{
    CmdBuffer::CmdAcquireEvent(acquireInfo, gpuEventCount, ppGpuEvents);

    // Barriers do not honor predication.
    const uint32 packetPredicate = m_cmdBufState.flags.packetPredicate;
    m_cmdBufState.flags.packetPredicate = 0;

    // Mark these as traditional barriers in RGP
    m_barrierMgr.DescribeBarrierStart(this, acquireInfo.reason, Developer::BarrierType::Acquire);

    Developer::BarrierOperations barrierOps = {};

    if (m_splitBarriers)
    {
        bool splitMemAllocated;
        AcquireReleaseInfo splitAcquireInfo = acquireInfo;
        Result result = GfxBarrierMgr::SplitImgBarriers(m_device.GetPlatform(), &splitAcquireInfo, &splitMemAllocated);

        if (result == Result::Success)
        {
            m_barrierMgr.AcquireEvent(this, splitAcquireInfo, gpuEventCount, ppGpuEvents, &barrierOps);
        }
        else if (result == Result::ErrorOutOfMemory)
        {
            NotifyAllocFailure();
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }

        // Delete memory allocated for splitting ImgBarriers if necessary.
        if (splitMemAllocated)
        {
            PAL_SAFE_DELETE_ARRAY(splitAcquireInfo.pImageBarriers, m_device.GetPlatform());
        }
    }
    else
    {
        m_barrierMgr.AcquireEvent(this, acquireInfo, gpuEventCount, ppGpuEvents, &barrierOps);
    }

    m_barrierMgr.DescribeBarrierEnd(this, &barrierOps);

    m_cmdBufState.flags.packetPredicate = packetPredicate;
}

// =====================================================================================================================
void GfxCmdBuffer::CmdReleaseThenAcquire(
    const AcquireReleaseInfo& barrierInfo)
{
    CmdBuffer::CmdReleaseThenAcquire(barrierInfo);

    // Barriers do not honor predication.
    const uint32 packetPredicate = m_cmdBufState.flags.packetPredicate;
    m_cmdBufState.flags.packetPredicate = 0;

    // Mark these as traditional barriers in RGP
    m_barrierMgr.DescribeBarrierStart(this, barrierInfo.reason, Developer::BarrierType::Full);

    Developer::BarrierOperations barrierOps = {};

    if (m_splitBarriers)
    {
        bool splitMemAllocated;
        AcquireReleaseInfo splitBarrierInfo = barrierInfo;
        Result result = GfxBarrierMgr::SplitImgBarriers(m_device.GetPlatform(), &splitBarrierInfo, &splitMemAllocated);

        if (result == Result::Success)
        {
            m_barrierMgr.ReleaseThenAcquire(this, splitBarrierInfo, &barrierOps);
        }
        else if (result == Result::ErrorOutOfMemory)
        {
            NotifyAllocFailure();
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }

        // Delete memory allocated for splitting ImgBarriers if necessary.
        if (splitMemAllocated)
        {
            PAL_SAFE_DELETE_ARRAY(splitBarrierInfo.pImageBarriers, m_device.GetPlatform());
        }
    }
    else
    {
        m_barrierMgr.ReleaseThenAcquire(this, barrierInfo, &barrierOps);
    }

    m_barrierMgr.DescribeBarrierEnd(this, &barrierOps);

    m_cmdBufState.flags.packetPredicate = packetPredicate;
}

// =====================================================================================================================
void GfxCmdBuffer::CmdResolveQuery(
    const IQueryPool& queryPool,
    QueryResultFlags  flags,
    QueryType         queryType,
    uint32            startQuery,
    uint32            queryCount,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    gpusize           dstStride)
{
    // Resolving a query is not supposed to honor predication.
    const uint32 packetPredicate = m_cmdBufState.flags.packetPredicate;
    m_cmdBufState.flags.packetPredicate = 0;
    m_device.RsrcProcMgr().CmdResolveQuery(this,
                                           static_cast<const QueryPool&>(queryPool),
                                           flags,
                                           queryType,
                                           startQuery,
                                           queryCount,
                                           static_cast<const GpuMemory&>(dstGpuMemory),
                                           dstOffset,
                                           dstStride);

    m_cmdBufState.flags.packetPredicate = packetPredicate;
}

// =====================================================================================================================
// Begins recording performance data using the specified Experiment object.
void GfxCmdBuffer::CmdBeginPerfExperiment(
    IPerfExperiment* pPerfExperiment)
{
    const PerfExperiment*const pExperiment = static_cast<PerfExperiment*>(pPerfExperiment);
    PAL_ASSERT(pExperiment != nullptr);

    // Preemption needs to be disabled during any perf experiment for accuracy.
    m_pCmdStream->DisablePreemption();

    // Indicates that this command buffer is used for enabling a perf experiment. This is used to write any VCOPs that
    // may be needed during submit time.
    const PerfExperimentFlags tracesEnabled = pExperiment->TracesEnabled();
    m_cmdBufPerfExptFlags.u32All |= tracesEnabled.u32All;

    pExperiment->IssueBegin(this, m_pCmdStream);
    if (tracesEnabled.perfCtrsEnabled || tracesEnabled.spmTraceEnabled)
    {
        m_cmdBufState.flags.perfCounterStarted = 1;
        m_cmdBufState.flags.perfCounterStopped = 0;
    }
    if (tracesEnabled.sqtTraceEnabled)
    {
        m_cmdBufState.flags.sqttStarted = 1;
        m_cmdBufState.flags.sqttStopped = 0;
    }
    if (tracesEnabled.dfSpmTraceEnabled)
    {
        // Cache a pointer to the DF SPM Perfmon Info so we can access it at submit time
        const DfSpmPerfmonInfo* pDfSpmPerfmonInfo = pExperiment->GetDfSpmPerfmonInfo();
        // We only support 1 DF perf experiment per command buffer.
        PAL_ASSERT((m_pDfSpmPerfmonInfo == nullptr) || (pDfSpmPerfmonInfo == m_pDfSpmPerfmonInfo));
        if (m_pDfSpmPerfmonInfo == nullptr)
        {
            m_pDfSpmPerfmonInfo = pDfSpmPerfmonInfo;
        }
    }

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

    // Preemption needs to be disabled during any perf experiment for accuracy.
    m_pCmdStream->DisablePreemption();
    pExperiment->UpdateSqttTokenMask(m_pCmdStream, sqttTokenConfig);
}

// =====================================================================================================================
void GfxCmdBuffer::CmdEndPerfExperiment(
    IPerfExperiment* pPerfExperiment)
{
    const PerfExperiment*const pExperiment = static_cast<PerfExperiment*>(pPerfExperiment);
    PAL_ASSERT(pPerfExperiment != nullptr);

    // Normally, we should only be ending the currently bound perf experiment opened in this command buffer.  However,
    // when gathering full-frame SQ thread traces, an experiment could be opened in one command buffer and ended in
    // another.
    PAL_ASSERT((pPerfExperiment == m_pCurrentExperiment) || (m_pCurrentExperiment == nullptr));

    // Preemption needs to be disabled during any perf experiment for accuracy.
    m_pCmdStream->DisablePreemption();

    pExperiment->IssueEnd(this, m_pCmdStream);

    const PerfExperimentFlags tracesEnabled = pExperiment->TracesEnabled();
    if (tracesEnabled.perfCtrsEnabled || tracesEnabled.spmTraceEnabled)
    {
        m_cmdBufState.flags.perfCounterStopped = 1;
    }
    if (tracesEnabled.sqtTraceEnabled)
    {
        m_cmdBufState.flags.sqttStopped = 1;
    }

    m_pCurrentExperiment = nullptr;
}

// =====================================================================================================================
void GfxCmdBuffer::CmdSetKernelArguments(
    uint32            firstArg,
    uint32            argCount,
    const void*const* ppValues)
{
    // It's illegal to call this function without an HSA ABI pipeline bound.
    PAL_ASSERT(m_computeState.hsaAbiMode && (m_computeState.pipelineState.pPipeline != nullptr));
    PAL_ASSERT(m_computeState.pKernelArguments != nullptr);

    const ComputePipeline& pipeline = *static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);
    const HsaAbi::CodeObjectMetadata& metadata = pipeline.HsaMetadata();

    if (firstArg + argCount > metadata.NumArguments())
    {
        PAL_ASSERT_ALWAYS_MSG("Kernel argument count is off! More arguments than expected");
        // Verify that we won't go out of bounds.
        SetCmdRecordingError(Result::ErrorInvalidValue);
    }
    else
    {
        for (uint32 idx = 0; idx < argCount; ++idx)
        {
            const HsaAbi::KernelArgument& arg = metadata.Arguments()[firstArg + idx];
            memcpy(m_computeState.pKernelArguments + arg.offset, ppValues[idx], arg.size);
        }
    }
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
void GfxCmdBuffer::AddFceSkippedImageCounter(
    GfxImage* pGfxImage)
{
    PAL_ASSERT(pGfxImage != nullptr);
    PAL_ASSERT(pGfxImage->IsFceOptimizationEnabled());

    const Result result = m_fceRefCountVec.PushBack(pGfxImage->GetFceRefCounter());
    if (result != Result::Success)
    {
        SetCmdRecordingError(result);
    }

    pGfxImage->IncrementFceRefCount();
}

// =====================================================================================================================
// Function which is called when  this CmdBuffer submission contains an ExecuteIndirect V2 PM4 that requires a
// GlobalSpillBuffer allocation made by the driver. If it's Task+Mesh EI an allocation needs to be made for the Compute
// and the Universal Queue.
void GfxCmdBuffer::SetExecuteIndirectV2GlobalSpill(
    bool hasTask)
{
    // If the first EI has task and subsequent EI calls don't. We still need to allocate the GlobalSpill on ACE.
    // So this value should not be overwritten.
    if (m_executeIndirectV2GlobalSpill != ContainsExecuteIndirectV2WithTask)
    {
        if (hasTask)
        {
            m_executeIndirectV2GlobalSpill = ContainsExecuteIndirectV2WithTask;
        }
        else
        {
            m_executeIndirectV2GlobalSpill = ContainsExecuteIndirectV2;
        }
    }
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
            bindParams.apiPsoHash         = newComputeState.pipelineState.apiPsoHash;

            CmdBindPipeline(bindParams);
        }

        // We're only supposed to save/restore kernel args in HSA mode and user-data in PAL mode.
        if (m_computeState.hsaAbiMode)
        {
            // It's impossible to be in HSA mode without a pipeline.
            PAL_ASSERT(m_computeState.pipelineState.pPipeline != nullptr);

            // By now the current pKernelArguments must have the same size as the saved original. We must memcpy it
            // because this function is used in places where we can't just assume ownership of the saved copy's buffer.
            const auto& pipeline = *static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);
            const uint32 size    = pipeline.HsaMetadata().KernargSegmentSize();

            memcpy(m_computeState.pKernelArguments, newComputeState.pKernelArguments, size);
        }
        else
        {
            CmdSetUserData(PipelineBindPoint::Compute,
                           0,
                           m_device.Parent()->ChipProperties().gfxip.maxUserDataEntries,
                           &newComputeState.csUserDataEntries.entries[0]);
        }
    }

    if (TestAnyFlagSet(stateFlags, ComputeStateBorderColorPalette) &&
        (newComputeState.pipelineState.pBorderColorPalette != m_computeState.pipelineState.pBorderColorPalette))
    {
        CmdBindBorderColorPalette(PipelineBindPoint::Compute, newComputeState.pipelineState.pBorderColorPalette);
    }
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
        pDestPipelineState->dirtyFlags.borderColorPalette = 1;
    }

    if (leakedPipelineState.pPipeline != nullptr)
    {
        pDestPipelineState->pPipeline = leakedPipelineState.pPipeline;
        pDestPipelineState->dirtyFlags.pipeline = 1;
    }

    for (uint32 index = 0; index < NumUserDataFlagsParts; ++index)
    {
        pDestUserDataEntries->dirty[index]   |= leakedUserDataEntries.dirty[index];
        pDestUserDataEntries->touched[index] |= leakedUserDataEntries.touched[index];

        auto mask = leakedUserDataEntries.touched[index];
        for (uint32 bit : BitIterSizeT(mask))
        {
            const uint32 entry = (bit + (UserDataEntriesPerMask * index));
            pDestUserDataEntries->entries[entry] = leakedUserDataEntries.entries[entry];
        }
    }
}

// =====================================================================================================================
bool GfxCmdBuffer::OptimizeAcqRelReleaseInfo(
    BarrierType   barrierType,
    const IImage* pImage,
    uint32*       pSrcStageMask,
    uint32*       pSrcAccessMask,
    uint32*       pDstStageMask,
    uint32*       pDstAccessMask
    ) const
{
    PAL_ASSERT((pSrcAccessMask != nullptr) && (pDstAccessMask != nullptr));

    const bool isClearToTarget = GfxBarrierMgr::IsClearToTargetTransition(*pSrcAccessMask, *pDstAccessMask);

    m_barrierMgr.OptimizeStageMask(this, barrierType, pSrcStageMask, pDstStageMask, isClearToTarget);

    return m_barrierMgr.OptimizeAccessMask(this,
                                           barrierType,
                                           static_cast<const Image*>(pImage),
                                           pSrcAccessMask,
                                           pDstAccessMask,
                                           true);
}

// =====================================================================================================================
// Updates a user-data table managed by embedded data & CPU updates.
void GfxCmdBuffer::UpdateUserDataTableCpu(
    UserDataTableState* pTable,
    uint32              dwordsNeeded,
    uint32              offsetInDwords,
    const uint32*       pSrcData,       // In: Data representing the *full* contents of the table, not just the part
                                        // between offsetInDwords and dwordsNeeded.
    uint32              alignmentInDwords)
{
    // The dwordsNeeded and offsetInDwords parameters together specify a "window" of the table which is relevant to
    // the active pipeline.  To save memory as well as cycles spent copying data, this will only allocate and populate
    // the portion of the user-data table inside that window.
    PAL_DEBUG_BUILD_ONLY_ASSERT((dwordsNeeded + offsetInDwords) <= pTable->sizeInDwords);

    // User-data can contain inline constant buffers which, for historical reasons, are defined in 4x32-bit chunks in
    // HLSL but are only DWORD size-aligned in the user-data layout. This means the following can occur:
    // 1. The app compiles a shader with 2 DWORDs in a constant buffer. The HLSL compiler implicitly pads the size
    //    of the constant buffer out to 4 DWORDs to meet the constant buffer size alignment rule. It also emits DXIL
    //    instructions which load a vector of 4 DWORDs from the constant buffer even though it will only use 2 values.
    // 2. The app defines a root signature which only contains 2 constants. The app is not required to add padding to
    //    the root signature. Accessing past the end of the root constants is defined to be undefined behavior.
    // Given that the input DXIL code instructs us to load 4 DWORDs, our compiled shader will do just that if the root
    // constants are spilled to memory. The values of those extra 2 DWORDs will be ignored but they are still read.
    // This can cause a GPU page fault if we get unlucky and the constant buffer padding falls in unmapped GPU memory.
    //
    // Page faulting is legal in this case but it's not at all user-friendly. We can avoid the page fault if we align
    // our table's base address to a multiple of 4 DWORDs. If each 4x32-bit load occurs on a 4x32-bit aligned address
    // it's impossible for part of that load to address unmapped memory.
    //
    // Aligning all tables to 4 DWORDs isn't expected to waste much memory so for simplictly we do it for all clients.
    // It should only matter if we interleave 1-3 DWORD embedded data allocations with table allocations many times,
    // such that this command buffer must allocate an additional embedded data chunk.
    const uint32 cbAlignment = Max(alignmentInDwords, 4u);

    gpusize gpuVirtAddr  = 0uLL;
    pTable->pCpuVirtAddr = (CmdAllocateEmbeddedData(dwordsNeeded, cbAlignment, &gpuVirtAddr) - offsetInDwords);
    pTable->gpuVirtAddr  = (gpuVirtAddr - (sizeof(uint32) * offsetInDwords));

    // There's technically a bug in the above table address calculation. We only write the low 32-bits of the table
    // address to user-data and assume the high bits are always the same. This is usually the case because we allocate
    // embedded data from a single 4GB virtual address range, but because we subtract the table offset from the real
    // vitual address we could underflow out of our fixed 4GB address range. This wouldn't be a problem if we sent the
    // full address to the GPU, but because the shader code infers the top 32 bits we can accidentally round up by 4GB.
    // This assert exists to detect this case at runtime.
    //
    // It's not that easy to fix this issue, we have two routes and neither seem attractive:
    // 1. Stop computing invalid pointers. This is probably the most correct solution but it's also the most difficult
    //    because we have an implicit contract with multiple compilers that the table pointer starts at offset zero.
    // 2. Define a maximum offset value and reserve enough VA space at the beginning of the VA range to ensure that we
    //    can never allocate embeded data in the range that can underflow. This will waste VA space and seems hacky.
    PAL_DEBUG_BUILD_ONLY_ASSERT(HighPart(gpuVirtAddr) == HighPart(pTable->gpuVirtAddr));

    // Optimize the loop with a memcpy if dwordsNeeded is large enough ( 6 DWORDs is the threshold measured )
    uint32* pDstData = (pTable->pCpuVirtAddr + offsetInDwords);
    pSrcData += offsetInDwords;
    if (dwordsNeeded >= 6)
    {
        memcpy(pDstData, pSrcData, dwordsNeeded * sizeof(uint32));
    }
    else
    {
        for (uint32 i = 0; i < dwordsNeeded; ++i)
        {
            *pDstData = *pSrcData;
            ++pDstData;
            ++pSrcData;
        }
    }

    // Mark that the latest contents of the user-data table have been uploaded to the current embedded data chunk.
    pTable->dirty = 0;
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
// Copies entryCount DWORDs from pEntryValues to pState->entries starting at firstEntry.
// The corresponding bit(s) in pState->touched are set to 1.
// Does redundant filtering: an entry's dirty bit is set only if the entry was untouched or had a different value.
void GfxCmdBuffer::UpdateUserData(
    UserDataEntries* pState,
    uint32           firstEntry,
    uint32           entryCount,
    const uint32*    pEntryValues)
{
    for (uint32 i = 0; i < entryCount; ++i)
    {
        const uint32 newValue       = pEntryValues[i];
        const uint32 userDataIndex  = firstEntry + i;
        const uint32 wordIndex      = userDataIndex / UserDataEntriesPerMask;
        const uint32 bitIndexInWord = userDataIndex % UserDataEntriesPerMask;
        const size_t flag           = size_t(1) << bitIndexInWord;
        const size_t touched        = pState->touched[wordIndex];
        const bool   bDiff          = (pState->entries[userDataIndex] != newValue);

        pState->entries[userDataIndex] = newValue;
        pState->dirty[wordIndex] |= (size_t(bDiff) << bitIndexInWord) | (~touched & flag);
        pState->touched[wordIndex] = touched | flag;
    }
}

// =====================================================================================================================
void PAL_STDCALL GfxCmdBuffer::CmdUpdateUserDataCs(
    ICmdBuffer*   pCmdBuffer,
    uint32        firstEntry,
    uint32        entryCount,
    const uint32* pEntryValues)
{
    auto* pThis = static_cast<GfxCmdBuffer*>(pCmdBuffer);
    UpdateUserData(&pThis->m_computeState.csUserDataEntries, firstEntry, entryCount, pEntryValues);
}

// =====================================================================================================================
// Helper function for updating user data entries and tracking flags common to different pipeline types. Specializes
// updating a single user data entry as well as WideBitfieldSetBit* functions to set two UserDataFlags bitmasks.
void GfxCmdBuffer::SetUserData(
    uint32           firstEntry,
    uint32           entryCount,
    UserDataEntries* pEntries,
    const uint32*    pEntryValues)
{
    uint32 index       = (firstEntry / UserDataEntriesPerMask);
    uint32 startingBit = firstEntry & (UserDataEntriesPerMask - 1);

    if (entryCount == 1)
    {
        // Equivalent to WideBitfieldSetBit for both touched and dirty bitmasks
        const size_t mask  = (static_cast<size_t>(1) << startingBit);

        pEntries->touched[index] |= mask;
        pEntries->dirty[index]   |= mask;

        pEntries->entries[firstEntry] = pEntryValues[0];
    }
    else
    {
        // Equivalent to WideBitfieldSetRange for both touched and dirty bitmasks
        uint32 numBits = entryCount;

        while (numBits > 0)
        {
            const uint32 maxNumBits = UserDataEntriesPerMask - startingBit;
            const uint32 curNumBits = (maxNumBits < numBits) ? maxNumBits : numBits;
            const size_t bitMask    = static_cast<size_t>(-1) >> (UserDataEntriesPerMask - curNumBits);

            pEntries->touched[index] |= (bitMask << startingBit);
            pEntries->dirty[index]   |= (bitMask << startingBit);

            index++;
            startingBit  = 0;
            numBits     -= curNumBits;
        }

        memcpy(&pEntries->entries[firstEntry], pEntryValues, entryCount * sizeof(uint32));
    }
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

    auto* const pThis    = static_cast<GfxCmdBuffer*>(pCmdBuffer);
    auto* const pEntries = &pThis->m_computeState.csUserDataEntries;

    // It's illegal to bind user-data when in HSA ABI mode.
    PAL_ASSERT(pThis->m_computeState.hsaAbiMode == false);

    // NOTE: Compute operations are expected to be far rarer than graphics ones, so at the moment it is not expected
    // that filtering-out redundant compute user-data updates is worthwhile.
    // For HWL's that definitely don't mind potentially more holes in the dirty mask, the newer UpdateUserData
    // functions can be used if redundant filtering is desired.
    SetUserData(firstEntry, entryCount, pEntries, pEntryValues);
}

// =====================================================================================================================
// Returns a new chunk by first searching the retained chunk list for a valid chunk then querying the command allocator
// if there are no retained chunks available.
CmdStreamChunk* GfxCmdBuffer::GetNextGeneratedChunk()
{
    CmdStreamChunk* pChunk = nullptr;

    if (m_status == Result::Success)
    {
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
            m_status = m_pCmdAllocator->GetNewChunk(LargeEmbeddedDataAlloc, false, &pChunk);

            // Something bad happen and the GfxCmdBuffer will always be in error status ever after
            PAL_ALERT(m_status != Result::Success);
        }
    }

    // If we fail to get a new Chunk from GPU memory either because we ran out of GPU memory or DeviceLost, get a dummy
    // chunk to allow the program to proceed until the error is progagated back to the client.
    if (m_status != Result::Success)
    {
        pChunk = m_pCmdAllocator->GetDummyChunk();
        pChunk->Reset();

        // Make sure there is only one reference of dummy chunk at back of chunk list
        if (m_generatedChunkList.Back() == pChunk)
        {
            m_generatedChunkList.PopBack(nullptr);
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
void GfxCmdBuffer::CopyHsaKernelArgsToMem(
    const DispatchDims&               offset,
    const DispatchDims&               threads,
    const DispatchDims&               logicalSize,
    gpusize*                          pKernArgsGpuVa,
    uint32*                           pLdsSize,
    const HsaAbi::CodeObjectMetadata& metadata)
{
    // Copy the kernel argument buffer into GPU memory.
    const uint32 allocSize      = NumBytesToNumDwords(metadata.KernargSegmentSize());
    const uint32 allocAlign     = NumBytesToNumDwords(metadata.KernargSegmentAlign());
    uint8* const pParams        = reinterpret_cast<uint8*>(
        CmdAllocateEmbeddedData(allocSize, allocAlign, pKernArgsGpuVa));
    const uint16 threadsX       = static_cast<uint16>(threads.x);
    const uint16 threadsY       = static_cast<uint16>(threads.y);
    const uint16 threadsZ       = static_cast<uint16>(threads.z);
    uint16 remainderSize        = 0; // no incomplete workgroups supported at this time.
    const uint32 dimensionality = (logicalSize.x > 1) + (logicalSize.y > 1) + (logicalSize.z > 1);
    uint64 ldsPointer           = 0;

    memcpy(pParams, m_computeState.pKernelArguments, metadata.KernargSegmentSize());

    // The global offsets are always zero, except in CmdDispatchOffset where they are dispatch-time values.
    // This could be moved out into CmdDispatchOffset if the overhead is too much but we'd have to return
    // out some extra state to make that work.

    // Phase 1: Fill out driver-controlled arguments which don't depend on other arguments.
    bool hasPhase2Args = false;

    for (uint32 idx = 0; idx < metadata.NumArguments(); ++idx)
    {
        const HsaAbi::KernelArgument& arg = metadata.Arguments()[idx];
        switch (arg.valueKind)
        {
        case HsaAbi::ValueKind::HiddenGlobalOffsetX:
            memcpy(pParams + arg.offset, &offset.x, Min<size_t>(sizeof(offset.x), arg.size));
            break;
        case HsaAbi::ValueKind::HiddenGlobalOffsetY:
            memcpy(pParams + arg.offset, &offset.y, Min<size_t>(sizeof(offset.y), arg.size));
            break;
        case HsaAbi::ValueKind::HiddenGlobalOffsetZ:
            memcpy(pParams + arg.offset, &offset.z, Min<size_t>(sizeof(offset.z), arg.size));
            break;
        case HsaAbi::ValueKind::HiddenBlockCountX:
            memcpy(pParams + arg.offset, &logicalSize.x, Min<size_t>(sizeof(logicalSize.x), arg.size));
            break;
        case HsaAbi::ValueKind::HiddenBlockCountY:
            memcpy(pParams + arg.offset, &logicalSize.y, Min<size_t>(sizeof(logicalSize.y), arg.size));
            break;
        case HsaAbi::ValueKind::HiddenBlockCountZ:
            memcpy(pParams + arg.offset, &logicalSize.z, Min<size_t>(sizeof(logicalSize.z), arg.size));
            break;
        case HsaAbi::ValueKind::HiddenGroupSizeX:
            memcpy(pParams + arg.offset, &threadsX, Min<size_t>(sizeof(threadsX), arg.size));
            break;
        case HsaAbi::ValueKind::HiddenGroupSizeY:
            memcpy(pParams + arg.offset, &threadsY, Min<size_t>(sizeof(threadsY), arg.size));
            break;
        case HsaAbi::ValueKind::HiddenGroupSizeZ:
            memcpy(pParams + arg.offset, &threadsZ, Min<size_t>(sizeof(threadsZ), arg.size));
            break;
        case HsaAbi::ValueKind::HiddenRemainderX:
            memcpy(pParams + arg.offset, &remainderSize, Min<size_t>(sizeof(remainderSize), arg.size));
            break;
        case HsaAbi::ValueKind::HiddenRemainderY:
            memcpy(pParams + arg.offset, &remainderSize, Min<size_t>(sizeof(remainderSize), arg.size));
            break;
        case HsaAbi::ValueKind::HiddenRemainderZ:
            memcpy(pParams + arg.offset, &remainderSize, Min<size_t>(sizeof(remainderSize), arg.size));
            break;
        case HsaAbi::ValueKind::HiddenGridDims:
            memcpy(pParams + arg.offset, &dimensionality, Min<size_t>(sizeof(dimensionality), arg.size));
            break;
        case HsaAbi::ValueKind::HiddenDynamicLdsSize:
            // We need to visit all DynamicSharedPointer args before we can fill out the dynamic LDS size.
            hasPhase2Args = true;
            break;
        case HsaAbi::ValueKind::DynamicSharedPointer:
            // We only support dyamic LDS pointers.
            PAL_ASSERT(arg.addressSpace == HsaAbi::AddressSpace::Local);

            // PAL must place the dynamic LDS allocations after the static LDS allocations. We need some extra
            // padding between allocations if the current ldsSize isn't already aligned to pointeeAlign.
            *pLdsSize = Pow2Align(*pLdsSize, arg.pointeeAlign);

            // Pad the pointer out to 64 bits just in case arg.size is 8 bytes.
            ldsPointer = *pLdsSize;
            memcpy(pParams + arg.offset, &ldsPointer, Min<size_t>(sizeof(ldsPointer), arg.size));

            // The caller must set each dynamic LDS pointer to the length of its dynamic allocation.
            // We must add these to our ldsSize so we know where the next dynamic pointer goes in LDS space.
            *pLdsSize += *reinterpret_cast<const uint32*>(m_computeState.pKernelArguments + arg.offset);
            break;
        case HsaAbi::ValueKind::ByValue:
        case HsaAbi::ValueKind::GlobalBuffer:
        case HsaAbi::ValueKind::Image:
            break; // these are handled by kernargs
        case HsaAbi::ValueKind::HiddenNone:
            break; // avoid the assert in this case
        case HsaAbi::ValueKind::HiddenQueuePtr:
        case HsaAbi::ValueKind::HiddenDefaultQueue:
        case HsaAbi::ValueKind::HiddenCompletionAction:
        case HsaAbi::ValueKind::HiddenHostcallBuffer:
            // Not supported by PAL, kernels request but never actually use them,
            // as compiler can't optimized out them for some cases
            break;
        default:
            PAL_ASSERT_ALWAYS();

        }
    }

    PAL_ASSERT_MSG(*pLdsSize <= m_device.Parent()->ChipProperties().gfxip.ldsSizePerThreadGroup,
        "LDS size exceeds the maximum size per thread group.");

    if (hasPhase2Args)
    {
        // Phase 2: Fill out any arguments which depend on the values we computed in phase 1.
        const uint32 dynamicLdsSize = *pLdsSize - metadata.GroupSegmentFixedSize();

        for (uint32 idx = 0; idx < metadata.NumArguments(); ++idx)
        {
            const HsaAbi::KernelArgument& arg = metadata.Arguments()[idx];
            switch (arg.valueKind)
            {
            case HsaAbi::ValueKind::HiddenDynamicLdsSize:
                memcpy(pParams + arg.offset, &dynamicLdsSize, Min<size_t>(sizeof(dynamicLdsSize), arg.size));
                break;
            default:
                // We should assume all unspecified args require no phase 2 handling.
                break;
            }
        }
    }
}

} // Pal
