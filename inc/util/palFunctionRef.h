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
// FunctionRef: A non-owning reference to a callable object, such as a lambda.

// This implementation is extracted from LLVM:
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <cstdint>
#include <type_traits>
#include <utility>

namespace Util
{

// =====================================================================================================================
// Features from C++20
template <typename T>
struct RemoveCvref
{
  using Type = std::remove_cv_t<std::remove_reference_t<T>>;
};

template <typename T> using RemoveCvrefT = typename Util::RemoveCvref<T>::Type;

// =====================================================================================================================
/// An efficient, type-erasing, non-owning reference to a callable. This is
/// intended for use as the type of a function parameter that is not used
/// after the function in question returns.
///
/// This class does not own the callable, so it is not in general safe to store
/// a FunctionRef.
template<typename Fn> class FunctionRef;

template<typename Ret, typename ...Params>
class FunctionRef<Ret(Params...)>
{
    Ret (*m_callback)(intptr_t callable, Params ...params) = nullptr;
    intptr_t m_callable;

    template<typename Callable>
    static Ret callback_fn(intptr_t callable, Params ...params)
    {
        return (*reinterpret_cast<Callable*>(callable))(std::forward<Params>(params)...);
    }

public:
    FunctionRef() = default;
    FunctionRef(std::nullptr_t) {}

    template <typename Callable>
    FunctionRef(
        Callable &&callable,
        // This is not the copy-constructor.
        std::enable_if_t<!std::is_same<RemoveCvrefT<Callable>, FunctionRef>::value> * = nullptr,
        // Functor must be callable and return a suitable type.
        std::enable_if_t<std::is_void<Ret>::value ||
                         std::is_convertible<decltype(std::declval<Callable>()(std::declval<Params>()...)),
                                             Ret>::value> * = nullptr
        )
        : m_callback(callback_fn<std::remove_reference_t<Callable>>),
          m_callable(reinterpret_cast<intptr_t>(&callable)) {}

    Ret operator()(Params ...params) const
    {
        return m_callback(m_callable, std::forward<Params>(params)...);
    }

    explicit operator bool() const { return m_callback; }
};

} // end namespace Util
