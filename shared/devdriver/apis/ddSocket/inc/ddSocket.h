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

#ifndef DD_SOCKET_HEADER
#define DD_SOCKET_HEADER

#include <ddSocketApi.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Attempts to create a new socket object with the provided connection information
DD_RESULT ddSocketConnect(
    const DDSocketConnectInfo* pInfo,     /// [in]  Connection info
    DDSocket*                  phSocket); /// [out] Handle to the new socket object

/// Attempts to create a new socket object in the listening state using the provided information
DD_RESULT ddSocketListen(
    const DDSocketListenInfo* pInfo,     /// [in]  Listen info
    DDSocket*                 phSocket); /// [out] Handle to the new socket object

/// Attempts to create a new socket object by accepting a pending client from an existing socket in the
/// listening state.
DD_RESULT ddSocketAccept(
    DDSocket  hListenSocket, /// [in] Handle to an existing socket in the listening state
    uint32_t  timeoutInMs,   /// Number of milliseconds to wait for the operation to complete
    DDSocket* phNewSocket);  /// [out] Handle to the new socket object

/// Raw interface for sending data through a socket
/// Note: This function exists to provide low-level functionality for compatibility purposes.
///       ddSocketSend is recommended for most users.
DD_RESULT ddSocketSendRaw(
    DDSocket    hSocket,     /// [in] Socket handle
    const void* pData,       /// [in] Data to send
    size_t      dataSize,    /// Data size in bytes
    uint32_t    timeoutInMs, /// Timeout in milliseconds
    size_t*     pBytesSent); /// Number of bytes sent

/// Raw interface for receiving data through a socket
/// Note: This function exists to provide low-level functionality for compatibility purposes.
///       ddSocketReceive is recommended for most users.
DD_RESULT ddSocketReceiveRaw(
    DDSocket    hSocket,         /// [in] Socket handle
    void*       pBuffer,         /// [out] Buffer to write data to
    size_t      bufferSize,      /// Size of the buffer in bytes
    uint32_t    timeoutInMs,     /// Timeout in milliseconds
    size_t*     pBytesReceived); /// Number of bytes written to the buffer

/// Attempts to send all provided data through a socket
/// This function will not return until all provided data has been sent or an error is encountered
DD_RESULT ddSocketSend(
    DDSocket    hSocket,   /// [in] Socket handle
    const void* pData,     /// [in] Data to send
    size_t      dataSize); /// Data size in bytes

/// Attempts to fill the provided buffer with data from a socket
/// This function will not return until the whole buffer has been filled or an error is encountered
DD_RESULT ddSocketReceive(
    DDSocket    hSocket,     /// [in] Socket handle
    void*       pBuffer,     /// [out] Buffer to write data to
    size_t      bufferSize); /// Size of the buffer in bytes

/// Same as ddSocketSend, but with a 64-bit size indicator sent before the data
/// This should be used with the associated ddSocketReceiveWithSizePrefix function to transfer fixed
/// quantities of data across the network.
DD_RESULT ddSocketSendWithSizePrefix(
    DDSocket    hSocket,   /// [in] Socket handle
    const void* pData,     /// [in] Data to send
    size_t      dataSize); /// Data size in bytes

/// Same as ddSocketReceive, but with a 64-bit size indicator sent before the data
/// This should be used with the associated ddSocketSendWithSizePrefix function to transfer fixed
/// quantities of data across the network.
DD_RESULT ddSocketReceiveWithSizePrefix(
    DDSocket    hSocket,      /// [in] Socket handle
    void*       pBuffer,      /// [out] Buffer to write data to
    size_t      bufferSize,   /// Size of the buffer in bytes
    uint64_t*   pSizePrefix); /// [out] Received size prefix value

/// Closes an existing socket object
///
/// Note: Closing a socket that's currently listening will cause any sockets created from it to become disconnected.
///       A disconnected socket still needs to be closed like a normal socket, but all send/receive functionality
///       will fail.
void ddSocketClose(
    DDSocket hSocket); /// [in] Handle of the socket object to close

/// Returns the negotiated protocol version associated with a socket object
/// This function is not valid for sockets in the listening state and will return 0 if called
/// on a socket in the listening state or an invalid handle.
/// NOTE: This function is for compatibility with legacy protocols only and should not be used with new code
uint32_t ddSocketQueryProtocolVersion(
    DDSocket hSocket); /// [in] Handle of the socket object to query

#ifdef __cplusplus
} // extern "C"
#endif

#endif
