/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "ddPlatform.h"
#include "messageChannel.h"

#include "socketMsgTransport.h"

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

#if   defined(DD_PLATFORM_WINDOWS_KM)
            DD_UNUSED(createInfo);
            // This if block is here for two reasons:
            //      (1) We need to fill this out, and we're going to! This will happen pretty early in
            //          the kernel driver bringup.
            //      (2) Until then, we need the else block below to compile.
            if (true)
            {
                DD_ASSERT_REASON("Message channel is not correctly implemented for Windows KM yet - CreateMessageChannel will fail and return NULL");
            }
// Windows is handled above, so we check posix UM platforms here.
#elif DD_PLATFORM_IS_POSIX
            if ((createInfo.hostInfo.type == TransportType::Remote) |
                (createInfo.hostInfo.type == TransportType::Local))
            {
                using MsgChannelSocket = MessageChannel<SocketMsgTransport>;
                pMsgChannel = DD_NEW(MsgChannelSocket, createInfo.allocCb)(createInfo.allocCb,
                    createInfo.channelInfo,
                    createInfo.hostInfo);
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
