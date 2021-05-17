/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include "legacyProtocolClient.h"
#include "protocols/etwProtocol.h"

namespace DevDriver
{
    class IMsgChannel;

    namespace ETWProtocol
    {
        class ETWClient : public LegacyProtocolClient
        {
        public:
            explicit ETWClient(IMsgChannel* pMsgChannel);
            ~ETWClient();
            Result EnableTracing(ProcessId processId);
            Result DisableTracing(size_t *pNumEvents);
            Result GetTraceData(GpuEvent *buffer, size_t numEvents);
        private:
            void ResetState() override;

            enum class SessionState
            {
                Idle = 0,
                Tracing,
                Waiting,
                Receiving,
            };

            SessionState m_sessionState;
        };

    }
} // DevDriver
