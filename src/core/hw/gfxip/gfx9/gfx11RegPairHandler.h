/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/gfx9Chip.h"

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
// Gfx11-specific register offset functions for template instantiation.
class _RegFuncs
{
public:
    static constexpr bool IsContext(uint32 regOffset)
    {
        return Util::InRange(regOffset, CONTEXT_SPACE_START, Gfx11::CONTEXT_SPACE_END);
    }

    static constexpr bool IsSh(uint32 regOffset)
    {
        return Util::InRange(regOffset, PERSISTENT_SPACE_START, PERSISTENT_SPACE_END);
    }

    static constexpr bool IsUConfig(uint32 regOffset)
    {
        return Util::InRange(regOffset, UCONFIG_SPACE_START, UCONFIG_SPACE_END);
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
// Gfx11 instantiation of the RegPairHandler.
template <typename T, const T& t>
class Gfx11RegPairHandler : public Pal::RegPairHandler<_RegFuncs, T, t>
{
};

// =====================================================================================================================
// Gfx11 instantiation of the PackedRegPairHandler.
template <typename T, const T& t>
class Gfx11PackedRegPairHandler : public Pal::PackedRegPairHandler<_RegFuncs, T, t>
{
};

} // namespace Gfx9
} // namespace Pal
