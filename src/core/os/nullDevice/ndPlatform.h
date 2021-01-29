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

#if PAL_BUILD_NULL_DEVICE

#include "core/platform.h"

namespace Pal
{
class  IScreen;
struct PlatformCreateInfo;

namespace NullDevice
{

// =====================================================================================================================
// Windows flavor of the Platform singleton. The responsibilities of the OS-specific Platform classes are interacting
// with the OS and kernel-mode drivers. On Windows, this includes managing access to the thunk calls as well as managing
// any/all LDA chains which the active devices can belong to.
class Platform final : public Pal::Platform
{
public:
    Platform(const PlatformCreateInfo& createInfo, const Util::AllocCallbacks& allocCb);
    virtual ~Platform() {}

    virtual void Destroy() override { this->~Platform(); }

    static Platform* CreateInstance(
        const PlatformCreateInfo&   createInfo,
        const Util::AllocCallbacks& allocCb,
        void*                       pPlacementAddr);
    static size_t  GetSize();

    virtual size_t GetScreenObjectSize() const override;

    virtual Result GetPrimaryLayout(uint32 vidPnSourceId, GetPrimaryLayoutOutput* pPrimaryLayoutOutput) override;

    virtual Result TurboSyncControl(const TurboSyncControlInput& turboSyncControlInput) override;

private:
    virtual Result ConnectToOsInterface() override;
    virtual Result ReQueryDevices() override;
    virtual Result ReQueryScreens(
        uint32*       pScreenCount,
        void*         pStorage[MaxScreens],
        Pal::IScreen* pScreens[MaxScreens]) override;

    NullGpuId  m_nullGpuId;

    PAL_DISALLOW_COPY_AND_ASSIGN(Platform);
    PAL_DISALLOW_DEFAULT_CTOR(Platform);
};

} // NullDevice
} // Pal

#endif
