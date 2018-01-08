/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
class Device;

// Represents an "image" of the PM4 commands necessary to write a BorderColorPalette to hardware for compute pipeline
// access.
struct BorderColorPalettePm4ImgCs
{
    PM4ME_NON_SAMPLE_EVENT_WRITE    csPartialFlush;    // CS_PARTIAL_FLUSH event.

    PM4_PFP_SET_UCONFIG_REG         setBcBaseAddrHdr;  // PM4 set data packet
    regTA_CS_BC_BASE_ADDR           taCsBcBaseAddr;    // Border color palette addr low bits
    regTA_CS_BC_BASE_ADDR_HI        taCsBcBaseAddrHi;  // Border color palette addr high bits
};

// Represents an "image" of the PM4 commands necessary to write a BorderColorPalette hardware for graphics pipeline
// access.
struct BorderColorPalettePm4ImgGfx
{
    PM4_PFP_SET_CONTEXT_REG     setBcBaseAddrHdr;  // PM4 set data packet.
    regTA_BC_BASE_ADDR          taBcBaseAddr;      // Border color palette address low bits
    regTA_BC_BASE_ADDR_HI       taBcBaseAddrHi;    // Border color palette address high bits
};

// =====================================================================================================================
// Gfx9 hardware layer BorderColorPalette class: responsible for binding the address of the border color palette in
// memory.
class BorderColorPalette : public Pal::BorderColorPalette
{
public:
    BorderColorPalette(const Device& device, const BorderColorPaletteCreateInfo& createInfo);

    uint32* WriteCommands(PipelineBindPoint bindPoint, CmdStream* pCmdStream, uint32* pCmdSpace) const;

protected:
    virtual ~BorderColorPalette() {}

    virtual void UpdateGpuMemoryBinding(gpusize gpuVirtAddr) override;

private:
    void BuildPm4Headers();

    const Device& m_device;

    BorderColorPalettePm4ImgCs  m_csPm4Cmds;
    BorderColorPalettePm4ImgGfx m_gfxPm4Cmds;

    PAL_DISALLOW_DEFAULT_CTOR(BorderColorPalette);
    PAL_DISALLOW_COPY_AND_ASSIGN(BorderColorPalette);
};

} // Gfx9
} // Pal
