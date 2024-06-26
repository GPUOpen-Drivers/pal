/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/device.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9PerfCtrInfo.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// These enums are defined by the SPM spec. They map block names to RLC-specific SPM block select codes.
enum Gfx10SpmGlobalBlockSelect : uint32
{
    Gfx10SpmGlobalBlockSelectCpg        = 0,
    Gfx10SpmGlobalBlockSelectCpc        = 1,
    Gfx10SpmGlobalBlockSelectCpf        = 2,
    Gfx10SpmGlobalBlockSelectGds        = 3,
    Gfx10SpmGlobalBlockSelectGcr        = 4,
    Gfx10SpmGlobalBlockSelectPh         = 5,
    Gfx10SpmGlobalBlockSelectGe         = 6,
    Gfx10SpmGlobalBlockSelectGl2a       = 7,
    Gfx10SpmGlobalBlockSelectGl2c       = 8,
    Gfx10SpmGlobalBlockSelectSdma       = 9,
    Gfx10SpmGlobalBlockSelectGus        = 10,
    Gfx10SpmGlobalBlockSelectEa         = 11,
    Gfx10SpmGlobalBlockSelectCha        = 12,
    Gfx10SpmGlobalBlockSelectChc        = 13,
    Gfx10SpmGlobalBlockSelectChcg       = 14,
    Gfx10SpmGlobalBlockSelectGpuvmAtcl2 = 15,
    Gfx10SpmGlobalBlockSelectGpuvmVml2  = 16,
    Gfx10SpmGlobalBlockSelectGe1        = 6,  // The old GE value was reused for GE1.
    Gfx10SpmGlobalBlockSelectGe2Se      = 17, // These counters are per-SE but still in the global SPM section.
    Gfx10SpmGlobalBlockSelectGe2Dist    = 18,
    Gfx11SpmGlobalBlockSelectRspm       = 31,
};

enum Gfx10SpmSeBlockSelect : uint32
{
    Gfx10SpmSeBlockSelectCb    = 0,
    Gfx10SpmSeBlockSelectDb    = 1,
    Gfx10SpmSeBlockSelectPa    = 2,
    Gfx10SpmSeBlockSelectSx    = 3,
    Gfx10SpmSeBlockSelectSc    = 4,
    Gfx10SpmSeBlockSelectTa    = 5,
    Gfx10SpmSeBlockSelectTd    = 6,
    Gfx10SpmSeBlockSelectTcp   = 7,
    Gfx10SpmSeBlockSelectSpi   = 8,
    Gfx10SpmSeBlockSelectSqg   = 9,
    Gfx10SpmSeBlockSelectGl1a  = 10,
    Gfx10SpmSeBlockSelectRmi   = 11,
    Gfx10SpmSeBlockSelectGl1c  = 12,
    Gfx10SpmSeBlockSelectGl1cg = 13,
    Gfx11SpmSeBlockSelectCbr   = 14,
    Gfx11SpmSeBlockSelectDbr   = 15,
    Gfx11SpmSeBlockSelectGl1h  = 16,
    Gfx11SpmSeBlockSelectSqc   = 17,
    Gfx11SpmSeBlockSelectPc    = 18,
    Gfx11SpmSeBlockSelectSeRpm = 31,
};

// There's a terrifyingly large number of UMCCH registers. This macro makes UpdateUmcchBlockInfo much more sane.
#define SET_UMCCH_INSTANCE_REGS(Ns, Idx) \
    pInfo->umcchRegAddr[Idx].perfMonCtlClk = Ns::mmUMCCH##Idx##_PerfMonCtlClk; \
    pInfo->umcchRegAddr[Idx].perModule[0] = { Ns::mmUMCCH##Idx##_PerfMonCtl1, Ns::mmUMCCH##Idx##_PerfMonCtr1_Lo, Ns::mmUMCCH##Idx##_PerfMonCtr1_Hi }; \
    pInfo->umcchRegAddr[Idx].perModule[1] = { Ns::mmUMCCH##Idx##_PerfMonCtl2, Ns::mmUMCCH##Idx##_PerfMonCtr2_Lo, Ns::mmUMCCH##Idx##_PerfMonCtr2_Hi }; \
    pInfo->umcchRegAddr[Idx].perModule[2] = { Ns::mmUMCCH##Idx##_PerfMonCtl3, Ns::mmUMCCH##Idx##_PerfMonCtr3_Lo, Ns::mmUMCCH##Idx##_PerfMonCtr3_Hi }; \
    pInfo->umcchRegAddr[Idx].perModule[3] = { Ns::mmUMCCH##Idx##_PerfMonCtl4, Ns::mmUMCCH##Idx##_PerfMonCtr4_Lo, Ns::mmUMCCH##Idx##_PerfMonCtr4_Hi }; \
    pInfo->umcchRegAddr[Idx].perModule[4] = { Ns::mmUMCCH##Idx##_PerfMonCtl5, Ns::mmUMCCH##Idx##_PerfMonCtr5_Lo, Ns::mmUMCCH##Idx##_PerfMonCtr5_Hi };

// There are a large number of mmGE_PERFCOUNT registers. These two macros allow confirming equivalence without an
// overwhelmingly large block of code
#define STATIC_ASSERT_GE_PERFCOUNT_REGS(Ns, Idx) \
    static_assert(((Gfx101::mmGE_PERFCOUNTER##Idx##_SELECT == Ns::mmGE_PERFCOUNTER##Idx##_SELECT) \
                   && (Gfx101::mmGE_PERFCOUNTER##Idx##_LO == Ns::mmGE_PERFCOUNTER##Idx##_LO) \
                   && (Gfx101::mmGE_PERFCOUNTER##Idx##_HI == Ns::mmGE_PERFCOUNTER##Idx##_HI)), \
                   "mmGE_PERFCOUNTER registers do not match!");

#define STATIC_ASSERT_GE_PERFCOUNT_SELECT1_REGS(Ns, Idx) \
    static_assert(Gfx101::mmGE_PERFCOUNTER##Idx##_SELECT1 == Ns::mmGE_PERFCOUNTER##Idx##_SELECT1, \
                    "mmGE_PERFCOUNTER SELECT1 registers do not match!");

// =====================================================================================================================
// A helper function which updates the gfx10 UMC perf counter registers, numScopedInstances, and maxEventId.
static void Gfx10UpdateUmcchBlockInfo(
    const Pal::Device&    device,
    PerfCounterInfo*      pInfo,
    PerfCounterBlockInfo* pBlockInfo)
{
    if (IsGfx101(device))
    {
        // Due to missing headers for some asics, we'll be selective here populating the max counts.
        pBlockInfo->maxEventId = Nv10MaxPerfEventIds[UmcPerfcountSelectId];

        // This is misleading for Navi14 becuase it's only supposed to have 8 SDPs, but in practice as long as
        // the first 8 all have the same register offsets the code at the end will clamp us to 8 instances.
        SET_UMCCH_INSTANCE_REGS(Gfx101, 0);
        SET_UMCCH_INSTANCE_REGS(Gfx101, 1);
        SET_UMCCH_INSTANCE_REGS(Gfx101, 2);
        SET_UMCCH_INSTANCE_REGS(Gfx101, 3);
        SET_UMCCH_INSTANCE_REGS(Gfx101, 4);
        SET_UMCCH_INSTANCE_REGS(Gfx101, 5);
        SET_UMCCH_INSTANCE_REGS(Gfx101, 6);
        SET_UMCCH_INSTANCE_REGS(Gfx101, 7);
        SET_UMCCH_INSTANCE_REGS(Gfx101, 8);
        SET_UMCCH_INSTANCE_REGS(Gfx101, 9);
        SET_UMCCH_INSTANCE_REGS(Gfx101, 10);
        SET_UMCCH_INSTANCE_REGS(Gfx101, 11);
        SET_UMCCH_INSTANCE_REGS(Gfx101, 12);
        SET_UMCCH_INSTANCE_REGS(Gfx101, 13);
        SET_UMCCH_INSTANCE_REGS(Gfx101, 14);
        SET_UMCCH_INSTANCE_REGS(Gfx101, 15);
    }
    else if (IsGfx103(device))
    {
        // Due to missing headers for some asics, we'll be selective here populating the max counts.
        pBlockInfo->maxEventId = Nv21MaxPerfEventIds[UmcPerfcountSelectId];

        if (IsNavi2x(device))
        {
            // These four are always the same.
            SET_UMCCH_INSTANCE_REGS(Gfx103, 0);
            SET_UMCCH_INSTANCE_REGS(Nv2x,   1);
            SET_UMCCH_INSTANCE_REGS(Gfx103, 2);
            SET_UMCCH_INSTANCE_REGS(Nv2x,   3);

            if (IsNavi21(device))
            {
                SET_UMCCH_INSTANCE_REGS(Nv21, 4);
                SET_UMCCH_INSTANCE_REGS(Nv21, 5);
                SET_UMCCH_INSTANCE_REGS(Nv21, 6);
                SET_UMCCH_INSTANCE_REGS(Nv21, 7);
                SET_UMCCH_INSTANCE_REGS(Nv21, 8);
                SET_UMCCH_INSTANCE_REGS(Nv21, 9);
                SET_UMCCH_INSTANCE_REGS(Nv21, 10);
                SET_UMCCH_INSTANCE_REGS(Nv21, 11);
                SET_UMCCH_INSTANCE_REGS(Nv21, 12);
                SET_UMCCH_INSTANCE_REGS(Nv21, 13);
                SET_UMCCH_INSTANCE_REGS(Nv21, 14);
                SET_UMCCH_INSTANCE_REGS(Nv21, 15);
            }
            else if (IsNavi22(device))
            {
                SET_UMCCH_INSTANCE_REGS(Nv22, 4);
                SET_UMCCH_INSTANCE_REGS(Nv22, 5);
                SET_UMCCH_INSTANCE_REGS(Nv22, 6);
                SET_UMCCH_INSTANCE_REGS(Nv22, 7);
                SET_UMCCH_INSTANCE_REGS(Nv22, 8);
                SET_UMCCH_INSTANCE_REGS(Nv22, 9);
                SET_UMCCH_INSTANCE_REGS(Nv22, 10);
                SET_UMCCH_INSTANCE_REGS(Nv22, 11);
            }
            else if (IsNavi23(device))
            {
                SET_UMCCH_INSTANCE_REGS(Nv23, 4);
                SET_UMCCH_INSTANCE_REGS(Nv23, 5);
                SET_UMCCH_INSTANCE_REGS(Nv23, 6);
                SET_UMCCH_INSTANCE_REGS(Nv23, 7);
            }
            else
            {
                // Navi24 only has the four common instances.
                PAL_ASSERT(IsNavi24(device));
            }
        }
        else if (device.ChipProperties().gpuType == GpuType::Integrated)
        {
            SET_UMCCH_INSTANCE_REGS(Gfx103, 0);
            SET_UMCCH_INSTANCE_REGS(Apu103, 1);
            SET_UMCCH_INSTANCE_REGS(Gfx103, 2);
            SET_UMCCH_INSTANCE_REGS(Apu103, 3);
            SET_UMCCH_INSTANCE_REGS(Apu103, 4);
            SET_UMCCH_INSTANCE_REGS(Apu103, 5);
            SET_UMCCH_INSTANCE_REGS(Apu103, 6);
            SET_UMCCH_INSTANCE_REGS(Apu103, 7);
            SET_UMCCH_INSTANCE_REGS(Apu103, 8);
            SET_UMCCH_INSTANCE_REGS(Apu103, 9);
            SET_UMCCH_INSTANCE_REGS(Apu103, 10);
            SET_UMCCH_INSTANCE_REGS(Apu103, 11);
            SET_UMCCH_INSTANCE_REGS(Apu103, 12);
            SET_UMCCH_INSTANCE_REGS(Apu103, 13);
            SET_UMCCH_INSTANCE_REGS(Apu103, 14);
            SET_UMCCH_INSTANCE_REGS(Apu103, 15);
        }
        else
        {
            PAL_NOT_IMPLEMENTED();
        }
    }
    else
    {
        PAL_NOT_IMPLEMENTED();
    }

    // We should have one UMC channel per SDP interface. We also should have a full set of registers for each of those
    // channels. However, we might not be able to read or write some of them due to a limitation in the CP's COPY_DATA
    // packet. We shouldn't expose any instances that could hit that limitation.
    pBlockInfo->numScopedInstances = 0;

    for (uint32 instance = 0; instance < device.ChipProperties().gfx9.numSdpInterfaces; ++instance)
    {
        bool regsAreValid = CmdUtil::CanUseCopyDataRegOffset(pInfo->umcchRegAddr[instance].perfMonCtlClk);

        for (uint32 idx = 0; regsAreValid && (idx < Gfx9MaxUmcchPerfModules); ++idx)
        {
            regsAreValid =
                (CmdUtil::CanUseCopyDataRegOffset(pInfo->umcchRegAddr[instance].perModule[idx].perfMonCtl)   &&
                 CmdUtil::CanUseCopyDataRegOffset(pInfo->umcchRegAddr[instance].perModule[idx].perfMonCtrLo) &&
                 CmdUtil::CanUseCopyDataRegOffset(pInfo->umcchRegAddr[instance].perModule[idx].perfMonCtrHi));
        }

        if (regsAreValid == false)
        {
            // Drop out now, don't expose this instance or any after it.
            break;
        }
        else
        {
            // This instance is good, check the next one.
            pBlockInfo->numScopedInstances++;
        }
    }
}

typedef unsigned int MaxEventIds[MaxPerfCtrId];
constexpr MaxEventIds UnknownMaxEventIds = {};

// =====================================================================================================================
// Get an array with the maximum values of each perfcounter for this device
static const MaxEventIds& GetEventLimits(
    const Pal::Device& device)
{
    const MaxEventIds* pOut = nullptr;
    switch(device.ChipProperties().revision)
    {
    case Pal::AsicRevision::Navi10:
    case Pal::AsicRevision::Navi12:
    case Pal::AsicRevision::Navi14:
        pOut = &Nv10MaxPerfEventIds;
        break;
    case Pal::AsicRevision::Navi21:
        pOut = &Nv21MaxPerfEventIds;
        break;
    case Pal::AsicRevision::Navi22:
        pOut = &Nv22MaxPerfEventIds;
        break;
    case Pal::AsicRevision::Navi23:
        pOut = &Nv23MaxPerfEventIds;
        break;
    case Pal::AsicRevision::Navi24:
        pOut = &Nv24MaxPerfEventIds;
        break;
    case Pal::AsicRevision::Rembrandt:
        pOut = &RembrandtMaxPerfEventIds;
        break;
    case Pal::AsicRevision::Raphael:
        pOut = &RaphaelMaxPerfEventIds;
        break;
    case Pal::AsicRevision::Navi31:
        pOut = &Nv31MaxPerfEventIds;
        break;
    case Pal::AsicRevision::Navi32:
        pOut = &Nv32MaxPerfEventIds;
        break;
    case Pal::AsicRevision::Navi33:
        pOut = &Nv33MaxPerfEventIds;
        break;
    case Pal::AsicRevision::Phoenix1:
        pOut = &Phx1MaxPerfEventIds;
        break;
    case Pal::AsicRevision::Phoenix2:
        pOut = &Phx2MaxPerfEventIds;
        break;
    default:
        PAL_ASSERT_ALWAYS(); // What chip is this?
        pOut = &UnknownMaxEventIds;
    }
    return *pOut;
}

// =====================================================================================================================
// A helper function which updates the gfx11 RPB's block info with device-specific data.
static void Gfx11UpdateRpbBlockInfo(
    const Pal::Device&    device,
    PerfCounterBlockInfo* pInfo)
{
    if (IsNavi3x(device))
    {
        pInfo->regAddr = { Nv3x::mmRPB_PERFCOUNTER_RSLT_CNTL, {
            { Nv3x::mmRPB_PERFCOUNTER0_CFG, 0, Nv3x::mmRPB_PERFCOUNTER_LO, Nv3x::mmRPB_PERFCOUNTER_HI },
            { Nv3x::mmRPB_PERFCOUNTER1_CFG, 0, Nv3x::mmRPB_PERFCOUNTER_LO, Nv3x::mmRPB_PERFCOUNTER_HI },
            { Nv3x::mmRPB_PERFCOUNTER2_CFG, 0, Nv3x::mmRPB_PERFCOUNTER_LO, Nv3x::mmRPB_PERFCOUNTER_HI },
            { Nv3x::mmRPB_PERFCOUNTER3_CFG, 0, Nv3x::mmRPB_PERFCOUNTER_LO, Nv3x::mmRPB_PERFCOUNTER_HI },
        }};
    }
    else if (IsPhoenix1(device)
       )
    {
        pInfo->regAddr = { Phx1::mmRPB_PERFCOUNTER_RSLT_CNTL, {
            { Phx1::mmRPB_PERFCOUNTER0_CFG, 0, Phx1::mmRPB_PERFCOUNTER_LO, Phx1::mmRPB_PERFCOUNTER_HI },
            { Phx1::mmRPB_PERFCOUNTER1_CFG, 0, Phx1::mmRPB_PERFCOUNTER_LO, Phx1::mmRPB_PERFCOUNTER_HI },
            { Phx1::mmRPB_PERFCOUNTER2_CFG, 0, Phx1::mmRPB_PERFCOUNTER_LO, Phx1::mmRPB_PERFCOUNTER_HI },
            { Phx1::mmRPB_PERFCOUNTER3_CFG, 0, Phx1::mmRPB_PERFCOUNTER_LO, Phx1::mmRPB_PERFCOUNTER_HI },
        }};
    }
    else
    if (IsPhoenix2(device)
       )
    {
        pInfo->regAddr = { Phx2::mmRPB_PERFCOUNTER_RSLT_CNTL, {
            { Phx2::mmRPB_PERFCOUNTER0_CFG, 0, Phx2::mmRPB_PERFCOUNTER_LO, Phx2::mmRPB_PERFCOUNTER_HI },
            { Phx2::mmRPB_PERFCOUNTER1_CFG, 0, Phx2::mmRPB_PERFCOUNTER_LO, Phx2::mmRPB_PERFCOUNTER_HI },
            { Phx2::mmRPB_PERFCOUNTER2_CFG, 0, Phx2::mmRPB_PERFCOUNTER_LO, Phx2::mmRPB_PERFCOUNTER_HI },
            { Phx2::mmRPB_PERFCOUNTER3_CFG, 0, Phx2::mmRPB_PERFCOUNTER_LO, Phx2::mmRPB_PERFCOUNTER_HI },
        }};
    }
    else
    {
        PAL_NOT_IMPLEMENTED();
    }
}

// =====================================================================================================================
// A helper function which updates the GDS's block info with device-specific data.
static void Gfx10PlusUpdateGdsBlockInfo(
    const Pal::Device&    device,
    PerfCounterBlockInfo* pInfo)
{
    if (IsGfx103Plus(device))
    {
        using namespace Gfx103PlusExclusive;

        // All of the legacy GDS counters were updated to full perfmon modules in gfx10.2.
        pInfo->numGenericSpmModules    = 4; // GDS_PERFCOUNTER0-3
        pInfo->numGenericLegacyModules = 0;
        pInfo->numSpmWires             = 8;

        pInfo->regAddr = { 0, {
            { mmGDS_PERFCOUNTER0_SELECT, mmGDS_PERFCOUNTER0_SELECT1, mmGDS_PERFCOUNTER0_LO, mmGDS_PERFCOUNTER0_HI },
            { mmGDS_PERFCOUNTER1_SELECT, mmGDS_PERFCOUNTER1_SELECT1, mmGDS_PERFCOUNTER1_LO, mmGDS_PERFCOUNTER1_HI },
            { mmGDS_PERFCOUNTER2_SELECT, mmGDS_PERFCOUNTER2_SELECT1, mmGDS_PERFCOUNTER2_LO, mmGDS_PERFCOUNTER2_HI },
            { mmGDS_PERFCOUNTER3_SELECT, mmGDS_PERFCOUNTER3_SELECT1, mmGDS_PERFCOUNTER3_LO, mmGDS_PERFCOUNTER3_HI },
        }};
    }
    else
    {
        pInfo->numGenericSpmModules    = 1; // GDS_PERFCOUNTER0
        pInfo->numGenericLegacyModules = 3; // GDS_PERFCOUNTER1-3
        pInfo->numSpmWires             = 2;

        pInfo->regAddr = { 0, {
            { mmGDS_PERFCOUNTER0_SELECT, mmGDS_PERFCOUNTER0_SELECT1, mmGDS_PERFCOUNTER0_LO, mmGDS_PERFCOUNTER0_HI },
            { mmGDS_PERFCOUNTER1_SELECT, 0,                          mmGDS_PERFCOUNTER1_LO, mmGDS_PERFCOUNTER1_HI },
            { mmGDS_PERFCOUNTER2_SELECT, 0,                          mmGDS_PERFCOUNTER2_LO, mmGDS_PERFCOUNTER2_HI },
            { mmGDS_PERFCOUNTER3_SELECT, 0,                          mmGDS_PERFCOUNTER3_LO, mmGDS_PERFCOUNTER3_HI },
        }};
    }
}

// =====================================================================================================================
// A helper function which updates SDMA's block info with device-specific data.
static void Gfx10UpdateSdmaBlockInfo(
    const Pal::Device& device,
    PerfCounterInfo*   pInfo)
{
    if (IsNavi2x(device))
    {
        using namespace Gfx103PlusExclusive;

        pInfo->sdmaRegAddr[0][0] = { mmSDMA0_PERFCOUNTER0_SELECT,       mmSDMA0_PERFCOUNTER0_SELECT1,
                                     mmSDMA0_PERFCOUNTER0_LO,           mmSDMA0_PERFCOUNTER0_HI };
        pInfo->sdmaRegAddr[0][1] = { mmSDMA0_PERFCOUNTER1_SELECT,       mmSDMA0_PERFCOUNTER1_SELECT1,
                                     mmSDMA0_PERFCOUNTER1_LO,           mmSDMA0_PERFCOUNTER1_HI };
        pInfo->sdmaRegAddr[1][0] = { Nv21::mmSDMA1_PERFCOUNTER0_SELECT, Nv21::mmSDMA1_PERFCOUNTER0_SELECT1,
                                     Nv21::mmSDMA1_PERFCOUNTER0_LO,     Nv21::mmSDMA1_PERFCOUNTER0_HI };
        pInfo->sdmaRegAddr[1][1] = { Nv21::mmSDMA1_PERFCOUNTER1_SELECT, Nv21::mmSDMA1_PERFCOUNTER1_SELECT1,
                                     Nv21::mmSDMA1_PERFCOUNTER1_LO,     Nv21::mmSDMA1_PERFCOUNTER1_HI };
    }
    else if (IsGfx103(device))
    {
        using namespace Gfx103PlusExclusive;

        pInfo->sdmaRegAddr[0][0] = { mmSDMA0_PERFCOUNTER0_SELECT, mmSDMA0_PERFCOUNTER0_SELECT1,
                                     mmSDMA0_PERFCOUNTER0_LO,     mmSDMA0_PERFCOUNTER0_HI };
        pInfo->sdmaRegAddr[0][1] = { mmSDMA0_PERFCOUNTER1_SELECT, mmSDMA0_PERFCOUNTER1_SELECT1,
                                     mmSDMA0_PERFCOUNTER1_LO,     mmSDMA0_PERFCOUNTER1_HI };
    }
    else
    {
        pInfo->sdmaRegAddr[0][0] = { Oss50::mmSDMA0_PERFCOUNTER0_SELECT, Oss50::mmSDMA0_PERFCOUNTER0_SELECT1,
                                     Oss50::mmSDMA0_PERFCOUNTER0_LO,     Oss50::mmSDMA0_PERFCOUNTER0_HI };
        pInfo->sdmaRegAddr[0][1] = { Oss50::mmSDMA0_PERFCOUNTER1_SELECT, Oss50::mmSDMA0_PERFCOUNTER1_SELECT1,
                                     Oss50::mmSDMA0_PERFCOUNTER1_LO,     Oss50::mmSDMA0_PERFCOUNTER1_HI };
        pInfo->sdmaRegAddr[1][0] = { Oss50::mmSDMA1_PERFCOUNTER0_SELECT, Oss50::mmSDMA1_PERFCOUNTER0_SELECT1,
                                     Oss50::mmSDMA1_PERFCOUNTER0_LO,     Oss50::mmSDMA1_PERFCOUNTER0_HI };
        pInfo->sdmaRegAddr[1][1] = { Oss50::mmSDMA1_PERFCOUNTER1_SELECT, Oss50::mmSDMA1_PERFCOUNTER1_SELECT1,
                                     Oss50::mmSDMA1_PERFCOUNTER1_LO,     Oss50::mmSDMA1_PERFCOUNTER1_HI };
    }
}

// =====================================================================================================================
// A helper function which updates the ATC's block info with device-specific data.
static void Gfx10UpdateAtcBlockInfo(
    const Pal::Device&    device,
    PerfCounterBlockInfo* pInfo)
{
    if (IsGfx103(device))
    {
        using namespace Gfx103;

        pInfo->regAddr = { mmATC_PERFCOUNTER_RSLT_CNTL, {
            { mmATC_PERFCOUNTER0_CFG, 0, mmATC_PERFCOUNTER_LO, mmATC_PERFCOUNTER_HI },
            { mmATC_PERFCOUNTER1_CFG, 0, mmATC_PERFCOUNTER_LO, mmATC_PERFCOUNTER_HI },
            { mmATC_PERFCOUNTER2_CFG, 0, mmATC_PERFCOUNTER_LO, mmATC_PERFCOUNTER_HI },
            { mmATC_PERFCOUNTER3_CFG, 0, mmATC_PERFCOUNTER_LO, mmATC_PERFCOUNTER_HI },
        }};
    }
    else
    {
        pInfo->regAddr = { Gfx101::mmATC_PERFCOUNTER_RSLT_CNTL, {
            { Gfx101::mmATC_PERFCOUNTER0_CFG, 0, Gfx101::mmATC_PERFCOUNTER_LO, Gfx101::mmATC_PERFCOUNTER_HI },
            { Gfx101::mmATC_PERFCOUNTER1_CFG, 0, Gfx101::mmATC_PERFCOUNTER_LO, Gfx101::mmATC_PERFCOUNTER_HI },
            { Gfx101::mmATC_PERFCOUNTER2_CFG, 0, Gfx101::mmATC_PERFCOUNTER_LO, Gfx101::mmATC_PERFCOUNTER_HI },
            { Gfx101::mmATC_PERFCOUNTER3_CFG, 0, Gfx101::mmATC_PERFCOUNTER_LO, Gfx101::mmATC_PERFCOUNTER_HI },
        }};
    }
}

// =====================================================================================================================
// A helper function which updates the EA's block info with device-specific data.
static void Gfx10UpdateEaBlockInfo(
    const Pal::Device&    device,
    PerfCounterBlockInfo* pInfo)
{
    if (IsGfx103(device))
    {
        using namespace Gfx103PlusExclusive;

        pInfo->regAddr = { mmGCEA_PERFCOUNTER_RSLT_CNTL, {
            { mmGCEA_PERFCOUNTER0_CFG,    0,                           mmGCEA_PERFCOUNTER_LO,  mmGCEA_PERFCOUNTER_HI  },
            { mmGCEA_PERFCOUNTER1_CFG,    0,                           mmGCEA_PERFCOUNTER_LO,  mmGCEA_PERFCOUNTER_HI  },
            { mmGCEA_PERFCOUNTER2_SELECT, mmGCEA_PERFCOUNTER2_SELECT1, mmGCEA_PERFCOUNTER2_LO, mmGCEA_PERFCOUNTER2_HI },
        }};
    }
    else
    {
        using namespace Gfx101;

        pInfo->regAddr = { mmGCEA_PERFCOUNTER_RSLT_CNTL, {
            { mmGCEA_PERFCOUNTER0_CFG, 0,                              mmGCEA_PERFCOUNTER_LO,  mmGCEA_PERFCOUNTER_HI  },
            { mmGCEA_PERFCOUNTER1_CFG, 0,                              mmGCEA_PERFCOUNTER_LO,  mmGCEA_PERFCOUNTER_HI  },
            { mmGCEA_PERFCOUNTER2_SELECT, mmGCEA_PERFCOUNTER2_SELECT1, mmGCEA_PERFCOUNTER2_LO, mmGCEA_PERFCOUNTER2_HI },
        }};
    }
}

// =====================================================================================================================
// A helper function which updates the GUS's block info with device-specific data.
static void Gfx10UpdateGusBlockInfo(
    const Pal::Device&    device,
    PerfCounterBlockInfo* pInfo)
{
    if (IsNavi2x(device))
    {
        pInfo->regAddr = { Nv2x::mmGUS_PERFCOUNTER_RSLT_CNTL, {
            { Nv2x::mmGUS_PERFCOUNTER0_CFG,    0,                                Nv2x::mmGUS_PERFCOUNTER_LO,  Nv2x::mmGUS_PERFCOUNTER_HI  },
            { Nv2x::mmGUS_PERFCOUNTER1_CFG,    0,                                Nv2x::mmGUS_PERFCOUNTER_LO,  Nv2x::mmGUS_PERFCOUNTER_HI  },
            { Nv2x::mmGUS_PERFCOUNTER2_SELECT, Nv2x::mmGUS_PERFCOUNTER2_SELECT1, Nv2x::mmGUS_PERFCOUNTER2_LO, Nv2x::mmGUS_PERFCOUNTER2_HI },
        }};
    }
    else
    {
        pInfo->regAddr = { Gfx101::mmGUS_PERFCOUNTER_RSLT_CNTL, {
            { Gfx101::mmGUS_PERFCOUNTER0_CFG,    0,                                  Gfx101::mmGUS_PERFCOUNTER_LO,  Gfx101::mmGUS_PERFCOUNTER_HI  },
            { Gfx101::mmGUS_PERFCOUNTER1_CFG,    0,                                  Gfx101::mmGUS_PERFCOUNTER_LO,  Gfx101::mmGUS_PERFCOUNTER_HI  },
            { Gfx101::mmGUS_PERFCOUNTER2_SELECT, Gfx101::mmGUS_PERFCOUNTER2_SELECT1, Gfx101::mmGUS_PERFCOUNTER2_LO, Gfx101::mmGUS_PERFCOUNTER2_HI },
        }};
    }
}

// =====================================================================================================================
// Initializes each block's basic hardware-defined information
// (distribution, numScopedInstances, numGenericSpmModules, etc.)
static void Gfx10InitBasicBlockInfo(
    const Pal::Device& device,
    GpuChipProperties* pProps)
{
    PerfCounterInfo*const pInfo    = &pProps->gfx9.perfCounterInfo.gfx9Info;
    const MaxEventIds&    maxIds   = GetEventLimits(device);
    const GfxIpLevel      gfxip    = device.ChipProperties().gfxLevel;
    const uint32          rbPerSa  = pProps->gfx9.maxNumRbPerSe / pProps->gfx9.numShaderArrays;

    // Pull in the generic gfx10 registers to make it easier to read the register tables.
    using namespace Gfx10;

    // Start by hard-coding hardware specific constants for each block. The shared blocks come first followed by
    // gfxip-specific blocks. Note that gfx10 removed or renamed a lot of blocks.
    //
    // The distribution and numScopedInstances (per-distribution) are derived from our hardware architecture.
    // The generic module counts are determined by:
    //   1. Does the block follow the generic programming model as defined by the perf experiment code?
    //   2. If so, there's one SPM module for each SELECT/SELECT1 pair and one legacy module for the remaining SELECTs.
    // The number of SPM wires is a hardware constant baked into each ASIC's design. So are the SPM block selects.
    // The maximum event IDs are the largest values from the hardware perf_sel enums.
    // Finally, we hard-code the PERFCOUNTER# register addresses for each module.

    PerfCounterBlockInfo*const pCpf = &pInfo->block[static_cast<uint32>(GpuBlock::Cpf)];
    pCpf->distribution              = PerfCounterDistribution::GlobalBlock;
    pCpf->numScopedInstances        = 1;
    pCpf->numGenericSpmModules      = 1; // CPF_PERFCOUNTER0
    pCpf->numGenericLegacyModules   = 1; // CPF_PERFCOUNTER1
    pCpf->numSpmWires               = 2;
    pCpf->spmBlockSelect            = Gfx10SpmGlobalBlockSelectCpf;
    pCpf->maxEventId                = maxIds[CpfPerfcountSelId];

    pCpf->regAddr = { 0, {
        { mmCPF_PERFCOUNTER0_SELECT, mmCPF_PERFCOUNTER0_SELECT1, mmCPF_PERFCOUNTER0_LO, mmCPF_PERFCOUNTER0_HI },
        { mmCPF_PERFCOUNTER1_SELECT, 0,                          mmCPF_PERFCOUNTER1_LO, mmCPF_PERFCOUNTER1_HI },
    }};

    PerfCounterBlockInfo*const pPa = &pInfo->block[static_cast<uint32>(GpuBlock::Pa)];
    pPa->distribution              = (gfxip <= GfxIpLevel::GfxIp10_1) ? PerfCounterDistribution::PerShaderArray
                                                                      : PerfCounterDistribution::PerShaderEngine;
    pPa->numScopedInstances        = 1;
    pPa->numGenericSpmModules      = 4; // PA_SU_PERFCOUNTER0-3
    pPa->numGenericLegacyModules   = 0;
    pPa->numSpmWires               = 8;
    pPa->spmBlockSelect            = Gfx10SpmSeBlockSelectPa;
    pPa->maxEventId                = maxIds[SuPerfcntSelId];

    pPa->regAddr = { 0, {
        { mmPA_SU_PERFCOUNTER0_SELECT, mmPA_SU_PERFCOUNTER0_SELECT1, mmPA_SU_PERFCOUNTER0_LO, mmPA_SU_PERFCOUNTER0_HI },
        { mmPA_SU_PERFCOUNTER1_SELECT, mmPA_SU_PERFCOUNTER1_SELECT1, mmPA_SU_PERFCOUNTER1_LO, mmPA_SU_PERFCOUNTER1_HI },
        { mmPA_SU_PERFCOUNTER2_SELECT, mmPA_SU_PERFCOUNTER2_SELECT1, mmPA_SU_PERFCOUNTER2_LO, mmPA_SU_PERFCOUNTER2_HI },
        { mmPA_SU_PERFCOUNTER3_SELECT, mmPA_SU_PERFCOUNTER3_SELECT1, mmPA_SU_PERFCOUNTER3_LO, mmPA_SU_PERFCOUNTER3_HI },
    }};

    // In gfx10 SC is subdivided into SCF (SCT) and 2xSCB per SA. The sets of perf counters (PA_SC_PERFCOUNTER{0-7})
    // are instantiated in each of the two SCBs. In the hardware docs these are called packers, thus we're really
    // gathering perf counters from individual packer instances.
    PerfCounterBlockInfo*const pSc = &pInfo->block[static_cast<uint32>(GpuBlock::Sc)];
    pSc->distribution              = PerfCounterDistribution::PerShaderArray;
    pSc->numScopedInstances        = 2;
    pSc->numGenericSpmModules      = 1; // PA_SC_PERFCOUNTER0
    pSc->numGenericLegacyModules   = 7; // PA_SC_PERFCOUNTER1-7
    pSc->numSpmWires               = 2;
    pSc->spmBlockSelect            = Gfx10SpmSeBlockSelectSc;
    pSc->maxEventId                = maxIds[ScPerfcntSelId];

    pSc->regAddr = { 0, {
        { mmPA_SC_PERFCOUNTER0_SELECT, mmPA_SC_PERFCOUNTER0_SELECT1, mmPA_SC_PERFCOUNTER0_LO, mmPA_SC_PERFCOUNTER0_HI },
        { mmPA_SC_PERFCOUNTER1_SELECT, 0,                            mmPA_SC_PERFCOUNTER1_LO, mmPA_SC_PERFCOUNTER1_HI },
        { mmPA_SC_PERFCOUNTER2_SELECT, 0,                            mmPA_SC_PERFCOUNTER2_LO, mmPA_SC_PERFCOUNTER2_HI },
        { mmPA_SC_PERFCOUNTER3_SELECT, 0,                            mmPA_SC_PERFCOUNTER3_LO, mmPA_SC_PERFCOUNTER3_HI },
        { mmPA_SC_PERFCOUNTER4_SELECT, 0,                            mmPA_SC_PERFCOUNTER4_LO, mmPA_SC_PERFCOUNTER4_HI },
        { mmPA_SC_PERFCOUNTER5_SELECT, 0,                            mmPA_SC_PERFCOUNTER5_LO, mmPA_SC_PERFCOUNTER5_HI },
        { mmPA_SC_PERFCOUNTER6_SELECT, 0,                            mmPA_SC_PERFCOUNTER6_LO, mmPA_SC_PERFCOUNTER6_HI },
        { mmPA_SC_PERFCOUNTER7_SELECT, 0,                            mmPA_SC_PERFCOUNTER7_LO, mmPA_SC_PERFCOUNTER7_HI },
    }};

    PerfCounterBlockInfo*const pSpi = &pInfo->block[static_cast<uint32>(GpuBlock::Spi)];
    pSpi->distribution              = PerfCounterDistribution::PerShaderEngine;
    pSpi->numScopedInstances        = 1;
    pSpi->numGenericSpmModules      = 4; // SPI_PERFCOUNTER0-3
    pSpi->numGenericLegacyModules   = 2; // SPI_PERFCOUNTER4-5
    pSpi->numSpmWires               = 8;
    pSpi->spmBlockSelect            = Gfx10SpmSeBlockSelectSpi;
    pSpi->maxEventId                = maxIds[SpiPerfcntSelId];

    pSpi->regAddr = { 0, {
        { mmSPI_PERFCOUNTER0_SELECT, mmSPI_PERFCOUNTER0_SELECT1, mmSPI_PERFCOUNTER0_LO, mmSPI_PERFCOUNTER0_HI },
        { mmSPI_PERFCOUNTER1_SELECT, mmSPI_PERFCOUNTER1_SELECT1, mmSPI_PERFCOUNTER1_LO, mmSPI_PERFCOUNTER1_HI },
        { mmSPI_PERFCOUNTER2_SELECT, mmSPI_PERFCOUNTER2_SELECT1, mmSPI_PERFCOUNTER2_LO, mmSPI_PERFCOUNTER2_HI },
        { mmSPI_PERFCOUNTER3_SELECT, mmSPI_PERFCOUNTER3_SELECT1, mmSPI_PERFCOUNTER3_LO, mmSPI_PERFCOUNTER3_HI },
        { mmSPI_PERFCOUNTER4_SELECT, 0,                          mmSPI_PERFCOUNTER4_LO, mmSPI_PERFCOUNTER4_HI },
        { mmSPI_PERFCOUNTER5_SELECT, 0,                          mmSPI_PERFCOUNTER5_LO, mmSPI_PERFCOUNTER5_HI },
    }};

    // The SQ counters are implemented by a single SQG in every shader engine. It has a unique programming model.
    // The SQ counter modules can be a global counter or one 32-bit SPM counter. 16-bit SPM is not supported but we
    // fake one 16-bit counter for now. All gfx10 ASICs contain all 16 out of the possible 16 counter modules.
    PerfCounterBlockInfo*const pSq = &pInfo->block[static_cast<uint32>(GpuBlock::Sq)];
    pSq->distribution              = PerfCounterDistribution::PerShaderEngine;
    pSq->numScopedInstances        = 1;
    pSq->num16BitSpmCounters       = 16;
    pSq->num32BitSpmCounters       = 16;
    pSq->numGlobalSharedCounters   = 16;
    pSq->numGenericSpmModules      = 0;
    pSq->numGenericLegacyModules   = 0;
    pSq->numSpmWires               = 16;
    pSq->spmBlockSelect            = Gfx10SpmSeBlockSelectSqg;
    pSq->maxEventId                = maxIds[SqPerfSelId];

    pSq->regAddr = { 0, {
        { mmSQ_PERFCOUNTER0_SELECT,  0, mmSQ_PERFCOUNTER0_LO,  mmSQ_PERFCOUNTER0_HI  },
        { mmSQ_PERFCOUNTER1_SELECT,  0, mmSQ_PERFCOUNTER1_LO,  mmSQ_PERFCOUNTER1_HI  },
        { mmSQ_PERFCOUNTER2_SELECT,  0, mmSQ_PERFCOUNTER2_LO,  mmSQ_PERFCOUNTER2_HI  },
        { mmSQ_PERFCOUNTER3_SELECT,  0, mmSQ_PERFCOUNTER3_LO,  mmSQ_PERFCOUNTER3_HI  },
        { mmSQ_PERFCOUNTER4_SELECT,  0, mmSQ_PERFCOUNTER4_LO,  mmSQ_PERFCOUNTER4_HI  },
        { mmSQ_PERFCOUNTER5_SELECT,  0, mmSQ_PERFCOUNTER5_LO,  mmSQ_PERFCOUNTER5_HI  },
        { mmSQ_PERFCOUNTER6_SELECT,  0, mmSQ_PERFCOUNTER6_LO,  mmSQ_PERFCOUNTER6_HI  },
        { mmSQ_PERFCOUNTER7_SELECT,  0, mmSQ_PERFCOUNTER7_LO,  mmSQ_PERFCOUNTER7_HI  },
        { mmSQ_PERFCOUNTER8_SELECT,  0, mmSQ_PERFCOUNTER8_LO,  mmSQ_PERFCOUNTER8_HI  },
        { mmSQ_PERFCOUNTER9_SELECT,  0, mmSQ_PERFCOUNTER9_LO,  mmSQ_PERFCOUNTER9_HI  },
        { mmSQ_PERFCOUNTER10_SELECT, 0, mmSQ_PERFCOUNTER10_LO, mmSQ_PERFCOUNTER10_HI },
        { mmSQ_PERFCOUNTER11_SELECT, 0, mmSQ_PERFCOUNTER11_LO, mmSQ_PERFCOUNTER11_HI },
        { mmSQ_PERFCOUNTER12_SELECT, 0, mmSQ_PERFCOUNTER12_LO, mmSQ_PERFCOUNTER12_HI },
        { mmSQ_PERFCOUNTER13_SELECT, 0, mmSQ_PERFCOUNTER13_LO, mmSQ_PERFCOUNTER13_HI },
        { mmSQ_PERFCOUNTER14_SELECT, 0, mmSQ_PERFCOUNTER14_LO, mmSQ_PERFCOUNTER14_HI },
        { mmSQ_PERFCOUNTER15_SELECT, 0, mmSQ_PERFCOUNTER15_LO, mmSQ_PERFCOUNTER15_HI },
    }};

    // The SX not a single block and thus has per-SE and per-SA qualities. For example, the SX crossbar routes requests
    // between SAs so it lives in the SE. However, the "interesting bits" of the SX are split in half, one half in
    // each SA. Perfcounter requests are forwarded to one half of the SX using the SA index so for us it's per-SA.
    PerfCounterBlockInfo*const pSx = &pInfo->block[static_cast<uint32>(GpuBlock::Sx)];
    pSx->distribution              = PerfCounterDistribution::PerShaderArray;
    pSx->numScopedInstances        = 1;
    pSx->numGenericSpmModules      = 2; // SX_PERFCOUNTER0-1
    pSx->numGenericLegacyModules   = 2; // SX_PERFCOUNTER2-3
    pSx->numSpmWires               = 4;
    pSx->spmBlockSelect            = Gfx10SpmSeBlockSelectSx;
    pSx->maxEventId                = maxIds[SxPerfcounterValsId];

    pSx->regAddr = { 0, {
        { mmSX_PERFCOUNTER0_SELECT, mmSX_PERFCOUNTER0_SELECT1, mmSX_PERFCOUNTER0_LO, mmSX_PERFCOUNTER0_HI },
        { mmSX_PERFCOUNTER1_SELECT, mmSX_PERFCOUNTER1_SELECT1, mmSX_PERFCOUNTER1_LO, mmSX_PERFCOUNTER1_HI },
        { mmSX_PERFCOUNTER2_SELECT, 0,                         mmSX_PERFCOUNTER2_LO, mmSX_PERFCOUNTER2_HI },
        { mmSX_PERFCOUNTER3_SELECT, 0,                         mmSX_PERFCOUNTER3_LO, mmSX_PERFCOUNTER3_HI },
    }};

    PerfCounterBlockInfo*const pTa = &pInfo->block[static_cast<uint32>(GpuBlock::Ta)];
    pTa->distribution              = PerfCounterDistribution::PerShaderArray;
    pTa->numScopedInstances        = pProps->gfx9.numCuPerSh;
    pTa->numGenericSpmModules      = 1; // TA_PERFCOUNTER0
    pTa->numGenericLegacyModules   = 1; // TA_PERFCOUNTER1
    pTa->numSpmWires               = 2;
    pTa->spmBlockSelect            = Gfx10SpmSeBlockSelectTa;
    pTa->maxEventId                = maxIds[TaPerfcountSelId];

    pTa->regAddr = { 0, {
        { mmTA_PERFCOUNTER0_SELECT, mmTA_PERFCOUNTER0_SELECT1, mmTA_PERFCOUNTER0_LO, mmTA_PERFCOUNTER0_HI },
        { mmTA_PERFCOUNTER1_SELECT, 0,                         mmTA_PERFCOUNTER1_LO, mmTA_PERFCOUNTER1_HI },
    }};

    PerfCounterBlockInfo*const pTd = &pInfo->block[static_cast<uint32>(GpuBlock::Td)];
    pTd->distribution              = PerfCounterDistribution::PerShaderArray;
    pTd->numScopedInstances        = pProps->gfx9.numCuPerSh;
    pTd->numGenericSpmModules      = 1; // TD_PERFCOUNTER0
    pTd->numGenericLegacyModules   = 1; // TD_PERFCOUNTER1
    pTd->numSpmWires               = 2;
    pTd->spmBlockSelect            = Gfx10SpmSeBlockSelectTd;
    pTd->maxEventId                = maxIds[TdPerfcountSelId];

    pTd->regAddr = { 0, {
        { mmTD_PERFCOUNTER0_SELECT, mmTD_PERFCOUNTER0_SELECT1, mmTD_PERFCOUNTER0_LO, mmTD_PERFCOUNTER0_HI },
        { mmTD_PERFCOUNTER1_SELECT, 0,                         mmTD_PERFCOUNTER1_LO, mmTD_PERFCOUNTER1_HI },
    }};

    PerfCounterBlockInfo*const pTcp = &pInfo->block[static_cast<uint32>(GpuBlock::Tcp)];
    pTcp->distribution              = PerfCounterDistribution::PerShaderArray;
    pTcp->numScopedInstances        = pProps->gfx9.gfx10.numTcpPerSa;
    pTcp->numGenericSpmModules      = 2; // TCP_PERFCOUNTER0-1
    pTcp->numGenericLegacyModules   = 2; // TCP_PERFCOUNTER2-3
    pTcp->numSpmWires               = 4;
    pTcp->spmBlockSelect            = Gfx10SpmSeBlockSelectTcp;
    pTcp->maxEventId                = maxIds[TcpPerfcountSelectId];

    pTcp->regAddr = { 0, {
        { mmTCP_PERFCOUNTER0_SELECT, mmTCP_PERFCOUNTER0_SELECT1, mmTCP_PERFCOUNTER0_LO, mmTCP_PERFCOUNTER0_HI },
        { mmTCP_PERFCOUNTER1_SELECT, mmTCP_PERFCOUNTER1_SELECT1, mmTCP_PERFCOUNTER1_LO, mmTCP_PERFCOUNTER1_HI },
        { mmTCP_PERFCOUNTER2_SELECT, 0,                          mmTCP_PERFCOUNTER2_LO, mmTCP_PERFCOUNTER2_HI },
        { mmTCP_PERFCOUNTER3_SELECT, 0,                          mmTCP_PERFCOUNTER3_LO, mmTCP_PERFCOUNTER3_HI },
    }};

    PerfCounterBlockInfo*const pDb = &pInfo->block[static_cast<uint32>(GpuBlock::Db)];
    pDb->distribution              = PerfCounterDistribution::PerShaderArray;
    pDb->numScopedInstances        = rbPerSa;
    pDb->numGenericSpmModules      = 2; // DB_PERFCOUNTER0-1
    pDb->numGenericLegacyModules   = 2; // DB_PERFCOUNTER2-3
    pDb->numSpmWires               = 4;
    pDb->spmBlockSelect            = Gfx10SpmSeBlockSelectDb;
    pDb->maxEventId                = maxIds[PerfcounterValsId];

    pDb->regAddr = { 0, {
        { mmDB_PERFCOUNTER0_SELECT, mmDB_PERFCOUNTER0_SELECT1, mmDB_PERFCOUNTER0_LO, mmDB_PERFCOUNTER0_HI },
        { mmDB_PERFCOUNTER1_SELECT, mmDB_PERFCOUNTER1_SELECT1, mmDB_PERFCOUNTER1_LO, mmDB_PERFCOUNTER1_HI },
        { mmDB_PERFCOUNTER2_SELECT, 0,                         mmDB_PERFCOUNTER2_LO, mmDB_PERFCOUNTER2_HI },
        { mmDB_PERFCOUNTER3_SELECT, 0,                         mmDB_PERFCOUNTER3_LO, mmDB_PERFCOUNTER3_HI },
    }};

    PerfCounterBlockInfo*const pCb = &pInfo->block[static_cast<uint32>(GpuBlock::Cb)];
    pCb->distribution              = PerfCounterDistribution::PerShaderArray;
    pCb->numScopedInstances        = rbPerSa;
    pCb->numGenericSpmModules      = 1; // CB_PERFCOUNTER0
    pCb->numGenericLegacyModules   = 3; // CB_PERFCOUNTER1-3
    pCb->numSpmWires               = 2;
    pCb->spmBlockSelect            = Gfx10SpmSeBlockSelectCb;
    pCb->maxEventId                = maxIds[CBPerfSelId];

    pCb->regAddr = { 0, {
        { mmCB_PERFCOUNTER0_SELECT, mmCB_PERFCOUNTER0_SELECT1, mmCB_PERFCOUNTER0_LO, mmCB_PERFCOUNTER0_HI },
        { mmCB_PERFCOUNTER1_SELECT, 0,                         mmCB_PERFCOUNTER1_LO, mmCB_PERFCOUNTER1_HI },
        { mmCB_PERFCOUNTER2_SELECT, 0,                         mmCB_PERFCOUNTER2_LO, mmCB_PERFCOUNTER2_HI },
        { mmCB_PERFCOUNTER3_SELECT, 0,                         mmCB_PERFCOUNTER3_LO, mmCB_PERFCOUNTER3_HI },
    }};

    PerfCounterBlockInfo*const pGds = &pInfo->block[static_cast<uint32>(GpuBlock::Gds)];
    pGds->distribution              = PerfCounterDistribution::GlobalBlock;
    pGds->numScopedInstances        = 1;
    pGds->spmBlockSelect            = Gfx10SpmGlobalBlockSelectGds;
    pGds->maxEventId                = maxIds[GdsPerfcountSelectId];

    // Sets the register addresses and configures the perf modules and SPM wires.
    Gfx10PlusUpdateGdsBlockInfo(device, pGds);

    PerfCounterBlockInfo*const pGrbm = &pInfo->block[static_cast<uint32>(GpuBlock::Grbm)];
    pGrbm->distribution              = PerfCounterDistribution::GlobalBlock;
    pGrbm->numScopedInstances        = 1;
    pGrbm->numGenericSpmModules      = 0;
    pGrbm->numGenericLegacyModules   = 2; // GRBM_PERFCOUNTER0-1
    pGrbm->numSpmWires               = 0;
    pGrbm->maxEventId                = MaxGrbmPerfSelGfx10;

    pGrbm->regAddr = { 0, {
        { mmGRBM_PERFCOUNTER0_SELECT, 0, mmGRBM_PERFCOUNTER0_LO, mmGRBM_PERFCOUNTER0_HI },
        { mmGRBM_PERFCOUNTER1_SELECT, 0, mmGRBM_PERFCOUNTER1_LO, mmGRBM_PERFCOUNTER1_HI },
    }};

    // These counters are a bit special. The GRBM is a global block but it defines one special counter per SE. We
    // abstract this as a special Grbm(per)Se block which needs special handling in the perf experiment.
    PerfCounterBlockInfo*const pGrbmSe = &pInfo->block[static_cast<uint32>(GpuBlock::GrbmSe)];
    pGrbmSe->distribution              = PerfCounterDistribution::PerShaderEngine;
    pGrbmSe->numScopedInstances        = 1;
    pGrbmSe->numGlobalOnlyCounters     = 1;
    pGrbmSe->numGenericSpmModules      = 0;
    pGrbmSe->numGenericLegacyModules   = 0;
    pGrbmSe->numSpmWires               = 0;
    pGrbmSe->maxEventId                = MaxGrbmSe0PerfSelGfx10;

    // By convention we access the counter register address array using the SE index.
    pGrbmSe->regAddr = { 0, {
        { mmGRBM_SE0_PERFCOUNTER_SELECT, 0, mmGRBM_SE0_PERFCOUNTER_LO, mmGRBM_SE0_PERFCOUNTER_HI },
        { mmGRBM_SE1_PERFCOUNTER_SELECT, 0, mmGRBM_SE1_PERFCOUNTER_LO, mmGRBM_SE1_PERFCOUNTER_HI },
        { mmGRBM_SE2_PERFCOUNTER_SELECT, 0, mmGRBM_SE2_PERFCOUNTER_LO, mmGRBM_SE2_PERFCOUNTER_HI },
        { mmGRBM_SE3_PERFCOUNTER_SELECT, 0, mmGRBM_SE3_PERFCOUNTER_LO, mmGRBM_SE3_PERFCOUNTER_HI },
    }};

    // The RLC's SELECT registers are non-standard because they lack PERF_MODE fields. This should be fine though
    // because we only use PERFMON_COUNTER_MODE_ACCUM which is zero. If we ever try to use a different mode the RLC
    // needs to be handled as a special case.
    static_assert(PERFMON_COUNTER_MODE_ACCUM == 0, "RLC legacy counters need special handling.");

    PerfCounterBlockInfo*const pRlc = &pInfo->block[static_cast<uint32>(GpuBlock::Rlc)];
    pRlc->distribution              = PerfCounterDistribution::GlobalBlock;
    pRlc->numScopedInstances        = 1;
    pRlc->numGenericSpmModules      = 0;
    pRlc->numGenericLegacyModules   = 2; // RLC_PERFCOUNTER0-1
    pRlc->numSpmWires               = 0;
    pRlc->maxEventId                = 6; // SERDES command write;

    pRlc->regAddr = { 0, {
        { mmRLC_PERFCOUNTER0_SELECT, 0, mmRLC_PERFCOUNTER0_LO, mmRLC_PERFCOUNTER0_HI },
        { mmRLC_PERFCOUNTER1_SELECT, 0, mmRLC_PERFCOUNTER1_LO, mmRLC_PERFCOUNTER1_HI },
    }};

    PerfCounterBlockInfo*const pDma = &pInfo->block[static_cast<uint32>(GpuBlock::Dma)];
    pDma->distribution              = PerfCounterDistribution::GlobalBlock;
    pDma->numScopedInstances        = device.EngineProperties().perEngine[EngineTypeDma].numAvailable;
    pDma->numGenericSpmModules      = 2; // SDMA#_PERFCOUNTER0-1
    pDma->numGenericLegacyModules   = 0;
    pDma->numSpmWires               = 4;
    pDma->spmBlockSelect            = Gfx10SpmGlobalBlockSelectSdma;
    pDma->maxEventId                = maxIds[SdmaPerfSelId];
    pDma->numScopedInstances        = Min(pDma->numScopedInstances, Gfx9MaxSdmaInstances);

    // Sets the register addresses.
    Gfx10UpdateSdmaBlockInfo(device, pInfo);

    PerfCounterBlockInfo*const pCpg = &pInfo->block[static_cast<uint32>(GpuBlock::Cpg)];
    pCpg->distribution              = PerfCounterDistribution::GlobalBlock;
    pCpg->numScopedInstances        = 1;
    pCpg->numGenericSpmModules      = 1; // CPG_PERFCOUNTER0
    pCpg->numGenericLegacyModules   = 1; // CPG_PERFCOUNTER1
    pCpg->numSpmWires               = 2;
    pCpg->spmBlockSelect            = Gfx10SpmGlobalBlockSelectCpg;
    pCpg->maxEventId                = maxIds[CpgPerfcountSelId];

    pCpg->regAddr = { 0, {
        { mmCPG_PERFCOUNTER0_SELECT, mmCPG_PERFCOUNTER0_SELECT1, mmCPG_PERFCOUNTER0_LO, mmCPG_PERFCOUNTER0_HI },
        { mmCPG_PERFCOUNTER1_SELECT, 0,                          mmCPG_PERFCOUNTER1_LO, mmCPG_PERFCOUNTER1_HI },
    }};

    PerfCounterBlockInfo*const pCpc = &pInfo->block[static_cast<uint32>(GpuBlock::Cpc)];
    pCpc->distribution              = PerfCounterDistribution::GlobalBlock;
    pCpc->numScopedInstances        = 1;
    pCpc->numGenericSpmModules      = 1; // CPC_PERFCOUNTER0
    pCpc->numGenericLegacyModules   = 1; // CPC_PERFCOUNTER1
    pCpc->numSpmWires               = 2;
    pCpc->spmBlockSelect            = Gfx10SpmGlobalBlockSelectCpc;
    pCpc->maxEventId                = maxIds[CpcPerfcountSelId];

    pCpc->regAddr = { 0, {
        { mmCPC_PERFCOUNTER0_SELECT, mmCPC_PERFCOUNTER0_SELECT1, mmCPC_PERFCOUNTER0_LO, mmCPC_PERFCOUNTER0_HI },
        { mmCPC_PERFCOUNTER1_SELECT, 0,                          mmCPC_PERFCOUNTER1_LO, mmCPC_PERFCOUNTER1_HI },
    }};

    // Also called the ATCL1.
    PerfCounterBlockInfo*const pAtc = &pInfo->block[static_cast<uint32>(GpuBlock::Atc)];
    pAtc->distribution              = PerfCounterDistribution::GlobalBlock;
    pAtc->numScopedInstances        = 1;
    pAtc->numGenericSpmModules      = 0;
    pAtc->numGenericLegacyModules   = 4; // ATC_PERFCOUNTER0-3
    pAtc->numSpmWires               = 0;
    pAtc->maxEventId                = 23;
    pAtc->isCfgStyle                = true;

    // Sets the register addresses.
    Gfx10UpdateAtcBlockInfo(device, pAtc);

    if ((IsNavi2x(device) == false))
    {
        PerfCounterBlockInfo*const pAtcL2 = &pInfo->block[static_cast<uint32>(GpuBlock::AtcL2)];
        pAtcL2->distribution              = PerfCounterDistribution::GlobalBlock;
        pAtcL2->numScopedInstances        = 1;
        pAtcL2->numGenericSpmModules      = 1; // GC_ATC_L2_PERFCOUNTER2
        pAtcL2->numGenericLegacyModules   = 2; // GC_ATC_L2_PERFCOUNTER0-1
        pAtcL2->numSpmWires               = 2;
        pAtcL2->spmBlockSelect            = Gfx10SpmGlobalBlockSelectGpuvmAtcl2;
        pAtcL2->maxEventId                = 8;
        pAtcL2->isCfgStyle                = true;

        pAtcL2->regAddr = { Apu103::mmGC_ATC_L2_PERFCOUNTER_RSLT_CNTL, {
            { Apu103::mmGC_ATC_L2_PERFCOUNTER0_CFG,    0,                                        Apu103::mmGC_ATC_L2_PERFCOUNTER_LO,  Apu103::mmGC_ATC_L2_PERFCOUNTER_HI  },
            { Apu103::mmGC_ATC_L2_PERFCOUNTER1_CFG,    0,                                        Apu103::mmGC_ATC_L2_PERFCOUNTER_LO,  Apu103::mmGC_ATC_L2_PERFCOUNTER_HI  },
            { Apu103::mmGC_ATC_L2_PERFCOUNTER2_SELECT, Apu103::mmGC_ATC_L2_PERFCOUNTER2_SELECT1, Apu103::mmGC_ATC_L2_PERFCOUNTER2_LO, Apu103::mmGC_ATC_L2_PERFCOUNTER2_HI },
        }};
    }

    // Also called the UTCL2.
    PerfCounterBlockInfo*const pMcVmL2 = &pInfo->block[static_cast<uint32>(GpuBlock::McVmL2)];
    pMcVmL2->distribution              = PerfCounterDistribution::GlobalBlock;
    pMcVmL2->numScopedInstances        = 1;
    pMcVmL2->numGenericSpmModules      = 2; // GCVML2_PERFCOUNTER2_0-1
    pMcVmL2->numGenericLegacyModules   = 8; // GCMC_VM_L2_PERFCOUNTER0-7
    pMcVmL2->numSpmWires               = 4;
    pMcVmL2->spmBlockSelect            = Gfx10SpmGlobalBlockSelectGpuvmVml2;
    pMcVmL2->maxEventId                = 90; // Number of l2 cache invalidations
    pMcVmL2->isCfgStyle                = true;

    pMcVmL2->regAddr = { mmGCMC_VM_L2_PERFCOUNTER_RSLT_CNTL, {
        { mmGCMC_VM_L2_PERFCOUNTER0_CFG,  0,                               mmGCMC_VM_L2_PERFCOUNTER_LO, mmGCMC_VM_L2_PERFCOUNTER_HI },
        { mmGCMC_VM_L2_PERFCOUNTER1_CFG,  0,                               mmGCMC_VM_L2_PERFCOUNTER_LO, mmGCMC_VM_L2_PERFCOUNTER_HI },
        { mmGCMC_VM_L2_PERFCOUNTER2_CFG,  0,                               mmGCMC_VM_L2_PERFCOUNTER_LO, mmGCMC_VM_L2_PERFCOUNTER_HI },
        { mmGCMC_VM_L2_PERFCOUNTER3_CFG,  0,                               mmGCMC_VM_L2_PERFCOUNTER_LO, mmGCMC_VM_L2_PERFCOUNTER_HI },
        { mmGCMC_VM_L2_PERFCOUNTER4_CFG,  0,                               mmGCMC_VM_L2_PERFCOUNTER_LO, mmGCMC_VM_L2_PERFCOUNTER_HI },
        { mmGCMC_VM_L2_PERFCOUNTER5_CFG,  0,                               mmGCMC_VM_L2_PERFCOUNTER_LO, mmGCMC_VM_L2_PERFCOUNTER_HI },
        { mmGCMC_VM_L2_PERFCOUNTER6_CFG,  0,                               mmGCMC_VM_L2_PERFCOUNTER_LO, mmGCMC_VM_L2_PERFCOUNTER_HI },
        { mmGCMC_VM_L2_PERFCOUNTER7_CFG,  0,                               mmGCMC_VM_L2_PERFCOUNTER_LO, mmGCMC_VM_L2_PERFCOUNTER_HI },
        { mmGCVML2_PERFCOUNTER2_0_SELECT, mmGCVML2_PERFCOUNTER2_0_SELECT1, mmGCVML2_PERFCOUNTER2_0_LO,  mmGCVML2_PERFCOUNTER2_0_HI  },
        { mmGCVML2_PERFCOUNTER2_1_SELECT, mmGCVML2_PERFCOUNTER2_1_SELECT1, mmGCVML2_PERFCOUNTER2_1_LO,  mmGCVML2_PERFCOUNTER2_1_HI  }
    }};

    // We should have one EA for each SDP.
    PerfCounterBlockInfo*const pEa = &pInfo->block[static_cast<uint32>(GpuBlock::Ea)];
    pEa->distribution              = PerfCounterDistribution::GlobalBlock;
    pEa->numScopedInstances        = pProps->gfx9.numSdpInterfaces;
    pEa->numGenericSpmModules      = 1;  // EA_PERFCOUNTER2
    pEa->numGenericLegacyModules   = 2;  // EA_PERFCOUNTER0-1
    pEa->numSpmWires               = 2;
    pEa->spmBlockSelect            = Gfx10SpmGlobalBlockSelectEa;
    pEa->isCfgStyle                = true;

    if (IsGfx103(device))
    {
        pEa->maxEventId            = 89; // | sarb | (size_p1 ) & (4(reqcoherent )) |
                                         // Coherent sized (in 32 byte increments) requests
    }
    else
    {
        pEa->maxEventId            = 88; // | mam | (3`b0, dbit_req_vld) |
                                         // Request considered valid by DBIT Memory sub-module
    }

    // Sets the register addresses.
    Gfx10UpdateEaBlockInfo(device, pEa);

    PerfCounterBlockInfo*const pRpb = &pInfo->block[static_cast<uint32>(GpuBlock::Rpb)];
    pRpb->distribution              = PerfCounterDistribution::GlobalBlock;
    pRpb->numScopedInstances        = 1;
    pRpb->numGenericSpmModules      = 0;
    pRpb->numGenericLegacyModules   = 4; // RPB_PERFCOUNTER0-3
    pRpb->numSpmWires               = 0;
    pRpb->maxEventId                = 63;
    pRpb->isCfgStyle                = true;

    pRpb->regAddr = { mmRPB_PERFCOUNTER_RSLT_CNTL, {
        { mmRPB_PERFCOUNTER0_CFG, 0, mmRPB_PERFCOUNTER_LO, mmRPB_PERFCOUNTER_HI },
        { mmRPB_PERFCOUNTER1_CFG, 0, mmRPB_PERFCOUNTER_LO, mmRPB_PERFCOUNTER_HI },
        { mmRPB_PERFCOUNTER2_CFG, 0, mmRPB_PERFCOUNTER_LO, mmRPB_PERFCOUNTER_HI },
        { mmRPB_PERFCOUNTER3_CFG, 0, mmRPB_PERFCOUNTER_LO, mmRPB_PERFCOUNTER_HI },
    }};

    // The RMI changes the way you use its perfcounters very frequently, see below for more info.
    PerfCounterBlockInfo*const pRmi = &pInfo->block[static_cast<uint32>(GpuBlock::Rmi)];
    pRmi->distribution              = PerfCounterDistribution::PerShaderArray;
    pRmi->spmBlockSelect            = Gfx10SpmSeBlockSelectRmi;
    pRmi->maxEventId                = maxIds[RMIPerfSelId];

    if (IsGfx103PlusExclusive(device))
    {
        // In gfx10.2+ there is only one RMI per RB. The RMI_PERFCOUNTER0/1 is only for read pipeline related counters
        // and RMI_PERCOUNTER2/3 is for write pipeline related counters. So in RMI_PERFCOUNTER0/1 all write related
        // counters are deprecated and in RMI_PERCOUNTER2/3 all read related counters are deprecated.
        //
        // This doesn't fit into our abstraction and is now the third model for how RMI perfcounters work. Rather than
        // codify this in an offical interface, which will probably become invalid in the future, we expose the read
        // and write counters and independent virtual sub-instances (even = read, odd = write). The user just has to
        // know to only program read events on the even instances and write events on the odd instances.
        pRmi->numScopedInstances        = rbPerSa * Gfx10NumRmiSubInstances;
        pRmi->numGenericSpmModules      = 1; // RMI_PERFCOUNTER0 or RMI_PERFCOUNTER2
        pRmi->numGenericLegacyModules   = 1; // RMI_PERFCOUNTER1 or RMI_PERFCOUNTER3
        pRmi->numSpmWires               = 4 / Gfx10NumRmiSubInstances; // We split these between our sub-instances.
    }
    else
    {
        // Each instance of RMI in a shader array connects to two RBs. There are two sets of perf counters which
        // profile one of these RB-RMI interfaces so PAL considers them separate perfcounter instances. However,
        // there are some events that are counted by both the sets of perf counters (ex: UTC events). The user just
        // has to know to only program RB0 events on the even instances and RB1 events on the odd instances.
        pRmi->numScopedInstances        = RoundUpQuotient(rbPerSa, 2u) * Gfx10NumRmiSubInstances;
        pRmi->numGenericSpmModules      = 1; // RMI_PERFCOUNTER0 or RMI_PERFCOUNTER2
        pRmi->numGenericLegacyModules   = 1; // RMI_PERFCOUNTER1 or RMI_PERFCOUNTER3
        pRmi->numSpmWires               = 4 / Gfx10NumRmiSubInstances; // We split these between our sub-instances.
    }

    pRmi->regAddr = { 0, {
            { mmRMI_PERFCOUNTER0_SELECT, mmRMI_PERFCOUNTER0_SELECT1, mmRMI_PERFCOUNTER0_LO, mmRMI_PERFCOUNTER0_HI },
            { mmRMI_PERFCOUNTER1_SELECT, 0,                          mmRMI_PERFCOUNTER1_LO, mmRMI_PERFCOUNTER1_HI },
            { mmRMI_PERFCOUNTER2_SELECT, mmRMI_PERFCOUNTER2_SELECT1, mmRMI_PERFCOUNTER2_LO, mmRMI_PERFCOUNTER2_HI },
            { mmRMI_PERFCOUNTER3_SELECT, 0,                          mmRMI_PERFCOUNTER3_LO, mmRMI_PERFCOUNTER3_HI },
    }};

    // This chip uses a 3rd party memory controller, so doesn't have a UMC
    if (true
    )
    {
        // The UMCCH has a unique programming model. It defines a fixed number of global counters for each instance.
        PerfCounterBlockInfo*const pUmcch = &pInfo->block[static_cast<uint32>(GpuBlock::Umcch)];
        pUmcch->distribution              = PerfCounterDistribution::GlobalBlock;
        pUmcch->numGlobalOnlyCounters     = Gfx9MaxUmcchPerfModules;
        pUmcch->numGenericSpmModules      = 0;
        pUmcch->numGenericLegacyModules   = 0;
        pUmcch->numSpmWires               = 0;

        // Note that this function also sets: numScopedInstances, maxEventId.
        Gfx10UpdateUmcchBlockInfo(device, pInfo, pUmcch);

        // A quick check to make sure we have registers for all instances. The fact that the number of instances varies
        // per ASIC doesn't mesh well with our register header scheme. If this hits UpdateUmcchBlockInfo needs fixing.
        PAL_ASSERT(pInfo->umcchRegAddr[pUmcch->numScopedInstances - 1].perfMonCtlClk != 0);
    }

    // The following blocks are new or renamed in gfx10.
    if (IsGfx103PlusExclusive(device))
    {
        using namespace Gfx103PlusExclusive;
        // Gfx10.3 split the GE perf counters into three independent units: GE1, GE2_DIST, and GE2_SE.
        PerfCounterBlockInfo*const pGe = &pInfo->block[static_cast<uint32>(GpuBlock::Ge)];
        pGe->distribution              = PerfCounterDistribution::GlobalBlock;
        pGe->numScopedInstances        = 1;
        pGe->numGenericSpmModules      = 4; // GE1_PERFCOUNTER0-3
        pGe->numGenericLegacyModules   = 0;
        pGe->numSpmWires               = 8;
        pGe->spmBlockSelect            = Gfx10SpmGlobalBlockSelectGe1;
        pGe->maxEventId                = maxIds[Ge1PerfcountSelectId];
        // Note that the register spec is incorrect on {GE1,GE2_DIST,GE2_SE}_PERFCOUNTER#_SELECT# - the PERF_MODE
        // fields are swapped. The actual hardware is identical to similar perfcounters so no special programming needed.

        pGe->regAddr = { 0, {
            { mmGE1_PERFCOUNTER0_SELECT, mmGE1_PERFCOUNTER0_SELECT1, mmGE1_PERFCOUNTER0_LO, mmGE1_PERFCOUNTER0_HI },
            { mmGE1_PERFCOUNTER1_SELECT, mmGE1_PERFCOUNTER1_SELECT1, mmGE1_PERFCOUNTER1_LO, mmGE1_PERFCOUNTER1_HI },
            { mmGE1_PERFCOUNTER2_SELECT, mmGE1_PERFCOUNTER2_SELECT1, mmGE1_PERFCOUNTER2_LO, mmGE1_PERFCOUNTER2_HI },
            { mmGE1_PERFCOUNTER3_SELECT, mmGE1_PERFCOUNTER3_SELECT1, mmGE1_PERFCOUNTER3_LO, mmGE1_PERFCOUNTER3_HI },
        }};

        PerfCounterBlockInfo*const pGeDist = &pInfo->block[static_cast<uint32>(GpuBlock::GeDist)];
        pGeDist->distribution              = PerfCounterDistribution::GlobalBlock;
        pGeDist->numScopedInstances        = 1;
        pGeDist->numGenericSpmModules      = 4; // GE2_DIST_PERFCOUNTER0-3
        pGeDist->numGenericLegacyModules   = 0;
        pGeDist->numSpmWires               = 8;
        pGeDist->spmBlockSelect            = Gfx10SpmGlobalBlockSelectGe2Dist;
        pGeDist->maxEventId                = maxIds[Ge2DistPerfcountSelectId];

        pGeDist->regAddr = { 0, {
            { mmGE2_DIST_PERFCOUNTER0_SELECT, mmGE2_DIST_PERFCOUNTER0_SELECT1, mmGE2_DIST_PERFCOUNTER0_LO, mmGE2_DIST_PERFCOUNTER0_HI },
            { mmGE2_DIST_PERFCOUNTER1_SELECT, mmGE2_DIST_PERFCOUNTER1_SELECT1, mmGE2_DIST_PERFCOUNTER1_LO, mmGE2_DIST_PERFCOUNTER1_HI },
            { mmGE2_DIST_PERFCOUNTER2_SELECT, mmGE2_DIST_PERFCOUNTER2_SELECT1, mmGE2_DIST_PERFCOUNTER2_LO, mmGE2_DIST_PERFCOUNTER2_HI },
            { mmGE2_DIST_PERFCOUNTER3_SELECT, mmGE2_DIST_PERFCOUNTER3_SELECT1, mmGE2_DIST_PERFCOUNTER3_LO, mmGE2_DIST_PERFCOUNTER3_HI },
        }};

        // Yes, it is correct that this block is programmed per-SE but has a global SPM select. It is technically not
        // a real per-SE block because it doesn't live in the SEs but the global hardware still creates one GE2_SE
        // instance for each SE. It listens to GRBM_GFX_INDEX like a per-SE block but the SPM wires correctly hook
        // into the global mux interface.
        PerfCounterBlockInfo*const pGeSe = &pInfo->block[static_cast<uint32>(GpuBlock::GeSe)];
        pGeSe->distribution              = PerfCounterDistribution::PerShaderEngine;
        pGeSe->numScopedInstances        = 1;
        pGeSe->numGenericSpmModules      = 4; // GE2_SE_PERFCOUNTER0-3
        pGeSe->numGenericLegacyModules   = 0;
        pGeSe->numSpmWires               = 8;
        pGeSe->spmBlockSelect            = Gfx10SpmGlobalBlockSelectGe2Se;
        pGeSe->maxEventId                = maxIds[Ge2SePerfcountSelectId];

        pGeSe->regAddr = { 0, {
            { mmGE2_SE_PERFCOUNTER0_SELECT, mmGE2_SE_PERFCOUNTER0_SELECT1, mmGE2_SE_PERFCOUNTER0_LO, mmGE2_SE_PERFCOUNTER0_HI },
            { mmGE2_SE_PERFCOUNTER1_SELECT, mmGE2_SE_PERFCOUNTER1_SELECT1, mmGE2_SE_PERFCOUNTER1_LO, mmGE2_SE_PERFCOUNTER1_HI },
            { mmGE2_SE_PERFCOUNTER2_SELECT, mmGE2_SE_PERFCOUNTER2_SELECT1, mmGE2_SE_PERFCOUNTER2_LO, mmGE2_SE_PERFCOUNTER2_HI },
            { mmGE2_SE_PERFCOUNTER3_SELECT, mmGE2_SE_PERFCOUNTER3_SELECT1, mmGE2_SE_PERFCOUNTER3_LO, mmGE2_SE_PERFCOUNTER3_HI },
        }};
    }
    else
    {
        PerfCounterBlockInfo*const pGe = &pInfo->block[static_cast<uint32>(GpuBlock::Ge)];
        pGe->distribution              = PerfCounterDistribution::GlobalBlock;
        pGe->numScopedInstances        = 1;
        pGe->numGenericSpmModules      = 4; // GE_PERFCOUNTER0-3
        pGe->numGenericLegacyModules   = 8; // GE_PERFCOUNTER4-11
        pGe->numSpmWires               = 8;
        pGe->spmBlockSelect            = Gfx10SpmGlobalBlockSelectGe;
        pGe->maxEventId                = maxIds[GePerfcountSelectId];

        pGe->regAddr = { 0, {
            { Gfx101::mmGE_PERFCOUNTER0_SELECT,  Gfx101::mmGE_PERFCOUNTER0_SELECT1, Gfx101::mmGE_PERFCOUNTER0_LO,  Gfx101::mmGE_PERFCOUNTER0_HI  },
            { Gfx101::mmGE_PERFCOUNTER1_SELECT,  Gfx101::mmGE_PERFCOUNTER1_SELECT1, Gfx101::mmGE_PERFCOUNTER1_LO,  Gfx101::mmGE_PERFCOUNTER1_HI  },
            { Gfx101::mmGE_PERFCOUNTER2_SELECT,  Gfx101::mmGE_PERFCOUNTER2_SELECT1, Gfx101::mmGE_PERFCOUNTER2_LO,  Gfx101::mmGE_PERFCOUNTER2_HI  },
            { Gfx101::mmGE_PERFCOUNTER3_SELECT,  Gfx101::mmGE_PERFCOUNTER3_SELECT1, Gfx101::mmGE_PERFCOUNTER3_LO,  Gfx101::mmGE_PERFCOUNTER3_HI  },
            { Gfx101::mmGE_PERFCOUNTER4_SELECT,  0,                                 Gfx101::mmGE_PERFCOUNTER4_LO,  Gfx101::mmGE_PERFCOUNTER4_HI  },
            { Gfx101::mmGE_PERFCOUNTER5_SELECT,  0,                                 Gfx101::mmGE_PERFCOUNTER5_LO,  Gfx101::mmGE_PERFCOUNTER5_HI  },
            { Gfx101::mmGE_PERFCOUNTER6_SELECT,  0,                                 Gfx101::mmGE_PERFCOUNTER6_LO,  Gfx101::mmGE_PERFCOUNTER6_HI  },
            { Gfx101::mmGE_PERFCOUNTER7_SELECT,  0,                                 Gfx101::mmGE_PERFCOUNTER7_LO,  Gfx101::mmGE_PERFCOUNTER7_HI  },
            { Gfx101::mmGE_PERFCOUNTER8_SELECT,  0,                                 Gfx101::mmGE_PERFCOUNTER8_LO,  Gfx101::mmGE_PERFCOUNTER8_HI  },
            { Gfx101::mmGE_PERFCOUNTER9_SELECT,  0,                                 Gfx101::mmGE_PERFCOUNTER9_LO,  Gfx101::mmGE_PERFCOUNTER9_HI  },
            { Gfx101::mmGE_PERFCOUNTER10_SELECT, 0,                                 Gfx101::mmGE_PERFCOUNTER10_LO, Gfx101::mmGE_PERFCOUNTER10_HI },
            { Gfx101::mmGE_PERFCOUNTER11_SELECT, 0,                                 Gfx101::mmGE_PERFCOUNTER11_LO, Gfx101::mmGE_PERFCOUNTER11_HI },
        }};
    }

    //
    // The GL1 arbiter. The GL1 complex is per-SA by definition.
    PerfCounterBlockInfo*const pGl1a = &pInfo->block[static_cast<uint32>(GpuBlock::Gl1a)];
    pGl1a->distribution              = PerfCounterDistribution::PerShaderArray;
    pGl1a->numScopedInstances        = 1;
    pGl1a->numGenericSpmModules      = 1; // GL1A_PERFCOUNTER0
    pGl1a->numGenericLegacyModules   = 3; // GL1A_PERFCOUNTER1-3
    pGl1a->numSpmWires               = 2;
    pGl1a->spmBlockSelect            = Gfx10SpmSeBlockSelectGl1a;
    pGl1a->maxEventId                = maxIds[Gl1aPerfSelId];

    pGl1a->regAddr = { 0, {
        { mmGL1A_PERFCOUNTER0_SELECT, mmGL1A_PERFCOUNTER0_SELECT1, mmGL1A_PERFCOUNTER0_LO, mmGL1A_PERFCOUNTER0_HI },
        { mmGL1A_PERFCOUNTER1_SELECT, 0,                           mmGL1A_PERFCOUNTER1_LO, mmGL1A_PERFCOUNTER1_HI },
        { mmGL1A_PERFCOUNTER2_SELECT, 0,                           mmGL1A_PERFCOUNTER2_LO, mmGL1A_PERFCOUNTER2_HI },
        { mmGL1A_PERFCOUNTER3_SELECT, 0,                           mmGL1A_PERFCOUNTER3_LO, mmGL1A_PERFCOUNTER3_HI },
    }};

    // The GL1 cache.
    PerfCounterBlockInfo*const pGl1c = &pInfo->block[static_cast<uint32>(GpuBlock::Gl1c)];
    pGl1c->distribution              = PerfCounterDistribution::PerShaderArray;
    pGl1c->numScopedInstances        = 4; // Each GL1A talks to four GL1C quadrants.
    pGl1c->numGenericSpmModules      = 1; // GL1C_PERFCOUNTER0
    pGl1c->numGenericLegacyModules   = 3; // GL1C_PERFCOUNTER1-3
    pGl1c->numSpmWires               = 2;
    pGl1c->spmBlockSelect            = Gfx10SpmSeBlockSelectGl1c;
    pGl1c->maxEventId                = maxIds[Gl1cPerfSelId];

    pGl1c->regAddr = { 0, {
        { mmGL1C_PERFCOUNTER0_SELECT, mmGL1C_PERFCOUNTER0_SELECT1, mmGL1C_PERFCOUNTER0_LO, mmGL1C_PERFCOUNTER0_HI },
        { mmGL1C_PERFCOUNTER1_SELECT, 0,                           mmGL1C_PERFCOUNTER1_LO, mmGL1C_PERFCOUNTER1_HI },
        { mmGL1C_PERFCOUNTER2_SELECT, 0,                           mmGL1C_PERFCOUNTER2_LO, mmGL1C_PERFCOUNTER2_HI },
        { mmGL1C_PERFCOUNTER3_SELECT, 0,                           mmGL1C_PERFCOUNTER3_LO, mmGL1C_PERFCOUNTER3_HI },
    }};

    // The GL2A (gl2 arbiter) block is typically broken down into four quadrants - we treat them as four instances.
    PerfCounterBlockInfo*const pGl2a = &pInfo->block[static_cast<uint32>(GpuBlock::Gl2a)];
    pGl2a->distribution              = PerfCounterDistribution::GlobalBlock;
    pGl2a->numScopedInstances        = pProps->gfx9.gfx10.numGl2a;
    pGl2a->numGenericSpmModules      = 2; // Gl2A_PERFCOUNTER0-1
    pGl2a->numGenericLegacyModules   = 2; // Gl2A_PERFCOUNTER2-3
    pGl2a->numSpmWires               = 4;
    pGl2a->spmBlockSelect            = Gfx10SpmGlobalBlockSelectGl2a;
    pGl2a->maxEventId                = maxIds[Gl2aPerfSelId];

    pGl2a->regAddr = { 0, {
        { mmGL2A_PERFCOUNTER0_SELECT, mmGL2A_PERFCOUNTER0_SELECT1, mmGL2A_PERFCOUNTER0_LO, mmGL2A_PERFCOUNTER0_HI },
        { mmGL2A_PERFCOUNTER1_SELECT, mmGL2A_PERFCOUNTER1_SELECT1, mmGL2A_PERFCOUNTER1_LO, mmGL2A_PERFCOUNTER1_HI },
        { mmGL2A_PERFCOUNTER2_SELECT, 0,                           mmGL2A_PERFCOUNTER2_LO, mmGL2A_PERFCOUNTER2_HI },
        { mmGL2A_PERFCOUNTER3_SELECT, 0,                           mmGL2A_PERFCOUNTER3_LO, mmGL2A_PERFCOUNTER3_HI },
    }};

    PerfCounterBlockInfo*const pGl2c = &pInfo->block[static_cast<uint32>(GpuBlock::Gl2c)];
    pGl2c->distribution              = PerfCounterDistribution::GlobalBlock;
    pGl2c->numScopedInstances        = pProps->gfx9.gfx10.numGl2c; // This should be equal to the number of EAs.
    pGl2c->numGenericSpmModules      = 2; // Gl2C_PERFCOUNTER0-1
    pGl2c->numGenericLegacyModules   = 2; // Gl2C_PERFCOUNTER2-3
    pGl2c->numSpmWires               = 4;
    pGl2c->spmBlockSelect            = Gfx10SpmGlobalBlockSelectGl2c;
    pGl2c->maxEventId                = maxIds[Gl2cPerfSelId];

    pGl2c->regAddr = { 0, {
        { mmGL2C_PERFCOUNTER0_SELECT, mmGL2C_PERFCOUNTER0_SELECT1, mmGL2C_PERFCOUNTER0_LO, mmGL2C_PERFCOUNTER0_HI },
        { mmGL2C_PERFCOUNTER1_SELECT, mmGL2C_PERFCOUNTER1_SELECT1, mmGL2C_PERFCOUNTER1_LO, mmGL2C_PERFCOUNTER1_HI },
        { mmGL2C_PERFCOUNTER2_SELECT, 0,                           mmGL2C_PERFCOUNTER2_LO, mmGL2C_PERFCOUNTER2_HI },
        { mmGL2C_PERFCOUNTER3_SELECT, 0,                           mmGL2C_PERFCOUNTER3_LO, mmGL2C_PERFCOUNTER3_HI },
    }};

    // The center hub arbiter (CHA). It's the global version of the GL1A and is used by global blocks.
    PerfCounterBlockInfo*const pCha = &pInfo->block[static_cast<uint32>(GpuBlock::Cha)];
    pCha->distribution              = PerfCounterDistribution::GlobalBlock;
    pCha->numScopedInstances        = 1;
    pCha->numGenericSpmModules      = 1; // CHA_PERFCOUNTER0
    pCha->numGenericLegacyModules   = 3; // CHA_PERFCOUNTER1-3
    pCha->numSpmWires               = 2;
    pCha->spmBlockSelect            = Gfx10SpmGlobalBlockSelectCha;
    pCha->maxEventId                = maxIds[ChaPerfSelId];

    pCha->regAddr = { 0, {
        { mmCHA_PERFCOUNTER0_SELECT, mmCHA_PERFCOUNTER0_SELECT1, mmCHA_PERFCOUNTER0_LO, mmCHA_PERFCOUNTER0_HI },
        { mmCHA_PERFCOUNTER1_SELECT, 0,                          mmCHA_PERFCOUNTER1_LO, mmCHA_PERFCOUNTER1_HI },
        { mmCHA_PERFCOUNTER2_SELECT, 0,                          mmCHA_PERFCOUNTER2_LO, mmCHA_PERFCOUNTER2_HI },
        { mmCHA_PERFCOUNTER3_SELECT, 0,                          mmCHA_PERFCOUNTER3_LO, mmCHA_PERFCOUNTER3_HI },
    }};

    // The center hub buffer (CHC). It's the global version of the GL1C and is used by global blocks.
    PerfCounterBlockInfo*const pChc = &pInfo->block[static_cast<uint32>(GpuBlock::Chc)];
    pChc->distribution              = PerfCounterDistribution::GlobalBlock;
    pChc->numScopedInstances        = 4; // It also has four quadrants like the GL1C.
    pChc->numGenericSpmModules      = 1; // CHC_PERFCOUNTER0
    pChc->numGenericLegacyModules   = 3; // CHC_PERFCOUNTER1-3
    pChc->numSpmWires               = 2;
    pChc->spmBlockSelect            = Gfx10SpmGlobalBlockSelectChc;
    pChc->maxEventId                = maxIds[ChcPerfSelId];

    pChc->regAddr = { 0, {
        { mmCHC_PERFCOUNTER0_SELECT, mmCHC_PERFCOUNTER0_SELECT1, mmCHC_PERFCOUNTER0_LO, mmCHC_PERFCOUNTER0_HI },
        { mmCHC_PERFCOUNTER1_SELECT, 0,                          mmCHC_PERFCOUNTER1_LO, mmCHC_PERFCOUNTER1_HI },
        { mmCHC_PERFCOUNTER2_SELECT, 0,                          mmCHC_PERFCOUNTER2_LO, mmCHC_PERFCOUNTER2_HI },
        { mmCHC_PERFCOUNTER3_SELECT, 0,                          mmCHC_PERFCOUNTER3_LO, mmCHC_PERFCOUNTER3_HI },
    }};

    // The global block that implements the graphics cache rinse feature.
    PerfCounterBlockInfo*const pGcr = &pInfo->block[static_cast<uint32>(GpuBlock::Gcr)];
    pGcr->distribution              = PerfCounterDistribution::GlobalBlock;
    pGcr->numScopedInstances        = 1;
    pGcr->numGenericSpmModules      = 1; // GCR_PERFCOUNTER0
    pGcr->numGenericLegacyModules   = 1; // GCR_PERFCOUNTER1
    pGcr->numSpmWires               = 2;
    pGcr->spmBlockSelect            = Gfx10SpmGlobalBlockSelectGcr;
    pGcr->maxEventId                = maxIds[GCRPerfSelId];

    pGcr->regAddr = { 0, {
        { mmGCR_PERFCOUNTER0_SELECT, mmGCR_PERFCOUNTER0_SELECT1, mmGCR_PERFCOUNTER0_LO, mmGCR_PERFCOUNTER0_HI },
        { mmGCR_PERFCOUNTER1_SELECT, 0,                          mmGCR_PERFCOUNTER1_LO, mmGCR_PERFCOUNTER1_HI },
    }};

    PerfCounterBlockInfo*const pPh = &pInfo->block[static_cast<uint32>(GpuBlock::Ph)];
    pPh->distribution              = PerfCounterDistribution::GlobalBlock;
    pPh->numScopedInstances        = 1;
    pPh->numGenericSpmModules      = 4; // PA_PH_PERFCOUNTER0-3
    pPh->numGenericLegacyModules   = 4; // PA_PH_PERFCOUNTER4-7
    pPh->numSpmWires               = 8;
    pPh->spmBlockSelect            = Gfx10SpmGlobalBlockSelectPh;
    pPh->maxEventId                = maxIds[PhPerfcntSelId];

    pPh->regAddr = { 0, {
        { mmPA_PH_PERFCOUNTER0_SELECT, mmPA_PH_PERFCOUNTER0_SELECT1, mmPA_PH_PERFCOUNTER0_LO, mmPA_PH_PERFCOUNTER0_HI },
        { mmPA_PH_PERFCOUNTER1_SELECT, mmPA_PH_PERFCOUNTER1_SELECT1, mmPA_PH_PERFCOUNTER1_LO, mmPA_PH_PERFCOUNTER1_HI },
        { mmPA_PH_PERFCOUNTER2_SELECT, mmPA_PH_PERFCOUNTER2_SELECT1, mmPA_PH_PERFCOUNTER2_LO, mmPA_PH_PERFCOUNTER2_HI },
        { mmPA_PH_PERFCOUNTER3_SELECT, mmPA_PH_PERFCOUNTER3_SELECT1, mmPA_PH_PERFCOUNTER3_LO, mmPA_PH_PERFCOUNTER3_HI },
        { mmPA_PH_PERFCOUNTER4_SELECT, 0,                            mmPA_PH_PERFCOUNTER4_LO, mmPA_PH_PERFCOUNTER4_HI },
        { mmPA_PH_PERFCOUNTER5_SELECT, 0,                            mmPA_PH_PERFCOUNTER5_LO, mmPA_PH_PERFCOUNTER5_HI },
        { mmPA_PH_PERFCOUNTER6_SELECT, 0,                            mmPA_PH_PERFCOUNTER6_LO, mmPA_PH_PERFCOUNTER6_HI },
        { mmPA_PH_PERFCOUNTER7_SELECT, 0,                            mmPA_PH_PERFCOUNTER7_LO, mmPA_PH_PERFCOUNTER7_HI },
    }};

    PerfCounterBlockInfo*const pUtcl1 = &pInfo->block[static_cast<uint32>(GpuBlock::UtcL1)];
    pUtcl1->distribution              = PerfCounterDistribution::PerShaderArray;
    pUtcl1->numScopedInstances        = 1;
    pUtcl1->numGenericLegacyModules   = 2; // UTCL1_PERFCOUNTER0-1
    pUtcl1->numSpmWires               = 0;
    pUtcl1->numGenericSpmModules      = 0;
    pUtcl1->maxEventId                = maxIds[UTCL1PerfSelId];

    pUtcl1->regAddr = { 0, {
        { mmUTCL1_PERFCOUNTER0_SELECT, 0, mmUTCL1_PERFCOUNTER0_LO, mmUTCL1_PERFCOUNTER0_HI },
        { mmUTCL1_PERFCOUNTER1_SELECT, 0, mmUTCL1_PERFCOUNTER1_LO, mmUTCL1_PERFCOUNTER1_HI },
    }};

    // The GUS and the blocks that exist to service it should exist as a unit. They are present on all gfx10.1 ASICs.
    if (IsGfx101(device) || (IsGfx103Plus(device) && (pProps->gpuType == GpuType::Discrete)))
    {
        // The CHCG connects the CH to the GUS, similar to how the CHC connects the CH to the GL2.
        PerfCounterBlockInfo*const pChcg = &pInfo->block[static_cast<uint32>(GpuBlock::Chcg)];
        pChcg->distribution              = PerfCounterDistribution::GlobalBlock;
        pChcg->numScopedInstances        = 1;
        pChcg->numGenericSpmModules      = 1; // CHCG_PERFCOUNTER0
        pChcg->numGenericLegacyModules   = 3; // CHCG_PERFCOUNTER1-3
        pChcg->numSpmWires               = 2;
        pChcg->spmBlockSelect            = Gfx10SpmGlobalBlockSelectChcg;
        pChcg->maxEventId                = maxIds[ChcgPerfSelId];

        pChcg->regAddr = { 0, {
            { Gfx101::mmCHCG_PERFCOUNTER0_SELECT, Gfx101::mmCHCG_PERFCOUNTER0_SELECT1, Gfx101::mmCHCG_PERFCOUNTER0_LO, Gfx101::mmCHCG_PERFCOUNTER0_HI },
            { Gfx101::mmCHCG_PERFCOUNTER1_SELECT, 0,                                   Gfx101::mmCHCG_PERFCOUNTER1_LO, Gfx101::mmCHCG_PERFCOUNTER1_HI },
            { Gfx101::mmCHCG_PERFCOUNTER2_SELECT, 0,                                   Gfx101::mmCHCG_PERFCOUNTER2_LO, Gfx101::mmCHCG_PERFCOUNTER2_HI },
            { Gfx101::mmCHCG_PERFCOUNTER3_SELECT, 0,                                   Gfx101::mmCHCG_PERFCOUNTER3_LO, Gfx101::mmCHCG_PERFCOUNTER3_HI },
        }};

        static_assert(Nv2x::mmCHCG_PERFCOUNTER0_SELECT == Gfx101::mmCHCG_PERFCOUNTER0_SELECT,
                      "Must fix CHCG registers!");

        PerfCounterBlockInfo*const pGus = &pInfo->block[static_cast<uint32>(GpuBlock::Gus)];
        pGus->distribution              = PerfCounterDistribution::GlobalBlock;
        pGus->numScopedInstances        = 1;
        pGus->numGenericSpmModules      = 1; // GUS_PERFCOUNTER2
        pGus->numGenericLegacyModules   = 2; // GUS_PERFCOUNTER0-1
        pGus->numSpmWires               = 2;
        pGus->spmBlockSelect            = Gfx10SpmGlobalBlockSelectGus;
        pGus->maxEventId                = 175; // | dram_wr | Transaction `end` from CH_HI combiner, latency sampler 1
        pGus->isCfgStyle                = true;

        // Sets the register addresses.
        Gfx10UpdateGusBlockInfo(device, pGus);
    }

    // The DF counters should be accessible in earlier hardware but gfx10.3 is when the UMD was given permission
    // to program the DF perfcounter registers.
    if (IsGfx103Plus(device))
    {
        // The data fabric (DF) perf counters are shared by all DF subblocks (e.g., the MALL). At the hardware level,
        // the DF's event selects combine the typical event ID with the instance ID of the target subblock. There is
        // nothing like GRBM_GFX_INDEX to select instances, you must use one global counter for each instance.
        //
        // We have abstracted each DF subblock as a single GpuBlock (e.g., GpuBlock::DfMall) to make it easier to
        // program DF counters. The block instance and counter event ID refer to a specific DF subblock instance and
        // that subblock's specific event selects. The only complication is that the "num counters" caps are shared
        // between all DF subblocks. The client must look at the max values reported by any DF subblock and apply that
        // to the sum of all enabled DF counters.
        PerfCounterBlockInfo*const pDfMall = &pInfo->block[static_cast<uint32>(GpuBlock::DfMall)];
        pDfMall->distribution              = PerfCounterDistribution::GlobalBlock;
        pDfMall->numScopedInstances        = pProps->gfx9.numSdpInterfaces; // There's one MALL per memory channel.
        pDfMall->instanceGroupSize         = pProps->gfx9.numSdpInterfaces; // There's one shared set of counters.
        pDfMall->numGlobalOnlyCounters     = 8; // DF_PIE_AON_PerfMon*0-7
        pDfMall->numGenericSpmModules      = 0;
        pDfMall->numGenericLegacyModules   = 0;
        pDfMall->numSpmWires               = 0;
        pDfMall->maxEventId                = MaxDfMallPerfSelNv2x;

        // Note that the DF isn't perfmon-style or cfg-style; it has a unique programming model.
        pDfMall->regAddr = { 0, {
            { Nv2x::mmDF_PIE_AON_PerfMonCtlLo0, Nv2x::mmDF_PIE_AON_PerfMonCtlHi0, Nv2x::mmDF_PIE_AON_PerfMonCtrLo0, Nv2x::mmDF_PIE_AON_PerfMonCtrHi0 },
            { Nv2x::mmDF_PIE_AON_PerfMonCtlLo1, Nv2x::mmDF_PIE_AON_PerfMonCtlHi1, Nv2x::mmDF_PIE_AON_PerfMonCtrLo1, Nv2x::mmDF_PIE_AON_PerfMonCtrHi1 },
            { Nv2x::mmDF_PIE_AON_PerfMonCtlLo2, Nv2x::mmDF_PIE_AON_PerfMonCtlHi2, Nv2x::mmDF_PIE_AON_PerfMonCtrLo2, Nv2x::mmDF_PIE_AON_PerfMonCtrHi2 },
            { Nv2x::mmDF_PIE_AON_PerfMonCtlLo3, Nv2x::mmDF_PIE_AON_PerfMonCtlHi3, Nv2x::mmDF_PIE_AON_PerfMonCtrLo3, Nv2x::mmDF_PIE_AON_PerfMonCtrHi3 },
            { Nv2x::mmDF_PIE_AON_PerfMonCtlLo4, Nv2x::mmDF_PIE_AON_PerfMonCtlHi4, Nv2x::mmDF_PIE_AON_PerfMonCtrLo4, Nv2x::mmDF_PIE_AON_PerfMonCtrHi4 },
            { Nv2x::mmDF_PIE_AON_PerfMonCtlLo5, Nv2x::mmDF_PIE_AON_PerfMonCtlHi5, Nv2x::mmDF_PIE_AON_PerfMonCtrLo5, Nv2x::mmDF_PIE_AON_PerfMonCtrHi5 },
            { Nv2x::mmDF_PIE_AON_PerfMonCtlLo6, Nv2x::mmDF_PIE_AON_PerfMonCtlHi6, Nv2x::mmDF_PIE_AON_PerfMonCtrLo6, Nv2x::mmDF_PIE_AON_PerfMonCtrHi6 },
            { Nv2x::mmDF_PIE_AON_PerfMonCtlLo7, Nv2x::mmDF_PIE_AON_PerfMonCtlHi7, Nv2x::mmDF_PIE_AON_PerfMonCtrLo7, Nv2x::mmDF_PIE_AON_PerfMonCtrHi7 },
        }};
    }
}

// =====================================================================================================================
// Initializes each block's basic hardware-defined information
// (distribution, numScopedInstances, numGenericSpmModules, etc.)
static void Gfx11InitBasicBlockInfo(
    const Pal::Device& device,
    GpuChipProperties* pProps)
{
    PerfCounterInfo*const pInfo    = &pProps->gfx9.perfCounterInfo.gfx9Info;
    const MaxEventIds&    maxIds   = GetEventLimits(device);
    const GfxIpLevel      gfxip    = device.ChipProperties().gfxLevel;
    const uint32          rbPerSa  = pProps->gfx9.maxNumRbPerSe / pProps->gfx9.numShaderArrays;

    // Pull in the generic gfx10+ and gfx11 registers to make it easier to read the register tables.
    using namespace Gfx103Plus;
    using namespace Gfx103PlusExclusive;
    using namespace Gfx11;

    // Start by hard-coding hardware specific constants for each block. The shared blocks come first followed by
    // gfxip-specific blocks. Note that gfx10 removed or renamed a lot of blocks.
    //
    // The distribution and numScopedInstances (per-distribution) are derived from our hardware architecture.
    // The generic module counts are determined by:
    //   1. Does the block follow the generic programming model as defined by the perf experiment code?
    //   2. If so, there's one SPM module for each SELECT/SELECT1 pair and one legacy module for the remaining SELECTs.
    // The number of SPM wires is a hardware constant baked into each ASIC's design. So are the SPM block selects.
    // The maximum event IDs are the largest values from the hardware perf_sel enums.
    // Finally, we hard-code the PERFCOUNTER# register addresses for each module.

    PerfCounterBlockInfo*const pCpf = &pInfo->block[static_cast<uint32>(GpuBlock::Cpf)];
    pCpf->distribution              = PerfCounterDistribution::GlobalBlock;
    pCpf->numScopedInstances        = 1;
    pCpf->numGenericSpmModules      = 1; // CPF_PERFCOUNTER0
    pCpf->numGenericLegacyModules   = 1; // CPF_PERFCOUNTER1
    pCpf->numSpmWires               = 2;
    pCpf->spmBlockSelect            = Gfx10SpmGlobalBlockSelectCpf;
    pCpf->maxEventId                = maxIds[CpfPerfcountSelId];

    pCpf->regAddr = { 0, {
        { mmCPF_PERFCOUNTER0_SELECT, mmCPF_PERFCOUNTER0_SELECT1, mmCPF_PERFCOUNTER0_LO, mmCPF_PERFCOUNTER0_HI },
        { mmCPF_PERFCOUNTER1_SELECT, 0,                          mmCPF_PERFCOUNTER1_LO, mmCPF_PERFCOUNTER1_HI },
    }};

    PerfCounterBlockInfo*const pPa = &pInfo->block[static_cast<uint32>(GpuBlock::Pa)];
    pPa->distribution              = PerfCounterDistribution::PerShaderEngine;
    pPa->numScopedInstances        = 1;
    pPa->numGenericSpmModules      = 4; // PA_SU_PERFCOUNTER0-3
    pPa->numGenericLegacyModules   = 0;
    pPa->numSpmWires               = 8;
    pPa->spmBlockSelect            = Gfx10SpmSeBlockSelectPa;
    pPa->maxEventId                = maxIds[SuPerfcntSelId];

    pPa->regAddr = { 0, {
        { mmPA_SU_PERFCOUNTER0_SELECT, mmPA_SU_PERFCOUNTER0_SELECT1, mmPA_SU_PERFCOUNTER0_LO, mmPA_SU_PERFCOUNTER0_HI },
        { mmPA_SU_PERFCOUNTER1_SELECT, mmPA_SU_PERFCOUNTER1_SELECT1, mmPA_SU_PERFCOUNTER1_LO, mmPA_SU_PERFCOUNTER1_HI },
        { mmPA_SU_PERFCOUNTER2_SELECT, mmPA_SU_PERFCOUNTER2_SELECT1, mmPA_SU_PERFCOUNTER2_LO, mmPA_SU_PERFCOUNTER2_HI },
        { mmPA_SU_PERFCOUNTER3_SELECT, mmPA_SU_PERFCOUNTER3_SELECT1, mmPA_SU_PERFCOUNTER3_LO, mmPA_SU_PERFCOUNTER3_HI },
    }};

    PerfCounterBlockInfo* const pPc = &pInfo->block[static_cast<uint32>(GpuBlock::Pc)];
    pPc->distribution = PerfCounterDistribution::PerShaderEngine;
    pPc->numScopedInstances = 1;
    pPc->numGenericSpmModules = 4; // PC_PERFCOUNTER0-3
    pPc->numGenericLegacyModules = 0;
    pPc->numSpmWires = 8;
    pPc->spmBlockSelect = Gfx11SpmSeBlockSelectPc;
    pPc->maxEventId = maxIds[PcPerfcntSelId];

    pPc->regAddr = { 0, {
        { mmPC_PERFCOUNTER0_SELECT, mmPC_PERFCOUNTER0_SELECT1, mmPC_PERFCOUNTER0_LO, mmPC_PERFCOUNTER0_HI },
        { mmPC_PERFCOUNTER1_SELECT, mmPC_PERFCOUNTER1_SELECT1, mmPC_PERFCOUNTER1_LO, mmPC_PERFCOUNTER1_HI },
        { mmPC_PERFCOUNTER2_SELECT, mmPC_PERFCOUNTER2_SELECT1, mmPC_PERFCOUNTER2_LO, mmPC_PERFCOUNTER2_HI },
        { mmPC_PERFCOUNTER3_SELECT, mmPC_PERFCOUNTER3_SELECT1, mmPC_PERFCOUNTER3_LO, mmPC_PERFCOUNTER3_HI },
    } };

    // In gfx10 SC is subdivided into SCF (SCT) and 2xSCB per SA. The sets of perf counters (PA_SC_PERFCOUNTER{0-7})
    // are instantiated in each of the two SCBs. In the hardware docs these are called packers, thus we're really
    // gathering perf counters from individual packer instances.
    PerfCounterBlockInfo*const pSc = &pInfo->block[static_cast<uint32>(GpuBlock::Sc)];
    pSc->distribution              = PerfCounterDistribution::PerShaderArray;
    pSc->numScopedInstances        = 2;
    pSc->numGenericSpmModules      = 1; // PA_SC_PERFCOUNTER0
    pSc->numGenericLegacyModules   = 7; // PA_SC_PERFCOUNTER1-7
    pSc->numSpmWires               = 2;
    pSc->spmBlockSelect            = Gfx10SpmSeBlockSelectSc;
    pSc->maxEventId                = maxIds[ScPerfcntSelId];

    pSc->regAddr = { 0, {
        { mmPA_SC_PERFCOUNTER0_SELECT, mmPA_SC_PERFCOUNTER0_SELECT1, mmPA_SC_PERFCOUNTER0_LO, mmPA_SC_PERFCOUNTER0_HI },
        { mmPA_SC_PERFCOUNTER1_SELECT, 0,                            mmPA_SC_PERFCOUNTER1_LO, mmPA_SC_PERFCOUNTER1_HI },
        { mmPA_SC_PERFCOUNTER2_SELECT, 0,                            mmPA_SC_PERFCOUNTER2_LO, mmPA_SC_PERFCOUNTER2_HI },
        { mmPA_SC_PERFCOUNTER3_SELECT, 0,                            mmPA_SC_PERFCOUNTER3_LO, mmPA_SC_PERFCOUNTER3_HI },
        { mmPA_SC_PERFCOUNTER4_SELECT, 0,                            mmPA_SC_PERFCOUNTER4_LO, mmPA_SC_PERFCOUNTER4_HI },
        { mmPA_SC_PERFCOUNTER5_SELECT, 0,                            mmPA_SC_PERFCOUNTER5_LO, mmPA_SC_PERFCOUNTER5_HI },
        { mmPA_SC_PERFCOUNTER6_SELECT, 0,                            mmPA_SC_PERFCOUNTER6_LO, mmPA_SC_PERFCOUNTER6_HI },
        { mmPA_SC_PERFCOUNTER7_SELECT, 0,                            mmPA_SC_PERFCOUNTER7_LO, mmPA_SC_PERFCOUNTER7_HI },
    }};

    PerfCounterBlockInfo*const pSpi = &pInfo->block[static_cast<uint32>(GpuBlock::Spi)];
    pSpi->distribution              = PerfCounterDistribution::PerShaderEngine;
    pSpi->numScopedInstances        = 1;
    pSpi->numGenericSpmModules      = 4; // SPI_PERFCOUNTER0-3
    pSpi->numGenericLegacyModules   = 2; // SPI_PERFCOUNTER4-5
    pSpi->numSpmWires               = 8;
    pSpi->spmBlockSelect            = Gfx10SpmSeBlockSelectSpi;
    pSpi->maxEventId                = maxIds[SpiPerfcntSelId];

    pSpi->regAddr = { 0, {
        { mmSPI_PERFCOUNTER0_SELECT, mmSPI_PERFCOUNTER0_SELECT1, mmSPI_PERFCOUNTER0_LO, mmSPI_PERFCOUNTER0_HI },
        { mmSPI_PERFCOUNTER1_SELECT, mmSPI_PERFCOUNTER1_SELECT1, mmSPI_PERFCOUNTER1_LO, mmSPI_PERFCOUNTER1_HI },
        { mmSPI_PERFCOUNTER2_SELECT, mmSPI_PERFCOUNTER2_SELECT1, mmSPI_PERFCOUNTER2_LO, mmSPI_PERFCOUNTER2_HI },
        { mmSPI_PERFCOUNTER3_SELECT, mmSPI_PERFCOUNTER3_SELECT1, mmSPI_PERFCOUNTER3_LO, mmSPI_PERFCOUNTER3_HI },
        { mmSPI_PERFCOUNTER4_SELECT, 0,                          mmSPI_PERFCOUNTER4_LO, mmSPI_PERFCOUNTER4_HI },
        { mmSPI_PERFCOUNTER5_SELECT, 0,                          mmSPI_PERFCOUNTER5_LO, mmSPI_PERFCOUNTER5_HI },
    }};

    // There are changes to the SQ perf counters from previous chips, but basically it's been
    // reduced from 16 counters to 8 and the counters have been reduced to 32-bit counters.
    PerfCounterBlockInfo*const pSqWgp = &pInfo->block[static_cast<uint32>(GpuBlock::SqWgp)];
    pSqWgp->distribution              = PerfCounterDistribution::PerShaderArray;
    // maxNumWgpPerSa is the sum of gfx10.numWgpAboveSpi and gfx10.numWgpBelowSpi
    pSqWgp->numScopedInstances        = pProps->gfx9.gfx10.maxNumWgpPerSa;
    pSqWgp->num16BitSpmCounters       = 16;
    pSqWgp->num32BitSpmCounters       = 8;
    pSqWgp->numGlobalSharedCounters   = 8;
    pSqWgp->numGenericSpmModules      = 0;
    pSqWgp->numGenericLegacyModules   = 0;
    pSqWgp->numSpmWires               = 8;
    pSqWgp->spmBlockSelect            = Gfx11SpmSeBlockSelectSqc;
    pSqWgp->maxEventId                = maxIds[SqPerfSelId];

   // section 1.5
   // Legacy perfcounters use a pair of SPM counters,
   // so in legacy mode you can only use counters 0, 2, 4, 6, 8, 10, 12, 14.
    pSqWgp->regAddr = { 0, {
        { mmSQ_PERFCOUNTER0_SELECT,   0, mmSQ_PERFCOUNTER0_LO,  0 },
        { mmSQ_PERFCOUNTER1_SELECT,   0,                    0,  0 },
        { mmSQ_PERFCOUNTER2_SELECT,   0, mmSQ_PERFCOUNTER1_LO,  0 },
        { mmSQ_PERFCOUNTER3_SELECT,   0,                    0,  0 },
        { mmSQ_PERFCOUNTER4_SELECT,   0, mmSQ_PERFCOUNTER2_LO,  0 },
        { mmSQ_PERFCOUNTER5_SELECT,   0,                    0,  0 },
        { mmSQ_PERFCOUNTER6_SELECT,   0, mmSQ_PERFCOUNTER3_LO,  0 },
        { mmSQ_PERFCOUNTER7_SELECT,   0,                    0,  0 },
        { mmSQ_PERFCOUNTER8_SELECT,   0, mmSQ_PERFCOUNTER4_LO,  0 },
        { mmSQ_PERFCOUNTER9_SELECT,   0,                    0,  0 },
        { mmSQ_PERFCOUNTER10_SELECT,  0, mmSQ_PERFCOUNTER5_LO,  0 },
        { mmSQ_PERFCOUNTER11_SELECT,  0,                    0,  0 },
        { mmSQ_PERFCOUNTER12_SELECT,  0, mmSQ_PERFCOUNTER6_LO,  0 },
        { mmSQ_PERFCOUNTER13_SELECT,  0,                    0,  0 },
        { mmSQ_PERFCOUNTER14_SELECT,  0, mmSQ_PERFCOUNTER7_LO,  0 },
        { mmSQ_PERFCOUNTER15_SELECT,  0,                    0,  0 },
    } };

    PerfCounterBlockInfo* const pSq = &pInfo->block[static_cast<uint32>(GpuBlock::Sq)];
    pSq->distribution            = PerfCounterDistribution::PerShaderEngine;
    pSq->numScopedInstances      = 1;
    pSq->num16BitSpmCounters     = 8;
    pSq->num32BitSpmCounters     = 8;
    pSq->numGlobalSharedCounters = 8;
    pSq->numGenericSpmModules    = 0;
    pSq->numGenericLegacyModules = 0;
    pSq->numSpmWires             = 8;
    pSq->spmBlockSelect          = Gfx10SpmSeBlockSelectSqg;
    pSq->maxEventId              = maxIds[SqgPerfSelId];

    pSq->regAddr = { 0, {
        { mmSQG_PERFCOUNTER0_SELECT, 0, mmSQG_PERFCOUNTER0_LO, mmSQG_PERFCOUNTER0_HI },
        { mmSQG_PERFCOUNTER1_SELECT, 0, mmSQG_PERFCOUNTER1_LO, mmSQG_PERFCOUNTER1_HI },
        { mmSQG_PERFCOUNTER2_SELECT, 0, mmSQG_PERFCOUNTER2_LO, mmSQG_PERFCOUNTER2_HI },
        { mmSQG_PERFCOUNTER3_SELECT, 0, mmSQG_PERFCOUNTER3_LO, mmSQG_PERFCOUNTER3_HI },
        { mmSQG_PERFCOUNTER4_SELECT, 0, mmSQG_PERFCOUNTER4_LO, mmSQG_PERFCOUNTER4_HI },
        { mmSQG_PERFCOUNTER5_SELECT, 0, mmSQG_PERFCOUNTER5_LO, mmSQG_PERFCOUNTER5_HI },
        { mmSQG_PERFCOUNTER6_SELECT, 0, mmSQG_PERFCOUNTER6_LO, mmSQG_PERFCOUNTER6_HI },
        { mmSQG_PERFCOUNTER7_SELECT, 0, mmSQG_PERFCOUNTER7_LO, mmSQG_PERFCOUNTER7_HI },
    } };

    // The SX not a single block and thus has per-SE and per-SA qualities. For example, the SX crossbar routes requests
    // between SAs so it lives in the SE. However, the "interesting bits" of the SX are split in half, one half in
    // each SA. Perfcounter requests are forwarded to one half of the SX using the SA index so for us it's per-SA.
    PerfCounterBlockInfo*const pSx = &pInfo->block[static_cast<uint32>(GpuBlock::Sx)];
    pSx->distribution              = PerfCounterDistribution::PerShaderArray;
    pSx->numScopedInstances        = 1;
    pSx->numGenericSpmModules      = 2; // SX_PERFCOUNTER0-1
    pSx->numGenericLegacyModules   = 2; // SX_PERFCOUNTER2-3
    pSx->numSpmWires               = 4;
    pSx->spmBlockSelect            = Gfx10SpmSeBlockSelectSx;
    pSx->maxEventId                = maxIds[SxPerfcounterValsId];

    pSx->regAddr = { 0, {
        { mmSX_PERFCOUNTER0_SELECT, mmSX_PERFCOUNTER0_SELECT1, mmSX_PERFCOUNTER0_LO, mmSX_PERFCOUNTER0_HI },
        { mmSX_PERFCOUNTER1_SELECT, mmSX_PERFCOUNTER1_SELECT1, mmSX_PERFCOUNTER1_LO, mmSX_PERFCOUNTER1_HI },
        { mmSX_PERFCOUNTER2_SELECT, 0,                         mmSX_PERFCOUNTER2_LO, mmSX_PERFCOUNTER2_HI },
        { mmSX_PERFCOUNTER3_SELECT, 0,                         mmSX_PERFCOUNTER3_LO, mmSX_PERFCOUNTER3_HI },
    }};

    PerfCounterBlockInfo*const pTa = &pInfo->block[static_cast<uint32>(GpuBlock::Ta)];
    pTa->distribution              = PerfCounterDistribution::PerShaderArray;
    pTa->numScopedInstances        = pProps->gfx9.numCuPerSh;
    pTa->numGenericSpmModules      = 1; // TA_PERFCOUNTER0
    pTa->numGenericLegacyModules   = 1; // TA_PERFCOUNTER1
    pTa->numSpmWires               = 2;
    pTa->spmBlockSelect            = Gfx10SpmSeBlockSelectTa;
    pTa->maxEventId                = maxIds[TaPerfcountSelId];

    pTa->regAddr = { 0, {
        { mmTA_PERFCOUNTER0_SELECT, mmTA_PERFCOUNTER0_SELECT1, mmTA_PERFCOUNTER0_LO, mmTA_PERFCOUNTER0_HI },
        { mmTA_PERFCOUNTER1_SELECT, 0,                         mmTA_PERFCOUNTER1_LO, mmTA_PERFCOUNTER1_HI },
    }};

    PerfCounterBlockInfo*const pTd = &pInfo->block[static_cast<uint32>(GpuBlock::Td)];
    pTd->distribution              = PerfCounterDistribution::PerShaderArray;
    pTd->numScopedInstances        = pProps->gfx9.numCuPerSh;
    pTd->numGenericSpmModules      = 1; // TD_PERFCOUNTER0
    pTd->numGenericLegacyModules   = 1; // TD_PERFCOUNTER1
    pTd->numSpmWires               = 2;
    pTd->spmBlockSelect            = Gfx10SpmSeBlockSelectTd;
    pTd->maxEventId                = maxIds[TdPerfcountSelId];

    pTd->regAddr = { 0, {
        { mmTD_PERFCOUNTER0_SELECT, mmTD_PERFCOUNTER0_SELECT1, mmTD_PERFCOUNTER0_LO, mmTD_PERFCOUNTER0_HI },
        { mmTD_PERFCOUNTER1_SELECT, 0,                         mmTD_PERFCOUNTER1_LO, mmTD_PERFCOUNTER1_HI },
    }};

    PerfCounterBlockInfo*const pTcp = &pInfo->block[static_cast<uint32>(GpuBlock::Tcp)];
    pTcp->distribution              = PerfCounterDistribution::PerShaderArray;
    pTcp->numScopedInstances        = pProps->gfx9.gfx10.numTcpPerSa;
    pTcp->numGenericSpmModules      = 2; // TCP_PERFCOUNTER0-1
    pTcp->numGenericLegacyModules   = 2; // TCP_PERFCOUNTER2-3
    pTcp->numSpmWires               = 4;
    pTcp->spmBlockSelect            = Gfx10SpmSeBlockSelectTcp;
    pTcp->maxEventId                = maxIds[TcpPerfcountSelectId];

    pTcp->regAddr = { 0, {
        { mmTCP_PERFCOUNTER0_SELECT, mmTCP_PERFCOUNTER0_SELECT1, mmTCP_PERFCOUNTER0_LO, mmTCP_PERFCOUNTER0_HI },
        { mmTCP_PERFCOUNTER1_SELECT, mmTCP_PERFCOUNTER1_SELECT1, mmTCP_PERFCOUNTER1_LO, mmTCP_PERFCOUNTER1_HI },
        { mmTCP_PERFCOUNTER2_SELECT, 0,                          mmTCP_PERFCOUNTER2_LO, mmTCP_PERFCOUNTER2_HI },
        { mmTCP_PERFCOUNTER3_SELECT, 0,                          mmTCP_PERFCOUNTER3_LO, mmTCP_PERFCOUNTER3_HI },
    }};

    PerfCounterBlockInfo*const pDb = &pInfo->block[static_cast<uint32>(GpuBlock::Db)];
    pDb->distribution              = PerfCounterDistribution::PerShaderArray;
    pDb->numScopedInstances        = rbPerSa;
    pDb->numGenericSpmModules      = 2; // DB_PERFCOUNTER0-1
    pDb->numGenericLegacyModules   = 2; // DB_PERFCOUNTER2-3
    pDb->numSpmWires               = 4;
    pDb->spmBlockSelect            = Gfx10SpmSeBlockSelectDb;
    pDb->maxEventId                = maxIds[PerfcounterValsId];

    pDb->regAddr = { 0, {
        { mmDB_PERFCOUNTER0_SELECT, mmDB_PERFCOUNTER0_SELECT1, mmDB_PERFCOUNTER0_LO, mmDB_PERFCOUNTER0_HI },
        { mmDB_PERFCOUNTER1_SELECT, mmDB_PERFCOUNTER1_SELECT1, mmDB_PERFCOUNTER1_LO, mmDB_PERFCOUNTER1_HI },
        { mmDB_PERFCOUNTER2_SELECT, 0,                         mmDB_PERFCOUNTER2_LO, mmDB_PERFCOUNTER2_HI },
        { mmDB_PERFCOUNTER3_SELECT, 0,                         mmDB_PERFCOUNTER3_LO, mmDB_PERFCOUNTER3_HI },
    }};

    PerfCounterBlockInfo*const pCb = &pInfo->block[static_cast<uint32>(GpuBlock::Cb)];
    pCb->distribution              = PerfCounterDistribution::PerShaderArray;
    pCb->numScopedInstances        = rbPerSa;
    pCb->numGenericSpmModules      = 1; // CB_PERFCOUNTER0
    pCb->numGenericLegacyModules   = 3; // CB_PERFCOUNTER1-3
    pCb->numSpmWires               = 2;
    pCb->spmBlockSelect            = Gfx10SpmSeBlockSelectCb;
    pCb->maxEventId                = maxIds[CBPerfSelId];

    pCb->regAddr = { 0, {
        { mmCB_PERFCOUNTER0_SELECT, mmCB_PERFCOUNTER0_SELECT1, mmCB_PERFCOUNTER0_LO, mmCB_PERFCOUNTER0_HI },
        { mmCB_PERFCOUNTER1_SELECT, 0,                         mmCB_PERFCOUNTER1_LO, mmCB_PERFCOUNTER1_HI },
        { mmCB_PERFCOUNTER2_SELECT, 0,                         mmCB_PERFCOUNTER2_LO, mmCB_PERFCOUNTER2_HI },
        { mmCB_PERFCOUNTER3_SELECT, 0,                         mmCB_PERFCOUNTER3_LO, mmCB_PERFCOUNTER3_HI },
    }};

    PerfCounterBlockInfo*const pGds = &pInfo->block[static_cast<uint32>(GpuBlock::Gds)];
    pGds->distribution              = PerfCounterDistribution::GlobalBlock;
    pGds->numScopedInstances        = 1;
    pGds->spmBlockSelect            = Gfx10SpmGlobalBlockSelectGds;
    pGds->maxEventId                = maxIds[GdsPerfcountSelectId];

    // Sets the register addresses and configures the perf modules and SPM wires.
    Gfx10PlusUpdateGdsBlockInfo(device, pGds);

    PerfCounterBlockInfo*const pGrbm = &pInfo->block[static_cast<uint32>(GpuBlock::Grbm)];
    pGrbm->distribution              = PerfCounterDistribution::GlobalBlock;
    pGrbm->numScopedInstances        = 1;
    pGrbm->numGenericSpmModules      = 0;
    pGrbm->numGenericLegacyModules   = 2; // GRBM_PERFCOUNTER0-1
    pGrbm->numSpmWires               = 0;
    pGrbm->maxEventId                = MaxGrbmPerfSelGfx11;

    pGrbm->regAddr = { 0, {
        { mmGRBM_PERFCOUNTER0_SELECT, 0, mmGRBM_PERFCOUNTER0_LO, mmGRBM_PERFCOUNTER0_HI },
        { mmGRBM_PERFCOUNTER1_SELECT, 0, mmGRBM_PERFCOUNTER1_LO, mmGRBM_PERFCOUNTER1_HI },
    }};

    // These counters are a bit special. The GRBM is a global block but it defines one special counter per SE. We
    // abstract this as a special Grbm(per)Se block which needs special handling in the perf experiment.
    PerfCounterBlockInfo*const pGrbmSe = &pInfo->block[static_cast<uint32>(GpuBlock::GrbmSe)];
    pGrbmSe->distribution              = PerfCounterDistribution::PerShaderEngine;
    pGrbmSe->numScopedInstances        = 1;
    pGrbmSe->numGlobalOnlyCounters     = 1;
    pGrbmSe->numGenericSpmModules      = 0;
    pGrbmSe->numGenericLegacyModules   = 0;
    pGrbmSe->numSpmWires               = 0;
    pGrbmSe->maxEventId                = MaxGrbmSe0PerfSelGfx11;

    // By convention we access the counter register address array using the SE index.
    if (IsNavi31(device))
    {
        pGrbmSe->regAddr = { 0, {
            { mmGRBM_SE0_PERFCOUNTER_SELECT,       0, mmGRBM_SE0_PERFCOUNTER_LO,       mmGRBM_SE0_PERFCOUNTER_HI       },
            { mmGRBM_SE1_PERFCOUNTER_SELECT,       0, mmGRBM_SE1_PERFCOUNTER_LO,       mmGRBM_SE1_PERFCOUNTER_HI       },
            { Nv3x::mmGRBM_SE2_PERFCOUNTER_SELECT, 0, Nv3x::mmGRBM_SE2_PERFCOUNTER_LO, Nv3x::mmGRBM_SE2_PERFCOUNTER_HI },
            { Nv31::mmGRBM_SE3_PERFCOUNTER_SELECT, 0, Nv31::mmGRBM_SE3_PERFCOUNTER_LO, Nv31::mmGRBM_SE3_PERFCOUNTER_HI },
        }};
    }
    else if (IsNavi3x(device))
    {
        pGrbmSe->regAddr = { 0, {
            { mmGRBM_SE0_PERFCOUNTER_SELECT,       0, mmGRBM_SE0_PERFCOUNTER_LO,       mmGRBM_SE0_PERFCOUNTER_HI       },
            { mmGRBM_SE1_PERFCOUNTER_SELECT,       0, mmGRBM_SE1_PERFCOUNTER_LO,       mmGRBM_SE1_PERFCOUNTER_HI       },
            { Nv3x::mmGRBM_SE2_PERFCOUNTER_SELECT, 0, Nv3x::mmGRBM_SE2_PERFCOUNTER_LO, Nv3x::mmGRBM_SE2_PERFCOUNTER_HI },
        }};
    }
    else
    {
        pGrbmSe->regAddr = { 0, {
            { mmGRBM_SE0_PERFCOUNTER_SELECT, 0, mmGRBM_SE0_PERFCOUNTER_LO, mmGRBM_SE0_PERFCOUNTER_HI },
            { mmGRBM_SE1_PERFCOUNTER_SELECT, 0, mmGRBM_SE1_PERFCOUNTER_LO, mmGRBM_SE1_PERFCOUNTER_HI },
        }};
    }

    // The RLC's SELECT registers are non-standard because they lack PERF_MODE fields. This should be fine though
    // because we only use PERFMON_COUNTER_MODE_ACCUM which is zero. If we ever try to use a different mode the RLC
    // needs to be handled as a special case.
    static_assert(PERFMON_COUNTER_MODE_ACCUM == 0, "RLC legacy counters need special handling.");

    PerfCounterBlockInfo*const pRlc = &pInfo->block[static_cast<uint32>(GpuBlock::Rlc)];
    pRlc->distribution              = PerfCounterDistribution::GlobalBlock;
    pRlc->numScopedInstances        = 1;
    pRlc->numGenericSpmModules      = 0;
    pRlc->numGenericLegacyModules   = 2; // RLC_PERFCOUNTER0-1
    pRlc->numSpmWires               = 0;
    pRlc->maxEventId                = 6; // SERDES command write;

    pRlc->regAddr = { 0, {
        { mmRLC_PERFCOUNTER0_SELECT, 0, mmRLC_PERFCOUNTER0_LO, mmRLC_PERFCOUNTER0_HI },
        { mmRLC_PERFCOUNTER1_SELECT, 0, mmRLC_PERFCOUNTER1_LO, mmRLC_PERFCOUNTER1_HI },
    }};

    PerfCounterBlockInfo*const pDma = &pInfo->block[static_cast<uint32>(GpuBlock::Dma)];
    pDma->distribution              = PerfCounterDistribution::GlobalBlock;
    pDma->numScopedInstances        = device.EngineProperties().perEngine[EngineTypeDma].numAvailable;
    pDma->numGenericSpmModules      = 2; // SDMA#_PERFCOUNTER0-1
    pDma->numGenericLegacyModules   = 0;
    pDma->numSpmWires               = 4;
    pDma->spmBlockSelect            = Gfx10SpmGlobalBlockSelectSdma;
    pDma->maxEventId                = maxIds[SdmaPerfmonSelId];
    pDma->numScopedInstances        = Min(pDma->numScopedInstances, Gfx9MaxSdmaInstances);

    pInfo->sdmaRegAddr[0][0] = { mmSDMA0_PERFCOUNTER0_SELECT, mmSDMA0_PERFCOUNTER0_SELECT1,
                                 mmSDMA0_PERFCOUNTER0_LO,     mmSDMA0_PERFCOUNTER0_HI };
    pInfo->sdmaRegAddr[0][1] = { mmSDMA0_PERFCOUNTER1_SELECT, mmSDMA0_PERFCOUNTER1_SELECT1,
                                 mmSDMA0_PERFCOUNTER1_LO,     mmSDMA0_PERFCOUNTER1_HI };

    if (IsNavi3x(device))
    {
        pInfo->sdmaRegAddr[1][0] = { Nv3x::mmSDMA1_PERFCOUNTER0_SELECT, Nv3x::mmSDMA1_PERFCOUNTER0_SELECT1,
                                     Nv3x::mmSDMA1_PERFCOUNTER0_LO,     Nv3x::mmSDMA1_PERFCOUNTER0_HI };
        pInfo->sdmaRegAddr[1][1] = { Nv3x::mmSDMA1_PERFCOUNTER1_SELECT, Nv3x::mmSDMA1_PERFCOUNTER1_SELECT1,
                                     Nv3x::mmSDMA1_PERFCOUNTER1_LO,     Nv3x::mmSDMA1_PERFCOUNTER1_HI };
    }

    PerfCounterBlockInfo*const pCpg = &pInfo->block[static_cast<uint32>(GpuBlock::Cpg)];
    pCpg->distribution              = PerfCounterDistribution::GlobalBlock;
    pCpg->numScopedInstances        = 1;
    pCpg->numGenericSpmModules      = 1; // CPG_PERFCOUNTER0
    pCpg->numGenericLegacyModules   = 1; // CPG_PERFCOUNTER1
    pCpg->numSpmWires               = 2;
    pCpg->spmBlockSelect            = Gfx10SpmGlobalBlockSelectCpg;
    pCpg->maxEventId                = maxIds[CpgPerfcountSelId];

    pCpg->regAddr = { 0, {
        { mmCPG_PERFCOUNTER0_SELECT, mmCPG_PERFCOUNTER0_SELECT1, mmCPG_PERFCOUNTER0_LO, mmCPG_PERFCOUNTER0_HI },
        { mmCPG_PERFCOUNTER1_SELECT, 0,                          mmCPG_PERFCOUNTER1_LO, mmCPG_PERFCOUNTER1_HI },
    }};

    PerfCounterBlockInfo*const pCpc = &pInfo->block[static_cast<uint32>(GpuBlock::Cpc)];
    pCpc->distribution              = PerfCounterDistribution::GlobalBlock;
    pCpc->numScopedInstances        = 1;
    pCpc->numGenericSpmModules      = 1; // CPC_PERFCOUNTER0
    pCpc->numGenericLegacyModules   = 1; // CPC_PERFCOUNTER1
    pCpc->numSpmWires               = 2;
    pCpc->spmBlockSelect            = Gfx10SpmGlobalBlockSelectCpc;
    pCpc->maxEventId                = maxIds[CpcPerfcountSelId];

    pCpc->regAddr = { 0, {
        { mmCPC_PERFCOUNTER0_SELECT, mmCPC_PERFCOUNTER0_SELECT1, mmCPC_PERFCOUNTER0_LO, mmCPC_PERFCOUNTER0_HI },
        { mmCPC_PERFCOUNTER1_SELECT, 0,                          mmCPC_PERFCOUNTER1_LO, mmCPC_PERFCOUNTER1_HI },
    }};

    // Also called the UTCL2.
    PerfCounterBlockInfo*const pMcVmL2 = &pInfo->block[static_cast<uint32>(GpuBlock::McVmL2)];
    pMcVmL2->distribution              = PerfCounterDistribution::GlobalBlock;
    pMcVmL2->numScopedInstances        = 1;
    pMcVmL2->numGenericSpmModules      = 2; // GCVML2_PERFCOUNTER2_0-1
    pMcVmL2->numGenericLegacyModules   = 8; // GCMC_VM_L2_PERFCOUNTER0-7
    pMcVmL2->numSpmWires               = 4;
    pMcVmL2->spmBlockSelect            = Gfx10SpmGlobalBlockSelectGpuvmVml2;
    pMcVmL2->maxEventId                = 20; // Number of l2 cache invalidations
    pMcVmL2->isCfgStyle                = true;

    pMcVmL2->regAddr = { mmGCMC_VM_L2_PERFCOUNTER_RSLT_CNTL, {
        { mmGCMC_VM_L2_PERFCOUNTER0_CFG,  0,                               mmGCMC_VM_L2_PERFCOUNTER_LO, mmGCMC_VM_L2_PERFCOUNTER_HI },
        { mmGCMC_VM_L2_PERFCOUNTER1_CFG,  0,                               mmGCMC_VM_L2_PERFCOUNTER_LO, mmGCMC_VM_L2_PERFCOUNTER_HI },
        { mmGCMC_VM_L2_PERFCOUNTER2_CFG,  0,                               mmGCMC_VM_L2_PERFCOUNTER_LO, mmGCMC_VM_L2_PERFCOUNTER_HI },
        { mmGCMC_VM_L2_PERFCOUNTER3_CFG,  0,                               mmGCMC_VM_L2_PERFCOUNTER_LO, mmGCMC_VM_L2_PERFCOUNTER_HI },
        { mmGCMC_VM_L2_PERFCOUNTER4_CFG,  0,                               mmGCMC_VM_L2_PERFCOUNTER_LO, mmGCMC_VM_L2_PERFCOUNTER_HI },
        { mmGCMC_VM_L2_PERFCOUNTER5_CFG,  0,                               mmGCMC_VM_L2_PERFCOUNTER_LO, mmGCMC_VM_L2_PERFCOUNTER_HI },
        { mmGCMC_VM_L2_PERFCOUNTER6_CFG,  0,                               mmGCMC_VM_L2_PERFCOUNTER_LO, mmGCMC_VM_L2_PERFCOUNTER_HI },
        { mmGCMC_VM_L2_PERFCOUNTER7_CFG,  0,                               mmGCMC_VM_L2_PERFCOUNTER_LO, mmGCMC_VM_L2_PERFCOUNTER_HI },
        { mmGCVML2_PERFCOUNTER2_0_SELECT, mmGCVML2_PERFCOUNTER2_0_SELECT1, mmGCVML2_PERFCOUNTER2_0_LO,  mmGCVML2_PERFCOUNTER2_0_HI  },
        { mmGCVML2_PERFCOUNTER2_1_SELECT, mmGCVML2_PERFCOUNTER2_1_SELECT1, mmGCVML2_PERFCOUNTER2_1_LO,  mmGCVML2_PERFCOUNTER2_1_HI  }
    }};

    // We should have one EA for each SDP.
    PerfCounterBlockInfo*const pEa = &pInfo->block[static_cast<uint32>(GpuBlock::Ea)];
    pEa->distribution              = PerfCounterDistribution::GlobalBlock;
    pEa->numScopedInstances        = pProps->gfx9.numSdpInterfaces;
    pEa->numGenericSpmModules      = 1;  // EA_PERFCOUNTER2
    pEa->numGenericLegacyModules   = 2;  // EA_PERFCOUNTER0-1
    pEa->numSpmWires               = 2;
    pEa->spmBlockSelect            = Gfx10SpmGlobalBlockSelectEa;
    pEa->maxEventId                = 86; // | wgmi | (burst_length) & (4(reqeob )) | Request chains per burst length
    pEa->isCfgStyle                = true;

    pEa->regAddr = { mmGCEA_PERFCOUNTER_RSLT_CNTL, {
        { mmGCEA_PERFCOUNTER0_CFG,    0,                           mmGCEA_PERFCOUNTER_LO,  mmGCEA_PERFCOUNTER_HI  },
        { mmGCEA_PERFCOUNTER1_CFG,    0,                           mmGCEA_PERFCOUNTER_LO,  mmGCEA_PERFCOUNTER_HI  },
        { mmGCEA_PERFCOUNTER2_SELECT, mmGCEA_PERFCOUNTER2_SELECT1, mmGCEA_PERFCOUNTER2_LO, mmGCEA_PERFCOUNTER2_HI },
    }};

    PerfCounterBlockInfo*const pRpb = &pInfo->block[static_cast<uint32>(GpuBlock::Rpb)];
    pRpb->distribution              = PerfCounterDistribution::GlobalBlock;
    pRpb->numScopedInstances        = 1;
    pRpb->numGenericSpmModules      = 0;
    pRpb->numGenericLegacyModules   = 4; // RPB_PERFCOUNTER0-3
    pRpb->numSpmWires               = 0;
    pRpb->maxEventId                = 63;
    pRpb->isCfgStyle                = true;

    // This just sets up the perf counter register addresses.
    Gfx11UpdateRpbBlockInfo(device, pRpb);

    // The RMI changes the way you use its perfcounters very frequently, see below for more info.
    PerfCounterBlockInfo*const pRmi = &pInfo->block[static_cast<uint32>(GpuBlock::Rmi)];
    pRmi->distribution              = PerfCounterDistribution::PerShaderArray;
    pRmi->spmBlockSelect            = Gfx10SpmSeBlockSelectRmi;
    pRmi->maxEventId                = maxIds[RMIPerfSelId];

    // In gfx10.2+ there is only one RMI per RB. The RMI_PERFCOUNTER0/1 is only for read pipeline related counters
    // and RMI_PERCOUNTER2/3 is for write pipeline related counters. So in RMI_PERFCOUNTER0/1 all write related
    // counters are deprecated and in RMI_PERCOUNTER2/3 all read related counters are deprecated.
    //
    // This doesn't fit into our abstraction and is now the third model for how RMI perfcounters work. Rather than
    // codify this in an offical interface, which will probably become invalid in the future, we expose the read
    // and write counters and independent virtual sub-instances (even = read, odd = write). The user just has to
    // know to only program read events on the even instances and write events on the odd instances.
    pRmi->numScopedInstances        = rbPerSa * Gfx10NumRmiSubInstances;
    pRmi->numGenericSpmModules      = 1; // RMI_PERFCOUNTER0 or RMI_PERFCOUNTER2
    pRmi->numGenericLegacyModules   = 1; // RMI_PERFCOUNTER1 or RMI_PERFCOUNTER3
    pRmi->numSpmWires               = 4 / Gfx10NumRmiSubInstances; // We split these between our sub-instances.

    pRmi->regAddr = { 0, {
            { mmRMI_PERFCOUNTER0_SELECT, mmRMI_PERFCOUNTER0_SELECT1, mmRMI_PERFCOUNTER0_LO, mmRMI_PERFCOUNTER0_HI },
            { mmRMI_PERFCOUNTER1_SELECT, 0,                          mmRMI_PERFCOUNTER1_LO, mmRMI_PERFCOUNTER1_HI },
            { mmRMI_PERFCOUNTER2_SELECT, mmRMI_PERFCOUNTER2_SELECT1, mmRMI_PERFCOUNTER2_LO, mmRMI_PERFCOUNTER2_HI },
            { mmRMI_PERFCOUNTER3_SELECT, 0,                          mmRMI_PERFCOUNTER3_LO, mmRMI_PERFCOUNTER3_HI },
    }};

    // Gfx10.3 split the GE perf counters into three independent units: GE1, GE2_DIST, and GE2_SE.
    PerfCounterBlockInfo*const pGe = &pInfo->block[static_cast<uint32>(GpuBlock::Ge)];
    pGe->distribution              = PerfCounterDistribution::GlobalBlock;
    pGe->numScopedInstances        = 1;
    pGe->numGenericSpmModules      = 4; // GE1_PERFCOUNTER0-3
    pGe->numGenericLegacyModules   = 0;
    pGe->numSpmWires               = 8;
    pGe->spmBlockSelect            = Gfx10SpmGlobalBlockSelectGe1;
    pGe->maxEventId                = maxIds[Ge1PerfcountSelectId];

    pGe->regAddr = { 0, {
        { mmGE1_PERFCOUNTER0_SELECT, mmGE1_PERFCOUNTER0_SELECT1, mmGE1_PERFCOUNTER0_LO, mmGE1_PERFCOUNTER0_HI },
        { mmGE1_PERFCOUNTER1_SELECT, mmGE1_PERFCOUNTER1_SELECT1, mmGE1_PERFCOUNTER1_LO, mmGE1_PERFCOUNTER1_HI },
        { mmGE1_PERFCOUNTER2_SELECT, mmGE1_PERFCOUNTER2_SELECT1, mmGE1_PERFCOUNTER2_LO, mmGE1_PERFCOUNTER2_HI },
        { mmGE1_PERFCOUNTER3_SELECT, mmGE1_PERFCOUNTER3_SELECT1, mmGE1_PERFCOUNTER3_LO, mmGE1_PERFCOUNTER3_HI },
    }};

    PerfCounterBlockInfo*const pGeDist = &pInfo->block[static_cast<uint32>(GpuBlock::GeDist)];
    pGeDist->distribution              = PerfCounterDistribution::GlobalBlock;
    pGeDist->numScopedInstances        = 1;
    pGeDist->numGenericSpmModules      = 4; // GE2_DIST_PERFCOUNTER0-3
    pGeDist->numGenericLegacyModules   = 0;
    pGeDist->numSpmWires               = 8;
    pGeDist->spmBlockSelect            = Gfx10SpmGlobalBlockSelectGe2Dist;
    pGeDist->maxEventId                = maxIds[Ge2DistPerfcountSelectId];

    pGeDist->regAddr = { 0, {
        { mmGE2_DIST_PERFCOUNTER0_SELECT, mmGE2_DIST_PERFCOUNTER0_SELECT1, mmGE2_DIST_PERFCOUNTER0_LO, mmGE2_DIST_PERFCOUNTER0_HI },
        { mmGE2_DIST_PERFCOUNTER1_SELECT, mmGE2_DIST_PERFCOUNTER1_SELECT1, mmGE2_DIST_PERFCOUNTER1_LO, mmGE2_DIST_PERFCOUNTER1_HI },
        { mmGE2_DIST_PERFCOUNTER2_SELECT, mmGE2_DIST_PERFCOUNTER2_SELECT1, mmGE2_DIST_PERFCOUNTER2_LO, mmGE2_DIST_PERFCOUNTER2_HI },
        { mmGE2_DIST_PERFCOUNTER3_SELECT, mmGE2_DIST_PERFCOUNTER3_SELECT1, mmGE2_DIST_PERFCOUNTER3_LO, mmGE2_DIST_PERFCOUNTER3_HI },
    }};

    // Yes, it is correct that this block is programmed per-SE but has a global SPM select. It is technically not
    // a real per-SE block because it doesn't live in the SEs but the global hardware still creates one GE2_SE
    // instance for each SE. It listens to GRBM_GFX_INDEX like a per-SE block but the SPM wires correctly hook
    // into the global mux interface.
    PerfCounterBlockInfo*const pGeSe = &pInfo->block[static_cast<uint32>(GpuBlock::GeSe)];
    pGeSe->distribution              = PerfCounterDistribution::PerShaderEngine;
    pGeSe->numScopedInstances        = 1;
    pGeSe->numGenericSpmModules      = 4; // GE2_SE_PERFCOUNTER0-3
    pGeSe->numGenericLegacyModules   = 0;
    pGeSe->numSpmWires               = 8;
    pGeSe->spmBlockSelect            = Gfx10SpmGlobalBlockSelectGe2Se;
    pGeSe->maxEventId                = maxIds[Ge2SePerfcountSelectId];

    pGeSe->regAddr = { 0, {
        { mmGE2_SE_PERFCOUNTER0_SELECT, mmGE2_SE_PERFCOUNTER0_SELECT1, mmGE2_SE_PERFCOUNTER0_LO, mmGE2_SE_PERFCOUNTER0_HI },
        { mmGE2_SE_PERFCOUNTER1_SELECT, mmGE2_SE_PERFCOUNTER1_SELECT1, mmGE2_SE_PERFCOUNTER1_LO, mmGE2_SE_PERFCOUNTER1_HI },
        { mmGE2_SE_PERFCOUNTER2_SELECT, mmGE2_SE_PERFCOUNTER2_SELECT1, mmGE2_SE_PERFCOUNTER2_LO, mmGE2_SE_PERFCOUNTER2_HI },
        { mmGE2_SE_PERFCOUNTER3_SELECT, mmGE2_SE_PERFCOUNTER3_SELECT1, mmGE2_SE_PERFCOUNTER3_LO, mmGE2_SE_PERFCOUNTER3_HI },
    }};

    //
    // The GL1 arbiter. The GL1 complex is per-SA by definition.
    PerfCounterBlockInfo*const pGl1a = &pInfo->block[static_cast<uint32>(GpuBlock::Gl1a)];
    pGl1a->distribution              = PerfCounterDistribution::PerShaderArray;
    pGl1a->numScopedInstances        = 1;
    pGl1a->numGenericSpmModules      = 1; // GL1A_PERFCOUNTER0
    pGl1a->numGenericLegacyModules   = 3; // GL1A_PERFCOUNTER1-3
    pGl1a->numSpmWires               = 2;
    pGl1a->spmBlockSelect            = Gfx10SpmSeBlockSelectGl1a;
    pGl1a->maxEventId                = maxIds[Gl1aPerfSelId];

    pGl1a->regAddr = { 0, {
        { mmGL1A_PERFCOUNTER0_SELECT, mmGL1A_PERFCOUNTER0_SELECT1, mmGL1A_PERFCOUNTER0_LO, mmGL1A_PERFCOUNTER0_HI },
        { mmGL1A_PERFCOUNTER1_SELECT, 0,                           mmGL1A_PERFCOUNTER1_LO, mmGL1A_PERFCOUNTER1_HI },
        { mmGL1A_PERFCOUNTER2_SELECT, 0,                           mmGL1A_PERFCOUNTER2_LO, mmGL1A_PERFCOUNTER2_HI },
        { mmGL1A_PERFCOUNTER3_SELECT, 0,                           mmGL1A_PERFCOUNTER3_LO, mmGL1A_PERFCOUNTER3_HI },
    }};

    // The GL1 cache.
    PerfCounterBlockInfo*const pGl1c = &pInfo->block[static_cast<uint32>(GpuBlock::Gl1c)];
    pGl1c->distribution              = PerfCounterDistribution::PerShaderArray;
    pGl1c->numScopedInstances        = 4; // Each GL1A talks to four GL1C quadrants.
    pGl1c->numGenericSpmModules      = 1; // GL1C_PERFCOUNTER0
    pGl1c->numGenericLegacyModules   = 3; // GL1C_PERFCOUNTER1-3
    pGl1c->numSpmWires               = 2;
    pGl1c->spmBlockSelect            = Gfx10SpmSeBlockSelectGl1c;
    pGl1c->maxEventId                = maxIds[Gl1cPerfSelId];

    pGl1c->regAddr = { 0, {
        { mmGL1C_PERFCOUNTER0_SELECT, mmGL1C_PERFCOUNTER0_SELECT1, mmGL1C_PERFCOUNTER0_LO, mmGL1C_PERFCOUNTER0_HI },
        { mmGL1C_PERFCOUNTER1_SELECT, 0,                           mmGL1C_PERFCOUNTER1_LO, mmGL1C_PERFCOUNTER1_HI },
        { mmGL1C_PERFCOUNTER2_SELECT, 0,                           mmGL1C_PERFCOUNTER2_LO, mmGL1C_PERFCOUNTER2_HI },
        { mmGL1C_PERFCOUNTER3_SELECT, 0,                           mmGL1C_PERFCOUNTER3_LO, mmGL1C_PERFCOUNTER3_HI },
    }};

    // The GL2A (gl2 arbiter) block is typically broken down into four quadrants - we treat them as four instances.
    PerfCounterBlockInfo*const pGl2a = &pInfo->block[static_cast<uint32>(GpuBlock::Gl2a)];
    pGl2a->distribution              = PerfCounterDistribution::GlobalBlock;
    pGl2a->numScopedInstances        = pProps->gfx9.gfx10.numGl2a;
    pGl2a->numGenericSpmModules      = 2; // Gl2A_PERFCOUNTER0-1
    pGl2a->numGenericLegacyModules   = 2; // Gl2A_PERFCOUNTER2-3
    pGl2a->numSpmWires               = 4;
    pGl2a->spmBlockSelect            = Gfx10SpmGlobalBlockSelectGl2a;
    pGl2a->maxEventId                = maxIds[Gl2aPerfSelId];

    pGl2a->regAddr = { 0, {
        { mmGL2A_PERFCOUNTER0_SELECT, mmGL2A_PERFCOUNTER0_SELECT1, mmGL2A_PERFCOUNTER0_LO, mmGL2A_PERFCOUNTER0_HI },
        { mmGL2A_PERFCOUNTER1_SELECT, mmGL2A_PERFCOUNTER1_SELECT1, mmGL2A_PERFCOUNTER1_LO, mmGL2A_PERFCOUNTER1_HI },
        { mmGL2A_PERFCOUNTER2_SELECT, 0,                           mmGL2A_PERFCOUNTER2_LO, mmGL2A_PERFCOUNTER2_HI },
        { mmGL2A_PERFCOUNTER3_SELECT, 0,                           mmGL2A_PERFCOUNTER3_LO, mmGL2A_PERFCOUNTER3_HI },
    }};

    PerfCounterBlockInfo*const pGl2c = &pInfo->block[static_cast<uint32>(GpuBlock::Gl2c)];
    pGl2c->distribution              = PerfCounterDistribution::GlobalBlock;
    pGl2c->numScopedInstances        = pProps->gfx9.gfx10.numGl2c; // This should be equal to the number of EAs.
    pGl2c->numGenericSpmModules      = 2; // Gl2C_PERFCOUNTER0-1
    pGl2c->numGenericLegacyModules   = 2; // Gl2C_PERFCOUNTER2-3
    pGl2c->numSpmWires               = 4;
    pGl2c->spmBlockSelect            = Gfx10SpmGlobalBlockSelectGl2c;
    pGl2c->maxEventId                = maxIds[Gl2cPerfSelId];

    pGl2c->regAddr = { 0, {
        { mmGL2C_PERFCOUNTER0_SELECT, mmGL2C_PERFCOUNTER0_SELECT1, mmGL2C_PERFCOUNTER0_LO, mmGL2C_PERFCOUNTER0_HI },
        { mmGL2C_PERFCOUNTER1_SELECT, mmGL2C_PERFCOUNTER1_SELECT1, mmGL2C_PERFCOUNTER1_LO, mmGL2C_PERFCOUNTER1_HI },
        { mmGL2C_PERFCOUNTER2_SELECT, 0,                           mmGL2C_PERFCOUNTER2_LO, mmGL2C_PERFCOUNTER2_HI },
        { mmGL2C_PERFCOUNTER3_SELECT, 0,                           mmGL2C_PERFCOUNTER3_LO, mmGL2C_PERFCOUNTER3_HI },
    }};

    // The center hub arbiter (CHA). It's the global version of the GL1A and is used by global blocks.
    PerfCounterBlockInfo*const pCha = &pInfo->block[static_cast<uint32>(GpuBlock::Cha)];
    pCha->distribution              = PerfCounterDistribution::GlobalBlock;
    pCha->numScopedInstances        = 1;
    pCha->numGenericSpmModules      = 1; // CHA_PERFCOUNTER0
    pCha->numGenericLegacyModules   = 3; // CHA_PERFCOUNTER1-3
    pCha->numSpmWires               = 2;
    pCha->spmBlockSelect            = Gfx10SpmGlobalBlockSelectCha;
    pCha->maxEventId                = maxIds[ChaPerfSelId];

    pCha->regAddr = { 0, {
        { mmCHA_PERFCOUNTER0_SELECT, mmCHA_PERFCOUNTER0_SELECT1, mmCHA_PERFCOUNTER0_LO, mmCHA_PERFCOUNTER0_HI },
        { mmCHA_PERFCOUNTER1_SELECT, 0,                          mmCHA_PERFCOUNTER1_LO, mmCHA_PERFCOUNTER1_HI },
        { mmCHA_PERFCOUNTER2_SELECT, 0,                          mmCHA_PERFCOUNTER2_LO, mmCHA_PERFCOUNTER2_HI },
        { mmCHA_PERFCOUNTER3_SELECT, 0,                          mmCHA_PERFCOUNTER3_LO, mmCHA_PERFCOUNTER3_HI },
    }};

    // The center hub buffer (CHC). It's the global version of the GL1C and is used by global blocks.
    PerfCounterBlockInfo*const pChc = &pInfo->block[static_cast<uint32>(GpuBlock::Chc)];
    pChc->distribution              = PerfCounterDistribution::GlobalBlock;
    pChc->numScopedInstances        = 4; // It also has four quadrants like the GL1C.
    pChc->numGenericSpmModules      = 1; // CHC_PERFCOUNTER0
    pChc->numGenericLegacyModules   = 3; // CHC_PERFCOUNTER1-3
    pChc->numSpmWires               = 2;
    pChc->spmBlockSelect            = Gfx10SpmGlobalBlockSelectChc;
    pChc->maxEventId                = maxIds[ChcPerfSelId];

    pChc->regAddr = { 0, {
        { mmCHC_PERFCOUNTER0_SELECT, mmCHC_PERFCOUNTER0_SELECT1, mmCHC_PERFCOUNTER0_LO, mmCHC_PERFCOUNTER0_HI },
        { mmCHC_PERFCOUNTER1_SELECT, 0,                          mmCHC_PERFCOUNTER1_LO, mmCHC_PERFCOUNTER1_HI },
        { mmCHC_PERFCOUNTER2_SELECT, 0,                          mmCHC_PERFCOUNTER2_LO, mmCHC_PERFCOUNTER2_HI },
        { mmCHC_PERFCOUNTER3_SELECT, 0,                          mmCHC_PERFCOUNTER3_LO, mmCHC_PERFCOUNTER3_HI },
    }};

    // The global block that implements the graphics cache rinse feature.
    PerfCounterBlockInfo*const pGcr = &pInfo->block[static_cast<uint32>(GpuBlock::Gcr)];
    pGcr->distribution              = PerfCounterDistribution::GlobalBlock;
    pGcr->numScopedInstances        = 1;
    pGcr->numGenericSpmModules      = 1; // GCR_PERFCOUNTER0
    pGcr->numGenericLegacyModules   = 1; // GCR_PERFCOUNTER1
    pGcr->numSpmWires               = 2;
    pGcr->spmBlockSelect            = Gfx10SpmGlobalBlockSelectGcr;
    pGcr->maxEventId                = maxIds[GCRPerfSelId];

    pGcr->regAddr = { 0, {
        { mmGCR_PERFCOUNTER0_SELECT, mmGCR_PERFCOUNTER0_SELECT1, mmGCR_PERFCOUNTER0_LO, mmGCR_PERFCOUNTER0_HI },
        { mmGCR_PERFCOUNTER1_SELECT, 0,                          mmGCR_PERFCOUNTER1_LO, mmGCR_PERFCOUNTER1_HI },
    }};

    PerfCounterBlockInfo*const pPh = &pInfo->block[static_cast<uint32>(GpuBlock::Ph)];
    pPh->distribution              = PerfCounterDistribution::GlobalBlock;
    pPh->numScopedInstances        = 1;
    pPh->numGenericSpmModules      = 4; // PA_PH_PERFCOUNTER0-3
    pPh->numGenericLegacyModules   = 4; // PA_PH_PERFCOUNTER4-7
    pPh->numSpmWires               = 8;
    pPh->spmBlockSelect            = Gfx10SpmGlobalBlockSelectPh;
    pPh->maxEventId                = maxIds[PhPerfcntSelId];

    pPh->regAddr = { 0, {
        { mmPA_PH_PERFCOUNTER0_SELECT, mmPA_PH_PERFCOUNTER0_SELECT1, mmPA_PH_PERFCOUNTER0_LO, mmPA_PH_PERFCOUNTER0_HI },
        { mmPA_PH_PERFCOUNTER1_SELECT, mmPA_PH_PERFCOUNTER1_SELECT1, mmPA_PH_PERFCOUNTER1_LO, mmPA_PH_PERFCOUNTER1_HI },
        { mmPA_PH_PERFCOUNTER2_SELECT, mmPA_PH_PERFCOUNTER2_SELECT1, mmPA_PH_PERFCOUNTER2_LO, mmPA_PH_PERFCOUNTER2_HI },
        { mmPA_PH_PERFCOUNTER3_SELECT, mmPA_PH_PERFCOUNTER3_SELECT1, mmPA_PH_PERFCOUNTER3_LO, mmPA_PH_PERFCOUNTER3_HI },
        { mmPA_PH_PERFCOUNTER4_SELECT, 0,                            mmPA_PH_PERFCOUNTER4_LO, mmPA_PH_PERFCOUNTER4_HI },
        { mmPA_PH_PERFCOUNTER5_SELECT, 0,                            mmPA_PH_PERFCOUNTER5_LO, mmPA_PH_PERFCOUNTER5_HI },
        { mmPA_PH_PERFCOUNTER6_SELECT, 0,                            mmPA_PH_PERFCOUNTER6_LO, mmPA_PH_PERFCOUNTER6_HI },
        { mmPA_PH_PERFCOUNTER7_SELECT, 0,                            mmPA_PH_PERFCOUNTER7_LO, mmPA_PH_PERFCOUNTER7_HI },
    }};

    PerfCounterBlockInfo*const pUtcl1 = &pInfo->block[static_cast<uint32>(GpuBlock::UtcL1)];
    pUtcl1->distribution              = PerfCounterDistribution::PerShaderArray;
    pUtcl1->numScopedInstances        = 1;
    pUtcl1->numGenericLegacyModules   = 2; // UTCL1_PERFCOUNTER0-1
    pUtcl1->numSpmWires               = 0;
    pUtcl1->numGenericSpmModules      = 0;
    pUtcl1->maxEventId                = maxIds[UTCL1PerfSelId];

    pUtcl1->regAddr = { 0, {
        { Gfx11::mmUTCL1_PERFCOUNTER0_SELECT, 0, Gfx11::mmUTCL1_PERFCOUNTER0_LO, Gfx11::mmUTCL1_PERFCOUNTER0_HI },
        { Gfx11::mmUTCL1_PERFCOUNTER1_SELECT, 0, Gfx11::mmUTCL1_PERFCOUNTER1_LO, Gfx11::mmUTCL1_PERFCOUNTER1_HI },
    }};

    // The GUS and the blocks that exist to service it should exist as a unit. They are present on all gfx10.1 ASICs.
    // The CHCG connects the CH to the GUS, similar to how the CHC connects the CH to the GL2.
    if (pProps->gpuType == GpuType::Discrete)
    {
        PAL_ASSERT(IsNavi3x(device));

        PerfCounterBlockInfo*const pChcg = &pInfo->block[static_cast<uint32>(GpuBlock::Chcg)];
        pChcg->distribution              = PerfCounterDistribution::GlobalBlock;
        pChcg->numScopedInstances        = 1;
        pChcg->numGenericSpmModules      = 1; // CHCG_PERFCOUNTER0
        pChcg->numGenericLegacyModules   = 3; // CHCG_PERFCOUNTER1-3
        pChcg->numSpmWires               = 2;
        pChcg->spmBlockSelect            = Gfx10SpmGlobalBlockSelectChcg;
        pChcg->maxEventId                = maxIds[ChcgPerfSelId];

        pChcg->regAddr = { 0, {
            { Nv3x::mmCHCG_PERFCOUNTER0_SELECT, Nv3x::mmCHCG_PERFCOUNTER0_SELECT1, Nv3x::mmCHCG_PERFCOUNTER0_LO, Nv3x::mmCHCG_PERFCOUNTER0_HI },
            { Nv3x::mmCHCG_PERFCOUNTER1_SELECT, 0,                                 Nv3x::mmCHCG_PERFCOUNTER1_LO, Nv3x::mmCHCG_PERFCOUNTER1_HI },
            { Nv3x::mmCHCG_PERFCOUNTER2_SELECT, 0,                                 Nv3x::mmCHCG_PERFCOUNTER2_LO, Nv3x::mmCHCG_PERFCOUNTER2_HI },
            { Nv3x::mmCHCG_PERFCOUNTER3_SELECT, 0,                                 Nv3x::mmCHCG_PERFCOUNTER3_LO, Nv3x::mmCHCG_PERFCOUNTER3_HI },
        }};

        PerfCounterBlockInfo*const pGus = &pInfo->block[static_cast<uint32>(GpuBlock::Gus)];
        pGus->distribution              = PerfCounterDistribution::GlobalBlock;
        pGus->numScopedInstances        = 1;
        pGus->numGenericSpmModules      = 1; // GUS_PERFCOUNTER2
        pGus->numGenericLegacyModules   = 2; // GUS_PERFCOUNTER0-1
        pGus->numSpmWires               = 2;
        pGus->spmBlockSelect            = Gfx10SpmGlobalBlockSelectGus;
        pGus->maxEventId                = 175; // | dram_wr | Transaction `end` from CH_HI combiner, latency sampler 1
        pGus->isCfgStyle                = true;

        // Sets the register addresses.
        pGus->regAddr = { Nv3x::mmGUS_PERFCOUNTER_RSLT_CNTL, {
            { Nv3x::mmGUS_PERFCOUNTER0_CFG,    0,                                Nv3x::mmGUS_PERFCOUNTER_LO,  Nv3x::mmGUS_PERFCOUNTER_HI  },
            { Nv3x::mmGUS_PERFCOUNTER1_CFG,    0,                                Nv3x::mmGUS_PERFCOUNTER_LO,  Nv3x::mmGUS_PERFCOUNTER_HI  },
            { Nv3x::mmGUS_PERFCOUNTER2_SELECT, Nv3x::mmGUS_PERFCOUNTER2_SELECT1, Nv3x::mmGUS_PERFCOUNTER2_LO, Nv3x::mmGUS_PERFCOUNTER2_HI },
        }};
    }
}

// =====================================================================================================================
// Initializes the performance counter information for an adapter structure, specifically for the Gfx9 hardware layer.
void InitPerfCtrInfo(
    const Pal::Device& device,
    GpuChipProperties* pProps)
{
    // Something pretty terrible will probably happen if this isn't true.
    PAL_ASSERT(pProps->gfx9.numShaderEngines <= Gfx9MaxShaderEngines);

    PerfCounterInfo*const pInfo = &pProps->gfx9.perfCounterInfo.gfx9Info;

    // The caller should already have zeroed this struct a long time ago but let's do it again just to be sure.
    // We depend very heavily on unsupported fields being zero by default.
    memset(pInfo, 0, sizeof(*pInfo));

    // Some fields require non-zero defaults.
    for (uint32 idx = 0; idx < static_cast<uint32>(GpuBlock::Count); idx++)
    {
        PerfCounterBlockInfo*const pBlock = &pInfo->block[idx];

        // The SPM block select requires a non-zero default. We use UINT32_MAX to indicate "invalid".
        pBlock->spmBlockSelect = UINT32_MAX;

        // Almost all blocks have per-instance counter hardware.
        pBlock->instanceGroupSize = 1;
    }

    // These features are supported by all ASICs.
    pInfo->features.counters         = 1;
    pInfo->features.threadTrace      = 1;
    pInfo->features.spmTrace         = 1;
    pInfo->features.supportPs1Events = 1;
    pInfo->features.dfSpmTrace = IsGfx103Plus(pProps->gfxLevel);

    // Set the hardware specified per-block information (see the function for what exactly that means).
    // There's so much code to do this that it had to go in a helper function for each version.
    if (IsGfx10(pProps->gfxLevel))
    {
        Gfx10InitBasicBlockInfo(device, pProps);
    }
    else
    {
        Gfx11InitBasicBlockInfo(device, pProps);
    }

    // Using that information, infer the remaining per-block properties.
    for (uint32 idx = 0; idx < static_cast<uint32>(GpuBlock::Count); idx++)
    {
        PerfCounterBlockInfo*const pBlock = &pInfo->block[idx];

        if (pBlock->distribution != PerfCounterDistribution::Unavailable)
        {
            // Compute the total instance count.
            if (pBlock->distribution == PerfCounterDistribution::PerShaderArray)
            {
                pBlock->numInstances =
                    pBlock->numScopedInstances * pProps->gfx9.numActiveShaderEngines * pProps->gfx9.numShaderArrays;
            }
            else if (pBlock->distribution == PerfCounterDistribution::PerShaderEngine)
            {
                pBlock->numInstances = pBlock->numScopedInstances * pProps->gfx9.numActiveShaderEngines;
            }
            else
            {
                pBlock->numInstances = pBlock->numScopedInstances;
            }

            // If this triggers we need to increase MaxPerfModules.
            const uint32 totalGenericModules = pBlock->numGenericSpmModules + pBlock->numGenericLegacyModules;
            PAL_ASSERT(totalGenericModules <= MaxPerfModules);

            // These are a fairly simple translation for the generic blocks. The blocks that require special treatment
            // must set the generic module counts to zero and manually set their numbers of counters.
            if (totalGenericModules > 0)
            {
                PAL_ASSERT((pBlock->num16BitSpmCounters == 0) && (pBlock->num32BitSpmCounters == 0) &&
                           (pBlock->numGlobalOnlyCounters == 0) && (pBlock->numGlobalSharedCounters == 0));

                pBlock->num16BitSpmCounters     = pBlock->numGenericSpmModules * 4;
                pBlock->num32BitSpmCounters     = pBlock->numGenericSpmModules * 2;
                pBlock->numGlobalOnlyCounters   = pBlock->numGenericLegacyModules;
                pBlock->numGlobalSharedCounters = pBlock->numGenericSpmModules;
            }

            // If some block has SPM counters it must have SPM wires and a SPM block select.
            PAL_ASSERT(((pBlock->num16BitSpmCounters == 0) && (pBlock->num32BitSpmCounters == 0)) ||
                       ((pBlock->numSpmWires > 0) && (pBlock->spmBlockSelect != UINT32_MAX)));

            // We expect that every block should have a non-zero max event ID.
            PAL_ASSERT(pBlock->maxEventId > 0);
        }
    }

    if (IsGfx10(pProps->gfxLevel))
    {
        // Clamp the some shared counters to zero. Some gfx10 blocks have generic SPM modules but seem to only support
        // SPM counters. If we program them as global counters we always get zero out. Rather than handle this as a
        // special case in the perf experiment, it's far easier and cleaner to just forbid the client from trying to
        // program it as a global counter.
        pInfo->block[static_cast<uint32>(GpuBlock::AtcL2)].numGlobalSharedCounters = 0;
        pInfo->block[static_cast<uint32>(GpuBlock::McVmL2)].numGlobalSharedCounters = 0;
    }

    // Verify that we didn't exceed any of our hard coded per-block constants.
    PAL_ASSERT(pInfo->block[static_cast<uint32>(GpuBlock::Dma)].numInstances         <= Gfx9MaxSdmaInstances);
    PAL_ASSERT(pInfo->block[static_cast<uint32>(GpuBlock::Dma)].numGenericSpmModules <= Gfx9MaxSdmaPerfModules);
    PAL_ASSERT(pInfo->block[static_cast<uint32>(GpuBlock::Umcch)].numInstances       <= Gfx9MaxUmcchInstances);
}

} // Gfx9
} // Pal
