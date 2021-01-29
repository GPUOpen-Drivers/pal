/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file messageChannel.cpp
* @brief Implementation file for message channel code.
***********************************************************************************************************************
*/

#include "ddPlatform.h"
#include "messageChannel.h"

#define DD_SUPPORT_SOCKET_TRANSPORT ((DD_PLATFORM_WINDOWS_UM && DEVDRIVER_BUILD_REMOTE_TRANSPORT) || (DD_PLATFORM_IS_POSIX))

#if DD_SUPPORT_SOCKET_TRANSPORT
    #include "socketMsgTransport.h"
#endif

#if defined(DD_PLATFORM_WINDOWS_UM)
    #include "win/ddWinPipeMsgTransport.h"
#endif

namespace DevDriver
{
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Tests for the presence of a connection using the specified parameters
    Result QueryConnectionAvailable(const HostInfo& hostInfo, uint32 timeoutInMs)
    {
        Result result = Result::Unavailable;

#if defined(DD_PLATFORM_WINDOWS_UM)
        if (hostInfo.type == TransportType::Local)
        {
            result = WinPipeMsgTransport::TestConnection(hostInfo, timeoutInMs);
        }
        else if (hostInfo.type == TransportType::Remote)
        {
#if DD_SUPPORT_SOCKET_TRANSPORT
            result = SocketMsgTransport::TestConnection(hostInfo, timeoutInMs);
#endif
        }
#elif DD_PLATFORM_IS_POSIX
        if ((hostInfo.type == TransportType::Remote) |
            (hostInfo.type == TransportType::Local))
        {
#if DD_SUPPORT_SOCKET_TRANSPORT
            result = SocketMsgTransport::TestConnection(hostInfo, timeoutInMs);
#endif
        }
#endif
        else
        {
            // Invalid transport type
            DD_WARN_REASON("Invalid transport type specified");
        }

        return result;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Create a new message channel object
    Result CreateMessageChannel(const MessageChannelCreateInfo2& createInfo, IMsgChannel** ppMessageChannel)
    {
        Result result = Result::InvalidParameter;

        IMsgChannel* pMsgChannel = nullptr;

        if (ppMessageChannel != nullptr)
        {
            result = Result::InsufficientMemory;

            // Make sure we have reasonable allocator functions before we try to use them
            DD_ASSERT(createInfo.allocCb.pfnAlloc != nullptr);
            DD_ASSERT(createInfo.allocCb.pfnFree != nullptr);

#if defined(DD_PLATFORM_WINDOWS_UM)
            if (createInfo.hostInfo.type == TransportType::Local)
            {
                using MsgChannelPipe = MessageChannel<WinPipeMsgTransport>;
                pMsgChannel = DD_NEW(MsgChannelPipe, createInfo.allocCb)(createInfo.allocCb,
                    createInfo.channelInfo,
                    createInfo.hostInfo);
            }
            else if (createInfo.hostInfo.type == TransportType::Remote)
            {
#if DD_SUPPORT_SOCKET_TRANSPORT
                using MsgChannelSocket = MessageChannel<SocketMsgTransport>;
                pMsgChannel = DD_NEW(MsgChannelSocket, createInfo.allocCb)(createInfo.allocCb,
                    createInfo.channelInfo,
                    createInfo.hostInfo);
#endif
            }
#elif DD_PLATFORM_IS_POSIX
            if ((createInfo.hostInfo.type == TransportType::Remote) |
                (createInfo.hostInfo.type == TransportType::Local))
            {
#if DD_SUPPORT_SOCKET_TRANSPORT
                using MsgChannelSocket = MessageChannel<SocketMsgTransport>;
                pMsgChannel = DD_NEW(MsgChannelSocket, createInfo.allocCb)(createInfo.allocCb,
                    createInfo.channelInfo,
                    createInfo.hostInfo);
#endif
            }
#endif
            else
            {
                // Invalid transport type
                DD_WARN_REASON("Invalid transport type specified");
            }

            if (pMsgChannel != nullptr)
            {
                result = Result::Success;
            }
        }

        if (result == Result::Success)
        {
            *ppMessageChannel = pMsgChannel;
        }

        return result;
    }
}
