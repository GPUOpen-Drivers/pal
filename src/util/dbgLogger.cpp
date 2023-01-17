/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palDbgLogMgr.h"
#include "palSysUtil.h"
#include "palSysMemory.h"

namespace Util
{
static constexpr char LineEnd[] = "\n";

// =====================================================================================================================
// Creates a complete file name for debug logging by adding library name, process name, and pid to the base name.
// Returns a truncated file name if incoming file name string size is not enough.
Result CreateLogFileName(
    char*                        pFileName,
    uint32                       fileNameSize,
    const DbgLoggerFileSettings& settings,
    const char*                  pBaseFileName)
{
    PAL_ASSERT((pFileName != nullptr) && (fileNameSize != 0));

    Result result                           = Result::Success;
    char processNameBuff[MaxFileNameStrLen] = {};
    char libNameBuff[MaxFileNameStrLen]     = {};
    pFileName[0]                            = '\0';

    // Add library name to file name if enabled
    if (TestAnyFlagSet(settings.fileSettingsFlags, FileSettings::AddLibName))
    {
        libNameBuff[0] = '_'; // Start the library name with '_'
        result = GetCurrentLibraryName(libNameBuff + 1, sizeof(libNameBuff) - 1, nullptr, 0);
    }

    // Add process name to file name if enabled
    if ((result == Result::Success) && (TestAnyFlagSet(settings.fileSettingsFlags, FileSettings::AddPname)))
    {
        char  executableNameBuffer[MaxPathStrLen] = {};
        char* pExecutableName = nullptr;
        result = GetExecutableName(executableNameBuffer, &pExecutableName, sizeof(executableNameBuffer));
        if (result == Result::Success)
        {
            processNameBuff[0] = '_'; // Start the process name with '_'
            SplitFilePath(executableNameBuffer, nullptr, 0, processNameBuff + 1, sizeof(processNameBuff) - 1);
        }
    }

    // Create log directory if it doesn't already exists.
    if ((result == Result::Success) && (File::Exists(settings.pLogDirectory) == false))
    {
        result = MkDirRecursively(settings.pLogDirectory);
    }

    if (result == Result::Success)
    {
        int32 len = 0;
        PAL_ASSERT(pBaseFileName != nullptr);
        if (TestAnyFlagSet(settings.fileSettingsFlags, FileSettings::AddPid))
        {
            len = Snprintf(pFileName, fileNameSize, "%s%s%s%s_%d.txt", settings.pLogDirectory,
                           pBaseFileName, libNameBuff, processNameBuff, GetIdOfCurrentProcess());
        }
        else
        {
            len = Snprintf(pFileName, fileNameSize, "%s%s%s%s.txt", settings.pLogDirectory,
                           pBaseFileName, libNameBuff, processNameBuff);
        }
        // Snprintf (see vsnprintf/vsnprintf_s doc) returns a negative number in case of encoding
        // error. Also, only when this returned value is non-negative and less than input buffer
        // size, the string has been completely written. Hence, check for this condition and set result.
        if ((len < 0) || (len >= fileNameSize))
        {
            // buffer too small, hence make it larger
            result = Result::ErrorInvalidMemorySize;
        }
    }
    return result;
}

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
    // Also, don't open file if logging is disabled. Otherwise, the log directory
    // gets littered with useless and empty log files.
    if ((g_dbgLogMgr.GetLoggingEnabled()) &&
        (TestAnyFlagSet(fileAccessMask, FileAccessMode::FileAccessRead) == false))
    {
        result = m_file.Open(pFileName, fileAccessMask);
    }

    return result;
}

// =====================================================================================================================
/// Prints the log message to output window.
void DbgLoggerPrint::WriteMessage(
    SeverityLevel   severity,
    OriginationType source,
    const char*     pClientTag,
    size_t          dataSize,
    const void*     pData)
{
    PAL_ASSERT(pData != nullptr);
    const char* pString = static_cast<const char*>(pData);

    // Otherwise, send the string to stderr.
    fputs(pString, stderr);
}
} //namespace Util
#endif
