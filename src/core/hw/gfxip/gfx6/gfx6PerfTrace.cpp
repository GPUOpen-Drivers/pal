/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx6/gfx6PerfTrace.h"
#include "core/hw/gfxip/gfx6/gfx6PerfCounter.h"
#include "palDequeImpl.h"

#include "core/hw/amdgpu_asic.h"

namespace Pal
{
namespace Gfx6
{

// =====================================================================================================================
// SpmTrace constuctor for gfx6 hardware layer.
SpmTrace::SpmTrace(
    const Device* pDevice)
    :
    Pal::SpmTrace(pDevice->Parent()),
    m_device(*pDevice)
{
    m_ringBaseLo.u32All     = 0;
    m_ringBaseHi.u32All     = 0;
    m_segmentSize.u32All    = 0;

    for (uint32 se = 0; se < static_cast<uint32>(SpmDataSegmentType::Count); ++se)
    {
        m_muxselRamData[se].pMuxselRamUint32 = nullptr;
    }
}

// =====================================================================================================================
// SpmTrace Destructor.
SpmTrace::~SpmTrace()
{
    // Free the muxsel ram data memory if it had been allocated.
    for (uint32 i = 0; i < static_cast<uint32>(SpmDataSegmentType::Count); ++i)
    {
        if (m_muxselRamData[i].pMuxselRamUint32 != nullptr)
        {
            PAL_SAFE_FREE(m_muxselRamData[i].pMuxselRamUint32, m_device.GetPlatform());
        }
    }
}

// =====================================================================================================================
// Initializes some member variables and creates copy of SpmTraceCreateInfo.
Result SpmTrace::Init(
    const SpmTraceCreateInfo& createInfo)
{
    Result result = Result::Success;

    m_ringSize.bits.RING_BASE_SIZE = createInfo.ringSize;

    m_spmPerfmonCntl.u32All = 0;
    m_spmPerfmonCntl.bits.PERFMON_SAMPLE_INTERVAL = static_cast<uint16>(createInfo.spmInterval);

    PAL_ASSERT(m_spmPerfmonCntl.bits.PERFMON_SAMPLE_INTERVAL == createInfo.spmInterval);
    m_numPerfCounters = createInfo.numPerfCounters;

    void* pMem = PAL_MALLOC(createInfo.numPerfCounters * sizeof(PerfCounterInfo),
                            m_device.GetPlatform(),
                            Util::SystemAllocType::AllocInternal);
    if (pMem != nullptr)
    {
        m_pPerfCounterCreateInfos = static_cast<PerfCounterInfo*>(pMem);
        memcpy(m_pPerfCounterCreateInfos,
               createInfo.pPerfCounterInfos,
               createInfo.numPerfCounters * sizeof(PerfCounterInfo));
    }
    else
    {
        result = Result::ErrorOutOfMemory;
    }

    return result;
}

// =====================================================================================================================
// Writes CP_PERFMON_CNTL to disable & reset and then start perf counters. A wait idle is expected to be called prior to
// calling this. A PERFMON_START VGT event is expected to be made by the caller following calling this function.
uint32* SpmTrace::WriteStartCommands(
    Pal::CmdStream* pCmdStream,
    uint32*         pCmdSpace)
{
    CmdStream* pHwlCmdStream = static_cast<CmdStream*>(pCmdStream);
    regCP_PERFMON_CNTL cpPerfmonCntl = {};

    cpPerfmonCntl.u32All                         = 0;
    cpPerfmonCntl.bits.PERFMON_STATE             = CP_PERFMON_STATE_START_COUNTING;
    cpPerfmonCntl.bits.SPM_PERFMON_STATE__CI__VI = CP_PERFMON_STATE_START_COUNTING;
    cpPerfmonCntl.bits.PERFMON_SAMPLE_ENABLE     = 1;

    pCmdSpace = pHwlCmdStream->WriteSetOneConfigReg(mmCP_PERFMON_CNTL__CI__VI,
                                                    cpPerfmonCntl.u32All,
                                                    pCmdSpace);
    return pCmdSpace;
}

// =====================================================================================================================
uint32* SpmTrace::WriteEndCommands(
    Pal::CmdStream* pCmdStream,
    uint32*         pCmdSpace)
{
    CmdStream* pHwlCmdStream = static_cast<CmdStream*>(pCmdStream);

    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_CNTL__CI__VI,
                                                     0,
                                                     pCmdSpace);

    // Write segment size, ring buffer size, ring buffer address registers
    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_SEGMENT_SIZE__CI__VI,
                                                     0,
                                                     pCmdSpace);
    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_RING_SIZE__CI__VI,
                                                     0,
                                                     pCmdSpace);
    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_RING_BASE_LO__CI__VI,
                                                     0,
                                                     pCmdSpace);
    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_RING_BASE_HI__CI__VI,
                                                     0,
                                                     pCmdSpace);

    uint32 muxselRamDwords;
    regGRBM_GFX_INDEX grbmGfxIndex = {};
    grbmGfxIndex.bits.INSTANCE_BROADCAST_WRITES = 1;
    grbmGfxIndex.bits.SH_BROADCAST_WRITES       = 1;

    uint32 muxselAddrReg = mmRLC_SPM_SE_MUXSEL_ADDR__CI__VI;

    // Reset the muxsel addr register.
    for (uint32 seIndex = 0; seIndex < static_cast<uint32>(SpmDataSegmentType::Count); ++seIndex)
    {
        muxselRamDwords = GetMuxselRamDwords(seIndex);

        if (muxselRamDwords != 0)
        {
            grbmGfxIndex.bits.SE_INDEX = seIndex;

            if (seIndex == static_cast<uint32>(SpmDataSegmentType::Global))
            {
                // Global section.
                grbmGfxIndex.bits.SE_INDEX            = 0;
                grbmGfxIndex.bits.SE_BROADCAST_WRITES = 1;
                muxselAddrReg = mmRLC_SPM_GLOBAL_MUXSEL_ADDR__CI__VI;
            }

            pCmdSpace = pHwlCmdStream->WriteSetOneConfigReg(m_device.CmdUtil().GetRegInfo().mmGrbmGfxIndex,
                                                            grbmGfxIndex.u32All,
                                                            pCmdSpace);

            pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(muxselAddrReg, 0, pCmdSpace);
        }
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Writes RLC mux-select data into mux select RAM, programs each perf counter requested for this trace, configures RLC
// with spm trace settings and resets cp_perfmon_cntl. Reserves space for commands as needed.
uint32* SpmTrace::WriteSetupCommands(
    gpusize         baseGpuVirtAddr,    // [in] Address of the overall gpu mem allocation used for trace.
    Pal::CmdStream* pCmdStream,
    uint32*         pCmdSpace)
{
    CmdStream* pHwlCmdStream = static_cast<CmdStream*>(pCmdStream);

    // (1) Write setup commands for each streaming perf counter.
    StreamingPerfCounter* pStreamingCounter = nullptr;
    for (auto iter = m_spmCounters.Begin(); iter.Get(); iter.Next())
    {
        pCmdStream->CommitCommands(pCmdSpace);
        pCmdSpace = pCmdStream->ReserveCommands();

        pStreamingCounter = static_cast<StreamingPerfCounter*>(*iter.Get());

        // We might have to reset the GRBM_GFX_INDEX for programming more counters as it would've been changed for
        // programming indexed counters previously.
        if (m_flags.hasIndexedCounters)
        {
            regGRBM_GFX_INDEX grbmGfxIndex = {};
            grbmGfxIndex.bits.SE_BROADCAST_WRITES       = 1;
            grbmGfxIndex.bits.SH_BROADCAST_WRITES       = 1;
            grbmGfxIndex.bits.INSTANCE_BROADCAST_WRITES = 1;

            pHwlCmdStream->WriteSetOneConfigReg(m_device.CmdUtil().GetRegInfo().mmGrbmGfxIndex,
                                                grbmGfxIndex.u32All,
                                                pCmdSpace);
        }

        pCmdSpace = pStreamingCounter->WriteSetupCommands(pCmdStream, pCmdSpace);
    }

    // (2) Write muxsel ram.
    for (uint32 seIndex = 0; seIndex < static_cast<uint32>(SpmDataSegmentType::Count); ++seIndex)
    {
        const uint32 muxselRamDwords = GetMuxselRamDwords(seIndex);

        // Write commands to write the muxsel ram data only if there is any data to write.
        if (muxselRamDwords != 0)
        {
            if (seIndex != static_cast<uint32>(SpmDataSegmentType::Global))
            {
                // Write the per-SE muxsel ram data.
                regGRBM_GFX_INDEX grbmGfxIndex              = {};
                grbmGfxIndex.bits.SE_INDEX                  = seIndex;
                grbmGfxIndex.bits.SH_BROADCAST_WRITES       = 1;
                grbmGfxIndex.bits.INSTANCE_BROADCAST_WRITES = 1;

                pCmdSpace = pHwlCmdStream->WriteSetOneConfigReg(m_device.CmdUtil().GetRegInfo().mmGrbmGfxIndex,
                                                                grbmGfxIndex.u32All,
                                                                pCmdSpace);

                pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_SE_MUXSEL_ADDR__CI__VI,
                                                                 0,
                                                                 pCmdSpace);

                for (uint32 i = 0; i < muxselRamDwords; ++i)
                {
                    // Depending on the number of counters requested and the SE configuration a large number of
                    // write_data packets can be generated.
                    pCmdStream->CommitCommands(pCmdSpace);
                    pCmdSpace = pCmdStream->ReserveCommands();

                    pCmdSpace += m_device.CmdUtil().BuildWriteData(mmRLC_SPM_SE_MUXSEL_DATA__CI__VI,
                                                                   1,
                                                                   WRITE_DATA_ENGINE_ME,
                                                                   WRITE_DATA_DST_SEL_REGISTER,
                                                                   true, // Wait for write confirmation
                                                                   (m_muxselRamData[seIndex].pMuxselRamUint32 + i),
                                                                   PredDisable,
                                                                   pCmdSpace);
                }
            }
            else
            {
                // Write the global muxsel ram data.
                regGRBM_GFX_INDEX grbmGfxIndex              = {};
                grbmGfxIndex.bits.SE_BROADCAST_WRITES       = 1;
                grbmGfxIndex.bits.SH_BROADCAST_WRITES       = 1;
                grbmGfxIndex.bits.INSTANCE_BROADCAST_WRITES = 1;

                pCmdSpace = pHwlCmdStream->WriteSetOneConfigReg(m_device.CmdUtil().GetRegInfo().mmGrbmGfxIndex,
                                                                grbmGfxIndex.u32All,
                                                                pCmdSpace);

                pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_GLOBAL_MUXSEL_ADDR__CI__VI,
                                                                 0,
                                                                 pCmdSpace);

                for (uint32 i = 0; i < muxselRamDwords; ++i)
                {
                    pCmdStream->CommitCommands(pCmdSpace);
                    pCmdSpace = pCmdStream->ReserveCommands();

                    pCmdSpace += m_device.CmdUtil().BuildWriteData(mmRLC_SPM_GLOBAL_MUXSEL_DATA__CI__VI,
                                                                   1,
                                                                   WRITE_DATA_ENGINE_ME,
                                                                   WRITE_DATA_DST_SEL_REGISTER,
                                                                   1, // Wait for write confirmation
                                                                   (m_muxselRamData[seIndex].pMuxselRamUint32 + i),
                                                                   PredDisable,
                                                                   pCmdSpace);
                }
            }
        }
    }

    // (3) Write the relevant RLC registers
    // Compute the start of the spm trace buffer location.
    const gpusize gpuVirtAddrShifted = (baseGpuVirtAddr + m_dataOffset);

    m_spmPerfmonCntl.bits.PERFMON_RING_MODE = 0;
    m_ringBaseLo.u32All = Util::LowPart(gpuVirtAddrShifted);
    m_ringBaseHi.u32All = Util::HighPart(gpuVirtAddrShifted);

    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_CNTL__CI__VI,
                                                     m_spmPerfmonCntl.u32All,
                                                     pCmdSpace);

    regRLC_SPM_PERFMON_SEGMENT_SIZE__CI__VI spmSegmentSize = {};
    spmSegmentSize.bits.GLOBAL_NUM_LINE      = m_segmentSizes[static_cast<uint32>(SpmDataSegmentType::Global)];
    spmSegmentSize.bits.SE0_NUM_LINE         = m_segmentSizes[static_cast<uint32>(SpmDataSegmentType::Se0)];
    spmSegmentSize.bits.SE1_NUM_LINE         = m_segmentSizes[static_cast<uint32>(SpmDataSegmentType::Se1)];
    spmSegmentSize.bits.SE2_NUM_LINE         = m_segmentSizes[static_cast<uint32>(SpmDataSegmentType::Se2)];
    spmSegmentSize.bits.PERFMON_SEGMENT_SIZE = m_segmentSizes[static_cast<uint32>(SpmDataSegmentType::Count)];

    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_SEGMENT_SIZE__CI__VI,
                                                     spmSegmentSize.u32All,
                                                     pCmdSpace);
    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_RING_BASE_LO__CI__VI,
                                                     m_ringBaseLo.u32All,
                                                     pCmdSpace);
    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_RING_BASE_HI__CI__VI,
                                                     m_ringBaseHi.u32All,
                                                     pCmdSpace);
    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_RING_SIZE__CI__VI,
                                                     m_ringSize.u32All,
                                                     pCmdSpace);

    // We do not use the ringing functionality of the output buffers, so always write 0 as the RDPTR.
    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_RING_RDPTR__CI__VI, 0, pCmdSpace);

    // Finally, disable and reset all counters.
    regCP_PERFMON_CNTL cpPerfmonCntl             = {};
    cpPerfmonCntl.bits.PERFMON_STATE             = CP_PERFMON_STATE_DISABLE_AND_RESET;
    cpPerfmonCntl.bits.SPM_PERFMON_STATE__CI__VI = CP_PERFMON_STATE_DISABLE_AND_RESET;

    pCmdSpace = pHwlCmdStream->WriteSetOneConfigReg(mmCP_PERFMON_CNTL__CI__VI,
                                                    cpPerfmonCntl.u32All,
                                                    pCmdSpace);

    // NOTE: It is the caller's responsibility to reset GRBM_GFX_INDEX.

    return pCmdSpace;
}

// =====================================================================================================================
ThreadTrace::ThreadTrace(
    const Device*        pDevice,    ///< [retained] Associated Device object
    const ThreadTraceInfo& info)      ///< [in] Trace creation info
    :
    Pal::ThreadTrace(pDevice->Parent(), info),
    m_device(*pDevice)
{
    m_sqThreadTraceSize.u32All = 0;

    m_sqThreadTraceMode.u32All = 0;
    m_sqThreadTraceMode.bits.MASK_PS      = 1;
    m_sqThreadTraceMode.bits.MASK_VS      = 1;
    m_sqThreadTraceMode.bits.MASK_GS      = 1;
    m_sqThreadTraceMode.bits.MASK_ES      = 1;
    m_sqThreadTraceMode.bits.MASK_HS      = 1;
    m_sqThreadTraceMode.bits.MASK_LS      = 1;
    m_sqThreadTraceMode.bits.MASK_CS      = 1;
    m_sqThreadTraceMode.bits.AUTOFLUSH_EN = 1;

    m_sqThreadTraceMask.u32All = 0;
    m_sqThreadTraceMask.bits.SIMD_EN     = PerfCtrInfo::SimdMaskAll;
    m_sqThreadTraceMask.bits.VM_ID_MASK  = SQ_THREAD_TRACE_VM_ID_MASK_SINGLE;
    m_sqThreadTraceMask.bits.RANDOM_SEED = PerfCtrInfo::MaximumRandomSeed;

    const GpuChipProperties& chipProps = pDevice->Parent()->ChipProperties();

    if ((chipProps.gfxLevel != GfxIpLevel::GfxIp6) ||
         IsOland(*(pDevice->Parent()))             ||
         IsHainan(*(pDevice->Parent())))
    {
        // On Sea Islands and newer hardware, as wells as Oland and Hainan, we need to pull
        // some register fields for SQ_THREAD_TRACE_MASK from the Adapter.
        regSQ_THREAD_TRACE_MASK sqThreadTraceMask;
        sqThreadTraceMask.u32All = chipProps.gfx6.sqThreadTraceMask;

        m_sqThreadTraceMask.bits.REG_STALL_EN__CI__VI =
                            sqThreadTraceMask.bits.REG_STALL_EN__CI__VI;
        m_sqThreadTraceMask.bits.SQ_STALL_EN__CI__VI =
                            sqThreadTraceMask.bits.SQ_STALL_EN__CI__VI;
        m_sqThreadTraceMask.bits.SPI_STALL_EN__CI__VI =
                            sqThreadTraceMask.bits.SPI_STALL_EN__CI__VI;

        // NOTE: DXX mentions in a comment that for Oland, the driver may need to force
        // SPI_STALL_EN to zero to avoid doubly creating some wavefronts, avoiding a
        // possible hang situation.
    }

    m_sqThreadTraceTokenMask.u32All = 0;
    m_sqThreadTraceTokenMask.bits.TOKEN_MASK = PerfCtrInfo::TokenMaskAll;
    m_sqThreadTraceTokenMask.bits.REG_MASK   = PerfCtrInfo::RegMaskAll;

    m_sqThreadTracePerfMask.u32All = 0;
    m_sqThreadTracePerfMask.bits.SH0_MASK = PerfCtrInfo::ShCuMaskAll;
    m_sqThreadTracePerfMask.bits.SH1_MASK = PerfCtrInfo::ShCuMaskAll;

    m_sqThreadTraceHiWater.u32All = 0;
    m_sqThreadTraceHiWater.bits.HIWATER = PerfCtrInfo::HiWaterDefault;

    // Default to only selecting CUs that aren't reserved for real time queues.
    uint32 cuTraceableCuMask = ~chipProps.gfxip.realTimeCuMask;

    // Find intersection between non-realtime and active queues
    if (chipProps.gfxLevel == GfxIpLevel::GfxIp6)
    {
        // If gfx6, default to first SH on the current shader engine
        cuTraceableCuMask &= chipProps.gfx6.activeCuMaskGfx6[m_shaderEngine][0];
    }
    else
    {
        cuTraceableCuMask &= chipProps.gfx6.activeCuMaskGfx7[m_shaderEngine];
    }

    // If it exists, select the first available CU from the mask
    uint32 firstActiveCu = 0;
    if (Util::BitMaskScanForward(&firstActiveCu, cuTraceableCuMask))
    {
        m_sqThreadTraceMask.bits.CU_SEL = firstActiveCu;
    }

    SetOptions(info);
}

// =====================================================================================================================
// Initializes one of the thread-trace creation options.
void ThreadTrace::SetOptions(
    const ThreadTraceInfo& info)
{
    const auto& flags  = info.optionFlags;
    const auto& values = info.optionValues;

    const size_t bufferSize = (flags.bufferSize) ? values.bufferSize : PerfCtrInfo::DefaultBufferSize;

    m_sqThreadTraceSize.bits.SIZE = (bufferSize >> PerfCtrInfo::BufferAlignShift);

    // Need to update our buffer-size parameter.
    m_dataSize = bufferSize;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 451
    if (flags.threadTraceTokenConfig)
    {
#else
    if (flags.threadTraceTokenMask || flags.threadTraceRegMask)
    {
        const ThreadTraceTokenConfig tokenConfig = { values.threadTraceTokenMask, values.threadTraceRegMask };
#endif
        SqttTokenMask tokenMask = {};
        SqttRegMask   regMask   = {};
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 451
        GetHwTokenConfig(values.threadTraceTokenConfig, &tokenMask, &regMask);
#else
        GetHwTokenConfig(tokenConfig, &tokenMask, &regMask);
#endif

        m_sqThreadTraceTokenMask.bits.TOKEN_MASK = tokenMask.u16All;
        m_sqThreadTraceTokenMask.bits.REG_MASK   = regMask.u8All;
    }

    if (flags.threadTraceTargetSh)
    {
        m_sqThreadTraceMask.bits.SH_SEL = values.threadTraceTargetSh;
    }

    if (flags.threadTraceTargetCu)
    {
        m_sqThreadTraceMask.bits.CU_SEL = values.threadTraceTargetCu;
    }

    if (flags.threadTraceSh0CounterMask)
    {
        m_sqThreadTracePerfMask.bits.SH0_MASK = values.threadTraceSh0CounterMask;
    }

    if (flags.threadTraceSh1CounterMask)
    {
        m_sqThreadTracePerfMask.bits.SH1_MASK = values.threadTraceSh1CounterMask;
    }

    if (flags.threadTraceSimdMask)
    {
        m_sqThreadTraceMask.bits.SIMD_EN = values.threadTraceSimdMask;
    }

    if (flags.threadTraceVmIdMask)
    {
        m_sqThreadTraceMask.bits.VM_ID_MASK = values.threadTraceVmIdMask;
    }

    if (flags.threadTraceRandomSeed)
    {
        m_sqThreadTraceMask.bits.RANDOM_SEED = values.threadTraceRandomSeed;
    }

    if (flags.threadTraceShaderTypeMask)
    {
        m_sqThreadTraceMode.bits.MASK_PS = (values.threadTraceShaderTypeMask & PerfShaderMaskPs) ? 1 : 0;
        m_sqThreadTraceMode.bits.MASK_VS = (values.threadTraceShaderTypeMask & PerfShaderMaskVs) ? 1 : 0;
        m_sqThreadTraceMode.bits.MASK_GS = (values.threadTraceShaderTypeMask & PerfShaderMaskGs) ? 1 : 0;
        m_sqThreadTraceMode.bits.MASK_ES = (values.threadTraceShaderTypeMask & PerfShaderMaskEs) ? 1 : 0;
        m_sqThreadTraceMode.bits.MASK_HS = (values.threadTraceShaderTypeMask & PerfShaderMaskHs) ? 1 : 0;
        m_sqThreadTraceMode.bits.MASK_LS = (values.threadTraceShaderTypeMask & PerfShaderMaskLs) ? 1 : 0;
        m_sqThreadTraceMode.bits.MASK_CS = (values.threadTraceShaderTypeMask & PerfShaderMaskCs) ? 1 : 0;
    }

    if (flags.threadTraceIssueMask)
    {
        m_sqThreadTraceMode.bits.ISSUE_MASK = values.threadTraceIssueMask;
    }

    if (flags.threadTraceWrapBuffer)
    {
        m_sqThreadTraceMode.bits.WRAP = values.threadTraceWrapBuffer;
    }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 422
    if (flags.threadTraceStallBehavior && (m_sqThreadTraceMask.bits.SQ_STALL_EN__CI__VI == 1))
    {
        // only override if kernel reports we're actually able to stall
        switch (values.threadTraceStallBehavior)
        {
        case GpuProfilerStallAlways:
            // stick with default, always stall when full
            break;
        case GpuProfilerStallLoseDetail:
            // On stall, lose instruction detail until we read enough
            // This results in about 30% less stalls while still *very* unlikely to drop packets.
            m_sqThreadTraceTokenMask.bits.REG_DROP_ON_STALL__CI__VI = 1;
            m_sqThreadTraceMask.bits.REG_STALL_EN__CI__VI           = 0;
            break;
        case GpuProfilerStallNever:
            // disable stalling entirely. Be prepared for packet loss.
            m_sqThreadTraceMask.bits.REG_STALL_EN__CI__VI           = 0;
            m_sqThreadTraceMask.bits.SQ_STALL_EN__CI__VI            = 0;
            m_sqThreadTraceMask.bits.SPI_STALL_EN__CI__VI           = 0;
            break;
        default:
            PAL_NEVER_CALLED();
            break;
        }
    }
#endif
}

// =====================================================================================================================
// Issues commands to set-up the GRBM_GFX_INDEX register to write to only the Shader Engine and Shader Array that this
// trace is associated with. Returns the next unused DWORD in pCmdSpace.
uint32* ThreadTrace::WriteGrbmGfxIndex(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    regGRBM_GFX_INDEX grbmGfxIndex = {};
    grbmGfxIndex.bits.SE_INDEX                  = m_shaderEngine;
    grbmGfxIndex.bits.SH_INDEX                  = m_sqThreadTraceMask.bits.SH_SEL;
    grbmGfxIndex.bits.INSTANCE_BROADCAST_WRITES = 1;

    return pCmdStream->WriteSetOneConfigReg(m_device.CmdUtil().GetRegInfo().mmGrbmGfxIndex,
                                            grbmGfxIndex.u32All,
                                            pCmdSpace);
}

// =====================================================================================================================
// Issues the PM4 commands necessary to setup this thread trace. Returns the next unused DWORD in pCmdSpace.
uint32* ThreadTrace::WriteSetupCommands(
    gpusize    baseGpuVirtAddr, ///< Base GPU virtual address of the owning Experiment
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    const auto& regInfo = m_device.CmdUtil().GetRegInfo();

    // Set GRBM_GFX_INDEX to isolate the SE/SH this trace is associated with.
    pCmdSpace = WriteGrbmGfxIndex(pCmdStream, pCmdSpace);

    // Compute the base address of the thread trace data, including the shift amount the
    // register expects.
    const gpusize gpuVirtAddrShifted = (baseGpuVirtAddr + m_dataOffset) >> PerfCtrInfo::BufferAlignShift;

    // Write the base address of the thread trace buffer.
    regSQ_THREAD_TRACE_BASE sqThreadTraceBase = {};
    sqThreadTraceBase.bits.ADDR = Util::LowPart(gpuVirtAddrShifted);

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regInfo.mmSqThreadTraceBase,
                                                  sqThreadTraceBase.u32All,
                                                  pCmdSpace);

    // Write the perf counter registers which control the thread trace properties.
    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regInfo.mmSqThreadTraceSize,
                                                  m_sqThreadTraceSize.u32All,
                                                  pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regInfo.mmSqThreadTraceMask,
                                                  m_sqThreadTraceMask.u32All,
                                                  pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regInfo.mmSqThreadTraceTokenMask,
                                                  m_sqThreadTraceTokenMask.u32All,
                                                  pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regInfo.mmSqThreadTracePerfMask,
                                                  m_sqThreadTracePerfMask.u32All,
                                                  pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regInfo.mmSqThreadTraceHiWater,
                                                  m_sqThreadTraceHiWater.u32All,
                                                  pCmdSpace);

    // NOTE: It is the caller's responsibility to reset GRBM_GFX_INDEX.

    return pCmdSpace;
}

// =====================================================================================================================
// Writes the commands required to update the sqtt token mask.
uint32* ThreadTrace::WriteUpdateSqttTokenMaskCommands(
    CmdStream*                    pCmdStream,
    uint32*                       pCmdSpace,
    const ThreadTraceTokenConfig& sqttTokenConfig
    ) const
{
    const auto& regInfo = m_device.CmdUtil().GetRegInfo();

    // Set GRBM_GFX_INDEX to isolate the SE/SH this trace is associated with.
    pCmdSpace = WriteGrbmGfxIndex(pCmdStream, pCmdSpace);

    SqttTokenMask tokenMask = {};
    SqttRegMask   regMask   = {};
    GetHwTokenConfig(sqttTokenConfig, &tokenMask, &regMask);

    // Update the token mask register
    regSQ_THREAD_TRACE_TOKEN_MASK tokenMaskReg = m_sqThreadTraceTokenMask;
    tokenMaskReg.bits.TOKEN_MASK               = tokenMask.u16All;
    tokenMaskReg.bits.REG_MASK                 = regMask.u8All;

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regInfo.mmSqThreadTraceTokenMask,
                                                  tokenMaskReg.u32All,
                                                  pCmdSpace);

    // NOTE: It is the caller's responsibility to reset GRBM_GFX_INDEX.

    return pCmdSpace;
}

// =====================================================================================================================
// Issues the PM4 commands necessary to start this thread trace. The owning Experiment object should have issued and
// idle before calling this. Returns the next unused DWORD in pCmdSpace.
uint32* ThreadTrace::WriteStartCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    const auto& regInfo = m_device.CmdUtil().GetRegInfo();

    // Set GRBM_GFX_INDEX to isolate the SE/SH this trace is associated with.
    pCmdSpace = WriteGrbmGfxIndex(pCmdStream, pCmdSpace);

    // Write SQ_THREAD_TRACE_CTRL with the reset_buffer flag set to instruct the hardware to
    // reset the trace buffer.
    regSQ_THREAD_TRACE_CTRL sqThreadTraceCtrl = {};
    sqThreadTraceCtrl.bits.RESET_BUFFER = 1;

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regInfo.mmSqThreadTraceCtrl,
                                                  sqThreadTraceCtrl.u32All,
                                                  pCmdSpace);

    // Write SQ_THREAD_TRACE_MODE with the mode field set to "on" to enable the trace.
    regSQ_THREAD_TRACE_MODE sqThreadTraceMode;
    sqThreadTraceMode.u32All    = m_sqThreadTraceMode.u32All;
    sqThreadTraceMode.bits.MODE = SQ_THREAD_TRACE_MODE_ON;

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regInfo.mmSqThreadTraceMode,
                                                  sqThreadTraceMode.u32All,
                                                  pCmdSpace);

    // NOTE: It is the caller's responsibility to reset GRBM_GFX_INDEX.

    return pCmdSpace;
}

// =====================================================================================================================
// Issues the PM4 commands necessary to stop this thread trace, and populate the parent experiment's GPU memory with the
// appropriate ThreadTraceInfoData contents. Returns the next unused DWORD in pCmdSpace.
uint32* ThreadTrace::WriteStopCommands(
    gpusize    baseGpuVirtAddr, ///< Base GPU virtual address of the owning Experiment
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    const auto& cmdUtil = m_device.CmdUtil();
    const auto& regInfo = cmdUtil.GetRegInfo();

    // Set GRBM_GFX_INDEX to isolate the SE/SH this trace is associated with.
    pCmdSpace = WriteGrbmGfxIndex(pCmdStream, pCmdSpace);

    // Write SQ_THREAD_TRACE_MODE with the mode field set to "off" to disable the trace.
    regSQ_THREAD_TRACE_MODE sqThreadTraceMode;
    sqThreadTraceMode.u32All    = m_sqThreadTraceMode.u32All;
    sqThreadTraceMode.bits.MODE = SQ_THREAD_TRACE_MODE_OFF;

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regInfo.mmSqThreadTraceMode,
                                                  sqThreadTraceMode.u32All,
                                                  pCmdSpace);

    // Flush the thread trace buffer to memory.
    pCmdSpace += cmdUtil.BuildEventWrite(THREAD_TRACE_FLUSH, pCmdSpace);

    // Poll the status register's busy bit to ensure that no events are being logged and written to memory.
    pCmdSpace += cmdUtil.BuildWaitRegMem(WAIT_REG_MEM_SPACE_REGISTER,
                                         WAIT_REG_MEM_FUNC_NOT_EQUAL,
                                         WAIT_REG_MEM_ENGINE_ME,
                                         cmdUtil.GetRegInfo().mmSqThreadTraceStatus,
                                         0x1,
                                         SQ_THREAD_TRACE_STATUS__BUSY_MASK,
                                         false,
                                         pCmdSpace);

    // The following code which issues the COPY_DATA commands assumes that the layout of the ThreadTraceInfoData
    // structure is ordered a particular way. Compile-time asserts help us help us guarantee the assumption.
    static_assert(offsetof(ThreadTraceInfoData, curOffset) == 0, "");
    static_assert(offsetof(ThreadTraceInfoData, traceStatus)  == sizeof(uint32), "");
    static_assert(offsetof(ThreadTraceInfoData, writeCounter) == (sizeof(uint32) * 2), "");

    // Compute the base address of the thread trace info segment.
    const gpusize gpuVirtAddr = (baseGpuVirtAddr + m_infoOffset);

    // Issue a trio of COPY_DATA commands to populate the ThreadTraceInfoData for this
    // thread trace:

    pCmdSpace += cmdUtil.BuildCopyData(COPY_DATA_SEL_DST_ASYNC_MEMORY,
                                       gpuVirtAddr,
                                       COPY_DATA_SEL_SRC_SYS_PERF_COUNTER,
                                       regInfo.mmSqThreadTraceWptr,
                                       COPY_DATA_SEL_COUNT_1DW,
                                       COPY_DATA_ENGINE_ME,
                                       COPY_DATA_WR_CONFIRM_WAIT,
                                       pCmdSpace);

    pCmdSpace += cmdUtil.BuildCopyData(COPY_DATA_SEL_DST_ASYNC_MEMORY,
                                       gpuVirtAddr + sizeof(uint32),
                                       COPY_DATA_SEL_SRC_SYS_PERF_COUNTER,
                                       regInfo.mmSqThreadTraceStatus,
                                       COPY_DATA_SEL_COUNT_1DW,
                                       COPY_DATA_ENGINE_ME,
                                       COPY_DATA_WR_CONFIRM_WAIT,
                                       pCmdSpace);

    pCmdSpace += cmdUtil.BuildCopyData(COPY_DATA_SEL_DST_ASYNC_MEMORY,
                                       gpuVirtAddr + (sizeof(uint32) * 2),
                                       COPY_DATA_SEL_SRC_SYS_PERF_COUNTER,
                                       mmSQ_THREAD_TRACE_CNTR,
                                       COPY_DATA_SEL_COUNT_1DW,
                                       COPY_DATA_ENGINE_ME,
                                       COPY_DATA_WR_CONFIRM_WAIT,
                                       pCmdSpace);

    // NOTE: It is the caller's responsibility to reset GRBM_GFX_INDEX.

    return pCmdSpace;
}

// =====================================================================================================================
// Converts the thread trace token config to the HW format for programming the SQTT_TOKEN_MASK register.
void ThreadTrace::GetHwTokenConfig(
    const ThreadTraceTokenConfig& tokenConfig, // [in] The input token config.
    SqttTokenMask*                pTokenMask,  // [out] The token mask in HW format.
    SqttRegMask*                  pRegMask     // [out] The reg mask in HW format.
    ) const
{
    const auto& configTokens  = tokenConfig.tokenMask;
    const auto& configRegMask = tokenConfig.regMask;

    PAL_ASSERT((pTokenMask != nullptr) && (pRegMask != nullptr));

    if (configTokens == ThreadTraceTokenTypeFlags::All)
    {
        // Enable all token types except Perf.
        pTokenMask->u16All = 0xBFFF;
    }
    else
    {
        // Perf counter gathering in thread trace is not supported currently.
        PAL_ALERT(Util::TestAnyFlagSet(configTokens, ThreadTraceTokenTypeFlags::Perf));

        pTokenMask->misc         = Util::TestAnyFlagSet(configTokens, ThreadTraceTokenTypeFlags::Misc         );
        pTokenMask->timestamp    = Util::TestAnyFlagSet(configTokens, ThreadTraceTokenTypeFlags::Timestamp    );
        pTokenMask->reg          = Util::TestAnyFlagSet(configTokens, ThreadTraceTokenTypeFlags::Reg          );
        pTokenMask->waveStart    = Util::TestAnyFlagSet(configTokens, ThreadTraceTokenTypeFlags::WaveStart    );
        pTokenMask->waveAlloc    = Util::TestAnyFlagSet(configTokens, ThreadTraceTokenTypeFlags::WaveAlloc    );
        pTokenMask->regCsPriv    = Util::TestAnyFlagSet(configTokens, ThreadTraceTokenTypeFlags::RegCsPriv    );
        pTokenMask->waveEnd      = Util::TestAnyFlagSet(configTokens, ThreadTraceTokenTypeFlags::WaveEnd      );
        pTokenMask->event        = Util::TestAnyFlagSet(configTokens, ThreadTraceTokenTypeFlags::Event        );
        pTokenMask->eventCs      = Util::TestAnyFlagSet(configTokens, ThreadTraceTokenTypeFlags::EventCs      );
        pTokenMask->eventGfx1    = Util::TestAnyFlagSet(configTokens, ThreadTraceTokenTypeFlags::EventGfx1    );
        pTokenMask->inst         = Util::TestAnyFlagSet(configTokens, ThreadTraceTokenTypeFlags::Inst         );
        pTokenMask->instPc       = Util::TestAnyFlagSet(configTokens, ThreadTraceTokenTypeFlags::InstPc       );
        pTokenMask->instUserData = Util::TestAnyFlagSet(configTokens, ThreadTraceTokenTypeFlags::InstUserData );
        pTokenMask->issue        = Util::TestAnyFlagSet(configTokens, ThreadTraceTokenTypeFlags::Issue        );
        pTokenMask->regCs        = Util::TestAnyFlagSet(configTokens, ThreadTraceTokenTypeFlags::RegCs        );
    }

    // There is no option to choose between register reads and writes in TT2.1, so we enable all register ops.
    const bool allRegs = Util::TestAllFlagsSet(configRegMask, ThreadTraceRegTypeFlags::AllRegWrites) ||
                         Util::TestAllFlagsSet(configRegMask, ThreadTraceRegTypeFlags::AllRegReads)  ||
                         Util::TestAllFlagsSet(configRegMask, ThreadTraceRegTypeFlags::AllReadsAndWrites);

    if (allRegs)
    {
        //Note: According to the thread trace programming guide, the "other" bit must always be set to 0.
        //      However, this should be safe so long as stable 'profiling' clocks are enabled
        pRegMask->u8All = 0xFF;
    }
    else
    {
        pRegMask->eventInitiator         = Util::TestAnyFlagSet(configRegMask, ThreadTraceRegTypeFlags::EventRegs);
        pRegMask->drawInitiator          = Util::TestAnyFlagSet(configRegMask, ThreadTraceRegTypeFlags::DrawRegs);
        pRegMask->dispatchInitiator      = Util::TestAnyFlagSet(configRegMask, ThreadTraceRegTypeFlags::DispatchRegs);
        pRegMask->userData               = Util::TestAnyFlagSet(configRegMask, ThreadTraceRegTypeFlags::UserdataRegs);
        pRegMask->gfxdec                 = Util::TestAnyFlagSet(configRegMask,
                                                                ThreadTraceRegTypeFlags::GraphicsContextRegs);
        pRegMask->shdec                  = Util::TestAnyFlagSet(configRegMask,
                                                                ThreadTraceRegTypeFlags::ShaderLaunchStateRegs);
        pRegMask->ttMarkerEventInitiator = Util::TestAnyFlagSet(configRegMask, ThreadTraceRegTypeFlags::MarkerRegs);
        pRegMask->other                  = Util::TestAnyFlagSet(configRegMask,
                                                                ThreadTraceRegTypeFlags::OtherConfigRegs);
    }
}

} // Gfx6
} // Pal
