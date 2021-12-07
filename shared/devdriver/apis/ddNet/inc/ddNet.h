/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

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

#endif // ! DD_NET_HEADER
