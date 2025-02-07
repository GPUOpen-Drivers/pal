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
#include "core/device.h"
#include "core/platform.h"
#include "palStringTableTraceSource.h"
#include "palVectorImpl.h"

using namespace Pal;

namespace GpuUtil
{
std::atomic<Pal::uint32> StringTableTraceSource::s_nextTableId = 1;

// =====================================================================================================================
StringTableTraceSource::StringTableTraceSource(
    IPlatform* pPlatform)
    :
    m_pPlatform(pPlatform),
    m_stringTables(pPlatform)
{
}

// =====================================================================================================================
StringTableTraceSource::~StringTableTraceSource()
{
    ClearStringTables();
}

// =====================================================================================================================
void StringTableTraceSource::AddStringTable(
    uint32        tableId,
    uint32        numStrings,
    const uint32* pStringOffsets,
    const char*   pStringData,
    uint32        stringDataSize)
{
    if (numStrings > 0)
    {
        const uint32 offsetSize = numStrings * sizeof(uint32);
        const uint32 chunkSize  = offsetSize + stringDataSize;

        void* pChunkData = PAL_MALLOC(chunkSize, m_pPlatform, Util::AllocInternalTemp);
        memcpy(pChunkData, pStringOffsets, offsetSize);
        memcpy(Util::VoidPtrInc(pChunkData, offsetSize), pStringData, stringDataSize);

        StringTableEntry entry =
        {
            .tableId    = tableId,
            .numStrings = numStrings,
            .chunkSize  = chunkSize,
            .pChunkData = pChunkData
        };

        Util::Result result = m_stringTables.PushBack(entry);

        if (result != Util::Result::Success)
        {
            PAL_SAFE_FREE(pChunkData, m_pPlatform);
        }
    }
}

// =====================================================================================================================
Result StringTableTraceSource::WriteStringTableChunks()
{
    Result result = Result::Success;

    for (uint32 i = 0; i < m_stringTables.NumElements(); ++i)
    {
        const StringTableEntry& entry = m_stringTables.At(i);

        TraceChunk::StringTableHeader header = {
            .tableId    = entry.tableId,
            .numStrings = entry.numStrings
        };

        TraceChunkInfo info = {
            .version            = TraceChunk::StringTableChunkVersion,
            .pHeader            = &header,
            .headerSize         = sizeof(TraceChunk::StringTableHeader),
            .pData              = entry.pChunkData,
            .dataSize           = entry.chunkSize,
            .enableCompression  = false
        };
        memcpy(info.id, TraceChunk::StringTableChunkId, TextIdentifierSize);

        result = m_pPlatform->GetTraceSession()->WriteDataChunk(this, info);
    }

    return Result::Success;
}

// =====================================================================================================================
void StringTableTraceSource::OnTraceFinished()
{
    // If WriteStringTableChunks failed, we get an incomplete trace but no worse than that.
    WriteStringTableChunks();

    // Clear the old data so we can start fresh next capture.
    ClearStringTables();
}

// =====================================================================================================================
void StringTableTraceSource::ClearStringTables()
{
    for (uint32 i = 0; i < m_stringTables.NumElements(); ++i)
    {
        PAL_SAFE_FREE(m_stringTables.At(i).pChunkData, m_pPlatform);
    }

    m_stringTables.Clear();
}

} // namespace GpuUtil
#endif
