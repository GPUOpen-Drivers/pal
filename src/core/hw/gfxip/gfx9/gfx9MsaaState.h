/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/gfx11RegPairHandler.h"
#include "core/hw/gfxip/msaaState.h"

namespace Pal
{
namespace Gfx9
{

class Device;

// =====================================================================================================================
// Gfx9 hardware layer MSAA State class: implements GFX9 specific functionality for the ApiStateObject class,
// specifically for MSAA state.
class MsaaState : public Pal::MsaaState
{
public:
    MsaaState(const Device& device, const MsaaStateCreateInfo& msaaState);

    virtual uint32* WriteCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const = 0;

    static uint32 ComputeMaxSampleDistance(
        uint32                       numSamples,
        const MsaaQuadSamplePattern& quadSamplePattern);

    static uint32* WriteSamplePositions(
        const MsaaQuadSamplePattern& samplePattern,
        uint32                       numSamples,
        CmdStream*                   pCmdStream,
        uint32*                      pCmdSpace);

    bool UsesLineStipple() const { return m_flags.usesLinesStipple; }
    bool ConservativeRasterizationEnabled() const { return (m_paScConsRastCntl.bits.OVER_RAST_ENABLE != 0); }

    uint32 NumSamples() const { return (1 << m_log2Samples); }
    uint32 Log2NumSamples() const { return m_log2Samples; }
    uint32 Log2OcclusionQuerySamples() const { return m_log2OcclusionQuerySamples; }

    bool ForceSampleRateShading() const { return m_flags.forceSampleRateShading; }

    regPA_SC_CONSERVATIVE_RASTERIZATION_CNTL PaScConsRastCntl() const { return m_paScConsRastCntl; }
    regPA_SC_AA_CONFIG PaScAaConfig() const { return m_paScAaConfig; }

    // THis class only owns these bits in PA_SC_AA_CONFIG.
    static const uint32 PcScAaConfigMask = (PA_SC_AA_CONFIG__MSAA_EXPOSED_SAMPLES_MASK |
                                            PA_SC_AA_CONFIG__AA_MASK_CENTROID_DTMN_MASK);

protected:
    virtual ~MsaaState() { }

    const uint32                             m_log2Samples;
    const uint32                             m_log2OcclusionQuerySamples;
    regPA_SC_AA_CONFIG                       m_paScAaConfig;     // Written at draw-time.
    regPA_SC_CONSERVATIVE_RASTERIZATION_CNTL m_paScConsRastCntl; // Written at draw-time.

    union
    {
        struct
        {
            uint32 waFixPostZConservativeRasterization :  1;
            uint32 forceSampleRateShading              :  1;
            uint32 usesLinesStipple                    :  1;
        };
        uint32  u32All;
    }  m_flags;

    PAL_DISALLOW_COPY_AND_ASSIGN(MsaaState);
    PAL_DISALLOW_DEFAULT_CTOR(MsaaState);
};

// =====================================================================================================================
// GFX11 RS64 specific implemenation of Msaa State class.
class Gfx11MsaaStateRs64 final : public Gfx9::MsaaState
{
public:
    Gfx11MsaaStateRs64(const Device& device, const MsaaStateCreateInfo& msaaState);

    virtual uint32* WriteCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const;

protected:
    virtual ~Gfx11MsaaStateRs64() { }

private:
    static constexpr uint32 Registers[] =
    {
        mmDB_EQAA,
        mmDB_ALPHA_TO_MASK,
        mmPA_SC_AA_MASK_X0Y0_X1Y0,
        mmPA_SC_AA_MASK_X0Y1_X1Y1,
        mmPA_SC_MODE_CNTL_0
    };

    using Regs = Gfx11PackedRegPairHandler<decltype(Registers), Registers>;

    PackedRegisterPair m_regs[Regs::NumPackedRegPairs()];

    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx11MsaaStateRs64);
    PAL_DISALLOW_DEFAULT_CTOR(Gfx11MsaaStateRs64);
};

// =====================================================================================================================
// GFX11 F32 specific implemenation of Msaa State class.
class Gfx11MsaaStateF32 final : public Gfx9::MsaaState
{
public:
    Gfx11MsaaStateF32(const Device& device, const MsaaStateCreateInfo& msaaState);

    virtual uint32* WriteCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const;

protected:
    virtual ~Gfx11MsaaStateF32() { }

private:
    static constexpr uint32 Registers[] =
    {
        mmDB_EQAA,
        mmDB_ALPHA_TO_MASK,
        mmPA_SC_AA_MASK_X0Y0_X1Y0,
        mmPA_SC_AA_MASK_X0Y1_X1Y1,
        mmPA_SC_MODE_CNTL_0
    };
    using Regs = Gfx11RegPairHandler<decltype(Registers), Registers>;

    static_assert(Regs::Size() == Regs::NumContext(), "Only context regs expected.");

    RegisterValuePair m_regs[Regs::Size()];

    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx11MsaaStateF32);
    PAL_DISALLOW_DEFAULT_CTOR(Gfx11MsaaStateF32);
};

// =====================================================================================================================
// GFX10 specific implemenation of Msaa State class.
class Gfx10MsaaState final : public Gfx9::MsaaState
{
public:
    Gfx10MsaaState(const Device& device, const MsaaStateCreateInfo& msaaState);

    virtual uint32* WriteCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const;

protected:
    virtual ~Gfx10MsaaState() { }

private:
    struct
    {
        regDB_EQAA                 dbEqaa;
        regDB_ALPHA_TO_MASK        dbAlphaToMask;
        regPA_SC_AA_MASK_X0Y0_X1Y0 paScAaMask1;
        regPA_SC_AA_MASK_X0Y1_X1Y1 paScAaMask2;
        regPA_SC_MODE_CNTL_0       paScModeCntl0;
    }  m_regs;

    uint32 m_dbReservedReg2;

    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx10MsaaState);
    PAL_DISALLOW_DEFAULT_CTOR(Gfx10MsaaState);
};

} // Gfx9
} // Pal
