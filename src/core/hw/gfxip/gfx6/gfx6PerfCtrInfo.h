/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx6/gfx6Chip.h"

namespace Pal
{

// Forward decl's
class  Device;
struct GpuChipProperties;
struct PerfTraceInfo;

namespace Gfx6
{

namespace PerfCtrInfo
{

extern void   InitPerfCtrInfo(GpuChipProperties* pProps);
extern Result ValidateTraceOptions(const Pal::Device& device, const PerfTraceInfo& info);

// These registers do exist on *some* Gfx8 variations. The Gfx8 headers used to create the merged headers don't include
// them though so they got the __SI__CI tag, but we know better, so we redefine them here without their tags for
// clarity.
typedef regMC_SEQ_PERF_SEQ_CTL__SI__CI regMC_SEQ_PERF_SEQ_CTL;
typedef regMC_SEQ_PERF_CNTL__SI__CI    regMC_SEQ_PERF_CNTL;
typedef regMC_SEQ_PERF_CNTL_1__SI__CI  regMC_SEQ_PERF_CNTL_1;

#define mmMC_SEQ_PERF_SEQ_CNT_A_I0              mmMC_SEQ_PERF_SEQ_CNT_A_I0__SI__CI
#define mmMC_SEQ_PERF_SEQ_CNT_A_I1              mmMC_SEQ_PERF_SEQ_CNT_A_I1__SI__CI
#define mmMC_SEQ_PERF_SEQ_CNT_B_I0              mmMC_SEQ_PERF_SEQ_CNT_B_I0__SI__CI
#define mmMC_SEQ_PERF_SEQ_CNT_B_I1              mmMC_SEQ_PERF_SEQ_CNT_B_I1__SI__CI
#define mmMC_SEQ_PERF_SEQ_CNT_C_I0              mmMC_SEQ_PERF_SEQ_CNT_C_I0__SI__CI
#define mmMC_SEQ_PERF_SEQ_CNT_C_I1              mmMC_SEQ_PERF_SEQ_CNT_C_I1__SI__CI
#define mmMC_SEQ_PERF_SEQ_CTL                   mmMC_SEQ_PERF_SEQ_CTL__SI__CI
#define mmMC_SEQ_PERF_CNTL                      mmMC_SEQ_PERF_CNTL__SI__CI
#define mmMC_SEQ_PERF_CNTL_1                    mmMC_SEQ_PERF_CNTL_1__SI__CI

// Maximum number of instances per shader array (SH): max number of CU's.
constexpr uint32 MaxNumInstances      = 16;
// Maximum number of instances per GPU block (incl. max. possible shader arrays: either two
// SE's and 2 SH's per SE, or 4 SE's with one SH each).
constexpr uint32 MaxNumBlockInstances = (MaxNumInstances * 4);
// Defines an invalid counter ID.
constexpr uint32 InvalidCounterId     = 0xFFFFFFFF;
// Maximum number of perf-ctr select registers per counter.
constexpr uint32 MaxPerfCtrSelectReg  = 2;

// Number of MC_SEQ channels per MCD tile.
constexpr uint32 NumMcChannels = 2;
// Default SIMD mask for SQ counters: (enable all four SIMD's).
constexpr uint32 DefaultSqSelectSimdMask = 0xF;
// Default Bank mask for SQ counters.
constexpr uint32 DefaultSqSelectBankMask = 0xF;
// Default Client mask for SQ counters.
constexpr uint32 DefaultSqSelectClientMask = 0xF;

// Max Streaming Counters in a block instance (Gfx7+)
constexpr uint32 Gfx7MaxStreamingCounters = 16;
// The number of streaming perf counters packed into one summary counter (Gfx7+)
constexpr uint32 Gfx7StreamingCtrsPerSummaryCtr = 4;

// Constants defining the maximum event value for each GPU block: any event ID in the range
// [0, MaxEvent) is valid.

// Gfx6 Specific
constexpr uint32 Gfx6PerfCtrCbMaxEvent     = 215;     //< CB
constexpr uint32 Gfx6PerfCtrCpMaxEvent     = 46;      //< CP
constexpr uint32 Gfx6PerfCtrDbMaxEvent     = 249;     //< DB
constexpr uint32 Gfx6PerfCtrDrmdmaMaxEvent = 55;      //< DRMDMA
constexpr uint32 Gfx6PerfCtrGdsMaxEvent    = 65;      //< GDS
constexpr uint32 Gfx6PerfCtrGrbmMaxEvent   = 29;      //< GRBM
constexpr uint32 Gfx6PerfCtrGrbmseMaxEvent = 15;      //< GRBMSE
constexpr uint32 Gfx6PerfCtrIaMaxEvent     = 22;      //< IA
constexpr uint32 Gfx6PerfCtrMcSeqMaxEvent  = 22;      //< MC_SEQ
constexpr uint32 Gfx6PerfCtrPaMaxEvent     = 136;     //< PA
constexpr uint32 Gfx6PerfCtrRlcMaxEvent    = 256;     //< RLC
constexpr uint32 Gfx6PerfCtrScMaxEvent     = 292;     //< SC
constexpr uint32 Gfx6PerfCtrSpiMaxEvent    = 189;     //< SPI
constexpr uint32 Gfx6PerfCtrSqMaxEvent     = 399;     //< SQ
constexpr uint32 Gfx6PerfCtrTaMaxEvent     = 106;     //< TA
constexpr uint32 Gfx6PerfCtrTcaMaxEvent    = 35;      //< TCA
constexpr uint32 Gfx6PerfCtrTccMaxEvent    = 128;     //< TCC
constexpr uint32 Gfx6PerfCtrTcpMaxEvent    = 110;     //< TCP
constexpr uint32 Gfx6PerfCtrTdMaxEvent     = 49;      //< TD
constexpr uint32 Gfx6PerfCtrSrbmMaxEvent   = 18;      //< SRBM
constexpr uint32 Gfx6PerfCtrSxMaxEvent     = 32;      //< SX
constexpr uint32 Gfx6PerfCtrVgtMaxEvent    = 140;     //< VGT

// Gfx7 Specific
constexpr uint32 Gfx7PerfCtrCpfMaxEvent    = 17;      //< CPF
constexpr uint32 Gfx7PerfCtrCpgMaxEvent    = 46;      //< CPG
constexpr uint32 Gfx7PerfCtrCpcMaxEvent    = 22;      //< CPC
constexpr uint32 Gfx7PerfCtrCbMaxEvent     = 226;     //< CB
constexpr uint32 Gfx7PerfCtrDbMaxEvent     = 257;     //< DB
constexpr uint32 Gfx7PerfCtrGrbmMaxEvent   = 34;      //< GRBM
constexpr uint32 Gfx7PerfCtrSrbmMaxEvent   = 19;      //< SRBM
constexpr uint32 Gfx7PerfCtrRlcMaxEvent    = 7;       //< RLC
constexpr uint32 Gfx7PerfCtrPaMaxEvent     = 153;     //< PA
constexpr uint32 Gfx7PerfCtrScMaxEvent     = 395;     //< SC
constexpr uint32 Gfx7PerfCtrSpiMaxEvent    = 186;     //< SPI
constexpr uint32 Gfx7PerfCtrSqMaxEvent     = 251;     //< SQ
constexpr uint32 Gfx7PerfCtrTaMaxEvent     = 111;     //< TA
constexpr uint32 Gfx7PerfCtrTdMaxEvent     = 55;      //< TD
constexpr uint32 Gfx7PerfCtrTcpMaxEvent    = 154;     //< TCP
constexpr uint32 Gfx7PerfCtrTccMaxEvent    = 160;     //< TCC
constexpr uint32 Gfx7PerfCtrTcaMaxEvent    = 39;      //< TCA
constexpr uint32 Gfx7PerfCtrTcsMaxEvent    = 128;     //< TCS
constexpr uint32 Gfx7PerfCtrGdsMaxEvent    = 121;     //< GDS
constexpr uint32 Gfx7PerfCtrSdmaMaxEvent   = 60;      //< SDMA
constexpr uint32 Gfx7PerfCtrGrbmseMaxEvent = 15;      //< GRBMSE
constexpr uint32 Gfx7PerfCtrSxMaxEvent     = 32;      //< SX
constexpr uint32 Gfx7PerfCtrVgtMaxEvent    = 140;     //< VGT
constexpr uint32 Gfx7PerfCtrIaMaxEvent     = 22;      //< IA
constexpr uint32 Gfx7PerfCtrMcSeqMaxEvent  = 22;      //< MC_SEQ
constexpr uint32 Gfx7PerfCtrWdMaxEvent     = 10;      //< WD

// Gfx8 Specific
constexpr uint32 Gfx8PerfCtrCpfMaxEvent    = 19;      //< CPF
constexpr uint32 Gfx8PerfCtrCpgMaxEvent    = 48;      //< CPG
constexpr uint32 Gfx8PerfCtrCpcMaxEvent    = 24;      //< CPC
constexpr uint32 Gfx8PerfCtrCbMaxEvent     = 396;     //< CB
constexpr uint32 Gfx8PerfCtrDbMaxEvent     = 257;     //< DB
constexpr uint32 Gfx8PerfCtrGrbmMaxEvent   = 34;      //< GRBM
constexpr uint32 Gfx8PerfCtrSrbmMaxEvent   = 28;      //< SRBM
constexpr uint32 Gfx8PerfCtrRlcMaxEvent    = 7;       //< RLC
constexpr uint32 Gfx8PerfCtrPaMaxEvent     = 153;     //< PA
constexpr uint32 Gfx8PerfCtrScMaxEvent     = 397;     //< SC
constexpr uint32 Gfx8PerfCtrSpiMaxEvent    = 197;     //< SPI
constexpr uint32 Gfx8PerfCtrSqMaxEvent     = 272;     //< SQ
constexpr uint32 Gfx8PerfCtrSqMaxEventFiji = 298;     //< SQ - Fiji
constexpr uint32 Gfx8PerfCtrTaMaxEvent     = 119;     //< TA
constexpr uint32 Gfx8PerfCtrTdMaxEvent     = 55;      //< TD
constexpr uint32 Gfx8PerfCtrTcpMaxEvent    = 180;     //< TCP
constexpr uint32 Gfx8PerfCtrTccMaxEvent    = 192;     //< TCC
constexpr uint32 Gfx8PerfCtrTcaMaxEvent    = 35;      //< TCA
constexpr uint32 Gfx8PerfCtrGdsMaxEvent    = 121;     //< GDS
constexpr uint32 Gfx8PerfCtrSdmaMaxEvent   = 62;      //< SDMA
constexpr uint32 Gfx8PerfCtrGrbmseMaxEvent = 15;      //< GRBMSE
constexpr uint32 Gfx8PerfCtrSxMaxEvent     = 34;      //< SX
constexpr uint32 Gfx8PerfCtrVgtMaxEvent    = 146;     //< VGT
constexpr uint32 Gfx8PerfCtrIaMaxEvent     = 22;      //< IA
constexpr uint32 Gfx8PerfCtrMcSeqMaxEvent  = 22;      //< MC_SEQ
constexpr uint32 Gfx8PerfCtrWdMaxEvent     = 37;      //< WD

// Constants defining the number of counters per block:
// CB/DB/PA/SC/SX/SQ/TA/TCP/TCC/TCA/GDS/VGT/IA/CPG/CPC/CPF/SPI/TD support variable bit widths
// via the CNTR_MODE field. This is only for streaming counters. See CP.doc Spec section 6.14.

// Gfx6 Specific
constexpr uint32 Gfx6NumCbCounters     = 4;   //< CB
constexpr uint32 Gfx6NumCpCounters     = 1;   //< CP
constexpr uint32 Gfx6NumDbCounters     = 4;   //< DB
constexpr uint32 Gfx6NumDrmdmaCounters = 2;   //< DRMDMA
constexpr uint32 Gfx6NumGdsCounters    = 4;   //< GDS
constexpr uint32 Gfx6NumGrbmCounters   = 2;   //< GRBM
constexpr uint32 Gfx6NumGrbmseCounters = 1;   //< GRBMSE
constexpr uint32 Gfx6NumIaCounters     = 4;   //< IA
constexpr uint32 Gfx6NumMcCounters     = 4;   //< MC
constexpr uint32 Gfx6NumPaCounters     = 4;   //< PA
constexpr uint32 Gfx6NumRlcCounters    = 2;   //< RLC
constexpr uint32 Gfx6NumScCounters     = 8;   //< SC
constexpr uint32 Gfx6NumSpiCounters    = 4;   //< SPI
//NOTE: Regspec shows 15 SQ counters, but only 8 are present.
constexpr uint32 Gfx6NumSqCounters     = 8;   //< SQ
constexpr uint32 Gfx6NumSrbmCounters   = 2;   //< SRBM
constexpr uint32 Gfx6NumSxCounters     = 4;   //< SX
constexpr uint32 Gfx6NumTaCounters     = 2;   //< TA
constexpr uint32 Gfx6NumTcaCounters    = 4;   //< TCA
constexpr uint32 Gfx6NumTccCounters    = 4;   //< TCC
constexpr uint32 Gfx6NumTcpCounters    = 4;   //< TCP
constexpr uint32 Gfx6NumTdCounters     = 1;   //< TD
constexpr uint32 Gfx6NumVgtCounters    = 4;   //< VGT

// Gfx7 Specific
constexpr uint32 Gfx7NumCbCounters     = 4;   //< CB
constexpr uint32 Gfx7NumCpcCounters    = 2;   //< CPC
constexpr uint32 Gfx7NumCpfCounters    = 2;   //< CPF
constexpr uint32 Gfx7NumCpgCounters    = 2;   //< CPG
constexpr uint32 Gfx7NumDbCounters     = 4;   //< DB
constexpr uint32 Gfx7NumGdsCounters    = 4;   //< GDS
constexpr uint32 Gfx7NumGrbmCounters   = 2;   //< GRBM
constexpr uint32 Gfx7NumGrbmseCounters = 1;   //< GRBMSE
constexpr uint32 Gfx7NumIaCounters     = 4;   //< IA
constexpr uint32 Gfx7NumMcCounters     = 4;   //< MC
constexpr uint32 Gfx7NumPaCounters     = 4;   //< PA
constexpr uint32 Gfx7NumRlcCounters    = 2;   //< RLC
constexpr uint32 Gfx7NumScCounters     = 8;   //< SC
constexpr uint32 Gfx7NumSdmaCounters   = 2;   //< SDMA
constexpr uint32 Gfx7NumSpiCounters    = 6;   //< SPI
constexpr uint32 Gfx7NumSqCounters     = 16;  //< SQ
constexpr uint32 Gfx7NumSrbmCounters   = 2;   //< SRBM
constexpr uint32 Gfx7NumSxCounters     = 4;   //< SX
constexpr uint32 Gfx7NumTaCounters     = 2;   //< TA
constexpr uint32 Gfx7NumTcaCounters    = 4;   //< TCA
constexpr uint32 Gfx7NumTccCounters    = 4;   //< TCC
constexpr uint32 Gfx7NumTcpCounters    = 4;   //< TCP
constexpr uint32 Gfx7NumTcsCounters    = 4;   //< TCS
constexpr uint32 Gfx7NumTdCounters     = 2;   //< TD
constexpr uint32 Gfx7NumVgtCounters    = 4;   //< VGT
constexpr uint32 Gfx7NumWdCounters     = 4;   //< WD

// Gfx8 Specific
constexpr uint32 Gfx8NumCbCounters     = 4;   //< CB
constexpr uint32 Gfx8NumCpcCounters    = 2;   //< CPC
constexpr uint32 Gfx8NumCpfCounters    = 2;   //< CPF
constexpr uint32 Gfx8NumCpgCounters    = 2;   //< CPG
constexpr uint32 Gfx8NumDbCounters     = 4;   //< DB
constexpr uint32 Gfx8NumGdsCounters    = 4;   //< GDS
constexpr uint32 Gfx8NumGrbmCounters   = 2;   //< GRBM
constexpr uint32 Gfx8NumGrbmseCounters = 1;   //< GRBMSE
constexpr uint32 Gfx8NumIaCounters     = 4;   //< IA
constexpr uint32 Gfx8NumMcCounters     = 4;   //< MC
constexpr uint32 Gfx8NumPaCounters     = 4;   //< PA
constexpr uint32 Gfx8NumRlcCounters    = 2;   //< RLC
constexpr uint32 Gfx8NumScCounters     = 8;   //< SC
constexpr uint32 Gfx8NumSdmaCounters   = 2;   //< SDMA
constexpr uint32 Gfx8NumSpiCounters    = 6;   //< SPI
constexpr uint32 Gfx8NumSqCounters     = 16;  //< SQ
constexpr uint32 Gfx8NumSrbmCounters   = 2;   //< SRBM
constexpr uint32 Gfx8NumSxCounters     = 4;   //< SX
constexpr uint32 Gfx8NumTaCounters     = 2;   //< TA
constexpr uint32 Gfx8NumTcaCounters    = 4;   //< TCA
constexpr uint32 Gfx8NumTccCounters    = 4;   //< TCC
constexpr uint32 Gfx8NumTcpCounters    = 4;   //< TCP
constexpr uint32 Gfx8NumTdCounters     = 2;   //< TD
constexpr uint32 Gfx8NumVgtCounters    = 4;   //< VGT
constexpr uint32 Gfx8NumWdCounters     = 4;   //< WD

// Performance monitoring state for disabling and resetting counters.
constexpr uint32 PerfmonDisableAndReset = 0;
// Performance monitoring state for starting counters.
constexpr uint32 PerfmonStartCounting   = 1;
// Performance monitoring state for stopping ("freezing") counters.
constexpr uint32 PerfmonStopCounting    = 2;

// Monitor period for the MC_SEQ_PERF_CNTL register.
constexpr uint32 McSeqMonitorPeriod = 0;
// Control value for MC_SEQ_PERF_CNTL::CNTL which clears the counter.
constexpr uint32 McSeqClearCounter  = 2;
// Control value for MC_SEQ_PERF_CNTL::CNTL which starts the counter.
constexpr uint32 McSeqStartCounter  = 0;

/// Maximum thread trace buffer size: 128MB per Engine.
constexpr size_t MaximumBufferSize = (128 * 1024 * 1024);
/// Default thread trace buffer size: 1MB per Engine.
constexpr size_t DefaultBufferSize = (1024 * 1024);
/// Thread trace buffer size and base address alignment shift: 12 bits (4KB)
constexpr uint32 BufferAlignShift = 12;
/// Thread trace buffer size and base address alignment
constexpr size_t BufferAlignment = (0x1 << BufferAlignShift);

/// Default thread trace random seed.
constexpr uint32 MaximumRandomSeed = 0xFFFF;

/// Default thread trace SIMD mask: enable all four SIMD's.
constexpr uint32 SimdMaskAll = 0xF;
/// Default thread trace Token mask: enable all 16 token types.
constexpr uint32 TokenMaskAll = 0xFFFF;
/// Default thread trace register mask: enable all 8 register types.
constexpr uint32 RegMaskAll = 0xFF;
/// Default thread trace CU mask: enable all CU's in a shader array.
constexpr uint32 ShCuMaskAll = 0xFFFF;

} // PerfExperiment
} // Gfx6
} // Pal
