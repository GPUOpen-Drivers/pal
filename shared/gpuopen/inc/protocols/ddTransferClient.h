/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  ddTransferClient.h
* @brief Class declaration for TransferClient.
***********************************************************************************************************************
*/

#pragma once

#include "baseProtocolClient.h"

#include "protocols/systemProtocols.h"

namespace DevDriver
{
    class IMsgChannel;

    namespace TransferProtocol
    {
        class TransferClient : public BaseProtocolClient
        {
        public:
            explicit TransferClient(IMsgChannel* pMsgChannel);
            ~TransferClient();

            // Requests a transfer on the remote client. Returns Success if the request was successful and data
            // is being sent to the client. Returns the size in bytes of the data being transferred in
            // pTransferSizeInBytes.
            Result RequestTransfer(BlockId blockId, size_t* pTransferSizeInBytes);

            // Reads transfer data from a previous transfer that completed successfully.
            Result ReadTransferData(uint8* pDstBuffer, size_t bufferSize, size_t* pBytesRead);

            // Aborts a transfer in progress.
            Result AbortTransfer();

            // Returns true if there's currently a transfer in progress.
            bool IsTransferInProgress() const { return (m_transferContext.state == TransferState::TransferInProgress); }

        private:
            void ResetState() override;

            enum class TransferState : uint32
            {
                Idle = 0,
                TransferInProgress,
                Error
            };

            // Context structure for tracking all state specific to a transfer.
            struct ClientTransferContext
            {
                TransferState state;
                uint32 totalBytes;
                uint32 numChunks;
                uint32 numChunksReceived;
                TransferPayload lastPayload;
                size_t dataChunkSizeInBytes;
                size_t dataChunkBytesRead;
            };

            ClientTransferContext m_transferContext;

            DD_STATIC_CONST uint32 kTransferChunkTimeoutInMs = 3000;
        };
    }
} // DevDriver
