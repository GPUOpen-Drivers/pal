/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <ddLocalNgMsgTransport.h>
#include <ddPlatform.h>
#include <ddDevModeControlCmds.h>

namespace DevDriver
{

// ================================================================================================================
// Tests to see if the client can connect to the utility driver through this transport
// @TODO:  This function can and should be rewritten as to not included an allocCb
Result LocalNgMsgTransport::TestConnection(const AllocCb& allocCb)
{
    DevModeControlDevice device(allocCb);

    Result result = device.Initialize(DevModeBusType::Auto);
    if (result == Result::Success)
    {
        device.Destroy();
    }

    return result;
}

LocalNgMsgTransport::LocalNgMsgTransport(
    const AllocCb& allocCb,
    Component      componentType,
    StatusFlags    initialFlags)
    : m_clientId(kBroadcastClientId)
    , m_componentType(componentType)
    , m_initialClientFlags(initialFlags)
    , m_devModeControlDevice(allocCb)
    , m_allocCb(allocCb)
    , m_sharedQueue()
{
    DD_UNUSED(m_allocCb);
}

LocalNgMsgTransport::~LocalNgMsgTransport()
{
    // We should never be connected while being destroyed. If this triggers, it means the user of this object
    // forgot to call disconnect before deleting.
    DD_ASSERT(IsConnected() == false);
}

Result LocalNgMsgTransport::Connect(ClientId* pClientId, uint32 timeoutInMs)
{
    DD_UNUSED(timeoutInMs);

    Result result = Result::Error;

    if (IsConnected() == false)
    {
        result = m_devModeControlDevice.Initialize(DevModeBusType::Auto);
    }

    if (result == Result::Success)
    {
        result = m_sharedQueue.Initialize(kMaxQueueLength, kMaxMessageSizeInBytes);
    }

    RegisterClientRequest request = {};

    if (result == Result::Success)
    {
        request.input.component           = m_componentType;
        request.input.messageQueueSend    = m_sharedQueue.GetSendQueue();
        request.input.messageQueueReceive = m_sharedQueue.GetReceiveQueue();
        request.input.initialClientFlags  = m_initialClientFlags;

        result = m_devModeControlDevice.MakeDevModeRequest(&request);
        if (result == Result::Success)
        {
            result = request.header.result;
        }
    }

    if (result == Result::Success)
    {
        // Update our internal state
        m_isConnected = true;
        m_clientId    = request.output.clientId;

        // Return the new client id to the caller
        *pClientId = request.output.clientId;

        m_sharedQueue.SetSendQueue(request.output.sendQueue);
        m_sharedQueue.SetReceiveQueue(request.output.receiveQueue);
    }
    else
    {
        // We failed to connect, clean up all intermediate resources

        // Destroy our shared queue object
        m_sharedQueue.Destroy();

        // Destroy the devmode device if we managed to create one
        m_devModeControlDevice.Destroy();
    }

    return result;
}

Result LocalNgMsgTransport::Disconnect()
{
    Result result = Result::Error;

    if (IsConnected())
    {
        UnregisterClientRequest request = {};

        request.input.clientId = m_clientId;

        result = m_devModeControlDevice.MakeDevModeRequest(&request);
        DD_UNHANDLED_RESULT(result);

        if (result == Result::Success)
        {
            result = request.header.result;
            DD_UNHANDLED_RESULT(result);
        }

        m_clientId = kBroadcastClientId;
        m_sharedQueue.Destroy();

        m_devModeControlDevice.Destroy();

        m_isConnected = false;
    }

    return result;
}

Result LocalNgMsgTransport::ReadMessage(MessageBuffer& messageBuffer, uint32 timeoutInMs)
{
    return m_sharedQueue.ReceiveMessage(messageBuffer, timeoutInMs);
}

Result LocalNgMsgTransport::WriteMessage(const MessageBuffer& messageBuffer)
{
    return m_sharedQueue.TransmitMessage(messageBuffer, kTransmitTimeoutInMs);
}

} // DevDriver
