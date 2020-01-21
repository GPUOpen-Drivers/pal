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

#pragma once

#include "core/engine.h"
#include "core/queueContext.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"

namespace Pal
{
namespace Gfx9
{

class ComputeEngine : public Engine
{
public:
    ComputeEngine(
        Device*    pDevice,
        EngineType type,
        uint32     index);

    virtual Result Init() override;

    ComputeRingSet* RingSet() { return &m_ringSet; }

    Result UpdateRingSet(uint32* pCounterVal, bool* pHasChanged);

private:
    Device*        m_pDevice;
    ComputeRingSet m_ringSet;
    uint32         m_currentUpdateCounter;  // Current watermark for the device-initiated context updates that have been
                                            // processed by this engine.

    PAL_DISALLOW_COPY_AND_ASSIGN(ComputeEngine);
    PAL_DISALLOW_DEFAULT_CTOR(ComputeEngine);
};

} // Gfx9
} // Pal
