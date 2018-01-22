/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/os/nullDevice/ndPlatform.h"
#include "palSysMemory.h"

#if PAL_BUILD_LAYERS
#include "core/layers/decorators.h"
#endif

#if PAL_BUILD_GPUOPEN
#include "devDriverUtil.h"
#include "devDriverServer.h"
#include "protocols/driverControlServer.h"
#include "protocols/rgpServer.h"
#include "protocols/loggingServer.h"
#endif

using namespace Util;

namespace Pal
{

#if PAL_BUILD_GPUOPEN
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
#endif

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
#if PAL_BUILD_GPUOPEN
    m_pDevDriverServer(nullptr),
    m_pRgpServer(nullptr),
    m_pLoggingServer(nullptr),
    m_pPipelineDumpService(nullptr),
#endif
    m_pfnDeveloperCb(DefaultDeveloperCb),
    m_pClientPrivateData(nullptr),
    m_svmRangeStart(0),
    m_maxSvmSize(createInfo.maxSvmSize),
    m_logCb()
{
    memset(&m_pDevice[0], 0, sizeof(m_pDevice));
    memset(&m_properties, 0, sizeof(m_properties));

    m_flags.u32All = 0;
    m_flags.usesDefaultAllocCb         = (createInfo.pAllocCb == nullptr);
    m_flags.disableGpuTimeout          = createInfo.flags.disableGpuTimeout;
    m_flags.force32BitVaSpace          = createInfo.flags.force32BitVaSpace;
    m_flags.createNullDevice           = createInfo.flags.createNullDevice;
    m_flags.enableSvmMode              = createInfo.flags.enableSvmMode;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 358
    m_flags.requestShadowDescVaRange   = createInfo.flags.requestShadowDescriptorVaRange;
#endif

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 377
    if (createInfo.pLogInfo != nullptr)
    {
        m_logCb = *createInfo.pLogInfo;
    }
#elif PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 368
    if (createInfo.pfnLogCb != nullptr)
    {
        m_logCb.pfnLogCb = createInfo.pfnLogCb;
    }
#endif

    Util::Strncpy(&m_settingsPath[0], createInfo.pSettingsPath, MaxSettingsPathLength);
}

// =====================================================================================================================
Platform::~Platform()
{
#if PAL_BUILD_GPUOPEN
    DestroyDevDriver();
#endif

#if PAL_ENABLE_PRINTS_ASSERTS
    // Unhook the debug print callback to keep assert/alert function (majorly for client driver) after platform get
    // destroyed. Otherwise random crash can be triggered when calling g_dbgPrintCallback with a dangling pointer.
    Util::DbgPrintCallback dbgPrintCallback = {};
    Util::SetDbgPrintCallback(dbgPrintCallback);
#endif

    if (m_flags.usesDefaultAllocCb)
    {
#if PAL_JEMALLOC_ALLOC_CALLBACKS
        DestroyJemallocAllocCallbacks();
#else
        OsDestroyDefaultAllocCallbacks();
#endif
    }
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
        pPlatform = Pal::NullDevice::Platform::CreateInstance(createInfo, allocCb, pPlacementAddr);
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
// client would like to honor.
Result Platform::QueryApplicationProfile(
    const char*         pFilename,
    const char*         pPathname,
    ApplicationProfile* pOut)
{
    PAL_ASSERT((pFilename != nullptr) && (pOut != nullptr));

    Result result = Result::ErrorUnavailable;

    if (m_deviceCount >= 1)
    {
        // NOTE: These application profiles are meant to be interpreted at system-wide scope. We'll only query the
        // first discovered physical GPU under the assumption that all GPU's would return the same profile (or none
        // at all, as the case may be).
        result = m_pDevice[0]->QueryApplicationProfile(pFilename, pPathname, pOut);
    }

    return result;
}

// =====================================================================================================================
// Queries the kernel-mode driver to determine if there is a platform-wide profile for a specific application that the
// client would like to honor. Returned in raw format.
Result Platform::QueryRawApplicationProfile(
    const char*              pFilename,
    const char*              pPathname,
    ApplicationProfileClient client,
    const char**             pOut)
{
    PAL_ASSERT((pFilename != nullptr) && (pOut != nullptr));

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

#if PAL_BUILD_GPUOPEN
    // Perform early initialization of the developer driver after the platform is available.
    if (result == Result::Success)
    {
        EarlyInitDevDriver();
    }
#endif

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

#if PAL_BUILD_GPUOPEN
    // Perform late initialization of the developer driver after devices have been enumerated.
    if (result == Result::Success)
    {
        LateInitDevDriver();
    }
#endif

    if (result == Result::Success)
    {
        result = InitProperties();
    }

    return result;
}

#if PAL_BUILD_GPUOPEN
// =====================================================================================================================
// Initializes a connection with the developer driver message bus if it's currently enabled on the system.
// This function should be called before device enumeration.
void Platform::EarlyInitDevDriver()
{
    DevDriver::TransportType transportType = DevDriver::TransportType::Local;
    bool isConnectionAvailable = DevDriver::DevDriverServer::IsConnectionAvailable(transportType);

    if (isConnectionAvailable)
    {
        static const char* pClientStr = "AMD Vulkan Driver";

        // Configure the developer driver server for driver usage
        DevDriver::DevDriverServerCreateInfo  createInfo  = {};
        createInfo.transportCreateInfo.type               = transportType;
        createInfo.transportCreateInfo.componentType      = DevDriver::Component::Driver;
        createInfo.transportCreateInfo.createUpdateThread = true;

        // Set up developer driver memory allocation callbacks
        createInfo.transportCreateInfo.allocCb.pUserdata  = this;
        createInfo.transportCreateInfo.allocCb.pfnAlloc   = &DevDriverAlloc;
        createInfo.transportCreateInfo.allocCb.pfnFree    = &DevDriverFree;

        // Copy the client string into the description field
        Util::Strncpy(createInfo.transportCreateInfo.clientDescription,
                      pClientStr,
                      sizeof(createInfo.transportCreateInfo.clientDescription));

        // Enable all supported protocols
        createInfo.enabledProtocols.logging       = true;
        createInfo.enabledProtocols.settings      = true;
        createInfo.enabledProtocols.driverControl = true;
        createInfo.enabledProtocols.rgp           = true;

        m_pDevDriverServer = PAL_NEW(DevDriver::DevDriverServer, this, AllocInternal) (createInfo);
        if (m_pDevDriverServer != nullptr)
        {
            bool pipelineDumpsEnabled = false;

            DevDriver::Result devDriverResult = m_pDevDriverServer->Initialize();

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

                // Destroy the developer driver server if developer mode is not enabled.
                if (devDriverResult != DevDriver::Result::Success)
                {
                    m_pDevDriverServer->Destroy();
                }
                else
                {
                    // Check if pipeline dumps are enabled based on the returned status flag.
                    const DevDriver::StatusFlags dumpFlag =
                        static_cast<DevDriver::StatusFlags>(DevDriver::ClientStatusFlags::PipelineDumpsEnabled);
                    pipelineDumpsEnabled = TestAnyFlagSet(filter.status, dumpFlag);
                }
            }
            else
            {
                // Trigger an assert if we fail to initialize the developer driver server.
                PAL_ASSERT_ALWAYS();
            }

            // Free the memory for the developer driver server object if we fail to initialize it completely.
            if (devDriverResult != DevDriver::Result::Success)
            {
                PAL_DELETE(m_pDevDriverServer, this);
                m_pDevDriverServer = nullptr;
            }
            else
            {
                // Cache the pointers for the RGP server and the logging server after initialization.
                m_pRgpServer     = m_pDevDriverServer->GetRGPServer();
                m_pLoggingServer = m_pDevDriverServer->GetLoggingServer();

                // Initialize the logging subsystem if we successfully started the dev driver server.
                m_pLoggingServer->AddCategoryTable(0, static_cast<uint32>(LogCategory::Count), LogCategoryTable);

                // Only create the pipeline dump service if pipeline dumps are enabled by the associated tool.
                if (pipelineDumpsEnabled)
                {
                    m_pPipelineDumpService = PAL_NEW(PipelineDumpService, this, AllocInternal)(this);
                    if (m_pPipelineDumpService != nullptr)
                    {
                        // Attempt to initialize the pipeline dump service.
                        if (m_pPipelineDumpService->Init() == Result::Success)
                        {
                            // If we're successful, register it with the message channel.
                            m_pDevDriverServer->GetMessageChannel()->RegisterService(m_pPipelineDumpService);
                        }
                        else
                        {
                            // If we fail to initialize, destroy the service object.
                            PAL_SAFE_DELETE(m_pPipelineDumpService, this);
                        }
                    }
                }
            }
        }
        else
        {
            // Trigger an assert if we're unable to create the developer driver server due to memory allocation failure.
            PAL_ASSERT_ALWAYS();
        }
    }
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

        // Set up the callbacks for changing the device clock
        DevDriver::DriverControlProtocol::DeviceClockCallbackInfo deviceClockCallbackInfo = {};

        deviceClockCallbackInfo.queryClockCallback = QueryClockCallback;
        deviceClockCallbackInfo.queryMaxClockCallback = QueryMaxClockCallback;
        deviceClockCallbackInfo.setCallback = SetClockModeCallback;
        deviceClockCallbackInfo.pUserdata = this;

        pDriverControlServer->SetNumGpus(m_deviceCount);

        // Set up the device clock callbacks.
        pDriverControlServer->SetDeviceClockCallback(deviceClockCallbackInfo);
    }
}

// =====================================================================================================================
// Destroys the connection to the developer driver message bus if it was previously initialized.
void Platform::DestroyDevDriver()
{
    if (m_pDevDriverServer != nullptr)
    {
        // Null out cached pointers
        m_pRgpServer = nullptr;
        m_pLoggingServer = nullptr;

        if (m_pPipelineDumpService != nullptr)
        {
            PAL_SAFE_DELETE(m_pPipelineDumpService, this);
        }

        m_pDevDriverServer->Destroy();
        PAL_SAFE_DELETE(m_pDevDriverServer, this);
    }
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

#if PAL_BUILD_DBG_OVERLAY
// =====================================================================================================================
bool Platform::IsDebugOverlayEnabled() const
{
    return (m_deviceCount > 0) ? m_pDevice[0]->Settings().debugOverlayEnabled : false;
}
#endif

#if PAL_BUILD_GPU_PROFILER
// =====================================================================================================================
GpuProfilerMode Platform::GpuProfilerMode() const
{
    return (m_deviceCount > 0) ? m_pDevice[0]->Settings().gpuProfilerMode : GpuProfilerDisabled;
}
#endif

#if PAL_BUILD_CMD_BUFFER_LOGGER
// =====================================================================================================================
bool Platform::IsCmdBufferLoggerEnabled() const
{
    return (m_deviceCount > 0) ? m_pDevice[0]->Settings().cmdBufferLoggerEnabled : false;
}
#endif

#if PAL_BUILD_INTERFACE_LOGGER
// =====================================================================================================================
bool Platform::IsInterfaceLoggerEnabled() const
{
    return (m_deviceCount > 0) ? m_pDevice[0]->Settings().interfaceLoggerEnabled : false;
}
#endif

#if PAL_BUILD_GPUOPEN
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
bool Platform::ShowDevDriverOverlay() const
{
    bool showOverlay = false;
    if (m_pDevDriverServer != nullptr)
    {
        // We show the overlay whenever there's a valid dev driver server object (which implies dev mode is enabled).
        // The one exception to this is when an RGP trace is running. In that case, we'll temporarily stop drawing
        // the overlay to prevent it from affecting the trace.
        showOverlay = ((m_pRgpServer == nullptr) || (m_pRgpServer->IsTraceRunning() == false));
    }

    return showOverlay;
}
#endif

// =====================================================================================================================
void Platform::LogMessage(
    LogLevel        level,
    LogCategoryMask categoryMask,
    const char*     pFormat,
    va_list         args)
{
#if PAL_BUILD_GPUOPEN
    if (m_pLoggingServer != nullptr)
    {
        m_pLoggingServer->Log(static_cast<DevDriver::LogLevel>(level),
                              categoryMask,
                              pFormat,
                              args);
    }
#endif

    if (m_logCb.pfnLogCb != nullptr)
    {
        m_logCb.pfnLogCb(
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 377
                         m_logCb.pClientData,
#endif
                         static_cast<uint32>(level),
                         categoryMask,
                         pFormat,
                         args);
    }
}

} // Pal
