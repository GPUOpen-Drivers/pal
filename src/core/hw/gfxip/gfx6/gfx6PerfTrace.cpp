/*
 *******************************************************************************
 *
 * Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "core/hw/gfxip/gfx6/gfx6CmdStream.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6PerfTrace.h"

#include "core/hw/amdgpu_asic.h"

namespace Pal
{
namespace Gfx6
{

// =====================================================================================================================
ThreadTrace::ThreadTrace(
    const Device*        pDevice,    ///< [retained] Associated Device object
    const PerfTraceInfo& info)      ///< [in] Trace creation info
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
    const PerfTraceInfo& info)
{
    const auto& flags  = info.optionFlags;
    const auto& values = info.optionValues;

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

    // NOTE: It is the caller's responsibility to reset GRBM_GFX_INDEX.

    return pCmdSpace;
}

// =====================================================================================================================
// Writes the commands required to update the sqtt token mask.
uint32* ThreadTrace::WriteUpdateSqttTokenMaskCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace,
    uint32     sqttTokenMask
    ) const
{
    const auto& regInfo = m_device.CmdUtil().GetRegInfo();

    // Set GRBM_GFX_INDEX to isolate the SE/SH this trace is associated with.
    pCmdSpace = WriteGrbmGfxIndex(pCmdStream, pCmdSpace);

    // Update the token mask register
    regSQ_THREAD_TRACE_TOKEN_MASK tokenMaskReg = m_sqThreadTraceTokenMask;
    tokenMaskReg.bits.TOKEN_MASK = sqttTokenMask;
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

} // Gfx6
} // Pal
