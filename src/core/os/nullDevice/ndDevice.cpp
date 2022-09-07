/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_BUILD_NULL_DEVICE

#include "core/hw/amdgpu_asic.h"
#include "core/os/nullDevice/ndDevice.h"
#include "core/os/nullDevice/ndFence.h"
#include "core/os/nullDevice/ndGpuMemory.h"
#include "core/os/nullDevice/ndPlatform.h"
#include "core/os/nullDevice/ndQueue.h"
#include "palFormatInfo.h"
#include "palSysMemory.h"

#if PAL_AMDGPU_BUILD
#include "core/os/amdgpu/amdgpuHeaders.h"
#endif

#include <limits.h>

using namespace Util;

namespace Pal
{
namespace NullDevice
{

static constexpr uint32 GfxEngineGfx6 = CIASICIDGFXENGINE_SOUTHERNISLAND;
static constexpr uint32 GfxEngineGfx9 = CIASICIDGFXENGINE_ARCTICISLAND;

constexpr NullIdLookup SentinelNullGpu = {NullGpuId::Max};

// Identification table for all the GPUs that are supported in NULL device mode
extern const NullIdLookup  NullIdLookupTable[] =
{
    { NullGpuId::Polaris10, FAMILY_VI,  VI_POLARIS10_P_A0,    PRID_VI_POLARIS10_C7,         GfxEngineGfx6,  DEVICE_ID_VI_POLARIS10_P_67DF, "POLARIS10:gfx803" },
    { NullGpuId::Polaris11, FAMILY_VI,  VI_POLARIS11_M_A0,    PRID_VI_POLARIS11_CF,         GfxEngineGfx6,  DEVICE_ID_VI_POLARIS11_M_67EF, "POLARIS11:gfx803" },
    { NullGpuId::Polaris12, FAMILY_VI,  VI_POLARIS12_V_A0,    PRID_VI_POLARIS12_C7,         GfxEngineGfx6,  DEVICE_ID_VI_POLARIS12_V_699F, "POLARIS12:gfx803" },
    { NullGpuId::Vega10,    FAMILY_AI,  AI_VEGA10_P_A0,       PRID_AI_VEGA10_C3,            GfxEngineGfx9,  DEVICE_ID_AI_VEGA10_P_6860,    "VEGA10:gfx900" },
    { NullGpuId::Raven,     FAMILY_RV,  RAVEN_A0,             PRID_RV_81,                   GfxEngineGfx9,  DEVICE_ID_RV_15DD,             "RAVEN:gfx902" },
    { NullGpuId::Vega12,    FAMILY_AI,  AI_VEGA12_P_A0,       PRID_AI_VEGA12_00,            GfxEngineGfx9,  DEVICE_ID_AI_VEGA12_P_69A0,    "VEGA12:gfx904" },
    { NullGpuId::Vega20,    FAMILY_AI,  AI_VEGA20_P_A0,       PRID_AI_VEGA20_00,            GfxEngineGfx9,  DEVICE_ID_AI_VEGA20_P_66A0,    "VEGA20:gfx906" },
    { NullGpuId::Raven2,    FAMILY_RV,  RAVEN2_A0,            PRID_RV_E2,                   GfxEngineGfx9,  DEVICE_ID_RV2_15D8,            "RAVEN2:gfx909" },
    { NullGpuId::Renoir,    FAMILY_RV,  RENOIR_A0,            PRID_RENOIR_01,               GfxEngineGfx9,  DEVICE_ID_RENOIR_1636,         "RENOIR:gfx90C" },

    { NullGpuId::Navi10,     FAMILY_NV,  NV_NAVI10_P_A2,       PRID_NV_NAVI10_00,           GfxEngineGfx9,  DEVICE_ID_NV_NAVI10_P_7310,    "NAVI10:gfx1010" },
    { NullGpuId::Navi12,     FAMILY_NV,  NV_NAVI12_P_A0,       PRID_NV_NAVI12_00,           GfxEngineGfx9,  DEVICE_ID_NV_NAVI12_P_7360,    "NAVI12:gfx1011" },
    { NullGpuId::Navi14,     FAMILY_NV,  NV_NAVI14_M_A0,       PRID_NV_NAVI14_00,           GfxEngineGfx9,  DEVICE_ID_NV_NAVI14_M_7340,    "NAVI14:gfx1012" },

    { NullGpuId::Navi21, FAMILY_NV,  NV_NAVI21_P_A0,       PRID_NV_NAVI10_00,            GfxEngineGfx9,  DEVICE_ID_NV_NAVI10_P_7310, "NAVI21:gfx1030" },
    { NullGpuId::Navi22, FAMILY_NV,  NV_NAVI22_P_A0,       PRID_NV_NAVI10_00,            GfxEngineGfx9,  DEVICE_ID_NV_NAVI10_P_7310, "NAVI22:gfx1031" },
    { NullGpuId::Navi23, FAMILY_NV,  NV_NAVI23_P_A0,       PRID_NV_NAVI10_00,            GfxEngineGfx9,  DEVICE_ID_NV_NAVI10_P_7310, "NAVI23:gfx1032" },
    { NullGpuId::Navi24, FAMILY_NV,  NV_NAVI24_P_A0,       PRID_NV_NAVI10_00,            GfxEngineGfx9,  DEVICE_ID_NV_NAVI10_P_7310, "NAVI24:gfx1034" },

};
extern const uint32 NullIdLookupTableCount = static_cast<uint32>(Util::ArrayLen(NullIdLookupTable));

// =====================================================================================================================
Device::Device(
    Platform*              pPlatform,
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
    Strncpy(&m_gpuName[0], nullIdLookup.pName, sizeof(m_gpuName));
}

// =====================================================================================================================
// Factory function for creating Device objects. Creates a new Windows::Device object if the GPU is supported by
// the PAL library.
Result Device::Create(
    Platform*  pPlatform,
    Device**   ppDeviceOut,
    NullGpuId  nullGpuId)
{
    const NullIdLookup nullIdLookup = GetDeviceById(nullGpuId);
    Result             result       = Result::ErrorInitializationFailed;

    // Determine if the GPU is supported by PAL, and if so, what its hardware IP levels are.
    HwIpLevels ipLevels = {};
    if (Pal::Device::DetermineGpuIpLevels(nullIdLookup.familyId,
                                          nullIdLookup.eRevId,
                                          UINT_MAX, // microcode version, we just want to be over the min-supported ver
                                          pPlatform,
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
                                                               nullIdLookup,
                                                               hwDeviceSizes);

            result = (*ppDeviceOut)->EarlyInit(ipLevels);
            if (result != Result::Success)
            {
                (*ppDeviceOut)->Cleanup(); // Ignore result; we've already failed.
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
// Finds the device info for a given NullGpuId
NullIdLookup Device::GetDeviceById(
    NullGpuId nullGpuId)
{
    NullIdLookup deviceInfo = SentinelNullGpu;
    for (const NullIdLookup& entry : NullIdLookupTable)
    {
        if (entry.nullId == nullGpuId)
        {
            deviceInfo = entry;
            break;
        }
    }
    return deviceInfo;
}

// =====================================================================================================================
// Finds the device info for a given NullGpuId
NullIdLookup Device::GetDeviceByName(
    const char* pGpuName)
{
    NullIdLookup deviceInfo = SentinelNullGpu;
    char localGpuName[64] = {};
    for (const NullIdLookup& entry : NullIdLookupTable)
    {
        if (entry.pName == nullptr)
        {
            continue;
        }

        // The lookup entries are in the form "gpuname:gfx###". We only want the first part.
        Strncpy(localGpuName, entry.pName, sizeof(localGpuName));
        char* pColon = strchr(localGpuName, ':');
        if (pColon == nullptr)
        {
            PAL_ASSERT_ALWAYS_MSG("NullGpuName %s is too long!", entry.pName);
            continue;
        }
        *pColon = '\0';

        if (Strcasecmp(pGpuName, localGpuName) == 0)
        {
            deviceInfo = entry;
            break;
        }
    }
    return deviceInfo;
}

// =====================================================================================================================
Result Device::AddEmulatedPrivateScreen(
    const PrivateScreenCreateInfo& createInfo,
    uint32*                        pTargetId)
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
// Captures a GPU timestamp with the corresponding CPU timestamps, allowing tighter CPU/GPU timeline synchronization.
Result Device::GetCalibratedTimestamps(
    CalibratedTimestamps* pCalibratedTimestamps
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
    Pal::Queue* pQueue = PAL_PLACEMENT_NEW(pPlacementAddr) Queue(1, this, &createInfo);

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
// Call to wait for multiple semaphores
Result Device::WaitForSemaphores(
    uint32                       semaphoreCount,
    const IQueueSemaphore*const* ppSemaphores,
    const uint64*                pValues,
    uint32                       flags,
    uint64                       timeout) const
{
    return Result::Unsupported;
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

// =====================================================================================================================
// Helper method which initializes the GPU chip properties for all hardware families using the GFX9 hardware layer.
void Device::FillGfx6ChipProperties(
    GpuChipProperties* pChipProps)
{
    auto*const  pChipInfo  = &pChipProps->gfx6;

    const uint32     familyId = pChipProps->familyId;
    const uint32     eRevId   = pChipProps->eRevId;
    const GfxIpLevel gfxLevel = pChipProps->gfxLevel;

    if (AMDGPU_IS_TAHITI(familyId, eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001; // MC_ARB_RAMCFG_DEFAULT;
        pChipInfo->gbAddrConfig            = 0x02011003; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg           = 0x0D0D0DCD; // PA_SC_RASTER_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg1          = 0x0000000D;
        pChipInfo->numShaderEngines        =    2; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    2; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    4; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numPhysicalVgprsPerSimd =  256; // tahiti__GPU__SP__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =    8; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =   12; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       = 1792; // tahiti__GPU__VGT__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_PITCAIRN(familyId, eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001; // MC_ARB_RAMCFG_DEFAULT;
        pChipInfo->gbAddrConfig            = 0x02010001; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg           = 0x0D0D0DCD; // PA_SC_RASTER_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg1          = 0x0000000D; // PA_SC_RASTER_CONFIG_1_DEFAULT;
        pChipInfo->numShaderEngines        =    2; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    2; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    4; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numPhysicalVgprsPerSimd =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =    5; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =    8; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       = 1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_CAPEVERDE(familyId, eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001; // MC_ARB_RAMCFG_DEFAULT;
        pChipInfo->gbAddrConfig            = 0x02010001; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg           = 0x0D0D0DCD; // PA_SC_RASTER_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg1          = 0x0000000D; // PA_SC_RASTER_CONFIG_1_DEFAULT;
        pChipInfo->numShaderEngines        =    1; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    2; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    4; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numPhysicalVgprsPerSimd =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =    5; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =    4; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       = 1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_OLAND(familyId, eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001; // MC_ARB_RAMCFG_DEFAULT;
        pChipInfo->gbAddrConfig            = 0x02010001; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg           = 0x0D0D0DCD; // PA_SC_RASTER_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg1          = 0x0000000D; // PA_SC_RASTER_CONFIG_1_DEFAULT;
        pChipInfo->numShaderEngines        =    1; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    2; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numPhysicalVgprsPerSimd =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =    6; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =    4; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   16; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       =  768; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   16; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_HAINAN(familyId, eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001; // MC_ARB_RAMCFG_DEFAULT;
        pChipInfo->gbAddrConfig            = 0x02010001; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg           = 0x0D0D0DCD; // PA_SC_RASTER_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg1          = 0x0000000D; // PA_SC_RASTER_CONFIG_1_DEFAULT;
        pChipInfo->numShaderEngines        =    1; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    1; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numPhysicalVgprsPerSimd =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =    5; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =    2; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   16; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       =  768; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   16; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_SPECTRE(familyId, eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001; // MC_ARB_RAMCFG_DEFAULT;
        pChipInfo->gbAddrConfig            = 0x02011002; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg           = 0x0D0D0DCD; // PA_SC_RASTER_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg1          = 0x0000000D; // PA_SC_RASTER_CONFIG_1_DEFAULT;
        pChipInfo->numShaderEngines        =    1; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    2; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numPhysicalVgprsPerSimd =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =    8; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =    4; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   16; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       =  768; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   16; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_SPOOKY(familyId, eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001; // MC_ARB_RAMCFG_DEFAULT;
        pChipInfo->gbAddrConfig            = 0x02011002; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg           = 0x0D0D0DCD; // PA_SC_RASTER_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg1          = 0x0000000D; // PA_SC_RASTER_CONFIG_1_DEFAULT;
        pChipInfo->numShaderEngines        =    1; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    1; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numPhysicalVgprsPerSimd =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =    3; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =    4; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   16; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       =  768; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   16; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_HAWAII(familyId, eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001; // MC_ARB_RAMCFG_DEFAULT;
        pChipInfo->gbAddrConfig            = 0x02011003; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg           = 0x0D0DCDCD; // PA_SC_RASTER_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg1          = 0x0000000D; // PA_SC_RASTER_CONFIG_1_DEFAULT;
        pChipInfo->numShaderEngines        =    4; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    4; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numPhysicalVgprsPerSimd =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =   11; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =   16; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       = 1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_GODAVARI(familyId, eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001; // MC_ARB_RAMCFG_DEFAULT;
        pChipInfo->gbAddrConfig            = 0x02010001; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg           = 0x0D0D0DCD; // PA_SC_RASTER_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg1          = 0x0000000D; // PA_SC_RASTER_CONFIG_1_DEFAULT;
        pChipInfo->numShaderEngines        =    1; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    2; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numPhysicalVgprsPerSimd =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =    2; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =    2; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   16; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       =  256; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   16; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_KALINDI(familyId, eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001; // MC_ARB_RAMCFG_DEFAULT;
        pChipInfo->gbAddrConfig            = 0x02010001; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg           = 0x0D0D0DCD; // PA_SC_RASTER_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg1          = 0x0000000D; // PA_SC_RASTER_CONFIG_1_DEFAULT;
        pChipInfo->numShaderEngines        =    1; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    1; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numPhysicalVgprsPerSimd =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =    2; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =    2; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   16; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       =  256; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   16; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_BONAIRE(familyId, eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001; // MC_ARB_RAMCFG_DEFAULT;
        pChipInfo->gbAddrConfig            = 0x02011002; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg           = 0x0D0D0DCD; // PA_SC_RASTER_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg1          = 0x0000000D; // PA_SC_RASTER_CONFIG_1_DEFAULT;
        pChipInfo->numShaderEngines        =    2; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    2; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numPhysicalVgprsPerSimd =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =    7; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =    4; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       = 1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_CARRIZO(familyId, eRevId) ||
             AMDGPU_IS_BRISTOL(familyId, eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001; // MC_ARB_RAMCFG_DEFAULT;
        pChipInfo->gbAddrConfig            = 0x22011003; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg           = 0x0D0DCDCD; // PA_SC_RASTER_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg1          = 0x0000000D; // PA_SC_RASTER_CONFIG_1_DEFAULT;
        pChipInfo->numShaderEngines        =    1; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    2; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numPhysicalVgprsPerSimd =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =    8; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =    4; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   16; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       =  768; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   16; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_ICELAND(familyId, eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001; // MC_ARB_RAMCFG_DEFAULT;
        pChipInfo->gbAddrConfig            = 0x22010001; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg           = 0x0D0DCDCD; // PA_SC_RASTER_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg1          = 0x0000000D; // PA_SC_RASTER_CONFIG_1_DEFAULT;
        pChipInfo->numShaderEngines        =    1; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    2; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numPhysicalVgprsPerSimd =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =    6; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =    2; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   16; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       =  768; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   16; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_TONGA(familyId, eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001; // MC_ARB_RAMCFG_DEFAULT;
        pChipInfo->gbAddrConfig            = 0x22011003; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg           = 0x0D0DCDCD; // PA_SC_RASTER_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg1          = 0x0000000D; // PA_SC_RASTER_CONFIG_1_DEFAULT;
        pChipInfo->numShaderEngines        =    4; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    2; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numPhysicalVgprsPerSimd =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =    8; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =   12; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       = 1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_FIJI(familyId, eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001; // MC_ARB_RAMCFG_DEFAULT;
        pChipInfo->gbAddrConfig            = 0x22011003; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg           = 0x0D0DCDCD; // PA_SC_RASTER_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg1          = 0x0000000D; // PA_SC_RASTER_CONFIG_1_DEFAULT;
        pChipInfo->numShaderEngines        =    4; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    4; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numPhysicalVgprsPerSimd =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =   16; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =   16; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       = 1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_POLARIS10(familyId, eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001; // MC_ARB_RAMCFG_DEFAULT;
        pChipInfo->gbAddrConfig            = 0x22011003; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg           = 0x0D0DCDCD; // PA_SC_RASTER_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg1          = 0x0000000D; // PA_SC_RASTER_CONFIG_1_DEFAULT;
        pChipInfo->numShaderEngines        =    4; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    2; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numPhysicalVgprsPerSimd =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =    9; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =    8; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       = 1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_POLARIS11(familyId, eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001; // MC_ARB_RAMCFG_DEFAULT;
        pChipInfo->gbAddrConfig            = 0x22011002; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg           = 0x0D0DCDCD; // PA_SC_RASTER_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg1          = 0x0000000D; // PA_SC_RASTER_CONFIG_1_DEFAULT;
        pChipInfo->numShaderEngines        =    2; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    2; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numPhysicalVgprsPerSimd =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =    8; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =    4; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       = 1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_POLARIS12(familyId, eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001; // MC_ARB_RAMCFG_DEFAULT;
        pChipInfo->gbAddrConfig            = 0x22011002; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg           = 0x0D0DCDCD; // PA_SC_RASTER_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg1          = 0x0000000D; // PA_SC_RASTER_CONFIG_1_DEFAULT;
        pChipInfo->numShaderEngines        =    2; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    2; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numPhysicalVgprsPerSimd =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =    5; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =    4; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       = 1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_STONEY(familyId, eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->mcArbRamcfg             = 0x00007001; // MC_ARB_RAMCFG_DEFAULT;
        pChipInfo->gbAddrConfig            = 0x22010001; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg           = 0x0D0DCDCD; // PA_SC_RASTER_CONFIG_DEFAULT;
        pChipInfo->paScRasterCfg1          = 0x0000000D; // PA_SC_RASTER_CONFIG_1_DEFAULT;
        pChipInfo->numShaderEngines        =    1; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    2; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->numPhysicalVgprsPerSimd =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =    3; // GPU__GC__NUM_CU_PER_SH;
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

    pChipInfo->numCuPerSh              = pChipInfo->maxNumCuPerSh;
    pChipInfo->backendDisableMask      = 0; // everything is enabled!
    pChipInfo->numActiveRbs            = pChipInfo->maxNumRbPerSe * pChipInfo->numShaderEngines;
    pChipInfo->gbTileMode[TILEINDEX_LINEAR_ALIGNED] = ADDR_TM_LINEAR_ALIGNED << 2;

    if ((gfxLevel == GfxIpLevel::GfxIp8) || (gfxLevel == GfxIpLevel::GfxIp8_1))
    {
        Gfx8InsertDummyTilingValues(pChipProps);
    }

    PAL_ASSERT(gfxLevel >= GfxIpLevel::GfxIp6);
    const uint32  activeCuMask = (1 << pChipInfo->numCuPerSh) - 1;

    // GFXIP 7+ hardware only has one shader array per shader engine!
    PAL_ASSERT(gfxLevel < GfxIpLevel::GfxIp7 || pChipInfo->numShaderArrays == 1);

    for (uint32 seIndex = 0; seIndex < pChipInfo->numShaderEngines; seIndex++)
    {
        for (uint32 shIndex = 0; shIndex < pChipInfo->numShaderArrays; shIndex++)
        {
            if (gfxLevel == GfxIpLevel::GfxIp6)
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
}

// =====================================================================================================================
// Helper method which initializes the GPU chip properties for all hardware families using the GFX6 hardware layer.
void Device::InitGfx6ChipProperties()
{
    // Call into the HWL to initialize the default values for many properties of the hardware (based on chip ID).
    Gfx6::InitializeGpuChipProperties(UINT_MAX, &m_chipProperties);

    // Fill in properties that would normally get pulled from the KMD
    FillGfx6ChipProperties(&m_chipProperties);

    // Call into the HWL to finish initializing some GPU properties which can be derived from the ones which we
    // overrode above.
    Gfx6::FinalizeGpuChipProperties(*this, &m_chipProperties);
}

// =====================================================================================================================
// Clients expect a valid number of swizzle equations when querying the device properties during their init code.
// It's a minor quality of life that doesn't impact correctness. Values are taken from a gfx8 GPU.
void Device::Gfx8InsertDummyTilingValues(
    GpuChipProperties* pChipProps)
{
    auto*const  pChipInfo  = &pChipProps->gfx6;

    uint32 constexpr Gfx8DummyTileModeValues[] = {
        0x00800150,
        0x00800950,
        0x00801150,
        0x00801950,
        0x00802950,
        0x00802948,
        0x00802954,
        0x00802954,
        0x00000144,
        0x02000148,
        0x02000150,
        0x06000154,
        0x06000154,
        0x02400148,
        0x02400150,
        0x02400170,
        0x06400154,
        0x06400154,
        0x0040014C,
        0x0100014C,
        0x0100015C,
        0x01000174,
        0x01000164,
        0x01000164,
        0x0040015C,
        0x01000160,
        0x01000178,
        0x02C00148,
        0x02C00150,
        0x06C00154,
        0x06C00154,
        0x00000000,
    };

    static_assert(sizeof(pChipInfo->gbTileMode) == sizeof(Gfx8DummyTileModeValues), "Size mismatch!");

    uint32 constexpr Gfx8DummyMacroTileModeValues[] = {
        0x000000E8,
        0x000000E8,
        0x000000E8,
        0x000000E4,
        0x000000D0,
        0x000000D0,
        0x000000D0,
        0x00000000,
        0x000000ED,
        0x000000E9,
        0x000000E8,
        0x000000E4,
        0x000000D0,
        0x00000090,
        0x00000040,
        0x00000000,
    };

    static_assert(sizeof(pChipInfo->gbMacroTileMode) == sizeof(Gfx8DummyMacroTileModeValues), "Size mismatch!");

    memcpy(pChipInfo->gbTileMode, &Gfx8DummyTileModeValues, sizeof(Gfx8DummyTileModeValues));
    memcpy(pChipInfo->gbMacroTileMode, &Gfx8DummyMacroTileModeValues, sizeof(Gfx8DummyMacroTileModeValues));
}

// =====================================================================================================================
// Helper method which initializes the GPU chip properties for all hardware families using the GFX9 hardware layer.
void Device::FillGfx9ChipProperties(
    GpuChipProperties* pChipProps)
{
    auto*const   pChipInfo = &pChipProps->gfx9;
    const uint32 familyId  = pChipProps->familyId;
    const uint32 eRevId    = pChipProps->eRevId;

    if (AMDGPU_IS_VEGA10(familyId, eRevId))
    {
        // NOTE: KMD only gives us a flag indicating whether the Off-chip LDS buffers are "large" or not. The HWL will
        // need to determine the actual LDS buffer size based on this flag.
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->gbAddrConfig            = 0x2A110002; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->numShaderEngines        =    4; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    4; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->minWavefrontSize        =   64;
        pChipInfo->maxWavefrontSize        =   64;
        pChipInfo->numPhysicalVgprsPerSimd =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =   16; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =   16; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       = 1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_VEGA12(familyId, eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->gbAddrConfig            = 0x26110001; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->numShaderEngines        =    4; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    2; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->minWavefrontSize        =   64;
        pChipInfo->maxWavefrontSize        =   64;
        pChipInfo->numPhysicalVgprsPerSimd =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =    5; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =    8; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       = 1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_VEGA20(familyId, eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->gbAddrConfig            = 0x2A110002; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->numShaderEngines        =    4; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    4; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->minWavefrontSize        =   64;
        pChipInfo->maxWavefrontSize        =   64;
        pChipInfo->numPhysicalVgprsPerSimd =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =   16; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =   16; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       = 1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_RAVEN(familyId, eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->gbAddrConfig            = 0x26010001; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->numShaderEngines        =    1; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    2; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->minWavefrontSize        =   64;
        pChipInfo->maxWavefrontSize        =   64;
        pChipInfo->numPhysicalVgprsPerSimd =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =   11; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =    4; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       = 1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_RAVEN2(familyId, eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->gbAddrConfig            = 0x26010001; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->numShaderEngines        =    1; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    1; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->minWavefrontSize        =   64;
        pChipInfo->maxWavefrontSize        =   64;
        pChipInfo->numPhysicalVgprsPerSimd =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =    3; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =    2; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       = 1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_RENOIR(familyId, eRevId))
    {
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->gbAddrConfig            = 0x26010001; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->numShaderEngines        =    1; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    1; // GPU__GC__NUM_SH_PER_SE;
        pChipInfo->maxNumRbPerSe           =    2; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =   64; // GPU__GC__WAVE_SIZE;
        pChipInfo->minWavefrontSize        =   64;
        pChipInfo->maxWavefrontSize        =   64;
        pChipInfo->numPhysicalVgprsPerSimd =  256; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =    8; // GPU__GC__NUM_CU_PER_SH;
        pChipInfo->numTccBlocks            =    4; // GPU__TC__NUM_TCCS;
        pChipInfo->gsVgtTableDepth         =   32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       = 1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_NAVI10(familyId, eRevId))
    {
        pChipInfo->supportSpiPrefPriority  =    1;
        pChipInfo->doubleOffchipLdsBuffers =    1;
        pChipInfo->gbAddrConfig            = 0x44; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->numShaderEngines        =    2; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    2; // GPU__GC__NUM_SA_PER_SE
        pChipInfo->maxNumRbPerSe           =    8; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =   32; // GPU__GC__SQ_WAVE_SIZE;
        pChipInfo->minWavefrontSize        =   32;
        pChipInfo->maxWavefrontSize        =   64;
        pChipInfo->numPhysicalVgprsPerSimd = 1024; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =   10; // GPU__GC__NUM_WGP_PER_SA * 2;
        pChipInfo->numTccBlocks            =   16; // GPU__GC__NUM_GL2C;
        pChipInfo->gsVgtTableDepth         =   32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       = 1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_NAVI12(familyId, eRevId))
    {
        pChipInfo->supportSpiPrefPriority  =    1;
        pChipInfo->doubleOffchipLdsBuffers =    1;
        pChipInfo->gbAddrConfig            = 0x44; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->numShaderEngines        =    2; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    2; // GPU__GC__NUM_SA_PER_SE
        pChipInfo->maxNumRbPerSe           =    8; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =   32; // GPU__GC__SQ_WAVE_SIZE;
        pChipInfo->minWavefrontSize        =   32;
        pChipInfo->maxWavefrontSize        =   64;
        pChipInfo->numPhysicalVgprsPerSimd = 1024; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =   10; // GPU__GC__NUM_WGP_PER_SA * 2;
        pChipInfo->numTccBlocks            =   16; // GPU__GC__NUM_GL2C;
        pChipInfo->gsVgtTableDepth         =   32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       = 1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_NAVI14(familyId, eRevId))
    {
        pChipInfo->supportSpiPrefPriority  =    1;
        pChipInfo->doubleOffchipLdsBuffers =    1;
        pChipInfo->gbAddrConfig            = 0x43; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->numShaderEngines        =    1; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =    2; // GPU__GC__NUM_SA_PER_SE
        pChipInfo->maxNumRbPerSe           =    8; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =   32; // GPU__GC__SQ_WAVE_SIZE;
        pChipInfo->minWavefrontSize        =   32;
        pChipInfo->maxWavefrontSize        =   64;
        {
            pChipInfo->numPhysicalVgprsPerSimd = 1024; // GPU__GC__NUM_GPRS;
        }
        pChipInfo->maxNumCuPerSh           =   12; // GPU__GC__NUM_WGP_PER_SA * 2;
        pChipInfo->numTccBlocks            =    8; // GPU__GC__NUM_GL2C;
        pChipInfo->gsVgtTableDepth         =   32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       = 1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =   32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_NAVI21(familyId, eRevId))
    {
        pChipInfo->supportSpiPrefPriority  =     1;
        pChipInfo->doubleOffchipLdsBuffers =     1;
        pChipInfo->gbAddrConfig            = 0x345; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->numShaderEngines        =     4; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =     2; // GPU__GC__NUM_SA_PER_SE
        pChipInfo->maxNumRbPerSe           =     4; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =    32; // GPU__GC__SQ_WAVE_SIZE;
        pChipInfo->minWavefrontSize        =    32;
        pChipInfo->maxWavefrontSize        =    64;
        pChipInfo->numPhysicalVgprsPerSimd =  1024; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =    10; // GPU__GC__NUM_WGP_PER_SA * 2;
        pChipInfo->numTccBlocks            =    20; // GPU__GC__NUM_GL2C;
        pChipInfo->gsVgtTableDepth         =    32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       =  1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =    32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_NAVI22(familyId, eRevId))
    {
        pChipInfo->supportSpiPrefPriority  =     1;
        pChipInfo->doubleOffchipLdsBuffers =     1;
        pChipInfo->gbAddrConfig            = 0x345; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->numShaderEngines        =     2; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =     2; // GPU__GC__NUM_SA_PER_SE
        pChipInfo->maxNumRbPerSe           =     4; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =    32; // GPU__GC__SQ_WAVE_SIZE;
        pChipInfo->minWavefrontSize        =    32;
        pChipInfo->maxWavefrontSize        =    64;
        pChipInfo->numPhysicalVgprsPerSimd =  1024; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =    14; // GPU__GC__NUM_WGP_PER_SA * 2;
        pChipInfo->numTccBlocks            =    20; // GPU__GC__NUM_GL2C;
        pChipInfo->gsVgtTableDepth         =    32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       =  1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =    32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_NAVI23(familyId, eRevId))
    {
        pChipInfo->supportSpiPrefPriority  =     1;
        pChipInfo->doubleOffchipLdsBuffers =     1;
        pChipInfo->gbAddrConfig            = 0x345; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->numShaderEngines        =     2; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =     2; // GPU__GC__NUM_SA_PER_SE
        pChipInfo->maxNumRbPerSe           =     4; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =    32; // GPU__GC__SQ_WAVE_SIZE;
        pChipInfo->minWavefrontSize        =    32;
        pChipInfo->maxWavefrontSize        =    64;
        pChipInfo->numPhysicalVgprsPerSimd =  1024; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =     8; // GPU__GC__NUM_WGP_PER_SA * 2;
        pChipInfo->numTccBlocks            =     8; // GPU__GC__NUM_GL2C;
        pChipInfo->gsVgtTableDepth         =    32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       =  1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =    32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_NAVI24(familyId, eRevId))
    {
        pChipInfo->supportSpiPrefPriority  =     1;
        pChipInfo->doubleOffchipLdsBuffers =     1;
        pChipInfo->gbAddrConfig            = 0x242; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->numShaderEngines        =     1; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =     2; // GPU__GC__NUM_SA_PER_SE
        pChipInfo->maxNumRbPerSe           =     4; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =    32; // GPU__GC__SQ_WAVE_SIZE;
        pChipInfo->minWavefrontSize        =    32;
        pChipInfo->maxWavefrontSize        =    64;
        pChipInfo->numPhysicalVgprsPerSimd =  1024; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =     8; // GPU__GC__NUM_WGP_PER_SA * 2;
        pChipInfo->numTccBlocks            =     4; // GPU__GC__NUM_GL2C;
        pChipInfo->gsVgtTableDepth         =    32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       =  1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =    32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else
    {
        // Unknown device id
        PAL_ASSERT_ALWAYS();
    }

    if (IsGfx103PlusExclusive(pChipProps->gfxLevel))
    {
        // Task/Mesh shaders are only supported for Gfx10.3+
        pChipInfo->supportMeshShader = pChipInfo->supportImplicitPrimitiveShader;
        pChipInfo->supportTaskShader = pChipInfo->supportImplicitPrimitiveShader;
    }

    // Assume all CUs and all RBs are active/enabled.
    pChipInfo->numCuPerSh         = pChipInfo->maxNumCuPerSh;
    pChipInfo->backendDisableMask = 0;
    pChipInfo->numActiveRbs       = pChipInfo->maxNumRbPerSe * pChipInfo->numShaderEngines;

    const uint32  activeCuMask = (1 << pChipInfo->numCuPerSh) - 1;
    for (uint32 seIndex = 0; seIndex < pChipInfo->numShaderEngines; seIndex++)
    {
        for (uint32 shIndex = 0; shIndex < pChipInfo->numShaderArrays; shIndex++)
        {
            pChipInfo->activeCuMask[seIndex][shIndex]   = activeCuMask;
            pChipInfo->alwaysOnCuMask[seIndex][shIndex] = activeCuMask;
        }
    }

    if (AMDGPU_IS_NAVI(familyId, eRevId))
    {
        PAL_ASSERT(pChipInfo->numCuPerSh <= 32);      // avoid overflow in activeWgpMask
        PAL_ASSERT((pChipInfo->numCuPerSh & 1) == 0); // CUs come in WGP pairs in gfx10
        const uint16  activeWgpMask = (1 << (pChipInfo->numCuPerSh / 2)) - 1;
        for (uint32 seIndex = 0; seIndex < pChipInfo->numShaderEngines; seIndex++)
        {
            for (uint32 shIndex = 0; shIndex < pChipInfo->numShaderArrays; shIndex++)
            {
                pChipInfo->gfx10.activeWgpMask[seIndex][shIndex]   = activeWgpMask;
                pChipInfo->gfx10.alwaysOnWgpMask[seIndex][shIndex] = activeWgpMask;
            }
        }
    }

}

// =====================================================================================================================
// Helper method which initializes the GPU chip properties for all hardware families using the GFX9 hardware layer.
void Device::InitGfx9ChipProperties()
{
    // Call into the HWL to initialize the default values for many properties of the hardware (based on chip ID).
    Gfx9::InitializeGpuChipProperties(GetPlatform(), UINT_MAX, &m_chipProperties);

    // Fill in properties that would normally get pulled from the KMD
    FillGfx9ChipProperties(&m_chipProperties);

    // Call into the HWL to finish initializing some GPU properties which can be derived from the ones which we
    // overrode above.
    Gfx9::FinalizeGpuChipProperties(*this, &m_chipProperties);
}

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
    m_chipProperties.vcnLevel = ipLevels.vcn;

    m_chipProperties.hwIpFlags.u32All = ipLevels.flags.u32All;

    for (uint32 i = 0; i < EngineTypeCount; i++)
    {
        m_engineProperties.perEngine[i].preferredCmdAllocHeaps[CommandDataAlloc]   = GpuHeapGartUswc;
        m_engineProperties.perEngine[i].preferredCmdAllocHeaps[EmbeddedDataAlloc]  = GpuHeapGartUswc;
        m_engineProperties.perEngine[i].preferredCmdAllocHeaps[GpuScratchMemAlloc] = GpuHeapInvisible;
    }

    for (uint32 i = 0; i < EngineTypeCount; i++)
    {
        switch (i)
        {
        case EngineTypeUniversal:
        case EngineTypeCompute:
        case EngineTypeDma:
            m_engineProperties.perEngine[i].flags.supportsTrackBusyChunks = 1;
            break;
        default:
            m_engineProperties.perEngine[i].flags.supportsTrackBusyChunks = 0;
            break;
        }
    }

    switch (m_chipProperties.gfxLevel)
    {
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
    case GfxIpLevel::GfxIp10_1:
    case GfxIpLevel::GfxIp9:
    case GfxIpLevel::GfxIp10_3:
        m_pFormatPropertiesTable = Gfx9::GetFormatPropertiesTable(m_chipProperties.gfxLevel,
                                                                  GetPlatform()->PlatformSettings());

        InitGfx9ChipProperties();
        Gfx9::InitializeGpuEngineProperties(m_chipProperties, &m_engineProperties);
        break;
    case GfxIpLevel::None:
        // No Graphics IP block found or recognized!
    default:
        break;
    }

    switch (m_chipProperties.ossLevel)
    {
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
    m_memoryProperties.vaEnd        = (1ull << m_chipProperties.gfxip.vaRangeNumBits) - 1;
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
// Application profiles?  Null devices?  I don't think so.
Result Device::QueryRawApplicationProfile(
    const wchar_t*           pFilename,
    const wchar_t*           pPathname,  // This parameter is optional and may be null
    ApplicationProfileClient client,
    const char**             pOut
    )
{
    return Result::Unsupported;
}

// =====================================================================================================================
Result Device::EnableSppProfile(
    const wchar_t* pFilename,
    const wchar_t* pPathname)
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

#endif
