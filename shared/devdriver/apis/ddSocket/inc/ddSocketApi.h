/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef DD_SOCKET_API_HEADER
#define DD_SOCKET_API_HEADER

#include <ddApi.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Compile time version information
#define DD_SOCKET_API_MAJOR_VERSION 0
#define DD_SOCKET_API_MINOR_VERSION 3
#define DD_SOCKET_API_PATCH_VERSION 0

#define DD_SOCKET_API_VERSION_STRING DD_API_STRINGIFY_VERSION(DD_SOCKET_API_MAJOR_VERSION, \
                                                              DD_SOCKET_API_MINOR_VERSION, \
                                                              DD_SOCKET_API_PATCH_VERSION)

/// Opaque handle to a developer driver network socket
typedef struct DDSocket_t* DDSocket;

/// Helper structure used to represent a range of versions
typedef struct DDSocketVersionRange
{
    uint32_t min; /// Minimum version
    uint32_t max; /// Maximum version
} DDSocketVersionRange;

/// Structure that contains the information required to connect a socket
typedef struct DDSocketConnectInfo
{
    DDNetConnection   hConnection;  /// A handle to an existing connection object
                                    //< This is typically acquired through the create information structure in
                                    //< module client contexts or module system contexts.
    DDClientId        clientId;     /// The identifier of the client to connect to
                                    //< This is effectively the "ip address"
    DDProtocolId      protocolId;   /// The identifier of the protocol to use for the connection
                                    //< This is effectively the "port"
    uint32_t          timeoutInMs;  /// Number of milliseconds to wait before timing out the connection operation
                                    //< [Optional] Specify 0 to use a reasonable but implementation defined default.

    struct
    {
        DDSocketVersionRange versionRange; /// The range of allowable protocol versions to use for the connection
                                        //< The connection process will attempt to negotiate the highest version included
                                        //< in the range and the result can be queried with ddSocketQueryProtocolVersion()
    } legacy;                           /// Information provided for compatibility with legacy protocols
                                        //< NOTE: New code should leave this memory zero initialized
                                        //<       Everything within this field will be removed in a later version of the API
} DDSocketConnectInfo;

/// Structure that contains the information required to listen on a socket
typedef struct DDSocketListenInfo
{
    DDNetConnection   hConnection;  /// A handle to an existing connection object
    DDProtocolId      protocolId;   /// The identifier of the protocol to use for the connection
                                    //< This is effectively the "port"
    uint32_t          maxPending;   /// [Optional]
                                    //< Maximum number of connections that can be pending or "not yet accepted"
                                    //< on the new socket object
                                    //< If not provided, an internal default will be used instead

    struct
    {
        DDSocketVersionRange versionRange; /// The range of allowable protocol versions to use for incoming connections
                                        //< The connection process will attempt to negotiate the highest version included
                                        //< in the range and the result can be queried with ddSocketQueryProtocolVersion()
    } legacy;                           /// Information provided for compatibility with legacy protocols
                                        //< NOTE: New code should leave this memory zero initialized
                                        //<       Everything within this field will be removed in a later version of the API
} DDSocketListenInfo;

/// Get version of the loaded library to check interface compatibility
typedef DDApiVersion (*PFN_ddSocketQueryVersion)(
    void);

/// Get human-readable representation of the loaded library version
typedef const char* (*PFN_ddSocketQueryVersionString)(
    void);

/// Attempts to create a new socket object with the provided connection information
typedef DD_RESULT (*PFN_ddSocketConnect)(
    const DDSocketConnectInfo* pInfo,     /// [in]  Connection info
    DDSocket*                  phSocket); /// [out] Handle to the new socket object

/// Attempts to create a new socket object in the listening state using the provided information
typedef DD_RESULT (*PFN_ddSocketListen)(
    const DDSocketListenInfo* pInfo,     /// [in]  Listen info
    DDSocket*                 phSocket); /// [out] Handle to the new socket object

/// Attempts to create a new socket object by accepting a pending client from an existing socket in the
/// listening state.
typedef DD_RESULT (*PFN_ddSocketAccept)(
    DDSocket  hListenSocket, /// [in] Handle to an existing socket in the listening state
    uint32_t  timeoutInMs,   /// Number of milliseconds to wait for the operation to complete
    DDSocket* phNewSocket);  /// [out] Handle to the new socket object

/// Raw interface for sending data through a socket
/// Note: This function exists to provide low-level functionality for compatibility purposes.
///       ddSocketSend is recommended for most users.
typedef DD_RESULT(*PFN_ddSocketSendRaw)(
    DDSocket    hSocket,     /// [in] Socket handle
    const void* pData,       /// [in] Data to send
    size_t      dataSize,    /// Data size in bytes
    uint32_t    timeoutInMs, /// Timeout in milliseconds
    size_t*     pBytesSent); /// Number of bytes sent

/// Raw interface for receiving data through a socket
/// Note: This function exists to provide low-level functionality for compatibility purposes.
///       ddSocketReceive is recommended for most users.
typedef DD_RESULT(*PFN_ddSocketReceiveRaw)(
    DDSocket    hSocket,         /// [in] Socket handle
    void*       pBuffer,         /// [out] Buffer to write data to
    size_t      bufferSize,      /// Size of the buffer in bytes
    uint32_t    timeoutInMs,     /// Timeout in milliseconds
    size_t*     pBytesReceived); /// Number of bytes written to the buffer

/// Attempts to send all provided data through a socket
/// This function will not return until all provided data has been sent or an error is encountered
typedef DD_RESULT (*PFN_ddSocketSend)(
    DDSocket    hSocket,   /// [in] Socket handle
    const void* pData,     /// [in] Data to send
    size_t      dataSize); /// Data size in bytes

/// Attempts to fill the provided buffer with data from a socket
/// This function will not return until the whole buffer has been filled or an error is encountered
typedef DD_RESULT (*PFN_ddSocketReceive)(
    DDSocket    hSocket,     /// [in] Socket handle
    void*       pBuffer,     /// [out] Buffer to write data to
    size_t      bufferSize); /// Size of the buffer in bytes

/// Same as ddSocketSend, but with a 64-bit size indicator sent before the data
/// This should be used with the associated ddSocketReceiveWithSizePrefix function to transfer fixed
/// quantities of data across the network.
typedef DD_RESULT (*PFN_ddSocketSendWithSizePrefix)(
    DDSocket    hSocket,   /// [in] Socket handle
    const void* pData,     /// [in] Data to send
    size_t      dataSize); /// Data size in bytes

/// Same as ddSocketReceive, but with a 64-bit size indicator sent before the data
/// This should be used with the associated ddSocketSendWithSizePrefix function to transfer fixed
/// quantities of data across the network.
typedef DD_RESULT (*PFN_ddSocketReceiveWithSizePrefix)(
    DDSocket    hSocket,      /// [in] Socket handle
    void*       pBuffer,      /// [out] Buffer to write data to
    size_t      bufferSize,   /// Size of the buffer in bytes
    uint64_t*   pSizePrefix); /// [out] Received size prefix value

/// Closes an existing socket object
///
/// Note: Closing a socket that's currently listening will cause any sockets created from it to become disconnected.
///       A disconnected socket still needs to be closed like a normal socket, but all send/receive functionality
///       will fail.
typedef void (*PFN_ddSocketClose)(
    DDSocket hSocket); /// [in] Handle of the socket object to close

/// Returns the negotiated protocol version associated with a socket object
/// This function is not valid for sockets in the listening state and will return 0 if called
/// on a socket in the listening state or an invalid handle.
/// NOTE: This function is for compatibility with legacy protocols only and should not be used with new code
typedef uint32_t (*PFN_ddSocketQueryProtocolVersion)(
    DDSocket hSocket); /// [in] Handle of the socket object to query

/// API structure
typedef struct DDSocketApi
{
    PFN_ddSocketQueryVersion          pfnQueryVersion;
    PFN_ddSocketQueryVersionString    pfnQueryVersionString;
    PFN_ddSocketConnect               pfnConnect;
    PFN_ddSocketListen                pfnListen;
    PFN_ddSocketAccept                pfnAccept;
    PFN_ddSocketSendRaw               pfnSendRaw;
    PFN_ddSocketReceiveRaw            pfnReceiveRaw;
    PFN_ddSocketSend                  pfnSend;
    PFN_ddSocketReceive               pfnReceive;
    PFN_ddSocketSendWithSizePrefix    pfnSendWithSizePrefix;
    PFN_ddSocketReceiveWithSizePrefix pfnReceiveWithSizePrefix;
    PFN_ddSocketClose                 pfnClose;
    PFN_ddSocketQueryProtocolVersion  pfnQueryProtocolVersion;
} DDSocketApi;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
