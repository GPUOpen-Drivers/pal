/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/layers/decorators.h"
#include "palMutex.h"
#include "palHashMapImpl.h"

namespace Pal
{

namespace DbgOverlay
{

class Device;
class FpsMgr;

enum AllocType : uint32
{
    AllocTypeInternal = 0,
    AllocTypeExternal = 1,
    AllocTypeCmdAlloc = 2,
    AllocTypeCount
};

// =====================================================================================================================
// Overlay Platform class. Inherits from PlatformDecorator
class Platform final : public PlatformDecorator
{
public:
    static Result Create(
        const PlatformCreateInfo&   createInfo,
        const Util::AllocCallbacks& allocCb,
        IPlatform*                  pNextPlatform,
        bool                        enabled,
        void*                       pPlacementAddr,
        IPlatform**                 ppPlatform);

    Platform(
        const PlatformCreateInfo&   createInfo,
        const Util::AllocCallbacks& allocCb,
        IPlatform*                  pNextPlatform,
        bool                        dbgOverlayEnabled)
        :
        PlatformDecorator(createInfo, allocCb, DbgOverlayCb, dbgOverlayEnabled, dbgOverlayEnabled, pNextPlatform),
        m_fpsMgrMap(64, this),
        m_rayTracingEverUsed(false)
    {
        ResetGpuWork();
    }

    virtual Result EnumerateDevices(
        uint32*    pDeviceCount,
        IDevice*   pDevices[MaxDevices]) override;

    virtual size_t GetScreenObjectSize() const override;

    virtual Result GetScreens(
        uint32*  pScreenCount,
        void*    pStorage[MaxScreens],
        IScreen* pScreens[MaxScreens]) override;

    const PlatformProperties& Properties() const { return m_properties; }

    FpsMgr* GetFpsMgr(UniquePresentKey key = 0);

    Device* GetDevice(uint32 deviceIndex);

    static void PAL_STDCALL DbgOverlayCb(
        void*                   pPrivateData,
        const uint32            deviceIndex,
        Developer::CallbackType type,
        void*                   pCbData);

    bool GetGpuWork(uint32 deviceIndex);

    void SetGpuWork(uint32 deviceIndex, bool isBusy);

    void ResetGpuWork();

    bool HasRayTracingBeenUsed(void) const { return m_rayTracingEverUsed; }

    void CheckRayTracing(const MultiSubmitInfo& submitInfo);

protected:
    virtual ~Platform();

private:
    typedef Util::HashMap<uint64,
        FpsMgr*,
        Platform,
        Util::JenkinsHashFunc,
        Util::DefaultEqualFunc,
        Util::HashAllocator<Platform>,
        64> FpsMgrMap;

    FpsMgrMap m_fpsMgrMap;                   // Have an FpsMgr for individual windows, swap chains, ...

    PlatformProperties m_properties;

    Util::Mutex        m_gpuWorkLock;
    volatile bool      m_gpuWork[MaxDevices];
    bool               m_rayTracingEverUsed;

    PAL_DISALLOW_DEFAULT_CTOR(Platform);
    PAL_DISALLOW_COPY_AND_ASSIGN(Platform);
};

} // DbgOverlay
} // Pal
