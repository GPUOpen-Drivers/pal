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

/// DbgLoggerFile related enums used by DbgLoggerFileSettings struct
enum FileSettings : uint32
{
    LogToDisk     = 0x01,  ///< Write debug messages to a disk file
    LogToTerminal = 0x02,  ///< Write debug messages to terminal or debuger's output window
    AddPid        = 0x04,  ///< Add PID to file name
    AddPname      = 0x08,  ///< Add process name to file name
    AddLibName    = 0x10,  ///< Add Library name to file name
    ForceFlush    = 0x20,  ///< Force a flush after every write
};

constexpr uint32 AllFileSettings = FileSettings::LogToDisk     |
                                   FileSettings::LogToTerminal |
                                   FileSettings::AddPid        |
                                   FileSettings::AddPname      |
                                   FileSettings::AddLibName    |
                                   FileSettings::ForceFlush;

/// Structure of file debug logger settings
struct DbgLoggerFileSettings : public DbgLogBaseSettings
{
    uint32       fileSettingsFlags; ///< Mask of file settings as defined above in FileSettings
    uint32       fileAccessFlags;   ///< Mask of file access modes as defined in Util::FileAccessMode
    const char*  pLogDirectory;     ///< Directory where log files will be written
};

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

/// Creates a complete file name for debug logging by adding library name, process name,
/// and pid to the base name if needed. Returns a truncated file name if incoming file
/// name string size is not enough.
///
/// @param [in] pFileName      String to hold the final name.
/// @param [in] fileNameSize   Size of the file name string.
/// @param [in] settings       File settings used to configure the file name.
/// @param [in] pBaseFileName  Base file name to which process name, library name, etc. will be added.
/// @returns Success if log file name creation succeeded.
///          Otherwise, returns the error code from 'GetExecutableName' or 'MkDirRecursively'
extern Result CreateLogFileName(
    char*                        pFileName,
    uint32                       fileNameSize,
    const DbgLoggerFileSettings& settings,
    const char*                  pBaseFileName);

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
    void LogMessage(
        SeverityLevel   severity,
        OriginationType source,
        const char*     pClientTag,
        size_t          dataSize,
        const void*     pData)
    {
        if (AcceptMessage(severity, source))
        {
            WriteMessage(severity, source, pClientTag, dataSize, pData);
        }
    }

    /// Logs a text string to a destination if this logger is interested in the input message.
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

    /// Logs a text string to a destination if this logger is interested in the input message.
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
        if (AcceptMessage(severity, source))
        {
            char outputMsg[DefaultFinalMsgSize];
            outputMsg[0] = '\0';
            FormatMessageSimple(outputMsg, DefaultFinalMsgSize, severity, pFormat, args);
            // If the msg was truncated, just accept it as is. We are not going to format
            // again with a bigger buffer at this time.
            WriteMessage(severity, source, pClientTag, (strlen(outputMsg) * sizeof(char)), outputMsg);
        }
    }

    /// Returns the pointer to debug loggers list node containing this logger.
    DbgLoggersList::Node* ListNode()
    {
        return &m_listNode;
    }

    /// Returns cutoff severity level.
    SeverityLevel GetCutoffSeverityLevel()
    {
        return m_cutoffSeverityLevel;
    }

    ///  Returns origination type mask.
    uint32 GetOriginationTypeMask()
    {
        return m_originationTypeMask;
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

    /// Checks to see if an incoming msg should be accpeted according to its severity and source.
    /// Messages will only get logged if they pass through this check.
    ///
    /// @param [in] severity     Specifies the log msg's severity level.
    /// @param [in] source       Specifies the log msg's origination type (source).
    /// @returns true if log msg's severity is above the cutoff level and source is in this logger's mask.
    ///          Otherwise, returns false.
    bool AcceptMessage(
        SeverityLevel   severity,
        OriginationType source)
    {
        return Util::AcceptMessage(severity, source, m_cutoffSeverityLevel, m_originationTypeMask);
    }

    /// Writes the message to a destination. Each derived class implements this method
    /// and knows where and how to write the message.
    ///
    /// @param [in] severity     Specifies the severity level of the log message
    /// @param [in] source       Specifies the origination type (source) of the log message
    /// @param [in] pClientTag   Indicates the client that logs a message. Only the first 'ClientTagSize'
    ///                          number of characters will be used to identify the client.
    /// @param [in] dataSize     Size of raw data.
    /// @param [in] pData        Pointer to raw data.
    virtual void WriteMessage(
        SeverityLevel   severity,
        OriginationType source,
        const char*     pClientTag,
        size_t          dataSize,
        const void*     pData) = 0;

    SeverityLevel        m_cutoffSeverityLevel;   ///< All messages below this SeverityLevel get filtered out.
    uint32               m_originationTypeMask;   ///< A mask of acceptable origination types
    DbgLoggersList::Node m_listNode;              ///< A node in the debug loggers list
};

/**
************************************************************************************************************************
* @brief     Class to log to a file. Log messages will be dumped to a file.
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
        uint32          sourceMask,
        bool            forceFlush)
        :
        IDbgLogger(severity, sourceMask),
        m_file(),
        m_forceFlush(forceFlush)
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
    void Cleanup()
    {
        m_file.Close();
    }

    /// Create a file logger that clients can use.
    ///
    /// @param [in]  settings          File settings used to configure the file logger
    /// @param [in]  pBaseFileName     Base file name to which process name, lib. name, etc. may get added
    /// @param [in]  pAllocator        Memory allocator
    /// @param [out] ppDbgLoggerFile   Pointer to hold the newly created file logger
    /// @returns Success if file logger got created. Otherwise, returns one of the following:
    ///          ErrorOutOfMemory - if memory allocation failed
    ///          Error code as returned by CreateLogFileName() or logger initialization.
    template <typename Allocator>
    static Result CreateFileLogger(
        const DbgLoggerFileSettings& settings,
        const char*                  pBaseFileName,
        Allocator*                   pAllocator,
        DbgLoggerFile**              ppDbgLoggerFile)
    {
        char fileName[MaxFileNameStrLen];
        Result result = CreateLogFileName(fileName, MaxFileNameStrLen, settings, pBaseFileName);
        if (result == Result::Success)
        {
            result          = Result::ErrorOutOfMemory;
            bool forceFlush = TestAnyFlagSet(settings.fileSettingsFlags, FileSettings::ForceFlush);
            DbgLoggerFile* pDbgLogger = PAL_NEW(DbgLoggerFile, pAllocator, AllocInternal)
                                               (settings.severityLevel, settings.origTypeMask, forceFlush);
            if (pDbgLogger != nullptr)
            {
                result = pDbgLogger->Init(fileName, settings.fileAccessFlags);
                if (result == Result::Success)
                {
                    g_dbgLogMgr.AttachDbgLogger(pDbgLogger);
                    *ppDbgLoggerFile = pDbgLogger;
                }
                else
                {
                    // Initialization failed. So no point trying to use this logger.
                    // Delete and set it to nullptr.
                    PAL_SAFE_DELETE(pDbgLogger, pAllocator);
                }
            }
        }
        return result;
    }

    /// Destroy the file logger.
    ///
    /// @param [in]  pDbgLoggerFile  File logger to destroy
    /// @param [in]  pAllocator      Memory allocator with which it was allocated
    template <typename Allocator>
    static void DestroyFileLogger(
        DbgLoggerFile* pDbgLoggerFile,
        Allocator*     pAllocator)
    {
        if (pDbgLoggerFile)
        {
            g_dbgLogMgr.DetachDbgLogger(pDbgLoggerFile);
            pDbgLoggerFile->Cleanup();
            PAL_SAFE_DELETE(pDbgLoggerFile, pAllocator);
        }
    }

protected:
    /// Writes the message to the file.
    ///
    /// @param [in] severity     Specifies the severity level of the log message
    /// @param [in] source       Specifies the origination type (source) of the log message
    /// @param [in] pClientTag   Indicates the client that logs a message. Only the first 'ClientTagSize'
    ///                          number of characters will be used to identify the client.
    /// @param [in] dataSize     Size of raw data.
    /// @param [in] pData        Pointer to raw data.
    virtual void WriteMessage(
        SeverityLevel   severity,
        OriginationType source,
        const char*     pClientTag,
        size_t          dataSize,
        const void*     pData)
    {
        m_file.Write(pData, dataSize);
        if (m_forceFlush)
        {
            m_file.Flush();
        }
    }

private:
    File m_file;       ///< File where debug messages will be logged.
    bool m_forceFlush; ///< Force a flush after every write
};

/**
************************************************************************************************************************
* @brief     Class to print log messages to an output window.
*
* Clients can use objects of this class for logging as:
* 1. Instantiate this logger, to get:       DbgLoggerPrint = PAL_NEW()(severityLevel, maskOfOriginationTypes)
* 2. Attach it with:                        AttachDbgLogger(DbgLoggerPrint)
* 3. When done, detach it with:             DetachDbgLogger(DbgLoggerPrint)
* 4. Delete this logger:                    PAL_SAFE_DELETE()
************************************************************************************************************************
*/
class DbgLoggerPrint final : public IDbgLogger
{
public:
    /// Constructor
    /// @param [in] severity     Specifies this logger's acceptable severity level.
    /// @param [in] sourceMask   Specifies this logger's acceptable origination types as a bit mask.
    DbgLoggerPrint(
        SeverityLevel   severity,
        uint32          sourceMask)
        :
        IDbgLogger(severity, sourceMask) {}

    /// Destructor
    virtual ~DbgLoggerPrint() {}

protected:
    /// Prints the message to an output window.
    ///
    /// @param [in] severity     Specifies the severity level of the log message
    /// @param [in] source       Specifies the origination type (source) of the log message
    /// @param [in] pClientTag   Indicates the client that logs a message. Only the first 'ClientTagSize'
    ///                          number of characters will be used to identify the client.
    /// @param [in] dataSize     Size of raw data.
    /// @param [in] pData        Pointer to raw data.
    virtual void WriteMessage(
        SeverityLevel   severity,
        OriginationType source,
        const char*     pClientTag,
        size_t          dataSize,
        const void*     pData);
};
} //namespace Util
#endif
