/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <dd_event.h>

#include <stdint.h>
#include <string.h>

namespace KernelCrashAnalysisEvents
{

constexpr uint32_t VersionMajor = 0;
constexpr uint32_t VersionMinor = 1;

constexpr uint32_t ProviderId = 0xE43C9C8E;

/// Unique id represeting each event. Each variable name of the enum value corresponds to the
/// struct with the same name.
enum class EventId : uint8_t
{
    PageFault = DDCommonEventId::FirstEventIdForIndividualProvider,
};

/// Data generated from kernel driver when a VM Page Fault happens.
struct PageFault
{
    uint32_t vmId;

    /// Process ID (PID) of the offending process.
    uint32_t processId;

    /// Page fault virtual address.
    uint64_t pageFaultAddress;

    /// Length of the process name.
    uint16_t processNameLength;

    /// The name of the offending process, encoded in UTF-8.
    uint8_t processName[64];

    void FromBuffer(const uint8_t* buffer)
    {
        memcpy(&vmId, buffer, sizeof(vmId));
        buffer += sizeof(vmId);

        memcpy(&processId, buffer, sizeof(processId));
        buffer += sizeof(processId);

        memcpy(&pageFaultAddress, buffer, sizeof(pageFaultAddress));
        buffer += sizeof(pageFaultAddress);

        memcpy(&processNameLength, buffer, sizeof(processNameLength));
        buffer += sizeof(processNameLength);

        memcpy(processName, buffer, processNameLength);
    }

    /// Fill the pre-allocated `buffer` with the data of this struct. The size of
    /// the buffer has to be at least `sizeof(PageFault)` big.
    ///
    /// Return the actual amount of bytes copied into `buffer`.
    uint32_t ToBuffer(uint8_t* buffer) const
    {
        memcpy(buffer, &vmId, sizeof(vmId));
        buffer += sizeof(vmId);

        memcpy(buffer, &processId, sizeof(processId));
        buffer += sizeof(processId);

        memcpy(buffer, &pageFaultAddress, sizeof(pageFaultAddress));
        buffer += sizeof(pageFaultAddress);

        memcpy(buffer, &processNameLength, sizeof(processNameLength));
        buffer += sizeof(processNameLength);

        memcpy(buffer, processName, processNameLength);

        return sizeof(vmId) + sizeof(processId) + sizeof(pageFaultAddress) + sizeof(processNameLength) + processNameLength;
    }
};

} // namespace KernelCrashAnalysisEvents
