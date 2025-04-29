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

class ResultEx
{
static const uint32_t OS_ERROR_TYPE_BITMASK        = 0xC000'0000; // Use the most significant 2 bits for error type.
static const uint32_t OS_ERROR_TYPE_BITMASK_STDERR = 0x8000'0000;
static const uint32_t OS_ERROR_TYPE_BITMASK_WIN32  = 0xC000'0000;
static const uint32_t OS_ERROR_VALUE_BITMASK       = ~(OS_ERROR_TYPE_BITMASK);
static const uint32_t OS_ERROR_VALUE_MAX           = OS_ERROR_VALUE_BITMASK;

private:
    DD_RESULT m_result;
    uint32_t  m_osError;

public:
    ResultEx()
        : m_result {DD_RESULT_UNKNOWN}
        , m_osError {0}
    {}

    ResultEx(DD_RESULT r)
        : m_result {r}
        , m_osError {0}
    {}

    void operator=(const DD_RESULT& r)
    {
        m_result = r;
        m_osError = 0;
    }

    bool operator==(const DD_RESULT& rhs) const
    {
        return (m_result == rhs);
    }

    bool operator!=(const DD_RESULT& rhs) const
    {
        return (m_result != rhs);
    }

    bool operator==(const ResultEx& other) const
    {
        return (m_result == other.m_result) && (m_osError == other.m_osError);
    }

    bool operator!=(const ResultEx& other) const
    {
        return !(*this == other);
    }

    /// Implicit conversion to bool.
    operator bool() const
    {
        return (m_result == DD_RESULT_SUCCESS);
    }

    /// Implicit conversion to DD_RESULT.
    operator DD_RESULT() const
    {
        return m_result;
    }

    /// Set std errors defined in errno.h.
    void SetStdError(int err);

    /// Set error codes returned from Windows APIs.
    void SetWin32Error(uint32_t err);

    /// Write the error string into a buffer. The error string is capped at `bufSize` bytes (including null-terminator).
    /// `bufSize` is recommended to be at least 128.
    void GetErrorString(char* pBuf, uint32_t bufSize) const;

private:
    void CopyWin32ErrorString(uint32_t err, char* pBuf, uint32_t bufSize) const;
};

inline bool operator==(const DD_RESULT& lhs, const ResultEx& rhs)
{
    return (rhs == lhs);
}

inline bool operator!=(const DD_RESULT& lhs, const ResultEx& rhs)
{
    return (rhs != lhs);
}

} // namespace DevDriver
