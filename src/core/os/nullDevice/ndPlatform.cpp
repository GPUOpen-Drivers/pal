/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_BUILD_NULL_DEVICE

#include "core/os/nullDevice/ndDevice.h"
#include "core/os/nullDevice/ndPlatform.h"

using namespace Util;

namespace Pal
{

namespace NullDevice
{

// =====================================================================================================================
Platform::Platform(
    const PlatformCreateInfo& createInfo,
    const AllocCallbacks&     allocCb)
    :
    Pal::Platform(createInfo, allocCb),
    m_nullGpuId(createInfo.nullGpuId)
{
}

// =====================================================================================================================
// Windows-specific platform factory function which instantiates a new Windows::Platform object.
Platform* Platform::CreateInstance(
    const PlatformCreateInfo& createInfo,
    const AllocCallbacks&     allocCb,
    void*                     pPlacementAddr)
{
    return PAL_PLACEMENT_NEW(pPlacementAddr) Pal::NullDevice::Platform(createInfo, allocCb);
}

// =====================================================================================================================
// The OS isn't going to get called to do anything.  There are no GPU memory allocations, no submits, no presents etc.
Result Platform::ConnectToOsInterface()
{
    return Result::Success;
}

// =====================================================================================================================
size_t Platform::GetSize()
{
    return sizeof(Pal::NullDevice::Platform);
}

// =====================================================================================================================
size_t Platform::GetScreenObjectSize() const
{
    return 0;
}

// =====================================================================================================================
// Enumerates all devices and LDA chains present in the system. For each device and LDA chain, the adapter info
// structures for the chains themselves and their connected devices will be queried from the KMD.
//
// This method may be called multiple times, because clients will use it to re-enumerate devices after a Device lost
// error occurs.
Result Platform::ReQueryDevices()
{
    constexpr uint32_t MaxNullGpuCount = static_cast<uint32_t>(NullGpuId::Max);

    Result result = Result::Unsupported;

    NullGpuInfo nullGpus[MaxNullGpuCount] = {};
    uint32 nullGpuCount = 0;

    if (m_nullGpuId < NullGpuId::Max)
    {
        nullGpus[0].nullGpuId = m_nullGpuId;
        nullGpuCount          = 1;
    }
    else if (m_nullGpuId == NullGpuId::All)
    {
        nullGpuCount = MaxNullGpuCount;

        result = EnumerateNullDevices(&nullGpuCount, nullGpus);

        if (result != Result::Success)
        {
            result       = Result::Unsupported;
            nullGpuCount = 0;
        }
    }
    else
    {
        // The constructor would leave m_nullGpuId as "max" if the environment variable that controls null device
        // creation doesn't match something we support.
        PAL_ASSERT(m_nullGpuId == NullGpuId::Max);
    }

    // Only create the last MaxDevices null devices if we are in NullGpuId::All mode.
    const uint32 firstNullGpu = (nullGpuCount > MaxDevices) ? (nullGpuCount - MaxDevices) : 0;

    for (uint32 nullGpu = firstNullGpu; nullGpu < nullGpuCount; nullGpu++)
    {
        NullDevice::Device* pDevice = nullptr;

        result = Device::Create(this, &pDevice, nullGpus[nullGpu].nullGpuId);

        if ((result == Result::Success) && (pDevice != nullptr))
        {
            m_pDevice[m_deviceCount++] = pDevice;
        }
    }

    return result;
}

// =====================================================================================================================
// Enumerates all physical screens present in the system. No screen is created on a NULL Device.
Result Platform::ReQueryScreens(
    uint32*  pScreenCount,
    void*    pStorage[MaxScreens],
    IScreen* pScreens[MaxScreens])
{
    return Result::Success;
}

// =====================================================================================================================
Result Platform::GetPrimaryLayout(
    uint32                  vidPnSourceId,
    GetPrimaryLayoutOutput* pPrimaryLayoutOutput)
{
    PAL_NOT_IMPLEMENTED();

    return Result::Success;
}

// =====================================================================================================================
Result Platform::TurboSyncControl(
    const TurboSyncControlInput& turboSyncControlInput)
{
    PAL_NOT_IMPLEMENTED();

    return Result::Success;
}

} // NullDevice

} // Pal

#endif
