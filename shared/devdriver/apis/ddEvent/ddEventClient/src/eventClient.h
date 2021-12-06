/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include <ddEventClient.h>
#include <legacy/legacyEventClient.h>

namespace Event
{

class EventClient
{
public:
    EventClient(DDNetConnection hConnection, const DDEventDataCallback& dataCb);
    ~EventClient();

    DD_RESULT Connect(
        DDClientId clientId,
        uint32_t   timeoutInMs);

    DD_RESULT ReadEventData(
        uint32_t timeoutInMs);

    DD_RESULT QueryProviders(
        const DDEventProviderVisitor* pVisitor);

    DD_RESULT ConfigureProviders(
        size_t                     numProviders,
        const DDEventProviderDesc* pProviders);

    DD_RESULT EnableProviders(
        size_t          numProviderIds,
        const uint32_t* pProviderIds);

    DD_RESULT DisableProviders(
        size_t          numProviderIds,
        const uint32_t* pProviderIds);

private:
    DD_RESULT BulkUpdateProviders(
        size_t          numProviderIds,
        const uint32_t* pProviderIds,
        bool            enabled);

    void ReceiveEventData(const void* pData, size_t dataSize);

    DevDriver::EventProtocol::EventClient m_legacyClient;
    DDEventDataCallback                   m_dataCb;
};

} // namespace Event
