/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef DD_TOOL_API_H
#define DD_TOOL_API_H

#include "dd_api_registry_api.h"
#include "dd_common_api.h"
#include "dd_logger_api.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DD_TOOL_API_VERSION_MAJOR 0
#define DD_TOOL_API_VERSION_MINOR 1
#define DD_TOOL_API_VERSION_PATCH 0

typedef struct DDToolInstance DDToolInstance;

typedef struct DDToolApi
{
    /// A opaque pointer to an internal DevDriver tool instance.
    DDToolInstance* pInstance;

    /// Load and initialize all DevDriver modules from a designated directory on the host system.
    ///
    /// @param pInstance Must be \ref DDModulesApi.pInstance.
    /// @return DD_RESULT_SUCCESS if all modules have been loaded and initialized successfully.
    /// @return Errors if any module fails to initialize.
    DD_RESULT (*LoadModules)(DDToolInstance* pInstance);

    /// Return a pointer to an instance of \ref DDApiRegistry. The returned pointer becomes invalid
    /// after the destruction of \ref DDToolApi.
    ///
    /// @param pInstance Must be \ref DDModulesApi.pInstance.
    /// @return a pointer to an instance of \ref DDApiRegistry.
    DDApiRegistry* (*GetApiRegistry)(DDToolInstance* pInstance);

    /// Connect to a \ref DDRouter.
    ///
    /// @param pInstance Must be \ref DDModulesApi.pInstance.
    /// @param pIpAddr A pointer to a string representing an IP address. Set this value to NULL to
    /// to connect to a local \ref DDRouter.
    /// @param port A port number when connecting to \ref DDRouter on remote machines.
    /// @return DD_RESULT_SUCCESS A connection is established, otherwise connection failed.
    DD_RESULT (*Connect)(DDToolInstance* pInstance, const char* pIpAddr, uint16_t port);

    /// Disconnect from a \ref DDRouter.
    ///
    /// @param pInstance Must be \ref DDModulesApi.pInstance.
    void (*Disconnect)(DDToolInstance* pInstance);

    /// Get the AMDLog client ID.
    /// Note: This can only be called after a successful call to Connect
    ///
    /// @param pInstance Must be \ref DDModulesApi.pInstance.
    /// @return The client ID for the AMDLog connection.
    DDClientId (*GetAmdlogClientId)(DDToolInstance* pInstance);

} DDToolApi;

typedef struct DDToolApiCreateInfo
{
    /// A pointer to a description string, encoded in UTF-8.
    const char* pDescription;

    /// The size, in bytes, of the string pointed to by \ref pDescription, not including null-terminator.
    size_t descriptionSize;

    /// A pointer to a string, encoded in UTF-8, that represents a path from which all DevDriver modules
    /// will be loaded. If this parameter is NULL, no module will be loaded.
    const char* pModulesDir;

    /// The size, in bytes, of the string pointed by \ref pModulesDir, not including null-terminator.
    /// If this parameter is zero, no module will be loaded.
    size_t moduleDirSize;

    /// A pointer to the log file path string, encoded in UTF-8. If this parameter is NULL or the specified
    /// file cannot be created/opened, a dummy logger is created. A dummy logger simply discards log messages.
    const char* pLogFilePath;

    /// The size, in bytes of the string pointed to by \ref pLogFilePath, not including null-terminator.
    size_t logFilePathSize;

    /// A custom logger. If \ref customLogger.Log is not NULL, \ref pLogFilePath is ignored, and the custom
    /// logger will be used.
    DDLoggerApi customLogger;

    /// The following timeouts should generally only be set when working on an emulator

    /// Retry timeout. If set to zero, a default value is set.
    uint32_t retryTimeoutInMs;

    /// Communication timeout. If set to zero, a default value is set.
    uint32_t communicationTimeoutInMs;

    /// Connection timeout. If set to zero, a default value is set.
    uint32_t connectionTimeoutInMs;
} DDToolApiCreateInfo;

/// Create an instance of \ref DDToolApi.
///
/// @param pCreateInfo A pointer to \ref DDToolApiCreateInfo.
/// @param[in/out] ppOutToolApi Will be set to point to an instance of \ref DDToolApi upon success.
/// @return DD_RESULT_SUCCESS If the function succeeds.
/// @return DD_RESULT_COMMON_INVALID_PARAMETER If \ref DDToolApiCreateInfo.pDescription is NULL or
///         \ref DDToolApiCreateInfo.descriptionSize is not greater than zero.
DD_RESULT DDToolApiCreate(const DDToolApiCreateInfo* pCreateInfo, DDToolApi** ppOutToolApi);

/// Destroy an instance of \ref DDToolApi and set its pointer to NULL.
/// @param[in/out] ppToolApi A double to an instance of \ref DDToolApi.
void DDToolApiDestroy(DDToolApi** ppToolApi);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
