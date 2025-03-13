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

#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/msaaState.h"
#include "core/hw/gfxip/gfx12/gfx12RegPairHandler.h"

namespace Pal
{
namespace Gfx12
{

class Device;

// =====================================================================================================================
// Gfx12 hardware layer MSAA State class: implements GFX12 specific functionality for the ApiStateObject class,
// specifically for MSAA state.
class MsaaState final : public Pal::MsaaState
{
public:
    MsaaState(const Device& device, const MsaaStateCreateInfo& createInfo);

    uint32* WriteCommands(uint32* pCmdSpace) const;

    static void SortSamples(uint32                       numSamplesPerPixel,
                            const MsaaQuadSamplePattern& quadSamplePattern,
                            uint32*                      pOutMaxSampleDist,
                            uint8                        outSortedIndices[MaxMsaaRasterizerSamples]);

    regPA_SC_MODE_CNTL_1 PaScModeCntl1() const { return m_paScModeCntl1; }
    regPA_SC_CONSERVATIVE_RASTERIZATION_CNTL PaScConsRastCntl() const
        { return Regs::GetC<mmPA_SC_CONSERVATIVE_RASTERIZATION_CNTL, PA_SC_CONSERVATIVE_RASTERIZATION_CNTL>(m_regs); }
    regPA_SC_AA_CONFIG PaScAaConfig() const { return Regs::GetC<mmPA_SC_AA_CONFIG, PA_SC_AA_CONFIG>(m_regs); }

private:
    virtual ~MsaaState() { }

    static constexpr uint32 Registers[] =
    {
        Chip::mmPA_SC_AA_CONFIG,
        Chip::mmPA_SC_MODE_CNTL_0,
        Chip::mmDB_ALPHA_TO_MASK,
        Chip::mmDB_EQAA,
        Chip::mmPA_SC_AA_MASK_X0Y0_X1Y0,
        Chip::mmPA_SC_AA_MASK_X0Y1_X1Y1,
        Chip::mmPA_SC_CONSERVATIVE_RASTERIZATION_CNTL,
    };
    using Regs = RegPairHandler<decltype(Registers), Registers>;

    static_assert(Regs::Size() == Regs::NumContext(), "Only context regs expected.");

    RegisterValuePair       m_regs[Regs::Size()];

    static constexpr uint32 PaSuModeCntl1Default =
        (1 << PA_SC_MODE_CNTL_1__WALK_ALIGN8_PRIM_FITS_ST__SHIFT)                |
        (1 << PA_SC_MODE_CNTL_1__WALK_FENCE_ENABLE__SHIFT)                       |
        (1 << PA_SC_MODE_CNTL_1__TILE_WALK_ORDER_ENABLE__SHIFT)                  |
        (1 << PA_SC_MODE_CNTL_1__SUPERTILE_WALK_ORDER_ENABLE__SHIFT)             |
        (1 << PA_SC_MODE_CNTL_1__MULTI_SHADER_ENGINE_PRIM_DISCARD_ENABLE__SHIFT) |
        (1 << PA_SC_MODE_CNTL_1__FORCE_EOV_CNTDWN_ENABLE__SHIFT)                 |
        (1 << PA_SC_MODE_CNTL_1__FORCE_EOV_REZ_ENABLE__SHIFT);

    // MSAA state owns PA_SC_MODE_CNTL1, but there is limitation that we have to program it during draw
    // validation, so store a dedicated m_paScModeCntl1 out of m_regs.
    Chip::PA_SC_MODE_CNTL_1 m_paScModeCntl1;

    PAL_DISALLOW_COPY_AND_ASSIGN(MsaaState);
    PAL_DISALLOW_DEFAULT_CTOR(MsaaState);
};

} // namespace Gfx12
} // namespace Pal
