/*
 *******************************************************************************
 *
 * Copyright (c) 2016-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

/**
***********************************************************************************************************************
* @file  ddTransferServer.h
* @brief Class declaration for TransferServer.
***********************************************************************************************************************
*/

#pragma once

#include "baseProtocolServer.h"
#include "protocols/systemProtocols.h"
#include "util/hashMap.h"

namespace DevDriver
{
    namespace TransferProtocol
    {
        class LocalBlock;

        // The protocol server implementation for the transfer protocol.
        class TransferServer : public BaseProtocolServer
        {
        public:
            explicit TransferServer(IMsgChannel* pMsgChannel);
            ~TransferServer();

            void Finalize() override;

            bool AcceptSession(const SharedPointer<ISession>& pSession) override;
            void SessionEstablished(const SharedPointer<ISession>& pSession) override;
            void UpdateSession(const SharedPointer<ISession>& pSession) override;
            void SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason) override;

            // Adds a local block to the list of registered blocks.
            void RegisterLocalBlock(const SharedPointer<LocalBlock>& pLocalBlock);

            // Removes a local block from the list of registered blocks.
            void UnregisterLocalBlock(const SharedPointer<LocalBlock>& pLocalBlock);

        private:
            // Mutex used for synchronizing the registered blocks list.
            Platform::Mutex m_mutex;

            // A list of all the local blocks that currently exist on this client.
            HashMap<BlockId, SharedPointer<LocalBlock>, 16> m_registeredLocalBlocks;
        };
    }
} // DevDriver
