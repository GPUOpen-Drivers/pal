/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/gfx9PerfCounter.h"
#include "core/hw/gfxip/gfx9/gfx9PerfTrace.h"
#include "palDequeImpl.h"

#include "core/hw/amdgpu_asic.h"

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
Gfx9SpmTrace::Gfx9SpmTrace(
    const Device* pDevice)
    :
    Pal::SpmTrace(pDevice->Parent()),
    m_device(*pDevice)
{
    m_ringBaseLo.u32All     = 0;
    m_ringBaseHi.u32All     = 0;
}

// =====================================================================================================================
// Initializes some member variables and creates copy of SpmTraceCreateInfo.
Result Gfx9SpmTrace::Init(
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
// Issues the PM4 commands necessary to start this thread trace. The owning Experiment object should have issued and
// idle before calling this. Returns the next unused DWORD in pCmdSpace.
uint32* Gfx9SpmTrace::WriteSetupCommands(
    gpusize         ringBaseAddr,
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
            grbmGfxIndex.gfx09.SH_BROADCAST_WRITES      = 1;
            grbmGfxIndex.bits.INSTANCE_BROADCAST_WRITES = 1;

            pHwlCmdStream->WriteSetOnePerfCtrReg(m_device.CmdUtil().GetRegInfo().mmGrbmGfxIndex,
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
                grbmGfxIndex.gfx09.SH_BROADCAST_WRITES      = 1;
                grbmGfxIndex.bits.INSTANCE_BROADCAST_WRITES = 1;

                pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(m_device.CmdUtil().GetRegInfo().mmGrbmGfxIndex,
                                                                 grbmGfxIndex.u32All,
                                                                 pCmdSpace);

                pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(Gfx09::mmRLC_SPM_SE_MUXSEL_ADDR,
                                                                 0,
                                                                 pCmdSpace);

                for (uint32 i = 0; i < muxselRamDwords; ++i)
                {
                    // Depending on the number of counters requested and the SE configuration a large number of
                    // write_data packets can be generated.
                    pCmdStream->CommitCommands(pCmdSpace);
                    pCmdSpace = pCmdStream->ReserveCommands();

                    pCmdSpace += m_device.CmdUtil().BuildWriteData(pCmdStream->GetEngineType(),
                                                                   Gfx09::mmRLC_SPM_SE_MUXSEL_DATA,
                                                                   1,
                                                                   engine_sel__me_write_data__micro_engine,
                                                                   dst_sel__me_write_data__mem_mapped_register,
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
                grbmGfxIndex.gfx09.SH_BROADCAST_WRITES      = 1;
                grbmGfxIndex.bits.INSTANCE_BROADCAST_WRITES = 1;

                pCmdSpace = pHwlCmdStream->WriteSetOneConfigReg(m_device.CmdUtil().GetRegInfo().mmGrbmGfxIndex,
                                                                grbmGfxIndex.u32All,
                                                                pCmdSpace);

                pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(Gfx09::mmRLC_SPM_GLOBAL_MUXSEL_ADDR,
                                                                 0,
                                                                 pCmdSpace);

                for (uint32 i = 0; i < muxselRamDwords; ++i)
                {
                    pCmdStream->CommitCommands(pCmdSpace);
                    pCmdSpace = pCmdStream->ReserveCommands();

                    pCmdSpace += m_device.CmdUtil().BuildWriteData(pCmdStream->GetEngineType(),
                                                                   Gfx09::mmRLC_SPM_GLOBAL_MUXSEL_DATA,
                                                                   1,
                                                                   engine_sel__me_write_data__micro_engine,
                                                                   dst_sel__me_write_data__mem_mapped_register,
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
    const gpusize gpuVirtAddrShifted = (ringBaseAddr + m_dataOffset);

    m_spmPerfmonCntl.bits.PERFMON_RING_MODE = 0;
    m_ringBaseLo.u32All = Util::LowPart(gpuVirtAddrShifted);
    m_ringBaseHi.u32All = Util::HighPart(gpuVirtAddrShifted);

    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_CNTL,
                                                     m_spmPerfmonCntl.u32All,
                                                     pCmdSpace);
    regRLC_SPM_PERFMON_SEGMENT_SIZE spmSegmentSize = {};
    spmSegmentSize.bits.GLOBAL_NUM_LINE      = m_segmentSizes[static_cast<uint32>(SpmDataSegmentType::Global)];
    spmSegmentSize.bits.SE0_NUM_LINE         = m_segmentSizes[static_cast<uint32>(SpmDataSegmentType::Se0)];
    spmSegmentSize.bits.SE1_NUM_LINE         = m_segmentSizes[static_cast<uint32>(SpmDataSegmentType::Se1)];
    spmSegmentSize.bits.SE2_NUM_LINE         = m_segmentSizes[static_cast<uint32>(SpmDataSegmentType::Se2)];
    spmSegmentSize.bits.PERFMON_SEGMENT_SIZE = m_segmentSizes[static_cast<uint32>(SpmDataSegmentType::Count)];

    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_SEGMENT_SIZE,
                                                     spmSegmentSize.u32All,
                                                     pCmdSpace);
    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_RING_BASE_LO,
                                                     m_ringBaseLo.u32All,
                                                     pCmdSpace);
    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_RING_BASE_HI,
                                                     m_ringBaseHi.u32All,
                                                     pCmdSpace);
    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_RING_SIZE,
                                                     m_ringSize.u32All,
                                                     pCmdSpace);

    // We do not use the ringing functionality of the output buffers, so always write 0 as the RDPTR.
    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(Gfx09::mmRLC_SPM_RING_RDPTR, 0, pCmdSpace);

    // Finally, disable and reset all counters.
    regCP_PERFMON_CNTL cpPerfmonCntl     = { };
    cpPerfmonCntl.bits.PERFMON_STATE     = CP_PERFMON_STATE_DISABLE_AND_RESET;
    cpPerfmonCntl.bits.SPM_PERFMON_STATE = CP_PERFMON_STATE_DISABLE_AND_RESET;

    pCmdSpace = pHwlCmdStream->WriteSetOneConfigReg(mmCP_PERFMON_CNTL,
                                                    cpPerfmonCntl.u32All,
                                                    pCmdSpace);

    // NOTE: It is the caller's responsibility to reset GRBM_GFX_INDEX.

    return pCmdSpace;
}

// =====================================================================================================================
uint32* Gfx9SpmTrace::WriteStartCommands(
    Pal::CmdStream* pCmdStream,
    uint32*         pCmdSpace)
{
    CmdStream* pHwlCmdStream = static_cast<CmdStream*>(pCmdStream);

    regCP_PERFMON_CNTL cpPerfmonCntl         = { };
    cpPerfmonCntl.bits.PERFMON_STATE         = CP_PERFMON_STATE_START_COUNTING;
    cpPerfmonCntl.bits.SPM_PERFMON_STATE     = CP_PERFMON_STATE_START_COUNTING;
    cpPerfmonCntl.bits.PERFMON_SAMPLE_ENABLE = 1;

    pCmdSpace = pHwlCmdStream->WriteSetOneConfigReg(mmCP_PERFMON_CNTL,
                                                    cpPerfmonCntl.u32All,
                                                    pCmdSpace);
    return pCmdSpace;
}

// =====================================================================================================================
uint32* Gfx9SpmTrace::WriteEndCommands(
    Pal::CmdStream* pCmdStream,
    uint32* pCmdSpace)
{
    CmdStream* pHwlCmdStream = static_cast<CmdStream*>(pCmdStream);

    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_CNTL,
                                                     0,
                                                     pCmdSpace);

    // Write segment size, ring buffer size, ring buffer address registers
    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_SEGMENT_SIZE,
                                                     0,
                                                     pCmdSpace);
    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_RING_SIZE,
                                                     0,
                                                     pCmdSpace);
    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_RING_BASE_LO,
                                                     0,
                                                     pCmdSpace);
    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_RING_BASE_HI,
                                                     0,
                                                     pCmdSpace);

    uint32 muxselRamDwords;
    regGRBM_GFX_INDEX grbmGfxIndex = { };
    grbmGfxIndex.bits.INSTANCE_BROADCAST_WRITES = 1;
    grbmGfxIndex.gfx09.SH_BROADCAST_WRITES      = 1;

    uint32 muxselAddrReg = Gfx09::mmRLC_SPM_SE_MUXSEL_ADDR;

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
                muxselAddrReg = Gfx09::mmRLC_SPM_GLOBAL_MUXSEL_ADDR;
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
ThreadTrace::ThreadTrace(
    const Device*        pDevice,   ///< [retained] Associated Device object
    const ThreadTraceInfo& info)    ///< [in] Trace creation info
    :
    Pal::ThreadTrace(pDevice->Parent(), info),
    m_device(*pDevice),
    m_info(info)
{
    const auto& flags  = m_info.optionFlags;
    const auto& values = m_info.optionValues;

    m_dataSize = (flags.bufferSize) ? values.bufferSize : PerfCtrInfo::DefaultBufferSize;
}

// =====================================================================================================================
// Issues the PM4 commands necessary to insert a thread trace marker. Returns the next unused DWORD in pCmdSpace.
uint32* ThreadTrace::WriteInsertMarker(
    PerfTraceMarkerType markerType, ///< Trace marker type
    uint32              data,       ///< Trace marker data payload
    CmdStream*          pCmdStream,
    uint32*             pCmdSpace
    ) const
{

    uint32 userDataRegAddr = 0;
    switch (markerType)
    {
    case PerfTraceMarkerType::A:
        userDataRegAddr = m_device.CmdUtil().GetRegInfo().mmSqThreadTraceUserData2;
        break;

    case PerfTraceMarkerType::B:
        userDataRegAddr = m_device.CmdUtil().GetRegInfo().mmSqThreadTraceUserData3;
        break;

    default:
        break;
    }

    // If this assert fires, we forgot to add a thread trace marker type to this method!
    PAL_ASSERT(userDataRegAddr != 0);

    // Writing the SQ_THREAD_TRACE_USERDATA_* register will cause the thread trace to insert
    // a user-data event with value of the register.
    return pCmdStream->WriteSetOnePerfCtrReg(userDataRegAddr, data, pCmdSpace);
}

// =====================================================================================================================
Gfx9ThreadTrace::Gfx9ThreadTrace(
    const Device*         pDevice,
    const ThreadTraceInfo&  info)
    :
    ThreadTrace(pDevice, info)
{
    m_sqThreadTraceSize.u32All    = 0;
    m_sqThreadTraceSize.bits.SIZE = (m_dataSize >> PerfCtrInfo::BufferAlignShift);

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
    m_sqThreadTraceMask.gfx09.SIMD_EN     = SimdMaskAll;
    m_sqThreadTraceMask.gfx09.VM_ID_MASK  = SQ_THREAD_TRACE_VM_ID_MASK_SINGLE;

    const GpuChipProperties& chipProps = pDevice->Parent()->ChipProperties();

    m_sqThreadTraceMask.gfx09.REG_STALL_EN = 1;
    m_sqThreadTraceMask.gfx09.SQ_STALL_EN  = 1;
    m_sqThreadTraceMask.gfx09.SPI_STALL_EN = 1;

    m_sqThreadTraceTokenMask.u32All = 0;
    m_sqThreadTraceTokenMask.gfx09.TOKEN_MASK = TokenMaskAll;
    m_sqThreadTraceTokenMask.gfx09.REG_MASK   = RegMaskAll;

    m_sqThreadTracePerfMask.u32All = 0;
    m_sqThreadTracePerfMask.bits.SH0_MASK = ShCuMaskAll;
    m_sqThreadTracePerfMask.bits.SH1_MASK = ShCuMaskAll;

    // Set location within fifo to stall at
    m_sqThreadTraceHiWater.u32All = 0;
    m_sqThreadTraceHiWater.bits.HIWATER = HiWaterDefault;

    // Default to only selecting CUs that are active and not reserved for realtime use.  GFX9 only has one
    // shader array.
    PAL_ASSERT(chipProps.gfx9.numShaderArrays == 1);
    const uint32 cuTraceableCuMask = chipProps.gfx9.activeCuMask[0][m_shaderEngine] & ~chipProps.gfxip.realTimeCuMask;

    // If it exists, select the first available CU from the mask
    uint32 firstActiveCu = 0;
    if (Util::BitMaskScanForward(&firstActiveCu, cuTraceableCuMask))
    {
        m_sqThreadTraceMask.gfx09.CU_SEL = firstActiveCu;
    }

    SetOptions();
}

// =====================================================================================================================
// Initializes one of the thread-trace creation options.
void Gfx9ThreadTrace::SetOptions()
{
    const auto& flags  = m_info.optionFlags;
    const auto& values = m_info.optionValues;

    if (flags.threadTraceTokenMask)
    {
        m_sqThreadTraceTokenMask.gfx09.TOKEN_MASK = values.threadTraceTokenMask;
    }

    if (flags.threadTraceRegMask)
    {
        m_sqThreadTraceTokenMask.gfx09.REG_MASK = values.threadTraceRegMask;
    }

    if (flags.threadTraceTargetSh)
    {
        m_sqThreadTraceMask.gfx09.SH_SEL = values.threadTraceTargetSh;
    }

    if (flags.threadTraceTargetCu)
    {
        m_sqThreadTraceMask.gfx09.CU_SEL = values.threadTraceTargetCu;
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
        m_sqThreadTraceMask.gfx09.SIMD_EN = values.threadTraceSimdMask;
    }

    if (flags.threadTraceVmIdMask)
    {
        m_sqThreadTraceMask.gfx09.VM_ID_MASK = values.threadTraceVmIdMask;
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
    if (flags.threadTraceStallBehavior)
    {
        switch (values.threadTraceStallBehavior)
        {
        case GpuProfilerStallAlways:
            // stick with default, always stall when full
            break;
        case GpuProfilerStallLoseDetail:
            // On stall, lose instruction detail until we read enough
            // This results in about 30% less stalls while still *very* unlikely to drop packets.
            m_sqThreadTraceTokenMask.gfx09.REG_DROP_ON_STALL = 1;
            m_sqThreadTraceMask.gfx09.REG_STALL_EN           = 0;
            break;
        case GpuProfilerStallNever:
            // disable stalling entirely. Be prepared for packet loss.
            m_sqThreadTraceMask.gfx09.REG_STALL_EN = 0;
            m_sqThreadTraceMask.gfx09.SQ_STALL_EN  = 0;
            m_sqThreadTraceMask.gfx09.SPI_STALL_EN = 0;
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
uint32* Gfx9ThreadTrace::WriteGrbmGfxIndex(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    regGRBM_GFX_INDEX grbmGfxIndex = {};
    grbmGfxIndex.bits.SE_INDEX                  = m_shaderEngine;
    grbmGfxIndex.gfx09.SH_INDEX                 = m_sqThreadTraceMask.gfx09.SH_SEL;
    grbmGfxIndex.bits.INSTANCE_BROADCAST_WRITES = 1;

    return pCmdStream->WriteSetOneConfigReg(m_device.CmdUtil().GetRegInfo().mmGrbmGfxIndex,
                                            grbmGfxIndex.u32All,
                                            pCmdSpace);
}

// =====================================================================================================================
// Issues the PM4 commands necessary to setup this thread trace. Returns the next unused DWORD in pCmdSpace.
uint32* Gfx9ThreadTrace::WriteSetupCommands(
    gpusize    baseGpuVirtAddr, ///< Base GPU virtual address of the owning Experiment
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    // Set GRBM_GFX_INDEX to isolate the SE/SH this trace is associated with.
    pCmdSpace = WriteGrbmGfxIndex(pCmdStream, pCmdSpace);

    // Compute the base address of the thread trace data, including the shift amount the
    // register expects.
    const gpusize gpuVirtAddrShifted = (baseGpuVirtAddr + m_dataOffset) >> PerfCtrInfo::BufferAlignShift;

    // Write the base address of the thread trace buffer.
    regSQ_THREAD_TRACE_BASE2 sqThreadTraceBase2 = {};
    sqThreadTraceBase2.bits.ADDR_HI = Util::HighPart(gpuVirtAddrShifted);

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(Gfx09::mmSQ_THREAD_TRACE_BASE2,
                                                  sqThreadTraceBase2.u32All,
                                                  pCmdSpace);

    regSQ_THREAD_TRACE_BASE sqThreadTraceBase = {};
    sqThreadTraceBase.bits.ADDR = Util::LowPart(gpuVirtAddrShifted);

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(Gfx09::mmSQ_THREAD_TRACE_BASE,
                                                  sqThreadTraceBase.u32All,
                                                  pCmdSpace);

    // Write the perf counter registers which control the thread trace properties.
    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(Gfx09::mmSQ_THREAD_TRACE_SIZE,
                                                  m_sqThreadTraceSize.u32All,
                                                  pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(Gfx09::mmSQ_THREAD_TRACE_MASK,
                                                  m_sqThreadTraceMask.u32All,
                                                  pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(Gfx09::mmSQ_THREAD_TRACE_TOKEN_MASK,
                                                  m_sqThreadTraceTokenMask.u32All,
                                                  pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(Gfx09::mmSQ_THREAD_TRACE_PERF_MASK,
                                                  m_sqThreadTracePerfMask.u32All,
                                                  pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(Gfx09::mmSQ_THREAD_TRACE_HIWATER,
                                                  m_sqThreadTraceHiWater.u32All,
                                                  pCmdSpace);

    // NOTE: It is the caller's responsibility to reset GRBM_GFX_INDEX.

    return pCmdSpace;
}

// =====================================================================================================================
// Writes the commands required to update the sqtt token mask.
uint32* Gfx9ThreadTrace::WriteUpdateSqttTokenMaskCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace,
    uint32     sqttTokenMask
    ) const
{
    // Set GRBM_GFX_INDEX to isolate the SE/SH this trace is associated with.
    pCmdSpace = WriteGrbmGfxIndex(pCmdStream, pCmdSpace);

    // Update the token mask register
    regSQ_THREAD_TRACE_TOKEN_MASK tokenMaskReg = m_sqThreadTraceTokenMask;
    tokenMaskReg.gfx09.TOKEN_MASK = sqttTokenMask;
    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(Gfx09::mmSQ_THREAD_TRACE_TOKEN_MASK,
                                                  tokenMaskReg.u32All,
                                                  pCmdSpace);

    // NOTE: It is the caller's responsibility to reset GRBM_GFX_INDEX.

    return pCmdSpace;
}

// =====================================================================================================================
// Issues the PM4 commands necessary to start this thread trace. The owning Experiment object should have issued and
// idle before calling this. Returns the next unused DWORD in pCmdSpace.
uint32* Gfx9ThreadTrace::WriteStartCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    // Set GRBM_GFX_INDEX to isolate the SE/SH this trace is associated with.
    pCmdSpace = WriteGrbmGfxIndex(pCmdStream, pCmdSpace);

    // Write SQ_THREAD_TRACE_CTRL with the reset_buffer flag set to instruct the hardware to
    // reset the trace buffer.
    regSQ_THREAD_TRACE_CTRL sqThreadTraceCtrl = {};
    sqThreadTraceCtrl.gfx09.RESET_BUFFER = 1;

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(Gfx09::mmSQ_THREAD_TRACE_CTRL,
                                                  sqThreadTraceCtrl.u32All,
                                                  pCmdSpace);

    // Write SQ_THREAD_TRACE_MODE with the mode field set to "on" to enable the trace.
    regSQ_THREAD_TRACE_MODE sqThreadTraceMode;
    sqThreadTraceMode.u32All    = m_sqThreadTraceMode.u32All;
    sqThreadTraceMode.bits.MODE = SQ_THREAD_TRACE_MODE_ON;

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(Gfx09::mmSQ_THREAD_TRACE_MODE,
                                                  sqThreadTraceMode.u32All,
                                                  pCmdSpace);

    // NOTE: It is the caller's responsibility to reset GRBM_GFX_INDEX.

    return pCmdSpace;
}

// =====================================================================================================================
// Issues the PM4 commands necessary to stop this thread trace, and populate the parent experiment's GPU memory with the
// appropriate ThreadTraceInfoData contents. Returns the next unused DWORD in pCmdSpace.
uint32* Gfx9ThreadTrace::WriteStopCommands(
    gpusize    baseGpuVirtAddr, ///< Base GPU virtual address of the owning Experiment
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    const auto& cmdUtil = m_device.CmdUtil();

    // Set GRBM_GFX_INDEX to isolate the SE/SH this trace is associated with.
    pCmdSpace = WriteGrbmGfxIndex(pCmdStream, pCmdSpace);

    // Write SQ_THREAD_TRACE_MODE with the mode field set to "off" to disable the trace.
    regSQ_THREAD_TRACE_MODE sqThreadTraceMode;
    sqThreadTraceMode.u32All    = m_sqThreadTraceMode.u32All;
    sqThreadTraceMode.bits.MODE = SQ_THREAD_TRACE_MODE_OFF;

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(Gfx09::mmSQ_THREAD_TRACE_MODE,
                                                  sqThreadTraceMode.u32All,
                                                  pCmdSpace);

    // Flush the thread trace buffer to memory.
    pCmdSpace += cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_FLUSH__GFX09, pCmdStream->GetEngineType(), pCmdSpace);

    // Poll the status register's busy bit to ensure that no events are being logged and written to memory.
    pCmdSpace += cmdUtil.BuildWaitRegMem(mem_space__me_wait_reg_mem__register_space,
                                         function__me_wait_reg_mem__not_equal_reference_value,
                                         engine_sel__me_wait_reg_mem__micro_engine,
                                         Gfx09::mmSQ_THREAD_TRACE_STATUS,
                                         0x1,
                                         Gfx09::SQ_THREAD_TRACE_STATUS__BUSY_MASK,
                                         pCmdSpace);

    // The following code which issues the COPY_DATA commands assumes that the layout of the ThreadTraceInfoData
    // structure is ordered a particular way. Compile-time asserts help us help us guarantee the assumption.
    static_assert(offsetof(ThreadTraceInfoData, curOffset)    == 0, "");
    static_assert(offsetof(ThreadTraceInfoData, traceStatus)  == sizeof(uint32), "");
    static_assert(offsetof(ThreadTraceInfoData, writeCounter) == (sizeof(uint32) * 2), "");

    // Compute the base address of the thread trace info segment.
    const gpusize gpuVirtAddr = (baseGpuVirtAddr + m_infoOffset);

    // Issue a trio of COPY_DATA commands to populate the ThreadTraceInfoData for this
    // thread trace:

    const EngineType engineType = pCmdStream->GetEngineType();
    const uint32     data[]     = { Gfx09::mmSQ_THREAD_TRACE_WPTR,
                                    Gfx09::mmSQ_THREAD_TRACE_STATUS,
                                    Gfx09::mmSQ_THREAD_TRACE_CNTR };

    for (uint32 i = 0; i < Util::ArrayLen(data); i++)
    {
        if (engineType == EngineTypeCompute)
        {
            pCmdSpace += cmdUtil.BuildCopyDataCompute(dst_sel__mec_copy_data__memory__GFX09,
                                                      gpuVirtAddr + i * sizeof(uint32),
                                                      src_sel__mec_copy_data__perfcounters,
                                                      data[i],
                                                      count_sel__mec_copy_data__32_bits_of_data,
                                                      wr_confirm__mec_copy_data__wait_for_confirmation,
                                                      pCmdSpace);
        }
        else
        {
            pCmdSpace += cmdUtil.BuildCopyDataGraphics(engine_sel__me_copy_data__micro_engine,
                                                       dst_sel__me_copy_data__memory__GFX09,
                                                       gpuVirtAddr + i * sizeof(uint32),
                                                       src_sel__me_copy_data__perfcounters,
                                                       data[i],
                                                       count_sel__me_copy_data__32_bits_of_data,
                                                       wr_confirm__me_copy_data__wait_for_confirmation,
                                                       pCmdSpace);
        }
    }
    // NOTE: It is the caller's responsibility to reset GRBM_GFX_INDEX.

    return pCmdSpace;
}

// =====================================================================================================================
// Validates the value of a thread-trace creation option.
Result Gfx9ThreadTrace::Init()
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();
    const auto&              flags     = m_info.optionFlags;
    const auto&              values    = m_info.optionValues;
    Result                   result    = ThreadTrace::Init();

    if ((result == Result::Success)  &&
        (flags.bufferSize)           &&
        ((values.bufferSize == 0) ||
         (values.bufferSize > PerfCtrInfo::MaximumBufferSize) ||
         (Util::Pow2Align(values.bufferSize, PerfCtrInfo::BufferAlignment) != values.bufferSize)))
    {
        result = Result::ErrorInvalidValue;
    }

    if ((result == Result::Success)  &&
        (flags.threadTraceTokenMask) &&
        ((values.threadTraceTokenMask & TokenMaskAll) != values.threadTraceTokenMask))
    {
        result = Result::ErrorInvalidValue;
    }

    if ((result == Result::Success) &&
        (flags.threadTraceRegMask)  &&
        ((values.threadTraceRegMask & RegMaskAll) != values.threadTraceRegMask))
    {
        result = Result::ErrorInvalidValue;
    }

    if ((result == Result::Success) &&
        (flags.threadTraceTargetSh) &&
        (values.threadTraceTargetSh >= chipProps.gfx9.numShaderArrays))
    {
        result = Result::ErrorInvalidValue;
    }

    if ((result == Result::Success) &&
        (flags.threadTraceTargetCu) &&
        (values.threadTraceTargetCu >= chipProps.gfx9.numCuPerSh))
    {
        result = Result::ErrorInvalidValue;
    }

    if ((result == Result::Success)         &&
        (flags.threadTraceSh0CounterMask)   &&
        ((values.threadTraceSh0CounterMask & ShCuMaskAll) != values.threadTraceSh0CounterMask))
    {
        result = Result::ErrorInvalidValue;
    }

    if ((result == Result::Success)         &&
        (flags.threadTraceSh1CounterMask)   &&
        ((values.threadTraceSh1CounterMask & ShCuMaskAll) != values.threadTraceSh1CounterMask))
    {
        result = Result::ErrorInvalidValue;
    }

    if ((result == Result::Success)   &&
        (flags.threadTraceSimdMask)   &&
        ((values.threadTraceSimdMask & SimdMaskAll) != values.threadTraceSimdMask))
    {
        result = Result::ErrorInvalidValue;
    }

    if ((result == Result::Success)   &&
        (flags.threadTraceVmIdMask)   &&
        (values.threadTraceVmIdMask > SQ_THREAD_TRACE_VM_ID_MASK_SINGLE_DETAIL))
    {
        result = Result::ErrorInvalidValue;
    }

    if ((result == Result::Success)         &&
        (flags.threadTraceShaderTypeMask)   &&
        ((values.threadTraceShaderTypeMask & PerfShaderMaskAll) != values.threadTraceShaderTypeMask))
    {
        result = Result::ErrorInvalidValue;
    }

    if ((result == Result::Success)   &&
        (flags.threadTraceIssueMask)  &&
        (values.threadTraceIssueMask > SQ_THREAD_TRACE_ISSUE_MASK_IMMED))
    {
        result = Result::ErrorInvalidValue;
    }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 422
    if ((result == Result::Success)       &&
        (flags.threadTraceStallBehavior)  &&
        (values.threadTraceStallBehavior > GpuProfilerStallNever))
    {
        result = Result::ErrorInvalidValue;
    }
#endif

    return result;
}

} // Gfx9
} // Pal
