/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include <msgTransport.h>
#include <ddAbstractSocket.h>

namespace DevDriver
{
    class SocketMsgTransport : public IMsgTransport
    {
    public:
        explicit SocketMsgTransport(const HostInfo& hostInfo);
        ~SocketMsgTransport();

        Result Connect(ClientId* pClientId, uint32 timeoutInMs) override;
        Result Disconnect() override;

        Result ReadMessage(MessageBuffer& messageBuffer, uint32 timeoutInMs) override;
        Result WriteMessage(const MessageBuffer& messageBuffer) override;

        const char* GetTransportName() const override
        {
            const char *pName = "Unknown";
            switch (m_socketType)
            {
                case SocketType::Tcp:
                    pName = "TCP Socket";
                    break;
                case SocketType::Udp:
                    pName = "UDP Socket";
                    break;
                case SocketType::Local:
#if !defined(DD_PLATFORM_WINDOWS_UM)
                    pName = "Unix Domain Socket";
                    break;
#endif
                default:
                    break;
            }

            return pName;
        }

        static Result TestConnection(const HostInfo& connectionInfo, uint32 timeoutInMs);

        DD_STATIC_CONST bool RequiresKeepAlive()
        {
            return true;
        }

        DD_STATIC_CONST bool RequiresClientRegistration()
        {
            return true;
        }

    private:
        Socket           m_clientSocket;
        bool             m_connected;
        char             m_hostname[kMaxStringLength];
        uint16           m_port;
        const SocketType m_socketType;
    };

} // DevDriver
