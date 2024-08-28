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

#pragma once

#include "palGpuUtil.h"
#include "palGpaSession.h"
#include "palTraceSession.h"

// Forward declarations
struct SpmCounterInfo;
namespace Pal
{
class Platform;
}

namespace GpuUtil
{
namespace TraceChunk
{
/// "SpmSession" RDF chunk identifier & version
constexpr char        SpmSessionChunkId[TextIdentifierSize] = "SpmSession";
constexpr Pal::uint32 SpmSessionChunkVersion                = 2;

/// Header for the "SpmSession" RDF chunk
struct SpmSessionHeader
{
    Pal::uint32 pciId;            /// The ID of the GPU the trace ran on
    Pal::uint32 flags;            /// SPM trace configuration flags (reserved for future use)
    Pal::uint32 samplingInterval; /// Perf. counter sampling interval
    Pal::uint32 numTimestamps;    /// Number of timestamps in the SPM trace data
    Pal::uint32 numSpmCounters;   /// Number of SPM counters sampled
};

/// "SpmCounterData" RDF chunk identifier & version
constexpr char        SpmCounterDataChunkId[TextIdentifierSize] = "SpmCounterData";
constexpr Pal::uint32 SpmCounterDataChunkVersion                = 2;

/// Header for the "SpmCounterData" RDF chunk
struct SpmCounterDataHeader
{
    Pal::uint32   pciId;         /// The ID of the GPU the trace ran on
    Pal::GpuBlock gpuBlock;      /// GPU block encoding
    Pal::uint32   blockInstance; /// Instance of the block in the ASIC
    Pal::uint32   eventIndex;    /// Index of the perf. counter event within the block
    Pal::uint32   dataSize;      /// Size (in bytes) of a single counter data item
};

constexpr char        SqttDataTextId[TextIdentifierSize] = "SqttData";
constexpr Pal::uint32 SqttDataChunkVersion               = 4;

/// SQTT Data RDF chunk
struct SqttDataHeader
{
    Pal::uint32 pciId;
    Pal::uint32 shaderEngine;
    Pal::uint32 sqttVersion;
    Pal::uint32 instrumentationVersionSpec;
    Pal::uint32 instrumentationVersionApi;
    Pal::uint32 wgpIndex;
    Pal::uint64 traceBufferSize;
    Pal::uint32 instructionTimingEnabled : 1;
    Pal::uint32 reserved                 : 31;
};

} // namespace TraceChunk

constexpr char        GpuPerfExpTraceSourceName[]  = "gpuperfexp";
constexpr Pal::uint32 GpuPerfExpTraceSourceVersion = 1;

// =====================================================================================================================
// This trace source manages an SQTT & SPM trace through GPA Session and produces SQTT & SPM Data RDF chunks
class GpuPerfExperimentTraceSource final : public ITraceSource
{
public:
    explicit GpuPerfExperimentTraceSource(Pal::Platform* pPlatform);
    virtual ~GpuPerfExperimentTraceSource();

    virtual void OnConfigUpdated(DevDriver::StructuredValue* pJsonConfig) override;

    virtual Pal::uint64 QueryGpuWorkMask() const override { return 0; }

    virtual void OnTraceAccepted() override;
    virtual void OnTraceBegin(
        Pal::uint32      gpuIndex,
        Pal::ICmdBuffer* pCmdBuf) override;
    virtual void OnTraceEnd(
        Pal::uint32      gpuIndex,
        Pal::ICmdBuffer* pCmdBuf) override;
    virtual void OnTraceFinished() override;

    virtual const char* GetName() const override { return GpuPerfExpTraceSourceName; }

    virtual Pal::uint32 GetVersion() const override { return GpuPerfExpTraceSourceVersion; }

private:
    // Writes the SqttData chunks to the trace session.
    void WriteSqttDataChunks();

    // Writes the SpmData chunks to the trace session.
    void WriteSpmDataChunks();

    void OnSpmConfigUpdated(DevDriver::StructuredValue* pJsonConfig);
    void OnSqttConfigUpdated(DevDriver::StructuredValue* pJsonConfig);

    void ReportInternalError(const char* pErrorMsg, Pal::Result result, bool isSqttError);

    Pal::Result WriteSpmSessionChunk(
        const SpmTraceInfo& traceInfo,
        const Pal::uint64*  pTimestamps,
        size_t              timestampBufferSize);

    Pal::Result WriteSpmCounterDataChunks(
        const SpmTraceInfo&   spmTraceInfo,
        const SpmCounterInfo* pCounterInfo,
        const void*           pSpmData);

    typedef Util::Vector<PerfCounterId, 8, Pal::Platform> PerfCounterList;

    struct SpmDataTraceConfig
    {
        bool            enabled;
        Pal::uint32     memoryLimitInMb;
        Pal::uint32     sampleFrequency;
        PerfCounterList perfCounterIds;
    };

    struct SqttDataTraceConfig
    {
        bool enabled;
        Pal::uint64 memoryLimitInMb;
        bool        enableInstructionTokens;
        Pal::uint32 seMask;
    };

    Pal::Platform* const m_pPlatform;
    GpaSession*          m_pGpaSession;
    Pal::uint32          m_gpaSampleId;
    bool                 m_traceIsHealthy;
    SpmDataTraceConfig   m_spmTraceConfig;
    SqttDataTraceConfig  m_sqttTraceConfig;
};

} // namespace GpuUtil
