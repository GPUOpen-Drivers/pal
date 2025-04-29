/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef DD_CONNECTION_API_H
#define DD_CONNECTION_API_H

#include "dd_common_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DD_CONNECTION_API_NAME "DD_CONNECTION_API"

#define DD_CONNECTION_API_VERSION_MAJOR 0
#define DD_CONNECTION_API_VERSION_MINOR 3
#define DD_CONNECTION_API_VERSION_PATCH 0

typedef struct DDConnectionInfo
{
    /// A number uniquely identifies a connection to UMD.
    DDConnectionId umdConnectionId;

    /// A number uniquely identifies a connection to KMD
    uint16_t       kmdConnectionId;

    /// The id for the host process.
    uint32_t       processId;

    /// The name of the host process.
    const char*    pProcessName;

    /// A pointer to string describing this connection
    const char*    pDescription;
} DDConnectionInfo;

typedef struct DDConnectionFilter
{
    /// A pointer to the user data that will be passed to the callback \ref DDConnectionFilter.filter.
    void* pUserData;

    /// A callback to be invoked to determine whether to ignore a connection before it's established.
    /// Note, this callback may be invoked on multiple threads.
    ///
    /// @param pUserData \ref DDConnectionFilter.pUserData.
    /// @param pConnInfo A pointer to a \ref DDConnectionInfo object.
    /// @return true The connection is filtered (ignored).
    /// @return false The connection is acknowledged.
    bool (*filter)(void* pUserData, const DDConnectionInfo* pConnInfo);
} DDConnectionFilter;

typedef struct DDConnectionCallbacksImpl DDConnectionCallbacksImpl;

typedef struct DDConnectionCallbacks
{
    /// An opaque pointer to the internal implementation.
    DDConnectionCallbacksImpl* pImpl;

    /// A function pointer to the callback to be invoked when \ref DDTool is connected to a \ref DDRouter.
    /// @param pImpl The value of \ref DDRouterConnectionCallbacks.pImpl.
    /// @param connectionId The id the uniquely represents the connection to \ref DDRouter.
    void (*OnRouterConnected)(DDConnectionCallbacksImpl* pImpl, DDConnectionId connectionId);

    /// A function pointer to the callback to be invoked when \ref DDTool is disconnected from \ref DDRouter.
    /// @param pImpl The value of \ref DDRouterConnectionCallbacks.pImpl.
    void (*OnRouterDisconnected)(DDConnectionCallbacksImpl* pImpl);

    /// A function pointer to the callback to be invoked when a driver connection is established. Note, this
    /// callback may be invoked multiple times for different UMD connections. Different UMD connections may
    /// share the same KMD connection.
    ///
    /// Note, this callback may be invoked on multiple threads.
    ///
    /// @param pImpl The value of \ref DDDriverConnectionCallbacks.pImpl.
    /// @param pConnInfo A pointer to a \ref DDConnectionInfo object uniquely describing a driver connection.
    void (*OnDriverConnected)(DDConnectionCallbacksImpl* pImpl, const DDConnectionInfo* pConnInfo);

    /// A function pointer to the callback to be invoked when a driver is disconnected.
    ///
    /// Note, this callback may be invoked on multiple threads.
    ///
    /// @param pImpl The value of \ref DDDriverConnectionCallbacks.pImpl.
    /// @param umdConnectionId A number uniquely identify a connection to UMD.
    void (*OnDriverDisconnected)(DDConnectionCallbacksImpl* pImpl, DDConnectionId umdConnectionId);

    /// A function pointer to the callback to be invoked when driver state changes.
    ///
    /// Note, this callback may be invoked on multiple threads.
    ///
    /// @param pImpl The value of \ref DDDriverConnectionCallbacks.pImpl.
    /// @param umdConnectionId A number uniquely identify a connection to UMD.
    /// @param state Current driver state.
    void (*OnDriverStateChanged)(DDConnectionCallbacksImpl* pImpl, DDConnectionId umdConnectionId, DD_DRIVER_STATE state);
} DDDriverConnectionCallbacks;

typedef struct DDConnectionInstance DDConnectionInstance;

typedef struct DDConnectionApi
{
    /// An opaque pointer to internal implementation of the connection api.
    DDConnectionInstance* pInstance;

    /// Set a filter for driver connection. If a connection is filtered, it's ignored, and \ref OnDriverConnected
    /// \ref OnDriverDisconnected and \ref OnDriverStateChanged won't be invoked. Note, successive calls of this
    /// function will overwrite previous filters. For the filter to take effect, it must be set before the call to
    /// \ref DDToolApi.Connect.
    ///
    /// @param pInstance Must be \ref DDConnectionApi.pInstance.
    /// @param pFilter A \ref DDConnectionFilter object. This parameter is copied internally.
    void (*SetConnectionFilter)(DDConnectionInstance* pInstance, DDConnectionFilter filter);

	/// Add an implementation of \ref DDConnectionCallbacks. Different implementations of the callbacks are
    /// identified by \ref DDConnectionCallbacks.pInstance.
    ///
    /// @param pInstance Must be \ref DDConnectionApi.pInstance.
    /// @param pCallbacks A pointer to a \ref DDConnectionCallbacks object. The pointed-to object itself doesn't
    /// need to out-live this function call because it's copied internally.
    /// @return DD_RESULT_SUCCESS The callbacks are successfully added.
    /// @return DD_RESULT_COMMON_ALREADY_EXISTS The callback implementation to be added alrady exists. Two
    /// object of \ref DDConnectionCallbacks are the same if their \ref DDConnectionCallbacks.pImpl
    /// are the same.
    /// @return DD_RESULT_COMMON_INVALID_PARAMETER If \param pCallbacks.pInstance is NULL.
	DD_RESULT (*AddConnectionCallbacks)(DDConnectionInstance* pInstance, const DDConnectionCallbacks* pCallbacks);

	/// Remove an implementation of \ref DDConnectionCallbacks.
    ///
    /// @param pInstance Must be \ref DDConnectionApi.pInstance.
    /// @param pImpl The same pointer that was previously passed to \ref AddRouterConnectionCallbacks.
    /// @return DD_RESULT_SUCCESS The callbacks are successfully removed.
    /// @return DD_RESULT_COMMON_DOES_NOT_EXIST The callbacks to be removed doesn't exist.
    /// @return DD_RESULT_COMMON_INVALID_PARAMETER If \param pImpl is NULL.
    DD_RESULT (*RemoveConnectionCallbacks)(DDConnectionInstance* pInstance, const DDConnectionCallbacksImpl* pImpl);

    /// Gets the current driver state for a connection
    ///
    /// @param pInstance Must be \ref DDConnectionApi.pInstance.
    /// @param umdConnectionId The id for the connection.
    /// @param pState The output state
    /// @return DD_RESULT_SUCCESS The request returned successfully.
    /// @return DD_RESULT_DD_GENERIC_INVALID_PARAMETER The connection does not exist or the pointer is null.
    /// @return Other failures from the call.
    DD_RESULT (*GetDriverState)(DDConnectionInstance* pInstance, DDConnectionId umdConnectionId, DD_DRIVER_STATE* pState);

} DDConnectionApi;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
