/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
/**
***********************************************************************************************************************
* @file  ddWinPipeUtil.h
* @brief Utility Functions for Windows Pipes
***********************************************************************************************************************
*/

#pragma once

#include <ddPlatform.h>

namespace DevDriver
{
    inline void LogPipeError(DWORD pipeErrorCode)
    {
        const char* pPipeErrorString = nullptr;

        switch (pipeErrorCode)
        {
            case ERROR_IO_INCOMPLETE:      pPipeErrorString = "IO Incomplete";      break;
            case ERROR_BROKEN_PIPE:        pPipeErrorString = "Broken Pipe";        break;
            case ERROR_OPERATION_ABORTED:  pPipeErrorString = "Operation Aborted";  break;
            case ERROR_PIPE_NOT_CONNECTED: pPipeErrorString = "Pipe Not Connected"; break;
            default: /* Leave the error string as nullptr */                        break;
        }

        if (pPipeErrorString != nullptr)
        {
            DD_PRINT(LogLevel::Warn, "Pipe Error: %s", pPipeErrorString);
        }
        else
        {
            DD_PRINT(LogLevel::Warn, "Pipe Error: Unknown (0x%x)", pipeErrorCode);
        }
    }
}
