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

#include <rpcClientHandler.h>
#include <ddPlatform.h>
#include <ddCommon.h>
#include <rpcServer.h>

#include <ddRpcShared.h>

using namespace DevDriver;

namespace Rpc
{

/// Maximum size allowed for incoming function parameter data
///
/// The server code will reject any requests from the client that use more parameter data than this limit specifies
/// This value is currently compile-time only. We'd like to make this configurable at run-time in the future: (#46)
static constexpr size_t kMaxParameterDataSize = 256 * 1024 * 1024;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
RpcClientHandler::RpcClientHandler(
    RpcServer* pServer,
    DDSocket   hSocket)
    : m_pServer(pServer)
    , m_hSocket(hSocket)
    , m_isActive(true)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
RpcClientHandler::~RpcClientHandler()
{
    if (m_thread.IsJoinable())
    {
        DD_UNHANDLED_RESULT(m_thread.Join(1000));
    }

    ddSocketClose(m_hSocket);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT RpcClientHandler::Initialize()
{
    return DevDriverToDDResult(m_thread.Start([](void* pUserdata) {
        RpcClientHandler* pThis = reinterpret_cast<RpcClientHandler*>(pUserdata);
        pThis->ClientThreadFunc();
    },
    this));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void RpcClientHandler::ClientThreadFunc()
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    DevDriver::Vector<uint8_t> requestParamBuffer(Platform::GenericAllocCb);

    while (result == DD_RESULT_SUCCESS)
    {
        uint64_t sizePrefix = 0;
        RpcRequestHeader header = {};

        result = ddSocketReceiveWithSizePrefix(m_hSocket, m_scratchBuffer, sizeof(m_scratchBuffer), &sizePrefix);
        if (result == DD_RESULT_SUCCESS)
        {
            if (sizePrefix <= SIZE_MAX)
            {
                result = DeserializeRequestHeader(&header, m_scratchBuffer, static_cast<size_t>(sizePrefix));
            }
            else
            {
                result = DD_RESULT_COMMON_UNSUPPORTED;
            }

            if ((result == DD_RESULT_SUCCESS) && (header.paramBufferSize > 0))
            {
                if (header.paramBufferSize <= SIZE_MAX)
                {
                    if (header.paramBufferSize <= kMaxParameterDataSize)
                    {
                        requestParamBuffer.Resize(static_cast<size_t>(header.paramBufferSize));

                        result = ddSocketReceive(
                            m_hSocket,
                            requestParamBuffer.Data(),
                            requestParamBuffer.Size());

                        // Clear out the parameter buffer if we fail to receive all the data for it
                        if (result != DD_RESULT_SUCCESS)
                        {
                            requestParamBuffer.Clear();
                        }
                    }
                    else
                    {
                        // The parameter data is too large, reject the request
                        result = DD_RESULT_DD_RPC_FUNC_PARAM_TOO_LARGE;
                    }
                }
                else
                {
                    result = DD_RESULT_COMMON_UNSUPPORTED;
                }
            }

            if (result == DD_RESULT_SUCCESS)
            {
                const DD_RESULT requestResult = ExecuteRequest(
                    header.service,
                    header.serviceVersion,
                    header.function,
                    requestParamBuffer.IsEmpty() ? nullptr : requestParamBuffer.Data(),
                    requestParamBuffer.Size()
                );

                // Clear the parameter buffer as soon as the service finishes using it
                requestParamBuffer.Clear();

                // Send a terminator back to the client to mark the end of the operation
                RpcResponseHeader response = {};
                response.type = RpcResponseType::Terminator;

                size_t bytesWritten = 0;
                result = SerializeResponseHeader(response, m_scratchBuffer, sizeof(m_scratchBuffer), &bytesWritten);

                if (result == DD_RESULT_SUCCESS)
                {
                    result = ddSocketSendWithSizePrefix(m_hSocket, m_scratchBuffer, bytesWritten);
                }

                if (result == DD_RESULT_SUCCESS)
                {
                    RpcTerminatorResponse terminator = {};
                    terminator.result = requestResult;

                    result = SerializeTerminatorResponse(terminator, m_scratchBuffer, sizeof(m_scratchBuffer), &bytesWritten);
                }

                if (result == DD_RESULT_SUCCESS)
                {
                    result = ddSocketSendWithSizePrefix(m_hSocket, m_scratchBuffer, bytesWritten);
                }
            }
        }
        else if (result == DD_RESULT_DD_GENERIC_NOT_READY)
        {
            // If we time out waiting for a request on the server thread, it just means the connected client hasn't
            // issued any requests. Change the result to a success since this is an expected scenario.
            result = DD_RESULT_SUCCESS;
        }
    }

    // Regardless of why we're exiting the processing thread, this client is now considered "inactive"
    m_isActive = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT RpcClientHandler::ExecuteRequest(
    DDRpcServiceId  serviceId,
    DDApiVersion    serviceVersion,
    DDRpcFunctionId functionId,
    const void*     pParameterData,
    size_t          parameterDataSize)
{
    // Create a writer to pass to the registered function during execution
    // This writer will attempt to write any incoming bytes into the network stream immediately. It will block if the
    // send window is full which provides back pressure at the app level. This avoids the need for intermediate buffers.
    // Control message housekeeping logic is also performed in the writer's begin function.
    DDByteWriter writer = {};
    writer.pUserdata = this;
    writer.pfnBegin = [](void* pUserdata, const size_t* pTotalDataSize) -> DD_RESULT {
        return static_cast<RpcClientHandler*>(pUserdata)->WriterBegin(pTotalDataSize);
    };
    writer.pfnWriteBytes = [](void* pUserdata, const void* pData, size_t dataSize) -> DD_RESULT {
        return static_cast<RpcClientHandler*>(pUserdata)->WriterWriteBytes(pData, dataSize);
    };
    writer.pfnEnd = [](void* pUserdata, DD_RESULT result) {
        static_cast<RpcClientHandler*>(pUserdata)->WriterEnd(result);
    };

    // Attempt function execution
    // We pass the parameter data into the function here if we received any from the client earlier
    return m_pServer->ExecuteRequest(
        serviceId,
        serviceVersion,
        functionId,
        pParameterData,
        parameterDataSize,
        writer);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT RpcClientHandler::WriterBegin(const size_t* pTotalDataSize)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    if (pTotalDataSize != nullptr)
    {
        // This response provides the data size up front so we should use the response size indicator sub-packet
        RpcResponseHeader response = {};
        response.type = RpcResponseType::SizeIndicator;

        size_t bytesWritten = 0;
        result = SerializeResponseHeader(response, m_scratchBuffer, sizeof(m_scratchBuffer), &bytesWritten);

        if (result == DD_RESULT_SUCCESS)
        {
            result = ddSocketSendWithSizePrefix(m_hSocket, m_scratchBuffer, bytesWritten);
        }

        if (result == DD_RESULT_SUCCESS)
        {
            RpcSizeIndicatorResponse indicator = {};
            indicator.size = static_cast<uint64_t>(*pTotalDataSize);

            result = SerializeSizeIndicatorResponse(indicator, m_scratchBuffer, sizeof(m_scratchBuffer), &bytesWritten);
        }

        if (result == DD_RESULT_SUCCESS)
        {
            result = ddSocketSendWithSizePrefix(m_hSocket, m_scratchBuffer, bytesWritten);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT RpcClientHandler::WriterWriteBytes(const void* pData, size_t dataSize)
{
    RpcResponseHeader response = {};
    response.type = RpcResponseType::Data;

    size_t bytesWritten = 0;
    DD_RESULT result = SerializeResponseHeader(response, m_scratchBuffer, sizeof(m_scratchBuffer), &bytesWritten);

    if (result == DD_RESULT_SUCCESS)
    {
        result = ddSocketSendWithSizePrefix(m_hSocket, m_scratchBuffer, bytesWritten);
    }

    if (result == DD_RESULT_SUCCESS)
    {
        RpcDataResponse data = {};
        data.size = static_cast<uint64_t>(dataSize);

        result = SerializeDataResponse(data, m_scratchBuffer, sizeof(m_scratchBuffer), &bytesWritten);
    }

    if (result == DD_RESULT_SUCCESS)
    {
        result = ddSocketSendWithSizePrefix(m_hSocket, m_scratchBuffer, bytesWritten);
    }

    if (result == DD_RESULT_SUCCESS)
    {
        result = ddSocketSend(m_hSocket, pData, dataSize);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void RpcClientHandler::WriterEnd(DD_RESULT result)
{
    DD_UNUSED(result);

    // Do nothing
}

} // namespace Rpc
