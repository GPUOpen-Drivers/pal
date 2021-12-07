/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include <ddSocket.h>

#define DD_RPC_INVALID_FUNC_ID    0
#define DD_RPC_INVALID_SERVICE_ID 0

namespace Rpc
{

/// ddRpc can function off of any protocol id, but systems that don't care can use this default by
/// specifying 0 for protocolId when connecting.
///
/// This is the ASCII code for 'A' for AMD.
constexpr DDProtocolId kDefaultRpcProtocolId = 65;

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

} // namespace Rpc

