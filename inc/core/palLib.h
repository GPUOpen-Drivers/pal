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
#define PAL_INTERFACE_MAJOR_VERSION 397

/// Minor interface version.  Note that the interface version is distinct from the PAL version itself, which is returned
/// in @ref Pal::PlatformProperties.
///
/// The minor version is incremented on any change to the PAL interface that is backward compatible which is limited to
/// adding new function, adding a new type (enum, struct, class, etc.), and adding a new value to an enum such that none
/// of the existing enum values will change.  This number will be reset to 0 when the major version is incremented.
///
/// @ingroup LibInit
#define PAL_INTERFACE_MINOR_VERSION 1

/// Minimum major interface version. This is the minimum interface version PAL supports in order to support backward
/// compatibility. When it is equal to PAL_INTERFACE_MAJOR_VERSION, only the latest interface version is supported.
///
/// @ingroup LibInit
#define PAL_MINIMUM_INTERFACE_MAJOR_VERSION 346

/// Minimum supported major interface version for gpuopen library. This is the minimum interface version of the gpuopen
/// library that PAL is backwards compatible to.
///
/// @ingroup LibInit
#define PAL_MINIMUM_GPUOPEN_INTERFACE_MAJOR_VERSION 26

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

/// This is a list of GPU's that the NULL OS layer can compile shaders to in off-line mode.
enum class NullGpuId : uint32
{
    Tahiti     = 0x0,
    Hainan     = 0x1,
    Bonaire    = 0x2,
    Hawaii     = 0x3,
    Kalindi    = 0x4,
    Godavari   = 0x5,
    Iceland    = 0x6,
    Carrizo    = 0x7,
    Tonga      = 0x8,
    Fiji       = 0x9,
    Stoney     = 0xA,
#if PAL_BUILD_GFX9
    Vega10     = 0xB,
#endif
    Raven      = 0xC,
    Max        = 0x10,
    All        = 0x11
};

/// Maps a null GPU ID to its associated text name.
struct NullGpuInfo
{
    NullGpuId   nullGpuId;  ///< ID of an ASIC that PAL supports for override purposes
    const char* pGpuName;   ///< Text name of the ASIC specified by nullGpuId
};

/// Specifies properties for @ref IPlatform creation. Input structure to Pal::CreatePlatform().
struct PlatformCreateInfo
{
    const Util::AllocCallbacks*  pAllocCb;      ///< Optional client-provided callbacks. If non-null, PAL will call the
                                                ///  specified callbacks to allocate and free all internal system
                                                ///  memory. If null, PAL will manage memory on its own through the C
                                                ///  runtime library.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 377
    const Util::LogCallbackInfo* pLogInfo;      ///< Optional client-provided callback info.  If non-null, Pal will
                                                ///  call the callback to pass debug prints to the client.
#elif PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 368
    Util::LogCallbackFunc        pfnLogCb;      ///< Optional client-provided callback.  If non-null, Pal will call the
                                                ///  specified callback to pass debug prints to the client.
#endif
    const char*                  pSettingsPath; ///< A null-terminated string describing the path to where settings are
                                                ///  located on the system. For example, on Windows, this will refer to
                                                ///  which UMD subkey to look in under a device's key. For Linux, this
                                                ///  is the path to the settings file.

    union
    {
        struct
        {
            uint32 disableGpuTimeout          :  1;      ///< Disables GPU timeout detection (Windows only)
            uint32 force32BitVaSpace          :  1;      ///< Forces 32bit VA space for the flat address with 32bit ISA
            uint32 createNullDevice           :  1;      ///< Set to create a null device, so "nullGpuId" below for the
                                                         ///  ID of the GPU the created device will be based on.  Null
                                                         ///  devices operate in IFH mode; useful for off-line shader
                                                         ///  compilations.
            uint32 enableSvmMode              :  1;      ///< Enable SVM mode. When this bit is set, PAL will reserve
                                                         ///  cpu va range with size "maxSvmSize", and allow client to
                                                         ///  to create gpu or pinned memory for use of Svm.
                                                         ///  For detail of SVM, please refer to CreateSvmGpuMemory
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 358
            uint32 requestShadowDescriptorVaRange : 1;   ///< Requests that PAL provides support for the client to use
                                                         ///  the @ref VaRange::ShadowDescriptorTable virtual-address
                                                         ///  range. Some GPU's may not be capable of supporting this,
                                                         ///  even when requested by the client.
            uint32 reserved                   : 27;      ///< Reserved for future use.
#else
            uint32 reserved                   : 28;      ///< Reserved for future use.
#endif
        };
        uint32 u32All;                          ///< Flags packed as 32-bit uint.
    } flags;                                    ///< Platform-wide creation flags.

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
/**
 ***********************************************************************************************************************
 * @page Build Building PAL
 *
 * Client-Integrated Builds
 * ------------------------
 * PAL is a _source deliverable_.  Clients will periodically promote PAL's source from //depot/stg/pal_prm into their
 * own tree and build a static pal.lib as part of their build process.  This process matches what is done for other
 * shared components in our driver stack such as SC, AddrLib, and VAM.
 *
 * PAL's main makefile is in .../pal/src/make/Makefile.pal.  The client is responsible for including this makefile while
 * specifying its own defs/rules to build the PAL lib in the right spot with the right options.  Here is a bare-bones
 * example of how Mantle builds PAL:
 *
 *     include $(ICD_DEPTH)/make/icddefs
 *
 *     LIB_TARGET = icdimportedpal
 *
 *     PAL_OS_BUILD      = $(ICD_OS_BUILD)
 *     PAL_SC_DIR        = SC_DIR  # Typically $(ICD_DEPTH)/imported/sc, but controlled via build variable.
 *     PAL_CLIENT_MANTLE = 1
 *
 *     include $(ICD_DEPTH)/make/Makefile.$(ICD_PLATFORM).icd
 *     include $(PAL_DEPTH)/src/make/Makefile.pal
 *
 *     include $(ICD_DEPTH)/make/icdrules
 *
 * ### Internal Pipeline Compiler Component
 *
 *  PAL is delivered alongside a module which can compile pipeline binaries in ELF format.  This module, named SCPC, is
 *  based on the AMD proprietary shader compiler (SC).  The following build options in PAL are used to control how SCPC
 *  is included in the PAL build.
 *
 *      __PAL_BUILD_SCPC__: Defaults to 1.  Controls whether or not the SCPC component is built as part of the PAL
 *      build.  Clients should only change this to zero if they are using something besides SCPC for compiling their
 *      pipeline binaries.
 *
 *      __PAL_ENABLE_INTERNAL_SCPC__: Defaults to 1.  Controls whether or not a PAL IDevice object will manage an
 *      internal instance of an SCPC ICompiler object for compiling pipelines.  If this is 1, then @ref PAL_BUILD_SCPC
 *      is overridden to be 1 as well.
 *
 * ### External Shader Compiler
 * PAL must be linked with an SC library built by the client.  The client must specify the location of the SC interface
 * header files with this build parameter:
 *
 * + __PAL_SC_DIR__: Root of SC source (PAL will include headers from both the Interface and IL/inc subdirectories).
 *
 * The client is responsible for providing a version of the SC library that is compatible with PAL.  PAL will fail to
 * build if SC's major interface version isn't supported.  Since PAL handles all interaction with SC, PAL is responsible
 * for defining the SC_CLIENT_MAJOR_INTERFACE_VERSION variable on behalf of the client.  In order to facilitate this,
 * clients must include the following PAL makefile before including SC's Makefile.sc:
 *
 *     .../pal/src/make/palSpecifiedScDefs
 *
 * ### Build Options
 * The following build options control PAL's behavior, and can be set as desired by the client:
 *
 * + __Required__:
 *     - __PAL_OS_BUILD__: Set to the OS target string.  Supported targets are wNow, wNow64a, wNxt, wNxt64a, lnx, and
 *       lnx64a.
 *     - __PAL_SC_DIR__: As described above.
 * + __Optional__:
 *     - __PAL_CLOSED_SOURCE__: Defaults to 1.  Set to 0 to build only open source-able code.
 *     - __PAL_BUILD_CORE__: Defaults to 1.  Set to 0 to build only the PAL utility companion functionality (only the
 *       Util namespace will be usable).
 *     - The following build options allow specific IP support to be explicitly included or excluded:
 *         + __PAL_BUILD_GFX6__: Defaults to 1.  Set to 0 to exclude support for GFXIP 6-8.
 *         + __PAL_BUILD_OSS1__: Defaults to 1.  Set to 0 to exclude support for OSSIP 1 (i.e., DRMDMA on SI chips).
 *         + __PAL_BUILD_OSS2__: Defaults to 1.  Set to 0 to exclude support for OSSIP 2 (i.e., SDMA on CI chips).
 *         + __PAL_BUILD_OSS2_4__: Defaults to 1.  Set to 0 to exclude support for OSSIP 2.4 (i.e., SDMA on VI chips).
 *     - __PAL_BUILD_LAYERS__: Defaults to 1.  If 0, PAL will not build support for any interface shim layers.
 *       Individual layers can be either built or excluded with the following variables:
 *         + __PAL_BUILD_DBG_OVERLAY__: Defaults to 1.  The debug overlay can be enabled via a setting, and will
 *           output useful performance and debug related information on the screen while the application runs.  If 0,
 *           this support will be built out of the driver, and the setting won't do anything.
 *         + __PAL_BUILD_GPU_PROFILER__: Defaults to 1. The GpuProfiler can be enabled via a setting, and will
 *           output useful performance and debug related information to a CSV file for offline analysis. If 0, this
 *           support will be built out of the driver and the setting won't do anything.
 *     - __PAL_ENABLE_PRINTS_ASSERTS__: Enables debug printing and assertions.  Even if enabled at build time, debug
 *       prints and asserts can be filtered based on category/severity via runtime setting.  Defaults to 1 on debug
 *       builds.
 *     - __PAL_MEMTRACK__: Enables memory leak and buffer overrun tracking.  Defaults to 1 on debug builds if debug
 *       prints are also enabled.  A report of leaked memory will be printed during IPlatform::Destroy().
 *     - __PAL_DEVELOPER_BUILD__: Defaults to 0. If 1, enables developer-specific interfaces for development purposes.
 *
 * @note Some Util functionality is inline/macro based, and therefore the appropriate defines must be set when building
 *       client files that include PAL headers.  In particular, PAL_MEMTRACK and PAL_ENABLE_PRINTS_ASSERTS are used in
 *       palAssert.h and palSysMemory.h, and must match the setting used when building PAL even when included outside
 *       of the PAL library.
 *
 * Next: @ref UtilOverview
 ***********************************************************************************************************************
 */

} // Pal
