/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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

// =====================================================================================================================
// Helper function which computes the maximum sample distance (from pixel center) based on the specified sample
// positions.
uint32 MsaaState::ComputeMaxSampleDistance(
    uint32                       numSamples,
    const MsaaQuadSamplePattern& quadSamplePattern)
{
    uint32 distance = 0;
    const Offset2d*const pSampleLocations = &quadSamplePattern.topLeft[0];

    for(uint32 i = 0; i < numSamples; ++i)
    {
        distance = Max(distance, static_cast<uint32>(Max(abs(pSampleLocations[i].x),
                                                         abs(pSampleLocations[i].y))));
    }

    return distance;
}

// =====================================================================================================================
MsaaState::MsaaState(
    const Device& device)
    :
    Pal::MsaaState(*device.Parent()),
    m_device(device),
    m_log2Samples(0),
    m_log2ShaderExportMaskSamples(0),
    m_sampleMask(0),
    m_log2OcclusionQuerySamples(0)
{
}

// =====================================================================================================================
// Assembles the PM4 headers for the commands in this MSAA state object.
void MsaaState::BuildPm4Headers()
{
    memset(&m_pm4Image, 0, sizeof(m_pm4Image));

    const CmdUtil& cmdUtil = m_device.CmdUtil();

    // 1st PM4 packet
    m_pm4Image.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmDB_EQAA, &m_pm4Image.hdrDbEqaa);

    // 2nd PM4 packet
    m_pm4Image.spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmPA_SC_AA_MASK_X0Y0_X1Y0,
                                                             mmPA_SC_AA_MASK_X0Y1_X1Y1,
                                                             &m_pm4Image.hdrPaScAaMask);
    // 3rd PM4 packet
    m_pm4Image.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmPA_SC_MODE_CNTL_0,
                                                            &m_pm4Image.hdrPaScModeCntl0);
    // 4th PM4 packet
    m_pm4Image.spaceNeeded += cmdUtil.BuildContextRegRmw(
                                mmPA_SC_AA_CONFIG,
                                static_cast<uint32>(~(PA_SC_AA_CONFIG__MAX_SAMPLE_DIST_MASK)),
                                0,
                                &m_pm4Image.paScAaConfigRmw);

    // 5th PM4 packet does a read/modify/write of DB_ALPHA_TO_MASK. The real packet will be created
    // later, we just need to get the size here.
    m_pm4Image.spaceNeeded += CmdUtil::GetContextRegRmwSize();
}

// =====================================================================================================================
size_t MsaaState::Pm4ImgSize(
    const MsaaStateCreateInfo& msaaState)
{
    size_t pm4ImgSize = sizeof(MsaaStatePm4Img);

    return pm4ImgSize;
}

// =====================================================================================================================
// Copies this MSAA state's PM4 commands into the specified command buffer. Returns the next unused DWORD in pCmdSpace.
uint32* MsaaState::WriteCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    // When the command stream is null, we are writing the commands for this state into a pre-allocated buffer that has
    // enough space for the commands.
    // When the command stream is non-null, we are writing the commands as part of a ICmdBuffer::CmdBind* call.
    if (pCmdStream == nullptr)
    {
        memcpy(pCmdSpace, &m_pm4Image, m_pm4Image.spaceNeeded * sizeof(uint32));
        pCmdSpace += m_pm4Image.spaceNeeded;
    }
    else
    {
        pCmdSpace = pCmdStream->WritePm4Image(m_pm4Image.spaceNeeded, &m_pm4Image, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Pre-constructs all the packets required to set the MSAA state
Result MsaaState::Init(
    const MsaaStateCreateInfo& msaaState)
{
    // pre GFX9 HW doesn't support conservative rasterization.
    PAL_ASSERT(msaaState.flags.enableConservativeRasterization == 0);

    m_log2Samples                 = Log2(msaaState.coverageSamples);
    m_sampleMask                  = msaaState.sampleMask;
    m_log2ShaderExportMaskSamples = Log2(msaaState.shaderExportMaskSamples);
    m_log2OcclusionQuerySamples   = Log2(msaaState.occlusionQuerySamples);

    BuildPm4Headers();

    // Use the supplied sample mask to initialize the PA_SC_AA_MASK_** registers:
    uint32 usedMask    = (m_sampleMask & ((1 << NumSamples()) - 1));
    uint32 maskSamples = NumSamples();

    // HW requires us to replicate the sample mask to all 16 bits if there are fewer than 16 samples active.
    while (maskSamples < 16)
    {
        usedMask     |= (usedMask << maskSamples);
        maskSamples <<= 1;
    }

    m_pm4Image.paScAaMask1.u32All = ((usedMask << 16) | usedMask);
    m_pm4Image.paScAaMask2.u32All = ((usedMask << 16) | usedMask);

    // Setup the PA_SC_MODE_CNTL_0 register
    m_pm4Image.paScModeCntl0.u32All = 0;
    m_pm4Image.paScModeCntl0.bits.VPORT_SCISSOR_ENABLE = 1;
    m_pm4Image.paScModeCntl0.bits.MSAA_ENABLE          = ((NumSamples() > 1) ? 1 : 0);

    // Setup the PA_SC_AA_CONFIG and DB_EQAA registers.
    m_pm4Image.dbEqaa.bits.STATIC_ANCHOR_ASSOCIATIONS = 1;
    m_pm4Image.dbEqaa.bits.HIGH_QUALITY_INTERSECTIONS = 1;
    m_pm4Image.dbEqaa.bits.INCOHERENT_EQAA_READS      = 1;
    m_pm4Image.dbEqaa.bits.INTERPOLATE_COMP_Z         = 1;

    const CmdUtil& cmdUtil = m_device.CmdUtil();

    if (msaaState.coverageSamples > 1)
    {
        regPA_SC_AA_CONFIG paScAaConfig        = {};
        paScAaConfig.bits.MSAA_NUM_SAMPLES     = m_log2Samples;
        paScAaConfig.bits.MSAA_EXPOSED_SAMPLES = Log2(msaaState.exposedSamples);

        // MAX_SAMPLE_DIST bits are written at CmdSetMsaaQuadSamplePattern based on the quadSamplePattern
        // via RMW packet.
        paScAaConfig.bits.MAX_SAMPLE_DIST      = 0;
        m_pm4Image.paScAaConfigRmw.regData     = paScAaConfig.u32All;

        m_pm4Image.dbEqaa.bits.MAX_ANCHOR_SAMPLES        = Log2(msaaState.depthStencilSamples);
        m_pm4Image.dbEqaa.bits.PS_ITER_SAMPLES           = Log2(msaaState.pixelShaderSamples);
        m_pm4Image.dbEqaa.bits.MASK_EXPORT_NUM_SAMPLES   = m_log2ShaderExportMaskSamples;
        m_pm4Image.dbEqaa.bits.ALPHA_TO_MASK_NUM_SAMPLES = Log2(msaaState.alphaToCoverageSamples);
        m_pm4Image.dbEqaa.bits.OVERRASTERIZATION_AMOUNT  = m_log2ShaderExportMaskSamples -
                                                           Log2(msaaState.sampleClusters);

        const Gfx6PalSettings& settings = GetGfx6Settings(*m_device.Parent());
        if (m_device.WaDbOverRasterization() && UsesOverRasterization())
        {
            // Apply the "DB Over-Rasterization" workaround:
            // The DB has a bug with early-Z where the DB kills pixels when over-rasterization is enabled. Most of
            // the time, simply forcing post-Z over-rasterization via DB_EQAA is a sufficient workaround. The
            // Gfx6GraphicsPipeline class handles the cases where it is not a sufficient workaround, such as when
            // early-Z is used with depth testing enabled.
            m_pm4Image.dbEqaa.bits.ENABLE_POSTZ_OVERRASTERIZATION = 1;
        }
    }

    // The following code sets up the alpha to mask dithering pattern.
    // If all offsets are set to the same value then there will be no dithering, and the number of gradations of
    // coverage on an edge will be at-most equal to the number of (coverage) samples in the current AA mode. The
    // chosen values set up a different offset for each pixel of a 2x2 quad, allowing many more levels of apparent
    // coverage. The graphics pipeline also writes to DB_ALPHA_TO_MASK so we must use a read/modify/write packet
    // to set these fields.
    regDB_ALPHA_TO_MASK regValue = {};
    if (msaaState.disableAlphaToCoverageDither)
    {
        regValue.bits.ALPHA_TO_MASK_OFFSET0 = 2;
        regValue.bits.ALPHA_TO_MASK_OFFSET1 = 2;
        regValue.bits.ALPHA_TO_MASK_OFFSET2 = 2;
        regValue.bits.ALPHA_TO_MASK_OFFSET3 = 2;
        regValue.bits.OFFSET_ROUND          = 0;
    }
    else
    {
        regValue.bits.ALPHA_TO_MASK_OFFSET0 = 3;
        regValue.bits.ALPHA_TO_MASK_OFFSET1 = 1;
        regValue.bits.ALPHA_TO_MASK_OFFSET2 = 0;
        regValue.bits.ALPHA_TO_MASK_OFFSET3 = 2;
        regValue.bits.OFFSET_ROUND          = 1;
    }
    regDB_ALPHA_TO_MASK regMask = {};
    regMask.u32All = DB_ALPHA_TO_MASK__ALPHA_TO_MASK_OFFSET0_MASK |
                     DB_ALPHA_TO_MASK__ALPHA_TO_MASK_OFFSET1_MASK |
                     DB_ALPHA_TO_MASK__ALPHA_TO_MASK_OFFSET2_MASK |
                     DB_ALPHA_TO_MASK__ALPHA_TO_MASK_OFFSET3_MASK |
                     DB_ALPHA_TO_MASK__OFFSET_ROUND_MASK;

    cmdUtil.BuildContextRegRmw(mmDB_ALPHA_TO_MASK,
                               regMask.u32All,
                               regValue.u32All,
                               &m_pm4Image.dbAlphaToMaskRmw);

    return Result::Success;
}

// =====================================================================================================================
// Constructs all the packets required to set the MSAA sample positions
void MsaaState::BuildSamplePosPm4Image(
    const CmdUtil&               cmdUtil,
    MsaaSamplePositionsPm4Img*   pSamplePosPm4Image,
    uint32                       numSamples,
    const MsaaQuadSamplePattern& quadSamplePattern,
    size_t*                      pCentroidPrioritiesHdrSize,
    size_t*                      pQuadSamplePatternHdrSize)
{
    // Setup the Centroid Priority registers
    size_t hdrSize = 0;

    hdrSize = cmdUtil.BuildSetSeqContextRegs(mmPA_SC_CENTROID_PRIORITY_0,
                                             mmPA_SC_CENTROID_PRIORITY_1,
                                             &pSamplePosPm4Image->hdrPaScCentroidPrio);

    pSamplePosPm4Image->spaceNeeded = hdrSize;

    if (pCentroidPrioritiesHdrSize != nullptr)
    {
        *pCentroidPrioritiesHdrSize = hdrSize;
    }

    SetCentroidPriorities(&pSamplePosPm4Image->paScCentroid, &quadSamplePattern.topLeft[0], numSamples);

    // Setup the sample locations registers
    hdrSize = cmdUtil.BuildSetSeqContextRegs(mmPA_SC_AA_SAMPLE_LOCS_PIXEL_X0Y0_0,
                                             mmPA_SC_AA_SAMPLE_LOCS_PIXEL_X1Y1_3,
                                             &pSamplePosPm4Image->hdrPaScSampleQuad);

    pSamplePosPm4Image->spaceNeeded += hdrSize;

    if (pQuadSamplePatternHdrSize != nullptr)
    {
        *pQuadSamplePatternHdrSize = hdrSize;
    }

    SetQuadSamplePattern(&pSamplePosPm4Image->paScSampleQuad, quadSamplePattern, numSamples);
}

// =====================================================================================================================
// Sets the centroid priority register fields based on the specified sample positions.
void MsaaState::SetCentroidPriorities(
    PaScCentroid*   pPaScCentroid,
    const Offset2d* pSampleLocations,
    uint32          numSamples)
{
    // distance from center of the pixel for each sample location
    uint32 distances[NumSampleLocations];
    // list of sample indices sorted by distance from center
    uint32 centroidPriorities[NumSampleLocations];

    // loop through each sample to calculate pythagorean distance from center
    for (uint32 i = 0; i < NumSampleLocations; i++)
    {
        if (i < numSamples)
        {
            distances[i] = (pSampleLocations[i].x * pSampleLocations[i].x) +
                           (pSampleLocations[i].y * pSampleLocations[i].y);
        }
        else
        {
            distances[i] = 0xFFFFFFFF;
        }
    }

    // Construct the sorted sample order
    for (uint32 i = 0; i < NumSampleLocations; i++)
    {
        if (i < numSamples)
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
        else
        {
            centroidPriorities[i] = 0;
        }
    }

    constexpr size_t NumSamplesPerReg = 8;
    constexpr size_t NumBitsPerSample = 4;
    constexpr size_t DistanceBitMask  = 0xF;

    uint32 reg0SampleIdx = 0;
    uint32 reg1SampleIdx = (NumSamplesPerReg % numSamples);

    for (size_t regSampleIdx = 0; regSampleIdx < NumSamplesPerReg; ++regSampleIdx)
    {
        const size_t shift = (NumBitsPerSample * regSampleIdx);

        pPaScCentroid->priority0.u32All |= ((centroidPriorities[reg0SampleIdx] & DistanceBitMask) << shift);
        pPaScCentroid->priority1.u32All |= ((centroidPriorities[reg1SampleIdx] & DistanceBitMask) << shift);

        reg0SampleIdx = (reg0SampleIdx + 1) % numSamples;
        reg1SampleIdx = (reg1SampleIdx + 1) % numSamples;
    }
}

// =====================================================================================================================
// Sets the sample locations register in the passed sample positions pm4 image.
void MsaaState::SetQuadSamplePattern(
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

} // Gfx6
} // Pal
