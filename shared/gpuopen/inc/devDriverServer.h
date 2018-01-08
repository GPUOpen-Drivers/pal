/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  devDriverServer.h
* @brief Class declaration for DevDriverServer.
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"
#include "msgChannel.h"
#include "msgTransport.h"
#include "protocols/systemProtocols.h"
#include "protocols/typemap.h"

namespace DevDriver
{
    class IProtocolServer;

    struct DevDriverServerCreateInfo
    {
        TransportCreateInfo transportCreateInfo;
        ProtocolFlags       enabledProtocols;
    };

    DD_STATIC_CONST uint32 kQueryStatusTimeoutInMs = 50;

#if !DD_VERSION_SUPPORTS(GPUOPEN_DISTRIBUTED_STATUS_FLAGS_VERSION)
    Result QueryDevDriverStatus(const TransportType type, StatusFlags* pFlags, HostInfo *pHostInfo = nullptr);
#endif

    class DevDriverServer
    {
    public:
        static bool IsConnectionAvailable(const TransportType type, uint32 timeout = kQueryStatusTimeoutInMs);

        explicit DevDriverServer(const DevDriverServerCreateInfo& createInfo);
        ~DevDriverServer();

        Result Initialize();
        void Finalize();
        void Destroy();

        bool IsConnected() const;
        IMsgChannel* GetMessageChannel() const;

        LoggingProtocol::LoggingServer* GetLoggingServer();
        SettingsProtocol::SettingsServer* GetSettingsServer();
        DriverControlProtocol::DriverControlServer* GetDriverControlServer();
        RGPProtocol::RGPServer* GetRGPServer();
    private:
        Result InitializeProtocols();
        void DestroyProtocols();

        Result RegisterProtocol(Protocol protocol);
        void UnregisterProtocol(Protocol protocol);
        void FinalizeProtocol(Protocol protocol);

        DevDriverServerCreateInfo   m_createInfo;
        IMsgChannel*                m_pMsgChannel;

        template <Protocol protocol, class ...Args>
        inline Result RegisterProtocol(Args... args);

        template <Protocol protocol>
        inline ProtocolServerType<protocol>* GetServer();
    };

} // DevDriver
