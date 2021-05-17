/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include "baseProtocolServer.h"
#include "protocols/ddTransferProtocol.h"

namespace DevDriver
{
    namespace TransferProtocol
    {
        class TransferManager;

        // The protocol server implementation for the transfer protocol.
        class TransferServer final : public BaseProtocolServer
        {
        public:
            explicit TransferServer(IMsgChannel* pMsgChannel, TransferManager* pTransferManager);
            ~TransferServer();

            void Finalize() override;

            bool AcceptSession(const SharedPointer<ISession>& pSession) override;
            void SessionEstablished(const SharedPointer<ISession>& pSession) override;
            void UpdateSession(const SharedPointer<ISession>& pSession) override;
            void SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason) override;
        private:
            TransferManager* m_pTransferManager;

            class TransferSession;
        };
    }
} // DevDriver
