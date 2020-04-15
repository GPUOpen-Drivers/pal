/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  ddEventClient.cpp
* @brief Implementation for EventClient
***********************************************************************************************************************
*/

#include <protocols/ddEventClient.h>
#include <msgChannel.h>
#include <ddTransferManager.h>

#define EVENT_CLIENT_MIN_VERSION EVENT_INDEXING_VERSION
#define EVENT_CLIENT_MAX_VERSION EVENT_INDEXING_VERSION

namespace DevDriver
{
namespace EventProtocol
{

// =====================================================================================================================
EventClient::EventClient(IMsgChannel* pMsgChannel)
    : BaseProtocolClient(pMsgChannel, Protocol::Event, EVENT_CLIENT_MIN_VERSION, EVENT_CLIENT_MAX_VERSION)
    , m_eventDataOffset(0)
    , m_eventDataPayloadOffset(0)
    , m_eventDataState(EventDataState::WaitingForHeader)
{
}

// =====================================================================================================================
EventClient::~EventClient()
{
}

// =====================================================================================================================
Result EventClient::QueryProviders(EventProvidersDescription** ppProvidersDescription)
{
    Result result = Result::Error;

    if (IsConnected() && (ppProvidersDescription != nullptr))
    {
        TransferProtocol::TransferManager& transferManager = m_pMsgChannel->GetTransferManager();

        SizedPayloadContainer container = {};
        container.CreatePayload<QueryProvidersRequestPayload>();

        result = SendPayloadContainer(container);
        if (result == Result::Success)
        {
            result = ReceiveResponsePayload(&container, EventMessage::QueryProvidersResponse);
        }

        TransferProtocol::PullBlock* pPullBlock = nullptr;

        if (result == Result::Success)
        {
            const QueryProvidersResponsePayload& response =
                container.GetPayload<QueryProvidersResponsePayload>();

            result = response.result;

            if (result == Result::Success)
            {
                pPullBlock = transferManager.OpenPullBlock(m_pSession->GetDestinationClientId(),
                                                            response.blockId);

                result = (pPullBlock != nullptr) ? Result::Success : Result::Error;
            }
        }

        if (result == Result::Success)
        {
            const size_t dataSize = pPullBlock->GetBlockDataSize();
            void* pMemory = DD_MALLOC(sizeof(EventProvidersDescription) + dataSize,
                                        alignof(EventProvidersDescription),
                                        m_pMsgChannel->GetAllocCb());

            if (pMemory != nullptr)
            {
                void* pResponseData = VoidPtrInc(pMemory, sizeof(EventProvidersDescription));

                EventProvidersDescription* pProvidersDescription =
                    new (pMemory) EventProvidersDescription(pResponseData);

                size_t bytesRead = 0;

                while (result == Result::Success)
                {
                    result = pPullBlock->Read(reinterpret_cast<uint8*>(VoidPtrInc(pResponseData, bytesRead)),
                                                (dataSize - bytesRead),
                                                &bytesRead);
                }

                if (result == Result::EndOfStream)
                {
                    transferManager.ClosePullBlock(&pPullBlock);

                    *ppProvidersDescription = pProvidersDescription;

                    result = Result::Success;
                }
            }
            else
            {
                result = Result::InsufficientMemory;
            }
        }
    }

    return result;
}

// =====================================================================================================================
Result EventClient::UpdateProviders(
    const EventProviderUpdateRequest* pProviderUpdates,
    uint32                            numProviderUpdates)
{
    Result result = Result::Error;

    if (IsConnected())
    {
        if ((pProviderUpdates != nullptr) && (numProviderUpdates > 0))
        {
            TransferProtocol::TransferManager& transferManager = m_pMsgChannel->GetTransferManager();

            // Iterate through the provider updates and calculate the total size
            size_t updateDataSize = 0;
            for (uint32 updateIndex = 0; updateIndex < numProviderUpdates; ++updateIndex)
            {
                updateDataSize += sizeof(ProviderUpdateHeader) + pProviderUpdates[updateIndex].eventDataSize;
            }

            SizedPayloadContainer container = {};
            container.CreatePayload<AllocateProviderUpdatesRequest>(static_cast<uint32>(updateDataSize));

            result = SendPayloadContainer(container);
            if (result == Result::Success)
            {
                result = ReceiveResponsePayload(&container, EventMessage::AllocateProviderUpdatesResponse);
            }

            if (result == Result::Success)
            {
                result = container.GetPayload<AllocateProviderUpdatesResponse>().result;
            }

            TransferProtocol::PushBlock* pPushBlock = nullptr;

            if (result == Result::Success)
            {
                const BlockId blockId = container.GetPayload<AllocateProviderUpdatesResponse>().blockId;
                pPushBlock =
                    transferManager.OpenPushBlock(
                        m_pSession->GetDestinationClientId(),
                        blockId,
                        updateDataSize);

                result = (pPushBlock != nullptr) ? Result::Success : Result::Error;
            }

            if (result == Result::Success)
            {
                for (uint32 updateIndex = 0; (updateIndex < numProviderUpdates) && (result == Result::Success); ++updateIndex)
                {
                    const EventProviderUpdateRequest& request = pProviderUpdates[updateIndex];

                    ProviderUpdateHeader header(request.id, static_cast<uint32>(request.eventDataSize), request.enabled);
                    result = pPushBlock->Write(reinterpret_cast<const uint8*>(&header), sizeof(header));

                    if ((result == Result::Success) && (request.eventDataSize > 0))
                    {
                        result = pPushBlock->Write(reinterpret_cast<const uint8*>(request.pEventData), request.eventDataSize);
                    }
                }

                if (result == Result::Success)
                {
                    result = pPushBlock->Finalize();
                }
            }

            if (pPushBlock != nullptr)
            {
                transferManager.ClosePushBlock(&pPushBlock);
            }

            if (result == Result::Success)
            {
                container.CreatePayload<ApplyProviderUpdatesRequest>();

                result = SendPayloadContainer(container);
            }

            if (result == Result::Success)
            {
                result = ReceiveResponsePayload(&container, EventMessage::ApplyProviderUpdatesResponse);
            }

            if (result == Result::Success)
            {
                result = container.GetPayload<ApplyProviderUpdatesResponse>().result;
            }
        }
        else
        {
            result = Result::InvalidParameter;
        }
    }

    return result;
}

// =====================================================================================================================
size_t EventClient::GetTokenSize(EventTokenType tokenType)
{
    switch (tokenType)
    {
    case EventTokenType::Provider:
        return sizeof(EventProviderToken);
    case EventTokenType::Data:
        return sizeof(EventDataToken);
    case EventTokenType::Timestamp:
        return sizeof(EventTimestampToken);
    case EventTokenType::TimeDelta:
        return sizeof(EventTimeDeltaToken);
    case EventTokenType::Count:
        break;
    }
    DD_ASSERT_REASON("Invalid token type!");
    return 0;
}

// =====================================================================================================================
void EventClient::OnTokenAvailable()
{
    if (m_callback.pCallback != nullptr)
    {
        m_callback.pCallback(m_callback.pUserdata, m_eventDataBuffer, m_eventDataOffset);
    }

    // Once we've returned from the callback, we can reset our state to prepare to process the next one
    ResetEventDataBufferState();
}

// =====================================================================================================================
Result EventClient::ReceiveEventData(const void* pEventData, size_t eventDataSize)
{
    Result result = Result::Success;
    size_t bytesRemaining = eventDataSize;

    while (bytesRemaining > 0)
    {
        switch (m_eventDataState)
        {
        case EventDataState::WaitingForHeader:
        {
            // Since we know
            //  A) we're at the start of the data buffer
            //  B) We have > 0 bytes remaining and
            //  C) the header is only 1 byte in size
            // we don't need to do any bounds checking on the copy.
            DD_ASSERT(m_eventDataOffset == 0);
            static_assert(kTokenHeaderSize == 1, "Size of event token header changed");

            memcpy(&m_eventDataBuffer[m_eventDataOffset], pEventData, kTokenHeaderSize);
            pEventData = VoidPtrInc(pEventData, kTokenHeaderSize);
            bytesRemaining -= kTokenHeaderSize;
            m_eventDataOffset += kTokenHeaderSize;

            m_eventDataState = EventDataState::WaitingForToken;
            break;
        }
        case EventDataState::WaitingForToken:
        {
            // If this if this condition is not true, something went very wrong and our offset is no longer
            // synchronized with our state.
            DD_ASSERT(m_eventDataOffset >= kTokenHeaderSize);

            const EventTokenHeader* pHeader = reinterpret_cast<const EventTokenHeader*>(m_eventDataBuffer);

            const size_t tokenSize = GetTokenSize(static_cast<EventTokenType>(pHeader->id));
            const size_t bytesCopied = m_eventDataOffset - kTokenHeaderSize;
            const size_t copySize = Platform::Min(bytesRemaining, (tokenSize - bytesCopied));

            // Similarly, we know that our event data buffer is big enough to hold any of the tokens we might receive
            // so we can just assert that this copy is withing the capacity.
            DD_ASSERT((m_eventDataOffset + copySize) <= kEventDataBufferSize);

            memcpy(&m_eventDataBuffer[m_eventDataOffset], pEventData, copySize);
            pEventData = VoidPtrInc(pEventData, copySize);
            bytesRemaining -= copySize;
            m_eventDataOffset += copySize;

            if (m_eventDataOffset == (tokenSize + kTokenHeaderSize))
            {
                if ((pHeader->id == static_cast<uint8>(EventTokenType::Data)) ||
                    (pHeader->id == static_cast<uint8>(EventTokenType::TimeDelta)))
                {
                    m_eventDataPayloadOffset = m_eventDataOffset;
                    m_eventDataState = EventDataState::WaitingForPayload;
                }
                else
                {
                    OnTokenAvailable();
                }
            }

            break;
        }
        case EventDataState::WaitingForPayload:
        {
            // We should always have more than a token header in our buffer if we reach this state.
            DD_ASSERT(m_eventDataOffset > kTokenHeaderSize);

            const EventTokenHeader* pHeader = reinterpret_cast<const EventTokenHeader*>(m_eventDataBuffer);

            size_t payloadSize = 0;
            if (pHeader->id == static_cast<uint8>(EventTokenType::Data))
            {
                const EventDataToken* pToken = reinterpret_cast<const EventDataToken*>(m_eventDataBuffer + kTokenHeaderSize);
                payloadSize = pToken->size;
            }
            else if (pHeader->id == static_cast<uint8>(EventTokenType::TimeDelta))
            {
                const EventTimeDeltaToken* pToken = reinterpret_cast<const EventTimeDeltaToken*>(m_eventDataBuffer + kTokenHeaderSize);
                payloadSize = pToken->numBytes;
            }
            else
            {
                DD_ASSERT_REASON("Invalid token type!");
                result = Result::Aborted;
            }
            DD_ASSERT(payloadSize != 0);

            if (result == Result::Success)
            {
                const size_t bytesCopied = m_eventDataOffset - m_eventDataPayloadOffset;
                const size_t copySize = Platform::Min(bytesRemaining, (payloadSize - bytesCopied));
                if ((m_eventDataOffset + copySize) <= kEventDataBufferSize)
                {
                    memcpy(&m_eventDataBuffer[m_eventDataOffset], pEventData, copySize);
                }
                else
                {
                    // In this case the variably sized payload is too big to fit in our data buffer. We'll assert and
                    // return an error
                    DD_ASSERT_ALWAYS();
                    result = Result::InsufficientMemory;
                }

                // But we also want to continue processing other tokens so we'll also advance the pointer/counters/state
                // whether there was an error or not.
                pEventData = VoidPtrInc(pEventData, copySize);
                bytesRemaining -= copySize;
                m_eventDataOffset += copySize;
            }

            if ((m_eventDataOffset - m_eventDataPayloadOffset) == payloadSize)
            {
                if (result == Result::Success)
                {
                    OnTokenAvailable();
                }
                else
                {
                    ResetEventDataBufferState();
                }
            }
            break;
        }
        }
    }

    return result;
}

// =====================================================================================================================
Result EventClient::ReadEventData(uint32 timeoutInMs)
{
    SizedPayloadContainer container = {};

    Result result = ReceivePayloadContainer(&container, timeoutInMs);

    if (result == Result::Success)
    {
        if (container.GetPayload<EventHeader>().command == EventMessage::EventDataUpdate)
        {
            const EventDataUpdatePayload& payload = container.GetPayload<EventDataUpdatePayload>();
            result = ReceiveEventData(payload.GetEventDataBuffer(), payload.GetEventDataSize());
        }
        else
        {
            // Return an error if we get an unexpected payload
            result = Result::Error;
        }
    }

    return result;
}

// =====================================================================================================================
Result EventClient::FreeProvidersDescription(EventProvidersDescription** ppProvidersDescription)
{
    Result result = Result::UriInvalidParameters;

    if (ppProvidersDescription != nullptr)
    {
        DD_DELETE(*ppProvidersDescription, m_pMsgChannel->GetAllocCb());
        *ppProvidersDescription = nullptr;

        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
void EventClient::ResetEventDataBufferState()
{
    m_eventDataOffset        = 0;
    m_eventDataPayloadOffset = 0;
    m_eventDataState         = EventDataState::WaitingForHeader;
}

// =====================================================================================================================
void EventClient::ResetState()
{
    ResetEventDataBufferState();
}

// =====================================================================================================================
Result EventClient::ReceiveResponsePayload(SizedPayloadContainer* pContainer, EventMessage responseType)
{
    // This function should never be used when the caller is directly looking for an event data update.
    // The code here is meant to filter out updates when the caller is looking for something else.
    DD_ASSERT(responseType != EventMessage::EventDataUpdate);

    Result result = Result::Success;

    do {
        result = ReceivePayloadContainer(pContainer);
        if (result == Result::Success)
        {
            if (pContainer->GetPayload<EventHeader>().command == responseType)
            {
                // We've found the requested response. Break out of the loop.
                break;
            }
            else if (pContainer->GetPayload<EventHeader>().command == EventMessage::EventDataUpdate)
            {
                const EventDataUpdatePayload& payload = pContainer->GetPayload<EventDataUpdatePayload>();

                result = ReceiveEventData(payload.GetEventDataBuffer(), payload.GetEventDataSize());
            }
            else
            {
                // We've received an unexpected response type. Exit the loop.
                result = Result::Error;
            }
        }
    } while (result == Result::Success);

    return result;
}

} // EventProtocol
} // DevDriver
