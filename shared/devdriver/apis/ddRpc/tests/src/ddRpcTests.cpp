
#include <ddRpcTests.h>
#include <ddCommon.h>

#include <util/ddByteReader.h>

#include <string>
#include <algorithm>

using namespace DevDriver;

/// Operator overloads for DDApiVersion
bool operator==(const DDApiVersion& a, const DDApiVersion& b)
{
    return
        (
            (a.major == b.major) &&
            (a.minor == b.minor) &&
            (a.patch == b.patch)
            );
}

bool operator!=(const DDApiVersion& a, const DDApiVersion& b)
{
    return !(a == b);
}

/// Unit Tests /////////////////////////////////////////////////////////////////////////////////////

// Arbitrary protocol id values used for testing
constexpr DDProtocolId kTestProtocolId        = 64;
constexpr DDProtocolId kInvalidTestProtocolId = 63;

constexpr DDRpcServiceId  kInvalidService  = 1000; // A non-zero but invalid service id.
constexpr DDRpcFunctionId kInvalidFunction = 2000; // A non-zero but invalid function id.

const DDRpcServerRegisterServiceInfo kTestServiceInfo = []() -> DDRpcServerRegisterServiceInfo {
    DDRpcServerRegisterServiceInfo info = {};
    info.id                             = 1337;
    info.version.major                  = 1;
    info.version.minor                  = 1;
    info.version.patch                  = 1;
    info.pName                          = "Test";
    info.pDescription                   = "A test service";

    return info;
}();

const DDRpcServerRegisterFunctionInfo kTestFunctionInfo = []() -> DDRpcServerRegisterFunctionInfo {
    DDRpcServerRegisterFunctionInfo info = {};
    info.serviceId                      = kTestServiceInfo.id;
    info.id                             = 64;
    info.pName                          = "TestFunction";
    info.pDescription                   = "A test function";
    info.pfnFuncCb = [](
        const DDRpcServerCallInfo* pCall) -> DD_RESULT
    {
        DD_API_UNUSED(pCall);

        return DD_RESULT_SUCCESS;
    };

    return info;
}();

const DDRpcServerRegisterFunctionInfo kTestVersionFunctionInfo = []() -> DDRpcServerRegisterFunctionInfo {
    DDRpcServerRegisterFunctionInfo info = {};
    info.serviceId                      = kTestServiceInfo.id;
    info.id                             = kTestFunctionInfo.id + 1;
    info.pName                          = "TestFunction";
    info.pDescription                   = "A test function";
    info.pfnFuncCb = [](
        const DDRpcServerCallInfo* pCall) -> DD_RESULT
    {
        return (kTestServiceInfo.version == pCall->version) ? DD_RESULT_SUCCESS
                                                            : DD_RESULT_COMMON_INVALID_PARAMETER;
    };

    return info;
}();

const DDRpcServerRegisterFunctionInfo kTestNoParamNoReturnFunctionInfo = []() -> DDRpcServerRegisterFunctionInfo {
    DDRpcServerRegisterFunctionInfo info = {};
    info.serviceId                      = kTestServiceInfo.id;
    info.id                             = kTestFunctionInfo.id + 2;
    info.pName                          = "TestFunction";
    info.pDescription                   = "A test function";
    info.pfnFuncCb = [](
        const DDRpcServerCallInfo* pCall) -> DD_RESULT
    {
        DD_API_UNUSED(pCall);

        return DD_RESULT_SUCCESS;
    };

    return info;
}();

static const uint64_t kTestReturnData = 0x12345678;
static const uint64_t kTestParamData  = 0x87654321;

const DDRpcServerRegisterFunctionInfo kTestNoParamReturnFunctionInfo = []() -> DDRpcServerRegisterFunctionInfo {
    DDRpcServerRegisterFunctionInfo info = {};
    info.serviceId                      = kTestServiceInfo.id;
    info.id                             = kTestFunctionInfo.id + 3;
    info.pName                          = "TestFunction";
    info.pDescription                   = "A test function";
    info.pfnFuncCb = [](
        const DDRpcServerCallInfo* pCall) -> DD_RESULT
    {
        ByteWriterWrapper writer(*pCall->pWriter);

        const DD_RESULT result = writer.Write(&kTestReturnData, sizeof(kTestReturnData));
        writer.End(result);

        return result;
    };

    return info;
}();

const DDRpcServerRegisterFunctionInfo kTestParamNoReturnFunctionInfo = []() -> DDRpcServerRegisterFunctionInfo {
    DDRpcServerRegisterFunctionInfo info = {};
    info.serviceId                      = kTestServiceInfo.id;
    info.id                             = kTestFunctionInfo.id + 4;
    info.pName                          = "TestFunction";
    info.pDescription                   = "A test function";
    info.pfnFuncCb = [](
        const DDRpcServerCallInfo* pCall) -> DD_RESULT
    {
        ByteReader reader(pCall->pParameterData, pCall->parameterDataSize);

        uint64_t testValue = 0;
        DD_RESULT result = DevDriverToDDResult(reader.Read(&testValue));

        if (result == DD_RESULT_SUCCESS)
        {
            result = (testValue == kTestParamData) ? DD_RESULT_SUCCESS : DD_RESULT_PARSING_INVALID_BYTES;
        }

        return result;
    };

    return info;
}();

const DDRpcServerRegisterFunctionInfo kTestParamReturnFunctionInfo = []() -> DDRpcServerRegisterFunctionInfo {
    DDRpcServerRegisterFunctionInfo info = {};
    info.serviceId                      = kTestServiceInfo.id;
    info.id                             = kTestFunctionInfo.id + 5;
    info.pName                          = "TestFunction";
    info.pDescription                   = "A test function";
    info.pfnFuncCb = [](
        const DDRpcServerCallInfo* pCall) -> DD_RESULT
    {
        ByteReader reader(pCall->pParameterData, pCall->parameterDataSize);

        uint64_t testValue = 0;
        DD_RESULT result = DevDriverToDDResult(reader.Read(&testValue));

        if (result == DD_RESULT_SUCCESS)
        {
            result = (testValue == kTestParamData) ? DD_RESULT_SUCCESS : DD_RESULT_PARSING_INVALID_BYTES;
        }

        if (result == DD_RESULT_SUCCESS)
        {
            ByteWriterWrapper writer(*pCall->pWriter);

            result = writer.Write(&kTestReturnData, sizeof(kTestReturnData));
            writer.End(result);
        }

        return result;
    };

    return info;
}();

/// Client Tests ///////////////////////////////////////////////////////////////////////////////////

/// Check that `ddRpcClientCreate()` calls validate their inputs sensibly
TEST_F(DDNoNetworkTest, ClientCreate_InvalidArgs)
{
    // Missing both params
    ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddRpcClientCreate(nullptr, nullptr));

    // Missing CreateInfo
    {
        DDRpcClient rcpClient;
        ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddRpcClientCreate(nullptr, &rcpClient));
    }

    // Missing ClientId
    {
        DDRpcClientCreateInfo info = {};
        ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddRpcClientCreate(&info, nullptr));
    }

    // Both arguments, but not filled out correctly.
    {
        DDRpcClient           rpcClient;
        DDRpcClientCreateInfo info = {}; // left empty intentionally
        ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddRpcClientCreate(&info, &rpcClient));
    }
    {
        DDRpcClient           rpcClient;
        DDRpcClientCreateInfo info = {};
        info.protocolId            = kInvalidTestProtocolId;
        ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddRpcClientCreate(&info, &rpcClient));
    }
    {
        DDRpcClient           rpcClient;
        DDRpcClientCreateInfo info = {};
        info.hConnection           = reinterpret_cast<DDNetConnection>(1); // hahaha don't do this
        ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddRpcClientCreate(&info, &rpcClient));
    }

    // Don't crash on destroy nullptr
    ddRpcClientDestroy(nullptr);
}

TEST_F(DDNetworkedTest, ClientCreate_InvalidArgs)
{
    DDRpcClientCreateInfo info = {};
    info.protocolId            = kInvalidTestProtocolId;
    info.hConnection           = m_hClientConnection;

    // Case: Create with invalid client id
    {
        info.clientId = DD_API_INVALID_CLIENT_ID;
        DDRpcClient rpcClient;
        ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddRpcClientCreate(&info, &rpcClient));
    }

    // Case: Create with inactive client id
    {
        info.clientId    = 1;   // This is valid but very unlikely to be live
        info.timeoutInMs = 100; // Make sure we don't waste too much time attempting to connect
        DDRpcClient rpcClient;
        ASSERT_NE(DD_RESULT_COMMON_INVALID_PARAMETER, ddRpcClientCreate(&info, &rpcClient));
    }
}

/// Server Tests ///////////////////////////////////////////////////////////////////////////////////

/// Check that `Create()` calls validate their inputs sensibly
TEST_F(DDNoNetworkTest, ServerCreate_InvalidArgs)
{
    DDRpcServerCreateInfo info = {};
    info.hConnection = nullptr;

    DDRpcServer hServer = nullptr;

    // Missing both parameters
    ASSERT_EQ(ddRpcServerCreate(nullptr, nullptr), DD_RESULT_COMMON_INVALID_PARAMETER);

    // Missing server pointer
    ASSERT_EQ(ddRpcServerCreate(&info, nullptr), DD_RESULT_COMMON_INVALID_PARAMETER);

    // Missing info pointer
    ASSERT_EQ(ddRpcServerCreate(nullptr, &hServer), DD_RESULT_COMMON_INVALID_PARAMETER);

    // Bad message channel
    info.hConnection = nullptr;
    info.protocolId = kTestProtocolId;
    ASSERT_EQ(ddRpcServerCreate(&info, &hServer), DD_RESULT_COMMON_INVALID_PARAMETER);

    // Don't crash on destroy nullptr
    ddRpcServerDestroy(nullptr);
}

TEST_F(DDNetworkedTest, ServerRegisterService_InvalidArgs)
{
    DDRpcServerCreateInfo info = {};
    info.hConnection = m_hServerConnection;
    info.protocolId = kTestProtocolId;

    DDRpcServer hServer = nullptr;
    ASSERT_EQ(ddRpcServerCreate(&info, &hServer), DD_RESULT_SUCCESS);

    ASSERT_EQ(ddRpcServerRegisterService(hServer, nullptr), DD_RESULT_COMMON_INVALID_PARAMETER);

    DDRpcServerRegisterServiceInfo serviceInfo  = {};
    ASSERT_EQ(ddRpcServerRegisterService(hServer, &serviceInfo), DD_RESULT_COMMON_INVALID_PARAMETER);

    serviceInfo.id = kTestServiceInfo.id;
    ASSERT_EQ(ddRpcServerRegisterService(hServer, &serviceInfo), DD_RESULT_COMMON_INVALID_PARAMETER);

    serviceInfo.pName = kTestServiceInfo.pName;
    ASSERT_EQ(ddRpcServerRegisterService(hServer, &serviceInfo), DD_RESULT_COMMON_INVALID_PARAMETER);

    ddRpcServerDestroy(hServer);
}

TEST_F(DDNetworkedTest, ServerServiceRegistration)
{
    DDRpcServerCreateInfo info = {};
    info.hConnection = m_hServerConnection;
    info.protocolId = kTestProtocolId;

    DDRpcServer hServer = nullptr;
    ASSERT_EQ(ddRpcServerCreate(&info, &hServer), DD_RESULT_SUCCESS);

    // Make sure unregistering an unregistered service doesn't cause issues
    ddRpcServerUnregisterService(hServer, kTestServiceInfo.id);

    // Successfully register the service
    ASSERT_EQ(ddRpcServerRegisterService(hServer, &kTestServiceInfo), DD_RESULT_SUCCESS);

    // Make sure it doesn't allow duplicate registration
    ASSERT_EQ(ddRpcServerRegisterService(hServer, &kTestServiceInfo), DD_RESULT_COMMON_ALREADY_EXISTS);

    // Unregister it
    ddRpcServerUnregisterService(hServer, kTestServiceInfo.id);

    // Make sure it allows the id to be re-registered now
    ASSERT_EQ(ddRpcServerRegisterService(hServer, &kTestServiceInfo), DD_RESULT_SUCCESS);

    ddRpcServerDestroy(hServer);
}

TEST_F(DDNetworkedTest, ServerRegisterFunction_InvalidArgs)
{
    DDRpcServerCreateInfo info = {};
    info.hConnection = m_hServerConnection;
    info.protocolId = kTestProtocolId;

    DDRpcServer hServer = nullptr;
    ASSERT_EQ(ddRpcServerCreate(&info, &hServer), DD_RESULT_SUCCESS);

    ASSERT_EQ(ddRpcServerRegisterFunction(hServer, nullptr), DD_RESULT_COMMON_INVALID_PARAMETER);

    DDRpcServerRegisterFunctionInfo funcInfo = {};
    ASSERT_EQ(ddRpcServerRegisterFunction(hServer, &funcInfo), DD_RESULT_COMMON_INVALID_PARAMETER);

    funcInfo.serviceId = kTestServiceInfo.id;
    ASSERT_EQ(ddRpcServerRegisterFunction(hServer, &funcInfo), DD_RESULT_COMMON_INVALID_PARAMETER);

    funcInfo.id = kTestFunctionInfo.id;
    ASSERT_EQ(ddRpcServerRegisterFunction(hServer, &funcInfo), DD_RESULT_COMMON_INVALID_PARAMETER);

    funcInfo.pName = kTestFunctionInfo.pName;
    ASSERT_EQ(ddRpcServerRegisterFunction(hServer, &funcInfo), DD_RESULT_COMMON_INVALID_PARAMETER);

    funcInfo.pDescription = kTestFunctionInfo.pDescription;
    ASSERT_EQ(ddRpcServerRegisterFunction(hServer, &funcInfo), DD_RESULT_COMMON_INVALID_PARAMETER);

    funcInfo.pfnFuncCb = [](
        const DDRpcServerCallInfo* pCall) -> DD_RESULT
    {
        DD_API_UNUSED(pCall);

        return DD_RESULT_SUCCESS;
    };
    ASSERT_EQ(ddRpcServerRegisterFunction(hServer, &funcInfo), DD_RESULT_COMMON_DOES_NOT_EXIST);

    ddRpcServerDestroy(hServer);
}

TEST_F(DDNetworkedTest, ServerFunctionRegistration)
{
    DDRpcServerCreateInfo info = {};
    info.hConnection = m_hServerConnection;
    info.protocolId = kTestProtocolId;

    DDRpcServer hServer = nullptr;
    ASSERT_EQ(ddRpcServerCreate(&info, &hServer), DD_RESULT_SUCCESS);

    ASSERT_EQ(ddRpcServerRegisterService(hServer, &kTestServiceInfo), DD_RESULT_SUCCESS);

    // Make sure unregistering an unregistered function doesn't cause issues
    ddRpcServerUnregisterFunction(hServer, kTestFunctionInfo.serviceId, kTestFunctionInfo.id);

    // Register the function
    ASSERT_EQ(ddRpcServerRegisterFunction(hServer, &kTestFunctionInfo), DD_RESULT_SUCCESS);

    // Make sure the function can't be registered twice
    // TODO: This should be using a common error code instead of a DD one but our internal
    //       devdriver results don't translate like you'd expect.
    ASSERT_EQ(ddRpcServerRegisterFunction(hServer, &kTestFunctionInfo), DD_RESULT_DD_GENERIC_ENTRY_EXISTS);

    // Unregister the function
    ddRpcServerUnregisterFunction(hServer, kTestFunctionInfo.serviceId, kTestFunctionInfo.id);

    // Make sure the function slot can be successfully re-registered
    ASSERT_EQ(ddRpcServerRegisterFunction(hServer, &kTestFunctionInfo), DD_RESULT_SUCCESS);

    ddRpcServerDestroy(hServer);
}

/// Combined Tests ////////////////////////////////////////////////////////////////////////////

// Case: Connect with valid client id with a specified protocol id
TEST_F(DDNetworkedTest, CheckValidConnection_TestProtocolId)
{
    // Setup a server that does nothing
    DDRpcServerCreateInfo serverInfo = {};
    serverInfo.protocolId            = kTestProtocolId;
    serverInfo.hConnection           = m_hServerConnection;

    DDRpcServer hRpcServer = DD_API_INVALID_HANDLE;
    ASSERT_EQ(DD_RESULT_SUCCESS, ddRpcServerCreate(&serverInfo, &hRpcServer));

    // Setup a client and attempt to connect to our server
    DDRpcClientCreateInfo clientInfo = {};
    clientInfo.protocolId            = kTestProtocolId;
    clientInfo.hConnection           = m_hClientConnection;
    clientInfo.clientId              = ddRpcServerQueryClientId(hRpcServer);

    DDRpcClient hRpcClient = DD_API_INVALID_HANDLE;
    ASSERT_EQ(DD_RESULT_SUCCESS, ddRpcClientCreate(&clientInfo, &hRpcClient));

    ddRpcClientDestroy(hRpcClient);
    ddRpcServerDestroy(hRpcServer);
}

TEST_F(ddRpcTest, ClientCall_InvalidArgs)
{
    // Both invalid
    ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddRpcClientCall(nullptr, nullptr));

    // Invalid info
    ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddRpcClientCall(m_hClient, nullptr));

    // Invalid client
    {
        DDRpcClientCallInfo info = {};
        ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddRpcClientCall(nullptr, &info));
    }

    // Invalid service
    {
        DDRpcClientCallInfo info = {};
        info.service = 0;
        info.function = static_cast<DDRpcFunctionId>(kTestFunctionInfo.id);
        ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddRpcClientCall(m_hClient, &info));
    }

    // Invalid function
    {
        DDRpcClientCallInfo info = {};
        info.service = kTestServiceInfo.id;
        info.function = 0;
        ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddRpcClientCall(m_hClient, &info));
    }

    // Invalid parameter buffer
    {
        DDRpcClientCallInfo info = {};
        info.service = kTestServiceInfo.id;
        info.function = static_cast<DDRpcFunctionId>(kTestFunctionInfo.id);
        info.paramBufferSize = 1;
        ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddRpcClientCall(m_hClient, &info));
    }

    // Invalid byte writer
    {
        DDRpcClientCallInfo info = {};
        info.service = kTestServiceInfo.id;
        info.function = static_cast<DDRpcFunctionId>(kTestFunctionInfo.id);
        ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddRpcClientCall(m_hClient, &info));
    }
}

TEST_F(ddRpcTest, MultipleClients)
{
    DDRpcClientCreateInfo info = {};
    info.hConnection          = m_hClientConnection;
    info.protocolId           = kTestProtocolId;
    info.clientId             = ddRpcServerQueryClientId(m_hServer);

    constexpr uint32_t kNumClients = 16;
    DDRpcClient clients[kNumClients] = {};

    // Connect temporary dummy clients and then disconnect them
    // This helps to test handling for multiple clients on the server side
    for (uint32_t clientIdx = 0; clientIdx < kNumClients; ++clientIdx)
    {
        ASSERT_EQ(ddRpcClientCreate(&info, &clients[clientIdx]), DD_RESULT_SUCCESS);
    }

    for (uint32_t clientIdx = 0; clientIdx < kNumClients; ++clientIdx)
    {
        ddRpcClientDestroy(clients[clientIdx]);
    }
}

TEST_F(ddRpcServiceTest, CheckInvalidRpcCalls)
{
    // Dummy writer
    EmptyByteWriter<DD_RESULT_COMMON_UNSUPPORTED> writer;

    // Case: Call zero service and non-zero function
    {
        DDRpcClientCallInfo info = {};
        info.service             = 0;
        info.function            = 1; // Invalid, but not-reserved
        info.pResponseWriter     = writer.Writer();
        ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddRpcClientCall(m_hClient, &info));
    }

    // Case: Call a valid service and zero function
    {
        DDRpcClientCallInfo info = {};
        info.service             = 1; // Invalid, but not-reserved
        info.function            = 0;
        info.pResponseWriter     = writer.Writer();
        ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddRpcClientCall(m_hClient, &info));
    }

    // Case: Call a valid service and function, but invalid version
    {
        DDRpcClientCallInfo info = {};
        info.service             = kInvalidService;
        info.function            = kInvalidFunction;
        info.pResponseWriter     = writer.Writer();
        ASSERT_EQ(DD_RESULT_COMMON_INVALID_PARAMETER, ddRpcClientCall(m_hClient, &info));
    }

    // Case: Call invalid function on invalid service
    {
        DDRpcClientCallInfo info  = {};
        info.service              = kInvalidService;
        info.serviceVersion.major = 1;
        info.function             = kInvalidFunction;
        info.pResponseWriter      = writer.Writer();
        ASSERT_EQ(DD_RESULT_DD_RPC_SERVICE_NOT_REGISTERED, ddRpcClientCall(m_hClient, &info));
    }

    // Case: Call invalid function on valid service
    {
        DDRpcClientCallInfo info  = {};
        info.service              = kTestServiceInfo.id;
        info.serviceVersion       = kTestServiceInfo.version;
        info.function             = kInvalidFunction;
        info.pResponseWriter      = writer.Writer();
        ASSERT_EQ(DD_RESULT_DD_RPC_FUNC_NOT_REGISTERED, ddRpcClientCall(m_hClient, &info));
    }

    // Case: Call valid function on invalid service
    {
        DDRpcClientCallInfo info  = {};
        info.service              = kInvalidService;
        info.serviceVersion       = kTestServiceInfo.version;
        info.function             = static_cast<DDRpcFunctionId>(kTestFunctionInfo.id);
        info.pResponseWriter      = writer.Writer();
        ASSERT_EQ(DD_RESULT_DD_RPC_SERVICE_NOT_REGISTERED, ddRpcClientCall(m_hClient, &info));
    }
}

TEST_F(ddRpcServiceTest, VersionMismatchClient)
{
    EmptyByteWriter<DD_RESULT_COMMON_UNSUPPORTED> writer;

    DDApiVersion testVersion = {};
    testVersion.major = 2;

    DDRpcClientCallInfo info = {};
    info.service             = kTestServiceInfo.id;
    info.serviceVersion      = testVersion;
    info.function            = kTestFunctionInfo.id + 1;
    info.pResponseWriter     = writer.Writer();
    ASSERT_EQ(DD_RESULT_COMMON_VERSION_MISMATCH, ddRpcClientCall(m_hClient, &info));
}

TEST_F(ddRpcServiceTest, VersionMismatchServer)
{
    EmptyByteWriter<DD_RESULT_COMMON_UNSUPPORTED> writer;

    DDRpcClientCallInfo info = {};
    info.service             = kTestServiceInfo.id;
    info.serviceVersion      = kTestServiceInfo.version;
    info.function            = kTestFunctionInfo.id + 1;
    info.pResponseWriter     = writer.Writer();
    ASSERT_EQ(DD_RESULT_SUCCESS, ddRpcClientCall(m_hClient, &info));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Check Rpc Calls with a valid response writer
// Some of these expect no data and assert that none comes in

TEST_F(ddRpcServiceTest, NoParamNoReturn)
{
    EmptyByteWriter<DD_RESULT_COMMON_UNSUPPORTED> writer;

    DDRpcClientCallInfo info = {};
    info.service             = kTestServiceInfo.id;
    info.serviceVersion      = kTestServiceInfo.version;
    info.function            = kTestFunctionInfo.id + 2;
    info.pResponseWriter     = writer.Writer();
    ASSERT_EQ(DD_RESULT_SUCCESS, ddRpcClientCall(m_hClient, &info));
}

TEST_F(ddRpcServiceTest, NoParamReturn)
{
    DynamicBufferByteWriter writer;

    DDRpcClientCallInfo info = {};
    info.service             = kTestServiceInfo.id;
    info.serviceVersion      = kTestServiceInfo.version;
    info.function            = kTestFunctionInfo.id + 3;
    info.pResponseWriter     = writer.Writer();
    ASSERT_EQ(DD_RESULT_SUCCESS, ddRpcClientCall(m_hClient, &info));

    ASSERT_EQ(writer.Size(), sizeof(kTestReturnData));
    ASSERT_EQ(memcmp(writer.Buffer(), &kTestReturnData, writer.Size()), 0);
}

TEST_F(ddRpcServiceTest, ParamNoReturn)
{
    EmptyByteWriter<DD_RESULT_COMMON_UNSUPPORTED> writer;

    DDRpcClientCallInfo info = {};
    info.service             = kTestServiceInfo.id;
    info.serviceVersion      = kTestServiceInfo.version;
    info.function            = kTestFunctionInfo.id + 4;
    info.paramBufferSize     = sizeof(kTestParamData);
    info.pParamBuffer        = &kTestParamData;
    info.pResponseWriter     = writer.Writer();
    ASSERT_EQ(DD_RESULT_SUCCESS, ddRpcClientCall(m_hClient, &info));
}

TEST_F(ddRpcServiceTest, ParamReturn)
{
    DynamicBufferByteWriter writer;

    DDRpcClientCallInfo info = {};
    info.service             = kTestServiceInfo.id;
    info.serviceVersion      = kTestServiceInfo.version;
    info.function            = kTestFunctionInfo.id + 5;
    info.paramBufferSize     = sizeof(kTestParamData);
    info.pParamBuffer        = &kTestParamData;
    info.pResponseWriter     = writer.Writer();
    ASSERT_EQ(DD_RESULT_SUCCESS, ddRpcClientCall(m_hClient, &info));

    ASSERT_EQ(writer.Size(), sizeof(kTestReturnData));
    ASSERT_EQ(memcmp(writer.Buffer(), &kTestReturnData, writer.Size()), 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Check Rpc Calls with NO response writer

TEST_F(ddRpcServiceTest, NoParamNoReturnNoWriter)
{
    DDRpcClientCallInfo info = {};
    info.service             = kTestServiceInfo.id;
    info.serviceVersion      = kTestServiceInfo.version;
    info.function            = kTestFunctionInfo.id + 2;
    ASSERT_EQ(DD_RESULT_SUCCESS, ddRpcClientCall(m_hClient, &info));
}

TEST_F(ddRpcServiceTest, NoParamReturnNoWriter)
{
    DDRpcClientCallInfo info = {};
    info.service             = kTestServiceInfo.id;
    info.serviceVersion      = kTestServiceInfo.version;
    info.function            = kTestFunctionInfo.id + 3;
    ASSERT_EQ(DD_RESULT_DD_RPC_FUNC_UNEXPECTED_RETURN_DATA, ddRpcClientCall(m_hClient, &info));
}

TEST_F(ddRpcServiceTest, ParamNoReturnNoWriter)
{
    DDRpcClientCallInfo info = {};
    info.service             = kTestServiceInfo.id;
    info.serviceVersion      = kTestServiceInfo.version;
    info.function            = kTestFunctionInfo.id + 4;
    info.paramBufferSize     = sizeof(kTestParamData);
    info.pParamBuffer        = &kTestParamData;
    ASSERT_EQ(DD_RESULT_SUCCESS, ddRpcClientCall(m_hClient, &info));
}

TEST_F(ddRpcServiceTest, ParamReturnNoWriter)
{
    DDRpcClientCallInfo info = {};
    info.service             = kTestServiceInfo.id;
    info.serviceVersion      = kTestServiceInfo.version;
    info.function            = kTestFunctionInfo.id + 5;
    info.paramBufferSize     = sizeof(kTestParamData);
    info.pParamBuffer        = &kTestParamData;
    ASSERT_EQ(DD_RESULT_DD_RPC_FUNC_UNEXPECTED_RETURN_DATA, ddRpcClientCall(m_hClient, &info));
}

/// GTest entry-point
GTEST_API_ int main(int argc, char** argv)
{
    // Run all tests
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

void ddRpcTest::SetUp()
{
    DDNetworkedTest::SetUp();

    // Setup a server that does nothing
    DDRpcServerCreateInfo serverInfo = {};
    serverInfo.protocolId            = kTestProtocolId;
    serverInfo.hConnection           = m_hServerConnection;

    ASSERT_EQ(DD_RESULT_SUCCESS, ddRpcServerCreate(&serverInfo, &m_hServer));

    // Setup a client and attempt to connect to our server
    DDRpcClientCreateInfo clientInfo = {};
    clientInfo.protocolId            = kTestProtocolId;
    clientInfo.hConnection           = m_hClientConnection;
    clientInfo.clientId              = ddRpcServerQueryClientId(m_hServer);

    ASSERT_EQ(DD_RESULT_SUCCESS, ddRpcClientCreate(&clientInfo, &m_hClient));
}

void ddRpcTest::TearDown()
{
    ddRpcClientDestroy(m_hClient);
    m_hClient = DD_API_INVALID_HANDLE;

    ddRpcServerDestroy(m_hServer);
    m_hServer = DD_API_INVALID_HANDLE;

    DDNetworkedTest::TearDown();
}

void ddRpcServiceTest::SetUp()
{
    // Setup the network first
    ddRpcTest::SetUp();

    // Register Service & Functions
    ASSERT_EQ(ddRpcServerRegisterService(m_hServer, &kTestServiceInfo), DD_RESULT_SUCCESS);
    ASSERT_EQ(ddRpcServerRegisterFunction(m_hServer, &kTestFunctionInfo), DD_RESULT_SUCCESS);

    ASSERT_EQ(ddRpcServerRegisterFunction(m_hServer, &kTestVersionFunctionInfo), DD_RESULT_SUCCESS);
    ASSERT_EQ(ddRpcServerRegisterFunction(m_hServer, &kTestNoParamNoReturnFunctionInfo), DD_RESULT_SUCCESS);
    ASSERT_EQ(ddRpcServerRegisterFunction(m_hServer, &kTestNoParamReturnFunctionInfo), DD_RESULT_SUCCESS);
    ASSERT_EQ(ddRpcServerRegisterFunction(m_hServer, &kTestParamNoReturnFunctionInfo), DD_RESULT_SUCCESS);
    ASSERT_EQ(ddRpcServerRegisterFunction(m_hServer, &kTestParamReturnFunctionInfo), DD_RESULT_SUCCESS);
}

void ddRpcServiceTest::TearDown()
{
    // Tear down the network last
    ddRpcTest::TearDown();
}
