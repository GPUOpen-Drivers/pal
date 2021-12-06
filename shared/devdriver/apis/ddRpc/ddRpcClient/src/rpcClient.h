/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

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
