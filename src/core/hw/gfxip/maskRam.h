/*
 *******************************************************************************
 *
 * Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All rights reserved.
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

#include "pal.h"
#include "palInlineFuncs.h"
#include "core/image.h"

namespace Pal
{

// =====================================================================================================================
// Manages the mask-RAM state for all slices of a single mipmap level of an Image resource. This is a base class which
// is common to all types of mask-RAM.
class MaskRam
{
public:
    // Returns the GPU total memory size needed.
    gpusize TotalSize() const { return m_totalSize; }
    gpusize SliceSize() const { return m_sliceSize; }
    gpusize Alignment() const { return m_alignment; }
    gpusize MemoryOffset() const { return m_offset; }

protected:
    MaskRam()
        :
        m_offset(0),
        m_sliceSize(0),
        m_totalSize(0),
        m_alignment(0)
    {}

    virtual ~MaskRam() {}

    void UpdateGpuMemOffset(gpusize* pGpuOffset)
    {
        PAL_ASSERT(m_totalSize > 0);
        m_offset      = Util::Pow2Align((*pGpuOffset), m_alignment);
        (*pGpuOffset) = (m_offset + m_totalSize);
    }

    uint32 MaskRamSlices(const Image& image, const SubResourceInfo& subResInfo) const
    {
        auto& createInfo = image.GetImageCreateInfo();
        return (createInfo.imageType == ImageType::Tex3d) ? subResInfo.extentTexels.depth : createInfo.arraySize;
    }

    gpusize  m_offset;    // GPU memory offset from base of parent Image.
    gpusize  m_sliceSize; // Per-slice GPU memory size
    gpusize  m_totalSize; // Total GPU memory size
    gpusize  m_alignment; // GPU memory alignment

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(MaskRam);
};

} // Pal
