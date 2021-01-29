/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

namespace Pal
{

class  Device;
struct GpuChipProperties;

namespace Gfx9
{

// The SQTT buffer size and alignment info can be queried out of our device. That means we need to define some shared
// constants for them instead of putting the constants into the perf experiment implementation.

// Set a maximum thread trace buffer size (2GB) and default size (1MB) per SQG/SE.
constexpr gpusize SqttMaximumBufferSize = 0x80000000;
constexpr gpusize SqttDefaultBufferSize = 1024 * 1024;
// The thread trace base address and buffer size must be shifted by 12 bits, giving us an alignment requirement.
constexpr uint32  SqttBufferAlignShift = 12;
constexpr gpusize SqttBufferAlignment  = 0x1 << SqttBufferAlignShift;

// Constants defining special block configurations that we must share between InitPerfCtrInfo and the perf experiment.
constexpr uint32 Gfx9MaxSqgPerfmonModules = 16; // The SQG can have up to 16 custom perfmon modules.

constexpr uint32 Gfx10NumRmiSubInstances = 2; // PAL considers each RMI isntance to consist of sub-instances.

constexpr uint32 Gfx10MaxDfPerfMon = 8; // The DF has 8 global perf counters.

// Called during device init to populate the perf counter info.
extern void InitPerfCtrInfo(const Pal::Device& device, GpuChipProperties* pProps);

} // Gfx9
} // Pal
