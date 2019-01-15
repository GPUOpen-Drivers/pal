/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palAssert.h"

#if PAL_ENABLE_PRINTS_ASSERTS

namespace Util
{

/// Defines the format of the global assertion control table (g_assertCatTable).
struct AssertTableEntry
{
    bool        enable;   ///< Enable/disable this assertion category.
    const char* pString;  ///< Assertion category name.
};

// Table of default values for each assertion category. This cannot be static since it is used by the PAL_ASSERT and
// PAL_ALERT macros.
AssertTableEntry g_assertCatTable[AssertCatCount] =
{
    // Debug breaks triggered by asserts/alerts are always disabled by default in non-debug builds.
#if PAL_DEBUG_BUILD
    { true,  "Assert" },
#else
    { false,  "Assert" },
#endif
    { false, "Alert"  },
};

// =====================================================================================================================
// Enables/disables the specified assert category.  Controlled by a setting and set during initialization.
void EnableAssertMode(
    AssertCategory category, // Assert category to control (i.e., assert or alert).
    bool           enable)   // Either enables or disables the specified assert category.
{
    PAL_ASSERT(category < AssertCatCount);

    g_assertCatTable[category].enable = enable;
}

// =====================================================================================================================
// Returns true if the specified assert category is currently enabled.
bool IsAssertCategoryEnabled(
    AssertCategory category) // Assert category to check
{
    return g_assertCatTable[category].enable;
}

} // Util

#endif
