/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palSettingsFileMgrImpl.h"
#include "palSysMemory.h"
#include "palInlineFuncs.h"

#include <limits.h>

using namespace Util;

namespace Pal
{
namespace NullDevice
{

// =====================================================================================================================
Device::Device(
    Platform*              pPlatform,
    const char*            pSettingsPath,
    const GpuInfo&         gpuInfo,
    const HwIpDeviceSizes& hwDeviceSizes)
    :
    Pal::Device(pPlatform,
                0, // deviceIndex, we only have one
                1, // always one screen attached to a screen
                sizeof(Device),
                hwDeviceSizes,
                UINT_MAX), // max semaphore count
    m_gpuInfo(gpuInfo),
    m_settingFileMgr(SettingsFileName, pPlatform),
    m_pSettingsPath(pSettingsPath)
{
    Strncpy(&m_gpuName[0], gpuInfo.pGpuName, sizeof(m_gpuName));
}

// =====================================================================================================================
// Factory function for creating Device objects. Creates a new Windows::Device object if the GPU is supported by
// the PAL library.
Result Device::Create(
    Platform*   pPlatform,
    const char* pSettingsPath,
    Device**    ppDeviceOut,
    NullGpuId   nullGpuId)
{
    Result result = Result::ErrorInitializationFailed;

    // Determine if the GPU is supported by PAL, and if so, what its hardware IP levels are.
    GpuInfo gpuInfo = {};
    HwIpLevels ipLevels = {};
    if ((GetGpuInfoForNullGpuId(nullGpuId, &gpuInfo) == Result::Success) &&
        Pal::Device::DetermineGpuIpLevels(gpuInfo.familyId,
                                          gpuInfo.eRevId,
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
                                      addrMgrSize;
        void* pMemory = PAL_MALLOC(neededMemSize, pPlatform, Util::AllocInternal);

        if (pMemory != nullptr)
        {
            (*ppDeviceOut) = PAL_PLACEMENT_NEW(pMemory) Device(pPlatform,
                                                               pSettingsPath,
                                                               gpuInfo,
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
    std::chrono::nanoseconds     timeout
    ) const
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
// Fill in the uCode versions for spoofed chips.
static void FillInSpoofedUcodeVersions(
    uint32                     familyId,
    uint32                     eRevId,
    GpuChipProperties*         pChipProps,
    const PalPlatformSettings& platformSettings)
{
    uint32 spoofedPfpVersion = UINT32_MAX;
    uint32 spoofedMecVersion = UINT32_MAX;

    if (IsGfx10(pChipProps->gfxLevel))
    {
        // Cap to RS64 - 1 (all GFX10 only have F32).
        spoofedPfpVersion = (Gfx11Rs64MinPfpUcodeVersion - 1);
        spoofedMecVersion = (Gfx11Rs64MinMecUcodeVersion - 1);
    }
    else if (IsGfx11(pChipProps->gfxLevel))
    {
        if (AMDGPU_IS_NAVI31(familyId, eRevId) ||
            AMDGPU_IS_NAVI32(familyId, eRevId) ||
            AMDGPU_IS_NAVI33(familyId, eRevId))
        {
            // Nothing to do here - RS64 based.
        }
        else if ((AMDGPU_IS_PHOENIX1(familyId, eRevId))   ||
                 (AMDGPU_IS_PHOENIX2(familyId, eRevId))   ||
#if PAL_BUILD_STRIX_HALO
                 (AMDGPU_IS_STRIX_HALO(familyId, eRevId)) ||
#endif
                 (AMDGPU_IS_STRIX1(familyId, eRevId)))
        {
            // Cap to RS64 - 1 (GFX11 APUs use F32).
            spoofedPfpVersion = (Gfx11Rs64MinPfpUcodeVersion - 1);
            spoofedMecVersion = (Gfx11Rs64MinMecUcodeVersion - 1);
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }
    }

    const uint32 pfpUcodeVersionSetting = platformSettings.spoofPfpUcodeVersion;
    const uint32 mecUcodeVersionSetting = platformSettings.spoofMecUcodeVersion;

    pChipProps->pfpUcodeVersion = (pfpUcodeVersionSetting == UINT32_MAX) ? spoofedPfpVersion : pfpUcodeVersionSetting;
    pChipProps->mecUcodeVersion = (mecUcodeVersionSetting == UINT32_MAX) ? spoofedMecVersion : mecUcodeVersionSetting;
}

// =====================================================================================================================
// Helper method which initializes the GPU chip properties for all hardware families using the GFX9 hardware layer.
void Device::FillGfx9ChipProperties(
    GpuChipProperties*         pChipProps,
    const PalPlatformSettings& platformSettings)
{
    auto*const   pChipInfo = &pChipProps->gfx9;
    const uint32 familyId  = pChipProps->familyId;
    const uint32 eRevId    = pChipProps->eRevId;

    FillInSpoofedUcodeVersions(familyId, eRevId, pChipProps, platformSettings);

    if (AMDGPU_IS_NAVI10(familyId, eRevId))
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
        pChipInfo->numPhysicalVgprsPerSimd = 1024; // GPU__GC__NUM_GPRS;
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
    else if (AMDGPU_IS_REMBRANDT(familyId, eRevId))
    {
        pChipInfo->supportSpiPrefPriority  =     1;
        pChipInfo->doubleOffchipLdsBuffers =     1;
        pChipInfo->gbAddrConfig            = 0x242; // GB_ADDR_CONFIG_DEFAULT; ///????
        pChipInfo->numShaderEngines        =     1; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =     2; // GPU__GC__NUM_SA_PER_SE
        pChipInfo->maxNumRbPerSe           =     4; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =    32; // GPU__GC__SQ_WAVE_SIZE;
        pChipInfo->minWavefrontSize        =    32;
        pChipInfo->maxWavefrontSize        =    64;
        pChipInfo->numPhysicalVgprsPerSimd =  1024; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =     6; // GPU__GC__NUM_WGP_PER_SA * 2;
        pChipInfo->numTccBlocks            =     4; // GPU__GC__NUM_GL2C;
        pChipInfo->gsVgtTableDepth         =    32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       =  1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =    32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_RAPHAEL(familyId, eRevId))
    {
        pChipInfo->supportSpiPrefPriority  =     1;
        pChipInfo->doubleOffchipLdsBuffers =     1;
        pChipInfo->gbAddrConfig            =  0x42; // GB_ADDR_CONFIG_DEFAULT; ///????
        pChipInfo->numShaderEngines        =     1; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =     1; // GPU__GC__NUM_SA_PER_SE
        pChipInfo->maxNumRbPerSe           =     1; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =    32; // GPU__GC__SQ_WAVE_SIZE;
        pChipInfo->minWavefrontSize        =    32;
        pChipInfo->maxWavefrontSize        =    64;
        pChipInfo->numPhysicalVgprsPerSimd =  1024; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =     2; // GPU__GC__NUM_WGP_PER_SA * 2;
        pChipInfo->numTccBlocks            =     2; // GPU__GC__NUM_GL2C;
        pChipInfo->gsVgtTableDepth         =    32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       =  1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =    32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_MENDOCINO(familyId, eRevId))
    {
        pChipInfo->supportSpiPrefPriority      =     1;
        pChipInfo->doubleOffchipLdsBuffers     =     1;
        pChipInfo->gbAddrConfig                =  0x42; // GB_ADDR_CONFIG_DEFAULT; ///????
        pChipInfo->numShaderEngines            =     1; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays             =     1; // GPU__GC__NUM_SA_PER_SE
        pChipInfo->maxNumRbPerSe               =     1; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize         =    32; // GPU__GC__SQ_WAVE_SIZE;

        pChipInfo->minWavefrontSize            =    32;
        pChipInfo->maxWavefrontSize            =    64;

        pChipInfo->numPhysicalVgprsPerSimd     =  1024; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh               =     2; // GPU__GC__NUM_WGP_PER_SA * 2;
        pChipInfo->numTccBlocks                =     2; // GPU__GC__NUM_GL2C;
        pChipInfo->gsVgtTableDepth             =    32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth           =  1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt            =    32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_NAVI31(familyId, eRevId))
    {
        pChipInfo->supportSpiPrefPriority  =     1;
        pChipInfo->doubleOffchipLdsBuffers =     1;
        pChipInfo->gbAddrConfig            = 0x545; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->numShaderEngines        =     6; // GC__NUM_SE;
        pChipInfo->numShaderArrays         =     2; // GC__NUM_SA_PER_SE
        pChipInfo->maxNumRbPerSe           =     4; // GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =    32; // GC__SQ_WAVE_SIZE;
        pChipInfo->minWavefrontSize        =    32;
        pChipInfo->maxWavefrontSize        =    64;
        pChipInfo->numPhysicalVgprsPerSimd =  1536; // GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =     8; // GC__NUM_WGP_PER_SA * 2;
        pChipInfo->numTccBlocks            =    24; // GC__NUM_GL2C;
        pChipInfo->gsVgtTableDepth         =    32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       =  1792; // GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =    32; // GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_NAVI32(familyId, eRevId))
    {
        pChipInfo->supportSpiPrefPriority  =     1;
        pChipInfo->doubleOffchipLdsBuffers =     1;
        pChipInfo->gbAddrConfig            = 0x545; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->numShaderEngines        =     3; // GC__NUM_SE;
        pChipInfo->numShaderArrays         =     2; // GC__NUM_SA_PER_SE
        pChipInfo->maxNumRbPerSe           =     4; // GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =    32; // GC__SQ_WAVE_SIZE;
        pChipInfo->minWavefrontSize        =    32;
        pChipInfo->maxWavefrontSize        =    64;
        pChipInfo->numPhysicalVgprsPerSimd =  1536; // GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =    10; // GC__NUM_WGP_PER_SA * 2;
        pChipInfo->numTccBlocks            =    16; // GC__NUM_GL2C;
        pChipInfo->gsVgtTableDepth         =    32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       =  1792; // GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =    32; // GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_NAVI33(familyId, eRevId))
    {
        pChipInfo->supportSpiPrefPriority  =     1;
        pChipInfo->doubleOffchipLdsBuffers =     1;
        pChipInfo->gbAddrConfig            = 0x343; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->numShaderEngines        =     2; // GC__NUM_SE;
        pChipInfo->numShaderArrays         =     2; // GC__NUM_SA_PER_SE
        pChipInfo->maxNumRbPerSe           =     4; // GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =    32; // GC__SQ_WAVE_SIZE;
        pChipInfo->minWavefrontSize        =    32;
        pChipInfo->maxWavefrontSize        =    64;
        pChipInfo->numPhysicalVgprsPerSimd =  1024; // GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =     8; // GC__NUM_WGP_PER_SA * 2;
        pChipInfo->numTccBlocks            =     8; // GC__NUM_GL2C;
        pChipInfo->gsVgtTableDepth         =    32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       =  1792; // GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =    32; // GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_PHOENIX1(familyId, eRevId))
    {
        pChipInfo->supportSpiPrefPriority  =     1;
        pChipInfo->doubleOffchipLdsBuffers =     1;
        pChipInfo->gbAddrConfig            = 0x242; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->numShaderEngines        =     1; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =     2; // GPU__GC__NUM_SA_PER_SE
        pChipInfo->maxNumRbPerSe           =     4; // GPU__GC__NUM_RB_PER_SE
        pChipInfo->nativeWavefrontSize     =    32; // GPU__GC__SQ_WAVE_SIZE;
        pChipInfo->minWavefrontSize        =    32;
        pChipInfo->maxWavefrontSize        =    64;
        pChipInfo->numPhysicalVgprsPerSimd =  1024; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =     6; // GPU__GC__NUM_WGP_PER_SA * 2;
        pChipInfo->numTccBlocks            =     4; // GPU__GC__NUM_GL2C;
        pChipInfo->gsVgtTableDepth         =    32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       =  1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =    32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_PHOENIX2(familyId, eRevId))
    {
        pChipInfo->supportSpiPrefPriority  =     1;
        pChipInfo->doubleOffchipLdsBuffers =     1;
        pChipInfo->gbAddrConfig            = 0x142; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->numShaderEngines        =     1; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =     1; // GPU__GC__NUM_SA_PER_SE
        pChipInfo->maxNumRbPerSe           =     1; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =    32; // GPU__GC__SQ_WAVE_SIZE;
        pChipInfo->minWavefrontSize        =    32;
        pChipInfo->maxWavefrontSize        =    64;
        pChipInfo->numPhysicalVgprsPerSimd =  1024; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =     4; // GPU__GC__NUM_WGP_PER_SA * 2;
        pChipInfo->numTccBlocks            =     4; // GPU__GC__NUM_GL2C;
        pChipInfo->gsVgtTableDepth         =    32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       =  1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =    32; // GPU__GC__NUM_MAX_GS_THDS;
    }
    else if (AMDGPU_IS_STRIX1(familyId, eRevId))
    {
        pChipInfo->supportSpiPrefPriority  =     1;
        pChipInfo->doubleOffchipLdsBuffers =     1;
        pChipInfo->gbAddrConfig            = 0x444; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->numShaderEngines        =     1; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =     2; // GPU__GC__NUM_SA_PER_SE
        pChipInfo->maxNumRbPerSe           =     4; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =    32; // GPU__GC__SQ_WAVE_SIZE;
        pChipInfo->minWavefrontSize        =    32;
        pChipInfo->maxWavefrontSize        =    64;
        pChipInfo->numPhysicalVgprsPerSimd =  1024; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =     8; // GPU__GC__NUM_WGP_PER_SA * 2;
        pChipInfo->numTccBlocks            =    16; // GPU__GC__NUM_GL2C;
        pChipInfo->gsVgtTableDepth         =    32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       =  1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =    32; // GPU__GC__NUM_MAX_GS_THDS;
    }
#if PAL_BUILD_STRIX_HALO
    else if (AMDGPU_IS_STRIX_HALO(familyId, eRevId))
    {
        pChipInfo->supportSpiPrefPriority  =     1;
        pChipInfo->doubleOffchipLdsBuffers =     1;
        pChipInfo->gbAddrConfig            = 0x343; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->numShaderEngines        =     2; // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         =     2; // GPU__GC__NUM_SA_PER_SE
        pChipInfo->maxNumRbPerSe           =     4; // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     =    32; // GPU__GC__SQ_WAVE_SIZE;
        pChipInfo->minWavefrontSize        =    32;
        pChipInfo->maxWavefrontSize        =    64;
        pChipInfo->numPhysicalVgprsPerSimd =  1536; // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           =    10; // GPU__GC__NUM_WGP_PER_SA * 2;
        pChipInfo->numTccBlocks            =     8; // GPU__GC__NUM_GL2C;
        pChipInfo->gsVgtTableDepth         =    32; // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       =  1792; // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        =    32; // GPU__GC__NUM_MAX_GS_THDS;
    }
#endif
    else
    {
        // Unknown device id
        PAL_ASSERT_ALWAYS();
    }

    if (IsGfx103Plus(pChipProps->gfxLevel))
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

    PAL_ASSERT(pChipInfo->numCuPerSh <= 32);      // avoid overflow in activeWgpMask
    PAL_ASSERT((pChipInfo->numCuPerSh & 1) == 0); // CUs come in WGP pairs

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

// =====================================================================================================================
// Helper method which initializes the GPU chip properties for all hardware families using the GFX9 hardware layer.
void Device::InitGfx9ChipProperties()
{
    // Call into the HWL to initialize the default values for many properties of the hardware (based on chip ID).
    Gfx9::InitializeGpuChipProperties(GetPlatform(), UINT_MAX, &m_chipProperties);

    // Fill in properties that would normally get pulled from the KMD
    FillGfx9ChipProperties(&m_chipProperties, GetPlatform()->PlatformSettings());

    // Call into the HWL to finish initializing some GPU properties which can be derived from the ones which we
    // overrode above.
    Gfx9::FinalizeGpuChipProperties(*this, &m_chipProperties);
}

#if PAL_BUILD_GFX12
// =====================================================================================================================
// Helper method which initializes the GPU chip properties for all hardware families using the Gfx12 hardware layer.
void Device::FillGfx12ChipProperties(
    GpuChipProperties*         pChipProps,
    const PalPlatformSettings& platformSettings)
{
    auto*const   pChipInfo = &pChipProps->gfx9;
    const uint32 familyId  = pChipProps->familyId;
    const uint32 eRevId    = pChipProps->eRevId;

    pChipProps->pfpUcodeVersion = platformSettings.spoofPfpUcodeVersion;
    pChipProps->mecUcodeVersion = platformSettings.spoofMecUcodeVersion;

#if PAL_BUILD_NAVI48
    if (AMDGPU_IS_NAVI48(familyId, eRevId))
    {
        pChipInfo->supportSpiPrefPriority  = 1;
        pChipInfo->doubleOffchipLdsBuffers = 1;
        pChipInfo->gbAddrConfig            = 0x0044; // GB_ADDR_CONFIG_DEFAULT;
        pChipInfo->numShaderEngines        = 4;      // GPU__GC__NUM_SE;
        pChipInfo->numShaderArrays         = 2;      // GPU__GC__NUM_SA_PER_SE
        pChipInfo->maxNumRbPerSe           = 4;      // GPU__GC__NUM_RB_PER_SE;
        pChipInfo->nativeWavefrontSize     = 32;     // GPU__GC__SQ_WAVE_SIZE;
        pChipInfo->minWavefrontSize        = 32;
        pChipInfo->maxWavefrontSize        = 64;
        pChipInfo->numPhysicalVgprsPerSimd = 1536;   // GPU__GC__NUM_GPRS;
        pChipInfo->maxNumCuPerSh           = 8;      // GPU__GC__NUM_WGP_PER_SA * 2;
        pChipInfo->numTccBlocks            = 32;      // GPU__GC__NUM_GL2C;
        pChipInfo->gsVgtTableDepth         = 32;     // GPU__VGT__GS_TABLE_DEPTH;
        pChipInfo->gsPrimBufferDepth       = 1792;   // GPU__GC__GSPRIM_BUFF_DEPTH;
        pChipInfo->maxGsWavesPerVgt        = 32;     // GPU__GC__NUM_MAX_GS_THDS;
    }
    else
#endif
    {
        // Unknown device id
        PAL_ASSERT_ALWAYS();
    }

    pChipInfo->supportMeshShader = pChipInfo->supportImplicitPrimitiveShader;
    pChipInfo->supportTaskShader = true;

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

// =====================================================================================================================
void Device::InitGfx12ChipProperties()
{
    Gfx12::InitializeGpuChipProperties(GetPlatform(), &m_chipProperties);

    FillGfx12ChipProperties(&m_chipProperties, GetPlatform()->PlatformSettings());

    Gfx12::FinalizeGpuChipProperties(*this, &m_chipProperties);
}
#endif

// =====================================================================================================================
Result Device::EarlyInit(
    const HwIpLevels& ipLevels)
{
    Result result = Result::Success;

    m_chipProperties.familyId    = m_gpuInfo.familyId;
    m_chipProperties.deviceId    = m_gpuInfo.deviceId;
    m_chipProperties.eRevId      = m_gpuInfo.eRevId;
    m_chipProperties.revisionId  = m_gpuInfo.revisionId;
    m_chipProperties.gfxEngineId = m_gpuInfo.gfxEngineId;
    m_chipProperties.gpuIndex    = 0;

    m_chipProperties.gfxLevel         = IpTripleToGfxLevel(ipLevels.gfx);
    m_chipProperties.gfxTriple        = ipLevels.gfx;
    m_chipProperties.vcnLevel         = ipLevels.vcn;
    m_chipProperties.hwIpFlags.u32All = ipLevels.flags.u32All;

    for (uint32 i = 0; i < EngineTypeCount; i++)
    {
        m_engineProperties.perEngine[i].preferredCmdAllocHeaps[CommandDataAlloc]       = GpuHeapGartUswc;
        m_engineProperties.perEngine[i].preferredCmdAllocHeaps[EmbeddedDataAlloc]      = GpuHeapGartUswc;
        m_engineProperties.perEngine[i].preferredCmdAllocHeaps[LargeEmbeddedDataAlloc] = GpuHeapGartUswc;
        m_engineProperties.perEngine[i].preferredCmdAllocHeaps[GpuScratchMemAlloc]     = GpuHeapInvisible;
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

    switch (m_chipProperties.gfxTriple.major)
    {
    case 10:
    case 11:
        m_pFormatPropertiesTable = Gfx9::GetFormatPropertiesTable(m_chipProperties.gfxLevel,
                                                                  GetPlatform()->PlatformSettings());

        InitGfx9ChipProperties();
        Gfx9::InitializeGpuEngineProperties(m_chipProperties, &m_engineProperties);
        break;

#if PAL_BUILD_GFX12
    case 12:
        m_pFormatPropertiesTable = Gfx12::GetFormatPropertiesTable(m_chipProperties.gfxLevel);

        InitGfx12ChipProperties();
        Gfx12::InitializeGpuEngineProperties(m_chipProperties, &m_engineProperties);
        break;
#endif

    case 0:
        // No Graphics IP block found or recognized!
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
        result = m_settingFileMgr.Init(m_pSettingsPath);

        if (result == Result::ErrorUnavailable)
        {
            // Unavailable means that the file was not found, which is an acceptable failure.
            PAL_DPINFO("No settings file loaded.");
            result = Result::Success;
        }
    }

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
    m_chipProperties.gfxip.ceRamSize                = 48 * 1024; // 48kB

    // We don't support any presents in null-device mode
    for (uint32 idx = 0; idx < EngineTypeCount; ++idx)
    {
        auto*  pPerEngine = &m_engineProperties.perEngine[idx];

        // No GPU engines are supported by the null device.
        pPerEngine->numAvailable          = 0;
        pPerEngine->sizeAlignInDwords     = 1;
        pPerEngine->startAlign            = 1;
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

    m_heapProperties[GpuHeapLocal].logicalSize      = 1048576;
    m_heapProperties[GpuHeapLocal].physicalSize     = 1048576;
    m_memoryProperties.barSize                      = 1048576;
    m_heapProperties[GpuHeapInvisible].logicalSize  = 1048576;
    m_heapProperties[GpuHeapInvisible].physicalSize = 1048576;
    m_memoryProperties.nonLocalHeapSize             = 1048576;

    m_memoryProperties.flags.ptbInNonLocal              = 0;
    m_memoryProperties.flags.adjustVmRangeEscapeSupport = 0;

    m_memoryProperties.flags.virtualRemappingSupport = 1;
    m_memoryProperties.flags.pinningSupport          = 1;
    m_memoryProperties.flags.globalGpuVaSupport      = false;
    m_memoryProperties.flags.svmSupport              = 0;
    m_memoryProperties.flags.autoPrioritySupport     = 0;
    m_memoryProperties.flags.supportPageFaultInfo    = 0;

    m_memoryProperties.flags.iommuv2Support = 0;

    const uint32 vaRangeNumBits = m_chipProperties.gfxip.vaRangeNumBits;
    const Result result         = FixupUsableGpuVirtualAddressRange(
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
    return m_settingFileMgr.GetValue(pSettingName, valueType, pValue, bufferSz);
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

// =====================================================================================================================
// This is not supported for the NULL device.
Result Device::SetMlPowerOptimization(
    bool enableOptimization
    ) const
{
    PAL_NOT_IMPLEMENTED();

    return Result::Success;
}

} // NullDevice
} // Pal

#endif
