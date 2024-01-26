/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <eventStreamer.h>

#include <ddPlatform.h>

#include <util/ddByteReader.h>

using namespace DevDriver;

namespace Event
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Default constructor. Initializes an inactive EventStreamer.
EventStreamer::EventStreamer()
    : m_hEventClient(DD_API_INVALID_HANDLE)
    , m_hEventParser(DD_API_INVALID_HANDLE)
    , m_exitRequested(false)
    , m_isStreaming(false)
    , m_encounteredErrors(false)
    , m_hasValidCallback(false)
    , m_eventPayloadBuffer(Platform::GenericAllocCb)
    , m_eventCb()
    {}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Default destructor. An EventStreamer should only be destroyed after EndStreaming() has been called.
EventStreamer::~EventStreamer()
{
    // A streamer should never be destroyed while it's still in the process of streaming event data.
    DD_ASSERT(IsStreaming() == false);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Configures the EventStreamer to begin receiving events from the specified provider and client.
/// When an event is received, the callback function will be invoked with fully-formed event data and metadata.
DD_RESULT EventStreamer::BeginStreaming(const DDEventStreamerCreateInfo& createInfo)
{
    // Attempt to enable the desired provider
    DDEventClientCreateInfo clientInfo = {};
    clientInfo.hConnection        = createInfo.hConnection;
    clientInfo.clientId           = createInfo.clientId;
    clientInfo.dataCb.pUserdata   = this;
    clientInfo.dataCb.pfnCallback = [](void* pUserdata, const void* pData, size_t dataSize) {
        reinterpret_cast<EventStreamer*>(pUserdata)->OnEventData(pData, dataSize);
    };

    SetEventCallback(createInfo.onEventCb);

    DD_RESULT result = ddEventClientCreate(&clientInfo, &m_hEventClient);

    if (result == DD_RESULT_SUCCESS)
    {
        DDEventParserCreateInfo parserInfo = {};
        parserInfo.writer.pUserdata = this;
        parserInfo.writer.pfnBegin = [](
            void* pUserdata,
            const DDEventParserEventInfo* pEvent,
            uint64_t totalPayloadSize) -> DD_RESULT {
            return reinterpret_cast<EventStreamer*>(pUserdata)->ParserOnBegin(*pEvent, static_cast<size_t>(totalPayloadSize));
        };

        parserInfo.writer.pfnWritePayloadChunk = [](
            void* pUserdata,
            const DDEventParserEventInfo* pEvent,
            const void* pData, uint64_t dataSize) -> DD_RESULT {
            return reinterpret_cast<EventStreamer*>(pUserdata)->ParserOnWritePayloadChunk(*pEvent, pData, static_cast<size_t>(dataSize));
        };

        parserInfo.writer.pfnEnd = [](
            void* pUserdata,
            const DDEventParserEventInfo* pEvent,
            DD_RESULT finalResult) {
            return reinterpret_cast<EventStreamer*>(pUserdata)->ParserOnEnd(*pEvent, finalResult);
        };

        result = ddEventParserCreate(&parserInfo, &m_hEventParser);
    }
    else
    {
        ddEventClientDestroy(m_hEventClient);
        ddEventParserDestroy(m_hEventParser);
    }

    if (result == DD_RESULT_SUCCESS)
    {
        result = ddEventClientEnableProviders(m_hEventClient, 1, &createInfo.providerId);
        if (result == DD_RESULT_SUCCESS)
        {
            // Start the thread that will pull event data
            m_exitRequested = false;
            result = DevDriverToDDResult(
                m_eventThread.Start([] (void* pUserdata) {
                    static_cast<EventStreamer*>(pUserdata)->EventReceiveThreadFunc();
                }
            , this));

            if (result == DD_RESULT_SUCCESS)
            {
                // We've successfully started the streaming process
                m_isStreaming = true;
            }
            else
            {
                // We failed to start our thread, but we did remotely enable the event provider.
                // We need to attempt to turn off the remote event provider before returning a failure.
                ddEventClientDisableProviders(m_hEventClient, 1, &createInfo.providerId);
            }
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Signals the EventStreamer to cease from receiving events and begin shutdown procedures.
/// Must be called before the EventStreamer is destroyed.
DD_RESULT EventStreamer::EndStreaming()
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    // Shut down the streaming thread
    if (m_eventThread.IsJoinable())
    {
        m_exitRequested = true;

        result = DevDriverToDDResult(m_eventThread.Join(1000));
    }

    if (result == DD_RESULT_SUCCESS)
    {
        ddEventParserDestroy(m_hEventParser);
        ddEventClientDestroy(m_hEventClient);

        m_isStreaming = false;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Called by the DDEventClient registered to this class when new event data is available.
///
/// During this method's execution, a DDEventParser is used to parse the event data; it is during the event streamer's
/// execution where the other internal callback functions - namely, EventStreamer::ParserOnBegin,
/// EventStreamer::ParserOnWritePayloadChunk, and EventStreamer::ParserOnEnd - are called to properly stream and
/// parse the event data before handing it off to the user-defined EventStreamerOnEventCallback.
void EventStreamer::OnEventData(const void* pData, size_t dataSize)
{
    const DD_RESULT result = ddEventParserParse(m_hEventParser, pData, dataSize);

    if (result != DD_RESULT_SUCCESS)
    {
        DD_WARN_REASON("Encountered errors during event token parsing!");
        m_encounteredErrors = true;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// This function is called by ddEventParserParse when an event has been received by the event client. It handles setup
/// required prior to the reception of event payload data.
///
/// The payload size ("totalPayloadSize") is provided to give an opportunity to allocate memory up-front.
/// The "eventInfo" parameter contains timestamp info, the event ID, and information on the provider generating the
/// event.
///
/// In the current implementation, this function is used solely to reserve space in our payload buffer.
/// The eventInfo parameter can be safely ignored.
DD_RESULT EventStreamer::ParserOnBegin(const DDEventParserEventInfo& eventInfo, size_t totalPayloadSize)
{
    DD_UNUSED(eventInfo);

    m_eventPayloadBuffer.Reserve(totalPayloadSize);

    return DD_RESULT_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// This function receives and handles the streaming of each individual chunk comprising the total event payload.
///
/// This function will be called one or more times after ParserOnBegin. On each call, the same eventInfo is provided as
/// was sent to ParserOnBegin, as well as a pointer to the next chunk of data for the payload.
/// The payload is broken into chunks in this way to handle cases where event data is very large and must be sent over
/// multiple network transactions.
///
/// The data pointer is only valid for the duration of the function call. Once this function returns, there is no
/// guarantee that the memory will remain.
///
/// For the current implementation, we simply copy the event payload data into the log message buffer, which is then
/// flushed to the logging output destination(s) during ParserOnEnd. Any data contained within eventInfo is ignored.
DD_RESULT EventStreamer::ParserOnWritePayloadChunk(
    const DDEventParserEventInfo& eventInfo,
    const void* pData,
    size_t dataSize)
{
    DD_UNUSED(eventInfo);

    const size_t offset = m_eventPayloadBuffer.Grow(dataSize);
    memcpy(&m_eventPayloadBuffer[offset], pData, dataSize);

    return DD_RESULT_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// This function is called once all payload data has been written through calls to ParserOnWritePayloadChunk.
///
/// The same eventInfo is provided, as well as a return code ("finalResult") indicating whether any errors were
/// present during the parsing of event data.
///
/// For the current implementation, the result of event parsing is logged, and, if successful, the final event
/// message is written to all active output logging destination(s).
DD_RESULT EventStreamer::ParserOnEnd(const DDEventParserEventInfo& eventInfo, DD_RESULT finalResult)
{
    if (m_hasValidCallback && finalResult == DD_RESULT_SUCCESS)
    {
        DD_ASSERT(m_eventCb.pfnCallback != nullptr);

        // Assuming everything is A-OK, just pass the event payload to
        // the user-defined callback
        m_eventCb.pfnCallback(
            m_eventCb.pUserdata,
            eventInfo,
            m_eventPayloadBuffer.Data(),
            m_eventPayloadBuffer.Size(),
            finalResult);
    }

    m_eventPayloadBuffer.Clear();

    return finalResult;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Receives and handles the event data from the event server.
void EventStreamer::EventReceiveThreadFunc()
{
    while ((m_exitRequested == false) && (m_encounteredErrors == false))
    {
        // Attempt to read some event data
        const DD_RESULT result = ddEventClientReadEventData(m_hEventClient, 100);

        if ((result == DD_RESULT_SUCCESS) || (result == DD_RESULT_DD_GENERIC_NOT_READY))
        {
            // We've either read event data successfully, or we've timed out.
            // Both of the situations are expected and we don't need to do anything special here.
        }
        else if (result == DD_RESULT_DD_GENERIC_END_OF_STREAM)
        {
            // The client disconnected. Break out of the read loop since we won't be receiving any more messages.
            break;
        }
        else
        {
            // We've encountered some sort of error so we should exit the loop to avoid further issues.
            m_encounteredErrors = true;
            break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Updates the callback function trigged when an event is received.
/// To remove an active callback, this method should be called with the pfnCallback parameter
/// set to nullptr. This will also allow events to be silently discarded.
void EventStreamer::SetEventCallback(const DDEventStreamerCallback& eventCb)
{
    m_hasValidCallback = (eventCb.pfnCallback != nullptr);
    m_eventCb = eventCb;
}

}; // namespace Event
