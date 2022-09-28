/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

using namespace DevDriver;
using namespace EventProtocol;
using namespace Util;

namespace Pal
{
constexpr uint32 EventFlushTimeoutInMs = 10;
constexpr uint32 NumLogProviderEvents  = uint32(OriginationType::Count) + uint32(SeverityLevel::Count);
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
    m_pDbgLoggerDevDriver(nullptr)
{
    PAL_ASSERT(pPlatform != nullptr);
    m_pEventServer = pPlatform->GetEventServer();
}

// =====================================================================================================================
// Establishes connection with the DevDriver event server by registering itself with this server.
Result LogEventProvider::Init(
    DbgLoggerDevDriver* pDbgLoggerDevDriver)
{
    Result result = Result::ErrorInvalidPointer;
    if (m_pEventServer != nullptr)
    {
        result =
            (m_pEventServer->RegisterProvider(this) == DevDriver::Result::Success) ? Result::Success
            : Result::ErrorUnknown;
    }
    m_pDbgLoggerDevDriver = pDbgLoggerDevDriver;

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
    m_pDbgLoggerDevDriver = nullptr;
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
/// Initializes the base class with default severity levels and origination types. These settings will
/// be overridden later if the user changes them from the connected tool.
DbgLoggerDevDriver::DbgLoggerDevDriver(
    IPlatform* pPlatform)
    :
    IDbgLogger(DefaultSeverityLevel, DefaultOriginationTypes),
    m_logEventProvider(pPlatform)
{
}
} //namespace Util
#endif
