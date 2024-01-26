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

#include <ddPlatform.h>

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

namespace DevDriver
{
    enum class SocketType : uint32
    {
        Unknown = 0,
        Tcp,
        Udp,
        Local
    };

    /**
    ***********************************************************************************************************************
    * @brief Encapsulates details of socket management for various platforms.
    ***********************************************************************************************************************
    */
    class Socket
    {
    public:
        Socket();

        /// Releases any OS-specific objects if they haven't previously been released in an explicit Destroy() call.
        ~Socket();

        /// Allocates/initializes the OS-specific object representing the socket.  Clients must call this method
        /// before using this object.
        ///
        /// @returns Success if the object was successfully initialized, or ErrorOutOfMemory if allocation of the
        ///          OS-specific object failed.
        Result Init(bool isNonBlocking, SocketType socketType);

        Result Connect(const char* pAddress, uint16 port);

        Result Select(bool* pReadState, bool* pWriteState, bool* pExceptState, uint32 timeoutInMs);

        Result Bind(const char* pAddress, uint16 port);

        Result Listen(uint32 backlog);

        Result Accept(Socket* pClientSocket);

        Result Send(const uint8* pData, size_t dataSize, size_t* pBytesSent);

        Result SendTo(const void* pSockAddr, size_t addrSize, const uint8* pData, size_t dataSize, size_t* pBytesSent);

        Result Receive(uint8* pBuffer, size_t bufferSize, size_t* pBytesReceived);

        Result ReceiveFrom(void *pSockAddr, size_t *addrSize, uint8* pBuffer, size_t bufferSize, size_t* pBytesReceived);

        Result Close();

        Result GetSocketName(char *pAddress, size_t addrLen, uint16* pPort);

        Result LookupAddressInfo(const char* pAddress, uint16 port, size_t addressInfoSize, char* pAddressInfo, size_t *pAddressSize);

    private:
        using OsSocketType = int;

        // When using Unix Domain sockets, we need to save the address to close the socket properly.
        char         m_address[128];
        size_t       m_addressSize;

        OsSocketType m_osSocket;
        bool         m_isNonBlocking;
        SocketType   m_socketType;
        addrinfo     m_hints;

        Result InitAsClient(OsSocketType socket, bool isNonBlocking);
    };

} // DevDriver
