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
