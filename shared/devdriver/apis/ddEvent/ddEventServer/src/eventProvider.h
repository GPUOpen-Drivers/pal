/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <ddEventServer.h>

#include <protocols/ddEventProvider.h>

namespace Event
{

class EventServer;

class EventProvider : public DevDriver::EventProtocol::BaseEventProvider
{
public:

    EventProvider(const DDEventProviderCreateInfo& createInfo);
    ~EventProvider();

    EventServer* GetServer() const { return m_pServer; }

    DD_RESULT EmitWithHeader(
        uint32_t    eventId,
        size_t      headerSize,
        const void* pHeader,
        size_t      payloadSize,
        const void* pPayload);

    DD_RESULT TestEmit(uint32_t eventId);

    DevDriver::EventProtocol::EventProviderId GetId() const override
    {
        return static_cast<DevDriver::EventProtocol::EventProviderId>(m_createInfo.id);
    }

    const void* GetEventDescriptionData() const override
    {
        return nullptr;
    }

    uint32_t GetEventDescriptionDataSize() const override
    {
        return 0;
    }

    const char* GetName() const override
    {
        return m_createInfo.name;
    }

    void OnEnable() override;
    void OnDisable() override;

private:
    DD_RESULT EmitInternal(
        uint32_t    eventId,
        size_t      headerSize,
        const void* pHeader,
        size_t      payloadSize,
        const void* pPayload,
        bool        isQuery);

    DDEventProviderCreateInfo m_createInfo;
    EventServer*              m_pServer;
};

} // namespace Event
