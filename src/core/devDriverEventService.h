/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "ddUriInterface.h"
#include "util/rmtWriter.h"
#include "palEventDefs.h"
#include "util/hashMap.h"

namespace Pal
{
    // String used to identify the service
    DD_STATIC_CONST char kEventServiceName[] = "event";

    DD_STATIC_CONST DevDriver::Version kEventServiceVersion = 1;

    class EventService : public DevDriver::IService
    {
    public:
        EventService(const DevDriver::AllocCb& allocCb);
        ~EventService();

        // Returns the name of the service
        const char* GetName() const override final { return kEventServiceName; }
        DevDriver::Version GetVersion() const override final { return kEventServiceVersion; }

        // Handles an incoming URI request
        DevDriver::Result HandleRequest(DevDriver::IURIRequestContext* pContext) override final;

        // Logs a Pal event
        void LogEvent(PalEvent eventId, const void* pEventData, size_t eventDataSize);

        // Returns true if memory profiling has been enabled
        bool IsMemoryProfilingEnabled() const { return m_isMemoryProfilingEnabled; }

    private:
        void LogResourceCreateEvent(uint8 delta, const void* pEventData, size_t eventDataSize);
        void WriteUserdataStringToken(uint8 delta, const char* pSnapshotName, bool isSnapshot);

        DevDriver::Platform::Mutex m_mutex;
        DevDriver::RmtWriter       m_rmtWriter;
        bool                       m_isMemoryProfilingEnabled;
        bool                       m_isInitialized;
};
} // DevDriPal
