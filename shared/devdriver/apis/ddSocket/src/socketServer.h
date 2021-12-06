/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include <baseProtocolServer.h>
#include <util/vector.h>

struct SocketServerCreateInfo
{
    DevDriver::IMsgChannel* pMsgChannel;
    DevDriver::Protocol     protocol;
    DevDriver::Version      minVersion;
    DevDriver::Version      maxVersion;
    uint32_t                maxPending;
};

class SocketServer : public DevDriver::BaseProtocolServer
{
public:
    SocketServer(const SocketServerCreateInfo& createInfo);
    ~SocketServer();

    // Session handling functions
    bool AcceptSession(const DevDriver::SharedPointer<DevDriver::ISession>& pSession) override;
    void SessionEstablished(const DevDriver::SharedPointer<DevDriver::ISession>& pSession) override;
    void UpdateSession(const DevDriver::SharedPointer<DevDriver::ISession>& pSession) override;
    void SessionTerminated(const DevDriver::SharedPointer<DevDriver::ISession>& pSession, DevDriver::Result terminationReason) override;

    DevDriver::Result AcceptConnection(DevDriver::SharedPointer<DevDriver::ISession>* ppSession, uint32_t timeoutInMs);

private:
    struct PendingConnection
    {
        DevDriver::SharedPointer<DevDriver::ISession> pSession;
    };

    uint32_t                             m_maxPendingConnections;
    DevDriver::Platform::AtomicLock      m_pendingConnectionsLock;
    DevDriver::Vector<PendingConnection> m_pendingConnections;
    DevDriver::Platform::Event           m_hasPendingConnections;

};
