/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "palInlineFuncs.h"

namespace Pal
{
// =====================================================================================================================
// Template class which handles compile-time determination of an array layout for register-value pairs for programming
// the hardware.
template <typename Ip, typename T, const T& t, bool adjustUConfig = false>
class RegPairHandler
{
public:
    // Returns the total number of registers represented in t.
    static constexpr uint32 Size()
    {
        return uint32(sizeof(T) / sizeof(t[0]));
    }

    // Returns the first index in t that corresponds with a context register.
    static constexpr uint32 FirstContextIdx()
    {
        uint32 index = UINT32_MAX;
        for (uint32 i = 0; i < Size(); i++)
        {
            if (Ip::IsContext(t[i]))
            {
                index = i;
                break;
            }
        }

        return index;
    }

    // Returns the number of context registers found in t.
    static constexpr uint32 NumContext()
    {
        uint32 numContext = 0;
        for (uint32 i = 0; i < Size(); i++)
        {
            numContext += Ip::IsContext(t[i]);
        }

        return numContext;
    }

    // Returns the first index in t that corresponds with an SH register.
    static constexpr uint32 FirstShIdx()
    {
        uint32 index = UINT32_MAX;
        for (uint32 i = 0; i < Size(); i++)
        {
            if (Ip::IsSh(t[i]))
            {
                index = i;
                break;
            }
        }

        return index;
    }

    // Returns the number of SH registers found in t.
    static constexpr uint32 NumSh()
    {
        uint32 numSh = 0;
        for (uint32 i = 0; i < Size(); i++)
        {
            numSh += Ip::IsSh(t[i]);
        }

        return numSh;
    }

    // Returns the first index in t that corresponds with a non-context, non-SH register.
    static constexpr uint32 FirstOtherIdx()
    {
        uint32 index = UINT32_MAX;
        for (uint32 i = 0; i < Size(); i++)
        {
            if ((Ip::IsContext(t[i]) == false) && (Ip::IsSh(t[i]) == false))
            {
                index = i;
                break;
            }
        }

        return index;
    }

    // Returns the number of non-context, non-SH registers found in t.
    static constexpr uint32 NumOther()
    {
        uint32 numOther = 0;
        for (uint32 i = 0; i < Size(); i++)
        {
            numOther += (Ip::IsContext(t[i]) == false) && (Ip::IsSh(t[i]) == false);
        }

        return numOther;
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

    // Initializes the RegisterValuePair array with default values of 0's.
    static constexpr void Init(RegisterValuePair p[])
    {
        for (uint32 i = 0; i < Size(); i++)
        {
            p[i] = { Ip::GetAdjustedRegOffset(t[i]), 0 };
        }
    }

    // Returns a non-const pointer to the entry that corresponds to the specified register offset, when that
    // register offset is known at compile time. Casts the return value to the specific type specified by R.
    template <uint32 RegOffset, typename R>
    static R* Get(RegisterValuePair p[])
    {
        constexpr uint32 RegIdx = Index(RegOffset);
        static_assert(RegIdx != UINT32_MAX, "Invalid register!");
        PAL_ASSERT(p[RegIdx].offset == Ip::GetAdjustedRegOffset(RegOffset));
        return reinterpret_cast<R*>(&p[RegIdx].value);
    }

    // Returns a non-const pointer to the entry that corresponds to the specified register offset, when that
    // register offset is dynamic. Casts the return value to the specific type specified by R.
    template <typename R>
    static R* Get(RegisterValuePair p[], uint32 regOffset)
    {
        const uint32 regIdx = Index(regOffset);
        PAL_ASSERT((regIdx != UINT32_MAX) && (p[regIdx].offset == Ip::GetAdjustedRegOffset(regOffset)));
        return reinterpret_cast<R*>(&p[regIdx].value);
    }

    // Returns a const reference to the entry that corresponds to the specified register offset, when that
    // register offset is known at compile time. Casts the return value to the specific type specified by R.
    template <uint32 RegOffset, typename R>
    static const R& GetC(const RegisterValuePair p[])
    {
        constexpr uint32 RegIdx = Index(RegOffset);
        static_assert(RegIdx != UINT32_MAX, "Invalid register!");
        PAL_ASSERT(p[RegIdx].offset == Ip::GetAdjustedRegOffset(RegOffset));
        return reinterpret_cast<const R&>(p[RegIdx].value);
    }

    // Returns whether the specified register offset is available.
    static constexpr bool Exist(uint32 regOffset)
    {
        return (Index(regOffset) != UINT32_MAX);
    }

private:
    // Verifies that all registers of a particular type are in a contiguous range so that they can be
    // written to the hardware without needing to jump around.
    static constexpr bool VerifyContiguousTypes()
    {
        bool   valid       = true;
        uint32 lastContext = UINT32_MAX;
        uint32 lastSh      = UINT32_MAX;
        uint32 lastOther   = UINT32_MAX;

        for (uint32 i = 0; valid && (i < Size()); i++)
        {
            if (Ip::IsContext(t[i]))
            {
                if (lastContext != UINT32_MAX)
                {
                    valid = (lastContext + 1) == i;
                }
                lastContext = i;
            }
            else if (Ip::IsSh(t[i]))
            {
                if (lastSh != UINT32_MAX)
                {
                    valid = (lastSh + 1) == i;
                }
                lastSh = i;
            }
            else if (Ip::IsUConfig(t[i]))
            {
                if (lastOther != UINT32_MAX)
                {
                    valid = (lastOther + 1) == i;
                }
                lastOther = i;
            }
            else
            {
                valid = false;
            }
        }

        return valid;
    }

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

    static_assert(VerifyContiguousTypes(),
                  "Register offset array provided contains registers of non-contiguous types!");
    static_assert(NumContext() + NumSh() + NumOther() == Size(), "Number of registers does not equal size!");
    static_assert(AllUniqueRegisters(),
                  "All register offsets specified should be unique; no duplicates should be found!");
};
} // namespace Pal
