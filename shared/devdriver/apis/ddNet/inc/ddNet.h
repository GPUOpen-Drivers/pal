/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef DD_NET_HEADER
#define DD_NET_HEADER

#include <ddNetApi.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Get version of the loaded library to check interface compatibility
DDApiVersion ddNetQueryVersion(void);

/// Get human-readable representation of the loaded library version
const char* ddNetQueryVersionString(void);

/// Convert a `DD_RESULT` into a human recognizable string.
const char* ddNetResultToString(
    DD_RESULT result);

/// Attempts to create a new connection to a developer driver network
DD_RESULT ddNetCreateConnection(
    const DDNetConnectionInfo* pInfo,         /// [in] connection info
    DDNetConnection*           phConnection); /// [out] Handle to a new connection object

/// Destroys an existing developer driver network connection object
///
/// The provided handle becomes invalid once this function returns and should be discarded
void ddNetDestroyConnection(
    DDNetConnection hConnection); /// [in] Connection handle

/// Returns the network client id associated with a connection object or 0 if an invalid handle is provided
DDClientId ddNetQueryClientId(
    DDNetConnection hConnection); /// [in] Handle to an existing connection object

/// Attempts to discover existing clients on the network based on the provided information
///
/// Returns DD_RESULT_SUCCESS when the caller's code indicates that it is finished with the discovery process
/// Returns DD_RESULT_DD_GENERIC_NOT_READY if the provided timeout is reached before the caller's code terminates
/// the operation.
/// NOTE: The implementation of this function intentionally ignores older network clients that lack complete
///       information. If you find that this function isn't detecting the clients you're looking for, be sure
///       to try a newer version of the network code in the client or switch to the legacy library in the tool.
DD_RESULT ddNetDiscover(
    DDNetConnection          hConnection, /// [in] Handle to an existing connection object
    const DDNetDiscoverInfo* pInfo);      /// [in] Information on how the discover operation should be performed

#ifdef __cplusplus
} // extern "C"
#endif

#endif
