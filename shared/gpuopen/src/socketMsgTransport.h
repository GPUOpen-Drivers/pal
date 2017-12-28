/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
/**
***********************************************************************************************************************
* @file  socketMsgTransport.h
* @brief Class declaration for SocketMsgTransport
***********************************************************************************************************************
*/

#pragma once

#include "msgTransport.h"
#include "socket.h"

namespace DevDriver
{
    class SocketMsgTransport : public IMsgTransport
    {
    public:
        explicit SocketMsgTransport(const TransportCreateInfo& createInfo);
        ~SocketMsgTransport();

        Result Connect(ClientId* pClientId, uint32 timeoutInMs) override;
        Result Disconnect() override;

        Result ReadMessage(MessageBuffer &messageBuffer, uint32 timeoutInMs) override;
        Result WriteMessage(const MessageBuffer &messageBuffer) override;

#if !DD_VERSION_SUPPORTS(GPUOPEN_DISTRIBUTED_STATUS_FLAGS_VERSION)
        Result UpdateClientStatus(ClientId clientId, StatusFlags flags) override;
        static Result QueryStatus(TransportType type, StatusFlags* pFlags, uint32 timeoutInMs, HostInfo* pHostInfo = nullptr);
#endif

        static Result TestConnection(TransportType type, uint32 timeoutInMs, HostInfo* pHostInfo = nullptr);

        DD_STATIC_CONST bool RequiresKeepAlive()
        {
            return true;
        }

        DD_STATIC_CONST bool RequiresClientRegistration()
        {
            return true;
        }

    private:
        Socket              m_clientSocket;
        bool                m_connected;
        HostInfo            m_hostInfo;
        const SocketType    m_socketType;
    };

} // DevDriver
