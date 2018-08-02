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

#pragma once

#include "core/hw/gfxip/gfx9/gfx9Chip.h"

namespace Pal
{

// Forward decl's
class  Device;
struct GpuChipProperties;
struct ThreadTraceInfo;

namespace Gfx9
{

namespace PerfCtrInfo
{

extern void   InitPerfCtrInfo(GpuChipProperties* pProps);

// Maximum number of ShaderEngines
constexpr uint32 MaxNumShaderEngines = 4;
// Maximum number of instances per shader array (SH): max number of CU's.
constexpr uint32 MaxNumInstances      = 16;
// Maximum number of instances per GPU block (incl. max. possible shader arrays: either two
// SE's and 2 SH's per SE, or 4 SE's with one SH each).
constexpr uint32 MaxNumBlockInstances = (MaxNumInstances * MaxNumShaderEngines);
// Defines an invalid counter ID.
constexpr uint32 InvalidCounterId     = 0xFFFFFFFF;
// Maximum number of perf-ctr select registers per counter.
constexpr uint32 MaxPerfCtrSelectReg  = 2;

// Default SIMD mask for SQ counters: (enable all four SIMD's).
constexpr uint32 DefaultSqSelectSimdMask = 0xF;
// Default Bank mask for SQ counters.
constexpr uint32 DefaultSqSelectBankMask = 0xF;
// Default Client mask for SQ counters.
constexpr uint32 DefaultSqSelectClientMask = 0xF;

// Constants defining the maximum event value for each GPU block: any event ID in the range
// [0, MaxEvent) is valid.

// Gfx9 Specific
constexpr uint32 Gfx9PerfCtrRlcMaxEvent    = 7; //< RLC, doesn't have enumerations, look in reg spec

constexpr uint32 Gfx9PerfCtrlEaMaxEvent     = 77;

constexpr uint32 Gfx9PerfCtrlAtcMaxEvent    = 23;
constexpr uint32 Gfx9PerfCtrlAtcL2MaxEvent  = 8;

constexpr uint32 Gfx9PerfCtrlRpbMaxEvent    = 63;

constexpr uint32 Gfx9PerfCtrlMcVmL2MaxEvent = 21;

constexpr uint32 Gfx9PerfCtrRmiMaxEvent    = (RMI_PERF_SEL_RMI_RB_EARLY_WRACK_NACK3 + 1);

// Max Streaming Counters in a block instance (Gfx7+)
constexpr uint32 Gfx9MaxStreamingCounters = 16;
// The number of streaming perf counters packed into one summary counter (Gfx7+)
constexpr uint32 Gfx9StreamingCtrsPerSummaryCtr = 4;

// Shift values required for programming PERF_SEL fields for streaming perf counters. While the bit-widths of the
// PERF_SEL fields may vary, these shift values are the same for all perf counters.
constexpr uint32 Gfx9PerfCounterPerfSel0Shift = CB_PERFCOUNTER0_SELECT__PERF_SEL__SHIFT;
constexpr uint32 Gfx9PerfCounterPerfSel1Shift = CB_PERFCOUNTER0_SELECT__PERF_SEL1__SHIFT;
constexpr uint32 Gfx9PerfCounterCntrModeShift = CB_PERFCOUNTER0_SELECT__CNTR_MODE__SHIFT;

// Constants defining the number of counters per block:
// CB/DB/PA/SC/SX/SQ/TA/TCP/TCC/TCA/GDS/VGT/IA/CPG/CPC/CPF/SPI/TD support variable bit widths
// via the CNTR_MODE field. This is only for streaming counters.

// Gfx9 Specific. These are based off of the number of perf-counter select registers that exist for each block.
constexpr uint32 Gfx9NumCbCounters     = 4;   //< CB
constexpr uint32 Gfx9NumCpcCounters    = 2;   //< CPC
constexpr uint32 Gfx9NumCpfCounters    = 2;   //< CPF
constexpr uint32 Gfx9NumCpgCounters    = 2;   //< CPG
constexpr uint32 Gfx9NumDbCounters     = 4;   //< DB
constexpr uint32 Gfx9NumGdsCounters    = 4;   //< GDS
constexpr uint32 Gfx9NumGrbmCounters   = 2;   //< GRBM
constexpr uint32 Gfx9NumGrbmseCounters = 4;   //< GRBMSE
constexpr uint32 Gfx9NumIaCounters     = 4;   //< IA
constexpr uint32 Gfx9NumPaCounters     = 4;   //< PA-SU block
constexpr uint32 Gfx9NumScCounters     = 8;   //< PA-SC
constexpr uint32 Gfx9NumRlcCounters    = 2;   //< RLC
constexpr uint32 Gfx9NumSdmaCounters   = 2;   //< SDMA
constexpr uint32 Gfx9NumSpiCounters    = 6;   //< SPI
constexpr uint32 Gfx9NumSqCounters     = 16;  //< SQ
constexpr uint32 Gfx9NumSxCounters     = 4;   //< SX
constexpr uint32 Gfx9NumTaCounters     = 2;   //< TA
constexpr uint32 Gfx9NumTcaCounters    = 4;   //< TCA
constexpr uint32 Gfx9NumTccCounters    = 4;   //< TCC
constexpr uint32 Gfx9NumTcpCounters    = 4;   //< TCP
constexpr uint32 Gfx9NumTdCounters     = 2;   //< TD
constexpr uint32 Gfx9NumVgtCounters    = 4;   //< VGT
constexpr uint32 Gfx9NumWdCounters     = 4;   //< WD
constexpr uint32 Gfx9NumEaCounters     = 2;   //< EA
constexpr uint32 Gfx9NumAtcCounters    = 4;   //< ATC
constexpr uint32 Gfx9NumAtcL2Counters  = 2;   //< ATC L2
constexpr uint32 Gfx9NumMcVmL2Counters = 8;   //< MC VM L2
constexpr uint32 Gfx9NumRpbCounters    = 4;   //< RPB
constexpr uint32 Gfx9NumRmiCounters    = 4;   //< RMI

/// Maximum thread trace buffer size: 128MB per Engine.
constexpr size_t MaximumBufferSize = (128 * 1024 * 1024);
/// Default thread trace buffer size: 1MB per Engine.
constexpr size_t DefaultBufferSize = (1024 * 1024);
/// Thread trace buffer size and base address alignment shift: 12 bits (4KB)
constexpr uint32 BufferAlignShift = 12;
/// Thread trace buffer size and base address alignment
constexpr size_t BufferAlignment = (0x1 << BufferAlignShift);

// Performance monitoring state for disabling and resetting counters.
constexpr uint32 PerfmonDisableAndReset = 0;
// Performance monitoring state for starting counters.
constexpr uint32 PerfmonStartCounting   = 1;
// Performance monitoring state for stopping ("freezing") counters.
constexpr uint32 PerfmonStopCounting    = 2;

enum Gfx9SpmGlobalBlockSelect : uint32
{
    Cpg = 0x0,
    Cpc = 0x1,
    Cpf = 0x2,
    Gds = 0x3,
    Tcc = 0x4,
    Tca = 0x5,
    Ia  = 0x6
};

enum Gfx9SpmSeBlockSelect : uint32
{
    Cb  = 0x0,
    Db  = 0x1,
    Pa  = 0x2,
    Sx  = 0x3,
    Sc  = 0x4,
    Ta  = 0x5,
    Td  = 0x6,
    Tcp = 0x7,
    Spi = 0x8,
    Sqg = 0x9,
    Vgt = 0xA,
    Rmi = 0xB
};

} // PerfExperiment
} // Gfx9
} // Pal
