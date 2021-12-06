#pragma once

#include <ddEventParser.h>
#include <ddEventParser.h> // Test the include guards

#include <ddEventServer.h>
#include <ddEventServer.h> // Test the include guards

#include <ddEventClient.h>
#include <ddEventClient.h> // Test the include guards

#include <ddTestUtil.h>

/// Constants used by the tests
static constexpr uint32_t kProviderIdBase = 0x8000;
static constexpr uint32_t kNumProviders = 4;
static constexpr uint32_t kNumEventsPerProvider = 8;
static constexpr uint32_t kTotalNumEvents = kNumProviders * kNumEventsPerProvider;
static constexpr size_t   kPayloadSizeInBytes = 4096;
static constexpr uint32_t kNumEventsPerThread = 64 * 1024;

/// A pre-connected Event client/server test fixture
/// This fixture provides an Event client/server pair
class DDEventTest : public DDNetworkedTest
{
public:
    DDEventTest();

protected:
    void SetUp();
    void TearDown();
    void OnEnabled();
    void OnDisabled();
    static bool VerifyPayloadData(const void* pPayload, size_t payloadSize);

    DDEventServer m_hServer = DD_API_INVALID_HANDLE;
    DDEventClient m_hClient = DD_API_INVALID_HANDLE;

    DDEventProvider m_providers[kNumProviders];

    uint32_t m_providerEnableCount  = 0;
    uint32_t m_providerDisableCount = 0;

    uint8_t m_payloadData[kPayloadSizeInBytes];

    void* m_pClientUserdata = nullptr;
};

