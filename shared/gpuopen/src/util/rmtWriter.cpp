/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <util/rmtWriter.h>

#include <ctime>

namespace DevDriver
{

//=====================================================================================================================
RmtWriter::RmtWriter(
    const AllocCb& allocCb)
    : m_allocCb(allocCb)
    , m_state(RmtWriterState::Uninitialized)
    , m_dataChunkHeaderOffset(0)
    , m_rmtFileData(m_allocCb)
{
}

//=====================================================================================================================
RmtWriter::~RmtWriter()
{
}

//=====================================================================================================================
void RmtWriter::Init()
{
    DD_ASSERT((m_state == RmtWriterState::Uninitialized) || (m_state == RmtWriterState::Finalized));

    // Make sure we start with an empty file buffer
    m_rmtFileData.Resize(0);
    m_eventTimer.Reset();

    m_state = RmtWriterState::Initialized;
}

//=====================================================================================================================
// Writes a file header chunk to the Rmt file. This is only necessary if the caller is writing an entire file with
// this writer instance.
// pFileCreateTime can be provided to set the create time in the RMT file header, if it is null then the current
//    time will be used.
void RmtWriter::WriteFileHeader(
    const time_t* pFileCreateTime)
{
    DD_ASSERT(m_state == RmtWriterState::Initialized);

    DD_UNUSED(pFileCreateTime);

    // Setup and write the file header
    struct tm* pRmtFileTime = nullptr;
    if (pFileCreateTime == nullptr)
    {
        time_t currTime = time(NULL);
        pRmtFileTime = gmtime(&currTime);
    }
    else
    {
        pRmtFileTime = gmtime(pFileCreateTime);
    }

    // The gmtime function should never return a null pointer.
    DD_ASSERT(pRmtFileTime != nullptr);

    RmtFileHeader fileHeader = {};
    fileHeader.magicNumber = RMT_FILE_MAGIC_NUMBER;
    fileHeader.versionMajor = RMT_FILE_MAJOR_VERSION;
    fileHeader.versionMinor = RMT_FILE_MINOR_VERSION;
    fileHeader.flags = 0;
    fileHeader.chunkOffset = sizeof(RmtFileHeader);
    fileHeader.second = pRmtFileTime->tm_sec;
    fileHeader.minute = pRmtFileTime->tm_min;
    fileHeader.hour = pRmtFileTime->tm_hour;
    fileHeader.dayInMonth = pRmtFileTime->tm_mday;
    fileHeader.month = pRmtFileTime->tm_mon;
    fileHeader.year = pRmtFileTime->tm_year;
    fileHeader.dayInWeek = pRmtFileTime->tm_wday;
    fileHeader.dayInYear = pRmtFileTime->tm_yday;
    fileHeader.isDaylightSavings = pRmtFileTime->tm_isdst;

    WriteBytes(&fileHeader, sizeof(fileHeader));
}

//=====================================================================================================================
// Writes a SystemInfo chunk to the Rmt file.  Callers may zero initialize the header field, as it will be
// filled out this function before writing.
void RmtWriter::WriteSystemInfo(
    RmtFileChunkSystemInfo systemInfo)
{
    DD_ASSERT(m_state == RmtWriterState::Initialized);

    // Fill out the chunk header
    systemInfo.header.chunkIdentifier.chunkType  = RMT_FILE_CHUNK_TYPE_SYSTEM_INFO;
    systemInfo.header.chunkIdentifier.chunkIndex = 0;
    systemInfo.header.versionMinor               = 1;
    systemInfo.header.versionMajor               = 0;
    systemInfo.header.sizeInBytes                = sizeof(RmtFileChunkSystemInfo);
    systemInfo.header.padding                    = 0;

    // Then write the data
    WriteBytes(&systemInfo, sizeof(RmtFileChunkSystemInfo));
}

//=====================================================================================================================
// Starts a new RMT data chunk.
void RmtWriter::BeginDataChunk(
    uint64 processId,
    uint64 threadId)
{
    DD_ASSERT(m_state == RmtWriterState::Initialized);

    // First create the chunk header and add it to the stream
    RmtFileChunkRmtData chunkHeader = {};
    chunkHeader.header.chunkIdentifier.chunkType  = RMT_FILE_CHUNK_TYPE_RMT_DATA;
    chunkHeader.header.chunkIdentifier.chunkIndex = 1;
    chunkHeader.header.versionMinor               = 1;
    chunkHeader.header.versionMajor               = 0;
    chunkHeader.header.sizeInBytes                = 0;
    chunkHeader.header.padding                    = 0;
    chunkHeader.processId                         = processId;
    chunkHeader.threadId                          = threadId;

    // Save the current data offset, so we can revisit the data chunk header to update the size once we know how many
    // bytes of token data has been written.
    m_dataChunkHeaderOffset = m_rmtFileData.Size();

    WriteBytes(&chunkHeader, sizeof(RmtFileChunkRmtData));

    m_state = RmtWriterState::WritingDataChunk;
}

//=====================================================================================================================
void RmtWriter::WriteTokenData(
    const RMT_TOKEN_DATA& tokenData)
{
    DD_ASSERT(m_state == RmtWriterState::WritingDataChunk);

    WriteBytes(tokenData.Data(), tokenData.Size());
}

//=====================================================================================================================
// Calculates the 4-bit delta for an RMT token, adding TIMESTAMP or TIME_DELTA tokens to the active data
// chunk as required
uint8 RmtWriter::CalculateDelta()
{
    DD_ASSERT(m_state == RmtWriterState::WritingDataChunk);

    const EventTimestamp eventTimestamp = m_eventTimer.CreateTimestamp();

    uint8 delta = 0;

    switch (eventTimestamp.type)
    {
        case EventTimestampType::Full:
        {
            // In this case we need to write a TIMESTAMP token and the delta returned will be zero
            RMT_MSG_TIMESTAMP tsToken(eventTimestamp.full.timestamp, eventTimestamp.full.frequency);
            WriteBytes(tsToken.Data(), tsToken.Size());

            break;
        }
        case EventTimestampType::LargeDelta:
        {
            // In this case, the time elapsed is short enough that we can get away with delta tokens
            // instead of a full timestamp.

            // Write out the final delta token
            RMT_MSG_TIME_DELTA tdToken(eventTimestamp.largeDelta.delta, eventTimestamp.largeDelta.numBytes);
            WriteBytes(tdToken.Data(), tdToken.Size());

            break;
        }
        case EventTimestampType::SmallDelta:
        {
            // In this case, the time elapsed since the last full timestamp packet is small enough that we can just
            // calculate and return the delta

            delta = eventTimestamp.smallDelta.delta;

            break;
        }
        default:
        {
            DD_ASSERT_REASON("Invalid event timestamp type!");
            break;
        }
    }

    return delta;
}

//=====================================================================================================================
void RmtWriter::EndDataChunk()
{
    DD_ASSERT(m_state == RmtWriterState::WritingDataChunk);

    const int32 rmtDataChunkSize = static_cast<int32>(m_rmtFileData.Size() - m_dataChunkHeaderOffset);

    RmtFileChunkRmtData* pHeader =
        static_cast<RmtFileChunkRmtData*>(VoidPtrInc(m_rmtFileData.Data(), m_dataChunkHeaderOffset));
    pHeader->header.sizeInBytes = rmtDataChunkSize;

    // Update our state
    m_state = RmtWriterState::Initialized;
    m_dataChunkHeaderOffset = 0;
}

//=====================================================================================================================
void RmtWriter::WriteData(const void* pData, size_t dataSize)
{
    DD_ASSERT((m_state == RmtWriterState::Initialized) || (m_state == RmtWriterState::WritingDataChunk));

    WriteBytes(pData, dataSize);
}

//=====================================================================================================================
void RmtWriter::Finalize()
{
    DD_ASSERT(m_state == RmtWriterState::Initialized);

    m_state = RmtWriterState::Finalized;
}

//=====================================================================================================================
// Writes to the RMT file stream
void RmtWriter::WriteBytes(
    const void* pData,
    size_t      dataSize)
{
    DD_ASSERT((m_state == RmtWriterState::Initialized) || (m_state == RmtWriterState::WritingDataChunk));

    // Add the bytes to our in-memory stream
    const size_t byteOffset = m_rmtFileData.Grow(dataSize);
    void* pDst = VoidPtrInc(m_rmtFileData.Data(), byteOffset);
    memcpy(pDst, pData, dataSize);
}

} // namespace DevDriver
