
#include <ddNetTests.h>
#include <ddCommon.h>

#include <string>

using namespace DevDriver;

/// Unit Tests /////////////////////////////////////////////////////////////////////////////////////

TEST_F(DDNetworkedTest, BasicTest)
{
    // Do nothing
    // Just verify that the fixture correctly sets up and shuts down
}

TEST_F(DDNetworkedTest, LocalTest)
{
    DDNetConnection hConnection = DD_API_INVALID_HANDLE;
    const DDNetConnectionInfo info = m_router.GenerateLocalInfo();
    ASSERT_EQ(ddNetCreateConnection(&info, &hConnection), DD_RESULT_SUCCESS);
    ddNetDestroyConnection(hConnection);
}

TEST_F(DDNetworkedTest, RemoteTest)
{
    DDNetConnection hConnection = DD_API_INVALID_HANDLE;
    const DDNetConnectionInfo info = m_router.GenerateRemoteInfo();
    ASSERT_EQ(ddNetCreateConnection(&info, &hConnection), DD_RESULT_SUCCESS);
    ddNetDestroyConnection(hConnection);
}

/// Helper structure used by the discovery test
struct DiscoveryTestContext
{
    DDClientId         id;
    DD_NET_CLIENT_TYPE type;
};

TEST_F(DDNetworkedTest, DiscoverTest)
{
    DDClientId clientId = ddNetQueryClientId(m_hClientConnection);

    DiscoveryTestContext context = {};
    context.id = clientId;

    DDNetDiscoverInfo info = {};
    info.pUserdata = &context;
    info.pfnCallback = [](void* pUserdata, const DDNetDiscoveredClientInfo* pInfo) -> int {
        DiscoveryTestContext& context = *reinterpret_cast<DiscoveryTestContext*>(pUserdata);

        bool continueDiscovery = true;

        // Once we've found the tool client we're looking for, we can terminate the search
        if (pInfo->id == context.id)
        {
            // Record the type of the client so we can do some validation later
            context.type = pInfo->type;

            continueDiscovery = false;
        }

        return continueDiscovery ? 1 : 0;
    };
    info.timeoutInMs = 100;
    ASSERT_EQ(ddNetDiscover(m_hServerConnection, &info), DD_RESULT_SUCCESS);

    // Identify if this client has the correct type or not
    ASSERT_EQ(context.type, DD_NET_CLIENT_TYPE_TOOL);
}

/// GTest entry-point
GTEST_API_ int main(int argc, char** argv)
{
    // Run all tests
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

