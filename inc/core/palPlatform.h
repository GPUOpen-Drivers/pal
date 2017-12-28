/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
/**
 ***********************************************************************************************************************
 * @file  palPlatform.h
 * @brief Defines the Platform Abstraction Library (PAL) IPlatform interface and related types.
 ***********************************************************************************************************************
 */

#pragma once

#include "pal.h"
#include "palSysMemory.h"
#include "palMemTrackerImpl.h"
#include "palDestroyable.h"
#include "palDeveloperHooks.h"

#if PAL_BUILD_GPUOPEN
// DevDriver forward declarations.
namespace DevDriver
{
    class DevDriverServer;
}
#endif

namespace Pal
{

// Forward declarations.
class IDevice;
class IScreen;

/// Maximum number of Devices possibly attached to a system.
constexpr uint32 MaxDevices = 16;
/// Maximum number of Screens possibly attached to a Device.
constexpr uint32 MaxScreensPerDevice = 6;
/// Maximum number of Screens possibly attached to a system.
constexpr uint32 MaxScreens = (MaxScreensPerDevice * MaxDevices);

/// 32-bit PAL version identifier.
///
/// Version number of the PAL library.  Major version is bumped on every promotion from stg/pal to stg/pal_prm.  Minor
/// version is bumped when a change is cherry-picked to stg/pal_prm.
///
/// @see PlatformProperties::palVersion
struct Version
{
    uint16 major;  ///< Major version number.
    uint16 minor;  ///< Minor version number.
};

/// Reports capabilities and general properties of this instantiation of the PAL library.
///
/// This covers any property that it platform-wide as opposed to being tied to a particular device in the system.
///
/// @see IPlatform::GetProperties
struct PlatformProperties
{
    Version palVersion;  ///< Version number of the PAL library.  Note that this is distinct from the interface version.
                         ///  It will be regularly updated as described in @ref Version.
    union
    {
        struct
        {
            uint32 supportNonSwapChainPresents :  1; ///< If set, non-swapchain presents are supported.
            uint32 supportBlockIfFlipping      :  1; ///< If set, IQueue::Submit can protect against command buffers
                                                     ///  that write to GPU memory queued for a flip present.
            uint32 explicitPresentModes        :  1; ///< If set, the PresentMode enums specified during direct and swap
                                                     ///  chain presents explicitly determine the presentation method.
                                                     ///  Additionally, the client must enumerate IScreens and use them
                                                     ///  to explicitly manage fullscreen ownership. Otherwise, the
                                                     ///  present modes are suggestions and fullscreen ownership is
                                                     ///  managed internally by PAL.
            uint32 reserved                    : 29; ///< Reserved for future use.
        };
        uint32 u32All;                               ///< Flags packed as 32-bit uint.
    };
};

/// Enumerates the GPU affinity modes which can be selected by an application profile. This determines the preference
/// of which Device(s) an application profile would like us to use for a specific application.
///
/// @see IPlatform::QueryApplocationProfile
enum class PxGpuAffinity : uint32
{
    Default = 0,      ///< No GPU affinity preference exists
    PowerSaving,      ///< Prefer running on a low-power device (such as an APU)
    HighPerformance,  ///< Prefer running on a high-performance device (such as the dGPU in a PX configuration).
    Global,           ///< Prefer running on whichever device is selected by the current dynamic power state.
};

/// Enumerates the dynamic power state of a PX configuration. This can determine the preference of which Devices(s) an
/// application runs on when the GPU affinity is 'Global'.
///
/// @see IPlatform::QueryApplicationProfile
enum class PxPowerState : uint32
{
    Unknown = 0,         ///< Power state is unknown due to a profile not being present or the configuration not
                         ///  being Power Express.
    PowerSaving,         ///< Prefer battery life savings over raw performance
    Balanced,            ///< Prefer neither battery life savings nor raw performance
    HighPerformance,     ///< Prefer raw performance over battery life savings
    ExtremePerformance,  ///< Strongly prefer raw performance over battery life savings
};

/// Reports information contained in an application-specific profile.
///
/// This covers Power Express specific information like GPU affinity for a specific title, etc.
///
/// @see IPlatform::QueryApplicationProfile()
struct ApplicationProfile
{
    PxGpuAffinity  affinity;    ///< Power Express GPU affinity mode requested by the application profile
    PxPowerState   powerState;  ///< Global Power Express power state. This is used in combination with the profile's
                                ///  GPU affinity to determine which GPU's should be utilized by an application.
};

/// The client that Pal may query profile for. the order is the same as SHARED_AP_AREA in KMD escape interface
enum class ApplicationProfileClient : uint32
{
    Uninitialized = 0,
    Dxx,
    Udx,
    Cfx,
    Ogl,
    Px,
    PxDynamic,
    User3D,
    Ocl,
    Mmd,
    Pplib,
    Dal,
    Chill,
};

/// Describes a primary surface view
///
/// @see IPlatform::GetPrimaryLayout()
struct PrimaryViewInfo
{
    Rect    rect;                       ///< Rectangle defining one portion of a primary surface layout.
    uint32  numIndices;                 ///< The size of the gpuIndex array.
    uint32  gpuIndex[MaxDevices];       ///< The devices in a linked adapter chain that can use this view.
};

/// Specifies output arguments for IPlatform::GetPrimaryLayout(), returning information about the layout of the primary
/// surface.
///
/// @see IPlatform::GetPrimaryLayout()
struct GetPrimaryLayoutOutput
{
    uint32                numViews;         ///< The number of views in the pViewInfoList array.
    PrimaryViewInfo*      pViewInfoList;    ///< The primary surface is composed of these views.
    union
    {
        struct
        {
            uint32 disablePartialCopy : 1;  ///< If this flag is not set, the client can transfer the specific views of
                                            ///  primary surface to peer GPUs. Otherwise, the client must transfer the
                                            ///  whole primary surface to peer GPUs.
            uint32 reserved           : 31; ///< Reserved for future use.
        };
        uint32 u32All;  ///< Flags packed as 32-bit uint.
    } flags;            ///< specifies primary surface layout flags.
};

/// Specifies TurboSync control mode
enum class TurboSyncControlMode : uint32
{
    Disable           = 0,      ///< Disable TurboSync
    Enable            = 1,      ///< Enable TurboSync
    UpdateAllocations = 2,      ///< Update allocations only, without disable or enable TurboSync
    Register          = 3,      ///< Register the current platform as TurboSync requested platform, doesn't actually
                                ///  activate TurboSync.
    Count
};

#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 337) // For TurboSync 2.0 mGPU support

constexpr uint32 TurboSyncMaxSurfaces = 2; ///< Specifies maximum number of surfaces in a private TurboSync swapchain

/// Input argument for IPlatform::TurboSyncControl. TurboSync is a feature that enables app to render at higher than
/// V-Sync frame rates while still being tearing-free. It creates a private swapchain and copy application's back
/// buffer to the primary in this private swapchain when application is flipping. KMD controls the flipping of the
/// private swapchain to screen.
struct TurboSyncControlInput
{
    TurboSyncControlMode mode;          ///< Specifies the TurboSync control mode
    uint32               vidPnSourceId; ///< The vidPnSourceId the call is targeted

    /// GpuMemory of the primaries in private swapchain, per-gpu. This is indexed by the device indices enumerated by
    /// the platform. Pal forwards the allocation handles (if IGpuMemory ptr is not null) to Kmd without validation.
    const IGpuMemory*    pPrimaryMemoryArray[MaxDevices][TurboSyncMaxSurfaces];
};

#endif

/**
************************************************************************************************************************
* @interface IPlatform
* @brief     Interface representing an client-configurable context of the PAL platform.
*
* This is the root of all client interaction with PAL. Each IPlatform contains a set of the IDevice's and IScreens
* found in the system.
*
* + Creation of IDevice and IScreen objects.
* + Installation of memory management callbacks.
* + Query application profiles from the system.
************************************************************************************************************************
*/
class IPlatform : public IDestroyable
{
public:
    /// Enumerates a list of available Devices.
    ///
    /// This function creates a set of @ref IDevice objects corresponding to the devices attached to the system.
    /// CreatePlatform() must be called before this function is called.
    ///
    /// This function may be called multiple times during the lifetime of the PAL lib, in which case all previous
    /// @ref IDevice and @ref IScreen objects are automatically destroyed.  The client is responsible for
    /// destroying all objects attached to the existing @ref IDevice objects before re-calling this function.
    /// Re-enumerating Devices is required if ErrorDeviceLost is ever returned by PAL, as this may indicate a device
    /// has been physically removed from the system.
    ///
    /// @note Before IPlatform::Destroy can be called, all devices returned by IPlatform::EnumerateDevices() must be
    ///       destroyed.
    ///
    /// @param [out] pDeviceCount Specifies the number of devices available in the system.  This is the number of valid
    ///                           entries in pDevices[].  Must not be null.
    /// @param [out] pDevices     Array to be populated with a device object pointer for each device available in the
    ///                           system. The first *pDeviceCount entries are valid.  Must not be null.
    ///
    /// @returns Success if all Devices were successfully enumerated in pDevices[].  Otherwise, one of the following
    ///          error codes may be returned:
    ///          + ErrorInitializationFailed will be returned if PAL is unable to query the available Devices.
    virtual Result EnumerateDevices(
        uint32*    pDeviceCount,
        IDevice*   pDevices[MaxDevices]) = 0;

    /// Returns the storage size of the object implementing IScreen.
    ///
    /// Use this to determine the size of each pStorage pointer passed to GetScreens.
    ///
    /// @returns the storage size in bytes of the object implementing IScreen.
    virtual size_t GetScreenObjectSize() const = 0;

    /// Retrieves the list of available screens.
    ///
    /// This function queries a set of @ref IScreen objects corresponding to the screens attached to the system.
    /// CreatePlatform() and IPlatform::EnumerateDevices() must be called before this function is called.
    ///
    /// This function may be called multiple times during the lifetime of the PAL lib. Each call returns a new
    /// set of screen objects.
    ///
    /// @ingroup LibInit
    ///
    /// @param [out] pScreenCount Specifies the number of screens available in the system.  This is the number of valid
    ///                           entries in pScreens[] and pStorage[].  Must not be null.
    /// @param [in]  pStorage     Array of caller-allocated storage for the screen objects. Each must be the size
    ///                           returned by GetScreenObjectSize. Must always pre-allocate MaxScreens worth, must
    ///                           not be NULL nor may any entry be NULL.
    /// @param [out] pScreens     Array to be populated with a screen pointer for each screen available in the system.
    ///                           The first *pScreenCount entries are valid.  Must not be null.
    ///
    /// @note pScreens[i] uses the storage from pStorage[i]. pStorage[i] is unused for i >= *pScreenCount.
    ///
    /// @returns Success if all screens were successfully retrieved in pScreens[].  Otherwise, one of the following
    ///          error codes may be returned:
    ///          + ErrorUnavailable if this was called prior to IPlatform::EnumerateDevices().
    virtual Result GetScreens(
        uint32*  pScreenCount,
        void*    pStorage[MaxScreens],
        IScreen* pScreens[MaxScreens]) = 0;

    /// Queries an application-specific profile for Power Express configurations.
    ///
    /// This function queries the kernel-mode driver to determine if there is a platform-wide profile for a specific
    /// application that the client would like to honor. It is optional, and doesn't need to be called if the client
    /// does not wish to support application profiles.
    ///
    /// This function may be called multiple times with different filename and pathname strings. This is useful because
    /// a particular application may be referenced by one or more identifier strings that the client wishes to query.
    ///
    /// @ingroup LibInit
    ///
    /// @param [in]  pFilename Filename of the application to query for its profile.
    /// @param [in]  pPathname Optional. Allows the caller to specify a pathname in addition to a filename if they wish.
    /// @param [out] pOut      Will be filled with the application profile parameters if the profile exists and was
    ///                        successfully queried.
    ///
    /// @returns Success if the application profile exists for the specified string(s) and the profile was successfully
    ///          retrieved, or Unsupported if the profile does not exist (or for non-PX configurations) and the query
    ///          was successfully performed. In this way, a client does not need to know whether or not their
    ///          configuration is PX before issuing this call. Otherwise, one of the following error codes may be
    ///          returned:
    ///          + ErrorInvalidPointer will be returned if pFilename or pOut is null.
    ///          + ErrorUnavailable if this is called before IPlatform::EnumerateDevices(), or if there were no Devices
    ///            discovered.
    virtual Result QueryApplicationProfile(
        const char*         pFilename,
        const char*         pPathname,
        ApplicationProfile* pOut) = 0;

    /// Queries a client specified application profile in raw format.
    ///
    /// This function queries the kernel-mode driver to determine if there is a platform-wide profile for a specific
    /// application that the client would like to honor. It is optional, and doesn't need to be called if the client
    /// does not wish to support application profiles.
    ///
    /// As the format of profile is client specified, the profile will be returned in raw format and client has the
    /// responsibility to parse the profile. @see GpuUtil::ProfileIterator provides a basic capability to iterate all
    /// properties in the raw data packet. The memory storing the raw data is managed by Pal.
    ///
    /// The pFilename string can be the EXE name, like "doom.exe", or the "Content Distribution Network" (CDN) ID,
    /// like "SteamAppId:570".  You can use the function GpuUtil::QueryAppContentDistributionId() to get the CDN ID.
    ///
    /// @ingroup LibInit
    ///
    /// @param [in]  pFilename Filename of the application or the Steam/EA/UPlay game ID to query for its profile.
    ///                        See GpuUtil::QueryAppContentDistributionId().
    /// @param [in]  pPathname Optional. Allows the caller to specify a pathname in addition to a filename if they wish.
    /// @param [in]  client    Client name that KMD will query the profile for
    /// @param [out] pOut      Will be filled with the application profile string if the profile exists and was
    ///                        successfully queried.
    ///
    /// @returns Success if the application profile exists for the specified string(s) and the profile was successfully
    ///          retrieved, or Unsupported if the profile does not exist and the query was successfully performed.
    ///          Otherwise, one of the following error codes may be returned:
    ///          + ErrorInvalidPointer will be returned if pFilename or pOut is null.
    ///          + ErrorUnavailable if this is called before IPlatform::EnumerateDevices(), or if there were no Devices
    ///            discovered.
    virtual Result QueryRawApplicationProfile(
        const char*              pFilename,
        const char*              pPathname,
        ApplicationProfileClient client,
        const char**             pOut) = 0;

    /// Reports the properties of the platform.
    ///
    /// Returns the capabilities and general properties of this platform instantiation.
    ///
    /// @param [out] pProperties Capabilities and general properties of this platform instantiation (not tied to a
    ///                          particular device).
    ///
    /// @returns Success if the properties were successfully queried and returned in pProperties.  Otherwise, one of the
    ///          following errors may be returned:
    ///          + ErrorInvalidPointer if pProperties is null.
    virtual Result GetProperties(
        PlatformProperties* pProperties) = 0;

    /// Installs the callback into the specified platform.
    ///
    /// @param [in] pPlatform        The platform to install the callback into.
    /// @param [in] pfnDeveloperCb   The developer callback function pointer to be executed by the pPlatform.
    /// @param [in] pPrivateData     Private data that is installed with the callback for use by the installer.
    static PAL_INLINE void InstallDeveloperCb(
        IPlatform*          pPlatform,
        Developer::Callback pfnDeveloperCb,
        void*               pPrivateData)
        { pPlatform->InstallDeveloperCb(pfnDeveloperCb, pPrivateData); };

#if PAL_BUILD_GPUOPEN
    /// Returns a pointer to the developer driver server object if developer mode is enabled on the system.
    ///
    /// @returns A valid DevDriver::DevDriverServer pointer if developer mode is enabled. If developer mode is not
    ///          enabled, nullptr will be returned.
    virtual DevDriver::DevDriverServer* GetDevDriverServer() = 0;
#endif

    /// Get primary surface layout based upon VidPnSource provided by client.
    ///
    /// This function is used by client to query the layout of the primary surface. The layout describes how primary
    /// surface is composed with a set of views. Each view provides the rectangle of the surface area and the GPUs
    /// this surface area will be displayed on.
    /// Client should make first call pass in pPrimaryLayoutOutput->pViewInfoList as NULL to query the number of views
    /// this primary surface has.
    /// Client then based on pPrimaryLayoutOutput->numViews, allocates the buffer for pViewInfoList. And client then
    /// makes the escape call again to query the actual view information.
    ///
    /// @param [in]      vidPnSourceId          VidPnSource ID that's associated to a primary surface.
    /// @param [in, out] pPrimaryLayoutOutput   Primary surface layout output arguments.
    ///
    /// @returns Success if the display layout on given vidPnSourceId was successfully queried.
    ///          Otherwise, one of the following errors may be returned:
    ///          + ErrorInvalidValue if pPrimaryLayoutOutput is invalid.
    ///          + ErrorUnavailable if no implementation on current platform.
    ///          + ErrorOutOfMemory if there is not enough system memory.
    virtual Result GetPrimaryLayout(
        uint32                  vidPnSourceId,
        GetPrimaryLayoutOutput* pPrimaryLayoutOutput) = 0;

#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 337) // For TurboSync 2.0 mGPU support
    /// Calls TurboSyncControl escape to control TurboSync on specific vidPnSourceId.
    ///
    /// The function is called when clients intend to toggle TurboSync on a vidPnSourceId. The client should allocate
    /// private swapchain primary surfaces that's compatible with the application swapchain primaries. When used to
    /// activate TurboSync, the private primaries' handles needs to be passed in the TurboSyncControlInput data.
    ///
    /// @param [in] turboSyncControlInput  TurboSyncControl input arguments. See TurboSyncControlInput.
    ///
    /// @returns Success if the TurboSyncControl request is handled successfully.
    virtual Result TurboSyncControl(
        const TurboSyncControlInput& turboSyncControlInput) = 0;
#endif

    /// Returns the value of the associated arbitrary client data pointer.
    /// Can be used to associate arbitrary data with a particular PAL object.
    ///
    /// @returns Pointer to client data.
    PAL_INLINE void* GetClientData() const
    {
        return m_pClientData;
    }

    /// Sets the value of the associated arbitrary client data pointer.
    /// Can be used to associate arbitrary data with a particular PAL object.
    ///
    /// @param  [in]    pClientData     A pointer to arbitrary client data.
    PAL_INLINE void SetClientData(
        void* pClientData)
    {
        m_pClientData = pClientData;
    }

    /// Allocates memory using the platform's ForwardAllocator.
    ///
    /// @param [in] allocInfo @see Util::AllocInfo
    ///
    /// @returns Pointer to the allocated memory on success, nullptr on failure.
    void* Alloc(const Util::AllocInfo& allocInfo)
    {
#if PAL_MEMTRACK
        return m_memTracker.Alloc(allocInfo);
#else
        return m_allocator.Alloc(allocInfo);
#endif
    }

    /// Frees memory using the platform's ForwardAllocator.
    ///
    /// @param [in] freeInfo @see Util::FreeInfo
    void  Free(const Util::FreeInfo& freeInfo)
    {
#if PAL_MEMTRACK
        m_memTracker.Free(freeInfo);
#else
        m_allocator.Free(freeInfo);
#endif
    }

    /// Logs a text string via the developer driver bus if it is currently connected.
    ///
    /// @param [in] level        Log priority level associated with the message.
    /// @param [in] categoryMask Log category mask that represents what category fields the message relates to.
    /// @param [in] pFormat      Format string for the log message.
    /// @param [in] args         Variable arguments that correspond to the format string.
    virtual void LogMessage(LogLevel        level,
                            LogCategoryMask categoryMask,
                            const char*     pFormat,
                            va_list         args) = 0;

    /// Logs a text string via the developer driver bus if it is currently connected.
    ///
    /// @param [in] level        Log priority level associated with the message.
    /// @param [in] categoryMask Log category mask that represents what category fields the message relates to.
    /// @param [in] pFormat      Format string for the log message.
    /// @param [in] ...          Variable arguments that correspond to the format string.
    void LogMessage(LogLevel        level,
                    LogCategoryMask categoryMask,
                    const char*     pFormat,
                                    ...)
    {
        va_list args;
        va_start(args, pFormat);
        LogMessage(level, categoryMask, pFormat, args);
        va_end(args);
    }

protected:
    /// @internal Constructor. Prevent use of new operator on this interface. Client must create objects by explicitly
    /// called the proper create method.
    IPlatform(
        const Util::AllocCallbacks& allocCb)
        :
#if PAL_MEMTRACK
        m_memTracker(&m_allocator),
#endif
        m_allocator(allocCb),
        m_pClientData(nullptr) { }

    /// @internal Destructor. Prevent use of delete operator on this interface.  Client must destroy objects by
    /// explicitly calling IDestroyable::Destroy() and is responsible for freeing the system memory allocated for the
    /// object on their own.
    virtual ~IPlatform() { }

    /// @internal Initialization common to all platforms; must be called in subclass overrides of this function.
    /// Currently only handles initialization of the memory leak tracker.
    virtual Result Init()
    {
#if PAL_MEMTRACK
        return m_memTracker.Init();
#else
        return Result::Success;
#endif
    }

    /// Used by the InstallDeveloperCb to install the event handler according to the derived platform.
    ///
    /// @param [in] pfnDeveloperCb   The developer callback function pointer to be executed by the pPlatform.
    /// @param [in] pPrivateData     Private data that is installed with the event handler for use by the installer.
    virtual void InstallDeveloperCb(
        Developer::Callback pfnDeveloperCb,
        void*               pPrivateData) = 0;

#if PAL_MEMTRACK
    /// @internal Memory leak tracker. Requires an allocator in order to perform the actual allocations. We can't
    /// provide this platform because that would result in a stack overflow. We must give it our forward allocator.
    Util::MemTracker<Util::ForwardAllocator> m_memTracker;
#endif

    /// @internal Memory allocator. Calls to Alloc() and Free() are chained down to the allocator's counterparts.
    Util::ForwardAllocator m_allocator;

private:
    /// @internal Client data pointer. This can have an arbitrary value and can be returned by calling GetClientData()
    /// and set via SetClientData().
    /// For non-top-layer objects, this will point to the layer above the current object.
    void* m_pClientData;
};

} // Pal
