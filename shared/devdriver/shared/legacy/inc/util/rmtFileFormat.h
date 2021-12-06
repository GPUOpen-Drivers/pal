/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#pragma once

#include <ddPlatform.h>

namespace DevDriver
{

/// Magic number for all RMT files.
#define RMT_FILE_MAGIC_NUMBER (0x494e494d)

#define RMT_FILE_MAJOR_VERSION 1
#define RMT_FILE_MINOR_VERSION 0

/// The maximum number of separate RMT streams in a file.
#define RMT_MAXIMUM_STREAMS  (256)

/// Structure encapsulating the file header of a RMT file.
typedef struct RmtFileHeader
{
    uint32  magicNumber;        ///< Magic number, always set to RMT_FILE_MAGIC_NUMBER.
    uint32  versionMajor;       ///< The major version number of the file.
    uint32  versionMinor;       ///< The minor version number of the file.
    uint32  flags;              ///< Bitfield of flags set with information about the file.
    int32   chunkOffset;        ///< The offset in bytes to the first chunk contained in the file.
    int32   second;             ///< The second in the minute that the RMT file was created.
    int32   minute;             ///< The minute in the hour that the RMT file was created.
    int32   hour;               ///< The hour in the day that the RMT file was created.
    int32   dayInMonth;         ///< The day in the month that the RMT file was created.
    int32   month;              ///< The month in the year that the RMT file was created.
    int32   year;               ///< The year that the RMT file was created.
    int32   dayInWeek;          ///< The day in the week that the RMT file was created.
    int32   dayInYear;          ///< The day in the year that the RMT file was created.
    int32   isDaylightSavings;  ///< Set to 1 if the time is subject to daylight savings.
} RmtFileHeader;

/// An enumeration of all chunk types used in the file format.
typedef enum RmtFileChunkType
{
    RMT_FILE_CHUNK_TYPE_ASIC_INFO     = 0,  ///< A chunk that encodes info about the ASIC on which the RMT file
                                            ///  was generated
    RMT_FILE_CHUNK_TYPE_API_INFO      = 1,  ///< A chunk that encodes info about hte API that the application
                                            ///  generating the RMT file was using.
    RMT_FILE_CHUNK_TYPE_SYSTEM_INFO   = 2,  ///< A chunk containing the description of the system on which the trace
                                            ///  was made.
    RMT_FILE_CHUNK_TYPE_RMT_DATA      = 3,  ///< A chunk containing the RMT data.
    RMT_FILE_CHUNK_TYPE_SEGMENT_INFO  = 4,  ///< A chunk containing segment information for the main process.
    RMT_FILE_CHUNK_TYPE_PROCESS_INFO  = 5,  ///< A chunk containing process state information at the start of the
                                            ///  RMT trace.
    RMT_FILE_CHUNK_TYPE_SNAPSHOT_INFO = 6,  ///< A chunk containing information about a snapshot.
    RMT_FILE_CHUNK_TYPE_ADAPTER_INFO  = 7,  ///< A chunk containing information about the adapter.

    // NOTE: Add new chunks above this.
    RMT_FILE_CHUNK_TYPE_COUNT               ///< The number of different chunk types.
} RmtFileChunkType;

/// An enumeration of flags about the file header.
typedef enum RmtFileChunkFileHeaderFlags
{
    RMT_FILE_HEADER_FLAG_RESERVED  = (1 << 0),  ///< Get the queue timing source
} RmtFileChunkFileHeaderFlags;

/// An enumeration of the API types.
typedef enum RmtApiType
{
    RMT_API_TYPE_DIRECTX_12 = 0,  ///< The trace contains data from a DirectX 12 application.
    RMT_API_TYPE_VULKAN     = 1,  ///< The trace contains data from a Vulkan application.
    RMT_API_TYPE_GENERIC    = 2,  ///< The API of the application is not known.
    RMT_API_TYPE_OPENCL     = 3,  ///< The API of the application is OpenCL.
    RMT_API_TYPE_COUNT            ///< The number of APIs supported.
} RmtApiType;

/// An enumeration of the memory types.
typedef enum RmtMemoryType
{
    RMT_MEMORY_TYPE_UNKNOWN = 0, ///< Unknown memory type
    RMT_MEMORY_TYPE_DDR2,
    RMT_MEMORY_TYPE_DDR3,
    RMT_MEMORY_TYPE_DDR4,
    RMT_MEMORY_TYPE_GDDR5,
    RMT_MEMORY_TYPE_GDDR6,
    RMT_MEMORY_TYPE_HBM,
    RMT_MEMORY_TYPE_HBM2,
    RMT_MEMORY_TYPE_HBM3,
    RMT_MEMORY_TYPE_COUNT        ///< The number of memory types supported.
} RmtMemoryType;

/// A structure encapsulating a single chunk identifier.
typedef struct RmtFileChunkIdentifier
{
    union
    {
        struct
        {
            RmtFileChunkType chunkType  : 8;   ///< The type of chunk.
            int32            chunkIndex : 8;   ///< The index of the chunk.
            int32            reserved   : 16;  ///< Reserved, set to 0.
        };

        uint32              value;            ///< 32bit value containing all the above fields.
    };
} RmtFileChunkIdentifier;

/// A structure encapsulating common fields of a chunk in the RMT file format.
typedef struct RmtFileChunkHeader
{
    RmtFileChunkIdentifier chunkIdentifier;  ///< A unique identifier for the chunk.
    int16                  versionMinor;     ///< The minor version of the chunk. Please see above note on ordering
                                             ///  of minor and major version
    int16                  versionMajor;     ///< The major version of the chunk.
    int32                  sizeInBytes;      ///< The size of the chunk in bytes.
    int32                  padding;          ///< Reserved padding dword.
} RmtFileChunkHeader;

    // This version number matches the spec revision version
    #define RMT_FILE_DATA_CHUNK_MAJOR_VERSION 1
    #define RMT_FILE_DATA_CHUNK_MINOR_VERSION 6

/// A structure encapsulating information about the location of the RMT data within the RMT file itself.
typedef struct RmtFileChunkRmtData
{
    RmtFileChunkHeader header;     ///< Common header for all chunks.
    uint64             processId;  ///< The process ID that generated this RMT data. If unknown program to 0.
    uint64             threadId;   ///< The CPU thread ID of the thread in the application that generated this RMT data.
} RmtFileChunkRmtData;

/// A structure encapsulating system information.
typedef struct RmtFileChunkSystemInfo
{
    RmtFileChunkHeader header;              ///< Common header for all chunks.
    char               vendorId[16];        ///< For x86 CPUs this is based off the 12 character ASCII string retreived
                                            ///  via CPUID instruction.
    char               processorBrand[48];  ///< For x86 CPUs this is based off the 48 byte null-terminated ASCII
                                            ///  processor brand using CPU instruction.
    uint64             padding;             ///< Padding after 48 byte string.
    uint64             timestampFrequency;  ///< The frequency of the timestamp clock (in Hz). For windows this is the
                                            ///  same as reported by the QueryPerformanceFrequency API.
    uint32             clockSpeed;          ///< The maximum clock frequency of the CPU (in MHz).
    int32              logicCores;          ///< The number of logical cores.
    int32              physicalCores;       ///< The number of physical cores.
    int32              systemRamInMB;       ///< The amount of system RAM expressed in MB.
} RmtFileChunkSystemInfo;

/// A structure encapsulating information about a single snapshot.
/// The name of the snapshot is written directly after this chunk structure.
typedef struct RmtFileChunkSnapshotData
{
    RmtFileChunkHeader header;        ///< Common header for all chunks.
    uint64             snapshotPoint; ///< 64bit timestamp of the snapshot.
    uint32             nameLength;    ///< Size in bytes of the snapshot name.
} RmtFileChunkSnapshotData;

/// A structure encapsulating information about a segment of memory.
typedef struct RmtFileChunkSegmentInfo
{
    RmtFileChunkHeader header;              ///< Common header for all chunks.
    uint64             physicalBaseAddress; ///< Physical base address of the segment.
    uint64             size;                ///< Size in bytes of the segment.
    uint32             heapType;            ///< Type of heap.
    uint32             memoryTypeIndex;     ///< Memory type index.
} RmtFileChunkSegmentInfo;

/// A structure encapsulating information about a segment of memory.
typedef struct RmtFileChunkAdapterInfo
{
    RmtFileChunkHeader header;            ///< Common header for all chunks.

    char               name[128];         ///< Name of the gpu

    uint32             familyId;          ///< PCI Family
    uint32             revisionId;        ///< PCI Revision
    uint32             deviceId;          ///< PCI Device

    uint32             minEngineClock;    ///< Minumum engine clock in Mhz
    uint32             maxEngineClock;    ///< Maximum engine clock in Mhz

    uint32             memoryType;        ///< Type of memory
    uint32             memoryOpsPerClock; ///< Number of memory operations per clock
    uint32             memoryBusWidth;    ///< Bus width of memory interface in bits
    uint32             memoryBandwidth;   ///< Bandwidth of memory in MB/s
    uint32             minMemoryClock;    ///< Minumum memory clock in Mhz
    uint32             maxMemoryClock;    ///< Minumum memory clock in Mhz
} RmtFileChunkAdapterInfo;

} // namespace DevDriver
