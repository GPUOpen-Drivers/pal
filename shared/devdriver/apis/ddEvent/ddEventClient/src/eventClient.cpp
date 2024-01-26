/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <eventClient.h>
#include <ddCommon.h>

#include <util/ddBitSet.h>

using namespace DevDriver;

namespace Event
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EventClient::EventClient(
    DDNetConnection            hConnection,
    const DDEventDataCallback& dataCb)
    : m_legacyClient(FromHandle(hConnection))
    , m_dataCb(dataCb)
    , m_eventProviderVersion(0)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EventClient::~EventClient()
{
    if (m_eventProviderVersion >= 1)
    {
        m_legacyClient.UnsubscribeFromProvider();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT EventClient::Connect(
    DDClientId clientId,
    uint32_t   timeoutInMs)
{
    timeoutInMs = (timeoutInMs == 0) ? kDefaultConnectionTimeoutMs : timeoutInMs;

    EventProtocol::EventCallbackInfo callbackInfo = {};
    callbackInfo.pUserdata = this;
    callbackInfo.pfnRawEventDataReceived = [](void* pUserdata, const void* pData, size_t dataSize)
    {
        auto* pThis = reinterpret_cast<EventClient*>(pUserdata);
        pThis->ReceiveEventData(pData, dataSize);
    };
    m_legacyClient.SetEventCallback(callbackInfo);

    return DevDriverToDDResult(m_legacyClient.Connect(clientId, timeoutInMs));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT EventClient::ReadEventData(
    uint32_t timeoutInMs)
{
    return DevDriverToDDResult(m_legacyClient.ReadEventData(timeoutInMs));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT EventClient::EnableProviders(
    size_t          numProviderIds,
    const uint32_t* pProviderIds)
{
    return BulkUpdateProviders(numProviderIds, pProviderIds, true);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT EventClient::DisableProviders(
    size_t          numProviderIds,
    const uint32_t* pProviderIds)
{
    return BulkUpdateProviders(numProviderIds, pProviderIds, false);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT EventClient::SubscribeToProvider(uint32_t providerId)
{
    EventProtocol::EventProvidersDescription* pProvidersDescription = nullptr;
    Result result = m_legacyClient.QueryProviders(&pProvidersDescription);

    if (result == Result::Success)
    {
        DD_ASSERT(pProvidersDescription != nullptr);
        if (pProvidersDescription->GetNumProviders() > 0)
        {
            // We use `version` field inside of all `ProviderDescriptionHeader`
            // should be the same, so we only need to check the first one. We
            // use this to check what version of `EventServer` is at.
            DevDriver::EventProtocol::EventProviderIterator iter = pProvidersDescription->GetFirstProvider();
            m_eventProviderVersion = iter.GetVersion();

            if (m_eventProviderVersion >= 1)
            {
                result = m_legacyClient.SubscribeToProvider(providerId);
            }
            else
            {
                // For version 0 of EventServer, we don't send SubscribeToProvider request.
            }
        }
        else
        {
            result = Result::Unavailable;
        }
    }

    return DevDriverToDDResult(result);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT EventClient::BulkUpdateProviders(
    size_t          numProviderIds,
    const uint32_t* pProviderIds,
    bool            enable)
{
    Result result = (numProviderIds > 0 ) ? Result::Success : Result::InvalidParameter;

    EventProtocol::EventProvidersDescription* pProvidersDescription = nullptr;
    if (result == Result::Success)
    {
        result = m_legacyClient.QueryProviders(&pProvidersDescription);
    }

    if (result == Result::Success)
    {
        // This should never be null if the function succeeded
        DD_ASSERT(pProvidersDescription != nullptr);

        Vector<EventProtocol::EventProviderUpdateRequest> providerUpdates(Platform::GenericAllocCb);
        Vector<uint8> providerEventData(Platform::GenericAllocCb);
        Vector<size_t> updateEventDataOffsets(Platform::GenericAllocCb);
        DynamicBitSet<> enabledEvents(Platform::GenericAllocCb);

        // We should never reach this code if the caller provided an empty list of providers to update
        DD_ASSERT(numProviderIds > 0);

        // Generate a "provider update" for each provider
        for (size_t providerIndex = 0; providerIndex < numProviderIds; ++providerIndex)
        {
            if (pProvidersDescription->GetNumProviders() > 0)
            {
                DevDriver::EventProtocol::EventProviderIterator iter = pProvidersDescription->GetFirstProvider();
                while (iter.IsValid())
                {
                    if (pProviderIds[providerIndex] == iter.GetId())
                    {
                        const size_t numEvents = iter.GetNumEvents();
                        enabledEvents.Resize(numEvents);
                        enabledEvents.SetAllBits();

                        const size_t dataSize = enabledEvents.SizeInBytes();
                        const size_t dataOffset = providerEventData.Grow(dataSize);

                        uint8* pEventData = static_cast<uint8*>(VoidPtrInc(providerEventData.Data(), dataOffset));
                        memcpy(pEventData, enabledEvents.Data(), dataSize);

                        // Store the offset for this update since we'll need it later to fix up the array pointers
                        updateEventDataOffsets.PushBack(dataOffset);

                        // Record the number of events on the provider so we can update all of them
                        DevDriver::EventProtocol::EventProviderUpdateRequest updateRequest = {};
                        updateRequest.id                                                   = iter.GetId();
                        updateRequest.enabled                                              = enable;
                        updateRequest.eventDataSize                                        = dataSize;

                        providerUpdates.PushBack(updateRequest);

                        break;
                    }
                    else
                    {
                        iter = iter.Next();
                    }
                }

                if (iter.IsValid() == false)
                {
                    // We were unable to find one of the caller's desired providers on the server
                    result = Result::Error;
                }
            }
            else
            {
                //  No providers returned so we definitely can't update the caller's desired providers
                result = Result::Error;
            }
        }

        if (result == Result::Success)
        {
            // It should not be possible to reach this position with a Success result if no provider updates
            // were added to our list.
            DD_ASSERT(providerUpdates.IsEmpty() == false);

            // Fix up all event data pointers before the function is finally called
            for (size_t updateIndex = 0; updateIndex < providerUpdates.Size(); ++updateIndex)
            {
                providerUpdates[updateIndex].pEventData =
                    VoidPtrInc(providerEventData.Data(), updateEventDataOffsets[updateIndex]);
            }

            result = m_legacyClient.UpdateProviders(
                providerUpdates.Data(),
                static_cast<uint32_t>(providerUpdates.Size()));
        }
    }

    m_legacyClient.FreeProvidersDescription(pProvidersDescription);

    return DevDriverToDDResult(result);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void EventClient::ReceiveEventData(
    const void* pData,
    size_t      dataSize)
{
    m_dataCb.pfnCallback(m_dataCb.pUserdata, pData, dataSize);
}

} // namespace Event
