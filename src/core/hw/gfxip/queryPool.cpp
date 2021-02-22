/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/eventDefs.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/queryPool.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
QueryPool::QueryPool(
    const Device&              device,
    const QueryPoolCreateInfo& createInfo,
    gpusize                    alignment,
    gpusize                    querySizeInBytes,
    gpusize                    tsSizeInBytes)
    :
    m_createInfo(createInfo),
    m_alignmentInBytes(alignment),
    m_gpuResultSizePerSlotInBytes(querySizeInBytes),
    m_timestampSizePerSlotInBytes(tsSizeInBytes),
    m_boundSizeInBytes((querySizeInBytes + tsSizeInBytes) * createInfo.numSlots),
    m_device(device),
    m_timestampStartOffset(m_createInfo.numSlots * m_gpuResultSizePerSlotInBytes)
{
    ResourceDescriptionQueryPool desc = {};
    desc.pCreateInfo = &m_createInfo;
    ResourceCreateEventData data = {};
    data.type = ResourceType::QueryPool;
    data.pResourceDescData = static_cast<void*>(&desc);
    data.resourceDescSize = sizeof(ResourceDescriptionQueryPool);
    data.pObj = this;
    m_device.GetPlatform()->GetEventProvider()->LogGpuMemoryResourceCreateEvent(data);
}

// =====================================================================================================================
QueryPool::~QueryPool()
{
    ResourceDestroyEventData data = {};
    data.pObj = this;
    m_device.GetPlatform()->GetEventProvider()->LogGpuMemoryResourceDestroyEvent(data);
}

// =====================================================================================================================
// Specifies requirements for GPU memory a client must bind to this object before using it: size, alignment, and heaps.
// NOTE: Part of the public IGpuMemoryBindable interface.
// Note that DX12 and Mantle/Vulkan have different pool/heap memory heap preference, PAL provides all supported heaps
// in default order, client drivers need to re-qualify by adjusting order or removing heap they don't like
void QueryPool::GetGpuMemoryRequirements(
    GpuMemoryRequirements* pGpuMemReqs
    ) const
{
    pGpuMemReqs->size            = m_boundSizeInBytes;
    pGpuMemReqs->alignment       = m_alignmentInBytes;
    pGpuMemReqs->flags.u32All    = 0;

    if (m_createInfo.flags.enableCpuAccess)
    {
        // If a query pool will have its results read back using the CPU, then GartCacheable is the only preferable
        // heap for efficiency.
        pGpuMemReqs->flags.cpuAccess = 1;
        pGpuMemReqs->heapCount = 1;
        pGpuMemReqs->heaps[0]  = GpuHeapGartCacheable;
    }
    else
    {
        const bool noInvisibleMem = (m_device.MemoryProperties().invisibleHeapSize == 0);

        // Otherwise, the other heaps prefer query pools to reside in GPU memory, but safely get evicted back to
        // nonlocal memory in high memory-pressure situations.
        if (noInvisibleMem)
        {
            pGpuMemReqs->heapCount = 2;
            pGpuMemReqs->heaps[0] = GpuHeapLocal;
            pGpuMemReqs->heaps[1] = GpuHeapGartUswc;
        }
        else
        {
            pGpuMemReqs->heapCount = 3;
            pGpuMemReqs->heaps[0] = GpuHeapInvisible;
            pGpuMemReqs->heaps[1] = GpuHeapLocal;
            pGpuMemReqs->heaps[2] = GpuHeapGartUswc;
        }
    }
}

// =====================================================================================================================
Result QueryPool::GetResults(
    QueryResultFlags flags,
    QueryType        queryType,
    uint32           startQuery,
    uint32           queryCount,
    const void*      pMappedGpuAddr,
    size_t*          pDataSize,
    void*            pData,
    size_t           stride)
{
    const size_t oneSlotResultSize  = GetResultSizeForOneSlot(flags);
    const size_t resultStride       = (stride == 0) ? oneSlotResultSize : stride;
    const size_t allSlotsResultSize = (queryCount - 1) * resultStride + oneSlotResultSize;

    Result result = Result::Success;

    PAL_ASSERT(pDataSize != nullptr);

    if (pData != nullptr)
    {
        if (result == Result::Success)
        {
            result = ValidateSlot(startQuery + queryCount - 1);
        }

        if ((result == Result::Success) && (*pDataSize < allSlotsResultSize))
        {
            result = Result::ErrorInvalidMemorySize;
        }

        if (m_device.GetIfhMode() == IfhModeDisabled)
        {
            void* pGpuData = nullptr;
            if (result == Result::Success)
            {
                if (pMappedGpuAddr == nullptr)
                {
                    result = m_gpuMemory.Map(&pGpuData);
                }
                else
                {
                    // Use the mapped GPU memory that was supplied.
                    pGpuData = const_cast<void*>(pMappedGpuAddr);
                }
            }

            if (result == Result::Success)
            {
                pGpuData = VoidPtrInc(pGpuData, GetGpuResultSizeInBytes(startQuery));

                // Call into the hardware layer to compute the results for the given query range.
                if (ComputeResults(flags, queryType, queryCount, resultStride, pGpuData, pData) == false)
                {
                    // Report that at least one of the queries was not ready. We still do this if QueryResultPartial is set.
                    result = Result::NotReady;
                }

                if (pMappedGpuAddr == nullptr)
                {
                    // Don't store the result from this as it will overwrite the result from retrieving the data.
                    const Result unmapResult = m_gpuMemory.Unmap();
                    PAL_ASSERT(unmapResult == Result::Success);
                }
            }
        }
        else
        {
            memset(pData, 0, *pDataSize);
        }
    }

    // Report the size needed to store all of the results.
    *pDataSize = allSlotsResultSize;

    return result;
}

// =====================================================================================================================
// Verifies that the specified slot is supported by this query pool.
Result QueryPool::ValidateSlot(
    uint32 slot
    ) const
{
    Result result = Result::Success;

    if (slot >= m_createInfo.numSlots)
    {
        result = Result::ErrorInvalidValue;
    }
    else if (m_gpuMemory.IsBound() == false)
    {
        result = Result::ErrorGpuMemoryNotBound;
    }

    return result;
}

// =====================================================================================================================
// Binds a block of GPU memory to this object.
// NOTE: Part of the public IGpuMemoryBindable interface.
Result QueryPool::BindGpuMemory(
    IGpuMemory* pGpuMemory,
    gpusize     offset)
{
    Result result = Device::ValidateBindObjectMemoryInput(pGpuMemory,
                                                          offset,
                                                          m_boundSizeInBytes,
                                                          m_alignmentInBytes,
                                                          false);

    if (result == Result::Success)
    {
        m_gpuMemory.Update(pGpuMemory, offset);

        GpuMemoryResourceBindEventData data = {};
        data.pObj = this;
        data.pGpuMemory = pGpuMemory;
        data.requiredGpuMemSize = m_boundSizeInBytes;
        data.offset = offset;
        m_device.GetPlatform()->GetEventProvider()->LogGpuMemoryResourceBindEvent(data);
    }

    return result;
}

// =====================================================================================================================
// Resets the query pool, performing either an optimized or normal reset depending on the command buffer type.
void QueryPool::Reset(
    GfxCmdBuffer* pCmdBuffer,
    CmdStream*    pCmdStream,
    uint32        startQuery,
    uint32        queryCount
    ) const
{
    if (ValidateSlot(startQuery + queryCount - 1) == Result::Success)
    {
        if (pCmdBuffer->GetEngineType() != EngineTypeDma)
        {
            OptimizedReset(pCmdBuffer, pCmdStream, startQuery, queryCount);
        }
        else
        {
            NormalReset(pCmdBuffer, pCmdStream, startQuery, queryCount);
        }
    }
}

// =====================================================================================================================
// Reset this query pool with CPU.
Result QueryPool::DoReset(
    uint32      startQuery,
    uint32      queryCount,
    void*       pMappedCpuAddr,
    gpusize     resetDataSizeInBytes,
    const void* pResetData)
{
    Result result = ValidateSlot(startQuery + queryCount - 1);

    if (result == Result::Success)
    {
        void* pGpuData = pMappedCpuAddr;

        if (pGpuData == nullptr)
        {
            result = m_gpuMemory.Map(&pGpuData);
        }

        if (result == Result::Success)
        {
            // Reset the query pool
            uint8* pQueryData = static_cast<uint8*>(VoidPtrInc(pGpuData, GetGpuResultSizeInBytes(startQuery)));
            const size_t itemCount = GetGpuResultSizeInBytes(queryCount) / static_cast<size_t>(resetDataSizeInBytes);

            for (size_t i = 0; i < itemCount; i++)
            {
                memcpy(pQueryData + i * resetDataSizeInBytes, pResetData, static_cast<size_t>(resetDataSizeInBytes));
            }

            if (HasTimestamps())
            {
                // Reset timestamps
                const size_t timestampSize   = static_cast<size_t>(m_timestampSizePerSlotInBytes);
                const size_t timestampOffset = static_cast<size_t>(m_timestampStartOffset);
                void* const  pTimestampData  = VoidPtrInc(pGpuData, timestampOffset + timestampSize * startQuery);

                memset(pTimestampData, 0, timestampSize * queryCount);
            }

            if (pMappedCpuAddr == nullptr)
            {
                result = m_gpuMemory.Unmap();
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Returns the GPU address for the given slot in the query pool.
Result QueryPool::GetQueryGpuAddress(
    uint32   slot,
    gpusize* pGpuAddr
    ) const
{
    Result result = ValidateSlot(slot);

    if (result == Result::Success)
    {
        (*pGpuAddr) = m_gpuMemory.GpuVirtAddr() + GetGpuResultSizeInBytes(slot);
    }

    return result;
}

// =====================================================================================================================
// Returns the GPU address for the given slot's timestamp in the query pool.
Result QueryPool::GetTimestampGpuAddress(
    uint32   slot,
    gpusize* pGpuAddr
    ) const
{
    // A size of zero indicates that this query pool didn't allocate timestamps and this should never be called.
    PAL_ASSERT(m_timestampSizePerSlotInBytes != 0);

    Result result = ValidateSlot(slot);

    if (result == Result::Success)
    {
        (*pGpuAddr) = m_gpuMemory.Memory()->Desc().gpuVirtAddr + GetTimestampOffset(slot);
    }

    return result;
}

// =====================================================================================================================
// Returns the GPU memory offset for the given slot's timestamp in the query pool.
gpusize QueryPool::GetTimestampOffset(
    uint32 slot
    ) const
{
    // A size of zero indicates that this query pool didn't allocate timestamps and this should never be called.
    PAL_ASSERT(m_timestampSizePerSlotInBytes != 0);
    return m_gpuMemory.Offset() + m_timestampStartOffset + slot * m_timestampSizePerSlotInBytes;
}

} // Pal
