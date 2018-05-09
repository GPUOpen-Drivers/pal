/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/g_palSettings.h"
#include "core/os/lnx/dri3/dri3WindowSystem.h"
#include "core/os/lnx/lnxDevice.h"
#include "core/os/lnx/lnxQueue.h"
#include "core/os/lnx/lnxSyncobjFence.h"
#include "core/os/lnx/lnxGpuMemory.h"
#include "core/os/lnx/lnxImage.h"
#include "core/os/lnx/lnxPlatform.h"
#include "core/os/lnx/lnxScreen.h"
#include "core/os/lnx/lnxSwapChain.h"
#include "core/os/lnx/lnxWindowSystem.h"
#include "core/os/lnx/lnxVamMgr.h"
#include "palAutoBuffer.h"
#include "palHashMapImpl.h"
#include "palInlineFuncs.h"
#include "palSettingsFileMgrImpl.h"
#include "palSysMemory.h"
#include "palSysUtil.h"
#include "palVectorImpl.h"
#include "palIntrusiveListImpl.h"
#include "core/addrMgr/addrMgr1/addrMgr1.h"
#if PAL_BUILD_GFX9
#include "core/addrMgr/addrMgr2/addrMgr2.h"
#else
//  NOTE: We need this for address pipe config.
#include "core/hw/gfxip/gfx6/chip/si_ci_vi_merged_enum.h"
#endif
// NOTE: We need this chip header for reading registers.
#include "core/hw/gfxip/gfx6/chip/si_ci_vi_merged_offset.h"
#include "core/hw/gfxip/gfx6/chip/si_ci_vi_merged_mask.h"

#include <climits>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <sys/utsname.h>

using namespace Util;
using namespace Pal::AddrMgr1;

namespace Pal
{
namespace Linux
{
// =====================================================================================================================
// Helper method which check result from drm function
static PAL_INLINE Result CheckResult(
    int32       ret,
    Result      defaultValue)
{
    Result retValue = Result::Success;
    switch (ret)
    {
    case 0:
        break;
    case -EINVAL:
        retValue = Result::ErrorInvalidValue;
        break;
    case -ENOMEM:
        retValue = Result::ErrorOutOfMemory;
        break;
    case -ENOSPC:
        retValue = Result::OutOfSpec;
        break;
    case -ETIMEDOUT:
    case -ETIME:
        retValue = Result::Timeout;
        break;
    case -ECANCELED:
        retValue = Result::ErrorDeviceLost;
        break;
    default:
        retValue = defaultValue;
        break;
    }
    return retValue;
}

constexpr gpusize _4GB = (1ull << 32u);
constexpr uint32 GpuPageSize = 4096;

constexpr char SettingsFileName[] = "amdPalSettings.cfg";

// Maybe the format supported by presentable image should be got from Xserver, so far we just use a fixed format list.
constexpr SwizzledFormat PresentableImageFormats[] =
{
    {   ChNumFormat::X8Y8Z8W8_Srgb,
        { ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::W },
    },
    {   ChNumFormat::X8Y8Z8W8_Unorm,
        { ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::W },
    },
};

// The amdgpu queue semaphores are binary semaphores so their counts are always either zero or one.
constexpr uint32 MaxSemaphoreCount = 1;

static LocalMemoryType TranslateMemoryType(uint32 memType);
static Result OpenAndInitializeDrmDevice(
    Platform*               pPlatform,
    const char*             pBusId,
    const char*             pPrimaryNodeName,
    const char*             pRenderNodeName,
    uint32*                 pFileDescriptor,
    amdgpu_device_handle*   pDeviceHandle,
    uint32*                 pDrmMajorVer,
    uint32*                 pDrmMinorVer,
    struct amdgpu_gpu_info* pGpuInfo,
    uint32*                 pCpVersion);

// =====================================================================================================================
Result Device::Create(
    Platform*               pPlatform,
    const char*             pSettingsPath,
    const char*             pBusId,
    const char*             pPrimaryNode,
    const char*             pRenderNode,
    const drmPciBusInfo&    pciBusInfo,
    uint32                  deviceIndex,
    Device**                ppDeviceOut)
{
    HwIpLevels             ipLevels           = {};
    HwIpDeviceSizes        hwDeviceSizes      = {};
    size_t                 addrMgrSize        = 0;
    uint32                 fileDescriptor     = 0;
    amdgpu_device_handle   hDevice            = nullptr;
    uint32                 drmMajorVer        = 0;
    uint32                 drmMinorVer        = 0;
    struct amdgpu_gpu_info gpuInfo            = {};
    uint32                 cpVersion          = 0;
    uint32                 attachedScreenCount= 0;

    Result result = OpenAndInitializeDrmDevice(pPlatform,
                                               pBusId,
                                               pPrimaryNode,
                                               pRenderNode,
                                               &fileDescriptor,
                                               &hDevice,
                                               &drmMajorVer,
                                               &drmMinorVer,
                                               &gpuInfo,
                                               &cpVersion);

    if (result == Result::Success)
    {
        if (Device::DetermineGpuIpLevels(gpuInfo.family_id, gpuInfo.chip_external_rev, cpVersion, &ipLevels) == false)
        {
            result = Result::ErrorInitializationFailed;
        }
    }

    if (result == Result::Success)
    {
        Device::GetHwIpDeviceSizes(ipLevels, &hwDeviceSizes, &addrMgrSize);

        size_t totalSize = 0;
        totalSize += sizeof(Device);
        totalSize += (hwDeviceSizes.gfx   +
                      hwDeviceSizes.oss   +
                      addrMgrSize);

        void*  pMemory  = PAL_MALLOC_BASE(totalSize,
                                          alignof(Device),
                                          pPlatform,
                                          Util::AllocInternal,
                                          Util::MemBlkType::Malloc);

        if (pMemory != nullptr)
        {
            const uint32 deviceNodeIndex = atoi(strstr(pPrimaryNode, "card") + strlen("card"));

            (*ppDeviceOut) = PAL_PLACEMENT_NEW(pMemory) Device(pPlatform,
                                                               pSettingsPath,
                                                               pBusId,
                                                               pRenderNode,
                                                               pPrimaryNode,
                                                               fileDescriptor,
                                                               hDevice,
                                                               drmMajorVer,
                                                               drmMinorVer,
                                                               sizeof(Device),
                                                               deviceIndex,
                                                               deviceNodeIndex,
                                                               attachedScreenCount,
                                                               gpuInfo,
                                                               hwDeviceSizes,
                                                               pciBusInfo);

            result = (*ppDeviceOut)->EarlyInit(ipLevels);

            if (result != Result::Success)
            {
                (*ppDeviceOut)->~Device();
                PAL_SAFE_FREE((*ppDeviceOut), pPlatform);
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
// This is help methods. Open drm device and initialize it.
// And also get the drm information.
static Result OpenAndInitializeDrmDevice(
    Platform*               pPlatform,
    const char*             pBusId,
    const char*             pPrimaryNode,
    const char*             pRenderNode,
    uint32*                 pFileDescriptor,
    amdgpu_device_handle*   pDeviceHandle,
    uint32*                 pDrmMajorVer,
    uint32*                 pDrmMinorVer,
    struct amdgpu_gpu_info* pGpuInfo,
    uint32*                 pCpVersion)
{
    Result                 result       = Result::Success;
    amdgpu_device_handle   deviceHandle = NULL;
    uint32                 majorVersion = 0;
    uint32                 minorVersion = 0;

    // Using render node here so that we can do the off-screen rendering without authentication.
    int32 fd = open(pRenderNode, O_RDWR, 0);

    const DrmLoaderFuncs& procs = pPlatform->GetDrmLoader().GetProcsTable();

    if (fd < 0 )
    {
        result = Result::ErrorInitializationFailed;
    }
    else
    {
        // Initialize the amdgpu device.
        result = CheckResult(procs.pfnAmdgpuDeviceInitialize(fd, &majorVersion, &minorVersion, &deviceHandle),
                             Result::ErrorInitializationFailed);
    }

    if (result == Result::Success)
    {
        uint32 version = 0;
        // amdgpu_query_gpu_info will never fail only if it is initialized.
        procs.pfnAmdgpuQueryGpuInfo(deviceHandle, pGpuInfo);
        if (procs.pfnAmdgpuQueryFirmwareVersion(deviceHandle,
                                             AMDGPU_INFO_FW_GFX_ME,
                                             0,
                                             0,
                                             &version,
                                             pCpVersion) != 0)
        {
            result = Result::ErrorInitializationFailed;
        }
    }

    if (result == Result::Success)
    {
        *pFileDescriptor = fd;
        *pDeviceHandle   = deviceHandle;
        *pDrmMajorVer    = majorVersion;
        *pDrmMinorVer    = minorVersion;
    }
    else
    {
        if (deviceHandle != nullptr)
        {
            procs.pfnAmdgpuDeviceDeinitialize(deviceHandle);
            *pDeviceHandle = nullptr;
        }
        if (fd > 0)
        {
            close(fd);
            fd = 0;
            *pFileDescriptor = 0;
        }
    }

    return result;
}

// =====================================================================================================================
Device::Device(
    Platform*                   pPlatform,
    const char*                 pSettingsPath,
    const char*                 pBusId,
    const char*                 pRenderNode,
    const char*                 pPrimaryNode,
    uint32                      fileDescriptor,
    amdgpu_device_handle        hDevice,
    uint32                      drmMajorVer,
    uint32                      drmMinorVer,
    size_t                      deviceSize,
    uint32                      deviceIndex,
    uint32                      deviceNodeIndex,
    uint32                      attachedScreenCount,
    const amdgpu_gpu_info&      gpuInfo,
    const HwIpDeviceSizes&      hwDeviceSizes,
    const drmPciBusInfo&        pciBusInfo)
    :
    Pal::Device(pPlatform, deviceIndex, attachedScreenCount, deviceSize, hwDeviceSizes, MaxSemaphoreCount),
    m_fileDescriptor(fileDescriptor),
    m_masterFileDescriptor(0),
    m_hDevice(hDevice),
    m_hContext(nullptr),
    m_deviceNodeIndex(deviceNodeIndex),
    m_drmMajorVer(drmMajorVer),
    m_drmMinorVer(drmMinorVer),
    m_useDedicatedVmid(false),
    m_pSettingsPath(pSettingsPath),
    m_settingsMgr(SettingsFileName, pPlatform),
    m_pSvmMgr(nullptr),
    m_mapAllocator(),
    m_reservedVaMap(32, &m_mapAllocator),
    m_supportQuerySensorInfo(false),
    m_globalRefMap(MemoryRefMapElements, pPlatform),
    m_semType(SemaphoreType::Legacy),
    m_fenceType(FenceType::Legacy),
    m_supportQueuePriority(false),
    m_supportVmAlwaysValid(false),
#if defined(PAL_DEBUG_PRINTS)
    m_drmProcs(pPlatform->GetDrmLoader().GetProcsTableProxy()),
#else
    m_drmProcs(pPlatform->GetDrmLoader().GetProcsTable())
#endif
{
    Util::Strncpy(m_busId, pBusId, MaxBusIdStringLen);
    Util::Strncpy(m_renderNodeName, pRenderNode, MaxNodeNameLen);
    Util::Strncpy(m_primaryNodeName, pPrimaryNode, MaxNodeNameLen);

    memcpy(&m_gpuInfo, &gpuInfo, sizeof(gpuInfo));

    m_chipProperties.pciBusNumber      = pciBusInfo.bus;
    m_chipProperties.pciDeviceNumber   = pciBusInfo.dev;
    m_chipProperties.pciFunctionNumber = pciBusInfo.func;
    m_chipProperties.gpuConnectedViaThunderbolt = false;

    memset(m_supportsPresent, 0, sizeof(m_supportsPresent));
}

// =====================================================================================================================
Device::~Device()
{
    if (m_hContext != nullptr)
    {
        m_drmProcs.pfnAmdgpuCsCtxFree(m_hContext);
        m_hContext = nullptr;
    }

    if (m_useDedicatedVmid)
    {
        m_drmProcs.pfnAmdgpuCsUnreservedVmid(m_hDevice);
    }

    VamMgrSingleton::Cleanup(this);
    if (m_hDevice != nullptr)
    {
        m_drmProcs.pfnAmdgpuDeviceDeinitialize(m_hDevice);
        m_hDevice = nullptr;
    }
    if (m_fileDescriptor > 0)
    {
        close(m_fileDescriptor);
        m_fileDescriptor = 0;
    }

    if (m_masterFileDescriptor > 0)
    {
        close(m_masterFileDescriptor);
        m_masterFileDescriptor = 0;
    }
}

// =====================================================================================================================
// This must clean up all internal GPU memory allocations and all objects created after EarlyInit and OsEarlyInit.
Result Device::Cleanup()
{
    Result result = Result::Success;

    if (GetSvmMgr() != nullptr)
    {
        result = GetSvmMgr()->Cleanup();
    }

    if (result == Result::Success)
    {
        result = Pal::Device::Cleanup();
    }

    PAL_SAFE_DELETE(m_pSvmMgr, m_pPlatform);

    // Note: Pal::Device::Cleanup() uses m_memoryProperties.vaRanges to find VAM sections for memory release.
    // If ranges aren't provided, then VAM silently leaks virtual addresses.
    VamMgrSingleton::FreeReservedVaRange(GetPlatform()->GetDrmLoader().GetProcsTable(), m_hDevice);
    memset(&m_memoryProperties.vaRange, 0, sizeof(m_memoryProperties.vaRange));
    return result;
}

// =====================================================================================================================
// Performs OS-specific early initialization steps for this Device object. Anything created or initialized by this
// function can only be destroyed or deinitialized on Device destruction.
Result Device::OsEarlyInit()
{
    Result result = m_globalRefMap.Init();

    if (result == Result::Success)
    {
        result = m_globalRefLock.Init();
    }

    if (result == Result::Success)
    {
        result = InitClkInfo();
    }

    return result;
}

// =====================================================================================================================
// Performs potentially unsafe OS-specific late initialization steps for this Device object. Anything created or
// initialized by this function must be destroyed or deinitialized in Cleanup().
Result Device::OsLateInit()
{
    Result result = Result::Success;
    // if we need to require dedicated per-process VMID
    if (Settings().requestDebugVmid && m_drmProcs.pfnAmdgpuCsReservedVmidisValid())
    {
        if (m_drmProcs.pfnAmdgpuCsReservedVmid(m_hDevice) != 0)
        {
            result = Result::ErrorInvalidValue;
        }
        else
        {
            m_useDedicatedVmid = true;
        }
    }

    if (static_cast<Platform*>(m_pPlatform)->IsProSemaphoreSupported())
    {
        m_semType = SemaphoreType::ProOnly;
    }

    // check sync object support status - with parital or complete features
    CheckSyncObjectSupportStatus();

    // reconfigure Semaphore/Fence Type with m_syncobjSupportState.
    if ((Settings().disableSyncObject == false) && m_syncobjSupportState.SyncobjSemaphore)
    {
        m_semType = SemaphoreType::SyncObj;

        if ((Settings().disableSyncobjFence == false) && m_syncobjSupportState.SyncobjFence)
        {
            m_fenceType = FenceType::SyncObj;
        }
    }

    // Current valid FenceType/SemaphoreType combination:
    // - Timestamp Fence + any Semaphore type.
    // - Syncobj Fence + Syncobj Semaphore.
    PAL_ASSERT((m_fenceType != FenceType::SyncObj) || (m_semType == SemaphoreType::SyncObj));

    // DrmVersion should be equal or greater than 3.22 in case to support queue priority
    if (static_cast<Platform*>(m_pPlatform)->IsQueuePrioritySupported() && IsDrmVersionOrGreater(3,22))
    {
        m_supportQueuePriority = true;
    }

    // Start to support per-vm bo from drm 3.20, but bugs were not fixed
    // until drm 3.25 on pro dkms stack or kernel 4.16 on upstream stack.
    if ((Settings().enableVmAlwaysValid == VmAlwaysValidForceEnable) ||
       ((Settings().enableVmAlwaysValid == VmAlwaysValidDefaultEnable) &&
       (IsDrmVersionOrGreater(3,25) || IsKernelVersionEqualOrGreater(4,16))))
    {
        m_supportVmAlwaysValid = true;
    }

    if (IsDrmVersionOrGreater(3,25))
    {
        m_supportQuerySensorInfo = true;
    }

    return result;
}

// =====================================================================================================================
Result Device::Finalize(
    const DeviceFinalizeInfo& finalizeInfo)
{
    Result result = Pal::Device::Finalize(finalizeInfo);

    if ((result == Result::Success) && m_pPlatform->SvmModeEnabled() &&
        (MemoryProperties().flags.iommuv2Support == 0))
    {
        m_pSvmMgr = PAL_NEW(SvmMgr, GetPlatform(), AllocInternal)(this);
        if (m_pSvmMgr != nullptr)
        {
            result = GetSvmMgr()->Init();
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    return result;
}

// =====================================================================================================================
Result Device::EarlyInit(
    const HwIpLevels& ipLevels)
{
    m_chipProperties.gfxLevel = ipLevels.gfx;
    m_chipProperties.ossLevel = ipLevels.oss;
    m_chipProperties.vceLevel = ipLevels.vce;
    m_chipProperties.uvdLevel = ipLevels.uvd;
    m_chipProperties.vcnLevel = ipLevels.vcn;

    Result result = VamMgrSingleton::Init();

    if (result == Result::Success)
    {
        result = InitGpuProperties();
    }

    if (result == Result::Success)
    {
        result = m_settingsMgr.Init(m_pSettingsPath);

        if (result == Result::ErrorUnavailable)
        {
            //Unavailable means that the file was not found, which is an acceptable failure.
            PAL_ALERT_ALWAYS();
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

    // Currently we don't have WaitForFences support for batched fences. This is OK because Vulkan is the only Linux
    // client and the Vulkan API forbids the application from triggering batching. However, PAL will trigger batching
    // internally unless we disable this swap chain optimization. In the long-term we should fix this to improve Linux
    // performance in applications that acquire their swap chain images early.
    m_disableSwapChainAcquireBeforeSignaling = true;

    // get the attached screen count
    GetScreens(&m_attachedScreenCount, nullptr, nullptr);

    return result;
}

// =====================================================================================================================
// Helper method which finalizes some of the Queue properties which cannot be determined until the settings are read.
void Device::FinalizeQueueProperties()
{
    m_engineProperties.maxInternalRefsPerSubmission = InternalMemMgrAllocLimit;
    m_engineProperties.maxUserMemRefsPerSubmission  = CmdBufMemReferenceLimit;

    m_engineProperties.perEngine[EngineTypeCompute].flags.supportVirtualMemoryRemap   = 1;
    m_engineProperties.perEngine[EngineTypeDma].flags.supportVirtualMemoryRemap       = 1;
    m_engineProperties.perEngine[EngineTypeUniversal].flags.supportVirtualMemoryRemap = 1;

    constexpr uint32 WindowedIdx = static_cast<uint32>(PresentMode::Windowed);
    constexpr uint32 FullscreenIdx = static_cast<uint32>(PresentMode::Fullscreen);

    // We can assume that we modes are valid on all WsiPlatforms
    m_supportedSwapChainModes[WindowedIdx]   =
        SupportImmediateSwapChain| SupportFifoSwapChain| SupportMailboxSwapChain;

    m_supportedSwapChainModes[FullscreenIdx] =
        SupportImmediateSwapChain| SupportFifoSwapChain| SupportMailboxSwapChain;

    static_assert(AMDGPU_CS_MAX_IBS_PER_SUBMIT >= MinCmdStreamsPerSubmission,
                  "The minimum supported number of command streams per submission is not enough for PAL!");
    if (Settings().maxNumCmdStreamsPerSubmit == 0)
    {
        m_queueProperties.maxNumCmdStreamsPerSubmit = AMDGPU_CS_MAX_IBS_PER_SUBMIT;
    }
    else
    {
        m_queueProperties.maxNumCmdStreamsPerSubmit =
            Max<uint32>(MinCmdStreamsPerSubmission, Min<uint32>(AMDGPU_CS_MAX_IBS_PER_SUBMIT,
                                                                Settings().maxNumCmdStreamsPerSubmit));
    }

    // Disable mid command buffer preemption on the DMA and Universal Engines if the setting has the feature disabled.
    // Furthermore, if the KMD does not support at least seven UDMA buffers per submission, we cannot support preemption
    // on the Universal Engine.
    if (((Settings().commandBufferPreemptionFlags & UniversalEnginePreemption) == 0) ||
        (m_queueProperties.maxNumCmdStreamsPerSubmit < 7))
    {
        m_engineProperties.perEngine[EngineTypeUniversal].flags.supportsMidCmdBufPreemption = 0;
        m_engineProperties.perEngine[EngineTypeUniversal].contextSaveAreaSize               = 0;
        m_engineProperties.perEngine[EngineTypeUniversal].contextSaveAreaAlignment          = 0;
    }
    if ((Settings().commandBufferPreemptionFlags & DmaEnginePreemption) == 0)
    {
        m_engineProperties.perEngine[EngineTypeDma].flags.supportsMidCmdBufPreemption = 0;
        m_engineProperties.perEngine[EngineTypeDma].contextSaveAreaSize               = 0;
        m_engineProperties.perEngine[EngineTypeDma].contextSaveAreaAlignment          = 0;
    }
}

// =====================================================================================================================
// Fills out a structure with details on the properties of this GPU object. This includes capability flags, supported
// queues, performance characteristics, OS-specific properties.
// NOTE: Part of the IDevice public interface.
Result Device::GetProperties(
    DeviceProperties* pInfo
    ) const
{
    Result result = Pal::Device::GetProperties(pInfo);

    if (result == Result::Success)
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 378
        pInfo->osProperties.supportOpaqueFdSemaphore = ((m_semType == SemaphoreType::ProOnly) ||
                                                        (m_semType == SemaphoreType::SyncObj));
        // Todo: Implement the sync file import/export upon sync object.
        pInfo->osProperties.supportSyncFileSemaphore = false;
#else
        pInfo->osProperties.supportProSemaphore      = ((m_semType == SemaphoreType::ProOnly) ||
                                                        (m_semType == SemaphoreType::SyncObj));
#endif

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 398
        pInfo->osProperties.supportSyncFileSemaphore = (m_semType == SemaphoreType::SyncObj);
        pInfo->osProperties.supportSyncFileFence     = (m_fenceType == FenceType::SyncObj);
#endif

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 364
        pInfo->osProperties.supportQueuePriority = m_supportQueuePriority;
        // Linux don't support changing the queue priority at the submission granularity.
        pInfo->osProperties.supportDynamicQueuePriority = false;
#endif

        pInfo->gpuMemoryProperties.flags.supportHostMappedForeignMemory =
            static_cast<Platform*>(m_pPlatform)->IsHostMappedForeignMemorySupported();
    }

    return result;
}

// =====================================================================================================================
// Initializes the GPU properties structures of this object's base class (Device). This includes the GPU-Memory
// properties, Queue properties, Chip properties and GPU name string.
Result Device::InitGpuProperties()
{
    uint32 version = 0;
    uint32 feature = 0;

    m_chipProperties.familyId   = m_gpuInfo.family_id;
    m_chipProperties.eRevId     = m_gpuInfo.chip_external_rev;
    m_chipProperties.revisionId = m_gpuInfo.pci_rev_id;
    m_chipProperties.deviceId   = m_gpuInfo.asic_id;
    m_chipProperties.gpuIndex   = 0; // Multi-GPU is not supported so far.

    m_chipProperties.imageProperties.minPitchAlignPixel = 0;

    // The unit of amdgpu is KHz but PAL is Hz
    m_chipProperties.gpuCounterFrequency = m_gpuInfo.gpu_counter_freq * 1000;

    // The unit of amdgpu is KHz but PAL is MHz
    m_chipProperties.maxEngineClock      = m_gpuInfo.max_engine_clk / 1000;
    m_chipProperties.maxMemoryClock      = m_gpuInfo.max_memory_clk / 1000;

    m_drmProcs.pfnAmdgpuQueryFirmwareVersion(m_hDevice,
                                  AMDGPU_INFO_FW_GFX_ME,
                                  0,
                                  0,
                                  &version,
                                  &feature);
    m_engineProperties.cpUcodeVersion = feature;

    const char* pMarketingName = m_drmProcs.pfnAmdgpuGetMarketingNameisValid() ?
                                 m_drmProcs.pfnAmdgpuGetMarketingName(m_hDevice) : nullptr;
    if (pMarketingName != nullptr)
    {
        Strncpy(&m_gpuName[0], pMarketingName, sizeof(m_gpuName));
    }
    else
    {
        Strncpy(&m_gpuName[0], "Unknown AMD GPU", sizeof(m_gpuName));
    }

    // ToDo: Retrieve ceram size of gfx engine from kmd, but the functionality is not supported yet.
    switch (m_chipProperties.gfxLevel)
    {
#if PAL_BUILD_GFX6
    case GfxIpLevel::GfxIp6:
    case GfxIpLevel::GfxIp7:
    case GfxIpLevel::GfxIp8:
    case GfxIpLevel::GfxIp8_1:
        m_chipProperties.gfxEngineId = CIASICIDGFXENGINE_SOUTHERNISLAND;
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
        m_chipProperties.gfxEngineId = CIASICIDGFXENGINE_ARCTICISLAND;
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

    Result result = InitMemQueueInfo();

    if (result == Result::Success)
    {
        m_chipProperties.gfxip.ceRamSize = m_gpuInfo.ce_ram_size;
        m_engineProperties.perEngine[EngineTypeUniversal].availableCeRamSize =
            m_gpuInfo.ce_ram_size - m_engineProperties.perEngine[EngineTypeUniversal].reservedCeRamSize;

        InitPerformanceRatings();
        InitMemoryHeapProperties();
    }

    return result;

}

#if PAL_BUILD_GFX6
// =====================================================================================================================
// Helper method which initializes the GPU chip properties for all hardware families using the GFX6 hardware layer.
void Device::InitGfx6ChipProperties()
{
    auto*const                    pChipInfo  = &m_chipProperties.gfx6;
    struct drm_amdgpu_info_device deviceInfo = {};

    memcpy(&pChipInfo->gbTileMode[0], &m_gpuInfo.gb_tile_mode[0], sizeof(pChipInfo->gbTileMode));
    memcpy(&pChipInfo->gbMacroTileMode[0], &m_gpuInfo.gb_macro_tile_mode[0], sizeof(pChipInfo->gbMacroTileMode));

    Gfx6::InitializeGpuChipProperties(m_engineProperties.cpUcodeVersion, &m_chipProperties);
    InitGfx6CuMask();

    if (!m_drmProcs.pfnAmdgpuBoVaOpRawisValid())
    {
        m_chipProperties.imageProperties.prtFeatures = static_cast<PrtFeatureFlags>(0);
    }

    // It should be per engine, but PAL does not. So just use the first one.
    pChipInfo->backendDisableMask = m_gpuInfo.backend_disable[0];
    pChipInfo->paScRasterCfg      = m_gpuInfo.pa_sc_raster_cfg[0];
    pChipInfo->paScRasterCfg1     = m_gpuInfo.pa_sc_raster_cfg1[0];

    uint32 spiConfigCntl = 0;
    ReadRegisters(mmSPI_CONFIG_CNTL, 1, 0xffffffff, 0, &spiConfigCntl);
    pChipInfo->sqgEventsEnabled = ((spiConfigCntl & SPI_CONFIG_CNTL__ENABLE_SQG_TOP_EVENTS_MASK) &&
                                   (spiConfigCntl & SPI_CONFIG_CNTL__ENABLE_SQG_BOP_EVENTS_MASK));

    pChipInfo->gbAddrConfig = m_gpuInfo.gb_addr_cfg;
    pChipInfo->mcArbRamcfg  = m_gpuInfo.mc_arb_ramcfg;

    pChipInfo->numShaderEngines = m_gpuInfo.num_shader_engines;
    pChipInfo->numShaderArrays  = m_gpuInfo.num_shader_arrays_per_engine;

    switch (m_chipProperties.gfxLevel)
    {
    case GfxIpLevel::GfxIp6:
    case GfxIpLevel::GfxIp7:
        ReadRegisters(mmSQ_THREAD_TRACE_MASK__SI__CI, 1,  0xffffffff, 0, &pChipInfo->sqThreadTraceMask);
        break;
    case GfxIpLevel::GfxIp8:
    case GfxIpLevel::GfxIp8_1:
        ReadRegisters(mmSQ_THREAD_TRACE_MASK__VI, 1, 0xffffffff, 0, &pChipInfo->sqThreadTraceMask);
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    pChipInfo->numMcdTiles = (m_gpuInfo.vram_bit_width / 64);

    if (m_drmProcs.pfnAmdgpuQueryInfo(m_hDevice, AMDGPU_INFO_DEV_INFO, sizeof(deviceInfo), &deviceInfo) == 0)
    {
        pChipInfo->doubleOffchipLdsBuffers = deviceInfo.gc_double_offchip_lds_buf;
    }

    Gfx6::FinalizeGpuChipProperties(&m_chipProperties);
    Gfx6::InitializePerfExperimentProperties(m_chipProperties, &m_perfExperimentProperties);

    m_engineProperties.perEngine[EngineTypeUniversal].flags.supportsMidCmdBufPreemption =
        (m_gpuInfo.ids_flags & AMDGPU_IDS_FLAGS_PREEMPTION) ? 1 : 0;
    m_engineProperties.perEngine[EngineTypeUniversal].contextSaveAreaSize               = 0;
    m_engineProperties.perEngine[EngineTypeUniversal].contextSaveAreaAlignment          = 0;

    m_engineProperties.perEngine[EngineTypeDma].flags.supportsMidCmdBufPreemption       =
        (m_gpuInfo.ids_flags & AMDGPU_IDS_FLAGS_PREEMPTION) ? 1 : 0;
    m_engineProperties.perEngine[EngineTypeDma].contextSaveAreaSize                     = 0;
    m_engineProperties.perEngine[EngineTypeDma].contextSaveAreaAlignment                = 0;
}

// =====================================================================================================================
// Helper method which gets the CuMasks and always on cu masks.
void Device::InitGfx6CuMask()
{
    auto*const pChipInfo = &m_chipProperties.gfx6;

    for (uint32 seIndex = 0; seIndex < m_gpuInfo.num_shader_engines; seIndex++)
    {
        constexpr uint32 AlwaysOnSeMaskSize = 16;
        constexpr uint32 AlwaysOnSeMask     = (1ul << AlwaysOnSeMaskSize) - 1;
        constexpr uint32 AlwaysOnShMaskSize = 8;
        constexpr uint32 AlwaysOnShMask     = (1ul << AlwaysOnShMaskSize) - 1;

        const uint32 aoSeMask = (m_gpuInfo.cu_ao_mask >> (seIndex * AlwaysOnSeMaskSize)) & AlwaysOnSeMask;

        // GFXIP 7+ hardware only has one shader array per shader engine!
        PAL_ASSERT(m_chipProperties.gfxLevel < GfxIpLevel::GfxIp7 || pChipInfo->numShaderArrays == 1);

        for (uint32 shIndex = 0; shIndex < m_gpuInfo.num_shader_arrays_per_engine; shIndex++)
        {
            if (m_chipProperties.gfxLevel == GfxIpLevel::GfxIp6)
            {
                const uint32 aoMask = (aoSeMask >> (shIndex * AlwaysOnShMaskSize)) & AlwaysOnShMask;
                pChipInfo->activeCuMaskGfx6[seIndex][shIndex]   = m_gpuInfo.cu_bitmap[seIndex][shIndex];
                pChipInfo->alwaysOnCuMaskGfx6[seIndex][shIndex] = aoMask;
            }
            else
            {
                pChipInfo->activeCuMaskGfx7[seIndex]   = m_gpuInfo.cu_bitmap[seIndex][shIndex];
                pChipInfo->alwaysOnCuMaskGfx7[seIndex] = aoSeMask;
            }
        }
    }
}
#endif

#if PAL_BUILD_GFX9
// =====================================================================================================================
// Helper method which initializes the GPU chip properties for all hardware families using the GFX9 hardware layer.
void Device::InitGfx9ChipProperties()
{
    auto*const                    pChipInfo  = &m_chipProperties.gfx9;
    struct drm_amdgpu_info_device deviceInfo = {};

    InitGfx9CuMask();
    // Call into the HWL to initialize the default values for many properties of the hardware (based on chip ID).
    Gfx9::InitializeGpuChipProperties(m_engineProperties.cpUcodeVersion, &m_chipProperties);

    if (!m_drmProcs.pfnAmdgpuBoVaOpRawisValid())
    {
        m_chipProperties.imageProperties.prtFeatures = static_cast<PrtFeatureFlags>(0);
    }

    pChipInfo->gbAddrConfig = m_gpuInfo.gb_addr_cfg;

    if (m_drmProcs.pfnAmdgpuQueryInfo(m_hDevice, AMDGPU_INFO_DEV_INFO, sizeof(deviceInfo), &deviceInfo) == 0)
    {
        pChipInfo->numShaderEngines        = deviceInfo.num_shader_engines;
        pChipInfo->numShaderArrays         = deviceInfo.num_shader_arrays_per_engine;
        pChipInfo->maxNumRbPerSe           = deviceInfo.num_rb_pipes / deviceInfo.num_shader_engines;
        pChipInfo->wavefrontSize           = deviceInfo.wave_front_size;
        pChipInfo->numShaderVisibleVgprs   = deviceInfo.num_shader_visible_vgprs;
        pChipInfo->maxNumCuPerSh           = deviceInfo.num_cu_per_sh;
        pChipInfo->numTccBlocks            = deviceInfo.num_tcc_blocks;
        pChipInfo->gsVgtTableDepth         = deviceInfo.gs_vgt_table_depth;
        pChipInfo->gsPrimBufferDepth       = deviceInfo.gs_prim_buffer_depth;
        pChipInfo->maxGsWavesPerVgt        = deviceInfo.max_gs_waves_per_vgt;
        pChipInfo->doubleOffchipLdsBuffers = deviceInfo.gc_double_offchip_lds_buf;
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    // Get the disabled render backend mask. m_gpuInfo.backend_disable is per se, m_gpuInfo.backend_disable[0]
    // is for se[0]. However backendDisableMask is in following organization if RbPerSe is 4,
    // *         b15 b14 b13 b12 - b11 b10 b9 b8 - b7 b6 b5 b4 - b3 b2 b1 b0
    // *             SE3/SH0          SE2/SH0        SE1/SH0       SE0/SH0
    pChipInfo->backendDisableMask = 0;

    for (uint32 i = 0; i < deviceInfo.num_shader_engines; i++)
    {
        uint32 disabledRbMaskPerSe     = (1 << pChipInfo->maxNumRbPerSe) - 1;
        uint32 disabledRbBits          = m_gpuInfo.backend_disable[i] & disabledRbMaskPerSe;
        pChipInfo->backendDisableMask |= disabledRbBits << (i * pChipInfo->maxNumRbPerSe);
    }

    // Call into the HWL to finish initializing some GPU properties which can be derived from the ones which we
    // overrode above.
    Gfx9::FinalizeGpuChipProperties(&m_chipProperties);

    pChipInfo->numActiveRbs = CountSetBits(m_gpuInfo.enabled_rb_pipes_mask);

    pChipInfo->primShaderInfo.primitiveBufferVa   = deviceInfo.prim_buf_gpu_addr;
    pChipInfo->primShaderInfo.primitiveBufferSize = deviceInfo.prim_buf_size;
    pChipInfo->primShaderInfo.positionBufferVa    = deviceInfo.pos_buf_gpu_addr;
    pChipInfo->primShaderInfo.positionBufferSize  = deviceInfo.pos_buf_size;
    pChipInfo->primShaderInfo.controlSidebandVa   = deviceInfo.cntl_sb_buf_gpu_addr;
    pChipInfo->primShaderInfo.controlSidebandSize = deviceInfo.cntl_sb_buf_size;
    pChipInfo->primShaderInfo.parameterCacheVa    = deviceInfo.param_buf_gpu_addr;
    pChipInfo->primShaderInfo.parameterCacheSize  = deviceInfo.param_buf_size;

    Gfx9::InitializePerfExperimentProperties(m_chipProperties, &m_perfExperimentProperties);

    m_engineProperties.perEngine[EngineTypeUniversal].flags.supportsMidCmdBufPreemption =
        (m_gpuInfo.ids_flags & AMDGPU_IDS_FLAGS_PREEMPTION) ? 1 : 0;
    m_engineProperties.perEngine[EngineTypeUniversal].contextSaveAreaSize               = 0;
    m_engineProperties.perEngine[EngineTypeUniversal].contextSaveAreaAlignment          = 0;

    m_engineProperties.perEngine[EngineTypeDma].flags.supportsMidCmdBufPreemption       =
        (m_gpuInfo.ids_flags & AMDGPU_IDS_FLAGS_PREEMPTION) ? 1 : 0;
    m_engineProperties.perEngine[EngineTypeDma].contextSaveAreaSize                     = 0;
    m_engineProperties.perEngine[EngineTypeDma].contextSaveAreaAlignment                = 0;
}

// =====================================================================================================================
// Helper method which gets the CuMasks and always on cu masks.
void Device::InitGfx9CuMask()
{
    auto*const pChipInfo = &m_chipProperties.gfx9;
    for (uint32 shIndex = 0; shIndex < m_gpuInfo.num_shader_arrays_per_engine; shIndex++)
    {
        for (uint32 seIndex = 0; seIndex < m_gpuInfo.num_shader_engines; seIndex++)
        {
            pChipInfo->activeCuMask[shIndex][seIndex] = m_gpuInfo.cu_bitmap[seIndex][shIndex];

            constexpr uint32 AlwaysOnSeMaskSize = 16;
            constexpr uint32 AlwaysOnSeMask     = (1ul << AlwaysOnSeMaskSize) - 1;

            const uint32 aoSeMask = (m_gpuInfo.cu_ao_mask >> (seIndex * AlwaysOnSeMaskSize)) & AlwaysOnSeMask;
            pChipInfo->alwaysOnCuMask[shIndex][seIndex] = aoSeMask;
        }
    }
}
#endif

// =====================================================================================================================
// Helper method which translate the amdgpu vram type into LocalMemoryType.
static LocalMemoryType TranslateMemoryType(
    uint32 memType)
{
    LocalMemoryType result = LocalMemoryType::Unknown;

    switch (memType)
    {
    case AMDGPU_VRAM_TYPE_UNKNOWN:
        {
            // Unknown  memory type
            PAL_ASSERT_ALWAYS();
            break;
        }

    case AMDGPU_VRAM_TYPE_GDDR1:
    case AMDGPU_VRAM_TYPE_GDDR3:
    case AMDGPU_VRAM_TYPE_GDDR4:
        {
            // PAL does not support any ASICs with GDDR(1/3/4) memory
            PAL_ASSERT_ALWAYS();
            break;
        }

    case AMDGPU_VRAM_TYPE_DDR2:
        {
            result = LocalMemoryType::Ddr2;
            break;
        }

    case AMDGPU_VRAM_TYPE_DDR3:
        {
            result = LocalMemoryType::Ddr3;
            break;
        }

    case AMDGPU_VRAM_TYPE_GDDR5:
        {
            result = LocalMemoryType::Gddr5;
            break;
        }

    case AMDGPU_VRAM_TYPE_HBM:
        {
            result = LocalMemoryType::Hbm;
            break;
        }

    default:
        {
            // Unhandled memory type
            PAL_ASSERT_ALWAYS();
            break;
        }
    }
    return result;
}

// =====================================================================================================================
// Helper method which initializes the GPU memory and queue properties.
Result Device::InitMemQueueInfo()
{
    Result                        result  = Result::Success;
    struct drm_amdgpu_memory_info memInfo = {};

    m_memoryProperties.localMemoryType    = TranslateMemoryType(m_gpuInfo.vram_type);
    m_memoryProperties.memOpsPerClock     = MemoryOpsPerClock(m_memoryProperties.localMemoryType);
    m_memoryProperties.vramBusBitWidth    = m_gpuInfo.vram_bit_width;
    m_memoryProperties.apuBandwidthFactor = 100;

    // NOTE: libdrm_amdgpu does not support the unmap-info buffer. This shouldn't be a problem for us because
    // libdrm_amdgpu also manages the PD and PTB's for us.
    m_memoryProperties.uibVersion = 0;

    // Since libdrm_amdgpu manages pde/pte for us, we can't get the size of a PDE or PTE entry, nor how much address
    // space is mapped by a single PDE. We need to hardcode these to make the Vam work.
    m_memoryProperties.pdeSize           = sizeof(gpusize);
    m_memoryProperties.pteSize           = sizeof(gpusize);
    m_memoryProperties.spaceMappedPerPde = (256ull * 1024ull * 1024ull);
    m_memoryProperties.numPtbsPerGroup   = 1;

    uint64_t startVa = 0;
    uint64_t endVa = 0;
    if (m_drmProcs.pfnAmdgpuQueryPrivateApertureisValid() &&
       (m_drmProcs.pfnAmdgpuQueryPrivateAperture(m_hDevice, &startVa, &endVa) == 0))
    {
        m_memoryProperties.privateApertureBase = startVa;
    }

    if (m_drmProcs.pfnAmdgpuQuerySharedApertureisValid() &&
       (m_drmProcs.pfnAmdgpuQuerySharedAperture(m_hDevice, &startVa, &endVa) == 0))
    {
        m_memoryProperties.sharedApertureBase = startVa;
    }

    if (m_drmProcs.pfnAmdgpuVaRangeQuery(m_hDevice,
                              amdgpu_gpu_va_range_general,
                              &m_memoryProperties.vaStart,
                              &m_memoryProperties.vaEnd) != 0)
    {
        result = Result::ErrorInvalidValue;
    }
    else
    {

        m_memoryProperties.vaInitialEnd = m_memoryProperties.vaEnd;

        // kernel reserve 8MB at the begining of VA space and expose all others, up to 64GB, to libdrm_amdgpu.so.
        // There are two VAM instance in the libdrm_amdgpu.so, one for 4GB below and the other for the remains.
        // In order to simplify the scenario, the VAM in PAL will not use 4GB below so far, thus the available Va range
        // will stick to 4GB and above
        PAL_ASSERT(m_memoryProperties.vaStart <= _4GB);

        m_memoryProperties.vaStart = _4GB;

        // libdrm_amdgpu will only report the whole continuous VA space. So there is no excluded va ranges
        // between start and end. The reserved first 4GB is at the beginning of whole VA range which is already be
        // carved out.
        m_memoryProperties.numExcludedVaRanges = 0;
        memset(&m_memoryProperties.excludedRange[0], 0, sizeof(m_memoryProperties.excludedRange));

        struct amdgpu_buffer_size_alignments sizeAlign = {};

        if (result == Result::Success)
        {
            if (m_drmProcs.pfnAmdgpuQueryBufferSizeAlignment(m_hDevice, &sizeAlign) != 0)
            {
                result = Result::ErrorInvalidValue;
            }
        }

        // Large page support
        if (result == Result::Success)
        {
            struct drm_amdgpu_info_device deviceInfo = {};
            if (m_drmProcs.pfnAmdgpuQueryInfo(m_hDevice, AMDGPU_INFO_DEV_INFO, sizeof(deviceInfo), &deviceInfo) == 0)
            {
                m_memoryProperties.largePageSupport.largePageSizeInBytes = deviceInfo.pte_fragment_size;
                // minSurfaceSize is an estimated value based on various peformance tests.
                m_memoryProperties.largePageSupport.minSurfaceSizeForAlignmentInBytes = deviceInfo.pte_fragment_size;
                m_memoryProperties.largePageSupport.gpuVaAlignmentNeeded = (deviceInfo.pte_fragment_size >= 64*1024);
                m_memoryProperties.largePageSupport.sizeAlignmentNeeded = (deviceInfo.pte_fragment_size >= 64*1024);
            }
        }

        if (result == Result::Success)
        {
            m_memoryProperties.fragmentSize = sizeAlign.size_local;

            // The libdrm_amdgpu GPU memory interfaces map very nicely to PAL's interfaces; we can simply use
            // GpuPageSize for all allocation granularities and also for virtualMemPageSize.
            m_memoryProperties.realMemAllocGranularity    = GpuPageSize;
            m_memoryProperties.virtualMemAllocGranularity = GpuPageSize;
            m_memoryProperties.virtualMemPageSize         = GpuPageSize;

            if (m_pPlatform->SvmModeEnabled() && (MemoryProperties().flags.iommuv2Support == 0))
            {
                // Calculate SVM start VA
                result = FixupUsableGpuVirtualAddressRange(m_chipProperties.gfxip.vaRangeNumBits);
            }
        }

        if (result == Result::Success)
        {
            result = VamMgrSingleton::InitVaRangesAndFinalizeVam(this);
        }

        if (result == Result::Success)
        {
            m_memoryProperties.flags.multipleVaRangeSupport  = 1;
            m_memoryProperties.flags.shadowDescVaSupport     = 1;
            m_memoryProperties.flags.virtualRemappingSupport = 1;
            m_memoryProperties.flags.pinningSupport          = 1; // Supported
            m_memoryProperties.flags.supportPerSubmitMemRefs = 1; // Supported
            m_memoryProperties.flags.globalGpuVaSupport      = 0; // Not supported
            m_memoryProperties.flags.svmSupport              = 1; // Supported
            m_memoryProperties.flags.autoPrioritySupport     = 0; // Not supported

            // Linux don't support High Bandwidth Cache Controller (HBCC) memory segment
            m_memoryProperties.hbccSizeInBytes   = 0;

            if (m_drmProcs.pfnAmdgpuQueryInfo(m_hDevice, AMDGPU_INFO_MEMORY, sizeof(memInfo), &memInfo) != 0)
            {
                struct amdgpu_heap_info heap_info = {0};
                if (m_drmProcs.pfnAmdgpuQueryHeapInfo(m_hDevice,
                                                      AMDGPU_GEM_DOMAIN_VRAM,
                                                      AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED,
                                                      &heap_info) == 0)
                {
                    m_memoryProperties.localHeapSize     = heap_info.heap_size;
                }
                if (m_drmProcs.pfnAmdgpuQueryHeapInfo(m_hDevice, AMDGPU_GEM_DOMAIN_VRAM, 0, &heap_info) == 0)
                {
                    m_memoryProperties.invisibleHeapSize = heap_info.heap_size;
                }
                if (m_drmProcs.pfnAmdgpuQueryHeapInfo(m_hDevice, AMDGPU_GEM_DOMAIN_GTT, 0, &heap_info) == 0)
                {
                    m_memoryProperties.nonLocalHeapSize  = heap_info.heap_size;
                }
            }
            else
            {
                m_memoryProperties.localHeapSize     = memInfo.cpu_accessible_vram.total_heap_size;
                m_memoryProperties.invisibleHeapSize = memInfo.vram.total_heap_size - m_memoryProperties.localHeapSize;
                m_memoryProperties.nonLocalHeapSize  = Pow2AlignDown(memInfo.gtt.total_heap_size,
                                                       m_memoryProperties.fragmentSize);
            }

            SystemInfo systemInfo = {};
            if (QuerySystemInfo(&systemInfo) == Result::Success)
            {
                // On the platform with VRAM bigger than system memory, kernel driver would return an incorrect
                // GTT heap size, which is bigger than system memory. So, workaround it before kernel has a fix.
                gpusize totalSysMemSize = static_cast<gpusize>(systemInfo.totalSysMemSize) * 1024 * 1024;
                m_memoryProperties.nonLocalHeapSize = Min(totalSysMemSize, m_memoryProperties.nonLocalHeapSize);
            }

            drm_amdgpu_capability cap = {};
            if (m_drmProcs.pfnAmdgpuQueryInfo(m_hDevice, AMDGPU_INFO_CAPABILITY, sizeof(cap), &cap) == 0)
            {
                // Report DGMA memory if available
                if (cap.flag & AMDGPU_CAPABILITY_DIRECT_GMA_FLAG)
                {
                    m_memoryProperties.busAddressableMemSize = cap.direct_gma_size * 1024 * 1024;
                }
            }
        }

        if (result == Result::Success)
        {
            result = m_reservedVaMap.Init();
        }
    }

    if (result == Result::Success)
    {

        for (uint32 i = 0; i < EngineTypeCount; ++i)
        {
            auto*const                   pEngineInfo = &m_engineProperties.perEngine[i];
            struct drm_amdgpu_info_hw_ip engineInfo  = {};

            switch (static_cast<EngineType>(i))
            {
            case EngineTypeUniversal:
                if (m_chipProperties.gfxLevel != GfxIpLevel::None)
                {
                    if (m_drmProcs.pfnAmdgpuQueryHwIpInfo(m_hDevice, AMDGPU_HW_IP_GFX, 0, &engineInfo) != 0)
                    {
                        result = Result::ErrorInvalidValue;
                    }
                    pEngineInfo->numAvailable      = Util::CountSetBits(engineInfo.available_rings);
                    pEngineInfo->startAlign        = engineInfo.ib_start_alignment;
                    pEngineInfo->sizeAlignInDwords = engineInfo.ib_size_alignment;
                }
                break;

            case EngineTypeCompute:
                if (m_chipProperties.gfxLevel != GfxIpLevel::None)
                {
                    if (m_drmProcs.pfnAmdgpuQueryHwIpInfo(m_hDevice, AMDGPU_HW_IP_COMPUTE, 0, &engineInfo) != 0)
                    {
                        result = Result::ErrorInvalidValue;
                    }
                    pEngineInfo->numAvailable      = Util::CountSetBits(engineInfo.available_rings);
                    pEngineInfo->startAlign        = engineInfo.ib_start_alignment;
                    pEngineInfo->sizeAlignInDwords = engineInfo.ib_size_alignment;
                }
                break;

            case EngineTypeExclusiveCompute:
                // NOTE: amdgpu doesn't support the ExclusiveCompute Queue.
                pEngineInfo->numAvailable = 0;
                pEngineInfo->startAlign = 8;
                pEngineInfo->sizeAlignInDwords = 1;
                break;

            case EngineTypeDma:
                if ((m_chipProperties.ossLevel != OssIpLevel::None)
                    )
                {
                    if (m_drmProcs.pfnAmdgpuQueryHwIpInfo(m_hDevice, AMDGPU_HW_IP_DMA, 0, &engineInfo) != 0)
                    {
                        result = Result::ErrorInvalidValue;
                    }
                    pEngineInfo->numAvailable      = Util::CountSetBits(engineInfo.available_rings);
                    pEngineInfo->startAlign        = engineInfo.ib_start_alignment;
                    pEngineInfo->sizeAlignInDwords = engineInfo.ib_size_alignment;
                }
                break;

            case EngineTypeTimer:
                // NOTE: amdgpu doesn't support the Timer Queue.
                pEngineInfo->numAvailable      = 0;
                pEngineInfo->startAlign        = 8;
                pEngineInfo->sizeAlignInDwords = 1;
                break;

            case EngineTypeHighPriorityUniversal:
            case EngineTypeHighPriorityGraphics:
                // not supported on linux
                pEngineInfo->numAvailable       = 0;
                pEngineInfo->startAlign         = 1;
                pEngineInfo->sizeAlignInDwords  = 1;
                break;
            default:
                PAL_ASSERT_ALWAYS();
                break;
            }
        }
    }

    if (result == Result::Success)
    {
        // For DRI3, the client cannot control the present mode (blit or flip), the Xserver that decides it internally.
        // Additionally the present is not executed by a queue because Xserver presents the image. So universal queue,
        // compute queue and DMA queue all support presents.
        m_supportsPresent[QueueTypeUniversal] = true;
        m_supportsPresent[QueueTypeCompute]   = true;
        m_supportsPresent[QueueTypeDma]       = true;

        // For now we don't support any direct presents. The client must use swap chain presents.
        for (uint32 idx = 0; idx < QueueTypeCount; ++idx)
        {
            if (m_supportsPresent[idx])
            {
                m_queueProperties.perQueue[idx].flags.supportsSwapChainPresents = 1;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Correlates a current GPU timsetamp with the CPU clock, allowing tighter CPU/GPU synchronization using timestamps.
// NOTE: This operation is currently not supported on Linux.
Result Device::CalibrateGpuTimestamp(
    GpuTimestampCalibration* pCalibrationData
    ) const
{
    uint64 gpuTimestamp = 0;
    Result result = Result::ErrorUnavailable;

    if (m_drmProcs.pfnAmdgpuQueryInfo(m_hDevice, AMDGPU_INFO_TIMESTAMP, sizeof(gpuTimestamp), &gpuTimestamp) == 0)
    {
        // the cpu timestamp is measured in ticks
        pCalibrationData->cpuWinPerfCounter = GetPerfCpuTime();
        pCalibrationData->gpuTimestamp = gpuTimestamp;
        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Specifies how many frames can be placed in the presentation queue.  This limits how many frames the CPU can get in
// front of the GPU.
//
// NOTE: This operation is currently not supported on Linux.
Result Device::SetMaxQueuedFrames(
    uint32 maxFrames)
{
    return Result::ErrorUnavailable;
}

// =====================================================================================================================
// Compares this physical GPU against another GPU object to determine how compatible they are for multi-GPU operations.
// NOTE: Part of the IDevice public interface.
Result Device::GetMultiGpuCompatibility(
    const IDevice&        otherDevice,
    GpuCompatibilityInfo* pInfo
    ) const
{
    Result result = Result::ErrorInvalidPointer;

    if (pInfo != nullptr)
    {
        // NOTE: Presently, PAL does not support multi-GPU on the amdgpu driver.
        pInfo->flags.u32All = 0;

        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Reads a setting from a configuration file.
bool Device::ReadSetting(
    const char*          pSettingName,
    Util::ValueType      valueType,
    void*                pValue,
    InternalSettingScope settingType,
    size_t               bufferSz
    ) const
{
    return m_settingsMgr.GetValue(pSettingName, valueType, pValue, bufferSz);
}

// =====================================================================================================================
// Allocates GDS for individual engines.
Result Device::AllocateGds(
    const DeviceGdsAllocInfo&   requested,
    DeviceGdsAllocInfo*         pAllocated)
{
    // TODO: implement it once amdgpu is ready.
    PAL_NOT_IMPLEMENTED();
    Result result = Result::ErrorUnavailable;

    return result;
}

// =====================================================================================================================
size_t Device::GpuMemoryObjectSize() const
{
    return sizeof(GpuMemory);
}

// =====================================================================================================================
Pal::GpuMemory* Device::ConstructGpuMemoryObject(
    void* pPlacementAddr)
{
    return PAL_PLACEMENT_NEW(pPlacementAddr) GpuMemory(this);
}

// =====================================================================================================================
// Determines the size of Pal::Linux::Queue, in bytes.
size_t Device::QueueObjectSize(
    const QueueCreateInfo& createInfo
    ) const
{
    size_t size = 0;

    switch (createInfo.queueType)
    {
    case QueueTypeCompute:
    case QueueTypeUniversal:
    case QueueTypeDma:
        // Add the size of m_pResourceList
        size = sizeof(Queue) + CmdBufMemReferenceLimit * sizeof(amdgpu_bo_handle);
        break;
    case QueueTypeTimer:
        // Timer Queue is not supported so far.
        PAL_NOT_IMPLEMENTED();
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return size;
}

// =====================================================================================================================
// Constructs a new Queue object in preallocated memory
Pal::Queue* Device::ConstructQueueObject(
    const QueueCreateInfo& createInfo,
    void*                  pPlacementAddr)
{
    Pal::Queue* pQueue = nullptr;

    switch (createInfo.queueType)
    {
    case QueueTypeCompute:
    case QueueTypeUniversal:
    case QueueTypeDma:
        pQueue = PAL_PLACEMENT_NEW(pPlacementAddr) Queue(this, createInfo);
        break;
    case QueueTypeTimer:
        // Timer Queue is not supported so far.
        PAL_NOT_IMPLEMENTED();
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return pQueue;
}

// =====================================================================================================================
// Determine the size of presentable image, in bytes.
void Device::GetPresentableImageSizes(
    const PresentableImageCreateInfo& createInfo,
    size_t*                           pImageSize,
    size_t*                           pGpuMemorySize,
    Result*                           pResult
    ) const
{
    Image::GetImageSizes((*this), createInfo, pImageSize, pGpuMemorySize, pResult);
}

// =====================================================================================================================
// Create an image which is presentable.
Result Device::CreatePresentableImage(
    const PresentableImageCreateInfo& createInfo,
    void*                             pImagePlacementAddr,
    void*                             pGpuMemoryPlacementAddr,
    Pal::IImage**                     ppImage,
    Pal::IGpuMemory**                 ppGpuMemory)
{
    return Image::CreatePresentableImage(this,
                                         createInfo,
                                         pImagePlacementAddr,
                                         pGpuMemoryPlacementAddr,
                                         ppImage,
                                         ppGpuMemory);
}

// =====================================================================================================================
// Determines the size in bytes of a Image object.
size_t Device::GetImageSize(
    const ImageCreateInfo& createInfo,
    Result*                pResult
    ) const
{
    if (pResult != nullptr)
    {
        constexpr ImageInternalCreateInfo NullInternalInfo = {};
        *pResult = Image::ValidateCreateInfo(this, createInfo, NullInternalInfo);
    }
#if PAL_ENABLE_PRINTS_ASSERTS
    else
    {
        constexpr ImageInternalCreateInfo NullInternalInfo = {};
        PAL_ASSERT(Image::ValidateCreateInfo(this, createInfo, NullInternalInfo) == Result::Success);
    }
#endif

    size_t size = (sizeof(Image) + Pal::Image::GetTotalSubresourceSize(*this, createInfo));

    if (m_pGfxDevice != nullptr)
    {
        size += m_pGfxDevice->GetImageSize(createInfo);
    }

    return size;
}

// =====================================================================================================================
// Creates and initializes a new Image object.
Result Device::CreateImage(
    const ImageCreateInfo& createInfo,
    void*                  pPlacementAddr,
    IImage**               ppImage)
{
    Pal::Image* pImage = nullptr;

    constexpr ImageInternalCreateInfo internalInfo = {};

    Result ret = CreateInternalImage(createInfo, internalInfo, pPlacementAddr, &pImage);
    if (ret == Result::Success)
    {
        (*ppImage) = pImage;
    }
    return ret;
}

// =====================================================================================================================
// Creates and initializes a new Image object.
Result Device::CreateInternalImage(
    const ImageCreateInfo&         createInfo,
    const ImageInternalCreateInfo& internalCreateInfo,
    void*                          pPlacementAddr,
    Pal::Image**                   ppImage)
{
    // NOTE: MGPU is not yet supported on Linux.
    PAL_ASSERT(internalCreateInfo.pOriginalImage == nullptr);

    (*ppImage) = PAL_PLACEMENT_NEW(pPlacementAddr) Image(this, createInfo, internalCreateInfo);

    Result result = (*ppImage)->Init();
    if (result != Result::Success)
    {
        (*ppImage)->Destroy();
        *ppImage = nullptr;
    }

    return result;
}

// =====================================================================================================================
// Swap chain information is related with OS window system, so get all of the information here.
Result Device::GetSwapChainInfo(
    OsDisplayHandle      hDisplay,
    OsWindowHandle       hWindow,
    WsiPlatform          wsiPlatform,
    SwapChainProperties* pSwapChainProperties)
{
    // Get current windows size (height, width) from window system.
    Result result = WindowSystem::GetWindowGeometry(this,
                                                    wsiPlatform,
                                                    hDisplay,
                                                    hWindow,
                                                    &pSwapChainProperties->currentExtent);

    if (result == Result::Success)
    {
        // In Vulkan spec, currentExtent is the current width and height of the surface, or the special value
        // (0xFFFFFFFF, 0xFFFFFFFF) indicating that the surface size will be determined by the extent of a swapchain
        // targeting the surface.
        if (pSwapChainProperties->currentExtent.width == UINT32_MAX)
        {
            const auto&  imageProperties = ChipProperties().imageProperties;

            // Allow any supported image size.
            pSwapChainProperties->minImageExtent.width  = 1;
            pSwapChainProperties->minImageExtent.height = 1;
            pSwapChainProperties->maxImageExtent.width  = imageProperties.maxImageDimension.width;
            pSwapChainProperties->maxImageExtent.height = imageProperties.maxImageDimension.height;
        }
        else
        {
            // Don't support presentation scaling.
            pSwapChainProperties->maxImageExtent.width  = pSwapChainProperties->currentExtent.width;
            pSwapChainProperties->maxImageExtent.height = pSwapChainProperties->currentExtent.height;
            pSwapChainProperties->minImageExtent.width  = pSwapChainProperties->currentExtent.width;
            pSwapChainProperties->minImageExtent.height = pSwapChainProperties->currentExtent.height;
        }

        pSwapChainProperties->minImageCount = 2;    // This is frequently, how many images must be in a swap chain in
                                                    // order for App to acquire an image in finite time if App currently
                                                    // doesn't own an image.

        // A swap chain must contain at most this many images. The only limits for maximum number of image count are
        // related with the amount of memory available, but here 16 should be enough for client.
        pSwapChainProperties->maxImageCount = MaxSwapChainLength;

        pSwapChainProperties->supportedTransforms = SurfaceTransformNone; // Don't support transform so far.
        pSwapChainProperties->currentTransforms   = SurfaceTransformNone; // Don't support transform so far.
        pSwapChainProperties->maxImageArraySize   = 1;                    // Don't support stereo so far.

        pSwapChainProperties->supportedUsageFlags.u32All        = 0;
        pSwapChainProperties->supportedUsageFlags.colorTarget   = 1;
        pSwapChainProperties->supportedUsageFlags.shaderRead    = 1;
        pSwapChainProperties->supportedUsageFlags.shaderWrite   = 1;

        // Get format supported by swap chain.
        pSwapChainProperties->imageFormatCount = sizeof(PresentableImageFormats)/sizeof(PresentableImageFormats[0]);
        for (uint32 i = 0; i < pSwapChainProperties->imageFormatCount; i++)
        {
            pSwapChainProperties->imageFormat[i] = PresentableImageFormats[i];
        }
    }

    return result;
}

// =====================================================================================================================
Result Device::DeterminePresentationSupported(
    OsDisplayHandle      hDisplay,
    Pal::WsiPlatform     wsiPlatform,
    int64                visualId)
{
    return WindowSystem::DeterminePresentationSupported(this, hDisplay, wsiPlatform, visualId);
}

// =====================================================================================================================
size_t Device::GetSwapChainSize(
    const SwapChainCreateInfo& createInfo,
    Result*                    pResult
    ) const
{
    // We do no swap chain create info validation for now.
    if (pResult != nullptr)
    {
        *pResult = Result::Success;
    }

    return SwapChain::GetSize(createInfo, *this);
}

// =====================================================================================================================
Result Device::CreateSwapChain(
    const SwapChainCreateInfo& createInfo,
    void*                      pPlacementAddr,
    ISwapChain**               ppSwapChain)
{
    return SwapChain::Create(createInfo, this, pPlacementAddr, ppSwapChain);
}

// =====================================================================================================================
// Call amdgpu to allocate buffer object.
Result Device::AllocBuffer(
    struct amdgpu_bo_alloc_request* pAllocRequest,
    amdgpu_bo_handle*               pBufferHandle
    ) const
{
    Result result = Result::Success;
    result = CheckResult(m_drmProcs.pfnAmdgpuBoAlloc(m_hDevice, pAllocRequest, pBufferHandle),
                         Result::ErrorOutOfGpuMemory);
    return result;
}

// =====================================================================================================================
// Call amdgpu to free buffer object.
Result Device::FreeBuffer(
    amdgpu_bo_handle hBuffer
    ) const
{
    Result result = Result::Success;
    result = CheckResult(m_drmProcs.pfnAmdgpuBoFree(hBuffer),
                         Result::ErrorInvalidValue);
    return result;
}

// =====================================================================================================================
// Converts a PAL MType into an amdgpu MTYPE constant.
static uint64 ConvertMType(
    MType mtype)
{
    constexpr uint64 MTypeTable[] =
    {
        AMDGPU_VM_MTYPE_DEFAULT, // Default
        AMDGPU_VM_MTYPE_NC,      // CachedNoncoherent
        AMDGPU_VM_MTYPE_CC,      // CachedCoherent
        AMDGPU_VM_MTYPE_UC,      // Uncached
    };

    static_assert(sizeof(MTypeTable) / sizeof(MTypeTable[0]) == static_cast<uint32>(MType::Count),
                  "The MTypeTable needs to be updated.");

    PAL_ASSERT(static_cast<uint32>(mtype) < static_cast<uint32>(MType::Count));

    return MTypeTable[static_cast<uint32>(mtype)];
}

// =====================================================================================================================
// Call amdgpu to map the virtual gpu address to part of the BO, which range is from offset to offset + size.
Result Device::MapVirtualAddress(
    amdgpu_bo_handle hBuffer,
    uint64           offset,
    uint64           size,
    uint64           virtualAddress,
    MType            mtype
    ) const
{
    Result result = Result::Success;

    constexpr uint64 operations = AMDGPU_VM_PAGE_READABLE | AMDGPU_VM_PAGE_WRITEABLE | AMDGPU_VM_PAGE_EXECUTABLE;
    const     uint64 mtypeFlag  = ConvertMType(mtype);

    // The operation flags and MTYPE flag should be mutually exclusive.
    PAL_ASSERT((operations & mtypeFlag) == 0);

    const uint64 flags = operations | mtypeFlag;
    if (m_drmProcs.pfnAmdgpuBoVaOpRawisValid())
    {
        result = CheckResult(m_drmProcs.pfnAmdgpuBoVaOpRaw(m_hDevice,
                                                           hBuffer,
                                                           offset,
                                                           size,
                                                           virtualAddress,
                                                           flags,
                                                           AMDGPU_VA_OP_MAP),
                             Result::ErrorInvalidValue);
    }
    else
    {
        result = CheckResult(m_drmProcs.pfnAmdgpuBoVaOp(hBuffer,
                                                        offset,
                                                        size,
                                                        virtualAddress,
                                                        0,
                                                        AMDGPU_VA_OP_MAP),
                             Result::ErrorInvalidValue);
    }

    return result;
}

// =====================================================================================================================
// Call amdgpu to unmap the virtual gpu address to part of the BO, which range is from offset to offset + size.
Result Device::UnmapVirtualAddress(
    amdgpu_bo_handle hBuffer,
    uint64           offset,
    uint64           size,
    uint64           virtualAddress
    ) const
{
    Result result = Result::Success;

    constexpr int64 ops = AMDGPU_VM_PAGE_READABLE | AMDGPU_VM_PAGE_WRITEABLE | AMDGPU_VM_PAGE_EXECUTABLE;
    if (m_drmProcs.pfnAmdgpuBoVaOpRawisValid())
    {
        result = CheckResult(m_drmProcs.pfnAmdgpuBoVaOpRaw(m_hDevice,
                                                           hBuffer,
                                                           offset,
                                                           size,
                                                           virtualAddress,
                                                           ops,
                                                           AMDGPU_VA_OP_UNMAP),
                             Result::ErrorInvalidValue);
    }
    else
    {
        result = CheckResult(m_drmProcs.pfnAmdgpuBoVaOp(hBuffer,
                                                        offset,
                                                        size,
                                                        virtualAddress,
                                                        0,
                                                        AMDGPU_VA_OP_UNMAP),
                             Result::ErrorInvalidValue);
    }

    return result;
}

// =====================================================================================================================
// Call amdgpu to setup PTEs for reserved PRT va space.
Result Device::ReservePrtVaRange(
    uint64  virtualAddress,
    uint64  size,
    MType   mtype
    ) const
{
    Result result = Result::ErrorUnavailable;

    constexpr uint64 operations = AMDGPU_VM_PAGE_PRT;
    const     uint64 mtypeFlag  = ConvertMType(mtype);

    // The operation flags and MTYPE flag should be mutually exclusive.
    PAL_ASSERT((operations & mtypeFlag) == 0);

    const uint64 flags = operations | mtypeFlag;

    if (m_drmProcs.pfnAmdgpuBoVaOpRawisValid())
    {
        result = CheckResult(m_drmProcs.pfnAmdgpuBoVaOpRaw(m_hDevice,
                                                           nullptr,
                                                           0,
                                                           size,
                                                           virtualAddress,
                                                           flags,
                                                           AMDGPU_VA_OP_MAP),
                             Result::ErrorInvalidValue);
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    return result;
}

// =====================================================================================================================
// Call amdgpu to discard the PTEs for reserved PRT va space.
Result Device::DiscardReservedPrtVaRange(
    uint64  virtualAddress,
    uint64  size
    ) const
{
    Result result = Result::ErrorUnavailable;
    int64 operation = AMDGPU_VM_PAGE_READABLE | AMDGPU_VM_PAGE_WRITEABLE | AMDGPU_VM_PAGE_EXECUTABLE;

    if (m_drmProcs.pfnAmdgpuBoVaOpRawisValid())
    {
        result = CheckResult(m_drmProcs.pfnAmdgpuBoVaOpRaw(m_hDevice,
                                                           nullptr,
                                                           0,
                                                           size,
                                                           virtualAddress,
                                                           operation,
                                                           AMDGPU_VA_OP_CLEAR),
                             Result::ErrorInvalidValue);
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    return result;
}

// =====================================================================================================================
// Replace the PRT mapping.
// If the hBuffer is nullptr, amdgpu will reset the PTE for the va range to the initial state with [T=1, V=0].
// If the hBuffer is valie, amdgpu will first unmap all existing va that in/overlap the requested va range, then map.
Result Device::ReplacePrtVirtualAddress(
    amdgpu_bo_handle hBuffer,
    uint64           offset,
    uint64           size,
    uint64           virtualAddress,
    MType            mtype
    ) const
{
    Result result = Result::ErrorUnavailable;

    const uint64 operations = (hBuffer != nullptr) ?
                                (AMDGPU_VM_PAGE_READABLE | AMDGPU_VM_PAGE_WRITEABLE | AMDGPU_VM_PAGE_EXECUTABLE) :
                                (AMDGPU_VM_PAGE_PRT);
    const uint64 mtypeFlag  = ConvertMType(mtype);

    // The operation flags and MTYPE flag should be mutually exclusive.
    PAL_ASSERT((operations & mtypeFlag) == 0);

    const uint64 flags = operations | mtypeFlag;
    if (m_drmProcs.pfnAmdgpuBoVaOpRawisValid())
    {
        result = CheckResult(m_drmProcs.pfnAmdgpuBoVaOpRaw(m_hDevice,
                                                           hBuffer,
                                                           offset,
                                                           size,
                                                           virtualAddress,
                                                           flags,
                                                           AMDGPU_VA_OP_REPLACE),
                             Result::ErrorInvalidValue);
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    return result;
}

// =====================================================================================================================
// Call amdgpu to export buffer object.
Result Device::ExportBuffer(
    amdgpu_bo_handle                hBuffer,
    enum amdgpu_bo_handle_type      type,
    uint32*                         pSharedHandle
    ) const
{
    Result result = Result::Success;

    if (m_drmProcs.pfnAmdgpuBoExport(hBuffer, type, pSharedHandle) != 0)
    {
        result = Result::ErrorInvalidValue;
    }

    return result;
}
// =====================================================================================================================
// Call amdgpu to import buffer object.
Result Device::ImportBuffer(
    enum amdgpu_bo_handle_type      type,
    uint32                          sharedHandle,
    struct amdgpu_bo_import_result* pOutput
    ) const
{
    Result result = Result::Success;

    if (m_drmProcs.pfnAmdgpuBoImport(m_hDevice, type, sharedHandle, pOutput) != 0)
    {
        result = Result::ErrorInvalidValue;
    }

    return result;
}

// =====================================================================================================================
// Call amdgpu to query buffer info.
Result Device::QueryBufferInfo(
    amdgpu_bo_handle        hBuffer,
    struct amdgpu_bo_info*  pInfo
    ) const
{
    Result result = Result::Success;

    if (m_drmProcs.pfnAmdgpuBoQueryInfo(hBuffer, pInfo) != 0)
    {
        result = Result::ErrorInvalidValue;
    }

    return result;
}

// =====================================================================================================================
// Call amdgpu to map a buffer object to cpu space.
Result Device::Map(
    amdgpu_bo_handle hBuffer,
    void**           ppCpu
    ) const
{
    Result result = Result::Success;

    if (m_drmProcs.pfnAmdgpuBoCpuMap(hBuffer, ppCpu) != 0)
    {
        result = Result::ErrorGpuMemoryMapFailed;
    }

    return result;
}

// =====================================================================================================================
// Call amdgpu to unmap a buffer object to cpu space.
Result Device::Unmap(
    amdgpu_bo_handle hBuffer
    ) const
{
    Result result = Result::Success;

    if (m_drmProcs.pfnAmdgpuBoCpuUnmap(hBuffer) != 0)
    {
        result = Result::ErrorGpuMemoryUnmapFailed;
    }

    return result;
}

// =====================================================================================================================
// Call amdgpu to wait a buffer object idle.
Result Device::WaitBufferIdle(
    amdgpu_bo_handle hBuffer,
    uint64           timeoutNs,
    bool*            pBufferBusy
    ) const
{
    Result result = Result::Success;

    if (m_drmProcs.pfnAmdgpuBoWaitForIdle(hBuffer, timeoutNs, pBufferBusy) != 0)
    {
        result = Result::NotReady;
    }

    return result;
}

// =====================================================================================================================
// Call amdgpu to create a command submission context.
Result Device::CreateCommandSubmissionContext(
    amdgpu_context_handle* pContextHandle,
    QueuePriority          priority
    ) const
{
    Result result = Result::Success;

    // Check if the global scheduling context isn't available and allocate a new one for each queue
    if (m_hContext == nullptr)
    {
        if (m_supportQueuePriority)
        {
            // for exisiting logic, the QueuePriority::Low refer to the default state.
            // Therefore the mapping between Pal and amdgpu should be adjusted as:
            constexpr int OsPriority[] =
            {
                AMDGPU_CTX_PRIORITY_NORMAL,     ///< QueuePriority::Low        = 0,
                AMDGPU_CTX_PRIORITY_HIGH,       ///< QueuePriority::Medium     = 1,
                AMDGPU_CTX_PRIORITY_VERY_HIGH,  ///< QueuePriority::High       = 2,
                AMDGPU_CTX_PRIORITY_LOW,        ///< QueuePriority::VeryLow    = 3,
            };

            static_assert((static_cast<uint32>(QueuePriority::Low) == 0)    &&
                          (static_cast<uint32>(QueuePriority::Medium) == 1) &&
                          (static_cast<uint32>(QueuePriority::High) == 2)   &&
                          (static_cast<uint32>(QueuePriority::VeryLow) == 3), "QueuePriority definition changed");

            if (m_drmProcs.pfnAmdgpuCsCtxCreate2(m_hDevice,
                                                 OsPriority[static_cast<uint32>(priority)],
                                                 pContextHandle) != 0)
            {
                result = Result::ErrorInvalidValue;
            }
        }
        // just ignore the priority.
        else
        {
            if (m_drmProcs.pfnAmdgpuCsCtxCreate(m_hDevice, pContextHandle) != 0)
            {
                result = Result::ErrorInvalidValue;
            }
        }
    }
    else
    {
        // Return the global scheduling context
        *pContextHandle = m_hContext;
    }

    return result;
}

// =====================================================================================================================
// Call amdgpu to destroy a command submission context.
Result Device::DestroyCommandSubmissionContext(
    amdgpu_context_handle hContext
    ) const
{
    Result result = Result::Success;

    if (m_hContext == nullptr)
    {
        if (m_drmProcs.pfnAmdgpuCsCtxFree(hContext) != 0)
        {
            result = Result::ErrorInvalidValue;
        }
    }

    return result;
}

// =====================================================================================================================
// Call amdgpu to submit the commands through amdgpu_cs_submit_raw, which requires caller to setup the cs_chunks
Result Device::SubmitRaw(
    amdgpu_context_handle           hContext,
    amdgpu_bo_list_handle           boList,
    uint32                          chunkCount,
    struct drm_amdgpu_cs_chunk *    pChunks,
    uint64*                         pFence
    ) const
{
    Result result = Result::Success;
    result = CheckResult(m_drmProcs.pfnAmdgpuCsSubmitRaw(m_hDevice, hContext, boList, chunkCount, pChunks, pFence),
                         Result::ErrorInvalidValue);

    return result;
}

// =====================================================================================================================
// Call amdgpu to submit the commands.
Result Device::Submit(
    amdgpu_context_handle     hContext,
    uint64                    flags,
    struct amdgpu_cs_request* pIbsRequest,
    uint32                    numberOfRequests,
    uint64*                   pFences
    ) const
{
    Result result = Result::Success;
    result = CheckResult(m_drmProcs.pfnAmdgpuCsSubmit(hContext, flags, pIbsRequest, numberOfRequests),
                         Result::ErrorInvalidValue);
    if (result == Result::Success)
    {
        *pFences = pIbsRequest->seq_no;
    }
    return result;
}

// =====================================================================================================================
// Determines the size in bytes of a Fence object.
size_t Device::GetFenceSize(
    Result*    pResult
    ) const
{
    size_t fenceSize = 0;

    if (pResult != nullptr)
    {
        (*pResult) = Result::Success;
    }

    if (GetFenceType() == FenceType::SyncObj)
    {
        fenceSize = sizeof(SyncobjFence);
    }
    else
    {
        fenceSize = sizeof(Fence);
    }

    return fenceSize;
}

// =====================================================================================================================
// Creates a new Fence object in preallocated memory provided by the caller.
Result Device::CreateFence(
    const FenceCreateInfo& createInfo,
    void*                  pPlacementAddr,
    IFence**               ppFence
    ) const
{
    Fence* pFence = nullptr;
    PAL_ASSERT((pPlacementAddr != nullptr) && (ppFence != nullptr));

    if (GetFenceType() == FenceType::SyncObj)
    {
        pFence = PAL_PLACEMENT_NEW(pPlacementAddr) SyncobjFence(*this);
    }
    else
    {
        pFence = PAL_PLACEMENT_NEW(pPlacementAddr) Fence();
    }

    // Set needsEvent argument to true - all client-created fences require event objects to support the
    // IDevice::WaitForFences interface.
    Result result = pFence->Init(createInfo, true);

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
    Fence* pFence = nullptr;
    PAL_ASSERT((pPlacementAddr != nullptr) && (ppFence != nullptr));

    if (GetFenceType() == FenceType::SyncObj)
    {
        pFence = PAL_PLACEMENT_NEW(pPlacementAddr) SyncobjFence(*this);
    }
    else
    {
        pFence = PAL_PLACEMENT_NEW(pPlacementAddr) Fence();
    }
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
// Call amdgpu to get the fence status.
Result Device::QueryFenceStatus(
    struct amdgpu_cs_fence* pFence,
    uint64                  timeoutNs
    ) const
{
    Result result  = Result::Success;
    uint32 expired = 0;
    result = CheckResult(m_drmProcs.pfnAmdgpuCsQueryFenceStatus(pFence, timeoutNs, 0, &expired),
                         Result::ErrorInvalidValue);
    if (result == Result::Success)
    {
        if (expired == 0)
        {
            result = Result::NotReady;
        }
    }
    return result;
}

// =====================================================================================================================
// Call amdgpu to wait for multiple fences
Result Device::WaitForFences(
    amdgpu_cs_fence* pFences,
    uint32           fenceCount,
    bool             waitAll,
    uint64           timeout
    ) const
{
    Result result = Result::Success;
    uint32 status = 0;
    uint32 index  = 0;
    if (m_drmProcs.pfnAmdgpuCsWaitFencesisValid())
    {
        result = CheckResult(m_drmProcs.pfnAmdgpuCsWaitFences(pFences, fenceCount, waitAll, timeout, &status, &index),
                             Result::ErrorInvalidValue);

        if (result == Result::Success)
        {
            PAL_ASSERT((status == 0) || (status == 1));
            result = (status == 0) ? Result::Timeout : Result::Success;
        }
    }
    else
    {
        for(;index < fenceCount; index++)
        {
             result = CheckResult(m_drmProcs.pfnAmdgpuCsQueryFenceStatus(&(pFences[index]), timeout, 0, &status),
                                  Result::ErrorInvalidValue);

             if (result != Result::Success)
             {
                 break;
             }
             else
             {
                 PAL_ASSERT((status == 0) || (status == 1));
                 result = (status == 0) ? Result::Timeout : Result::Success;
                 if (result != Result::Success)
                 {
                     break;
                 }
             }
        }
    }
    return result;
}

// =====================================================================================================================
// Call amdgpu to wait for multiple fences (fence based on Sync Object)
Result Device::WaitForSyncobjFences(
    uint32_t*            pFences,
    uint32               fenceCount,
    uint64               timeout,
    uint32               flags,
    uint32*              pFirstSignaled
    ) const
{
    Result result = Result::Success;

    if (m_drmProcs.pfnAmdgpuCsSyncobjWaitisValid())
    {
        result = CheckResult(m_drmProcs.pfnAmdgpuCsSyncobjWait(m_hDevice,
                                                               pFences,
                                                               fenceCount,
                                                               timeout,
                                                               flags,
                                                               pFirstSignaled),
                             Result::ErrorInvalidValue);
    }

    return result;
}

// =====================================================================================================================
// Call amdgpu to reset syncobj fences
Result Device::ResetSyncObject(
    uint32_t*            pFences,
    uint32_t             fenceCount
    ) const
{
    Result result = Result::Success;

    if (m_drmProcs.pfnAmdgpuCsSyncobjResetisValid())
    {
        result = CheckResult(m_drmProcs.pfnAmdgpuCsSyncobjReset(m_hDevice,
                                                                pFences,
                                                                fenceCount),
                             Result::ErrorInvalidValue);
    }

    return result;
}

// =====================================================================================================================
// Call amdgpu to read the value of a register.
Result Device::ReadRegisters(
    uint32  dwordOffset,
    uint32  count,
    uint32  instance,
    uint32  flags,
    uint32* pValues
    ) const
{
    Result result = Result::Success;
    if (m_drmProcs.pfnAmdgpuReadMmRegisters(m_hDevice, dwordOffset, count, instance, flags, pValues) != 0)
    {
        result = Result::ErrorInvalidValue;
    }

    return result;
}

// =====================================================================================================================
// Call amdgpu to create a list of buffer objects which are referenced by the commands submit.
Result Device::CreateResourceList(
    uint32                 numberOfResources,
    amdgpu_bo_handle*      pResources,
    uint8*                 pResourcePriorities,
    amdgpu_bo_list_handle* pListHandle
    ) const
{
    Result result = Result::Success;

    const int listCreateRetVal = m_drmProcs.pfnAmdgpuBoListCreate(m_hDevice,
                                                                  numberOfResources,
                                                                  pResources,
                                                                  pResourcePriorities,
                                                                  pListHandle);

    if (listCreateRetVal != 0)
    {
        result = Result::ErrorOutOfGpuMemory;
    }

    return result;
}

// =====================================================================================================================
// Call amdgpu to destroy a bo list.
Result Device::DestroyResourceList(
    amdgpu_bo_list_handle handle
    ) const
{
    Result result = Result::Success;

    if (m_drmProcs.pfnAmdgpuBoListDestroy(handle) != 0)
    {
        result = Result::ErrorInvalidValue;
    }

    return result;
}

// =====================================================================================================================
// Check if the GPU presentFd point to is same with the devices'. The caller must ensure the presentDeviceFd is valid.
// Every GPU has three device node on Linux. card0 is a super node which require authentication and it can be used to do
// anything, including buffer management, KMS (kernel mode setting), rendering. controlD64 is used for KMS access only.
// renderD128 is used for rendering, and authentication is not required. X Server is usually open the card0, and PAL
// open the renderD128. So need to get the bus ID and check if they are same. If X server opens the render node too,
// compare the node name to check if they are the same one.
Result Device::IsSameGpu(
    int32 presentDeviceFd,
    bool* pIsSame
    ) const
{
    Result result = Result::Success;

    *pIsSame = false;

    // both the render node and master node can use this interface to get the device name.
    const char* pDeviceName = m_drmProcs.pfnDrmGetRenderDeviceNameFromFd(presentDeviceFd);

    if (pDeviceName == nullptr)
    {
        result = Result::ErrorUnknown;
    }

    if (result == Result::Success)
    {
        *pIsSame = (strcasecmp(&m_renderNodeName[0], pDeviceName) == 0);
    }

    return result;
}

// =====================================================================================================================
// convert the surface format from PAL definition to AMDGPU definition.
static AMDGPU_PIXEL_FORMAT PalToAmdGpuFormatConversion(
    const SwizzledFormat format)
{
    // we don't support types of format other than R8G8B8A8 or B8G8R8A8 so far.
    return AMDGPU_PIXEL_FORMAT__8_8_8_8;
}

// =====================================================================================================================
static uint32 AmdGpuToPalTileModeConversion(
    AMDGPU_TILE_MODE tileMode)
{
    uint32 palTileMode = ADDR_TM_LINEAR_GENERAL;
    switch (tileMode)
    {
    case AMDGPU_TILE_MODE__LINEAR_GENERAL:
        palTileMode = ADDR_TM_LINEAR_GENERAL;
        break;
    case AMDGPU_TILE_MODE__LINEAR_ALIGNED:
        palTileMode = ADDR_TM_LINEAR_ALIGNED;
        break;
    case AMDGPU_TILE_MODE__1D_TILED_THIN1:
        palTileMode = ADDR_TM_1D_TILED_THIN1;
        break;
    case AMDGPU_TILE_MODE__1D_TILED_THICK:
        palTileMode = ADDR_TM_1D_TILED_THICK;
        break;
    case AMDGPU_TILE_MODE__2D_TILED_THIN1:
        palTileMode = ADDR_TM_2D_TILED_THIN1;
        break;
    case AMDGPU_TILE_MODE__2D_TILED_THIN2:
        palTileMode = ADDR_TM_2D_TILED_THIN2;
        break;
    case AMDGPU_TILE_MODE__2D_TILED_THIN4:
        palTileMode = ADDR_TM_2D_TILED_THIN4;
        break;
    case AMDGPU_TILE_MODE__2D_TILED_THICK:
        palTileMode = ADDR_TM_2D_TILED_THICK;
        break;
    case AMDGPU_TILE_MODE__2B_TILED_THIN1:
        palTileMode = ADDR_TM_2B_TILED_THIN1;
        break;
    case AMDGPU_TILE_MODE__2B_TILED_THIN2:
        palTileMode = ADDR_TM_2B_TILED_THIN2;
        break;
    case AMDGPU_TILE_MODE__2B_TILED_THIN4:
        palTileMode = ADDR_TM_2B_TILED_THIN4;
        break;
    case AMDGPU_TILE_MODE__2B_TILED_THICK:
        palTileMode = ADDR_TM_2B_TILED_THICK;
        break;
    case AMDGPU_TILE_MODE__3D_TILED_THIN1:
        palTileMode = ADDR_TM_3D_TILED_THIN1;
        break;
    case AMDGPU_TILE_MODE__3D_TILED_THICK:
        palTileMode = ADDR_TM_3D_TILED_THICK;
        break;
    case AMDGPU_TILE_MODE__3B_TILED_THIN1:
        palTileMode = ADDR_TM_3B_TILED_THIN1;
        break;
    case AMDGPU_TILE_MODE__3B_TILED_THICK:
        palTileMode = ADDR_TM_3B_TILED_THICK;
        break;
    case AMDGPU_TILE_MODE__2D_TILED_XTHICK:
        palTileMode = ADDR_TM_2D_TILED_XTHICK;
        break;
    case AMDGPU_TILE_MODE__3D_TILED_XTHICK:
        palTileMode = ADDR_TM_3D_TILED_XTHICK;
        break;
    case AMDGPU_TILE_MODE__PRT_TILED_THIN1:
        palTileMode = ADDR_TM_PRT_TILED_THIN1;
        break;
    case AMDGPU_TILE_MODE__PRT_2D_TILED_THIN1:
        palTileMode = ADDR_TM_PRT_2D_TILED_THIN1;
        break;
    case AMDGPU_TILE_MODE__PRT_3D_TILED_THIN1:
        palTileMode = ADDR_TM_PRT_3D_TILED_THIN1;
        break;
    case AMDGPU_TILE_MODE__PRT_TILED_THICK:
        palTileMode = ADDR_TM_PRT_TILED_THICK;
        break;
    case AMDGPU_TILE_MODE__PRT_2D_TILED_THICK:
        palTileMode = ADDR_TM_PRT_2D_TILED_THICK;
        break;
    case AMDGPU_TILE_MODE__PRT_3D_TILED_THICK:
        palTileMode = ADDR_TM_PRT_3D_TILED_THICK;
        break;
    default:
        palTileMode = ADDR_TM_LINEAR_GENERAL;
        break;
    }

    return palTileMode;
}

// =====================================================================================================================
// convert the tiling mode from PAL definition to AMDGPU definition.
static AMDGPU_TILE_MODE PalToAmdGpuTileModeConversion(
    uint32 tileMode)
{
    constexpr AMDGPU_TILE_MODE TileModes[] =
    {
        AMDGPU_TILE_MODE__LINEAR_GENERAL,       //ADDR_TM_LINEAR_GENERAL      = 0,
        AMDGPU_TILE_MODE__LINEAR_ALIGNED,       //ADDR_TM_LINEAR_ALIGNED      = 1,
        AMDGPU_TILE_MODE__1D_TILED_THIN1,       //ADDR_TM_1D_TILED_THIN1      = 2,
        AMDGPU_TILE_MODE__1D_TILED_THICK,       //ADDR_TM_1D_TILED_THICK      = 3,
        AMDGPU_TILE_MODE__2D_TILED_THIN1,       //ADDR_TM_2D_TILED_THIN1      = 4,
        AMDGPU_TILE_MODE__2D_TILED_THIN2,       //ADDR_TM_2D_TILED_THIN2      = 5,
        AMDGPU_TILE_MODE__2D_TILED_THIN4,       //ADDR_TM_2D_TILED_THIN4      = 6,
        AMDGPU_TILE_MODE__2D_TILED_THICK,       //ADDR_TM_2D_TILED_THICK      = 7,
        AMDGPU_TILE_MODE__2B_TILED_THIN1,       //ADDR_TM_2B_TILED_THIN1      = 8,
        AMDGPU_TILE_MODE__2B_TILED_THIN2,       //ADDR_TM_2B_TILED_THIN2      = 9,
        AMDGPU_TILE_MODE__2B_TILED_THIN4,       //ADDR_TM_2B_TILED_THIN4      = 10,
        AMDGPU_TILE_MODE__2B_TILED_THICK,       //ADDR_TM_2B_TILED_THICK      = 11,
        AMDGPU_TILE_MODE__3D_TILED_THIN1,       //ADDR_TM_3D_TILED_THIN1      = 12,
        AMDGPU_TILE_MODE__3D_TILED_THICK,       //ADDR_TM_3D_TILED_THICK      = 13,
        AMDGPU_TILE_MODE__3B_TILED_THIN1,       //ADDR_TM_3B_TILED_THIN1      = 14,
        AMDGPU_TILE_MODE__3B_TILED_THICK,       //ADDR_TM_3B_TILED_THICK      = 15,
        AMDGPU_TILE_MODE__2D_TILED_XTHICK,      //ADDR_TM_2D_TILED_XTHICK     = 16,
        AMDGPU_TILE_MODE__3D_TILED_XTHICK,      //ADDR_TM_3D_TILED_XTHICK     = 17,
        AMDGPU_TILE_MODE__INVALID,              //ADDR_TM_POWER_SAVE          = 18,
        AMDGPU_TILE_MODE__PRT_TILED_THIN1,      //ADDR_TM_PRT_TILED_THIN1     = 19,
        AMDGPU_TILE_MODE__PRT_2D_TILED_THIN1,   //ADDR_TM_PRT_2D_TILED_THIN1  = 20,
        AMDGPU_TILE_MODE__PRT_3D_TILED_THIN1,   //ADDR_TM_PRT_3D_TILED_THIN1  = 21,
        AMDGPU_TILE_MODE__PRT_TILED_THICK,      //ADDR_TM_PRT_TILED_THICK     = 22,
        AMDGPU_TILE_MODE__PRT_2D_TILED_THICK,   //ADDR_TM_PRT_2D_TILED_THICK  = 23,
        AMDGPU_TILE_MODE__PRT_3D_TILED_THICK,   //ADDR_TM_PRT_3D_TILED_THICK  = 24,
        AMDGPU_TILE_MODE__INVALID,              //ADDR_TM_COUNT               = 25,
    };
    return TileModes[tileMode];
}

// =====================================================================================================================
static uint32 AmdGpuToPalTileTypeConversion(
    AMDGPU_MICRO_TILE_MODE tileType)
{
    uint32 palTileType = ADDR_NON_DISPLAYABLE;
    switch (tileType)
    {
    case AMDGPU_MICRO_TILE_MODE__DISPLAYABLE:
        palTileType = ADDR_DISPLAYABLE;
        break;
    case AMDGPU_MICRO_TILE_MODE__NON_DISPLAYABLE:
        palTileType = ADDR_NON_DISPLAYABLE;
        break;
    case AMDGPU_MICRO_TILE_MODE__DEPTH_SAMPLE_ORDER:
        palTileType = ADDR_DEPTH_SAMPLE_ORDER;
        break;
    case AMDGPU_MICRO_TILE_MODE__ROTATED:
        palTileType = ADDR_ROTATED;
        break;
    case AMDGPU_MICRO_TILE_MODE__THICK:
        palTileType = ADDR_THICK;
        break;
    default:
        palTileType = ADDR_NON_DISPLAYABLE;
        break;
    };

    return palTileType;
}

// =====================================================================================================================
// convert the micro tile mode from PAL definition to AMDGPU definition.
static AMDGPU_MICRO_TILE_MODE PalToAmdGpuTileTypeConversion(
    const uint32 tileType)
{
    constexpr AMDGPU_MICRO_TILE_MODE TileTypes[] =
    {
        AMDGPU_MICRO_TILE_MODE__DISPLAYABLE,        //ADDR_DISPLAYABLE        = 0,
        AMDGPU_MICRO_TILE_MODE__NON_DISPLAYABLE,    //ADDR_NON_DISPLAYABLE    = 1,
        AMDGPU_MICRO_TILE_MODE__DEPTH_SAMPLE_ORDER, //ADDR_DEPTH_SAMPLE_ORDER = 2,
        AMDGPU_MICRO_TILE_MODE__ROTATED,            //ADDR_ROTATED            = 3,
        AMDGPU_MICRO_TILE_MODE__THICK,              //ADDR_THICK              = 4,
    };
    return TileTypes[tileType];
}

// =====================================================================================================================
static uint32 AmdGpuToPalPipeConfigConversion(
    AMDGPU_PIPE_CFG pipeConfig)
{
    uint32 palPipeConfig = ADDR_SURF_P2;
    switch (pipeConfig)
    {
        case AMDGPU_PIPE_CFG__P2:
            palPipeConfig = ADDR_SURF_P2;
            break;
        case AMDGPU_PIPE_CFG__P4_8x16:
            palPipeConfig = ADDR_SURF_P4_8x16;
            break;
        case AMDGPU_PIPE_CFG__P4_16x16:
            palPipeConfig = ADDR_SURF_P4_16x16;
            break;
        case AMDGPU_PIPE_CFG__P4_16x32:
            palPipeConfig = ADDR_SURF_P4_16x32;
            break;
        case AMDGPU_PIPE_CFG__P4_32x32:
            palPipeConfig = ADDR_SURF_P4_32x32;
            break;
        case AMDGPU_PIPE_CFG__P8_16x16_8x16:
            palPipeConfig = ADDR_SURF_P8_16x16_8x16;
            break;
        case AMDGPU_PIPE_CFG__P8_16x32_8x16:
            palPipeConfig = ADDR_SURF_P8_16x32_8x16;
            break;
        case AMDGPU_PIPE_CFG__P8_32x32_8x16:
            palPipeConfig = ADDR_SURF_P8_32x32_8x16;
            break;
        case AMDGPU_PIPE_CFG__P8_16x32_16x16:
            palPipeConfig = ADDR_SURF_P8_16x32_16x16;
            break;
        case AMDGPU_PIPE_CFG__P8_32x32_16x16:
            palPipeConfig = ADDR_SURF_P8_32x32_16x16;
            break;
        case AMDGPU_PIPE_CFG__P8_32x32_16x32:
            palPipeConfig = ADDR_SURF_P8_32x32_16x32;
            break;
        case AMDGPU_PIPE_CFG__P8_32x64_32x32:
            palPipeConfig = ADDR_SURF_P8_32x64_32x32;
            break;
        case AMDGPU_PIPE_CFG__P16_32x32_8x16:
#if PAL_BUILD_GFX9
            palPipeConfig = ADDR_SURF_P16_32x32_8x16;
#else
            palPipeConfig = ADDR_SURF_P16_32x32_8x16__CI__VI;
#endif
            break;
        case AMDGPU_PIPE_CFG__P16_32x32_16x16:
#if PAL_BUILD_GFX9
            palPipeConfig = ADDR_SURF_P16_32x32_16x16;
#else
            palPipeConfig = ADDR_SURF_P16_32x32_16x16__CI__VI;
#endif
            break;
        default:
            palPipeConfig = ADDR_SURF_P2;
            break;
    }
    return palPipeConfig;
}

// =====================================================================================================================
// convert the pipe config from PAL definition to AMDGPU definition.
static AMDGPU_PIPE_CFG PalToAmdGpuPipeConfigConversion(
    const uint32 pipeConfig)
{
    constexpr AMDGPU_PIPE_CFG PipeConfigs[] =
    {
            AMDGPU_PIPE_CFG__P2,                    //ADDR_SURF_P2                             = 0x00000000,
            AMDGPU_PIPE_CFG__INVALID,               //ADDR_SURF_P2_RESERVED0                   = 0x00000001,
            AMDGPU_PIPE_CFG__INVALID,               //ADDR_SURF_P2_RESERVED1                   = 0x00000002,
            AMDGPU_PIPE_CFG__INVALID,               //ADDR_SURF_P2_RESERVED2                   = 0x00000003,
            AMDGPU_PIPE_CFG__P4_8x16,               //ADDR_SURF_P4_8x16                        = 0x00000004,
            AMDGPU_PIPE_CFG__P4_16x16,              //ADDR_SURF_P4_16x16                       = 0x00000005,
            AMDGPU_PIPE_CFG__P4_16x32,              //ADDR_SURF_P4_16x32                       = 0x00000006,
            AMDGPU_PIPE_CFG__P4_32x32,              //ADDR_SURF_P4_32x32                       = 0x00000007,
            AMDGPU_PIPE_CFG__P8_16x16_8x16,         //ADDR_SURF_P8_16x16_8x16                  = 0x00000008,
            AMDGPU_PIPE_CFG__P8_16x32_8x16,         //ADDR_SURF_P8_16x32_8x16                  = 0x00000009,
            AMDGPU_PIPE_CFG__P8_32x32_8x16,         //ADDR_SURF_P8_32x32_8x16                  = 0x0000000a,
            AMDGPU_PIPE_CFG__P8_16x32_16x16,        //ADDR_SURF_P8_16x32_16x16                 = 0x0000000b,
            AMDGPU_PIPE_CFG__P8_32x32_16x16,        //ADDR_SURF_P8_32x32_16x16                 = 0x0000000c,
            AMDGPU_PIPE_CFG__P8_32x32_16x32,        //ADDR_SURF_P8_32x32_16x32                 = 0x0000000d,
            AMDGPU_PIPE_CFG__P8_32x64_32x32,        //ADDR_SURF_P8_32x64_32x32                 = 0x0000000e,
            AMDGPU_PIPE_CFG__INVALID,               //ADDR_SURF_P8_RESERVED0__CI__VI           = 0x0000000f,
            AMDGPU_PIPE_CFG__P16_32x32_8x16,        //ADDR_SURF_P16_32x32_8x16__CI__VI         = 0x00000010,
            AMDGPU_PIPE_CFG__P16_32x32_16x16,       //ADDR_SURF_P16_32x32_16x16__CI__VI        = 0x00000011,
    };
    return PipeConfigs[pipeConfig];
}

// this union is used to map the level one metadata definition used by mesa radeon driver.
union AmdGpuTilingFlags
{
    struct
    {
        uint64 arrayMode       : AMDGPU_TILING_PIPE_CONFIG_SHIFT;
        uint64 pipeConfig      : AMDGPU_TILING_TILE_SPLIT_SHIFT - AMDGPU_TILING_PIPE_CONFIG_SHIFT;
        uint64 tileSplit       : AMDGPU_TILING_MICRO_TILE_MODE_SHIFT - AMDGPU_TILING_TILE_SPLIT_SHIFT;
        uint64 microTileMode   : AMDGPU_TILING_BANK_WIDTH_SHIFT - AMDGPU_TILING_MICRO_TILE_MODE_SHIFT;
        uint64 bankWidth       : AMDGPU_TILING_BANK_HEIGHT_SHIFT - AMDGPU_TILING_BANK_WIDTH_SHIFT;
        uint64 bankHeight      : AMDGPU_TILING_MACRO_TILE_ASPECT_SHIFT - AMDGPU_TILING_BANK_HEIGHT_SHIFT;
        uint64 macroTileAspect : AMDGPU_TILING_NUM_BANKS_SHIFT - AMDGPU_TILING_MACRO_TILE_ASPECT_SHIFT;
        uint64 numBanks        : 2;  // mask is 3 which means 2 bit.
        uint64 reserved        : 41; // AMDGPU_TILING_NUM_BANKS_SHIFT is 21 plus 2 bit equals 23.
    };
    uint64  u64All;
};

// =====================================================================================================================
// Update Image's tiling information from metadata.
void Device::UpdateImageInfo(
    amdgpu_bo_handle    hBuffer,
    Image*              pImage)
{
    struct amdgpu_bo_info info = {};
    SubResourceInfo* const pSubResInfo = pImage->GetSubresourceInfo(0);

    if ((m_drmProcs.pfnAmdgpuBoQueryInfo(hBuffer, &info) == 0) &&
        (info.metadata.size_metadata >= PRO_UMD_METADATA_SIZE))
    {
        if (ChipProperties().gfxLevel < GfxIpLevel::GfxIp9)
        {
            AddrMgr1::TileInfo*const pTileInfo = static_cast<AddrMgr1::TileInfo*>(pImage->GetSubresourceTileInfo(0));
            auto*const pUmdMetaData = reinterpret_cast<amdgpu_bo_umd_metadata*>
                                      (&info.metadata.umd_metadata[PRO_UMD_METADATA_OFFSET_DWORD]);
            auto*const pSubResInfo  = pImage->GetSubresourceInfo(0);

            pSubResInfo->extentTexels.width         = pUmdMetaData->width_in_pixels;
            pSubResInfo->extentTexels.height        = pUmdMetaData->height;
            pSubResInfo->rowPitch                   = pUmdMetaData->aligned_pitch_in_bytes;
            pSubResInfo->actualExtentTexels.height  = pUmdMetaData->aligned_height;
            pTileInfo->tileIndex                    = pUmdMetaData->tile_index;
            pTileInfo->tileMode                     = AmdGpuToPalTileModeConversion(pUmdMetaData->tile_mode);
            pTileInfo->tileType                     = AmdGpuToPalTileTypeConversion(pUmdMetaData->micro_tile_mode);

            pTileInfo->pipeConfig                   = AmdGpuToPalPipeConfigConversion(
                                                                pUmdMetaData->tile_config.pipe_config);

            pTileInfo->banks                        = pUmdMetaData->tile_config.banks;
            pTileInfo->bankWidth                    = pUmdMetaData->tile_config.bank_width;
            pTileInfo->bankHeight                   = pUmdMetaData->tile_config.bank_height;
            pTileInfo->macroAspectRatio             = pUmdMetaData->tile_config.macro_aspect_ratio;
            pTileInfo->tileSplitBytes               = pUmdMetaData->tile_config.tile_split_bytes;
            pTileInfo->tileSwizzle                  = pUmdMetaData->pipeBankXor;
        }
#if PAL_BUILD_GFX9
        else if (ChipProperties().gfxLevel == GfxIpLevel::GfxIp9)
        {
            AddrMgr2::TileInfo*const pTileInfo   = static_cast<AddrMgr2::TileInfo*>(pImage->GetSubresourceTileInfo(0));
            auto*const pUmdMetaData = reinterpret_cast<amdgpu_bo_umd_metadata*>
                                      (&info.metadata.umd_metadata[PRO_UMD_METADATA_OFFSET_DWORD]);
            pTileInfo->pipeBankXor = pUmdMetaData->pipeBankXor;
        }
#endif
        else
        {
            PAL_NOT_IMPLEMENTED();
        }
    }
}

// =====================================================================================================================
// Sync vulkan buffer/image info for external usage such as Xserver consume Vulkan's render output.
Result Device::UpdateExternalImageInfo(
    const PresentableImageCreateInfo&  createInfo,
    Pal::GpuMemory*                    pGpuMemory,
    Pal::Image*                        pImage)
{
    return Image::UpdateExternalImageInfo(this, createInfo, pGpuMemory, pImage);
}

// =====================================================================================================================
// Create Presentable Memory Object. Parameter sharedHandle is only useful for Android, discard it for Linux case.
Result Device::CreatePresentableMemoryObject(
    Image*           pImage,
    void*            pMemObjMem,
    OsDisplayHandle  sharedHandle,
    Pal::GpuMemory** ppMemObjOut)
{
    return Image::CreatePresentableMemoryObject(this, pImage, pMemObjMem, ppMemObjOut);
}

// =====================================================================================================================
const char* Device::GetCacheFilePath() const
{
    const char* pPath = getenv("AMD_SHADER_DISK_CACHE_PATH");
    if (pPath == nullptr)
    {
        pPath = getenv("HOME");
    }
    return pPath;
}

// =====================================================================================================================
// Update the metadata, including the tiling mode, pixel format, pitch, aligned height, to metadata associated
// with the memory object.  The consumer of the memory object will get the metadata when importing it and view the image
// in the exactly the same way.
void Device::UpdateMetaData(
    amdgpu_bo_handle    hBuffer,
    const Image&        image)
{
    amdgpu_bo_metadata metadata = {};
    const SubResourceInfo*const    pSubResInfo = image.SubresourceInfo(0);

    // First 32 dwords are reserved for open source components.
    auto*const pUmdMetaData = reinterpret_cast<amdgpu_bo_umd_metadata*>
                              (&metadata.umd_metadata[PRO_UMD_METADATA_OFFSET_DWORD]);

    if (ChipProperties().gfxLevel < GfxIpLevel::GfxIp9)
    {
        metadata.tiling_info   = AMDGPU_TILE_MODE__2D_TILED_THIN1;
        metadata.size_metadata = PRO_UMD_METADATA_SIZE;

        const SubResourceInfo*const    pSubResInfo = image.SubresourceInfo(0);
        const AddrMgr1::TileInfo*const pTileInfo   = AddrMgr1::GetTileInfo(&image, 0);

        memset(&metadata.umd_metadata[0], 0, PRO_UMD_METADATA_OFFSET_DWORD * sizeof(metadata.umd_metadata[0]));
        pUmdMetaData->width_in_pixels        = pSubResInfo->extentTexels.width;
        pUmdMetaData->height                 = pSubResInfo->extentTexels.height;
        pUmdMetaData->aligned_pitch_in_bytes = pSubResInfo->rowPitch;
        pUmdMetaData->aligned_height         = pSubResInfo->actualExtentTexels.height;
        pUmdMetaData->tile_index             = pTileInfo->tileIndex;
        pUmdMetaData->format                 = PalToAmdGpuFormatConversion(pSubResInfo->format);
        pUmdMetaData->tile_mode              = PalToAmdGpuTileModeConversion(pTileInfo->tileMode);
        pUmdMetaData->micro_tile_mode        = PalToAmdGpuTileTypeConversion(pTileInfo->tileType);

        pUmdMetaData->pipeBankXor            = pTileInfo->tileSwizzle;

        pUmdMetaData->tile_config.pipe_config        = PalToAmdGpuPipeConfigConversion(pTileInfo->pipeConfig);
        pUmdMetaData->tile_config.banks              = pTileInfo->banks;
        pUmdMetaData->tile_config.bank_width         = pTileInfo->bankWidth;
        pUmdMetaData->tile_config.bank_height        = pTileInfo->bankHeight;
        pUmdMetaData->tile_config.macro_aspect_ratio = pTileInfo->macroAspectRatio;
        pUmdMetaData->tile_config.tile_split_bytes   = pTileInfo->tileSplitBytes;

        // set the tiling_info according to mesa's definition.
        AmdGpuTilingFlags tilingFlags;
        // the tilingFlags uses ADDRLIB definition but not AMDGPU.
        tilingFlags.u64All          = 0;
        tilingFlags.arrayMode       = pTileInfo->tileMode;
        tilingFlags.pipeConfig      = pTileInfo->pipeConfig;
        tilingFlags.tileSplit       = pTileInfo->tileSplitBytes;
        tilingFlags.bankWidth       = pTileInfo->bankWidth;
        tilingFlags.bankHeight      = pTileInfo->bankHeight;
        tilingFlags.macroTileAspect = pTileInfo->macroAspectRatio;
        tilingFlags.numBanks        = pTileInfo->banks;

        // in order to sharing resource metadata with Mesa3D, the definition have to follow Mesa's way.
        // the micro tile mode is used in Mesa to indicate whether the surface is displyable.
        // it is bool typed, 0 for displayable and 1 for not displayable in current version.
        // forcing it to be 0 for presentable image,
        tilingFlags.microTileMode   = 0;

        metadata.tiling_info = tilingFlags.u64All;
    }
#if PAL_BUILD_GFX9
    else if (ChipProperties().gfxLevel == GfxIpLevel::GfxIp9)
    {
        const SubResourceInfo*const    pSubResInfo = image.SubresourceInfo(0);
        const AddrMgr2::TileInfo*const pTileInfo   = AddrMgr2::GetTileInfo(&image, 0);

        PAL_ASSERT(static_cast<uint32>(AMDGPU_SWIZZLE_MODE_MAX_TYPE)
            == static_cast<uint32>(ADDR_SW_MAX_TYPE));

        PAL_ASSERT(static_cast<uint32>(AMDGPU_ADDR_RSRC_TEX_2D) == static_cast<uint32>(ADDR_RSRC_TEX_2D));

        AMDGPU_SWIZZLE_MODE curSwizzleMode   =
            static_cast<AMDGPU_SWIZZLE_MODE>(image.GetGfxImage()->GetSwTileMode(pSubResInfo));

        metadata.swizzle_info   = curSwizzleMode;
        metadata.size_metadata  = PRO_UMD_METADATA_SIZE;

        memset(&metadata.umd_metadata[0], 0, PRO_UMD_METADATA_OFFSET_DWORD * sizeof(metadata.umd_metadata[0]));
        pUmdMetaData->width_in_pixels        = pSubResInfo->extentTexels.width;
        pUmdMetaData->height                 = pSubResInfo->extentTexels.height;
        pUmdMetaData->aligned_pitch_in_bytes = pSubResInfo->rowPitch;
        pUmdMetaData->aligned_height         = pSubResInfo->actualExtentTexels.height;
        pUmdMetaData->format                 = PalToAmdGpuFormatConversion(pSubResInfo->format);

        pUmdMetaData->pipeBankXor  = pTileInfo->pipeBankXor;
        pUmdMetaData->swizzleMode  = curSwizzleMode;
        pUmdMetaData->resourceType = AMDGPU_ADDR_RSRC_TEX_2D;
    }
#endif
    else
    {
        PAL_NOT_IMPLEMENTED();
    }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 367
    pUmdMetaData->flags.optimal_shareable = image.GetImageCreateInfo().flags.optimalShareable;

    if (pUmdMetaData->flags.optimal_shareable)
    {
        //analysis the shared metadata if surface is optimal shareable
        SharedMetadataInfo sharedMetadataInfo = {};
        image.GetGfxImage()->GetSharedMetadataInfo(&sharedMetadataInfo);

        auto*const pUmdSharedMetadata = reinterpret_cast<amdgpu_shared_metadata_info*>
                                            (&pUmdMetaData->shared_metadata_info);
        pUmdSharedMetadata->dcc_offset   = sharedMetadataInfo.dccOffset;
        pUmdSharedMetadata->cmask_offset = sharedMetadataInfo.cmaskOffset;
        pUmdSharedMetadata->fmask_offset = sharedMetadataInfo.fmaskOffset;
        pUmdSharedMetadata->htile_offset = sharedMetadataInfo.htileOffset;

        pUmdSharedMetadata->flags.shader_fetchable =
            sharedMetadataInfo.flags.shaderFetchable;
        pUmdSharedMetadata->flags.shader_fetchable_fmask =
            sharedMetadataInfo.flags.shaderFetchableFmask;
        pUmdSharedMetadata->flags.has_wa_tc_compat_z_range =
            sharedMetadataInfo.flags.hasWaTcCompatZRange;
        pUmdSharedMetadata->flags.has_eq_gpu_access =
            sharedMetadataInfo.flags.hasEqGpuAccess;
        pUmdSharedMetadata->flags.has_htile_lookup_table =
            sharedMetadataInfo.flags.hasHtileLookupTable;

        pUmdSharedMetadata->dcc_state_offset =
            sharedMetadataInfo.dccStateMetaDataOffset;
        pUmdSharedMetadata->fast_clear_value_offset =
            sharedMetadataInfo.fastClearMetaDataOffset;
        pUmdSharedMetadata->fce_state_offset =
            sharedMetadataInfo.fastClearEliminateMetaDataOffset;

        if (sharedMetadataInfo.fmaskOffset != 0)
        {
            //if the shared surface is a color surface, reuse the htileOffset as fmaskXor.
            PAL_ASSERT(sharedMetadataInfo.htileOffset == 0);
            pUmdSharedMetadata->flags.htile_as_fmask_xor = 1;
            pUmdSharedMetadata->htile_offset = sharedMetadataInfo.fmaskXor;
        }

        if (sharedMetadataInfo.flags.hasHtileLookupTable)
        {
            PAL_ASSERT(sharedMetadataInfo.dccStateMetaDataOffset == 0);
            pUmdSharedMetadata->htile_lookup_table_offset =
                sharedMetadataInfo.htileLookupTableOffset;
        }
        //linux don't need use this value to pass extra information for now
        pUmdSharedMetadata->resource_id = 0;
    }
#endif

    m_drmProcs.pfnAmdgpuBoSetMetadata(hBuffer, &metadata);
}

// =====================================================================================================================
// For SyncObject feature: We check Platform's feature by judging whether the libdrm's API is valid or not. But there
// is no way to guarantee the corresponding kernel ioctl is correctly support. We already meet broken kernel image
// (4.13) with only partial sync object ioctl's implementation while libdrm (2.4.89) have all wrapper functions.
// To confirm Sync Object's real support status, we will invoke some important ioctls to double confirm and update
// the status in Device::m_syncobjSupportState.
void Device::CheckSyncObjectSupportStatus()
{
    Result status = Result::Success;
    bool isDrmCapWithSyncobj = false;
    uint64_t supported = 0;
    Platform* pLnxPlatform = static_cast<Platform*>(m_pPlatform);

    m_syncobjSupportState.flags = 0;

    if (m_drmProcs.pfnDrmGetCap(m_fileDescriptor, DRM_CAP_SYNCOBJ, &supported) == 0)
    {
        isDrmCapWithSyncobj = (supported == 1);
    }

    if (isDrmCapWithSyncobj && (pLnxPlatform->IsSyncObjectSupported()))
    {
        amdgpu_syncobj_handle hSyncobj = 0;

        // Check Basic SyncObject's support with create and destroy api.
        status = CreateSyncObject(0, &hSyncobj);
        if (status == Result::Success)
        {
            status = DestroySyncObject(hSyncobj);
        }
        m_syncobjSupportState.SyncobjSemaphore = (status == Result::Success);

        // Check CreateSignaledSyncObject support with DRM_SYNCOBJ_CREATE_SIGNALED flags
        // Dependent on Basic SyncObject's support
        if ((pLnxPlatform->IsCreateSignaledSyncObjectSupported()) &&
            (m_syncobjSupportState.SyncobjSemaphore == 1))
        {
            status = CreateSyncObject(DRM_SYNCOBJ_CREATE_SIGNALED, &hSyncobj);
            m_syncobjSupportState.InitialSignaledSyncobjSemaphore = (status == Result::Success);

            // Check SyncobjFence needed SyncObject api with wait/reset interface
            // Dependent on CreateSignaledSyncObject support; Will just wait on this initial Signaled Syncobj.
            if ((pLnxPlatform->IsSyncobjFenceSupported()) &&
                (m_syncobjSupportState.InitialSignaledSyncobjSemaphore == 1))
            {
                if (status == Result::Success)
                {
                    uint32 count = 1;
                    uint64 timeout = 0;
                    uint32 flags = DRM_SYNCOBJ_WAIT_FLAGS_WAIT_ALL;
                    uint32 firstSignaledFence = UINT32_MAX;

                    status = WaitForSyncobjFences(&hSyncobj,
                                                  count,
                                                  timeout,
                                                  flags,
                                                  &firstSignaledFence);
                    if (status == Result::Success)
                    {
                        status = ResetSyncObject(&hSyncobj, 1);
                    }
                    DestroySyncObject(hSyncobj);
                    m_syncobjSupportState.SyncobjFence = (status == Result::Success);
                }
            }
        }
    }
}

// =====================================================================================================================
Result Device::SyncObjImportSyncFile(
    int                     syncFileFd,
    amdgpu_syncobj_handle   syncObj
    ) const
{
    int32 ret = 0;
    ret = m_drmProcs.pfnAmdgpuCsSyncobjImportSyncFile(m_hDevice,
                                                      syncObj,
                                                      syncFileFd);
    return CheckResult(ret, Result::ErrorUnknown);
}

// =====================================================================================================================
Result Device::SyncObjExportSyncFile(
    amdgpu_syncobj_handle   syncObj,
    int*                    pSyncFileFd
    ) const
{
    int32 ret = 0;
    ret = m_drmProcs.pfnAmdgpuCsSyncobjExportSyncFile(m_hDevice,
                                                      syncObj,
                                                      pSyncFileFd);
    return CheckResult(ret, Result::ErrorUnknown);
}

// =====================================================================================================================
Result Device::ConveySyncObjectState(
    amdgpu_syncobj_handle importSyncObj,
    amdgpu_syncobj_handle exportSyncObj
    ) const
{
    // In current kernel driver, the ioctl to transfer fence state is not implemented.
    // I have to use two IOCTLs to emulate the transfer operation.
    // It still runs into problem, since we cannot guarantee the fence is still valid when we call the export, since
    // the fence would be null-ed if signaled.
    int32 syncFileFd = 0;
    int32 ret = m_drmProcs.pfnAmdgpuCsSyncobjExportSyncFile(m_hDevice,
                                                            exportSyncObj,
                                                            &syncFileFd);
    if (ret == 0)
    {
        ret = m_drmProcs.pfnAmdgpuCsSyncobjImportSyncFile(m_hDevice,
                                                          importSyncObj,
                                                          syncFileFd);
        close(syncFileFd);
    }

    return CheckResult(ret, Result::ErrorUnknown);
}

// =====================================================================================================================
Result Device::CreateSyncObject(
    uint32                    flags,
    amdgpu_syncobj_handle*    pSyncObject
    ) const
{
    amdgpu_syncobj_handle syncObject = 0;
    Result result = Result::ErrorUnavailable;

    if (m_drmProcs.pfnAmdgpuCsCreateSyncobj2isValid())
    {
        result = CheckResult(m_drmProcs.pfnAmdgpuCsCreateSyncobj2(m_hDevice, flags, &syncObject),
                             Result::ErrorUnknown);
    }
    else if (m_drmProcs.pfnAmdgpuCsCreateSyncobjisValid())
    {
        result = CheckResult(m_drmProcs.pfnAmdgpuCsCreateSyncobj(m_hDevice, &syncObject),
                             Result::ErrorUnknown);
    }

    if (result == Result::Success)
    {
        *pSyncObject = syncObject;
    }

    return result;
}

// =====================================================================================================================
Result Device::DestroySyncObject(
    amdgpu_syncobj_handle    syncObject
    ) const
{
    return CheckResult(m_drmProcs.pfnAmdgpuCsDestroySyncobj(m_hDevice, syncObject), Result::ErrorUnknown);
}

// =====================================================================================================================
OsExternalHandle Device::ExportSyncObject(
    amdgpu_syncobj_handle    syncObject
    ) const
{
    OsExternalHandle handle;
    if (m_drmProcs.pfnAmdgpuCsExportSyncobj(m_hDevice, syncObject, reinterpret_cast<int32*>(&handle)) != 0)
    {
        handle = -1;
    }

    return handle;
}

// =====================================================================================================================
Result Device::ImportSyncObject(
    OsExternalHandle          fd,
    amdgpu_syncobj_handle*    pSyncObject
    ) const
{
    Result result = CheckResult(m_drmProcs.pfnAmdgpuCsImportSyncobj(m_hDevice, fd, pSyncObject), Result::ErrorUnknown);
    if (result == Result::Success)
    {
        // it is up to driver to close the imported fd.
        close(fd);
    }

    return result;
}

// =====================================================================================================================
Result Device::CreateSemaphore(
    bool                     isCreatedSignaled,
    amdgpu_semaphore_handle* pSemaphoreHandle
    ) const
{
    Result                  result;
    amdgpu_sem_handle       hSem       = 0;
    amdgpu_semaphore_handle hSemaphore = nullptr;

    if (m_semType == SemaphoreType::ProOnly)
    {
        result = CheckResult(m_drmProcs.pfnAmdgpuCsCreateSem(m_hDevice, &hSem), Result::ErrorUnknown);
        if (result == Result::Success)
        {
            *pSemaphoreHandle = reinterpret_cast<amdgpu_semaphore_handle>(hSem);
        }
    }
    else if (m_semType == SemaphoreType::SyncObj)
    {
        uint32 flags = isCreatedSignaled ? DRM_SYNCOBJ_CREATE_SIGNALED : 0;

        result = CreateSyncObject(flags, &hSem);
        if (result == Result::Success)
        {
            *pSemaphoreHandle = reinterpret_cast<amdgpu_semaphore_handle>(hSem);
        }
    }
    else
    {
        result = CheckResult(m_drmProcs.pfnAmdgpuCsCreateSemaphore(&hSemaphore), Result::ErrorUnknown);
        if (result == Result::Success)
        {
            *pSemaphoreHandle = hSemaphore;
        }
    }

    return result;
}

// =====================================================================================================================
Result Device::DestroySemaphore(
    amdgpu_semaphore_handle hSemaphore
    ) const
{
    Result result;
    if (m_semType == SemaphoreType::ProOnly)
    {
        result = CheckResult(m_drmProcs.pfnAmdgpuCsDestroySem(m_hDevice, reinterpret_cast<uintptr_t>(hSemaphore)),
                             Result::ErrorUnknown);
    }
    else if (m_semType == SemaphoreType::SyncObj)
    {
        result = DestroySyncObject(reinterpret_cast<uintptr_t>(hSemaphore));
    }
    else
    {
        result = CheckResult(m_drmProcs.pfnAmdgpuCsDestroySemaphore(hSemaphore),
                             Result::ErrorUnknown);
    }

    return result;
}

// =====================================================================================================================
Result Device::WaitSemaphore(
    amdgpu_context_handle   hContext,
    uint32                  ipType,
    uint32                  ipInstance,
    uint32                  ring,
    amdgpu_semaphore_handle hSemaphore
    ) const
{
    Result result = Result::Success;
    if (m_semType == SemaphoreType::ProOnly)
    {
        result = CheckResult(m_drmProcs.pfnAmdgpuCsWaitSem(m_hDevice,
                                                           hContext,
                                                           ipType,
                                                           ipInstance,
                                                           ring,
                                                           reinterpret_cast<uintptr_t>(hSemaphore)),
                             Result::ErrorUnknown);
    }
    else if (m_semType == SemaphoreType::SyncObj)
    {
        PAL_NEVER_CALLED();
    }
    else
    {
        result = CheckResult(m_drmProcs.pfnAmdgpuCsWaitSemaphore(hContext,
                                                                 ipType,
                                                                 ipInstance,
                                                                 ring,
                                                                 hSemaphore),
                             Result::ErrorUnknown);
    }

    return result;
}

// =====================================================================================================================
Result Device::SignalSemaphore(
    amdgpu_context_handle   hContext,
    uint32                  ipType,
    uint32                  ipInstance,
    uint32                  ring,
    amdgpu_semaphore_handle hSemaphore
    ) const
{
    Result result = Result::Success;
    if (m_semType == SemaphoreType::ProOnly)
    {
        result = CheckResult(m_drmProcs.pfnAmdgpuCsSignalSem(m_hDevice,
                                                             hContext,
                                                             ipType,
                                                             ipInstance,
                                                             ring,
                                                             reinterpret_cast<uintptr_t>(hSemaphore)),
                             Result::ErrorUnknown);
    }
    else if (m_semType == SemaphoreType::SyncObj)
    {
        PAL_NEVER_CALLED();
        result = Result::ErrorUnknown;
    }
    else
    {
        result = CheckResult(m_drmProcs.pfnAmdgpuCsSignalSemaphore(hContext,
                                                                   ipType,
                                                                   ipInstance,
                                                                   ring,
                                                                   hSemaphore),
                             Result::ErrorUnknown);
    }

    return result;
}

// =====================================================================================================================
OsExternalHandle  Device::ExportSemaphore(
    amdgpu_semaphore_handle hSemaphore,
    bool                    isReference
    ) const
{
    OsExternalHandle handle;
    if (m_semType == SemaphoreType::ProOnly)
    {
        if (m_drmProcs.pfnAmdgpuCsExportSem(m_hDevice,
                                        reinterpret_cast<uintptr_t>(hSemaphore),
                                        reinterpret_cast<int32*>(&handle)) != 0)
        {
            handle = -1;
        }
    }
    else if (m_semType == SemaphoreType::SyncObj)
    {
        if (isReference)
        {
            handle = ExportSyncObject(reinterpret_cast<uintptr_t>(hSemaphore));
        }
        else
        {
            SyncObjExportSyncFile(reinterpret_cast<uintptr_t>(hSemaphore), reinterpret_cast<int32*>(&handle));
        }
    }
    else
    {
        handle = -1;
    }

    return handle;
}

// =====================================================================================================================
Result Device::ImportSemaphore(
    OsExternalHandle         fd,
    amdgpu_semaphore_handle* pSemaphoreHandle,
    bool                     isReference
    ) const
{
    Result result = Result::Success;
    amdgpu_sem_handle hSem = 0;

    if (m_semType == SemaphoreType::ProOnly)
    {
        result = CheckResult(m_drmProcs.pfnAmdgpuCsImportSem(m_hDevice, fd, &hSem), Result::ErrorUnknown);
        if (result == Result::Success)
        {
            // the ownershipo of fd has been transfered to driver.
            // driver need to close the fd if importing successfully otherwise there is a resource leak.
            close(fd);
            *pSemaphoreHandle = reinterpret_cast<amdgpu_semaphore_handle>(hSem);
        }
    }
    else if (m_semType == SemaphoreType::SyncObj)
    {
        if (isReference)
        {
            result = ImportSyncObject(fd, &hSem);
        }
        else
        {
            result = CreateSyncObject(0, &hSem);
            if (result == Result::Success)
            {
                result = SyncObjImportSyncFile(fd, hSem);
            }
            if (result == Result::Success)
            {
                close(fd);
            }
        }
        if (result == Result::Success)
        {
            *pSemaphoreHandle = reinterpret_cast<amdgpu_semaphore_handle>(hSem);
        }
    }
    else
    {
        result = Result::Unsupported;
    }

    return result;
}

// =====================================================================================================================
// Adds GPU memory objects to this device's global memory list and populate the changes to all its queues
Result Device::AddGpuMemoryReferences(
    uint32              gpuMemRefCount,
    const GpuMemoryRef* pGpuMemoryRefs,
    IQueue*             pQueue,
    uint32              flags)
{
    Result result = Result::Success;

    if (pQueue == nullptr)
    {
        {
            // Queue-list operations need to be protected.
            MutexAuto lock(&m_queueLock);

            for (auto iter = m_queues.Begin(); iter.IsValid(); iter.Next())
            {
                Queue*const pLinuxQueue = static_cast<Queue*>(iter.Get());
                result = pLinuxQueue->AddGpuMemoryReferences(gpuMemRefCount, pGpuMemoryRefs);
            }
        }

        AddToGlobalList(gpuMemRefCount, pGpuMemoryRefs);
    }
    else
    {
        Queue* pLinuxQueue = static_cast<Queue*>(pQueue);
        result = pLinuxQueue->AddGpuMemoryReferences(gpuMemRefCount, pGpuMemoryRefs);
    }

    return result;
}

// =====================================================================================================================
// Removes GPU memory objects from this device's global memory list and populate the change to all its queues
Result Device::RemoveGpuMemoryReferences(
    uint32            gpuMemoryCount,
    IGpuMemory*const* ppGpuMemory,
    IQueue*           pQueue)
{
    if (pQueue == nullptr)
    {
        {
            // Queue-list operations need to be protected.
            MutexAuto lock(&m_queueLock);

            for (auto iter = m_queues.Begin(); iter.IsValid(); iter.Next())
            {
                Queue*const pLinuxQueue = static_cast<Queue*>(iter.Get());
                pLinuxQueue->RemoveGpuMemoryReferences(gpuMemoryCount, ppGpuMemory);
            }
        }

        RemoveFromGlobalList(gpuMemoryCount, ppGpuMemory);
    }
    else
    {
        Queue* pLinuxQueue = static_cast<Queue*>(pQueue);
        pLinuxQueue->RemoveGpuMemoryReferences(gpuMemoryCount, ppGpuMemory);
    }

    return Result::Success;
}

// =====================================================================================================================
// Adds GPU memory objects to this device's global memory list
void Device::AddToGlobalList(
    uint32              gpuMemRefCount,
    const GpuMemoryRef* pGpuMemoryRefs)
{
    MutexAuto lock(&m_globalRefLock);
    Result ret = Result::Success;
    for (uint32 i = 0; i < gpuMemRefCount; i++)
    {
        IGpuMemory* pGpuMemory = pGpuMemoryRefs[i].pGpuMemory;
        bool alreadyExists = false;
        uint32* pRefCount = nullptr;

        ret = m_globalRefMap.FindAllocate(pGpuMemory, &alreadyExists, &pRefCount);
        if (ret != Result::Success)
        {
            // Not enough room or some other error, so just abort
            PAL_ASSERT_ALWAYS();
            break;
        }
        else
        {
            PAL_ASSERT(pRefCount != nullptr);
            if (alreadyExists)
            {
                ++(*pRefCount);
            }
            else
            {
                (*pRefCount) = 1;
            }
        }
    }
}

// =====================================================================================================================
// Removes GPU memory objects from this device's global memory list
void Device::RemoveFromGlobalList(
    uint32            gpuMemoryCount,
    IGpuMemory*const* ppGpuMemory)
{
    MutexAuto lock(&m_globalRefLock);
    for (uint32 i = 0; i < gpuMemoryCount; i++)
    {
        IGpuMemory* pGpuMemory = ppGpuMemory[i];
        uint32* pRefCount = m_globalRefMap.FindKey(pGpuMemory);
        if (pRefCount != nullptr)
        {
            PAL_ALERT(*pRefCount <= 0);
            if (--(*pRefCount) == 0)
            {
                m_globalRefMap.Erase(pGpuMemory);
            }
        }
    }
}

// =====================================================================================================================
// On a queue's creation, we need to add it to the list of tracked queues for this device.
Result Device::AddQueue(
    Pal::Queue* pQueue)
{
    // This function, AddGpuMemoryReferences, and RemoveGpuMemoryReferences all assume we don't support timer queues.
    PAL_ASSERT(pQueue->Type() != QueueTypeTimer);

    // Call the parent function first.
    Result result = Pal::Device::AddQueue(pQueue);

    uint32        numEntries  = 0;
    GpuMemoryRef* pMemRefList = nullptr;

    if (result == Result::Success)
    {
        MutexAuto lock(&m_globalRefLock);

        // Then update the new queue with the list of memory already added to this device.
        numEntries = m_globalRefMap.GetNumEntries();

        if (numEntries > 0)
        {
            pMemRefList = PAL_NEW_ARRAY(GpuMemoryRef, numEntries, m_pPlatform, SystemAllocType::AllocInternalTemp);

            if (pMemRefList == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
            else
            {
                auto iter = m_globalRefMap.Begin();
                for (uint32 i = 0; i < numEntries; i++, iter.Next())
                {
                    pMemRefList[i].flags.u32All = 0;
                    pMemRefList[i].pGpuMemory   = iter.Get()->key;
                }
            }
        }
    }

    if (result == Result::Success)
    {
        result = static_cast<Queue*>(pQueue)->AddGpuMemoryReferences(numEntries, pMemRefList);
    }

    PAL_SAFE_DELETE_ARRAY(pMemRefList, m_pPlatform);

    return result;
}

// =====================================================================================================================
// Assign virtual address for the allocation.
Result Device::AssignVirtualAddress(
    Pal::GpuMemory*     pGpuMemory,
    gpusize*            pGpuVirtAddr) // [in/out] In: Zero, or the desired VA. Out: The assigned VA.
{
    Result ret        = Result::ErrorOutOfGpuMemory;
    const auto vaPart = ChooseVaPartition(pGpuMemory->VirtAddrRange());

    if (vaPart == VaPartition::Default)
    {
        const auto&      memoryDesc    = pGpuMemory->Desc();
        gpusize          baseAllocated = 0;
        amdgpu_va_handle hVaRange      = nullptr;
        ret = CheckResult(m_drmProcs.pfnAmdgpuVaRangeAlloc(m_hDevice,
                                             amdgpu_gpu_va_range_general,
                                             memoryDesc.size,
                                             memoryDesc.alignment,
                                             *pGpuVirtAddr,
                                             &baseAllocated,
                                             &hVaRange,
                                             0),
                          Result::ErrorUnknown);
        if (ret == Result::Success)
        {
            // If the caller had a particular VA in mind we should make sure amdgpu gave it to us.
            PAL_ASSERT((*pGpuVirtAddr == 0) || (*pGpuVirtAddr == baseAllocated));

            *pGpuVirtAddr = baseAllocated;
            static_cast<GpuMemory*>(pGpuMemory)->SetVaRangeHandle(hVaRange);
        }
    }
    else if ((vaPart == VaPartition::DescriptorTable) ||
             (vaPart == VaPartition::ShadowDescriptorTable))
    {
        VirtAddrAssignInfo vaInfo = { };
        vaInfo.size      = pGpuMemory->Desc().size;
        vaInfo.alignment = pGpuMemory->Desc().alignment;
        vaInfo.range     = pGpuMemory->VirtAddrRange();

        ret = VamMgrSingleton::AssignVirtualAddress(this, vaInfo, pGpuVirtAddr);
        static_cast<GpuMemory*>(pGpuMemory)->SetVaRangeHandle(nullptr);
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    return ret;
}

// =====================================================================================================================
// Free virtual address for the allocation.
void Device::FreeVirtualAddress(
    Pal::GpuMemory*     pGpuMemory)
{
    GpuMemory* pMemory = static_cast<GpuMemory*>(pGpuMemory);
    const auto vaPart  = ChooseVaPartition(pGpuMemory->VirtAddrRange());
    if (vaPart == VaPartition::Default)
    {
        PAL_ASSERT(pMemory->VaRangeHandle() != nullptr);
        m_drmProcs.pfnAmdgpuVaRangeFree(pMemory->VaRangeHandle());
    }
    else if ((vaPart == VaPartition::DescriptorTable) ||
             (vaPart == VaPartition::ShadowDescriptorTable))
    {
        PAL_ASSERT(pMemory->VaRangeHandle() == nullptr);
        VamMgrSingleton::FreeVirtualAddress(this, *pGpuMemory);
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }
    pMemory->SetVaRangeHandle(nullptr);
}

// =====================================================================================================================
// Reserve gpu va range.
Result Device::ReserveGpuVirtualAddress(
    VaRange                 vaRange,
    gpusize                 baseVirtAddr,
    gpusize                 size,
    bool                    isVirtual,
    VirtualGpuMemAccessMode virtualAccessMode,
    gpusize*                pGpuVirtAddr)
{
    Result result = Result::Success;

    // On Linux, these ranges are reserved by VamMgrSingleton
    if ((vaRange != VaRange::Svm) &&
        (vaRange != VaRange::DescriptorTable) &&
        (vaRange != VaRange::ShadowDescriptorTable))
    {
        ReservedVaRangeInfo* pInfo = m_reservedVaMap.FindKey(baseVirtAddr);

        if (pInfo == nullptr)
        {
            ReservedVaRangeInfo info = {};
            gpusize baseAllocated;

            result = CheckResult(m_drmProcs.pfnAmdgpuVaRangeAlloc(
                                                    m_hDevice,
                                                    amdgpu_gpu_va_range_general,
                                                    size,
                                                    0u,
                                                    baseVirtAddr,
                                                    &baseAllocated,
                                                    &info.vaHandle,
                                                    0u),
                                 Result::ErrorUnknown);
            info.size = size;

            if (result == Result::Success)
            {
                PAL_ASSERT(baseAllocated == baseVirtAddr);
                m_reservedVaMap.Insert(baseVirtAddr, info);
            }
        }
        // Reservations using the same base address are not allowed
        else
        {
            result = Result::ErrorOutOfGpuMemory;
        }
    }

    return result;
}

// =====================================================================================================================
// Free reserved gpu va range.
Result Device::FreeGpuVirtualAddress(
    gpusize vaStartAddress,
    gpusize vaSize)
{
    Result result = Result::Success;

    ReservedVaRangeInfo* pInfo = m_reservedVaMap.FindKey(vaStartAddress);

    // ReserveGpuVirtualAddress doesn't allow for duplicate reservations, so we can safely free the address range
    if (pInfo != nullptr)
    {
        if (pInfo->size != vaSize)
        {
            result = Result::ErrorInvalidMemorySize;
        }

        if (result == Result::Success)
        {
            result = CheckResult(m_drmProcs.pfnAmdgpuVaRangeFree(pInfo->vaHandle),
                                 Result::ErrorUnknown);
            m_reservedVaMap.Erase(vaStartAddress);
        }
    }

    return result;
}

// =====================================================================================================================
Result Device::InitReservedVaRanges()
{
    return VamMgrSingleton::GetReservedVaRange(
        GetPlatform()->GetDrmLoader().GetProcsTable(),
        m_hDevice,
        GetPlatform()->IsDtifEnabled(),
        &m_memoryProperties);
}

// =====================================================================================================================
// Opens shared GPU memory from anyone except another PAL device in the same LDA chain.
Result Device::OpenExternalSharedGpuMemory(
    const ExternalGpuMemoryOpenInfo& openInfo,
    void*                            pPlacementAddr,
    GpuMemoryCreateInfo*             pMemCreateInfo,
    IGpuMemory**                     ppGpuMemory)
{
    Result result = Result::ErrorInvalidPointer;

    if ((pPlacementAddr != nullptr) && (pMemCreateInfo != nullptr) && (ppGpuMemory != nullptr))
    {
        GpuMemoryCreateInfo createInfo = {};
        // some information is zeroed out which will be filled later after importing the buffer
        createInfo.size         = 0;
        createInfo.alignment    = 0;
        createInfo.vaRange      = VaRange::Default;
        createInfo.priority     = GpuMemPriority::High;
        createInfo.heapCount    = 0;

        GpuMemoryInternalCreateInfo internalInfo = {};
        internalInfo.flags.isExternal = 1;

        internalInfo.hExternalResource  = openInfo.resourceInfo.hExternalResource;
        internalInfo.externalHandleType = amdgpu_bo_handle_type_dma_buf_fd;

        Pal::GpuMemory* pGpuMemory = ConstructGpuMemoryObject(pPlacementAddr);
        result = pGpuMemory->Init(createInfo, internalInfo);
        if (IsErrorResult(result))
        {
            pGpuMemory->Destroy();
            pGpuMemory = nullptr;
        }

        if (result == Result::Success)
        {
            // fill back the GpuMemoryCreateInfo.
            const GpuMemoryDesc& desc = pGpuMemory->Desc();
            createInfo.size = desc.size;
            createInfo.alignment = desc.alignment;
            GpuHeap*    pHeaps = &createInfo.heaps[0];
            static_cast<GpuMemory*>(pGpuMemory)->GetHeapsInfo(&createInfo.heapCount, &pHeaps);
            memcpy(pMemCreateInfo, &createInfo, sizeof(GpuMemoryCreateInfo));

            (*ppGpuMemory) = pGpuMemory;
        }
    }

    return result;
}

// =====================================================================================================================
// Create buffer object from system virtual address with size aligned to page size.
// The memory is not pinned down immediately. It is only guaranteed that the memory will be pinned down in
// per submission granularity.
Result Device::PinMemory(
    const void*         pCpuAddress,
    uint64              size,
    uint64*             pOffset,
    amdgpu_bo_handle*   pBufferHandle) const
{
    Result result = Result::Success;

    if ((size == 0) ||
        (!IsPow2Aligned(size, GpuPageSize)))
    {
        result = Result::ErrorInvalidMemorySize;
    }
    else if ((pCpuAddress == nullptr) ||
             (VoidPtrAlign(const_cast<void*>(pCpuAddress), GpuPageSize) != pCpuAddress))
    {
        result = Result::ErrorInvalidPointer;
    }
    else
    {
        *pOffset = 0;
        int32 retValue;
        {
            retValue = m_drmProcs.pfnAmdgpuCreateBoFromUserMem(m_hDevice,
                                                               const_cast<void*>(pCpuAddress),
                                                               size,
                                                               pBufferHandle);
        }

        // The amdgpu driver doesn't support multiple pinned buffer objects from the same system memory page.
        // If the request to pin memory above failed, we need to search for the existing pinned buffer object.
        // the bo that we found here is refcounted in the kernel.
        if ((retValue != 0) && m_drmProcs.pfnAmdgpuFindBoByCpuMappingisValid())
        {
            retValue = m_drmProcs.pfnAmdgpuFindBoByCpuMapping(m_hDevice,
                    const_cast<void*>(pCpuAddress),
                    size,
                    pBufferHandle,
                    pOffset);
        }

        if (retValue != 0)
        {
            result = Result::ErrorOutOfMemory;
        }
    }
    return result;
}

// =====================================================================================================================
// Set/Query the device clock mode.
Result Device::SetClockMode(
    const SetClockModeInput& setClockModeInput,
    SetClockModeOutput*      pSetClockModeOutput)
{
    Result result                         = Result::Success;

    int        ioRet                      = 0;
    const bool needUpdatePerformanceLevel = (DeviceClockMode::Query          != setClockModeInput.clockMode) &&
                                            (DeviceClockMode::QueryProfiling != setClockModeInput.clockMode) &&
                                            (DeviceClockMode::QueryPeak      != setClockModeInput.clockMode);
    char       writeBuf[MaxClockSysFsEntryNameLen];

    const char* pStrKMDInterface[]    =
    {
        "profile_exit",            // see the comments of DeviceClockMode::Default
        "profile_query",           // place holder, will not be passed to KMD (by means of needUpdatePerformanceLevel)
        "profile_standard",        // see the comments of DeviceClockMode::Profiling
        "profile_min_mclk",        // see the comments of DeviceClockMode::MinimumMemory
        "profile_min_sclk",        // see the comments of DeviceClockMode::MinimumEngine
        "profile_peak",            // see the comments of DeviceClockMode::Peak
        "profile_query_profiling", // place holder, will not be passed to KMD (by means of needUpdatePerformanceLevel)
        "profile_query_peak",      // place holder, will not be passed to KMD (by means of needUpdatePerformanceLevel)
    };

    // prepare contents which will be written to sysfs
    static_assert(static_cast<uint32>(DeviceClockMode::Default)        == 0, "DeviceClockMode definition changed!");
    static_assert(static_cast<uint32>(DeviceClockMode::Query)          == 1, "DeviceClockMode definition changed!");
    static_assert(static_cast<uint32>(DeviceClockMode::Profiling)      == 2, "DeviceClockMode definition changed!");
    static_assert(static_cast<uint32>(DeviceClockMode::MinimumMemory)  == 3, "DeviceClockMode definition changed!");
    static_assert(static_cast<uint32>(DeviceClockMode::MinimumEngine)  == 4, "DeviceClockMode definition changed!");
    static_assert(static_cast<uint32>(DeviceClockMode::Peak)           == 5, "DeviceClockMode definition changed!");
    static_assert(static_cast<uint32>(DeviceClockMode::QueryProfiling) == 6, "DeviceClockMode definition changed!");
    static_assert(static_cast<uint32>(DeviceClockMode::QueryPeak)      == 7, "DeviceClockMode definition changed!");

    PAL_ASSERT(static_cast<uint32>(setClockModeInput.clockMode) < sizeof(pStrKMDInterface)/sizeof(char*));

    memset(writeBuf, 0, sizeof(writeBuf));
    snprintf(writeBuf, sizeof(writeBuf), "%s", pStrKMDInterface[static_cast<uint32>(setClockModeInput.clockMode)]);

    // write to sysfs
    if (needUpdatePerformanceLevel)
    {
        int forcePerformanceLevelFd = open(m_forcePerformanceLevelPath, O_WRONLY);
        if (forcePerformanceLevelFd < 0)
        {
            result = Result::ErrorUnavailable;
        }

        if (result == Result::Success)
        {
            ioRet = write(forcePerformanceLevelFd, writeBuf, strlen(writeBuf));
            PAL_ALERT(static_cast<size_t>(ioRet) != strlen(writeBuf));
            if (static_cast<size_t>(ioRet) != strlen(writeBuf))
            {
                result = Result::ErrorUnavailable;
            }

            close(forcePerformanceLevelFd);
        }
    }

    uint32 sClkCurLevelIndex = 0;
    uint32 mClkCurLevelIndex = 0;

    ClkInfo sClkInfo(GetPlatform()); // shader clock info
    ClkInfo mClkInfo(GetPlatform()); // memory clock info

    sClkInfo.Clear();
    mClkInfo.Clear();

    // reload shader clock
    if (result == Result::Success)
    {
        result = ParseClkInfo(m_sClkPath, &sClkInfo, &sClkCurLevelIndex);
    }

    // reload memory clock
    if (result == Result::Success)
    {
        result = ParseClkInfo(m_mClkPath, &mClkInfo, &mClkCurLevelIndex);
    }

    // For ASIC SI, although the UMD/KMD interface exists, no content in it.
    // Add handling for this exception
    if ((0 == sClkInfo.NumElements()) || (0 == mClkInfo.NumElements()))
    {
        result = Result::ErrorUnavailable;
    }

    // generate results
    if ((result == Result::Success) && (pSetClockModeOutput != nullptr))
    {
        const uint32 mClkMaxLevelIndex = mClkInfo.NumElements() - 1;
        const uint32 sClkMaxLevelIndex = sClkInfo.NumElements() - 1;
        PAL_ASSERT(sClkCurLevelIndex <= sClkMaxLevelIndex);
        PAL_ASSERT(mClkCurLevelIndex <= mClkMaxLevelIndex);
        // Check result of amdgpu_query_gpu_info and /sys/class/drm/cardX/device/pp_dpm_Xclk mismatch
        PAL_ASSERT(m_chipProperties.maxEngineClock == sClkInfo.At(sClkMaxLevelIndex).value);
        PAL_ASSERT(m_chipProperties.maxMemoryClock == mClkInfo.At(mClkMaxLevelIndex).value);

        uint32 sClkInMhz       = 0;
        uint32 mClkInMhz       = 0;
        float  requiredSclkVal = 0.0f;
        float  maxSclkVal      = static_cast<float>(sClkInfo.At(sClkMaxLevelIndex).value);
        float  requiredMclkVal = 0.0f;
        float  maxMclkVal      = static_cast<float>(mClkInfo.At(mClkMaxLevelIndex).value);

        switch (setClockModeInput.clockMode)
        {
            case DeviceClockMode::QueryProfiling:
                // get stable pstate sclk in Mhz from KMD
                if (m_supportQuerySensorInfo)
                {
                    result = CheckResult(m_drmProcs.pfnAmdgpuQuerySensorInfo(m_hDevice,
                                                                             AMDGPU_INFO_SENSOR_STABLE_PSTATE_GFX_SCLK,
                                                                             sizeof(uint32),
                                                                             static_cast<void*>(&sClkInMhz)),
                                         Result::ErrorInvalidValue);
                }
                else
                {
                    result = Result::ErrorUnavailable;
                }

                if (result == Result::Success)
                {
                    // get stable pstate mclk in Mhz from KMD
                    if (m_supportQuerySensorInfo)
                    {
                        result = CheckResult(m_drmProcs.pfnAmdgpuQuerySensorInfo(m_hDevice,
                                                                                 AMDGPU_INFO_SENSOR_STABLE_PSTATE_GFX_MCLK,
                                                                                 sizeof(uint32),
                                                                                 static_cast<void*>(&mClkInMhz)),
                                             Result::ErrorInvalidValue);
                    }
                    else
                    {
                        result = Result::ErrorUnavailable;
                    }
                }

                if (result == Result::Success)
                {
#if PAL_ENABLE_PRINTS_ASSERTS
                    // There are three ways that could be used to query clocks under Linux:
                    // 1. By libdrm interface amdgpu_query_gpu_info, but this interface only provide peak clock
                    // 2. By libdrm interface amdgpu_query_sensor_info, but this interface only provide profiling clock
                    // 3. By direct reading /sys/class/drm/cardX/device/pp_dpm_Xclk, this interface could provide all
                    //    existing clock levels and the max level as peak clock.
                    // Check result of amdgpu_query_sensor_info and /sys/class/drm/cardX/device/pp_dpm_Xclk mismatch
                    bool isQueriedSclkValid = false;
                    bool isQueriedMclkValid = false;

                    for (uint32 i = 0; i < sClkInfo.NumElements(); ++i)
                    {
                        if (sClkInfo.At(i).value == sClkInMhz)
                        {
                            isQueriedSclkValid = true;
                        }
                    }

                    for (uint32 i = 0; i < mClkInfo.NumElements(); ++i)
                    {
                        if (mClkInfo.At(i).value == mClkInMhz)
                        {
                            isQueriedMclkValid = true;
                        }
                    }

                    PAL_ASSERT(isQueriedSclkValid);
                    PAL_ASSERT(isQueriedMclkValid);
#endif
                    requiredSclkVal = static_cast<float>(sClkInMhz);
                    requiredSclkVal = static_cast<float>(mClkInMhz);
                }
                break;
            case DeviceClockMode::QueryPeak:
                requiredSclkVal = maxSclkVal;
                requiredMclkVal = maxMclkVal;
                break;
            default:
                // for all other clockMode, use current clock value
                requiredSclkVal = static_cast<float>(sClkInfo.At(sClkCurLevelIndex).value);
                requiredMclkVal = static_cast<float>(mClkInfo.At(mClkCurLevelIndex).value);
        }

        if (result == Result::Success)
        {
            pSetClockModeOutput->engineClockRatioToPeak = requiredSclkVal / maxSclkVal;
            pSetClockModeOutput->memoryClockRatioToPeak = requiredMclkVal / maxMclkVal;
        }
    }

    return result;
}

// =====================================================================================================================
// Parse shader and memory clock from sysfs file exported by KMD
Result Device::ParseClkInfo(
    const char* pFilePath,
    ClkInfo*    pClkInfo,
    uint32*     pCurIndex)
{
    Result result = Result::Success;
    int    ioRet  = 0;

    char   readBuffer[ClockInfoReadBufLen];

    PAL_ASSERT((nullptr != pFilePath) && (nullptr != pClkInfo) && (nullptr != pCurIndex));

    int fd = open(pFilePath, O_RDONLY);
    if (fd < 0)
    {
        result = Result::ErrorUnavailable;
    }

    // read all contents to readBuffer
    if (result == Result::Success)
    {
        uint32 totalReadChars = 0;

        memset(readBuffer, 0, sizeof(readBuffer));

        do
        {
            ioRet = read(fd, readBuffer + totalReadChars, ClockInfoReadBufLen - totalReadChars);
            if (ioRet > 0)
            {
                totalReadChars += ioRet;
            }
            else if ((ioRet < 0) && (errno != EINTR))
            {
                PAL_ALERT("read pp_dpm_clk info error");
                result = Result::ErrorUnavailable;
                break;
            }
        } while ((totalReadChars < ClockInfoReadBufLen) && (0 != ioRet));

        // ensure we didn't overflow the readBuffer, otherwise we should increase the ClockInfoReadBufLen
        PAL_ASSERT(totalReadChars < ClockInfoReadBufLen);
        close(fd);
    }
    if (result == Result::Success)
    {
        char*  pCurLine       = readBuffer;
        uint32 totalInfoCount = 0;

        while (true)
        {
            ClockInfo curInfo    = {0, 0, false};
            char*     pCurStrPtr = pCurLine;

            // 0: 150Mhz
            // 1: 1375Mhz *
            curInfo.level = atoi(pCurStrPtr);
            pCurStrPtr    = strchr(pCurStrPtr, ' ');
            if (nullptr  != pCurStrPtr)
            {
                curInfo.value = atoi(pCurStrPtr);
            }
            else
            {
                // KMD protocol changed?
                PAL_ALERT(!"read pp_dpm_clk info error");
                result = Result::ErrorUnavailable;
                break;
            }

            // Based on current protocol
            PAL_ASSERT(curInfo.level == totalInfoCount);
            totalInfoCount++;

            pCurStrPtr   = strchr(pCurStrPtr, '\n');
            if (nullptr != pCurStrPtr)
            {
                pCurStrPtr        = pCurStrPtr - 1;
                curInfo.isCurrent = (*pCurStrPtr == '*');
                if (curInfo.isCurrent)
                {
                    *pCurIndex    = curInfo.level;
                }
            }
            else
            {
                // KMD protocol changed?
                PAL_ALERT(!"read pp_dpm_clk info error");
                result = Result::ErrorUnavailable;
                break;
            }

            pClkInfo->PushBack(curInfo);

            // next line
            pCurLine = strchr(pCurLine, '\n');
            if ((nullptr != pCurLine) &&
                (static_cast<size_t>(pCurLine - readBuffer) < ClockInfoReadBufLen) &&
                (*(pCurLine + 1) != '\0'))
            {
                pCurLine = pCurLine + 1;
            }
            else
            {
                // no new lines
                break;
            }
        }
    }
    return result;
}

// =====================================================================================================================
// Initialize all needed sysfs file path exported by KMD.
// Parse shader and memory clock.
Result Device::InitClkInfo()
{
    // init sysfs file path
    snprintf(m_forcePerformanceLevelPath,
             sizeof(m_forcePerformanceLevelPath),
             "/sys/class/drm/card%u/device/power_dpm_force_performance_level",
             GetDeviceNodeIndex());
    snprintf(m_sClkPath,
             sizeof(m_sClkPath),
             "/sys/class/drm/card%u/device/pp_dpm_sclk",
             GetDeviceNodeIndex());
    snprintf(m_mClkPath,
             sizeof(m_mClkPath),
             "/sys/class/drm/card%u/device/pp_dpm_mclk",
             GetDeviceNodeIndex());

    return Result::Success;
}

// =====================================================================================================================
void Device::SetVaRangeInfo(
    uint32       partIndex,
    VaRangeInfo* pVaRange)
{
    PAL_ASSERT((pVaRange != nullptr) && (partIndex < static_cast<uint32>(VaPartition::Count)));
    m_memoryProperties.vaRange[partIndex] = *pVaRange;
}

// =====================================================================================================================
Result Device::CheckExecutionState() const
{
    // linux don't have device level interface to query the device state.
    // just query the gpu timestamp
    // the kernel will return -NODEV if gpu reset happens.
    uint64 gpuTimestamp = 0;

    Result result = CheckResult(m_drmProcs.pfnAmdgpuQueryInfo(m_hDevice,
                                                              AMDGPU_INFO_TIMESTAMP,
                                                              sizeof(gpuTimestamp),
                                                              &gpuTimestamp), Result::Success);
    return result;
}

// =====================================================================================================================
// Helper function to check kernel version.
bool Device::IsKernelVersionEqualOrGreater(
    uint32    kernelMajorVer,
    uint32    kernelMinorVer
    ) const
{
    bool result = false;

    struct utsname buffer = {};

    if (uname(&buffer) == 0)
    {
        uint32 majorVersion = 0;
        uint32 minorVersion = 0;

        if (sscanf(buffer.release, "%d.%d", &majorVersion, &minorVersion) == 2)
        {
            if ((majorVersion > kernelMajorVer) ||
               ((majorVersion == kernelMajorVer) && (minorVersion >= kernelMinorVer)))
            {
                result = true;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Helper function to get all information needed to create an external shared image or GPU memory. On some clients this
// may require an OpenResource thunk and may result in a dynamic allocation. If a dynamic allocation occured, the
// address will be stored in pPrivData and must be freed by the caller once they are done with the allocation info.
Result Device::OpenExternalResource(
    const ExternalResourceOpenInfo& openInfo,
    ExternalSharedInfo*             pSharedInfo
    ) const
{
    // hard code the amdgpu_bo_handle_type_dma_buf_fd.
    // this can be extended later in case pal need to support more types.
    Result result = ImportBuffer(amdgpu_bo_handle_type_dma_buf_fd,
                                 openInfo.hExternalResource,
                                 &pSharedInfo->importResult);

    if (result == Result::Success)
    {
        result  = QueryBufferInfo(pSharedInfo->importResult.buf_handle, &pSharedInfo->info);
    }

    if (result == Result::Success)
    {
        pSharedInfo->hExternalResource = openInfo.hExternalResource;
        PAL_ASSERT(pSharedInfo->importResult.alloc_size == pSharedInfo->info.alloc_size);
    }

    return result;
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
    Result result = Result::ErrorInvalidPointer;

    if ((pImageSize != nullptr) && (pGpuMemorySize != nullptr))
    {
        ExternalSharedInfo  sharedInfo = {};
        result = OpenExternalResource(openInfo.resourceInfo, &sharedInfo);

        if (result == Result::Success)
        {
            ImageCreateInfo createInfo = {};
            Image::GetExternalSharedImageCreateInfo(*this, openInfo, sharedInfo, &createInfo);

            (*pImageSize)     = GetImageSize(createInfo, nullptr);
            (*pGpuMemorySize) = GetExternalSharedGpuMemorySize(nullptr);

            if (pImgCreateInfo != nullptr)
            {
                memcpy(pImgCreateInfo, &createInfo, sizeof(createInfo));
            }

            // We don't need to keep the reference to the BO anymore.
            FreeBuffer(sharedInfo.importResult.buf_handle);
        }
    }

    return result;
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
    Result result = Result::ErrorInvalidPointer;

    if ((pImagePlacementAddr     != nullptr) &&
        (pGpuMemoryPlacementAddr != nullptr) &&
        (ppImage                 != nullptr) &&
        (ppGpuMemory             != nullptr))
    {
        ExternalSharedInfo  sharedInfo = {};
        result = OpenExternalResource(openInfo.resourceInfo, &sharedInfo);

        if (result == Result::Success)
        {
            result = Image::CreateExternalSharedImage(this,
                                                      openInfo,
                                                      sharedInfo,
                                                      pImagePlacementAddr,
                                                      pGpuMemoryPlacementAddr,
                                                      pMemCreateInfo,
                                                      ppImage,
                                                      ppGpuMemory);

            // We don't need to keep the reference to the BO anymore.
            FreeBuffer(sharedInfo.importResult.buf_handle);
        }
    }

    return result;
}

// =====================================================================================================================
// Creates a GPU memory object which was opened from anyone except another PAL device in the same LDA chain
Result Device::CreateGpuMemoryFromExternalShare(
    const TypedBufferCreateInfo* pTypedBufferCreateInfo,
    Pal::Image*                  pImage,
    const ExternalSharedInfo&    sharedInfo,
    void*                        pPlacementAddr,
    GpuMemoryCreateInfo*         pCreateInfo,
    Pal::GpuMemory**             ppGpuMemory)
{
    // Require that create info is provided because we'll need it either way unlike the interface where it is optional.
    PAL_ASSERT(pCreateInfo != nullptr);

    PAL_ASSERT(m_memoryProperties.realMemAllocGranularity == 4096);

    pCreateInfo->alignment = Max(static_cast<gpusize>(sharedInfo.info.phys_alignment),
                                 m_memoryProperties.realMemAllocGranularity);
    pCreateInfo->size      = Pow2Align(sharedInfo.info.alloc_size, pCreateInfo->alignment);
    pCreateInfo->vaRange   = VaRange::Default;
    pCreateInfo->priority  = GpuMemPriority::High;

    if (sharedInfo.info.preferred_heap & AMDGPU_GEM_DOMAIN_GTT)
    {
        // Check for any unexpected flags
        PAL_ASSERT((sharedInfo.info.preferred_heap & ~AMDGPU_GEM_DOMAIN_GTT) == 0);

        if (sharedInfo.info.alloc_flags & AMDGPU_GEM_CREATE_CPU_GTT_USWC)
        {
            // Check for any unexpected flags
            PAL_ASSERT((sharedInfo.info.alloc_flags & ~AMDGPU_GEM_CREATE_CPU_GTT_USWC) == 0);

            pCreateInfo->heaps[pCreateInfo->heapCount++] = GpuHeapGartUswc;
        }
        else
        {
            // Check for any unexpected flags
            PAL_ASSERT(sharedInfo.info.alloc_flags == 0);

            pCreateInfo->heaps[pCreateInfo->heapCount++] = GpuHeapGartCacheable;
        }
    }
    else if (sharedInfo.info.preferred_heap & AMDGPU_GEM_DOMAIN_VRAM)
    {
        // Check for any unexpected flags
        PAL_ASSERT((sharedInfo.info.preferred_heap & ~AMDGPU_GEM_DOMAIN_VRAM) == 0);

        if (sharedInfo.info.alloc_flags & AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED)
        {
            // Check for any unexpected flags
            PAL_ASSERT((sharedInfo.info.alloc_flags & ~AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED) == 0);
            pCreateInfo->heaps[pCreateInfo->heapCount++] = GpuHeapLocal;
        }
        else if (sharedInfo.info.alloc_flags & AMDGPU_GEM_CREATE_NO_CPU_ACCESS)
        {
            // Check for any unexpected flags
            PAL_ASSERT((sharedInfo.info.alloc_flags & ~AMDGPU_GEM_CREATE_NO_CPU_ACCESS) == 0);

            pCreateInfo->heaps[pCreateInfo->heapCount++] = GpuHeapInvisible;
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    GpuMemoryInternalCreateInfo internalInfo = {};
    internalInfo.flags.isExternal   = 1;
    internalInfo.hExternalResource  = sharedInfo.hExternalResource;
    internalInfo.externalHandleType = amdgpu_bo_handle_type_dma_buf_fd;

    if (pTypedBufferCreateInfo != nullptr)
    {
        PAL_ASSERT(pImage == nullptr);

        pCreateInfo->flags.typedBuffer = true;
        pCreateInfo->typedBufferInfo   = *pTypedBufferCreateInfo;
    }
    else if (pImage != nullptr)
    {
        pCreateInfo->pImage          = pImage;
        pCreateInfo->flags.flippable = pImage->IsFlippable();

        internalInfo.flags.privateScreen = (pImage->GetPrivateScreen() != nullptr);
    }

    GpuMemory* pGpuMemory = static_cast<GpuMemory*>(ConstructGpuMemoryObject(pPlacementAddr));
    Result     result     = pGpuMemory->Init(*pCreateInfo, internalInfo);

    if (result != Result::Success)
    {
        pGpuMemory->Destroy();
        pGpuMemory = nullptr;
    }

    (*ppGpuMemory) = pGpuMemory;

    return result;
}

// =====================================================================================================================
// Query bus addresses
Result Device::InitBusAddressableGpuMemory(
    IQueue*           pQueue,
    uint32            gpuMemCount,
    IGpuMemory*const* ppGpuMemList)
{
    Result result = Result::Success;

    for (uint32 i = 0; (i < gpuMemCount) && (result == Result::Success); i++)
    {
        GpuMemory* pGpuMem = static_cast<GpuMemory*>(ppGpuMemList[i]);
        result = pGpuMem->QuerySdiBusAddress();
    }
    return result;
}

// =====================================================================================================================
// Query local SDI surface attributes
Result Device::QuerySdiSurface(
    amdgpu_bo_handle    hSurface,
    uint64*             pPhysAddress)
{
    PAL_ASSERT(pPhysAddress != nullptr);

    Result result = CheckResult(m_drmProcs.pfnAmdgpuBoGetPhysAddress(hSurface,
                                                                     pPhysAddress),
                                Result::ErrorOutOfGpuMemory);

    return result;
}

// =====================================================================================================================
// allocate External Physical Memory
Result Device::SetSdiSurface(
    GpuMemory*  pGpuMem,
    gpusize*    pCardAddr)
{
    amdgpu_va_handle hVaRange;
    amdgpu_bo_handle hBuffer;
    uint64 vaAllocated;

    Result result = MapSdiMemory(m_hDevice,
                                 pGpuMem->Desc().surfaceBusAddr,
                                 pGpuMem->Desc().size,
                                 hBuffer,
                                 hVaRange,
                                 vaAllocated);

    if (result == Result::Success)
    {
        pGpuMem->SetSurfaceHandle(hBuffer);
        pGpuMem->SetVaRangeHandle(hVaRange);
        *pCardAddr = vaAllocated;
        result = MapSdiMemory(m_hDevice,
                              pGpuMem->Desc().markerBusAddr,
                              pGpuMem->Desc().size,
                              hBuffer,
                              hVaRange,
                              vaAllocated);
    }

    if (result == Result::Success)
    {
        pGpuMem->SetMarkerHandle(hBuffer);
        pGpuMem->SetMarkerVaRangeHandle(hVaRange);
        pGpuMem->SetBusAddrMarkerVa(vaAllocated);
    }

    return Result::Success;
}

// =====================================================================================================================
// Free External Physical Memory
Result Device::FreeSdiSurface(
    GpuMemory*  pGpuMem)
{
    Result result = Result::Success;

    if (pGpuMem->GetBusAddrMarkerVa() != 0)
    {
        result = UnmapSdiMemory(pGpuMem->GetBusAddrMarkerVa(),
                                pGpuMem->Desc().size,
                                pGpuMem->MarkerHandle(),
                                pGpuMem->MarkerVaRangeHandle());
        pGpuMem->SetBusAddrMarkerVa(0);
    }

    return result;
}

// =====================================================================================================================
// allocate buffer and VA for Surface/Marker of External Physical Memory
Result Device::MapSdiMemory(
    amdgpu_device_handle    hDevice,
    uint64                  busAddress,
    gpusize                 size,
    amdgpu_bo_handle&       hBuffer,
    amdgpu_va_handle&       hVaRange,
    uint64&                 vaAllocated)
{
    Result result = CheckResult(m_drmProcs.pfnAmdgpuCreateBoFromPhysMem(hDevice,
                                                                        busAddress,
                                                                        size,
                                                                        &hBuffer),
                                Result::ErrorOutOfGpuMemory);

    if (result == Result::Success)
    {
        result = CheckResult(m_drmProcs.pfnAmdgpuVaRangeAlloc(hDevice,
                                                              amdgpu_gpu_va_range_general,
                                                              size,
                                                              m_memoryProperties.fragmentSize,
                                                              0,
                                                              &vaAllocated,
                                                              &hVaRange,
                                                              0),
                             Result::ErrorInvalidValue);
    }

    if (result == Result::Success)
    {
        result = CheckResult(m_drmProcs.pfnAmdgpuBoVaOp(hBuffer,
                                                        0,
                                                        size,
                                                        vaAllocated,
                                                        0,
                                                        AMDGPU_VA_OP_MAP),
                             Result::ErrorInvalidValue);
    }

    return result;
}

// =====================================================================================================================
// Free buffer and VA for External Physical Memory
Result Device::UnmapSdiMemory(
    uint64                  virtAddress,
    gpusize                 size,
    amdgpu_bo_handle        hBuffer,
    amdgpu_va_handle        hVaRange)
{
    Result result = CheckResult(m_drmProcs.pfnAmdgpuBoVaOp(hBuffer,
                                                           0,
                                                           size,
                                                           virtAddress,
                                                           0,
                                                           AMDGPU_VA_OP_UNMAP),
                                Result::ErrorInvalidValue);

    if (result == Result::Success)
    {
        result = CheckResult(m_drmProcs.pfnAmdgpuVaRangeFree(hVaRange),
                             Result::ErrorInvalidValue);
    }

    return result;
}

// =====================================================================================================================
Result Device::QueryScreenModesForConnector(
    uint32      connectorId,
    uint32*     pModeCount,
    ScreenMode* pScreenModeList)
{
    Result result = Result::Success;

    if (!m_masterFileDescriptor)
    {
        m_masterFileDescriptor = open(m_primaryNodeName, O_RDWR | O_CLOEXEC | O_NONBLOCK);
        m_drmProcs.pfnDrmDropMaster(m_masterFileDescriptor);
    }

    drmModeConnector* pConnector = m_drmProcs.pfnDrmModeGetConnector(m_masterFileDescriptor, connectorId);
    if (pConnector == nullptr)
    {
        result = Result::ErrorInvalidValue;
    }
    else
    {
        PAL_ASSERT((pConnector->connection == DRM_MODE_CONNECTED) && (pConnector->count_modes > -1));

        if (pScreenModeList != nullptr)
        {
            uint32 loopCount = pConnector->count_modes;

            if (*pModeCount < static_cast<uint32>(pConnector->count_modes))
            {
                result = Result::ErrorInvalidMemorySize;
                loopCount = *pModeCount;
            }

            for (uint32 j = 0; j < loopCount; j++)
            {
                struct drm_mode_modeinfo* pMode =
                    reinterpret_cast<struct drm_mode_modeinfo *>(&pConnector->modes[j]);

                pScreenModeList[j].extent.width   = pMode->hdisplay;
                pScreenModeList[j].extent.height  = pMode->vdisplay;
                pScreenModeList[j].refreshRate    = pMode->vrefresh;
                pScreenModeList[j].flags.u32All   = 0;
            }

            *pModeCount = loopCount;
        }
        else
        {
            *pModeCount = pConnector->count_modes;
        }
    }

    m_drmProcs.pfnDrmModeFreeConnector(pConnector);

    return result;
}

// =====================================================================================================================
Result Device::GetScreens(
    uint32*  pScreenCount,
    void*    pStorage[MaxScreens],
    IScreen* pScreens[MaxScreens])

{
    Result result = Result::Success;

    if (!m_masterFileDescriptor)
    {
        m_masterFileDescriptor = open(m_primaryNodeName, O_RDWR | O_CLOEXEC | O_NONBLOCK);
        m_drmProcs.pfnDrmDropMaster(m_masterFileDescriptor);
    }

    PAL_ASSERT(m_masterFileDescriptor >= 0);
    PAL_ASSERT(pScreenCount != nullptr);

    // Enumerate connector and construct IScreen for any connected connector.
    drmModeRes *pResources = m_drmProcs.pfnDrmModeGetResources(m_masterFileDescriptor);

    if (pResources != nullptr)
    {
        uint32 screenCount = 0;

        for (int32 i = 0; i < pResources->count_connectors; i++)
        {
            drmModeConnector* pConnector = m_drmProcs.pfnDrmModeGetConnector(m_masterFileDescriptor,
                                                                             pResources->connectors[i]);
            if (pConnector == nullptr)
            {
                continue;
            }

            if ((pConnector->connection == DRM_MODE_CONNECTED) && (pConnector->count_modes > 0))
            {
                if (pStorage != nullptr)
                {
                    // Find out the preferred mode
                    uint32 preferredWidth  = 0;
                    uint32 preferredHeight = 0;
                    for (int32 j = 0; j < pConnector->count_modes; j++)
                    {
                        auto*const pMode = reinterpret_cast<drm_mode_modeinfo *>(&pConnector->modes[j]);

                        if ((preferredWidth  < pMode->hdisplay) &&
                            (preferredHeight < pMode->vdisplay))
                        {
                            preferredWidth  = pMode->hdisplay;
                            preferredHeight = pMode->vdisplay;
                        }
                    }

                    Extent2d physicalDimension = {pConnector->mmWidth, pConnector->mmHeight};
                    Extent2d physicalResolution = {preferredWidth, preferredHeight};

                    Screen* pScreen = PAL_PLACEMENT_NEW(pStorage[screenCount]) Screen(this,
                            physicalDimension,
                            physicalResolution,
                            pResources->connectors[i]);

                    result = pScreen->Init();

                    if (result == Result::Success)
                    {
                        pScreens[screenCount] = pScreen;
                    }
                }
                screenCount ++;
            }
            m_drmProcs.pfnDrmModeFreeConnector(pConnector);
        }
        m_drmProcs.pfnDrmModeFreeResources(pResources);

        if (result == Result::Success)
        {
            *pScreenCount = screenCount;
        }
    }
    return result;
}

} // Linux
} // Pal
