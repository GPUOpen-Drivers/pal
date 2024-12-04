/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <dd_logger_api.h>
#include <dd_assert.h>
#include <dd_result.h>

#include <stb_sprintf.h>

#include <fcntl.h>
#include <unistd.h>

#include <cstring>
#include <cstdlib>

namespace
{

constexpr size_t STACK_LOG_BUF_SIZE = 512;

struct Logger
{
    union
    {
        struct
        {
            int handle;
        } file;

        struct
        {
            DDLoggerLogCallback logCallback;
            void*               pUserData;
        } callback;
    };

    DD_LOG_LVL     level;
    DD_LOGGER_TYPE type;
    bool           rawLoggingEnabled;
};

void SetLogLevel(DDLoggerInstance* pInstance, DD_LOG_LVL level)
{
    Logger* pLogger = reinterpret_cast<Logger*>(pInstance);
    pLogger->level = level;
}

void SetLogRaw(DDLoggerInstance* pInstance, bool setLogRaw)
{
    Logger* pLogger = reinterpret_cast<Logger*>(pInstance);
    pLogger->rawLoggingEnabled = setLogRaw;
}

void LogWrite(Logger* pLogger, DD_LOG_LVL level, const char* pLogMsg, uint32_t msgSize)
{
    if (pLogger->type == DD_LOGGER_TYPE_FILE)
    {
        ssize_t bytesWritten = write(pLogger->file.handle, pLogMsg, msgSize);
        DD_ASSERT((uint32_t)bytesWritten == msgSize);
        (void)bytesWritten;
    }
    else if (pLogger->type == DD_LOGGER_TYPE_CALLBACK)
    {
        pLogger->callback.logCallback(pLogger->callback.pUserData, level, pLogMsg, msgSize);
    }
}

void Log(DDLoggerInstance* pInstance, DD_LOG_LVL level, const char* pFormat, ...)
{
    const char LogLevelPrefixVerbose[] = "[VERBOSE] ";
    const char LogLevelPrefixInfo[] = "[INFO] ";
    const char LogLevelPrefixWarn[] = "[WARN] ";
    const char LogLevelPrefixError[] = "[ERROR] ";

    const char* const LogLevelPrefix[DD_LOG_LVL_COUNT] = {
        LogLevelPrefixVerbose,
        LogLevelPrefixInfo,
        LogLevelPrefixWarn,
        LogLevelPrefixError};

    const uint32_t LogLevelPrefixLength[DD_LOG_LVL_COUNT] = {
        sizeof(LogLevelPrefixVerbose) - 1,
        sizeof(LogLevelPrefixInfo) - 1,
        sizeof(LogLevelPrefixWarn) - 1,
        sizeof(LogLevelPrefixError) - 1};

    Logger* pLogger = reinterpret_cast<Logger*>(pInstance);
    if (level >= pLogger->level)
    {
        int writtenSize = 0;
        int logSize = 0;

        char tempBuf[STACK_LOG_BUF_SIZE];

        // Prepend the verbosity level (if enabled)
        if (pLogger->rawLoggingEnabled == false)
        {
            std::memcpy(tempBuf, LogLevelPrefix[level], LogLevelPrefixLength[level]);
            writtenSize = LogLevelPrefixLength[level];
            logSize += writtenSize;
        }

        va_list va;
        va_start(va, pFormat);

        // Reserve one byte for newline character.
        writtenSize = stbsp_vsnprintf(tempBuf + writtenSize, STACK_LOG_BUF_SIZE - writtenSize - 1, pFormat, va);
        logSize += writtenSize;

        va_end(va);

        // Post-fix a newline character (if enabled)
        if (pLogger->rawLoggingEnabled == false)
        {
            tempBuf[logSize]   = '\n';
        }

        LogWrite(pLogger, level, tempBuf, logSize);
    }
}

void SetLogLevelNull(DDLoggerInstance*, DD_LOG_LVL)
{
}

void SetLogRawNull(DDLoggerInstance*, bool)
{
}

void LogNull(DDLoggerInstance*, DD_LOG_LVL, const char*, ...)
{
}

DD_RESULT DDLoggerCreateFileLogger(const char* pFilePath, uint32_t filePathSize, Logger* pLogger)
{
    if ((pFilePath == nullptr) || (filePathSize == 0))
    {
        return DD_RESULT_COMMON_INVALID_PARAMETER;
    }

    DD_RESULT result = DD_RESULT_SUCCESS;

    const uint32_t DefaultPathSizeMax = 4096; // including null-terminator

    if (filePathSize >= DefaultPathSizeMax)
    {
        return DD_RESULT_COMMON_OUT_OF_RANGE;
    }

    // `pFilePath` isn't guaranteed to be null-terminated, so allocate a new path buffer and append '\0'.
    char pFilePathBuf[DefaultPathSizeMax] {};
    std::memcpy(pFilePathBuf, pFilePath, filePathSize);
    pFilePathBuf[filePathSize] = '\0';

    pLogger->file.handle = open(pFilePathBuf, O_WRONLY | O_CREAT | O_TRUNC, S_IWUSR | S_IROTH);
    if (pLogger->file.handle == -1)
    {
        result = DD_RESULT_COMMON_INVALID_PARAMETER;
    }

    return result;
}

DD_RESULT DDLoggerCreateCallbackLogger(DDLoggerLogCallback logCallback, void* pCallbackUserdata, Logger* pLogger)
{
    if (logCallback == nullptr)
    {
        return DD_RESULT_COMMON_INVALID_PARAMETER;
    }

    DD_RESULT result = DD_RESULT_SUCCESS;

    pLogger->callback.logCallback = logCallback;
    pLogger->callback.pUserData   = pCallbackUserdata;

    return result;
}

} // anonymous namespace

DD_RESULT DDLoggerCreate(DDLoggerCreateInfo* pCreateInfo, DDLoggerApi* pOutLoggerApi)
{
    if ((pCreateInfo == nullptr) || (pOutLoggerApi == nullptr))
    {
        return DD_RESULT_COMMON_INVALID_PARAMETER;
    }

    Logger* pLogger = (Logger*)std::calloc(1, sizeof(*pLogger));
    if (pLogger == nullptr)
    {
        return DD_RESULT_COMMON_OUT_OF_HEAP_MEMORY;
    }

    DD_RESULT result = DD_RESULT_SUCCESS;

    (*pOutLoggerApi) = {};

    if (result == DD_RESULT_SUCCESS)
    {
        switch (pCreateInfo->type)
        {
        case DD_LOGGER_TYPE_FILE:
            result = DDLoggerCreateFileLogger(pCreateInfo->file.pFilePath, pCreateInfo->file.filePathSize, pLogger);
            break;
        case DD_LOGGER_TYPE_CALLBACK:
            result = DDLoggerCreateCallbackLogger(
                pCreateInfo->callback.logCallback, pCreateInfo->callback.pUserData, pLogger);
            break;
        default:
            result = DD_RESULT_COMMON_INVALID_PARAMETER;
            DD_ASSERT(false);
            break;
        }
    }

    bool createNullLogger = false;
    if (result == DD_RESULT_COMMON_INVALID_PARAMETER)
    {
        // nullptr filepath or zero file path size or nullptr callback indicates that the caller wants to create a null
        // logger.
        createNullLogger = true;
        result = DD_RESULT_SUCCESS;
    }

    pLogger->level               = DD_LOG_LVL_ERROR; // default to error log level.
    pLogger->type                = pCreateInfo->type;
    pLogger->rawLoggingEnabled   = pCreateInfo->rawLogging;

    if ((createNullLogger) || (result != DD_RESULT_SUCCESS))
    {
        // If failed to create a logger, intentionally or not, make a dummy logger instead.
        pOutLoggerApi->pInstance    = nullptr;
        pOutLoggerApi->SetLogLevel  = SetLogLevelNull;
        pOutLoggerApi->SetLogRaw    = SetLogRawNull;
        pOutLoggerApi->Log          = LogNull;

        std::free(pLogger);
    }
    else
    {
        pOutLoggerApi->pInstance   = reinterpret_cast<DDLoggerInstance*>(pLogger);
        pOutLoggerApi->SetLogLevel = SetLogLevel;
        pOutLoggerApi->SetLogRaw   = SetLogRaw;
        pOutLoggerApi->Log         = Log;
    }

    return result;
}

void DDLoggerDestroy(DDLoggerApi* pLoggerApi)
{
    if (pLoggerApi != nullptr)
    {
        if (pLoggerApi->pInstance != nullptr)
        {
            Logger* pLogger = reinterpret_cast<Logger*>(pLoggerApi->pInstance);

            if (pLogger->type == DD_LOGGER_TYPE_FILE)
            {
                int err = close(pLogger->file.handle);
                DD_ASSERT(err == 0);
                (void)err;
            }

            delete pLogger;
        }

        pLoggerApi->pInstance   = nullptr;
        pLoggerApi->SetLogLevel = nullptr;
        pLoggerApi->SetLogRaw   = nullptr;
        pLoggerApi->Log         = nullptr;
    }
}
