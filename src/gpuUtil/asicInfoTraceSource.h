/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palPlatform.h"
#include "palDevice.h"
#include "palGpuUtil.h"
#include "palTraceSession.h"

namespace Pal
{
class Platform;
class Device;
}

const uint32_t TRACE_GPU_NAME_MAX_SIZE = 256;
const uint32_t TRACE_MAX_NUM_SE        = 32;
const uint32_t TRACE_SA_PER_SE         = 2;

// Struct for storing information about gpu clock speeds.
struct GpuClocksSample
{
    Pal::uint32 gpuEngineClockSpeed; // Current speed of the gpu engine clock in MHz
    Pal::uint32 gpuMemoryClockSpeed; // Current speed of the gpu memory clock in MHz
};

// Specifies the graphics IP level
struct TraceGfxIpLevel
{
    Pal::uint16 major;
    Pal::uint16 minor;
    Pal::uint16 stepping;
};

// GPU types
enum class TraceGpuType : Pal::uint32
{
    Unknown,
    Integrated,
    Discrete,
    Virtual
};

// Memory types, similar to Pal::LocalMemoryType
enum class TraceMemoryType : Pal::uint32
{
    Unknown,
    Ddr,
    Ddr2,
    Ddr3,
    Ddr4,
    Ddr5,
    Gddr3,
    Gddr4,
    Gddr5,
    Gddr6,
    Hbm,
    Hbm2,
    Hbm3,
    Lpddr4,
    Lpddr5
};

/// Asic Info struct, based off of SqttFileChunkAsicInfo. This is to be mapped to the RDF-based TraceChunkInfo
/// in TraceSession.
typedef struct TraceChunkAsicInfo
{
    uint64_t        shaderCoreClockFrequency;                  // Gpu core clock frequency in Hz
    uint64_t        memoryClockFrequency;                      // Memory clock frequency in Hz
    uint64_t        gpuTimestampFrequency;                     // Frequency of the gpu timestamp clock in Hz
    uint64_t        maxShaderCoreClock;                        // Maximum shader core clock frequency in Hz
    uint64_t        maxMemoryClock;                            // Maximum memory clock frequency in Hz
    int32_t         deviceId;
    int32_t         deviceRevisionId;
    int32_t         vgprsPerSimd;                              // Number of VGPRs per SIMD
    int32_t         sgprsPerSimd;                              // Number of SGPRs per SIMD
    int32_t         shaderEngines;                             // Number of shader engines
    int32_t         computeUnitPerShaderEngine;                // Number of compute units per shader engine
    int32_t         simdPerComputeUnit;                        // Number of SIMDs per compute unit
    int32_t         wavefrontsPerSimd;                         // Number of wavefronts per SIMD
    int32_t         minimumVgprAlloc;                          // Minimum number of VGPRs per wavefront
    int32_t         vgprAllocGranularity;                      // Allocation granularity of VGPRs
    int32_t         minimumSgprAlloc;                          // Minimum number of SGPRs per wavefront
    int32_t         sgprAllocGranularity;                      // Allocation granularity of SGPRs
    int32_t         hardwareContexts;                          // Number of hardware contexts
    TraceGpuType    gpuType;
    TraceGfxIpLevel gfxIpLevel;
    int32_t         gpuIndex;
    int32_t         ceRamSize;                                 // Max size in bytes of CE RAM space available
    int32_t         ceRamSizeGraphics;                         // Max CE RAM size available to graphics engine in bytes
    int32_t         ceRamSizeCompute;                          // Max CE RAM size available to Compute engine in bytes
    int32_t         maxNumberOfDedicatedCus;                   // Number of CUs dedicated to real time audio queue
    int64_t         vramSize;                                  // Total number of bytes to VRAM
    int32_t         vramBusWidth;                              // Width of the bus to VRAM
    int32_t         l2CacheSize;                               // Total number of bytes in L2 Cache
    int32_t         l1CacheSize;                               // Total number of L1 cache bytes per CU
    int32_t         ldsSize;                                   // Total number of LDS bytes per CU
    char            gpuName[TRACE_GPU_NAME_MAX_SIZE];          // Name of the GPU, padded to 256 bytes
    float           aluPerClock;                               // Number of ALUs per clock
    float           texturePerClock;                           // Number of texture per clock
    float           primsPerClock;                             // Number of primitives per clock
    float           pixelsPerClock;                            // Number of pixels per clock
    uint32_t        memoryOpsPerClock;                         // Number of memory operations per memory clock cycle
    TraceMemoryType memoryChipType;                            // Type of memory chip used by the ASIC
    uint32_t        ldsGranularity;                            // LDS allocation granularity expressed in bytes
    uint16_t        cuMask[TRACE_MAX_NUM_SE][TRACE_SA_PER_SE]; // Mask of present, non-harvested CUs (physical layout)
} TraceChunkAsicInfo;

namespace GpuUtil
{

constexpr char AsicInfoTraceSourceName[] = "asicinfo";
constexpr Pal::uint32 AsicInfoTraceSourceVersion = 1;

const char chunkTextIdentifier[GpuUtil::TextIdentifierSize] = "AsicInfo";

// =====================================================================================================================
// A trace source that sends ASIC information to the trace session. This is one of the "default" trace sources that are
// registered with the current PAL-owned trace session on start-up.
class AsicInfoTraceSource : public ITraceSource
{
public:
    AsicInfoTraceSource(Pal::Platform* pPlatform);
    virtual ~AsicInfoTraceSource();

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 712
    virtual void OnConfigUpdated(DevDriver::StructuredValue* pJsonConfig) override {}
#else
    virtual void OnConfigUpdated(const char* pJsonConfig) override {}
#endif

    virtual Pal::uint64 QueryGpuWorkMask() const override { return 0; }

    virtual void OnTraceAccepted() override {}
    virtual void OnTraceBegin(Pal::uint32 gpuIndex, Pal::ICmdBuffer* pCmdBuf) override {}
    virtual void OnTraceEnd(Pal::uint32 gpuIndex, Pal::ICmdBuffer* pCmdBuf) override {}
    virtual void OnTraceFinished() override;

    virtual const char* GetName() const override { return AsicInfoTraceSourceName; }

    virtual Pal::uint32 GetVersion() const override { return AsicInfoTraceSourceVersion; }

    // Helper function to fill in the TraceChunkAsicInfo struct based on the DeviceProperties and
    // PerfExperimentProperties provided.
    Pal::Result FillTraceChunkAsicInfo(const Pal::DeviceProperties& properties,
                                       const Pal::PerfExperimentProperties& perfExpProps,
                                       const GpuClocksSample& gpuClocks,
                                       TraceChunkAsicInfo* pAsicInfo);

    // Queries the engine and memory clocks from DeviceProperties
    Pal::Result SampleGpuClocks(GpuClocksSample* pGpuClocksSample,
                                Pal::Device* pDevice,
                                Pal::DeviceProperties deviceProps) const;

    // Translate TraceChunkAsicInfo to TraceChunkInfo and write it into TraceSession
    void WriteAsicInfoTraceChunk();

private:
    Pal::Platform* const m_pPlatform; // Platform associated with this TraceSource
};

}
