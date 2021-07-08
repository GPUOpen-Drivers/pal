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

#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/msaaState.h"

namespace Pal
{
namespace Gfx9
{

class Device;

// =====================================================================================================================
// Gfx9 hardware layer MSAA State class: implements GFX9 specific functionality for the ApiStateObject class,
// specifically for MSAA state.
class MsaaState final : public Pal::MsaaState
{
public:
    MsaaState(const Device& device, const MsaaStateCreateInfo& msaaState);

    uint32* WriteCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    static uint32 ComputeMaxSampleDistance(
        uint32                       numSamples,
        const MsaaQuadSamplePattern& quadSamplePattern);

    static uint32* WriteSamplePositions(
        const MsaaQuadSamplePattern& samplePattern,
        uint32                       numSamples,
        CmdStream*                   pCmdStream,
        uint32*                      pCmdSpace);

    bool UsesOverRasterization() const { return (m_regs.dbEqaa.bits.OVERRASTERIZATION_AMOUNT != 0); }
    bool ShaderCanKill() const { return (m_regs.dbAlphaToMask.bits.ALPHA_TO_MASK_ENABLE != 0); }
    bool UsesLineStipple() const { return (m_regs.paScModeCntl0.bits.LINE_STIPPLE_ENABLE != 0); }
    bool ConservativeRasterizationEnabled() const { return (m_regs.paScConsRastCntl.bits.OVER_RAST_ENABLE != 0); }

    regPA_SC_MODE_CNTL_0 PaScModeCntl0() const { return m_regs.paScModeCntl0; }

    uint32 NumSamples() const { return (1 << m_log2Samples); }
    uint32 Log2NumSamples() const { return m_log2Samples; }
    uint32 Log2OcclusionQuerySamples() const { return m_log2OcclusionQuerySamples; }

    regPA_SC_CONSERVATIVE_RASTERIZATION_CNTL PaScConsRastCntl() const { return m_regs.paScConsRastCntl; }
    regPA_SC_AA_CONFIG PaScAaConfig() const { return m_paScAaConfig; }

    // THis class only owns these bits in PA_SC_AA_CONFIG.
    static const uint32 PcScAaConfigMask = (PA_SC_AA_CONFIG__MSAA_EXPOSED_SAMPLES_MASK |
                                            PA_SC_AA_CONFIG__AA_MASK_CENTROID_DTMN_MASK);

protected:
    virtual ~MsaaState() { }

    void Init(const Device& device, const MsaaStateCreateInfo& msaaState);

    uint32             m_log2Samples;
    uint32             m_log2OcclusionQuerySamples;
    regPA_SC_AA_CONFIG m_paScAaConfig; // This register is only written in the draw-time validation code.

    union
    {
        struct
        {
            uint32 waFixPostZConservativeRasterization :  1;
            uint32 gfx10_3                             :  1;
        };
        uint32  u32All;
    }  m_flags;

    struct
    {
        regDB_EQAA                                dbEqaa;
        regDB_ALPHA_TO_MASK                       dbAlphaToMask;
        uint32                                    dbReservedReg2;
        regPA_SC_AA_MASK_X0Y0_X1Y0                paScAaMask1;
        regPA_SC_AA_MASK_X0Y1_X1Y1                paScAaMask2;
        regPA_SC_MODE_CNTL_0                      paScModeCntl0;
        regPA_SC_CONSERVATIVE_RASTERIZATION_CNTL  paScConsRastCntl;

    }  m_regs;

    PAL_DISALLOW_COPY_AND_ASSIGN(MsaaState);
    PAL_DISALLOW_DEFAULT_CTOR(MsaaState);
};

} // Gfx9
} // Pal
