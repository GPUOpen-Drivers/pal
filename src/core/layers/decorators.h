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

#include "palBorderColorPalette.h"
#include "palCmdAllocator.h"
#include "palCmdBuffer.h"
#include "palColorBlendState.h"
#include "palColorTargetView.h"
#include "palDepthStencilState.h"
#include "palDepthStencilView.h"
#include "palDevice.h"
#include "palFence.h"
#include "palGpuEvent.h"
#include "palGpuMemory.h"
#include "palHashMap.h"
#include "palImage.h"
#include "palIndirectCmdGenerator.h"
#include "palMsaaState.h"
#include "palPerfExperiment.h"
#include "palPipeline.h"
#include "palPipelineAbi.h"
#include "palPlatform.h"
#include "palPrivateScreen.h"
#include "palQueryPool.h"
#include "palQueue.h"
#include "palQueueSemaphore.h"
#include "palScreen.h"
#include "palSwapChain.h"
#include "palSysMemory.h"

// =====================================================================================================================
// The following classes are all implementations of the Decorator pattern for the public PAL interface.
// PAL uses this decorator to wrap all classes created by an IDevice (and the IDevice itself) in order to provide added
// functionality in different layers.
//
// The current layers are:
//  + DbgOverlay
//      The DbgOverlay provides a visual confirmation that a particular driver is rendering, along with useful debug
//      data such as current memory heap allocations sizes, which GPU is presenting, and the version of the driver
//      rendering.
//  + CmdBufferLogger
//      The CmdBufferLogger provides command buffer dump annotations for offline analysis. This layer is only
//      available in developer builds.
//  + GpuProfiler
//      The GpuProfiler provides timings, performance experiments, and thread traces on a per-command buffer basis.
//
// For future layers, the only class that needs its own overriden implementation is DeviceDecorator. Not all functions
// in DeviceDecorator need to be overridden just the ones that the layer needs to use.
//
// Part of the layer implementation requires that any layer have similar sized "stacks," meaning that an IDevice and a
// IGpuMemory contain the same number of layers, in the same order. The default implementation of DeviceDecorator has
// all of the necessary functionality to create default implementations of every PAL object.
// =====================================================================================================================

namespace Pal
{

// Forward decl's
class BorderColorPaletteDecorator;
class CmdBufferDecorator;
class CmdBufferFwdDecorator;
class ColorBlendStateDecorator;
class ColorTargetViewDecorator;
class DepthStencilStateDecorator;
class DepthStencilViewDecorator;
class DeviceDecorator;
class FenceDecorator;
class GpuEventDecorator;
class GpuMemoryDecorator;
class ImageDecorator;
class IndirectCmdGeneratorDecorator;
class MsaaStateDecorator;
class PerfExperimentDecorator;
class PipelineDecorator;
class PlatformDecorator;
class PrivateScreenDecorator;
class QueryPoolDecorator;
class QueueDecorator;
class QueueSemaphoreDecorator;
class ScreenDecorator;
class ScissorStateDecorator;
class ViewportStateDecorator;

extern IBorderColorPalette*   NextBorderColorPalette(const IBorderColorPalette* pBorderColorPalette);
extern ICmdAllocator*         NextCmdAllocator(const ICmdAllocator* pCmdAllocator);
extern ICmdBuffer*            NextCmdBuffer(const ICmdBuffer* pCmdBuffer);
extern CmdBufferBuildInfo     NextCmdBufferBuildInfo(const CmdBufferBuildInfo& info);
extern IColorBlendState*      NextColorBlendState(const IColorBlendState* pColorBlendState);
extern IColorTargetView*      NextColorTargetView(const IColorTargetView* pView);
extern IDepthStencilState*    NextDepthStencilState(const IDepthStencilState* pDepthStencilState);
extern IDepthStencilView*     NextDepthStencilView(const IDepthStencilView* pDepthStencilView);
extern IDevice*               NextDevice(const IDevice* pDevice);
extern IFence*                NextFence(const IFence* pFence);
extern IGpuEvent*             NextGpuEvent(const IGpuEvent* pGpuEvent);
extern IGpuMemory*            NextGpuMemory(const IGpuMemory* pGpuMemory);
extern IImage*                NextImage(const IImage* pImage);
extern IIndirectCmdGenerator* NextIndirectCmdGenerator(const IIndirectCmdGenerator* pGenerator);
extern IMsaaState*            NextMsaaState(const IMsaaState* pMsaaState);
extern IPerfExperiment*       NextPerfExperiment(const IPerfExperiment* pPerfExperiment);
extern IPipeline*             NextPipeline(const IPipeline* pPipeline);
extern PipelineBindParams     NextPipelineBindParams(const PipelineBindParams& params);
extern IPlatform*             NextPlatform(const IPlatform* pPlatform);
extern IPrivateScreen*        NextPrivateScreen(const IPrivateScreen* pScreen);
extern IQueryPool*            NextQueryPool(const IQueryPool* pQueryPool);
extern IQueue*                NextQueue(const IQueue* pQueue);
extern IQueueSemaphore*       NextQueueSemaphore(const IQueueSemaphore* pQueueSemaphore);
extern IScreen*               NextScreen(const IScreen* pScreen);
extern IScissorState*         NextScissorState(const IScissorState* pScissorState);
extern ISwapChain*            NextSwapChain(const ISwapChain* pSwapChain);
extern IViewportState*        NextViewportState(const IViewportState* pViewportState);

// =====================================================================================================================
// Templated function for getting the previous layer that wraps the one passed in.
template<typename IObj>
IObj* PreviousObject(
    IObj* pObj)
{
    IObj* pPreviousObj = nullptr;

    // If the current object is null, then so is the previous object.
    if (pObj != nullptr)
    {
        pPreviousObj = reinterpret_cast<IObj*>(pObj->GetClientData());
    }

    return pPreviousObj;
}

// =====================================================================================================================
// Templated function for computing the placement address for the decorator's next object.
template<typename IObj>
void* NextObjectAddr(
    void* pObjectAddr)
{
    return Util::VoidPtrInc(pObjectAddr, sizeof(IObj));
}

// =====================================================================================================================
// Returns true if the PreviousObject was non-null, and thus the pData->pCmdBuffer data is valid for this layer.
static bool TranslateBarrierEventData(
    void* pCbData)
{
    Developer::BarrierData* pData       = static_cast<Developer::BarrierData*>(pCbData);
    pData->transition.imageInfo.pImage  = PreviousObject(pData->transition.imageInfo.pImage);

    ICmdBuffer* pPrevCmdBuffer          = PreviousObject(pData->pCmdBuffer);
    const bool  hasValidData            = (pPrevCmdBuffer != nullptr);
    pData->pCmdBuffer                   = (hasValidData) ? pPrevCmdBuffer : pData->pCmdBuffer;

    return hasValidData;
}

// =====================================================================================================================
// Returns true if the PreviousObject was non-null, and thus the pData->pCmdBuffer data is valid for this layer.
static bool TranslateDrawDispatchData(
    void* pCbData)
{
    Developer::DrawDispatchData* pData = static_cast<Developer::DrawDispatchData*>(pCbData);

    ICmdBuffer* pPrevCmdBuffer         = PreviousObject(pData->pCmdBuffer);
    const bool  hasValidData           = (pPrevCmdBuffer != nullptr);
    pData->pCmdBuffer                  = (hasValidData) ? pPrevCmdBuffer : pData->pCmdBuffer;

    return hasValidData;
}

// =====================================================================================================================
class PlatformDecorator : public IPlatform
{
public:
    PlatformDecorator(
        const Util::AllocCallbacks&  allocCb,
        Developer::Callback          developerCb,
        bool                         installCallback,
        bool                         isLayerEnabled,
        IPlatform*                   pNextPlatform);

    virtual Result EnumerateDevices(
        uint32*    pDeviceCount,
        IDevice*   pDevices[MaxDevices]) override;

    virtual size_t GetScreenObjectSize() const override;

    virtual Result GetScreens(
        uint32*  pScreenCount,
        void*    pStorage[MaxScreens],
        IScreen* pScreens[MaxScreens]) override;

    virtual Result QueryApplicationProfile(
        const char*         pFilename,
        const char*         pPathname,
        ApplicationProfile* pOut) override
        { return m_pNextLayer->QueryApplicationProfile(pFilename, pPathname, pOut); }

    virtual Result QueryRawApplicationProfile(
        const char*              pFilename,
        const char*              pPathname,
        ApplicationProfileClient client,
        const char**             pOut) override
        { return m_pNextLayer->QueryRawApplicationProfile(pFilename, pPathname, client, pOut); }

    virtual Result GetProperties(
        PlatformProperties* pProperties) override
        { return m_pNextLayer->GetProperties(pProperties); }

    // Part of the IDestroyable public interface.
    virtual void Destroy() override
    {
        IPlatform* pNextLayer = m_pNextLayer;
        this->~PlatformDecorator();
        pNextLayer->Destroy();
    }

    // Empty developer callback for null operation.
    static void PAL_STDCALL DefaultDeveloperCb(
        void*                   pPrivateData,
        const uint32            deviceIndex,
        Developer::CallbackType type,
        void*                   pCbData);

    // Executes the developer callback.
    void DeveloperCb(
        uint32                  deviceIndex,
        Developer::CallbackType type,
        void*                   pCbData)
    {
        if (m_pfnDeveloperCb != nullptr)
        {
            m_pfnDeveloperCb(m_pClientPrivateData, deviceIndex, type, pCbData);
        }
    }

    virtual Result GetPrimaryLayout(
        uint32                  vidPnSourceId,
        GetPrimaryLayoutOutput* pPrimaryLayoutOutput) override
        { return m_pNextLayer->GetPrimaryLayout(vidPnSourceId, pPrimaryLayoutOutput); }

    virtual Result TurboSyncControl(
        const TurboSyncControlInput& turboSyncControlInput) override;

#if PAL_BUILD_GPUOPEN
    virtual DevDriver::DevDriverServer* GetDevDriverServer() override
    {
        return m_pNextLayer->GetDevDriverServer();
    }
#endif

    virtual void LogMessage(
        LogLevel        level,
        LogCategoryMask categoryMask,
        const char*     pFormat,
        va_list         args) override
    { m_pNextLayer->LogMessage(level, categoryMask, pFormat, args); }

    IPlatform* GetNextLayer() const { return m_pNextLayer; }

protected:
    virtual ~PlatformDecorator();

    DeviceDecorator* PreviousDevice(uint32 deviceIndex) { return m_pDevices[deviceIndex]; }

    void TearDownGpus();

    virtual void InstallDeveloperCb(
        Developer::Callback pfnDeveloperCb,
        void*               pPrivateData) override;

    IPlatform*const        m_pNextLayer;
    DeviceDecorator*       m_pDevices[MaxDevices];
    uint32                 m_deviceCount;
    Developer::Callback    m_pfnDeveloperCb;
    void*                  m_pClientPrivateData;
    bool                   m_installDeveloperCb;
    const bool             m_layerEnabled;

private:
    PAL_DISALLOW_DEFAULT_CTOR(PlatformDecorator);
    PAL_DISALLOW_COPY_AND_ASSIGN(PlatformDecorator);
};

// =====================================================================================================================
class ScreenDecorator : public IScreen
{
public:
    ScreenDecorator(IScreen* pNextScreen, DeviceDecorator** ppDevices, uint32 deviceCount)
        :
        m_pNextLayer(pNextScreen),
        m_ppDevices(ppDevices),
        m_deviceCount(deviceCount)
    {}

    virtual void Destroy() override
    {
        IScreen* pNextLayer = m_pNextLayer;
        this->~ScreenDecorator();
        pNextLayer->Destroy();
    }

    virtual ~ScreenDecorator() {}

    virtual Result GetProperties(
        ScreenProperties* pInfo) const override;

    virtual Result GetScreenModeList(
        uint32*     pScreenModeCount,
        ScreenMode* pScreenModeList) const override
        { return m_pNextLayer->GetScreenModeList(pScreenModeCount, pScreenModeList); }

    virtual Result RegisterWindow(
        OsWindowHandle hWindow) override
        { return m_pNextLayer->RegisterWindow(hWindow); }

    virtual Result TakeFullscreenOwnership(
        const IImage& image) override
        { return m_pNextLayer->TakeFullscreenOwnership(*NextImage(&image)); }

    virtual Result ReleaseFullscreenOwnership() override
        { return m_pNextLayer->ReleaseFullscreenOwnership(); }

    virtual Result SetGammaRamp(
        const GammaRamp& gammaRamp) override
        { return m_pNextLayer->SetGammaRamp(gammaRamp); }

    virtual Result GetFormats(
        uint32*           pFormatCount,
        SwizzledFormat*   pFormatList) override
    { return m_pNextLayer->GetFormats(pFormatCount, pFormatList); }

    virtual Result GetColorCapabilities(
        ScreenColorCapabilities* pCapabilities) override
    { return m_pNextLayer->GetColorCapabilities(pCapabilities); }

    virtual Result SetColorConfiguration(
        const ScreenColorConfig* pColorConfig) override
    { return m_pNextLayer->SetColorConfiguration(pColorConfig); }

    virtual Result WaitForVerticalBlank() const override
        { return m_pNextLayer->WaitForVerticalBlank(); }

    virtual Result GetScanLine(
        int32* pScanLine) const override
        { return m_pNextLayer->GetScanLine(pScanLine); }

    IScreen* GetNextLayer() const { return m_pNextLayer; }

protected:
    const IDevice* GetDeviceFromNextLayer(const IDevice* pDevice) const;

    IScreen*const     m_pNextLayer;
    DeviceDecorator** m_ppDevices;
    uint32            m_deviceCount;

private:
    PAL_DISALLOW_DEFAULT_CTOR(ScreenDecorator);
    PAL_DISALLOW_COPY_AND_ASSIGN(ScreenDecorator);
};

// =====================================================================================================================
class DeviceDecorator : public IDevice
{
public:
    DeviceDecorator(PlatformDecorator* pPlatform, IDevice* pNextDevice);
    virtual ~DeviceDecorator() {}

    virtual Result GetLinearImageAlignments(
        LinearImageAlignments* pAlignments) const override
        { return m_pNextLayer->GetLinearImageAlignments(pAlignments); }

    virtual Result GetProperties(
        DeviceProperties* pInfo) const override
        { return m_pNextLayer->GetProperties(pInfo); }

    virtual Result CheckExecutionState() const override
        { return m_pNextLayer->CheckExecutionState(); }

    virtual PalPublicSettings* GetPublicSettings() override
        { return m_pNextLayer->GetPublicSettings(); }

    virtual const CmdBufferLoggerSettings& GetCmdBufferLoggerSettings() const override
        { return m_pNextLayer->GetCmdBufferLoggerSettings(); }

    virtual const DebugOverlaySettings& GetDbgOverlaySettings() const override
        { return m_pNextLayer->GetDbgOverlaySettings(); }

    virtual const GpuProfilerSettings& GetGpuProfilerSettings() const override
        { return m_pNextLayer->GetGpuProfilerSettings(); }

    virtual const InterfaceLoggerSettings& GetInterfaceLoggerSettings() const override
        { return m_pNextLayer->GetInterfaceLoggerSettings(); }

    virtual Result CommitSettingsAndInit() override
        { return m_pNextLayer->CommitSettingsAndInit(); }

    virtual Result AllocateGds(
        const DeviceGdsAllocInfo&   requested,
        DeviceGdsAllocInfo*         pAllocated) override
        { return m_pNextLayer->AllocateGds(requested, pAllocated); }

    virtual Result Finalize(const DeviceFinalizeInfo& finalizeInfo) override;

    virtual Result Cleanup() override
        { return m_pNextLayer->Cleanup(); }

    virtual bool ReadSetting(
        const char*     pSettingName,
        SettingScope    settingScope,
        Util::ValueType valueType,
        void*           pValue,
        size_t          bufferSz = 0) const override
    {
        return m_pNextLayer->ReadSetting(pSettingName, settingScope, valueType, pValue, bufferSz);
    }

    virtual uint32 GetMaxAtomicCounters(
        EngineType engineType,
        uint32     maxNumEngines) const override
        { return m_pNextLayer->GetMaxAtomicCounters(engineType, maxNumEngines); }

    virtual gpusize GetMaxGpuMemoryAlignment() const override
        { return m_pNextLayer->GetMaxGpuMemoryAlignment(); }

    virtual Result GetMultiGpuCompatibility(
        const IDevice&        otherDevice,
        GpuCompatibilityInfo* pInfo) const override
        { return m_pNextLayer->GetMultiGpuCompatibility(*NextDevice(&otherDevice), pInfo); }

    virtual Result GetGpuMemoryHeapProperties(
        GpuMemoryHeapProperties info[GpuHeapCount]) const override
        { return m_pNextLayer->GetGpuMemoryHeapProperties(info); }

    virtual Result GetFormatProperties(
        MergedFormatPropertiesTable* pInfo) const override
        { return m_pNextLayer->GetFormatProperties(pInfo); }

    virtual Result GetPerfExperimentProperties(
        PerfExperimentProperties* pProperties) const override
        { return m_pNextLayer->GetPerfExperimentProperties(pProperties); }

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

    virtual Result SetMaxQueuedFrames(
        uint32 maxFrames) override
        { return m_pNextLayer->SetMaxQueuedFrames(maxFrames); }

    virtual Result ResetFences(
        uint32              fenceCount,
        IFence*const*       ppFences) const override;

    virtual Result WaitForFences(
        uint32              fenceCount,
        const IFence*const* ppFences,
        bool                waitAll,
        uint64              timeout
        ) const override;

    virtual Result CalibrateGpuTimestamp(
        GpuTimestampCalibration* pCalibrationData) const override
        { return m_pNextLayer->CalibrateGpuTimestamp(pCalibrationData); }

    virtual void BindTrapHandler(
        PipelineBindPoint pipelineType,
        IGpuMemory*       pGpuMemory,
        gpusize           offset) override
        { m_pNextLayer->BindTrapHandler(pipelineType, NextGpuMemory(pGpuMemory), offset); }

    virtual void BindTrapBuffer(
        PipelineBindPoint pipelineType,
        IGpuMemory*       pGpuMemory,
        gpusize           offset) override
        { m_pNextLayer->BindTrapBuffer(pipelineType, NextGpuMemory(pGpuMemory), offset); }

    virtual Result GetSwapChainInfo(
        OsDisplayHandle      hDisplay,
        OsWindowHandle       hWindow,
        WsiPlatform          wsiPlatform,
        SwapChainProperties* pSwapChainProperties) override;

    virtual Result DeterminePresentationSupported(
        OsDisplayHandle      hDisplay,
        WsiPlatform          wsiPlatform,
        int64                visualId) override
        { return m_pNextLayer->DeterminePresentationSupported(hDisplay, wsiPlatform, visualId); }

    virtual Result DetermineExternalSharedResourceType(
        const ExternalResourceOpenInfo& openInfo,
        bool*                           pIsImage) const override
        { return m_pNextLayer->DetermineExternalSharedResourceType(openInfo, pIsImage); }

    virtual size_t GetQueueSize(
        const QueueCreateInfo& createInfo,
        Result*                pResult) const override;

    virtual Result CreateQueue(
        const QueueCreateInfo& createInfo,
        void*                  pPlacementAddr,
        IQueue**               ppQueue) override;

    virtual size_t GetGpuMemorySize(
        const GpuMemoryCreateInfo& createInfo,
        Result*                    pResult) const override;

    virtual Result CreateGpuMemory(
        const GpuMemoryCreateInfo& createInfo,
        void*                      pPlacementAddr,
        IGpuMemory**               ppGpuMemory) override;

    virtual size_t GetPinnedGpuMemorySize(
        const PinnedGpuMemoryCreateInfo& createInfo,
        Result*                          pResult) const override;

    virtual Result CreatePinnedGpuMemory(
        const PinnedGpuMemoryCreateInfo& createInfo,
        void*                            pPlacementAddr,
        IGpuMemory**                     ppGpuMemory) override;

    virtual size_t GetSvmGpuMemorySize(
        const SvmGpuMemoryCreateInfo& createInfo,
        Result*                       pResult) const override;

    virtual Result CreateSvmGpuMemory(
        const SvmGpuMemoryCreateInfo& createInfo,
        void*                         pPlacementAddr,
        IGpuMemory**                  ppGpuMemory) override;

    virtual size_t GetSharedGpuMemorySize(
        const GpuMemoryOpenInfo& openInfo,
        Result*                  pResult) const override;

    virtual Result OpenSharedGpuMemory(
        const GpuMemoryOpenInfo& openInfo,
        void*                    pPlacementAddr,
        IGpuMemory**             ppGpuMemory) override;

    virtual size_t GetExternalSharedGpuMemorySize(
        Result* pResult) const override;

    virtual size_t GetPeerGpuMemorySize(
        const PeerGpuMemoryOpenInfo& openInfo,
        Result*                      pResult) const override;

    virtual Result OpenPeerGpuMemory(
        const PeerGpuMemoryOpenInfo& openInfo,
        void*                        pPlacementAddr,
        IGpuMemory**                 ppGpuMemory) override;

    virtual Result OpenExternalSharedGpuMemory(
        const ExternalGpuMemoryOpenInfo& openInfo,
        void*                            pPlacementAddr,
        GpuMemoryCreateInfo*             pMemCreateInfo,
        IGpuMemory**                     ppGpuMemory) override;

    virtual size_t GetImageSize(
        const ImageCreateInfo& createInfo,
        Result*                pResult) const override;

    virtual Result CreateImage(
        const ImageCreateInfo& createInfo,
        void*                  pPlacementAddr,
        IImage**               ppImage) override;

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

    virtual void GetPeerImageSizes(
        const PeerImageOpenInfo& openInfo,
        size_t*                  pPeerImageSize,
        size_t*                  pPeerGpuMemorySize,
        Result*                  pResult) const override;

    virtual Result OpenPeerImage(
        const PeerImageOpenInfo& openInfo,
        void*                    pImagePlacementAddr,
        void*                    pGpuMemoryPlacementAddr,
        IImage**                 ppImage,
        IGpuMemory**             ppGpuMemory) override;

    virtual size_t GetColorTargetViewSize(
        Result* pResult) const override;

    virtual Result CreateColorTargetView(
        const ColorTargetViewCreateInfo& createInfo,
        void*                            pPlacementAddr,
        IColorTargetView**               ppColorTargetView) const override;

    virtual size_t GetDepthStencilViewSize(
        Result* pResult) const override;

    virtual Result CreateDepthStencilView(
        const DepthStencilViewCreateInfo& createInfo,
        void*                             pPlacementAddr,
        IDepthStencilView**               ppDepthStencilView) const override;

    static void PAL_STDCALL DecoratorCreateTypedBufViewSrds(
        const IDevice*        pDevice,
        uint32                count,
        const BufferViewInfo* pBufferViewInfo,
        void*                 pOut);

    static void PAL_STDCALL DecoratorCreateUntypedBufViewSrds(
        const IDevice*        pDevice,
        uint32                count,
        const BufferViewInfo* pBufferViewInfo,
        void*                 pOut);

    Result ValidateSamplerInfo(const SamplerInfo& info) const override;

    static void PAL_STDCALL DecoratorCreateSamplerSrds(
        const IDevice*     pDevice,
        uint32             count,
        const SamplerInfo* pSamplerInfo,
        void*              pOut);

    virtual Result ValidateImageViewInfo(
        const ImageViewInfo& viewInfo) const override;

    static void PAL_STDCALL DecoratorCreateImageViewSrds(
        const IDevice*       pDevice,
        uint32               count,
        const ImageViewInfo* pImgViewInfo,
        void*                pOut);

    virtual Result ValidateFmaskViewInfo(
        const FmaskViewInfo& viewInfo) const override;

    static void PAL_STDCALL DecoratorCreateFmaskViewSrds(
        const IDevice*       pDevice,
        uint32               count,
        const FmaskViewInfo* pFmaskViewInfo,
        void*                pOut);

    virtual Result SetSamplePatternPalette(
        const SamplePatternPalette& palette) override
        { return m_pNextLayer->SetSamplePatternPalette(palette); }

    virtual size_t GetBorderColorPaletteSize(
        const BorderColorPaletteCreateInfo& createInfo,
        Result*                             pResult) const override;

    virtual Result CreateBorderColorPalette(
        const BorderColorPaletteCreateInfo& createInfo,
        void*                               pPlacementAddr,
        IBorderColorPalette**               ppPalette) const override;

    virtual size_t GetComputePipelineSize(
        const ComputePipelineCreateInfo& createInfo,
        Result*                          pResult) const override;

    virtual Result CreateComputePipeline(
        const ComputePipelineCreateInfo& createInfo,
        void*                            pPlacementAddr,
        IPipeline**                      ppPipeline) override;

    virtual size_t GetGraphicsPipelineSize(
        const GraphicsPipelineCreateInfo& createInfo,
        Result*                           pResult) const override;

    virtual Result CreateGraphicsPipeline(
        const GraphicsPipelineCreateInfo& createInfo,
        void*                             pPlacementAddr,
        IPipeline**                       ppPipeline) override;

    virtual size_t GetMsaaStateSize(
        const MsaaStateCreateInfo& createInfo,
        Result*                    pResult) const override;

    virtual Result CreateMsaaState(
        const MsaaStateCreateInfo& createInfo,
        void*                      pPlacementAddr,
        IMsaaState**               ppMsaaState) const override;

    virtual size_t GetColorBlendStateSize(
        const ColorBlendStateCreateInfo& createInfo,
        Result*                          pResult) const override;

    virtual Result CreateColorBlendState(
        const ColorBlendStateCreateInfo& createInfo,
        void*                            pPlacementAddr,
        IColorBlendState**               ppColorBlendState) const override;

    virtual size_t GetDepthStencilStateSize(
        const DepthStencilStateCreateInfo& createInfo,
        Result*                            pResult) const override;

    virtual Result CreateDepthStencilState(
        const DepthStencilStateCreateInfo& createInfo,
        void*                              pPlacementAddr,
        IDepthStencilState**               ppDepthStencilState) const override;
    virtual size_t GetQueueSemaphoreSize(
        const QueueSemaphoreCreateInfo& createInfo,
        Result*                         pResult) const override;

    virtual Result CreateQueueSemaphore(
        const QueueSemaphoreCreateInfo& createInfo,
        void*                           pPlacementAddr,
        IQueueSemaphore**               ppQueueSemaphore) override;

    virtual size_t GetSharedQueueSemaphoreSize(
        const QueueSemaphoreOpenInfo& openInfo,
        Result*                       pResult) const override;

    virtual Result OpenSharedQueueSemaphore(
        const QueueSemaphoreOpenInfo& openInfo,
        void*                         pPlacementAddr,
        IQueueSemaphore**             ppQueueSemaphore) override;

    virtual size_t GetExternalSharedQueueSemaphoreSize(
        const ExternalQueueSemaphoreOpenInfo& openInfo,
        Result*                               pResult) const override;

    virtual Result OpenExternalSharedQueueSemaphore(
        const ExternalQueueSemaphoreOpenInfo& openInfo,
        void*                                 pPlacementAddr,
        IQueueSemaphore**                     ppQueueSemaphore) override;

    virtual size_t GetFenceSize(
        Result* pResult) const override;

    virtual Result CreateFence(
        const FenceCreateInfo& createInfo,
        void*                  pPlacementAddr,
        IFence**               ppFence) const override;

    virtual Result OpenFence(
        const FenceOpenInfo& openInfo,
        void*                pPlacementAddr,
        IFence**             ppFence) const override;

    virtual size_t GetGpuEventSize(
        const GpuEventCreateInfo& createInfo,
        Result*                   pResult) const override;

    virtual Result CreateGpuEvent(
        const GpuEventCreateInfo& createInfo,
        void*                     pPlacementAddr,
        IGpuEvent**               ppGpuEvent) override;

    virtual size_t GetQueryPoolSize(
        const QueryPoolCreateInfo& createInfo,
        Result*                    pResult) const override;

    virtual Result CreateQueryPool(
        const QueryPoolCreateInfo& createInfo,
        void*                      pPlacementAddr,
        IQueryPool**               ppQueryPool) const override;

    virtual size_t GetCmdAllocatorSize(
        const CmdAllocatorCreateInfo& createInfo,
        Result*                       pResult) const override;

    virtual Result CreateCmdAllocator(
        const CmdAllocatorCreateInfo& createInfo,
        void*                         pPlacementAddr,
        ICmdAllocator**               ppCmdAllocator) override;

    virtual size_t GetCmdBufferSize(
        const CmdBufferCreateInfo& createInfo,
        Result*                    pResult) const override;

    virtual Result CreateCmdBuffer(
        const CmdBufferCreateInfo& createInfo,
        void*                      pPlacementAddr,
        ICmdBuffer**               ppCmdBuffer) override;

    virtual size_t GetIndirectCmdGeneratorSize(
        const IndirectCmdGeneratorCreateInfo& createInfo,
        Result*                               pResult) const override;

    virtual Result CreateIndirectCmdGenerator(
        const IndirectCmdGeneratorCreateInfo& createInfo,
        void*                                 pPlacementAddr,
        IIndirectCmdGenerator**               ppGenerator) const override;

    virtual size_t GetPerfExperimentSize(
        const PerfExperimentCreateInfo& createInfo,
        Result*                         pResult) const override;

    virtual Result CreatePerfExperiment(
        const PerfExperimentCreateInfo& createInfo,
        void*                           pPlacementAddr,
        IPerfExperiment**               ppPerfExperiment) const override;

    virtual Result GetPrivateScreens(
        uint32*          pNumScreens,
        IPrivateScreen** ppScreens) override;

    virtual Result AddEmulatedPrivateScreen(
        const PrivateScreenCreateInfo& createInfo,
        uint32*                        pTargetId) override
        { return m_pNextLayer->AddEmulatedPrivateScreen(createInfo, pTargetId); }

    virtual Result RemoveEmulatedPrivateScreen(
        uint32 targetId) override
        { return m_pNextLayer->RemoveEmulatedPrivateScreen(targetId); }

    virtual void GetPrivateScreenImageSizes(
        const PrivateScreenImageCreateInfo& createInfo,
        size_t*                             pImageSize,
        size_t*                             pGpuMemorySize,
        Result*                             pResult) const override;

    virtual Result CreatePrivateScreenImage(
        const PrivateScreenImageCreateInfo& createInfo,
        void*                               pImagePlacementAddr,
        void*                               pGpuMemoryPlacementAddr,
        IImage**                            ppImage,
        IGpuMemory**                        ppGpuMemory) override;

    virtual size_t GetSwapChainSize(
        const SwapChainCreateInfo& createInfo,
        Result*                    pResult) const override;

    virtual Result CreateSwapChain(
        const SwapChainCreateInfo& createInfo,
        void*                      pPlacementAddr,
        ISwapChain**               ppSwapChain) override;

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

    virtual Result SetPowerProfile(
        PowerProfile        profile,
        CustomPowerProfile* pInfo) override;

    virtual Result QueryWorkStationCaps(
        WorkStationCaps* pCaps) const override { return m_pNextLayer->QueryWorkStationCaps(pCaps); }

    virtual Result QueryDisplayConnectors(
        uint32*                     pConnectorCount,
        DisplayConnectorProperties* pConnectors) override
        { return m_pNextLayer->QueryDisplayConnectors(pConnectorCount, pConnectors); }

    virtual Result GetPrimaryInfo(
        const GetPrimaryInfoInput&  primaryInfoInput,
        GetPrimaryInfoOutput*       pPrimaryInfoOutput) const override
        { return m_pNextLayer->GetPrimaryInfo(primaryInfoInput, pPrimaryInfoOutput); }

    virtual Result SetClockMode(
        const SetClockModeInput& clockModeInput,
        SetClockModeOutput*      pClockModeOutput) override
        { return m_pNextLayer->SetClockMode(clockModeInput, pClockModeOutput); }

    virtual Result GetStereoDisplayModes(
        uint32*                   pStereoModeCount,
        StereoDisplayModeOutput*  pStereoModeList) const override
        { return m_pNextLayer->GetStereoDisplayModes(pStereoModeCount, pStereoModeList); }

    virtual Result GetActive10BitPackedPixelMode(
        Active10BitPackedPixelModeOutput*  pMode) const override
        { return m_pNextLayer->GetActive10BitPackedPixelMode(pMode); }

    virtual Result RequestKmdReinterpretAs10Bit(
        const IGpuMemory* pGpuMemory) const override
        { return m_pNextLayer->RequestKmdReinterpretAs10Bit(pGpuMemory); }

    virtual Result SetMgpuMode(
        const SetMgpuModeInput& setMgpuModeInput) const override
        { return m_pNextLayer->SetMgpuMode(setMgpuModeInput); }

    virtual Result GetXdmaInfo(
        uint32              vidPnSrcId,
        const IGpuMemory&   gpuMemory,
        GetXdmaInfoOutput*  pGetXdmaInfoOutput) const override
        { return m_pNextLayer->GetXdmaInfo(vidPnSrcId, *NextGpuMemory(&gpuMemory), pGetXdmaInfoOutput); }

    virtual Result PollFullScreenFrameMetadataControl(
        uint32                         vidPnSrcId,
        PerSourceFrameMetadataControl* pFrameMetadataControl) const override
        { return m_pNextLayer->PollFullScreenFrameMetadataControl(vidPnSrcId, pFrameMetadataControl); }

    virtual uint32 GetValidFormatFeatureFlags(
        const ChNumFormat format,
        const ImageAspect aspect,
        const ImageTiling tiling) const override
        { return m_pNextLayer->GetValidFormatFeatureFlags(format, aspect, tiling); }

    virtual Result FlglQueryState(
       Pal::FlglState* pState) override
       { return m_pNextLayer->FlglQueryState(pState); }

    virtual Result FlglSetFrameLock(
       bool enable) override
       { return m_pNextLayer->FlglSetFrameLock(enable); }

    virtual Result FlglResetFrameCounter() const override
       { return m_pNextLayer->FlglResetFrameCounter(); }

    virtual Result FlglGetFrameCounter(uint64* pValue) const override
       { return m_pNextLayer->FlglGetFrameCounter(pValue); }

    virtual Result FlglGetFrameCounterResetStatus(bool* pReset) const override
       { return m_pNextLayer->FlglGetFrameCounterResetStatus(pReset); }

    virtual Result GetFlipStatus(
        uint32           vidPnSrcId,
        FlipStatusFlags* pFlipFlags,
        bool*            pIsFlipOwner) const override
        { return m_pNextLayer->GetFlipStatus(vidPnSrcId, pFlipFlags, pIsFlipOwner); }

    virtual Result DidChillSettingsChange(
        bool* pChangeDetected) override
        { return m_pNextLayer->DidChillSettingsChange(pChangeDetected); }

    virtual Result GetChillGlobalEnable(
        bool* pGlobalEnable) override
        { return m_pNextLayer->GetChillGlobalEnable(pGlobalEnable); }

    virtual Result InitBusAddressableGpuMemory(
        IQueue*           pQueue,
        uint32            gpuMemCount,
        IGpuMemory*const* ppGpuMemList) override
        { return m_pNextLayer->InitBusAddressableGpuMemory(pQueue, gpuMemCount, ppGpuMemList); }

    virtual Result CreateVirtualDisplay(
        const VirtualDisplayInfo& virtualDisplayInfo,
        uint32*                   pScreenTargetId) override
        { return m_pNextLayer->CreateVirtualDisplay(virtualDisplayInfo, pScreenTargetId); }

    virtual Result DestroyVirtualDisplay(
        uint32     screenTargetId) override
        { return m_pNextLayer->DestroyVirtualDisplay(screenTargetId); }

    virtual Result GetVirtualDisplayProperties(
        uint32                    screenTargetId,
        VirtualDisplayProperties* pProperties) override
        { return m_pNextLayer->GetVirtualDisplayProperties(screenTargetId, pProperties); }

    virtual bool DetermineHwStereoRenderingSupported(
        const GraphicPipelineViewInstancingInfo& viewInstancingInfo) const override
        { return m_pNextLayer->DetermineHwStereoRenderingSupported(viewInstancingInfo); }

    const DeviceFinalizeInfo& GetFinalizeInfo() const { return m_finalizeInfo; }
    IDevice*                  GetNextLayer() const { return m_pNextLayer; }
    PlatformDecorator*        GetPlatform()  const { return m_pPlatform; }

protected:
    DeviceFinalizeInfo      m_finalizeInfo;
    IDevice*const           m_pNextLayer;
    PlatformDecorator*      m_pPlatform;

    // Decorators for all enumerated private screens. They are created during GetPrivateScreens and destroyed when the
    // next layer's private screens are destroyed using a callback function.
    PrivateScreenDecorator* m_pPrivateScreens[MaxPrivateScreens];

private:
    virtual PrivateScreenDecorator* NewPrivateScreenDecorator(IPrivateScreen* pNextScreen, uint32 deviceIdx);
    static void PAL_STDCALL DestroyPrivateScreen(void* pOwner);

    PAL_DISALLOW_DEFAULT_CTOR(DeviceDecorator);
    PAL_DISALLOW_COPY_AND_ASSIGN(DeviceDecorator);
};

// =====================================================================================================================
class BorderColorPaletteDecorator : public IBorderColorPalette
{
public:
    BorderColorPaletteDecorator(IBorderColorPalette* pNextPalette, const DeviceDecorator* pNextDevice)
        :
        m_pNextLayer(pNextPalette), m_pDevice(pNextDevice)
    {}

    // Part of the IBorderColorPalette public interface.
    virtual Result Update(
        uint32       firstEntry,
        uint32       entryCount,
        const float* pEntries) override
        { return m_pNextLayer->Update(firstEntry, entryCount, pEntries); }

    // Part of the IGpuMemoryBindable public interface.
    virtual void GetGpuMemoryRequirements(GpuMemoryRequirements* pGpuMemReqs) const override
        { m_pNextLayer->GetGpuMemoryRequirements(pGpuMemReqs); }

    // Part of the IGpuMemoryBindable public interface.
    virtual Result BindGpuMemory(IGpuMemory* pGpuMemory, gpusize offset) override
        { return m_pNextLayer->BindGpuMemory(NextGpuMemory(pGpuMemory), offset); }

    // Part of the IDestroyable public interface.
    virtual void Destroy() override
    {
        IBorderColorPalette* pNextLayer = m_pNextLayer;
        this->~BorderColorPaletteDecorator();
        pNextLayer->Destroy();
    }

    const IDevice*       GetDevice() const { return m_pDevice; }
    IBorderColorPalette* GetNextLayer() const { return m_pNextLayer; }

protected:
    virtual ~BorderColorPaletteDecorator() {}

    IBorderColorPalette*const   m_pNextLayer;
    const DeviceDecorator*const m_pDevice;

private:
    PAL_DISALLOW_DEFAULT_CTOR(BorderColorPaletteDecorator);
    PAL_DISALLOW_COPY_AND_ASSIGN(BorderColorPaletteDecorator);
};

// =====================================================================================================================
class CmdAllocatorDecorator : public ICmdAllocator
{
public:
    CmdAllocatorDecorator(ICmdAllocator* pNextCmdAllocator)
        :
        m_pNextLayer(pNextCmdAllocator)
    {}

    virtual Result Reset() override { return m_pNextLayer->Reset(); }

    // Part of the IDestroyable public interface.
    virtual void Destroy() override
    {
        ICmdAllocator*const pNextLayer = m_pNextLayer;
        this->~CmdAllocatorDecorator();
        pNextLayer->Destroy();
    }

    ICmdAllocator* GetNextLayer() const { return m_pNextLayer; }

protected:
    virtual ~CmdAllocatorDecorator() {}

    ICmdAllocator*const m_pNextLayer;

    PAL_DISALLOW_DEFAULT_CTOR(CmdAllocatorDecorator);
    PAL_DISALLOW_COPY_AND_ASSIGN(CmdAllocatorDecorator);
};

// =====================================================================================================================
class CmdBufferDecorator : public ICmdBuffer
{
public:
    CmdBufferDecorator(ICmdBuffer* pNextCmdBuffer, const DeviceDecorator* pNextDevice)
    :
    m_pNextLayer(pNextCmdBuffer), m_pDevice(pNextDevice)
    {}

    const IDevice*  GetDevice() const { return m_pDevice; }
    ICmdBuffer*     GetNextLayer() const { return m_pNextLayer; }

protected:
    ICmdBuffer*const             m_pNextLayer;
    const DeviceDecorator*const  m_pDevice;

    PAL_DISALLOW_DEFAULT_CTOR(CmdBufferDecorator);
    PAL_DISALLOW_COPY_AND_ASSIGN(CmdBufferDecorator);
};

// =====================================================================================================================
class CmdBufferFwdDecorator : public CmdBufferDecorator
{
public:
    CmdBufferFwdDecorator(ICmdBuffer* pNextCmdBuffer, const DeviceDecorator* pNextDevice)
        :
        CmdBufferDecorator(pNextCmdBuffer, pNextDevice)
    {
        m_funcTable.pfnCmdSetUserData[static_cast<uint32>(PipelineBindPoint::Compute)] =
            &CmdBufferFwdDecorator::CmdSetUserDataDecoratorCs;
        m_funcTable.pfnCmdSetUserData[static_cast<uint32>(PipelineBindPoint::Graphics)] =
            &CmdBufferFwdDecorator::CmdSetUserDataDecoratorGfx;
        m_funcTable.pfnCmdDraw                     = CmdDrawDecorator;
        m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexedDecorator;
        m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMultiDecorator;
        m_funcTable.pfnCmdDrawIndexedIndirectMulti = CmdDrawIndexedIndirectMultiDecorator;
        m_funcTable.pfnCmdDispatch                 = CmdDispatchDecorator;
        m_funcTable.pfnCmdDispatchIndirect         = CmdDispatchIndirectDecorator;
        m_funcTable.pfnCmdDispatchOffset           = CmdDispatchOffsetDecorator;
    }

    virtual Result Begin(const CmdBufferBuildInfo& info) override
        { return m_pNextLayer->Begin(NextCmdBufferBuildInfo(info)); }

    virtual Result End() override
        { return m_pNextLayer->End(); }

    virtual Result Reset(ICmdAllocator* pCmdAllocator, bool returnGpuMemory) override
        { return m_pNextLayer->Reset(NextCmdAllocator(pCmdAllocator), returnGpuMemory); }

    virtual uint32 GetEmbeddedDataLimit() const override
        { return m_pNextLayer->GetEmbeddedDataLimit(); }

    virtual void CmdBindPipeline(
        const PipelineBindParams& params) override
        { m_pNextLayer->CmdBindPipeline(NextPipelineBindParams(params)); }

    virtual void CmdBindMsaaState(
        const IMsaaState* pMsaaState) override
        { m_pNextLayer->CmdBindMsaaState(NextMsaaState(pMsaaState)); }

    virtual void CmdBindColorBlendState(
        const IColorBlendState* pColorBlendState) override
        { m_pNextLayer->CmdBindColorBlendState(NextColorBlendState(pColorBlendState)); }

    virtual void CmdBindDepthStencilState(
        const IDepthStencilState* pDepthStencilState) override
        { m_pNextLayer->CmdBindDepthStencilState(NextDepthStencilState(pDepthStencilState)); }
    virtual void CmdSetIndirectUserData(
        uint16      tableId,
        uint32      dwordOffset,
        uint32      dwordSize,
        const void* pSrcData) override
        { m_pNextLayer->CmdSetIndirectUserData(tableId, dwordOffset, dwordSize, pSrcData); }

    virtual void CmdSetIndirectUserDataWatermark(
        uint16 tableId,
        uint32 dwordLimit) override
        { m_pNextLayer->CmdSetIndirectUserDataWatermark(tableId, dwordLimit); }

    virtual void CmdBindIndexData(
        gpusize   gpuAddr,
        uint32    indexCount,
        IndexType indexType) override
        { m_pNextLayer->CmdBindIndexData(gpuAddr, indexCount, indexType); }

    virtual void CmdBindTargets(
        const BindTargetParams& params) override;

    virtual void CmdBindStreamOutTargets(
        const BindStreamOutTargetParams& params) override
        { m_pNextLayer->CmdBindStreamOutTargets(params); }

    virtual void CmdSetBlendConst(
        const BlendConstParams& params) override
        { m_pNextLayer->CmdSetBlendConst(params); }

    virtual void CmdSetInputAssemblyState(
        const InputAssemblyStateParams& params) override
        { m_pNextLayer->CmdSetInputAssemblyState(params); }

    virtual void CmdSetTriangleRasterState(
        const TriangleRasterStateParams& params) override
        { m_pNextLayer->CmdSetTriangleRasterState(params); }

    virtual void CmdSetPointLineRasterState(
        const PointLineRasterStateParams& params) override
        { m_pNextLayer->CmdSetPointLineRasterState(params); }

    virtual void CmdSetDepthBiasState(
        const DepthBiasParams& params) override
        { m_pNextLayer->CmdSetDepthBiasState(params); }

    virtual void CmdSetDepthBounds(
        const DepthBoundsParams& params) override
        { m_pNextLayer->CmdSetDepthBounds(params); }

    virtual void CmdSetStencilRefMasks(
        const StencilRefMaskParams& params) override
        { m_pNextLayer->CmdSetStencilRefMasks(params); }

    virtual void CmdSetMsaaQuadSamplePattern(
        uint32                       numSamplesPerPixel,
        const MsaaQuadSamplePattern& quadSamplePattern) override
        { m_pNextLayer->CmdSetMsaaQuadSamplePattern(numSamplesPerPixel, quadSamplePattern); }

    virtual void CmdSetViewports(
        const ViewportParams& params) override
        { m_pNextLayer->CmdSetViewports(params); }

    virtual void CmdSetScissorRects(
        const ScissorRectParams& params) override
        { m_pNextLayer->CmdSetScissorRects(params); }

    virtual void CmdSetGlobalScissor(
        const GlobalScissorParams& params) override
        { m_pNextLayer->CmdSetGlobalScissor(params); }

    virtual void CmdBarrier(const BarrierInfo& barrierInfo) override;

    virtual void CmdCopyMemory(
        const IGpuMemory&       srcGpuMemory,
        const IGpuMemory&       dstGpuMemory,
        uint32                  regionCount,
        const MemoryCopyRegion* pRegions) override
    {
        m_pNextLayer->CmdCopyMemory(*NextGpuMemory(&srcGpuMemory),
                                    *NextGpuMemory(&dstGpuMemory),
                                    regionCount,
                                    pRegions);
    }

    virtual void CmdCopyImage(
        const IImage&          srcImage,
        ImageLayout            srcImageLayout,
        const IImage&          dstImage,
        ImageLayout            dstImageLayout,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        uint32                 flags) override
    {
        m_pNextLayer->CmdCopyImage(*NextImage(&srcImage),
                                   srcImageLayout,
                                   *NextImage(&dstImage),
                                   dstImageLayout,
                                   regionCount,
                                   pRegions,
                                   flags);
    }

    virtual void CmdCopyMemoryToImage(
        const IGpuMemory&            srcGpuMemory,
        const IImage&                dstImage,
        ImageLayout                  dstImageLayout,
        uint32                       regionCount,
        const MemoryImageCopyRegion* pRegions) override
    {
        m_pNextLayer->CmdCopyMemoryToImage(*NextGpuMemory(&srcGpuMemory),
                                           *NextImage(&dstImage),
                                           dstImageLayout,
                                           regionCount,
                                           pRegions);
    }

    virtual void CmdCopyImageToMemory(
        const IImage&                srcImage,
        ImageLayout                  srcImageLayout,
        const IGpuMemory&            dstGpuMemory,
        uint32                       regionCount,
        const MemoryImageCopyRegion* pRegions) override
    {
        m_pNextLayer->CmdCopyImageToMemory(*NextImage(&srcImage),
                                           srcImageLayout,
                                           *NextGpuMemory(&dstGpuMemory),
                                           regionCount,
                                           pRegions);
    }

    virtual void CmdCopyMemoryToTiledImage(
        const IGpuMemory&                 srcGpuMemory,
        const IImage&                     dstImage,
        ImageLayout                       dstImageLayout,
        uint32                            regionCount,
        const MemoryTiledImageCopyRegion* pRegions) override
    {
        m_pNextLayer->CmdCopyMemoryToTiledImage(*NextGpuMemory(&srcGpuMemory),
                                                *NextImage(&dstImage),
                                                dstImageLayout,
                                                regionCount,
                                                pRegions);
    }

    virtual void CmdCopyTiledImageToMemory(
        const IImage&                     srcImage,
        ImageLayout                       srcImageLayout,
        const IGpuMemory&                 dstGpuMemory,
        uint32                            regionCount,
        const MemoryTiledImageCopyRegion* pRegions) override
    {
        m_pNextLayer->CmdCopyTiledImageToMemory(*NextImage(&srcImage),
                                                srcImageLayout,
                                                *NextGpuMemory(&dstGpuMemory),
                                                regionCount,
                                                pRegions);
    }

    virtual void CmdCopyTypedBuffer(
        const IGpuMemory&            srcGpuMemory,
        const IGpuMemory&            dstGpuMemory,
        uint32                       regionCount,
        const TypedBufferCopyRegion* pRegions) override
    {
        m_pNextLayer->CmdCopyTypedBuffer(*NextGpuMemory(&srcGpuMemory),
                                         *NextGpuMemory(&dstGpuMemory),
                                         regionCount,
                                         pRegions);
    }

    virtual void CmdCopyRegisterToMemory(
        uint32            srcRegisterOffset,
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset) override
    {
        m_pNextLayer->CmdCopyRegisterToMemory(srcRegisterOffset, *NextGpuMemory(&dstGpuMemory), dstOffset);
    }

    virtual void CmdScaledCopyImage(
        const ScaledCopyInfo&        copyInfo) override
    {
        ScaledCopyInfo nextCopyInfo = {};

        nextCopyInfo.pSrcImage      = NextImage(copyInfo.pSrcImage);
        nextCopyInfo.srcImageLayout = copyInfo.srcImageLayout;
        nextCopyInfo.pDstImage      = NextImage(copyInfo.pDstImage);
        nextCopyInfo.dstImageLayout = copyInfo.dstImageLayout;
        nextCopyInfo.regionCount    = copyInfo.regionCount;
        nextCopyInfo.pRegions       = copyInfo.pRegions;
        nextCopyInfo.filter         = copyInfo.filter;
        nextCopyInfo.rotation       = copyInfo.rotation;
        nextCopyInfo.pColorKey      = copyInfo.pColorKey;
        nextCopyInfo.flags          = copyInfo.flags;

        m_pNextLayer->CmdScaledCopyImage(nextCopyInfo);
    }

    virtual void CmdColorSpaceConversionCopy(
        const IImage&                     srcImage,
        ImageLayout                       srcImageLayout,
        const IImage&                     dstImage,
        ImageLayout                       dstImageLayout,
        uint32                            regionCount,
        const ColorSpaceConversionRegion* pRegions,
        TexFilter                         filter,
        const ColorSpaceConversionTable&  cscTable) override
    {
        m_pNextLayer->CmdColorSpaceConversionCopy(*NextImage(&srcImage),
                                                  srcImageLayout,
                                                  *NextImage(&dstImage),
                                                  dstImageLayout,
                                                  regionCount,
                                                  pRegions,
                                                  filter,
                                                  cscTable);
    }

    virtual void CmdCloneImageData(
        const IImage& srcImage,
        const IImage& dstImage) override
    {
        m_pNextLayer->CmdCloneImageData(*NextImage(&srcImage), *NextImage(&dstImage));
    }

    virtual void CmdUpdateMemory(
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset,
        gpusize           dataSize,
        const uint32*     pData) override
        { m_pNextLayer->CmdUpdateMemory(*NextGpuMemory(&dstGpuMemory), dstOffset, dataSize, pData); }

    virtual void CmdUpdateBusAddressableMemoryMarker(
        const IGpuMemory& dstGpuMemory,
        uint32            value) override
        { m_pNextLayer->CmdUpdateBusAddressableMemoryMarker(*NextGpuMemory(&dstGpuMemory), value); }

    virtual void CmdFillMemory(
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset,
        gpusize           fillSize,
        uint32            data) override
        { m_pNextLayer->CmdFillMemory(*NextGpuMemory(&dstGpuMemory), dstOffset, fillSize, data); }

    virtual void CmdClearColorBuffer(
        const IGpuMemory& gpuMemory,
        const ClearColor& color,
        SwizzledFormat    bufferFormat,
        uint32            bufferOffset,
        uint32            bufferExtent,
        uint32            rangeCount = 0,
        const Range*      pRanges    = nullptr) override
    {
        m_pNextLayer->CmdClearColorBuffer(*NextGpuMemory(&gpuMemory),
                                          color,
                                          bufferFormat,
                                          bufferOffset,
                                          bufferExtent,
                                          rangeCount,
                                          pRanges);
    }

    virtual void CmdClearBoundColorTargets(
        uint32                          colorTargetCount,
        const BoundColorTarget*         pBoundColorTargets,
        uint32                          regionCount,
        const ClearBoundTargetRegion*   pClearRegions) override
    {
        m_pNextLayer->CmdClearBoundColorTargets(colorTargetCount,
                                                pBoundColorTargets,
                                                regionCount,
                                                pClearRegions);
    }

    virtual void CmdClearColorImage(
        const IImage&      image,
        ImageLayout        imageLayout,
        const ClearColor&  color,
        uint32             rangeCount,
        const SubresRange* pRanges,
        uint32             boxCount,
        const Box*         pBoxes,
        uint32             flags) override
    {
        m_pNextLayer->CmdClearColorImage(*NextImage(&image),
                                         imageLayout,
                                         color,
                                         rangeCount,
                                         pRanges,
                                         boxCount,
                                         pBoxes,
                                         flags);
    }

    virtual void CmdClearBoundDepthStencilTargets(
        float                           depth,
        uint8                           stencil,
        uint32                          samples,
        uint32                          fragments,
        DepthStencilSelectFlags         flag,
        uint32                          regionCount,
        const ClearBoundTargetRegion*   pClearRegions) override
    {
        m_pNextLayer->CmdClearBoundDepthStencilTargets(depth,
                                                       stencil,
                                                       samples,
                                                       fragments,
                                                       flag,
                                                       regionCount,
                                                       pClearRegions);
    }

    virtual void CmdClearDepthStencil(
        const IImage&      image,
        ImageLayout        depthLayout,
        ImageLayout        stencilLayout,
        float              depth,
        uint8              stencil,
        uint32             rangeCount,
        const SubresRange* pRanges,
        uint32             rectCount,
        const Rect*        pRects,
        uint32             flags) override
    {
        m_pNextLayer->CmdClearDepthStencil(*NextImage(&image),
                                           depthLayout,
                                           stencilLayout,
                                           depth,
                                           stencil,
                                           rangeCount,
                                           pRanges,
                                           rectCount,
                                           pRects,
                                           flags);
    }

    virtual void CmdClearBufferView(
        const IGpuMemory& gpuMemory,
        const ClearColor& color,
        const void*       pBufferViewSrd,
        uint32            rangeCount = 0,
        const Range*      pRanges    = nullptr) override
        { m_pNextLayer->CmdClearBufferView(*NextGpuMemory(&gpuMemory), color, pBufferViewSrd, rangeCount, pRanges); }

    virtual void CmdClearImageView(
        const IImage&     image,
        ImageLayout       imageLayout,
        const ClearColor& color,
        const void*       pImageViewSrd,
        uint32            rectCount = 0,
        const Rect*       pRects    = nullptr) override
        { m_pNextLayer->CmdClearImageView(*NextImage(&image), imageLayout, color, pImageViewSrd, rectCount, pRects); }

    virtual void CmdResolveImage(
        const IImage&             srcImage,
        ImageLayout               srcImageLayout,
        const IImage&             dstImage,
        ImageLayout               dstImageLayout,
        ResolveMode               resolveMode,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions) override
    {
        m_pNextLayer->CmdResolveImage(*NextImage(&srcImage),
                                      srcImageLayout,
                                      *NextImage(&dstImage),
                                      dstImageLayout,
                                      resolveMode,
                                      regionCount,
                                      pRegions);
    }

    virtual void CmdCopyImageToPackedPixelImage(
        const IImage&          srcImage,
        const IImage&          dstImage,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        Pal::PackedPixelType   packPixelType) override
    {
        m_pNextLayer->CmdCopyImageToPackedPixelImage(*NextImage(&srcImage),
                                                     *NextImage(&dstImage),
                                                     regionCount,
                                                     pRegions,
                                                     packPixelType);
    }

    virtual void CmdSetEvent(
        const IGpuEvent& gpuEvent, HwPipePoint setPoint) override
        { m_pNextLayer->CmdSetEvent(*NextGpuEvent(&gpuEvent), setPoint); }

    virtual void CmdResetEvent(
        const IGpuEvent& gpuEvent, HwPipePoint resetPoint) override
        { m_pNextLayer->CmdResetEvent(*NextGpuEvent(&gpuEvent), resetPoint); }

    virtual void CmdPredicateEvent(
        const IGpuEvent& gpuEvent) override
        { m_pNextLayer->CmdPredicateEvent(*NextGpuEvent(&gpuEvent)); }

    virtual void CmdMemoryAtomic(
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset,
        uint64            srcData,
        AtomicOp          atomicOp) override
        { m_pNextLayer->CmdMemoryAtomic(*NextGpuMemory(&dstGpuMemory), dstOffset, srcData, atomicOp); }

    virtual void CmdBeginQuery(
        const IQueryPool& queryPool,
        QueryType         queryType,
        uint32            slot,
        QueryControlFlags flags) override
        { m_pNextLayer->CmdBeginQuery(*NextQueryPool(&queryPool), queryType, slot, flags); }

    virtual void CmdEndQuery(
        const IQueryPool& queryPool,
        QueryType         queryType,
        uint32            slot) override
        { m_pNextLayer->CmdEndQuery(*NextQueryPool(&queryPool), queryType, slot); }

    virtual void CmdResolveQuery(
        const IQueryPool& queryPool,
        QueryResultFlags  flags,
        QueryType         queryType,
        uint32            startQuery,
        uint32            queryCount,
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset,
        gpusize           dstStride) override
    {
        m_pNextLayer->CmdResolveQuery(*NextQueryPool(&queryPool),
                                      flags,
                                      queryType,
                                      startQuery,
                                      queryCount,
                                      *NextGpuMemory(&dstGpuMemory),
                                      dstOffset,
                                      dstStride);
    }

    virtual void CmdResetQueryPool(
        const IQueryPool& queryPool,
        uint32            startQuery,
        uint32            queryCount) override
        { m_pNextLayer->CmdResetQueryPool(*NextQueryPool(&queryPool), startQuery, queryCount); }

    virtual void CmdWriteTimestamp(
        HwPipePoint       pipePoint,
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset) override
        { m_pNextLayer->CmdWriteTimestamp(pipePoint, *NextGpuMemory(&dstGpuMemory), dstOffset); }

    virtual void CmdWriteImmediate(
        HwPipePoint        pipePoint,
        uint64             data,
        ImmediateDataWidth dataSize,
        gpusize            address) override
        { m_pNextLayer->CmdWriteImmediate(pipePoint, data, dataSize, address); }

    virtual void CmdLoadGds(
        HwPipePoint       pipePoint,
        uint32            dstGdsOffset,
        const IGpuMemory& srcGpuMemory,
        gpusize           srcMemOffset,
        uint32            size) override
    {
        m_pNextLayer->CmdLoadGds(pipePoint,
                                 dstGdsOffset,
                                 *NextGpuMemory(&srcGpuMemory),
                                 srcMemOffset,
                                 size);
    }

    virtual void CmdStoreGds(
        HwPipePoint       pipePoint,
        uint32            srcGdsOffset,
        const IGpuMemory& dstGpuMemory,
        gpusize           dstMemOffset,
        uint32            size,
        bool              waitForWC) override
    {
        m_pNextLayer->CmdStoreGds(pipePoint,
                                  srcGdsOffset,
                                  *NextGpuMemory(&dstGpuMemory),
                                  dstMemOffset,
                                  size,
                                  waitForWC);
    }

    virtual void CmdUpdateGds(
        HwPipePoint       pipePoint,
        uint32            gdsOffset,
        uint32            dataSize,
        const uint32*     pData) override
    {
        m_pNextLayer->CmdUpdateGds(pipePoint,
                                   gdsOffset,
                                   dataSize,
                                   pData);
    }

    virtual void CmdFillGds(
        HwPipePoint       pipePoint,
        uint32            gdsOffset,
        uint32            fillSize,
        uint32            data) override
    {
        m_pNextLayer->CmdFillGds(pipePoint,
                                 gdsOffset,
                                 fillSize,
                                 data);
    }

    virtual void CmdLoadBufferFilledSizes(
        const gpusize (&gpuVirtAddr)[MaxStreamOutTargets]) override
        { m_pNextLayer->CmdLoadBufferFilledSizes(gpuVirtAddr); }

    virtual void CmdSaveBufferFilledSizes(
        const gpusize (&gpuVirtAddr)[MaxStreamOutTargets]) override
        { m_pNextLayer->CmdSaveBufferFilledSizes(gpuVirtAddr); }

    virtual void CmdBindBorderColorPalette(
        PipelineBindPoint          pipelineBindPoint,
        const IBorderColorPalette* pPalette) override
        { m_pNextLayer->CmdBindBorderColorPalette(pipelineBindPoint, NextBorderColorPalette(pPalette)); }

    virtual void CmdSetPredication(
        IQueryPool*         pQueryPool,
        uint32              slot,
        const IGpuMemory*   pGpuMemory,
        gpusize             offset,
        PredicateType       predType,
        bool                predPolarity,
        bool                waitResults,
        bool                accumulateData) override
    {
        m_pNextLayer->CmdSetPredication(pQueryPool, slot, pGpuMemory, offset, predType,
                                        predPolarity, waitResults, accumulateData);
    }

    virtual void CmdIf(
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint64            data,
        uint64            mask,
        CompareFunc       compareFunc) override
        { m_pNextLayer->CmdIf(*NextGpuMemory(&gpuMemory), offset, data, mask, compareFunc); }

    virtual void CmdElse() override
        { m_pNextLayer->CmdElse(); }

    virtual void CmdEndIf() override
        { m_pNextLayer->CmdEndIf(); }

    virtual void CmdWhile(
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint64            data,
        uint64            mask,
        CompareFunc       compareFunc) override
        { m_pNextLayer->CmdWhile(*NextGpuMemory(&gpuMemory), offset, data, mask, compareFunc); }

    virtual void CmdEndWhile() override
        { m_pNextLayer->CmdEndWhile(); }

    virtual void CmdWaitRegisterValue(
        uint32      registerOffset,
        uint32      data,
        uint32      mask,
        CompareFunc compareFunc) override
        { m_pNextLayer->CmdWaitRegisterValue(registerOffset, data, mask, compareFunc); }

    virtual void CmdWaitMemoryValue(
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint32            data,
        uint32            mask,
        CompareFunc       compareFunc) override
        { m_pNextLayer->CmdWaitMemoryValue(*NextGpuMemory(&gpuMemory), offset, data, mask, compareFunc); }

    virtual void CmdWaitBusAddressableMemoryMarker(
        const IGpuMemory& gpuMemory,
        uint32            data,
        uint32            mask,
        CompareFunc       compareFunc) override
        { m_pNextLayer->CmdWaitBusAddressableMemoryMarker(*NextGpuMemory(&gpuMemory), data, mask, compareFunc); }

    virtual void CmdBeginPerfExperiment(
        IPerfExperiment* pPerfExperiment) override
        { m_pNextLayer->CmdBeginPerfExperiment(NextPerfExperiment(pPerfExperiment)); }

    virtual void CmdUpdatePerfExperimentSqttTokenMask(
        IPerfExperiment* pPerfExperiment,
        uint32           sqttTokenMask) override
        { m_pNextLayer->CmdUpdatePerfExperimentSqttTokenMask(NextPerfExperiment(pPerfExperiment), sqttTokenMask); }

    virtual void CmdEndPerfExperiment(
        IPerfExperiment* pPerfExperiment) override
        { m_pNextLayer->CmdEndPerfExperiment(NextPerfExperiment(pPerfExperiment)); }

    virtual void CmdInsertTraceMarker(
        PerfTraceMarkerType markerType,
        uint32              markerData) override
        { m_pNextLayer->CmdInsertTraceMarker(markerType, markerData); }

    virtual void CmdInsertRgpTraceMarker(
        uint32      numDwords,
        const void* pData) override
        { m_pNextLayer->CmdInsertRgpTraceMarker(numDwords, pData); }

    virtual void CmdLoadCeRam(
        const IGpuMemory& srcGpuMemory,
        gpusize           memOffset,
        uint32            ramOffset,
        uint32            dwordSize) override
        { m_pNextLayer->CmdLoadCeRam(*NextGpuMemory(&srcGpuMemory), memOffset, ramOffset, dwordSize); }

    virtual void CmdDumpCeRam(
        const IGpuMemory& dstGpuMemory,
        gpusize           memOffset,
        uint32            ramOffset,
        uint32            dwordSize,
        uint32            currRingPos,
        uint32            ringSize) override
    {
        m_pNextLayer->CmdDumpCeRam(*NextGpuMemory(&dstGpuMemory),
                                   memOffset,
                                   ramOffset,
                                   dwordSize,
                                   currRingPos,
                                   ringSize);
    }

    virtual void CmdWriteCeRam(
        const void* pSrcData,
        uint32      ramOffset,
        uint32      dwordSize) override
        { m_pNextLayer->CmdWriteCeRam(pSrcData, ramOffset, dwordSize); }

    virtual uint32* CmdAllocateEmbeddedData(
        uint32   sizeInDwords,
        uint32   alignmentInDwords,
        gpusize* pGpuAddress) override
        { return m_pNextLayer->CmdAllocateEmbeddedData(sizeInDwords, alignmentInDwords, pGpuAddress); }

    virtual void CmdExecuteNestedCmdBuffers(
        uint32            cmdBufferCount,
        ICmdBuffer*const* ppCmdBuffers) override;

    virtual void CmdSaveComputeState(
        uint32 stateFlags) override
        { m_pNextLayer->CmdSaveComputeState(stateFlags); }

    virtual void CmdRestoreComputeState(
        uint32 stateFlags) override
        { m_pNextLayer->CmdRestoreComputeState(stateFlags); }

    virtual void CmdExecuteIndirectCmds(
        const IIndirectCmdGenerator& generator,
        const IGpuMemory&            gpuMemory,
        gpusize                      offset,
        uint32                       maximumCount,
        gpusize                      countGpuAddr) override
    {
        m_pNextLayer->CmdExecuteIndirectCmds(*NextIndirectCmdGenerator(&generator),
                                             *NextGpuMemory(&gpuMemory),
                                             offset,
                                             maximumCount,
                                             countGpuAddr);
    }

    virtual void CmdCommentString(const char* pComment) override
        { return m_pNextLayer->CmdCommentString(pComment); }

    virtual void CmdSetUserClipPlanes(
        uint32               firstPlane,
        uint32               planeCount,
        const UserClipPlane* pPlanes) override
        { m_pNextLayer->CmdSetUserClipPlanes(firstPlane, planeCount, pPlanes); }

    virtual void CmdFlglSync() override
        { m_pNextLayer->CmdFlglSync(); }

    virtual void CmdFlglEnable() override
        { m_pNextLayer->CmdFlglEnable(); }

    virtual void CmdFlglDisable() override
        { m_pNextLayer->CmdFlglDisable(); }

    virtual void CmdXdmaWaitFlipPending() override
        { m_pNextLayer->CmdXdmaWaitFlipPending(); }

    virtual void CmdStartGpuProfilerLogging() override
        { m_pNextLayer->CmdStartGpuProfilerLogging(); }

    virtual void CmdStopGpuProfilerLogging() override
        { m_pNextLayer->CmdStopGpuProfilerLogging(); }

    virtual void CmdSetViewInstanceMask(uint32 mask) override
        { m_pNextLayer->CmdSetViewInstanceMask(mask); }

    virtual void CmdSetHiSCompareState0(
        CompareFunc compFunc,
        uint32      compMask,
        uint32      compValue,
        bool        enable) override
    {
        m_pNextLayer->CmdSetHiSCompareState0(compFunc, compMask, compValue, enable);
    }

    virtual void CmdSetHiSCompareState1(
        CompareFunc compFunc,
        uint32      compMask,
        uint32      compValue,
        bool        enable) override
    {
        m_pNextLayer->CmdSetHiSCompareState1(compFunc, compMask, compValue, enable);
    }

    // Part of the IDestroyable public interface.
    virtual void Destroy() override
    {
        ICmdBuffer* pNextLayer = m_pNextLayer;
        this->~CmdBufferFwdDecorator();
        pNextLayer->Destroy();
    }

protected:
    virtual ~CmdBufferFwdDecorator() {}

private:
    static void PAL_STDCALL CmdSetUserDataDecoratorCs(
        ICmdBuffer*   pCmdBuffer,
        uint32        firstEntry,
        uint32        entryCount,
        const uint32* pEntryValues);

    static void PAL_STDCALL CmdSetUserDataDecoratorGfx(
        ICmdBuffer*   pCmdBuffer,
        uint32        firstEntry,
        uint32        entryCount,
        const uint32* pEntryValues);

    static void PAL_STDCALL CmdDrawDecorator(
        ICmdBuffer* pCmdBuffer,
        uint32      firstVertex,
        uint32      vertexCount,
        uint32      firstInstance,
        uint32      instanceCount)
    {
        ICmdBuffer* pNextLayer = static_cast<CmdBufferFwdDecorator*>(pCmdBuffer)->m_pNextLayer;
        pNextLayer->CmdDraw(firstVertex, vertexCount, firstInstance, instanceCount);
    }

    static void PAL_STDCALL CmdDrawIndexedDecorator(
        ICmdBuffer* pCmdBuffer,
        uint32      firstIndex,
        uint32      indexCount,
        int32       vertexOffset,
        uint32      firstInstance,
        uint32      instanceCount)
    {
        ICmdBuffer* pNextLayer = static_cast<CmdBufferFwdDecorator*>(pCmdBuffer)->m_pNextLayer;
        pNextLayer->CmdDrawIndexed(firstIndex, indexCount, vertexOffset, firstInstance, instanceCount);
    }

    static void PAL_STDCALL CmdDrawIndirectMultiDecorator(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint32            stride,
        uint32            maximumCount,
        gpusize           countGpuAddr)
    {
        ICmdBuffer* pNextLayer = static_cast<CmdBufferFwdDecorator*>(pCmdBuffer)->m_pNextLayer;
        pNextLayer->CmdDrawIndirectMulti(*NextGpuMemory(&gpuMemory), offset, stride, maximumCount, countGpuAddr);
    }

    static void PAL_STDCALL CmdDrawIndexedIndirectMultiDecorator(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint32            stride,
        uint32            maximumCount,
        gpusize           countGpuAddr)
    {
        ICmdBuffer* pNextLayer = static_cast<CmdBufferFwdDecorator*>(pCmdBuffer)->m_pNextLayer;
        pNextLayer->CmdDrawIndexedIndirectMulti(*NextGpuMemory(&gpuMemory), offset, stride, maximumCount, countGpuAddr);
    }

    static void PAL_STDCALL CmdDispatchDecorator(
        ICmdBuffer* pCmdBuffer,
        uint32      x,
        uint32      y,
        uint32      z)
    {
        ICmdBuffer* pNextLayer = static_cast<CmdBufferFwdDecorator*>(pCmdBuffer)->m_pNextLayer;
        pNextLayer->CmdDispatch(x, y, z);
    }

    static void PAL_STDCALL CmdDispatchIndirectDecorator(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset)
    {
        ICmdBuffer* pNextLayer = static_cast<CmdBufferFwdDecorator*>(pCmdBuffer)->m_pNextLayer;
        pNextLayer->CmdDispatchIndirect(*NextGpuMemory(&gpuMemory), offset);
    }

    static void PAL_STDCALL CmdDispatchOffsetDecorator(
        ICmdBuffer* pCmdBuffer,
        uint32      xOffset,
        uint32      yOffset,
        uint32      zOffset,
        uint32      xDim,
        uint32      yDim,
        uint32      zDim)
    {
        ICmdBuffer* pNextLayer = static_cast<CmdBufferFwdDecorator*>(pCmdBuffer)->m_pNextLayer;
        pNextLayer->CmdDispatchOffset(xOffset, yOffset, zOffset, xDim, yDim, zDim);
    }

    PAL_DISALLOW_DEFAULT_CTOR(CmdBufferFwdDecorator);
    PAL_DISALLOW_COPY_AND_ASSIGN(CmdBufferFwdDecorator);
};

// =====================================================================================================================
class ColorBlendStateDecorator : public IColorBlendState
{
public:
    ColorBlendStateDecorator(IColorBlendState* pNextState, const DeviceDecorator* pNextDevice)
        :
        m_pNextLayer(pNextState), m_pDevice(pNextDevice)
    {}

    // Part of the IDestroyable public interface.
    virtual void Destroy() override
    {
        IColorBlendState* pNextLayer = m_pNextLayer;
        this->~ColorBlendStateDecorator();
        pNextLayer->Destroy();
    }

    const IDevice*    GetDevice() const { return m_pDevice; }
    IColorBlendState* GetNextLayer() const { return m_pNextLayer; }

protected:
    virtual ~ColorBlendStateDecorator() {}

    IColorBlendState*const      m_pNextLayer;
    const DeviceDecorator*const m_pDevice;

private:
    PAL_DISALLOW_DEFAULT_CTOR(ColorBlendStateDecorator);
    PAL_DISALLOW_COPY_AND_ASSIGN(ColorBlendStateDecorator);

};

// =====================================================================================================================
class ColorTargetViewDecorator : public IColorTargetView
{
public:
    ColorTargetViewDecorator(IColorTargetView* pNextView, const DeviceDecorator* pNextDevice)
        :
        m_pNextLayer(pNextView), m_pDevice(pNextDevice)
    {}

    const IDevice*    GetDevice() const { return m_pDevice; }
    IColorTargetView* GetNextLayer() const { return m_pNextLayer; }

protected:
    virtual ~ColorTargetViewDecorator() {}

    IColorTargetView*const      m_pNextLayer;
    const DeviceDecorator*const m_pDevice;

private:
    PAL_DISALLOW_DEFAULT_CTOR(ColorTargetViewDecorator);
    PAL_DISALLOW_COPY_AND_ASSIGN(ColorTargetViewDecorator);

};

// =====================================================================================================================
class DepthStencilStateDecorator : public IDepthStencilState
{
public:
    DepthStencilStateDecorator(IDepthStencilState* pNextState, const DeviceDecorator* pNextDevice)
        :
        m_pNextLayer(pNextState), m_pDevice(pNextDevice)
    {}

    // Part of the IDestroyable public interface.
    virtual void Destroy() override
    {
        IDepthStencilState* pNextLayer = m_pNextLayer;
        this->~DepthStencilStateDecorator();
        pNextLayer->Destroy();
    }

    const IDevice*      GetDevice() const { return m_pDevice; }
    IDepthStencilState* GetNextLayer() const { return m_pNextLayer; }

protected:
    virtual ~DepthStencilStateDecorator() {}

    IDepthStencilState*const    m_pNextLayer;
    const DeviceDecorator*const m_pDevice;

private:
    PAL_DISALLOW_DEFAULT_CTOR(DepthStencilStateDecorator);
    PAL_DISALLOW_COPY_AND_ASSIGN(DepthStencilStateDecorator);

};

// =====================================================================================================================
class DepthStencilViewDecorator : public IDepthStencilView
{
public:
    DepthStencilViewDecorator(IDepthStencilView* pNextView, const DeviceDecorator* pNextDevice)
        :
        m_pNextLayer(pNextView), m_pDevice(pNextDevice)
    {}

    const IDevice*     GetDevice() const { return m_pDevice; }
    IDepthStencilView* GetNextLayer() const { return m_pNextLayer; }

protected:
    virtual ~DepthStencilViewDecorator() {}

    IDepthStencilView*const     m_pNextLayer;
    const DeviceDecorator*const m_pDevice;

private:
    PAL_DISALLOW_DEFAULT_CTOR(DepthStencilViewDecorator);
    PAL_DISALLOW_COPY_AND_ASSIGN(DepthStencilViewDecorator);

};

// =====================================================================================================================
class FenceDecorator : public IFence
{
public:
    FenceDecorator(IFence* pNextFence, const DeviceDecorator* pNextDevice)
        :
        m_pNextLayer(pNextFence), m_pDevice(pNextDevice)
    {}

    virtual ~FenceDecorator() {}

    virtual Result GetStatus() const override
        { return m_pNextLayer->GetStatus(); }

    virtual OsExternalHandle GetHandle() const override
        { return m_pNextLayer->GetHandle(); }

    // Part of the IDestroyable public interface.
    virtual void Destroy() override
    {
        IFence* pNextLayer = m_pNextLayer;
        this->~FenceDecorator();
        pNextLayer->Destroy();
    }

    const IDevice*  GetDevice() const { return m_pDevice; }
    IFence*         GetNextLayer() const { return m_pNextLayer; }

protected:
    IFence*const                m_pNextLayer;
    const DeviceDecorator*const m_pDevice;

private:
    PAL_DISALLOW_DEFAULT_CTOR(FenceDecorator);
    PAL_DISALLOW_COPY_AND_ASSIGN(FenceDecorator);

};

// =====================================================================================================================
class GpuEventDecorator : public IGpuEvent
{
public:
    GpuEventDecorator(IGpuEvent* pNextEvent, const DeviceDecorator* pNextDevice)
        :
        m_pNextLayer(pNextEvent), m_pDevice(pNextDevice)
    {}

    virtual Result GetStatus() override
        { return m_pNextLayer->GetStatus(); }

    virtual Result Set() override
        { return m_pNextLayer->Set(); }

    virtual Result Reset() override
        { return m_pNextLayer->Reset(); }

    // Part of the IDestroyable public interface.
    virtual void Destroy() override
    {
        IGpuEvent* pNextLayer = m_pNextLayer;
        this->~GpuEventDecorator();
        pNextLayer->Destroy();
    }

    const IDevice*  GetDevice() const { return m_pDevice; }
    IGpuEvent*      GetNextLayer() const { return m_pNextLayer; }

protected:
    virtual ~GpuEventDecorator() {}

    IGpuEvent*const             m_pNextLayer;
    const DeviceDecorator*const m_pDevice;

private:
    PAL_DISALLOW_DEFAULT_CTOR(GpuEventDecorator);
    PAL_DISALLOW_COPY_AND_ASSIGN(GpuEventDecorator);
};

// =====================================================================================================================
class GpuMemoryDecorator : public IGpuMemory
{
public:
    GpuMemoryDecorator(IGpuMemory* pNextGpuMem, const DeviceDecorator* pNextDevice);

    virtual Result SetPriority(
        GpuMemPriority       priority,
        GpuMemPriorityOffset priorityOffset) override
        { return m_pNextLayer->SetPriority(priority, priorityOffset); }

    virtual Result Map(
        void** ppData) override
        { return m_pNextLayer->Map(ppData); }

    virtual Result Unmap() override
        { return m_pNextLayer->Unmap(); }

    virtual OsExternalHandle GetSharedExternalHandle() const override
        { return m_pNextLayer->GetSharedExternalHandle(); }

    // Part of the IDestroyable public interface.
    virtual void Destroy() override
    {
        IGpuMemory* pNextLayer = m_pNextLayer;
        this->~GpuMemoryDecorator();
        pNextLayer->Destroy();
    }

    const IDevice* GetDevice()    const { return m_pDevice; }
    IGpuMemory*    GetNextLayer() const { return m_pNextLayer; }

protected:
    virtual ~GpuMemoryDecorator() {}

    IGpuMemory*const            m_pNextLayer;
    const DeviceDecorator*const m_pDevice;

private:
    PAL_DISALLOW_DEFAULT_CTOR(GpuMemoryDecorator);
    PAL_DISALLOW_COPY_AND_ASSIGN(GpuMemoryDecorator);
};

// =====================================================================================================================
class ImageDecorator : public IImage
{
public:
    ImageDecorator(IImage* pNextImage, const DeviceDecorator* pNextDevice)
        :
        IImage(pNextImage->GetImageCreateInfo()),
        m_pNextLayer(pNextImage), m_pDevice(pNextDevice)
    {}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 365
    virtual Result GetMemoryLayout(
        ImageMemoryLayout* pLayout) const override
        { return m_pNextLayer->GetMemoryLayout(pLayout); }
#endif
    virtual const ImageMemoryLayout& GetMemoryLayout() const override
        { return m_pNextLayer->GetMemoryLayout(); }

    virtual Result GetSubresourceLayout(
        SubresId      subresId,
        SubresLayout* pLayout) const override
        { return m_pNextLayer->GetSubresourceLayout(subresId, pLayout); }

    virtual void GetGpuMemoryRequirements(
        GpuMemoryRequirements* pGpuMemReqs) const override
        { m_pNextLayer->GetGpuMemoryRequirements(pGpuMemReqs); }

    virtual Result BindGpuMemory(
        IGpuMemory* pGpuMemory,
        gpusize     offset) override
        { return m_pNextLayer->BindGpuMemory(NextGpuMemory(pGpuMemory), offset); }

    virtual void SetOptimalSharingLevel(MetadataSharingLevel level) override
        { m_pNextLayer->SetOptimalSharingLevel(level); }

    virtual MetadataSharingLevel GetOptimalSharingLevel() const override
        { return m_pNextLayer->GetOptimalSharingLevel(); }

    // Part of the IDestroyable public interface.
    virtual void Destroy() override
    {
        IImage* pNextLayer = m_pNextLayer;
        this->~ImageDecorator();
        pNextLayer->Destroy();
    }

    const IDevice* GetDevice() const { return m_pDevice; }
    IImage*        GetNextLayer() const { return m_pNextLayer; }

protected:
    virtual ~ImageDecorator() {}

    IImage*const                m_pNextLayer;
    const DeviceDecorator*const m_pDevice;

private:
    PAL_DISALLOW_DEFAULT_CTOR(ImageDecorator);
    PAL_DISALLOW_COPY_AND_ASSIGN(ImageDecorator);
};

// =====================================================================================================================
class IndirectCmdGeneratorDecorator : public IIndirectCmdGenerator
{
public:
    IndirectCmdGeneratorDecorator(IIndirectCmdGenerator* pNextGenerator, const DeviceDecorator* pNextDevice)
        :
        m_pNextLayer(pNextGenerator), m_pDevice(pNextDevice)
    {}

    virtual void GetGpuMemoryRequirements(
        GpuMemoryRequirements* pGpuMemReqs) const override
        { m_pNextLayer->GetGpuMemoryRequirements(pGpuMemReqs); }

    virtual Result BindGpuMemory(
        IGpuMemory* pGpuMemory,
        gpusize     offset) override
        { return m_pNextLayer->BindGpuMemory(NextGpuMemory(pGpuMemory), offset); }

    virtual void Destroy() override
    {
        IIndirectCmdGenerator* pNextLayer = m_pNextLayer;
        this->~IndirectCmdGeneratorDecorator();
        pNextLayer->Destroy();
    }

    const IDevice*         GetDevice() const { return m_pDevice; }
    IIndirectCmdGenerator* GetNextLayer() const { return m_pNextLayer; }

protected:
    virtual ~IndirectCmdGeneratorDecorator() {}

    IIndirectCmdGenerator*const  m_pNextLayer;
    const DeviceDecorator*const  m_pDevice;

private:
    PAL_DISALLOW_DEFAULT_CTOR(IndirectCmdGeneratorDecorator);
    PAL_DISALLOW_COPY_AND_ASSIGN(IndirectCmdGeneratorDecorator);
};

// =====================================================================================================================
class MsaaStateDecorator : public IMsaaState
{
public:
    MsaaStateDecorator(IMsaaState* pNextState, const DeviceDecorator* pNextDevice)
        :
        m_pNextLayer(pNextState), m_pDevice(pNextDevice)
    {}

    // Part of the IDestroyable public interface.
    virtual void Destroy() override
    {
        IMsaaState* pNextLayer = m_pNextLayer;
        this->~MsaaStateDecorator();
        pNextLayer->Destroy();
    }

    const IDevice*  GetDevice() const { return m_pDevice; }
    IMsaaState*     GetNextLayer() const { return m_pNextLayer; }

protected:
    virtual ~MsaaStateDecorator() {}

    IMsaaState*const            m_pNextLayer;
    const DeviceDecorator*const m_pDevice;

private:
    PAL_DISALLOW_DEFAULT_CTOR(MsaaStateDecorator);
    PAL_DISALLOW_COPY_AND_ASSIGN(MsaaStateDecorator);
};

// =====================================================================================================================
class PerfExperimentDecorator : public IPerfExperiment
{
public:
    PerfExperimentDecorator(IPerfExperiment* pNextExperiment, const DeviceDecorator* pNextDevice)
        :
        m_pNextLayer(pNextExperiment), m_pDevice(pNextDevice)
    {}

    virtual Result AddCounter(
        const PerfCounterInfo& counterInfo) override
        { return m_pNextLayer->AddCounter(counterInfo); }

    virtual Result GetGlobalCounterLayout(
        GlobalCounterLayout* pLayout) const override
        { return m_pNextLayer->GetGlobalCounterLayout(pLayout); }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 373
    virtual Result AddTrace(
        const PerfTraceInfo& traceInfo) override
        { return m_pNextLayer->AddTrace(traceInfo); }
#else
    virtual Result AddThreadTrace(
        const ThreadTraceInfo& traceInfo) override
        { return m_pNextLayer->AddThreadTrace(traceInfo); }
#endif

    virtual Result AddSpmTrace(
        const SpmTraceCreateInfo& spmInfo) override
        { return m_pNextLayer->AddSpmTrace(spmInfo); }

    virtual Result GetThreadTraceLayout(
        ThreadTraceLayout* pLayout) const override
        { return m_pNextLayer->GetThreadTraceLayout(pLayout); }

    virtual Result GetSpmTraceLayout(
        SpmTraceLayout* pLayout) const override
        { return m_pNextLayer->GetSpmTraceLayout(pLayout); }

    virtual Result Finalize() override
        { return m_pNextLayer->Finalize(); }

    virtual void GetGpuMemoryRequirements(
        GpuMemoryRequirements* pGpuMemReqs) const override
        { m_pNextLayer->GetGpuMemoryRequirements(pGpuMemReqs); }

    virtual Result BindGpuMemory(
        IGpuMemory* pGpuMemory,
        gpusize     offset) override
        { return m_pNextLayer->BindGpuMemory(NextGpuMemory(pGpuMemory), offset); }

    // Part of the IDestroyable public interface.
    virtual void Destroy() override
    {
        IPerfExperiment* pNextLayer = m_pNextLayer;
        this->~PerfExperimentDecorator();
        pNextLayer->Destroy();
    }

    const IDevice*   GetDevice() const { return m_pDevice; }
    IPerfExperiment* GetNextLayer() const { return m_pNextLayer; }

protected:
    virtual ~PerfExperimentDecorator() {}

    IPerfExperiment*const       m_pNextLayer;
    const DeviceDecorator*const m_pDevice;

private:
    PAL_DISALLOW_DEFAULT_CTOR(PerfExperimentDecorator);
    PAL_DISALLOW_COPY_AND_ASSIGN(PerfExperimentDecorator);
};

// =====================================================================================================================
class PipelineDecorator : public IPipeline
{
public:
    PipelineDecorator(IPipeline* pNextPipeline, const DeviceDecorator* pNextDevice)
        :
        m_pNextLayer(pNextPipeline), m_pDevice(pNextDevice)
    {}

    virtual Result GetShaderStats(
        ShaderType   shaderType,
        ShaderStats* pShaderStats,
        bool         getDisassemblySize) const override
        { return m_pNextLayer->GetShaderStats(shaderType, pShaderStats, getDisassemblySize); }

    virtual Result GetShaderCode(ShaderType shaderType, size_t* pSize, void* pBuffer) const override
        { return m_pNextLayer->GetShaderCode(shaderType, pSize, pBuffer); }

    virtual Result GetPerformanceData(Util::Abi::HardwareStage hardwareStage, size_t* pSize, void* pBuffer) override
        { return m_pNextLayer->GetPerformanceData(hardwareStage, pSize, pBuffer); }

    virtual Result QueryAllocationInfo(size_t* pNumEntries, GpuMemSubAllocInfo* const pAllocInfoList) override
        { return m_pNextLayer->QueryAllocationInfo(pNumEntries, pAllocInfoList); }

    virtual const PipelineInfo& GetInfo() const override { return m_pNextLayer->GetInfo(); }

    virtual Util::Abi::ApiHwShaderMapping ApiHwShaderMapping() const override
        { return m_pNextLayer->ApiHwShaderMapping(); }

    // Part of the IDestroyable public interface.
    virtual void Destroy() override
    {
        IPipeline* pNextLayer = m_pNextLayer;
        this->~PipelineDecorator();
        pNextLayer->Destroy();
    }

    const IDevice*  GetDevice() const { return m_pDevice; }
    IPipeline*      GetNextLayer() const { return m_pNextLayer; }

protected:
    virtual ~PipelineDecorator() {}

    IPipeline*const             m_pNextLayer;
    const DeviceDecorator*const m_pDevice;

private:
    PAL_DISALLOW_DEFAULT_CTOR(PipelineDecorator);
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineDecorator);
};

// =====================================================================================================================
class QueryPoolDecorator : public IQueryPool
{
public:
    QueryPoolDecorator(IQueryPool* pNextPool, const DeviceDecorator* pNextDevice)
        :
        m_pNextLayer(pNextPool), m_pDevice(pNextDevice)
    {}

    virtual Result GetResults(
        QueryResultFlags flags,
        QueryType        queryType,
        uint32           startQuery,
        uint32           queryCount,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 371
        const void*      pMappedGpuAddr,
#endif
        size_t*          pDataSize,
        void*            pData,
        size_t           stride) override
        {
            return m_pNextLayer->GetResults(flags,
                                            queryType,
                                            startQuery,
                                            queryCount,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 371
                                            pMappedGpuAddr,
#endif
                                            pDataSize,
                                            pData,
                                            stride);
        }

    virtual Result Reset(
        uint32 startQuery,
        uint32 queryCount) override
        { return m_pNextLayer->Reset(startQuery, queryCount); }

    virtual void GetGpuMemoryRequirements(
        GpuMemoryRequirements* pGpuMemReqs) const override
        { m_pNextLayer->GetGpuMemoryRequirements(pGpuMemReqs); }

    virtual Result BindGpuMemory(
        IGpuMemory* pGpuMemory,
        gpusize     offset) override
        { return m_pNextLayer->BindGpuMemory(NextGpuMemory(pGpuMemory), offset); }

    // Part of the IDestroyable public interface.
    virtual void Destroy() override
    {
        IQueryPool* pNextLayer = m_pNextLayer;
        this->~QueryPoolDecorator();
        pNextLayer->Destroy();
    }

    const IDevice*  GetDevice() const { return m_pDevice; }
    IQueryPool*     GetNextLayer() const { return m_pNextLayer; }

protected:
    virtual ~QueryPoolDecorator() {}

    IQueryPool*const            m_pNextLayer;
    const DeviceDecorator*const m_pDevice;

private:
    PAL_DISALLOW_DEFAULT_CTOR(QueryPoolDecorator);
    PAL_DISALLOW_COPY_AND_ASSIGN(QueryPoolDecorator);
};

// =====================================================================================================================
class QueueDecorator : public IQueue
{
public:
    QueueDecorator(IQueue* pNextQueue, const DeviceDecorator* pDevice)
        :
        m_pNextLayer(pNextQueue), m_pDevice(pDevice)
    {}

    virtual ~QueueDecorator() {}

    virtual Result Submit(
        const SubmitInfo& submitInfo) override;

    virtual Result WaitIdle() override
        { return m_pNextLayer->WaitIdle(); }

    virtual Result SignalQueueSemaphore(
        IQueueSemaphore* pQueueSemaphore) override
        { return m_pNextLayer->SignalQueueSemaphore(NextQueueSemaphore(pQueueSemaphore)); }

    virtual Result WaitQueueSemaphore(
        IQueueSemaphore* pQueueSemaphore) override
        { return m_pNextLayer->WaitQueueSemaphore(NextQueueSemaphore(pQueueSemaphore)); }

    virtual Result PresentDirect(
        const PresentDirectInfo& presentInfo) override;

    virtual Result PresentSwapChain(
        const PresentSwapChainInfo& presentInfo) override;

    virtual Result Delay(
        float delay) override
        { return m_pNextLayer->Delay(delay); }

    virtual Result DelayAfterVsync(
        float delayInUs, const IPrivateScreen* pScreen) override
        { return m_pNextLayer->DelayAfterVsync(delayInUs, NextPrivateScreen(pScreen)); }

    virtual Result RemapVirtualMemoryPages(
        uint32                         rangeCount,
        const VirtualMemoryRemapRange* pRanges,
        bool                           doNotWait,
        IFence*                        pFence) override;

    virtual Result CopyVirtualMemoryPageMappings(
        uint32                                    rangeCount,
        const VirtualMemoryCopyPageMappingsRange* pRanges,
        bool                                      doNotWait) override;

    virtual Result UpdateAppPowerProfile(
        const wchar_t* pFileName,
        const wchar_t* pPathName) override
    { return m_pNextLayer->UpdateAppPowerProfile(pFileName, pPathName); }

    // Part of the IDestroyable public interface.
    virtual void Destroy() override
    {
        IQueue* pNextLayer = m_pNextLayer;
        this->~QueueDecorator();
        pNextLayer->Destroy();
    }

    const IDevice* GetDevice() const { return m_pDevice; }
    IQueue*        GetNextLayer() const { return m_pNextLayer; }

    virtual Result AssociateFenceWithLastSubmit(IFence* pFence) override
    {
        return m_pNextLayer->AssociateFenceWithLastSubmit(NextFence(pFence));
    }

    virtual void SetExecutionPriority(QueuePriority priority) override
        { return m_pNextLayer->SetExecutionPriority(priority); }

    virtual Result QueryAllocationInfo(size_t* pNumEntries, GpuMemSubAllocInfo* const pAllocInfoList) override
        { return m_pNextLayer->QueryAllocationInfo(pNumEntries, pAllocInfoList); }

    virtual QueueType Type() const override { return m_pNextLayer->Type(); }

    virtual EngineType GetEngineType() const override { return m_pNextLayer->GetEngineType(); }

    virtual Result QueryKernelContextInfo(KernelContextInfo* pKernelContextInfo) const override
        { return m_pNextLayer->QueryKernelContextInfo(pKernelContextInfo); }

protected:
    IQueue*                      m_pNextLayer;
    const DeviceDecorator*const  m_pDevice;

private:
    PAL_DISALLOW_DEFAULT_CTOR(QueueDecorator);
    PAL_DISALLOW_COPY_AND_ASSIGN(QueueDecorator);
};

// =====================================================================================================================
// If a layer wishes to subclass QueueSemaphoreDecorator it should also subclass SwapChainDecorator and override the
// NewQueueSemaphoreDecorator function. This is so queue semaphores wrapped by AcquireNextImage use that layer's
// QueueSemaphore type.
class QueueSemaphoreDecorator : public IQueueSemaphore
{
public:
    QueueSemaphoreDecorator(IQueueSemaphore* pNextSemaphore, const DeviceDecorator* pNextDevice)
        :
        m_pNextLayer(pNextSemaphore), m_pDevice(pNextDevice)
    {}

    virtual ~QueueSemaphoreDecorator() {}

    virtual bool HasStalledQueues() override
        { return m_pNextLayer->HasStalledQueues(); }

    virtual OsExternalHandle ExportExternalHandle() const override
        { return m_pNextLayer->ExportExternalHandle(); }

    // Part of the IDestroyable public interface.
    virtual void Destroy() override
    {
        IQueueSemaphore* pNextLayer = m_pNextLayer;
        this->~QueueSemaphoreDecorator();
        pNextLayer->Destroy();
    }

    const IDevice*    GetDevice() const { return m_pDevice; }
    IQueueSemaphore*  GetNextLayer() const { return m_pNextLayer; }

protected:
    IQueueSemaphore*const         m_pNextLayer;
    const DeviceDecorator*const   m_pDevice;

private:
    PAL_DISALLOW_DEFAULT_CTOR(QueueSemaphoreDecorator);
    PAL_DISALLOW_COPY_AND_ASSIGN(QueueSemaphoreDecorator);
};

// =====================================================================================================================
class PrivateScreenDecorator : public IPrivateScreen
{
public:
    PrivateScreenDecorator(IPrivateScreen* pNextScreen, DeviceDecorator* pNextDevice, uint32 index)
        :
        m_pNextLayer(pNextScreen), m_pDevice(pNextDevice), m_index(index), m_pOwner(nullptr)
    {}

    virtual ~PrivateScreenDecorator() {}

    DeviceDecorator* GetDevice() const { return m_pDevice; }
    IPrivateScreen*  GetNextLayer() const { return m_pNextLayer; }
    uint32           GetIndex() const { return m_index; }
    void*            GetOwner() const { return m_pOwner; }

    virtual void BindOwner(void* pOwner) override { m_pOwner = pOwner; }

    virtual Result GetProperties(
        size_t*                  pNumFormats,
        size_t*                  pEdidSize,
        PrivateScreenProperties* pInfo) const override
        { return m_pNextLayer->GetProperties(pNumFormats, pEdidSize, pInfo); }

    virtual Result GetPresentStats(
        PrivateScreenPresentStats* pStats) override { return m_pNextLayer->GetPresentStats(pStats); }

    virtual Result Enable(
        const PrivateScreenEnableInfo& info) override { return m_pNextLayer->Enable(info); }

    virtual Result Disable() override { return m_pNextLayer->Disable(); }

    virtual Result Blank() override { return m_pNextLayer->Blank(); }

    virtual Result Present(
        const PrivateScreenPresentInfo& presentInfo) override
    {
        PrivateScreenPresentInfo nextPresentInfo = presentInfo;
        nextPresentInfo.pSrcImg = NextImage(presentInfo.pSrcImg);
        nextPresentInfo.pPresentDoneFence = NextFence(presentInfo.pPresentDoneFence);
        return m_pNextLayer->Present(nextPresentInfo);
    }

    virtual Result GetScanLine(int32* pScanLine) override { return m_pNextLayer->GetScanLine(pScanLine); }

    virtual Result GetConnectorProperties(
        uint32*                           pDataSize,
        PrivateScreenConnectorProperties* pConnectorProperties) override
        { return m_pNextLayer->GetConnectorProperties(pDataSize, pConnectorProperties); }

    virtual Result GetDisplayMode(
        PrivateDisplayMode* pMode) override { return m_pNextLayer->GetDisplayMode(pMode); }

    virtual Result SetGammaRamp(const GammaRamp* pGammaRamp) override
        { return m_pNextLayer->SetGammaRamp(pGammaRamp); }

    virtual Result SetPowerMode(PrivateDisplayPowerState powerMode) override
        { return m_pNextLayer->SetPowerMode(powerMode); }

    virtual Result SetDisplayMode(const PrivateDisplayMode& displayMode) override
        { return m_pNextLayer->SetDisplayMode(displayMode); }

    virtual Result SetColorMatrix(const ColorTransform& matrix) override
        { return m_pNextLayer->SetColorMatrix(matrix); }

    virtual Result SetEventAfterVsync(OsExternalHandle hEvent, uint32 delayInUs, bool repeated) override
        { return m_pNextLayer->SetEventAfterVsync(hEvent, delayInUs, repeated); }

    virtual Result GetHdcpStatus(HdcpStatus* pStatus) override
        { return m_pNextLayer->GetHdcpStatus(pStatus); }

    virtual Result EnableAudio(bool enable) override
        { return m_pNextLayer->EnableAudio(enable); }

protected:
    IPrivateScreen*       m_pNextLayer;
    DeviceDecorator*const m_pDevice;
    uint32                m_index;      // The index of this private screen in the device decorator's array.
    void*                 m_pOwner;     // The owner data provided by the layer above this one.

private:
    PAL_DISALLOW_DEFAULT_CTOR(PrivateScreenDecorator);
    PAL_DISALLOW_COPY_AND_ASSIGN(PrivateScreenDecorator);
};

// =====================================================================================================================
class SwapChainDecorator : public ISwapChain
{
public:
    SwapChainDecorator(ISwapChain* pNextSwapChain, const DeviceDecorator* pNextDevice)
        :
        m_pNextLayer(pNextSwapChain),
        m_pDevice(pNextDevice)
    {}

    virtual Result AcquireNextImage(
        const AcquireNextImageInfo& acquireInfo,
        uint32*                     pImageIndex) override;

    // Part of the IDestroyable public interface.
    virtual void Destroy() override
    {
        ISwapChain* pNextLayer = m_pNextLayer;
        this->~SwapChainDecorator();
        pNextLayer->Destroy();
    }

    virtual Result WaitIdle() override
        { return m_pNextLayer->WaitIdle(); }

    const IDevice*  GetDevice() const { return m_pDevice; }
    ISwapChain*     GetNextLayer() const { return m_pNextLayer; }

protected:
    virtual ~SwapChainDecorator() {}

    ISwapChain*const            m_pNextLayer;
    const DeviceDecorator*const m_pDevice;

private:

    PAL_DISALLOW_DEFAULT_CTOR(SwapChainDecorator);
    PAL_DISALLOW_COPY_AND_ASSIGN(SwapChainDecorator);
};

} // Pal
