/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  ddLocalNgMsgTransport.h
* @brief Class declaration for LocalNgMsgTransport
***********************************************************************************************************************
*/

#pragma once

#include <ddDevModeControlDevice.h>
#include <ddDevModeQueue.h>
#include <msgTransport.h>

namespace DevDriver
{
    class LocalNgMsgTransport : public IMsgTransport
    {
    public:
        explicit LocalNgMsgTransport(
            const AllocCb& allocCb,
            Component      componentType,
            StatusFlags    initialFlags);
        ~LocalNgMsgTransport();

        Result Connect(ClientId* pClientId, uint32 timeoutInMs) override;
        Result Disconnect() override;

        Result ReadMessage(MessageBuffer& messageBuffer, uint32 timeoutInMs) override;
        Result WriteMessage(const MessageBuffer& messageBuffer) override;

        const char* GetTransportName() const override
        {
            return "Local Ng";
        }

        DD_STATIC_CONST bool RequiresKeepAlive()
        {
            return false;
        }

        DD_STATIC_CONST bool RequiresClientRegistration()
        {
            return false;
        }

        static Result TestConnection(const AllocCb& allocCb);
    private:
        bool IsConnected() const { return m_isConnected; }

        ClientId             m_clientId;
        Component            m_componentType;
        StatusFlags          m_initialClientFlags;
        DevModeControlDevice m_devModeControlDevice;
        AllocCb              m_allocCb;
        SharedQueue          m_sharedQueue;
        bool                 m_isConnected;

        DD_STATIC_CONST uint32 kTransmitTimeoutInMs = 50;
        DD_STATIC_CONST uint32 kReceiveTimeoutInMs = 50;
    };

} // DevDriver
