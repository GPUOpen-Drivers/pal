/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6PerfExperiment.h"
#include "core/hw/gfxip/pm4CmdBuffer.h"
#include "core/platform.h"
#include "palVectorImpl.h"

using namespace Util;

namespace Pal
{
namespace Gfx6
{

// As stated in the header we will zero-init our legacy select registers under the assumption that if the block has
// CNTR_MODE and PERF_MODE fields this will set them to their expected values.
static_assert((PERFMON_SPM_MODE_OFF == 0) && (PERFMON_COUNTER_MODE_ACCUM == 0),
              "Our legacy counter logic assumes CNTR_MODE and PERF_MODE should always be zero.");

// We assume these enums match their SE indices in a few places.
static_assert(static_cast<uint32>(SpmDataSegmentType::Se0) == 0, "SpmDataSegmentType::Se0 is not 0.");
static_assert(static_cast<uint32>(SpmDataSegmentType::Se1) == 1, "SpmDataSegmentType::Se1 is not 1.");
static_assert(static_cast<uint32>(SpmDataSegmentType::Se2) == 2, "SpmDataSegmentType::Se2 is not 2.");
static_assert(static_cast<uint32>(SpmDataSegmentType::Se3) == 3, "SpmDataSegmentType::Se3 is not 3.");

// Default SQ select masks for our counter options (by default, select all).
constexpr uint32 DefaultSqSelectSimdMask   = 0xF;
constexpr uint32 DefaultSqSelectBankMask   = 0xF;
constexpr uint32 DefaultSqSelectClientMask = 0xF;

// Stall when at 5/8s of the output buffer because data will still come in from already-issued waves
constexpr uint32  SqttHiWaterValue = 4;
// Bitmask limits for some sqtt parameters.
constexpr uint32  SqttPerfCounterCuMask = 0xFFFF;
constexpr uint32  SqttDetailedSimdMask  = 0xF;
constexpr uint32  SqttMaxRandomSeed     = 0xFFFF;
// Safe defaults for token exclude mask and register include mask for the SQTT_TOKEN_MASK/2 registers.
constexpr uint32  SqttRegMaskDefault      = 0xFF;
constexpr uint32  SqttTokenMaskDefault    = 0xBFFF;
constexpr uint32  Gfx7SqttInstMaskDefault = 0xFFFF;
constexpr uint32  Gfx8SqttInstMaskDefault = 0xFFFFFFFF;

// The SPM ring buffer base address must be 32-byte aligned.
constexpr uint32 SpmRingBaseAlignment = 32;

// The bound GPU memory must be aligned to the maximum of all alignment requirements.
constexpr gpusize GpuMemoryAlignment = Max<gpusize>(SqttBufferAlignment, SpmRingBaseAlignment);

// =====================================================================================================================
// Converts the thread trace token config to the HW format for programming the TOKEN_MASK register.
static uint32 GetSqttTokenMask(
    const ThreadTraceTokenConfig& tokenConfig)
{
    union
    {
        struct
        {
            union
            {
                struct
                {
                    uint16 misc         : 1;
                    uint16 timestamp    : 1;
                    uint16 reg          : 1;
                    uint16 waveStart    : 1;
                    uint16 waveAlloc    : 1;
                    uint16 regCsPriv    : 1;
                    uint16 waveEnd      : 1;
                    uint16 event        : 1;
                    uint16 eventCs      : 1;
                    uint16 eventGfx1    : 1;
                    uint16 inst         : 1;
                    uint16 instPc       : 1;
                    uint16 instUserData : 1;
                    uint16 issue        : 1;
                    uint16 perf         : 1;
                    uint16 regCs        : 1;
                };
                uint16 u16All;
            } tokenMask;

            union
            {
                struct
                {
                    uint8 eventInitiator         : 1;
                    uint8 drawInitiator          : 1;
                    uint8 dispatchInitiator      : 1;
                    uint8 userData               : 1;
                    uint8 ttMarkerEventInitiator : 1;
                    uint8 gfxdec                 : 1;
                    uint8 shdec                  : 1;
                    uint8 other                  : 1;
                };
                uint8 u8All;
            } regMask;

            uint8 reserved;
        };

        uint32 u32All;
    } value;

    if (tokenConfig.tokenMask == ThreadTraceTokenTypeFlags::All)
    {
        // Enable all token types except Perf.
        value.tokenMask.u16All = 0xBFFF;
    }
    else
    {
        // Perf counter gathering in thread trace is not supported currently.
        PAL_ALERT(TestAnyFlagSet(tokenConfig.tokenMask, ThreadTraceTokenTypeFlags::Perf));

        value.tokenMask.misc         = TestAnyFlagSet(tokenConfig.tokenMask, ThreadTraceTokenTypeFlags::Misc);
        value.tokenMask.timestamp    = TestAnyFlagSet(tokenConfig.tokenMask, ThreadTraceTokenTypeFlags::Timestamp);
        value.tokenMask.reg          = TestAnyFlagSet(tokenConfig.tokenMask, ThreadTraceTokenTypeFlags::Reg);
        value.tokenMask.waveStart    = TestAnyFlagSet(tokenConfig.tokenMask, ThreadTraceTokenTypeFlags::WaveStart);
        value.tokenMask.waveAlloc    = TestAnyFlagSet(tokenConfig.tokenMask, ThreadTraceTokenTypeFlags::WaveAlloc);
        value.tokenMask.regCsPriv    = TestAnyFlagSet(tokenConfig.tokenMask, ThreadTraceTokenTypeFlags::RegCsPriv);
        value.tokenMask.waveEnd      = TestAnyFlagSet(tokenConfig.tokenMask, ThreadTraceTokenTypeFlags::WaveEnd);
        value.tokenMask.event        = TestAnyFlagSet(tokenConfig.tokenMask, ThreadTraceTokenTypeFlags::Event);
        value.tokenMask.eventCs      = TestAnyFlagSet(tokenConfig.tokenMask, ThreadTraceTokenTypeFlags::EventCs);
        value.tokenMask.eventGfx1    = TestAnyFlagSet(tokenConfig.tokenMask, ThreadTraceTokenTypeFlags::EventGfx1);
        value.tokenMask.inst         = TestAnyFlagSet(tokenConfig.tokenMask, ThreadTraceTokenTypeFlags::Inst);
        value.tokenMask.instPc       = TestAnyFlagSet(tokenConfig.tokenMask, ThreadTraceTokenTypeFlags::InstPc);
        value.tokenMask.instUserData = TestAnyFlagSet(tokenConfig.tokenMask, ThreadTraceTokenTypeFlags::InstUserData);
        value.tokenMask.issue        = TestAnyFlagSet(tokenConfig.tokenMask, ThreadTraceTokenTypeFlags::Issue);
        value.tokenMask.regCs        = TestAnyFlagSet(tokenConfig.tokenMask, ThreadTraceTokenTypeFlags::RegCs);
    }

    // There is no option to choose between register reads and writes in TT2.1, so we enable all register ops.
    const bool allRegs = TestAllFlagsSet(tokenConfig.tokenMask, ThreadTraceRegTypeFlags::AllRegWrites) ||
                         TestAllFlagsSet(tokenConfig.tokenMask, ThreadTraceRegTypeFlags::AllRegReads)  ||
                         TestAllFlagsSet(tokenConfig.tokenMask, ThreadTraceRegTypeFlags::AllReadsAndWrites);

    if (allRegs)
    {
        //Note: According to the thread trace programming guide, the "other" bit must always be set to 0.
        //      However, this should be safe so long as stable 'profiling' clocks are enabled
        value.regMask.u8All = 0xFF;
    }
    else
    {
        const uint32 mask = tokenConfig.regMask;

        value.regMask.eventInitiator         = TestAnyFlagSet(mask, ThreadTraceRegTypeFlags::EventRegs);
        value.regMask.drawInitiator          = TestAnyFlagSet(mask, ThreadTraceRegTypeFlags::DrawRegs);
        value.regMask.dispatchInitiator      = TestAnyFlagSet(mask, ThreadTraceRegTypeFlags::DispatchRegs);
        value.regMask.userData               = TestAnyFlagSet(mask, ThreadTraceRegTypeFlags::UserdataRegs);
        value.regMask.gfxdec                 = TestAnyFlagSet(mask, ThreadTraceRegTypeFlags::GraphicsContextRegs);
        value.regMask.shdec                  = TestAnyFlagSet(mask, ThreadTraceRegTypeFlags::ShaderLaunchStateRegs);
        value.regMask.ttMarkerEventInitiator = TestAnyFlagSet(mask, ThreadTraceRegTypeFlags::MarkerRegs);
        value.regMask.other                  = TestAnyFlagSet(mask, ThreadTraceRegTypeFlags::OtherConfigRegs);
    }

    return value.u32All;
}

// =====================================================================================================================
PerfExperiment::PerfExperiment(
    const Device*                   pDevice,
    const PerfExperimentCreateInfo& createInfo)
    :
    Pal::PerfExperiment(pDevice->Parent(), createInfo, GpuMemoryAlignment),
    m_chipProps(pDevice->Parent()->ChipProperties()),
    m_counterInfo(pDevice->Parent()->ChipProperties().gfx6.perfCounterInfo),
    m_settings(pDevice->Settings()),
    m_registerInfo(pDevice->CmdUtil().GetRegInfo()),
    m_cmdUtil(pDevice->CmdUtil()),
    m_globalCounters(m_pPlatform),
    m_pSpmCounters(nullptr),
    m_numSpmCounters(0),
    m_spmRingSize(0),
    m_spmSampleInterval(0)
{
    memset(m_sqtt,           0, sizeof(m_sqtt));
    memset(m_pMuxselRams,    0, sizeof(m_pMuxselRams));
    memset(m_numMuxselLines, 0, sizeof(m_numMuxselLines));
    memset(&m_select,        0, sizeof(m_select));
}

// =====================================================================================================================
PerfExperiment::~PerfExperiment()
{
    PAL_SAFE_DELETE_ARRAY(m_pSpmCounters, m_pPlatform);

    for (uint32 idx = 0; idx < MaxNumSpmSegments; ++idx)
    {
        PAL_SAFE_DELETE_ARRAY(m_pMuxselRams[idx], m_pPlatform);
    }

    for (uint32 block = 0; block < GpuBlockCount; ++block)
    {
        if (m_select.pGeneric[block] != nullptr)
        {
            for (uint32 instance = 0; instance < m_select.numGeneric[block]; ++instance)
            {
                PAL_SAFE_DELETE_ARRAY(m_select.pGeneric[block][instance].pModules, m_pPlatform);
            }

            PAL_SAFE_DELETE_ARRAY(m_select.pGeneric[block], m_pPlatform);
        }
    }
}

// =====================================================================================================================
Result PerfExperiment::Init()
{
    Result result = Result::Success;

    // Validate some of our design assumption about the the hardware. These seem like valid assumptions but we can't
    // check them at compile time so this has to be an assert and an error instead of a static assert.
    if ((m_counterInfo.block[static_cast<uint32>(GpuBlock::Sq)].numGlobalInstances     > Gfx6MaxShaderEngines) ||
        (m_counterInfo.block[static_cast<uint32>(GpuBlock::GrbmSe)].numGlobalInstances > Gfx6MaxShaderEngines) ||
        (m_counterInfo.block[static_cast<uint32>(GpuBlock::Dma)].numGlobalInstances    > Gfx7MaxSdmaInstances) ||
        (m_counterInfo.block[static_cast<uint32>(GpuBlock::Mc)].numGlobalInstances     > MaxMcdTiles * NumMcChannels))
    {
        PAL_ASSERT_ALWAYS();
        result = Result::ErrorInitializationFailed;
    }

    return result;
}

// =====================================================================================================================
// Allocates memory for the generic select state. We need to allocate memory for all blocks that exist on our GPU
// unless we have special handling for them. To reduce the perf experiment overhead we delay allocating this memory
// until the client tries to add a global counter or SPM counter for a particular block and instance.
Result PerfExperiment::AllocateGenericStructs(
    GpuBlock block,
    uint32   globalInstance)
{
    Result       result             = Result::Success;
    const uint32 blockIdx           = static_cast<uint32>(block);
    const uint32 numGlobalInstances = m_counterInfo.block[blockIdx].numGlobalInstances;
    const uint32 numGenericModules  = m_counterInfo.block[blockIdx].numGenericSpmModules +
                                      m_counterInfo.block[blockIdx].numGenericLegacyModules;

    // Only continue if:
    // - There are instances of this block on our device.
    // - This block has generic counter modules.
    if ((numGlobalInstances > 0) && (numGenericModules > 0))
    {
        // Check that we haven't allocated the per-instance array already.
        if (m_select.pGeneric[blockIdx] == nullptr)
        {
            m_select.numGeneric[blockIdx] = numGlobalInstances;
            m_select.pGeneric[blockIdx] =
                PAL_NEW_ARRAY(GenericBlockSelect, numGlobalInstances, m_pPlatform, AllocObject);

            if (m_select.pGeneric[blockIdx] == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
            else
            {
                memset(m_select.pGeneric[blockIdx], 0, sizeof(GenericBlockSelect) * m_select.numGeneric[blockIdx]);
            }
        }

        // Check that we haven't allocated the per-module array already.
        if ((result == Result::Success) && (m_select.pGeneric[blockIdx][globalInstance].pModules == nullptr))
        {
            GenericBlockSelect*const pSelect = &m_select.pGeneric[blockIdx][globalInstance];

            // We need one GenericModule for each SPM module and legacy module.
            pSelect->numModules = numGenericModules;
            pSelect->pModules   = PAL_NEW_ARRAY(GenericSelect, numGenericModules, m_pPlatform, AllocObject);

            if (pSelect->pModules == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
            else
            {
                memset(pSelect->pModules, 0, sizeof(GenericSelect) * numGenericModules);

                // Set each module's type field at creation. It only depends on counter info.
                if (m_counterInfo.block[blockIdx].isCfgStyle)
                {
                    // We haven't implemented cfg-style counters in the gfx6 layer because the few that
                    // exist (e.g. ATC) aren't currently exposed to the client (and may never be).
                    PAL_ASSERT_ALWAYS();
                    result = Result::ErrorInitializationFailed;
                }
                else
                {
                    // Select-style: the perfmon modules always come before the legacy modules.
                    uint32 moduleIdx = 0;
                    while (moduleIdx < m_counterInfo.block[blockIdx].numGenericSpmModules)
                    {
                        pSelect->pModules[moduleIdx++].type = SelectType::Perfmon;
                    }

                    while (moduleIdx < pSelect->numModules)
                    {
                        pSelect->pModules[moduleIdx++].type = SelectType::LegacySel;
                    }
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
// This function adds a single global counter for a specific instance of some hardware block. It must:
// - If this is the first time this instance has enabled a counter, update hasCounters and get a GRBM_GFX_INDEX.
// - Locate an unused counter module (perfmon or legacy) and mark it as fully in use.
// - Configure that counter's primary PERF_SEL and other modes for global counting.
// - Update the counter mapping's data type and counter ID.
//
// Implementation notes:
// - According to the HW docs, the counters must be enabled in module order.
// - Most blocks name their SPM control CNTR_MODE and name their counter controls PERF_MODE, this is confusing.
Result PerfExperiment::AddCounter(
    const PerfCounterInfo& info)
{
    Result               result  = Result::Success;
    GlobalCounterMapping mapping = {};

    if (m_isFinalized)
    {
        // The perf experiment cannot be changed once it is finalized.
        result = Result::ErrorUnavailable;
    }
    else
    {
        // Set up the general mapping information and validate the counter. We will decide on an output offset later.
        result = BuildCounterMapping(info, &mapping.general);
    }

    if (result == Result::Success)
    {
        // Make sure we will have the necessary generic select structs for this block and instance.
        result = AllocateGenericStructs(info.block, info.instance);
    }

    InstanceMapping instanceMapping = {};

    if (result == Result::Success)
    {
        // Get an instance mapping for this counter. We don't really need to do this once per AddCounter call but
        // doing it up-front here makes things a bit simpler below.
        result = BuildInstanceMapping(info.block, info.instance, &instanceMapping);
    }

    // Enable a global perf counter select and update the mapping's counterId.
    if (result == Result::Success)
    {
        const uint32 block = static_cast<uint32>(info.block);

        if (info.block == GpuBlock::Sq)
        {
            // The SQ counters are 64-bit.
            mapping.general.dataType = PerfCounterDataType::Uint64;

            // The SQG has special registers so it needs its own implementation.
            if (m_select.sqg[info.instance].hasCounters == false)
            {
                // Turn on this instance and populate its GRBM_GFX_INDEX.
                m_select.sqg[info.instance].hasCounters  = true;
                m_select.sqg[info.instance].grbmGfxIndex = BuildGrbmGfxIndex(instanceMapping, info.block);
            }

            bool searching = true;

            for (uint32 idx = 0; searching && (idx < ArrayLen(m_select.sqg[info.instance].perfmon)); ++idx)
            {
                if (m_select.sqg[info.instance].perfmonInUse[idx] == false)
                {
                    // Our SQ PERF_SEL fields are 9 bits. Verify that our event ID can fit.
                    PAL_ASSERT(info.eventId <= ((1 << 9) - 1));

                    m_select.sqg[info.instance].perfmonInUse[idx]           = true;
                    m_select.sqg[info.instance].perfmon[idx].bits.PERF_SEL  = info.eventId;
                    m_select.sqg[info.instance].perfmon[idx].bits.SIMD_MASK = DefaultSqSelectSimdMask;
                    m_select.sqg[info.instance].perfmon[idx].bits.SPM_MODE  = PERFMON_SPM_MODE_OFF;
                    m_select.sqg[info.instance].perfmon[idx].bits.PERF_MODE = PERFMON_COUNTER_MODE_ACCUM;

                    if (m_chipProps.gfxLevel >= GfxIpLevel::GfxIp7)
                    {
                        // The SQC bank mask and client mask only exist on gfx7+.
                        m_select.sqg[info.instance].perfmon[idx].bits.SQC_BANK_MASK   = DefaultSqSelectBankMask;
                        m_select.sqg[info.instance].perfmon[idx].bits.SQC_CLIENT_MASK = DefaultSqSelectClientMask;
                    }

                    mapping.counterId = idx;
                    searching         = false;
                }
            }

            if (searching)
            {
                // There are no more global counters in this instance.
                result = Result::ErrorInvalidValue;
            }
        }
        else if (info.block == GpuBlock::GrbmSe)
        {
            // The GRBM counters are 64-bit.
            mapping.general.dataType = PerfCounterDataType::Uint64;

            // The GRBM has a single counter per SE instance; enable that counter if it is unused.
            if (m_select.grbmSe[info.instance].hasCounter == false)
            {
                // Our GRBM PERF_SEL fields are 6 bits. Verify that our event ID can fit.
                PAL_ASSERT(info.eventId <= ((1 << 6) - 1));

                m_select.grbmSe[info.instance].hasCounter           = true;
                m_select.grbmSe[info.instance].select.bits.PERF_SEL = info.eventId;

                mapping.counterId = 0;
            }
            else
            {
                // The only counter is in use.
                result = Result::ErrorInvalidValue;
            }
        }
        else if (info.block == GpuBlock::Dma)
        {
            // The SDMA counters are 32-bit.
            mapping.general.dataType = PerfCounterDataType::Uint32;

            // SDMA perf_sel fields are 6 bits. Verify that our event ID can fit.
            PAL_ASSERT(info.eventId <= ((1 << 6) - 1));

            // Each GFX7+ SDMA engine defines two special global counters controlled by one register.
            if (m_select.sdma[info.instance].hasCounter[0] == false)
            {
                m_select.sdma[info.instance].hasCounter[0]                 = true;
                m_select.sdma[info.instance].perfmonCntl.bits.PERF_ENABLE0 = 1;
                m_select.sdma[info.instance].perfmonCntl.bits.PERF_CLEAR0  = 1; // Might as well clear it.
                m_select.sdma[info.instance].perfmonCntl.bits.PERF_SEL0    = info.eventId;

                mapping.counterId = 0;
            }
            else if (m_select.sdma[info.instance].hasCounter[1] == false)
            {
                m_select.sdma[info.instance].hasCounter[1]                 = true;
                m_select.sdma[info.instance].perfmonCntl.bits.PERF_ENABLE1 = 1;
                m_select.sdma[info.instance].perfmonCntl.bits.PERF_CLEAR1  = 1; // Might as well clear it.
                m_select.sdma[info.instance].perfmonCntl.bits.PERF_SEL1    = info.eventId;

                mapping.counterId = 1;
            }
            else
            {
                // The only two counters are in use.
                result = Result::ErrorInvalidValue;
            }
        }
        else if (info.block == GpuBlock::Mc)
        {
            // The MC counters are 32-bit.
            mapping.general.dataType = PerfCounterDataType::Uint32;

            // MC select fields are 5 bits. Verify that our event ID can fit.
            PAL_ASSERT(info.eventId <= ((1 << 5) - 1));

            // Map this instance back to a MC[D] and channel. The old perf experiment code treated the MC index as the
            // "inner" index so we do the same.
            const uint32 mcIdx = info.instance % m_chipProps.gfx6.numMcdTiles;
            const uint32 chIdx = info.instance / m_chipProps.gfx6.numMcdTiles;

            // Find the next unused global counter in the special MC state. We have to check each counter manually.
            // And split the event ID into 4-bit and 1-bit sections.
            if (chIdx == 0)
            {
                if (m_select.mc[mcIdx].counterInUse[chIdx][0] == false)
                {
                    m_select.mc[mcIdx].hasCounters              = true;
                    m_select.mc[mcIdx].counterInUse[chIdx][0]   = true;
                    m_select.mc[mcIdx].perfSeqCntl.bits.SEL_A   = info.eventId & 0xF;
                    m_select.mc[mcIdx].perfCntl1.bits.SEL_A_MSB = (info.eventId & 0x10) >> 4;

                    mapping.counterId = 0;
                }
                else if (m_select.mc[mcIdx].counterInUse[chIdx][1] == false)
                {
                    m_select.mc[mcIdx].hasCounters              = true;
                    m_select.mc[mcIdx].counterInUse[chIdx][1]   = true;
                    m_select.mc[mcIdx].perfSeqCntl.bits.SEL_B   = info.eventId & 0xF;
                    m_select.mc[mcIdx].perfCntl1.bits.SEL_B_MSB = (info.eventId & 0x10) >> 4;

                    mapping.counterId = 1;
                }
                else if (m_select.mc[mcIdx].counterInUse[chIdx][2] == false)
                {
                    m_select.mc[mcIdx].hasCounters                  = true;
                    m_select.mc[mcIdx].counterInUse[chIdx][2]       = true;
                    m_select.mc[mcIdx].perfSeqCntl.bits.SEL_CH0_C   = info.eventId & 0xF;
                    m_select.mc[mcIdx].perfCntl1.bits.SEL_CH0_C_MSB = (info.eventId & 0x10) >> 4;

                    mapping.counterId = 2;
                }
                else if (m_select.mc[mcIdx].counterInUse[chIdx][3] == false)
                {
                    m_select.mc[mcIdx].hasCounters                  = true;
                    m_select.mc[mcIdx].counterInUse[chIdx][3]       = true;
                    m_select.mc[mcIdx].perfSeqCntl.bits.SEL_CH0_D   = info.eventId & 0xF;
                    m_select.mc[mcIdx].perfCntl1.bits.SEL_CH0_D_MSB = (info.eventId & 0x10) >> 4;

                    mapping.counterId = 3;
                }
                else
                {
                    // There are no more global counters in this instance.
                    result = Result::ErrorInvalidValue;
                }
            }
            else
            {
                PAL_ASSERT(chIdx == 1);

                if (m_select.mc[mcIdx].counterInUse[chIdx][0] == false)
                {
                    m_select.mc[mcIdx].hasCounters                  = true;
                    m_select.mc[mcIdx].counterInUse[chIdx][0]       = true;
                    m_select.mc[mcIdx].perfSeqCntl.bits.SEL_CH1_A   = info.eventId & 0xF;
                    m_select.mc[mcIdx].perfCntl1.bits.SEL_CH1_A_MSB = (info.eventId & 0x10) >> 4;

                    mapping.counterId = 0;
                }
                else if (m_select.mc[mcIdx].counterInUse[chIdx][1] == false)
                {
                    m_select.mc[mcIdx].hasCounters                  = true;
                    m_select.mc[mcIdx].counterInUse[chIdx][1]       = true;
                    m_select.mc[mcIdx].perfSeqCntl.bits.SEL_CH1_B   = info.eventId & 0xF;
                    m_select.mc[mcIdx].perfCntl1.bits.SEL_CH1_B_MSB = (info.eventId & 0x10) >> 4;

                    mapping.counterId = 1;
                }
                else if (m_select.mc[mcIdx].counterInUse[chIdx][2] == false)
                {
                    m_select.mc[mcIdx].hasCounters                  = true;
                    m_select.mc[mcIdx].counterInUse[chIdx][2]       = true;
                    m_select.mc[mcIdx].perfSeqCntl.bits.SEL_CH1_C   = info.eventId & 0xF;
                    m_select.mc[mcIdx].perfCntl1.bits.SEL_CH1_C_MSB = (info.eventId & 0x10) >> 4;

                    mapping.counterId = 2;
                }
                else if (m_select.mc[mcIdx].counterInUse[chIdx][3] == false)
                {
                    m_select.mc[mcIdx].hasCounters                  = true;
                    m_select.mc[mcIdx].counterInUse[chIdx][3]       = true;
                    m_select.mc[mcIdx].perfSeqCntl.bits.SEL_CH1_D   = info.eventId & 0xF;
                    m_select.mc[mcIdx].perfCntl1.bits.SEL_CH1_D_MSB = (info.eventId & 0x10) >> 4;

                    mapping.counterId = 3;
                }
                else
                {
                    // There are no more global counters in this instance.
                    result = Result::ErrorInvalidValue;
                }
            }
        }
        else if (m_select.pGeneric[block] != nullptr)
        {
            // All generic global counters are 64-bit.
            mapping.general.dataType = PerfCounterDataType::Uint64;

            // Finally, handle all generic blocks.
            GenericBlockSelect*const pSelect = &m_select.pGeneric[block][info.instance];

            if (m_select.pGeneric[block][info.instance].hasCounters == false)
            {
                // Turn on this instance and populate its GRBM_GFX_INDEX.
                pSelect->hasCounters  = true;
                pSelect->grbmGfxIndex = BuildGrbmGfxIndex(instanceMapping, info.block);
            }

            // Find and enable a global counter. All of the counter user guides say that the modules need to be
            // enabled in counter register# order.
            bool searching = true;

            for (uint32 moduleIdx = 0; searching && (moduleIdx < pSelect->numModules); ++moduleIdx)
            {
                if (pSelect->pModules[moduleIdx].inUse == 0)
                {
                    switch (pSelect->pModules[moduleIdx].type)
                    {
                    case SelectType::Perfmon:
                        // Our generic select PERF_SEL fields are 9 bits. Verify that our event ID can fit.
                        PAL_ASSERT(info.eventId <= ((1 << 9) - 1));

                        // A global counter uses the whole perfmon module (0xF).
                        pSelect->pModules[moduleIdx].inUse                       = 0xF;
                        pSelect->pModules[moduleIdx].perfmon.sel0.bits.PERF_SEL  = info.eventId;
                        pSelect->pModules[moduleIdx].perfmon.sel0.bits.CNTR_MODE = PERFMON_SPM_MODE_OFF;
                        pSelect->pModules[moduleIdx].perfmon.sel0.bits.PERF_MODE = PERFMON_COUNTER_MODE_ACCUM;
                        break;

                    case SelectType::LegacySel:
                        // Our generic select PERF_SEL fields are 10 bits. Verify that our event ID can fit.
                        PAL_ASSERT(info.eventId <= ((1 << 10) - 1));

                        // A global counter uses the whole legacy module (0xF).
                        pSelect->pModules[moduleIdx].inUse                    = 0xF;
                        pSelect->pModules[moduleIdx].legacySel.bits.PERF_SEL  = info.eventId;
                        break;

                    default:
                        // What is this?
                        PAL_ASSERT_ALWAYS();
                        break;
                    }

                    mapping.counterId = moduleIdx;
                    searching         = false;
                }
            }

            if (searching)
            {
                // There are no more global counters in this instance.
                result = Result::ErrorInvalidValue;
            }
        }
        else
        {
            // We don't support this block on this device.
            result = Result::ErrorInvalidValue;
        }
    }

    // Record the counter mapping as our last step so we don't end up with bad mappings when we're out of counters.
    if (result == Result::Success)
    {
        result = m_globalCounters.PushBack(mapping);
    }

    if (result == Result::Success)
    {
        m_perfExperimentFlags.perfCtrsEnabled = true;
    }

    return result;
}

// =====================================================================================================================
// This function configures a single SPM counter (16-bit or 32-bit) for a specific instance of some block. It must:
// - If this is the first time this instance has enabled a counter, update hasCounters and get a GRBM_GFX_INDEX.
// - Locate an unused perfmon counter module and mark part of it in use.
// - Configure that counter's SPM mode, PERF_SELs, and other state for 16-bit or 32-bit SPM counting.
// - Identify which SPM wire will be used and finish building the SPM counter mapping.
//
// Implementation notes:
// - According to the HW docs, the counters must be enabled in module order.
// - Most blocks name their SPM control CNTR_MODE and name their counter controls PERF_MODE, this is confusing.
Result PerfExperiment::AddSpmCounter(
    const PerfCounterInfo& info,
    SpmCounterMapping*     pMapping)
{
    Result result = Result::Success;

    if (m_isFinalized)
    {
        // The perf experiment cannot be changed once it is finalized.
        result = Result::ErrorUnavailable;
    }
    else
    {
        // Set up the general mapping information and validate the counter.
        result = BuildCounterMapping(info, &pMapping->general);
    }

    if (result == Result::Success)
    {
        // Make sure we will have the necessary generic select structs for this block and instance.
        result = AllocateGenericStructs(info.block, info.instance);
    }

    InstanceMapping instanceMapping = {};

    if (result == Result::Success)
    {
        // Get an instance mapping for this counter.
        result = BuildInstanceMapping(info.block, info.instance, &instanceMapping);
    }

    // Enable a select register and finish building our counter mapping within some SPM segment. We need to track which
    // SPM wire is hooked up to the current module and which 16-bit sub-counter we selected within that wire.
    const uint32 block      = static_cast<uint32>(info.block);
    uint32       spmWire    = 0;
    uint32       subCounter = 0;

    if (result == Result::Success)
    {
        if (info.block == GpuBlock::Sq)
        {
            // The SQG has special registers so it needs its own implementation.
            if (m_select.sqg[info.instance].hasCounters == false)
            {
                // Turn on this instance and populate its GRBM_GFX_INDEX.
                m_select.sqg[info.instance].hasCounters = true;
                m_select.sqg[info.instance].grbmGfxIndex = BuildGrbmGfxIndex(instanceMapping, info.block);
            }

            bool searching = true;

            for (uint32 idx = 0; searching && (idx < ArrayLen(m_select.sqg[info.instance].perfmon)); ++idx)
            {
                if (m_select.sqg[info.instance].perfmonInUse[idx] == false)
                {
                    // Our SQ PERF_SEL fields are 9 bits. Verify that our event ID can fit.
                    PAL_ASSERT(info.eventId <= ((1 << 9) - 1));

                    // The SQG doesn't support 16-bit counters and only has one 32-bit counter per select register.
                    // As long as the counter doesn't wrap over 16 bits we can enable a 32-bit counter and treat
                    // it exactly like a 16-bit counter and still get useful data. Note that "LEVEL" counters require
                    // us to use the no-clamp & no-reset SPM mode.
                    const uint32 spmMode = IsSqLevelEvent(info.eventId) ? PERFMON_SPM_MODE_32BIT_NO_CLAMP
                                                                        : PERFMON_SPM_MODE_32BIT_CLAMP;

                    m_select.sqg[info.instance].perfmonInUse[idx]           = true;
                    m_select.sqg[info.instance].perfmon[idx].bits.PERF_SEL  = info.eventId;
                    m_select.sqg[info.instance].perfmon[idx].bits.SIMD_MASK = DefaultSqSelectSimdMask;
                    m_select.sqg[info.instance].perfmon[idx].bits.SPM_MODE  = spmMode;
                    m_select.sqg[info.instance].perfmon[idx].bits.PERF_MODE = PERFMON_COUNTER_MODE_ACCUM;

                    if (m_chipProps.gfxLevel >= GfxIpLevel::GfxIp7)
                    {
                        // The SQC bank mask and client mask only exist on gfx7+.
                        m_select.sqg[info.instance].perfmon[idx].bits.SQC_BANK_MASK   = DefaultSqSelectBankMask;
                        m_select.sqg[info.instance].perfmon[idx].bits.SQC_CLIENT_MASK = DefaultSqSelectClientMask;
                    }

                    // Each SQ module gets a single wire with one sub-counter (use the default value of zero).
                    spmWire   = idx;
                    searching = false;
                }
            }

            if (searching)
            {
                // There are no more compatible SPM counters in this instance.
                result = Result::ErrorInvalidValue;
            }
        }
        else if (m_select.pGeneric[block] != nullptr)
        {
            // Finally, handle all generic blocks.
            GenericBlockSelect*const pSelect = &m_select.pGeneric[block][info.instance];

            if (m_select.pGeneric[block][info.instance].hasCounters == false)
            {
                // Turn on this instance and populate its GRBM_GFX_INDEX.
                pSelect->hasCounters = true;
                pSelect->grbmGfxIndex = BuildGrbmGfxIndex(instanceMapping, info.block);
            }

            // Search for an unused 16-bit sub-counter. This will need to be reworked when we add 32-bit support.
            bool searching = true;

            for (uint32 idx = 0; idx < pSelect->numModules; idx++)
            {
                if (pSelect->pModules[idx].type == SelectType::Perfmon)
                {
                    // Our generic select PERF_SEL fields are 9 bits. Verify that our event ID can fit.
                    PAL_ASSERT(info.eventId <= ((1 << 9) - 1));

                    // Each write holds two 16-bit sub-counters. We must check each wire individually because
                    // some blocks look like they have a whole perfmon module but only use half of it.
                    if (spmWire < m_counterInfo.block[block].numSpmWires)
                    {
                        if (TestAnyFlagSet(pSelect->pModules[idx].inUse, 0x1) == false)
                        {
                            pSelect->pModules[idx].inUse                      |= 0x1;
                            pSelect->pModules[idx].perfmon.sel0.bits.PERF_SEL  = info.eventId;
                            pSelect->pModules[idx].perfmon.sel0.bits.CNTR_MODE = PERFMON_SPM_MODE_16BIT_CLAMP;
                            pSelect->pModules[idx].perfmon.sel0.bits.PERF_MODE = PERFMON_COUNTER_MODE_ACCUM;

                            subCounter = 0;
                            searching  = false;
                            break;
                        }
                        else if (TestAnyFlagSet(pSelect->pModules[idx].inUse, 0x2) == false)
                        {
                            pSelect->pModules[idx].inUse                       |= 0x2;
                            pSelect->pModules[idx].perfmon.sel0.bits.PERF_SEL1  = info.eventId;
                            pSelect->pModules[idx].perfmon.sel0.bits.PERF_MODE1 = PERFMON_COUNTER_MODE_ACCUM;

                            subCounter = 1;
                            searching  = false;
                            break;
                        }

                        spmWire++;
                    }

                    if (spmWire < m_counterInfo.block[block].numSpmWires)
                    {
                        if (TestAnyFlagSet(pSelect->pModules[idx].inUse, 0x4) == false)
                        {
                            pSelect->pModules[idx].inUse                       |= 0x4;
                            pSelect->pModules[idx].perfmon.sel1.bits.PERF_SEL2  = info.eventId;
                            pSelect->pModules[idx].perfmon.sel1.bits.PERF_MODE2 = PERFMON_COUNTER_MODE_ACCUM;

                            subCounter = 0;
                            searching  = false;
                            break;
                        }
                        else if (TestAnyFlagSet(pSelect->pModules[idx].inUse, 0x8) == false)
                        {
                            pSelect->pModules[idx].inUse                       |= 0x8;
                            pSelect->pModules[idx].perfmon.sel1.bits.PERF_SEL3  = info.eventId;
                            pSelect->pModules[idx].perfmon.sel1.bits.PERF_MODE3 = PERFMON_COUNTER_MODE_ACCUM;

                            subCounter = 1;
                            searching  = false;
                            break;
                        }

                        spmWire++;
                    }
                }
            }

            if (searching)
            {
                // There are no more SPM counters in this instance.
                result = Result::ErrorInvalidValue;
            }
        }
        else
        {
            // We don't support this block on this device or it doesn't support SPM.
            result = Result::ErrorInvalidValue;
        }
    }

    if (result == Result::Success)
    {
        if (m_counterInfo.block[block].spmBlockSelect == UINT32_MAX)
        {
            // This block doesn't support SPM. Assert that that this is the client's mistake.
            PAL_ASSERT((m_counterInfo.block[block].num16BitSpmCounters == 0) &&
                       (m_counterInfo.block[block].num32BitSpmCounters == 0));

            result = Result::ErrorInvalidValue;
        }
        else
        {
            PAL_ASSERT(spmWire < m_counterInfo.block[block].numSpmWires);
            PAL_ASSERT(subCounter < 2); // Each wire is 32 bits and each sub-counter is 16 bits.

            pMapping->segment = (m_counterInfo.block[block].distribution == PerfCounterDistribution::GlobalBlock)
                                    ? SpmDataSegmentType::Global
                                    : static_cast<SpmDataSegmentType>(instanceMapping.seIndex);

            // For now we only support 16-bit counters so this counter is either even or odd. 32-bit counters will
            // be both even and odd so that we get the full 32-bit value from the SPM wire.
            pMapping->isEven = (subCounter == 0);
            pMapping->isOdd  = (subCounter != 0);

            if (pMapping->isEven)
            {
                pMapping->evenMuxsel.counter  = 2 * spmWire; // We want the lower 16 bits of this wire.
                pMapping->evenMuxsel.block    = m_counterInfo.block[block].spmBlockSelect;
                pMapping->evenMuxsel.instance = instanceMapping.instanceIndex;
            }

            if (pMapping->isOdd)
            {
                pMapping->oddMuxsel.counter  = 2 * spmWire + 1; // We want the upper 16 bits of this wire.
                pMapping->oddMuxsel.block    = m_counterInfo.block[block].spmBlockSelect;
                pMapping->oddMuxsel.instance = instanceMapping.instanceIndex;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// It looks like the client can only call this function once per SE which makes things simple. It must:
// - Validate the ThreadTraceInfo against our HW-specific requirements.
// - Compute all register state based on the ThreadTraceInfo.
Result PerfExperiment::AddThreadTrace(
    const ThreadTraceInfo& traceInfo)
{
    Result result = Result::Success;

    if (m_isFinalized)
    {
        // The perf experiment cannot be changed once it is finalized.
        result = Result::ErrorUnavailable;
    }
    // Validate the trace info.
    else if (traceInfo.instance >= m_chipProps.gfx6.numShaderEngines)
    {
        // There's one thread trace instance per SQG.
        result = Result::ErrorInvalidValue;
    }
    else if (m_sqtt[traceInfo.instance].inUse)
    {
        // You can't use the same instance twice!
        result = Result::ErrorInvalidValue;
    }
    else if ((traceInfo.optionFlags.bufferSize != 0) &&
             ((traceInfo.optionValues.bufferSize == 0) ||
              (traceInfo.optionValues.bufferSize > SqttMaximumBufferSize) ||
              (IsPow2Aligned(traceInfo.optionValues.bufferSize, SqttBufferAlignment) == false)))
    {
        // The buffer size can't be larger than the maximum size and it must be properly aligned.
        result = Result::ErrorInvalidValue;
    }
    else if ((traceInfo.optionFlags.threadTraceTokenConfig != 0) &&
             (traceInfo.optionValues.threadTraceTokenConfig.tokenMask == 0) &&
             (traceInfo.optionValues.threadTraceTokenConfig.regMask == 0))
    {
        // The thread trace token config can't be empty.
        result = Result::ErrorInvalidValue;
    }
    else if ((traceInfo.optionFlags.threadTraceTargetSh != 0) &&
             (traceInfo.optionValues.threadTraceTargetSh >= m_chipProps.gfx6.numShaderArrays))
    {
        // The detailed shader array is out of bounds.
        result = Result::ErrorInvalidValue;
    }
    else if ((traceInfo.optionFlags.threadTraceTargetCu != 0) &&
             (traceInfo.optionValues.threadTraceTargetCu >= m_chipProps.gfx6.numCuPerSh))
    {
        // The detailed CU is out of bounds.
        result = Result::ErrorInvalidValue;
    }
    else if ((traceInfo.optionFlags.threadTraceSh0CounterMask != 0) &&
             TestAnyFlagSet(traceInfo.optionValues.threadTraceSh0CounterMask, ~SqttPerfCounterCuMask))
    {
        // A CU is selected that doesn't exist.
        result = Result::ErrorInvalidValue;
    }
    else if ((traceInfo.optionFlags.threadTraceSh1CounterMask != 0) &&
             TestAnyFlagSet(traceInfo.optionValues.threadTraceSh1CounterMask, ~SqttPerfCounterCuMask))
    {
        // A CU is selected that doesn't exist.
        result = Result::ErrorInvalidValue;
    }
    else if ((traceInfo.optionFlags.threadTraceSimdMask != 0) &&
             (TestAnyFlagSet(traceInfo.optionValues.threadTraceSimdMask, ~SqttDetailedSimdMask)))
    {
        // A SIMD is selected that doesn't exist.
        result = Result::ErrorInvalidValue;
    }
    else if ((traceInfo.optionFlags.threadTraceVmIdMask != 0) &&
             (traceInfo.optionValues.threadTraceVmIdMask > SQ_THREAD_TRACE_VM_ID_MASK_SINGLE_DETAIL))
    {
        // This feels like a hack.
        result = Result::ErrorInvalidValue;
    }
    else if ((traceInfo.optionFlags.threadTraceRandomSeed != 0) &&
             (traceInfo.optionValues.threadTraceRandomSeed > SqttMaxRandomSeed))
    {
        // The given seed won't fit in the register field.
        result = Result::ErrorInvalidValue;
    }
    else if ((traceInfo.optionFlags.threadTraceShaderTypeMask != 0) &&
             ((traceInfo.optionValues.threadTraceShaderTypeMask & ~PerfShaderMaskAll) != 0))
    {
        // What is this shader stage?
        result = Result::ErrorInvalidValue;
    }
    else if ((traceInfo.optionFlags.threadTraceIssueMask != 0) &&
             ((traceInfo.optionValues.threadTraceIssueMask > SQ_THREAD_TRACE_ISSUE_MASK_IMMED__CI__VI) ||
              ((m_chipProps.gfxLevel == GfxIpLevel::GfxIp6) &&
               (traceInfo.optionValues.threadTraceIssueMask > SQ_THREAD_TRACE_ISSUE_MASK_STALLED_AND_IMMED))))
    {
        // This feels like a hack.
        result = Result::ErrorInvalidValue;
    }
    else if ((traceInfo.optionFlags.threadTraceStallBehavior != 0) &&
             (traceInfo.optionValues.threadTraceStallBehavior > GpuProfilerStallNever))
    {
        // The stall mode is invalid.
        result = Result::ErrorInvalidValue;
    }
    else
    {
        m_perfExperimentFlags.sqtTraceEnabled = true;

        // Set all sqtt properties for this trace except for the buffer offset which is found during Finalize.
        m_sqtt[traceInfo.instance].inUse = true;
        m_sqtt[traceInfo.instance].bufferSize = (traceInfo.optionFlags.bufferSize != 0)
                ? traceInfo.optionValues.bufferSize : SqttDefaultBufferSize;

        // Default to all shader stages enabled.
        const PerfExperimentShaderFlags shaderMask = (traceInfo.optionFlags.threadTraceShaderTypeMask != 0)
                ? traceInfo.optionValues.threadTraceShaderTypeMask : PerfShaderMaskAll;

        // Default to getting detailed tokens from shader array 0.
        const uint32 shIndex = (traceInfo.optionFlags.threadTraceTargetSh != 0)
                ? traceInfo.optionValues.threadTraceTargetSh : 0;

        // Target this trace's specific SE and SH.
        m_sqtt[traceInfo.instance].grbmGfxIndex.bits.SE_INDEX = traceInfo.instance;
        m_sqtt[traceInfo.instance].grbmGfxIndex.bits.SH_INDEX = shIndex;
        m_sqtt[traceInfo.instance].grbmGfxIndex.bits.INSTANCE_BROADCAST_WRITES = 1;

        // Setup the mode register.
        m_sqtt[traceInfo.instance].mode.bits.MASK_PS      = ((shaderMask & PerfShaderMaskPs) != 0);
        m_sqtt[traceInfo.instance].mode.bits.MASK_VS      = ((shaderMask & PerfShaderMaskVs) != 0);
        m_sqtt[traceInfo.instance].mode.bits.MASK_GS      = ((shaderMask & PerfShaderMaskGs) != 0);
        m_sqtt[traceInfo.instance].mode.bits.MASK_ES      = ((shaderMask & PerfShaderMaskEs) != 0);
        m_sqtt[traceInfo.instance].mode.bits.MASK_HS      = ((shaderMask & PerfShaderMaskHs) != 0);
        m_sqtt[traceInfo.instance].mode.bits.MASK_LS      = ((shaderMask & PerfShaderMaskLs) != 0);
        m_sqtt[traceInfo.instance].mode.bits.MASK_CS      = ((shaderMask & PerfShaderMaskCs) != 0);
        m_sqtt[traceInfo.instance].mode.bits.MODE         = SQ_THREAD_TRACE_MODE_ON;
        m_sqtt[traceInfo.instance].mode.bits.CAPTURE_MODE = SQ_THREAD_TRACE_CAPTURE_MODE_ALL;
        m_sqtt[traceInfo.instance].mode.bits.AUTOFLUSH_EN = 1; // Periodically flush SQTT data to memory.

        // By default capture all instruction scheduling updates.
        m_sqtt[traceInfo.instance].mode.bits.ISSUE_MASK = (traceInfo.optionFlags.threadTraceIssueMask != 0)
                ? traceInfo.optionValues.threadTraceIssueMask : SQ_THREAD_TRACE_ISSUE_MASK_ALL;

        // By default don't wrap.
        m_sqtt[traceInfo.instance].mode.bits.WRAP =
            ((traceInfo.optionFlags.threadTraceWrapBuffer != 0) && traceInfo.optionValues.threadTraceWrapBuffer);

        if ((m_chipProps.gfxLevel >= GfxIpLevel::GfxIp7) || IsOland(*m_pDevice) || IsHainan(*m_pDevice))
        {
            // On gfx7+, Oland, and Hainan, we must get some SQ_THREAD_TRACE_MASK fields from our chip props.
            m_sqtt[traceInfo.instance].mask.u32All = m_chipProps.gfx6.sqThreadTraceMask;
        }

        if (traceInfo.optionFlags.threadTraceTargetCu != 0)
        {
            m_sqtt[traceInfo.instance].mask.bits.CU_SEL = traceInfo.optionValues.threadTraceTargetCu;
        }
        else
        {
            // Pick a default detailed token CU within our shader array.
            // Default to only selecting CUs that are active and not reserved for realtime use.
            uint32 traceableCuMask = ~m_chipProps.gfxip.realTimeCuMask;

            // Find intersection between non-realtime and active queues
            if (m_chipProps.gfxLevel == GfxIpLevel::GfxIp6)
            {
                // If gfx6, default to first SH on the current shader engine
                traceableCuMask &= m_chipProps.gfx6.activeCuMaskGfx6[traceInfo.instance][0];
            }
            else
            {
                traceableCuMask &= m_chipProps.gfx6.activeCuMaskGfx7[traceInfo.instance];
            }

            const int32 customDefaultSqttDetailedCuIndex = m_pDevice->Settings().defaultSqttDetailedCuIndex;

            if (customDefaultSqttDetailedCuIndex >= 0)
            {
                if (BitfieldIsSet(traceableCuMask, customDefaultSqttDetailedCuIndex))
                {
                    m_sqtt[traceInfo.instance].mask.bits.CU_SEL = customDefaultSqttDetailedCuIndex;
                }
                else
                {
                    // We can't select a non-traceable CU!
                    result = Result::ErrorInvalidValue;
                }
            }
            else
            {
                // Default to the first active CU
                uint32 firstActiveCu = 0;
                if (BitMaskScanForward(&firstActiveCu, traceableCuMask) == false)
                {
                    // We should always have at least one non-realtime CU.
                    PAL_ASSERT_ALWAYS();
                }

                m_sqtt[traceInfo.instance].mask.bits.CU_SEL = firstActiveCu;
            }

        }

        m_sqtt[traceInfo.instance].mask.bits.SH_SEL = shIndex;

        // Default to getting detailed tokens from all SIMDs.
        m_sqtt[traceInfo.instance].mask.bits.SIMD_EN = (traceInfo.optionFlags.threadTraceSimdMask != 0)
                ? traceInfo.optionValues.threadTraceSimdMask : SqttDetailedSimdMask;

        // By default we should only trace our VMID.
        m_sqtt[traceInfo.instance].mask.bits.VM_ID_MASK = (traceInfo.optionFlags.threadTraceVmIdMask != 0)
                ? traceInfo.optionValues.threadTraceVmIdMask : SQ_THREAD_TRACE_VM_ID_MASK_SINGLE;

        // By default use the max random seed.
        m_sqtt[traceInfo.instance].mask.bits.RANDOM_SEED = (traceInfo.optionFlags.threadTraceRandomSeed != 0)
                ? traceInfo.optionValues.threadTraceRandomSeed : SqttMaxRandomSeed;

        // By default enable sqtt perf counters for all CUs.
        m_sqtt[traceInfo.instance].perfMask.bits.SH0_MASK = (traceInfo.optionFlags.threadTraceSh0CounterMask != 0)
                ? traceInfo.optionValues.threadTraceSh0CounterMask : SqttPerfCounterCuMask;

        m_sqtt[traceInfo.instance].perfMask.bits.SH1_MASK = (traceInfo.optionFlags.threadTraceSh1CounterMask != 0)
                ? traceInfo.optionValues.threadTraceSh1CounterMask : SqttPerfCounterCuMask;

        if (traceInfo.optionFlags.threadTraceTokenConfig != 0)
        {
            m_sqtt[traceInfo.instance].tokenMask.u32All =
                GetSqttTokenMask(traceInfo.optionValues.threadTraceTokenConfig);
        }
        else
        {
            // By default trace all tokens and registers.
            m_sqtt[traceInfo.instance].tokenMask.bits.TOKEN_MASK = SqttTokenMaskDefault;
            m_sqtt[traceInfo.instance].tokenMask.bits.REG_MASK   = SqttRegMaskDefault;
        }

        // Only override if kernel reports we're actually able to stall.
        if ((traceInfo.optionFlags.threadTraceStallBehavior != 0) &&
            (m_sqtt[traceInfo.instance].mask.bits.SQ_STALL_EN__CI__VI == 1))
        {
            switch (traceInfo.optionValues.threadTraceStallBehavior)
            {
            case GpuProfilerStallAlways:
                // stick with default, always stall when full
                break;
            case GpuProfilerStallLoseDetail:
                // On stall, lose instruction detail until we read enough
                // This results in about 30% less stalls while still *very* unlikely to drop packets.
                m_sqtt[traceInfo.instance].tokenMask.bits.REG_DROP_ON_STALL__CI__VI = 1;
                m_sqtt[traceInfo.instance].mask.bits.REG_STALL_EN__CI__VI           = 0;
                break;
            case GpuProfilerStallNever:
                // disable stalling entirely. Be prepared for packet loss.
                m_sqtt[traceInfo.instance].mask.bits.REG_STALL_EN__CI__VI = 0;
                m_sqtt[traceInfo.instance].mask.bits.SQ_STALL_EN__CI__VI  = 0;
                m_sqtt[traceInfo.instance].mask.bits.SPI_STALL_EN__CI__VI = 0;
                break;
            default:
                PAL_NEVER_CALLED();
                break;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// It looks like the client can only call this function once per PerfExperiment which makes things simple. It must:
// - Add one SPM counter for each counter in the trace.
// - Store some global SPM state.
Result PerfExperiment::AddSpmTrace(
    const SpmTraceCreateInfo& spmCreateInfo)
{
    Result result = Result::Success;

    if (m_isFinalized)
    {
        // The perf experiment cannot be changed once it is finalized.
        result = Result::ErrorUnavailable;
    }
    else if ((spmCreateInfo.ringSize > UINT32_MAX) ||
             (IsPow2Aligned(spmCreateInfo.ringSize, SpmRingBaseAlignment) == false))
    {
        // The ring size register is only 32 bits and its value must be aligned.
        result = Result::ErrorInvalidValue;
    }
    else if ((spmCreateInfo.spmInterval < 32) || (spmCreateInfo.spmInterval > UINT16_MAX))
    {
        // The sample interval must be at least 32 and must fit in 16 bits.
        result = Result::ErrorInvalidValue;
    }
    else
    {
        // Create a SpmCounterMapping for every SPM counter.
        m_numSpmCounters = spmCreateInfo.numPerfCounters;
        m_pSpmCounters   = PAL_NEW_ARRAY(SpmCounterMapping, m_numSpmCounters, m_pPlatform, AllocObject);

        if (m_pSpmCounters == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }
        else
        {
            // The counter mappings are just POD so zero them out.
            memset(m_pSpmCounters, 0, sizeof(*m_pSpmCounters) * m_numSpmCounters);

            for (uint32 idx = 0; (result == Result::Success) && (idx < m_numSpmCounters); ++idx)
            {
                result = AddSpmCounter(spmCreateInfo.pPerfCounterInfos[idx], &m_pSpmCounters[idx]);
            }
        }
    }

    // Now the fun part: we must create a muxsel ram for every segment with SPM counters. First we figure out how
    // big each segment is and create some memory for it. Second we figure out where each SPM counter fits into its
    // segment, identifying its memory offsets and filling in its muxsel values.
    //
    // The global segment always starts with a 64-bit timestamp. Define its size in counters and the magic muxsel value
    // we use to select it.
    constexpr uint32 GlobalTimestampCounters = sizeof(uint64) / sizeof(uint16);
    constexpr uint16 GlobalTimestampSelect   = 0xF0F0;

    // Allocate the segment memory.
    for (uint32 segment = 0; (result == Result::Success) && (segment < MaxNumSpmSegments); ++segment)
    {
        // Start by calculating the total size of the ram.
        const bool isGlobalSegment = (static_cast<SpmDataSegmentType>(segment) == SpmDataSegmentType::Global);
        uint32     evenCounters    = isGlobalSegment ? GlobalTimestampCounters : 0;
        uint32     oddCounters     = 0;

        for (uint32 idx = 0; idx < m_numSpmCounters; ++idx)
        {
            if (static_cast<uint32>(m_pSpmCounters[idx].segment) == segment)
            {
                // Note that isEven and isOdd are not exclusive (e.g., 32-bit counters).
                PAL_ASSERT(m_pSpmCounters[idx].isEven || m_pSpmCounters[idx].isOdd);

                if (m_pSpmCounters[idx].isEven)
                {
                    evenCounters++;
                }

                if (m_pSpmCounters[idx].isOdd)
                {
                    oddCounters++;
                }
            }
        }

        // Get the total size in lines. Lines always go in "even, odd, even, odd..." order but we can end on any kind
        // of line. This means there are only two cases to consider: if we have more even lines or not.
        const uint32 evenLines  = RoundUpQuotient(evenCounters, MuxselLineSizeInCounters);
        const uint32 oddLines   = RoundUpQuotient(oddCounters,  MuxselLineSizeInCounters);
        const uint32 totalLines = (evenLines > oddLines) ? (2 * evenLines - 1) : (2 * oddLines);

        if (totalLines > 0)
        {
            m_numMuxselLines[segment] = totalLines;
            m_pMuxselRams[segment]    = PAL_NEW_ARRAY(SpmLineMapping, totalLines, m_pPlatform, AllocObject);

            if (m_pMuxselRams[segment] == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
            else
            {
                // The ram is POD so just zero it out. Note that zero is a muxsel mapping that means "I don't care".
                memset(m_pMuxselRams[segment], 0, sizeof(*m_pMuxselRams[segment]) * totalLines);
            }
        }
    }

    if (result == Result::Success)
    {
        // Now we know how big all of the segments are so we can figure out where each counter will fit in the sample
        // memory layout. It's time to find those offsets and fill out the muxsel values.
        for (uint32 segment = 0; segment < MaxNumSpmSegments; ++segment)
        {
            if (m_pMuxselRams[segment] != nullptr)
            {
                // Figure out where this entire segment starts in sample memory. The RLC hardware hard-codes this
                // order: Global, SE0, SE1, SE2, SE3. Add up the sizes of those segments in order until we find our
                // segment.
                //
                // Note that our layout interface expects offsets in units of 16-bit counters instead of bytes.
                // To meet that expectation our offsets are also in units of 16-bit counters.
                constexpr SpmDataSegmentType SegmentOrder[MaxNumSpmSegments] =
                {
                    SpmDataSegmentType::Global,
                    SpmDataSegmentType::Se0,
                    SpmDataSegmentType::Se1,
                    SpmDataSegmentType::Se2,
                    SpmDataSegmentType::Se3,
                };

                uint32 segmentOffset = 0;

                for (uint32 idx = 0; segment != static_cast<uint32>(SegmentOrder[idx]); ++idx)
                {
                    segmentOffset += m_numMuxselLines[static_cast<uint32>(SegmentOrder[idx])] *
                                     MuxselLineSizeInCounters;
                }

                // Walk through the even and odd lines in parallel, adding all enabled counters.
                uint32 evenCounterIdx = 0;
                uint32 evenLineIdx    = 0;
                uint32 oddCounterIdx  = 0;
                uint32 oddLineIdx     = 1;

                if (static_cast<SpmDataSegmentType>(segment) == SpmDataSegmentType::Global)
                {
                    // First, add the global timestamp selects.
                    for (uint32 idx = 0; idx < GlobalTimestampCounters; ++idx)
                    {
                        m_pMuxselRams[segment][evenLineIdx].muxsel[evenCounterIdx++].u16All = GlobalTimestampSelect;
                    }
                }

                for (uint32 idx = 0; idx < m_numSpmCounters; ++idx)
                {
                    if (static_cast<uint32>(m_pSpmCounters[idx].segment) == segment)
                    {
                        if (m_pSpmCounters[idx].isEven)
                        {
                            // If this counter has an even part it always contains the lower 16 bits.
                            m_pSpmCounters[idx].offsetLo =
                                segmentOffset + evenLineIdx * MuxselLineSizeInCounters + evenCounterIdx;

                            // Copy the counter's muxsel into the even line.
                            m_pMuxselRams[segment][evenLineIdx].muxsel[evenCounterIdx] = m_pSpmCounters[idx].evenMuxsel;

                            // Move on to the next even counter, possibly skipping over an odd line.
                            if (++evenCounterIdx == MuxselLineSizeInCounters)
                            {
                                evenCounterIdx = 0;
                                evenLineIdx += 2;
                            }
                        }

                        if (m_pSpmCounters[idx].isOdd)
                        {
                            // If this counter is even and odd it must be 32-bit and this must be the upper half.
                            // Otherwise this counter is 16-bit and it's the lower half.
                            const uint32 oddOffset =
                                segmentOffset + oddLineIdx * MuxselLineSizeInCounters + oddCounterIdx;

                            if (m_pSpmCounters[idx].isEven)
                            {
                                m_pSpmCounters[idx].offsetHi = oddOffset;
                            }
                            else
                            {
                                m_pSpmCounters[idx].offsetLo = oddOffset;
                            }

                            // Copy the counter's muxsel into the odd line.
                            m_pMuxselRams[segment][oddLineIdx].muxsel[oddCounterIdx] = m_pSpmCounters[idx].oddMuxsel;

                            // Move on to the next odd counter, possibly skipping over an even line.
                            if (++oddCounterIdx == MuxselLineSizeInCounters)
                            {
                                oddCounterIdx = 0;
                                oddLineIdx += 2;
                            }
                        }
                    }
                }
            }
        }

        // If we made it this far the SPM trace is ready to go.
        m_perfExperimentFlags.spmTraceEnabled       = true;
        m_spmRingSize                               = static_cast<uint32>(spmCreateInfo.ringSize);
        m_spmSampleInterval                         = static_cast<uint16>(spmCreateInfo.spmInterval);
    }
    else
    {
        // If some error occured do what we can to reset our state. It's too much trouble to revert each select
        // register so those counter slots are inaccessable for the lifetime of this perf experiment.
        PAL_SAFE_DELETE_ARRAY(m_pSpmCounters, m_pPlatform);

        for (uint32 idx = 0; idx < MaxNumSpmSegments; ++idx)
        {
            PAL_SAFE_DELETE_ARRAY(m_pMuxselRams[idx], m_pPlatform);
        }
    }

    return result;
}

// =====================================================================================================================
// Finalize the perf experiment by figuring out where each data section fits in the bound GPU memory.
Result PerfExperiment::Finalize()
{
    Result result = Result::Success;

    if (m_isFinalized)
    {
        // The perf experiment cannot be finalized again.
        result = Result::ErrorUnavailable;
    }
    else
    {
        // Build up the total GPU memory size by figuring out where each section needs to go.
        m_totalMemSize = 0;

        if (m_perfExperimentFlags.perfCtrsEnabled)
        {
            // Finalize the global counters by giving each one an offset within the "begin" and "end" sections. We do
            // this simply by placing the counters one after each other. In the end we will also have the total size of
            // the sections.
            gpusize globalSize = 0;

            for (uint32 idx = 0; idx < m_globalCounters.NumElements(); ++idx)
            {
                GlobalCounterMapping*const pMapping = &m_globalCounters.At(idx);
                const bool                 is64Bit  = (pMapping->general.dataType == PerfCounterDataType::Uint64);

                pMapping->offset = globalSize;
                globalSize      += is64Bit ? sizeof(uint64) : sizeof(uint32);
            }

            // Denote where the "begin" and "end" sections live in the bound GPU memory.
            m_globalBeginOffset = m_totalMemSize;
            m_globalEndOffset   = m_globalBeginOffset + globalSize;
            m_totalMemSize      = m_globalEndOffset + globalSize;
        }

        if (m_perfExperimentFlags.sqtTraceEnabled)
        {
            // Add space for each thread trace's info struct and output buffer. The output buffers have high alignment
            // requirements so we group them together after the info structs.
            for (uint32 idx = 0; idx < ArrayLen(m_sqtt); ++idx)
            {
                if (m_sqtt[idx].inUse)
                {
                    m_sqtt[idx].infoOffset = m_totalMemSize;
                    m_totalMemSize        += sizeof(ThreadTraceInfoData);
                }
            }

            // We only need to align the first buffer offset because the sizes should all be aligned.
            m_totalMemSize = Pow2Align(m_totalMemSize, SqttBufferAlignment);

            for (uint32 idx = 0; idx < ArrayLen(m_sqtt); ++idx)
            {
                if (m_sqtt[idx].inUse)
                {
                    m_sqtt[idx].bufferOffset = m_totalMemSize;
                    m_totalMemSize          += m_sqtt[idx].bufferSize;

                    PAL_ASSERT(IsPow2Aligned(m_sqtt[idx].bufferSize, SqttBufferAlignment));
                }
            }
        }

        if (m_perfExperimentFlags.spmTraceEnabled)
        {
            // Finally, add space for the SPM ring buffer.
            m_spmRingOffset = Pow2Align(m_totalMemSize, SpmRingBaseAlignment);
            m_totalMemSize  = m_spmRingOffset + m_spmRingSize;
        }

        m_isFinalized = true;
    }

    return result;
}

// =====================================================================================================================
Result PerfExperiment::GetGlobalCounterLayout(
    GlobalCounterLayout* pLayout
    ) const
{
    Result result = Result::Success;

    if (m_isFinalized == false)
    {
        // This data isn't ready until the perf experiment is finalized.
        result = Result::ErrorUnavailable;
    }
    else if (pLayout == nullptr)
    {
        result = Result::ErrorInvalidValue;
    }
    else if (pLayout->sampleCount == 0)
    {
        pLayout->sampleCount = m_globalCounters.NumElements();
    }
    else if (pLayout->sampleCount < m_globalCounters.NumElements())
    {
        result = Result::ErrorInvalidValue;
    }
    else
    {
        pLayout->sampleCount = m_globalCounters.NumElements();

        for (uint32 idx = 0; idx < m_globalCounters.NumElements(); ++idx)
        {
            const GlobalCounterMapping& mapping = m_globalCounters.At(idx);

            pLayout->samples[idx].block            = mapping.general.block;
            pLayout->samples[idx].instance         = mapping.general.globalInstance;
            pLayout->samples[idx].slot             = mapping.counterId;
            pLayout->samples[idx].eventId          = mapping.general.eventId;
            pLayout->samples[idx].dataType         = mapping.general.dataType;
            pLayout->samples[idx].beginValueOffset = m_globalBeginOffset + mapping.offset;
            pLayout->samples[idx].endValueOffset   = m_globalEndOffset + mapping.offset;
        }
    }

    return result;
}

// =====================================================================================================================
Result PerfExperiment::GetThreadTraceLayout(
    ThreadTraceLayout* pLayout
    ) const
{
    Result result = Result::Success;

    if (m_isFinalized == false)
    {
        // This data isn't ready until the perf experiment is finalized.
        result = Result::ErrorUnavailable;
    }
    else if (pLayout == nullptr)
    {
        result = Result::ErrorInvalidValue;
    }
    else
    {
        // We need the total number of actice thread traces which isn't something we store.
        uint32 numThreadTraces = 0;
        for (uint32 idx = 0; idx < ArrayLen(m_sqtt); ++idx)
        {
            numThreadTraces += m_sqtt[idx].inUse;
        }

        if (pLayout->traceCount == 0)
        {
            pLayout->traceCount = numThreadTraces;
        }
        else if (pLayout->traceCount < numThreadTraces)
        {
            result = Result::ErrorInvalidValue;
        }
        else
        {
            pLayout->traceCount = numThreadTraces;

            uint32 traceIdx = 0;
            for (uint32 idx = 0; idx < ArrayLen(m_sqtt); ++idx)
            {
                if (m_sqtt[idx].inUse)
                {
                    pLayout->traces[traceIdx].shaderEngine = idx;
                    pLayout->traces[traceIdx].computeUnit  = m_sqtt[idx].mask.bits.CU_SEL;
                    pLayout->traces[traceIdx].infoOffset   = m_sqtt[idx].infoOffset;
                    pLayout->traces[traceIdx].infoSize     = sizeof(ThreadTraceInfoData);
                    pLayout->traces[traceIdx].dataOffset   = m_sqtt[idx].bufferOffset;
                    pLayout->traces[traceIdx].dataSize     = m_sqtt[idx].bufferSize;

                    traceIdx++;
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
Result PerfExperiment::GetSpmTraceLayout(
    SpmTraceLayout* pLayout
    ) const
{
    Result result = Result::Success;

    if (m_isFinalized == false)
    {
        // This data isn't ready until the perf experiment is finalized.
        result = Result::ErrorUnavailable;
    }
    else if (pLayout == nullptr)
    {
        result = Result::ErrorInvalidValue;
    }
    else if (pLayout->numCounters == 0)
    {
        pLayout->numCounters = m_numSpmCounters;
    }
    else if (pLayout->numCounters < m_numSpmCounters)
    {
        result = Result::ErrorInvalidValue;
    }
    else
    {
        constexpr uint32 LineSizeInBytes = MuxselLineSizeInDwords * sizeof(uint32);

        pLayout->offset          = m_spmRingOffset;
        pLayout->wptrOffset      = m_spmRingOffset; // The write pointer is the first thing written to the ring buffer.
        pLayout->wptrGranularity = 1;
        pLayout->sampleOffset    = LineSizeInBytes; // The samples start one line in.

        pLayout->sampleSizeInBytes = 0;

        for (uint32 idx = 0; idx < MaxNumSpmSegments; ++idx)
        {
            pLayout->segmentSizeInBytes[idx] = m_numMuxselLines[idx] * LineSizeInBytes;
            pLayout->sampleSizeInBytes      += pLayout->segmentSizeInBytes[idx];
        }

        pLayout->numCounters = m_numSpmCounters;

        for (uint32 idx = 0; idx < m_numSpmCounters; ++idx)
        {
            pLayout->counterData[idx].segment  = m_pSpmCounters[idx].segment;
            pLayout->counterData[idx].offset   = m_pSpmCounters[idx].offsetLo;
            pLayout->counterData[idx].gpuBlock = m_pSpmCounters[idx].general.block;
            pLayout->counterData[idx].instance = m_pSpmCounters[idx].general.globalInstance;
            pLayout->counterData[idx].eventId  = m_pSpmCounters[idx].general.eventId;

            // The interface can't handle 32-bit SPM counters yet...
            PAL_ASSERT(m_pSpmCounters[idx].offsetHi == 0);
        }
    }

    return result;
}

// =====================================================================================================================
// Issues commands into the specified command stream which instruct the HW to start recording performance data.
void PerfExperiment::IssueBegin(
    GfxCmdBuffer*   pCmdBuffer,
    Pal::CmdStream* pPalCmdStream
    ) const
{
    CmdStream*const  pCmdStream = static_cast<CmdStream*>(pPalCmdStream);
    const EngineType engineType = pCmdStream->GetEngineType();

    if (m_isFinalized == false)
    {
        // It's illegal to execute a perf experiment before it's finalized.
        PAL_ASSERT_ALWAYS();
    }
    else
    {
        uint32* pCmdSpace = pCmdStream->ReserveCommands();

        // Given that we're about to change a large number of config registers we really should wait for prior work
        // (including prior perf experiments) to be idle before doing anything.
        //
        // This isn't in the docs, but we've been told by hardware engineers that we need to do a wait-idle here when
        // sampling from global counters. We might be able to remove this when global counters are disabled.
        const bool cacheFlush = ((m_createInfo.optionFlags.cacheFlushOnCounterCollection != 0) &&
                                 m_createInfo.optionValues.cacheFlushOnCounterCollection);

        pCmdSpace = WriteWaitIdle(cacheFlush, pCmdBuffer, pCmdStream, pCmdSpace);

        // Disable and reset all types of perf counters. We will enable the counters when everything is ready.
        // Note that PERFMON_ENABLE_MODE controls per-context filtering which we don't support.
        regCP_PERFMON_CNTL cpPerfmonCntl = {};
        cpPerfmonCntl.bits.PERFMON_STATE             = CP_PERFMON_STATE_DISABLE_AND_RESET;
        cpPerfmonCntl.bits.SPM_PERFMON_STATE__CI__VI = STRM_PERFMON_STATE_DISABLE_AND_RESET;
        cpPerfmonCntl.bits.PERFMON_ENABLE_MODE       = CP_PERFMON_ENABLE_MODE_ALWAYS_COUNT;

        pCmdSpace = pCmdStream->WriteSetOneConfigReg(m_registerInfo.mmCpPerfmonCntl, cpPerfmonCntl.u32All, pCmdSpace);

        if (m_chipProps.gfxLevel >= GfxIpLevel::GfxIp8)
        {
            // The RLC controls perfmon clock gating. Before doing anything else we should turn on perfmon clocks.
            regRLC_PERFMON_CLK_CNTL__VI rlcPerfmonClkCntl = {};
            rlcPerfmonClkCntl.bits.PERFMON_CLOCK_STATE = 1;

            pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmRLC_PERFMON_CLK_CNTL__VI,
                                                         rlcPerfmonClkCntl.u32All,
                                                         pCmdSpace);
        }

        // Thread traces and many types of perf counters require SQG events. To keep things simple we should just
        // enable them unconditionally. This shouldn't have any effect in the cases that don't really need them on.
        pCmdSpace = WriteUpdateSpiConfigCntl(true, pCmdStream, pCmdSpace);

        if (m_perfExperimentFlags.perfCtrsEnabled || m_perfExperimentFlags.spmTraceEnabled)
        {
            // SQ_PERFCOUNTER_CTRL controls how the SQGs increments its perf counters. We treat it as global state.
            regSQ_PERFCOUNTER_CTRL sqPerfCounterCtrl = {};

            if (m_createInfo.optionFlags.sqShaderMask != 0)
            {
                sqPerfCounterCtrl.bits.PS_EN = ((m_createInfo.optionValues.sqShaderMask & PerfShaderMaskPs) != 0);
                sqPerfCounterCtrl.bits.VS_EN = ((m_createInfo.optionValues.sqShaderMask & PerfShaderMaskVs) != 0);
                sqPerfCounterCtrl.bits.GS_EN = ((m_createInfo.optionValues.sqShaderMask & PerfShaderMaskGs) != 0);
                sqPerfCounterCtrl.bits.ES_EN = ((m_createInfo.optionValues.sqShaderMask & PerfShaderMaskEs) != 0);
                sqPerfCounterCtrl.bits.HS_EN = ((m_createInfo.optionValues.sqShaderMask & PerfShaderMaskHs) != 0);
                sqPerfCounterCtrl.bits.LS_EN = ((m_createInfo.optionValues.sqShaderMask & PerfShaderMaskLs) != 0);
                sqPerfCounterCtrl.bits.CS_EN = ((m_createInfo.optionValues.sqShaderMask & PerfShaderMaskCs) != 0);
            }
            else
            {
                // By default sample from all shader stages.
                sqPerfCounterCtrl.bits.PS_EN = 1;
                sqPerfCounterCtrl.bits.VS_EN = 1;
                sqPerfCounterCtrl.bits.GS_EN = 1;
                sqPerfCounterCtrl.bits.ES_EN = 1;
                sqPerfCounterCtrl.bits.HS_EN = 1;
                sqPerfCounterCtrl.bits.LS_EN = 1;
                sqPerfCounterCtrl.bits.CS_EN = 1;
            }

            // Note that we must write this after CP_PERFMON_CNTRL because the CP ties ownership of this state to it.
            pCmdSpace = pCmdStream->WriteSetOneConfigReg(m_registerInfo.mmSqPerfCounterCtrl,
                                                         sqPerfCounterCtrl.u32All,
                                                         pCmdSpace);
        }

        if (m_perfExperimentFlags.spmTraceEnabled)
        {
            // Fully configure the RLC SPM state. There's a lot of code for this so it's in a helper function.
            pCmdSpace = WriteSpmSetup(pCmdStream, pCmdSpace);
        }

        if (m_perfExperimentFlags.perfCtrsEnabled || m_perfExperimentFlags.spmTraceEnabled)
        {
            // Write the necessary PERFCOUNTER#_SELECT registers. This is another huge chunk of code in a helper
            // function. This state is shared between SPM counters and global counters.
            pCmdSpace = WriteSelectRegisters(pCmdStream, pCmdSpace);
        }

        if (m_perfExperimentFlags.sqtTraceEnabled)
        {
            // Setup all thread traces and start tracing.
            pCmdSpace = WriteStartThreadTraces(pCmdStream, pCmdSpace);

            // The old perf experiment code did a PS_PARTIAL_FLUSH and a wait-idle here because it "seems to help us
            // more reliably gather thread-trace data". That doesn't make any sense and isn't backed-up by any of the
            // HW programming guides. It has been duplicated here to avoid initial regressions but should be removed.
            if (m_pDevice->EngineSupportsGraphics(engineType))
            {
                pCmdSpace += m_cmdUtil.BuildEventWrite(PS_PARTIAL_FLUSH, pCmdSpace);
            }

            pCmdSpace = WriteWaitIdle(false, pCmdBuffer, pCmdStream, pCmdSpace);
        }

        if (m_perfExperimentFlags.perfCtrsEnabled)
        {
            // This will transition the counter state from "reset" to "stop" and take the begin samples. It will
            // also reset all counters that have convenient reset bits in their config registers.
            pCmdSpace = WriteStopAndSampleGlobalCounters(true, pCmdBuffer, pCmdStream, pCmdSpace);
        }

        // Tell the SPM counters and global counters start counting.
        if (m_perfExperimentFlags.perfCtrsEnabled || m_perfExperimentFlags.spmTraceEnabled)
        {
            // CP_PERFMON_CNTL only enables non-windowed counters.
            if (m_perfExperimentFlags.perfCtrsEnabled)
            {
                cpPerfmonCntl.bits.PERFMON_STATE = CP_PERFMON_STATE_START_COUNTING;
            }

            if (m_perfExperimentFlags.spmTraceEnabled)
            {
                cpPerfmonCntl.bits.SPM_PERFMON_STATE__CI__VI = STRM_PERFMON_STATE_START_COUNTING;
            }

            pCmdSpace = pCmdStream->WriteSetOneConfigReg(m_registerInfo.mmCpPerfmonCntl,
                                                         cpPerfmonCntl.u32All,
                                                         pCmdSpace);

            // Also enable windowed perf counters. This most likely applies to many block types, rather than try to
            // find them all just always send the event (it shouldn't hurt). This should be required by legacy counters
            // and SPM counters.
            pCmdSpace = WriteUpdateWindowedCounters(true, pCmdStream, pCmdSpace);

            // Enable all of the special global config registers. Only clear the counters if we didn't call
            // WriteStopAndSampleGlobalCounters which already clears them and assumes we're not going to reset the
            // counters again after taking the initial sample.
            pCmdSpace = WriteEnableCfgRegisters(true, (m_perfExperimentFlags.perfCtrsEnabled == false), pCmdStream, pCmdSpace);
        }

        pCmdStream->CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Issues commands into the specified command stream which instruct the HW to stop recording performance data.
void PerfExperiment::IssueEnd(
    GfxCmdBuffer*   pCmdBuffer,
    Pal::CmdStream* pPalCmdStream
    ) const
{
    CmdStream*const  pCmdStream = static_cast<CmdStream*>(pPalCmdStream);
    const EngineType engineType = pCmdStream->GetEngineType();

    if (m_isFinalized == false)
    {
        // It's illegal to execute a perf experiment before it's finalized.
        PAL_ASSERT_ALWAYS();
    }
    else
    {
        uint32* pCmdSpace = pCmdStream->ReserveCommands();

        // This isn't in the docs, but we've been told by hardware engineers that we need to do a wait-idle here when
        // sampling from global counters. We might be able to remove this when global counters are disabled.
        const bool cacheFlush = ((m_createInfo.optionFlags.cacheFlushOnCounterCollection != 0) &&
                                 m_createInfo.optionValues.cacheFlushOnCounterCollection);

        pCmdSpace = WriteWaitIdle(cacheFlush, pCmdBuffer, pCmdStream, pCmdSpace);

        // This is the CP_PERFMON_CNTL state that should be currently active.
        if (m_perfExperimentFlags.perfCtrsEnabled)
        {
            // This will transition the counter state from "start" to "stop" and take the end samples.
            pCmdSpace = WriteStopAndSampleGlobalCounters(false, pCmdBuffer, pCmdStream, pCmdSpace);
        }
        else if (m_perfExperimentFlags.spmTraceEnabled)
        {
            // If SPM is enabled but we didn't call WriteSampleGlobalCounters we still need to disable these manually.
            pCmdSpace = WriteUpdateWindowedCounters(false, pCmdStream, pCmdSpace);
            pCmdSpace = WriteEnableCfgRegisters(false, false, pCmdStream, pCmdSpace);

            // The docs don't say we need to stop SPM, transitioning directly from start to disabled seems legal.
            // We stop the SPM counters anyway for parity with the global counter path and because it looks good.
            regCP_PERFMON_CNTL cpPerfmonCntl = {};
            cpPerfmonCntl.bits.PERFMON_STATE             = CP_PERFMON_STATE_DISABLE_AND_RESET;
            cpPerfmonCntl.bits.SPM_PERFMON_STATE__CI__VI = STRM_PERFMON_STATE_STOP_COUNTING;

            pCmdSpace = pCmdStream->WriteSetOneConfigReg(m_registerInfo.mmCpPerfmonCntl,
                                                         cpPerfmonCntl.u32All,
                                                         pCmdSpace);
        }

        if (m_perfExperimentFlags.sqtTraceEnabled)
        {
            // Stop all thread traces and copy back some information not contained in the thread trace tokens.
            pCmdSpace = WriteStopThreadTraces(pCmdStream, pCmdSpace);
        }

        if (m_perfExperimentFlags.spmTraceEnabled)
        {
            // The old perf experiment code did a wait-idle between stopping SPM and resetting things. It said that
            // the RLC can page fault on its remaining writes if we reset things too early. This requirement isn't
            // captured in any HW programming docs but it does seem like a reasonable concern.
            pCmdSpace = WriteWaitIdle(false, pCmdBuffer, pCmdStream, pCmdSpace);
        }

        // Start disabling and resetting state that we need to clean up. Note that things like the select registers
        // can be left alone because the counters won't do anything unless the global enable switches are on.

        // Throw the master disable-and-reset switch.
        regCP_PERFMON_CNTL cpPerfmonCntl = {};
        cpPerfmonCntl.bits.PERFMON_STATE             = CP_PERFMON_STATE_DISABLE_AND_RESET;
        cpPerfmonCntl.bits.SPM_PERFMON_STATE__CI__VI = STRM_PERFMON_STATE_DISABLE_AND_RESET;

        pCmdSpace = pCmdStream->WriteSetOneConfigReg(m_registerInfo.mmCpPerfmonCntl, cpPerfmonCntl.u32All, pCmdSpace);

        // Restore SPI_CONFIG_CNTL by turning SQG events back off.
        pCmdSpace = WriteUpdateSpiConfigCntl(false, pCmdStream, pCmdSpace);

        if (m_chipProps.gfxLevel >= GfxIpLevel::GfxIp8)
        {
            // The RLC controls perfmon clock gating. Before we're done here, we must turn the perfmon clocks back off.
            regRLC_PERFMON_CLK_CNTL__VI rlcPerfmonClkCntl = {};
            rlcPerfmonClkCntl.bits.PERFMON_CLOCK_STATE = 0;

            pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmRLC_PERFMON_CLK_CNTL__VI,
                                                         rlcPerfmonClkCntl.u32All,
                                                         pCmdSpace);
        }

        pCmdStream->CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Issues commands into the specified command stream which instruct the HW to pause recording performance data.
void PerfExperiment::BeginInternalOps(
    Pal::CmdStream* pPalCmdStream
    ) const
{
    CmdStream*const  pCmdStream = static_cast<CmdStream*>(pPalCmdStream);
    const EngineType engineType = pCmdStream->GetEngineType();

    if (m_isFinalized == false)
    {
        // It's illegal to execute a perf experiment before it's finalized.
        PAL_ASSERT_ALWAYS();
    }
    // We don't pause by default, the client has to explicitly ask us to not sample internal operations.
    else if ((m_createInfo.optionFlags.sampleInternalOperations != 0) &&
             (m_createInfo.optionValues.sampleInternalOperations == false))
    {
        uint32* pCmdSpace = pCmdStream->ReserveCommands();

        // Issue the necessary commands to stop counter collection (SPM and global counters) without resetting
        // any counter programming.

        // First stop windowed counters, then stop global counters.
        pCmdSpace = WriteUpdateWindowedCounters(false, pCmdStream, pCmdSpace);

        // NOTE: We probably should add a wait-idle here. If we don't wait the global counters will stop counting
        // while the prior draw/dispatch is still active which will under count. There is no wait here currently
        // because the old perf experiment code didn't wait.

        // Write CP_PERFMON_CNTL such that SPM and global counters stop counting.
        regCP_PERFMON_CNTL cpPerfmonCntl = {};
        cpPerfmonCntl.bits.PERFMON_STATE             =
            m_perfExperimentFlags.perfCtrsEnabled ? CP_PERFMON_STATE_STOP_COUNTING
                                                  : CP_PERFMON_STATE_DISABLE_AND_RESET;
        cpPerfmonCntl.bits.SPM_PERFMON_STATE__CI__VI =
            m_perfExperimentFlags.spmTraceEnabled ? STRM_PERFMON_STATE_STOP_COUNTING
                                                  : STRM_PERFMON_STATE_DISABLE_AND_RESET;

        pCmdSpace = pCmdStream->WriteSetOneConfigReg(m_registerInfo.mmCpPerfmonCntl, cpPerfmonCntl.u32All, pCmdSpace);

        // Stop the cfg-style counters too. It's not clear if these are included in the above guidelines so just stop
        // them at the end to be safe.
        pCmdSpace = WriteEnableCfgRegisters(false, false, pCmdStream, pCmdSpace);

        pCmdStream->CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Issues commands into the specified command stream which instruct the HW to resume recording performance data.
void PerfExperiment::EndInternalOps(
    Pal::CmdStream* pPalCmdStream
    ) const
{
    CmdStream*const  pCmdStream = static_cast<CmdStream*>(pPalCmdStream);
    const EngineType engineType = pCmdStream->GetEngineType();

    if (m_isFinalized == false)
    {
        // It's illegal to execute a perf experiment before it's finalized.
        PAL_ASSERT_ALWAYS();
    }
    // Submit the resume commands under the same condition that we issued the pause commands.
    else if ((m_createInfo.optionFlags.sampleInternalOperations != 0) &&
             (m_createInfo.optionValues.sampleInternalOperations == false))
    {
        uint32* pCmdSpace = pCmdStream->ReserveCommands();

        // NOTE: We probably should add a wait-idle here. If we don't wait the global counters will start counting
        // while the internal draw/dispatch is still active and it will be counted. There is no wait here currently
        // because the old perf experiment code didn't wait.

        // Rewrite the "start" state for all counters.
        regCP_PERFMON_CNTL cpPerfmonCntl = {};
        cpPerfmonCntl.bits.PERFMON_STATE             =
            m_perfExperimentFlags.perfCtrsEnabled ? CP_PERFMON_STATE_START_COUNTING
                                                  : CP_PERFMON_STATE_DISABLE_AND_RESET;
        cpPerfmonCntl.bits.SPM_PERFMON_STATE__CI__VI =
            m_perfExperimentFlags.spmTraceEnabled ? STRM_PERFMON_STATE_START_COUNTING
                                                  : STRM_PERFMON_STATE_DISABLE_AND_RESET;
        cpPerfmonCntl.bits.PERFMON_ENABLE_MODE       = CP_PERFMON_ENABLE_MODE_ALWAYS_COUNT;

        pCmdSpace = pCmdStream->WriteSetOneConfigReg(m_registerInfo.mmCpPerfmonCntl, cpPerfmonCntl.u32All, pCmdSpace);
        pCmdSpace = WriteUpdateWindowedCounters(true, pCmdStream, pCmdSpace);
        pCmdSpace = WriteEnableCfgRegisters(true, false, pCmdStream, pCmdSpace);

        pCmdStream->CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Issues update commands into the specified command stream which instruct the HW to modify the sqtt token mask
// and register mask for each active thread trace.
void PerfExperiment::UpdateSqttTokenMask(
    Pal::CmdStream*               pPalCmdStream,
    const ThreadTraceTokenConfig& sqttTokenConfig
    ) const
{
    CmdStream*const pCmdStream = static_cast<CmdStream*>(pPalCmdStream);

    if (m_isFinalized == false)
    {
        // It's illegal to execute a perf experiment before it's finalized.
        PAL_ASSERT_ALWAYS();
    }
    else if (m_perfExperimentFlags.sqtTraceEnabled)
    {
        uint32* pCmdSpace = pCmdStream->ReserveCommands();

        for (uint32 idx = 0; idx < ArrayLen(m_sqtt); ++idx)
        {
            if (m_sqtt[idx].inUse)
            {
                pCmdSpace = pCmdStream->WriteSetOneConfigReg(m_registerInfo.mmGrbmGfxIndex,
                                                             m_sqtt[idx].grbmGfxIndex.u32All,
                                                             pCmdSpace);

                regSQ_THREAD_TRACE_TOKEN_MASK tokenMask = {};
                tokenMask.u32All = GetSqttTokenMask(sqttTokenConfig);

                // This field isn't controlled by the token config.
                tokenMask.bits.REG_DROP_ON_STALL__CI__VI = m_sqtt[idx].tokenMask.bits.REG_DROP_ON_STALL__CI__VI;

                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(m_registerInfo.mmSqThreadTraceTokenMask,
                                                              m_sqtt[idx].tokenMask.u32All,
                                                              pCmdSpace);
            }
        }

        // Switch back to global broadcasting before returning to the rest of PAL.
        pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);

        pCmdStream->CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Issues update commands into the specified command stream which instruct the HW to modify the sqtt token mask
// and register mask any active thread traces.
//
// Updates the SQTT token mask for all SEs outside of a specific PerfExperiment.  Used by GPA Session when targeting
// a single event for instruction level trace during command buffer building.
void PerfExperiment::UpdateSqttTokenMaskStatic(
    Pal::CmdStream*               pPalCmdStream,
    const ThreadTraceTokenConfig& sqttTokenConfig,
    const Device&                 device)
{
    CmdStream*const pCmdStream = static_cast<CmdStream*>(pPalCmdStream);
    uint32*         pCmdSpace  = pCmdStream->ReserveCommands();

    regSQ_THREAD_TRACE_TOKEN_MASK tokenMask = {};
    tokenMask.u32All = GetSqttTokenMask(sqttTokenConfig);

    // Note that we will lose the current value of the REG_DROP_ON_STALL field.
    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(device.CmdUtil().GetRegInfo().mmSqThreadTraceTokenMask,
                                                  tokenMask.u32All,
                                                  pCmdSpace);

    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Fills out a CounterMapping based on an interface perf counter. It also validates the counter information.
Result PerfExperiment::BuildCounterMapping(
    const PerfCounterInfo& info,
    CounterMapping*        pMapping
    ) const
{
    Result result = Result::Success;

    if (info.block >= GpuBlock::Count)
    {
        // What is this block?
        result = Result::ErrorInvalidValue;
    }
    else if (m_counterInfo.block[static_cast<uint32>(info.block)].distribution == PerfCounterDistribution::Unavailable)
    {
        // This block is not available on this GPU.
        result = Result::ErrorInvalidValue;
    }
    else if (info.instance > m_counterInfo.block[static_cast<uint32>(info.block)].numGlobalInstances)
    {
        // This instance doesn't exist.
        result = Result::ErrorInvalidValue;
    }
    else if (info.eventId > m_counterInfo.block[static_cast<uint32>(info.block)].maxEventId)
    {
        // This event doesn't exist.
        result = Result::ErrorInvalidValue;
    }
    else
    {
        // Fill out the mapping struct.
        pMapping->block          = info.block;
        pMapping->globalInstance = info.instance;
        pMapping->eventId        = info.eventId;
    }

    return result;
}

// =====================================================================================================================
// Fills out an InstanceMapping for some block based on a global instance value. It will also validate that the global
// instance has a valid internal instance index.
Result PerfExperiment::BuildInstanceMapping(
    GpuBlock         block,
    uint32           globalInstance,
    InstanceMapping* pMapping
    ) const
{
    Result result        = Result::Success;
    uint32 seIndex       = 0;
    uint32 saIndex       = 0;
    uint32 instanceIndex = 0;

    const PerfCounterBlockInfo& blockInfo = m_counterInfo.block[static_cast<uint32>(block)];

    if (blockInfo.distribution == PerfCounterDistribution::GlobalBlock)
    {
        // Global blocks have a one-to-one instance mapping.
        instanceIndex = globalInstance;
    }
    else if (blockInfo.distribution == PerfCounterDistribution::PerShaderEngine)
    {
        // We want the SE index to be the outer index and the local instance to be the inner index.
        seIndex       = globalInstance / blockInfo.numInstances;
        instanceIndex = globalInstance % blockInfo.numInstances;
    }
    else if (blockInfo.distribution == PerfCounterDistribution::PerShaderArray)
    {
        // From outermost to innermost, the internal indices are in the order: SE, SA, local instance.
        seIndex       = (globalInstance / blockInfo.numInstances) / m_chipProps.gfx6.numShaderArrays;
        saIndex       = (globalInstance / blockInfo.numInstances) % m_chipProps.gfx6.numShaderArrays;
        instanceIndex = globalInstance % blockInfo.numInstances;
    }

    if (seIndex >= m_chipProps.gfx6.numShaderEngines)
    {
        // This shader engine doesn't exist on our device.
        result = Result::ErrorInvalidValue;
    }
    else if (saIndex >= m_chipProps.gfx6.numShaderArrays)
    {
        // This shader array doesn't exist on our device.
        result = Result::ErrorInvalidValue;
    }
    else if (instanceIndex >= blockInfo.numInstances)
    {
        // This instance doesn't exist on our device.
        result = Result::ErrorInvalidValue;
    }
    else
    {
        PAL_ASSERT(pMapping != nullptr);

        pMapping->seIndex       = seIndex;
        pMapping->saIndex       = saIndex;
        pMapping->instanceIndex = instanceIndex;
    }

    return result;
}

// =====================================================================================================================
// Assuming this is an SQ counter select, return true if it's a "LEVEL" counter, which require special SPM handling.
bool PerfExperiment::IsSqLevelEvent(
    uint32 eventId
    ) const
{
    bool isLevelEvent = false;

    // We should only try to configure SPM on gfx7+.
    PAL_ASSERT(m_chipProps.gfxLevel >= GfxIpLevel::GfxIp7);

    if (eventId == SQ_PERF_SEL_LEVEL_WAVES)
    {
        isLevelEvent = true;
    }
    else if (eventId == SQ_PERF_SEL_LEVEL_WAVES_CU__CI__VI)
    {
        isLevelEvent = true;
    }
    else if ((eventId >= SQ_PERF_SEL_INST_LEVEL_VMEM__CI__VI) &&
             (eventId <= SQ_PERF_SEL_INST_LEVEL_EXP__CI__VI))
    {
        isLevelEvent = true;
    }
    else if (eventId == SQ_PERF_SEL_IFETCH_LEVEL__CI__VI)
    {
        isLevelEvent = true;
    }
    else if ((eventId >= SQ_PERF_SEL_USER_LEVEL0__CI__VI) &&
             (eventId <= SQ_PERF_SEL_USER_LEVEL15__CI__VI))
    {
        isLevelEvent = true;
    }
    else if (m_chipProps.gfxLevel == GfxIpLevel::GfxIp7)
    {
        if ((eventId >= SQC_PERF_SEL_ICACHE_INFLIGHT_LEVEL__CI) &&
            (eventId <= SQC_PERF_SEL_DCACHE_TC_INFLIGHT_LEVEL__CI))
        {
            isLevelEvent = true;
        }
    }
    else
    {
        if ((eventId >= SQC_PERF_SEL_ICACHE_INFLIGHT_LEVEL__VI) &&
            (eventId <= SQC_PERF_SEL_DCACHE_TC_INFLIGHT_LEVEL__VI))
        {
            isLevelEvent = true;
        }
        else if (eventId == SQ_PERF_SEL_ATC_INST_LEVEL_VMEM__VI)
        {
            isLevelEvent = true;
        }
        else if (eventId == SQ_PERF_SEL_ATC_INST_LEVEL_SMEM__VI)
        {
            isLevelEvent = true;
        }
    }

    return isLevelEvent;
}

// =====================================================================================================================
// Fills out a GRBM_GFX_INDEX for some block based on an InstanceMapping
regGRBM_GFX_INDEX PerfExperiment::BuildGrbmGfxIndex(
    const InstanceMapping& mapping,
    GpuBlock               block
    ) const
{
    regGRBM_GFX_INDEX grbmGfxIndex = {};
    grbmGfxIndex.bits.SE_INDEX       = mapping.seIndex;
    grbmGfxIndex.bits.SH_INDEX       = mapping.saIndex;
    grbmGfxIndex.bits.INSTANCE_INDEX = mapping.instanceIndex;

    switch (m_counterInfo.block[static_cast<uint32>(block)].distribution)
    {
    case PerfCounterDistribution::GlobalBlock:
        // Global block writes should broadcast to SEs and SAs.
        grbmGfxIndex.bits.SE_BROADCAST_WRITES  = 1;
        // Intentional fall through
    case PerfCounterDistribution::PerShaderEngine:
        // Per-SE block writes should broadcast to SAs.
        grbmGfxIndex.bits.SH_BROADCAST_WRITES = 1;
        break;

    default:
        // Otherwise no broadcast bits should be set.
        break;
    }

    return grbmGfxIndex;
}

// =====================================================================================================================
// A helper function for IssueBegin which writes the necessary commands to setup SPM. This essentially boils down to:
// - Program the RLC's control registers.
// - Upload each muxsel ram.
uint32* PerfExperiment::WriteSpmSetup(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    // Configure the CP and RLC state that controls SPM.
    struct
    {
        regRLC_SPM_PERFMON_CNTL__CI__VI         cntl;
        regRLC_SPM_PERFMON_RING_BASE_LO__CI__VI ringBaseLo;
        regRLC_SPM_PERFMON_RING_BASE_HI__CI__VI ringBaseHi;
        regRLC_SPM_PERFMON_RING_SIZE__CI__VI    ringSize;
        regRLC_SPM_PERFMON_SEGMENT_SIZE__CI__VI segmentSize;
    } rlcInit = {};

    const gpusize ringBaseAddr = m_gpuMemory.GpuVirtAddr() + m_spmRingOffset;

    // The spec requires that the ring address and size be aligned to 32-bytes.
    PAL_ASSERT(IsPow2Aligned(ringBaseAddr,  SpmRingBaseAlignment));
    PAL_ASSERT(IsPow2Aligned(m_spmRingSize, SpmRingBaseAlignment));

    rlcInit.cntl.bits.PERFMON_RING_MODE       = 0; // No stall and no interupt on overflow.
    rlcInit.cntl.bits.PERFMON_SAMPLE_INTERVAL = m_spmSampleInterval;
    rlcInit.ringBaseLo.bits.RING_BASE_LO      = LowPart(ringBaseAddr);
    rlcInit.ringBaseHi.bits.RING_BASE_HI      = HighPart(ringBaseAddr);
    rlcInit.ringSize.bits.RING_BASE_SIZE      = m_spmRingSize;

    // Program the muxsel line sizes. Note that PERFMON_SEGMENT_SIZE only has space for 31 lines per segment.
    bool   over31Lines = false;
    uint32 totalLines  = 0;

    for (uint32 idx = 0; idx < MaxNumSpmSegments; ++idx)
    {
        over31Lines = over31Lines || (m_numMuxselLines[idx] > 31);
        totalLines += m_numMuxselLines[idx];
    }

    // We have no way to handle more than 31 lines. Assert so that the user knows this is broken but continue
    // anyway and hope to maybe get some partial data.
    PAL_ASSERT(over31Lines == false);

    rlcInit.segmentSize.bits.PERFMON_SEGMENT_SIZE = totalLines;
    rlcInit.segmentSize.bits.SE0_NUM_LINE         = m_numMuxselLines[0];
    rlcInit.segmentSize.bits.SE1_NUM_LINE         = m_numMuxselLines[1];
    rlcInit.segmentSize.bits.SE2_NUM_LINE         = m_numMuxselLines[2];
    rlcInit.segmentSize.bits.GLOBAL_NUM_LINE      = m_numMuxselLines[static_cast<uint32>(SpmDataSegmentType::Global)];

    pCmdSpace = pCmdStream->WriteSetSeqConfigRegs(mmRLC_SPM_PERFMON_CNTL__CI__VI,
                                                  mmRLC_SPM_PERFMON_SEGMENT_SIZE__CI__VI,
                                                  &rlcInit,
                                                  pCmdSpace);

    // Now upload each muxsel ram to the RLC. If a particular segment is empty we skip it.
    for (uint32 idx = 0; idx < MaxNumSpmSegments; ++idx)
    {
        if (m_numMuxselLines[idx] > 0)
        {
            WriteDataInfo writeData = {};
            uint32        muxselAddr = 0;

            if (static_cast<SpmDataSegmentType>(idx) == SpmDataSegmentType::Global)
            {
                pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);

                writeData.dstAddr = mmRLC_SPM_GLOBAL_MUXSEL_DATA__CI__VI;
                muxselAddr        = mmRLC_SPM_GLOBAL_MUXSEL_ADDR__CI__VI;
            }
            else
            {
                pCmdSpace = WriteGrbmGfxIndexBroadcastSe(idx, pCmdStream, pCmdSpace);

                writeData.dstAddr = mmRLC_SPM_SE_MUXSEL_DATA__CI__VI;
                muxselAddr        = mmRLC_SPM_SE_MUXSEL_ADDR__CI__VI;
            }

            writeData.engineSel = WRITE_DATA_ENGINE_ME;
            writeData.dstSel    = WRITE_DATA_DST_SEL_REGISTER;

            // Each data value must be written into MUXSEL_DATA, if we let the CP increment the register address
            // we will overwrite other registers.
            writeData.dontIncrementAddr = true;

            // The muxsel ram is inlined into the command stream and could be large so we need a loop that carefully
            // splits it into chunks and repeatedly commits and reserves space.
            constexpr uint32 PacketHeaders = CmdUtil::GetSetDataHeaderSize() + 1 + CmdUtil::GetWriteDataHeaderSize();
            const uint32     maxDwords     = pCmdStream->ReserveLimit() - PacketHeaders;
            const uint32     maxLines      = maxDwords / MuxselLineSizeInDwords;

            for (uint32 line = 0; line < m_numMuxselLines[idx]; line += maxLines)
            {
                const uint32 numLines = Min(maxLines, m_numMuxselLines[idx] - line);

                pCmdStream->CommitCommands(pCmdSpace);
                pCmdSpace = pCmdStream->ReserveCommands();

                // Each time we issue a new write_data we must first update MUXSEL_ADDR to point to the next muxsel.
                pCmdSpace  = pCmdStream->WriteSetOneConfigReg(muxselAddr, line * MuxselLineSizeInDwords, pCmdSpace);
                pCmdSpace += m_cmdUtil.BuildWriteData(writeData,
                                                      numLines * MuxselLineSizeInDwords,
                                                      m_pMuxselRams[idx][line].u32Array,
                                                      pCmdSpace);

                pCmdStream->CommitCommands(pCmdSpace);
                pCmdSpace = pCmdStream->ReserveCommands();
            }
        }
    }

    static_assert(static_cast<uint32>(SpmDataSegmentType::Global) == static_cast<uint32>(SpmDataSegmentType::Count) - 1,
                  "We assume the global SPM segment writes its registers last which restores global broadcasting.");

    return pCmdSpace;
}

// =====================================================================================================================
// A helper function for IssueBegin which writes the necessary commands to start all thread traces.
uint32* PerfExperiment::WriteStartThreadTraces(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    for (uint32 idx = 0; idx < ArrayLen(m_sqtt); ++idx)
    {
        if (m_sqtt[idx].inUse)
        {
            // Get fresh command space once per trace, just in case.
            pCmdStream->CommitCommands(pCmdSpace);
            pCmdSpace = pCmdStream->ReserveCommands();

            pCmdSpace = pCmdStream->WriteSetOneConfigReg(m_registerInfo.mmGrbmGfxIndex,
                                                         m_sqtt[idx].grbmGfxIndex.u32All,
                                                         pCmdSpace);

            const gpusize shiftedAddr = (m_gpuMemory.GpuVirtAddr() + m_sqtt[idx].bufferOffset) >> SqttBufferAlignShift;
            const gpusize shiftedSize = m_sqtt[idx].bufferSize >> SqttBufferAlignShift;

            // These four registers must be written first in this specific order.
            regSQ_THREAD_TRACE_BASE2__CI__VI sqttBase2 = {};
            regSQ_THREAD_TRACE_BASE          sqttBase  = {};
            regSQ_THREAD_TRACE_SIZE          sqttSize  = {};
            regSQ_THREAD_TRACE_CTRL          sqttCtrl  = {};

            sqttBase2.bits.ADDR_HI     = HighPart(shiftedAddr);
            sqttBase.bits.ADDR         = LowPart(shiftedAddr);
            sqttSize.bits.SIZE         = shiftedSize;
            sqttCtrl.bits.RESET_BUFFER = 1;

            if (m_chipProps.gfxLevel >= GfxIpLevel::GfxIp7)
            {
                // This doesn't exist on gfx6.
                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(m_registerInfo.mmSqThreadTraceBase2,
                                                              sqttBase2.u32All,
                                                              pCmdSpace);
            }

            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(m_registerInfo.mmSqThreadTraceBase,
                                                          sqttBase.u32All,
                                                          pCmdSpace);

            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(m_registerInfo.mmSqThreadTraceSize,
                                                          sqttSize.u32All,
                                                          pCmdSpace);

            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(m_registerInfo.mmSqThreadTraceCtrl,
                                                          sqttCtrl.u32All,
                                                          pCmdSpace);

            // These registers can be in any order.
            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(m_registerInfo.mmSqThreadTraceMask,
                                                          m_sqtt[idx].mask.u32All,
                                                          pCmdSpace);

            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(m_registerInfo.mmSqThreadTraceTokenMask,
                                                          m_sqtt[idx].tokenMask.u32All,
                                                          pCmdSpace);

            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(m_registerInfo.mmSqThreadTracePerfMask,
                                                          m_sqtt[idx].perfMask.u32All,
                                                          pCmdSpace);

            // The INST_MASK changes size in each gfxip version.
            if (m_chipProps.gfxLevel == GfxIpLevel::GfxIp7)
            {
                regSQ_THREAD_TRACE_TOKEN_MASK2__CI sqttTokenMask2 = {};
                sqttTokenMask2.bits.INST_MASK = Gfx7SqttInstMaskDefault;

                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_TOKEN_MASK2__CI,
                                                              sqttTokenMask2.u32All,
                                                              pCmdSpace);
            }
            else if (m_chipProps.gfxLevel >= GfxIpLevel::GfxIp8)
            {
                regSQ_THREAD_TRACE_TOKEN_MASK2__VI sqttTokenMask2 = {};
                sqttTokenMask2.bits.INST_MASK = Gfx8SqttInstMaskDefault;

                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_TOKEN_MASK2__VI,
                                                              sqttTokenMask2.u32All,
                                                              pCmdSpace);
            }

            regSQ_THREAD_TRACE_HIWATER sqttHiwater = {};
            sqttHiwater.bits.HIWATER = SqttHiWaterValue;

            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(m_registerInfo.mmSqThreadTraceHiWater,
                                                          sqttHiwater.u32All,
                                                          pCmdSpace);

            // We must write this register last because it turns on thread traces.
            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(m_registerInfo.mmSqThreadTraceMode,
                                                          m_sqtt[idx].mode.u32All,
                                                          pCmdSpace);
        }
    }

    // Start the thread traces. Note that CI+ should have the COMPUTE_THREAD_TRACE_ENABLE register but the docs don't
    // say anything about how we should use it. The old perf experiment code also didn't use it.
    pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);
    pCmdSpace += m_cmdUtil.BuildEventWrite(THREAD_TRACE_START, pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
// A helper function for IssueEnd which writes the necessary commands to stop all thread traces.
uint32* PerfExperiment::WriteStopThreadTraces(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    // Stop the thread traces. Note that CI+ should have the COMPUTE_THREAD_TRACE_ENABLE register but the docs don't
    // say anything about how we should use it. The old perf experiment code also didn't use it.
    pCmdSpace += m_cmdUtil.BuildEventWrite(THREAD_TRACE_STOP, pCmdSpace);
    pCmdSpace += m_cmdUtil.BuildEventWrite(THREAD_TRACE_FINISH, pCmdSpace);

    for (uint32 idx = 0; idx < ArrayLen(m_sqtt); ++idx)
    {
        if (m_sqtt[idx].inUse)
        {
            // Get fresh command space once per trace, just in case.
            pCmdStream->CommitCommands(pCmdSpace);
            pCmdSpace = pCmdStream->ReserveCommands();

            pCmdSpace = pCmdStream->WriteSetOneConfigReg(m_registerInfo.mmGrbmGfxIndex,
                                                         m_sqtt[idx].grbmGfxIndex.u32All,
                                                         pCmdSpace);

            // Set the mode to "OFF".
            regSQ_THREAD_TRACE_MODE sqttMode = m_sqtt[idx].mode;
            sqttMode.bits.MODE = SQ_THREAD_TRACE_MODE_OFF;

            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(m_registerInfo.mmSqThreadTraceMode,
                                                          sqttMode.u32All,
                                                          pCmdSpace);

            // Poll the status register's busy bit to ensure that no events are being logged and written to memory.
            pCmdSpace += m_cmdUtil.BuildWaitRegMem(WAIT_REG_MEM_SPACE_REGISTER,
                                                   WAIT_REG_MEM_FUNC_EQUAL,
                                                   WAIT_REG_MEM_ENGINE_ME,
                                                   m_registerInfo.mmSqThreadTraceStatus,
                                                   0,
                                                   SQ_THREAD_TRACE_STATUS__BUSY_MASK,
                                                   false,
                                                   pCmdSpace);

            // Use COPY_DATA to read back the info struct one DWORD at a time.
            const gpusize infoAddr = m_gpuMemory.GpuVirtAddr() + m_sqtt[idx].infoOffset;

            // If each member doesn't start at a DWORD offset this won't work.
            static_assert(offsetof(ThreadTraceInfoData, curOffset)    == 0,                  "");
            static_assert(offsetof(ThreadTraceInfoData, traceStatus)  == sizeof(uint32),     "");
            static_assert(offsetof(ThreadTraceInfoData, writeCounter) == sizeof(uint32) * 2, "");

            const uint32 infoRegisters[] =
            {
                m_registerInfo.mmSqThreadTraceWptr,
                m_registerInfo.mmSqThreadTraceStatus,
                mmSQ_THREAD_TRACE_CNTR
            };

            for (uint32 regIdx = 0; regIdx < ArrayLen(infoRegisters); regIdx++)
            {
                pCmdSpace += m_cmdUtil.BuildCopyData(COPY_DATA_SEL_DST_ASYNC_MEMORY,
                                                     infoAddr + regIdx * sizeof(uint32),
                                                     COPY_DATA_SEL_SRC_SYS_PERF_COUNTER,
                                                     infoRegisters[regIdx],
                                                     COPY_DATA_SEL_COUNT_1DW,
                                                     COPY_DATA_ENGINE_ME,
                                                     COPY_DATA_WR_CONFIRM_WAIT,
                                                     pCmdSpace);
            }
        }
    }

    // Restore global broadcasting.
    pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
// A helper function for IssueBegin which writes the necessary commands to set every enabled PERFCOUNTER#_SELECT.
uint32* PerfExperiment::WriteSelectRegisters(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    // The SQG has special select programming instructions.
    for (uint32 instance = 0; instance < ArrayLen(m_select.sqg); ++instance)
    {
        if (m_select.sqg[instance].hasCounters)
        {
            const PerfCounterRegAddr& regAddr = m_counterInfo.block[static_cast<uint32>(GpuBlock::Sq)].regAddr;

            // The SQ counters must be programmed while broadcasting to all SQs on the target SE. This should be
            // fine because each "SQ" instance here is really a SQG instance and there's only one in each SE.
            pCmdSpace = WriteGrbmGfxIndexBroadcastSe(m_select.sqg[instance].grbmGfxIndex.bits.SE_INDEX,
                                                     pCmdStream,
                                                     pCmdSpace);

            for (uint32 idx = 0; idx < ArrayLen(m_select.sqg[instance].perfmon); ++idx)
            {
                if (m_select.sqg[instance].perfmonInUse[idx])
                {
                    PAL_ASSERT(regAddr.perfcounter[idx].selectOrCfg != 0);

                    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regAddr.perfcounter[idx].selectOrCfg,
                                                                  m_select.sqg[instance].perfmon[idx].u32All,
                                                                  pCmdSpace);
                }
            }

            // Get fresh command space just in case we're close to running out.
            pCmdStream->CommitCommands(pCmdSpace);
            pCmdSpace = pCmdStream->ReserveCommands();
        }
    }

    // We program the GRBM's per-SE counters separately from its generic global counters.
    for (uint32 instance = 0; instance < ArrayLen(m_select.grbmSe); ++instance)
    {
        if (m_select.grbmSe[instance].hasCounter)
        {
            // By convention we access the counter register address array using the SE index.
            const PerfCounterRegAddr& regAddr = m_counterInfo.block[static_cast<uint32>(GpuBlock::GrbmSe)].regAddr;

            PAL_ASSERT(regAddr.perfcounter[instance].selectOrCfg != 0);

            // The GRBM is global and has one instance so we can just use global broadcasting.
            pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);
            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regAddr.perfcounter[instance].selectOrCfg,
                                                          m_select.grbmSe[instance].select.u32All,
                                                          pCmdSpace);

            // Get fresh command space just in case we're close to running out.
            pCmdStream->CommitCommands(pCmdSpace);
            pCmdSpace = pCmdStream->ReserveCommands();
        }
    }

    // Program the legacy SDMA select registers. These should only be enabled on gfx7+.
    for (uint32 instance = 0; instance < ArrayLen(m_select.sdma); ++instance)
    {
        if (m_select.sdma[instance].hasCounter[0] || m_select.sdma[instance].hasCounter[1])
        {
            // Each GFX7+ SDMA engine is a global block with a unique register that controls both counters.
            PAL_ASSERT(m_counterInfo.sdmaRegAddr[instance][0].selectOrCfg != 0);

            pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);
            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(m_counterInfo.sdmaRegAddr[instance][0].selectOrCfg,
                                                          m_select.sdma[instance].perfmonCntl.u32All,
                                                          pCmdSpace);

            // Get fresh command space just in case we're close to running out.
            pCmdStream->CommitCommands(pCmdSpace);
            pCmdSpace = pCmdStream->ReserveCommands();
        }
    }

    // Program the global MC per-counter control registers. They are per-MCD instead of per global instance.
    if (HasMcCounters())
    {
        // It's not clear if we really need to set this for the MC, might as well for now.
        pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);

        for (uint32 mcdInstance = 0; mcdInstance < ArrayLen(m_select.mc); ++mcdInstance)
        {
            if (m_select.mc[mcdInstance].hasCounters)
            {
                // Target our register writes to this instance.
                pCmdSpace = WriteMcConfigTargetInstance(mcdInstance, pCmdStream, pCmdSpace);

                // By convention SEQ_CTL is the first select, CNTL_1 is the second select, the "Lo" registers are for
                // channel 0, and the "Hi" registers are for channel 1.
                const PerfCounterRegAddr& regAddr = m_counterInfo.block[static_cast<uint32>(GpuBlock::Mc)].regAddr;

                // Write the registers that specify the counter selects. They are shared by all counters.
                PAL_ASSERT((regAddr.perfcounter[0].selectOrCfg != 0) && (regAddr.perfcounter[0].select1 != 0));

                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regAddr.perfcounter[0].selectOrCfg,
                                                              m_select.mc[mcdInstance].perfSeqCntl.u32All,
                                                              pCmdSpace);

                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regAddr.perfcounter[0].select1,
                                                              m_select.mc[mcdInstance].perfCntl1.u32All,
                                                              pCmdSpace);

                // Get fresh command space just in case we're close to running out.
                pCmdStream->CommitCommands(pCmdSpace);
                pCmdSpace = pCmdStream->ReserveCommands();
            }
        }

        // Restore MC write broadcasting.
        pCmdSpace = WriteMcConfigBroadcastGlobal(pCmdStream, pCmdSpace);
    }

    // Finally, write the generic blocks' select registers.
    for (uint32 block = 0; block < GpuBlockCount; ++block)
    {
        if (m_select.pGeneric[block] != nullptr)
        {
            const PerfCounterRegAddr& regAddr = m_counterInfo.block[block].regAddr;

            for (uint32 instance = 0; instance < m_select.numGeneric[block]; ++instance)
            {
                const GenericBlockSelect& select = m_select.pGeneric[block][instance];

                if (select.hasCounters)
                {
                    // Write GRBM_GFX_INDEX to target this specific block instance and enable its active modules.
                    pCmdSpace = pCmdStream->WriteSetOneConfigReg(m_registerInfo.mmGrbmGfxIndex,
                                                                 select.grbmGfxIndex.u32All,
                                                                 pCmdSpace);

                    for (uint32 idx = 0; idx < select.numModules; ++idx)
                    {
                        if (select.pModules[idx].inUse != 0)
                        {
                            if(select.pModules[idx].type == SelectType::Perfmon)
                            {
                                // The perfmon registers come in SELECT/SELECT1 pairs.
                                PAL_ASSERT((regAddr.perfcounter[idx].selectOrCfg != 0) &&
                                           (regAddr.perfcounter[idx].select1 != 0));

                                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regAddr.perfcounter[idx].selectOrCfg,
                                                                              select.pModules[idx].perfmon.sel0.u32All,
                                                                              pCmdSpace);

                                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regAddr.perfcounter[idx].select1,
                                                                              select.pModules[idx].perfmon.sel1.u32All,
                                                                              pCmdSpace);
                            }
                            else
                            {
                                // Legacy modules have one register.
                                PAL_ASSERT(regAddr.perfcounter[idx].selectOrCfg != 0);

                                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regAddr.perfcounter[idx].selectOrCfg,
                                                                              select.pModules[idx].legacySel.u32All,
                                                                              pCmdSpace);
                            }
                        }
                    }

                    // Get fresh command space just in case we're close to running out.
                    pCmdStream->CommitCommands(pCmdSpace);
                    pCmdSpace = pCmdStream->ReserveCommands();
                }
            }
        }
    }

    // Restore global broadcasting.
    pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
// A helper function for IssueBegin which writes the necessary commands to toggle global config/control registers.
uint32* PerfExperiment::WriteEnableCfgRegisters(
    bool       enable,
    bool       clear,
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    // The MC has a global control register which we broadcast to all MCDs.
    if (HasMcCounters())
    {
        if (clear)
        {
            regMC_SEQ_PERF_CNTL__SI__CI mcSeqPerfCntl = {};
            mcSeqPerfCntl.bits.CNTL = 2; // CLEAR COUNTER

            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmMC_SEQ_PERF_CNTL__SI__CI, mcSeqPerfCntl.u32All, pCmdSpace);
        }

        regMC_SEQ_PERF_CNTL__SI__CI mcSeqPerfCntl = {};
        mcSeqPerfCntl.bits.MONITOR_PERIOD = 0; // The smallest possible collecting period.
        mcSeqPerfCntl.bits.CNTL           = 0; // NO WINDOW NO TERMINAL COUNT (enabled)

        pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmMC_SEQ_PERF_CNTL__SI__CI, mcSeqPerfCntl.u32All, pCmdSpace);
    }

    // The RLC has a special global control register. It works just like CP_PERFMON_CNTL.
    if (HasGenericCounters(GpuBlock::Rlc))
    {
        if (clear)
        {
            regRLC_PERFMON_CNTL rlcPerfmonCntl = {};
            rlcPerfmonCntl.bits.PERFMON_STATE = CP_PERFMON_STATE_DISABLE_AND_RESET;

            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(m_registerInfo.mmRlcPerfmonCntl,
                                                          rlcPerfmonCntl.u32All,
                                                          pCmdSpace);
        }

        regRLC_PERFMON_CNTL rlcPerfmonCntl = {};
        rlcPerfmonCntl.bits.PERFMON_STATE = enable ? CP_PERFMON_STATE_START_COUNTING : CP_PERFMON_STATE_STOP_COUNTING;

        pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(m_registerInfo.mmRlcPerfmonCntl,
                                                      rlcPerfmonCntl.u32All,
                                                      pCmdSpace);
    }

    // The SRBM has a special global control register. It works just like CP_PERFMON_CNTL.
    if (HasGenericCounters(GpuBlock::Srbm))
    {
        if (clear)
        {
            regSRBM_PERFMON_CNTL srbmPerfmonCntl = {};
            srbmPerfmonCntl.bits.PERFMON_STATE = CP_PERFMON_STATE_DISABLE_AND_RESET;

            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(m_registerInfo.mmSrbmPerfmonCntl,
                                                          srbmPerfmonCntl.u32All,
                                                          pCmdSpace);
        }

        regSRBM_PERFMON_CNTL srbmPerfmonCntl = {};
        srbmPerfmonCntl.bits.PERFMON_STATE = enable ? CP_PERFMON_STATE_START_COUNTING : CP_PERFMON_STATE_STOP_COUNTING;

        pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(m_registerInfo.mmSrbmPerfmonCntl,
                                                      srbmPerfmonCntl.u32All,
                                                      pCmdSpace);
    }

    // Get fresh command space just in case we're close to running out.
    pCmdStream->CommitCommands(pCmdSpace);
    pCmdSpace = pCmdStream->ReserveCommands();

    return pCmdSpace;
}

// =====================================================================================================================
// A helper function for IssueBegin which writes the necessary commands to stop the global perf counters and sample
// them. It will leave them stopped.
uint32* PerfExperiment::WriteStopAndSampleGlobalCounters(
    bool          isBeginSample,
    GfxCmdBuffer* pCmdBuffer,
    CmdStream*    pCmdStream,
    uint32*       pCmdSpace
    ) const
{
    const EngineType engineType = pCmdStream->GetEngineType();

    // The recommended sampling procedure: stop windowed, sample, wait-idle, stop global, read values.
    //
    // By experimentation, global blocks don't listen to perfcounter events so we must always set PERFMON_SAMPLE_ENABLE
    // while also issuing the event. We could probably take a long time to study how each specific block responds to
    // events or the sample bit to come up with the optimal programming, but for now just always do both to make sure
    // we definitely get results.
    pCmdSpace = WriteUpdateWindowedCounters(false, pCmdStream, pCmdSpace);
    pCmdSpace += m_cmdUtil.BuildEventWrite(PERFCOUNTER_SAMPLE, pCmdSpace);

    pCmdSpace = WriteWaitIdle(false, pCmdBuffer, pCmdStream, pCmdSpace);

    // Stop the global counters. If SPM is enabled we also stop its counters so that they don't sample our sampling.
    regCP_PERFMON_CNTL cpPerfmonCntl = {};
    cpPerfmonCntl.bits.PERFMON_SAMPLE_ENABLE     = 1;
    cpPerfmonCntl.bits.PERFMON_STATE             = CP_PERFMON_STATE_STOP_COUNTING;
    cpPerfmonCntl.bits.SPM_PERFMON_STATE__CI__VI =
        m_perfExperimentFlags.spmTraceEnabled ? STRM_PERFMON_STATE_STOP_COUNTING
                                              : STRM_PERFMON_STATE_DISABLE_AND_RESET;

    pCmdSpace = pCmdStream->WriteSetOneConfigReg(m_registerInfo.mmCpPerfmonCntl, cpPerfmonCntl.u32All, pCmdSpace);

    // Stop the global config counters too. It's not clear if these are included in the above guidelines so just stop
    // them at the end to be safe. If we're getting the begin samples we should also initialize these counters by
    // clearing them.
    pCmdSpace = WriteEnableCfgRegisters(false, isBeginSample, pCmdStream, pCmdSpace);

    // The old perf experiment code also sets the RLC's PERFMON_SAMPLE_ENABLE bit each time it samples. I can't find
    // any documentation that has anything to say at all about this field so let's just do the same thing.
    if (HasGenericCounters(GpuBlock::Rlc))
    {
        regRLC_PERFMON_CNTL rlcPerfmonCntl = {};
        rlcPerfmonCntl.bits.PERFMON_STATE         = CP_PERFMON_STATE_STOP_COUNTING;
        rlcPerfmonCntl.bits.PERFMON_SAMPLE_ENABLE = 1;

        pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(m_registerInfo.mmRlcPerfmonCntl,
                                                      rlcPerfmonCntl.u32All,
                                                      pCmdSpace);
    }

    // The old perf experiment code also sets the SRBM's PERFMON_SAMPLE_ENABLE bit each time it samples. I can't find
    // any documentation that has anything to say at all about this field so let's just do the same thing.
    if (HasGenericCounters(GpuBlock::Srbm))
    {
        regSRBM_PERFMON_CNTL srbmPerfmonCntl = {};
        srbmPerfmonCntl.bits.PERFMON_STATE         = CP_PERFMON_STATE_STOP_COUNTING;
        srbmPerfmonCntl.bits.PERFMON_SAMPLE_ENABLE = 1;

        pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(m_registerInfo.mmSrbmPerfmonCntl,
                                                      srbmPerfmonCntl.u32All,
                                                      pCmdSpace);
    }

    // Copy each counter's value from registers to memory, one at a time.
    const gpusize destBaseAddr = m_gpuMemory.GpuVirtAddr() + (isBeginSample ? m_globalBeginOffset : m_globalEndOffset);

    for (uint32 idx = 0; idx < m_globalCounters.NumElements(); ++idx)
    {
        const GlobalCounterMapping& mapping  = m_globalCounters.At(idx);
        const uint32                instance = mapping.general.globalInstance;
        const uint32                block    = static_cast<uint32>(mapping.general.block);

        if (mapping.general.block == GpuBlock::Sq)
        {
            // NOTE: SQ reads can time out and fail if the performance result read occurs too shortly after the sample.
            //       The workaround for this is to set the read timeout duration to its maximum value while we sample
            //       the counter. This is acceptable since we know the data is going to be written and we're willing
            //       to wait for it. Unfortunately, we cannot write GRBM_CNTL on Gfx7, but the problem still exists on
            //       those chips. DXX doesn't have a solution for this, either.
            if (m_chipProps.gfxLevel == GfxIpLevel::GfxIp6)
            {
                regGRBM_CNTL grbmCntlWait = {};
                grbmCntlWait.bits.READ_TIMEOUT = 0xFF;

                pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmGRBM_CNTL, grbmCntlWait.u32All, pCmdSpace);
            }

            // This is essentially the generic path but we keep our GRBM_GFX_INDEX in a special location.
            pCmdSpace = pCmdStream->WriteSetOneConfigReg(m_registerInfo.mmGrbmGfxIndex,
                                                         m_select.sqg[instance].grbmGfxIndex.u32All,
                                                         pCmdSpace);

            pCmdSpace = WriteCopy64BitCounter(m_counterInfo.block[block].regAddr.perfcounter[mapping.counterId].lo,
                                              m_counterInfo.block[block].regAddr.perfcounter[mapping.counterId].hi,
                                              destBaseAddr + mapping.offset,
                                              pCmdStream,
                                              pCmdSpace);

            // Restore the default value of GRBM_CNTL if we changed it earlier.
            if (m_chipProps.gfxLevel == GfxIpLevel::GfxIp6)
            {
                regGRBM_CNTL grbmCntlDefault = {};
                grbmCntlDefault.bits.READ_TIMEOUT = 0x18;

                pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmGRBM_CNTL, grbmCntlDefault.u32All, pCmdSpace);
            }
        }
        else if (mapping.general.block == GpuBlock::GrbmSe)
        {
            // The per-SE counters are different from the generic case in two ways:
            // 1. The GRBM is a global block so we need to use global broadcasting.
            // 2. The register addresses are unique for each instance.
            pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);
            pCmdSpace = WriteCopy64BitCounter(m_counterInfo.block[block].regAddr.perfcounter[instance].lo,
                                              m_counterInfo.block[block].regAddr.perfcounter[instance].hi,
                                              destBaseAddr + mapping.offset,
                                              pCmdStream,
                                              pCmdSpace);
        }
        else if (mapping.general.block == GpuBlock::Dma)
        {
            // Each SDMA engine is a global block which defines unique 32-bit global counter registers.
            pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);

            PAL_ASSERT(m_counterInfo.sdmaRegAddr[instance][mapping.counterId].lo != 0);

            pCmdSpace += m_cmdUtil.BuildCopyData(COPY_DATA_SEL_DST_ASYNC_MEMORY,
                                                 destBaseAddr + mapping.offset,
                                                 COPY_DATA_SEL_SRC_SYS_PERF_COUNTER,
                                                 m_counterInfo.sdmaRegAddr[instance][mapping.counterId].lo,
                                                 COPY_DATA_SEL_COUNT_1DW,
                                                 COPY_DATA_ENGINE_ME,
                                                 COPY_DATA_WR_CONFIRM_WAIT,
                                                 pCmdSpace);
        }
        else if (mapping.general.block == GpuBlock::Mc)
        {
            // Each MC is a global block that defines 32-bit global counter registers.
            // Map this instance back to a MC[D] and channel. The old perf experiment code treated the MC index as the
            // "inner" index so we do the same.
            const uint32 mcIdx = instance % m_chipProps.gfx6.numMcdTiles;
            const uint32 chIdx = instance / m_chipProps.gfx6.numMcdTiles;

            // It's not clear if we really need to set this for the MC, might as well for now.
            pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);

            // Target our register writes to this instance.
            pCmdSpace = WriteMcConfigTargetInstance(mcIdx, pCmdStream, pCmdSpace);

            // By convention the "Lo" registers are for channel 0, and the "Hi" registers are for channel 1.
            const uint32 regAddr = (chIdx == 0) ? m_counterInfo.block[block].regAddr.perfcounter[mapping.counterId].lo
                                                : m_counterInfo.block[block].regAddr.perfcounter[mapping.counterId].hi;

            pCmdSpace += m_cmdUtil.BuildCopyData(COPY_DATA_SEL_DST_ASYNC_MEMORY,
                                                 destBaseAddr + mapping.offset,
                                                 COPY_DATA_SEL_SRC_SYS_PERF_COUNTER,
                                                 regAddr,
                                                 COPY_DATA_SEL_COUNT_1DW,
                                                 COPY_DATA_ENGINE_ME,
                                                 COPY_DATA_WR_CONFIRM_WAIT,
                                                 pCmdSpace);
        }
        else if (m_select.pGeneric[block] != nullptr)
        {
            // Set GRBM_GFX_INDEX so that we're talking to the specific block instance which own the given counter.
            pCmdSpace = pCmdStream->WriteSetOneConfigReg(m_registerInfo.mmGrbmGfxIndex,
                                                         m_select.pGeneric[block][instance].grbmGfxIndex.u32All,
                                                         pCmdSpace);

            // Copy the counter value out to memory.
            pCmdSpace = WriteCopy64BitCounter(m_counterInfo.block[block].regAddr.perfcounter[mapping.counterId].lo,
                                              m_counterInfo.block[block].regAddr.perfcounter[mapping.counterId].hi,
                                              destBaseAddr + mapping.offset,
                                              pCmdStream,
                                              pCmdSpace);
        }
        else
        {
            // What block did we forget to implement?
            PAL_ASSERT_ALWAYS();
        }

        // Get fresh command space just in case we're close to running out.
        pCmdStream->CommitCommands(pCmdSpace);
        pCmdSpace = pCmdStream->ReserveCommands();
    }

    // Restore global broadcasting.
    pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);

    if (HasMcCounters())
    {
        // Restore global MC broadcasting too.
        pCmdSpace = WriteMcConfigBroadcastGlobal(pCmdStream, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// A helper function for WriteSampleGlobalCounters which writes two COPY_DATAs to read out a 64-bit counter for some
// counter in some block.
uint32* PerfExperiment::WriteCopy64BitCounter(
    uint32     regAddrLo,
    uint32     regAddrHi,
    gpusize    destAddr,
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    // Copy out the 64-bit value in two parts.
    PAL_ASSERT((regAddrLo != 0) && (regAddrHi != 0));

    pCmdSpace += m_cmdUtil.BuildCopyData(COPY_DATA_SEL_DST_ASYNC_MEMORY,
                                         destAddr,
                                         COPY_DATA_SEL_SRC_SYS_PERF_COUNTER,
                                         regAddrLo,
                                         COPY_DATA_SEL_COUNT_1DW,
                                         COPY_DATA_ENGINE_ME,
                                         COPY_DATA_WR_CONFIRM_WAIT,
                                         pCmdSpace);

    pCmdSpace += m_cmdUtil.BuildCopyData(COPY_DATA_SEL_DST_ASYNC_MEMORY,
                                         destAddr + sizeof(uint32),
                                         COPY_DATA_SEL_SRC_SYS_PERF_COUNTER,
                                         regAddrHi,
                                         COPY_DATA_SEL_COUNT_1DW,
                                         COPY_DATA_ENGINE_ME,
                                         COPY_DATA_WR_CONFIRM_WAIT,
                                         pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
// Writes GRBM_GFX_INDEX in the given command space such that we are broadcasting to all instances on the whole chip.
uint32* PerfExperiment::WriteGrbmGfxIndexBroadcastGlobal(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    regGRBM_GFX_INDEX grbmGfxIndex = {};
    grbmGfxIndex.bits.SE_BROADCAST_WRITES       = 1;
    grbmGfxIndex.bits.SH_BROADCAST_WRITES       = 1;
    grbmGfxIndex.bits.INSTANCE_BROADCAST_WRITES = 1;

    return pCmdStream->WriteSetOneConfigReg(m_registerInfo.mmGrbmGfxIndex, grbmGfxIndex.u32All, pCmdSpace);
}

// =====================================================================================================================
// Writes GRBM_GFX_INDEX in the given command space such that we are broadcasting to all instances in a given SE.
uint32* PerfExperiment::WriteGrbmGfxIndexBroadcastSe(
    uint32     seIndex,
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    regGRBM_GFX_INDEX grbmGfxIndex = {};
    grbmGfxIndex.bits.SE_INDEX                  = seIndex;
    grbmGfxIndex.bits.SH_BROADCAST_WRITES       = 1;
    grbmGfxIndex.bits.INSTANCE_BROADCAST_WRITES = 1;

    return pCmdStream->WriteSetOneConfigReg(m_registerInfo.mmGrbmGfxIndex, grbmGfxIndex.u32All, pCmdSpace);
}

// =====================================================================================================================
// Writes the MC config such that the SRBM broadcasts writes to all MC[D] instances.
uint32* PerfExperiment::WriteMcConfigBroadcastGlobal(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    // The register's default value sets every bit in the write enable mask.
    return pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(m_counterInfo.mcConfig.regAddress,
                                                         m_counterInfo.mcConfig.writeEnableMask,
                                                         pCmdSpace);
}

// =====================================================================================================================
// Writes the MC config such that the SRBM targets a specific MC[D].
uint32* PerfExperiment::WriteMcConfigTargetInstance(
    uint32     mcdInstance,
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    if (IsTonga(*m_pDevice) && (m_chipProps.gfx6.numMcdTiles == 4))
    {
        // The four MCD Tonga uses MCDs 0, 2, 3, and 5.
        // So we must map the logical MCD instance in the [0123] range to MCD [0235].
        constexpr uint32 InstanceToMcdMap[] = { 0, 2, 3, 5 };
        mcdInstance = InstanceToMcdMap[mcdInstance];
    }

    // The register starts with one write-enable bit for each MCD followed by an enum selecting the read MCD.
    // We will use this to send reads and writes to a specific MCD so set the single write bit and the read select.
    const uint32 mcRegValue = ((mcdInstance << m_counterInfo.mcConfig.readEnableShift) | (1 << mcdInstance));

    return pCmdStream->WriteSetOneConfigReg(m_registerInfo.mmGrbmGfxIndex, mcRegValue, pCmdSpace);
}

// =====================================================================================================================
// Writes a packet that updates the SQG event controls in SPI_CONFIG_CNTL.
uint32* PerfExperiment::WriteUpdateSpiConfigCntl(
    bool       enableSqgEvents,
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    // Both SQ performance counters and traces need the SQG events enabled. Force them on ourselves if KMD doesn't have
    // them active by default.
    if (m_chipProps.gfx6.sqgEventsEnabled == false)
    {
        // We really only want to update the SQG fields but can't. This will clobber some important sounding defaults
        // (e.g., GPR_WRITE_PRIORITY and EXP_PRIORITY_ORDER) and this could be a problem.
        regSPI_CONFIG_CNTL spiConfigCntl = {};
        spiConfigCntl.bits.ENABLE_SQG_TOP_EVENTS = enableSqgEvents;
        spiConfigCntl.bits.ENABLE_SQG_BOP_EVENTS = enableSqgEvents;

        pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSPI_CONFIG_CNTL, spiConfigCntl.u32All, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Writes a packet that starts or stops windowed perf counters.
uint32* PerfExperiment::WriteUpdateWindowedCounters(
    bool       enable,
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    // We should use an event on universal queues but set a register on async compute queues.
    if ((m_chipProps.gfxLevel == GfxIpLevel::GfxIp6) || m_pDevice->EngineSupportsGraphics(pCmdStream->GetEngineType()))
    {
        pCmdSpace += m_cmdUtil.BuildEventWrite(enable ? PERFCOUNTER_START : PERFCOUNTER_STOP, pCmdSpace);
    }

    if (m_chipProps.gfxLevel != GfxIpLevel::GfxIp6)
    {
        regCOMPUTE_PERFCOUNT_ENABLE__CI__VI computeEnable = {};
        computeEnable.bits.PERFCOUNT_ENABLE = enable;

        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_PERFCOUNT_ENABLE__CI__VI,
                                                                computeEnable.u32All,
                                                                pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Writes the necessary packets to wait for GPU idle and optionally flush and invalidate all caches.
uint32* PerfExperiment::WriteWaitIdle(
    bool          flushCaches,
    GfxCmdBuffer* pCmdBuffer,
    CmdStream*    pCmdStream,
    uint32*       pCmdSpace
    ) const
{
    if (m_pDevice->EngineSupportsGraphics(pCmdStream->GetEngineType()))
    {
        // Use a CS_PARTIAL_FLUSH and SURFACE_SYNC to wait for CS and graphics work to complete. Use the surface sync
        // to flush caches if requested.
        //
        // Note that this isn't a true wait-idle for the graphics engine. In order to wait for the very bottom of
        // the pipeline we would have to wait for a EOP TS event. Doing that inflates the perf experiment overhead
        // by a not-insignificant margin. Thus we go with this much faster waiting method which covers almost all of
        // the same cases as the wait for EOP TS. If we run into issues with counters at the end of the graphics
        // pipeline or counters that monitor the event pipeline we might need to change this.
        pCmdSpace += m_cmdUtil.BuildEventWrite(CS_PARTIAL_FLUSH, pCmdSpace);

        regCP_COHER_CNTL cpCoherCntl;
        cpCoherCntl.u32All = CpCoherCntlStallMask;

        if (flushCaches)
        {
            cpCoherCntl.u32All |= (CP_COHER_CNTL__TCL1_ACTION_ENA_MASK      |
                                   CP_COHER_CNTL__TC_ACTION_ENA_MASK        |
                                   CP_COHER_CNTL__CB_ACTION_ENA_MASK        |
                                   CP_COHER_CNTL__DB_ACTION_ENA_MASK        |
                                   CP_COHER_CNTL__SH_KCACHE_ACTION_ENA_MASK |
                                   CP_COHER_CNTL__SH_ICACHE_ACTION_ENA_MASK);
        }

        pCmdSpace += m_cmdUtil.BuildSurfaceSync(cpCoherCntl,
                                                SURFACE_SYNC_ENGINE_ME,
                                                FullSyncBaseAddr,
                                                FullSyncSize,
                                                pCmdSpace);

        pCmdSpace += m_cmdUtil.BuildPfpSyncMe(pCmdSpace);
    }
    else
    {
        // Wait for all work to be idle and use an ACQUIRE_MEM to flush any caches.
        pCmdSpace += m_cmdUtil.BuildWaitCsIdle(EngineTypeCompute, pCmdBuffer->TimestampGpuVirtAddr(), pCmdSpace);

        if (flushCaches)
        {
            regCP_COHER_CNTL cpCoherCntl;
            cpCoherCntl.u32All = (CP_COHER_CNTL__TCL1_ACTION_ENA_MASK      |
                                  CP_COHER_CNTL__TC_ACTION_ENA_MASK        |
                                  CP_COHER_CNTL__SH_KCACHE_ACTION_ENA_MASK |
                                  CP_COHER_CNTL__SH_ICACHE_ACTION_ENA_MASK);

            pCmdSpace += m_cmdUtil.BuildGenericSync(cpCoherCntl,
                                                    SURFACE_SYNC_ENGINE_ME,
                                                    FullSyncBaseAddr,
                                                    FullSyncSize,
                                                    true,
                                                    pCmdSpace);
        }
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Returns true if we've enabled any global or SPM counters for the given generic block.
bool PerfExperiment::HasGenericCounters(
    GpuBlock block
    ) const
{
    bool hasCounters = false;

    for (uint32 idx = 0; idx < m_select.numGeneric[static_cast<uint32>(block)]; ++idx)
    {
        if (m_select.pGeneric[static_cast<uint32>(block)][idx].hasCounters)
        {
            hasCounters = true;
            break;
        }
    }

    return hasCounters;
}

// =====================================================================================================================
// Returns true if we've enabled any global MC counters.
bool PerfExperiment::HasMcCounters() const
{
    bool hasCounters = false;

    for (uint32 idx = 0; idx < ArrayLen(m_select.mc); ++idx)
    {
        if (m_select.mc[idx].hasCounters)
        {
            hasCounters = true;
            break;
        }
    }

    return hasCounters;
}

} // Gfx6
} // Pal
