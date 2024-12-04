/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

namespace GpuUtil
{

namespace TraceChunk
{

using Pal::int32;
using Pal::int64;
using Pal::uint16;
using Pal::uint32;
using Pal::uint64;

/// Struct for storing information about GPU clock speeds.
struct GpuClocksSample
{
    uint32 gpuEngineClockSpeed; // Current speed of the GPU engine clock in MHz
    uint32 gpuMemoryClockSpeed; // Current speed of the GPU memory clock in MHz
};

/// Specifies the graphics IP level
struct TraceGfxIpLevel
{
    uint16 major;
    uint16 minor;
    uint16 stepping;
};

/// GPU types
enum class TraceGpuType : uint32
{
    Unknown,
    Integrated,
    Discrete,
    Virtual
};

// Memory types, similar to Pal::LocalMemoryType
enum class TraceMemoryType : uint32
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

constexpr uint32 TraceGpuNameMaxSize = 256;
constexpr uint32 TraceMaxNumSe       = 32;
constexpr uint32 TraceSaPerSe        = 2;
constexpr uint32 BitsPerShaderEngine = 4;

const     char   AsicInfoChunkId[TextIdentifierSize] = "AsicInfo";
constexpr uint32 AsicInfoChunkVersion                = 3;

/// Asic Info struct, based off of SqttFileChunkAsicInfo.
struct AsicInfo
{
    uint32          pciId;                                     // The ID of the GPU queried
    uint64          shaderCoreClockFrequency;                  // Gpu core clock frequency in Hz
    uint64          memoryClockFrequency;                      // Memory clock frequency in Hz
    uint64          gpuTimestampFrequency;                     // Frequency of the gpu timestamp clock in Hz
    uint64          maxShaderCoreClock;                        // Maximum shader core clock frequency in Hz
    uint64          maxMemoryClock;                            // Maximum memory clock frequency in Hz
    int32           deviceId;                                  // PCIE device ID
    int32           deviceRevisionId;                          // PCIE revision ID
    int32           vgprsPerSimd;                              // Number of VGPRs per SIMD
    int32           sgprsPerSimd;                              // Number of SGPRs per SIMD
    int32           shaderEngines;                             // Number of shader engines
    int32           computeUnitPerShaderEngine;                // Number of compute units per shader engine
    int32           simdPerComputeUnit;                        // Number of SIMDs per compute unit
    int32           wavefrontsPerSimd;                         // Number of wavefronts per SIMD
    int32           minimumVgprAlloc;                          // Minimum number of VGPRs per wavefront
    int32           vgprAllocGranularity;                      // Allocation granularity of VGPRs
    int32           minimumSgprAlloc;                          // Minimum number of SGPRs per wavefront
    int32           sgprAllocGranularity;                      // Allocation granularity of SGPRs
    int32           hardwareContexts;                          // Number of hardware contexts
    TraceGpuType    gpuType;
    TraceGfxIpLevel gfxIpLevel;
    int32           gpuIndex;
    int32           ceRamSize;                                 // Max size in bytes of CE RAM space available
    int32           ceRamSizeGraphics;                         // Max CE RAM size available to graphics engine in bytes
    int32           ceRamSizeCompute;                          // Max CE RAM size available to Compute engine in bytes
    int32           maxNumberOfDedicatedCus;                   // Number of CUs dedicated to real time audio queue
    int64           vramSize;                                  // Total number of bytes to VRAM
    int32           vramBusWidth;                              // Width of the bus to VRAM
    int32           l2CacheSize;                               // Total number of bytes in L2 Cache
                                                               // (TCC on GCN hardware, GL2C on RDNA hardware)
    int32           l1CacheSize;                               // Total number of L1 cache bytes per CU (TCP)
                                                               // (this is L0 on RDNA hardware)
    int32           ldsSize;                                   // Total number of LDS bytes per CU
    char            gpuName[TraceGpuNameMaxSize];              // Name of the GPU, padded to 256 bytes
    float           aluPerClock;                               // Number of ALUs per clock
    float           texturePerClock;                           // Number of texture per clock
    float           primsPerClock;                             // Number of primitives per clock
    float           pixelsPerClock;                            // Number of pixels per clock
    uint32          memoryOpsPerClock;                         // Number of memory operations per memory clock cycle
    TraceMemoryType memoryChipType;                            // Type of memory chip used by the ASIC
    uint32          ldsGranularity;                            // LDS allocation granularity expressed in bytes
    uint16          cuMask[TraceMaxNumSe][TraceSaPerSe];       // Mask of present, non-harvested CUs (physical layout)
    uint32          pixelPackerMask[BitsPerShaderEngine];      // Mask of present, non-harvested pixel packers --
                                                               // 4 bits per shader engine (max of 32 shader engines)
    uint32          gl1CacheSize;                              // Total number of GL1 cache bytes per shader array
    uint32          instCacheSize;                             // Total number of Instruction cache bytes per CU
    uint32          scalarCacheSize;                           // Total number of Scalar cache (K$) bytes per CU
    uint32          mallCacheSize;                             // Total number of MALL cache (Infinity cache) bytes
};

} // namespace TraceChunk

constexpr char        AsicInfoTraceSourceName[]  = "asicinfo";
constexpr Pal::uint32 AsicInfoTraceSourceVersion = 3;

// =====================================================================================================================
// A trace source that sends ASIC information to the trace session. This is one of the "default" trace sources that are
// registered with the current PAL-owned trace session on start-up.
class AsicInfoTraceSource : public ITraceSource
{
public:
    AsicInfoTraceSource(Pal::Platform* pPlatform);
    virtual ~AsicInfoTraceSource();

    virtual void OnConfigUpdated(DevDriver::StructuredValue* pJsonConfig) override {}

    virtual Pal::uint64 QueryGpuWorkMask() const override { return 0; }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 908
    virtual void OnTraceAccepted(Pal::uint32 gpuIndex, Pal::ICmdBuffer* pCmdBuf) override {}
#else
    virtual void OnTraceAccepted() override {}
#endif
    virtual void OnTraceBegin(Pal::uint32 gpuIndex, Pal::ICmdBuffer* pCmdBuf) override {}
    virtual void OnTraceEnd(Pal::uint32 gpuIndex, Pal::ICmdBuffer* pCmdBuf) override {}
    virtual void OnTraceFinished() override;

    virtual const char* GetName() const override { return AsicInfoTraceSourceName; }

    virtual Pal::uint32 GetVersion() const override { return AsicInfoTraceSourceVersion; }

private:
    // Helper function to fill in the TraceChunk::AsicInfo struct based on the DeviceProperties and
    // PerfExperimentProperties provided.
    void FillTraceChunkAsicInfo(const Pal::DeviceProperties&         properties,
                                const Pal::PerfExperimentProperties& perfExpProps,
                                const TraceChunk::GpuClocksSample&   gpuClocks,
                                TraceChunk::AsicInfo*                pAsicInfo);

    // Queries the engine and memory clocks from DeviceProperties
    Pal::Result SampleGpuClocks(TraceChunk::GpuClocksSample* pGpuClocksSample,
                                Pal::Device*                 pDevice,
                                const Pal::DeviceProperties& deviceProps) const;

    Pal::Platform* const m_pPlatform;
};

} // namespace GpuUtil
