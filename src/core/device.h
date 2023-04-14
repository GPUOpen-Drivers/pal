/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/dmaUploadRing.h"
#include "palCmdAllocator.h"
#include "palDevice.h"
#include "palDeque.h"
#include "palEvent.h"
#include "palFile.h"
#include "palHashMap.h"
#include "palInlineFuncs.h"
#include "palIntrusiveList.h"
#include "palMutex.h"
#include "palPipeline.h"
#if defined(__unix__)
#include "palSettingsFileMgr.h"
#endif
#include "palSysMemory.h"
#include "palTextWriter.h"
#include "palShaderLibrary.h"
#include "palLiterals.h"

#include "core/hw/amdgpu_asic.h"

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

#if defined(__unix__)
constexpr char SettingsFileName[] = "amdVulkanSettings.cfg";
#endif

// Internal representation of the IDevice::m_pfnTable structure.
struct DeviceInterfacePfnTable
{
    CreateBufferViewSrdsFunc pfnCreateTypedBufViewSrds;
    CreateBufferViewSrdsFunc pfnCreateUntypedBufViewSrds;
    CreateImageViewSrdsFunc  pfnCreateImageViewSrds;
    CreateFmaskViewSrdsFunc  pfnCreateFmaskViewSrds;
    CreateSamplerSrdsFunc    pfnCreateSamplerSrds;
    CreateBvhSrdsFunc        pfnCreateBvhSrds;
};

// Maximum number of excluded virtual address ranges.
constexpr size_t MaxExcludedVaRanges = 32;

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
    CaptureReplay,          // SEE: VaRange::CaptureReplay
    Prt,                    // Some platforms require a specific VA range in order to properly setup HW for PRTs.
                            // To simplify client support, no corresponding VA range exists and this partition
                            // will be chosen instead of the default when required.
    Count,
};

// Tracks device-wide information for the hardware scheduler (HWS).
struct HwsContextInfo
{
    uint32 userQueueSize; // User queue size in bytes

    union
    {
        struct
        {
            uint32 numQueuesRealtime : 4;  // Realtime
            uint32 numQueuesHigh     : 4;  // High
            uint32 numQueuesMedium   : 4;  // Medium
            uint32 numQueuesNormal   : 4;  // Normal
            uint32 numQueuesLow      : 4;  // Idle
            uint32 maxTotalQueues    : 8;  // Max number of queues in a context
            uint32 reserved          : 4;
        };
        uint32 u32All;
    } bits;
};

// Indicates the number of available pipes for each engine type.
struct HwsPipesPerEngine
{
    union
    {
        struct
        {
            uint32 graphics  : 4;
            uint32 compute   : 6;
            uint32 dma       : 4;
            uint32 reserved  : 18;
        };
        uint32 u32All;
    };
};

// Indicates whether this engine instance can be used for gang submission workloads via a multi-queue.
struct GangSubmitEngineSupportFlags
{
    union
    {
        struct
        {
            uint32 graphics  : 1;
            uint32 compute   : 1;
            uint32 dma       : 1;
            uint32 reserved : 29;
        };
        uint32 u32All;
    };
};

struct HwsInfo
{
    union
    {
        struct
        {
            uint32 gfxHwsEnabled     : 1;                // flag graphic engine enablement using OS HWS (MES)
            uint32 computeHwsEnabled : 1;                // flag compute engine enablement using OS HWS (MES)
            uint32 dmaHwsEnabled     : 1;                // flag dma engine enablement using OS HWS (MES)
            uint32 vcnHwsEnabled     : 1;                // flag vcn engine enablement using OS HWS (MM HWS)
            uint32 reserved          : 28;
        };
        uint32 osHwsEnableFlags;
    };
    HwsContextInfo               gfx;                    // Graphics HWS context info
    HwsContextInfo               compute;                // Compute HWS context info
    HwsContextInfo               sdma;                   // SDMA HWS context info
    HwsContextInfo               vcn;                    // VCN HWS context info
    uint32                       gdsSaveAreaSize;        // GDS save area size in bytes
    uint64                       engineOrdinalMask;      // Indicates which engines (by ordinal) support MES HWS
    uint64                       videoEngineOrdinalMask; // Indicates which video engines (by ordinal) support UMS HWS
    // Indicates whether this engine instance can be used for gang submission workloads via a multi-queue.
    GangSubmitEngineSupportFlags gangSubmitEngineFlags;
    // Indicates the number of available pipes for each engine type.
    HwsPipesPerEngine            numOfPipesPerEngine;
};

// Additional flags that are kept with the IP levels
union HwIpLevelFlags
{
    struct {
        uint32 reserved0  : 1;
        uint32 isSpoofed  : 1; // GPU is spoofed and OS props should be ignored.
        uint32 reserved   : 30;
    };
    uint32 u32All;
};

// Bundles the IP levels for all kinds of HW IP.
struct HwIpLevels
{
    GfxIpLevel     gfx;
    OssIpLevel     oss;
    VceIpLevel     vce;
    UvdIpLevel     uvd;
    VcnIpLevel     vcn;
    SpuIpLevel     spu;
    PspIpLevel     psp;
    HwIpLevelFlags flags;
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

    gpusize nonLocalHeapSize;
    gpusize hbccSizeInBytes;    // Size of High Bandwidth Cache Controller (HBCC) memory segment.
                                // HBCC memory segment comprises of system and local video memory, where HW/KMD
                                // will ensure high performance by migrating pages accessed by hardware to local.
                                // This HBCC memory segment is only available on certain platforms.
    gpusize busAddressableMemSize;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 766
    gpusize barSize;            // Total VRAM which can be accessed by the CPU.
#endif

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

    gpusize dcnPrimarySurfaceVaStartAlign; // Starting VA alignment of primary surface provided by KMD
    gpusize dcnPrimarySurfaceVaSizeAlign;  // Physical size alignment of primary surface provided by KMD

    // BIG_PAGE is not supported for allocations < bigPageMinAlignment. If BIG_PAGE is supported then
    // allocations >= bigPageMinAlignment and < bigPageLargeAlignment must have their size, VA, and PA all aligned to
    // bigPageMinAlignment and for allocations > bigPageLargeAlignment aligned to bigPageLargeAlignment.
    // If KMD LargePage feature is disabled, bigPageLargeAlignment = bigPageMinAlignment.
    // If bigPageMinAlignment = 0, BIG_PAGE is not supported.
    gpusize bigPageLargeAlignment;
    gpusize bigPageMinAlignment;

    // ITERATE_256 is not supported for allocations < iterate256MinAlignment and in that case iterate256 bit must be
    // set to 1. If ITERATE_256 is supported then iterate256 bit is set to 0 and allocations >= iterate256MinAlignment
    // and < iterate256LargeAlignment must have their size, VA, and PA all aligned to iterate256MinAlignment and for
    // allocations > iterate256LargeAlignment aligned to iterate256LargeAlignment.
    // If KMD LargePage feature is disabled, iterate256LargeAlignment = iterate256MinAlignment.
    // If Iterate256MinAlignment is 0, iterate256 bit must always be set to 1 for the relevant depth buffers.
    gpusize iterate256LargeAlignment;
    gpusize iterate256MinAlignment;

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
            uint32 supportsTmz                :  1; // Indicates TMZ (or HSFB) protected memory is supported.
            uint32 supportsMall               :  1; // Indicates that this device supports the MALL.
            uint32 supportPageFaultInfo       :  1; // Indicates support for querying page fault information
            uint32 reserved                   : 15;
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

    struct
    {
        uint32   numAvailable;
        uint32   startAlign;                    // Alignment requirement for the starting address of command buffers.
        uint32   sizeAlignInDwords;             // Alignment requirement for the size of command buffers.
        uint32   maxControlFlowNestingDepth;    // Maximum depth of nested control-flow operations in command buffers.
        uint32   availableCeRamSize;            // Size of CE RAM available on this queue for use by clients.
        Extent3d minTiledImageCopyAlignment;    // Minimum alignments for X/Y/Z/Width/Height/Depth for
                                                // ICmdBuffer::CmdCopyImage() between optimally tiled images.
        Extent3d minTiledImageMemCopyAlignment; // Minimum alignments for X/Y/Z/Width/Height/Depth for
                                                // ICmdBuffer::CmdCopyImageToMemory() or
                                                // ICmdBuffer::CmdCopyMemoryToImage() for optimally tiled images.
        Extent3d minLinearMemCopyAlignment;     // Minimum alignments for X/Y/Z/Width/Height/Depth for
                                                // ICmdBuffer::CmdCopyTypedBuffer()
        uint32   minTimestampAlignment;         // If timestampSupport is set, this is the minimum address alignment in
                                                // bytes of the dstOffset argument to ICmdBuffer::CmdWriteTimestamp().
        uint32   queueSupport;                  // A mask of QueueTypeSupport flags indicating queue support.
        uint32   maxNumDedicatedCu;             // The maximum possible number of dedicated CUs per compute ring
        uint32   dedicatedCuGranularity;        // The granularity at which compute units can be dedicated to a queue.

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
                uint32 memory64bPredicationSupport     :  1;
                uint32 memory32bPredicationSupport     :  1;
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
                uint32 mustUseSvmIfSupported           :  1;
                uint32 supportsTrackBusyChunks         :  1;
                uint32 supportsUnmappedPrtPageAccess   :  1;
                uint32 memory32bPredicationEmulated    :  1;
                uint32 supportsClearCopyMsaaDsDst      :  1;
#if PAL_BUILD_GFX11
                uint32 supportsPws                     :  1; // HW supports pixel wait sync plus.
#else
                uint32 reserved1                       :  1;
#endif
                uint32 reserved                        :  6;
            };
            uint32 u32All;
        } flags;

        struct
        {
            union
            {
                struct
                {
                    uint32 exclusive                :  1;
                    uint32 mustUseDispatchTunneling :  1;
                    uint32 supportsMultiQueue       :  1;
                    uint32 hwsEnabled               :  1;
                    uint32 reserved                 : 28;
                };
                uint32 u32All;
            } flags;

            uint32 queuePrioritySupport;              // Mask of QueuePrioritySupport flags indicating which queue
                                                      // priority levels are supported.
            uint32 dispatchTunnelingPrioritySupport;  // Mask of QueuePrioritySupport flags indicating which queue
                                                      // priority levels support dispatch tunneling.
            uint32 maxFrontEndPipes;                  // Up to this number of IQueue objects can be consumed in
                                                      //  parallel by the front-end of this engine instance. It will
                                                      //  only be greater than 1 on hardware scheduled engine backed
                                                      //  by multiple hardware pipes/threads.
        } capabilities[MaxAvailableEngines];

        /// Specifies the suggested heap preference clients should use when creating an @ref ICmdAllocator that will
        /// allocate command space for this engine type.  These heap preferences should be specified in the allocHeap
        /// parameter of @ref CmdAllocatorCreateInfo.  Clients are free to ignore these defaults and use their own
        /// heap preferences, but may suffer a performance penalty.
        GpuHeap preferredCmdAllocHeaps[CmdAllocatorTypeCount];

        /// Indicate which queue supports per-command, per-submit, or per-queue TMZ based on the queue type.
        TmzSupportLevel tmzSupportLevel;
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
                uint32 reserved744               :  1;
                uint32 reserved                  : 30;
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

// Size of an instruction cache line (bytes).
constexpr gpusize ShaderICacheLineSize      = 64;

// Maximum number of VGPRs that any one shader can access.
constexpr uint32 MaxVgprPerShader = 256;

#if PAL_BUILD_GFX
// Blocks can be distributed across the GPU in a few different ways.
enum class PerfCounterDistribution : uint32
{
    Unavailable = 0,    // Performance counter is unavailable.
    PerShaderEngine,    // Performance counter instances are per shader engine.
    PerShaderArray,     // Performance counter instances are per shader array.
    GlobalBlock,        // Performance counter exists outside of the shader engines.
};

// Set this to the highest number of perf counter modules across all blocks (except the UMCCH which is a special case).
constexpr uint32 MaxPerfModules = 16;

// The "PERFCOUNTER" registers frequently change address values between ASICs. To help keep this mess out of the
// perf experiment code we define two structs and add them to the perf counter block info struct. Note that the
// interpretation of these values depends on other state from the block info struct and some fields will be zero
// if they have no meaning to a particular block or if that block has special case logic.

// This struct has any registers that might have different addresses for each module.
struct PerfCounterRegAddrPerModule
{
    uint32 selectOrCfg; // PERFCOUNTER#_SELECT or PERFCOUNTER#_CFG, depending on whether it's cfg-style.
    uint32 select1;     // PERFCOUNTER#_SELECT1 for perfmon modules.
    uint32 lo;          // PERCOUNTER#_LO or PERFCOUNTER_LO for cfg-style.
    uint32 hi;          // PERCOUNTER#_HI or PERFCOUNTER_HI for cfg-style.
};

// A container for all perf counter register addresses for a single block. This is only used by blocks which use the
// same register addresses for all instances. Some global blocks with multiple instances (e.g., SDMA) don't listen to
// GRBM_GFX_INDEX and instead have unique register addresses for each instance so they can't use this struct; they are
// rare so we treat them as special cases.
struct PerfCounterRegAddr
{
    uint32 perfcounterRsltCntl; // Cfg-style blocks define a shared PERFCOUNTER_RSLT_CNTL register.

    // Any registers that might have different addresses for each module. Indexed by the counter number in the registers
    // (e.g., the 2 in CB_PERFCOUNTER2_LO).
    PerfCounterRegAddrPerModule perfcounter[MaxPerfModules];
};

// Contains general information about perf counters for a HW block.
struct PerfCounterBlockInfo
{
    PerfCounterDistribution distribution;            // How the block is distributed across the chip.
    uint32                  numInstances;            // Number of block instances in each distribution.
    uint32                  numGlobalInstances;      // Number of instances multiplied by the number of distributions.
    uint32                  maxEventId;              // Maximum valid event ID for this block, note zero is valid.
    uint32                  num16BitSpmCounters;     // Maximum number of 16-bit SPM counters per instance.
    uint32                  num32BitSpmCounters;     // Maximum number of 32-bit SPM counters per instance.
    uint32                  numGlobalOnlyCounters;   // Number of global counters that are legacy only, per instance.
    uint32                  numGlobalSharedCounters; // Number of global counters that use the same counter state as
                                                     // SPM counters, per instance.

    // If the instance group size is equal to one, every block instance has its own independent counter hardware.
    // PAL guarantees this is true for all non-DF blocks.
    //
    // Otherwise the instance group size will be a value greater than one which indicates how many sequential
    // instances share the same counter hardware. The client must take care to not enable too many counters within
    // each of these groups.
    //
    // For example, the DfMall block may expose 16 instances with 8 global counters but define a group size of 16.
    // In that case all instances are part of one massive group which uses one pool of counter state such that no
    // combination of DfMall counter configurations can exceed 8 global counters.
    uint32 instanceGroupSize;

    // These fields are meant only for internal use in the perf experiment code.
    PerfCounterRegAddr regAddr;     // The perfcounter register addresses for this block.
    uint32 numGenericSpmModules;    // Number of SPM perfmon modules per instance. Can be configured as 1 global
                                    // counter, 1-2 32-bit SPM counters, or 1-4 16-bit SPM counters.
    uint32 numGenericLegacyModules; // Number of legacy (global only) counter modules per instance.
    uint32 numSpmWires;             // The number of 32-bit serial data wires going to the RLC. This is the ultimate
                                    // limit on the number of SPM counters.
    uint32 spmBlockSelect;          // Identifies this block in the RLC's SPM select logic.
    bool   isCfgStyle;              // An alternative counter programming model that: specifies legacy  "CFG" registers
                                    // instead of "SELECT" registers, uses a master "RSLT_CNTL" register, and can
                                    // optionally use generic SPM.
};

// SDMA is a global block with unique registers for each instance; this requires special handling.
constexpr uint32 Gfx7MaxSdmaInstances   = 2;
constexpr uint32 Gfx7MaxSdmaPerfModules = 2;

// Contains information for perf counters for the Gfx6 layer.
struct Gfx6PerfCounterInfo
{
    PerfExperimentDeviceFeatureFlags features;
    PerfCounterBlockInfo             block[static_cast<size_t>(GpuBlock::Count)];

    struct
    {
        uint32                       regAddress;      // Chip-specific MC_CONFIG register address
        uint32                       readEnableShift; // Chip-specific MC_CONFIG read shift mask
        uint32                       writeEnableMask; // Chip-specific MC_CONFIG write-enable mask
    } mcConfig;                                       // We need this information to access the MC counters.

    // SDMA addresses are handled specially
    PerfCounterRegAddrPerModule      sdmaRegAddr[Gfx7MaxSdmaInstances][Gfx7MaxSdmaPerfModules];
};

// SDMA is a global block with unique registers for each instance; this requires special handling.
constexpr uint32 Gfx9MaxSdmaInstances   = 2;
constexpr uint32 Gfx9MaxSdmaPerfModules = 2;

#if PAL_BUILD_GFX11
constexpr uint32 Gfx9MaxShaderEngines = 6;  // GFX11 parts have six SE's
// In gfx11, the max number of wgp instances per shader array is 5.
// The max number of shader array per shader engine is 2.
// The max number of shader engines is 6.
constexpr uint32 Gfx11MaxWgps = 5 * 6 * 2;
// Minimum PFP uCode version that indicates the device is running in RS64 mode
constexpr uint32 Gfx11Rs64MinPfpUcodeVersion = 300;
#else
constexpr uint32 Gfx9MaxShaderEngines = 4;  // We can't have more than 4 SEs on gfx9+.
#endif

// UMC is the block that interfaces between the Scalable Data Fabric (SDF) and the physical DRAM. Each UMC block
// has 1..n channels. Typically, there is one UMC channel per EA block, or one per SDP (Scalable Data Port). We
// abstract this as the "UMCCH" (UMC per CHannel), a global block with one instance per channel. The UMC is totally
// outside of the graphics core so it defines unique registers for each channel which requires special handling.
constexpr uint32 Gfx9MaxUmcchInstances   = 32;
constexpr uint32 Gfx9MaxUmcchPerfModules = 5;

// Contains information for perf counters for the Gfx9 layer.
struct Gfx9PerfCounterInfo
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
#endif

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

    AsicRevision   revision;  // PAL specific ASIC revision identifier
    GpuType        gpuType;
    GfxIpLevel     gfxLevel;
    OssIpLevel     ossLevel;
    VceIpLevel     vceLevel;
    UvdIpLevel     uvdLevel;
    VcnIpLevel     vcnLevel;
    SpuIpLevel     spuLevel;
    PspIpLevel     pspLevel;
    HwIpLevelFlags hwIpFlags;
    uint32         gfxStepping; // Stepping level of this GPU's GFX block.

    uint32   vceUcodeVersion;                   // VCE Video encode firmware version
    uint32   uvdUcodeVersion;                   // UVD Video encode firmware version
    uint32   vcnUcodeVersion;                   // VCN Video encode firmware version

    uint32   vceFwVersionMajor;                 // VCE Video encode firmware major version
    uint32   vceFwVersionMinor;                 // VCE Video encode firmware minor version

    uint32   uvdEncodeFwInterfaceVersionMajor;  // UVD Video encode firmware interface major version
    uint32   uvdEncodeFwInterfaceVersionMinor;  // UVD Video encode firmware interface minor version
    uint32   uvdFwVersionSubMinor;              // UVD firmware sub-minor version

    uint32   vcnEncodeFwInterfaceVersionMajor;  // VCN Video encode firmware interface major version
    uint32   vcnEncodeFwInterfaceVersionMinor;  // VCN Video encode firmware interface minor version
    uint32   vcnFwVersionSubMinor;              // VCN Video firmware sub-minor version (revision)

    uint32 cpUcodeVersion;      // Command Processor feature version.
                                // Assume all blocks in the CP share the same feature version.
    uint32 pfpUcodeVersion;     // Command Processor, compute engine firmware version.

    struct
    {
        union
        {
            struct
            {
                // Images created on this device supports AQBS stereo mode, this AQBS stereo mode doesn't apply to the
                // array-based stereo feature supported by Presentable images.
                uint32 supportsAqbsStereoMode       :  1;

                // Images created on this device support being sampled with corner sampling.
                uint32 supportsCornerSampling       :  1;
                // Whether to support Display Dcc
                uint32 supportDisplayDcc            :  1;
                // Placeholder, do not use.
                uint32 placeholder0                 :  1;
                // Reserved for future use.
                uint32 reserved                     : 28;
            };
            uint32 u32All;
        } flags;

        uint32                 minPitchAlignPixel;
        Extent3d               maxImageDimension;
        uint32                 maxImageArraySize;
        PrtFeatureFlags        prtFeatures;
        gpusize                prtTileSize;
        MsaaFlags              msaaSupport;
        uint8                  maxMsaaFragments;
        uint8                  numSwizzleEqs;
        const SwizzleEquation* pSwizzleEqs;
        bool                   tilingSupported[static_cast<size_t>(ImageTiling::Count)];
        Extent2d               vrsTileSize;  // Pixel dimensions of a VRS tile.  0x0 indicates image-based shading
                                             // rate is not supported.
    } imageProperties;

    // GFXIP specific information which is shared by all GFXIP hardware layers.
#if PAL_BUILD_GFX
    struct
    {
        uint32 maxUserDataEntries;
        uint32 vaRangeNumBits;
        uint32 realTimeCuMask;
        uint32 maxThreadGroupSize;
        uint32 maxAsyncComputeThreadGroupSize;
        uint32 maxComputeThreadGroupCountX;          // Maximum number of compute thread groups to dispatch
        uint32 maxComputeThreadGroupCountY;
        uint32 maxComputeThreadGroupCountZ;
        uint32 hardwareContexts;
        uint32 ldsSizePerThreadGroup;                // Maximum LDS size available per thread group in bytes.
        uint32 ldsSizePerCu;                         // Maximum LDS size available per CU in KB.
        uint32 ldsGranularity;                       // LDS allocation granularity in bytes.
        uint32 numOffchipTessBuffers;
        uint32 offChipTessBufferSize;                // Size of each off-chip tessellation buffer in bytes.
        uint32 tessFactorBufferSizePerSe;            // Size of the tessellation-factor buffer per SE, in bytes.
        uint32 ceRamSize;                            // Maximum on-chip CE RAM size in bytes.
        uint32 maxPrimgroupSize;
        uint32 mallSizeInBytes;                      // Total size in bytes of MALL (Memory Attached Last Level - L3)
                                                     // cache in the device.
        uint32 tccSizeInBytes;                       // Total size in bytes of TCC (L2) in the device.
        uint32 tcpSizeInBytes;                       // Size in bytes of one TCP (L1). There is one TCP per CU.
        uint32 gl1cSizePerSa;                        // Size in bytes of GL1 cache per SA.
        uint32 instCacheSizePerCu;                   // Size in bytes of instruction cache per CU/WGP.
        uint32 scalarCacheSizePerCu;                 // Size in bytes of scalar cache per CU/WGP.
        uint32 maxLateAllocVsLimit;                  // Maximum number of VS waves that can be in flight without
                                                     // having param cache and position buffer space.
        uint32 shaderPrefetchBytes;                  // Number of bytes the SQ will prefetch, if any.

        uint32 gl2UncachedCpuCoherency;              // If supportGl2Uncached is set, then this is a bitmask of all
                                                     // CacheCoherencyUsageFlags that will be coherent with CPU
                                                     // reads/writes. Note that reporting CoherShader only means
                                                     // that GLC accesses will be CPU coherent.
                                                     // Note: Only valid if supportGl2Uncached is true.
        uint32 maxGsOutputVert;                      // Maximum number of GS vertices output.
        uint32 maxGsTotalOutputComponents;           // Maximum number of GS output components totally.
        uint32 maxGsInvocations;                     // Maximum number of GS prim instances, corresponding to geometry
                                                     // shader invocation in glsl.
        uint32 dynamicLaunchDescSize;                // Dynamic compute pipeline launch descriptor size in bytes
        // Mask of active pixel packers. The mask is 128 bits wide, assuming a max of 32 SEs and a max of 4 pixel
        // packers (indicated by a single bit each) per physical SE (includes harvested SEs).
        uint32 activePixelPackerMask[ActivePixelPackerMaskDwords];

        struct
        {
            uint32 supportGl2Uncached               :  1; // Indicates support for the allocation of GPU L2
                                                          // un-cached memory. See gl2UncachedCpuCoherency
            uint32 supportsVrs                      :  1; // Indicates support for variable rate shading
#if (PAL_BUILD_GFX11)
            uint32 supportsSwStrmout                :  1; // Indicates support for software streamout
#else
            uint32 reserved2                        :  1;
#endif
            uint32 supportsHwVs                     :  1; // Indicates hardware support for Vertex Shaders
            uint32 reserved3                        :  1;
            uint32 supportCaptureReplay             :  1; // Indicates support for Capture Replay
            uint32 supportHsaAbi                    :  1;
            uint32 supportAceOffload                :  1;
            uint32 supportStaticVmid                :  1; // Indicates support for static-VMID.
            uint32 supportFloat32BufferAtomics      :  1; // Indicates support for float32 buffer atomics
            uint32 supportFloat32ImageAtomics       :  1; // Indicates support for float32 image atomics
            uint32 supportFloat32BufferAtomicAdd    :  1; // Indicates support for float32 buffer atomics add op
            uint32 supportFloat32ImageAtomicAdd     :  1; // Indicates support for float32 image atomics add op
            uint32 supportFloat32ImageAtomicMinMax  :  1; // Indicates support for float32 image atomics min and max op
            uint32 supportFloat64BufferAtomicMinMax :  1; // Indicates support for float64 image atomics min and max op
            uint32 supportFloat64SharedAtomicMinMax :  1; // Indicates support for float64 shared atomics min and max op

            uint32 reserved                         : 16;
        };
    } gfxip;
#endif

    // GFX family specific data which may differ based on graphics IP level.
    union
    {
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
            uint32 numPhysicalVgprsPerSimd;
            uint32 numPhysicalVgprs;
            uint32 minVgprAlloc;
            uint32 vgprAllocGranularity;
            uint32 nativeWavefrontSize;
            uint32 numShaderEngines;
            uint32 numShaderArrays;
            uint32 numScPerSe;              // Num Shader Complex per Shader Engine
            uint32 numPackerPerSc;
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
                uint32 support64BitInstructions                 :  1;
                uint32 supportBorderColorSwizzle                :  1;
                uint64 supportFloat64Atomics                    :  1;
                uint32 supportIndexAttribIndirectPkt            :  1;  // Indicates support for INDEX_ATTRIB_INDIRECT
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
                uint32 placeholder0                             :  1; // Placeholder. Do not use.
                uint32 supportOutOfOrderPrimitives              :  1; // HW supports higher throughput for out of order
                uint32 supportShaderSubgroupClock               :  1; // HW supports clock functions across subgroup.
                uint32 supportShaderDeviceClock                 :  1; // HW supports clock functions across device.
                uint32 supportImageViewMinLod                   :  1; // Indicates image srd supports min_lod.
                uint32 reserved                                 :  7;
            };

            Gfx6PerfCounterInfo perfCounterInfo; // Contains information for perf counters for a specific hardware block
        } gfx6;
        // Hardware-specific information for GFXIP 9+ hardware.
        struct
        {
            uint32 backendDisableMask;
            uint32 gbAddrConfig;
            uint32 spiConfigCntl;
            uint32 paScTileSteeringOverride;
            uint32 numShaderEngines;        // Max Possible SEs on this GPU
            uint32 activeSeMask;
            uint32 numActiveShaderEngines;  // Number of Non-harvested SEs
            uint32 numShaderArrays;
            uint32 maxNumRbPerSe;
            uint32 activeNumRbPerSe;
            uint32 numScPerSe;              // Num Shader Complex per Shader Engine
            uint32 numPackerPerSc;
            uint32 nativeWavefrontSize;
            uint32 minWavefrontSize;        // The smallest supported wavefront size.
            uint32 maxWavefrontSize;        // All powers of two between the min size and max size are supported.
            uint32 numShaderVisibleSgprs;
            uint32 numPhysicalSgprs;
            uint32 minSgprAlloc;
            uint32 sgprAllocGranularity;
            uint32 numPhysicalVgprsPerSimd;
            uint32 numPhysicalVgprs;
            uint32 minVgprAlloc;
            uint32 vgprAllocGranularity;
            uint32 numCuPerSh;
            uint32 maxNumCuPerSh;
            uint32 numActiveCus;
            uint32 numAlwaysOnCus;
            uint32 numPhysicalCus;
            uint32 numTccBlocks;
            uint32 numSimdPerCu;
            uint32 numWavesPerSimd;
            uint32 numSqcBarriersPerCu;
            uint32 numActiveRbs;
            uint32 numTotalRbs;
            uint32 gsVgtTableDepth;
            uint32 gsPrimBufferDepth;
            uint32 maxGsWavesPerVgt;
            uint32 parameterCacheLines;
            uint32 sdmaDefaultRdL2Policy;
            uint32 sdmaDefaultWrL2Policy;
            bool   sdmaL2PolicyValid;
            uint32 activeCuMask  [Gfx9MaxShaderEngines] [MaxShaderArraysPerSe]; // Indexed using the physical SE
            uint32 alwaysOnCuMask[Gfx9MaxShaderEngines] [MaxShaderArraysPerSe]; // Indexed using the physical SE

            uint32 numSdpInterfaces;          // Number of Synchronous Data Port interfaces to memory.

            struct
            {
                uint32  numTcpPerSa;
                uint32  numWgpAboveSpi;
                uint32  numWgpBelowSpi;
                uint32  numGl2a;
                uint32  numGl2c;

                uint32  supportedVrsRates;          // Bitmask of VrsShadingRate enumerations indicating supported modes
                bool    supportVrsWithDsExports;    // If true, asic support coarse VRS rates
                                                    // when z or stencil exports are enabled

                uint32  minNumWgpPerSa;
                uint32  maxNumWgpPerSa;
                uint16  activeWgpMask  [Gfx9MaxShaderEngines] [MaxShaderArraysPerSe];   // Indexed using the physical SE
                uint16  alwaysOnWgpMask[Gfx9MaxShaderEngines] [MaxShaderArraysPerSe];   // Indexed using the physical SE
            } gfx10;

            struct
            {

                uint64 doubleOffchipLdsBuffers            :  1; // HW supports 2x number of offchip LDS buffers
                                                                // per SE
                uint64 supportFp16Fetch                   :  1;
                uint64 supportFp16Dot2                    :  1;
                uint64 support16BitInstructions           :  1;
                uint64 support64BitInstructions           :  1;
                uint64 supportBorderColorSwizzle          :  1;
                uint64 supportFloat64Atomics              :  1;
                uint64 supportDoubleRate16BitInstructions :  1;
                uint64 rbPlus                             :  1;
                uint64 supportConservativeRasterization   :  1;
                uint64 supportPrtBlendZeroMode            :  1;
                uint64 supports2BitSignedValues           :  1;
                uint64 supportPrimitiveOrderedPs          :  1;
                uint64 lbpwEnabled                        :  1; // Indicates Load Balance Per Watt is enabled
                uint64 supportPatchTessDistribution       :  1; // HW supports patch distribution mode.
                uint64 supportDonutTessDistribution       :  1; // HW supports donut distribution mode.
                uint64 supportTrapezoidTessDistribution   :  1; // HW supports trapezoidal distribution mode.
                uint64 supportAddrOffsetDumpAndSetShPkt   :  1; // Indicates support for DUMP_CONST_RAM_OFFSET
                                                                // and SET_SH_REG_OFFSET indexed packet.
                uint64 supportAddrOffsetSetSh256Pkt       :  1; // Indicates support for SET_SH_REG_OFFSET_256B
                                                                // indexed packet.
                uint64 supportImplicitPrimitiveShader     :  1;
                uint64 supportSpp                         :  1; // HW supports Shader Profiling for Power
                uint64 validPaScTileSteeringOverride      :  1; // Value of paScTileSteeringOverride is valid
                uint64 placeholder0                       :  1; // Placeholder. Do not use.
                uint64 supportPerShaderStageWaveSize      :  1; // HW supports changing the wave size
                uint64 supportCustomWaveBreakSize         :  1;
                uint64 supportMsaaCoverageOut             :  1; // HW supports MSAA coverage samples
                uint64 supportPostDepthCoverage           :  1; // HW supports post depth coverage feature
                uint64 supportSpiPrefPriority             :  1;
                uint64 timestampResetOnIdle               :  1; // GFX OFF feature causes the timestamp to reset.
                uint64 support1xMsaaSampleLocations       :  1; // HW supports 1xMSAA custom quad sample patterns
                uint64 supportReleaseAcquireInterface     :  1; // Set if HW supports the basic functionalities of
                                                                // acquire /release-based barrier interface.This
                                                                // provides CmdReleaseThenAcquire() as a convenient
                                                                // way to replace the legacy barrier interface's
                                                                // CmdBarrier() to handle single point barriers.
                uint64 supportSplitReleaseAcquire         :  1; // Set if HW supports additional split barrier feature
                                                                // on top of basic acquire/release interface support.
                                                                // This provides CmdAcquire() and CmdRelease() to
                                                                // implement split barriers.
                                                                // Note: supportReleaseAcquireInterface is a
                                                                // prerequisite to supportSplitReleaseAcquire.
                uint64 eccProtectedGprs                   :  1; // Are VGPR's ECC-protected?
                uint64 overrideDefaultSpiConfigCntl       :  1; // KMD provides default value for SPI_CONFIG_CNTL.
                uint64 supportOutOfOrderPrimitives        :  1; // HW supports higher throughput for out of order
                uint64 supportIntersectRayBarycentrics    :  1; // HW supports the ray intersection mode which
                                                                // returns triangle barycentrics.
                uint64 supportShaderSubgroupClock         :  1; // HW supports clock functions across subgroup.
                uint64 supportShaderDeviceClock           :  1; // HW supports clock functions across device.
                uint64 supportAlphaToOne                  :  1; // HW supports forcing alpha channel to one
                uint64 supportSingleChannelMinMaxFilter   :  1; // HW supports any min/max filter.
                uint64 supportSortAgnosticBarycentrics    :  1; // HW provides provoking vertex for custom interp
                uint64 supportMeshShader                  :  1;
                uint64 supportTaskShader                  :  1;
                uint64 supportMsFullRangeRtai             :  1; // HW supports full range render target array
                                                                // for Mesh Shaders.
#if PAL_BUILD_GFX11
                uint64 supportRayTraversalStack           :  1;
                uint64 supportPointerFlags                :  1;
#else
                uint64 placeholder5                       :  2;
#endif
                uint64 supportTextureGatherBiasLod        :  1; // HW supports SQ_IMAGE_GATHER4_L_O
                uint64 supportInt8Dot                     :  1; // HW supports a dot product 8bit.
                uint64 supportInt4Dot                     :  1; // HW supports a dot product 4bit.
                uint64 support2DRectList                  :  1; // HW supports PrimitiveTopology::TwoDRectList.
                uint64 supportImageViewMinLod             :  1; // Indicates image srd supports min_lod.
                uint64 stateShadowingByCpFw               :  1; // Indicates that state shadowing is done is CP FW.
                uint64 support3dUavZRange                 :  1; // HW supports read-write ImageViewSrds of 3D images
                                                                // with zRange specified.
                uint64 reserved                           : 11;
            };

            RayTracingIpLevel rayTracingIp;      //< HW RayTracing IP version

            Gfx9PerfCounterInfo perfCounterInfo; // Contains info for perf counters for a specific hardware block

        } gfx9;
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
        uint32 bvh;
    } srdSizes;

    struct
    {
        const void* pNullBufferView;
        const void* pNullImageView;
        const void* pNullFmaskView;
        const void* pNullSampler;
    } nullSrds;

   uint32 pciDomainNumber;              // PCI domain number in the system for this GPU
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

    union
    {
        struct
        {
            uint32 p2pSupportEncode     : 1; // whether encoding HW can access FB memory of remote GPU in chain
            uint32 p2pSupportDecode     : 1; // whether decoding HW can access FB memory of remote GPU in chain
            uint32 p2pSupportTmz        : 1; // whether protected content can be transferred over P2P
            uint32 p2pCrossGPUCoherency : 1; // whether remote FB memory can be accessed without need for cache flush
            uint32 xgmiEnabled          : 1; // Whether XGMI is enabled
            uint32 reserved             : 27;
        };
        uint32 u32All;
    } p2pSupport;

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
    static constexpr uint32  CmdBufInternalAllocSize      = 128 * Util::OneKibibyte;
    static constexpr uint32  CmdBufInternalSuballocSize   = 8 * Util::OneKibibyte;
    static constexpr uint32  CmdBufMemReferenceLimit      = 16384;
    static constexpr uint32  InternalMemMgrAllocLimit     = 128;
    static constexpr uint32  GpuMemoryChunkGranularity    = 128 * Util::OneMebibyte;
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

    virtual Result CheckExecutionState(
        PageFaultStatus* pPageFaultStatus) const override;

    // NOTE: PAL internals can access the same information more directly via the HeapProperties() getter.
    virtual Result GetGpuMemoryHeapProperties(
        GpuMemoryHeapProperties info[GpuHeapCount]) const override;

    // NOTE: PAL internals can access the same information more directly via non-virtual functions in this class.
    virtual Result GetFormatProperties(
        MergedFormatPropertiesTable* pInfo
        ) const override;

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
        size_t               bufferSz = 0) const;

    virtual PalPublicSettings* GetPublicSettings() override;
    virtual Result CommitSettingsAndInit() override;
    virtual Result Finalize(const DeviceFinalizeInfo& finalizeInfo) override;

    Result SplitSubresRanges(
        uint32              rangeCount,
        const SubresRange*  pRanges,
        uint32*             pSplitRangeCount,
        const SubresRange** ppSplitRanges,
        bool*               pMemAllocated) const;

    static Result SplitBarrierTransitions(
        Platform*    pPlatform,
        BarrierInfo* pBarrier,
        bool*        pMemAllocated);

    static Result SplitImgBarriers(
        Platform*           pPlatform,
        AcquireReleaseInfo* pBarrier,
        bool*               pMemAllocated);

    virtual Result QueryRawApplicationProfile(
        const wchar_t*           pFilename,
        const wchar_t*           pPathname,
        ApplicationProfileClient client,
        const char**             pOut) = 0;

    virtual Result EnableSppProfile(
        const wchar_t*           pFilename,
        const wchar_t*           pPathname) = 0;

    virtual Result SelectSppTable(
        uint32 pixelCount,
        uint32 msaaRate) const = 0;

    // NOTE: Part of the public IDevice interface.
    virtual size_t GetQueueSize(
        const QueueCreateInfo& createInfo,
        Result*                pResult) const override;

    // Helper method for determining the size of a Queue context object, in bytes.
    size_t QueueContextSize(const QueueCreateInfo& createInfo) const;

    // NOTE: Part of the public IDevice interface.
    virtual Result CreateQueue(
        const QueueCreateInfo& createInfo,
        void*                  pPlacementAddr,
        IQueue**               ppQueue) override;

    // NOTE: Part of the public IDevice interface.
    virtual size_t GetMultiQueueSize(
        uint32                 queueCount,
        const QueueCreateInfo* pCreateInfo,
        Result*                pResult) const override;

    // NOTE: Part of the public IDevice interface.
    virtual Result CreateMultiQueue(
        uint32                 queueCount,
        const QueueCreateInfo* pCreateInfo,
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
        const ColorTargetViewCreateInfo&  createInfo,
        ColorTargetViewInternalCreateInfo internalInfo,
        void*                             pPlacementAddr,
        IColorTargetView**                ppColorTargetView) const;

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
    virtual size_t GetShaderLibrarySize(
        const ShaderLibraryCreateInfo& createInfo,
        Result*                        pResult) const override
    {
        return (m_pGfxDevice == nullptr) ? 0 : m_pGfxDevice->GetShaderLibrarySize(createInfo, pResult);
    }

    virtual Result CreateShaderLibrary(
        const ShaderLibraryCreateInfo& createInfo,
        void*                          pPlacementAddr,
        IShaderLibrary**               ppLibrary) override
    {
        return (m_pGfxDevice == nullptr) ? Result::ErrorUnavailable :
                m_pGfxDevice->CreateShaderLibrary(createInfo, pPlacementAddr,
                                                  createInfo.flags.clientInternal, ppLibrary);
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
    virtual bool CanEnableDualSourceBlend(
        const ColorBlendStateCreateInfo& createInfo) const override
    {
        return (m_pGfxDevice == nullptr) ? false : m_pGfxDevice->CanEnableDualSourceBlend(createInfo);
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

    // NOTE: Part of the public IDevice interface.
    virtual const char* GetCacheFilePath() const override
        { return m_cacheFilePath; }

    // NOTE: Part of the public IDevice interface.
    virtual const char* GetDebugFilePath() const override
        { return m_debugFilePath; }

    // NOTE: Part of the public IDevice interface.
    virtual Result SetStaticVmidMode(
        bool enable) override;

    virtual Result InitBusAddressableGpuMemory(
        IQueue*           pQueue,
        uint32            gpuMemCount,
        IGpuMemory*const* ppGpuMemList) override
    {
        return Result::Unsupported;
    }

    virtual Result AddGpuMemoryReferences(
        uint32              gpuMemRefCount,
        const GpuMemoryRef* pGpuMemoryRefs,
        IQueue*             pQueue,
        uint32              flags
        ) override;

    virtual Result RemoveGpuMemoryReferences(
        uint32            gpuMemoryCount,
        IGpuMemory*const* ppGpuMemory,
        IQueue*           pQueue
        ) override;

    virtual void GetReferencedMemoryTotals(
        gpusize  referencedGpuMemTotal[GpuHeapCount]) const override;

    static Result ValidateBindObjectMemoryInput(
        const IGpuMemory* pMemObject,
        gpusize           offset,
        gpusize           objMemSize,
        gpusize           objAlignment,
        bool              allowVirtualBinding = false);

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
    { return m_heapProperties[heap]; }

    gpusize HeapLogicalSize(GpuHeap heap) const
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 766
        return HeapProperties(heap).logicalSize;
#else
        return HeapProperties(heap).heapSize;
#endif
    }

    const GpuMemoryProperties& MemoryProperties() const { return m_memoryProperties; }
    const GpuEngineProperties& EngineProperties() const { return m_engineProperties; }
    const GpuQueueProperties&  QueueProperties()  const { return m_queueProperties;  }
    const GpuChipProperties&   ChipProperties()   const { return m_chipProperties;   }
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

    uint32 MaxQueueSemaphoreCount() const { return m_maxSemaphoreCount; }

    // Helper method to index into the format support info table.
    FormatFeatureFlags FeatureSupportFlags(ChNumFormat format, ImageTiling tiling) const
    {
        return m_pFormatPropertiesTable->features[static_cast<uint32>(format)][tiling != ImageTiling::Linear];
    }

    bool SupportStateShadowingByCpFw() const
    {
        return m_chipProperties.gfx9.stateShadowingByCpFw;
    }

    bool SupportsStaticVmid() const
        { return m_chipProperties.gfxip.supportStaticVmid; }

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

    // Checks if a format is supported.
    bool SupportsFormat(ChNumFormat format) const
    {
        return ((FeatureSupportFlags(format, ImageTiling::Linear)  != 0) ||
                (FeatureSupportFlags(format, ImageTiling::Optimal) != 0));
    }

    virtual Result AddQueue(Queue* pQueue);
    void RemoveQueue(Queue* pQueue);

    Engine* GetEngine(EngineType type, uint32 engineIndex)
        { return m_pEngines[type][engineIndex]; }

    Result CreateEngine(EngineType engineType, uint32 engineIndex);

    CmdStream* GetDummyCommandStream(EngineType engineType)
        { return m_pDummyCommandStreams[engineType]; }

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

    virtual bool LegacyHwsTrapHandlerPresent() const { return false; }

    // Determines the start (inclusive) and end (exclusive) virtual addresses for the specified virtual address range.
    void VirtualAddressRange(VaPartition vaPartition, gpusize* pStartVirtAddr, gpusize* pEndVirtAddr) const;

    // Chooses a VA partition based on the given VaRange enum.
    VaPartition ChooseVaPartition(VaRange range, bool isVirtual) const;

    const SettingsLoader* const GetSettingsLoader() const { return m_pSettingsLoader; }

    virtual bool IsNull() const { return false; }

    bool IsUsingAutoPriorityForInternalAllocations() const
        { return m_memoryProperties.flags.autoPrioritySupport & m_finalizeInfo.flags.internalGpuMemAutoPriority; }

    bool IsPreemptionSupported(EngineType engineType) const
        { return m_engineProperties.perEngine[engineType].flags.supportsMidCmdBufPreemption; }

    bool IsConstantEngineSupported(EngineType engineType) const
        { return (m_engineProperties.perEngine[engineType].flags.constantEngineSupport != 0); }

#if PAL_BUILD_GFX11
    // Returns whether any pixel-wait-sync-plus feature can be enabled.
    bool UsePws(EngineType engineType) const;

    // Returns whether the pixel-wait-sync-plus late acquire point feature can be enabled.
    bool UsePwsLateAcquirePoint(EngineType engineType) const;
#endif

#if PAL_ENABLE_PRINTS_ASSERTS
    bool IsCmdBufDumpEnabledViaHotkey() const { return m_cmdBufDumpEnabledViaHotkey; }
#else
    bool IsCmdBufDumpEnabledViaHotkey() const { return false; }
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

    void SetHdrColorspaceFormat(ScreenColorSpace newFormat) { m_hdrColorspaceFormat = newFormat; }

    bool UsingHdrColorspaceFormat() const;

    const PalPublicSettings* GetPublicSettings() const
        { return static_cast<const PalPublicSettings*>(&m_publicSettings); }

    virtual bool ValidatePipelineUploadHeap(const GpuHeap& preferredHeap) const;

    // Add or subtract some memory from our per-heap totals. We refcount each added GPU memory object so it's safe
    // to add memory multiple times or subtract it multiple times.
    Result AddToReferencedMemoryTotals(
        uint32              gpuMemRefCount,
        const GpuMemoryRef* pGpuMemoryRefs);

    Result SubtractFromReferencedMemoryTotals(
        uint32            gpuMemoryCount,
        IGpuMemory*const* ppGpuMemory,
        bool              forceSubtract);

    bool    IsSpoofed() const { return m_chipProperties.hwIpFlags.isSpoofed != 0; }
    IfhMode GetIfhMode() const;

    // Helper for creating DmaUploadRing for PAL internal use.
    virtual Result CreateDmaUploadRing() = 0;

    Result AcquireRingSlot(UploadRingSlot* pSlotId);

    size_t UploadUsingEmbeddedData(
        UploadRingSlot  slotId,
        Pal::GpuMemory* pDst,
        gpusize         dstOffset,
        size_t          bytes,
        void**          ppEmbeddedData);

    Result SubmitDmaUploadRing(
        UploadRingSlot    slotId,
        UploadFenceToken* pCompletionFence,
        uint64            pagingFenceVal);

    Result WaitForPendingUpload(
        Pal::Queue* pWaiter,
        UploadFenceToken fenceValue);

    virtual bool IsHwEmulationEnabled() const { return false; }

    bool HasLargeLocalHeap() const;

    bool IssueSqttMarkerEvents() const;
    bool EnablePerfCountersInPreamble() const;

    const FlglState& GetFlglState() const { return m_flglState; }

    void LogCodeObjectToDisk(
        Util::StringView<char> prefix,
        Util::StringView<char> name,
        PipelineHash           hash,
        bool                   isInternal,
        const void*            pCodeObject,
        size_t                 codeObjectLen) const;

    bool IsFinalized() const { return m_deviceFinalized; }
    size_t NumQueues() const { return m_queues.NumElements(); }
    uint32 AttachedScreenCount() const { return m_attachedScreenCount; }

protected:
    Device(
        Platform*              pPlatform,
        uint32                 deviceIndex,
        uint32                 attachedScreenCount,
        size_t                 deviceSize,
        const HwIpDeviceSizes& hwDeviceSizes,
        uint32                 maxSemaphoreCount);

    static bool DetermineGpuIpLevels(
        uint32            familyId,       // AMDGPU Family ID.
        uint32            eRevId,         // AMDGPU Revision ID.
        uint32            cpMicrocodeVersion,
        const Platform*   pPlatform,
        HwIpLevels*       pIpLevels);

    static void GetHwIpDeviceSizes(
        const HwIpLevels& ipLevels,
        HwIpDeviceSizes*  pHwDeviceSizes,
        size_t*           pAddrMgrSize);

    void InitPerformanceRatings();
    void InitMemoryHeapProperties();
    Result InitSettings();

    virtual Result OsEarlyInit() { return Result::Success; }
    virtual Result OsLateInit()  { return Result::Success; }

    virtual Result OsSetStaticVmidMode(
        bool enable) = 0;

    virtual void OsFinalizeSettings() { }

    // Responsible for setting up some of this GPU's queue properties which are based on settings.
    virtual void FinalizeQueueProperties() = 0;
    void FinalizeMemoryHeapProperties();

    virtual Result ProbeGpuVaRange(
        gpusize     vaStart,
        gpusize     vaSize,
        VaPartition vaPartition) const
        { return Result::Success; }

    Result FindGpuVaRangeReverse(
        gpusize*    pVaStart,
        gpusize     vaEnd,
        gpusize     vaSize,
        gpusize     vaAlignment,
        VaPartition vaParttion) const;

    Result FindGpuVaRange(
        gpusize*    pStartVaAddr,
        gpusize     vaEnd,
        gpusize     vaSize,
        gpusize     vaAlignment,
        VaPartition vaParttion,
        bool        reserveCpuVa = false) const;

    Result FixupUsableGpuVirtualAddressRange(uint32 vaRangeNumBits);

    // Queries the size of a Queue object, in bytes. This can return zero if a Queue type is unsupported.
    virtual size_t QueueObjectSize(
        const QueueCreateInfo& createInfo) const = 0;

    // Queries the size of a MultiQueue object, in bytes. This can return zero if a Queue type is unsupported.
    virtual size_t MultiQueueObjectSize(
        uint32                 queueCount,
        const QueueCreateInfo* pCreateInfo) const
    {
        return 0;
    }

    // Constructs a new Queue object in preallocated memory.
    virtual Queue* ConstructQueueObject(
        const QueueCreateInfo& createInfo,
        void*                  pPlacementAddr) = 0;

    // Constructs a new MutltiQueue object in preallocated memory.
    virtual Queue* ConstructMultiQueueObject(
        uint32                 queueCount,
        const QueueCreateInfo* pCreateInfo,
        void*                  pPlacementAddr)
    {
        return nullptr;
    }

    // Constructs a new CmdBuffer object in preallocated memory.
    Result ConstructCmdBuffer(
        const CmdBufferCreateInfo& createInfo,
        void*                      pPlacementAddr,
        CmdBuffer**                ppCmdBuffer) const;

    // Constructs a new GpuMemory object in preallocated memory.
    virtual GpuMemory* ConstructGpuMemoryObject(
        void* pPlacementAddr) = 0;

    virtual Result EnumPrivateScreensInfo(
        uint32* pNumScreens) = 0;

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

    GpuUtil::TextWriter<Platform>*     m_pTextWriter;
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

    CmdStream*    m_pDummyCommandStreams[EngineTypeCount];

    char  m_gpuName[MaxDeviceName];

    bool  m_deviceFinalized;    // Set if the client has ever call Finalize().

#if PAL_ENABLE_PRINTS_ASSERTS
    bool  m_settingsCommitted;           // Set if the client has ever called CommitSettingsAndInit().
    bool  m_cmdBufDumpEnabledViaHotkey;  // Command buffer dumping is enabled on the next frame
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

    PalPublicSettings      m_publicSettings;
    SettingsLoader*        m_pSettingsLoader;

#if defined(__unix__)
    Util::SettingsFileMgr<Platform>  m_settingsMgr;
#endif

    // Get*FilePath need to return a persistent storage
    char m_cacheFilePath[Util::MaxPathStrLen];
    char m_debugFilePath[Util::MaxPathStrLen];

    Util::Mutex    m_dmaUploadRingLock;
    DmaUploadRing* m_pDmaUploadRing;

    uint32         m_staticVmidRefCount;

private:
    Result HwlEarlyInit();
    void   InitPageFaultDebugSrd();
    Result InitDummyChunkMem();
    Result CreateInternalCmdAllocators();

    Result SetupPublicSettingDefaults();

    Result CreateEngines(const DeviceFinalizeInfo& finalizeInfo);

    Result CreateDummyCommandStreams();

    uint64 GetTimeoutValueInNs(uint64  appTimeoutInNs) const;

    typedef Util::HashMap<IGpuMemory*, uint32, Pal::Platform>  MemoryRefMap;

    MemoryRefMap  m_referencedGpuMem;
    Util::Mutex   m_referencedGpuMemLock;
    gpusize       m_referencedGpuMemBytes[GpuHeapCount];

    AddrMgr*               m_pAddrMgr;
    CmdAllocator*          m_pTrackedCmdAllocator;
    CmdAllocator*          m_pUntrackedCmdAllocator;
    const uint32           m_deviceIndex;       // Unique index of this GPU compared to all other GPUs in the system.
    const size_t           m_deviceSize;
    const HwIpDeviceSizes  m_hwDeviceSizes;
    const uint32           m_maxSemaphoreCount; // The OS-specific GPU semaphore max signal count.
    volatile uint32        m_frameCnt;  // Device frame count
    ImageTexOptLevel       m_texOptLevel; // Client specified texture optimize level for internally-created views
    ScreenColorSpace       m_hdrColorspaceFormat;  // Current HDR Colorspace Format

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
    const Device&      device,
    GpuChipProperties* pInfo);

extern void InitializePerfExperimentProperties(
    const GpuChipProperties&  chipProps,
    PerfExperimentProperties* pProperties);

// Initialize default values for the GPU engine properties for GFXIP 6/7/8 hardware.
extern void InitializeGpuEngineProperties(
    const GpuChipProperties& chipProps,
    GpuEngineProperties* pInfo);

} // Gfx6

namespace Gfx9
{
// Determines the GFXIP level of an GFXIP 9+ GPU.
extern GfxIpLevel DetermineIpLevel(
    uint32 familyId,    // Hardware Family ID.
    uint32 eRevId,      // Software Revision ID.
    uint32 microcodeVersion);

// Gets the static format properties table for GFXIP 9+ hardware.
extern const MergedFormatPropertiesTable* GetFormatPropertiesTable(
    GfxIpLevel                 gfxIpLevel,
    const PalPlatformSettings& settings);

extern void InitializePerfExperimentProperties(
    const GpuChipProperties&  chipProps,
    PerfExperimentProperties* pProperties);

// Initialize default values for the GPU chip properties for GFXIP9+ hardware.
extern void InitializeGpuChipProperties(
    const Platform*    pPlatform,
    uint32             cpUcodeVersion,
    GpuChipProperties* pInfo);

// Finalize default values for the GPU chip properties for GFXIP9+ hardware.
extern void FinalizeGpuChipProperties(
    const Device&      device,
    GpuChipProperties* pInfo);

// Initialize default values for the GPU engine properties for GFXIP9+ hardware.
extern void InitializeGpuEngineProperties(
    const GpuChipProperties&  chipProps,
    GpuEngineProperties*      pInfo);
}

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
inline bool IsGfx6(const Device& device)
{
    return (device.ChipProperties().gfxLevel == GfxIpLevel::GfxIp6);
}

inline bool IsGfx7(const Device& device)
{
    return (device.ChipProperties().gfxLevel == GfxIpLevel::GfxIp7);
}

inline bool IsGfx8(const Device& device)
{
    return ((device.ChipProperties().gfxLevel == GfxIpLevel::GfxIp8) ||
        (device.ChipProperties().gfxLevel == GfxIpLevel::GfxIp8_1));
}

constexpr bool IsGfx9(GfxIpLevel gfxLevel)
{
    return (gfxLevel == GfxIpLevel::GfxIp9);
}
inline bool IsGfx9(const Device& device)
{
    return IsGfx9(device.ChipProperties().gfxLevel);
}

#if PAL_BUILD_GFX11
constexpr bool IsGfx11(GfxIpLevel gfxLevel)
{
    return (gfxLevel == GfxIpLevel::GfxIp11_0)
        ;
}

inline bool IsGfx11(const Device& device)
{
    return IsGfx11(device.ChipProperties().gfxLevel);
}

#if PAL_BUILD_NAVI31
inline bool IsNavi31(const Device& device)
{
    return AMDGPU_IS_NAVI31(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}
inline bool IsNavi31XtxA0(const Device& device)
{
    return SKU_IS_NAVI31_XTX_A0(device.ChipProperties().deviceId,
                                device.ChipProperties().eRevId,
                                device.ChipProperties().revisionId);
}
#endif
#if PAL_BUILD_NAVI3X
inline bool IsNavi3x(const Device& device)
{
    return (device.ChipProperties().familyId == FAMILY_NV3);
}
#endif
#endif

constexpr bool IsGfx10(GfxIpLevel gfxLevel)
{
    return ((gfxLevel == GfxIpLevel::GfxIp10_1)
            || (gfxLevel == GfxIpLevel::GfxIp10_3)
           );
}
inline bool IsGfx10(const Device& device)
{
    return IsGfx10(device.ChipProperties().gfxLevel);
}

constexpr bool IsGfx10Plus(GfxIpLevel gfxLevel)
{
    return IsGfx10(gfxLevel)
#if PAL_BUILD_GFX11
           || IsGfx11(gfxLevel)
#endif
           ;
}

inline bool IsGfx10Plus(const Device& device)
{
    return IsGfx10Plus(device.ChipProperties().gfxLevel);
}

inline bool IsTahiti(const Device& device)
{
    return AMDGPU_IS_TAHITI(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

inline bool IsPitcairn(const Device& device)
{
    return AMDGPU_IS_PITCAIRN(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

inline bool IsCapeVerde(const Device& device)
{
    return AMDGPU_IS_CAPEVERDE(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

inline bool IsOland(const Device& device)
{
    return AMDGPU_IS_OLAND(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

inline bool IsHainan(const Device& device)
{
    return AMDGPU_IS_HAINAN(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

inline bool IsBonaire(const Device& device)
{
    return AMDGPU_IS_BONAIRE(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

inline bool IsHawaii(const Device& device)
{
    return AMDGPU_IS_HAWAII(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

inline bool IsSpectre(const Device& device)
{
    return AMDGPU_IS_SPECTRE(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

inline bool IsSpooky(const Device& device)
{
    return AMDGPU_IS_SPOOKY(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

inline bool IsKalindi(const Device& device)
{
    return AMDGPU_IS_KALINDI(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

inline bool IsGodavari(const Device& device)
{
    return AMDGPU_IS_GODAVARI(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

inline bool IsIceland(const Device& device)
{
    return AMDGPU_IS_ICELAND(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

inline bool IsTonga(const Device& device)
{
    return AMDGPU_IS_TONGA(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

inline bool IsFiji(const Device& device)
{
    return AMDGPU_IS_FIJI(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

inline bool IsPolaris10(const Device& device)
{
    return AMDGPU_IS_POLARIS10(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

inline bool IsPolaris11(const Device& device)
{
    return AMDGPU_IS_POLARIS11(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

inline bool IsPolaris12(const Device& device)
{
    return AMDGPU_IS_POLARIS12(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

inline bool IsCarrizo(const Device& device)
{
    return AMDGPU_IS_CARRIZO(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

inline bool IsStoney(const Device& device)
{
    return AMDGPU_IS_STONEY(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

inline bool IsVega10(const Device& device)
{
    return AMDGPU_IS_VEGA10(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

inline bool IsVega12(const Device& device)
{
    return AMDGPU_IS_VEGA12(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

inline bool IsVega20(const Device& device)
{
    return AMDGPU_IS_VEGA20(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

inline bool IsRavenFamily(const Device& device)
{
    return FAMILY_IS_RV(device.ChipProperties().familyId);
}
inline bool IsRaven(const Device& device)
{
    return AMDGPU_IS_RAVEN(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

inline bool IsRaven2(const Device& device)
{
    return AMDGPU_IS_RAVEN2(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

inline bool IsRenoir(const Device& device)
{
    return AMDGPU_IS_RENOIR(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

inline bool IsNavi(const Device& device)
{
    return AMDGPU_IS_NAVI(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}
inline bool IsNavi10(const Device& device)
{
    return AMDGPU_IS_NAVI10(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}
inline bool IsNavi12(const Device& device)
{
    return AMDGPU_IS_NAVI12(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

// The ASICs for are still referred within the interface and null backend.
// So we still keep identification suppport for them though they're no longer supported
inline bool IsNavi14(const Device& device)
{
    return AMDGPU_IS_NAVI14(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}
inline bool IsNavi1x(const Device& device)
{
        return (
            IsNavi10(device)
            || IsNavi12(device)
            || IsNavi14(device)
           );
}
inline bool IsNavi21(const Device& device)
{
    return AMDGPU_IS_NAVI21(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}
inline bool IsNavi22(const Device& device)
{
    return AMDGPU_IS_NAVI22(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}
inline bool IsNavi23(const Device& device)
{
    return AMDGPU_IS_NAVI23(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}
inline bool IsNavi24(const Device& device)
{
    return AMDGPU_IS_NAVI24(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}
inline bool IsNavi2x(const Device& device)
{
    return (IsNavi21(device) ||
            IsNavi22(device) ||
            IsNavi23(device)
            || IsNavi24(device)
           );
}

// The ASICs are still referred within the interface and null backend.
// So we still keep identification suppport for them though they're no longer supported.

inline bool IsRembrandt(const Device& device)
{
    return AMDGPU_IS_REMBRANDT(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

inline bool IsRaphael(const Device& device)
{
    return AMDGPU_IS_RAPHAEL(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

inline bool IsMendocino(const Device& device)
{
    return AMDGPU_IS_MENDOCINO(device.ChipProperties().familyId, device.ChipProperties().eRevId);
}

constexpr bool IsGfx103(GfxIpLevel gfxLevel)
{
    return (gfxLevel == GfxIpLevel::GfxIp10_3);
}
inline bool IsGfx103(const Device& device)
{
    return (device.ChipProperties().gfxLevel == GfxIpLevel::GfxIp10_3);
}
constexpr bool IsGfx103Derivative(GfxIpLevel gfxLevel)
{
    return ((gfxLevel == GfxIpLevel::GfxIp10_3)
           );
}
inline bool IsGfx103Derivative(const Device& device)
{
    return (IsGfx103Derivative(device.ChipProperties().gfxLevel));
}
inline bool IsGfx103Plus(GfxIpLevel gfxLevel)
{
    return (gfxLevel > GfxIpLevel::GfxIp10_1);
}
inline bool IsGfx103Plus(const Device& device)
{
    return IsGfx103Plus(device.ChipProperties().gfxLevel);
}
constexpr bool IsGfx103PlusExclusive(GfxIpLevel gfxLevel)
{
    return ((gfxLevel > GfxIpLevel::GfxIp10_1)
           );
}
inline bool IsGfx103PlusExclusive(const Device& device)
{
    return IsGfx103PlusExclusive(device.ChipProperties().gfxLevel);
}
constexpr bool IsGfx103CorePlus(GfxIpLevel gfxLevel)
{
    return ((gfxLevel >= GfxIpLevel::GfxIp10_3)
            );
}
inline bool IsGfx103CorePlus(const Device& device)
{
    return (IsGfx103CorePlus(device.ChipProperties().gfxLevel));
}
constexpr bool IsGfx101(GfxIpLevel gfxLevel)
{
    return (gfxLevel == GfxIpLevel::GfxIp10_1);
}
inline bool IsGfx101(const Device& device)
{
    return IsGfx101(device.ChipProperties().gfxLevel);
}
inline bool IsGfx10Bard(const Device& device)
{
    return (false
            );
}
#if  PAL_BUILD_GFX11
inline bool IsGfx104Plus(const Device& device)
{
    return (false
#if PAL_BUILD_GFX11
            || IsGfx11(device)
#endif
    );
}
constexpr bool IsGfx104Plus(GfxIpLevel gfxLevel)
{
    return (false
#if PAL_BUILD_GFX11
            || IsGfx11(gfxLevel)
#endif
    );
}
#endif

inline bool IsGfx091xPlus(const Device& device)
{
    return (IsVega12(device) || IsVega20(device) || IsRaven2(device) || IsRenoir(device) || IsGfx10(device)
#if PAL_BUILD_GFX11
            || IsGfx11(device)
#endif
           );
}

} // Pal
