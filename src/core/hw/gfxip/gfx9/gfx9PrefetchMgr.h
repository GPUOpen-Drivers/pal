/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/prefetchMgr.h"

namespace Pal
{
namespace Gfx9
{

class Device;
enum  PrefetchMethod : uint32;

// =====================================================================================================================
// Gfx9-specific implementation of the PrefetchMgr.  Manages prefetching (L2 shader cache warming) for various types
// of data on behalf of a command buffer.
class PrefetchMgr : public Pal::PrefetchMgr
{
public:
    PrefetchMgr(const Device& device);
    virtual ~PrefetchMgr() {}

    uint32* RequestPrefetch(PrefetchType prefetchType, gpusize addr, size_t sizeInBytes, uint32* pCmdSpace) const;

private:
    // Structure storing information about a particular prefetch type.
    struct PrefetchTypeDescriptor
    {
        PrefetchMethod method;    // Selects which prefetch method should be used for this type.
        size_t         minSize;   // Minimum size to prefetch.
        size_t         clampSize; // Clamp prefetches to this maximum size.

        gpusize        curAddr;   // Current address bound to this prefetch type.
        uint32         curSize;   // Current size of the data bound to this prefetch type.
    };

    // Descriptor structure for each prefetch type.
    PrefetchTypeDescriptor m_prefetchDescriptors[NumPrefetchTypes];

    PAL_DISALLOW_COPY_AND_ASSIGN(PrefetchMgr);
    PAL_DISALLOW_DEFAULT_CTOR(PrefetchMgr);
};

} // Gfx9
} // Pal
