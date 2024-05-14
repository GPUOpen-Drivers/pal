/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <amdrdf.h>
#include <stdio.h>

#include <ddCommon.h>
#include <ddEventClient.h>
#include <ddEventParser.h>

#include <dd_event/common.h>

namespace DevDriver
{

DD_RESULT ConvertRdfResult(int rResult);

typedef void (*PFN_ddReceiveEvent)(
    void*                  pUserdata,              /// [in] Userdata pointer
    DDEventParserEventInfo eventInfo,              /// [in] Received event info
    const void*            pEventDataPayload,      /// [in] Pointer to event data
    size_t                 eventDataPayloadSize);  /// [in] Event data size

class RdfEventStreamer
{

public:
    RdfEventStreamer(const LoggerUtil& logger);
    ~RdfEventStreamer();

    DD_RESULT BeginStreaming(
        DDClientId      clientId,
        DDNetConnection hConn,
        uint32_t        providerId);

    DD_RESULT EndStreaming(bool isClientAlive);

    uint64_t GetTotalDataSize() { return m_totalDataSize; }

    DD_RESULT TransferDataStream(
        const DDIOHeartbeat& ioHeartbeat,
        rdfChunkFileWriter*  pRdfChunkWriter,
        bool                 useCompression);

    void RegisterReceiveEventFunc(void* pUserdata, PFN_ddReceiveEvent pfnReceiveEvent)
    {
        m_pfnReceiveEvent = pfnReceiveEvent;
        m_pReceiveCbUserdata = pUserdata;
    }

private:

    void OnEventData(const void* pData, size_t dataSize);

    DD_RESULT EventWritePayloadChunk(const DDEventParserEventInfo* pEvent, const void* pData, size_t dataSize);

    static void EventPullingThreadFn(void* pUserdata);

    DD_RESULT Init(
        DDClientId      clientId,
        DDNetConnection hConnection,
        uint32_t        providerId);

    void LogInfo(const char* pFmt, ...);
    void LogError(const char* pFmt, ...);
    void LogErrorOnFailure(bool condition, const char* pFmt, ...);

    DDEventParser                 m_hEventParser;
    DDEventClient                 m_hEventClient;
    uint32_t                      m_providerId;
    bool                          m_isStreaming;
    bool                          m_exitRequested;
    bool                          m_errorOccurred;
    bool                          m_foundFirstEvent;
    DevDriver::Platform::Thread   m_eventThread;
    LoggerUtil                    m_logger;
    FILE*                         m_pStreamFile;
    DevDriver::Platform::Atomic64 m_totalDataSize;
    DevDriver::Platform::Mutex    m_streamMutex;
    DDEventProviderHeader         m_rdfChunkHeader; // Need to collect timestamp info from first event
    PFN_ddReceiveEvent            m_pfnReceiveEvent;
    void*                         m_pReceiveCbUserdata;
    DDEventParserEventInfo        m_currEvent;
};

} // namespace DevDriver
