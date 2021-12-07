/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include <ddEventServer.h>
#include <protocols/ddEventServer.h>

#include <util/vector.h>

namespace Event
{

class EventProvider;

/// Class responsible for managing the server side implementation of the event protocol
///
/// Providers can be created inside this object to expose them to remote clients on the network
class EventServer
{
public:
    EventServer(DDNetConnection hConnection);
    ~EventServer();

    DD_RESULT Initialize();

    DD_RESULT RegisterProvider(EventProvider* pProvider);
    void UnregisterProvider(EventProvider* pProvider);

private:
    DDNetConnection m_hConnection;

    DevDriver::EventProtocol::EventServer m_server;
};

} // namespace Event
