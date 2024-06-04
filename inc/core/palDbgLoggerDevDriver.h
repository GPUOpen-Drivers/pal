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
/**
 ***********************************************************************************************************************
 * @file  palDbgLoggerDevDriver.h
 * @brief Defines the logger to log messages to a connected tool through the DevDriver.
 ***********************************************************************************************************************
 */

#pragma once

#if PAL_ENABLE_LOGGING

#include "palDbgLogger.h"
#include "palDbgLogMgr.h"
#include "palDbgLogHelper.h"
#include "protocols/ddEventProvider.h"
#include "pal.h"
#include "palPlatform.h"
#include "palVector.h"

namespace Pal
{
class IPlatform;
class DbgLoggerDevDriver;

/**
************************************************************************************************************************
* @brief     Class to provide debug log messages to the DevDriver.
*            Clients should never create this class and try to use it on its own because this is meant to be
*            used with the DbgLoggerDevDriver to log debug messages out to a connected tool. For more details,
*            please see the DbgLoggerDevDriver class description.
************************************************************************************************************************
*/
class LogEventProvider final : public DevDriver::EventProtocol::BaseEventProvider
{
public:
    /// Constructor
    /// @param [in] pPlatform     Pointer to the IPlatform object used to access the DevDriver event server.
    LogEventProvider(
        IPlatform* pPlatform);

    /// Destructor
    virtual ~LogEventProvider() override
    {
        Destroy();
    }

    /// Establishes a connection to the DevDriver event server by registering itself.
    ///
    /// @returns Success if provider registration is successful, otherwise returns one of the following:
    ///          ErrorInvalidPointer - if event server is a null pointer.
    ///          ErrorUnknown - if provider registration failed.
    Util::Result Init();

    /// Closes the connection to the DevDriver event server by unregistering itself.
    void Destroy();

    /// Logs the incoming raw data with the DevDriver.
    ///
    /// @param [in] severity     Specifies the severity level of the log message
    /// @param [in] source       Specifies the origination type (source) of the log message
    /// @param [in] pClientTag   Indicates the client that logs a message. Only the first 'ClientTagSize'
    ///                          number of characters will be used to identify the client.
    /// @param [in] dataSize     Size of raw data.
    /// @param [in] pData        Pointer to raw data.
    void LogMessage(
        Util::SeverityLevel   severity,
        Util::OriginationType source,
        const char*           pClientTag,
        size_t                dataSize,
        const void*           pData);

    /// @returns the Id of this provider.
    virtual DevDriver::EventProtocol::EventProviderId GetId() const override
    {
        return ProviderId;
    }

    /// @returns the name of this provider
    const char* GetName() const override { return "PalDbgLogEventProvider"; }

    /// @returns a description of this provider.
    virtual const void* GetEventDescriptionData() const override;

    /// @returns description data size.
    virtual uint32 GetEventDescriptionDataSize() const override;

private:
    /// The data output with a LogStringEvent by this provider will consist of this structure followed by the
    /// variable length string
    struct LogStringEventInfo
    {
        uint32 severity;
        uint32 originationType;
        char   pClientTag[Util::ClientTagSize];
        uint32 logStringLength;
    };

    /// LogEventProvider's provider Id is the chain of ASCII codes of each letter in 'LogE'
    static constexpr DevDriver::EventProtocol::EventProviderId ProviderId = 0x4C6F6745; // 'LogE'

    /// Used to pass log messages out to the tool.
    DevDriver::EventProtocol::EventServer* m_pEventServer;

    /// Vector used as a resizable buffer to hold event data
    Util::Vector<uint8, sizeof(LogStringEventInfo) + 256, IPlatform> m_eventData;

    // Event ID for a string log message
    static const uint32 KLogStringEventId = 1;

    PAL_DISALLOW_COPY_AND_ASSIGN(LogEventProvider);
    PAL_DISALLOW_DEFAULT_CTOR(LogEventProvider);
};

/**
************************************************************************************************************************
*@brief     Class to log to Dev Driver. Log messages will be sent to the Dev Driver through the embedded log
*           event provider object. Since this logger works with the DevDriver to send the log messages out
*           to the connected tool, it should only be instantiated when DeveloperMode is enabled.
*
* Clients can use objects of this class for logging as :
* 1. Instantiate this logger, to get :      if (pPlatform->IsDeveloperModeEnabled())
*                                               pDbgLoggerDevDriver = PAL_NEW()(pPlatform)
* 2. Initialize this logger with :          result = pDbgLoggerDevDriver->Init()
* 3. Attach it with :                       if (result == Result::Success) AttachDbgLogger(pDbgLoggerDevDriver)
* 4. When done, detach it with :            DetachDbgLogger(pDbgLoggerDevDriver)
* 5. Delete this logger:                    PAL_SAFE_DELETE(), the m_logEventProvider will be destroyed in its destructor
* ***********************************************************************************************************************
*/
class DbgLoggerDevDriver final : public Util::IDbgLogger
{
public:
    /// Constructor. Initializes the base class with the provided severity level and origination types. These settings will
    /// be overridden later if the user changes them from the connected tool.
    /// @param [in] settings   Settings for this logger
    /// @param [in] pPlatform  Pointer to the IPlatform object used to access the DevDriver event server.
    DbgLoggerDevDriver(
        Util::DbgLogBaseSettings settings,
        IPlatform* pPlatform);

    /// Destructor
    virtual ~DbgLoggerDevDriver() {}

    /// Create a DevDriver logger that clients can use.
    ///
    /// @param [in]  settings             Settings for this logger
    /// @param [in]  pAllocator           Memory allocator
    /// @param [out] ppDbgLoggerDevDriver Pointer to hold the newly created DevDriver logger
    static Result CreateDevDriverLogger(
        Util::DbgLogBaseSettings settings,
        IPlatform*               pPlatform,
        DbgLoggerDevDriver**     ppDbgLoggerDevDriver);

    /// Destroy the DevDriver logger.
    ///
    /// @param [in]  DbgLoggerDevDriver Print logger to destroy
    /// @param [in]  pAllocator         Memory allocator with which it was allocated
    static void DestroyDevDriverLogger(
        DbgLoggerDevDriver* pDbgLoggerDevDriver,
        IPlatform*          pPlatform);

    /// Initializes the LogEventProvider object.
    ///
    /// @returns the code from LogEventProvider initialization.
    Util::Result Init()
    {
        return m_logEventProvider.Init();
    }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 879
    /// Cleanup any data structures used by the logger.
    void Cleanup()
    {
        // Do nothing for now, the m_logEventProvider will be destroyed in its destructor
    }
#endif

protected:
    /// Writes the message to the log event provider
    ///
    /// @param [in] severity     Specifies the severity level of the log message
    /// @param [in] source       Specifies the origination type (source) of the log message
    /// @param [in] pClientTag   Indicates the client that logs a message. Only the first 'ClientTagSize'
    ///                          number of characters will be used to identify the client.
    /// @param [in] dataSize     Size of raw data.
    /// @param [in] pData        Pointer to raw data.
    virtual void WriteMessage(
        Util::SeverityLevel   severity,
        Util::OriginationType source,
        const char*           pClientTag,
        size_t                dataSize,
        const void*           pData) override
    {
        // Just pass the message through to the event provider
        m_logEventProvider.LogMessage(severity, source, pClientTag, dataSize, pData);
    }

private:
    LogEventProvider m_logEventProvider; ///< LogEventProvider object used to communicate with the DevDriver.
};
} //namespace Util
#endif
