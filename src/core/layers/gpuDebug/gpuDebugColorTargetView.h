/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_DEVELOPER_BUILD

#include "core/layers/decorators.h"

namespace Pal
{
namespace GpuDebug
{

// Forward decl's.
class Device;

// =====================================================================================================================
class ColorTargetView final : public ColorTargetViewDecorator
{
public:
    ColorTargetView(
        IColorTargetView*                pNextView,
        const ColorTargetViewCreateInfo& createInfo,
        const Device*                    pDevice);

    SwizzledFormat Format() const { return m_format; }

private:
    virtual ~ColorTargetView() { }

    const Device*        m_pDevice;
    const SwizzledFormat m_format;

    PAL_DISALLOW_DEFAULT_CTOR(ColorTargetView);
    PAL_DISALLOW_COPY_AND_ASSIGN(ColorTargetView);
};

} // GpuDebug
} // Pal

#endif
