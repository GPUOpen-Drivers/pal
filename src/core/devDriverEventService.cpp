/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/devDriverEventService.h"
#include "core/devDriverEventServiceConv.h"
#include "core/eventDefs.h"
#include "core/gpuMemory.h"

#include "palSysUtil.h"

#include "util/rmtTokens.h"
#include "util/rmtResourceDescriptions.h"

using namespace DevDriver;

namespace Pal
{
// =====================================================================================================================
EventService::EventService(const AllocCb& allocCb)
    : m_rmtWriter(allocCb)
    , m_isMemoryProfilingEnabled(false)
    , m_isInitialized(false)
{
}

// =====================================================================================================================
EventService::~EventService()
{
}

// =====================================================================================================================
DevDriver::Result EventService::HandleRequest(
    IURIRequestContext* pContext)
{
    DD_ASSERT(pContext != nullptr);

    // Make sure we aren't logging while we handle a network request
    Platform::LockGuard<Platform::Mutex> lock(m_mutex);

    DevDriver::Result result = DevDriver::Result::Unavailable;

    const char* const pArgDelim = " ";
    char* pStrtokContext = nullptr;

    // Safety note: Strtok handles nullptr by returning nullptr. We handle that below.
    char* pCmdName = Platform::Strtok(pContext->GetRequestArguments(), pArgDelim, &pStrtokContext);
    char* pCmdArg1 = Platform::Strtok(nullptr, pArgDelim, &pStrtokContext);

    if (strcmp(pCmdName, "enableMemoryProfiling") == 0)
    {
        if (m_isMemoryProfilingEnabled == false)
        {
            m_isMemoryProfilingEnabled = true;
            result = DevDriver::Result::Success;

            m_rmtWriter.Init();
            m_rmtWriter.BeginDataChunk(Util::GetIdOfCurrentProcess(), 0);
        }
    }
    else if (strcmp(pCmdName, "disableMemoryProfiling") == 0)
    {
        if (m_isMemoryProfilingEnabled)
        {
            m_isMemoryProfilingEnabled = false;
            result = DevDriver::Result::Success;

            m_rmtWriter.EndDataChunk();
            m_rmtWriter.Finalize();

            const size_t rmtDataSize = m_rmtWriter.GetRmtDataSize();
            if (rmtDataSize > 0)
            {
                IByteWriter* pWriter = nullptr;
                result = pContext->BeginByteResponse(&pWriter);
                if (result == DevDriver::Result::Success)
                {
                    pWriter->WriteBytes(m_rmtWriter.GetRmtData(), rmtDataSize);

                    result = pWriter->End();
                }
            }
        }
    }

    return result;
}

void EventService::WriteTokenData(const DevDriver::RMT_TOKEN_DATA& token)
{
    // Make sure we aren't logging while we handle a network request
    Platform::LockGuard<Platform::Mutex> lock(m_mutex);
    if (IsMemoryProfilingEnabled())
    {
        m_rmtWriter.WriteTokenData(token);
    }
}

} // Pal
