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

#ifndef DD_ROUTER_UTILS_API_H
#define DD_ROUTER_UTILS_API_H

#include "dd_common_api.h"
#include "dd_allocator_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DD_ROUTER_UTILS_API_NAME "DD_ROUTER_UTILS_API"

#define DD_ROUTER_UTILS_API_VERSION_MAJOR 0
#define DD_ROUTER_UTILS_API_VERSION_MINOR 1
#define DD_ROUTER_UTILS_API_VERSION_PATCH 0

typedef struct DDRouterUtilsInstance DDRouterUtilsInstance;

typedef struct DDRouterUtilsApi
{
    /// An opaque pointer to internal implementation of the router utils api.
    DDRouterUtilsInstance* pInstance;

    /// Retrieve the system information of the target machine where \ref DDRouter is running. This function
    /// caches the retrieved data and returns the cached data in subsequent calls. Note, system info is
    /// only available after the connection to \ref DDRouter has been established.
    ///
    /// @param pInstance Must be \ref DDRouterUtilsApi.pInstance.
    /// @param[out] pBuf A pointer to a buffer to receive system information data. This pointer can be NULL.
    /// @param[in,out] pSize A pointer to the size value. If \param pBuf is non-NULL, the pointed-to value
    /// represents the size of \param pBuf. Otherwise, the required size is written.
    /// @return DD_RESULT_SUCCESS The required size of system information data is written.
    /// @return DD_RESULT_SUCCESS The system information data is written to the buffer pointed to by \param pBuf.
    /// @return DD_RESULT_COMMON_BUFFER_TOO_SMALL The value pointed to by \param pSize is too small.
    /// @return DD_RESULT_COMMON_INVALID_PARAMETER \param pSize is NULL.
    /// @return DD_RESULT_DD_GENERIC_NOT_READY If router connection isn't ready.
    /// @return Other errors if the query failed.
    DD_RESULT (*GetSysInfo)(DDRouterUtilsInstance* pInstance, char* pBuf, size_t* pSize);

    /// Query the time stamp and frequency on the target machine. Time stamp is a monotonically
    /// increasing value representing the number of ticks since the machine boot. Frequency
    /// represents number of ticks per second.
    ///
    /// @param pInstance Must be \ref DDRouterUtilsApi.pInstance.
    /// @param[out] pTimestamp A pointer to a variable to receive time stamp value.
    /// @param[out] pFrequency A pointer to a variable to receive frequency value.
    /// @return DD_RESULT_SUCCESS Time stamp and frequency are set successfully.
    /// @return DD_RESULT_COMMON_INVALID_PARAMETER A NULL pointer is passed.
    /// @return DD_RESULT_DD_GENERIC_NOT_READY If router connection isn't ready.
    /// @return Other errors if the query failed.
    DD_RESULT (*GetTimestampAndFrequency)(DDRouterUtilsInstance* pInstance, uint64_t* pTimestamp, uint64_t* pFrequency);

    /// Queries the full path of a process on the target machine.
    /// @param pInstance Must be \ref DDRouterUtilsApi.pInstance.
    /// @param processId The identifier of the process to query.
    /// @param allocator The allocator to use to allocate the buffer.
    /// @param [out] pProcessPath The process path.
    /// @return DD_RESULT_SUCCESS Process path was queried successfully.
    /// @return DD_RESULT_DD_GENERIC_NOT_READY If router connection isn't ready.
    /// @return Other errors if the query failed.
    DD_RESULT (*QueryPathByProcessId)(DDRouterUtilsInstance* pInstance, uint32_t processId, DDAllocator allocator, char** pProcessPath);

} DDRouterUtilsApi;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
