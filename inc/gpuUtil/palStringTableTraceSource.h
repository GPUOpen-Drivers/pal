/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palTraceSession.h"
#include "palVector.h"

#include <atomic>

namespace Pal
{
class IDevice;
class IPlatform;
} // namespace Pal

namespace GpuUtil
{

class GpaSession;

namespace TraceChunk
{

/// "StringTable" RDF chunk identifier & version
constexpr char        StringTableChunkId[TextIdentifierSize] = "StringTable";
constexpr Pal::uint32 StringTableChunkVersion = 1;

/// Header for the "StringTable" RDF chunk
struct StringTableHeader
{
    Pal::uint32 tableId;        /// The ID of the string table
    Pal::uint32 numStrings;     /// The number of strings in the table
};

} // namespace TraceChunk

/// Trace Source name & version
constexpr char        StringTableTraceSourceName[]  = "stringtable";
constexpr Pal::uint32 StringTableTraceSourceVersion = 1;

// =====================================================================================================================
class StringTableTraceSource : public ITraceSource
{
public:
    StringTableTraceSource(Pal::IPlatform* pPlatform);
    virtual ~StringTableTraceSource();

    void AddStringTable(
        Pal::uint32        tableId,
        Pal::uint32        numStrings,
        const Pal::uint32* pStringOffsets,
        const char*        pStringData,
        Pal::uint32        stringDataSize);

    Pal::uint32 AcquireTableId() { return ++s_nextTableId; }

    // ==== Base Class Overrides =================================================================================== //
    virtual void OnConfigUpdated(DevDriver::StructuredValue* pJsonConfig) override { }

    virtual Pal::uint64 QueryGpuWorkMask() const override { return 0; }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 908
    virtual void OnTraceAccepted(Pal::uint32 gpuIndex, Pal::ICmdBuffer* pCmdBuf) override { }
#else
    virtual void OnTraceAccepted() override { }
#endif
    virtual void OnTraceBegin(Pal::uint32 gpuIndex, Pal::ICmdBuffer* pCmdBuf) override { }
    virtual void OnTraceEnd(Pal::uint32 gpuIndex, Pal::ICmdBuffer* pCmdBuf) override { }
    virtual void OnTraceFinished() override;

    virtual const char* GetName()    const override { return StringTableTraceSourceName; }
    virtual Pal::uint32 GetVersion() const override { return StringTableTraceSourceVersion; }

protected:
    struct StringTableEntry
    {
        Pal::uint32  tableId;       // unique id that identifies this table
        Pal::uint32  numStrings;    // number of strings in this table
        Pal::uint32  chunkSize;     // size of the chunk in bytes
        void*        pChunkData;    // pointer to the chunk data
    };

    Pal::Result WriteStringTableChunks();
    void ClearStringTables();

    Pal::IPlatform* const m_pPlatform;
    Util::Vector<StringTableEntry, 8, Pal::IPlatform> m_stringTables;

    static std::atomic<Pal::uint32> s_nextTableId;
};

} // namespace GpuUtil

