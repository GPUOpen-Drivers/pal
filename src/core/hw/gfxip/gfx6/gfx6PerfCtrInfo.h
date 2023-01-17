/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

namespace Pal
{

class  Device;
struct GpuChipProperties;

namespace Gfx6
{

// The SQTT buffer size and alignment info can be queried out of our device. That means we need to define some shared
// constants for them instead of putting the constants into the perf experiment implementation.

// Set a maximum thread trace buffer and default size per SQG/SE.
constexpr gpusize SqttMaximumBufferSize = 2 * Util::OneGibibyte;
constexpr gpusize SqttDefaultBufferSize = Util::OneMebibyte;
// The thread trace base address and buffer size must be shifted by 12 bits, giving us an alignment requirement.
constexpr uint32  SqttBufferAlignShift = 12;
constexpr gpusize SqttBufferAlignment  = 0x1 << SqttBufferAlignShift;

// Constants defining special block configurations that we must share between InitPerfCtrInfo and the perf experiment.
// PAL's abstract MaxShaderEngines is very large (32) so in the interest of not wasting memory we define a new one.
constexpr uint32 Gfx6MaxShaderEngines     = 4; // We can't have more than 4 SEs on gfx9+.
constexpr uint32 Gfx6MaxSqgPerfmonModules = 8; // All gfx6-8 SQGs only have 8 out of 16 possible counter modules.
constexpr uint32 MaxMcdTiles              = 8; // This is a guess based on our GPU properties code.
constexpr uint32 NumMcChannels            = 2; // Each MC has two channels (0 and 1).
constexpr uint32 NumMcCountersPerCh       = 4; // Each MC has four counters per channel (A, B, C, and D).

// Called during device init to populate the perf counter info.
extern void InitPerfCtrInfo(const Pal::Device& device, GpuChipProperties* pProps);

} // Gfx6
} // Pal
