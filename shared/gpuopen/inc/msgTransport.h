/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include "gpuopen.h"

namespace DevDriver
{
    class IMsgTransport
    {
    public:
        virtual ~IMsgTransport() {}

        // Connect and disconnect from the transport.
        virtual Result Connect(ClientId* pClientId, uint32 timeoutInMs) = 0;
        virtual Result Disconnect() = 0;

        // Read and Write messages from a connected transport
        virtual Result WriteMessage(const MessageBuffer &messageBuffer) = 0;
        virtual Result ReadMessage(MessageBuffer &messageBuffer, uint32 timeoutInMs) = 0;

        // Get a human-readable string describing the connection type.
        virtual const char* GetTransportName() const = 0;

        // Static method to be implemented by individual transports
        // true indicates that the transport is incapable of detecting
        //   dropped connections and some form of keep-alive is required
        // false indicates that the transport can properly detect dropped
        //   connections
        DD_STATIC_CONST bool RequiresKeepAlive()
        {
            return false;
        }

        // Static method to be implemented by individual transports
        // true indicates that Connect is expected to also negotiate a client ID
        // false indicates that the MessageChannel needs to do it's own client ID
        //   negotiation, e.g. in the case of network connections
        DD_STATIC_CONST bool RequiresClientRegistration()
        {
            return false;
        }
    protected:
        IMsgTransport() {}
    };

} // DevDriver
