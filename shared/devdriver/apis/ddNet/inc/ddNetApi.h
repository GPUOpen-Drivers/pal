/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef DD_NET_API_HEADER
#define DD_NET_API_HEADER

#include <ddApi.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Compile time version information
#define DD_NET_API_MAJOR_VERSION 0
#define DD_NET_API_MINOR_VERSION 5
#define DD_NET_API_PATCH_VERSION 0

#define DD_NET_API_VERSION_STRING DD_API_STRINGIFY_VERSION(DD_NET_API_MAJOR_VERSION, \
                                                           DD_NET_API_MINOR_VERSION, \
                                                           DD_NET_API_PATCH_VERSION)

/// Name of the API
#define DD_NET_API_NAME "ddNet"

/// Description of the API
#define DD_NET_API_DESCRIPTION "API that allows applications to connect to driver communication networks"

/// Identifier for the API
/// This identifier is used to acquire access to the API's interface
// Note: This is "drvnetwk" in big endian ASCII
#define DD_NET_API_ID 0x6472766e6574776b

/// Types of developer mode clients
typedef enum
{
    DD_NET_CLIENT_TYPE_UNKNOWN               = 0, /// Default value
    DD_NET_CLIENT_TYPE_SERVER                = 1, /// A program that passively interacts with clients on the bus
    DD_NET_CLIENT_TYPE_TOOL                  = 2, /// A program that actively interacts with clients on the bus
    DD_NET_CLIENT_TYPE_TOOL_WITH_DRIVER_INIT = 3, /// Same as DD_NET_CLIENT_TYPE_TOOL, but with driver init handling
    DD_NET_CLIENT_TYPE_DRIVER                = 4, /// A user mode driver
    DD_NET_CLIENT_TYPE_DRIVER_KERNEL         = 5, /// A kernel mode driver
    DD_NET_CLIENT_TYPE_COUNT                 = 6, /// Total number of client types
} DD_NET_CLIENT_TYPE;

/// Information for a connection
typedef struct DDNetConnectionInfo
{
    DD_NET_CLIENT_TYPE type;         /// Type of client
                                     /// This can be seen by other clients on the network and is used to enable
                                     /// special functionality in some cases.

    const char*        pDescription; /// Brief description of the client
                                     /// This string can be queried by other programs on the message bus

    const char*        pHostname;    /// String containing the IP address or hostname of the target
                                     /// machine that ddNet will attempt to connect to.
                                     /// If set to nullptr, the implementation assumes the target and client machine labels
                                     /// refer to the same machine and a local connection is attempted.

    uint16_t           port;         /// If `pHostname` is not nullptr, this is used as the port number
                                     /// for network communications. If set to 0, a default port number
                                     /// is chosen.
                                     ///
                                     /// If `pHostname` is nullptr, this is used as an identifier for
                                     /// local inter-process communications. To use the default communication
                                     /// channel, set this to 0.

    uint32_t           timeoutInMs;  /// Number of milliseconds to wait before timing out the connection operation
                                     /// [Optional] Specify 0 to use a reasonable but implementation defined default.
} DDNetConnectionInfo;

/// Data structure that contains information about a client that has been discovered
typedef struct DDNetDiscoveredClientInfo
{
    const char*        pProcessName; /// Name of the process that this client resides in
    const char*        pDescription; /// Description provided by the client
    uint32_t           processId;    /// Identifier associated with the client's process
    DD_NET_CLIENT_TYPE type;         /// Type associated with the client
    DDClientId         id;           /// Network identifier associated with the client
} DDNetDiscoveredClientInfo;

/// Callback function used to handle client discovery
/// Return non-zero from this callback to indicate that the discovery process should be continued.
/// Return zero from this callback to indicate that the discovery process should be terminated.
typedef int (*PFN_ddNetClientDiscoveredCallback)(void* pUserdata, const DDNetDiscoveredClientInfo* pInfo);

/// Data structure that describes how a client discovery operation should be performed
typedef struct DDNetDiscoverInfo
{
    PFN_ddNetClientDiscoveredCallback pfnCallback; /// Callback function pointer
    void*                             pUserdata;   /// Userdata for callback
    DD_NET_CLIENT_TYPE                targetType;  /// Used to scope the discover operation to a specific client type
                                                   /// When set to something other than 0, the callback will only be
                                                   /// invoked if the client's type matches the provided type. When set
                                                   /// to 0, this field has no effect on the clients returned by the
                                                   /// callback.
    uint32_t                          timeoutInMs; /// Timeout in milliseconds
} DDNetDiscoverInfo;

/// Get version of the loaded library to check interface compatibility
typedef DDApiVersion (*PFN_ddNetQueryVersion)(
    void);

/// Get human-readable representation of the loaded library version
typedef const char* (*PFN_ddNetQueryVersionString)(
    void);

/// Convert a `DD_RESULT` into a human recognizable string.
typedef const char* (*PFN_ddNetResultToString)(
    DD_RESULT result);

/// Attempts to create a new connection to a developer driver network
typedef DD_RESULT (*PFN_ddNetCreateConnection)(
    const DDNetConnectionInfo* pInfo,         /// [in] Connection info
    DDNetConnection*           phConnection); /// [out] Handle to a new connection object

/// Destroys an existing developer driver network connection object
///
/// The provided handle becomes invalid once this function returns and should be discarded
typedef void (*PFN_ddNetDestroyConnection)(
    DDNetConnection hConnection); /// [in] Connection handle

/// Returns the network client id associated with a connection object or 0 if an invalid handle is provided
typedef DDClientId (*PFN_ddNetQueryClientId)(
    DDNetConnection hConnection); /// [in] Handle to an existing connection object

/// Attempts to discover existing clients on the network based on the provided information
///
/// Returns DD_RESULT_SUCCESS when the caller's code indicates that it is finished with the discovery process
/// Returns DD_RESULT_DD_GENERIC_NOT_READY if the provided timeout is reached before the caller's code terminates
/// the operation.
/// NOTE: The implementation of this function intentionally ignores older network clients that lack complete
///       information. If you find that this function isn't detecting the clients you're looking for, be sure
///       to try a newer version of the network code in the client or switch to the legacy library in the tool.
typedef DD_RESULT (*PFN_ddNetDiscover)(
    DDNetConnection          hConnection, /// [in] Handle to an existing connection object
    const DDNetDiscoverInfo* pInfo);      /// [in] Information on how the discover operation should be performed

/// API structure
typedef struct DDNetApi
{
    PFN_ddNetQueryVersion       pfnQueryVersion;
    PFN_ddNetQueryVersionString pfnQueryVersionString;
    PFN_ddNetResultToString     pfnResultToString;
    PFN_ddNetCreateConnection   pfnCreateConnection;
    PFN_ddNetDestroyConnection  pfnDestroyConnection;
    PFN_ddNetQueryClientId      pfnQueryClientId;
    PFN_ddNetDiscover           pfnDiscover;
} DDNetApi;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
