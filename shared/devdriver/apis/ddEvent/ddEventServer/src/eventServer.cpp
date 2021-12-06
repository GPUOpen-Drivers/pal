/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#include <eventServer.h>
#include <eventProvider.h>

#include <ddCommon.h>

#include <msgChannel.h>

using namespace DevDriver;

namespace Event
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EventServer::EventServer(
    DDNetConnection hConnection)
    : m_hConnection(hConnection)
    , m_server(FromHandle(hConnection))
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EventServer::~EventServer()
{
    DD_UNHANDLED_RESULT(FromHandle(m_hConnection)->UnregisterProtocolServer(&m_server));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT EventServer::Initialize()
{
    return DevDriverToDDResult(FromHandle(m_hConnection)->RegisterProtocolServer(&m_server));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT EventServer::RegisterProvider(
    EventProvider* pProvider)
{
    return DevDriverToDDResult(m_server.RegisterProvider(pProvider));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void EventServer::UnregisterProvider(
    EventProvider* pProvider)
{
    DD_UNHANDLED_RESULT(m_server.UnregisterProvider(pProvider));
}

} // namespace Event
