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

#include "protocols/etwServer.h"
#include "msgChannel.h"

#include "util/queue.h"
#include "util/vector.h"
#include "win/ddWinEtwServerSession.h"

#include "ddPlatform.h"

#include <thread>

namespace DevDriver
{
    namespace ETWProtocol
    {
        ETWServer::ETWServer(IMsgChannel* pMsgChannel)
            : BaseProtocolServer(pMsgChannel, Protocol::ETW, kVersion, kVersion)
        {
            DD_ASSERT(m_pMsgChannel != nullptr);
        }

        ETWServer::~ETWServer()
        {
        }

        void ETWServer::Finalize()
        {
        }

        bool ETWServer::AcceptSession(const SharedPointer<ISession>& pSession)
        {
            DD_UNUSED(pSession);
            return true;
        }

        void ETWServer::SessionEstablished(const SharedPointer<ISession>& pSession)
        {
            // Allocate session data for the newly established session
            ETWSession *pSessionObject = DD_NEW(ETWSession, m_pMsgChannel->GetAllocCb())(pSession, m_pMsgChannel->GetAllocCb());
            if (pSessionObject != nullptr)
            {
                // Starting with RS5, we need to constantly listen for ETW events for AssociateContext events.
                DD_UNHANDLED_RESULT(pSessionObject->BeginTrace(kAssocationContextProcessId));
                pSession->SetUserData(pSessionObject);
            }
            else
            {
                DD_ASSERT_REASON("Out of memory - DD_NEW returned NULL.");
            }
        }

        void ETWServer::UpdateSession(const SharedPointer<ISession>& pSession)
        {
            ETWSession* pSessionObject = reinterpret_cast<ETWSession*>(pSession->GetUserData());
            if (pSessionObject != nullptr)
            {
                pSessionObject->UpdateSession();
            }
        }

        void ETWServer::SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason)
        {
            DD_UNUSED(terminationReason);
            ETWSession* pSessionObject = reinterpret_cast<ETWSession*>(pSession->SetUserData(nullptr));

            if (pSessionObject != nullptr)
            {
                DD_UNHANDLED_RESULT(pSessionObject->EndTrace());
                DD_DELETE(pSessionObject, m_pMsgChannel->GetAllocCb());
            }
        }

        bool ETWServer::QueryETWSupported()
        {
            return TraceSession::QueryETWSupport();
        }
    } // ETWProtocol
} // DevDriver
