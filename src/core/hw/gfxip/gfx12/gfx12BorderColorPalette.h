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

#pragma once

#include "core/hw/gfxip/borderColorPalette.h"
#include "core/hw/gfxip/gfx12/gfx12RegPairHandler.h"

namespace Pal
{

namespace Gfx12
{

class CmdStream;
class Device;

// =====================================================================================================================
// Gfx12 hardware layer BorderColorPalette class: responsible for binding the address of the border color palette in
// memory.
class BorderColorPalette final : public Pal::BorderColorPalette
{
public:
    BorderColorPalette(const Device& device, const BorderColorPaletteCreateInfo& createInfo);

    uint32* WriteCommands(PipelineBindPoint bindPoint,
                          CmdStream*        pCmdStream,
                          uint32*           pCmdSpace) const;

protected:
    virtual ~BorderColorPalette() {}

    virtual void UpdateGpuMemoryBinding(gpusize gpuVirtAddr) override;

private:
    gpusize m_gpuVirtAddr;

    static constexpr uint32 GraphicsRegs[] =
    {
        mmTA_BC_BASE_ADDR,
        mmTA_BC_BASE_ADDR_HI,
    };
    using Graphics = RegPairHandler<decltype(GraphicsRegs), GraphicsRegs>;

    static_assert(Graphics::Size() == Graphics::NumContext(), "Only Context regs expected.");

    static constexpr uint32 CompRegs[] =
    {
        mmTA_CS_BC_BASE_ADDR,
        mmTA_CS_BC_BASE_ADDR_HI,
    };
    using Comp = RegPairHandler<decltype(CompRegs), CompRegs>;

    RegisterValuePair m_gfx[Graphics::Size()];
    RegisterValuePair m_comp[Comp::Size()];

    PAL_DISALLOW_DEFAULT_CTOR(BorderColorPalette);
    PAL_DISALLOW_COPY_AND_ASSIGN(BorderColorPalette);
};

} // namespace Gfx12
} // namespace Pal
