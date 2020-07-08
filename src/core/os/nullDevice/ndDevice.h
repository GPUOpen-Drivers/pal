/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_BUILD_NULL_DEVICE

#include "core/device.h"
#include "core/os/nullDevice/ndDmaUploadRing.h"
#include "palPlatform.h"

namespace Pal
{
namespace NullDevice
{

// Lookup table of GPU names by NullGpuId
extern const char* pNullGpuNames[static_cast<uint32>(NullGpuId::Max)];

class Platform;

// Structure to convert between Null GPU ID's and other GPU asic identification information
struct  NullIdLookup
{
    uint32  familyId;
    uint32  eRevId;
    uint32  revisionId;
    uint32  gfxEngineId;
    uint32  deviceId;
};

// =====================================================================================================================
// Null flavor of the Device class.
class Device : public Pal::Device
{
public:
    static Result Create(Platform*  pPlatform,
                         Device**   ppDeviceOut,
                         NullGpuId  nullGpuId);

    static bool IsValid(NullGpuId nullGpuId);

    virtual Result AddEmulatedPrivateScreen(
        const PrivateScreenCreateInfo& createInfo,
        uint32*                        pTargetId) override;

    virtual Result ReserveGpuVirtualAddress(VaPartition             vaPartition,
                                            gpusize                 baseVirtAddr,
                                            gpusize                 size,
                                            bool                    isVirtual,
                                            VirtualGpuMemAccessMode virtualAccessMode,
                                            gpusize*                pGpuVirtAddr) override
        { return Result::ErrorUnavailable;}
    virtual Result FreeGpuVirtualAddress(gpusize vaStartAddress, gpusize vaSize) override
        { return Result::ErrorUnavailable;}

    Result AssignVirtualAddress(const Pal::GpuMemory& gpuMemory, gpusize* pGpuVirtAddr, VaPartition vaPartition);

    // NOTE: Part of the public IDevice interface.
    virtual Result GetCalibratedTimestamps(
        CalibratedTimestamps* pCalibratedTimestamps) const override;

    virtual Result Cleanup() override;

    virtual Result CreateImage(
        const ImageCreateInfo& createInfo,
        void*                  pPlacementAddr,
        IImage**               ppImage) override;

    virtual Result CreateInternalImage(
        const ImageCreateInfo&         createInfo,
        const ImageInternalCreateInfo& internalCreateInfo,
        void*                          pPlacementAddr,
        Pal::Image**                   ppImage) override;

    virtual Result CreatePresentableImage(
        const PresentableImageCreateInfo& createInfo,
        void*                             pImagePlacementAddr,
        void*                             pGpuMemoryPlacementAddr,
        IImage**                          ppImage,
        IGpuMemory**                      ppGpuMemory) override;

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

    // NOTE: Part of the public IDevice interface.
    virtual Result WaitForSemaphores(
        uint32                       semaphoreCount,
        const IQueueSemaphore*const* ppSemaphores,
        const uint64*                pValues,
        uint32                       flags,
        uint64                       timeout) const override;

    virtual Result CreateSwapChain(
        const SwapChainCreateInfo& createInfo,
        void*                      pPlacementAddr,
        ISwapChain**               ppSwapChain) override;

    virtual Result DeterminePresentationSupported(
        OsDisplayHandle      hDisplay,
        WsiPlatform          wsiPlatform,
        int64                visualId) override { return Result::Unsupported; } // can't present anything

    virtual uint32 GetSupportedSwapChainModes(
        WsiPlatform wsiPlatform,
        PresentMode mode) const override { return 0; }

    virtual Result DetermineExternalSharedResourceType(
        const ExternalResourceOpenInfo& openInfo,
        bool*                           pIsImage) const override;

    virtual Result EnumPrivateScreensInfo(uint32* pNumScreens) override;

    virtual Result Finalize(const DeviceFinalizeInfo& finalizeInfo) override;

    void FreeVirtualAddress(const Pal::GpuMemory& gpuMemory) { }

    virtual Result GetExternalSharedImageSizes(
        const ExternalImageOpenInfo& openInfo,
        size_t*                      pImageSize,
        size_t*                      pGpuMemorySize,
        ImageCreateInfo*             pImgCreateInfo) const override;

    virtual Result GetFlipStatus(
        uint32           vidPnSrcId,
        FlipStatusFlags* pFlipFlags,
        bool*            pIsFlipOwner) const override { return Result::Success; }

    // NOTE: Part of the public IDevice interface.
    virtual size_t GetImageSize(
        const ImageCreateInfo& createInfo,
        Result*                pResult) const override;

    virtual Result GetMultiGpuCompatibility(
        const IDevice&        otherDevice,
        GpuCompatibilityInfo* pInfo) const override;

    virtual void GetPresentableImageSizes(
        const PresentableImageCreateInfo& createInfo,
        size_t*                           pImageSize,
        size_t*                           pGpuMemorySize,
        Result*                           pResult) const override;

    virtual Result GetPrimaryInfo(
        const GetPrimaryInfoInput&  primaryInfoInput,
        GetPrimaryInfoOutput*       pPrimaryInfoOutput) const override;

    virtual Result GetStereoDisplayModes(
        uint32*                   pStereoModeCount,
        StereoDisplayModeOutput*  pStereoModeList) const override;

    virtual Result GetActive10BitPackedPixelMode(
        Active10BitPackedPixelModeOutput* pMode) const override;

    virtual Result RequestKmdReinterpretAs10Bit(
        const IGpuMemory* pGpuMemory) const override;

    virtual size_t GetSwapChainSize(
        const SwapChainCreateInfo& createInfo,
        Result*                    pResult) const override;

    virtual Result GetXdmaInfo(
        uint32              vidPnSrcId,
        const IGpuMemory&   gpuMemory,
        GetXdmaInfoOutput*  pGetXdmaInfoOutput) const override;

    virtual size_t GpuMemoryObjectSize() const override;

    virtual bool IsMasterGpu() const override { return true; } // only one gpu, it's going to be the master

    virtual bool IsNull() const override { return true; }

    virtual Result OpenExternalSharedGpuMemory(
        const ExternalGpuMemoryOpenInfo& openInfo,
        void*                            pPlacementAddr,
        GpuMemoryCreateInfo*             pMemCreateInfo,
        IGpuMemory**                     ppGpuMemory) override;

    virtual Result OpenExternalSharedImage(
        const ExternalImageOpenInfo& openInfo,
        void*                        pImagePlacementAddr,
        void*                        pGpuMemoryPlacementAddr,
        GpuMemoryCreateInfo*             pMemCreateInfo,
        IImage**                         ppImage,
        IGpuMemory**                     ppGpuMemory) override;

    virtual Result PollFullScreenFrameMetadataControl(
        uint32                         vidPnSrcId,
        PerSourceFrameMetadataControl* pFrameMetadataControl) const override;

    virtual Result QueryDisplayConnectors(
        uint32*                     pConnectorCount,
        DisplayConnectorProperties* pConnectors) override;

    virtual Result QueryWorkStationCaps(
        WorkStationCaps* pCaps) const override;

    virtual Result RemoveEmulatedPrivateScreen(
        uint32 targetId) override;

    virtual Result SetClockMode(
        const SetClockModeInput& setClockModeInput,
        SetClockModeOutput*      pSetClockModeOutput) override { return Result::Success; }

    virtual Result SetMaxQueuedFrames(
        uint32 maxFrames) override;

    virtual Result SetMgpuMode(
        const SetMgpuModeInput& setMgpuModeInput) const override;

    virtual Result SetPowerProfile(
        PowerProfile         profile,
        CustomPowerProfile*  pInfo) override;

    virtual Result FlglQueryState(
        Pal::FlglState* pState) override { return Result::Success; }

    virtual Result FlglSetFrameLock(
        bool enable) override { return Result::Success; }

    virtual Result FlglResetFrameCounter() const override { return Result::Success; }

    virtual Result FlglGetFrameCounter(
        uint64* pValue) const  override { return Result::Success; }

    virtual Result FlglGetFrameCounterResetStatus(
        bool* pReset) const override { return Result::Success; }

    virtual Result DidRsFeatureSettingsChange(
        uint32  rsFeatures,
        uint32* pRsFeaturesChanged) override
    {
       if (pRsFeaturesChanged != nullptr)
       {
           *pRsFeaturesChanged = 0;
       }
       return Result::Success;
    }

    virtual Result GetRsFeatureGlobalSettings(
        RsFeatureType  rsFeature,
        RsFeatureInfo* pRsFeatureInfo) override
    {
       if (pRsFeatureInfo != nullptr)
       {
           *pRsFeatureInfo = { };
       }
       return Result::Success;
    }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 537

    virtual bool DidDelagSettingsChange()     override { return false; }
    virtual bool DidTurboSyncSettingsChange() override { return false; }

    virtual Result DidChillSettingsChange(
        bool* pChangeDetected) override
    {
        if (pChangeDetected != nullptr)
        {
            *pChangeDetected = false;
        }
        return Result::Success;
    }

    virtual Result GetChillGlobalEnable(
        bool* pGlobalEnable) override
    {
        if (pGlobalEnable != nullptr)
        {
            *pGlobalEnable = false;
        }
        return Result::Success;
    }
#endif

    virtual Result UpdateChillStatus(
        uint64 lastChillActiveTimeStampUs) override { return Result::Success; }

    virtual Result InitBusAddressableGpuMemory(
        IQueue*           pQueue,
        uint32            gpuMemCount,
        IGpuMemory*const* ppGpuMemList) override { return Result::Success; };

    virtual Result CreateVirtualDisplay(
        const VirtualDisplayInfo& virtualDisplayInfo,
        uint32*                   pScreenTargetId) override { return Result::Success; }

    virtual Result DestroyVirtualDisplay(
        uint32     screenTargetId) override { return Result::Success; }

    virtual Result GetVirtualDisplayProperties(
        uint32                    screenTargetId,
        VirtualDisplayProperties* pProperties) override { return Result::Success; }

    virtual Result QueryRadeonSoftwareVersion(
        char*  pBuffer,
        size_t bufferLength) const override { return Result::Unsupported; }

    virtual Result CreateDmaUploadRing() override { return Result::Success; };

protected:
    Device(
        Platform*              pPlatform,
        const char*            pName,
        const NullIdLookup&    nullIdLookup,
        const HwIpDeviceSizes& hwDeviceSizes);

    virtual Pal::GpuMemory* ConstructGpuMemoryObject(
        void* pPlacementAddr) override;

    virtual Pal::Queue* ConstructQueueObject(
        const QueueCreateInfo& createInfo,
        void*                  pPlacementAddr) override;

    virtual void FinalizeQueueProperties() override;

    virtual size_t QueueObjectSize(
        const QueueCreateInfo& createInfo) const override;

    virtual bool ValidatePipelineUploadHeap(const GpuHeap& preferredHeap) const override { return false; }

    virtual Pal::Queue* ConstructMultiQueueObject(
        uint32                 queueCount,
        const QueueCreateInfo* pCreateInfo,
        void*                  pPlacementAddr) override
    {
        return nullptr;
    }

    virtual size_t MultiQueueObjectSize(
        uint32                 queueCount,
        const QueueCreateInfo* pCreateInfo) const override
    {
        return 0;
    }

private:
    virtual Result EarlyInit(const HwIpLevels& ipLevels) override;

    virtual Result GetSwapChainInfo(
        OsDisplayHandle      hDisplay,
        OsWindowHandle       hWindow,
        Pal::WsiPlatform     wsiPlatform,
        SwapChainProperties* pSwapChainProperties) override;

    Result InitMemoryProperties();

    void InitExternalPhysicalHeap();

    void InitOutputPaths();

    virtual Result OsEarlyInit() override;

    virtual Result OsLateInit() override;

    virtual Result QueryApplicationProfile(
        const char*         pFilename,
        const char*         pPathname,
        ApplicationProfile* pOut) const override;

    virtual Result QueryRawApplicationProfile(
        const char*              pFilename,
        const char*              pPathname,
        ApplicationProfileClient client,
        const char**             pOut) override;

    virtual Result EnableSppProfile(
        const char*              pFilename,
        const char*              pPathname) override;

    virtual Result SelectSppTable(
        uint32 pixelCount,
        uint32 msaaRate) const override { return Result::Unsupported; }

    virtual bool ReadSetting(
        const char*          pSettingName,
        Util::ValueType      valueType,
        void*                pValue,
        InternalSettingScope settingType,
        size_t               bufferSz = 0) const override;

#if PAL_BUILD_GFX6
    void InitGfx6ChipProperties();
#endif

    void InitGfx9ChipProperties();

    const NullIdLookup&  m_nullIdLookup;

    PAL_DISALLOW_DEFAULT_CTOR(Device);
    PAL_DISALLOW_COPY_AND_ASSIGN(Device);
};

} // NullDevice
} // Pal

#endif
