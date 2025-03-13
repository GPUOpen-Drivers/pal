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

#include "core/hw/gfxip/gfx12/gfx12CmdStream.h"
#include "core/hw/gfxip/gfx12/gfx12CmdUtil.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"
#include "core/hw/gfxip/gfx12/gfx12PerfExperiment.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/platform.h"
#include "palLiterals.h"
#include "palVectorImpl.h"

using namespace Util;
using namespace Util::Literals;

namespace Pal
{
namespace Gfx12
{

// We assume these enums match their SE indices in a few places.
static_assert(static_cast<uint32>(SpmDataSegmentType::Se0) == 0, "SpmDataSegmentType::Se0 is not 0.");
static_assert(static_cast<uint32>(SpmDataSegmentType::Se1) == 1, "SpmDataSegmentType::Se1 is not 1.");
static_assert(static_cast<uint32>(SpmDataSegmentType::Se2) == 2, "SpmDataSegmentType::Se2 is not 2.");
static_assert(static_cast<uint32>(SpmDataSegmentType::Se3) == 3, "SpmDataSegmentType::Se3 is not 3.");

// We assume the zero initialized register means modes are disabled
static_assert(CP_PERFMON_STATE_DISABLE_AND_RESET == 0);
static_assert(STRM_PERFMON_STATE_DISABLE_AND_RESET == 0);

// Default SQ select masks for our counter options (by default, select all).
constexpr uint32 DefaultSqSelectSimdMask   = 0xF;
constexpr uint32 DefaultSqSelectBankMask   = 0xF;
constexpr uint32 DefaultSqSelectClientMask = 0xF;

// Bitmask limits for some sqtt parameters.
constexpr uint32 SqttPerfCounterCuMask = 0xFFFF;
constexpr uint32 SqttDetailedSimdMask  = 0xF;
// Stall when at 6/8s of the output buffer because data will still come in from already-issued waves
constexpr uint32 SqttHiWaterValue = 5;
// Safe defaults for token exclude mask and register include+exclude mask for the SQTT_TOKEN_MASK register.
constexpr uint32 SqttRegIncludeMaskDefault   = (SQ_TT_TOKEN_MASK_SQDEC_BIT   |
                                                SQ_TT_TOKEN_MASK_SHDEC_BIT   |
                                                SQ_TT_TOKEN_MASK_GFXUDEC_BIT |
                                                SQ_TT_TOKEN_MASK_CONTEXT_BIT |
                                                SQ_TT_TOKEN_MASK_COMP_BIT);
constexpr uint32 SqttTokenExcludeMaskDefault = ((1 << SQ_TT_TOKEN_EXCLUDE_VMEMEXEC_SHIFT) |
                                                (1 << SQ_TT_TOKEN_EXCLUDE_ALUEXEC_SHIFT)  |
                                                (1 << SQ_TT_TOKEN_EXCLUDE_WAVERDY_SHIFT));
constexpr uint32 SqttRegExcludeMaskDefault   = 0x0;
// The low watermark will be set to high watermark minus low watermark offset. This is HW's recommended default.
constexpr uint32 SqttLoWaterOffsetValue = 4;

// The SPM ring buffer base address must be 32-byte aligned.
constexpr uint32 SpmRingBaseAlignment = 32;

// The DF SPM buffer alignment
constexpr uint32 DfSpmBufferAlignment = 0x10000;

// The bound GPU memory must be aligned to the maximum of all alignment requirements.
constexpr gpusize GpuMemoryAlignment = Max<gpusize>(SqttBufferAlignment, SpmRingBaseAlignment);

// Layout for SQWGP instance programming
union PerWgpInstanceLayout
{
    struct
    {
        uint32 blockIndex : 2; // The index of the block within the WGP.
        uint32 wgpIndex   : 3; // The WGP index within the SPI side of this shader array.
        uint32 isBelowSpi : 1; // 0 - The side with lower WGP numbers, 1 - the side with higher WGP numbers.
        uint32 reserved   : 26;
    } bits;

    uint32 u32All;
};

// =====================================================================================================================
// Converts the thread trace token config to the GFX12 format for programming the TOKEN_MASK register.
static regSQ_THREAD_TRACE_TOKEN_MASK GetSqttTokenMask(
    const ThreadTraceTokenConfig& tokenConfig)
{
    regSQ_THREAD_TRACE_TOKEN_MASK value = {};
    // Setting SPI_SQG_EVENT_CTL.bits.ENABLE_SQG_BOP_EVENTS to 1 only allows SPI to send BOP events to SQG.
    // If BOP_EVENTS_TOKEN_INCLUDE is 0, SQG will not issue BOP event token writes to SQTT buffer.
    value.bits.BOP_EVENTS_TOKEN_INCLUDE = 1;

    // Thread tracing of barrier completion events may cause a functional error where a shader instruction is lost.
    // Thread trace barrier must be disabled via EXCLUDE_BARRIER_WAIT = 1.
    value.bits.EXCLUDE_BARRIER_WAIT     = 1;

    const uint32 tokenExclude       = ~tokenConfig.tokenMask;
    const bool   vmemExecExclude    = TestAnyFlagSet(tokenExclude, ThreadTraceTokenTypeFlags::VmemExec);
    const bool   aluExecExclude     = TestAnyFlagSet(tokenExclude, ThreadTraceTokenTypeFlags::AluExec);
    const bool   valuInstExclude    = TestAnyFlagSet(tokenExclude, ThreadTraceTokenTypeFlags::ValuInst);
    const bool   waveRdyExclude     = TestAnyFlagSet(tokenExclude, ThreadTraceTokenTypeFlags::WaveRdy);
    const bool   immediateExclude   = TestAnyFlagSet(tokenExclude, ThreadTraceTokenTypeFlags::Immediate);
    const bool   utilCounterExclude = TestAnyFlagSet(tokenExclude, ThreadTraceTokenTypeFlags::UtilCounter);
    const bool   waveAllocExclude   = TestAnyFlagSet(tokenExclude, ThreadTraceTokenTypeFlags::WaveAlloc);
    const bool   realTimeExclude    = TestAnyFlagSet(tokenExclude, ThreadTraceTokenTypeFlags::RealTime);

    // Combine legacy TT enumerations with the newer (TT 3.3) enumerations.
    const bool regExclude           = TestAnyFlagSet(tokenExclude, ThreadTraceTokenTypeFlags::Reg   |
                                                                   ThreadTraceTokenTypeFlags::RegCs |
                                                                   ThreadTraceTokenTypeFlags::RegCsPriv);

    const bool eventExclude         = TestAnyFlagSet(tokenExclude, ThreadTraceTokenTypeFlags::Event   |
                                                                   ThreadTraceTokenTypeFlags::EventCs |
                                                                   ThreadTraceTokenTypeFlags::EventGfx1);

    const bool instExclude          = TestAnyFlagSet(tokenExclude, ThreadTraceTokenTypeFlags::Inst   |
                                                                   ThreadTraceTokenTypeFlags::InstPc |
                                                                   ThreadTraceTokenTypeFlags::InstUserData);

    const bool waveStartEndExclude  = TestAnyFlagSet(tokenExclude, ThreadTraceTokenTypeFlags::WaveStart |
                                                                   ThreadTraceTokenTypeFlags::WaveEnd);

    uint32 hwTokenExclude = ((vmemExecExclude     << SQ_TT_TOKEN_EXCLUDE_VMEMEXEC_SHIFT)     |
                             (aluExecExclude      << SQ_TT_TOKEN_EXCLUDE_ALUEXEC_SHIFT)      |
                             (valuInstExclude     << SQ_TT_TOKEN_EXCLUDE_VALUINST_SHIFT)     |
                             (waveRdyExclude      << SQ_TT_TOKEN_EXCLUDE_WAVERDY_SHIFT)      |
                             (waveStartEndExclude << SQ_TT_TOKEN_EXCLUDE_WAVESTARTEND_SHIFT) |
                             (immediateExclude    << SQ_TT_TOKEN_EXCLUDE_IMMEDIATE_SHIFT)    |
                             (utilCounterExclude  << SQ_TT_TOKEN_EXCLUDE_UTILCTR_SHIFT)      |
                             (waveAllocExclude    << SQ_TT_TOKEN_EXCLUDE_WAVEALLOC_SHIFT)    |
                             (regExclude          << SQ_TT_TOKEN_EXCLUDE_REG_SHIFT)          |
                             (eventExclude        << SQ_TT_TOKEN_EXCLUDE_EVENT_SHIFT)        |
                             (instExclude         << SQ_TT_TOKEN_EXCLUDE_INST_SHIFT)         |
                             (realTimeExclude     << SQ_TT_TOKEN_EXCLUDE_REALTIME));

    value.bits.TOKEN_EXCLUDE = hwTokenExclude;

    // Compute Register include mask. Obtain reg mask from combined legacy (TT 2.3 and below) and the newer (TT 3.3)
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

    // Warning. Attempting to trace all register reads or enabling thread trace to capture all GRBM and CSDATA bus
    // activity could cause GPU hang or generate lot of thread trace traffic.
    PAL_ALERT(grbmCsDataRegs);

    value.bits.REG_INCLUDE = ((sqdecRegs       << SQ_TT_TOKEN_MASK_SQDEC_SHIFT)   |
                              (shdecRegs       << SQ_TT_TOKEN_MASK_SHDEC_SHIFT)   |
                              (gfxudecRegs     << SQ_TT_TOKEN_MASK_GFXUDEC_SHIFT) |
                              (compRegs        << SQ_TT_TOKEN_MASK_COMP_SHIFT)    |
                              (contextRegs     << SQ_TT_TOKEN_MASK_CONTEXT_SHIFT) |
                              (otherConfigRegs << SQ_TT_TOKEN_MASK_CONFIG_SHIFT)  |
                              (grbmCsDataRegs  << SQ_TT_TOKEN_MASK_ALL_SHIFT));

    // Compute additional bits for the exclude mask.
    const bool userDataRegs = TestAnyFlagSet(tokenConfig.regMask, ThreadTraceRegTypeFlags::UserdataRegs);
    const bool cpmemcRegs   = TestAnyFlagSet(tokenConfig.regMask, ThreadTraceRegTypeFlags::AllRegReads);

    value.bits.REG_EXCLUDE = ~((userDataRegs << SQ_TT_REG_EXCLUDE_USER_DATA_SHIFT)      |
                               (cpmemcRegs   << SQ_TT_REG_EXCLUDE_CP_ME_MC_RADDR_SHIFT) |
                               (compRegs     << SQ_TT_REG_EXCLUDE_GRBM_COMPUTE_EXCLUDE_SHIFT));

    return value;
}

// =====================================================================================================================
// Helper to fill shader stage enable bits in SQ & SQG_PERFCOUNTER_CTRL. This function assumes SQ & SQG_PERFCOUNTER_CTRL
// hold the exact same fields in the exact same bit order.
static uint32 GetSqSqgPerfCounterCtrlBits(
    bool                      useClientMask,
    PerfExperimentShaderFlags shaderMask)
{
    const PerfExperimentShaderFlags sqgShaderMask = useClientMask ? shaderMask : PerfShaderMaskAll;

    regSQG_PERFCOUNTER_CTRL sqgPerfCounterCtrl = {};
    sqgPerfCounterCtrl.bits.PS_EN              = TestAnyFlagSet(sqgShaderMask, PerfShaderMaskPs);
    sqgPerfCounterCtrl.bits.GS_EN              = TestAnyFlagSet(sqgShaderMask, PerfShaderMaskGs);
    sqgPerfCounterCtrl.bits.HS_EN              = TestAnyFlagSet(sqgShaderMask, PerfShaderMaskHs);
    sqgPerfCounterCtrl.bits.CS_EN              = TestAnyFlagSet(sqgShaderMask, PerfShaderMaskCs);

    return sqgPerfCounterCtrl.u32All;
}

// =====================================================================================================================
// Returns true if we've enabled global or SPM counters for any of the non-generic blocks. Valid blocks accessed by this
// function are SQG, SQWGP, GRBMSE/GRBMH, and UMCCH.
template <typename T, size_t N>
static bool HasNonGenericGlobalCounters(
    const T (&block)[N])
{
    bool hasCounters = false;

    for (uint32 instance = 0; instance < ArrayLen(block); ++instance)
    {
        if (block[instance].hasCounters)
        {
            hasCounters = true;
            break;
        }
    }

    return hasCounters;
}

// =====================================================================================================================
// Identifies the proper filtertype
static SelectFilter GetEventFilter(
    GpuBlock block,
    uint32   eventId)
{
    SelectFilter type = SelectFilter::End;
    // TODO: Update the eventIDs when the regspec gets updated with these events
    if (block == GpuBlock::Cpg)
    {
        if ((eventId == 87) || (eventId == 94))
        {
            type = SelectFilter::PFP;
        }
        else if ((eventId == 88) || (eventId == 95))
        {
            type = SelectFilter::ME;
        }
    }
    else if (block == GpuBlock::Cpc)
    {
        if ((eventId == 45) || (eventId == 46))
        {
            type = SelectFilter::MES;
        }
        else if ((eventId >= 52) || (eventId <= 55))
        {
            type = SelectFilter::MEC;
        }
    }
    return type;
}

// =====================================================================================================================
PerfExperiment::PerfExperiment(
    const Device*                   pDevice,
    const PerfExperimentCreateInfo& createInfo)
    :
    Pal::PerfExperiment(pDevice->Parent(), createInfo, GpuMemoryAlignment),
    m_chipProps(pDevice->Parent()->ChipProperties()),
    m_counterInfo(pDevice->Parent()->ChipProperties().gfx9.perfCounterInfo.gfx12Info),
    m_settings(pDevice->Settings()),
    m_globalCounters(m_pPlatform),
    m_pSpmCounters(nullptr),
    m_numSpmCounters(0),
    m_spmSampleLines(0),
    m_gfx12MaxMuxSelLines(0),
    m_spmRingSize(0),
    m_spmMaxSamples(0),
    m_spmSampleInterval(0),
    m_pDfSpmCounters(nullptr),
    m_seWithActiveSqCounters(0)
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
    if ((m_counterInfo.block[static_cast<uint32>(GpuBlock::Sq)].numInstances     > MaxShaderEngines) ||
        (m_counterInfo.block[static_cast<uint32>(GpuBlock::SqWgp)].numInstances  > MaxWgps)          ||
        (m_counterInfo.block[static_cast<uint32>(GpuBlock::Dma)].numInstances    > MaxSdmaInstances) ||
        (m_counterInfo.block[static_cast<uint32>(GpuBlock::Umcch)].numInstances  > MaxUmcchInstances))
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
    const uint32 numInstances       = m_counterInfo.block[blockIdx].numInstances;
    const uint32 numGenericModules  = m_counterInfo.block[blockIdx].numGenericSpmModules +
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
// Converts provided counter info to appropriate filter to be set up for any counters needing the filtering
// NOTE: This overwrites any existing filters, so sequentially added counters that require the same filter register
//       will apply only the last provided filter
void PerfExperiment::AddFilter(
    const Pal::PerfCounterInfo& info)
{
    SelectFilter filter = GetEventFilter(info.block, info.eventId);
    if (filter != SelectFilter::End)
    {
        m_select.filters.activeFilters |= (1 << static_cast<uint8>(filter));
        switch (filter)
        {
        case SelectFilter::PFP:
            m_select.filters.PFP_RS64_CNTL = info.subConfig.rs64Cntl;
            break;
        case SelectFilter::ME:
            m_select.filters.ME_RS64_CNTL = info.subConfig.rs64Cntl;
            break;
        case SelectFilter::MES:
            m_select.filters.MES_RS64_CNTL = info.subConfig.rs64Cntl;
            break;
        case SelectFilter::MEC:
            m_select.filters.MEC_RS64_CNTL = info.subConfig.rs64Cntl;
            break;
        default:
            // What is this?
            PAL_ASSERT_ALWAYS();
            break;
        }
    }
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
        const uint32 block = static_cast<uint32>(info.block);

        AddFilter(info);

        if (info.block == GpuBlock::SqWgp)
        {
            m_seWithActiveSqCounters |= (1 << instanceMapping.seIndex);

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
                    m_select.sqWgp[info.instance].perfmonInUse[idx] = true;
                    PAL_ASSERT(((idx & 0x3) == 0) || (info.eventId <= SP_PERF_SEL_VALU_PENDING_QUEUE_STALL));
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
        else if (info.block == GpuBlock::Umcch)
        {
            // The UMCCH counters are physically 48-bit, but we dont have a good way to handle the upper dword register
            // also containing the threshold configuration when the client of the perf experiment can read the GPU
            // written buffer, containing the threshold in upper bits, without our intervention.
            // Skip the upper reg read and have the client consume just the 32bit value
            mapping.dataType = PerfCounterDataType::Uint32;

            // Find the next unused global counter in the special UMCCH state.
            bool searching = true;

            for (uint32 idx = 0; searching && (idx < ArrayLen(m_select.umcch[info.instance].perfmonInUse)); ++idx)
            {
                if (m_select.umcch[info.instance].perfmonInUse[idx] == false)
                {
                    m_select.umcch[info.instance].hasCounters                       = true;
                    m_select.umcch[info.instance].perfmonInUse[idx]                 = true;
                    m_select.umcch[info.instance].thresholdSet[idx]                 = false;
                    m_select.umcch[info.instance].perfmonCntl[idx].bits.EventSelect = info.eventId;
                    m_select.umcch[info.instance].perfmonCntl[idx].bits.Enable      = 1;
                    m_select.umcch[info.instance].perfmonCntl[idx].bits.RdWrMask    = info.subConfig.umc.rdWrMask;

                    // If the client sets these extra values trust that they've got it right.
                    // Several counters have configurable Thresholds, setup as follows...
                    //   Set ThreshCntEn = 2 for > (1 for <).
                    //   Set ThreshCnt to the amount to compare against.

                    // "DcqOccupancy" replaces earlier asics fixed DcqOccupancy_00/25/50/75/90 buckets.
                    // Current DCQ is 64x1 in size, so to replicate old fixed events set:
                    //   ThreshCntEn=2 (>)
                    //   ThreshCnt=00 to count all (>0%), 14 to count >25%, 31 to count >50%, 47 to count >75%
                    m_select.umcch[info.instance].perfmonCtrHi[idx].bits.ThreshCntEn = info.subConfig.umc.eventThresholdEn;
                    m_select.umcch[info.instance].perfmonCtrHi[idx].bits.ThreshCnt   = info.subConfig.umc.eventThreshold;

                    // flag that we need to configure threshold for this event
                    m_select.umcch[info.instance].thresholdSet[idx] = true;

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
            else
            {
                m_perfExperimentFlags.dfCtrsEnabled = 1;
            }
        }
        else if (m_select.pGeneric[block] != nullptr)
        {
            if (static_cast<GpuBlock>(block) == GpuBlock::Sq)
            {
                m_seWithActiveSqCounters |= (1 << instanceMapping.seIndex);
            }

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
                        // A global counter uses the whole perfmon module (0xF).
                        pSelect->pModules[moduleIdx].inUse                       = 0xF;
                        pSelect->pModules[moduleIdx].perfmon.sel0.bits.PERF_SEL  = info.eventId;
                        pSelect->pModules[moduleIdx].perfmon.sel0.bits.CNTR_MODE = PERFMON_SPM_MODE_OFF;
                        pSelect->pModules[moduleIdx].perfmon.sel0.bits.PERF_MODE = PERFMON_COUNTER_MODE_ACCUM;
                        break;

                    case SelectType::LegacySel:
                        // A global counter uses the whole legacy module (0xF).
                        pSelect->pModules[moduleIdx].inUse                    = 0xF;
                        pSelect->pModules[moduleIdx].legacySel.bits.PERF_SEL  = info.eventId;
                        pSelect->pModules[moduleIdx].legacySel.bits.PERF_MODE = PERFMON_COUNTER_MODE_ACCUM;
                        break;

                    case SelectType::LegacyCfg:
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
    const uint32 block          = static_cast<uint32>(info.block);
    uint32       spmWire        = 0;
    uint32       subCounterMask = 0;

    // Note that "LEVEL" counters require us to use the no-clamp & no-reset SPM mode
    constexpr PERFMON_SPM_MODE SpmModeTable[2][2] =
    {
        { PERFMON_SPM_MODE_16BIT_CLAMP, PERFMON_SPM_MODE_16BIT_NO_CLAMP },
        { PERFMON_SPM_MODE_32BIT_CLAMP, PERFMON_SPM_MODE_32BIT_NO_CLAMP }
    };

    // To be indexed with PerfExperimentSpmTestMode
    constexpr PERFMON_SPM_MODE SpmTestModes[] = { PERFMON_SPM_MODE_OFF,
                                                  PERFMON_SPM_MODE_TEST_MODE_0,
                                                  PERFMON_SPM_MODE_TEST_MODE_1,
                                                  PERFMON_SPM_MODE_TEST_MODE_2 };

    if (result == Result::Success)
    {
        AddFilter(info);

        // SQG only supports 32bit SPM
        const bool is32Bit = ((info.block == GpuBlock::Sq) || (info.counterType == PerfCounterType::Spm32));
        const bool isLevel = (info.block == GpuBlock::Sq) ? IsSqLevelEvent(info.eventId)
                                                          : (info.block == GpuBlock::SqWgp)
                                                            ? IsSqWgpLevelEvent(info.eventId) : false;
        const PerfExperimentSpmTestMode testMode = m_pDevice->Settings().perfExperimentSpmTestMode;
        const PERFMON_SPM_MODE spmMode = testMode == PerfExperimentSpmTestMode::Disabled
                                         ? SpmModeTable[is32Bit][isLevel] : SpmTestModes[testMode];

        if (info.block == GpuBlock::SqWgp)
        {
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
                    m_select.sqWgp[info.instance].perfmonInUse[idx]           = true;
                    m_select.sqWgp[info.instance].perfmon[idx].bits.PERF_SEL  = info.eventId;
                    m_select.sqWgp[info.instance].perfmon[idx].bits.SPM_MODE  = spmMode;
                    m_select.sqWgp[info.instance].perfmon[idx].bits.PERF_MODE = PERFMON_COUNTER_MODE_ACCUM;

                    if (is32Bit)
                    {
                        // 32bit uses two 16bit slots, so reserve the second slot
                        // Hardware doesnt use the "odd" selects in 32bit mode, but maintain default programming
                        m_select.sqWgp[info.instance].perfmonInUse[idx + 1]   = true;
                        m_select.sqWgp[info.instance].perfmon[idx + 1].u32All = mmSQ_PERFCOUNTER0_SELECT_DEFAULT;

                        subCounterMask = 3;
                    }
                    else
                    {
                        subCounterMask = 1 << (idx % 2);
                    }

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
        else if (info.block == GpuBlock::RlcUser)
        {
            // This block refers to user data registers available for marker purposes
            m_select.rlcUser.hasCounters = true;

            if (m_select.rlcUser.perfmonInUse[info.instance] == false)
            {
                m_select.rlcUser.perfmonInUse[info.instance] = true;
                // Each RLC User Data gets a single wire with one 32-bit counter (select both 16-bit halves).
                spmWire        = info.instance;
                subCounterMask = 0x3;
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
                    // Each wire holds two 16-bit sub-counters. We must check each wire individually because
                    // some blocks look like they have a whole perfmon module but only use half of it.
                    if (spmWire < m_counterInfo.block[block].numSpmWires)
                    {
                        if (TestAnyFlagSet(pSelect->pModules[idx].inUse, 0x1) == false)
                        {
                            // Each 32bit module requires both 16-bit halves.
                            subCounterMask = (is32Bit) ? 0x3 : 0x1;

                            pSelect->pModules[idx].inUse                      |= subCounterMask;
                            pSelect->pModules[idx].perfmon.sel0.bits.PERF_SEL  = info.eventId;
                            pSelect->pModules[idx].perfmon.sel0.bits.CNTR_MODE = spmMode;
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

                    // SQ implementation has only sel0
                    if ((spmWire < m_counterInfo.block[block].numSpmWires) && (info.block != GpuBlock::Sq))
                    {
                        if (TestAnyFlagSet(pSelect->pModules[idx].inUse, 0x4) == false)
                        {
                            // Each 32bit module requires both 16-bit halves.
                            subCounterMask = (is32Bit) ? 0x3 : 0x1;

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
        const uint32 stallMode = (traceInfo.optionFlags.threadTraceStallBehavior != 0) ?
                                       traceInfo.optionValues.threadTraceStallBehavior : GpuProfilerStallAlways;

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
            }
        }

        m_sqtt[realInstance].ctrl.bits.MODE            = SQ_TT_MODE_ON;
        m_sqtt[realInstance].ctrl.bits.HIWATER         = SqttHiWaterValue;
        m_sqtt[realInstance].ctrl.bits.UTIL_TIMER      = 1;
        m_sqtt[realInstance].ctrl.bits.DRAW_EVENT_EN   = 1;

        m_sqtt[realInstance].ctrl.bits.LOWATER_OFFSET  = SqttLoWaterOffsetValue;

        // On Navi2x hw, the polarity of AutoFlushMode is inverted, thus this step is necessary to correct
        m_sqtt[realInstance].ctrl.bits.AUTO_FLUSH_MODE = 0;

        m_sqtt[realInstance].ctrl.bits.SPI_STALL_EN    = (stallMode != GpuProfilerStallNever);
        m_sqtt[realInstance].ctrl.bits.SQ_STALL_EN     = (stallMode != GpuProfilerStallNever);
        m_sqtt[realInstance].ctrl.bits.REG_AT_HWM      = (stallMode == GpuProfilerStallAlways)     ? 2 :
                                                         (stallMode == GpuProfilerStallLoseDetail) ? 1 : 0;

        // By default don't stall all SIMDs.
        m_sqtt[realInstance].ctrl.bits.STALL_ALL_SIMDS = ((traceInfo.optionFlags.threadTraceStallAllSimds != 0) &&
                                                          (traceInfo.optionValues.threadTraceStallAllSimds));

        static_assert((static_cast<uint32>(PerfShaderMaskPs) == static_cast<uint32>(SQ_TT_WTYPE_INCLUDE_PS_BIT) &&
                       static_cast<uint32>(PerfShaderMaskGs) == static_cast<uint32>(SQ_TT_WTYPE_INCLUDE_GS_BIT) &&
                       static_cast<uint32>(PerfShaderMaskHs) == static_cast<uint32>(SQ_TT_WTYPE_INCLUDE_HS_BIT) &&
                       static_cast<uint32>(PerfShaderMaskCs) == static_cast<uint32>(SQ_TT_WTYPE_INCLUDE_CS_BIT)));

        // ES/LS/VS are unsupported, unset those flags
        uint32 validFlags = ~(static_cast<uint32>(PerfShaderMaskEs) |
                              static_cast<uint32>(PerfShaderMaskLs) |
                              static_cast<uint32>(PerfShaderMaskVs));

        m_sqtt[realInstance].mask.bits.WTYPE_INCLUDE = static_cast<uint32>(shaderMask) & validFlags;

        m_sqtt[realInstance].mask.bits.SA_SEL = shIndex;

        // Divide by two to convert from CUs to WGPs.
        m_sqtt[realInstance].mask.bits.WGP_SEL = cuIndex / 2;

        // Default to getting detailed tokens from SIMD 0.
        m_sqtt[realInstance].mask.bits.SIMD_SEL = (traceInfo.optionFlags.threadTraceSimdMask != 0) ?
                                                  traceInfo.optionValues.threadTraceSimdMask       : 0;

        m_sqtt[realInstance].mask.bits.EXCLUDE_NONDETAIL_SHADERDATA =
            (traceInfo.optionFlags.threadTraceExcludeNonDetailShaderData != 0) &&
            traceInfo.optionValues.threadTraceExcludeNonDetailShaderData;

        if (traceInfo.optionFlags.threadTraceTokenConfig != 0)
        {
            m_sqtt[realInstance].tokenMask = GetSqttTokenMask(traceInfo.optionValues.threadTraceTokenConfig);
        }
        else
        {
            // By default trace all tokens and registers.
            m_sqtt[realInstance].tokenMask.bits.TOKEN_EXCLUDE = SqttTokenExcludeMaskDefault;
            m_sqtt[realInstance].tokenMask.bits.REG_INCLUDE   = SqttRegIncludeMaskDefault;
            m_sqtt[realInstance].tokenMask.bits.REG_EXCLUDE   = SqttRegExcludeMaskDefault;
        }
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 899
        if (traceInfo.optionFlags.threadTraceEnableExecPop)
        {
            m_sqtt[realInstance].tokenMask.bits.TTRACE_EXEC = traceInfo.optionValues.threadTraceEnableExecPop;
        }
#endif
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
    else if (m_perfExperimentFlags.dfCtrsEnabled)
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
            m_dfSpmPerfmonInfo.perfmonEvents[i]        = GetMallEventSelect(info.eventId, info.instance);
            m_dfSpmPerfmonInfo.perfmonUnitMasks[i]     = info.subConfig.df.eventQualifier & 0xFF;
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
    createInfo.size                = Pow2Align(dfSpmBufferSize, DfSpmBufferAlignment);
    createInfo.alignment           = DfSpmBufferAlignment;
    createInfo.vaRange             = VaRange::Default;
    createInfo.priority            = GpuMemPriority::High;
    createInfo.mallPolicy          = GpuMemMallPolicy::Never;
    createInfo.flags.gl2Uncached   = 1;
    createInfo.flags.cpuInvisible  = 1;
    // Ensure a fall back to local is available in case there is no Invisible Memory.
    if (m_pDevice->HeapLogicalSize(GpuHeapInvisible) > 0)
    {
        createInfo.heapCount = 3;
        createInfo.heaps[0]  = GpuHeapInvisible;
        createInfo.heaps[1]  = GpuHeapLocal;
        createInfo.heaps[2]  = GpuHeapGartCacheable;
    }
    else
    {
        createInfo.heapCount = 2;
        createInfo.heaps[0]  = GpuHeapLocal;
        createInfo.heaps[1]  = GpuHeapGartCacheable;
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

    constexpr uint32 MinSpmInterval = 32;

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
    else if ((spmCreateInfo.spmInterval < MinSpmInterval) || (spmCreateInfo.spmInterval > UINT16_MAX))
    {
        // The sample interval must be >= MinSpmInterval and must fit in 16 bits.
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

    m_gfx12MaxMuxSelLines = 0;

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
            m_gfx12MaxMuxSelLines     = Max(m_gfx12MaxMuxSelLines, totalLines);
        }
    }

    for (int32 segment = 0; (result == Result::Success) && (segment < MaxNumSpmSegments); segment++)
    {
        if (static_cast<SpmDataSegmentType>(segment) != SpmDataSegmentType::Global)
        {
            m_numMuxselLines[segment] = m_gfx12MaxMuxSelLines;
        }
    }

    for (int32 segment = 0; (result == Result::Success) && (segment < MaxNumSpmSegments); segment++)
    {
        if (m_numMuxselLines[segment] == 0)
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
        for (uint32 segment = 0; segment < MaxNumSpmSegments; ++segment)
        {
            if (m_pMuxselRams[segment] != nullptr)
            {
                // Figure out where this entire segment starts in sample memory. The RLC hardware hard-codes this
                // order: Global, SE0, SE1, ... , SEN. Add up the sizes of those segments in order until
                // we find our segment.
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

                // Walk through the even and odd lines in parallel, adding all enabled counters. In this logic we assume
                // all counters are 16-bit even if we're running 32-bit SPM. This works out fine because the RLC splits
                // all values into 16-bit chunks and writes them to memory independently.
                uint32 evenCounterIdx = 0;
                uint32 evenLineIdx    = 0;
                uint32 oddCounterIdx  = 0;
                uint32 oddLineIdx     = 1;

                if (static_cast<SpmDataSegmentType>(segment) == SpmDataSegmentType::Global)
                {
                    // The global segment always starts with a 64-bit timestamp, that's 4 16-bit counters worth of data.
                    constexpr uint32 NumGlobalTimestampCounters = sizeof(uint64) / sizeof(uint16);

                    for (; evenCounterIdx < NumGlobalTimestampCounters; evenCounterIdx++)
                    {
                        // Select the REFCLK timestamp counter
                        MuxselEncoding timestampMuxsel = {};
                        timestampMuxsel.glbSeSa.block     = 31; // RSPM
                        timestampMuxsel.glbSeSa.instance  =  2; // REFCLK timestamp count
                        timestampMuxsel.glbSeSa.counter   = evenCounterIdx;

                        m_pMuxselRams[segment][evenLineIdx].muxsel[evenCounterIdx] = timestampMuxsel;
                    }
                }

                for (uint32 idx = 0; idx < m_numSpmCounters; ++idx)
                {
                    if (static_cast<uint32>(m_pSpmCounters[idx].segment) == segment)
                    {
                        if (m_pSpmCounters[idx].isEven)
                        {
                            // If this counter has an even part it always contains the lower 16 bits. Find its offset
                            // within each sample in units of 16-bit counters and then convert that to bytes.
                            const uint32 offset = segmentOffset + evenLineIdx * MuxselLineSizeInCounters +
                                                  evenCounterIdx;
                            m_pSpmCounters[idx].offsetLo  = offset * sizeof(uint16);

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
                            // Otherwise this counter is 16-bit and it's the lower half. Find its offset
                            // within each sample in units of 16-bit counters and then convert that to bytes.
                            const uint32 offset = segmentOffset + oddLineIdx * MuxselLineSizeInCounters + oddCounterIdx;

                            if (m_pSpmCounters[idx].isEven)
                            {
                                m_pSpmCounters[idx].offsetHi = offset * sizeof(uint16);
                            }
                            else
                            {
                                m_pSpmCounters[idx].offsetLo = offset * sizeof(uint16);
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

        m_spmSampleLines = 0;

        for (uint32 idx = 0; idx < MaxNumSpmSegments; ++idx)
        {
            m_spmSampleLines += m_numMuxselLines[idx];
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

            // When begin counter samples are disabled we still reserve space for a single uint64 which SampleBegin sets
            // to zero. We will point every counter's beginValueOffset to this shared zero value.
            const gpusize beginSize = m_settings.forcePerfExperimentBeginCounterSample ? globalSize : sizeof(uint64);

            // Denote where the "begin" and "end" sections live in the bound GPU memory.
            m_globalBeginOffset = m_totalMemSize;
            m_globalEndOffset   = m_globalBeginOffset + beginSize;
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

            // If begin counters are disabled, force the client to use a shared zeroed-out value for all counters.
            const gpusize beginValueOffset = m_settings.forcePerfExperimentBeginCounterSample ? mapping.offset : 0;

            pLayout->samples[idx].block            = mapping.general.block;
            pLayout->samples[idx].instance         = mapping.general.globalInstance;
            pLayout->samples[idx].slot             = mapping.counterId;
            pLayout->samples[idx].eventId          = mapping.general.eventId;
            pLayout->samples[idx].dataType         = mapping.dataType;
            pLayout->samples[idx].beginValueOffset = m_globalBeginOffset + beginValueOffset;
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
                    pLayout->traces[traceIdx].computeUnit = m_sqtt[idx].mask.bits.WGP_SEL;

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

        // The samples start one line in.
        pLayout->sampleOffset  = SampleLineSizeInBytes;
        pLayout->sampleStride  = SampleLineSizeInBytes * m_spmSampleLines;
        pLayout->maxNumSamples = m_spmMaxSamples;
        pLayout->numCounters   = m_numSpmCounters;

        for (uint32 idx = 0; idx < m_numSpmCounters; ++idx)
        {
            const SpmCounterMapping& mapping = m_pSpmCounters[idx];
            SpmCounterData*const     pOut    = pLayout->counterData + idx;

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

        pCmdSpace = pCmdStream->WritePerfCounterWindow(true, pCmdSpace);

        // WaitIdle ensures the work before Begin is not profiled in this experiment
        pCmdSpace = WriteWaitIdle(m_flushCache, pCmdBuffer, pCmdStream, pCmdSpace);

        regCP_PERFMON_CNTL cpPerfmonCntl = {};
        // Disable and reset all types of perf counters. We will enable the counters when everything is ready.
        // Note that PERFMON_ENABLE_MODE controls per-context filtering which we don't support.
        cpPerfmonCntl.bits.PERFMON_STATE       = CP_PERFMON_STATE_DISABLE_AND_RESET;
        cpPerfmonCntl.bits.SPM_PERFMON_STATE   = STRM_PERFMON_STATE_DISABLE_AND_RESET;
        cpPerfmonCntl.bits.PERFMON_ENABLE_MODE = CP_PERFMON_ENABLE_MODE_ALWAYS_COUNT;

        pCmdSpace = WriteCpPerfmonCtrl(cpPerfmonCntl, pCmdStream, pCmdSpace);

        // Thread traces and many types of perf counters require SQG events. To keep things simple we should just
        // enable them unconditionally. This shouldn't have any effect in the cases that don't really need them on.
        pCmdSpace = WriteUpdateSpiSqgEventCtl(true, pCmdStream, pCmdSpace);

        if (m_perfExperimentFlags.perfCtrsEnabled || m_perfExperimentFlags.spmTraceEnabled)
        {
            pCmdSpace = WriteFilters(pCmdStream, pCmdSpace);
            pCmdSpace = WriteSelectRegisters(pCmdStream, pCmdSpace);
        }

        if (m_perfExperimentFlags.spmTraceEnabled)
        {
            pCmdSpace = WriteSpmSetup(pCmdStream, pCmdSpace);
        }

        if (m_perfExperimentFlags.sqtTraceEnabled)
        {
            pCmdSpace = WriteStartThreadTraces(pCmdStream, pCmdSpace);
        }

        // Cfg mix the clear bit with the sample controls. Track whether we cleared at the time of sample
        bool cfgCleared = false;

        if (m_perfExperimentFlags.perfCtrsEnabled)
        {
            if (m_settings.forcePerfExperimentBeginCounterSample)
            {
                pCmdSpace  = WriteStopAndSample(true, true, pCmdBuffer, pCmdStream, pCmdSpace);
                cfgCleared = true;
            }
            else
            {
                // Zero our single qword used for all begin values if avoiding the individual samples
                const uint32  ZeroTs[2] = {};
                WriteDataInfo writeZero = {};
                writeZero.engineType = pCmdStream->GetEngineType();
                writeZero.engineSel = engine_sel__me_write_data__micro_engine;
                writeZero.dstAddr = m_gpuMemory.GpuVirtAddr() + m_globalBeginOffset;
                writeZero.dstSel = dst_sel__me_write_data__tc_l2;
                pCmdSpace += CmdUtil::BuildWriteData(writeZero, ArrayLen(ZeroTs), ZeroTs, pCmdSpace);
            }
        }

        // Tell the SPM counters and global counters start counting.
        if (m_perfExperimentFlags.perfCtrsEnabled || m_perfExperimentFlags.spmTraceEnabled)
        {
            // Order here is from most costly and infrequent to most important to reduce observing perfmon ops

            pCmdSpace = WriteCfgRegisters(true, (cfgCleared == false), pCmdStream, pCmdSpace);
            pCmdSpace = WriteUpdateWindowedCounters(true, pCmdStream, pCmdSpace);

            if (m_perfExperimentFlags.perfCtrsEnabled)
            {
                cpPerfmonCntl.bits.PERFMON_STATE = CP_PERFMON_STATE_START_COUNTING;
            }

            if (m_perfExperimentFlags.spmTraceEnabled)
            {
                cpPerfmonCntl.bits.SPM_PERFMON_STATE = STRM_PERFMON_STATE_START_COUNTING;
            }

            pCmdSpace = WriteCpPerfmonCtrl(cpPerfmonCntl, pCmdStream, pCmdSpace);

        }

        pCmdSpace = pCmdStream->WritePerfCounterWindow(false, pCmdSpace);

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

    if (m_isFinalized == false)
    {
        // It's illegal to execute a perf experiment before it's finalized.
        PAL_ASSERT_ALWAYS();
    }
    else
    {
        uint32* pCmdSpace = pCmdStream->ReserveCommands();

        pCmdSpace = pCmdStream->WritePerfCounterWindow(true, pCmdSpace);

        // This will WaitIdle, transition the counter state to "stop", and take end samples if enabled
        pCmdSpace = WriteStopAndSample(m_perfExperimentFlags.perfCtrsEnabled, false, pCmdBuffer, pCmdStream, pCmdSpace);

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
        pCmdSpace = WriteCpPerfmonCtrl(cpPerfmonCntl, pCmdStream, pCmdSpace);

        // Restore SPI_SQG_EVENT_CTL by turning SQG events back off.
        pCmdSpace = WriteUpdateSpiSqgEventCtl(false, pCmdStream, pCmdSpace);

        pCmdSpace = WriteDisableDfCounters(pCmdStream, pCmdSpace);

        pCmdSpace = pCmdStream->WritePerfCounterWindow(false , pCmdSpace);

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

        pCmdSpace = pCmdStream->WritePerfCounterWindow(true, pCmdSpace);

        // Issue the necessary commands to stop counter collection (SPM and global counters) without resetting
        // any counter programming.

        // First stop windowed counters, then stop global counters.
        pCmdSpace = WriteUpdateWindowedCounters(false, pCmdStream, pCmdSpace);

        // NOTE: We probably should add a wait-idle here. If we don't wait the global counters will stop counting
        // while the prior draw/dispatch is still active which will under count. There is no wait here currently
        // because the old perf experiment code didn't wait.

        // Write CP_PERFMON_CNTL such that SPM and global counters stop counting.
        regCP_PERFMON_CNTL cpPerfmonCntl = {};

        if (m_perfExperimentFlags.perfCtrsEnabled)
        {
            cpPerfmonCntl.bits.PERFMON_STATE = CP_PERFMON_STATE_STOP_COUNTING;
        }
        if (m_perfExperimentFlags.spmTraceEnabled)
        {
            cpPerfmonCntl.bits.SPM_PERFMON_STATE = STRM_PERFMON_STATE_STOP_COUNTING;
        }

        pCmdSpace = WriteCpPerfmonCtrl(cpPerfmonCntl, pCmdStream, pCmdSpace);
        pCmdSpace = WriteCfgRegisters(false, false, pCmdStream, pCmdSpace);

        pCmdSpace = pCmdStream->WritePerfCounterWindow(false, pCmdSpace);

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

        pCmdSpace = pCmdStream->WritePerfCounterWindow(true, pCmdSpace);

        // Enable Cfg counters first because they take longer to enable
        pCmdSpace = WriteCfgRegisters(true, false, pCmdStream, pCmdSpace);

        // NOTE: We probably should add a wait-idle here. If we don't wait the global counters will start counting
        // while the internal draw/dispatch is still active and it will be counted. There is no wait here currently
        // because the old perf experiment code didn't wait.

        pCmdSpace = WriteUpdateWindowedCounters(true, pCmdStream, pCmdSpace);

        // Rewrite the "start" state for all counters.
        regCP_PERFMON_CNTL cpPerfmonCntl       = {};

        if (m_perfExperimentFlags.perfCtrsEnabled)
        {
            cpPerfmonCntl.bits.PERFMON_STATE = CP_PERFMON_STATE_START_COUNTING;
        }
        if (m_perfExperimentFlags.spmTraceEnabled)
        {
            cpPerfmonCntl.bits.SPM_PERFMON_STATE = STRM_PERFMON_STATE_START_COUNTING;
        }

        pCmdSpace = WriteCpPerfmonCtrl(cpPerfmonCntl, pCmdStream, pCmdSpace);
        pCmdSpace = pCmdStream->WritePerfCounterWindow(false, pCmdSpace);

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

    if (m_isFinalized == false)
    {
        // It's illegal to execute a perf experiment before it's finalized.
        PAL_ASSERT_ALWAYS();
    }
    else if (m_perfExperimentFlags.sqtTraceEnabled)
    {
        CmdStream* const              pCmdStream = static_cast<CmdStream*>(pPalCmdStream);
        uint32*                       pCmdSpace  = pCmdStream->ReserveCommands();
        regSQ_THREAD_TRACE_TOKEN_MASK tokenMask  = GetSqttTokenMask(sqttTokenConfig);

        pCmdSpace = pCmdStream->WritePerfCounterWindow(true, pCmdSpace);

        for (uint32 idx = 0; idx < ArrayLen(m_sqtt); ++idx)
        {
            if (m_sqtt[idx].inUse)
            {
                pCmdSpace = WriteGrbmGfxIndexInstance(m_sqtt[idx].grbmGfxIndex, pCmdStream, pCmdSpace);

                // These fields aren't controlled by the token config.
                tokenMask.bits.INST_EXCLUDE   = m_sqtt[idx].tokenMask.bits.INST_EXCLUDE;
                tokenMask.bits.REG_DETAIL_ALL = m_sqtt[idx].tokenMask.bits.REG_DETAIL_ALL;

                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_TOKEN_MASK,
                                                              tokenMask.u32All,
                                                              pCmdSpace);

            }
        }

        // Switch back to global broadcasting before returning to the rest of PAL.
        pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);

        pCmdSpace = pCmdStream->WritePerfCounterWindow(false , pCmdSpace);

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
    const ThreadTraceTokenConfig& sqttTokenConfig)
{
    CmdStream*const               pCmdStream = static_cast<CmdStream*>(pPalCmdStream);
    uint32*                       pCmdSpace  = pCmdStream->ReserveCommands();
    regSQ_THREAD_TRACE_TOKEN_MASK tokenMask  = GetSqttTokenMask(sqttTokenConfig);

    // Note that we will lose the current value of the INST_EXCLUDE and REG_DETAIL_ALL fields. They default
    // to zero so hopefully the default value is fine.
    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_TOKEN_MASK, tokenMask.u32All, pCmdSpace);

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
    else if (m_counterInfo.block[static_cast<uint32>(info.block)].distribution == PerfCounterDistribution::Unavailable)
    {
        // This block is not available on this GPU.
        result = Result::ErrorInvalidValue;
    }
    else if (info.instance > m_counterInfo.block[static_cast<uint32>(info.block)].numInstances)
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
        seIndex       = globalInstance / blockInfo.numScopedInstances;
        instanceIndex = globalInstance % blockInfo.numScopedInstances;
    }
    else if (blockInfo.distribution == PerfCounterDistribution::PerShaderArray)
    {
        // From outermost to innermost, the internal indices are in the order: SE, SA, local instance.
        seIndex       = (globalInstance / blockInfo.numScopedInstances) / m_chipProps.gfx9.numShaderArrays;
        saIndex       = (globalInstance / blockInfo.numScopedInstances) % m_chipProps.gfx9.numShaderArrays;
        instanceIndex = globalInstance % blockInfo.numScopedInstances;
    }

    if (seIndex >= m_chipProps.gfx9.numShaderEngines)
    {
        // This shader engine doesn't exist on our device.
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
    grbmGfxIndex.bits.SE_INDEX     = VirtualSeToRealSe(mapping.seIndex);
    grbmGfxIndex.bits.SA_INDEX     = mapping.saIndex;

    switch (m_counterInfo.block[static_cast<uint32>(block)].distribution)
    {
    case PerfCounterDistribution::GlobalBlock:
        // Global block writes should broadcast to SEs and SAs.
        grbmGfxIndex.bits.SE_BROADCAST_WRITES  = 1;
        // Intentional fall through
    case PerfCounterDistribution::PerShaderEngine:
        // Per-SE block writes should broadcast to SAs.
        grbmGfxIndex.bits.SA_BROADCAST_WRITES = 1;
        break;
    default:
        // Otherwise no broadcast bits should be set.
        break;
    }

    // Some blocks use a different instance index format that requires some bit swizzling.
    uint32 instance = mapping.instanceIndex;

    if ((block == GpuBlock::Ta) || (block == GpuBlock::Td) || (block == GpuBlock::Tcp))
    {
        PerWgpInstanceLayout instanceIndex = {};

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
    else if (block == GpuBlock::SqWgp)
    {
        PerWgpInstanceLayout instanceIndex = {};

        // Based on code from Pal::Gfx12::InitializeGpuChipProperties below:
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

    grbmGfxIndex.bits.INSTANCE_INDEX = instance;

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
    const PerfCounterBlockInfo& blockInfo = m_counterInfo.block[static_cast<uint32>(block)];

    if (block == GpuBlock::SqWgp)
    {
        muxsel.wgp.counter     = counter;
        muxsel.wgp.instance    = 0;
        muxsel.wgp.wgp         = mapping.instanceIndex;
        muxsel.wgp.shaderArray = mapping.saIndex;
        muxsel.wgp.block       = blockInfo.spmBlockSelect;
    }
    else if (block == GpuBlock::RlcUser)
    {
        muxsel.glbSeSa.counter  = counter;
        muxsel.glbSeSa.instance = 4;
        muxsel.glbSeSa.block    = blockInfo.spmBlockSelect;
    }
    else
    {
        // Check that we can re-use the SA form for Glb and SE blocks
        PAL_ASSERT(mapping.instanceIndex < (1 << 5)); // 5 == bitwidth of instance field

        // Other WGP blocks can use WGP+Instance as a joint CU ID same as per SA
        muxsel.glbSeSa.counter     = counter;
        muxsel.glbSeSa.instance    = mapping.instanceIndex;
        muxsel.glbSeSa.shaderArray = mapping.saIndex;
        muxsel.glbSeSa.block       = blockInfo.spmBlockSelect;
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
    writeZero.engineType    = pCmdStream->GetEngineType();
    writeZero.engineSel     = engine_sel__me_write_data__micro_engine;
    writeZero.dstAddr       = ringBaseAddr + m_spmRingSize - m_spmSampleLines * SampleLineSizeInBytes;
    writeZero.dstSel        = dst_sel__me_write_data__tc_l2;
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

    pCmdSpace = CmdStream::WriteSetSeqUConfigRegs(mmRLC_SPM_PERFMON_CNTL,
                                                  mmRLC_SPM_PERFMON_RING_SIZE,
                                                  &rlcInit,
                                                  pCmdSpace);

    // HW uses this reg value as offset. If PAL doesn't zero out this register
    // then the WRPTR value only continues to grow. This moves the result data further and further into the SPM data
    // buffer. Originally an undocumented change in the SPM initialization procedure from Navi2X where we don't have to
    // set this register. The docs have been updated to include this register. Also listed in the new HW documentation
    // are the RLC_SPM_SEGMENT_THRESHOLD and RLC_SPM_RING_RDPTR registers. PAL is intentionally not setting those
    // registers because PAL doesn't have SPM stall's or SPM interrupts enabled. The documentation refers to this as
    // "RING_MODE == 0". If PAL ever trys to enable either one of those features then PAL should set both of those
    // registers below along with the RLC_SPM_RING_WRPTR. Be aware that in particular setting the RLC_SPM_RING_RDPTR
    // register requires "privilege" either enabled manually in the CP mircocode or by the KMD.
    regRLC_SPM_RING_WRPTR rlcSpmRingWrptr   = {};
    rlcSpmRingWrptr.bits.PERFMON_RING_WRPTR = 0;
    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_RING_WRPTR, rlcSpmRingWrptr.u32All, pCmdSpace);

    // Program the muxsel line sizes. Note that PERFMON_SEGMENT_SIZE only has space for 31 lines per segment.
    regRLC_SPM_PERFMON_SEGMENT_SIZE spmSegmentSize = {};

    bool   over31Lines = false;
    uint32 totalLines  = 0;

    for (uint32 idx = 0; idx < MaxNumSpmSegments; ++idx)
    {
        over31Lines = over31Lines || (m_numMuxselLines[idx] > 31);
        totalLines += m_numMuxselLines[idx];
    }

    // RLC_SPM_ACCUM_MODE needs its state reset as we've disabled GPO when entering stable pstate.
    constexpr regRLC_SPM_ACCUM_MODE rlcSpmAccumMode = {};
    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_ACCUM_MODE, rlcSpmAccumMode.u32All, pCmdSpace);

    // TOTAL_NUM_SEGMENT should be (global + SE_NUM_SEGMENT * numActiveShaderEngines)
    spmSegmentSize.bits.TOTAL_NUM_SEGMENT  = totalLines;
    spmSegmentSize.bits.GLOBAL_NUM_SEGMENT = m_numMuxselLines[static_cast<uint32>(SpmDataSegmentType::Global)];
    // There is only one segment size value for Gfx12. Every shader engine line count will be set to whatever was
    // the highest value found in the spm config.
    spmSegmentSize.bits.SE_NUM_SEGMENT     = m_gfx12MaxMuxSelLines;

    PAL_ASSERT(m_chipProps.gfx9.numActiveShaderEngines <= 6);

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_SEGMENT_SIZE, spmSegmentSize.u32All, pCmdSpace);

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

                writeData.dstAddr = mmRLC_SPM_GLOBAL_MUXSEL_DATA;
                muxselAddr        = mmRLC_SPM_GLOBAL_MUXSEL_ADDR;
            }
            else
            {
                pCmdSpace = WriteGrbmGfxIndexBroadcastSe(idx, pCmdStream, pCmdSpace);

                writeData.dstAddr = mmRLC_SPM_SE_MUXSEL_DATA;
                muxselAddr        = mmRLC_SPM_SE_MUXSEL_ADDR;
            }

            writeData.engineType = pCmdStream->GetEngineType();
            writeData.engineSel  = engine_sel__me_write_data__micro_engine;
            writeData.dstSel     = dst_sel__me_write_data__mem_mapped_register;

            // Each data value must be written into MUXSEL_DATA, if we let the CP increment the register address
            // we will overwrite other registers.
            writeData.dontIncrementAddr = true;

            // The muxsel ram is inlined into the command stream and could be large so we need a loop that carefully
            // splits it into chunks and repeatedly commits and reserves space. We assume we get the user-config branch
            // when defining PacketHeaders below.
            constexpr uint32 PacketHeaders = CmdUtil::SetOneUConfigRegSizeDwords + CmdUtil::WriteDataSizeDwords(0);
            const uint32     maxDwords     = pCmdStream->ReserveLimit() - PacketHeaders;
            const uint32     maxLines      = maxDwords / MuxselLineSizeInDwords;

            for (uint32 line = 0; line < m_numMuxselLines[idx]; line += maxLines)
            {
                const uint32 numLines = Min(maxLines, m_numMuxselLines[idx] - line);

                pCmdStream->CommitCommands(pCmdSpace);
                pCmdSpace = pCmdStream->ReserveCommands();

                // Each time we issue a new write_data we must first update MUXSEL_ADDR to point to the next muxsel.
                pCmdSpace  = pCmdStream->WriteSetOnePerfCtrReg(muxselAddr,  line * MuxselLineSizeInDwords, pCmdSpace);

                pCmdSpace += CmdUtil::BuildWriteData(writeData,
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

            pCmdSpace = WriteGrbmGfxIndexInstance(m_sqtt[idx].grbmGfxIndex, pCmdStream, pCmdSpace);

            const gpusize shiftedAddr = (m_gpuMemory.GpuVirtAddr() + m_sqtt[idx].bufferOffset) >> SqttBufferAlignShift;
            const gpusize shiftedSize = m_sqtt[idx].bufferSize >> SqttBufferAlignShift;

            regSQ_THREAD_TRACE_BUF0_BASE_LO sqttBuf0BaseLo = {};
            regSQ_THREAD_TRACE_BUF0_BASE_HI sqttBuf0BaseHi = {};
            regSQ_THREAD_TRACE_BUF0_SIZE    sqttBuf0Size   = {};

            sqttBuf0Size.bits.SIZE      = shiftedSize;
            sqttBuf0BaseHi.bits.BASE_HI = HighPart(shiftedAddr);
            sqttBuf0BaseLo.bits.BASE_LO = LowPart(shiftedAddr);

            //  These 3 registers must be written in this order: SIZE, BASE_LO and then BASE_HI
            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_BUF0_SIZE,
                                                          sqttBuf0Size.u32All,
                                                          pCmdSpace);

            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_BUF0_BASE_LO,
                                                          sqttBuf0BaseLo.u32All,
                                                          pCmdSpace);

            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_BUF0_BASE_HI,
                                                          sqttBuf0BaseHi.u32All,
                                                          pCmdSpace);

            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_WPTR,
                                                          0,
                                                          pCmdSpace);

            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_MASK,
                                                          m_sqtt[idx].mask.u32All,
                                                          pCmdSpace);

            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_TOKEN_MASK,
                                                          m_sqtt[idx].tokenMask.u32All,
                                                          pCmdSpace);

            // We must write this register last because it turns on thread traces.
            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_CTRL,
                                                          m_sqtt[idx].ctrl.u32All,
                                                          pCmdSpace);
        }
    }

    // Start the thread traces. The spec says it's best to use an event on graphics but we should write the
    // THREAD_TRACE_ENABLE register on compute.
    pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);

    if (m_pDevice->EngineSupportsGraphics(pCmdStream->GetEngineType()))
    {
        pCmdSpace += CmdUtil::BuildNonSampleEventWrite(THREAD_TRACE_START, pCmdStream->GetEngineType(), pCmdSpace);
    }
    else
    {
        regCOMPUTE_THREAD_TRACE_ENABLE computeEnable = {};
        computeEnable.bits.THREAD_TRACE_ENABLE       = 1;

        pCmdSpace = CmdStream::WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_THREAD_TRACE_ENABLE,
                                                               computeEnable.u32All,
                                                               pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// A helper function for IssueEnd which writes the necessary commands to stop all thread traces.
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
        pCmdSpace += CmdUtil::BuildNonSampleEventWrite(THREAD_TRACE_STOP, engineType, pCmdSpace);
    }
    else
    {
        regCOMPUTE_THREAD_TRACE_ENABLE computeEnable = {};
        computeEnable.bits.THREAD_TRACE_ENABLE = 0;

        pCmdSpace = CmdStream::WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_THREAD_TRACE_ENABLE,
                                                               computeEnable.u32All,
                                                               pCmdSpace);
    }

    // Send a TRACE_FINISH event (even on compute).
    pCmdSpace += CmdUtil::BuildNonSampleEventWrite(THREAD_TRACE_FINISH, engineType, pCmdSpace);

    for (uint32 idx = 0; idx < ArrayLen(m_sqtt); ++idx)
    {
        if (m_sqtt[idx].inUse)
        {
            // Get fresh command space once per trace, just in case.
            pCmdStream->CommitCommands(pCmdSpace);
            pCmdSpace = pCmdStream->ReserveCommands();

            pCmdSpace = WriteGrbmGfxIndexInstance(m_sqtt[idx].grbmGfxIndex, pCmdStream, pCmdSpace);

            // Poll the status register's finish_done bit to be sure that the trace buffer is written out.
            pCmdSpace += CmdUtil::BuildWaitRegMem(engineType,
                                                  mem_space__me_wait_reg_mem__register_space,
                                                  function__me_wait_reg_mem__not_equal_reference_value,
                                                  engine_sel__me_wait_reg_mem__micro_engine,
                                                  mmSQ_THREAD_TRACE_STATUS,
                                                  0,
                                                  SQ_THREAD_TRACE_STATUS__FINISH_DONE_MASK,
                                                  pCmdSpace);

            // Set the mode to "OFF".
            regSQ_THREAD_TRACE_CTRL sqttCtrl = m_sqtt[idx].ctrl;
            sqttCtrl.bits.MODE = SQ_TT_MODE_OFF;

            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_CTRL,
                                                          sqttCtrl.u32All,
                                                          pCmdSpace);

            // Poll the status register's busy bit to wait for it to totally turn off.
            pCmdSpace += CmdUtil::BuildWaitRegMem(engineType,
                                                  mem_space__me_wait_reg_mem__register_space,
                                                  function__me_wait_reg_mem__equal_to_the_reference_value,
                                                  engine_sel__me_wait_reg_mem__micro_engine,
                                                  mmSQ_THREAD_TRACE_STATUS,
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
                mmSQ_THREAD_TRACE_WPTR,
                mmSQ_THREAD_TRACE_STATUS,
                mmSQ_THREAD_TRACE_DROPPED_CNTR
            };

            for (uint32 regIdx = 0; regIdx < ArrayLen(InfoRegisters); regIdx++)
            {
                pCmdSpace = pCmdStream->WriteCopyPerfCtrRegToMemory(InfoRegisters[regIdx],
                                                                    infoAddr + regIdx * sizeof(uint32),
                                                                    pCmdSpace);
            }
        }
    }

    // Restore global broadcasting.
    pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
// A helper function for WriteSelectRegisters that sets up filters that affect the selected event behavior
uint32* PerfExperiment::WriteFilters(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    // SQ_PERFCOUNTER_CTRL controls how the SQs increment their perf counters. We treat it as global state.
    regSQ_PERFCOUNTER_CTRL sqPerfCounterCtrl = {};

    sqPerfCounterCtrl.u32All = GetSqSqgPerfCounterCtrlBits(m_createInfo.optionFlags.sqWgpShaderMask,
                                                            m_createInfo.optionValues.sqWgpShaderMask);

    // Note that we must write this after CP_PERFMON_CNTRL because the CP ties ownership of this state to it.
    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_PERFCOUNTER_CTRL, sqPerfCounterCtrl.u32All, pCmdSpace);

    regSQG_PERFCOUNTER_CTRL sqgPerfCounterCtrl = {};
    // Note that PAL's GpuBlock::Sq maps to the SQG counters so we use sqShaderMask here.
    sqgPerfCounterCtrl.u32All = GetSqSqgPerfCounterCtrlBits(m_createInfo.optionFlags.sqShaderMask,
                                                            m_createInfo.optionValues.sqShaderMask);

    // Set this bit to ensure the subsequent write(s) WriteStopAndSample sync to a different value
    // If begin sample is enabled, the next value to be read is one, so here write to 0
    // if begin sample is disabled, the next value to be read is zero, so here write to 1
    PAL_ASSERT(m_settings.waPreventSqgTimingRace == true);
    sqgPerfCounterCtrl.bitfields.DISABLE_ME1PIPE3_PERF = (m_settings.forcePerfExperimentBeginCounterSample ? 0 : 1);

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQG_PERFCOUNTER_CTRL, sqgPerfCounterCtrl.u32All, pCmdSpace);

    if (m_select.filters.activeFilters != 0)
    {
        for (uint8 bit = 0; bit < static_cast<uint8>(SelectFilter::End); bit++)
        {
            if (BitfieldIsSet<uint32>(m_select.filters.activeFilters, bit))
            {
                uint32       regVal = 0;
                uint32       offset = 0;
                SelectFilter filter = (SelectFilter)bit;

                if (filter == SelectFilter::PFP)
                {
                    regCP_GFX_RS64_PERFCOUNT_CNTL0 reg = {};
                    reg.bits.EVENT_SEL = m_select.filters.PFP_RS64_CNTL;
                    regVal = reg.u32All;
                    offset = mmCP_GFX_RS64_PERFCOUNT_CNTL0;
                }
                else if (filter == SelectFilter::ME)
                {
                    regCP_GFX_RS64_PERFCOUNT_CNTL1 reg = {};
                    reg.bits.EVENT_SEL = m_select.filters.ME_RS64_CNTL;
                    regVal = reg.u32All;
                    offset = mmCP_GFX_RS64_PERFCOUNT_CNTL1;
                }
                else if (filter == SelectFilter::MES)
                {
                    regCP_MES_PERFCOUNT_CNTL reg = {};
                    reg.bits.EVENT_SEL = m_select.filters.MES_RS64_CNTL;
                    regVal = reg.u32All;
                    offset = mmCP_MES_PERFCOUNT_CNTL;
                }
                else if (filter == SelectFilter::MEC)
                {
                    regCP_MEC_RS64_PERFCOUNT_CNTL reg = {};
                    reg.bits.EVENT_SEL = m_select.filters.MEC_RS64_CNTL;
                    regVal = reg.u32All;
                    offset = mmCP_MEC_RS64_PERFCOUNT_CNTL;
                }
                else
                {
                    PAL_NOT_IMPLEMENTED();
                }

                if (offset != 0)
                {
                    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(offset, regVal, pCmdSpace);
                }
            }
        }
        // Get fresh command space just in case we're close to running out.
        pCmdStream->CommitCommands(pCmdSpace);
        pCmdSpace = pCmdStream->ReserveCommands();
    }
    return pCmdSpace;
}

// =====================================================================================================================
// A helper function for IssueBegin which writes the necessary commands to set every enabled PERFCOUNTER#_SELECT.
uint32* PerfExperiment::WriteSelectRegisters(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    const bool forceBroadcast = m_settings.perfExperimentGlobalSelect;

    if (forceBroadcast)
    {
        pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);
    }

    for (uint32 instance = 0; instance < ArrayLen(m_select.sqWgp); ++instance)
    {
        if (m_select.sqWgp[instance].hasCounters)
        {
            constexpr uint32 SimdPerWgp = 4;

            const PerfCounterRegAddr& regAddr = m_counterInfo.block[static_cast<uint32>(GpuBlock::SqWgp)].regAddr;

            const uint32 loopLength = forceBroadcast ? 1 : SimdPerWgp;

            // While the counters themselves are present at the WGP level, the logic that feeds them is duplicated
            // per SIMD, requiring us to direct the programming repeatedly across all SIMD so they all count
            // forceBroadcast alleviates this requirement by sending the write to all SIMD simultaneously
            for (uint32 simd = 0; simd < loopLength; simd++)
            {
                if (forceBroadcast == false)
                {
                    regGRBM_GFX_INDEX    reg           = m_select.sqWgp[instance].grbmGfxIndex;
                    PerWgpInstanceLayout instanceIndex = { .u32All = reg.bitfields.INSTANCE_INDEX };

                    // Update the blockIndex necessary for programming all SIMD to the same select
                    instanceIndex.bits.blockIndex = simd;

                    // Propagate the instance back to the local register value
                    reg.bitfields.INSTANCE_INDEX = instanceIndex.u32All;

                    pCmdSpace = WriteGrbmGfxIndexInstance(reg, pCmdStream, pCmdSpace);
                }

                for (uint32 idx = 0; idx < ArrayLen(m_select.sqWgp[instance].perfmon); ++idx)
                {
                    if (m_select.sqWgp[instance].perfmonInUse[idx])
                    {
                        PAL_ASSERT(regAddr.perfcounter[idx].selectOrCfg != 0);

                        pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regAddr.perfcounter[idx].selectOrCfg,
                                                                      m_select.sqWgp[instance].perfmon[idx].u32All,
                                                                      pCmdSpace);
                    }
                }
            }

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
                    PAL_ASSERT(m_counterInfo.umcchRegAddr[instance].perModule[idx].selectOrCfg != 0);

                    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(
                        m_counterInfo.umcchRegAddr[instance].perModule[idx].selectOrCfg,
                        m_select.umcch[instance].perfmonCntl[idx].u32All,
                        pCmdSpace);

                    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(
                        m_counterInfo.umcchRegAddr[instance].perModule[idx].hi,
                        m_select.umcch[instance].perfmonCtrHi[idx].u32All,
                        pCmdSpace);
                }
            }

            // Get fresh command space just in case we're close to running out.
            pCmdStream->CommitCommands(pCmdSpace);
            pCmdSpace = pCmdStream->ReserveCommands();
        }
    }

    const DfSelectState& dfSelect = m_select.df;
    if (dfSelect.hasCounters)
    {
        // Reset broadcast should not be needed since DF not part of graphics, but let's be safe with a known state
        pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);

        for (uint32 idx = 0; idx < ArrayLen(dfSelect.perfmonConfig); ++idx)
        {
            if (dfSelect.perfmonConfig[idx].perfmonInUse)
            {
                const uint32                       eventSelect   = dfSelect.perfmonConfig[idx].eventSelect;
                const uint16                       eventUnitMask = dfSelect.perfmonConfig[idx].eventUnitMask;
                DF_PIE_AON_PerfMonCtlLo0           perfmonCtlLo  = {};
                DF_PIE_AON_PerfMonCtlHi0           perfmonCtlHi  = {};
                const PerfCounterRegAddrPerModule& regs          =
                    m_counterInfo.block[uint32(GpuBlock::DfMall)].regAddr.perfcounter[idx];

                // Manually reset counters to zero before enabling below. DF has no global reset.
                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regs.lo, 0, pCmdSpace);
                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regs.hi, 0, pCmdSpace);

                perfmonCtlLo.bits.En            = 1;
                perfmonCtlLo.bits.UnitMaskLo    = BitExtract(eventUnitMask, 0, 7);
                perfmonCtlLo.bits.UnitMaskHi    = BitExtract(eventUnitMask, 8, 11);
                perfmonCtlLo.bits.EventSelectLo = BitExtract(eventSelect, 0, 7);
                perfmonCtlHi.bits.EventSelectHi = BitExtract(eventSelect, 8, 13);

                // By convention we put the CtlLo in selectOrCfg and the CtlHi in select1.
                PAL_ASSERT((regs.selectOrCfg != 0) && (regs.select1 != 0));

                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regs.selectOrCfg, perfmonCtlLo.u32All, pCmdSpace);
                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regs.select1, perfmonCtlHi.u32All, pCmdSpace);

                // Each write can be a copy_data which uses many dwords of space. Re-reserve to avoid overflow
                pCmdStream->CommitCommands(pCmdSpace);
                pCmdSpace = pCmdStream->ReserveCommands();
            }
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
                    if (forceBroadcast == false)
                    {
                        // Write GRBM_GFX_INDEX to target this specific block instance and enable its active modules.
                        pCmdSpace = WriteGrbmGfxIndexInstance(select.grbmGfxIndex, pCmdStream, pCmdSpace);
                    }

                    for (uint32 idx = 0; idx < select.numModules; ++idx)
                    {
                        if (select.pModules[idx].inUse != 0)
                        {
                            const PerfCounterRegAddrPerModule* pRegAddr = nullptr;

                            if (block == static_cast<uint32>(GpuBlock::Dma))
                            {
                                // SDMA has unique registers for each instance.
                                pRegAddr = &m_counterInfo.sdmaRegAddr[instance][idx];
                            }
                            else
                            {
                                pRegAddr = &m_counterInfo.block[block].regAddr.perfcounter[idx];
                            }

                            if (select.pModules[idx].type == SelectType::Perfmon)
                            {
                                PAL_ASSERT((pRegAddr->selectOrCfg != 0));

                                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(pRegAddr->selectOrCfg,
                                                                              select.pModules[idx].perfmon.sel0.u32All,
                                                                              pCmdSpace);

                                if (pRegAddr->select1 != 0)
                                {
                                    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(pRegAddr->select1,
                                                                                  select.pModules[idx].perfmon.sel1.u32All,
                                                                                  pCmdSpace);
                                }
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
// A helper function which writes the necessary commands to control all cfg-style blocks.
uint32* PerfExperiment::WriteCfgRegisters(
    bool       enable,
    bool       clear,
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    for (uint32 block = 0; block < GpuBlockCount; ++block)
    {
        // Check for an active instance before we broadcast this register. We only write it once.
        if (m_counterInfo.block[block].isCfgStyle && HasGenericCounters(static_cast<GpuBlock>(block)))
        {
            ResultCntl resultCntl = {};
            resultCntl.bits.ENABLE_ANY = enable;
            resultCntl.bits.CLEAR_ALL  = clear;

            PAL_ASSERT(m_counterInfo.block[block].regAddr.perfcounterRsltCntl != 0);

            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(m_counterInfo.block[block].regAddr.perfcounterRsltCntl,
                                                          resultCntl.u32All,
                                                          pCmdSpace);
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
                perfmonCtlClk.bits.GlblReset   = 1;
                perfmonCtlClk.u32All          |= PerfMonCtlClk__GlblResetMsk_MASK;

                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(m_counterInfo.umcchRegAddr[instance].perfMonCtlClk,
                                                              perfmonCtlClk.u32All,
                                                              pCmdSpace);
            }

            regPerfMonCtlClk perfmonCtlClk = {};
            perfmonCtlClk.bits.GlblMonEn   = enable;

            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(m_counterInfo.umcchRegAddr[instance].perfMonCtlClk,
                                                          perfmonCtlClk.u32All,
                                                          pCmdSpace);

            // Assume each counter uses the same amount of space and determine if next loop we'll run out
            pCmdSpace = pCmdStream->ReReserveCommands(pCmdSpace, pCmdSpace - pStartSpace);
            pStartSpace = pCmdSpace;
        }
    }

    // Get fresh command space just in case we're close to running out.
    pCmdStream->CommitCommands(pCmdSpace);
    pCmdSpace = pCmdStream->ReserveCommands();

    return pCmdSpace;
}

// =====================================================================================================================
// A helper function to write necessary commands to stop perf counters and sample them. It will leave counters stopped.
uint32* PerfExperiment::WriteStopAndSample(
    bool          sample,
    bool          isBegin,
    GfxCmdBuffer* pCmdBuffer,
    CmdStream*    pCmdStream,
    uint32*       pCmdSpace
    ) const
{
    const EngineType   engineType    = pCmdStream->GetEngineType();
    // IssueBegin starts with WaitIdle + flush to exclude prior work
    bool               cacheFlush    = m_flushCache && (isBegin == false);
    regCP_PERFMON_CNTL cpPerfmonCntl = {};

    if (m_perfExperimentFlags.perfCtrsEnabled)
    {
        cpPerfmonCntl.bits.PERFMON_STATE = CP_PERFMON_STATE_STOP_COUNTING;
    }
    if (m_perfExperimentFlags.spmTraceEnabled)
    {
        cpPerfmonCntl.bits.SPM_PERFMON_STATE = STRM_PERFMON_STATE_STOP_COUNTING;
    }

    // A mix of blocks across the GC require the PERFCOUNTER pipeline events and/or global control via CP_PERFMON_CNTL
    // We must do both to accomplish a broad sample and stop
    if (sample)
    {
        // Expect counters to sample globally if requested to sample
        PAL_ASSERT(m_perfExperimentFlags.perfCtrsEnabled);
        // PERFCOUNTER_SAMPLE is pipelined so can be safely run through the pipe before WaitIdle
        pCmdSpace += CmdUtil::BuildNonSampleEventWrite(PERFCOUNTER_SAMPLE, engineType, pCmdSpace);
        // Enable SAMPLE for global counters
        cpPerfmonCntl.bits.PERFMON_SAMPLE_ENABLE = 1;
    }

    // Flush and wait to ensure all prior work has completed operation before disabling counters
    pCmdSpace = WriteWaitIdle(cacheFlush, pCmdBuffer, pCmdStream, pCmdSpace);
    // Stop windowed counters after the sample has completed through the pipeline
    pCmdSpace = WriteUpdateWindowedCounters(false, pCmdStream, pCmdSpace);
    // Send global stop signals
    pCmdSpace = WriteCpPerfmonCtrl(cpPerfmonCntl, pCmdStream, pCmdSpace);
    // Sq for GFX12 has a GRBM fifo that needs extra synchronization to ensure the sample has completed
    pCmdSpace = WriteSqSync(isBegin, pCmdStream, pCmdSpace);
    // Stop and optionally clear the config type counters
    pCmdSpace = WriteCfgRegisters(false, isBegin, pCmdStream, pCmdSpace);

    PAL_ASSERT(m_perfExperimentFlags.perfCtrsEnabled || (m_globalCounters.NumElements() == 0));
    const bool    sampleSelect = m_settings.perfExperimentSampleSelect;
    const gpusize destBaseAddr = m_gpuMemory.GpuVirtAddr() + (isBegin ? m_globalBeginOffset : m_globalEndOffset);

    // Copy each counter's value from registers to memory, one at a time.
    for (uint32 idx = 0; idx < m_globalCounters.NumElements(); ++idx)
    {
        const GlobalCounterMapping&        mapping  = m_globalCounters.At(idx);
        const uint32                       instance = mapping.general.globalInstance;
        const uint32                       block    = static_cast<uint32>(mapping.general.block);
        const PerfCounterRegAddrPerModule* pRegs    = nullptr;

        if (mapping.general.block == GpuBlock::SqWgp)
        {
            pCmdSpace = WriteGrbmGfxIndexInstance(m_select.sqWgp[instance].grbmGfxIndex, pCmdStream, pCmdSpace);
            pRegs = &m_counterInfo.block[block].regAddr.perfcounter[mapping.counterId];
        }
        else if (mapping.general.block == GpuBlock::Umcch)
        {
            // The UMCCH is global and has registers that vary per-instance and per-counter.
            pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);
            pRegs = &m_counterInfo.umcchRegAddr[instance].perModule[mapping.counterId];
        }
        else if (mapping.general.block == GpuBlock::DfMall)
        {
            // The DF is global and has registers that vary per-counter.
            pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);
            pRegs = &m_counterInfo.block[static_cast<uint32>(GpuBlock::DfMall)].regAddr.perfcounter[mapping.counterId];
        }
        else if (m_select.pGeneric[block] != nullptr)
        {
            // Set GRBM_GFX_INDEX so that we're talking to the specific block instance which own the given counter.
            pCmdSpace = WriteGrbmGfxIndexInstance(m_select.pGeneric[block][instance].grbmGfxIndex,
                                                  pCmdStream,
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

            if (block == static_cast<uint32>(GpuBlock::Dma))
            {
                // SDMA has unique registers for each instance.
                pRegs = &m_counterInfo.sdmaRegAddr[instance][mapping.counterId];
            }
            else
            {
                pRegs = &m_counterInfo.block[block].regAddr.perfcounter[mapping.counterId];
            }
        }
        else
        {
            // What block did we forget to implement?
            PAL_ASSERT_ALWAYS();
        }

        if (pRegs != nullptr)
        {
            const gpusize destAddr = destBaseAddr + mapping.offset;
            const uint32  loOffset = sampleSelect ? pRegs->selectOrCfg : pRegs->lo;
            PAL_ASSERT(loOffset != 0);
            pCmdSpace = pCmdStream->WriteCopyPerfCtrRegToMemory(loOffset, destAddr, pCmdSpace);

            if (mapping.dataType == PerfCounterDataType::Uint64)
            {
                const uint32 hiOffset = sampleSelect ? pRegs->select1 : pRegs->hi;
                PAL_ASSERT(hiOffset != 0);
                pCmdSpace = pCmdStream->WriteCopyPerfCtrRegToMemory(hiOffset, destAddr + sizeof(uint32), pCmdSpace);
            }

            // Get fresh command space just in case we're close to running out.
            pCmdStream->CommitCommands(pCmdSpace);
            pCmdSpace = pCmdStream->ReserveCommands();
        }
    }

    pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
// Writes sequence in the given command space to disable the DF counters
uint32* PerfExperiment::WriteDisableDfCounters(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    // The DF doesn't listen to CP_PERFMON_CNTL and doesn't have a global cfg on/off switch so individually set the
    // enable to false for each counter. This also clears the lo event select and mask but at end sample we no longer
    // need the rest of the PerfmonCtlLo.
    const DfSelectState& select = m_select.df;
    if (select.hasCounters)
    {
        for (uint32 idx = 0; idx < ArrayLen(select.perfmonConfig); ++idx)
        {
            if (select.perfmonConfig[idx].perfmonInUse)
            {
                constexpr DF_PIE_AON_PerfMonCtlLo0 PerfmonCtlLo = {};
                constexpr DF_PIE_AON_PerfMonCtlHi0 PerfmonCtlHi = {};
                const PerfCounterRegAddrPerModule& regs = m_counterInfo.block[uint32(GpuBlock::DfMall)]
                                                                       .regAddr.perfcounter[idx];
                // By convention we put the CtlLo in selectOrCfg
                PAL_ASSERT(regs.selectOrCfg != 0);
                // By convention we put the CtlHi in select1
                PAL_ASSERT(regs.select1 != 0);

                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regs.selectOrCfg, PerfmonCtlLo.u32All, pCmdSpace);

                // Technically only the lo needs written to clear the enable bit, but testing has shown the
                // counters misbehave at the next enable after writing only the lo.
                pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regs.select1, PerfmonCtlHi.u32All, pCmdSpace);
            }
        }

        // Get fresh command space just in case we're close to running out.
        pCmdStream->CommitCommands(pCmdSpace);
        pCmdSpace = pCmdStream->ReserveCommands();
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Writes GRBM_GFX_INDEX in the given command space such that we direct reads or writes to a specific instance
uint32* PerfExperiment::WriteGrbmGfxIndexInstance(
    regGRBM_GFX_INDEX grbmGfxIndex,
    CmdStream*        pCmdStream,
    uint32*           pCmdSpace
    ) const
{
    return pCmdStream->WriteSetOneUConfigReg(mmGRBM_GFX_INDEX, grbmGfxIndex.u32All, pCmdSpace);
}

// =====================================================================================================================
// Writes GRBM_GFX_INDEX in the given command space such that we are broadcasting to all instances on the whole chip.
uint32* PerfExperiment::WriteGrbmGfxIndexBroadcastGlobal(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    regGRBM_GFX_INDEX grbmGfxIndex              = {};
    grbmGfxIndex.bits.SE_BROADCAST_WRITES       = 1;
    grbmGfxIndex.bits.SA_BROADCAST_WRITES       = 1;
    grbmGfxIndex.bits.INSTANCE_BROADCAST_WRITES = 1;

    return pCmdStream->WriteSetOneUConfigReg(mmGRBM_GFX_INDEX, grbmGfxIndex.u32All, pCmdSpace);
}

// =====================================================================================================================
// Writes GRBM_GFX_INDEX in the given command space such that we are broadcasting to all instances in a given SE.
uint32* PerfExperiment::WriteGrbmGfxIndexBroadcastSe(
    uint32     seIndex,
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    regGRBM_GFX_INDEX grbmGfxIndex              = {};
    grbmGfxIndex.bits.SE_INDEX                  = VirtualSeToRealSe(seIndex);
    grbmGfxIndex.bits.SA_BROADCAST_WRITES       = 1;
    grbmGfxIndex.bits.INSTANCE_BROADCAST_WRITES = 1;

    return pCmdStream->WriteSetOneUConfigReg(mmGRBM_GFX_INDEX, grbmGfxIndex.u32All, pCmdSpace);
}

// =====================================================================================================================
// Writes a packet that updates the SQG event controls in SPI_SQG_EVENT_CTL.
uint32* PerfExperiment::WriteUpdateSpiSqgEventCtl(
    bool       enableSqgEvents,
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    regSPI_SQG_EVENT_CTL spiSqgEventCntl = {};

    spiSqgEventCntl.bits.ENABLE_SQG_TOP_EVENTS = enableSqgEvents;
    spiSqgEventCntl.bits.ENABLE_SQG_BOP_EVENTS = enableSqgEvents;

    pCmdSpace = pCmdStream->WriteSetOneUConfigReg(mmSPI_SQG_EVENT_CTL, spiSqgEventCntl.u32All, pCmdSpace);

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
            pCmdSpace += CmdUtil::BuildNonSampleEventWrite(PERFCOUNTER_START,
                                                           pCmdStream->GetEngineType(),
                                                           pCmdSpace);
        }
        else
        {
            pCmdSpace += CmdUtil::BuildNonSampleEventWrite(PERFCOUNTER_STOP,
                                                           pCmdStream->GetEngineType(),
                                                           pCmdSpace);
        }
    }

    regCOMPUTE_PERFCOUNT_ENABLE computeEnable = {};
    computeEnable.bits.PERFCOUNT_ENABLE       = enable;

    pCmdSpace = CmdStream::WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_PERFCOUNT_ENABLE,
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
    WriteWaitEopInfo waitEopInfo = {};
    waitEopInfo.hwGlxSync  = flushCaches ? SyncGlxWbInvAll : SyncGlxNone;
    waitEopInfo.hwRbSync   = (flushCaches && pCmdBuffer->IsGraphicsSupported()) ? SyncRbWbInv : SyncRbNone;
    waitEopInfo.hwAcqPoint = AcquirePointPfp;

    return pCmdBuffer->WriteWaitEop(waitEopInfo, pCmdSpace);
}

// =====================================================================================================================
// Writes the necessary packets to set and synchronize CP_PERFMON_CTRL across SE
uint32* PerfExperiment::WriteCpPerfmonCtrl(
    regCP_PERFMON_CNTL cpPerfmonCntl,
    CmdStream*         pCmdStream,
    uint32*            pCmdSpace
    ) const
{

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmCP_PERFMON_CNTL, cpPerfmonCntl.u32All, pCmdSpace);

    if (Pal::Device::EngineSupportsGraphics(pCmdStream->GetEngineType()))
    {
        // Ensure all SEs receive the update before continuing
        for (uint32 se = 0; se < m_chipProps.gfx9.numActiveShaderEngines; se++)
        {
            pCmdSpace = WriteGrbmGfxIndexBroadcastSe(se, pCmdStream, pCmdSpace);

            constexpr regGRBMH_SYNC GrbmhSync = { .bits = {.GFX_PIPE0_PERFMON_SYNC = 1 } };

            pCmdSpace += CmdUtil::BuildWaitRegMem(pCmdStream->GetEngineType(),
                                                    mem_space__me_wait_reg_mem__register_space,
                                                    function__me_wait_reg_mem__equal_to_the_reference_value,
                                                    engine_sel__me_wait_reg_mem__micro_engine,
                                                    mmGRBMH_SYNC,
                                                    GrbmhSync.u32All,
                                                    GrbmhSync.u32All,
                                                    pCmdSpace);

            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmGRBMH_SYNC, GrbmhSync.u32All, pCmdSpace);
        }

        pCmdSpace = WriteGrbmGfxIndexBroadcastGlobal(pCmdStream, pCmdSpace);
    }

    // Get fresh command space
    pCmdStream->CommitCommands(pCmdSpace);
    pCmdSpace = pCmdStream->ReserveCommands();

    return pCmdSpace;
}

// =====================================================================================================================
// Writes the necessary packets to ensure SQ/SQG are synchronized to the latest write
uint32* PerfExperiment::WriteSqSync(
    bool       flagBit,
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    if (m_seWithActiveSqCounters != 0)
    {
        const EngineType        engineType         = pCmdStream->GetEngineType();
        regSQG_PERFCOUNTER_CTRL sqgPerfCounterCtrl = {};

        // Note that PAL's GpuBlock::Sq maps to the SQG counters so we use sqShaderMask here.
        sqgPerfCounterCtrl.u32All = GetSqSqgPerfCounterCtrlBits(m_createInfo.optionFlags.sqShaderMask,
                                                                m_createInfo.optionValues.sqShaderMask);

        sqgPerfCounterCtrl.bitfields.DISABLE_ME1PIPE3_PERF = flagBit;

        // Double check the 'WA' is active & new programming model actually required
        PAL_ASSERT(m_settings.waPreventSqgTimingRace == true);
        // In GFX12, the GRBM module in SQG (SH) is re-designed and the order of CP write/read transactions cannot be
        // guaranteed. That can potentially break the usage of the perf counter in legacy mode. To collect the perf
        // counters values in legacy mode, we issue a perf sample, through grbm write to
        // CP_PERFMON_CNTL.PERFMON_SAMPLE_ENABLE, and followed by read of the perf counters, through grbm read of
        // SQG/SQC/SQ_PERFCOUNTER[0,1,…]_LO/HI registers. Since the read can be ahead of the write, wrong perf counters
        // results may be reported.
        // To summarize:
        // The SQG may stall the write of CP_PERFMON_CNTL, which can result in the read getting ahead of CP_PERFMON_CNTL
        // and returning old counter values. As a 'WA', we can poll on a known value to ensure it has completed. HW
        // suggests toggling the value of SQ_PERFCOUNTER_CTRL.DISABLE_ME1PIPE3_PERF and polling till the value is
        // confirmed to be set. We can toggle this value b/w IssueBegin() and IssueEnd().
        pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQG_PERFCOUNTER_CTRL, sqgPerfCounterCtrl.u32All, pCmdSpace);

        for (uint32 se = 0; se < m_chipProps.gfx9.numActiveShaderEngines; se++)
        {
            if (BitfieldIsSet<uint32>(m_seWithActiveSqCounters, se))
            {
                pCmdSpace = WriteGrbmGfxIndexBroadcastSe(se, pCmdStream, pCmdSpace);

                // Wait for DISABLE_ME1PIPE3_PERF to be set
                pCmdSpace += CmdUtil::BuildWaitRegMem(engineType,
                    mem_space__me_wait_reg_mem__register_space,
                    function__me_wait_reg_mem__equal_to_the_reference_value,
                    engine_sel__me_wait_reg_mem__micro_engine,
                    mmSQG_PERFCOUNTER_CTRL,
                    sqgPerfCounterCtrl.u32All,
                    UINT32_MAX,
                    pCmdSpace);
            }
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
// Assuming this is an SQG counter select, return true if it's a "LEVEL" counter, which require special SPM handling.
bool PerfExperiment::IsSqLevelEvent(
    uint32 eventId
    ) const
{
    bool isLevelEvent = false;

    if (eventId == SQG_PERF_SEL_LEVEL_WGP_ACTIVE)
    {
        isLevelEvent = true;
    }

    return isLevelEvent;
}

// =====================================================================================================================
// Assuming this is an SqWgp counter select, return true if it's a "LEVEL" counter, which require special SPM handling.
bool PerfExperiment::IsSqWgpLevelEvent(
    uint32 eventId
    ) const
{
    bool isLevelEvent = false;

    if (eventId == SQ_PERF_SEL_LEVEL_WAVES)
    {
        isLevelEvent = true;
    }
    else if ((eventId >= SQ_PERF_SEL_INST_LEVEL_EXP) &&
             (eventId <= SQ_PERF_SEL_INST_LEVEL_TEX_STORE))
    {
        isLevelEvent = true;
    }
    else if (eventId == SQ_PERF_SEL_IFETCH_LEVEL)
    {
        isLevelEvent = true;
    }
    else if ((eventId >= SQ_PERF_SEL_USER_LEVEL0) &&
             (eventId <= SQ_PERF_SEL_USER_LEVEL15))
    {
        isLevelEvent = true;
    }
    else if ((eventId >= SQ_PERF_SEL_INSTS_VEC32_LEVEL_LDS_LOAD) &&
             (eventId <= SQ_PERF_SEL_INSTS_VEC32_LEVEL_LDS_STORE))
    {
        isLevelEvent = true;
    }
    else if (eventId == SQ_PERF_SEL_INSTS_VEC32_LEVEL_LDS)
    {
        isLevelEvent = true;
    }
    else if (eventId == SQ_PERF_SEL_INSTS_VEC32_LEVEL_LDS_PARAM_DIRECT)
    {
        isLevelEvent = true;
    }
    else if ((eventId >= SQC_PERF_SEL_ICACHE_INFLIGHT_LEVEL) &&
             (eventId <= SQC_PERF_SEL_DCACHE_TC_INFLIGHT_LEVEL))
    {
        isLevelEvent = true;
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
    uint32 firstInstance = 0x38;

    // Compute the HW event select from the DF subblock instance and subblock event ID.
    const uint32 eventSelect = ((firstInstance + subBlockInstance) << 6) | eventId;

    // DF EventSelect fields are 14 bits (in three sections). Verify that our event select can fit.
    PAL_ASSERT(eventSelect <= ((1 << 14) - 1));

    return eventSelect;
}

} // Gfx12
} // Pal
