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

namespace Pal
{

// =====================================================================================================================
// Template class which handles compile-time determination of an array layout for register-values for programming
// the hardware. This mirrors RegPairHandler but is just an array of register values rather than reg offset/value pairs.
template <typename T, const T& t>
class RegHandler
{
public:
    // Returns the total number of registers represented in t.
    static constexpr uint32 Size()
    {
        return uint32(sizeof(T) / sizeof(t[0]));
    }

    // Returns the index of the specified register offset.
    static constexpr uint32 Index(uint32 regOffset)
    {
        uint32 index = UINT32_MAX;

        for (uint32 i = 0; i < Size(); i++)
        {
            if (t[i] == regOffset)
            {
                index = i;
                break;
            }
        }

        return index;
    }

    // Returns a non-const pointer to the entry that corresponds to the specified register offset, when that
    // register offset is known at compile time. Casts the return value to the specific type specified by R.
    template <uint32 RegOffset, typename R>
    static R* Get(uint32 p[])
    {
        constexpr uint32 RegIdx = Index(RegOffset);
        static_assert(RegIdx != UINT32_MAX, "Invalid register!");
        return reinterpret_cast<R*>(&p[RegIdx]);
    }

    // Returns a non-const pointer to the entry that corresponds to the specified register offset, when that
    // register offset is dynamic. Casts the return value to the specific type specified by R.
    template <typename R>
    static R* Get(uint32 p[], uint32 regOffset)
    {
        constexpr uint32 RegIdx = Index(regOffset);
        static_assert(RegIdx != UINT32_MAX, "Invalid register!");
        return reinterpret_cast<R*>(&p[RegIdx]);
    }

    // Returns a const reference to the entry that corresponds to the specified register offset, when that
    // register offset is known at compile time. Casts the return value to the specific type specified by R.
    template <uint32 RegOffset, typename R>
    static const R& GetC(const uint32 p[])
    {
        constexpr uint32 RegIdx = Index(RegOffset);
        static_assert(RegIdx != UINT32_MAX, "Invalid register!");
        return reinterpret_cast<const R&>(p[RegIdx]);
    }

    // Returns whether the specified register offset is available.
    static constexpr bool Exist(uint32 regOffset)
    {
        return (Index(regOffset) != UINT32_MAX);
    }

private:
    // Verifies that the same register does not appear twice.
    static constexpr bool AllUniqueRegisters()
    {
        bool unique = true;
        for (uint32 i = 0; unique && (i < Size()); i++)
        {
            for (uint32 j = 0; unique && (j < Size()); j++)
            {
                if (i == j)
                {
                    continue;
                }

                if (t[i] == t[j])
                {
                    unique = false;
                    break;
                }
            }
        }

        return unique;
    }

    static_assert(AllUniqueRegisters(),
                  "All register offsets specified should be unique; no duplicates should be found!");
};

} // namespace Pal
