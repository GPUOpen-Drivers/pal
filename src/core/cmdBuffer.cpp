/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/cmdBuffer.h"
#include "core/device.h"
#include "core/gpuEvent.h"
#include "core/platform.h"
#include "core/perfExperiment.h"
#include "core/queue.h"
#include "palAutoBuffer.h"
#include "palLinearAllocator.h"
#include "palSysUtil.h"
#include "palVectorImpl.h"

using namespace Util;

namespace Pal
{

/// Defines the contents of the default color-space-conversion table used for converting YUV data to RGB data.
const ColorSpaceConversionTable DefaultCscTableYuvToRgb =
{{
    {  1.164f,  0.0f,    1.596f, -0.875f, },
    {  1.164f, -0.392f, -0.813f,  0.532f, },
    {  1.164f,  2.017f,  0.0f,   -1.086f, },
}};

/// Defines the contents of the default color-space-conversion table used for converting RGB data to YUV data.
const ColorSpaceConversionTable DefaultCscTableRgbToYuv =
{{
    {  0.257f,  0.504f,  0.098f,  0.063f, },
    { -0.148f, -0.291f,  0.439f,  0.502f, },
    {  0.439f, -0.368f, -0.071f,  0.502f, },
}};

static void PAL_STDCALL CmdDrawInvalid(
    ICmdBuffer* pCmdBuffer,
    uint32      firstVertex,
    uint32      vertexCount,
    uint32      firstInstance,
    uint32      instanceCount,
    uint32      drawId);
static void PAL_STDCALL CmdDrawOpaqueInvalid(
    ICmdBuffer* pCmdBuffer,
    gpusize     streamOutFilledSizeVa,
    uint32      streamOutOffset,
    uint32      stride,
    uint32      firstInstance,
    uint32      instanceCount);
static void PAL_STDCALL CmdDrawIndexedInvalid(
    ICmdBuffer* pCmdBuffer,
    uint32      firstIndex,
    uint32      indexCount,
    int32       vertexOffset,
    uint32      firstInstance,
    uint32      instanceCount,
    uint32      drawId);
static void PAL_STDCALL CmdDrawIndirectMultiInvalid(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            stride,
    uint32            maximumCount,
    gpusize           countGpuAddr);
static void PAL_STDCALL CmdDrawIndexedIndirectMultiInvalid(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            stride,
    uint32            maximumCount,
    gpusize           countGpuAddr);
static void PAL_STDCALL CmdDispatchInvalid(ICmdBuffer* pCmdBuffer, DispatchDims size);
static void PAL_STDCALL CmdDispatchIndirectInvalid(ICmdBuffer* pCmdBuffer, const IGpuMemory& gpuMemory, gpusize offset);
static void PAL_STDCALL CmdDispatchOffsetInvalid(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims offset,
    DispatchDims launchSize,
    DispatchDims logicalSize);

uint32 CmdBuffer::s_numCreated[QueueTypeCount] = {};

// =====================================================================================================================
CmdBuffer::CmdBuffer(
    const Device&              device,
    const CmdBufferCreateInfo& createInfo)
    :
    m_createInfo(createInfo),
    m_engineType(createInfo.engineType),
    m_pCmdAllocator(static_cast<CmdAllocator*>(createInfo.pCmdAllocator)),
    m_pMemAllocator(nullptr),
    m_pMemAllocatorStartPos(nullptr),
    m_status(Result::Success),
    m_executionMarkerAddr(0),
    m_executionMarkerCount(0),
    m_embeddedData(device.GetPlatform()),
    m_gpuScratchMem(device.GetPlatform()),
    m_gpuScratchMemAllocLimit(0),
    m_lastPagingFence(0),
    m_p2pBltWaInfo(device.GetPlatform()),
    m_p2pBltWaLastChunkAddr(0),
    m_implicitGangSubQueueCount(0),
    m_device(device),
    m_recordState(CmdBufferRecordState::Reset) ,
    m_uniqueId(0),
    m_numCmdBufsBegun(0),
    m_ib2DumpInfos(device.GetPlatform())
{
    m_buildFlags.u32All = 0;
    m_flags.u32All      = 0;

    // Initialize all draw/dispatch funcs to invalid stubs.  HWIP command buffer classes that support these interfaces
    // will overwrite the function pointers.
    m_funcTable.pfnCmdDraw                      = CmdDrawInvalid;
    m_funcTable.pfnCmdDrawOpaque                = CmdDrawOpaqueInvalid;
    m_funcTable.pfnCmdDrawIndexed               = CmdDrawIndexedInvalid;
    m_funcTable.pfnCmdDrawIndirectMulti         = CmdDrawIndirectMultiInvalid;
    m_funcTable.pfnCmdDrawIndexedIndirectMulti  = CmdDrawIndexedIndirectMultiInvalid;
    m_funcTable.pfnCmdDispatch                  = CmdDispatchInvalid;
    m_funcTable.pfnCmdDispatchIndirect          = CmdDispatchIndirectInvalid;
    m_funcTable.pfnCmdDispatchOffset            = CmdDispatchOffsetInvalid;
    m_funcTable.pfnCmdDispatchDynamic           = CmdDispatchDynamicInvalid;
    m_funcTable.pfnCmdDispatchMesh              = CmdDispatchMeshInvalid;
    m_funcTable.pfnCmdDispatchMeshIndirectMulti = CmdDispatchMeshIndirectMultiInvalid;
}

// =====================================================================================================================
CmdBuffer::~CmdBuffer()
{
    ReturnLinearAllocator();
    ReturnDataChunks(&m_embeddedData, EmbeddedDataAlloc, true);
    ReturnDataChunks(&m_gpuScratchMem, GpuScratchMemAlloc, true);
}

// =====================================================================================================================
// Destroys an internal command buffer object: invokes the destructor and frees the system memory block it resides in.
void CmdBuffer::DestroyInternal()
{
    Platform*const pPlatform = m_device.GetPlatform();
    Destroy();
    PAL_FREE(this, pPlatform);
}

// =====================================================================================================================
Result CmdBuffer::Init(
    const CmdBufferInternalCreateInfo& internalInfo)
{
    m_internalInfo.flags.u32all = internalInfo.flags.u32all;

    if (m_pCmdAllocator != nullptr)
    {
        m_gpuScratchMemAllocLimit = (m_pCmdAllocator->ChunkSize(GpuScratchMemAlloc) / sizeof(uint32));
    }

    // Set the bit based on m_pCmdAllocator autoMemoryReuse bit value.
    m_flags.autoMemoryReuse = (m_pCmdAllocator != nullptr) && (m_pCmdAllocator->AutomaticMemoryReuse());

    Result result = Reset(nullptr, true);

    if (result == Result::Success)
    {
        m_uniqueId = AtomicIncrement(&s_numCreated[static_cast<size_t>(GetQueueType())]);
    }

    return result;
}

// =====================================================================================================================
// Resets the command buffer's previous contents and state, then puts it into a building state allowing new commands
// to be recorded.
Result CmdBuffer::Begin(
    const CmdBufferBuildInfo& info)
{
    Result result = Result::Success;

    // Must have a valid command allocator specified either at creation or at reset.
    if (m_pCmdAllocator == nullptr)
    {
        result = Result::ErrorBuildingCommandBuffer;
    }

    if (result == Result::Success)
    {
        // Don't allow an already-begun command buffer to be begun again
        if (m_recordState == CmdBufferRecordState::Building)
        {
            result = Result::ErrorIncompleteCommandBuffer;
        }
        else
        {
            const PalSettings& settings = m_device.Settings();

            // Assemble our building flags for this command building session.
            m_buildFlags = info.flags;

            if (settings.cmdBufForceOneTimeSubmit == CmdBufForceOneTimeSubmit::CmdBufForceOneTimeSubmitOn)
            {
                m_buildFlags.optimizeOneTimeSubmit = 1;
            }
            else if (settings.cmdBufForceOneTimeSubmit == CmdBufForceOneTimeSubmit::CmdBufForceOneTimeSubmitOff)
            {
                m_buildFlags.optimizeOneTimeSubmit = 0;
            }

            // One time submit implies exclusive submit. In the rest of the driver we will check for exclusive submit
            // instead of checking both flags.
            if (m_buildFlags.optimizeOneTimeSubmit == 1)
            {
                m_buildFlags.optimizeExclusiveSubmit = 1;
            }

            // Disallowing this command buffer to be launched via an IB2 packet is meaningless for root level command
            // buffers.
            if (IsNested() == false)
            {
                m_buildFlags.disallowNestedLaunchViaIb2 = 0;
            }
            else if (settings.cmdBufDisallowNestedLaunchViaIb2)
            {
                m_buildFlags.disallowNestedLaunchViaIb2 = 1;
            }

            // Obtain a linear allocator for this command building session. It should be impossible for us to have a
            // non-null linear allocator at this time.
            PAL_ASSERT(m_pMemAllocator == nullptr);

            // Use the client's external memory allocator if possible, otherwise ask the command allocator for one.
            m_flags.internalMemAllocator = (info.pMemAllocator == nullptr);
            m_pMemAllocator              = m_flags.internalMemAllocator ? m_pCmdAllocator->GetNewLinearAllocator()
                                                                        : info.pMemAllocator;
            if (m_pMemAllocator == nullptr)
            {
                // We must have failed to allocate an internal memory allocator, we can't recover from this.
                result = Result::ErrorOutOfMemory;
            }
            else
            {
                // Remember the current location of the allocator, we will rewind to this spot when we return it.
                m_pMemAllocatorStartPos = m_pMemAllocator->Current();
            }

            if (result == Result::Success)
            {
                CmdStreamBeginFlags cmdStreamflags = {};
                cmdStreamflags.prefetchCommands = m_buildFlags.prefetchCommands;
                cmdStreamflags.optimizeCommands =
                    (((settings.cmdBufOptimizePm4 == Pm4OptDefaultEnable) && m_buildFlags.optimizeGpuSmallBatch) ||
                     (settings.cmdBufOptimizePm4 == Pm4OptForceEnable));

                // If the app explicitly called "reset" on this command buffer, there's no need to do another reset
                // on the command streams.
                result = BeginCommandStreams(cmdStreamflags, m_recordState != CmdBufferRecordState::Reset);
            }

            if (result == Result::Success)
            {
                m_implicitGangSubQueueCount = 0;
                m_p2pBltWaInfo.Clear();

                // Reset and initialize all internal state before we start building commands.
                ResetState();

                result = AddPreamble();
            }

            if (result == Result::Success)
            {
                m_recordState = CmdBufferRecordState::Building;

                // Don't really need to do this unless PM4 dumping has been enabled in the settings, but it takes
                // longer to determine if its necessary than to just increment the variable.
                m_numCmdBufsBegun++;
            }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 757
            if (settings.disableQueryInternalOps == true)
            {
                m_buildFlags.disableQueryInternalOps = 1;
            }
#endif
        }
    }

    if ((result == Result::Success) && (IsDumpingEnabled()))
    {
        char filename[MaxFilenameLength] = {};

        GetCmdBufDumpFilename(filename, MaxFilenameLength * sizeof(char));
        OpenCmdBufDumpFile(&filename[0]);
    }

    return result;
}

// =====================================================================================================================
// Returns all embedded data if we must do a reset.
Result CmdBuffer::BeginCommandStreams(
    CmdStreamBeginFlags cmdStreamFlags,
    bool                doReset)
{
    if (doReset)
    {
        // NOTE: PAL does not currently support retaining command buffer chunks when doing an implicit reset
        ReturnDataChunks(&m_embeddedData, EmbeddedDataAlloc, true);
        ReturnDataChunks(&m_gpuScratchMem, GpuScratchMemAlloc, true);
    }

    return Result::Success;
}

// =====================================================================================================================
// Completes recording of a command buffer in the building state, making it executable.
Result CmdBuffer::End()
{
    Result result = m_status;

    if (result != Result::Success)
    {
        // Nothing to do, just report the result to caller.
    }
    else if (m_recordState == CmdBufferRecordState::Building)
    {
        result = AddPostamble();

        // Update the last paging fence to reflect that of the command allocator and of all nested command buffers
        // called by this command buffer (if any).
        UpdateLastPagingFence(m_pCmdAllocator->LastPagingFence());

        // NOTE: The root chunk comes from the last command stream in this command buffer because for universal command
        // buffers, the order of command streams is CE, DE. We always want the "DE" to be the root since the CE may not
        // have any commands, depending on what operations get recorded to the command buffer.
        const Pal::CmdStream*const pCmdStream = GetCmdStream(NumCmdStreams() - 1);

        if (pCmdStream->GetNumChunks() > 0)
        {
            CmdStreamChunk*const pRootChunk = pCmdStream->GetFirstChunk();

            // Update the embedded data chunks with the correct root chunk reference.
            for (auto iter = m_embeddedData.chunkList.Begin(); iter.IsValid(); iter.Next())
            {
                iter.Get()->UpdateRootInfo(pRootChunk);
            }

            // Update the GPU scratch-memory chunks with the correct root chunk reference.
            for (auto iter = m_gpuScratchMem.chunkList.Begin(); iter.IsValid(); iter.Next())
            {
                iter.Get()->UpdateRootInfo(pRootChunk);
            }

        }

        if (result == Result::Success)
        {
            m_recordState = CmdBufferRecordState::Executable;
        }
    }
    else
    {
        result = Result::ErrorIncompleteCommandBuffer;
    }

    // Regardless of our result rewind and return our linear allocator to avoid leaking memory.
    ReturnLinearAllocator();

    return result;
}

// =====================================================================================================================
// Explicitly resets a command buffer, releasing any internal resources associated with it and putting it in the reset
// state.
Result CmdBuffer::Reset(
    ICmdAllocator* pCmdAllocator,
    bool           returnGpuMemory)
{
    m_recordState          = CmdBufferRecordState::Reset;
    m_lastPagingFence      = 0;

    m_executionMarkerCount = 0;
    m_executionMarkerAddr  = 0;

    // reset the dumpInfos
    m_ib2DumpInfos.Clear();

    // We must attempt to return our linear allocator in the case that the client reset this command buffer while it was
    // in the building state. In normal operation this call will do nothing and take no locks.
    ReturnLinearAllocator();

    ReturnDataChunks(&m_embeddedData, EmbeddedDataAlloc, returnGpuMemory);
    ReturnDataChunks(&m_gpuScratchMem, GpuScratchMemAlloc, returnGpuMemory);

    m_status = Result::Success;
    if ((pCmdAllocator != nullptr) && (pCmdAllocator != m_pCmdAllocator))
    {
        // It is illegal to retain data chunks when changing allocators
        if (returnGpuMemory == false)
        {
            m_status = Result::ErrorInvalidValue;
            PAL_ASSERT_ALWAYS();
        }
        else
        {
            m_pCmdAllocator           = static_cast<CmdAllocator*>(pCmdAllocator);
            m_gpuScratchMemAllocLimit = (m_pCmdAllocator->ChunkSize(GpuScratchMemAlloc) / sizeof(uint32));

            // Update the autoMemoryReuse bit by the new cmdAllocator
            m_flags.autoMemoryReuse   = m_pCmdAllocator->AutomaticMemoryReuse();

        }
    }

    return m_status;
}

// =====================================================================================================================
// Returns a new chunk by first searching the retained chunk list for a valid chunk then querying the command allocator
// if there are no retained chunks available.
CmdStreamChunk* CmdBuffer::GetNextDataChunk(
    CmdAllocType type,
    ChunkData*   pData,
    uint32       numDwords)
{
    CmdStreamChunk* pChunk = nullptr;

    if (m_status == Result::Success)
    {
        // First search the retained chunk list
        if (pData->retainedChunks.IsEmpty() == false)
        {
            // The command allocator always allocates uniformly-sized chunk, so any retained chunk should be big enough.
            // When the chunk was retained the reference count was not modified so no need to add a reference here.
            pData->retainedChunks.PopBack(&pChunk);
        }

        // If a retained chunk could not be found then allocate a new one from the command allocator
        if (pChunk == nullptr)
        {
            // It's either the first time we're requesting space for this stream, or the "most recent" chunk for this
            // stream doesn't have enough space to accomodate this request.  Either way, we need to obtain a new chunk.
            // The allocator adds a reference for us automatically. Data chunks cannot be root (head) chunks.
            m_status = m_pCmdAllocator->GetNewChunk(type, false, &pChunk);

            // Something bad happen and the CmdBuffer will always be in error status ever after
            PAL_ALERT(m_status != Result::Success);
        }
    }

    // If we fail to get a new Chunk from GPU memory either because we ran out of GPU memory or DeviceLost, get a dummy
    // chunk to allow the program to proceed until the error is propagated back to the client.
    if (m_status != Result::Success)
    {
        pChunk = m_pCmdAllocator->GetDummyChunk();
        pChunk->Reset();

        // Make sure there is only one reference of dummy chunk at back of chunk list
        if (pData->chunkList.Back() == pChunk)
        {
            pData->chunkList.PopBack(nullptr);
        }
    }

    // We have to have a chunk at this point
    PAL_ASSERT(pChunk != nullptr);

    // add this chunk to the end of our list.
    const Result result = pData->chunkList.PushBack(pChunk);
    PAL_ASSERT(result == Result::Success);

    // Embedded data chunks can't be executed so we shouldn't have created a busy tracker.
    PAL_ASSERT(pChunk->DwordsRemaining() == pChunk->SizeDwords());

    // Remember how much of this chunk is available.
    pData->chunkDwordsAvailable = pChunk->DwordsRemaining();

    // It's possible for a client to request more command buffer space then what fits in a single chunk.
    // This is unsupported.
    PAL_ASSERT(numDwords <= pData->chunkDwordsAvailable);

    return pChunk;
}

// =====================================================================================================================
// Returns a chunk that can accomodate the specified number of dwords for the specified data type. A new chunk will be
// allocated if necessary.
CmdStreamChunk* CmdBuffer::GetDataChunk(
    CmdAllocType type,
    ChunkData*   pData,
    uint32       numDwords)
{
    CmdStreamChunk* pChunk = nullptr;

    if (numDwords > pData->chunkDwordsAvailable)
    {
        pChunk = GetNextDataChunk(type, pData, numDwords);
    }
    else
    {
        // Ok, the chunk at the end of our chunk list has room to support this request, so just use that.
        pChunk = pData->chunkList.Back();
    }

    return pChunk;
}

// =====================================================================================================================
uint32* CmdBuffer::CmdAllocateEmbeddedData(
    uint32   sizeInDwords,
    uint32   alignmentInDwords,
    gpusize* pGpuAddress)
{
    gpusize offset      = 0;
    GpuMemory* pGpuMem  = nullptr;
    uint32* pSpace      = CmdAllocateEmbeddedData(sizeInDwords, alignmentInDwords, &pGpuMem, &offset);
    *pGpuAddress        = pGpuMem->Desc().gpuVirtAddr + offset;

    return pSpace;
}

// =====================================================================================================================
// Returns the GPU memory object pointer that can accomodate the specified number of dwords of the embedded data.
// The offset of the embedded data to the allocated memory is also returned.
// This call is only used by PAL internally and should be called when running in physical mode.
// A new chunk will be allocated if necessary.
uint32* CmdBuffer::CmdAllocateEmbeddedData(
    uint32      sizeInDwords,
    uint32      alignmentInDwords,
    GpuMemory** ppGpuMem,
    gpusize*    pOffset)
{
    // The size of an aligned embedded data allocation can change per chunk. That means we might need to compute the
    // size twice here if GetChunk gets a new chunk from the command allocator.
    CmdStreamChunk*const pOldChunk = m_embeddedData.chunkList.IsEmpty() ? GetEmbeddedDataChunk(1) :
                                                                          m_embeddedData.chunkList.Back();
    const uint32 embeddedDataLimitDwords = GetEmbeddedDataLimit();

    // Caller to this function should make sure the requested size is not larger than limitation.
    // Since this function does not have logic to provide multiple chunks for the request.
    PAL_ASSERT(sizeInDwords <= embeddedDataLimitDwords);

    uint32 alignedSizeInDwords = pOldChunk->ComputeSpaceSize(sizeInDwords, alignmentInDwords);

    // The address alignment operation above may generate a alignedSizeInDwords as
    // embeddedDataLimit < alignedSizeInDwords < embeddedDataLimit + alignmentInDwords
    // For example, if the chunk has used 9 DW and chunk size is 100 DW, when the sizeInDW is 100 and aligment is 8,
    // ComputeSpaceSize() generate correct alignedSizeInDwords as 107.
    // However, in this case, we cannot directly use 107 as input parameter later since it it over the limit
    // of the embedded data chunk size. If this case happens, it means sizeInDwords is larger than
    // embeddedDataLimit - alignmentInDwords. So it is safe and proper to just use embeddedDataLimit as the
    // the requested aligned data size. The reason is, both 107 and 100 will make the embedded chunk finding function
    // to grab a chunk that has nothing written to it yet. and both value should be >= sizeInDwords, which is the
    // requested size provided by the caller of this function.
    if (alignedSizeInDwords > embeddedDataLimitDwords)
    {
        alignedSizeInDwords = embeddedDataLimitDwords;
    }

    CmdStreamChunk*const pNewChunk = GetEmbeddedDataChunk(alignedSizeInDwords);
    if (pNewChunk != pOldChunk)
    {
        // The previously active chunk didn't have enough space left, compute the size again using the new chunk.
        alignedSizeInDwords = pNewChunk->ComputeSpaceSize(sizeInDwords, alignmentInDwords);
    }
    PAL_ASSERT(alignedSizeInDwords <= m_embeddedData.chunkDwordsAvailable);

    // Record that the tail object in our chunk list has less space available than it did before.
    m_embeddedData.chunkDwordsAvailable -= alignedSizeInDwords;

    const uint32 alignmentOffsetInDwords = alignedSizeInDwords - sizeInDwords;
    gpusize      allocationOffset        = 0;
    uint32*      pSpace                  = pNewChunk->GetSpace(alignedSizeInDwords, ppGpuMem, &allocationOffset) +
                                           alignmentOffsetInDwords;

    *pOffset = allocationOffset + (alignmentOffsetInDwords * sizeof(uint32));

    return pSpace;
}

// =====================================================================================================================
// Allocates a small piece of local-invisible GPU memory for internal PAL operations, such as CE RAM dumps, etc.  This
// will result in pulling a new chunk from the command allocator if necessary.  This memory has the same lifetime as the
// embedded data allocations and the command buffer itself.
// Returns the GPU memory object pointer that can accomodate the specified number of dwords of the scratch memory.
// The offset of the allocated scratch memory to the scratch GpuMemory starting address is also returned.
gpusize CmdBuffer::AllocateGpuScratchMem(
    uint32      sizeInDwords,
    uint32      alignmentInDwords,
    GpuMemory** ppGpuMem,
    gpusize*    pOffset)
{
    // The size of an aligned data allocation can change per chunk.  We may need to compute the size twice if this
    // call results in pulling a new chunk from the allocator.
    CmdStreamChunk*const pOldChunk = m_gpuScratchMem.chunkList.IsEmpty()
        ? GetDataChunk(GpuScratchMemAlloc, &m_gpuScratchMem, 1)
        : m_gpuScratchMem.chunkList.Back();

    // Caller to this function should make sure the requested size is not larger than the limit.
    PAL_ASSERT(sizeInDwords <= m_gpuScratchMemAllocLimit);

    uint32 alignedSizeInDwords = pOldChunk->ComputeSpaceSize(sizeInDwords, alignmentInDwords);
    // If aligning the requested size bumps us up over the allocation limit, just use the limit itself as the
    // requested size.  This works because it will force the chunk list to pull a new chunk from the allocator
    // and that will be guaranteed to fit since the beginning of each chunk is larger than the maximum expected
    // alignment.
    if (alignedSizeInDwords > m_gpuScratchMemAllocLimit)
    {
        alignedSizeInDwords = m_gpuScratchMemAllocLimit;
    }

    CmdStreamChunk*const pNewChunk = GetDataChunk(GpuScratchMemAlloc, &m_gpuScratchMem, alignedSizeInDwords);
    if (pNewChunk != pOldChunk)
    {
        // The previously active chunk didn't have enough space left, compute the size again using the new chunk.
        alignedSizeInDwords = pNewChunk->ComputeSpaceSize(sizeInDwords, alignmentInDwords);
    }
    PAL_ASSERT(alignedSizeInDwords <= m_gpuScratchMem.chunkDwordsAvailable);

    // Record that the tail object in our chunk list has less space available than it did before.
    m_gpuScratchMem.chunkDwordsAvailable -= alignedSizeInDwords;

    const uint32 alignmentOffsetInDwords = alignedSizeInDwords - sizeInDwords;
    gpusize      allocationOffset        = 0;
    uint32*const pUnused                 = pNewChunk->GetSpace(alignedSizeInDwords, ppGpuMem, &allocationOffset);

    *pOffset = allocationOffset + (alignmentOffsetInDwords * sizeof(uint32));

    return ((*ppGpuMem)->Desc().gpuVirtAddr + (*pOffset));
}

// =====================================================================================================================
// Get memory from scratch memory chunk and bind to GPU event. Scratch memory is in Invisible heap, so the event is GPU
// access only. Hence client is responsible for resetting the event from GPU, and cannot call Set(), Reset(),
// GetStatus().
Result CmdBuffer::AllocateAndBindGpuMemToEvent(
    IGpuEvent* pGpuEvent)
{
    PAL_ASSERT(pGpuEvent != nullptr);

    // For now only GpuEventPool and CmdBuffer's internal GpuEvent use this path to bind GPU memory. These usecases
    // assume the event is GPU access only. So it's fine to directly allocate scratch memory heap for the event instead
    // of choosing heap based on the heap requirement.
    PAL_ASSERT((static_cast<GpuEvent*>(pGpuEvent))->IsGpuAccessOnly());

    GpuMemoryRequirements gpuMemReqs = { };
    pGpuEvent->GetGpuMemoryRequirements(&gpuMemReqs);

    const uint32 sizeInDwords      = static_cast<uint32>(gpuMemReqs.size / sizeof(uint32));
    const uint32 alignmentInDwords = static_cast<uint32>(gpuMemReqs.alignment / sizeof(uint32));
    GpuMemory*   pGpuMem           = nullptr;
    gpusize      offset            = 0;

    const gpusize unusedGpuAddr = AllocateGpuScratchMem(sizeInDwords, alignmentInDwords, &pGpuMem, &offset);

    // AllocateGpuScratchMem() always returns a valid GPU address, even if we fail to obtain memory from the allocator.
    // In that scenario, the allocator returns a dummy chunk so we can always have a valid object to access, and sets
    // m_status to a failure code.
    return (m_status == Result::Success) ? pGpuEvent->BindGpuMemory(pGpuMem, offset) : m_status;
}

// =====================================================================================================================
// Root level barrier function.  Currently only used for validation of depth / stencil image transitions, and range
// validation.
void CmdBuffer::CmdBarrier(
    const BarrierInfo& barrierInfo)
{
#if PAL_ENABLE_PRINTS_ASSERTS
    AutoBuffer<bool, 32, Platform>  processed(barrierInfo.transitionCount, m_device.GetPlatform());
    if (processed.Capacity() >= barrierInfo.transitionCount)
    {
        memset(&processed[0], 0, sizeof(bool) * barrierInfo.transitionCount);

        for (uint32  idx = 0; idx < barrierInfo.transitionCount; idx++)
        {
            const BarrierTransition&  transition     = barrierInfo.pTransitions[idx];
            const auto&               transitionInfo = transition.imageInfo;
            const auto*               pImage         = static_cast<const Image*>(transitionInfo.pImage);

            if (pImage != nullptr)
            {
                const ImageCreateFlags&  imageCreateFlags = pImage->GetImageCreateInfo().flags;

                // Validate the range.
                pImage->ValidateSubresRange(transitionInfo.subresRange);

                // If we have (deep breath):
                //     A depth image with both Z and stencil planes
                //     That is coming out of uninitialized state
                //     That we haven't seen before
                //     That is valid for sub-resource-init
                //     That must transition both the depth and stencil planes on the same barrier call to be safe
                //
                // then we need to do a little more validation.
                if (pImage->IsDepthStencilTarget()                                             &&
                    (pImage->GetImageInfo().numPlanes == 2)                                    &&
                    TestAnyFlagSet(transitionInfo.oldLayout.usages, LayoutUninitializedTarget) &&
                    (processed[idx] == false)                                                  &&
                    imageCreateFlags.perSubresInit                                             &&
                    (imageCreateFlags.separateDepthPlaneInit == false))
                {
                    const uint32 firstPlane = transitionInfo.subresRange.startSubres.plane;
                    const uint32 otherPlane = (firstPlane == 0) ? 1 : 0;

                    bool  otherPlaneFound = false;
                    for (uint32  innerIdx = idx + 1;
                         ((otherPlaneFound == false) && (innerIdx < barrierInfo.transitionCount));
                         innerIdx++)
                    {
                        const auto&  innerTransitionInfo = barrierInfo.pTransitions[innerIdx].imageInfo;

                        // We found the other plane if this transition is:
                        //   1) Referencing the same image
                        //   2) Also coming out of uninitialized state
                        //   3) Refers to the "other" plane
                        if ((innerTransitionInfo.pImage == pImage)    &&
                            TestAnyFlagSet(innerTransitionInfo.oldLayout.usages, LayoutUninitializedTarget) &&
                            (innerTransitionInfo.subresRange.startSubres.plane == otherPlane))
                        {
                            processed[innerIdx] = true;
                            otherPlaneFound    = true;
                        }
                    }

                    PAL_ALERT(otherPlaneFound == false);

                    processed[idx] = true;
                } // end check for an image that needs more validation
            }
        } // end loop through all the transitions associated with this barrier
    }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 751
    PAL_ASSERT(barrierInfo.flags.splitBarrierEarlyPhase == 0);
    PAL_ASSERT(barrierInfo.flags.splitBarrierLatePhase == 0);
    PAL_ASSERT(barrierInfo.pSplitBarrierGpuEvent == nullptr);
#endif

    PAL_ASSERT((barrierInfo.gpuEventWaitCount == 0) || (barrierInfo.ppGpuEvents != nullptr));

    for (uint32 i = 0; i < barrierInfo.gpuEventWaitCount; i++)
    {
        PAL_ASSERT(barrierInfo.ppGpuEvents[i] != nullptr);
    }
#endif
}

// =====================================================================================================================
uint32 CmdBuffer::CmdRelease(
    const AcquireReleaseInfo& releaseInfo)
{
    // Validate input data.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
    PAL_ASSERT(releaseInfo.dstGlobalStageMask == 0);
#else
    PAL_ASSERT(releaseInfo.dstStageMask == 0);
#endif
    PAL_ASSERT(releaseInfo.dstGlobalAccessMask == 0);
    for (uint32 i = 0; i < releaseInfo.memoryBarrierCount; i++)
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
        PAL_ASSERT(releaseInfo.pMemoryBarriers[i].dstStageMask == 0);
#endif
        PAL_ASSERT(releaseInfo.pMemoryBarriers[i].dstAccessMask == 0);
    }
    for (uint32 i = 0; i < releaseInfo.imageBarrierCount; i++)
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
        PAL_ASSERT(releaseInfo.pImageBarriers[i].dstStageMask == 0);
#endif
        PAL_ASSERT(releaseInfo.pImageBarriers[i].dstAccessMask == 0);
    }

#if PAL_ENABLE_PRINTS_ASSERTS
    VerifyBarrierTransitions(releaseInfo);
#endif

    return 0;
}

// =====================================================================================================================
void CmdBuffer::CmdAcquire(
    const AcquireReleaseInfo& acquireInfo,
    uint32                    syncTokenCount,
    const uint32*             pSyncTokens)
{
    // Validate input data.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
    PAL_ASSERT(acquireInfo.srcGlobalStageMask == 0);
#else
    PAL_ASSERT(acquireInfo.srcStageMask == 0);
#endif
    PAL_ASSERT(acquireInfo.srcGlobalAccessMask == 0);
    for (uint32 i = 0; i < acquireInfo.memoryBarrierCount; i++)
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
        PAL_ASSERT(acquireInfo.pMemoryBarriers[i].srcStageMask == 0);
#endif
        PAL_ASSERT(acquireInfo.pMemoryBarriers[i].srcAccessMask == 0);
    }
    for (uint32 i = 0; i < acquireInfo.imageBarrierCount; i++)
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
        PAL_ASSERT(acquireInfo.pImageBarriers[i].srcStageMask == 0);
#endif
        PAL_ASSERT(acquireInfo.pImageBarriers[i].srcAccessMask == 0);
    }

    PAL_ASSERT((syncTokenCount > 0) && (pSyncTokens != nullptr));

#if PAL_ENABLE_PRINTS_ASSERTS
    VerifyBarrierTransitions(acquireInfo);
#endif
}

// =====================================================================================================================
void CmdBuffer::CmdReleaseEvent(
    const AcquireReleaseInfo& releaseInfo,
    const IGpuEvent*          pGpuEvent)
{
    // Validate input data.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
    PAL_ASSERT(releaseInfo.dstGlobalStageMask == 0);
#else
    PAL_ASSERT(releaseInfo.dstStageMask == 0);
#endif
    PAL_ASSERT(releaseInfo.dstGlobalAccessMask == 0);
    for (uint32 i = 0; i < releaseInfo.memoryBarrierCount; i++)
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
        PAL_ASSERT(releaseInfo.pMemoryBarriers[i].dstStageMask == 0);
#endif
        PAL_ASSERT(releaseInfo.pMemoryBarriers[i].dstAccessMask == 0);
    }
    for (uint32 i = 0; i < releaseInfo.imageBarrierCount; i++)
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
        PAL_ASSERT(releaseInfo.pImageBarriers[i].dstStageMask == 0);
#endif
        PAL_ASSERT(releaseInfo.pImageBarriers[i].dstAccessMask == 0);
    }

    PAL_ASSERT(pGpuEvent != nullptr);

#if PAL_ENABLE_PRINTS_ASSERTS
    VerifyBarrierTransitions(releaseInfo);
#endif
}

// =====================================================================================================================
void CmdBuffer::CmdAcquireEvent(
    const AcquireReleaseInfo& acquireInfo,
    uint32                    gpuEventCount,
    const IGpuEvent*const*    ppGpuEvents)
{
    // Validate input data.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
    PAL_ASSERT(acquireInfo.srcGlobalStageMask == 0);
#else
    PAL_ASSERT(acquireInfo.srcStageMask == 0);
#endif
    PAL_ASSERT(acquireInfo.srcGlobalAccessMask == 0);
    for (uint32 i = 0; i < acquireInfo.memoryBarrierCount; i++)
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
        PAL_ASSERT(acquireInfo.pMemoryBarriers[i].srcStageMask == 0);
#endif
        PAL_ASSERT(acquireInfo.pMemoryBarriers[i].srcAccessMask == 0);
    }
    for (uint32 i = 0; i < acquireInfo.imageBarrierCount; i++)
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
        PAL_ASSERT(acquireInfo.pImageBarriers[i].srcStageMask == 0);
#endif
        PAL_ASSERT(acquireInfo.pImageBarriers[i].srcAccessMask == 0);
    }

    PAL_ASSERT((gpuEventCount > 0) && (ppGpuEvents != nullptr));

    for (uint32 i = 0; i < gpuEventCount; i++)
    {
        PAL_ASSERT(ppGpuEvents[i] != nullptr);
    }

#if PAL_ENABLE_PRINTS_ASSERTS
    VerifyBarrierTransitions(acquireInfo);
#endif
}

// =====================================================================================================================
void CmdBuffer::CmdReleaseThenAcquire(
    const AcquireReleaseInfo& barrierInfo)
{
#if PAL_ENABLE_PRINTS_ASSERTS
    VerifyBarrierTransitions(barrierInfo);
#endif
}

// =====================================================================================================================
void CmdBuffer::CmdCopyDfSpmTraceData(
    const IPerfExperiment& perfExperiment,
    const IGpuMemory&      dstGpuMemory,
    gpusize                dstOffset)
{
    const PerfExperiment& experiment  = static_cast<const PerfExperiment&>(perfExperiment);

    const DfSpmPerfmonInfo* pDfSpmPerfmonInfo = experiment.GetDfSpmPerfmonInfo();

    const gpusize dfSpmTraceBufferSize = pDfSpmPerfmonInfo->pDfSpmTraceBuffer->Desc().size;

    MemoryCopyRegion region = { };
    region.copySize  = sizeof(DfSpmTraceMetadataLayout);
    region.srcOffset = 0;
    region.dstOffset = dstOffset;
    CmdCopyMemory(*pDfSpmPerfmonInfo->pDfSpmMetadataBuffer,
                  dstGpuMemory,
                  1,
                  &region);

    region.copySize  = dfSpmTraceBufferSize;
    region.srcOffset = 0;
    region.dstOffset = dstOffset + sizeof(DfSpmTraceMetadataLayout);
    CmdCopyMemory(*pDfSpmPerfmonInfo->pDfSpmTraceBuffer,
                  dstGpuMemory,
                  1,
                  &region);
}

// =====================================================================================================================
// Writes the commands necessary to write "data" to the specified event.
// Invoked whenever you call ICmdBuffer::CmdSetEvent or ICmdBuffer::CmdResetEvent.
void CmdBuffer::WriteEvent(
    const IGpuEvent& gpuEvent,
    HwPipePoint      pipePoint,
    uint32           data)
{
    // Events can only be set (0xDEADBEEF) or reset (0xCAFEBABE)
    PAL_ASSERT((data == GpuEvent::SetValue) || (data == GpuEvent::ResetValue));

    const GpuEvent&       event       = static_cast<const GpuEvent&>(gpuEvent);
    const BoundGpuMemory& boundMemObj = event.GetBoundGpuMemory();

    if (boundMemObj.IsBound())
    {
        WriteEventCmd(boundMemObj, pipePoint, data);
    }
    else
    {
        // Client never bound any memory to this event object, so there's nothing to do
        PAL_ASSERT_ALWAYS();
    }
}

// =====================================================================================================================
// Returns specified type of data chunks by adding them to the retained chunk list or returning to the parent allocator.
void CmdBuffer::ReturnDataChunks(
    ChunkData*   pData,
    CmdAllocType type,
    bool         returnGpuMemory)
{
    if ((m_status != Result::Success) && (pData->chunkList.IsEmpty() == false))
    {
        // If something went wrong in the previous recording, then our chunk list may have the allocator's dummy chunk
        // at the back of the list.  Since no chunk list truly owns the dummy chunk, we must pop it off the list before
        // proceeding.
        if (pData->chunkList.Back() == m_pCmdAllocator->GetDummyChunk())
        {
            CmdStreamChunk* pChunk = nullptr;
            pData->chunkList.PopBack(&pChunk);
            pChunk->Reset();
        }
    }

    if (returnGpuMemory)
    {
        // The client requested that we return all chunks, add any remaining retained chunks to the chunk list so they
        // can be returned to the allocator with the rest.
        while (pData->retainedChunks.IsEmpty() == false)
        {
            CmdStreamChunk* pChunk = nullptr;
            pData->retainedChunks.PopBack(&pChunk);
            pData->chunkList.PushBack(pChunk);
        }

        // Return all chunks to the command allocator.
        if ((pData->chunkList.IsEmpty() == false) && (m_flags.autoMemoryReuse == true))
        {
            m_pCmdAllocator->ReuseChunks(type, false, pData->chunkList.Begin());
        }
    }
    else
    {
        // Reset the chunks to be retained and add them to the retained list. We can only reset them here because
        // of the interface requirement that the client guarantee that no one is using this command stream anymore.
        for (auto iter = pData->chunkList.Begin(); iter.IsValid(); iter.Next())
        {
            iter.Get()->Reset();
            pData->retainedChunks.PushBack(iter.Get());
        }
    }

    pData->chunkList.Clear();
    pData->chunkDwordsAvailable = 0;
}

// =====================================================================================================================
// Rewinds our linear allocator and releases ownership of it.
void CmdBuffer::ReturnLinearAllocator()
{
    if (m_pMemAllocator != nullptr)
    {
        // We always record the starting position when we get a linear allocator so this should always be valid.
        PAL_ASSERT(m_pMemAllocatorStartPos != nullptr);

        m_pMemAllocator->Rewind(m_pMemAllocatorStartPos, false);
        m_pMemAllocatorStartPos = nullptr;

        // If our linear allocator came from our ICmdAllocator's internal pool we should try to return it.
        if (m_flags.internalMemAllocator)
        {
            m_pCmdAllocator->ReuseLinearAllocator(m_pMemAllocator);
        }

        m_pMemAllocator = nullptr;
    }
}

// =====================================================================================================================
// Called before starting a P2P BLT where the P2P PCI BAR workaround is enabled.  The caller is responsible for ensuring
// the regions are broken up into appropriate small chunks, this function just tracks information that will eventually
// be required by the OS backends for passing info to the KMD.
void CmdBuffer::P2pBltWaCopyBegin(
    const GpuMemory* pDstMemory,
    uint32           regionCount,
    const gpusize*   pChunkAddrs)
{
    // This function should not be called unless the P2P BAR WA is enabled and the destination memory is on a different
    // GPU.
    PAL_ASSERT(m_device.ChipProperties().p2pBltWaInfo.required && pDstMemory->AccessesPeerMemory());

    // Only the universal and SDMA engines support the P2P BLT WA, clients should be honoring the
    // p2pCopyToInvisibleHeapIllegal engine property and we should never hit this function on other engines.
    PAL_ASSERT((GetEngineType() == EngineTypeUniversal) || (GetEngineType() == EngineTypeDma));

    P2pBltWaInfo entry       = { };
    entry.type               = P2pBltWaInfoType::PerCopy;
    entry.perCopy.pDstMemory = pDstMemory;

    // Run through list of chunks.  Mirror the logic in P2pBltWaCopyNextRegion, we will only insert a new chunk VCOP
    // if the chunk address is different than the previous chunk.  This is because the overhead for the PCI BAR
    // updates performed per-chunk are presumed to be expensive.
    m_p2pBltWaLastChunkAddr = ~0u;
    for (uint32 i = 0; i < regionCount; i++)
    {
        if (pChunkAddrs[i] != m_p2pBltWaLastChunkAddr)
        {
            entry.perCopy.numChunks++;
            m_p2pBltWaLastChunkAddr = pChunkAddrs[i];
        }
    }
    m_p2pBltWaInfo.PushBack(entry);

    m_p2pBltWaLastChunkAddr = ~0u;
}

// =====================================================================================================================
// Called before each region of a P2P BLT where the P2P PCI BAR workaround is enabled.  The caller is responsible for
// ensuring the regions are broken up into appropriate small chunks, this function just tracks information that will
// eventually be required by the OS backends for passing info to the KMD.
void CmdBuffer::P2pBltWaCopyNextRegion(
    CmdStream* pCmdStream,
    gpusize    chunkAddr)
{
    // Only add a new chunk entry if the chunk address is different than the last chunk entry.  This logic must be
    // mirrored in P2pBltWaCopyBegin().
    if (chunkAddr != m_p2pBltWaLastChunkAddr)
    {
        P2pBltWaInfo entry = { };
        entry.type               = P2pBltWaInfoType::PerChunk;
        entry.perChunk.startAddr = chunkAddr;

        // Do a dummy reserve and commit here to ensure the GetCurrentGpuVa() call below will be correct.  Otherwise,
        // GetCurrentGpuVa() could return an address pointing to the end of one chunk that would be replace once
        // ReserveCommands() is called.
        uint32* pCmdSpace = pCmdStream->ReserveCommands();
        pCmdStream->CommitCommands(pCmdSpace);

        // Record the VA where KMD should patch the PCI BAR update commands.
        entry.perChunk.cmdBufPatchGpuVa = pCmdStream->GetCurrentGpuVa();

        // KMD patching the command stream is an explicit address dependency.
        pCmdStream->NotifyAddressDependent();

        pCmdSpace = pCmdStream->ReserveCommands();

        // Insert appropriate number of NOPs based on the engine-specific requirements.
        const uint32 nopDwords = (m_engineType == EngineType::EngineTypeDma) ?
                                 m_device.ChipProperties().p2pBltWaInfo.dmaPlaceholderDwords :
                                 m_device.ChipProperties().p2pBltWaInfo.gfxPlaceholderDwords;

        // KMD doesn't always patch over the entire NOPed section.  Make each DWORD of reserved space a valid NOP so
        // that we won't leave garbage in the command buffer to be executed by the GPU if KMD only patches over some.
        for (uint32 i = 0; i < nopDwords; i++)
        {
            pCmdSpace = WriteNops(pCmdSpace, 1);
        }

        pCmdStream->CommitCommands(pCmdSpace);

        m_p2pBltWaInfo.PushBack(entry);

        m_p2pBltWaLastChunkAddr = chunkAddr;
    }
}

// =====================================================================================================================
// A helper function to check if any of this command buffer's command streams are chunk address dependent.
bool CmdBuffer::HasAddressDependentCmdStream() const
{
    bool addressDependent = false;

    for (uint32 idx = 0; idx < NumCmdStreams(); ++idx)
    {
        const CmdStream* pStream = GetCmdStream(idx);
        if (pStream != nullptr)
        {
            addressDependent |=  pStream->IsAddressDependent();
        }
    }

    return addressDependent;
}

// =====================================================================================================================
void CmdBuffer::EndCmdBufferDump(
    const CmdStream** ppCmdStreams,
    uint32            cmdStreamsNum)
{
    if (IsDumpingEnabled() && DumpFile()->IsOpen())
    {
        if (m_device.Settings().cmdBufDumpFormat == CmdBufDumpFormatBinaryHeaders)
        {
            const CmdBufferDumpFileHeader fileHeader =
            {
                static_cast<uint32>(sizeof(CmdBufferDumpFileHeader)), // Structure size
                1,                                                    // Header version
                m_device.ChipProperties().familyId,                   // ASIC family
                m_device.ChipProperties().eRevId,                     // ASIC revision
                0                                                     // Ib2 Start Index
            };
            DumpFile()->Write(&fileHeader, sizeof(fileHeader));

            CmdBufferListHeader listHeader =
            {
                static_cast<uint32>(sizeof(CmdBufferListHeader)),   // Structure size
                0,                                                  // Engine index
                0                                                   // Number of command buffer chunks
            };

            for (uint32 i = 0; i < cmdStreamsNum && ppCmdStreams[i] != nullptr; i++)
            {
                listHeader.count += ppCmdStreams[i]->GetNumChunks();
            }

            DumpFile()->Write(&listHeader, sizeof(listHeader));
        }

        DumpCmdStreamsToFile(DumpFile(), m_device.Settings().cmdBufDumpFormat);
        DumpFile()->Close();
    }
}

// =====================================================================================================================
// Open the dump file, gets the directory from device setting so the file is dumped to the correct folder.
void CmdBuffer::OpenCmdBufDumpFile(
    const char* pFilename)
{
    const auto& settings = m_device.Settings();
    constexpr const char* const pSuffix[] =
    {
        ".txt",     // CmdBufDumpFormat::CmdBufDumpFormatText
        ".bin",     // CmdBufDumpFormat::CmdBufDumpFormatBinary
        ".pm4"      // CmdBufDumpFormat::CmdBufDumpFormatBinaryHeaders
    };

    const char* pLogDir = &settings.cmdBufDumpDirectory[0];

    // Create the directory. We don't care if it fails (existing is fine, failure is caught when opening the file).
    MkDir(pLogDir);

    // Maximum length of a filename allowed for command buffer dumps, seems more reasonable than 32
    constexpr uint32 MaxFilenameLength = 512;

    char fullFilename[MaxFilenameLength] = {};

    // Add log directory to file name to make name unique.
    Snprintf(fullFilename,
             MaxFilenameLength,
             "%s/%s%s",
             pLogDir,
             pFilename,
             pSuffix[settings.cmdBufDumpFormat]);

    if (settings.cmdBufDumpFormat == CmdBufDumpFormat::CmdBufDumpFormatText)
    {
        Result result = m_file.Open(&fullFilename[0], FileAccessMode::FileAccessWrite);
        PAL_ALERT_MSG(result != Result::Success, "Failed to open CmdBuf dump file '%s'", fullFilename);
    }
    else if ((settings.cmdBufDumpFormat == CmdBufDumpFormat::CmdBufDumpFormatBinary) ||
             (settings.cmdBufDumpFormat == CmdBufDumpFormat::CmdBufDumpFormatBinaryHeaders))
    {
        const uint32 fileMode = FileAccessMode::FileAccessWrite | FileAccessMode::FileAccessBinary;
        Result result = m_file.Open(&fullFilename[0], fileMode);
        PAL_ALERT_MSG(result != Result::Success, "Failed to open CmdBuf dump file '%s'", fullFilename);
    }
    else
    {
        // If we get here, dumping is enabled, but it's not one of the modes listed above.
        // Perhaps someone added a new mode?
        PAL_ASSERT_ALWAYS();
    }
}

// =====================================================================================================================
// Dumps the IB2 to a file with headers.
static void DumpIb2ToFile(
    const Ib2DumpInfo& dumpInfo,
    Util::File*        pFile,
    CmdBufDumpFormat   dumpFormat)
{
    const uint32 subEngineId = GetSubEngineId(dumpInfo.subEngineType, dumpInfo.engineType, false);

    if ((dumpFormat == CmdBufDumpFormatBinary) || (dumpFormat == CmdBufDumpFormatBinaryHeaders))
    {
        if (dumpFormat == CmdBufDumpFormatBinaryHeaders)
        {
            const CmdBufferIb2DumpHeader chunkheader =
            {
                static_cast<uint32>(sizeof(CmdBufferIb2DumpHeader)),
                dumpInfo.ib2Size,
                subEngineId,
                dumpInfo.gpuVa,
            };
            pFile->Write(&chunkheader, sizeof(chunkheader));
        }
        pFile->Write(dumpInfo.pCpuAddress, (uint32)(dumpInfo.ib2Size));
    }
    else if (dumpFormat == CmdBufDumpFormatText)
    {
        constexpr uint32 MaxLineSize = 128;
        char line[MaxLineSize];

        // first put some indication that this is an IB2
        Snprintf(line, MaxLineSize, "# IB2 - Command Length: %d - IB2 GPU VA: %016llX\n",
                 dumpInfo.ib2Size / sizeof(uint32), dumpInfo.gpuVa);

        Result result = pFile->Write(line, strlen(line));
        for (uint32 idx = 0; (idx < dumpInfo.ib2Size / sizeof(uint32)) && (result == Result::Success); ++idx)
        {
            Snprintf(line, MaxLineSize, "0x%08x\n", dumpInfo.pCpuAddress[idx]);
            result = pFile->Write(line, strlen(line));
        }

        PAL_ASSERT(result == Result::Success);
    }
    else
    {
        PAL_ALERT_ALWAYS_MSG("Unsupported Dump Format - Pal::Ib2DumpInfo::DumpToFile");
    }
}

// =====================================================================================================================
// Dumps all the IB2s created in this buffer to the file with appropriate Headers.
void CmdBuffer::DumpIb2s(
    Util::File*      pFile,
    CmdBufDumpFormat mode)
{
    const uint32 numIb2 = m_ib2DumpInfos.size();
    if (numIb2 > 0)
    {
        if (mode == CmdBufDumpFormatBinaryHeaders)
        {
            CmdBufferListHeader listHeader =
            {
                static_cast<uint32>(sizeof(CmdBufferListHeader)),   // Structure size
                GetEngineType(),                                    // Engine index
                numIb2,                                             // Number of Ib2 chunks
            };
            pFile->Write(&listHeader, sizeof(listHeader));
        }

        for (auto iter = m_ib2DumpInfos.Begin(); iter.IsValid(); iter.Next())
        {
            const Ib2DumpInfo& dumpInfo = iter.Get();
            DumpIb2ToFile(dumpInfo, pFile, mode);
        }
    }
}

// =====================================================================================================================
// Doesn't insert if an Ib2 with the same gpuVa is already inserted.
void CmdBuffer::InsertIb2DumpInfo(
    const Ib2DumpInfo& dumpInfo)
{
    bool foundMatch = false;
    for (auto iter = m_ib2DumpInfos.Begin(); iter.IsValid(); iter.Next())
    {
        Ib2DumpInfo iterDumpInfo = iter.Get();
        if (dumpInfo.gpuVa == iterDumpInfo.gpuVa)
        {
            foundMatch = true;
            break;
        }
    }
    if (foundMatch == false)
    {
        PAL_ASSERT(m_ib2DumpInfos.PushBack(dumpInfo) == Result::Success);
    }
}

// =====================================================================================================================
// This tracks the IB2 launch addresses if CmdStream::Call(...) will use an IB2
void CmdBuffer::TrackIb2DumpInfoFromExecuteNestedCmds(
    const Pal::CmdStream& targetStream)
{
    for (auto chunkIter = targetStream.GetFwdIterator(); chunkIter.IsValid(); chunkIter.Next())
    {
        // When not chaining, can just push back multiple dumpinfos
        const auto* const pChunk  = chunkIter.Get();

        const Ib2DumpInfo dumpInfo =
        {
            pChunk->CpuAddr(),                             // CPU address of the commands
            pChunk->CmdDwordsToExecute() * sizeof(uint32), // Length of the dump in bytes
            pChunk->GpuVirtAddr(),                         // GPU virtual address of the commands
            targetStream.GetEngineType(),                  // Engine Type
            targetStream.GetSubEngineType(),               // Sub Engine Type
        };

        InsertIb2DumpInfo(dumpInfo);
    }
}

// =====================================================================================================================
void CmdBuffer::GetCmdBufDumpFilename(
    char*  pOutput,
    size_t outputBufSize) const
{
    // filename is:  eeeeeeeeexx_yyyyy, where "eeeeeeeee" is the target engine, "xx" is the number of command
    //               buffers that have been created so far (one based) and "yyyyy" is the number of times this
    //               command buffer has been begun (also one based).
    //
    // All streams associated with this command buffer are included in this one file.
    switch (GetEngineType())
    {
    case EngineTypeUniversal:
        Snprintf(pOutput, outputBufSize, "universal%02d_%05d", UniqueId(), NumBegun());
        break;
    case EngineTypeCompute:
        Snprintf(pOutput, outputBufSize, "compute%02d_%05d", UniqueId(), NumBegun());
        break;
    case EngineTypeDma:
        Snprintf(pOutput, outputBufSize, "dma%02d_%05d", UniqueId(), NumBegun());
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }
}

// =====================================================================================================================
// Gets the subEngineId for dump headers
const uint32 GetSubEngineId(
    const SubEngineType subEngineType,
    const EngineType    engineType,
    const bool          isPreamble)
{
    uint32 subEngineId = 0; // DE subengine ID

    if (subEngineType == SubEngineType::ConstantEngine)
    {
        if (isPreamble)
        {
            subEngineId = 2; // CE preamble subengine ID
        }
        else
        {
            subEngineId = 1; // CE subengine ID
        }
    }
    else if ((engineType == EngineType::EngineTypeCompute) ||
             (subEngineType == SubEngineType::AsyncCompute))
    {
        subEngineId = 3; // Compute subengine ID
    }
    else if (engineType == EngineType::EngineTypeDma)
    {
        subEngineId = 4; // SDMA engine ID
    }

    return subEngineId;
}

// =====================================================================================================================
// Default implementation of CmdDraw is unimplemented, derived CmdBuffer classes should override it if supported.
static void PAL_STDCALL CmdDrawInvalid(
    ICmdBuffer* pCmdBuffer,
    uint32      firstVertex,
    uint32      vertexCount,
    uint32      firstInstance,
    uint32      instanceCount,
    uint32      drawId)
{
    PAL_NEVER_CALLED();
}

// =====================================================================================================================
// Default implementation of CmdDraw is unimplemented, derived CmdBuffer classes should override it if supported.
static void PAL_STDCALL CmdDrawOpaqueInvalid(
    ICmdBuffer* pCmdBuffer,
    gpusize     streamOutFilledSizeVa,
    uint32      streamOutOffset,
    uint32      stride,
    uint32      firstInstance,
    uint32      instanceCount)
{
    PAL_NEVER_CALLED();
}

// =====================================================================================================================
// Default implementation of CmdDrawIndexed is unimplemented, derived CmdBuffer classes should override it if supported.
static void PAL_STDCALL CmdDrawIndexedInvalid(
    ICmdBuffer* pCmdBuffer,
    uint32      firstIndex,
    uint32      indexCount,
    int32       vertexOffset,
    uint32      firstInstance,
    uint32      instanceCount,
    uint32      drawId)
{
    PAL_NEVER_CALLED();
}

// =====================================================================================================================
// Default implementation of CmdDrawIndirectMulti is unimplemented, derived CmdBuffer classes should override it if
// supported.
static void PAL_STDCALL CmdDrawIndirectMultiInvalid(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            stride,
    uint32            maximumCount,
    gpusize           countGpuAddr)
{
    PAL_NEVER_CALLED();
}

// =====================================================================================================================
// Default implementation of CmdDrawIndexedIndirectMulti is unimplemented, derived CmdBuffer classes should override it
// if supported.
static void PAL_STDCALL CmdDrawIndexedIndirectMultiInvalid(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            stride,
    uint32            maximumCount,
    gpusize           countGpuAddr)
{
    PAL_NEVER_CALLED();
}

// =====================================================================================================================
// Default implementation of CmdDispatch is unimplemented, derived CmdBuffer classes should override it if supported.
void PAL_STDCALL CmdBuffer::CmdDispatchInvalid(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims size)
{
    PAL_NEVER_CALLED();
}

// =====================================================================================================================
// Default implementation of CmdDispatchIndirect is unimplemented, derived CmdBuffer classes should override it if
// supported.
void PAL_STDCALL CmdBuffer::CmdDispatchIndirectInvalid(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset)
{
    PAL_NEVER_CALLED();
}

// =====================================================================================================================
// Default implementation of CmdDispatchOffset is unimplemented, derived CmdBuffer classes should override it if
// supported.
void PAL_STDCALL CmdBuffer::CmdDispatchOffsetInvalid(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims offset,
    DispatchDims launchSize,
    DispatchDims logicalSize)
{
    PAL_NEVER_CALLED();
}

// =====================================================================================================================
// Default implementation of CmdDispatchDynamic is unimplemented, derived CmdBuffer classes should override it if
// supported.
void PAL_STDCALL CmdBuffer::CmdDispatchDynamicInvalid(
    ICmdBuffer*  pCmdBuffer,
    gpusize      gpuVa,
    DispatchDims size)
{
    PAL_NEVER_CALLED();
}

// =====================================================================================================================
// Default implementation of CmdDispatchMesh is unimplemented, derived CmdBuffer classes should override it if
// supported.
void PAL_STDCALL CmdBuffer::CmdDispatchMeshInvalid(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims size)
{
    PAL_NEVER_CALLED();
}

// =====================================================================================================================
// Default implementation of CmdDispatchMeshIndirect is unimplemented, derived CmdBuffer classes should override it if
// supported.
void PAL_STDCALL CmdBuffer::CmdDispatchMeshIndirectMultiInvalid(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            stride,
    uint32            maximumCount,
    gpusize           countGpuAddr)
{
    PAL_NEVER_CALLED();
}

// =====================================================================================================================
// Helper function used for validation of depth / stencil image transitions, and range validation. For
// Release/acquire-based barrier only.
void CmdBuffer::VerifyBarrierTransitions(
    const AcquireReleaseInfo& barrierInfo
    ) const
{
    AutoBuffer<bool, 16, Platform>  processed(barrierInfo.imageBarrierCount, m_device.GetPlatform());
    if (processed.Capacity() >= barrierInfo.imageBarrierCount)
    {
        memset(&processed[0], 0, sizeof(bool) * barrierInfo.imageBarrierCount);

        for (uint32  idx = 0; idx < barrierInfo.imageBarrierCount; idx++)
        {
            const ImgBarrier& transition = barrierInfo.pImageBarriers[idx];
            const auto*       pImage     = static_cast<const Image*>(transition.pImage);

            PAL_ASSERT(pImage != nullptr);

            const ImageCreateFlags&  imageCreateFlags = pImage->GetImageCreateInfo().flags;

            // Validate the range.
            pImage->ValidateSubresRange(transition.subresRange);

            // If we have (deep breath):
            //     A depth image with both Z and stencil planes
            //     That is coming out of uninitialized state
            //     That we haven't seen before
            //     That is valid for sub-resource-init
            //     That must transition both the depth and stencil planes on the same barrier call to be safe
            //
            // then we need to do a little more validation.
            if (pImage->IsDepthStencilTarget()                                             &&
                (pImage->GetImageInfo().numPlanes == 2)                                    &&
                TestAnyFlagSet(transition.oldLayout.usages, LayoutUninitializedTarget)     &&
                (processed[idx] == false)                                                  &&
                imageCreateFlags.perSubresInit                                             &&
                (imageCreateFlags.separateDepthPlaneInit == false))
            {
                const uint32 firstPlane = transition.subresRange.startSubres.plane;
                const uint32 otherPlane = (firstPlane == 0) ? 1 : 0;

                bool  otherPlaneFound = false;
                for (uint32  innerIdx = idx + 1;
                     ((otherPlaneFound == false) && (innerIdx < barrierInfo.imageBarrierCount));
                     innerIdx++)
                {
                    const ImgBarrier& innerTransition = barrierInfo.pImageBarriers[innerIdx];

                    // We found the other plane if this transition is:
                    //   1) Referencing the same image
                    //   2) Also coming out of uninitialized state
                    //   3) Refers to the "other" plane
                    if ((innerTransition.pImage == pImage)                                          &&
                        TestAnyFlagSet(innerTransition.oldLayout.usages, LayoutUninitializedTarget) &&
                        (innerTransition.subresRange.startSubres.plane == otherPlane))
                    {
                        processed[innerIdx] = true;
                        otherPlaneFound    = true;
                    }
                }

                PAL_ALERT(otherPlaneFound == false);

                processed[idx] = true;
            } // end check for an image that needs more validation
        } // end loop through all the transitions associated with this barrier
    }
}

// =====================================================================================================================
uint32 CmdBuffer::GetUsedSize(
    CmdAllocType type
    ) const
{
    uint32 sizeInDwords = 0;

    switch (type)
    {
    case EmbeddedDataAlloc:
        for (auto iter = m_embeddedData.chunkList.Begin(); iter.IsValid(); iter.Next())
        {
            sizeInDwords += iter.Get()->DwordsAllocated();
        }
        break;
    case GpuScratchMemAlloc:
        for (auto iter = m_gpuScratchMem.chunkList.Begin(); iter.IsValid(); iter.Next())
        {
            sizeInDwords += iter.Get()->DwordsAllocated();
        }
        break;
    case CommandDataAlloc:
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return (sizeInDwords * sizeof(uint32));
}

// =====================================================================================================================
void CmdBuffer::NotifyAllocFailure()
{
    PAL_ALERT_ALWAYS();
    SetCmdRecordingError(Result::ErrorOutOfMemory);
}

// =====================================================================================================================
// The command recording status is sticky, remembering the first error seen during command recording.
void CmdBuffer::SetCmdRecordingError(
    Result error)
{
    // By definition this has to be an error.
    PAL_ASSERT(Util::IsErrorResult(error));

    if (Util::IsErrorResult(m_status) == false)
    {
        m_status = error;
    }
}

// =====================================================================================================================
// Generic implementation for products or command buffers that don't support VRS.
void CmdBuffer::CmdSetPerDrawVrsRate(
    const VrsRateParams&  rateParams)
{
    PAL_NOT_IMPLEMENTED();
}

// =====================================================================================================================
// Generic implementation for products or command buffers that don't support VRS.
void CmdBuffer::CmdSetVrsCenterState(
    const VrsCenterState&  centerState)
{
    PAL_NOT_IMPLEMENTED();
}

// =====================================================================================================================
// Generic implementation for products or command buffers that don't support VRS.
void CmdBuffer::CmdBindSampleRateImage(
    const IImage*  pImage)
{
    PAL_NOT_IMPLEMENTED();
}

} // Pal
