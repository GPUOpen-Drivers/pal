/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
                    pName = "Unix Domain Socket";
                    break;
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
