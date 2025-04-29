/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#pragma once

#include <stdint.h>

#include "dd_common_api.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define DD_ENHANCED_CRASH_INFO_API_NAME "DD_ENHANCED_CRASH_INFO_API"

#define DD_ENHANCED_CRASH_INFO_API_VERSION_MAJOR 0
#define DD_ENHANCED_CRASH_INFO_API_VERSION_MINOR 1
#define DD_ENHANCED_CRASH_INFO_API_VERSION_PATCH 0

typedef struct DDEnhancedCrashInfoInstance DDEnhancedCrashInfoInstance;

typedef struct DDEnhancedCrashInfoConfigFlags
{
    uint8_t  captureWaveData   : 1;
    uint8_t  enableSingleMemOp : 1;
    uint8_t  enableSingleAluOp : 1;
    uint32_t reserved          : 29;
} DDEnhancedCrashInfoConfigFlags;

/// struct for input/output of Enhanced Crash Info config
typedef struct DDEnhancedCrashInfoConfig
{
    uint64_t                       processId;
    DDEnhancedCrashInfoConfigFlags flags;
} DDEnhancedCrashInfoConfig;

typedef struct DDEnhancedCrashInfoApi
{
    /// An opaque pointer to the internal implementation of the EnhancedCrashInfo API.
    DDEnhancedCrashInfoInstance* pInstance;

   /// Queries the current Enhanced Crash Info config.
    ///
    /// @param      pInstance Must be \ref DDEnhancedCrashInfoApi.pInstance.
    /// @param[out] pEnhancedCrashInfoConfig the current config is written through this parameter
    /// @return     DD_RESULT_SUCCESS Query was successful.
    /// @return     DD_RESULT_COMMON_INVALID_PARAMETER If pointers are null or connection is invalid.
    /// @return     Other errors if query failed.
    DD_RESULT (*QueryEnhancedCrashInfoConfig)(DDEnhancedCrashInfoInstance* pInstance,
                                              DDEnhancedCrashInfoConfig*   pEnhancedCrashInfoConfig);

    /// Sets the Enhanced Crash Info config
    ///
    /// @param  pInstance Must be \ref DDEnhancedCrashInfoApi.pInstance.
    /// @param  pEnhancedCrashInfoConfig the configuration to set
    /// @return DD_RESULT_SUCCESS Request was successful.
    /// @return DD_RESULT_DD_GENERIC_UNAVAILABLE If the connection is invalid.
    /// @return Other errors if request failed.
    DD_RESULT (*SetEnhancedCrashInfoConfig)(DDEnhancedCrashInfoInstance*     pInstance,
                                            const DDEnhancedCrashInfoConfig* pEnhancedCrashInfoConfig);
} DDEnhancedCrashInfoApi;

#ifdef __cplusplus
} // extern "C"
#endif

