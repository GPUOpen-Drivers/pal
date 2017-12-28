/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx6/gfx6Chip.h"

namespace Pal
{

namespace Gfx6
{

class CmdStream;
class Device;

// =====================================================================================================================
// Represents an "image" of the PM4 commands necessary to write a BorderColorPalette to hardware for compute pipeline
// access.
struct BorderColorPalettePm4ImgCs
{
    PM4CMDEVENTWRITE                  csPartialFlush;    ///< CS_PARTIAL_FLUSH event.

    PM4CMDSETDATA                     setBcBaseAddrHdr;  ///< PM4 set data packet
    regTA_CS_BC_BASE_ADDR             taCsBcBaseAddr;    ///< Border color palette addr low bits

    // The HI address register is not present on Gfx6. The implementation assume they are the last thing in the
    // structure (before spaceNeeded)
    regTA_CS_BC_BASE_ADDR_HI__CI__VI  taCsBcBaseAddrHi;  ///< Border color palette addr high bits

    /// Command space needed, in DWORDs. This field must always be last in the structure to not
    /// interfere w/ the actual commands contained within.
    size_t spaceNeeded;
};

// =====================================================================================================================
// Represents an "image" of the PM4 commands necessary to write a BorderColorPalette hardware for graphics pipeline
// access.
struct BorderColorPalettePm4ImgGfx
{
    PM4CMDSETDATA                  setBcBaseAddrHdr;  ///< PM4 set data packet.
    regTA_BC_BASE_ADDR             taBcBaseAddr;      ///< Border color palette address low bits

    // The HI address register is not present on Gfx6. The implementation assume they are the last thing in the
    // structure (before spaceNeeded)
    regTA_BC_BASE_ADDR_HI__CI__VI  taBcBaseAddrHi;    ///< Border color palette address high bits

    /// Command space needed, in DWORDs. This field must always be last in the structure to not
    /// interfere w/ the actual commands contained within.
    size_t spaceNeeded;
};

// =====================================================================================================================
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
};

} // Gfx6
} // Pal
