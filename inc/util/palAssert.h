/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palAssert.h
 * @brief PAL utility collection assert macros.
 ***********************************************************************************************************************
 */

#pragma once

#include "palDbgPrint.h"
#include "palUtil.h"

#include <signal.h>

/// OS-independent macro to force a break into the debugger.
#define PAL_DEBUG_BREAK() raise(SIGTRAP);

#if PAL_HAS_BUILTIN(__builtin_expect) || (defined(__GNUC__) && !defined(__clang__))
/// Informs the compiler to assume that the given expression likely evaluates to true, and returns that expression.
#define PAL_PREDICT_TRUE(expr) __builtin_expect(!!(expr), 1)
/// Informs the compiler to assume that the given expression likely evaluates to false, and returns that expression.
#define PAL_PREDICT_FALSE(expr) __builtin_expect((expr), 0)
#else
#define PAL_PREDICT_TRUE(expr) (expr)
#define PAL_PREDICT_FALSE(expr) (expr)
#endif

/// OS-independent macro to direct static code analysis to assume the specified expression will always be true.
#define PAL_ANALYSIS_ASSUME(expr) static_cast<void>(PAL_PREDICT_TRUE(expr))

namespace Util
{

/// A helper function to check the size-in-bits of a 'reserved' member in a bitfield.
/// This is intended for use with static_asserts to ensure things don't go out-of-sync.
///
/// @param [in] expectedTotalBitWidth  Number of bits expected in the whole type
/// @param [in] expectedReservedBits   Number of bits in the 'reserved' field
///
/// @return true if the bit lengths of the type T match the values in the args.
///         true if the compiler lacks support to do this at compile time.
///
/// @note This may not work properly with old compilers, but this is meant for linting anyhow.
template <typename T>
constexpr bool CheckReservedBits(
    uint32 expectedTotalBitWidth,
    uint32 expectedReservedBits)
{
#if PAL_CPLUSPLUS_AT_LEAST(PAL_CPLUSPLUS_14) || (defined(__cpp_constexpr) && (__cpp_constexpr >= 201304))
    bool match = false;

    // Fail if the whole size is different
    if (sizeof(T) * 8 == expectedTotalBitWidth)
    {
        // Get the width of the reserved field by detecting when it stops filling bits
        T      sample       = {};
        uint64 mask         = 0;
        uint32 reservedBits = 0;
        do
        {
            sample = {};
            mask   = (mask << 1) | 1;
            reservedBits++;
            sample.reserved = mask;
        } while ((sample.reserved == mask) && (reservedBits < sizeof(T) * 8));
        // when the loop terminates, it's one past the size of the field.
        match = (reservedBits - 1) == expectedReservedBits;
    }
    return match;
#else
    // C++11 lacks support for doing anything useful with constexpr
    return true;
#endif
}

#if PAL_ENABLE_PRINTS_ASSERTS

/// Specifies how severe an triggered assert (or alert) is.
///
/// Both asserts and alerts can print out a debug string and break into the debugger.  Asserts are to be used to verify
/// the known, assumed state of the program at any time.  Alerts are to be used to notify the developer of a _possible_,
/// but unexpected condition such as memory allocation failure, an OS call failure, or an application behavior that is
/// known to be slow.
enum AssertCategory : uint32
{
    AssertCatAssert = 0,
    AssertCatAlert,
    AssertCatCount
};

/// Enables/disables the specified assert category.
///
/// Probably controlled by a setting and set during initialization.
///
/// @param [in] category  Assert category to enable/disable (asserts or alerts).
/// @param [in] enable    True to enable the specified assert category, false to disable it.
extern void EnableAssertMode(
    AssertCategory category,
    bool           enable);

/// Returns true if the specified assert category is enabled and false otherwise.
///
/// @param [in] category  Assert category to check
extern bool IsAssertCategoryEnabled(
    AssertCategory category);

#endif

} // namespace Util

#if PAL_ENABLE_PRINTS_ASSERTS
/// If the expression evaluates to false, then an error message with the specified reason will be printed via the
/// debug print system. A debug break will also be triggered if they're currently enabled for asserts.
///
/// @note This version of assert inlines an 'int 3' every time it is used so that each occurrence can be zapped
///       independently.  This macro cannot be used in assignment operations.
#define PAL_TRIGGER_ASSERT(_pFormat, ...)                     \
{                                                             \
    PAL_DPERROR(_pFormat, ##__VA_ARGS__);                     \
    if (Util::IsAssertCategoryEnabled(Util::AssertCatAssert)) \
    {                                                         \
        PAL_DEBUG_BREAK();                                    \
    }                                                         \
}

#define PAL_ASSERT_MSG(_expr, _pReasonFmt, ...)                                                   \
{                                                                                                 \
    bool _expr_eval = static_cast<bool>(_expr);                                                   \
    if (PAL_PREDICT_FALSE(_expr_eval == false))                                                   \
    {                                                                                             \
        PAL_TRIGGER_ASSERT("Assertion failed: %s | Reason: " _pReasonFmt, #_expr, ##__VA_ARGS__); \
    }                                                                                             \
    PAL_ANALYSIS_ASSUME(_expr_eval);                                                              \
}

/// Calls the PAL_ASSERT_MSG macro with a generic reason string
#define PAL_ASSERT(_expr) PAL_ASSERT_MSG(_expr, "%s", "Unknown")

/// Debug build only PAL assert, the typical usage is when make an assertion on a debug-only variables.
/// The only difference than PAL assert is it's empty in release mode.
#define PAL_DEBUG_BUILD_ONLY_ASSERT(_expr) \
{                                          \
    PAL_ASSERT(_expr);                     \
}

/// If the expression evaluates to true, then a warning message with the specified reason will be printed via the
/// debug print system. A debug break will also be triggered if they're currently enabled for alerts.
///
/// @note This is the opposite polarity of asserts.  The assert macro _asserts_ that the specified condition is true.
///       While the alert macro _alerts_ the developer if the specified condition is true.
///
/// This macro should be used in places where an assert is inappropriate because an error condition is _possible_, but
/// not typically expected.  For example, asserting that an OS call succeeded should be avoided since there cannot be an
/// assumption that it will succeed.  Nonetheless, a developer may want to be alerted immediately and dropped into the
/// debugger when such a failure occurs.
#define PAL_TRIGGER_ALERT(_pFormat, ...)                     \
{                                                            \
    PAL_DPWARN(_pFormat, ##__VA_ARGS__);                     \
    if (Util::IsAssertCategoryEnabled(Util::AssertCatAlert)) \
    {                                                        \
        PAL_DEBUG_BREAK();                                   \
    }                                                        \
}

#define PAL_ALERT_MSG(_expr, _pReasonFmt, ...)                                                  \
{                                                                                               \
    if (PAL_PREDICT_FALSE(_expr))                                                               \
    {                                                                                           \
        PAL_TRIGGER_ALERT("Alert triggered: %s | Reason: " _pReasonFmt, #_expr, ##__VA_ARGS__); \
    }                                                                                           \
}

/// Calls the PAL_ALERT_MSG macro with a generic reason string
#define PAL_ALERT(_expr) PAL_ALERT_MSG(_expr, "%s", "Unknown")

/// Convenience macro that asserts if something has never been tested.
#define PAL_NOT_TESTED_MSG(_pReasonFmt, ...) PAL_TRIGGER_ASSERT("Code Not Tested! | Reason: " _pReasonFmt, ##__VA_ARGS__)
#define PAL_NOT_TESTED() PAL_NOT_TESTED_MSG("%s", "Unknown")

/// Convenience macro that asserts if something has not been implemented.
#define PAL_NOT_IMPLEMENTED_MSG(_pReasonFmt, ...) PAL_TRIGGER_ASSERT("Not Implemented! | Reason: " _pReasonFmt, ##__VA_ARGS__)
#define PAL_NOT_IMPLEMENTED() PAL_NOT_IMPLEMENTED_MSG("%s", "Unknown")

/// Convenience macro that asserts if an area of code that shouldn't be executed is reached.
#define PAL_NEVER_CALLED_MSG(_pReasonFmt, ...) PAL_TRIGGER_ASSERT("Code should never be called! | Reason: " _pReasonFmt, ##__VA_ARGS__)
#define PAL_NEVER_CALLED() PAL_NEVER_CALLED_MSG("%s", "Unknown")

/// Convenience macro that always asserts.  Expect this to be used instead of PAL_ASSERT(false).
#define PAL_ASSERT_ALWAYS_MSG(_pReasonFmt, ...) PAL_TRIGGER_ASSERT("Unconditional Assert | Reason: " _pReasonFmt, ##__VA_ARGS__)
#define PAL_ASSERT_ALWAYS() PAL_ASSERT_ALWAYS_MSG("%s", "Unknown")

/// Convenience macro that always alerts.  Expect this to be used instead of PAL_ALERT(true).
#define PAL_ALERT_ALWAYS_MSG(_pReasonFmt, ...) PAL_TRIGGER_ALERT("Unconditional Alert | Reason: " _pReasonFmt, ##__VA_ARGS__)
#define PAL_ALERT_ALWAYS() PAL_ALERT_ALWAYS_MSG("%s", "Unknown")

#else

#define PAL_ASSERT(_expr)                  PAL_ANALYSIS_ASSUME(_expr)
#define PAL_ASSERT_MSG(_expr, ...)         PAL_ANALYSIS_ASSUME(_expr)
#define PAL_DEBUG_BUILD_ONLY_ASSERT(_expr) ((void)0)
#define PAL_ALERT(_expr)                   ((void)0)
#define PAL_ALERT_MSG(_expr, ...)          ((void)0)
#define PAL_NOT_TESTED()                   ((void)0)
#define PAL_NOT_TESTED_MSG(...)            ((void)0)
#define PAL_NOT_IMPLEMENTED()              ((void)0)
#define PAL_NOT_IMPLEMENTED_MSG(...)       ((void)0)
#define PAL_NEVER_CALLED()                 ((void)0)
#define PAL_NEVER_CALLED_MSG(...)          ((void)0)
#define PAL_ASSERT_ALWAYS()                ((void)0)
#define PAL_ASSERT_ALWAYS_MSG(...)         ((void)0)
#define PAL_ALERT_ALWAYS()                 ((void)0)
#define PAL_ALERT_ALWAYS_MSG(...)          ((void)0)

#endif

