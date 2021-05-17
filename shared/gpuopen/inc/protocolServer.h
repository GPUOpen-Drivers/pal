/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include "gpuopen.h"
#include "protocolSession.h"

namespace DevDriver
{
    class IMsgChannel;
    class Session;

    class IProtocolServer : public IProtocolSession
    {
    public:
        virtual ~IProtocolServer() {}

        virtual void Finalize() = 0;

        virtual bool GetSupportedVersion(Version minVersion, Version maxVersion, Version *version) const = 0;
        virtual bool AcceptSession(const SharedPointer<ISession>& pSession) = 0;

        virtual void SessionEstablished(const SharedPointer<ISession> &pSession) = 0;
        virtual void UpdateSession(const SharedPointer<ISession> &pSession) = 0;
        virtual void SessionTerminated(const SharedPointer<ISession> &pSession, Result terminationReason) = 0;
    protected:
        IProtocolServer() {}
    };

} // DevDriver
