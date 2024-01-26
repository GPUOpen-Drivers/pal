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

#include "core/platform.h"
#include "core/device.h"
#include "palGpaSession.h"
#include "palDequeImpl.h"
#include "palVectorImpl.h"
#include "palHashSetImpl.h"
#include "palCodeObjectTraceSource.h"

#include <sqtt_file_format.h>

using namespace Pal;
using namespace Util;
using namespace GpuUtil::TraceChunk;

namespace GpuUtil
{

// =====================================================================================================================
CodeObjectTraceSource::CodeObjectTraceSource(
    Pal::IPlatform* pPlatform)
    :
    m_pPlatform(pPlatform),
    m_codeObjectRecords(pPlatform),
    m_loadEventRecords(pPlatform),
    m_psoCorrelationRecords(pPlatform),
    m_registeredPipelines(512, m_pPlatform),
    m_registeredApiHashes(512, m_pPlatform)
{
}

// =====================================================================================================================
CodeObjectTraceSource::~CodeObjectTraceSource()
{
    for (auto* pRecord : m_codeObjectRecords)
    {
        PAL_FREE(pRecord, m_pPlatform);
    }
    m_codeObjectRecords.Clear();
    m_loadEventRecords.Clear();
    m_psoCorrelationRecords.Clear();
    m_registeredPipelines.Reset();
    m_registeredApiHashes.Reset();
}

// =====================================================================================================================
void CodeObjectTraceSource::OnTraceFinished()
{
    Result result = Result::Success;

    m_registerPipelineLock.LockForRead();

    if (result == Result::Success)
    {
        result = WriteCodeObjectChunks();
    }

    if (result == Result::Success)
    {
        result = WriteLoaderEventsChunk();
    }

    if (result == Result::Success)
    {
        result = WritePsoCorrelationChunk();
    }

    m_registerPipelineLock.UnlockForRead();
}

// =====================================================================================================================
// Writes out a "CodeObject" chunk for each code object cached during the trace.
Result CodeObjectTraceSource::WriteCodeObjectChunks()
{
    Result result = Result::Success;

    // Emit "CodeObject" chunks...
    for (auto it = m_codeObjectRecords.Begin();
         it.IsValid() && (result == Result::Success);
         it.Next())
    {
        const CodeObjectDatabaseRecord* pRecord = it.Get();
        const void* pCodeObjectBlob = VoidPtrInc(pRecord, sizeof(CodeObjectDatabaseRecord));

        CodeObjectHeader header = {
            .codeObjectHash     = {
                .lower = pRecord->codeObjectHash.lower,
                .upper = pRecord->codeObjectHash.upper
            }
        };

        TraceChunkInfo info    = {
            .version           = CodeObjectChunkVersion,
            .pHeader           = &header,
            .headerSize        = sizeof(CodeObjectHeader),
            .pData             = pCodeObjectBlob,
            .dataSize          = pRecord->recordSize,
            .enableCompression = false
        };
        memcpy(info.id, CodeObjectChunkId, TextIdentifierSize);

        result = m_pPlatform->GetTraceSession()->WriteDataChunk(this, info);
    }

    return result;
}

// =====================================================================================================================
// Writes out a "COLoaderEvent" chunk. This chunk contains all Code Object loader events captured during a trace.
Result CodeObjectTraceSource::WriteLoaderEventsChunk()
{
    Result result = Result::Success;

    if (m_loadEventRecords.NumElements() > 0)
    {
        // Initialize a buffer large enough to store every Code Object Load Event that was registered.
        // This buffer will be the "COLoaderEvent" chunk payload.
        const size_t bufferSize = sizeof(CodeObjectLoadEvent) * m_loadEventRecords.NumElements();
        void* pBuffer = PAL_MALLOC(bufferSize, m_pPlatform, AllocInternal);

        if (pBuffer != nullptr)
        {
            // Copy each load event record into the buffer
            for (uint32 i = 0; i < m_loadEventRecords.NumElements(); i++)
            {
                static_cast<CodeObjectLoadEvent*>(pBuffer)[i] = m_loadEventRecords[i];
            }

            TraceChunkInfo info    = {
                .version           = CodeObjectLoadEventChunkVersion,
                .pHeader           = nullptr,
                .headerSize        = 0,
                .pData             = pBuffer,
                .dataSize          = static_cast<int64>(bufferSize),
                .enableCompression = false
            };
            memcpy(info.id, CodeObjectLoadEventChunkId, TextIdentifierSize);

            result = m_pPlatform->GetTraceSession()->WriteDataChunk(this, info);

            PAL_SAFE_FREE(pBuffer, m_pPlatform);
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    return result;
}

// =====================================================================================================================
// Writes out a "PsoCorrelation" chunk. This chunk contains all PSO Correlation records captured during a trace.
Result CodeObjectTraceSource::WritePsoCorrelationChunk()
{
    Result result = Result::Success;

    if (m_psoCorrelationRecords.NumElements() > 0)
    {
        // Initialize a buffer large enough to store every PSO correlation record that was registered.
        // This buffer will be the "PsoCorrelation" chunk payload.
        const size_t bufferSize = sizeof(PsoCorrelation) * m_psoCorrelationRecords.NumElements();
        void* pBuffer = PAL_MALLOC(bufferSize, m_pPlatform, AllocInternal);

        if (pBuffer != nullptr)
        {
            // Copy each PsoCorrelation record into the buffer
            for (uint32 i = 0; i < m_psoCorrelationRecords.NumElements(); i++)
            {
                static_cast<PsoCorrelation*>(pBuffer)[i] = m_psoCorrelationRecords[i];
            }

            TraceChunkInfo info    = {
                .version           = PsoCorrelationChunkVersion,
                .pHeader           = nullptr,
                .headerSize        = 0,
                .pData             = pBuffer,
                .dataSize          = static_cast<int64>(bufferSize),
                .enableCompression = false
            };
            memcpy(info.id, PsoCorrelationChunkId, TextIdentifierSize);

            result = m_pPlatform->GetTraceSession()->WriteDataChunk(this, info);

            PAL_SAFE_FREE(pBuffer, m_pPlatform);
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    return result;
}

// =====================================================================================================================
// Helper function to add a new code object load event record. Copied from GpaSession.
Result CodeObjectTraceSource::AddCodeObjectLoadEvent(
    const IShaderLibrary*   pLibrary,
    CodeObjectLoadEventType eventType)
{
    PAL_ASSERT(pLibrary != nullptr);
    const LibraryInfo& info = pLibrary->GetInfo();

    size_t numGpuAllocations = 0;
    Result result = pLibrary->QueryAllocationInfo(&numGpuAllocations, nullptr);

    GpuMemSubAllocInfo gpuSubAlloc = { };
    if (result == Result::Success)
    {
        PAL_ASSERT(numGpuAllocations == 1);
        result = pLibrary->QueryAllocationInfo(&numGpuAllocations, &gpuSubAlloc);
    }

    if (result == Result::Success)
    {
        CodeObjectLoadEvent record = {
            .eventType      = eventType,
            .baseAddress    = (gpuSubAlloc.address + gpuSubAlloc.offset),
            .codeObjectHash = { info.internalLibraryHash.stable, info.internalLibraryHash.unique },
            .timestamp      = static_cast<uint64>(GetPerfCpuTime())
        };

        m_registerPipelineLock.LockForWrite();
        result = m_loadEventRecords.PushBack(record);
        m_registerPipelineLock.UnlockForWrite();
    }

    return result;
}

// =====================================================================================================================
// Helper function to add a new code object load event record. Copied from GpaSession.
Result CodeObjectTraceSource::AddCodeObjectLoadEvent(
    const IPipeline*        pPipeline,
    CodeObjectLoadEventType eventType)
{
    PAL_ASSERT(pPipeline != nullptr);
    const PipelineInfo& info = pPipeline->GetInfo();

    size_t numGpuAllocations = 0;
    Result result = pPipeline->QueryAllocationInfo(&numGpuAllocations, nullptr);

    GpuMemSubAllocInfo gpuSubAlloc = { };
    if (result == Result::Success)
    {
        PAL_ASSERT(numGpuAllocations == 1);
        result = pPipeline->QueryAllocationInfo(&numGpuAllocations, &gpuSubAlloc);
    }

    if (result == Result::Success)
    {
        CodeObjectLoadEvent record = {
            .eventType      = eventType,
            .baseAddress    = (gpuSubAlloc.address + gpuSubAlloc.offset),
            .codeObjectHash = { info.internalPipelineHash.stable, info.internalPipelineHash.unique },
            .timestamp      = static_cast<uint64>(GetPerfCpuTime())
        };

        m_registerPipelineLock.LockForWrite();
        result = m_loadEventRecords.PushBack(record);
        m_registerPipelineLock.UnlockForWrite();
    }

    return result;
}

// =====================================================================================================================
// Helper function to add a new code object load event record. Copied from GpaSession.
Result CodeObjectTraceSource::AddCodeObjectLoadEvent(
    const ElfBinaryInfo&    elfBinaryInfo,
    CodeObjectLoadEventType eventType)
{
    PAL_ASSERT(elfBinaryInfo.pBinary != nullptr);
    PAL_ASSERT(elfBinaryInfo.pGpuMemory != nullptr);

    CodeObjectLoadEvent record = {
        .eventType      = eventType,
        .baseAddress    = elfBinaryInfo.pGpuMemory->Desc().gpuVirtAddr + elfBinaryInfo.offset,
        .codeObjectHash = { elfBinaryInfo.originalHash, elfBinaryInfo.compiledHash },
        .timestamp      = static_cast<uint64>(GetPerfCpuTime())
    };

    m_registerPipelineLock.LockForWrite();
    Result result = m_loadEventRecords.PushBack(record);
    m_registerPipelineLock.UnlockForWrite();

    return result;
}

// =====================================================================================================================
Result CodeObjectTraceSource::RegisterLibrary(
    const IShaderLibrary*      pLibrary,
    const RegisterLibraryInfo& clientInfo)
{
    PAL_ASSERT(pLibrary != nullptr);

    // Even if the library was already previously encountered, we still want to record every time it gets loaded.
    Result result = AddCodeObjectLoadEvent(pLibrary, CodeObjectLoadEventType::LoadToGpuMemory);

    const LibraryInfo& libraryInfo = pLibrary->GetInfo();

    m_registerPipelineLock.LockForWrite();
    if ((result == Result::Success) && (clientInfo.apiHash != 0))
    {
        MetroHash::Hash tempHash = { };

        MetroHash128 hasher;
        hasher.Update(clientInfo.apiHash);
        hasher.Update(libraryInfo.internalLibraryHash);
        hasher.Finalize(tempHash.bytes);

        const uint64 uniqueHash = MetroHash::Compact64(&tempHash);

        if (m_registeredApiHashes.Contains(uniqueHash) == false)
        {
            // Record a mapping of API hash -> internal library hash so they can be correlated.
            TraceChunk::PsoCorrelation record = {
                .apiPsoHash           = clientInfo.apiHash,
                .internalPipelineHash = libraryInfo.internalLibraryHash,
                .apiLevelObjectName   = { '\0' } // unused
            };
            result = m_psoCorrelationRecords.PushBack(record);

            if (result == Result::Success)
            {
                result = m_registeredApiHashes.Insert(uniqueHash);
            }
        }
    }

    // Record the compiled hash, if it hasn't been seen before
    if (result == Result::Success)
    {
        result = m_registeredPipelines.Contains(libraryInfo.internalLibraryHash.unique)
               ? Result::AlreadyExists
               : m_registeredPipelines.Insert(libraryInfo.internalLibraryHash.unique);
    }
    m_registerPipelineLock.UnlockForWrite();

    // Store a copy of the code object & associated metadata
    if (result == Result::Success)
    {
        void* pCodeObjectRecord = nullptr;

        CodeObjectDatabaseRecord record = { };
        record.codeObjectHash = { libraryInfo.internalLibraryHash.stable, libraryInfo.internalLibraryHash.unique };

        result = pLibrary->GetCodeObject(&record.recordSize, nullptr);

        if (result == Result::Success)
        {
            // Pad the record size to the nearest multiple of 4 bytes per the RGP file format spec.
            record.recordSize = Pow2Align(record.recordSize, 4U);

            // Allocate space to store all the information for one record.
            pCodeObjectRecord = PAL_MALLOC((sizeof(CodeObjectDatabaseRecord) + record.recordSize),
                                           m_pPlatform,
                                           SystemAllocType::AllocInternal);

            if (pCodeObjectRecord != nullptr)
            {
                // Write the record header.
                memcpy(pCodeObjectRecord, &record, sizeof(record));

                // Write the code object binary.
                result = pLibrary->GetCodeObject(&record.recordSize, VoidPtrInc(pCodeObjectRecord, sizeof(record)));

                if (result != Result::Success)
                {
                    // Deallocate if some error occurred.
                    PAL_SAFE_FREE(pCodeObjectRecord, m_pPlatform)
                }
            }
            else
            {
                result = Result::ErrorOutOfMemory;
            }
        }

        if (result == Result::Success)
        {
            m_registerPipelineLock.LockForWrite();
            m_codeObjectRecords.PushBack(static_cast<CodeObjectDatabaseRecord*>(pCodeObjectRecord));
            m_registerPipelineLock.UnlockForWrite();
        }
    }

    return result;
}

// =====================================================================================================================
Result CodeObjectTraceSource::UnregisterLibrary(
    const IShaderLibrary* pLibrary)
{
    return AddCodeObjectLoadEvent(pLibrary, CodeObjectLoadEventType::UnloadFromGpuMemory);
}

// =====================================================================================================================
Result CodeObjectTraceSource::RegisterPipeline(
    IPipeline*           pPipeline,
    RegisterPipelineInfo pipelineInfo)
{
    PAL_ASSERT(pPipeline != nullptr);

    // Even if the pipeline was already previously encountered, we still want to record every time it gets loaded.
    Result result = AddCodeObjectLoadEvent(pPipeline, CodeObjectLoadEventType::LoadToGpuMemory);

    const PipelineInfo& pipeInfo = pPipeline->GetInfo();

    m_registerPipelineLock.LockForWrite();
    if ((result == Result::Success) && (pipelineInfo.apiPsoHash != 0))
    {
        MetroHash::Hash tempHash = {};

        MetroHash128 hasher;
        hasher.Update(pipelineInfo.apiPsoHash);
        hasher.Update(pipeInfo.internalPipelineHash);
        hasher.Finalize(tempHash.bytes);

        const uint64 uniqueHash = MetroHash::Compact64(&tempHash);

        if (m_registeredApiHashes.Contains(uniqueHash) == false)
        {
            // Record a mapping of API PSO hash -> internal pipeline hash so they can be correlated.
            TraceChunk::PsoCorrelation record = {
                .apiPsoHash           = pipelineInfo.apiPsoHash,
                .internalPipelineHash = pipeInfo.internalPipelineHash,
                .apiLevelObjectName   = { '\0' } // unused
            };
            result = m_psoCorrelationRecords.PushBack(record);

            if (result == Result::Success)
            {
                result = m_registeredApiHashes.Insert(uniqueHash);
            }
        }
    }

    // Record the compiled hash, if it hasn't been seen before
    if (result == Result::Success)
    {
        const uint64 hash = pipeInfo.internalPipelineHash.unique ^ pipeInfo.internalPipelineHash.stable;
        result = m_registeredPipelines.Contains(hash)
               ? Result::AlreadyExists
               : m_registeredPipelines.Insert(hash);
    }
    m_registerPipelineLock.UnlockForWrite();

    // Store a copy of the code object & associated metadata
    if (result == Result::Success)
    {
        CodeObjectDatabaseRecord record = { };
        record.codeObjectHash = { pipeInfo.internalPipelineHash.stable, pipeInfo.internalPipelineHash.unique };

        result = pPipeline->GetCodeObject(&record.recordSize, nullptr);

        void* pCodeObjectRecord = nullptr;

        if (result == Result::Success)
        {
            PAL_ASSERT(record.recordSize != 0);

            // Pad the record size to the nearest multiple of 4 bytes per the RGP file format spec.
            record.recordSize = Pow2Align(record.recordSize, 4U);

            // Allocate space to store all the information for one record.
            pCodeObjectRecord = PAL_MALLOC((sizeof(CodeObjectDatabaseRecord) + record.recordSize),
                                           m_pPlatform,
                                           SystemAllocType::AllocInternal);

            if (pCodeObjectRecord != nullptr)
            {
                // Write the record header.
                memcpy(pCodeObjectRecord, &record, sizeof(record));

                // Write the pipeline binary.
                result = pPipeline->GetCodeObject(&record.recordSize, VoidPtrInc(pCodeObjectRecord, sizeof(record)));

                if (result != Result::Success)
                {
                    // Deallocate if some error occurred.
                    PAL_SAFE_FREE(pCodeObjectRecord, m_pPlatform)
                }
            }
            else
            {
                result = Result::ErrorOutOfMemory;
            }
        }

        if (result == Result::Success)
        {
            m_registerPipelineLock.LockForWrite();
            m_codeObjectRecords.PushBack(static_cast<CodeObjectDatabaseRecord*>(pCodeObjectRecord));
            m_registerPipelineLock.UnlockForWrite();
        }
    }

    return result;
}

// =====================================================================================================================
Result CodeObjectTraceSource::UnregisterPipeline(
    IPipeline* pPipeline)
{
    return AddCodeObjectLoadEvent(pPipeline, CodeObjectLoadEventType::UnloadFromGpuMemory);
}

// =====================================================================================================================
Result CodeObjectTraceSource::RegisterElfBinary(
    const ElfBinaryInfo& elfBinaryInfo)
{
    PAL_ASSERT(elfBinaryInfo.pBinary != nullptr);

    // Even if the library was already previously encountered, we still want to record every time it gets loaded.
    Result result = AddCodeObjectLoadEvent(elfBinaryInfo, CodeObjectLoadEventType::LoadToGpuMemory);

    m_registerPipelineLock.LockForWrite();
    if ((result == Result::Success) && (elfBinaryInfo.originalHash != 0))
    {
        MetroHash::Hash tempHash = {};

        MetroHash128 hasher;
        hasher.Update(elfBinaryInfo.originalHash);
        hasher.Update(elfBinaryInfo.compiledHash);
        hasher.Finalize(tempHash.bytes);

        const uint64 uniqueHash = MetroHash::Compact64(&tempHash);

        if (m_registeredApiHashes.Contains(uniqueHash) == false)
        {
            // Record a mapping of API hash -> internal library hash so they can be correlated.
            PsoCorrelation record = {
                .apiPsoHash           = elfBinaryInfo.originalHash,
                .internalPipelineHash =
                {
                    .stable = elfBinaryInfo.compiledHash,
                    .unique = uniqueHash
                }
            };
            result = m_psoCorrelationRecords.PushBack(record);

            if (result == Result::Success)
            {
                result = m_registeredApiHashes.Insert(uniqueHash);
            }
        }
    }

    // Record the compiled hash, if it hasn't been seen before
    if (result == Result::Success)
    {
        result = m_registeredPipelines.Contains(elfBinaryInfo.compiledHash)
               ? Result::AlreadyExists
               : m_registeredPipelines.Insert(elfBinaryInfo.compiledHash);
    }
    m_registerPipelineLock.UnlockForWrite();

    // Store a copy of the code object & associated metadata
    if (result == Result::Success)
    {
        PAL_ASSERT(elfBinaryInfo.binarySize != 0);

        CodeObjectDatabaseRecord record = {
            .recordSize     = Pow2Align(elfBinaryInfo.binarySize, 4U), // Pad to 4 byte boundary per the RGP spec
            .codeObjectHash = { elfBinaryInfo.originalHash, elfBinaryInfo.compiledHash }
        };

        // Allocate space to store all the information for one record.
        void* pCodeObjectRecord = PAL_MALLOC((sizeof(CodeObjectDatabaseRecord) + record.recordSize),
                                              m_pPlatform,
                                              SystemAllocType::AllocInternal);

        if (pCodeObjectRecord != nullptr)
        {
            // Write the record header.
            memcpy(pCodeObjectRecord, &record, sizeof(record));

            // Write the code object binary.
            memcpy(VoidPtrInc(pCodeObjectRecord,
                              sizeof(record)),
                              elfBinaryInfo.pBinary,
                              record.recordSize);

        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }

        if (result == Result::Success)
        {
            m_registerPipelineLock.LockForWrite();
            m_codeObjectRecords.PushBack(static_cast<CodeObjectDatabaseRecord*>(pCodeObjectRecord));
            m_registerPipelineLock.UnlockForWrite();
        }
    }

    return result;
}

// =====================================================================================================================
Result CodeObjectTraceSource::UnregisterElfBinary(
    const ElfBinaryInfo& elfBinaryInfo)
{
    return AddCodeObjectLoadEvent(elfBinaryInfo, CodeObjectLoadEventType::UnloadFromGpuMemory);
}

} // namespace GpuUtil

#endif
