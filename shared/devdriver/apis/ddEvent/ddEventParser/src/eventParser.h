/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include <ddEventParser.h>

#include <legacy/legacyEventParser.h>

namespace Event
{

class EventParser
{
public:
    EventParser(const DDEventWriter& writer);

    DD_RESULT Parse(
        const void* pData,
        size_t      dataSize);

private:
    DevDriver::Result EventReceived(const DevDriver::EventProtocol::EventReceivedInfo& eventInfo);
    DevDriver::Result PayloadData(const void* pData, size_t dataSize);

    DevDriver::EventProtocol::EventParser m_parser;

    DDEventWriter          m_writer;
    DDEventParserEventInfo m_eventInfo;
    bool                   m_isReadingPayload;
    uint64_t               m_payloadBytesRemaining;
};

} // namespace Event
