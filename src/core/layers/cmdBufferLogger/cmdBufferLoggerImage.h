/*
 *******************************************************************************
 *
 * Copyright (c) 2016-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

#include "core/layers/decorators.h"

namespace Pal
{

namespace CmdBufferLogger
{

// Forward declarations
class Device;

// =====================================================================================================================
// Overlay Image class. Inherits from ImageDecorator
class Image : public ImageDecorator
{
public:
    Image(IImage* pNextImage, Device* pDevice);

    virtual ~Image();

    virtual Result BindGpuMemory(
        IGpuMemory* pGpuMemory,
        gpusize     offset) override;

    void SetBoundGpuMemory(IGpuMemory* pGpuMemory, gpusize offset)
    {
        m_pBoundMemObj   = pGpuMemory;
        m_boundMemOffset = offset;
    }

    // Returns the mem object that's bound to this surface, nullptr if nothing is bound yet
    IGpuMemory*  GetBoundMemObject() const { return m_pBoundMemObj; }

    // Returns the offset into the memory that is bound to this image, results are undefined if nothing is bound
    // (or if memory was bound and has since been unbound).
    gpusize GetBoundMemOffset() const { return m_boundMemOffset; }

private:
    IGpuMemory*            m_pBoundMemObj;   // The memory bound to this image
    gpusize                m_boundMemOffset; // The offset into the bound memory

    PAL_DISALLOW_DEFAULT_CTOR(Image);
    PAL_DISALLOW_COPY_AND_ASSIGN(Image);
};

} // CmdBufferLogger
} // Pal
