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

#include "core/cmdAllocator.h"
#include "core/cmdBuffer.h"
#include "core/device.h"
#include "core/engine.h"
#include "core/fence.h"
#include "core/gpuEvent.h"
#include "core/image.h"
#include "core/masterQueueSemaphore.h"
#include "core/openedQueueSemaphore.h"
#include "core/platform.h"
#include "core/queue.h"
#include "core/settingsLoader.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/ossip/ossDevice.h"
#include "core/addrMgr/addrMgr.h"
#include "core/svmMgr.h"
#include "palDequeImpl.h"
#include "palFormatInfo.h"
#include "palHashMapImpl.h"
#include "palIntrusiveListImpl.h"
#include "palIterator.h"
#include "palPipeline.h"
#if defined(__unix__)
#include "palSettingsFileMgrImpl.h"
#endif
#include "palSysUtil.h"
#include "palTextWriterImpl.h"
#include "palDepthStencilView.h"
#include "palGpuMemory.h"

#include <limits.h>

// Dev Driver includes
#include "msgChannel.h"
#include "devDriverServer.h"
#include "protocols/driverControlServer.h"
#include "protocols/rgpServer.h"

using namespace Util;
using namespace Util::Literals;

namespace Pal
{

// Translation table for obtaining memory ops per clock for a given Pal::LocalMemoryType.
static constexpr uint32 MemoryOpsPerClockTable[static_cast<uint32>(LocalMemoryType::Count)] =
{
    0,  // Unknown
    2,  // Ddr2
    2,  // Ddr3
    2,  // Ddr4
    4,  // Gddr5
    16, // Gddr6
    2,  // Hbm
    2,  // Hbm2
    2,  // Hbm3
    2,  // Lpddr4
    4,  // Lpddr5
    4   // Ddr5
};

// =====================================================================================================================
static void PAL_STDCALL DefaultCreateBufferViewSrds(
    const IDevice*        pDevice,
    uint32                count,
    const BufferViewInfo* pBufferViewInfo,
    void*                 pOut)
{
}

// =====================================================================================================================
static void PAL_STDCALL DefaultCreateImageViewSrds(
    const IDevice*       pDevice,
    uint32               count,
    const ImageViewInfo* pImgViewInfo,
    void*                pOut)
{
}

// =====================================================================================================================
static void PAL_STDCALL DefaultCreateFmaskViewSrds(
    const IDevice*       pDevice,
    uint32               count,
    const FmaskViewInfo* pFmaskViewInfo,
    void*                pOut)
{
}

// =====================================================================================================================
static void PAL_STDCALL DefaultCreateSamplerSrds(
    const IDevice*      pDevice,
    uint32              count,
    const SamplerInfo*  pSamplerInfo,
    void*               pOut)
{
}

// =====================================================================================================================
// Helper function that calculates memory ops per clock for a given memory type.
uint32 MemoryOpsPerClock(
    LocalMemoryType memoryType)
{
    return MemoryOpsPerClockTable[static_cast<uint32>(memoryType)];
}

// =====================================================================================================================
// Looks at the ATI family and revision ID's to determine the IP levels of each of the GPU's HWIP blocks. Return whether
// or not the GPU is actually supported by PAL.
bool Device::DetermineGpuIpLevels(
    uint32            familyId,       // Hardware family ID.  See GpuChipProperties::familyId.
    uint32            eRevId,         // Hardware revision ID.  See GpuChipProperties::eRevId.
    uint32            cpMicrocodeVersion,
    const Platform*   pPlatform,
    HwIpLevels*       pIpLevels)
{
    pIpLevels->gfx = GfxIpLevel::None;
    pIpLevels->oss = OssIpLevel::None;
    pIpLevels->vce = VceIpLevel::None;
    pIpLevels->uvd = UvdIpLevel::None;
    pIpLevels->vcn = VcnIpLevel::None;
    pIpLevels->flags.u32All = 0;

    pIpLevels->flags.isSpoofed = pPlatform->GpuIsSpoofed();
    bool emulationEnabled = (pPlatform->IsEmulationEnabled() ||
                             pPlatform->NullDeviceEnabled()  ||
                             pPlatform->GpuIsSpoofed());

    switch (familyId)
    {
    case FAMILY_POLARIS:
        pIpLevels->gfx = Gfx6::DetermineIpLevel(familyId, eRevId, cpMicrocodeVersion);
        break;
    case FAMILY_AI:
    case FAMILY_RV:
    case FAMILY_NV:
    case FAMILY_RMB:
    case FAMILY_RPL:
    case FAMILY_MDN:
#if PAL_BUILD_NAVI3X
    case FAMILY_NV3:
#endif
        pIpLevels->gfx = Gfx9::DetermineIpLevel(familyId, eRevId, cpMicrocodeVersion);
        break;

    default:
        break;
    }

#if PAL_BUILD_OSS
    switch (familyId)
    {
#if PAL_BUILD_OSS2_4
    case FAMILY_POLARIS:
        pIpLevels->oss = Oss2_4::DetermineIpLevel(familyId, eRevId);
        break;
#endif
#if PAL_BUILD_OSS4
    case FAMILY_AI:
    case FAMILY_RV:
        pIpLevels->oss = Oss4::DetermineIpLevel(familyId, eRevId);
        break;
#endif
    case FAMILY_NV:
        // GFX10 GPUs have moved the SDMA block into the GFX layer; there is no OSS layer
        // for this GPU.  The proper GFX layer for this family was determined above.
        break;
    case FAMILY_RMB:
        break;
    case FAMILY_RPL:
        break;
    case FAMILY_MDN:
        break;
#if PAL_BUILD_NAVI3X
    case FAMILY_NV3:
        break;
#endif
    default:
        break;
    }
#endif

    PAL_ALERT_MSG(pIpLevels->gfx == GfxIpLevel::None, "Unknown Gfx familyId:0x%x eRevId:0x%x", familyId, eRevId);

    // A GPU is considered supported by PAL if at least one of its hardware IP blocks is recognized.
    return ((pIpLevels->gfx != GfxIpLevel::None) || (pIpLevels->oss != OssIpLevel::None) ||
            (pIpLevels->vce != VceIpLevel::None) || (pIpLevels->uvd != UvdIpLevel::None) ||
            (pIpLevels->vcn != VcnIpLevel::None));
}

// Initial HashMap element size for referenced GPU memory allocations.
constexpr uint32 ReferencedMemoryMapElements = 2048;

// =====================================================================================================================
Device::Device(
    Platform*              pPlatform,
    uint32                 deviceIndex,
    uint32                 attachedScreenCount,
    size_t                 deviceSize,
    const HwIpDeviceSizes& hwDeviceSizes,
    uint32                 maxSemaphoreCount)
    :
    m_pPlatform(pPlatform),
    m_memMgr(this),
    m_connectedPrivateScreens(0),
    m_emulatedPrivateScreens(0),
    m_emulatedTargetId(UINT_MAX),
    m_attachedScreenCount(attachedScreenCount),
    m_pGfxDevice(nullptr),
    m_pOssDevice(nullptr),
    m_pTextWriter(nullptr),
    m_devDriverClientId(0),
    m_pFormatPropertiesTable(nullptr),
    m_deviceFinalized(false),
#if PAL_ENABLE_PRINTS_ASSERTS
    m_settingsCommitted(false),
    m_cmdBufDumpEnabledViaHotkey(false),
#endif
    m_force32BitVaSpace(pPlatform->Force32BitVaSpace()),
    m_disableSwapChainAcquireBeforeSignaling(false),
    m_pSettingsLoader(nullptr),
#if defined(__unix__)
    m_settingsMgr(SettingsFileName, pPlatform),
#endif
    m_dmaUploadRingLock(),
    m_pDmaUploadRing(nullptr),
    m_referencedGpuMem(ReferencedMemoryMapElements, pPlatform),
    m_staticVmidRefCount(0),
    m_referencedGpuMemLock(),
    m_pAddrMgr(nullptr),
    m_pTrackedCmdAllocator(nullptr),
    m_pUntrackedCmdAllocator(nullptr),
    m_deviceIndex(deviceIndex),
    m_deviceSize(deviceSize),
    m_hwDeviceSizes(hwDeviceSizes),
    m_maxSemaphoreCount(maxSemaphoreCount),
    m_frameCnt(0),
    m_texOptLevel(ImageTexOptLevel::Default),
    m_hdrColorspaceFormat(ScreenColorSpace::TfUndefined)
{
    memset(&m_finalizeInfo, 0, sizeof(m_finalizeInfo));
    memset(&m_privateScreenInfo[0], 0, sizeof(m_privateScreenInfo));
    memset(&m_pPrivateScreens[0], 0, sizeof(m_pPrivateScreens));
    memset(&m_pEmulatedPrivateScreens[0], 0, sizeof(m_pEmulatedPrivateScreens));
    memset(&m_memoryProperties, 0, sizeof(m_memoryProperties));
    memset(&m_engineProperties, 0, sizeof(m_engineProperties));
    memset(&m_queueProperties, 0, sizeof(m_queueProperties));
    memset(&m_chipProperties, 0, sizeof(m_chipProperties));
    memset(&m_heapProperties[0], 0, sizeof(m_heapProperties));
    memset(&m_pEngines[0], 0, sizeof(m_pEngines));
    memset(&m_pDummyCommandStreams[0], 0, sizeof(m_pDummyCommandStreams));
    memset(&m_gpuName[0], 0, sizeof(m_gpuName));
    memset(&m_flglState, 0, sizeof(m_flglState));
    memset(&m_flags, 0, sizeof(m_flags));
    memset(&m_bigSoftwareRelease, 0, sizeof(m_bigSoftwareRelease));
    memset(&m_virtualDisplayCaps, 0, sizeof(m_virtualDisplayCaps));
    memset(&m_cacheFilePath,      0, sizeof(m_cacheFilePath));
    memset(&m_debugFilePath,      0, sizeof(m_debugFilePath));
    memset(&m_referencedGpuMemBytes[0], 0, sizeof(m_referencedGpuMemBytes));
    memset(&m_hwsInfo, 0, sizeof(m_hwsInfo));
    memset(&m_publicSettings, 0, sizeof(m_publicSettings));
}

// =====================================================================================================================
Device::~Device()
{
    // If we're destroying the device, the client must have destroyed all of their queues.
    PAL_ASSERT(m_queues.IsEmpty());

    // These objects must be destroyed in Cleanup().
    for (uint32 i = 0; i < MaxPrivateScreens; i++)
    {
        PAL_ASSERT(m_pPrivateScreens[i] == nullptr);
    }

    PAL_ASSERT(m_pTrackedCmdAllocator == nullptr);
    PAL_ASSERT(m_pUntrackedCmdAllocator == nullptr);

    PAL_ASSERT(m_staticVmidRefCount == 0);

    if (m_pGfxDevice != nullptr)
    {
        m_pGfxDevice->Destroy();
        m_pGfxDevice = nullptr;
    }

    if (m_pOssDevice != nullptr)
    {
        m_pOssDevice->Destroy();
        m_pOssDevice = nullptr;
    }

    if (m_pAddrMgr != nullptr)
    {
        m_pAddrMgr->Destroy();
        m_pAddrMgr = nullptr;
    }

    PAL_SAFE_DELETE(m_pSettingsLoader, m_pPlatform);
}

// =====================================================================================================================
// This must clean up all internal GPU memory allocations and all objects created after EarlyInit. Note that EarlyInit
// is called when the platform creates the device objects so the work it does must be preserved if we are to reuse
// this device object.
Result Device::Cleanup()
{
    Result result = Result::Success;

    if (m_pDmaUploadRing != nullptr)
    {
        // It will call destructor of DmaUploadRing to free internal resources of m_pDmaUploadRing.
        PAL_SAFE_DELETE(m_pDmaUploadRing, m_pPlatform);
    }

    // If we're cleaning up the device, the client must have destroyed all of their queues.
    PAL_ASSERT(m_queues.IsEmpty());

    for (uint32 i = 0; i < MaxPrivateScreens; i++)
    {
        if (m_pPrivateScreens[i] != nullptr)
        {
            m_pPrivateScreens[i]->~PrivateScreen();
            PAL_SAFE_FREE(m_pPrivateScreens[i], GetPlatform());
        }
    }

    for (uint32 i = 0; i < MaxPrivateScreens; i++)
    {
        if (m_pEmulatedPrivateScreens[i] != nullptr)
        {
            m_pEmulatedPrivateScreens[i]->~PrivateScreen();
            PAL_SAFE_FREE(m_pEmulatedPrivateScreens[i], GetPlatform());
        }
    }

    m_connectedPrivateScreens = 0;

    if (m_pTextWriter != nullptr)
    {
        PAL_SAFE_DELETE(m_pTextWriter, m_pPlatform);
    }

    for (uint32 engineType = 0; engineType < EngineTypeCount; engineType++)
    {
        PAL_SAFE_DELETE(m_pDummyCommandStreams[engineType], m_pPlatform);
    }

    if (m_pGfxDevice != nullptr)
    {
        result = m_pGfxDevice->Cleanup();
    }

    if (m_pTrackedCmdAllocator != nullptr)
    {
        m_pTrackedCmdAllocator->DestroyInternal();
        m_pTrackedCmdAllocator = nullptr;
    }

    if (m_pUntrackedCmdAllocator != nullptr)
    {
        m_pUntrackedCmdAllocator->DestroyInternal();
        m_pUntrackedCmdAllocator = nullptr;
    }

    if ((m_staticVmidRefCount > 0) && SupportsStaticVmid())
    {
        result = OsSetStaticVmidMode(false);
        if (result == Result::Success)
        {
            m_staticVmidRefCount = 0;
        }
    }

    if (m_pageFaultDebugSrdMem.IsBound() && (result == Result::Success))
    {
        GpuMemory* gpuMemory = m_pageFaultDebugSrdMem.Memory();
        gpusize    virtAddr  = gpuMemory->Desc().gpuVirtAddr;
        gpusize    size      = gpuMemory->Desc().size;
        gpusize    virtSize  = Pow2Align(size, m_memoryProperties.virtualMemAllocGranularity);

        result = m_memMgr.FreeGpuMem(gpuMemory, m_pageFaultDebugSrdMem.Offset());
        m_pageFaultDebugSrdMem.Update(nullptr, 0);

        Result freeVaResult = FreeGpuVirtualAddress(virtAddr, virtSize);

        if (m_pPlatform->GetGpuMemoryEventProvider() != nullptr)
        {
            ResourceDestroyEventData destroyData = {};
            destroyData.pObj = &m_pageFaultDebugSrdMem;
            m_pPlatform->GetGpuMemoryEventProvider()->LogGpuMemoryResourceDestroyEvent(destroyData);
        }

        // An error here is not fatal, but it will likely prevent new debug SRDs from being allocated in new devices.
        PAL_ALERT(freeVaResult != Result::Success);
    }

    if (m_dummyChunkMem.IsBound() && (result == Result::Success))
    {
        result = m_memMgr.FreeGpuMem(m_dummyChunkMem.Memory(), m_dummyChunkMem.Offset());
        m_dummyChunkMem.Update(nullptr, 0);

        if (m_pPlatform->GetGpuMemoryEventProvider() != nullptr)
        {
            ResourceDestroyEventData destroyData = {};
            destroyData.pObj = &m_dummyChunkMem;
            m_pPlatform->GetGpuMemoryEventProvider()->LogGpuMemoryResourceDestroyEvent(destroyData);
        }
    }

    for (uint32 engineType = 0; engineType < EngineTypeCount; engineType++)
    {
        for (uint32 engineIdx = 0; engineIdx < MaxAvailableEngines; engineIdx++)
        {
            PAL_SAFE_DELETE(m_pEngines[engineType][engineIdx], m_pPlatform);
        }
    }

    // NOTE: Explicitly free all internal GPU memory. Any child object which needs to free GPU memory MUST be torn
    // down before this!
    m_memMgr.FreeAllocations();

    m_deviceFinalized   = false;
#if PAL_ENABLE_PRINTS_ASSERTS
    m_settingsCommitted = false;
#endif

    if (m_pPlatform->SvmModeEnabled() && (m_pPlatform->GetSvmRangeStart() != 0) &&
        (MemoryProperties().flags.iommuv2Support == 0))
    {
        auto*const  pVaRange   = &m_memoryProperties.vaRange[0];
        const auto& vaSvmRange = pVaRange[static_cast<uint32>(VaPartition::Svm)];

        if (vaSvmRange.baseVirtAddr != 0)
        {
            result = VirtualRelease(reinterpret_cast<void*>(vaSvmRange.baseVirtAddr),
                                    static_cast<size_t>(vaSvmRange.size));
            m_pPlatform->SetSvmRangeStart(0);
        }
    }

    return result;
}

// =====================================================================================================================
// Performs early initialization of this device, which involves initializating the device properties.
Result Device::EarlyInit(
    const HwIpLevels& ipLevels)
{
    Result result = m_referencedGpuMem.Init();

    if (result == Result::Success)
    {
        result = OsEarlyInit();
    }

    if (result == Result::Success)
    {
        result = HwlEarlyInit();
    }

    if (result == Result::Success)
    {
        // Unlike all other properties, these must be initialized after HwlEarlyInit because they come from AddrLib.
        m_chipProperties.imageProperties.numSwizzleEqs = static_cast<uint8>(m_pAddrMgr->NumSwizzleEquations());
        m_chipProperties.imageProperties.pSwizzleEqs   = m_pAddrMgr->SwizzleEquations();
    }

    return result;
}

// =====================================================================================================================
Result Device::SetupPublicSettingDefaults()
{
    const GfxIpLevel gfxLevel = ChipProperties().gfxLevel;
    Result ret = Result::Success;

    m_publicSettings.fastDepthStencilClearMode                = FastDepthStencilClearMode::Default;
    m_publicSettings.forceLoadObjectFailure                   = false;
    m_publicSettings.distributionTessMode                     = DistributionTessDefault;
    m_publicSettings.contextRollOptimizationFlags             = 0;
    m_publicSettings.unboundDescriptorDebugSrdCount           = 1;
    m_publicSettings.disableResourceProcessingManager         = false;
    m_publicSettings.tcCompatibleMetaData                     = 0x7F;
    m_publicSettings.cpDmaCmdCopyMemoryMaxBytes               = 64_KiB;
    m_publicSettings.forceHighClocks                          = false;
    m_publicSettings.cmdBufBatchedSubmitChainLimit            = 128;
    m_publicSettings.cmdAllocResidency                        = 0xF;
    m_publicSettings.presentableImageNumberThreshold          = 16;
    m_publicSettings.hintInvariantDepthStencilClearValues     = false;
    m_publicSettings.hintDisableSmallSurfColorCompressionSize = 128;
    m_publicSettings.disableEscapeCall                        = false;
    m_publicSettings.longRunningSubmissions                   = false;
    m_publicSettings.borderColorPaletteSizeLimit              = 4096;
    m_publicSettings.disableCommandBufferPreemption           = false;
    m_publicSettings.disableSkipFceOptimization               = false;
    m_publicSettings.dccBitsPerPixelThreshold                 = UINT_MAX;
    m_publicSettings.largePageMinSizeForVaAlignmentInBytes    =
        m_memoryProperties.largePageSupport.minSurfaceSizeForAlignmentInBytes;
    m_publicSettings.largePageMinSizeForSizeAlignmentInBytes  =
        m_memoryProperties.largePageSupport.minSurfaceSizeForAlignmentInBytes;
    m_publicSettings.miscellaneousDebugString[0]              = '\0';
    m_publicSettings.renderedByString[0]                      = '\0';
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 727
    m_publicSettings.useAcqRelInterface                       = false;
#endif
    m_publicSettings.zeroUnboundDescDebugSrd                  = false;
    m_publicSettings.pipelinePreferredHeap                    = HasLargeLocalHeap() ? GpuHeap::GpuHeapLocal
                                                                                    : GpuHeap::GpuHeapInvisible;
    m_publicSettings.depthClampBasedOnZExport                 = true;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 743
    m_publicSettings.forceWaitPointPreColorToPostIndexFetch   = false;
#else
    m_publicSettings.forceWaitPointPreColorToPostPrefetch     = false;
#endif
    m_publicSettings.enableExecuteIndirectPacket              = false;
    m_publicSettings.disableExecuteIndirectAceOffload         = false;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 706
    m_publicSettings.dccInitialClearKind                      = static_cast<uint32>(DccInitialClearKind::Uncompressed);
#endif
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 713
    m_publicSettings.disableInternalVrsImage                  = false;
#endif
#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 716) && (PAL_CLIENT_INTERFACE_MAJOR_VERSION < 719)
    m_publicSettings.memMgrPoolAllocationSizeInBytes          = 0;
#endif
#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 744)
    m_publicSettings.binningContextStatesPerBin               = 0;
    m_publicSettings.binningPersistentStatesPerBin            = 0;
#endif
#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 749)
#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 753)
    m_publicSettings.disableBinningPsKill                     = OverrideMode::Default;
#else
    m_publicSettings.disableBinningPsKill                     = DisableBinningPsKill::Default;
#endif
#endif
#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION < 755)
    m_publicSettings.isolineDistributionFactor                =  12;
    m_publicSettings.triDistributionFactor                    =  30;
    m_publicSettings.quadDistributionFactor                   =  24;
    m_publicSettings.donutDistributionFactor                  =  24;
    m_publicSettings.trapezoidDistributionFactor              =   6;
#endif
    m_publicSettings.nggLateAllocGs                           = 127;
    m_publicSettings.rpmViewsBypassMall                       = RpmViewsBypassMallOff;
    m_publicSettings.expandHiZRangeForResummarize             = false;

    if (false
#if PAL_BUILD_GFX11
        || IsGfx11(gfxLevel)
#endif
        )
    {
        m_publicSettings.optDepthOnlyExportRate    = true;
    }
    else
    {
        m_publicSettings.optDepthOnlyExportRate    = false;
    }

#if PAL_BUILD_GFX11
#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION < 777)
    m_publicSettings.gfx11SampleMaskTrackerWatermark = 0;
#endif
#endif

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 790
    m_publicSettings.limitCbFetch256B = false;
#endif

    m_publicSettings.binningMode            = DeferredBatchBinAccurate;
    m_publicSettings.customBatchBinSize     = 0x800080;
    m_publicSettings.binningMaxPrimPerBatch = 1024;

    m_publicSettings.pwsMode = PwsMode::Enabled;

    m_publicSettings.maxScratchRingSizeBaseline = 268435456;
    m_publicSettings.maxScratchRingSizeScalePct = 10;

    return ret;
}

// =====================================================================================================================
// Helper function to create a sub-device for each present hardware IP.
Result Device::HwlEarlyInit()
{
    void* pCurrPlacementAddr            = VoidPtrInc(this, m_deviceSize);
    void*const pGfxPlacementAddr        = pCurrPlacementAddr;
    pCurrPlacementAddr                  = VoidPtrInc(pGfxPlacementAddr, m_hwDeviceSizes.gfx);
    void*const pOssPlacementAddr        = pCurrPlacementAddr;
    pCurrPlacementAddr                  = VoidPtrInc(pOssPlacementAddr, m_hwDeviceSizes.oss);
    void*const pAddrMgrPlacementAddr    = pCurrPlacementAddr;

    Result result = Result::Success;

    DeviceInterfacePfnTable pfnTable = {};
    pfnTable.pfnCreateTypedBufViewSrds   = &DefaultCreateBufferViewSrds;
    pfnTable.pfnCreateUntypedBufViewSrds = &DefaultCreateBufferViewSrds;
    pfnTable.pfnCreateImageViewSrds      = &DefaultCreateImageViewSrds;
    pfnTable.pfnCreateFmaskViewSrds      = &DefaultCreateFmaskViewSrds;
    pfnTable.pfnCreateSamplerSrds        = &DefaultCreateSamplerSrds;

#if PAL_BUILD_GFX
    switch (ChipProperties().gfxLevel)
    {
    case GfxIpLevel::GfxIp6:
    case GfxIpLevel::GfxIp7:
    case GfxIpLevel::GfxIp8:
    case GfxIpLevel::GfxIp8_1:
        result = Gfx6::CreateDevice(this, pGfxPlacementAddr, &pfnTable, &m_pGfxDevice);
        break;
    case GfxIpLevel::GfxIp9:
    case GfxIpLevel::GfxIp10_1:
    case GfxIpLevel::GfxIp10_3:
#if PAL_BUILD_GFX11
    case GfxIpLevel::GfxIp11_0:
#endif
        result = Gfx9::CreateDevice(this, pGfxPlacementAddr, &pfnTable, &m_pGfxDevice);
        break;
    default:
        PAL_ASSERT(m_hwDeviceSizes.gfx == 0);
        break;
    }
#endif

    if ((result == Result::Success) && (m_pGfxDevice != nullptr))
    {
        PAL_ASSERT(m_pSettingsLoader != nullptr);
        result = m_pGfxDevice->InitHwlSettings(m_pSettingsLoader->GetSettingsPtr());
    }

#if PAL_BUILD_OSS
    if (result == Result::Success)
    {
        switch (ChipProperties().ossLevel)
        {
#if PAL_BUILD_OSS2_4
        case OssIpLevel::OssIp2_4:
            result = Oss2_4::CreateDevice(this, pOssPlacementAddr, &m_pOssDevice);
            break;
#endif
#if PAL_BUILD_OSS4
        case OssIpLevel::OssIp4:
            result = Oss4::CreateDevice(this, pOssPlacementAddr, &m_pOssDevice);
            break;
#endif
        default:
            PAL_ASSERT(m_hwDeviceSizes.oss == 0);
            break;
        }
    }
#endif

    if (result == Result::Success)
    {
        if ((ChipProperties().gfxLevel < GfxIpLevel::GfxIp9) &&
            (ChipProperties().ossLevel < OssIpLevel::OssIp4))
        {
            result = AddrMgr1::Create(this, pAddrMgrPlacementAddr, &m_pAddrMgr);
        }

        else
        {
            result = AddrMgr2::Create(this, pAddrMgrPlacementAddr, &m_pAddrMgr);
        }
    }

    // Store the function pointers for various functionality.
    if (result == Result::Success)
    {
        m_pfnTable.pfnCreateTypedBufViewSrds   = pfnTable.pfnCreateTypedBufViewSrds;
        m_pfnTable.pfnCreateUntypedBufViewSrds = pfnTable.pfnCreateUntypedBufViewSrds;
        m_pfnTable.pfnCreateImageViewSrds      = pfnTable.pfnCreateImageViewSrds;
        m_pfnTable.pfnCreateFmaskViewSrds      = pfnTable.pfnCreateFmaskViewSrds;
        m_pfnTable.pfnCreateSamplerSrds        = pfnTable.pfnCreateSamplerSrds;
        m_pfnTable.pfnCreateBvhSrds            = pfnTable.pfnCreateBvhSrds;
    }

    return result;
}

// =====================================================================================================================
// Calculates the performance rating for the GPU's engine and memory
void Device::InitPerformanceRatings()
{
    // Performance rating denominator.
    constexpr uint64 PerfRatingDenominator = 100;

    // CU performance multiplier.
    constexpr float DGpuCuPerfMultiplier = 1.15f;
    constexpr float IGpuCuPerfMultiplier = 1.0f;

    // Compute engine performance rating
    const float cuMultiplier  = (m_chipProperties.gpuType == GpuType::Integrated) ? IGpuCuPerfMultiplier
                                                                                  : DGpuCuPerfMultiplier;
    uint32 simdWidthMultiplier = 16;
    uint32 numShaderEngines    = 0;
    uint32 numShaderArrays     = 1;
    uint32 numCuPerSh          = 0;
    uint32 numSimdPerCu        = 0;
    uint32 numWavesPerSimd     = 0;

#if PAL_BUILD_GFX
    switch (m_chipProperties.gfxLevel)
    {
        case GfxIpLevel::GfxIp6:
        case GfxIpLevel::GfxIp7:
        case GfxIpLevel::GfxIp8:
        case GfxIpLevel::GfxIp8_1:
            numShaderEngines = m_chipProperties.gfx6.numShaderEngines;
            numShaderArrays  = m_chipProperties.gfx6.numShaderArrays;
            numCuPerSh       = m_chipProperties.gfx6.numCuPerSh;
            numSimdPerCu     = m_chipProperties.gfx6.numSimdPerCu;
            numWavesPerSimd  = m_chipProperties.gfx6.numWavesPerSimd;
            break;
        case GfxIpLevel::GfxIp9:
            numShaderEngines = m_chipProperties.gfx9.numShaderEngines;
            numShaderArrays  = m_chipProperties.gfx9.numShaderArrays;
            numCuPerSh       = m_chipProperties.gfx9.numCuPerSh;
            numSimdPerCu     = m_chipProperties.gfx9.numSimdPerCu;
            numWavesPerSimd  = m_chipProperties.gfx9.numWavesPerSimd;
            break;
        case GfxIpLevel::GfxIp10_1:
        case GfxIpLevel::GfxIp10_3:
#if PAL_BUILD_GFX11
        case GfxIpLevel::GfxIp11_0:
#endif
            simdWidthMultiplier = 32;
            numShaderEngines    = m_chipProperties.gfx9.numShaderEngines;
            numShaderArrays     = m_chipProperties.gfx9.numShaderArrays;
            numCuPerSh          = m_chipProperties.gfx9.numCuPerSh;
            numSimdPerCu        = m_chipProperties.gfx9.numSimdPerCu;
            numWavesPerSimd     = m_chipProperties.gfx9.numWavesPerSimd;
            break;
        case GfxIpLevel::None:
            // No Graphics IP block found or recognized!
        default:
            break;
    }
#endif

    const uint64 numSimdWaveSlots     = numShaderEngines *
                                        numShaderArrays  *
                                        numCuPerSh       *
                                        numSimdPerCu     *
                                        numWavesPerSimd;
    const uint64 simdPerf             = static_cast<uint64>(numSimdWaveSlots * simdWidthMultiplier * cuMultiplier);
    const uint64 maxEngineClock       = m_chipProperties.maxEngineClock;
    m_chipProperties.enginePerfRating = static_cast<uint32>((simdPerf * maxEngineClock) / PerfRatingDenominator);

    uint32 memoryPerfValue = m_chipProperties.maxMemoryClock    *
                             m_memoryProperties.vramBusBitWidth *
                             m_memoryProperties.memOpsPerClock;

    if (m_chipProperties.gpuType == GpuType::Integrated)
    {
        // APU shares memory bandwidth between its GPU and CPU, therefore must reduce
        memoryPerfValue = (memoryPerfValue * m_memoryProperties.apuBandwidthFactor) / PerfRatingDenominator;

    }

    m_chipProperties.memoryPerfRating = memoryPerfValue;
}

// =====================================================================================================================
// Initializes the properties for GPU memory heaps:
//  GpuHeapLocal (partial)
//  GpuHeapInvisible (partial)
//  GpuHeapGartCacheable
//  GpuHeapGartUswc
// Derived devices are expected to fill out the logical, physical, and bar sizes for GpuHeapLocal and GpuHeapInvisible.
void Device::InitMemoryHeapProperties()
{
    for (uint32 i = 0; i < GpuHeapCount; ++i)
    {
        m_heapProperties[i].flags.u32All = 0;

        switch (static_cast<GpuHeap>(i))
        {
        case GpuHeapLocal:
            m_heapProperties[i].flags.cpuVisible       = 1;
            m_heapProperties[i].flags.cpuGpuCoherent   = 1;
            m_heapProperties[i].flags.cpuUncached      = 1;
            m_heapProperties[i].flags.cpuWriteCombined = 1;
            break;
        case GpuHeapInvisible:
            m_heapProperties[i].flags.cpuUncached = 1;
            break;
        case GpuHeapGartCacheable:
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 766
            m_heapProperties[i].logicalSize  = m_memoryProperties.nonLocalHeapSize;
            m_heapProperties[i].physicalSize = m_memoryProperties.nonLocalHeapSize;
#else
            m_heapProperties[i].heapSize         = m_memoryProperties.nonLocalHeapSize;
            m_heapProperties[i].physicalHeapSize = m_memoryProperties.nonLocalHeapSize;
#endif
            m_heapProperties[i].flags.cpuVisible     = 1;
            m_heapProperties[i].flags.cpuGpuCoherent = 1;
            m_heapProperties[i].flags.holdsPinned    = 1;
            m_heapProperties[i].flags.shareable      = 1;
            break;
        case GpuHeapGartUswc:
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 766
            m_heapProperties[i].logicalSize  = m_memoryProperties.nonLocalHeapSize;
            m_heapProperties[i].physicalSize = m_memoryProperties.nonLocalHeapSize;
#else
            m_heapProperties[i].heapSize         = m_memoryProperties.nonLocalHeapSize;
            m_heapProperties[i].physicalHeapSize = m_memoryProperties.nonLocalHeapSize;
#endif
            m_heapProperties[i].flags.cpuVisible       = 1;
            m_heapProperties[i].flags.cpuGpuCoherent   = 1;
            m_heapProperties[i].flags.cpuUncached      = 1;
            m_heapProperties[i].flags.cpuWriteCombined = 1;
            m_heapProperties[i].flags.shareable        = 1;
            break;
        default:
            PAL_ASSERT_ALWAYS();
            break;
        }
    }
}

// =====================================================================================================================
// Initializes the Pal setting structures
Result Device::InitSettings()
{
    Result ret = Result::Success;

    // Make sure we only initialize settings once
    if (m_pSettingsLoader == nullptr)
    {
        m_pSettingsLoader = PAL_NEW(Pal::SettingsLoader, GetPlatform(), AllocInternal)(this);

        if (m_pSettingsLoader == nullptr)
        {
            ret = Result::ErrorOutOfMemory;
        }
        else
        {
            ret = m_pSettingsLoader->Init();
        }

        // If the regular settings initialize successfully, then initialize the public settings too.
        if (ret == Result::Success)
        {
            ret = SetupPublicSettingDefaults();
        }
    }

    return ret;
}

// =====================================================================================================================
// Initializes the size of each HWIP blocks' private Device object.
void Device::GetHwIpDeviceSizes(
    const HwIpLevels& ipLevels,
    HwIpDeviceSizes*  pHwDeviceSizes,
    size_t*           pAddrMgrSize)
{
    size_t  gfxAddrMgrSize = 0;
    size_t  ossAddrMgrSize = 0;
    size_t  maxAddrMgrSize = 0;

    PAL_ASSERT((pHwDeviceSizes != nullptr) && (pAddrMgrSize != nullptr));

    switch (ipLevels.gfx)
    {
    case GfxIpLevel::GfxIp6:
    case GfxIpLevel::GfxIp7:
    case GfxIpLevel::GfxIp8:
    case GfxIpLevel::GfxIp8_1:
        pHwDeviceSizes->gfx = Gfx6::GetDeviceSize();
        gfxAddrMgrSize      = AddrMgr1::GetSize();
        break;
    case GfxIpLevel::GfxIp9:
    case GfxIpLevel::GfxIp10_1:
    case GfxIpLevel::GfxIp10_3:
#if PAL_BUILD_GFX11
    case GfxIpLevel::GfxIp11_0:
#endif
        pHwDeviceSizes->gfx = Gfx9::GetDeviceSize(ipLevels.gfx);
        gfxAddrMgrSize      = AddrMgr2::GetSize();
        break;
    default:
        break;
    }

#if PAL_BUILD_OSS
    switch (ipLevels.oss)
    {
#if PAL_BUILD_OSS2_4
    case OssIpLevel::OssIp2_4:
        pHwDeviceSizes->oss = Oss2_4::GetDeviceSize();
        ossAddrMgrSize      = AddrMgr1::GetSize();
        break;
#endif
#if PAL_BUILD_OSS4
    case OssIpLevel::OssIp4:
        pHwDeviceSizes->oss = Oss4::GetDeviceSize();
        ossAddrMgrSize      = AddrMgr2::GetSize();
        break;
#endif
    default:
        break;
    }
#endif

    maxAddrMgrSize = Max(gfxAddrMgrSize, ossAddrMgrSize);

    // Not having a block should be ok, but if a block exists, they all better be
    // using the same size address manager.
    PAL_ASSERT ((gfxAddrMgrSize == 0) || (gfxAddrMgrSize == maxAddrMgrSize));
    PAL_ASSERT ((ossAddrMgrSize == 0) || (ossAddrMgrSize == maxAddrMgrSize));

    *pAddrMgrSize = maxAddrMgrSize;
}

// =====================================================================================================================
// Acquire or release a static VMID. It is illegal to disable/release a static VMID without a corresponding prior
// enable/acquire.
Result Device::SetStaticVmidMode(
    bool enable)
{
    Result result = Result::Unsupported;

    if (SupportsStaticVmid())
    {
        bool enabledCurrently = m_staticVmidRefCount > 0;
        if ((enabledCurrently == false) && (enable == false))
        {
            // Prevent an underflow and erroneous release
            result = Result::ErrorInvalidValue;
        }
        else
        {
            m_staticVmidRefCount += enable ? 1 : -1;

            const bool enabledAfter = m_staticVmidRefCount > 0;
            if (enabledCurrently == enabledAfter)
            {
                // We already have a static VMID active/disabled
                result = Result::Success;
            }
            else
            {
                result = OsSetStaticVmidMode(enable);
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Find a gpu VA range with size(vaSize) and base address between *pVaStart and (vaEnd - vaSize) that is not reserved.
// It tries vaEnd-vaSize to vaEnd first, then vaEnd-vaSize-vaAlignment to vaEnd-vaAlignment. It's inverted order of
// FindGpuVaRange.
// If successfull, *pVaStart will be set to the base address of the found VA range.
Result Device::FindGpuVaRangeReverse(
    gpusize*    pVaStart,
    gpusize     vaEnd,
    gpusize     vaSize,
    gpusize     vaAlignment,
    VaPartition vaParttion
    ) const
{
    Result result = Result::ErrorOutOfGpuMemory;

    PAL_ASSERT(IsPowerOfTwo(vaAlignment));
    PAL_ASSERT(vaEnd - *pVaStart >= vaSize);

    *pVaStart = Pow2Align(*pVaStart, vaAlignment);
    vaEnd     = Pow2AlignDown(vaEnd, vaAlignment);

    for (gpusize vaAddr = (vaEnd - vaSize); vaAddr >= *pVaStart; vaAddr -= vaAlignment)
    {
        result = ProbeGpuVaRange(vaAddr, vaSize, vaParttion);

        if (result == Result::Success)
        {
            *pVaStart = vaAddr;
            break;
        }
    }

    return result;
}

// =====================================================================================================================
// Find a gpu VA range with size(vaSize) and base address between *pVaStart and (vaEnd - vaSize) that is not reserved.
// If reserveCpuVa is true, the equivalent VA range in host memory will tried to be reserved.
// If successfull, *pVaStart will be set to the base address of the found VA range.
Result Device::FindGpuVaRange(
    gpusize*    pVaStart,
    gpusize     vaEnd,
    gpusize     vaSize,
    gpusize     vaAlignment,
    VaPartition vaParttion,
    bool        reserveCpuVa
    ) const
{
    Result result = Result::Success;

    PAL_ASSERT(IsPowerOfTwo(vaAlignment));
    *pVaStart = Pow2Align(*pVaStart, vaAlignment);

    for (gpusize vaAddr = *pVaStart; vaAddr <= (vaEnd - vaSize); vaAddr += vaAlignment)
    {
        gpusize vaAllocated = 0u;
        result = Result::Success;

        if (reserveCpuVa)
        {
            result = VirtualReserve(static_cast<size_t>(vaSize),
                                    reinterpret_cast<void**>(&vaAllocated),
                                    reinterpret_cast<void*>(vaAddr));

            // Make sure we get the address that we requested
            if (vaAllocated != vaAddr)
            {
                result = Result::ErrorOutOfMemory;
            }
        }

        if (result == Result::Success)
        {
            result = ProbeGpuVaRange(vaAddr, vaSize, vaParttion);

            if (result == Result::Success)
            {
                *pVaStart = vaAddr;
                break;
            }
        }

        // If the gpu VA range wasn't available, free it on the host side
        if (reserveCpuVa)
        {
            VirtualRelease(reinterpret_cast<void*>(vaAllocated),
                           static_cast<size_t>(vaSize));
        }
    }

    return result;
}

// =====================================================================================================================
// Uses the GPU's excluded virtual address ranges to clamp the "usable" portion of the address space
Result Device::FixupUsableGpuVirtualAddressRange(
    uint32 vaRangeNumBits)
{
    /*  The overall GPU's virtual address space contains ranges of addresses which are excluded from user access. This
     *  partitioning between kernel and user ranges is done *very* differently on Windows vs. Linux: (see below)
     *
     *  On Windows, there is typically one small (a few KB) excluded range, at the bottom of the address space, like
     *  this:
     *    +-+---------------------------------------------------------+
     *  0 |x|                                                         | 1 TB
     *    +-+---------------------------------------------------------+
     *                             ^ User-Usable Range
     *
     *  On Linux, CMM/QS typically reports two excluded ranges: one very large one (hundreds of GB) at the bottom of
     *  the address space, and another (somewhat large, ~16 GB) at the top of the address space, like this:
     *    +-----------------------------------------------+-----+-----+
     *  0 |xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|     |xxxxx| 1 TB
     *    +-----------------------------------------------+-----+-----+
     *                                                       ^ User-Usable Range
     *
     *  To prevent GPU devices from allocating too many page table blocks, we artificially limit the virtual address
     *  range which is user-accessible when initializing the Device's address manager. Employing this cap blindly on
     *  Linux would result in the entire user-accessible region of address space being cut off, which renders virtual
     *  address mapping impossible. (Since all valid addresses would lie in an excluded range!!)
     *
     *  Rather than have a Linux-specific workaround which changes the default address limit, the code below will use
     *  the excluded VA range data to clamp the address space as best we can to our ideal limit without eliminating
     *  all of the user-usable address range (which later gets reported to VAM).
     */
    Result result = Result::Success;
    gpusize usableVaStart = m_memoryProperties.vaStart;
    gpusize usableVaEnd   = m_memoryProperties.vaEnd;

    for (size_t idx = 0; idx < m_memoryProperties.numExcludedVaRanges; ++idx)
    {
        const gpusize vaStart = m_memoryProperties.excludedRange[idx].baseVirtAddr;
        const gpusize vaEnd   = (vaStart + m_memoryProperties.excludedRange[idx].size);

        if ((vaStart <= usableVaStart) && (vaEnd > usableVaStart))
        {
            // This excluded range overlaps the beginning of the user-usable range: restrict the start of the user-
            // usable range to the end of this excluded range.
            PAL_ASSERT(vaEnd < usableVaEnd);
            usableVaStart = vaEnd;
        }
        else if ((vaEnd >= usableVaEnd) && (vaStart < usableVaEnd))
        {
            // This excluded range overlaps the end of the user-usable range: restrict the end of the user-usable
            // range to the beginning of this excluded range.
            PAL_ASSERT(vaStart > usableVaStart);
            usableVaEnd = vaStart;
        }
    }

    // Compute the maximum number of bits we'll allow into the GPU address range: it is one more bit than we need
    // to represent the start of the "usable" address space. This limit should be at least 36 bits, but never be
    // larger than the number of bits necessary to represent the "true" GPU virtual address limit.
    const uint32 usableVaRangeBitLimit = Max(vaRangeNumBits, (Log2(usableVaStart) + 1));

    PAL_ASSERT((m_force32BitVaSpace || (usableVaRangeBitLimit >= MinVaRangeNumBits)) &&
               (usableVaRangeBitLimit <= m_chipProperties.gfxip.vaRangeNumBits));

    // Get the highest possible virtual address limit.  Don't let virtual addresses go beyond the specified end
    // or beyond what our bit range allows us to access.
    const gpusize maxPossibleVirtualAddr = (1ull << usableVaRangeBitLimit) - 1;
    m_memoryProperties.vaUsableEnd = Min(m_memoryProperties.vaEnd, maxPossibleVirtualAddr);

    if (m_memoryProperties.flags.resizeableVaRange == 0)
    {
        // If the GPU doesn't support resizing the page directory, we can safely clamp the virtual address range
        // we report to the memory manager to the "usable" end we just computed.
        m_memoryProperties.vaEnd        = Min(m_memoryProperties.vaEnd, m_memoryProperties.vaUsableEnd);
        m_memoryProperties.vaInitialEnd = m_memoryProperties.vaEnd;

        usableVaEnd = Min(usableVaEnd, m_memoryProperties.vaEnd);
    }

    // Align to the fragment size. Note: usableVaEnd will contain +1, thus useable size calculation doesn't require +1
    usableVaStart = RoundUpToMultiple(usableVaStart, m_memoryProperties.fragmentSize);
    usableVaEnd   = RoundDownToMultiple(usableVaEnd + 1, m_memoryProperties.fragmentSize);

    /*  The whole usable GPU virtual address range is partitioned into several sub-ranges, to allow certain items
     *  such as descriptor tables be addressed on the GPU using a 32bit address.
     *
     *  This is accomplished by creating a sub-range of GPU virtual address space for descriptor tables which is only
     *  4GB in size and aligned to a 4GB base address: thus, we can know the upper 32 bits of the full address at a
     *  shader's compilation time and give that information to SC.
     *
     *  (To prevent us from using really high virtual addresses unless we run out of lower addresses, we'll carve out
     *   the non-Default partitions from the lowest possible addresses.)
     */
    constexpr gpusize _1GB  = (1uLL << 30u);
    constexpr gpusize _4GB  = (1ull << 32u);
    constexpr gpusize _16GB = (1ull << 34u);

    auto*const pVaRange = &m_memoryProperties.vaRange[0];
    if ((usableVaEnd - usableVaStart) >= (7ull * _4GB))
    {
        // Case #1
        // This is the ideal scenario: we have more than 28 GB of address space, so we can use the first two 4 GB
        // sections for the ShadowDescriptorTable and DescriptorTable ranges, and the last 16GB for Capture Replay,
        // the leftovers for Default.
        gpusize baseVirtAddr = usableVaStart;

        result = FindGpuVaRange(&baseVirtAddr, usableVaEnd, _4GB, _4GB, VaPartition::DescriptorTable);

        if (result == Result::Success)
        {
            pVaRange[static_cast<uint32>(VaPartition::DescriptorTable)].baseVirtAddr = baseVirtAddr;
            pVaRange[static_cast<uint32>(VaPartition::DescriptorTable)].size = _4GB;
        }

        baseVirtAddr += _4GB;

        // Reserve VA range for ShadowDescriptorTable only if fmask SRDs are supported
        if (m_chipProperties.srdSizes.fmaskView > 0)
        {
            result = FindGpuVaRange(&baseVirtAddr, usableVaEnd, _4GB, _4GB, VaPartition::ShadowDescriptorTable);

            if (result == Result::Success)
            {
                pVaRange[static_cast<uint32>(VaPartition::ShadowDescriptorTable)].baseVirtAddr = baseVirtAddr;
                pVaRange[static_cast<uint32>(VaPartition::ShadowDescriptorTable)].size = _4GB;
            }

            baseVirtAddr += _4GB;
        }

        if (result == Result::Success)
        {
            pVaRange[static_cast<uint32>(VaPartition::Default)].baseVirtAddr = baseVirtAddr;
            pVaRange[static_cast<uint32>(VaPartition::Default)].size         = (usableVaEnd - baseVirtAddr);

            if (result == Result::Success)
            {
                if (m_memoryProperties.vaStartPrt > 0)
                {
                    if (m_chipProperties.gfxip.supportCaptureReplay == 1)
                    {
                        result = FindGpuVaRangeReverse(&baseVirtAddr,
                                                       m_memoryProperties.vaStartPrt,
                                                       _16GB,
                                                       _1GB,
                                                       VaPartition::CaptureReplay);
                        if (result == Result::Success)
                        {
                            gpusize virtAddr = pVaRange[static_cast<uint32>(VaPartition::Default)].baseVirtAddr;

                            pVaRange[static_cast<uint32>(VaPartition::Default)].size = baseVirtAddr - virtAddr;

                            pVaRange[static_cast<uint32>(VaPartition::CaptureReplay)].baseVirtAddr = baseVirtAddr;
                            pVaRange[static_cast<uint32>(VaPartition::CaptureReplay)].size         = _16GB;
                        }
                    }
                    else
                    {
                        // If a dedicated PRT VA range exists, adjust the default VA range to exclude it.
                        pVaRange[static_cast<uint32>(VaPartition::Default)].size = (m_memoryProperties.vaStartPrt -
                                                                                    baseVirtAddr);
                    }

                    pVaRange[static_cast<uint32>(VaPartition::Prt)].baseVirtAddr = m_memoryProperties.vaStartPrt;
                    pVaRange[static_cast<uint32>(VaPartition::Prt)].size         = (usableVaEnd -
                                                                                    m_memoryProperties.vaStartPrt);
                }
                else if (m_chipProperties.gfxip.supportCaptureReplay == 1)
                {
                    result = FindGpuVaRangeReverse(&baseVirtAddr,
                                                   usableVaEnd,
                                                   _16GB,
                                                   _1GB,
                                                   VaPartition::CaptureReplay);
                    if (result == Result::Success)
                    {
                        gpusize virtAddr = pVaRange[static_cast<uint32>(VaPartition::Default)].baseVirtAddr;

                        PAL_ASSERT(baseVirtAddr - virtAddr >= _4GB);
                        pVaRange[static_cast<uint32>(VaPartition::Default)].size = baseVirtAddr - virtAddr;

                        pVaRange[static_cast<uint32>(VaPartition::CaptureReplay)].baseVirtAddr = baseVirtAddr;
                        pVaRange[static_cast<uint32>(VaPartition::CaptureReplay)].size         = _16GB;
                    }

                }
            }
        }

        m_memoryProperties.flags.multipleVaRangeSupport = 1;

        // Enable support for shadow desc VA range if fmask SRDs are supported
        m_memoryProperties.flags.shadowDescVaSupport = (m_chipProperties.srdSizes.fmaskView > 0);
    }
    else if ((((usableVaEnd - usableVaStart) >= (5uLL * _1GB)) && (m_chipProperties.srdSizes.fmaskView > 0)) ||
             (((usableVaEnd - usableVaStart) >= (4uLL * _1GB)) && (m_chipProperties.srdSizes.fmaskView == 0)))
    {
        // Case #2:
        // This is not quite ideal, but still workable: we have more than 5 GB of address space, so we can use two
        // 1 GB sections for the ShadowDescriptor and DescriptorTable ranges. The remaining space (1 GB - 4GB and
        // >5 GB) will be used for default, and needs to be split into two subsections.
        gpusize baseVirtAddr = m_memoryProperties.fragmentSize
            ? ((usableVaStart - 1) / m_memoryProperties.fragmentSize + 1) * m_memoryProperties.fragmentSize
            : usableVaStart;

        // Need to account for any exclusion at the beginning of the range
        const gpusize descTblSize = _1GB - (baseVirtAddr - RoundDownToMultiple(baseVirtAddr, _1GB));

        result = ProbeGpuVaRange(baseVirtAddr, descTblSize, VaPartition::DescriptorTable);

        if (result == Result::Success)
        {
            pVaRange[static_cast<uint32>(VaPartition::DescriptorTable)].baseVirtAddr = baseVirtAddr;
            pVaRange[static_cast<uint32>(VaPartition::DescriptorTable)].size         = descTblSize;
        }

        baseVirtAddr += descTblSize;

        // Reserve VA range for ShadowDescriptorTable only if fmask SRDs are supported
        if (m_chipProperties.srdSizes.fmaskView > 0)
        {
            pVaRange[static_cast<uint32>(VaPartition::Default)].baseVirtAddr = baseVirtAddr;
            pVaRange[static_cast<uint32>(VaPartition::Default)].size         = (3uLL * _1GB);

            baseVirtAddr += (3uLL * _1GB);

            //@todo Consider not having a separate VA range for shadow desc table, it reserves 4G of VA space which may
            //      not actaully be used.  APU's have limited VA space due to restrictions on page table size allowed in
            //      memory and reserving a range may increase page table size.
            result = ProbeGpuVaRange(baseVirtAddr, _1GB, VaPartition::ShadowDescriptorTable);

            if (result == Result::Success)
            {
                pVaRange[static_cast<uint32>(VaPartition::ShadowDescriptorTable)].baseVirtAddr = baseVirtAddr;
                pVaRange[static_cast<uint32>(VaPartition::ShadowDescriptorTable)].size         = _1GB;
            }

            baseVirtAddr += _1GB;

            if (usableVaEnd > baseVirtAddr)
            {
                pVaRange[static_cast<uint32>(VaPartition::DefaultBackup)].baseVirtAddr = baseVirtAddr;
                pVaRange[static_cast<uint32>(VaPartition::DefaultBackup)].size         = (usableVaEnd - baseVirtAddr);

                m_memoryProperties.flags.defaultVaRangeSplit = 1;
            }
        }
        else
        {
            pVaRange[static_cast<uint32>(VaPartition::Default)].baseVirtAddr = baseVirtAddr;
            pVaRange[static_cast<uint32>(VaPartition::Default)].size         = (usableVaEnd - baseVirtAddr);
        }

        PAL_ASSERT(m_memoryProperties.vaStartPrt == 0);

        m_memoryProperties.flags.multipleVaRangeSupport = 1;

        // Enable support for shadow desc VA range.
        m_memoryProperties.flags.shadowDescVaSupport = (m_chipProperties.srdSizes.fmaskView > 0);
    }
    else
    {
        // Case #3:
        // This is the least preferred scenario: there is not enough VA space to use separate sections for different
        // purposes.  This path is encountered in special cases (such as emulation) and with 32-bit apps.
        pVaRange[static_cast<uint32>(VaPartition::Default)].baseVirtAddr = usableVaStart;
        pVaRange[static_cast<uint32>(VaPartition::Default)].size         = (usableVaEnd - usableVaStart);

        PAL_ASSERT(m_memoryProperties.vaStartPrt == 0);
    }

    if (GetPlatform()->RequestShadowDescVaRange() == false)
    {
        m_memoryProperties.flags.shadowDescVaSupport = 0;
    }

    return result;
}

// =====================================================================================================================
Result Device::CommitSettingsAndInit()
{
    PAL_ASSERT(m_pSettingsLoader != nullptr);
    m_pSettingsLoader->FinalizeSettings();

    OsFinalizeSettings();

    // The memory heap properties need to be finalized after the settings because we use settings to store the
    // performance ratings for each GPU memory heap.
    FinalizeMemoryHeapProperties();
    FinalizeQueueProperties();

#if PAL_BUILD_GFX
    // The GFX device may need to override some chip properties based on settings.
    if (GetGfxDevice() != nullptr)
    {
        GetGfxDevice()->FinalizeChipProperties(&m_chipProperties);
    }
#endif

#if PAL_ENABLE_PRINTS_ASSERTS
    m_settingsCommitted = true;
#endif

    return LateInit();
}

// =====================================================================================================================
// Performs any late-stage initialization that can only be done after settings have been committed.
Result Device::LateInit()
{
    Result result = OsLateInit();

    // if we need to require dedicated per-process VMID, do so now
    if ((result == Result::Success) && Settings().requestDebugVmid && SupportsStaticVmid())
    {
        result = SetStaticVmidMode(true);
    }

#if PAL_BUILD_GFX
    if ((m_pGfxDevice != nullptr) && (result == Result::Success))
    {
        result = m_pGfxDevice->LateInit();
    }
#endif

    return result;
}

// =====================================================================================================================
// Allocates GPU accessible memory for a special debug srd. This must be the first allocation in the Descriptor-Table
// range on the device. This srd is used to debug cases where a client forgets to bind a one dword user data entry.
// Since the shader automatically adds high bits to one dword addresses provided by the client, we end up with an
// address like X`00000000 instead of 0`00000000. Unfortunately, X`00000000 is a valid address, so the hardware will try
// to read from it and the memory at that location could be anything. If it looks enough like an srd, the hardware will
// continue execution and blow up at some unrelated place later. The debug srd prevents this from happening by creating
// a valid srd at X`00000000 which points to an invalid address. This will immediately cause a page fault when accessed.
// The invalid address in the debug srd is controlled by a setting so we can change it if necessary to make this
// situation easier to detect from a page fault error message. We can also use the setting to bypass the issue if we
// need to by setting the address to 00000000. This will cause the hardware to drop the read instead of page faulting.
void Device::InitPageFaultDebugSrd()
{
    const uint32 numDebugSrds = m_publicSettings.unboundDescriptorDebugSrdCount;
    if (numDebugSrds > 0)
    {
        const size_t maxSrdSize = Max(Max(Max(m_chipProperties.srdSizes.bufferView,
                                              m_chipProperties.srdSizes.fmaskView),
                                              m_chipProperties.srdSizes.imageView),
                                              m_chipProperties.srdSizes.sampler);

        GpuMemoryCreateInfo createInfo = {};
        createInfo.vaRange   = VaRange::DescriptorTable;
        createInfo.alignment = 0;
        createInfo.size      = static_cast<gpusize>(maxSrdSize * numDebugSrds);
        createInfo.priority  = GpuMemPriority::Normal;
        createInfo.heaps[0]  = GpuHeapGartUswc;
        createInfo.heapCount = 1;

        // This allocation must always be placed at the beginning of the DescriptorTable VA range.
        const uint32  vaRangeIndex = static_cast<uint32>(VaPartition::DescriptorTable);
        const gpusize baseVirtAddr = m_memoryProperties.vaRange[vaRangeIndex].baseVirtAddr;
        const gpusize vaSize       = Pow2Align(createInfo.size, m_memoryProperties.virtualMemAllocGranularity);
        GpuMemoryInternalCreateInfo internalCreateInfo = {};
        internalCreateInfo.flags.alwaysResident    = 1;
        internalCreateInfo.flags.pageFaultDebugSrd = 1;

        Result result = ReserveGpuVirtualAddress(VaPartition::DescriptorTable,
                                                 baseVirtAddr,
                                                 vaSize,
                                                 true,
                                                 VirtualGpuMemAccessMode::Undefined,
                                                 &internalCreateInfo.baseVirtAddr);

        GpuMemory* pGpuMem = nullptr;
        gpusize memOffset = 0;

        if (result == Result::Success)
        {
            result = m_memMgr.AllocateGpuMem(createInfo, internalCreateInfo, false, &pGpuMem, nullptr);
        }
        else
        {
            // Failing to initialize the Page - Fault Debug SRD is not a fatal error, since well-behaved applications
            // should never need this feature, and it is only a debug feature for non-well-behaved applications.
            // Situations where this would fail to initialize under normal conditions are multi - GPU configurations and
            // applications which create multiple Devices.
            PAL_ALERT_ALWAYS();
        }

        void* pData = nullptr;
        if (result == Result::Success)
        {
            if ((m_pPlatform != nullptr) && (m_pPlatform->GetGpuMemoryEventProvider() != nullptr))
            {
                ResourceDescriptionMiscInternal desc;
                desc.type = MiscInternalAllocType::PageFaultSRD;

                ResourceCreateEventData createData = {};
                createData.type = ResourceType::MiscInternal;
                createData.pObj = &m_pageFaultDebugSrdMem;
                createData.pResourceDescData = &desc;
                createData.resourceDescSize = sizeof(ResourceDescriptionMiscInternal);

                m_pPlatform->GetGpuMemoryEventProvider()->LogGpuMemoryResourceCreateEvent(createData);

                GpuMemoryResourceBindEventData bindData = {};
                bindData.pGpuMemory = pGpuMem;
                bindData.pObj = &m_pageFaultDebugSrdMem;
                bindData.offset = memOffset;
                bindData.requiredGpuMemSize = createInfo.size;
                m_pPlatform->GetGpuMemoryEventProvider()->LogGpuMemoryResourceBindEvent(bindData);

                Developer::BindGpuMemoryData callbackData = {};
                callbackData.pObj               = bindData.pObj;
                callbackData.requiredGpuMemSize = bindData.requiredGpuMemSize;
                callbackData.pGpuMemory         = bindData.pGpuMemory;
                callbackData.offset             = bindData.offset;
                callbackData.isSystemMemory     = bindData.isSystemMemory;
                DeveloperCb(Developer::CallbackType::BindGpuMemory, &callbackData);
            }

            m_pageFaultDebugSrdMem.Update(pGpuMem, memOffset);

            result = m_pageFaultDebugSrdMem.Map(reinterpret_cast<void**>(&pData));
        }

        if (result == Result::Success)
        {
            if (m_publicSettings.zeroUnboundDescDebugSrd == true)
            {
                // Set null srds to avoid app bugs when reading from a null descriptor table.
                memset(pData, 0, numDebugSrds * maxSrdSize);
            }
            else
            {
                BufferViewInfo bufferViewInfo = {};

                bufferViewInfo.gpuAddr = 0xDEADBEEFDEADBEEF;
                bufferViewInfo.range = UINT64_MAX;
                bufferViewInfo.stride = 1;
                bufferViewInfo.swizzledFormat = UndefinedSwizzledFormat;

                for (uint32 srdIndex = 0; srdIndex < numDebugSrds; ++srdIndex)
                {
                    CreateUntypedBufferViewSrds(1, &bufferViewInfo, pData);
                    pData = VoidPtrInc(pData, maxSrdSize);
                }
            }
            result = m_pageFaultDebugSrdMem.Unmap();
        }

        // In certain multi-gpu configurations, failing this operation is the expected behavior. The only time failure
        // is unexpected is when this function is running on the master gpu. Even in that case though, this is just a
        // debug feature and the only consequence of failure is the loss of the debug srds.
        PAL_ALERT((result != Result::Success) && IsMasterGpu());
    }
}

// =====================================================================================================================
Result Device::InitDummyChunkMem()
{
    GpuMemoryCreateInfo createInfo = {};
    createInfo.vaRange             = VaRange::Default;
    createInfo.alignment           = 0;
    createInfo.size                = 4096;
    createInfo.priority            = GpuMemPriority::Normal;
    createInfo.heaps[0]            = GpuHeapGartUswc;
    createInfo.heapCount           = 1;

    GpuMemoryInternalCreateInfo internalCreateInfo = {};
    internalCreateInfo.flags.alwaysResident        = 1;

    GpuMemory* pGpuMem = nullptr;
    gpusize memOffset  = 0;
    Result result      = m_memMgr.AllocateGpuMem(createInfo, internalCreateInfo, false, &pGpuMem, &memOffset);

    if (result == Result::Success)
    {
        if ((m_pPlatform != nullptr) && (m_pPlatform->GetGpuMemoryEventProvider() != nullptr))
        {
            ResourceDescriptionMiscInternal desc;
            desc.type = MiscInternalAllocType::DummyChunk;

            ResourceCreateEventData createData = {};
            createData.type = ResourceType::MiscInternal;
            createData.pObj = &m_dummyChunkMem;
            createData.pResourceDescData = &desc;
            createData.resourceDescSize = sizeof(ResourceDescriptionMiscInternal);

            m_pPlatform->GetGpuMemoryEventProvider()->LogGpuMemoryResourceCreateEvent(createData);

            GpuMemoryResourceBindEventData bindData = {};
            bindData.pGpuMemory = pGpuMem;
            bindData.pObj = &m_dummyChunkMem;
            bindData.offset = memOffset;
            bindData.requiredGpuMemSize = createInfo.size;
            m_pPlatform->GetGpuMemoryEventProvider()->LogGpuMemoryResourceBindEvent(bindData);

            Developer::BindGpuMemoryData callbackData = {};
            callbackData.pObj               = bindData.pObj;
            callbackData.requiredGpuMemSize = bindData.requiredGpuMemSize;
            callbackData.pGpuMemory         = bindData.pGpuMemory;
            callbackData.offset             = bindData.offset;
            callbackData.isSystemMemory     = bindData.isSystemMemory;
            DeveloperCb(Developer::CallbackType::BindGpuMemory, &callbackData);
        }

        m_dummyChunkMem.Update(pGpuMem, memOffset);
    }

    return result;
}

// =====================================================================================================================
Result Device::CreateInternalCmdAllocators()
{
    // We need a thread-safe, per-device, internal CmdAllocator to service our internal command buffers. It will be
    // primarily used by queue contexts and presentation techniques which have small workloads. Ideally the sizes below
    // will be small to reduce waste but not so small that we see chaining or many CmdStreamAllocations.
    //
    // Note that we create a fully tracked auto-reuse m_allocator and an untracked auto-reuse m_allocator. Ideally we'd
    // use the tracked m_allocator for all internal command buffers but some engines do not currently support tracking.
    // It is PAL's responsibility to only reset or destroy the untracked command buffers when it is safe to do so.
    CmdAllocatorCreateInfo createInfo = {};
    createInfo.flags.threadSafe      = 1;
    createInfo.flags.autoMemoryReuse = 1;
    createInfo.allocInfo[CommandDataAlloc].allocHeap      = CmdBufInternalAllocHeap;
    createInfo.allocInfo[CommandDataAlloc].allocSize      = CmdBufInternalAllocSize;
    createInfo.allocInfo[CommandDataAlloc].suballocSize   = CmdBufInternalSuballocSize;
    createInfo.allocInfo[EmbeddedDataAlloc].allocHeap     = CmdBufInternalAllocHeap;
    createInfo.allocInfo[EmbeddedDataAlloc].allocSize     = CmdBufInternalAllocSize;
    createInfo.allocInfo[EmbeddedDataAlloc].suballocSize  = CmdBufInternalSuballocSize;
    createInfo.allocInfo[GpuScratchMemAlloc].allocHeap    = GpuHeapInvisible;
    createInfo.allocInfo[GpuScratchMemAlloc].allocSize    = CmdBufInternalAllocSize;
    createInfo.allocInfo[GpuScratchMemAlloc].suballocSize = CmdBufInternalSuballocSize;

    Result result = CreateInternalCmdAllocator(createInfo, &m_pTrackedCmdAllocator);

    if (result == Result::Success)
    {
        createInfo.flags.disableBusyChunkTracking = 1;

        result = CreateInternalCmdAllocator(createInfo, &m_pUntrackedCmdAllocator);
    }

    return result;
}

// =====================================================================================================================
// Fully initializes this Device object by creating each HWIP block's Device objects and all of the client-requested
// Queues.
Result Device::Finalize(
    const DeviceFinalizeInfo& finalizeInfo)
{
#if PAL_ENABLE_PRINTS_ASSERTS
    // Clients must call CommitSettingsAndInit() before Finalize().
    PAL_ASSERT(m_settingsCommitted);
#endif

    Result result = (finalizeInfo.flags.internalGpuMemAutoPriority && !m_memoryProperties.flags.autoPrioritySupport)
        ? Result::ErrorInvalidFlags
        : Result::Success;

    if (result == Result::Success)
    {
        for (uint32 idx = 0; idx < EngineTypeCount; idx++)
        {
            if (Util::CountSetBits(finalizeInfo.requestedEngineCounts[idx].engines) >
                m_engineProperties.perEngine[idx].numAvailable)
            {
                result = Result::ErrorInvalidValue;
                break;
            }
        }
    }

    if (result == Result::Success)
    {
        constexpr uint32 CeRamSizeAlignment = 32;

        for (uint32 i = 0; i < EngineTypeCount; i++)
        {
            PAL_ASSERT(finalizeInfo.ceRamSizeUsed[i] <= m_engineProperties.perEngine[i].availableCeRamSize);
            PAL_ASSERT(IsPow2Aligned(finalizeInfo.ceRamSizeUsed[i], CeRamSizeAlignment));
        }

        memcpy(&m_finalizeInfo, &finalizeInfo, sizeof(DeviceFinalizeInfo));

#if PAL_BUILD_GFX
        if (m_pGfxDevice != nullptr)
        {
            // Initialize an srd that's used to help debug unbound one dword descriptors.
            InitPageFaultDebugSrd();

            // Create the dummy command allocator chunk memory. This needs to be done before any command
            // allocators are created.
            if (result == Result::Success)
            {
                result = InitDummyChunkMem();
            }

            if (result == Result::Success)
            {
                result = CreateInternalCmdAllocators();
            }
        }
#endif
            // Initialize a real dummy command stream, which is filled with NOP
            if (result == Result::Success)
            {
                result = CreateDummyCommandStreams();
            }

            // Refer to Device::ValidatePipelineUploadHeap.
            // If any of the following conditions is false, it should be safe to not create a dmaUploadRing,
            // since pipeline uploader will overwrite client's preference of pipeline heap in case that
            // no dma engine is available.
            if ((result == Result::Success)                                    &&
                (m_engineProperties.perEngine[EngineTypeDma].numAvailable > 0) &&
                (m_pPlatform->InternalResidencyOptsDisabled() == false)        &&
                (HeapLogicalSize(GpuHeapInvisible) > 0))
            {
                result = CreateDmaUploadRing();
            }

#if PAL_BUILD_GFX
            if (m_pGfxDevice != nullptr && result == Result::Success)
            {
                // Finalize the device here after the internal copy queues have been created.
                result = m_pGfxDevice->Finalize();
            }
#endif
    }

    if (result == Result::Success)
    {
        result = CreateEngines(finalizeInfo);
    }

    // If developer mode is enabled we need to initialize some internal resources.
    if ((result == Result::Success) && m_pPlatform->IsDeveloperModeEnabled())
    {
        // This pointer should always be valid if developer mode is enabled.
        DevDriver::DevDriverServer* pDevDriverServer = m_pPlatform->GetDevDriverServer();
        PAL_ASSERT(pDevDriverServer != nullptr);

        // Cache the developer driver client id so we don't have to look it up from the server every time
        // we draw the developer overlay later.
        m_devDriverClientId = pDevDriverServer->GetMessageChannel()->GetClientId();

        m_pTextWriter = PAL_NEW(GpuUtil::TextWriter<Platform>, m_pPlatform, AllocInternal)(this, m_pPlatform);
        result        = (m_pTextWriter != nullptr) ? m_pTextWriter->Init() : Result::ErrorOutOfMemory;
    }

    m_texOptLevel     = finalizeInfo.internalTexOptLevel;
    m_deviceFinalized = true;

    return result;
}

// =====================================================================================================================
// Helper function that takes a list of SubresRanges, and splits the SubresRanges that have multiple planes specified
// into SubresRanges with a single plane specified. If the this function allocates memory pMemAllocated is set to true
// and the caller is responsible for deleting the memory.
Result Device::SplitSubresRanges(
    uint32              rangeCount,        // Number of SubresRanges in pRanges
    const SubresRange*  pRanges,           // Array of SubresRanges that could contain multi-plane ranges.
    uint32*             pSplitRangeCount,  // Set to number of SubresRanges in pSplitRanges
    const SubresRange** ppSplitRanges,     // Set to point to either the original array of SubresRanges if no
                                           // multi-plane ranges are found, or a new array of SubresRanges
                                           // that has the same information except with the multi plane
                                           // ranges split into single plane ranges. If a new array is
                                           // created the function returns true (false otherwise), and the
                                           // caller is responsible for deleting the memory.
    bool*               pMemAllocated      // If the this function allocates memory true is set and the caller is
                                           // responsible for deleting the memory.
    ) const
{
    PAL_ASSERT((pSplitRangeCount != nullptr) && (ppSplitRanges != nullptr));

    Result result = Result::Success;

    *pMemAllocated = false;
    uint32 splitCount = 0;

    for (uint32 i = 0; i < rangeCount; i++)
    {
        splitCount += pRanges[i].numPlanes;
    }

    PAL_ASSERT(splitCount >= rangeCount);

    if (splitCount <= rangeCount)
    {
        *ppSplitRanges    = pRanges;
        *pSplitRangeCount = rangeCount;
    }
    else
    {
        SubresRange* pNewSplitRanges = PAL_NEW_ARRAY(SubresRange, splitCount, GetPlatform(), AllocInternalTemp);
        if (pNewSplitRanges == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }
        else
        {
            *pMemAllocated = true;

            // Copy the ranges to the new memory and split them when necessary.
            uint32 newSplitCount = 0;
            for (uint32 i = 0; i < rangeCount; i++)
            {
                pNewSplitRanges[newSplitCount] = pRanges[i];
                pNewSplitRanges[newSplitCount].numPlanes = 1;
                newSplitCount++;

                for (uint32 plane = pRanges[i].startSubres.plane + 1;
                    plane < (pRanges[i].startSubres.plane + pRanges[i].numPlanes);
                    plane++)
                {
                    pNewSplitRanges[newSplitCount] = pRanges[i];
                    pNewSplitRanges[newSplitCount].numPlanes = 1;
                    pNewSplitRanges[newSplitCount].startSubres.plane = plane;
                    newSplitCount++;
                }
            }

            PAL_ASSERT(newSplitCount == splitCount);

            *ppSplitRanges    = pNewSplitRanges;
            *pSplitRangeCount = newSplitCount;
        }
    }

    PAL_ASSERT(*ppSplitRanges != nullptr);

    return result;
}

// =====================================================================================================================
// Helper function that takes a BarrierInfo, and splits the SubresRanges in the BarrierTransitions that have multiple
// planes specified into BarrierTransitions with a single plane SubresRange specified. If the this function allocates
// memory pMemAllocated is set to true and the caller is responsible for deleting the memory.
Result Device::SplitBarrierTransitions(
    Platform*    pPlatform,
    BarrierInfo* pBarrier,      // Copy of a BarrierInfo struct that can have its pTransitions replaced.
                                // If pTransitions contains imageInfos with SubresRanges that contain
                                // multiple planes then a new list of transitions will be allocated,
                                // so that the list of transitions only contain single plane ranges. If
                                // memory is allocated for a new list of transitions true is returned
                                // (false otherwise), and the caller is responsible for deleting the
                                // memory.
    bool*        pMemAllocated) // If the this function allocates memory true is set and the caller is
                                // responsible for deleting the memory.
{
    PAL_ASSERT(pBarrier != nullptr);

    Result result = Result::Success;

    *pMemAllocated = false;

    uint32 splitCount = 0;
    for (uint32 i = 0; i < pBarrier->transitionCount; i++)
    {
        const BarrierTransition& transition = pBarrier->pTransitions[i];
        splitCount += (transition.imageInfo.pImage != nullptr) ? transition.imageInfo.subresRange.numPlanes : 1;
    }

    PAL_ASSERT(splitCount >= pBarrier->transitionCount);

    if (splitCount > pBarrier->transitionCount)
    {
        BarrierTransition* pNewSplitTransitions =
            PAL_NEW_ARRAY(BarrierTransition, splitCount, pPlatform, AllocInternalTemp);
        if (pNewSplitTransitions == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }
        else
        {
            *pMemAllocated = true;

            // Copy the transitions to the new memory and split them when necessary.
            uint32 newSplitCount = 0;
            for (uint32 i = 0; i < pBarrier->transitionCount; i++)
            {
                const BarrierTransition& transition = pBarrier->pTransitions[i];
                pNewSplitTransitions[newSplitCount] = transition;
                newSplitCount++;

                if (transition.imageInfo.pImage != nullptr)
                {
                    // newSplitCount was incremented above so use - 1 here.
                    pNewSplitTransitions[newSplitCount-1].imageInfo.subresRange.numPlanes = 1;

                    const SubresRange& subresRange = pBarrier->pTransitions[i].imageInfo.subresRange;

                    for (uint32 plane = subresRange.startSubres.plane + 1;
                        plane < (subresRange.startSubres.plane + subresRange.numPlanes);
                        plane++)
                    {
                        pNewSplitTransitions[newSplitCount] = pBarrier->pTransitions[i];
                        pNewSplitTransitions[newSplitCount].imageInfo.subresRange.numPlanes = 1;
                        pNewSplitTransitions[newSplitCount].imageInfo.subresRange.startSubres.plane = plane;
                        newSplitCount++;
                    }
                }
            }

            PAL_ASSERT(newSplitCount == splitCount);

            pBarrier->transitionCount = newSplitCount;
            pBarrier->pTransitions    = pNewSplitTransitions;
        }
    }

    return result;
}

// =====================================================================================================================
// Helper function that takes an AcquireReleaseInfo, and splits the SubresRanges in the ImgBarriers that have multiple
// planes specified into ImgBarriers with a single plane SubresRanges specified. If the this function allocates memory
// pMemAllocated is set to true and the caller is responsible for deleting the memory.
Result Device::SplitImgBarriers(
    Platform*           pPlatform,
    AcquireReleaseInfo* pBarrier,      // Copy of a AcquireReleaseInfo struct that can have its pImageBarriers
                                       // replaced. If pImageBarriers has SubresRanges that contain
                                       // multiple planes then a new list of image barriers will be allocated,
                                       // so that the list of barriers only contain single plane ranges. If
                                       // memory is allocated for a new list of barriers true is returned
                                       // (false otherwise), and the caller is responsible for deleting the
                                       // memory.
    bool*               pMemAllocated) // If the this function allocates memory true is set and the caller is
                                       // responsible for deleting the memory.
{
    PAL_ASSERT(pBarrier != nullptr);

    Result result = Result::Success;

    *pMemAllocated = false;

    uint32 splitCount = 0;
    for (uint32 i = 0; i < pBarrier->imageBarrierCount; i++)
    {
        splitCount += pBarrier->pImageBarriers[i].subresRange.numPlanes;
    }

    PAL_ASSERT(splitCount >= pBarrier->imageBarrierCount);

    if (splitCount > pBarrier->imageBarrierCount)
    {
        ImgBarrier* pNewSplitTransitions = PAL_NEW_ARRAY(ImgBarrier, splitCount, pPlatform, AllocInternalTemp);
        if (pNewSplitTransitions == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }
        else
        {
            *pMemAllocated = true;

            // Copy the transitions to the new memory and split them when necessary.
            uint32 newSplitCount = 0;
            for (uint32 i = 0; i < pBarrier->imageBarrierCount; i++)
            {
                pNewSplitTransitions[newSplitCount] = pBarrier->pImageBarriers[i];
                pNewSplitTransitions[newSplitCount].subresRange.numPlanes = 1;
                newSplitCount++;

                const SubresRange& subresRange = pBarrier->pImageBarriers[i].subresRange;

                for (uint32 plane = subresRange.startSubres.plane + 1;
                    plane < (subresRange.startSubres.plane + subresRange.numPlanes);
                    plane++)
                {
                    pNewSplitTransitions[newSplitCount] = pBarrier->pImageBarriers[i];
                    pNewSplitTransitions[newSplitCount].subresRange.numPlanes = 1;
                    pNewSplitTransitions[newSplitCount].subresRange.startSubres.plane = plane;
                    newSplitCount++;
                }
            }

            PAL_ASSERT(newSplitCount == splitCount);

            pBarrier->imageBarrierCount = newSplitCount;
            pBarrier->pImageBarriers    = pNewSplitTransitions;
        }
    }

    return result;
}

// =====================================================================================================================
Result Device::CreateEngines(
    const DeviceFinalizeInfo& finalizeInfo)
{
    Result result = Result::Success;

    for (uint32 i = 0; ((i < EngineTypeCount) && (result == Result::Success)); i++)
    {
        uint32           engines    = finalizeInfo.requestedEngineCounts[i].engines;
        const EngineType engineType = static_cast<EngineType>(i);

        for (uint32 index : BitIter32(engines))
        {
            result = CreateEngine(engineType, index);
            if (result != Result::Success)
            {
                break;
            }

            if (m_engineProperties.perEngine[engineType].flags.physicalAddressingMode)
            {
                m_flags.physicalEnginesAvailable = 1;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// This helper function allows us to create an engine for the specified type and index. This function is mainly executed
// from the Device::CreateEngines() function above. However, in certain circumstances, PAL may need to create an engine
// for internal use. If the engine is created internally, that engine has no support for features that are given to
// engines at Device::Finalize() time, such as GDS usage.
Result Device::CreateEngine(
    EngineType engineType,
    uint32     engineIndex)
{
    Result result = Result::Success;

    switch (engineType)
    {
    case EngineTypeUniversal:
    case EngineTypeCompute:
#if PAL_BUILD_GFX
        if (m_pGfxDevice != nullptr)
        {
            result = m_pGfxDevice->CreateEngine(engineType, engineIndex, &m_pEngines[engineType][engineIndex]);
        }
#endif
        break;
    case EngineTypeDma:
#if PAL_BUILD_OSS
        if (m_pOssDevice != nullptr)
        {
            result = m_pOssDevice->CreateEngine(engineType, engineIndex, &m_pEngines[engineType][engineIndex]);
        }
#endif

        // GFX10 and newer level parts have the DMA engine as part of the GFX device...
        if (IsGfx10Plus(*this))
        {
            result = m_pGfxDevice->CreateEngine(engineType, engineIndex, &m_pEngines[engineType][engineIndex]);
        }
        break;
    case EngineTypeTimer:
    {
        Engine* pEngine = PAL_NEW(Engine, GetPlatform(), AllocInternal)(*this, engineType, engineIndex);
        if (pEngine != nullptr)
        {
            result = pEngine->Init();
            if (result == Result::Success)
            {
                m_pEngines[engineType][engineIndex] = pEngine;
            }
            else
            {
                PAL_DELETE(pEngine, GetPlatform());
            }
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    }
    break;

    default:
        // What is this?
        PAL_ASSERT_ALWAYS();
        result = Result::ErrorUnknown;
        break;
    };

    return result;
}

// =====================================================================================================================
// This helper function allows us to create Dummy Command Streams (Filled with NOP) for ip-specific engines
Result Device::CreateDummyCommandStreams()
{
    Result result = Result::Success;

    for (uint32 i = 0; ((i < EngineTypeCount) && (result == Result::Success)); i++)
    {
        const EngineType engineType = static_cast<EngineType>(i);

        if (m_engineProperties.perEngine[engineType].numAvailable > 0)
        {
            switch (engineType)
            {
#if PAL_BUILD_GFX
            case EngineTypeUniversal:
            case EngineTypeCompute:
                if (m_pGfxDevice != nullptr)
                {
                    result = m_pGfxDevice->CreateDummyCommandStream(engineType, &m_pDummyCommandStreams[engineType]);
                }
                break;
#endif
#if PAL_BUILD_OSS || PAL_BUILD_GFX
            case EngineTypeDma:
#if PAL_BUILD_OSS
                // Most GPU's use OSSIP for DMA engines.
                if (m_pOssDevice != nullptr)
                {
                    // Create OSS command stream for DMA...
                    result = m_pOssDevice->CreateDummyCommandStream(engineType, &m_pDummyCommandStreams[engineType]);
                }
#endif
#if PAL_BUILD_GFX
                // Some GPUs use GFXIP instead for DMA engines.
                if ((m_pDummyCommandStreams[engineType] == nullptr) && (m_pGfxDevice != nullptr))
                {
                    // Create GFX command stream for DMA...
                    result = m_pGfxDevice->CreateDummyCommandStream(engineType, &m_pDummyCommandStreams[engineType]);
                }
#endif
                break;
#endif

            default:
                // No corresponding dummy command stream for this engine
                m_pDummyCommandStreams[engineType] = nullptr;
                break;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Finalizes the properties of each GPU memory heap available to this GPU. This must be called after the settings
// loader has been finalized to make sure that the heap performance settings have been finalized.
void Device::FinalizeMemoryHeapProperties()
{
    if (Settings().force64kPageGranularity)
    {
        m_memoryProperties.realMemAllocGranularity    = Pow2Align(m_memoryProperties.realMemAllocGranularity, 65536);
        m_memoryProperties.virtualMemAllocGranularity =
            Pow2Align(m_memoryProperties.virtualMemAllocGranularity, 65536);
        m_memoryProperties.virtualMemPageSize         = Pow2Align(m_memoryProperties.virtualMemPageSize, 65536);
    }
}

// =====================================================================================================================
// Fills out a structure with details on the properties of this GPU object. This includes capability flags, supported
// queues, performance characteristics, etc.
// NOTE: Part of the IDevice public interface.
Result Device::GetProperties(
    DeviceProperties* pInfo
    ) const
{
    Result result = Result::ErrorInvalidPointer;

    if (pInfo != nullptr)
    {
        memset(pInfo, 0, sizeof(DeviceProperties));

        // NOTE: We must identify with the ATI vendor ID rather than AMD, as apps can be hardcoded to detect ATI ID.
        pInfo->vendorId                         = ATI_VENDOR_ID;
        pInfo->deviceId                         = m_chipProperties.deviceId;
        pInfo->revisionId                       = m_chipProperties.revisionId;
        pInfo->eRevId                           = m_chipProperties.eRevId;
        pInfo->revision                         = m_chipProperties.revision;
        pInfo->gfxStepping                      = m_chipProperties.gfxStepping;
        pInfo->gpuType                          = m_chipProperties.gpuType;
        pInfo->gfxLevel                         = m_chipProperties.gfxLevel;
        pInfo->gpuPerformanceCapacity           = m_chipProperties.gpuPerformanceCapacity;
        pInfo->ossLevel                         = m_chipProperties.ossLevel;
        pInfo->uvdLevel                         = m_chipProperties.uvdLevel;
        pInfo->vceLevel                         = m_chipProperties.vceLevel;
        pInfo->vcnLevel                         = m_chipProperties.vcnLevel;
        pInfo->spuLevel                         = m_chipProperties.spuLevel;
        pInfo->pspLevel                         = m_chipProperties.pspLevel;

        Strncpy(&pInfo->gpuName[0], &m_gpuName[0], sizeof(pInfo->gpuName));

        pInfo->attachedScreenCount         = m_attachedScreenCount;
        pInfo->gpuIndex                    = m_chipProperties.gpuIndex;
        pInfo->maxGpuMemoryRefsResident    = m_engineProperties.maxUserMemRefsPerSubmission;
        pInfo->timestampFrequency          = m_chipProperties.gpuCounterFrequency;
        pInfo->maxSemaphoreCount           = m_maxSemaphoreCount;

        for (uint32 i = 0; i < EngineTypeCount; ++i)
        {
            const auto& engineInfo  = m_engineProperties.perEngine[i];
            auto*const  pEngineInfo = &pInfo->engineProperties[i];

            pEngineInfo->engineCount                   = engineInfo.numAvailable;
            pEngineInfo->queueSupport                  = engineInfo.queueSupport;
            pEngineInfo->ceRamSizeAvailable            = engineInfo.availableCeRamSize;
            pEngineInfo->controlFlowNestingLimit       = engineInfo.maxControlFlowNestingDepth;
            pEngineInfo->minTiledImageCopyAlignment    = engineInfo.minTiledImageCopyAlignment;
            pEngineInfo->minTiledImageMemCopyAlignment = engineInfo.minTiledImageMemCopyAlignment;
            pEngineInfo->minLinearMemCopyAlignment     = engineInfo.minLinearMemCopyAlignment;
            pEngineInfo->minTimestampAlignment         = engineInfo.minTimestampAlignment;
            pEngineInfo->maxNumDedicatedCu             = engineInfo.maxNumDedicatedCu;
            pEngineInfo->tmzSupportLevel               = engineInfo.tmzSupportLevel;

            // This may have its own value in the future for now use maxNumDedicatedCu
            pEngineInfo->maxNumDedicatedCuPerQueue     = engineInfo.maxNumDedicatedCu;

            // Long-term this value would be preferred to be reported by KMD.
            pEngineInfo->dedicatedCuGranularity        = engineInfo.dedicatedCuGranularity;

            if (engineInfo.flags.borderColorPaletteSupport != 0)
            {
                pEngineInfo->maxBorderColorPaletteSize = GetPublicSettings()->borderColorPaletteSizeLimit;
            }

            pEngineInfo->flags.supportsTimestamps              = engineInfo.flags.timestampSupport;
            pEngineInfo->flags.supportsQueryPredication        = engineInfo.flags.queryPredicationSupport;
            pEngineInfo->flags.supports32bitMemoryPredication  = engineInfo.flags.memory32bPredicationSupport ||
                                                                 engineInfo.flags.memory32bPredicationEmulated;
            pEngineInfo->flags.supports64bitMemoryPredication  = engineInfo.flags.memory64bPredicationSupport;
            pEngineInfo->flags.supportsConditionalExecution    = engineInfo.flags.conditionalExecutionSupport;
            pEngineInfo->flags.supportsLoopExecution           = engineInfo.flags.loopExecutionSupport;
            pEngineInfo->flags.supportsRegMemAccess            = engineInfo.flags.regMemAccessSupport;
            pEngineInfo->flags.supportsMismatchedTileTokenCopy = engineInfo.flags.supportsMismatchedTileTokenCopy;
            pEngineInfo->flags.supportsImageInitBarrier        = engineInfo.flags.supportsImageInitBarrier;
            pEngineInfo->flags.supportsImageInitPerSubresource = engineInfo.flags.supportsImageInitPerSubresource;
            pEngineInfo->flags.supportVirtualMemoryRemap       = engineInfo.flags.supportVirtualMemoryRemap;
            pEngineInfo->flags.runsInPhysicalMode              = engineInfo.flags.physicalAddressingMode;
            pEngineInfo->flags.supportPersistentCeRam          = engineInfo.flags.supportPersistentCeRam;
            pEngineInfo->flags.p2pCopyToInvisibleHeapIllegal   = engineInfo.flags.p2pCopyToInvisibleHeapIllegal;
            pEngineInfo->flags.supportsTrackBusyChunks         = engineInfo.flags.supportsTrackBusyChunks;
            pEngineInfo->flags.supportsUnmappedPrtPageAccess   = engineInfo.flags.supportsUnmappedPrtPageAccess;
            pEngineInfo->flags.supportsClearCopyMsaaDsDst      = engineInfo.flags.supportsClearCopyMsaaDsDst;

            for (uint32 engineIdx = 0; engineIdx < MaxAvailableEngines; engineIdx++)
            {
                const auto& capabilitiesInfo  = engineInfo.capabilities[engineIdx];
                auto*const  pCapabilitiesInfo = &pEngineInfo->capabilities[engineIdx];

                pCapabilitiesInfo->flags.exclusive                  = capabilitiesInfo.flags.exclusive;
                pCapabilitiesInfo->flags.mustUseDispatchTunneling   = capabilitiesInfo.flags.mustUseDispatchTunneling;
                pCapabilitiesInfo->queuePrioritySupport             = capabilitiesInfo.queuePrioritySupport;
                pCapabilitiesInfo->dispatchTunnelingPrioritySupport = capabilitiesInfo.dispatchTunnelingPrioritySupport;
                pCapabilitiesInfo->flags.supportsMultiQueue         = capabilitiesInfo.flags.supportsMultiQueue;
                pCapabilitiesInfo->maxFrontEndPipes                 = capabilitiesInfo.maxFrontEndPipes;
                pCapabilitiesInfo->flags.hwsEnabled                 = capabilitiesInfo.flags.hwsEnabled;
            }

            for (uint32 j = 0; j < CmdAllocatorTypeCount; j++)
            {
                pEngineInfo->preferredCmdAllocHeaps[j] = engineInfo.preferredCmdAllocHeaps[j];
            }
        }

        for (uint32 i = 0; i < QueueTypeCount; ++i)
        {
            const auto& queueInfo  = m_queueProperties.perQueue[i];
            auto*       pQueueInfo = &pInfo->queueProperties[i];

            pQueueInfo->flags.supportsSwapChainPresents = queueInfo.flags.supportsSwapChainPresents;
            pQueueInfo->supportedDirectPresentModes     = queueInfo.supportedDirectPresentModes;
        }

        pInfo->gpuMemoryProperties.flags.virtualRemappingSupport = m_memoryProperties.flags.virtualRemappingSupport;
        pInfo->gpuMemoryProperties.flags.pinningSupport          = m_memoryProperties.flags.pinningSupport;
        pInfo->gpuMemoryProperties.flags.supportPerSubmitMemRefs = m_memoryProperties.flags.supportPerSubmitMemRefs;
        pInfo->gpuMemoryProperties.flags.globalGpuVaSupport      = m_memoryProperties.flags.globalGpuVaSupport;
        pInfo->gpuMemoryProperties.flags.svmSupport              = m_memoryProperties.flags.svmSupport;
        pInfo->gpuMemoryProperties.flags.iommuv2Support          = m_memoryProperties.flags.iommuv2Support;
        pInfo->gpuMemoryProperties.flags.shadowDescVaSupport     = m_memoryProperties.flags.shadowDescVaSupport;
        pInfo->gpuMemoryProperties.flags.autoPrioritySupport     = m_memoryProperties.flags.autoPrioritySupport;
        pInfo->gpuMemoryProperties.flags.pageMigrationEnabled    = m_memoryProperties.flags.intraSubmitMigration;
        pInfo->gpuMemoryProperties.flags.supportsTmz             = m_memoryProperties.flags.supportsTmz;
        pInfo->gpuMemoryProperties.flags.supportsMall            = m_memoryProperties.flags.supportsMall;
        pInfo->gpuMemoryProperties.flags.supportPageFaultInfo    = m_memoryProperties.flags.supportPageFaultInfo;

        pInfo->gpuMemoryProperties.realMemAllocGranularity    = m_memoryProperties.realMemAllocGranularity;
        pInfo->gpuMemoryProperties.virtualMemAllocGranularity = m_memoryProperties.virtualMemAllocGranularity;
        pInfo->gpuMemoryProperties.virtualMemPageSize         = m_memoryProperties.virtualMemPageSize;
        pInfo->gpuMemoryProperties.fragmentSize               = m_memoryProperties.fragmentSize;
        pInfo->gpuMemoryProperties.maxCaptureReplaySize       =
            m_memoryProperties.vaRange[static_cast<uint32>(VaPartition::CaptureReplay)].size;

        pInfo->gpuMemoryProperties.maxVirtualMemSize  =
            Util::Max(m_memoryProperties.vaRange[static_cast<uint32>(VaPartition::Prt)].size,
                      m_memoryProperties.vaRange[static_cast<uint32>(VaPartition::Default)].size);
        pInfo->gpuMemoryProperties.vaStart            = m_memoryProperties.vaStart;
        pInfo->gpuMemoryProperties.vaEnd              = m_memoryProperties.vaEnd;
        pInfo->gpuMemoryProperties.descTableVaStart =
            m_memoryProperties.vaRange[static_cast<uint32>(VaPartition::DescriptorTable)].baseVirtAddr;
        pInfo->gpuMemoryProperties.shadowDescTableVaStart =
            m_memoryProperties.vaRange[static_cast<uint32>(VaPartition::ShadowDescriptorTable)].baseVirtAddr;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 766
        pInfo->gpuMemoryProperties.maxPhysicalMemSize = (m_heapProperties[GpuHeapLocal].physicalSize +
                                                         m_heapProperties[GpuHeapInvisible].physicalSize +
                                                         m_memoryProperties.nonLocalHeapSize);
        pInfo->gpuMemoryProperties.maxLocalMemSize    = (m_heapProperties[GpuHeapLocal].logicalSize +
                                                         m_heapProperties[GpuHeapInvisible].logicalSize);
        pInfo->gpuMemoryProperties.barSize            = m_memoryProperties.barSize;
#else
        pInfo->gpuMemoryProperties.maxPhysicalMemSize = (m_heapProperties[GpuHeapLocal].physicalHeapSize +
                                                         m_heapProperties[GpuHeapInvisible].physicalHeapSize +
                                                         m_memoryProperties.nonLocalHeapSize);
        pInfo->gpuMemoryProperties.maxLocalMemSize    = (m_heapProperties[GpuHeapLocal].heapSize +
                                                         m_heapProperties[GpuHeapInvisible].heapSize);
#endif
        pInfo->gpuMemoryProperties.localMemoryType    = m_memoryProperties.localMemoryType;

        pInfo->gpuMemoryProperties.privateApertureBase         = m_memoryProperties.privateApertureBase;
        pInfo->gpuMemoryProperties.sharedApertureBase          = m_memoryProperties.sharedApertureBase;
        pInfo->gpuMemoryProperties.busAddressableMemSize       = m_memoryProperties.busAddressableMemSize;

        pInfo->gpuMemoryProperties.largePageSizeInBytes =
                m_memoryProperties.largePageSupport.largePageSizeInBytes;

        pInfo->gpuMemoryProperties.performance.maxMemClock     = static_cast<float>(m_chipProperties.maxMemoryClock);
        pInfo->gpuMemoryProperties.performance.memPerfRating   = m_chipProperties.memoryPerfRating;
        pInfo->gpuMemoryProperties.performance.vramBusBitWidth = m_memoryProperties.vramBusBitWidth;
        pInfo->gpuMemoryProperties.performance.memOpsPerClock  = m_memoryProperties.memOpsPerClock;

        pInfo->imageProperties.maxDimensions    = m_chipProperties.imageProperties.maxImageDimension;
        pInfo->imageProperties.maxArraySlices   = m_chipProperties.imageProperties.maxImageArraySize;
        pInfo->imageProperties.prtFeatures      = m_chipProperties.imageProperties.prtFeatures;
        pInfo->imageProperties.prtTileSize      = m_chipProperties.imageProperties.prtTileSize;
        pInfo->imageProperties.msaaSupport      = m_chipProperties.imageProperties.msaaSupport;
        pInfo->imageProperties.maxMsaaFragments = m_chipProperties.imageProperties.maxMsaaFragments;
        pInfo->imageProperties.vrsTileSize      = m_chipProperties.imageProperties.vrsTileSize;

        pInfo->imageProperties.flags.u32All                       = 0;
        pInfo->imageProperties.flags.supportsAqbsStereoMode       =
                m_chipProperties.imageProperties.flags.supportsAqbsStereoMode;
        pInfo->imageProperties.flags.supportsCornerSampling       =
                m_chipProperties.imageProperties.flags.supportsCornerSampling;

        pInfo->imageProperties.numSwizzleEqs   = m_chipProperties.imageProperties.numSwizzleEqs;

        PAL_ASSERT(pInfo->imageProperties.numSwizzleEqs > 0);
        PAL_ASSERT(pInfo->imageProperties.numSwizzleEqs <= 0xFF);

        pInfo->imageProperties.pSwizzleEqs     = m_chipProperties.imageProperties.pSwizzleEqs;

        for (uint32 idx = 0; idx < static_cast<uint32>(ImageTiling::Count); ++idx)
        {
            pInfo->imageProperties.tilingSupported[idx] = m_chipProperties.imageProperties.tilingSupported[idx];
        }

        pInfo->pciProperties.domainNumber                     = m_chipProperties.pciDomainNumber;
        pInfo->pciProperties.busNumber                        = m_chipProperties.pciBusNumber;
        pInfo->pciProperties.deviceNumber                     = m_chipProperties.pciDeviceNumber;
        pInfo->pciProperties.functionNumber                   = m_chipProperties.pciFunctionNumber;
        pInfo->pciProperties.flags.u32All                     = 0;
        pInfo->pciProperties.flags.gpuConnectedViaThunderbolt = m_chipProperties.gpuConnectedViaThunderbolt ? 1 : 0;
        pInfo->pciProperties.flags.gpuEmulatedInSoftware      = GetPlatform()->IsEmulationEnabled() ? 1 : 0;
        pInfo->pciProperties.flags.gpuEmulatedInHardware      = IsHwEmulationEnabled() ? 1 : 0;

        pInfo->gfxipProperties.cpUcodeVersion    = m_chipProperties.cpUcodeVersion;
        pInfo->gfxipProperties.pfpUcodeVersion   = m_chipProperties.pfpUcodeVersion;

#if PAL_BUILD_GFX
        pInfo->gfxipProperties.maxUserDataEntries = m_chipProperties.gfxip.maxUserDataEntries;

#endif

        switch (m_chipProperties.gfxLevel)
        {
        case GfxIpLevel::GfxIp6:
        case GfxIpLevel::GfxIp7:
        case GfxIpLevel::GfxIp8:
        case GfxIpLevel::GfxIp8_1:
        {
            const auto& gfx6Props = m_chipProperties.gfx6;

            pInfo->gfxipProperties.flags.u64All                         = 0;
            pInfo->gfxipProperties.flags.support8bitIndices             = gfx6Props.support8bitIndices;
            pInfo->gfxipProperties.flags.support16BitInstructions       = gfx6Props.support16BitInstructions;
            pInfo->gfxipProperties.flags.support64BitInstructions       = gfx6Props.support64BitInstructions;
            pInfo->gfxipProperties.flags.supportBorderColorSwizzle      = gfx6Props.supportBorderColorSwizzle;
            pInfo->gfxipProperties.flags.supportFloat64Atomics          = gfx6Props.supportFloat64Atomics;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 720
            pInfo->gfxipProperties.flags.supportFloatAtomics            =
                m_chipProperties.gfxip.supportFloat32BufferAtomics |
                m_chipProperties.gfxip.supportFloat32ImageAtomics  |
                gfx6Props.supportFloat64Atomics;
#endif
            pInfo->gfxipProperties.flags.supportShaderSubgroupClock     = gfx6Props.supportShaderSubgroupClock;
            pInfo->gfxipProperties.flags.supportShaderDeviceClock       = gfx6Props.supportShaderDeviceClock;
            pInfo->gfxipProperties.flags.supports2BitSignedValues       = gfx6Props.supports2BitSignedValues;
            pInfo->gfxipProperties.flags.supportRgpTraces               = gfx6Props.supportRgpTraces;
            pInfo->gfxipProperties.flags.supportImageViewMinLod         = gfx6Props.supportImageViewMinLod;

            pInfo->gfxipProperties.flags.supportSingleChannelMinMaxFilter = 1;
            pInfo->gfxipProperties.flags.supportPerChannelMinMaxFilter    = 0; // GFX6-8 only support single channel
                                                                               // min/max filter

            pInfo->gfxipProperties.shaderCore.numShaderEngines     = gfx6Props.numShaderEngines;
            pInfo->gfxipProperties.shaderCore.numShaderArrays      = gfx6Props.numShaderArrays;
            pInfo->gfxipProperties.shaderCore.numCusPerShaderArray = gfx6Props.numCuPerSh;
            pInfo->gfxipProperties.shaderCore.maxCusPerShaderArray = gfx6Props.maxNumCuPerSh;
            pInfo->gfxipProperties.shaderCore.numAvailableCus      = gfx6Props.numShaderEngines *
                                                                     gfx6Props.numShaderArrays *
                                                                     gfx6Props.numCuPerSh;
            pInfo->gfxipProperties.shaderCore.numPhysicalCus       = gfx6Props.numShaderEngines *
                                                                     gfx6Props.numShaderArrays *
                                                                     gfx6Props.maxNumCuPerSh;
            pInfo->gfxipProperties.shaderCore.numSimdsPerCu        = gfx6Props.numSimdPerCu;
            pInfo->gfxipProperties.shaderCore.numWavefrontsPerSimd = gfx6Props.numWavesPerSimd;
            pInfo->gfxipProperties.shaderCore.numActiveRbs         = gfx6Props.numActiveRbs;
            pInfo->gfxipProperties.shaderCore.nativeWavefrontSize  = gfx6Props.nativeWavefrontSize;
            pInfo->gfxipProperties.shaderCore.minWavefrontSize     = gfx6Props.nativeWavefrontSize;
            pInfo->gfxipProperties.shaderCore.maxWavefrontSize     = gfx6Props.nativeWavefrontSize;
            pInfo->gfxipProperties.shaderCore.numAvailableSgprs    = gfx6Props.numShaderVisibleSgprs;
            pInfo->gfxipProperties.shaderCore.sgprsPerSimd         = gfx6Props.numPhysicalSgprs;
            pInfo->gfxipProperties.shaderCore.minSgprAlloc         = gfx6Props.minSgprAlloc;
            pInfo->gfxipProperties.shaderCore.sgprAllocGranularity = gfx6Props.sgprAllocGranularity;
            pInfo->gfxipProperties.shaderCore.numAvailableVgprs    = MaxVgprPerShader;
            pInfo->gfxipProperties.shaderCore.vgprsPerSimd         = gfx6Props.numPhysicalVgprsPerSimd;
            pInfo->gfxipProperties.shaderCore.minVgprAlloc         = gfx6Props.minVgprAlloc;
            pInfo->gfxipProperties.shaderCore.vgprAllocGranularity = gfx6Props.vgprAllocGranularity;
            pInfo->gfxipProperties.shaderCore.gsPrimBufferDepth    = gfx6Props.gsPrimBufferDepth;
            pInfo->gfxipProperties.shaderCore.gsVgtTableDepth      = gfx6Props.gsVgtTableDepth;

            // Tessellation distribution mode flags.
            pInfo->gfxipProperties.flags.supportPatchTessDistribution     = gfx6Props.supportPatchTessDistribution;
            pInfo->gfxipProperties.flags.supportDonutTessDistribution     = gfx6Props.supportDonutTessDistribution;
            pInfo->gfxipProperties.flags.supportTrapezoidTessDistribution = gfx6Props.supportTrapezoidTessDistribution;

            // No pre-GFX9 GPU supported this.
            pInfo->gfxipProperties.flags.supportsPerShaderStageWaveSize = 0;

            pInfo->gfxipProperties.flags.supportOutOfOrderPrimitives = gfx6Props.supportOutOfOrderPrimitives;

            if (m_chipProperties.gfxLevel == GfxIpLevel::GfxIp6)
            {
                // Gfx6 has a max 2SE x 2SH layout
                for (uint32 se = 0; se < gfx6Props.numShaderEngines; ++se)
                {
                    for (uint32 sh = 0; sh < gfx6Props.numShaderArrays; ++sh)
                    {
                        pInfo->gfxipProperties.shaderCore.activeCuMask[se][sh] = gfx6Props.activeCuMaskGfx6[se][sh];
                    }
                }
            }
            else
            {
                // Gfx7-8 have a max 4SE x 1SH layout
                for (uint32 se = 0; se < gfx6Props.numShaderEngines; ++se)
                {
                    pInfo->gfxipProperties.shaderCore.activeCuMask[se][0] = gfx6Props.activeCuMaskGfx7[se];
                }
            }

            static_assert(sizeof(m_chipProperties.gfxip.activePixelPackerMask) ==
                sizeof(pInfo->gfxipProperties.shaderCore.activePixelPackerMask),
                "PAL Device and interface active pixel packer mask sizes do not match!");
            memcpy(pInfo->gfxipProperties.shaderCore.activePixelPackerMask, m_chipProperties.gfxip.activePixelPackerMask,
                sizeof(pInfo->gfxipProperties.shaderCore.activePixelPackerMask));

            break;
        }

        case GfxIpLevel::GfxIp9:
        case GfxIpLevel::GfxIp10_1:
        case GfxIpLevel::GfxIp10_3:
#if PAL_BUILD_GFX11
        case GfxIpLevel::GfxIp11_0:
#endif
        {
            const auto& gfx9Props = m_chipProperties.gfx9;

            pInfo->gfxipProperties.flags.u64All                             = 0;
            pInfo->gfxipProperties.flags.support8bitIndices                 = 0;
            pInfo->gfxipProperties.flags.supportFp16Fetch                   = gfx9Props.supportFp16Fetch;
            pInfo->gfxipProperties.flags.supportFp16Dot2                    = gfx9Props.supportFp16Dot2;
            pInfo->gfxipProperties.flags.support16BitInstructions           = gfx9Props.support16BitInstructions;
            pInfo->gfxipProperties.flags.support64BitInstructions           = gfx9Props.support64BitInstructions;
            pInfo->gfxipProperties.flags.supportBorderColorSwizzle          = gfx9Props.supportBorderColorSwizzle;
            pInfo->gfxipProperties.flags.supportImageViewMinLod             = gfx9Props.supportImageViewMinLod;
            pInfo->gfxipProperties.flags.supportFloat64Atomics              = gfx9Props.supportFloat64Atomics;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 720
            pInfo->gfxipProperties.flags.supportFloatAtomics                =
                m_chipProperties.gfxip.supportFloat32BufferAtomics |
                m_chipProperties.gfxip.supportFloat32ImageAtomics  |
                gfx9Props.supportFloat64Atomics;
#endif
            pInfo->gfxipProperties.flags.supportShaderSubgroupClock         = gfx9Props.supportShaderSubgroupClock;
            pInfo->gfxipProperties.flags.supportShaderDeviceClock           = gfx9Props.supportShaderDeviceClock;
            pInfo->gfxipProperties.flags.supportAlphaToOne                  = gfx9Props.supportAlphaToOne;
            pInfo->gfxipProperties.flags.supportDoubleRate16BitInstructions =
                gfx9Props.supportDoubleRate16BitInstructions;
            pInfo->gfxipProperties.flags.supportConservativeRasterization = gfx9Props.supportConservativeRasterization;
            pInfo->gfxipProperties.flags.supportPrtBlendZeroMode          = gfx9Props.supportPrtBlendZeroMode;
            pInfo->gfxipProperties.flags.supportSingleChannelMinMaxFilter = gfx9Props.supportSingleChannelMinMaxFilter;
            pInfo->gfxipProperties.flags.supportPerChannelMinMaxFilter    = gfx9Props.supportSingleChannelMinMaxFilter;
            pInfo->gfxipProperties.flags.supportRgpTraces                 = 1;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 752
            pInfo->gfxipProperties.flags.supportMeshShader = gfx9Props.supportMeshShader;
            pInfo->gfxipProperties.flags.supportTaskShader = gfx9Props.supportTaskShader;
#else
            pInfo->gfxipProperties.flags.supportMeshShader = ((gfx9Props.supportMeshShader != 0) &&
                                                              (gfx9Props.supportTaskShader != 0));
#endif
            pInfo->gfxipProperties.flags.supportMsFullRangeRtai           = gfx9Props.supportMsFullRangeRtai;
            pInfo->gfxipProperties.flags.supports2BitSignedValues         = gfx9Props.supports2BitSignedValues;
            pInfo->gfxipProperties.flags.supportPrimitiveOrderedPs        = gfx9Props.supportPrimitiveOrderedPs;
            pInfo->gfxipProperties.flags.supportImplicitPrimitiveShader   = gfx9Props.supportImplicitPrimitiveShader;
            pInfo->gfxipProperties.flags.supportSpp                       = gfx9Props.supportSpp;
            pInfo->gfxipProperties.flags.timestampResetOnIdle             = gfx9Props.timestampResetOnIdle;
            pInfo->gfxipProperties.flags.supportReleaseAcquireInterface   = gfx9Props.supportReleaseAcquireInterface;
            pInfo->gfxipProperties.flags.supportSplitReleaseAcquire       = gfx9Props.supportSplitReleaseAcquire;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 776
            pInfo->gfxipProperties.shaderCore.numShaderEngines     = gfx9Props.numActiveShaderEngines;
#else
            pInfo->gfxipProperties.shaderCore.numShaderEngines     = gfx9Props.numShaderEngines;
#endif
            pInfo->gfxipProperties.shaderCore.numShaderArrays      = gfx9Props.numShaderArrays;
            pInfo->gfxipProperties.shaderCore.numCusPerShaderArray = gfx9Props.numCuPerSh;
            pInfo->gfxipProperties.shaderCore.maxCusPerShaderArray = gfx9Props.maxNumCuPerSh;
            pInfo->gfxipProperties.shaderCore.numAvailableCus      = gfx9Props.numActiveCus;
            pInfo->gfxipProperties.shaderCore.numPhysicalCus       = gfx9Props.numPhysicalCus;
            pInfo->gfxipProperties.shaderCore.numSimdsPerCu        = gfx9Props.numSimdPerCu;
            pInfo->gfxipProperties.shaderCore.numWavefrontsPerSimd = gfx9Props.numWavesPerSimd;
            pInfo->gfxipProperties.shaderCore.numActiveRbs         = gfx9Props.numActiveRbs;
            pInfo->gfxipProperties.shaderCore.nativeWavefrontSize  = gfx9Props.nativeWavefrontSize;
            pInfo->gfxipProperties.shaderCore.minWavefrontSize     = gfx9Props.minWavefrontSize;
            pInfo->gfxipProperties.shaderCore.maxWavefrontSize     = gfx9Props.maxWavefrontSize;
            pInfo->gfxipProperties.shaderCore.numAvailableSgprs    = gfx9Props.numShaderVisibleSgprs;
            pInfo->gfxipProperties.shaderCore.sgprsPerSimd         = gfx9Props.numPhysicalSgprs;
            pInfo->gfxipProperties.shaderCore.minSgprAlloc         = gfx9Props.minSgprAlloc;
            pInfo->gfxipProperties.shaderCore.sgprAllocGranularity = gfx9Props.sgprAllocGranularity;
            pInfo->gfxipProperties.shaderCore.numAvailableVgprs    = MaxVgprPerShader;
            pInfo->gfxipProperties.shaderCore.vgprsPerSimd         = gfx9Props.numPhysicalVgprsPerSimd;
            pInfo->gfxipProperties.shaderCore.minVgprAlloc         = gfx9Props.minVgprAlloc;
            pInfo->gfxipProperties.shaderCore.vgprAllocGranularity = gfx9Props.vgprAllocGranularity;
            pInfo->gfxipProperties.shaderCore.gsPrimBufferDepth    = gfx9Props.gsPrimBufferDepth;
            pInfo->gfxipProperties.shaderCore.gsVgtTableDepth      = gfx9Props.gsVgtTableDepth;

            pInfo->gfxipProperties.shaderCore.flags.u32All = 0;
            pInfo->gfxipProperties.shaderCore.flags.eccProtectedGprs = gfx9Props.eccProtectedGprs;

            // Tessellation distribution mode flags.
            pInfo->gfxipProperties.flags.supportPatchTessDistribution     = gfx9Props.supportPatchTessDistribution;
            pInfo->gfxipProperties.flags.supportDonutTessDistribution     = gfx9Props.supportDonutTessDistribution;
            pInfo->gfxipProperties.flags.supportTrapezoidTessDistribution = gfx9Props.supportTrapezoidTessDistribution;

            pInfo->gfxipProperties.flags.supportMsaaCoverageOut         = gfx9Props.supportMsaaCoverageOut;
            pInfo->gfxipProperties.flags.supportPostDepthCoverage       = gfx9Props.supportPostDepthCoverage;
            pInfo->gfxipProperties.flags.supportSpiPrefPriority         = gfx9Props.supportSpiPrefPriority;
            pInfo->gfxipProperties.flags.supportWaveBreakSize           = gfx9Props.supportCustomWaveBreakSize;
            pInfo->gfxipProperties.flags.supportsPerShaderStageWaveSize = gfx9Props.supportPerShaderStageWaveSize;

            pInfo->gfxipProperties.flags.support1xMsaaSampleLocations   = gfx9Props.support1xMsaaSampleLocations;
            pInfo->gfxipProperties.flags.supportOutOfOrderPrimitives    = gfx9Props.supportOutOfOrderPrimitives;
            pInfo->gfxipProperties.flags.supportTextureGatherBiasLod    = gfx9Props.supportTextureGatherBiasLod;

            pInfo->gfxipProperties.flags.supportIntersectRayBarycentrics = gfx9Props.supportIntersectRayBarycentrics;
#if PAL_BUILD_GFX11
            pInfo->gfxipProperties.flags.supportRayTraversalStack        = gfx9Props.supportRayTraversalStack;
            pInfo->gfxipProperties.flags.supportPointerFlags             = gfx9Props.supportPointerFlags;
#endif

            pInfo->gfxipProperties.supportedVrsRates                     = gfx9Props.gfx10.supportedVrsRates;
            pInfo->gfxipProperties.flags.supportVrsWithDsExports         = gfx9Props.gfx10.supportVrsWithDsExports ? 1 : 0;

            pInfo->gfxipProperties.rayTracingIp    = gfx9Props.rayTracingIp;

            pInfo->gfxipProperties.flags.supportSortAgnosticBarycentrics = gfx9Props.supportSortAgnosticBarycentrics;

            PAL_ASSERT((gfx9Props.numShaderEngines <= MaxShaderEngines) &&
                       (gfx9Props.numShaderArrays  <= MaxShaderArraysPerSe));

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 776
            // Remove any holes in the CuMask and PixelPacker before passing it up to the client
            const uint32 pixelPackersPerSe = m_chipProperties.gfx9.numScPerSe * m_chipProperties.gfx9.numPackerPerSc;
            uint32 activeSe = 0;
            for (uint32 se = 0; se < gfx9Props.numShaderEngines; ++se)
            {
                if (TestAnyFlagSet(gfx9Props.activeSeMask, 1u << se))
                {
                    for (uint32 sa = 0; sa < gfx9Props.numShaderArrays; ++sa)
                    {
                        pInfo->gfxipProperties.shaderCore.activeCuMask[activeSe][sa] = gfx9Props.activeCuMask[se][sa];
                    }
                    for (uint32 packer = 0; packer < pixelPackersPerSe; ++packer)
                    {
                        if (WideBitfieldIsSet(m_chipProperties.gfxip.activePixelPackerMask,
                            packer + (MaxPixelPackerPerSe * se)))
                        {
                            WideBitfieldSetBit(pInfo->gfxipProperties.shaderCore.activePixelPackerMask,
                                packer + (MaxPixelPackerPerSe * activeSe));
                        }
                    }
                    activeSe++;
                }
            }
#else
            for (uint32 se = 0; se < gfx9Props.numShaderEngines; ++se)
            {
                for (uint32 sa = 0; sa < gfx9Props.numShaderArrays; ++sa)
                {
                    pInfo->gfxipProperties.shaderCore.activeCuMask[se][sa] = gfx9Props.activeCuMask[se][sa];
                }
            }

            static_assert(sizeof(m_chipProperties.gfxip.activePixelPackerMask) ==
                sizeof(pInfo->gfxipProperties.shaderCore.activePixelPackerMask),
                "PAL Device and interface active pixel packer mask sizes do not match!");
            memcpy(pInfo->gfxipProperties.shaderCore.activePixelPackerMask, m_chipProperties.gfxip.activePixelPackerMask,
                sizeof(pInfo->gfxipProperties.shaderCore.activePixelPackerMask));
#endif

            pInfo->gfxipProperties.flags.supportInt8Dot     = gfx9Props.supportInt8Dot;
            pInfo->gfxipProperties.flags.supportInt4Dot     = gfx9Props.supportInt4Dot;
            pInfo->gfxipProperties.flags.support2DRectList  = gfx9Props.support2DRectList;
            pInfo->gfxipProperties.flags.support3dUavZRange = gfx9Props.support3dUavZRange;
            break;
        }

        default:
            // What is this?
            PAL_NOT_IMPLEMENTED();
            break;
        }

        pInfo->gfxipProperties.dynamicLaunchDescSize          = m_chipProperties.gfxip.dynamicLaunchDescSize;

        pInfo->gfxipProperties.maxThreadGroupSize             = m_chipProperties.gfxip.maxThreadGroupSize;
        pInfo->gfxipProperties.maxAsyncComputeThreadGroupSize = m_chipProperties.gfxip.maxAsyncComputeThreadGroupSize;

        pInfo->gfxipProperties.maxComputeThreadGroupCountX    = m_chipProperties.gfxip.maxComputeThreadGroupCountX;
        pInfo->gfxipProperties.maxComputeThreadGroupCountY    = m_chipProperties.gfxip.maxComputeThreadGroupCountY;
        pInfo->gfxipProperties.maxComputeThreadGroupCountZ    = m_chipProperties.gfxip.maxComputeThreadGroupCountZ;

        pInfo->gfxipProperties.maxBufferViewStride = MaxMemoryViewStride;
        pInfo->gfxipProperties.hardwareContexts    = m_chipProperties.gfxip.hardwareContexts;
        pInfo->gfxipProperties.maxPrimgroupSize    = m_chipProperties.gfxip.maxPrimgroupSize;

        pInfo->gfxipProperties.mallSizeInBytes     = m_chipProperties.gfxip.mallSizeInBytes;

        pInfo->gfxipProperties.maxGsOutputVert            = m_chipProperties.gfxip.maxGsOutputVert;
        pInfo->gfxipProperties.maxGsTotalOutputComponents = m_chipProperties.gfxip.maxGsTotalOutputComponents;
        pInfo->gfxipProperties.maxGsInvocations           = m_chipProperties.gfxip.maxGsInvocations;

        pInfo->gfxipProperties.performance.maxGpuClock     = static_cast<float>(m_chipProperties.maxEngineClock);
        pInfo->gfxipProperties.performance.aluPerClock     = static_cast<float>(m_chipProperties.alusPerClock);
        pInfo->gfxipProperties.performance.texPerClock     = static_cast<float>(m_chipProperties.texelsPerClock);
        pInfo->gfxipProperties.performance.primsPerClock   = static_cast<float>(m_chipProperties.primsPerClock);
        pInfo->gfxipProperties.performance.pixelsPerClock  = static_cast<float>(m_chipProperties.pixelsPerClock);
        pInfo->gfxipProperties.performance.gfxipPerfRating = m_chipProperties.enginePerfRating;

        pInfo->gfxipProperties.shaderCore.ldsSizePerCu           = m_chipProperties.gfxip.ldsSizePerCu;
        pInfo->gfxipProperties.shaderCore.ldsSizePerThreadGroup  = m_chipProperties.gfxip.ldsSizePerThreadGroup;
        pInfo->gfxipProperties.shaderCore.ldsGranularity         = m_chipProperties.gfxip.ldsGranularity;
        pInfo->gfxipProperties.shaderCore.numOffchipTessBuffers  = m_chipProperties.gfxip.numOffchipTessBuffers;
        pInfo->gfxipProperties.shaderCore.offchipTessBufferSize  = m_chipProperties.gfxip.offChipTessBufferSize;
        pInfo->gfxipProperties.shaderCore.tessFactorBufSizePerSe = m_chipProperties.gfxip.tessFactorBufferSizePerSe;
        pInfo->gfxipProperties.shaderCore.tccSizeInBytes         = m_chipProperties.gfxip.tccSizeInBytes;
        pInfo->gfxipProperties.shaderCore.tcpSizeInBytes         = m_chipProperties.gfxip.tcpSizeInBytes;
        pInfo->gfxipProperties.shaderCore.maxLateAllocVsLimit    = m_chipProperties.gfxip.maxLateAllocVsLimit;
        pInfo->gfxipProperties.shaderCore.gl1cSizePerSa          = m_chipProperties.gfxip.gl1cSizePerSa;
        pInfo->gfxipProperties.shaderCore.instCacheSizePerCu     = m_chipProperties.gfxip.instCacheSizePerCu;
        pInfo->gfxipProperties.shaderCore.scalarCacheSizePerCu   = m_chipProperties.gfxip.scalarCacheSizePerCu;

        pInfo->gfxipProperties.gl2UncachedCpuCoherency           = m_chipProperties.gfxip.gl2UncachedCpuCoherency;
        pInfo->gfxipProperties.flags.supportGl2Uncached          = m_chipProperties.gfxip.supportGl2Uncached;
        pInfo->gfxipProperties.flags.supportCaptureReplay        = m_chipProperties.gfxip.supportCaptureReplay;
        pInfo->gfxipProperties.flags.supportHsaAbi               = m_chipProperties.gfxip.supportHsaAbi;
        pInfo->gfxipProperties.flags.supportStaticVmid           = m_chipProperties.gfxip.supportStaticVmid;
        pInfo->gfxipProperties.flags.supportFloat32BufferAtomics = m_chipProperties.gfxip.supportFloat32BufferAtomics;
        pInfo->gfxipProperties.flags.supportFloat32ImageAtomics  = m_chipProperties.gfxip.supportFloat32ImageAtomics;

        pInfo->gfxipProperties.flags.supportFloat32BufferAtomicAdd
            = m_chipProperties.gfxip.supportFloat32BufferAtomicAdd;
        pInfo->gfxipProperties.flags.supportFloat32ImageAtomicAdd
            = m_chipProperties.gfxip.supportFloat32ImageAtomicAdd;
        pInfo->gfxipProperties.flags.supportFloat32ImageAtomicMinMax
            = m_chipProperties.gfxip.supportFloat32ImageAtomicMinMax;
        pInfo->gfxipProperties.flags.supportFloat64BufferAtomicMinMax
            = m_chipProperties.gfxip.supportFloat64BufferAtomicMinMax;
        pInfo->gfxipProperties.flags.supportFloat64SharedAtomicMinMax
            = m_chipProperties.gfxip.supportFloat64SharedAtomicMinMax;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 735
        pInfo->gfxipProperties.flags.supportFloat32Atomics       = m_chipProperties.gfxip.supportFloat32BufferAtomics |
                                                                   m_chipProperties.gfxip.supportFloat32ImageAtomics;
#endif

        pInfo->gfxipProperties.srdSizes.bufferView = m_chipProperties.srdSizes.bufferView;
        pInfo->gfxipProperties.srdSizes.imageView  = m_chipProperties.srdSizes.imageView;
        pInfo->gfxipProperties.srdSizes.fmaskView  = m_chipProperties.srdSizes.fmaskView;
        pInfo->gfxipProperties.srdSizes.sampler    = m_chipProperties.srdSizes.sampler;
        pInfo->gfxipProperties.srdSizes.bvh        = m_chipProperties.srdSizes.bvh;

        pInfo->gfxipProperties.nullSrds.pNullBufferView = m_chipProperties.nullSrds.pNullBufferView;
        pInfo->gfxipProperties.nullSrds.pNullImageView  = m_chipProperties.nullSrds.pNullImageView;
        pInfo->gfxipProperties.nullSrds.pNullFmaskView  = m_chipProperties.nullSrds.pNullFmaskView;
        pInfo->gfxipProperties.nullSrds.pNullSampler    = m_chipProperties.nullSrds.pNullSampler;

        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Checks and returns execution state of the device. This is a default implementation for platforms that don't have
// ability to query the GPU execution state from OS or KMD. Returns Success for platforms that can't detect the GPU
// state, which is equivalent to GPU being active.
// NOTE: Part of the IDevice public interface.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 796
Result Device::CheckExecutionState(
    PageFaultStatus* pPageFaultStatus
    )
#else
Result Device::CheckExecutionState(
    PageFaultStatus* pPageFaultStatus
    ) const
#endif
{
    return Result::Success;
}

// =====================================================================================================================
// Reports properties of all GPU memory heaps available to this GPU (e.g., size, whether it is CPU visible or not,
// performance characteristics, etc.).
// NOTE: Part of the public IDevice interface.
Result Device::GetGpuMemoryHeapProperties(
    GpuMemoryHeapProperties info[GpuHeapCount]
    ) const
{
    Result result = Result::ErrorInvalidPointer;

    if (info != nullptr)
    {
        memcpy(&info[0], &m_heapProperties[0], sizeof(m_heapProperties));

        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Reports all format and tiling mode related properties for this GPU.
// NOTE: Part of the IDevice public interface.
Result Device::GetFormatProperties(
    MergedFormatPropertiesTable* pInfo
    ) const
{
    Result result = Result::ErrorInvalidPointer;

    if (pInfo != nullptr)
    {
        memcpy(pInfo, m_pFormatPropertiesTable, sizeof(*m_pFormatPropertiesTable));

        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Reports performance experiment related properties for this GPU.
// NOTE: Part of the IDevice public interface.
Result Device::GetPerfExperimentProperties(
    PerfExperimentProperties* pProperties
    ) const
{
    Result result = Result::ErrorInvalidPointer;

    if (pProperties != nullptr)
    {
        (*pProperties) = m_perfExperimentProperties;

        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Obtains an Entry of the DmaUploadRing.
Result Device::AcquireRingSlot(
    UploadRingSlot* pSlotId)
{
    Util::MutexAuto lock(&m_dmaUploadRingLock);

    PAL_ASSERT(m_pDmaUploadRing != nullptr);
    return m_pDmaUploadRing->AcquireRingSlot(pSlotId);
}

// =====================================================================================================================
// Record commands which upload part of pipeline ELF from CPU to GPU.
size_t Device::UploadUsingEmbeddedData(
    UploadRingSlot  slotId,
    Pal::GpuMemory* pDst,
    gpusize         dstOffset,
    size_t          bytes,
    void**          ppEmbeddedData)
{
    PAL_ASSERT(m_pDmaUploadRing != nullptr);
    return m_pDmaUploadRing->UploadUsingEmbeddedData(slotId,pDst,dstOffset,bytes,ppEmbeddedData);
}

// =====================================================================================================================
/// Determine when a large local heap is available.  A large local heap is any size above 256MB.
bool Device::HasLargeLocalHeap() const
{
    return HeapLogicalSize(GpuHeapLocal) > 256_MiB;
}

// =====================================================================================================================
// Submit command buffer at slotId to the DmaUploadRing's internal dma queue.
// pCompletionFence is used to track when GPU finishes the work of this command buffer.
Result Device::SubmitDmaUploadRing(
    UploadRingSlot    slotId,
    UploadFenceToken* pCompletionFence,
    uint64            pagingFenceVal)
{
    Util::MutexAuto lock(&m_dmaUploadRingLock);

    PAL_ASSERT(m_pDmaUploadRing != nullptr);
    return m_pDmaUploadRing->Submit(slotId, pCompletionFence, pagingFenceVal);
}

// =====================================================================================================================
// pWaiter will wait until DmaUploadRing's internal dma queue's internal fence value reach fenceValue.
Result Device::WaitForPendingUpload(
    Pal::Queue*      pWaiter,
    UploadFenceToken fenceValue)
{
    Util::MutexAuto lock(&m_dmaUploadRingLock);

    PAL_ASSERT(m_pDmaUploadRing != nullptr);
    return m_pDmaUploadRing->WaitForPendingUpload(pWaiter, fenceValue);
}

// =====================================================================================================================
// Determines the size, in bytes, needed to create an IQueue.
// NOTE: Part of the public IDevice interface.
size_t Device::GetQueueSize(
    const QueueCreateInfo& createInfo,
    Result*                pResult
    ) const
{
    if (pResult != nullptr)
    {
        const EngineType engineType = createInfo.engineType;
        const uint32 numAvailable = EngineProperties().perEngine[engineType].numAvailable;

        if ((createInfo.queueType >= QueueTypeCount)   ||
            (engineType >= EngineTypeCount)            ||
            (createInfo.engineIndex >= numAvailable))
        {
            *pResult = Result::ErrorInvalidValue;
        }
        else
        {
            *pResult = Result::Success;
        }
    }

    return QueueContextSize(createInfo) + QueueObjectSize(createInfo);
}

// =====================================================================================================================
// Creates a new IQueue object in preallocated memory provided by the caller.
// NOTE: Part of the public IDevice interface.
Result Device::CreateQueue(
    const QueueCreateInfo& createInfo,
    void*                  pPlacementAddr,
    IQueue**               ppQueue)
{
    Queue* pQueue = ConstructQueueObject(createInfo, pPlacementAddr);

    PAL_ASSERT(pQueue != nullptr);
    Result result = pQueue->Init(&createInfo, VoidPtrInc(pPlacementAddr, QueueObjectSize(createInfo)));

    if (result == Result::Success)
    {
        result = pQueue->LateInit();
    }

    if (result == Result::Success)
    {
        (*ppQueue) = pQueue;
    }
    else
    {
        pQueue->Destroy();
    }

    return result;
}

// =====================================================================================================================
// Determines the size, in bytes, needed to create an IQueue.
// NOTE: Part of the public IDevice interface.
size_t Device::GetMultiQueueSize(
    uint32                 queueCount,
    const QueueCreateInfo* pCreateInfo,
    Result*                pResult
    ) const
{
    PAL_ASSERT(queueCount > 0);

    if (pResult != nullptr)
    {
        Result result = Result::Success;
        for (uint32 qIndex = 0; (qIndex < queueCount) && (result == Result::Success); qIndex++)
        {
            const EngineType engineType = pCreateInfo[qIndex].engineType;
            const uint32 numAvailable   = EngineProperties().perEngine[engineType].numAvailable;

            if ((pCreateInfo[qIndex].queueType >= QueueTypeCount) ||
                (engineType >= EngineTypeCount) ||
                (pCreateInfo[qIndex].engineIndex >= numAvailable))
            {
                result = Result::ErrorInvalidValue;
            }
        }
        *pResult = result;
    }

    size_t multiQueueSize  = 0;
    size_t masterQueueSize = MultiQueueObjectSize(queueCount, pCreateInfo);
    // multiQueue is used for gang submission only for now. gang submission is only supported on WDDM2.5+.
    // Therefore, if client tries to create a multiQueue on other OS, the function returns 0 to indicate a failure.
    if (masterQueueSize > 0)
    {
        multiQueueSize += masterQueueSize;
        for (uint32 qIndex = 0; qIndex < queueCount; qIndex++)
        {
            multiQueueSize += QueueContextSize(pCreateInfo[qIndex]);
        }
    }

    return multiQueueSize;
}

// =====================================================================================================================
// Creates a new IQueue object in preallocated memory provided by the caller.
// NOTE: Part of the public IDevice interface.
Result Device::CreateMultiQueue(
    uint32                 queueCount,
    const QueueCreateInfo* pCreateInfo,
    void*                  pPlacementAddr,
    IQueue**               ppQueue)
{
    Result result = Result::Unsupported;

    Queue* pQueue = ConstructMultiQueueObject(queueCount, pCreateInfo, pPlacementAddr);

    if (pQueue != nullptr)
    {
        result = pQueue->Init(pCreateInfo, VoidPtrInc(pPlacementAddr, MultiQueueObjectSize(queueCount, pCreateInfo)));

        if (result == Result::Success)
        {
            result = pQueue->LateInit();
        }

        if (result == Result::Success)
        {
            (*ppQueue) = pQueue;
        }
        else
        {
            pQueue->Destroy();
        }
    }

    return result;
}

// =====================================================================================================================
// Helper method for determining the size of a Queue context object, in bytes.
size_t Device::QueueContextSize(
    const QueueCreateInfo& createInfo
    ) const
{
    GfxDevice*  pGfxDevice = GetGfxDevice();
    OssDevice*  pOssDevice = GetOssDevice();

    size_t size = 0;
    switch (createInfo.queueType)
    {
    case QueueTypeCompute:
    case QueueTypeUniversal:
        size = (pGfxDevice == nullptr) ? 0 : pGfxDevice->GetQueueContextSize(createInfo);
        break;
    case QueueTypeDma:
        if (pOssDevice == nullptr)
        {
            if (pGfxDevice != nullptr)
            {
                size = pGfxDevice->GetQueueContextSize(createInfo);
            }
        }
        else
        {
            size = pOssDevice->GetQueueContextSize(createInfo);
        }
        break;
    case QueueTypeTimer:
        size = sizeof(QueueContext);
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return size;
}

// =====================================================================================================================
// Returns the largest possible GPU memory alignment requirement for any IGpuMemoryBindable object created on this
// device.  Images are the only objects that can have a required alignment greater than a page, so query addrlib for
// their max requirement.
gpusize Device::GetMaxGpuMemoryAlignment() const
{
    gpusize maxAlignment = MemoryProperties().fragmentSize;

    ADDR_GET_MAX_ALIGNMENTS_OUTPUT addrLibOutput = { };
    ADDR_HANDLE                    addrHandle    = m_pAddrMgr->AddrLibHandle();

    if (AddrGetMaxAlignments(addrHandle, &addrLibOutput) == ADDR_OK)
    {
        maxAlignment = Max<gpusize>(maxAlignment, addrLibOutput.baseAlign);
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    ADDR_GET_MAX_ALIGNMENTS_OUTPUT addrLibMetaOutput = {};

    if (AddrGetMaxMetaAlignments(addrHandle, &addrLibMetaOutput) == ADDR_OK)
    {
        maxAlignment = Max<gpusize>(maxAlignment, addrLibMetaOutput.baseAlign);
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    PAL_ASSERT(IsPowerOfTwo(maxAlignment));
    return maxAlignment;
}

// =====================================================================================================================
// Resets the specified set of fences.
// NOTE: Part of the public IDevice interface.
Result Device::ResetFences(
    uint32              fenceCount,
    IFence*const*       ppFenceList
    ) const
{
    Result result = Result::Success;

    for (uint32 i = 0; i < fenceCount; ++i)
    {
        result = reinterpret_cast<Fence*>(ppFenceList[i])->Reset();
        if (result != Result::Success)
        {
            break;
        }
    }

    return result;
}

// =====================================================================================================================
// Returns the timeout value that the fence is supplied with in terms of nanoseconds.  Takes into consideration any
// timeout override specified in the settings.
uint64  Device::GetTimeoutValueInNs(
    uint64  appTimeoutInNs ///< application-specified timeout, in nanoseconds
    ) const
{
    constexpr uint64  NanosecondsPerSecond = 1000000000ull;

    const auto&  settings    = Settings();
    const uint64 timeoutInNs = (((appTimeoutInNs == 0) || (settings.fenceTimeoutOverrideInSec == 0))
                                ? appTimeoutInNs // no timeout requested by app or no override in settings
                                : settings.fenceTimeoutOverrideInSec * NanosecondsPerSecond);

    return timeoutInNs;
}

// =====================================================================================================================
// Stalls the current thread until one or all of the specified fences have been reached by the GPU.
// All fences must have been submitted at least once before this is called.  Using a zero timeout value returns
// immediately and can be used to determine the status of a set of fences without stalling.
// NOTE: Part of the public IDevice interface.
Result Device::WaitForFences(
    uint32              fenceCount,
    const IFence*const* ppFenceList,
    bool                waitAll,
    uint64              timeout
    ) const
{
    Result result = Result::ErrorInvalidPointer;

    if (fenceCount == 0)
    {
        result = Result::ErrorInvalidValue;
    }
    else if (ppFenceList != nullptr)
    {
        const uint64  timeoutInNs = GetTimeoutValueInNs(timeout);

        result = static_cast<const Fence *>(ppFenceList[0])->WaitForFences(
                                      *this,
                                      fenceCount,
                                      reinterpret_cast<const Fence*const*>(ppFenceList),
                                      waitAll,
                                      timeoutInNs);
    }

    return result;
}

// =====================================================================================================================
// Determines the size in bytes of a CmdAllocator object.
// NOTE: Part of the public IDevice interface.
size_t Device::GetCmdAllocatorSize(
    const CmdAllocatorCreateInfo& createInfo,
    Result*                       pResult
    ) const
{
    return CmdAllocator::GetSize(createInfo, pResult);
}

// =====================================================================================================================
// Constructs and initializes a new CmdAllocator object.
// NOTE: Part of the public IDevice interface.
Result Device::CreateCmdAllocator(
    const CmdAllocatorCreateInfo& createInfo,
    void*                         pPlacementAddr,
    ICmdAllocator**               ppCmdAllocator)
{
    Result result = Result::ErrorInvalidPointer;

    if ((pPlacementAddr != nullptr) && (ppCmdAllocator != nullptr))
    {
        CmdAllocator* pCmdAllocator = PAL_PLACEMENT_NEW(pPlacementAddr) CmdAllocator(this, createInfo);

        result = pCmdAllocator->Init(createInfo, pCmdAllocator + 1);
        if (result != Result::Success)
        {
            pCmdAllocator->Destroy();
            pCmdAllocator = nullptr;
        }

        (*ppCmdAllocator) = pCmdAllocator;
    }

    return result;
}

// =====================================================================================================================
// Constructs and initializes a new CmdAllocator object for PAL-internal use.
Result Device::CreateInternalCmdAllocator(
    const CmdAllocatorCreateInfo& createInfo,
    CmdAllocator**                ppCmdAllocator)
{
    Result result     = Result::ErrorOutOfMemory;
    void*  pObjectMem = PAL_MALLOC(GetCmdAllocatorSize(createInfo, nullptr), GetPlatform(), AllocInternal);

    if (pObjectMem != nullptr)
    {
        result = CreateCmdAllocator(createInfo, pObjectMem, reinterpret_cast<ICmdAllocator**>(ppCmdAllocator));

        if (result != Result::Success)
        {
            PAL_SAFE_FREE(pObjectMem, GetPlatform());
        }
    }

    return result;
}

// =====================================================================================================================
// Determines the size in bytes of a CmdBuffer object.
// NOTE: Part of the public IDevice interface.
size_t Device::GetCmdBufferSize(
    const CmdBufferCreateInfo& createInfo,
    Result*                    pResult
    ) const
{
    Result validationResult = Result::Success;
    size_t size = 0;

    switch (createInfo.queueType)
    {
    case QueueTypeUniversal:
    case QueueTypeCompute:
        size = m_pGfxDevice->GetCmdBufferSize(createInfo);
        break;

    case QueueTypeDma:
        if (m_pOssDevice != nullptr)
        {
            size = m_pOssDevice->GetCmdBufferSize();
        }
        else
        {
            // Some devices have moved DMA operations into the graphics engine...  If there's no
            // OSS device, check if the graphics device can handle this.
            size = m_pGfxDevice->GetCmdBufferSize(createInfo);
        }
        break;

    default:
        PAL_ASSERT_ALWAYS();
        validationResult = Result::ErrorInvalidQueueType;
        break;
    }

    if (pResult != nullptr)
    {
        (*pResult) = validationResult;
    }

    return size;
}

// =====================================================================================================================
// Constructs a new command buffer. Shared implementation for creating either a public or private command buffer.
Result Device::ConstructCmdBuffer(
    const CmdBufferCreateInfo& createInfo,
    void*                      pPlacementAddr,
    CmdBuffer**                ppCmdBuffer
    ) const
{
    PAL_ASSERT((pPlacementAddr != nullptr) && (ppCmdBuffer != nullptr));
    CmdBuffer* pCmdBuffer = nullptr;
    Result     result     = Result::Success;

    switch (createInfo.queueType)
    {
        case QueueTypeUniversal:
        case QueueTypeCompute:
            result = m_pGfxDevice->CreateCmdBuffer(createInfo, pPlacementAddr, &pCmdBuffer);
            break;

        case QueueTypeDma:
            if (m_pOssDevice != nullptr)
            {
                result = m_pOssDevice->CreateCmdBuffer(createInfo, pPlacementAddr, &pCmdBuffer);
            }
            else
            {
                // Some devices have moved DMA operations into the graphics engine...  If there's no
                // OSS device, check if the graphics device can handle this.
                result = m_pGfxDevice->CreateCmdBuffer(createInfo, pPlacementAddr, &pCmdBuffer);
            }
            break;

        default:
            result = Result::ErrorInvalidQueueType;
            PAL_ASSERT_ALWAYS();
            break;
    }

    if (result == Result::Success)
    {
        (*ppCmdBuffer) = pCmdBuffer;
    }

    return result;
}

// =====================================================================================================================
// Creates a new CmdBuffer object in preallocated memory provided by the caller.
// NOTE: Part of the public IDevice interface.
Result Device::CreateCmdBuffer(
    const CmdBufferCreateInfo& createInfo,
    void*                      pPlacementAddr,
    ICmdBuffer**               ppCmdBuffer)
{
    Result result = Result::ErrorInvalidPointer;

    if ((pPlacementAddr != nullptr) && (ppCmdBuffer != nullptr))
    {
        CmdBuffer* pCmdBuffer = nullptr;
        result = ConstructCmdBuffer(createInfo, pPlacementAddr, &pCmdBuffer);

        if (result == Result::Success)
        {
            constexpr CmdBufferInternalCreateInfo InternalInfo = {};
            result = pCmdBuffer->Init(InternalInfo);

            if (result != Result::Success)
            {
                pCmdBuffer->Destroy();
                pCmdBuffer = nullptr;
            }

            (*ppCmdBuffer) = pCmdBuffer;
        }
    }

    return result;
}

// =====================================================================================================================
// Constructs and initializes a new CmdBuffer object for PAL-internal use.
Result Device::CreateInternalCmdBuffer(
    const CmdBufferCreateInfo&         createInfo,
    const CmdBufferInternalCreateInfo& internalInfo,
    CmdBuffer**                        ppCmdBuffer)
{
    Result result     = Result::ErrorOutOfMemory;
    void*  pObjectMem = PAL_MALLOC(GetCmdBufferSize(createInfo, nullptr), GetPlatform(), AllocInternal);

    if (pObjectMem != nullptr)
    {
        result = CreateCmdBuffer(createInfo, pObjectMem, reinterpret_cast<ICmdBuffer**>(ppCmdBuffer));

        if (result != Result::Success)
        {
            PAL_SAFE_FREE(pObjectMem, GetPlatform());
        }
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetIndirectCmdGeneratorSize(
    const IndirectCmdGeneratorCreateInfo& createInfo,
    Result*                               pResult) const
{
    size_t size = 0;

    if (GetGfxDevice() != nullptr)
    {
        size = GetGfxDevice()->GetIndirectCmdGeneratorSize(createInfo, pResult);
    }
    else if (pResult != nullptr)
    {
        (*pResult) = Result::Unsupported;
    }

    return size;
}

// =====================================================================================================================
Result Device::CreateIndirectCmdGenerator(
    const IndirectCmdGeneratorCreateInfo& createInfo,
    void*                                 pPlacementAddr,
    IIndirectCmdGenerator**               ppGenerator) const
{
    Result result = Result::Unsupported;

    if ((pPlacementAddr == nullptr) || (ppGenerator == nullptr))
    {
        result = Result::ErrorInvalidPointer;
    }
    else if (GetGfxDevice() != nullptr)
    {
        result = GetGfxDevice()->CreateIndirectCmdGenerator(createInfo, pPlacementAddr, ppGenerator);
    }

    return result;
}

// =====================================================================================================================
// Determines the size in bytes of a QueueSemaphore object.
size_t Device::GetQueueSemaphoreSize(
    const QueueSemaphoreCreateInfo& createInfo,
    Result*                         pResult
    ) const
{
    if (pResult != nullptr)
    {
        *pResult = QueueSemaphore::ValidateInit(this, createInfo);
    }
#if PAL_ENABLE_PRINTS_ASSERTS
    else
    {
        PAL_ASSERT(QueueSemaphore::ValidateInit(this, createInfo) == Result::Success);
    }
#endif

    return sizeof(MasterQueueSemaphore);
}

// =====================================================================================================================
// Creates a new QueueSemaphore object in preallocated memory provided by the caller.
Result Device::CreateQueueSemaphore(
    const QueueSemaphoreCreateInfo& createInfo,
    void*                           pPlacementAddr,
    IQueueSemaphore**               ppQueueSemaphore)
{
    Result result = Result::ErrorInvalidPointer;

    if ((pPlacementAddr != nullptr) && (ppQueueSemaphore != nullptr))
    {
        MasterQueueSemaphore* pSemaphore = PAL_PLACEMENT_NEW(pPlacementAddr) MasterQueueSemaphore(this);

        result = pSemaphore->Init(createInfo);
        if (result != Result::Success)
        {
            pSemaphore->Destroy();
            pSemaphore = nullptr;
        }

        (*ppQueueSemaphore) = pSemaphore;
    }

    return result;
}

// =====================================================================================================================
// Determines the size in bytes of a QueueSemaphore object which is being opened from another shared Semaphore object.
size_t Device::GetSharedQueueSemaphoreSize(
    const QueueSemaphoreOpenInfo& openInfo,
    Result*                       pResult
    ) const
{
    if (pResult != nullptr)
    {
        *pResult = QueueSemaphore::ValidateOpen(this, openInfo);
    }
#if PAL_ENABLE_PRINTS_ASSERTS
    else
    {
        PAL_ASSERT(QueueSemaphore::ValidateOpen(this, openInfo) == Result::Success);
    }
#endif

    return sizeof(OpenedQueueSemaphore);
}

// =====================================================================================================================
// Creates a new QueueSemaphore object in preallocated memory provided by the caller. The new Semaphore is opened from
// a shareable Semaphore which was created on a different Device.
Result Device::OpenSharedQueueSemaphore(
    const QueueSemaphoreOpenInfo& openInfo,
    void*                         pPlacementAddr,
    IQueueSemaphore**             ppQueueSemaphore)
{
    Result result = Result::ErrorInvalidPointer;

    if ((pPlacementAddr != nullptr) && (ppQueueSemaphore != nullptr))
    {
        OpenedQueueSemaphore* pSemaphore = PAL_PLACEMENT_NEW(pPlacementAddr) OpenedQueueSemaphore(this);

        result = pSemaphore->Open(openInfo);
        if (result != Result::Success)
        {
            pSemaphore->Destroy();
            pSemaphore = nullptr;
        }

        (*ppQueueSemaphore) = pSemaphore;
    }

    return result;
}

// =====================================================================================================================
// Determines the size in bytes of a QueueSemaphore object which is being opened from external shared handle.
size_t Device::GetExternalSharedQueueSemaphoreSize(
    const ExternalQueueSemaphoreOpenInfo& openInfo,
    Result*                               pResult
    ) const
{
    if (pResult != nullptr)
    {
        if (openInfo.externalSemaphore == 0)
        {
            (*pResult) = Result::ErrorInvalidPointer;
        }
        else
        {
            (*pResult) = Result::Success;
        }
    }

    return sizeof(MasterQueueSemaphore);
}

// =====================================================================================================================
// Creates a new QueueSemaphore object in preallocated memory provided by the caller. The new Semaphore is opened from
// an external shared handle.
Result Device::OpenExternalSharedQueueSemaphore(
    const ExternalQueueSemaphoreOpenInfo& openInfo,
    void*                                 pPlacementAddr,
    IQueueSemaphore**                     ppQueueSemaphore)
{
    PAL_ASSERT((pPlacementAddr != nullptr) && (ppQueueSemaphore != nullptr));

    MasterQueueSemaphore* pSemaphore = PAL_PLACEMENT_NEW(pPlacementAddr) MasterQueueSemaphore(this);

    Result result = pSemaphore->OpenExternal(openInfo);
    if (result != Result::Success)
    {
        pSemaphore->Destroy();
        pSemaphore = nullptr;
    }
    else
    {
        pSemaphore->InitExternal();
    }

    (*ppQueueSemaphore) = pSemaphore;

    return result;
}

// =====================================================================================================================
// Constructs and initializes a new GpuMemory object for PAL-internal use.
Result Device::CreateInternalFence(
    const FenceCreateInfo& createInfo,
    Fence**                ppFence
    ) const
{
    Result result = Result::ErrorInvalidPointer;

    if (ppFence != nullptr)
    {
        void* pObjectMem = PAL_MALLOC(GetFenceSize(nullptr), GetPlatform(), AllocInternal);
        if (pObjectMem != nullptr)
        {
            result = CreateFence(createInfo, pObjectMem, reinterpret_cast<IFence**>(ppFence));
            if (IsErrorResult(result))
            {
                PAL_SAFE_FREE(pObjectMem, GetPlatform());
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
// Determines the size in bytes of a GpuEvent object.
size_t Device::GetGpuEventSize(
    const GpuEventCreateInfo& createInfo,
    Result*                   pResult
    ) const
{
    if (pResult != nullptr)
    {
        (*pResult) = Result::Success;
    }

    return sizeof(GpuEvent);
}

// =====================================================================================================================
// Creates a new GpuEvent object in preallocated memory provided by the caller.
Result Device::CreateGpuEvent(
    const GpuEventCreateInfo& createInfo,
    void*                     pPlacementAddr,
    IGpuEvent**               ppGpuEvent)
{
    PAL_ASSERT((pPlacementAddr != nullptr) && (ppGpuEvent != nullptr));

    (*ppGpuEvent) = PAL_PLACEMENT_NEW(pPlacementAddr) GpuEvent(createInfo, this);

    return Result::Success;
}

// =====================================================================================================================
// Gets the query pool size.
size_t Device::GetQueryPoolSize(
    const QueryPoolCreateInfo& createInfo,
    Result*                    pResult) const
{
    size_t result;

    switch (createInfo.queryPoolType)
    {
    // GFXIP query pool types
    case QueryPoolType::Occlusion:
    case QueryPoolType::PipelineStats:
    case QueryPoolType::StreamoutStats:
        if (m_pGfxDevice == nullptr)
        {
            result = 0;
        }
        else
        {
            result = m_pGfxDevice->GetQueryPoolSize(createInfo, pResult);
        }
        break;

    default:
        result = 0;
        break;
    }

    return result;
}

// =====================================================================================================================
// Creates a new Query pool object
Result Device::CreateQueryPool(
    const QueryPoolCreateInfo& createInfo,
    void*                      pPlacementAddr,
    IQueryPool**               ppQueryPool) const
{
    Result result;

    switch (createInfo.queryPoolType)
    {
    // GFXIP query pool types
    case QueryPoolType::Occlusion:
    case QueryPoolType::PipelineStats:
    case QueryPoolType::StreamoutStats:
        if (m_pGfxDevice == nullptr)
        {
            result = Result::ErrorUnavailable;
        }
        else
        {
            result = m_pGfxDevice->CreateQueryPool(createInfo, pPlacementAddr, ppQueryPool);
        }
        break;

    default:
        result = Result::ErrorInvalidOrdinal;
        break;
    }

    return result;
}

// =====================================================================================================================
// Helper function to validate whether the GPU memory bind is valid with the specified offset, size, and alignment.
Result Device::ValidateBindObjectMemoryInput(
    const IGpuMemory* pIGpuMemory,
    gpusize           offset,
    gpusize           objMemSize,
    gpusize           objAlignment,
    bool              allowVirtualBinding)
{
    Result result = Result::Success;

    if (pIGpuMemory != nullptr)
    {
        const GpuMemory*const pGpuMemory = static_cast<const GpuMemory*>(pIGpuMemory);

        if (pGpuMemory->IsVirtual() && (allowVirtualBinding == false))
        {
            result = Result::ErrorUnavailable;
        }
        else if (pGpuMemory->Desc().size < (objMemSize + offset))
        {
            // Check that offset plus the required GPU memory size is completely within the memory object.
            result = Result::ErrorInvalidMemorySize;
        }
        else if (((pGpuMemory->Desc().gpuVirtAddr + offset) % objAlignment) != 0)
        {
            result = Result::ErrorInvalidAlignment;
        }
    }

    return result;
}

// =====================================================================================================================
// Determines the size in bytes of a GpuMemory object.
size_t Device::GetGpuMemorySize(
    const GpuMemoryCreateInfo& createInfo,
    Result*                    pResult
    ) const
{
    if (pResult != nullptr)
    {
        *pResult = GpuMemory::ValidateCreateInfo(this, createInfo);
    }

    return GpuMemoryObjectSize();
}

// =====================================================================================================================
// Constructs and initializes a new GpuMemory object.
// NOTE: Part of the public IDevice interface.
Result Device::CreateGpuMemory(
    const GpuMemoryCreateInfo& createInfo,
    void*                      pPlacementAddr,
    IGpuMemory**               ppGpuMemory)
{
    GpuMemoryInternalCreateInfo internalInfo = {};
    internalInfo.flags.isClient = 1;

    if (createInfo.flags.busAddressable || createInfo.flags.gl2Uncached)
    {
        internalInfo.mtype = MType::Uncached;
    }

    Result result = Result::ErrorInvalidPointer;

    if ((pPlacementAddr != nullptr) && (ppGpuMemory != nullptr))
    {
        GpuMemory* pGpuMemory = ConstructGpuMemoryObject(pPlacementAddr);
        result = pGpuMemory->Init(createInfo, internalInfo);
        if (IsErrorResult(result))
        {
            pGpuMemory->Destroy();
            pGpuMemory = nullptr;
        }
        else
        {
            m_pPlatform->GetGpuMemoryEventProvider()->LogCreateGpuMemoryEvent(pGpuMemory);
        }

        (*ppGpuMemory) = pGpuMemory;
    }

    return result;
}

// =====================================================================================================================
// Constructs and initializes a new GpuMemory object for PAL-internal use.
Result Device::CreateInternalGpuMemory(
    const GpuMemoryCreateInfo&         createInfo,
    const GpuMemoryInternalCreateInfo& internalInfo,
    GpuMemory**                        ppGpuMemory)
{
    Result result = Result::ErrorInvalidPointer;

    if (ppGpuMemory != nullptr)
    {
        void* pObjectMem = PAL_MALLOC(GetGpuMemorySize(createInfo, nullptr), GetPlatform(), AllocInternal);
        if (pObjectMem != nullptr)
        {
            result = CreateInternalGpuMemory(createInfo, internalInfo, pObjectMem, ppGpuMemory);
            if (IsErrorResult(result))
            {
                PAL_SAFE_FREE(pObjectMem, GetPlatform());
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
// Constructs and initializes a new GpuMemory object for PAL-internal use.
Result Device::CreateInternalGpuMemory(
    const GpuMemoryCreateInfo&         createInfo,
    const GpuMemoryInternalCreateInfo& internalInfo,
    void*                              pPlacementAddr,
    GpuMemory**                        ppGpuMemory)
{
    Result result = Result::ErrorInvalidPointer;

    if ((pPlacementAddr != nullptr) && (ppGpuMemory != nullptr))
    {
        (*ppGpuMemory) = ConstructGpuMemoryObject(pPlacementAddr);
        result = (*ppGpuMemory)->Init(createInfo, internalInfo);
        if (IsErrorResult(result))
        {
            (*ppGpuMemory)->Destroy();
            (*ppGpuMemory) = nullptr;
        }
        else if (m_pPlatform->GetGpuMemoryEventProvider() != nullptr)
        {
            m_pPlatform->GetGpuMemoryEventProvider()->LogCreateGpuMemoryEvent((*ppGpuMemory));
        }
    }

    return result;
}

// =====================================================================================================================
// Determines the size in bytes of a pinned GpuMemory object.
// NOTE: Part of the public IDevice interface.
size_t Device::GetPinnedGpuMemorySize(
    const PinnedGpuMemoryCreateInfo& createInfo,
    Result*                          pResult
    ) const
{
    if (pResult != nullptr)
    {
        *pResult = GpuMemory::ValidatePinInfo(this, createInfo);
    }
#if PAL_ENABLE_PRINTS_ASSERTS
    else
    {
        PAL_ASSERT(GpuMemory::ValidatePinInfo(this, createInfo) == Result::Success);
    }
#endif

    return GpuMemoryObjectSize();
}

// =====================================================================================================================
// Constructs and initializes a new GpuMemory object created from pinnning system memory to GPU address space.
// NOTE: Part of the public IDevice interface.
Result Device::CreatePinnedGpuMemory(
    const PinnedGpuMemoryCreateInfo& createInfo,
    void*                            pPlacementAddr,
    IGpuMemory**                     ppGpuMemory)
{
    Result result = Result::ErrorInvalidPointer;

    if ((pPlacementAddr != nullptr) && (ppGpuMemory != nullptr))
    {
        GpuMemory* pGpuMemory = ConstructGpuMemoryObject(pPlacementAddr);
        result = pGpuMemory->Init(createInfo);
        if (result != Result::Success)
        {
            pGpuMemory->Destroy();
            pGpuMemory = nullptr;
        }
        else
        {
            m_pPlatform->GetGpuMemoryEventProvider()->LogCreateGpuMemoryEvent(pGpuMemory);
        }

        (*ppGpuMemory) = pGpuMemory;
    }

    return result;
}

// =====================================================================================================================
// Determines the size in bytes of a SVM Memory object.
// NOTE: Part of the public IDevice interface.
size_t Device::GetSvmGpuMemorySize(
    const SvmGpuMemoryCreateInfo& createInfo,
    Result*                       pResult
    ) const
{
    if (pResult != nullptr)
    {
        *pResult = GpuMemory::ValidateSvmInfo(this, createInfo);
    }

    return GpuMemoryObjectSize();
}

// =====================================================================================================================
// Constructs and initializes a new SVM GpuMemory object.
// NOTE: Part of the public IDevice interface.
Result Device::CreateSvmGpuMemory(
    const SvmGpuMemoryCreateInfo& createInfo,
    void*                         pPlacementAddr,
    IGpuMemory**                  ppGpuMemory)
{
    Result result = Result::Success;

    if (GetPlatform()->SvmModeEnabled() == false)
    {
        result = Result::Unsupported;
    }
    if ((pPlacementAddr == nullptr) || (ppGpuMemory == nullptr))
    {
        result = Result::ErrorInvalidPointer;
    }
    if (result == Result::Success)
    {
        GpuMemory* pGpuMemory = ConstructGpuMemoryObject(pPlacementAddr);
        result = pGpuMemory->Init(createInfo);
        if (result != Result::Success)
        {
            pGpuMemory->Destroy();
            pGpuMemory = nullptr;
        }
        else
        {
            m_pPlatform->GetGpuMemoryEventProvider()->LogCreateGpuMemoryEvent(pGpuMemory);
        }

        (*ppGpuMemory) = pGpuMemory;
    }

    return result;
}

// =====================================================================================================================
// Determines the size in bytes of a shared GpuMemory object.
// NOTE: Part of the public IDevice interface.
size_t Device::GetSharedGpuMemorySize(
    const GpuMemoryOpenInfo& openInfo,
    Result*                  pResult
    ) const
{
    if (pResult != nullptr)
    {
        *pResult = GpuMemory::ValidateOpenInfo(this, openInfo);
    }
#if PAL_ENABLE_PRINTS_ASSERTS
    else
    {
        PAL_ASSERT(GpuMemory::ValidateOpenInfo(this, openInfo) == Result::Success);
    }
#endif

    return GpuMemoryObjectSize();
}

// =====================================================================================================================
// Constructs and initializes a new shared GpuMemory object.
// NOTE: Part of the public IDevice interface.
Result Device::OpenSharedGpuMemory(
    const GpuMemoryOpenInfo& openInfo,
    void*                    pPlacementAddr,
    IGpuMemory**             ppGpuMemory)
{
    Result result = Result::ErrorInvalidPointer;

    if ((pPlacementAddr != nullptr) && (ppGpuMemory != nullptr))
    {
        GpuMemory* pGpuMemory = ConstructGpuMemoryObject(pPlacementAddr);
        result = pGpuMemory->Init(openInfo);
        if (result != Result::Success)
        {
            pGpuMemory->Destroy();
            pGpuMemory = nullptr;
        }
        else
        {
            m_pPlatform->GetGpuMemoryEventProvider()->LogCreateGpuMemoryEvent(pGpuMemory);
        }

        (*ppGpuMemory) = pGpuMemory;
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetPeerGpuMemorySize(
    const PeerGpuMemoryOpenInfo& openInfo,
    Result*                      pResult
    ) const
{
    if (pResult != nullptr)
    {
        *pResult = GpuMemory::ValidatePeerOpenInfo(this, openInfo);
    }
#if PAL_ENABLE_PRINTS_ASSERTS
    else
    {
        PAL_ASSERT(GpuMemory::ValidatePeerOpenInfo(this, openInfo) == Result::Success);
    }
#endif

    return GpuMemoryObjectSize();
}

// =====================================================================================================================
// Constructs and initializes a new peer GpuMemory object.
// NOTE: Part of the public IDevice interface.
Result Device::OpenPeerGpuMemory(
    const PeerGpuMemoryOpenInfo& openInfo,
    void*                        pPlacementAddr,
    IGpuMemory**                 ppGpuMemory)
{
    Result result = Result::ErrorInvalidPointer;

    if ((pPlacementAddr != nullptr) && (ppGpuMemory != nullptr))
    {
        GpuMemory* pGpuMemory = ConstructGpuMemoryObject(pPlacementAddr);
        result = pGpuMemory->Init(openInfo);
        if (result != Result::Success)
        {
            pGpuMemory->Destroy();
            pGpuMemory = nullptr;
        }
        else
        {
            m_pPlatform->GetGpuMemoryEventProvider()->LogCreateGpuMemoryEvent(pGpuMemory);
        }

        (*ppGpuMemory) = pGpuMemory;
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetExternalSharedGpuMemorySize(
    Result* pResult
    ) const
{
    if (pResult != nullptr)
    {
        (*pResult) = Result::Success;
    }

    return GpuMemoryObjectSize();
}

// =====================================================================================================================
void Device::GetPeerImageSizes(
    const PeerImageOpenInfo& openInfo,
    size_t*                  pPeerImageSize,
    size_t*                  pPeerGpuMemorySize,
    Result*                  pResult
    ) const
{
    const Image* pOrigImage = static_cast<const Image*>(openInfo.pOriginalImage);

    if (pResult != nullptr)
    {
        PeerGpuMemoryOpenInfo peerGpuOpenInfo = {};
        peerGpuOpenInfo.pOriginalMem = pOrigImage->GetBoundGpuMemory().Memory();

        *pResult = GpuMemory::ValidatePeerOpenInfo(this, peerGpuOpenInfo);
    }
#if PAL_ENABLE_PRINTS_ASSERTS
    else
    {
        PeerGpuMemoryOpenInfo peerGpuOpenInfo = {};
        peerGpuOpenInfo.pOriginalMem = pOrigImage->GetBoundGpuMemory().Memory();

        PAL_ASSERT(GpuMemory::ValidatePeerOpenInfo(this, peerGpuOpenInfo) == Result::Success);
    }
#endif

    (*pPeerImageSize)     = GetImageSize(pOrigImage->GetImageCreateInfo(), pResult);
    (*pPeerGpuMemorySize) = GpuMemoryObjectSize();
}

// =====================================================================================================================
// Opens, creates, and initializes a peer image and the associated peer gpu memory.
// Note: To support suballocated images, the peer image can be bound to a shared memory allocation. Ensure that
//       pGpuMemoryPlacementAddr is null and ppGpuMemory is referencing the shared memory allocation.
Result Device::OpenPeerImage(
    const PeerImageOpenInfo& openInfo,
    void*                    pImagePlacementAddr,
    void*                    pGpuMemoryPlacementAddr,
    IImage**                 ppImage,
    IGpuMemory**             ppGpuMemory)
{
    const Image*          pOrigImage       = static_cast<const Image*>(openInfo.pOriginalImage);
    const BoundGpuMemory& pOrigBoundGpuMem = pOrigImage->GetBoundGpuMemory();
    PAL_ASSERT(pOrigImage != nullptr);

    ImageInternalCreateInfo internalInfo = pOrigImage->GetInternalCreateInfo();
    internalInfo.pOriginalImage          = pOrigImage;

    // The original image has memory associated with it, so create an Image on this device that is identical to the
    // original image (which was created on a separate device)
    Result result = CreateInternalImage(pOrigImage->GetImageCreateInfo(),
                                        internalInfo,
                                        pImagePlacementAddr,
                                        reinterpret_cast<Image**>(ppImage));

    if (result == Result::Success)
    {
        if (pGpuMemoryPlacementAddr != nullptr)
        {
            // Ok, we have a new image object.  Now we need a peer version of the memory object that is bound to the
            // original image.  The peer memory corresponds to the entire memory object bound to the original image, not
            // just the portion of the memory that corresponds to the image.
            PeerGpuMemoryOpenInfo gpuOpenInfo = {};
            gpuOpenInfo.pOriginalMem = pOrigBoundGpuMem.Memory();

            result = OpenPeerGpuMemory(gpuOpenInfo, pGpuMemoryPlacementAddr, ppGpuMemory);
        }
        else
        {
            // We have already opened peer memory for the image. Assert that we are referencing
            // the memory with the correct device.
            PAL_ASSERT(this == static_cast<const GpuMemory*>(*ppGpuMemory)->GetDevice());
        }
    }

    if (result == Result::Success)
    {
        // Everything worked, final step here is to bind our memory to our image
        Image*  pNewImage = static_cast<Image*>(*ppImage);

        // The peer memory mirrors the entire actual "real" memory, so we need to bind the peer memory at the same
        // offset that the "real" memory is bound with.
        result = pNewImage->BindGpuMemory(*ppGpuMemory, pOrigBoundGpuMem.Offset());
    }

    if (result != Result::Success)
    {
        if ((*ppImage) != nullptr)
        {
            (*ppImage)->Destroy();
            (*ppImage) = nullptr;
        }

        if (pGpuMemoryPlacementAddr != nullptr)
        {
            if ((*ppGpuMemory) != nullptr)
            {
                (*ppGpuMemory)->Destroy();
                (*ppGpuMemory) = nullptr;
            }
        }
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetColorTargetViewSize(
    Result* pResult
    ) const
{
    return (m_pGfxDevice != nullptr) ? m_pGfxDevice->GetColorTargetViewSize(pResult) : 0;
}

// =====================================================================================================================
// Creates and initializes a new color target view
Result Device::CreateColorTargetView(
    const ColorTargetViewCreateInfo& createInfo,
    void*                            pPlacementAddr,
    IColorTargetView**               ppColorTargetView
    ) const
{
    constexpr ColorTargetViewInternalCreateInfo NullInternalInfo = {};

    return (m_pGfxDevice != nullptr) ?
        m_pGfxDevice->CreateColorTargetView(createInfo, NullInternalInfo, pPlacementAddr, ppColorTargetView) :
        Result::ErrorUnavailable;
}

// =====================================================================================================================
// Creates and initializes a new color target view for PAL internal use
Result Device::CreateInternalColorTargetView(
    const ColorTargetViewCreateInfo&  createInfo,
    ColorTargetViewInternalCreateInfo internalInfo,
    void*                             pPlacementAddr,
    IColorTargetView**                ppColorTargetView
    ) const
{
    static_assert(sizeof(ColorTargetViewInternalCreateInfo) <= sizeof(uint64), "Consider passing by ref.");

    return (m_pGfxDevice != nullptr) ?
        m_pGfxDevice->CreateColorTargetView(createInfo, internalInfo, pPlacementAddr, ppColorTargetView) :
        Result::ErrorUnavailable;
}

// =====================================================================================================================
size_t Device::GetDepthStencilViewSize(
    Result* pResult
    ) const
{
    return (m_pGfxDevice != nullptr) ? m_pGfxDevice->GetDepthStencilViewSize(pResult) : 0;
}

// =====================================================================================================================
// Creates and initializes a new depth stencil view
Result Device::CreateDepthStencilView(
    const DepthStencilViewCreateInfo& createInfo,
    void*                             pPlacementAddr,
    IDepthStencilView**               ppDepthStencilView
    ) const
{
    constexpr DepthStencilViewInternalCreateInfo NullInternalInfo = {};

    Result result = Result::Success;

    if (m_pGfxDevice == nullptr)
    {
        result = Result::ErrorUnavailable;
    }

    if (result == Result::Success)
    {
        if (createInfo.flags.stencilOnlyView &&
            (createInfo.pImage->GetImageCreateInfo().usageFlags.stencilOnlyTarget == 0))
        {
            result = Result::ErrorInvalidFlags;
        }
    }

    if (result == Result::Success)
    {
        result = m_pGfxDevice->CreateDepthStencilView(createInfo, NullInternalInfo, pPlacementAddr, ppDepthStencilView);
    }

    return result;
}

// =====================================================================================================================
// Creates and initializes a new depth stencil view for PAL internal use
Result Device::CreateInternalDepthStencilView(
    const DepthStencilViewCreateInfo&         createInfo,
    const DepthStencilViewInternalCreateInfo& internalInfo,
    void*                                     pPlacementAddr,
    IDepthStencilView**                       ppDepthStencilView
    ) const
{
    return (m_pGfxDevice != nullptr) ?
        m_pGfxDevice->CreateDepthStencilView(createInfo, internalInfo, pPlacementAddr, ppDepthStencilView) :
        Result::ErrorUnavailable;
}
// =====================================================================================================================
// Creates and initializes a new graphics pipeline.
Result Device::CreateGraphicsPipeline(
    const GraphicsPipelineCreateInfo& createInfo,
    void*                             pPlacementAddr,
    IPipeline**                       ppPipeline)
{
    constexpr GraphicsPipelineInternalCreateInfo NullInternalInfo = {};

    return (m_pGfxDevice != nullptr) ?
            m_pGfxDevice->CreateGraphicsPipeline(createInfo, NullInternalInfo, pPlacementAddr,
                                                 createInfo.flags.clientInternal, ppPipeline) :
            Result::ErrorUnavailable;
}

// =====================================================================================================================
// Determine if hardware accelerated stereo rendering can be enabled for given graphic pipeline.
bool Device::DetermineHwStereoRenderingSupported(
    const GraphicPipelineViewInstancingInfo& viewInstancingInfo
    ) const
{
    return (m_pGfxDevice != nullptr) ?
            m_pGfxDevice->DetermineHwStereoRenderingSupported(viewInstancingInfo) :
            false;
}

// =====================================================================================================================
// Compares an image plane's format with a view format and returns whether or not the view format is compatible
// with the image.
static Result ValidateCompatibleImageViewFormats(
    const Image& image,     // Image object
    uint32       plane,     // Image plane
    ChNumFormat  viewFmt)   // View format
{
    Result result = Result::ErrorFormatIncompatibleWithImageFormat;

    const ImageCreateInfo& imageInfo = image.GetImageCreateInfo();
    ChNumFormat            imageFmt  = imageInfo.swizzledFormat.format;

    if (Formats::IsYuvPlanar(imageFmt))
    {
        // YUV planar images only allow image view formats which match that of the base subresource of the view plane.
        const SubresId baseSubRes = { plane, 0, 0, };
        imageFmt = image.SubresourceInfo(baseSubRes)->format.format;
    }

    const uint32 imageBpp = Formats::BitsPerPixel(imageFmt);
    const uint32 viewBpp  = Formats::BitsPerPixel(viewFmt);

    if (image.IsColorPlane(plane) || Formats::IsYuv(imageInfo.swizzledFormat.format))
    {
        // Normally, YUV and color images allow any image view format which matches the bits-per-pixel of the base
        // image.
        if (imageBpp == viewBpp)
        {
            result = Result::Success;
        }
        // However, if the image format is YUV-packed and the view format is not, then one exception is allowed:
        // the view format's bits-per-pixel can be twice that of the image format.
        else if (Formats::IsYuvPacked(imageFmt)           &&
                 (Formats::IsYuvPacked(viewFmt) == false) &&
                 ((imageBpp << 1) == viewBpp))
        {
            result = Result::Success;
        }
    }
    else if (Formats::IsDepthStencilOnly(viewFmt))
    {
        result = Result::ErrorInvalidFormat;
    }
    // Depth/stencil images introduce some exceptions to the above, because they can have multiple planes (depth and
    // stencil), but a single image view can only access one of these planes:
    else if (image.IsDepthPlane(plane))
    {
        if ((viewBpp == 32) &&
            ((imageFmt == ChNumFormat::X32_Float) || (imageFmt == ChNumFormat::D32_Float_S8_Uint)))
        {
            // The view can have an R32 channel format when viewing the depth plane of an R32 or an R32G8
            // depth/stencil image.
            result = Result::Success;
        }
        else if ((viewBpp == 16) &&
                 ((imageFmt == ChNumFormat::X16_Unorm) || (imageFmt == ChNumFormat::D16_Unorm_S8_Uint)))
        {
            // The view can have an R16 channel format when viewing the depth plane of an
            // R16 or an R16G8 depth/stencil image.
            result = Result::Success;
        }
        else
        {
            result = Result::ErrorFormatIncompatibleWithImagePlane;
        }
    }
    else if (image.IsStencilPlane(plane))
    {
        if ((viewFmt == ChNumFormat::X8_Uint) &&
            ((imageFmt == ChNumFormat::D32_Float_S8_Uint) ||
             (imageFmt == ChNumFormat::D16_Unorm_S8_Uint) ||
             (imageFmt == ChNumFormat::X8_Uint)))
        {
            // The view can have an R8 channel format when viewing the stencil plane of an
            // R32G8 or R16G8 depth/stencil image or an R8 stencil-only image.
            result = Result::Success;
        }
        else
        {
            result = Result::ErrorFormatIncompatibleWithImagePlane;
        }
    }

    return result;
}

// =====================================================================================================================
// Error checks ImageViewInfo parameters for an image view SRD.
Result Device::ValidateImageViewInfo(
    const ImageViewInfo& info
    ) const
{
    Result result = Result::Success;

    const auto*const       pImage     = static_cast<const Image*>(info.pImage);
    const ImageCreateInfo& imgInfo    = pImage->GetImageCreateInfo();
    const uint32           viewPlane  = info.subresRange.startSubres.plane;
    const SwizzledFormat   viewFmt    = info.swizzledFormat;

    // Verify the view plane specified is valid for the image.
    if (viewPlane >= pImage->GetImageInfo().numPlanes)
    {
        result = Result::ErrorImagePlaneUnavailable;
    }
    // Verify the image object has read or write access flags or both set.
    else if (!pImage->IsShaderReadable() && !pImage->IsShaderWritable())
    {
        result = Result::ErrorImageNotShaderAccessible;
    }
    // Check swizzle
    else if ((Formats::IsValidChannelSwizzle(viewFmt.format, info.swizzledFormat.swizzle.r) == false) ||
             (Formats::IsValidChannelSwizzle(viewFmt.format, info.swizzledFormat.swizzle.g) == false) ||
             (Formats::IsValidChannelSwizzle(viewFmt.format, info.swizzledFormat.swizzle.b) == false) ||
             (Formats::IsValidChannelSwizzle(viewFmt.format, info.swizzledFormat.swizzle.a) == false))
    {
        result = Result::ErrorInvalidFormatSwizzle;
    }
    // Verify the base mip level is valid for the given image object.
    // Make sure the base mip level requested in the view isn't for more mip levels
    // than the image we're viewing actually has.
    if (info.subresRange.startSubres.mipLevel >= imgInfo.mipLevels)
    {
        result = Result::ErrorInvalidBaseMipLevel;
    }

    // Verify the view format is compatible with the image format
    if (result == Result::Success)
    {
        result = ValidateCompatibleImageViewFormats(*pImage, viewPlane, viewFmt.format);
    }

    // Check slice array and image view type
    if (result == Result::Success)
    {
        const uint32 imgSamples      = imgInfo.samples;
        const uint32 imgArraySize    = imgInfo.arraySize;
        const uint32 viewBaseSlice   = info.subresRange.startSubres.arraySlice;
        const uint32 viewArraySize   = info.subresRange.numSlices;
        const uint32 viewMaxSlice    = (viewArraySize + viewBaseSlice);
        const ImageViewType viewType = info.viewType;

        // Views must have at least one array slice, for all types.
        if (viewArraySize == 0)
        {
            result = Result::ErrorInvalidViewArraySize;
        }
        // Verify that the view type is compatible with the image type and that the number of
        // viewable slices doesn't go past the number of existing slices.
        else
        {
            switch (imgInfo.imageType)
            {
            case ImageType::Tex1d:
                // 1D image -- view must be 1D and (baseArraySlice + arraySlices) must be within the
                // image's arraySize bounds.
                if (viewType != ImageViewType::Tex1d)
                {
                    result = Result::ErrorViewTypeIncompatibleWithImageType;
                }
                else if (viewMaxSlice > imgArraySize)
                {
                    result = Result::ErrorInsufficientImageArraySize;
                }
                break;
            case ImageType::Tex2d:
                // 2D image can have 2D views, cubemap views
                // 2D views -- (baseArraySlice + arraySlices) must be within the image's arraySize
                // bounds.
                if (viewType == ImageViewType::Tex2d)
                {
                    if (viewMaxSlice > imgArraySize)
                    {
                        result = Result::ErrorInsufficientImageArraySize;
                    }
                }
                // Cubemap views -- image must be single-sampled, 6 * (baseArraySlice + arraySlices)
                // must be within the image's arraySize bounds, and height must match the width.
                else if (viewType == ImageViewType::TexCube)
                {
                    if (imgSamples > 1)
                    {
                        result = Result::ErrorCubemapIncompatibleWithMsaa;
                    }
                    else if (viewMaxSlice > imgArraySize)
                    {
                        result = Result::ErrorInsufficientImageArraySize;
                    }
                    else if (imgInfo.extent.width != imgInfo.extent.height)
                    {
                        result = Result::ErrorCubemapNonSquareFaceSize;
                    }
                }
                // 1D and 3D views are illegal
                else
                {
                    result = Result::ErrorViewTypeIncompatibleWithImageType;
                }
                break;
            case ImageType::Tex3d:
                // 3D image -- view must be 3D and (baseArraySlice + arraySlices) must be 1 if not
                // using 2D image view of a 3D image
                if ((viewType == ImageViewType::Tex2d) && imgInfo.flags.view3dAs2dArray)
                {
                    if (viewMaxSlice > imgInfo.extent.depth)
                    {
                        result = Result::ErrorInvalidViewArraySize;
                    }
                }
                else if (viewType != ImageViewType::Tex3d)
                {
                    result = Result::ErrorViewTypeIncompatibleWithImageType;
                }
                else if (viewArraySize != 1)
                {
                    result = Result::ErrorInvalidViewArraySize;
                }
                else if (viewBaseSlice != 0)
                {
                    result = Result::ErrorInvalidViewBaseSlice;
                }
                break;
            case ImageType::Count:
                PAL_ASSERT_ALWAYS();
                break;
            }
        }
    }

    if (result == Result::Success)
    {
        result = m_pGfxDevice->HwlValidateImageViewInfo(info);
    }

    return result;
}

// =====================================================================================================================
// Error checks FmaskViewInfo parameters for an fmask view SRD.
Result Device::ValidateFmaskViewInfo(
    const FmaskViewInfo& info
    ) const
{
    Result result = Result::Success;

    const Image* pImage            = static_cast<const Image*>(info.pImage);
    const ImageCreateInfo& imgInfo = pImage->GetImageCreateInfo();

    // Check fmask availability for fmask view
    if (pImage->GetGfxImage()->HasFmaskData() == false)
    {
        result = Result::ErrorImageFmaskUnavailable;
    }

    // Check slice array
    if (result == Result::Success)
    {
        // Views must have at least one array slice, for all types.
        if (info.arraySize == 0)
        {
            result = Result::ErrorInvalidViewArraySize;
        }
        // Verify that the number of viewable slices doesn't go past the number of existing slices.
        else
        {
            switch (imgInfo.imageType)
            {
            case ImageType::Tex1d:
            case ImageType::Tex3d:
                // 1D/3D image -- not compatible with fmask views
                result = Result::ErrorViewTypeIncompatibleWithImageType;
                break;

            case ImageType::Tex2d:
                // 2D views -- (baseArraySlice + arraySlices) must be within the image's arraySize
                // bounds.
                if (info.baseArraySlice + info.arraySize > imgInfo.arraySize)
                {
                    result = Result::ErrorInsufficientImageArraySize;
                }
                break;

            case ImageType::Count:
                PAL_ASSERT_ALWAYS();
                break;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Error checks SamplerInfo paramters for a sampler SRD.
Result Device::ValidateSamplerInfo(
    const SamplerInfo& samplerInfo
    ) const
{
    Result result = Result::Success;

    // The legal range for minLod and maxLod is [0...16] inclusive.
    if (((samplerInfo.minLod < 0.f) || (samplerInfo.minLod > 16.f)) ||
        ((samplerInfo.maxLod < 0.f) || (samplerInfo.maxLod > 16.f)))
    {
        result = Result::ErrorInvalidValue;
    }
    // Max LOD value should be greater or equal to min LOD
    else if (samplerInfo.maxLod < samplerInfo.minLod)
    {
        result = Result::ErrorInvalidValue;
    }
    // The legal range for mipLodBias is [-16...16] inclusive.
    else if ((samplerInfo.mipLodBias < -16.f) || (samplerInfo.mipLodBias > 16.f))
    {
        result = Result::ErrorInvalidValue;
    }
    // The legal range for maxAnisotropy is [1...16] inclusive.
    else if ((samplerInfo.maxAnisotropy < 1) || (samplerInfo.maxAnisotropy > 16))
    {
        result = Result::ErrorInvalidValue;
    }

    if (result == Result::Success)
    {
        result = m_pGfxDevice->HwlValidateSamplerInfo(samplerInfo);
    }

    return result;
}

// =====================================================================================================================
Platform* Device::GetPlatform() const
{
    return m_pPlatform;
}

// =====================================================================================================================
// Fills out the pal settings structure
const PalSettings& Device::Settings() const
{
    PAL_ASSERT(m_pSettingsLoader != nullptr);
    return m_pSettingsLoader->GetSettings();
}

// =====================================================================================================================
// Gets a modifiable pointer to the public settings.
PalPublicSettings* Device::GetPublicSettings()
{
    return &m_publicSettings;
}

// =====================================================================================================================
// The settings hash is used during pipeline loading to verify that the pipeline data is compatible between when it was
// stored and when it was loaded.
Util::MetroHash::Hash Device::GetSettingsHash() const
{
    PAL_ASSERT(m_pSettingsLoader != nullptr);

    // We just combine the core and Hwl hashes by XOR'ing each DWORD
    auto returnHash = m_pSettingsLoader->GetSettingsHash();
    if (m_pGfxDevice != nullptr)
    {
        auto hwlHash = m_pGfxDevice->GetSettingsHash();
        for (uint8 i=0; i<4; i++)
        {
            returnHash.dwords[i] ^= hwlHash.dwords[i];
        }
    }
    return returnHash;
}

// =====================================================================================================================
// Read settings. Part of the public IDevice interface.
bool Device::ReadSetting(
    const char*     pSettingName,
    SettingScope    settingScope,
    Util::ValueType valueType,
    void*           pValue,
    size_t          bufferSz
    ) const
{
    const InternalSettingScope internalScope =
        (settingScope == SettingScope::Driver) ? PrivateDriverKey : PublicCatalystKey;

    return ReadSetting(pSettingName, valueType, pValue, internalScope, bufferSz);
}

// =====================================================================================================================
// Reads a setting from a configuration file on Linux builds, and does nothing for other OS platforms.  It is up to them
// to implement another mechanism for reading settings (such as reading Windows registry keys, etc.).
bool Device::ReadSetting(
    const char*          pSettingName,
    Util::ValueType      valueType,
    void*                pValue,
    InternalSettingScope settingType,
    size_t               bufferSz
    ) const
{
#if defined(__unix__)
    return m_settingsMgr.GetValue(pSettingName, valueType, pValue, bufferSz);
#else
    return false;
#endif
}

// =====================================================================================================================
// Gets currently connected private screens.
Result Device::GetPrivateScreens(
    uint32*          pNumScreens,
    IPrivateScreen** ppScreens)
{
    PAL_ASSERT(pNumScreens != nullptr);

    uint32 newNumScreens = 0;
    Result result = EnumPrivateScreensInfo(&newNumScreens);

    if (result == Result::Success)
    {
        if (m_connectedPrivateScreens + m_emulatedPrivateScreens > MaxPrivateScreens)
        {
            result = Result::ErrorTooManyPrivateScreens;
        }
    }

    if (result == Result::Success)
    {
        // The slot of true indicates the currently enumerated private screen already exists in previously enumerated
        // list and should be skipped. This is to index in m_privateScreenInfo[] and save another loop in old screens to
        // find new screens that are needed to be created.
        bool skipped[MaxPrivateScreens] = {};

        // There are some previously enumerated private screens.
        if (m_connectedPrivateScreens > 0)
        {
            // Find those unchanged private screens.
            for (uint32 i = 0; i < MaxPrivateScreens; i++)
            {
                if (m_pPrivateScreens[i] != nullptr)
                {
                    bool removed = true;
                    for (uint32 n = 0; n < newNumScreens; n++)
                    {
                        if (m_pPrivateScreens[i]->Hash() == m_privateScreenInfo[n].props.hash)
                        {
                            removed    = false;
                            skipped[n] = true;
                            break;
                        }
                    }
                    if (removed)
                    {
                        m_pPrivateScreens[i]->~PrivateScreen();
                        PAL_SAFE_FREE(m_pPrivateScreens[i], GetPlatform());
                    }
                }
            }
        }

        uint32 slot = 0;
        for (uint32 n = 0; n < newNumScreens; n++)
        {
            if (skipped[n] == false)
            {
                // Find an available slot, note removed ones have been set to nullptr above.
                while (m_pPrivateScreens[slot] != nullptr)
                {
                    slot++;
                }
                PAL_ASSERT(slot < MaxPrivateScreens);

                // KMD does not report some of the formats, using fixed value MaxPrivateScreens instead.
                size_t memSize = sizeof(PrivateScreen) +
                                 MaxPrivateScreenFormats * sizeof(SwizzledFormat);
                PrivateScreen* pScreen =
                    static_cast<PrivateScreen*>(PAL_MALLOC(memSize, GetPlatform(), AllocInternal));

                if (pScreen == nullptr)
                {
                    result = Result::ErrorOutOfMemory;
                    break;
                }
                // This is only to tell PrivateScreen object the pointer to format list portion of pre-allocated memory.
                m_privateScreenInfo[n].props.pFormats = reinterpret_cast<SwizzledFormat*>(pScreen + 1);
                m_pPrivateScreens[slot] = PAL_PLACEMENT_NEW(pScreen) PrivateScreen(this, m_privateScreenInfo[n]);
                PAL_ASSERT(m_pPrivateScreens[slot]  != nullptr);

                result = m_pPrivateScreens[slot]->InitPhysical();
                if (result != Result::Success)
                {
                    m_pPrivateScreens[slot]->~PrivateScreen();
                    PAL_SAFE_FREE(m_pPrivateScreens[slot], GetPlatform());
                    break;
                }
                // This slot is occupied, move to next available one....
                slot++;
            }
        }

        if (result == Result::Success)
        {
            m_connectedPrivateScreens = newNumScreens;
            PAL_ASSERT(slot <= newNumScreens);
        }
        else
        {
            m_connectedPrivateScreens = slot;
            newNumScreens             = slot;
        }
    }

    if (result == Result::Success)
    {
        if (ppScreens != nullptr)
        {
            if (m_connectedPrivateScreens > 0)
            {
                // The output doesn't try to purge the empty slots.
                for (uint32 ordinal = 0; ordinal < MaxPrivateScreens; ordinal++)
                {
                    ppScreens[ordinal] = m_pPrivateScreens[ordinal];
                }
            }

            if (m_emulatedPrivateScreens > 0)
            {
                for (uint32 i = 0; i < MaxPrivateScreens; i++)
                {
                    // Place emulated private screeens at the end of the array, not to break orders of physical ones.
                    uint32 ordinal = MaxPrivateScreens - 1;
                    if (m_pEmulatedPrivateScreens[i] != nullptr)
                    {
                        // In theory, there should be some empty slot if we ensure the total number of real and emulated
                        // private screens doesn't exceed MaxPrivateScreens.
                        while (ppScreens[ordinal] != nullptr)
                        {
                            ordinal--;
                        }

                        ppScreens[ordinal] = m_pEmulatedPrivateScreens[i];
                    }
                }
                newNumScreens += m_emulatedPrivateScreens;
            }
        }
    }

    *pNumScreens = newNumScreens;

    return result;
}

// =====================================================================================================================
void Device::GetPrivateScreenImageSizes(
    const PrivateScreenImageCreateInfo& createInfo,
    size_t*                             pImageSize,
    size_t*                             pGpuMemorySize,
    Result*                             pResult
    ) const
{
    if (pResult != nullptr)
    {
        *pResult = Image::ValidatePrivateCreateInfo(this, createInfo);
    }
#if PAL_ENABLE_PRINTS_ASSERTS
    else
    {
        PAL_ASSERT(Image::ValidatePrivateCreateInfo(this, createInfo) == Result::Success);
    }
#endif

    ImageCreateInfo imgInfo = {};
    ConvertPrivateScreenImageCreateInfo(createInfo, &imgInfo);

    *pImageSize     = GetImageSize(imgInfo, pResult);
    *pGpuMemorySize = GpuMemoryObjectSize();
}

// =====================================================================================================================
Result Device::CreatePrivateScreenImage(
    const PrivateScreenImageCreateInfo& createInfo,
    void*                               pImagePlacementAddr,
    void*                               pGpuMemoryPlacementAddr,
    IImage**                            ppImage,
    IGpuMemory**                        ppGpuMemory)
{
    PAL_ASSERT ((pImagePlacementAddr != nullptr) && (pGpuMemoryPlacementAddr != nullptr) &&
                (ppImage != nullptr) && (ppGpuMemory != nullptr));

    return Image::CreatePrivateScreenImage(this,
                                           createInfo,
                                           pImagePlacementAddr,
                                           pGpuMemoryPlacementAddr,
                                           ppImage,
                                           ppGpuMemory);
}

// =====================================================================================================================
Result Device::AddGpuMemoryReferences(
    uint32              gpuMemRefCount,
    const GpuMemoryRef* pGpuMemoryRefs,
    IQueue*             pQueue,
    uint32              flags
    )
{
    m_pPlatform->GetGpuMemoryEventProvider()->LogGpuMemoryAddReferencesEvent(gpuMemRefCount, pGpuMemoryRefs, pQueue, flags);
    return AddToReferencedMemoryTotals(gpuMemRefCount, pGpuMemoryRefs);
}

// =====================================================================================================================
Result Device::RemoveGpuMemoryReferences(
    uint32            gpuMemoryCount,
    IGpuMemory*const* ppGpuMemory,
    IQueue*           pQueue
    )
{
    m_pPlatform->GetGpuMemoryEventProvider()->LogGpuMemoryRemoveReferencesEvent(gpuMemoryCount, ppGpuMemory, pQueue);
    return SubtractFromReferencedMemoryTotals(gpuMemoryCount, ppGpuMemory, false);
}

// =====================================================================================================================
// For each GPU memory object:
// - Increment its refcount
// - If it wasn't referenced before, update its preferred heap's total size.
Result Device::AddToReferencedMemoryTotals(
    uint32              gpuMemRefCount,
    const GpuMemoryRef* pGpuMemoryRefs)
{
    Result result = Result::Success;

    MutexAuto lock(&m_referencedGpuMemLock);

    for (uint32 i = 0; i < gpuMemRefCount; i++)
    {
        uint32* pValue        = nullptr;
        bool    alreadyExists = false;

        result = m_referencedGpuMem.FindAllocate(pGpuMemoryRefs[i].pGpuMemory, &alreadyExists, &pValue);
        if (result != Result::Success)
        {
            // Not enough room or some other error, so just abort
            PAL_ASSERT_ALWAYS();
            break;
        }
        else
        {
            if (alreadyExists == false)
            {
                *pValue = 1;

                const GpuMemoryDesc& desc = pGpuMemoryRefs[i].pGpuMemory->Desc();

                // Making a virtual GPU memory allocation resident doesn't make any additional physical pages resident,
                // so we shouldn't count them here.
                if (desc.flags.isVirtual == false)
                {
                    PAL_ASSERT(desc.heapCount > 0);
                    m_referencedGpuMemBytes[desc.heaps[0]] += desc.size;
                }
            }
            else
            {
                ++(*pValue);
            }
        }
    }

    return result;
}

// =====================================================================================================================
// For each GPU memory object:
// - Decrement its refcount
// - If all references are released, update its preferred heap's total size.
// forceSubtract forces this function to release all references and update the heap size. We expect this to be used
// when GPU memory objects are destroyed.
Result Device::SubtractFromReferencedMemoryTotals(
    uint32            gpuMemoryCount,
    IGpuMemory*const* ppGpuMemory,
    bool              forceSubtract)
{
    MutexAuto lock(&m_referencedGpuMemLock);

    for (uint32 i = 0; i < gpuMemoryCount; i++)
    {
        uint32*const pValue  = m_referencedGpuMem.FindKey(ppGpuMemory[i]);
        if (pValue != nullptr)
        {
            PAL_ASSERT(*pValue > 0);
            if ((--(*pValue) == 0) || forceSubtract)
            {
                m_referencedGpuMem.Erase(ppGpuMemory[i]);

                const GpuMemoryDesc& desc = ppGpuMemory[i]->Desc();

                // Making a virtual GPU memory allocation resident doesn't make any additional physical pages resident,
                // so we shouldn't count them here.
                if (desc.flags.isVirtual == false)
                {
                    PAL_ASSERT(desc.heapCount > 0);
                    PAL_ASSERT(m_referencedGpuMemBytes[desc.heaps[0]] >= desc.size);
                    m_referencedGpuMemBytes[desc.heaps[0]] -= desc.size;
                }
            }
        }
    }

    return Result::Success;
}

// =====================================================================================================================
IfhMode Device::GetIfhMode() const
{
    return Settings().ifh;
}

// =====================================================================================================================
void Device::GetReferencedMemoryTotals(
    gpusize  referencedGpuMemTotal[GpuHeapCount]
    ) const
{
    for (uint32 i = 0; i < GpuHeapCount; ++i)
    {
        referencedGpuMemTotal[i] = m_referencedGpuMemBytes[i];
    }
}

// =====================================================================================================================
// On a queue's creation, we need to add it to the list of tracked queues for this device.
Result Device::AddQueue(
    Queue* pQueue)
{
    PAL_ASSERT(pQueue != nullptr);

    // Queue-list operations need to be protected.
    MutexAuto lock(&m_queueLock);
    m_queues.PushFront(pQueue->DeviceMembershipNode());

    return Result::Success;
}

// =====================================================================================================================
// On a queue's destruction, we remove it from the list of tracked queues for this device.
void Device::RemoveQueue(
    Queue* pQueue)
{
    PAL_ASSERT(pQueue != nullptr);

    // Queue-list operations need to be protected.
    MutexAuto lock(&m_queueLock);
    m_queues.Erase(pQueue->DeviceMembershipNode());
}

// =====================================================================================================================
// Determines the start (inclusive) and end (exclusive) virtual addresses for the specified virtual address range.
void Device::VirtualAddressRange(
    VaPartition vaPartition,
    gpusize*    pStartVirtAddr,
    gpusize*    pEndVirtAddr
    ) const
{
    PAL_ASSERT((pStartVirtAddr != nullptr) && (pEndVirtAddr != nullptr));

    const auto& addrRange = m_memoryProperties.vaRange[static_cast<uint32>(vaPartition)];

    (*pStartVirtAddr) = addrRange.baseVirtAddr;
    (*pEndVirtAddr)   = (addrRange.baseVirtAddr + addrRange.size);
}

// =====================================================================================================================
// Chooses a VA partition based on the given VaRange enum.
VaPartition Device::ChooseVaPartition(
    VaRange range,
    bool    isVirtual
    ) const
{
    constexpr VaPartition LookupTable[] =
    {
        VaPartition::Default,                   // VaRange::Default
        VaPartition::DescriptorTable,           // VaRange::DescriptorTable
        VaPartition::ShadowDescriptorTable,     // VaRange::ShadowDescriptorTable
        VaPartition::Svm,                       // VaRange::Svm
        VaPartition::CaptureReplay,             // VaRange::CaptureReplay
    };

    // Use the VA partition associated with the VA range, unless the Device does not support multiple VA ranges. In
    // that case, just use the default range.
    VaPartition partition = VaPartition::Default;

    if (m_memoryProperties.flags.multipleVaRangeSupport != 0)
    {
        // If this is a virtual only allocation, a separate partition may be required.
        if (isVirtual && (m_memoryProperties.vaStartPrt > 0))
        {
            PAL_ASSERT(range == VaRange::Default);

            partition = VaPartition::Prt;
        }
        else
        {
            partition = LookupTable[static_cast<uint32>(range)];
        }
    }

    return partition;
}

// =====================================================================================================================
// Increment frame count and move to next frame
void Device::IncFrameCount()
{
#if PAL_ENABLE_PRINTS_ASSERTS
    // Force command buffer dumping on for the next frame if the user is currently holding Shift-F10.
    m_cmdBufDumpEnabledViaHotkey = IsKeyPressed(KeyCode::Shift_F10);
#endif
    Util::AtomicIncrement(&m_frameCnt);
}

// =====================================================================================================================
bool Device::UsingHdrColorspaceFormat(
    ) const
{
    constexpr uint32 HdrMask = TfPq2084      |
                               CsBt2020      |
                               CsDolbyVision |
                               CsAdobe       |
                               CsDciP3       |
                               CsScrgb       |
                               CsUserDefined |
                               CsNative;

    return ((m_hdrColorspaceFormat & HdrMask) != 0);
}

// =====================================================================================================================
// Applies the developer overlay to the destination image by writing commands into the provided command buffer
void Device::ApplyDevOverlay(
    const IImage& dstImage,
    ICmdBuffer*   pCmdBuffer
    ) const
{
    PAL_ASSERT(m_pPlatform->IsDeveloperModeEnabled());
    PAL_ASSERT(pCmdBuffer != nullptr);

    // Get the developer mode driver server
    DevDriver::DevDriverServer* pDevDriverServer = m_pPlatform->GetDevDriverServer();
    // This pointer should never be null if developer mode is enabled
    PAL_ASSERT(pDevDriverServer != nullptr);

    // Increment after every write
    uint32 letterHeight = 0;
    // Write the Developer Mode text on screen
    constexpr const char* DeveloperModeString = "Radeon Developer Mode";
    m_pTextWriter->DrawDebugText(dstImage,
                                 pCmdBuffer,
                                 DeveloperModeString,
                                 0,
                                 letterHeight);
    letterHeight += GpuUtil::TextWriterFont::LetterHeight;

    constexpr uint32 OverlayTextBufferSize = 256;
    char overlayTextBuffer[OverlayTextBufferSize] = {};

    if (pDevDriverServer->IsConnected())
    {
        // Get the RGPServer object
        DevDriver::RGPProtocol::RGPServer* pRgpServer = pDevDriverServer->GetRGPServer();
        // This pointer should always be valid if developer mode is enabled.
        PAL_ASSERT(pRgpServer != nullptr);

        // Check the profiling status
        const char* pTraceStatusString = "Disabled";
        if (pRgpServer->TracesEnabled())
        {
            pTraceStatusString = pRgpServer->IsTracePending() ? "Pending" : "Ready";
        }

        // Print the profiling status string
        Util::Snprintf(overlayTextBuffer,
                       OverlayTextBufferSize, "RGP Profiling: %s",
                       pTraceStatusString);
        m_pTextWriter->DrawDebugText(dstImage,
                                     pCmdBuffer,
                                     overlayTextBuffer,
                                     0,
                                     letterHeight);
        letterHeight += GpuUtil::TextWriterFont::LetterHeight;

        // Check the RMV trace status
        const char* pRmvTraceStatusString = m_pPlatform->GetGpuMemoryEventProvider()->IsMemoryProfilingEnabled() ?
            "Active": "Inactive";

        // Print the RMV trace status string
        Util::Snprintf(overlayTextBuffer,
                       OverlayTextBufferSize, "RMV Tracing: %s",
                       pRmvTraceStatusString);
        m_pTextWriter->DrawDebugText(dstImage,
                                     pCmdBuffer,
                                     overlayTextBuffer,
                                     0,
                                     letterHeight);
        letterHeight += GpuUtil::TextWriterFont::LetterHeight;

        // Check Crash Analysis Status
        const char* pRgdTraceStatusString = m_pPlatform->IsCrashAnalysisModeEnabled() ? "Active" : "Inactive";

        //Print Crash Analysis Status
        Util::Snprintf(overlayTextBuffer,
                       OverlayTextBufferSize, "Crash Analysis: %s",
                       pRgdTraceStatusString);
        m_pTextWriter->DrawDebugText(dstImage,
                                     pCmdBuffer,
                                     overlayTextBuffer,
                                     0,
                                     letterHeight);
        letterHeight += GpuUtil::TextWriterFont::LetterHeight;

        // Write the device clock mode

        // These labels differ from the DeviceClockMode enum name so as to match the names used by RDP.
        constexpr const char* pClockModeTable[] = {
            "Unknown",          // Corresponds with DeviceClockMode::Unknown
            "Normal",           // Corresponds with DeviceClockMode::Default
            "Stable",           // Corresponds with DeviceClockMode::Profiling
            "Minimum Memory",   // Corresponds with DeviceClockMode::MinimumMemory
            "Minimum Engine",   // Corresponds with DeviceClockMode::MinimumEngine
            "Peak"              // Corresponds with DeviceClockMode::Peak
        };

        static_assert(static_cast<uint32>(DevDriver::DriverControlProtocol::DeviceClockMode::Unknown)       == 0,
                      "Unexpected DeviceClockMode::Unknown");
        static_assert(static_cast<uint32>(DevDriver::DriverControlProtocol::DeviceClockMode::Default)       == 1,
                      "Unexpected DeviceClockMode::Default");
        static_assert(static_cast<uint32>(DevDriver::DriverControlProtocol::DeviceClockMode::Profiling)     == 2,
                      "Unexpected DeviceClockMode::Profiling");
        static_assert(static_cast<uint32>(DevDriver::DriverControlProtocol::DeviceClockMode::MinimumMemory) == 3,
                      "Unexpected DeviceClockMode::MinimumMemory");
        static_assert(static_cast<uint32>(DevDriver::DriverControlProtocol::DeviceClockMode::MinimumEngine) == 4,
                      "Unexpected DeviceClockMode::MinimumEngine");
        static_assert(static_cast<uint32>(DevDriver::DriverControlProtocol::DeviceClockMode::Peak)          == 5,
                      "Unexpected DeviceClockMode::Peak");
        static_assert(static_cast<uint32>(DevDriver::DriverControlProtocol::DeviceClockMode::Count)         == 6,
                      "Unexpected DeviceClockMode::Count");

        // Get the DriverControlServer object
        DevDriver::DriverControlProtocol::DriverControlServer* pDriverControlServer =
            pDevDriverServer->GetDriverControlServer();

        // This pointer should always be valid if developer mode is enabled.
        PAL_ASSERT(pDriverControlServer != nullptr);

        // Get the device clock mode
        const DevDriver::DriverControlProtocol::DeviceClockMode clockMode =
            pDriverControlServer->GetDeviceClockMode(GetDeviceIndex());
        PAL_ASSERT(clockMode < DevDriver::DriverControlProtocol::DeviceClockMode::Count);

        // Print the clock mode on screen
        Util::Snprintf(overlayTextBuffer,
                       OverlayTextBufferSize,
                       "Clock Mode: %s",
                       pClockModeTable[static_cast<uint32>(clockMode)]);
        m_pTextWriter->DrawDebugText(dstImage,
                                     pCmdBuffer,
                                     overlayTextBuffer,
                                     0,
                                     letterHeight);
        letterHeight += GpuUtil::TextWriterFont::LetterHeight;

        // Print the client string and Client Id on screen
        Util::Snprintf(overlayTextBuffer,
            OverlayTextBufferSize,
            "Client: %s",
            m_pPlatform->GetClientApiStr());
        m_pTextWriter->DrawDebugText(dstImage,
            pCmdBuffer,
            overlayTextBuffer,
            0,
            letterHeight);
        letterHeight += GpuUtil::TextWriterFont::LetterHeight;

        Util::Snprintf(overlayTextBuffer,
                       OverlayTextBufferSize,
                       "Client Id: %d",
                       m_devDriverClientId);
        m_pTextWriter->DrawDebugText(dstImage,
                                     pCmdBuffer,
                                     overlayTextBuffer,
                                     0,
                                     letterHeight);
        letterHeight += GpuUtil::TextWriterFont::LetterHeight;
    }
    else // !IsConnected()
    {
        // Print status
        m_pTextWriter->DrawDebugText(dstImage,
                                     pCmdBuffer,
                                     "Disconnected",
                                     0,
                                     letterHeight);
        letterHeight += GpuUtil::TextWriterFont::LetterHeight;
    }

    // If the setting is enabled, display a visual confirmation of HDR Mode
    if (Settings().overlayReportHDR)
    {
        Util::Snprintf(overlayTextBuffer,
                       OverlayTextBufferSize,
                       "HDR: %s - Colorspace Format: %u",
                       UsingHdrColorspaceFormat() ? "Enabled" : "Disabled",
                       m_hdrColorspaceFormat);

        m_pTextWriter->DrawDebugText(dstImage,
                                     pCmdBuffer,
                                     overlayTextBuffer,
                                     0,
                                     letterHeight);
        letterHeight += GpuUtil::TextWriterFont::LetterHeight;
    }

    // If the setting is enabled, display a visual confirmation of MES HWS Mode (only for supported HW)
    if (Settings().overlayReportMes && (ChipProperties().gfxLevel >= GfxIpLevel::GfxIp10_1))
    {
        Util::Snprintf(overlayTextBuffer,
                       OverlayTextBufferSize,
                       "MES HWS: %s",
                       (GetHwsInfo().gfxHwsEnabled     ||
                        GetHwsInfo().computeHwsEnabled ||
                        GetHwsInfo().dmaHwsEnabled) ? "Enabled" : "Disabled");

        m_pTextWriter->DrawDebugText(dstImage,
                                     pCmdBuffer,
                                     overlayTextBuffer,
                                     0,
                                     letterHeight);
        letterHeight += GpuUtil::TextWriterFont::LetterHeight;
    }

    // Issue a barrier to ensure the text written via CS is complete and flushed out of L2.
    BarrierInfo barrier = {};
    barrier.waitPoint   = HwPipePreCs;

    const HwPipePoint postCs   = HwPipePostCs;
    barrier.pipePointWaitCount = 1;
    barrier.pPipePoints        = &postCs;

    BarrierTransition transition = {};
    transition.srcCacheMask      = CoherShader;
    transition.dstCacheMask      = CoherShader;

    barrier.transitionCount = 1;
    barrier.pTransitions    = &transition;

    barrier.reason          = Developer::BarrierReasonDevDriverOverlay;

    pCmdBuffer->CmdBarrier(barrier);
}

// =====================================================================================================================
bool Device::EngineSupportsCompute(
    EngineType  engineType)
{
    const bool supportsCompute = ((engineType == EngineTypeCompute)   ||
                                  (engineType == EngineTypeUniversal)
                                  );

    return supportsCompute;
}

// =====================================================================================================================
bool Device::EngineSupportsGraphics(
    EngineType  engineType)
{
    const bool supportsGfx = (engineType == EngineTypeUniversal);

    return supportsGfx;
}

// =====================================================================================================================
// P2P WA can be required from either GFX or OSSIP, but we wanted to put the bulk of the implementation in some hardware
// layer.  Forward this call to the GFX HWL regardless of the caller IP.
Result Device::P2pBltWaModifyRegionListMemory(
    const IGpuMemory&       dstGpuMemory,
    uint32                  regionCount,
    const MemoryCopyRegion* pRegions,
    uint32*                 pNewRegionCount,
    MemoryCopyRegion*       pNewRegions,
    gpusize*                pChunkAddrs
    ) const
{
    Result result = Result::Success;

    if (m_pGfxDevice != nullptr)
    {
        result = m_pGfxDevice->P2pBltWaModifyRegionListMemory(dstGpuMemory,
                                                              regionCount,
                                                              pRegions,
                                                              pNewRegionCount,
                                                              pNewRegions,
                                                              pChunkAddrs);
    }

    return result;
}

// =====================================================================================================================
// P2P WA can be required from either GFX or OSSIP, but we wanted to put the bulk of the implementation in some hardware
// layer.  Forward this call to the GFX HWL regardless of the caller IP.
Result Device::P2pBltWaModifyRegionListImage(
    const Pal::Image&      srcImage,
    const Pal::Image&      dstImage,
    uint32                 regionCount,
    const ImageCopyRegion* pRegions,
    uint32*                pNewRegionCount,
    ImageCopyRegion*       pNewRegions,
    gpusize*               pChunkAddrs
    ) const
{
    Result result = Result::Success;

    if (m_pGfxDevice != nullptr)
    {
        result = m_pGfxDevice->P2pBltWaModifyRegionListImage(srcImage,
                                                             dstImage,
                                                             regionCount,
                                                             pRegions,
                                                             pNewRegionCount,
                                                             pNewRegions,
                                                             pChunkAddrs);
    }

    return result;
}

// =====================================================================================================================
// P2P WA can be required from either GFX or OSSIP, but we wanted to put the bulk of the implementation in some hardware
// layer.  Forward this call to the GFX HWL regardless of the caller IP.
Result Device::P2pBltWaModifyRegionListImageToMemory(
    const Pal::Image&            srcImage,
    const IGpuMemory&            dstGpuMemory,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions,
    uint32*                      pNewRegionCount,
    MemoryImageCopyRegion*       pNewRegions,
    gpusize*                     pChunkAddrs
    ) const
{
    Result result = Result::Success;

    // Implementation
    if (m_pGfxDevice != nullptr)
    {
        result = m_pGfxDevice->P2pBltWaModifyRegionListImageToMemory(srcImage,
                                                                     dstGpuMemory,
                                                                     regionCount,
                                                                     pRegions,
                                                                     pNewRegionCount,
                                                                     pNewRegions,
                                                                     pChunkAddrs);
    }

    return result;
}

// =====================================================================================================================
// P2P WA can be required from either GFX or OSSIP, but we wanted to put the bulk of the implementation in some hardware
// layer.  Forward this call to the GFX HWL regardless of the caller IP.
Result Device::P2pBltWaModifyRegionListMemoryToImage(
    const IGpuMemory&            srcGpuMemory,
    const Pal::Image&            dstImage,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions,
    uint32*                      pNewRegionCount,
    MemoryImageCopyRegion*       pNewRegions,
    gpusize*                     pChunkAddrs
    ) const
{
    Result result = Result::Success;

    if (m_pGfxDevice != nullptr)
    {
        result = m_pGfxDevice->P2pBltWaModifyRegionListMemoryToImage(srcGpuMemory,
                                                                     dstImage,
                                                                     regionCount,
                                                                     pRegions,
                                                                     pNewRegionCount,
                                                                     pNewRegions,
                                                                     pChunkAddrs);
    }

    return result;
}

// =====================================================================================================================
// Returns true if the specified heap is valid for pipelines on this device
bool Device::ValidatePipelineUploadHeap(
    const GpuHeap& preferredHeap
    ) const
{
    // Never prefer the heap which doesn't exit.
    bool valid = HeapLogicalSize(preferredHeap);

    if (preferredHeap == GpuHeap::GpuHeapInvisible)
    {
        // Disable pipeline upload to local invisible memory if clients have chosen to disable internal residency
        // optimizations or there is no DMA engine support. Other heap types don't have any restrictions.
        if (m_pPlatform->InternalResidencyOptsDisabled()                    ||
            (EngineProperties().perEngine[EngineTypeDma].numAvailable == 0) ||
            (m_publicSettings.pipelinePreferredHeap != GpuHeap::GpuHeapInvisible)
           )
        {
            valid = false;
        }
    }

    return valid;
}

// =====================================================================================================================
bool Device::IssueSqttMarkerEvents() const
{
    const PalPlatformSettings& platformSettings = m_pPlatform->PlatformSettings();

    // PAL only expects SQTT to be enabled by the GPU profiler or by dev driver. If it is enabled we should tell our
    // command buffers to emit SQTT marker events.
    const bool sqttEnabled = (platformSettings.gpuProfilerMode > GpuProfilerCounterAndTimingOnly) &&
                             TestAnyFlagSet(platformSettings.gpuProfilerConfig.traceModeMask, GpuProfilerTraceSqtt);

    return sqttEnabled || m_pPlatform->IsDevDriverProfilingEnabled() || GetPublicSettings()->enableSqttMarkerEvent;
}

// =====================================================================================================================
bool Device::EnablePerfCountersInPreamble() const
{
    const PalPlatformSettings& platformSettings = m_pPlatform->PlatformSettings();
    bool enable = true;

    switch (Settings().startingPerfcounterState)
    {
        case StartingPerfcounterStateDisabled:
            enable = false;
            break;
        case StartingPerfcounterStateEnabled:
            enable = true;
            break;
        case StartingPerfcounterStateAuto:
            enable = (platformSettings.gpuProfilerMode > GpuProfilerDisabled) ||
                     m_pPlatform->IsDevDriverProfilingEnabled() ||
                     GetPublicSettings()->enableSqttMarkerEvent; // we assume if client wants SQTT they want counters too
            break;
        case StartingPerfcounterStateUntouched:
        default:
            PAL_NEVER_CALLED();
    }

    return enable;
}

// =====================================================================================================================
// Writes an ELF code object used by a pipeline or library to disk.
void Device::LogCodeObjectToDisk(
    StringView<char> prefix,
    StringView<char> name,      // Optional: Can be the empty string if a human-readable filename is not desired.
    PipelineHash     hash,
    bool             isInternal,
    const void*      pCodeObject,
    size_t           codeObjectLen
    ) const
{
    const PalSettings& settings = Settings();

    const bool dumpEnabled = settings.logPipelineElf &&
                             (isInternal ? settings.pipelineElfLogConfig.logInternal
                                         : settings.pipelineElfLogConfig.logExternal);

    const uint64 hashToDump  = settings.pipelineElfLogConfig.logHash;
    const bool   hashMatches = ((hashToDump == 0) || (hash.stable == hashToDump));

    if (dumpEnabled && hashMatches)
    {
        const char*const pLogDir = &settings.pipelineElfLogConfig.logDirectory[0];

        // Create the directory. We don't care if it fails (existing is fine, failure is caught when opening the file).
        MkDirRecursively(pLogDir);

        // This Snprintf has been split into pieces to try to handle pipelines with extremely long names.
        // We will truncate the name string if necessary, preserving the path, prefix, and suffix.
        constexpr int32 MaxLen = 260; // Util::File has an implicit 260 char limit on Windows.
        char  fileName[MaxLen] = {};
        int32 offset = Snprintf(fileName, MaxLen, "%s/%s_", pLogDir, prefix.Data());

        if (offset < 0)
        {
            // Offset will be -1 if not even the path and prefix fit.
            PAL_ASSERT_ALWAYS();
        }
        else
        {
            char*  pNextChar = fileName + offset;
            size_t remaining = MaxLen - offset;

            if (name.IsEmpty())
            {
                Snprintf(pNextChar, remaining, "0x%016llX.elf", hash.stable);
            }
            else
            {
                const size_t copyLen = Util::EncodeAsFilename(pNextChar, remaining - 5, name, false, false);
                pNextChar += copyLen;
                remaining -= copyLen;

                Strncpy(pNextChar, ".elf", remaining);
            }

            File file;
            Result result = file.Open(fileName, FileAccessWrite | FileAccessBinary);

            if (result == Result::Success)
            {
                result = file.Write(pCodeObject, codeObjectLen);
            }

            PAL_ASSERT(result == Result::Success);
        }
    }
}

#if PAL_BUILD_GFX11

// =====================================================================================================================
bool Device::UsePws(
    EngineType engineType
    ) const
{
    return m_engineProperties.perEngine[engineType].flags.supportsPws &&
           (m_publicSettings.pwsMode != PwsMode::Disabled);
}

// =====================================================================================================================
bool Device::UsePwsLateAcquirePoint(
    EngineType engineType
    ) const
{
    return m_engineProperties.perEngine[engineType].flags.supportsPws &&
           (m_publicSettings.pwsMode == PwsMode::Enabled);
}

#endif

} // Pal
