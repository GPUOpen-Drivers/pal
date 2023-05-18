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

#pragma once

#include "palLib.h"
#include "palPlatform.h"
#include "platformSettingsLoader.h"
#include "core/gpuMemoryEventProvider.h"
#include "core/layers/crashAnalysis/crashAnalysisEventProvider.h"
#include "g_coreSettings.h"
#include "g_platformSettings.h"
#include "ver.h"
#include "ddApi.h"

#if PAL_BUILD_RDF
// GpuUtil forward declarations.
namespace GpuUtil
{
class TraceSession;
class AsicInfoTraceSource;
class ApiInfoTraceSource;
class UberTraceService;
class FrameTraceController;
}
#endif

namespace DriverUtilsService
{
class DriverUtilsService;
}

// DevDriver forward declarations.
namespace DevDriver
{
    class DevDriverServer;

    namespace RGPProtocol
    {
        class RGPServer;
    }
}

namespace Pal
{

class CmdStreamChunk;
class Device;
class PipelineDumpService;
class CmdBuffer;

// Structure containing the GPU identifying information
struct GpuId
{
    uint32  familyId;
    uint32  eRevId;
    uint32  revisionId;
    uint32  gfxEngineId;
    uint32  deviceId;
};

/**
 ***********************************************************************************************************************
 * @brief Get the default allocation callback.
 *
 * @param [out] pAllocCb Pointer to the allocation callback structure. Must not be null.
 ***********************************************************************************************************************
 */
extern void PAL_STDCALL GetDefaultAllocCb(
    Util::AllocCallbacks* pAllocCb);

// =====================================================================================================================
// Class which manages global functionality for a particular PAL instantiation.
//
// Platform responsibilities include tracking all supported devices (i.e., Adapters) available in the system,
// abstracting any interface with the OS and kernel-mode driver, and constructing OS-specific objects for other PAL
// components.
class Platform : public IPlatform
{
public:
    virtual ~Platform();

    static size_t GetSize();
    static Result Create(
        const PlatformCreateInfo&   createInfo,
        const Util::AllocCallbacks& allocCb,
        void*                       pPlacementAddr,
        Platform**                  ppPlatform);

    virtual Result EnumerateDevices(
        uint32*    pDeviceCount,
        IDevice*   pDevices[MaxDevices]) override;

    virtual Result GetScreens(
        uint32*  pScreenCount,
        void*    pStorage[MaxScreens],
        IScreen* pScreens[MaxScreens]) override;

    virtual Result QueryRawApplicationProfile(
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 759
        const wchar_t*                pFilename,
        const wchar_t*                pPathname,
#else
        const char*                   pFilename,
        const char*                   pPathname,
#endif
        Pal::ApplicationProfileClient client,
        const char**                  pOut) override;

    virtual Result EnableSppProfile(
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 759
        const wchar_t* pFilename,
        const wchar_t* pPathname
#else
        const char*    pFilename,
        const char*    pPathname
#endif
    ) override;

    virtual Result GetProperties(
        PlatformProperties* pProperties) override;

    Result ReEnumerateDevices();

    Device* GetDevice(uint32 index) const
    {
        PAL_ASSERT(index < m_deviceCount);
        return m_pDevice[index];
    }

#if PAL_BUILD_RDF
    virtual GpuUtil::TraceSession* GetTraceSession() override { return m_pTraceSession; }
    GpuUtil::FrameTraceController* GetFrameTraceController() { return m_pFrameTraceController; }
    void UpdateFrameTraceController(CmdBuffer* pCmdBuffer);
#endif

    uint32       GetDeviceCount()  const { return m_deviceCount; }
    const char*  GetSettingsPath() const { return &m_settingsPath[0]; }
    virtual const PalPlatformSettings& PlatformSettings() const override { return m_settingsLoader.GetSettings(); }
    PalPlatformSettings* PlatformSettingsPtr() { return m_settingsLoader.GetSettingsPtr(); }

    const PlatformProperties& GetProperties() const { return m_properties; }

    bool IsEmulationEnabled() const
    {
        return false;
    }

    void DeveloperCb(
        uint32                  deviceIndex,
        Developer::CallbackType type,
        void*                   pData);

    virtual uint32 GetEnabledCallbackTypes() const override { return m_enabledCallbackTypesMask; }
    virtual void   SetEnabledCallbackTypes(uint32 enabledCallbackTypesMask) override;
    bool           IsSubAllocTrackingEnabled() const { return m_subAllocTrackingEnabled; }

    virtual DevDriver::DevDriverServer* GetDevDriverServer() override { return m_pDevDriverServer; }
    virtual DevDriver::EventProtocol::EventServer* GetEventServer() override { return m_pEventServer; }

    virtual SettingsRpcService::SettingsService* GetSettingsService() override { return m_pSettingsService; }

    bool IsDeveloperModeEnabled() const { return (m_pDevDriverServer != nullptr); }
    bool IsDevDriverProfilingEnabled() const;
    virtual bool IsTracingEnabled() const override;
    virtual bool IsCrashAnalysisModeEnabled() const override;
    bool ShowDevDriverOverlay() const;
    bool Force32BitVaSpace() const { return m_flags.force32BitVaSpace; }

    const char* GetClientApiStr() const;
    ClientApi   GetClientApiId()  const { return m_clientApiId; }

    bool SvmModeEnabled()                const { return m_flags.enableSvmMode; }
    bool RequestShadowDescVaRange()      const { return m_flags.requestShadowDescVaRange; }
    bool InternalResidencyOptsDisabled() const { return m_flags.disableInternalResidencyOpts; }
    bool NullDeviceEnabled()             const { return m_flags.createNullDevice; }
    bool GpuIsSpoofed()                  const { return m_flags.gpuIsSpoofed; }
    bool DontOpenPrimaryNode()           const { return m_flags.dontOpenPrimaryNode; }

    gpusize GetSvmRangeStart() const { return m_svmRangeStart; }
    void SetSvmRangeStart(gpusize svmRangeStart) { m_svmRangeStart = svmRangeStart; }
    gpusize GetMaxSizeOfSvm() const { return m_maxSvmSize; }

    virtual void LogMessage(LogLevel        level,
                            LogCategoryMask categoryMask,
                            const char*     pFormat,
                            va_list         args) override;

    GpuMemoryEventProvider* GetGpuMemoryEventProvider() { return &m_gpuMemoryEventProvider; }
    CrashAnalysisEventProvider* GetCrashAnalysisEventProvider() { return &m_crashAnalysisEventProvider; }

    virtual void LogEvent(
        PalEvent    eventId,
        const void* pEventData,
        uint32      eventDataSize) override;

    bool OverrideGpuId(GpuId* pGpuId);

    const uint16 GetClientApiMajorVer() const { return m_clientApiMajorVer; }
    const uint16 GetClientApiMinorVer() const { return m_clientApiMinorVer; }

#if PAL_ENABLE_LOGGING
    virtual void GetDbgLoggerFileSettings(
        Util::DbgLoggerFileSettings* pSettings) override;
#endif

protected:
    Platform(const PlatformCreateInfo& createInfo, const Util::AllocCallbacks& allocCb);

    virtual Result Init() override;

    virtual Result InitProperties();

    void TearDownDevices();

    // Connects to the host operating system's interface for communicating with the kernel-mode driver.
    virtual Result ConnectToOsInterface() = 0;

    // Queries the host operating system and kernel-mode driver for the set of available devices.
    virtual Result ReQueryDevices() = 0;

    // Queries the host operating system and kernel mode driver for the set of available screens.
    virtual Result ReQueryScreens(
        uint32*  pScreenCount,
        void*    pStorage[MaxScreens],
        IScreen* pScreens[MaxScreens]) = 0;

    // Installs the client event handler for communication between core and layers.
    virtual void InstallDeveloperCb(
        Developer::Callback pfnDeveloperCb,
        void*               pClientData) override
    {
        m_pfnDeveloperCb     = pfnDeveloperCb;
        m_pClientPrivateData = pClientData;
    }

    bool DisableGpuTimeout() const { return m_flags.disableGpuTimeout; }

    Device*            m_pDevice[MaxDevices];
    uint32             m_deviceCount;
    PlatformProperties m_properties;

    const ClientApi m_clientApiId;
    const uint16    m_clientApiMajorVer;
    const uint16    m_clientApiMinorVer;

    static constexpr uint32 MaxSettingsPathLength = 256;
    char m_settingsPath[MaxSettingsPathLength];

    union
    {
        struct
        {
            uint32 disableGpuTimeout            : 1; // Disables GPU timeout detection
            uint32 force32BitVaSpace            : 1; // Forces 32 bit VA space for the flat address in 32 bit ISA
            uint32 createNullDevice             : 1; // If set, creates a NULL device based on the nullGpuId
            uint32 enableSvmMode                : 1; // If set, SVM mode is enabled
            uint32 requestShadowDescVaRange     : 1; // Requests that PAL provides support for the client to use
                                                     // the @ref VaRange::ShadowDescriptorTable virtual-address range.
            uint32 disableInternalResidencyOpts : 1; // Disable residency optimizations for internal GPU memory
                                                     // allocations.
            uint32 supportRgpTraces             : 1; // Indicates that the client supports RGP tracing. PAL will use
                                                     // this flag and the hardware support flag to setup the
                                                     // DevDriver RgpServer.
            uint32 gpuIsSpoofed                 : 1; // If set, ignores device properties reported by OS and pretends
                                                     // to be a given GPU instead.
            uint32 dontOpenPrimaryNode          : 1; // If set, no primary node is needed.
            uint32 disableDevDriver             : 1; // If set, no DevDriverServer will be created.
            uint32 reserved                     : 22; // Reserved for future use.
        };
        uint32 u32All;
    } m_flags;

private:
    // Empty callback for when no installed developer callback exists.
    static void PAL_STDCALL DefaultDeveloperCb(
        void*                   pPrivateData,
        const uint32            deviceIndex,
        Developer::CallbackType type,
        void*                   pCbData) {}

    // OS specific factory method for instantiating the correct type of object for the Platform singleton. Each
    // OS platform should have its own implementation of this memberfunction.
    static Platform* CreateInstance(
        const PlatformCreateInfo&   createInfo,
        const Util::AllocCallbacks& allocCb,
        void*                       pPlacementAddr);

    void InitRuntimeSettings(Device* pDevice);

    // Developer Driver functionality.
    // Initialization + Destruction functions
    Result EarlyInitDevDriver();
    void LateInitDevDriver();
    void DestroyDevDriver();

    void RegisterRpcServices();
    void DestroyRpcServices();

    // "Server" object provided by the DevDriver software component which exposes the main interface for the developer
    // driver functionality. The server object handles all developer driver protocol management internally and exposes
    // interfaces to each protocol through explicit objects which can be retrieved through the main interface.
    DevDriver::DevDriverServer* m_pDevDriverServer;
    DevDriver::EventProtocol::EventServer* m_pEventServer;
    PlatformSettingsLoader m_settingsLoader;

    // Locally cached pointers to protocol servers.
    DevDriver::RGPProtocol::RGPServer* m_pRgpServer;
    // Settings RPC Service for DevDriver tool connection
    SettingsRpcService::SettingsService* m_pSettingsService;

    // Service for misc driver utils
    DriverUtilsService::DriverUtilsService* m_pDriverUtilsService;

#if PAL_BUILD_RDF
    // TraceSession Initialization and Destruction functions
    Result InitTraceSession();
    void DestroyTraceSession();

    // Register the trace controllers in TraceSession
    Result InitTraceControllers();
    Result RegisterTraceControllers();
    void DestroyTraceControllers();

    // Register the "default" trace sources(eg. GpuInfo) on start-up, to reduce burden on clients and
    // provide boilerplate info to Tools
    Result InitDefaultTraceSources();
    Result RegisterDefaultTraceSources();
    void DestroyDefaultTraceSources();

    // Starts the UberTraceService on the driver side
    Result CreateUberTraceService();

    // TraceSession that is centrally owned and managed by PAL
    GpuUtil::TraceSession* m_pTraceSession;

    // Frame trace controller that drives the PAL-owned TraceSession from begin to end
    GpuUtil::FrameTraceController* m_pFrameTraceController;

    GpuUtil::AsicInfoTraceSource* m_pAsicInfoTraceSource; // Trace source that sends device info to PAL-owned TraceSession
    GpuUtil::ApiInfoTraceSource* m_pApiInfoTraceSource;   // Trace source that sends api info to PAL-owned TraceSession

    // UberTraceService that communicates with Tools
    GpuUtil::UberTraceService* m_pUberTraceService;
#endif

    DDRpcServer                m_rpcServer;
    Developer::Callback        m_pfnDeveloperCb;
    void*                      m_pClientPrivateData;
    gpusize                    m_svmRangeStart;
    gpusize                    m_maxSvmSize;
    Util::LogCallbackInfo      m_logCb;
    GpuMemoryEventProvider     m_gpuMemoryEventProvider;
    CrashAnalysisEventProvider m_crashAnalysisEventProvider;
    uint32                     m_enabledCallbackTypesMask;
    bool                       m_subAllocTrackingEnabled;

    PAL_DISALLOW_COPY_AND_ASSIGN(Platform);
};

} // Pal
