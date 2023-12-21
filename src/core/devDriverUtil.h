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

#include "pal.h"
#include "core/platform.h"
#include "palHashMap.h"
#include "palMutex.h"

// Forward declarations.
namespace DevDriver
{
    namespace DriverControlProtocol
    {
        enum struct DeviceClockMode : Pal::uint32;
    }
    class IStructuredWriter;
}

namespace Pal
{
#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION < GPUOPEN_DRIVER_CONTROL_QUERY_CLOCKS_BY_MODE_VERSION
DevDriver::Result QueryClockCallback(
    uint32                                            gpuIndex,
    float*                                            pGpuClock,
    float*                                            pMemClock,
    void*                                             pUserData);
#else
DevDriver::Result QueryClockCallback(
    uint32                                            gpuIndex,
    DevDriver::DriverControlProtocol::DeviceClockMode clockMode,
    float*                                            pGpuClock,
    float*                                            pMemClock,
    void*                                             pUserData);
#endif

DevDriver::Result QueryMaxClockCallback(
    uint32 gpuIndex,
    float* pGpuClock,
    float* pMemClock,
    void*  pUserData);

DevDriver::Result SetClockModeCallback(
    uint32                                            gpuIndex,
    DevDriver::DriverControlProtocol::DeviceClockMode clockMode,
    void*                                             pUserData);

void PalCallback(
    DevDriver::IStructuredWriter* pWriter,
    void*                         pUserData);

void* DevDriverAlloc(
    void* pUserdata,
    size_t size,
    size_t alignment,
    bool zero);

void DevDriverFree(
    void* pUserdata,
    void* pMemory);

Result DdResultToPalResult(DD_RESULT ddResult);

} // Pal
