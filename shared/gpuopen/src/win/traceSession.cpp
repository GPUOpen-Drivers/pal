/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "traceSession.h"
#include <tchar.h>
#include <string>
#include <ObjBase.h>

#include "ddPlatform.h"

#define SAFE_DELETE(x) {if(x){free(x); (x) = nullptr; }}
#define SAFE_DELETE_ARRAY(x) {if(x){free(x); x = nullptr; }}

using namespace DevDriver;
//-----------------------------------------------------------------------------
/// Constructor for the TraceSession class.
/// \param sessionName The name of the tracing session.
//-----------------------------------------------------------------------------
TraceSession::TraceSession()
    : m_session({})
    , m_traceLogFile({})
    , m_sessionHandle(0)
    , m_traceHandle(0)
{
}

//-----------------------------------------------------------------------------
/// Destructor for the TraceSession class.
//-----------------------------------------------------------------------------
TraceSession::~TraceSession()
{
}

//-----------------------------------------------------------------------------
/// Start the trace session.
/// \returns True when the session was started correctly, and false if it failed.
//-----------------------------------------------------------------------------
bool TraceSession::Start(const char* pSessionName)
{
    bool result = false;
    DD_PRINT(LogLevel::Verbose, "[TraceSession::Start] Start called");

    if (m_sessionHandle == 0)
    {
        ZeroMemory(&m_session, sizeof(m_session));
        m_session.properties.Wnode.BufferSize = sizeof(m_session);

        Platform::Strncpy(m_session.name, pSessionName, sizeof(m_session.name));

        // Setting this to "1" means event timestamps will be based on QueryPerformanceCounter.
        m_session.properties.Wnode.ClientContext = 1;
        m_session.properties.Wnode.Flags = WNODE_FLAG_TRACED_GUID;
        m_session.properties.LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
        m_session.properties.LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
        m_session.properties.LogFileNameOffset = 0;

        // Create the trace session.
        ULONG status = StartTrace(&m_sessionHandle, m_session.name, &m_session.properties);
        DD_PRINT(LogLevel::Info, "[TraceSession::Start] Etw Trace StartTrace() status == %u", status);

        // If we fail to start the trace because one already exists with the same name, attempt to
        // stop the existing trace, then start a new one.
        if (status == ERROR_ALREADY_EXISTS)
        {
            DD_PRINT(LogLevel::Info, "[TraceSession::Start] Etw Trace already exists - Stopping.");
            // Stop the existing trace.
            status = ControlTrace(NULL, m_session.name, &m_session.properties, EVENT_TRACE_CONTROL_STOP);
            DD_PRINT(LogLevel::Info, "[TraceSession::Start] Etw Trace ControlTrace(Stop) status == %u", status);
            if (status == ERROR_SUCCESS)
            {
                // Start a new trace if we successfully stopped the existing one.
                status = StartTrace(&m_sessionHandle, m_session.name, &m_session.properties);
                DD_PRINT(LogLevel::Info, "[TraceSession::Start] Etw Trace StartTrace() (second) status == %u", status);
            }
        }

        result = (status == ERROR_SUCCESS);
        DD_PRINT(LogLevel::Verbose, "[TraceSession::Start] Start: %u", status);
    }
    return result;
}

//-----------------------------------------------------------------------------
/// Enable the ETW provider with the incoming GUID.
/// \param providerId The GUID for the provider to enable.
/// \param level The level of detail to provide in each logged event.
/// \param anyKeyword A bitmask to determine the set of events to provide.
/// \param allKeyword A bitmask to restrict the set of event categories to provide.
/// \returns True if enabling the provider was successful, and false if it failed.
//-----------------------------------------------------------------------------
bool TraceSession::EnableProvider(const GUID& providerId, UCHAR level, ULONGLONG anyKeyword, ULONGLONG allKeyword)
{
    bool result = false;
    DD_PRINT(LogLevel::Verbose, "[TraceSession::EnableProvider] EnableProvider called");

    if (m_sessionHandle != 0)
    {
        ULONG status = EnableTraceEx2(m_sessionHandle, &providerId, EVENT_CONTROL_CODE_ENABLE_PROVIDER, level, anyKeyword, allKeyword, 0, nullptr);
        result = (status == ERROR_SUCCESS);
    }
    return result;
}

//-----------------------------------------------------------------------------
/// Enable the ETW provider with the incoming GUID string.
/// \param providerId The GUID string for the provider to enable.
/// \param level The level of detail to provide in each logged event.
/// \param anyKeyword A bitmask to determine the set of events to provide.
/// \param allKeyword A bitmask to restrict the set of event categories to provide.
/// \returns True if enabling the provider was successful, and false if it failed.
//-----------------------------------------------------------------------------
bool TraceSession::EnableProviderByGUID(const LPCWSTR& inGUID, UCHAR level, ULONGLONG anyKeyword, ULONGLONG allKeyword)
{
    bool result = false;
    DD_PRINT(LogLevel::Verbose, "[TraceSession::EnableProviderByGUID] EnableProviderByGUID called");
    if (m_sessionHandle != 0)
    {
        GUID providerGuid;
        HRESULT converted = CLSIDFromString(inGUID, &providerGuid);

        if (converted == S_OK)
        {
            ULONG status = EnableTraceEx2(m_sessionHandle, &providerGuid, EVENT_CONTROL_CODE_ENABLE_PROVIDER, level, anyKeyword, allKeyword, 0, nullptr);
            result = (status == ERROR_SUCCESS);
            DD_PRINT(LogLevel::Verbose, "[TraceSession::EnableProviderByGUID] Provider enabled: %u",status);
        }
    }

    // Failed to convert the incoming GUID.
    return result;
}

//-----------------------------------------------------------------------------
/// The global callback for all incoming ETW events.
/// \param pEventRecord The ETW event structure raised by the provider.
//-----------------------------------------------------------------------------
VOID WINAPI EventRecordCallback(PEVENT_RECORD pEventRecord)
{
    reinterpret_cast<ETWConsumerBase*>(pEventRecord->UserContext)->OnEventRecord(pEventRecord);
}

//-----------------------------------------------------------------------------
/// On a trace with the provided consumer.
/// \param pConsumer The consumer to use when opening a new tracing session.
/// \returns True when the trace was opened successfully, and false if it failed.
//-----------------------------------------------------------------------------
bool TraceSession::Open(ETWConsumerBase* pConsumer)
{
    bool result = false;
    DD_ASSERT(pConsumer != nullptr);
    DD_PRINT(LogLevel::Verbose, "[TraceSession::Open] Open called");

    if (m_sessionHandle != 0 && m_traceHandle == 0)
    {
        ZeroMemory(&m_traceLogFile, sizeof(m_traceLogFile));
        m_traceLogFile.LoggerName = m_session.name;
        m_traceLogFile.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_RAW_TIMESTAMP;
        m_traceLogFile.EventRecordCallback = &EventRecordCallback;
        m_traceLogFile.Context = pConsumer;

        m_traceHandle = ::OpenTrace(&m_traceLogFile);
        result = (m_traceHandle != 0);
        DD_PRINT(LogLevel::Verbose, "[TraceSession::Open] Trace session open: %s", result ? "Successful" : "Unsuccessful");

    }
    return result;
}

//-----------------------------------------------------------------------------
/// Process all new incoming events from the trace session.
/// \returns True when processing the trace was successful, and false when it fails.
//-----------------------------------------------------------------------------
bool TraceSession::Process()
{
    bool result = false;
    if (m_sessionHandle != 0 && m_traceHandle != 0)
    {
        DD_PRINT(LogLevel::Verbose, "[TraceSession::Process] Process trace starting");
        ULONG status = ProcessTrace(&m_traceHandle, 1, nullptr, nullptr);
        DD_PRINT(LogLevel::Verbose, "[TraceSession::Process] Process trace finished");
        result = (status == ERROR_SUCCESS);
    }
    DD_PRINT(LogLevel::Verbose,
             "[TraceSession::Process] Trace session processing: %s",
             result ? "Successful" : "Unsuccessful");
    return result;
}

//-----------------------------------------------------------------------------
/// Close an active trace session.
/// \returns True when the trace was closed successfully, and false if it failed.
//-----------------------------------------------------------------------------
bool TraceSession::Close()
{
    bool result = false;
    DD_PRINT(LogLevel::Verbose, "[TraceSession::Close] Trace session closing");
    if (m_traceHandle != 0)
    {
        // We should always have a valid session when we stop the trace.
        // If we don't, it probably means someone stopped the trace session before closing the trace.
        DD_ASSERT(m_sessionHandle != 0);

        ULONG status = ::CloseTrace(m_traceHandle);
        result = (status == ERROR_SUCCESS || status == ERROR_CTX_CLOSE_PENDING);
        DD_ASSERT(result);
        DD_PRINT(LogLevel::Verbose, "[TraceSession::Close] Trace session close: %u", status);
        m_traceHandle = 0;
    }
    return result;
}

//-----------------------------------------------------------------------------
/// Disable a trace provider by GUID.
/// \param The GUID for the provider to disable.
/// \returns True when the provider was disabled successfully, and false if it failed.
//-----------------------------------------------------------------------------
bool TraceSession::DisableProvider(const GUID& providerId)
{
    bool result = false;
    if (m_sessionHandle != 0)
    {
        ULONG status = EnableTraceEx2(m_sessionHandle, &providerId, EVENT_CONTROL_CODE_DISABLE_PROVIDER, 0, 0, 0, 0, nullptr);
        result = (status == ERROR_SUCCESS);
        DD_ASSERT(result);
    }
    return result;
}

//-----------------------------------------------------------------------------
/// Disable a trace provider by GUID.
/// \param The GUID for the provider to disable.
/// \returns True when the provider was disabled successfully, and false if it failed.
//-----------------------------------------------------------------------------
bool TraceSession::DisableProviderByGUID(const LPCWSTR& inGUID)
{
    bool result = false;
    if (m_sessionHandle != 0)
    {
        GUID providerGuid;
        HRESULT converted = CLSIDFromString(inGUID, &providerGuid);

        if (converted == S_OK)
        {
            ULONG status = EnableTraceEx2(m_sessionHandle, &providerGuid, EVENT_CONTROL_CODE_DISABLE_PROVIDER, 0, 0, 0, 0, nullptr);
            result = (status == ERROR_SUCCESS);
            DD_PRINT(LogLevel::Verbose, "[TraceSession::DisableProviderByGUID] Provider disabled: %u",status);
        }
    }
    return result;
}

//-----------------------------------------------------------------------------
/// Stop the tracing sesssion from processing events.
/// \returns True when the trace session was stopped successfully, and false if it failed.
//-----------------------------------------------------------------------------
bool TraceSession::Stop()
{
    bool result = false;
    DD_PRINT(LogLevel::Verbose, "[TraceSession::Stop] Trace session stopping");
    if (m_sessionHandle != 0)
    {
        // We should always close the trace before stopping the session.
        DD_ASSERT(m_traceHandle == 0);

        ULONG status = ControlTrace(m_sessionHandle, m_session.name, &m_session.properties, EVENT_TRACE_CONTROL_STOP);
        result = (status == ERROR_SUCCESS);
        DD_ASSERT(result);
        DD_PRINT(LogLevel::Verbose, "[TraceSession::Stop] Trace session stop: %u", status);
        m_sessionHandle = 0;
    }
    return result;
}

//-----------------------------------------------------------------------------
/// Retrieve the trace session's timestamp frequency.
/// \returns The trace session's timestamp frequency.
//-----------------------------------------------------------------------------
LONGLONG TraceSession::PerfFreq() const
{
    return m_traceLogFile.LogfileHeader.PerfFreq.QuadPart;
}

//-----------------------------------------------------------------------------
/// Queries if ETW is supported on the system.
/// \returns True if ETW is supported.
//-----------------------------------------------------------------------------

bool TraceSession::QueryETWSupport()
{
    bool result = false;

    SessionProperties sessionProperties = {};
    ZeroMemory(&sessionProperties, sizeof(sessionProperties));

    Platform::Strncpy(sessionProperties.name, "ETW Support Query", sizeof(sessionProperties.name));

    char traceSuffix[32];
    Platform::Snprintf(traceSuffix, sizeof(traceSuffix), " - (%u)", DevDriver::Platform::GetProcessId());

    Platform::Strcat(sessionProperties.name, traceSuffix, sizeof(sessionProperties.name));

    sessionProperties.properties.Wnode.BufferSize = sizeof(sessionProperties);

    sessionProperties.properties.Wnode.ClientContext = 1;
    sessionProperties.properties.Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    sessionProperties.properties.LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    sessionProperties.properties.LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
    sessionProperties.properties.LogFileNameOffset = 0;

    TRACEHANDLE sessionHandle;

    // Create the trace session.
    const ULONG startStatus = StartTrace(&sessionHandle, sessionProperties.name, &sessionProperties.properties);
    if (startStatus == ERROR_SUCCESS)
    {
        const ULONG stopStatus = ControlTrace(sessionHandle, sessionProperties.name, &sessionProperties.properties, EVENT_TRACE_CONTROL_STOP);
        if (stopStatus == ERROR_SUCCESS)
        {
            result = true;
        }
        else
        {
            DD_PRINT(LogLevel::Verbose, "[TraceSession::IsETWAvailable] Failed to stop ETW support query trace! Status: %u", stopStatus);
        }
    }
    else if (startStatus != ERROR_ACCESS_DENIED)
    {
        DD_PRINT(LogLevel::Verbose, "[TraceSession::IsETWAvailable] StartTrace in ETW support query returned an unexpected status: %u", startStatus);
    }

    return result;
}
