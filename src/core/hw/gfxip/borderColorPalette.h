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

#include "palBorderColorPalette.h"
#include "core/gpuMemory.h"

namespace Pal
{

class      CmdStream;
class      Device;
enum class PipelineBindPoint : uint32;

// =====================================================================================================================
class BorderColorPalette : public IBorderColorPalette
{
public:
    virtual ~BorderColorPalette() { }

    // NOTE: Part of the public IDestroyable interface.
    virtual void Destroy() override { this->~BorderColorPalette(); }

    // NOTE: Part of the public IGpuMemoryBindable interface.
    virtual void GetGpuMemoryRequirements(GpuMemoryRequirements* pGpuMemReqs) const override;

    // NOTE: Part of the public IGpuMemoryBindable interface.
    virtual Result BindGpuMemory(IGpuMemory* pGpuMemory, gpusize offset) override;

    // NOTE: This is part of the IBorderColorPalette interface.
    virtual Result Update(uint32 firstEntry, uint32 entryCount, const float* pEntries) override;

protected:
    BorderColorPalette(
        const Device&                       device,
        const BorderColorPaletteCreateInfo& createInfo,
        gpusize                             gpuMemAlign);

    virtual void UpdateGpuMemoryBinding(gpusize gpuVirtAddr) = 0;

private:
    const Device&  m_device;
    const uint32   m_numEntries;

    const gpusize  m_gpuMemSize;
    const gpusize  m_gpuMemAlignment;

    BoundGpuMemory m_gpuMemory;
};

} // Pal
