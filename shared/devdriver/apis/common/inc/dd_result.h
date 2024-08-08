/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

namespace DevDriver
{

/// Convert `errno` defined in errno.h to `DD_RESULT` if a corresponding enum value exists,
/// otherwise `DD_RESULT_COMMON_UNKNOWN` is returned.
///
/// `errno` should always be a positive integer. But some third-party libraries use positive
/// return values to indicate success, and thus leave error codes negative. Negative `err`
/// will be negated before being converted to `DD_RESULT`.
DD_RESULT ResultFromErrno(int err);

/// Convert a Windows error code to DD_RESULT. `err` usually is the return value of Win32 API `GetLastError()`.
DD_RESULT ResultFromWin32Error(uint32_t err);

/// Convert a `DD_RESULT` to a null-terminated string.
const char* StringResult(DD_RESULT r);

} // namespace DevDriver
