/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

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
    m_dataChunkHeaderOffset = 0;
    m_rmtFileData.Resize(0);
    m_eventTimer.Reset();

    m_state = RmtWriterState::Initialized;
}

//=====================================================================================================================
void RmtWriter::Reset()
{
    m_state = RmtWriterState::Uninitialized;
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
    systemInfo.header.sizeInBytes                = sizeof(systemInfo);
    systemInfo.header.padding                    = 0;

    // Then write the data
    WriteBytes(&systemInfo, sizeof(systemInfo));
}

//=====================================================================================================================
void RmtWriter::WriteSegmentInfo(RmtFileChunkSegmentInfo segmentInfo)
{
    DD_ASSERT(m_state == RmtWriterState::Initialized);

    // Fill out the chunk header
    segmentInfo.header.chunkIdentifier.chunkType  = RMT_FILE_CHUNK_TYPE_SEGMENT_INFO;
    segmentInfo.header.chunkIdentifier.chunkIndex = 0;
    segmentInfo.header.versionMinor               = 1;
    segmentInfo.header.versionMajor               = 0;
    segmentInfo.header.sizeInBytes                = sizeof(segmentInfo);
    segmentInfo.header.padding                    = 0;

    // Then write the data
    WriteBytes(&segmentInfo, sizeof(segmentInfo));
}

//=====================================================================================================================
void RmtWriter::WriteAdapterInfo(RmtFileChunkAdapterInfo adapterInfo)
{
    DD_ASSERT(m_state == RmtWriterState::Initialized);

    // Fill out the chunk header
    adapterInfo.header.chunkIdentifier.chunkType  = RMT_FILE_CHUNK_TYPE_ADAPTER_INFO;
    adapterInfo.header.chunkIdentifier.chunkIndex = 0;
    adapterInfo.header.versionMinor               = 1;
    adapterInfo.header.versionMajor               = 0;
    adapterInfo.header.sizeInBytes                = sizeof(adapterInfo);
    adapterInfo.header.padding                    = 0;

    // Then write the data
    WriteBytes(&adapterInfo, sizeof(adapterInfo));
}

//=====================================================================================================================
void RmtWriter::WriteSnapshot(
    const char* pSnapshotName,
    uint64      snapshotTimestamp)
{
    DD_ASSERT(pSnapshotName != nullptr);

    const uint32 snapshotNameLength = static_cast<uint32>(strlen(pSnapshotName));

    RmtFileChunkSnapshotData snapshotData = {};

    // Fill out the chunk header
    snapshotData.header.chunkIdentifier.chunkType  = RMT_FILE_CHUNK_TYPE_SNAPSHOT_INFO;
    snapshotData.header.chunkIdentifier.chunkIndex = 0;
    snapshotData.header.versionMinor               = 1;
    snapshotData.header.versionMajor               = 0;
    snapshotData.header.sizeInBytes                = sizeof(snapshotData) + snapshotNameLength;
    snapshotData.header.padding                    = 0;

    // Fill out the chunk fields
    snapshotData.snapshotPoint = (snapshotTimestamp == 0) ? Platform::QueryTimestamp() : snapshotTimestamp;
    snapshotData.nameLength    = snapshotNameLength;

    // Write the chunk data
    WriteBytes(&snapshotData, sizeof(snapshotData));

    // Write the snapshot name data
    WriteBytes(pSnapshotName, snapshotNameLength);
}

//=====================================================================================================================
// Starts a new RMT data chunk.
void RmtWriter::BeginDataChunk(
    uint64 processId,
    uint64 threadId)
{
    DD_ASSERT(m_state == RmtWriterState::Initialized);

    // Save the current data offset, so we can revisit the data chunk header to update the size once we know how many
    // bytes of token data has been written.
    m_dataChunkHeaderOffset = m_rmtFileData.Size();

    // Create the chunk header with a zero byte size and add it to the stream
    WriteDataChunkHeader(
        processId,
        threadId,
        0,
        0,
        RMT_FILE_DATA_CHUNK_MAJOR_VERSION,
        RMT_FILE_DATA_CHUNK_MINOR_VERSION);

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
void RmtWriter::WriteDataChunk(const void* pData, size_t dataSize)
{
    DD_ASSERT((m_state == RmtWriterState::Initialized) || (m_state == RmtWriterState::WritingDataChunk));

    WriteBytes(pData, dataSize);
}

//=====================================================================================================================
void RmtWriter::WriteDataChunkHeader(
    uint64 processId,
    uint64 threadId,
    size_t dataSize,
    uint32 chunkIndex,
    uint16 rmtMajorVersion,
    uint16 rmtMinorVersion)
{
    DD_ASSERT(m_state == RmtWriterState::Initialized);

    // First create the chunk header and add it to the stream
    RmtFileChunkRmtData chunkHeader = {};
    chunkHeader.header.chunkIdentifier.chunkType  = RMT_FILE_CHUNK_TYPE_RMT_DATA;
    chunkHeader.header.chunkIdentifier.chunkIndex = chunkIndex;
    chunkHeader.header.versionMinor               = rmtMajorVersion;
    chunkHeader.header.versionMajor               = rmtMinorVersion;
    chunkHeader.header.sizeInBytes                = static_cast<int32>(dataSize) + sizeof(chunkHeader);
    chunkHeader.header.padding                    = 0;
    chunkHeader.processId                         = processId;
    chunkHeader.threadId                          = threadId;

    WriteBytes(&chunkHeader, sizeof(chunkHeader));
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
