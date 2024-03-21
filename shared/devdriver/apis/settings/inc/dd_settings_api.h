/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef DD_SETTINGS_API_H
#define DD_SETTINGS_API_H

#include <dd_allocator_api.h>
#include <dd_common_api.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DD_SETTINGS_API_NAME "DD_SETTINGS_API"

#define DD_SETTINGS_API_VERSION_MAJOR 1
#define DD_SETTINGS_API_VERSION_MINOR 0
#define DD_SETTINGS_API_VERSION_PATCH 0

#define DD_SETTINGS_MAX_COMPONENT_NAME_SIZE 41 // include null-terminator
#define DD_SETTINGS_MAX_PATH_SIZE           256
#define DD_SETTINGS_MAX_FILE_NAME_SIZE      256
#define DD_SETTINGS_MAX_MISC_STRING_SIZE    256

typedef uint32_t DD_SETTINGS_NAME_HASH;

typedef enum
{
    DD_SETTINGS_TYPE_BOOL = 0,
    DD_SETTINGS_TYPE_INT8,
    DD_SETTINGS_TYPE_UINT8,
    DD_SETTINGS_TYPE_INT16,
    DD_SETTINGS_TYPE_UINT16,
    DD_SETTINGS_TYPE_INT32,
    DD_SETTINGS_TYPE_UINT32,
    DD_SETTINGS_TYPE_INT64,
    DD_SETTINGS_TYPE_UINT64,
    DD_SETTINGS_TYPE_FLOAT,
    DD_SETTINGS_TYPE_STRING,
} DD_SETTINGS_TYPE;

typedef enum
{
    DD_SETTINGS_DRIVER_TYPE_DX12 = 0,
    DD_SETTINGS_DRIVER_TYPE_DX10,
    DD_SETTINGS_DRIVER_TYPE_VULKAN,
    DD_SETTINGS_DRIVER_TYPE_COUNT,
} DD_SETTINGS_DRIVER_TYPE;

typedef struct
{
    /// The hash value of the setting name.
    DD_SETTINGS_NAME_HASH hash;

    struct
    {
        /// The type of the setting. Must be one of the values of \ref DD_SETTINGS_TYPE.
        uint8_t type    : 8;

        /// The size of the value pointed to by `pValue`.
        /// NB: For a string setting, if its type is static char array, this value represents the size
        /// of the array NOT the length of the string. If the type variable-size char array, this value
        /// represents the length of the string. In either case the size includes null-terminator.
        /// NB: If it's a non-string optional setting, this value represents the size of the inner type
        /// T, not DevDriver::Optional<T>.
        uint16_t size   : 16;

        /// Whether the setting is wrapped inside DevDriver::Optional.
        bool isOptional : 1;
    };

    /// A pointer to the setting value stored somewhere else.
    void* pValue;
} DDSettingsValueRef;

/// This struct represents a settings component and all of its values.
typedef struct
{
    /// A null-terminated string.
    char componentName[DD_SETTINGS_MAX_COMPONENT_NAME_SIZE];

    /// The number of items in the array pointed to by \ref DDSettingsComponentValueRefs.pValues.
    size_t numValues;

    /// A pointer to an array of DDSettingsValueRef.
    DDSettingsValueRef* pValues;
} DDSettingsComponentValueRefs;

typedef struct DDSettingsInstance DDSettingsInstance;

typedef struct
{
    /// A opaque pointer to an internal settings implementation.
    DDSettingsInstance* pInstance;

    /// Retrieve settings blobs of all components from a driver.
    ///
    /// @param[in] pInstance Must be \ref DDSettingsApi.pInstance.
    /// @param apiType Which driver to load settings blobs from.
    /// @param[in] pDriverPathOverride A path to the overridding driver. If NULL, the default driver path is used.
    /// @param[in] driverPathOverrideSize The length of the path pointed to by \param pDriverPathOverride, not including
    /// null-terminator. If 0, the default driver path is used.
    /// @param reload Whether to reload settings blobs from the driver.
    /// @param[out] ppSettingsBlobs Will be set to a pointer to the buffer to receive settings blobs.
    /// @param[out] pSettingsBlobsSize Will be set to the size of the buffer of settings blobs.
    /// @param[int] alloc Used to allocate the buffer to receive settings blobs.
    DD_RESULT (*QuerySettingsBlobsAll)(
        DDSettingsInstance*     pInstance,
        DD_SETTINGS_DRIVER_TYPE driverType,
        const char*             pDriverPathOverride,
        size_t                  driverPathOverrideSize,
        bool                    reload,
        char**                  ppSettingsBlobs,
        size_t*                 pSettingsBlobsSize,
        DDAllocator             alloc);

    /// Send user overrides of all settings components to a driver.
    ///
    /// @param[in] pInstance Must be \ref DDSettingsApi.pInstance.
    /// @param[in] umdConnectionId The id for the umd connection over which user overrides will be sent.
    /// @param[in] numComponents Number of items in the array pointed to by \param pComponentsOverrides.
    /// @param[in] pComponentsOverrides A pointer to an array of \ref DDSettingsComponentValueRefs.
    DD_RESULT (*SendAllUserOverrides)(
        DDSettingsInstance*                 pInstance,
        uint16_t                            umdConnectionId,
        size_t                              numComponents,
        const DDSettingsComponentValueRefs* pComponentsOverrides);

    /// Query the values of settings in all components from a driver.
    ///
    /// @param[in] pInstance Must be \ref DDSettingsApi.pInstance.
    /// @param umdConnectionId The id for the umd connection over which user overrides will be sent.
    /// @param[out] ppBuffer Will be set to a pointer to a byte-array containing settings values.
    /// @param[out] pSize The size of the byte-array pointed to by *\param ppBuffer.
    /// @param[in] alloc Will be used to allocate the byte-array mentioned above.
    DD_RESULT (*QueryAllCurrentValues)(
        DDSettingsInstance* pInstance,
        uint16_t            umdConnectionId,
        uint8_t**           ppBuffer,
        size_t*             pSize,
        DDAllocator         alloc);
} DDSettingsApi;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
