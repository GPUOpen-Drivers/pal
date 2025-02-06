/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_BUILD_RDF
#include "palHashMapImpl.h"
#include "palUserMarkerHistoryTraceSource.h"

using namespace Pal;

namespace GpuUtil
{

// =====================================================================================================================
UserMarkerHistoryTraceSource::UserMarkerHistoryTraceSource(IPlatform* pPlatform)
    :
    m_pPlatform(pPlatform),
    m_userMarkerHistoryMap(128, pPlatform)
{
    Util::Result result = m_userMarkerHistoryMap.Init();
    PAL_ASSERT(result == Util::Result::Success);
}

// =====================================================================================================================
UserMarkerHistoryTraceSource::~UserMarkerHistoryTraceSource()
{
    ClearUserMarkerHistoryMap();
}

// =====================================================================================================================
void UserMarkerHistoryTraceSource::AddUserMarkerHistory(
    uint32        sqttCbId,
    uint32        tableId,
    uint32        numOps,
    const uint32* pUserMakerHistory)
{
    const uint32 chunkSize = numOps * sizeof(uint32);
    void* pHistory = nullptr;

    if (numOps > 0)
    {
        pHistory = PAL_MALLOC(chunkSize, m_pPlatform, Util::AllocInternalTemp);
        memcpy(pHistory, pUserMakerHistory, chunkSize);
    }

    UserMarkerHistoryEntry entry =
    {
        .tableId            = tableId,
        .numOps             = numOps,
        .pUserMarkerHistory = static_cast<uint32*>(pHistory)
    };

    Util::Result result = m_userMarkerHistoryMap.Insert(sqttCbId, entry);

    if (result != Util::Result::Success)
    {
        PAL_SAFE_FREE(pHistory, m_pPlatform);
    }
}

// =====================================================================================================================
Result UserMarkerHistoryTraceSource::WriteUserMarkerHistoryChunks()
{
    Result result = Result::Success;

    for (auto it = m_userMarkerHistoryMap.Begin(); it.Get() != nullptr; it.Next())
    {
        const uint32 sqttCbId = it.Get()->key;
        const UserMarkerHistoryEntry& entry = it.Get()->value;

        TraceChunk::UserMarkerHistoryHeader header = {
            .sqttCbId   = sqttCbId,
            .tableId    = entry.tableId,
            .numOps     = entry.numOps
        };

        TraceChunkInfo info = {
            .version            = TraceChunk::UserMarkerHistoryChunkVersion,
            .pHeader            = &header,
            .headerSize         = sizeof(TraceChunk::UserMarkerHistoryHeader),
            .pData              = entry.pUserMarkerHistory,
            .dataSize           = header.numOps * sizeof(uint32),
            .enableCompression  = false
        };
        memcpy(info.id, TraceChunk::UserMarkerHistoryChunkId, TextIdentifierSize);

        result = m_pPlatform->GetTraceSession()->WriteDataChunk(this, info);

        if (result != Result::Success)
        {
            break;
        }
    }

    return result;
}

// =====================================================================================================================
void UserMarkerHistoryTraceSource::OnTraceFinished()
{
    // If WriteUserMarkerHistory failed, we get an incomplete trace but no worse than that.
    WriteUserMarkerHistoryChunks();

    // Clear the old history so we can start fresh next capture.
    ClearUserMarkerHistoryMap();
}

// =====================================================================================================================
void UserMarkerHistoryTraceSource::ClearUserMarkerHistoryMap()
{
    for (auto it = m_userMarkerHistoryMap.Begin(); it.Get() != nullptr; it.Next())
    {
        PAL_SAFE_FREE(it.Get()->value.pUserMarkerHistory, m_pPlatform);
    }
    m_userMarkerHistoryMap.Reset();
}

} // namespace GpuUtil
#endif
