/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#pragma once

#include <ddSocket.h>

#define DD_RPC_INVALID_FUNC_ID    0
#define DD_RPC_INVALID_SERVICE_ID 0

/// Compile time version information for keeping track of the services query interface:
#define DD_RPC_SERVICES_QUERY_MAJOR_VERSION 0
#define DD_RPC_SERVICES_QUERY_MINOR_VERSION 1
#define DD_RPC_SERVICES_QUERY_PATCH_VERSION 0

#define DD_RPC_SERVICES_QUERY_VERSION_STRING DD_API_STRINGIFY_VERSION(DD_RPC_SERVICES_QUERY_MAJOR_VERSION, \
                                                                      DD_RPC_SERVICES_QUERY_MINOR_VERSION, \
                                                                      DD_RPC_SERVICES_QUERY_PATCH_VERSION)

namespace Rpc
{

/// ddRpc can function off of any protocol id, but systems that don't care can use this default by
/// specifying 0 for protocolId when connecting.
///
/// This is the ASCII code for 'A' for AMD.
constexpr DDProtocolId kDefaultRpcProtocolId = 65;

/// To note special handling for some RPC calls, create a reserved RPC Service ID.
constexpr DDRpcServiceId kServicesQueryRpcServiceId = 0xFFFFFFFF;

/// Intermediate structures for RPC network logic
///
/// These structures must NEVER be sent across the network directly!
/// Users MUST use the Serialize/Deserialize functions when moving the information in these structures over the network

/// Header sent before every RPC request
struct RpcRequestHeader
{
    /// Identifier of the service that should handle this request
    DDRpcServiceId service;

    /// Indicates compatibility requirements for this request
    DDApiVersion serviceVersion;

    /// Identifier of the function that should be invoked for this request
    DDRpcFunctionId function;

    /// Size of the parameter buffer that follows this packet or 0 if this request doesn't have one
    uint64_t paramBufferSize;
};

/// Known types of responses that can be sent from the server in response to a call operation
enum class RpcResponseType : uint32_t
{
    Unknown = 0,

    SizeIndicator,
    Data,
    Terminator,

    Count
};

/// Header sent before every RPC response
///
/// NOTE: Multiple response messages may be sent as a result of a single call operation
struct RpcResponseHeader
{
    /// Type of response that follows this header
    RpcResponseType type;
};

/// Indicates the total size of the response data that will be sent during the call operation
struct RpcSizeIndicatorResponse
{
    /// Total size of the incoming response data
    uint64_t size;
};

/// Payload-carrying response message
///
/// Payload data associated with this message will immediately follow from the network
/// NOTE: A single call operation may generate several of these messages depending on how the data is sent
///       by the service implementation.
struct RpcDataResponse
{
    /// Size of the following payload data associated with this packet
    uint64_t size;
};

/// End-Of-Operation termination message
///
/// This message indicates to the client that the service has finished processing its request and this
/// is the last message that will be sent as part of the operation.
struct RpcTerminatorResponse
{
    /// The result code returned when execution of the remote function was completed
    DD_RESULT result;
};

/// Boilerplate serialize/deserialize implementations for all intermediate RPC message types
///
/// These should be used to convert intermediate messages to a robust carrier format before transferring them across
/// the network.

DD_RESULT SerializeRequestHeader(const RpcRequestHeader& header, void* pBuffer, size_t bufferSize, size_t* pBytesWritten);
DD_RESULT DeserializeRequestHeader(RpcRequestHeader* pHeader, const void* pBuffer, size_t bufferSize);

DD_RESULT SerializeResponseHeader(const RpcResponseHeader& header, void* pBuffer, size_t bufferSize, size_t* pBytesWritten);
DD_RESULT DeserializeResponseHeader(RpcResponseHeader* pHeader, const void* pBuffer, size_t bufferSize);

DD_RESULT SerializeSizeIndicatorResponse(const RpcSizeIndicatorResponse& response, void* pBuffer, size_t bufferSize, size_t* pBytesWritten);
DD_RESULT DeserializeSizeIndicatorResponse(RpcSizeIndicatorResponse* pResponse, const void* pBuffer, size_t bufferSize);

DD_RESULT SerializeDataResponse(const RpcDataResponse& response, void* pBuffer, size_t bufferSize, size_t* pBytesWritten);
DD_RESULT DeserializeDataResponse(RpcDataResponse* pResponse, const void* pBuffer, size_t bufferSize);

DD_RESULT SerializeTerminatorResponse(const RpcTerminatorResponse& response, void* pBuffer, size_t bufferSize, size_t* pBytesWritten);
DD_RESULT DeserializeTerminatorResponse(RpcTerminatorResponse* pResponse, const void* pBuffer, size_t bufferSize);

// Functions to track the services query interface. These are only used internally by the client/server code:
DDApiVersion RpcServicesQueryVersion();
const char*  RpcServicesQueryVersionString();

} // namespace Rpc

