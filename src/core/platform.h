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

#pragma once

#include "palLib.h"
#include "palPlatform.h"
#include "core/g_palSettings.h"
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

    const PlatformProperties& GetProperties() const { return m_properties; }

#if PAL_BUILD_DBG_OVERLAY
    bool IsDebugOverlayEnabled() const;
#endif
#if PAL_BUILD_GPU_PROFILER
    GpuProfilerMode GpuProfilerMode() const;
#endif
#if PAL_BUILD_CMD_BUFFER_LOGGER
    bool IsCmdBufferLoggerEnabled() const;
#endif
#if PAL_BUILD_INTERFACE_LOGGER
    bool IsInterfaceLoggerEnabled() const;
#endif
    virtual bool IsViceEnabled()      const { return false; }
    virtual bool IsDtifEnabled()      const { return false; }
            bool IsEmulationEnabled() const { return IsViceEnabled() || IsDtifEnabled(); }

    void DeveloperCb(
        uint32                  deviceIndex,
        Developer::CallbackType type,
        void*                   pData)
        { m_pfnDeveloperCb(m_pClientPrivateData, deviceIndex, type, pData); }

#if PAL_BUILD_GPUOPEN
    DevDriver::DevDriverServer* GetDevDriverServer() { return m_pDevDriverServer; }

    bool IsDeveloperModeEnabled() const { return (m_pDevDriverServer != nullptr); }
    bool IsDevDriverProfilingEnabled() const;
    bool ShowDevDriverOverlay() const;
    PipelineDumpService* GetPipelineDumpService() { return m_pPipelineDumpService; }
#else
    bool IsDeveloperModeEnabled() const { return false; }
    bool IsDevDriverProfilingEnabled() const { return false; }
    bool ShowDevDriverOverlay() const { return false; }
#endif

    bool Force32BitVaSpace() const { return m_flags.force32BitVaSpace; }

    bool SvmModeEnabled()     const { return m_flags.enableSvmMode; }
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 358
    bool RequestShadowDescVaRange() const { return m_flags.requestShadowDescVaRange; }
#endif

    gpusize GetSvmRangeStart() const { return m_svmRangeStart; }
    void SetSvmRangeStart(gpusize svmRangeStart) { m_svmRangeStart = svmRangeStart; }
    gpusize GetMaxSizeOfSvm() const { return m_maxSvmSize; }

    virtual void LogMessage(LogLevel        level,
                            LogCategoryMask categoryMask,
                            const char*     pFormat,
                            va_list         args) override;

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

#if PAL_BUILD_GPUOPEN
    // Developer Driver functionality.

    // Initialization + Destruction functions
    void EarlyInitDevDriver();
    void LateInitDevDriver();
    void DestroyDevDriver();

    // "Server" object provided by the GpuOpen software component which exposes the main interface for the developer
    // driver functionality. The server object handles all developer driver protocol management internally and exposes
    // interfaces to each protocol through explicit objects which can be retrieved through the main interface.
    DevDriver::DevDriverServer* m_pDevDriverServer;

    // Locally cached pointers to protocol servers.
    DevDriver::RGPProtocol::RGPServer*         m_pRgpServer;
    DevDriver::LoggingProtocol::LoggingServer* m_pLoggingServer;

    // Pipeline dump service exposed via the developer driver.
    PipelineDumpService* m_pPipelineDumpService;
#endif

    Developer::Callback    m_pfnDeveloperCb;
    void*                  m_pClientPrivateData;
    gpusize                m_svmRangeStart;
    gpusize                m_maxSvmSize;
    Util::LogCallbackFunc  m_pfnLogCb;

    union
    {
        struct
        {
            uint32 usesDefaultAllocCb         :  1;     // We need to track this so that we can properly clean up the
                                                        // default OS-specific allocation callbacks if they are in use.
            uint32 disableGpuTimeout          :  1;     // Disables GPU timeout detection
            uint32 force32BitVaSpace          :  1;     // Forces 32 bit VA space for the flat address in 32 bit ISA
            uint32 createNullDevice           :  1;     // If set, creates a NULL device based on the nullGpuId
            uint32 enableSvmMode              :  1;     // If set, SVM mode is enabled
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 358
            uint32 requestShadowDescVaRange   :  1;     // Requests that PAL provides support for the client to use
                                                        // the @ref VaRange::ShadowDescriptorTable virtual-address range.
            uint32 reserved                   : 26;
#else
            uint32 reserved                   : 27;
#endif
        };
        uint32 u32All;
    } m_flags;

    PAL_DISALLOW_COPY_AND_ASSIGN(Platform);
};

} // Pal
