/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "socketMsgTransport.h"
#include "protocols/systemProtocols.h"
#include "ddPlatform.h"

using namespace DevDriver::ClientManagementProtocol;

namespace DevDriver
{
    // Take a TransportType and find the associated SocketType for the current platform
    inline static SocketType TransportToSocketType(TransportType type)
    {
        SocketType result = SocketType::Unknown;
        switch (type)
        {
        case TransportType::Local:
            result = SocketType::Local;
            break;
        case TransportType::Remote:
            result = SocketType::Udp;
            break;
        default:
            DD_WARN_REASON("Invalid transport type specified");
            break;
        }
        return result;
    }

    SocketMsgTransport::SocketMsgTransport(const HostInfo& hostInfo) :
        m_connected(false),
        m_socketType(TransportToSocketType(hostInfo.type))
    {
        if ((m_socketType != SocketType::Udp) && (m_socketType != SocketType::Local))
        {
            DD_ASSERT_REASON("Unsupported socket type provided");
        }

        // Only UDP/remote sockets have valid hostname fields
        if (m_socketType == SocketType::Udp)
        {
            Platform::Strncpy(m_hostname, hostInfo.pHostname);
        }
        else
        {
            DD_ASSERT(hostInfo.pHostname == nullptr);
        }

        m_port = hostInfo.port;
    }

    SocketMsgTransport::~SocketMsgTransport()
    {
        Disconnect();
    }

    Result SocketMsgTransport::Connect(ClientId* pClientId, uint32 timeoutInMs)
    {
        DD_UNUSED(timeoutInMs);
        DD_UNUSED(pClientId);

        // Attempt to connect to the remote host.
        Result result = Result::Error;

        if (!m_connected)
        {
            result = m_clientSocket.Init(true, m_socketType);

            if (result == Result::Success)
            {
                // Bind with no host info will bind our local side of the socket to a random port that is capable of
                // receiving from any address.
                result = m_clientSocket.Bind(nullptr, 0);
            }

            if (result == Result::Success)
            {
                // Only UDP/Remote socket types have a valid hostname to connect to
                // Local sockets use the address as a prefix instead
                const char* pAddress = (m_socketType == SocketType::Udp) ? m_hostname : "AMD-Developer-Service";
                result = m_clientSocket.Connect(pAddress, m_port);
            }
            m_connected = (result == Result::Success);
        }
        return result;
    }

    Result SocketMsgTransport::Disconnect()
    {
        Result result = Result::Error;
        if (m_connected)
        {
            m_connected = false;
            result = m_clientSocket.Close();
        }
        return result;
    }

    Result SocketMsgTransport::ReadMessage(MessageBuffer &messageBuffer, uint32 timeoutInMs)
    {
        bool canRead = m_connected;
        bool exceptState = true;
        Result result = Result::Success;

        if (canRead & (timeoutInMs > 0))
        {
            result = m_clientSocket.Select(&canRead, nullptr, &exceptState, timeoutInMs);
        }

        if (result == Result::Success)
        {
            if (canRead)
            {
                size_t bytesReceived;
                result = m_clientSocket.Receive(reinterpret_cast<uint8*>(&messageBuffer), sizeof(MessageBuffer), &bytesReceived);

                if (result == Result::Success)
                {
                    result = ValidateMessageBuffer(&messageBuffer, bytesReceived);
                }
            }
            else if (exceptState)
            {
                result = Result::Error;
            }
            else
            {
                result = Result::NotReady;
            }
        }
        return result;
    }

    Result SocketMsgTransport::WriteMessage(const MessageBuffer &messageBuffer)
    {
        Result result = Result::Error;

        if (m_connected)
        {
            if (messageBuffer.header.payloadSize <= kMaxPayloadSizeInBytes)
            {
                const size_t totalMsgSize = (sizeof(MessageHeader) + messageBuffer.header.payloadSize);

                size_t bytesSent = 0;
                result = m_clientSocket.Send(reinterpret_cast<const uint8*>(&messageBuffer), totalMsgSize, &bytesSent);

                if (result == Result::Success)
                {
                    result = (bytesSent == totalMsgSize) ? Result::Success : Result::Error;
                }
            }
        }

        return result;
    }

    // ================================================================================================================
    // Tests to see if the client can connect to RDS through this transport
    Result SocketMsgTransport::TestConnection(const HostInfo& hostInfo, uint32 timeoutInMs)
    {
        Result result = Result::Error;
        Socket clientSocket;
        const SocketType sType = TransportToSocketType(hostInfo.type);

        if (sType != SocketType::Unknown)
        {
            result = clientSocket.Init(true, sType);

            if (result == Result::Success)
            {
                // Bind with no host info will bind our local side of the socket to a random port that is capable of
                // receiving from any address.
                result = clientSocket.Bind(nullptr, 0);

                // If we were able to bind to a socket we the connect to the remote host/port specified
                if (result == Result::Success)
                {
                    // Only UDP/Remote socket types have a valid hostname to connect to
                    // Local sockets use the address as a prefix instead
                    const char* pAddress = (sType == SocketType::Udp) ? hostInfo.pHostname : "AMD-Developer-Service";
                    result = clientSocket.Connect(pAddress, hostInfo.port);
                }

                // If we made it this far we need to actually make sure we can actually communicate with the remote host
                if (result == Result::Success)
                {
                    // In order to test connectivity we are going to manually send a KeepAlive message. This message
                    // is discarded by both clients and RDS, making it safe to use for this purpose
                    MessageBuffer message = kOutOfBandMessage;
                    message.header.messageId = static_cast<MessageCode>(ManagementMessage::KeepAlive);

                    // Transmit the KeepAlive packet
                    size_t bytesWritten = 0;
                    result = clientSocket.Send(
                        reinterpret_cast<const uint8 *>(&message),
                        sizeof(message.header),
                        &bytesWritten);

                    if (result == Result::Success)
                    {
                        // Wait until a response is waiting
                        bool canRead = false;
                        bool exceptState = false;
                        result = clientSocket.Select(&canRead, nullptr, &exceptState, timeoutInMs);
                        if ((result == Result::Success) & (canRead) & (!exceptState))
                        {
                            // read the response
                            MessageBuffer responseMessage = {};
                            size_t bytesReceived;
                            result = clientSocket.Receive(reinterpret_cast<uint8 *>(&responseMessage), sizeof(MessageBuffer), &bytesReceived);

                            // Check to make sure we got the response + that the response is the expected size
                            // KeepAlive is defined as having no additional payload, so it will only ever be the size of a header
                            if ((result == Result::Success) & (bytesReceived == sizeof(responseMessage.header)))
                            {
                                // Since we received a response, we know there is a server. An invalid packet here means that either
                                // the remote server didn't understand the request or that there was a logical bug on the server.
                                // In either case we treat this as a version mismatch since we can't tell the difference.
                                result = Result::VersionMismatch;

                                // @TODO: If we receive a regular broadcast packet here, we should ignore it instead of assuming that
                                //        we have a version mismatch here.

                                // check packet validity and set success if true
                                if (IsOutOfBandMessage(responseMessage) &&
                                    IsValidOutOfBandMessage(responseMessage) &&
                                    (responseMessage.header.messageId == static_cast<MessageCode>(ManagementMessage::KeepAlive)))
                                {
                                    result = Result::Success;
                                }
                            }
                        }
                    }
                }
                clientSocket.Close();
            }
        }
        return result;
    }
} // DevDriver
