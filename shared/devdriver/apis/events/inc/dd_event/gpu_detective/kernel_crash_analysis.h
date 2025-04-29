/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "../common.h"
#include "ddPlatform.h"

#include <stdint.h>
#include <string.h>

using namespace DevDriver;

namespace KernelCrashAnalysisEvents
{
#pragma pack(push, 1)

constexpr uint32_t VersionMajor = 0;
constexpr uint32_t VersionMinor = 1;

constexpr uint32_t ProviderId = 0xE43C9C8E;

/// Unique id represeting each event. Each variable name of the enum value corresponds to the
/// struct with the same name.
enum class EventId : uint8_t
{
    PageFault        = DDCommonEventId::FirstEventIdForIndividualProvider,
    ShaderWaves      = DDCommonEventId::FirstEventIdForIndividualProvider + 1,
    SeInfo           = DDCommonEventId::FirstEventIdForIndividualProvider + 2,
    MmrRegisters     = DDCommonEventId::FirstEventIdForIndividualProvider + 3,
    WaveRegisters    = DDCommonEventId::FirstEventIdForIndividualProvider + 4,
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
        uint32_t copySize = 0;

        memcpy(buffer + copySize, &vmId, sizeof(vmId));
        copySize += sizeof(vmId);

        memcpy(buffer + copySize, &processId, sizeof(processId));
        copySize += sizeof(processId);

        memcpy(buffer + copySize, &pageFaultAddress, sizeof(pageFaultAddress));
        copySize += sizeof(pageFaultAddress);

        memcpy(buffer + copySize, &processNameLength, sizeof(processNameLength));
        copySize += sizeof(processNameLength);

        memcpy(buffer + copySize, processName, processNameLength);
        copySize += processNameLength;

        return copySize;
    }
};

// offset and data of a single memory mapped register
struct MmrRegisterInfo
{
    uint32_t offset;
    uint32_t data;
};

// Note: Must exactly match KmdMmrRegistersEventData in KmdEventDefs.h
struct MmrRegistersData
{
    uint32_t version;

    // GPU identifier for these register events
    uint32_t gpuId;

    // number of MMrRegisterInfo structures which follow
    uint32_t numRegisters;

    // array of MMrRegisterInfo
    // actual array length is `numRegisters`
    MmrRegisterInfo registerInfos[1];

    static size_t CalculateStructureSize(uint32_t numRegisterInfoForCalculation)
    {
        numRegisterInfoForCalculation = Platform::Max(1U, numRegisterInfoForCalculation);
        return sizeof(MmrRegistersData) +
               sizeof(MmrRegisterInfo) * (numRegisterInfoForCalculation - 1);
    }

    static size_t CalculateBufferSize(uint32_t numRegisterInfoForCalculation)
    {
        return sizeof(MmrRegistersData) +
               sizeof(MmrRegisterInfo) * (numRegisterInfoForCalculation - 1);
    }

    static uint32_t GetNumMmrRegistersFromBuffer(const uint8_t *pBuffer)
    {
        pBuffer += offsetof(MmrRegistersData, numRegisters);
        return *reinterpret_cast<const uint32_t*>(pBuffer);
    }

    size_t FromBuffer(const uint8_t* pBuffer)
    {
        uint32_t numRegistersInBuffer = GetNumMmrRegistersFromBuffer(pBuffer);
        size_t   copySize             = CalculateBufferSize(numRegistersInBuffer);
        memcpy(this, pBuffer, copySize);
        return copySize;
    }

    size_t ToBuffer(uint8_t* pBuffer)
    {
        size_t copySize = CalculateBufferSize(numRegisters);
        memcpy(pBuffer, this, copySize);
        return copySize;
    }
};

// Graphics Register Bus Manager status registers
struct GrbmStatusSeRegs
{
    uint32_t    version;
    uint32_t    grbmStatusSe0;
    uint32_t    grbmStatusSe1;
    uint32_t    grbmStatusSe2;
    uint32_t    grbmStatusSe3;
    // SE4 and SE5 are NV31 specific, 2x does not have this
    uint32_t    grbmStatusSe4;
    uint32_t    grbmStatusSe5;
};

// Note: Must exactly match KmdWaveInfo in KmdEventDefs.h
struct WaveInfo
{
    uint32_t    version;

    union
    {
        struct
        {
            unsigned int    waveId  : 5;
            unsigned int            : 3;
            unsigned int    simdId  : 2;
            unsigned int    wgpId   : 4;
            unsigned int            : 2;
            unsigned int    saId    : 1;
            unsigned int            : 1;
            unsigned int    seId    : 4;
            unsigned int    reserved: 10;
        };
        uint32_t        shaderId;
    };
};

// NOTE: HangType must match the Hangtype enum in kmdEventDefs.h
enum HangType : uint32_t
{
    pageFault     = 0,
    nonPageFault  = 1,
    Unknown       = 2,
};

// Note: Must exactly match KmdShaderWavesEventData in kmdEventDefs.h
struct ShaderWaves
{
    // structure version
    uint32_t         version;

    // GPU identifier for these register events
    uint32_t         gpuId;

    HangType         typeOfHang;
    GrbmStatusSeRegs grbmStatusSeRegs;

    uint32_t         numberOfHungWaves;
    uint32_t         numberOfActiveWaves;

    // aray of hung waves followed by active waves
    // KmdWaveInfo * [numberOfHungWaves]
    // KmdWaveInfo * [numberOfActiveWaves]
    WaveInfo         waveInfos[1];

    static size_t CalculateStructureSize(uint32_t numWaveInfoForCalculation)
    {
        numWaveInfoForCalculation = Platform::Max(1U, numWaveInfoForCalculation);
        return sizeof(ShaderWaves) +
               sizeof(WaveInfo) * (numWaveInfoForCalculation - 1);
    }

    static size_t CalculateBufferSize(uint32_t numWaveInfoForCalculation)
    {
        return sizeof(ShaderWaves) +
               sizeof(WaveInfo) * (numWaveInfoForCalculation - 1);
    }

    static uint32_t GetTotalNumWavesFromBuffer(const uint8_t *pBuffer)
    {
        uint32_t actualNumberOfHungWaves;
        uint32_t actualNumberOfActiveWaves;

        pBuffer += offsetof(ShaderWaves, numberOfHungWaves);
        actualNumberOfHungWaves   = *reinterpret_cast<const uint32_t*>(pBuffer);

        pBuffer += sizeof(numberOfHungWaves);
        actualNumberOfActiveWaves = *reinterpret_cast<const uint32_t*>(pBuffer);

        return actualNumberOfHungWaves + actualNumberOfActiveWaves;
    }

    size_t FromBuffer(const uint8_t* pBuffer)
    {
        uint32_t numWavesInBuffer = GetTotalNumWavesFromBuffer(pBuffer);
        size_t   copySize         = CalculateBufferSize(numWavesInBuffer);
        memcpy(this, pBuffer, copySize);
        return copySize;
    }

    size_t ToBuffer(uint8_t* pBuffer)
    {
        size_t copySize = CalculateBufferSize(numberOfHungWaves + numberOfActiveWaves);
        memcpy(pBuffer, this, copySize);
        return copySize;
    }
};

struct SeRegsInfo
{
    uint32_t version;
    uint32_t spiDebugBusy;
    uint32_t sqDebugStsGlobal;
    uint32_t sqDebugStsGlobal2;
};

struct SeInfo
{
    // structure version
    uint32_t   version;

    // GPU identifier for these register events
    uint32_t   gpuId;

    // number of SeRegsInfo structures in seRegsInfos array
    uint32_t   numSe;
    SeRegsInfo seRegsInfos[1];

    static size_t CalculateStructureSize(uint32_t numSeRegsInfoForCalculation)
    {
        numSeRegsInfoForCalculation = Platform::Max(1U, numSeRegsInfoForCalculation);
        return sizeof(SeInfo) +
               sizeof(SeRegsInfo) * (numSeRegsInfoForCalculation - 1);
    }

    static size_t CalculateBufferSize(uint32_t numSeRegsInfoForCalculation)
    {
        return sizeof(SeInfo) +
               sizeof(SeRegsInfo) * (numSeRegsInfoForCalculation - 1);
    }

    static uint32_t GetTotalSeRegsInfosFromBuffer(const uint8_t *pBuffer)
    {
        pBuffer += offsetof(SeInfo, numSe);
        return *reinterpret_cast<const uint32_t*>(pBuffer);
    }

    size_t FromBuffer(const uint8_t* pBuffer)
    {
        uint32_t numSeInBuffer = GetTotalSeRegsInfosFromBuffer(pBuffer);
        size_t   copySize      = CalculateBufferSize(numSeInBuffer);
        memcpy(this, pBuffer, copySize);
        return copySize;
    }

    size_t ToBuffer(uint8_t* pBuffer)
    {
        size_t copySize = CalculateBufferSize(numSe);
        memcpy(pBuffer, this, copySize);
        return copySize;
   }
};

// offset and data of a single shader wave register
struct WaveRegisterInfo
{
    uint32_t offset;
    uint32_t data;
};

// Note: Must exactly match KmdWaveRegistersEventData in KmdEventDefs.h
struct WaveRegistersData
{
    uint32_t version;

    uint32_t shaderId;

    // number of WaveRegisterInfo structures which follow
    uint32_t numRegisters;

    // array of WaveRegisterInfo
    // actual array length is `numRegisters`
    WaveRegisterInfo registerInfos[1];

    static size_t CalculateStructureSize(uint32_t numRegisterInfoForCalculation)
    {
        numRegisterInfoForCalculation = Platform::Max(1U, numRegisterInfoForCalculation);
        return sizeof(WaveRegistersData) +
               sizeof(WaveRegisterInfo) * (numRegisterInfoForCalculation - 1);
    }

    static size_t CalculateBufferSize(uint32_t numRegisterInfoForCalculation)
    {
        return sizeof(WaveRegistersData) +
               sizeof(WaveRegisterInfo) * (numRegisterInfoForCalculation - 1);
    }

    static uint32_t GetNumWaveRegistersFromBuffer(const uint8_t *pBuffer)
    {
        pBuffer += offsetof(WaveRegistersData, numRegisters);
        return *reinterpret_cast<const uint32_t*>(pBuffer);
    }

    size_t FromBuffer(const uint8_t* pBuffer)
    {
        uint32_t numRegistersInBuffer = GetNumWaveRegistersFromBuffer(pBuffer);
        size_t   copySize             = CalculateBufferSize(numRegistersInBuffer);
        memcpy(this, pBuffer, copySize);
        return copySize;
    }

    size_t ToBuffer(uint8_t* pBuffer)
    {
        size_t copySize = CalculateBufferSize(numRegisters);
        memcpy(pBuffer, this, copySize);
        return copySize;
    }
};

#pragma pack(pop)
} // namespace KernelCrashAnalysisEvents
