/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx6/gfx6Chip.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/msaaState.h"

namespace Pal
{
namespace Gfx6
{

class Device;

// =====================================================================================================================
// Gfx6 hardware layer MSAA State class: implements GFX6 specific functionality for the ApiStateObject class,
// specifically for MSAA state.
class MsaaState final : public Pal::MsaaState
{
public:
    MsaaState(const Device& device, const MsaaStateCreateInfo& msaaState);

    uint32* WriteCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    static uint32* WriteSamplePositions(
        const MsaaQuadSamplePattern& samplePattern,
        uint32                       numSamples,
        CmdStream*                   pCmdStream,
        uint32*                      pCmdSpace);

    bool UsesOverRasterization() const { return (m_regs.dbEqaa.bits.OVERRASTERIZATION_AMOUNT != 0); }

    uint32 NumSamples() const { return (1 << m_log2Samples); }
    uint32 NumShaderExportMaskSamples() const { return (1 << m_log2ShaderExportMaskSamples); }

    uint32 Log2NumSamples() const { return m_log2Samples; }
    uint32 Log2OcclusionQuerySamples() const { return m_log2OcclusionQuerySamples; }

private:
    virtual ~MsaaState() { }

    void Init(const Device& device, const MsaaStateCreateInfo& msaaState);

    uint32  m_log2Samples;
    uint32  m_log2ShaderExportMaskSamples;
    uint32  m_sampleMask;
    uint32  m_log2OcclusionQuerySamples;

    struct
    {
        regDB_EQAA                  dbEqaa;
        regPA_SC_AA_MASK_X0Y0_X1Y0  paScAaMask1;
        regPA_SC_AA_MASK_X0Y1_X1Y1  paScAaMask2;
        regPA_SC_MODE_CNTL_0        paScModeCntl0;
        regPA_SC_AA_CONFIG          paScAaConfig;
        regDB_ALPHA_TO_MASK         dbAlphaToMask;
    }  m_regs;

    PAL_DISALLOW_COPY_AND_ASSIGN(MsaaState);
    PAL_DISALLOW_DEFAULT_CTOR(MsaaState);
};

} // Gfx6
} // Pal
