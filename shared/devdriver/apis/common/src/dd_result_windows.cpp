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

#include <dd_result.h>
#include <dd_assert.h>
#include <stb_sprintf.h>
#include <Windows.h>
#include <winerror.h>

namespace DevDriver
{

DD_RESULT ResultFromWin32Error(uint32_t err)
{
    switch (err)
    {
        case ERROR_SUCCESS: return DD_RESULT_SUCCESS;
        case ERROR_ACCESS_DENIED: return DD_RESULT_COMMON_ACCESS_DENIED;
        case WAIT_TIMEOUT: return DD_RESULT_NET_TIMED_OUT;
        case WSAHOST_NOT_FOUND: // fallthrough
        case ERROR_FILE_NOT_FOUND: return DD_RESULT_FS_NOT_FOUND;
        case ERROR_INVALID_PARAMETER: // fallthrough
        case WSAEFAULT: // fallthrough
        case WSAEINVAL: // fallthrough
        case ERROR_INVALID_HANDLE: return DD_RESULT_COMMON_INVALID_PARAMETER;
        case ERROR_NOT_ENOUGH_MEMORY: // fallthrough
        case ERROR_OUTOFMEMORY: return DD_RESULT_COMMON_OUT_OF_HEAP_MEMORY;
        case ERROR_NO_DATA: // fallthrough
        case ERROR_PIPE_NOT_CONNECTED: // fallthrough
        case ERROR_BROKEN_PIPE: return DD_RESULT_NET_NOT_CONNECTED;
        case ERROR_FILE_TOO_LARGE: return DD_RESULT_COMMON_OUT_OF_RANGE;
        case WSAEINTR: return DD_RESULT_NET_INTERRUPTED;
        case WSASYSNOTREADY: return DD_RESULT_DD_GENERIC_NOT_READY;
        case WSAVERNOTSUPPORTED: return DD_RESULT_COMMON_UNSUPPORTED;
        case WSAEPROCLIM: return DD_RESULT_COMMON_OUT_OF_RANGE;
        case WSAECONNABORTED: return DD_RESULT_NET_CONNECTION_ABORTED;
        case WSAECONNREFUSED: return DD_RESULT_NET_CONNECTION_REFUSED;
        case WSANOTINITIALISED: return DD_RESULT_DD_GENERIC_NOT_READY;
        default: return DD_RESULT_UNKNOWN;
    }
}

void ResultEx::SetWin32Error(uint32_t err)
{
    m_result = ResultFromWin32Error(err);
    if (m_result == DD_RESULT_SUCCESS)
    {
        m_osError = 0;
    }
    else
    {
        DD_ASSERT(err <= OS_ERROR_VALUE_MAX);
        m_osError = OS_ERROR_TYPE_BITMASK_WIN32 | err;
    }
}

void ResultEx::CopyWin32ErrorString(uint32_t err, char* pBuf, uint32_t bufSize) const
{
    uint32_t bytesWritten = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_MAX_WIDTH_MASK, 0, err, 0, pBuf, bufSize, 0);
    DD_ASSERT(bytesWritten > 0);
    stbsp_snprintf(pBuf + bytesWritten, bufSize - bytesWritten, "Win32ErrNo: %u.", err);
}

} // namespace DevDriver
