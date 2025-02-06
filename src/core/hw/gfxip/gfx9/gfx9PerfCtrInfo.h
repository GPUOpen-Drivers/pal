/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "pal.h"
#include "palLiterals.h"
#include "core/perfExperiment.h"

namespace Pal
{

class  Device;
struct GpuChipProperties;

namespace Gfx9
{
// SDMA is a global block with unique registers for each instance; this requires special handling.
constexpr uint32 Gfx9MaxSdmaInstances = 2;
constexpr uint32 Gfx9MaxSdmaPerfModules = 2;

// In gfx11, the max number of wgp instances per shader array is 5.
// The max number of shader array per shader engine is 2.
// The max number of shader engines is 6.
constexpr uint32 Gfx11MaxWgps = 5 * 6 * 2;

// UMC is the block that interfaces between the Scalable Data Fabric (SDF) and the physical DRAM. Each UMC block
// has 1..n channels. Typically, there is one UMC channel per EA block, or one per SDP (Scalable Data Port). We
// abstract this as the "UMCCH" (UMC per CHannel), a global block with one instance per channel. The UMC is totally
// outside of the graphics core so it defines unique registers for each channel which requires special handling.
constexpr uint32 Gfx9MaxUmcchInstances = 32;
constexpr uint32 Gfx9MaxUmcchPerfModules = 5;

// The SQTT buffer size and alignment info can be queried out of our device. That means we need to define some shared
// constants for them instead of putting the constants into the perf experiment implementation.

// Set a maximum thread trace buffer and default size per SQG/SE.
constexpr gpusize SqttMaximumBufferSize = 2 * Util::OneGibibyte;
constexpr gpusize SqttDefaultBufferSize = Util::OneMebibyte;
// The thread trace base address and buffer size must be shifted by 12 bits, giving us an alignment requirement.
constexpr uint32  SqttBufferAlignShift = 12;
constexpr gpusize SqttBufferAlignment  = 0x1 << SqttBufferAlignShift;

// Constants defining special block configurations that we must share between InitPerfCtrInfo and the perf experiment.
constexpr uint32 Gfx9MaxSqgPerfmonModules = 16; // The SQG can have up to 16 custom perfmon modules.
constexpr uint32 Gfx10NumRmiSubInstances = 2;   // PAL considers each RMI isntance to consist of sub-instances.
constexpr uint32 Gfx10MaxDfPerfMon = 8;         // The DF has 8 global perf counters.
constexpr uint32 Gfx11MaxSqgPerfmonModules = 8; // The SQG can have up to 8 custom perfmon modules.
constexpr uint32 Gfx11MaxSqPerfmonModules = 16; // The SQ can have up to 16 custom perfmon modules.

// Contains information for perf counters for the Gfx9 layer.
struct PerfCounterInfo
{
    PerfExperimentDeviceFeatureFlags features;
    PerfCounterBlockInfo             block[static_cast<size_t>(GpuBlock::Count)];

    // SDMA and UMCCH register addresses are handled specially
    PerfCounterRegAddrPerModule      sdmaRegAddr[Gfx9MaxSdmaInstances][Gfx9MaxSdmaPerfModules];

    struct
    {
        uint32 perfMonCtlClk;     // Master control for this instance's counters (UMCCH#_PerfMonCtlClk).

        struct
        {
            uint32 perfMonCtl;   // Controls each UMCCH counter (UMCCH#_PerfMonCtl#).
            uint32 perfMonCtrLo; // The lower half of each counter (UMCCH#_PerfMonCtr#_Lo).
            uint32 perfMonCtrHi; // The upper half of each counter (UMCCH#_PerfMonCtr#_Hi).
        } perModule[Gfx9MaxUmcchPerfModules];
    } umcchRegAddr[Gfx9MaxUmcchInstances];
};

// Called during device init to populate the perf counter info.
extern void InitPerfCtrInfo(const Pal::Device& device, GpuChipProperties* pProps);

} // Gfx9
} // Pal
