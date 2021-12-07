#pragma once

#include <ddRpcServer.h>
#include <ddRpcServer.h> // Test the include guards

#include <ddRpcClient.h>
#include <ddRpcClient.h> // Test the include guards

#include <ddTestUtil.h>

/// A pre-connected RPC client/server test fixture
/// This fixture provides an RPC client/server pair
class ddRpcTest : public DDNetworkedTest
{
protected:
    void SetUp();
    void TearDown();

    DDRpcServer m_hServer = DD_API_INVALID_HANDLE;
    DDRpcClient m_hClient = DD_API_INVALID_HANDLE;
};

/// A test fixture that already has a pre-registered service + functions
class ddRpcServiceTest : public ddRpcTest
{
protected:
    void SetUp();
    void TearDown();
};
