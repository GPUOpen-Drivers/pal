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

// DevDriver includes
#include <ddAmdGpuInfo.h>
#include <ddPlatform.h>

// Linux/DRM includes
#include <amdgpu.h>
#include <amdgpu_drm.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <xf86drm.h>

// WA: We need to build on CentOS 7, which uses an older libdrm that does not define this.
// This was introduced in libdrm 2.4.99.
#ifndef AMDGPU_VRAM_TYPE_GDDR6
    #define AMDGPU_VRAM_TYPE_GDDR6 9
#endif

// WA: We need to build on CentOS 7, which uses an older libdrm that does not define this.
// WA: We need to build on Ubuntu 16.04, which uses an older libdrm that does not define this.
#ifndef AMDGPU_VRAM_TYPE_DDR4
    #define AMDGPU_VRAM_TYPE_DDR4 8
#endif

// WA: We need to build on CentOS 7, which uses an older libdrm that does not define this.
// WA: We need to build on Ubuntu 16.04, which uses an older libdrm that does not define this.
#ifndef AMDGPU_VRAM_TYPE_DDR5
    #define AMDGPU_VRAM_TYPE_DDR5 10
#endif

// WA: We need to build on CentOS 7, which uses an older libdrm that does not define this.
// WA: We need to build on Ubuntu 16.04, which uses an older libdrm that does not define this.
#ifndef AMDGPU_VRAM_TYPE_LPDDR5
   #define AMDGPU_VRAM_TYPE_LPDDR5 12
#endif

namespace DevDriver
{
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Structure returned by amdgpu_query_gpu_info() to describe GPU h/w info
struct amdgpu_gpu_info
{
    /** Asic id */
    uint32_t asic_id;
    /** Chip revision */
    uint32_t chip_rev;
    /** Chip external revision */
    uint32_t chip_external_rev;
    /** Family ID */
    uint32_t family_id;
    /** Special flags */
    uint64_t ids_flags;
    /** max engine clock*/
    uint64_t max_engine_clk;
    /** max memory clock */
    uint64_t max_memory_clk;
    /** number of shader engines */
    uint32_t num_shader_engines;
    /** number of shader arrays per engine */
    uint32_t num_shader_arrays_per_engine;
    /**  Number of available good shader pipes */
    uint32_t avail_quad_shader_pipes;
    /**  Max. number of shader pipes.(including good and bad pipes  */
    uint32_t max_quad_shader_pipes;
    /** Number of parameter cache entries per shader quad pipe */
    uint32_t cache_entries_per_quad_pipe;
    /**  Number of available graphics context */
    uint32_t num_hw_gfx_contexts;
    /** Number of render backend pipes */
    uint32_t rb_pipes;
    /**  Enabled render backend pipe mask */
    uint32_t enabled_rb_pipes_mask;
    /** Frequency of GPU Counter */
    uint32_t gpu_counter_freq;
    /** CC_RB_BACKEND_DISABLE.BACKEND_DISABLE per SE */
    uint32_t backend_disable[4];
    /** Value of MC_ARB_RAMCFG register*/
    uint32_t mc_arb_ramcfg;
    /** Value of GB_ADDR_CONFIG */
    uint32_t gb_addr_cfg;
    /** Values of the GB_TILE_MODE0..31 registers */
    uint32_t gb_tile_mode[32];
    /** Values of GB_MACROTILE_MODE0..15 registers */
    uint32_t gb_macro_tile_mode[16];
    /** Value of PA_SC_RASTER_CONFIG register per SE */
    uint32_t pa_sc_raster_cfg[4];
    /** Value of PA_SC_RASTER_CONFIG_1 register per SE */
    uint32_t pa_sc_raster_cfg1[4];
    /* CU info */
    uint32_t cu_active_number;
    uint32_t cu_ao_mask;
    uint32_t cu_bitmap[4][4];
    /* video memory type info*/
    uint32_t vram_type;
    /* video memory bit width*/
    uint32_t vram_bit_width;
    /** constant engine ram size*/
    uint32_t ce_ram_size;
    /* vce harvesting instance */
    uint32_t vce_harvest_config;
    /* PCI revision ID */
    uint32_t pci_rev_id;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function pointer prototypes for the functions we will retrieve from the libdrm_amdgpu library
typedef int32 (*PFN_DrmGetDevices)(drmDevicePtr* pDevices, int32 maxDevices);

typedef int32 (*PFN_AmdgpuQueryGpuInfo)(amdgpu_device_handle hDevice, struct amdgpu_gpu_info* pInfo);

typedef int32 (*PFN_AmdgpuDeviceInitialize)(
    int                   fd,
    uint32*               pMajorVersion,
    uint32*               pMinorVersion,
    amdgpu_device_handle* pDeviceHandle);

typedef int32 (*PFN_AmdgpuDeviceDeinitialize)(amdgpu_device_handle hDevice);

typedef const char* (*PFN_AmdgpuGetMarketingName)(amdgpu_device_handle hDevice);

typedef int32 (*PFN_AmdgpuQueryInfo)(amdgpu_device_handle hDevice, uint32 infoId, uint32 size, void* pValue);

typedef int32 (
    *PFN_AmdgpuQueryHeapInfo)(amdgpu_device_handle hDevice, uint32 heap, uint32 flags, struct amdgpu_heap_info* pInfo);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static LocalMemoryType TranslateMemoryType(uint32 memType)
{
    switch (memType)
    {
        case AMDGPU_VRAM_TYPE_GDDR1:
        case AMDGPU_VRAM_TYPE_GDDR3:
        case AMDGPU_VRAM_TYPE_GDDR4:
            DD_ASSERT_REASON("Unexepcted memory type - GDDR1-4 are not supported by current drivers");
            break;

        case AMDGPU_VRAM_TYPE_DDR2: return LocalMemoryType::Ddr2; break;
        case AMDGPU_VRAM_TYPE_DDR3: return LocalMemoryType::Ddr3; break;
        case AMDGPU_VRAM_TYPE_DDR4: return LocalMemoryType::Ddr4; break;
	    case AMDGPU_VRAM_TYPE_DDR5: return LocalMemoryType::Ddr5; break;
	    case AMDGPU_VRAM_TYPE_GDDR5: return LocalMemoryType::Gddr5; break;
        case AMDGPU_VRAM_TYPE_GDDR6: return LocalMemoryType::Gddr6; break;
        case AMDGPU_VRAM_TYPE_HBM: return LocalMemoryType::Hbm; break;
        case AMDGPU_VRAM_TYPE_LPDDR5: return LocalMemoryType::Lpddr5; break;

        default: DD_ASSERT_REASON("Unrecognized memory type"); break;
    }

    return LocalMemoryType::Unknown;
}

uint32 DetermineNumberOfCus(const amdgpu_gpu_info& info)
{
    DD_ASSERT(info.num_shader_engines <= 4);
    DD_ASSERT(info.num_shader_arrays_per_engine <= 4);

    uint32 numCus = 0;
    for (uint32 shaderEngine = 0; shaderEngine < info.num_shader_engines; ++shaderEngine)
    {
        for (uint32 shaderArray = 0; shaderArray < info.num_shader_arrays_per_engine; ++shaderArray)
        {
            numCus += CountSetBits(info.cu_bitmap[shaderEngine][shaderArray]);
        }
    }

    return numCus;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Linux implementation of QueryGpuInfo, loads the dynamic library libdrm_amdgpu and calls functions to create a device
// handle per GPU on the system, then query the GPU info from it to populate our own AmdGpuInfo struct used by the info
// service.
Result QueryGpuInfo(const AllocCb& allocCb, Vector<AmdGpuInfo>* pGpus)
{
    DD_UNUSED(allocCb);
    Result result = Result::InvalidParameter;

    if (pGpus != nullptr)
    {
        const char* kAmdGpuLibraryName = "libdrm_amdgpu.so.1";
        Platform::Library libdrmLoader;
        result = libdrmLoader.Load(kAmdGpuLibraryName);

        if (result == Result::Success)
        {
            constexpr uint32 MaxDevices           = 16;
            drmDevicePtr     pDevices[MaxDevices] = {};
            int32            deviceCount          = 0;

            // Get the devices
            PFN_DrmGetDevices pfnGetDevices;
            if (libdrmLoader.GetFunction("drmGetDevices", &pfnGetDevices) && (pfnGetDevices != nullptr))
            {
                deviceCount = pfnGetDevices(pDevices, MaxDevices);
            }

            result = (deviceCount > 0) ? Result::Success : Result::Unavailable;

            for (uint32 i = 0; (i < static_cast<uint32>(deviceCount)) && (result == Result::Success); ++i)
            {
                AmdGpuInfo outGpuInfo = {};

                // Copy over the PCI data
                outGpuInfo.pci.bus      = pDevices[i]->businfo.pci->bus;
                outGpuInfo.pci.device   = pDevices[i]->businfo.pci->dev;
                outGpuInfo.pci.function = pDevices[i]->businfo.pci->func;

                // Open the amdgpu device file descriptor
                int32 renderFd  = open(pDevices[i]->nodes[DRM_NODE_RENDER], O_RDWR, 0);
                int32 primaryFd = open(pDevices[i]->nodes[DRM_NODE_PRIMARY], O_RDWR, 0);

                amdgpu_device_handle deviceHandle = NULL;
                uint32               majorVersion = 0;
                uint32               minorVersion = 0;

                // Initialize device
                if ((renderFd < 0) || (primaryFd < 0))
                {
                    result = Result::Rejected;
                }
                else
                {
                    // Initialize the amdgpu device.
                    PFN_AmdgpuDeviceInitialize pfnDeviceInitialize;
                    if (libdrmLoader.GetFunction("amdgpu_device_initialize", &pfnDeviceInitialize) &&
                        (pfnDeviceInitialize != nullptr))
                    {
                        result = (pfnDeviceInitialize(renderFd, &majorVersion, &minorVersion, &deviceHandle) == 0) ?
                                     Result::Success :
                                     Result::Error;
                    }
                }

                // Query GPU Info
                amdgpu_gpu_info gpuInfo = {};
                if (result == Result::Success)
                {
                    PFN_AmdgpuQueryGpuInfo pfnQueryGpuInfo;
                    if (libdrmLoader.GetFunction("amdgpu_query_gpu_info", &pfnQueryGpuInfo) &&
                        (pfnQueryGpuInfo != nullptr))
                    {
                        result = (pfnQueryGpuInfo(deviceHandle, &gpuInfo) == 0) ? Result::Success : Result::Error;
                    }
                }

                drm_amdgpu_memory_info memInfo = {};
                if (result == Result::Success)
                {
                    // Now translate to our own AmdGpuInfo struct
                    outGpuInfo.asic.ids.deviceId   = gpuInfo.asic_id;
                    outGpuInfo.asic.ids.eRevId     = gpuInfo.chip_external_rev;
                    outGpuInfo.asic.ids.revisionId = gpuInfo.pci_rev_id;
                    outGpuInfo.asic.ids.family     = gpuInfo.family_id;
                    // amdgpu reports this in KHz, we store it as Hz
                    outGpuInfo.engineClocks.max = gpuInfo.max_engine_clk * 1000;

                    outGpuInfo.asic.gpuIndex       = i;
                    outGpuInfo.asic.gpuCounterFreq = gpuInfo.gpu_counter_freq * 1000;
                    outGpuInfo.asic.numCus = DetermineNumberOfCus(gpuInfo);

                    outGpuInfo.memory.type           = TranslateMemoryType(gpuInfo.vram_type);
                    outGpuInfo.memory.memOpsPerClock = MemoryOpsPerClock(outGpuInfo.memory.type);
                    outGpuInfo.memory.busBitWidth    = gpuInfo.vram_bit_width;
                    // amdgpu reports this in KHz, we store it as Hz
                    outGpuInfo.memory.clocksHz.max = gpuInfo.max_memory_clk * 1000;

                    outGpuInfo.memory.hbccSize = 0; // Per PAL - Linux doesn't support HBCC

                    // drm version info
                    outGpuInfo.drmVersion.Major = majorVersion;
                    outGpuInfo.drmVersion.Minor = minorVersion;

                    // Get the marketing name string
                    PFN_AmdgpuGetMarketingName pfnGetMarketingName;
                    if (libdrmLoader.GetFunction("amdgpu_get_marketing_name", &pfnGetMarketingName) &&
                        (pfnGetMarketingName != nullptr))
                    {
                        const char* pMarketingName = pfnGetMarketingName(deviceHandle);
                        if (pMarketingName != nullptr)
                        {
                            Platform::Strncpy(outGpuInfo.name, pMarketingName);
                        }
                    }

                    // Query additional memory info
                    PFN_AmdgpuQueryInfo pfnQueryInfo;
                    if (libdrmLoader.GetFunction("amdgpu_query_info", &pfnQueryInfo) &&
                        (pfnQueryInfo != nullptr))
                    {
                        if (pfnQueryInfo(deviceHandle, AMDGPU_INFO_MEMORY, sizeof(memInfo), &memInfo) != 0)
                        {
                            struct amdgpu_heap_info heap_info = {};
                            PFN_AmdgpuQueryHeapInfo pfnAmdgpuQueryHeapInfo;
                            if (libdrmLoader.GetFunction("amdgpu_query_heap_info", &pfnAmdgpuQueryHeapInfo) &&
                                (pfnAmdgpuQueryHeapInfo != nullptr))
                            {
                                if (pfnAmdgpuQueryHeapInfo(
                                        deviceHandle,
                                        AMDGPU_GEM_DOMAIN_VRAM,
                                        AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED,
                                        &heap_info) == 0)
                                {
                                    outGpuInfo.memory.localHeap.size = heap_info.heap_size;
                                }

                                if (pfnAmdgpuQueryHeapInfo(deviceHandle, AMDGPU_GEM_DOMAIN_VRAM, 0, &heap_info) == 0)
                                {
                                    outGpuInfo.memory.invisibleHeap.size = heap_info.heap_size;
                                }
                            }
                        }
                        else
                        {
                            outGpuInfo.memory.localHeap.size     = memInfo.cpu_accessible_vram.total_heap_size;
                            outGpuInfo.memory.invisibleHeap.size =
                                memInfo.vram.total_heap_size - outGpuInfo.memory.localHeap.size;

                            // Currently libdrm doesn't provide base physical addresses. We just assume that
                            // the base address of the local visible memory region starts at 0, and the invisible
                            // memory region follows immediately after, and set their base addresses accordingly.
                            // See issue #361.
                            outGpuInfo.memory.localHeap.physAddr = 0;
                            outGpuInfo.memory.invisibleHeap.physAddr = outGpuInfo.memory.localHeap.size;
                        }
                    }
                    pGpus->PushBack(outGpuInfo);
                }

                // Deinitialize device
                PFN_AmdgpuDeviceDeinitialize pfnDeviceDeinitialize;
                if (libdrmLoader.GetFunction("amdgpu_device_deinitialize", &pfnDeviceDeinitialize) &&
                    (pfnDeviceDeinitialize != nullptr))
                {
                    pfnDeviceDeinitialize(deviceHandle);
                }
            }

            libdrmLoader.Close();
        }
        else
        {
            result = Result::FileNotFound;
        }
    }

    return result;
}
} // namespace DevDriver
