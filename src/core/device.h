/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/image.h"
#include "core/internalMemMgr.h"
#include "core/privateScreen.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/ossip/ossDevice.h"
#include "core/addrMgr/addrMgr.h"
#include "palDevice.h"
#include "palInlineFuncs.h"
#include "palIntrusiveList.h"
#include "palMutex.h"
#include "palPipeline.h"
#include "palTextWriter.h"

#include "core/hw/amdgpu_asic.h"
#include "palCmdAllocator.h"

typedef void* ADDR_HANDLE;

namespace Util { enum class ValueType : uint32; }
namespace Util { namespace MetroHash { struct Hash; } }

namespace Pal
{

class  CmdAllocator;
class  CmdBuffer;
class  Fence;
class  GpuMemory;
class  OssDevice;
class  Platform;
class  SettingsLoader;
class  Queue;
struct ApplicationProfile;
struct DeviceFinalizeInfo;
struct CmdBufferInternalCreateInfo;
struct GpuMemoryInternalCreateInfo;
struct PalSettings;

// Indicates to the address lib not to use tile index.
constexpr int32 TileIndexUnused = -1;
// Indicates to the address lib to use linear-general mode
constexpr int32 TileIndexLinearGeneral = -2;
// Indicates to the address lib that macro mode index is not necessary.
constexpr int32 TileIndexNoMacroIndex = -3;

// Minimum HW requirement is 32 bits for the flat address instruction with 32 bit ISA.
constexpr uint32 VaRangeLimitTo32bits = 32u;

// PAL requires the range of the device virtual address space to be atleast 36 bits.
constexpr uint32 MinVaRangeNumBits = 36u;

// PAL minimum fragment size for local memory allocations
constexpr gpusize PageSize = 0x1000u;

// Internal representation of the IDevice::m_pfnTable structure.
struct DeviceInterfacePfnTable
{
    CreateBufferViewSrdsFunc pfnCreateTypedBufViewSrds;
    CreateBufferViewSrdsFunc pfnCreateUntypedBufViewSrds;
    CreateImageViewSrdsFunc  pfnCreateImageViewSrds;
    CreateFmaskViewSrdsFunc  pfnCreateFmaskViewSrds;
    CreateSamplerSrdsFunc    pfnCreateSamplerSrds;
};

// Maximum number of excluded virtual address ranges.
constexpr size_t MaxExcludedVaRanges = 32;

// Struct describing GDS parition size and offset (in bytes) for a single engine
struct GdsInfo
{
    uint32 size;
    uint32 offset;
};

// Enumerates the internal virtual address space partitions used for supporting multiple VA ranges.
enum class VaPartition : uint32
{
    Default,                // SEE: VaRange::Default
    DefaultBackup,          // Some platforms don't have enough virtual address space to use 4 GB sections for
                            // DescriptorTable and ShadowDescriptorTable, so we split the default VaRange into
                            // two pieces so that the other VA ranges will fit better.
    DescriptorTable,        // SEE: VaRange::DescriptorTable
    ShadowDescriptorTable,  // SEE: VaRange::ShadowDescriptorTable
    Svm,                    // SEE: VaRange::Svm
    Prt,                    // Some platforms require a specific VA range in order to properly setup HW for PRTs.
                            // To simplify client support, no corresponding VA range exists and this partition
                            // will be chosen instead of the default when required.
    Count,
};

// Tracks device-wide information for the compute hardware scheduler (HWS) feature. When running in HWS mode,
// compute queue submissions made via a WDDM interface are executed under-the-covers by KMD/KFD with an HSA-style
// user mode queues. This feature automatically enables pre-emption of compute work by context save restore. See
// Windows::Device::InitHwScheduler() for more info.
struct HwsInfo
{
    uint32 userQueueSize;   //User queue size in byte
    uint32 gdsSaveAreaSize; //GDS save area size in byte
    uint32 engineMask;      //Indicates which compute engine supports HWS
};

// Bundles the IP levels for all kinds of HW IP.
struct HwIpLevels
{
    GfxIpLevel gfx;
    OssIpLevel oss;
    VceIpLevel vce;
    UvdIpLevel uvd;
    VcnIpLevel vcn;
};

// Bundles the sizes of the HW IP specific device classes.
struct HwIpDeviceSizes
{
    size_t gfx;
    size_t oss;
};

// Indicates a allocated virtual address range.
struct VaRangeInfo
{
    gpusize baseVirtAddr;    // Base address of the virtual address range
    gpusize size;            // Size of the virtual address range (in bytes)
};

// Everything PAL & its clients would ever need to know about a GPU's memory subsystem.
struct GpuMemoryProperties
{
    LocalMemoryType localMemoryType;
    uint32          memOpsPerClock;
    uint32          vramBusBitWidth;
    // For APU's, memory bandwidth is shared between the GPU and the CPU. This is the factor which total memory
    // bandwidth is reduced by to get the true GPU memory bandwidth.
    uint32          apuBandwidthFactor;

    gpusize localHeapSize;
    gpusize invisibleHeapSize;
    gpusize nonLocalHeapSize;
    gpusize hbccSizeInBytes;    // Size of High Bandwidth Cache Controller (HBCC) memory segment.
                                // HBCC memory segment comprises of system and local video memory, where HW/KMD
                                // will ensure high performance by migrating pages accessed by hardware to local.
                                // This HBCC memory segment is only available on certain platforms.
    gpusize busAddressableMemSize;

    gpusize vaStart;        // Starting address of the GPU's virtual address space
    gpusize vaEnd;          // Ending address of the GPU's virtual address space
    // Some hardware has dynamically-sizeable virtual address ranges. For such GPU's, this is the initial ending
    // address of the virtual address space.
    gpusize vaInitialEnd;
    // NOTE: To prevent consuming all of a GPU's available global or submission memory reference entries for the
    // GPU's page table blocks, we limit the "usable" portion of the GPU address space to put an effective upper
    // bound on the number of page table blocks a given Device will require.
    gpusize vaUsableEnd;    // Ending address of the "usable" portion of the GPU's virtual address space
    gpusize vaStartPrt;     // Subset of the VA range for Partially Resident Textures (PRTs) when required by KMD

    gpusize pdeSize;            // Size (in bytes) of a page directory entry (PDE)
    gpusize pteSize;            // Size (in bytes) of a page table entry (PTE)
    gpusize spaceMappedPerPde;  // Amount of VA space mapped in a single PDE
    uint32  numPtbsPerGroup;    // Number of 4K page table blocks (PTBs) grouped into a single allocation
    gpusize fragmentSize;       // Size in bytes of a video memory fragment.
    uint32  uibVersion;         // Version number of the Unmap-info Buffer format

    gpusize realMemAllocGranularity;    // The addrs and sizes of "real" GPU memory objects must be aligned to this.
    gpusize virtualMemAllocGranularity; // The addrs and sizes of virtual GPU memory objects must be aligned to this.
    gpusize virtualMemPageSize;         // Size in bytes of a virtual GPU memory page.

    gpusize privateApertureBase; // Private memory base address for generic address space implementation.
    gpusize sharedApertureBase;  // Shared memory base address for generic address space implementation.

    gpusize gpuvmOffsetForSvm;   // Offset provided by KMD for 64bit Win10 SVM capable HW to access gpu va.
    union
    {
        struct
        {
            uint32 intraSubmitMigration       :  1; // Indicates support for GPU memory migration during submissions.
            uint32 virtualRemappingSupport    :  1; // Indicates support for remapping virtual memory
            uint32 pinningSupport             :  1; // Indicates support for accessing pinned system memory
            uint32 supportPerSubmitMemRefs    :  1; // Indicates whether specifying memory references at Submit time
                                                    // is supported versus via the IDevice or IQueue AddReferences()
            uint32 ptbInNonLocal              :  1; // Indicates hardware support for PTB's being in non-local memory
            uint32 resizeableVaRange          :  1; // Indicates that the hardware's VA range is resizable
            uint32 adjustVmRangeEscapeSupport :  1; // Indicates that adjusting the VA range requires an Escape
            uint32 multipleVaRangeSupport     :  1; // Indicates the device has enough VA range to support multiple
                                                    // partitions of that range.
            uint32 defaultVaRangeSplit        :  1; // Indicates that the device is somewhat limited in VA range, and
                                                    // needs to split up the 'Default' VA range to accomodate this.
            uint32 globalGpuVaSupport         :  1; // Indicates support for GPU virtual addresses that are visible
                                                    // to all devices.
            uint32 svmSupport                 :  1; // Indicates support for SVM
            uint32 shadowDescVaSupport        :  1; // Indicates support to for shadow desc VA range.
            uint32 iommuv2Support             :  1; // Indicates support for IOMMUv2
            uint32 autoPrioritySupport        :  1; // Indiciates that the platform supports automatic allocation
                                                    // priority management.
            uint32 placeholder0               :  1; // Placeholder.
            uint32 reserved                   : 17;
        };
        uint32 u32All;
    } flags;

    // Some areas of the GPU's virtual address range are reserved for system or future use. This list contains all
    // ranges of invalid GPU virtual addresses which cannot be assigned for use by a user-mode driver.
    size_t numExcludedVaRanges;
    VaRangeInfo excludedRange[MaxExcludedVaRanges];

    // These are the usable areas of the GPU's virtual address range. The whole range is partitioned into several
    // several smaller ranges designated for specific use cases (such as descriptor tables in Vulkan or D3D12).
    VaRangeInfo vaRange[static_cast<uint32>(VaPartition::Count)];

    struct
    {
        // Total size in bytes calculated with LargePageSize value from KMD. 4k*2^(LargePageSize)
        // LargePageSize 0 = 4K; 4 = 64K ; 9 = 2MB etc.
        gpusize largePageSizeInBytes;
        // Minimum size in bytes for which GPUVA\Size alignment needed. Calculated with MinSurfaceSizeForAlignment
        // from KMD = 4k*2^(MinSurfaceSizeForAlignment)
        gpusize minSurfaceSizeForAlignmentInBytes;
        // the starting GPUVA of the allocations must be aligned to the LargePageSize
        bool gpuVaAlignmentNeeded;
        // the size of the allocations must be aligned to the LargePageSize.
        // When set, HWVMINFO.fragment == LARGE_PAGE_FEATURES_FLAGS.LargePageSize
        bool sizeAlignmentNeeded;
    } largePageSupport;
};

// Everything PAL & its clients would ever need to know about a GPU's available engines.
struct GpuEngineProperties
{
    // Maximum number of user and internal memory references allowed with a single Queue submission.
    uint32 maxUserMemRefsPerSubmission;
    uint32 maxInternalRefsPerSubmission;
    uint32 cpUcodeVersion;               // Version number of the CP microcode.

    struct
    {
        uint32   numAvailable;
        uint32   startAlign;                    // Alignment requirement for the starting address of command buffers.
        uint32   sizeAlignInDwords;             // Alignment requirement for the size of command buffers.
        uint32   maxControlFlowNestingDepth;    // Maximum depth of nested control-flow operations in command buffers.
        uint32   availableCeRamSize;            // Size of CE RAM available on this queue for use by clients.
        uint32   reservedCeRamSize;             // Size of CE RAM reserved on this queue for internal PAL use.
        Extent3d minTiledImageCopyAlignment;    // Minimum alignments for X/Y/Z/Width/Height/Depth for
                                                // ICmdBuffer::CmdCopyImage() between optimally tiled images.
        Extent3d minTiledImageMemCopyAlignment; // Minimum alignments for X/Y/Z/Width/Height/Depth for
                                                // ICmdBuffer::CmdCopyImageToMemory() or
                                                // ICmdBuffer::CmdCopyMemoryToImage() for optimally tiled images.
        Extent3d minLinearMemCopyAlignment;     // Minimum alignments for X/Y/Z/Width/Height/Depth for
                                                // ICmdBuffer::CmdCopyTypedBuffer()
        uint32   minTimestampAlignment;         // If timestampSupport is set, this is the minimum address alignment in
                                                // bytes of the dstOffset argument to ICmdBuffer::CmdWriteTimestamp().
        uint32   availableGdsSize;              // Total GDS size in bytes available for all engines of a particular
                                                // queue type.
        uint32   gdsSizePerEngine;              // Maximum GDS size in bytes available for a single engine.
        uint32   queueSupport;                  // A mask of QueueTypeSupport flags indicating queue support.
        uint32   maxNumDedicatedCu;             // The maximum possible number of dedicated CUs per compute ring

        gpusize  contextSaveAreaSize;           // Size of the context-save-area for this engine, in bytes. This area
                                                // of memory is used for mid-command buffer preemption.
        gpusize  contextSaveAreaAlignment;      // Alignment of the context-save-area for this engine, in bytes.

        union
        {
            struct
            {
                uint32 physicalAddressingMode          :  1;
                uint32 mustBuildCmdBuffersInSystemMem  :  1;
                uint32 timestampSupport                :  1;
                uint32 borderColorPaletteSupport       :  1;
                uint32 queryPredicationSupport         :  1;
                uint32 memoryPredicationSupport        :  1;
                uint32 conditionalExecutionSupport     :  1;
                uint32 loopExecutionSupport            :  1;
                uint32 constantEngineSupport           :  1;
                uint32 regMemAccessSupport             :  1;
                uint32 supportsMismatchedTileTokenCopy :  1;
                uint32 supportsImageInitBarrier        :  1;
                uint32 supportsImageInitPerSubresource :  1;
                uint32 indirectBufferSupport           :  1;
                uint32 supportVirtualMemoryRemap       :  1;
                uint32 supportPersistentCeRam          :  1;
                uint32 supportsMidCmdBufPreemption     :  1;
                uint32 supportSvm                      :  1;
                uint32 p2pCopyToInvisibleHeapIllegal   :  1;
                uint32 reserved                        : 13;
            };
            uint32 u32All;
        } flags;

        EngineSubType engineSubType[MaxAvailableEngines];

        /// Specifies the suggested heap preference clients should use when creating an @ref ICmdAllocator that will
        /// allocate command space for this engine type.  These heap preferences should be specified in the allocHeap
        /// parameter of @ref CmdAllocatorCreateInfo.  Clients are free to ignore these defaults and use their own
        /// heap preferences, but may suffer a performance penalty.
        GpuHeap preferredCmdAllocHeaps[CmdAllocatorTypeCount];
    } perEngine[EngineTypeCount];
};

// Everything PAL & its clients would ever need to know about a GPU's available queues.
struct GpuQueueProperties
{
    struct
    {
        union
        {
            struct
            {
                uint32 supportsSwapChainPresents :  1;
                uint32 reserved                  : 31;
            };
            uint32 u32All;
        } flags;

         uint32 supportedDirectPresentModes; // A mask of PresentModeSupport flags indicating support for various
                                             // PresentModes when calling IQueue::PresentDirect().
    } perQueue[QueueTypeCount];

    // Maximum number of command streams allowed in a single Queue submission.
    uint32 maxNumCmdStreamsPerSubmit;
};

// Minimum number of command streams which need to be supported per Queue submission in order for PAL to function
// properly.
constexpr uint32 MinCmdStreamsPerSubmission = 4;

#if PAL_BUILD_GFX6
// Maximum amount of counters per GPU block.
constexpr size_t Gfx6MaxCountersPerBlock = 16;

// Contains information for perf counters for Gfx6 HW.
struct Gfx6PerfCounterInfo
{
    PerfExperimentDeviceFeatureFlags features;       // Performance experiment feature flags.

    struct
    {
        bool     available;                         // If the block is available for perf experiments.
        uint32   numShaderEngines;                  // Number of shader engines which contain this block
                                                    // (1 for global blocks)
        uint32   numShaderArrays;                   // Number of shader arrays which contain this block
                                                    // (1 for global blocks)
        uint32   numInstances;                      // Number of block instances in each shader array
        uint32   numCounters;                       // Number of counters for each instance
        uint32   maxEventId;                        // Maximum number of events for this block
        uint32   numStreamingCounters;              // Number of streaming perf ctr's for each instance
        uint32   numStreamingCounterRegs;           // Number of registers which can be configured for
                                                    // streaming counters
        uint32   spmBlockSelectCode;                // The select code for obtaining spm counter data for this block;

        struct
        {
            uint32  perfSel0RegAddr;                // Perf select register offset #0
            uint32  perfSel1RegAddr;                // Perf select register offset #1
            uint32  perfCountLoAddr;                // Performance counter low address register offset
            uint32  perfCountHiAddr;                // Performance counter high address register offset
        } regInfo[Gfx6MaxCountersPerBlock];         // Register information for each counter
    } block[static_cast<size_t>(GpuBlock::Count)];  // Counter information for each GPU block

    uint32       mcConfigRegAddress;                // Chip-specific MC_CONFIG register address
    uint32       mcWriteEnableMask;                 // Chip-specific MC_CONFIG write-enable mask
    uint32       mcReadEnableShift;                 // Chip-specific MC_CONFIG read shift mask
};
#endif // PAL_BUILD_GFX6

#if PAL_BUILD_GFX9
// Maximum amount of counters per GPU block for Gfx9
constexpr size_t MaxCountersPerBlock = 16;

// Contains information for perf counters for Gfx9
struct Gfx9PerfCounterInfo
{
    PerfExperimentDeviceFeatureFlags features;       // Performance experiment feature flags.

    struct
    {
        bool     available;                         // If the block is available for perf experiments.
        uint32   numShaderEngines;                  // Number of shader engines which contain this block
                                                    // (1 for global blocks)
        uint32   numShaderArrays;                   // Number of shader arrays which contain this block
                                                    // (1 for global blocks)
        uint32   numInstances;                      // Number of block instances in each shader array
        uint32   numCounters;                       // Number of counters for each instance
        uint32   maxEventId;                        // Maximum number of events for this block
        uint32   numStreamingCounters;              // Number of streaming perf ctr's for each instance
        uint32   numStreamingCounterRegs;           // Number of registers which can be configured for
                                                    // streaming counters
        uint32   spmBlockSelectCode;                // The select code for obtaining spm counter data for this block;
        struct
        {
            uint32  perfSel0RegAddr;                // Perf select register offset #0
            uint32  perfSel1RegAddr;                // Perf select register offset #1
            uint32  perfCountLoAddr;                // Performance counter low address register offset
            uint32  perfCountHiAddr;                // Performance counter high address register offset
            uint32  perfRsltCntlRegAddr;            // Perf result control register offset
        } regInfo[MaxCountersPerBlock];             // Register information for each counter
    } block[static_cast<size_t>(GpuBlock::Count)];  // Counter information for each GPU block
};
#endif // PAL_BUILD_GFX9

// Everything PAL & its clients would ever need to know about the actual GPU hardware.
struct GpuChipProperties
{
    uint32  gfxEngineId;    // Coarse-grain GFX engine ID (R800, SI, etc.). See cwddeci.h
    uint32  familyId;       // Hardware family ID.  Driver-defined identifier for a particular family of devices.
                            // E.g., FAMILY_SI, FAMILY_VI, etc. as defined in amdgpu_asic.h
    uint32  eRevId;         // Hardware revision ID.  Driver-defined identifier for a particular device and
                            // sub-revision in the hardware family designated by the familyId.
                            // See AMDGPU_TAHITI_RANGE, AMDGPU_FIJI_RANGE, etc. as defined in amdgpu_asic.h.
    uint32  revisionId;     // PCI revision ID.  8-bit value as reported in the device structure in the PCI config
                            // space.  Identifies a revision of a specific PCI device ID.
    uint32  deviceId;       // PCI device ID.  16-bit value device ID as reported in the PCI config space.
    uint32  gpuIndex;       // Index of this GPU in whatever group of GPU's it belongs to.

    AsicRevision revision;  // PAL specific ASIC revision identifier
    GpuType      gpuType;
    GfxIpLevel   gfxLevel;
    OssIpLevel   ossLevel;
    VceIpLevel   vceLevel;
    UvdIpLevel   uvdLevel;
    VcnIpLevel   vcnLevel;
    uint32       gfxStepping; // Stepping level of this GPU's GFX block.

    uint32   vceUcodeVersion;                   // VCE Video encode firmware version
    uint32   uvdUcodeVersion;                   // UVD Video encode firmware version

    uint32   vceFwVersionMajor;                 // VCE Video encode firmware major version
    uint32   vceFwVersionMinor;                 // VCE Video encode firmware minor version

    uint32   uvdEncodeFwInterfaceVersionMajor;  // UVD Video encode firmware interface major version
    uint32   uvdEncodeFwInterfaceVersionMinor;  // UVD Video encode firmware interface minor version
    uint32   uvdFwVersionSubMinor;              // UVD firmware sub-minor version

    struct
    {
        union
        {
            struct
            {
                /// Images created on this device support single sampled texture quilting
                uint32 supportsSingleSampleQuilting :  1;

                /// Images created on this device supports AQBS stereo mode, this AQBS stereo mode doesn't apply to the
                /// array-based stereo feature supported by Presentable images.
                uint32 supportsAqbsStereoMode       :  1;

                /// Reserved for future use.
                uint32 reserved                     : 30;
            };
            uint32 u32All;           ///< Flags packed as 32-bit uint.
        } flags;

        uint32                 minPitchAlignPixel;
        Extent3d               maxImageDimension;
        uint32                 maxImageArraySize;
        PrtFeatureFlags        prtFeatures;
        gpusize                prtTileSize;
        uint8                  numSwizzleEqs;
        const SwizzleEquation* pSwizzleEqs;
        bool                   tilingSupported[static_cast<size_t>(ImageTiling::Count)];
    } imageProperties;

    // GFXIP specific information which is shared by all GFXIP hardware layers.
#if PAL_BUILD_GFX
    struct
    {
        uint32 maxUserDataEntries;
        uint32 fastUserDataEntries[NumShaderTypes];
        uint32 vaRangeNumBits;
        uint32 realTimeCuMask;
        uint32 maxThreadGroupSize;
        uint32 maxAsyncComputeThreadGroupSize;
        uint32 gdsSize;
        uint32 hardwareContexts;
        uint32 ldsSizePerThreadGroup;                // Maximum LDS size available per thread group in bytes.
        uint32 ldsSizePerCu;                         // Maximum LDS size available per CU in KB.
        uint32 ldsGranularity;                       // LDS allocation granularity in bytes.
        uint32 offChipTessBufferSize;                // Size of each off-chip tessellation buffer in bytes.
        uint32 tessFactorBufferSizePerSe;            // Size of the tessellation-factor buffer per SE, in bytes.
        uint32 ceRamSize;                            // Maximum on-chip CE RAM size in bytes.
        uint32 tccSizeInBytes;                       // Total size in bytes of TCC (L2) in the device.
        uint32 tcpSizeInBytes;                       // Size in bytes of one TCP (L1). There is one TCP per CU.
        uint32 maxLateAllocVsLimit;                  // Maximum number of VS waves that can be in flight without
                                                     // having param cache and position buffer space.
        bool   queuesUseCaches;                      // If gfxip queue processors use cached reads/writes.
    } gfxip;
#endif

    // GFX family specific data which may differ based on graphics IP level.
    union
    {
#if PAL_BUILD_GFX6
        // Hardware-specific information for GFXIP 6/7/8 hardware.
        struct
        {
            // Values of hardware registers which the KMD reads for us during initialization.
            uint32 mcArbRamcfg;
            uint32 gbAddrConfig;
            uint32 paScRasterCfg;
            uint32 paScRasterCfg1;

            uint32 backendDisableMask;
            uint32 sqThreadTraceMask;

            uint32 gbTileMode[32];
            uint32 gbMacroTileMode[16];

            // NOTE: GFXIP 6 hardware is organized into one or two shader engines each containing one or two shader
            // arrays, which contain the CU's. GFXIP 7+ hardware simply has up to four shader engines, each with a
            // single shader array (containing the CU's).
            union
            {
                uint32 activeCuMaskGfx6[2][2];
                uint32 activeCuMaskGfx7[4];
            };
            union
            {
                uint32 alwaysOnCuMaskGfx6[2][2];
                uint32 alwaysOnCuMaskGfx7[4];
            };

            uint32 numCuPerSh;
            uint32 numCuAlwaysOnPerSh;
            uint32 numSimdPerCu;
            uint32 numWavesPerSimd;
            uint32 numActiveRbs;
            uint32 numTotalRbs;
            uint32 numShaderVisibleSgprs;
            uint32 numPhysicalSgprs;
            uint32 minSgprAlloc;
            uint32 sgprAllocGranularity;
            uint32 numShaderVisibleVgprs;
            uint32 numPhysicalVgprs;
            uint32 minVgprAlloc;
            uint32 vgprAllocGranularity;
            uint32 wavefrontSize;
            uint32 numShaderEngines;
            uint32 numShaderArrays;
            uint32 maxNumCuPerSh;
            uint32 maxNumRbPerSe;
            uint32 numMcdTiles;
            uint32 numTcaBlocks;
            uint32 numTccBlocks;
            uint32 gsVgtTableDepth;
            uint32 gsPrimBufferDepth;
            uint32 maxGsWavesPerVgt;

            struct
            {
                // Indicates the chip supports double the amount of offchip LDS buffers per SE
                uint32 doubleOffchipLdsBuffers                  :  1;
                uint32 rbPlus                                   :  1;
                uint32 sqgEventsEnabled                         :  1;
                uint32 support8bitIndices                       :  1;
                uint32 support16BitInstructions                 :  1;
                uint32 supportSetShIndexPkt                     :  1;  // Indicates support for packet SET_SH_REG_INDEX
                uint32 supportLoadRegIndexPkt                   :  1;  // Indicates support for LOAD_*_REG_INDEX packets
                uint32 supportAddrOffsetDumpAndSetShPkt         :  1;  // Indicates support for DUMP_CONST_RAM_OFFSET
                                                                       // and SE_SH_REG_OFFSET indexed packet.
                uint32 supportPreemptionWithChaining            :  1;  // Indicates microcode support for preemption of
                                                                       // chained command chunks.
                uint32 supports2BitSignedValues                 :  1;
                uint32 lbpwEnabled                              :  1; // Indicates Load Balance Per Watt is enabled
                uint32 rbReconfigureEnabled                     :  1; // Indicates RB reconfigure feature is enabled
                uint32 supportPatchTessDistribution             :  1; // HW supports patch distribution mode.
                uint32 supportDonutTessDistribution             :  1; // HW supports donut distribution mode.
                uint32 supportTrapezoidTessDistribution         :  1; // HW supports trapezoidal distribution mode.
                uint32 supportRgpTraces                         :  1; // HW supports RGP traces.

                uint32 reserved                                 : 16;

            };

            Gfx6PerfCounterInfo perfCounterInfo; // Contains information for perf counters for a specific hardware block
        } gfx6;
#endif // PAL_BUILD_GFX6
#if PAL_BUILD_GFX9
        // Hardware-specific information for GFXIP 9+ hardware.
        struct
        {
            uint32 backendDisableMask;
            uint32 gbAddrConfig;
            uint32 paScTileSteeringOverride;
            uint32 numShaderEngines;
            uint32 numShaderArrays;
            uint32 maxNumRbPerSe;
            uint32 wavefrontSize;
            uint32 numShaderVisibleSgprs;
            uint32 numPhysicalSgprs;
            uint32 minSgprAlloc;
            uint32 sgprAllocGranularity;
            uint32 numShaderVisibleVgprs;
            uint32 numPhysicalVgprs;
            uint32 minVgprAlloc;
            uint32 vgprAllocGranularity;
            uint32 numCuPerSh;
            uint32 maxNumCuPerSh;
            uint32 numTccBlocks;
            uint32 numSimdPerCu;
            uint32 numWavesPerSimd;
            uint32 numActiveRbs;
            uint32 numTotalRbs;
            uint32 gsVgtTableDepth;
            uint32 gsPrimBufferDepth;
            uint32 maxGsWavesPerVgt;
            uint32 parameterCacheLines;
            // First index is the shader array, second index is the shader engine
            uint32 activeCuMask[4][4];
            uint32 alwaysOnCuMask[4][4];

            struct
            {

                uint32 doubleOffchipLdsBuffers                  :  1; // HW supports 2x number of offchip LDS buffers
                                                                      // per SE
                uint32 supportFp16Fetch                         :  1;
                uint32 support16BitInstructions                 :  1;
                uint32 supportDoubleRate16BitInstructions       :  1;
                uint32 rbPlus                                   :  1;
                uint32 supportConservativeRasterization         :  1;
                uint32 supportPrtBlendZeroMode                  :  1;
                uint32 sqgEventsEnabled                         :  1;
                uint32 supports2BitSignedValues                 :  1;
                uint32 supportPrimitiveOrderedPs                :  1;
                uint32 lbpwEnabled                              :  1; // Indicates Load Balance Per Watt is enabled
                uint32 supportPatchTessDistribution             :  1; // HW supports patch distribution mode.
                uint32 supportDonutTessDistribution             :  1; // HW supports donut distribution mode.
                uint32 supportTrapezoidTessDistribution         :  1; // HW supports trapezoidal distribution mode.
                uint32 supportLoadRegIndexPkt                   :  1; // Indicates support for LOAD_*_REG_INDEX packets
                uint32 supportAddrOffsetDumpAndSetShPkt         :  1; // Indicates support for DUMP_CONST_RAM_OFFSET
                                                                      // and SET_SH_REG_OFFSET indexed packet.
                uint32 supportImplicitPrimitiveShader           :  1;
                uint32 supportSpp                               :  1; // HW supports Shader Profiling for Power
                uint32 validPaScTileSteeringOverride            :  1; // Value of paScTileSteeringOverride is valid
                uint32 placeholder0                             :  1; // Placeholder. Do not use.
                uint32 placeholder1                             :  3; // Placeholder. Do not use.
                uint32 timestampResetOnIdle                     :  1; // GFX OFF feature causes the timestamp to reset.
                uint32 placeholder2                             :  1; // Placeholder. Do not use.
                uint32 reserved                                 :  8;
            };

            struct
            {
                gpusize primitiveBufferVa;      // GPU Virtual Address for offchip primitive buffer.
                gpusize primitiveBufferSize;    // Size of the offchip primitive buffer.
                gpusize positionBufferVa;       // GPU Virtual Address for offchip position buffer.
                gpusize positionBufferSize;     // Size of offchip position buffer.
                gpusize controlSidebandVa;      // GPU Virtual Address for offchip control sideband buffer.
                gpusize controlSidebandSize;    // Size of offchip control sideband buffer.
                gpusize parameterCacheVa;       // GPU Virtual Address for offchip parameter cache buffer.
                gpusize parameterCacheSize;     // Size of offchip parameter cache buffer.
            } primShaderInfo;   // KMD allocated buffers for Next Generation Graphics pipelines.

            Gfx9PerfCounterInfo perfCounterInfo; // Contains info for perf counters for a specific hardware block

        } gfx9;
#endif // PAL_BUILD_GFX9
    };

    // Maximum engine and memory clock speeds (MHz)
    uint32 maxEngineClock;  // Max Engine Clock reported is not valid until device is finalized in DX builds.
    uint32 maxMemoryClock;  // Max Memory Clock reported is not valid until device is finalized in DX builds.
    uint32 alusPerClock;
    uint32 pixelsPerClock;
    uint32 primsPerClock;
    uint32 texelsPerClock;
    uint64 gpuCounterFrequency;

    uint32 enginePerfRating;
    uint32 memoryPerfRating;

    struct
    {
        uint32 bufferView;
        uint32 imageView;
        uint32 fmaskView;
        uint32 sampler;
    } srdSizes;

    struct
    {
        const void* pNullBufferView;
        const void* pNullImageView;
        const void* pNullFmaskView;
        const void* pNullSampler;
    } nullSrds;

   uint32 pciBusNumber;                 // PCI bus number in the system for this GPU
   uint32 pciDeviceNumber;              // PCI device number in the system for this GPU
   uint32 pciFunctionNumber;            // PCI function number in the system for this GPU
   bool   gpuConnectedViaThunderbolt;   // GPU connects to system through thunder bolt
   bool   requiresOnionAccess;          // Some APUs have issues with Garlic that can be worked around by using Onion

   // Data controlling the P2P PCI BAR workaround required for some hardware.  Note that this is not tied to GFXIP.
   struct
   {
       bool   required;              // True if this chip requires the workaround.
       uint32 maxCopyChunkSize;      // Maximum size of a chunk.  A chunk here refers to a range of VA that can be
                                     // written by a stream of P2P BLT commands without reprogramming the PCI BAR.
       uint32 gfxPlaceholderDwords;  // How many dwords of NOPs need to be inserted before each chunk when executing
                                     // P2P BLTs on GFX engines.  KMD will eventually patch over these NOP areas with
                                     // commands that will reprogram the PCI BAR.
       uint32 dmaPlaceholderDwords;  // Same as gfxPlaceholderDwords, but for the SDMA engine ("DMA" engine type in
                                     // PAL terminology).
   } p2pBltWaInfo;
};

// Helper function that calculates memory ops per clock for a given memory type.
uint32 MemoryOpsPerClock(LocalMemoryType memoryType);

// =====================================================================================================================
// Represents a client-configurable context for a particular physical GPU. Responsibilities include allocating GDS
// partitions. Also serves as a factory for other child objects, such as Command Buffers.
//
// Each HWIP block may require its own HW-specific Device object. To accommodate this need, the Device will contain an
// object for each HWIP block present in the associated Physical GPU (e.g., GFXIP, OSSIP, etc.). All of these objects
// share a single system-memory allocation. (See: Device::Init() for more details.)
class Device : public IDevice
{
public:
    static_assert(sizeof(DeviceInterfacePfnTable) == sizeof(IDevice::DevicePfnTable),
                  "Internal PfnTable does not match the version in IDevice.");

    static constexpr GpuHeap CmdBufInternalAllocHeap      = GpuHeap::GpuHeapGartCacheable;
    static constexpr uint32  CmdBufInternalAllocSize      = 128*1024;
    static constexpr uint32  CmdBufInternalSuballocSize   = 8 * 1024;
    static constexpr uint32  CmdBufMemReferenceLimit      = 16384;
    static constexpr uint32  InternalMemMgrAllocLimit     = 128;
    static constexpr uint32  GpuMemoryChunkGranularity    = 128 * 1024 * 1024;
    static constexpr uint32  MaxMemoryViewStride          = (1UL << 14) - 1;
    static constexpr uint32  CmdStreamReserveLimit        = 256;
    static constexpr uint32  PollInterval                 = 10;
    static constexpr uint32  OcclusionQueryDmaBufferSlots = 256;
    static constexpr uint32  OcclusionQueryDmaLowerBound  = 1024;
    static constexpr bool    CbSimpleFloatEnable          = true;

    virtual ~Device();
    virtual Result Cleanup() override;

    virtual Result EarlyInit(const HwIpLevels& ipLevels);
    virtual Result LateInit();

    virtual Result GetLinearImageAlignments(
        LinearImageAlignments* pAlignments) const override
    {
        return (m_pGfxDevice == nullptr) ? Result::ErrorUnavailable :
                m_pGfxDevice->GetLinearImageAlignments(pAlignments);
    }

    // NOTE: PAL internals can access the same information more directly via the MemoryProperties(), QueueProperties(),
    // or ChipProperties() getters.
    virtual Result GetProperties(
        DeviceProperties* pInfo) const override;

    virtual Result CheckExecutionState() const override;

    // NOTE: PAL internals can access the same information more directly via the HeapProperties() getter.
    virtual Result GetGpuMemoryHeapProperties(
        GpuMemoryHeapProperties info[GpuHeapCount]) const override;

    // NOTE: PAL internals can access the same information more directly via non-virtual functions in this class.
    virtual Result GetFormatProperties(
        MergedFormatPropertiesTable* pInfo
        ) const override;

    // NOTE: Part of the public IDevice interface.
    virtual uint32 GetValidFormatFeatureFlags(
        const ChNumFormat format,
        const ImageAspect aspect,
        const ImageTiling tiling) const override
    {
        PAL_ASSERT(m_pGfxDevice != nullptr);
        return m_pGfxDevice->GetValidFormatFeatureFlags(format, aspect, tiling);
    }

    virtual Result GetPerfExperimentProperties(
        PerfExperimentProperties* pProperties) const override;

    // NOTE: Part of the public IDevice interface.
    virtual void BindTrapHandler(PipelineBindPoint pipelineType, IGpuMemory* pGpuMemory, gpusize offset) override
    {
        PAL_ASSERT(m_pGfxDevice != nullptr);
        m_pGfxDevice->BindTrapHandler(pipelineType, pGpuMemory, offset);
    }

    // NOTE: Part of the public IDevice interface.
    virtual void BindTrapBuffer(PipelineBindPoint pipelineType, IGpuMemory* pGpuMemory, gpusize offset) override
    {
        PAL_ASSERT(m_pGfxDevice != nullptr);
        m_pGfxDevice->BindTrapBuffer(pipelineType, pGpuMemory, offset);
    }

    // NOTE: Part of the public IDevice interface.
    virtual uint32 GetMaxAtomicCounters(
        EngineType engineType,
        uint32     maxNumEngines) const override;

    // NOTE: Part of the public IDevice interface.
    virtual bool ReadSetting(
        const char*     pSettingName,
        SettingScope    settingScope,
        Util::ValueType valueType,
        void*           pValue,
        size_t          bufferSz = 0) const override;

    // This function is responsible for reading a specific settings from the OS appropriate source
    // (e.g. registry or config file)
    virtual bool ReadSetting(
        const char*          pSettingName,
        Util::ValueType      valueType,
        void*                pValue,
        InternalSettingScope settingType,
        size_t               bufferSz = 0) const = 0;

    virtual PalPublicSettings* GetPublicSettings() override;
    virtual const CmdBufferLoggerSettings& GetCmdBufferLoggerSettings() const override
        { return m_cmdBufLoggerSettings; }
    virtual const DebugOverlaySettings& GetDbgOverlaySettings() const override
        { return m_dbgOverlaySettings; }
    virtual const GpuProfilerSettings& GetGpuProfilerSettings() const override
        { return m_gpuProfilerSettings; }
    virtual const InterfaceLoggerSettings& GetInterfaceLoggerSettings() const override
        { return m_interfaceLoggerSettings; }
    virtual Result CommitSettingsAndInit() override;
    virtual Result Finalize(const DeviceFinalizeInfo& finalizeInfo) override;

    // Queries a PX application profile for the specified application filename and pathname.
    virtual Result QueryApplicationProfile(
        const char*         pFilename,
        const char*         pPathname,
        ApplicationProfile* pOut) const = 0;

    virtual Result QueryRawApplicationProfile(
        const char*              pFilename,
        const char*              pPathname,
        ApplicationProfileClient client,
        const char**             pOut) = 0;

    // NOTE: Part of the public IDevice interface.
    virtual size_t GetQueueSize(
        const QueueCreateInfo& createInfo,
        Result*                pResult) const override;

    // NOTE: Part of the public IDevice interface.
    virtual Result CreateQueue(
        const QueueCreateInfo& createInfo,
        void*                  pPlacementAddr,
        IQueue**               ppQueue) override;

    // NOTE: Part of the public IDevice interface.
    virtual gpusize GetMaxGpuMemoryAlignment() const override;

    // NOTE: Part of the public IDevice interface.
    virtual Result ResetFences(
        uint32              fenceCount,
        IFence*const*       ppFences) const override;

    // NOTE: Part of the public IDevice interface.
    virtual Result WaitForFences(
        uint32              fenceCount,
        const IFence*const* ppFences,
        bool                waitAll,
        uint64              timeout) const override;

    // Queries the size of a GpuMemory object, in bytes.
    virtual size_t GpuMemoryObjectSize() const = 0;

    // NOTE: Part of the public IDevice interface.
    virtual size_t GetGpuMemorySize(
        const GpuMemoryCreateInfo& createInfo,
        Result*                    pResult) const override;

    // NOTE: Part of the public IDevice interface.
    virtual Result CreateGpuMemory(
        const GpuMemoryCreateInfo& createInfo,
        void*                      pPlacementAddr,
        IGpuMemory**               ppGpuMemory) override;

    Result CreateInternalGpuMemory(
        const GpuMemoryCreateInfo&         createInfo,
        const GpuMemoryInternalCreateInfo& internalInfo,
        GpuMemory**                        ppGpuMemory);

    Result CreateInternalGpuMemory(
        const GpuMemoryCreateInfo&         createInfo,
        const GpuMemoryInternalCreateInfo& internalInfo,
        void*                              pPlacementAddr,
        GpuMemory**                        ppGpuMemory);

    // NOTE: Part of the public IDevice interface.
    virtual size_t GetPinnedGpuMemorySize(
        const PinnedGpuMemoryCreateInfo& createInfo,
        Result*                          pResult) const override;

    // NOTE: Part of the public IDevice interface.
    virtual Result CreatePinnedGpuMemory(
        const PinnedGpuMemoryCreateInfo& createInfo,
        void*                            pPlacementAddr,
        IGpuMemory**                     ppGpuMemory) override;

    // NOTE: Part of the public IDevice interface.
    virtual size_t GetSvmGpuMemorySize(
        const SvmGpuMemoryCreateInfo& createInfo,
        Result*                       pResult) const override;

    // NOTE: Part of the public IDevice interface.
    virtual Result CreateSvmGpuMemory(
        const SvmGpuMemoryCreateInfo& createInfo,
        void*                         pPlacementAddr,
        IGpuMemory**                  ppGpuMemory) override;

    // NOTE: Part of the public IDevice interface.
    virtual size_t GetSharedGpuMemorySize(
        const GpuMemoryOpenInfo& openInfo,
        Result*                  pResult) const override;

    // NOTE: Part of the public IDevice interface.
    virtual size_t GetExternalSharedGpuMemorySize(
        Result* pResult) const override;

    // NOTE: Part of the public IDevice interface.
    virtual Result OpenSharedGpuMemory(
        const GpuMemoryOpenInfo& openInfo,
        void*                    pPlacementAddr,
        IGpuMemory**             ppGpuMemory) override;

    // NOTE: Part of the public IDevice interface.
    virtual size_t GetPeerGpuMemorySize(
        const PeerGpuMemoryOpenInfo& openInfo,
        Result*                      pResult) const override;

    // NOTE: Part of the public IDevice interface.
    virtual Result OpenPeerGpuMemory(
        const PeerGpuMemoryOpenInfo& openInfo,
        void*                        pPlacementAddr,
        IGpuMemory**                 ppGpuMemory) override;

    virtual Result CreateInternalImage(
        const ImageCreateInfo&         createInfo,
        const ImageInternalCreateInfo& extCreateInfo,
        void*                          pPlacementAddr,
        Image**                        ppImage) = 0;

    // NOTE: Part of the public IDevice interface.
    virtual void GetPeerImageSizes(
        const PeerImageOpenInfo& openInfo,
        size_t*                  pPeerImageSize,
        size_t*                  pPeerGpuMemorySize,
        Result*                  pResult) const override;

    // NOTE: Part of the public IDevice interface.
    virtual Result OpenPeerImage(
        const PeerImageOpenInfo& openInfo,
        void*                    pImagePlacementAddr,
        void*                    pGpuMemoryPlacementAddr,
        IImage**                 ppImage,
        IGpuMemory**             ppGpuMemory) override;

    // NOTE: Part of the public IDevice interface.
    virtual size_t GetColorTargetViewSize(
        Result* pResult) const override;

    // NOTE: Part of the public IDevice interface.
    virtual Result CreateColorTargetView(
        const ColorTargetViewCreateInfo& createInfo,
        void*                            pPlacementAddr,
        IColorTargetView**               ppColorTargetView) const override;

    Result CreateInternalColorTargetView(
        const ColorTargetViewCreateInfo&         createInfo,
        const ColorTargetViewInternalCreateInfo& internalInfo,
        void*                                    pPlacementAddr,
        IColorTargetView**                       ppColorTargetView) const;

    // NOTE: Part of the public IDevice interface.
    virtual size_t GetDepthStencilViewSize(
        Result* pResult) const override;

    // NOTE: Part of the public IDevice interface.
    virtual Result CreateDepthStencilView(
        const DepthStencilViewCreateInfo& createInfo,
        void*                             pPlacementAddr,
        IDepthStencilView**               ppDepthStencilView) const override;

    Result CreateInternalDepthStencilView(
        const DepthStencilViewCreateInfo&         createInfo,
        const DepthStencilViewInternalCreateInfo& internalInfo,
        void*                                     pPlacementAddr,
        IDepthStencilView**                       ppDepthStencilView) const;

    // NOTE: Part of the public IDevice interface.
    virtual Result ValidateImageViewInfo(const ImageViewInfo& viewInfo) const override;

    // NOTE: Part of the public IDevice interface.
    virtual Result ValidateFmaskViewInfo(const FmaskViewInfo& info) const override;

    // NOTE: Part of the public IDevice interface.
    Result ValidateSamplerInfo(const SamplerInfo& info) const override;

    // NOTE: Part of the public IDevice interface.
    virtual size_t GetBorderColorPaletteSize(
        const BorderColorPaletteCreateInfo& createInfo,
        Result*                             pResult) const override
        { return (m_pGfxDevice == nullptr) ? 0 : m_pGfxDevice->GetBorderColorPaletteSize(createInfo, pResult); }

    // NOTE: Part of the public IDevice interface.
    virtual Result CreateBorderColorPalette(
        const BorderColorPaletteCreateInfo& createInfo,
        void*                               pPlacementAddr,
        IBorderColorPalette**               ppPalette) const override
    {
        return (m_pGfxDevice == nullptr) ? Result::ErrorUnavailable :
                m_pGfxDevice->CreateBorderColorPalette(createInfo, pPlacementAddr, ppPalette);
    }

    // NOTE: Part of the public IDevice interface.
    virtual size_t GetComputePipelineSize(
        const ComputePipelineCreateInfo& createInfo,
        Result*                          pResult) const override
        { return (m_pGfxDevice == nullptr) ? 0 : m_pGfxDevice->GetComputePipelineSize(createInfo, pResult); }

    // NOTE: Part of the public IDevice interface.
    virtual Result CreateComputePipeline(
        const ComputePipelineCreateInfo& createInfo,
        void*                            pPlacementAddr,
        IPipeline**                      ppPipeline) override
    {
        return (m_pGfxDevice == nullptr) ? Result::ErrorUnavailable :
                m_pGfxDevice->CreateComputePipeline(createInfo, pPlacementAddr,
                                                    createInfo.flags.clientInternal, ppPipeline);
    }

    // NOTE: Part of the public IDevice interface.
    virtual size_t GetGraphicsPipelineSize(
        const GraphicsPipelineCreateInfo& createInfo,
        Result*                           pResult) const override
        { return (m_pGfxDevice == nullptr) ? 0 : m_pGfxDevice->GetGraphicsPipelineSize(createInfo, false, pResult); }

    // NOTE: Part of the public IDevice interface.
    virtual Result CreateGraphicsPipeline(
        const GraphicsPipelineCreateInfo& createInfo,
        void*                             pPlacementAddr,
        IPipeline**                       ppPipeline) override;

    // NOTE: Part of the public IDevice interface.
    virtual size_t GetMsaaStateSize(
        const MsaaStateCreateInfo& createInfo,
        Result*                    pResult) const override
    {
        return (m_pGfxDevice == nullptr) ? 0 : m_pGfxDevice->GetMsaaStateSize(createInfo, pResult);
    }

    // NOTE: Part of the public IDevice interface.
    virtual Result CreateMsaaState(
        const MsaaStateCreateInfo& createInfo,
        void*                      pPlacementAddr,
        IMsaaState**               ppMsaaState) const override
    {
        return (m_pGfxDevice == nullptr) ? Result::ErrorUnavailable :
                m_pGfxDevice->CreateMsaaState(createInfo, pPlacementAddr, ppMsaaState);
    }
    // NOTE: Part of the public IDevice interface.
    virtual size_t GetColorBlendStateSize(
        const ColorBlendStateCreateInfo& createInfo,
        Result*                          pResult) const override
    {
        return (m_pGfxDevice == nullptr) ? 0 : m_pGfxDevice->GetColorBlendStateSize(createInfo, pResult);
    }

    // NOTE: Part of the public IDevice interface.
    virtual Result CreateColorBlendState(
        const ColorBlendStateCreateInfo& createInfo,
        void*                            pPlacementAddr,
        IColorBlendState**               ppColorBlendState) const override
    {
        return (m_pGfxDevice == nullptr) ? Result::ErrorUnavailable :
                m_pGfxDevice->CreateColorBlendState(createInfo, pPlacementAddr, ppColorBlendState);
    }

    // NOTE: Part of the public IDevice interface.
    virtual size_t GetDepthStencilStateSize(
        const DepthStencilStateCreateInfo& createInfo,
        Result*                            pResult) const override
    {
        return (m_pGfxDevice == nullptr) ? 0 :
                m_pGfxDevice->GetDepthStencilStateSize(createInfo, pResult);
    }

    // NOTE: Part of the public IDevice interface.
    virtual Result CreateDepthStencilState(
        const DepthStencilStateCreateInfo& createInfo,
        void*                              pPlacementAddr,
        IDepthStencilState**               ppDepthStencilState) const override
    {
        return (m_pGfxDevice == nullptr) ? Result::ErrorUnavailable :
                m_pGfxDevice->CreateDepthStencilState(createInfo, pPlacementAddr, ppDepthStencilState);
    }

    // NOTE: Part of the public IDevice interface.
    virtual size_t GetQueueSemaphoreSize(
        const QueueSemaphoreCreateInfo& createInfo,
        Result*                         pResult) const override;

    // NOTE: Part of the public IDevice interface.
    virtual Result CreateQueueSemaphore(
        const QueueSemaphoreCreateInfo& createInfo,
        void*                           pPlacementAddr,
        IQueueSemaphore**               ppQueueSemaphore) override;

    // NOTE: Part of the public IDevice interface.
    virtual size_t GetSharedQueueSemaphoreSize(
        const QueueSemaphoreOpenInfo& openInfo,
        Result*                       pResult) const override;

    // NOTE: Part of the public IDevice interface.
    virtual Result OpenSharedQueueSemaphore(
        const QueueSemaphoreOpenInfo& openInfo,
        void*                         pPlacementAddr,
        IQueueSemaphore**             ppQueueSemaphore) override;

    // NOTE: Part of the public IDevice interface.
    virtual size_t GetExternalSharedQueueSemaphoreSize(
        const ExternalQueueSemaphoreOpenInfo& openInfo,
        Result*                               pResult) const override;

    // NOTE: Part of the public IDevice interface.
    virtual Result OpenExternalSharedQueueSemaphore(
        const ExternalQueueSemaphoreOpenInfo& openInfo,
        void*                                 pPlacementAddr,
        IQueueSemaphore**                     ppQueueSemaphore) override;

    Result CreateInternalFence(
        const FenceCreateInfo& createInfo,
        Fence**                ppFence) const;

    // NOTE: Part of the public IDevice interface.
    virtual size_t GetGpuEventSize(
        const GpuEventCreateInfo& createInfo,
        Result*                   pResult) const override;

    // NOTE: Part of the public IDevice interface.
    virtual Result CreateGpuEvent(
        const GpuEventCreateInfo& createInfo,
        void*                     pPlacementAddr,
        IGpuEvent**               ppGpuEvent) override;

    // NOTE: Part of the public IDevice interface.
    virtual size_t GetQueryPoolSize(
        const QueryPoolCreateInfo& createInfo,
        Result*                    pResult) const override;

    // NOTE: Part of the public IDevice interface.
    virtual Result CreateQueryPool(
        const QueryPoolCreateInfo& createInfo,
        void*                      pPlacementAddr,
        IQueryPool**               ppQueryPool) const override;

    // NOTE: Part of the public IDevice interface.
    virtual size_t GetCmdAllocatorSize(
        const CmdAllocatorCreateInfo& createInfo,
        Result*                       pResult) const override;

    // NOTE: Part of the public IDevice interface.
    virtual Result CreateCmdAllocator(
        const CmdAllocatorCreateInfo& createInfo,
        void*                         pPlacementAddr,
        ICmdAllocator**               ppCmdAllocator) override;

    Result CreateInternalCmdAllocator(
        const CmdAllocatorCreateInfo& createInfo,
        CmdAllocator**                ppCmdAllocator);

    // NOTE: Part of the public IDevice interface.
    virtual size_t GetCmdBufferSize(
        const CmdBufferCreateInfo& createInfo,
        Result*                    pResult) const override;

    // NOTE: Part of the public IDevice interface.
    virtual Result CreateCmdBuffer(
        const CmdBufferCreateInfo& createInfo,
        void*                      pPlacementAddr,
        ICmdBuffer**               ppCmdBuffer) override;

    Result CreateInternalCmdBuffer(
        const CmdBufferCreateInfo&         createInfo,
        const CmdBufferInternalCreateInfo& internalInfo,
        CmdBuffer**                        ppCmdBuffer);

    // NOTE: Part of the public IDevice interface.
    virtual size_t GetPerfExperimentSize(
        const PerfExperimentCreateInfo& createInfo,
        Result*                         pResult) const override
        { return (m_pGfxDevice == nullptr) ? 0 : m_pGfxDevice->GetPerfExperimentSize(createInfo, pResult); }

    // NOTE: Part of the public IDevice interface.
    virtual Result CreatePerfExperiment(
        const PerfExperimentCreateInfo& createInfo,
        void*                           pPlacementAddr,
        IPerfExperiment**               ppPerfExperiment) const override
    {
        return (m_pGfxDevice == nullptr) ? Result::ErrorUnavailable :
               m_pGfxDevice->CreatePerfExperiment(createInfo, pPlacementAddr, ppPerfExperiment);
    }

    virtual size_t GetIndirectCmdGeneratorSize(
        const IndirectCmdGeneratorCreateInfo& createInfo,
        Result*                               pResult) const override;

    virtual Result CreateIndirectCmdGenerator(
        const IndirectCmdGeneratorCreateInfo& createInfo,
        void*                                 pPlacementAddr,
        IIndirectCmdGenerator**               ppGenerator) const override;

    // NOTE: Part of the public IDevice interface.
    virtual Result GetPrivateScreens(
        uint32*          pNumScreens,
        IPrivateScreen** ppScreens) override;

    // NOTE: Part of the public IDevice interface.
    virtual void GetPrivateScreenImageSizes(
        const PrivateScreenImageCreateInfo& createInfo,
        size_t*                             pImageSize,
        size_t*                             pGpuMemorySize,
        Result*                             pResult) const override;

    // NOTE: Part of the public IDevice interface.
    virtual Result CreatePrivateScreenImage(
        const PrivateScreenImageCreateInfo& createInfo,
        void*                               pImagePlacementAddr,
        void*                               pGpuMemoryPlacementAddr,
        IImage**                            ppImage,
        IGpuMemory**                        ppGpuMemory) override;

    // NOTE: Part of the public IDevice interface.
    virtual Result SetSamplePatternPalette(
        const SamplePatternPalette& palette) override
    {
        return (m_pGfxDevice == nullptr) ? Result::ErrorUnavailable :
               m_pGfxDevice->SetSamplePatternPalette(palette);
    }

    // NOTE: Part of the public IDevice interface.
    virtual Result CreateVirtualDisplay(
        const VirtualDisplayInfo& virtualDisplayInfo,
        uint32*                   pScreenTargetId) override
        { return Result::Unsupported; }

    // NOTE: Part of the public IDevice interface.
    virtual Result DestroyVirtualDisplay(
        uint32     screenTargetId) override
        { return Result::Unsupported; }

    // NOTE: Part of the public IDevice interface.
    virtual Result GetVirtualDisplayProperties(
        uint32                    screenTargetId,
        VirtualDisplayProperties* pProperties) override
        { return Result::Unsupported; }

    // NOTE: Part of the public IDevice interface.
    virtual bool DetermineHwStereoRenderingSupported(
        const GraphicPipelineViewInstancingInfo& viewInstancingInfo) const override;

    virtual Result InitBusAddressableGpuMemory(
        IQueue*           pQueue,
        uint32            gpuMemCount,
        IGpuMemory*const* ppGpuMemList) override
    {
        return Result::Unsupported;
    }

    Result ValidateBindObjectMemoryInput(
        const IGpuMemory* pMemObject,
        gpusize           offset,
        gpusize           objMemSize,
        gpusize           objAlignment,
        bool              allowVirtualBinding = false) const;

    // Requests the device to reserve the GPU VA partition.
    virtual Result ReserveGpuVirtualAddress(VaPartition             vaPartition,
                                            gpusize                 vaStartAddress,
                                            gpusize                 vaSize,
                                            bool                    isVirtual,
                                            VirtualGpuMemAccessMode virtualAccessMode,
                                            gpusize*                pGpuVirtAddr) = 0;

    // Requests the device to free the GPU VA range.
    virtual Result FreeGpuVirtualAddress(gpusize vaStartAddress, gpusize vaSize) = 0;

    // Checks if this GPU is the master in a group of linked GPU's.
    virtual bool IsMasterGpu() const = 0;

    GfxDevice* GetGfxDevice() const { return m_pGfxDevice; }
    OssDevice* GetOssDevice() const { return m_pOssDevice; }
    const AddrMgr* GetAddrMgr() const { return m_pAddrMgr;   }

    const GpuMemoryHeapProperties& HeapProperties(GpuHeap heap) const
        { return m_heapProperties[static_cast<size_t>(heap)]; }

    const GpuMemoryProperties& MemoryProperties() const { return m_memoryProperties; }
    const GpuEngineProperties& EngineProperties() const { return m_engineProperties; }
    const GpuQueueProperties&  QueueProperties()  const { return m_queueProperties;  }
    const GpuChipProperties& ChipProperties() const { return m_chipProperties; }
    const HwsInfo& GetHwsInfo() const { return m_hwsInfo; }
    const PerfExperimentProperties& PerfProperties() const { return m_perfExperimentProperties; }

    InternalMemMgr* MemMgr() { return &m_memMgr; }

    // Returns the internal tracked command allocator except for engines that do not support tracking.
    CmdAllocator* InternalCmdAllocator(EngineType engineType) const
        { return m_pTrackedCmdAllocator; }

    // Returns the internal untracked command allocator for queue context specific use.
    CmdAllocator* InternalUntrackedCmdAllocator() const { return m_pUntrackedCmdAllocator; }

    const PalSettings& Settings() const;
    Util::MetroHash::Hash GetSettingsHash() const;

    Platform* GetPlatform() const;

    ADDR_HANDLE AddrLibHandle() const { return GetAddrMgr()->AddrLibHandle(); }

    const GdsInfo& GdsInfo(EngineType engineType, uint32 queueIndex) const
        { return m_gdsInfo[static_cast<size_t>(engineType)][queueIndex]; }

    const uint32 GdsEngineSizes(EngineType engineType) const
        { return m_gdsSizes[static_cast<size_t>(engineType)]; }

    const bool PerPipelineBindPointGds() const
        { return m_perPipelineBindPointGds; }

    uint32 MaxQueueSemaphoreCount() const { return m_maxSemaphoreCount; }

    // Helper method to index into the format support info table.
    FormatFeatureFlags FeatureSupportFlags(ChNumFormat format, ImageTiling tiling) const
    {
        return m_pFormatPropertiesTable->features[static_cast<uint32>(format)][tiling != ImageTiling::Linear];
    }

    // Checks if a format/tiling-type pairing supports shader image-read operations.
    bool SupportsImageRead(ChNumFormat format, ImageTiling tiling) const
        { return ((FeatureSupportFlags(format, tiling) & FormatFeatureImageShaderRead) != 0); }

    // Checks if a format/tiling-type pairing supports shader image-write operations.
    bool SupportsImageWrite(ChNumFormat format, ImageTiling tiling) const
        { return ((FeatureSupportFlags(format, tiling) & FormatFeatureImageShaderWrite) != 0); }

    // Checks if a format/tiling-type pairing supports copy operations.
    bool SupportsCopy(ChNumFormat format, ImageTiling tiling) const
        { return ((FeatureSupportFlags(format, tiling) & FormatFeatureCopy) != 0); }

    // Checks if a format/tiling-type pairing supports format conversion operations.
    bool SupportsFormatConversion(ChNumFormat format, ImageTiling tiling) const
        { return ((FeatureSupportFlags(format, tiling) & FormatFeatureFormatConversion) != 0); }

    // Checks if a format/tiling-type pairing supports format conversion operations as the source image.
    bool SupportsFormatConversionSrc(ChNumFormat srcFormat, ImageTiling srcTiling) const
        { return ((FeatureSupportFlags(srcFormat, srcTiling) & FormatFeatureFormatConversionSrc) != 0); }

    // Checks if a format/tiling-type pairing supports format conversion operations as the destination image.
    bool SupportsFormatConversionDst(ChNumFormat dstFormat, ImageTiling dstTiling) const
        { return ((FeatureSupportFlags(dstFormat, dstTiling) & FormatFeatureFormatConversionDst) != 0); }

    // Checks if a format/tiling-type pairing supports shader read operations for buffer SRDs of this format.
    bool SupportsMemoryViewRead(ChNumFormat format, ImageTiling tiling) const
        { return ((FeatureSupportFlags(format, tiling) & FormatFeatureMemoryShaderRead) != 0); }

    // Checks if a format/tiling-type pairing supports shader write operations for buffer SRDs of this format.
    bool SupportsMemoryViewWrite(ChNumFormat format, ImageTiling tiling) const
        { return ((FeatureSupportFlags(format, tiling) & FormatFeatureMemoryShaderWrite) != 0); }

    // Checks if a format/tiling-type pairing supports color target operations.
    bool SupportsColor(ChNumFormat format, ImageTiling tiling) const
        { return ((FeatureSupportFlags(format, tiling) & FormatFeatureColorTargetWrite) != 0); }

    // Checks if a format/tiling-type pairing supports color blend operations.
    bool SupportsBlend(ChNumFormat format, ImageTiling tiling) const
        { return ((FeatureSupportFlags(format, tiling) & FormatFeatureColorTargetBlend) != 0); }

    // Checks if a format/tiling-type pairing supports depth target operations.
    bool SupportsDepth(ChNumFormat format, ImageTiling tiling) const
        { return ((FeatureSupportFlags(format, tiling) & FormatFeatureDepthTarget) != 0); }

    // Checks if a format/tiling-type pairing supports stencil target operations.
    bool SupportsStencil(ChNumFormat format, ImageTiling tiling) const
        { return ((FeatureSupportFlags(format, tiling) & FormatFeatureStencilTarget) != 0); }

    // Checks if a format/tiling-type pairing supports multisampled target operations.
    bool SupportsMsaa(ChNumFormat format, ImageTiling tiling) const
        { return ((FeatureSupportFlags(format, tiling) & FormatFeatureMsaaTarget) != 0); }

    // Checks if a format/tiling-type pairing supports windowed-mode present operations.
    bool SupportsWindowedPresent(ChNumFormat format, ImageTiling tiling) const
        { return ((FeatureSupportFlags(format, tiling) & FormatFeatureWindowedPresent) != 0); }

    virtual Result AddQueue(Queue* pQueue);
    void RemoveQueue(Queue* pQueue);

    Engine* GetEngine(EngineType type, uint32 engineIndex)
        { return m_pEngines[type][engineIndex]; }

    Result CreateEngine(EngineType engineType, uint32 engineIndex);

    size_t IndirectUserDataTableSize(uint32 tableId) const
        { return m_finalizeInfo.indirectUserDataTable[tableId].sizeInDwords; }
    size_t IndirectUserDataTableCeRamOffset(uint32 tableId) const
        { return m_finalizeInfo.indirectUserDataTable[tableId].offsetInDwords; }

    size_t CeRamBytesUsed(EngineType engine) const
        { return m_finalizeInfo.ceRamSizeUsed[engine]; }

    size_t CeRamDwordsUsed(EngineType engine) const { return CeRamBytesUsed(engine) / sizeof(uint32); }

    // Override per-device settings as needed
    virtual void OverrideDefaultSettings(PalSettings* pSettings) const {}

    // Helper function to call client provided private screen destroy time callback.
    void PrivateScreenDestroyNotication(void* pOwner) const
        { m_finalizeInfo.privateScreenNotifyInfo.pfnOnDestroy(pOwner); }

    void DeveloperCb(Developer::CallbackType type, void* pCbData) const
        { m_pPlatform->DeveloperCb(m_deviceIndex, type, pCbData); }

    virtual bool HwsTrapHandlerPresent() const { return false; }

    virtual const char* GetCacheFilePath() const = 0;

    // Determines the start (inclusive) and end (exclusive) virtual addresses for the specified virtual address range.
    void VirtualAddressRange(VaPartition vaPartition, gpusize* pStartVirtAddr, gpusize* pEndVirtAddr) const;

    // Chooses a VA partition based on the given VaRange enum.
    VaPartition ChooseVaPartition(VaRange range, bool isVirtual) const;

    const SettingsLoader* const GetSettingsLoader() const { return m_pSettingsLoader; }

    virtual bool IsNull() const { return false; }

    const bool IsUsingAutoPriorityForInternalAllocations() const
        { return m_memoryProperties.flags.autoPrioritySupport & m_finalizeInfo.flags.internalGpuMemAutoPriority; }

    const bool IsPreemptionSupported(EngineType engineType) const
        { return m_engineProperties.perEngine[engineType].flags.supportsMidCmdBufPreemption; }

#if PAL_ENABLE_PRINTS_ASSERTS
    bool IsCmdBufDumpEnabled() const { return m_cmdBufDumpEnabled; }
#endif
    uint32 GetFrameCount() const { return m_frameCnt; }
    void IncFrameCount();

    ImageTexOptLevel TexOptLevel() const { return m_texOptLevel; }

    void ApplyDevOverlay(const IImage& dstImage, ICmdBuffer* pCmdBuffer) const;

    bool PhysicalEnginesAvailable() const { return m_flags.physicalEnginesAvailable; }

    // UMD should write to MP1_SMN_FPS_CNT reg when it's not written by KMD
    bool ShouldWriteFrameCounterRegister() const { return m_flags.smnFpsCntRegWrittenByKmd == 0; }

    static bool EngineSupportsCompute(EngineType  engineType);
    static bool EngineSupportsGraphics(EngineType  engineType);

    bool IsP2pBltWaRequired(const GpuMemory& dstGpuMemory) const
        { return (ChipProperties().p2pBltWaInfo.required && dstGpuMemory.AccessesPeerMemory()); }

    Result P2pBltWaModifyRegionListMemory(
        const IGpuMemory&       dstGpuMemory,
        uint32                  regionCount,
        const MemoryCopyRegion* pRegions,
        uint32*                 pNewRegionCount,
        MemoryCopyRegion*       pNewRegions,
        gpusize*                pChunkAddrs) const;

    Result P2pBltWaModifyRegionListImage(
        const Pal::Image&      srcImage,
        const Pal::Image&      dstImage,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        uint32*                pNewRegionCount,
        ImageCopyRegion*       pNewRegions,
        gpusize*               pChunkAddrs) const;

    Result P2pBltWaModifyRegionListImageToMemory(
        const Pal::Image&            srcImage,
        const IGpuMemory&            dstGpuMemory,
        uint32                       regionCount,
        const MemoryImageCopyRegion* pRegions,
        uint32*                      pNewRegionCount,
        MemoryImageCopyRegion*       pNewRegions,
        gpusize*                     pChunkAddrs) const;

    Result P2pBltWaModifyRegionListMemoryToImage(
        const IGpuMemory&            srcGpuMemory,
        const Pal::Image&            dstImage,
        uint32                       regionCount,
        const MemoryImageCopyRegion* pRegions,
        uint32*                      pNewRegionCount,
        MemoryImageCopyRegion*       pNewRegions,
        gpusize*                     pChunkAddrs) const;

    const BoundGpuMemory& GetDummyChunkMem() const { return m_dummyChunkMem; }

    bool DisableSwapChainAcquireBeforeSignalingClient() const { return m_disableSwapChainAcquireBeforeSignaling; }

    bool LocalInvDropCpuWrites() const { return m_localInvDropCpuWrites; }

    void SetHdrColorspaceFormat(ScreenColorSpace newFormat) { m_hdrColorspaceFormat = newFormat; }

    const PalPublicSettings* GetPublicSettings() const
        { return static_cast<const PalPublicSettings*>(&m_publicSettings); }

protected:
    Device(
        Platform*              pPlatform,
        uint32                 deviceIndex,
        uint32                 attachedScreenCount,
        size_t                 deviceSize,
        const HwIpDeviceSizes& hwDeviceSizes,
        uint32                 maxSemaphoreCount);

    static bool DetermineGpuIpLevels(
        uint32      familyId,       // AMDGPU Family ID.
        uint32      eRevId,         // AMDGPU Revision ID.
        uint32      cpMicrocodeVersion,
        HwIpLevels* pIpLevels);

    static void GetHwIpDeviceSizes(
        const HwIpLevels& ipLevels,
        HwIpDeviceSizes*  pHwDeviceSizes,
        size_t*           pAddrMgrSize);

    void InitPerformanceRatings();
    void InitMemoryHeapProperties();
    Result InitSettings();

    virtual Result OsEarlyInit() = 0;
    virtual Result OsLateInit() = 0;

    // Responsible for setting up some of this GPU's queue properties which are based on settings.
    virtual void FinalizeQueueProperties() = 0;
    void FinalizeMemoryHeapProperties();

    virtual Result ProbeGpuVaRange(
        gpusize vaStart,
        gpusize vaSize) const
        { return Result::Success; }

    Result FindGpuVaRange(
        gpusize* pStartVaAddr,
        gpusize  vaEnd,
        gpusize  vaSize,
        gpusize  vaAlignment,
        bool     reserveCpuVa = false) const;

    Result FixupUsableGpuVirtualAddressRange(uint32 vaRangeNumBits);

    // Queries the size of a Queue object, in bytes. This can return zero if a Queue type is unsupported.
    virtual size_t QueueObjectSize(
        const QueueCreateInfo& createInfo) const = 0;

    // Constructs a new Queue object in preallocated memory.
    virtual Queue* ConstructQueueObject(
        const QueueCreateInfo& createInfo,
        void*                  pPlacementAddr) = 0;

    // Constructs a new CmdBuffer object in preallocated memory.
    Result ConstructCmdBuffer(
        const CmdBufferCreateInfo& createInfo,
        void*                      pPlacementAddr,
        CmdBuffer**                ppCmdBuffer) const;

    size_t QueueContextSize(const QueueCreateInfo& createInfo) const;

    // Constructs a new GpuMemory object in preallocated memory.
    virtual GpuMemory* ConstructGpuMemoryObject(
        void* pPlacementAddr) = 0;

    virtual Result EnumPrivateScreensInfo(uint32* pNumScreens) = 0;

    uint32 GetDeviceIndex() const
        { return m_deviceIndex; }

    Platform*      m_pPlatform;
    InternalMemMgr m_memMgr;

    // An array stores enumerated private screens info and only m_connectedPrivateScreens out of them are valid.
    PrivateScreenCreateInfo m_privateScreenInfo[MaxPrivateScreens];
    // Number of enumerated private screens, which means: first m_connectedPrivateScreens slots have valid information
    // in m_privateScreenInfo[] and m_connectedPrivateScreens out of MaxPrivateScreens slots are valid objects in
    // m_pPrivateScreens[].
    uint32          m_connectedPrivateScreens;
    // Number of emulated private screens, which will merged to m_connectedPrivateScreens after enumeration.
    uint32          m_emulatedPrivateScreens;
    // A counter starts from 0xFFFFFFFF to represent virtual target id or index for emulated private screen.
    uint32          m_emulatedTargetId;
    // An array stores currently valid private screen objects, note that this array is not necessarily ordered the same
    // as m_privateScreenInfo[] as this might be sparsely populated.
    PrivateScreen*  m_pPrivateScreens[MaxPrivateScreens];
    // An array stores emulated private screen objects.
    PrivateScreen*  m_pEmulatedPrivateScreens[MaxPrivateScreens];
    // Count of screen which are attached to the device.
    uint32          m_attachedScreenCount;

    DeviceFinalizeInfo m_finalizeInfo;
    GfxDevice*         m_pGfxDevice;
    OssDevice*         m_pOssDevice;

#if PAL_BUILD_GPUOPEN
    GpuUtil::TextWriter<Platform>*     m_pTextWriter;
#endif
    uint32                             m_devDriverClientId;

    FlglState                          m_flglState;

    GpuMemoryProperties                m_memoryProperties;
    GpuEngineProperties                m_engineProperties;
    GpuQueueProperties                 m_queueProperties;
    GpuChipProperties                  m_chipProperties;
    HwsInfo                            m_hwsInfo;
    PerfExperimentProperties           m_perfExperimentProperties;
    const MergedFormatPropertiesTable* m_pFormatPropertiesTable;
    GpuMemoryHeapProperties            m_heapProperties[GpuHeapCount];

    Engine*       m_pEngines[EngineTypeCount][MaxAvailableEngines];

    uint32        m_gdsSizes[EngineTypeCount];
    Pal::GdsInfo  m_gdsInfo[EngineTypeCount][MaxAvailableEngines];
    bool          m_perPipelineBindPointGds;

    // A mask of SwapChainModeSupport flags for each present mode.
    // This indicates which kinds of swap chains can be
    // created depending on the client's intended present mode.
    uint32 m_supportedSwapChainModes[static_cast<uint32>(PresentMode::Count)];

    char  m_gpuName[MaxDeviceName];

#if PAL_ENABLE_PRINTS_ASSERTS
    bool  m_settingsCommitted;  // Set if the client has ever called CommitSettingsAndInit().
    bool  m_deviceFinalized;    // Set if the client has ever call Finalize().
    bool  m_cmdBufDumpEnabled;  // Command buffer dumping is enabled on the next frame
#endif

    const bool m_force32BitVaSpace;  // Forces 32 bit virtual address space

    // We must keep a list of queues that have been created so that we can ask them to remove retired memory references
    // when necessary
    Util::IntrusiveList<Queue> m_queues;
    Util::Mutex                m_queueLock; // Serializes all queue-list operations

    // Gpu memory that contains a special srd used for debugging unbound descriptors.
    BoundGpuMemory m_pageFaultDebugSrdMem;

    // Dummy memory used when DeviceLost happened.
    BoundGpuMemory m_dummyChunkMem;

    BigSoftwareReleaseInfo m_bigSoftwareRelease;   // Big Software (BigSW) Release Version

    VirtualDisplayCapabilities m_virtualDisplayCaps; // Virtual display capabilities

    struct
    {
        uint32 physicalEnginesAvailable :  1;  // Client enabled 1 or more physical engines during device finalization.
        uint32 smnFpsCntRegWrittenByKmd :  1;  // KMD writes to MP1_SMN_FPS_CNT reg so UMD should skip it
        uint32 reserved                 : 30;
    } m_flags;

    bool m_disableSwapChainAcquireBeforeSignaling;
    bool m_localInvDropCpuWrites;

    PalPublicSettings       m_publicSettings;
    CmdBufferLoggerSettings m_cmdBufLoggerSettings;
    DebugOverlaySettings    m_dbgOverlaySettings;
    GpuProfilerSettings     m_gpuProfilerSettings;
    InterfaceLoggerSettings m_interfaceLoggerSettings;

private:
    Result HwlEarlyInit();
    void   InitPageFaultDebugSrd();
    Result InitDummyChunkMem();
    Result CreateInternalCmdAllocators();

    Result CreateEngines(const DeviceFinalizeInfo& finalizeInfo);

    uint64  GetTimeoutValueInNs(uint64  appTimeoutInNs) const;

    Result SetupPublicSettingDefaults();

    void CopyLayerSettings();

    AddrMgr*              m_pAddrMgr;
    CmdAllocator*         m_pTrackedCmdAllocator;
    CmdAllocator*         m_pUntrackedCmdAllocator;
    SettingsLoader*       m_pSettingsLoader;
    const uint32          m_deviceIndex;       // Unique index of this GPU compared to all other GPUs in the system.
    const size_t          m_deviceSize;
    const HwIpDeviceSizes m_hwDeviceSizes;
    const uint32          m_maxSemaphoreCount; // The OS-specific GPU semaphore max signal count.
    volatile uint32       m_frameCnt;  // Device frame count
    ImageTexOptLevel      m_texOptLevel; // Client specified texture optimize level for internally-created views
    ScreenColorSpace      m_hdrColorspaceFormat;  // Current HDR Colorspace Format

    PAL_DISALLOW_DEFAULT_CTOR(Device);
    PAL_DISALLOW_COPY_AND_ASSIGN(Device);
};

// NOTE: Below are prototypes for several utility functions for each HWIP namespace in PAL. These functions are for
// determining what IP level of a particular HWIP block (GFXIP, OSSIP, etc.) a GPU supports, as well as initializing
// the GPU chip properties for a particular version of a HWIP block. Each HWIP namespace in PAL corresponds to one
// hardware layer for that group of IP levels. Each HWIP namespace must have the following function:
//
// IpLevel DetermineIpLevel(familyId, eRevId, ...);
// * This function is used to determine the IP level that the appropriate hardware layer decides the GPU device
//   has. For hardware which is not supported by a specific hardware layer, this function will return an IP level
//   indicating the HWIP block is absent. The "..." in the argument list is to indicate that some HWL's may require
//   additional parameters beyond the typical family & revision IDs to determine the supported IP level.
//
// Each GFXIP namespace must also have the following functions:
//
// const FormatPropertiesTable* GetFormatPropertiesTable();
// * This function is used to select the proper format properties table. These tables are static so the Device
//   class can save this pointer to a member variable in its constructor.
//
// Result InitializeGpuChipProperties(GpuChipProperties*);
// * This function is used to setup default values for the fields in the GpuChipProperties structure for certain
//   GFXIP levels. Some kernel-mode drivers provide explicit overrides for these values which will be overridden
//   in the OS-specific children of the Device class.
//
// void FinalizeGpuChipProperties(GpuChipProperties*);
// * This function is used to finalize any values of GpuChipProperties which can be derived from some of the other
//   fields initialized in InitializeGpuChipProperties() or overridden by the kernel-mode driver.
//
// void InitializePerfExperimentProperties(const GpuChipProperties&, PerfExperimentProperties*);
// * This function is used to setup default values for the fields in the PerfExperimentProperties structure for
//   certain GFXIP levels.

#if PAL_BUILD_GFX6
namespace Gfx6
{
// Determines the GFXIP level of an GFXIP 6/7/8 GPU.
extern GfxIpLevel DetermineIpLevel(
    uint32 familyId,    // Hardware Family ID.
    uint32 eRevId,      // Software Revision ID.
    uint32 microcodeVersion);

// Gets the static format properties table for GFXIP 6/7/8 hardware.
extern const MergedFormatPropertiesTable* GetFormatPropertiesTable(
    GfxIpLevel gfxIpLevel);

// Initialize default values for the GPU chip properties for GFXIP 6/7/8 hardware.
extern void InitializeGpuChipProperties(
    uint32             cpUcodeVersion,
    GpuChipProperties* pInfo);

// Finalize default values for the GPU chip properties for GFXIP 6/7/8 hardware.
extern void FinalizeGpuChipProperties(
    GpuChipProperties* pInfo);

extern void InitializePerfExperimentProperties(
    const GpuChipProperties&  chipProps,
    PerfExperimentProperties* pProperties);

// Initialize default values for the GPU engine properties for GFXIP 6/7/8 hardware.
extern void InitializeGpuEngineProperties(
    GfxIpLevel           gfxIpLevel,
    uint32               familyId,
    uint32               eRevId,
    GpuEngineProperties* pInfo);

// Creates SettingsLoader object for GFXIP 6/7/8 hardware
extern Pal::SettingsLoader* CreateSettingsLoader(Pal::Device* pDevice);
} // Gfx6
#endif

#if PAL_BUILD_GFX9
namespace Gfx9
{
// Determines the GFXIP level of an GFXIP 9+ GPU.
extern GfxIpLevel DetermineIpLevel(
    uint32 familyId,    // Hardware Family ID.
    uint32 eRevId,      // Software Revision ID.
    uint32 microcodeVersion);

// Gets the static format properties table for GFXIP 9+ hardware.
extern const MergedFormatPropertiesTable* GetFormatPropertiesTable(
    GfxIpLevel gfxIpLevel);

extern void InitializePerfExperimentProperties(
    const GpuChipProperties&  chipProps,
    PerfExperimentProperties* pProperties);

// Initialize default values for the GPU chip properties for GFXIP9+ hardware.
extern void InitializeGpuChipProperties(
    uint32             cpUcodeVersion,
    GpuChipProperties* pInfo);

// Finalize default values for the GPU chip properties for GFXIP9+ hardware.
extern void FinalizeGpuChipProperties(
    GpuChipProperties* pInfo);

// Initialize default values for the GPU engine properties for GFXIP 6/7/8 hardware.
extern void InitializeGpuEngineProperties(
    GfxIpLevel           gfxIpLevel,
    uint32               familyId,
    uint32               eRevId,
    GpuEngineProperties* pInfo);

// Creates SettingsLoader object for GFXIP 9+ hardware
extern Pal::SettingsLoader* CreateSettingsLoader(Pal::Device* pDevice);
}
#endif // PAL_BUILD_GFX9

#if PAL_BUILD_OSS1
namespace Oss1
{
// Determines the OSSIP level of an OSSIP 1 GPU.
extern OssIpLevel DetermineIpLevel(
    uint32 familyId,        // Hardware Family ID.
    uint32 eRevId);         // Software Revision ID.

// Initialize default values for the GPU engine properties for OSSIP 1 hardware.
extern void InitializeGpuEngineProperties(
    GpuEngineProperties* pInfo);
} // Oss1
#endif

#if PAL_BUILD_OSS2
namespace Oss2
{
// Determines the OSSIP level of an OSSIP 2 GPU.
extern OssIpLevel DetermineIpLevel(
    uint32 familyId,        // Hardware Family ID.
    uint32 eRevId);         // Software Revision ID.

// Initialize default values for the GPU engine properties for OSSIP 2 hardware.
extern void InitializeGpuEngineProperties(
    GpuEngineProperties* pInfo);
} // Oss2
#endif

#if PAL_BUILD_OSS2_4
namespace Oss2_4
{
// Determines the OSSIP level of an OSSIP 2.4+ GPU.
extern OssIpLevel DetermineIpLevel(
    uint32 familyId,        // Hardware Family ID.
    uint32 eRevId);         // Software Revision ID.

// Initialize default values for the GPU engine properties for OSSIP 2.4 hardware.
extern void InitializeGpuEngineProperties(
    GpuEngineProperties* pInfo);
} // Oss2_4
#endif

#if PAL_BUILD_OSS4
namespace Oss4
{
// Determines the OSSIP level of an OSSIP 4 GPU.
extern OssIpLevel DetermineIpLevel(
    uint32 familyId,        // Hardware Family ID.
    uint32 eRevId);         // Software Revision ID.

// Initialize default values for the GPU engine properties for OSSIP 4 hardware.
extern void InitializeGpuEngineProperties(
    GpuEngineProperties* pInfo);
} // Oss4
#endif

// ASIC family and chip identification functions
PAL_INLINE bool IsGfx6(const Device& device)
{
    return (device.ChipProperties().gfxLevel == GfxIpLevel::GfxIp6);
}

PAL_INLINE bool IsGfx7(const Device& device)
{
    return (device.ChipProperties().gfxLevel == GfxIpLevel::GfxIp7);
}

PAL_INLINE bool IsGfx8(const Device& device)
{
    return ((device.ChipProperties().gfxLevel == GfxIpLevel::GfxIp8) ||
        (device.ChipProperties().gfxLevel == GfxIpLevel::GfxIp8_1));
}

#if PAL_BUILD_GFX9
PAL_INLINE bool IsGfx9(const Device& device)
{
    return (device.ChipProperties().gfxLevel == GfxIpLevel::GfxIp9);
}
#endif

PAL_INLINE bool IsTahiti(const Device& device)
{
    return AMDGPU_IS_TAHITI(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

PAL_INLINE bool IsPitcairn(const Device& device)
{
    return AMDGPU_IS_PITCAIRN(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

PAL_INLINE bool IsCapeVerde(const Device& device)
{
    return AMDGPU_IS_CAPEVERDE(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

PAL_INLINE bool IsOland(const Device& device)
{
    return AMDGPU_IS_OLAND(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

PAL_INLINE bool IsHainan(const Device& device)
{
    return AMDGPU_IS_HAINAN(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

PAL_INLINE bool IsBonaire(const Device& device)
{
    return AMDGPU_IS_BONAIRE(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

PAL_INLINE bool IsHawaii(const Device& device)
{
    return AMDGPU_IS_HAWAII(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

PAL_INLINE bool IsSpectre(const Device& device)
{
    return AMDGPU_IS_SPECTRE(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

PAL_INLINE bool IsSpooky(const Device& device)
{
    return AMDGPU_IS_SPOOKY(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

PAL_INLINE bool IsKalindi(const Device& device)
{
    return AMDGPU_IS_KALINDI(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

PAL_INLINE bool IsGodavari(const Device& device)
{
    return AMDGPU_IS_GODAVARI(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

PAL_INLINE bool IsIceland(const Device& device)
{
    return AMDGPU_IS_ICELAND(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

PAL_INLINE bool IsTonga(const Device& device)
{
    return AMDGPU_IS_TONGA(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

PAL_INLINE bool IsFiji(const Device& device)
{
    return AMDGPU_IS_FIJI(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

PAL_INLINE bool IsPolaris10(const Device& device)
{
    return AMDGPU_IS_POLARIS10(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

PAL_INLINE bool IsPolaris11(const Device& device)
{
    return AMDGPU_IS_POLARIS11(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

PAL_INLINE bool IsPolaris12(const Device& device)
{
    return AMDGPU_IS_POLARIS12(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

PAL_INLINE bool IsCarrizo(const Device& device)
{
    return AMDGPU_IS_CARRIZO(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

PAL_INLINE bool IsStoney(const Device& device)
{
    return AMDGPU_IS_STONEY(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

#if PAL_BUILD_GFX9
PAL_INLINE bool IsVega10(const Device& device)
{
    return AMDGPU_IS_VEGA10(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

PAL_INLINE bool IsRaven(const Device& device)
{
    return AMDGPU_IS_RAVEN(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}
#endif // PAL_BUILD_GFX9

} // Pal
