/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
