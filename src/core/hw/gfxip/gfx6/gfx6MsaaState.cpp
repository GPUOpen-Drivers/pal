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

#include "core/hw/gfxip/gfx6/gfx6CmdStream.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6MsaaState.h"
#include "palInlineFuncs.h"

using namespace Util;

namespace Pal
{
namespace Gfx6
{

constexpr uint32 NumSampleLocations = 16;

struct PaScCentroid
{
    regPA_SC_CENTROID_PRIORITY_0 priority0;    // Centroid Priorities #0
    regPA_SC_CENTROID_PRIORITY_1 priority1;    // Centroid Priorities #1
};

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
MsaaState::MsaaState(
    const Device&              device,
    const MsaaStateCreateInfo& createInfo)
    :
    Pal::MsaaState(),
    m_log2Samples(0),
    m_log2ShaderExportMaskSamples(0),
    m_sampleMask(0),
    m_log2OcclusionQuerySamples(0)
{
    memset(&m_regs, 0, sizeof(m_regs));
    Init(device, createInfo);
}

// =====================================================================================================================
// Copies this MSAA state's PM4 commands into the specified command buffer. Returns the next unused DWORD in pCmdSpace.
uint32* MsaaState::WriteCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    constexpr uint32 PaScAaConfigMask  = static_cast<uint32>(~(PA_SC_AA_CONFIG__MAX_SAMPLE_DIST_MASK));
    constexpr uint32 DbAlphaToMaskMask = static_cast<uint32>(DB_ALPHA_TO_MASK__ALPHA_TO_MASK_OFFSET0_MASK |
                                                             DB_ALPHA_TO_MASK__ALPHA_TO_MASK_OFFSET1_MASK |
                                                             DB_ALPHA_TO_MASK__ALPHA_TO_MASK_OFFSET2_MASK |
                                                             DB_ALPHA_TO_MASK__ALPHA_TO_MASK_OFFSET3_MASK |
                                                             DB_ALPHA_TO_MASK__OFFSET_ROUND_MASK);

    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_EQAA, m_regs.dbEqaa.u32All, pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmPA_SC_AA_MASK_X0Y0_X1Y0,
                                                   mmPA_SC_AA_MASK_X0Y1_X1Y1,
                                                   &m_regs.paScAaMask1,
                                                   pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmPA_SC_MODE_CNTL_0, m_regs.paScModeCntl0.u32All, pCmdSpace);

    pCmdSpace = pCmdStream->WriteContextRegRmw(mmPA_SC_AA_CONFIG,
                                               PaScAaConfigMask,
                                               m_regs.paScAaConfig.u32All,
                                               pCmdSpace);

    return pCmdStream->WriteContextRegRmw(mmDB_ALPHA_TO_MASK,
                                          DbAlphaToMaskMask,
                                          m_regs.dbAlphaToMask.u32All,
                                          pCmdSpace);
}

// =====================================================================================================================
// Initializes the register state related to MSAA.
void MsaaState::Init(
    const Device&              device,
    const MsaaStateCreateInfo& msaaState)
{
    // pre GFX9 HW doesn't support conservative rasterization.
    PAL_ASSERT(msaaState.flags.enableConservativeRasterization == 0);

    m_log2Samples                 = Log2(msaaState.coverageSamples);
    m_sampleMask                  = msaaState.sampleMask;
    m_log2ShaderExportMaskSamples = Log2(msaaState.shaderExportMaskSamples);
    m_log2OcclusionQuerySamples   = Log2(msaaState.occlusionQuerySamples);

    // Use the supplied sample mask to initialize the PA_SC_AA_MASK_** registers:
    uint32 usedMask    = (m_sampleMask & ((1 << NumSamples()) - 1));
    uint32 maskSamples = NumSamples();

    // HW requires us to replicate the sample mask to all 16 bits if there are fewer than 16 samples active.
    while (maskSamples < 16)
    {
        usedMask     |= (usedMask << maskSamples);
        maskSamples <<= 1;
    }

    m_regs.paScAaMask1.u32All = ((usedMask << 16) | usedMask);
    m_regs.paScAaMask2.u32All = ((usedMask << 16) | usedMask);

    // Setup the PA_SC_MODE_CNTL_0 register
    m_regs.paScModeCntl0.u32All = 0;
    m_regs.paScModeCntl0.bits.LINE_STIPPLE_ENABLE  = msaaState.flags.enableLineStipple;
    m_regs.paScModeCntl0.bits.VPORT_SCISSOR_ENABLE = 1;
    m_regs.paScModeCntl0.bits.MSAA_ENABLE          = ((NumSamples() > 1) ? 1 : 0);

    // Setup the PA_SC_AA_CONFIG and DB_EQAA registers.
    m_regs.dbEqaa.bits.STATIC_ANCHOR_ASSOCIATIONS = 1;
    m_regs.dbEqaa.bits.HIGH_QUALITY_INTERSECTIONS = 1;
    m_regs.dbEqaa.bits.INCOHERENT_EQAA_READS      = 1;

    // INTERPOLATE_COMP_Z was turned off at default as a workaround to prevent corruption in depth resources
    // due to an issue in EQAA hardware implementation. When EQAA is on, the corruption can occur
    // in any apps that use depth resources. This will have no performance impact,
    // and it will only impact quality in the eqaa cases (when rasterization rate is greater than the number
    // of depth samples this basically doesn't happen in our drivers today).
    m_regs.dbEqaa.bits.INTERPOLATE_COMP_Z = (device.Settings().waDisableDbEqaaInterpolateCompZ) ? 0 : 1;

    if (msaaState.coverageSamples > 1)
    {
        m_regs.paScAaConfig.bits.MSAA_NUM_SAMPLES     = m_log2Samples;
        m_regs.paScAaConfig.bits.MSAA_EXPOSED_SAMPLES = Log2(msaaState.exposedSamples);

        m_regs.dbEqaa.bits.MAX_ANCHOR_SAMPLES        = Log2(msaaState.depthStencilSamples);
        m_regs.dbEqaa.bits.PS_ITER_SAMPLES           = Log2(msaaState.pixelShaderSamples);
        m_regs.dbEqaa.bits.MASK_EXPORT_NUM_SAMPLES   = m_log2ShaderExportMaskSamples;
        m_regs.dbEqaa.bits.ALPHA_TO_MASK_NUM_SAMPLES = Log2(msaaState.alphaToCoverageSamples);
        m_regs.dbEqaa.bits.OVERRASTERIZATION_AMOUNT  = m_log2ShaderExportMaskSamples - Log2(msaaState.sampleClusters);

        if (device.WaDbOverRasterization() && UsesOverRasterization())
        {
            // Apply the "DB Over-Rasterization" workaround:
            // The DB has a bug with early-Z where the DB kills pixels when over-rasterization is enabled. Most of
            // the time, simply forcing post-Z over-rasterization via DB_EQAA is a sufficient workaround. The
            // Gfx6GraphicsPipeline class handles the cases where it is not a sufficient workaround, such as when
            // early-Z is used with depth testing enabled.
            m_regs.dbEqaa.bits.ENABLE_POSTZ_OVERRASTERIZATION = 1;
        }
    }

    // The following code sets up the alpha to mask dithering pattern.
    // If all offsets are set to the same value then there will be no dithering, and the number of gradations of
    // coverage on an edge will be at-most equal to the number of (coverage) samples in the current AA mode. The
    // chosen values set up a different offset for each pixel of a 2x2 quad, allowing many more levels of apparent
    // coverage. The graphics pipeline also writes to DB_ALPHA_TO_MASK so we must use a read/modify/write packet
    // to set these fields.
    if (msaaState.flags.disableAlphaToCoverageDither)
    {
        m_regs.dbAlphaToMask.bits.ALPHA_TO_MASK_OFFSET0 = 2;
        m_regs.dbAlphaToMask.bits.ALPHA_TO_MASK_OFFSET1 = 2;
        m_regs.dbAlphaToMask.bits.ALPHA_TO_MASK_OFFSET2 = 2;
        m_regs.dbAlphaToMask.bits.ALPHA_TO_MASK_OFFSET3 = 2;
        m_regs.dbAlphaToMask.bits.OFFSET_ROUND          = 0;
    }
    else
    {
        m_regs.dbAlphaToMask.bits.ALPHA_TO_MASK_OFFSET0 = 3;
        m_regs.dbAlphaToMask.bits.ALPHA_TO_MASK_OFFSET1 = 1;
        m_regs.dbAlphaToMask.bits.ALPHA_TO_MASK_OFFSET2 = 0;
        m_regs.dbAlphaToMask.bits.ALPHA_TO_MASK_OFFSET3 = 2;
        m_regs.dbAlphaToMask.bits.OFFSET_ROUND          = 1;
    }
}

// =====================================================================================================================
// Sets the centroid priority register fields based on the specified sample positions.
static void SetCentroidPriorities(
    PaScCentroid*   pPaScCentroid,
    const Offset2d* pSampleLocations,
    uint32          numSamples)
{
    // distance from center of the pixel for each sample location
    uint32 distances[NumSampleLocations];
    // list of sample indices sorted by distance from center
    uint32 centroidPriorities[NumSampleLocations];

    // loop through each sample to calculate pythagorean distance from center
    for (uint32 i = 0; i < numSamples; i++)
    {
        distances[i] = (pSampleLocations[i].x * pSampleLocations[i].x) +
                       (pSampleLocations[i].y * pSampleLocations[i].y);
    }

    // Construct the sorted sample order
    for (uint32 i = 0; i < numSamples; i++)
    {
        // Loop through the distance array and find the smallest remaining distance
        uint32 minIdx = 0;
        for(uint32 j = 1; j < numSamples; j++)
        {
            if (distances[j] < distances[minIdx])
            {
                minIdx = j;
            }
        }
        // Add the sample index to our priority list
        centroidPriorities[i] = minIdx;

        // Then change the distance for that sample to max to mark it as sorted
        distances[minIdx] = 0xFFFFFFFF;
    }

    PAL_ASSERT((numSamples == 1) || (numSamples == 2) || (numSamples == 4) || (numSamples == 8) || (numSamples == 16));
    const uint32 sampleMask = numSamples - 1;

    // If using fewer than 16 samples, we must fill the extra distance fields by re-cycling through the samples in
    // order as many times as necessary to fill all fields.
    pPaScCentroid->priority0.u32All =
        (centroidPriorities[0]              << PA_SC_CENTROID_PRIORITY_0__DISTANCE_0__SHIFT) |
        (centroidPriorities[1 & sampleMask] << PA_SC_CENTROID_PRIORITY_0__DISTANCE_1__SHIFT) |
        (centroidPriorities[2 & sampleMask] << PA_SC_CENTROID_PRIORITY_0__DISTANCE_2__SHIFT) |
        (centroidPriorities[3 & sampleMask] << PA_SC_CENTROID_PRIORITY_0__DISTANCE_3__SHIFT) |
        (centroidPriorities[4 & sampleMask] << PA_SC_CENTROID_PRIORITY_0__DISTANCE_4__SHIFT) |
        (centroidPriorities[5 & sampleMask] << PA_SC_CENTROID_PRIORITY_0__DISTANCE_5__SHIFT) |
        (centroidPriorities[6 & sampleMask] << PA_SC_CENTROID_PRIORITY_0__DISTANCE_6__SHIFT) |
        (centroidPriorities[7 & sampleMask] << PA_SC_CENTROID_PRIORITY_0__DISTANCE_7__SHIFT);

    pPaScCentroid->priority1.u32All =
        (centroidPriorities[ 8 & sampleMask] << PA_SC_CENTROID_PRIORITY_1__DISTANCE_8__SHIFT)  |
        (centroidPriorities[ 9 & sampleMask] << PA_SC_CENTROID_PRIORITY_1__DISTANCE_9__SHIFT)  |
        (centroidPriorities[10 & sampleMask] << PA_SC_CENTROID_PRIORITY_1__DISTANCE_10__SHIFT) |
        (centroidPriorities[11 & sampleMask] << PA_SC_CENTROID_PRIORITY_1__DISTANCE_11__SHIFT) |
        (centroidPriorities[12 & sampleMask] << PA_SC_CENTROID_PRIORITY_1__DISTANCE_12__SHIFT) |
        (centroidPriorities[13 & sampleMask] << PA_SC_CENTROID_PRIORITY_1__DISTANCE_13__SHIFT) |
        (centroidPriorities[14 & sampleMask] << PA_SC_CENTROID_PRIORITY_1__DISTANCE_14__SHIFT) |
        (centroidPriorities[15 & sampleMask] << PA_SC_CENTROID_PRIORITY_1__DISTANCE_15__SHIFT);
}

// =====================================================================================================================
// Sets the sample locations register in the passed sample positions pm4 image.
static void SetQuadSamplePattern(
    PaScSampleQuad*              pPaScSampleQuad,
    const MsaaQuadSamplePattern& quadSamplePattern,
    uint32                       numSamples)
{
    const Offset2d* pSampleLocations = nullptr;

    constexpr size_t NumOfPixelsInQuad         = 4;
    constexpr size_t NumSamplesPerRegister     = 4;
    constexpr size_t BitsPerLocationCoordinate = 4;
    constexpr size_t BitMaskLocationCoordinate = 0xF;

    for (uint32 pixIdx = 0; pixIdx < NumOfPixelsInQuad; ++pixIdx)
    {
        regPA_SC_AA_SAMPLE_LOCS_PIXEL_X0Y0_0* pSampleQuadXY0 = nullptr;

        // The pixel coordinates within a sampling pattern (quad) are mapped to the registers as following:
        //    ------------------------------       ---------------
        //    | (topLeft)   | (topRight)   |       | X0Y0 | X1Y0 |
        //    ------------------------------  ==>  ---------------
        //    | (bottomLeft)| (bottomRight)|       | X0Y1 | X1Y1 |
        //    ------------------------------       ---------------
        //

        switch (pixIdx)
        {
        case 0:
            pSampleLocations = &quadSamplePattern.topLeft[0];
            pSampleQuadXY0   = &pPaScSampleQuad->X0Y0[0];
            break;
        case 1:
            pSampleLocations = &quadSamplePattern.topRight[0];
            pSampleQuadXY0   = &pPaScSampleQuad->X1Y0[0];
            break;
        case 2:
            pSampleLocations = &quadSamplePattern.bottomLeft[0];
            pSampleQuadXY0   = &pPaScSampleQuad->X0Y1[0];
            break;
        case 3:
            pSampleLocations = &quadSamplePattern.bottomRight[0];
            pSampleQuadXY0   = &pPaScSampleQuad->X1Y1[0];
            break;
        default:
            PAL_ASSERT_ALWAYS();
            break;
        }

        regPA_SC_AA_SAMPLE_LOCS_PIXEL_X0Y0_0* pSampleQuad = pSampleQuadXY0;
        for (uint32 sampleIdx = 0; sampleIdx < numSamples; ++sampleIdx)
        {
            uint32 sampleRegisterIdx = sampleIdx / NumSamplesPerRegister;
            uint32 sampleLocationIdx = sampleIdx % NumSamplesPerRegister;

            pSampleQuad = pSampleQuadXY0 + sampleRegisterIdx;

            const size_t shiftX = (BitsPerLocationCoordinate * 2) * sampleLocationIdx;
            const size_t shiftY = (shiftX + BitsPerLocationCoordinate);

            pSampleQuad->u32All |= ((pSampleLocations[sampleIdx].x & BitMaskLocationCoordinate) << shiftX);
            pSampleQuad->u32All |= ((pSampleLocations[sampleIdx].y & BitMaskLocationCoordinate) << shiftY);
        }
    }
}

// =====================================================================================================================
// Helper function which computes the maximum sample distance (from pixel center) based on the specified sample
// positions.
static uint32 ComputeMaxSampleDistance(
    uint32                       numSamples,
    const MsaaQuadSamplePattern& quadSamplePattern)
{
    uint32 distance = 0;

    for (uint32 i = 0; i < numSamples; ++i)
    {
        distance = Max(distance, static_cast<uint32>(Max(abs(quadSamplePattern.topLeft[i].x),
                                                         abs(quadSamplePattern.topLeft[i].y))));
        distance = Max(distance, static_cast<uint32>(Max(abs(quadSamplePattern.topRight[i].x),
                                                         abs(quadSamplePattern.topRight[i].y))));
        distance = Max(distance, static_cast<uint32>(Max(abs(quadSamplePattern.bottomLeft[i].x),
                                                         abs(quadSamplePattern.bottomLeft[i].y))));
        distance = Max(distance, static_cast<uint32>(Max(abs(quadSamplePattern.bottomRight[i].x),
                                                         abs(quadSamplePattern.bottomRight[i].y))));
    }

    return distance;
}

// =====================================================================================================================
uint32* MsaaState::WriteSamplePositions(
    const MsaaQuadSamplePattern& samplePattern,
    uint32                       numSamples,
    CmdStream*                   pCmdStream,
    uint32*                      pCmdSpace)
{
    PaScCentroid paScCentroid = { };
    SetCentroidPriorities(&paScCentroid, &samplePattern.topLeft[0], numSamples);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmPA_SC_CENTROID_PRIORITY_0,
                                                   mmPA_SC_CENTROID_PRIORITY_1,
                                                   &paScCentroid,
                                                   pCmdSpace);

    PaScSampleQuad paScSampleQuad = { };
    SetQuadSamplePattern(&paScSampleQuad, samplePattern, numSamples);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmPA_SC_AA_SAMPLE_LOCS_PIXEL_X0Y0_0,
                                                   mmPA_SC_AA_SAMPLE_LOCS_PIXEL_X1Y1_3,
                                                   &paScSampleQuad,
                                                   pCmdSpace);

    regPA_SC_AA_CONFIG paScAaConfig = { };
    paScAaConfig.bits.MAX_SAMPLE_DIST = ComputeMaxSampleDistance(numSamples, samplePattern);

    return pCmdStream->WriteContextRegRmw(mmPA_SC_AA_CONFIG,
                                          static_cast<uint32>(PA_SC_AA_CONFIG__MAX_SAMPLE_DIST_MASK),
                                          paScAaConfig.u32All,
                                          pCmdSpace);
}

} // Gfx6
} // Pal
