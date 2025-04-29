/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#pragma once

#include <cstddef>
#include <cstdio>

namespace DevDriver
{

struct PlacementNewDummy
{
    explicit PlacementNewDummy () {}
};

} // namespace DevDriver

// Placement new operator overload. C++ standard doesn't allow overloading of the placement new operator with
// the default signature, but we can overload it with a different signature, thus PlacementNewDummy.
void* operator new(std::size_t size, void* pMemory, DevDriver::PlacementNewDummy dummy) noexcept;

// This delete operator should never be called. It's defined here for pure symmetry purpose (some compilers
// also require a matching delete operator).
//
// For objects that are constructed via the placement new operator, no delete operator should be called on
// them, because the delete operator wouldn't know how to de-allocate the memory. Instead, Objects' destructor
// should be called manually: `pObject->~Object();`.
void operator delete(void*, void*, DevDriver::PlacementNewDummy) noexcept;

namespace DevDriver
{

// A helper to construct `count` number of objects already allocated in memory pointed to by `pMemory`.
template<typename T, typename... Ps>
void PlaceNew(std::size_t count, T* pMemory, Ps... args)
{
    T* pObject = pMemory;
    for (std::size_t i = 0; i < count; ++i)
    {
        new(pObject, DevDriver::PlacementNewDummy{}) T(args...);
        pObject += 1;
    }
}

// A helper to destruct `count` number of objects residing in the memory pointed by `pMemory`.
template<typename T>
void PlaceDelete(std::size_t count, T* pMemory)
{
    T* pObject = pMemory;
    for (std::size_t i = 0; i < count; ++i)
    {
        pObject->~T();
        pObject += 1;
    }
}

} // namespace DevDriver

