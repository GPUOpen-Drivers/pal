/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palAutoBuffer.h"
#include "core/platform.h"
#include "core/device.h"
#include "asicInfoTraceSource.h"

using namespace Pal;

namespace GpuUtil
{

// Translation table for obtaining TraceMemoryType given a Pal::LocalMemoryType.
static constexpr TraceMemoryType TraceMemoryTypeTable[] =
{
    TraceMemoryType::Unknown,
    TraceMemoryType::Ddr2,
    TraceMemoryType::Ddr3,
    TraceMemoryType::Ddr4,
    TraceMemoryType::Gddr5,
    TraceMemoryType::Gddr6,
    TraceMemoryType::Hbm,
    TraceMemoryType::Hbm2,
    TraceMemoryType::Hbm3,
    TraceMemoryType::Lpddr4,
    TraceMemoryType::Lpddr5,
    TraceMemoryType::Ddr5
};

static_assert(Util::ArrayLen(TraceMemoryTypeTable) == static_cast<uint32>(Pal::LocalMemoryType::Count),
    "The number of LocalMemoryTypes have changed. Please update TraceMemoryTypeTable.");

// =====================================================================================================================
// Populate TraceGfxIpLevel from Pal::GfxIpLevel. Note: All stepping values are set to zero here. They will be correctly
// reassigned later from DeviceProperties::gfxStepping.
void PopulateTraceGfxIpLevel(
    const GfxIpLevel& gfxIpLevel,
    TraceGfxIpLevel*  pTraceGfxIpLevel)
{
    switch (gfxIpLevel)
    {
#if PAL_BUILD_GFX
    case GfxIpLevel::GfxIp6:
        *pTraceGfxIpLevel = { 6, 0, 0 };
        break;
    case GfxIpLevel::GfxIp7:
        *pTraceGfxIpLevel = { 7, 0, 0 };
        break;
    case GfxIpLevel::GfxIp8:
        *pTraceGfxIpLevel = { 8, 0, 0 };
        break;
    case GfxIpLevel::GfxIp8_1:
        *pTraceGfxIpLevel = { 8, 1, 0 };
        break;
    case GfxIpLevel::GfxIp10_1:
        *pTraceGfxIpLevel = { 10, 1, 0 };
        break;
    case GfxIpLevel::GfxIp9:
        *pTraceGfxIpLevel = { 9, 0, 0 };
        break;
    case GfxIpLevel::GfxIp10_3:
        *pTraceGfxIpLevel = { 10, 3, 0 };
        break;
#if PAL_BUILD_GFX11
    case GfxIpLevel::GfxIp11_0:
        *pTraceGfxIpLevel = { 11, 0, 0 };
        break;
#endif
#endif
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }
}

// =====================================================================================================================
AsicInfoTraceSource::AsicInfoTraceSource(
    Platform* pPlatform)
    :
    m_pPlatform(pPlatform)
{
}

// =====================================================================================================================
AsicInfoTraceSource::~AsicInfoTraceSource()
{
}

// =====================================================================================================================
// Queries the engine and memory clocks from DeviceProperties
Result AsicInfoTraceSource::SampleGpuClocks(
    GpuClocksSample* pGpuClocksSample,
    Device*          pDevice,
    DeviceProperties deviceProps) const
{
    Result result = Result::ErrorInvalidPointer;

    if (pGpuClocksSample != nullptr)
    {
        result = Result::Success;
    }

    SetClockModeInput clockModeInput = {};

    // Query the the current device clock ratios.
    // Note: During profiling, we prefer the asic to run at fixed speeds to obtain stable and repeatable results.
    clockModeInput.clockMode = DeviceClockMode::Query;

    SetClockModeOutput clockModeOutput = {};

    if (result == Result::Success)
    {
        result = pDevice->SetClockMode(clockModeInput, &clockModeOutput);
    }

    if (result == Result::Success)
    {
        const float maxEngineClock = deviceProps.gfxipProperties.performance.maxGpuClock;
        const float maxMemoryClock = deviceProps.gpuMemoryProperties.performance.maxMemClock;

        const uint32 engineClock = clockModeOutput.engineClockFrequency;
        const uint32 memoryClock = clockModeOutput.memoryClockFrequency;

        pGpuClocksSample->gpuEngineClockSpeed = engineClock;
        pGpuClocksSample->gpuMemoryClockSpeed = memoryClock;
    }

    return result;
}

// =====================================================================================================================
Result AsicInfoTraceSource::FillTraceChunkAsicInfo(
    const Pal::DeviceProperties&         properties,
    const Pal::PerfExperimentProperties& perfExpProps,
    const GpuClocksSample&               gpuClocks,
    TraceChunkAsicInfo*                  pAsicInfo)
{
    Result result = (pAsicInfo == nullptr) ? Result::ErrorInvalidPointer : Result::Success;

    pAsicInfo->shaderCoreClockFrequency   = gpuClocks.gpuEngineClockSpeed * 1000000;
    pAsicInfo->memoryClockFrequency       = gpuClocks.gpuMemoryClockSpeed * 1000000;

    pAsicInfo->deviceId                   = properties.deviceId;
    pAsicInfo->deviceRevisionId           = properties.revisionId;
    pAsicInfo->vgprsPerSimd               = properties.gfxipProperties.shaderCore.vgprsPerSimd;
    pAsicInfo->sgprsPerSimd               = properties.gfxipProperties.shaderCore.sgprsPerSimd;
    pAsicInfo->shaderEngines              = properties.gfxipProperties.shaderCore.numShaderEngines;
    uint32 computeUnitPerShaderEngine     = 0;
    for (uint32 seIndex = 0; seIndex < properties.gfxipProperties.shaderCore.numShaderEngines; seIndex++)
    {
        uint32 totalActiveCu = 0;
        for (uint32 saIndex = 0; saIndex < properties.gfxipProperties.shaderCore.numShaderArrays; saIndex++)
        {
            const uint32 activeCuMask   = properties.gfxipProperties.shaderCore.activeCuMask[seIndex][saIndex];
            totalActiveCu += Util::CountSetBits(activeCuMask);
        }
        // If there are no Active CU's then we assume this engine is disabled(harvested), and ignore the zero count
        if (totalActiveCu != 0)
        {
            computeUnitPerShaderEngine = totalActiveCu;
        }
    }
    pAsicInfo->computeUnitPerShaderEngine = computeUnitPerShaderEngine;
    pAsicInfo->simdPerComputeUnit         = properties.gfxipProperties.shaderCore.numSimdsPerCu;
    pAsicInfo->wavefrontsPerSimd          = properties.gfxipProperties.shaderCore.numWavefrontsPerSimd;
    pAsicInfo->minimumVgprAlloc           = properties.gfxipProperties.shaderCore.minVgprAlloc;
    pAsicInfo->vgprAllocGranularity       = properties.gfxipProperties.shaderCore.vgprAllocGranularity;
    pAsicInfo->minimumSgprAlloc           = properties.gfxipProperties.shaderCore.minSgprAlloc;
    pAsicInfo->sgprAllocGranularity       = properties.gfxipProperties.shaderCore.sgprAllocGranularity;
    pAsicInfo->hardwareContexts           = properties.gfxipProperties.hardwareContexts;
    pAsicInfo->gpuType                    = static_cast<TraceGpuType>(properties.gpuType);

    PopulateTraceGfxIpLevel(properties.gfxLevel, &(pAsicInfo->gfxIpLevel));
    pAsicInfo->gfxIpLevel.stepping        = properties.gfxStepping;

    pAsicInfo->gpuIndex                   = properties.gpuIndex;
    pAsicInfo->ceRamSize                  = properties.gfxipProperties.ceRamSize;

    pAsicInfo->maxNumberOfDedicatedCus    = properties.engineProperties[EngineTypeUniversal].maxNumDedicatedCu;
    pAsicInfo->ceRamSizeGraphics          = properties.engineProperties[EngineTypeUniversal].ceRamSizeAvailable;
    pAsicInfo->ceRamSizeCompute           = properties.engineProperties[EngineTypeCompute].ceRamSizeAvailable;

    pAsicInfo->vramBusWidth               = properties.gpuMemoryProperties.performance.vramBusBitWidth;
    pAsicInfo->vramSize                   = properties.gpuMemoryProperties.maxLocalMemSize;
    pAsicInfo->l2CacheSize                = properties.gfxipProperties.shaderCore.tccSizeInBytes;
    pAsicInfo->l1CacheSize                = properties.gfxipProperties.shaderCore.tcpSizeInBytes;
    pAsicInfo->ldsSize                    = properties.gfxipProperties.shaderCore.ldsSizePerCu;

    memcpy(pAsicInfo->gpuName, &properties.gpuName, TRACE_GPU_NAME_MAX_SIZE);

    pAsicInfo->aluPerClock                = properties.gfxipProperties.performance.aluPerClock;
    pAsicInfo->texturePerClock            = properties.gfxipProperties.performance.texPerClock;
    pAsicInfo->primsPerClock              = properties.gfxipProperties.performance.primsPerClock;
    pAsicInfo->pixelsPerClock             = properties.gfxipProperties.performance.pixelsPerClock;

    pAsicInfo->gpuTimestampFrequency      = properties.timestampFrequency;

    pAsicInfo->maxShaderCoreClock =
        static_cast<uint64>(properties.gfxipProperties.performance.maxGpuClock * 1000000.0f);
    pAsicInfo->maxMemoryClock =
        static_cast<uint64>(properties.gpuMemoryProperties.performance.maxMemClock * 1000000.0f);

    pAsicInfo->memoryOpsPerClock = properties.gpuMemoryProperties.performance.memOpsPerClock;

    pAsicInfo->memoryChipType =
        TraceMemoryTypeTable[static_cast<uint32>(properties.gpuMemoryProperties.localMemoryType)];

    pAsicInfo->ldsGranularity = properties.gfxipProperties.shaderCore.ldsGranularity;

    for (uint32 se = 0; se < MaxShaderEngines; se++)
    {
        for (uint32 sa = 0; sa < MaxShaderArraysPerSe; sa++)
        {
            pAsicInfo->cuMask[se][sa] = static_cast<uint16>(properties.gfxipProperties.shaderCore.activeCuMask[se][sa]);

            // If this triggers we need to update the RGP spec to use at least 32 bits per SA.
            PAL_ASSERT(Util::TestAnyFlagSet(
                        properties.gfxipProperties.shaderCore.activeCuMask[se][sa], 0xffff0000) == false);
        }
    }

    return result;
}

// =====================================================================================================================
// Translate TraceChunkAsicInfo to TraceChunkInfo and write it into TraceSession
void AsicInfoTraceSource::WriteAsicInfoTraceChunk()
{
    Result result = Result::Success;
    uint32 deviceCount = m_pPlatform->GetDeviceCount();

    for (uint32 i = 0 ; (i < deviceCount) && (result == Result::Success) ; i++)
    {
        Device* pDevice = m_pPlatform->GetDevice(i);

        Pal::DeviceProperties deviceProps;
        Pal::PerfExperimentProperties perfExperimentProps;

        // Load device properties
        result = pDevice->GetProperties(&deviceProps);

        if (result == Result::Success)
        {
            // Load PerfExperiment properties
            result = pDevice->GetPerfExperimentProperties(&perfExperimentProps);
        }

        if (result == Result::Success)
        {
            // Populate gpu clock values
            GpuClocksSample gpuClocksSample = {};
            SampleGpuClocks(&gpuClocksSample, pDevice, deviceProps);

            // Populate the TraceAsicChunk with the Asic details
            TraceChunkAsicInfo traceChunkAsicInfo = {};
            result = FillTraceChunkAsicInfo(deviceProps, perfExperimentProps, gpuClocksSample, &traceChunkAsicInfo);

            // Prepare the chunk header and write the chunk data (ie. device info) into TraceSession.
            // Each device corresponds to one chunk in the RDF file.
            if (result == Result::Success)
            {
                TraceChunkInfo info;
                memcpy(info.id, chunkTextIdentifier, GpuUtil::TextIdentifierSize);
                info.pHeader           = nullptr;
                info.headerSize        = 0;
                info.version           = 1;
                info.pData             = &traceChunkAsicInfo;
                info.dataSize          = sizeof(TraceChunkAsicInfo);
                info.enableCompression = false;

                result = m_pPlatform->GetTraceSession()->WriteDataChunk(this, info);
            }
        }
    }
}

// =====================================================================================================================
void AsicInfoTraceSource::OnTraceFinished()
{
    WriteAsicInfoTraceChunk();
}

}
#endif
