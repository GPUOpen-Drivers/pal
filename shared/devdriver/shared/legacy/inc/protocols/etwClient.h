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
