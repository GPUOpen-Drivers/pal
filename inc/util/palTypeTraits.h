/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 704
/// Convenience alias to typename std::enable_if::type
/// Deprecated by C++14's std::enable_if_t
template<bool B, typename T = void>
using EnableIf = typename std::enable_if<B, T>::type;

/// Convenience alias to typename std::underlying_type::type
/// Deprecated by C++14's std::underlying_type_t
template<typename T>
using UnderlyingType = typename std::underlying_type<T>::type;
#endif

/// Convenience alias to typename std::integral_constant for the common case where T is bool
/// Will be deprecated by C++17's std::bool_constant
template<bool B>
using BoolConstant = std::integral_constant<bool, B>;

/// Type trait to form a logical negation (NOT) of a type trait.
/// Will be deprecated by C++17's std::negation
template<class B>
struct Negation : BoolConstant<!bool(B::value)>{};

/// Type trait to form a logical conjution (AND) between a sequence of type traits.
/// Will be deprecated by C++17's std::conjunction
template<typename B1, typename... Bn>
struct Conjunction : std::is_same<std::tuple<std::true_type, B1, Bn...>, std::tuple<B1, Bn..., std::true_type>>{};

/// Type trait to form a logical disjunction (OR) between a sequence of type traits.
/// Will be deprecated by C++17's std::disjunction
template<typename B1, typename... Bn>
struct Disjunction : Negation<Conjunction<B1, Bn...>>{};

/// Type trait to determine if a given type is a scoped enum.
/// Will be deprecated by C++23's std::is_scoped_enum.
template<typename E, bool = std::is_enum<E>::value>
struct IsScopedEnum : std::false_type{};
template<typename E>
struct IsScopedEnum<E, true> : public BoolConstant<!std::is_convertible<E, std::underlying_type_t<E>>::value>{};

/// Type trait to determine if two types are the same scoped enums.
template<typename T1, typename T2>
using AreSameScopedEnum = Conjunction<IsScopedEnum<T1>, IsScopedEnum<T2>, std::is_same<T1, T2>>;

/// Type trait to determine if, of two types, one is a scoped enum and one is an integral.
/// It must be one and the other, not both.
template<typename T1, typename T2>
using OneIsScopedEnumOneIsIntegral = Disjunction<Conjunction<IsScopedEnum<T1>, std::is_integral<T2>>,
                                                 Conjunction<IsScopedEnum<T2>, std::is_integral<T1>>>;

/// Casts a scoped enum value to it's underlying type.
/// This is intended to be a shortcut to save from typing the pattern static_cast<underlying_type>.
/// Will be deprecated by C++23's std::to_underlying
///
/// @returns The value of e as it's underlying integer type.
template<typename E,
         std::enable_if_t<std::is_enum<E>::value, int> = 0>
constexpr std::underlying_type_t<E> ToUnderlyingType(E e) noexcept
{
    return static_cast<std::underlying_type_t<E>>(e);
}

/// Helper for the auto enum operators.
/// Template overload for ToUnderlyingType<Enum>() which takes integral types but doesn't change them.
/// With C++17 constexpr-if, this will not be necessary.
///
/// @returns i unchanged
template<typename I,
         std::enable_if_t<std::is_integral<I>::value, int> = 0>
constexpr I ToUnderlyingType(I i) noexcept
{
    return i;
}

/// Bitwise not operator for scoped enums (enum classes).
///
/// @returns the bitwise inverse of the passed enum
template<typename E,
         std::enable_if_t<IsScopedEnum<E>::value, int> = 0>
constexpr E operator~(E rhs) noexcept
{
    return static_cast<E>(~ToUnderlyingType(rhs));
}

/// Bitwise or operator for scoped enums (enum classes).
/// One or both of the operands must be a scoped enum. The other can be any integral type.
///
/// @returns the bitwise or of the two operands. The resulting type is the underlying type of the enum.
template<typename T1,
         typename T2,
         std::enable_if_t<AreSameScopedEnum<T1, T2>::value || OneIsScopedEnumOneIsIntegral<T1, T2>::value, int> = 0>
constexpr auto operator|(T1 lhs, T2 rhs) noexcept -> decltype(ToUnderlyingType(lhs) | ToUnderlyingType(rhs))
{
    return ToUnderlyingType(lhs) | ToUnderlyingType(rhs);
}

/// Bitwise and operator for scoped enums (enum classes).
/// One or both of the operands must be a scoped enum. The other can be any integral type.
///
/// @returns the bitwise and of the two operands. The resulting type is the underlying type of the enum.
template<typename T1,
         typename T2,
         std::enable_if_t<AreSameScopedEnum<T1, T2>::value || OneIsScopedEnumOneIsIntegral<T1, T2>::value, int> = 0>
constexpr auto operator&(T1 lhs, T2 rhs) noexcept -> decltype(ToUnderlyingType(lhs) | ToUnderlyingType(rhs))
{
    return ToUnderlyingType(lhs) & ToUnderlyingType(rhs);
}

/// Bitwise xor operator for scoped enums (enum classes).
/// One or both of the operands must be a scoped enum. The other can be any integral type.
///
/// @returns the bitwise xor of the two operands. The resulting type is the underlying type of the enum.
template<typename T1,
         typename T2,
         std::enable_if_t<AreSameScopedEnum<T1, T2>::value || OneIsScopedEnumOneIsIntegral<T1, T2>::value, int> = 0>
constexpr auto operator^(T1 lhs, T2 rhs) noexcept -> decltype(ToUnderlyingType(lhs) | ToUnderlyingType(rhs))
{
    return ToUnderlyingType(lhs) ^ ToUnderlyingType(rhs);
}

/// Bitwise or-equals operator for scoped enums (enum classes).
///
/// @returns the bitwise or-equals of the two passed enums
template<typename E, std::enable_if_t<IsScopedEnum<E>::value, int> = 0>
constexpr E& operator|=(E& lhs, E rhs) noexcept
{
    return lhs = static_cast<E>(lhs | rhs);
}

/// Bitwise or-equals operator for scoped enums (enum classes).
///
/// @returns the bitwise or-equals of the two passed enums
template<typename E,
         typename T,
         std::enable_if_t<IsScopedEnum<E>::value, int> = 0,
         std::enable_if_t<std::is_integral<T>::value, int> = 0>
constexpr T& operator|=(T& lhs, E rhs) noexcept
{
    return lhs = static_cast<T>(lhs | rhs);
}

/// Bitwise and-equals operator for scoped enums (enum classes).
///
/// @returns the bitwise and-equals of the two passed enums
template<typename E, std::enable_if_t<IsScopedEnum<E>::value, int> = 0>
constexpr E& operator&=(E& lhs, E rhs) noexcept
{
    return lhs = static_cast<E>(lhs & rhs);
}

/// Bitwise and-equals operator for scoped enums (enum classes).
///
/// @returns the bitwise and-equals of the two passed enums
template<typename E,
         typename T,
         std::enable_if_t<IsScopedEnum<E>::value, int> = 0,
         std::enable_if_t<std::is_integral<T>::value, int> = 0>
constexpr T& operator&=(T& lhs, E rhs) noexcept
{
    return lhs = static_cast<T>(lhs & rhs);
}

/// Bitwise xor-equals operator for scoped enums (enum classes).
///
/// @returns the bitwise xor-equals of the two passed enums
template<typename E, std::enable_if_t<IsScopedEnum<E>::value, int> = 0>
constexpr E& operator^=(E& lhs, E rhs) noexcept
{
    return lhs = static_cast<E>(lhs ^ rhs);
}

/// Bitwise xor-equals operator for scoped enums (enum classes).
///
/// @returns the bitwise xor-equals of the two passed enums
template<typename E,
         typename T,
         std::enable_if_t<IsScopedEnum<E>::value, int> = 0,
         std::enable_if_t<std::is_integral<T>::value, int> = 0>
constexpr T& operator^=(T& lhs, E rhs) noexcept
{
    return lhs = static_cast<T>(lhs ^ rhs);
}

/// Bitwise left shift operator for scoped enums (enum classes).
/// One or both of the operands must be a scoped enum. The other can be any integral type.
///
/// @returns the bitwise left shift of the two operands. The resulting type is the underlying type of the enum.
template<typename T1,
         typename T2,
         std::enable_if_t<AreSameScopedEnum<T1, T2>::value || OneIsScopedEnumOneIsIntegral<T1, T2>::value, int> = 0>
constexpr auto operator<<(T1 lhs, T2 rhs) noexcept -> decltype(ToUnderlyingType(lhs) | ToUnderlyingType(rhs))
{
    return ToUnderlyingType(lhs) << ToUnderlyingType(rhs);
}

/// Bitwise right shift operator for scoped enums (enum classes).
/// One or both of the operands must be a scoped enum. The other can be any integral type.
///
/// @returns the bitwise right shift of the two operands. The resulting type is the underlying type of the enum.
template<typename T1,
         typename T2,
         std::enable_if_t<AreSameScopedEnum<T1, T2>::value || OneIsScopedEnumOneIsIntegral<T1, T2>::value, int> = 0>
constexpr auto operator>>(T1 lhs, T2 rhs) noexcept -> decltype(ToUnderlyingType(lhs) | ToUnderlyingType(rhs))
{
    return ToUnderlyingType(lhs) >> ToUnderlyingType(rhs);
}

} // Util
