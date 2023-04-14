/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
namespace InterfaceLogger
{

class Device;
class Platform;

// =====================================================================================================================
class GpuMemory final : public GpuMemoryDecorator
{
public:
    GpuMemory(IGpuMemory* pNextGpuMemory, const Device* pDevice, uint32 objectId);

    // Returns this object's unique ID.
    uint32 ObjectId() const { return m_objectId; }

    // Public IGpuMemory interface methods:
    virtual Result SetPriority(
        GpuMemPriority       priority,
        GpuMemPriorityOffset priorityOffset) override;
    virtual Result Map(
        void** ppData) override;
    virtual Result Unmap() override;
    virtual Result SetSdiRemoteBusAddress(gpusize surfaceBusAddr, gpusize markerBusAddr) override;

    // Public IDestroyable interface methods:
    virtual void Destroy() override;

private:
    virtual ~GpuMemory() { }

    Platform*const m_pPlatform;
    const uint32   m_objectId;

    PAL_DISALLOW_DEFAULT_CTOR(GpuMemory);
    PAL_DISALLOW_COPY_AND_ASSIGN(GpuMemory);
};

} // InterfaceLogger
} // Pal

#endif
