/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

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
