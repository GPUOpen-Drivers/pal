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

#include <ddRpcShared.h>
#include <ddPlatform.h>
#include <mpack.h>

using namespace DevDriver;

namespace Rpc
{

/// Constant string definitions used during the serialization process
///
/// These are defined in a single location to combat inconsistency errors

constexpr const char kRequestHeaderServiceKey[]             = "service";
constexpr const char kRequestHeaderServiceVersionKey[]      = "serviceVersion";
constexpr const char kRequestHeaderServiceVersionMajorKey[] = "major";
constexpr const char kRequestHeaderServiceVersionMinorKey[] = "minor";
constexpr const char kRequestHeaderServiceVersionPatchKey[] = "patch";
constexpr const char kRequestHeaderFunctionKey[]            = "function";
constexpr const char kRequestHeaderParamBufferSizeKey[]     = "paramBufferSize";

constexpr const char kResponseHeaderTypeKey[] = "type";

constexpr const char kSizeIndicatorResponseSizeKey[] = "size";

constexpr const char kDataResponseSizeKey[] = "size";

constexpr const char kTerminatorResponseResultKey[] = "result";

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT SerializeRequestHeader(const RpcRequestHeader& header, void* pBuffer, size_t bufferSize, size_t* pBytesWritten)
{
    DD_RESULT result = DD_RESULT_PARSING_INVALID_MSGPACK;

    mpack_writer_t writer;
    mpack_writer_init(&writer, static_cast<char*>(pBuffer), bufferSize);

    mpack_start_map(&writer, 4);

    mpack_write_cstr(&writer, kRequestHeaderServiceKey);
    mpack_write_uint(&writer, header.service);

    mpack_write_cstr(&writer, kRequestHeaderServiceVersionKey);
    mpack_start_map(&writer, 3);
    {
        mpack_write_cstr(&writer, kRequestHeaderServiceVersionMajorKey);
        mpack_write_uint(&writer, header.serviceVersion.major);

        mpack_write_cstr(&writer, kRequestHeaderServiceVersionMinorKey);
        mpack_write_uint(&writer, header.serviceVersion.minor);

        mpack_write_cstr(&writer, kRequestHeaderServiceVersionPatchKey);
        mpack_write_uint(&writer, header.serviceVersion.patch);
    }
    mpack_finish_map(&writer);

    mpack_write_cstr(&writer, kRequestHeaderFunctionKey);
    mpack_write_uint(&writer, header.function);

    mpack_write_cstr(&writer, kRequestHeaderParamBufferSizeKey);
    mpack_write_uint(&writer, header.paramBufferSize);

    mpack_finish_map(&writer);

    if (mpack_writer_error(&writer) == mpack_ok)
    {
        *pBytesWritten = mpack_writer_buffer_used(&writer);

        result = DD_RESULT_SUCCESS;
    }
    else
    {
        DD_PRINT(LogLevel::Warn, "Serialization of RPC request header failed!");
    }

    mpack_writer_destroy(&writer);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT DeserializeRequestHeader(RpcRequestHeader* pHeader, const void* pBuffer, size_t bufferSize)
{
    DD_RESULT result = DD_RESULT_PARSING_INVALID_MSGPACK;

    mpack_tree_t tree;
    mpack_tree_init_data(&tree, static_cast<const char*>(pBuffer), bufferSize);
    mpack_tree_parse(&tree);
    mpack_node_t root = mpack_tree_root(&tree);

    pHeader->service = mpack_node_u32(mpack_node_map_cstr(root, kRequestHeaderServiceKey));

    mpack_node_t serviceVersion   = mpack_node_map_cstr(root, kRequestHeaderServiceVersionKey);
    pHeader->serviceVersion.major = mpack_node_u32(mpack_node_map_cstr(serviceVersion, kRequestHeaderServiceVersionMajorKey));
    pHeader->serviceVersion.minor = mpack_node_u32(mpack_node_map_cstr(serviceVersion, kRequestHeaderServiceVersionMinorKey));
    pHeader->serviceVersion.patch = mpack_node_u32(mpack_node_map_cstr(serviceVersion, kRequestHeaderServiceVersionPatchKey));

    pHeader->function        = mpack_node_u32(mpack_node_map_cstr(root, kRequestHeaderFunctionKey));
    pHeader->paramBufferSize = mpack_node_u64(mpack_node_map_cstr(root, kRequestHeaderParamBufferSizeKey));

    if (mpack_tree_error(&tree) == mpack_ok)
    {
        result = DD_RESULT_SUCCESS;
    }
    else
    {
        DD_PRINT(LogLevel::Warn, "Deserialization of RPC request header failed!");
    }

    mpack_tree_destroy(&tree);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT SerializeResponseHeader(const RpcResponseHeader& header, void* pBuffer, size_t bufferSize, size_t* pBytesWritten)
{
    DD_RESULT result = DD_RESULT_PARSING_INVALID_MSGPACK;

    mpack_writer_t writer;
    mpack_writer_init(&writer, static_cast<char*>(pBuffer), bufferSize);

    mpack_start_map(&writer, 1);

    mpack_write_cstr(&writer, kResponseHeaderTypeKey);
    mpack_write_uint(&writer, static_cast<uint64_t>(header.type));

    mpack_finish_map(&writer);

    if (mpack_writer_error(&writer) == mpack_ok)
    {
        *pBytesWritten = mpack_writer_buffer_used(&writer);

        result = DD_RESULT_SUCCESS;
    }
    else
    {
        DD_PRINT(LogLevel::Warn, "Serialization of RPC response header failed!");
    }

    mpack_writer_destroy(&writer);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT DeserializeResponseHeader(RpcResponseHeader* pHeader, const void* pBuffer, size_t bufferSize)
{
    DD_RESULT result = DD_RESULT_PARSING_INVALID_MSGPACK;

    mpack_tree_t tree;
    mpack_tree_init_data(&tree, static_cast<const char*>(pBuffer), bufferSize);
    mpack_tree_parse(&tree);
    mpack_node_t root = mpack_tree_root(&tree);

    pHeader->type = static_cast<RpcResponseType>(mpack_node_u32(mpack_node_map_cstr(root, kResponseHeaderTypeKey)));

    if (mpack_tree_error(&tree) == mpack_ok)
    {
        result = DD_RESULT_SUCCESS;
    }
    else
    {
        DD_PRINT(LogLevel::Warn, "Deserialization of RPC response header failed!");
    }

    mpack_tree_destroy(&tree);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT SerializeSizeIndicatorResponse(const RpcSizeIndicatorResponse& response, void* pBuffer, size_t bufferSize, size_t* pBytesWritten)
{
    DD_RESULT result = DD_RESULT_PARSING_INVALID_MSGPACK;

    mpack_writer_t writer;
    mpack_writer_init(&writer, static_cast<char*>(pBuffer), bufferSize);

    mpack_start_map(&writer, 1);

    mpack_write_cstr(&writer, kSizeIndicatorResponseSizeKey);
    mpack_write_uint(&writer, response.size);

    mpack_finish_map(&writer);

    if (mpack_writer_error(&writer) == mpack_ok)
    {
        *pBytesWritten = mpack_writer_buffer_used(&writer);

        result = DD_RESULT_SUCCESS;
    }
    else
    {
        DD_PRINT(LogLevel::Warn, "Serialization of RPC size indicator response failed!");
    }

    mpack_writer_destroy(&writer);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT DeserializeSizeIndicatorResponse(RpcSizeIndicatorResponse* pResponse, const void* pBuffer, size_t bufferSize)
{
    DD_RESULT result = DD_RESULT_PARSING_INVALID_MSGPACK;

    mpack_tree_t tree;
    mpack_tree_init_data(&tree, static_cast<const char*>(pBuffer), bufferSize);
    mpack_tree_parse(&tree);
    mpack_node_t root = mpack_tree_root(&tree);

    pResponse->size = mpack_node_u64(mpack_node_map_cstr(root, kSizeIndicatorResponseSizeKey));

    if (mpack_tree_error(&tree) == mpack_ok)
    {
        result = DD_RESULT_SUCCESS;
    }
    else
    {
        DD_PRINT(LogLevel::Warn, "Deserialization of RPC size indicator response failed!");
    }

    mpack_tree_destroy(&tree);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT SerializeDataResponse(const RpcDataResponse& response, void* pBuffer, size_t bufferSize, size_t* pBytesWritten)
{
    DD_RESULT result = DD_RESULT_PARSING_INVALID_MSGPACK;

    mpack_writer_t writer;
    mpack_writer_init(&writer, static_cast<char*>(pBuffer), bufferSize);

    mpack_start_map(&writer, 1);

    mpack_write_cstr(&writer, kDataResponseSizeKey);
    mpack_write_uint(&writer, response.size);

    mpack_finish_map(&writer);

    if (mpack_writer_error(&writer) == mpack_ok)
    {
        *pBytesWritten = mpack_writer_buffer_used(&writer);

        result = DD_RESULT_SUCCESS;
    }
    else
    {
        DD_PRINT(LogLevel::Warn, "Serialization of RPC data response failed!");
    }

    mpack_writer_destroy(&writer);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT DeserializeDataResponse(RpcDataResponse* pResponse, const void* pBuffer, size_t bufferSize)
{
    DD_RESULT result = DD_RESULT_PARSING_INVALID_MSGPACK;

    mpack_tree_t tree;
    mpack_tree_init_data(&tree, static_cast<const char*>(pBuffer), bufferSize);
    mpack_tree_parse(&tree);
    mpack_node_t root = mpack_tree_root(&tree);

    pResponse->size = mpack_node_u64(mpack_node_map_cstr(root, kDataResponseSizeKey));

    if (mpack_tree_error(&tree) == mpack_ok)
    {
        result = DD_RESULT_SUCCESS;
    }
    else
    {
        DD_PRINT(LogLevel::Warn, "Deserialization of RPC data response failed!");
    }

    mpack_tree_destroy(&tree);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT SerializeTerminatorResponse(const RpcTerminatorResponse& response, void* pBuffer, size_t bufferSize, size_t* pBytesWritten)
{
    DD_RESULT result = DD_RESULT_PARSING_INVALID_MSGPACK;

    mpack_writer_t writer;
    mpack_writer_init(&writer, static_cast<char*>(pBuffer), bufferSize);

    mpack_start_map(&writer, 1);

    mpack_write_cstr(&writer, kTerminatorResponseResultKey);
    mpack_write_uint(&writer, response.result);

    mpack_finish_map(&writer);

    if (mpack_writer_error(&writer) == mpack_ok)
    {
        *pBytesWritten = mpack_writer_buffer_used(&writer);

        result = DD_RESULT_SUCCESS;
    }
    else
    {
        DD_PRINT(LogLevel::Warn, "Serialization of RPC terminator response failed!");
    }

    mpack_writer_destroy(&writer);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT DeserializeTerminatorResponse(RpcTerminatorResponse* pResponse, const void* pBuffer, size_t bufferSize)
{
    DD_RESULT result = DD_RESULT_PARSING_INVALID_MSGPACK;

    mpack_tree_t tree;
    mpack_tree_init_data(&tree, static_cast<const char*>(pBuffer), bufferSize);
    mpack_tree_parse(&tree);
    mpack_node_t root = mpack_tree_root(&tree);

    pResponse->result = static_cast<DD_RESULT>(mpack_node_u64(mpack_node_map_cstr(root, kTerminatorResponseResultKey)));

    if (mpack_tree_error(&tree) == mpack_ok)
    {
        result = DD_RESULT_SUCCESS;
    }
    else
    {
        DD_PRINT(LogLevel::Warn, "Deserialization of RPC terminator response failed!");
    }

    mpack_tree_destroy(&tree);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DDApiVersion RpcServicesQueryVersion()
{
    DDApiVersion version = {};

    version.major = DD_RPC_SERVICES_QUERY_MAJOR_VERSION;
    version.minor = DD_RPC_SERVICES_QUERY_MINOR_VERSION;
    version.patch = DD_RPC_SERVICES_QUERY_PATCH_VERSION;

    return version;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* RpcServicesQueryVersionString()
{
    return DD_RPC_SERVICES_QUERY_VERSION_STRING;
}

} // namespace Rpc

