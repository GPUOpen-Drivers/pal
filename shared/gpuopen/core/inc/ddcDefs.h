/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include <ddLegacyDefs.h>

#include <stdint.h>
#include <stddef.h>

// Macros for conditional language support.
#ifdef _MSVC_LANG
    #define DD_CPLUSPLUS _MSVC_LANG
#else
    #define DD_CPLUSPLUS __cplusplus
#endif
// Denotes versions of the C++ standard from __cplusplus.
// See here for details on what values you can expect:
#define CPP98 (199711L)
#define CPP11 (201103L)
#define CPP14 (201402L)
#define CPP17 (201703L)
#define CPP20 (202002L)
#define DD_CPLUSPLUS_SUPPORTS(x) (DD_CPLUSPLUS >= (x))

static_assert(DD_CPLUSPLUS_SUPPORTS(CPP11), "C++11 is required to build devdriver.");

#if defined(_MSC_VER)
    #define DD_ALIGNAS(x)__declspec(align(x))
    #if _MSC_VER < 1900
        #define DD_STATIC_CONST static const
    #else
        #define DD_STATIC_CONST static constexpr
    #endif
#endif

#if !defined(DD_STATIC_CONST)
    #if defined(__cplusplus) && __cplusplus >= 201103L
        #define DD_STATIC_CONST static constexpr
    #else
        #define DD_STATIC_CONST static const
    #endif
#endif

#if DD_CPLUSPLUS_SUPPORTS(CPP14)
    #define DD_CPP14_CONSTEXPR_FN constexpr
    #define DD_CPP14_STATIC_ASSERT(a, b) static_assert(a, b)
#else
    #define DD_CPP14_CONSTEXPR_FN inline
    #define DD_CPP14_STATIC_ASSERT(a, b)
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
#if DD_OPT_VERBOSE_STATIC_ASSERTS
    #include <type_traits>

    // Conditionally expose a `value` member using SFINAE and `std::enable_if`.
    // For a more complete overview, see:
    // [C++]
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
    // Require that a function's return value, or an entire type, be used.
    #define DD_NODISCARD    [[nodiscard]]

    // Do not warn about switch statement cases falling through.  Place this macro as the case body, e.g.
    //  switch (x)
    //  {
    //      case 0: DD_FALLTHROUGH();
    //      case 1: DD_FALLTHROUGH();
    //      case 2:
    //          printf("0, 1, or 2");
    //          break;
    //  }
    //
    #define DD_FALLTHROUGH()  [[fallthrough]]
#else
    // Require that a function's return value, or an entire type, be used.
    // This option is aggressive enough that we do not enable it when C++17 is not enabled
    #define DD_NODISCARD

    // Do not warn about switch statement cases falling through.  Place this macro as the case body, e.g.
    //  switch (x)
    //  {
    //      case 0: DD_FALLTHROUGH();
    //      case 1: DD_FALLTHROUGH();
    //      case 2:
    //          printf("0, 1, or 2");
    //          break;
    //  }
    //
    #if defined(__clang__)
        #define DD_FALLTHROUGH() [[clang::fallthrough]]
    #elif defined(__GNUC__)
        #if __GNUC__ >= 7
            // gnu::fallthrough isn't supported until GCC 7+
            #define DD_FALLTHROUGH() [[gnu::fallthrough]]
        #else
            // Not supported on older versions of GCC
            #define DD_FALLTHROUGH()
        #endif
    #elif defined(_MSC_VER)
        // Not supported on MSVC - who doesn't warn about this issue in the first place.
        #define DD_FALLTHROUGH()
    #else
        // We don't know what compiler this is, so just no-op the macro.
        #define DD_FALLTHROUGH()
    #endif
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

// Add a detailed function name macro
// These vary across platforms, so we'll just pick the first one that's defined
#if defined(__FUNCSIG__)
    #define DD_FUNCTION_NAME __FUNCSIG__
#elif defined(__PRETTY_FUNCTION__)
    #define DD_FUNCTION_NAME __PRETTY_FUNCTION__
#else
    #define DD_FUNCTION_NAME __FUNCTION__
#endif

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
    ConnectionExists = 11,
    FileNotFound = 12,
    FunctionNotFound = 13,
    InterfaceNotFound = 14,
    EntryExists = 15,
    FileAccessError = 16,
    FileIoError = 17,
    LimitReached = 18,

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
