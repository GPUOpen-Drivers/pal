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

#ifndef DD_DRIVER_UTILS_API_H
#define DD_DRIVER_UTILS_API_H

#include "dd_common_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DD_DRIVER_UTILS_API_NAME "DD_DRIVER_UTILS_API"

#define DD_DRIVER_UTILS_API_VERSION_MAJOR 0
#define DD_DRIVER_UTILS_API_VERSION_MINOR 3
#define DD_DRIVER_UTILS_API_VERSION_PATCH 0

typedef enum DD_DRIVER_UTILS_FEATURE_FLAG
{
    /// Explicitly enable a feature in driver.
    DD_DRIVER_UTILS_FEATURE_FLAG_ENABLE  = 0,

    /// Explicitly disable a feature in driver. Some features are incompatible with others, so you
    /// might want to explicitly disable them.
    DD_DRIVER_UTILS_FEATURE_FLAG_DISABLE = 1,

    /// Ignore the state of a feature in driver, and let others decide to whether enable it or not.
    DD_DRIVER_UTILS_FEATURE_FLAG_IGNORE  = 2
} DD_DRIVER_UTILS_FEATURE_FLAG;

typedef enum DD_DRIVER_UTILS_FEATURE
{
    DD_DRIVER_UTILS_FEATURE_TRACING = 0,
    DD_DRIVER_UTILS_FEATURE_CRASH_ANALYSIS,
    DD_DRIVER_UTILS_FEATURE_SHADER_INSTRUMENTATION,
    DD_DRIVER_UTILS_FEATURE_STATIC_VMID,

    DD_DRIVER_UTILS_FEATURE_COUNT,
} DD_DRIVER_UTILS_FEATURE;

typedef struct DDDriverUtilsInstance DDDriverUtilsInstance;

typedef struct DDDriverUtilsApi
{
    /// An opaque pointer to the internal implementation of the driver utils api.
    DDDriverUtilsInstance* pInstance;

    /// @brief Enable, disable or ignore a feature in driver.
    ///
    /// This API can be called by multiple setters as long as they all request the same change.
    /// Once a feature is enabled/disabled by setter(s), it can no longer be changed by others until
    /// all setters, who previously enabled/disabled the feature, have ignored the feature.
    ///
    /// Feature flags are locked as soon as one driver connection is established. Users should call
    /// this API before any driver connection is established.
    ///
    /// @param[in] pInstance Must be @ref DDDriverUtilsApi.pInstance.
    /// @param[in] feature A feature to update.
    /// @param[in] flag Whether to enable, disable or ignore this feature in driver.
    /// @param[in] pSetterName A pointer to a name string indicating who is calling this API.
    /// @param[in] setterNameSize The size of \param pSetterName, excluding null-terminator.
    /// @return DD_RESULT_SUCCESS The requested feature flag was upated successfully.
    /// @return DD_RESULT_DD_GENERIC_NOT_READY Failed to update the feature flag because the feature
    /// has been updated by others to a state incompatible with the requested state.
    /// @return DD_RESULT_DD_GENERIC_CONNECTION_EXITS Failed to update the feature flag because at
    /// least one driver connection exists.
    DD_RESULT (*SetFeature)(
        DDDriverUtilsInstance*       pInstance,
        DD_DRIVER_UTILS_FEATURE      feature,
        DD_DRIVER_UTILS_FEATURE_FLAG flag,
        const char*                  pSetterName,
        uint32_t                     setterNameSize);

    /// @brief Queries PAL driver information JSON for client.
    ///
    /// @param[in] pInstance Must be @ref DDDriverUtilsApi.pInstance.
    /// @param[in] umdConnectionId The umd connection id.
    /// @param[in] writer The byte writer receiving the JSON data.
    /// @return DD_RESULT_SUCCESS if query succeeded.
    /// @return Other error codes if query failed.
    DD_RESULT (*QueryPalDriverInfo)(
        DDDriverUtilsInstance*      pInstance,
        DDConnectionId              umdConnectionId,
        const DDByteWriter&         writer);

    /// @brief Sets the driver overlay string in PAL
    ///
    /// @param[in] pInstance Must be @ref DDDriverUtilsApi.pInstance.
    /// @param[in] umdConnectionId The umd connection id.
    /// @param[in] pOverlayString The string to set.
    ///            Must be less than or equal to kMaxOverlayStringLength (including null terminator).
    /// @param[in] strIdx The index of the string to set, less than kNumOverlayStrings
    DD_RESULT(*SetDriverOverlayString)(
        DDDriverUtilsInstance* pInstance,
        DDConnectionId         umdConnectionId,
        const char*            pOverlayString,
        uint32_t               strIdx);
} DDDriverUtilsApi;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
