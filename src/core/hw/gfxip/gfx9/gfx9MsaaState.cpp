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

#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9MsaaState.h"
#include "g_gfx9Settings.h"
#include "palInlineFuncs.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
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
    Pal::MsaaState(createInfo),
    m_log2Samples(Log2(createInfo.coverageSamples)),
    m_log2OcclusionQuerySamples(Log2(createInfo.occlusionQuerySamples))
{
    m_flags.u32All = 0;
    m_flags.waFixPostZConservativeRasterization = device.Settings().waFixPostZConservativeRasterization;
    m_flags.forceSampleRateShading              = createInfo.flags.forceSampleRateShading;
    m_flags.usesLinesStipple                    = createInfo.flags.enableLineStipple;
}

// =====================================================================================================================
static inline void SetupRegs(
    const Device&                             device,
    const MsaaStateCreateInfo&                msaaState,
    regDB_EQAA*                               pDbEqaa,
    regDB_ALPHA_TO_MASK*                      pDbAlphaToMask,
    uint32*                                   pDbReservedReg2,
    regPA_SC_AA_MASK_X0Y0_X1Y0*               pPaScAaMask1,
    regPA_SC_AA_MASK_X0Y1_X1Y1*               pPaScAaMask2,
    regPA_SC_MODE_CNTL_0*                     pPaScModeCntl0,
    regPA_SC_CONSERVATIVE_RASTERIZATION_CNTL* pPaScConsRastCntl,
    regPA_SC_AA_CONFIG*                       pPaScAaConfig)
{
    const auto& settings = GetGfx9Settings(*device.Parent());

    // Use the supplied sample mask to initialize the PA_SC_AA_MASK_** registers:
    uint32 usedMask    = (msaaState.sampleMask & ((1 << msaaState.coverageSamples) - 1));
    uint32 maskSamples = msaaState.coverageSamples;

    // HW requires us to replicate the sample mask to all 16 bits if there are fewer than 16 samples active.
    while (maskSamples < 16)
    {
        usedMask     |= (usedMask << maskSamples);
        maskSamples <<= 1;
    }

    pPaScAaMask1->u32All = ((usedMask << 16) | usedMask);
    pPaScAaMask2->u32All = ((usedMask << 16) | usedMask);

    // Setup the PA_SC_MODE_CNTL_0 register
    pPaScModeCntl0->u32All                      = 0;
    pPaScModeCntl0->bits.LINE_STIPPLE_ENABLE    = msaaState.flags.enableLineStipple;
    pPaScModeCntl0->bits.VPORT_SCISSOR_ENABLE   = 1;
    pPaScModeCntl0->bits.MSAA_ENABLE            = (((msaaState.coverageSamples > 1) ||
                                                    (msaaState.flags.enable1xMsaaSampleLocations)) ? 1 : 0);
    pPaScModeCntl0->bits.ALTERNATE_RBS_PER_TILE = 1;

    // Setup the PA_SC_AA_CONFIG and DB_EQAA registers.
    pDbEqaa->u32All                          = 0;
    pDbEqaa->bits.STATIC_ANCHOR_ASSOCIATIONS = 1;
    pDbEqaa->bits.HIGH_QUALITY_INTERSECTIONS = 1;
    pDbEqaa->bits.INCOHERENT_EQAA_READS      = 1;

    // INTERPOLATE_COMP_Z should always be set to 0
    pDbEqaa->bits.INTERPOLATE_COMP_Z         = 0;

    pPaScAaConfig->u32All = 0;

    if ((msaaState.coverageSamples > 1) || (msaaState.flags.enable1xMsaaSampleLocations))
    {
        const uint32 log2ShaderExportSamples = Log2(msaaState.shaderExportMaskSamples);

        pPaScAaConfig->bits.MSAA_EXPOSED_SAMPLES = Log2(msaaState.exposedSamples);

        pDbEqaa->bits.MAX_ANCHOR_SAMPLES        = Log2(msaaState.depthStencilSamples);
        pDbEqaa->bits.PS_ITER_SAMPLES           = Log2(msaaState.pixelShaderSamples);
        pDbEqaa->bits.MASK_EXPORT_NUM_SAMPLES   = log2ShaderExportSamples;
        pDbEqaa->bits.ALPHA_TO_MASK_NUM_SAMPLES = Log2(msaaState.alphaToCoverageSamples);
        pDbEqaa->bits.OVERRASTERIZATION_AMOUNT  = log2ShaderExportSamples - Log2(msaaState.sampleClusters);
    }

    // The DB_SHADER_CONTROL register has a "ALPHA_TO_MASK_DISABLE" field that overrides this one.  DB_SHADER_CONTROL
    // is owned by the pipeline.  Always set this bit here and use the DB_SHADER_CONTROL to control the enabling.
    pDbAlphaToMask->u32All                    = 0;
    pDbAlphaToMask->bits.ALPHA_TO_MASK_ENABLE = 1;

    // The following code sets up the alpha to mask dithering pattern.
    // If all offsets are set to the same value then there will be no dithering, and the number of gradations of
    // coverage on an edge will be at-most equal to the number of (coverage) samples in the current AA mode. The
    // chosen values set up a different offset for each pixel of a 2x2 quad, allowing many more levels of apparent
    // coverage.
    if (msaaState.flags.disableAlphaToCoverageDither)
    {
        pDbAlphaToMask->bits.ALPHA_TO_MASK_OFFSET0 = 2;
        pDbAlphaToMask->bits.ALPHA_TO_MASK_OFFSET1 = 2;
        pDbAlphaToMask->bits.ALPHA_TO_MASK_OFFSET2 = 2;
        pDbAlphaToMask->bits.ALPHA_TO_MASK_OFFSET3 = 2;
        pDbAlphaToMask->bits.OFFSET_ROUND          = 0;
    }
    else
    {
        pDbAlphaToMask->bits.ALPHA_TO_MASK_OFFSET0 = 3;
        pDbAlphaToMask->bits.ALPHA_TO_MASK_OFFSET1 = 1;
        pDbAlphaToMask->bits.ALPHA_TO_MASK_OFFSET2 = 0;
        pDbAlphaToMask->bits.ALPHA_TO_MASK_OFFSET3 = 2;
        pDbAlphaToMask->bits.OFFSET_ROUND          = 1;
    }

    pPaScConsRastCntl->u32All = 0;

    if (msaaState.flags.enableConservativeRasterization)
    {
        pPaScAaConfig->bits.AA_MASK_CENTROID_DTMN = 1;

        pPaScConsRastCntl->bits.NULL_SQUAD_AA_MASK_ENABLE     = 0;
        pPaScConsRastCntl->bits.PREZ_AA_MASK_ENABLE           = 1;
        pPaScConsRastCntl->bits.POSTZ_AA_MASK_ENABLE          = 1;
        pPaScConsRastCntl->bits.CENTROID_SAMPLE_OVERRIDE      = 1;

        pDbEqaa->bits.ENABLE_POSTZ_OVERRASTERIZATION = 0;
        pDbEqaa->bits.OVERRASTERIZATION_AMOUNT       = 4;

        switch (msaaState.conservativeRasterizationMode)
        {
        case ConservativeRasterizationMode::Overestimate:
            pPaScConsRastCntl->bits.OVER_RAST_ENABLE              = 1;
            pPaScConsRastCntl->bits.OVER_RAST_SAMPLE_SELECT       = 0;
            pPaScConsRastCntl->bits.UNDER_RAST_ENABLE             = 0;
            pPaScConsRastCntl->bits.UNDER_RAST_SAMPLE_SELECT      = 1;
            pPaScConsRastCntl->bits.PBB_UNCERTAINTY_REGION_ENABLE = 1;
            pPaScConsRastCntl->bits.COVERAGE_AA_MASK_ENABLE       = (settings.disableCoverageAaMask ? 0 : 1);
            break;

        case ConservativeRasterizationMode::Underestimate:
            pPaScConsRastCntl->bits.OVER_RAST_ENABLE              = 0;
            pPaScConsRastCntl->bits.OVER_RAST_SAMPLE_SELECT       = 1;
            pPaScConsRastCntl->bits.UNDER_RAST_ENABLE             = 1;
            pPaScConsRastCntl->bits.UNDER_RAST_SAMPLE_SELECT      = 0;
            pPaScConsRastCntl->bits.PBB_UNCERTAINTY_REGION_ENABLE = 0;
            pPaScConsRastCntl->bits.COVERAGE_AA_MASK_ENABLE       = 0;
            break;

        case ConservativeRasterizationMode::Count:
            PAL_ASSERT_ALWAYS();
            break;
        }
    }
    else
    {
        pPaScConsRastCntl->bits.OVER_RAST_ENABLE              = 0;
        pPaScConsRastCntl->bits.UNDER_RAST_ENABLE             = 0;
        pPaScConsRastCntl->bits.PBB_UNCERTAINTY_REGION_ENABLE = 0;
        pPaScConsRastCntl->bits.NULL_SQUAD_AA_MASK_ENABLE     = 1;
        pPaScConsRastCntl->bits.PREZ_AA_MASK_ENABLE           = 0;
        pPaScConsRastCntl->bits.POSTZ_AA_MASK_ENABLE          = 0;
        pPaScConsRastCntl->bits.CENTROID_SAMPLE_OVERRIDE      = 0;
    }

    if (settings.waFixPostZConservativeRasterization &&
        (TestAllFlagsSet(pPaScAaMask1->u32All, ((1 << msaaState.exposedSamples) - 1)) == false))
    {
        //    We have an issue in Navi10 related to Late - Z Conservative rasterization when the mask is partially lit.
        //
        //    The logic that determines whether the mask is partially lit needs to be fed into an existing piece of
        //    logic.  Unfortunately, when we do this as an ECO, it creates a giant logic cone and breaks timing.
        //
        //    A compromise solution is to define a context register that lets hardware know that the mask is partially
        //    lit.  The SWA would require that when PA_SC_AA_MASK_AA_MASK is partially lit with the number of
        //    samples defined by PA_SC_AA_CONFIG_MSAA_EXPOSED_SAMPLES, software would need to write the corresponding
        //    "PARTIALLY LIT" bit for that context.

        // NOTE: A check to confirm equivalence b/w DB_RESERVED_REG_2__FIELD_1_MASK offset for Gfx101 and Gfx103 is
        //       already performed in WriteCommands() above.
        *pDbReservedReg2 = DB_RESERVED_REG_2__FIELD_1_MASK & 0x1;
    }
    else
    {
        *pDbReservedReg2 = 0;
    }

    // Make sure we don't write outside of the state this class owns.
    PAL_ASSERT((pPaScAaConfig->u32All & (~MsaaState::PcScAaConfigMask)) == 0);
}

// =====================================================================================================================
// Sets the centroid priority register fields based on the specified sample positions.
static void SetCentroidPriorities(
    PaScCentroid*         pPaScCentroid,
    const SampleLocation* pSampleLocations,
    uint32                numSamples)
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
        for (uint32 j = 1; j < numSamples; j++)
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
    const SampleLocation* pSampleLocations = nullptr;

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
uint32 MsaaState::ComputeMaxSampleDistance(
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

    return pCmdSpace;
}

// =====================================================================================================================
Gfx11MsaaStateRs64::Gfx11MsaaStateRs64(
    const Device&              device,
    const MsaaStateCreateInfo& msaaState)
    :
    Gfx9::MsaaState(device, msaaState)
{
    uint32 dbReservedReg2; //< Not needed for GFX11

    Regs::Init(m_regs);

    SetupRegs(device,
              msaaState,
              Regs::Get<mmDB_EQAA, DB_EQAA>(m_regs),
              Regs::Get<mmDB_ALPHA_TO_MASK, DB_ALPHA_TO_MASK>(m_regs),
              &dbReservedReg2,
              Regs::Get<mmPA_SC_AA_MASK_X0Y0_X1Y0, PA_SC_AA_MASK_X0Y0_X1Y0>(m_regs),
              Regs::Get<mmPA_SC_AA_MASK_X0Y1_X1Y1, PA_SC_AA_MASK_X0Y1_X1Y1>(m_regs),
              Regs::Get<mmPA_SC_MODE_CNTL_0, PA_SC_MODE_CNTL_0>(m_regs),
              &m_paScConsRastCntl,
              &m_paScAaConfig);

    Regs::Finalize(m_regs);
}

// =====================================================================================================================
// Writes the PM4 commands required to bind the state object to the specified bind point. Returns the next unused DWORD
// in pCmdSpace.
uint32* Gfx11MsaaStateRs64::WriteCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    PAL_ASSERT(m_flags.waFixPostZConservativeRasterization == 0); //< Navi10 only SWA

    return pCmdStream->WriteSetConstContextRegPairs(m_regs, Regs::NumRegsWritten(), pCmdSpace);
}

// =====================================================================================================================
Gfx11MsaaStateF32::Gfx11MsaaStateF32(
    const Device&              device,
    const MsaaStateCreateInfo& msaaState)
    :
    Gfx9::MsaaState(device, msaaState)
{
    uint32 dbReservedReg2; //< Not needed for GFX11

    Regs::Init(m_regs);

    SetupRegs(device,
              msaaState,
              Regs::Get<mmDB_EQAA, DB_EQAA>(m_regs),
              Regs::Get<mmDB_ALPHA_TO_MASK, DB_ALPHA_TO_MASK>(m_regs),
              &dbReservedReg2,
              Regs::Get<mmPA_SC_AA_MASK_X0Y0_X1Y0, PA_SC_AA_MASK_X0Y0_X1Y0>(m_regs),
              Regs::Get<mmPA_SC_AA_MASK_X0Y1_X1Y1, PA_SC_AA_MASK_X0Y1_X1Y1>(m_regs),
              Regs::Get<mmPA_SC_MODE_CNTL_0, PA_SC_MODE_CNTL_0>(m_regs),
              &m_paScConsRastCntl,
              &m_paScAaConfig);
}

// =====================================================================================================================
// Writes the PM4 commands required to bind the state object to the specified bind point. Returns the next unused DWORD
// in pCmdSpace.
uint32* Gfx11MsaaStateF32::WriteCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    PAL_ASSERT(m_flags.waFixPostZConservativeRasterization == 0); //< Navi10 only SWA

    return pCmdStream->WriteSetContextRegPairs(m_regs, Regs::Size(), pCmdSpace);
}

// =====================================================================================================================
Gfx10MsaaState::Gfx10MsaaState(
    const Device&              device,
    const MsaaStateCreateInfo& msaaState)
    :
    Gfx9::MsaaState(device, msaaState)
{
    SetupRegs(device,
              msaaState,
              &(m_regs.dbEqaa),
              &(m_regs.dbAlphaToMask),
              &m_dbReservedReg2,
              &(m_regs.paScAaMask1),
              &(m_regs.paScAaMask2),
              &(m_regs.paScModeCntl0),
              &m_paScConsRastCntl,
              &m_paScAaConfig);
}

// =====================================================================================================================
// Writes the PM4 commands required to bind the state object to the specified bind point. Returns the next unused DWORD
// in pCmdSpace.
uint32* Gfx10MsaaState::WriteCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_EQAA, m_regs.dbEqaa.u32All, pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmPA_SC_AA_MASK_X0Y0_X1Y0,
                                                   mmPA_SC_AA_MASK_X0Y1_X1Y1,
                                                   &m_regs.paScAaMask1,
                                                   pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmPA_SC_MODE_CNTL_0, m_regs.paScModeCntl0.u32All, pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_ALPHA_TO_MASK, m_regs.dbAlphaToMask.u32All, pCmdSpace);

    if (m_flags.waFixPostZConservativeRasterization != 0)
    {
        uint32 dbReservedReg2Mask = static_cast<uint32>(~DB_RESERVED_REG_2__FIELD_1_MASK);

        pCmdSpace = pCmdStream->WriteContextRegRmw(mmDB_RESERVED_REG_2,
                                                   dbReservedReg2Mask,
                                                   m_dbReservedReg2,
                                                   pCmdSpace);
    }

    return pCmdSpace;
}

} // Gfx9
} // Pal
