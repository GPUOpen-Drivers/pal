/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
// Represents an "image" of the PM4 commands necessary to write MSAA-specific state to hardware. The required register
// writes are grouped into sets based on sequential register addresses, so that we can minimize the amount of PM4 space
// needed by setting several reg's in each packet.
struct MsaaStatePm4Img
{
    PM4CMDSETDATA                         hdrDbEqaa;        // 1st PM4 set data packet
    regDB_EQAA                            dbEqaa;           // DB EQAA control register

    PM4CMDSETDATA                         hdrPaScAaMask;    // 2nd PM4 set data packet
    regPA_SC_AA_MASK_X0Y0_X1Y0            paScAaMask1;      // PA SC AA mask #1
    regPA_SC_AA_MASK_X0Y1_X1Y1            paScAaMask2;      // PA SC AA mask #2

    PM4CMDSETDATA                         hdrPaScModeCntl0; // 3rd PM4 set data packet
    regPA_SC_MODE_CNTL_0                  paScModeCntl0;    // PA SC MDOE control register 0

    PM4CONTEXTREGRMW                      paScAaConfigRmw;  // PA SC AA config register

    // 5th PM4 packet, a read/write/modify of DB_ALPHA_TO_MASK
    PM4CONTEXTREGRMW                      dbAlphaToMaskRmw;

    // Command space needed, in DWORDs. This field must always be last in the structure to not
    // interfere w/ the actual commands contained within.
    size_t                                spaceNeeded;
};

// =====================================================================================================================
// Centroid priority registers
struct PaScCentroid
{
    regPA_SC_CENTROID_PRIORITY_0 priority0;    // Centroid Priorities #0
    regPA_SC_CENTROID_PRIORITY_1 priority1;    // Centroid Priorities #1
};

// =====================================================================================================================
// MSAA sample locations registers
struct PaScSampleQuad
{
    // MSAA sample locations for pixel 0,0 in a 2x2 Quad
    regPA_SC_AA_SAMPLE_LOCS_PIXEL_X0Y0_0  X0Y0[NumSampleQuadRegs];
    // MSAA sample locations for pixel 1,0 in a 2x2 Quad
    regPA_SC_AA_SAMPLE_LOCS_PIXEL_X0Y0_0  X1Y0[NumSampleQuadRegs];
    // MSAA sample locations for pixel 0,1 in a 2x2 Quad
    regPA_SC_AA_SAMPLE_LOCS_PIXEL_X0Y0_0  X0Y1[NumSampleQuadRegs];
    // MSAA sample locations for pixel 1,1 in a 2x2 Quad
    regPA_SC_AA_SAMPLE_LOCS_PIXEL_X0Y0_0  X1Y1[NumSampleQuadRegs];
};

// =====================================================================================================================
// Represents an "image" of the PM4 commands necessary to write MSAA-sample position state to hardware. The required
// register writes are grouped into sets based on sequential register addresses, so that we can minimize the amount of
// PM4 space needed by setting several reg's in each packet.
struct MsaaSamplePositionsPm4Img
{
    PM4CMDSETDATA  hdrPaScCentroidPrio;  // 1st PM4 set data packet
    PaScCentroid   paScCentroid;

    PM4CMDSETDATA  hdrPaScSampleQuad;    // 2nd PM4 set data packet
    PaScSampleQuad paScSampleQuad;
};

constexpr uint32 SizeOfMsaaSamplePositionsPm4ImageInDwords = sizeof(MsaaSamplePositionsPm4Img) / sizeof(uint32);

// =====================================================================================================================
// Gfx6 hardware layer MSAA State class: implements GFX6 specific functionality for the ApiStateObject class,
// specifically for MSAA state.
class MsaaState : public Pal::MsaaState
{
public:
    MsaaState();

    Result Init(const Device& device, const MsaaStateCreateInfo& msaaState);

    static uint32 ComputeMaxSampleDistance(uint32                       numSamples,
                                           const MsaaQuadSamplePattern& quadSamplePattern);

    static void BuildSamplePosPm4Image(
        const CmdUtil&               cmdUtil,
        MsaaSamplePositionsPm4Img*   pSamplePosPm4Image,
        uint32                       numSamples,
        const MsaaQuadSamplePattern& quadSamplePattern);

    static void SetCentroidPriorities(
        PaScCentroid*   pPaScCentroid,
        const Offset2d* pSampleLocations,
        uint32          numSamples);

    static void SetQuadSamplePattern(
        PaScSampleQuad*              pPaScSampleQuad,
        const MsaaQuadSamplePattern& quadSamplePattern,
        uint32                       numSamples);

    static size_t Pm4ImgSize(const MsaaStateCreateInfo& msaaState);
    uint32* WriteCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    bool UsesOverRasterization() const { return (m_pm4Image.dbEqaa.bits.OVERRASTERIZATION_AMOUNT != 0); }

    uint32 NumSamples() const { return (1 << m_log2Samples); }
    uint32 NumShaderExportMaskSamples() const { return (1 << m_log2ShaderExportMaskSamples); }
    uint32 Log2NumSamples() const { return m_log2Samples; }
    uint32 Log2OcclusionQuerySamples() const { return m_log2OcclusionQuerySamples; }

protected:
    virtual ~MsaaState() {}

private:
    void BuildPm4Headers(const Device& device);

    uint32  m_log2Samples;
    uint32  m_log2ShaderExportMaskSamples;
    uint32  m_sampleMask;
    uint32  m_log2OcclusionQuerySamples;

    MsaaStatePm4Img  m_pm4Image;

    PAL_DISALLOW_COPY_AND_ASSIGN(MsaaState);
};

} // Gfx6
} // Pal
