/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "g_coreSettings.h"
#include "core/os/nullDevice/ndDevice.h"
#include "core/os/amdgpu/amdgpuDevice.h"
#include "core/os/amdgpu/amdgpuImage.h"
#include "core/os/amdgpu/amdgpuQueue.h"
#include "core/os/amdgpu/amdgpuSwapChain.h"
#include "core/os/amdgpu/amdgpuSyncobjFence.h"
#include "core/os/amdgpu/amdgpuTimestampFence.h"
#include "core/os/amdgpu/amdgpuVamMgr.h"
#include "core/os/amdgpu/amdgpuWindowSystem.h"
#if PAL_HAVE_DRI3_PLATFORM
#include "core/os/amdgpu/dri3/dri3WindowSystem.h"
#endif
#include "core/queueSemaphore.h"
#include "core/device.h"
#include "palAutoBuffer.h"
#include "palHashMapImpl.h"
#include "palInlineFuncs.h"
#include "palSettingsFileMgrImpl.h"
#include "palSysMemory.h"
#include "palSysUtil.h"
#include "palVectorImpl.h"
#include "palIntrusiveListImpl.h"
#include "core/addrMgr/addrMgr1/addrMgr1.h"
#include "core/addrMgr/addrMgr2/addrMgr2.h"
//  NOTE: We need this for address pipe config.
#include "core/hw/gfxip/gfx6/chip/si_ci_vi_merged_enum.h"
// NOTE: We need this chip header for reading registers.
#include "core/hw/gfxip/gfx6/chip/si_ci_vi_merged_offset.h"
#include "core/hw/gfxip/gfx6/chip/si_ci_vi_merged_mask.h"

#include <climits>
#include <inttypes.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <sys/sysmacros.h>

using namespace Util;
using namespace Pal::AddrMgr1;

namespace Pal
{
namespace Amdgpu
{

// =====================================================================================================================
// Helper method which check result from drm function
static Result CheckResult(
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
        retValue = Result::ErrorOutOfGpuMemory;
        break;
    case -ETIMEDOUT:
    case -ETIME:
        retValue = Result::Timeout;
        break;
    case -ECANCELED:
        retValue = Result::ErrorDeviceLost;
        break;
    case -EACCES:
        retValue = Result::ErrorPermissionDenied;
        break;
    default:
        retValue = defaultValue;
        break;
    }
    return retValue;
}

constexpr gpusize _4GB = (1ull << 32u);
constexpr uint32 GpuPageSize = 4096;

constexpr char UserDefaultConfigFileSubPath[] = "/.config";
constexpr char UserDefaultCacheFileSubPath[]  = "/.cache";
constexpr char UserDefaultDebugFilePath[]     = "/var/tmp";

// 32 bpp formats are supported on all supported gpu's and amdgpu kms drivers:
constexpr SwizzledFormat PresentableSwizzledFormat[] =
{
    {
        ChNumFormat::X8Y8Z8W8_Unorm,
        { ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::W }
    },
    {
        ChNumFormat::X8Y8Z8W8_Srgb,
        { ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::W }
    },
    {
        ChNumFormat::X10Y10Z10W2_Unorm,
        { ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::W }
    },
    {
        ChNumFormat::X10Y10Z10W2_Unorm,
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W }
    },
};

// 64 bpp formats are supported on more recent supported gpu's and amdgpu kms drivers:
constexpr SwizzledFormat Presentable16BitSwizzledFormat[] =
{
    {
        ChNumFormat::X16Y16Z16W16_Float,
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W }
    },
    {
        ChNumFormat::X16Y16Z16W16_Unorm,
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W }
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
    int32                   availableNodes,
    int32*                  pFileDescriptor,
    int32*                  pPrimaryFileDescriptor,
    amdgpu_device_handle*   pDeviceHandle,
    uint32*                 pDrmMajorVer,
    uint32*                 pDrmMinorVer,
    struct amdgpu_gpu_info* pGpuInfo,
    uint32*                 pCpVersion,
    DrmNodeProperties*      pDrmNodeProperties);

// =====================================================================================================================
Result Device::Create(
    Platform*               pPlatform,
    const char*             pSettingsPath,
    const char*             pBusId,
    const char*             pPrimaryNode,
    const char*             pRenderNode,
    int32                   availableNodes,
    const drmPciBusInfo&    pciBusInfo,
    uint32                  deviceIndex,
    Device**                ppDeviceOut)
{
    HwIpLevels             ipLevels              = {};
    HwIpDeviceSizes        hwDeviceSizes         = {};
    size_t                 addrMgrSize           = 0;
    int32                  fileDescriptor        = 0;
    int32                  primaryFileDescriptor = 0;
    amdgpu_device_handle   hDevice               = nullptr;
    uint32                 drmMajorVer           = 0;
    uint32                 drmMinorVer           = 0;
    struct amdgpu_gpu_info gpuInfo               = {};
    uint32                 cpVersion             = 0;
    uint32                 attachedScreenCount   = 0;
    DrmNodeProperties      drmNodeProperties     = {};

    Result result = OpenAndInitializeDrmDevice(pPlatform,
                                               pBusId,
                                               pPrimaryNode,
                                               pRenderNode,
                                               availableNodes,
                                               &fileDescriptor,
                                               &primaryFileDescriptor,
                                               &hDevice,
                                               &drmMajorVer,
                                               &drmMinorVer,
                                               &gpuInfo,
                                               &cpVersion,
                                               &drmNodeProperties);

    if (result == Result::Success)
    {
        if (Device::DetermineGpuIpLevels(gpuInfo.family_id,
                                         gpuInfo.chip_external_rev,
                                         cpVersion,
                                         pPlatform,
                                         &ipLevels) == false)
        {
            result = Result::ErrorInitializationFailed;
        }
    }

    if (result == Result::Success)
    {
        GetHwIpDeviceSizes(ipLevels, &hwDeviceSizes, &addrMgrSize);

        size_t totalSize = 0;
        totalSize += sizeof(Device);
        totalSize += (hwDeviceSizes.gfx   +
                      hwDeviceSizes.oss   +
                      addrMgrSize);

        void*  pMemory  = PAL_MALLOC_ALIGNED(totalSize,
                                             alignof(Device),
                                             pPlatform,
                                             Util::AllocInternal);

        if (pMemory != nullptr)
        {
            const uint32 deviceNodeIndex = atoi(strstr(pPrimaryNode, "card") + strlen("card"));

            DeviceConstructorParams constructorParams = {
                .pPlatform             = pPlatform,
                .pSettingsPath         = pSettingsPath,
                .pBusId                = pBusId,
                .pRenderNode           = pRenderNode,
                .pPrimaryNode          = pPrimaryNode,
                .fileDescriptor        = fileDescriptor,
                .primaryFileDescriptor = primaryFileDescriptor,
                .hDevice               = hDevice,
                .drmMajorVer           = drmMajorVer,
                .drmMinorVer           = drmMinorVer,
                .deviceSize            = sizeof(Device),
                .deviceIndex           = deviceIndex,
                .deviceNodeIndex       = deviceNodeIndex,
                .attachedScreenCount   = attachedScreenCount,
                .gpuInfo               = gpuInfo,
                .hwDeviceSizes         = hwDeviceSizes,
                .pciBusInfo            = pciBusInfo,
                .drmNodeProperties     = drmNodeProperties
            };

            (*ppDeviceOut) = PAL_PLACEMENT_NEW(pMemory) Device(constructorParams);

            result = (*ppDeviceOut)->EarlyInit(ipLevels);

            if (result != Result::Success)
            {
                (*ppDeviceOut)->Cleanup(); // Ignore result; we've already failed.
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
// Helper function which overrides certain GPU properties for experiment purposes.
static void ValidateGpuInfo(
    Platform*               pPlatform,
    struct amdgpu_gpu_info* pGpuInfo) // in,out: GPU info to (optionally) override
{
    GpuId gpuId = { };
    gpuId.familyId   = pGpuInfo->family_id;
    gpuId.eRevId     = pGpuInfo->chip_external_rev;
    gpuId.revisionId = pGpuInfo->pci_rev_id;
    gpuId.deviceId   = pGpuInfo->asic_id;
    if (pPlatform->OverrideGpuId(&gpuId))
    {
        pGpuInfo->family_id         = gpuId.familyId;
        pGpuInfo->asic_id           = gpuId.deviceId;
        pGpuInfo->chip_external_rev = gpuId.eRevId;
        pGpuInfo->pci_rev_id        = gpuId.revisionId;
        // On amdgpu, the gfxEngineId is set up based on graphics IP level later on. No need to override it here.
    }
}

// =====================================================================================================================
// This is help methods. Open drm device and initialize it.
// And also get the drm information.
static Result OpenAndInitializeDrmDevice(
    Platform*               pPlatform,
    const char*             pBusId,
    const char*             pPrimaryNode,
    const char*             pRenderNode,
    int32                   availableNodes,
    int32*                  pFileDescriptor,
    int32*                  pPrimaryFileDescriptor,
    amdgpu_device_handle*   pDeviceHandle,
    uint32*                 pDrmMajorVer,
    uint32*                 pDrmMinorVer,
    struct amdgpu_gpu_info* pGpuInfo,
    uint32*                 pCpVersion,
    DrmNodeProperties*      pDrmNodeProperties)
{
    Result                 result       = Result::Success;
    amdgpu_device_handle   deviceHandle = nullptr;
    uint32                 majorVersion = 0;
    uint32                 minorVersion = 0;

    // Using render node here so that we can do the off-screen rendering without authentication.
    int32 fd        = open(pRenderNode, O_RDWR, 0);
    PAL_ASSERT(fd > 0); // Make sure the user has the "Render" permission to access /dev/dri/render*

    int32 primaryFd = InvalidFd;

    if (pPlatform->DontOpenPrimaryNode() == false)
    {
        primaryFd = open(pPrimaryNode, O_RDWR, 0);
    }

    const DrmLoaderFuncs& procs = pPlatform->GetDrmLoader().GetProcsTable();

    if ((fd < 0) ||
        ((primaryFd < 0) && (pPlatform->DontOpenPrimaryNode() == false)))
    {
        result = Result::ErrorInitializationFailed;
    }
    else
    {
        drmVersionPtr version = procs.pfnDrmGetVersion(fd);

        // Verify the kernel module name, only support "amdgpu"
        bool hasSupportedKmd = ((version != nullptr)    &&
                                (version->name_len > 0) &&
                                (strcmp(version->name, "amdgpu") == 0));

        if (hasSupportedKmd)
        {
            // Initialize the amdgpu device.
            result = CheckResult(procs.pfnAmdgpuDeviceInitialize(fd, &majorVersion, &minorVersion, &deviceHandle),
                                Result::ErrorInitializationFailed);
        }
        else
        {
            result = Result::Unsupported;
        }

        procs.pfnDrmFreeVersion(version);
    }

    if (result == Result::Success)
    {
        // amdgpu_query_gpu_info will never fail only if it is initialized.
        procs.pfnAmdgpuQueryGpuInfo(deviceHandle, pGpuInfo);
        ValidateGpuInfo(pPlatform, pGpuInfo);

        uint32 version = 0;
        if (procs.pfnAmdgpuQueryFirmwareVersion(deviceHandle,
                                             AMDGPU_INFO_FW_GFX_ME,
                                             0,
                                             0,
                                             &version,
                                             pCpVersion) != 0)
        {
            result = Result::ErrorInitializationFailed;
        }
        else
        {
            if (pPlatform->DontOpenPrimaryNode() == false)
            {
                procs.pfnDrmSetClientCap(primaryFd, DRM_CLIENT_CAP_ATOMIC, 1);
            }
        }
    }

    if (result == Result::Success)
    {
        *pFileDescriptor        = fd;
        *pPrimaryFileDescriptor = primaryFd;
        *pDeviceHandle          = deviceHandle;
        *pDrmMajorVer           = majorVersion;
        *pDrmMinorVer           = minorVersion;
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

            fd               = InvalidFd;
            *pFileDescriptor = InvalidFd;
        }

        if (primaryFd > 0)
        {
            close(primaryFd);

            primaryFd               = InvalidFd;
            *pPrimaryFileDescriptor = InvalidFd;
        }
    }

    if (result == Result::Success)
    {
        struct stat pPrimaryStat = {0};
        struct stat pRenderStat  = {0};

        pDrmNodeProperties->flags.hasPrimaryDrmNode = (availableNodes & (1 << DRM_NODE_PRIMARY)) &&
                                                      (stat(pPrimaryNode, &pPrimaryStat) == 0);
        pDrmNodeProperties->primaryDrmNodeMajor     = static_cast<int64>(major(pPrimaryStat.st_rdev));
        pDrmNodeProperties->primaryDrmNodeMinor     = static_cast<int64>(minor(pPrimaryStat.st_rdev));

        pDrmNodeProperties->flags.hasRenderDrmNode  = (availableNodes & (1 << DRM_NODE_RENDER)) &&
                                                      (stat(pRenderNode, &pRenderStat) == 0);
        pDrmNodeProperties->renderDrmNodeMajor      = static_cast<int64>(major(pRenderStat.st_rdev));
        pDrmNodeProperties->renderDrmNodeMinor      = static_cast<int64>(minor(pRenderStat.st_rdev));
    }

    return result;
}

// =====================================================================================================================
Device::Device(
    const DeviceConstructorParams& constructorParams)
    :
    Pal::Device(constructorParams.pPlatform,
                constructorParams.deviceIndex,
                constructorParams.attachedScreenCount,
                constructorParams.deviceSize,
                constructorParams.hwDeviceSizes,
                MaxSemaphoreCount),
    m_fileDescriptor(constructorParams.fileDescriptor),
    m_primaryFileDescriptor(constructorParams.primaryFileDescriptor),
    m_hDevice(constructorParams.hDevice),
    m_pVamMgr(nullptr),
    m_deviceNodeIndex(constructorParams.deviceNodeIndex),
    m_useSharedGpuContexts(false),
    m_hContext(nullptr),
    m_hTmzContext(nullptr),
    m_drmMajorVer(constructorParams.drmMajorVer),
    m_drmMinorVer(constructorParams.drmMinorVer),
    m_pSettingsPath(constructorParams.pSettingsPath),
    m_pSvmMgr(nullptr),
    m_mapAllocator(),
    m_reservedVaMap(32, &m_mapAllocator),
    m_globalRefMap(MemoryRefMapElements, constructorParams.pPlatform),
    m_semType(SemaphoreType::Legacy),
    m_fenceType(FenceType::Legacy),
#if defined(PAL_DEBUG_PRINTS)
    m_drmProcs(constructorParams.pPlatform->GetDrmLoader().GetProcsTableProxy())
#else
    m_drmProcs(constructorParams.pPlatform->GetDrmLoader().GetProcsTable())
#endif
{
    Util::Strncpy(m_busId, constructorParams.pBusId, MaxBusIdStringLen);
    Util::Strncpy(m_renderNodeName, constructorParams.pRenderNode, MaxNodeNameLen);
    Util::Strncpy(m_primaryNodeName, constructorParams.pPrimaryNode, MaxNodeNameLen);

    memcpy(&m_gpuInfo, &constructorParams.gpuInfo, sizeof(constructorParams.gpuInfo));
    memcpy(&m_drmNodeProperties, &constructorParams.drmNodeProperties, sizeof(constructorParams.drmNodeProperties));

    m_chipProperties.pciDomainNumber            = constructorParams.pciBusInfo.domain;
    m_chipProperties.pciBusNumber               = constructorParams.pciBusInfo.bus;
    m_chipProperties.pciDeviceNumber            = constructorParams.pciBusInfo.dev;
    m_chipProperties.pciFunctionNumber          = constructorParams.pciBusInfo.func;
    m_chipProperties.gpuConnectedViaThunderbolt = false;

    m_featureState.flags = 0;
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

    if (m_hTmzContext != nullptr)
    {
        m_drmProcs.pfnAmdgpuCsCtxFree(m_hTmzContext);
        m_hTmzContext = nullptr;
    }

    if (m_pVamMgr != nullptr)
    {
        VamMgrSingleton::Cleanup(this);
        m_pVamMgr = nullptr;
    }

    if (m_hDevice != nullptr)
    {
        m_drmProcs.pfnAmdgpuDeviceDeinitialize(m_hDevice);
        m_hDevice = nullptr;
    }

    if (m_fileDescriptor > 0)
    {
        close(m_fileDescriptor);
        m_fileDescriptor = InvalidFd;
    }

    if (m_primaryFileDescriptor > 0)
    {
        close(m_primaryFileDescriptor);
        m_primaryFileDescriptor = InvalidFd;
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

    if (static_cast<Platform*>(m_pPlatform)->IsProSemaphoreSupported())
    {
        m_semType = SemaphoreType::ProOnly;
    }

    // check sync object support status - with parital or complete features
    if (Settings().disableSyncObject == false)
    {
        CheckSyncObjectSupportStatus();

        // reconfigure Semaphore/Fence Type with m_syncobjSupportState.
        if (m_syncobjSupportState.syncobjSemaphore)
        {
            m_semType = SemaphoreType::SyncObj;

            if ((Settings().disableSyncobjFence == false) && m_syncobjSupportState.syncobjFence)
            {
                m_fenceType = FenceType::SyncObj;
            }
        }
    }

    // Current valid FenceType/SemaphoreType combination:
    // - Timestamp Fence + any Semaphore type.
    // - Syncobj Fence + Syncobj Semaphore.
    PAL_ASSERT((m_fenceType != FenceType::SyncObj) || (m_semType == SemaphoreType::SyncObj));

    if ((m_fenceType != FenceType::SyncObj) || (m_semType != SemaphoreType::SyncObj))
    {
        m_syncobjSupportState.timelineSemaphore = 0;
    }

    // DrmVersion should be equal or greater than 3.22 in case to support queue priority
    if (static_cast<Platform*>(m_pPlatform)->IsQueuePrioritySupported() && IsDrmVersionOrGreater(3,22))
    {
        m_featureState.supportQueuePriority = 1;
    }

    if (static_cast<Platform*>(m_pPlatform)->IsQueueIfhKmdSupported())
    {
        m_featureState.supportQueueIfhKmd = 1;
    }

    // Start to support per-vm bo from drm 3.20, but bugs were not fixed
    // until drm 3.25 on pro dkms stack or kernel 4.16 on upstream stack.
    if ((Settings().enableVmAlwaysValid == VmAlwaysValidForceEnable) ||
       ((Settings().enableVmAlwaysValid == VmAlwaysValidDefaultEnable) &&
       (IsDrmVersionOrGreater(3,25) || IsKernelVersionEqualOrGreater(4,16))))
    {
        m_featureState.supportVmAlwaysValid = 1;
    }

    if (IsDrmVersionOrGreater(3,25))
    {
        m_featureState.supportQuerySensorInfo = 1;
    }

    // The fix did not bump the kernel version, thus it is only safe to enable it start from the next version: 3.27
    // The fix also has been pulled into 4.18.rc1 upstream kernel already.
    if (IsDrmVersionOrGreater(3,27) || IsKernelVersionEqualOrGreater(4,18))
    {
        m_featureState.requirePrtReserveVaWa = 0;
    }
    else
    {
        m_featureState.requirePrtReserveVaWa = 1;
    }

    if ((static_cast<Platform*>(m_pPlatform)->IsRaw2SubmitSupported()) && (m_semType == SemaphoreType::SyncObj))
    {
        m_featureState.supportRaw2Submit = 1;
    }

    // When using amdgpu_cs_submit_raw to submit raw IBs, amdgpu_bo_list_handle will be directly passed to DRM.
    // After switching to amdgpu_cs_submit_raw2, amdgpu_bo_handles will be passed to DRM with one of amdgpu_cs_chunks.
    // The field to save amdgpu_bo_list_handle in the old interface amdgpu_cs_submit_raw is updated to a uint value in
    // amdgpu_cs_submit_raw2, which is supposed to be 0 and never initialized.
    // Unless DRM version is under 3.27, that uint value will be re-enabled. In this case, amdgpu_bo_list_create_raw
    // will be used to convert the amdgpu_bo_handles to a uint handle.
    if (IsDrmVersionOrGreater(3,27))
    {
        m_featureState.useBoListCreate = 0;
    }
    else
    {
        m_featureState.useBoListCreate = 1;
    }

    // Context IOCTL stable pstate interface was introduced from drm 3.45,
    // but kernel bugs was not fixed until 3.49
    if (IsDrmVersionOrGreater(3, 49))
    {
        m_featureState.supportPowerDpmIoctl = 1;
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
            result = GetSvmMgr()->Init(&m_memoryProperties.vaRange[static_cast<uint32>(VaPartition::Svm)]);
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

    m_chipProperties.hwIpFlags.u32All = ipLevels.flags.u32All;

    Result result = VamMgrSingleton::Init();

    // Init paths
    InitOutputPaths();

    if (result == Result::Success)
    {
        // Step 1: try default(as well as global) path
        result = m_settingsMgr.Init(m_pSettingsPath);

        // Step 2: if no global setting found, try XDG_CONFIG_HOME and user specific path
        if (result == Result::ErrorUnavailable)
        {
            const char* pXdgConfigPath = getenv("XDG_CONFIG_HOME");
            if (pXdgConfigPath != nullptr)
            {
                result = m_settingsMgr.Init(pXdgConfigPath);
            }
            else
            {
                // XDG_CONFIG_HOME is not set, fall back to $HOME
                char userDefaultConfigFilePath[MaxPathStrLen];

                const char* pPath = getenv("HOME");
                if (pPath != nullptr)
                {
                    Snprintf(userDefaultConfigFilePath, sizeof(userDefaultConfigFilePath), "%s%s",
                             pPath, UserDefaultConfigFileSubPath);
                    result = m_settingsMgr.Init(userDefaultConfigFilePath);
                }
                else
                {
                    result = Result::ErrorUnavailable;
                }
            }
        }

        if (result == Result::ErrorUnavailable)
        {
            // Unavailable means that the file was not found, which is an acceptable failure.
            PAL_DPINFO("No settings file loaded.");
            result = Result::Success;
        }
    }

    if (result == Result::Success)
    {
        result = InitGpuProperties();
    }

    if (result == Result::Success)
    {
        result = InitTmzHeapProperties();
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
    if (result == Result::Success)
    {
        result = GetScreens(&m_attachedScreenCount, nullptr, nullptr);
    }

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

    static_assert(MaxIbsPerSubmit >= MinCmdStreamsPerSubmission,
                  "The minimum supported number of command streams per submission is not enough for PAL!");
    if (Settings().maxNumCmdStreamsPerSubmit == 0)
    {
        m_queueProperties.maxNumCmdStreamsPerSubmit = MaxIbsPerSubmit;
    }
    else
    {
        m_queueProperties.maxNumCmdStreamsPerSubmit =
            Max<uint32>(MinCmdStreamsPerSubmission, Min<uint32>(MaxIbsPerSubmit,
                                                                Settings().maxNumCmdStreamsPerSubmit));
    }

    // Disable mid command buffer preemption on the DMA and Universal Engines if the setting has the feature disabled.
    // Furthermore, if the KMD does not support at least seven UDMA buffers per submission, we cannot support preemption
    // on the Universal Engine.
    //
    // Doing this while the KMD has enabled MCBP can cause corruption or hangs on other drivers. The only safe way to
    // disable MCBP is to fully enable everything like it's on and then disable preemption in the workload CmdStream.
    const bool fullyDisableMcbp = (Settings().cmdBufPreemptionMode == CmdBufPreemptModeFullDisableUnsafe);

    if (fullyDisableMcbp || (m_queueProperties.maxNumCmdStreamsPerSubmit < 7))
    {
        m_engineProperties.perEngine[EngineTypeUniversal].flags.supportsMidCmdBufPreemption = 0;
        m_engineProperties.perEngine[EngineTypeUniversal].contextSaveAreaSize               = 0;
        m_engineProperties.perEngine[EngineTypeUniversal].contextSaveAreaAlignment          = 0;
    }

    if (fullyDisableMcbp)
    {
        m_engineProperties.perEngine[EngineTypeDma].flags.supportsMidCmdBufPreemption = 0;
        m_engineProperties.perEngine[EngineTypeDma].contextSaveAreaSize               = 0;
        m_engineProperties.perEngine[EngineTypeDma].contextSaveAreaAlignment          = 0;
    }

    if (m_memoryProperties.flags.supportsTmz)
    {
        m_engineProperties.perEngine[EngineTypeUniversal].tmzSupportLevel     = TmzSupportLevel::PerSubmission;
        if (SupportCsTmz())
        {
            m_engineProperties.perEngine[EngineTypeCompute].tmzSupportLevel   = TmzSupportLevel::PerQueue;
        }
        else
        {
            m_engineProperties.perEngine[EngineTypeCompute].tmzSupportLevel   = TmzSupportLevel::None;
        }
        m_engineProperties.perEngine[EngineTypeDma].tmzSupportLevel           = TmzSupportLevel::PerCommandOp;
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
        pInfo->osProperties.supportOpaqueFdSemaphore = ((m_semType == SemaphoreType::ProOnly) ||
                                                        (m_semType == SemaphoreType::SyncObj));
        // Todo: Implement the sync file import/export upon sync object.
        pInfo->osProperties.supportSyncFileSemaphore = false;

        pInfo->osProperties.supportSyncFileSemaphore = (m_semType == SemaphoreType::SyncObj);
        pInfo->osProperties.supportSyncFileFence     = (m_fenceType == FenceType::SyncObj);

        pInfo->osProperties.timelineSemaphore.support                 = m_syncobjSupportState.timelineSemaphore;
        pInfo->osProperties.timelineSemaphore.supportHostQuery        = m_syncobjSupportState.timelineSemaphore;
        pInfo->osProperties.timelineSemaphore.supportHostWait         = m_syncobjSupportState.timelineSemaphore;
        pInfo->osProperties.timelineSemaphore.supportHostSignal       = m_syncobjSupportState.timelineSemaphore;
        pInfo->osProperties.timelineSemaphore.supportWaitBeforeSignal = false;

        pInfo->osProperties.supportQueuePriority = (m_featureState.supportQueuePriority != 0);
        // Linux don't support changing the queue priority at the submission granularity.
        pInfo->osProperties.supportDynamicQueuePriority = false;

        // Expose available time domains for calibrated timestamps
        pInfo->osProperties.timeDomains.supportDevice                  = true;
        pInfo->osProperties.timeDomains.supportClockMonotonic          = true;
        pInfo->osProperties.timeDomains.supportClockMonotonicRaw       = true;
        pInfo->osProperties.timeDomains.supportQueryPerformanceCounter = false;

        pInfo->osProperties.flags.hasPrimaryDrmNode = m_drmNodeProperties.flags.hasPrimaryDrmNode;
        pInfo->osProperties.flags.hasRenderDrmNode  = m_drmNodeProperties.flags.hasRenderDrmNode;
        pInfo->osProperties.primaryDrmNodeMajor     = m_drmNodeProperties.primaryDrmNodeMajor;
        pInfo->osProperties.primaryDrmNodeMinor     = m_drmNodeProperties.primaryDrmNodeMinor;
        pInfo->osProperties.renderDrmNodeMajor      = m_drmNodeProperties.renderDrmNodeMajor;
        pInfo->osProperties.renderDrmNodeMinor      = m_drmNodeProperties.renderDrmNodeMinor;

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

    m_chipProperties.familyId   = m_gpuInfo.family_id;
    m_chipProperties.eRevId     = m_gpuInfo.chip_external_rev;
    m_chipProperties.revisionId = m_gpuInfo.pci_rev_id;
    m_chipProperties.deviceId   = m_gpuInfo.asic_id;
    m_chipProperties.gpuIndex   = 0; // Multi-GPU is not supported so far.

    m_chipProperties.imageProperties.minPitchAlignPixel = 0;

    // ceRamSize must be set before InitializeGpuEngineProperties which reference it
    m_chipProperties.gfxip.ceRamSize = m_gpuInfo.ce_ram_size;

    // The unit of amdgpu is KHz but PAL is Hz
    m_chipProperties.gpuCounterFrequency = m_gpuInfo.gpu_counter_freq * 1000;

    // The unit of amdgpu is KHz but PAL is MHz
    m_chipProperties.maxEngineClock      = m_gpuInfo.max_engine_clk / 1000;
    m_chipProperties.maxMemoryClock      = m_gpuInfo.max_memory_clk / 1000;

    uint32 meFwVersion = 0;
    uint32 meFwFeature = 0;
    int32 drmRet = m_drmProcs.pfnAmdgpuQueryFirmwareVersion(m_hDevice,
                                                             AMDGPU_INFO_FW_GFX_ME,
                                                             0,
                                                             0,
                                                             &meFwVersion,
                                                             &meFwFeature);
    PAL_ASSERT(drmRet == 0);

    uint32 pfpFwVersion = 0;
    uint32 pfpFwFeature = 0;
    drmRet = m_drmProcs.pfnAmdgpuQueryFirmwareVersion(m_hDevice,
                                                      AMDGPU_INFO_FW_GFX_PFP,
                                                      0,
                                                      0,
                                                      &pfpFwVersion,
                                                      &pfpFwFeature);
    PAL_ASSERT(drmRet == 0);

    // Feature versions are assumed to be the same within the CP.
    m_chipProperties.cpUcodeVersion    = meFwFeature;
    m_chipProperties.pfpUcodeVersion   = pfpFwVersion;

    const char* pMarketingName = m_drmProcs.pfnAmdgpuGetMarketingNameisValid() ?
                                 m_drmProcs.pfnAmdgpuGetMarketingName(m_hDevice) : nullptr;
    if ((pMarketingName != nullptr) && (IsSpoofed() == false))
    {
        Strncpy(&m_gpuName[0], pMarketingName, sizeof(m_gpuName));
    }
    else
    {
        Strncpy(&m_gpuName[0], "Unknown AMD GPU", sizeof(m_gpuName));
    }

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

    // ToDo: Retrieve ceram size of gfx engine from kmd, but the functionality is not supported yet.
    switch (m_chipProperties.gfxLevel)
    {
    case GfxIpLevel::GfxIp6:
    case GfxIpLevel::GfxIp7:
    case GfxIpLevel::GfxIp8:
    case GfxIpLevel::GfxIp8_1:
        m_chipProperties.gfxEngineId = CIASICIDGFXENGINE_SOUTHERNISLAND;
        m_pFormatPropertiesTable     = Gfx6::GetFormatPropertiesTable(m_chipProperties.gfxLevel);
        InitGfx6ChipProperties();
        Gfx6::InitializeGpuEngineProperties(m_chipProperties,
                                            &m_engineProperties);
        break;
    case GfxIpLevel::GfxIp10_1:
    case GfxIpLevel::GfxIp9:
    case GfxIpLevel::GfxIp10_3:
#if PAL_BUILD_GFX11
    case GfxIpLevel::GfxIp11_0:
#endif
        m_chipProperties.gfxEngineId = CIASICIDGFXENGINE_ARCTICISLAND;
        m_pFormatPropertiesTable     = Gfx9::GetFormatPropertiesTable(m_chipProperties.gfxLevel,
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

    Result result = InitMemInfo();

    // InitSettings() relies on chipProperties because of heapPerf, so it must be called after chipProperties is
    // initialized. InitQueueInfo() relies on settings to disable DMA, so InitSettings() must be called prior to
    // set engine information.
    // InitSettings() relies on m_memoryProperties.largePageSupport.minSurfaceSizeForAlignmentInBytes to set
    // m_publicSettings.largePageMinSizeForAlignmentInBytes. So it must be called afeter InitMemInfo.
    if (result == Result::Success)
    {
        result = InitSettings();
    }

    if (result == Result::Success)
    {
        result = InitQueueInfo();
    }

    if (result == Result::Success)
    {
        m_engineProperties.perEngine[EngineTypeUniversal].availableCeRamSize = m_gpuInfo.ce_ram_size;

        InitPerformanceRatings();
        InitMemoryHeapProperties();
    }

    return result;

}
// =====================================================================================================================
// Hardware support determines which heaps can support TMZ.
Result Device::InitTmzHeapProperties()
{
    Result result = Result::Success;
    // Init Tmz state of each heaps.
    m_heapProperties[GpuHeapInvisible].flags.supportsTmz     = 0;
    m_heapProperties[GpuHeapLocal].flags.supportsTmz         = 0;
    m_heapProperties[GpuHeapGartUswc].flags.supportsTmz      = 0;
    m_heapProperties[GpuHeapGartCacheable].flags.supportsTmz = 0;

    m_memoryProperties.flags.supportsTmz = m_memoryProperties.flags.supportsTmz && Settings().tmzEnabled;
    // set the heap support for protected region
    if (m_memoryProperties.flags.supportsTmz)
    {
        if (IsRavenFamily(*this))
        {
            m_heapProperties[GpuHeapInvisible].flags.supportsTmz     = 1;
            m_heapProperties[GpuHeapLocal].flags.supportsTmz         = 1;

            if (MemoryProperties().flags.iommuv2Support == false)
            {
                m_heapProperties[GpuHeapGartUswc].flags.supportsTmz      = 1;
                m_heapProperties[GpuHeapGartCacheable].flags.supportsTmz = 1;
            }
        }
        else if (IsNavi1x(*this))
        {
            m_heapProperties[GpuHeapInvisible].flags.supportsTmz     = 1;
            m_heapProperties[GpuHeapLocal].flags.supportsTmz         = 1;
            m_heapProperties[GpuHeapGartUswc].flags.supportsTmz      = 0;
            m_heapProperties[GpuHeapGartCacheable].flags.supportsTmz = 0;
        }
        else if (IsMendocino(*this))
        {
            m_heapProperties[GpuHeapInvisible].flags.supportsTmz     = 1;
            m_heapProperties[GpuHeapLocal].flags.supportsTmz         = 1;

            if (MemoryProperties().flags.iommuv2Support == false)
            {
                m_heapProperties[GpuHeapGartUswc].flags.supportsTmz      = 1;
                m_heapProperties[GpuHeapGartCacheable].flags.supportsTmz = 1;
            }
        }
        else if (IsGfx10Plus(*this))
        {
            // All GFX10+ chips support page based local TMZ memory at least.
            m_heapProperties[GpuHeapInvisible].flags.supportsTmz     = 1;
            m_heapProperties[GpuHeapLocal].flags.supportsTmz         = 1;
            m_heapProperties[GpuHeapGartUswc].flags.supportsTmz      = 0;
            m_heapProperties[GpuHeapGartCacheable].flags.supportsTmz = 0;
        }
        else
        {
            result = Pal::Result::ErrorUnknown;
            PAL_NOT_IMPLEMENTED();
        }

        //Assert that at least one heap is claimed to support TMZ/VPR from KMD if we're here
        PAL_ASSERT(m_heapProperties[GpuHeapInvisible].flags.supportsTmz ||
                   m_heapProperties[GpuHeapLocal].flags.supportsTmz     ||
                   m_heapProperties[GpuHeapGartUswc].flags.supportsTmz  ||
                   m_heapProperties[GpuHeapGartCacheable].flags.supportsTmz);
    }
    return result;
}

// =====================================================================================================================
// Helper method which tests validity of cu_ao_bitmap in device information structure.
static bool TestCuAlwaysOnBitmap(
    struct drm_amdgpu_info_device* pDeviceInfo)
{
    bool result = false;

    for (uint32 seIndex = 0; (result == false) && (seIndex < pDeviceInfo->num_shader_engines); seIndex++)
    {
        for (uint32 shIndex = 0; shIndex < pDeviceInfo->num_shader_arrays_per_engine; shIndex++)
        {
#if PAL_BUILD_GFX11
            // The cu_bitmap is a 4x4 array, so Linux KMD uses cu_bitmap[][2] and cu_bitmap[][3] to represent the mask
            // of SEs > 4 like this:
            //      |SE0 SH0|SE0 SH1|SE4 SH0|SE4 SH1|
            //      |SE1 SH0|SE1 SH1|SE5 SH0|SE5 SH1|
            //      |SE2 SH0|SE2 SH1|...............
            //      |SE3 SH0|SE3 SH1|...............
            if (pDeviceInfo->cu_ao_bitmap[seIndex % 4][shIndex + 2 * (seIndex / 4)] != 0)
#else
            if (pDeviceInfo->cu_ao_bitmap[seIndex][shIndex] != 0)
#endif
            {
                result = true;
                break;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Helper method which initializes the GPU chip properties for all hardware families using the GFX6 hardware layer.
void Device::InitGfx6ChipProperties()
{
    auto*const                    pChipInfo  = &m_chipProperties.gfx6;
    struct drm_amdgpu_info_device deviceInfo = {};

    memcpy(&pChipInfo->gbTileMode[0], &m_gpuInfo.gb_tile_mode[0], sizeof(pChipInfo->gbTileMode));
    memcpy(&pChipInfo->gbMacroTileMode[0], &m_gpuInfo.gb_macro_tile_mode[0], sizeof(pChipInfo->gbMacroTileMode));

    Gfx6::InitializeGpuChipProperties(m_chipProperties.cpUcodeVersion, &m_chipProperties);

    // Any chip info from the KMD does not apply to a spoofed chip and should be ignored
    if (IsSpoofed() == false)
    {
        if (!m_drmProcs.pfnAmdgpuBoVaOpRawisValid())
        {
            m_chipProperties.imageProperties.prtFeatures = static_cast<PrtFeatureFlags>(0);
        }

        // It should be per engine, but PAL does not. So just use the first one.
        pChipInfo->backendDisableMask = m_gpuInfo.backend_disable[0];
        pChipInfo->paScRasterCfg      = m_gpuInfo.pa_sc_raster_cfg[0];
        pChipInfo->paScRasterCfg1     = m_gpuInfo.pa_sc_raster_cfg1[0];

        uint32 spiConfigCntl = 0;
        ReadRegisters(Gfx6::mmSPI_CONFIG_CNTL, 1, 0xffffffff, 0, &spiConfigCntl);
        pChipInfo->sqgEventsEnabled = ((spiConfigCntl & Gfx6::SPI_CONFIG_CNTL__ENABLE_SQG_TOP_EVENTS_MASK) &&
                                    (spiConfigCntl & Gfx6::SPI_CONFIG_CNTL__ENABLE_SQG_BOP_EVENTS_MASK));

        pChipInfo->gbAddrConfig             = m_gpuInfo.gb_addr_cfg;
        pChipInfo->mcArbRamcfg              = m_gpuInfo.mc_arb_ramcfg;

        pChipInfo->numShaderEngines = m_gpuInfo.num_shader_engines;
        pChipInfo->numShaderArrays  = m_gpuInfo.num_shader_arrays_per_engine;

        switch (m_chipProperties.gfxLevel)
        {
        case GfxIpLevel::GfxIp6:
        case GfxIpLevel::GfxIp7:
            ReadRegisters(Gfx6::mmSQ_THREAD_TRACE_MASK__SI__CI, 1,  0xffffffff, 0, &pChipInfo->sqThreadTraceMask);
            break;
        case GfxIpLevel::GfxIp8:
        case GfxIpLevel::GfxIp8_1:
            ReadRegisters(Gfx6::mmSQ_THREAD_TRACE_MASK__VI, 1, 0xffffffff, 0, &pChipInfo->sqThreadTraceMask);
            break;
        default:
            PAL_ASSERT_ALWAYS();
            break;
        }

        if (m_drmProcs.pfnAmdgpuQueryInfo(m_hDevice, AMDGPU_INFO_DEV_INFO, sizeof(deviceInfo), &deviceInfo) == 0)
        {
            pChipInfo->doubleOffchipLdsBuffers = deviceInfo.gc_double_offchip_lds_buf;
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }

        InitGfx6CuMask(&deviceInfo);
    }
    else
    {
#if PAL_BUILD_NULL_DEVICE
        NullDevice::Device::FillGfx6ChipProperties(&m_chipProperties);
#else
        PAL_ASSERT_ALWAYS_MSG("NullDevice spoofing requested but not compiled in!");
#endif
    }

    Gfx6::FinalizeGpuChipProperties(*this, &m_chipProperties);
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
void Device::InitGfx6CuMask(
    struct drm_amdgpu_info_device* pDeviceInfo)
{
    auto*const pChipInfo = &m_chipProperties.gfx6;

    PAL_ASSERT(pDeviceInfo != nullptr);
    const bool hasValidAoBitmap = TestCuAlwaysOnBitmap(pDeviceInfo);

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
                pChipInfo->alwaysOnCuMaskGfx6[seIndex][shIndex] =
                    hasValidAoBitmap ? pDeviceInfo->cu_ao_bitmap[seIndex][shIndex] : aoMask;
            }
            else
            {
                pChipInfo->activeCuMaskGfx7[seIndex]   = m_gpuInfo.cu_bitmap[seIndex][shIndex];
                pChipInfo->alwaysOnCuMaskGfx7[seIndex] =
                    hasValidAoBitmap ? pDeviceInfo->cu_ao_bitmap[seIndex][shIndex] : aoSeMask;
            }
        }
    }
}

// =====================================================================================================================
// Helper method which initializes the GPU chip properties for all hardware families using the GFX9 hardware layer.
void Device::InitGfx9ChipProperties()
{
    auto*const                    pChipInfo  = &m_chipProperties.gfx9;
    struct drm_amdgpu_info_device deviceInfo = {};

    // Call into the HWL to initialize the default values for many properties of the hardware (based on chip ID).
    Gfx9::InitializeGpuChipProperties(GetPlatform(), m_chipProperties.cpUcodeVersion, &m_chipProperties);

    // Any chip info from the KMD does not apply to a spoofed chip and should be ignored
    if (IsSpoofed() == false)
    {
        if (!m_drmProcs.pfnAmdgpuBoVaOpRawisValid())
        {
            m_chipProperties.imageProperties.prtFeatures = static_cast<PrtFeatureFlags>(0);
        }

        if (((m_chipProperties.imageProperties.flags.supportDisplayDcc == 1) &&
            (IsDrmVersionOrGreater(3, 34) == false))
            )
        {
            m_chipProperties.imageProperties.flags.supportDisplayDcc = 0;
        }
        pChipInfo->gbAddrConfig = m_gpuInfo.gb_addr_cfg;

        if (m_drmProcs.pfnAmdgpuQueryInfo(m_hDevice, AMDGPU_INFO_DEV_INFO, sizeof(deviceInfo), &deviceInfo) == 0)
        {
            pChipInfo->numShaderEngines         = deviceInfo.num_shader_engines;
            pChipInfo->numShaderArrays          = deviceInfo.num_shader_arrays_per_engine;
            pChipInfo->maxNumRbPerSe            = deviceInfo.num_rb_pipes / deviceInfo.num_shader_engines;
            pChipInfo->nativeWavefrontSize      = deviceInfo.wave_front_size;
            pChipInfo->numPhysicalVgprsPerSimd  = deviceInfo.num_shader_visible_vgprs;
            pChipInfo->maxNumCuPerSh            = deviceInfo.num_cu_per_sh;
            pChipInfo->numTccBlocks             = deviceInfo.num_tcc_blocks;
            pChipInfo->gsVgtTableDepth          = deviceInfo.gs_vgt_table_depth;
            pChipInfo->gsPrimBufferDepth        = deviceInfo.gs_prim_buffer_depth;
            pChipInfo->maxGsWavesPerVgt         = deviceInfo.max_gs_waves_per_vgt;
            pChipInfo->doubleOffchipLdsBuffers  = deviceInfo.gc_double_offchip_lds_buf;
            pChipInfo->paScTileSteeringOverride = 0;
            pChipInfo->sdmaL2PolicyValid        = false;
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }

        if ((m_drmProcs.pfnAmdgpuVmReserveVmidisValid() || m_drmProcs.pfnAmdgpuCsReservedVmidisValid()) &&
            (m_drmProcs.pfnAmdgpuVmUnreserveVmidisValid() || m_drmProcs.pfnAmdgpuCsUnreservedVmidisValid()))
        {
            m_chipProperties.gfxip.supportStaticVmid = 1;
        }

        if (IsGfx10(m_chipProperties.gfxLevel))
        {
            // We should probably ask that GPU__GC__NUM_TCP_PER_SA, GPU__GC__NUM_WGP0_PER_SA, and GPU__GC__NUM_WGP1_PER_SA
            // be added to drm_amdgpu_info_device. For now use the hard-coded WGP defaults and assume 2 TCPs per WGP.
            const uint32 wgpPerSa = pChipInfo->gfx10.numWgpAboveSpi + pChipInfo->gfx10.numWgpBelowSpi;

            // If this triggers we probably didn't give this ASIC a hard-coded default WGP count.
            PAL_ASSERT(wgpPerSa > 0);

            pChipInfo->gfx10.numTcpPerSa = 2 * wgpPerSa;
        }
#if PAL_BUILD_GFX11
        else if (IsGfx11(m_chipProperties.gfxLevel))
        {
            pChipInfo->gfx10.numTcpPerSa    =  8; // GC__NUM_TCP_PER_SA
            pChipInfo->gfx10.numWgpAboveSpi =  4; // GC__NUM_WGP0_PER_SA
            pChipInfo->gfx10.numWgpBelowSpi =  0; // GC__NUM_WGP1_PER_SA
        }
#endif

        InitGfx9CuMask(&deviceInfo);
    }
    else
    {
#if PAL_BUILD_NULL_DEVICE
        NullDevice::Device::FillGfx9ChipProperties(&m_chipProperties);
#else
        PAL_ASSERT_ALWAYS_MSG("NullDevice spoofing requested but not compiled in!");
#endif
    }

    // Call into the HWL to finish initializing some GPU properties which can be derived from the ones which we
    // overrode above.
    Gfx9::FinalizeGpuChipProperties(*this, &m_chipProperties);

    if (IsSpoofed() == false)
    {
        pChipInfo->numActiveRbs = CountSetBits(m_gpuInfo.enabled_rb_pipes_mask);

        pChipInfo->backendDisableMask = (~m_gpuInfo.enabled_rb_pipes_mask) & ((1 << pChipInfo->numTotalRbs) - 1);
    }

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
void Device::InitGfx9CuMask(
    struct drm_amdgpu_info_device* pDeviceInfo)
{
    auto*const pChipInfo = &m_chipProperties.gfx9;

    PAL_ASSERT(pDeviceInfo != nullptr);
    const bool hasValidAoBitmap = TestCuAlwaysOnBitmap(pDeviceInfo);

    for (uint32 seIndex = 0; seIndex < m_gpuInfo.num_shader_engines; seIndex++)
    {
        for (uint32 shIndex = 0; shIndex < m_gpuInfo.num_shader_arrays_per_engine; shIndex++)
        {
#if PAL_BUILD_GFX11
            // The cu_bitmap is a 4x4 array, so Linux KMD uses cu_bitmap[][2] and cu_bitmap[][3] to represent the mask
            // of SEs > 4 like this:
            //      |SE0 SH0|SE0 SH1|SE4 SH0|SE4 SH1|
            //      |SE1 SH0|SE1 SH1|SE5 SH0|SE5 SH1|
            //      |SE2 SH0|SE2 SH1|...............
            //      |SE3 SH0|SE3 SH1|...............
            pChipInfo->activeCuMask[seIndex][shIndex] =
                m_gpuInfo.cu_bitmap[seIndex - 4 * (seIndex / 4)][shIndex + 2 * (seIndex / 4)];

            if (hasValidAoBitmap)
            {
                pChipInfo->alwaysOnCuMask[seIndex][shIndex] =
                    pDeviceInfo->cu_ao_bitmap[seIndex - 4 * (seIndex / 4)][shIndex + 2 * (seIndex / 4)];
            }
            // For Gfx11, the concept of always on CUs is dropped, and the Gfx core is either ON or OFF entirely
            // So we can treat all active CUs as always on CUs on Gfx11
            else if (IsGfx11(m_chipProperties.gfxLevel))
            {
                pChipInfo->alwaysOnCuMask[seIndex][shIndex] = pChipInfo->activeCuMask[seIndex][shIndex];
            }
#else
            pChipInfo->activeCuMask[seIndex][shIndex] = m_gpuInfo.cu_bitmap[seIndex][shIndex];

            if (hasValidAoBitmap)
            {
                pChipInfo->alwaysOnCuMask[seIndex][shIndex] = pDeviceInfo->cu_ao_bitmap[seIndex][shIndex];
            }
#endif
            else
            {
                constexpr uint32 AlwaysOnSeMaskSize = 16;
                constexpr uint32 AlwaysOnSeMask     = (1ul << AlwaysOnSeMaskSize) - 1;

                const uint32 aoSeMask = (m_gpuInfo.cu_ao_mask >> (seIndex * AlwaysOnSeMaskSize)) & AlwaysOnSeMask;

                pChipInfo->alwaysOnCuMask[seIndex][shIndex] = aoSeMask;
            }
        }
    }

    if (IsGfx10Plus(m_chipProperties.gfxLevel))
    {
        // We start by assuming that the most WGP per SA that we get will are the feature defines.
        pChipInfo->gfx10.minNumWgpPerSa = pChipInfo->gfx10.numWgpAboveSpi + pChipInfo->gfx10.numWgpBelowSpi;
        pChipInfo->gfx10.maxNumWgpPerSa = 1;
        PAL_ASSERT(pChipInfo->gfx10.minNumWgpPerSa != 0);

        // In GFX 10, we need convert CU mask to WGP mask.
        for (uint32 seIndex = 0; seIndex < m_gpuInfo.num_shader_engines; seIndex++)
        {
            for (uint32 shIndex = 0; shIndex < m_gpuInfo.num_shader_arrays_per_engine; shIndex++)
            {
                pChipInfo->gfx10.activeWgpMask[seIndex][shIndex] = 0;
                pChipInfo->gfx10.alwaysOnWgpMask[seIndex][shIndex] = 0;
                // For gfx10 each WGP has two CU's, so we'll convert the bit masks(0x3->0x1) accordingly:
                // CuMask(32 bits) -> WGPmask(16 bits)
                for (uint32 cuIdx = 0; cuIdx < 32; cuIdx += 2)
                {
                    const uint32 cuBit = 3 << cuIdx;
                    const uint32 wgpMask  = 1 << (cuIdx >> 1);
                    if (TestAnyFlagSet(pChipInfo->activeCuMask[seIndex][shIndex], cuBit))
                    {
                        pChipInfo->gfx10.activeWgpMask[seIndex][shIndex] |= wgpMask;
                    }
                    if (TestAnyFlagSet(pChipInfo->alwaysOnCuMask[seIndex][shIndex], cuBit))
                    {
                        pChipInfo->gfx10.alwaysOnWgpMask[seIndex][shIndex] |= wgpMask;
                    }
                }

                const uint32 numActiveWgpPerSa = Util::CountSetBits(pChipInfo->gfx10.activeWgpMask[seIndex][shIndex]);
                if (numActiveWgpPerSa > 0)
                {
                    pChipInfo->gfx10.minNumWgpPerSa = Min(pChipInfo->gfx10.minNumWgpPerSa, numActiveWgpPerSa);
                    pChipInfo->gfx10.maxNumWgpPerSa = Max(pChipInfo->gfx10.maxNumWgpPerSa, numActiveWgpPerSa);
                }
            }
        }

        PAL_ASSERT(pChipInfo->gfx10.maxNumWgpPerSa >= 1);
    }
}

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
            PAL_ALERT_ALWAYS();
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

    case AMDGPU_VRAM_TYPE_DDR4:
        {
            result = LocalMemoryType::Ddr4;
            break;
        }

    case AMDGPU_VRAM_TYPE_DDR5:
        {
            result = LocalMemoryType::Ddr5;
            break;
        }

    case AMDGPU_VRAM_TYPE_GDDR5:
        {
            result = LocalMemoryType::Gddr5;
            break;
        }

    case AMDGPU_VRAM_TYPE_GDDR6:
        {
            result = LocalMemoryType::Gddr6;
            break;
        }

    case AMDGPU_VRAM_TYPE_HBM:
        {
            result = LocalMemoryType::Hbm;
            break;
        }

    case AMDGPU_VRAM_TYPE_LPDDR4:
        {
            result = LocalMemoryType::Lpddr4;
            break;
        }

    case AMDGPU_VRAM_TYPE_LPDDR5:
        {
            result = LocalMemoryType::Lpddr5;
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
// Helper method which initializes the GPU memory properties.
Result Device::InitMemInfo()
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

    uint64 startVa = 0;
    uint64 endVa   = 0;

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
        m_memoryProperties.vaUsableEnd  = m_memoryProperties.vaEnd;

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
                // supportsTmz flag might be overriden by panel settings in InitTmzHeapProperties().
                m_memoryProperties.flags.supportsTmz = (deviceInfo.ids_flags & AMDGPU_IDS_FLAGS_TMZ) ? 1 : 0;
            }
        }

        if (result == Result::Success)
        {
            result = VamMgrSingleton::GetVamMgr(this, &m_pVamMgr);
        }

        if (result == Result::Success)
        {
            m_memoryProperties.fragmentSize = sizeAlign.size_local;

            // The libdrm_amdgpu GPU memory interfaces map very nicely to PAL's interfaces; we can simply use
            // GpuPageSize for all allocation granularities and also for virtualMemPageSize.
            m_memoryProperties.realMemAllocGranularity    = GpuPageSize;
            m_memoryProperties.virtualMemAllocGranularity = GpuPageSize;
            m_memoryProperties.virtualMemPageSize         = GpuPageSize;

            // Calculate VA partitions
            result = FixupUsableGpuVirtualAddressRange(m_chipProperties.gfxip.vaRangeNumBits);
        }

        if (result == Result::Success)
        {
            result = m_pVamMgr->Finalize(this);
        }

        if (result == Result::Success)
        {
            m_memoryProperties.flags.virtualRemappingSupport = 1;
            m_memoryProperties.flags.pinningSupport          = 1; // Supported
            m_memoryProperties.flags.supportPerSubmitMemRefs = 1; // Supported
            m_memoryProperties.flags.globalGpuVaSupport      = 0; // Not supported
            m_memoryProperties.flags.svmSupport              = 1; // Supported
            m_memoryProperties.flags.autoPrioritySupport     = 0; // Not supported
            m_memoryProperties.flags.supportPageFaultInfo    = 0; // Not supported

            // Linux don't support High Bandwidth Cache Controller (HBCC) memory segment
            m_memoryProperties.hbccSizeInBytes   = 0;

            gpusize localHeapSize = 0;
            gpusize invisibleHeapSize = 0;

            if (m_drmProcs.pfnAmdgpuQueryInfo(m_hDevice, AMDGPU_INFO_MEMORY, sizeof(memInfo), &memInfo) != 0)
            {
                struct amdgpu_heap_info heap_info = {0};
                if (m_drmProcs.pfnAmdgpuQueryHeapInfo(m_hDevice,
                                                      AMDGPU_GEM_DOMAIN_VRAM,
                                                      AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED,
                                                      &heap_info) == 0)
                {
                    localHeapSize = heap_info.heap_size;
                }
                if (m_drmProcs.pfnAmdgpuQueryHeapInfo(m_hDevice, AMDGPU_GEM_DOMAIN_VRAM, 0, &heap_info) == 0)
                {
                    invisibleHeapSize = heap_info.heap_size;
                }
                if (m_drmProcs.pfnAmdgpuQueryHeapInfo(m_hDevice, AMDGPU_GEM_DOMAIN_GTT, 0, &heap_info) == 0)
                {
                    m_memoryProperties.nonLocalHeapSize  = heap_info.heap_size;
                }
            }
            else
            {
                localHeapSize     = memInfo.cpu_accessible_vram.total_heap_size;
                invisibleHeapSize = memInfo.vram.total_heap_size - localHeapSize;
                m_memoryProperties.nonLocalHeapSize = Pow2AlignDown(memInfo.gtt.total_heap_size,
                                                      m_memoryProperties.fragmentSize);
            }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 766
            m_heapProperties[GpuHeapLocal].logicalSize      = localHeapSize;
            m_heapProperties[GpuHeapLocal].physicalSize     = localHeapSize;
            m_memoryProperties.barSize                      = localHeapSize;
            m_heapProperties[GpuHeapInvisible].logicalSize  = invisibleHeapSize;
            m_heapProperties[GpuHeapInvisible].physicalSize = invisibleHeapSize;
#else
            m_heapProperties[GpuHeapLocal].heapSize             = localHeapSize;
            m_heapProperties[GpuHeapLocal].physicalHeapSize     = localHeapSize;
            m_heapProperties[GpuHeapInvisible].heapSize         = invisibleHeapSize;
            m_heapProperties[GpuHeapInvisible].physicalHeapSize = invisibleHeapSize;
#endif

            SystemInfo systemInfo = {};
            if (QuerySystemInfo(&systemInfo) == Result::Success)
            {
                // On the platform with VRAM bigger than system memory, kernel driver would return an incorrect
                // GTT heap size, which is bigger than system memory. So, workaround it before kernel has a fix.
                gpusize totalSysMemSize = static_cast<gpusize>(systemInfo.totalSysMemSize) * 1024 * 1024;
                if (GetPlatform()->PlatformSettings().overrideNonLocalHeapSize != 0)
                {
                    m_memoryProperties.nonLocalHeapSize = GetPlatform()->PlatformSettings().overrideNonLocalHeapSize;
                }
                else
                {
                    m_memoryProperties.nonLocalHeapSize = Min(totalSysMemSize, m_memoryProperties.nonLocalHeapSize);
                }
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

    return result;
}

// =====================================================================================================================
// Helper method which initializes the queue properties.
Result Device::InitQueueInfo()
{
    Result result = Result::Success;
    constexpr QueuePrioritySupport AnyPriority = static_cast<QueuePrioritySupport>(SupportQueuePriorityIdle   |
                                                                                   SupportQueuePriorityNormal |
                                                                                   SupportQueuePriorityMedium |
                                                                                   SupportQueuePriorityHigh   |
                                                                                   SupportQueuePriorityRealtime);

    const bool supportsMultiQueue = SupportsExplicitGang();

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
                    pEngineInfo->sizeAlignInDwords = Pow2Align(engineInfo.ib_size_alignment,
                                                               sizeof(uint32)) / sizeof(uint32);
                    for (uint32 engineIdx = 0; engineIdx < pEngineInfo->numAvailable; engineIdx++)
                    {
                        pEngineInfo->capabilities[engineIdx].queuePrioritySupport     = AnyPriority;
                        pEngineInfo->capabilities[engineIdx].maxFrontEndPipes         = 1;
                        pEngineInfo->capabilities[engineIdx].flags.supportsMultiQueue = supportsMultiQueue;
                    }
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
                    pEngineInfo->sizeAlignInDwords = Pow2Align(engineInfo.ib_size_alignment,
                                                               sizeof(uint32)) / sizeof(uint32);

                    uint32 engineIdx          = 0;
                    uint32 normalQueueSupport = AnyPriority;

                    for (; engineIdx < pEngineInfo->numAvailable; engineIdx++)
                    {
                        pEngineInfo->capabilities[engineIdx].queuePrioritySupport     = normalQueueSupport;
                        pEngineInfo->capabilities[engineIdx].flags.supportsMultiQueue = supportsMultiQueue;

                        // Kernel doesn't expose this info.
                        pEngineInfo->capabilities[engineIdx].maxFrontEndPipes = 1;
                    }
                }
                break;

            case EngineTypeDma:
                // GFX10+ parts have the DMA engine in the GFX block, not in the OSS, but any DMA engine
                // will report queue support before this is called.
                if ((Settings().disableSdmaEngine == false) &&
                    (TestAnyFlagSet(pEngineInfo->queueSupport, SupportQueueTypeDma)))
                {
                    if (m_drmProcs.pfnAmdgpuQueryHwIpInfo(m_hDevice, AMDGPU_HW_IP_DMA, 0, &engineInfo) != 0)
                    {
                        result = Result::ErrorInvalidValue;
                    }
                    pEngineInfo->numAvailable      = Util::CountSetBits(engineInfo.available_rings);
                    pEngineInfo->startAlign        = engineInfo.ib_start_alignment;
                    pEngineInfo->sizeAlignInDwords = Pow2Align(engineInfo.ib_size_alignment,
                                                               sizeof(uint32)) / sizeof(uint32);
                }
                break;

            case EngineTypeTimer:
                // NOTE: amdgpu doesn't support the Timer Queue.
                pEngineInfo->numAvailable      = 0;
                pEngineInfo->startAlign        = 8;
                pEngineInfo->sizeAlignInDwords = 1;
                break;

                // not supported on linux
                pEngineInfo->numAvailable       = 0;
                pEngineInfo->startAlign         = 1;
                pEngineInfo->sizeAlignInDwords  = 1;
                break;
            default:
                PAL_ASSERT_ALWAYS();
                break;
        }

        if ((pEngineInfo->numAvailable > 0) && (pEngineInfo->capabilities[0].queuePrioritySupport == 0))
        {
            // Give a default priority if there's not a more specific one provided.
            for (uint32 engineIdx = 0; engineIdx < pEngineInfo->numAvailable; engineIdx++)
            {
                pEngineInfo->capabilities[engineIdx].queuePrioritySupport = SupportQueuePriorityNormal;
                pEngineInfo->capabilities[engineIdx].maxFrontEndPipes     = 1;
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

        // This code is added here because it is entirely reliant on kernel level support for implicit/explicit
        // gang submit. As a result, this GFXIP-specific logic is being handled in InitQueueInfo.
        const bool supportsImplicitGangSubmit = SupportsExplicitGang() &&
                                                IsAceGfxGangSubmitSupported();

        const bool supportsExplicitGangSubmit = SupportsExplicitGang();

        if (IsGfx103PlusExclusive(m_chipProperties.gfxLevel))
        {
            m_chipProperties.gfx9.supportMeshShader =  m_chipProperties.gfx9.supportImplicitPrimitiveShader;
            m_chipProperties.gfx9.supportTaskShader = (m_chipProperties.gfx9.supportImplicitPrimitiveShader &&
                                                       (supportsImplicitGangSubmit || supportsExplicitGangSubmit));
        }
        m_chipProperties.gfxip.supportAceOffload = 0;

    }

    return result;
}

// =====================================================================================================================
// This is help methods. Init cache and debug file paths
void Device::InitOutputPaths()
{
    const char* pPath;

    // Initialize the root path of cache files
    // Cascade:
    // 1. Find AMD_SHADER_DISK_CACHE_PATH to keep backward compatibility.
    // 2. Find XDG_CACHE_HOME.
    // 3. If AMD_SHADER_DISK_CACHE_PATH and XDG_CACHE_HOME both not set,
    //    use "$HOME/.cache".
    pPath = getenv("AMD_SHADER_DISK_CACHE_PATH");

    if (pPath == nullptr)
    {
        pPath = getenv("XDG_CACHE_HOME");
    }

    if (pPath != nullptr)
    {
        Strncpy(m_cacheFilePath, pPath, sizeof(m_cacheFilePath));
    }
    else
    {
        pPath = getenv("HOME");
        if (pPath != nullptr)
        {
            Snprintf(m_cacheFilePath, sizeof(m_cacheFilePath), "%s%s", pPath, UserDefaultCacheFileSubPath);
        }
    }

    // Initialize the root path of debug files which is used to put all files
    // for debug purpose (such as logs, dumps, replace shader)
    // Cascade:
    // 1. Find AMD_DEBUG_DIR.
    // 2. Find TMPDIR.
    // 3. If AMD_DEBUG_DIR and TMPDIR both not set, use "/var/tmp"
    pPath = getenv("AMD_DEBUG_DIR");
    if (pPath == nullptr)
    {
        pPath = getenv("TMPDIR");
    }

    if (pPath == nullptr)
    {
        pPath = UserDefaultDebugFilePath;
    }

    Strncpy(m_debugFilePath, pPath, sizeof(m_debugFilePath));
}

// =====================================================================================================================
// Captures a GPU timestamp with the corresponding CPU timestamps, allowing tighter CPU/GPU timeline synchronization.
Result Device::GetCalibratedTimestamps(
    CalibratedTimestamps* pCalibratedTimestamps
    ) const
{
    Result result = Result::Success;

    if (pCalibratedTimestamps != nullptr)
    {
        uint64 gpuTimestamp = 0;
        uint64 cpuTimestampBeforeGpuTimestampRaw = GetPerfCpuTime(true);
        uint64 cpuTimestampBeforeGpuTimestamp = GetPerfCpuTime();

        if (m_drmProcs.pfnAmdgpuQueryInfo(m_hDevice, AMDGPU_INFO_TIMESTAMP, sizeof(gpuTimestamp), &gpuTimestamp) == 0)
        {
            uint64 cpuTimestampAfterGpuTimestampRaw = GetPerfCpuTime(true);
            uint64 cpuTimestampAfterGpuTimestamp = GetPerfCpuTime();

            uint64 maxDeviation = Max(cpuTimestampAfterGpuTimestamp - cpuTimestampBeforeGpuTimestamp,
                                      cpuTimestampAfterGpuTimestampRaw - cpuTimestampBeforeGpuTimestampRaw);

            pCalibratedTimestamps->gpuTimestamp                  = gpuTimestamp;
            pCalibratedTimestamps->cpuClockMonotonicTimestamp    = cpuTimestampBeforeGpuTimestamp;
            pCalibratedTimestamps->cpuClockMonotonicRawTimestamp = cpuTimestampBeforeGpuTimestampRaw;
            pCalibratedTimestamps->maxDeviation                  = maxDeviation;
        }
        else
        {
            // Unable to get a GPU timestamp, return error.
            result = Result::ErrorUnavailable;
        }
    }
    else
    {
        result = Result::ErrorInvalidPointer;
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
        const Device& otherLnxDevice = static_cast<const Device&>(otherDevice);
        pInfo->flags.u32All = 0;

        const PalSettings& settings = Settings();

        //Unlike windows,there is no concept of an LDA chain on Linux.
        //Linux can share resource, like memory and semaphore, across any supported devices.
        //And peer transfer is also supported in general.
        if (settings.mgpuCompatibilityEnabled)
        {
            pInfo->flags.sharedMemory = 1;
            pInfo->flags.sharedSync   = 1;
            if (settings.peerMemoryEnabled)
            {
                pInfo->flags.peerTransferWrite = 1;
            }
            if (settings.hwCompositingEnabled)
            {
                pInfo->flags.shareThisGpuScreen  = 1;
                pInfo->flags.shareOtherGpuScreen = 1;
            }
            if (m_chipProperties.gfxLevel == otherLnxDevice.ChipProperties().gfxLevel)
            {
                pInfo->flags.iqMatch = 1;
                if (m_chipProperties.deviceId == otherLnxDevice.ChipProperties().deviceId)
                {
                    pInfo->flags.gpuFeatures = 1;
                }
            }
        }

        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
size_t Device::GpuMemoryObjectSize() const
{
    return sizeof(Amdgpu::GpuMemory);
}

// =====================================================================================================================
Pal::GpuMemory* Device::ConstructGpuMemoryObject(
    void* pPlacementAddr)
{
    return PAL_PLACEMENT_NEW(pPlacementAddr) Amdgpu::GpuMemory(this);
}

// =====================================================================================================================
// Determines the size of Pal::Amdgpu::Queue, in bytes.
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
        // Add the size of Amdgpu::Queue::m_pResourceList or m_pResourceObjectList
        size = sizeof(Amdgpu::Queue) + CmdBufMemReferenceLimit * sizeof(this);

        if (createInfo.enableGpuMemoryPriorities)
        {
            // Add the size of Amdgpu::Queue::m_pResourcePriorityList
            size += CmdBufMemReferenceLimit * sizeof(uint8);
        }

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
Result Device::CreateDmaUploadRing()
{
    MutexAuto lock(&m_dmaUploadRingLock);

    Result result = Result::Success;

    if (m_pDmaUploadRing == nullptr)
    {
        m_pDmaUploadRing = PAL_NEW(DmaUploadRing, GetPlatform(), Util::SystemAllocType::AllocInternal)(this);

        if (m_pDmaUploadRing == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }

        if (result == Result::Success)
        {
            result = m_pDmaUploadRing->Init();
            if (result != Result::Success)
            {
                // It will call destructor of DmaUploadRing to free internal resources of m_pDmaUploadRing.
                PAL_SAFE_DELETE(m_pDmaUploadRing, m_pPlatform);
            }
        }
    }

    return result;
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
        pQueue = PAL_PLACEMENT_NEW(pPlacementAddr) Amdgpu::Queue(1, this, &createInfo);
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
size_t Device::MultiQueueObjectSize(
    uint32                 queueCount,
    const QueueCreateInfo* pCreateInfo
    ) const
{
    return QueueObjectSize(pCreateInfo[0]);
}

// =====================================================================================================================
Pal::Queue* Device::ConstructMultiQueueObject(
    uint32                 queueCount,
    const QueueCreateInfo* pCreateInfo,
    void*                  pPlacementAddr)
{
    PAL_ASSERT(queueCount > 0);
    // Make sure every queue supports HWS and every queue is QueueTypeCompute, QueueTypeUniversal, or QueueTypeDma.
    bool isMultiQueueType = true;
    Pal::Queue* pQueue    = nullptr;

    for (uint32 qIndex = 0; qIndex < queueCount; qIndex++)
    {
        switch (pCreateInfo[qIndex].queueType)
        {
        case QueueTypeCompute:
        case QueueTypeUniversal:
        case QueueTypeDma:
            break;
        default:
            // We don't expect a multiQueue would be of any other queue type at this stage.
            isMultiQueueType = false;
            break;
        }
    }

    if (isMultiQueueType)
    {
        switch (pCreateInfo[0].queueType)
        {
        case QueueTypeCompute:
        case QueueTypeUniversal:
        case QueueTypeDma:
            pQueue = PAL_PLACEMENT_NEW(pPlacementAddr) Amdgpu::Queue(queueCount, this, pCreateInfo);
            break;
        default:
            // We don't expect a multiQueue would be of any other queue type at this stage.
            PAL_ASSERT_ALWAYS();
            break;
        }
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

    ImageInternalCreateInfo internalInfo = {};
    ImageCreateInfo modifiedCreateInfo = createInfo;

    // [AMDVLK-179][X-plane]Vulkan does not properly synchronize with OpenGL in X-Plane 11.50
    // This issue root cause is AMDVLK and Mesa have different pipeBankXor for a shareable image.
    // This image is created by AMDVLK, and export to Mesa OGL.
    // In mesa, for a shareable image, mesa will not compute pipeBankXor, set it be zero.
    // Different pipeBankXor cause X-plane's third_party plugin dialog corruption, with mesa stack.
    // This fix only for Vulkan export image to other components on Linux Platform.
    // Not impact AMDVLK import external image.
    if (createInfo.flags.optimalShareable)
    {
        if (ChipProperties().gfxLevel >= GfxIpLevel::GfxIp9)
        {
            internalInfo.flags.useSharedTilingOverrides = 1;
            // PipeBankXor is zero initialized by internalInfo declaration
            // Do not override the swizzle mode value
            internalInfo.gfx9.sharedSwizzleMode = ADDR_SW_MAX_TYPE;
        }
        else
        {
            internalInfo.flags.useSharedTilingOverrides = 1;
            // Tile swizzle is zero initialized by internalInfo declaration
            // Do not override below values
            internalInfo.gfx6.sharedTileMode = ADDR_TM_COUNT;
            internalInfo.gfx6.sharedTileType = TileTypeInvalid;
            internalInfo.gfx6.sharedTileIndex = TileIndexUnused;
        }
    }

#if PAL_DISPLAY_DCC
    else if ((createInfo.flags.flippable == true) &&
             (createInfo.usageFlags.disableOptimizedDisplay == 0) &&
             (SupportDisplayDcc() == true) &&
             // VCAM_SURFACE_DESC does not support YUV presentable yet
             (Formats::IsYuv(createInfo.swizzledFormat.format) == false))
    {
        DisplayDccCaps displayDcc = { };

        GetDisplayDccInfo(displayDcc);
        PAL_ASSERT(displayDcc.dcc_256_128_128 ||
                   displayDcc.dcc_128_128_unconstrained ||
                   displayDcc.dcc_256_64_64);
        if (displayDcc.pipeAligned == 0)
        {
            internalInfo.displayDcc.value                 = displayDcc.value;
            internalInfo.displayDcc.enabled               = 1;
            modifiedCreateInfo.flags.optimalShareable     = 1;
        }
    }
#endif

    Result ret = CreateInternalImage(modifiedCreateInfo, internalInfo, pPlacementAddr, &pImage);
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
//Get Display Dcc Info
void Device::GetDisplayDccInfo(
    DisplayDccCaps& displayDcc
    ) const
{
    PAL_ASSERT(ChipProperties().imageProperties.flags.supportDisplayDcc == 1);
    if (m_gpuInfo.rb_pipes == 1)
    {
        displayDcc.pipeAligned = 1;
        displayDcc.rbAligned   = 1;
    }
    else
    {
        displayDcc.pipeAligned = 0;
        displayDcc.rbAligned   = 0;
        {
            // Refer to gfx9_compute_surface function of Mesa3d,if gfxLevel greater or equal to GfxIp10_3,
            // displaydcc parameter should be set to "Independent64=1, Independent128=1, maxCompress=64B"
            // to meet DCN requirements, therefore here dcc_256_64_64 should be set to 1.
            if (ChipProperties().gfxLevel >= GfxIpLevel::GfxIp10_3)
            {
                displayDcc.dcc_256_256_unconstrained = 0;
                displayDcc.dcc_256_128_128           = 0;
                displayDcc.dcc_128_128_unconstrained = 0;
                displayDcc.dcc_256_64_64             = 1;
            }
        }
    }
}

// =====================================================================================================================
// Returns true if gpu + amdgpu kms driver do support 16 bit floating point display.
bool Device::HasFp16DisplaySupport() const
{
    bool supported = false;

    // On Linux 5.8 (DRM 3.38) and later we also have the 64 bpp fp16 floating point format
    // on display engines of generation DCE 11.2 - DCE 12, and all DCN engines, iow. Polaris
    // and later.
    if ((IsDrmVersionOrGreater(3, 38) || IsKernelVersionEqualOrGreater(5, 8)) &&
        (IsGfx10Plus(m_chipProperties.gfxLevel) || IsGfx9(*this) ||
        (IsGfx8(*this) && (IsPolaris10(*this) || IsPolaris11(*this) || IsPolaris12(*this)))))
    {
        supported = true;
    }

    // On Linux 5.12 and later or DRM 3.41 and later we also have the fp16 floating point format
    // on all display engines since DCE 8.0, ie. additionally on Gfx7-DCE 8.x, Gfx8-10.0/11.0.
    if ((IsDrmVersionOrGreater(3, 41) || IsKernelVersionEqualOrGreater(5, 12)) &&
        (IsGfx8(*this) || IsGfx7(*this)))
    {
        supported = true;
    }

    return supported;
}

// =====================================================================================================================
// Returns true if gpu + amdgpu kms driver do support 16 bit unorm fixed point display.
bool Device::HasRgba16DisplaySupport() const
{
    bool supported = false;

    // On Linux 5.14 (DRM 3.42) and later we also have the 64 bpp rgba16 unorm fixed point format
    // on display engines of generation DCE 8.0 - DCE 12, and on all DCN engines, iow. Sea Islands
    // and later0. However, current pal no longer supports Sea Islands, so check for gfxLevel >= 8.
    if ((IsDrmVersionOrGreater(3, 42) || IsKernelVersionEqualOrGreater(5, 14)) &&
        (m_chipProperties.gfxLevel >= GfxIpLevel::GfxIp8))
    {
        supported = true;
    }

    return supported;
}

// =====================================================================================================================
// Swap chain information is related with OS window system, so get all of the information here.
Result Device::GetSwapChainInfo(
    OsDisplayHandle      hDisplay,
    OsWindowHandle       hWindow,
    WsiPlatform          wsiPlatform,
    SwapChainProperties* pSwapChainProperties)
{
    const uint32 baseFormatCount = static_cast<uint32>(ArrayLen(PresentableSwizzledFormat));

    // This is frequently, how many images must be in a swap chain in order for the app to acquire an image
    // in finite time if the app currently doesn't own an image.
    pSwapChainProperties->minImageCount = 2;

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

    // ISwapChain::SetHdrMetData interface is not supported
    pSwapChainProperties->colorSpaceCount = 0;

    // Get formats supported by swap chain. We have at least the 32 bpp formats.
    pSwapChainProperties->imageFormatCount = baseFormatCount;

    // Some gpu + amdgpu kms combinations do support fp16 scanout and display.
    if (HasFp16DisplaySupport())
    {
        PAL_ASSERT(pSwapChainProperties->imageFormatCount < MaxPresentableImageFormat);

        // fp16 is first slot in Presentable16BitSwizzledFormat[].
        pSwapChainProperties->imageFormatCount++;

        // All gpu's which support fp16 do also support rgba16 unorm with recent amdgpu kms.
        if (HasRgba16DisplaySupport())
        {
            PAL_ASSERT(pSwapChainProperties->imageFormatCount < MaxPresentableImageFormat);

            // rgba16 unorm is the second slot in Presentable16BitSwizzledFormat[].
            pSwapChainProperties->imageFormatCount++;
        }
    }

    for (uint32 i = 0; i < pSwapChainProperties->imageFormatCount; i++)
    {
        if (i < baseFormatCount)
        {
            pSwapChainProperties->imageFormat[i] = PresentableSwizzledFormat[i];
        }
        else
        {
            pSwapChainProperties->imageFormat[i] = Presentable16BitSwizzledFormat[i - baseFormatCount];
        }
    }

    // Get overrides and current window size (height, width) from window system.
    Result result = WindowSystem::GetWindowProperties(this,
                                                      wsiPlatform,
                                                      hDisplay,
                                                      hWindow,
                                                      pSwapChainProperties);

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
uint32 Device::GetSupportedSwapChainModes(
    WsiPlatform wsiPlatform,
    PresentMode mode
    ) const
{
    // The swap chain modes various from wsiPlatform to wsiPlatform. X and Wayland window system support immediate and
    // FIFO mode, and pal implements mailbox mode for both window systems.
    // DirectDisplay can directly render to a display without using intermediate window system, the display is exclusive
    // to a process, so it only has full screen mode. FIFO is the basic requirement for now, and it's the only mode
    // implemented by PAL, but immediate and mailbox modes can also be supported if necessary.

    uint32 swapchainModes = 0;
    if (mode == PresentMode::Windowed)
    {
        if (wsiPlatform != DirectDisplay)
        {
            swapchainModes = SupportImmediateSwapChain | SupportFifoSwapChain | SupportMailboxSwapChain;
        }
    }
    else
    {
        swapchainModes = SupportImmediateSwapChain | SupportFifoSwapChain | SupportMailboxSwapChain;
    }

    return swapchainModes;
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
// Call amdgpu to reserve/unreserve a vmid. The SPM_VMID will be updated right before any job is submitted to GPU if
// there is any VMID reserved.
Result Device::OsSetStaticVmidMode(
    bool enable)
{
    Result result = Result::Success;

    if (enable)
    {
        // Reserve a VMID
        if (m_drmProcs.pfnAmdgpuVmReserveVmidisValid())
        {
            result = CheckResult(m_drmProcs.pfnAmdgpuVmReserveVmid(m_hDevice, 0), Result::ErrorOutOfMemory);
        }
        else if (m_drmProcs.pfnAmdgpuCsReservedVmidisValid())
        {
            result = CheckResult(m_drmProcs.pfnAmdgpuCsReservedVmid(m_hDevice), Result::ErrorOutOfMemory);
        }
    }
    else
    {
        // Unreserve a VMID
        if (m_drmProcs.pfnAmdgpuVmUnreserveVmidisValid())
        {
            result = CheckResult(m_drmProcs.pfnAmdgpuVmUnreserveVmid(m_hDevice, 0), Result::ErrorOutOfMemory);
        }
        else if (m_drmProcs.pfnAmdgpuCsUnreservedVmidisValid())
        {
            result = CheckResult(m_drmProcs.pfnAmdgpuCsUnreservedVmid(m_hDevice), Result::ErrorOutOfMemory);
        }
    }

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

    static_assert(ArrayLen(MTypeTable) == static_cast<uint32>(MType::Count),
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

    uint64 operations = AMDGPU_VM_PAGE_PRT;

    // we have to enabl w/a to delay update the va mapping in case kernel did not ready with the fix.
    if (m_featureState.requirePrtReserveVaWa != 0)
    {
        operations |= AMDGPU_VM_DELAY_UPDATE;
    }

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
// Call amdgpu to create a command submission context, without checking global contexts
Result Device::CreateCommandSubmissionContextRaw(
    amdgpu_context_handle* pContextHandle,
    QueuePriority          priority,
    bool                   isTmzOnly
    ) const
{
    Result result = Result::Success;

    if ((SupportCsTmz() == false) && isTmzOnly)
    {
        result = Result::ErrorInvalidValue;
    }

    if (result == Result::Success)
    {
        if (m_featureState.supportQueuePriority != 0)
        {
            constexpr int32 QueuePriorityToAmdgpuPriority[] =
            {
                AMDGPU_CTX_PRIORITY_NORMAL,     // QueuePriority::Normal     = 0,
                AMDGPU_CTX_PRIORITY_LOW,        // QueuePriority::Idle       = 1,
                AMDGPU_CTX_PRIORITY_NORMAL,     // QueuePriority::Medium     = 2,
                AMDGPU_CTX_PRIORITY_HIGH,       // QueuePriority::High       = 3,
                AMDGPU_CTX_PRIORITY_VERY_HIGH,  // QueuePriority::Realtime   = 4,
            };

            static_assert((static_cast<uint32>(QueuePriority::Normal)   == 0) &&
                          (static_cast<uint32>(QueuePriority::Idle)     == 1) &&
                          (static_cast<uint32>(QueuePriority::Medium)   == 2) &&
                          (static_cast<uint32>(QueuePriority::High)     == 3) &&
                          (static_cast<uint32>(QueuePriority::Realtime) == 4),
                "The QueuePriorityToAmdgpuPriority table needs to be updated.");
            if (m_featureState.supportQueueIfhKmd != 0)
            {
                uint32 flags = 0;
                flags |= (Settings().ifh == IfhModeKmd) ? AMDGPU_CTX_FLAGS_IFH : 0;
                flags |= isTmzOnly ? AMDGPU_CTX_FLAGS_SECURE : 0;
                result = CheckResult(m_drmProcs.pfnAmdgpuCsCtxCreate3(m_hDevice,
                                                        QueuePriorityToAmdgpuPriority[static_cast<uint32>(priority)],
                                                        flags,
                                                        pContextHandle),
                                     Result::ErrorInvalidValue);
            }
            else
            {
                result = CheckResult(m_drmProcs.pfnAmdgpuCsCtxCreate2(m_hDevice,
                                                        QueuePriorityToAmdgpuPriority[static_cast<uint32>(priority)],
                                                        pContextHandle),
                                     Result::ErrorInvalidValue);
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

    return result;
}

// =====================================================================================================================
// Call amdgpu to create a command submission context.
Result Device::CreateCommandSubmissionContext(
    amdgpu_context_handle* pContextHandle,
    QueuePriority          priority,
    bool                   isTmzOnly
    )
{
    Result result = Result::Success;

    // Check if the global scheduling context isn't available and allocate a new one for each queue
    if (m_useSharedGpuContexts == false)
    {
        result = CreateCommandSubmissionContextRaw(pContextHandle, priority, isTmzOnly);
    }
    else
    {
        // If we're using global scheduling contexts, lazy-create and return them.
        // Ignore queue priority for the global scheduling contexts
        const MutexAuto guard(&m_contextLock);
        if (isTmzOnly)
        {
            if (m_hTmzContext == nullptr)
            {
                result = CreateCommandSubmissionContextRaw(&m_hTmzContext, QueuePriority::Medium, isTmzOnly);
            }
            *pContextHandle = m_hTmzContext;
        }
        else
        {
            if (m_hContext == nullptr)
            {
                result = CreateCommandSubmissionContextRaw(&m_hContext, QueuePriority::Medium, isTmzOnly);
            }
            *pContextHandle = m_hContext;
        }
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

    if ((hContext != m_hContext) && (hContext != m_hTmzContext))
    {
        if (m_drmProcs.pfnAmdgpuCsCtxFree(hContext) != 0)
        {
            result = Result::ErrorInvalidValue;
        }
    }

    return result;
}

// =====================================================================================================================
// Call amdgpu to submit the commands through amdgpu_cs_submit_raw2, which requires caller to setup the cs_chunks
Result Device::SubmitRaw2(
    amdgpu_context_handle           hContext,
    uint32                          bo_handle_list,
    uint32                          chunkCount,
    struct drm_amdgpu_cs_chunk *    pChunks,
    uint64*                         pFence
    ) const
{
    Result result = Result::Success;
    result = CheckResult(m_drmProcs.pfnAmdgpuCsSubmitRaw2(m_hDevice, hContext, bo_handle_list, chunkCount, pChunks, pFence),
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
        fenceSize = sizeof(TimestampFence);
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
    Pal::Fence* pFence = nullptr;
    PAL_ASSERT((pPlacementAddr != nullptr) && (ppFence != nullptr));

    if (GetFenceType() == FenceType::SyncObj)
    {
        pFence = PAL_PLACEMENT_NEW(pPlacementAddr) SyncobjFence(*this);
    }
    else
    {
        pFence = PAL_PLACEMENT_NEW(pPlacementAddr) TimestampFence();
    }

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
    Pal::Fence* pFence = nullptr;
    PAL_ASSERT((pPlacementAddr != nullptr) && (ppFence != nullptr));

    if (GetFenceType() == FenceType::SyncObj)
    {
        pFence = PAL_PLACEMENT_NEW(pPlacementAddr) SyncobjFence(*this);
    }
    else
    {
        pFence = PAL_PLACEMENT_NEW(pPlacementAddr) TimestampFence();
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
Result Device::WaitForOsFences(
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
// Call amdgpu to wait for multiple semaphores
Result Device::WaitForSemaphores(
    uint32                       semaphoreCount,
    const IQueueSemaphore*const* ppSemaphores,
    const uint64*                pValues,
    uint32                       flags,
    uint64                       timeout) const
{
    Result result = Result::Success;

    AutoBuffer<uint32, 16, Pal::Platform> hSyncobjs(semaphoreCount, GetPlatform());
    AutoBuffer<uint64, 16, Pal::Platform> points(semaphoreCount, GetPlatform());

    if (semaphoreCount == 0)
    {
        result = Result::ErrorInvalidValue;
    }

    if (m_drmProcs.pfnAmdgpuCsSyncobjTimelineWaitisValid() == false)
    {
        result = Result::Unsupported;
    }

    if ((hSyncobjs.Capacity() < semaphoreCount) || (points.Capacity() < semaphoreCount))
    {
        result = Result::ErrorOutOfMemory;
    }

    if (result == Result::Success)
    {
        for (uint32 i = 0; i < semaphoreCount; i++)
        {
            if (ppSemaphores == nullptr || (ppSemaphores[i] == nullptr))
            {
                result = Result::ErrorInvalidPointer;
                break;
            }
            amdgpu_semaphore_handle hSemaphore =
                static_cast<const QueueSemaphore*>(ppSemaphores[i])->GetSyncObjHandle();

            hSyncobjs[i] = reinterpret_cast<uintptr_t>(hSemaphore);
            points[i]    = pValues[i];
        }

        if (result == Result::Success) {
            uint32 waitFlags = DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT;

            if ((flags & HostWaitFlags::HostWaitAny) == 0)
            {
                waitFlags |= DRM_SYNCOBJ_WAIT_FLAGS_WAIT_ALL;
            }

            int32 ret = m_drmProcs.pfnAmdgpuCsSyncobjTimelineWait(m_hDevice,
                                                                  &hSyncobjs[0],
                                                                  &points[0],
                                                                  semaphoreCount,
                                                                  ComputeAbsTimeout(timeout),
                                                                  waitFlags,
                                                                  nullptr);
            result = CheckResult(ret, Result::ErrorUnknown);
        }
    }

    return result;
}

// =====================================================================================================================
// Call amdgpu to wait for multiple fences (fence based on Sync Object)
Result Device::WaitForSyncobjFences(
    uint32*              pFences,
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
    const uint32* pFences,
    uint32        fenceCount
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
    Result result = CheckResult(m_drmProcs.pfnAmdgpuBoListCreate(m_hDevice,
                                                          numberOfResources,
                                                          pResources,
                                                          pResourcePriorities,
                                                          pListHandle),
                                Result::ErrorOutOfGpuMemory);

    return result;
}

// =====================================================================================================================
// Call amdgpu to destroy a bo list.
Result Device::DestroyResourceList(
    amdgpu_bo_list_handle handle
    ) const
{
    Result result = CheckResult(m_drmProcs.pfnAmdgpuBoListDestroy(handle),
                                Result::ErrorInvalidValue);

    return result;
}

// =====================================================================================================================
// Call amdgpu to create a list of buffer objects which are referenced by the commands submit.
Result Device::CreateResourceListRaw(
    uint32                           numberOfResources,
    struct drm_amdgpu_bo_list_entry* pBoListEntry,
    uint32*                          pListHandle
    ) const
{
    Result result = CheckResult(m_drmProcs.pfnAmdgpuBoListCreateRaw(m_hDevice,
                                                             numberOfResources,
                                                             pBoListEntry,
                                                             pListHandle),
                                Result::ErrorOutOfGpuMemory);

    return result;
}

// =====================================================================================================================
// Call amdgpu to destroy a bo list.
Result Device::DestroyResourceListRaw(
    uint32 handle
    ) const
{
    Result result = CheckResult(m_drmProcs.pfnAmdgpuBoListDestroyRaw(m_hDevice, handle),
                                Result::ErrorInvalidValue);

    return result;
}

// =====================================================================================================================
static uint32 AmdGpuToPalPipeConfigConversion(
    AMDGPU_PIPE_CFG pipeConfig)
{
    uint32 palPipeConfig = 0;
    namespace Chip = Pal::Gfx9::Chip;

    switch (pipeConfig)
    {
        case AMDGPU_PIPE_CFG__P2:
            palPipeConfig = Chip::ADDR_SURF_P2;
            break;
        case AMDGPU_PIPE_CFG__P4_8x16:
            palPipeConfig = Chip::ADDR_SURF_P4_8x16;
            break;
        case AMDGPU_PIPE_CFG__P4_16x16:
            palPipeConfig = Chip::ADDR_SURF_P4_16x16;
            break;
        case AMDGPU_PIPE_CFG__P4_16x32:
            palPipeConfig = Chip::ADDR_SURF_P4_16x32;
            break;
        case AMDGPU_PIPE_CFG__P4_32x32:
            palPipeConfig = Chip::ADDR_SURF_P4_32x32;
            break;
        case AMDGPU_PIPE_CFG__P8_16x16_8x16:
            palPipeConfig = Chip::ADDR_SURF_P8_16x16_8x16;
            break;
        case AMDGPU_PIPE_CFG__P8_16x32_8x16:
            palPipeConfig = Chip::ADDR_SURF_P8_16x32_8x16;
            break;
        case AMDGPU_PIPE_CFG__P8_32x32_8x16:
            palPipeConfig = Chip::ADDR_SURF_P8_32x32_8x16;
            break;
        case AMDGPU_PIPE_CFG__P8_16x32_16x16:
            palPipeConfig = Chip::ADDR_SURF_P8_16x32_16x16;
            break;
        case AMDGPU_PIPE_CFG__P8_32x32_16x16:
            palPipeConfig = Chip::ADDR_SURF_P8_32x32_16x16;
            break;
        case AMDGPU_PIPE_CFG__P8_32x32_16x32:
            palPipeConfig = Chip::ADDR_SURF_P8_32x32_16x32;
            break;
        case AMDGPU_PIPE_CFG__P8_32x64_32x32:
            palPipeConfig = Chip::ADDR_SURF_P8_32x64_32x32;
            break;
        case AMDGPU_PIPE_CFG__P16_32x32_8x16:
            palPipeConfig = Chip::ADDR_SURF_P16_32x32_8x16;
            break;
        case AMDGPU_PIPE_CFG__P16_32x32_16x16:
            palPipeConfig = Chip::ADDR_SURF_P16_32x32_16x16;
            break;
        default:
            palPipeConfig = Chip::ADDR_SURF_P2;
            break;
    }

    namespace Gfx6 = Pal::Gfx6::Chip;
    namespace Gfx9 = Pal::Gfx9::Chip;

    // clang-format off
    static_assert(static_cast<uint32>(Gfx6::ADDR_SURF_P2)             == static_cast<uint32>(Gfx9::ADDR_SURF_P2),             "Enums need updating!");
    static_assert(static_cast<uint32>(Gfx6::ADDR_SURF_P4_8x16)        == static_cast<uint32>(Gfx9::ADDR_SURF_P4_8x16),        "Enums need updating!");
    static_assert(static_cast<uint32>(Gfx6::ADDR_SURF_P4_16x16)       == static_cast<uint32>(Gfx9::ADDR_SURF_P4_16x16),       "Enums need updating!");
    static_assert(static_cast<uint32>(Gfx6::ADDR_SURF_P4_16x32)       == static_cast<uint32>(Gfx9::ADDR_SURF_P4_16x32),       "Enums need updating!");
    static_assert(static_cast<uint32>(Gfx6::ADDR_SURF_P4_32x32)       == static_cast<uint32>(Gfx9::ADDR_SURF_P4_32x32),       "Enums need updating!");
    static_assert(static_cast<uint32>(Gfx6::ADDR_SURF_P8_16x16_8x16)  == static_cast<uint32>(Gfx9::ADDR_SURF_P8_16x16_8x16),  "Enums need updating!");
    static_assert(static_cast<uint32>(Gfx6::ADDR_SURF_P8_16x32_8x16)  == static_cast<uint32>(Gfx9::ADDR_SURF_P8_16x32_8x16),  "Enums need updating!");
    static_assert(static_cast<uint32>(Gfx6::ADDR_SURF_P8_32x32_8x16)  == static_cast<uint32>(Gfx9::ADDR_SURF_P8_32x32_8x16),  "Enums need updating!");
    static_assert(static_cast<uint32>(Gfx6::ADDR_SURF_P8_16x32_16x16) == static_cast<uint32>(Gfx9::ADDR_SURF_P8_16x32_16x16), "Enums need updating!");
    static_assert(static_cast<uint32>(Gfx6::ADDR_SURF_P8_32x32_16x16) == static_cast<uint32>(Gfx9::ADDR_SURF_P8_32x32_16x16), "Enums need updating!");
    static_assert(static_cast<uint32>(Gfx6::ADDR_SURF_P8_32x32_16x32) == static_cast<uint32>(Gfx9::ADDR_SURF_P8_32x32_16x32), "Enums need updating!");
    static_assert(static_cast<uint32>(Gfx6::ADDR_SURF_P8_32x64_32x32) == static_cast<uint32>(Gfx9::ADDR_SURF_P8_32x64_32x32), "Enums need updating!");

    static_assert(static_cast<uint32>(Gfx6::ADDR_SURF_P16_32x32_8x16__CI__VI)  == static_cast<uint32>(Gfx9::ADDR_SURF_P16_32x32_8x16),  "Enums need updating!");
    static_assert(static_cast<uint32>(Gfx6::ADDR_SURF_P16_32x32_16x16__CI__VI) == static_cast<uint32>(Gfx9::ADDR_SURF_P16_32x32_16x16), "Enums need updating!");
    // clang-format on

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
    const auto& imageCreateInfo = pImage->GetImageCreateInfo();
    const uint32 numPlanes      = pImage->GetImageInfo().numPlanes;
    const uint32 subResPerPlane = (imageCreateInfo.mipLevels * imageCreateInfo.arraySize);

    // The following code assumes that the number of subresources in the Image matches the number of planes
    // (i.e., each plane has only one subresource)
    PAL_ASSERT(subResPerPlane == 1);

    if(m_drmProcs.pfnAmdgpuBoQueryInfo(hBuffer, &info) == 0)
    {
        if (info.metadata.size_metadata >= PRO_UMD_METADATA_SIZE)
        {
            if (ChipProperties().gfxLevel >= GfxIpLevel::GfxIp9)
            {
                AddrMgr2::TileInfo *const pTileInfo = static_cast<AddrMgr2::TileInfo*>
                                                           (pImage->GetSubresourceTileInfo(0));
                auto*const pUmdMetaData             = reinterpret_cast<amdgpu_bo_umd_metadata*>
                                                           (&info.metadata.umd_metadata[PRO_UMD_METADATA_OFFSET_DWORD]);
                pTileInfo->pipeBankXor              = pUmdMetaData->pipeBankXor;

                for (uint32 plane = 1; plane < numPlanes; plane++)
                {
                    AddrMgr2::TileInfo *const pPlaneTileInfo = static_cast<AddrMgr2::TileInfo*>
                                                               (pImage->GetSubresourceTileInfo(subResPerPlane * plane));
                    pPlaneTileInfo->pipeBankXor              = pUmdMetaData->additionalPipeBankXor[plane - 1];
                }
            }
            else
            {
                AddrMgr1::TileInfo *const pTileInfo = static_cast<AddrMgr1::TileInfo*>
                                                        (pImage->GetSubresourceTileInfo(0));
                auto*const pUmdMetaData             = reinterpret_cast<amdgpu_bo_umd_metadata*>
                                                        (&info.metadata.umd_metadata[PRO_UMD_METADATA_OFFSET_DWORD]);

                pSubResInfo->extentTexels.width        = pUmdMetaData->width_in_pixels;
                pSubResInfo->extentTexels.height       = pUmdMetaData->height;
                pSubResInfo->rowPitch                  = pUmdMetaData->aligned_pitch_in_bytes;
                pSubResInfo->actualExtentTexels.height = pUmdMetaData->aligned_height;

                pTileInfo->tileIndex                   = pUmdMetaData->tile_index;
                pTileInfo->tileMode                    = AmdGpuToAddrTileModeConversion(pUmdMetaData->tile_mode);
                pTileInfo->tileType                    = static_cast<uint32>(pUmdMetaData->micro_tile_mode);
                pTileInfo->pipeConfig                  = AmdGpuToPalPipeConfigConversion(
                                                         pUmdMetaData->tile_config.pipe_config);
                pTileInfo->banks                       = pUmdMetaData->tile_config.banks;
                pTileInfo->bankWidth                   = pUmdMetaData->tile_config.bank_width;
                pTileInfo->bankHeight                  = pUmdMetaData->tile_config.bank_height;
                pTileInfo->macroAspectRatio            = pUmdMetaData->tile_config.macro_aspect_ratio;
                pTileInfo->tileSplitBytes              = pUmdMetaData->tile_config.tile_split_bytes;
                pTileInfo->tileSwizzle                 = pUmdMetaData->pipeBankXor;

                for (uint32 plane = 1; plane < numPlanes; plane++)
                {
                    AddrMgr1::TileInfo *const pPlaneTileInfo = static_cast<AddrMgr1::TileInfo *>
                                                               (pImage->GetSubresourceTileInfo(subResPerPlane * plane));
                    pPlaneTileInfo->tileSwizzle              = pUmdMetaData->additionalPipeBankXor[plane - 1];
                }
            }
        }
        else if (IsMesaMetadata(info.metadata))
        {
            if (ChipProperties().gfxLevel < GfxIpLevel::GfxIp9)
            {
                AmdGpuTilingFlags tilingFlags;
                tilingFlags.u64All      = info.metadata.tiling_info;
                auto*const pMetaData    = reinterpret_cast<const amdgpu_bo_umd_metadata*>(&info.metadata.umd_metadata);
                auto*const pRawMetaData = reinterpret_cast<const uint32*>(pMetaData);
                auto*const pTileInfo    = static_cast<AddrMgr1::TileInfo*>(pImage->GetSubresourceTileInfo(0));

                pSubResInfo->extentTexels.width        = imageCreateInfo.extent.width;
                pSubResInfo->extentTexels.height       = imageCreateInfo.extent.height;
                pSubResInfo->actualExtentTexels.height = imageCreateInfo.extent.height;
                pSubResInfo->format                    = imageCreateInfo.swizzledFormat;

                pTileInfo->tileIndex        = (pRawMetaData[5] >> 20) & 0x1F;
                pTileInfo->tileType         = tilingFlags.microTileMode;
                pTileInfo->pipeConfig       = tilingFlags.pipeConfig;
                pTileInfo->banks            = tilingFlags.numBanks;
                pTileInfo->bankWidth        = tilingFlags.bankWidth;
                pTileInfo->bankHeight       = tilingFlags.bankHeight;
                pTileInfo->macroAspectRatio = tilingFlags.macroTileAspect;
                pTileInfo->tileSplitBytes   = tilingFlags.tileSplit;
            }
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
// Create Presentable Memory Object.
Result Device::CreatePresentableMemoryObject(
    const PresentableImageCreateInfo&   createInfo,
    Image*                              pImage,
    void*                               pMemObjMem,
    Pal::GpuMemory**                    ppMemObjOut)
{
    return Image::CreatePresentableMemoryObject(this, createInfo, pImage, pMemObjMem, ppMemObjOut);
}

// =====================================================================================================================
// Update the metadata, including the tiling mode, pixel format, pitch, aligned height, to metadata associated
// with the memory object.  The consumer of the memory object will get the metadata when importing it and view the image
// in the exactly the same way.
void Device::UpdateMetaData(
    amdgpu_bo_handle         hBuffer,
    const Image&             image,
    const Amdgpu::GpuMemory* pAmdgpuGpuMem)
{
    amdgpu_bo_metadata metadata = {};
    const SubResourceInfo*const    pSubResInfo = image.SubresourceInfo(0);
    auto imageCreateInfo        = image.GetImageCreateInfo();
    const uint32 subResPerPlane = (imageCreateInfo.mipLevels * imageCreateInfo.arraySize);

    // First 32 dwords are reserved for open source components.
    auto*const pUmdMetaData = reinterpret_cast<amdgpu_bo_umd_metadata*>
                              (&metadata.umd_metadata[PRO_UMD_METADATA_OFFSET_DWORD]);

    if (ChipProperties().gfxLevel >= GfxIpLevel::GfxIp9)
    {
        const SubResourceInfo*const    pSubResInfo = image.SubresourceInfo(0);
        const AddrMgr2::TileInfo*const pTileInfo   = AddrMgr2::GetTileInfo(&image, 0);

        PAL_ASSERT(static_cast<uint32>(AMDGPU_SWIZZLE_MODE_MAX_TYPE)
            == static_cast<uint32>(ADDR_SW_MAX_TYPE));

        PAL_ASSERT(static_cast<uint32>(AMDGPU_ADDR_RSRC_TEX_2D) == static_cast<uint32>(ADDR_RSRC_TEX_2D));

        AMDGPU_SWIZZLE_MODE curSwizzleMode   =
            static_cast<AMDGPU_SWIZZLE_MODE>(image.GetGfxImage()->GetSwTileMode(pSubResInfo));

        metadata.size_metadata  = PRO_UMD_METADATA_SIZE;

        memset(&metadata.umd_metadata[0], 0, PRO_UMD_METADATA_OFFSET_DWORD * sizeof(metadata.umd_metadata[0]));
        pUmdMetaData->width_in_pixels        = pSubResInfo->extentTexels.width;
        pUmdMetaData->height                 = pSubResInfo->extentTexels.height;
        pUmdMetaData->depth                  = pSubResInfo->extentTexels.depth;
        pUmdMetaData->aligned_pitch_in_bytes = pSubResInfo->rowPitch;
        pUmdMetaData->aligned_height         = pSubResInfo->actualExtentTexels.height;
        pUmdMetaData->format                 = PalToAmdGpuFormatConversion(pSubResInfo->format);

        pUmdMetaData->pipeBankXor  = pTileInfo->pipeBankXor;

        for (uint32 plane = 1; plane < (image.GetImageInfo().numPlanes); plane++)
        {
            const AddrMgr2::TileInfo*const pPlaneTileInfo  = AddrMgr2::GetTileInfo(&image, (subResPerPlane * plane));
            pUmdMetaData->additionalPipeBankXor[plane - 1] = pPlaneTileInfo->pipeBankXor;
        }

        pUmdMetaData->swizzleMode  = curSwizzleMode;
        pUmdMetaData->resourceType = static_cast<AMDGPU_ADDR_RESOURCE_TYPE>(imageCreateInfo.imageType);

        DccState dccState = {};

        // We cannot differentiate displayable DCC from standard DCC in the existing metadata. However, the control register values
        // should match between displayable DCC and standard DCC.
        if (image.GetGfxImage()->HasDisplayDccData())
        {
            image.GetGfxImage()->GetDisplayDccState(&dccState);
        }
        else
        {
            image.GetGfxImage()->GetDccState(&dccState);
        }

        metadata.tiling_info = 0;
        metadata.tiling_info |= AMDGPU_TILING_SET(SWIZZLE_MODE, curSwizzleMode);
        // In order to sharing resource metadata with Mesa3D, the definition have to follow Mesa's way.
        // The swizzle_info is used in Mesa to indicate whether the surface is displyable.
        metadata.tiling_info |= AMDGPU_TILING_SET(SCANOUT, imageCreateInfo.flags.presentable);
        metadata.tiling_info |= AMDGPU_TILING_SET(DCC_OFFSET_256B, Get256BAddrLo(dccState.primaryOffset));
        metadata.tiling_info |= AMDGPU_TILING_SET(DCC_PITCH_MAX, (dccState.pitch - 1));
        metadata.tiling_info |= AMDGPU_TILING_SET(DCC_INDEPENDENT_64B, dccState.independentBlk64B);
        metadata.tiling_info |= AMDGPU_TILING_SET(DCC_INDEPENDENT_128B, dccState.independentBlk128B);
        metadata.tiling_info |= AMDGPU_TILING_SET(DCC_MAX_COMPRESSED_BLOCK_SIZE, dccState.maxCompressedBlockSize);
        metadata.tiling_info |= AMDGPU_TILING_SET(DCC_MAX_UNCOMPRESSED_BLOCK_SIZE, dccState.maxUncompressedBlockSize);
    }
    else
    {
        metadata.tiling_info   = AMDGPU_TILE_MODE__2D_TILED_THIN1;
        metadata.size_metadata = PRO_UMD_METADATA_SIZE;

        const SubResourceInfo*const    pSubResInfo = image.SubresourceInfo(0);
        const AddrMgr1::TileInfo*const pTileInfo   = AddrMgr1::GetTileInfo(&image, 0);

        memset(&metadata.umd_metadata[0], 0, PRO_UMD_METADATA_OFFSET_DWORD * sizeof(metadata.umd_metadata[0]));
        pUmdMetaData->width_in_pixels        = pSubResInfo->extentTexels.width;
        pUmdMetaData->height                 = pSubResInfo->extentTexels.height;
        pUmdMetaData->depth                  = pSubResInfo->extentTexels.depth;
        pUmdMetaData->aligned_pitch_in_bytes = pSubResInfo->rowPitch;
        pUmdMetaData->aligned_height         = pSubResInfo->actualExtentTexels.height;
        pUmdMetaData->tile_index             = pTileInfo->tileIndex;
        pUmdMetaData->format                 = PalToAmdGpuFormatConversion(pSubResInfo->format);
        pUmdMetaData->tile_mode              = AddrToAmdGpuTileModeConversion(pTileInfo->tileMode);
        pUmdMetaData->micro_tile_mode        = static_cast<AMDGPU_MICRO_TILE_MODE>(pTileInfo->tileType);

        pUmdMetaData->pipeBankXor            = pTileInfo->tileSwizzle;

        for (uint32 plane = 1; plane < (image.GetImageInfo().numPlanes); plane++)
        {
            const AddrMgr1::TileInfo*const pPlaneTileInfo  = AddrMgr1::GetTileInfo(&image, (subResPerPlane * plane));
            pUmdMetaData->additionalPipeBankXor[plane - 1] = pPlaneTileInfo->tileSwizzle;
        }

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

    pUmdMetaData->array_size              = imageCreateInfo.arraySize;
    pUmdMetaData->flags.mip_levels        = imageCreateInfo.mipLevels;
    pUmdMetaData->flags.cubemap           = imageCreateInfo.flags.cubemap;
    pUmdMetaData->flags.render_target     = imageCreateInfo.usageFlags.colorTarget;
    pUmdMetaData->flags.depth_stencil     = imageCreateInfo.usageFlags.depthStencil;
    pUmdMetaData->flags.texture           = imageCreateInfo.usageFlags.shaderRead;
    pUmdMetaData->flags.unodered_access   = imageCreateInfo.usageFlags.shaderWrite;
    pUmdMetaData->flags.resource_type     = static_cast<AMDGPU_ADDR_RESOURCE_TYPE>(imageCreateInfo.imageType);
    pUmdMetaData->flags.optimal_shareable = imageCreateInfo.flags.optimalShareable;
    pUmdMetaData->flags.samples           = imageCreateInfo.samples;

    if (pUmdMetaData->flags.optimal_shareable)
    {
        //analysis the shared metadata if surface is optimal shareable
        SharedMetadataInfo sharedMetadataInfo = {};
        image.GetGfxImage()->GetSharedMetadataInfo(&sharedMetadataInfo);

        PAL_ASSERT(sharedMetadataInfo.numPlanes <= 1);

        auto*const pUmdSharedMetadata = reinterpret_cast<amdgpu_shared_metadata_info*>
                                            (&pUmdMetaData->shared_metadata_info);
        pUmdSharedMetadata->dcc_offset   = sharedMetadataInfo.dccOffset[0];
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
        pUmdSharedMetadata->flags.has_cmask_eq_gpu_access =
            sharedMetadataInfo.flags.hasCmaskEqGpuAccess;
        pUmdSharedMetadata->flags.has_htile_lookup_table =
            sharedMetadataInfo.flags.hasHtileLookupTable;
        pUmdSharedMetadata->flags.htile_has_ds_metadata =
            sharedMetadataInfo.flags.htileHasDsMetadata;

        pUmdSharedMetadata->dcc_state_offset =
            sharedMetadataInfo.dccStateMetaDataOffset[0];
        pUmdSharedMetadata->fast_clear_value_offset =
            sharedMetadataInfo.fastClearMetaDataOffset[0];
        pUmdSharedMetadata->fce_state_offset =
            sharedMetadataInfo.fastClearEliminateMetaDataOffset[0];
        if ((sharedMetadataInfo.fmaskOffset != 0) &&
            (ChipProperties().gfxLevel >= GfxIpLevel::GfxIp9))
        {
            // Only hardware of gfxlevel >= gfx9 supports that Fmask has its own PipeBankXor.
            //if the shared surface is a color surface, reuse the htileOffset as fmaskXor.
            PAL_ASSERT(sharedMetadataInfo.htileOffset == 0);
            pUmdSharedMetadata->flags.htile_as_fmask_xor = 1;
            pUmdSharedMetadata->htile_offset = sharedMetadataInfo.fmaskXor;
            pUmdSharedMetadata->fmaskSwizzleMode = static_cast<AMDGPU_SWIZZLE_MODE>
                (sharedMetadataInfo.fmaskSwizzleMode);
        }
        if (sharedMetadataInfo.flags.hasHtileLookupTable)
        {
            PAL_ASSERT(sharedMetadataInfo.dccStateMetaDataOffset[0] == 0);
            pUmdSharedMetadata->htile_lookup_table_offset =
                sharedMetadataInfo.htileLookupTableOffset;
        }

        pUmdSharedMetadata->resource_id        = LowPart(pAmdgpuGpuMem->Desc().uniqueId);
        pUmdSharedMetadata->resource_id_high32 = HighPart(pAmdgpuGpuMem->Desc().uniqueId);

        // In order to support displayable dcc in linux window mode,
        // it's needed to share standard dcc metadata with Mesa3D when displayable DCC has enabled.
        // According to Mesa3D metadata parsing function ac_surface_set_umd_metadata,
        // Mesa3D share standard dcc metadata though first 10 dwords of umd_metadata of struct amdgpu_bo_metadata.
        if ((ChipProperties().gfxLevel >= GfxIpLevel::GfxIp10_3) &&
            image.GetGfxImage()->HasDisplayDccData() &&
            (pUmdSharedMetadata->dcc_offset != 0))
        {
            auto*const pMesaUmdMetaData = reinterpret_cast<MesaUmdMetaData*>(&metadata.umd_metadata[0]);
            // Metadata image format format version 1
            pMesaUmdMetaData->header.version                        = 1;
            pMesaUmdMetaData->header.vendorId                       = ATI_VENDOR_ID;
            pMesaUmdMetaData->header.asicId                         = m_gpuInfo.asic_id;
            pMesaUmdMetaData->imageSrd.gfx10.metaPipeAligned        = sharedMetadataInfo.pipeAligned[0];
            // both displayable dcc and standard dcc is enbaled,compression must be enabled.
            pMesaUmdMetaData->imageSrd.gfx10.compressionEnable      = 1;
            pMesaUmdMetaData->imageSrd.gfx10.metaDataOffset         = pUmdSharedMetadata->dcc_offset >> 8;
        }
    }

    m_drmProcs.pfnAmdgpuBoSetMetadata(hBuffer, &metadata);
}

// =====================================================================================================================
// Update the GPU memory's unique ID in the metadata associated with the memory object. The GPU memory's unique ID will
// be available via the metadata after import.
void Device::UpdateMetaDataUniqueId(
    const Amdgpu::GpuMemory* pAmdgpuGpuMem)
{
    amdgpu_bo_handle hBuffer = pAmdgpuGpuMem->SurfaceHandle();
    amdgpu_bo_info   info    = {};

    // Read current metadata first if it exists
    QueryBufferInfo(hBuffer, &info);

    // Only update metadata of which the BO allocated by Pal
    if (info.metadata.size_metadata == PRO_UMD_METADATA_SIZE)
    {
        // First 32 dwords are reserved for open source components.
        auto*const pUmdMetaData       = reinterpret_cast<amdgpu_bo_umd_metadata*>
                                            (&info.metadata.umd_metadata[PRO_UMD_METADATA_OFFSET_DWORD]);

        auto*const pUmdSharedMetadata = reinterpret_cast<amdgpu_shared_metadata_info*>
                                            (&pUmdMetaData->shared_metadata_info);

        // Update metadata structure with GPU memory's unique ID
        pUmdSharedMetadata->resource_id        = LowPart(pAmdgpuGpuMem->Desc().uniqueId);
        pUmdSharedMetadata->resource_id_high32 = HighPart(pAmdgpuGpuMem->Desc().uniqueId);

        // Set new metadata
        int32 drmRet = m_drmProcs.pfnAmdgpuBoSetMetadata(hBuffer, &info.metadata);
        PAL_ASSERT(drmRet == 0);
    }
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
    uint64 supported = 0;
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
        m_syncobjSupportState.syncobjSemaphore = (status == Result::Success);

        // Check CreateSignaledSyncObject support with DRM_SYNCOBJ_CREATE_SIGNALED flags
        // Dependent on Basic SyncObject's support
        if ((pLnxPlatform->IsCreateSignaledSyncObjectSupported()) &&
            (m_syncobjSupportState.syncobjSemaphore == 1))
        {
            status = CreateSyncObject(DRM_SYNCOBJ_CREATE_SIGNALED, &hSyncobj);
            m_syncobjSupportState.initialSignaledSyncobjSemaphore = (status == Result::Success);

            // Check SyncobjFence needed SyncObject api with wait/reset interface
            // Dependent on CreateSignaledSyncObject support; Will just wait on this initial Signaled Syncobj.
            if ((pLnxPlatform->IsSyncobjFenceSupported()) &&
                (m_syncobjSupportState.initialSignaledSyncobjSemaphore == 1))
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
                    m_syncobjSupportState.syncobjFence = (status == Result::Success);
                }
            }
        }
        if (IsDrmVersionOrGreater(3,32))
        {
            uint64 cap = 0;

            if (m_drmProcs.pfnDrmGetCap(m_fileDescriptor, DRM_CAP_SYNCOBJ_TIMELINE, &cap) == 0)
            {
                m_syncobjSupportState.timelineSemaphore = ((cap == 1)       &&
                    m_drmProcs.pfnAmdgpuCsSyncobjTransferisValid()          &&
                    m_drmProcs.pfnAmdgpuCsSyncobjQueryisValid()             &&
                    m_drmProcs.pfnAmdgpuCsSyncobjQuery2isValid()            &&
                    m_drmProcs.pfnAmdgpuCsSyncobjTimelineWaitisValid()      &&
                    m_drmProcs.pfnAmdgpuCsSyncobjTimelineSignalisValid()    &&
                    m_syncobjSupportState.syncobjFence                      &&
                    m_syncobjSupportState.syncobjSemaphore);

                if (m_syncobjSupportState.timelineSemaphore)
                {
                    amdgpu_syncobj_handle hSyncobj = 0;

                    // Check Basic SyncObject's support with Query2 api.
                    status = CreateSyncObject(DRM_SYNCOBJ_CREATE_SIGNALED, &hSyncobj);
                    if (status == Result::Success)
                    {
                        uint64 queryValue = 0;

                        status = QuerySemaphoreValue(
                                reinterpret_cast<amdgpu_semaphore_handle>(hSyncobj),
                                &queryValue,
                                DRM_SYNCOBJ_QUERY_FLAGS_LAST_SUBMITTED);
                        if (status != Result::Success)
                        {
                            m_syncobjSupportState.timelineSemaphore = 0;
                        }

                        DestroySyncObject(hSyncobj);
                    }
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
    uint64                importPoint,
    amdgpu_syncobj_handle exportSyncObj,
    uint64                exportPoint
    ) const
{
    // In current kernel driver, the ioctl to transfer fence state is not implemented.
    // I have to use two IOCTLs to emulate the transfer operation.
    // It still runs into problem, since we cannot guarantee the fence is still valid when we call the export, since
    // the fence would be null-ed if signaled.
    int32 syncFileFd = 0;
    int32 ret = 0;

    if (m_syncobjSupportState.timelineSemaphore)
    {
        uint32 flags = DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT;

        ret = m_drmProcs.pfnAmdgpuCsSyncobjTransfer(m_hDevice,
                                                    importSyncObj,
                                                    importPoint,
                                                    exportSyncObj,
                                                    exportPoint,
                                                    flags);
    }
    else
    {
        ret = m_drmProcs.pfnAmdgpuCsSyncobjExportSyncFile(m_hDevice,
                                                          exportSyncObj,
                                                          &syncFileFd);
        if (ret == 0)
        {
            ret = m_drmProcs.pfnAmdgpuCsSyncobjImportSyncFile(m_hDevice,
                                                              importSyncObj,
                                                              syncFileFd);
            close(syncFileFd);
        }
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
Result Device::SignalSyncObject(
    amdgpu_syncobj_handle*      pSyncObject,
    uint32                      numSyncObject
    ) const
{
    int32 ret = m_drmProcs.pfnAmdgpuCsSyncobjSignal(m_hDevice, pSyncObject, numSyncObject);

    return CheckResult(ret, Result::ErrorUnknown);
}

// =====================================================================================================================
Result Device::CreateSemaphore(
    bool                     isCreatedSignaled,
    bool                     isCreatedTimeline,
    uint64                   initialCount,
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

            if (isCreatedTimeline)
            {
                result = SignalSemaphoreValue(*pSemaphoreHandle, initialCount);
            }
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
Result Device::QuerySemaphoreValue(
    amdgpu_semaphore_handle  hSemaphore,
    uint64*                  pValue,
    uint32                   flags
    ) const
{
    int32 ret = 0;

    if (m_syncobjSupportState.timelineSemaphore)
    {
        amdgpu_syncobj_handle hSyncobj = reinterpret_cast<uintptr_t>(hSemaphore);
        ret = m_drmProcs.pfnAmdgpuCsSyncobjQuery2(m_hDevice,
                                                  &hSyncobj,
                                                  pValue,
                                                  1,
                                                  flags);
    }

    return CheckResult(ret, Result::ErrorUnknown);
}

// =====================================================================================================================
Result Device::WaitSemaphoreValue(
    amdgpu_semaphore_handle  hSemaphore,
    uint64                   value,
    uint32                   flags,
    uint64                   timeoutNs
    ) const
{
    int32 ret = 0;

    if (m_syncobjSupportState.timelineSemaphore)
    {
        amdgpu_syncobj_handle hSyncobj = reinterpret_cast<uintptr_t>(hSemaphore);

        ret = m_drmProcs.pfnAmdgpuCsSyncobjTimelineWait(m_hDevice,
                                                        &hSyncobj,
                                                        &value,
                                                        1,
                                                        ComputeAbsTimeout(timeoutNs),
                                                        flags,
                                                        nullptr);
    }

    return CheckResult(ret, Result::ErrorUnknown);
}

// =====================================================================================================================
bool Device::IsWaitBeforeSignal(
    amdgpu_semaphore_handle  hSemaphore,
    uint64                   value
    ) const
{
    bool  waitBeforeSignal = false;
    int32 ret = 0;

    if (m_syncobjSupportState.timelineSemaphore)
    {
        amdgpu_syncobj_handle hSyncobj = reinterpret_cast<uintptr_t>(hSemaphore);

        if (m_drmProcs.pfnAmdgpuCsSyncobjQuery2isValid())
        {
            uint64 queryValue = 0;

            ret = m_drmProcs.pfnAmdgpuCsSyncobjQuery2(m_hDevice,
                                                      &hSyncobj,
                                                      &queryValue,
                                                      1,
                                                      DRM_SYNCOBJ_QUERY_FLAGS_LAST_SUBMITTED);
            PAL_ASSERT(ret == 0);
            if (ret == 0)
            {
                waitBeforeSignal = queryValue < value ? true : false;
            }
        }
        else
        {
            int32 flags = DRM_SYNCOBJ_WAIT_FLAGS_WAIT_AVAILABLE;

            ret = m_drmProcs.pfnAmdgpuCsSyncobjTimelineWait(m_hDevice,
                                                            &hSyncobj,
                                                            &value,
                                                            1,
                                                            0,
                                                            flags,
                                                            nullptr);
            waitBeforeSignal = ret == -EINVAL ? true : false;
        }
    }
    return waitBeforeSignal;
}

// =====================================================================================================================
Result Device::SignalSemaphoreValue(
    amdgpu_semaphore_handle  hSemaphore,
    uint64                   value
    ) const
{
    int32 ret = 0;

    if (m_syncobjSupportState.timelineSemaphore)
    {
        amdgpu_syncobj_handle hSyncobj = reinterpret_cast<uintptr_t>(hSemaphore);
        ret = m_drmProcs.pfnAmdgpuCsSyncobjTimelineSignal(m_hDevice,
                                                          &hSyncobj,
                                                          &value,
                                                          1);
    }

    return CheckResult(ret, Result::ErrorUnknown);
}

// =====================================================================================================================
// Adds GPU memory objects to this device's global memory list and populate the changes to all its queues
Result Device::AddGpuMemoryReferences(
    uint32              gpuMemRefCount,
    const GpuMemoryRef* pGpuMemoryRefs,
    IQueue*             pQueue,
    uint32              flags)
{
    Result result = Pal::Device::AddGpuMemoryReferences(gpuMemRefCount, pGpuMemoryRefs, pQueue, flags);

    if (result == Result::Success)
    {
        if (pQueue == nullptr)
        {
            result = AddGlobalReferences(gpuMemRefCount, pGpuMemoryRefs);
        }
        else
        {
            Queue* pLinuxQueue = static_cast<Queue*>(pQueue);
            result = pLinuxQueue->AddGpuMemoryReferences(gpuMemRefCount, pGpuMemoryRefs);
        }
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
    Result result = Pal::Device::RemoveGpuMemoryReferences(gpuMemoryCount, ppGpuMemory, pQueue);

    if (result == Result::Success)
    {
        if (pQueue == nullptr)
        {
            RemoveGlobalReferences(gpuMemoryCount, ppGpuMemory, false);
        }
        else
        {
            Queue* pLinuxQueue = static_cast<Queue*>(pQueue);
            pLinuxQueue->RemoveGpuMemoryReferences(gpuMemoryCount, ppGpuMemory, false);
        }
    }

    return result;
}

// =====================================================================================================================
// Set dirty for global memory reference list to all its queues
void Device::DirtyGlobalReferences()
{
    MutexAuto lock(&m_queueLock);

    for (auto iter = m_queues.Begin(); iter.IsValid(); iter.Next())
    {
        Queue*const pLinuxQueue = static_cast<Queue*>(iter.Get());
        pLinuxQueue->DirtyGlobalReferences();
    }
}

// =====================================================================================================================
// Adds GPU memory objects to this device's global memory list and all per-queue lists.
Result Device::AddGlobalReferences(
    uint32              gpuMemRefCount,
    const GpuMemoryRef* pGpuMemoryRefs)
{
    Result result = Result::Success;

    // First take the queue lock in isolation.
    {
        MutexAuto lock(&m_queueLock);

        for (auto iter = m_queues.Begin(); iter.IsValid(); iter.Next())
        {
            Queue*const pLinuxQueue = static_cast<Queue*>(iter.Get());
            result = pLinuxQueue->AddGpuMemoryReferences(gpuMemRefCount, pGpuMemoryRefs);
        }
    }

    // Then take the global ref lock in isolation.
    if (result == Result::Success)
    {
        MutexAuto lock(&m_globalRefLock);

        for (uint32 i = 0; (i < gpuMemRefCount) && (result == Result::Success); i++)
        {
            IGpuMemory* pGpuMemory    = pGpuMemoryRefs[i].pGpuMemory;
            bool        alreadyExists = false;
            uint32*     pRefCount     = nullptr;

            result = m_globalRefMap.FindAllocate(pGpuMemory, &alreadyExists, &pRefCount);

            if (result == Result::Success)
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

    return result;
}

// =====================================================================================================================
// Removes GPU memory objects from this device's global memory list and all per-queue lists.
void Device::RemoveGlobalReferences(
    uint32            gpuMemoryCount,
    IGpuMemory*const* ppGpuMemory,
    bool              forceRemove)
{
    // First take the queue lock in isolation.
    {
        MutexAuto lock(&m_queueLock);

        for (auto iter = m_queues.Begin(); iter.IsValid(); iter.Next())
        {
            Queue*const pLinuxQueue = static_cast<Queue*>(iter.Get());
            pLinuxQueue->RemoveGpuMemoryReferences(gpuMemoryCount, ppGpuMemory, forceRemove);
        }
    }

    // Then take the global ref lock in isolation.
    {
        MutexAuto lock(&m_globalRefLock);

        for (uint32 i = 0; i < gpuMemoryCount; i++)
        {
            IGpuMemory* pGpuMemory = ppGpuMemory[i];
            uint32*     pRefCount  = m_globalRefMap.FindKey(pGpuMemory);

            if (pRefCount != nullptr)
            {
                PAL_ALERT(*pRefCount <= 0);

                if ((--(*pRefCount) == 0) || forceRemove)
                {
                    m_globalRefMap.Erase(pGpuMemory);
                }
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
    Result            ret    = Result::ErrorOutOfGpuMemory;
    const VaPartition vaPart = pGpuMemory->VirtAddrPartition();

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
             (vaPart == VaPartition::ShadowDescriptorTable) ||
             (vaPart == VaPartition::CaptureReplay))
    {
        VirtAddrAssignInfo vaInfo = { };
        vaInfo.size      = pGpuMemory->Desc().size;
        vaInfo.alignment = pGpuMemory->Desc().alignment;
        vaInfo.partition = vaPart;

        ret = m_pVamMgr->AssignVirtualAddress(this, vaInfo, pGpuVirtAddr);
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
    Pal::GpuMemory* pGpuMemory)
{
    GpuMemory*        pMemory = static_cast<GpuMemory*>(pGpuMemory);
    const VaPartition vaPart  = pGpuMemory->VirtAddrPartition();
    if (vaPart == VaPartition::Default)
    {
        PAL_ASSERT(pMemory->VaRangeHandle() != nullptr);
        m_drmProcs.pfnAmdgpuVaRangeFree(pMemory->VaRangeHandle());
    }
    else if ((vaPart == VaPartition::DescriptorTable) ||
             (vaPart == VaPartition::ShadowDescriptorTable) ||
             (vaPart == VaPartition::CaptureReplay))
    {
        PAL_ASSERT(pMemory->VaRangeHandle() == nullptr);
        m_pVamMgr->FreeVirtualAddress(this, pGpuMemory);
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }
    pMemory->SetVaRangeHandle(nullptr);
}

// =====================================================================================================================
Result Device::ProbeGpuVaRange(
    gpusize     vaStart,
    gpusize     vaSize,
    VaPartition vaPartition
    ) const
{
    return m_pVamMgr->AllocateVaRange(this, vaPartition, vaStart, vaSize);
}

// =====================================================================================================================
// Reserve gpu va range.
Result Device::ReserveGpuVirtualAddress(
    VaPartition             vaPartition,
    gpusize                 baseVirtAddr,
    gpusize                 size,
    bool                    isVirtual,
    VirtualGpuMemAccessMode virtualAccessMode,
    gpusize*                pGpuVirtAddr)
{
    Result result = Result::Success;

    // On Linux, some partitions are reserved by VamMgrSingleton
    if (VamMgrSingleton::IsVamPartition(vaPartition) == false)
    {
        ReservedVaRangeInfo* pInfo = m_reservedVaMap.FindKey(baseVirtAddr);

        if (pInfo == nullptr)
        {
            ReservedVaRangeInfo info = {};

            result = CheckResult(m_drmProcs.pfnAmdgpuVaRangeAlloc(m_hDevice,
                                                                  amdgpu_gpu_va_range_general,
                                                                  size,
                                                                  0u,
                                                                  baseVirtAddr,
                                                                  pGpuVirtAddr,
                                                                  &info.vaHandle,
                                                                  0u),
                                 Result::ErrorUnknown);
            info.size = size;

            if (result == Result::Success)
            {
                PAL_ALERT(*pGpuVirtAddr != baseVirtAddr);
                m_reservedVaMap.Insert(*pGpuVirtAddr, info);
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
// Opens shared GPU memory from anyone except another PAL device in the same LDA chain.
Result Device::OpenExternalSharedGpuMemory(
    const ExternalGpuMemoryOpenInfo& openInfo,
    void*                            pPlacementAddr,
    GpuMemoryCreateInfo*             pMemCreateInfo,
    IGpuMemory**                     ppGpuMemory)
{
    Result result = Result::ErrorInvalidPointer;

    PAL_ASSERT(openInfo.resourceInfo.flags.globalGpuVa == 0);

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
    const bool needUpdateStablePState     = (DeviceClockMode::Query          != setClockModeInput.clockMode) &&
                                            (DeviceClockMode::QueryProfiling != setClockModeInput.clockMode) &&
                                            (DeviceClockMode::QueryPeak      != setClockModeInput.clockMode) &&
                                            (Settings().neverChangeClockMode == false);

    const char* pStrKMDInterface[]    =
    {
        "profile_exit",            // see the comments of DeviceClockMode::Default
        "profile_query",           // place holder, will not be passed to KMD (by means of needUpdateStablePState)
        "profile_standard",        // see the comments of DeviceClockMode::Profiling
        "profile_min_mclk",        // see the comments of DeviceClockMode::MinimumMemory
        "profile_min_sclk",        // see the comments of DeviceClockMode::MinimumEngine
        "profile_peak",            // see the comments of DeviceClockMode::Peak
        "profile_query_profiling", // place holder, will not be passed to KMD (by means of needUpdateStablePState)
        "profile_query_peak",      // place holder, will not be passed to KMD (by means of needUpdateStablePState)
    };

    PAL_ASSERT(static_cast<uint32>(setClockModeInput.clockMode) < sizeof(pStrKMDInterface)/sizeof(char*));

    if (needUpdateStablePState)
    {
        if (m_featureState.supportPowerDpmIoctl != 0)
        {
            uint32 amdgpuCtxStablePstate = 0;
            bool profileExit = FALSE;
            switch (setClockModeInput.clockMode)
            {
                case Pal::DeviceClockMode::Default:
                    amdgpuCtxStablePstate = AMDGPU_CTX_STABLE_PSTATE_NONE;
                    profileExit = TRUE;
                    break;
                case Pal::DeviceClockMode::Profiling:
                    amdgpuCtxStablePstate = AMDGPU_CTX_STABLE_PSTATE_STANDARD;
                    break;
                case Pal::DeviceClockMode::MinimumMemory:
                    amdgpuCtxStablePstate = AMDGPU_CTX_STABLE_PSTATE_MIN_MCLK;
                    break;
                case Pal::DeviceClockMode::MinimumEngine:
                    amdgpuCtxStablePstate = AMDGPU_CTX_STABLE_PSTATE_MIN_SCLK;
                    break;
                case Pal::DeviceClockMode::Peak:
                    amdgpuCtxStablePstate = AMDGPU_CTX_STABLE_PSTATE_PEAK;
                    break;
                default:
                    break;
            }

            if (m_hContext == nullptr)
            {
                if (m_drmProcs.pfnAmdgpuCsCtxCreate(m_hDevice, &m_hContext) != 0)
                {
                    result = Result::ErrorInvalidValue;
                }
            }

            // write with Ioctl
            if (result == Result::Success)
            {
                result = CheckResult(m_drmProcs.pfnAmdgpuCsCtxStablePstate(m_hContext,
                                                                           AMDGPU_CTX_OP_SET_STABLE_PSTATE,
                                                                           amdgpuCtxStablePstate,
                                                                           NULL),
                                     Result::ErrorInvalidValue);
            }

            if ((m_hContext != nullptr) && profileExit)
            {
                m_drmProcs.pfnAmdgpuCsCtxFree(m_hContext);
                m_hContext = nullptr;
            }
        }
        else
        {
            // prepare contents which will be written to sysfs
            static_assert(static_cast<uint32>(DeviceClockMode::Default)        == 0, "DeviceClockMode definition changed!");
            static_assert(static_cast<uint32>(DeviceClockMode::Query)          == 1, "DeviceClockMode definition changed!");
            static_assert(static_cast<uint32>(DeviceClockMode::Profiling)      == 2, "DeviceClockMode definition changed!");
            static_assert(static_cast<uint32>(DeviceClockMode::MinimumMemory)  == 3, "DeviceClockMode definition changed!");
            static_assert(static_cast<uint32>(DeviceClockMode::MinimumEngine)  == 4, "DeviceClockMode definition changed!");
            static_assert(static_cast<uint32>(DeviceClockMode::Peak)           == 5, "DeviceClockMode definition changed!");
            static_assert(static_cast<uint32>(DeviceClockMode::QueryProfiling) == 6, "DeviceClockMode definition changed!");
            static_assert(static_cast<uint32>(DeviceClockMode::QueryPeak)      == 7, "DeviceClockMode definition changed!");

            char writeBuf[MaxClockSysFsEntryNameLen];
            memset(writeBuf, 0, sizeof(writeBuf));
            snprintf(writeBuf, sizeof(writeBuf), "%s", pStrKMDInterface[static_cast<uint32>(setClockModeInput.clockMode)]);

            // write to sysfs
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
                if (m_featureState.supportQuerySensorInfo != 0)
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
                    if (m_featureState.supportQuerySensorInfo != 0)
                    {
                        result = CheckResult(m_drmProcs.pfnAmdgpuQuerySensorInfo(
                                                    m_hDevice,
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
                    requiredMclkVal = static_cast<float>(mClkInMhz);
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
            pSetClockModeOutput->engineClockFrequency = requiredSclkVal;
            pSetClockModeOutput->memoryClockFrequency = requiredMclkVal;
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
                PAL_ALERT_ALWAYS_MSG("read pp_dpm_clk info error");
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
                PAL_ALERT_ALWAYS_MSG("read pp_dpm_clk info error");
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
                PAL_ALERT_ALWAYS_MSG("read pp_dpm_clk info error");
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
Result Device::CheckExecutionState(
    PageFaultStatus* pPageFaultStatus
    ) const
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
    const auto handleType = static_cast<amdgpu_bo_handle_type>(openInfo.handleType);
    Result result = ImportBuffer(handleType,
                                 openInfo.hExternalResource,
                                 &pSharedInfo->importResult);

    if (result == Result::Success)
    {
        result  = QueryBufferInfo(pSharedInfo->importResult.buf_handle, &pSharedInfo->info);
    }

    if (result == Result::Success)
    {
        pSharedInfo->hExternalResource     = openInfo.hExternalResource;
        pSharedInfo->handleType            = handleType;
        PAL_ASSERT(pSharedInfo->importResult.alloc_size == pSharedInfo->info.alloc_size);
        PAL_ALERT_MSG((pSharedInfo->info.metadata.size_metadata == 0) && (GetPlatform()->GetDeviceCount() == 1),
            "Metadata should not be empty for BO coming from the same device. "
            "Note this might be a false alarm if you have setup like Intel GPU + AMD GPU");
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
            result = Image::GetExternalSharedImageCreateInfo(*this, openInfo, sharedInfo, &createInfo);

            if (result == Result::Success)
            {
                (*pImageSize)     = GetImageSize(createInfo, nullptr);
                (*pGpuMemorySize) = GetExternalSharedGpuMemorySize(nullptr);

                if (pImgCreateInfo != nullptr)
                {
                    memcpy(pImgCreateInfo, &createInfo, sizeof(createInfo));
                }
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

    PAL_ASSERT(openInfo.resourceInfo.flags.globalGpuVa == 0);

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
    const ExternalImageOpenInfo& openInfo,
    const ExternalSharedInfo&    sharedInfo,
    void*                        pPlacementAddr,
    GpuMemoryCreateInfo*         pCreateInfo,
    Pal::GpuMemory**             ppGpuMemory)
{
    // Require that create info is provided because we'll need it either way unlike the interface where it is optional.
    PAL_ASSERT(pCreateInfo != nullptr);

    PAL_ASSERT((sharedInfo.info.phys_alignment % 4096) == 0);
    PAL_ASSERT((sharedInfo.info.alloc_size     % 4096) == 0);

    pCreateInfo->alignment = static_cast<gpusize>(sharedInfo.info.phys_alignment);
    pCreateInfo->size      = sharedInfo.info.alloc_size;

    pCreateInfo->vaRange   = VaRange::Default;
    pCreateInfo->priority  = GpuMemPriority::High;

    if (sharedInfo.info.preferred_heap & AMDGPU_GEM_DOMAIN_GTT)
    {
        if (sharedInfo.info.alloc_flags & AMDGPU_GEM_CREATE_CPU_GTT_USWC)
        {
            pCreateInfo->heaps[pCreateInfo->heapCount++] = GpuHeapGartUswc;
        }
        else
        {
            pCreateInfo->heaps[pCreateInfo->heapCount++] = GpuHeapGartCacheable;
        }
    }

    const auto& imageCreateInfo = pImage->GetImageCreateInfo();

    if (sharedInfo.info.preferred_heap & AMDGPU_GEM_DOMAIN_VRAM)
    {
        if (sharedInfo.info.alloc_flags & AMDGPU_GEM_CREATE_NO_CPU_ACCESS)
        {
            pCreateInfo->heaps[pCreateInfo->heapCount++] = GpuHeapInvisible;
        }
        else
        {
            pCreateInfo->heaps[pCreateInfo->heapCount++] = GpuHeapLocal;
        }
    }

    GpuMemoryInternalCreateInfo internalInfo = {};
    internalInfo.flags.isExternal   = 1;
    internalInfo.hExternalResource  = sharedInfo.hExternalResource;
    internalInfo.externalHandleType = sharedInfo.handleType;

    if (pTypedBufferCreateInfo != nullptr)
    {
        PAL_ASSERT(pImage == nullptr);

        pCreateInfo->flags.typedBuffer = true;
        pCreateInfo->typedBufferInfo   = *pTypedBufferCreateInfo;
    }
    else if (pImage != nullptr)
    {
        pCreateInfo->pImage              = pImage;
        pCreateInfo->flags.flippable     = pImage->IsFlippable();
        pCreateInfo->flags.presentable   = pImage->IsPresentable();
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 723
        pCreateInfo->flags.privateScreen = (pImage->GetPrivateScreen() != nullptr);
#else
        internalInfo.flags.privateScreen = (pImage->GetPrivateScreen() != nullptr);
#endif
    }

    GpuMemory* pGpuMemory = static_cast<GpuMemory*>(ConstructGpuMemoryObject(pPlacementAddr));

    Result result = pGpuMemory->Init(*pCreateInfo, internalInfo);

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
                              MemoryProperties().virtualMemPageSize,
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
                                MemoryProperties().virtualMemPageSize,
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

    drmModeConnector* pConnector = m_drmProcs.pfnDrmModeGetConnector(m_primaryFileDescriptor, connectorId);
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

    PAL_ASSERT(pScreenCount != nullptr);

    // Enumerate connector and construct IScreen for any connected connector.
    drmModeRes* pResources = m_drmProcs.pfnDrmModeGetResources(m_primaryFileDescriptor);

    if (pResources != nullptr)
    {
        uint32 screenCount = 0;

        for (int32 i = 0; i < pResources->count_connectors; i++)
        {
            drmModeConnector* pConnector = m_drmProcs.pfnDrmModeGetConnector(m_primaryFileDescriptor,
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

                        if ((preferredWidth  < pMode->hdisplay) && (preferredHeight < pMode->vdisplay))
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

constexpr uint32 EdidExtensionLen       = 128;
constexpr uint32 EdidExtendedTagCode    = 0x07;
constexpr uint32 HdrStaticMetadataBlock = 0x06;
constexpr uint32 ColorimetryDataBlock   = 0x05;
constexpr uint32 CeaDataBlockLengthMask = 0x1f;

// =====================================================================================================================
// Helper functions to get the CEA extension from EDID
static uint8* GetCeaExtensionBlock(
    uint8* pEdidData,
    uint32 dataLength)
{
    uint8* pCeaBlock = nullptr;
    uint32 i         = 1; // Skip the base block

    while(i * EdidExtensionLen < dataLength)
    {
        if (pEdidData[i * EdidExtensionLen] == 0x02)
        {
            pCeaBlock = pEdidData + i * EdidExtensionLen;
            break;
        }

        i++;
    }

    return pCeaBlock;
}

// =====================================================================================================================
// Helper function to convert binary to decimal and mulitply 10000
static uint32 BitsToDecimal(
    uint32 value)
{

    float result = 0;

    for (uint32 i = 0; i < 10; i++)
    {
        if ((value & (1 << i)) != 0)
        {
            result += 1 / pow(2, 10 - i);
        }
    }

    return result * 10000;
}

// =====================================================================================================================
// Helper functions to get the static metadata from CEA/CTA extension
static void GetColorCharacteristicsFromEdid(
    uint8*             pEdid,
    HdrOutputMetadata* pHdrMetaData)
{

    pHdrMetaData->metadata.chromaticityRedX        = BitsToDecimal((pEdid[0x1B] << 2) | ((pEdid[0x19] & 0xC0) >> 6));
    pHdrMetaData->metadata.chromaticityRedY        = BitsToDecimal((pEdid[0x1C] << 2) | ((pEdid[0x19] & 0x30) >> 4));
    pHdrMetaData->metadata.chromaticityGreenX      = BitsToDecimal((pEdid[0x1D] << 2) | ((pEdid[0x19] & 0x0C) >> 2));
    pHdrMetaData->metadata.chromaticityGreenY      = BitsToDecimal((pEdid[0x1E] << 2) | (pEdid[0x19] & 0x03));
    pHdrMetaData->metadata.chromaticityBlueX       = BitsToDecimal((pEdid[0x1F] << 2) | ((pEdid[0x1A] & 0xC0) >> 6));
    pHdrMetaData->metadata.chromaticityBlueY       = BitsToDecimal((pEdid[0x20] << 2) | ((pEdid[0x1A] & 0x30) >> 4));
    pHdrMetaData->metadata.chromaticityWhitePointX = BitsToDecimal((pEdid[0x21] << 2) | ((pEdid[0x1A] & 0x0C) >> 2));
    pHdrMetaData->metadata.chromaticityWhitePointY = BitsToDecimal((pEdid[0x22] << 2) | (pEdid[0x1A] & 0x03));

    return;
}

// =====================================================================================================================
// Helper functions to get the static metadata and color space from CEA/CTA extension
static Result GetHdrStaticMetadataFromCea(
    uint8*             pCeaData,
    HdrOutputMetadata* pHdrMetaData)
{

    uint32 ceaVersion             = pCeaData[1];
    uint32 ceaDataBlockEndOffset  = pCeaData[2];

    Result result = Result::Success;

    if (ceaVersion < 3)
    {
        result = Result::ErrorUnavailable;
    }

    if (result == Result::Success)
    {

        bool foundMetadata = false;

        for (uint32 i = 4; i < ceaDataBlockEndOffset; i += (pCeaData[i] & CeaDataBlockLengthMask) + 1)
        {
            if (((pCeaData[i] & 0xe0) >> 5) == EdidExtendedTagCode)
            {
                if (pCeaData[i + 1] == HdrStaticMetadataBlock)
                {
                    uint32 length = pCeaData[i] & CeaDataBlockLengthMask;

                    if (length >= 3)
                    {

                        if (pCeaData[i + 2] & 0x04)
                        {
                            pHdrMetaData->metadata.eotf = HDMI_EOTF_SMPTE_ST2084;
                        }

                        if (pCeaData[i + 3] & 0x01)
                        {
                            pHdrMetaData->metadata.metadataType = HDMI_STATIC_METADATA_TYPE1;
                        }

                        uint8 codeValue = (length >= 4) ? pCeaData[i + 4] : 0;

                        if (codeValue > 0)
                        {
                            pHdrMetaData->metadata.maxLuminance = 50 * pow(2, codeValue/32.0);
                        }
                        else
                        {
                            // When there is no desired max luminance in EDID, set maxLuminance 0 to indicate
                            // max luminance is unknown.
                            pHdrMetaData->metadata.maxLuminance = 0;
                        }

                        codeValue = (length >= 5) ? pCeaData[i + 5] : 0;

                        if (codeValue > 0)
                        {
                            pHdrMetaData->metadata.maxFrameAverageLightLevel = 50 * pow(2, codeValue/32.0);
                        }
                        else
                        {
                            pHdrMetaData->metadata.maxFrameAverageLightLevel = 0;
                        }

                        codeValue                           = (length >= 6) ? pCeaData[i + 6] : 0;
                        pHdrMetaData->metadata.minLuminance = pHdrMetaData->metadata.maxLuminance *
                                                              (pow(codeValue/255.0, 2) / 100) * 10000;

                        foundMetadata = true;

                        break;
                    }
                }
            }
        }

        if (foundMetadata == false)
        {
            result = Result::ErrorUnavailable;
        }
    }

    return result;
}

// =====================================================================================================================
// Get HDR metadata, return ErrorUnavailable to indicate either KMD or connector/monitor doesn't support HDR
Result Device::GetHdrMetaData(
    uint32             connectorId,
    HdrOutputMetadata* pHdrMetaData
    ) const
{
    // There is no libdrm interface or properties to query if kernel driver (amdgpu) support HDR. To workaround this
    // issue, parsing EDID directly to get the  metadata and check if there is any property named HDR_OUTPUT_METADATA?
    // No means amdgpu doesn't support HDR. Hopefully KMD will provide an approach to query the capability in the
    // future.

    drmModeObjectPropertiesPtr pProps = m_drmProcs.pfnDrmModeObjectGetProperties(m_primaryFileDescriptor,
                                                                                 connectorId,
                                                                                 DRM_MODE_OBJECT_CONNECTOR);

    bool driverSupportHdr    = false;
    bool connectorSupportHdr = false;

    Result result = (pProps == nullptr) ? Result::ErrorOutOfMemory : Result::Success;

    for (uint32 i = 0; (result == Result::Success) && (i < pProps->count_props); i++)
    {
        uint32             propId    = pProps->props[i];
        uint64             propValue = pProps->prop_values[i];
        drmModePropertyPtr pProp     = m_drmProcs.pfnDrmModeGetProperty(m_primaryFileDescriptor, propId);

        if (pProp == nullptr)
        {
            result = Result::ErrorOutOfMemory;
            break;
        }

        if (strcmp(pProp->name, "HDR_OUTPUT_METADATA") == 0)
        {
            driverSupportHdr = true;
        }
        else if (strcmp(pProp->name, "EDID") == 0)
        {
            // Get the EDID and parse it to get metadata
            PAL_ASSERT((pProp->flags & DRM_MODE_PROP_BLOB) != 0);

            drmModePropertyBlobPtr pBlob = nullptr;

            pBlob = m_drmProcs.pfnDrmModeGetPropertyBlob(m_primaryFileDescriptor, propValue);

            if (pBlob == nullptr)
            {
                result = Result::ErrorUnavailable;
                break;
            }

            uint8* pCeaData = GetCeaExtensionBlock(reinterpret_cast<uint8*>(pBlob->data), pBlob->length);

            if (pCeaData != nullptr)
            {
                result = GetHdrStaticMetadataFromCea(pCeaData, pHdrMetaData);
            }
            else
            {
                result = Result::ErrorUnavailable;
            }

            if (result == Result::Success)
            {
                GetColorCharacteristicsFromEdid(reinterpret_cast<uint8*>(pBlob->data), pHdrMetaData);
                connectorSupportHdr = true;
            }

            m_drmProcs.pfnDrmModeFreePropertyBlob(pBlob);
        }

        m_drmProcs.pfnDrmModeFreeProperty(pProp);
    }

    m_drmProcs.pfnDrmModeFreeObjectProperties(pProps);

    if ((result == Result::Success) && ((driverSupportHdr == false) || (connectorSupportHdr == false)))
    {
        result = Result::ErrorUnavailable;
    }

    return result;
}

// =====================================================================================================================
// Set HDR metadata and "max bpc" 10 to enable the HDR display pipeline.
Result Device::SetHdrMetaData(
    int32              drmMasterFd,
    uint32             connectorId,
    HdrOutputMetadata* pHdrMetaData
    ) const
{
    uint32                     blobId = 0;
    int32 drmFd = (drmMasterFd != InvalidFd) ? drmMasterFd : m_primaryFileDescriptor;
    drmModeObjectPropertiesPtr pProps = m_drmProcs.pfnDrmModeObjectGetProperties(drmFd,
                                                                                 connectorId,
                                                                                 DRM_MODE_OBJECT_CONNECTOR);

    drmModeAtomicReqPtr pAtomicRequest = m_drmProcs.pfnDrmModeAtomicAlloc();

    bool maxBpcSet   = false;
    bool metaDataSet = false;

    Result result = CheckResult(m_drmProcs.pfnDrmModeCreatePropertyBlob(drmFd,
                                                                        reinterpret_cast<void*>(pHdrMetaData),
                                                                        sizeof(*pHdrMetaData),
                                                                        &blobId),
                                Result::ErrorInvalidValue);

    if ((pProps == nullptr) || (pAtomicRequest == nullptr))
    {
        result = Result::ErrorOutOfMemory;
    }

    for (uint32 i = 0; (result == Result::Success) && (i < pProps->count_props) && (!maxBpcSet || !metaDataSet); i++)
    {
        uint32             propId    = pProps->props[i];
        uint64             propValue = pProps->prop_values[i];
        drmModePropertyPtr pProp     = m_drmProcs.pfnDrmModeGetProperty(drmFd, propId);

        if (pProp == nullptr)
        {
            result = Result::ErrorOutOfMemory;
            break;
        }

        if (strcmp(pProp->name, "max bpc") == 0)
        {
            // Increase "max bpc" to at least 10 bits, as needed by HDR-10, if the current limit is lower.
            if (propValue < 10)
            {
                result = (m_drmProcs.pfnDrmModeAtomicAddProperty(pAtomicRequest, connectorId, propId, 10) < 0) ?
                            Result::ErrorInvalidValue : Result::Success;
            }

            maxBpcSet = true;
        }
        else if (strcmp(pProp->name, "HDR_OUTPUT_METADATA") == 0)
        {
            result = (m_drmProcs.pfnDrmModeAtomicAddProperty(pAtomicRequest, connectorId, propId, blobId) < 0) ?
                         Result::ErrorInvalidValue : Result::Success;;

            metaDataSet = true;
        }

        m_drmProcs.pfnDrmModeFreeProperty(pProp);
    }

    if ((result == Result::Success) && maxBpcSet && metaDataSet)
    {
        m_drmProcs.pfnDrmSetClientCap(drmFd, DRM_CLIENT_CAP_ATOMIC, 1);
        result = CheckResult(m_drmProcs.pfnDrmModeAtomicCommit(drmFd,
                                                               pAtomicRequest,
                                                               DRM_MODE_ATOMIC_ALLOW_MODESET,
                                                               nullptr),
                             Result::ErrorInvalidValue);
    }
    else if (result == Result::Success)
    {
        result = Result::ErrorUnavailable;
    }

    if (blobId > 0)
    {
        m_drmProcs.pfnDrmModeDestroyPropertyBlob(drmFd, blobId);
    }

    if (pAtomicRequest != nullptr)
    {
        m_drmProcs.pfnDrmModeAtomicFree(pAtomicRequest);
    }

    return result;
}

// =====================================================================================================================
Result Device::QueryWorkStationCaps(
    WorkStationCaps* pCaps
    ) const
{
    Result result = Result::ErrorUnavailable;

    return result;
}

// =====================================================================================================================
// Tell if the present device is same as rendering device
Result Device::IsSameGpu(
    int32 presentDeviceFd,
    bool* pIsSame
    ) const
{
    Result result                   = Result::Success;
    char   busId[MaxBusIdStringLen] = {};

    *pIsSame = false;
     drmDevicePtr presentDevice = nullptr;

     int32 ret = m_drmProcs.pfnDrmGetDevice2(presentDeviceFd, 0, &presentDevice);
     if (ret)
      {
         result = Result::ErrorUnknown;
      }
      else
      {
         PAL_ASSERT(presentDevice->bustype == DRM_BUS_PCI);

         Util::Snprintf(busId,
                        MaxBusIdStringLen,
                        "pci:%04x:%02x:%02x.%d",
                        presentDevice->businfo.pci->domain,
                        presentDevice->businfo.pci->bus,
                        presentDevice->businfo.pci->dev,
                        presentDevice->businfo.pci->func);

         m_drmProcs.pfnDrmFreeDevice(&presentDevice);
        }

    if (result == Result::Success)
    {
        *pIsSame = (Util::Strcasecmp(&m_busId[0], busId) == 0);
    }

    return result;
}

// =====================================================================================================================
// Tell if the present device is same as rendering device
Result Device::IsSameGpu(
    char* pDeviceName,
    bool* pIsSame
    ) const
{
    Result result                   = Result::Success;

    *pIsSame = (Util::Strcasecmp(&m_primaryNodeName[0], pDeviceName) == 0) ||
               (Util::Strcasecmp(&m_renderNodeName[0], pDeviceName) == 0);

    return result;
}

// =====================================================================================================================
// Helper function for Vam manager to allocate va range
Result Device::AllocVaRange(
    uint64            size,
    uint64            vaBaseRequired,
    uint64*           pVaAllocated,
    amdgpu_va_handle* pVaRange
    ) const
{
    return CheckResult(m_drmProcs.pfnAmdgpuVaRangeAlloc(m_hDevice,
                                                        amdgpu_gpu_va_range_general,
                                                        size,
                                                        m_memoryProperties.fragmentSize,
                                                        vaBaseRequired,
                                                        pVaAllocated,
                                                        pVaRange,
                                                        0),
                       Result::ErrorUnknown);
}

// =====================================================================================================================
// Helper function for Vam manager to free va range
void Device::FreeVaRange(
    amdgpu_va_handle hVaRange
    ) const
{
    m_drmProcs.pfnAmdgpuVaRangeFree(hVaRange);
}

// =====================================================================================================================
// Add bo and va information into shared bo map
bool Device::AddToSharedBoMap(
    amdgpu_bo_handle hBuffer,
    amdgpu_va_handle hVaRange,
    gpusize          gpuVirtAddr)
{
    return m_pVamMgr->AddToSharedBoMap(hBuffer, hVaRange, gpuVirtAddr);
}

// =====================================================================================================================
// Remove bo information from shared bo map
bool Device::RemoveFromSharedBoMap(
    amdgpu_bo_handle hBuffer)
{
    return m_pVamMgr->RemoveFromSharedBoMap(hBuffer);
}

// =====================================================================================================================
// Search bo handle in shared bo map to get va information
amdgpu_va_handle Device::SearchSharedBoMap(
    amdgpu_bo_handle hBuffer,
    gpusize*         pGpuVirtAddr)
{
    return m_pVamMgr->SearchSharedBoMap(hBuffer, pGpuVirtAddr);
}

} // Amdgpu
} // Pal
