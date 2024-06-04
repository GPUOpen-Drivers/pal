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

#if PAL_ENABLE_LOGGING
#include "palDbgLoggerDevDriver.h"
#include "core/devDriverUtil.h"
#include "palVectorImpl.h"

using namespace DevDriver;
using namespace EventProtocol;
using namespace Util;

namespace Pal
{
constexpr uint32 EventFlushTimeoutInMs = 10;
constexpr uint32 NumLogProviderEvents  = 1;
constexpr char   EventDescription[]    = "Generic driver log messages";

/// Some defaults for this logger
constexpr SeverityLevel DefaultSeverityLevel = SeverityLevel::Debug; ///< Least restrictive, accepts all levels
constexpr uint32 DefaultOriginationTypes     = AllOriginationTypes;  ///< Accepts msgs from all sources

// =====================================================================================================================
LogEventProvider::LogEventProvider(
    IPlatform* pPlatform)
    :
    BaseEventProvider(
        { pPlatform, DevDriverAlloc, DevDriverFree },
        NumLogProviderEvents,
        EventFlushTimeoutInMs),
    m_pEventServer(nullptr),
    m_eventData(pPlatform)
{
    PAL_ASSERT(pPlatform != nullptr);
    m_pEventServer = pPlatform->GetEventServer();
}

// =====================================================================================================================
// Establishes connection with the DevDriver event server by registering itself with this server.
Result LogEventProvider::Init()
{
    Result result = Result::ErrorInvalidPointer;
    if (m_pEventServer != nullptr)
    {
        result =
            (m_pEventServer->RegisterProvider(this) == DevDriver::Result::Success) ? Result::Success
            : Result::ErrorUnknown;
    }

    return result;
}

// =====================================================================================================================
// Closes connection with the DevDriver event server by unregistering itself.
void LogEventProvider::Destroy()
{
    if (m_pEventServer != nullptr)
    {
        DD_UNHANDLED_RESULT(m_pEventServer->UnregisterProvider(this));
    }
}

// =====================================================================================================================
// Returns a description of this provider.
const void* LogEventProvider::GetEventDescriptionData() const
{
    return EventDescription;
}

// =====================================================================================================================
// Returns the event description data size.
uint32 LogEventProvider::GetEventDescriptionDataSize() const
{
    return sizeof(EventDescription);
}

// =====================================================================================================================
// Logs a message through the DevDriver logger
void LogEventProvider::LogMessage(
    Util::SeverityLevel   severity,
    Util::OriginationType source,
    const char*           pClientTag,
    size_t                dataSize,
    const void*           pData)
{
    // Currently, the only supported event is a string log message, so we can just directly write that event
    LogStringEventInfo eventInfo = {};
    eventInfo.severity = static_cast<uint32>(severity);
    eventInfo.originationType = static_cast<uint32>(source);
    Util::Strncpy(eventInfo.pClientTag, pClientTag, ClientTagSize);

    const size_t bufferSize = sizeof(LogStringEventInfo) + dataSize;
    const Result result = m_eventData.Resize(static_cast<uint32>(bufferSize), 0);

    if (result == Result::Success)
    {
        void* pBufferPtr = static_cast<void*>(m_eventData.Data());
        memcpy(pBufferPtr, &eventInfo, sizeof(eventInfo));
        pBufferPtr = Util::VoidPtrInc(pBufferPtr, sizeof(eventInfo));
        memcpy(pBufferPtr, pData, dataSize);
        WriteEvent(KLogStringEventId, static_cast<const void*>(m_eventData.Data()), bufferSize);
    }
    else
    {
        // If we can't allocate space for the data then we'll just assert and drop the message
        PAL_ASSERT_ALWAYS();
    }
}

// =====================================================================================================================
// Creates a DevDriver logger
Result DbgLoggerDevDriver::CreateDevDriverLogger(
    Util::DbgLogBaseSettings settings,
    IPlatform*               pPlatform,
    DbgLoggerDevDriver**     ppDbgLoggerDevDriver)
{
    Result result = Result::ErrorOutOfMemory;
    DbgLoggerDevDriver* pDbgLoggerDevDriver = PAL_NEW(DbgLoggerDevDriver, pPlatform, AllocInternal)
                                                     (settings, pPlatform);
    if (pDbgLoggerDevDriver != nullptr)
    {
        result = pDbgLoggerDevDriver->Init();

        if (result == Result::Success)
        {
            g_dbgLogMgr.AttachDbgLogger(pDbgLoggerDevDriver);
            (*ppDbgLoggerDevDriver) = pDbgLoggerDevDriver;
        }
        else
        {
            PAL_DELETE(pDbgLoggerDevDriver, pPlatform);
        }
    }

    return result;
}

// =====================================================================================================================
/// Destroy the DevDriver logger.
void DbgLoggerDevDriver::DestroyDevDriverLogger(
    DbgLoggerDevDriver* pDbgLoggerDevDriver,
    IPlatform*          pPlatform)
{
    if (pDbgLoggerDevDriver != nullptr)
    {
        g_dbgLogMgr.DetachDbgLogger(pDbgLoggerDevDriver);
        PAL_SAFE_DELETE(pDbgLoggerDevDriver, pPlatform);
    }
}

// =====================================================================================================================
/// Initializes the base class with default severity levels and origination types. These settings will
/// be overridden later if the user changes them from the connected tool.
DbgLoggerDevDriver::DbgLoggerDevDriver(
    Util::DbgLogBaseSettings settings,
    IPlatform*               pPlatform)
    :
    IDbgLogger(settings.severityLevel, settings.origTypeMask),
    m_logEventProvider(pPlatform)
{
}
} //namespace Util
#endif
