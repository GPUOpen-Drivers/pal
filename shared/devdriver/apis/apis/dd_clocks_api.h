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

#ifndef DD_CLOCKS_API_H
#define DD_CLOCKS_API_H

#include <stdint.h>

#include "dd_common_api.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define DD_CLOCKS_API_NAME "DD_CLOCKS_API"

#define DD_CLOCKS_API_VERSION_MAJOR 0
#define DD_CLOCKS_API_VERSION_MINOR 1
#define DD_CLOCKS_API_VERSION_PATCH 0

typedef struct DDClocksInstance DDClocksInstance;

/// The clock states
typedef enum DD_DEVICE_CLOCK_MODE
{
    DD_DEVICE_CLOCK_MODE_UNKNOWN = 0,
    DD_DEVICE_CLOCK_MODE_NORMAL  = 1,
    DD_DEVICE_CLOCK_MODE_STABLE  = 2,
    DD_DEVICE_CLOCK_MODE_PEAK    = 3,
    DD_DEVICE_CLOCK_MODE_COUNT,
} DD_DEVICE_CLOCK_MODE;

/// General input struct for input/output of clock info
typedef struct DDClockModeInfo
{
    DD_DEVICE_CLOCK_MODE mode;
    DDGpuId              gpuId;
} DDClockModeInfo;

typedef struct DDClockFreqs
{
    uint64_t gpuClock;    /// Frequency of the gpu clock
    uint64_t memoryClock; /// Frequency of the memory clock
} DDClockFreqs;

/// Structure that describes a clock mode
typedef struct DDDeviceClocksClockModeDescription
{
    const char*               pName;        /// Name of the clock mode
    const char*               pDescription; /// Description of the clock mode
    DD_DEVICE_CLOCK_MODE      id;           /// Identifier associated with the clock mode
} DDDeviceClocksClockModeDescription;

/// Structure that contains information about a clock mode
typedef struct DDDeviceClocksClockModeInfo
{
    const DDDeviceClocksClockModeDescription* pDescription; /// Pointer to a description associated with the clock mode
    DDClockFreqs                              clks;         /// Frequency of the clocks in Hz
} DDDeviceClocksClockModeInfo;

typedef struct DDClocksApi
{
    /// An opaque pointer to the internal implementation of the Clocks API.
    DDClocksInstance* pInstance;

    /// Queries the device clock modes.
    ///
    /// @param pInstance Must be \ref DDClocksApi.pInstance.
    /// @param pNumClockModes The number of params that are returned.
    /// @param ppClockModes The clock modes to return.
    /// @param gpuId The ID for this GPU, see definition above.
    /// @return DD_RESULT_SUCCESS Query was successful.
    /// @return DD_RESULT_COMMON_INVALID_PARAMETER If pointers are null or connection is invalid.
    /// @return Other errors if query failed.
    DD_RESULT (*QueryClockModes)(DDClocksInstance*                   pInstance,
                                 uint32_t*                           pNumClockModes,
                                 DDDeviceClocksClockModeInfo*        pClockModes,
                                 DDGpuId                             gpuId);

    /// Queries the current clock mode.
    ///
    /// @param pInstance Must be \ref DDClocksApi.pInstance.
    /// @param pClockModeId The output clock mode.
    /// @param gpuId The ID for this GPU, see definition above.
    /// @return DD_RESULT_SUCCESS Query was successful.
    /// @return DD_RESULT_COMMON_INVALID_PARAMETER If pointers are null or connection is invalid.
    /// @return Other errors if query failed.
    DD_RESULT (*QueryCurrentClockMode)(DDClocksInstance*        pInstance,
                                       DD_DEVICE_CLOCK_MODE*    pClockModeId,
                                       DDGpuId                  gpuId);

    /// Sets the clock mode to the provided mode
    ///
    /// @param pInstance Must be \ref DDClocksApi.pInstance.
    /// @param clockModeId The clock mode to set.
    /// @param gpuId The ID for this GPU, see definition above.
    /// @return DD_RESULT_SUCCESS Request was successful.
    /// @return DD_RESULT_FS_PERMISSION_DENIED On Linux only libdrm >= 3.49 supports setting clock modes.
    ///         If libdrm is older, we try to set clock modes through sysfile which requires root permission.
    ///         This error is returned if the router running on the target machine doesn't have root
    ///         permission to modify the sysfile.
    /// @return DD_RESULT_DD_GENERIC_UNAVAILABLE If the connection is invalid.
    /// @return Other errors if request failed.
    DD_RESULT (*SetClockMode)(DDClocksInstance*    pInstance,
                              DD_DEVICE_CLOCK_MODE clockModeId,
                              DDGpuId              gpuId);

} DDClocksApi;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
