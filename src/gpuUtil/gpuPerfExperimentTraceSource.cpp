/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "gpuPerfExperimentTraceSource.h"
#include "palSysUtil.h"
#include "palVectorImpl.h"
#include "palLiterals.h"
#include "core/platform.h"
#include "core/device.h"
#include "util/ddStructuredReader.h"
#include "protocols/rgpServer.h"
#include <sqtt_file_format.h>

using namespace Pal;
using namespace GpuUtil::TraceChunk;
using namespace Util::Literals;
using DevDriver::StructuredValue;

namespace GpuUtil
{

constexpr uint32 DefaultDeviceIndex             = 0;
constexpr uint32 DefaultSqttMemoryLimitInMb     = 80;
constexpr uint32 DefaultSpmMemoryLimitInMb      = 128;
constexpr uint32 DefaultSampleFrequency         = 4096;
constexpr uint32 DefaultSeMask                  = 0;
constexpr bool   DefaultEnableInstructionTokens = false;

constexpr uint32 InstrumentationSpecVersion     = 1;
#if (PAL_BUILD_BRANCH >= 2410)
constexpr uint32 InstrumentationApiVersion      = 5;
#else
constexpr uint32 InstrumentationApiVersion      = 3;
#endif

// =====================================================================================================================
// Extracts Client API information from a PAL Platform and converts it into a GpaSession-friendly format.
static void GetClientApiInfo(
    const Platform* pPlatform,        // [in]  PAL Platform
    ApiType*        pApiType,         // [out] Client API type
    uint16*         pApiMajorVersion, // [out] Client API major version
    uint16*         pApiMinorVersion) // [out] Client API minor version
{
    if ((pPlatform        != nullptr) &&
        (pApiType         != nullptr) &&
        (pApiMajorVersion != nullptr) &&
        (pApiMinorVersion != nullptr))
    {
        switch (pPlatform->GetClientApiId())
        {
        case ClientApi::Dx12:
            (*pApiType)         = ApiType::DirectX12;
            break;
        case ClientApi::Vulkan:
            (*pApiType)         = ApiType::Vulkan;
            break;
        case ClientApi::OpenCl:
            (*pApiType)         = ApiType::OpenCl;
            break;
        case ClientApi::Hip:
            (*pApiType)         = ApiType::Hip;
            break;
        default:
            (*pApiType)         = ApiType::Generic;
            (*pApiMajorVersion) = 0;
            (*pApiMinorVersion) = 0;
            break;
        }

        if (*pApiType != ApiType::Generic)
        {
            (*pApiMajorVersion) = pPlatform->GetClientApiMajorVer();
            (*pApiMinorVersion) = pPlatform->GetClientApiMinorVer();
        }
    }
}

// =====================================================================================================================
GpuPerfExperimentTraceSource::GpuPerfExperimentTraceSource(
    Pal::Platform* pPlatform)
    :
    m_pPlatform(pPlatform),
    m_pGpaSession(nullptr),
    m_gpaSampleId(0),
    m_traceIsHealthy(false),
    m_sqttTraceConfig(
        SqttDataTraceConfig {
            .enabled = false,
            .memoryLimitInMb = DefaultSqttMemoryLimitInMb,
            .enableInstructionTokens = DefaultEnableInstructionTokens,
            .seMask = DefaultSeMask
        }
    ),
    m_spmTraceConfig(
        SpmDataTraceConfig {
            .enabled = false,
            .memoryLimitInMb = DefaultSpmMemoryLimitInMb,
            .sampleFrequency = DefaultSampleFrequency,
            .perfCounterIds = PerfCounterList(pPlatform)
        }
    )
{
}

// =====================================================================================================================
GpuPerfExperimentTraceSource::~GpuPerfExperimentTraceSource()
{
    if (m_pGpaSession != nullptr)
    {
        PAL_SAFE_FREE(m_pGpaSession, m_pPlatform);
    }
}

// =====================================================================================================================
// Update the current config with the given JSON
void GpuPerfExperimentTraceSource::OnConfigUpdated(
    DevDriver::StructuredValue* pJsonConfig)
{
    StructuredValue value;

    if (pJsonConfig->GetValueByKey("sqtt", &value))
    {
        OnSqttConfigUpdated(&value);
    }

    if (pJsonConfig->GetValueByKey("spm", &value))
    {
        OnSpmConfigUpdated(&value);
    }
}

// =====================================================================================================================
// Trace accepted. Initialize the GPA Session.
void GpuPerfExperimentTraceSource::OnTraceAccepted(
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 908
    uint32      gpuIndex,
    ICmdBuffer* pCmdBuf)
#else
    )
#endif
{
    m_traceIsHealthy = true;

    Result  result  = Result::Success;
    Device* pDevice = m_pPlatform->GetDevice(DefaultDeviceIndex);

    ApiType apiType;
    uint16  apiMajorVersion;
    uint16  apiMinorVersion;

    GetClientApiInfo(m_pPlatform, &apiType, &apiMajorVersion, &apiMinorVersion);

    m_pGpaSession = PAL_NEW(GpuUtil::GpaSession,
                            m_pPlatform,
                            Util::SystemAllocType::AllocInternal)
                            (
                                m_pPlatform,
                                pDevice,
                                apiMajorVersion,
                                apiMinorVersion,
                                apiType,
                                InstrumentationSpecVersion,
                                InstrumentationApiVersion
                            );

    if (m_pGpaSession != nullptr)
    {
        result = m_pGpaSession->Init();

        if (result != Result::Success)
        {
            ReportInternalError("Error encountered when initializing the GpaSession",
                                result,
                                m_sqttTraceConfig.enabled);
            PAL_SAFE_FREE(m_pGpaSession, m_pPlatform);
        }
    }
    else
    {
        result = Result::ErrorOutOfMemory;
        ReportInternalError("System is out of memory: cannot allocate trace resources",
                            result,
                            m_sqttTraceConfig.enabled);
    }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 908
    if (result == Result::Success)
    {
        GpaSessionBeginInfo beginInfo = {};
        result = m_pGpaSession->Begin(beginInfo);

        if (result == Result::Success)
        {
            GpaSampleConfig sampleConfig = { };
            sampleConfig.type = GpaSampleType::Trace;

            // Configure SQTT
            if (m_sqttTraceConfig.enabled)
            {
                sampleConfig.sqtt.seDetailedMask = m_sqttTraceConfig.seMask;
                sampleConfig.sqtt.gpuMemoryLimit = m_sqttTraceConfig.memoryLimitInMb * 1_MiB;
                sampleConfig.sqtt.tokenMask = ThreadTraceTokenTypeFlags::All;
                sampleConfig.sqtt.flags.enable = true;
                sampleConfig.sqtt.flags.supressInstructionTokens =
                    (m_sqttTraceConfig.enableInstructionTokens == false);
            }

            // Configure SPM
            if (m_spmTraceConfig.enabled)
            {
                sampleConfig.perfCounters.numCounters = m_spmTraceConfig.perfCounterIds.NumElements();
                sampleConfig.perfCounters.pIds = m_spmTraceConfig.perfCounterIds.Data();
                sampleConfig.perfCounters.spmTraceSampleInterval = m_spmTraceConfig.sampleFrequency;
                sampleConfig.perfCounters.gpuMemoryLimit = m_spmTraceConfig.memoryLimitInMb * 1_MiB;
            }

            // Begin the trace
            result = m_pGpaSession->BeginSample(pCmdBuf, sampleConfig, &m_gpaSampleId);
        }

        if (result != Result::Success)
        {
            ReportInternalError("Error encountered when starting the GpaSession trace sample",
                result,
                m_sqttTraceConfig.enabled);
        }
    }
#endif
}

// =====================================================================================================================
// Begin trace. Begin tracing with GPA Session.
void GpuPerfExperimentTraceSource::OnTraceBegin(
    uint32      gpuIndex,
    ICmdBuffer* pCmdBuf)
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 908
    if (m_traceIsHealthy)
    {
        Result result = Result::Success;

        // move the GpaSession state from 'Reset' to 'Building'
        GpaSessionBeginInfo beginInfo = {};
        result = m_pGpaSession->Begin(beginInfo);

        if (result == Result::Success)
        {
            GpaSampleConfig sampleConfig = { };
            sampleConfig.type            = GpaSampleType::Trace;

            // Configure SQTT
            if (m_sqttTraceConfig.enabled)
            {
                sampleConfig.sqtt.seDetailedMask                   = m_sqttTraceConfig.seMask;
                sampleConfig.sqtt.gpuMemoryLimit                   = m_sqttTraceConfig.memoryLimitInMb * 1_MiB;
                sampleConfig.sqtt.tokenMask                        = ThreadTraceTokenTypeFlags::All;
                sampleConfig.sqtt.flags.enable                     = true;
                sampleConfig.sqtt.flags.supressInstructionTokens   =
                    (m_sqttTraceConfig.enableInstructionTokens == false);
            }

            // Configure SPM
            if (m_spmTraceConfig.enabled)
            {
                sampleConfig.perfCounters.numCounters            = m_spmTraceConfig.perfCounterIds.NumElements();
                sampleConfig.perfCounters.pIds                   = m_spmTraceConfig.perfCounterIds.Data();
                sampleConfig.perfCounters.spmTraceSampleInterval = m_spmTraceConfig.sampleFrequency;
                sampleConfig.perfCounters.gpuMemoryLimit         = m_spmTraceConfig.memoryLimitInMb * 1_MiB;
            }

            // Begin the trace
            result = m_pGpaSession->BeginSample(pCmdBuf, sampleConfig, &m_gpaSampleId);
        }

        if (result != Result::Success)
        {
            ReportInternalError("Error encountered when starting the GpaSession trace sample",
                                result,
                                m_sqttTraceConfig.enabled);
        }
    }
#endif
}

// =====================================================================================================================
// Trace end. End the tracing with GPA Session.
void GpuPerfExperimentTraceSource::OnTraceEnd(
    uint32      gpuIndex,
    ICmdBuffer* pCmdBuf)
{
    if (m_traceIsHealthy)
    {
        m_pGpaSession->EndSample(pCmdBuf, m_gpaSampleId);

        Result result = m_pGpaSession->End(pCmdBuf);

        if (result != Result::Success)
        {
            ReportInternalError("Error encountered when ending the GpaSession", result, m_sqttTraceConfig.enabled);
        }
    }
}

// =====================================================================================================================
// Trace finished. Ensure GPA Session is in READY state and produce the data chunks
void GpuPerfExperimentTraceSource::OnTraceFinished()
{
    if (m_traceIsHealthy)
    {
        Device* pDevice = m_pPlatform->GetDevice(DefaultDeviceIndex);

        // The trace controller should ensure the command buffer previously
        // passed to GpaSession is complete, so GpaSession should be READY
        const bool isReady = m_pGpaSession->IsReady();
        PAL_ASSERT(isReady);

        if (isReady)
        {
            if (m_sqttTraceConfig.enabled)
            {
                WriteSqttDataChunks();
            }

            if (m_spmTraceConfig.enabled)
            {
                WriteSpmDataChunks();
            }
        }
        else
        {
            ReportInternalError("GPA Session is not ready: could not write chunks.",
                                Result::NotReady,
                                m_sqttTraceConfig.enabled);
        }
    }
}

// =====================================================================================================================
void GpuPerfExperimentTraceSource::OnSqttConfigUpdated(
    DevDriver::StructuredValue* pJsonConfig)
{
    StructuredValue value;

    if (pJsonConfig->GetValueByKey("enabled", &value))
    {
        m_sqttTraceConfig.enabled = value.GetBoolOr(false);
    }

    if (pJsonConfig->GetValueByKey("memoryLimitInMb", &value))
    {
        m_sqttTraceConfig.memoryLimitInMb = value.GetUint64Or(DefaultSqttMemoryLimitInMb);
    }

    if (pJsonConfig->GetValueByKey("enableInstructionTokens", &value))
    {
        m_sqttTraceConfig.enableInstructionTokens = value.GetBoolOr(DefaultEnableInstructionTokens);
    }

    if (pJsonConfig->GetValueByKey("seMask", &value))
    {
        m_sqttTraceConfig.seMask = value.GetUint32Or(DefaultSeMask);
    }
}

// =====================================================================================================================
void GpuPerfExperimentTraceSource::OnSpmConfigUpdated(
    StructuredValue* pJsonConfig)
{
    StructuredValue value;

    if (pJsonConfig->GetValueByKey("enabled", &value))
    {
        m_spmTraceConfig.enabled = value.GetBoolOr(false);
    }

    if (pJsonConfig->GetValueByKey("sampleFrequency", &value))
    {
        m_spmTraceConfig.sampleFrequency = value.GetUint32Or(DefaultSampleFrequency);
    }

    if (pJsonConfig->GetValueByKey("memoryLimitInMb", &value))
    {
        m_spmTraceConfig.memoryLimitInMb = value.GetUint32Or(DefaultSpmMemoryLimitInMb);
    }

    if (pJsonConfig->GetValueByKey("perfCounters", &value))
    {
        Result result = Result::Success;

        if (value.IsArray() == false)
        {
            result = Result::ErrorInvalidValue;
        }

        PerfExperimentProperties perfProps = { };

        if (result == Result::Success)
        {
            result = m_pPlatform->GetDevice(DefaultDeviceIndex)->GetPerfExperimentProperties(&perfProps);
        }

        m_spmTraceConfig.perfCounterIds.Clear();

        for (size_t i = 0; (i < value.GetArrayLength()) && (result == Result::Success); i++)
        {
            StructuredValue row = value[i];

            if ((row.IsArray() == false) || (row.GetArrayLength() != 3))
            {
                result = Result::ErrorInvalidValue;
                break;
            }

            const uint32 blockId    = row[0].GetUint32Or(0);
            const uint32 instanceId = row[1].GetUint32Or(0);
            const uint32 eventId    = row[2].GetUint32Or(0);

            if (blockId >= static_cast<uint32>(GpuBlock::Count))
            {
                result = Result::ErrorInvalidValue;
                break;
            }

            const GpuBlockPerfProperties& blockPerfProps = perfProps.blocks[blockId];

            if ((blockPerfProps.available) && (eventId <= blockPerfProps.maxEventId))
            {
                if (instanceId == DevDriver::RGPProtocol::kSpmAllInstancesId)
                {
                    // If the instance id has been set to the special "all instances" value, then the user wants to
                    // gather data from all instances available on the current gpu. Expand this request into as many
                    // perf counter id values as we need to by iterating over the number of instances in the block.
                    for (uint32 j = 0; j < blockPerfProps.instanceCount; ++j)
                    {
                        PerfCounterId counterId = {
                            .block    = GpuBlock(blockId),
                            .instance = j,
                            .eventId  = eventId
                        };

                        m_spmTraceConfig.perfCounterIds.PushBack(counterId);
                    }
                }
                else if (instanceId < blockPerfProps.instanceCount)
                {
                    // This is just a regular counter request.
                    PerfCounterId counterId = {
                        .block    = GpuBlock(blockId),
                        .instance = instanceId,
                        .eventId  = eventId
                    };

                    m_spmTraceConfig.perfCounterIds.PushBack(counterId);
                }
                else
                {
                    result = Result::ErrorInvalidValue;
                    break;
                }
            }
            else
            {
                result = Result::ErrorInvalidValue;
                break;
            }
        }

        // If the SPM counters weren't formatted correctly, emit an error chunk
        if (result != Result::Success)
        {
            ReportInternalError("Invalid trace configuration: SPM Counters are malformed", result, false);
        }
    }
}

// =====================================================================================================================
// Test an SE mask to see if the specified SE is enabled.
// Valid for seMask and seDetailedMask masks.
bool GpuPerfExperimentTraceSource::TestSeMask(
    uint32 seMask,
    uint32 seIndex)
{
    return (seMask == 0) || Util::TestAnyFlagSet(seMask, 1 << seIndex);
}

// =====================================================================================================================
// Writes the SQTT Data chunks to the trace session
void GpuPerfExperimentTraceSource::WriteSqttDataChunks()
{
    Result result = Result::Success;

    for (uint32 traceIndex = 0; result == Result::Success; traceIndex++)
    {
        size_t dataSize = 0;
        result = m_pGpaSession->GetSqttTraceData(m_gpaSampleId,
                                                 traceIndex,
                                                 nullptr,
                                                 &dataSize,
                                                 nullptr);
        PAL_ASSERT((result == Result::Success) || (result == Result::NotFound));

        if (result == Result::Success)
        {
            void* pSqttTraceData = PAL_MALLOC(dataSize, m_pPlatform, Util::AllocInternalTemp);

            if (pSqttTraceData != nullptr)
            {
                SqttTraceInfo traceInfo = { };

                result = m_pGpaSession->GetSqttTraceData(m_gpaSampleId,
                                                         traceIndex,
                                                         &traceInfo,
                                                         &dataSize,
                                                         pSqttTraceData);
                PAL_ASSERT(result == Result::Success);

                if (result == Result::Success)
                {
                    SqttDataHeader sqttDataHeader = {
                        .pciId                      = m_pPlatform->GetPciId(DefaultDeviceIndex).u32All,
                        .shaderEngine               = traceInfo.shaderEngine,
                        .sqttVersion                = traceInfo.sqttVersion,
                        .instrumentationVersionSpec = InstrumentationSpecVersion,
                        .instrumentationVersionApi  = InstrumentationApiVersion,
                        .wgpIndex                   = traceInfo.computeUnit,
                        .traceBufferSize            = traceInfo.bufferSize,
                        .instructionTimingEnabled   = m_sqttTraceConfig.enableInstructionTokens &&
                                                      TestSeMask(m_sqttTraceConfig.seMask,
                                                                 traceInfo.shaderEngine)
                    };

                    TraceChunkInfo info = { };

                    memcpy(info.id, SqttDataTextId, TextIdentifierSize);
                    info.pHeader           = &sqttDataHeader;
                    info.headerSize        = sizeof(SqttDataHeader);
                    info.version           = SqttDataChunkVersion;
                    info.pData             = pSqttTraceData;
                    info.dataSize          = dataSize;
                    info.enableCompression = false;
                    result = m_pPlatform->GetTraceSession()->WriteDataChunk(this, info);
                }

                PAL_SAFE_FREE(pSqttTraceData, m_pPlatform);
            }
            else
            {
                result = Result::ErrorOutOfMemory;
            }
        }
    }

    if ((result != Result::Success) && (result != Result::NotFound))
    {
        ReportInternalError("Error encountered when writing SQTT data chunks", result, true);
    }
}

// =====================================================================================================================
void GpuPerfExperimentTraceSource::WriteSpmDataChunks()
{
    Result result = Result::ErrorInvalidPointer;

    // Get the required buffer size, along with some trace metadata
    SpmTraceInfo traceInfo  = { };
    size_t       bufferSize = 0;
    result = m_pGpaSession->GetSpmTraceData(m_gpaSampleId, &traceInfo, &bufferSize, nullptr);

    // Allocate the buffer & fill with SPM trace data
    void* pSpmData = nullptr;
    if (result == Result::Success)
    {
        pSpmData = PAL_MALLOC(bufferSize, m_pPlatform, Util::SystemAllocType::AllocInternalTemp);

        if (pSpmData != nullptr)
        {
            result = m_pGpaSession->GetSpmTraceData(m_gpaSampleId, nullptr, &bufferSize, pSpmData);
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    if (result == Result::Success)
    {
        // Break apart the constituent pieces of the SPM trace data:
        // Timestamp array & array size
        const uint64* pTimestamps   = static_cast<uint64*>(pSpmData);
        const uint32  timestampSize = traceInfo.numTimestamps * sizeof(uint64);

        // SpmCounterInfo array & array size
        const auto*  pCounterInfo    = static_cast<SpmCounterInfo*>(Util::VoidPtrInc(pSpmData, timestampSize));
        const uint32 counterInfoSize = traceInfo.numSpmCounters * sizeof(SpmCounterInfo);

        result = WriteSpmSessionChunk(traceInfo, pTimestamps, timestampSize);

        if (result == Result::Success)
        {
            result = WriteSpmCounterDataChunks(traceInfo, pCounterInfo, pSpmData);
        }
    }

    if (pSpmData != nullptr)
    {
        PAL_SAFE_FREE(pSpmData, m_pPlatform);
    }

    if (result != Result::Success)
    {
        ReportInternalError("Error encountered when writing SPM data chunks", result, false);
    }
}

// =====================================================================================================================
Result GpuPerfExperimentTraceSource::WriteSpmSessionChunk(
    const SpmTraceInfo& spmTraceInfo,
    const uint64*       pTimestamps,
    size_t              timestampBufferSize)
{
    SpmSessionHeader sessionHeader = {
        .pciId            = m_pPlatform->GetPciId(DefaultDeviceIndex).u32All,
        .flags            = 0x0, // unused
        .samplingInterval = spmTraceInfo.sampleFrequency,
        .numTimestamps    = spmTraceInfo.numTimestamps,
        .numSpmCounters   = spmTraceInfo.numSpmCounters
    };

    TraceChunkInfo info    = {
        .version           = SpmSessionChunkVersion,
        .pHeader           = &sessionHeader,
        .headerSize        = sizeof(SpmSessionHeader),
        .pData             = pTimestamps,
        .dataSize          = static_cast<int64>(timestampBufferSize),
        .enableCompression = false
    };
    memcpy(info.id, SpmSessionChunkId, TextIdentifierSize);

    return m_pPlatform->GetTraceSession()->WriteDataChunk(this, info);
}

// =====================================================================================================================
Result GpuPerfExperimentTraceSource::WriteSpmCounterDataChunks(
    const SpmTraceInfo&   spmTraceInfo,
    const SpmCounterInfo* pCounterInfo,
    const void*           pSpmData)
{
    Result result = Result::Success;

    // Emit the "SpmCounterData" chunks: for each counter, dump counter metadata + sampled counter data
    for (uint32 i = 0; (i < spmTraceInfo.numSpmCounters) && (result == Result::Success); i++)
    {
        SpmCounterDataHeader counterHeader = {
            .pciId         = m_pPlatform->GetPciId(DefaultDeviceIndex).u32All,
            .gpuBlock      = static_cast<GpuBlock>(pCounterInfo[i].block),
            .blockInstance = pCounterInfo[i].instance,
            .eventIndex    = pCounterInfo[i].eventIndex,
            .dataSize      = pCounterInfo[i].dataSize
        };

        PAL_ASSERT(pCounterInfo[i].block < static_cast<uint32>(GpuBlock::Count));
        PAL_ASSERT(pCounterInfo[i].dataSize == sizeof(uint16) || pCounterInfo[i].dataSize == sizeof(uint32));

        TraceChunkInfo info    = {
            .version           = SpmCounterDataChunkVersion,
            .pHeader           = &counterHeader,
            .headerSize        = sizeof(counterHeader),
            .pData             = Util::VoidPtrInc(pSpmData, pCounterInfo[i].dataOffset),
            .dataSize          = pCounterInfo[i].dataSize * spmTraceInfo.numTimestamps,
            .enableCompression = false
        };
        memcpy(info.id, SpmCounterDataChunkId, TextIdentifierSize);

        result = m_pPlatform->GetTraceSession()->WriteDataChunk(this, info);
    }

    return result;
}

// =====================================================================================================================
void GpuPerfExperimentTraceSource::ReportInternalError(
    const char* pErrorMsg,
    Result      result,
    bool        isSqttError)
{
    // Mark that an internal error was encountered and the trace cannot proceed
    m_traceIsHealthy = false;

    // Emit the error message as an RDF chunk
    Result errResult = m_pPlatform->GetTraceSession()->ReportError(isSqttError ? SqttDataTextId : SpmSessionChunkId,
                                                                   pErrorMsg,
                                                                   strlen(pErrorMsg),
                                                                   TraceErrorPayload::ErrorString,
                                                                   result);
    PAL_ASSERT(errResult == Result::Success);
}

} // namespace GpuUtil

#endif
