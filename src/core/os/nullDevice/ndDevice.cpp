/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/os/nullDevice/ndDevice.h"
#include "core/os/nullDevice/ndFence.h"
#include "core/os/nullDevice/ndGpuMemory.h"
#include "core/os/nullDevice/ndPlatform.h"
#include "core/os/nullDevice/ndQueue.h"
#include "palFormatInfo.h"
#include "palSysMemory.h"

#include "core/os/lnx/lnxHeaders.h"

#include <limits.h>
#include "core/hw/amdgpu_asic.h"

using namespace Util;

namespace Pal
{
namespace NullDevice
{

#define PAL_UNDEFINED_NULL_DEVICE  FAMILY_UNKNOWN, 0, 0, CIASICIDGFXENGINE_UNKNOWN, 0

// Identification table for all the GPUs that are supported in NULL device mode
constexpr  NullIdLookup  NullIdLookupTable[]=
{
    { FAMILY_SI,      SI_TAHITI_P_A21,   PRID_SI_TAHITI,            CIASICIDGFXENGINE_SOUTHERNISLAND, DEVICE_ID_SI_TAHITI_P_6780  },
    { FAMILY_SI,      SI_HAINAN_V_A0,    PRID_SI_HAINAN_EXO_81,     CIASICIDGFXENGINE_SOUTHERNISLAND, DEVICE_ID_SI_HAINAN_V_6660  },
    { FAMILY_CI,      CI_BONAIRE_M_A0,   PRID_CI_BONAIRE_TOBAGO_81, CIASICIDGFXENGINE_SOUTHERNISLAND, DEVICE_ID_CI_BONAIRE_M_6640 },
    { FAMILY_CI,      CI_HAWAII_P_A0,    PRID_CI_HAWAII_80,         CIASICIDGFXENGINE_SOUTHERNISLAND, DEVICE_ID_CI_HAWAII_P_67BE  },
    { FAMILY_KV,      KV_KALINDI_A0,     PRID_KV_KALINDI_00,        CIASICIDGFXENGINE_SOUTHERNISLAND, DEVICE_ID_KALINDI__9830     },
    { FAMILY_KV,      KV_GODAVARI_A0,    PRID_GODAVARI_MULLINS_01,  CIASICIDGFXENGINE_SOUTHERNISLAND, DEVICE_ID_KV_GODAVARI__9850 },
    { FAMILY_VI,      VI_ICELAND_M_A0,   PRID_VI_ICELAND_MESO_81,   CIASICIDGFXENGINE_SOUTHERNISLAND, DEVICE_ID_VI_ICELAND_M_6900 },
    { FAMILY_CZ,      CZ_CARRIZO_A0,     PRID_CZ_CARRIZO_C4,        CIASICIDGFXENGINE_SOUTHERNISLAND, DEVICE_ID_CZ_CARRIZO_9870   },
    { FAMILY_VI,      VI_TONGA_P_A1,     PRID_VI_TONGA_00,          CIASICIDGFXENGINE_SOUTHERNISLAND, DEVICE_ID_VI_TONGA_P_6920   },
    { FAMILY_VI,      VI_FIJI_P_A0,      PRID_VI_FIJI_CC,           CIASICIDGFXENGINE_SOUTHERNISLAND, DEVICE_ID_VI_FIJI_P_7300    },
    { FAMILY_CZ,      CZ_STONEY_A0,      PRID_ST_80,                CIASICIDGFXENGINE_SOUTHERNISLAND, DEVICE_ID_ST_98E4           },
#if PAL_BUILD_GFX9
    { FAMILY_AI,      AI_VEGA10_P_A0,    PRID_AI_VEGA10_C3,         CIASICIDGFXENGINE_ARCTICISLAND,   DEVICE_ID_AI_VEGA10_P_6860  },
#else
    { PAL_UNDEFINED_NULL_DEVICE                                                                                                   },
#endif
    { FAMILY_RV,      RAVEN_A0,          PRID_RV_81,                CIASICIDGFXENGINE_ARCTICISLAND,   DEVICE_ID_RV_15DD           },
#if PAL_BUILD_GFX9
    { FAMILY_AI,      AI_VEGA12_P_A0,    PRID_AI_VEGA12_00,         CIASICIDGFXENGINE_ARCTICISLAND,   DEVICE_ID_AI_VEGA12_P_69A0  },
#else
    { PAL_UNDEFINED_NULL_DEVICE                                                                                                   },
#endif
    { PAL_UNDEFINED_NULL_DEVICE                                                                                                   },
    { PAL_UNDEFINED_NULL_DEVICE                                                                                                   },
    { PAL_UNDEFINED_NULL_DEVICE                                                                                                   },
};

const char* pNullGpuNames[static_cast<uint32>(Pal::NullGpuId::Max)] =
{
    "TAHITI",
    "HAINAN",
    "BONAIRE",
    "HAWAII",
    "KALINDI",
    "GODAVARI",
    "ICELAND",
    "CARRIZO",
    "TONGA",
    "FIJI",
    "STONEY",
#if PAL_BUILD_GFX9
    "VEGA10",
#else
    nullptr,
#endif
    "RAVEN",
#if PAL_BUILD_GFX9
    "VEGA12",
#else
    nullptr,
#endif
    nullptr,
    nullptr,
};

// =====================================================================================================================
Device::Device(
    Platform*              pPlatform,
    const char*            pName,
    const NullIdLookup&    nullIdLookup,
    const HwIpDeviceSizes& hwDeviceSizes)
    :
    Pal::Device(pPlatform,
                0, // deviceIndex, we only have one
                1, // always one screen attached to a screen
                sizeof(Device),
                hwDeviceSizes,
                UINT_MAX), // max semaphore count
    m_nullIdLookup(nullIdLookup)
{
    Strncpy(&m_gpuName[0], pName, sizeof(m_gpuName));
}

// =====================================================================================================================
// Factory function for creating Device objects. Creates a new Windows::Device object if the GPU is supported by
// the PAL library.
Result Device::Create(
    Platform*  pPlatform,
    Device**   ppDeviceOut,
    NullGpuId  nullGpuId)
{
    const auto&  nullIdLookup = NullIdLookupTable[static_cast<uint32>(nullGpuId)];
    const char*  pName        = pNullGpuNames[static_cast<uint32>(nullGpuId)];
    Result       result       = Result::ErrorInitializationFailed;

    // Determine if the GPU is supported by PAL, and if so, what its hardware IP levels are.
    HwIpLevels ipLevels = {};
    if (Pal::Device::DetermineGpuIpLevels(nullIdLookup.familyId,
                                          nullIdLookup.eRevId,
                                          UINT_MAX, // microcode version, we just want to be over the min-supported ver
                                          &ipLevels))
    {
        const size_t deviceSize = sizeof(Device);

        size_t          addrMgrSize   = 0;
        HwIpDeviceSizes hwDeviceSizes = {};
        GetHwIpDeviceSizes(ipLevels, &hwDeviceSizes, &addrMgrSize);
        const size_t  neededMemSize = deviceSize          +
                                      hwDeviceSizes.gfx   +
                                      hwDeviceSizes.oss   +
                                      addrMgrSize;
        void* pMemory = PAL_MALLOC(neededMemSize, pPlatform, Util::AllocInternal);

        if (pMemory != nullptr)
        {
            (*ppDeviceOut) = PAL_PLACEMENT_NEW(pMemory) Device(pPlatform,
                                                               pName,
                                                               nullIdLookup,
                                                               hwDeviceSizes);

            result = (*ppDeviceOut)->EarlyInit(ipLevels);
            if (result != Result::Success)
            {
                (*ppDeviceOut)->~Device();
                PAL_SAFE_FREE(pMemory, pPlatform);
            }
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    return result;
}

// =====================================================================================================================
// Queries whether a given NullGpuId corresponds to a valid device.
bool Device::IsValid(
    NullGpuId nullGpuId)
{
    const auto&  nullIdLookup = NullIdLookupTable[static_cast<uint32>(nullGpuId)];
    const char*  pName        = pNullGpuNames[static_cast<uint32>(nullGpuId)];

    return ((nullGpuId                <  NullGpuId::Max)            &&
            (pName                    != nullptr)                   &&
            (nullIdLookup.familyId    != FAMILY_UNKNOWN)            &&
            (nullIdLookup.gfxEngineId != CIASICIDGFXENGINE_UNKNOWN) &&
            (nullIdLookup.deviceId    != 0)                         &&
            (nullIdLookup.eRevId      != 0));
}

// =====================================================================================================================
Result Device::AddEmulatedPrivateScreen(
    const PrivateScreenCreateInfo& createInfo,
    uint32*                        pTargetId)
{
    return Result::Success;
}

// =====================================================================================================================
Result Device::AddGpuMemoryReferences(
    uint32              gpuMemRefCount,
    const GpuMemoryRef* pGpuMemoryRefs,
    IQueue*             pQueue,
    uint32              flags
    )
{
    // We're not going to be submitting anything, so there's no need to add memory references
    return Result::Success;
}

// =====================================================================================================================
// Allocates GDS for individual engines.
Result Device::AllocateGds(
    const DeviceGdsAllocInfo&   requested,
    DeviceGdsAllocInfo*         pAllocated)
{
    return Result::Success;
}

// =====================================================================================================================
Result Device::AssignVirtualAddress(
    const Pal::GpuMemory&  gpuMemory,
    gpusize*               pGpuVirtAddr,
    VaPartition            vaPartition)
{
    const GpuMemoryProperties& gpuMemProps = MemoryProperties();

    *pGpuVirtAddr = gpuMemProps.vaRange[static_cast<uint32>(vaPartition)].baseVirtAddr;

    return Result::Success;
}

// =====================================================================================================================
// Correlates a current GPU timsetamp with the CPU clock, allowing tighter CPU/GPU synchronization using timestamps.
Result Device::CalibrateGpuTimestamp(
    GpuTimestampCalibration* pCalibrationData
    ) const
{
    PAL_NOT_IMPLEMENTED();

    return Result::Success;
}

// =====================================================================================================================
Result Device::Cleanup()
{
    return Result::Success;
}

// =====================================================================================================================
Pal::GpuMemory* Device::ConstructGpuMemoryObject(
    void* pPlacementAddr)
{
    return PAL_PLACEMENT_NEW(pPlacementAddr) NdGpuMemory(this);
}

// =====================================================================================================================
Pal::Queue* Device::ConstructQueueObject(
    const QueueCreateInfo& createInfo,
    void*                  pPlacementAddr)
{
    Pal::Queue* pQueue = PAL_PLACEMENT_NEW(pPlacementAddr) Queue(this, createInfo);

    return pQueue;
}

// =====================================================================================================================
// Creates and initializes a new Image object.
Result Device::CreateImage(
    const ImageCreateInfo& createInfo,
    void*                  pPlacementAddr,
    IImage**               ppImage)
{
    return Result::Unsupported;
}

// =====================================================================================================================
// Creates and initializes a new Image object.
Result Device::CreateInternalImage(
    const ImageCreateInfo&         createInfo,
    const ImageInternalCreateInfo& internalCreateInfo,
    void*                          pPlacementAddr,
    Pal::Image**                   ppImage)
{
    // The GetImageSize() function should prevent us from getting here
    PAL_NEVER_CALLED();

    return Result::Unsupported;
}

// =====================================================================================================================
Result Device::CreatePresentableImage(
    const PresentableImageCreateInfo& createInfo,
    void*                             pImagePlacementAddr,
    void*                             pGpuMemoryPlacementAddr,
    IImage**                          ppImage,
    IGpuMemory**                      ppGpuMemory)
{
    // Don't expect to ever get here based on the implementation (or lack thereof) of GetPresentableImageSizes()
    PAL_NEVER_CALLED();

    return Result::Unsupported;
}

// =====================================================================================================================
// Determines the size in bytes of a Fence object.
size_t Device::GetFenceSize(
    Result* pResult
    ) const
{
    if (pResult != nullptr)
    {
        (*pResult) = Result::Success;
    }

    return sizeof(Fence);
}

// =====================================================================================================================
// Creates a new Fence object in preallocated memory provided by the caller.
Result Device::CreateFence(
    const FenceCreateInfo& createInfo,
    void*                  pPlacementAddr,
    IFence**               ppFence
    ) const
{
    PAL_ASSERT((pPlacementAddr != nullptr) && (ppFence != nullptr));

    Fence* pFence = PAL_PLACEMENT_NEW(pPlacementAddr) Fence();

    // Set needsEvent argument to true - all client-created fences require event objects to support the
    // IDevice::WaitForFences interface.
    Result result = pFence->Init(createInfo);

    if (result != Result::Success)
    {
        pFence->Destroy();
        pFence = nullptr;
    }

    (*ppFence) = pFence;

    return result;
}

// =====================================================================================================================
// Open/Reconstruct the pFence from a handle or a name.
Result Device::OpenFence(
    const FenceOpenInfo& openInfo,
    void*                pPlacementAddr,
    IFence**             ppFence
    ) const
{
    PAL_ASSERT((pPlacementAddr != nullptr) && (ppFence != nullptr));

    Fence* pFence = PAL_PLACEMENT_NEW(pPlacementAddr) Fence();

    Result result = pFence->OpenHandle(openInfo);

    if (result != Result::Success)
    {
        pFence->Destroy();
        pFence = nullptr;
    }

    (*ppFence) = pFence;

    return result;
}

// =====================================================================================================================
// Create swap chain based on local window system.
Result Device::CreateSwapChain(
    const SwapChainCreateInfo&  createInfo,
    void*                       pPlacementAddr,
    ISwapChain**                ppSwapChain)
{
    // GetSwapChainSize() implementation (or lack thereof) should prevent us from ever getting here
    PAL_NEVER_CALLED();

    return Result::Unsupported;
}

// =====================================================================================================================
// This would require having multiple devices
Result Device::DetermineExternalSharedResourceType(
    const ExternalResourceOpenInfo& openInfo,
    bool*                           pIsImage
    ) const
{
    return Result::Unsupported;
}

#if PAL_BUILD_GFX6
// =====================================================================================================================
// Helper method which initializes the GPU chip properties for all hardware families using the GFX6 hardware layer.
void Device::InitGfx6ChipProperties()
{
    auto*const  pChipInfo  = &m_chipProperties.gfx6;

    // Call into the HWL to initialize the default values for many properties of the hardware (based on chip ID).
    Gfx6::InitializeGpuChipProperties(UINT_MAX, &m_chipProperties);

    if (AMDGPU_IS_TAHITI(m_nullIdLookup.familyId, m_nullIdLookup.eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001;
        pChipInfo->gbAddrConfig            = 0x02011003;
        pChipInfo->paScRasterCfg           = 0x0D0D0DCD;
        pChipInfo->paScRasterCfg1          = 0x0000000D;
        pChipInfo->numShaderEngines        =    2; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    2; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    4; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->wavefrontSize           =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numShaderVisibleVgprs   =  256; // tahiti__GPU__SP__NUM_GPRS;
        pChipInfo->numCuPerSh              =    8; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =   12; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       = 1792; // tahiti__GPU__VGT__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_HAINAN(m_nullIdLookup.familyId, m_nullIdLookup.eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001;
        pChipInfo->gbAddrConfig            = 0x02010001;
        pChipInfo->paScRasterCfg           = 0x0D0D0DCD;
        pChipInfo->paScRasterCfg1          = 0x0000000D;
        pChipInfo->numShaderEngines        =    1; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    1; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->wavefrontSize           =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numShaderVisibleVgprs   =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->numCuPerSh              =    5; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =    2; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   16; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       =  768; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   16; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_BONAIRE(m_nullIdLookup.familyId, m_nullIdLookup.eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001;
        pChipInfo->gbAddrConfig            = 0x02011002;
        pChipInfo->paScRasterCfg           = 0x0D0D0DCD;
        pChipInfo->paScRasterCfg1          = 0x0000000D;
        pChipInfo->numShaderEngines        =    2; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    2; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->wavefrontSize           =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numShaderVisibleVgprs   =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->numCuPerSh              =    7; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =    4; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       = 1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_HAWAII(m_nullIdLookup.familyId, m_nullIdLookup.eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001;
        pChipInfo->gbAddrConfig            = 0x02011003;
        pChipInfo->paScRasterCfg           = 0x0D0DCDCD;
        pChipInfo->paScRasterCfg1          = 0x0000000D;
        pChipInfo->numShaderEngines        =    4; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    4; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->wavefrontSize           =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numShaderVisibleVgprs   =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->numCuPerSh              =   11; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =   16; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       = 1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_GODAVARI(m_nullIdLookup.familyId, m_nullIdLookup.eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001;
        pChipInfo->gbAddrConfig            = 0x02010001;
        pChipInfo->paScRasterCfg           = 0x0D0D0DCD;
        pChipInfo->paScRasterCfg1          = 0x0000000D;
        pChipInfo->numShaderEngines        =    1; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    2; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->wavefrontSize           =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numShaderVisibleVgprs   =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->numCuPerSh              =    2; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =    2; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   16; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       =  256; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   16; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_KALINDI(m_nullIdLookup.familyId, m_nullIdLookup.eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001;
        pChipInfo->gbAddrConfig            = 0x02010001;
        pChipInfo->paScRasterCfg           = 0x0D0D0DCD;
        pChipInfo->paScRasterCfg1          = 0x0000000D;
        pChipInfo->numShaderEngines        =    1; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    1; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->wavefrontSize           =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numShaderVisibleVgprs   =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->numCuPerSh              =    2; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =    2; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   16; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       =  256; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   16; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_ICELAND(m_nullIdLookup.familyId, m_nullIdLookup.eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001;
        pChipInfo->gbAddrConfig            = 0x22010001;
        pChipInfo->paScRasterCfg           = 0x0D0DCDCD;
        pChipInfo->paScRasterCfg1          = 0x0000000D;
        pChipInfo->numShaderEngines        =    1; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    2; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->wavefrontSize           =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numShaderVisibleVgprs   =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->numCuPerSh              =    6; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =    2; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   16; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       =  768; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   16; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_CARRIZO(m_nullIdLookup.familyId, m_nullIdLookup.eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001;
        pChipInfo->gbAddrConfig            = 0x22011003;
        pChipInfo->paScRasterCfg           = 0x0D0DCDCD;
        pChipInfo->paScRasterCfg1          = 0x0000000D;
        pChipInfo->numShaderEngines        =    1; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    2; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->wavefrontSize           =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numShaderVisibleVgprs   =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->numCuPerSh              =    8; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =    4; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   16; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       =  768; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   16; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_TONGA(m_nullIdLookup.familyId, m_nullIdLookup.eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001;
        pChipInfo->gbAddrConfig            = 0x22011003;
        pChipInfo->paScRasterCfg           = 0x0D0DCDCD;
        pChipInfo->paScRasterCfg1          = 0x0000000D;
        pChipInfo->numShaderEngines        =    4; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    2; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->wavefrontSize           =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numShaderVisibleVgprs   =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->numCuPerSh              =    8; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =   12; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       = 1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_FIJI(m_nullIdLookup.familyId, m_nullIdLookup.eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001;
        pChipInfo->gbAddrConfig            = 0x22011003;
        pChipInfo->paScRasterCfg           = 0x0D0DCDCD;
        pChipInfo->paScRasterCfg1          = 0x0000000D;
        pChipInfo->numShaderEngines        =    4; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    4; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->wavefrontSize           =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numShaderVisibleVgprs   =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->numCuPerSh              =   16; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =   16; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       = 1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_STONEY(m_nullIdLookup.familyId, m_nullIdLookup.eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001;
        pChipInfo->gbAddrConfig            = 0x22010001;
        pChipInfo->paScRasterCfg           = 0x0D0DCDCD;
        pChipInfo->paScRasterCfg1          = 0x0000000D;
        pChipInfo->numShaderEngines        =    1; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    2; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->wavefrontSize           =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numShaderVisibleVgprs   =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->numCuPerSh              =    3; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =    2; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   16; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       =  256; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   16; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else
    {
        // Unknown device id
        PAL_ASSERT_ALWAYS();
    }

    pChipInfo->backendDisableMask      = 0; // everything is enabled!
    pChipInfo->numActiveRbs            = pChipInfo->maxNumRbPerSe * pChipInfo->numShaderEngines;
    pChipInfo->gbTileMode[TILEINDEX_LINEAR_ALIGNED] = ADDR_TM_LINEAR_ALIGNED << 2;
    PAL_ASSERT(m_chipProperties.gfxLevel >= GfxIpLevel::GfxIp6);
    const uint32  activeCuMask = (1 << pChipInfo->numCuPerSh) - 1;

    // GFXIP 7+ hardware only has one shader array per shader engine!
    PAL_ASSERT(m_chipProperties.gfxLevel < GfxIpLevel::GfxIp7 || pChipInfo->numShaderArrays == 1);

    for (uint32 seIndex = 0; seIndex < pChipInfo->numShaderEngines; seIndex++)
    {
        for (uint32 shIndex = 0; shIndex < pChipInfo->numShaderArrays; shIndex++)
        {
            if (m_chipProperties.gfxLevel == GfxIpLevel::GfxIp6)
            {
                pChipInfo->activeCuMaskGfx6[seIndex][shIndex]   = activeCuMask;
                pChipInfo->alwaysOnCuMaskGfx6[seIndex][shIndex] = activeCuMask;
            }
            else
            {
                pChipInfo->activeCuMaskGfx7[seIndex]   = activeCuMask;
                pChipInfo->alwaysOnCuMaskGfx7[seIndex] = activeCuMask;
            }
        }
    }

    // Call into the HWL to finish initializing some GPU properties which can be derived from the ones which we
    // overrode above.
    Gfx6::FinalizeGpuChipProperties(&m_chipProperties);
}
#endif

#if PAL_BUILD_GFX9
// =====================================================================================================================
// Helper method which initializes the GPU chip properties for all hardware families using the GFX9 hardware layer.
void Device::InitGfx9ChipProperties()
{
    auto*const  pChipInfo  = &m_chipProperties.gfx9;

    // Call into the HWL to initialize the default values for many properties of the hardware (based on chip ID).
    Gfx9::InitializeGpuChipProperties(UINT_MAX, &m_chipProperties);

    if (AMDGPU_IS_VEGA10(m_nullIdLookup.familyId, m_nullIdLookup.eRevId))
    {
        // NOTE: KMD only gives us a flag indicating whether the Off-chip LDS buffers are "large" or not. The HWL will
        // need to determine the actual LDS buffer size based on this flag.
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->gbAddrConfig            = 0x2A110002;
        pChipInfo->numShaderEngines        =    4; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    4; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->wavefrontSize           =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numShaderVisibleVgprs   =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->numCuPerSh              =   16; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =   16; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       = 1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_VEGA12(m_nullIdLookup.familyId, m_nullIdLookup.eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->gbAddrConfig            = 0x26110001;
        pChipInfo->numShaderEngines        =    4; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    2; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->wavefrontSize           =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numShaderVisibleVgprs   =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->numCuPerSh              =    5; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =    8; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       = 1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_RAVEN(m_nullIdLookup.familyId, m_nullIdLookup.eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->gbAddrConfig            = 0x26010001;
        pChipInfo->numShaderEngines        =    1;  // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    2; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->wavefrontSize           =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numShaderVisibleVgprs   =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->numCuPerSh              =   11; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =    4; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       = 1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else
    {
        // Unknown device id
        PAL_ASSERT_ALWAYS();
    }

    pChipInfo->backendDisableMask = 0; // everything is enabled!
    pChipInfo->numActiveRbs       = pChipInfo->maxNumRbPerSe * pChipInfo->numShaderEngines;

    const uint32  activeCuMask = (1 << pChipInfo->numCuPerSh) - 1;
    for (uint32 shIndex = 0; shIndex < pChipInfo->numShaderArrays; shIndex++)
    {
        for (uint32 seIndex = 0; seIndex < pChipInfo->numShaderEngines; seIndex++)
        {
            pChipInfo->activeCuMask[shIndex][seIndex]   = activeCuMask;
            pChipInfo->alwaysOnCuMask[shIndex][seIndex] = activeCuMask;
        }
    }

    // Call into the HWL to finish initializing some GPU properties which can be derived from the ones which we
    // overrode above.
    Gfx9::FinalizeGpuChipProperties(&m_chipProperties);
}
#endif

// =====================================================================================================================
Result Device::EarlyInit(
    const HwIpLevels& ipLevels)
{
    Result result = Result::Success;

    m_chipProperties.familyId    = m_nullIdLookup.familyId;
    m_chipProperties.deviceId    = m_nullIdLookup.deviceId;
    m_chipProperties.eRevId      = m_nullIdLookup.eRevId;
    m_chipProperties.revisionId  = m_nullIdLookup.revisionId;
    m_chipProperties.gfxEngineId = m_nullIdLookup.gfxEngineId;
    m_chipProperties.gpuIndex    = 0;

    m_chipProperties.gfxLevel = ipLevels.gfx;
    m_chipProperties.ossLevel = ipLevels.oss;
    m_chipProperties.vceLevel = ipLevels.vce;
    m_chipProperties.uvdLevel = ipLevels.uvd;

    for (uint32 i = 0; i < EngineTypeCount; i++)
    {
        m_engineProperties.perEngine[i].preferredCmdAllocHeaps[CommandDataAlloc]   = GpuHeapGartUswc;
        m_engineProperties.perEngine[i].preferredCmdAllocHeaps[EmbeddedDataAlloc]  = GpuHeapGartUswc;
        m_engineProperties.perEngine[i].preferredCmdAllocHeaps[GpuScratchMemAlloc] = GpuHeapInvisible;
    }

    switch (m_chipProperties.gfxLevel)
    {
#if PAL_BUILD_GFX6
    case GfxIpLevel::GfxIp6:
    case GfxIpLevel::GfxIp7:
    case GfxIpLevel::GfxIp8:
    case GfxIpLevel::GfxIp8_1:
        m_pFormatPropertiesTable    = Gfx6::GetFormatPropertiesTable(m_chipProperties.gfxLevel);

        InitGfx6ChipProperties();
        Gfx6::InitializeGpuEngineProperties(m_chipProperties.gfxLevel,
                                            m_chipProperties.familyId,
                                            m_chipProperties.eRevId,
                                            &m_engineProperties);
        break;
#endif
#if PAL_BUILD_GFX9
    case GfxIpLevel::GfxIp9:
        m_pFormatPropertiesTable    = Gfx9::GetFormatPropertiesTable(m_chipProperties.gfxLevel);

        InitGfx9ChipProperties();
        Gfx9::InitializeGpuEngineProperties(m_chipProperties.gfxLevel,
                                            m_chipProperties.familyId,
                                            m_chipProperties.eRevId,
                                            &m_engineProperties);
        break;
#endif
    case GfxIpLevel::None:
        // No Graphics IP block found or recognized!
    default:
        break;
    }

    switch (m_chipProperties.ossLevel)
    {
#if PAL_BUILD_OSS1
    case OssIpLevel::OssIp1:
        Oss1::InitializeGpuEngineProperties(&m_engineProperties);
        break;
#endif
#if PAL_BUILD_OSS2
    case OssIpLevel::OssIp2:
        Oss2::InitializeGpuEngineProperties(&m_engineProperties);
        break;
#endif
#if PAL_BUILD_OSS2_4
    case OssIpLevel::OssIp2_4:
        Oss2_4::InitializeGpuEngineProperties(&m_engineProperties);
        break;
#endif
#if PAL_BUILD_OSS4
    case OssIpLevel::OssIp4:
        Oss4::InitializeGpuEngineProperties(&m_engineProperties);
        break;
#endif
    case OssIpLevel::None:
        // No OSS IP block found or recognized!
    default:
        break;
    }

    if (result == Result::Success)
    {
        result = InitMemoryProperties();

        if (result == Result::Success)
        {
            InitMemoryHeapProperties();
        }
    }

    // Init paths
    InitOutputPaths();

    if (result == Result::Success)
    {
        result = InitSettings();
    }

    if (result == Result::Success)
    {
        // The base class assumes the chip properties have been initialized so it must be called last.
        result = Pal::Device::EarlyInit(ipLevels);
    }

    return result;
}

// =====================================================================================================================
// Enumerate private screen info.  We don't have HW so there are no screens to enumerate.
Result Device::EnumPrivateScreensInfo(
    uint32* pNumScreens)
{
    return Result::Success;
}

// =====================================================================================================================
// Peforms extra initialization which needs to be done when the client is ready to start using the device.
Result Device::Finalize(
    const DeviceFinalizeInfo& finalizeInfo)
{
    return Pal::Device::Finalize(finalizeInfo);
}

// =====================================================================================================================
// Helper method which finalizes some of the Queue properties which cannot be determined until the settings are read.
void Device::FinalizeQueueProperties()
{
    m_queueProperties.maxNumCmdStreamsPerSubmit     = MinCmdStreamsPerSubmission;
    m_engineProperties.maxInternalRefsPerSubmission = InternalMemMgrAllocLimit;
    m_engineProperties.maxUserMemRefsPerSubmission  = (CmdBufMemReferenceLimit -
                                                       m_engineProperties.maxInternalRefsPerSubmission);

    // We don't support any presents in null-device mode
    for (uint32 idx = 0; idx < EngineTypeCount; ++idx)
    {
        auto*  pPerEngine = &m_engineProperties.perEngine[idx];

        // No GPU engines are supported by the null device.
        pPerEngine->numAvailable          = 0;
        pPerEngine->sizeAlignInDwords     = 1;
        pPerEngine->startAlign            = 1;
        pPerEngine->availableCeRamSize    = 48 * 1024; // 48kB
    }
}

// =====================================================================================================================
// Get the image size, memory size and the create info of image from an external shared image
Result Device::GetExternalSharedImageSizes(
    const ExternalImageOpenInfo& openInfo,
    size_t*                      pImageSize,
    size_t*                      pGpuMemorySize,
    ImageCreateInfo*             pImgCreateInfo
    ) const
{
    return Result::Unsupported;
}

// =====================================================================================================================
// Determines the size in bytes of a Image object.
size_t Device::GetImageSize(
    const ImageCreateInfo& createInfo,
    Result*                pResult
    ) const
{
    // There's no need for an image object on a device designed only to support off-line shader compilations
    *pResult = Result::Unsupported;

    return 0;
}

// =====================================================================================================================
// Compares this device against another device to determine how compatible they are for multi-GPU operations.
// NOTE: Part of the IDevice public interface.
Result Device::GetMultiGpuCompatibility(
    const IDevice&        otherDevice,
    GpuCompatibilityInfo* pInfo
    ) const
{
    // If this gets called, something very weird is happening as we don't have multi-gpus...
    PAL_NEVER_CALLED();

    return Result::Success;
}

// =====================================================================================================================
void Device::GetPresentableImageSizes(
    const PresentableImageCreateInfo& createInfo,
    size_t*                           pImageSize,
    size_t*                           pGpuMemorySize,
    Result*                           pResult
    ) const
{
    // We're not going to present anything, so this shouldn't need to do anything.
    *pImageSize     = 0;
    *pGpuMemorySize = 0;
    *pResult        = Result::Unsupported;
}

// =====================================================================================================================
// Retrieves info about the primary surface, except we don't have one.
Result Device::GetPrimaryInfo(
    const GetPrimaryInfoInput&  primaryInfoInput,
    GetPrimaryInfoOutput*       pPrimaryInfoOutput
    ) const
{
    PAL_NOT_IMPLEMENTED();

    return Result::Success;
}

// =====================================================================================================================
// Retrieves stereo display modes, except we don't have one.
Result Device::GetStereoDisplayModes(
    uint32*                   pStereoModeCount,
    StereoDisplayModeOutput*  pStereoModeList) const
{
    PAL_NOT_IMPLEMENTED();

    return Result::Success;
}

// =====================================================================================================================
// Retrieves active 10-Bit and packed pixel modes, except we don't have one.
Result Device::GetActive10BitPackedPixelMode(
    Active10BitPackedPixelModeOutput*  pMode) const
{
    PAL_NOT_IMPLEMENTED();

    return Result::Success;
}

// =====================================================================================================================
// Inform KMD that the present blt dst surface must be reinterpreted as 10-bits per channel, except we don't have one.
Result Device::RequestKmdReinterpretAs10Bit(
    const IGpuMemory* pGpuMemory) const
{
    PAL_NOT_IMPLEMENTED();

    return Result::Success;
}

// =====================================================================================================================
// Get swap chain information for swap chain creation.
Result Device::GetSwapChainInfo(
    OsDisplayHandle      hDisplay,
    OsWindowHandle       hWindow,
    WsiPlatform          wsiPlatform,
    SwapChainProperties* pSwapChainProperties)
{
    // This might actually have to fill in the pSwapChaiProperties structure?
    PAL_NOT_IMPLEMENTED();

    return Result::Success;
}

// =====================================================================================================================
// Don't allow creation of swap chains
size_t Device::GetSwapChainSize(
    const SwapChainCreateInfo& createInfo,
    Result*                    pResult
    ) const
{
    *pResult = Result::Unsupported;

    return 0;
}

// =====================================================================================================================
// Helper function to Call KMD to get XDMA cache buffer info.
Result Device::GetXdmaInfo(
    uint32              vidPnSrcId,
    const IGpuMemory&   gpuMemory,
    GetXdmaInfoOutput*  pGetXdmaInfoOutput
    ) const
{
    return Result::Success;
}

// =====================================================================================================================
size_t Device::GpuMemoryObjectSize() const
{
    return sizeof(NdGpuMemory);
}

// =====================================================================================================================
// Helper method which initializes the GPU memory properties.
Result Device::InitMemoryProperties()
{
    m_memoryProperties.vaStart      = 0;
    m_memoryProperties.vaEnd        = (1ull << MinVaRangeNumBits) - 1;
    m_memoryProperties.vaInitialEnd = m_memoryProperties.vaEnd;
    m_memoryProperties.vaUsableEnd  = m_memoryProperties.vaEnd;

    // PAL uses VAM to allocate GPU virtual addresses; VAM itself requires a 4KB allocation granularity.
    // We are further limited by our implementation of virtual memory page mapping which must track each individual
    // page mapping in the PAL virtual GPU memory object. To save memory, we limit the virtualMemPageSize (and thus
    // the virtualMemAllocGranularity) to 64KB.
    constexpr gpusize VamGranularity  = 4096;
    constexpr gpusize VirtualPageSize = 65536;

    m_memoryProperties.realMemAllocGranularity    = VamGranularity;
    m_memoryProperties.virtualMemAllocGranularity = VirtualPageSize;
    m_memoryProperties.virtualMemPageSize         = VirtualPageSize;

    m_memoryProperties.localMemoryType     = LocalMemoryType::Unknown;
    m_memoryProperties.memOpsPerClock      = MemoryOpsPerClock(m_memoryProperties.localMemoryType);
    m_memoryProperties.vramBusBitWidth     = 32;
    m_memoryProperties.uibVersion          = 0;
    m_memoryProperties.pdeSize             = sizeof(uint64);
    m_memoryProperties.pteSize             = sizeof(uint64);
    m_memoryProperties.spaceMappedPerPde   = m_memoryProperties.vaEnd + 1;
    m_memoryProperties.numPtbsPerGroup     = 1;
    m_memoryProperties.fragmentSize        = 0x10000;
    m_memoryProperties.numExcludedVaRanges = 0;

    m_memoryProperties.privateApertureBase = 0;
    m_memoryProperties.sharedApertureBase  = 0;

    m_memoryProperties.localHeapSize     = 1048576;
    m_memoryProperties.invisibleHeapSize = 1048576;
    m_memoryProperties.nonLocalHeapSize  = 1048576;

    m_memoryProperties.flags.ptbInNonLocal              = 0;
    m_memoryProperties.flags.adjustVmRangeEscapeSupport = 0;

    m_memoryProperties.flags.virtualRemappingSupport = 1;
    m_memoryProperties.flags.pinningSupport          = 1;
    m_memoryProperties.flags.supportPerSubmitMemRefs = false;
    m_memoryProperties.flags.globalGpuVaSupport      = false;
    m_memoryProperties.flags.svmSupport              = 0;
    m_memoryProperties.flags.autoPrioritySupport     = 0;

    m_memoryProperties.flags.iommuv2Support = 0;

    const uint32 vaRangeNumBits = m_chipProperties.gfxip.vaRangeNumBits;
    Result       result         = FixupUsableGpuVirtualAddressRange(
                                      m_force32BitVaSpace ? VaRangeLimitTo32bits : vaRangeNumBits);

    return result;
}

// =====================================================================================================================
// Initializes the properties External physical memory(SDI) as seen by the GPU.
void Device::InitExternalPhysicalHeap()
{
    m_memoryProperties.busAddressableMemSize = 0;
}

// =====================================================================================================================
// This is help methods. Init cache and debug file paths
void Device::InitOutputPaths()
{
    const char* pPath;

    // Initialize the root path of cache files and debug files
    // Cascade:
    // 1. Find APPDATA to keep backward compatibility.
    pPath = getenv("APPDATA");

    if (pPath != nullptr)
    {
        Strncpy(m_cacheFilePath, pPath, sizeof(m_cacheFilePath));
        Strncpy(m_debugFilePath, pPath, sizeof(m_debugFilePath));
    }
}

// =====================================================================================================================
// Shared GPU memory only applies to scenarios with multiple devices which we don't have...
Result Device::OpenExternalSharedGpuMemory(
    const ExternalGpuMemoryOpenInfo& openInfo,
    void*                            pPlacementAddr,
    GpuMemoryCreateInfo*             pMemCreateInfo,
    IGpuMemory**                     ppGpuMemory)
{
    return Result::Unsupported;
}

// =====================================================================================================================
// Opens shared image from anyone except another PAL device in the same LDA chain.
Result Device::OpenExternalSharedImage(
    const ExternalImageOpenInfo& openInfo,
    void*                        pImagePlacementAddr,
    void*                        pGpuMemoryPlacementAddr,
    GpuMemoryCreateInfo*         pMemCreateInfo,
    IImage**                     ppImage,
    IGpuMemory**                 ppGpuMemory)
{
    // don't expect to ever get here since GetExternalSharedImageSizes() should prevent it
    PAL_NEVER_CALLED();

    return Result::Unsupported;
}

// =====================================================================================================================
// Performs OS-specific early initialization steps for this Device object. Anything created or initialized by this
// function can only be destroyed or deinitialized on Device destruction.
Result Device::OsEarlyInit()
{
    return Result::Success;
}

// =====================================================================================================================
// Performs potentially unsafe OS-specific late initialization steps for this Device object. Anything created or
// initialized by this function must be destroyed or deinitialized in Cleanup().
Result Device::OsLateInit()
{
    return Result::Success;
}

// =====================================================================================================================
// Gets current fullscreen frame metadata control flags from the shared memory buffer.
Result Device::PollFullScreenFrameMetadataControl(
    uint32                         vidPnSrcId,
    PerSourceFrameMetadataControl* pFrameMetadataControl
    ) const
{
    return Result::Unsupported;
}

// =====================================================================================================================
// We don't support NULL devices that emulate workstation GPUs, so there's nothing to do here.
Result Device::QueryWorkStationCaps(
    WorkStationCaps* pCaps
    ) const
{
    return Result::Success;
}

// =====================================================================================================================
// We're not ever submitting anything, so we never added gpu memory references, which implies we don't need to remove
// them either.  :-)
Result Device::RemoveGpuMemoryReferences(
    uint32            gpuMemoryCount,
    IGpuMemory*const* ppGpuMemory,
    IQueue*           pQueue
    )
{
    return Result::Success;
}

// =====================================================================================================================
// Application profiles?  Null devices?  I don't think so.
Result Device::QueryApplicationProfile(
    const char*         pFilename,
    const char*         pPathname,  // This parameter is optional and may be null!
    ApplicationProfile* pOut
    ) const
{
    return Result::Unsupported;
}
// =====================================================================================================================
// Application profiles?  Null devices?  I don't think so.
Result Device::QueryRawApplicationProfile(
    const char*              pFilename,
    const char*              pPathname,  // This parameter is optional and may be null!
    ApplicationProfileClient client,
    const char**             pOut
    )
{
    return Result::Unsupported;
}
// =====================================================================================================================
Result Device::QueryDisplayConnectors(
    uint32*                     pConnectorCount,
    DisplayConnectorProperties* pConnectors)
{
    return Result::Unsupported;
}

// =====================================================================================================================
size_t Device::QueueObjectSize(
    const QueueCreateInfo& createInfo
    ) const
{
    return sizeof(Queue);
}

// =====================================================================================================================
// Would normally read the specificied setting from the registry, but we don't do that since we don't even know where
// "our" registry is.
bool Device::ReadSetting(
    const char*          pSettingName,
    Util::ValueType      valueType,
    void*                pValue,
    InternalSettingScope settingType,
    size_t               bufferSz
    ) const
{
    return false;
}

// =====================================================================================================================
// We never added an emulated private screen, so there shouldn't be anything to do at "remove" time either
Result Device::RemoveEmulatedPrivateScreen(
    uint32 targetId)
{
    return Result::Success;
}
// =====================================================================================================================
// Helper function to set MGPU compositing mode.  We don't support multiple NULL devices, so there's nothing to do.
Result Device::SetMgpuMode(
    const SetMgpuModeInput& setMgpuModeInput
    ) const
{
    return Result::Success;
}

// =====================================================================================================================
// Specifies how many frames can be placed in the presentation queue.  We can't submit or present anything in NULL
// device mode, so it doesn't really matter what they say.
Result Device::SetMaxQueuedFrames(
    uint32 maxFrames)
{
    return Result::Success;
}

// =====================================================================================================================
// This is not supported for the NULL device.
Result Device::SetPowerProfile(
    PowerProfile         profile,
    CustomPowerProfile*  pInfo)
{
    PAL_NOT_IMPLEMENTED();

    return Result::Success;
}

} // NullDevice
} // Pal
