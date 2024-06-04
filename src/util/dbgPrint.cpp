/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palDbgLogMgr.h"
#endif
#include "palDbgPrint.h"
#include "palFile.h"
#include "palStringView.h"
#include "palSysMemory.h"
#include <cstdio>

namespace Util
{

#if (PAL_ENABLE_PRINTS_ASSERTS || PAL_ENABLE_LOGGING)
/// Global callback used to report debug print messages.
/// If the pointer is set to a valid callback, the callback will be called every time a debug message is printed
DbgPrintCallback g_dbgPrintCallback = { nullptr, nullptr };

#if   defined(__unix__)
// Directory where log files will be written.
static constexpr char LogDirectoryStr[] = "/var/tmp/";
#endif

// Entry in the global table of debug print targets. Defines the debug category, output mode and prefix string to use.
struct DbgPrintTarget
{
    DbgPrintMode outputMode;  // Print to debugger, print to file, or disabled.
    const char*  pPrefix;     // Prefix to add to each debug print.
    const char*  pFileName;   // Filename of log file (if outputMode is File).
};

// Global table of information for each debug print category.
static DbgPrintTarget g_dbgPrintTable[DbgPrintCatCount] =
{
    { DbgPrintMode::Print,         "Info",  "palInfo.txt",  },  // DbgPrintCatInfoMsg
    { DbgPrintMode::Print,         "Warn",  "palWarn.txt",  },  // DbgPrintCatWarnMsg
    { DbgPrintMode::Print,         "Error", "palError.txt", },  // DbgPrintCatErrorMsg
    { DbgPrintMode::Disable,       "ScMsg", "palScMsg.txt", },  // DbgPrintCatScMsg
    { DbgPrintMode::Print,         "Event", "palEvent.txt", },  // DbgPrintCatEventPrintMsg
    { DbgPrintMode::PrintCallback, "Event", "palEvent.txt", },  // DbgPrintCatEventPrintCallbackMsg
    { DbgPrintMode::File,          "Info",  "palLog.txt",   }   // DbgPrintCatMsgFile
};

#if PAL_ENABLE_LOGGING
/// Some defaults for debug prints
constexpr SeverityLevel   DefaultSeverityLevel   = SeverityLevel::Error;
constexpr OriginationType DefaultOriginationType = OriginationType::DebugPrint;
#endif

// =====================================================================================================================
// Sends the specified log string (pString) to the appropriate output (i.e., file or debugger, configured in the target
// argument).
static void OutputString(
    const DbgPrintTarget& target,
    DbgPrintCategory      category,
    const char*           pString)
{
    Result result = Result::Success;
    File   logFile;

    switch (target.outputMode)
    {
    case DbgPrintMode::Print:
    {
        // Otherwise, send the string to stderr.
        fputs(pString, stderr);

        // If there's a valid callback function, then invoke it with the current message.
        if (g_dbgPrintCallback.pCallbackFunc != nullptr)
        {
            g_dbgPrintCallback.pCallbackFunc(g_dbgPrintCallback.pUserdata, category, pString);
        }

        break;
    }

    case DbgPrintMode::PrintCallback:
    {
        // Only output to the debug callback and avoid other debug output.

        // If there's a valid callback function, then invoke it with the current message.
        if (g_dbgPrintCallback.pCallbackFunc != nullptr)
        {
            g_dbgPrintCallback.pCallbackFunc(g_dbgPrintCallback.pUserdata, category, pString);
        }

        break;
    }

    case DbgPrintMode::File:
#if PAL_ENABLE_PRINTS_ASSERTS
        PAL_ASSERT(target.pFileName != nullptr);
        result = OpenLogFile(&logFile, target.pFileName, FileAccessAppend);
        if (result == Result::Success)
        {
            logFile.Write(pString, strlen(pString));
        }
        break;
#endif

    case DbgPrintMode::Disable:
    default:
        // For performance, the debug print methods should avoid formatting strings and attempting to output the string
        // earlier on than this.
        PAL_NEVER_CALLED();
        break;
    }
}

// =====================================================================================================================
// Assembles a log string in the specified output buffer.  Will return ErrorInvalidMemorySize if the destination buffer
// is not large enough.
static Result BuildString(
    char*                 pOutBuf,   // [out] Output buffer to contain assembled string.
    size_t                bufSize,   // Size of the destination buffer.
    const DbgPrintTarget& target,    // Debug output target (mode, prefix, and filename).
    DbgPrintStyle         style,     // Text output style (i.e., has prefix and/or CR-LF).
    const char*           pFormat,   // Printf-style format string.
    va_list               argList)   // Printf-style argument list.
{
    pOutBuf[0] = '\0';

    // Add the prefix string, if requested
    if (TestAnyFlagSet(style, DbgPrintStyleNoPrefix) == false)
    {
        const char* const pPrefixString = "AMD-PAL: ";

        Strncat(pOutBuf, bufSize, pPrefixString);
        Strncat(pOutBuf, bufSize, target.pPrefix);
        Strncat(pOutBuf, bufSize, ": ");
    }

    const size_t length = strlen(pOutBuf);
    Vsnprintf(pOutBuf + length, bufSize - length, pFormat, argList);

    // Add the CR/LF, if requested.
    if (TestAnyFlagSet(style, DbgPrintStyleNoCrLf) == false)
    {
        const char* const pEndString = "\r\n";

        Strncat(pOutBuf, bufSize, pEndString);
    }

    const size_t finalLength = strlen(pOutBuf);
    PAL_ASSERT(finalLength < bufSize);

    // Assume that if the final string length is bufSize-1 then some stuff was truncated.
    return (finalLength == (bufSize - 1)) ? Result::ErrorInvalidMemorySize : Result::Success;
}

// =====================================================================================================================
// Assembles a log string and sends it to the desired output target.  Common implementation shared by the rest of the
// debug print functions.
static void DbgVPrintfHelper(
    DbgPrintCategory      category, // Debug print category (info, warning, error, etc)
    DbgPrintStyle         style,    // Text output style (i.e., has prefix and/or CR-LF).
    const char*           pFormat,  // Printf-style format string.
    va_list               argList)  // Printf-style argument list.
{
    // Look up the debug print target based on the category
    const DbgPrintTarget& target = g_dbgPrintTable[category];

    static constexpr size_t BufferLength = 1024;

    char  buffer[BufferLength];

    char* pOutputBuf = &buffer[0];
    char* pLargeBuf  = nullptr;

    if (BuildString(pOutputBuf, BufferLength, target, style, pFormat, argList) != Result::Success)
    {
        const size_t largeBufferLength = 1024*1024;
        pLargeBuf = new char[largeBufferLength];

        if (pLargeBuf != nullptr)
        {
            pOutputBuf = pLargeBuf;
            BuildString(pOutputBuf, largeBufferLength, target, style, pFormat, argList);
        }
    }

    OutputString(target, category, pOutputBuf);

    delete [] pLargeBuf;
}

#if PAL_ENABLE_LOGGING
// =====================================================================================================================
// Map DbgPrintCategory enumerators to the new logging system.
static void MapDbgPrintCategory(
    DbgPrintCategory category,       // Debug message category.
    SeverityLevel*   pSeverityLevel, // Severity level above which all severities are accepted
    OriginationType* pOrigType)      // Source of debug message
{
    switch (category)
    {
    case DbgPrintCategory::DbgPrintCatInfoMsg:
        *pSeverityLevel = SeverityLevel::Info;
        break;
    case DbgPrintCategory::DbgPrintCatWarnMsg:
        *pSeverityLevel = SeverityLevel::Warning;
        break;
    case DbgPrintCategory::DbgPrintCatErrorMsg:
        *pSeverityLevel = SeverityLevel::Error;
        break;
    case DbgPrintCategory::DbgPrintCatScMsg:
        *pOrigType = OriginationType::PipelineCompiler;
        break;
    case DbgPrintCategory::DbgPrintCatEventPrintMsg:
    case DbgPrintCategory::DbgPrintCatEventPrintCallbackMsg:
    case DbgPrintCategory::DbgPrintCatMsgFile:
        // Ignore these enumerators
        break;
    default:
        PAL_NEVER_CALLED();
        break;
    }
}
#endif

// =====================================================================================================================
// Assembles a log string and sends it to the desired output target.  This method accepts a pre-initialized va_list
// parameter instead of a direct variable argument list, and is used when printing out messages on behalf of SC.
void DbgVPrintf(
    DbgPrintCategory category, // Debug message category.
    DbgPrintStyle    style,    // Text output style (i.e., has prefix and/or CR-LF).
    const char*      pFormat,  // Printf-style format string.
    va_list          argList)  // Printf-style argument list.
{
    PAL_ASSERT(category < DbgPrintCatCount);

#if PAL_ENABLE_PRINTS_ASSERTS
    if (g_dbgPrintTable[category].outputMode != DbgPrintMode::Disable)
    {
        DbgVPrintfHelper(category, style, pFormat, argList);
    }
#elif PAL_ENABLE_LOGGING
    SeverityLevel severityLevel = DefaultSeverityLevel;
    OriginationType origType    = DefaultOriginationType;
    // Map DbgPrintCategory to the new logging system. DbgPrintStyle is currently ignored.
    MapDbgPrintCategory(category, &severityLevel, &origType);

    // Proceed only if logging is enabled and message is acceptable
    if (g_dbgLogMgr.GetLoggingEnabled() && (g_dbgLogMgr.AcceptMessage(severityLevel, origType)))
    {
        Util::DbgVLog(severityLevel, origType, "AMD-PAL", pFormat, argList);
    }
#endif
}

// =====================================================================================================================
// Generic debug printf function to be used when the caller wishes to specify the output category and style.
void DbgPrintf(
    DbgPrintCategory category, // Debug message category.
    DbgPrintStyle    style,    // Text output style (i.e., has prefix and/or CR-LF).
    const char*      pFormat,  // Printf-style format string.
    ...)                       // Printf-style argument list.
{
    PAL_ASSERT(category < DbgPrintCatCount);

    // Check for PAL_ENABLE_PRINTS_ASSERTS before checking for PAL_ENABLE_LOGGING
    // so that the call to DbgLog() isn't duplicated when also called from PAL_DPINFO,
    // PAL_DPWARN, and PAL_DPERROR.
#if PAL_ENABLE_PRINTS_ASSERTS
    if (g_dbgPrintTable[category].outputMode != DbgPrintMode::Disable)
    {
        va_list argList;
        va_start(argList, pFormat);

        DbgVPrintfHelper(category, style, pFormat, argList);

        va_end(argList);
    }
#elif PAL_ENABLE_LOGGING
    SeverityLevel severityLevel = DefaultSeverityLevel;
    OriginationType origType    = DefaultOriginationType;
    // Map DbgPrintCategory to the new logging system. DbgPrintStyle is currently ignored.
    MapDbgPrintCategory(category, &severityLevel, &origType);

    // Proceed only if logging is enabled and message is acceptable
    if (g_dbgLogMgr.GetLoggingEnabled() && (g_dbgLogMgr.AcceptMessage(severityLevel, origType)))
    {
        va_list argList;
        va_start(argList, pFormat);
        Util::DbgVLog(severityLevel, origType, "AMD-PAL", pFormat, argList);
        va_end(argList);
    }
#endif
}

#endif

#if PAL_ENABLE_PRINTS_ASSERTS
// =====================================================================================================================
// Sets the debug print mode (output to debugger, write to file, disabled) for the specified category of messages.
// Probably controlled by setting and set during initialization.
void SetDbgPrintMode(
    DbgPrintCategory category, // Message category to control (e.g., CS dumps, SC output, etc.).
    DbgPrintMode     mode)     // New mode to be used for this message category.
{
    PAL_ASSERT(category < DbgPrintCatCount);

    g_dbgPrintTable[category].outputMode = mode;
}

// =====================================================================================================================
// Opens a file called "pFilename" that resides in the "LogDirectoryStr" directory.  This function exists in all build
// configurations.
Result OpenLogFile(
    File*       pFile,     // [out] File object to represent the opened file.
    const char* pFilename, // Filename to open.
    uint32      flags)     // ORed mask of FileAccessMode values specifying how this file will be accessed.
{
    char fullyQualifiedFilename[FILENAME_MAX];

    Snprintf(&fullyQualifiedFilename[0], FILENAME_MAX, "%s%s", LogDirectoryStr, pFilename);

    Result result = pFile->Open(fullyQualifiedFilename, flags);
    PAL_ALERT(result != Result::Success);

    return result;
}

// =====================================================================================================================
// Sets the global debug print callback.
void SetDbgPrintCallback(
    const DbgPrintCallback& callback)
{
    g_dbgPrintCallback = callback;
}
#endif

// =====================================================================================================================
// Compiler-specific wrapper of the standard vsnprintf implementation. If buffer is a nullptr it returns the length of
// the string that would be printed had a buffer with enough space been provided.
int32 Vsnprintf(
    char*       pOutput,  // [out] Output string.
    size_t      bufSize,  // Available space in pOutput (in bytes).
    const char* pFormat,  // Printf-style format string.
    va_list     argList)  // Pre-started variable argument list.
{
    // It is undefined to request a (count > 0) to be copied while providing a null buffer. Covers common causes of
    // crash in different versions of vsnprintf.
    PAL_ASSERT((pOutput == nullptr) ? (bufSize == 0) : (bufSize > 0));

    int32 length = -1;

    va_list argList2;

    va_copy(argList2, argList);

    // vsnprintf prints upto (bufSize - 1) entries leaving space for the terminating null character. On C99
    // compatible platforms, if a null buffer and size of zero is provided, it returns the length of the string.
    length = vsnprintf(pOutput, bufSize, pFormat, argList2);

    va_end(argList2);

    return length;
}

// =====================================================================================================================
// Variable argument wrapper on sprintf function to be used when output needs to be written to a string and no prefix
// information is required.
int32 Snprintf(
    char*       pOutput,  // [out] Output string.
    size_t      bufSize,  // Available space in pOutput (in bytes).
    const char* pFormat,  // Printf-style format string.
    ...)                  // Printf-style argument list.
{
    va_list argList;
    va_start(argList, pFormat);

    const int32 length = Vsnprintf(pOutput, bufSize, pFormat, argList);

    va_end(argList);

    return length;
}

// =====================================================================================================================
// Compiler-specific wrapper of the standard vsnprintf implementation. If buffer is a nullptr it returns the length of
// the string that would be printed had a buffer with enough space been provided.
int32 Vsnprintf(
    wchar_t*       pOutput,  // [out] Output string.
    size_t         bufSize,  // Available space in pOutput (in wchar count).
    const wchar_t* pFormat,  // Printf-style format string.
    va_list        argList)  // Pre-started variable argument list.
{
    // It is undefined to request a (count > 0) to be copied while providing a null buffer. Covers common causes of
    // crash in different versions of vsnprintf.
    PAL_ASSERT((pOutput == nullptr) ? (bufSize == 0) : (bufSize > 0));

    int32 length = -1;

    va_list argList2;

    va_copy(argList2, argList);

    // vswprintf prints upto (bufSize - 1) entries leaving space for the terminating null character.
    length = vswprintf(pOutput, bufSize, pFormat, argList2);

    va_end(argList2);

    return length;
}

// =====================================================================================================================
// Variable argument wrapper on sprintf function to be used when output needs to be written to a string and no prefix
// information is required.
int32 Snprintf(
    wchar_t*       pOutput,  // [out] Output string.
    size_t         bufSize,  // Available space in pOutput (in wchar count).
    const wchar_t* pFormat,  // Printf-style format string.
    ...)                     // Printf-style argument list.
{
    va_list argList;
    va_start(argList, pFormat);

    const int32 length = Vsnprintf(pOutput, bufSize, pFormat, argList);

    va_end(argList);

    return length;
}

// =====================================================================================================================
// Copy an arbitrary string into the provided buffer, encoding as necessary to avoid characters that are illegal
// in filenames (assuming the more restrictive Windows rules, even on non-Windows OSs).
//
// Any byte that would be illegal is encoded as % then two hex digits, like in a URL.
//
// The return value works like C++ standard snprintf:
// - If the provided buffer is big enough, it returns the number of bytes written, excluding the terminating \0.
// - If the provided buffer is not big enough, then the result string is truncated to fit, and the function returns
//   the number of bytes that would have been written if the buffer had been long enough, excluding the
//   terminating \0.
// - Passing 0 buffer length is allowed as a special case of that, and nullptr pOutput is then allowed.
size_t EncodeAsFilename(
    char*                   pOutput,            // [out] Output string; can be nullptr if bufSize is 0
    size_t                  bufSize,            // Available space in pOutput (in bytes)
    const StringView<char>& input,              // [in] Input string
    bool                    allowSpace,         // Allow (do not % encode) space
    bool                    allowDirSeparator)  // Allow (do not % encode) / and \ characters
{
    size_t       sizeSoFar = 0;
    const char*  pInput    = input.Data();
    const size_t length    = input.Length();

    for (size_t i = 0; i < length; ++pInput)
    {
        int ch = *pInput;
        const char* pFormat = "%c";
        if (((ch >= '\0' && (ch < ' '))) ||
            ((ch == ' ') && (allowSpace == false)) ||
            (((ch == '\\') || (ch == '/')) && (allowDirSeparator == false)) ||
            (ch == '<') || (ch == '>') || (ch == ':') || (ch == '"') || (ch == '|') ||
            (ch == '?') || (ch == 0x7f))
        {
            // Illegal character needs encoding.
            pFormat = "%%%2.2X";
        }

        // Call snprintf only if available buffer size > 0, and 1 is reserved for '\0'
        if ((sizeSoFar + 1) < bufSize)
        {
            sizeSoFar += snprintf(pOutput + sizeSoFar, bufSize - sizeSoFar, pFormat, ch & 0xff);
        }
        i++;
    }
    return sizeSoFar;
}

} // Util
