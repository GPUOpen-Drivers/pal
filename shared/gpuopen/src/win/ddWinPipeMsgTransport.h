/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include "msgTransport.h"
#include "ddPlatform.h"

namespace DevDriver
{
    class WinPipeMsgTransport : public IMsgTransport
    {
    public:
        explicit WinPipeMsgTransport(const HostInfo& hostInfo);
        ~WinPipeMsgTransport();

        Result Connect(ClientId* pClientId, uint32 timeoutInMs) override;
        Result Disconnect() override;

        Result ReadMessage(MessageBuffer& messageBuffer, uint32 timeoutInMs) override;
        Result WriteMessage(const MessageBuffer& messageBuffer) override;

        const char* GetTransportName() const override
        {
            return "Named Pipe";
        }

        DD_STATIC_CONST bool RequiresKeepAlive()
        {
            return false;
        }

        DD_STATIC_CONST bool RequiresClientRegistration()
        {
            return true;
        }

        static Result TestConnection(const HostInfo& hostInfo, uint32 timeoutInMs);
    private:
        HANDLE m_pipeHandle;
        char   m_pipeName[kMaxStringLength];

        struct PendingTransaction
        {
            OVERLAPPED oOverlap;
            MessageBuffer message;
            DWORD cbSize;
            bool ioPending;
        };

        PendingTransaction m_readTransaction;
        PendingTransaction m_writeTransaction;
    };

} // DevDriver
