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

#include "palGpaSession.h"
#include "palGpuUtil.h"
#include "palTraceSession.h"
#include "palVector.h"
#include "palHashSet.h"
#include "palMutex.h"

namespace Pal
{
class IPlatform;
class IDevice;
class IShaderLibrary;
} // namespace Pal

namespace GpuUtil
{
class GpaSession;
} // namespace GpuUtil

namespace GpuUtil
{

namespace TraceChunk
{

/// "CodeObject" RDF chunk identifier & version
constexpr char        CodeObjectChunkId[TextIdentifierSize] = "CodeObject";
constexpr Pal::uint32 CodeObjectChunkVersion                = 2;

/// Header for the "CodeObject" RDF chunk
struct CodeObjectHeader
{
    Pal::uint32     pciId;          /// The ID of the GPU the trace was run on
    Pal::ShaderHash codeObjectHash; /// Hash of the code object binary
};

/// "COLoadEvent" RDF chunk identifier & version
constexpr char        CodeObjectLoadEventChunkId[TextIdentifierSize] = "COLoadEvent";
constexpr Pal::uint32 CodeObjectLoadEventChunkVersion                = 3;

struct CodeObjectLoadEventHeader
{
    Pal::uint32 count; /// Number of load events in this chunk
};

/// Describes whether a load event was into GPU memory or from.
enum class CodeObjectLoadEventType : Pal::uint32
{
    LoadToGpuMemory     = 0, /// Code Object was loaded into GPU memory
    UnloadFromGpuMemory = 1  /// Code Object was unloaded from GPU memory
};

/// Describes a GPU load/unload of a Code Object. Payload for "COLoadEvent" RDF chunk.
struct CodeObjectLoadEvent
{
    Pal::uint32             pciId;          /// The ID of the GPU the trace was run on
    CodeObjectLoadEventType eventType;      /// Type of loader event
    Pal::uint64             baseAddress;    /// Base address where the code object was loaded
    Pal::ShaderHash         codeObjectHash; /// Hash of the (un)loaded code object binary
    Pal::uint64             timestamp;      /// CPU timestamp of this event being triggered
};

/// "PsoCorrelation" RDF chunk identifier & version
constexpr char        PsoCorrelationChunkId[TextIdentifierSize] = "PsoCorrelation";
constexpr Pal::uint32 PsoCorrelationChunkVersion                = 3;

struct PsoCorrelationHeader
{
    Pal::uint32 count;  /// Number of PSO correlations in this chunk
};

/// Payload for the "PsoCorrelation" RDF chunk
struct PsoCorrelation
{
    Pal::uint32       pciId;                  /// The ID of the GPU the trace was run on
    Pal::uint64       apiPsoHash;             /// Hash of the API-level Pipeline State Object
    Pal::PipelineHash internalPipelineHash;   /// Hash of all inputs to the pipeline compiler
    char              apiLevelObjectName[64]; /// Debug object name (null-terminated)
};

} // namespace TraceChunk

/// CodeObject Trace Source name & version
constexpr char        CodeObjectTraceSourceName[]  = "codeobject";
constexpr Pal::uint32 CodeObjectTraceSourceVersion = 3;

// =====================================================================================================================
class CodeObjectTraceSource : public ITraceSource
{
public:
    CodeObjectTraceSource(Pal::IPlatform* pPlatform);
    ~CodeObjectTraceSource();

    // ==== TraceSource Native Functions ========================================================================== //
    Pal::Result RegisterPipeline(Pal::IPipeline* pPipeline, RegisterPipelineInfo pipelineInfo);
    Pal::Result UnregisterPipeline(Pal::IPipeline* pPipeline);

    Pal::Result RegisterLibrary(const Pal::IShaderLibrary* pLibrary, const RegisterLibraryInfo& clientInfo);
    Pal::Result UnregisterLibrary(const Pal::IShaderLibrary* pLibrary);

    Pal::Result RegisterElfBinary(const ElfBinaryInfo& elfBinaryInfo);
    Pal::Result UnregisterElfBinary(const ElfBinaryInfo& elfBinaryInfo);

    // ==== Base Class Overrides =================================================================================== //
    virtual void OnConfigUpdated(DevDriver::StructuredValue* pJsonConfig) override { }

    virtual Pal::uint64 QueryGpuWorkMask() const override { return 0; }

    virtual void OnTraceAccepted() override { }
    virtual void OnTraceBegin(Pal::uint32 gpuIndex, Pal::ICmdBuffer* pCmdBuf) override { }
    virtual void OnTraceEnd(Pal::uint32 gpuIndex, Pal::ICmdBuffer* pCmdBuf) override { }
    virtual void OnTraceFinished() override;

    virtual const char* GetName()    const override { return CodeObjectTraceSourceName; }
    virtual Pal::uint32 GetVersion() const override { return CodeObjectTraceSourceVersion; }

private:
    Pal::Result AddCodeObjectLoadEvent(
        const Pal::IShaderLibrary*          pLibrary,
        TraceChunk::CodeObjectLoadEventType eventType);
    Pal::Result AddCodeObjectLoadEvent(
        const Pal::IPipeline*               pLibrary,
        TraceChunk::CodeObjectLoadEventType eventType);
    Pal::Result AddCodeObjectLoadEvent(
        const ElfBinaryInfo&                elfBinaryInfo,
        TraceChunk::CodeObjectLoadEventType eventType);

    Pal::Result WriteCodeObjectChunks();
    Pal::Result WriteLoaderEventsChunk();
    Pal::Result WritePsoCorrelationChunk();

    struct CodeObjectDatabaseRecord
    {
        Pal::uint32     recordSize;
        Pal::ShaderHash codeObjectHash;
    };

    Pal::IPlatform* const m_pPlatform;

    Util::RWLock                                                      m_registerPipelineLock;
    Util::Vector<CodeObjectDatabaseRecord*,       8, Pal::IPlatform>  m_codeObjectRecords;
    Util::Vector<TraceChunk::CodeObjectLoadEvent, 8, Pal::IPlatform>  m_loadEventRecords;
    Util::Vector<TraceChunk::PsoCorrelation,      8, Pal::IPlatform>  m_psoCorrelationRecords;
    Util::HashSet<Pal::uint64, Pal::IPlatform, Util::JenkinsHashFunc> m_registeredPipelines;
    Util::HashSet<Pal::uint64, Pal::IPlatform, Util::JenkinsHashFunc> m_registeredApiHashes;
};

} // namespace GpuUtil

