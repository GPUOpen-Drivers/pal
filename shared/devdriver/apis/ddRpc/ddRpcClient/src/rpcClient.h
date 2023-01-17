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

#pragma once

#include <ddRpcClient.h>

#include <ddCommon.h>

#include <ddSocket.h>

namespace Rpc
{

/// Size of the internal scratch buffer used by the RpcClient
static constexpr size_t kClientScratchBufferSize = 64 * 1024;

/// Class responsible for managing the client side implementation of the RPC protocol
///
/// Services hosted by a remote server can be interacted with via the Call function
class RpcClient
{
public:
    /// Extracts a pointer to this object from an API handle
    static RpcClient* FromHandle(DDRpcClient hClient)
    {
        assert(hClient != DD_API_INVALID_HANDLE);
        return reinterpret_cast<RpcClient*>(hClient);
    }

    /// Returns this object casted to an API handle type
    DDRpcClient Handle() { return reinterpret_cast<DDRpcClient>(this); }

    RpcClient() {}
    ~RpcClient();

    /// Connects the client to the provided remote server and prepares it for use
    ///
    /// This must be successfully called before Call is used
    DD_RESULT Init(const DDRpcClientCreateInfo& info);

    /// Attempts to perform a remote procedure call on the connected server
    ///
    /// This object must be successfully initialized before calling this
    DD_RESULT Call(const DDRpcClientCallInfo& info);

private:
    DDSocket m_hSocket = DD_API_INVALID_HANDLE;

    // Internal scratch buffer used to hold flatbuffer encoded control messages
    // This buffer also acts as an intermediate scratchpad for transferring large amounts of response data back to the
    // caller from the network via DDByteWriter.
    uint8_t m_scratchBuffer[kClientScratchBufferSize];
};

} // namespace Rpc
