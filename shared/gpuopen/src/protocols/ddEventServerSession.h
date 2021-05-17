/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include <protocols/ddEventProtocol.h>
#include <protocols/ddEventServer.h>
#include <ddTransferManager.h>

namespace DevDriver
{
    namespace EventProtocol
    {
        enum class SessionState
        {
            ReceivePayload = 0,
            ProcessPayload,
            SendPayload,
        };

        class EventServerSession
        {
        public:
            EventServerSession(const AllocCb& allocCb, SharedPointer<ISession> pSession, EventServer* pServer, TransferProtocol::TransferManager* pTransferManager);
            ~EventServerSession();

            void UpdateSession();

        private:
            struct EventChunkInfo
            {
                EventChunk* pChunk;
                size_t      bytesSent;
            };

            // Protocol message handlers
            SessionState HandleQueryProvidersRequest(SizedPayloadContainer& container);
            SessionState HandleAllocateProviderUpdatesRequest(SizedPayloadContainer& container);
            SessionState HandleApplyProviderUpdatesRequest(SizedPayloadContainer& container);

            void SendEventData();

            EventServer*                                 m_pServer;
            SharedPointer<ISession>                      m_pSession;
            AllocCb                                      m_allocCb;
            SizedPayloadContainer                        m_payloadContainer;
            SessionState                                 m_state;
            TransferProtocol::TransferManager*           m_pTransferManager;
            SharedPointer<TransferProtocol::ServerBlock> m_pUpdateBlock;
            SizedPayloadContainer                        m_eventPayloadContainer;
            bool                                         m_eventPayloadPending;
            EventChunkInfo                               m_eventChunkInfo;
        };
    }
}
