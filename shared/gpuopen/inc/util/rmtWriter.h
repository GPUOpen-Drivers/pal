/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include <ddPlatform.h>
#include <util/vector.h>

#include <util/ddEventTimer.h>
#include <util/rmtFileFormat.h>
#include <util/rmtTokens.h>

namespace DevDriver
{

class RmtWriter
{
public:
    RmtWriter(const AllocCb& allocCb);
    ~RmtWriter();

    // Initializes the RMT file writer.
    void Init();

    // Resets the internal state of the RMT file writer
    void Reset();

    // Writes a file header chunk to the Rmt file. This is only necessary if the caller is writing an entire file with
    // this writer instance.
    // pFileCreateTime can be provided to set the create time in the RMT file header, if it is null then the current
    //    time will be used.
    void WriteFileHeader(const time_t* pFileCreateTime = nullptr);

    // Writes a SystemInfo chunk to the Rmt file.  Callers may zero initialize the header field, as it will be
    // filled out this function before writing.
    void WriteSystemInfo(RmtFileChunkSystemInfo systemInfo);

    // Writes a SegmentInfo chunk to the Rmt file.  Callers may zero initialize the header field, as it will be
    // filled out this function before writing.
    void WriteSegmentInfo(RmtFileChunkSegmentInfo segmentInfo);

    // Writes an AdapterInfo chunk to the Rmt file.  Callers may zero initialize the header field, as it will be
    // filled out this function before writing.
    void WriteAdapterInfo(RmtFileChunkAdapterInfo adapterInfo);

    // Writes a snapshot chunk to the Rmt file.
    void WriteSnapshot(const char* pSnapshotName, uint64 snapshotTimestamp = 0);

    // These functions are used to create and add token data to an RMT data chunk.
    void BeginDataChunk(uint64 processId, uint64 threadId);
    void WriteTokenData(const RMT_TOKEN_DATA& tokenData);
    // Calculates the 4-bit delta for an RMT token, adding TIMESTAMP or TIME_DELTA tokens to the active data
    // chunk as required
    uint8 CalculateDelta();
    void EndDataChunk();

    // Writes a chunk into the RMT file from an external source
    void WriteDataChunk(const void* pData, size_t dataSize);

    // Alias for WriteDataChunk
    // This function exists to maintain backwards compatibility with older code.
    void WriteData(const void* pData, size_t dataSize) { WriteDataChunk(pData, dataSize); }

    // Writes a chunk header into the RMT file
    void WriteDataChunkHeader(
        uint64 processId,
        uint64 threadId,
        size_t dataSize,
        uint32 chunkIndex,
        uint16 rmtMajorVersion,
        uint16 rmtMinorVersion);

    void Finalize();

    const void* GetRmtData() const { return m_rmtFileData.IsEmpty() ? nullptr : m_rmtFileData.Data(); }
    size_t GetRmtDataSize() const { return m_rmtFileData.Size(); }

private:
    void WriteBytes(const void* pData, size_t dataSize);

    enum RmtWriterState
    {
        Uninitialized,
        Initialized,
        WritingDataChunk,
        Finalized
    };

    const AllocCb&  m_allocCb;
    RmtWriterState  m_state;
    size_t          m_dataChunkHeaderOffset;
    EventTimer      m_eventTimer;
    Vector<uint8>   m_rmtFileData;

};

} // namespace DevDriver
