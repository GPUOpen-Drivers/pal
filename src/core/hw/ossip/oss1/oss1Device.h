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

#pragma once

#include "core/hw/ossip/ossDevice.h"

namespace Pal
{
namespace Oss1
{

// Width/height of a tile in pixels
static constexpr uint32 TileWidth = 8;

// Number of tile pixels
static constexpr uint32 TilePixels = 64;

// =====================================================================================================================
// OSS1 hardware layer implementation of OssDevice. Responsible for creating HW-specific objects such as Queue
// contexts.
class Device : public OssDevice
{
public:
    Device(Pal::Device* pDevice) : OssDevice(pDevice) { }
    virtual ~Device() { }

    virtual Result CreateEngine(
        EngineType engineType,
        uint32     engineIndex,
        Engine**   ppEngine) override;

    virtual Result CreateDummyCommandStream(EngineType engineType, Pal::CmdStream** ppCmdStream) const override;

    virtual size_t GetQueueContextSize(const QueueCreateInfo& createInfo) const override;

    virtual Result CreateQueueContext(
        Queue*         pQueue,
        void*          pPlacementAddr,
        QueueContext** ppQueueContext) override;

    virtual size_t GetCmdBufferSize() const override;

    virtual Result CreateCmdBuffer(
        const CmdBufferCreateInfo& createInfo,
        void*                      pPlacementAddr,
        CmdBuffer**                ppCmdBuffer) override;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(Device);
};

} // Oss1
} // Pal
