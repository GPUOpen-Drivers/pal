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

#include "core/hw/gfxip/gfx12/gfx12CmdUtil.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"
#include "core/hw/gfxip/gfx12/gfx12MsaaState.h"
#include "palInlineFuncs.h"

using namespace Util;

namespace Pal
{
namespace Gfx12
{

// =====================================================================================================================
MsaaState::MsaaState(
    const Device&              device,
    const MsaaStateCreateInfo& createInfo)
    :
    Pal::MsaaState(createInfo),
    m_regs{},
    m_paScModeCntl1{}
{
    Regs::Init(m_regs);

    auto* pPaScModeCntl0 = Regs::Get<mmPA_SC_MODE_CNTL_0, PA_SC_MODE_CNTL_0>(m_regs);

    pPaScModeCntl0->bits.MSAA_ENABLE                   = (createInfo.coverageSamples > 1) |
                                                         createInfo.flags.enable1xMsaaSampleLocations;
    pPaScModeCntl0->bits.VPORT_SCISSOR_ENABLE          = 1;
    pPaScModeCntl0->bits.LINE_STIPPLE_ENABLE           = createInfo.flags.enableLineStipple;
    pPaScModeCntl0->bits.ALTERNATE_RBS_PER_TILE        = 1;
    pPaScModeCntl0->bits.IMPLICIT_VPORT_SCISSOR_ENABLE = 1;

    auto* pDbAlphaToMask = Regs::Get<mmDB_ALPHA_TO_MASK, DB_ALPHA_TO_MASK>(m_regs);
    pDbAlphaToMask->bits.ALPHA_TO_MASK_ENABLE = 1;

    // The following code sets up the alpha to mask dithering pattern.  If all offsets are set to the same value then
    // there will be no dithering, and the number of gradations of coverage on an edge will be at-most equal to the
    // number of (coverage) samples in the current AA mode. The chosen values set up a different offset for each pixel
    // of a 2x2 quad, allowing many more levels of apparent coverage.
    if (createInfo.flags.disableAlphaToCoverageDither)
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

    auto* pDbEqaa = Regs::Get<mmDB_EQAA, DB_EQAA>(m_regs);
    pDbEqaa->bits.STATIC_ANCHOR_ASSOCIATIONS = 1;
    // On gfx12, HIGH_QUALITY_INTERSECTION should be always 1.
    pDbEqaa->bits.HIGH_QUALITY_INTERSECTIONS = 1;

    auto* pPaScAaConfig = Regs::Get<mmPA_SC_AA_CONFIG, PA_SC_AA_CONFIG>(m_regs);

    if (pPaScModeCntl0->bits.MSAA_ENABLE)
    {
        pPaScAaConfig->bits.MSAA_NUM_SAMPLES         = Log2(createInfo.coverageSamples);
        pPaScAaConfig->bits.MSAA_EXPOSED_SAMPLES     = Log2(createInfo.exposedSamples);
        pPaScAaConfig->bits.PS_ITER_SAMPLES          = Log2(createInfo.pixelShaderSamples);

        pDbEqaa->bits.MASK_EXPORT_NUM_SAMPLES    = Log2(createInfo.shaderExportMaskSamples);
        pDbEqaa->bits.ALPHA_TO_MASK_NUM_SAMPLES  = Log2(createInfo.alphaToCoverageSamples);
        pDbEqaa->bits.OVERRASTERIZATION_AMOUNT   = pDbEqaa->bits.MASK_EXPORT_NUM_SAMPLES -
                                                   Log2(createInfo.sampleClusters);
    }

    auto* pPaScConservativeRasterizationCntl =
        Regs::Get<mmPA_SC_CONSERVATIVE_RASTERIZATION_CNTL, PA_SC_CONSERVATIVE_RASTERIZATION_CNTL>(m_regs);

    pPaScConservativeRasterizationCntl->bits.NULL_SQUAD_AA_MASK_ENABLE = 1;

    if (createInfo.flags.enableConservativeRasterization)
    {
        pPaScAaConfig->bits.MSAA_NUM_SAMPLES                               = 0;
        pPaScAaConfig->bits.AA_MASK_CENTROID_DTMN                          = 1;
        pDbEqaa->bits.OVERRASTERIZATION_AMOUNT                             = 4;
        pPaScConservativeRasterizationCntl->bits.NULL_SQUAD_AA_MASK_ENABLE = 0;
        pPaScConservativeRasterizationCntl->bits.PREZ_AA_MASK_ENABLE       = 1;
        pPaScConservativeRasterizationCntl->bits.POSTZ_AA_MASK_ENABLE      = 1;
        pPaScConservativeRasterizationCntl->bits.CENTROID_SAMPLE_OVERRIDE  = 1;

        if (createInfo.conservativeRasterizationMode == ConservativeRasterizationMode::Overestimate)
        {
            pPaScConservativeRasterizationCntl->bits.OVER_RAST_ENABLE              = 1;
            pPaScConservativeRasterizationCntl->bits.OVER_RAST_SAMPLE_SELECT       = 0;
            pPaScConservativeRasterizationCntl->bits.UNDER_RAST_ENABLE             = 0;
            pPaScConservativeRasterizationCntl->bits.UNDER_RAST_SAMPLE_SELECT      = 0;
            pPaScConservativeRasterizationCntl->bits.PBB_UNCERTAINTY_REGION_ENABLE = 1;
        }
        else
        {
            PAL_ASSERT(createInfo.conservativeRasterizationMode == ConservativeRasterizationMode::Underestimate);

            pPaScConservativeRasterizationCntl->bits.OVER_RAST_ENABLE              = 0;
            pPaScConservativeRasterizationCntl->bits.OVER_RAST_SAMPLE_SELECT       = 0;
            pPaScConservativeRasterizationCntl->bits.UNDER_RAST_ENABLE             = 1;
            pPaScConservativeRasterizationCntl->bits.UNDER_RAST_SAMPLE_SELECT      = 0;
            pPaScConservativeRasterizationCntl->bits.PBB_UNCERTAINTY_REGION_ENABLE = 0;
        }
    }

    // HW requires us to replicate the sample mask to all 16 bits if there are fewer than 16 samples active.
    uint32 mask        = createInfo.sampleMask & ((1 << createInfo.coverageSamples) - 1);
    uint32 maskSamples = createInfo.coverageSamples;

    while (maskSamples < 16)
    {
        mask |= (mask << maskSamples);
        maskSamples <<= 1;
    }

    auto* pPaScAaMaskX0y0X1y0 = Regs::Get<mmPA_SC_AA_MASK_X0Y0_X1Y0, PA_SC_AA_MASK_X0Y0_X1Y0>(m_regs);
    auto* pPaScAaMaskX0y1X1y1 = Regs::Get<mmPA_SC_AA_MASK_X0Y1_X1Y1, PA_SC_AA_MASK_X0Y1_X1Y1>(m_regs);

    // Replicate expanded sample mask to all pixels in the quad.
    pPaScAaMaskX0y0X1y0->bits.AA_MASK_X0Y0 = mask;
    pPaScAaMaskX0y0X1y0->bits.AA_MASK_X1Y0 = mask;
    pPaScAaMaskX0y1X1y1->bits.AA_MASK_X0Y1 = mask;
    pPaScAaMaskX0y1X1y1->bits.AA_MASK_X1Y1 = mask;

    m_paScModeCntl1.u32All = PaSuModeCntl1Default;
    // Hardware team recommendation is to set WALK_FENCE_SIZE to 512 pixels for 4/8/16 pipes and 256 pixels
    // for 2 pipes.
    m_paScModeCntl1.bits.WALK_FENCE_SIZE = ((device.GetGbAddrConfig().bits.NUM_PIPES <= 1) ? 2 : 3);

    // Pipeline owns PA_SC_SHADER_CONTROL.bits.PS_ITER_SAMPLE and MsaaState owns PA_SC_MODE_CNTL1.bits.PS_ITER_SAMPLE.
    // Sample rate shading will be enabled if either bit is set.
    m_paScModeCntl1.bits.PS_ITER_SAMPLE  = createInfo.flags.forceSampleRateShading;
}

// =====================================================================================================================
// Given a quad sample pattern, calculates (a) the maximum distance between each sample offset and the pixel center,
// and (b) a sorted list of sample indices in order of ascending distance from the pixel center.  For the sorted list,
// the pattern of the top-left pixel is used.
void MsaaState::SortSamples(
    uint32                       numSamplesPerPixel,
    const MsaaQuadSamplePattern& quadSamplePattern,
    uint32*                      pOutMaxSampleDist,
    uint8                        outSortedIndices[MaxMsaaRasterizerSamples])
{
    // TODO: This could be hoisted to HWL common code

    // NOTE: This distance metric is different from the one used to sort samples.  It is not the pythagorean distance
    // used to sort samples below.  Not sure if this is correct or not (it seems like it would produce values too low
    // for "diagonal" sample offsets), but it is the one used by previous HWLs.
    uint32 maxAbsComp = 0;

    // Calculate the maximum distance between the pixel center and the outermost subpixel sample.
    for (uint32 i = 0; i < numSamplesPerPixel; ++i)
    {
        maxAbsComp = Max(maxAbsComp, static_cast<uint32>(Max(abs(quadSamplePattern.topLeft[i].x),
                                                             abs(quadSamplePattern.topLeft[i].y))));
        maxAbsComp = Max(maxAbsComp, static_cast<uint32>(Max(abs(quadSamplePattern.topRight[i].x),
                                                             abs(quadSamplePattern.topRight[i].y))));
        maxAbsComp = Max(maxAbsComp, static_cast<uint32>(Max(abs(quadSamplePattern.bottomLeft[i].x),
                                                             abs(quadSamplePattern.bottomLeft[i].y))));
        maxAbsComp = Max(maxAbsComp, static_cast<uint32>(Max(abs(quadSamplePattern.bottomRight[i].x),
                                                             abs(quadSamplePattern.bottomRight[i].y))));
    }

    *pOutMaxSampleDist = maxAbsComp;

    // There is only a single pair of registers for centroid priorities.  The sample positions of the top-left
    // pixel in the pattern are used to sort all pixels' samples.
    const SampleLocation* pSampleLocations = &quadSamplePattern.topLeft[0];

    // Distance from center of the pixel for each sample location
    uint32 distances[Pal::MaxMsaaRasterizerSamples];

    // Loop through each sample to calculate pythagorean distance from center
    for (uint32 i = 0; i < numSamplesPerPixel; i++)
    {
        distances[i] = (pSampleLocations[i].x * pSampleLocations[i].x) +
                       (pSampleLocations[i].y * pSampleLocations[i].y);
    }

    // Construct the sorted sample order
    for (uint32 i = 0; i < numSamplesPerPixel; i++)
    {
        // Loop through the distance array and find the smallest remaining distance
        uint32 minIdx = 0;

        for (uint32 j = 1; j < numSamplesPerPixel; j++)
        {
            if (distances[j] < distances[minIdx])
            {
                minIdx = j;
            }
        }

        // Add the sample index to our priority list
        outSortedIndices[i] = minIdx;

        // Then change the distance for that sample to max to mark it as sorted
        distances[minIdx] = 0xFFFFFFFF;
    }
}

// =====================================================================================================================
uint32* MsaaState::WriteCommands(
    uint32* pCmdSpace
    ) const
{
    static_assert(Regs::Size() == Regs::NumContext(), "Unexpected additional registers!");
    return pCmdSpace + CmdUtil::BuildSetContextPairs(m_regs, Regs::Size(), pCmdSpace);
}

} // namespace Gfx12
} // namespace Pal
