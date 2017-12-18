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

#include "core/layers/decorators.h"
#include "palMutex.h"

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

enum DebugOverlayLocation : uint32
{
    DebugOverlayUpperLeft = 0,
    DebugOverlayUpperRight = 1,
    DebugOverlayLowerRight = 2,
    DebugOverlayLowerLeft = 3,
    DebugOverlayCount = 4,

};

enum TimeGraphColor : uint32
{
    BlackColor = 0,
    RedColor = 1,
    GreenColor = 2,
    BlueColor = 3,
    YellowColor = 4,
    CyanColor = 5,
    MagentaColor = 6,
    WhiteColor = 7,

};

struct DebugOverlaySettings
{
    bool    visualConfirmEnabled;
    bool    timeGraphEnabled;
    DebugOverlayLocation    debugOverlayLocation;
    TimeGraphColor    timeGraphGridLineColor;
    TimeGraphColor    timeGraphCpuLineColor;
    TimeGraphColor    timeGraphGpuLineColor;
    uint32    maxBenchmarkTime;
    bool    debugUsageLogEnable;
    char    debugUsageLogDirectory[512];
    char    debugUsageLogFilename[512];
    bool    logFrameStats;
    char    frameStatsLogDirectory[512];
    uint32    maxLoggedFrames;
    bool    overlayCombineNonLocal;
    bool    overlayReportCmdAllocator;
    bool    overlayReportExternal;
    bool    overlayReportInternal;
    char    renderedByString[61];
    char    miscellaneousDebugString[61];
    bool    printFrameNumber;
};

// =====================================================================================================================
// Overlay Platform class. Inherits from PlatformDecorator
class Platform : public PlatformDecorator
{
public:
    static Result Create(
        const Util::AllocCallbacks& allocCb,
        IPlatform*                  pNextPlatform,
        bool                        enabled,
        void*                       pPlacementAddr,
        IPlatform**                 ppPlatform);

    Platform(const Util::AllocCallbacks& allocCb, IPlatform* pNextPlatform, bool dbgOverlayEnabled)
        :
        PlatformDecorator(allocCb, DbgOverlayCb, dbgOverlayEnabled, dbgOverlayEnabled, pNextPlatform),
        m_pFpsMgr(nullptr)
    {
        m_gpuWorkLock.Init();
        ResetGpuWork();
    }

    virtual Result Init() override;

    virtual Result EnumerateDevices(
        uint32*    pDeviceCount,
        IDevice*   pDevices[MaxDevices]) override;

    virtual size_t GetScreenObjectSize() const override;

    virtual Result GetScreens(
        uint32*  pScreenCount,
        void*    pStorage[MaxScreens],
        IScreen* pScreens[MaxScreens]) override;

    const PlatformProperties& Properties() const { return m_properties; }

    FpsMgr* GetFpsMgr() const { return m_pFpsMgr; }

    Device* GetDevice(uint32 deviceIndex);

    static void PAL_STDCALL DbgOverlayCb(
        void*                   pPrivateData,
        const uint32            deviceIndex,
        Developer::CallbackType type,
        void*                   pCbData);

    bool GetGpuWork(uint32 deviceIndex);

    void SetGpuWork(uint32 deviceIndex, bool isBusy);

    void ResetGpuWork();

    const DebugOverlaySettings& OverlaySettings() const { return m_overlaySettings; }

protected:
    ~Platform();

private:
    Result UpdateSettings();

    PlatformProperties m_properties;
    FpsMgr*            m_pFpsMgr;

    Util::Mutex        m_gpuWorkLock;
    volatile bool      m_gpuWork[MaxDevices];

    DebugOverlaySettings m_overlaySettings;

    PAL_DISALLOW_DEFAULT_CTOR(Platform);
    PAL_DISALLOW_COPY_AND_ASSIGN(Platform);
};

} // DbgOverlay
} // Pal
