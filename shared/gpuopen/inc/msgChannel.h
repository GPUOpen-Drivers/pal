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
* @file  msgChannel.h
* @brief Interface declaration for IMsgChannel
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"

namespace DevDriver
{
    class IProtocolClient;
    class IProtocolServer;

    namespace TransferProtocol
    {
        class TransferManager;
    }

    namespace URIProtocol
    {
        class URIService;
    }

    DD_STATIC_CONST uint32 kDefaultUpdateTimeoutInMs = 10;
    DD_STATIC_CONST uint32 kFindClientTimeout        = 500;

    class IMsgChannel
    {
    public:
        virtual ~IMsgChannel() {}

        virtual Result Register(uint32 timeoutInMs = ~(0u)) = 0;
        virtual Result Unregister() = 0;
        virtual bool IsConnected() = 0;

        virtual void Update(uint32 timeoutInMs = kDefaultUpdateTimeoutInMs) = 0;

        virtual Result Send(ClientId dstClientId,
                            Protocol protocol,
                            MessageCode message,
                            const ClientMetadata& metadata,
                            uint32 payloadSizeInBytes,
                            const void* pPayload) = 0;
        virtual Result Receive(MessageBuffer& message, uint32 timeoutInMs) = 0;
        virtual Result Forward(const MessageBuffer& messageBuffer) = 0;

        virtual Result EstablishSession(ClientId dstClientId, IProtocolClient* pClient) = 0;

        virtual Result RegisterProtocolServer(IProtocolServer* pServer) = 0;
        virtual Result UnregisterProtocolServer(IProtocolServer* pServer) = 0;
        virtual IProtocolServer* GetProtocolServer(Protocol protocol) = 0;

        virtual Result SetStatusFlags(StatusFlags flags) = 0;
        virtual StatusFlags GetStatusFlags() const = 0;

        virtual ClientId GetClientId() const = 0;

        virtual const ClientInfoStruct& GetClientInfo() const = 0;

        virtual Result FindFirstClient(const ClientMetadata& filter,
                                       ClientId*             pClientId,
                                       uint32                timeoutInMs     = kFindClientTimeout,
                                       ClientMetadata*       pClientMetadata = nullptr) = 0;

        virtual const AllocCb& GetAllocCb() const = 0;

        virtual TransferProtocol::TransferManager& GetTransferManager() = 0;

        virtual Result RegisterService(URIProtocol::URIService* pService) = 0;
        virtual Result UnregisterService(URIProtocol::URIService* pService) = 0;

        template <ClientStatusFlags flag>
        Result SetStatusFlag(bool enable)
        {
            Result toggleResult = Result::Success;
            StatusFlags oldFlags = GetStatusFlags();
            StatusFlags newFlags;

            if (enable)
            {
                // Toggle developer mode
                newFlags = oldFlags | static_cast<DevDriver::StatusFlags>(flag);
            } else
            {
                // Toggle developer mode
                newFlags = oldFlags & ~static_cast<DevDriver::StatusFlags>(flag);
            }

            if (newFlags != oldFlags)
            {
                toggleResult = SetStatusFlags(newFlags);
            }
            return toggleResult;
        }

        template <ClientStatusFlags flag>
        bool GetStatusFlag() const
        {
            return ((GetStatusFlags() & static_cast<StatusFlags>(flag)) != 0);
        }

    protected:
        IMsgChannel() {}
    };

} // DevDriver
