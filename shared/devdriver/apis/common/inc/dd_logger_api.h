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

#ifndef DD_LOGGER_API_H
#define DD_LOGGER_API_H

#include <dd_common_api.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DD_LOGGER_API_NAME "DD_LOGGER_API"

#define DD_LOGGER_API_VERSION_MAJOR 0
#define DD_LOGGER_API_VERSION_MINOR 1
#define DD_LOGGER_API_VERSION_PATCH 0

typedef enum
{
    DD_LOG_LVL_VERBOSE = 0,
    DD_LOG_LVL_INFO,
    DD_LOG_LVL_WARN,
    DD_LOG_LVL_ERROR,
    DD_LOG_LVL_COUNT,
} DD_LOG_LVL;

typedef struct
{
    /// A path to a file on local disk. The path string should be encoded in UTF-8. A new file will be
    /// created if it doesn't already exist. If the file is already present, its content will be overwritten.
    const char* pFilePath;

    /// Size, in bytes, of \ref pFilePath, NOT including the null-terminator.
    uint32_t filePathSize;
} DDLoggerCreateInfo;

typedef struct DDLoggerInstance DDLoggerInstance;

typedef struct
{
    /// An opaque pointer to the internal implementation of DDLoggerApi.
    DDLoggerInstance* pInstance;

    /// Set the current log level. Logs with levels smaller than the current log level will be ignored.
    /// The default log level is DD_LOG_LVL_ERROR.
    /// @param level Log level.
    void (*SetLogLevel)(DDLoggerInstance* pInstance, DD_LOG_LVL level);

    /// Log an ASCII string to file. The formatted string will be truncated if its length exceeds the maximum.
    /// This function always append a newline at the end of the logged string. It also prefix the logged
    /// string with the log level passed. For example:
    /// [ERROR] example log string.
    ///
    /// @param pInstance Must be \ref DDLoggerApi.pInstance.
    /// @param level Log level.
    /// @param pFormat printf style format string.
    void (*Log)(DDLoggerInstance* pInstance, DD_LOG_LVL level, const char* pFormat, ...);
} DDLoggerApi;

/// Create an instance of \ref DDLoggerApi.
///
/// @param pCreateInfo See \ref DDLoggerCreateInfo.
/// @param ppOutLoggerApi[in/out] Will be set to an instance of \ref DDLoggerApi. If the function succeeds,
/// a valid logger is created, which writes log messages to the file specified by
/// \ref DDLoggerCreateInfo.pFilePath. Upon failure, `*pOutLoggerApi` is set to a dummy logger that simply
/// discards log messages. This parameter cannot be NULL.
///
/// @return DD_RESULT_SUCCESS An valid logger is created.
/// @return DD_RESULT_COMMON_INVALID_PARAMETER The call failed, a dummy logger is created.
DD_RESULT DDLoggerCreate(DDLoggerCreateInfo* pCreateInfo, DDLoggerApi* pOutLoggerApi);

/// Destroy an instance of \ref DDLoggerApi.
///
/// @param ppLoggerApi[in/out] A pointer to an instance of \ref DDLoggerApi.
void DDLoggerDestroy(DDLoggerApi* pLoggerApi);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
