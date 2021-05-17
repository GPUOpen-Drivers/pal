/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include "protocolServer.h"

namespace DevDriver
{
    class IMsgChannel;

    class BaseProtocolServer : public IProtocolServer
    {
    public:
        virtual ~BaseProtocolServer();

        Protocol GetProtocol() const override final { return m_protocol; };
        SessionType GetType() const override final { return SessionType::Server; };
        Version GetMinVersion() const override final { return m_minVersion; };
        Version GetMaxVersion() const override final { return m_maxVersion; };

        bool GetSupportedVersion(Version minVersion, Version maxVersion, Version * version) const override final;

        virtual void Finalize() override;
    protected:
        BaseProtocolServer(IMsgChannel* pMsgChannel, Protocol protocol, Version minVersion, Version maxVersion);

        // Helper functions for working with SizedPayloadContainers
        Result SendPayload(ISession* pSession, const SizedPayloadContainer* pPayload, uint32 timeoutInMs);
        Result ReceivePayload(ISession* pSession, SizedPayloadContainer* pPayload, uint32 timeoutInMs);

        IMsgChannel* const m_pMsgChannel;
        const Protocol m_protocol;
        const Version m_minVersion;
        const Version m_maxVersion;

        bool m_isFinalized;
    };

} // DevDriver
