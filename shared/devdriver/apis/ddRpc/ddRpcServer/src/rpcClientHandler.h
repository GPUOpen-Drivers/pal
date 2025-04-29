/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <rpcServer.h>

#include <ddSocket.h>
#include <ddRpcShared.h>

#include <ddPlatform.h>
#include <util/vector.h>

namespace Rpc
{

/// Size of the internal scratch buffer used by the client handler
///
/// This buffer doesn't need to be very large since it's only used to hold serialized RPC messages
/// We don't know EXACTLY how large the max size is since it's depends on the serialization logic, so we just
/// select something that "should" be large enough. If this assumptions turns out to be wrong, we'll simply end up
/// with serialization failures and the problem should be caught during early testing.
///
/// The size requirements only change when this RPC control logic changes. This is not affected by the content
/// being carried over RPC.
static constexpr size_t kHandlerScratchBufferSize = 256;

/// Class responsible for handling communication with an individual network client on behalf of the server
class RpcClientHandler
{
public:
    RpcClientHandler(RpcServer* pServer, DDSocket hSocket);
    ~RpcClientHandler();

    // Initializes the internal state of the object and prepares it for use
    // This method launches a thread internally which services the needs of the client over the network
    DD_RESULT Initialize();

    // Returns true if this client is still considered "active"
    //
    // A client becomes "inactive" when it has stopped processing messages from the network
    // This can happen because an error was encountered, or because the client on the other side disconnected
    bool IsActive() const { return m_isActive; }

private:
    // A function that runs on its own thread and manages all network IO with the client
    void ClientThreadFunc();

    // Helper function that forwards a request to the associated server with the provided parameters
    // for execution.
    DD_RESULT ExecuteRequest(
        DDRpcServiceId  serviceId,
        DDApiVersion    serviceVersion,
        DDRpcFunctionId functionId,
        const void*     pParameterData,
        size_t          parameterDataSize);

    // Helper functions for the byte writer that's used to write response data back to the client
    // These functions manage any internal control messages that need to be sent in addition to the actual
    // response data returned by function execution.
    DD_RESULT WriterBegin(const size_t* pTotalDataSize);
    DD_RESULT WriterWriteBytes(const void* pData, size_t dataSize);
    void WriterEnd(DD_RESULT result);

    // Pointer back to the associated RPC server object
    RpcServer* m_pServer;

    // Socket object associated with the client that this object is responsible for
    DDSocket m_hSocket;

    // Thread used to perform client network operations on
    DevDriver::Platform::Thread m_thread;

    // Used to track whether or not this object still represents a valid client connection
    bool m_isActive;

    // Internal scratch buffer used to hold encoded control messages
    uint8_t m_scratchBuffer[kHandlerScratchBufferSize];
};

} // namespace Rpc
