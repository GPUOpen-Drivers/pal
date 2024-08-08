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

#include <stb_sprintf.h>

#include <fcntl.h>
#include <unistd.h>

#include <cstring>

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

void Log(DDLoggerInstance* pInstance, DD_LOG_LVL level, const char* pFormat, ...)
{
    const char* const LogLevelStr[DD_LOG_LVL_COUNT] = {"VERBOSE", "INFO", "WARN", "ERROR"};

    Logger* pLogger = reinterpret_cast<Logger*>(pInstance);
    if (level >= pLogger->level)
    {
        int writtenSize = 0;
        int logSize = 0;

        char tempBuf[STACK_LOG_BUF_SIZE];

        va_list va;
        va_start(va, pFormat);

        // Prepend the verbosity level (if enabled)
        if (pLogger->rawLoggingEnabled == false)
        {
            writtenSize = stbsp_sprintf(tempBuf, "[%s] ", LogLevelStr[level]);
            DD_ASSERT(writtenSize > 0);
            logSize += writtenSize;
        }

        // Reserve one byte for newline character.
        writtenSize = stbsp_vsnprintf(tempBuf + writtenSize, STACK_LOG_BUF_SIZE - writtenSize - 1, pFormat, va);
        logSize += writtenSize;

        // Post-fix a newline character (if enabled)
        if (pLogger->rawLoggingEnabled == false)
        {
            tempBuf[logSize]   = '\n';
            tempBuf[++logSize] = '\0';
        }
        else
        {
            tempBuf[logSize] = '\0';
        }

        va_end(va);

        if (pLogger->type == DD_LOGGER_TYPE_FILE)
        {
            ssize_t bytesWritten = write(pLogger->file.handle, tempBuf, logSize);
            DD_ASSERT(bytesWritten == logSize);
            (void)bytesWritten;
        }
        else if (pLogger->type == DD_LOGGER_TYPE_CALLBACK)
        {
            pLogger->callback.logCallback(level, pLogger->callback.pUserData, tempBuf, logSize);
        }
    }
}

void SetLogNullLevel(DDLoggerInstance*, DD_LOG_LVL)
{
}

void SetLogRawNull(DDLoggerInstance*, bool)
{
}

void LogNull(DDLoggerInstance*, DD_LOG_LVL, const char*, ...)
{
}

DD_RESULT DDLoggerCreateFileLogger(DDLoggerCreateInfo* pCreateInfo, DDLoggerApi* pOutLoggerApi)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    if ((pCreateInfo->file.pFilePath != nullptr) && (pCreateInfo->file.filePathSize > 0))
    {
        Logger* pLogger            = new Logger;
        pLogger->type              = DD_LOGGER_TYPE_FILE;
        pLogger->level             = DD_LOG_LVL_ERROR;
        pLogger->rawLoggingEnabled = pCreateInfo->rawLogging;

        // `pCreateInfo->pFilePath` isn't guaranteed to be null-terminated,
        // so allocate a new path buffer and append '\0'.
        char* pFilePathBuf = new char[pCreateInfo->file.filePathSize + 1];
        std::memcpy(pFilePathBuf, pCreateInfo->file.pFilePath, pCreateInfo->file.filePathSize);
        pFilePathBuf[pCreateInfo->file.filePathSize] = '\0';

        pLogger->file.handle = open(pFilePathBuf, O_WRONLY | O_CREAT | O_TRUNC, S_IWUSR | S_IROTH);
        if (pLogger->file.handle == -1)
        {
            result = DD_RESULT_COMMON_INVALID_PARAMETER;
        }

        delete[] pFilePathBuf;

        if (result == DD_RESULT_SUCCESS)
        {
            pOutLoggerApi->pInstance   = reinterpret_cast<DDLoggerInstance*>(pLogger);
            pOutLoggerApi->SetLogLevel = SetLogLevel;
            pOutLoggerApi->SetLogRaw   = SetLogRaw;
            pOutLoggerApi->Log         = Log;
        }
        else
        {
            delete pLogger;
            pLogger = nullptr;
        }
    }

    // If failed to create a logger, intentionally or not, make a dummy logger instead.
    if (pOutLoggerApi->pInstance == nullptr)
    {
        pOutLoggerApi->SetLogLevel = SetLogNullLevel;
        pOutLoggerApi->SetLogRaw   = SetLogRawNull;
        pOutLoggerApi->Log         = LogNull;
    }

    return result;
}

DD_RESULT DDLoggerCreateCallbackLogger(DDLoggerCreateInfo* pCreateInfo, DDLoggerApi* pOutLoggerApi)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    if (pCreateInfo->callback.logCallback != nullptr)
    {
        Logger* pLogger               = new Logger;
        pLogger->type                 = DD_LOGGER_TYPE_CALLBACK;
        pLogger->level                = DD_LOG_LVL_ERROR;
        pLogger->rawLoggingEnabled    = pCreateInfo->rawLogging;
        pLogger->callback.logCallback = pCreateInfo->callback.logCallback;
        pLogger->callback.pUserData   = pCreateInfo->callback.pUserData;

        pOutLoggerApi->pInstance      = reinterpret_cast<DDLoggerInstance*>(pLogger);
        pOutLoggerApi->SetLogLevel    = SetLogLevel;
        pOutLoggerApi->SetLogRaw      = SetLogRaw;
        pOutLoggerApi->Log            = Log;
    }
    else
    {
        pOutLoggerApi->pInstance      = nullptr;
        pOutLoggerApi->SetLogLevel    = SetLogNullLevel;
        pOutLoggerApi->SetLogRaw      = SetLogRawNull;
        pOutLoggerApi->Log            = LogNull;

        result = DD_RESULT_COMMON_INVALID_PARAMETER;
    }

    return result;
}

} // anonymous namespace

DD_RESULT DDLoggerCreate(DDLoggerCreateInfo* pCreateInfo, DDLoggerApi* pOutLoggerApi)
{
    if ((pCreateInfo == nullptr) || (pOutLoggerApi == nullptr))
    {
        return DD_RESULT_COMMON_INVALID_PARAMETER;
    }

    DD_RESULT result = DD_RESULT_SUCCESS;

    (*pOutLoggerApi) = {};

    switch (pCreateInfo->type)
    {
    case DD_LOGGER_TYPE_FILE:
        result = DDLoggerCreateFileLogger(pCreateInfo, pOutLoggerApi);
        break;
    case DD_LOGGER_TYPE_CALLBACK:
        result = DDLoggerCreateCallbackLogger(pCreateInfo, pOutLoggerApi);
        break;
    default:
        result = DD_RESULT_COMMON_INVALID_PARAMETER;
        DD_ASSERT(false);
        break;
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
