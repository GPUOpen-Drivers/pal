/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

// palOptional.h : PAL util analog of std::optional

#pragma once

#include "palAssert.h"
#include "palUtil.h"
#include <optional>

namespace Util
{

/**
 ***********************************************************************************************************************
 * @brief Optional container
 *
 * Optional is a container for another type, where the value is optionally present. It is based on C++17 std::optional,
 * but removing its exception-throwing behavior.
 *
 ***********************************************************************************************************************
 */
template<typename T> class Optional : public std::optional<T>
{
public:
    // Default constructor.
    constexpr Optional() : std::optional<T>() {}

    // Constructor given bare value.
    template<typename InputT = T> constexpr Optional(InputT&& input) : std::optional<T>(input) {}

    // HasValue : has_value renamed to fit PAL style.
    constexpr bool HasValue() const { return std::optional<T>::has_value(); }

    // Value : value renamed to fit PAL style and avoid exception. We need to avoid an exception even
    // on a release build, so we make it trap instead.
    constexpr const T& Value() const&
    {
        if (HasValue() == false)
        {
            PAL_ASSERT_ALWAYS();
        }
        return std::optional<T>::value();
    }
    constexpr T& Value()&
    {
        if (HasValue() == false)
        {
            PAL_ASSERT_ALWAYS();
        }
        return std::optional<T>::value();
    }
    constexpr T&& Value()&&
    {
        if (HasValue() == false)
        {
            PAL_ASSERT_ALWAYS();
        }
        return std::optional<T>::value();
    }

    // value : deleted to avoid accidental exception-generating use
    constexpr T& value()& = delete;

    // ValueOr : value_or renamed to fit PAL style.
    template<typename DefaultT> constexpr T ValueOr(DefaultT&& defaultVal) const&
        { return std::optional<T>::value_or(defaultVal); }
    template<typename DefaultT> constexpr T ValueOr(DefaultT&& defaultVal)&&
        { return std::optional<T>::value_or(defaultVal); }

    // Reset : reset renamed to fit PAL style.
    void Reset() { std::optional<T>::reset(); }

    // Implicit conversion to bool deleted to avoid client code accidentally using it on an Optional<bool>
    // thinking that it tests the embedded bool value.
    operator bool() const = delete;
    operator bool() = delete;
};

} // Util
