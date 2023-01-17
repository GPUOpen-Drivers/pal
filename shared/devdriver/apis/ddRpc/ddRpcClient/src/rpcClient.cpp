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

#include <rpcClient.h>

#include <ddRpcShared.h>

namespace Rpc
{

/// Simple C++ wrapper class for the DDByteWriter C interface
/// The actual presence of the writer is optional. When given a NULL writer, writes are ignored and always succeed.
///
/// This class drives the provided DDByteWriter with an easier to use interface
class OptionalByteWriterWrapper
{
public:
    explicit OptionalByteWriterWrapper() : m_pWriter(nullptr), m_started(false) {}
    OptionalByteWriterWrapper(const DDByteWriter* pWriter) : m_pWriter(pWriter), m_started(false) {}

    /// Begins a byte writing operation and sets the total data size up-front
    ///
    /// NOTE: This method is optional and may be skipped if the caller isn't aware of the total number of bytes to be
    ///       written up-front.
    DD_RESULT Begin(size_t totalDataSize)
    {
        DD_RESULT result = DD_RESULT_UNKNOWN;

        if (m_started == false)
        {
            if (m_pWriter != nullptr)
            {
                result = m_pWriter->pfnBegin(m_pWriter->pUserdata, &totalDataSize);
            }
            else
            {
                result = DD_RESULT_SUCCESS;
            }

            if (result == DD_RESULT_SUCCESS)
            {
                m_started = true;
            }
        }

        return result;
    }

    /// Writes the provided bytes into the underlying writer
    ///
    /// This method will automatically begin the underlying writer if this is the first write into it
    DD_RESULT Write(const void* pData, size_t dataSize)
    {
        DD_RESULT result = DD_RESULT_SUCCESS;

        if (m_started == false)
        {
            if (m_pWriter != nullptr)
            {
                result = m_pWriter->pfnBegin(m_pWriter->pUserdata, nullptr);
            }
            else
            {
                result = DD_RESULT_SUCCESS;
            }

            if (result == DD_RESULT_SUCCESS)
            {
                m_started = true;
            }
        }

        if (result == DD_RESULT_SUCCESS)
        {
            if (m_pWriter != nullptr)
            {
                result = m_pWriter->pfnWriteBytes(m_pWriter->pUserdata, pData, dataSize);
            }
        }

        return result;
    }

    /// Ends the byte writing operation and closes the underlying writer
    ///
    /// This method MUST be called to finish the write operation!
    void End(DD_RESULT result)
    {
        if (m_pWriter != nullptr)
        {
            m_pWriter->pfnEnd(m_pWriter->pUserdata, result);
        }
    }

    /// If this Writer was initialized with a NULL writer, this will return true
    bool IsEmpty() const { return (m_pWriter == nullptr); }

private:
    const DDByteWriter* m_pWriter;
    bool                m_started;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Helper function used to validate that we're receiving a valid amount of response data from the server
DD_RESULT ValidateResponseData(
    uint64_t dataSize,
    size_t   totalDataSize,
    size_t   totalDataReceived)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    if (dataSize == 0)
    {
        result = DD_RESULT_DD_RPC_CTRL_INVALID_RESPONSE_DATA_SIZE;
    }
    else if (dataSize <= SIZE_MAX)
    {
        const size_t responseDataSize = static_cast<size_t>(dataSize);

        // If we're working with a known response size, make sure this data packet wouldn't be more
        // data than we expect to see
        if ((totalDataSize != 0) && ((totalDataReceived + responseDataSize) > totalDataSize))
        {
            result = DD_RESULT_DD_RPC_CTRL_RESPONSE_SIZE_MISMATCH;
        }
    }
    else
    {
        // Response data size is too large for the current machine
        result = DD_RESULT_DD_RPC_FUNC_RESPONSE_REJECTED;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Helper function used to receive raw response data from the remote server
DD_RESULT ReceiveRawResponseData(
    DDSocket                   hSocket,
    void*                      pBuf,
    size_t                     bufSize,
    size_t                     dataSize,
    OptionalByteWriterWrapper& writer)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    size_t bytesRead = 0;

    // Read all response data from this packet
    while ((result == DD_RESULT_SUCCESS) && (bytesRead < dataSize))
    {
        const size_t bytesRemaining = (dataSize - bytesRead);
        const size_t bytesToRead = DevDriver::Platform::Min(bytesRemaining, bufSize);
        result = ddSocketReceive(hSocket, pBuf, bytesToRead);

        if (result == DD_RESULT_SUCCESS)
        {
            result = writer.Write(pBuf, bytesToRead);
            bytesRead += bytesToRead;

            if (result != DD_RESULT_SUCCESS)
            {
                DD_PRINT(
                    DevDriver::LogLevel::Warn,
                    "Application ByteWriter WriteBytes failed with %s",
                    ddApiResultToString(result));
            }
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
RpcClient::~RpcClient()
{
    ddSocketClose(m_hSocket);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT RpcClient::Init(const DDRpcClientCreateInfo& rpcInfo)
{
    const DDProtocolId protocolId = (rpcInfo.protocolId == DD_API_INVALID_PROTOCOL_ID) ? kDefaultRpcProtocolId : rpcInfo.protocolId;

    DDSocketConnectInfo connectInfo = {};
    connectInfo.hConnection         = rpcInfo.hConnection;
    connectInfo.clientId            = rpcInfo.clientId;
    connectInfo.protocolId          = protocolId;
    connectInfo.timeoutInMs         = rpcInfo.timeoutInMs;

    return ddSocketConnect(&connectInfo, &m_hSocket);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT RpcClient::Call(const DDRpcClientCallInfo& info)
{
    DD_RESULT result;

    // Not all Calls require a response writer, so we support no-oping the writer calls when it's omitted
    // It's an error to receive data while omitting a DDByteWriter, so we'll check this at the end and report appropriately.
    // It's important that we actually use a real writer in the mean time, so that we actually handle all of the data
    // in the Rpc  "transaction". Otherwise, we'd just bail as soon as the payload size comes in.
    OptionalByteWriterWrapper writer(info.pResponseWriter);

    // Request
    {
        RpcRequestHeader request = {};
        request.service          = info.service;
        request.serviceVersion   = info.serviceVersion;
        request.function         = info.function;
        request.paramBufferSize  = info.paramBufferSize;

        size_t bytesWritten = 0;
        result = SerializeRequestHeader(request, m_scratchBuffer, sizeof(m_scratchBuffer), &bytesWritten);

        if (result == DD_RESULT_SUCCESS)
        {
            result = ddSocketSendWithSizePrefix(m_hSocket, m_scratchBuffer, bytesWritten);
        }

        if (result == DD_RESULT_SUCCESS)
        {
            result = ddSocketSend(m_hSocket, info.pParamBuffer, info.paramBufferSize);
        }
    }

    // Response
    uint64_t sizePrefix = 0;
    RpcResponseHeader response = {};

    if (result == DD_RESULT_SUCCESS)
    {
        result = ddSocketReceiveWithSizePrefix(m_hSocket, m_scratchBuffer, sizeof(m_scratchBuffer), &sizePrefix);
    }

    if (result == DD_RESULT_SUCCESS)
    {
        if (sizePrefix <= SIZE_MAX)
        {
            result = DeserializeResponseHeader(&response, m_scratchBuffer, static_cast<size_t>(sizePrefix));
        }
        else
        {
            result = DD_RESULT_COMMON_UNSUPPORTED;
        }
    }

    if (result == DD_RESULT_SUCCESS)
    {
        switch (response.type)
        {
        // If we receive a size indicator sub-packet or response data, then that means the server side
        // has response data to return to us and we'll start receiving it shortly.
        case RpcResponseType::SizeIndicator:
        case RpcResponseType::Data:
        {
            size_t responseSize = 0;

            // The response size is optional and may not always be included
            const bool hasResponseSize = (response.type == RpcResponseType::SizeIndicator);
            if (hasResponseSize)
            {
                // Read the response size from the packet
                RpcSizeIndicatorResponse sizeIndicator = {};
                result = ddSocketReceiveWithSizePrefix(m_hSocket, m_scratchBuffer, sizeof(m_scratchBuffer), &sizePrefix);

                if (result == DD_RESULT_SUCCESS)
                {
                    if (sizePrefix <= SIZE_MAX)
                    {
                        result = DeserializeSizeIndicatorResponse(&sizeIndicator, m_scratchBuffer, static_cast<size_t>(sizePrefix));
                    }
                    else
                    {
                        result = DD_RESULT_COMMON_UNSUPPORTED;
                    }
                }

                if (result == DD_RESULT_SUCCESS)
                {
                    if (sizeIndicator.size <= SIZE_MAX)
                    {
                        responseSize = static_cast<size_t>(sizeIndicator.size);
                    }
                    else
                    {
                        // Response size is too large for the current machine
                        result = DD_RESULT_DD_RPC_FUNC_RESPONSE_REJECTED;
                    }
                }

                if (result == DD_RESULT_SUCCESS)
                {
                    // A known response size should never be zero
                    result = (responseSize != 0) ? DD_RESULT_SUCCESS : DD_RESULT_DD_RPC_CTRL_INVALID_RESPONSE_SIZE;
                }

                if (result == DD_RESULT_SUCCESS)
                {
                    // Since we know the response size, we'll call begin explicitly here.
                    // If this is skipped (because we don't know the full size), the first call to writer.Write() will
                    // correctly handle calling Begin for the underlying DDByteWriter.
                    result = writer.Begin(responseSize);

                    if (result != DD_RESULT_SUCCESS)
                    {
                        DD_PRINT(
                            DevDriver::LogLevel::Warn,
                            "Application ByteWriter Begin failed with %s",
                            ddApiResultToString(result));
                    }
                }
            }

            size_t totalResponseDataReceived = 0;

            if ((result == DD_RESULT_SUCCESS) && (hasResponseSize == false))
            {
                // If we don't have a response size, then this packet actually contains response data so we need to process it
                RpcDataResponse data = {};
                result = ddSocketReceiveWithSizePrefix(m_hSocket, m_scratchBuffer, sizeof(m_scratchBuffer), &sizePrefix);

                if (result == DD_RESULT_SUCCESS)
                {
                    if (sizePrefix <= SIZE_MAX)
                    {
                        result = DeserializeDataResponse(&data, m_scratchBuffer, static_cast<size_t>(sizePrefix));
                    }
                    else
                    {
                        result = DD_RESULT_COMMON_UNSUPPORTED;
                    }
                }

                if (result == DD_RESULT_SUCCESS)
                {
                    result = ValidateResponseData(data.size, responseSize, totalResponseDataReceived);
                }

                if (result == DD_RESULT_SUCCESS)
                {
                    // Write the response data to our writer
                    result = ReceiveRawResponseData(
                        m_hSocket,
                        m_scratchBuffer,
                        sizeof(m_scratchBuffer),
                        static_cast<size_t>(data.size),
                        writer);
                }

                if (result == DD_RESULT_SUCCESS)
                {
                    totalResponseDataReceived += static_cast<size_t>(data.size);
                }
            }

            // Handle all remaining packets until a terminator is encountered
            while (result == DD_RESULT_SUCCESS)
            {
                result = ddSocketReceiveWithSizePrefix(m_hSocket, m_scratchBuffer, sizeof(m_scratchBuffer), &sizePrefix);

                if (result == DD_RESULT_SUCCESS)
                {
                    if (sizePrefix <= SIZE_MAX)
                    {
                        result = DeserializeResponseHeader(&response, m_scratchBuffer, static_cast<size_t>(sizePrefix));
                    }
                    else
                    {
                        result = DD_RESULT_COMMON_UNSUPPORTED;
                    }
                }

                if (result == DD_RESULT_SUCCESS)
                {
                    if (response.type == RpcResponseType::Data)
                    {
                        RpcDataResponse data = {};
                        result = ddSocketReceiveWithSizePrefix(m_hSocket, m_scratchBuffer, sizeof(m_scratchBuffer), &sizePrefix);

                        if (result == DD_RESULT_SUCCESS)
                        {
                            if (sizePrefix <= SIZE_MAX)
                            {
                                result = DeserializeDataResponse(&data, m_scratchBuffer, static_cast<size_t>(sizePrefix));
                            }
                            else
                            {
                                result = DD_RESULT_COMMON_UNSUPPORTED;
                            }
                        }

                        if (result == DD_RESULT_SUCCESS)
                        {
                            result = ValidateResponseData(data.size, responseSize, totalResponseDataReceived);
                        }

                        if (result == DD_RESULT_SUCCESS)
                        {
                            result = ReceiveRawResponseData(
                                m_hSocket,
                                m_scratchBuffer,
                                sizeof(m_scratchBuffer),
                                static_cast<size_t>(data.size),
                                writer);
                        }

                        if (result == DD_RESULT_SUCCESS)
                        {
                            totalResponseDataReceived += static_cast<size_t>(data.size);
                        }
                    }
                    else if (response.type == RpcResponseType::Terminator)
                    {
                        RpcTerminatorResponse terminator = {};
                        result = ddSocketReceiveWithSizePrefix(m_hSocket, m_scratchBuffer, sizeof(m_scratchBuffer), &sizePrefix);

                        if (result == DD_RESULT_SUCCESS)
                        {
                            if (sizePrefix <= SIZE_MAX)
                            {
                                result = DeserializeTerminatorResponse(&terminator, m_scratchBuffer, static_cast<size_t>(sizePrefix));
                            }
                            else
                            {
                                result = DD_RESULT_COMMON_UNSUPPORTED;
                            }
                        }

                        if (result == DD_RESULT_SUCCESS)
                        {
                            result = terminator.result;
                        }

                        if (result == DD_RESULT_SUCCESS)
                        {
                            // Make sure we either didn't know the response size, or the final size matches what we expect.
                            // We only need to do this if the server side claims to have executed successfully
                            result = ((responseSize == 0) || (totalResponseDataReceived == responseSize)) ? DD_RESULT_SUCCESS
                                                                                                          : DD_RESULT_DD_RPC_CTRL_RESPONSE_SIZE_MISMATCH;
                        }

                        writer.End(result);

                        // Break out of the loop since we've reached the end of the response
                        break;
                    }
                    else
                    {
                        // Invalid response type
                        result = DD_RESULT_DD_RPC_CTRL_UNEXPECTED_RESPONSE_TYPE;
                        break;
                    }
                }
            }

            // If we successfully received all of the data here, but were never given a writer...
            if ((result == DD_RESULT_SUCCESS) && (totalResponseDataReceived > 0) && writer.IsEmpty())
            {
                // Mark that as an error - this is not what the caller generally expects.
                result = DD_RESULT_DD_RPC_FUNC_UNEXPECTED_RETURN_DATA;
            }

            break;
        }

        // We've received a terminator from the server.
        // This indicates that no further data will be received in response to this request.
        // This can be received at this time due to an error, or simply because the function doesn't return any data.
        case RpcResponseType::Terminator:
        {
            // The response contains no response data
            RpcTerminatorResponse terminator = {};
            result = ddSocketReceiveWithSizePrefix(m_hSocket, m_scratchBuffer, sizeof(m_scratchBuffer), &sizePrefix);

            if (result == DD_RESULT_SUCCESS)
            {
                if (sizePrefix <= SIZE_MAX)
                {
                    result = DeserializeTerminatorResponse(&terminator, m_scratchBuffer, static_cast<size_t>(sizePrefix));
                }
                else
                {
                    result = DD_RESULT_COMMON_UNSUPPORTED;
                }
            }

            if (result == DD_RESULT_SUCCESS)
            {
                result = terminator.result;
            }

            break;
        }
        default:
        {
            // Invalid response type
            result = DD_RESULT_DD_RPC_CTRL_UNEXPECTED_RESPONSE_TYPE;
            break;
        }
        }
    }

    return result;
}

} // namespace Rpc
