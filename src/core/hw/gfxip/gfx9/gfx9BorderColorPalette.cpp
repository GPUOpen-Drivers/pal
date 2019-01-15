/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx9/gfx9BorderColorPalette.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// TA_BC_BASE_ADDR/TA_CS_BC_BASE_ADDR require 256 byte alignment.
static constexpr gpusize GpuMemAlignment = 256;

// =====================================================================================================================
BorderColorPalette::BorderColorPalette(
    const Device&                       device,
    const BorderColorPaletteCreateInfo& createInfo) :
    Pal::BorderColorPalette(*device.Parent(), createInfo, GpuMemAlignment),
    m_device(device)
{
    BuildPm4Headers();
}

// =====================================================================================================================
// Sets up the image of PM4 commands used to write this border color palette to hardware.
void BorderColorPalette::BuildPm4Headers()
{
    const CmdUtil& cmdUtil = m_device.CmdUtil();

    // We cannot fully initialize the GPU VA of the palette until it is bound to GPU memory.  This is done in
    // HwlUpdateMemoryBinding(). Instead of explicitly setting each register value to zero, we can just zero-out the
    // PM4 image and only setup the fields we know at init-time.
    memset(&m_csPm4Cmds,  0, sizeof(m_csPm4Cmds));
    memset(&m_gfxPm4Cmds, 0, sizeof(m_gfxPm4Cmds));

    cmdUtil.BuildNonSampleEventWrite(CS_PARTIAL_FLUSH, EngineTypeCompute, &m_csPm4Cmds.csPartialFlush);
    cmdUtil.BuildSetSeqConfigRegs(mmTA_CS_BC_BASE_ADDR,
                                  mmTA_CS_BC_BASE_ADDR_HI,
                                  &m_csPm4Cmds.setBcBaseAddrHdr);

    cmdUtil.BuildSetSeqContextRegs(mmTA_BC_BASE_ADDR,
                                   mmTA_BC_BASE_ADDR_HI,
                                   &m_gfxPm4Cmds.setBcBaseAddrHdr);
}

// =====================================================================================================================
// Writes the PM4 commands required to bind this pipeline to pCmdSpace. Returns the next unused DWORD in pCmdSpace.
uint32* BorderColorPalette::WriteCommands(
    PipelineBindPoint bindPoint,
    CmdStream*        pCmdStream,
    uint32*           pCmdSpace
    ) const
{
    const void* pPm4Cmds      = nullptr;
    size_t      pm4CmdEntries = 0;

    switch (bindPoint)
    {
    case PipelineBindPoint::Compute:
        pPm4Cmds      = &m_csPm4Cmds;
        pm4CmdEntries = (sizeof(m_csPm4Cmds) / sizeof(uint32));
        break;
    case PipelineBindPoint::Graphics:
        pPm4Cmds      = &m_gfxPm4Cmds;
        pm4CmdEntries = (sizeof(m_gfxPm4Cmds) / sizeof(uint32));
        break;
    default:
        PAL_ALERT_ALWAYS();
        break;
    }

    return pCmdStream->WritePm4Image(pm4CmdEntries, pPm4Cmds, pCmdSpace);
}

// =====================================================================================================================
// Notifies the HWL that the GPU memory binding for this border color palette has changed.
void BorderColorPalette::UpdateGpuMemoryBinding(
    gpusize gpuVirtAddr)
{
    const uint32 addrLow  = Get256BAddrLo(gpuVirtAddr);
    const uint32 addrHigh = Get256BAddrHi(gpuVirtAddr);

    m_csPm4Cmds.taCsBcBaseAddr.bits.ADDRESS   = addrLow;
    m_csPm4Cmds.taCsBcBaseAddrHi.bits.ADDRESS = addrHigh;

    m_gfxPm4Cmds.taBcBaseAddr.bits.ADDRESS    = addrLow;
    m_gfxPm4Cmds.taBcBaseAddrHi.bits.ADDRESS  = addrHigh;
}

} // Gfx9
} // Pal
