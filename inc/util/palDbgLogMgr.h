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
 * @file  palDbgLogMgr.h
 * @brief Defines the PAL debug log manager class that manages various debug loggers capable of logging debug messages
 *        to various destinations.
 ***********************************************************************************************************************
 */

#pragma once

#include "palDbgLogHelper.h"
#include "palUtil.h"
#include "palThread.h"
#include "palSysMemory.h"
#include "palMutex.h"
#include "palIntrusiveList.h"

#include "stdarg.h"

namespace Util
{
class IDbgLogger;
class DbgLogMgr;

/// A global DbgLogMgr object available for use by any driver component from start to end of the application.
extern DbgLogMgr g_dbgLogMgr;

/**
************************************************************************************************************************
* @brief     A class to manage various debug loggers.
*
*            The DbgLogMgr is created during driver load time and remains in existence throughout the life of the
*            application. It is a global object available for any component's use. Its life span enables logging
*            of messages right from the start to end of the application.
*
*            The DbgLogMgr maintains a list of debug loggers and when a message arrives for logging, the manager calls
*            the LogMessage() function of all the loggers in this list. Each logger then takes care of logging the
*            message to its destination.
*
*            Expected usage is for a client to create a debug logger for a particular destination and attach it to this
*            list with AttachDbgLogger(). When this logger is no longer needed, the client detaches it with
*            DetachDbgLogger() and destroys the logger.  It is the responsibility of the client to create and destroy
*            the loggers it uses.
************************************************************************************************************************
*/
class DbgLogMgr
{
    /// A useful shorthand for Intrusive list of IDbgLogger(s)
    typedef IntrusiveList<IDbgLogger> DbgLoggersList;

public:
    /// Debug Log Manager constructor.
    DbgLogMgr();

    /// Debug Log Manager destructor.
    ~DbgLogMgr();

    /// Attaches a debug logger for logging debug messages
    ///
    /// @param [in]  dbgLogger   logger to attach to the logger list
    /// @returns Success if no error while attaching. Otherwise returns one of the following:
    ///          ErrorInvalidPointer if incoming pDbgLogger is a null pointer
    ///          ErrorUnknown for all other failures.
    Result AttachDbgLogger(
        IDbgLogger* pDbgLogger);

    /// Detaches a debug logger
    ///
    /// @param [in] dbgLogger   logger to detach from the logger list. Caller responsible for logger's destruction.
    /// @returns Success if no error while detaching. Otherwise returns one of the following:
    ///          ErrorInvalidPointer if incoming pDbgLogger is a null pointer
    ///          ErrorUnknown for all other failures.
    Result DetachDbgLogger(
        IDbgLogger* pDbgLogger);

    /// Calls LogMessageInternal() which calls the LogMessage() functions of all attached debug loggers in its list.
    /// Individual debug loggers will log the incoming message if they are interested in it.
    /// This variant of LogMessage() logs the raw data buffer to a destination.
    ///
    /// @param [in] severity     Specifies the severity level of the log message
    /// @param [in] source       Specifies the origination type (source) of the log message
    /// @param [in] pClientTag   Indicates the client that logs a message. Only the first 'ClientTagSize'
    ///                          number of characters will be used to identify the client.
    /// @param [in] dataSize     Size of raw data.
    /// @param [in] pData        Pointer to raw data.
    void LogMessage(
        SeverityLevel   severity,
        OriginationType source,
        const char*     pClientTag,
        size_t          dataSize,
        const void*     pData)
    {
        LogMessageInternal(severity, source, pClientTag, dataSize, pData);
    }

    /// Calls the LogMessage() functions of all attached debug loggers in its list.
    /// Individual debug loggers will log the incoming message if they are interested in it.
    /// This variant of LogMessage() logs a text string to a destination.
    ///
    /// @param [in] severity     Specifies the severity level of the log message
    /// @param [in] source       Specifies the origination type (source) of the log message
    /// @param [in] pClientTag   Indicates the client that logs a message. Only the first 'ClientTagSize'
    ///                          number of characters will be used to identify the client.
    /// @param [in] pFormat      Format string for the log message.
    /// @param [in] ...          Variable arguments that correspond to the format string.
    void LogMessage(
        SeverityLevel   severity,
        OriginationType source,
        const char*     pClientTag,
        const char*     pFormat,
        ...)
    {
        va_list args;
        va_start(args, pFormat);
        LogMessage(severity, source, pClientTag, pFormat, args);
        va_end(args);
    }

    /// Calls LogMessageInternal() which calls the LogMessage() functions of all attached debug loggers in its list.
    /// Individual debug loggers will log the incoming message if they are interested in it.
    /// This variant of LogMessage() logs a text string to a destination.
    ///
    /// This is made public so that Util::DbgLog() can call this function. On its own, it
    /// is not very useful to PAL clients.
    ///
    /// @param [in] severity     Specifies the severity level of the log message
    /// @param [in] source       Specifies the origination type (source) of the log message
    /// @param [in] pClientTag   Indicates the client that logs a message. Only the first 'ClientTagSize'
    ///                          number of characters will be used to identify the client.
    /// @param [in] pFormat      Format string for the log message.
    /// @param [in] args         Variable arguments list
    void LogMessage(
        SeverityLevel   severity,
        OriginationType source,
        const char*     pClientTag,
        const char*     pFormat,
        va_list         args)
    {
        LogMessageInternal(severity, source, pClientTag, pFormat, args);
    }

    /// Get debug logging state. Clients can use this info to decide whether to
    /// create debug loggers or not.
    /// @returns debug logging state.
    bool GetLoggingEnabled()
    {
        return !m_dbgLoggersList.IsEmpty();
    }

    /// DbgLogMgr may have internal errors that can be queried through this method.
    /// For example: m_error = true when CreateThreadLocalKey() returns error.
    /// Clients can query for this error and decide whether to use the DbgLogMgr object
    /// or not.
    /// @returns the state of internal error as true or false.
    bool HasError()
    {
        return m_error;
    }

    /// Checks to see if an incoming message should be accepted according to its severity and source.
    /// Messages will reach the loggers only if they pass through this check.
    ///
    /// @param [in] severity     Specifies the log message's severity level.
    /// @param [in] source       Specifies the log message's origination type (source).
    /// @returns true if log message's severity is above acceptable severity level and source
    ///          is in its mask. Otherwise, returns false.
    bool AcceptMessage(
        SeverityLevel   severity,
        OriginationType source)
    {
        return Util::AcceptMessage(severity,
                                   source,
                                   m_dbgLogBaseSettings.severityLevel,
                                   m_dbgLogBaseSettings.origTypeMask);
    }

    /// Return debug log manager's base settings.
    const DbgLogBaseSettings& GetDbgLogBaseSettings()
    {
        return m_dbgLogBaseSettings;
    }

    /// Set debug log manager's base settings to the incoming values.
    ///
    /// @param [in] dbgLogBaseSettings     Base settings that will replace current values.
    void SetDbgLogBaseSettings(
        const DbgLogBaseSettings& dbgLogBaseSettings)
    {
        m_dbgLogBaseSettings.severityLevel = dbgLogBaseSettings.severityLevel;
        m_dbgLogBaseSettings.origTypeMask  = dbgLogBaseSettings.origTypeMask;
    }

private:
    /// A variadic template function having common code to check for thread safety (reentry guard and RWLock)
    /// before calling each logger's LogMessage(). All public LogMessage() variants will call this internal
    /// helper so that there is no code duplication.
    ///
    /// Individual debug loggers will log the incoming message if they are interested in it.
    ///
    /// @param [in] args         Variable arguments typename
    template <typename... Args>
    void LogMessageInternal(Args... args);

    bool               m_error;              ///< Keeps track of internal errors.  Clients can query for this
                                             ///< and decide whether to use the DbgLogMgr object or not.
    ThreadLocalKey     m_reentryGuardKey;    ///< Thread-local key for reentry guard to protect
                                             ///< LogMessage() from re-entry by same thread.
    RWLock             m_dbgLoggersLock;     ///< Serialize access to DbgLoggers list.
    DbgLoggersList     m_dbgLoggersList;     ///< List of debug loggers.
    DbgLogBaseSettings m_dbgLogBaseSettings; ///< struct containing base severity level and orig type mask

    PAL_DISALLOW_COPY_AND_ASSIGN(DbgLogMgr);
};
} // Util
