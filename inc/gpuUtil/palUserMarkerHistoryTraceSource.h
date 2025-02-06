/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palHashMap.h"
#include "palTraceSession.h"

namespace Pal
{
class IPlatform;
class IDevice;
} // namespace Pal

namespace GpuUtil
{
class GpaSession;

namespace TraceChunk
{

/// "StringTable" RDF chunk identifier & version
constexpr char        UserMarkerHistoryChunkId[TextIdentifierSize] = "UserMarkerHist";
constexpr Pal::uint32 UserMarkerHistoryChunkVersion = 1;

/// Header for the "UserMarkerHistory" RDF chunk
struct UserMarkerHistoryHeader
{
    Pal::uint32 sqttCbId;
    Pal::uint32 tableId;
    Pal::uint32 numOps;
};

} // namespace TraceChunk

/// Trace Source name & version
constexpr char        UserMarkerHistoryTraceSourceName[] = "usermarkerhist";
constexpr Pal::uint32 UserMarkerHistoryTraceSourceVersion = 1;

// =====================================================================================================================
class UserMarkerHistoryTraceSource : public ITraceSource
{
public:
    UserMarkerHistoryTraceSource(Pal::IPlatform* pPlatform);
    virtual ~UserMarkerHistoryTraceSource();

    void AddUserMarkerHistory(Pal::uint32        sqttCbId,
                              Pal::uint32        tableId,
                              Pal::uint32        numOps,
                              const Pal::uint32* pUserMarkerHistory);

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

    virtual const char* GetName()    const override { return UserMarkerHistoryTraceSourceName; }
    virtual Pal::uint32 GetVersion() const override { return UserMarkerHistoryTraceSourceVersion; }

private:
    struct UserMarkerHistoryEntry
    {
        Pal::uint32  tableId;               // unique ID for the table
        Pal::uint32  numOps;                // number of user marker operations in the table
        Pal::uint32* pUserMarkerHistory;    // pointer to the user marker history data
    };

    typedef Util::HashMap<Pal::uint32, UserMarkerHistoryEntry, Pal::IPlatform> UserMarkerHistoryMap;

    Pal::Result WriteUserMarkerHistoryChunks();
    void ClearUserMarkerHistoryMap();

    Pal::IPlatform* const m_pPlatform;
    UserMarkerHistoryMap  m_userMarkerHistoryMap;
};

} // namespace GpuUtil

