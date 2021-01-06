/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/eventProvider.h"
#include "core/g_palSettings.h"
#include "core/g_palPlatformSettings.h"
#include "ver.h"

// DevDriver forward declarations.
namespace DevDriver
{
    class DevDriverServer;

    namespace RGPProtocol
    {
        class RGPServer;
    }

    namespace LoggingProtocol
    {
        class LoggingServer;
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

    virtual Result QueryApplicationProfile(
        const char*         pFilename,
        const char*         pPathname,
        ApplicationProfile* pOut) override;

    virtual Result QueryRawApplicationProfile(
        const char*              pFilename,
        const char*              pPathname,
        Pal::ApplicationProfileClient client,
        const char**             pOut) override;

    virtual Result EnableSppProfile(
        const char*              pFilename,
        const char*              pPathname) override;

    virtual Result GetProperties(
        PlatformProperties* pProperties) override;

    Result ReEnumerateDevices();

    Device* GetDevice(uint32 index) const
    {
        PAL_ASSERT(index < m_deviceCount);
        return m_pDevice[index];
    }

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
        void*                   pData)
        { m_pfnDeveloperCb(m_pClientPrivateData, deviceIndex, type, pData); }

    virtual DevDriver::DevDriverServer* GetDevDriverServer() override { return m_pDevDriverServer; }

    bool IsDeveloperModeEnabled() const { return (m_pDevDriverServer != nullptr); }
    bool IsDevDriverProfilingEnabled() const;
    bool ShowDevDriverOverlay() const;
    bool Force32BitVaSpace() const { return m_flags.force32BitVaSpace; }

    bool SvmModeEnabled()                const { return m_flags.enableSvmMode; }
    bool RequestShadowDescVaRange()      const { return m_flags.requestShadowDescVaRange; }
    bool InternalResidencyOptsDisabled() const { return m_flags.disableInternalResidencyOpts; }

    gpusize GetSvmRangeStart() const { return m_svmRangeStart; }
    void SetSvmRangeStart(gpusize svmRangeStart) { m_svmRangeStart = svmRangeStart; }
    gpusize GetMaxSizeOfSvm() const { return m_maxSvmSize; }

    virtual void LogMessage(LogLevel        level,
                            LogCategoryMask categoryMask,
                            const char*     pFormat,
                            va_list         args) override;

    EventProvider* GetEventProvider() { return &m_eventProvider; }

    virtual void LogEvent(
        PalEvent    eventId,
        const void* pEventData,
        uint32      eventDataSize) override;

    bool OverrideGpuId(GpuId* pGpuId) const;

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
            uint32 reserved                     : 25; // Reserved for future use.
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

    // "Server" object provided by the GpuOpen software component which exposes the main interface for the developer
    // driver functionality. The server object handles all developer driver protocol management internally and exposes
    // interfaces to each protocol through explicit objects which can be retrieved through the main interface.
    DevDriver::DevDriverServer* m_pDevDriverServer;
    PlatformSettingsLoader m_settingsLoader;

    // Locally cached pointers to protocol servers.
    DevDriver::RGPProtocol::RGPServer*         m_pRgpServer;
    DevDriver::LoggingProtocol::LoggingServer* m_pLoggingServer;

    Developer::Callback    m_pfnDeveloperCb;
    void*                  m_pClientPrivateData;
    gpusize                m_svmRangeStart;
    gpusize                m_maxSvmSize;
    Util::LogCallbackInfo  m_logCb;
    EventProvider          m_eventProvider;

    PAL_DISALLOW_COPY_AND_ASSIGN(Platform);
};

} // Pal
