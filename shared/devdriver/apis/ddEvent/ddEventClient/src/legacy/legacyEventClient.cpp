/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <legacy/legacyEventClient.h>
#include <msgChannel.h>
#include <ddTransferManager.h>
#include <dd_timeout_constants.h>

#define EVENT_CLIENT_MIN_VERSION EVENT_INDEXING_VERSION
#define EVENT_CLIENT_MAX_VERSION EVENT_INDEXING_VERSION

namespace DevDriver
{
namespace EventProtocol
{

// =====================================================================================================================
EventClient::EventClient(IMsgChannel* pMsgChannel)
    : LegacyProtocolClient(pMsgChannel, Protocol::Event, EVENT_CLIENT_MIN_VERSION, EVENT_CLIENT_MAX_VERSION)
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

        result = SendPayloadContainer(container,
                                      g_timeoutConstants.communicationTimeoutInMs,
                                      g_timeoutConstants.retryTimeoutInMs);
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
                pPullBlock = transferManager.OpenPullBlock(GetRemoteClientId(),
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

            result = SendPayloadContainer(container,
                                          g_timeoutConstants.communicationTimeoutInMs,
                                          g_timeoutConstants.retryTimeoutInMs);
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
                        GetRemoteClientId(),
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

                result = SendPayloadContainer(container,
                                              g_timeoutConstants.communicationTimeoutInMs,
                                              g_timeoutConstants.retryTimeoutInMs);
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
void EventClient::EmitEventData(const void* pEventData, size_t eventDataSize)
{
    if (m_callback.pfnRawEventDataReceived != nullptr)
    {
        m_callback.pfnRawEventDataReceived(m_callback.pUserdata, pEventData, eventDataSize);
    }
}

// =====================================================================================================================
Result EventClient::ReadEventData(uint32 timeoutInMs)
{
    SizedPayloadContainer container = {};

    Result result = ReceivePayloadContainer(&container, timeoutInMs, g_timeoutConstants.retryTimeoutInMs);

    if (result == Result::Success)
    {
        if (container.GetPayload<EventHeader>().command == EventMessage::EventDataUpdate)
        {
            const EventDataUpdatePayload& payload = container.GetPayload<EventDataUpdatePayload>();
            EmitEventData(payload.GetEventDataBuffer(), payload.GetEventDataSize());
        }
        else if (container.GetPayload<EventHeader>().command == EventMessage::SubscribeToProviderResponse)
        {
            result = container.GetPayload<SubscribeToProviderResponse>().result;
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
void EventClient::FreeProvidersDescription(EventProvidersDescription* pProvidersDescription)
{
    DD_DELETE(pProvidersDescription, m_pMsgChannel->GetAllocCb());
}

// =====================================================================================================================
Result EventClient::SubscribeToProvider(EventProviderId providerId)
{
    SizedPayloadContainer payload = {};
    payload.CreatePayload<SubscribeToProviderRequest>(providerId);

    Result result = TransactPayloadContainer(&payload,
                                             g_timeoutConstants.communicationTimeoutInMs,
                                             g_timeoutConstants.retryTimeoutInMs);
    if (result == Result::Success)
    {
        result = payload.GetPayload<SubscribeToProviderResponse>().result;
    }

    return result;
}

// =====================================================================================================================
void EventClient::UnsubscribeFromProvider()
{
    SizedPayloadContainer payload = {};
    payload.CreatePayload<UnsubscribeFromProviderRequest>();
    SendPayloadContainer(payload, g_timeoutConstants.communicationTimeoutInMs, g_timeoutConstants.retryTimeoutInMs);
}

// =====================================================================================================================
Result EventClient::ReceiveResponsePayload(SizedPayloadContainer* pContainer, EventMessage responseType)
{
    // This function should never be used when the caller is directly looking for an event data update.
    // The code here is meant to filter out updates when the caller is looking for something else.
    DD_ASSERT(responseType != EventMessage::EventDataUpdate);

    Result result = Result::Success;

    do {
        result = ReceivePayloadContainer(pContainer,
                                         g_timeoutConstants.communicationTimeoutInMs,
                                         g_timeoutConstants.retryTimeoutInMs);
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

                EmitEventData(payload.GetEventDataBuffer(), payload.GetEventDataSize());
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

} // namespace EventProtocol
} // namespace DevDriver
