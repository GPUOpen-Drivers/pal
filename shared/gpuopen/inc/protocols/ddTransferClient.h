/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include "legacyProtocolClient.h"
#include "protocols/ddTransferProtocol.h"

namespace DevDriver
{
    class IMsgChannel;

    namespace TransferProtocol
    {
        class TransferClient final : public LegacyProtocolClient
        {
        public:
            explicit TransferClient(IMsgChannel* pMsgChannel);
            ~TransferClient();

            // Requests a transfer on the remote client. Returns Success if the request was successful and data
            // is being sent to the client. Returns the size in bytes of the data being transferred in
            // pTransferSizeInBytes.
            Result RequestPullTransfer(BlockId blockId, size_t* pTransferSizeInBytes);

            // Reads transfer data from a previous transfer that completed successfully.
            Result ReadPullTransferData(uint8* pDstBuffer, size_t bufferSize, size_t* pBytesRead);

            // Aborts a pull transfer in progress.
            Result AbortPullTransfer();

            // Requests a transfer on the remote client. Returns Success if the request was successful and data
            // can be written to the server.
            Result RequestPushTransfer(BlockId blockId, size_t transferSizeInBytes);

            // Writes transfer data to the remote server.
            Result WritePushTransferData(const uint8* pSrcBuffer, size_t bufferSize);

            // Closes the push transfer session, optionally discarding any data already transmitted
            Result ClosePushTransfer(bool discard = false);

            // Returns true if there's currently a transfer in progress.
            bool IsTransferInProgress() const
            {
                return IsConnected() && (m_transferContext.state == TransferState::TransferInProgress);
            }

            Result ReadTransferData(uint8* pDstBuffer, size_t bufferSize, size_t* pBytesRead)
            {
                return ReadPullTransferData(pDstBuffer, bufferSize, pBytesRead);
            }

        private:
            void ResetState() override;

            // Helper method to send a payload, handling backwards compatibility and retrying.
            Result SendTransferPayload(const SizedPayloadContainer& container,
                                       uint32                       timeoutInMs = kDefaultCommunicationTimeoutInMs,
                                       uint32                       retryInMs   = kDefaultRetryTimeoutInMs);
            // Helper method to handle sending a payload from a SizedPayloadContainer, including retrying if busy.
            Result ReceiveTransferPayload(SizedPayloadContainer* pContainer,
                                          uint32                 timeoutInMs = kDefaultCommunicationTimeoutInMs,
                                          uint32                 retryInMs   = kDefaultRetryTimeoutInMs);
            // Helper method to send and then receive using a SizedPayloadContainer object.
            Result TransactTransferPayload(SizedPayloadContainer* pContainer,
                                           uint32                 timeoutInMs = kDefaultCommunicationTimeoutInMs,
                                           uint32                 retryInMs   = kDefaultRetryTimeoutInMs);

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
                TransferType  type;
                uint32 totalBytes;
                uint32 crc32;
                size_t dataChunkSizeInBytes;
                size_t dataChunkBytesTransfered;
                SizedPayloadContainer scratchPayload;
            };

            ClientTransferContext m_transferContext;

            DD_STATIC_CONST uint32 kTransferChunkTimeoutInMs = 3000;
        };
    }
} // DevDriver
