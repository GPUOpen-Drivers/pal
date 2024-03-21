/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_BUILD_RDF

#include "core/platform.h"
#include "core/device.h"
#include "clockCalibTraceSource.h"

using namespace Pal;

namespace GpuUtil
{

// =====================================================================================================================
// Writes a clock calibration chunk for each device to the trace session.
void ClockCalibrationTraceSource::OnTraceFinished()
{
    Result result = Result::Success;
    const uint32 deviceCount = m_pPlatform->GetDeviceCount();

    for (uint32 i = 0; ((i < deviceCount) && (result == Result::Success)); i++)
    {
        TraceChunkClockCalibration chunk = { };

        const Device* pDevice = m_pPlatform->GetDevice(i);

        DeviceProperties props = { };
        result = pDevice->GetProperties(&props);

        if (result == Result::Success)
        {
            CalibratedTimestamps timestamps = { };
            result = pDevice->GetCalibratedTimestamps(&timestamps);

            if (result == Result::Success)
            {
                chunk.pciId        = m_pPlatform->GetPciId(props.gpuIndex).u32All;
                chunk.gpuTimestamp = timestamps.gpuTimestamp;

                if (props.osProperties.timeDomains.supportQueryPerformanceCounter != 0)
                {
                    chunk.cpuTimestamp = timestamps.cpuQueryPerfCounterTimestamp;
                }
                else if (props.osProperties.timeDomains.supportClockMonotonic != 0)
                {
                    chunk.cpuTimestamp = timestamps.cpuClockMonotonicTimestamp;
                }
                else if (props.osProperties.timeDomains.supportClockMonotonicRaw != 0)
                {
                    chunk.cpuTimestamp = timestamps.cpuClockMonotonicRawTimestamp;
                }
                else
                {
                    result = Result::ErrorUnknown;
                }
            }
        }

        if (result == Result::Success)
        {
            TraceChunkInfo info    = {
                .version           = ClockCalibChunkVersion,
                .pHeader           = nullptr,
                .headerSize        = 0,
                .pData             = &chunk,
                .dataSize          = sizeof(TraceChunkClockCalibration),
                .enableCompression = false
            };
            memcpy(info.id, ClockCalibTextId, TextIdentifierSize);

            result = m_pPlatform->GetTraceSession()->WriteDataChunk(this, info);
        }

        PAL_ASSERT(result == Result::Success);
    }
}

} // namespace GpuUtil

#endif
