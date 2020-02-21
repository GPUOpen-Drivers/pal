/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/devDriverUtil.h"
#include "core/device.h"
#include "palHashMapImpl.h"

#include "ddTransferManager.h"
#include "protocols/driverControlServer.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
// DevDriver DeviceClockMode to Pal::DeviceClockMode table
static DeviceClockMode PalDeviceClockModeTable[] =
{
    DeviceClockMode::Default,       // Unknown       = 0
    DeviceClockMode::Default,       // Default       = 1
    DeviceClockMode::Profiling,     // Profiling     = 2
    DeviceClockMode::MinimumMemory, // MinimumMemory = 3
    DeviceClockMode::MinimumEngine, // MinimumEngine = 4
    DeviceClockMode::Peak           // Peak          = 5
};

#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION < GPUOPEN_DRIVER_CONTROL_QUERY_CLOCKS_BY_MODE_VERSION
// =====================================================================================================================
// Callback function which returns the current device clock for the requested gpu.
DevDriver::Result QueryClockCallback(
    uint32 gpuIndex,
    float* pGpuClock,
    float* pMemClock,
    void*  pUserData)
{
    Platform* pPlatform = reinterpret_cast<Platform*>(pUserData);

    DevDriver::Result result = DevDriver::Result::Error;

    Device* pPalDevice = nullptr;

    if (gpuIndex < pPlatform->GetDeviceCount())
    {
        pPalDevice = pPlatform->GetDevice(gpuIndex);
    }

    if (pPalDevice != nullptr)
    {
        const GpuChipProperties& chipProps = pPalDevice->ChipProperties();

        SetClockModeInput clockModeInput = {};
        clockModeInput.clockMode = DeviceClockMode::Query;

        SetClockModeOutput clockModeOutput = {};

        Result palResult = pPalDevice->SetClockMode(clockModeInput, &clockModeOutput);

        if (palResult == Result::Success)
        {
            *pGpuClock = (static_cast<float>(chipProps.maxEngineClock) * clockModeOutput.engineClockRatioToPeak);
            *pMemClock = (static_cast<float>(chipProps.maxMemoryClock) * clockModeOutput.memoryClockRatioToPeak);

            result = DevDriver::Result::Success;
        }
    }

    return result;
}
#else
// =====================================================================================================================
// Callback function which returns the current device clock for the requested gpu.
DevDriver::Result QueryClockCallback(
    uint32                                            gpuIndex,
    DevDriver::DriverControlProtocol::DeviceClockMode clockMode,
    float*                                            pGpuClock,
    float*                                            pMemClock,
    void*                                             pUserData)
{
    DevDriver::Result result     = DevDriver::Result::Error;
    Platform*         pPlatform  = reinterpret_cast<Platform*>(pUserData);
    Device*           pPalDevice = nullptr;
    if (gpuIndex < pPlatform->GetDeviceCount())
    {
        pPalDevice = pPlatform->GetDevice(gpuIndex);
    }

    if (pPalDevice != nullptr)
    {
        const GpuChipProperties& chipProps = pPalDevice->ChipProperties();
        DeviceClockMode inputMode = DeviceClockMode::Default;

        switch (clockMode)
        {
        case DevDriver::DriverControlProtocol::DeviceClockMode::Default:
            inputMode = DeviceClockMode::Query;
            break;
        case DevDriver::DriverControlProtocol::DeviceClockMode::Profiling:
            inputMode = DeviceClockMode::QueryProfiling;
            break;
        case DevDriver::DriverControlProtocol::DeviceClockMode::Peak:
            inputMode = DeviceClockMode::QueryPeak;
            break;
        default:
            PAL_ASSERT_ALWAYS();
        }

        SetClockModeInput clockModeInput = {};
        clockModeInput.clockMode = inputMode;

        SetClockModeOutput clockModeOutput = {};
        Result palResult = pPalDevice->SetClockMode(clockModeInput, &clockModeOutput);

        if (palResult == Result::Success)
        {
            *pGpuClock = (static_cast<float>(chipProps.maxEngineClock) * clockModeOutput.engineClockRatioToPeak);
            *pMemClock = (static_cast<float>(chipProps.maxMemoryClock) * clockModeOutput.memoryClockRatioToPeak);

            result = DevDriver::Result::Success;
        }
    }

    return result;
}
#endif
// =====================================================================================================================
// Callback function which returns the max device clock for the requested gpu.
DevDriver::Result QueryMaxClockCallback(
    uint32 gpuIndex,
    float* pGpuClock,
    float* pMemClock,
    void*  pUserData)
{
    Platform* pPlatform = reinterpret_cast<Platform*>(pUserData);

    DevDriver::Result result = DevDriver::Result::Error;

    Device* pPalDevice = nullptr;

    if (gpuIndex < pPlatform->GetDeviceCount())
    {
        pPalDevice = pPlatform->GetDevice(gpuIndex);
    }

    if (pPalDevice != nullptr)
    {
        const GpuChipProperties& chipProps = pPalDevice->ChipProperties();

        *pGpuClock = static_cast<float>(chipProps.maxEngineClock);
        *pMemClock = static_cast<float>(chipProps.maxMemoryClock);

        result = DevDriver::Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Callback function which sets the current device clock mode for the requested gpu.
DevDriver::Result SetClockModeCallback(
    uint32                                            gpuIndex,
    DevDriver::DriverControlProtocol::DeviceClockMode clockMode,
    void*                                             pUserData)
{
    Platform* pPlatform = reinterpret_cast<Platform*>(pUserData);

    DevDriver::Result result = DevDriver::Result::Error;

    Device* pPalDevice = nullptr;

    if (gpuIndex < pPlatform->GetDeviceCount())
    {
        pPalDevice = pPlatform->GetDevice(gpuIndex);
    }

    if (pPalDevice != nullptr)
    {
        // Convert the DevDriver DeviceClockMode enum into a Pal enum
        DeviceClockMode palClockMode = PalDeviceClockModeTable[static_cast<uint32>(clockMode)];

        SetClockModeInput clockModeInput = {};
        clockModeInput.clockMode = palClockMode;

        Result palResult = pPalDevice->SetClockMode(clockModeInput, nullptr);

        result = (palResult == Result::Success) ? DevDriver::Result::Success : DevDriver::Result::Error;
    }

    return result;
}

// =====================================================================================================================
// Callback function used to allocate memory inside the developer driver component.
void* DevDriverAlloc(
    void*  pUserdata,
    size_t size,
    size_t alignment,
    bool   zero)
{
    Platform* pAllocator = reinterpret_cast<Platform*>(pUserdata);

    //NOTE: Alignment is ignored here since PAL always aligns to an entire cache line by default. This shouldn't be an
    //      issue because no type should require more than a cache line of alignment (64 bytes).
    PAL_ASSERT(alignment <= PAL_CACHE_LINE_BYTES);

    void* pMemory = zero ? PAL_CALLOC(size, pAllocator, Util::AllocInternal)
                         : PAL_MALLOC(size, pAllocator, Util::AllocInternal);

    return pMemory;
}

// =====================================================================================================================
// Callback function used to free memory inside the developer driver component.
void DevDriverFree(
    void* pUserdata,
    void* pMemory)
{
    Platform* pAllocator = reinterpret_cast<Platform*>(pUserdata);

    PAL_FREE(pMemory, pAllocator);
}

} // Pal
