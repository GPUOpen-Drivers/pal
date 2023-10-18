/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "../inc/dd_logger_api.h"
#include "../inc/dd_assert.h"
#include "../inc/dd_mutex.h"

#include <stb_sprintf.h>

#include <fcntl.h>
#include <unistd.h>

#include <cstring>

namespace
{

constexpr size_t STACK_LOG_BUF_SIZE = 512;

struct Logger
{
    int fileHandle;

    DD_LOG_LVL level;
};

void SetLogLevel(DDLoggerInstance* pInstance, DD_LOG_LVL level)
{
    Logger* pLogger = reinterpret_cast<Logger*>(pInstance);
    pLogger->level = level;
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

        writtenSize = stbsp_sprintf(tempBuf, "[%s]", LogLevelStr[level]);
        DD_ASSERT(writtenSize > 0);
        logSize += writtenSize;

        // Reserve one byte for newline character.
        writtenSize = stbsp_vsnprintf(tempBuf + writtenSize, STACK_LOG_BUF_SIZE - writtenSize - 1, pFormat, va);
        logSize += writtenSize;

        tempBuf[logSize] = '\n';
        tempBuf[logSize + 1] = '\0';

        va_end(va);

        ssize_t bytesWritten = write(pLogger->fileHandle, tempBuf, logSize);
        DD_ASSERT(bytesWritten = logSize);
        (void)bytesWritten;
    }
}

void SetLogNullLevel(DDLoggerInstance*, DD_LOG_LVL)
{
}

void LogNull(DDLoggerInstance*, DD_LOG_LVL, const char*, ...)
{
}

} // anonymous namespace

DD_RESULT DDLoggerCreate(DDLoggerCreateInfo* pCreateInfo, DDLoggerApi* pOutLoggerApi)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    Logger* pLogger = nullptr;
    *pOutLoggerApi = {};

    if (pCreateInfo->pFilePath != nullptr && pCreateInfo->filePathSize > 0)
    {
        pLogger = new Logger;
        pLogger->level = DD_LOG_LVL_ERROR;

        // `pCreateInfo->pFilePath` isn't guaranteed to be null-terminated, so allocate a new path buffer and
        // append '\0'.
        char* pFilePathBuf = new char[pCreateInfo->filePathSize + 1];
        std::memcpy(pFilePathBuf, pCreateInfo->pFilePath, pCreateInfo->filePathSize);
        pFilePathBuf[pCreateInfo->filePathSize] = '\0';

        pLogger->fileHandle = open(pFilePathBuf, O_WRONLY | O_CREAT | O_TRUNC, S_IWUSR | S_IROTH);
        if (pLogger->fileHandle == -1)
        {
            result = DD_RESULT_COMMON_INVALID_PARAMETER;
        }

        delete[] pFilePathBuf;

        if (result == DD_RESULT_SUCCESS)
        {
            pOutLoggerApi->pInstance   = reinterpret_cast<DDLoggerInstance*>(pLogger);
            pOutLoggerApi->SetLogLevel = SetLogLevel;
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
        pOutLoggerApi->Log         = LogNull;
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

            int err = close(pLogger->fileHandle);
            DD_ASSERT(err == 0);
            (void)err;

            delete pLogger;
        }

        pLoggerApi->pInstance = nullptr;
        pLoggerApi->SetLogLevel = nullptr;
        pLoggerApi->Log = nullptr;
    }
}
