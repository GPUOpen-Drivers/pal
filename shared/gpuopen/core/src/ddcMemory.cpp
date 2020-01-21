/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  ddcMemory.cpp
* @brief Implementation for memory utility functions
***********************************************************************************************************************
*/

#include <ddPlatform.h>

using namespace DevDriver;

void* operator new(
    size_t         size,
    const AllocCb& allocCb,
    size_t         align,
    bool           zero,
    const char*    pFilename,
    int            lineNumber,
    const char*    pFunction
) noexcept
{
    void* pMem = nullptr;

    // We do the allocation through DD_MALLOC/DD_CALLOC because they handle extra,
    // platform-specific alignment requirements.
    // Namely, posix expects align >= sizeof(void*).
    if (zero)
    {
        pMem = DD_CALLOC(size, align, allocCb);
    }
    else
    {
        pMem = DD_MALLOC(size, align, allocCb);
    }

    if (pMem != nullptr)
    {
        DD_PRINT(
            LogLevel::Never,
            "Allocated %zu bytes (aligned to %zu, %s) in %s:%d by %s()",
            size,
            align,
            (zero ? "zeroed" : "uninitialized"),
            pFilename,
            lineNumber,
            pFunction
        );
    }
    else
    {
        DD_PRINT(
            LogLevel::Error,
            "Failed to allocate %zu bytes (aligned to %zu, %s) in %s:%d by %s()",
            size,
            align,
            (zero ? "zeroed" : "uninitialized"),
            pFilename,
            lineNumber,
            pFunction
        );
    }

    return pMem;
}

// Nothing should call this directly. The compiler only wants this available so that it can call it when an exception
// is thrown. We live in a wonderful world without exceptions, so we don't care about that exceptional case.
void operator delete(
    void*          , // pObject
    const AllocCb& , // allocCb
    size_t         , // align
    bool           , // zero
    const char*    , // pFilename
    int            , // lineNumber
    const char*      // pFunction
) noexcept
{
    DD_WARN_REASON(
        "If you're reading this, you're the first person to see this function called. "
        "Please evaluate how that happened and then possibly implement this function. "
        "Best guess? Your constructor threw and the compiler is trying to free the allocation.");
    DD_ASSERT_ALWAYS();
}
