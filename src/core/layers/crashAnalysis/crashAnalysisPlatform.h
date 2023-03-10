/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "crashAnalysisEventProvider.h"

#include <atomic>

namespace Pal
{
namespace CrashAnalysis
{

// =====================================================================================================================
class Platform final : public PlatformDecorator
{
public:
    static Result Create(
        const PlatformCreateInfo&   createInfo,
        const Util::AllocCallbacks& allocCb,
        IPlatform*                  pNextPlatform,
        bool                        enabled,
        void*                       pPlacementAddr,
        IPlatform**                 ppPlatform,
        CrashAnalysisEventProvider* pCrashAnalysisEventProvider);

    Platform(
        const PlatformCreateInfo&   createInfo,
        const Util::AllocCallbacks& allocCb,
        IPlatform*                  pNextPlatform,
        bool                        enabled,
        CrashAnalysisEventProvider* pEventProvider);

    virtual Result Init() override;
    virtual void Destroy() override;

    bool IsEnabled() const { return m_layerEnabled; }

    // Generates an ID, unique within this Platform, for a generic resource
    uint32 GenerateResourceId()
    {
        return m_resourceId.fetch_add(1, std::memory_order_relaxed);
    }

    CrashAnalysisEventProvider* GetCrashAnalysisEventProvider() { return m_pCrashAnalysisEventProvider; }

    // Public IPlatform interface methods:
    virtual Result EnumerateDevices(
        uint32*  pDeviceCount,
        IDevice* pDevices[MaxDevices]) override;
    virtual size_t GetScreenObjectSize() const override;
    virtual Result GetScreens(
        uint32*  pScreenCount,
        void*    pStorage[MaxScreens],
        IScreen* pScreens[MaxScreens]) override;

    static void PAL_STDCALL CrashAnalysisCb(
        void*                   pPrivateData,
        const uint32            deviceIndex,
        Developer::CallbackType type,
        void*                   pCbData);

private:
    virtual ~Platform() { }

    CrashAnalysisEventProvider* m_pCrashAnalysisEventProvider;
    std::atomic<uint32>         m_resourceId;

    PAL_DISALLOW_DEFAULT_CTOR(Platform);
    PAL_DISALLOW_COPY_AND_ASSIGN(Platform);
};

} // namespace CrashAnalysis
} // namespace Pal
