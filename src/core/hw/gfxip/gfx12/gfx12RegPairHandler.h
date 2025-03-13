/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/regPairHandler.h"
#include "core/hw/gfxip/gfx12/gfx12Chip.h"

namespace Pal
{
namespace Gfx12
{

// =====================================================================================================================
// Gfx12-specific register offset functions for template instantiation.
class _RegFuncs
{
public:
    static constexpr bool IsContext(uint32 regOffset)
    {
        return Util::InRange(regOffset, Chip::CONTEXT_SPACE_START, Chip::CONTEXT_SPACE_END);
    }

    static constexpr bool IsSh(uint32 regOffset)
    {
        return Util::InRange(regOffset, Chip::PERSISTENT_SPACE_START, Chip::PERSISTENT_SPACE_END);
    }

    static constexpr bool IsUConfig(uint32 regOffset)
    {
        return Util::InRange(regOffset, Chip::UCONFIG_SPACE_START, UConfigRangeEnd);
    }

    static constexpr uint32 GetAdjustedRegOffset(uint32 regOffset)
    {
        uint32 spaceStart = 0;

        if (IsSh(regOffset))
        {
            spaceStart = PERSISTENT_SPACE_START;
        }
        else if (IsContext(regOffset))
        {
            spaceStart = CONTEXT_SPACE_START;
        }
        else if (IsUConfig(regOffset))
        {
            spaceStart = UCONFIG_SPACE_START;
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }

        return regOffset - spaceStart;
    }
};

// =====================================================================================================================
// Gfx12 instantiation of the RegPairHandler.
template <typename T, const T& t>
class RegPairHandler : public Pal::RegPairHandler<_RegFuncs, T, t>
{
};

} // namespace Gfx12
} // namespace Pal
