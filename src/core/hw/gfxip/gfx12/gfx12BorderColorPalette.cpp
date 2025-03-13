/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx12/gfx12BorderColorPalette.h"
#include "core/hw/gfxip/gfx12/gfx12CmdStream.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"

using namespace Util;

namespace Pal
{
namespace Gfx12
{

// BorderColorPalette require 256 byte alignment.
static constexpr gpusize GpuMemAlignment = 256;

// =====================================================================================================================
BorderColorPalette::BorderColorPalette(
    const Device&                       device,
    const BorderColorPaletteCreateInfo& createInfo) :
    Pal::BorderColorPalette(*device.Parent(), createInfo, GpuMemAlignment),
    m_gpuVirtAddr(0),
    m_gfx{},
    m_comp{}
{
    Graphics::Init(m_gfx);
    Comp::Init(m_comp);
}

// =====================================================================================================================
void BorderColorPalette::UpdateGpuMemoryBinding(
    gpusize gpuVirtAddr)
{
    m_gpuVirtAddr = gpuVirtAddr;

    Graphics::Get<mmTA_BC_BASE_ADDR,    TA_BC_BASE_ADDR>(m_gfx)->bits.ADDRESS = Get256BAddrLo(m_gpuVirtAddr);
    Graphics::Get<mmTA_BC_BASE_ADDR_HI, TA_BC_BASE_ADDR_HI>(m_gfx)->bits.ADDRESS = Get256BAddrHi(m_gpuVirtAddr);

    Comp::Get<mmTA_CS_BC_BASE_ADDR, TA_CS_BC_BASE_ADDR>(m_comp)->bits.ADDRESS = Get256BAddrLo(m_gpuVirtAddr);
    Comp::Get<mmTA_CS_BC_BASE_ADDR_HI, TA_CS_BC_BASE_ADDR_HI>(m_comp)->bits.ADDRESS = Get256BAddrHi(m_gpuVirtAddr);
}

// =====================================================================================================================
// Writes the PM4 commands required to bind this pipeline to pCmdSpace. Returns the next unused DWORD in pCmdSpace.
uint32* BorderColorPalette::WriteCommands(
    PipelineBindPoint bindPoint,
    CmdStream*        pCmdStream,
    uint32*           pCmdSpace
    ) const
{
    if (bindPoint == PipelineBindPoint::Compute)
    {
        // We must wait for idle before changing the compute state.
        pCmdSpace += CmdUtil::BuildNonSampleEventWrite(CS_PARTIAL_FLUSH, pCmdStream->GetEngineType(), pCmdSpace);

        static_assert(Comp::Size() == Comp::NumOther(), "Only UConfig expected here.");
        pCmdSpace = CmdStream::WriteSetUConfigPairs(m_comp, Comp::Size(), pCmdSpace);
    }
    else
    {
        PAL_ASSERT(bindPoint == PipelineBindPoint::Graphics);

        static_assert(Graphics::Size() == Graphics::NumContext(), "Only Context expected here.");
        pCmdSpace = CmdStream::WriteSetContextPairs(m_gfx, Graphics::Size(), pCmdSpace);
    }

    return pCmdSpace;
}

} // namespace Gfx12
} // namespace Pal
