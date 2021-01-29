/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/borderColorPalette.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"

namespace Pal
{

namespace Gfx9
{

class CmdStream;
class CmdUtil;
class Device;

// =====================================================================================================================
// Gfx9 hardware layer BorderColorPalette class: responsible for binding the address of the border color palette in
// memory.
class BorderColorPalette final : public Pal::BorderColorPalette
{
public:
    BorderColorPalette(const Device& device, const BorderColorPaletteCreateInfo& createInfo);

    uint32* WriteCommands(PipelineBindPoint bindPoint,
                          gpusize           timestampGpuAddr,
                          CmdStream*        pCmdStream,
                          uint32*           pCmdSpace) const;

protected:
    virtual ~BorderColorPalette() {}

    virtual void UpdateGpuMemoryBinding(gpusize gpuVirtAddr) override { m_gpuVirtAddr = gpuVirtAddr; }

private:
    const CmdUtil& m_cmdUtil;
    gpusize        m_gpuVirtAddr;

    PAL_DISALLOW_DEFAULT_CTOR(BorderColorPalette);
    PAL_DISALLOW_COPY_AND_ASSIGN(BorderColorPalette);
};

} // Gfx9
} // Pal
