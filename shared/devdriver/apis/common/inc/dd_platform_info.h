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

#pragma once

#include <dd_common_api.h>
#include <dd_result.h>

namespace DevDriver
{
class PlatformInfo
{
public:
    /// Initialize the global PlatformInfo object.
    ///
    /// This call is thread-safe. Only the first call of this function does the actual initialization. It is highly
    /// recommended that this function is called once at the start of the program before calling the individual
    /// retrieval functions below.
    ///
    /// @return DD_RESULT_SUCCESS if initialization succeeded.
    /// @return other errors if initialization failed. All fields in PlatformInfo object will be set to default values.
    static ResultEx Init();

    /// Get the page size.
    ///
    /// This function will first initialize the global PlatformInfo object if it hasn't been initialized. It is
    /// recommended that `Init()` is called once at the start of the program before calling this function.
    static uint32_t GetPageSize();

    /// Get the cache line size.
    ///
    /// This function will first initialize the global PlatformInfo object if it hasn't been initialized. It is
    /// recommended that `Init()` is called once at the start of the program before calling this function.
    static uint32_t GetCacheLineSize();

    /// Get L1 cache size.
    ///
    /// This function will first initialize the global PlatformInfo object if it hasn't been initialized. It is
    /// recommended that `Init()` is called once at the start of the program before calling this function.
    static uint32_t GetL1CacheSize();

    /// Get L2 cache size.
    ///
    /// This function will first initialize the global PlatformInfo object if it hasn't been initialized. It is
    /// recommended that `Init()` is called once at the start of the program before calling this function.
    static uint32_t GetL2CacheSize();

    /// Get L3 cache size.
    ///
    /// This function will first initialize the global PlatformInfo object if it hasn't been initialized. It is
    /// recommended that `Init()` is called once at the start of the program before calling this function.
    static uint32_t GetL3CacheSize();
};

} // namespace DevDriver
