/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
void ClockCalibrationTraceSource::WriteClockCalibrationChunks()
{
    Result result = Result::Success;
    const uint32 deviceCount = m_pPlatform->GetDeviceCount();

    for (uint32 i = 0; ((i < deviceCount) && (result == Result::Success)); i++)
    {
        TraceChunkClockCalibration chunk = { };
        Device* pDevice = m_pPlatform->GetDevice(i);

        DeviceProperties props = { };
        result = pDevice->GetProperties(&props);

        if (result == Result::Success)
        {
            CalibratedTimestamps timestamps = { };
            result = pDevice->GetCalibratedTimestamps(&timestamps);

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

        if (result == Result::Success)
        {
            TraceChunkInfo info = { };
            memcpy(info.id, ClockCalibTextId, GpuUtil::TextIdentifierSize);
            info.pHeader           = nullptr;
            info.headerSize        = 0;
            info.version           = GetVersion();
            info.pData             = &chunk;
            info.dataSize          = sizeof(TraceChunkClockCalibration);
            info.enableCompression = false;

            m_pPlatform->GetTraceSession()->WriteDataChunk(this, info);
        }
    }
}

// =====================================================================================================================
void ClockCalibrationTraceSource::OnTraceFinished()
{
    WriteClockCalibrationChunks();
}

} // namespace GpuUtil

#endif
