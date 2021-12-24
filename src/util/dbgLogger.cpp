/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palDbgLogger.h"
#include "palSysUtil.h"
#include "palSysMemory.h"

namespace Util
{
static constexpr uint32 MsgSize = 1024;  ///< max size for the main message

/// Final formatted message consists of: "<severity>:<main message>\r\n"
/// Max size for severity = 8, which is strlen("Critical")
/// So, Final msg size = 8 + 1(for :) + msgSize + 2(for \r\t) + 1(for null termination) = msgSize + 12
static constexpr uint32 FinalMsgSize = MsgSize + 12;

static constexpr char LineEnd[] = "\n";

// =====================================================================================================================
// Provides simple formatting of the log message of the form: "<severity level>:<main msg>\r\n".
// Assumes that the input msg string is null terminated and formats only if there is enough space in the input buffer.
// The main msg will be truncated if it is longer than a predefined msg size.
// Users can call this function again with larger buffer space if it is aborted due to lack of space.
Result FormatMessageSimple(
    char*           pOutputMsg,
    uint32          outputMsgSize,
    SeverityLevel   severity,
    const char*     pFormat,
    va_list         args)
{
    Result result = Result::ErrorInvalidMemorySize;

    // Proceed only if there is enough space.
    if (outputMsgSize >= FinalMsgSize)
    {
        char buffer[MsgSize];
        buffer[0] = '\0';
        if (Util::Vsnprintf(buffer, MsgSize, pFormat, args) > 0)
        {
            result = Result::Success;
        }
        Snprintf(pOutputMsg, outputMsgSize, "%s: %s%s", SeverityLevelTable[static_cast<uint32>(severity)],
                 buffer, LineEnd);
    }

    return result;
}

// =====================================================================================================================
/// Initialize any data structures needed by the file logger.
Result DbgLoggerFile::Init(
    const char* pFileName)
{
    return m_file.Open(pFileName, FileAccessWrite);
}

// =====================================================================================================================
/// Cleanup any data structures used by the file logger.
void DbgLoggerFile::Cleanup()
{
    m_file.Close();
}

// =====================================================================================================================
/// Write the incoming message to the file if it passes through this logger's filter.
void DbgLoggerFile::LogMessage(
    SeverityLevel   severity,
    OriginationType source,
    const char*     pClientTag,
    const char*     pFormat,
    va_list         args)
{
    if (FilterMessage(severity, source))
    {
        char outputMsg[FinalMsgSize];
        outputMsg[0] = '\0';
        FormatMessageSimple(outputMsg, FinalMsgSize, severity, pFormat, args);
        // If the msg was truncated, just accept it as is. We are not going to format
        // again with a bigger buffer at this time.
        m_file.Write(outputMsg, (strlen(outputMsg) * sizeof(char)));
    }
}

// =====================================================================================================================
void DbgLoggerFile::LogMessage(
    SeverityLevel   severity,
    OriginationType source,
    const char*     pClientTag,
    size_t          dataSize,
    const void*     pData)
{
    // TODO in the next phase of development
    PAL_NOT_IMPLEMENTED();
}
} //namespace Util
#endif
