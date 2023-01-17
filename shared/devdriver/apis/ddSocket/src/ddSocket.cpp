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

#include <ddSocket.h>
#include <gpuopen.h>
#include <ddCommon.h>
#include <msgChannel.h>
#include <socketServer.h>

using namespace DevDriver;

/// Timeout used to make sure blocking calls don't spend too long blocked since it makes latency significantly higher
/// TODO: Find some way to improve this in the future, but we need to do this for now to avoid poor performance
///       The additional cpu overhead shouldn't be too big of a deal since we're already talkings tens of ms.
///       This is likely only an issue because the session state often changes inside devdriver without notifying the
///       threads waiting on semaphores.
constexpr uint32_t kRetryTimeoutMs = 250;

/// Total number of retry attempts before an operation should be considered a failure
/// This is used to prevent the blocking calls from blocking "forever".
constexpr uint32_t kMaxRetryCount = 8;

/// Default maximum number of pending connections to allow for ddSocketListen
constexpr uint32_t kDefaultMaxPendingConnections = 8;

// Enumerates the possible types of sockets returned by the API
enum class SocketType : uint32_t
{
    Unknown = 0,
    Client,
    Server
};

// A helper structure that sits in front of the actual socket-type specific data in memory
// This is used to identify which type of a socket is passed into the API by the caller since the handles are the same
struct SocketStub
{
    IMsgChannel* pMsgChannel;
    SocketType   type;
};

// Small helper structure that contains the state for a client socket
struct ClientSocketContext
{
    SocketStub              stub;
    SharedPointer<ISession> pSession;
    size_t                  payloadCacheOffset;
    size_t                  payloadCacheSize;
    uint8_t                 payloadCache[kMaxPayloadSizeInBytes];

    ClientSocketContext(IMsgChannel* pMsgChannel)
        : stub({ pMsgChannel, SocketType::Client })
        , payloadCacheOffset(0)
        , payloadCacheSize(0)
    {
    }
};

// Small helper structure that contains the state for a server socket
struct ServerSocketContext
{
    SocketStub    stub;
    SocketServer  server;

    ServerSocketContext(const SocketServerCreateInfo& createInfo)
        : stub({ createInfo.pMsgChannel, SocketType::Server })
        , server(createInfo)
    {
    }

    ~ServerSocketContext()
    {
        // Remove the server from the message channel before we allow its memory to be destroyed
        DD_UNHANDLED_RESULT(stub.pMsgChannel->UnregisterProtocolServer(&server));
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Helper function to unwrap the DDSocket into a client socket
/// Inspects the input parameter and either returns a valid pointer via the output parameter or an error code via the
/// return value.
DD_RESULT ExtractClientHandle(
    DDSocket              hSocket,
    ClientSocketContext** ppContext)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if ((hSocket != nullptr) && (ppContext != nullptr))
    {
        if (reinterpret_cast<const SocketStub*>(hSocket)->type == SocketType::Client)
        {
            *ppContext = reinterpret_cast<ClientSocketContext*>(hSocket);
            result = DD_RESULT_SUCCESS;
        }
        else
        {
            result = DD_RESULT_NET_SOCKET_TYPE_UNSUPPORTED;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Helper function to unwrap the DDSocket into a server socket
/// Inspects the input parameter and either returns a valid pointer via the output parameter or an error code via the
/// return value.
DD_RESULT ExtractServerHandle(
    DDSocket              hSocket,
    ServerSocketContext** ppContext)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if ((hSocket != nullptr) && (ppContext != nullptr))
    {
        if (reinterpret_cast<const SocketStub*>(hSocket)->type == SocketType::Server)
        {
            *ppContext = reinterpret_cast<ServerSocketContext*>(hSocket);
            result = DD_RESULT_SUCCESS;
        }
        else
        {
            result = DD_RESULT_NET_SOCKET_TYPE_UNSUPPORTED;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DDApiVersion ddSocketQueryVersion()
{
    DDApiVersion version = {};

    version.major = DD_SOCKET_API_MAJOR_VERSION;
    version.minor = DD_SOCKET_API_MINOR_VERSION;
    version.patch = DD_SOCKET_API_PATCH_VERSION;

    return version;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* ddSocketQueryVersionString()
{
    return DD_SOCKET_API_VERSION_STRING;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddSocketConnect(
    const DDSocketConnectInfo* pInfo,
    DDSocket*                  phSocket)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if ((pInfo != nullptr)                                &&
        (pInfo->hConnection != nullptr)                   &&
        (pInfo->clientId != DD_API_INVALID_CLIENT_ID)     &&
        (pInfo->protocolId != DD_API_INVALID_PROTOCOL_ID) &&
        (phSocket != nullptr))
    {
        const DDSocketConnectInfo& info = *pInfo;

        IMsgChannel* pMsgChannel = FromHandle(info.hConnection);

        EstablishSessionInfo sessionInfo = {};
        sessionInfo.protocol           = static_cast<Protocol>(info.protocolId); // TODO: Filter out reserved protocols?
        sessionInfo.minProtocolVersion = static_cast<Version>(info.legacy.versionRange.min);
        sessionInfo.maxProtocolVersion = static_cast<Version>(info.legacy.versionRange.max);
        sessionInfo.remoteClientId     = info.clientId;

        SharedPointer<ISession> pSession;
        result = DevDriverToDDResult(pMsgChannel->EstablishSessionForClient(&pSession, sessionInfo));
        if (result == DD_RESULT_SUCCESS)
        {
            // Wait for the connection to complete
            const uint32_t timeoutInMs = (info.timeoutInMs == 0) ? kDefaultConnectionTimeoutMs : info.timeoutInMs;
            result = DevDriverToDDResult(pSession->WaitForConnection(timeoutInMs));
        }

        if (result == DD_RESULT_SUCCESS)
        {
            ClientSocketContext* pContext = DD_NEW(ClientSocketContext, pMsgChannel->GetAllocCb())(pMsgChannel);
            if (pContext != nullptr)
            {
                pContext->pSession = pSession;

                *phSocket = reinterpret_cast<DDSocket>(pContext);
            }
            else
            {
                result = DD_RESULT_COMMON_OUT_OF_HEAP_MEMORY;
            }
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddSocketListen(
    const DDSocketListenInfo* pInfo,
    DDSocket*                 phSocket)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if ((pInfo != nullptr)              &&
        (pInfo->hConnection != nullptr) &&
        (phSocket != nullptr))
    {
        const DDSocketListenInfo& info = *pInfo;

        IMsgChannel* pMsgChannel = FromHandle(info.hConnection);

        SocketServerCreateInfo createInfo = {};
        createInfo.pMsgChannel = pMsgChannel;
        createInfo.protocol    = static_cast<Protocol>(info.protocolId);
        createInfo.maxVersion  = static_cast<Version>(info.legacy.versionRange.min);
        createInfo.minVersion  = static_cast<Version>(info.legacy.versionRange.max);
        createInfo.maxPending  = (info.maxPending != 0) ? info.maxPending : kDefaultMaxPendingConnections;

        ServerSocketContext* pContext = DD_NEW(ServerSocketContext, pMsgChannel->GetAllocCb())(createInfo);
        if (pContext != nullptr)
        {
            result = DevDriverToDDResult(pMsgChannel->RegisterProtocolServer(&pContext->server));

            if (result == DD_RESULT_SUCCESS)
            {
                *phSocket = reinterpret_cast<DDSocket>(pContext);
            }
            else
            {
                DD_DELETE(pContext, pMsgChannel->GetAllocCb());
            }
        }
        else
        {
            result = DD_RESULT_COMMON_OUT_OF_HEAP_MEMORY;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddSocketAccept(
    DDSocket  hListenSocket,
    uint32_t  timeoutInMs,
    DDSocket* phNewSocket)
{
    ServerSocketContext* pServerContext = nullptr;
    DD_RESULT result = ExtractServerHandle(hListenSocket, &pServerContext);

    if ((result == DD_RESULT_SUCCESS) &&
        (phNewSocket != nullptr))
    {
        DevDriver::SharedPointer<DevDriver::ISession> pSession;
        result = DevDriverToDDResult(pServerContext->server.AcceptConnection(&pSession, timeoutInMs));
        if (result == DD_RESULT_SUCCESS)
        {
            ClientSocketContext* pClientContext =
                DD_NEW(ClientSocketContext, pServerContext->stub.pMsgChannel->GetAllocCb())(
                    pServerContext->stub.pMsgChannel);

            if (pClientContext != nullptr)
            {
                pClientContext->pSession = pSession;

                *phNewSocket = reinterpret_cast<DDSocket>(pClientContext);
            }
            else
            {
                result = DD_RESULT_COMMON_OUT_OF_HEAP_MEMORY;
            }
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddSocketSendRaw(
    DDSocket    hSocket,
    const void* pData,
    size_t      dataSize,
    uint32_t    timeoutInMs,
    size_t*     pBytesSent)
{
    ClientSocketContext* pClientContext = nullptr;
    DD_RESULT result = ExtractClientHandle(hSocket, &pClientContext);

    if (result == DD_RESULT_SUCCESS)
    {
        const bool areParametersValid =
            (((dataSize == 0) || (pData != nullptr)) && // No data to send, or we have data to send plus a valid buffer
             (pBytesSent != nullptr));                  // Output bytes sent pointer is valid

        if (areParametersValid == false)
        {
            result = DD_RESULT_COMMON_INVALID_PARAMETER;
        }
    }

    if (result == DD_RESULT_SUCCESS)
    {
        size_t bytesSent = 0;
        size_t bytesRemaining = dataSize;

        const uint64 startTime = Platform::GetCurrentTimeInMs();

        result = DD_RESULT_SUCCESS;

        uint32 timeoutRemaining = timeoutInMs;

        // Write as much of the chunk into packets as we can
        while ((bytesRemaining > 0) && (result == DD_RESULT_SUCCESS))
        {
            const uint32_t bytesToSend = Platform::Min(static_cast<uint32>(bytesRemaining), kMaxPayloadSizeInBytes);
            const void* pDataPtr = VoidPtrInc(pData, bytesSent);

            result = DevDriverToDDResult(pClientContext->pSession->Send(bytesToSend, pDataPtr, timeoutRemaining));

            if (result == DD_RESULT_SUCCESS)
            {
                bytesSent += bytesToSend;
                bytesRemaining = dataSize - bytesSent;

                const uint64 elapsedTime = (Platform::GetCurrentTimeInMs() - startTime);
                if (elapsedTime < timeoutRemaining)
                {
                    timeoutRemaining -= static_cast<uint32>(elapsedTime);
                }
                else if (bytesRemaining > 0)
                {
                    // The timeout has expired, return the to caller.
                    result = DD_RESULT_DD_GENERIC_NOT_READY;
                }
            }
        }

        if ((bytesSent > 0) && (result == DD_RESULT_DD_GENERIC_NOT_READY))
        {
            result = DD_RESULT_SUCCESS;
        }

        if (result == DD_RESULT_SUCCESS)
        {
            *pBytesSent = bytesSent;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddSocketReceiveRaw(
    DDSocket hSocket,
    void*    pBuffer,
    size_t   bufferSize,
    uint32_t timeoutInMs,
    size_t*  pBytesReceived)
{
    ClientSocketContext* pClientContext = nullptr;
    DD_RESULT result = ExtractClientHandle(hSocket, &pClientContext);

    if (result == DD_RESULT_SUCCESS)
    {
        const bool areParametersValid =
            (((bufferSize == 0) || (pBuffer != nullptr)) && // No data to receive, or we have data to receive plus a valid buffer
             (pBytesReceived != nullptr));                  // Output bytes received pointer is valid

        if (areParametersValid == false)
        {
            result = DD_RESULT_COMMON_INVALID_PARAMETER;
        }
    }

    if (result == DD_RESULT_SUCCESS)
    {
        size_t bytesReceived = 0;
        size_t bytesRemaining = bufferSize;

        result = DD_RESULT_SUCCESS;

        // Check if we have cached data from a prior call that hasn't been returned to the caller yet
        // and buffer space to fill.
        if ((bytesRemaining > 0) && (pClientContext->payloadCacheSize > 0))
        {
            // Return as many bytes of cached data as we can
            const size_t bytesToReceive = Platform::Min(bufferSize, pClientContext->payloadCacheSize);
            const void* pDataPtr = VoidPtrInc(pClientContext->payloadCache, pClientContext->payloadCacheOffset);
            memcpy(pBuffer, pDataPtr, bytesToReceive);

            // Update the input buffer + size
            bytesReceived += bytesToReceive;
            bytesRemaining = bufferSize - bytesReceived;

            // Update the payload cache state
            pClientContext->payloadCacheOffset += bytesToReceive;
            pClientContext->payloadCacheSize = pClientContext->payloadCacheSize - bytesToReceive;

            // We don't have to worry about clearing the payload cache offset to 0 when we consume all
            // the data here since it'll be handled the next time the cache is used.
        }

        if (bytesRemaining >= kMaxPayloadSizeInBytes)
        {
            // We must always read in max payload size byte increments since the underlying session system is payload based rather
            // than byte based.
            void* pDataPtr = VoidPtrInc(pBuffer, bytesReceived);

            uint32 curBytesReceived = 0;
            result = DevDriverToDDResult(pClientContext->pSession->Receive(kMaxPayloadSizeInBytes, pDataPtr, &curBytesReceived, timeoutInMs));

            if (result == DD_RESULT_SUCCESS)
            {
                bytesReceived += curBytesReceived;
                bytesRemaining = bufferSize - bytesReceived;
            }
        }
        else if (bytesRemaining > 0)
        {
            // We have to do a partial read here since the caller's buffer isn't large enough to fit a full size payload in it.
            // We handle this by reading to an internal cache first and then copying the partial results back to the caller after.

            // We should always have an empty payload cache at this point or it means it wasn't properly consumed at the beginning of the function.
            DD_ASSERT(pClientContext->payloadCacheSize == 0);

            uint32 curBytesReceived = 0;
            result = DevDriverToDDResult(pClientContext->pSession->Receive(kMaxPayloadSizeInBytes, pClientContext->payloadCache, &curBytesReceived, timeoutInMs));

            if (result == DD_RESULT_SUCCESS)
            {
                // We've successfully read a payload into our local cache, now we need to transfer it from the cache back to the caller.
                pClientContext->payloadCacheSize += curBytesReceived;
                pClientContext->payloadCacheOffset = 0;

                // Return as many bytes of cached data as we can
                // We don't need to apply the payload cache offset here since we know it's always 0 in this case
                const size_t bytesToReceive = Platform::Min(bytesRemaining, pClientContext->payloadCacheSize);
                void* pDataPtr = VoidPtrInc(pBuffer, bytesReceived);
                memcpy(pDataPtr, pClientContext->payloadCache, bytesToReceive);

                bytesReceived += bytesToReceive;
                bytesRemaining = bufferSize - bytesReceived;

                // Update the payload cache state
                pClientContext->payloadCacheOffset += bytesToReceive;
                pClientContext->payloadCacheSize = pClientContext->payloadCacheSize - bytesToReceive;

                // We should only leave this loop with a valid cache payload and no bytes remaining in the caller's buffer, or
                // an empty cache payload and space left in the caller's buffer.
                // If this wasn't the case, we'd need to handle the possibility of a valid cache payload in the regular receive loop code.
                DD_ASSERT((bytesRemaining == 0) || (pClientContext->payloadCacheSize == 0));
            }
        }
        else
        {
            // We have no space left in our buffer because we either filled it entirely from our payload cache,
            // or there was never any room to start with.
            DD_ASSERT(bytesRemaining == 0);
        }

        if (result == DD_RESULT_SUCCESS)
        {
            *pBytesReceived = bytesReceived;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddSocketSend(
    DDSocket    hSocket,
    const void* pData,
    size_t      dataSize)
{
    DD_RESULT result = DD_RESULT_SUCCESS;
    const auto* pBytes = static_cast<const uint8_t*>(pData);
    size_t bytesLeft = dataSize;

    size_t retryCount = 0;
    while ((result == DD_RESULT_SUCCESS) && (bytesLeft > 0))
    {
        size_t bytesSent = 0;

        result = ddSocketSendRaw(hSocket, pBytes, bytesLeft, kRetryTimeoutMs, &bytesSent);

        if (result == DD_RESULT_SUCCESS)
        {
            pBytes += bytesSent;
            bytesLeft -= bytesSent;
        }
        else if ((result == DD_RESULT_DD_GENERIC_NOT_READY) && (retryCount < kMaxRetryCount))
        {
            // Retry when a timeout is encountered and we haven't met our max retry count
            result = DD_RESULT_SUCCESS;
            ++retryCount;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddSocketReceive(
    DDSocket hSocket,
    void*    pBuffer,
    size_t   bufferSize)
{
    DD_RESULT result = DD_RESULT_SUCCESS;
    auto* pBytes = static_cast<uint8_t*>(pBuffer);
    size_t bytesLeft = bufferSize;

    size_t retryCount = 0;
    while ((result == DD_RESULT_SUCCESS) && (bytesLeft > 0))
    {
        size_t bytesRecv = 0;

        result = ddSocketReceiveRaw(hSocket, pBytes, bytesLeft, kRetryTimeoutMs, &bytesRecv);

        if (result == DD_RESULT_SUCCESS)
        {
            pBytes += bytesRecv;
            bytesLeft -= bytesRecv;
        }
        else if ((result == DD_RESULT_DD_GENERIC_NOT_READY) && (retryCount < kMaxRetryCount))
        {
            // Retry when a timeout is encountered and we haven't met our max retry count
            result = DD_RESULT_SUCCESS;
            ++retryCount;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddSocketSendWithSizePrefix(
    DDSocket    hSocket,
    const void* pData,
    size_t      dataSize)
{
    const uint64_t sizePrefix = static_cast<uint64_t>(dataSize);

    DD_RESULT result = ddSocketSend(hSocket, &sizePrefix, sizeof(sizePrefix));
    if (result == DD_RESULT_SUCCESS)
    {
        result = ddSocketSend(hSocket, pData, dataSize);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddSocketReceiveWithSizePrefix(
    DDSocket  hSocket,
    void*     pBuffer,
    size_t    bufferSize,
    uint64_t* pSizePrefix)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if (pSizePrefix != nullptr)
    {
        uint64_t sizePrefix = 0;

        result = ddSocketReceive(hSocket, &sizePrefix, sizeof(sizePrefix));
        if (result == DD_RESULT_SUCCESS)
        {
            if (sizePrefix <= SIZE_MAX)
            {
                if (sizePrefix <= bufferSize)
                {
                    result = ddSocketReceive(hSocket, pBuffer, static_cast<size_t>(sizePrefix));

                    if (result == DD_RESULT_SUCCESS)
                    {
                        *pSizePrefix = sizePrefix;
                    }
                }
                else
                {
                    result = DD_RESULT_COMMON_BUFFER_TOO_SMALL;
                }
            }
            else
            {
                // Large size prefixes are not supported on 32-bit systems
                result = DD_RESULT_COMMON_UNSUPPORTED;
            }
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ddSocketClose(
    DDSocket hSocket)
{
    if (hSocket != nullptr)
    {
        switch (reinterpret_cast<const SocketStub*>(hSocket)->type)
        {
        case SocketType::Client:
        {
            ClientSocketContext* pContext = reinterpret_cast<ClientSocketContext*>(hSocket);
            DD_DELETE(pContext, pContext->stub.pMsgChannel->GetAllocCb());
            break;
        }
        case SocketType::Server:
        {
            ServerSocketContext* pContext = reinterpret_cast<ServerSocketContext*>(hSocket);
            DD_DELETE(pContext, pContext->stub.pMsgChannel->GetAllocCb());
            break;
        }
        default:
        {
            DD_ASSERT_REASON("Invalid socket type");
            break;
        }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t ddSocketQueryProtocolVersion(
    DDSocket hSocket)
{
    uint32_t protocolVersion = 0;

    if ((hSocket != nullptr) && (reinterpret_cast<const SocketStub*>(hSocket)->type == SocketType::Client))
    {
        ClientSocketContext* pContext = reinterpret_cast<ClientSocketContext*>(hSocket);
        protocolVersion = static_cast<uint32_t>(pContext->pSession->GetVersion());
    }

    return protocolVersion;
}
