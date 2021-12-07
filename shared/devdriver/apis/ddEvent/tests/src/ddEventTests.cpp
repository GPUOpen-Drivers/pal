#include <ddEventTests.h>
#include <ddCommon.h>

#include <util/hashSet.h>

#include <thread>

using namespace DevDriver;

/// Unit Tests /////////////////////////////////////////////////////////////////////////////////////

/// Parser Tests ///////////////////////////////////////////////////////////////////////////////////

typedef DD_RESULT (*PFN_VerifyEvent)(void* pUserdata, const DDEventParserEventInfo* pInfo, size_t payloadSize, const void* pPayload);
struct VerifyEventCallback
{
    void*           pUserdata;
    PFN_VerifyEvent pfnVerifyEvent;
};

class EventVerifier
{
public:
    EventVerifier(const VerifyEventCallback& callback)
        : m_hParser(DD_API_INVALID_HANDLE)
        , m_callback(callback)
        , m_numEventsParsed(0)
        , m_payload(Platform::GenericAllocCb)
        , m_payloadOffset(0)
        , m_hasEncounteredErrors(false)
    {
    }

    ~EventVerifier()
    {
        ddEventParserDestroy(m_hParser);
    }

    DD_RESULT Initialize()
    {
        DDEventParserCreateInfo info = {};
        info.writer.pUserdata = this;
        info.writer.pfnBegin = [](void* pUserdata, const DDEventParserEventInfo* pEvent, uint64_t totalPayloadSize) -> DD_RESULT {
            return reinterpret_cast<EventVerifier*>(pUserdata)->Begin(pEvent, totalPayloadSize);
        };
        info.writer.pfnWritePayloadChunk = [](void* pUserdata, const DDEventParserEventInfo* pEvent, const void* pData, uint64_t dataSize) -> DD_RESULT {
            return reinterpret_cast<EventVerifier*>(pUserdata)->WritePayloadChunk(pEvent, pData, static_cast<size_t>(dataSize));
        };
        info.writer.pfnEnd = [](void* pUserdata, const DDEventParserEventInfo* pEvent, DD_RESULT finalResult) {
            return reinterpret_cast<EventVerifier*>(pUserdata)->End(pEvent, finalResult);
        };

        return ddEventParserCreate(&info, &m_hParser);
    }

    void Verify(const void* pData, size_t dataSize)
    {
        if (ddEventParserParse(m_hParser, pData, dataSize) != DD_RESULT_SUCCESS)
        {
            m_hasEncounteredErrors = true;
        }
    }

    size_t GetNumEventsParsed() const { return m_numEventsParsed; }

    bool HasEncounteredErrors() const { return m_hasEncounteredErrors; }

private:
    DD_RESULT Begin(const DDEventParserEventInfo* pEvent, uint64_t totalPayloadSize)
    {
        DD_UNUSED(pEvent);

        m_payloadOffset = 0;
        m_payload.Resize(static_cast<size_t>(totalPayloadSize));

        return DD_RESULT_SUCCESS;
    }

    DD_RESULT WritePayloadChunk(const DDEventParserEventInfo* pEvent, const void* pData, size_t dataSize)
    {
        DD_ASSERT((m_payloadOffset + dataSize) <= m_payload.Size());

        DD_UNUSED(pEvent);

        memcpy(m_payload.Data() + m_payloadOffset, pData, dataSize);
        m_payloadOffset += dataSize;

        return DD_RESULT_SUCCESS;
    }

    DD_RESULT End(const DDEventParserEventInfo* pEvent, DD_RESULT finalResult)
    {
        if (finalResult == DD_RESULT_SUCCESS)
        {
            finalResult = m_callback.pfnVerifyEvent(m_callback.pUserdata, pEvent, m_payload.Size(), m_payload.Data());
        }

        if (finalResult == DD_RESULT_SUCCESS)
        {
            ++m_numEventsParsed;
        }

        return finalResult;
    }

    DDEventParser           m_hParser;
    VerifyEventCallback     m_callback;
    uint32_t                m_numEventsParsed;
    Vector<uint8_t>         m_payload;
    size_t                  m_payloadOffset;
    bool                    m_hasEncounteredErrors;
};

void EventDataCallback(
    void*       pUserdata,
    const void* pData,
    size_t      dataSize)
{
    if (pUserdata != nullptr)
    {
        EventVerifier* pVerifier = *reinterpret_cast<EventVerifier**>(pUserdata);
        pVerifier->Verify(pData, dataSize);
    }
}

/// Check that event parser calls validate their inputs sensibly
TEST_F(DDNoNetworkTest, ParserCreate_InvalidArgs)
{
    // Missing both params
    ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddEventParserCreate(nullptr, nullptr));

    // Missing CreateInfo
    {
        DDEventParser hParser;
        ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddEventParserCreate(nullptr, &hParser));
    }

    // Missing ClientId
    {
        DDEventParserCreateInfo info = {};
        ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddEventParserCreate(&info, nullptr));
    }

    // Both arguments, but not filled out correctly.
    {
        DDEventParser           hParser;
        DDEventParserCreateInfo info = {}; // left empty intentionally
        ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddEventParserCreate(&info, &hParser));
    }

    // TODO: Partially filled out writer tests

    // Don't crash on destroy nullptr
    ddEventParserDestroy(nullptr);
}

/// Client Tests ///////////////////////////////////////////////////////////////////////////////////

/// Check that `ddEventClientCreate()` calls validate their inputs sensibly
TEST_F(DDNoNetworkTest, ClientCreate_InvalidArgs)
{
    // Missing both params
    ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddEventClientCreate(nullptr, nullptr));

    // Missing CreateInfo
    {
        DDEventClient eventClient;
        ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddEventClientCreate(nullptr, &eventClient));
    }

    // Missing ClientId
    {
        DDEventClientCreateInfo info = {};
        ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddEventClientCreate(&info, nullptr));
    }

    // Both arguments, but not filled out correctly.
    {
        DDEventClient           eventClient;
        DDEventClientCreateInfo info = {}; // left empty intentionally
        ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddEventClientCreate(&info, &eventClient));
    }
    {
        DDEventClient           eventClient;
        DDEventClientCreateInfo info = {};
        ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddEventClientCreate(&info, &eventClient));
    }
    {
        DDEventClient           eventClient;
        DDEventClientCreateInfo info = {};
        info.hConnection           = reinterpret_cast<DDNetConnection>(1); // hahaha don't do this
        ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddEventClientCreate(&info, &eventClient));
    }

    // Don't crash on destroy nullptr
    ddEventClientDestroy(nullptr);
}

TEST_F(DDNetworkedTest, ClientCreate_InvalidArgs)
{
    DDEventClientCreateInfo info = {};
    info.hConnection             = m_hClientConnection;

    // Case: Create with invalid client id
    {
        info.clientId = DD_API_INVALID_CLIENT_ID;
        DDEventClient hClient;
        ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddEventClientCreate(&info, &hClient));
    }

    // Case: Create with inactive client id and invalid callback
    {
        info.clientId    = 1;   // This is valid but very unlikely to be live
        info.timeoutInMs = 100; // Make sure we don't waste too much time attempting to connect
        DDEventClient hClient;
        ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddEventClientCreate(&info, &hClient));
    }
}

/// Server Tests ///////////////////////////////////////////////////////////////////////////////////

/// Check that `Create()` calls validate their inputs sensibly
TEST_F(DDNoNetworkTest, ServerCreate_InvalidArgs)
{
    DDEventServerCreateInfo info = {};
    info.hConnection = nullptr;

    DDEventServer hServer = nullptr;

    // Missing both parameters
    ASSERT_EQ(ddEventServerCreate(nullptr, nullptr), DD_RESULT_COMMON_INVALID_PARAMETER);

    // Missing server pointer
    ASSERT_EQ(ddEventServerCreate(&info, nullptr), DD_RESULT_COMMON_INVALID_PARAMETER);

    // Missing info pointer
    ASSERT_EQ(ddEventServerCreate(nullptr, &hServer), DD_RESULT_COMMON_INVALID_PARAMETER);

    // Bad message channel
    info.hConnection = nullptr;
    ASSERT_EQ(ddEventServerCreate(&info, &hServer), DD_RESULT_COMMON_INVALID_PARAMETER);

    // Don't crash on destroy nullptr
    ddEventServerDestroy(nullptr);
}

TEST_F(DDNetworkedTest, ServerCreateProvider_InvalidArgs)
{
    DDEventServerCreateInfo info = {};
    info.hConnection = m_hServerConnection;

    DDEventServer hServer = nullptr;
    ASSERT_EQ(ddEventServerCreate(&info, &hServer), DD_RESULT_SUCCESS);

    ASSERT_EQ(ddEventServerCreateProvider(nullptr, nullptr), DD_RESULT_COMMON_INVALID_PARAMETER);

    DDEventProviderCreateInfo providerInfo  = {};
    ASSERT_EQ(ddEventServerCreateProvider(&providerInfo, nullptr), DD_RESULT_COMMON_INVALID_PARAMETER);

    DDEventProvider hProvider = nullptr;
    ASSERT_EQ(ddEventServerCreateProvider(&providerInfo, &hProvider), DD_RESULT_COMMON_INVALID_PARAMETER);

    providerInfo.hServer = hServer;
    ASSERT_EQ(ddEventServerCreateProvider(&providerInfo, &hProvider), DD_RESULT_COMMON_INVALID_PARAMETER);

    providerInfo.id = 0xbeef0000;
    ASSERT_EQ(ddEventServerCreateProvider(&providerInfo, &hProvider), DD_RESULT_COMMON_INVALID_PARAMETER);

    ddEventServerDestroy(hServer);
}

TEST_F(DDNetworkedTest, ServerProviderEmit_InvalidArgs)
{
    DDEventServerCreateInfo info = {};
    info.hConnection = m_hServerConnection;

    DDEventServer hServer = nullptr;
    ASSERT_EQ(ddEventServerCreate(&info, &hServer), DD_RESULT_SUCCESS);

    DDEventProviderCreateInfo providerInfo  = {};
    providerInfo.hServer = hServer;
    providerInfo.id = 0xbeef0000;
    providerInfo.numEvents = 1;

    DDEventProvider hProvider = nullptr;
    ASSERT_EQ(ddEventServerCreateProvider(&providerInfo, &hProvider), DD_RESULT_SUCCESS);

    ASSERT_EQ(ddEventServerEmit(
        nullptr,
        0,
        0,
        nullptr
    ), DD_RESULT_COMMON_INVALID_PARAMETER);

    ASSERT_EQ(ddEventServerEmit(
        hProvider,
        0,
        1,
        nullptr
    ), DD_RESULT_COMMON_INVALID_PARAMETER);

    ASSERT_EQ(ddEventServerEmitWithHeader(
        nullptr,
        0,
        0,
        nullptr,
        0,
        nullptr
    ), DD_RESULT_COMMON_INVALID_PARAMETER);

    ASSERT_EQ(ddEventServerEmitWithHeader(
        hProvider,
        0,
        0,
        nullptr,
        1,
        nullptr
    ), DD_RESULT_COMMON_INVALID_PARAMETER);

    ASSERT_EQ(ddEventServerEmitWithHeader(
        hProvider,
        0,
        1,
        nullptr,
        1,
        nullptr
    ), DD_RESULT_COMMON_INVALID_PARAMETER);

    ASSERT_EQ(ddEventServerEmitWithHeader(
        hProvider,
        0,
        1,
        nullptr,
        0,
        nullptr
    ), DD_RESULT_COMMON_INVALID_PARAMETER);

    ddEventServerDestroyProvider(hProvider);
    ddEventServerDestroy(hServer);
}

/// Combined Tests ////////////////////////////////////////////////////////////////////////////

// Case: Connect with valid client id
TEST_F(DDNetworkedTest, CheckValidConnection)
{
    // Setup a server that does nothing
    DDEventServerCreateInfo serverInfo = {};
    serverInfo.hConnection = m_hServerConnection;

    DDEventServer hEventServer = DD_API_INVALID_HANDLE;
    ASSERT_EQ(DD_RESULT_SUCCESS, ddEventServerCreate(&serverInfo, &hEventServer));

    // Setup a client and attempt to connect to our server
    DDEventClientCreateInfo clientInfo = {};
    clientInfo.hConnection        = m_hClientConnection;
    clientInfo.clientId           = m_serverClientId;
    clientInfo.dataCb.pfnCallback = EventDataCallback;

    DDEventClient hEventClient = DD_API_INVALID_HANDLE;
    ASSERT_EQ(DD_RESULT_SUCCESS, ddEventClientCreate(&clientInfo, &hEventClient));

    ddEventClientDestroy(hEventClient);
    ddEventServerDestroy(hEventServer);
}

TEST_F(DDEventTest, EmitDisabledProviders)
{
    for (uint32_t providerIndex = 0; providerIndex < kNumProviders; ++providerIndex)
    {
        const DDEventProvider hProvider = m_providers[providerIndex];
        for (uint32_t eventIndex = 0; eventIndex < kNumEventsPerProvider; ++eventIndex)
        {
            ASSERT_EQ(
                DD_RESULT_DD_EVENT_EMIT_PROVIDER_DISABLED,
                ddEventServerEmit(hProvider, eventIndex, 0, nullptr));
        }
    }
}

TEST_F(DDEventTest, TestEmitDisabledProviders)
{
    for (uint32_t providerIndex = 0; providerIndex < kNumProviders; ++providerIndex)
    {
        const DDEventProvider hProvider = m_providers[providerIndex];
        for (uint32_t eventIndex = 0; eventIndex < kNumEventsPerProvider; ++eventIndex)
        {
            ASSERT_EQ(
                DD_RESULT_DD_EVENT_EMIT_PROVIDER_DISABLED,
                ddEventServerTestEmit(hProvider, eventIndex));
        }
    }
}

TEST_F(DDEventTest, QueryProviders)
{
    HashSet<uint32_t> providerIds(Platform::GenericAllocCb);
    for (uint32_t providerIndex = 0; providerIndex < kNumProviders; ++providerIndex)
    {
        ASSERT_EQ(providerIds.Insert(kProviderIdBase + providerIndex), Result::Success);
    }

    DDEventProviderVisitor visitor = {};
    visitor.pUserdata = &providerIds;
    visitor.pfnVisit = [](void* pUserdata, const DDEventProviderDesc* pProvider) -> DD_RESULT {
        const HashSet<uint32_t>* pProviderIds = reinterpret_cast<const HashSet<uint32_t>*>(pUserdata);

        const DD_RESULT result = pProviderIds->Contains(pProvider->providerId) ? DD_RESULT_SUCCESS
                                                                               : DD_RESULT_COMMON_UNKNOWN;

        return result;
    };
    ASSERT_EQ(ddEventClientQueryProviders(m_hClient, &visitor), DD_RESULT_SUCCESS);
}

struct ProviderUpdateContainer
{
    ProviderUpdateContainer()
        : providerDescs(Platform::GenericAllocCb)
        , eventStatusBuffer(Platform::GenericAllocCb)
        , totalEvents(0)
    {
    }

    // TODO: This is awful, we need a better API
    Vector<DDEventProviderDesc> providerDescs;
    Vector<DDEventEnabledStatus> eventStatusBuffer;
    size_t totalEvents;
};

TEST_F(DDEventTest, ConfigureProviders)
{
    ProviderUpdateContainer container;

    DDEventProviderVisitor visitor = {};
    visitor.pUserdata = &container;
    visitor.pfnVisit = [](void* pUserdata, const DDEventProviderDesc* pProvider) -> DD_RESULT {
        auto* pContainer = reinterpret_cast<ProviderUpdateContainer*>(pUserdata);

        pContainer->providerDescs.PushBack(*pProvider);

        pContainer->totalEvents += pProvider->numEvents;

        return DD_RESULT_SUCCESS;
    };
    ASSERT_EQ(ddEventClientQueryProviders(m_hClient, &visitor), DD_RESULT_SUCCESS);

    container.eventStatusBuffer.Resize(container.totalEvents);

    size_t eventStatusOffset = 0;
    for (size_t providerIndex = 0; providerIndex < container.providerDescs.Size(); ++providerIndex)
    {
        DDEventProviderDesc& desc = container.providerDescs[providerIndex];
        desc.providerStatus.isEnabled = true;

        desc.pEventStatus = &container.eventStatusBuffer[eventStatusOffset];

        for (uint32_t eventStatusIndex = 0; eventStatusIndex < desc.numEvents; ++eventStatusIndex)
        {
            container.eventStatusBuffer[eventStatusOffset + eventStatusIndex].isEnabled = true;
        }

        eventStatusOffset += desc.numEvents;
    }

    ASSERT_EQ(ddEventClientConfigureProviders(m_hClient, container.providerDescs.Size(), container.providerDescs.Data()), DD_RESULT_SUCCESS);
}

TEST_F(DDEventTest, ProviderStateChange)
{
    ASSERT_EQ(m_providerEnableCount, 0u);
    ASSERT_EQ(m_providerDisableCount, 0u);

    // Enable the provider
    const uint32_t providerId = kProviderIdBase;
    ASSERT_EQ(ddEventClientEnableProviders(m_hClient, 1, &providerId), DD_RESULT_SUCCESS);

    ASSERT_EQ(m_providerEnableCount, 1u);
    ASSERT_EQ(m_providerDisableCount, 0u);

    // Disable the provider
    ASSERT_EQ(ddEventClientDisableProviders(m_hClient, 1, &providerId), DD_RESULT_SUCCESS);

    ASSERT_EQ(m_providerEnableCount, 1u);
    ASSERT_EQ(m_providerDisableCount, 1u);
}

TEST_F(DDEventTest, SingleEventTransfer_NoPayload)
{
    // Enable the provider
    const uint32_t providerId = kProviderIdBase;
    ASSERT_EQ(ddEventClientEnableProviders(m_hClient, 1, &providerId), DD_RESULT_SUCCESS);

    // Emit an event
    ASSERT_EQ(ddEventServerEmit(m_providers[0], 0, 0, nullptr), DD_RESULT_SUCCESS);

    VerifyEventCallback verifyCallback = {};
    verifyCallback.pfnVerifyEvent = [](void* pUserdata, const DDEventParserEventInfo* pInfo, size_t payloadSize, const void* pPayload) -> DD_RESULT {
        DD_UNUSED(pUserdata);

        return ((pInfo->providerId == kProviderIdBase) &&
                (pInfo->eventId == 0)                  &&
                (pInfo->eventIndex == 0)               &&
                (payloadSize == 0)                     &&
                (pPayload == nullptr)) ? DD_RESULT_SUCCESS : DD_RESULT_COMMON_UNKNOWN;
    };

    EventVerifier verifier(verifyCallback);
    ASSERT_EQ(verifier.Initialize(), DD_RESULT_SUCCESS);

    m_pClientUserdata = &verifier;

    DD_RESULT result = DD_RESULT_SUCCESS;
    while ((result == DD_RESULT_SUCCESS) && (verifier.GetNumEventsParsed() < 1))
    {
        // Receive event data
        result = ddEventClientReadEventData(m_hClient, 250);
    }
    ASSERT_EQ(result, DD_RESULT_SUCCESS);
    ASSERT_EQ(verifier.GetNumEventsParsed(), 1u);
    ASSERT_FALSE(verifier.HasEncounteredErrors());
}

TEST_F(DDEventTest, MultipleEventTransfer_NoPayload)
{
    // Enable all providers
    uint32_t providerIds[kNumProviders];
    for (uint32_t providerIndex = 0; providerIndex < kNumProviders; ++providerIndex)
    {
        providerIds[providerIndex] = kProviderIdBase + providerIndex;
    }
    ASSERT_EQ(ddEventClientEnableProviders(m_hClient, Platform::ArraySize(providerIds), providerIds), DD_RESULT_SUCCESS);

    // Emit all events
    for (uint32_t providerIndex = 0; providerIndex < kNumProviders; ++providerIndex)
    {
        const DDEventProvider hProvider = m_providers[providerIndex];
        for (uint32_t eventIndex = 0; eventIndex < kNumEventsPerProvider; ++eventIndex)
        {
            ASSERT_EQ(
                DD_RESULT_SUCCESS,
                ddEventServerEmit(hProvider, eventIndex, 0, nullptr));
        }
    }

    uint32_t expectedEventIndices[kNumProviders];
    memset(expectedEventIndices, 0, sizeof(expectedEventIndices));

    VerifyEventCallback verifyCallback = {};
    verifyCallback.pUserdata = expectedEventIndices;
    verifyCallback.pfnVerifyEvent = [](void* pUserdata, const DDEventParserEventInfo* pInfo, size_t payloadSize, const void* pPayload) -> DD_RESULT {
        uint32* pExpectedEventIndices = reinterpret_cast<uint32*>(pUserdata);

        bool isValid = false;

        const uint32_t providerIdIndex = pInfo->providerId - kProviderIdBase;
        if (providerIdIndex < kNumProviders)
        {
            const size_t expectedEventIndex = pExpectedEventIndices[providerIdIndex];

            ++pExpectedEventIndices[providerIdIndex];

            if (expectedEventIndex < kNumEventsPerProvider)
            {
                const size_t expectedEventId = expectedEventIndex;

                isValid = ((pInfo->eventId == expectedEventId)       &&
                           (pInfo->eventIndex == expectedEventIndex) &&
                           (payloadSize == 0)                        &&
                           (pPayload == nullptr));
            }
        }

        return isValid ? DD_RESULT_SUCCESS : DD_RESULT_COMMON_UNKNOWN;
    };

    EventVerifier verifier(verifyCallback);
    ASSERT_EQ(verifier.Initialize(), DD_RESULT_SUCCESS);

    m_pClientUserdata = &verifier;

    DD_RESULT result = DD_RESULT_SUCCESS;
    while ((result == DD_RESULT_SUCCESS) && (verifier.GetNumEventsParsed() < kTotalNumEvents))
    {
        // Receive event data
        result = ddEventClientReadEventData(m_hClient, 250);
    }
    ASSERT_EQ(result, DD_RESULT_SUCCESS);
    ASSERT_EQ(verifier.GetNumEventsParsed(), kTotalNumEvents);
    ASSERT_FALSE(verifier.HasEncounteredErrors());
}

TEST_F(DDEventTest, SingleEventTransfer_WithPayload)
{
    // Enable the provider
    const uint32_t providerId = kProviderIdBase;
    ASSERT_EQ(ddEventClientEnableProviders(m_hClient, 1, &providerId), DD_RESULT_SUCCESS);

    // Emit an event
    ASSERT_EQ(ddEventServerEmit(m_providers[0], 0, sizeof(m_payloadData), m_payloadData), DD_RESULT_SUCCESS);

    VerifyEventCallback verifyCallback = {};
    verifyCallback.pfnVerifyEvent = [](void* pUserdata, const DDEventParserEventInfo* pInfo, size_t payloadSize, const void* pPayload) -> DD_RESULT {
        DD_UNUSED(pUserdata);

        bool isValid = ((pInfo->providerId == kProviderIdBase) &&
                (pInfo->eventId == 0)                          &&
                (pInfo->eventIndex == 0)                       &&
                (payloadSize == kPayloadSizeInBytes)           &&
                (pPayload != nullptr));

        if (isValid)
        {
            isValid = VerifyPayloadData(pPayload, payloadSize);
        }

        return isValid ? DD_RESULT_SUCCESS : DD_RESULT_COMMON_UNKNOWN;
    };

    EventVerifier verifier(verifyCallback);
    ASSERT_EQ(verifier.Initialize(), DD_RESULT_SUCCESS);

    m_pClientUserdata = &verifier;

    DD_RESULT result = DD_RESULT_SUCCESS;
    while ((result == DD_RESULT_SUCCESS) && (verifier.GetNumEventsParsed() < 1))
    {
        // Receive event data
        result = ddEventClientReadEventData(m_hClient, 250);
    }
    ASSERT_EQ(result, DD_RESULT_SUCCESS);
    ASSERT_EQ(verifier.GetNumEventsParsed(), 1u);
    ASSERT_FALSE(verifier.HasEncounteredErrors());
}

TEST_F(DDEventTest, MultipleEventTransfer_WithPayload)
{
    // Enable all providers
    uint32_t providerIds[kNumProviders];
    for (uint32_t providerIndex = 0; providerIndex < kNumProviders; ++providerIndex)
    {
        providerIds[providerIndex] = kProviderIdBase + providerIndex;
    }
    ASSERT_EQ(ddEventClientEnableProviders(m_hClient, Platform::ArraySize(providerIds), providerIds), DD_RESULT_SUCCESS);

    // Emit all events
    for (uint32_t providerIndex = 0; providerIndex < kNumProviders; ++providerIndex)
    {
        const DDEventProvider hProvider = m_providers[providerIndex];
        for (uint32_t eventIndex = 0; eventIndex < kNumEventsPerProvider; ++eventIndex)
        {
            ASSERT_EQ(
                DD_RESULT_SUCCESS,
                ddEventServerEmit(hProvider, eventIndex, sizeof(m_payloadData), m_payloadData));
        }
    }

    uint32_t expectedEventIndices[kNumProviders];
    memset(expectedEventIndices, 0, sizeof(expectedEventIndices));

    VerifyEventCallback verifyCallback = {};
    verifyCallback.pUserdata = expectedEventIndices;
    verifyCallback.pfnVerifyEvent = [](void* pUserdata, const DDEventParserEventInfo* pInfo, size_t payloadSize, const void* pPayload) -> DD_RESULT {
        uint32* pExpectedEventIndices = reinterpret_cast<uint32*>(pUserdata);

        bool isValid = false;

        const uint32_t providerIdIndex = pInfo->providerId - kProviderIdBase;
        if (providerIdIndex < kNumProviders)
        {
            const size_t expectedEventIndex = pExpectedEventIndices[providerIdIndex];

            ++pExpectedEventIndices[providerIdIndex];

            if (expectedEventIndex < kNumEventsPerProvider)
            {
                const size_t expectedEventId = expectedEventIndex;

                isValid = ((pInfo->eventId == expectedEventId)       &&
                           (pInfo->eventIndex == expectedEventIndex) &&
                           (payloadSize == kPayloadSizeInBytes)      &&
                           (pPayload != nullptr));
            }
        }

        if (isValid)
        {
            isValid = VerifyPayloadData(pPayload, payloadSize);
        }

        return isValid ? DD_RESULT_SUCCESS : DD_RESULT_COMMON_UNKNOWN;
    };

    EventVerifier verifier(verifyCallback);
    ASSERT_EQ(verifier.Initialize(), DD_RESULT_SUCCESS);

    m_pClientUserdata = &verifier;

    DD_RESULT result = DD_RESULT_SUCCESS;
    while ((result == DD_RESULT_SUCCESS) && (verifier.GetNumEventsParsed() < kTotalNumEvents))
    {
        // Receive event data
        result = ddEventClientReadEventData(m_hClient, 250);
    }
    ASSERT_EQ(result, DD_RESULT_SUCCESS);
    ASSERT_EQ(verifier.GetNumEventsParsed(), kTotalNumEvents);
    ASSERT_FALSE(verifier.HasEncounteredErrors());
}

struct EventThreadContext
{
    DDEventProvider hProvider;
    uint32 threadIndex;
};

TEST_F(DDEventTest, ThreadedEventTransfer_WithPayload)
{
    // Enable the providers
    DDEventProviderCreateInfo providerInfo = {};
    providerInfo.hServer = m_hServer;
    providerInfo.id = 0x1337;
    providerInfo.numEvents = std::thread::hardware_concurrency();

    DDEventProvider hProvider = DD_API_INVALID_HANDLE;
    ASSERT_EQ(DD_RESULT_SUCCESS, ddEventServerCreateProvider(&providerInfo, &hProvider));

    ASSERT_EQ(ddEventClientEnableProviders(m_hClient, 1, &providerInfo.id), DD_RESULT_SUCCESS);

    // Launch event threads
    // BUG: Storing threads in a DevDriver::Vector can lead to crashes: (GH #416)
    Vector<Platform::Thread> threads(Platform::GenericAllocCb);
    Vector<EventThreadContext> threadContexts(Platform::GenericAllocCb);
    Vector<uint32_t> expectedEventIndices(Platform::GenericAllocCb);

    threads.Resize(providerInfo.numEvents);
    threadContexts.Resize(providerInfo.numEvents);
    expectedEventIndices.Resize(providerInfo.numEvents);

    memset(expectedEventIndices.Data(), 0, expectedEventIndices.Size() * sizeof(uint32_t));

    for (uint32 threadIndex = 0; threadIndex < providerInfo.numEvents; ++threadIndex)
    {
        EventThreadContext& threadContext = threadContexts[threadIndex];
        threadContext.hProvider = hProvider;
        threadContext.threadIndex = threadIndex;

        const Result threadResult = threads[threadIndex].Start([](void* pUserdata) {
            EventThreadContext* pContext = reinterpret_cast<EventThreadContext*>(pUserdata);

            for (uint32 eventIndex = 0; eventIndex < kNumEventsPerThread; ++eventIndex)
            {
                ddEventServerEmit(pContext->hProvider, pContext->threadIndex, sizeof(uint32), &eventIndex);
            }
        }, &threadContext);

        ASSERT_EQ(threadResult, Result::Success);
    }

    VerifyEventCallback verifyCallback = {};
    verifyCallback.pUserdata = &expectedEventIndices;
    verifyCallback.pfnVerifyEvent = [](void* pUserdata, const DDEventParserEventInfo* pInfo, size_t payloadSize, const void* pPayload) -> DD_RESULT {
        auto* pExpectedEventIndices = reinterpret_cast<Vector<uint32_t>*>(pUserdata);

        bool isValid = false;

        if (pInfo->providerId == 0x1337)
        {
            const uint32_t expectedEventIndex = (*pExpectedEventIndices)[pInfo->eventId];

            ++(*pExpectedEventIndices)[pInfo->eventId];

            if ((expectedEventIndex < kNumEventsPerThread) &&
                (pPayload != nullptr)                      &&
                (payloadSize == sizeof(uint32_t)))
            {
                uint32_t perThreadEventIndex = 0;
                memcpy(&perThreadEventIndex, pPayload, sizeof(uint32_t));

                isValid = (perThreadEventIndex == expectedEventIndex);
            }
        }

        return isValid ? DD_RESULT_SUCCESS : DD_RESULT_COMMON_UNKNOWN;
    };

    EventVerifier verifier(verifyCallback);
    ASSERT_EQ(verifier.Initialize(), DD_RESULT_SUCCESS);

    m_pClientUserdata = &verifier;

    const size_t totalNumEvents = providerInfo.numEvents * kNumEventsPerThread;

    DD_RESULT result = DD_RESULT_SUCCESS;
    while ((result == DD_RESULT_SUCCESS) && (verifier.HasEncounteredErrors() == false) && (verifier.GetNumEventsParsed() < totalNumEvents))
    {
        // Receive event data
        result = ddEventClientReadEventData(m_hClient, 250);
    }
    ASSERT_EQ(result, DD_RESULT_SUCCESS);
    ASSERT_EQ(verifier.GetNumEventsParsed(), totalNumEvents);
    ASSERT_FALSE(verifier.HasEncounteredErrors());

    // Join event threads
    for (uint32 threadIndex = 0; threadIndex < providerInfo.numEvents; ++threadIndex)
    {
        ASSERT_EQ(threads[threadIndex].Join(1000), Result::Success);
    }

    ddEventServerDestroyProvider(hProvider);
}

/// GTest entry-point
GTEST_API_ int main(int argc, char** argv)
{
    // Run all tests
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

DDEventTest::DDEventTest()
{
    memset(m_providers, 0, sizeof(m_providers));
}

void DDEventTest::SetUp()
{
    DDNetworkedTest::SetUp();

    // Setup a server that does nothing
    DDEventServerCreateInfo serverInfo = {};
    serverInfo.hConnection = m_hServerConnection;

    ASSERT_EQ(DD_RESULT_SUCCESS, ddEventServerCreate(&serverInfo, &m_hServer));

    // Setup a client and attempt to connect to our server
    DDEventClientCreateInfo clientInfo = {};
    clientInfo.hConnection        = m_hClientConnection;
    clientInfo.clientId           = m_serverClientId;
    clientInfo.dataCb.pUserdata   = &m_pClientUserdata;
    clientInfo.dataCb.pfnCallback = EventDataCallback;

    ASSERT_EQ(DD_RESULT_SUCCESS, ddEventClientCreate(&clientInfo, &m_hClient));

    for (uint32_t providerIndex = 0; providerIndex < kNumProviders; ++providerIndex)
    {
        DDEventProviderCreateInfo providerInfo = {};
        providerInfo.hServer = m_hServer;
        providerInfo.id = kProviderIdBase + providerIndex;
        providerInfo.numEvents = kNumEventsPerProvider;

        providerInfo.stateChangeCb.pUserdata = this;
        providerInfo.stateChangeCb.pfnEnabled =
            [](void* pUserdata)
        {
            reinterpret_cast<DDEventTest*>(pUserdata)->OnEnabled();
        };
        providerInfo.stateChangeCb.pfnDisabled =
            [](void* pUserdata)
        {
            reinterpret_cast<DDEventTest*>(pUserdata)->OnDisabled();
        };

        ASSERT_EQ(DD_RESULT_SUCCESS, ddEventServerCreateProvider(&providerInfo, &m_providers[providerIndex]));
    }

    for (size_t byteIndex = 0; byteIndex < kPayloadSizeInBytes; ++byteIndex)
    {
        m_payloadData[byteIndex] = static_cast<uint8_t>(byteIndex % 256);
    }
}

void DDEventTest::TearDown()
{
    for (uint32_t providerIndex = 0; providerIndex < kNumProviders; ++providerIndex)
    {
        ddEventServerDestroyProvider(m_providers[providerIndex]);

        m_providers[providerIndex] = DD_API_INVALID_HANDLE;
    }

    ddEventClientDestroy(m_hClient);
    m_hClient = DD_API_INVALID_HANDLE;

    ddEventServerDestroy(m_hServer);
    m_hServer = DD_API_INVALID_HANDLE;

    DDNetworkedTest::TearDown();
}

void DDEventTest::OnEnabled()
{
    ++m_providerEnableCount;
}

void DDEventTest::OnDisabled()
{
    ++m_providerDisableCount;
}

bool DDEventTest::VerifyPayloadData(
    const void* pPayload,
    size_t      payloadSize)
{
    bool isValid = (payloadSize == kPayloadSizeInBytes);

    if (isValid)
    {
        for (size_t byteIndex = 0; byteIndex < kPayloadSizeInBytes; ++byteIndex)
        {
            if (reinterpret_cast<const uint8_t*>(pPayload)[byteIndex] != static_cast<uint8_t>(byteIndex % 256))
            {
                isValid = false;

                break;
            }
        }
    }

    return isValid;
}

