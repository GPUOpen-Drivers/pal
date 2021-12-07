/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include <ddCommon.h>
#include <ddEventServer.h>

namespace Event
{

class EventServer;
class EventProvider;

/// Define DDEventServer as an alias for EventServer
DD_DEFINE_HANDLE(DDEventServer, EventServer*);

/// Define DDEventProvider as an alias for EventProvider
DD_DEFINE_HANDLE(DDEventProvider, EventProvider*);

} // namespace Event
