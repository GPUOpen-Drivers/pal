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

#include "core/device.h"
#include "core/platform.h"
#include "core/settingsLoader.h"
#include "core/os/nullDevice/ndDevice.h"
#include "core/os/nullDevice/ndPlatform.h"
#include "palAssert.h"
#if PAL_ENABLE_LOGGING
#include "palDbgLogger.h"
#include "palDbgLogMgr.h"
#endif
#include "palDbgPrint.h"
#include "palSysUtil.h"
#include "palSysMemory.h"
#include "core/layers/decorators.h"
#if PAL_AMDGPU_BUILD
#include "core/os/amdgpu/amdgpuHeaders.h"
#else
#include "core/os/wddm/wddmHeaders.h"
#endif

#if PAL_BUILD_RDF
#include "palTraceSession.h"
#include "gpuUtil/asicInfoTraceSource.h"
#include "gpuUtil/apiInfoTraceSource.h"
#include "gpuUtil/uberTraceService.h"
#include "gpuUtil/frameTraceController.h"
#endif

// Dev Driver includes
#include "devDriverUtil.h"
#include "devDriverServer.h"
#include "protocols/driverControlServer.h"
#include "protocols/rgpServer.h"
#include "protocols/ddInfoService.h"
#include "settingsService.h"
#include "driverUtilsService.h"

using namespace Util;

namespace Pal
{

static_assert(static_cast<uint32>(LogLevel::Debug)   == static_cast<uint32>(DevDriver::LogLevel::Debug),
              "DevDriver::LogLevel enum mismatch!");
static_assert(static_cast<uint32>(LogLevel::Verbose) == static_cast<uint32>(DevDriver::LogLevel::Verbose),
              "DevDriver::LogLevel enum mismatch!");
static_assert(static_cast<uint32>(LogLevel::Info)    == static_cast<uint32>(DevDriver::LogLevel::Info),
              "DevDriver::LogLevel enum mismatch!");
static_assert(static_cast<uint32>(LogLevel::Alert)   == static_cast<uint32>(DevDriver::LogLevel::Alert),
              "DevDriver::LogLevel enum mismatch!");
static_assert(static_cast<uint32>(LogLevel::Error)   == static_cast<uint32>(DevDriver::LogLevel::Error),
              "DevDriver::LogLevel enum mismatch!");
static_assert(static_cast<uint32>(LogLevel::Always)  == static_cast<uint32>(DevDriver::LogLevel::Always),
              "DevDriver::LogLevel enum mismatch!");

#if PAL_ENABLE_PRINTS_ASSERTS
// =====================================================================================================================
// Callback function used to route debug prints into the logging protocol.
void PAL_STDCALL DbgPrintCb(
    void*                  pUserdata,
    Util::DbgPrintCategory category,
    const char*            pText)
{
    IPlatform* pPlatform = static_cast<IPlatform*>(pUserdata);

    // Convert the debug print category into a log level.
    constexpr LogLevel LogLevelLookup[DbgPrintCatCount] =
    {
        LogLevel::Info,
        LogLevel::Alert,
        LogLevel::Error,
        LogLevel::Info
    };

    pPlatform->LogMessage(LogLevelLookup[category], LogCategoryMaskInternal, "%s", pText);
}
#endif

// =====================================================================================================================
Platform::Platform(
    const PlatformCreateInfo& createInfo,
    const AllocCallbacks&     allocCb)
    :
    Pal::IPlatform(allocCb),
    m_deviceCount(0),
#if  (PAL_CLIENT_INTERFACE_MAJOR_VERSION>= 734)
    m_clientApiId(createInfo.clientApiId),
#else
    m_clientApiId(ClientApi::Vulkan),
#endif
    m_clientApiMajorVer(createInfo.apiMajorVer),
    m_clientApiMinorVer(createInfo.apiMinorVer),
    m_pDevDriverServer(nullptr),
    m_pSettingsService(nullptr),
    m_pDriverUtilsService(nullptr),
    m_pEventServer(nullptr),
    m_settingsLoader(this),
    m_pRgpServer(nullptr),
#if PAL_BUILD_RDF
    m_pTraceSession(nullptr),
    m_pFrameTraceController(nullptr),
    m_pAsicInfoTraceSource(nullptr),
    m_pApiInfoTraceSource(nullptr),
    m_pUberTraceService(nullptr),
#endif
    m_rpcServer(DD_API_INVALID_HANDLE),
    m_pfnDeveloperCb(DefaultDeveloperCb),
    m_pClientPrivateData(nullptr),
    m_svmRangeStart(0),
    m_maxSvmSize(createInfo.maxSvmSize),
    m_logCb(),
    m_gpuMemoryEventProvider(this),
    m_crashAnalysisEventProvider(this),
    m_enabledCallbackTypesMask(Developer::DefaultEnabledCallbackTypes),
    m_subAllocTrackingEnabled(false)
{
    memset(&m_pDevice[0], 0, sizeof(m_pDevice));
    memset(&m_properties, 0, sizeof(m_properties));

    m_flags.u32All = 0;
    m_flags.disableGpuTimeout            = createInfo.flags.disableGpuTimeout;
    m_flags.force32BitVaSpace            = createInfo.flags.force32BitVaSpace;
    m_flags.createNullDevice             = createInfo.flags.createNullDevice;
    m_flags.enableSvmMode                = createInfo.flags.enableSvmMode;
    m_flags.requestShadowDescVaRange     = createInfo.flags.requestShadowDescriptorVaRange;
    m_flags.disableInternalResidencyOpts = createInfo.flags.disableInternalResidencyOpts;
    m_flags.supportRgpTraces             = createInfo.flags.supportRgpTraces;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 754
    m_flags.dontOpenPrimaryNode          = createInfo.flags.dontOpenPrimaryNode;
#endif
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 765
    m_flags.disableDevDriver              = createInfo.flags.disableDevDriver;
#endif

    if (createInfo.pLogInfo != nullptr)
    {
        m_logCb = *createInfo.pLogInfo;
    }

    Util::Strncpy(&m_settingsPath[0], createInfo.pSettingsPath, MaxSettingsPathLength);
}

// =====================================================================================================================
Platform::~Platform()
{
    DestroyDevDriver();
#if PAL_BUILD_RDF
    DestroyDefaultTraceSources();
    DestroyTraceControllers();
    DestroyTraceSession();
#endif

#if PAL_ENABLE_PRINTS_ASSERTS
    // Unhook the debug print callback to keep assert/alert function (majorly for client driver) after platform get
    // destroyed. Otherwise random crash can be triggered when calling g_dbgPrintCallback with a dangling pointer.
    Util::DbgPrintCallback dbgPrintCallback = {};
    Util::SetDbgPrintCallback(dbgPrintCallback);
#endif
}

// =====================================================================================================================
// Creates and initializes the Platform singleton instance. This may result in additional DLL's being loaded (for
// obtaining pointers to OS thunks on Windows, etc.) so it is very unsafe to call this from within a client driver's
// DllMain() function on Windows.
//
// This function is not re-entrant!
Result Platform::Create(
    const PlatformCreateInfo& createInfo,
    const AllocCallbacks&     allocCb,
    void*                     pPlacementAddr,
    Platform**                ppPlatform)
{
    Result    result    = Result::ErrorInitializationFailed;
    Platform* pPlatform = nullptr;

    // Create either a "null" device (good for off-line shader compilation and not much else) or a real device.
    if (createInfo.flags.createNullDevice)
    {
#if PAL_BUILD_NULL_DEVICE
        pPlatform = Pal::NullDevice::Platform::CreateInstance(createInfo, allocCb, pPlacementAddr);
#else
        // Why are you trying to do this?
        PAL_ASSERT_ALWAYS();
#endif
    }
    else
    {
        pPlatform = CreateInstance(createInfo, allocCb, pPlacementAddr);
    }

    if (pPlatform != nullptr)
    {
        result = pPlatform->Init();
    }

    if (result == Result::Success)
    {
        (*ppPlatform) = pPlatform;
    }
    else if (pPlatform != nullptr)
    {
        pPlatform->Destroy();
    }

    return result;
}

// =====================================================================================================================
// Returns a count and list of devices attached to the system.  If this function is called more than once, then it will
// also cleanup any device objects enumerated on the previous call, a sequence expected when the client is returned an
// ErrorDeviceLost error from any PAL function.  See the public interface documentation for more detail.
Result Platform::EnumerateDevices(
    uint32*  pDeviceCount,
    IDevice* pDevices[MaxDevices])
{
    PAL_ASSERT((pDeviceCount != nullptr) && (pDevices != nullptr));

    Result result = ReEnumerateDevices();
    if (result == Result::Success)
    {
        *pDeviceCount = m_deviceCount;
        for (uint32 gpu = 0; gpu < *pDeviceCount; ++gpu)
        {
            pDevices[gpu] = m_pDevice[gpu];
        }

        // We need to internally query the screen topology so that each Device will know whether or not screen(s) are
        // available. This affects which presentation techniques are selected by any Queues created for the Device.
        uint32 dummyScreenCount = 0;
        result = ReQueryScreens(&dummyScreenCount, nullptr, nullptr);
    }

    return result;
}

// =====================================================================================================================
// Retrieves the list of available screens.  This function queries a set of IScreen objects corresponding to the screens
// attached to the system. The caller owns any returned IScreens.
Result Platform::GetScreens(
    uint32*  pScreenCount,
    void*    pStorage[MaxScreens],
    IScreen* pScreens[MaxScreens])
{
    PAL_ASSERT(pScreenCount != nullptr);

    Result result = Result::ErrorUnavailable;

    if (m_deviceCount >= 1)
    {
        result = ReQueryScreens(pScreenCount, pStorage, pScreens);
    }

    return result;
}

// =====================================================================================================================
// Queries the kernel-mode driver to determine if there is a platform-wide profile for a specific application that the
// client would like to honor. Returned in raw format.
Result Platform::QueryRawApplicationProfile(
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 759
    const wchar_t*           pFilename,
    const wchar_t*           pPathname,
#else
    const char*              pFilenameChar,
    const char*              pPathnameChar,
#endif
    ApplicationProfileClient client,
    const char**             pOut)
{

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 759
    PAL_ASSERT((pFilename != nullptr) && (pOut != nullptr));
#else
    PAL_ASSERT((pFilenameChar != nullptr) && (pOut != nullptr));
    PAL_ASSERT(strlen(pFilenameChar) < Util::MaxFileNameStrLen);

    wchar_t pFilename[Util::MaxFileNameStrLen];
    wchar_t pPathname[MaxPathStrLen];
    // KMD expects wchars
    Mbstowcs(pFilename, pFilenameChar, ArrayLen(pFilename));
    if (pPathnameChar != nullptr)
    {
        PAL_ASSERT(strlen(pPathnameChar) < MaxPathStrLen);
        Mbstowcs(pPathname, pPathnameChar, ArrayLen(pPathname));
    }
    else
    {
        // Null-terminate the string.
        pPathname[0] = '\0';
    }
#endif

    Result result = Result::ErrorUnavailable;

    if (m_deviceCount >= 1)
    {
        // NOTE: These application profiles are meant to be interpreted at system-wide scope. We'll only query the
        // first discovered physical GPU under the assumption that all GPU's would return the same profile (or none
        // at all, as the case may be).
        result = m_pDevice[0]->QueryRawApplicationProfile(pFilename, pPathname, client, pOut);
    }

    return result;
}

// =====================================================================================================================
// Queries the kernel-mode driver to determine if there is a platform-wide spp profile for a specific application that
// the client would like to honor.
Result Platform::EnableSppProfile(
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 759
    const wchar_t* pFilename,
    const wchar_t* pPathname
#else
    const char*    pFilenameChar,
    const char*    pPathnameChar
#endif
)
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 759
    PAL_ASSERT(pFilename != nullptr);
#else
    PAL_ASSERT(pFilenameChar != nullptr);
    PAL_ASSERT(strlen(pFilenameChar) < Util::MaxFileNameStrLen);

    wchar_t pFilename[Util::MaxFileNameStrLen];
    wchar_t pPathname[Util::MaxPathStrLen];
    // KMD expects wchars
    Mbstowcs(pFilename, pFilenameChar, ArrayLen(pFilename));
    if (pPathnameChar != nullptr)
    {
        PAL_ASSERT(strlen(pPathnameChar) < Util::MaxPathStrLen);
        Mbstowcs(pPathname, pPathnameChar, ArrayLen(pPathname));
    }
#endif

    Result result = Result::ErrorUnavailable;

    if (m_deviceCount >= 1)
    {
        // NOTE: The spp profiles are meant to be interpreted at system-wide scope. We'll only query the
        // first discovered physical GPU under the assumption that all GPU's would return the same profile (or none
        // at all, as the case may be).
        result = m_pDevice[0]->EnableSppProfile(pFilename, pPathname);
    }

    return result;
}

// =====================================================================================================================
Result Platform::GetProperties(
    PlatformProperties* pProperties)
{
    Result result = Result::ErrorInvalidPointer;

    if (pProperties != nullptr)
    {
        // Copy our pre-baked properties struct.
        memcpy(pProperties, &m_properties, sizeof(m_properties));

        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Helper method which destroys all previously enumerated devices.
void Platform::TearDownDevices()
{
    for (uint32 gpu = 0; gpu < m_deviceCount; ++gpu)
    {
        const Result result = m_pDevice[gpu]->Cleanup();
        PAL_ASSERT(result == Result::Success);

        m_pDevice[gpu]->~Device();
        PAL_SAFE_FREE(m_pDevice[gpu], this);
    }
    m_deviceCount = 0;
}

// =====================================================================================================================
// Initializes the platform singleton's connection to the host operating system and kernel-mode driver.
//
// This function is not re-entrant!
Result Platform::Init()
{
    Result result = IPlatform::Init();

#if PAL_BUILD_RDF
    // Safer to initialize the TraceSession before EarlyInitDevDriver(), since the session's JSON-based config may be
    // updated during that method
    if (result == Result::Success)
    {
        result = InitTraceSession();
    }
#endif

    // Perform early initialization of the developer driver after the platform is available.
    if (result == Result::Success)
    {
        result = EarlyInitDevDriver();
    }

#if PAL_ENABLE_PRINTS_ASSERTS
    // Set the debug print callback to make debug prints visible over the logging protocol.
    Util::DbgPrintCallback dbgPrintCallback = {};
    dbgPrintCallback.pCallbackFunc = &DbgPrintCb;
    dbgPrintCallback.pUserdata = this;
    Util::SetDbgPrintCallback(dbgPrintCallback);
#endif

    if (result == Result::Success)
    {
        result = ConnectToOsInterface();
    }

    if (result == Result::Success)
    {
        result = ReEnumerateDevices();
    }

    // Perform late initialization of the developer driver after devices have been enumerated.
    if (result == Result::Success)
    {
        LateInitDevDriver();
    }

    if (result == Result::Success)
    {
        result = InitProperties();
    }

#if PAL_BUILD_RDF
    if (result == Result::Success)
    {
        result = InitTraceControllers();
    }

    if (result == Result::Success)
    {
        result = RegisterTraceControllers();
    }

    if (result == Result::Success)
    {
        result = InitDefaultTraceSources();
    }

    if (result == Result::Success)
    {
        result = RegisterDefaultTraceSources();
    }
#endif

    return result;
}

// =====================================================================================================================
// Optionally overrides the GPU ID for a single device.  This can be initiated through the panel settings for some build
// configurations.  This MUST BE called after EarlyInitDevDriver() !!
bool Platform::OverrideGpuId(
    GpuId* pGpuId) // in,out: GPU ID information to potentially override.
{
    bool overridden = false;

    const PalPlatformSettings& settings = PlatformSettings();

#if PAL_BUILD_NULL_DEVICE
    if (strcmp(settings.spoofNullGpuIfh, "") != 0)
    {
        NullDevice::NullIdLookup foundGpu = NullDevice::Device::GetDeviceByName(settings.spoofNullGpuIfh);
        if (foundGpu.nullId != NullGpuId::Max)
        {
            pGpuId->gfxEngineId = foundGpu.gfxEngineId;
            pGpuId->familyId    = foundGpu.familyId;
            pGpuId->revisionId  = foundGpu.revisionId;
            pGpuId->eRevId      = foundGpu.eRevId;
            pGpuId->deviceId    = foundGpu.deviceId;

            overridden = true;
        }
        else
        {
            PAL_ALERT_ALWAYS_MSG("NullGpu name '%s' not found!", settings.spoofNullGpuIfh);
        }
    }
#endif

    if (overridden)
    {
        m_flags.gpuIsSpoofed = 1;
    }

    return overridden;
}

// =====================================================================================================================
// Initializes a connection with the developer driver message bus if it's currently enabled on the system.
// This function should be called before device enumeration.
Result Platform::EarlyInitDevDriver()
{
    DevDriver::HostInfo hostInfo = DevDriver::kDefaultNamedPipe;

    bool isConnectionAvailable = (m_flags.disableDevDriver == false);
    if (isConnectionAvailable)
    {
        isConnectionAvailable = DevDriver::DevDriverServer::IsConnectionAvailable(hostInfo);

    }

    DevDriver::Result devDriverResult = DevDriver::Result::Success;
    if (isConnectionAvailable)
    {
        const char*const pClientStr = GetClientApiStr();

        // Configure the developer driver server for driver usage
        DevDriver::ServerCreateInfo  createInfo = {};
        createInfo.connectionInfo = hostInfo;
        createInfo.componentType = DevDriver::Component::Driver;
        createInfo.createUpdateThread = true;

        // Set up developer driver memory allocation callbacks
        DevDriver::AllocCb allocCb = {};
        allocCb.pUserdata = this;
        allocCb.pfnAlloc = &DevDriverAlloc;
        allocCb.pfnFree = &DevDriverFree;

        // Copy the client string into the description field
        Util::Strncpy(createInfo.clientDescription, pClientStr, sizeof(createInfo.clientDescription));

        // Enable all supported protocols
        createInfo.servers.settings      = true;
        createInfo.servers.driverControl = true;
        createInfo.servers.rgp           = true;
        createInfo.servers.event         = true;

        m_pDevDriverServer = PAL_NEW(DevDriver::DevDriverServer, this, AllocInternal) (allocCb, createInfo);
        if (m_pDevDriverServer != nullptr)
        {
            devDriverResult = m_pDevDriverServer->Initialize();

            if (devDriverResult == DevDriver::Result::Success)
            {
                // We successfully initialized the message bus. Check if developer mode is enabled by attempting
                // to locate a tool on the bus that has the developer mode enabled status flag set.

                DevDriver::IMsgChannel* pMsgChannel = m_pDevDriverServer->GetMessageChannel();

                DevDriver::ClientId clientId = DevDriver::kBroadcastClientId;
                DevDriver::ClientMetadata filter = {};
                filter.clientType = DevDriver::Component::Tool;
                filter.status =
                    static_cast<DevDriver::StatusFlags>(DevDriver::ClientStatusFlags::DeveloperModeEnabled);

                devDriverResult = pMsgChannel->FindFirstClient(filter,
                                                               &clientId,
                                                               DevDriver::kFindClientTimeout,
                                                               &filter);
            }
            else
            {
                // Trigger an assert if we fail to initialize the developer driver server.
                PAL_ASSERT_ALWAYS();
            }

            // Free the memory for the developer driver server object if we fail to initialize it completely.
            if (devDriverResult != DevDriver::Result::Success)
            {
                PAL_SAFE_DELETE(m_pDevDriverServer, this);
            }
            else // Initialize the event server and our services if we have a valid DevDriver server.
            {
                m_pEventServer = m_pDevDriverServer->GetEventServer();

#if PAL_ENABLE_RPC_SETTINGS
                DevDriver::AllocCb allocCb = {};
                allocCb.pUserdata = this;
                allocCb.pfnAlloc = &DevDriverAlloc;
                allocCb.pfnFree = &DevDriverFree;

                m_pSettingsService = PAL_NEW(SettingsRpcService::SettingsService, this, AllocInternal)(allocCb);
                PAL_ASSERT(m_pSettingsService != nullptr);
#endif

                m_pDriverUtilsService = PAL_NEW(DriverUtilsService::DriverUtilsService, this, AllocInternal)(this);
            }
        }
        else
        {
            // Trigger an assert if we're unable to create the developer driver server due to memory allocation failure.
            PAL_ASSERT_ALWAYS();
        }
    }

    // Initialize Platform settings
    Result result = m_settingsLoader.Init();

    if ((result == Result::Success) && (m_pDevDriverServer != nullptr))
    {
        // Initialize the event providers if we have a valid connection
        result = m_gpuMemoryEventProvider.Init();

        if (result == Result::Success)
        {
            result = m_crashAnalysisEventProvider.Init();
        }

        if (result == Result::Success)
        {
#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION < GPUOPEN_DRIVER_CONTROL_CLEANUP_VERSION
            m_pDevDriverServer->StartDeviceInit();
#else
            DevDriver::DriverControlProtocol::DriverControlServer* pDriverControlServer =
                m_pDevDriverServer->GetDriverControlServer();

            PAL_ASSERT(pDriverControlServer != nullptr);

#if PAL_BUILD_RDF
            CreateUberTraceService();
#endif
            RegisterRpcServices();

            pDriverControlServer->StartEarlyDeviceInit();
#endif
            if (pDriverControlServer->IsDriverIgnored() == false)
            {
                // Cache the pointer for the RGP server after successful initialization.
                m_pRgpServer = m_pDevDriverServer->GetRGPServer();
            }
            else
            {
                // The tools have indicated that they do not wish to communicate with this driver
                // so we can safely destroy all of the previously initialized DevDriver infrastructure.
                m_gpuMemoryEventProvider.Destroy();
                m_crashAnalysisEventProvider.Destroy();

#if PAL_BUILD_RDF
                DestroyRpcServices();
#endif

                PAL_SAFE_DELETE(m_pDevDriverServer, this);
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Finishes any initialization of the developer driver that requires the devices to be initialized first.
// This function should be called after device enumeration.
void Platform::LateInitDevDriver()
{
    // Late init only needs to be performed if we actually set up the developer driver object earlier.
    if (m_pDevDriverServer != nullptr)
    {
        DevDriver::DriverControlProtocol::DriverControlServer* pDriverControlServer =
            m_pDevDriverServer->GetDriverControlServer();

        // If the developer driver server is initialized successfully and we requested a specific protocol
        // during initialization, the associated object should always be valid.
        PAL_ASSERT(pDriverControlServer != nullptr);

        // Set up the callbacks for changing the device clock.
#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION < GPUOPEN_DRIVER_CONTROL_QUERY_CLOCKS_BY_MODE_VERSION
        DevDriver::DriverControlProtocol::DeviceClockCallbackInfo deviceClockCallbackInfo = {};

        deviceClockCallbackInfo.queryClockCallback = QueryClockCallback;
        deviceClockCallbackInfo.queryMaxClockCallback = QueryMaxClockCallback;
        deviceClockCallbackInfo.setCallback = SetClockModeCallback;
        deviceClockCallbackInfo.pUserdata = this;
#else
        DevDriver::DriverControlProtocol::DeviceClockCallbackInfo deviceClockCallbackInfo = {};

        deviceClockCallbackInfo.queryClockCallback = QueryClockCallback;
        deviceClockCallbackInfo.setCallback = SetClockModeCallback;
        deviceClockCallbackInfo.pUserdata = this;
#endif

        // This is a callback that can be used for any pal information as we add more support for more
        // information. Currently it is only used for perf experiment information.
        DevDriver::InfoURIService::InfoService::InfoSource palInfoSource = {};

        palInfoSource.name             = "pal";
        palInfoSource.version          = 1;
        palInfoSource.pUserdata        = this;
        palInfoSource.pfnWriteCallback = &PalCallback;

        m_pDevDriverServer->GetInfoService()->RegisterInfoSource(palInfoSource);

        pDriverControlServer->SetNumGpus(m_deviceCount);

        // Set up the device clock callbacks.
        pDriverControlServer->SetDeviceClockCallback(deviceClockCallbackInfo);
    }

    // Now that we have some valid devices we can look for settings overrides in the registry/settings file.
    // Note, we don't really care if this is the device that will actually be used for rendering, we just
    // need a device object for the OS specific ReadSetting function.
    if (m_deviceCount >= 1)
    {
        m_settingsLoader.ReadSettings(m_pDevice[0]);
    }

    // And then before finishing init we have an opportunity to override the settings default values based on
    // runtime info
    m_settingsLoader.OverrideDefaults();
    m_settingsLoader.ValidateSettings(); // Also, validate them.

#if PAL_ENABLE_LOGGING
    // Configure debug log manager as soon as settings are overridden.
    g_dbgLogMgr.SetLoggingEnabled(PlatformSettings().dbgLogEnabled);
#endif

    // Late init only needs to be performed if we actually set up the developer driver object earlier.
#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION >= GPUOPEN_DRIVER_CONTROL_CLEANUP_VERSION
    if (m_pDevDriverServer != nullptr)
    {
        if (m_deviceCount >= 1)
        {
            Pal::DeviceProperties deviceProperties = {};

            m_pDevice[0]->GetProperties(&deviceProperties);

            // If both the client and the hardware support rgp traces, attempt to enable them on the rgp server.
            if ((m_flags.supportRgpTraces) && (deviceProperties.gfxipProperties.flags.supportRgpTraces))
            {
                // Enable the trace functionality on the RGP Server.
                m_pDevDriverServer->GetRGPServer()->EnableTraces();
            }
        }

        DevDriver::DriverControlProtocol::DriverControlServer* pDriverControlServer =
            m_pDevDriverServer->GetDriverControlServer();

        PAL_ASSERT(pDriverControlServer != nullptr);
    }
#endif
}

// =====================================================================================================================
// Destroys the connection to the developer driver message bus if it was previously initialized.
void Platform::DestroyDevDriver()
{
    if (m_pDevDriverServer != nullptr)
    {
        DestroyRpcServices();

        m_gpuMemoryEventProvider.Destroy();
        m_crashAnalysisEventProvider.Destroy();

        // Null out cached pointers
        m_pRgpServer   = nullptr;
        m_pEventServer = nullptr;

        m_pDevDriverServer->Destroy();
        PAL_SAFE_DELETE(m_pDevDriverServer, this);
    }
}

#if PAL_BUILD_RDF
// =====================================================================================================================
// Creates and initializes a TraceSession that's centrally owned and managed by PAL
Result Platform::InitTraceSession()
{
    Result result = Result::ErrorOutOfMemory;

    m_pTraceSession = PAL_NEW(GpuUtil::TraceSession, this, AllocInternal) (this);

    if (m_pTraceSession != nullptr)
    {
        result = m_pTraceSession->Init();
    }

    return result;
}

// =====================================================================================================================
// Destroys the PAL-owned TraceSession
void Platform::DestroyTraceSession()
{
    if (m_pTraceSession != nullptr)
    {
        PAL_SAFE_DELETE(m_pTraceSession, this);
    }
}

// =====================================================================================================================
Result Platform::InitTraceControllers()
{
    Result result = Result::Success;
    m_pFrameTraceController = PAL_NEW(GpuUtil::FrameTraceController, this, AllocInternal) (this);

    if (m_pFrameTraceController == nullptr)
    {
        result = Result::ErrorOutOfMemory;
    }

    return result;
}

// =====================================================================================================================
Result Platform::RegisterTraceControllers()
{
    Result result = m_pTraceSession->RegisterController(m_pFrameTraceController);
    return result;
}

// =====================================================================================================================
void Platform::UpdateFrameTraceController(
    CmdBuffer* pCmdBuffer)
{
    m_pFrameTraceController->UpdateFrame(pCmdBuffer);
}

// =====================================================================================================================
void Platform::DestroyTraceControllers()
{
    if (m_pFrameTraceController != nullptr)
    {
        PAL_SAFE_DELETE(m_pFrameTraceController, this);
    }
}

// =====================================================================================================================
Result Platform::InitDefaultTraceSources()
{
    Result result = Result::Success;
    m_pAsicInfoTraceSource = PAL_NEW(GpuUtil::AsicInfoTraceSource, this, AllocInternal) (this);
    m_pApiInfoTraceSource  = PAL_NEW(GpuUtil::ApiInfoTraceSource, this, AllocInternal) (this);

    if ((m_pAsicInfoTraceSource == nullptr) || (m_pApiInfoTraceSource == nullptr))
    {
        result = Result::ErrorOutOfMemory;
    }

    return result;
}

// =====================================================================================================================
// Register the default trace sources(eg. GpuInfo) on start-up
Result Platform::RegisterDefaultTraceSources()
{
    Result result = m_pTraceSession->RegisterSource(m_pAsicInfoTraceSource);

    if (Util::IsErrorResult(result) == false)
    {
        result = m_pTraceSession->RegisterSource(m_pApiInfoTraceSource);
    }

    return result;
}

// =====================================================================================================================
// Destroy the default trace sources
void Platform::DestroyDefaultTraceSources()
{
    if (m_pAsicInfoTraceSource != nullptr)
    {
        PAL_SAFE_DELETE(m_pAsicInfoTraceSource, this);
    }
    if (m_pApiInfoTraceSource != nullptr)
    {
        PAL_SAFE_DELETE(m_pApiInfoTraceSource, this);
    }
}

// =====================================================================================================================
// Creates a UberTraceService that forwards network requests into TraceSession
Result Platform::CreateUberTraceService()
{
    Result result = Result::Success;

    m_pUberTraceService = PAL_NEW(GpuUtil::UberTraceService, this, AllocInternal) (this);

    if (m_pUberTraceService == nullptr)
    {
        result = Result::ErrorOutOfMemory;
    }

    return result;
}
#endif

// =====================================================================================================================
void Platform::RegisterRpcServices()
{
    DD_RESULT devDriverResult = DD_RESULT_SUCCESS;

    DevDriver::IMsgChannel* pMsgChannel = m_pDevDriverServer->GetMessageChannel();

    DDRpcServerCreateInfo rpcServerInfo = {};
    rpcServerInfo.hConnection = reinterpret_cast<DDNetConnection>(pMsgChannel);

    // Create the rpc server with above parameters
    devDriverResult = ddRpcServerCreate(&rpcServerInfo, &m_rpcServer);

    if (devDriverResult == DD_RESULT_SUCCESS)
    {
#if PAL_BUILD_RDF
        if (m_pUberTraceService != nullptr)
        {
            devDriverResult = UberTrace::RegisterService(m_rpcServer, m_pUberTraceService);
            PAL_ASSERT(devDriverResult == DD_RESULT_SUCCESS);
        }
#endif

        if (m_pSettingsService != nullptr)
        {
            devDriverResult = SettingsRpc::RegisterService(m_rpcServer, m_pSettingsService);
            PAL_ASSERT(devDriverResult == DD_RESULT_SUCCESS);
        }

        if (m_pDriverUtilsService != nullptr)
        {
            devDriverResult = DriverUtils::RegisterService(m_rpcServer, m_pDriverUtilsService);
            PAL_ASSERT(devDriverResult == DD_RESULT_SUCCESS);
        }

        pMsgChannel->RegisterProtocolServer(m_pDevDriverServer->GetDriverControlServer());
    }
}

// =====================================================================================================================
void Platform::DestroyRpcServices()
{
#if PAL_BUILD_RDF
    if (m_pUberTraceService != nullptr)
    {
        PAL_SAFE_DELETE(m_pUberTraceService, this);
    }
#endif
    if (m_pSettingsService != nullptr)
    {
        PAL_SAFE_DELETE(m_pSettingsService, this);
    }

    if (m_pDriverUtilsService != nullptr)
    {
        PAL_SAFE_DELETE(m_pDriverUtilsService, this);
    }

    if (m_rpcServer != DD_API_INVALID_HANDLE)
    {
        ddRpcServerDestroy(m_rpcServer);
        m_rpcServer = DD_API_INVALID_HANDLE;
    }
}

// =====================================================================================================================
// Forwards event logging calls to the event provider.
void Platform::LogEvent(
    PalEvent    eventId,
    const void* pEventData,
    uint32      eventDataSize)
{
    static_assert(static_cast<uint32>(PalEvent::Count) == 17, "Write support for new event!");

    PAL_ASSERT(pEventData != nullptr);

    switch(eventId)
    {
    case PalEvent::CreateGpuMemory:
    case PalEvent::DestroyGpuMemory:
    case PalEvent::GpuMemoryCpuMap:
    case PalEvent::GpuMemoryCpuUnmap:
    case PalEvent::GpuMemoryAddReference:
    case PalEvent::GpuMemoryRemoveReference:
        // These functions are not currently supported/expected through the PAL interface
        PAL_ASSERT_ALWAYS();
        break;
    case PalEvent::GpuMemoryResourceBind:
        PAL_ASSERT(eventDataSize == sizeof(GpuMemoryResourceBindEventData));
        m_gpuMemoryEventProvider.LogGpuMemoryResourceBindEvent(*(static_cast<const GpuMemoryResourceBindEventData*>(pEventData)));
        break;
    case PalEvent::GpuMemoryResourceCreate:
        PAL_ASSERT(eventDataSize == sizeof(ResourceCreateEventData));
        m_gpuMemoryEventProvider.LogGpuMemoryResourceCreateEvent(*(static_cast<const ResourceCreateEventData*>(pEventData)));
        break;
    case PalEvent::GpuMemoryResourceDestroy:
        PAL_ASSERT(eventDataSize == sizeof(ResourceDestroyEventData));
        m_gpuMemoryEventProvider.LogGpuMemoryResourceDestroyEvent(*(static_cast<const ResourceDestroyEventData*>(pEventData)));
        break;
    case PalEvent::GpuMemoryMisc:
        PAL_ASSERT(eventDataSize == sizeof(MiscEventData));
        m_gpuMemoryEventProvider.LogGpuMemoryMiscEvent(*(static_cast<const MiscEventData*>(pEventData)));
        break;
    case PalEvent::GpuMemorySnapshot:
        PAL_ASSERT(eventDataSize == sizeof(GpuMemorySnapshotEventData));
        m_gpuMemoryEventProvider.LogGpuMemorySnapshotEvent(*(static_cast<const GpuMemorySnapshotEventData*>(pEventData)));
        break;
    case PalEvent::DebugName:
        PAL_ASSERT(eventDataSize == sizeof(DebugNameEventData));
        m_gpuMemoryEventProvider.LogDebugNameEvent(*(static_cast<const DebugNameEventData*>(pEventData)));
        break;
    case PalEvent::ResourceCorrelation:
        PAL_ASSERT(eventDataSize == sizeof(ResourceCorrelationEventData));
        m_gpuMemoryEventProvider.LogResourceCorrelationEvent(*(static_cast<const ResourceCorrelationEventData*>(pEventData)));
        break;
    case PalEvent::ResourceInfoUpdate:
        PAL_ASSERT(eventDataSize == sizeof(ResourceUpdateEventData));
        m_gpuMemoryEventProvider.LogResourceUpdateEvent(*(static_cast<const ResourceUpdateEventData*>(pEventData)));
        break;
    default:
        PAL_ASSERT_ALWAYS_MSG("Unhandled PalEvent type");
    }
}

#if PAL_ENABLE_LOGGING
// =====================================================================================================================
// Copy PalPlatformSettings.dbgLoggerFileConfig fields into 'settings'
void Platform::GetDbgLoggerFileSettings(
    Util::DbgLoggerFileSettings* pSettings)
{
    const PalPlatformSettings& platformSettings = PlatformSettings();
    pSettings->pLogDirectory     = platformSettings.dbgLoggerFileConfig.logDirectory;
    pSettings->fileSettingsFlags = platformSettings.dbgLoggerFileConfig.fileSettingsFlags;
    pSettings->fileAccessFlags   = platformSettings.dbgLoggerFileConfig.fileAccessFlags;
    pSettings->origTypeMask      = platformSettings.dbgLoggerFileConfig.origTypeMask;
    pSettings->severityLevel     = static_cast<Util::SeverityLevel>(platformSettings.dbgLoggerFileConfig.severityLevel);
}
#endif

// =====================================================================================================================
// Initializes the platform's properties structure. Assume that the constructor zeroed the properties and fill out all
// os-independent properties.
Result Platform::InitProperties()
{
    // PAL version number defined in res/ver.h.
    m_properties.palVersion.major = PAL_VERSION_NUMBER_MAJOR;
    m_properties.palVersion.minor = PAL_VERSION_NUMBER_MINOR;

    return Result::Success;
}

// =====================================================================================================================
// Queries the operating system for the set of available devices. This call may be made more than once, because clients
// will call it again when recovering from a Device-lost error. To handle this, we need to tear down all devices
// which had been enumerated during the previous call (if any exist).
//
// This function is not re-entrant!
Result Platform::ReEnumerateDevices()
{
    TearDownDevices();

    Result result = ReQueryDevices();

    if (result != Result::Success)
    {
        TearDownDevices();
    }
    return result;
}

// =====================================================================================================================
void Platform::DeveloperCb(
    uint32                  deviceIndex,
    Developer::CallbackType type,
    void*                   pData)
{
    if (BitfieldIsSet(m_enabledCallbackTypesMask, static_cast<uint32>(type)))
    {
        m_pfnDeveloperCb(m_pClientPrivateData, deviceIndex, type, pData);
    }
}

// =====================================================================================================================
void Platform::SetEnabledCallbackTypes(uint32 enabledCallbackTypesMask)
{
    m_enabledCallbackTypesMask = enabledCallbackTypesMask;

    if (BitfieldIsSet(m_enabledCallbackTypesMask, static_cast<uint32>(Developer::CallbackType::SubAllocGpuMemory)) &&
        BitfieldIsSet(m_enabledCallbackTypesMask, static_cast<uint32>(Developer::CallbackType::SubFreeGpuMemory)))
    {
        m_subAllocTrackingEnabled = true;
    }
}

// =====================================================================================================================
bool Platform::IsDevDriverProfilingEnabled() const
{
    bool isProfilingEnabled = false;

    if (m_pRgpServer != nullptr)
    {
        isProfilingEnabled = m_pRgpServer->TracesEnabled();
    }

    return isProfilingEnabled;
}

// =====================================================================================================================
bool Platform::IsTracingEnabled() const
{
    bool isTracingEnabled = false;

    if (m_pDriverUtilsService != nullptr)
    {
        isTracingEnabled = m_pDriverUtilsService->IsTracingEnabled();
    }

    // To support legacy behavior, check if RGP tracing is enabled:
    isTracingEnabled |= IsDevDriverProfilingEnabled();

    return isTracingEnabled;
}

// =====================================================================================================================
bool Platform::IsCrashAnalysisModeEnabled() const
{
    bool isCrashAnalysisModeEnabled = false;

    return isCrashAnalysisModeEnabled;
}

// =====================================================================================================================
bool Platform::ShowDevDriverOverlay() const
{
    bool showOverlay = false;
    if (m_pDevDriverServer != nullptr)
    {
        showOverlay = m_pDevDriverServer->ShouldShowOverlay();
    }

    return showOverlay;
}

// =====================================================================================================================
void Platform::LogMessage(
    LogLevel        level,
    LogCategoryMask categoryMask,
    const char*     pFormat,
    va_list         args)
{
    if (m_logCb.pfnLogCb != nullptr)
    {
        m_logCb.pfnLogCb(m_logCb.pClientData,
                         static_cast<uint32>(level),
                         categoryMask,
                         pFormat,
                         args);
    }
}

// =====================================================================================================================
const char* Platform::GetClientApiStr() const
{
    const char* pStr = "Invalid Driver";

    switch(m_clientApiId)
    {
    case ClientApi::Pal:
        pStr = "AMD PAL";
        break;
    case ClientApi::Dx9:
        pStr = "AMD DirectX9 Driver";
        break;
    case ClientApi::Dx12:
        pStr = "AMD DirectX12 Driver";
        break;
    case ClientApi::Vulkan:
        pStr = "AMD Vulkan Driver";
        break;
    case ClientApi::Mantle:
        pStr = "AMD Mantle Driver";
        break;
    case ClientApi::OpenCl:
        pStr = "AMD OpenCL Driver";
        break;
    case ClientApi::Hip:
        pStr = "AMD HIP Driver";
        break;
    }

    return pStr;
}

} // Pal
