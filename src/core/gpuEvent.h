/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
    virtual ~GpuEvent();

    virtual void Destroy() override;

    virtual void GetGpuMemoryRequirements(GpuMemoryRequirements* pGpuMemReqs) const override;
    virtual Result BindGpuMemory(IGpuMemory* pGpuMemory, gpusize offset) override;

    virtual Result GetStatus() override;
    virtual Result Set() override;
    virtual Result Reset() override;

    static constexpr uint32  SetValue     = 0xDEADBEEF;
    static constexpr uint32  ResetValue   = 0xCAFEBABE;
    static constexpr uint64  SetValue64   = 0xDEADBEEFDEADBEEF;

    const BoundGpuMemory& GetBoundGpuMemory() const { return m_gpuMemory; }

    bool IsGpuAccessOnly() const { return (m_createInfo.flags.gpuAccessOnly == 1); }

private:
    Result CpuWrite(uint32 slotId, uint32 data);

    const GpuEventCreateInfo m_createInfo;
    Device*const             m_pDevice;

    BoundGpuMemory    m_gpuMemory;
    volatile uint32*  m_pEventData;

    const uint32      m_numSlotsPerEvent;

    PAL_DISALLOW_COPY_AND_ASSIGN(GpuEvent);
};

} // Pal
