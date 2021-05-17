/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

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
