/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
    if (outputMsgSize >= DefaultFinalMsgSize)
    {
        char buffer[DefaultMsgSize];
        buffer[0] = '\0';
        if (Util::Vsnprintf(buffer, DefaultMsgSize, pFormat, args) > 0)
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
    const char* pFileName,
    uint32      fileAccessMask)
{
    Result result = Result::ErrorInvalidFlags;

    // This logger always writes to a file, so a FileAccessRead mode is invalid.
    if (TestAnyFlagSet(fileAccessMask, FileAccessMode::FileAccessRead) == false)
    {
        result = m_file.Open(pFileName, fileAccessMask);
    }

    return result;
}

// =====================================================================================================================
/// Cleanup any data structures used by the file logger.
void DbgLoggerFile::Cleanup()
{
    m_file.Close();
}

// =====================================================================================================================
/// Formatted writes - Write the incoming message to the file if it passes through this logger's filter.
void DbgLoggerFile::LogMessage(
    SeverityLevel   severity,
    OriginationType source,
    const char*     pClientTag,
    const char*     pFormat,
    va_list         args)
{
    if (FilterMessage(severity, source))
    {
        char outputMsg[DefaultFinalMsgSize];
        outputMsg[0] = '\0';
        FormatMessageSimple(outputMsg, DefaultFinalMsgSize, severity, pFormat, args);
        // If the msg was truncated, just accept it as is. We are not going to format
        // again with a bigger buffer at this time.
        m_file.Write(outputMsg, (strlen(outputMsg) * sizeof(char)));
    }
}

// =====================================================================================================================
/// Unformatted writes - Write the incoming raw data to the file if it passes through this logger's filter.
void DbgLoggerFile::LogMessage(
    SeverityLevel   severity,
    OriginationType source,
    const char*     pClientTag,
    size_t          dataSize,
    const void*     pData)
{
    if (FilterMessage(severity, source))
    {
        m_file.Write(pData, dataSize);
    }
}
} //namespace Util
#endif
