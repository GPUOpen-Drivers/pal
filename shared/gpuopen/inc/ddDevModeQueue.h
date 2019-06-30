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
/**
***********************************************************************************************************************
* @file  ddDevModeQueue.h
* @brief Header file for the developer mode queue
***********************************************************************************************************************
*/
#pragma once

#include <gpuopen.h>

namespace DevDriver
{

DD_STATIC_CONST uint32 kMaxQueueLength = 256;

////////////////////////////
// Common definition of a shared buffer used
// for message passing
struct SharedBuffer
{
    // Shared buffer handle
    Handle hSharedBufferObject;

    // address of buffer in memory
    Handle hSharedBufferView;
};

DD_CHECK_SIZE(SharedBuffer, 16);

//////////////////////////
// Common defintion of a message handler
// This is the resources associated with a
// message handler - associated with a client/process.
struct QueueInfo
{
    // struct to hold all parameters to the shared buffer
    SharedBuffer sharedBuffer;

    // Semaphores to control
    //    Write -> signaled when queue is available to write into, blocks when full
    //    Read -> signaled when queue has data to read, blocks when empty
    Handle       hSemWrite;
    Handle       hSemRead;

    // size of buffer
    Size         bufferSize;

    // offset (in bytes) of the first message within the shared buffer
    Size         messageOffset;

    // queue length / message size is agreed upon by client/server during registration
    Size         queueLength;
    Size         queueMessageSize;
};

DD_CHECK_SIZE(QueueInfo, 48);

//////////////////////////
// Common defintion of a message channel
// This defines the set of message queues
// (send and receive) and the local thread
// for processing the in-comming messages.
class SharedQueue
{
public:
    SharedQueue();
    ~SharedQueue();

    uint32 QueryReceiveCount() const;
    uint32 QueryTransmitCount() const;
    uint32 QueryTransmitFailureCount() const;
    uint32 QueryReceiveFailureCount() const;

    Result Initialize(uint32 queueLength, uint32 queueMessageSize);
    void Destroy();

    ////////////////////////////
    // Send a message from the given clientID
    Result TransmitMessage(const MessageBuffer &messageBuffer, const uint32 timeout);
    Result ReceiveMessage(MessageBuffer &messageBuffer, const uint32 timeout);

    const QueueInfo &GetSendQueue() const { return m_sendQueue; };
    const QueueInfo &GetReceiveQueue() const { return m_receiveQueue; };

    void SetSendQueue(QueueInfo &queue) { m_sendQueue = queue; };
    void SetReceiveQueue(QueueInfo &queue) { m_receiveQueue = queue; };

    static size_t GetHeaderSize();
protected:
    QueueInfo m_sendQueue;
    QueueInfo m_receiveQueue;
};

}
