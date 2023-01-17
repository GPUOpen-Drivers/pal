/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <ddTestUtil.h>

#include <ddRouter.h>
#include <ddNet.h>
#include <ddRpcServerApi.h>
#include <ddRpcServer.h>
#include <ddRpcClient.h>

using namespace DevDriver;

////////////////////////////////////////////////////////////////////////////////
uint16_t MakeLocalPort()
{
    return static_cast<uint16_t>(Platform::GetProcessId());
}

////////////////////////////////////////////////////////////////////////////////
uint16_t MakeRemotePort()
{
    return DD_API_DEFAULT_NETWORK_PORT + (Platform::GetProcessId() % 4096);
}

////////////////////////////////////////////////////////////////////////////////
DD_RESULT DDTestRouter::Init(const char* pTestName)
{
    const uint16_t localPort = MakeLocalPort();

    DDRouterCreateInfo info = {};

    // Provide a unique description
    Vector<char> description(Platform::GenericAllocCb);
    {
        const char* kFormatString = "Test Router [%u] %s";
        int size = Platform::Snprintf(
            nullptr,
            0,
            kFormatString,
            localPort,
            pTestName);

        description.Resize(size);
        Platform::Snprintf(
            description.Data(),
            description.Size(),
            kFormatString,
            localPort,
            pTestName);

        info.pDescription = description.Data();
    }

    // Disable kernel client
    info.transportFlags.fields.disableKernelTransport = 1;

    // Disable external network access to avoid unnecessary complexity during tests
    info.transportFlags.fields.disableExternalNetwork = 1;

    // If connections are inconsistent, increase the tieout count
    info.clientTimeoutCount = kDefaultClientTimeoutCount;

    // Generate a unique network port for the current test
    info.remotePort = MakeRemotePort();

    // Save the network port for later use by test fixtures
    m_remotePort = info.remotePort;

    // Run on a private connection to avoid collisions when tests run in parallel
    if (localPort != 0)
    {
        info.localPort = localPort;

        // Save the local connection id value for later use by test fixtures
        m_localPort = localPort;

        DD_PRINT(LogLevel::Info, "Router is using local connection id: \"%u\"", info.localPort);
    }
    else
    {
        DD_PRINT(LogLevel::Info, "Router is using default local connection id");
    }

    return ddRouterCreate(&info, &m_hRouter);
}

////////////////////////////////////////////////////////////////////////////////
DDTestRouter::~DDTestRouter()
{
    // Destroy the router
    ddRouterDestroy(m_hRouter);
}

////////////////////////////////////////////////////////////////////////////////
DDNetConnectionInfo DDTestRouter::GenerateLocalInfo()
{
    DDNetConnectionInfo info = {};

    info.pDescription = "Local Test Client";
    info.type = DD_NET_CLIENT_TYPE_TOOL;
    info.port = m_localPort;

    return info;
}

////////////////////////////////////////////////////////////////////////////////
DDNetConnectionInfo DDTestRouter::GenerateRemoteInfo()
{
    DDNetConnectionInfo info = {};

    info.pDescription = "Remote Test Client";
    info.type = DD_NET_CLIENT_TYPE_TOOL;
    info.pHostname = "localhost";
    info.port = m_remotePort;

    return info;
}

////////////////////////////////////////////////////////////////////////////////
void DDNetworkedTest::SetUp()
{
    ASSERT_EQ(m_router.Init(::testing::UnitTest::GetInstance()->current_test_info()->name()), DD_RESULT_SUCCESS);

    const uint16_t localPort = m_router.GetLocalPort();

    DDNetConnectionInfo connectionInfo = {};
    connectionInfo.type = DD_NET_CLIENT_TYPE_TOOL;
    connectionInfo.pDescription = "Test Client Connection";
    connectionInfo.port = localPort;

    // Create a normal local connection to the router as the client side
    ASSERT_EQ(ddNetCreateConnection(&connectionInfo, &m_hClientConnection), DD_RESULT_SUCCESS);

    m_clientClientId = ddNetQueryClientId(m_hClientConnection);

    // Create another local connection to the router as the server side
    connectionInfo.type = DD_NET_CLIENT_TYPE_SERVER;
    connectionInfo.pDescription = "Test Server Connection";

    ASSERT_EQ(ddNetCreateConnection(&connectionInfo, &m_hServerConnection), DD_RESULT_SUCCESS);

    m_serverClientId = ddNetQueryClientId(m_hServerConnection);
}

////////////////////////////////////////////////////////////////////////////////
void DDNetworkedTest::TearDown()
{
    ddNetDestroyConnection(m_hServerConnection);
    ddNetDestroyConnection(m_hClientConnection);

    m_hClientConnection = DD_API_INVALID_HANDLE;
    m_clientClientId    = DD_API_INVALID_CLIENT_ID;

    m_hServerConnection = DD_API_INVALID_HANDLE;
    m_serverClientId    = DD_API_INVALID_CLIENT_ID;
}

////////////////////////////////////////////////////////////////////////////////
void ClientServerTest::HandleMessageChannelEvent(BusEventType type, const void* pEventData, size_t eventDataSize)
{
    DD_UNUSED(pEventData);
    DD_UNUSED(eventDataSize);
    if (type == BusEventType::ClientHalted)
    {
        m_haltedEvent.Signal();
    }
    else if (type == BusEventType::PongRequest)
    {
        // Do nothing
    }
    else
    {
        DD_ASSERT_REASON("Invalid Bus Event Type!");
    }
}

////////////////////////////////////////////////////////////////////////////////
void ClientServerTest::SetUp()
{
    DDNetworkedTest::SetUp();

    HostInfo testHostInfo = kDefaultNamedPipe;
    testHostInfo.port = m_router.GetLocalPort();

    ClientCreateInfo clientCreateInfo = {};
    clientCreateInfo.connectionInfo = testHostInfo;
    Platform::Strncpy(clientCreateInfo.clientDescription, "Test Tool");

    clientCreateInfo.componentType = Component::Tool;

    clientCreateInfo.createUpdateThread = true;
    clientCreateInfo.initialFlags = static_cast<StatusFlags>(ClientStatusFlags::DeveloperModeEnabled);

    m_pClient = new DevDriverClient(Platform::GenericAllocCb, clientCreateInfo);
    ASSERT_NE(m_pClient, nullptr) << "Failed to allocate memory for client";

    Result initResult = m_pClient->Initialize();
    ASSERT_EQ(initResult, Result::Success) << "Failed to initialize client";

    // Pass a lambda function as the event callback that simply passes through to this->HandleMessageChannelEvent()
    BusEventCallback busEventCb = {};
    busEventCb.pfnEventCallback =
        [](void* pUserdata, BusEventType type, const void* pEventData, size_t eventDataSize) {
            ClientServerTest* pTest = reinterpret_cast<ClientServerTest*>(pUserdata);
            pTest->HandleMessageChannelEvent(type, pEventData, eventDataSize);
        };
    busEventCb.pUserdata = this;

    m_pClient->GetMessageChannel()->SetBusEventCallback(busEventCb);

    ServerCreateInfo serverCreateInfo = {};
    serverCreateInfo.connectionInfo = testHostInfo;
    strncpy(serverCreateInfo.clientDescription, "Test UMD", sizeof(serverCreateInfo.clientDescription));
    serverCreateInfo.componentType = Component::Driver;
    serverCreateInfo.createUpdateThread = true;

    serverCreateInfo.servers.logging = true;
    serverCreateInfo.servers.settings = true;
    serverCreateInfo.servers.driverControl = true;
    serverCreateInfo.servers.rgp = true;
    serverCreateInfo.servers.event = true;

    m_pServer = new DevDriverServer(Platform::GenericAllocCb, serverCreateInfo);
    ASSERT_NE(m_pServer, nullptr) << "Failed to allocate memory for server";

    initResult = m_pServer->Initialize();
    ASSERT_EQ(initResult, Result::Success) << "Failed to initialize server";
}

////////////////////////////////////////////////////////////////////////////////
void ClientServerTest::TearDown()
{
    if (m_pServer != nullptr)
    {
        m_pServer->Destroy();
        delete m_pServer;
        m_pServer = nullptr;
    }

    if (m_pClient != nullptr)
    {
        m_pClient->Destroy();
        delete m_pClient;
        m_pClient = nullptr;
    }

    DDNetworkedTest::TearDown();
}

////////////////////////////////////////////////////////////////////////////////
void RpcClientServerTest::SetUp()
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

////////////////////////////////////////////////////////////////////////////////
void RpcClientServerTest::TearDown()
{
    ddRpcClientDestroy(m_hClient);
    m_hClient = DD_API_INVALID_HANDLE;

    ddRpcServerDestroy(m_hServer);
    m_hServer = DD_API_INVALID_HANDLE;

    DDNetworkedTest::TearDown();
}
