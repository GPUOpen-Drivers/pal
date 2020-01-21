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
* @file  listener.h
* @brief Interface declaration for IListener
***********************************************************************************************************************
*/

#pragma once

#include <gpuopen.h>
#include <ddPlatform.h>

namespace DevDriver
{
    class IListener;

    // Flags for configuring the listener behavior
    union ListenerConfigFlags
    {
        struct
        {
            uint32 enableKernelTransport : 1;  // Enables a special transport that allows clients to communicate across
                                               // the user mode / kernel mode boundary
            uint32 enableServer          : 1;  // Enables the built-in listener server which allows the listener to
                                               // communicate at an application protocol level with other clients on
                                               // the bus
            uint32 enableEmbeddedClient  : 1;  // Enables the kernel version of the built-in listener server
            uint32 reserved              : 29; // Reserved for future usage
        };
        uint32     value;
    };

    // An address and port pair that the listener can listen for connections on
    struct ListenerBindAddress
    {
        char   hostAddress[kMaxStringLength]; // Network host address
        uint32 port;                          // Network port
    };

    // Creation information for the built in listener server.
    struct ListenerServerCreateInfo
    {
        ProtocolFlags enabledProtocols;
    };

    // Creation information for the listener object
    struct ListenerCreateInfo
    {
        char                     description[kMaxStringLength];   // Description string used to identify the listener on the message bus
        ListenerConfigFlags      flags;                           // Configuration flags
        ListenerServerCreateInfo serverCreateInfo;                // Creation information for the built in listener server
        ListenerBindAddress*     pAddressesToBind;                // A list of addresses to lister for connections on
        uint32                   numAddresses;                    // The number of entries in pAddressesToBind
        AllocCb                  allocCb;                         // An allocation callback that is used to manage memory allocations
        char                     localHostname[kMaxStringLength]; // Hostname for the local listener transport
    };

    // Create a new listener object
    Result CreateListener(const ListenerCreateInfo& createInfo, IListener** ppListener);

    class IListener
    {
    public:
        virtual ~IListener() {}

        virtual void Destroy() = 0;

    protected:
        IListener() {};
    };

} // DevDriver
