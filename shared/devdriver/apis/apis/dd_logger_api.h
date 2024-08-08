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

#ifndef DD_LOGGER_API_H
#define DD_LOGGER_API_H

#include "dd_common_api.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DD_LOGGER_API_NAME "DD_LOGGER_API"

#define DD_LOGGER_API_VERSION_MAJOR 0
#define DD_LOGGER_API_VERSION_MINOR 2
#define DD_LOGGER_API_VERSION_PATCH 0

typedef enum
{
    DD_LOG_LVL_VERBOSE = 0,
    DD_LOG_LVL_INFO,
    DD_LOG_LVL_WARN,
    DD_LOG_LVL_ERROR,
    DD_LOG_LVL_COUNT,
} DD_LOG_LVL;

typedef struct DDLoggerInstance DDLoggerInstance;
typedef void (*DDLoggerLogCallback)(DD_LOG_LVL logLevel, void* pUserData, const char* pMessage, uint32_t messageSize);

/// Enumeration of logger types.
typedef enum
{
    /// Log messages to a file.
    DD_LOGGER_TYPE_FILE = 0,
    /// Log messages to a user-defined callback function.
    DD_LOGGER_TYPE_CALLBACK,
} DD_LOGGER_TYPE;

/// Structure defining creation parameters for a logger.
/// The logger will be created based on the type specified in \ref type.
/// Either file-based logging or callback-based logging can be created.
typedef struct
{
    /// Type of logger to create.
    DD_LOGGER_TYPE type;

    /// If `true`, no formatting is applied to log messages.
    bool rawLogging;

    union
    {
        struct
        {
            /// A path to a file on local disk. The path string should be encoded in UTF-8. A new file will be
            /// created if it doesn't already exist. If the file is already present, its content will be overwritten.
            const char* pFilePath;

            /// Size, in bytes, of \ref file.pFilePath, NOT including the null-terminator.
            uint32_t filePathSize;
        } file;

        struct
        {
            /// Callback function to be executed when a log message is generated.
            /// Note that log messages will be filtered according to verbosity level before
            /// being passed to the callback.
            DDLoggerLogCallback logCallback;

            /// Opaque pointer to data that will be passed to the callback function. Can be NULL.
            void* pUserData;
        } callback;
    };
} DDLoggerCreateInfo;

typedef struct
{
    /// An opaque pointer to the internal implementation of DDLoggerApi.
    DDLoggerInstance* pInstance;

    /// Set the current log level. Logs with levels smaller than the current log level will be ignored.
    /// The default log level is \ref DD_LOG_LVL_ERROR.
    /// @param level Verbosity level for logger
    void (*SetLogLevel)(DDLoggerInstance* pInstance, DD_LOG_LVL level);

    /// Writes log messages as-is without any modifications.
    ///
    /// By default, log messages are prepended with `[LOG_LEVEL]` and postfixed with a newline.
    /// This function disables that behavior, causing messages to be written to the logger directly.
    /// @note If you are implementing a custom callback and want fine-grain control over how logs
    ///       are formatted, this should be enabled. For file-based logging, this should not be set.
    void (*SetLogRaw)(DDLoggerInstance* pInstance, bool setLogRaw);

    /// Logs a UTF-8 string. The formatted string will be truncated if its length exceeds the maximum.
    ///
    /// @note Unless `SetLogRaw` is enabled, this function append a newline at the end of the logged
    ///       string, in addition to prefixing the message log level. For example:
    ///
    ///       [ERROR] Example log string.
    ///
    /// @param[in] pInstance Must be \ref DDLoggerApi.pInstance.
    /// @param[in] level     Log level of the message.
    /// @param[in] pFormat   `printf` style format string.
    void (*Log)(DDLoggerInstance* pInstance, DD_LOG_LVL level, const char* pFormat, ...);
} DDLoggerApi;

/// Creates an instance of \ref DDLoggerApi.
/// The logger may write messages to a file or call a user-defined callback function,
/// depending on the parameters specified in \ref DDLoggerCreateInfo.
///
/// @note If the logger creation fails, a dummy logger is created that simply discards log messages.
///
/// @param[in]     pCreateInfo    See \ref DDLoggerCreateInfo.
/// @param[in/out] ppOutLoggerApi Will be set to an instance of \ref DDLoggerApi.
///
/// @return \ref DD_RESULT_SUCCESS An valid logger is created.
/// @return \ref DD_RESULT_COMMON_INVALID_PARAMETER The call failed, a dummy logger is created.
DD_RESULT DDLoggerCreate(DDLoggerCreateInfo* pCreateInfo, DDLoggerApi* pOutLoggerApi);

/// Destroy an instance of \ref DDLoggerApi.
///
/// @param[in/out] ppLoggerApi A pointer to an instance of \ref DDLoggerApi.
void DDLoggerDestroy(DDLoggerApi* pLoggerApi);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
