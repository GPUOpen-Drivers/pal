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

#pragma once

#include "core/device.h"
#include "core/os/lnx/lnxHeaders.h"
#include "palSettingsFileMgr.h"
#include "palHashMap.h"
#include "core/os/lnx/drmLoader.h"
#include "core/os/lnx/lnxPlatform.h"
#include "core/os/lnx/lnxGpuMemory.h"
#include "core/svmMgr.h"
namespace Pal
{
namespace Linux
{

class Image;
class WindowSystem;

static constexpr size_t MaxBusIdStringLen         = 20;
static constexpr size_t MaxNodeNameLen            = 32;
static constexpr size_t MaxClockInfoCount         = 16;
static constexpr size_t MaxClockSysFsEntryNameLen = 100;
static constexpr size_t ClockInfoReadBufLen       = 4096;

enum class SemaphoreType : uint32
{
    Legacy   = 1 << 0,
    ProOnly  = 1 << 1,
    SyncObj  = 1 << 2,
};
enum class FenceType : uint32
{
    Legacy   = 1 << 0,
    SyncObj  = 1 << 2,
};

// All information necessary to create PAL image/memory object from an external shared resource. It is used to return
// information from OpenExternalResource().
struct ExternalSharedInfo
{
    Pal::OsExternalHandle   hExternalResource;  // External resource handle
    amdgpu_bo_import_result importResult;       // Status of the shared resource import.
    amdgpu_bo_info          info;               // DRM's internal allocation info.
};

// =====================================================================================================================
// Linux flavor of the Device class. Objects of this class are responsible for managing virtual address space via VAM
// and implementing the factory methods exposed by the public IDevice interface.
class Device : public Pal::Device
{
public:
    static Result Create(
        Platform*               pPlatform,
        const char*             pSettingsPath,
        const char*             pBusId,
        const char*             pPrimaryNode,
        const char*             pRenderNode,
        const drmPciBusInfo&    pciBusInfo,
        uint32                  deviceIndex,
        Device**                ppDeviceOut);

    Device(
        Platform*                   pPlatform,
        const char*                 pSettingsPath,
        const char*                 pBusId,
        const char*                 pRenderNode,
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
        const drmPciBusInfo&        pciBusInfo);

    virtual ~Device();

    virtual Result Finalize(const DeviceFinalizeInfo& finalizeInfo) override;
    virtual Result Cleanup() override;

    // NOTE: Part of the public IDevice interface.
    virtual Result GetProperties(
        DeviceProperties* pInfo) const override;

    // NOTE: Part of the public IDevice interface.
    virtual Result AllocateGds(
        const DeviceGdsAllocInfo&   requested,
        DeviceGdsAllocInfo*         pAllocated) override;

    virtual Result CalibrateGpuTimestamp(
        GpuTimestampCalibration* pCalibrationData) const override;

    virtual Result GetMultiGpuCompatibility(
            const IDevice&        otherDevice,
            GpuCompatibilityInfo* pInfo) const override;

    virtual bool ReadSetting(
            const char*          pSettingName,
            Util::ValueType      valueType,
            void*                pValue,
            InternalSettingScope settingType,
            size_t               bufferSz = 0) const override;

    virtual Result QueryApplicationProfile(
            const char*         pFilename,
            const char*         pPathname,
            ApplicationProfile* pOut) const override { return Result::Unsupported; }

    virtual Result QueryRawApplicationProfile(
            const char*              pFilename,
            const char*              pPathname,
            ApplicationProfileClient client,
            const char**             pOut) override { return Result::Unsupported; }

    virtual bool IsMasterGpu() const override { return true; }

    virtual Result AddGpuMemoryReferences(
        uint32              gpuMemRefCount,
        const GpuMemoryRef* pGpuMemoryRefs,
        IQueue*             pQueue,
        uint32              flags) override;

    virtual Result RemoveGpuMemoryReferences(
        uint32            gpuMemoryCount,
        IGpuMemory*const* ppGpuMemory,
        IQueue*           pQueue) override;

    virtual Result SetMaxQueuedFrames(
        uint32 maxFrames) override;

    virtual Result OpenExternalSharedGpuMemory(
        const ExternalGpuMemoryOpenInfo& openInfo,
        void*                            pPlacementAddr,
        GpuMemoryCreateInfo*             pMemCreateInfo,
        IGpuMemory**                     ppGpuMemory) override;

    virtual void GetPresentableImageSizes(
        const PresentableImageCreateInfo& createInfo,
        size_t*                           pImageSize,
        size_t*                           pGpuMemorySize,
        Result*                           pResult) const override;

    virtual Result CreatePresentableImage(
        const PresentableImageCreateInfo& createInfo,
        void*                             pImagePlacementAddr,
        void*                             pGpuMemoryPlacementAddr,
        IImage**                          ppImage,
        IGpuMemory**                      ppGpuMemory) override;

    virtual Result GetPrivateScreens(
        uint32*          pNumScreens,
        IPrivateScreen** ppScreens) override { return Result::ErrorUnavailable; }

    virtual size_t GetImageSize(
        const ImageCreateInfo& createInfo,
        Result*                pResult) const override;

    // NOTE: Part of the public IDevice interface.
    virtual Result CreateImage(
        const ImageCreateInfo& createInfo,
        void*                  pPlacementAddr,
        IImage**               ppImage) override;

    virtual Result CreateInternalImage(
        const ImageCreateInfo&         createInfo,
        const ImageInternalCreateInfo& internalCreateInfo,
        void*                          pPlacementAddr,
        Pal::Image**                   ppImage) override;

    virtual Result DetermineExternalSharedResourceType(
        const ExternalResourceOpenInfo& openInfo,
        bool*                           pIsImage) const override { return Result::ErrorUnavailable; }

    virtual Result GetExternalSharedImageSizes(
        const ExternalImageOpenInfo& openInfo,
        size_t*                      pImageSize,
        size_t*                      pGpuMemorySize,
        ImageCreateInfo*             pImgCreateInfo) const override;

    virtual Result OpenExternalSharedImage(
        const ExternalImageOpenInfo& openInfo,
        void*                        pImagePlacementAddr,
        void*                        pGpuMemoryPlacementAddr,
        GpuMemoryCreateInfo*         pMemCreateInfo,
        IImage**                     ppImage,
        IGpuMemory**                 ppGpuMemory) override;

    virtual Result AddEmulatedPrivateScreen(
        const PrivateScreenCreateInfo& createInfo,
        uint32*                        pTargetId) override { return Result::ErrorUnavailable; }

    virtual Result RemoveEmulatedPrivateScreen(
        uint32 targetId) override { return Result::ErrorUnavailable; }

    virtual Result GetSwapChainInfo(
        OsDisplayHandle      hDisplay,
        OsWindowHandle       hWindow,
        Pal::WsiPlatform     wsiPlatform,
        SwapChainProperties* pSwapChainProperties) override;

    virtual Result DeterminePresentationSupported(
        OsDisplayHandle      hDisplay,
        Pal::WsiPlatform     wsiPlatform,
        int64                visualId) override;

    virtual size_t GetSwapChainSize(
        const SwapChainCreateInfo& createInfo,
        Result*                    pResult) const override;

    virtual Result CreateSwapChain(
        const SwapChainCreateInfo& createInfo,
        void*                      pPlacementAddr,
        ISwapChain**               ppSwapChain) override;

    virtual Result SetPowerProfile(
        PowerProfile        profile,
        CustomPowerProfile* pInfo) override { return Result::ErrorUnavailable; }

    virtual Result QueryWorkStationCaps(
        WorkStationCaps* pCaps) const override { return Result::ErrorUnavailable; }

    virtual Result QueryDisplayConnectors(
        uint32*                     pConnectorCount,
        DisplayConnectorProperties* pConnectors) override { return Result::ErrorUnavailable; }

    virtual Result AddQueue(
        Pal::Queue* pQueue) override;

    virtual Result GetPrimaryInfo(
        const GetPrimaryInfoInput&  primaryInfoInput,
        GetPrimaryInfoOutput*       pPrimaryInfoOutput) const override { return Result::ErrorUnavailable; }

    virtual Result GetStereoDisplayModes(
        uint32*                   pStereoModeCount,
        StereoDisplayModeOutput*  pStereoModeList) const override { return Result::ErrorUnavailable; }

    virtual Result GetActive10BitPackedPixelMode(
        Active10BitPackedPixelModeOutput* pMode) const override { return Result::ErrorUnavailable; }

    virtual Result RequestKmdReinterpretAs10Bit(
        const IGpuMemory* pGpuMemory) const override { return Result::ErrorUnavailable; }

    // TODO: Add implementation once amdgpu support is done.
    virtual Result SetClockMode(
        const SetClockModeInput& setClockModeInput,
        SetClockModeOutput*      pSetClockModeOutput) override;

    virtual Result SetMgpuMode(
        const SetMgpuModeInput& setMgpuModeInput) const override { return Result::ErrorUnavailable; }

    virtual Result GetXdmaInfo(
        uint32              vidPnSrcId,
        const IGpuMemory&   gpuMemory,
        GetXdmaInfoOutput*  pGetXdmaInfoOutput) const override { return Result::ErrorUnavailable; }

    virtual Result PollFullScreenFrameMetadataControl(
        uint32                         vidPnSrcId,
        PerSourceFrameMetadataControl* pFrameMetadataControl) const override { return Result::ErrorUnavailable; }

    virtual Result FlglQueryState(
        Pal::FlglState* pState) override
    {
        PAL_NOT_IMPLEMENTED();
        return Result::ErrorUnavailable;
    }

    virtual Result FlglSetFrameLock(
        bool enable) override
    {
       PAL_NOT_IMPLEMENTED();
       return Result::ErrorUnavailable;
    }

    virtual Result FlglResetFrameCounter() const override
    {
       PAL_NOT_IMPLEMENTED();
       return Result::ErrorUnavailable;
    }

    virtual Result FlglGetFrameCounterResetStatus(
        bool* pReset) const override
    {
       PAL_NOT_IMPLEMENTED();
       return Result::ErrorUnavailable;
    }

    virtual Result FlglGetFrameCounter(
        uint64* pValue) const override
    {
       PAL_NOT_IMPLEMENTED();
       return Result::ErrorUnavailable;
    }

    virtual Result DidChillSettingsChange(
        bool* pChangeDetected) override
    {
       PAL_NOT_IMPLEMENTED();
       if (pChangeDetected != nullptr)
       {
           *pChangeDetected = false;
       }
       return Result::ErrorUnavailable;
    }

    virtual Result GetChillGlobalEnable(
        bool* pGlobalEnable) override
    {
       PAL_NOT_IMPLEMENTED();
       if (pGlobalEnable != nullptr)
       {
           *pGlobalEnable = false;
       }
       return Result::ErrorUnavailable;
    }

    virtual Result CreateVirtualDisplay(
        const VirtualDisplayInfo& virtualDisplayInfo,
        uint32*                   pScreenTargetId) override
    {
        PAL_NOT_IMPLEMENTED();
        return Result::ErrorUnavailable;
    }

    virtual Result DestroyVirtualDisplay(
        uint32     screenTargetId) override
    {
        PAL_NOT_IMPLEMENTED();
        return Result::ErrorUnavailable;
    };

    virtual Result GetVirtualDisplayProperties(
        uint32                    screenTargetId,
        VirtualDisplayProperties* pProperties) override
    {
        PAL_NOT_IMPLEMENTED();
        return Result::ErrorUnavailable;
    }

    virtual Result CheckExecutionState() const override;

    bool IsVmAlwaysValidSupported() const { return m_supportVmAlwaysValid; }

    // Access KMD interfaces
    Result AllocBuffer(
        struct amdgpu_bo_alloc_request* pAllocRequest,
        amdgpu_bo_handle*               pBufferHandle) const;

    Result PinMemory(
        const void*                     pCpuAddress,
        uint64                          size,
        uint64*                         pOffset,
        amdgpu_bo_handle*               pBufferHandle) const;

    Result FreeBuffer(
        amdgpu_bo_handle hBuffer) const;

    Result ExportBuffer(
        amdgpu_bo_handle                hBuffer,
        enum amdgpu_bo_handle_type      type,
        uint32*                         pSharedHandle) const;

    Result ImportBuffer(
        enum amdgpu_bo_handle_type      type,
        uint32                          sharedHandle,
        struct amdgpu_bo_import_result* pOutput) const;

    Result QueryBufferInfo(
        amdgpu_bo_handle        hBuffer,
        struct amdgpu_bo_info*  pInfo) const;

    Result Map(
        amdgpu_bo_handle hBuffer,
        void**           ppCpu) const;

    Result Unmap(
        amdgpu_bo_handle hBuffer) const;

    Result MapVirtualAddress(
        amdgpu_bo_handle hBuffer,
        uint64           offset,
        uint64           size,
        uint64           virtualAddress,
        MType            mtype) const;

    Result UnmapVirtualAddress(
        amdgpu_bo_handle hBuffer,
        uint64           offset,
        uint64           size,
        uint64           virtualAddress) const;

    Result ReservePrtVaRange(
        uint64  virtualAddress,
        uint64  size,
        MType   mtype) const;

    Result DiscardReservedPrtVaRange(
        uint64  virtualAddress,
        uint64  size) const;

    Result ReplacePrtVirtualAddress(
        amdgpu_bo_handle hBuffer,
        uint64           offset,
        uint64           size,
        uint64           virtualAddress,
        MType            mtype) const;

    Result WaitBufferIdle(
        amdgpu_bo_handle hBuffer,
        uint64           timeoutNs,
        bool*            pBufferBusy) const;

    Result CreateCommandSubmissionContext(
        amdgpu_context_handle* pContextHandle,
        QueuePriority          priority) const;

    Result DestroyCommandSubmissionContext(
        amdgpu_context_handle hContext) const;

    Result SubmitRaw(
        amdgpu_context_handle           hContext,
        amdgpu_bo_list_handle           boList,
        uint32                          chunkCount,
        struct drm_amdgpu_cs_chunk*     pChunks,
        uint64*                         pFence) const;

    Result Submit(
        amdgpu_context_handle     hContext,
        uint64                    flags,
        struct amdgpu_cs_request* pIbsRequest,
        uint32                    numberOfRequests,
        uint64*                   pFences) const;

    virtual size_t GetFenceSize(
        Result* pResult) const override;

    // NOTE: Part of the public IDevice interface.
    virtual Result CreateFence(
        const FenceCreateInfo& createInfo,
        void*                  pPlacementAddr,
        IFence**               ppFence) const override;

    // NOTE: Part of the public IDevice interface.
    virtual Result OpenFence(
        const FenceOpenInfo& openInfo,
        void*                pPlacementAddr,
        IFence**             ppFence) const override;

    Result QueryFenceStatus(
        struct amdgpu_cs_fence* pFence,
        uint64                  timeoutNs) const;

    Result WaitForFences(
        amdgpu_cs_fence* pFences,
        uint32           fenceCount,
        bool             waitAll,
        uint64           timeout) const;

    Result WaitForSyncobjFences(
        uint32_t*            pFences,
        uint32               fenceCount,
        uint64               timeout,
        uint32               flags,
        uint32*              pFirstSignaled) const;

    Result ResetSyncObject(
        uint32_t*            pFences,
        uint32_t             fenceCount) const;

    bool IsInitialSignaledSyncobjSemaphoreSupported() const
        { return m_syncobjSupportState.InitialSignaledSyncobjSemaphore == 1; }

    Result ReadRegisters(
        uint32  dwordOffset,
        uint32  count,
        uint32  instance,
        uint32  flags,
        uint32* pValues) const;

    Result CreateResourceList(
        uint32                 numberOfResources,
        amdgpu_bo_handle*      resources,
        uint8*                 pResourcePriorities,
        amdgpu_bo_list_handle* pListHandle) const;

    Result DestroyResourceList(
        amdgpu_bo_list_handle handle) const;

    Result IsSameGpu(
        int32 presentDeviceFd,
        bool* pIsSame) const;

    Result CreateSyncObject(
        uint32                    flags,
        amdgpu_syncobj_handle*    pSyncObject) const;

    Result DestroySyncObject(
        amdgpu_syncobj_handle    syncObject) const;

    OsExternalHandle ExportSyncObject(
        amdgpu_syncobj_handle    syncObject) const;

    Result ImportSyncObject(
        OsExternalHandle          fd,
        amdgpu_syncobj_handle*    pSyncObject) const;

    Result ConveySyncObjectState(
        amdgpu_syncobj_handle    importSyncObj,
        amdgpu_syncobj_handle    exportSyncObj) const;

    Result CreateSemaphore(
        bool                     isCreatedSignaled,
        amdgpu_semaphore_handle* pSemaphoreHandle) const;

    Result DestroySemaphore(
        amdgpu_semaphore_handle hSemaphore) const;

    Result WaitSemaphore(
        amdgpu_context_handle   hContext,
        uint32                  ipType,
        uint32                  ipInstance,
        uint32                  ring,
        amdgpu_semaphore_handle hSemaphore) const;

    Result SignalSemaphore(
        amdgpu_context_handle   hContext,
        uint32                  ipType,
        uint32                  ipInstance,
        uint32                  ring,
        amdgpu_semaphore_handle hSemaphore) const;

    OsExternalHandle ExportSemaphore(
        amdgpu_semaphore_handle hSemaphore) const;

    Result ImportSemaphore(
        OsExternalHandle         fd,
        amdgpu_semaphore_handle* pSemaphoreHandle) const;

    Result AssignVirtualAddress(
        Pal::GpuMemory*         pGpuMemory,
        gpusize*                pGpuVirtAddr);

    void FreeVirtualAddress(
        Pal::GpuMemory*         pGpuMemory);

    // Reserve gpu virtual address range. This function is called by SVM manager 'SvmMgr'
    virtual Result ReserveGpuVirtualAddress(VaRange                 vaRange,
                                            gpusize                 baseVirtAddr,
                                            gpusize                 size,
                                            bool                    isVirtual,
                                            VirtualGpuMemAccessMode virtualAccessMode,
                                            gpusize*                pGpuVirtAddr) override;

    // Free gpu virtual address. This function is called by SVM manager 'SvmMgr'
    virtual Result FreeGpuVirtualAddress(gpusize vaStartAddress, gpusize vaSize) override;

    virtual Result GetFlipStatus(
        uint32           vidPnSrcId,
        FlipStatusFlags* pFlipFlags,
        bool*            pIsFlipOwner) const override { return Result::ErrorUnavailable; }

    Result CreateGpuMemoryFromExternalShare(
        const TypedBufferCreateInfo* pTypedBufferCreateInfo,
        Pal::Image*                  pImage,
        const ExternalSharedInfo&    sharedInfo,
        void*                        pPlacementAddr,
        GpuMemoryCreateInfo*         pCreateInfo,
        Pal::GpuMemory**             ppGpuMemory);

    // Member access
    amdgpu_device_handle DeviceHandle() const {return m_hDevice;}

    Platform* GetPlatform() const { return reinterpret_cast<Linux::Platform*>(m_pPlatform); }

    int32 GetFileDescriptor() const { return m_fileDescriptor; }

    bool GetPresentSupport(QueueType type) const { return m_supportsPresent[type]; }

    void UpdateMetaData(
        amdgpu_bo_handle    hBuffer,
        const Image&        image);

    void UpdateImageInfo(
        amdgpu_bo_handle    hBuffer,
        Image*              image);

    virtual Result UpdateExternalImageInfo(
        const PresentableImageCreateInfo&  createInfo,
        Pal::GpuMemory*                    pGpuMemory,
        Pal::Image*                        pImage);

    virtual Result CreatePresentableMemoryObject(
        Image*           pImage,
        void*            pMemObjMem,
        OsDisplayHandle  sharedHandle,
        Pal::GpuMemory** ppMemObjOut);

    virtual const char* GetCacheFilePath() const override;

    virtual void OverrideDefaultSettings(PalSettings* pSettings) const override {};

    void SetVaRangeInfo(
        uint32       partIndex,
        VaRangeInfo* pVaRange);

    bool SemWaitRequiresSubmission() const { return m_semType != SemaphoreType::ProOnly; }

    bool SupportRawSubmit() const
    {
        return m_drmProcs.pfnAmdgpuCsSubmitRawisValid();
    }

    SemaphoreType GetSemaphoreType() const { return m_semType; }
    FenceType     GetFenceType()     const { return m_fenceType; }

    Result SyncObjImportSyncFile(
        int                     syncFileFd,
        amdgpu_syncobj_handle   syncObj) const;

    Result  SyncObjExportSyncFile(
        amdgpu_syncobj_handle   syncObj,
        int*                    pSyncFileFd) const;

    Result InitReservedVaRanges();

    virtual Result InitBusAddressableGpuMemory(
        IQueue*           pQueue,
        uint32            gpuMemCount,
        IGpuMemory*const* ppGpuMemList) override;

    Result QuerySdiSurface(
        amdgpu_bo_handle    hSurface,
        uint64*             pPhysAddress);

    Result SetSdiSurface(
        GpuMemory*  pGpuMem,
        gpusize*    pCardAddr);

    Result FreeSdiSurface(GpuMemory* pGpuMem);

    SvmMgr* GetSvmMgr() const { return m_pSvmMgr; }

protected:
    virtual void FinalizeQueueProperties() override;

    virtual size_t QueueObjectSize(
        const QueueCreateInfo& createInfo) const override;

    virtual Pal::Queue* ConstructQueueObject(
        const QueueCreateInfo& createInfo,
        void*                  pPlacementAddr) override;

    virtual size_t GpuMemoryObjectSize() const override;

    virtual Pal::GpuMemory* ConstructGpuMemoryObject(
        void* pPlacementAddr) override;

    virtual Result EnumPrivateScreensInfo(
        uint32* pNumScreen) override { return Result::ErrorUnavailable; }

    virtual Result OsLateInit() override;

private:
    virtual Result OsEarlyInit() override;

    virtual Result EarlyInit(const HwIpLevels& ipLevels) override;

    bool IsDrmVersionOrGreater(uint32 drmMajorVer, uint32 drmMinorVer) const
    {
        bool isDrmVersionOrGreater = false;

        if ((m_drmMajorVer > drmMajorVer) || ((m_drmMajorVer == drmMajorVer) && (m_drmMinorVer >= drmMinorVer)))
        {
            isDrmVersionOrGreater = true;
        }

        return isDrmVersionOrGreater;
    }

    bool IsKernelVersionEqualOrGreater(
        uint32    kernelMajorVer,
        uint32    kernelMinorVer
        ) const;

    void CheckSyncObjectSupportStatus();

    Result OpenExternalResource(
        const ExternalResourceOpenInfo& openInfo,
        ExternalSharedInfo*             pSharedInfo
        ) const;

    Result InitGpuProperties();
    Result InitMemQueueInfo();

#if PAL_BUILD_GFX6
    void InitGfx6ChipProperties();
    void InitGfx6CuMask();
#endif

#if PAL_BUILD_GFX9
    void InitGfx9ChipProperties();
    void InitGfx9CuMask();
#endif

    const uint32 GetDeviceNodeIndex() { return m_deviceNodeIndex; }

    Result MapSdiMemory(
        amdgpu_device_handle    hDevice,
        uint64                  busAddress,
        gpusize                 size,
        amdgpu_bo_handle&       hBuffer,
        amdgpu_va_handle&       hVaRange,
        uint64&                 vaAllocated);

    Result UnmapSdiMemory(
        uint64                  virtAddress,
        gpusize                 size,
        amdgpu_bo_handle        hBuffer,
        amdgpu_va_handle        hVaRange);

    int32                 m_fileDescriptor;         // File descriptor used for communicating with the kernel driver
    amdgpu_device_handle  m_hDevice;                // Device handle of the amdgpu
    amdgpu_context_handle m_hContext;               // Context handle of the amdgpu device
    const uint32          m_deviceNodeIndex;        // The device node index in the system, with this node, driver could
                                                    // open the device with /dev/dri/card+m_deviceNodeIndex.

    uint32 const         m_drmMajorVer;
    uint32 const         m_drmMinorVer;
    char                 m_busId[MaxBusIdStringLen];             // Device bus Id name string.
    char                 m_primaryNodeName[MaxNodeNameLen];      // Name string of primary node.
    char                 m_renderNodeName[MaxNodeNameLen];       // Name string of render node.
    amdgpu_gpu_info      m_gpuInfo;                              // Gpu info queried from kernel
    bool                 m_supportsPresent[QueueTypeCount];      // Indicates if each queue type supports presents.

    bool                 m_useDedicatedVmid;         // Indicate if use per-process VMID.
    bool                 m_supportExternalSemaphore; // Indicate if external semaphore is supported.

    const char*                     m_pSettingsPath;
    Util::SettingsFileMgr<Platform> m_settingsMgr;

    SvmMgr* m_pSvmMgr;

    struct ReservedVaRangeInfo
    {
        gpusize size;
        amdgpu_va_handle vaHandle;
    };
    typedef Util::HashMap<gpusize, ReservedVaRangeInfo, GenericAllocatorAuto> ReservedVaMap;
    GenericAllocatorAuto m_mapAllocator;
    ReservedVaMap m_reservedVaMap;

    // Store information of shader and memory clock.
    // For example(cat /sys/class/drm/card0/device/pp_dpm_mclk):
    // 0: 150Mhz
    // 1: 1375Mhz *
    struct ClockInfo
    {
        uint32 level;     // clock level, index of specific value.
        uint32 value;     // clock value, in Mhz.
        bool   isCurrent; // '*' postfix means it's the current clock level.
    };
    typedef Util::Vector<ClockInfo, MaxClockInfoCount, Pal::Platform> ClkInfo;
    char    m_forcePerformanceLevelPath[MaxClockSysFsEntryNameLen];
    char    m_sClkPath[MaxClockSysFsEntryNameLen];
    char    m_mClkPath[MaxClockSysFsEntryNameLen];
    bool    m_supportQuerySensorInfo;
    static Result ParseClkInfo(const char* pFilePath, ClkInfo* pClkInfo, uint32* pCurIndex);
    Result        InitClkInfo();

    // NOTE: There are no API level residency functions on the queue and so references are added/removed by the device.
    //       Only submit-level residency model is supported so we need to populate the device global list to the queue.
    //       Then, at submit time the queue will submit it's own allocation list
    void RemoveFromGlobalList(uint32 gpuMemoryCount, IGpuMemory*const* ppGpuMemory);
    void AddToGlobalList(uint32 gpuMemRefCount, const GpuMemoryRef* pGpuMemoryRefs);

    typedef Util::HashMap<IGpuMemory*, uint32, Pal::Platform> MemoryRefMap;
    MemoryRefMap m_globalRefMap;
    Util::Mutex  m_globalRefLock;
    static constexpr uint32 MemoryRefMapElements = 2048;

    // we have three types of semaphore to support in order to be able to:
    // 1: backward compatible.
    // 2: work on both upstream and pro kernel
    SemaphoreType m_semType;
    FenceType     m_fenceType;

    // state flags for real sync object support status.
    // double check syncobj's implementation: with paritial or full features in libdrm.so and drm.ko.
    union
    {
        struct
        {
            uint32 SyncobjSemaphore                : 1;
            uint32 InitialSignaledSyncobjSemaphore : 1;
            uint32 SyncobjFence                    : 1;
            uint32 reserved                        : 29;
        };
        uint32 flags;
    } m_syncobjSupportState;

    // Support creating submission queue with priority.
    bool m_supportQueuePriority;
    // Support creating bo that is always resident in current VM.
    bool m_supportVmAlwaysValid;
#if defined(PAL_DEBUG_PRINTS)
    const DrmLoaderFuncsProxy& m_drmProcs;
#else
    const DrmLoaderFuncs& m_drmProcs;
#endif

    PAL_DISALLOW_DEFAULT_CTOR(Device);
    PAL_DISALLOW_COPY_AND_ASSIGN(Device);
};
} // Linux
} // Pal
