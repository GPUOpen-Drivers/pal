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
SpmTrace::SpmTrace(
    const Device* pDevice)
    :
    Pal::SpmTrace(pDevice->Parent()),
    m_device(*pDevice)
{
    m_ringBaseLo.u32All     = 0;
    m_ringBaseHi.u32All     = 0;
    m_segmentSize.u32All    = 0;
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
// Issues the PM4 commands necessary to start this thread trace. The owning Experiment object should have issued and
// idle before calling this. Returns the next unused DWORD in pCmdSpace.
uint32* SpmTrace::WriteSetupCommands(
    gpusize         ringBaseAddr,
    Pal::CmdStream* pCmdStream,
    uint32*         pCmdSpace)
{
    PAL_NOT_IMPLEMENTED();
    return pCmdSpace;
}

// =====================================================================================================================
uint32* SpmTrace::WriteStartCommands(
    Pal::CmdStream* pCmdStream,
    uint32*         pCmdSpace)
{
    // how do you start an spm trace?
    // cp perfmon master reg
    // vgt event

    PAL_NOT_IMPLEMENTED();
    return pCmdSpace;
}

// =====================================================================================================================
uint32* SpmTrace::WriteEndCommands(
    Pal::CmdStream* pCmdStream,
    uint32* pCmdSpace)
{
    PAL_NOT_IMPLEMENTED();
    return pCmdSpace;
}

// =====================================================================================================================
void SpmTrace::CalculateSegmentSize()
{
    PAL_NOT_IMPLEMENTED();
}

// =====================================================================================================================
void SpmTrace::CalculateMuxRam()
{
    PAL_NOT_IMPLEMENTED();
}

// =====================================================================================================================
Result SpmTrace::GetTraceLayout(
    SpmTraceLayout* pLayout
    ) const
{
    Result result = Result::Success;

    PAL_NOT_IMPLEMENTED();
    return result;
}

// =====================================================================================================================
ThreadTrace::ThreadTrace(
    const Device*        pDevice,   ///< [retained] Associated Device object
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 373
    const PerfTraceInfo& info)      ///< [in] Trace creation info
#else
    const ThreadTraceInfo& info)    ///< [in] Trace creation info
#endif
    :
    Pal::ThreadTrace(pDevice->Parent(), info),
    m_device(*pDevice),
    m_info(info)
{
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 373
    const PerfTraceInfo&  info)
#else
    const ThreadTraceInfo&  info)
#endif
    :
    ThreadTrace(pDevice, info)
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
    m_sqThreadTraceMask.bits.SIMD_EN     = SimdMaskAll;
    m_sqThreadTraceMask.bits.VM_ID_MASK  = SQ_THREAD_TRACE_VM_ID_MASK_SINGLE;

    const GpuChipProperties& chipProps = pDevice->Parent()->ChipProperties();

    // We need to pull some register fields for SQ_THREAD_TRACE_MASK from the Adapter.
    regSQ_THREAD_TRACE_MASK__GFX09 sqThreadTraceMask = {};

    m_sqThreadTraceMask.bits.REG_STALL_EN = sqThreadTraceMask.bits.REG_STALL_EN;
    m_sqThreadTraceMask.bits.SQ_STALL_EN  = sqThreadTraceMask.bits.SQ_STALL_EN;
    m_sqThreadTraceMask.bits.SPI_STALL_EN = sqThreadTraceMask.bits.SPI_STALL_EN;

    // NOTE: DXX mentions in a comment that for Oland, the driver may need to force
    //       SPI_STALL_EN to zero to avoid doubly creating some wavefronts, avoiding a
    //       possible hang situation.
    // TODO: Need to follow-up on this, the DXX comments for it are unclear.

    m_sqThreadTraceTokenMask.u32All = 0;
    m_sqThreadTraceTokenMask.bits.TOKEN_MASK = TokenMaskAll;
    m_sqThreadTraceTokenMask.bits.REG_MASK   = RegMaskAll;

    m_sqThreadTracePerfMask.u32All = 0;
    m_sqThreadTracePerfMask.bits.SH0_MASK = ShCuMaskAll;
    m_sqThreadTracePerfMask.bits.SH1_MASK = ShCuMaskAll;

    // Default to only selecting CUs that are active and not reserved for realtime use.  GFX9 only has one
    // shader array.
    PAL_ASSERT(chipProps.gfx9.numShaderArrays == 1);
    const uint32 cuTraceableCuMask = chipProps.gfx9.activeCuMask[0][m_shaderEngine] & ~chipProps.gfxip.realTimeCuMask;

    // If it exists, select the first available CU from the mask
    uint32 firstActiveCu = 0;
    if (Util::BitMaskScanForward(&firstActiveCu, cuTraceableCuMask))
    {
        m_sqThreadTraceMask.bits.CU_SEL = firstActiveCu;
    }

    SetOptions();
}

// =====================================================================================================================
// Initializes one of the thread-trace creation options.
void Gfx9ThreadTrace::SetOptions()
{
    const auto& flags  = m_info.optionFlags;
    const auto& values = m_info.optionValues;

    const size_t bufferSize = (flags.bufferSize) ? values.bufferSize : PerfCtrInfo::DefaultBufferSize;

    m_sqThreadTraceSize.bits.SIZE = (bufferSize >> PerfCtrInfo::BufferAlignShift);

    // Need to update our buffer-size parameter.
    m_dataSize = bufferSize;

    if (flags.threadTraceTokenMask)
    {
        m_sqThreadTraceTokenMask.bits.TOKEN_MASK = values.threadTraceTokenMask;
    }

    if (flags.threadTraceRegMask)
    {
        m_sqThreadTraceTokenMask.bits.REG_MASK = values.threadTraceRegMask;
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
}

// =====================================================================================================================
// Issues commands to set-up the GRBM_GFX_INDEX register to write to only the Shader Engine and Shader Array that this
// trace is associated with. Returns the next unused DWORD in pCmdSpace.
uint32* Gfx9ThreadTrace::WriteGrbmGfxIndex(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    regGRBM_GFX_INDEX__GFX09 grbmGfxIndex = {};
    grbmGfxIndex.bits.SE_INDEX                  = m_shaderEngine;
    grbmGfxIndex.bits.SH_INDEX                  = m_sqThreadTraceMask.bits.SH_SEL;
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
    regSQ_THREAD_TRACE_BASE2__GFX09 sqThreadTraceBase2 = {};
    sqThreadTraceBase2.bits.ADDR_HI = Util::HighPart(gpuVirtAddrShifted);

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_BASE2__GFX09,
                                                  sqThreadTraceBase2.u32All,
                                                  pCmdSpace);

    regSQ_THREAD_TRACE_BASE__GFX09 sqThreadTraceBase = {};
    sqThreadTraceBase.bits.ADDR = Util::LowPart(gpuVirtAddrShifted);

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_BASE__GFX09,
                                                  sqThreadTraceBase.u32All,
                                                  pCmdSpace);

    // Write the perf counter registers which control the thread trace properties.
    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_SIZE__GFX09,
                                                  m_sqThreadTraceSize.u32All,
                                                  pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_MASK__GFX09,
                                                  m_sqThreadTraceMask.u32All,
                                                  pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_TOKEN_MASK__GFX09,
                                                  m_sqThreadTraceTokenMask.u32All,
                                                  pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_PERF_MASK__GFX09,
                                                  m_sqThreadTracePerfMask.u32All,
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
    const auto& regInfo = m_device.CmdUtil().GetRegInfo();

    // Set GRBM_GFX_INDEX to isolate the SE/SH this trace is associated with.
    pCmdSpace = WriteGrbmGfxIndex(pCmdStream, pCmdSpace);

    // Update the token mask register
    regSQ_THREAD_TRACE_TOKEN_MASK__GFX09 tokenMaskReg = m_sqThreadTraceTokenMask;
    tokenMaskReg.bits.TOKEN_MASK = sqttTokenMask;
    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_TOKEN_MASK__GFX09,
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
    regSQ_THREAD_TRACE_CTRL__GFX09 sqThreadTraceCtrl = {};
    sqThreadTraceCtrl.bits.RESET_BUFFER = 1;

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_CTRL__GFX09,
                                                  sqThreadTraceCtrl.u32All,
                                                  pCmdSpace);

    // Write SQ_THREAD_TRACE_MODE with the mode field set to "on" to enable the trace.
    regSQ_THREAD_TRACE_MODE__GFX09 sqThreadTraceMode;
    sqThreadTraceMode.u32All    = m_sqThreadTraceMode.u32All;
    sqThreadTraceMode.bits.MODE = SQ_THREAD_TRACE_MODE_ON;

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_MODE__GFX09,
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
    regSQ_THREAD_TRACE_MODE__GFX09 sqThreadTraceMode;
    sqThreadTraceMode.u32All    = m_sqThreadTraceMode.u32All;
    sqThreadTraceMode.bits.MODE = SQ_THREAD_TRACE_MODE_OFF;

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_MODE__GFX09,
                                                  sqThreadTraceMode.u32All,
                                                  pCmdSpace);

    // Flush the thread trace buffer to memory.
    pCmdSpace += cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_FLUSH__GFX09, pCmdStream->GetEngineType(), pCmdSpace);

    // Poll the status register's busy bit to ensure that no events are being logged and written to memory.
    pCmdSpace += cmdUtil.BuildWaitRegMem(mem_space__me_wait_reg_mem__register_space,
                                         function__me_wait_reg_mem__not_equal_reference_value,
                                         engine_sel__me_wait_reg_mem__micro_engine,
                                         mmSQ_THREAD_TRACE_STATUS__GFX09,
                                         0x1,
                                         SQ_THREAD_TRACE_STATUS__BUSY_MASK__GFX09,
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
    const uint32     data[]     = { mmSQ_THREAD_TRACE_WPTR__GFX09,
                                    mmSQ_THREAD_TRACE_STATUS__GFX09,
                                    mmSQ_THREAD_TRACE_CNTR__GFX09 };

    for (uint32 i = 0; i < (sizeof(data) / sizeof(data[0])); i++)
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
        ((values.bufferSize > PerfCtrInfo::MaximumBufferSize) ||
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

    return result;
}

} // Gfx9
} // Pal
