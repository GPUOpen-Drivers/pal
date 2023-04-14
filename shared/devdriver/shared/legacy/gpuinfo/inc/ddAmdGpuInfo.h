/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <ddPlatform.h>
#include <util/vector.h>

namespace DevDriver
{

/// Enumerates all of the types of local video memory which could be associated with a GPU
enum struct LocalMemoryType : uint32
{
    Unknown = 0,

    Ddr2,
    Ddr3,
    Ddr4,
    Gddr5,
    Gddr6,
    Hbm,
    Hbm2,
    Hbm3,
    Lpddr4,
    Lpddr5,
    Ddr5,

    Count
};

// Get memory ops per clock for a given LocalMemoryType
inline uint32 MemoryOpsPerClock(LocalMemoryType type)
{
    switch (type)
    {
        case LocalMemoryType::Unknown: return 0;
        case LocalMemoryType::Count:   return 0;

        case LocalMemoryType::Ddr2:    return 2;
        case LocalMemoryType::Ddr3:    return 2;
        case LocalMemoryType::Ddr4:    return 2;
        case LocalMemoryType::Gddr5:   return 4;
        case LocalMemoryType::Gddr6:   return 16;
        case LocalMemoryType::Hbm:     return 2;
        case LocalMemoryType::Hbm2:    return 2;
        case LocalMemoryType::Hbm3:    return 2;
        case LocalMemoryType::Lpddr4:  return 2;
        case LocalMemoryType::Lpddr5:  return 4;
        case LocalMemoryType::Ddr5:    return 4;
    }

    return 0;
};

// Get a printable string for a memory type.
// LocalMemoryType::Unknown or invalid enums return an empty string
inline const char* ToString(LocalMemoryType type)
{
    switch (type)
    {
        case LocalMemoryType::Unknown: return "";
        case LocalMemoryType::Count:   return "";

        case LocalMemoryType::Ddr2:    return "Ddr2";
        case LocalMemoryType::Ddr3:    return "Ddr3";
        case LocalMemoryType::Ddr4:    return "Ddr4";
        case LocalMemoryType::Gddr5:   return "Gddr5";
        case LocalMemoryType::Gddr6:   return "Gddr6";
        case LocalMemoryType::Hbm:     return "Hbm";
        case LocalMemoryType::Hbm2:    return "Hbm2";
        case LocalMemoryType::Hbm3:    return "Hbm3";
        case LocalMemoryType::Lpddr4:  return "Lpddr4";
        case LocalMemoryType::Lpddr5:  return "Lpddr5";
        case LocalMemoryType::Ddr5:    return "Ddr5";
    }

    return nullptr;
};

/// An amalgamation of information about a single GPU
/// This GPU will have identified as AMD when initially queried
/// There is an InfoService node in ListenerCore that mirrors this struct into Json
struct AmdGpuInfo
{
    char name[128];             // Name of the AMD GPU
    char driverInstallDir[256]; // Path to the driver installation directory

    struct PciLocation
    {
        uint32 bus;
        uint32 device;
        uint32 function;
    };
    // This can be used to uniquely identify a GPU in a system
    PciLocation pci;

    struct AsicInfo
    {
        uint32 gpuIndex;       // Index of gpu as enumerated
        uint64 gpuCounterFreq; // ???
        uint32 numCus;         // The number of compute units.

        struct Ids
        {
            uint32 gfxEngineId; // Coarse-grain GFX engine ID (R800, SI, etc.)
            uint32 family;      // Hardware family ID. Driver-defined identifier for a particular family of devices.
            uint32 eRevId;      // Hardware revision ID. Driver-defined identifier for a particular device and
                                // sub-revision in the hardware family designated by the familyId.
                                // See AMDGPU_TAHITI_RANGE, AMDGPU_FIJI_RANGE, etc. as defined in amdgpu_asic.h.
            uint32 revisionId;  // PCI revision ID. 8-bit value as reported in the device structure in the PCI config
                                // space.  Identifies a revision of a specific PCI device ID.
            uint32 deviceId;    // PCI device ID. 16-bit value device ID as reported in the PCI config space.
        };
        Ids ids;
    };
    AsicInfo asic;

    struct ClocksFreqRange
    {
        uint64 min; // The minimum clock frequency for a component in Hz
        uint64 max; // The maxmimum clock frequency for a component in Hz
    };
    ClocksFreqRange engineClocks;

    static constexpr uint64 kMaxExcludedVaRanges = 0x20;
    struct MemoryInfo
    {
        LocalMemoryType type;
        uint32          memOpsPerClock;
        uint32          busBitWidth;

        ClocksFreqRange clocksHz;

        struct HeapInfo
        {
            uint64 physAddr;
            uint64 size;
        };
        HeapInfo localHeap;
        HeapInfo invisibleHeap;

        uint64 hbccSize; // Size of High Bandwidth Cache Controller (HBCC) memory segment.
                         // HBCC memory segment comprises of system and local video memory, where HW/KMD
                         // will ensure high performance by migrating pages accessed by hardware to local.
                         // This HBCC memory segment is only available on certain platforms.

        struct VaRange
        {
            uint64 base;
            uint64 size;
        };
        VaRange excludedVaRanges[kMaxExcludedVaRanges];

        // Compute the memory bandwidth in bytes for a partially-filled out adapter
        // This is called as part of QueryGpuInfo.
        uint64 BandwidthInBytes() const
        {
            // Bit-Bandwidth is computed as the multiple of several properties:
            return busBitWidth      // Bits per MemOp
                   * memOpsPerClock // MemOps per MemClock
                   * clocksHz.max   // MemClocks per second
                   / 8;             // Convert Bits to Bytes
        }
    };
    MemoryInfo memory;

    struct BigSwVersion
    {
        uint32 Major;
        uint32 Minor;
        uint32 Misc;
    };
    BigSwVersion bigSwVersion;

    struct LibDrmVersion
    {
        uint32_t Major; // drm major version
        uint32_t Minor; // drm minor version
    };
    LibDrmVersion drmVersion;

    AmdGpuInfo() { memset(this, 0, sizeof(*this)); }
};

// Query information about all AMD adapters in the system
Result QueryGpuInfo(const AllocCb& allocCb, Vector<AmdGpuInfo>* pGpus);

// Counts the number of 1 bits
inline uint32 CountSetBits(uint32 value)
{
    uint32 numberOfOnes = 0;
    for (uint8 digit = 0; digit < 32; digit++)
    {
        if ((value & (1 << digit)) != 0)
        {
            ++numberOfOnes;
        }
    }

    return numberOfOnes;
}

} // namespace DevDriver
