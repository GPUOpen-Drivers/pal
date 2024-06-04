/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palTypeTraits.h
 * @brief PAL utility collection of compile-time templated C++ type traits
 ***********************************************************************************************************************
 */

#pragma once

#include <tuple>
#include <type_traits>

namespace Util
{

/// Type trait to determine if a given type is a scoped enum.
/// Will be deprecated by C++23's std::is_scoped_enum.
template<typename E, bool = std::is_enum<E>::value>
struct IsScopedEnum : std::false_type{};
template<typename E>
struct IsScopedEnum<E, true> : public std::bool_constant<!std::is_convertible<E, std::underlying_type_t<E>>::value>{};

/// Type trait to determine if a given type is an unscoped enum.
template<typename E>
using IsUnscopedEnum = std::conjunction<std::is_enum<E>, std::negation<IsScopedEnum<E>>>;

/// Type trait to determine if the given type is an enum or integral.
template<typename T>
using IsEnumOrIntegral = std::disjunction<std::is_enum<T>, std::is_integral<T>>;

/// Type trait to determine if at least one of the given types is an enum.
template<typename T1, typename T2>
using OneIsEnum = std::disjunction<std::is_enum<T1>, std::is_enum<T2>>;

/// Type trait to determine if both of the given types are an enum or integral.
template<typename T1, typename T2>
using BothEnumOrIntegral = std::conjunction<std::disjunction<std::is_enum<T1>, std::is_integral<T1>>,
                                            std::disjunction<std::is_enum<T2>, std::is_integral<T2>>>;

/// Type trait to determine if at least one of the given types is an enum. The other may be an enum or integral.
template<typename T1, typename T2>
using OneIsEnumOtherIsEnumOrIntegral = std::conjunction<OneIsEnum<T1, T2>, BothEnumOrIntegral<T1, T2>>;

/// Casts a scoped enum value to it's underlying type.
/// This is intended to be a shortcut to save from typing the pattern static_cast<underlying_type>.
/// Will be deprecated by C++23's std::to_underlying
///
/// @returns The value of e as it's underlying integer type.
template<typename E>
constexpr auto ToUnderlyingType(E e) noexcept
{
    if constexpr (std::is_enum_v<E>)
    {
        return std::underlying_type_t<E>(e);
    }
    else
    {
        return e;
    }
}

/// Bitwise not operator for scoped enums (enum classes).
///
/// @returns the bitwise inverse of the passed enum
template<typename E,
         std::enable_if_t<IsScopedEnum<E>::value, int> = 0>
constexpr E operator~(E r) noexcept
{
    return E(~ToUnderlyingType(r));
}

/// Bitwise or operator for enums.
///
/// @returns the bitwise or of the two operands. The resulting type is the underlying type of the enum.
template<typename L,
         typename R,
         std::enable_if_t<OneIsEnumOtherIsEnumOrIntegral<L, R>::value, int> = 0>
constexpr auto operator|(L l, R r) noexcept -> decltype(ToUnderlyingType(l) | ToUnderlyingType(r))
{
    // We need to cast them to the same type to avoid signed/unsigned mismatch.
    using UnderlyingType = decltype(ToUnderlyingType(l) | ToUnderlyingType(r));
    return UnderlyingType(l) | UnderlyingType(r);
}

/// Bitwise and operator for enums.
///
/// @returns the bitwise and of the two operands. The resulting type is the underlying type of the enum.
template<typename L,
         typename R,
         std::enable_if_t<OneIsEnumOtherIsEnumOrIntegral<L, R>::value, int> = 0>
constexpr auto operator&(L l, R r) noexcept -> decltype(ToUnderlyingType(l) | ToUnderlyingType(r))
{
    // We need to cast them to the same type to avoid signed/unsigned mismatch.
    using UnderlyingType = decltype(ToUnderlyingType(l) | ToUnderlyingType(r));
    return UnderlyingType(l) & UnderlyingType(r);
}

/// Bitwise xor operator for enums.
///
/// @returns the bitwise xor of the two operands. The resulting type is the underlying type of the enum.
template<typename L,
         typename R,
         std::enable_if_t<OneIsEnumOtherIsEnumOrIntegral<L, R>::value, int> = 0>
constexpr auto operator^(L l, R r) noexcept -> decltype(ToUnderlyingType(l) | ToUnderlyingType(r))
{
    // We need to cast them to the same type to avoid signed/unsigned mismatch.
    using UnderlyingType = decltype(ToUnderlyingType(l) | ToUnderlyingType(r));
    return UnderlyingType(l) ^ UnderlyingType(r);
}

/// Bitwise or-equals operator for enums.
///
/// @returns the bitwise or-equals of the two passed values
template<typename L,
         typename R,
         std::enable_if_t<std::conjunction<std::is_enum<L>, IsEnumOrIntegral<R>>::value, int> = 0>
constexpr L& operator|=(L& l, R r) noexcept
{
    // We need to cast them to the same type to avoid signed/unsigned mismatch.
    using UnderlyingType = decltype(ToUnderlyingType(l) | ToUnderlyingType(r));
    return l = L(UnderlyingType(l) | UnderlyingType(r));
}

/// Bitwise and-equals operator for enums.
///
/// @returns the bitwise and-equals of the two passed values
template<typename L,
         typename R,
         std::enable_if_t<std::conjunction<std::is_enum<L>, IsEnumOrIntegral<R>>::value, int> = 0>
constexpr L& operator&=(L& l, R r) noexcept
{
    // We need to cast them to the same type to avoid signed/unsigned mismatch.
    using UnderlyingType = decltype(ToUnderlyingType(l) | ToUnderlyingType(r));
    return l = L(UnderlyingType(l) & UnderlyingType(r));
}

/// Bitwise xor-equals operator for enums.
///
/// @returns the bitwise xor-equals of the two passed values
template<typename L,
         typename R,
         std::enable_if_t<std::conjunction<std::is_enum<L>, IsEnumOrIntegral<R>>::value, int> = 0>
constexpr L& operator^=(L& l, R r) noexcept
{
    // We need to cast them to the same type to avoid signed/unsigned mismatch.
    using UnderlyingType = decltype(ToUnderlyingType(l) | ToUnderlyingType(r));
    return l = L(UnderlyingType(l) ^ UnderlyingType(r));
}

/// Bitwise left shift operator for enums.
///
/// @returns the bitwise left shift of the two operands. The resulting type is the underlying type of the enum.
template<typename L,
         typename R,
         std::enable_if_t<OneIsEnumOtherIsEnumOrIntegral<L, R>::value, int> = 0>
constexpr auto operator<<(L l, R r) noexcept -> decltype(ToUnderlyingType(l) | ToUnderlyingType(r))
{
    // We need to cast them to the same type to avoid signed/unsigned mismatch.
    using UnderlyingType = decltype(ToUnderlyingType(l) | ToUnderlyingType(r));
    return UnderlyingType(l) << UnderlyingType(r);
}

/// Bitwise right shift operator for enums.
///
/// @returns the bitwise right shift of the two operands. The resulting type is the underlying type of the enum.
template<typename L,
         typename R,
         std::enable_if_t<OneIsEnumOtherIsEnumOrIntegral<L, R>::value, int> = 0>
constexpr auto operator>>(L l, R r) noexcept -> decltype(ToUnderlyingType(l) | ToUnderlyingType(r))
{
    // We need to cast them to the same type to avoid signed/unsigned mismatch.
    using UnderlyingType = decltype(ToUnderlyingType(l) | ToUnderlyingType(r));
    return UnderlyingType(l) >> UnderlyingType(r);
}

/// Comparison operator for enums.
template<typename L,
         typename R,
         std::enable_if_t<std::conjunction<std::is_enum<L>, std::is_enum<R>>::value, int> = 0>
constexpr bool operator==(L l, R r) noexcept
{
    // We need to cast them to the same type to avoid signed/unsigned mismatch.
    using UnderlyingType = decltype(ToUnderlyingType(l) | ToUnderlyingType(r));
    return UnderlyingType(l) == UnderlyingType(r);
}

/// Comparison operator for enums.
template<typename L,
         typename R,
         std::enable_if_t<std::conjunction<std::is_enum<L>, std::is_enum<R>>::value, int> = 0>
constexpr bool operator!=(L l, R r) noexcept
{
    // We need to cast them to the same type to avoid signed/unsigned mismatch.
    using UnderlyingType = decltype(ToUnderlyingType(l) | ToUnderlyingType(r));
    return UnderlyingType(l) != UnderlyingType(r);
}

/// Addition operator for enums.
///
/// @returns the addition of the two operands. The resulting type is the underlying type of the enum.
template<typename L,
         typename R,
         std::enable_if_t<OneIsEnumOtherIsEnumOrIntegral<L, R>::value, int> = 0>
constexpr auto operator+(L l, R r) noexcept -> decltype(ToUnderlyingType(l) + ToUnderlyingType(r))
{
    // We need to cast them to the same type to avoid signed/unsigned mismatch.
    using UnderlyingType = decltype(ToUnderlyingType(l) + ToUnderlyingType(r));
    return UnderlyingType(l) + UnderlyingType(r);
}

/// Subtraction operator for enums.
///
/// @returns the subtraction of the two operands. The resulting type is the underlying type of the enum.
template<typename L,
         typename R,
         std::enable_if_t<OneIsEnumOtherIsEnumOrIntegral<L, R>::value, int> = 0>
constexpr auto operator-(L l, R r) noexcept -> decltype(ToUnderlyingType(l) - ToUnderlyingType(r))
{
    // We need to cast them to the same type to avoid signed/unsigned mismatch.
    using UnderlyingType = decltype(ToUnderlyingType(l) - ToUnderlyingType(r));
    return UnderlyingType(l) - UnderlyingType(r);
}

/// Pre-increment operator for enums.
///
/// @returns enum after the increment.
template<typename E,
         std::enable_if_t<std::is_enum<E>::value, int> = 0>
constexpr E operator++(E& e) noexcept
{
    e = E(e + 1);
    return e;
}

/// Post-increment operator for enums.
///
/// @returns enum before the increment.
template<typename E,
    std::enable_if_t<std::is_enum<E>::value, int> = 0>
constexpr E operator++(E& e, int) noexcept
{
    const E ret = e;
    ++e;
    return ret;
}

/// Pre-decrement operator for enums.
///
/// @returns enum after the decrement.
template<typename E,
    std::enable_if_t<std::is_enum<E>::value, int> = 0>
constexpr E operator--(E& e) noexcept
{
    e = E(e - 1);
    return e;
}

/// Post-decrement operator for enums.
///
/// @returns enum before the decrement.
template<typename E,
    std::enable_if_t<std::is_enum<E>::value, int> = 0>
constexpr E operator--(E& e, int) noexcept
{
    const E ret = e;
    --e;
    return ret;
}
} // Util
