/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

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
