/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include "gpuopen.h"
#include "protocolSession.h"

namespace DevDriver
{
    class Session;

    class IProtocolClient : public IProtocolSession
    {
    public:
        virtual ~IProtocolClient() {}

        virtual Version GetSessionVersion() const = 0;

        virtual Result Connect(ClientId clientId, uint32 timeoutInMs) = 0;
        virtual void Disconnect() = 0;

        virtual bool IsConnected() const = 0;
        virtual ClientId GetRemoteClientId() const = 0;

        virtual bool QueryConnectionStatus() = 0;
    protected:
        IProtocolClient() {}
    };

} // DevDriver
