/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/device.h"
#include "core/platform.h"
#include "core/os/nullDevice/ndPlatform.h"
#include "core/os/nullDevice/ndDevice.h"

#if PAL_BUILD_GPU_PROFILER
#include "core/layers/gpuProfiler/gpuProfilerPlatform.h"
#endif
#if PAL_BUILD_CMD_BUFFER_LOGGER
#include "core/layers/cmdBufferLogger/cmdBufferLoggerPlatform.h"
#endif
#if PAL_BUILD_DBG_OVERLAY
#include "core/layers/dbgOverlay/dbgOverlayPlatform.h"
#endif
#if PAL_BUILD_INTERFACE_LOGGER
#include "core/layers/interfaceLogger/interfaceLoggerPlatform.h"
#endif
#if PAL_BUILD_PM4_INSTRUMENTOR
#include "core/layers/pm4Instrumentor/pm4InstrumentorPlatform.h"
#endif

#include "addrinterface.h"
#include "vaminterface.h"

namespace Pal
{

// Static asserts to check that the client's AddrLib and VAM libraries are compatible with PAL.  If one of these
// asserts trips, then PAL will likely need an update in order to support a breaking change in one of these library's
// interfaces.
static_assert(ADDRLIB_VERSION_MAJOR == 6, "Unexpected AddrLib major version.");
static_assert(VAM_VERSION_MAJOR == 1, "Unexpected VAM major version.");

// Static asserts to ensure clients have defined PAL_CLIENT_INTERFACE_MAJOR_VERSION and that it falls in the supported
// range.
#ifndef PAL_CLIENT_INTERFACE_MAJOR_VERSION
    static_assert(false, "Client must define PAL_CLIENT_INTERFACE_MAJOR_VERSION.");
#else
    static_assert((PAL_CLIENT_INTERFACE_MAJOR_VERSION >= PAL_MINIMUM_INTERFACE_MAJOR_VERSION) &&
                  (PAL_CLIENT_INTERFACE_MAJOR_VERSION <= PAL_INTERFACE_MAJOR_VERSION),
                  "The specified PAL_CLIENT_INTERFACE_MAJOR_VERSION is not supported.");
#endif

// Static asserts to ensure clients have defined a supported GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION
#ifndef GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION
    static_assert(false, "Client must define GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION.");
#else
    static_assert((GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION >= PAL_MINIMUM_GPUOPEN_INTERFACE_MAJOR_VERSION),
                  "The specified GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION is not supported.");
#endif

// =====================================================================================================================
// Returns the size necessary to initialize a PAL Platform object.
size_t PAL_STDCALL GetPlatformSize()
{
    // The switch between the "real" and "null" device is determined at run-time.  We would never have both active
    // simultaneously.
#if PAL_BUILD_NULL_DEVICE
    size_t platformSize = Util::Max(Platform::GetSize(), NullDevice::Platform::GetSize());
#else
    size_t platformSize = Platform::GetSize();
#endif

    // We need to always assume that the all layers can be enabled.  Unfortunately, at this point, we have not yet read
    // the settings for the GPUs present so we do not know which layers will actually be enabled.
#if PAL_BUILD_INTERFACE_LOGGER
    platformSize += sizeof(InterfaceLogger::Platform);
#endif
#if PAL_BUILD_DBG_OVERLAY
    platformSize += sizeof(DbgOverlay::Platform);
#endif
#if PAL_BUILD_GPU_PROFILER
    platformSize += sizeof(GpuProfiler::Platform);
#endif
#if PAL_BUILD_CMD_BUFFER_LOGGER
    platformSize += sizeof(CmdBufferLogger::Platform);
#endif
#if PAL_BUILD_PM4_INSTRUMENTOR
    platformSize += sizeof(Pm4Instrumentor::Platform);
#endif

    return platformSize;
}

// =====================================================================================================================
// Get the default allocation callback.
void PAL_STDCALL GetDefaultAllocCb(
    Util::AllocCallbacks* pAllocCb)
{
    PAL_ASSERT(pAllocCb != nullptr);

    // Initialize default OS-specific callbacks.
    OsInitDefaultAllocCallbacks(pAllocCb);

}

// =====================================================================================================================
// Initializes the PAL Platform object. This is the first call made by the client on startup, typically during process
// attach. See the public interface documentation for more detail.
Result PAL_STDCALL CreatePlatform(
    const PlatformCreateInfo&   createInfo,
    void*                       pPlacementAddr,
    IPlatform**                 ppPlatform)
{
    Result result = Result::Success;
    Util::AllocCallbacks allocCb = {};

    if ((createInfo.pAllocCb != nullptr) &&
        ((createInfo.pAllocCb->pfnAlloc == nullptr) || (createInfo.pAllocCb->pfnFree == nullptr)))
    {
        // If the client is specifying allocation callbacks, they must define both an alloc and free function pointer.
        result = Result::ErrorInvalidPointer;
    }
    else if ((pPlacementAddr == nullptr) || (createInfo.pSettingsPath == nullptr))
    {
        // The client must specify memory.
        result = Result::ErrorInvalidPointer;
    }
    else if (createInfo.pAllocCb == nullptr)
    {
        GetDefaultAllocCb(&allocCb);
    }
    else
    {
        allocCb = *createInfo.pAllocCb;
    }

    // NOTE: If a specific layer is being built we must always create a Platform decorator for that layer.
    //       This avoids a rather difficult issue where we need to place the IPlatform the client uses at the beginning
    //       of the memory they allocate (or we could have an issue when they go to free that memory). It is easier to
    //       just create the Platform decorator for every layer and make it the responsibility of the layer to
    //       understand when it is enabled or disabled.
#if PAL_BUILD_INTERFACE_LOGGER
    pPlacementAddr = Util::VoidPtrInc(pPlacementAddr, sizeof(InterfaceLogger::Platform));
#endif
#if PAL_BUILD_DBG_OVERLAY
    pPlacementAddr = Util::VoidPtrInc(pPlacementAddr, sizeof(DbgOverlay::Platform));
#endif
#if PAL_BUILD_GPU_PROFILER
    pPlacementAddr = Util::VoidPtrInc(pPlacementAddr, sizeof(GpuProfiler::Platform));
#endif
#if PAL_BUILD_CMD_BUFFER_LOGGER
    pPlacementAddr = Util::VoidPtrInc(pPlacementAddr, sizeof(CmdBufferLogger::Platform));
#endif
#if PAL_BUILD_PM4_INSTRUMENTOR
    pPlacementAddr = Util::VoidPtrInc(pPlacementAddr, sizeof(Pm4Instrumentor::Platform));
#endif

    Platform* pCorePlatform = nullptr;

    if (result == Result::Success)
    {
        result = Platform::Create(createInfo, allocCb, pPlacementAddr, &pCorePlatform);
    }

    IPlatform* pCurPlatform = pCorePlatform;

#if PAL_BUILD_PM4_INSTRUMENTOR
    if (result == Result::Success)
    {
        pPlacementAddr = Util::VoidPtrDec(pPlacementAddr, sizeof(Pm4Instrumentor::Platform));
        pCurPlatform->SetClientData(pPlacementAddr);

        result = Pm4Instrumentor::Platform::Create(createInfo,
                                                   allocCb,
                                                   pCurPlatform,
                                                   pCorePlatform->PlatformSettings().pm4InstrumentorEnabled,
                                                   pPlacementAddr,
                                                   &pCurPlatform);
    }
#endif

#if PAL_BUILD_CMD_BUFFER_LOGGER
    if (result == Result::Success)
    {
        pPlacementAddr = Util::VoidPtrDec(pPlacementAddr, sizeof(CmdBufferLogger::Platform));
        pCurPlatform->SetClientData(pPlacementAddr);

        result = CmdBufferLogger::Platform::Create(allocCb,
                                                   pCurPlatform,
                                                   pCorePlatform->PlatformSettings().cmdBufferLoggerEnabled,
                                                   pPlacementAddr,
                                                   &pCurPlatform);
    }
#endif

#if PAL_BUILD_GPU_PROFILER
    if (result == Result::Success)
    {
        pPlacementAddr = Util::VoidPtrDec(pPlacementAddr, sizeof(GpuProfiler::Platform));
        pCurPlatform->SetClientData(pPlacementAddr);

        result = GpuProfiler::Platform::Create(createInfo,
                                               allocCb,
                                               pCurPlatform,
                                               pCorePlatform->PlatformSettings().gpuProfilerMode,
                                               pPlacementAddr,
                                               &pCurPlatform);
    }
#endif

#if PAL_BUILD_DBG_OVERLAY
    if (result == Result::Success)
    {
        pPlacementAddr = Util::VoidPtrDec(pPlacementAddr, sizeof(DbgOverlay::Platform));
        pCurPlatform->SetClientData(pPlacementAddr);

        result = DbgOverlay::Platform::Create(allocCb,
                                              pCurPlatform,
                                              pCorePlatform->PlatformSettings().debugOverlayEnabled,
                                              pPlacementAddr,
                                              &pCurPlatform);
    }
#endif

#if PAL_BUILD_INTERFACE_LOGGER
    if (result == Result::Success)
    {
        pPlacementAddr = Util::VoidPtrDec(pPlacementAddr, sizeof(InterfaceLogger::Platform));
        pCurPlatform->SetClientData(pPlacementAddr);

        result = InterfaceLogger::Platform::Create(createInfo,
                                                   allocCb,
                                                   pCurPlatform,
                                                   pCorePlatform->PlatformSettings().interfaceLoggerEnabled,
                                                   pPlacementAddr,
                                                   &pCurPlatform);
    }
#endif

    if (result == Result::Success)
    {
        (*ppPlatform) = pCurPlatform;
    }

    return result;
}

// =====================================================================================================================
// If pNullGpuInfoArray is non-null, then on output it will be populated with the corresponding text name for each
// NULL GPU ID enumeration.  Otherwise, pNullGpuCount will be set to the maximum number of entries possible in the
// pNullGpuInfoArray structure.
Result PAL_STDCALL EnumerateNullDevices(
    uint32*       pNullGpuCount,
    NullGpuInfo*  pNullGpuInfoArray)
{
#if PAL_BUILD_NULL_DEVICE
    Result  result = Result::Success;

    if (pNullGpuCount != nullptr)
    {
        uint32        nullGpuCount    = 0;
        const uint32  maxNullGpuCount = (pNullGpuInfoArray != nullptr) ? *pNullGpuCount :
                                                                         static_cast<uint32>(NullGpuId::Max);

        for (uint32 idx = 0;
             ((idx < static_cast<uint32>(NullGpuId::Max)) && (nullGpuCount < maxNullGpuCount));
             ++idx)
        {
            const NullGpuId  nullGpuId = static_cast<NullGpuId>(idx);

            if (NullDevice::Device::IsValid(nullGpuId))
            {
                if (pNullGpuInfoArray != nullptr)
                {
                    NullGpuInfo*  pNullGpuInfo = &pNullGpuInfoArray[nullGpuCount];

                    pNullGpuInfo->nullGpuId = nullGpuId;
                    pNullGpuInfo->pGpuName  = NullDevice::pNullGpuNames[idx];
                }

                ++nullGpuCount;
            }
        }

        // On output, this reflects the number of valid entries in the pNullGpuInfoArray
        *pNullGpuCount = nullGpuCount;
    }
    else
    {
        // No valid count info, can't continue
        result = Result::ErrorInvalidPointer;
    }
#else
    const Result result = Result::Unsupported;
#endif

    return result;
}

} // Pal
