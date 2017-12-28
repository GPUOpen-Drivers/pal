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

#include "ddTransferManager.h"
#include "messageChannel.h"

namespace DevDriver
{
    namespace TransferProtocol
    {
        // =====================================================================================================================
        TransferManager::TransferManager(const AllocCb& allocCb)
            : m_pMessageChannel(nullptr)
            , m_pSessionManager(nullptr)
            , m_pTransferServer(nullptr)
            , m_allocCb(allocCb)
            , m_nextBlockId(1) // Block Ids start at 1. 0 is invalid.
        {
        }

        // =====================================================================================================================
        TransferManager::~TransferManager()
        {
            Destroy();
        }

        // =====================================================================================================================
        Result TransferManager::Init(IMsgChannel* pMsgChannel, SessionManager* pSessionManager)
        {
            DD_ASSERT(pMsgChannel != nullptr);
            DD_ASSERT(pSessionManager != nullptr);

            m_pMessageChannel = pMsgChannel;
            m_pSessionManager = pSessionManager;

            m_pTransferServer = DD_NEW(TransferServer, m_allocCb)(m_pMessageChannel);
            if (m_pTransferServer != nullptr)
            {
                m_pSessionManager->RegisterProtocolServer(m_pTransferServer);
            }

            return (m_pTransferServer != nullptr) ? Result::Success : Result::Error;
        }

        // =====================================================================================================================
        void TransferManager::Destroy()
        {
            if (m_pTransferServer != nullptr)
            {
                m_pSessionManager->UnregisterProtocolServer(m_pTransferServer);
                DD_DELETE(m_pTransferServer, m_allocCb);
                m_pTransferServer = nullptr;
            }
        }

        // =====================================================================================================================
        SharedPointer<LocalBlock> TransferManager::AcquireLocalBlock()
        {
            const BlockId blockId = m_nextBlockId;
            Platform::AtomicIncrement(&m_nextBlockId);

            // Attempt to allocate a new local block
            SharedPointer<LocalBlock> pBlock = SharedPointer<LocalBlock>::Create(m_allocCb,
                                                                                 m_allocCb,
                                                                                 blockId);
            if (!pBlock.IsNull())
            {
                m_pTransferServer->RegisterLocalBlock(pBlock);
            }

            return pBlock;
        }

        // =====================================================================================================================
        void TransferManager::ReleaseLocalBlock(SharedPointer<LocalBlock>& pBlock)
        {
            DD_ASSERT(!pBlock.IsNull());

            m_pTransferServer->UnregisterLocalBlock(pBlock);

            // Clear the external shared pointer to the block.
            pBlock.Clear();
        }

        // =====================================================================================================================
        RemoteBlock* TransferManager::OpenRemoteBlock(ClientId clientId, BlockId blockId)
        {
            RemoteBlock* pBlock = DD_NEW(RemoteBlock, m_allocCb)(m_pMessageChannel, blockId);
            if (pBlock != nullptr)
            {
                // Connect to the remote client and request a transfer.
                Result result = pBlock->m_transferClient.Connect(clientId);
                if (result == Result::Success)
                {
                    result = pBlock->m_transferClient.RequestTransfer(blockId, &pBlock->m_blockDataSize);
                }

                // If we fail the transfer or connection, destroy the block.
                if (result != Result::Success)
                {
                    pBlock->m_transferClient.Disconnect();
                    DD_DELETE(pBlock, m_allocCb);
                    pBlock = nullptr;
                }
            }
            return pBlock;
        }

        // =====================================================================================================================
        void TransferManager::CloseRemoteBlock(RemoteBlock** ppBlock)
        {
            DD_ASSERT(ppBlock != nullptr);

            TransferProtocol::TransferClient& transferClient = (*ppBlock)->m_transferClient;
            if (transferClient.IsTransferInProgress())
            {
                // Attempt to abort the transfer if there's currently one in progress.
                const Result result = transferClient.AbortTransfer();
                DD_ASSERT(result == Result::Success);
            }

            transferClient.Disconnect();
            DD_DELETE((*ppBlock), m_allocCb);
            *ppBlock = nullptr;
        }

        // =====================================================================================================================
        void LocalBlock::Write(const uint8* pSrcBuffer, size_t numBytes)
        {
            // Writes can only be performed on blocks that are not closed.
            DD_ASSERT(m_isClosed == false);

            if (numBytes > 0)
            {
                // Calculate how many bytes we have available.
                const size_t blockCapacityInBytes = (m_chunks.Size() * kTransferChunkSizeInBytes);
                const size_t bytesAvailable = (blockCapacityInBytes - m_blockDataSize);

                // Allocate more chunks if necessary.
                if (bytesAvailable < numBytes)
                {
                    const size_t additionalBytesRequired = (numBytes - bytesAvailable);
                    const size_t numChunksRequired =
                        (Platform::Pow2Align(additionalBytesRequired, kTransferChunkSizeInBytes) / kTransferChunkSizeInBytes);
                    m_chunks.Resize(m_chunks.Size() + numChunksRequired);
                }

                // Copy the new data into the block
                uint8* pData = (reinterpret_cast<uint8*>(m_chunks.Data()) + m_blockDataSize);
                memcpy(pData, pSrcBuffer, numBytes);
                m_blockDataSize += numBytes;
            }
        }

        // =====================================================================================================================
        void LocalBlock::Close()
        {
            DD_ASSERT(m_isClosed == false);

            m_isClosed = true;
        }

        // =====================================================================================================================
        void LocalBlock::Reset()
        {
            m_isClosed = false;
            m_blockDataSize = 0;
        }

        // =====================================================================================================================
        void LocalBlock::BeginTransfer()
        {
            m_pendingTransfersMutex.Lock();

            // Increment the number of pending transfers.
            ++m_numPendingTransfers;

            // Reset the event if this is the first transfer that's starting on this block.
            if (m_numPendingTransfers == 1)
            {
                m_transfersCompletedEvent.Clear();
            }

            m_pendingTransfersMutex.Unlock();
        }

        // =====================================================================================================================
        void LocalBlock::EndTransfer()
        {
            m_pendingTransfersMutex.Lock();

            // We should always have pending transfers when end is called.
            DD_ASSERT(m_numPendingTransfers > 0);

            // Decrement the number of pending transfers.
            --m_numPendingTransfers;

            // Signal the event if this was the last transfer that was pending on this block.
            if (m_numPendingTransfers == 0)
            {
                m_transfersCompletedEvent.Signal();
            }

            m_pendingTransfersMutex.Unlock();
        }

        // =====================================================================================================================
        Result LocalBlock::WaitForPendingTransfers(uint32 timeoutInMs)
        {
            return m_transfersCompletedEvent.Wait(timeoutInMs);
        }

        // =====================================================================================================================
        Result RemoteBlock::Read(uint8* pDstBuffer, size_t bufferSize, size_t* pBytesRead)
        {
            return m_transferClient.ReadTransferData(pDstBuffer, bufferSize, pBytesRead);
        }
    } // TransferProtocol
} // DevDriver
