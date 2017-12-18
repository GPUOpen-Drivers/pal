/*
 *******************************************************************************
 *
 * Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All rights reserved.
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

namespace Pal
{

class GfxDevice;

// =====================================================================================================================
// Manages prefetching (L2 shader cache warming) for various types of data on behalf of a command buffer.
class PrefetchMgr
{
public:
    virtual ~PrefetchMgr() {}

    void EnableShaderPrefetch(bool enable);

    static constexpr uint32 RequiredStartAlign = 0x20; // Require 32 byte address alignment for prefetching.
    static constexpr uint32 RequiredSizeAlign  = 0x20; // Require 32 byte size alignment for prefetching.

protected:
    PrefetchMgr(const GfxDevice& device);

    static constexpr uint32 AllShaderPrefetchMask = (1 << PrefetchCs)          |
                                                    (1 << PrefetchVs)          |
                                                    (1 << PrefetchHs)          |
                                                    (1 << PrefetchDs)          |
                                                    (1 << PrefetchGs)          |
                                                    (1 << PrefetchCopyShader)  |
                                                    (1 << PrefetchPs);

    static constexpr uint32 AllPrefetchMask        = AllShaderPrefetchMask;

    static constexpr uint32 GfxPrefetchMask        = (1 << PrefetchVs)         |
                                                     (1 << PrefetchHs)         |
                                                     (1 << PrefetchDs)         |
                                                     (1 << PrefetchGs)         |
                                                     (1 << PrefetchCopyShader) |
                                                     (1 << PrefetchPs);

    static constexpr uint32 CsPrefetchMask         = (1 << PrefetchCs);

    const GfxDevice& m_device;          // Associated device.
    uint32           m_curPrefetchMask; // Mask of enabled prefetch types.
    uint32           m_dirtyFlags;      // Mask of prefetch types that need validation.

private:
    const uint32 m_shaderPrefetchMask;  // Mask of shader prefetch types allowed by settings.

    PAL_DISALLOW_COPY_AND_ASSIGN(PrefetchMgr);
    PAL_DISALLOW_DEFAULT_CTOR(PrefetchMgr);
};

} // Pal
