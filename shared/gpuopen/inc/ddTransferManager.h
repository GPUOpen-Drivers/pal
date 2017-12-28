/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  ddTransferManager.h
* @brief Class declaration for TransferManager
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"
#include "util/sharedptr.h"
#include "util/vector.h"
#include "protocols/systemProtocols.h"
#include "protocols/ddTransferServer.h"
#include "protocols/ddTransferClient.h"

namespace DevDriver
{
    class IMsgChannel;
    class SessionManager;

    namespace TransferProtocol
    {
        class TransferManager;

        // Size of an individual "chunk" within a transfer operation.
        static const size_t kTransferChunkSizeInBytes = 4096;

        // A struct that represents a single transfer chunk
        struct TransferChunk
        {
            uint8 Data[kTransferChunkSizeInBytes];
        };

        // Base class for transfer blocks.
        // A "block" is a binary blob of data associated with a unique id. Blocks can be created locally via the
        // transfer manager's AcquireLocalBlock function. Once a local block is closed, it can be accessed remotely
        // by other clients on the message bus. Remote clients can simply use their own transfer manager to open
        // the desired block over the bus via OpenRemoteBlock.
        class TransferBlock
        {
        public:
            explicit TransferBlock(BlockId blockId)
                : m_blockDataSize(0)
                , m_blockId(blockId) {}

            // Returns the unique id associated with this block
            BlockId GetBlockId() const { return m_blockId; }

            // Returns the size of the data contained within this block in bytes
            size_t GetBlockDataSize() const { return m_blockDataSize; }

        protected:
            size_t  m_blockDataSize; // The size of the data held by the block
            BlockId m_blockId;       // The id associated with this block
        };

        // A "local" transfer block.
        // Only supports writes and must be closed before the data can be accessed remotely.
        // Writes can only be performed on blocks that have not been closed.
        class LocalBlock : public TransferBlock
        {
            friend class TransferServer;
        public:
            explicit LocalBlock(const AllocCb& allocCb, BlockId blockId)
                : TransferBlock(blockId)
                , m_isClosed(false)
                , m_chunks(allocCb)
                , m_numPendingTransfers(0)
                , m_transfersCompletedEvent(true)
                {}

            // Writes numBytes bytes from pSrcBuffer into the block.
            void Write(const uint8* pSrcBuffer, size_t numBytes);

            // Closes the block which exposes it to external clients and prevents further writes.
            void Close();

            // Resets the block to its initial state. Does not return allocated memory.
            void Reset();

            // Returns true if this block has been closed.
            bool IsClosed() const { return m_isClosed; }

            // Returns a const pointer to the underlying data contained within the block.
            const uint8* GetBlockData() const { return reinterpret_cast<const uint8*>(m_chunks.Data()); }

            // Waits for all pending transfers to complete or for the timeout to expire.
            Result WaitForPendingTransfers(uint32 timeoutInMs);

        private:
            // Notifies the block that a new transfer has begun.
            void BeginTransfer();

            // Notifies the block that an existing transfer has ended.
            void EndTransfer();

            bool                  m_isClosed;                // A bool that indicates if the block is closed
            Vector<TransferChunk> m_chunks;                  // A list of transfer chunks used to store data
            Platform::Mutex       m_pendingTransfersMutex;   // A mutex used to control access to the pending transfers counter
            uint32                m_numPendingTransfers;     // A counter used to track the number of pending transfers
            Platform::Event       m_transfersCompletedEvent; // An event that is signaled when all pendings transfers are completed
        };

        // A "remote" transfer block.
        // Only supports reads.
        class RemoteBlock : public TransferBlock
        {
            friend class TransferManager;
        public:
            explicit RemoteBlock(IMsgChannel* pMsgChannel, BlockId blockId)
                : TransferBlock(blockId)
                , m_transferClient(pMsgChannel) {}

            // Reads up to bufferSize bytes into pDstBuffer from the block.
            // Returns the number of bytes read in pBytesRead.
            Result Read(uint8* pDstBuffer, size_t bufferSize, size_t* pBytesRead);

        private:
            TransferClient m_transferClient;
        };

        // Transfer manager class.
        // Manages interactions with local/remote transfer blocks.
        class TransferManager
        {
        public:
            explicit TransferManager(const AllocCb& allocCb);
            ~TransferManager();

            Result Init(IMsgChannel* pMsgChannel, SessionManager* pSessionManager);
            void Destroy();

            // Returns a shared pointer to a local block or nullptr in the case of an error.
            // Shared pointers are always used with local blocks to make sure they aren't destroyed
            // while a remote download is in progress.
            SharedPointer<LocalBlock> AcquireLocalBlock();

            // Releases a local block. This prevents new remote transfer requests from succeeding.
            // This will clear the local block pointer inside the shared pointer object.
            void ReleaseLocalBlock(SharedPointer<LocalBlock>& pBlock);

            // Attempts to open a block exposed by a remote client over the message bus.
            // Returns a valid RemoteBlock pointer on success and nullptr on failure.
            RemoteBlock* OpenRemoteBlock(ClientId clientId, BlockId blockId);

            // Closes a remote block and deletes the underlying resources.
            // This will null out the remote block pointer that is passed in as ppBlock.
            void CloseRemoteBlock(RemoteBlock** ppBlock);

        private:
            IMsgChannel*     m_pMessageChannel;
            SessionManager*  m_pSessionManager;
            TransferServer*  m_pTransferServer;
            AllocCb          m_allocCb;
            Platform::Atomic m_nextBlockId;
        };
    } // TransferProtocol
} // DevDriver
