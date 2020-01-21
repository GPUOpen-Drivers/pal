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

#include "core/hw/gfxip/gfx6/gfx6BorderColorPalette.h"
#include "core/hw/gfxip/gfx6/gfx6CmdStream.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"

using namespace Util;

namespace Pal
{
namespace Gfx6
{

/// TA_BC_BASE_ADDR/TA_CS_BC_BASE_ADDR require 256 byte alignment.
static constexpr gpusize GpuMemAlignment = 256;

// =====================================================================================================================
BorderColorPalette::BorderColorPalette(
    const Device&                       device,
    const BorderColorPaletteCreateInfo& createInfo) :
    Pal::BorderColorPalette(*device.Parent(), createInfo, GpuMemAlignment),
    m_isGfx6(device.Parent()->ChipProperties().gfxLevel == Pal::GfxIpLevel::GfxIp6),
    m_cmdUtil(device.CmdUtil()),
    m_gpuVirtAddr(0)
{
}

// =====================================================================================================================
// Writes the PM4 commands required to bind this pipeline to pCmdSpace. Returns the next unused DWORD in pCmdSpace.
uint32* BorderColorPalette::WriteCommands(
    PipelineBindPoint bindPoint,
    gpusize           timestampGpuAddr,
    CmdStream*        pCmdStream,
    uint32*           pCmdSpace
    ) const
{
    // The address must be written in shifted 256-bit-aligned form. Note that gfx6 doesn't have a "hi" addr register.
    const uint32 addrRegValues[2] =
    {
        Get256BAddrLo(m_gpuVirtAddr),
        Get256BAddrHi(m_gpuVirtAddr)
    };

    if (bindPoint == PipelineBindPoint::Compute)
    {
        // We must wait for idle before changing the compute state.
        pCmdSpace += m_cmdUtil.BuildWaitCsIdle(pCmdStream->GetEngineType(), timestampGpuAddr, pCmdSpace);

        if (m_isGfx6)
        {
            pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmTA_CS_BC_BASE_ADDR__SI, addrRegValues[0], pCmdSpace);
        }
        else
        {
            pCmdSpace = pCmdStream->WriteSetSeqConfigRegs(mmTA_CS_BC_BASE_ADDR__CI__VI,
                                                          mmTA_CS_BC_BASE_ADDR_HI__CI__VI,
                                                          addrRegValues,
                                                          pCmdSpace);
        }
    }
    else
    {
        PAL_ASSERT(bindPoint == PipelineBindPoint::Graphics);

        if (m_isGfx6)
        {
            pCmdSpace = pCmdStream->WriteSetOneContextReg(mmTA_BC_BASE_ADDR, addrRegValues[0], pCmdSpace);
        }
        else
        {
            pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmTA_BC_BASE_ADDR,
                                                           mmTA_BC_BASE_ADDR_HI__CI__VI,
                                                           addrRegValues,
                                                           pCmdSpace);
        }
    }

    return pCmdSpace;
}

} // Gfx6
} // Pal
