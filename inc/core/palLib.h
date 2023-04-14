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
/**
 ***********************************************************************************************************************
 * @file  palLib.h
 * @brief Defines the Platform Abstraction Library (PAL) initialization and destruction functions.
 ***********************************************************************************************************************
 */

#pragma once

#include "pal.h"
#include "palSysMemory.h"
#include "palDbgPrint.h"

/// Major interface version.  Note that the interface version is distinct from the PAL version itself, which is returned
/// in @ref Pal::PlatformProperties.
///
/// @attention Updates to the major version indicate an interface change that is not backward compatible and may require
///            action from each client during their next integration.  When determining if a change is backward
///            compatible, it is not assumed that the client will initialize all input structs to 0.
///
/// @ingroup LibInit
#define PAL_INTERFACE_MAJOR_VERSION 793

/// Minor interface version.  Note that the interface version is distinct from the PAL version itself, which is returned
/// in @ref Pal::PlatformProperties.
///
/// The minor version is incremented on any change to the PAL interface that is backward compatible which is limited to
/// adding new function, adding a new type (enum, struct, class, etc.), and adding a new value to an enum such that none
/// of the existing enum values will change.  This number will be reset to 0 when the major version is incremented.
///
/// @ingroup LibInit
#define PAL_INTERFACE_MINOR_VERSION 3

/// Minimum major interface version. This is the minimum interface version PAL supports in order to support backward
/// compatibility. When it is equal to PAL_INTERFACE_MAJOR_VERSION, only the latest interface version is supported.
///
/// @ingroup LibInit
#define PAL_MINIMUM_INTERFACE_MAJOR_VERSION 700

/// Minimum supported major interface version for devdriver library. This is the minimum interface version of the devdriver
/// library that PAL is backwards compatible to.
///
/// @ingroup LibInit
#define PAL_MINIMUM_GPUOPEN_INTERFACE_MAJOR_VERSION 38

/**
 ***********************************************************************************************************************
 * @def     PAL_INTERFACE_VERSION
 * @ingroup LibInit
 * @brief   Current PAL interface version packed into a 32-bit unsigned integer.
 *
 * @see PAL_INTERFACE_MAJOR_VERSION
 * @see PAL_INTERFACE_MINOR_VERSION
 *
 * @hideinitializer
 ***********************************************************************************************************************
 */
#define PAL_INTERFACE_VERSION ((PAL_INTERFACE_MAJOR_VERSION << 16) | PAL_INTERFACE_MINOR_VERSION)

namespace Pal
{

// Forward declarations
class      IPlatform;

/// This is a list of GPUs that the NULL OS layer can compile shaders to in offline mode.
enum class NullGpuId : uint32
{
    Polaris10 = 0x00,
    Polaris11 = 0x01,
    Polaris12 = 0x02,

    Vega10 = 0x04,
    Raven  = 0x05,
    Vega12 = 0x06,
    Vega20 = 0x07,
    Raven2 = 0x08,
    Renoir = 0x09,

    Navi10           = 0x0A,
    Navi12           = 0x0B,
    Navi14           = 0x0D,
    Navi21           = 0x0F,
    Navi22           = 0x10,
    Navi23           = 0x11,
    Navi24           = 0x12,
    Rembrandt        = 0x14,
#if PAL_BUILD_NAVI31
    Navi31           = 0x1A,
#endif
    Raphael          = 0x1E,
    Max              = 0x23,
    All              = 0x24
};

/// Maps a null GPU ID to its associated text name.
struct NullGpuInfo
{
    NullGpuId   nullGpuId;  ///< ID of an ASIC that PAL supports for override purposes
    const char* pGpuName;   ///< Text name of the ASIC specified by nullGpuId
};

/// PAL client APIs.
enum class ClientApi : uint32
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 734
    Invalid = 0,
#endif
    Pal     = 0,
    Dx9     = 1,
    Dx12    = 3,
    Vulkan  = 4,
    Mantle  = 5,
    OpenCl  = 7,
    Hip     = 8,
};

/// Specifies properties for @ref IPlatform creation. Input structure to Pal::CreatePlatform().
struct PlatformCreateInfo
{
    const Util::AllocCallbacks*  pAllocCb;      ///< Optional client-provided callbacks. If non-null, PAL will call the
                                                ///  specified callbacks to allocate and free all internal system
                                                ///  memory. If null, PAL will manage memory on its own through the C
                                                ///  runtime library.
    const Util::LogCallbackInfo* pLogInfo;      ///< Optional client-provided callback info.  If non-null, Pal will
                                                ///  call the callback to pass debug prints to the client.

    const char*                  pSettingsPath; ///< A null-terminated string describing the path to where settings are
                                                ///  located on the system. For example, on Windows, this will refer to
                                                ///  which UMD subkey to look in under a device's key. For Linux, this
                                                ///  is the path to the settings file.

    union
    {
        struct
        {
            uint32 disableGpuTimeout              :  1; ///< Disables GPU timeout detection (Windows only)
            uint32 force32BitVaSpace              :  1; ///< Forces 32bit VA space for the flat address with 32bit ISA
            uint32 createNullDevice               :  1; ///< Set to create a null device, so "nullGpuId" below for the
                                                        ///  ID of the GPU the created device will be based on.  Null
                                                        ///  devices operate in IFH mode; useful for off-line shader
                                                        ///  compilations.
            uint32 enableSvmMode                  :  1; ///< Enable SVM mode. When this bit is set, PAL will reserve
                                                        ///  cpu va range with size "maxSvmSize", and allow client to
                                                        ///  to create gpu or pinned memory for use of Svm.
                                                        ///  For detail of SVM, please refer to CreateSvmGpuMemory
            uint32 requestShadowDescriptorVaRange :  1; ///< Requests that PAL provides support for the client to use
                                                        ///  the @ref VaRange::ShadowDescriptorTable virtual-address
                                                        ///  range. Some GPU's may not be capable of supporting this,
                                                        ///  even when requested by the client.
            uint32 disableInternalResidencyOpts   :  1; ///< Disables residency optimizations for internal GPU memory
                                                        ///  allocations.  Some clients may wish to have them turned
                                                        ///  off to save on system resources.
            uint32 supportRgpTraces               :  1; ///< Indicates that the client supports RGP tracing. PAL will
                                                        ///  use this flag and the hardware support flag to setup the
                                                        ///  DevDriver RgpServer.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 754
            uint32 dontOpenPrimaryNode            :  1; ///< No primary node is needed (Linux only)
#else
            uint32 placeholder754                 :  1;
#endif
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 765
            uint32 disableDevDriver               : 1;  ///< If no DevDriverMgr should be created with this Platform.
#else
            uint32 placeholder765                 : 1;
#endif
            uint32 reserved                       : 23; ///< Reserved for future use.
        };
        uint32 u32All;                                  ///< Flags packed as 32-bit uint.
    } flags;                                            ///< Platform-wide creation flags.

    union
    {
        ClientApi                clientApiId;           ///< Client API ID.
    };

    NullGpuId                    nullGpuId;             ///< ID for the null device.  Ignored unless the above
                                                        ///  flags.createNullDevice bit is set.
    uint16                       apiMajorVer;           ///< Major API version number to be used by RGP.  Should be
                                                        ///  set by client based on their contract with RGP.
    uint16                       apiMinorVer;           ///< Minor API version number to be used by RGP.  Should be
                                                        ///  set by client based on their contract with RGP.
    gpusize                      maxSvmSize;            ///  Maximum amount of virtual address space that will be
                                                        ///  reserved for SVM
};

/**
************************************************************************************************************************
* @brief Determines the amount of system memory required for a Platform object.
*
* This function must be called before any other interaction with PAL. An allocation of this amount of memory must be
* provided in the pPlacementAddr parameter of Pal::CreatePlatform.
*
* @ingroup LibInit
*
* @returns Size, in bytes, of system memory required for an IPlatform object.
************************************************************************************************************************
*/
size_t PAL_STDCALL GetPlatformSize();

/**
 ***********************************************************************************************************************
 * @brief Creates the Platform Abstraction Library.
 *
 * On execution of CreatePlatform(), PAL will establish a connection for OS and KMD communication, install the specified
 * system memory allocation callbacks, and initialize any global internal services.  Finally, the client will be
 * returned an object pointer to the instantiated platform object, which is used to query the capabilities of the
 * system.
 *
 * @ingroup LibInit
 *
 * @param [in]  createInfo     Parameters indicating the client requirements for the platform such as allocation
                               callbacks or the settings path.
 * @param [in]  pPlacementAddr Pointer to the location where PAL should construct this object.  There must be as
 *                             much size available here as reported by calling GetPlatformSize().
 * @param [out] ppPlatform     Platform object pointer to the instantiated platform. Must not be null.
 *
 * @returns Success if the initialization completed successfully.  Otherwise, one of the following error codes may be
 *          returned:
 *          + ErrorInvalidPointer will be returned if:
 *              - pPlatform is null.
 *              - pPlacementAddr is null.
 *              - createInfo.pAllocCb is non-null but pfnAlloc and/or pfnFree is null.
 *              - createInfo.pSettingsPath is null.
 *          + ErrorInitializationFailed will be returned if PAL is unable to open a connection to the OS.
 ***********************************************************************************************************************
 */
Result PAL_STDCALL CreatePlatform(
    const PlatformCreateInfo&   createInfo,
    void*                       pPlacementAddr,
    IPlatform**                 ppPlatform);

/**
 ***********************************************************************************************************************
 * @brief Provides an association of NULL devices and their associated text name.  NULL devices operate in IFH mode
 *        and are primarily intended for off-line shader compilation mode.  The text name is provided for end-user
 *        identification of the GPU device being created.
 *
 * @param [in,out] pNullDeviceCount   On input, this is the size of the "pNullDevices" array.  On output, this
 *                                    reflects the number of valid entries in the "pNullDevices" array.
 * @param [out]    pNullDevices       Includes information on the valid NULL devices supported by the system.  If
 *                                    this is NULL, then pNullDeviceCount reflects the maximum possible size of the
 *                                    null-devices array.
 *
 * @returns Success if the initialization completed successfully.  Otherwise, one of the following error codes may be
 *          returned:
 *          + ErrorInvalidPointer will be returned if either input is NULL.
 ***********************************************************************************************************************
 */
Result PAL_STDCALL EnumerateNullDevices(
    uint32*       pNullDeviceCount,
    NullGpuInfo*  pNullDevices);

/**
 ***********************************************************************************************************************
 * @defgroup LibInit Library Initialization and Destruction
 *
 * Before initializing PAL, it is important to make sure that the interface version is consistent with the client's
 * expectations.  The client should check @ref PAL_INTERFACE_MAJOR_VERSION to ensure the major interface version has not
 * changed since the last PAL integration.  Ideally, this should be performed with a compile-time assert comparing
 * @ref PAL_INTERFACE_MAJOR_VERSION against a client-maintained expected major version.   Minor interface version
 * changes should be backward compatible, and do not require a client change to maintain previous levels of
 * functionality.
 *
 * On startup, the client's first call to PAL must be GetPlatformSize() followed by CreatePlatform().  This function
 * gives an opportunity for PAL to perform any necessary platform-wide initialization such as opening a connection for
 * communication with the operating system and kernel mode driver or initializing tracking facilities for system memory
 * management.  CreatePlatform() returns a created IPlatform object for future interaction with PAL.
 *
 * PAL optionally allows the client to specify a set of memory management callbacks during initialization.  If
 * specified, PAL will not allocate or free any memory directly from the runtime, instead calling back to the client.
 * The client (or application, if the client forwards on the requests) may be able to implement a more efficient
 * allocation scheme.
 *
 * After a successful call to CreatePlatform(), the client should call @ref IPlatform::EnumerateDevices() in order to
 * get a list of supported devices attached to the system.  This function returns an array of @ref IDevice objects
 * which are used by the client to query properties of the devicess and eventually execute work on those devices.
 * IPlatform::EnumerateDevices() is not available to util-only clients (PAL_BUILD_CORE=0).
 *
 * The client may re-enumerate devices at any time by calling IPlatform::EnumerateDevices().  The client must make sure
 * there is no active work on any device and that all objects associated with those devices have been destroyed.
 * IPlatform::EnumerateDevices() will destroy all previously reported @ref IDevice objects and return a fresh set.
 * The client is required to re-enumerate devices when it receives a ErrorDeviceLost error from PAL.
 *
 * After enumerating devices, either during start-up or when recovering from an ErrorDeviceLost error, the client must
 * setup and finalize PAL's per-device settings.  See IDevice::GetPublicSettings(), IDevice::SetDxRuntimeData(),
 * IDevice::CommitSettingsAndInit(), and IDevice::Finalize() for details.
 *
 * After enumerating devices and finalizing them, the client may query the set of available screens. This is done by
 * calling the @ref IPlatform::GetScreens() function.  Note that screens are not available for DX clients.  Each screen
 * is accessible by zero or more of the enumerated devices. Most screens are accessible from a "main" device as well as
 * several other devices which can perform cross-display Flip presents to the screen. In some configurations, screens
 * may not be directly to any of PAL's devices, in which case fullscreen presents are unavailable to that screen. (This
 * typically only occurs in PowerExpress configurations.) Note that when IPlatform::EnumerateDevices() is called, any
 * enumerated @ref IScreen objects which existed prior to that call are invalidated for the specified platform and
 * IPlatform::GetScreens() needs to be called again to get the updated list of screens.
 *
 * On shutdown, the client should call @ref IPlatform::Destroy() to allow PAL to cleanup and free any remaining
 * platform-wide resources.  The client must ensure this call is not made until all other created objects are idle and
 * destroyed (if destroyable).
 *
 * When the client is asked to destroy a device it may call IDevice::Cleanup() to explicitly clean up the device. Some
 * clients will find it necessary to call Cleanup(), for example, if their devices have OS handles that become invalid.
 * Note that Cleanup() doesn't destroy the device; it will return to its initial state, as if it was newly enumerated.
 ***********************************************************************************************************************
 */

} // Pal
