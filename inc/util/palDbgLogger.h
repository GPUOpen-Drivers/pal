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
/**
 ***********************************************************************************************************************
 * @file  palDbgLogger.h
 * @brief Defines various debug loggers that are derived from the base IDbgLogger.
 ***********************************************************************************************************************
 */

#pragma once

#if PAL_ENABLE_LOGGING

#include "palDbgLogHelper.h"
#include "palFile.h"
#include "palIntrusiveList.h"

namespace Util
{
static constexpr uint32 DefaultMsgSize = 1024;  ///< default max size for the main message

/// By default, the final formatted message consists of: "<severity>:<main message>\r\n"
/// Max size for severity = 8, which is strlen("Critical")
/// So, default final msg size is:
/// 8 + 1(for :) + DefaultMsgSize + 2(for \r\t) + 1(for null termination) = DefaultMsgSize + 12
/// Individual loggers may override these defaults when implementing more sophisticated formatting scheme.
static constexpr uint32 DefaultFinalMsgSize = DefaultMsgSize + 12;

/// Provides simple formatting of the log message of the form: "<severity level>:<main msg>\r\t".
/// The main msg conforms to a max size of 'msgSize' beyond which the main message will be truncated.
/// It assumes that the input msg string is null terminated and formats only if there is enough space
/// in the input buffer. Users can call this function again with larger buffer space if formatting
/// is aborted due to lack of space.
///
/// Debug Loggers can use this or implemenmt more sophisticated formatting per their needs.
///
/// @param [in] pOutputMsg      Null terminated buffer to hold the formatted message.
/// @param [in] outputMsgSize   Size of message buffer.
/// @param [in] severity        Severity level of the log message that will be added as a prefix to the message.
/// @param [in] pFormat         Format string for the log message.
/// @param [in] args            Variable arguments list.
/// @returns Success if message was formatted successfully. Otherwise, returns 'ErrorInvalidMemorySize'
///          if user didn't provide a large enough buffer to fit the formatted string.
extern Result FormatMessageSimple(
    char*           pOutputMsg,
    uint32          outputMsgSize,
    SeverityLevel   severity,
    const char*     pFormat,
    va_list         args);

/**
************************************************************************************************************************
* @interface IDbgLogger
* @brief     Interface representing a debug logger.
*
* Base class for debug loggers. Derived classes are one for each of the logging destination types
*
* + Provides common LogMessage() interfaces.
************************************************************************************************************************
*/
class IDbgLogger
{
    /// A useful shorthand for Intrusive list of IDbgLogger(s)
    typedef IntrusiveList<IDbgLogger> DbgLoggersList;

public:
    /// Logs a buffer to a destination if this logger is interested in the input message.
    ///
    /// @param [in] severity     Specifies the severity level of the log message
    /// @param [in] source       Specifies the origination type (source) of the log message
    /// @param [in] pClientTag   Indicates the client that logs a message. Only the first 'ClientTagSize'
    ///                          number of characters will be used to identify the client.
    /// @param [in] dataSize     Size of raw data.
    /// @param [in] pData        Pointer to raw data.
    virtual void LogMessage(
        SeverityLevel   severity,
        OriginationType source,
        const char*     pClientTag,
        size_t          dataSize,
        const void*     pData) = 0;

    /// Logs a text string to a destination if this logger is interested in the input message.
    ///
    /// @param [in] severity     Specifies the severity level of the log message
    /// @param [in] source       Specifies the origination type (source) of the log message
    /// @param [in] pClientTag   Indicates the client that logs a message. Only the first 'ClientTagSize'
    ///                          number of characters will be used to identify the client.
    /// @param [in] pFormat      Format string for the log message.
    /// @param [in] ...          Variable arguments that correspond to the format string.
    virtual void LogMessage(
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

    /// Logs a text string to a destination if this logger is interested in the input message.
    ///
    /// @param [in] severity     Specifies the severity level of the log message
    /// @param [in] source       Specifies the origination type (source) of the log message
    /// @param [in] pClientTag   Indicates the client that logs a message. Only the first 'ClientTagSize'
    ///                          number of characters will be used to identify the client.
    /// @param [in] pFormat      Format string for the log message.
    /// @param [in] args         Variable arguments list
    virtual void LogMessage(
        SeverityLevel   severity,
        OriginationType source,
        const char*     pClientTag,
        const char*     pFormat,
        va_list         args) = 0;

    /// Returns the pointer to debug loggers list node containing this logger.
    DbgLoggersList::Node* ListNode()
    {
        return &m_listNode;
    }

protected:
    /// Constructor that sets the cutoff severity and origination mask to the incoming values.
    ///
    /// @param [in] severity     Specifies this logger's acceptable severity level.
    /// @param [in] sourceMask   Specifies this logger's acceptable origination types as a bit mask.
    IDbgLogger(
        SeverityLevel   severity,
        uint32          sourceMask)
        :
        m_cutoffSeverityLevel(severity),
        m_originationTypeMask(sourceMask),
        m_listNode(this)
    {}

    ///  Destructor
    virtual ~IDbgLogger() {}

    /// Filters the incoming msg according to its severity and source. Messages will only
    /// get logged if they pass through this filter.
    ///
    /// @param [in] severity     Specifies the log msg's severity level.
    /// @param [in] source       Specifies the log msg's origination type (source).
    /// @returns true if log msg's severity is above the cutoff level and source is in this logger's mask.
    ///          Otherwise, returns false.
    bool FilterMessage(
        SeverityLevel   severity,
        OriginationType source)
    {
        const uint32 sourceFlag = (1ul << uint32(source));
        return ((severity >= m_cutoffSeverityLevel) && (TestAnyFlagSet(m_originationTypeMask, sourceFlag)));
    }

    SeverityLevel        m_cutoffSeverityLevel;   ///< All messages below this SeverityLevel get filtered out.
    uint32               m_originationTypeMask;   ///< A mask of acceptable origination types
    DbgLoggersList::Node m_listNode;              ///< A node in the debug loggers list
};

/**
************************************************************************************************************************
* @brief     Class to log to a file. Log messages will be buffered and dumped to a file.
*
* Clients can use objects of this class for logging as:
* 1. Instantiate this logger, to get:       pDbgLoggerFile = PAL_NEW()(severityLevel, maskOfOriginationTypes)
* 2. Initialize this logger with:           pDbgLoggerFile->Init("someFileName")
* 3. Attach it with:                        AttachDbgLogger(pDbgLoggerFile)
* 4. When done, detach it with:             DetachDbgLogger(pDbgLoggerFile)
* 5. De-initialize with:                    pDbgLoggerFile->Cleanup();
* 6. Delete this logger:                    PAL_SAFE_DELETE()
************************************************************************************************************************
*/
class DbgLoggerFile final : public IDbgLogger
{
public:
    /// Constructor
    /// @param [in] severity     Specifies this logger's acceptable severity level.
    /// @param [in] sourceMask   Specifies this logger's acceptable origination types as a bit mask.
    DbgLoggerFile(
        SeverityLevel   severity,
        uint32          sourceMask)
        :
        IDbgLogger(severity, sourceMask),
        m_file()
    {}

    /// Destructor
    virtual ~DbgLoggerFile() {}

    /// Initialize any data structures needed by the file logger.
    ///
    /// @param [in]  pFileName      Name of the file where the messages will be logged
    /// @param [in]  fileAccessMask Mask of file access modes
    /// @returns Success if successful, otherwise returns one of the following codes:
    ///          1. ErrorInvalidFlags - if fileAccessMask contains a file read mode
    ///          2. An error code as returned by the file Open() operation.
    Result Init(
        const char* pFileName,
        uint32      fileAccessMask);

    /// Cleanup any data structures used by the file logger.
    void Cleanup();

    /// Logs a buffer to a destination if this logger is interested in the input message.
    ///
    /// @param [in] severity     Specifies the severity level of the log message
    /// @param [in] source       Specifies the origination type (source) of the log message
    /// @param [in] pClientTag   Indicates the client that logs a message. Only the first 'ClientTagSize'
    ///                          number of characters will be used to identify the client.
    /// @param [in] dataSize     Size of raw data.
    /// @param [in] pData        Pointer to raw data.
    virtual void LogMessage(
        SeverityLevel   severity,
        OriginationType source,
        const char*     pClientTag,
        size_t          dataSize,
        const void*     pData) override;

    /// Logs a text string to a destination if this logger is interested in the input message.
    ///
    /// @param [in] severity     Specifies the severity level of the log message
    /// @param [in] source       Specifies the origination type (source) of the log message
    /// @param [in] pClientTag   Indicates the client that logs a message. Only the first 'ClientTagSize'
    ///                          number of characters will be used to identify the client.
    /// @param [in] pFormat      Format string for the log message.
    /// @param [in] args         Variable arguments list
    virtual void LogMessage(
        SeverityLevel   severity,
        OriginationType source,
        const char*     pClientTag,
        const char*     pFormat,
        va_list         args) override;

private:
    File m_file; ///< File where debug messages will be logged.
};
} //namespace Util
#endif
