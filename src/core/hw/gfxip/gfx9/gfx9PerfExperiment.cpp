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

#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9PerfExperiment.h"
#include "core/hw/gfxip/pm4CmdBuffer.h"
#include "core/platform.h"
#include "palLiterals.h"
#include "palVectorImpl.h"
#include "palDbgLogger.h"

using namespace Util;
using namespace Util::Literals;

namespace Pal
{
namespace Gfx9
{

// We assume these enums match their SE indices in a few places.
static_assert(uint32(SpmDataSegmentType::Se0) == 0, "SpmDataSegmentType::Se0 is not 0.");
static_assert(uint32(SpmDataSegmentType::Se1) == 1, "SpmDataSegmentType::Se1 is not 1.");
static_assert(uint32(SpmDataSegmentType::Se2) == 2, "SpmDataSegmentType::Se2 is not 2.");
static_assert(uint32(SpmDataSegmentType::Se3) == 3, "SpmDataSegmentType::Se3 is not 3.");

// Default SQ select masks for our counter options (by default, select all).
constexpr uint32 DefaultSqSelectBankMask = 0xF;

// Bitmask limits for some sqtt parameters.
constexpr uint32  SqttDetailedSimdMask = 0xF;

// Stall when at 6/8s of the output buffer because data will still come in from already-issued waves
constexpr uint32  SqttGfx10HiWaterValue = 5;
// Safe defaults for token exclude mask and register include mask for the gfx10 SQTT_TOKEN_MASK register.
constexpr uint32  SqttGfx10RegMaskDefault = (SQ_TT_TOKEN_MASK_SQDEC_BIT   |
                                             SQ_TT_TOKEN_MASK_SHDEC_BIT   |
                                             SQ_TT_TOKEN_MASK_GFXUDEC_BIT |
                                             SQ_TT_TOKEN_MASK_CONTEXT_BIT |
                                             SQ_TT_TOKEN_MASK_COMP_BIT);
constexpr uint32  SqttGfx10TokenMaskDefault = ((1 << SQ_TT_TOKEN_EXCLUDE_VMEMEXEC_SHIFT) |
                                               (1 << SQ_TT_TOKEN_EXCLUDE_ALUEXEC_SHIFT)  |
                                               (1 << SQ_TT_TOKEN_EXCLUDE_WAVERDY_SHIFT)  |
                                               (1 << SQ_TT_TOKEN_EXCLUDE_PERF_SHIFT));
// The low watermark will be set to high watermark minus low watermark offset. This is HW's recommended default.
constexpr uint32  SqttGfx103LoWaterOffsetValue = 4;
// For now set this to zero (disabled) because we need to update the register headers to get the new enums.
constexpr uint32  SqttGfx103RegExcludeMaskDefault = 0x0;

// The SPM ring buffer base address must be 32-byte aligned.
constexpr uint32 SpmRingBaseAlignment = 32;

// The DF SPM buffer alignment
constexpr uint32 DfSpmBufferAlignment = 0x10000;

// The bound GPU memory must be aligned to the maximum of all alignment requirements.
constexpr gpusize GpuMemoryAlignment = Max<gpusize>(SqttBufferAlignment, SpmRingBaseAlignment);

// =====================================================================================================================
static void SetSqttTokenExclude(
    const Pal::Device&             device,
    regSQ_THREAD_TRACE_TOKEN_MASK* pRegValue,
    uint32                         tokenExclude)
{
    if (IsGfx101(device))
    {
        pRegValue->gfx101.TOKEN_EXCLUDE = tokenExclude;
    }
    else if (IsGfx103PlusExclusive(device))
    {
        pRegValue->gfx103PlusExclusive.TOKEN_EXCLUDE = tokenExclude;
    }
    else
    {
        // What is this?
        PAL_ASSERT_ALWAYS();
    }
}

// =====================================================================================================================
// Converts the thread trace token config to the gfx9 format for programming the TOKEN_MASK register.
static regSQ_THREAD_TRACE_TOKEN_MASK GetGfx10SqttTokenMask(
    const Pal::Device&            device,
    const ThreadTraceTokenConfig& tokenConfig)
{
    regSQ_THREAD_TRACE_TOKEN_MASK value = {};
    if (IsGfx103PlusExclusive(device))
    {
        // Setting SPI_CONFIG_CNTL.bits.ENABLE_SQG_BOP_EVENTS to 1 only allows SPI to send BOP events to SQG.
        // If BOP_EVENTS_TOKEN_INCLUDE is 0, SQG will not issue BOP event token writes to SQTT buffer.
        value.gfx103PlusExclusive.BOP_EVENTS_TOKEN_INCLUDE = 1;
    }

    const uint32 tokenExclude       = ~tokenConfig.tokenMask;
    const bool   vmemExecExclude    = TestAnyFlagSet(tokenExclude, ThreadTraceTokenTypeFlags::VmemExec);
    const bool   aluExecExclude     = TestAnyFlagSet(tokenExclude, ThreadTraceTokenTypeFlags::AluExec);
    const bool   valuInstExclude    = TestAnyFlagSet(tokenExclude, ThreadTraceTokenTypeFlags::ValuInst);
    const bool   waveRdyExclude     = TestAnyFlagSet(tokenExclude, ThreadTraceTokenTypeFlags::WaveRdy);
    const bool   immediateExclude   = TestAnyFlagSet(tokenExclude, ThreadTraceTokenTypeFlags::Immediate);
    const bool   utilCounterExclude = TestAnyFlagSet(tokenExclude, ThreadTraceTokenTypeFlags::UtilCounter);
    const bool   waveAllocExclude   = TestAnyFlagSet(tokenExclude, ThreadTraceTokenTypeFlags::WaveAlloc);
    const bool   immed1Exclude      = TestAnyFlagSet(tokenExclude, ThreadTraceTokenTypeFlags::Immed1);

    // Perf counters through thread trace is expected to be deprecated.
    constexpr bool PerfExclude = true;

    // Combine legacy TT enumerations with the newer (TT 3.0) enumerations).
    const bool regExclude   = TestAnyFlagSet(tokenExclude, ThreadTraceTokenTypeFlags::Reg   |
                                                           ThreadTraceTokenTypeFlags::RegCs |
                                                           ThreadTraceTokenTypeFlags::RegCsPriv);

    const bool eventExclude = TestAnyFlagSet(tokenExclude, ThreadTraceTokenTypeFlags::Event   |
                                                           ThreadTraceTokenTypeFlags::EventCs |
                                                           ThreadTraceTokenTypeFlags::EventGfx1);

    const bool instExclude  = TestAnyFlagSet(tokenExclude, ThreadTraceTokenTypeFlags::Inst   |
                                                           ThreadTraceTokenTypeFlags::InstPc |
                                                           ThreadTraceTokenTypeFlags::InstUserData);

    uint32 hwTokenExclude = ((vmemExecExclude    << SQ_TT_TOKEN_EXCLUDE_VMEMEXEC_SHIFT)  |
                             (aluExecExclude     << SQ_TT_TOKEN_EXCLUDE_ALUEXEC_SHIFT)   |
                             (valuInstExclude    << SQ_TT_TOKEN_EXCLUDE_VALUINST_SHIFT)  |
                             (waveRdyExclude     << SQ_TT_TOKEN_EXCLUDE_WAVERDY_SHIFT)   |
                             (immediateExclude   << SQ_TT_TOKEN_EXCLUDE_IMMEDIATE_SHIFT) |
                             (utilCounterExclude << SQ_TT_TOKEN_EXCLUDE_UTILCTR_SHIFT)   |
                             (waveAllocExclude   << SQ_TT_TOKEN_EXCLUDE_WAVEALLOC_SHIFT) |
                             (regExclude         << SQ_TT_TOKEN_EXCLUDE_REG_SHIFT)       |
                             (eventExclude       << SQ_TT_TOKEN_EXCLUDE_EVENT_SHIFT)     |
                             (instExclude        << SQ_TT_TOKEN_EXCLUDE_INST_SHIFT)      |
                             (PerfExclude        << SQ_TT_TOKEN_EXCLUDE_PERF_SHIFT));

    if (IsGfx103Plus(device))
    {
        // This control bit was removed. The "Immediate" flag is the only control for all IMMED instructions.
        PAL_ALERT(immed1Exclude);
    }
    else
    {
        hwTokenExclude |= immed1Exclude << SQ_TT_TOKEN_EXCLUDE_IMMED1_SHIFT__GFX101;
    }

    SetSqttTokenExclude(device, &value, hwTokenExclude);

    // Compute Register include mask. Obtain reg mask from combined legacy (TT 2.3 and below) and the newer (TT 3.0)
    // register types.
    const bool sqdecRegs       = TestAnyFlagSet(tokenConfig.regMask, ThreadTraceRegTypeFlags::ShaderConfigRegs |
                                                                     ThreadTraceRegTypeFlags::DrawRegs         |
                                                                     ThreadTraceRegTypeFlags::DispatchRegs);

    const bool shdecRegs       = TestAnyFlagSet(tokenConfig.regMask, ThreadTraceRegTypeFlags::ShaderLaunchStateRegs |
                                                                     ThreadTraceRegTypeFlags::DrawRegs              |
                                                                     ThreadTraceRegTypeFlags::DispatchRegs);

    const bool gfxudecRegs     = TestAnyFlagSet(tokenConfig.regMask, ThreadTraceRegTypeFlags::GraphicsPipeStateRegs |
                                                                     ThreadTraceRegTypeFlags::DrawRegs);

    const bool compRegs        = TestAnyFlagSet(tokenConfig.regMask, ThreadTraceRegTypeFlags::AsyncComputeRegs |
                                                                     ThreadTraceRegTypeFlags::DispatchRegs);

    const bool contextRegs     = TestAnyFlagSet(tokenConfig.regMask, ThreadTraceRegTypeFlags::GraphicsContextRegs |
                                                                     ThreadTraceRegTypeFlags::DrawRegs);

    const bool otherConfigRegs = TestAnyFlagSet(tokenConfig.regMask, ThreadTraceRegTypeFlags::OtherConfigRegs);

    // Note: This is for debug only. Enabling this can lead to a chip hang.
    const bool grbmCsDataRegs  = TestAnyFlagSet(tokenConfig.regMask, ThreadTraceRegTypeFlags::OtherBusRegs);

    // Note: Hw docs mention that this should normally be zero.
    const bool regReads        = TestAnyFlagSet(tokenConfig.regMask, ThreadTraceRegTypeFlags::AllRegReads);

    // Warning. Attempting to trace all register reads or enabling thread trace to capture all GRBM and CSDATA bus
    // activity could cause GPU hang or generate lot of thread trace traffic.
    PAL_ALERT(grbmCsDataRegs || regReads);

    // The enum is renamed, but the functionality is unchanged to the average thread trace user.
    // If you wanted 'other' for debugging, you probably wanted everything anyways.
    static_assert(SQ_TT_TOKEN_MASK_OTHER_SHIFT__GFX10 == SQ_TT_TOKEN_MASK_ALL_SHIFT__GFX11,
                  "Thread trace enum has changed");

    value.bits.REG_INCLUDE = ((sqdecRegs       << SQ_TT_TOKEN_MASK_SQDEC_SHIFT)        |
                              (shdecRegs       << SQ_TT_TOKEN_MASK_SHDEC_SHIFT)        |
                              (gfxudecRegs     << SQ_TT_TOKEN_MASK_GFXUDEC_SHIFT)      |
                              (compRegs        << SQ_TT_TOKEN_MASK_COMP_SHIFT)         |
                              (contextRegs     << SQ_TT_TOKEN_MASK_CONTEXT_SHIFT)      |
                              (otherConfigRegs << SQ_TT_TOKEN_MASK_CONFIG_SHIFT)       |
                              (grbmCsDataRegs  << SQ_TT_TOKEN_MASK_OTHER_SHIFT__GFX10) |
                              (regReads        << SQ_TT_TOKEN_MASK_READS_SHIFT__GFX10));

    if (IsGfx103PlusExclusive(device))
    {
        // We want to update REG_EXCLUDE based on the bools we computed above but can't until we fix the reg headers.
        value.gfx103PlusExclusive.REG_EXCLUDE = SqttGfx103RegExcludeMaskDefault;
    }

    return value;
}

// =====================================================================================================================
PerfExperiment::PerfExperiment(
    const Device*                   pDevice,
    const PerfExperimentCreateInfo& createInfo)
    :
    Pal::PerfExperiment(pDevice->Parent(), createInfo, GpuMemoryAlignment),
    m_chipProps(pDevice->Parent()->ChipProperties()),
    m_counterInfo(pDevice->Parent()->ChipProperties().gfx9.perfCounterInfo.gfx9Info),
    m_settings(pDevice->Settings()),
    m_cmdUtil(pDevice->CmdUtil()),
    m_globalCounters(m_pPlatform),
    m_pSpmCounters(nullptr),
    m_numSpmCounters(0),
    m_maxSeMuxSelLines(0),
    m_spmSampleLines(0),
    m_spmRingSize(0),
    m_spmMaxSamples(0),
    m_spmSampleInterval(0),
    m_pDfSpmCounters(nullptr),
    m_numDfSpmCounters(0),
    m_neverStopCounters(false)
{
    memset(m_sqtt,              0, sizeof(m_sqtt));
    memset(m_pMuxselRams,       0, sizeof(m_pMuxselRams));
    memset(m_numMuxselLines,    0, sizeof(m_numMuxselLines));
    memset(&m_dfSpmPerfmonInfo, 0, sizeof(m_dfSpmPerfmonInfo));
    memset(&m_select,           0, sizeof(m_select));
}

// =====================================================================================================================
PerfExperiment::~PerfExperiment()
{
    PAL_SAFE_DELETE_ARRAY(m_pSpmCounters, m_pPlatform);
    PAL_SAFE_DELETE_ARRAY(m_pDfSpmCounters, m_pPlatform);

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

    if (m_dfSpmPerfmonInfo.pDfSpmTraceBuffer != nullptr)
    {
        m_dfSpmPerfmonInfo.pDfSpmTraceBuffer->DestroyInternal();
    }

    if (m_dfSpmPerfmonInfo.pDfSpmMetadataBuffer != nullptr)
    {
        m_dfSpmPerfmonInfo.pDfSpmMetadataBuffer->DestroyInternal();
    }
}

// =====================================================================================================================
Result PerfExperiment::Init()
{
    Result result = Result::Success;

    // Validate some of our design assumption about the the hardware. These seem like valid assumptions but we can't
    // check them at compile time so this has to be an assert and an error instead of a static assert.
    if ((m_counterInfo.block[uint32(GpuBlock::Sq)].numInstances     > Gfx9MaxShaderEngines) ||
        (m_counterInfo.block[uint32(GpuBlock::SqWgp)].numInstances  > Gfx11MaxWgps)         ||
        (m_counterInfo.block[uint32(GpuBlock::GrbmSe)].numInstances > Gfx9MaxShaderEngines) ||
        (m_counterInfo.block[uint32(GpuBlock::Dma)].numInstances    > Gfx9MaxSdmaInstances) ||
        (m_counterInfo.block[uint32(GpuBlock::Umcch)].numInstances  > Gfx9MaxUmcchInstances))
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
    Result       result            = Result::Success;
    const uint32 blockIdx          = uint32(block);
    const uint32 numInstances      = m_counterInfo.block[blockIdx].numInstances;
    const uint32 numGenericModules = m_counterInfo.block[blockIdx].numGenericSpmModules +
                                     m_counterInfo.block[blockIdx].numGenericLegacyModules;

    // Only continue if:
    // - There are instances of this block on our device.
    // - This block has generic counter modules.
    if ((numInstances > 0) && (numGenericModules > 0))
    {
        // Check that we haven't allocated the per-instance array already.
        if (m_select.pGeneric[blockIdx] == nullptr)
        {
            m_select.numGeneric[blockIdx] = numInstances;
            m_select.pGeneric[blockIdx]   = PAL_NEW_ARRAY(GenericBlockSelect, numInstances, m_pPlatform, AllocObject);

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
                    // Cfg-style: the legacy modules come first followed by the perfmon modules.
                    uint32 moduleIdx = 0;
                    while (moduleIdx < m_counterInfo.block[blockIdx].numGenericLegacyModules)
                    {
                        pSelect->pModules[moduleIdx++].type = SelectType::LegacyCfg;
                    }

                    while (moduleIdx < pSelect->numModules)
                    {
                        pSelect->pModules[moduleIdx++].type = SelectType::Perfmon;
                    }
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
// - SPM_MODE_OFF and COUNTER_MODE_ACCUM are both equal to zero but we still set them to be explicit.
//
Result PerfExperiment::AddCounter(
    const Pal::PerfCounterInfo& info)
{
    Result               result  = Result::Success;
    GlobalCounterMapping mapping = {};

    if (m_isFinalized)
    {
        // The perf experiment cannot be changed once it is finalized.
        result = Result::ErrorUnavailable;
    }
    else if ((info.block == GpuBlock::DfMall) && m_perfExperimentFlags.dfSpmTraceEnabled)
    {
        // DF cumulative counters cannot be added if DF SPM is enabled.
        result = Result::ErrorInitializationFailed;
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
        const uint32 block = uint32(info.block);

        if (info.block == GpuBlock::Sq)
        {
            // The SQG counters are 64-bit.
            mapping.dataType = PerfCounterDataType::Uint64;

            // The SQG has special registers so it needs its own implementation.
            if (m_select.sqg[info.instance].hasCounters == false)
            {
                // Turn on this instance and populate its GRBM_GFX_INDEX.
                m_select.sqg[info.instance].hasCounters = true;
                m_select.sqg[info.instance].grbmGfxIndex = BuildGrbmGfxIndex(instanceMapping, info.block);
            }

            const uint32 sqgNumModules = IsGfx11(*m_pDevice) ? Gfx11MaxSqgPerfmonModules : Gfx9MaxSqgPerfmonModules;
            bool         searching     = true;

            for (uint32 idx = 0; searching && (idx < sqgNumModules); ++idx)
            {
                if (m_select.sqg[info.instance].perfmonInUse[idx] == false)
                {
                    // Our SQ/SQG PERF_SEL fields are 9 bits. Verify that our event ID can fit.
                    PAL_ASSERT(info.eventId <= ((1 << 9) - 1));

                    m_select.sqg[info.instance].perfmonInUse[idx]              = true;
                    m_select.sqg[info.instance].perfmon[idx].bits.PERF_SEL  = info.eventId;
                    m_select.sqg[info.instance].perfmon[idx].bits.SPM_MODE  = PERFMON_SPM_MODE_OFF;
                    m_select.sqg[info.instance].perfmon[idx].bits.PERF_MODE = PERFMON_COUNTER_MODE_ACCUM;

                    // The SQC bank mask was removed in gfx10.3.
                    if (IsGfx103Plus(*m_pDevice) == false)
                    {
                        m_select.sqg[info.instance].perfmon[idx].most.SQC_BANK_MASK = DefaultSqSelectBankMask;
                    }

                    mapping.counterId = idx;
                    searching = false;
                }
            }

            if (searching)
            {
                // There are no more global counters in this instance.
                result = Result::ErrorInvalidValue;
            }
        }
        else if (info.block == GpuBlock::SqWgp)
        {
            PAL_ASSERT(IsGfx11(*m_pDevice));
            // The SQ counters are 32-bit.
            mapping.dataType = PerfCounterDataType::Uint32;

            // The SQ has special registers so it needs its own implementation.
            if (m_select.sqWgp[info.instance].hasCounters == false)
            {
                // Turn on this instance and populate its GRBM_GFX_INDEX.
                m_select.sqWgp[info.instance].hasCounters  = true;
                m_select.sqWgp[info.instance].grbmGfxIndex = BuildGrbmGfxIndex(instanceMapping, info.block);
            }

            bool searching = true;

            for (uint32 idx = 0; searching && (idx < ArrayLen(m_select.sqWgp[info.instance].perfmon)); idx += 2)
            {
                if (m_select.sqWgp[info.instance].perfmonInUse[idx] == false)
                {
                    // Our SQ PERF_SEL fields are 9 bits. Verify that our event ID can fit.
                    PAL_ASSERT(info.eventId <= ((1 << 9) - 1));

                    bool skip = false;
                    for (uint32 instance = 0; instance < ArrayLen(m_select.sqWgp); instance++)
                    {
                        // SQWGP select programming is broadcast per SE, so prevent two instances within the same se
                        // from using different programming
                        if (m_select.sqWgp[instance].perfmonInUse[idx] &&
                            (m_select.sqWgp[instance].grbmGfxIndex.bits.SE_INDEX ==
                                m_select.sqWgp[info.instance].grbmGfxIndex.bits.SE_INDEX) &&
                            (m_select.sqWgp[instance].perfmon[idx].bits.PERF_SEL != info.eventId))
                        {
                            skip = true;
                        }
                    }

                    if (skip)
                    {
                        continue;
                    }

                    m_select.sqWgp[info.instance].perfmonInUse[idx] = true;

                    PAL_ASSERT(((idx & 0x3) == 0) || (info.eventId <= SP_PERF_SEL_VALU_PENDING_QUEUE_STALL__GFX11));
                    m_select.sqWgp[info.instance].perfmon[idx].bits.PERF_SEL  = info.eventId;
                    m_select.sqWgp[info.instance].perfmon[idx].bits.SPM_MODE  = PERFMON_SPM_MODE_OFF;
                    m_select.sqWgp[info.instance].perfmon[idx].bits.PERF_MODE = PERFMON_COUNTER_MODE_ACCUM;

                    // "Control registers 0,2,4,...,14 map to data registers 0,1,2,...,7."
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
            mapping.dataType = PerfCounterDataType::Uint64;

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
        else if (info.block == GpuBlock::Umcch)
        {
            // The UMCCH counters are 64-bit.
            mapping.dataType = PerfCounterDataType::Uint64;

            // Find the next unused global counter in the special UMCCH state.
            bool searching = true;

            for (uint32 idx = 0; searching && (idx < ArrayLen(m_select.umcch[info.instance].perfmonInUse)); ++idx)
            {
                if (m_select.umcch[info.instance].perfmonInUse[idx] == false)
                {
                    // UMCCH EventSelect fields are 8 bits. Verify that our event ID can fit.
                    PAL_ASSERT(info.eventId <= ((1 << 8) - 1));

                    m_select.umcch[info.instance].hasCounters       = true;
                    m_select.umcch[info.instance].perfmonInUse[idx] = true;
                    m_select.umcch[info.instance].thresholdSet[idx] = false;
                    m_select.umcch[info.instance].perfmonCntl[idx].most.EventSelect = info.eventId;
                    m_select.umcch[info.instance].perfmonCntl[idx].most.Enable      = 1;

                    if (IsGfx103Plus(*m_pDevice) && ((info.subConfig.umc.rdWrMask >= 1) && (info.subConfig.umc.rdWrMask <= 2)))
                    {
                        m_select.umcch[info.instance].perfmonCntl[idx].most.RdWrMask = info.subConfig.umc.rdWrMask;
                    }

                    if (IsGfx103Plus(*m_pDevice) &&
                        ((info.subConfig.umc.eventThresholdEn != 0) || (info.subConfig.umc.eventThreshold != 0)))
                    {
                        // If the client sets these extra values trust that they've got it right.
                        //   Set ThreshCntEn = 2 for > (1 for <).
                        //   Set ThreshCnt to the amount to compare against.

                        // "DcqOccupancy" replaces earlier asics fixed DcqOccupancy_00/25/50/75/90 buckets.
                        // Current DCQ is 64x1 in size, so to replicate old fixed events set:
                        //   ThreshCntEn=2 (>)
                        //   ThreshCnt=00 to count all (>0%), 14 to count >25%, 31 to count >50%, 47 to count >75%
                        m_select.umcch[info.instance].perfmonCtrHi[idx].nv2x.ThreshCntEn = info.subConfig.umc.eventThresholdEn;
                        m_select.umcch[info.instance].perfmonCtrHi[idx].nv2x.ThreshCnt   = info.subConfig.umc.eventThreshold;

                        // flag that we need to configure threshold for this event
                        m_select.umcch[info.instance].thresholdSet[idx] = true;
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
        else if (info.block == GpuBlock::DfMall)
        {
            // The DF counters are 64-bit.
            mapping.dataType = PerfCounterDataType::Uint64;

            const uint32        subInstance = info.instance;
            DfSelectState*const pSelect     = &m_select.df;

            // Find the next unused global counter in the special DF state.
            bool searching = true;
            for (uint32 idx = 0; searching && (idx < ArrayLen(pSelect->perfmonConfig)); ++idx)
            {
                if (pSelect->perfmonConfig[idx].perfmonInUse == false)
                {
                    pSelect->hasCounters                      = true;
                    pSelect->perfmonConfig[idx].perfmonInUse  = true;
                    pSelect->perfmonConfig[idx].eventSelect   = GetMallEventSelect(info.eventId, subInstance);
                    pSelect->perfmonConfig[idx].eventUnitMask = info.subConfig.df.eventQualifier & 0xFFFF;

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
        else if (m_select.pGeneric[block] != nullptr)
        {
            // All generic global counters are 64-bit.
            mapping.dataType = PerfCounterDataType::Uint64;

            // Finally, handle all generic blocks.
            GenericBlockSelect*const pSelect = &m_select.pGeneric[block][info.instance];

            if (m_select.pGeneric[block][info.instance].hasCounters == false)
            {
                // Turn on this instance and populate its GRBM_GFX_INDEX.
                pSelect->hasCounters  = true;
                pSelect->grbmGfxIndex = BuildGrbmGfxIndex(instanceMapping, info.block);
            }

            // Find and enable a global counter. All of the counter user guides say that the modules need to be
            // enabled in counter register# order. This ordering is different between cfg and select styles but
            // we already abstracted that using the module type.
            bool searching = true;

            for (uint32 moduleIdx = 0; searching && (moduleIdx < pSelect->numModules); ++moduleIdx)
            {
                if (pSelect->pModules[moduleIdx].inUse == 0)
                {
                    switch (pSelect->pModules[moduleIdx].type)
                    {
                    case SelectType::Perfmon:
                        // Our generic select PERF_SEL fields are 10 bits. Verify that our event ID can fit.
                        PAL_ASSERT(info.eventId <= ((1 << 10) - 1));

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
                        pSelect->pModules[moduleIdx].legacySel.bits.PERF_MODE = PERFMON_COUNTER_MODE_ACCUM;
                        break;

                    case SelectType::LegacyCfg:
                        // Our cfg PERF_SEL fields are 8 bits. Verify that our event ID can fit.
                        PAL_ASSERT(info.eventId <= ((1 << 8) - 1));

                        // A global counter uses the whole legacy module (0xF).
                        pSelect->pModules[moduleIdx].inUse                    = 0xF;
                        pSelect->pModules[moduleIdx].legacyCfg.bits.PERF_SEL  = info.eventId;
                        pSelect->pModules[moduleIdx].legacyCfg.bits.PERF_MODE = PERFMON_COUNTER_MODE_ACCUM;
                        pSelect->pModules[moduleIdx].legacyCfg.bits.ENABLE    = 1;
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
// - COUNTER_MODE_ACCUM is equal to zero but we still set it to be explicit.
// - PAL currently hard-codes 16-bit SPM in every block that supports it. If some block only supports 32-bit SPM then
//   we hard-code that configuration. The client does not control the SPM counter bit-depth.
Result PerfExperiment::AddSpmCounter(
    const Pal::PerfCounterInfo& info,
    SpmCounterMapping*          pMapping)
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
    // 32-bit SPM wire is hooked up to the selected module and which 16-bit sub-counters we selected within that wire.
    // In 16-bit mode we just use one sub-counter, in 32-bit mode we must use both sub-counters.
    const uint32 block          = uint32(info.block);
    uint32       spmWire        = 0;
    uint32       subCounterMask = 0;

    // Note that "LEVEL" counters require us to use the no-clamp & no-reset SPM mode.
    constexpr PERFMON_SPM_MODE SpmModeTable[2][2] =
    {
        { PERFMON_SPM_MODE_16BIT_CLAMP, PERFMON_SPM_MODE_16BIT_NO_CLAMP },
        { PERFMON_SPM_MODE_32BIT_CLAMP, PERFMON_SPM_MODE_32BIT_NO_CLAMP }
    };

    if (result == Result::Success)
    {
        // The SQG doesn't support 16-bit counters and only has one 32-bit counter per select register.
        const bool is32Bit = ((info.block == GpuBlock::Sq) || (info.counterType == PerfCounterType::Spm32));

        if (info.block == GpuBlock::Sq)
        {
            // The SQG has special registers so it needs its own implementation.
            if (m_select.sqg[info.instance].hasCounters == false)
            {
                // Turn on this instance and populate its GRBM_GFX_INDEX.
                m_select.sqg[info.instance].hasCounters  = true;
                m_select.sqg[info.instance].grbmGfxIndex = BuildGrbmGfxIndex(instanceMapping, info.block);
            }

            // Note that "LEVEL" counters require us to use the no-clamp & no-reset SPM mode.
            const bool   isLevel       = IsSqLevelEvent(info.eventId);
            const uint32 sqgNumModules = IsGfx11(*m_pDevice) ? Gfx11MaxSqgPerfmonModules : Gfx9MaxSqgPerfmonModules;
            bool         searching     = true;

            for (uint32 idx = 0; searching && (idx < sqgNumModules); ++idx)
            {
                if (m_select.sqg[info.instance].perfmonInUse[idx] == false)
                {
                    // Our SQG PERF_SEL fields are 9 bits. Verify that our event ID can fit.
                    PAL_ASSERT(info.eventId <= ((1 << 9) - 1));

                    m_select.sqg[info.instance].perfmonInUse[idx]           = true;
                    m_select.sqg[info.instance].perfmon[idx].bits.PERF_SEL  = info.eventId;
                    m_select.sqg[info.instance].perfmon[idx].bits.SPM_MODE  = SpmModeTable[is32Bit][isLevel];
                    m_select.sqg[info.instance].perfmon[idx].bits.PERF_MODE = PERFMON_COUNTER_MODE_ACCUM;

                    // The SQC bank mask was removed in gfx10.3.
                    if (IsGfx103Plus(*m_pDevice) == false)
                    {
                        m_select.sqg[info.instance].perfmon[idx].most.SQC_BANK_MASK = DefaultSqSelectBankMask;
                    }

                    // Each SQ module gets a single wire with one 32-bit counter (select both 16-bit halves).
                    spmWire        = idx;
                    subCounterMask = 0x3;
                    searching      = false;
                }
            }

            if (searching)
            {
                // There are no more compatible SPM counters in this instance.
                result = Result::ErrorInvalidValue;
            }
        }
        else if (info.block == GpuBlock::SqWgp)
        {
            PAL_ASSERT(IsGfx11(*m_pDevice));
            // The SQ has special registers so it needs its own implementation.
            if (m_select.sqWgp[info.instance].hasCounters == false)
            {
                // Turn on this instance and populate its GRBM_GFX_INDEX.
                m_select.sqWgp[info.instance].hasCounters  = true;
                m_select.sqWgp[info.instance].grbmGfxIndex = BuildGrbmGfxIndex(instanceMapping, info.block);
            }

            bool         searching = true;
            const uint32 stride    = is32Bit ? 2 : 1;

            for (uint32 idx = 0; searching && (idx < ArrayLen(m_select.sqWgp[info.instance].perfmon)); idx += stride)
            {
                if (m_select.sqWgp[info.instance].perfmonInUse[idx] == false)
                {
                    // Our SQ PERF_SEL fields are 9 bits. Verify that our event ID can fit.
                    PAL_ASSERT(info.eventId <= ((1 << 9) - 1));

                    bool skip = false;
                    for (uint32 instance = 0; instance < ArrayLen(m_select.sqWgp); instance++)
                    {
                        // SQWGP select programming is broadcast per SE, so prevent two instances within the same se
                        // from using different programming
                        if (m_select.sqWgp[instance].perfmonInUse[idx] &&
                            (m_select.sqWgp[instance].grbmGfxIndex.bits.SE_INDEX ==
                                m_select.sqWgp[info.instance].grbmGfxIndex.bits.SE_INDEX) &&
                            (m_select.sqWgp[instance].perfmon[idx].bits.PERF_SEL != info.eventId))
                        {
                            skip = true;
                        }
                    }

                    if (skip)
                    {
                        continue;
                    }

                    // Note that "LEVEL" counters require us to use the no-clamp & no-reset SPM mode.
                    const bool isLevel = IsSqWgpLevelEvent(info.eventId);

                    m_select.sqWgp[info.instance].perfmonInUse[idx]           = true;
                    m_select.sqWgp[info.instance].perfmon[idx].bits.PERF_SEL  = info.eventId;
                    m_select.sqWgp[info.instance].perfmon[idx].bits.SPM_MODE  = SpmModeTable[is32Bit][isLevel];
                    m_select.sqWgp[info.instance].perfmon[idx].bits.PERF_MODE = PERFMON_COUNTER_MODE_ACCUM;

                    if (is32Bit)
                    {
                        // 32bit uses two 16bit slots, so reserve the second slot
                        // Hardware doesnt use the "odd" selects in 32bit mode, but maintain an acceptable programming
                        m_select.sqWgp[info.instance].perfmonInUse[idx + 1]   = true;
                        m_select.sqWgp[info.instance].perfmon[idx + 1].u32All = 0;

                        subCounterMask = 3;
                    }
                    else
                    {
                        subCounterMask = 1 << (idx % 2);
                    }

                    // The gfx11 SQ counters share one 32-bit accumulator/wire per pair of selects.
                    // The even selects get the first 16-bit half and the odds get the second half.
                    spmWire        = idx / 2;
                    searching      = false;
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
                pSelect->hasCounters  = true;
                pSelect->grbmGfxIndex = BuildGrbmGfxIndex(instanceMapping, info.block);
            }

            // Search for an unused 16-bit sub-counter. This will need to be reworked when we add 32-bit support.
            bool searching = true;

            for (uint32 idx = 0; idx < pSelect->numModules; idx++)
            {
                if (pSelect->pModules[idx].type == SelectType::Perfmon)
                {
                    // Our generic select PERF_SEL fields are 10 bits. Verify that our event ID can fit.
                    PAL_ASSERT(info.eventId <= ((1 << 10) - 1));

                    // Each write holds two 16-bit sub-counters. We must check each wire individually because
                    // some blocks look like they have a whole perfmon module but only use half of it.
                    if (spmWire < m_counterInfo.block[block].numSpmWires)
                    {
                        if (TestAnyFlagSet(pSelect->pModules[idx].inUse, 0x1) == false)
                        {
                            // Each 32bit module requires both 16-bit halves.
                            subCounterMask = is32Bit ? 0x3 : 0x1;

                            pSelect->pModules[idx].inUse                      |= subCounterMask;
                            pSelect->pModules[idx].perfmon.sel0.bits.PERF_SEL  = info.eventId;
                            // No other block needs the no_clamp behavior in the SpmModeTable
                            pSelect->pModules[idx].perfmon.sel0.bits.CNTR_MODE = SpmModeTable[is32Bit][0];
                            pSelect->pModules[idx].perfmon.sel0.bits.PERF_MODE = PERFMON_COUNTER_MODE_ACCUM;

                            searching      = false;
                            break;
                        }
                        else if (TestAnyFlagSet(pSelect->pModules[idx].inUse, 0x2) == false)
                        {
                            pSelect->pModules[idx].inUse                       |= 0x2;
                            pSelect->pModules[idx].perfmon.sel0.bits.PERF_SEL1  = info.eventId;
                            pSelect->pModules[idx].perfmon.sel0.bits.PERF_MODE1 = PERFMON_COUNTER_MODE_ACCUM;

                            subCounterMask = 0x2;
                            searching      = false;
                            break;
                        }

                        spmWire++;
                    }

                    if (spmWire < m_counterInfo.block[block].numSpmWires)
                    {
                        if (TestAnyFlagSet(pSelect->pModules[idx].inUse, 0x4) == false)
                        {
                            // Each 32bit module requires both 16-bit halves.
                            subCounterMask = is32Bit ? 0x3 : 0x1;

                            pSelect->pModules[idx].inUse                       |= (subCounterMask << 2);
                            pSelect->pModules[idx].perfmon.sel1.bits.PERF_SEL2  = info.eventId;
                            pSelect->pModules[idx].perfmon.sel1.bits.PERF_MODE2 = PERFMON_COUNTER_MODE_ACCUM;

                            searching      = false;
                            break;
                        }
                        else if (TestAnyFlagSet(pSelect->pModules[idx].inUse, 0x8) == false)
                        {
                            pSelect->pModules[idx].inUse                       |= 0x8;
                            pSelect->pModules[idx].perfmon.sel1.bits.PERF_SEL3  = info.eventId;
                            pSelect->pModules[idx].perfmon.sel1.bits.PERF_MODE3 = PERFMON_COUNTER_MODE_ACCUM;

                            subCounterMask = 0x2;
                            searching      = false;
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

            if (info.block == GpuBlock::GeSe)
            {
                // The GE2_SE is odd because it has one instance per-SE, programmed in the SE_INDEX, but it's actually
                // a global block so its SPM data goes into the global SPM data segment.
                pMapping->segment = SpmDataSegmentType::Global;
            }
            else
            {
                pMapping->segment = (m_counterInfo.block[block].distribution == PerfCounterDistribution::GlobalBlock)
                                        ? SpmDataSegmentType::Global
                                        : static_cast<SpmDataSegmentType>(instanceMapping.seIndex);
            }

            if (HasRmiSubInstances(info.block) && ((info.instance % Gfx10NumRmiSubInstances) != 0))
            {
                // Odd instances are the second set of counters in the real HW RMI instance.
                spmWire += 2;
            }

            // We expect this is 0x1 or 0x2 for a 16-bit counter or 0x3 for a 32-bit counter.
            PAL_ASSERT((subCounterMask >= 0x1) && (subCounterMask <= 0x3));

            if (TestAnyFlagSet(subCounterMask, 0x1))
            {
                // We want the lower 16 bits of this wire.
                pMapping->isEven     = true;
                pMapping->evenMuxsel = BuildMuxselEncoding(instanceMapping, info.block, 2 * spmWire);
            }

            if (TestAnyFlagSet(subCounterMask, 0x2))
            {
                // We want the upper 16 bits of this wire.
                pMapping->isOdd     = true;
                pMapping->oddMuxsel = BuildMuxselEncoding(instanceMapping, info.block, 2 * spmWire + 1);
            }
        }
    }

    return result;
}

// =====================================================================================================================
// It looks like the client can only call this function once per PerfExperiment which makes things simple. It must:
// - Add one SPM counter for each counter in the trace.
// - Store some global SPM state.
Result PerfExperiment::AddThreadTrace(
    const ThreadTraceInfo& traceInfo)
{
    Result result = Result::Success;

    const uint32 realInstance = VirtualSeToRealSe(traceInfo.instance);

    if (m_isFinalized)
    {
        // The perf experiment cannot be changed once it is finalized.
        result = Result::ErrorUnavailable;
    }
    // Validate the trace info.
    else if (traceInfo.instance >= m_chipProps.gfx9.numActiveShaderEngines)
    {
        // There's one thread trace instance per SQG.
        result = Result::ErrorInvalidValue;
    }
    else if (m_sqtt[realInstance].inUse)
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
             (traceInfo.optionValues.threadTraceTargetSh >= m_chipProps.gfx9.numShaderArrays))
    {
        // The detailed shader array is out of bounds.
        result = Result::ErrorInvalidValue;
    }
    else if ((traceInfo.optionFlags.threadTraceTargetCu != 0) &&
             (traceInfo.optionValues.threadTraceTargetCu >= m_chipProps.gfx9.maxNumCuPerSh))
    {
        // The detailed CU is out of bounds. This does not check whether the CU is active, merely that it exists physically.
        result = Result::ErrorInvalidValue;
    }
    else if ((traceInfo.optionFlags.threadTraceSimdMask != 0) &&
             (TestAnyFlagSet(traceInfo.optionValues.threadTraceSimdMask, ~SqttDetailedSimdMask)))
    {
        // A SIMD is selected that doesn't exist.
        result = Result::ErrorInvalidValue;
    }
    else if ((traceInfo.optionFlags.threadTraceShaderTypeMask != 0) &&
             ((traceInfo.optionValues.threadTraceShaderTypeMask & ~PerfShaderMaskAll) != 0))
    {
        // What is this shader stage?
        result = Result::ErrorInvalidValue;
    }
    else if ((traceInfo.optionFlags.threadTraceStallBehavior != 0) &&
             (traceInfo.optionValues.threadTraceStallBehavior > GpuProfilerStallNever))
    {
        // The stall mode is invalid.
        result = Result::ErrorInvalidValue;
    }
    else if ((traceInfo.optionFlags.threadTraceSimdMask != 0) &&
             (IsPowerOfTwo(traceInfo.optionValues.threadTraceSimdMask) == false))
    {
        // The SIMD mask is treated as an index on gfx10+, use IsPowerOfTwo to check that only one bit is set.
        result = Result::ErrorInvalidValue;
    }
    else if ((traceInfo.optionFlags.threadTraceSh0CounterMask != 0) ||
             (traceInfo.optionFlags.threadTraceSh1CounterMask != 0) ||
             (traceInfo.optionFlags.threadTraceVmIdMask != 0)       ||
             (traceInfo.optionFlags.threadTraceIssueMask != 0)      ||
             (traceInfo.optionFlags.threadTraceWrapBuffer != 0))
    {
        // None of these options can be supported on gfx10+.
        result = Result::ErrorInvalidValue;
    }

    // Note that threadTraceRandomSeed cannot be implemented on gfx9+ but using it shouldn't cause an error because
    // doing nothing with the seed should still give us deterministic traces.

    if (result == Result::Success)
    {
        m_perfExperimentFlags.sqtTraceEnabled = true;

        // Set all sqtt properties for this trace except for the buffer offset which is found during Finalize.
        m_sqtt[realInstance].inUse = true;
        m_sqtt[realInstance].bufferSize = (traceInfo.optionFlags.bufferSize != 0)
                ? traceInfo.optionValues.bufferSize : SqttDefaultBufferSize;

        // Default to all shader stages enabled.
        const PerfExperimentShaderFlags shaderMask = (traceInfo.optionFlags.threadTraceShaderTypeMask != 0)
                ? traceInfo.optionValues.threadTraceShaderTypeMask : PerfShaderMaskAll;

        // Default to getting detailed tokens from shader array 0.
        const uint32 shIndex = (traceInfo.optionFlags.threadTraceTargetSh != 0)
                ? traceInfo.optionValues.threadTraceTargetSh : 0;

        // Target this trace's specific SE and SH.
        m_sqtt[realInstance].grbmGfxIndex.bits.SE_INDEX = realInstance;
        m_sqtt[realInstance].grbmGfxIndex.bits.SA_INDEX = shIndex;
        m_sqtt[realInstance].grbmGfxIndex.bits.INSTANCE_BROADCAST_WRITES = 1;

        // By default stall always so that we get accurate data.
        const uint32 stallMode =
            (traceInfo.optionFlags.threadTraceStallBehavior != 0) ? traceInfo.optionValues.threadTraceStallBehavior :
                                                                    GpuProfilerStallAlways;

        uint32 cuIndex = 0;
        if (traceInfo.optionFlags.threadTraceTargetCu != 0)
        {
            cuIndex = traceInfo.optionValues.threadTraceTargetCu;
        }
        else
        {
            // Pick a default detailed token WGP/CU within our shader array. Default to only selecting WGPs/CUs that are
            // active and not reserved for realtime use. Note that there is no real time WGP mask, but all of the
            // CU masks are still populated with two adjacent bits set for each WGP.
            const uint32 traceableCuMask =
                m_chipProps.gfx9.activeCuMask[realInstance][shIndex] & ~m_chipProps.gfxip.realTimeCuMask;

            const int32 customDefaultSqttDetailedCuIndex = m_pDevice->Settings().defaultSqttDetailedCuIndex;

            if (customDefaultSqttDetailedCuIndex >= 0)
            {
                if (BitfieldIsSet(traceableCuMask, customDefaultSqttDetailedCuIndex))
                {
                    cuIndex = customDefaultSqttDetailedCuIndex;
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
                if (BitMaskScanForward(&cuIndex, traceableCuMask) == false)
                {
                    // We should always have at least one non-realtime CU.
                    PAL_ASSERT_ALWAYS();
                }

#if PAL_AMDGPU_BUILD
                if (IsGfx11(*m_pDevice))
                {
                    cuIndex = (m_chipProps.gfx9.gfx10.maxNumWgpPerSa - 1) * 2;
                }
#endif
            }
        }

        // Note that gfx10 has new thread trace modes. For now we use "on" to match the gfx9 implementation.
        // We may want to consider using one of the new modes by default.
        m_sqtt[realInstance].ctrl.bits.MODE          = SQ_TT_MODE_ON;
        m_sqtt[realInstance].ctrl.bits.HIWATER       = SqttGfx10HiWaterValue;
        m_sqtt[realInstance].ctrl.bits.UTIL_TIMER    = 1;
        m_sqtt[realInstance].ctrl.bits.RT_FREQ       = SQ_TT_RT_FREQ_4096_CLK;
        m_sqtt[realInstance].ctrl.bits.DRAW_EVENT_EN = 1;

        if (IsGfx103PlusExclusive(*m_pDevice))
        {
            m_sqtt[realInstance].ctrl.gfx103PlusExclusive.LOWATER_OFFSET = SqttGfx103LoWaterOffsetValue;

            // On Navi2x hw, the polarity of AutoFlushMode is inverted, thus this step is necessary to correct
            m_sqtt[realInstance].ctrl.gfx103PlusExclusive.AUTO_FLUSH_MODE = m_settings.waAutoFlushModePolarityInversed;
        }

        // Enable all stalling in "always" mode, "lose detail" mode only disables register stalls.
        if (IsGfx10(*m_pDevice))
        {
            m_sqtt[realInstance].ctrl.gfx10.REG_STALL_EN      = (stallMode == GpuProfilerStallAlways);
            m_sqtt[realInstance].ctrl.gfx10.SPI_STALL_EN      = (stallMode != GpuProfilerStallNever);
            m_sqtt[realInstance].ctrl.gfx10.SQ_STALL_EN       = (stallMode != GpuProfilerStallNever);
            m_sqtt[realInstance].ctrl.gfx10.REG_DROP_ON_STALL = (stallMode != GpuProfilerStallAlways);
        }
        else if (IsGfx11(*m_pDevice))
        {
            m_sqtt[realInstance].ctrl.gfx11.SPI_STALL_EN      = (stallMode != GpuProfilerStallNever);
            m_sqtt[realInstance].ctrl.gfx11.SQ_STALL_EN       = (stallMode != GpuProfilerStallNever);
            m_sqtt[realInstance].ctrl.gfx11.REG_AT_HWM        = (stallMode == GpuProfilerStallAlways) ? 2 :
                                                                (stallMode != GpuProfilerStallAlways) ? 1 : 0;
        }

        static_assert((uint32(PerfShaderMaskPs) == uint32(SQ_TT_WTYPE_INCLUDE_PS_BIT) &&
                       uint32(PerfShaderMaskGs) == uint32(SQ_TT_WTYPE_INCLUDE_GS_BIT) &&
                       uint32(PerfShaderMaskHs) == uint32(SQ_TT_WTYPE_INCLUDE_HS_BIT) &&
                       uint32(PerfShaderMaskCs) == uint32(SQ_TT_WTYPE_INCLUDE_CS_BIT) &&
                       uint32(PerfShaderMaskVs) == uint32(SQ_TT_WTYPE_INCLUDE_VS_BIT__GFX10) &&
                       uint32(PerfShaderMaskEs) == uint32(SQ_TT_WTYPE_INCLUDE_ES_BIT__GFX10) &&
                       uint32(PerfShaderMaskLs) == uint32(SQ_TT_WTYPE_INCLUDE_LS_BIT__GFX10)),
                      "We assume that the SQ_TT_WTYPE enum matches PerfExperimentShaderFlags.");

        if (IsGfx11(*m_pDevice))
        {
            // ES/LS are unsupported, unset those flags
            uint32 validFlags = ~(uint32(PerfShaderMaskEs) | uint32(PerfShaderMaskLs));
            if (m_pDevice->ChipProperties().gfxip.supportsHwVs == false)
            {
                // When the HW-VS is unsupported, unset that flag too
                validFlags &= ~(uint32(PerfShaderMaskVs));
            }
            m_sqtt[realInstance].mask.bits.WTYPE_INCLUDE = uint32(shaderMask) & validFlags;
            m_sqtt[realInstance].mask.most.EXCLUDE_NONDETAIL_SHADERDATA =
                (traceInfo.optionFlags.threadTraceExcludeNonDetailShaderData != 0) &&
                (traceInfo.optionValues.threadTraceExcludeNonDetailShaderData);
        }
        else
        {
            m_sqtt[realInstance].mask.bits.WTYPE_INCLUDE = shaderMask;
        }

        m_sqtt[realInstance].mask.bits.SA_SEL = shIndex;

        // Divide by two to convert from CUs to WGPs.
        m_sqtt[realInstance].mask.bits.WGP_SEL = cuIndex / 2;

        // Default to getting detailed tokens from SIMD 0.
        m_sqtt[realInstance].mask.bits.SIMD_SEL =
            (traceInfo.optionFlags.threadTraceSimdMask != 0) ? traceInfo.optionValues.threadTraceSimdMask : 0;

        if (traceInfo.optionFlags.threadTraceTokenConfig != 0)
        {
            m_sqtt[realInstance].tokenMask =
                GetGfx10SqttTokenMask(*m_pDevice, traceInfo.optionValues.threadTraceTokenConfig);
        }
        else
        {
            // By default trace all tokens and registers.
            SetSqttTokenExclude(*m_pDevice, &m_sqtt[realInstance].tokenMask, SqttGfx10TokenMaskDefault);
            m_sqtt[realInstance].tokenMask.bits.REG_INCLUDE = SqttGfx10RegMaskDefault;

            if (IsGfx103PlusExclusive(*m_pDevice))
            {
                m_sqtt[realInstance].tokenMask.gfx103PlusExclusive.REG_EXCLUDE = SqttGfx103RegExcludeMaskDefault;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// The KMD is responsible for actually adding the counters so we just need to attach the create info to
// the experiment. This includes a pointer to the buffers that have not been created yet, but will be created
// in GpaSession::AcquireDfGpuMem().
Result PerfExperiment::AddDfSpmTrace(
    const SpmTraceCreateInfo& dfSpmCreateInfo)
{
    Result result = Result::Success;
    if (m_isFinalized)
    {
        // The perf experiment cannot be changed once it is finalized.
        result = Result::ErrorUnavailable;
    }
    else if (dfSpmCreateInfo.ringSize > UINT32_MAX)
    {
        // The ring size register is only 32 bits and its value must be aligned.
        result = Result::ErrorInvalidValue;
    }
    else if (dfSpmCreateInfo.spmInterval < 1)
    {
        // The sample interval must be at least 1.
        result = Result::ErrorInvalidValue;
    }
    else if (m_perfExperimentFlags.perfCtrsEnabled && HasGlobalDfCounters())
    {
        // DF SPM cannot be enabled if there are alreay DF cumulative counters.
        result = Result::ErrorInitializationFailed;
    }
    else
    {
        m_numDfSpmCounters = dfSpmCreateInfo.numPerfCounters;
        m_pDfSpmCounters   = PAL_NEW_ARRAY(SpmCounterMapping, m_numDfSpmCounters, m_pPlatform, AllocObject);

        m_perfExperimentFlags.dfSpmTraceEnabled  = true;
        m_dfSpmPerfmonInfo.perfmonUsed           = m_numDfSpmCounters;
        m_dfSpmPerfmonInfo.samplingIntervalNs    = static_cast<uint16>(dfSpmCreateInfo.spmInterval);

        for (uint32 i = 0; i < dfSpmCreateInfo.numPerfCounters; i++)
        {
            const Pal::PerfCounterInfo& info = dfSpmCreateInfo.pPerfCounterInfos[i];

            m_pDfSpmCounters[i].general.block          = info.block;
            m_pDfSpmCounters[i].general.eventId        = info.eventId;
            m_pDfSpmCounters[i].general.globalInstance = info.instance;

            // The instance to eventId mapping here is probably not right and hasn't been tested at all.
            // Does DF SPM even work on gfx11 now that we need to think in terms of MCDs?
            PAL_ASSERT(IsGfx11(*m_pDevice) == false);

            m_dfSpmPerfmonInfo.perfmonEvents[i]    = GetMallEventSelect(info.eventId, info.instance);
            m_dfSpmPerfmonInfo.perfmonUnitMasks[i] = info.subConfig.df.eventQualifier & 0xFF;
        }

        result = AllocateDfSpmBuffers(dfSpmCreateInfo.ringSize);
    }

    return result;
}

// ====================================================================================================================
// Acquires additional buffers for the DF SPM trace.
Result PerfExperiment::AllocateDfSpmBuffers(
    gpusize    dfSpmBufferSize)
{
    GpuMemoryCreateInfo createInfo = {};
    createInfo.size       = Pow2Align(dfSpmBufferSize, DfSpmBufferAlignment);
    createInfo.alignment  = DfSpmBufferAlignment;
    createInfo.vaRange    = VaRange::Default;
    createInfo.priority   = GpuMemPriority::High;
    createInfo.mallPolicy = GpuMemMallPolicy::Never;
    createInfo.flags.gl2Uncached  = 1;
    createInfo.flags.cpuInvisible = 1;
    // Ensure a fall back to local is available in case there is no Invisible Memory.
    if (m_pDevice->HeapLogicalSize(GpuHeapInvisible) > 0)
    {
        createInfo.heapCount = 3;
        createInfo.heaps[0] = GpuHeapInvisible;
        createInfo.heaps[1] = GpuHeapLocal;
        createInfo.heaps[2] = GpuHeapGartCacheable;
    }
    else
    {
        createInfo.heapCount = 2;
        createInfo.heaps[0] = GpuHeapLocal;
        createInfo.heaps[1] = GpuHeapGartCacheable;
    }

    GpuMemoryInternalCreateInfo internalCreateInfo = {};
    internalCreateInfo.flags.dfSpmTraceBuffer = 1;
    internalCreateInfo.flags.alwaysResident   = 1;

    Result result = m_pDevice->CreateInternalGpuMemory(createInfo,
                                                       internalCreateInfo,
                                                       &m_dfSpmPerfmonInfo.pDfSpmTraceBuffer);

    if (result == Result::Success)
    {
        createInfo.size      = sizeof(DfSpmTraceMetadataLayout);
        createInfo.alignment = DfSpmBufferAlignment;

        internalCreateInfo.flags.dfSpmTraceBuffer = 0;
        result = m_pDevice->CreateInternalGpuMemory(createInfo,
                                                    internalCreateInfo,
                                                    &m_dfSpmPerfmonInfo.pDfSpmMetadataBuffer);
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

    if (m_isFinalized || (m_numSpmCounters != 0))
    {
        // The perf experiment cannot be changed once it is finalized. You also can't have more than one SPM trace.
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
    // The global segment always starts with a 64-bit timestamp, we need to save space for it.
    constexpr uint32 GlobalTimestampCounters = sizeof(uint64) / sizeof(uint16);

    // Allocate the segment memory. SE segments are virtualized so harvested SEs are clustered at the end and must have
    // zero counters enabled. Note that AddSpmCounter already verified all of our counters are for active SEs.
    for (uint32 segment = 0; (result == Result::Success) && (segment < MaxNumSpmSegments); ++segment)
    {
        // Start by calculating the total size of the ram.
        const bool isGlobalSegment = (static_cast<SpmDataSegmentType>(segment) == SpmDataSegmentType::Global);
        uint32     evenCounters    = isGlobalSegment ? GlobalTimestampCounters : 0;
        uint32     oddCounters     = 0;

        for (uint32 idx = 0; idx < m_numSpmCounters; ++idx)
        {
            if (uint32(m_pSpmCounters[idx].segment) == segment)
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

            // We don't want to include the global segment in our "max among all SEs" calculation.
            if ((isGlobalSegment == false) && (m_maxSeMuxSelLines < totalLines))
            {
                m_maxSeMuxSelLines = totalLines;
            }
        }
    }

    for (int32 segment = 0; (result == Result::Success) && (segment < MaxNumSpmSegments); segment++)
    {
        if(m_numMuxselLines[segment] == 0)
        {
            continue;
        }

        m_pMuxselRams[segment] = PAL_NEW_ARRAY(SpmLineMapping, m_numMuxselLines[segment], m_pPlatform, AllocObject);

        if (m_pMuxselRams[segment] == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }
        else
        {
            // The ram is POD so just zero it out. Note that zero is a muxsel mapping that means "I don't care".
            memset(m_pMuxselRams[segment], 0, sizeof(*m_pMuxselRams[segment]) * m_numMuxselLines[segment]);
        }
    }

    if (result == Result::Success)
    {
        // Now we know how big all of the segments are so we can figure out where each counter will fit in the sample
        // memory layout. It's time to find those offsets and fill out the muxsel values.
        //
        // The RLC hardware hard-codes this segment order: Global, SE0, SE1, ... , SEN.
        // We'll build up the counter locations in that order to make thinking about this stuff easier.
        //
        // The Global segment is always first so let's just handle it explicitly.
        FillMuxselRam(SpmDataSegmentType::Global, 0);

        const uint32 globalLines = m_numMuxselLines[uint32(SpmDataSegmentType::Global)];

        // Now we just need to fill out the per-SE state. We purposefully defined our SE segment enums in terms of
        // virtual (active) SEs so looping over the active SEs will give us the right segment enums for free.
        static_assert(uint32(SpmDataSegmentType::Se0) == 0);

        if (IsGfx11(*m_pDevice))
        {
            // Gfx11's RLC uses one segment size for every single SE (SE_NUM_SEGMENT) which has two impacts:
            // 1. If we vary the number of counters between SEs then we need to pad out the smaller ones. In the worst
            //    case we need a full SE's worth of space for every SE with zero counters.
            // 2. If our GPU has harvested SEs we need to include padding for them too! The RLC works in terms of real
            //    SEs, not the "virtual" SE range we prefer in this code.
            //
            // These properties make it hard to track the offset as we go. It's easier to recompute it each iteration.
            for (uint32 idx = 0; idx < m_chipProps.gfx9.numActiveShaderEngines; ++idx)
            {
                const uint32 offset = globalLines + VirtualSeToRealSe(idx) * m_maxSeMuxSelLines;

                PAL_ASSERT(idx < uint32(SpmDataSegmentType::Global));
                FillMuxselRam(static_cast<SpmDataSegmentType>(idx), offset);
            }

            // Once more to compute the total sample size.
            // Note that we're using the physical SE count here because that's the count the RLC uses.
            m_spmSampleLines = globalLines + m_chipProps.gfx9.numShaderEngines * m_maxSeMuxSelLines;
        }
        else
        {
            uint32 offset = globalLines;

            for (uint32 idx = 0; idx < m_chipProps.gfx9.numActiveShaderEngines; ++idx)
            {
                PAL_ASSERT(idx < uint32(SpmDataSegmentType::Global));
                FillMuxselRam(static_cast<SpmDataSegmentType>(idx), offset);

                // The RLC tightly packs the enabled segments so we don't need to explicitly map between virtual and
                // physical SEs. For example, if SE0 has no counters or is harvested the RLC will see SE0_NUM_LINE = 0
                // so it knows SE1 comes next in memory, no padding necessary! Adding the current SE's size here
                // will always give us the proper offset for the next enabled physical SE.
                offset += m_numMuxselLines[idx];
            }

            // The offset now points past the very last segment so it's actually the sample size.
            m_spmSampleLines = offset;
        }

        // Now for one final trick, we need to tweak our SPM ring buffer size. This implements part of the SPM parsing
        // scheme described in palPerfExperiment.h above the SpmTraceLayout struct so read that first.
        //
        // PAL is a UMD so we can't program SPM as a proper ring buffer. Instead we tell the RLC to automatically wrap
        // back to the start of the ring when it reaches the end. The RLC can split sample writes across the wrap point
        // which makes it difficult to parse the samples out in order.
        //
        // We can avoid that issue if we carefully select our ring size to make the wrapping line up perfectly.
        // Essentially we just need our ring size to be a multiple of our sample size, that way the final sample in
        // the ring ends exactly when the ring ends. Each time the ring wraps the first wrapping sample starts at the
        // top of the ring. That means the client can always start parsing samples at the top of the ring and the data
        // will make perfect sense, no need to check for wrapping!
        //
        // The client gave us a suggested ring buffer size in the create info. We shouldn't use more memory than they
        // specified but we can use less. This code figures out how many whole samples fit in their ring size and then
        // converts that back up to bytes to get PAL's final ring size. Note that configuring a ring with no sample
        // space doesn't make sense so we do bump that up to enough memory for a single sample.
        //
        // Note that the RLC reserves one full bitline at the very start of the ring for the ring buffer header.
        // The samples start immediately after that header and the wrapping logic skips over the header.
        const uint32 maxSizeInLines = Max(1u, uint32(spmCreateInfo.ringSize) / SampleLineSizeInBytes);

        m_spmMaxSamples = Max(1u, (maxSizeInLines - 1) / m_spmSampleLines);
        m_spmRingSize   = (m_spmMaxSamples * m_spmSampleLines + 1) * SampleLineSizeInBytes;

        // If we made it this far the SPM trace is ready to go.
        m_perfExperimentFlags.spmTraceEnabled = true;
        m_spmSampleInterval                   = uint16(spmCreateInfo.spmInterval);
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
// Walks the specified segment's muxsel RAM, filling out the RAM's contents and updating the relevant SPM counter's
// offsets. It's safe/harmless to call this on segments with no counters/RAM.
void PerfExperiment::FillMuxselRam(
    SpmDataSegmentType segment,
    uint32             offsetInLines)
{
    PAL_ASSERT(uint32(segment) < MaxNumSpmSegments);
    SpmLineMapping*const pMappings = m_pMuxselRams[uint32(segment)];

    if (pMappings != nullptr)
    {
        // Walk through the even and odd lines in parallel, adding all enabled counters. In this logic we assume all
        // counters are 16-bit even if we're running 32-bit SPM. This works out fine because the RLC splits all values
        // into 16-bit chunks and writes them to memory independently.
        uint32 evenCounterIdx = 0;
        uint32 evenLineIdx    = 0;
        uint32 oddCounterIdx  = 0;
        uint32 oddLineIdx     = 1;

        if (segment == SpmDataSegmentType::Global)
        {
            // The global segment always starts with a 64-bit timestamp, that's 4 16-bit counters worth of data.
            constexpr uint32 NumGlobalTimestampCounters = sizeof(uint64) / sizeof(uint16);
            MuxselEncoding timestampMuxsel              = {};
            timestampMuxsel.u16All                      = 0xF0F0;

            for (; evenCounterIdx < NumGlobalTimestampCounters; evenCounterIdx++)
            {
                // Gfx11 requires different timestamp programming
                if (IsGfx11(*m_pDevice))
                {
                    timestampMuxsel                = {};
                    timestampMuxsel.gfx11.block    = 31; // RSPM
                    timestampMuxsel.gfx11.instance =  2; // REFCLK timestamp count
                    timestampMuxsel.gfx11.counter  = evenCounterIdx;
                }

                pMappings[evenLineIdx].muxsel[evenCounterIdx] = timestampMuxsel;
            }
        }

        for (uint32 idx = 0; idx < m_numSpmCounters; ++idx)
        {
            SpmCounterMapping*const pCounter = m_pSpmCounters + idx;

            if (pCounter->segment == segment)
            {
                if (pCounter->isEven)
                {
                    // If this counter has an even part it always contains the lower 16 bits. Find its offset
                    // within each sample in units of 16-bit counters and then convert that to bytes.
                    const uint32 offset = (offsetInLines + evenLineIdx) * MuxselLineSizeInCounters + evenCounterIdx;
                    pCounter->offsetLo  = offset * sizeof(uint16);

                    // Copy the counter's muxsel into the even line.
                    pMappings[evenLineIdx].muxsel[evenCounterIdx] = pCounter->evenMuxsel;

                    // Move on to the next even counter, possibly skipping over an odd line.
                    if (++evenCounterIdx == MuxselLineSizeInCounters)
                    {
                        evenCounterIdx = 0;
                        evenLineIdx += 2;
                    }
                }

                if (pCounter->isOdd)
                {
                    // If this counter is even and odd it must be 32-bit and this must be the upper half.
                    // Otherwise this counter is 16-bit and it's the lower half. Find its offset
                    // within each sample in units of 16-bit counters and then convert that to bytes.
                    const uint32 offset = (offsetInLines + oddLineIdx) * MuxselLineSizeInCounters + oddCounterIdx;

                    if (pCounter->isEven)
                    {
                        pCounter->offsetHi = offset * sizeof(uint16);
                    }
                    else
                    {
                        pCounter->offsetLo = offset * sizeof(uint16);
                    }

                    // Copy the counter's muxsel into the odd line.
                    pMappings[oddLineIdx].muxsel[oddCounterIdx] = pCounter->oddMuxsel;

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
                const bool                 is64Bit  = (pMapping->dataType == PerfCounterDataType::Uint64);

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

        // If someone enabled any SQ counters and the "never stop" workaround is on we must take special action.
        m_neverStopCounters = m_settings.waNeverStopSqCounters && m_select.sqg->hasCounters;

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
            pLayout->samples[idx].dataType         = mapping.dataType;
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
                    pLayout->traces[traceIdx].shaderEngine = RealSeToVirtualSe(idx);
                    pLayout->traces[traceIdx].infoOffset   = m_sqtt[idx].infoOffset;
                    pLayout->traces[traceIdx].infoSize     = sizeof(ThreadTraceInfoData);
                    pLayout->traces[traceIdx].dataOffset   = m_sqtt[idx].bufferOffset;
                    pLayout->traces[traceIdx].dataSize     = m_sqtt[idx].bufferSize;

                    // Our thread trace tools seem to expect that this is in units of WGPs.
                    pLayout->traces[traceIdx].computeUnit  = m_sqtt[idx].mask.bits.WGP_SEL;
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
        pLayout->offset           = m_spmRingOffset;
        pLayout->wrPtrOffset      = 0; // The write pointer is the first thing written to the ring buffer
        pLayout->wrPtrGranularity = 1;

        if (IsGfx11(*m_pDevice))
        {
            // On GFX11, the write pointer written to the ring is in units of segments and therefore must be multiplied
            // by SampleLineSizeInBytes (32) to get the correct data size.
            pLayout->wrPtrGranularity = SampleLineSizeInBytes;
        }

        // The samples start one line in.
        pLayout->sampleOffset  = SampleLineSizeInBytes;
        pLayout->sampleStride  = SampleLineSizeInBytes * m_spmSampleLines;
        pLayout->maxNumSamples = m_spmMaxSamples;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 810
        pLayout->wptrOffset        = pLayout->offset + pLayout->wrPtrOffset;
        pLayout->wptrGranularity   = pLayout->wrPtrGranularity;
        pLayout->sampleSizeInBytes = pLayout->sampleStride;

        for (uint32 idx = 0; idx < MaxNumSpmSegments; ++idx)
        {
            pLayout->segmentSizeInBytes[idx] = m_numMuxselLines[idx] * SampleLineSizeInBytes;
        }
#endif

        pLayout->numCounters = m_numSpmCounters;

        for (uint32 idx = 0; idx < m_numSpmCounters; ++idx)
        {
            const SpmCounterMapping& mapping = m_pSpmCounters[idx];
            SpmCounterData*const     pOut    = pLayout->counterData + idx;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 810
            pOut->segment  = mapping.segment;
            pOut->offset   = mapping.offsetLo / sizeof(uint16); // In units of counters!
#endif
            pOut->gpuBlock = mapping.general.block;
            pOut->instance = mapping.general.globalInstance;
            pOut->eventId  = mapping.general.eventId;
            pOut->offsetLo = mapping.offsetLo;

            // The client needs to combine the low and high halves of each 32-bit value.
            if (mapping.isEven && mapping.isOdd)
            {
                pOut->is32Bit  = true;
                pOut->offsetHi = mapping.offsetHi;
            }
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

        regCP_PERFMON_CNTL cpPerfmonCntl = {};
        // Disable and reset all types of perf counters. We will enable the counters when everything is ready.
        // Note that PERFMON_ENABLE_MODE controls per-context filtering which we don't support.
        cpPerfmonCntl.bits.PERFMON_STATE = CP_PERFMON_STATE_DISABLE_AND_RESET;
        cpPerfmonCntl.bits.SPM_PERFMON_STATE = STRM_PERFMON_STATE_DISABLE_AND_RESET;
        cpPerfmonCntl.bits.PERFMON_ENABLE_MODE = CP_PERFMON_ENABLE_MODE_ALWAYS_COUNT;

        pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmCP_PERFMON_CNTL, cpPerfmonCntl.u32All, pCmdSpace);

        // On GFX11, perfmon clock state toggling is handled by KMD via PAL's SetClockMode() escape as this field was
        // moved into a privileged config register.
        if (IsGfx11(*m_pDevice) == false)
        {
            // The RLC controls perfmon clock gating. Before doing anything else we should turn on perfmon clocks.
            regRLC_PERFMON_CLK_CNTL rlcPerfmonClkCntl  = {};
            rlcPerfmonClkCntl.bits.PERFMON_CLOCK_STATE = 1;

            pCmdSpace = pCmdStream->WriteSetOneConfigReg(Gfx10::mmRLC_PERFMON_CLK_CNTL,
                                                         rlcPerfmonClkCntl.u32All,
                                                         pCmdSpace);
        }

        // Thread traces and many types of perf counters require SQG events. To keep things simple we should just
        // enable them unconditionally. This shouldn't have any effect in the cases that don't really need them on.
        pCmdSpace = WriteUpdateSpiConfigCntl(true, pCmdStream, pCmdSpace);

        if (m_perfExperimentFlags.perfCtrsEnabled || m_perfExperimentFlags.spmTraceEnabled)
        {
            // SQ_PERFCOUNTER_CTRL controls how the SQs increment their perf counters. We treat it as global state.
            regSQ_PERFCOUNTER_CTRL sqPerfCounterCtrl = {};

            // By default sample from all shader stages.
            PerfExperimentShaderFlags sqShaderMask = PerfShaderMaskAll;

            if (IsGfx11(*m_pDevice))
            {
                if (m_createInfo.optionFlags.sqWgpShaderMask != 0)
                {
                    // Gfx11 added new SQ-per-WGP counters which took over the old SQ_PERFCOUNTER registers. That means
                    // we should use PAL's sqWgpShaderMask when programming SQ_PERFCOUNTER_CTRL on gfx11. On older HW
                    // we should use sqShaderMask because the SQ registers used to control the SQG counters.
                    sqShaderMask = m_createInfo.optionValues.sqWgpShaderMask;
                }
            }
            else
            {
                if (m_createInfo.optionFlags.sqShaderMask != 0)
                {
                    sqShaderMask = m_createInfo.optionValues.sqShaderMask;
                }
            }

            sqPerfCounterCtrl.bits.PS_EN = TestAnyFlagSet(sqShaderMask, PerfShaderMaskPs);
            sqPerfCounterCtrl.bits.GS_EN = TestAnyFlagSet(sqShaderMask, PerfShaderMaskGs);
            sqPerfCounterCtrl.bits.HS_EN = TestAnyFlagSet(sqShaderMask, PerfShaderMaskHs);
            sqPerfCounterCtrl.bits.CS_EN = TestAnyFlagSet(sqShaderMask, PerfShaderMaskCs);

            if (m_pDevice->ChipProperties().gfxip.supportsHwVs)
            {
                sqPerfCounterCtrl.gfx10.VS_EN = TestAnyFlagSet(sqShaderMask, PerfShaderMaskVs);
            }

            if (IsGfx10(*m_pDevice))
            {
                sqPerfCounterCtrl.gfx10.LS_EN = TestAnyFlagSet(sqShaderMask, PerfShaderMaskLs);
                sqPerfCounterCtrl.gfx10.ES_EN = TestAnyFlagSet(sqShaderMask, PerfShaderMaskEs);
            }

            // Note that we must write this after CP_PERFMON_CNTRL because the CP ties ownership of this state to it.
            pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmSQ_PERFCOUNTER_CTRL, sqPerfCounterCtrl.u32All, pCmdSpace);

            if (IsGfx11(*m_pDevice))
            {
                // Gfx11 split the SQG shader controls out into the new SQG_PERFCOUNTER_CTRL. Note that PAL's
                // GpuBlock::Sq maps to the SQG counters so we use sqShaderMask here.
                regSQG_PERFCOUNTER_CTRL sqgPerfCounterCtrl = {};

                const PerfExperimentShaderFlags sqgShaderMask =
                    (m_createInfo.optionFlags.sqShaderMask != 0) ? m_createInfo.optionValues.sqShaderMask
                                                                 : PerfShaderMaskAll;

                sqgPerfCounterCtrl.bits.PS_EN = TestAnyFlagSet(sqgShaderMask, PerfShaderMaskPs);
                sqgPerfCounterCtrl.bits.GS_EN = TestAnyFlagSet(sqgShaderMask, PerfShaderMaskGs);
                sqgPerfCounterCtrl.bits.HS_EN = TestAnyFlagSet(sqgShaderMask, PerfShaderMaskHs);
                sqgPerfCounterCtrl.bits.CS_EN = TestAnyFlagSet(sqgShaderMask, PerfShaderMaskCs);

                pCmdSpace = pCmdStream->WriteSetOneConfigReg(Gfx11::mmSQG_PERFCOUNTER_CTRL,
                                                             sqgPerfCounterCtrl.u32All,
                                                             pCmdSpace);
            }
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
                pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(PS_PARTIAL_FLUSH, engineType, pCmdSpace);
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
                cpPerfmonCntl.bits.SPM_PERFMON_STATE = STRM_PERFMON_STATE_START_COUNTING;
            }

            pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmCP_PERFMON_CNTL, cpPerfmonCntl.u32All, pCmdSpace);

            // Also enable windowed perf counters. This most likely applies to many block types, rather than try to
            // find them all just always send the event (it shouldn't hurt). This should be required by legacy counters
            // and SPM counters.
            pCmdSpace = WriteUpdateWindowedCounters(true, pCmdStream, pCmdSpace);

            // Enable all of the cfg-style global counters. Each block has an extra enable register. Only clear them
            // if we didn't call WriteStopAndSampleGlobalCounters which already clears them and assumes we're not
            // going to reset the counters again after taking the initial sample.
            pCmdSpace = WriteEnableCfgRegisters(true, (m_perfExperimentFlags.perfCtrsEnabled == false),
                                                pCmdStream, pCmdSpace);
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
            cpPerfmonCntl.bits.PERFMON_STATE     = CP_PERFMON_STATE_DISABLE_AND_RESET;
            cpPerfmonCntl.bits.SPM_PERFMON_STATE = m_neverStopCounters ? STRM_PERFMON_STATE_DISABLE_AND_RESET
                                                                       : STRM_PERFMON_STATE_STOP_COUNTING;

            pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmCP_PERFMON_CNTL, cpPerfmonCntl.u32All, pCmdSpace);
        }

        if (m_perfExperimentFlags.sqtTraceEnabled)
        {
            // Stop all thread traces and copy back some information not contained in the thread trace tokens.
            pCmdSpace = WriteStopThreadTraces(pCmdBuffer, pCmdStream, pCmdSpace);
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
        cpPerfmonCntl.bits.PERFMON_STATE     = CP_PERFMON_STATE_DISABLE_AND_RESET;
        cpPerfmonCntl.bits.SPM_PERFMON_STATE = STRM_PERFMON_STATE_DISABLE_AND_RESET;

        pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmCP_PERFMON_CNTL, cpPerfmonCntl.u32All, pCmdSpace);

        // Restore SPI_CONFIG_CNTL by turning SQG events back off.
        pCmdSpace = WriteUpdateSpiConfigCntl(false, pCmdStream, pCmdSpace);

        // See IssueBegin(). PERFMON_CLOCK_STATE is handled by KMD on GFX11.
        if (IsGfx11(*m_pDevice) == false)
        {
            // The RLC controls perfmon clock gating. Before we're done here, we must turn the perfmon clocks back off.
            regRLC_PERFMON_CLK_CNTL rlcPerfmonClkCntl  = {};
            rlcPerfmonClkCntl.bits.PERFMON_CLOCK_STATE = 0;

            pCmdSpace = pCmdStream->WriteSetOneConfigReg(Gfx10::mmRLC_PERFMON_CLK_CNTL,
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

        if (m_perfExperimentFlags.perfCtrsEnabled == 0)
        {
            cpPerfmonCntl.bits.PERFMON_STATE = CP_PERFMON_STATE_DISABLE_AND_RESET;
        }
        else if (m_neverStopCounters)
        {
            // It's not possible to pause global counters in this mode, we have to keep them running.
            // This basically breaks this pause functionality but there's nothing we can do about it.
            cpPerfmonCntl.bits.PERFMON_STATE = CP_PERFMON_STATE_START_COUNTING;
        }
        else
        {
            cpPerfmonCntl.bits.PERFMON_STATE = CP_PERFMON_STATE_STOP_COUNTING;
        }

        if (m_perfExperimentFlags.spmTraceEnabled == 0)
        {
            cpPerfmonCntl.bits.SPM_PERFMON_STATE = STRM_PERFMON_STATE_DISABLE_AND_RESET;
        }
        else if (m_neverStopCounters)
        {
            // It's not possible to pause SPM counters in this mode, we have to keep them running.
            // This basically breaks this pause functionality but there's nothing we can do about it.
            cpPerfmonCntl.bits.SPM_PERFMON_STATE = STRM_PERFMON_STATE_START_COUNTING;
        }
        else
        {
            cpPerfmonCntl.bits.SPM_PERFMON_STATE = STRM_PERFMON_STATE_STOP_COUNTING;
        }

        pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmCP_PERFMON_CNTL, cpPerfmonCntl.u32All, pCmdSpace);

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
        cpPerfmonCntl.bits.PERFMON_STATE       =
            m_perfExperimentFlags.perfCtrsEnabled ? CP_PERFMON_STATE_START_COUNTING
                                                  : CP_PERFMON_STATE_DISABLE_AND_RESET;
        cpPerfmonCntl.bits.SPM_PERFMON_STATE   =
            m_perfExperimentFlags.spmTraceEnabled ? STRM_PERFMON_STATE_START_COUNTING
                                                  : STRM_PERFMON_STATE_DISABLE_AND_RESET;
        cpPerfmonCntl.bits.PERFMON_ENABLE_MODE = CP_PERFMON_ENABLE_MODE_ALWAYS_COUNT;

        pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmCP_PERFMON_CNTL, cpPerfmonCntl.u32All, pCmdSpace);
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
                pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmGRBM_GFX_INDEX,
                                                             m_sqtt[idx].grbmGfxIndex.u32All,
                                                             pCmdSpace);

                regSQ_THREAD_TRACE_TOKEN_MASK tokenMask = GetGfx10SqttTokenMask(*m_pDevice, sqttTokenConfig);

                // These fields aren't controlled by the token config.
                tokenMask.bits.INST_EXCLUDE   = m_sqtt[idx].tokenMask.bits.INST_EXCLUDE;
                tokenMask.bits.REG_DETAIL_ALL = m_sqtt[idx].tokenMask.bits.REG_DETAIL_ALL;

                const uint32 regAddr = IsGfx11(*m_pDevice) ? Gfx11::mmSQ_THREAD_TRACE_TOKEN_MASK
                                                           : Gfx10::mmSQ_THREAD_TRACE_TOKEN_MASK;

                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regAddr, tokenMask.u32All, pCmdSpace);
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
    const Pal::Device&  palDevice  = *(device.Parent());
    CmdStream*const     pCmdStream = static_cast<CmdStream*>(pPalCmdStream);
    uint32*             pCmdSpace  = pCmdStream->ReserveCommands();

    const uint32 regAddr = IsGfx11(palDevice) ? Gfx11::mmSQ_THREAD_TRACE_TOKEN_MASK
                                              : Gfx10::mmSQ_THREAD_TRACE_TOKEN_MASK;

    const regSQ_THREAD_TRACE_TOKEN_MASK tokenMask = GetGfx10SqttTokenMask(palDevice, sqttTokenConfig);

    // Note that we will lose the current value of the INST_EXCLUDE and REG_DETAIL_ALL fields. They default
    // to zero so hopefully the default value is fine.
    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regAddr, tokenMask.u32All, pCmdSpace);
    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Fills out a CounterMapping based on an interface perf counter. It also validates the counter information.
Result PerfExperiment::BuildCounterMapping(
    const Pal::PerfCounterInfo& info,
    CounterMapping*             pMapping
    ) const
{
    Result result = Result::Success;

    if (info.block >= GpuBlock::Count)
    {
        // What is this block?
        result = Result::ErrorInvalidValue;
    }
    else if (m_counterInfo.block[uint32(info.block)].distribution == PerfCounterDistribution::Unavailable)
    {
        // This block is not available on this GPU.
        result = Result::ErrorInvalidValue;
    }
    else if (info.instance > m_counterInfo.block[uint32(info.block)].numInstances)
    {
        // This instance doesn't exist.
        result = Result::ErrorInvalidValue;
    }
    else if (info.eventId > m_counterInfo.block[uint32(info.block)].maxEventId)
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

    const PerfCounterBlockInfo& blockInfo = m_counterInfo.block[uint32(block)];

    if (blockInfo.distribution == PerfCounterDistribution::GlobalBlock)
    {
        // Global blocks have a one-to-one instance mapping.
        instanceIndex = globalInstance;
    }
    else if (blockInfo.distribution == PerfCounterDistribution::PerShaderEngine)
    {
        // We want the SE index to be the outer index and the local instance to be the inner index.
        seIndex       = globalInstance / blockInfo.numScopedInstances;
        instanceIndex = globalInstance % blockInfo.numScopedInstances;
    }
    else if (blockInfo.distribution == PerfCounterDistribution::PerShaderArray)
    {
        // From outermost to innermost, the internal indices are in the order: SE, SA, local instance.
        seIndex       = (globalInstance / blockInfo.numScopedInstances) / m_chipProps.gfx9.numShaderArrays;
        saIndex       = (globalInstance / blockInfo.numScopedInstances) % m_chipProps.gfx9.numShaderArrays;
        instanceIndex = globalInstance % blockInfo.numScopedInstances;

        if (HasRmiSubInstances(block))
        {
            // Pairs of perfcounter sub-instances are sequential, so we can convert to the proper HW instance ID by
            // dividing by the number of sub-instances.
            instanceIndex /= Gfx10NumRmiSubInstances;
        }
    }

    if (seIndex >= m_chipProps.gfx9.numActiveShaderEngines)
    {
        // This virtual shader engine doesn't exist on our device.
        result = Result::ErrorInvalidValue;
    }
    else if (saIndex >= m_chipProps.gfx9.numShaderArrays)
    {
        // This shader array doesn't exist on our device.
        result = Result::ErrorInvalidValue;
    }
    else if (instanceIndex >= blockInfo.numScopedInstances)
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
// Fills out a GRBM_GFX_INDEX for some block based on an InstanceMapping
regGRBM_GFX_INDEX PerfExperiment::BuildGrbmGfxIndex(
    const InstanceMapping& mapping,
    GpuBlock               block
    ) const
{
    regGRBM_GFX_INDEX grbmGfxIndex = {};
    grbmGfxIndex.bits.SE_INDEX = VirtualSeToRealSe(mapping.seIndex);
    grbmGfxIndex.bits.SA_INDEX = mapping.saIndex;

    switch (m_counterInfo.block[uint32(block)].distribution)
    {
    case PerfCounterDistribution::GlobalBlock:
        // Global block writes should broadcast to SEs and SAs.
        grbmGfxIndex.bits.SE_BROADCAST_WRITES = 1;
        [[fallthrough]];
    case PerfCounterDistribution::PerShaderEngine:
        // Per-SE block writes should broadcast to SAs.
        grbmGfxIndex.bits.SA_BROADCAST_WRITES = 1;
        break;

    default:
        // Otherwise no broadcast bits should be set.
        break;
    }

    // Some gfx10 blocks use a different instance index format that requires some bit swizzling.
    uint32 instance = mapping.instanceIndex;

    // Note that SQ registers would normally require a special per-SIMD instance index format but the SQ perf counter
    // registers are special. All SQ and SQC perf counters are implemented in the per-SE SQG block. Thus we don't
    // need any special handing for the SQ or SQC here, we can just pass along our flat index.
    if ((block == GpuBlock::Ta) || (block == GpuBlock::Td) || (block == GpuBlock::Tcp))
    {
        // The shader array hardware defines this instance index format.
        union
        {
            struct
            {
                uint32 blockIndex :  2; // The index of the block within the WGP.
                uint32 wgpIndex   :  3; // The WGP index within the SPI side of this shader array.
                uint32 isBelowSpi :  1; // 0 - The side with lower WGP numbers, 1 - the side with higher WGP numbers.
                uint32 reserved   : 26;
            } bits;

            uint32 u32All;
        } instanceIndex = {};

        // These blocks are per-CU.
        constexpr uint32 NumCuPerWgp    = 2;
        const     uint32 numWgpAboveSpi = m_chipProps.gfx9.gfx10.numWgpAboveSpi;
        const     uint32 flatWgpIndex   = mapping.instanceIndex / NumCuPerWgp;
        const     bool   isBelowSpi     = (flatWgpIndex >= numWgpAboveSpi);

        instanceIndex.bits.blockIndex = mapping.instanceIndex % NumCuPerWgp;
        instanceIndex.bits.wgpIndex   = isBelowSpi ? (flatWgpIndex - numWgpAboveSpi) : flatWgpIndex;
        instanceIndex.bits.isBelowSpi = isBelowSpi;

        instance = instanceIndex.u32All;
    }
    else if (IsGfx11(m_chipProps.gfxLevel) && (block == GpuBlock::SqWgp))
    {
        // The shader array hardware defines this instance index format.
        union
        {
            struct
            {
                uint32 blockIndex : 2; // The index of the block within the WGP.
                uint32 wgpIndex   : 3; // The WGP index within the SPI side of this shader array.
                uint32 isBelowSpi : 1; // 0 - The side with lower WGP numbers, 1 - the side with higher WGP numbers.
                uint32 reserved   : 26;
            } bits;

            uint32 u32All;
        } instanceIndex = {};

        // Based on code from Pal::Gfx9::InitializeGpuChipProperties below:
        // pInfo->gfx9.gfx10.numWgpAboveSpi = 4; // GPU__GC__NUM_WGP0_PER_SA
        // pInfo->gfx9.gfx10.numWgpBelowSpi = 0; // GPU__GC__NUM_WGP1_PER_SA
        // We can see that instance 0-3 are wgp above spi, 0 is the nearest one to spi.
        const     uint32 numWgpAboveSpi = m_chipProps.gfx9.gfx10.numWgpAboveSpi;
        const     bool   isBelowSpi     = (mapping.instanceIndex >= numWgpAboveSpi);
        instanceIndex.bits.wgpIndex     = isBelowSpi ? (mapping.instanceIndex - numWgpAboveSpi)
                                                     : mapping.instanceIndex;
        instanceIndex.bits.isBelowSpi   = isBelowSpi;

        instance = instanceIndex.u32All;
    }

    grbmGfxIndex.most.INSTANCE_INDEX = instance;

    return grbmGfxIndex;
}

// =====================================================================================================================
// A helper function for AddSpmCounter which builds a muxsel struct given some counter information.
MuxselEncoding PerfExperiment::BuildMuxselEncoding(
    const InstanceMapping& mapping,
    GpuBlock               block,
    uint32                 counter
    ) const
{
    MuxselEncoding              muxsel    = {};
    const PerfCounterBlockInfo& blockInfo = m_counterInfo.block[uint32(block)];

    if (IsGfx11(m_chipProps.gfxLevel))
    {
        if (block == GpuBlock::SqWgp)
        {
            muxsel.gfx11Wgp.counter     = counter;
            muxsel.gfx11Wgp.instance    = 0;
            muxsel.gfx11Wgp.wgp         = mapping.instanceIndex;
            muxsel.gfx11Wgp.shaderArray = mapping.saIndex;
            muxsel.gfx11Wgp.block       = blockInfo.spmBlockSelect;
        }
        else
        {
            // Check that we can re-use the SA form for Glb and SE blocks
            PAL_ASSERT(mapping.instanceIndex < (1 << 5)); // 5 == bitwidth of instance field

            // Other WGP blocks can use WGP+Instance as a joint CU ID same as per SA
            muxsel.gfx11.counter     = counter;
            muxsel.gfx11.instance    = mapping.instanceIndex;
            muxsel.gfx11.shaderArray = mapping.saIndex;
            muxsel.gfx11.block       = blockInfo.spmBlockSelect;
        }
    }
    else if (blockInfo.distribution == PerfCounterDistribution::GlobalBlock)
    {
        muxsel.gfx9.counter  = counter;
        muxsel.gfx9.block    = blockInfo.spmBlockSelect;
        muxsel.gfx9.instance = mapping.instanceIndex;
    }
    else if (block == GpuBlock::GeSe)
    {
        // The GE2_SE is odd because it has one instance per-SE, programmed in the SE_INDEX, but it's actually a global
        // block so we need to use the global muxsel encoding and pass in the SE_INDEX as the instance.
        muxsel.gfx9.counter  = counter;
        muxsel.gfx9.block    = blockInfo.spmBlockSelect;
        muxsel.gfx9.instance = mapping.seIndex;
    }
    else
    {
        uint32 counterId = counter;

        if (HasRmiSubInstances(block) && (m_chipProps.gfxLevel < GfxIpLevel::GfxIp10_3))
        {
            // Use a non-default mapping of counter select IDs for the RMI block. This changes the counter
            // mapping from {0,..7} to {4,5,6,7,0,1,2,3}
            counterId = (counterId < 4) ? (counterId + 4) : (counterId - 4);
        }

        // Gfx10 per-SE muxsels use a slightly different encoding.
        muxsel.gfx10Se.counter     = counterId;
        muxsel.gfx10Se.block       = blockInfo.spmBlockSelect;
        muxsel.gfx10Se.shaderArray = mapping.saIndex;
        muxsel.gfx10Se.instance    = mapping.instanceIndex;
    }

    return muxsel;
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
    const gpusize ringBaseAddr = m_gpuMemory.GpuVirtAddr() + m_spmRingOffset;

    // The spec requires that the ring address and size be aligned to 32-bytes.
    PAL_ASSERT(IsPow2Aligned(ringBaseAddr,  SpmRingBaseAlignment));
    PAL_ASSERT(IsPow2Aligned(m_spmRingSize, SpmRingBaseAlignment));

    // Zero out the 64-bit timestamp at the start of the final sample in the ring buffer. Recall that we carefully
    // sized the ring to have no extra space at the end, that's why we can just subtract the size of one sample.
    // This implements part of the SPM parsing scheme described in palPerfExperiment.h above the SpmTraceLayout
    // struct so read that too.
    const uint32  ZeroTs[2] = {};
    WriteDataInfo writeZero = {};
    writeZero.engineType = pCmdStream->GetEngineType();
    writeZero.engineSel  = engine_sel__me_write_data__micro_engine;
    writeZero.dstAddr    = ringBaseAddr + m_spmRingSize - m_spmSampleLines * SampleLineSizeInBytes;
    writeZero.dstSel     = dst_sel__me_write_data__tc_l2;
    pCmdSpace += CmdUtil::BuildWriteData(writeZero, ArrayLen(ZeroTs), ZeroTs, pCmdSpace);

    // Configure the RLC state that controls SPM.
    struct
    {
        regRLC_SPM_PERFMON_CNTL         cntl;
        regRLC_SPM_PERFMON_RING_BASE_LO ringBaseLo;
        regRLC_SPM_PERFMON_RING_BASE_HI ringBaseHi;
        regRLC_SPM_PERFMON_RING_SIZE    ringSize;
    } rlcInit = {};

    rlcInit.cntl.bits.PERFMON_RING_MODE       = 0; // No stall and no interupt on overflow.
    rlcInit.cntl.bits.PERFMON_SAMPLE_INTERVAL = m_spmSampleInterval;
    rlcInit.ringBaseLo.bits.RING_BASE_LO      = LowPart(ringBaseAddr);
    rlcInit.ringBaseHi.bits.RING_BASE_HI      = HighPart(ringBaseAddr);
    rlcInit.ringSize.bits.RING_BASE_SIZE      = m_spmRingSize;

    pCmdSpace = pCmdStream->WriteSetSeqConfigRegs(mmRLC_SPM_PERFMON_CNTL,
                                                  mmRLC_SPM_PERFMON_RING_SIZE,
                                                  &rlcInit,
                                                  pCmdSpace);

    // We have to set this register on Navi3X. The HW uses this value as offset. If PAL doesn't zero out this register
    // than the WRPTR value only continues to grow. This moves the result data further and further into the SPM data
    // buffer. Originally an undocumented change in the SPM initialization procedure from Navi2X where we don't have to
    // set this register. The docs have been updated to include this register. Also listed in the new HW documentation
    // are the RLC_SPM_SEGMENT_THRESHOLD and RLC_SPM_RING_RDPTR registers. PAL is intentionally not setting those
    // registers because PAL doesn't have SPM stall's or SPM interrupts enabled. The documentation refers to this as
    // "RING_MODE == 0". If PAL ever trys to enable either one of those features then PAL should set both of those
    // registers below along with the RLC_SPM_RING_WRPTR. Be aware that in particular setting the RLC_SPM_RING_RDPTR
    // register requires "privilege" either enabled manually in the CP mircocode or by the KMD.
    const bool isGfx11 = IsGfx11(*m_pDevice);

    if (isGfx11)
    {
        regRLC_SPM_RING_WRPTR rlcSpmRingWrptr = {};
        rlcSpmRingWrptr.bits.PERFMON_RING_WRPTR = 0;
        pCmdSpace = pCmdStream->WriteSetOneConfigReg(Gfx11::mmRLC_SPM_RING_WRPTR, rlcSpmRingWrptr.u32All, pCmdSpace);
    }

    // RLC_SPM_ACCUM_MODE needs its state reset as we've disabled GPO when entering stable pstate.
    constexpr regRLC_SPM_ACCUM_MODE rlcSpmAccumMode = {};
    pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmRLC_SPM_ACCUM_MODE, rlcSpmAccumMode.u32All, pCmdSpace);

    // Program the muxsel line sizes.
    const uint32 globalLines = m_numMuxselLines[uint32(SpmDataSegmentType::Global)];

    if (isGfx11)
    {
        regRLC_SPM_PERFMON_SEGMENT_SIZE spmSegmentSize = {};

        // TOTAL_NUM_SEGMENT should be (global + SE_NUM_SEGMENT * numShaderEngines)
        spmSegmentSize.gfx11.TOTAL_NUM_SEGMENT  = m_spmSampleLines;
        spmSegmentSize.gfx11.GLOBAL_NUM_SEGMENT = globalLines;
        // There is only one SE segment size value for Gfx11. Every shader engine line count will be set to whatever
        // was the highest value found in the spm config.
        spmSegmentSize.gfx11.SE_NUM_SEGMENT     = m_maxSeMuxSelLines;

        pCmdSpace = pCmdStream->WriteSetOneConfigReg(Gfx11::mmRLC_SPM_PERFMON_SEGMENT_SIZE,
                                                     spmSegmentSize.u32All,
                                                     pCmdSpace);
    }
    else
    {
        // The SE# fields in the following registers are indexed by physical SEs but our m_numMuxselLines array is
        // index in terms of virtual SEs. Default to zero to zero out all harvested SEs.
        uint32 physicalSeLines[Gfx9MaxShaderEngines] = {};
        bool   over31Lines = (globalLines > 31);

        for (uint32 idx = 0; idx < m_chipProps.gfx9.numActiveShaderEngines; ++idx)
        {
            PAL_ASSERT(idx < uint32(SpmDataSegmentType::Global));
            physicalSeLines[VirtualSeToRealSe(idx)] = m_numMuxselLines[idx];
            over31Lines = over31Lines || (m_numMuxselLines[idx] > 31);
        }

        regRLC_SPM_PERFMON_SEGMENT_SIZE spmSegmentSize = {};

        if (over31Lines)
        {
            // We must use these extended registers when at least one segment is over 31 lines. The original
            // SEGMENT_SIZE register must still be written but it must be full of zeros.
            struct
            {
                regRLC_SPM_PERFMON_SE3TO0_SEGMENT_SIZE se3To0SegmentSize;
                regRLC_SPM_PERFMON_GLB_SEGMENT_SIZE    glbSegmentSize;
            } rlcExtendedSize = {};

            rlcExtendedSize.se3To0SegmentSize.bits.SE0_NUM_LINE      = physicalSeLines[0];
            rlcExtendedSize.se3To0SegmentSize.bits.SE1_NUM_LINE      = physicalSeLines[1];
            rlcExtendedSize.se3To0SegmentSize.bits.SE2_NUM_LINE      = physicalSeLines[2];
            rlcExtendedSize.se3To0SegmentSize.bits.SE3_NUM_LINE      = physicalSeLines[3];
            rlcExtendedSize.glbSegmentSize.bits.PERFMON_SEGMENT_SIZE = m_spmSampleLines;
            rlcExtendedSize.glbSegmentSize.bits.GLOBAL_NUM_LINE      = globalLines;

            pCmdSpace = pCmdStream->WriteSetSeqConfigRegs(Gfx10::mmRLC_SPM_PERFMON_SE3TO0_SEGMENT_SIZE,
                                                          Gfx10::mmRLC_SPM_PERFMON_GLB_SEGMENT_SIZE,
                                                          &rlcExtendedSize,
                                                          pCmdSpace);
        }
        else
        {
            spmSegmentSize.gfx10.PERFMON_SEGMENT_SIZE = m_spmSampleLines;
            spmSegmentSize.gfx10.SE0_NUM_LINE         = physicalSeLines[0];
            spmSegmentSize.gfx10.SE1_NUM_LINE         = physicalSeLines[1];
            spmSegmentSize.gfx10.SE2_NUM_LINE         = physicalSeLines[2];
            spmSegmentSize.gfx10.GLOBAL_NUM_LINE      = globalLines;
        }

        pCmdSpace = pCmdStream->WriteSetOneConfigReg(Gfx10::mmRLC_SPM_PERFMON_SEGMENT_SIZE,
                                                     spmSegmentSize.u32All,
                                                     pCmdSpace);
    }

    // Now upload each muxsel ram to the RLC. If a particular segment is empty we skip it.
    for (uint32 idx = 0; idx < MaxNumSpmSegments; ++idx)
    {
        if (m_numMuxselLines[idx] > 0)
        {
            WriteDataInfo writeData  = {};
            uint32        muxselAddr = 0;

            if (static_cast<SpmDataSegmentType>(idx) == SpmDataSegmentType::Global)
            {
                pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);

                writeData.dstAddr = isGfx11 ? Gfx11::mmRLC_SPM_GLOBAL_MUXSEL_DATA : Gfx10::mmRLC_SPM_GLOBAL_MUXSEL_DATA;
                muxselAddr        = isGfx11 ? Gfx11::mmRLC_SPM_GLOBAL_MUXSEL_ADDR : Gfx10::mmRLC_SPM_GLOBAL_MUXSEL_ADDR;
            }
            else
            {
                pCmdSpace = WriteGrbmGfxIndexBroadcastSe(idx, pCmdStream, pCmdSpace);

                writeData.dstAddr = isGfx11 ? Gfx11::mmRLC_SPM_SE_MUXSEL_DATA : Gfx10::mmRLC_SPM_SE_MUXSEL_DATA;
                muxselAddr        = isGfx11 ? Gfx11::mmRLC_SPM_SE_MUXSEL_ADDR : Gfx10::mmRLC_SPM_SE_MUXSEL_ADDR;
            }

            writeData.engineType = pCmdStream->GetEngineType();
            writeData.engineSel  = engine_sel__me_write_data__micro_engine;
            writeData.dstSel     = dst_sel__me_write_data__mem_mapped_register;

            // Each data value must be written into MUXSEL_DATA, if we let the CP increment the register address
            // we will overwrite other registers.
            writeData.dontIncrementAddr = true;

            // The muxsel ram is inlined into the command stream and could be large so we need a loop that carefully
            // splits it into chunks and repeatedly commits and reserves space. Note that the addr registers are
            // defintely user-config but we need to use SetOnePerfCtrReg to handle the isPerfCtr header bit. We
            // assume we get the user-config branch when defining PacketHeaders below.
            PAL_ASSERT(CmdUtil::IsUserConfigReg(muxselAddr));

            constexpr uint32 PacketHeaders = CmdUtil::ConfigRegSizeDwords + 1 + CmdUtil::WriteDataSizeDwords;
            const uint32     maxDwords     = pCmdStream->ReserveLimit() - PacketHeaders;
            const uint32     maxLines      = maxDwords / MuxselLineSizeInDwords;

            for (uint32 line = 0; line < m_numMuxselLines[idx]; line += maxLines)
            {
                const uint32 numLines = Min(maxLines, m_numMuxselLines[idx] - line);

                pCmdStream->CommitCommands(pCmdSpace);
                pCmdSpace = pCmdStream->ReserveCommands();

                // Each time we issue a new write_data we must first update MUXSEL_ADDR to point to the next muxsel.
                pCmdSpace  = pCmdStream->WriteSetOnePerfCtrReg(muxselAddr,
                                                               line * MuxselLineSizeInDwords,
                                                               pCmdSpace);

                pCmdSpace += m_cmdUtil.BuildWriteData(writeData,
                                                      numLines * MuxselLineSizeInDwords,
                                                      m_pMuxselRams[idx][line].u32Array,
                                                      pCmdSpace);

                pCmdStream->CommitCommands(pCmdSpace);
                pCmdSpace = pCmdStream->ReserveCommands();
            }
        }
    }

    static_assert(uint32(SpmDataSegmentType::Global) == uint32(SpmDataSegmentType::Count) - 1,
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

            pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmGRBM_GFX_INDEX,
                                                         m_sqtt[idx].grbmGfxIndex.u32All,
                                                         pCmdSpace);

            const gpusize shiftedAddr = (m_gpuMemory.GpuVirtAddr() + m_sqtt[idx].bufferOffset) >> SqttBufferAlignShift;
            const gpusize shiftedSize = m_sqtt[idx].bufferSize >> SqttBufferAlignShift;

            if (IsGfx11(*m_pDevice))
            {
                regSQ_THREAD_TRACE_BUF0_BASE sqttBuf0Base = {};
                regSQ_THREAD_TRACE_BUF0_SIZE sqttBuf0Size = {};

                sqttBuf0Size.bits.SIZE    = shiftedSize;
                sqttBuf0Size.bits.BASE_HI = HighPart(shiftedAddr);
                sqttBuf0Base.bits.BASE_LO = LowPart(shiftedAddr);

                // All of these registers were moved to privileged space in gfx10 which is pretty silly.
                // We need to write the thread trace buffer size register before the base address register.
                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(Gfx11::mmSQ_THREAD_TRACE_BUF0_SIZE,
                                                              sqttBuf0Size.u32All,
                                                              pCmdSpace);

                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(Gfx11::mmSQ_THREAD_TRACE_BUF0_BASE,
                                                              sqttBuf0Base.u32All,
                                                              pCmdSpace);

                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(Gfx11::mmSQ_THREAD_TRACE_MASK,
                                                              m_sqtt[idx].mask.u32All,
                                                              pCmdSpace);

                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(Gfx11::mmSQ_THREAD_TRACE_TOKEN_MASK,
                                                              m_sqtt[idx].tokenMask.u32All,
                                                              pCmdSpace);

                // We must write this register last because it turns on thread traces.
                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(Gfx11::mmSQ_THREAD_TRACE_CTRL,
                                                              m_sqtt[idx].ctrl.u32All,
                                                              pCmdSpace);
            }
            else
            {
                regSQ_THREAD_TRACE_BUF0_BASE sqttBuf0Base = {};
                regSQ_THREAD_TRACE_BUF0_SIZE sqttBuf0Size = {};

                sqttBuf0Size.bits.SIZE    = shiftedSize;
                sqttBuf0Size.bits.BASE_HI = HighPart(shiftedAddr);
                sqttBuf0Base.bits.BASE_LO = LowPart(shiftedAddr);

                // All of these registers were moved to privileged space in gfx10 which is pretty silly.
                // We need to write the thread trace buffer size register before the base address register.
                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(Gfx10::mmSQ_THREAD_TRACE_BUF0_SIZE,
                                                              sqttBuf0Size.u32All,
                                                              pCmdSpace);

                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(Gfx10::mmSQ_THREAD_TRACE_BUF0_BASE,
                                                              sqttBuf0Base.u32All,
                                                              pCmdSpace);

                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(Gfx10::mmSQ_THREAD_TRACE_MASK,
                                                              m_sqtt[idx].mask.u32All,
                                                              pCmdSpace);

                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(Gfx10::mmSQ_THREAD_TRACE_TOKEN_MASK,
                                                              m_sqtt[idx].tokenMask.u32All,
                                                              pCmdSpace);

                // We must write this register last because it turns on thread traces.
                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(Gfx10::mmSQ_THREAD_TRACE_CTRL,
                                                              m_sqtt[idx].ctrl.u32All,
                                                              pCmdSpace);
            }
        }
    }

    // Start the thread traces. The spec says it's best to use an event on graphics but we should write the
    // THREAD_TRACE_ENABLE register on compute.
    pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);

    if (m_pDevice->EngineSupportsGraphics(pCmdStream->GetEngineType()))
    {
        pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_START, pCmdStream->GetEngineType(), pCmdSpace);
    }
    else
    {
        regCOMPUTE_THREAD_TRACE_ENABLE computeEnable = {};
        computeEnable.bits.THREAD_TRACE_ENABLE = 1;

        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_THREAD_TRACE_ENABLE,
                                                                computeEnable.u32All,
                                                                pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// A helper function for IssueEnd which writes the necessary commands to stop all thread traces.
//
uint32* PerfExperiment::WriteStopThreadTraces(
    GfxCmdBuffer* pCmdBuffer,
    CmdStream*    pCmdStream,
    uint32*       pCmdSpace
    ) const
{
    const EngineType engineType = pCmdStream->GetEngineType();

    // Stop the thread traces. The spec says it's best to use an event on graphics but we should write the
    // THREAD_TRACE_ENABLE register on compute.
    if (m_pDevice->EngineSupportsGraphics(engineType))
    {
        pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_STOP, engineType, pCmdSpace);
    }
    else
    {
        regCOMPUTE_THREAD_TRACE_ENABLE computeEnable = {};
        computeEnable.bits.THREAD_TRACE_ENABLE = 0;

        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_THREAD_TRACE_ENABLE,
                                                                computeEnable.u32All,
                                                                pCmdSpace);
    }

    // Send a TRACE_FINISH event (even on compute).
    pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_FINISH, engineType, pCmdSpace);

    for (uint32 idx = 0; idx < ArrayLen(m_sqtt); ++idx)
    {
        if (m_sqtt[idx].inUse)
        {
            // Get fresh command space once per trace, just in case.
            pCmdStream->CommitCommands(pCmdSpace);
            pCmdSpace = pCmdStream->ReserveCommands();

            pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmGRBM_GFX_INDEX,
                                                         m_sqtt[idx].grbmGfxIndex.u32All,
                                                         pCmdSpace);

            if (IsGfx11(*m_pDevice))
            {
                // Poll the status register's finish_done bit to be sure that the trace buffer is written out.
                pCmdSpace += m_cmdUtil.BuildWaitRegMem(engineType,
                                                       mem_space__me_wait_reg_mem__register_space,
                                                       function__me_wait_reg_mem__not_equal_reference_value,
                                                       engine_sel__me_wait_reg_mem__micro_engine,
                                                       Gfx11::mmSQ_THREAD_TRACE_STATUS,
                                                       0,
                                                       SQ_THREAD_TRACE_STATUS__FINISH_DONE_MASK,
                                                       pCmdSpace);

                // Set the mode to "OFF".
                regSQ_THREAD_TRACE_CTRL sqttCtrl = m_sqtt[idx].ctrl;
                sqttCtrl.bits.MODE = SQ_TT_MODE_OFF;

                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(Gfx11::mmSQ_THREAD_TRACE_CTRL,
                                                              sqttCtrl.u32All,
                                                              pCmdSpace);

                // Poll the status register's busy bit to wait for it to totally turn off.
                pCmdSpace += m_cmdUtil.BuildWaitRegMem(engineType,
                                                       mem_space__me_wait_reg_mem__register_space,
                                                       function__me_wait_reg_mem__equal_to_the_reference_value,
                                                       engine_sel__me_wait_reg_mem__micro_engine,
                                                       Gfx11::mmSQ_THREAD_TRACE_STATUS,
                                                       0,
                                                       SQ_THREAD_TRACE_STATUS__BUSY_MASK,
                                                       pCmdSpace);

                // Use COPY_DATA to read back the info struct one DWORD at a time.
                const gpusize infoAddr = m_gpuMemory.GpuVirtAddr() + m_sqtt[idx].infoOffset;

                // If each member doesn't start at a DWORD offset this won't wor.
                static_assert(offsetof(ThreadTraceInfoData, curOffset)    == 0,                  "");
                static_assert(offsetof(ThreadTraceInfoData, traceStatus)  == sizeof(uint32),     "");
                static_assert(offsetof(ThreadTraceInfoData, writeCounter) == sizeof(uint32) * 2, "");

                // These chips don't have SQ_THREAD_TRACE_CNTR but SQ_THREAD_TRACE_DROPPED_CNTR seems good enough.
                constexpr uint32 InfoRegisters[] =
                {
                    Gfx11::mmSQ_THREAD_TRACE_WPTR,
                    Gfx11::mmSQ_THREAD_TRACE_STATUS,
                    Gfx11::mmSQ_THREAD_TRACE_DROPPED_CNTR
                };

                for (uint32 regIdx = 0; regIdx < ArrayLen(InfoRegisters); regIdx++)
                {
                    pCmdSpace = pCmdStream->WriteCopyPerfCtrRegToMemory(InfoRegisters[regIdx],
                                                                        infoAddr + regIdx * sizeof(uint32),
                                                                        pCmdSpace);
                }

                // On GFX11, instead of tracking the write address offset from the current buffer base address, the
                // actual write address offset is logged. The SW workaround is to subtract the buffer base address from
                // the actual address logged in SQ_THREAD_TRACE_WPTR.OFFSET field.
                if (m_settings.waSqgTtWptrOffsetFixup)
                {
                    // The SQTT backing memory cannot be GL2-uncached system memory because we cannot rely on the
                    // platform to support PCIE-atomic.
                    PAL_ASSERT(m_gpuMemory.Memory()->Mtype() != MType::Uncached);

                    // The thread trace write pointer offset within buffer is in unit of 32bytes, so shift by 5 bits.
                    constexpr uint32  SqttWptrOffsetAlignShift = 5;

                    // The base address value stored in SQ_THREAD_TRACE_BASE is [43:12], a 4KByte-aligned address.
                    // The WPTR address stored in SQ_THREAD_TRACE_WPTR.OFFSET field is [33:5], a 32Byte-aligned address.
                    // In order to do the subtraction, we need to do the following steps to convert the buffer base
                    // address to WPTR.OFFSET field's alignment and granularity.
                    // 1. Get the raw byte-aligned buffer address.
                    // 2. Shift right 5 bits to 32Byte-aligned address. Note that the bufferAddr[11:0] should be all
                    //    zero because the buffer base address is SqttBufferAlignShift aligned.
                    // 3. Only take the lower 32 bits.
                    // 4. Mask off the higher 3 bits because the WPTR.OFFSET field is 29 bits. Only the lower 29 bits
                    //    of the base address are valid for subtraction. Then shift left 0 bits to place the base
                    //    address value into the offset bitfield.
                    // Note, the calculation needs to be revised if SQTT double buffering is implemented. Specifically
                    // the WPTR.BUFFER_ID field will then need to be carried into initWptrValue value for subtraction.
                    const gpusize bufferAddr    = m_gpuMemory.GpuVirtAddr() + m_sqtt[idx].bufferOffset;
                    const gpusize shiftedAddrLo = LowPart(bufferAddr >> SqttWptrOffsetAlignShift);
                    const uint32  initWptrValue = (shiftedAddrLo << SQ_THREAD_TRACE_WPTR__OFFSET__SHIFT) &
                                                  SQ_THREAD_TRACE_WPTR__OFFSET_MASK;

                    pCmdSpace += m_cmdUtil.BuildAtomicMem(AtomicOp::SubInt32, infoAddr, initWptrValue, pCmdSpace);
                }
            }
            else
            {
                if (m_settings.waBadSqttFinishResults)
                {
                    // On some chips, the finish_done field is broken due to harvesting.
                    // A full pipe flush can be done instead.
                    pCmdSpace = WriteWaitIdle(false, pCmdBuffer, pCmdStream, pCmdSpace);
                }
                else
                {
                    // Poll the status register's finish_done bit to be sure that the trace buffer is written out.
                    pCmdSpace += m_cmdUtil.BuildWaitRegMem(engineType,
                                                        mem_space__me_wait_reg_mem__register_space,
                                                        function__me_wait_reg_mem__not_equal_reference_value,
                                                        engine_sel__me_wait_reg_mem__micro_engine,
                                                        Gfx10::mmSQ_THREAD_TRACE_STATUS,
                                                        0,
                                                        SQ_THREAD_TRACE_STATUS__FINISH_DONE_MASK,
                                                        pCmdSpace);
                }

                // Set the mode to "OFF".
                regSQ_THREAD_TRACE_CTRL sqttCtrl = m_sqtt[idx].ctrl;
                sqttCtrl.bits.MODE = SQ_TT_MODE_OFF;

                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(Gfx10::mmSQ_THREAD_TRACE_CTRL,
                                                              sqttCtrl.u32All,
                                                              pCmdSpace);

                // Poll the status register's busy bit to wait for it to totally turn off.
                pCmdSpace += m_cmdUtil.BuildWaitRegMem(engineType,
                                                       mem_space__me_wait_reg_mem__register_space,
                                                       function__me_wait_reg_mem__equal_to_the_reference_value,
                                                       engine_sel__me_wait_reg_mem__micro_engine,
                                                       Gfx10::mmSQ_THREAD_TRACE_STATUS,
                                                       0,
                                                       SQ_THREAD_TRACE_STATUS__BUSY_MASK,
                                                       pCmdSpace);

                // Use COPY_DATA to read back the info struct one DWORD at a time.
                const gpusize infoAddr = m_gpuMemory.GpuVirtAddr() + m_sqtt[idx].infoOffset;

                // If each member doesn't start at a DWORD offset this won't work.
                static_assert(offsetof(ThreadTraceInfoData, curOffset)    == 0,                  "");
                static_assert(offsetof(ThreadTraceInfoData, traceStatus)  == sizeof(uint32),     "");
                static_assert(offsetof(ThreadTraceInfoData, writeCounter) == sizeof(uint32) * 2, "");

                // Gfx10 doesn't have SQ_THREAD_TRACE_CNTR but SQ_THREAD_TRACE_DROPPED_CNTR seems good enough.
                constexpr uint32 InfoRegisters[] =
                {
                    Gfx10::mmSQ_THREAD_TRACE_WPTR,
                    Gfx10::mmSQ_THREAD_TRACE_STATUS,
                    Gfx10::mmSQ_THREAD_TRACE_DROPPED_CNTR
                };

                for (uint32 regIdx = 0; regIdx < ArrayLen(InfoRegisters); regIdx++)
                {
                    pCmdSpace = pCmdStream->WriteCopyPerfCtrRegToMemory(InfoRegisters[regIdx],
                                                                        infoAddr + regIdx * sizeof(uint32),
                                                                        pCmdSpace);
                }
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
            // The SQ counters must be programmed while broadcasting to all SQs on the target SE. This should be
            // fine because each "SQ" instance here is really a SQG instance and there's only one in each SE.
            pCmdSpace = WriteGrbmGfxIndexBroadcastSe(instance,
                                                     pCmdStream,
                                                     pCmdSpace);

            const uint32 sqgNumModules = IsGfx11(*m_pDevice) ? Gfx11MaxSqgPerfmonModules : Gfx9MaxSqgPerfmonModules;
            const PerfCounterRegAddr& regAddr = m_counterInfo.block[uint32(GpuBlock::Sq)].regAddr;

            for (uint32 idx = 0; idx < sqgNumModules; ++idx)
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

    uint32 sqWgpSelects[Gfx9MaxShaderEngines][Gfx11MaxSqPerfmonModules] = {};

    for (uint32 instance = 0; instance < ArrayLen(m_select.sqWgp); ++instance)
    {
        const GlobalSelectState::SqWgp& instSelect = m_select.sqWgp[instance];

        if (instSelect.hasCounters)
        {
            // For pre-gfx11 asic, cient should not try to profile sqWgp block.
            PAL_ASSERT(IsGfx11(*m_pDevice));
            const PerfCounterRegAddr& regAddr = m_counterInfo.block[uint32(GpuBlock::SqWgp)].regAddr;

            regGRBM_GFX_INDEX reg =
            {
                .u32All = instSelect.grbmGfxIndex.u32All
            };
            reg.bits.SA_BROADCAST_WRITES       = 1;
            reg.bits.INSTANCE_BROADCAST_WRITES = 1;
            pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmGRBM_GFX_INDEX, reg.u32All, pCmdSpace);

            uint32 realSe = reg.bits.SE_INDEX;

            for (uint32 idx = 0; idx < ArrayLen(instSelect.perfmon); ++idx)
            {
                if (instSelect.perfmonInUse[idx])
                {
                    PAL_ASSERT(regAddr.perfcounter[idx].selectOrCfg != 0);

                    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regAddr.perfcounter[idx].selectOrCfg,
                                                                  instSelect.perfmon[idx].u32All,
                                                                  pCmdSpace);

                    // Check for cross SE alignment on the same counter instance to warn against different values per SE
                    if ((sqWgpSelects[realSe][idx] != 0) &&
                        (instSelect.perfmon[idx].u32All != sqWgpSelects[realSe][idx]))
                    {
                        DbgLog(SeverityLevel::Error, OriginationType::GpuProfiler, "GPUProfiler",
                            "Cross SE variance detected for SQWGP performance counter %d. Only last select is effective"
                            ". Existing: %x New: %x (%s:%d:%s)", idx, sqWgpSelects[realSe][idx],
                            instSelect.perfmon[idx].u32All, __FILE__, __LINE__, __func__);
                    }
                    sqWgpSelects[realSe][idx] = instSelect.perfmon[idx].u32All;

                    // Some SQ-per-WGP perfmons actually have zero counters! What a unique programming model.
                    if (regAddr.perfcounter[idx].lo != 0)
                    {
                        // Zero out this counter value before we start using it. Experiments show this fixes some
                        // issues with the SQ latency counters getting stuck at "0xFFFFFFFF", likely due to saturation.
                        // Note that SQ's legacy counters are 32-bit.
                        pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regAddr.perfcounter[idx].lo, 0, pCmdSpace);
                    }
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
            const PerfCounterRegAddr& regAddr = m_counterInfo.block[uint32(GpuBlock::GrbmSe)].regAddr;

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

    // Program the global UMCCH per-counter control registers.
    for (uint32 instance = 0; instance < ArrayLen(m_select.umcch); ++instance)
    {
        if (m_select.umcch[instance].hasCounters)
        {
            pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);

            for (uint32 idx = 0; idx < ArrayLen(m_select.umcch[instance].perfmonInUse); ++idx)
            {
                if (m_select.umcch[instance].perfmonInUse[idx])
                {
                    PAL_ASSERT(m_counterInfo.umcchRegAddr[instance].perModule[idx].perfMonCtl != 0);

                    {
                        pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(
                            m_counterInfo.umcchRegAddr[instance].perModule[idx].perfMonCtl,
                            m_select.umcch[instance].perfmonCntl[idx].u32All,
                            pCmdSpace);
                    }
                    if (IsGfx103Plus(*m_pDevice) && m_select.umcch[instance].thresholdSet[idx])
                    {
                        {
                            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(
                                                         m_counterInfo.umcchRegAddr[instance].perModule[idx].perfMonCtrHi,
                                                         m_select.umcch[instance].perfmonCtrHi[idx].u32All,
                                                         pCmdSpace);
                        }
                    }
                }
            }

            // Get fresh command space just in case we're close to running out.
            pCmdStream->CommitCommands(pCmdSpace);
            pCmdSpace = pCmdStream->ReserveCommands();
        }
    }

    // Program the global DF per-counter control registers.
    {
        const DfSelectState& select = m_select.df;

        if (select.hasCounters)
        {
            // Reset broadcast should not be needed since DF not part of graphics, but let's be safe with a known state
            pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);

            for (uint32 idx = 0; idx < ArrayLen(select.perfmonConfig); ++idx)
            {
                if (select.perfmonConfig[idx].perfmonInUse)
                {
                    const uint32 eventSelect   = select.perfmonConfig[idx].eventSelect;
                    const uint16 eventUnitMask = select.perfmonConfig[idx].eventUnitMask;

                    {
                        // Gfx10.3 requires this packet.
                        PAL_ASSERT(m_pDevice->ChipProperties().cpUcodeVersion > 29);

                        pCmdSpace += m_cmdUtil.BuildPerfmonControl(idx, true, eventSelect, eventUnitMask, pCmdSpace);
                    }
                }
            }

            // Get fresh command space just in case we're close to running out.
            pCmdStream->CommitCommands(pCmdSpace);
            pCmdSpace = pCmdStream->ReserveCommands();
        }
    }

    // Finally, write the generic blocks' select registers.
    for (uint32 block = 0; block < GpuBlockCount; ++block)
    {
        if (m_select.pGeneric[block] != nullptr)
        {
            for (uint32 instance = 0; instance < m_select.numGeneric[block]; ++instance)
            {
                const GenericBlockSelect& select = m_select.pGeneric[block][instance];

                if (select.hasCounters)
                {
                    // Write GRBM_GFX_INDEX to target this specific block instance and enable its active modules.
                    pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmGRBM_GFX_INDEX,
                                                                 select.grbmGfxIndex.u32All,
                                                                 pCmdSpace);

                    for (uint32 idx = 0; idx < select.numModules; ++idx)
                    {
                        if (select.pModules[idx].inUse != 0)
                        {
                            const PerfCounterRegAddrPerModule* pRegAddr = nullptr;

                            if (block == uint32(GpuBlock::Dma))
                            {
                                // SDMA has unique registers for each instance.
                                pRegAddr = &m_counterInfo.sdmaRegAddr[instance][idx];
                            }
                            else if (HasRmiSubInstances(static_cast<GpuBlock>(block)))
                            {
                                // RegAddr for even sub-instances of RMI start from index 0 and from index 2 for odd
                                // sub-instances.
                                if ((instance % Gfx10NumRmiSubInstances) == 0)
                                {
                                    pRegAddr = &m_counterInfo.block[block].regAddr.perfcounter[idx];
                                }
                                else
                                {
                                    pRegAddr = &m_counterInfo.block[block].regAddr.perfcounter[idx + 2];
                                }
                            }
                            else
                            {
                                pRegAddr = &m_counterInfo.block[block].regAddr.perfcounter[idx];
                            }

                            if (select.pModules[idx].type == SelectType::Perfmon)
                            {
                                // The perfmon registers come in SELECT/SELECT1 pairs.
                                PAL_ASSERT((pRegAddr->selectOrCfg != 0) &&
                                           (pRegAddr->select1 != 0));

                                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(pRegAddr->selectOrCfg,
                                                                              select.pModules[idx].perfmon.sel0.u32All,
                                                                              pCmdSpace);

                                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(pRegAddr->select1,
                                                                              select.pModules[idx].perfmon.sel1.u32All,
                                                                              pCmdSpace);
                            }
                            else
                            {
                                // Both legacy module types use one register so we can use the same code here.
                                PAL_ASSERT(pRegAddr->selectOrCfg != 0);

                                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(pRegAddr->selectOrCfg,
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
// A helper function for IssueBegin which writes the necessary commands to enable all cfg-style blocks.
uint32* PerfExperiment::WriteEnableCfgRegisters(
    bool       enable,
    bool       clear,
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    for (uint32 block = 0; block < GpuBlockCount; ++block)
    {
        if (m_counterInfo.block[block].isCfgStyle)
        {
            // Check for an active instance before we broadcast this register. We only write it once.
            for (uint32 instance = 0; instance < m_select.numGeneric[block]; ++instance)
            {
                if (m_select.pGeneric[block][instance].hasCounters)
                {
                    ResultCntl resultCntl = {};
                    resultCntl.bits.ENABLE_ANY = enable;
                    resultCntl.bits.CLEAR_ALL  = clear;

                    PAL_ASSERT(m_counterInfo.block[block].regAddr.perfcounterRsltCntl != 0);

                    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(
                        m_counterInfo.block[block].regAddr.perfcounterRsltCntl, resultCntl.u32All, pCmdSpace);

                    break;
                }
            }
        }
    }

    // Get fresh command space just in case we're close to running out.
    pCmdStream->CommitCommands(pCmdSpace);
    pCmdSpace = pCmdStream->ReserveCommands();

    uint32* pStartSpace = pCmdSpace;

    // The UMCCH has a per-instance register that acts just like a rslt_cntl register. Let's enable it here.
    for (uint32 instance = 0; instance < ArrayLen(m_select.umcch); ++instance)
    {

        if (m_select.umcch[instance].hasCounters)
        {
            PAL_ASSERT(m_counterInfo.umcchRegAddr[instance].perfMonCtlClk != 0);

            if (clear)
            {
                regPerfMonCtlClk perfmonCtlClk = {};
                perfmonCtlClk.most.GlblReset   = 1;

                if (IsNavi3x(*m_pDevice))
                {
                    perfmonCtlClk.u32All |= Nv3x::PerfMonCtlClk__GlblResetMsk_MASK;
                }
                else
                {
                    constexpr uint32 GblbRsrcMskMask = Gfx101::PerfMonCtlClk__GlblResetMsk_MASK;
                    static_assert(GblbRsrcMskMask == Nv2x::PerfMonCtlClk__GlblResetMsk_MASK,
                                  "GblbRsrcMskMask does not match all chips!");

                    perfmonCtlClk.u32All |= GblbRsrcMskMask;
                }

                {
                    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(m_counterInfo.umcchRegAddr[instance].perfMonCtlClk,
                                                                  perfmonCtlClk.u32All,
                                                                  pCmdSpace);
                }
            }

            regPerfMonCtlClk perfmonCtlClk = {};
            perfmonCtlClk.most.GlblMonEn = enable;

            {
                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(m_counterInfo.umcchRegAddr[instance].perfMonCtlClk,
                                                              perfmonCtlClk.u32All,
                                                              pCmdSpace);
            }

            // Assume each counter uses the same amount of space and determine if next loop we'll run out
            pCmdSpace = pCmdStream->ReReserveCommands(pCmdSpace, pCmdSpace - pStartSpace);
            pStartSpace = pCmdSpace;
        }
    }

    // Get fresh command space just in case we're close to running out.
    pCmdStream->CommitCommands(pCmdSpace);
    pCmdSpace = pCmdStream->ReserveCommands();

    // The RLC has a special global control register. It works just like CP_PERFMON_CNTL.
    if (HasGenericCounters(GpuBlock::Rlc))
    {
        if (clear)
        {
            regRLC_PERFMON_CNTL rlcPerfmonCntl = {};
            rlcPerfmonCntl.bits.PERFMON_STATE = CP_PERFMON_STATE_DISABLE_AND_RESET;

            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmRLC_PERFMON_CNTL, rlcPerfmonCntl.u32All, pCmdSpace);
        }

        regRLC_PERFMON_CNTL rlcPerfmonCntl = {};
        rlcPerfmonCntl.bits.PERFMON_STATE = enable ? CP_PERFMON_STATE_START_COUNTING : CP_PERFMON_STATE_STOP_COUNTING;

        pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmRLC_PERFMON_CNTL, rlcPerfmonCntl.u32All, pCmdSpace);
    }

    // Get fresh command space just in case we're close to running out.
    pCmdStream->CommitCommands(pCmdSpace);
    pCmdSpace = pCmdStream->ReserveCommands();

    // The RMI has a special control register. We normally program the RMI using a per-instance GRBM_GFX_INDEX
    // but this control is constant for all instances so we only need to write it once using global broadcasting.
    if (HasGenericCounters(GpuBlock::Rmi))
    {
        regRMI_PERF_COUNTER_CNTL rmiPerfCounterCntl = {};
        rmiPerfCounterCntl.bits.PERF_SOFT_RESET = clear;

        if (enable)
        {
            // These hard-coded default values come from the old perf experiment code.
            rmiPerfCounterCntl.bits.TRANS_BASED_PERF_EN_SEL             = 1;
            rmiPerfCounterCntl.bits.EVENT_BASED_PERF_EN_SEL             = 1;
            rmiPerfCounterCntl.bits.TC_PERF_EN_SEL                      = 1;
            rmiPerfCounterCntl.bits.PERF_EVENT_WINDOW_MASK0             = 0x1;
            rmiPerfCounterCntl.bits.PERF_EVENT_WINDOW_MASK1             = 0x2;
            rmiPerfCounterCntl.bits.PERF_COUNTER_CID                    = 0x8;
            rmiPerfCounterCntl.bits.PERF_COUNTER_BURST_LENGTH_THRESHOLD = 1;
        }

        pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmRMI_PERF_COUNTER_CNTL, rmiPerfCounterCntl.u32All, pCmdSpace);
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
    // The recommended sampling procedure: stop windowed, sample, wait-idle, stop global, read values.
    // Global blocks don't listen to perfcounter events so we set PERFMON_SAMPLE_ENABLE while also issuing the event.
    // We could probably take a long time to study how each specific block responds to events or the sample bit to
    // come up with the optimal programming.
    pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(PERFCOUNTER_SAMPLE, pCmdStream->GetEngineType(), pCmdSpace);
    pCmdSpace = WriteWaitIdle(false, pCmdBuffer, pCmdStream, pCmdSpace);
    pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);

    pCmdSpace = WriteUpdateWindowedCounters(false, pCmdStream, pCmdSpace);

    // Trigger a counter sample using CP_PERFMON_CNTL. Ideally we'd also set global counters and SPM counters to
    // STOP_COUNTING but if it's not safe to do so we need to pick something that works well enough.
    regCP_PERFMON_CNTL cpPerfmonCntl = {};
    cpPerfmonCntl.bits.PERFMON_SAMPLE_ENABLE = 1;

    if (m_neverStopCounters == false)
    {
        cpPerfmonCntl.bits.PERFMON_STATE = CP_PERFMON_STATE_STOP_COUNTING;
    }
    else if (isBeginSample)
    {
        // We gather the begin samples before we start the counters. We'd prefer to set them to STOP_COUNTING because
        // we're not really meant to sample them in reset mode. But, if we can't use STOP_COUNTING it seems that
        // sampling them while reset is OK. If it's proven that this isn't actually OK then we need to start all
        // counters before we take the beginning sample, which will run without being paused. This will seriously
        // distort any clock counting perf counters so it's a last resort.
        cpPerfmonCntl.bits.PERFMON_STATE = CP_PERFMON_STATE_DISABLE_AND_RESET;
    }
    else
    {
        // It's not possible to stop global counters in this mode, we have to keep them running.
        cpPerfmonCntl.bits.PERFMON_STATE = CP_PERFMON_STATE_START_COUNTING;
    }

    if (m_perfExperimentFlags.spmTraceEnabled == 0)
    {
        cpPerfmonCntl.bits.SPM_PERFMON_STATE = STRM_PERFMON_STATE_DISABLE_AND_RESET;
    }
    else if (m_neverStopCounters)
    {
        // Note that we also can't stop SPM if the workaround is enabled, we'll have to handle that later.
        cpPerfmonCntl.bits.SPM_PERFMON_STATE = STRM_PERFMON_STATE_START_COUNTING;
    }
    else
    {
        // This prevents SPM counters from sampling the global counter sampling packets.
        cpPerfmonCntl.bits.SPM_PERFMON_STATE = STRM_PERFMON_STATE_STOP_COUNTING;
    }

    pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmCP_PERFMON_CNTL, cpPerfmonCntl.u32All, pCmdSpace);

    // Stop the cfg-style counters too. It's not clear if these are included in the above guidelines so just stop them
    // at the end to be safe. If we're getting the begin samples we should also initialize these counters by clearing
    // them.
    pCmdSpace = WriteEnableCfgRegisters(false, isBeginSample, pCmdStream, pCmdSpace);

    // The old perf experiment code also sets the RLC's PERFMON_SAMPLE_ENABLE bit each time it samples. I can't find
    // any documentation that has anything to say at all about this field so let's just do the same thing.
    if (HasGenericCounters(GpuBlock::Rlc))
    {
        regRLC_PERFMON_CNTL rlcPerfmonCntl = {};
        rlcPerfmonCntl.bits.PERFMON_STATE         = CP_PERFMON_STATE_STOP_COUNTING;
        rlcPerfmonCntl.bits.PERFMON_SAMPLE_ENABLE = 1;

        pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmRLC_PERFMON_CNTL, rlcPerfmonCntl.u32All, pCmdSpace);
    }

    // Copy each counter's value from registers to memory, one at a time.
    const gpusize destBaseAddr = m_gpuMemory.GpuVirtAddr() + (isBeginSample ? m_globalBeginOffset : m_globalEndOffset);

    for (uint32 idx = 0; idx < m_globalCounters.NumElements(); ++idx)
    {
        const GlobalCounterMapping& mapping  = m_globalCounters.At(idx);
        const uint32                instance = mapping.general.globalInstance;
        const uint32                block    = uint32(mapping.general.block);

        if (mapping.general.block == GpuBlock::GrbmSe)
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
        else if (mapping.general.block == GpuBlock::Sq)
        {
            pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmGRBM_GFX_INDEX,
                                                         m_select.sqg[instance].grbmGfxIndex.u32All,
                                                         pCmdSpace);

            pCmdSpace = WriteCopy64BitCounter(m_counterInfo.block[block].regAddr.perfcounter[mapping.counterId].lo,
                                              m_counterInfo.block[block].regAddr.perfcounter[mapping.counterId].hi,
                                              destBaseAddr + mapping.offset,
                                              pCmdStream,
                                              pCmdSpace);

            // Get fresh command space just in case we're close to running out.
            pCmdStream->CommitCommands(pCmdSpace);
            pCmdSpace = pCmdStream->ReserveCommands();
        }
        else if (mapping.general.block == GpuBlock::SqWgp)
        {
            // Since gfx11, we can specify gloabl perf counters for sq,sqc,sp blocks.
            // Prior to gfx11, those counters are considered as part of SQG block.
            PAL_ASSERT(IsGfx11(*m_pDevice));
            pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmGRBM_GFX_INDEX,
                                                         m_select.sqWgp[instance].grbmGfxIndex.u32All,
                                                         pCmdSpace);

            PAL_ASSERT(m_counterInfo.block[block].regAddr.perfcounter[mapping.counterId].lo != 0);

            pCmdSpace = pCmdStream->WriteCopyPerfCtrRegToMemory(
                                        m_counterInfo.block[block].regAddr.perfcounter[mapping.counterId].lo,
                                        destBaseAddr + mapping.offset,
                                        pCmdSpace);

            // Get fresh command space just in case we're close to running out.
            pCmdStream->CommitCommands(pCmdSpace);
            pCmdSpace = pCmdStream->ReserveCommands();
        }
        else if (mapping.general.block == GpuBlock::Umcch)
        {
            // The UMCCH is global and has registers that vary per-instance and per-counter.
            pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);

            {
                pCmdSpace = WriteCopy64BitCounter(
                                m_counterInfo.umcchRegAddr[instance].perModule[mapping.counterId].perfMonCtrLo,
                                m_counterInfo.umcchRegAddr[instance].perModule[mapping.counterId].perfMonCtrHi,
                                destBaseAddr + mapping.offset,
                                pCmdStream,
                                pCmdSpace);
            }
        }
        else if (mapping.general.block == GpuBlock::DfMall)
        {
            // The DF is global and has registers that vary per-counter.
            pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);

            const PerfCounterRegAddr& regAddr = m_counterInfo.block[uint32(GpuBlock::DfMall)].regAddr;
            const PerfCounterRegAddrPerModule& regs = regAddr.perfcounter[mapping.counterId];

            {
                pCmdSpace = WriteCopy64BitCounter(regs.lo,
                                                  regs.hi,
                                                  destBaseAddr + mapping.offset,
                                                  pCmdStream,
                                                  pCmdSpace);
            }
        }
        else if (m_select.pGeneric[block] != nullptr)
        {
            // Set GRBM_GFX_INDEX so that we're talking to the specific block instance which own the given counter.
            pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmGRBM_GFX_INDEX,
                                                         m_select.pGeneric[block][instance].grbmGfxIndex.u32All,
                                                         pCmdSpace);

            if (m_counterInfo.block[block].isCfgStyle)
            {
                // Tell the block which perf counter value to move into the shared lo/hi registers.
                ResultCntl resultCntl = {};
                resultCntl.bits.PERF_COUNTER_SELECT = mapping.counterId;

                PAL_ASSERT(m_counterInfo.block[block].regAddr.perfcounterRsltCntl != 0);

                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(m_counterInfo.block[block].regAddr.perfcounterRsltCntl,
                                                              resultCntl.u32All,
                                                              pCmdSpace);
            }

            const PerfCounterRegAddrPerModule* pRegAddr = nullptr;

            if (block == uint32(GpuBlock::Dma))
            {
                // SDMA has unique registers for each instance.
                pRegAddr = &m_counterInfo.sdmaRegAddr[instance][mapping.counterId];
            }
            else if (HasRmiSubInstances(static_cast<GpuBlock>(block)))
            {
                // Register address offsets for even sub-instances of RMI start from index 0 and from index 2 for
                // odd sub-instances.
                if ((instance % Gfx10NumRmiSubInstances) == 0)
                {
                    pRegAddr = &m_counterInfo.block[block].regAddr.perfcounter[mapping.counterId];
                }
                else
                {
                    pRegAddr = &m_counterInfo.block[block].regAddr.perfcounter[mapping.counterId + 2];
                }
            }
            else
            {
                pRegAddr = &m_counterInfo.block[block].regAddr.perfcounter[mapping.counterId];
            }

            // Copy the counter value out to memory.
            pCmdSpace = WriteCopy64BitCounter(pRegAddr->lo,
                                              pRegAddr->hi,
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

    // The DF doesn't listen to CP_PERFMON_CNTL and doesn't have a global cfg on/off switch which means these
    // counters will keep going indefinitely if we leave them on. We don't do this for any other blocks, but it's
    // probably best to break from tradition and disable(zero out) when sampling is stopped.
    if (isBeginSample == false)
    {
        {
            const DfSelectState& select = m_select.df;

            if (select.hasCounters)
            {
                // Reset broadcast should not be needed since DF not part of graphics but let's be safe
                pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);

                for (uint32 idx = 0; idx < ArrayLen(select.perfmonConfig); ++idx)
                {
                    if (select.perfmonConfig[idx].perfmonInUse)
                    {
                        {
                            // Gfx10.3 requires this packet.
                            PAL_ASSERT(m_pDevice->ChipProperties().cpUcodeVersion > 29);

                            pCmdSpace += m_cmdUtil.BuildPerfmonControl(idx, false, 0, 0, pCmdSpace);
                        }
                    }
                }

                // Get fresh command space just in case we're close to running out.
                pCmdStream->CommitCommands(pCmdSpace);
                pCmdSpace = pCmdStream->ReserveCommands();
            }
        }
    }

    // Restore global broadcasting.
    pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);

    return pCmdSpace;
}

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

    pCmdSpace = pCmdStream->WriteCopyPerfCtrRegToMemory(regAddrLo, destAddr,                  pCmdSpace);
    pCmdSpace = pCmdStream->WriteCopyPerfCtrRegToMemory(regAddrHi, destAddr + sizeof(uint32), pCmdSpace);

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
    grbmGfxIndex.bits.SA_BROADCAST_WRITES       = 1;
    grbmGfxIndex.bits.INSTANCE_BROADCAST_WRITES = 1;

    return pCmdStream->WriteSetOneConfigReg(mmGRBM_GFX_INDEX, grbmGfxIndex.u32All, pCmdSpace);
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
    grbmGfxIndex.bits.SE_INDEX                  = VirtualSeToRealSe(seIndex);
    grbmGfxIndex.bits.SA_BROADCAST_WRITES       = 1;
    grbmGfxIndex.bits.INSTANCE_BROADCAST_WRITES = 1;

    return pCmdStream->WriteSetOneConfigReg(mmGRBM_GFX_INDEX, grbmGfxIndex.u32All, pCmdSpace);
}

// Some registers were moved from user space to privileged space, we must access them using _UMD or _REMAP registers.
// The problem is that only some ASICs moved the registers so we can't use any one name consistently. The good news is
// that most of the _UMD and _REMAP registers have the same user space address as the old user space registers.
// If these asserts pass we can just use the Gfx101 version of these registers everywhere in our code.
static_assert(Gfx101::mmSPI_CONFIG_CNTL_REMAP == Nv2x::mmSPI_CONFIG_CNTL_REMAP);
static_assert(Gfx101::mmSPI_CONFIG_CNTL_REMAP == Apu103::mmSPI_CONFIG_CNTL);
static_assert(Gfx101::mmSPI_CONFIG_CNTL_REMAP == Gfx11::mmSPI_CONFIG_CNTL);

// =====================================================================================================================
// Writes a packet that updates the SQG event controls in SPI_CONFIG_CNTL.
uint32* PerfExperiment::WriteUpdateSpiConfigCntl(
    bool       enableSqgEvents,
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    // We really only want to update the SQG fields. Set the others to their defaults and hope no one changed them.
    // (For now we don't change this register anywhere else in the driver.)
    regSPI_CONFIG_CNTL spiConfigCntl = {};

    // Use KMD's default value if we have it, otherwise fall back to the hard-coded default.
    if (m_chipProps.gfx9.overrideDefaultSpiConfigCntl != 0)
    {
        spiConfigCntl.u32All = m_chipProps.gfx9.spiConfigCntl;
    }
    else
    {
        spiConfigCntl.u32All = mmSPI_CONFIG_CNTL_DEFAULT;
    }

    spiConfigCntl.bits.ENABLE_SQG_TOP_EVENTS = enableSqgEvents;
    spiConfigCntl.bits.ENABLE_SQG_BOP_EVENTS = enableSqgEvents;

    // MEC doesn't support RMW, so directly set the register.
    if (m_pDevice->EngineSupportsGraphics(pCmdStream->GetEngineType()))
    {
        constexpr uint32 SpiConfigCntlSqgEventsMask = ((1 << SPI_CONFIG_CNTL__ENABLE_SQG_BOP_EVENTS__SHIFT) |
                                                       (1 << SPI_CONFIG_CNTL__ENABLE_SQG_TOP_EVENTS__SHIFT));

        pCmdSpace += m_cmdUtil.BuildRegRmw(Gfx101::mmSPI_CONFIG_CNTL_REMAP,
                                           spiConfigCntl.u32All,
                                           ~(SpiConfigCntlSqgEventsMask),
                                           pCmdSpace);
    }
    else
    {
        pCmdSpace = pCmdStream->WriteSetOneConfigReg(Gfx101::mmSPI_CONFIG_CNTL_REMAP,
                                                     spiConfigCntl.u32All,
                                                     pCmdSpace);
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
    // As with thread traces, we must use an event on universal queues but set a register on compute queues.
    if (m_pDevice->EngineSupportsGraphics(pCmdStream->GetEngineType()))
    {
        if (enable)
        {
            pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(PERFCOUNTER_START,
                                                            pCmdStream->GetEngineType(),
                                                            pCmdSpace);
        }
        else if (m_settings.waCbPerfCounterStuckZero == false)
        {
            pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(PERFCOUNTER_STOP,
                                                            pCmdStream->GetEngineType(),
                                                            pCmdSpace);
        }
    }

    regCOMPUTE_PERFCOUNT_ENABLE computeEnable = {};
    computeEnable.bits.PERFCOUNT_ENABLE = enable;

    pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_PERFCOUNT_ENABLE,
                                                            computeEnable.u32All,
                                                            pCmdSpace);

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
    const EngineType engineType = pCmdStream->GetEngineType();
    Pm4CmdBuffer*    pPm4CmdBuf = static_cast<Pm4CmdBuffer*>(pCmdBuffer);

    if (flushCaches)
    {
        // We need to use a pipelined event to flush and invalidate all of the RB caches. This may require the
        // CP to spin-loop on a timestamp in memory so it may be much slower than the non-flushing path.
        const SyncRbFlags rbSync = pPm4CmdBuf->IsGraphicsSupported() ? SyncRbWbInv : SyncRbNone;
        pCmdSpace = pPm4CmdBuf->WriteWaitEop(HwPipeTop, false, SyncGlxWbInvAll, rbSync, pCmdSpace);
    }
    else
    {
        // Use a CS_PARTIAL_FLUSH to wait for CS work to complete.
        //
        // Note that this isn't a true wait-idle for the compute/gfx engine. In order to wait for the very bottom of
        // the pipeline we would have to wait for a EOP TS event. Doing that inflates the perf experiment overhead
        // by a not-insignificant margin (~150ns or ~4K clocks on Vega10). Thus we go with this much faster waiting
        // method which covers almost all of the same cases as the wait for EOP TS. If we run into issues with
        // counters at the end of the graphics pipeline or counters that monitor the event pipeline we might need
        // to change this.
        pCmdSpace = pPm4CmdBuf->WriteWaitCsIdle(pCmdSpace);

        if (m_pDevice->EngineSupportsGraphics(engineType))
        {
            AcquireMemGfxSurfSync acquireInfo = {};
            acquireInfo.flags.pfpWait       = 1;
            acquireInfo.flags.cbTargetStall = 1;
            acquireInfo.flags.dbTargetStall = 1;

            pCmdSpace += m_cmdUtil.BuildAcquireMemGfxSurfSync(acquireInfo, pCmdSpace);
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

    for (uint32 idx = 0; idx < m_select.numGeneric[uint32(block)]; ++idx)
    {
        if (m_select.pGeneric[uint32(block)][idx].hasCounters)
        {
            hasCounters = true;
            break;
        }
    }

    return hasCounters;
}

// =====================================================================================================================
bool PerfExperiment::HasGlobalDfCounters() const
{
    bool hasCounters = false;

    hasCounters = m_select.df.hasCounters;

    return hasCounters;
}

// =====================================================================================================================
// Returns true if this block is an RMI block and PAL considers this instance as consisting of virtual sub-instances.
bool PerfExperiment::HasRmiSubInstances(
    GpuBlock block
    ) const
{
    return (block == GpuBlock::Rmi);
}

// =====================================================================================================================
// Assuming this is an SqWgp counter select, return true if it's a "LEVEL" counter, which require special SPM handling.
bool PerfExperiment::IsSqWgpLevelEvent(
    uint32 eventId
    ) const
{
    bool isLevelEvent = false;

    PAL_ASSERT(IsGfx11(m_chipProps.gfxLevel));

    if (eventId == SQ_PERF_SEL_LEVEL_WAVES)
    {
        isLevelEvent = true;
    }
    else if ((eventId >= SQ_PERF_SEL_INST_LEVEL_EXP__GFX11) &&
             (eventId <= SQ_PERF_SEL_INST_LEVEL_TEX_STORE__GFX11))
    {
        isLevelEvent = true;
    }
    else if (eventId == SQ_PERF_SEL_IFETCH_LEVEL__GFX110)
    {
        isLevelEvent = true;
    }
    else if ((eventId >= SQ_PERF_SEL_USER_LEVEL0__GFX11) &&
             (eventId <= SQ_PERF_SEL_USER_LEVEL15__GFX11))
    {
        isLevelEvent = true;
    }
    else if ((eventId >= SQC_PERF_SEL_ICACHE_INFLIGHT_LEVEL__GFX11) &&
             (eventId <= SQC_PERF_SEL_DCACHE_TC_INFLIGHT_LEVEL__GFX11))
    {
        isLevelEvent = true;
    }

    return isLevelEvent;
}

// =====================================================================================================================
// Assuming this is an SQG counter select, return true if it's a "LEVEL" counter, which require special SPM handling.
bool PerfExperiment::IsSqLevelEvent(
    uint32 eventId
    ) const
{
    bool isLevelEvent = false;

    if (IsGfx11(m_chipProps.gfxLevel))
    {
        if (eventId == SQG_PERF_SEL_LEVEL_WAVES)
        {
            isLevelEvent = true;
        }
    }
    else
    {
        if (eventId == SQ_PERF_SEL_LEVEL_WAVES)
        {
            isLevelEvent = true;
        }
        else if ((eventId >= SQC_PERF_SEL_ICACHE_INFLIGHT_LEVEL__GFX10) &&
                 (eventId <= SQC_PERF_SEL_DCACHE_TC_INFLIGHT_LEVEL__GFX10))
        {
            isLevelEvent = true;
        }
        else if (eventId == SQC_PERF_SEL_ICACHE_UTCL0_INFLIGHT_LEVEL__GFX101)
        {
            isLevelEvent = true;
        }
        else if (eventId == SQC_PERF_SEL_ICACHE_UTCL1_INFLIGHT_LEVEL__GFX101)
        {
            isLevelEvent = true;
        }
        else if (eventId == SQC_PERF_SEL_DCACHE_UTCL0_INFLIGHT_LEVEL__GFX101)
        {
            isLevelEvent = true;
        }
        else if (eventId == SQC_PERF_SEL_DCACHE_UTCL1_INFLIGHT_LEVEL__GFX101)
        {
            isLevelEvent = true;
        }
        else
        {
            if ((eventId >= SQ_PERF_SEL_INST_LEVEL_EXP__GFX10) &&
                (eventId <= SQ_PERF_SEL_INST_LEVEL_TEX_STORE__GFX10))
            {
                isLevelEvent = true;
            }
            else if (eventId == SQ_PERF_SEL_IFETCH_LEVEL__GFX10)
            {
                isLevelEvent = true;
            }
            else if ((eventId >= SQ_PERF_SEL_USER_LEVEL0__GFX10) &&
                     (eventId <= SQ_PERF_SEL_USER_LEVEL15__GFX10))
            {
                isLevelEvent = true;
            }
        }
    }

    return isLevelEvent;
}

// =====================================================================================================================
// Needed for SE harvesting. Translate the Virtual Shader Engine that apps use to the real Hardware Shader Engine
uint32 PerfExperiment::VirtualSeToRealSe(
    uint32 index
    ) const
{
    uint32 seCount = 0;
    uint32 seIndex = 0;
    for (; seIndex < m_chipProps.gfx9.numShaderEngines; seIndex++)
    {
        if ((m_chipProps.gfx9.activeSeMask & (1 << seIndex)) &&
            (index == seCount++))
        {
            break;
        }
    }
    return seIndex;
}

// =====================================================================================================================
// Needed for SE harvesting. Translate the Real Shader Engine Index to the virtual shader index
uint32 PerfExperiment::RealSeToVirtualSe(
    uint32 index
    ) const
{
    // If they are asking for a ShaderEngine index that is larger than what we have we can't find it
    PAL_ASSERT(index < m_chipProps.gfx9.numShaderEngines);
    uint32 seIndex = 0;
    for (uint32 i = 0; i < index; i++)
    {
        if (m_chipProps.gfx9.activeSeMask & (1 << i))
        {
            seIndex++;
        }
    }
    return seIndex;
}

// =====================================================================================================================
// Gets the event select value for this perfmon based on the perf counter info
uint32 PerfExperiment::GetMallEventSelect(
    uint32 eventId,
    uint32 subBlockInstance
    ) const
{
    // The DF counters are programmed differently than other blocks using a 14-bit "EventSelect":
    //   EventSelect[13:6] specifies the DF subblock instance.
    //   EventSelect[5:0]  specifies the subblock event ID
    PAL_ASSERT(eventId <= ((1 << 6) - 1));

    // Figure out which DF subblock is our first MALL instance, the rest of them follow immediately after.
    uint32 firstInstance = 0;

    if (IsGfx103(*m_pDevice))
    {
        firstInstance = 0x26;
    }
    else
    {
        PAL_NOT_IMPLEMENTED();
    }

    // Compute the HW event select from the DF subblock instance and subblock event ID.
    const uint32 eventSelect = ((firstInstance + subBlockInstance) << 6) | eventId;

    // DF EventSelect fields are 14 bits (in three sections). Verify that our event select can fit.
    PAL_ASSERT(eventSelect <= ((1 << 14) - 1));

    return eventSelect;
}

} // gfx9
} // Pal
