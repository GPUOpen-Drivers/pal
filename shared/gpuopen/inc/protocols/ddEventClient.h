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
/**
***********************************************************************************************************************
* @file  ddEventClient.h
* @brief Class declaration for EventClient.
***********************************************************************************************************************
*/

#pragma once

#include <baseProtocolClient.h>
#include <protocols/ddEventProtocol.h>

namespace DevDriver
{
    class IMsgChannel;

    namespace TransferProtocol
    {
        class PullBlock;
    }

    namespace EventProtocol
    {
        class EventClient;

        typedef void(*EventDataReceived)(void* pUserdata, const void* pEventData, size_t eventDataSize);

        struct EventDataCallbackInfo
        {
            EventDataReceived pCallback = nullptr;
            void*             pUserdata = nullptr;
        };

        class EventProviderIterator
        {
            friend class EventProvidersDescription;

        public:
            bool IsValid() const
            {
                const bool isInRange = (m_providerIndex < m_numProviders);

                DD_ASSERT((m_pProviderData == nullptr) || isInRange);

                return isInRange;
            }

            uint32 GetId() const
            {
                return GetHeader().providerId;
            }

            uint32 GetNumEvents() const
            {
                return GetHeader().numEvents;
            }

            const void* GetEventData() const
            {
                return (reinterpret_cast<const uint8*>(m_pProviderData) + GetHeader().GetEventDataOffset());
            }

            uint32 GetEventDataSize() const
            {
                return static_cast<uint32>(GetHeader().GetEventDataSize());
            }

            const void* GetEventDescriptionData() const
            {
                return (reinterpret_cast<const uint8*>(m_pProviderData) + GetHeader().GetEventDescriptionOffset());
            }

            uint32 GetEventDescriptionDataSize() const
            {
                return static_cast<uint32>(GetHeader().eventDescriptionDataSize);
            }

            EventProviderIterator Next()
            {
                EventProviderIterator nextIterator;

                const uint32 nextProviderIndex = (m_providerIndex + 1);
                if (nextProviderIndex < m_numProviders)
                {
                    const void* pNextProviderData =
                        reinterpret_cast<const uint8*>(m_pProviderData) + GetHeader().GetNextProviderDescriptionOffset();
                    nextIterator = EventProviderIterator(nextProviderIndex,
                                                         m_numProviders,
                                                         pNextProviderData);
                }

                return nextIterator;
            }

        private:
            EventProviderIterator()
                : m_providerIndex(0)
                , m_numProviders(0)
                , m_pProviderData(nullptr)
            {
            }
            EventProviderIterator(
                uint32      providerIndex,
                uint32      numProviders,
                const void* pProviderData)
                : m_providerIndex(providerIndex)
                , m_numProviders(numProviders)
                , m_pProviderData(pProviderData)
            {
            }

            const ProviderDescriptionHeader& GetHeader() const
            {
                return *reinterpret_cast<const ProviderDescriptionHeader*>(m_pProviderData);
            }

            uint32      m_providerIndex;
            uint32      m_numProviders;
            const void* m_pProviderData;
        };

        class EventProvidersDescription
        {
            friend class EventClient;

        public:
            ~EventProvidersDescription() = default;

            uint32 GetNumProviders() const
            {
                return GetHeader().numProviders;
            }

            EventProviderIterator GetFirstProvider()
            {
                return EventProviderIterator(0,
                                             GetNumProviders(),
                                             reinterpret_cast<const uint8*>(m_pResponseData) + sizeof(QueryProvidersResponseHeader));
            }

        private:
            EventProvidersDescription(
                const void* pResponseData)
                : m_pResponseData(pResponseData)
            {
            }

            const QueryProvidersResponseHeader& GetHeader() const
            {
                return *reinterpret_cast<const QueryProvidersResponseHeader*>(m_pResponseData);
            }

            const void* m_pResponseData;
        };

        struct EventProviderUpdateRequest
        {
            EventProviderId id;
            bool enabled;

            const void* pEventData;
            size_t eventDataSize;
        };

        class EventClient : public BaseProtocolClient
        {
        public:
            explicit EventClient(IMsgChannel* pMsgChannel);
            ~EventClient();

            // Sets the event data callback which will be called whenever new data is available from the server
            void SetEventDataCallback(const EventDataCallbackInfo& callbackInfo)
            {
                m_callback = callbackInfo;
            }

            Result QueryProviders(EventProvidersDescription** ppProvidersDescription);

            Result UpdateProviders(const EventProviderUpdateRequest* pProviderUpdates, uint32 numProviderUpdates);

            // Reads any available event data from the server
            Result ReadEventData(uint32 timeoutInMs = kDefaultCommunicationTimeoutInMs);

            Result FreeProvidersDescription(EventProvidersDescription** ppProvidersDescription);

        private:
            void ResetState() override;

            Result ReceiveResponsePayload(SizedPayloadContainer* pContainer, EventMessage responseType);

            EventDataCallbackInfo m_callback;
        };

    }
} // DevDriver
