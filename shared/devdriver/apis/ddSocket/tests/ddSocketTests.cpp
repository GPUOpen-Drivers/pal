
#include <ddSocketTests.h>
#include <ddCommon.h>
#include <ddNet.h>

#include <string>

using namespace DevDriver;

/// Unit Tests /////////////////////////////////////////////////////////////////////////////////////

/// Arbitrary protocol id value used for testing
const DDProtocolId kTestProtocolId = 64;

/// Used to specify the max number of connections that can be pending on an accept operation at once
const uint32_t kTestProtocolMaxPendingConnections = 8;

/// Used to specify a reasonable default timeout value for send/receive/accept operations
const uint32_t kTestTimeoutInMs = 1000;

/// Check that `Create()` calls validate their inputs sensibly
TEST_F(DDNoNetworkTest, ConnectArgumentValidation)
{
    // Missing parameters
    DD_RESULT result = ddSocketConnect(nullptr, nullptr);
    ASSERT_EQ(result, DD_RESULT_COMMON_INVALID_PARAMETER);

    // Missing output socket
    DDSocketConnectInfo connectInfo = {};
    result = ddSocketConnect(&connectInfo, nullptr);
    ASSERT_EQ(result, DD_RESULT_COMMON_INVALID_PARAMETER);

    // Missing connect info
    DDSocket hSocket = DD_API_INVALID_HANDLE;
    result = ddSocketConnect(nullptr, &hSocket);
    ASSERT_EQ(result, DD_RESULT_COMMON_INVALID_PARAMETER);

    // Empty connect info
    result = ddSocketConnect(&connectInfo, &hSocket);
    ASSERT_EQ(result, DD_RESULT_COMMON_INVALID_PARAMETER);

    // Invalid client id
    connectInfo.hConnection = reinterpret_cast<DDNetConnection>(static_cast<size_t>(0xdeadbeef));
    connectInfo.clientId = DD_API_INVALID_CLIENT_ID;
    connectInfo.protocolId = kTestProtocolId;
    result = ddSocketConnect(&connectInfo, &hSocket);
    ASSERT_EQ(result, DD_RESULT_COMMON_INVALID_PARAMETER);

    // Invalid protocol id
    connectInfo.clientId = 0xdead;
    connectInfo.protocolId = 0;
    result = ddSocketConnect(&connectInfo, &hSocket);
    ASSERT_EQ(result, DD_RESULT_COMMON_INVALID_PARAMETER);
}

// TODO: Listen argument validation

// TODO: Accept argument validation

// TODO: Send argument validation

// TODO: Receive argument validation

// TODO: Close argument validation

TEST_F(DDNetworkedTest, UnsuccessfulConnection)
{
    DDSocket hSocket = DD_API_INVALID_HANDLE;

    DDSocketConnectInfo connectInfo = {};
    connectInfo.hConnection = m_hClientConnection;
    connectInfo.clientId = ddNetQueryClientId(m_hServerConnection);
    connectInfo.protocolId = kTestProtocolId;
    connectInfo.timeoutInMs = 100; // Use a small delay here since we expect this to time out
    const DD_RESULT result = ddSocketConnect(&connectInfo, &hSocket);
    ASSERT_EQ(result, DD_RESULT_DD_GENERIC_NOT_READY);
}

TEST_F(DDNetworkedTest, BasicConnection)
{
    DDSocket hListenSocket = DD_API_INVALID_HANDLE;
    DDSocketListenInfo listenInfo = {};
    listenInfo.hConnection = m_hServerConnection;
    listenInfo.protocolId = kTestProtocolId;
    listenInfo.maxPending = kTestProtocolMaxPendingConnections;
    DD_RESULT result = ddSocketListen(&listenInfo, &hListenSocket);
    ASSERT_EQ(result, DD_RESULT_SUCCESS);

    DDSocket hClientSocket = DD_API_INVALID_HANDLE;
    DDSocketConnectInfo connectInfo = {};
    connectInfo.hConnection = m_hClientConnection;
    connectInfo.clientId = ddNetQueryClientId(m_hServerConnection);
    connectInfo.protocolId = kTestProtocolId;
    result = ddSocketConnect(&connectInfo, &hClientSocket);
    ASSERT_EQ(result, DD_RESULT_SUCCESS);

    // Verify that send/receive doesn't work on listen sockets
    ASSERT_EQ(ddSocketSendRaw(hListenSocket, nullptr, 0, 0, nullptr), DD_RESULT_NET_SOCKET_TYPE_UNSUPPORTED);
    ASSERT_EQ(ddSocketReceiveRaw(hListenSocket, nullptr, 0, 0, nullptr), DD_RESULT_NET_SOCKET_TYPE_UNSUPPORTED);

    // Verify that version queries don't work on listening sockets
    ASSERT_EQ(ddSocketQueryProtocolVersion(hListenSocket), 0u);

    // Verify that the version was negotiated correctly
    ASSERT_EQ(ddSocketQueryProtocolVersion(hClientSocket), 0u);

    ddSocketClose(hClientSocket);
    ddSocketClose(hListenSocket);
}

TEST_F(DDNetworkedTest, AcceptClient)
{
    DDSocket hListenSocket = DD_API_INVALID_HANDLE;
    DDSocketListenInfo listenInfo = {};
    listenInfo.hConnection = m_hServerConnection;
    listenInfo.protocolId = kTestProtocolId;
    listenInfo.maxPending = kTestProtocolMaxPendingConnections;
    DD_RESULT result = ddSocketListen(&listenInfo, &hListenSocket);
    ASSERT_EQ(result, DD_RESULT_SUCCESS);

    DDSocket hClientSocket = DD_API_INVALID_HANDLE;
    DDSocketConnectInfo connectInfo = {};
    connectInfo.hConnection = m_hClientConnection;
    connectInfo.clientId = ddNetQueryClientId(m_hServerConnection);
    connectInfo.protocolId = kTestProtocolId;
    result = ddSocketConnect(&connectInfo, &hClientSocket);
    ASSERT_EQ(result, DD_RESULT_SUCCESS);

    DDSocket hServerSocket = DD_API_INVALID_HANDLE;
    result = ddSocketAccept(hListenSocket, kTestTimeoutInMs, &hServerSocket);
    ASSERT_EQ(result, DD_RESULT_SUCCESS);

    size_t bytesSent = 0;
    result = ddSocketSendRaw(hClientSocket, nullptr, 0, kTestTimeoutInMs, &bytesSent);
    ASSERT_EQ(result, DD_RESULT_SUCCESS);
    ASSERT_EQ(bytesSent, static_cast<size_t>(0));

    size_t bytesReceived = 0;
    result = ddSocketReceiveRaw(hServerSocket, nullptr, 0, kTestTimeoutInMs, &bytesReceived);
    ASSERT_EQ(result, DD_RESULT_SUCCESS);
    ASSERT_EQ(bytesReceived, static_cast<size_t>(0));

    ddSocketClose(hServerSocket);
    ddSocketClose(hClientSocket);
    ddSocketClose(hListenSocket);
}

TEST_F(DDNetworkedTest, SimpleTransfer)
{
    DDSocket hListenSocket = DD_API_INVALID_HANDLE;
    DDSocketListenInfo listenInfo = {};
    listenInfo.hConnection = m_hServerConnection;
    listenInfo.protocolId = kTestProtocolId;
    listenInfo.maxPending = kTestProtocolMaxPendingConnections;
    DD_RESULT result = ddSocketListen(&listenInfo, &hListenSocket);
    ASSERT_EQ(result, DD_RESULT_SUCCESS);

    DDSocket hClientSocket = DD_API_INVALID_HANDLE;
    DDSocketConnectInfo connectInfo = {};
    connectInfo.hConnection = m_hClientConnection;
    connectInfo.clientId = ddNetQueryClientId(m_hServerConnection);
    connectInfo.protocolId = kTestProtocolId;
    result = ddSocketConnect(&connectInfo, &hClientSocket);
    ASSERT_EQ(result, DD_RESULT_SUCCESS);

    DDSocket hServerSocket = DD_API_INVALID_HANDLE;
    result = ddSocketAccept(hListenSocket, kTestTimeoutInMs, &hServerSocket);
    ASSERT_EQ(result, DD_RESULT_SUCCESS);

    // Send some test data over the network
    constexpr uint8_t kTestData[] =
    {
        0, 1, 2, 3, 4, 5, 6, 7
    };

    ASSERT_EQ(ddSocketSend(hClientSocket, kTestData, sizeof(kTestData)), DD_RESULT_SUCCESS);

    // Receive the test data into a new array
    uint8_t testBuffer[sizeof(kTestData)] = {};
    ASSERT_EQ(ddSocketReceive(hServerSocket, testBuffer, sizeof(testBuffer)), DD_RESULT_SUCCESS);

    // Compare the data
    ASSERT_EQ(memcmp(kTestData, testBuffer, sizeof(kTestData)), 0);

    ddSocketClose(hServerSocket);
    ddSocketClose(hClientSocket);
    ddSocketClose(hListenSocket);
}

TEST_F(DDNetworkedTest, SimpleSizePrefixedTransfer)
{
    DDSocket hListenSocket = DD_API_INVALID_HANDLE;
    DDSocketListenInfo listenInfo = {};
    listenInfo.hConnection = m_hServerConnection;
    listenInfo.protocolId = kTestProtocolId;
    listenInfo.maxPending = kTestProtocolMaxPendingConnections;
    DD_RESULT result = ddSocketListen(&listenInfo, &hListenSocket);
    ASSERT_EQ(result, DD_RESULT_SUCCESS);

    DDSocket hClientSocket = DD_API_INVALID_HANDLE;
    DDSocketConnectInfo connectInfo = {};
    connectInfo.hConnection = m_hClientConnection;
    connectInfo.clientId = ddNetQueryClientId(m_hServerConnection);
    connectInfo.protocolId = kTestProtocolId;
    result = ddSocketConnect(&connectInfo, &hClientSocket);
    ASSERT_EQ(result, DD_RESULT_SUCCESS);

    DDSocket hServerSocket = DD_API_INVALID_HANDLE;
    result = ddSocketAccept(hListenSocket, kTestTimeoutInMs, &hServerSocket);
    ASSERT_EQ(result, DD_RESULT_SUCCESS);

    // Send some test data over the network
    constexpr uint8_t kTestData[] =
    {
        0, 1, 2, 3, 4, 5, 6, 7
    };

    ASSERT_EQ(ddSocketSendWithSizePrefix(hClientSocket, kTestData, sizeof(kTestData)), DD_RESULT_SUCCESS);

    // Receive the test data into a new array
    uint8_t testBuffer[sizeof(kTestData)] = {};
    uint64_t sizePrefix = 0;
    ASSERT_EQ(ddSocketReceiveWithSizePrefix(hServerSocket, testBuffer, sizeof(testBuffer), nullptr), DD_RESULT_COMMON_INVALID_PARAMETER);
    ASSERT_EQ(ddSocketReceiveWithSizePrefix(hServerSocket, testBuffer, sizeof(testBuffer), &sizePrefix), DD_RESULT_SUCCESS);
    ASSERT_EQ(sizePrefix, static_cast<uint64_t>(sizeof(testBuffer)));

    // Compare the data
    ASSERT_EQ(memcmp(kTestData, testBuffer, sizeof(kTestData)), 0);

    ddSocketClose(hServerSocket);
    ddSocketClose(hClientSocket);
    ddSocketClose(hListenSocket);
}

class SingleThreadedTransferHelper
{
public:
    SingleThreadedTransferHelper(DDSocket hClientSocket, DDSocket hServerSocket)
        : m_hClientSocket(hClientSocket)
        , m_hServerSocket(hServerSocket)
        , m_pSendData(nullptr)
        , m_sendDataSize(0)
        , m_totalBytesSent(0)
        , m_receiveBuffer(Platform::GenericAllocCb)
        , m_totalBytesReceived(0)
    {
    }

    DD_RESULT Transfer(const void* pData, size_t dataSize)
    {
        m_totalBytesSent = 0;
        m_totalBytesReceived = 0;
        m_pSendData = pData;
        m_sendDataSize = dataSize;
        m_receiveBuffer.Resize(dataSize);

        DD_RESULT result = DD_RESULT_SUCCESS;

        while ((result == DD_RESULT_SUCCESS) && (m_totalBytesSent < m_sendDataSize))
        {
            result = Send();
            if (result == DD_RESULT_DD_GENERIC_NOT_READY)
            {
                result = Receive();

                if (result == DD_RESULT_DD_GENERIC_NOT_READY)
                {
                    result = DD_RESULT_SUCCESS;
                }
            }
        }

        while ((result == DD_RESULT_SUCCESS) && (m_totalBytesReceived < m_totalBytesSent))
        {
            result = Receive();

            if (result == DD_RESULT_DD_GENERIC_NOT_READY)
            {
                result = DD_RESULT_SUCCESS;
            }
        }

        if (result == DD_RESULT_SUCCESS)
        {
            result = ValidateTransfer();
        }

        return result;
    }

    bool IsTransferComplete() const
    {
        return (m_totalBytesReceived == m_sendDataSize);
    }

private:
    DD_RESULT Send()
    {
        DD_RESULT result = DD_RESULT_SUCCESS;

        // Send the test data
        while ((result == DD_RESULT_SUCCESS) && (m_totalBytesSent < m_sendDataSize))
        {
            const void* pData = VoidPtrInc(m_pSendData, m_totalBytesSent);
            const size_t bytesToSend = m_sendDataSize - m_totalBytesSent;

            size_t bytesSent = 0;
            result = ddSocketSendRaw(m_hClientSocket, pData, bytesToSend, kTestTimeoutInMs, &bytesSent);

            if (result == DD_RESULT_SUCCESS)
            {
                m_totalBytesSent += bytesSent;
            }
        }

        return result;
    }

    DD_RESULT Receive()
    {
        DD_RESULT result = DD_RESULT_SUCCESS;

        // Receive the test data
        while ((result == DD_RESULT_SUCCESS)                 &&
            (m_totalBytesReceived < m_totalBytesSent)        &&
            (m_totalBytesReceived < m_receiveBuffer.Size()))
        {
            // We should always have more data sent than received in this function
            DD_ASSERT(m_totalBytesSent > m_totalBytesReceived);

            void* pData = VoidPtrInc(m_receiveBuffer.Data(), m_totalBytesReceived);
            const size_t bytesToReceive = m_totalBytesSent - m_totalBytesReceived;

            size_t bytesReceived = 0;
            result = ddSocketReceiveRaw(m_hServerSocket, pData, bytesToReceive, kTestTimeoutInMs, &bytesReceived);

            if (result == DD_RESULT_SUCCESS)
            {
                m_totalBytesReceived += bytesReceived;
            }
        }

        return result;
    }

    DD_RESULT ValidateTransfer() const
    {
        DD_RESULT result = IsTransferComplete() ? DD_RESULT_SUCCESS : DD_RESULT_DD_GENERIC_NOT_READY;

        if (result == DD_RESULT_SUCCESS)
        {
            result = (memcmp(m_pSendData, m_receiveBuffer.Data(), m_sendDataSize) == 0) ? DD_RESULT_SUCCESS : DD_RESULT_PARSING_INVALID_BYTES;
        }

        return result;
    }

    DDSocket m_hClientSocket;
    DDSocket m_hServerSocket;

    const void* m_pSendData;
    size_t m_sendDataSize;
    size_t m_totalBytesSent;

    Vector<uint8_t> m_receiveBuffer;
    size_t m_totalBytesReceived;
};

TEST_P(ddSocketSingleThreadedTransferTest, SingleThreadedTransfer)
{
    DDSocket hListenSocket = DD_API_INVALID_HANDLE;
    DDSocketListenInfo listenInfo = {};
    listenInfo.hConnection = m_hServerConnection;
    listenInfo.protocolId = kTestProtocolId;
    listenInfo.maxPending = kTestProtocolMaxPendingConnections;
    DD_RESULT result = ddSocketListen(&listenInfo, &hListenSocket);
    ASSERT_EQ(result, DD_RESULT_SUCCESS);

    DDSocket hClientSocket = DD_API_INVALID_HANDLE;
    DDSocketConnectInfo connectInfo = {};
    connectInfo.hConnection = m_hClientConnection;
    connectInfo.clientId = ddNetQueryClientId(m_hServerConnection);
    connectInfo.protocolId = kTestProtocolId;
    result = ddSocketConnect(&connectInfo, &hClientSocket);
    ASSERT_EQ(result, DD_RESULT_SUCCESS);

    DDSocket hServerSocket = DD_API_INVALID_HANDLE;
    result = ddSocketAccept(hListenSocket, kTestTimeoutInMs, &hServerSocket);
    ASSERT_EQ(result, DD_RESULT_SUCCESS);

    // Initialize the test data
    DevDriver::Vector<uint8_t> testData(Platform::GenericAllocCb);
    testData.Resize(GetParam());
    for (size_t byteIndex = 0; byteIndex < testData.Size(); ++byteIndex)
    {
        testData[byteIndex] = static_cast<uint8_t>(byteIndex % 256);
    }

    SingleThreadedTransferHelper helper(hClientSocket, hServerSocket);
    result = helper.Transfer(testData.Data(), testData.Size());
    ASSERT_EQ(result, DD_RESULT_SUCCESS);

    ddSocketClose(hServerSocket);
    ddSocketClose(hClientSocket);
    ddSocketClose(hListenSocket);
}

INSTANTIATE_TEST_CASE_P(
    ddSocketTransferTest,
    ddSocketSingleThreadedTransferTest,
    ::testing::Values(1, 4, 8, 64, 4096, 65536, 1024 * 1024)
);

class MultiThreadedTransferHelper
{
public:
    MultiThreadedTransferHelper(DDSocket hClientSocket, DDSocket hServerSocket)
        : m_hClientSocket(hClientSocket)
        , m_hServerSocket(hServerSocket)
        , m_pSendData(nullptr)
        , m_sendDataSize(0)
        , m_receiveBuffer(Platform::GenericAllocCb)
    {
    }

    DD_RESULT Transfer(const void* pData, size_t dataSize)
    {
        m_pSendData = pData;
        m_sendDataSize = dataSize;
        m_receiveBuffer.Resize(dataSize);

        DD_RESULT result = DevDriverToDDResult(m_receiveThread.Start(ReceiveThreadFunc, this));

        if (result == DD_RESULT_SUCCESS)
        {
            result = Send();
        }

        if (result == DD_RESULT_SUCCESS)
        {
            result = DevDriverToDDResult(m_receiveThread.Join(1000));
        }

        if (result == DD_RESULT_SUCCESS)
        {
            result = ValidateTransfer();
        }

        return result;
    }

private:
    DD_RESULT Send()
    {
        return ddSocketSend(m_hClientSocket, m_pSendData, m_sendDataSize);
    }

    void Receive()
    {
        ddSocketReceive(m_hServerSocket, m_receiveBuffer.Data(), m_receiveBuffer.Size());
    }

    DD_RESULT ValidateTransfer() const
    {
        return (memcmp(m_pSendData, m_receiveBuffer.Data(), m_sendDataSize) == 0) ? DD_RESULT_SUCCESS : DD_RESULT_PARSING_INVALID_BYTES;
    }

    static void ReceiveThreadFunc(void* pUserdata)
    {
        MultiThreadedTransferHelper* pThis = reinterpret_cast<MultiThreadedTransferHelper*>(pUserdata);

        pThis->Receive();
    }

    DDSocket m_hClientSocket;
    DDSocket m_hServerSocket;

    const void* m_pSendData;
    size_t m_sendDataSize;

    Vector<uint8_t> m_receiveBuffer;
    Platform::Thread m_receiveThread;
};

TEST_P(ddSocketMultiThreadedTransferTest, MultithreadedTransfer)
{
    DDSocket hListenSocket = DD_API_INVALID_HANDLE;
    DDSocketListenInfo listenInfo = {};
    listenInfo.hConnection = m_hServerConnection;
    listenInfo.protocolId = kTestProtocolId;
    listenInfo.maxPending = kTestProtocolMaxPendingConnections;
    DD_RESULT result = ddSocketListen(&listenInfo, &hListenSocket);
    ASSERT_EQ(result, DD_RESULT_SUCCESS);

    DDSocket hClientSocket = DD_API_INVALID_HANDLE;
    DDSocketConnectInfo connectInfo = {};
    connectInfo.hConnection = m_hClientConnection;
    connectInfo.clientId = ddNetQueryClientId(m_hServerConnection);
    connectInfo.protocolId = kTestProtocolId;
    result = ddSocketConnect(&connectInfo, &hClientSocket);
    ASSERT_EQ(result, DD_RESULT_SUCCESS);

    DDSocket hServerSocket = DD_API_INVALID_HANDLE;
    result = ddSocketAccept(hListenSocket, kTestTimeoutInMs, &hServerSocket);
    ASSERT_EQ(result, DD_RESULT_SUCCESS);

    // Initialize the test data
    DevDriver::Vector<uint8_t> testData(Platform::GenericAllocCb);
    testData.Resize(GetParam());
    for (size_t byteIndex = 0; byteIndex < testData.Size(); ++byteIndex)
    {
        testData[byteIndex] = static_cast<uint8_t>(byteIndex % 256);
    }

    MultiThreadedTransferHelper helper(hClientSocket, hServerSocket);
    result = helper.Transfer(testData.Data(), testData.Size());
    ASSERT_EQ(result, DD_RESULT_SUCCESS);

    ddSocketClose(hServerSocket);
    ddSocketClose(hClientSocket);
    ddSocketClose(hListenSocket);
}

INSTANTIATE_TEST_CASE_P(
    ddSocketTransferTest,
    ddSocketMultiThreadedTransferTest,
    ::testing::Values(1, 4, 8, 64, 4096, 65536, 1024 * 1024, 4 * 1024 * 1024, 64 * 1024 * 1024)
);

TEST_P(ddSocketVariableChunkSizesTest, BasicTest)
{
    DDSocket hListenSocket = DD_API_INVALID_HANDLE;
    DDSocketListenInfo listenInfo = {};
    listenInfo.hConnection = m_hServerConnection;
    listenInfo.protocolId = kTestProtocolId;
    listenInfo.maxPending = kTestProtocolMaxPendingConnections;
    DD_RESULT result = ddSocketListen(&listenInfo, &hListenSocket);
    ASSERT_EQ(result, DD_RESULT_SUCCESS);

    DDSocket hClientSocket = DD_API_INVALID_HANDLE;
    DDSocketConnectInfo connectInfo = {};
    connectInfo.hConnection = m_hClientConnection;
    connectInfo.clientId = ddNetQueryClientId(m_hServerConnection);
    connectInfo.protocolId = kTestProtocolId;
    result = ddSocketConnect(&connectInfo, &hClientSocket);
    ASSERT_EQ(result, DD_RESULT_SUCCESS);

    DDSocket hServerSocket = DD_API_INVALID_HANDLE;
    result = ddSocketAccept(hListenSocket, kTestTimeoutInMs, &hServerSocket);
    ASSERT_EQ(result, DD_RESULT_SUCCESS);

    constexpr size_t kTestDataSize = 4096;
    const size_t readChunkSize = std::get<0>(GetParam());
    const size_t writeChunkSize = std::get<1>(GetParam());
    DevDriver::Vector<uint8_t> sendData(Platform::GenericAllocCb);
    sendData.Resize(kTestDataSize);
    for (size_t byteIndex = 0; byteIndex < kTestDataSize; ++byteIndex)
    {
        sendData[byteIndex] = static_cast<uint8_t>(byteIndex % 256);
    }

    // NOTE: We assume the send window can hold at least 4k in this test!
    size_t totalBytesSent = 0;
    while (totalBytesSent < kTestDataSize)
    {
        const void* pData = VoidPtrInc(sendData.Data(), totalBytesSent);
        const size_t bytesToSend = Platform::Min(writeChunkSize, kTestDataSize - totalBytesSent);

        size_t bytesSent = 0;
        result = ddSocketSendRaw(hClientSocket, pData, bytesToSend, kTestTimeoutInMs, &bytesSent);
        if (result == DD_RESULT_SUCCESS)
        {
            totalBytesSent += bytesSent;
        }
        else if (result == DD_RESULT_DD_GENERIC_NOT_READY)
        {
            result = DD_RESULT_SUCCESS;
        }
        ASSERT_EQ(result, DD_RESULT_SUCCESS);
    }
    ASSERT_EQ(totalBytesSent, kTestDataSize);

    DevDriver::Vector<uint8_t> receiveData(Platform::GenericAllocCb);
    receiveData.Resize(kTestDataSize);
    size_t totalBytesReceived = 0;
    while (totalBytesReceived < kTestDataSize)
    {
        void* pData = VoidPtrInc(receiveData.Data(), totalBytesReceived);
        const size_t bytesToReceive = Platform::Min(readChunkSize, kTestDataSize - totalBytesReceived);

        size_t bytesReceived = 0;
        result = ddSocketReceiveRaw(hServerSocket, pData, bytesToReceive, kTestTimeoutInMs, &bytesReceived);
        if (result == DD_RESULT_SUCCESS)
        {
            totalBytesReceived += bytesReceived;
        }
        else if (result == DD_RESULT_DD_GENERIC_NOT_READY)
        {
            result = DD_RESULT_SUCCESS;
        }
        ASSERT_EQ(result, DD_RESULT_SUCCESS);
    }
    ASSERT_EQ(totalBytesReceived, kTestDataSize);

    ASSERT_EQ(memcmp(sendData.Data(), receiveData.Data(), kTestDataSize), 0);

    ddSocketClose(hServerSocket);
    ddSocketClose(hClientSocket);
    ddSocketClose(hListenSocket);
}

INSTANTIATE_TEST_CASE_P(
    ddSocketVariableChunkSizesTest,
    ddSocketVariableChunkSizesTest,
    ::testing::Values(
        std::make_tuple(65536, 32),
        std::make_tuple(65536, 64),
        std::make_tuple(65536, 4096),
        std::make_tuple(65536, 65536),
        std::make_tuple(4096,  65536),
        std::make_tuple(64,    65536),
        std::make_tuple(32,    65536)
    )
);

/// GTest entry-point
GTEST_API_ int main(int argc, char** argv)
{
    // Run all tests
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

