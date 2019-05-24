/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/pipeline.h"
#include "core/hw/gfxip/gfx6/g_gfx6PalSettings.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6ShadowedRegisters.h"

#include <cstdio>

using namespace Util;

namespace Pal
{
namespace Gfx6
{

// Lookup table for converting a VGT_EVENT_TYPE to the appropriate event index.
constexpr uint32 EventTypeToIndexTable[] =
{
    EVENT_WRITE_INDEX_INVALID,              // Reserved_0x00                   0x00000000
    EVENT_WRITE_INDEX_SAMPLE_STREAMOUTSTATS,// SAMPLE_STREAMOUTSTATS1          0x00000001
    EVENT_WRITE_INDEX_SAMPLE_STREAMOUTSTATS,// SAMPLE_STREAMOUTSTATS2          0x00000002
    EVENT_WRITE_INDEX_SAMPLE_STREAMOUTSTATS,// SAMPLE_STREAMOUTSTATS3          0x00000003
    EVENT_WRITE_INDEX_ANY_EOP_TIMESTAMP,    // CACHE_FLUSH_TS                  0x00000004
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // CONTEXT_DONE                    0x00000005
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // CACHE_FLUSH                     0x00000006
    EVENT_WRITE_INDEX_VS_PS_PARTIAL_FLUSH,  // CS_PARTIAL_FLUSH                0x00000007
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // VGT_STREAMOUT_SYNC              0x00000008
    EVENT_WRITE_INDEX_INVALID,              // Reserved_0x09                   0x00000009
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // VGT_STREAMOUT_RESET             0x0000000a
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // END_OF_PIPE_INCR_DE             0x0000000b
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // END_OF_PIPE_IB_END              0x0000000c
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // RST_PIX_CNT                     0x0000000d
    EVENT_WRITE_INDEX_INVALID,              // Reserved_0x0E                   0x0000000e
    EVENT_WRITE_INDEX_VS_PS_PARTIAL_FLUSH,  // VS_PARTIAL_FLUSH                0x0000000f
    EVENT_WRITE_INDEX_VS_PS_PARTIAL_FLUSH,  // PS_PARTIAL_FLUSH                0x00000010
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // FLUSH_HS_OUTPUT                 0x00000011
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // FLUSH_LS_OUTPUT                 0x00000012
    EVENT_WRITE_INDEX_INVALID,              // Reserved_0x13                   0x00000013
    EVENT_WRITE_INDEX_ANY_EOP_TIMESTAMP,    // CACHE_FLUSH_AND_INV_TS_EVENT    0x00000014
    EVENT_WRITE_INDEX_ZPASS_DONE,           // ZPASS_DONE                      0x00000015
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // CACHE_FLUSH_AND_INV_EVENT       0x00000016
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // PERFCOUNTER_START               0x00000017
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // PERFCOUNTER_STOP                0x00000018
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // PIPELINESTAT_START              0x00000019
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // PIPELINESTAT_STOP               0x0000001a
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // PERFCOUNTER_SAMPLE              0x0000001b
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // FLUSH_ES_OUTPUT                 0x0000001c
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // FLUSH_GS_OUTPUT                 0x0000001d
    EVENT_WRITE_INDEX_SAMPLE_PIPELINESTAT,  // SAMPLE_PIPELINESTAT             0x0000001e
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // SO_VGTSTREAMOUT_FLUSH           0x0000001f
    EVENT_WRITE_INDEX_SAMPLE_STREAMOUTSTATS,// SAMPLE_STREAMOUTSTATS           0x00000020
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // RESET_VTX_CNT                   0x00000021
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // BLOCK_CONTEXT_DONE              0x00000022
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // CS_CONTEXT_DONE                 0x00000023
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // VGT_FLUSH                       0x00000024
    EVENT_WRITE_INDEX_INVALID,              // Reserved_0x25                   0x00000025
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // SQ_NON_EVENT                    0x00000026
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // SC_SEND_DB_VPZ                  0x00000027
    EVENT_WRITE_INDEX_ANY_EOP_TIMESTAMP,    // BOTTOM_OF_PIPE_TS               0x00000028
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // FLUSH_SX_TS                     0x00000029
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // DB_CACHE_FLUSH_AND_INV          0x0000002a
    EVENT_WRITE_INDEX_ANY_EOP_TIMESTAMP,    // FLUSH_AND_INV_DB_DATA_TS        0x0000002b
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // FLUSH_AND_INV_DB_META           0x0000002c
    EVENT_WRITE_INDEX_ANY_EOP_TIMESTAMP,    // FLUSH_AND_INV_CB_DATA_TS        0x0000002d
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // FLUSH_AND_INV_CB_META           0x0000002e
    EVENT_WRITE_INDEX_ANY_EOS_TIMESTAMP,    // CS_DONE                         0x0000002f
    EVENT_WRITE_INDEX_ANY_EOS_TIMESTAMP,    // PS_DONE                         0x00000030
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // FLUSH_AND_INV_CB_PIXEL_DATA     0x00000031
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // SX_CB_RAT_ACK_REQUEST           0x00000032
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // THREAD_TRACE_START              0x00000033
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // THREAD_TRACE_STOP               0x00000034
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // THREAD_TRACE_MARKER             0x00000035
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // THREAD_TRACE_FLUSH              0x00000036
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // THREAD_TRACE_FINISH             0x00000037
    EVENT_WRITE_INDEX_ZPASS_DONE,           // PIXEL_PIPE_STAT_CONTROL         0x00000038
    EVENT_WRITE_INDEX_ZPASS_DONE,           // PIXEL_PIPE_STAT_DUMP            0x00000039
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,    // PIXEL_PIPE_STAT_RESET           0x0000003a
};

// Size of the event index table, in entries.
constexpr size_t EventTypeToIndexTableSize = ArrayLen(EventTypeToIndexTable);

// Lookup table for converting a AtomicOp index into a TC_OP on Gfx6 hardware.
constexpr TC_OP Gfx6AtomicOpConversionTable[] =
{
    TC_OP_ATOMIC_ADD_RTN_32,      // AddInt32
    TC_OP_ATOMIC_SUB_RTN_32,      // SubInt32
    TC_OP_ATOMIC_UMIN_RTN_32__SI, // MinUint32
    TC_OP_ATOMIC_UMAX_RTN_32__SI, // MaxUint32
    TC_OP_ATOMIC_SMIN_RTN_32__SI, // MinSint32
    TC_OP_ATOMIC_SMAX_RTN_32__SI, // MaxSing32
    TC_OP_ATOMIC_AND_RTN_32__SI,  // AndInt32
    TC_OP_ATOMIC_OR_RTN_32__SI,   // OrInt32
    TC_OP_ATOMIC_XOR_RTN_32__SI,  // XorInt32
    TC_OP_ATOMIC_INC_RTN_32__SI,  // IncUint32
    TC_OP_ATOMIC_DEC_RTN_32__SI,  // DecUint32
    TC_OP_ATOMIC_ADD_RTN_64,      // AddInt64
    TC_OP_ATOMIC_SUB_RTN_64,      // SubInt64
    TC_OP_ATOMIC_UMIN_RTN_64__SI, // MinUint64
    TC_OP_ATOMIC_UMAX_RTN_64__SI, // MaxUint64
    TC_OP_ATOMIC_SMIN_RTN_64__SI, // MinSint64
    TC_OP_ATOMIC_SMAX_RTN_64__SI, // MaxSint64
    TC_OP_ATOMIC_AND_RTN_64__SI,  // AndInt64
    TC_OP_ATOMIC_OR_RTN_64__SI,   // OrInt64
    TC_OP_ATOMIC_XOR_RTN_64__SI,  // XorInt64
    TC_OP_ATOMIC_INC_RTN_64__SI,  // IncUint64
    TC_OP_ATOMIC_DEC_RTN_64__SI,  // DecUint64
};

// Size of the Gfx6AtomicOp conversion table, in entries.
constexpr size_t Gfx6AtomicOpConversionTableSize = ArrayLen(Gfx6AtomicOpConversionTable);

// The Gfx6AtomicOp table should contain one entry for each AtomicOp.
static_assert((Gfx6AtomicOpConversionTableSize == static_cast<size_t>(AtomicOp::Count)),
              "Gfx6AtomicOp conversion table has too many/few entries");

// Lookup table for converting a AtomicOp index into a TC_OP on Gfx7 hardware.
constexpr TC_OP Gfx7AtomicOpConversionTable[] =
{
    TC_OP_ATOMIC_ADD_RTN_32,          // AddInt32
    TC_OP_ATOMIC_SUB_RTN_32,          // SubInt32
    TC_OP_ATOMIC_UMIN_RTN_32__CI__VI, // MinUint32
    TC_OP_ATOMIC_UMAX_RTN_32__CI__VI, // MaxUint32
    TC_OP_ATOMIC_SMIN_RTN_32__CI__VI, // MinSint32
    TC_OP_ATOMIC_SMAX_RTN_32__CI__VI, // MaxSing32
    TC_OP_ATOMIC_AND_RTN_32__CI__VI,  // AndInt32
    TC_OP_ATOMIC_OR_RTN_32__CI__VI,   // OrInt32
    TC_OP_ATOMIC_XOR_RTN_32__CI__VI,  // XorInt32
    TC_OP_ATOMIC_INC_RTN_32__CI__VI,  // IncUint32
    TC_OP_ATOMIC_DEC_RTN_32__CI__VI,  // DecUint32
    TC_OP_ATOMIC_ADD_RTN_64,          // AddInt64
    TC_OP_ATOMIC_SUB_RTN_64,          // SubInt64
    TC_OP_ATOMIC_UMIN_RTN_64__CI__VI, // MinUint64
    TC_OP_ATOMIC_UMAX_RTN_64__CI__VI, // MaxUint64
    TC_OP_ATOMIC_SMIN_RTN_64__CI__VI, // MinSint64
    TC_OP_ATOMIC_SMAX_RTN_64__CI__VI, // MaxSint64
    TC_OP_ATOMIC_AND_RTN_64__CI__VI,  // AndInt64
    TC_OP_ATOMIC_OR_RTN_64__CI__VI,   // OrInt64
    TC_OP_ATOMIC_XOR_RTN_64__CI__VI,  // XorInt64
    TC_OP_ATOMIC_INC_RTN_64__CI__VI,  // IncUint64
    TC_OP_ATOMIC_DEC_RTN_64__CI__VI,  // DecUint64
};

// Size of the Gfx7AtomicOp conversion table, in entries.
constexpr size_t Gfx7AtomicOpConversionTableSize = ArrayLen(Gfx7AtomicOpConversionTable);

// The CiAtomicOp table should contain one entry for each AtomicOp.
static_assert((Gfx7AtomicOpConversionTableSize == static_cast<size_t>(AtomicOp::Count)),
              "Gfx7AtomicOp conversion table has too many/few entries");

// =====================================================================================================================
// Note that this constructor is invoked before settings have been committed.
CmdUtil::CmdUtil(
    const Pal::Device& device)
    :
    m_device(device),
    m_chipFamily(device.ChipProperties().gfxLevel)
#if PAL_ENABLE_PRINTS_ASSERTS
    , m_verifyShadowedRegisters(device.Settings().cmdUtilVerifyShadowedRegRanges &&
                                (m_chipFamily >= GfxIpLevel::GfxIp8))
#endif
{
    switch (m_chipFamily)
    {
    case GfxIpLevel::GfxIp6:
        m_registerInfo.mmCpPerfmonCntl          = mmCP_PERFMON_CNTL__SI;
        m_registerInfo.mmCpStrmoutCntl          = mmCP_STRMOUT_CNTL__SI;
        m_registerInfo.mmGrbmGfxIndex           = mmGRBM_GFX_INDEX__SI;
        m_registerInfo.mmRlcPerfmonCntl         = mmRLC_PERFMON_CNTL__SI;
        m_registerInfo.mmSqPerfCounterCtrl      = mmSQ_PERFCOUNTER_CTRL__SI;
        m_registerInfo.mmSqThreadTraceUserData2 = mmSQ_THREAD_TRACE_USERDATA_2__SI;
        m_registerInfo.mmSqThreadTraceUserData3 = mmSQ_THREAD_TRACE_USERDATA_3__SI;
        m_registerInfo.mmSqThreadTraceBase      = mmSQ_THREAD_TRACE_BASE__SI__CI;
        m_registerInfo.mmSqThreadTraceBase2     = 0;
        m_registerInfo.mmSqThreadTraceSize      = mmSQ_THREAD_TRACE_SIZE__SI__CI;
        m_registerInfo.mmSqThreadTraceMask      = mmSQ_THREAD_TRACE_MASK__SI__CI;
        m_registerInfo.mmSqThreadTraceTokenMask = mmSQ_THREAD_TRACE_TOKEN_MASK__SI__CI;
        m_registerInfo.mmSqThreadTracePerfMask  = mmSQ_THREAD_TRACE_PERF_MASK__SI__CI;
        m_registerInfo.mmSqThreadTraceCtrl      = mmSQ_THREAD_TRACE_CTRL__SI__CI;
        m_registerInfo.mmSqThreadTraceMode      = mmSQ_THREAD_TRACE_MODE__SI__CI;
        m_registerInfo.mmSqThreadTraceWptr      = mmSQ_THREAD_TRACE_WPTR__SI__CI;
        m_registerInfo.mmSqThreadTraceStatus    = mmSQ_THREAD_TRACE_STATUS__SI__CI;
        m_registerInfo.mmSqThreadTraceHiWater   = mmSQ_THREAD_TRACE_HIWATER__SI__CI;
        m_registerInfo.mmSrbmPerfmonCntl        = mmSRBM_PERFMON_CNTL__SI__CI;
        break;

    case GfxIpLevel::GfxIp7:
        m_registerInfo.mmCpPerfmonCntl          = mmCP_PERFMON_CNTL__CI__VI;
        m_registerInfo.mmCpStrmoutCntl          = mmCP_STRMOUT_CNTL__CI__VI;
        m_registerInfo.mmGrbmGfxIndex           = mmGRBM_GFX_INDEX__CI__VI;
        m_registerInfo.mmRlcPerfmonCntl         = mmRLC_PERFMON_CNTL__CI__VI;
        m_registerInfo.mmSqPerfCounterCtrl      = mmSQ_PERFCOUNTER_CTRL__CI__VI;
        m_registerInfo.mmSqThreadTraceUserData2 = mmSQ_THREAD_TRACE_USERDATA_2__CI__VI;
        m_registerInfo.mmSqThreadTraceUserData3 = mmSQ_THREAD_TRACE_USERDATA_3__CI__VI;
        m_registerInfo.mmSqThreadTraceBase      = mmSQ_THREAD_TRACE_BASE__SI__CI;
        m_registerInfo.mmSqThreadTraceBase2     = mmSQ_THREAD_TRACE_BASE2__CI;
        m_registerInfo.mmSqThreadTraceSize      = mmSQ_THREAD_TRACE_SIZE__SI__CI;
        m_registerInfo.mmSqThreadTraceMask      = mmSQ_THREAD_TRACE_MASK__SI__CI;
        m_registerInfo.mmSqThreadTraceTokenMask = mmSQ_THREAD_TRACE_TOKEN_MASK__SI__CI;
        m_registerInfo.mmSqThreadTracePerfMask  = mmSQ_THREAD_TRACE_PERF_MASK__SI__CI;
        m_registerInfo.mmSqThreadTraceCtrl      = mmSQ_THREAD_TRACE_CTRL__SI__CI;
        m_registerInfo.mmSqThreadTraceMode      = mmSQ_THREAD_TRACE_MODE__SI__CI;
        m_registerInfo.mmSqThreadTraceWptr      = mmSQ_THREAD_TRACE_WPTR__SI__CI;
        m_registerInfo.mmSqThreadTraceStatus    = mmSQ_THREAD_TRACE_STATUS__SI__CI;
        m_registerInfo.mmSqThreadTraceHiWater   = mmSQ_THREAD_TRACE_HIWATER__SI__CI;
        m_registerInfo.mmSrbmPerfmonCntl        = mmSRBM_PERFMON_CNTL__SI__CI;
        break;

    case GfxIpLevel::GfxIp8:
    case GfxIpLevel::GfxIp8_1:
        m_registerInfo.mmCpPerfmonCntl          = mmCP_PERFMON_CNTL__CI__VI;
        m_registerInfo.mmCpStrmoutCntl          = mmCP_STRMOUT_CNTL__CI__VI;
        m_registerInfo.mmGrbmGfxIndex           = mmGRBM_GFX_INDEX__CI__VI;
        m_registerInfo.mmRlcPerfmonCntl         = mmRLC_PERFMON_CNTL__CI__VI;
        m_registerInfo.mmSqPerfCounterCtrl      = mmSQ_PERFCOUNTER_CTRL__CI__VI;
        m_registerInfo.mmSqThreadTraceUserData2 = mmSQ_THREAD_TRACE_USERDATA_2__CI__VI;
        m_registerInfo.mmSqThreadTraceUserData3 = mmSQ_THREAD_TRACE_USERDATA_3__CI__VI;
        m_registerInfo.mmSqThreadTraceBase      = mmSQ_THREAD_TRACE_BASE__VI;
        m_registerInfo.mmSqThreadTraceBase2     = mmSQ_THREAD_TRACE_BASE2__VI;
        m_registerInfo.mmSqThreadTraceSize      = mmSQ_THREAD_TRACE_SIZE__VI;
        m_registerInfo.mmSqThreadTraceMask      = mmSQ_THREAD_TRACE_MASK__VI;
        m_registerInfo.mmSqThreadTraceTokenMask = mmSQ_THREAD_TRACE_TOKEN_MASK__VI;
        m_registerInfo.mmSqThreadTracePerfMask  = mmSQ_THREAD_TRACE_PERF_MASK__VI;
        m_registerInfo.mmSqThreadTraceCtrl      = mmSQ_THREAD_TRACE_CTRL__VI;
        m_registerInfo.mmSqThreadTraceMode      = mmSQ_THREAD_TRACE_MODE__VI;
        m_registerInfo.mmSqThreadTraceWptr      = mmSQ_THREAD_TRACE_WPTR__VI;
        m_registerInfo.mmSqThreadTraceStatus    = mmSQ_THREAD_TRACE_STATUS__VI;
        m_registerInfo.mmSqThreadTraceHiWater   = mmSQ_THREAD_TRACE_HIWATER__VI;
        m_registerInfo.mmSrbmPerfmonCntl        = mmSRBM_PERFMON_CNTL__VI;
        break;

    default:
        PAL_NOT_IMPLEMENTED();
        break;
    }
}

// =====================================================================================================================
// Gets the number of DWORDs that are required for a cond exec packet.
uint32 CmdUtil::GetCondExecSizeInDwords() const
{
    // Note that the "CI" packet is used on CI+ ASICs.
    return (m_chipFamily == GfxIpLevel::GfxIp6) ? PM4_CMD_COND_EXEC_DWORDS : PM4_CMD_COND_EXEC_CI_DWORDS;
}

// =====================================================================================================================
// Gets the worst case number of DWORDs that are required for a CP DMA packet.
uint32 CmdUtil::GetDmaDataWorstCaseSize() const
{
    // If the CP DMA alignment workaround is enabled we might issue up to three DMA packets.
    const uint32 packetCount = (GetGfx6Settings(m_device).cpDmaSrcAlignment != CpDmaAlignmentDefault) ? 3 : 1;
    const uint32 packetSize  = (m_chipFamily == GfxIpLevel::GfxIp6) ? PM4_CMD_CP_DMA_DWORDS : PM4_CMD_DMA_DATA_DWORDS;

    return packetCount * packetSize;
}

// =====================================================================================================================
// Gets the number of DWORDs that are required for a CP DMA packet.
uint32 CmdUtil::GetDmaDataSizeInDwords(
    const DmaDataInfo& dmaData
    ) const
{
    uint32 dmaCount = 0;

    const uint32 alignment = GetGfx6Settings(m_device).cpDmaSrcAlignment;

    // See BuildDmaData() for details on the alignment workaround logic.
    if ((alignment != CpDmaAlignmentDefault) && (dmaData.srcSel != CPDMA_SRC_SEL_DATA))
    {
        const uint32 addrAlignUp = static_cast<uint32>(Pow2Align(dmaData.srcAddr, alignment) - dmaData.srcAddr);

        if ((addrAlignUp > 0) && (dmaData.numBytes >= 512) && (dmaData.srcSel != CPDMA_SRC_SEL_GDS))
        {
            dmaCount = 2;
        }
        else
        {
            dmaCount = 1;
        }

        const uint32 sizeAlignUp = Pow2Align(dmaData.numBytes, alignment) - dmaData.numBytes;

        if (sizeAlignUp > 0)
        {
            dmaCount++;
        }
    }
    else
    {
        dmaCount = 1;
    }

    const uint32 packetSize  = (m_chipFamily == GfxIpLevel::GfxIp6) ? PM4_CMD_CP_DMA_DWORDS : PM4_CMD_DMA_DATA_DWORDS;

    return dmaCount * packetSize;
}

// =====================================================================================================================
// Gets the minimum number of DWORDs that are required for a NOP packet.
uint32 CmdUtil::GetMinNopSizeInDwords() const
{
    // GFX8 added a new NOP packet mode to support 1DW NOPs, otherwise we're stuck at 2DW.
    return (m_chipFamily >= GfxIpLevel::GfxIp8) ? 1 : 2;
}

// =====================================================================================================================
// Converts a VGT event type to the appropriate event index.
uint32 CmdUtil::EventIndexFromEventType(
    VGT_EVENT_TYPE eventType)
{
    PAL_ASSERT(eventType < EventTypeToIndexTableSize);
    PAL_ASSERT(EventTypeToIndexTable[eventType] != EVENT_WRITE_INDEX_INVALID);

    return EventTypeToIndexTable[eventType];
}

// =====================================================================================================================
// Converts a CompareFunc enum to the appropriate function for a CondIndirectBuffer packet.
uint32 CmdUtil::CondIbFuncFromCompareType(
    CompareFunc compareFunc)
{
    constexpr uint32 ConversionTableSize = 7;
    constexpr uint32 ConversionTable[ConversionTableSize] =
    {
        COND_INDIRECT_BUFFER_FUNC_LESS,          // CompareFunc::Less
        COND_INDIRECT_BUFFER_FUNC_EQUAL,         // CompareFunc::Equal
        COND_INDIRECT_BUFFER_FUNC_LESS_EQUAL,    // CompareFunc::LessEqual
        COND_INDIRECT_BUFFER_FUNC_GREATER,       // CompareFunc::Greater
        COND_INDIRECT_BUFFER_FUNC_NOT_EQUAL,     // CompareFunc::NotEqual
        COND_INDIRECT_BUFFER_FUNC_GREATER_EQUAL, // CompareFunc::GreaterEqual
        COND_INDIRECT_BUFFER_FUNC_ALWAYS         // CompareFunc::Always
    };

    // CompareFunc::Never is not supported natively by the hardware.
    PAL_ASSERT(compareFunc != CompareFunc::Never);

    const uint32 index = static_cast<uint32>(compareFunc) - static_cast<uint32>(CompareFunc::Less);
    PAL_ASSERT(index < ConversionTableSize);

    return ConversionTable[index];
}

// =====================================================================================================================
// Converts a CompareFunc enum to the appropriate function for a WaitRegMem packet.
uint32 CmdUtil::WaitRegMemFuncFromCompareType(
    CompareFunc compareFunc)
{
    constexpr uint32 ConversionTableSize = 7;
    constexpr uint32 ConversionTable[ConversionTableSize] =
    {
        WAIT_REG_MEM_FUNC_LESS,          // CompareFunc::Less
        WAIT_REG_MEM_FUNC_EQUAL,         // CompareFunc::Equal
        WAIT_REG_MEM_FUNC_LESS_EQUAL,    // CompareFunc::LessEqual
        WAIT_REG_MEM_FUNC_GREATER,       // CompareFunc::Greater
        WAIT_REG_MEM_FUNC_NOT_EQUAL,     // CompareFunc::NotEqual
        WAIT_REG_MEM_FUNC_GREATER_EQUAL, // CompareFunc::GreaterEqual
        WAIT_REG_MEM_FUNC_ALWAYS         // CompareFunc::Always
    };

    // CompareFunc::Never is not supported natively by the hardware.
    PAL_ASSERT(compareFunc != CompareFunc::Never);
    // CompareFunc::Always is supported by the hardware
    PAL_ALERT(compareFunc == CompareFunc::Always);

    const uint32 index = static_cast<uint32>(compareFunc) - static_cast<uint32>(CompareFunc::Less);
    PAL_ASSERT(index < ConversionTableSize);

    return ConversionTable[index];
}

// =====================================================================================================================
// True if the specified register is in config reg space, false otherwise.
bool CmdUtil::IsConfigReg(
    uint32 regAddr
    ) const
{
    return ((regAddr >= CONFIG_SPACE_START) && (regAddr <= CONFIG_SPACE_END__SI));
}

// =====================================================================================================================
// True if the specified register is in user-config reg space, false otherwise.
bool CmdUtil::IsUserConfigReg(
    uint32 regAddr
    ) const
{
    return ((regAddr >= UCONFIG_SPACE_START__CI__VI) && (regAddr <= UCONFIG_SPACE_END__CI__VI));
}

// =====================================================================================================================
// True if the specified register is in context reg space, false otherwise.
bool CmdUtil::IsContextReg(
    uint32 regAddr
    ) const
{
    const uint32 contextSpaceEnd =
        (m_chipFamily == GfxIpLevel::GfxIp6) ? CONTEXT_SPACE_END__SI : CONTEXT_SPACE_END__CI__VI;

    const bool isContextReg = ((regAddr >= CONTEXT_SPACE_START) && (regAddr <= contextSpaceEnd));

    // Assert if we need to extend our internal range of context registers we actually set.
    PAL_ASSERT((isContextReg == false) || ((regAddr - CONTEXT_SPACE_START) < CntxRegUsedRangeSize));

    return isContextReg;
}

// =====================================================================================================================
// True if the specified register is in persistent data space, false otherwise.
bool CmdUtil::IsShReg(
    uint32 regAddr
    ) const
{
    const bool isShReg = ((regAddr >= PERSISTENT_SPACE_START) && (regAddr <= PERSISTENT_SPACE_END));

    // Assert if we need to extend our internal range of SH registers we actually set.
    PAL_ASSERT((isShReg == false) || ((regAddr - PERSISTENT_SPACE_START) < ShRegUsedRangeSize));

    return isShReg;
}

// =====================================================================================================================
// True if the specified register is in a privileged register space.
bool CmdUtil::IsPrivilegedConfigReg(
    uint32 regAddr
    ) const
{
    bool isPrivReg = false;

    if (m_chipFamily == GfxIpLevel::GfxIp6)
    {
        constexpr uint32 SiMcRegSpaceStart = 0x0800;
        constexpr uint32 SiMcRegSpaceEnd   = 0x0BFF;
        // On Gfx6, all of the config registers we care about are non-privileged except ones in the MC aperture range
        isPrivReg = ((regAddr >= SiMcRegSpaceStart) && (regAddr <= SiMcRegSpaceEnd));
    }
    else
    {
        // On Gfx7+, any config register which is not in the user-config space is considered privileged.
        isPrivReg = (IsUserConfigReg(regAddr) == false);
    }

    return isPrivReg;
}

// =====================================================================================================================
// True if the specified atomic operation acts on 32-bit values.
bool CmdUtil::Is32BitAtomicOp(
    AtomicOp atomicOp
    ) const
{
    // AddInt64 is the first 64-bit operation.
    return (static_cast<int32>(atomicOp) < static_cast<int32>(AtomicOp::AddInt64));
}

// =====================================================================================================================
// Converts AtomicOp values into their corresponding TC_OP values. The caller must verify that AtomicOp is valid!
TC_OP CmdUtil::TranslateAtomicOp(
    AtomicOp atomicOp
    ) const
{
    const TC_OP* pConvert = (m_chipFamily == GfxIpLevel::GfxIp6) ? Gfx6AtomicOpConversionTable :
                                                                   Gfx7AtomicOpConversionTable;
    // AddInt32 is the first AtomicOp enum value
    return pConvert[static_cast<int32>(atomicOp)];
}

// =====================================================================================================================
// Builds a PM4 packet which issues an ACQUIRE_MEM command.  Only available on Gfx7+ compute queues. Returns the size of
// the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildAcquireMem(
    regCP_COHER_CNTL cpCoherCntl,  // CP coher_cntl value (controls which sync actions occur).
    gpusize          baseAddress,  // Base address for sync. Set to 0 for full sync.
    gpusize          sizeBytes,    // Size of sync range in bytes.  Set to all Fs for full sync.
    void*            pBuffer       // [out] Build the PM4 packet in this buffer.
    ) const
{
    PAL_ASSERT(m_chipFamily != GfxIpLevel::GfxIp6);

    constexpr size_t PacketSize = PM4_CMD_ACQUIRE_MEM_DWORDS;
    auto*const       pPacket    = static_cast<PM4ACQUIREMEM*>(pBuffer);

    if ((m_chipFamily >= GfxIpLevel::GfxIp8) && (cpCoherCntl.bits.TC_ACTION_ENA == 1))
    {
        // On Gfx8, TC_WB_ACTION_ENA__CI__VI must go together with the TC_ACTION_ENA bit to flush and invalidate the
        // L2 cache.
        cpCoherCntl.bits.TC_WB_ACTION_ENA__CI__VI = 1;
    }

    pPacket->header.u32All = Type3Header(IT_ACQUIRE_MEM__CI__VI, PacketSize);
    pPacket->coherCntl     = cpCoherCntl.u32All;
    pPacket->engine        = 0;

    // Need to align-down the given base address and then add the difference to the size, and align that new size.
    // Note that if sizeBytes is equal to FullSyncSize we should clamp it to the max virtual address.
    constexpr gpusize Alignment = 256;
    constexpr gpusize SizeShift = 8;

    const gpusize alignedAddress = Pow2AlignDown(baseAddress, Alignment);
    const gpusize alignedSize    = (sizeBytes == FullSyncSize)
                                        ? m_device.MemoryProperties().vaUsableEnd
                                        : Pow2Align((sizeBytes + (baseAddress - alignedAddress)), Alignment);

    pPacket->coherSize    = static_cast<uint32>(alignedSize >> SizeShift);
    pPacket->ordinal4     = 0;
    pPacket->coherSizeHi  = static_cast<uint32>(alignedSize >> 40);

    pPacket->coherBaseLo  = Get256BAddrLo(alignedAddress);
    pPacket->ordinal6     = 0;
    pPacket->coherBaseHi  = Get256BAddrHi(alignedAddress);

    pPacket->ordinal7     = 0;
    pPacket->pollInterval = Pal::Device::PollInterval;

    return PacketSize;
}

// =====================================================================================================================
// Builds an ATOMIC_MEM packet. The caller should make sure that atomicOp is valid. This method assumes that pPacket has
// been initialized to zeros. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildAtomicMem(
    AtomicOp atomicOp,
    gpusize  dstMemAddr,
    uint64   srcData,    // Constant operand for the atomic operation.
    void*    pBuffer     // [out] Build the PM4 packet in this buffer.
    ) const
{
    // The destination address must be aligned to the size of the operands.
    PAL_ASSERT((dstMemAddr != 0) && IsPow2Aligned(dstMemAddr, (Is32BitAtomicOp(atomicOp) ? 4 : 8)));

    constexpr size_t    PacketSize = PM4_CMD_ATOMIC_DWORDS;
    auto*const          pPacket    = static_cast<PM4CMDATOMIC*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_ATOMIC, PacketSize);
    pPacket->ordinal2      = 0;
    pPacket->atomOp        = TranslateAtomicOp(atomicOp);
    pPacket->addressLo     = LowPart(dstMemAddr);
    pPacket->addressHi     = HighPart(dstMemAddr);
    pPacket->srcDataLo     = LowPart(srcData);
    pPacket->srcDataHi     = HighPart(srcData);
    pPacket->ordinal7      = 0;
    pPacket->ordinal8      = 0;
    pPacket->ordinal9      = 0;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a clear state command. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildClearState(
    void* pBuffer // [out] Build the PM4 packet in this buffer.
    ) const
{
    constexpr size_t PacketSize = PM4_CMD_CLEAR_STATE_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDCLEARSTATE*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_CLEAR_STATE, PacketSize);
    pPacket->dummyData     = 0;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a cond exec command. Returns the size of the PM4 command assembled, in DWORDs
size_t CmdUtil::BuildCondExec(
    gpusize gpuVirtAddr,
    uint32  sizeInDwords,
    void*   pBuffer       // [out] Build the PM4 packet in this buffer.
    ) const
{
    size_t packetSize = 0;

    if (m_chipFamily == GfxIpLevel::GfxIp6)
    {
        packetSize = PM4_CMD_COND_EXEC_DWORDS;
        auto*const pPacket = static_cast<PM4CMDCONDEXEC*>(pBuffer);

        pPacket->header.u32All = Type3Header(IT_COND_EXEC, packetSize);
        pPacket->boolAddrLo    = LowPart(gpuVirtAddr);
        pPacket->ordinal3      = 0;
        pPacket->boolAddrHi    = HighPart(gpuVirtAddr);
        //pPacket->command       = 0; // 0 == discard, set by ordinal3 = 0
        pPacket->ordinal4      = 0;
        pPacket->execCount     = sizeInDwords;
    }
    else
    {
        packetSize = PM4_CMD_COND_EXEC_CI_DWORDS;
        auto*const pPacket = static_cast<PM4CMDCONDEXEC_CI*>(pBuffer);

        pPacket->header.u32All = Type3Header(IT_COND_EXEC, packetSize);
        pPacket->boolAddrLo    = LowPart(gpuVirtAddr);
        pPacket->boolAddrHi32  = HighPart(gpuVirtAddr);
        pPacket->ordinal4      = 0;
        //pPacket->control       = 0; // 0 == discard, set by ordinal4 = 0
        pPacket->ordinal5      = 0;
        pPacket->execCount     = sizeInDwords;
    }

    return packetSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a cond IB command. This function doesn't take arguments for the pass/fail indirect
// buffer locations because in practice we never know those details when we build this packet. Returns the size of the
// PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildCondIndirectBuffer(
    CompareFunc compareFunc,
    gpusize     compareGpuAddr,
    uint64      reference,
    uint64      mask,
    bool        constantEngine,
    void*       pBuffer         // [out] Build the PM4 packet in this buffer.
    ) const
{
    // This packet doesn't support a function equivalent to CompareFunc::Never. The caller should detect this case and
    // use CompareFunc::Always instead, swapping the values for the indirect buffer locations.
    PAL_ASSERT(compareFunc != CompareFunc::Never);

    constexpr size_t    PacketSize = PM4_CMD_COND_INDIRECT_BUFFER_DWORDS;
    auto*const          pPacket    = static_cast<PM4CMDCONDINDIRECTBUFFER*>(pBuffer);
    const IT_OpCodeType opCode     = constantEngine ? IT_INDIRECT_BUFFER_CNST : IT_COND_INDIRECT_BUFFER;

    pPacket->header.u32All = Type3Header(opCode, PacketSize);
    pPacket->ordinal2      = 0;
    pPacket->mode          = COND_INDIRECT_BUFFER_MODE_IF_ELSE;
    pPacket->function      = CondIbFuncFromCompareType(compareFunc);
    pPacket->compareAddrLo = LowPart(compareGpuAddr);
    pPacket->ordinal4      = 0;
    pPacket->compareAddrHi = HighPart(compareGpuAddr);
    pPacket->maskLo        = LowPart(mask);
    pPacket->maskHi        = HighPart(mask);
    pPacket->referenceLo   = LowPart(reference);
    pPacket->referenceHi   = HighPart(reference);
    pPacket->ordinal9      = 0;
    pPacket->ordinal10     = 0;
    pPacket->ordinal11     = 0;
    pPacket->ordinal12     = 0;
    pPacket->ordinal13     = 0;
    pPacket->ordinal14     = 0;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a context control command. Returns the size of the PM4 command assembled, in DWORDs
size_t CmdUtil::BuildContextControl(
    CONTEXT_CONTROL_ENABLE loadBits,
    CONTEXT_CONTROL_ENABLE shadowBits,
    void*                  pBuffer     // [out] Build the PM4 packet in this buffer.
    ) const
{
    constexpr size_t PacketSize = PM4_CMD_CONTEXT_CTL_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDCONTEXTCONTROL*>(pBuffer);

    pPacket->header.u32All       = Type3Header(IT_CONTEXT_CONTROL, PacketSize);
    pPacket->loadControl.u32All  = loadBits.u32All;
    pPacket->shadowEnable.u32All = shadowBits.u32All;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which reads a context register, masks off a portion of it, then writes the provided data to the
// masked off fields. The register mask applies to the fields being written to, as follows:
//      newRegVal = (oldRegVal & ~regMask) | (regData & regMask)
// Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildContextRegRmw(
    uint32 regAddr,
    uint32 regMask,
    uint32 regData,
    void*  pBuffer  // [out] Build the PM4 packet in this buffer.
    ) const
{
#if PAL_ENABLE_PRINTS_ASSERTS
    CheckShadowedContextReg(regAddr);
#endif

    constexpr size_t PacketSize = PM4_CONTEXT_REG_RMW_DWORDS;
    auto*const       pPacket    = static_cast<PM4CONTEXTREGRMW*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_CONTEXT_REG_RMW, PacketSize);
    pPacket->ordinal2      = 0;
    pPacket->regOffset     = regAddr - CONTEXT_SPACE_START;
    pPacket->regMask       = regMask;
    pPacket->regData       = regData;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which reads a config register, and performs immediate mode AND and OR operations on the regVal
// using the masks provided as follows:
//     newRegVal = (oldRegVal & andMask) | (orMask)
// Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildRegRmw(
    uint32 regAddr,
    uint32 orMask,
    uint32 andMask,
    void*  pBuffer  // [out] Build the PM4 packet in this buffer.
    ) const
{
    PAL_ASSERT(((m_chipFamily == GfxIpLevel::GfxIp6) && IsConfigReg(regAddr)) ||
               ((m_chipFamily >= GfxIpLevel::GfxIp7) && IsUserConfigReg(regAddr)));

    constexpr size_t PacketSize = PM4_CMD_REG_RMW_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDREGRMW*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_REG_RMW, PacketSize);
    pPacket->ordinal2      = 0;
    pPacket->modAdrs       = regAddr;
    //pPacket->orMaskSrc     = 0; // set by ordinal2 = 0
    //pPacket->andMaskSrc    = 0; // set by ordinal2 = 0
    pPacket->orMask        = orMask;
    pPacket->andMask       = andMask;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which performs a CPDMA transfer (Gfx6). Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildCpDmaInternal(
    const DmaDataInfo& dmaData,
    void*              pBuffer  // [out] Build the PM4 packet in this buffer.
    ) const
{
    PAL_ASSERT(m_chipFamily == GfxIpLevel::GfxIp6); // CP_DMA is deprecated after Gfx6

    // The "byteCount" field only has 21 bits (numBytes must be less than 2MB).
    PAL_ASSERT(dmaData.numBytes < (1 << 21));

    // L2 DMAs are not supported by this packet.
    PAL_ASSERT(dmaData.srcSel != CPDMA_SRC_SEL_SRC_ADDR_USING_L2);

    constexpr size_t PacketSize = PM4_CMD_CP_DMA_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDCPDMA*>(pBuffer);

    pPacket->header.u32All     = Type3Header(IT_CP_DMA, PacketSize, ShaderGraphics, dmaData.predicate);
    pPacket->ordinal3          = 0;
    pPacket->dstSel            = dmaData.dstSel;
    pPacket->engine            = dmaData.usePfp ? CP_DMA_ENGINE_PFP : CP_DMA_ENGINE_ME;
    pPacket->srcSel            = dmaData.srcSel;
    pPacket->cpSync            = dmaData.sync;
    pPacket->dstAddrLo         = LowPart(dmaData.dstAddr);
    pPacket->dstAddrHi         = HighPart(dmaData.dstAddr);
    pPacket->ordinal6          = 0;
    pPacket->command.byteCount = dmaData.numBytes;
    pPacket->command.disWc     = dmaData.disableWc;

    if (dmaData.srcSel == CPDMA_SRC_SEL_DATA)
    {
        pPacket->ordinal2 = dmaData.srcData;
    }
    else if (dmaData.srcSel == CPDMA_SRC_SEL_GDS)
    {
        // GDS offset is provided in srcData field.
        pPacket->srcAddrLo            = dmaData.srcData;
        pPacket->command.srcAddrSpace = CPDMA_ADDR_SPACE_REG;
    }
    else
    {
        pPacket->srcAddrLo            = LowPart(dmaData.srcAddr);
        pPacket->srcAddrHi            = HighPart(dmaData.srcAddr);
        pPacket->command.srcAddrSpace = dmaData.srcAddrSpace;
    }

    pPacket->command.dstAddrSpace = (dmaData.dstSel == CPDMA_DST_SEL_GDS) ? CPDMA_ADDR_SPACE_REG : dmaData.dstAddrSpace;
    pPacket->command.srcAddrInc   = (pPacket->command.srcAddrSpace != CPDMA_ADDR_SPACE_MEM);
    pPacket->command.dstAddrInc   = (pPacket->command.dstAddrSpace != CPDMA_ADDR_SPACE_MEM);

    return PacketSize;
}

// =====================================================================================================================
// Builds a COPY_DATA packet. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildCopyData(
    uint32  dstSel,    // Destination selection, one of COPY_DATA_SEL_*
    gpusize dstAddr,
    uint32  srcSel,    // Source selection, one of COPY_DATA_SEL_*
    gpusize srcAddr,   // Source address (or value) of the copy, possibly ignored based on value of srcSel.
    uint32  countSel,  // Count selection, one of COPY_DATA_SEL_COUNT_*
    uint32  engineSel, // Engine selection, one of COPY_DATA_ENGINE_
    uint32  wrConfirm, // Write confirmation, one of COPY_DATA_WR_CONFIRM_*
    void*   pBuffer    // [out] Build the PM4 packet in this buffer.
    ) const
{
    // We can't read or write to a privileged register using COPY_DATA_SEL_REG. Note that there is a backdoor to get
    // around this: COPY_DATA_SEL_[SRC|DST]_SYNC_MEMORY. This backdoor is meant for perf counters but might work on
    // other registers.
    PAL_ASSERT((dstSel != COPY_DATA_SEL_REG) || (IsPrivilegedConfigReg(LowPart(dstAddr)) == false));
    PAL_ASSERT((srcSel != COPY_DATA_SEL_REG) || (IsPrivilegedConfigReg(LowPart(srcAddr)) == false));

    PAL_ASSERT((countSel  == COPY_DATA_SEL_COUNT_1DW)      || (countSel  == COPY_DATA_SEL_COUNT_2DW));
    PAL_ASSERT((wrConfirm == COPY_DATA_WR_CONFIRM_NO_WAIT) || (wrConfirm == COPY_DATA_WR_CONFIRM_WAIT));

    constexpr size_t PacketSize = PM4_CMD_COPY_DATA_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDCOPYDATA*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_COPY_DATA, PacketSize);
    pPacket->ordinal2      = 0;
    pPacket->srcSel        = srcSel;
    pPacket->dstSel        = dstSel;
    pPacket->countSel      = countSel;
    pPacket->wrConfirm     = wrConfirm;
    pPacket->engineSel     = engineSel;
    pPacket->srcAddressLo  = LowPart(srcAddr);
    pPacket->srcAddressHi  = HighPart(srcAddr);
    pPacket->dstAddressLo  = LowPart(dstAddr);
    pPacket->dstAddressHi  = HighPart(dstAddr);

    return PacketSize;
}

// =====================================================================================================================
// Builds a DISPATCH_DIRECT packet. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildDispatchDirect(
    uint32       xDim,              // Thread groups (or threads) to launch (X dimension).
    uint32       yDim,              // Thread groups (or threads) to launch (Y dimension).
    uint32       zDim,              // Thread groups (or threads) to launch (Z dimension).
    bool         dimInThreads,      // X/Y/Z dimensions are in unit of threads if true.
    bool         forceStartAt000,   // Forces COMPUTE_START_X/Y/Z at (0, 0, 0)
    PM4Predicate predicate,
    void*        pBuffer            // [out] Build the PM4 packet in this buffer.
    ) const
{
    // Gfx6 does not support dispatch in threads and we don't expect to do so.
    PAL_ASSERT((dimInThreads == false) || (m_chipFamily != GfxIpLevel::GfxIp6));

    constexpr size_t PacketSize = PM4_CMD_DISPATCH_DIRECT_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDDISPATCHDIRECT*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_DISPATCH_DIRECT, PacketSize, ShaderCompute, predicate);
    pPacket->dimX          = xDim;
    pPacket->dimY          = yDim;
    pPacket->dimZ          = zDim;

    pPacket->dispatchInitiator.u32All                             = 0;
    pPacket->dispatchInitiator.bits.COMPUTE_SHADER_EN             = 1;
    pPacket->dispatchInitiator.bits.USE_THREAD_DIMENSIONS__CI__VI = dimInThreads;
    pPacket->dispatchInitiator.bits.FORCE_START_AT_000            = forceStartAt000;

    // Set unordered mode to allow waves launch faster. This bit is related to the QoS (Quality of service) feature and
    // should be safe to set by default as the feature gets enabled only when allowed by the KMD. This bit also only
    // applies to asynchronous compute pipe and the graphics pipe simply ignores it.
    pPacket->dispatchInitiator.bits.ORDER_MODE__CI__VI            = 1;

    return PacketSize;
}

// =====================================================================================================================
// Builds a DISPATCH_INDIRECT packet. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildDispatchIndirect(
    gpusize      offset, // Byte offset to the indirect args data.
    PM4Predicate predicate,
    void*        pBuffer // [out] Build the PM4 packet in this buffer.
    ) const
{
    // Dispatch argument offset in the buffer has to be 4-byte aligned. The offset's high part is unused.
    PAL_ASSERT(IsPow2Aligned(offset, 4) && (HighPart(offset) == 0));

    constexpr size_t PacketSize = PM4_CMD_DISPATCH_INDIRECT_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDDISPATCHINDIRECT*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_DISPATCH_INDIRECT, PacketSize, ShaderCompute, predicate);
    pPacket->dataOffset    = LowPart(offset);

    pPacket->dispatchInitiator.u32All                  = 0;
    pPacket->dispatchInitiator.bits.COMPUTE_SHADER_EN  = 1;
    pPacket->dispatchInitiator.bits.FORCE_START_AT_000 = 1;
    pPacket->dispatchInitiator.bits.ORDER_MODE__CI__VI = 1;

    return PacketSize;
}

// =====================================================================================================================
// Builds a DISPATCH_INDIRECT packet for the MEC. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildDispatchIndirectMec(
    gpusize address, // Address of the indirect args data.
    void*   pBuffer  // [out] Build the PM4 packet in this buffer.
    ) const
{
    // This is only supported on Gfx7+ and the dispatch argument buffer address has to be 4-byte aligned.
    PAL_ASSERT((m_chipFamily >= GfxIpLevel::GfxIp7) && IsPow2Aligned(address, 4));

    constexpr size_t PacketSize = PM4_CMD_DISPATCH_INDIRECT_MEC_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDDISPATCHINDIRECTMEC*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_DISPATCH_INDIRECT, PacketSize, ShaderCompute);
    pPacket->addressLo     = LowPart(address);
    pPacket->addressHi     = HighPart(address);

    pPacket->dispatchInitiator.u32All                  = 0;
    pPacket->dispatchInitiator.bits.COMPUTE_SHADER_EN  = 1;
    pPacket->dispatchInitiator.bits.FORCE_START_AT_000 = 1;
    pPacket->dispatchInitiator.bits.ORDER_MODE__CI__VI = 1;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which performs a CP DMA transfer. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildDmaData(
    const DmaDataInfo& dmaData,
    void*              pBuffer  // [out] Build the PM4 packet in this buffer.
    ) const
{
    size_t packetSize = 0;

    // This is to workaround a HW bug in CP DMA.
    //      When the DMA source address is not 32-byte aligned, the performance of current DMA packet will be low. And
    //      when the DMA size is not 32-byte aligned, the following DMA packets will be in low performance until the
    //      accumalated DMA size is 32-byte aligned again.
    // So the workaround is:
    // 1) If the source address is 32B aligned but size is not.
    // - Submit the DMA packet followed by a patch packet copying (32 - (originalNumBytes % 32).
    // 2) If the source address is not 32B aligned.
    // - If the size < 512 bytes, submit the DMA packet directly and apply case 1) if size is not 32B aligned.
    // - If the size >= 512 bytes, split the DMA packet into one "body" packet starting from the
    //   (32 - originalSrcAddr % 32) offset to originalSrcAddr, and one "head" packet having
    //   (32 - originalSrcAddr % 32) bytes. Submit the "body" packet first then the "head" packet. Finally apply case
    //   1) if size is not 32B aligned.
    //
    // Note that DMAs with a GDS source don't need the source address to be aligned.
    //
    // This bug has been worked around in the CP microcode on some ASICs.

    const uint32 alignment = GetGfx6Settings(m_device).cpDmaSrcAlignment;

    // Do a quick check to eliminate some cases that will never need this workaround.
    if ((alignment != CpDmaAlignmentDefault) && (dmaData.srcSel != CPDMA_SRC_SEL_DATA))
    {
        // Compute the number of bytes needed to align the source address.
        const uint32 addrAlignUp = static_cast<uint32>(Pow2Align(dmaData.srcAddr, alignment) - dmaData.srcAddr);

        // Evaluate the case which requires us to split the DMA into a "head" DMA and a "body" DMA. As stated in the
        // main comment, this requires an unaligned source address, a size of at least 512 bytes, and a non-GDS source.
        if ((addrAlignUp > 0) && (dmaData.numBytes >= 512) && (dmaData.srcSel != CPDMA_SRC_SEL_GDS))
        {
            DmaDataInfo body      = dmaData;
            DmaDataInfo splitHead = dmaData;

            // The "head" packet starts from the original srcAddr and has addrAlignUp size.
            splitHead.numBytes = addrAlignUp;

            // Adjust the remaining body packet by addrAlignUp.
            body.srcAddr  += addrAlignUp;
            body.dstAddr  += addrAlignUp;
            body.numBytes -= addrAlignUp;
            body.sync      = false;
            body.disableWc = true;

            // Issue "body" packet first, then the "head" packet.
            packetSize  = BuildGenericDmaDataInternal(body, pBuffer);
            packetSize += BuildGenericDmaDataInternal(splitHead, static_cast<uint32*>(pBuffer) + packetSize);
        }
        else
        {
            // We must submit the unmodified DMA request if:
            // - The address is aligned.
            // - The address is not aligned but the size is less than 512 bytes.
            // - The source select is GDS (no alignment is required).
            packetSize = BuildGenericDmaDataInternal(dmaData, pBuffer);
        }

        // In all cases we need to issue the size fix-up packet if the size is not aligned.
        const uint32 sizeAlignUp = Pow2Align(dmaData.numBytes, alignment) - dmaData.numBytes;

        if (sizeAlignUp > 0)
        {
            packetSize += BuildDmaDataSizeFixup(sizeAlignUp, static_cast<uint32*>(pBuffer) + packetSize);
        }
    }
    else
    {
        // Just write the DMA that the caller asked for.
        packetSize = BuildGenericDmaDataInternal(dmaData, pBuffer);
    }

    return packetSize;
}

// =====================================================================================================================
// Builds a PM4 packet which performs a CP DMA transfer. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildDmaDataInternal(
    const DmaDataInfo& dmaData,
    void*              pBuffer  // [out] Build the PM4 packet in this buffer.
    ) const
{
    PAL_ASSERT(m_chipFamily != GfxIpLevel::GfxIp6); // DMA_DATA is only valid on Gfx7 and newer!

    // The "byteCount" field only has 21 bits (numBytes must be less than 2MB).
    PAL_ASSERT(dmaData.numBytes < (1 << 21));

    constexpr size_t PacketSize = PM4_CMD_DMA_DATA_DWORDS;
    auto*const       pPacket    = static_cast<PM4DMADATA*>(pBuffer);

    // When buildng the packet directly in the command buffer with pPacket, the code the compiler generated was
    // reading from the uncached command buffer.  Building the packet in a local variable and then copying the
    // local variable to the command buffer avoids reading from uncached memory.
    PM4DMADATA pkt;

    pkt.header.u32All = Type3Header(IT_DMA_DATA__CI__VI, PacketSize, ShaderGraphics, dmaData.predicate);
    pkt.ordinal2      = 0;
    pkt.engine        = dmaData.usePfp ? CP_DMA_ENGINE_PFP : CP_DMA_ENGINE_ME;
    pkt.dstSel        = dmaData.dstSel;
    pkt.srcSel        = dmaData.srcSel;
    pkt.cpSync        = dmaData.sync;

    // Both the GDS offset and memory address are stored in dstAddr and in both cases should be written to the
    // dstAddrLo/Hi fields.
    pkt.dstAddrLo     = LowPart(dmaData.dstAddr);
    pkt.dstAddrHi     = HighPart(dmaData.dstAddr);

    pkt.ordinal7      = 0;
    pkt.byteCount     = dmaData.numBytes;
    pkt.disWC         = dmaData.disableWc;

    if (dmaData.srcSel == CPDMA_SRC_SEL_DATA)
    {
        pkt.data     = dmaData.srcData;
        pkt.ordinal4 = 0;
    }
    else if (dmaData.srcSel == CPDMA_SRC_SEL_GDS)
    {
        // GDS offset is provided in srcData field.
        pkt.srcAddrLo = dmaData.srcData;
        pkt.ordinal4  = 0;
    }
    else
    {
        pkt.srcAddrLo = LowPart(dmaData.srcAddr);
        pkt.srcAddrHi = HighPart(dmaData.srcAddr);
        pkt.sas       = dmaData.srcAddrSpace;
        pkt.saic      = (dmaData.srcAddrSpace != CPDMA_ADDR_SPACE_MEM);
    }

    if (dmaData.dstSel == CPDMA_DST_SEL_DST_ADDR)
    {
        pkt.das  = dmaData.dstAddrSpace;
        pkt.daic = (dmaData.dstAddrSpace != CPDMA_ADDR_SPACE_MEM);
    }

    *pPacket = pkt;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which performs a CP DMA transfer of patch memory to realign the DMA size. Returns the size of the
// PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildDmaDataSizeFixup(
    uint32 sizeInBytes,
    void*  pBuffer      // [out] Build the PM4 packet in this buffer.
    ) const
{
    const bool usingL2    = (m_chipFamily != GfxIpLevel::GfxIp6);
    const Pal::Gfx6::Device& gfx6Device = static_cast<const Pal::Gfx6::Device&>(*m_device.GetGfxDevice());
    const bool usingL2Dst = usingL2 && (gfx6Device.WaCpDmaHangMcTcAckDrop() == false);

    DmaDataInfo sizeFixup = {};

    sizeFixup.srcAddr      = static_cast<Device*>(m_device.GetGfxDevice())->CpDmaPatchMem().GpuVirtAddr();
    sizeFixup.srcSel       = usingL2 ? CPDMA_SRC_SEL_SRC_ADDR_USING_L2 : CPDMA_SRC_SEL_SRC_ADDR;
    sizeFixup.srcAddrSpace = CPDMA_ADDR_SPACE_MEM;
    sizeFixup.dstAddr      = sizeFixup.srcAddr;
    sizeFixup.dstSel       = usingL2Dst ? CPDMA_DST_SEL_DST_ADDR_USING_L2 : CPDMA_DST_SEL_DST_ADDR;
    sizeFixup.dstAddrSpace = sizeFixup.srcAddrSpace;
    sizeFixup.numBytes     = sizeInBytes;
    sizeFixup.disableWc    = true;

    return BuildGenericDmaDataInternal(sizeFixup, pBuffer);
}

// =====================================================================================================================
// Builds a PM4 packet which issues an indexed draw using IT_DRAW_INDEX_2. Returns the size of the PM4 command
// assembled, in DWORDs.
size_t CmdUtil::BuildDrawIndex2(
    uint32       indexCount,
    uint32       indexBufSize,
    gpusize      indexBufAddr,
    PM4Predicate predicate,
    void*        pBuffer
    ) const
{
    constexpr size_t PacketSize = PM4_CMD_DRAW_INDEX_2_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDDRAWINDEX2*>(pBuffer);

    // Workaround for Gfx6 bug This is a DMA clamping bug that occurs when both the DMA base address (word aligned) is
    // zero and DMA_MAX_SIZE is zero.The max address used to determine when to start clamping underflows and therefore
    // the logic thinks it should start clamping at word address 0xFF FFFF FFFF (DMA Last Max Word Address).
    // assign dma_max_word_addr_d = rbiu_dma_base + dma_max_num_words - 1
    // Setting the IB addr to 2 or higher solves this issue
    if ((indexBufAddr == 0x0) &&
        (static_cast<const Pal::Gfx6::Device*>(m_device.GetGfxDevice())->WaMiscNullIb() == true))
    {
        indexBufAddr = 0x2;
    }

    pPacket->header.u32All = Type3Header(IT_DRAW_INDEX_2, PacketSize, ShaderGraphics, predicate);
    pPacket->maxSize       = indexBufSize;
    pPacket->indexBaseLo   = LowPart(indexBufAddr);
    pPacket->indexBaseHi   = HighPart(indexBufAddr);
    pPacket->indexCount    = indexCount;

    pPacket->drawInitiator.u32All             = 0;
    pPacket->drawInitiator.bits.SOURCE_SELECT = DI_SRC_SEL_DMA;
    pPacket->drawInitiator.bits.MAJOR_MODE    = DI_MAJOR_MODE_0;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a non-indexed draw. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildDrawIndexAuto(
    uint32       indexCount,
    bool         useOpaque,
    PM4Predicate predicate,
    void*        pBuffer     // [out] Build the PM4 packet in this buffer.
    ) const
{
    PAL_ASSERT((indexCount == 0) || (useOpaque == false));

    constexpr size_t PacketSize = PM4_CMD_DRAW_INDEX_AUTO_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDDRAWINDEXAUTO*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_DRAW_INDEX_AUTO, PacketSize, ShaderGraphics, predicate);
    pPacket->indexCount    = indexCount;

    pPacket->drawInitiator.u32All             = 0;
    pPacket->drawInitiator.bits.SOURCE_SELECT = DI_SRC_SEL_AUTO_INDEX;
    pPacket->drawInitiator.bits.MAJOR_MODE    = DI_MAJOR_MODE_0;
    pPacket->drawInitiator.bits.USE_OPAQUE    = useOpaque ? 1 : 0;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a multi indexed, indirect draw command into the given DE command stream. Returns the
// size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildDrawIndexIndirect(
    gpusize      offset,       // Byte offset to the indirect args data.
    uint32       baseVtxLoc,   // Register VS expects to read baseVtxLoc from.
    uint32       startInstLoc, // Register VS expects to read startInstLoc from.
    PM4Predicate predicate,
    void*        pBuffer       // [out] Build the PM4 packet in this buffer.
    ) const
{
    // Draw argument offset in the buffer has to be 4-byte aligned.
    PAL_ASSERT(IsPow2Aligned(offset, 4));

    constexpr size_t PacketSize = PM4_CMD_DRAW_INDEX_INDIRECT_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDDRAWINDEXINDIRECT*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_DRAW_INDEX_INDIRECT, PacketSize, ShaderGraphics, predicate);
    pPacket->dataOffset    = LowPart(offset);
    pPacket->ordinal3      = 0;
    pPacket->baseVtxLoc    = baseVtxLoc - PERSISTENT_SPACE_START;
    pPacket->ordinal4      = 0;
    pPacket->startInstLoc  = startInstLoc - PERSISTENT_SPACE_START;

    pPacket->drawInitiator.u32All             = 0;
    pPacket->drawInitiator.bits.SOURCE_SELECT = DI_SRC_SEL_DMA;
    pPacket->drawInitiator.bits.MAJOR_MODE    = DI_MAJOR_MODE_0;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues an indexed, indirect draw command into the given DE command stream. Returns the size
// of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildDrawIndexIndirectMulti(
    gpusize      offset,       // Byte offset to the indirect args data.
    uint32       baseVtxLoc,   // Register VS expects to read baseVtxLoc from.
    uint32       startInstLoc, // Register VS expects to read startInstLoc from.
    uint32       drawIndexLoc, // Register VS expects to read drawIndex from.
    uint32       stride,       // Stride from one indirect args data structure to the next.
    uint32       count,        // Number of draw calls to loop through, or max draw calls if count is in GPU memory.
    gpusize      countGpuAddr, // GPU address containing the count.
    PM4Predicate predicate,
    void*        pBuffer       // [out] Build the PM4 packet in this buffer.
    ) const
{
    // Draw argument offset in the buffer has to be 4-byte aligned.
    PAL_ASSERT(IsPow2Aligned(offset, 4));

    size_t bytesWritten;

    constexpr size_t PacketSize = PM4_CMD_DRAW_INDEX_INDIRECT_MULTI_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDDRAWINDEXINDIRECTMULTI*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_DRAW_INDEX_INDIRECT_MULTI, PacketSize, ShaderGraphics, predicate);
    pPacket->dataOffset    = LowPart(offset);
    pPacket->ordinal3      = 0;
    pPacket->baseVtxLoc    = baseVtxLoc - PERSISTENT_SPACE_START;
    pPacket->ordinal4      = 0;
    pPacket->startInstLoc  = startInstLoc - PERSISTENT_SPACE_START;
    pPacket->ordinal5      = 0;

    if (drawIndexLoc != UserDataNotMapped)
    {
        pPacket->drawIndexEnable = 1;
        pPacket->drawIndexLoc    = drawIndexLoc - PERSISTENT_SPACE_START;
    }

    if (countGpuAddr != 0)
    {
        pPacket->countIndirectEnable = 1;
        pPacket->countAddrLo         = LowPart(countGpuAddr);
        pPacket->countAddrHi         = HighPart(countGpuAddr);
    }
    else
    {
        pPacket->countAddrLo = 0;
        pPacket->countAddrHi = 0;
    }

    pPacket->count  = count;
    pPacket->stride = stride;

    pPacket->drawInitiator.u32All             = 0;
    pPacket->drawInitiator.bits.SOURCE_SELECT = DI_SRC_SEL_DMA;
    pPacket->drawInitiator.bits.MAJOR_MODE    = DI_MAJOR_MODE_0;

    bytesWritten = PacketSize;

    return bytesWritten;
}

// =====================================================================================================================
// Builds a PM4 packet which issues an indexed draw using IT_DRAW_INDEX_OFFSET_2. Returns the size of the PM4 command
// assembled, in DWORDs.
size_t CmdUtil::BuildDrawIndexOffset2(
    uint32       indexCount,
    uint32       indexBufSize,
    uint32       indexOffset,
    PM4Predicate predicate,
    void*        pBuffer
    ) const
{
    constexpr size_t PacketSize = PM4_CMD_DRAW_INDEX_OFFSET_2_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDDRAWINDEXOFFSET2*>(pBuffer);

    pPacket->header.u32All      = Type3Header(IT_DRAW_INDEX_OFFSET_2, PacketSize, ShaderGraphics, predicate);
    pPacket->maxSize            = indexBufSize;
    pPacket->indexOffset        = indexOffset;
    pPacket->indexCount.u32All  = indexCount;

    pPacket->drawInitiator.u32All             = 0;
    pPacket->drawInitiator.bits.SOURCE_SELECT = DI_SRC_SEL_DMA;
    pPacket->drawInitiator.bits.MAJOR_MODE    = DI_MAJOR_MODE_0;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a draw indirect multi command into the given DE command stream. Returns the size of
// the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildDrawIndirectMulti(
    gpusize      offset,       // Byte offset to the indirect args data.
    uint32       baseVtxLoc,   // Register VS expects to read baseVtxLoc from.
    uint32       startInstLoc, // Register VS expects to read startInstLoc from.
    uint32       drawIndexLoc, // Register VS expects to read drawIndex from.
    uint32       stride,       // Stride from one indirect args data structure to the next.
    uint32       count,        // Number of draw calls to loop through, or max draw calls if count is in GPU memory.
    gpusize      countGpuAddr, // GPU address containing the count.
    PM4Predicate predicate,
    void*        pBuffer       // [out] Build the PM4 packet in this buffer.
    ) const
{
    size_t bytesWritten;

    // Draw argument offset in the buffer has to be 4-byte aligned.
    PAL_ASSERT(IsPow2Aligned(offset, 4));

    constexpr size_t PacketSize = PM4_CMD_DRAW_INDIRECT_MULTI_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDDRAWINDIRECTMULTI*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_DRAW_INDIRECT_MULTI, PacketSize, ShaderGraphics, predicate);
    pPacket->dataOffset    = LowPart(offset);
    pPacket->ordinal3      = 0;
    pPacket->baseVtxLoc    = baseVtxLoc - PERSISTENT_SPACE_START;
    pPacket->ordinal4      = 0;
    pPacket->startInstLoc  = startInstLoc - PERSISTENT_SPACE_START;
    pPacket->ordinal5      = 0;

    if (drawIndexLoc != UserDataNotMapped)
    {
        pPacket->drawIndexEnable = 1;
        pPacket->drawIndexLoc    = drawIndexLoc - PERSISTENT_SPACE_START;
    }

    if (countGpuAddr != 0)
    {
        pPacket->countIndirectEnable = 1;
        pPacket->countAddrLo         = LowPart(countGpuAddr);
        pPacket->countAddrHi         = HighPart(countGpuAddr);
    }
    else
    {
        pPacket->countAddrLo = 0;
        pPacket->countAddrHi = 0;
    }

    pPacket->count  = count;
    pPacket->stride = stride;

    pPacket->drawInitiator.u32All             = 0;
    pPacket->drawInitiator.bits.SOURCE_SELECT = DI_SRC_SEL_AUTO_INDEX;
    pPacket->drawInitiator.bits.MAJOR_MODE    = DI_MAJOR_MODE_0;

    bytesWritten = PacketSize;

    return bytesWritten;
}

// =====================================================================================================================
// Builds a PM4 constant engine command to dump the specified amount of data from CE RAM into GPU memory through the L2
// cache. Returns the size of the PM4 command built, in DWORDs.
size_t CmdUtil::BuildDumpConstRam(
    gpusize dstGpuAddr,
    uint32  ramByteOffset,
    uint32  dwordSize,     // Amount of data to dump, in DWORDs.
    void*   pBuffer        // [out] Build the PM4 packet in this buffer.
   ) const
{
    PAL_ASSERT(IsPow2Aligned(dstGpuAddr, 4));
    PAL_ASSERT(IsPow2Aligned(ramByteOffset, 4));
    PAL_ASSERT(dwordSize != 0);

    constexpr size_t PacketSize = PM4_CMD_DUMP_CONST_RAM_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDCONSTRAMDUMP*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_DUMP_CONST_RAM, PacketSize);
    pPacket->ordinal2      = 0;
    pPacket->offset        = ramByteOffset;
    pPacket->ordinal3      = 0;
    pPacket->numDwords     = dwordSize;
    pPacket->addrLo        = LowPart(dstGpuAddr);
    pPacket->addrHi        = HighPart(dstGpuAddr);

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 constant engine command to dump the specified amount of data from CE RAM to an indirect GPU memory
// address through the L2 cache. Returns the size of the PM4 command built, in DWORDs.
size_t CmdUtil::BuildDumpConstRamOffset(
    uint32  dstAddrOffset,
    uint32  ramByteOffset,
    uint32  dwordSize,     // Amount of data to dump, in DWORDs.
    void*   pBuffer        // [out] Build the PM4 packet in this buffer.
   ) const
{
    // Packet is only supported on GFX8.0+
    PAL_ASSERT(m_chipFamily >= GfxIpLevel::GfxIp8);

    PAL_ASSERT(IsPow2Aligned(dstAddrOffset, 4));
    PAL_ASSERT(IsPow2Aligned(ramByteOffset, 4));
    PAL_ASSERT(dwordSize != 0);

    constexpr size_t PacketSize = PM4_CMD_DUMP_CONST_RAM_OFFSET_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDCONSTRAMDUMPOFFSET*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_DUMP_CONST_RAM_OFFSET__VI, PacketSize);
    pPacket->ordinal2      = 0;
    pPacket->offset        = ramByteOffset;
    pPacket->ordinal3      = 0;
    pPacket->numDwords     = dwordSize;
    pPacket->addrOffset    = dstAddrOffset;

    return PacketSize;
}

// =====================================================================================================================
// Constructs a PM4 packet which issues the specified event. All events work on universal queues but the other queues
// can't process PS_PARTIAL_FLUSH or VS_PARTIAL_FLUSH. Returns the size of the PM4 command built, in DWORDs.
size_t CmdUtil::BuildEventWrite(
    VGT_EVENT_TYPE eventType,
    void*          pBuffer    // [out] Build the PM4 packet in this buffer.
    ) const
{
    constexpr size_t PacketSize = PM4_CMD_WAIT_EVENT_WRITE_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDEVENTWRITE*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_EVENT_WRITE, PacketSize);
    pPacket->ordinal2      = 0;
    pPacket->eventType     = eventType;
    pPacket->eventIndex    = EventIndexFromEventType(eventType);

    if ((eventType == CS_PARTIAL_FLUSH) && (m_chipFamily >= GfxIpLevel::GfxIp7))
    {
        // Set the highest bit of ordinal2 for CS_PARTIAL_FLUSH to offload queue
        // until EOP queue goes empty. This works for MEC introduced from CI+/GfxIp7+,
        // and does nothing on ME/graphics queue or asics without MEC.
        pPacket->offload_enable = 1;
    }

    PAL_ASSERT((pPacket->eventIndex != EVENT_WRITE_INDEX_ANY_EOP_TIMESTAMP) &&
               (pPacket->eventIndex != EVENT_WRITE_INDEX_ANY_EOS_TIMESTAMP));

    return PacketSize;
}

// =====================================================================================================================
// Builds an event-write-eop packet. Returns the size of the PM4 command built, in DWORDs.
size_t CmdUtil::BuildEventWriteEop(
    VGT_EVENT_TYPE eventType,
    gpusize        gpuAddress,
    uint32         dataSel,    // One of the EVENTWRITEEOP_DATA_SEL_* constants
    uint64         data,       // data to write, ignored except for DATA_SEL_SEND_DATA{32,64}
    bool           flushInvL2, // If true, do a full L2 cache flush and invalidate.
    void*          pBuffer     // [out] Build the PM4 packet in this buffer.
    ) const
{
    // Only 16 bits are available for the high address, more than 16 bits is not supported
    PAL_ASSERT ((HighPart(gpuAddress) >> 16) == 0);

    // 32-bit data must be DWORD aligned.
    PAL_ASSERT((dataSel != EVENTWRITEEOP_DATA_SEL_SEND_DATA32) || IsPow2Aligned(gpuAddress, 4));

    // 64-bit data must be QWORD aligned.
    PAL_ASSERT(((dataSel != EVENTWRITEEOP_DATA_SEL_SEND_DATA64) &&
                (dataSel != EVENTWRITEEOP_DATA_SEL_SEND_GPU_CLOCK)) ||
               IsPow2Aligned(gpuAddress, 8));

    // These are the only event types supported by this packet
    PAL_ASSERT ((eventType == BOTTOM_OF_PIPE_TS)        ||
                (eventType == CACHE_FLUSH_TS)           ||
                (eventType == FLUSH_AND_INV_CB_DATA_TS) ||
                (eventType == CACHE_FLUSH_AND_INV_TS_EVENT));

    constexpr size_t IndividualPacketSize = PM4_CMD_WAIT_EVENT_WRITE_EOP_DWORDS;
    size_t           packetSize           = IndividualPacketSize;
    auto*            pPacket              = static_cast<PM4CMDEVENTWRITEEOP*>(pBuffer);

    PM4CMDEVENTWRITEEOP pkt;

    pkt.header.u32All = Type3Header(IT_EVENT_WRITE_EOP, IndividualPacketSize);
    pkt.ordinal2      = 0;
    pkt.eventType     = eventType;
    pkt.eventIndex    = EventIndexFromEventType(eventType);
    if (flushInvL2)
    {
        if (m_chipFamily == GfxIpLevel::GfxIp6)
        {
            pkt.invalidateL2__SI  = 1;
        }
        else if (m_chipFamily >= GfxIpLevel::GfxIp7)
        {
            pkt.tcWbActionEna__CI = 1;
            pkt.tcActionEna__CI   = 1;
        }
    }
    pkt.addressLo     = LowPart(gpuAddress);
    pkt.ordinal4      = 0;
    pkt.addressHi     = HighPart(gpuAddress);
    pkt.dataSel       = dataSel;

    // This won't send an interrupt but will wait for write confirm before writing the data to memory.
    pkt.intSel        = (dataSel == EVENTWRITEEOP_DATA_SEL_DISCARD) ? EVENTWRITEEOP_INT_SEL_NONE
                                                                    : EVENTWRITEEOP_INT_SEL_SEND_DATA_ON_CONFIRM;

    // Fill in data for the workaround first to make sure we write to write combined memory in order.

    if (static_cast<Pal::Gfx6::Device*>(m_device.GetGfxDevice())->WaEventWriteEopPrematureL2Inv() && flushInvL2)
    {
        // We need to issue a dummy packet for this workaround. Simply duplicate the current packet and set the first
        // packet's data fields to some dummy data.
        const uint64 dummyData = data - 1;
        pkt.dataLo = LowPart(dummyData);
        pkt.dataHi = HighPart(dummyData);

        *pPacket = pkt;

        pPacket++;

        packetSize = IndividualPacketSize * 2;
    }

    pkt.dataLo        = LowPart(data);
    pkt.dataHi        = HighPart(data);

    *pPacket = pkt;

    return packetSize;
}

// =====================================================================================================================
// Builds an event-write-eos packet. Returns the size of the PM4 command built, in DWORDs.
size_t CmdUtil::BuildEventWriteEos(
    VGT_EVENT_TYPE eventType,
    gpusize        dstMemAddr,
    uint32         command,
    uint32         data,       // Data to write when event occurs
    uint32         gdsIndex,   // GDS index from start of partition
    uint32         gdsSize,    // Number of DWORDs to read from GDS
    void*          pBuffer     // [out] Build the PM4 packet in this buffer.
    ) const
{
    // Destination address must be DWORD aligned.
    PAL_ASSERT(IsPow2Aligned(dstMemAddr, 4));

    // Only 16 bits are available for the high address, more than 16 bits is not supported
    PAL_ASSERT((HighPart(dstMemAddr) >> 16) == 0);

    // These are the only event types supported by this packet.
    PAL_ASSERT((eventType == CS_DONE) || (eventType == PS_DONE));

    // These are the only commands supported currently.
    PAL_ASSERT((command == EVENT_WRITE_EOS_CMD_STORE_GDS_DATA_TO_MEMORY) ||
               (command == EVENT_WRITE_EOS_CMD_STORE_32BIT_DATA_TO_MEMORY));

    // We can only have a GDS size iff. we have a GDS data selection.
    PAL_ASSERT((command == EVENT_WRITE_EOS_CMD_STORE_GDS_DATA_TO_MEMORY) == (gdsSize > 0));

    size_t     totalSize = PM4_CMD_EVENT_WRITE_EOS_DWORDS;
    auto*const pPacket   = static_cast<PM4CMDEVENTWRITEEOS*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_EVENT_WRITE_EOS, totalSize);
    pPacket->ordinal2      = 0;
    pPacket->eventType     = eventType;
    pPacket->eventIndex    = EVENT_WRITE_INDEX_ANY_EOS_TIMESTAMP;
    pPacket->addressLo     = LowPart(dstMemAddr);
    pPacket->ordinal4      = 0;
    pPacket->addressHi     = HighPart(dstMemAddr);
    pPacket->command       = command;

    if (command == EVENT_WRITE_EOS_CMD_STORE_GDS_DATA_TO_MEMORY)
    {
        pPacket->gdsIndex = gdsIndex;
        pPacket->size     = gdsSize;

        // The CPDMA performance issue affects EVENT_WRITE_EOS if the source is GDS. We only need to patch the GDS size.
        const uint32 sizeInBytes = gdsSize * sizeof(uint32);
        const uint32 alignment   = GetGfx6Settings(m_device).cpDmaSrcAlignment;
        const uint32 fixupSize   = Pow2Align(sizeInBytes, alignment) - sizeInBytes;

        if (fixupSize > 0)
        {
            totalSize += BuildDmaDataSizeFixup(fixupSize, pPacket + 1);
        }
    }
    else
    {
        pPacket->data = data;
    }

    return totalSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues an event write. Returns the size of the PM4 command built, in DWORDs.
size_t CmdUtil::BuildEventWriteQuery(
    VGT_EVENT_TYPE eventType,
    gpusize        address,   // Address in which to write the query results.
    void*          pBuffer    // [out] Build the PM4 packet in this buffer.
    ) const
{
    // Make sure our address is aligned to the packet requirements.
    PAL_ASSERT(IsPow2Aligned(address, 8));

    constexpr size_t PacketSize = PM4_CMD_WAIT_EVENT_WRITE_QUERY_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDEVENTWRITEQUERY*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_EVENT_WRITE, PacketSize);
    pPacket->ordinal2      = 0;
    pPacket->eventType     = eventType;
    pPacket->eventIndex    = EventIndexFromEventType(eventType);
    pPacket->addressLo     = LowPart(address);
    pPacket->addressHi32   = HighPart(address);
    return PacketSize;
}

// =====================================================================================================================
// Builds either a SURFACE_SYNC packet or an ACQUIRE_MEM packet depending on the GFXIP level and which engine will
// execute it. Returns the size of the PM4 command built, in DWORDs.
size_t CmdUtil::BuildGenericSync(
    regCP_COHER_CNTL cpCoherCntl,      // CP coher_cntl value (controls which sync actions occur).
    uint32           syncEngine,       // Sync engine (PFP or ME).
    gpusize          baseAddress,      // Base address for sync. Set to 0 for full sync.
    gpusize          sizeBytes,        // Size of sync range in bytes. Set to all Fs for full sync.
    bool             forComputeEngine,
    void*            pBuffer           // [out] Build the PM4 packet in this buffer.
    ) const
{
    size_t packetSize = 0;

    if (forComputeEngine)
    {
        // Mask cpCoherCntl so that it is restricted to the set of flags that are valid on compute queues.
        cpCoherCntl.u32All &= CpCoherCntlComputeValidMask;

        if (m_chipFamily >= GfxIpLevel::GfxIp7)
        {
            packetSize = BuildAcquireMem(cpCoherCntl, baseAddress, sizeBytes, pBuffer);
        }
        else
        {
            packetSize = BuildSurfaceSync(cpCoherCntl, syncEngine, baseAddress, sizeBytes, pBuffer);
        }
    }
    else
    {
        packetSize = BuildSurfaceSync(cpCoherCntl, syncEngine, baseAddress, sizeBytes, pBuffer);
    }

    return packetSize;
}

// =====================================================================================================================
// Builds either a EVENT_WRITE_EOP packet or a RELEASE_MEM packet depending on the GFXIP level and which engine will
// execute it. Returns the size of the PM4 command built, in DWORDs.
size_t CmdUtil::BuildGenericEopEvent(
    VGT_EVENT_TYPE eventType,
    gpusize        gpuAddress,
    uint32         dataSel,          // One of the EVENTWRITEEOP_DATA_SEL_* constants.
    uint64         data,             // data to write, ignored except for DATA_SEL_SEND_DATA{32,64}.
    bool           forComputeEngine,
    bool           flushInvL2,       // If true, do a full L2 cache flush and invalidate.
    void*          pBuffer           // [out] Build the PM4 packet in this buffer.
    ) const
{
    size_t packetSize = 0;

    if (forComputeEngine && (m_chipFamily >= GfxIpLevel::GfxIp7))
    {
        // Assert that data selects match between event write and release mem.
        static_assert((EVENTWRITEEOP_DATA_SEL_DISCARD        == RELEASEMEM_DATA_SEL_DISCARD) &&
                      (EVENTWRITEEOP_DATA_SEL_SEND_DATA32    == RELEASEMEM_DATA_SEL_SEND_DATA32) &&
                      (EVENTWRITEEOP_DATA_SEL_SEND_DATA64    == RELEASEMEM_DATA_SEL_SEND_DATA64) &&
                      (EVENTWRITEEOP_DATA_SEL_SEND_GPU_CLOCK == RELEASEMEM_DATA_SEL_SEND_GPU_CLOCK),
                      "Data selects do not match between event write and release mem");

        packetSize = BuildReleaseMem(eventType, gpuAddress, dataSel, data, 0, 0, flushInvL2, pBuffer);
    }
    else
    {
        packetSize = BuildEventWriteEop(eventType, gpuAddress, dataSel, data, flushInvL2, pBuffer);
    }

    return packetSize;
}

// =====================================================================================================================
// Builds either a EVENT_WRITE_EOS packet or a RELEASE_MEM packet depending on the GFXIP level and which engine will
// execute it. Returns the size of the PM4 command built, in DWORDs.
size_t CmdUtil::BuildGenericEosEvent(
    VGT_EVENT_TYPE eventType,
    gpusize        dstMemAddr,
    uint32         command,          // One of the EVENT_WRITE_EOS_CMD_* constants.
    uint32         data,             // Data to write when event occurs.
    uint32         gdsIndex,         // GDS index from start of partition.
    uint32         gdsSize,          // Number of DWORDs to read from GDS.
    bool           forComputeEngine,
    void*          pBuffer           // [out] Build the PM4 packet in this buffer.
    ) const
{
    size_t packetSize = 0;

    if (forComputeEngine && (m_chipFamily >= GfxIpLevel::GfxIp7))
    {
        uint32 dataSel = 0;

        // Translate from an EOS command to a release mem command.
        if (command == EVENT_WRITE_EOS_CMD_STORE_GDS_DATA_TO_MEMORY)
        {
            dataSel = RELEASEMEM_DATA_SEL_STORE_GDS_DATA;
        }
        else if (command == EVENT_WRITE_EOS_CMD_STORE_32BIT_DATA_TO_MEMORY)
        {
            dataSel = RELEASEMEM_DATA_SEL_SEND_DATA32;
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }

        packetSize = BuildReleaseMem(eventType, dstMemAddr, dataSel, data, gdsIndex, gdsSize, false, pBuffer);
    }
    else
    {
        packetSize = BuildEventWriteEos(eventType, dstMemAddr, command, data, gdsIndex, gdsSize, pBuffer);
    }

    return packetSize;
}

// =====================================================================================================================
// Builds a PM4 constant engine command to increment the CE counter. Returns the size of the PM4 command built, in
// DWORDs.
size_t CmdUtil::BuildIncrementCeCounter(
    void* pBuffer // [out] Build the PM4 packet in this buffer.
    ) const
{
    constexpr size_t PacketSize = PM4_CMD_INC_CE_COUNTER_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDINCCECOUNTER*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_INCREMENT_CE_COUNTER, PacketSize);
    pPacket->ordinal2      = 0;
    pPacket->incCEcounter  = 1;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 command to increment the DE counter. Returns the size of the PM4 command built, in DWORDs.
size_t CmdUtil::BuildIncrementDeCounter(
    void* pBuffer // [out] Build the PM4 packet in this buffer.
    ) const
{
    constexpr size_t PacketSize = PM4_CMD_INC_DE_COUNTER_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDINCDECOUNTER*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_INCREMENT_DE_COUNTER, PacketSize);
    pPacket->ordinal2      = 0;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues an "index attributes indirect" command into the given DE stream. Return the size of
// the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildIndexAttributesIndirect(
    gpusize baseAddr,   // Base address of an array of index attributes
    uint16  index,      // Index into the array of index attributes to load
    void*   pBuffer     // [out] Build the PM4 packet in this buffer.
    ) const
{
    PAL_ASSERT(IsPow2Aligned(baseAddr, 16)); // Address must be 4-DWORD aligned

    constexpr size_t PacketSize = PM4_CMD_INDEX_ATTRIBUTES_INDIRECT_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDINDEXATTRIBUTESINDIRECT*>(pBuffer);

    pPacket->header.u32All  = Type3Header(IT_INDEX_ATTRIBUTES_INDIRECT__CI__VI, PacketSize);
    pPacket->ordinal2       = 0;
    pPacket->addressLo      = (LowPart(baseAddr) >> 4);
    pPacket->addressHi      = HighPart(baseAddr);
    pPacket->ordinal4       = 0;
    pPacket->attributeIndex = index;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a "index base" command into the given DE command stream. Return the size of the PM4
// command assembled, in DWORDs.
size_t CmdUtil::BuildIndexBase(
    gpusize baseAddr, // Base address of index buffer (w/ offset).
    void*   pBuffer   // [out] Build the PM4 packet in this buffer.
    ) const
{
    // Address must be 2 byte aligned
    PAL_ASSERT(IsPow2Aligned(baseAddr, 2));

    constexpr size_t PacketSize = PM4_CMD_DRAW_INDEX_BASE_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDDRAWINDEXBASE*>(pBuffer);

    // Workaround for Gfx6 bug This is a DMA clamping bug that occurs when both the DMA base address (word aligned) is
    // zero and DMA_MAX_SIZE is zero.The max address used to determine when to start clamping underflows and therefore
    // the logic thinks it should start clamping at word address 0xFF FFFF FFFF (DMA Last Max Word Address).
    // assign dma_max_word_addr_d = rbiu_dma_base + dma_max_num_words - 1
    // Setting the IB addr to 2 or higher solves this issue
    if ((baseAddr == 0x0) &&
        (static_cast<const Pal::Gfx6::Device*>(m_device.GetGfxDevice())->WaMiscNullIb() == true))
    {
        baseAddr = 0x2;
    }

    pPacket->header.u32All = Type3Header(IT_INDEX_BASE, PacketSize);
    pPacket->addrLo        = LowPart(baseAddr);
    pPacket->ordinal3      = 0;
    pPacket->addrHi        = HighPart(baseAddr);

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a "index buffer size" command into the given DE command stream. Returns the size of
// the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildIndexBufferSize(
    uint32 indexCount,
    void*  pBuffer     // [out] Build the PM4 packet in this buffer.
    ) const
{
    constexpr size_t PacketSize = PM4_CMD_DRAW_INDEX_BUFFER_SIZE_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDDRAWINDEXBUFFERSIZE*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_INDEX_BUFFER_SIZE, PacketSize);
    pPacket->numIndices    = indexCount;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a "index type" command into the given DE command stream. Returns the size of the PM4
// command assembled, in DWORDs.
size_t CmdUtil::BuildIndexType(
    regVGT_DMA_INDEX_TYPE__VI vgtDmaIndexType,
    void*                     pBuffer    // [out] Build the PM4 packet in this buffer.
    ) const
{
    PAL_ASSERT((vgtDmaIndexType.bits.INDEX_TYPE != VGT_INDEX_8__VI) || (m_chipFamily >= GfxIpLevel::GfxIp8));

    constexpr size_t PacketSize = PM4_CMD_DRAW_INDEX_TYPE_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDDRAWINDEXTYPE*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_INDEX_TYPE, PacketSize);
    pPacket->ordinal2      = vgtDmaIndexType.u32All;

    return PacketSize;
}

// =====================================================================================================================
//  Builds a INDIRECT_BUFFER packet that is chained to another indirect buffer located at the specified address. Returns
//  the size of the PM4 command assembled, in DWORDs.
//
//  NOTE: If the chain bit is not set, an IB2 will be lanched. *Not* setting that bit from within an IB2 will cause a
//        hang because the CP does not support an IB3.
size_t CmdUtil::BuildIndirectBuffer(
    gpusize gpuAddr,
    size_t  sizeInDwords,
    bool    chain,
    bool    constantEngine,
    bool    enablePreemption,
    void*   pBuffer         // [out] Build the PM4 packet in this buffer.
    ) const
{
    // Address must be four byte aligned and the size must be nonzero.
    PAL_ASSERT(IsPow2Aligned(gpuAddr, 4) && (sizeInDwords != 0));

    constexpr size_t    PacketSize = PM4_CMD_INDIRECT_BUFFER_DWORDS;
    auto*const          pPacket    = static_cast<PM4CMDINDIRECTBUFFER*>(pBuffer);
    const IT_OpCodeType opCode     = constantEngine ? IT_INDIRECT_BUFFER_CNST : IT_INDIRECT_BUFFER;

    pPacket->header.u32All = Type3Header(opCode, PacketSize);
    pPacket->ibBaseLo      = LowPart(gpuAddr);
    pPacket->ibBaseHi32    = HighPart(gpuAddr);
    pPacket->ordinal4      = 0;

    if (m_chipFamily == GfxIpLevel::GfxIp6)
    {
        pPacket->SI.ibSize = static_cast<uint32>(sizeInDwords);
        pPacket->SI.chain  = chain;
        PAL_ASSERT(enablePreemption == false);
    }
#if SI_CI_VI_PM4DEFS_VERSION_MAJOR > 1 || SI_CI_VI_PM4DEFS_VERSION_MINOR >= 18
    else if (m_chipFamily >= GfxIpLevel::GfxIp8)
    {
        pPacket->VI.ibSize = static_cast<uint32>(sizeInDwords);
        pPacket->VI.chain  = chain;
        pPacket->VI.valid  = 1;
        pPacket->VI.preEna = enablePreemption;
    }
#endif
    else
    {
        pPacket->CI.ibSize = static_cast<uint32>(sizeInDwords);
        pPacket->CI.chain  = chain;
        pPacket->CI.valid  = 1;
        PAL_ASSERT(enablePreemption == false);
    }

    return PacketSize;
}

// =====================================================================================================================
// Helper method which builds a LOADDATA PM4 packet for loading multiple regions of a specific type of register from
// GPU memory.
template <IT_OpCodeType opCode>
size_t CmdUtil::BuildLoadRegsOne(
    gpusize       gpuVirtAddr,
    uint32        startRegOffset,
    uint32        count,
    PM4ShaderType shaderType,
    void*         pBuffer
    ) const
{
    // The GPU virtual address must be DWORD-aligned and not use more than 48 bits.
    PAL_ASSERT(IsPow2Aligned(gpuVirtAddr, 4));
    PAL_ASSERT((HighPart(gpuVirtAddr) & 0xFFFF0000) == 0);

    constexpr size_t PacketSize = PM4_CMD_LOAD_DATA_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDLOADDATA*>(pBuffer);

    pPacket->header.u32All  = Type3Header(opCode, PacketSize, shaderType);
    pPacket->addrLo         = LowPart(gpuVirtAddr);
    pPacket->addrHi.u32All  = 0;
    pPacket->addrHi.ADDR_HI = HighPart(gpuVirtAddr);
    if (opCode == IT_LOAD_CONFIG_REG)
    {
        pPacket->addrHi.WAIT_IDLE = 1;
    }
    pPacket->regOffset      = startRegOffset;
    pPacket->numDwords      = count;

    return PacketSize;
}

// =====================================================================================================================
// Helper method which builds a LOADDATA PM4 packet for loading multiple regions of a specific type of register from
// GPU memory.
template <IT_OpCodeType opCode>
size_t CmdUtil::BuildLoadRegsMulti(
    gpusize              gpuVirtAddr,
    const RegisterRange* pRanges,
    uint32               rangeCount,
    PM4ShaderType        shaderType,
    void*                pBuffer
    ) const
{
    PAL_ASSERT((pRanges != nullptr) && (rangeCount >= 1));

    // The GPU virtual address must be DWORD-aligned and not use more than 48 bits.
    PAL_ASSERT(IsPow2Aligned(gpuVirtAddr, 4));
    PAL_ASSERT((HighPart(gpuVirtAddr) & 0xFFFF0000) == 0);

    const size_t packetSize = PM4_CMD_LOAD_DATA_DWORDS + (2 * (rangeCount - 1));
    auto*const   pPacket    = static_cast<PM4CMDLOADDATA*>(pBuffer);

    pPacket->header.u32All  = Type3Header(opCode, packetSize, shaderType);
    pPacket->addrLo         = LowPart(gpuVirtAddr);
    pPacket->addrHi.u32All  = 0;
    pPacket->addrHi.ADDR_HI = HighPart(gpuVirtAddr);
    if (opCode == IT_LOAD_CONFIG_REG)
    {
        pPacket->addrHi.WAIT_IDLE = 1;
    }

    // Note: This is a variable-length packet. The PM4CMDLOADDATA packet contains space for the first register range,
    // but not the others (though they are expected to immediately follow in the command buffer).
    memcpy(&pPacket->ordinal4, pRanges, (sizeof(RegisterRange) * rangeCount));

    return packetSize;
}

// =====================================================================================================================
// Helper method which builds a LOADDATA_INDEX PM4 packeet for loading a specific type of register from GPU memory
// without updaing the register-shadowing address in the CP.
template <IT_OpCodeType opCode, bool directAddress, uint32 dataFormat>
size_t CmdUtil::BuildLoadRegsIndex(
    gpusize       gpuVirtAddrOrAddrOffset,
    uint32        startRegOffset,
    uint32        count,
    PM4ShaderType shaderType,
    void*         pBuffer
    ) const
{
    // This packet is only supported on Gfx 8.0+
    PAL_ASSERT(m_chipFamily >= GfxIpLevel::GfxIp8);

    // The GPU virtual address and/or address offset gets added to a base address set via SET_BASE packet. CP then
    // loads the data from that address and it must be DWORD aligned.
    PAL_ASSERT(IsPow2Aligned(gpuVirtAddrOrAddrOffset, 4));

    constexpr size_t PacketSize = PM4_CMD_LOAD_DATA_INDEX_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDLOADDATAINDEX*>(pBuffer);

    pPacket->header.u32All  = Type3Header(opCode, PacketSize, shaderType);
    pPacket->addrLo.u32All  = 0;
    if (directAddress)
    {
        // Only the low 16 bits of addrOffset are honored for the high portion of the GPU virtual address!
        PAL_ASSERT((HighPart(gpuVirtAddrOrAddrOffset) & 0xFFFF0000) == 0);

        pPacket->addrLo.index   = LOAD_DATA_INDEX_DIRECT_ADDR;
        pPacket->addrLo.ADDR_LO = (LowPart(gpuVirtAddrOrAddrOffset) >> 2);
        pPacket->addrOffset     = HighPart(gpuVirtAddrOrAddrOffset);
    }
    else
    {
        // The high part of the offset is ignored when not using direct-address mode because the offset is only
        // specified to the packet using 32 bits.
        PAL_ASSERT(HighPart(gpuVirtAddrOrAddrOffset) == 0);

        pPacket->addrLo.index = LOAD_DATA_INDEX_OFFSET;
        pPacket->addrOffset   = LowPart(gpuVirtAddrOrAddrOffset);
    }
    pPacket->ordinal4   = 0;
    pPacket->dataFormat = dataFormat;
    pPacket->numDwords  = count;

    if (dataFormat == LOAD_DATA_FORMAT_OFFSET_AND_SIZE)
    {
        pPacket->regOffset = startRegOffset;
    }
    else // LOAD_DATA_FORMAT_OFFSET_AND_DATA
    {
        PAL_ASSERT(startRegOffset == 0);
    }

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a load_config_reg command to load a single group of consecutive config registers
// from video memory.  Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildLoadConfigRegs(
    gpusize              gpuVirtAddr,
    const RegisterRange* pRanges,
    uint32               rangeCount,
    void*                pBuffer       // [out] Build the PM4 packet in this buffer.
    ) const
{
    return BuildLoadRegsMulti<IT_LOAD_CONFIG_REG>(gpuVirtAddr, pRanges, rangeCount, ShaderGraphics, pBuffer);
}

// =====================================================================================================================
// Builds a PM4 packet which issues a load_context_reg command to load a single group of consecutive context registers
// from video memory.  Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildLoadContextRegs(
    gpusize              gpuVirtAddr,
    const RegisterRange* pRanges,
    uint32               rangeCount,
    void*                pBuffer       // [out] Build the PM4 packet in this buffer.
    ) const
{
    return BuildLoadRegsMulti<IT_LOAD_CONTEXT_REG>(gpuVirtAddr, pRanges, rangeCount, ShaderGraphics, pBuffer);
}

// =====================================================================================================================
// Builds a PM4 packet which issues a load_context_reg command to load a single group of consecutive context registers
// from video memory.  Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildLoadContextRegs(
    gpusize gpuVirtAddr,
    uint32  startRegAddr,
    uint32  count,
    void*   pBuffer       // [out] Build the PM4 packet in this buffer.
    ) const
{
    PAL_ASSERT(IsContextReg(startRegAddr));
    return BuildLoadRegsOne<IT_LOAD_CONTEXT_REG>(gpuVirtAddr,
                                                 (startRegAddr - CONTEXT_SPACE_START),
                                                 count,
                                                 ShaderGraphics,
                                                 pBuffer);
}

// =====================================================================================================================
// Builds a PM4 packet which issues a load_context_reg_index command to load a single group of consecutive context
// registers from an indirect video memory offset. The memory base address is set via set_base packet.
// Returns the size of the PM4 command assembled, in DWORDs.
template <bool directAddress>
size_t CmdUtil::BuildLoadContextRegsIndex(
    gpusize gpuVirtAddrOrAddrOffset,
    uint32  startRegAddr,
    uint32  count,
    void*   pBuffer       // [out] Build the PM4 packet in this buffer.
    ) const
{
    PAL_ASSERT(IsContextReg(startRegAddr));
    return BuildLoadRegsIndex<IT_LOAD_CONTEXT_REG_INDEX__VI,
                              directAddress,
                              LOAD_DATA_FORMAT_OFFSET_AND_SIZE>(gpuVirtAddrOrAddrOffset,
                                                                (startRegAddr - CONTEXT_SPACE_START),
                                                                count,
                                                                ShaderGraphics,
                                                                pBuffer);
}

template
size_t CmdUtil::BuildLoadContextRegsIndex<true>(
    gpusize gpuVirtAddrOrAddrOffset,
    uint32  startRegAddr,
    uint32  count,
    void*   pBuffer
    ) const;

template
size_t CmdUtil::BuildLoadContextRegsIndex<false>(
    gpusize gpuVirtAddrOrAddrOffset,
    uint32  startRegAddr,
    uint32  count,
    void*   pBuffer
    ) const;

// =====================================================================================================================
// Builds a PM4 packet which issues a load_context_reg_index command to load a series of individual context registers
// stored in GPU memory.  Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildLoadContextRegsIndex(
    gpusize gpuVirtAddr,
    uint32  count,
    void*   pBuffer       // [out] Build the PM4 packet in this buffer.
    ) const
{
    return BuildLoadRegsIndex<IT_LOAD_CONTEXT_REG_INDEX__VI,
                              true,
                              LOAD_DATA_FORMAT_OFFSET_AND_DATA>(gpuVirtAddr, 0, count, ShaderGraphics, pBuffer);
}

// =====================================================================================================================
// Builds a PM4 packet which issues a load_sh_reg command to load a single group of consecutive persistent space
// registers from video memory. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildLoadShRegs(
    gpusize              gpuVirtAddr,
    const RegisterRange* pRanges,
    uint32               rangeCount,
    PM4ShaderType        shaderType,
    void*                pBuffer       // [out] Build the PM4-packet in this buffer.
    ) const
{
    return BuildLoadRegsMulti<IT_LOAD_SH_REG>(gpuVirtAddr, pRanges, rangeCount, shaderType, pBuffer);
}

// =====================================================================================================================
// Builds a PM4 packet which issues a load_sh_reg command to load a single group of consecutive persistent space
// registers from video memory. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildLoadShRegs(
    gpusize       gpuVirtAddr,
    uint32        startRegAddr,
    uint32        count,
    PM4ShaderType shaderType,
    void*         pBuffer       // [out] Build the PM4-packet in this buffer.
    ) const
{
    PAL_ASSERT(IsShReg(startRegAddr));
    return BuildLoadRegsOne<IT_LOAD_SH_REG>(gpuVirtAddr,
                                            (startRegAddr - PERSISTENT_SPACE_START),
                                            count,
                                            shaderType,
                                            pBuffer);
}

// =====================================================================================================================
// Builds a PM4 packet which issues a load_sh_reg_index command to load a single group of consecutive persistent state
// registers from indirect video memory offset. The memory base address is set via set_base packet.
// Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildLoadShRegsIndex(
    uint32        addrOffset,
    uint32        startRegAddr,
    uint32        count,
    PM4ShaderType shaderType,
    void*         pBuffer       // [out] Build the PM4-packet in this buffer.
    ) const
{
    PAL_ASSERT(IsShReg(startRegAddr));
    return BuildLoadRegsIndex<IT_LOAD_SH_REG_INDEX__VI,
                              false,
                              LOAD_DATA_FORMAT_OFFSET_AND_SIZE>(addrOffset,
                                                                (startRegAddr - PERSISTENT_SPACE_START),
                                                                count,
                                                                shaderType,
                                                                pBuffer);
}

// =====================================================================================================================
// Builds a PM4 packet which issues a load_sh_reg_index command to load a series of individual persistent-state
// registers stored in GPU memory.  Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildLoadShRegsIndex(
    gpusize       gpuVirtAddr,
    uint32        count,
    PM4ShaderType shaderType,
    void*         pBuffer       // [out] Build the PM4 packet in this buffer.
    ) const
{
    return BuildLoadRegsIndex<IT_LOAD_SH_REG_INDEX__VI,
                              true,
                              LOAD_DATA_FORMAT_OFFSET_AND_DATA>(gpuVirtAddr, 0, count, shaderType, pBuffer);
}

// =====================================================================================================================
// Builds a PM4 packet which issues a load_config_reg command to load a single group of consecutive user-coonfig
// registers from video memory.  Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildLoadUserConfigRegs(
    gpusize              gpuVirtAddr,
    const RegisterRange* pRanges,
    uint32               rangeCount,
    void*                pBuffer       // [out] Build the PM4 packet in this buffer.
    ) const
{
    return BuildLoadRegsMulti<IT_LOAD_UCONFIG_REG__CI__VI>(gpuVirtAddr, pRanges, rangeCount, ShaderGraphics, pBuffer);
}

// =====================================================================================================================
// Builds a PM4 constant engine command to load the specified amount of data from GPU memory into CE RAM. Returns the
// size of the PM4 command built, in DWORDs.
size_t CmdUtil::BuildLoadConstRam(
    gpusize srcGpuAddr,
    uint32  ramByteOffset,
    uint32  dwordSize,     // Amount of data to load, in DWORDs. Must be a multiple of 8
    void*   pBuffer        // [out] Build the PM4 packet in this buffer.
    ) const
{
    PAL_ASSERT(IsPow2Aligned(srcGpuAddr, 32));
    PAL_ASSERT(IsPow2Aligned(ramByteOffset, 32));
    PAL_ASSERT(IsPow2Aligned(dwordSize, 8));

    constexpr size_t PacketSize = PM4_CMD_LOAD_CONST_RAM_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDCONSTRAMLOAD*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_LOAD_CONST_RAM, PacketSize);
    pPacket->addrLo        = LowPart(srcGpuAddr);
    pPacket->addrHi        = HighPart(srcGpuAddr);
    pPacket->ordinal4      = 0;
    pPacket->numDwords     = dwordSize;
    pPacket->ordinal5      = 0;
    pPacket->offset        = ramByteOffset;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 command which issues either a wait or signal operation on a memory semaphore. Returns the size of the
// PM4 command written, in DWORDs.
size_t CmdUtil::BuildMemSemaphore(
    gpusize gpuVirtAddr,
    uint32  semaphoreOp,     // Semaphore operation to issue
    uint32  semaphoreClient, // GPU block to issue the operation: can be either the CP, CB or DB.
    bool    useMailbox,      // If true, a signal operation will wait for the mailbox to be written.
    bool    isBinary,        // If true, signals write "1" instead of incrementing
    void*   pBuffer          // [out] Build the PM4 packet in this buffer.
    ) const
{
    PAL_ASSERT((semaphoreClient == MEM_SEMA_CP) ||
               (semaphoreClient == MEM_SEMA_CB) ||
               (semaphoreClient == MEM_SEMA_DB));

    // NOTE: The useMailbox and isBinary parameters are ignored for Wait operations.
    if (semaphoreOp != MEM_SEMA_SIGNAL)
    {
        PAL_ASSERT(semaphoreOp == MEM_SEMA_WAIT);

        useMailbox = false;
        isBinary   = false;
    }

    constexpr size_t PacketSize = PM4_CMD_MEM_SEMAPHORE_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDMEMSEMAPHORE*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_MEM_SEMAPHORE, PacketSize);
    pPacket->addrLo        = LowPart(gpuVirtAddr);
    pPacket->ordinal3      = 0;

    if (m_chipFamily == GfxIpLevel::GfxIp6)
    {
        // The MEM_SEMAPHORE packet is slightly different for Gfx6 family hardware: only 40 bit addresses are supported
        // for the memory location, and there are some extra unused bits in the last DWORD of the packet.
        PAL_ASSERT((HighPart(gpuVirtAddr) & 0xFFFFFF00) == 0);

        pPacket->SI.addrHi     = HighPart(gpuVirtAddr);
        pPacket->SI.clientCode = semaphoreClient;
        pPacket->SI.semSel     = semaphoreOp;
        pPacket->SI.signalType = isBinary;
        pPacket->SI.useMailbox = useMailbox;
    }
    else
    {
        // Gfx7 and newer hardware families support 48 bit addresses for the memory location.
        PAL_ASSERT((HighPart(gpuVirtAddr) & 0xFFFF0000) == 0);

        pPacket->CI.addrHi     = HighPart(gpuVirtAddr);
        pPacket->CI.clientCode = semaphoreClient;
        pPacket->CI.semSel     = semaphoreOp;
        pPacket->CI.signalType = isBinary;
        pPacket->CI.useMailbox = useMailbox;
    }

    return PacketSize;
}

// =====================================================================================================================
// Builds a NOP command as long as the specified number of DWORDs. Note: Type-2 packets are not supported, so a single
// DWORD NOP is not supported by this function except on Gfx8 where a new type-3 packet was added for that purpose.
// Returns the size of the PM4 command built, in DWORDs.
size_t CmdUtil::BuildNop(
    size_t numDwords,
    void*  pBuffer    // [out] Build the PM4 packet in this buffer.
    ) const
{
    uint32* pNopHeader = static_cast<uint32*>(pBuffer);

    if (numDwords == 0)
    {
        // No padding required.
    }
    else if (numDwords == 1)
    {
        // Gfx8 adds a new type-3 packet that is a single DWORD long.
        PAL_ASSERT (m_chipFamily >= GfxIpLevel::GfxIp8);

        // PM4 type 3 NOP will use special size (i.e., 0x3FFF, the max possible size field) to represent one DWORD NOP.
        // Add two here since the macro will replace with "count -2".
        constexpr uint32  MaxCountField = ((1 << PM4_COUNT_SHIFT) - 1);

        (*pNopHeader) = Type3Header(IT_NOP, MaxCountField + 2);
    }
    else
    {
        (*pNopHeader) = Type3Header(IT_NOP, numDwords);
    }

    return numDwords;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a "num instances" command into the given DE command stream. Returns the Size of the
// PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildNumInstances(
    uint32 instanceCount,
    void*  pBuffer        // [out] Build the PM4 packet in this buffer.
    ) const
{
    constexpr size_t PacketSize = PM4_CMD_DRAW_NUM_INSTANCES_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDDRAWNUMINSTANCES*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_NUM_INSTANCES, PacketSize);
    pPacket->numInstances  = instanceCount;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 command to add the differences in the given set of ZPASS begin and end counts. Returns the size of the
// PM4 command built, in DWORDs.
size_t CmdUtil::BuildOcclusionQuery(
    gpusize queryMemAddr, // DB0 start address, 16-byte aligned
    gpusize dstMemAddr,   // Accumulated ZPASS count destination, 4-byte aligned
    void*   pBuffer       // [out] Build the PM4 packet in this buffer.
    ) const
{
    constexpr gpusize Over48Bits = 0xFFFF000000000000;

    // The query address should be 48-bits and 16-byte aligned.
    PAL_ASSERT(IsPow2Aligned(queryMemAddr, 16) && ((queryMemAddr & Over48Bits) == 0) && (queryMemAddr != 0));

    // The destination address should be 48-bits and 4-byte aligned.
    PAL_ASSERT(IsPow2Aligned(dstMemAddr, 4) && ((dstMemAddr & Over48Bits) == 0) && (dstMemAddr != 0));

    // Note that queryAddr means "zpass query sum address" and not "query pool counters address". Instead startAddr is
    // the "query pool counters addess".
    constexpr size_t PacketSize = PM4_CMD_OCCLUSION_QUERY_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDOCCLUSIONQUERY*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_OCCLUSION_QUERY, PacketSize);
    pPacket->startAddrLo   = LowPart(queryMemAddr);
    pPacket->ordinal3      = 0;
    pPacket->startAddrHi   = HighPart(queryMemAddr);
    pPacket->queryAddrLo   = LowPart(dstMemAddr);
    pPacket->ordinal5      = 0;
    pPacket->queryAddrHi   = HighPart(dstMemAddr);

    return PacketSize;
}

// =====================================================================================================================
// Constructs a PM4 packet which issues a sync command instructing the PFP to stall until the ME is no longer busy. This
// packet will hang on the compute queue; it is the caller's responsibility to ensure that this function is called
// safely. Returns the size of the PM4 command built, in DWORDs.
size_t CmdUtil::BuildPfpSyncMe(
    void* pBuffer // [out] Build the PM4 packet in this buffer.
    ) const
{
    constexpr size_t PacketSize = PM4_CMD_PFP_SYNC_ME_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDPFPSYNCME*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_PFP_SYNC_ME, PacketSize);
    pPacket->dummyData     = 0;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which marks the beginning or end of either a draw-engine preamble or the initialization of
// clear-state memory. Returns the size of the PM4 command build, in DWORDs.
size_t CmdUtil::BuildPreambleCntl(
    uint32 command,
    void*  pBuffer      // [out] Build the PM4 packet in this buffer.
    ) const
{
    PAL_ASSERT((command == PREAMBLE_CNTL_PREAMBLE_BEGIN)    ||
               (command == PREAMBLE_CNTL_PREAMBLE_END)      ||
               (command == PREAMBLE_CNTL_CLEAR_STATE_BEGIN) ||
               (command == PREAMBLE_CNTL_CLEAR_STATE_END));

    constexpr size_t PacketSize = (sizeof(PM4CMDPREAMBLECNTL) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4CMDPREAMBLECNTL*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_PREAMBLE_CNTL, PacketSize);
    pPacket->ordinal2      = 0;
    pPacket->command       = command;

    return PacketSize;
}

// =====================================================================================================================
// Builds a release_mem packet to the specified stream.  This packet is only usable on compute queues on Gfx7 or newer
// ASICs. Returns the size of the PM4 command built, in DWORDs.
size_t CmdUtil::BuildReleaseMem(
    VGT_EVENT_TYPE eventType,
    gpusize        gpuAddress,
    uint32         dataSel,    // One of the RELEASEMEM_DATA_SEL_* constants
    uint64         data,       // data to write, ignored except for DATA_SEL_SEND_DATA{32,64}
    uint32         gdsAddr,    // GDS DWORD offset from start of partition for DATA_SEL_STORE_GDS
    uint32         gdsSize,    // Number of DWords to store for DATA_SEL_STORE_GDS
    bool           flushInvL2, // If true, do a full L2 cache flush and invalidate.
    void*          pBuffer     // [out] Build the PM4 packet in this buffer.
    ) const
{
    // Packet is only usable on Gfx7 and newer ASICs.  For Gfx6, use event-write-eop.
    PAL_ASSERT(m_chipFamily != GfxIpLevel::GfxIp6);

    // 32-bit data must be DWORD aligned.
    PAL_ASSERT((dataSel != RELEASEMEM_DATA_SEL_SEND_DATA32) || IsPow2Aligned(gpuAddress, 4));

    // 64-bit data must be QWORD aligned.
    PAL_ASSERT(((dataSel != RELEASEMEM_DATA_SEL_SEND_DATA64) && (dataSel != RELEASEMEM_DATA_SEL_SEND_GPU_CLOCK)) ||
               IsPow2Aligned(gpuAddress, 8));

    // We can only have a GDS size iff. we have a GDS data selection.
    PAL_ASSERT((dataSel == RELEASEMEM_DATA_SEL_STORE_GDS_DATA) == (gdsSize > 0));

    // This data selection is not supported.
    PAL_ASSERT(dataSel != RELEASEMEM_DATA_SEL_SEND_CP_PERFCOUNTER);

    // Only certain event types are supported by this packet.
    PAL_ASSERT ((eventType == CS_DONE)                      ||
                (eventType == CACHE_FLUSH_TS)               ||
                (eventType == CACHE_FLUSH_AND_INV_TS_EVENT) ||
                (eventType == BOTTOM_OF_PIPE_TS)            ||
                (eventType == FLUSH_AND_INV_DB_DATA_TS)     ||
                (eventType == FLUSH_AND_INV_CB_DATA_TS));

    size_t     totalSize = PM4_CMD_RELEASE_MEM_DWORDS;
    auto*const pPacket   = static_cast<PM4CMDRELEASEMEM*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_RELEASE_MEM__CI__VI, totalSize);
    pPacket->ordinal2      = 0;
    pPacket->eventType     = eventType;
    pPacket->eventIndex    = EventIndexFromEventType(eventType);
    if (flushInvL2)
    {
        pPacket->tcWbActionEna = 1;
        pPacket->tcActionEna   = 1;
    }
    pPacket->ordinal3      = 0;
    pPacket->dstSel        = RELEASEMEM_DST_SEL_MEMORY;
    pPacket->dataSel       = dataSel;
    pPacket->addressLo     = LowPart(gpuAddress);
    pPacket->addressHi     = HighPart(gpuAddress);

    // This won't send an interrupt but will wait for write confirm before writing the data to memory.
    pPacket->intSel        = (dataSel == RELEASEMEM_DATA_SEL_DISCARD) ? RELEASEMEM_INT_SEL_NONE
                                                                      : RELEASEMEM_INT_SEL_SEND_DATA_ON_CONFIRM;

    if (dataSel == RELEASEMEM_DATA_SEL_STORE_GDS_DATA)
    {
        const uint32 cpUcodeVersion = m_device.EngineProperties().cpUcodeVersion;
        if (((m_chipFamily == GfxIpLevel::GfxIp7) && (cpUcodeVersion < 29)) ||
            ((m_chipFamily >= GfxIpLevel::GfxIp8) && (cpUcodeVersion < 39)))
        {
            // Note that we must convert the gdsAddr (DWORD-based) to a gdsAddress (byte-based) when using RELEASE_MEM.
            gdsAddr *= sizeof(uint32);
        }

        pPacket->gdsIndex  = gdsAddr;
        pPacket->numDwords = gdsSize;
        pPacket->ordinal7  = 0;

        // The CPDMA performance issue affects RELEASE_MEM if the source is GDS. We only need to patch the GDS size.
        const uint32 sizeInBytes = gdsSize * sizeof(uint32);
        const uint32 alignment   = GetGfx6Settings(m_device).cpDmaSrcAlignment;
        const uint32 fixupSize   = Pow2Align(sizeInBytes, alignment) - sizeInBytes;

        if (fixupSize > 0)
        {
            totalSize += BuildDmaDataSizeFixup(fixupSize, pPacket + 1);
        }
    }
    else
    {
        pPacket->dataLo = LowPart(data);
        pPacket->dataHi = HighPart(data);
    }

    return totalSize;
}

// =====================================================================================================================
// Builds a REWIND packet for telling compute queues to reload the command buffer data after this packet. Returns the
// size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildRewind(
    bool  offloadEnable,
    bool  valid,
    void* pBuffer
    ) const
{
    // This packet is only supported on compute queues, and only for gfx7 hardware and newer!
    PAL_ASSERT(m_chipFamily >= GfxIpLevel::GfxIp7);

    constexpr size_t PacketSize = PM4_CMD_REWIND_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDREWIND*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_REWIND__CI__VI, PacketSize, ShaderCompute);
    pPacket->ordinal2      = 0;
    pPacket->offloadEnable = offloadEnable;
    pPacket->valid         = valid;

    return PacketSize;
}

// =====================================================================================================================
// Builds a SET_BASE packet for indirect draws/dispatches. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildSetBase(
    PM4ShaderType shaderType,
    uint32        baseIndex,
    gpusize       baseAddr,
    void*         pBuffer     // [out] Build the PM4 packet in this buffer.
    ) const
{
    constexpr size_t PacketSize = PM4_CMD_DRAW_SET_BASE_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDDRAWSETBASE*>(pBuffer);

    PAL_ASSERT((baseIndex == BASE_INDEX_DRAW_INDIRECT) || (baseIndex == BASE_INDEX_LOAD_REG) ||
               (baseIndex == BASE_INDEX_INDIRECT_DATA) || (baseIndex == BASE_INDEX_CE_DST_BASE_ADDR));
    PAL_ASSERT(IsPow2Aligned(baseAddr, 8));

    pPacket->header.u32All = Type3Header(IT_SET_BASE, PacketSize, shaderType);
    pPacket->ordinal2      = 0;
    pPacket->baseIndex     = baseIndex;
    pPacket->addressLo     = LowPart(baseAddr);
    pPacket->ordinal4      = 0;
    pPacket->addressHi     = HighPart(baseAddr);

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which sets one config register. The index field is used to set special registers on Gfx7+ and
// should be set to zero except when setting one of those registers; it has no effect on Gfx6. Returns the size of the
// PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildSetOneConfigReg(
    uint32 regAddr,
    void*  pBuffer, // [out] Build the PM4 packet in this buffer.
    uint32 index
    ) const
{
    constexpr size_t PacketSize = PM4_CMD_SET_DATA_DWORDS + 1;
    auto*const       pPacket    = static_cast<PM4CMDSETDATA*>(pBuffer);

    if (m_chipFamily == GfxIpLevel::GfxIp6)
    {
        PAL_ASSERT(IsConfigReg(regAddr));

        pPacket->header.u32All = Type3Header(IT_SET_CONFIG_REG, PacketSize);
        pPacket->ordinal2      = SetDataOrdinal2(regAddr - CONFIG_SPACE_START);
    }
    else
    {
#if PAL_ENABLE_PRINTS_ASSERTS
        CheckShadowedUserConfigReg(regAddr);
#endif
        PAL_ASSERT(((regAddr != mmVGT_PRIMITIVE_TYPE__CI__VI) || (index == SET_UCONFIG_INDEX_PRIM_TYPE))   &&
                   ((regAddr != mmVGT_INDEX_TYPE__CI__VI)     || (index == SET_UCONFIG_INDEX_INDEX_TYPE))  &&
                   ((regAddr != mmVGT_NUM_INSTANCES__CI__VI)  || (index == SET_UCONFIG_INDEX_NUM_INSTANCES)));

        pPacket->header.u32All = Type3Header(IT_SET_UCONFIG_REG__CI__VI, PacketSize);
        pPacket->ordinal2 = SetDataOrdinal2(regAddr - UCONFIG_SPACE_START__CI__VI, index);
    }

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which sets one context register. Note that unlike R6xx/EG/NI, GCN has no compute contexts, so all
// context registers are for graphics. The index field is used to set special registers on Gfx7+ and should be set to
// zero except when setting one of those registers; it has no effect on Gfx6. Returns the size of the PM4 command
// assembled, in DWORDs.
size_t CmdUtil::BuildSetOneContextReg(
    uint32 regAddr,
    void*  pBuffer, // [out] Build the PM4 packet in this buffer.
    uint32 index
    ) const
{
#if PAL_ENABLE_PRINTS_ASSERTS
    CheckShadowedContextReg(regAddr);
#endif
    PAL_ASSERT(((regAddr != mmIA_MULTI_VGT_PARAM)  || (index == SET_CONTEXT_INDEX_MULTI_VGT_PARAM))  &&
               ((regAddr != mmVGT_LS_HS_CONFIG)    || (index == SET_CONTEXT_INDEX_VGT_LS_HS_CONFIG)) &&
               ((regAddr != mmPA_SC_RASTER_CONFIG) || (index == SET_CONTEXT_INDEX_PA_SC_RASTER_CONFIG)
                                                   || (m_device.ChipProperties().gfx6.rbReconfigureEnabled == 0)));

    constexpr size_t PacketSize = PM4_CMD_SET_DATA_DWORDS + 1;
    auto*const       pPacket    = static_cast<PM4CMDSETDATA*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_SET_CONTEXT_REG, PacketSize);
    pPacket->ordinal2      = SetDataOrdinal2(regAddr - CONTEXT_SPACE_START, index);

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which sets one SH register. Returns size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildSetOneShReg(
    uint32        regAddr,
    PM4ShaderType shaderType,
    void*         pBuffer     // [out] Build the PM4 packet in this buffer.
    ) const
{
#if PAL_ENABLE_PRINTS_ASSERTS
    CheckShadowedShReg(shaderType, regAddr);
#endif

    constexpr size_t PacketSize = PM4_CMD_SET_DATA_DWORDS + 1;
    auto*const       pPacket    = static_cast<PM4CMDSETDATA*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_SET_SH_REG, PacketSize, shaderType);
    pPacket->ordinal2      = SetDataOrdinal2(regAddr - PERSISTENT_SPACE_START);

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which sets one SH register. Returns size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildSetOneShRegIndex(
    uint32        regAddr,
    PM4ShaderType shaderType,
    uint32        index,
    void*         pBuffer     // [out] Build the PM4 packet in this buffer.
    ) const
{
#if PAL_ENABLE_PRINTS_ASSERTS
    CheckShadowedShReg(shaderType, regAddr);
#endif

    constexpr size_t PacketSize = PM4_CMD_SET_DATA_DWORDS + 1;
    auto*const       pPacket    = static_cast<PM4CMDSETDATA*>(pBuffer);

    // Revert to the 'old' packet if there is no support for IT_SET_SH_REG_INDEX
    if (m_device.ChipProperties().gfx6.supportSetShIndexPkt == 0)
    {
        pPacket->header.u32All = Type3Header(IT_SET_SH_REG, PacketSize, shaderType);
        pPacket->ordinal2      = SetDataOrdinal2(regAddr - PERSISTENT_SPACE_START);
    }
    else
    {
        pPacket->header.u32All = Type3Header(IT_SET_SH_REG_INDEX__CI__VI, PacketSize, shaderType);
        pPacket->ordinal2      = SetDataOrdinal2(regAddr - PERSISTENT_SPACE_START, index);
    }

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which sets a sequence of config registers starting with startRegAddr and ending with endRegAddr
// (inclusive). Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildSetSeqConfigRegs(
    uint32 startRegAddr,
    uint32 endRegAddr,
    void*  pBuffer       // [out] Build the PM4 packet in this buffer.
    ) const
{
    PAL_ASSERT(endRegAddr >= startRegAddr);

    const size_t packetSize = PM4_CMD_SET_DATA_DWORDS + endRegAddr - startRegAddr + 1;
    auto*const   pPacket    = static_cast<PM4CMDSETDATA*>(pBuffer);

    if (m_chipFamily == GfxIpLevel::GfxIp6)
    {
        PAL_ASSERT(IsConfigReg(startRegAddr) && IsConfigReg(endRegAddr));

        pPacket->header.u32All = Type3Header(IT_SET_CONFIG_REG, packetSize);
        pPacket->ordinal2      = SetDataOrdinal2(startRegAddr - CONFIG_SPACE_START);
    }
    else
    {
#if PAL_ENABLE_PRINTS_ASSERTS
        CheckShadowedUserConfigRegs(startRegAddr, endRegAddr);
#endif

        pPacket->header.u32All = Type3Header(IT_SET_UCONFIG_REG__CI__VI, packetSize);
        pPacket->ordinal2      = SetDataOrdinal2(startRegAddr - UCONFIG_SPACE_START__CI__VI);
    }

    return packetSize;
}

// =====================================================================================================================
// Builds a PM4 packet which sets a sequence of context registers starting with startRegAddr and ending with endRegAddr
// (inclusive). Note that unlike R6xx/EG/NI, GCN has no compute contexts, so all context registers are for graphics.
// Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildSetSeqContextRegs(
    uint32 startRegAddr,
    uint32 endRegAddr,
    void*  pBuffer       // [out] Build the PM4 packet in this buffer.
    ) const
{
    PAL_ASSERT(endRegAddr >= startRegAddr);
#if PAL_ENABLE_PRINTS_ASSERTS
    CheckShadowedContextRegs(startRegAddr, endRegAddr);
#endif

    const size_t packetSize = PM4_CMD_SET_DATA_DWORDS + endRegAddr - startRegAddr + 1;
    auto*const   pPacket    = static_cast<PM4CMDSETDATA*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_SET_CONTEXT_REG, packetSize);
    pPacket->ordinal2      = SetDataOrdinal2(startRegAddr - CONTEXT_SPACE_START);

    return packetSize;
}

// =====================================================================================================================
// Builds a PM4 packet which sets a sequence of Graphics SH registers starting with startRegAddr and ending with
// endRegAddr (inclusive). Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildSetSeqShRegs(
    uint32        startRegAddr,
    uint32        endRegAddr,
    PM4ShaderType shaderType,
    void*         pBuffer       // [out] Build the PM4 packet in this buffer.
    ) const
{
    PAL_ASSERT(endRegAddr >= startRegAddr);
#if PAL_ENABLE_PRINTS_ASSERTS
    CheckShadowedShRegs(shaderType, startRegAddr, endRegAddr);
#endif

    const size_t packetSize = PM4_CMD_SET_DATA_DWORDS + endRegAddr - startRegAddr + 1;
    auto*const   pPacket    = static_cast<PM4CMDSETDATA*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_SET_SH_REG, packetSize, shaderType);
    pPacket->ordinal2      = SetDataOrdinal2(startRegAddr - PERSISTENT_SPACE_START);

    return packetSize;
}

// =====================================================================================================================
// Builds a PM4 packet which sets a sequence of Graphics SH registers starting with startRegAddr and ending with
// endRegAddr (inclusive). Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildSetSeqShRegsIndex(
    uint32        startRegAddr,
    uint32        endRegAddr,
    PM4ShaderType shaderType,
    uint32        index,
    void*         pBuffer       // [out] Build the PM4 packet in this buffer.
    ) const
{
    PAL_ASSERT(endRegAddr >= startRegAddr);
#if PAL_ENABLE_PRINTS_ASSERTS
    CheckShadowedShRegs(shaderType, startRegAddr, endRegAddr);
#endif

    const size_t packetSize = PM4_CMD_SET_DATA_DWORDS + endRegAddr - startRegAddr + 1;
    auto*const   pPacket = static_cast<PM4CMDSETDATA*>(pBuffer);

    // Revert to the 'old' packet if there is no support for SET_SH_REG_INDEX
    if (m_device.ChipProperties().gfx6.supportSetShIndexPkt == 0)
    {
        pPacket->header.u32All = Type3Header(IT_SET_SH_REG, packetSize, shaderType);
        pPacket->ordinal2      = SetDataOrdinal2(startRegAddr - PERSISTENT_SPACE_START);
    }
    else
    {
        pPacket->header.u32All = Type3Header(IT_SET_SH_REG_INDEX__CI__VI, packetSize, shaderType);
        pPacket->ordinal2      = SetDataOrdinal2(startRegAddr - PERSISTENT_SPACE_START, index);
    }

    return packetSize;
}

// =====================================================================================================================
// Builds a SET_PREDICATION packet. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildSetPredication(
    gpusize       gpuVirtAddr,
    bool          predicationBool,    // Controls the polarity of the predication test. E.g., for occlusion predicates,
                                      // true indicates to draw if any pixels passed the Z-test while false indicates
                                      // to draw if no pixels passed the Z-test.
    bool          occlusionHint,      // Controls whether the hardware should wait for all ZPASS data to be written by
                                      // the DB's before proceeding. True chooses to wait until all ZPASS data is ready,
                                      // false chooses to assume that the draw should not be skipped if the ZPASS data
                                      // is not ready yet.
    PredicateType predType,
    bool          continuePredicate,  // Contols how data is accumulated across cmd buffer boundaries. True indicates
                                      // that this predicate is a continuation of the previous one, accumulating data
                                      // between them.
    void*         pBuffer
    ) const
{
    static_assert(static_cast<uint32>(PredicateType::Zpass)     == SET_PRED_ZPASS &&
                  static_cast<uint32>(PredicateType::PrimCount) == SET_PRED_PRIMCOUNT &&
                  static_cast<uint32>(PredicateType::Boolean)   == SET_PRED_MEM,
                  "Unexpected values for the PredicateType enum.");

    constexpr size_t PacketSize = PM4_CMD_SET_PREDICATION_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDSETPREDICATION*>(pBuffer);

    // The predication memory address must be 16-byte aligned, and cannot be wider than 40 bits.
    PAL_ASSERT(((gpuVirtAddr & 0xF) == 0) && (gpuVirtAddr <= ((1uLL << 40) - 1)));

    pPacket->header.u32All      = Type3Header(IT_SET_PREDICATION, PacketSize);
    pPacket->startAddressLo     = LowPart(gpuVirtAddr);
    pPacket->ordinal3           = 0;
    pPacket->startAddrHi        = HighPart(gpuVirtAddr);
    pPacket->predicationBoolean = (predicationBool ? 1 : 0);
    pPacket->hint               = ((predType == PredicateType::Zpass) && occlusionHint) ? 1 : 0;
    pPacket->predOp             = static_cast<uint32>(predType);
    pPacket->continueBit        = ((predType == PredicateType::Zpass) && continuePredicate) ? 1 : 0;

    return PacketSize;
}

// =====================================================================================================================
// Builds a STRMOUT_BUFFER_UPDATE packet. Returns the size of the PM4 command assembled, in DWORDs.
// All operations except STRMOUT_CNTL_OFFSET_SEL_NONE will internally issue a VGT_STREAMOUT_RESET event.
size_t CmdUtil::BuildStrmoutBufferUpdate(
    uint32  bufferId,
    uint32  sourceSelect,   // Controls which streamout update operation to perform.
    uint32  explicitOffset, // When sourceSelect = EXPLICIT_OFFSET, this is the value to be written into the buffer
                            // filled size counter.
    gpusize dstGpuVirtAddr, // When sourceSelect = NONE, this is the GPU virtual address where the buffer filled size
                            // will be written-to.
    gpusize srcGpuVirtAddr, // When sourceSelect = READ_SRC_ADDRESS, this is the GPU virtual address where the buffer
                            // filled size will be read from.
    void*   pBuffer// [out] Build the PM4 packet in this buffer.
    ) const
{
    constexpr size_t PacketSize = PM4_CMD_STRMOUT_BUFFER_UPDATE_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDSTRMOUTBUFFERUPDATE*>(pBuffer);

    pPacket->header.u32All      = Type3Header(IT_STRMOUT_BUFFER_UPDATE, PacketSize);
    pPacket->ordinal2           = 0;
    pPacket->offsetSourceSelect = sourceSelect;
    pPacket->bufferSelect       = bufferId;
    pPacket->ordinal3           = 0;
    pPacket->ordinal4           = 0;
    pPacket->ordinal5           = 0;

    // The dataType field was added in uCode version #26 to support stream-out size in bytes.
    PAL_ASSERT(m_device.EngineProperties().cpUcodeVersion >= 26);
    constexpr uint32 DataType = 1; // 1 Indicates the GPU memory buffer-filled-size is in bytes.

    switch (sourceSelect)
    {
    case STRMOUT_CNTL_OFFSET_SEL_EXPLICT_OFFSET:
        pPacket->bufferOffset = explicitOffset;
        break;
    case STRMOUT_CNTL_OFFSET_SEL_READ_VGT_BUFFER_FILLED_SIZE:
        // No additional members need to be set for this operation.
        break;
    case STRMOUT_CNTL_OFFSET_SEL_READ_SRC_ADDRESS:
        pPacket->srcAddressLo = LowPart(srcGpuVirtAddr);
        pPacket->srcAddressHi = HighPart(srcGpuVirtAddr);
        pPacket->dataType     = DataType;
        break;
    case STRMOUT_CNTL_OFFSET_SEL_NONE:
        pPacket->storeBufferFilledSize = 1;
        pPacket->dstAddressLo          = LowPart(dstGpuVirtAddr);
        pPacket->dstAddressHi          = HighPart(dstGpuVirtAddr);
        pPacket->dataType              = DataType;
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a SURFACE_SYNC command. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildSurfaceSync(
    regCP_COHER_CNTL cpCoherCntl, // CP coher_cntl value (controls which sync actions occur).
    uint32           syncEngine,  // Sync engine (PFP or ME).
    gpusize          baseAddress, // Base address for sync. Set to 0 for full sync.
    gpusize          sizeBytes,   // Size of sync range in bytes. Set to all Fs for full sync.
    void*            pBuffer      // [out] Build the PM4 packet in this buffer.
    ) const
{
    constexpr size_t PacketSize = PM4_CMD_SURFACE_SYNC_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDSURFACESYNC*>(pBuffer);

    if ((m_chipFamily >= GfxIpLevel::GfxIp8) && (cpCoherCntl.bits.TC_ACTION_ENA == 1))
    {
        // On Gfx8+, TC_WB_ACTION_ENA__CI__VI must go together with the TC_ACTION_ENA bit to flush and invalidate the L2
        // cache.
        cpCoherCntl.bits.TC_WB_ACTION_ENA__CI__VI = 1;
    }

    pPacket->header.u32All = Type3Header(IT_SURFACE_SYNC, PacketSize);
    pPacket->coherCntl     = cpCoherCntl.u32All;
    pPacket->engine        = syncEngine;
    pPacket->pollInterval  = Pal::Device::PollInterval;

    // Need to align-down the given base address and then add the difference to the size, and align that new size.
    // Note that if sizeBytes is equal to FullSyncSize we should leave it as-is.
    constexpr gpusize Alignment = 256;
    constexpr gpusize SizeShift = 8;

    const gpusize alignedAddress = Pow2AlignDown(baseAddress, Alignment);
    const gpusize alignedSize    = (sizeBytes == FullSyncSize)
                                        ? FullSyncSize
                                        : Pow2Align((sizeBytes + (baseAddress - alignedAddress)), Alignment);

    pPacket->cpCoherBase.bits.COHER_BASE_256B = Get256BAddrLo(alignedAddress);
    pPacket->cpCoherSize.bits.COHER_SIZE_256B = (alignedSize >> SizeShift);

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 command to stall the CP ME until the CP's DMA engine has finished all previous CP_DMA/DMA_DATA commands.
// Returns the size of the PM4 command written, in DWORDs.
size_t CmdUtil::BuildWaitDmaData(
    void* pBuffer // [out] Build the PM4 packet in this buffer.
    ) const
{
    // The most efficient way to do this is to issue a dummy DMA that copies zero bytes.
    // The DMA engine will see that there's no work to do and skip this DMA request, however, the ME microcode will
    // see the sync flag and still wait for all DMAs to complete.
    DmaDataInfo dmaDataInfo = {};
    dmaDataInfo.dstSel   = CPDMA_DST_SEL_DST_ADDR;
    dmaDataInfo.srcSel   = CPDMA_SRC_SEL_SRC_ADDR;
    dmaDataInfo.dstAddr  = 0;
    dmaDataInfo.srcAddr  = 0;
    dmaDataInfo.numBytes = 0;
    dmaDataInfo.sync     = true;
    dmaDataInfo.usePfp   = false;

    return BuildDmaData(dmaDataInfo, pBuffer);
}

// =====================================================================================================================
// Builds a PM4 command to stall the DE until the CE counter is positive, then decrements the CE counter. Returns the
// size of the PM4 command written, in DWORDs.
size_t CmdUtil::BuildWaitOnCeCounter(
    bool  invalidateKcache,
    void* pBuffer // [out] Build the PM4 packet in this buffer.
    ) const
{
    constexpr size_t PacketSize = PM4_CMD_WAIT_ON_CE_COUNTER_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDWAITONCECOUNTER*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_WAIT_ON_CE_COUNTER, PacketSize);
    pPacket->ordinal2      = 0;
    pPacket->control       = invalidateKcache;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 command to stall the CE until it is less than the specified number of draws ahead of the DE. Returns
// the size of the PM4 command written, in DWORDs.
size_t CmdUtil::BuildWaitOnDeCounterDiff(
    uint32 counterDiff,
    void*  pBuffer      // [out] Build the PM4 packet in this buffer.
    ) const
{
    constexpr size_t PacketSize = PM4_CMD_WAIT_ON_DE_COUNTER_DIFF_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDWAITONDECOUNTERDIFF*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_WAIT_ON_DE_COUNTER_DIFF, PacketSize);
    pPacket->counterDiff   = counterDiff;

    return PacketSize;
}

// =====================================================================================================================
// Builds a set of PM4 commands that update a timestamp value to a known value, writes an EOP timestamp event with a
// known different value then waits for the timestamp value to update. Returns the size of the PM4 command built, in
// DWORDs.
size_t CmdUtil::BuildWaitOnEopEvent(
    VGT_EVENT_TYPE eventType,
    gpusize        gpuAddr,
    void*          pBuffer    // [out] Build the PM4 packet in this buffer.
    ) const
{
    constexpr uint32 ClearedTimestamp   = 0x11111111;
    constexpr uint32 CompletedTimestamp = 0x22222222;

    // These are the only event types supported by this packet sequence.
    PAL_ASSERT ((eventType == BOTTOM_OF_PIPE_TS)        ||
                (eventType == CACHE_FLUSH_TS)           ||
                (eventType == FLUSH_AND_INV_CB_DATA_TS) ||
                (eventType == CACHE_FLUSH_AND_INV_TS_EVENT));

    // Write a known value to the timestamp.
    WriteDataInfo writeData = {};
    writeData.dstAddr   = gpuAddr;
    writeData.engineSel = WRITE_DATA_ENGINE_ME;
    writeData.dstSel    = WRITE_DATA_DST_SEL_MEMORY_ASYNC;

    size_t totalSize = BuildWriteData(writeData, ClearedTimestamp, pBuffer);

    // Issue the specified timestamp event.
    totalSize += BuildEventWriteEop(eventType,
                                    gpuAddr,
                                    EVENTWRITEEOP_DATA_SEL_SEND_DATA32,
                                    CompletedTimestamp,
                                    false,
                                    static_cast<uint32*>(pBuffer) + totalSize);

    // Wait on the timestamp value.
    totalSize += BuildWaitRegMem(WAIT_REG_MEM_SPACE_MEMORY,
                                 WAIT_REG_MEM_FUNC_EQUAL,
                                 WAIT_REG_MEM_ENGINE_ME,
                                 gpuAddr,
                                 CompletedTimestamp,
                                 0xFFFFFFFF,
                                 false,
                                 static_cast<uint32*>(pBuffer) + totalSize);

    return totalSize;
}

// =====================================================================================================================
// Builds a set of PM4 commands that update a timestamp value to a known value, writes an EOP timestamp event with a
// known different value then waits for the timestamp value to update. Returns the size of the PM4 command built, in
// DWORDs.
size_t CmdUtil::BuildWaitOnGenericEopEvent(
    VGT_EVENT_TYPE eventType,
    gpusize        gpuAddr,
    bool           forComputeEngine,
    void*          pBuffer           // [out] Build the PM4 packet in this buffer.
    ) const
{
    constexpr uint32 ClearedTimestamp   = 0x11111111;
    constexpr uint32 CompletedTimestamp = 0x22222222;

    // These are the only event types supported by this packet sequence.
    PAL_ASSERT ((eventType == BOTTOM_OF_PIPE_TS)        ||
                (eventType == CACHE_FLUSH_TS)           ||
                (eventType == FLUSH_AND_INV_CB_DATA_TS) ||
                (eventType == CACHE_FLUSH_AND_INV_TS_EVENT));

    // Write a known value to the timestamp.
    WriteDataInfo writeData = {};
    writeData.dstAddr   = gpuAddr;
    writeData.engineSel = WRITE_DATA_ENGINE_ME;
    writeData.dstSel    = WRITE_DATA_DST_SEL_MEMORY_ASYNC;

    size_t totalSize = BuildWriteData(writeData, ClearedTimestamp, pBuffer);

    // Issue the specified timestamp event.
    totalSize += BuildGenericEopEvent(eventType,
                                      gpuAddr,
                                      EVENTWRITEEOP_DATA_SEL_SEND_DATA32,
                                      CompletedTimestamp,
                                      forComputeEngine,
                                      false,
                                      static_cast<uint32*>(pBuffer) + totalSize);

    // Wait on the timestamp value.
    totalSize += BuildWaitRegMem(WAIT_REG_MEM_SPACE_MEMORY,
                                 WAIT_REG_MEM_FUNC_EQUAL,
                                 WAIT_REG_MEM_ENGINE_ME,
                                 gpuAddr,
                                 CompletedTimestamp,
                                 0xFFFFFFFF,
                                 false,
                                 static_cast<uint32*>(pBuffer) + totalSize);

    return totalSize;
}

// =====================================================================================================================
// Builds a set of PM4 commands that update a timestamp value to a known value, writes an EOS timestamp event with a
// known different value then waits for the timestamp value to update. Returns the size of the PM4 command built, in
// DWORDs.
size_t CmdUtil::BuildWaitOnEosEvent(
    VGT_EVENT_TYPE eventType,
    gpusize        gpuAddr,
    void*          pBuffer    // [out] Build the PM4 packet in this buffer.
    ) const
{
    constexpr uint32 ClearedTimestamp   = 0x11111111;
    constexpr uint32 CompletedTimestamp = 0x22222222;

    // These are the only event types supported by this packet sequence.
    PAL_ASSERT ((eventType == PS_DONE) || (eventType == CS_DONE));

    // Write a known value to the timestamp.
    WriteDataInfo writeData = {};
    writeData.dstAddr   = gpuAddr;
    writeData.engineSel = WRITE_DATA_ENGINE_ME;
    writeData.dstSel    = WRITE_DATA_DST_SEL_MEMORY_ASYNC;

    size_t totalSize = BuildWriteData(writeData, ClearedTimestamp, pBuffer);

    // Issue the specified timestamp event.
    totalSize += BuildEventWriteEos(eventType,
                                    gpuAddr,
                                    EVENT_WRITE_EOS_CMD_STORE_32BIT_DATA_TO_MEMORY,
                                    CompletedTimestamp,
                                    0,
                                    0,
                                    static_cast<uint32*>(pBuffer) + totalSize);

    // Wait on the timestamp value.
    totalSize += BuildWaitRegMem(WAIT_REG_MEM_SPACE_MEMORY,
                                 WAIT_REG_MEM_FUNC_EQUAL,
                                 WAIT_REG_MEM_ENGINE_ME,
                                 gpuAddr,
                                 CompletedTimestamp,
                                 0xFFFFFFFF,
                                 false,
                                 static_cast<uint32*>(pBuffer) + totalSize);

    return totalSize;
}

// =====================================================================================================================
// Builds a set of PM4 commands that update a timestamp value to a known value, writes an EOS timestamp event with a
// known different value then waits for the timestamp value to update. Returns the size of the PM4 command built, in
// DWORDs.
size_t CmdUtil::BuildWaitOnGenericEosEvent(
    VGT_EVENT_TYPE eventType,
    gpusize        gpuAddr,
    bool           forComputeEngine,
    void*          pBuffer           // [out] Build the PM4 packet in this buffer.
    ) const
{
    constexpr uint32 ClearedTimestamp   = 0x11111111;
    constexpr uint32 CompletedTimestamp = 0x22222222;

    // These are the only event types supported by this packet sequence.
    PAL_ASSERT ((eventType == PS_DONE) || (eventType == CS_DONE));

    // Write a known value to the timestamp.
    WriteDataInfo writeData = {};
    writeData.dstAddr   = gpuAddr;
    writeData.engineSel = WRITE_DATA_ENGINE_ME;
    writeData.dstSel    = WRITE_DATA_DST_SEL_MEMORY_ASYNC;

    size_t totalSize = BuildWriteData(writeData, ClearedTimestamp, pBuffer);

    // Issue the specified timestamp event.
    totalSize += BuildGenericEosEvent(eventType,
                                      gpuAddr,
                                      EVENT_WRITE_EOS_CMD_STORE_32BIT_DATA_TO_MEMORY,
                                      CompletedTimestamp,
                                      0,
                                      0,
                                      forComputeEngine,
                                      static_cast<uint32*>(pBuffer) + totalSize);

    // Wait on the timestamp value.
    totalSize += BuildWaitRegMem(WAIT_REG_MEM_SPACE_MEMORY,
                                 WAIT_REG_MEM_FUNC_EQUAL,
                                 WAIT_REG_MEM_ENGINE_ME,
                                 gpuAddr,
                                 CompletedTimestamp,
                                 0xFFFFFFFF,
                                 false,
                                 static_cast<uint32*>(pBuffer) + totalSize);

    return totalSize;
}

// =====================================================================================================================
// Builds a WAIT_REG_MEM PM4 packet. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildWaitRegMem(
    uint32        memSpace,
    uint32        function,
    uint32        engine,
    gpusize       addr,
    uint32        reference,
    uint32        mask,
    bool          isSdi,
    void*         pBuffer    // [out] Build the PM4 packet in this buffer.
    ) const
{
    // The destination address must be DWORD aligned.
    PAL_ASSERT((memSpace != WAIT_REG_MEM_SPACE_MEMORY) || IsPow2Aligned(addr, 4));

    constexpr size_t PacketSize = PM4_CMD_WAIT_REG_MEM_DWORDS;
    auto*const       pPacket    = static_cast<PM4CMDWAITREGMEM*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_WAIT_REG_MEM, PacketSize);
    pPacket->ordinal2      = 0;
    pPacket->function      = function;
    pPacket->memSpace      = memSpace;
    pPacket->engine        = engine;
    pPacket->uncached__VI  = (isSdi) ? 1 : 0;
    pPacket->pollAddressLo = LowPart(addr);
    pPacket->pollAddressHi = HighPart(addr);
    pPacket->reference     = reference;
    pPacket->mask          = mask;
    pPacket->pollInterval  = Pal::Device::PollInterval;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 constant engine command to write the specified amount of data from CPU memory into CE RAM. Returns the
// size of the PM4 command written, in DWORDs.
size_t CmdUtil::BuildWriteConstRam(
    const void* pSrcData,       // [in] Pointer to source data in CPU memory
    uint32      ramByteOffset,  // Offset into CE RAM. Must be 4-byte aligned.
    uint32      dwordSize,      // Amount of data to write, in DWORDs
    void*       pBuffer         // [out] Build the PM4 packet in this buffer.
    ) const
{
    const size_t packetSize = PM4_CMD_WRITE_CONST_RAM_DWORDS + dwordSize;
    auto*const   pPacket    = static_cast<PM4CMDCONSTRAMWRITE*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_WRITE_CONST_RAM, packetSize);
    pPacket->ordinal2      = 0;
    pPacket->offset        = ramByteOffset;

    // Copy the data into the buffer after the packet.
    memcpy(pPacket + 1, pSrcData, dwordSize * sizeof(uint32));

    return packetSize;
}

// =====================================================================================================================
// Builds a PM4 packet that writes a single data DWORD into GPU memory. Returns the size of the PM4 command assembled,
// in DWORDs.
size_t CmdUtil::BuildWriteData(
    const WriteDataInfo& info,
    uint32               data,
    void*                pBuffer // [out] Build the PM4 packet in this buffer.
    ) const
{
    // Make sure the address and size are valid. For register writes we don't need the alignment requirement.
    PAL_ASSERT(((info.dstSel == WRITE_DATA_DST_SEL_REGISTER) || (info.dstSel == WRITE_DATA_DST_SEL_GDS)) ||
               (((info.dstAddr & 0x3) == 0)));

    // Make sure the engine selection is valid.
    PAL_ASSERT((info.engineSel == WRITE_DATA_ENGINE_ME)  ||
               (info.engineSel == WRITE_DATA_ENGINE_PFP) ||
               (info.engineSel == WRITE_DATA_ENGINE_CE));

    const size_t packetSize = PM4_CMD_WRITE_DATA_DWORDS + 1;
    auto*const   pPacket    = static_cast<PM4CMDWRITEDATA*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_WRITE_DATA, packetSize, ShaderGraphics, info.predicate);
    pPacket->ordinal2      = 0;
    pPacket->dstSel        = info.dstSel;
    pPacket->wrOneAddr     = info.dontIncrementAddr;
    pPacket->wrConfirm     = (info.dontWriteConfirm == false);
    pPacket->engineSel     = info.engineSel;
    pPacket->dstAddrLo     = LowPart(info.dstAddr);
    pPacket->dstAddrHi     = HighPart(info.dstAddr);

    uint32*const pDataPayload = static_cast<uint32*>(pBuffer) + packetSize - 1;
    *pDataPayload = data;

    return packetSize;
}

// =====================================================================================================================
// Builds a WRITE_DATA PM4 packet. If pData is non-null it will also copy in the data payload. Returns the size of the
// PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildWriteData(
    const WriteDataInfo& info,
    size_t               dwordsToWrite,
    const uint32*        pData,
    void*                pBuffer        // [out] Build the PM4 packet in this buffer.
    ) const
{
    // Make sure the address and size are valid. For register writes we don't need the alignment requirement.
    PAL_ASSERT(((info.dstSel == WRITE_DATA_DST_SEL_REGISTER) || (info.dstSel == WRITE_DATA_DST_SEL_GDS)) ||
               (((info.dstAddr & 0x3) == 0) && (dwordsToWrite > 0)));

    // Make sure the engine selection is valid.
    PAL_ASSERT((info.engineSel == WRITE_DATA_ENGINE_ME)  ||
               (info.engineSel == WRITE_DATA_ENGINE_PFP) ||
               (info.engineSel == WRITE_DATA_ENGINE_CE));

    const size_t packetSize = PM4_CMD_WRITE_DATA_DWORDS + dwordsToWrite;
    auto*const   pPacket    = static_cast<PM4CMDWRITEDATA*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_WRITE_DATA, packetSize, ShaderGraphics, info.predicate);
    pPacket->ordinal2      = 0;
    pPacket->dstSel        = info.dstSel;
    pPacket->wrOneAddr     = info.dontIncrementAddr;
    pPacket->wrConfirm     = (info.dontWriteConfirm == false);
    pPacket->engineSel     = info.engineSel;
    pPacket->dstAddrLo     = LowPart(info.dstAddr);
    pPacket->dstAddrHi     = HighPart(info.dstAddr);

    if (pData != nullptr)
    {
        // Copy the data into the buffer after the packet.
        memcpy(pPacket + 1, pData, dwordsToWrite * sizeof(uint32));
    }

    return packetSize;
}

// =====================================================================================================================
// Builds a WRITE_DATA PM4 packet. If pPeriodData is non-null its contents (of length dwordsPerPeriod) will be copied
// into the data payload periodsToWrite times. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildWriteDataPeriodic(
    const WriteDataInfo& info,
    size_t               dwordsPerPeriod,
    size_t               periodsToWrite,
    const uint32*        pPeriodData,
    void*                pBuffer          // [out] Build the PM4 packet in this buffer.
    ) const
{
    const size_t dwordsToWrite = dwordsPerPeriod * periodsToWrite;

    // Make sure the address and size are valid.
    PAL_ASSERT(((info.dstAddr & 0x3) == 0) && (dwordsToWrite > 0));

    // Make sure the engine selection is valid.
    PAL_ASSERT((info.engineSel == WRITE_DATA_ENGINE_ME)  ||
               (info.engineSel == WRITE_DATA_ENGINE_PFP) ||
               (info.engineSel == WRITE_DATA_ENGINE_CE));

    const size_t packetSize = PM4_CMD_WRITE_DATA_DWORDS + dwordsToWrite;
    auto*const   pPacket    = static_cast<PM4CMDWRITEDATA*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_WRITE_DATA, packetSize, ShaderGraphics, info.predicate);
    pPacket->ordinal2      = 0;
    pPacket->dstSel        = info.dstSel;
    pPacket->wrOneAddr     = info.dontIncrementAddr;
    pPacket->wrConfirm     = (info.dontWriteConfirm == false);
    pPacket->engineSel     = info.engineSel;
    pPacket->dstAddrLo     = LowPart(info.dstAddr);
    pPacket->dstAddrHi     = HighPart(info.dstAddr);

    if (pPeriodData != nullptr)
    {
        // Copy the data into the buffer after the packet.
        const size_t bytesPerPeriod = sizeof(uint32) * dwordsPerPeriod;
        uint32*      pDataSection   = reinterpret_cast<uint32*>(pPacket + 1);

        for (; periodsToWrite > 0; periodsToWrite--)
        {
            memcpy(pDataSection, pPeriodData, bytesPerPeriod);
            pDataSection += dwordsPerPeriod;
        }
    }

    return packetSize;
}

// =====================================================================================================================
// Builds an NOP PM4 packet with the ASCII string comment embedded inside. The comment is preceeded by a signature
// that analysis tools can use to tell that this is a comment.
size_t CmdUtil::BuildCommentString(
    const char* pComment,
    void*       pBuffer) const
{
    const size_t stringLength         = strlen(pComment) + 1;
    const size_t packetSize           = PM4_CMD_NOP_DWORDS + 3 + (stringLength + 3) / sizeof(uint32);
    auto*const   pPacket              = static_cast<PM4CMDNOP*>(pBuffer);
    uint32*      pData                = reinterpret_cast<uint32*>(pPacket + 1);

    PAL_ASSERT(stringLength < CmdBuffer::MaxCommentStringLength);

    // Build header (NOP, signature, size, type)
    pPacket->header.u32All = Type3Header(IT_NOP, static_cast<uint32>(packetSize));
    pData[0]               = CmdBuffer::CommentSignature;
    pData[1]               = static_cast<uint32>(packetSize);
    pData[2]               = static_cast<uint32>(CmdBufferCommentType::String);

    // Append data
    memcpy(pData + 3, pComment, stringLength);

    return packetSize;
}

// =====================================================================================================================
// On GFX7+ CPDMA can read/write through L2.  Issue a BLT of the pipeline data to itself in order to prime its data in
// L2.
void CmdUtil::BuildPipelinePrefetchPm4(
    const PipelineUploader& uploader,
    PipelinePrefetchPm4*    pOutput
    ) const
{
    const PalSettings&     coreSettings = m_device.Settings();
    const Gfx6PalSettings& hwlSettings  = static_cast<const Device*>(m_device.GetGfxDevice())->Settings();

    if ((m_device.ChipProperties().gfxLevel != GfxIpLevel::GfxIp6) && coreSettings.pipelinePrefetchEnable)
    {
        const gpusize prefetchAddr = uploader.PrefetchAddr();
        uint32        prefetchSize = static_cast<uint32>(uploader.PrefetchSize());

        if (coreSettings.shaderPrefetchClampSize != 0)
        {
            prefetchSize = Min(prefetchSize, coreSettings.shaderPrefetchClampSize);
        }

        // The .text section of the code object should be well aligned, but the prefetched data may not be.  In that
        // case, just prefetch what we can without triggering the unaligned CPDMA workaround which would require an
        // indeterminant amount of command space.
        prefetchSize = Pow2AlignDown(prefetchSize, hwlSettings.cpDmaSrcAlignment);

        // We always expect the prefetched portion of the code object to be shader code that must be 256 byte aligned.
        PAL_ASSERT(IsPow2Aligned(prefetchAddr, hwlSettings.cpDmaSrcAlignment));

        const auto& gfx6Device = static_cast<const Pal::Gfx6::Device&>(*m_device.GetGfxDevice());

        DmaDataInfo dmaDataInfo  = {};
        dmaDataInfo.dstAddr      = prefetchAddr;
        dmaDataInfo.dstAddrSpace = CPDMA_ADDR_SPACE_MEM;
        dmaDataInfo.dstSel       = gfx6Device.WaCpDmaHangMcTcAckDrop() ? CPDMA_DST_SEL_DST_ADDR :
                                                                         CPDMA_DST_SEL_DST_ADDR_USING_L2;
        dmaDataInfo.srcAddr      = prefetchAddr;
        dmaDataInfo.srcAddrSpace = CPDMA_ADDR_SPACE_MEM;
        dmaDataInfo.srcSel       = CPDMA_SRC_SEL_SRC_ADDR_USING_L2;
        dmaDataInfo.numBytes     = prefetchSize;
        dmaDataInfo.disableWc    = true;

        const size_t dmaCmdSize = BuildDmaData(dmaDataInfo, &pOutput->dmaData);
        pOutput->spaceNeeded = sizeof(PM4DMADATA) / sizeof(uint32);

        // If this triggers, we just corrupted some memory.
        PAL_ASSERT(dmaCmdSize == pOutput->spaceNeeded);
    }
    else
    {
        pOutput->spaceNeeded = 0;
    }
}

#if PAL_ENABLE_PRINTS_ASSERTS
// =====================================================================================================================
// Helper function which verifies that the specified context register falls within one of the ranges which are shadowed
// when mid command buffer preemption is enabled.
void CmdUtil::CheckShadowedContextReg(
    uint32 regAddr
    ) const
{
    PAL_ASSERT(IsContextReg(regAddr));

    if (m_verifyShadowedRegisters)
    {
        PAL_ASSERT(m_chipFamily >= GfxIpLevel::GfxIp8);
        if (IsRegisterInRangeList(regAddr, &NonShadowedRangesGfx8[0], NumNonShadowedRangesGfx8) == false)
        {
            if (m_device.ChipProperties().gfx6.rbReconfigureEnabled)
            {
                PAL_ASSERT((IsRegisterInRangeList((regAddr - CONTEXT_SPACE_START),
                                                  &ContextShadowRangeRbReconfig[0],
                                                  NumContextShadowRangesRbReconfig)));
            }
            else
            {
                PAL_ASSERT(IsRegisterInRangeList((regAddr - CONTEXT_SPACE_START),
                                                 &ContextShadowRange[0],
                                                 NumContextShadowRanges));
            }
        }
    }
}

// =====================================================================================================================
// Helper function which verifies that the specified set of sequential context registers falls within one of the ranges
// which are shadowed when mid command buffer preemption is enabled.
void CmdUtil::CheckShadowedContextRegs(
    uint32 startRegAddr,
    uint32 endRegAddr
    ) const
{
    PAL_ASSERT(IsContextReg(startRegAddr) && IsContextReg(endRegAddr));

    if (m_verifyShadowedRegisters)
    {
        PAL_ASSERT(m_chipFamily >= GfxIpLevel::GfxIp8);
        if (false == AreRegistersInRangeList(startRegAddr,
                                             endRegAddr,
                                             &NonShadowedRangesGfx8[0],
                                             NumNonShadowedRangesGfx8))
        {
            if (m_device.ChipProperties().gfx6.rbReconfigureEnabled)
            {
                PAL_ASSERT(AreRegistersInRangeList((startRegAddr - CONTEXT_SPACE_START),
                                                   (endRegAddr - CONTEXT_SPACE_START),
                                                   &ContextShadowRangeRbReconfig[0],
                                                   NumContextShadowRangesRbReconfig));
            }
            else
            {
                PAL_ASSERT(AreRegistersInRangeList((startRegAddr - CONTEXT_SPACE_START),
                                                   (endRegAddr - CONTEXT_SPACE_START),
                                                   &ContextShadowRange[0],
                                                   NumContextShadowRanges));
            }
        }
    }
}

// =====================================================================================================================
// Helper function which verifies that the specified SH register falls within one of the ranges which are shadowed when
// mid command buffer preemption is enabled.
void CmdUtil::CheckShadowedShReg(
    PM4ShaderType shaderType,
    uint32        regAddr
    ) const
{
    PAL_ASSERT(IsShReg(regAddr));

    if (m_verifyShadowedRegisters)
    {
        PAL_ASSERT(m_chipFamily >= GfxIpLevel::GfxIp8);
        if (IsRegisterInRangeList(regAddr, &NonShadowedRangesGfx8[0], NumNonShadowedRangesGfx8) == false)
        {
            if (shaderType == ShaderGraphics)
            {
                PAL_ASSERT(IsRegisterInRangeList((regAddr - PERSISTENT_SPACE_START),
                                                 &GfxShShadowRange[0],
                                                 NumGfxShShadowRanges));
            }
            else
            {
                PAL_ASSERT(IsRegisterInRangeList((regAddr - PERSISTENT_SPACE_START),
                                                 &CsShShadowRange[0],
                                                 NumCsShShadowRanges));
            }
        }
    }
}

// =====================================================================================================================
// Helper function which verifies that the specified set of sequential SH registers falls within one of the ranges which
// are shadowed when mid command buffer preemption is enabled.
void CmdUtil::CheckShadowedShRegs(
    PM4ShaderType shaderType,
    uint32        startRegAddr,
    uint32        endRegAddr
    ) const
{
    PAL_ASSERT(IsShReg(startRegAddr) && IsShReg(endRegAddr));

    if (m_verifyShadowedRegisters)
    {
        PAL_ASSERT(m_chipFamily >= GfxIpLevel::GfxIp8);
        if (false == AreRegistersInRangeList(startRegAddr,
                                             endRegAddr,
                                             &NonShadowedRangesGfx8[0],
                                             NumNonShadowedRangesGfx8))
        {
            if (shaderType == ShaderGraphics)
            {
                PAL_ASSERT(AreRegistersInRangeList((startRegAddr - PERSISTENT_SPACE_START),
                                                   (endRegAddr - PERSISTENT_SPACE_START),
                                                   &GfxShShadowRange[0],
                                                   NumGfxShShadowRanges));
            }
            else
            {
                PAL_ASSERT(AreRegistersInRangeList((startRegAddr - PERSISTENT_SPACE_START),
                                                   (endRegAddr - PERSISTENT_SPACE_START),
                                                   &CsShShadowRange[0],
                                                   NumCsShShadowRanges));
            }
        }
    }
}

// =====================================================================================================================
// Helper function which verifies that the specified user-config register falls within one of the ranges which are
// shadowed when mid command buffer preemption is enabled.
void CmdUtil::CheckShadowedUserConfigReg(
    uint32 regAddr
    ) const
{
    PAL_ASSERT(IsUserConfigReg(regAddr));

    if (m_verifyShadowedRegisters)
    {
        PAL_ASSERT(m_chipFamily >= GfxIpLevel::GfxIp8);
        if (IsRegisterInRangeList(regAddr, &NonShadowedRangesGfx8[0], NumNonShadowedRangesGfx8) == false)
        {
            PAL_ASSERT(IsRegisterInRangeList((regAddr - UCONFIG_SPACE_START__CI__VI),
                                             &UserConfigShadowRangeGfx7[0],
                                             NumUserConfigShadowRangesGfx7));
        }
    }
}

// =====================================================================================================================
// Helper function which verifies that the specified set of sequential user-config registers falls within one of the
// ranges which are shadowed when mid command buffer preemption is enabled.
void CmdUtil::CheckShadowedUserConfigRegs(
    uint32 startRegAddr,
    uint32 endRegAddr
    ) const
{
    PAL_ASSERT(IsUserConfigReg(startRegAddr) && IsUserConfigReg(endRegAddr));

    if (m_verifyShadowedRegisters)
    {
        PAL_ASSERT(m_chipFamily >= GfxIpLevel::GfxIp8);
        if (false == AreRegistersInRangeList(startRegAddr,
                                             endRegAddr,
                                             &NonShadowedRangesGfx8[0],
                                             NumNonShadowedRangesGfx8))
        {
            PAL_ASSERT(AreRegistersInRangeList((startRegAddr - UCONFIG_SPACE_START__CI__VI),
                                               (endRegAddr - UCONFIG_SPACE_START__CI__VI),
                                               &UserConfigShadowRangeGfx7[0],
                                               NumUserConfigShadowRangesGfx7));
        }
    }
}
#endif

} // Gfx6
} // Pal
