/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  ddcDefs.h
* @brief Developer Driver Definitions Header
***********************************************************************************************************************
*/

#pragma once

#include <stdint.h>
#include <stddef.h>

// Macros for conditional language support.
    #define DD_CPLUSPLUS __cplusplus
// Denotes versions of the C++ standard from __cplusplus.
// See here for details on what values you can expect:
//      https://en.cppreference.com/w/cpp/preprocessor/replace
#define CPP98 (199711L)
#define CPP11 (201103L)
#define CPP14 (201402L)
#define CPP17 (201703L)
#define DD_CPLUSPLUS_SUPPORTS(x) (DD_CPLUSPLUS >= (x))

static_assert(DD_CPLUSPLUS_SUPPORTS(CPP11), "C++11 is required to build devdriver.");

#if !defined(DD_STATIC_CONST)
    #if defined(__cplusplus) && __cplusplus >= 201103L
        #define DD_STATIC_CONST static constexpr
    #else
        #define DD_STATIC_CONST static const
    #endif
#endif

#if !defined(DD_ALIGNAS)
    #if defined(__cplusplus) && __cplusplus >= 201103L
        #define DD_ALIGNAS(x) alignas(x)
    #else
        static_assert(false, "Error: unsupported compiler detected. Support is required to build.");
    #endif
#endif

// Creates a structure with the specified name and alignment.
#define DD_ALIGNED_STRUCT(name, alignment) struct DD_ALIGNAS(alignment) name

// Creates a structure with the specified alignment, and mark it as final to ensure it cannot be used as a parent class
#define DD_NETWORK_STRUCT(name, alignment) struct DD_ALIGNAS(alignment) name final

// This is disabled by default because (1) it's horribly hacky and (2) doesn't work very well in some compilers.
#if DEVDRIVER_ENABLE_VERBOSE_STATIC_ASSERTS
    #include <type_traits>

    // Conditionally expose a `value` member using SFINAE and `std::enable_if`.
    // For a more complete overview, see:
    // [C++]
    //      https://en.cppreference.com/w/cpp/types/enable_if
    // tl:dr; If left != right, prints an error message that includes the template type.
    //          We only do this because static_assert can't take format arguments.
    template <size_t left, size_t right>
    struct CheckEqualActualVsExpected
    {
        // If template arguments `left` and `right` are not equal, the `enable_if_t` type will fail to resolve, causing an error at the call-site.
        // When the sizes are equal, this is a regular and boring struct with a single static member that's always `true`.
        using Bool = typename std::enable_if_t<(left == right), bool>;
        static constexpr Bool value = true;
    };

    // In our case, the call-site is in this macro. The error message will print the entire type that failed to resolve,
    // e.g. CheckEqualActualVsExpected<12, 8> -- this means that the actual size was 12, but the expected size is 8.
    // This has no effect if the sizes are equal: the template type here resolves and `value` evaluates as `true`, passing the static_assert.
    #define DD_CHECK_SIZE(typeA, expectedSize) \
        static_assert(                         \
            CheckEqualActualVsExpected<        \
                sizeof(typeA),                 \
                (expectedSize)                 \
            >::value,                          \
            ""                                 \
        );
#else
    #define DD_CHECK_SIZE(x, size) static_assert(sizeof(x) == size_t(size), "sizeof(" # x ") should be " # size " bytes but has changed recently")
#endif

#define DD_UNUSED(x) (static_cast<void>(x))

#define _DD_STRINGIFY(str) #str
#define DD_STRINGIFY(x) _DD_STRINGIFY(x)

#if DD_CPLUSPLUS_SUPPORTS(CPP17)
    #define DD_NODISCARD [[nodiscard]]
#else
    #define DD_NODISCARD
#endif

// Include in the private section of a class declaration in order to disallow use of the copy and assignment operator
#define DD_DISALLOW_COPY_AND_ASSIGN(_typename) \
    _typename(const _typename&);               \
    _typename& operator =(const _typename&);

// Include in the private section of a class declaration in order to disallow use of the default constructor
#define DD_DISALLOW_DEFAULT_CTOR(_typename)   \
    _typename();

// Detect the CPU architecture for the target.
// These are often evaluated during the preprocessor stage, so it's important that we don't rely on things like sizeof.
#if   UINTPTR_MAX == 0xFFFFFFFF
    #define DEVDRIVER_ARCHITECTURE_BITS 32
#elif UINTPTR_MAX == 0xFFFFFFFFFFFFFFFF
    #define DEVDRIVER_ARCHITECTURE_BITS 64
#else
    static_assert(false, "Unknown or unsupported target architecture.");
#endif
static_assert(DEVDRIVER_ARCHITECTURE_BITS == (8 * sizeof(void*)), // Assume 8-bits-per-byte.
             "DEVDRIVER_ARCHITECTURE_BITS does not match sizeof(void*).");

#define DD_BUILD_32 (DEVDRIVER_ARCHITECTURE_BITS == 32)
#define DD_BUILD_64 (DEVDRIVER_ARCHITECTURE_BITS == 64)

// Common Typedefs
// These types are shared between all platforms,
// and need to be defined before including a specific platform header.

namespace DevDriver
{

typedef int8_t   int8;    ///< 8-bit integer.
typedef int16_t  int16;   ///< 16-bit integer.
typedef int32_t  int32;   ///< 32-bit integer.
typedef int64_t  int64;   ///< 64-bit integer.
typedef uint8_t  uint8;   ///< Unsigned 8-bit integer.
typedef uint16_t uint16;  ///< Unsigned 16-bit integer.
typedef uint32_t uint32;  ///< Unsigned 32-bit integer.
typedef uint64_t uint64;  ///< Unsigned 64-bit integer.

typedef uint32_t ProcessId;
typedef uint32_t Size;
typedef uint64_t Handle;

DD_STATIC_CONST Handle kNullPtr = 0;
DD_STATIC_CONST Handle kInvalidHandle = 0;

////////////////////////////
// Common result codes
enum struct Result : uint32
{
    //// Generic Result Code  ////
    Success = 0,
    Error = 1,
    NotReady = 2,
    VersionMismatch = 3,
    Unavailable = 4,
    Rejected = 5,
    EndOfStream = 6,
    Aborted = 7,
    InsufficientMemory = 8,
    InvalidParameter = 9,
    InvalidClientId = 10,

    //// URI PROTOCOL  ////
    UriServiceRegistrationError = 1000,
    UriStringParseError = 1001,
    UriInvalidParameters = 1002,
    UriInvalidPostDataBlock = 1003,
    UriInvalidPostDataSize = 1004,
    UriFailedToAcquirePostBlock = 1005,
    UriFailedToOpenResponseBlock = 1006,
    UriRequestFailed = 1007,
    UriPendingRequestError = 1008,
    UriInvalidChar = 1009,
    UriInvalidJson = 1010,

    //// Settings URI Service  ////
    SettingsUriInvalidComponent = 2000,
    SettingsUriInvalidSettingName = 2001,
    SettingsUriInvalidSettingValue = 2002,
    SettingsUriInvalidSettingValueSize = 2003,

    //// Info URI Service ////
    InfoUriSourceNameInvalid = 3000,
    InfoUriSourceCallbackInvalid = 3001,
    InfoUriSourceAlreadyRegistered = 3002,
    InfoUriSourceWriteFailed = 3003,
};

} // namespace DevDriver
