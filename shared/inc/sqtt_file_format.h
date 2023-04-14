/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef SQTT_FILE_FORMAT_H
#define SQTT_FILE_FORMAT_H

#include <stdint.h>
#include "palInlineFuncs.h"
#include "palPerfExperiment.h"

/** Magic number for all SQTT files.
 */
#define SQTT_FILE_MAGIC_NUMBER            0x50303042

/// The major version number of the RGP file format specification that this header corresponds to.
#define RGP_FILE_FORMAT_SPEC_MAJOR_VER 1

/// The minor version number of the RGP file format specification that this header corresponds to.
#define RGP_FILE_FORMAT_SPEC_MINOR_VER 5

struct RgpChunkVersionNumbers
{
    uint16_t majorVersion;
    uint16_t minorVersion;
};

#ifdef __cplusplus
extern "C" {
#endif

/** An structure defining available File Header flags.
*/
typedef struct SqttFileHeaderFlags
{
    union
    {
        struct
        {
            int32_t  isSemaphoreQueueTimingETW            :  1;  /*!< Indicates the source of semaphore queue timing
                                                                      data is ETW. */
            int32_t  noQueueSemaphoreTimeStamps           :  1;  /*!< Indicates the queue timing data does not perform
                                                                      dummy submits for semaphore signal/wait timestamps
                                                                      and just reports those timestamps as 0. */
            int32_t  reserved                             : 30;  /*!< Reserved, set to 0. */
        };

        uint32_t value;                              /*!< 32bit value containing all the above fields. */
    };
} SqttFileHeaderFlags;

/** Structure encapsulating the file header of an SQTT file.
 */
typedef struct SqttFileHeader
{
    uint32_t                magicNumber;       /*!< Magic number, always set to <c><i>SQTT_FILE_MAGIC_NUMBER</i></c> */
    uint32_t                versionMajor;      /*!< The major version number of the file. */
    uint32_t                versionMinor;      /*!< The minor version number of the file. */
    SqttFileHeaderFlags     flags;             /*!< Bitfield of flags set with information about the file. */
    int32_t                 chunkOffset;       /*!< The offset in bytes to the first chunk contained in the file. */
    int32_t                 second;            /*!< The second in the minute that the RGP file was created. */
    int32_t                 minute;            /*!< The minute in the hour that the RGP file was created. */
    int32_t                 hour;              /*!< The hour in the day that the RGP file was created. */
    int32_t                 dayInMonth;        /*!< The day in the month that the RGP file was created. */
    int32_t                 month;
    int32_t                 year;
    int32_t                 dayInWeek;
    int32_t                 dayInYear;
    int32_t                 isDaylightSavings;
} SqttFileHeader;

/** An enumeration of all chunk types used in the file format.
 */
typedef enum SqttFileChunkType
{
    SQTT_FILE_CHUNK_TYPE_ASIC_INFO,                    /*!< Description of the ASIC on which the trace was made. */
    SQTT_FILE_CHUNK_TYPE_SQTT_DESC,                    /*!< Description of the SQTT data. */
    SQTT_FILE_CHUNK_TYPE_SQTT_DATA,                    /*!< SQTT data for a single shader engine. */
    SQTT_FILE_CHUNK_TYPE_API_INFO,                     /*!< Description of the API on which the trace was made. */
    SQTT_FILE_CHUNK_TYPE_RESERVED,                     /*!< Reserved (Should not be used). */
    SQTT_FILE_CHUNK_TYPE_QUEUE_EVENT_TIMINGS,          /*!< Timings for queue events that occurred during trace. */
    SQTT_FILE_CHUNK_TYPE_CLOCK_CALIBRATION,            /*!< Information required to correlate between clock domains. */
    SQTT_FILE_CHUNK_TYPE_CPU_INFO,                     /*!< Description of the CPU on which the trace was made. */
    SQTT_FILE_CHUNK_TYPE_SPM_DB,                       /*!< SPM trace data. */
    SQTT_FILE_CHUNK_TYPE_CODE_OBJECT_DATABASE,         /*!< Pipeline code object database. */
    SQTT_FILE_CHUNK_TYPE_CODE_OBJECT_LOADER_EVENTS,    /*!< Code object loader event data. */
    SQTT_FILE_CHUNK_TYPE_PSO_CORRELATION,              /*!< Pipeline State Object -> Code Object correlation mapping. */
    SQTT_FILE_CHUNK_TYPE_RESERVED1,                    /*!< Reserved (Should not be used). */
    SQTT_FILE_CHUNK_TYPE_DF_SPM_DB,                    /*!< DF SPM trace data. */
    SQTT_FILE_CHUNK_TYPE_INSTRUMENTATION_TABLE,        /*!< Instrumentation table. */

    SQTT_FILE_CHUNK_TYPE_COUNT,
} SqttFileChunkType;

/// Lookup table providing the major version and minor version numbers for the RGP chunks within this header.
static constexpr RgpChunkVersionNumbers RgpChunkVersionNumberLookup[] =
{
    {0, 6}, // SQTT_FILE_CHUNK_TYPE_ASIC_INFO,
    {0, 2}, // SQTT_FILE_CHUNK_TYPE_SQTT_DESC,
    {0, 0}, // SQTT_FILE_CHUNK_TYPE_SQTT_DATA,
    {0, 2}, // SQTT_FILE_CHUNK_TYPE_API_INFO,
    {0, 0}, // SQTT_FILE_CHUNK_TYPE_RESERVED,
    {1, 1}, // SQTT_FILE_CHUNK_TYPE_QUEUE_EVENT_TIMINGS,
    {0, 0}, // SQTT_FILE_CHUNK_TYPE_CLOCK_CALIBRATION,
    {0, 0}, // SQTT_FILE_CHUNK_TYPE_CPU_INFO,
    {2, 0}, // SQTT_FILE_CHUNK_TYPE_SPM_DB,
    {0, 0}, // SQTT_FILE_CHUNK_TYPE_CODE_OBJECT_DATABASE,
    {1, 1}, // SQTT_FILE_CHUNK_TYPE_CODE_OBJECT_LOADER_EVENTS
    {0, 0}, // SQTT_FILE_CHUNK_TYPE_PSO_CORRELATION
    {0, 0}, // SQTT_FILE_CHUNK_TYPE_RESERVED1
    {0, 0}, // SQTT_FILE_CHUNK_TYPE_DF_SPM_DB,
    {0, 0}, // SQTT_FILE_CHUNK_TYPE_INSTRUMENTATION_TABLE
};

static_assert(Util::ArrayLen(RgpChunkVersionNumberLookup) == static_cast<uint32_t>(SQTT_FILE_CHUNK_TYPE_COUNT),
              "The version number lookup table must be updated when adding/deleting a chunk!");

/** An enumeration of flags about ASIC info.
 */
typedef enum SqttFileChunkAsicInfoFlags
{
    SQTT_FILE_CHUNK_ASIC_INFO_FLAG_SC_PACKER_NUMBERING      = (1 << 0),
    SQTT_FILE_CHUNK_ASIC_INFO_FLAG_PS1_EVENT_TOKENS_ENABLED = (1 << 1)
} SqttFileChunkAsicInfoFlags;

/** An enumeration of the API types.
 */
typedef enum SqttApiType
{
    SQTT_API_TYPE_DIRECTX_12 = 0,
    SQTT_API_TYPE_VULKAN     = 1,
    SQTT_API_TYPE_GENERIC    = 2,
    SQTT_API_TYPE_OPENCL     = 3,
    SQTT_API_TYPE_HIP        = 5
} SqttApiType;

/** A structure encapsulating a single chunk identifier.

    A chunk identifier comprises of the chunk type, and an index. The index is unique for each instance of the chunk.
    For example, if a specific ASIC had 4 Shader Engines there would be multiple SQTT_FILE_CHUNK_SQTT_DATA with
    indicies ranging from [0..3].
 */
typedef struct SqttFileChunkIdentifier
{
    union
    {
        struct
        {
            SqttFileChunkType   chunkType   : 8;            /*!< The type of chunk. */
            int32_t             chunkIndex  : 8;            /*!< The index of the chunk. */
            int32_t             reserved    : 16;           /*!< Reserved, set to 0. */
        };

        uint32_t value;                                     /*!< 32bit value containing all the above fields. */
    };
} SqttFileChunkIdentifier;

/** A structure encapsulating common fields of a chunk in the SQTT file format.
 */
typedef struct SqttFileChunkHeader
{
    SqttFileChunkIdentifier     chunkIdentifier;            /*!< A unique identifier for the chunk. */
    uint16_t                    minorVersion;               /*!< The minor version of the chunk. */
    uint16_t                    majorVersion;               /*!< The major version of the chunk. */
    int32_t                     sizeInBytes;                /*!< The size of the chunk in bytes. */
    int32_t                     padding;                    /*!< Reserved padding dword. */
} SqttFileChunkHeader;

/** An enumeration of GPU types.
 */
typedef enum SqttGpuType
{
    SQTT_GPU_TYPE_UNKNOWN    = 0x0,
    SQTT_GPU_TYPE_INTEGRATED = 0x1,
    SQTT_GPU_TYPE_DISCRETE   = 0x2,
    SQTT_GPU_TYPE_VIRTUAL    = 0x3
} SqttGpuType;

/** An enumeration of gfx-ip levels.
 */
typedef enum SqttGfxIpLevel
{
    SQTT_GFXIP_LEVEL_NONE       = 0x0,
    SQTT_GFXIP_LEVEL_GFXIP_6    = 0x1,
    SQTT_GFXIP_LEVEL_GFXIP_7    = 0x2,
    SQTT_GFXIP_LEVEL_GFXIP_8    = 0x3,
    SQTT_GFXIP_LEVEL_GFXIP_8_1  = 0x4,
    SQTT_GFXIP_LEVEL_GFXIP_9    = 0x5,
    SQTT_GFXIP_LEVEL_GFXIP_10_1 = 0x7,
    SQTT_GFXIP_LEVEL_GFXIP_10_3 = 0x9,
#if PAL_BUILD_GFX11
    SQTT_GFXIP_LEVEL_GFXIP_11_0 = 0xC,
#endif
} SqttGfxIpLevel;

/** An enumeration of memory types.
 */
typedef enum SqttMemoryType
{
    SQTT_MEMORY_TYPE_UNKNOWN = 0x0,
    SQTT_MEMORY_TYPE_DDR     = 0x1,
    SQTT_MEMORY_TYPE_DDR2    = 0x2,
    SQTT_MEMORY_TYPE_DDR3    = 0x3,
    SQTT_MEMORY_TYPE_DDR4    = 0x4,
    SQTT_MEMORY_TYPE_DDR5    = 0x5,
    SQTT_MEMORY_TYPE_GDDR3   = 0x10,
    SQTT_MEMORY_TYPE_GDDR4   = 0x11,
    SQTT_MEMORY_TYPE_GDDR5   = 0x12,
    SQTT_MEMORY_TYPE_GDDR6   = 0x13,
    SQTT_MEMORY_TYPE_HBM     = 0x20,
    SQTT_MEMORY_TYPE_HBM2    = 0x21,
    SQTT_MEMORY_TYPE_HBM3    = 0x22,
    SQTT_MEMORY_TYPE_LPDDR4  = 0x30,
    SQTT_MEMORY_TYPE_LPDDR5  = 0x31,
} SqttMemoryType;

const uint32_t SQTT_GPU_NAME_MAX_SIZE               = 256;
const uint32_t SQTT_MAX_NUM_SE                      = 32;
const uint32_t SQTT_SA_PER_SE                       = 2;
const uint32_t SQTT_ACTIVE_PIXEL_PACKER_MASK_DWORDS = 4;

/** A structure encapsulating information about the ASIC on which the trace was performed.
 */
typedef struct SqttFileChunkAsicInfo
{
    SqttFileChunkHeader header;                          /*!< Common header for all chunks. */
    uint64_t            flags;                           /*!< Flags for the ASIC info chunk. */
    uint64_t            traceShaderCoreClock;            /*!< The shader core clock frequency during SQTT traces. */
    uint64_t            traceMemoryClock;                /*!< The memory clock frequency during SQTT traces. */
    int32_t             deviceId;                        /*!< Device ID for the card where the trace was performed. */
    int32_t             deviceRevisionId;                /*!< Device revision ID for card where trace was performed. */
    int32_t             vgprsPerSimd;                    /*!< The number of VGPRs per SIMD. */
    int32_t             sgprsPerSimd;                    /*!< The number of SGPRs per SIMD. */
    int32_t             shaderEngines;                   /*!< The number of shader engines. */
    int32_t             computeUnitPerShaderEngine;      /*!< The number of compute units per shader engine. */
    int32_t             simdPerComputeUnit;              /*!< The number of SIMDs per compute unit. */
    int32_t             wavefrontsPerSimd;               /*!< The number of wavefronts per SIMD. */
    int32_t             minimumVgprAlloc;                /*!< Minimum number of VGPRs per wavefront. */
    int32_t             vgprAllocGranularity;            /*!< The allocation granularity of VGPRs. */
    int32_t             minimumSgprAlloc;                /*!< Minimum number of SGPRs per wavefront. */
    int32_t             sgprAllocGranularity;            /*!< The allocation granularity of SGPRs. */
    int32_t             hardwareContexts;                /*!< The number of hardware contexts. */
    SqttGpuType         gpuType;                         /*!< The type of GPU. */
    SqttGfxIpLevel      gfxIpLevel;                      /*!< The gfxip level of this GPU. */
    int32_t             gpuIndex;                        /*!< The index of this GPU. */
    int32_t             gdsSize;                         /*!< Size in bytes of global data store in bytes in GPU. */
    int32_t             gdsPerShaderEngine;              /*!< Max size in bytes of GDS space available per SE. */
    int32_t             ceRamSize;                       /*!< Max size in bytes of CE RAM space available. */
    int32_t             ceRamSizeGraphics;               /*!< Max CE RAM size available to graphics engine in bytes.*/
    int32_t             ceRamSizeCompute;                /*!< Max CE RAM size available to Compute engine in bytes.*/
    int32_t             maxNumberOfDedicatedCus;         /*!< The number of CUs dedicated to real time audio queue. */
    int64_t             vramSize;                        /*!< The total number of bytes to VRAM. */
    int32_t             vramBusWidth;                    /*!< The width of the bus to VRAM. */
    int32_t             l2CacheSize;                     /*!< The total number of bytes in L2 Cache. */
    int32_t             l1CacheSize;                     /*!< The total number of L1 cache bytes per CU.. */
    int32_t             ldsSize;                         /*!< The total number of LDS bytes per CU.. */
    char                gpuName[SQTT_GPU_NAME_MAX_SIZE]; /*!< The name of the GPU. Padded to 256 bytes. */
    float               aluPerClock;                     /*!< The number of ALUs per clock. */
    float               texturePerClock;                 /*!< The number of texture per clock. */
    float               primsPerClock;                   /*!< The number of primitives per clock. */
    float               pixelsPerClock;                  /*!< The number of pixels per clock. */
    uint64_t            gpuTimestampFrequency;           /*!< The frequency of the gpu timestamp clock in Hz. */
    uint64_t            maxShaderCoreClock;              /*!< The max shader core clock frequency. */
    uint64_t            maxMemoryClock;                  /*!< The max memory clock frequency. */
    uint32_t            memoryOpsPerClock;               /*!< Number of memory operations per memory clock cycle. */
    SqttMemoryType      memoryChipType;                  /*!< The type of memory chip used by the asic. */
    uint32_t            ldsGranularity;                  /*!< The LDS allocation granularity expressed in bytes. */
    uint16_t            cuMask[SQTT_MAX_NUM_SE][SQTT_SA_PER_SE];
                                                         /*!< Mask of present, non-harvested CUs (physical layout) */
    char                reserved1[128];                  /*!< Reserved for future changes to CU mask */
    uint32_t            activePixelPackerMask[SQTT_ACTIVE_PIXEL_PACKER_MASK_DWORDS];
                                                         /*!< Mask of live pixel packers. Max 32 SEs and 4 packers/SE */
    char                reserved2[16];                   /*!< Reserved for future changes to pixel packer mask */
    uint32_t            gl1CacheSize;                    /*!< Total size of GL1 cache per shader array in bytes */
    uint32_t            instructionCacheSize;            /*!< Total size of instruction cache per CU/WGP in bytes */
    uint32_t            scalarCacheSize;                 /*!< Total size of scalar cache per CU/WGP in bytes */
    uint32_t            mallCacheSize;                   /*!< Total size of MALL cache in bytes */
} SqttFileChunkAsicInfo;

static_assert(sizeof(SqttFileChunkAsicInfo::cuMask)    == 1024 / 8, "cuMask doesn't match RGP Spec");
static_assert(sizeof(SqttFileChunkAsicInfo::cuMask[0]) == 32   / 8, "cuMask SE size doesn't match RGP Spec");
static_assert(sizeof(SqttFileChunkAsicInfo::reserved1) == 1024 / 8, "reserved1 doesn't match RGP Spec");
static_assert(sizeof(SqttFileChunkAsicInfo::activePixelPackerMask) == 128 / 8,
                                                                    "activePixelPackerMask doens't match RGP Spec");
static_assert(sizeof(SqttFileChunkAsicInfo::reserved2) == 128  / 8, "reserved2 doesn't match RGP Spec");

/** An enumeration of the SQTT profiling mode.
*/
typedef enum SqttProfilingMode
{
    SQTT_PROFILING_MODE_PRESENT      = 0x0,            /*!< Present based profiling. */
    SQTT_PROFILING_MODE_USER_MARKERS = 0x1,            /*!< User Marker based profiling. */
    SQTT_PROFILING_MODE_INDEX        = 0x2,            /*!< Index (dispatch/frame number) based profiling. */
    SQTT_PROFILING_MODE_TAG          = 0x3,            /*!< Tag based profiling. */
} SqttProfilingMode;

static constexpr uint32_t kUserMarkerStringLength = 256;

/** An union of the SQTT profiling mode data.
*/
typedef union SqttProfilingModeData
{
    struct
    {
        char start[kUserMarkerStringLength];
        char end[kUserMarkerStringLength];
    } userMarkerProfilingData;

    struct
    {
        uint32_t start;
        uint32_t end;
    } indexProfilingData;

    struct
    {
        uint32_t beginHi;
        uint32_t beginLo;
        uint32_t endHi;
        uint32_t endLo;
    } tagProfilingData;

} SqttProfilingModeData;

/** An enumeration of the SQTT instruction trace mode.
*/
typedef enum SqttInstructionTraceMode
{
    SQTT_INSTRUCTION_TRACE_DISABLED   = 0x0, /*!< Instruction trace was disabled. */
    SQTT_INSTRUCTION_TRACE_FULL_FRAME = 0x1, /*!< Instruction trace was enabled for the full frame. */
    SQTT_INSTRUCTION_TRACE_API_PSO    = 0x2, /*!< Instruction trace was enabled for a single PSO. */
} SqttInstructionTraceMode;

/** An structure containing the SQTT instruction trace mode data. This is either the API Pso filter or the shader engine
 *  filter that was used to control which shader engine(s) instruction tracing data was captured from.
*/
typedef union SqttInstructionTraceData
{
    struct
    {
        uint64_t apiPsoFilter;
    } apiPsoData;

    struct
    {
        uint32_t mask;
    } shaderEngineFilter;

} SqttInstructionTraceData;

/** A structure encapsulating information about the API on which the trace was performed.
 */
typedef struct SqttFileChunkApiInfo
{
    SqttFileChunkHeader         header;                     /*!< Common header for all chunks. */
    SqttApiType                 apiType;                    /*!< The type of API used. */
    uint16_t                    versionMajor;               /*!< The major API version. */
    uint16_t                    versionMinor;               /*!< The minor API version. */
    SqttProfilingMode           profilingMode;              /*!< The profiling mode used to capture this trace. */
    uint32_t                    reserved;                   /*!< Reserved for 64-bit alignment. */
    SqttProfilingModeData       profilingModeData;          /*!< The input arguments provided for the profilingMode. */
    SqttInstructionTraceMode    instructionTraceMode;       /*!< The mode used for instruction tracing. */
    uint32_t                    reserved2;                  /*!< Reserved for 64-bit alignment. */
    SqttInstructionTraceData    instructionTraceData;       /*!< Input arguments related to instructionTraceMode. */
} SqttFileChunkApiInfo;

/** An enumeration of the SQTT versions.
 */
typedef enum SqttVersion
{
    SQTT_VERSION_NONE         = 0x0,                        /*!< Not supported. */
    SQTT_VERSION_1_0          = 0x1,                        /*!< TT 1.0 Evergreen ("8xx"). */
    SQTT_VERSION_1_1          = 0x2,                        /*!< TT 1.1 Northern Islands ("9xx"). */
    SQTT_VERSION_2_0          = 0x3,                        /*!< TT 2.0 Southern Islands ("GfxIp6"). */
    SQTT_VERSION_2_1          = 0x4,                        /*!< TT 2.1 Sea Islands ("GfxIp7"). */
    SQTT_VERSION_2_2          = 0x5,                        /*!< TT 2.2 Volcanic Islands ("GfxIp8"). */
    SQTT_VERSION_2_3          = 0x6,                        /*!< TT 2.3 Vega, MI100, MI200 (GfxIp9). */
    SQTT_VERSION_3_0          = 0x7,                        /*!< TT 3.0 Navi1, Navi2 (GfxIp10-10.3). */
    SQTT_VERSION_2_4          = SQTT_VERSION_3_0,           /*!< Left for legacy reasons. */
    SQTT_VERSION_RESERVED_0x8 = 0x8,                        /*!< Reserved. */
    SQTT_VERSION_RESERVED_0x9 = 0x9,                        /*!< Reserved. */
    SQTT_VERSION_RESERVED_0xA = 0xA,                        /*!< Reserved. */
#if PAL_BUILD_GFX11
    SQTT_VERSION_3_2          = 0xB,                        /*!< TT 3.2 */
#else
    SQTT_VERSION_RESERVED_0xB = 0xB,                        /*!< Reserved. */
#endif
} SqttVersion;

/** A structure encapsulating the description of the data contained in the matching SQTT_FILE_CHUNK_SQTT_DATA chunk.
 */
typedef struct SqttFileChunkSqttDesc
{
    SqttFileChunkHeader     header;                             /*!< Common header for all file chunks. */
    int32_t                 shaderEngineIndex;                  /*!< The shader engine index for the SQTT data. */
    SqttVersion             sqttVersion;                        /*!< The version of the SQTT that is implemented. */
    union
    {
        struct
        {
            int32_t                 instrumentationVersion;     /*!< The major version number of the instrumentation
                                                                specification that matching SQTT_DATA chunk contains */
        } v0;

        struct
        {
            int16_t                 instrumentationSpecVersion; /*!< Version of instrumentation as defined by spec. */
            int16_t                 instrumentationApiVersion;  /*!< Version of instrumentation as defined by api. */
            int32_t                 computeUnitIndex;           /*!< Phys index of compute unit that executed trace */
        } v1;
    };
} SqttFileChunkSqttDesc;

/** A structure encapsulating information about the location of the SQTT data within the SQTT file itself.
 */
typedef struct SqttFileChunkSqttData
{
    SqttFileChunkHeader         header;                     /*!< Common header for all chunks. */
    int32_t                     offset;                     /*!< Offset from start of file (bytes) to SQTT data. */
    int32_t                     size;                       /*!< The size (in bytes) of the SQTT data. */
} SqttFileChunkSqttData;

/**  A structure encapsulating information about the code object database.
 */
typedef struct SqttFileChunkCodeObjectDatabase
{
    SqttFileChunkHeader header;
    uint32_t            offset;
    uint32_t            flags;
    uint32_t            size;
    uint32_t            recordCount;
} SqttFileChunkCodeObjectDatabase;

/**  A structure encapsulating information about each record in the code object database.
 */
typedef struct SqttCodeObjectDatabaseRecord
{
    uint32_t  recordSize;  /*!< The size of the code object in bytes. */
} SqttCodeObjectDatabaseRecord;

/**  A structure encapsulating information for a timeline of code object loader events.
 */
typedef struct SqttFileChunkCodeObjectLoaderEvents
{
    SqttFileChunkHeader header;
    uint32_t            offset;
    uint32_t            flags;
    uint32_t            recordSize;                         /*!< Size of a single SqttCodeObjectLoaderEventRecord. */
    uint32_t            recordCount;
} SqttFileChunkCodeObjectLoaderEvents;

/** An enumeration of the code object loader event types.
*/
typedef enum SqttCodeObjectLoaderEventType
{
    SQTT_CODE_OBJECT_LOAD_TO_GPU_MEMORY     = 0x00000000,
    SQTT_CODE_OBJECT_UNLOAD_FROM_GPU_MEMORY = 0x00000001
} SqttCodeObjectLoaderEventType;

/**  A structure encapsulating a 128-bit hash.
*/
typedef struct SqttHash128
{
    uint64_t lower;   /*!< Lower 64-bits of hash. */
    uint64_t upper;   /*!< Upper 64-bits of hash. */
} SqttHash128;

/**  A structure encapsulating information about each record in the loader events chunk.
 */
typedef struct SqttCodeObjectLoaderEventRecord
{
    SqttCodeObjectLoaderEventType eventType;          /*!< The type of loader event. */
    uint32_t                      reserved;           /*!< Reserved. */
    uint64_t                      baseAddress;        /*!< The base address where the code object has been loaded. */
    SqttHash128                   codeObjectHash;     /*!< Code object hash. For now, same as internal pipeline hash. */
    uint64_t                      timestamp;          /*!< CPU timestamp of this event in clock cycle units. */
} SqttCodeObjectLoaderEventRecord;

/**  A structure encapsulating information for Pipeline State Object correlation mappings.
 */
typedef struct SqttFileChunkPsoCorrelation
{
    SqttFileChunkHeader header;
    uint32_t            offset;
    uint32_t            flags;
    uint32_t            recordSize;                         /*!< Size of a single SqttPsoCorrelationRecord. */
    uint32_t            recordCount;
} SqttFileChunkPsoCorrelation;

/**  A structure encapsulating information about each record in the PSO correlation chunk.
 */
typedef struct SqttPsoCorrelationRecord
{
    uint64_t    apiPsoHash;            /*!< API PSO hash provided by the client driver. */
    SqttHash128 internalPipelineHash;  /*!< Internal pipeline hash provided by the pipeline compiler. */
    char        apiObjectName[64];     /*!< (Optional) Debug object name as a null-terminated string. */
} SqttPsoCorrelationRecord;

/** A structure encapsulating information about the API on which the trace was performed.
 */
typedef struct SqttFileChunkQueueEventTimings
{
    SqttFileChunkHeader header;                     /*!< Common header for all chunks. */

    // Record count and size (in bytes) of the table of SqttQueueInfoRecord structs that describes information about
    // queues that measured timing events.
    uint32_t queueInfoTableRecordCount;
    uint32_t queueInfoTableSize;

    // Record count and size (in bytes) of the table of SqttQueueEventRecord structs that describes information about
    // each measured queue event.
    uint32_t queueEventTableRecordCount;
    uint32_t queueEventTableSize;
} SqttFileChunkQueueEventTimings;

/** An enumeration of all valid queue types.
*/
typedef enum SqttQueueType
{
    SQTT_QUEUE_TYPE_UNKNOWN         = 0x0,
    SQTT_QUEUE_TYPE_UNIVERSAL       = 0x1,
    SQTT_QUEUE_TYPE_COMPUTE         = 0x2,
    SQTT_QUEUE_TYPE_DMA             = 0x3,
} SqttQueueType;

/** An enumeration of all valid engine types.
*/
typedef enum SqttEngineType
{
    SQTT_ENGINE_TYPE_UNKNOWN                 = 0x0,
    SQTT_ENGINE_TYPE_UNIVERSAL               = 0x1,
    SQTT_ENGINE_TYPE_COMPUTE                 = 0x2,
    SQTT_ENGINE_TYPE_EXCLUSIVE_COMPUTE       = 0x3,
    SQTT_ENGINE_TYPE_DMA                     = 0x4,
    SQTT_ENGINE_TYPE_HIGH_PRIORITY_UNIVERSAL = 0x7,
    SQTT_ENGINE_TYPE_HIGH_PRIORITY_GRAPHICS  = 0x8,
} SqttEngineType;

/** A structure encapsulating hardware information about a queue.
*/
typedef struct SqttQueueHardwareInfo
{
    union
    {
        struct
        {
            SqttQueueType  queueType  : 8;  /*!< The logical type of queue. */
            SqttEngineType engineType : 8;  /*!< The type of hardware engine the queue is mapped to. */
            uint32_t       reserved   : 16; /*!< Reserved, set to 0. */
        };

        uint32_t value;                     /*!< 32bit value containing all the above fields. */
    };
} SqttQueueHardwareInfo;

// Queue-specific information about each queue that measured timing events
typedef struct SqttQueueInfoRecord
{
    uint64_t              queueID;      // API-specific queue ID (e.g. VkQueue handle for Vulkan queues)
    uint64_t              queueContext; // OS context value
    SqttQueueHardwareInfo hardwareInfo; // Hardware level queue info
    uint32_t              reserved;     // Space reserved for future use.
} SqttQueueInfoRecord;

// Value of SqttQueueEventItem eventType field (see below)
enum SqttQueueEventType
{
    SQTT_QUEUE_TIMING_EVENT_CMDBUF_SUBMIT,
    SQTT_QUEUE_TIMING_EVENT_SIGNAL_SEMAPHORE,
    SQTT_QUEUE_TIMING_EVENT_WAIT_SEMAPHORE,
    SQTT_QUEUE_TIMING_EVENT_PRESENT
};

// Information about a particular timed queue event.
typedef struct SqttQueueEventRecord
{
    uint32_t eventType;         // Type of the timing event
    uint32_t sqttCbId;          // SQTT command buffer id. Only valid for Submit type events
    uint64_t frameIndex;        // Global frame index. Starts at 1 and is incremented for each present.
    uint32_t queueInfoIndex;    // Index into the SqttQueueInfoRecord table of which queue triggered this event
    uint32_t submitSubIndex;    // Sub index of event within a submission. Only valid for Submit type events.
    uint64_t apiId;             // Api specific value that is also specific to each event type.
                                // Submit = API Command Buffer ID that was submitted
                                // Signal = API Semaphore ID that was signaled
                                // Wait   = API Semaphore ID that was waited on
    uint64_t cpuTimestamp;      // Cpu timestamp of when this event was triggered in clock cycle units.
    uint64_t gpuTimestamps[2];  // Event specific gpu timestamps for this event in clock cycle units.
} SqttQueueEventRecord;

/** A structure encapsulating information about the clock domains involved in the trace.
 */
typedef struct SqttFileChunkClockCalibration
{
    SqttFileChunkHeader header;       /*!< Common header for all chunks. */

    uint64_t            cpuTimestamp; /*!< Cpu timestamp value measured in clock cycle units. */
    uint64_t            gpuTimestamp; /*!< Gpu timestamp value measured in clock cycle units. */
    uint64_t            reserved;     /*!< Space reserved for future use. */
} SqttFileChunkClockCalibration;

/** A structure encapsulating information about the cpu used in the trace.
 */
typedef struct SqttFileChunkCpuInfo
{
    SqttFileChunkHeader header;                /*!< Common header for all chunks. */

    uint32_t            vendorId[4];           /*!< Cpu vendor identifier. */
    uint32_t            processorBrand[12];    /*!< Cpu brand string. */
    uint32_t            reserved[2];           /*!< Reserved for future use. */
    uint64_t            cpuTimestampFrequency; /*!< The frequency of the cpu timestamp clock in Hz. */
    uint32_t            clockSpeed;            /*!< The maximum clock speed of the cpu in MHz. */
    uint32_t            numLogicalCores;       /*!< The number of threads that can run simultaneously on the cpu. */
    uint32_t            numPhysicalCores;      /*!< The number of physical cores in the cpu. */
    uint32_t            systemRamSize;         /*!< The size of system RAM in MB. */
} SqttFileChunkCpuInfo;

typedef enum SpmGpuBlock
{
    SPM_GPU_BLOCK_CPF     = 0x0,
    SPM_GPU_BLOCK_IA      = 0x1,
    SPM_GPU_BLOCK_VGT     = 0x2,
    SPM_GPU_BLOCK_PA      = 0x3,
    SPM_GPU_BLOCK_SC      = 0x4,
    SPM_GPU_BLOCK_SPI     = 0x5,
    SPM_GPU_BLOCK_SQ      = 0x6,
    SPM_GPU_BLOCK_SX      = 0x7,
    SPM_GPU_BLOCK_TA      = 0x8,
    SPM_GPU_BLOCK_TD      = 0x9,
    SPM_GPU_BLOCK_TCP     = 0xA,
    SPM_GPU_BLOCK_TCC     = 0xB,
    SPM_GPU_BLOCK_TCA     = 0xC,
    SPM_GPU_BLOCK_DB      = 0xD,
    SPM_GPU_BLOCK_CB      = 0xE,
    SPM_GPU_BLOCK_GDS     = 0xF,
    SPM_GPU_BLOCK_SRBM    = 0x10,
    SPM_GPU_BLOCK_GRBM    = 0x11,
    SPM_GPU_BLOCK_GRBMSE  = 0x12,
    SPM_GPU_BLOCK_RLC     = 0x13,
    SPM_GPU_BLOCK_DMA     = 0x14,
    SPM_GPU_BLOCK_MC      = 0x15,
    SPM_GPU_BLOCK_CPG     = 0x16,
    SPM_GPU_BLOCK_CPC     = 0x17,
    SPM_GPU_BLOCK_WD      = 0x18,
    SPM_GPU_BLOCK_TCS     = 0x19,
    SPM_GPU_BLOCK_ATC     = 0x1A,
    SPM_GPU_BLOCK_ATCL2   = 0x1B,
    SPM_GPU_BLOCK_MCVML2  = 0x1C,
    SPM_GPU_BLOCK_EA      = 0x1D,
    SPM_GPU_BLOCK_RPB     = 0x1E,
    SPM_GPU_BLOCK_RMI     = 0x1F,
    SPM_GPU_BLOCK_UMCCH   = 0x20,
    SPM_GPU_BLOCK_GE      = 0x21,
    SPM_GPU_BLOCK_GE1     = SPM_GPU_BLOCK_GE,
    SPM_GPU_BLOCK_GL1A    = 0x22,
    SPM_GPU_BLOCK_GL1C    = 0x23,
    SPM_GPU_BLOCK_GL1CG   = 0x24,
    SPM_GPU_BLOCK_GL2A    = 0x25,
    SPM_GPU_BLOCK_GL2C    = 0x26,
    SPM_GPU_BLOCK_CHA     = 0x27,
    SPM_GPU_BLOCK_CHC     = 0x28,
    SPM_GPU_BLOCK_CHCG    = 0x29,
    SPM_GPU_BLOCK_GUS     = 0x2A,
    SPM_GPU_BLOCK_GCR     = 0x2B,
    SPM_GPU_BLOCK_PH      = 0x2C,
    SPM_GPU_BLOCK_UTCL1   = 0x2D,
    SPM_GPU_BLOCK_GEDIST  = 0x2E,
    SPM_GPU_BLOCK_GESE    = 0x2F,
    SPM_GPU_BLOCK_DFMALL  = 0x30,
#if PAL_BUILD_GFX11
    SPM_GPU_BLOCK_SQWGP   = 0x31,
#endif
    SPM_GPU_BLOCK_COUNT
} SpmGpuBlock;

static_assert(SpmGpuBlock::SPM_GPU_BLOCK_COUNT >= static_cast<uint32_t>(Pal::GpuBlock::Count),
              "The SpmGpuBlock enumeration needs to be updated!");

typedef struct SpmCounterInfo
{
    SpmGpuBlock   block;
    uint32_t      instance;
    uint32_t      eventIndex;                   /*!<  Index of the perf counter event within the block.       */
    uint32_t      dataOffset;                   /*!<  Offset of counter data from the beginning of the chunk. */
    uint32_t      dataSize;                     /*!<  Size in bytes of a single counter data item.            */
} SpmCounterInfo;

typedef struct SqttFileSpmInfoFlags
{
    union
    {
        struct
        {
            int32_t  reserved : 32;                  /*!< Reserved, set to 0. */
        };

        uint32_t value;                              /*!< 32bit value containing all the above fields. */
    };
} SqttFileSpmInfoFlags;

typedef struct SqttFileChunkSpmDb
{
    SqttFileChunkHeader  header;
    SqttFileSpmInfoFlags flags;
    uint32_t             preambleSize;           /*!<  Size in bytes of SqttFileChunkSpmDb. */
    uint32_t             numTimestamps;          /*!<  Number of timestamps in this trace. */
    uint32_t             numSpmCounterInfo;      /*!<  Number of SpmCounterInfo. */
    uint32_t             spmCounterInfoSize;     /*!<  Size in bytes of a single SpmCounterInfo. */
    uint32_t             samplingInterval;       /*!<  The sampling interval */
} SqttFileChunkSpmDb;

static constexpr RgpChunkVersionNumbers SpmDbV1Version = {1, 3};
typedef struct SpmCounterInfoV1
{
    SpmGpuBlock   block;
    uint32_t      instance;
    uint32_t      dataOffset;                   /*!<  Offset of counter data from the beginning of the chunk. */
    uint32_t      eventIndex;                   /*!<  Index of the perf counter event within the block.       */
} SpmCounterInfoV1;

typedef struct SqttFileChunkSpmDbV1
{
    SqttFileChunkHeader  header;
    SqttFileSpmInfoFlags flags;
    uint32_t             numTimestamps;          /*!<  Number of timestamps in this trace. */
    uint32_t             numSpmCounterInfo;      /*!<  Number of SpmCounterInfo. */
    uint32_t             samplingInterval;       /*!<  The sampling interval */
} SqttFileChunkSpmDbV1;

typedef struct SqttFileDfSpmInfoFlags
{
    union
    {
        struct
        {
            int32_t  overflow     : 1;               /*!< */
            int32_t  gtscLimitHit : 1;               /*!< */
            int32_t  reserved     : 30;              /*!< Reserved, set to 0. */
        };

        uint32_t value;                              /*!< 32bit value containing all the above fields. */
    };
} SqttFileDfSpmInfoFlags;

typedef struct DfSpmCounterInfo
{
    SpmGpuBlock   block;
    uint32_t      eventQualifier;               /*!<  Unit mask that tells which operation to monitor e.g. reads and writes  */
    uint32_t      instance;                     /*!<  The instance of the component that is being monitored.  */
    uint32_t      eventIndex;                   /*!<  Index of the perf counter event within the block.       */
    uint32_t      dataValidOffset;              /*!<  Offset to the valid bit for this perf counter data.     */
    uint32_t      dataOffset;                   /*!<  Offset of counter data from the beginning of the chunk. */
    uint32_t      dataSize;                     /*!<  Size in bytes of a single counter data item.            */
} DfSpmCounterInfo;

typedef struct SqttFileChunkDfSpmDb
{
    SqttFileChunkHeader  header;
    SqttFileSpmInfoFlags flags;
    uint32_t             preambleSize;           /*!<  Size in bytes of SqttFileChunkSpmDb. */
    uint32_t             numTimestamps;          /*!<  Number of timestamps in this trace. */
    uint32_t             numDfSpmCounterInfo;    /*!<  Number of DfSpmCounterInfo. */
    uint32_t             dfSpmCounterInfoSize;   /*!<  Size in bytes of a single DfSpmCounterInfo. */
    uint32_t             samplingInterval;       /*!<  The sampling interval */
} SqttFileChunkDfSpmDb;

/** A structure encapsulating the state of the SQTT file parser.
 */
typedef struct SqttFileParser {
    SqttFileHeader              header;
    int32_t                     nextChunkOffset;
    const void*                 fileBuffer;
    int32_t                     fileBufferSize;
} SqttFileParser;

/** Create an SQTT file parser from a buffer.
 */
//SqttErrorCode sqttFileParserCreateFromBuffer(
    //SqttFileParser* fileParser,
    //const void* fileBuffer,
    //int32_t fileBufferSize);

/** Get a pointer to the next chunk in the file.
 */
//SqttErrorCode sqttFileParserParseNextChunk(
    //SqttFileParser* fileParser,
    //SqttFileChunkHeader** parsedChunk);

#ifdef __cplusplus
}
#endif
#endif /* #ifndef SQTT_FILE_FORMAT_H */
