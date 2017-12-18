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

#include "palGpuEvent.h"
#include "core/gpuMemory.h"

namespace Pal
{

class Device;

// =====================================================================================================================
// Represents an event object can be set or reset by both the CPU and GPU, and its status can be queried by the CPU.
// See IGpuEvent documentation for more details.
class GpuEvent : public IGpuEvent
{
public:
    GpuEvent(const GpuEventCreateInfo& createInfo, Device* pDevice);
    ~GpuEvent();

    Result Init();

    // NOTE: Part of the public IDestroyable interface.
    virtual void Destroy() override;

    // Note: Part of the public IGpuEvent interface.
    virtual Result GetStatus() override;
    virtual Result Set() override;
    virtual Result Reset() override;

    static constexpr uint32  SetValue   = 0xDEADBEEF;
    static constexpr uint32  ResetValue = 0xCAFEBABE;

    const BoundGpuMemory& GetBoundGpuMemory() const { return m_gpuMemory; }

private:
    Result CpuWrite(uint32 data);

    const GpuEventCreateInfo m_createInfo;
    Device*const             m_pDevice;

    BoundGpuMemory           m_gpuMemory;
    volatile uint32*         m_pEventData;

    PAL_DISALLOW_COPY_AND_ASSIGN(GpuEvent);
};

} // Pal
