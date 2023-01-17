/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "protocols/ddEventProvider.h"
#include "pal.h"

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
    /// @param [in] pDbgLoggerDevDriver      Pointer to the DevDriver logger that uses this event provider. This is
    ///                                      used to pass the incoming SeverityLevel and OriginationType set from a
    ///                                      connected tool to the DevDriver logger. The DevDriver logger uses this
    ///                                      information to filter the debug messages.
    /// @returns Success if provider registration is successful, otherwise returns one of the following:
    ///          ErrorInvalidPointer - if event server is a null pointer.
    ///          ErrorUnknown - if provider registration failed.
    Util::Result Init(DbgLoggerDevDriver* pDbgLoggerDevDriver);

    /// Closes the connection to the DevDriver event server by unregistering itself.
    void Destroy();

    /// Logs the incoming raw data with the DevDriver.
    ///
    /// @param [in] eventId      Bit position of debug log message's OriginationType. The DevDriver maintains a bit
    ///                          mask of OriginationType and SeverityLevel packed into one uint32 word. It checks
    ///                          the bit at eventId in this word and if set, logs the incoming message out to the
    ///                          connected tool.
    /// @param [in] dataSize     Size of incoming raw data.
    /// @param [in] pData        Pointer to the raw data that needs to be logged.
    void LogMessage(
        uint32      eventId,
        size_t      dataSize,
        const void* pData)
    {
        WriteEvent(eventId, pData, dataSize);
    }

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
    /// LogEventProvider's provider Id is the chain of ASCII codes of each letter in 'LogE'
    static constexpr DevDriver::EventProtocol::EventProviderId ProviderId = 0x4C6F6745; // 'LogE'

    DevDriver::EventProtocol::EventServer* m_pEventServer;        ///< used to pass log messages out to the tool.
    DbgLoggerDevDriver*                    m_pDbgLoggerDevDriver; ///< Logger that contains this provider.

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
* 5. De-initialize with :                   pDbgLoggerDevDriver->Cleanup();
* 6. Delete this logger:                    PAL_SAFE_DELETE()
* ***********************************************************************************************************************
*/
class DbgLoggerDevDriver final : public Util::IDbgLogger
{
public:
    /// Constructor. Initializes the base class with all severity levels and origination types. These settings will
    /// be overridden later if the user changes them from the connected tool.
    /// @param [in] pPlatform     Pointer to the IPlatform object used to access the DevDriver event server.
    DbgLoggerDevDriver(
        IPlatform* pPlatform);

    /// Destructor
    virtual ~DbgLoggerDevDriver()
    {
        Cleanup();
    }

    /// Initializes the LogEventProvider object.
    ///
    /// @returns the code from LogEventProvider initialization.
    Util::Result Init()
    {
        return m_logEventProvider.Init(this);
    }

    /// Cleanup any data structures used by the logger.
    void Cleanup()
    {
        m_logEventProvider.Destroy();
    }

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
        m_logEventProvider.LogMessage(uint32(source), dataSize, pData);
    }

private:
    LogEventProvider m_logEventProvider; ///< LogEventProvider object used to communicate with the DevDriver.
};
} //namespace Util
#endif
