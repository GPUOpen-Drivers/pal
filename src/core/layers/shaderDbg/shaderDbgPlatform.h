/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

namespace Pal
{
namespace ShaderDbg
{

// =====================================================================================================================
class Platform : public PlatformDecorator
{
public:
    static Result Create(
        const Util::AllocCallbacks& allocCb,
        IPlatform*                  pNextPlatform,
        bool                        enabled,
        void*                       pPlacementAddr,
        IPlatform**                 ppPlatform);

    Platform(const Util::AllocCallbacks& allocCb, IPlatform* pNextPlatform, bool enabled);

    // Public IPlatform interface methods:
    virtual Result EnumerateDevices(uint32* pDeviceCount, IDevice* pDevices[MaxDevices]) override;
    virtual size_t GetScreenObjectSize() const override;
    virtual Result GetScreens(
        uint32*  pScreenCount,
        void*    pStorage[MaxScreens],
        IScreen* pScreens[MaxScreens]) override;

    bool IsEnabled() const { return m_layerEnabled; }
    const char* LogDirName() const { return &m_logDirName[0]; }

    static void PAL_STDCALL ShaderDbgCb(
        void*                   pPrivateData,
        const uint32            deviceIndex,
        Developer::CallbackType type,
        void*                   pCbData);

private:
    virtual ~Platform() { }

    // Storage for a unique log directory per session based on the executable name and current date/time.
    static constexpr size_t LogDirNameLength = 512;
    char m_logDirName[LogDirNameLength];

    PAL_DISALLOW_DEFAULT_CTOR(Platform);
    PAL_DISALLOW_COPY_AND_ASSIGN(Platform);
};

} // ShaderDbg
} // Pal
