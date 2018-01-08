/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

// Copyright (c) AMD Corporation. All rights reserved.

#ifndef SQTT_FILE_FORMAT_H
#define SQTT_FILE_FORMAT_H

#include <stdint.h>

/** Magic number for all SQTT files.
 */
#define SQTT_FILE_MAGIC_NUMBER            0x50303042

#ifdef __cplusplus
extern "C" {
#endif

/** Structure encapsulating the file header of an SQTT file.
 */
typedef struct SqttFileHeader
{
    uint32_t                magicNumber;                    /*!< Magic number, always set to <c><i>SQTT_FILE_MAGIC_NUMBER</i></c>. */
    uint32_t                versionMajor;                   /*!< The major version number of the file. */
    uint32_t                versionMinor;                   /*!< The minor version number of the file. */
    uint32_t                flags;                          /*!< Bitfield of flags set with information about the file. */
    int32_t                 chunkOffset;                    /*!< The offset in bytes to the first chunk contained in the file. */
    int32_t                 second;                         /*!< The second in the minute that the RGP file was created. */
    int32_t                 minute;                         /*!< The minute in the hour that the RGP file was created. */
    int32_t                 hour;                           /*!< The hour in the day that the RGP file was created. */
    int32_t                 dayInMonth;                     /*!< The day in the month that the RGP file was created. */
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
    SQTT_FILE_CHUNK_TYPE_ASIC_INFO,                         /*!< A chunk containing the description of the ASIC on which the trace was made. */
    SQTT_FILE_CHUNK_TYPE_SQTT_DESC,                         /*!< A chunk containing the description of the SQTT data. */
    SQTT_FILE_CHUNK_TYPE_SQTT_DATA,                         /*!< A chunk containing the SQTT data for a single shader engine. */
    SQTT_FILE_CHUNK_TYPE_API_INFO,                          /*!< A chunk containing the description of the API on which the trace was made. */
    SQTT_FILE_CHUNK_TYPE_ISA_DATABASE,                      /*!< A chunk containing the Shader ISA code. */
    SQTT_FILE_CHUNK_TYPE_QUEUE_EVENT_TIMINGS,               /*!< A chunk containing the timings for queue events that occurred during the trace. */
    SQTT_FILE_CHUNK_TYPE_CLOCK_CALIBRATION,                 /*!< A chunk containing the information required to correlate between clock domains. */
    SQTT_FILE_CHUNK_TYPE_CPU_INFO,                          /*!< A chunk containing the description of the CPU on which the trace was made. */
    SQTT_FILE_CHUNK_TYPE_SPM_DB,                            /*!< A chunk containing the SPM trace data. */

    SQTT_FILE_CHUNK_TYPE_COUNT
} SqttFileChunkType;

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
    SQTT_API_TYPE_DIRECTX_12,
    SQTT_API_TYPE_VULKAN
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
    int32_t                     version;                    /*!< The version of the chunk. */
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
    SQTT_GFXIP_LEVEL_NONE      = 0x0,
    SQTT_GFXIP_LEVEL_GFXIP_6   = 0x1,
    SQTT_GFXIP_LEVEL_GFXIP_7   = 0x2,
    SQTT_GFXIP_LEVEL_GFXIP_8   = 0x3,
    SQTT_GFXIP_LEVEL_GFXIP_8_1 = 0x4,
    SQTT_GFXIP_LEVEL_GFXIP_9   = 0x5
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
    SQTT_MEMORY_TYPE_GDDR3   = 0x10,
    SQTT_MEMORY_TYPE_GDDR4   = 0x11,
    SQTT_MEMORY_TYPE_GDDR5   = 0x12,
    SQTT_MEMORY_TYPE_GDDR6   = 0x13,
    SQTT_MEMORY_TYPE_HBM     = 0x20,
    SQTT_MEMORY_TYPE_HBM2    = 0x21,
    SQTT_MEMORY_TYPE_HBM3    = 0x22,
} SqttMemoryType;

const uint32_t SQTT_GPU_NAME_MAX_SIZE = 256;

/** A structure encapsulating information about the ASIC on which the trace was performed.
 */
typedef struct SqttFileChunkAsicInfo
{
    SqttFileChunkHeader         header;                          /*!< Common header for all chunks. */
    uint64_t                    flags;                           /*!< Flags for the ASIC info chunk. */
    uint64_t                    traceShaderCoreClock;            /*!< The shader core clock frequency during SQTT traces. */
    uint64_t                    traceMemoryClock;                /*!< The memory clock frequency during SQTT traces. */
    int32_t                     deviceId;                        /*!< The device ID for the card where the trace was performed. */
    int32_t                     deviceRevisionId;                /*!< The device revision ID for the card where the trace was performed. */
    int32_t                     vgprsPerSimd;                    /*!< The number of VGPRs per SIMD. */
    int32_t                     sgprsPerSimd;                    /*!< The number of SGPRs per SIMD. */
    int32_t                     shaderEngines;                   /*!< The number of shader engines. */
    int32_t                     computeUnitPerShaderEngine;      /*!< The number of compute units per shader engine. */
    int32_t                     simdPerComputeUnit;              /*!< The number of SIMDs per compute unit. */
    int32_t                     wavefrontsPerSimd;               /*!< The number of wavefronts per SIMD. */
    int32_t                     minimumVgprAlloc;                /*!< Minimum number of VGPRs per wavefront. */
    int32_t                     vgprAllocGranularity;            /*!< The allocation granularity of VGPRs. */
    int32_t                     minimumSgprAlloc;                /*!< Minimum number of SGPRs per wavefront. */
    int32_t                     sgprAllocGranularity;            /*!< The allocation granularity of SGPRs. */
    int32_t                     hardwareContexts;                /*!< The number of hardware contexts. */
    SqttGpuType                 gpuType;                         /*!< The type of GPU. */
    SqttGfxIpLevel              gfxIpLevel;                      /*!< The gfxip level of this GPU. */
    int32_t                     gpuIndex;                        /*!< The index of this GPU. */
    int32_t                     gdsSize;                         /*!< The size in bytes of the global data store in bytes in GPU. */
    int32_t                     gdsPerShaderEngine;              /*!< The max size in bytes of GDS space available per SE. */
    int32_t                     ceRamSize;                       /*!< The max size in bytes of CE RAM space available. */
    int32_t                     ceRamSizeGraphics;               /*!< The max CE RAM size available to graphics engine in bytes.*/
    int32_t                     ceRamSizeCompute;                /*!< The max CE RAM size available to Compute engine in bytes.*/
    int32_t                     maxNumberOfDedicatedCus;         /*!< The number of CUs dedicated to real time audio queue. */
    int64_t                     vramSize;                        /*!< The total number of bytes to VRAM. */
    int32_t                     vramBusWidth;                    /*!< The width of the bus to VRAM. */
    int32_t                     l2CacheSize;                     /*!< The total number of bytes in L2 Cache. */
    int32_t                     l1CacheSize;                     /*!< The total number of L1 cache bytes per CU.. */
    int32_t                     ldsSize;                         /*!< The total number of LDS bytes per CU.. */
    char                        gpuName[SQTT_GPU_NAME_MAX_SIZE]; /*!< The name of the GPU. Padded to 256 bytes. */
    float                       aluPerClock;                     /*!< The number of ALUs per clock. */
    float                       texturePerClock;                 /*!< The number of texture per clock. */
    float                       primsPerClock;                   /*!< The number of primitives per clock. */
    float                       pixelsPerClock;                  /*!< The number of pixels per clock. */
    uint64_t                    gpuTimestampFrequency;           /*!< The frequency of the gpu timestamp clock in Hz. */
    uint64_t                    maxShaderCoreClock;              /*!< The max shader core clock frequency. */
    uint64_t                    maxMemoryClock;                  /*!< The max memory clock frequency. */
    uint32_t                    memoryOpsPerClock;               /*!< The number of memory operations per memory clock cycle. */
    SqttMemoryType              memoryChipType;                  /*!< The type of memory chip used by the asic. */
} SqttFileChunkAsicInfo;

/** A structure encapsulating information about the API on which the trace was performed.
 */
typedef struct SqttFileChunkApiInfo
{
    SqttFileChunkHeader         header;                     /*!< Common header for all chunks. */
    SqttApiType                 apiType;                    /*!< The type of API used. */
    uint16_t                    versionMajor;               /*!< The major API version. */
    uint16_t                    versionMinor;               /*!< The minor API version. */
} SqttFileChunkApiInfo;

/** An enumeration of the SQTT versions.
 */
typedef enum SqttVersion
{
    SQTT_VERSION_NONE = 0x0,                                /*!< Not supported. */
    SQTT_VERSION_1_0  = 0x1,                                /*!< Evergreen (8xx). */
    SQTT_VERSION_1_1  = 0x2,                                /*!< Northern Islands (9xx). */
    SQTT_VERSION_2_0  = 0x3,                                /*!< Southern Islands (GfxIp 6). */
    SQTT_VERSION_2_1  = 0x4,                                /*!< Sea Islands (GfxIp 7). */
    SQTT_VERSION_2_2  = 0x5,                                /*!< Volcanic Islands (GfxIP 8). */
    SQTT_VERSION_2_3  = 0x6                                 /*!< GfxIp 9. */

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
            int32_t                 instrumentationVersion;     /*!< The major version number of the instrumentation specification
                                                                that the matching SQTT_DATA chunk contains*/
        } v0;

        struct
        {
            int16_t                 instrumentationSpecVersion; /*!< The version of the instrumentation as defined by the spec. */
            int16_t                 instrumentationApiVersion;  /*!< The version of the instrumentation as defined by the api. */
            int32_t                 computeUnitIndex;           /*!< The physical index of the compute unit that executed the trace. */
        } v1;
    };
} SqttFileChunkSqttDesc;

/** A structure encapsulating information about the location of the SQTT data within the SQTT file itself.
 */
typedef struct SqttFileChunkSqttData
{
    SqttFileChunkHeader         header;                     /*!< Common header for all chunks. */
    int32_t                     offset;                     /*!< Offset from the start of the file (in bytes) to the SQTT data. */
    int32_t                     size;                       /*!< The size (in bytes) of the SQTT data. */
} SqttFileChunkSqttData;

/** An enumeration of the hardware shader stage that the shader will run on. Bitfield of shader stages.
*/
typedef enum SqttShaderType
{
    SQTT_SHADER_TYPE_PS       = 0x00000001,                 /*!< Pixel Shader stage. */
    SQTT_SHADER_TYPE_VS       = 0x00000002,                 /*!< Vertex Shader stage. */
    SQTT_SHADER_TYPE_GS       = 0x00000004,                 /*!< Geometry Shader stage. */
    SQTT_SHADER_TYPE_ES       = 0x00000008,                 /*!< Export Shader stage. */
    SQTT_SHADER_TYPE_HS       = 0x00000010,                 /*!< Hull shader stage. */
    SQTT_SHADER_TYPE_LS       = 0x00000020,                 /*!< Local shader stage. */
    SQTT_SHADER_TYPE_CS       = 0x00000040,                 /*!< Compute Shader. */
    SQTT_SHADER_TYPE_RESERVED = 0x00000080                  /*!< Reserved. */
} SqttShaderType;

/** An enumeration of the shader operations.
*/
typedef enum SqttShaderFlags
{
    SQTT_SHADER_WRITES_UAV         = 0x1,
    SQTT_SHADER_WRITES_DEPTH       = 0x2,
    SQTT_SHADER_STREAM_OUT_ENABLED = 0x4
} SqttShaderFlags;

/**  A structure encapsulating a 128-bit shader hash
*/
typedef struct SqttShaderHash
{
    uint64_t lower;   ///< Lower 64-bits of hash
    uint64_t upper;   ///< Upper 64-bits of hash
} ShaderHash;

/**  A structure encapsulating information about each ISA blob in each record of the shader ISA database.
 */
typedef struct SqttShaderIsaBlobHeader
{
    uint32_t        sizeInBytes;                            /*!< The size of the ISA chunk (in bytes). */
    uint32_t        actualVgprCount;                        /*!< The actual number of VGPRs used by the shader. */
    uint32_t        actualSgprCount;                        /*!< The actual number of SGPRs used by the shader. */
    uint32_t        actualLdsCount;                         /*!< The actual number of LDS bytes used by the shader. */
    SqttShaderHash  apiShaderHash;                          /*!< The 128-bit hash of the shader, API specific. */
    SqttShaderHash  palShaderHash;                          /*!< The 128-bit PAL internal hash of the shader used in the shader cache. */
    uint32_t        scratchSize;                            /*!< The number of DWORDs used for scratch memory. */
    uint32_t        flags;                                  /*!< Flags as defined in SqttShaderFlags. */
    uint64_t        baseAddress;                            /*!< The base address of the first instruction in the chunk. */
} SqttShaderIsaBlobHeader;

/**  A structure encapsulating information about each record in the shader ISA database.
 */
typedef struct SqttIsaDatabaseRecord
{
    struct
    {
        uint32_t shaderStage : 8;                            /*!< Shader stage mask as defined in SqttShaderType. */
        uint32_t reserved    : 24;                           /*!< Reserved for future use. */
    };
    uint32_t         recordSize;                             /*!< The size of the record (including all blobs) in bytes. */
} SqttIsaDbRecord;

/**  A structure encapsulating information about the shader ISA database.
 */
typedef struct SqttFileChunkIsaDatabase
{
    SqttFileChunkHeader header;
    uint32_t            offset;
    uint32_t            size;
    uint32_t            recordCount;
} SqttFileChunkIsaDatabase;

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
    SQTT_QUEUE_TYPE_UNKNOWN   = 0x0,
    SQTT_QUEUE_TYPE_UNIVERSAL = 0x1,
    SQTT_QUEUE_TYPE_COMPUTE   = 0x2,
    SQTT_QUEUE_TYPE_DMA       = 0x3
} SqttQueueType;

/** An enumeration of all valid engine types.
*/
typedef enum SqttEngineType
{
    SQTT_ENGINE_TYPE_UNKNOWN           = 0x0,
    SQTT_ENGINE_TYPE_UNIVERSAL         = 0x1,
    SQTT_ENGINE_TYPE_COMPUTE           = 0x2,
    SQTT_ENGINE_TYPE_EXCLUSIVE_COMPUTE = 0x3,
    SQTT_ENGINE_TYPE_DMA               = 0x4
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
    uint32_t            clockSpeed;            /*!< The maximum clock speed of the cpu in Hz. */
    uint32_t            numLogicalCores;       /*!< The number of threads that can run simultaneously on the cpu. */
    uint32_t            numPhysicalCores;      /*!< The number of physical cores in the cpu. */
    uint32_t            systemRamSize;         /*!< The size of system RAM in MB. */
} SqttFileChunkCpuInfo;

typedef enum SpmGpuBlock
{
    CPF = 0,
    IA,
    VGT,
    PA,
    SC,
    SPI,
    SQ,
    SX,
    TA,
    TD,
    TCP,
    TCC,
    TCA,
    DB,
    CB,
    GDS,
    SRBM,
    GRBM,
    GRBMSE,
    RLC,
    DMA,
    MC,
    CPG,
    CPC,
    WD,
    TCS,
    ATC,
    ATCL2,
    MCVML2,
    EA,
    RPB,
    RMI,

/*Gfx10 blocks*/
    GE,
    GL1A,
    GL1C,
    GL1CG,
    GL2A,           // TCA is used in Gfx9, and changed to GL2A in Gfx10
    GL2C,           // TCC is used in Gfx9, and changed to GL2C in Gfx10
    COUNT
} SpmGpuBlock;

typedef struct SpmCounterInfo
{
    SpmGpuBlock   block;
    uint32_t      instance;
    uint32_t      dataOffset;                   /*!<  Offset of counter data from the beginning of the chunk. */
} SpmCounterInfo;

typedef struct SqttFileChunkSpmDb
{
    SqttFileChunkHeader header;
    uint32_t            numTimestamps;          /*!<  Number of timestamps in this trace. */
    uint32_t            numSpmCounterInfo;      /*!<  Number of SpmCounterInfo. */
} SqttFileChunkSpmDb;

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
