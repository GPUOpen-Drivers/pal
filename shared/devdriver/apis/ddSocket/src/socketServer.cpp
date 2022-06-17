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

#include <socketServer.h>
#include <msgChannel.h>

using namespace DevDriver;

SocketServer::SocketServer(
    const SocketServerCreateInfo& createInfo)
    : BaseProtocolServer(createInfo.pMsgChannel, createInfo.protocol, createInfo.minVersion, createInfo.maxVersion)
    , m_maxPendingConnections(createInfo.maxPending)
    , m_pendingConnections(createInfo.pMsgChannel->GetAllocCb())
    , m_hasPendingConnections(false)
{
}

SocketServer::~SocketServer()
{
}

//////////////// Session Handling Functions ////////////////////////
bool SocketServer::AcceptSession(const SharedPointer<ISession>& pSession)
{
    DD_UNUSED(pSession);

    Platform::LockGuard<Platform::AtomicLock> lock(m_pendingConnectionsLock);
    return (static_cast<uint32>(m_pendingConnections.Size()) < m_maxPendingConnections);
}

void SocketServer::SessionEstablished(const SharedPointer<ISession>& pSession)
{
    Platform::LockGuard<Platform::AtomicLock> lock(m_pendingConnectionsLock);
    PendingConnection connection = {};
    connection.pSession = pSession;

    // TODO: Do we care about handling allocation failure here? It will just result in the session being ignored.
    m_pendingConnections.PushBack(connection);

    m_hasPendingConnections.Signal();
}

void SocketServer::UpdateSession(const SharedPointer<ISession>& pSession)
{
    // Do nothing
    DD_UNUSED(pSession);
}

void SocketServer::SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason)
{
    DD_UNUSED(terminationReason);

    // Remove the session from the pending connections list if it closes before being consumed by the application
    Platform::LockGuard<Platform::AtomicLock> lock(m_pendingConnectionsLock);
    for (auto connectionIter = m_pendingConnections.Begin(); connectionIter != m_pendingConnections.End(); ++connectionIter)
    {
        if (connectionIter->pSession == pSession)
        {
            m_pendingConnections.Remove(connectionIter);
            break;
        }
    }

    if (m_pendingConnections.IsEmpty())
    {
        m_hasPendingConnections.Clear();
    }
}

Result SocketServer::AcceptConnection(
    SharedPointer<ISession>* ppSession,
    uint32_t                 timeoutInMs)
{
    DD_ASSERT(ppSession != nullptr);

    Result result = m_hasPendingConnections.Wait(timeoutInMs);
    if (result == Result::Success)
    {
        Platform::LockGuard<Platform::AtomicLock> lock(m_pendingConnectionsLock);
        PendingConnection connection = {};
        if (m_pendingConnections.PopFront(&connection))
        {
            // Return the pending connection's session to the caller
            *ppSession = connection.pSession;
        }
        else
        {
            // This can happen since we don't wait while holding the lock.
            // This is intentional since waiting while holding the lock could allow the app to stall
            // the background update thread for a signficiant amount of time.
            // In the case where the pending connection we were waiting for is removed by the time we
            // acquire the lock, we just pretend we timed out. This could cause unexpected timing behavior
            // at the app level, but it seems like the best option.
            result = Result::NotReady;
        }
    }

    return result;
}
