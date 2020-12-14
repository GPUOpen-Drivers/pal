/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_BUILD_GPU_DEBUG

#include "core/layers/decorators.h"

namespace Pal
{
namespace GpuDebug
{

// Forward decl's.
class Device;

// =====================================================================================================================
class ColorBlendState : public ColorBlendStateDecorator
{
public:
    ColorBlendState(
        IColorBlendState*                pNextState,
        const ColorBlendStateCreateInfo& createInfo,
        const Device*                    pDevice);

    const ColorBlendStateCreateInfo& CreateInfo() const { return m_createInfo; }

private:
    virtual ~ColorBlendState() { }

    const Device*                   m_pDevice;
    const ColorBlendStateCreateInfo m_createInfo;

    PAL_DISALLOW_DEFAULT_CTOR(ColorBlendState);
    PAL_DISALLOW_COPY_AND_ASSIGN(ColorBlendState);
};

} // GpuDebug
} // Pal

#endif
