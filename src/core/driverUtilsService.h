/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "platform.h"
#include "g_DriverUtilsService.h"
#include <dd_mutex.h>

namespace DriverUtilsService
{
// =====================================================================================================================
// DriverUtilsService based off of DevDriver's DriverUtil protocol.
// This service provides a simple interface for modifying the driver with a lightweight tool.
class DriverUtilsService : public DriverUtils::IDriverUtilsService
{
public:
    DriverUtilsService(Pal::Platform* pPlatform);
    virtual ~DriverUtilsService();

    // Attempts to enable tracing
    virtual DD_RESULT EnableTracing() override;
    virtual DD_RESULT EnableCrashAnalysisMode() override;
    virtual DD_RESULT QueryPalDriverInfo(const DDByteWriter& writer) override;
    virtual DD_RESULT EnableDriverFeatures(const void* pParamBuffer, size_t paramBufferSize) override;
    virtual DD_RESULT SetOverlayString(const void* pParamBuffer, size_t paramBufferSize) override;

    bool IsTracingEnabled() const { return m_isTracingEnabled; }
    bool IsCrashAnalysisModeEnabled() const { return m_crashAnalysisModeEnabled; }
    bool IsRaytracingShaderTokenRequested() const { return m_raytracingShaderTokenEnabled; }
    bool IsStaticVmidRequested() const { return m_staticVmid; }
    const char* GetOverlayBufferString(Pal::uint32 idx) const { return m_overlayBuffer[idx]; }
    void LockOverlayBuffer() { m_overlayMutex.Lock(); }
    void UnlockOverlayBuffer() { m_overlayMutex.Unlock(); }
    bool UseOverlayBuffer() const { return m_useOverlayBuffer; }

private:
    bool             m_isTracingEnabled;
    bool             m_crashAnalysisModeEnabled;
    bool             m_raytracingShaderTokenEnabled;
    bool             m_staticVmid;
    bool             m_useOverlayBuffer;
    Pal::Platform*   m_pPlatform;
    char             m_overlayBuffer[kNumOverlayStrings][kMaxOverlayStringLength];
    DevDriver::Mutex m_overlayMutex;
};
}
