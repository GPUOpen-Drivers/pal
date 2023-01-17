/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <ddNet.h>
#include <gpuopen.h>
#include <ddCommon.h>
#include <msgChannel.h>

using namespace DevDriver;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Helper function that converts a DD_NET_CLIENT_TYPE into a DevDriver::Component
Component ClientTypeToComponent(
    DD_NET_CLIENT_TYPE type)
{
    Component component = Component::Unknown;
    switch (type)
    {
    case DD_NET_CLIENT_TYPE_SERVER:
    {
        component = Component::Server;
        break;
    }

    case DD_NET_CLIENT_TYPE_TOOL:
    case DD_NET_CLIENT_TYPE_TOOL_WITH_DRIVER_INIT:
    {
        component = Component::Tool;
        break;
    }

    case DD_NET_CLIENT_TYPE_DRIVER:
    case DD_NET_CLIENT_TYPE_DRIVER_KERNEL:
    {
        component = Component::Driver;
        break;
    }

    default:
    {
        // Allow the function to return the unknown component type if nothing matches
        break;
    }
    }

    return component;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Helper function that converts DevDriver::ClientMetadata into a DD_NET_CLIENT_TYPE
DD_NET_CLIENT_TYPE MetadataToClientType(
    const ClientMetadata& metadata)
{
    DD_NET_CLIENT_TYPE type = DD_NET_CLIENT_TYPE_UNKNOWN;

    switch (metadata.clientType)
    {
    case Component::Server:
    {
        type = DD_NET_CLIENT_TYPE_SERVER;

        break;
    }

    case Component::Tool:
    {
        const bool developerModeEnabled =
            ((metadata.status & static_cast<uint32_t>(ClientStatusFlags::DeveloperModeEnabled)) != 0);

        const bool driverInitEnabled =
            (((metadata.status & static_cast<uint32_t>(ClientStatusFlags::PlatformHaltOnConnect)) != 0) ||
             ((metadata.status & static_cast<uint32_t>(ClientStatusFlags::DriverInitializer)) != 0));

        // We only consider clients with the appropriate flags set to be "true" tools
        if (developerModeEnabled)
        {
            if (driverInitEnabled)
            {
                type = DD_NET_CLIENT_TYPE_TOOL_WITH_DRIVER_INIT;
            }
            else
            {
                type = DD_NET_CLIENT_TYPE_TOOL;
            }
        }

        break;
    }
    case Component::Driver:
    {
        // TODO: We need a REAL way to identify the kernel driver
        type = DD_NET_CLIENT_TYPE_DRIVER;

        break;
    }

    default:
    {
        // Allow the function to return the unknown component type if nothing matches
        break;
    }
    }

    return type;
}

/// A helper struct for storing client info used for the function `InitializeMsgChannel`.
struct ClientInfo
{
    DD_NET_CLIENT_TYPE type;         /// Type of client
    const char*        pDescription; /// Brief description of the client.
    uint32_t           timeoutInMs;  /// Number of milliseconds to wait before timing out the connection operation.
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Helper function that creates a message channel based on the provided input parameters
///
/// This is used by both the local and remote connection creation functions
DD_RESULT InitializeMsgChannel(
    const ClientInfo&          clientInfo,
    const DevDriver::HostInfo& hostInfo,
    DevDriver::IMsgChannel**   ppMsgChannel)
{
    // Assert on programmer error
    DD_ASSERT(ppMsgChannel != nullptr);

    MessageChannelCreateInfo2 createInfo = {};

    // Message channel configuration
    Platform::Strncpy(createInfo.channelInfo.clientDescription, clientInfo.pDescription);
    createInfo.channelInfo.createUpdateThread = true;
    createInfo.channelInfo.componentType = ClientTypeToComponent(clientInfo.type);

    // Tools need to specify special client status flags so drivers can understand their intentions over the network
    // and modify their own behavior accordingly.
    if ((clientInfo.type == DD_NET_CLIENT_TYPE_TOOL) ||
        (clientInfo.type == DD_NET_CLIENT_TYPE_TOOL_WITH_DRIVER_INIT))
    {
        createInfo.channelInfo.initialFlags |=
            static_cast<StatusFlags>(ClientStatusFlags::DeveloperModeEnabled);

        if (clientInfo.type == DD_NET_CLIENT_TYPE_TOOL_WITH_DRIVER_INIT)
        {
            // We currently have multiple flags that indicate some sort of support for driver initialization
            // but we're trying to standardize on a single one to reduce complexity. (DriverInitializer)
            // The value isn't used yet, but we want to start setting it now to make back-compat easier.
            createInfo.channelInfo.initialFlags |=
                (static_cast<StatusFlags>(ClientStatusFlags::PlatformHaltOnConnect) |
                 static_cast<StatusFlags>(ClientStatusFlags::DriverInitializer));
        }
    }

    // Target host information
    createInfo.hostInfo = hostInfo;

    // Memory allocation callbacks
    // TODO: Implement Memory Allocation Callbacks for ddNet (Related to #48)
    //       Unfortunately, this is MUCH more complicated than you'd think. In order to create DevDriver AllocCbs
    //       from the DDAllocCallbacks, we need to ensure that an ApiAllocCallbacks structure shares the same lifetime
    //       as the IMsgChannel. This is very difficult to do since we don't control the memory allocation for the
    //       object right now, and it often gets created and passed to us from places other than ddNet. It's not
    //       possible for us to deal with that case. The only way to robustly handle this is to make sure we only
    //       receive message channel pointers that were created by ddNet which basically requires moving every existing
    //       piece of code that uses message channels onto ddNet. This is a significant amount of work for something
    //       we're not sure is actually required in the driver. For now, we just use global allocators.
    const AllocCb allocCb = Platform::GenericAllocCb;
    createInfo.allocCb = allocCb;

    IMsgChannel* pMsgChannel = nullptr;
    Result result = CreateMessageChannel(createInfo, &pMsgChannel);
    if (result == Result::Success)
    {
        // Ensure that we got a valid pointer back
        DD_ASSERT(pMsgChannel != nullptr);

        const uint32_t timeoutInMs = (clientInfo.timeoutInMs == 0) ? kDefaultConnectionTimeoutMs
                                                                   : clientInfo.timeoutInMs;

        result = pMsgChannel->Register(timeoutInMs);

        if (result != Result::Success)
        {
            DD_DELETE(pMsgChannel, allocCb);
        }
    }

    if (result == Result::Success)
    {
        *ppMsgChannel = pMsgChannel;
    }

    DD_RESULT finalResult = DevDriverToDDResult(result);

    // Translate some of the generic error codes to ddNet specific ones
    if (finalResult == DD_RESULT_DD_GENERIC_NOT_READY)
    {
        finalResult = DD_RESULT_NET_TIMED_OUT;
    }
    else if (finalResult == DD_RESULT_DD_GENERIC_FILE_ACCESS_ERROR)
    {
        finalResult = DD_RESULT_NET_CONNECTION_REFUSED;
    }

    return finalResult;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DDApiVersion ddNetQueryVersion(void)
{
    DDApiVersion version = {};

    version.major = DD_NET_API_MAJOR_VERSION;
    version.minor = DD_NET_API_MINOR_VERSION;
    version.patch = DD_NET_API_PATCH_VERSION;

    return version;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* ddNetQueryVersionString(void)
{
    return DD_NET_API_VERSION_STRING;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* ddNetResultToString(
    DD_RESULT result)
{
    return ddApiResultToString(result);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddNetCreateConnection(
    const DDNetConnectionInfo* pInfo,
    DDNetConnection*           phConnection)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    HostInfo hostInfo = {};

    if ((pInfo != nullptr)               &&
        (phConnection != nullptr)        &&
        (pInfo->pDescription != nullptr) &&
        DD_VALIDATE_ENUM(DD_NET_CLIENT_TYPE, pInfo->type))
    {
        if (pInfo->pHostname != nullptr)
        {
            if (strlen(pInfo->pHostname) != 0)
            {
                hostInfo.type = TransportType::Remote;

                hostInfo.pHostname = pInfo->pHostname;

                // We replace the port with our default value if the application provides 0 as the port number.
                hostInfo.port = ((pInfo->port != 0) ? pInfo->port : DD_API_DEFAULT_NETWORK_PORT);

                result = DD_RESULT_SUCCESS;
            }
            else
            {
                DD_PRINT(
                    LogLevel::Warn,
                    "Attempting to connect to the hostname \"\", which is empty. This is probably a programmer error",
                    pInfo->pHostname);
            }
        }
        else // Attempt local connection if `pHostname` is nullptr.
        {
            hostInfo = kDefaultNamedPipe;
            hostInfo.port = pInfo->port;

            result = DD_RESULT_SUCCESS;
        }
    }

    if (result == DD_RESULT_SUCCESS)
    {
        IMsgChannel* pMsgChannel = nullptr;
        const ClientInfo clientInfo =
        {
            pInfo->type,
            pInfo->pDescription,
            pInfo->timeoutInMs,
        };
        result = InitializeMsgChannel(clientInfo, hostInfo, &pMsgChannel);

        if (result == DD_RESULT_SUCCESS)
        {
            *phConnection = ToHandle(pMsgChannel);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ddNetDestroyConnection(
    DDNetConnection hConnection)
{
    if (hConnection != DD_API_INVALID_HANDLE)
    {
        IMsgChannel* pMsgChannel = FromHandle(hConnection);

        // We have to extract the AllocCb structure from the message channel object first since it's about to be deleted
        const AllocCb allocCb = pMsgChannel->GetAllocCb();
        DD_DELETE(pMsgChannel, allocCb);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DDClientId ddNetQueryClientId(
    DDNetConnection hConnection)
{
    DDClientId clientId = DD_API_INVALID_CLIENT_ID;

    IMsgChannel* pMsgChannel = FromHandle(hConnection);
    if (pMsgChannel != nullptr)
    {
        clientId = static_cast<DDClientId>(pMsgChannel->GetClientId());
    }

    return clientId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddNetDiscover(
    DDNetConnection          hConnection,
    const DDNetDiscoverInfo* pInfo)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if ((hConnection != DD_API_INVALID_HANDLE) && (pInfo != nullptr))
    {
        IMsgChannel* pMsgChannel = FromHandle(hConnection);

        ClientMetadata filter = {};
        filter.clientType = ClientTypeToComponent(pInfo->targetType);

        DiscoverClientsInfo discoverInfo = {};
        discoverInfo.pfnCallback = [](void* pUserdata, const DiscoveredClientInfo& info) -> bool {
            const auto* pDiscoverInfo = *reinterpret_cast<const DDNetDiscoverInfo**>(pUserdata);

            bool continueDiscovery = true;

            // NOTE: We only propagate clients that have full information to simplify the top level API
            //       This will cause older clients to be ignored by users of this ddNet. If tool code
            //       needs to support legacy client code, it should use the legacy gpuopen library
            //       implementation directly.
            if (info.clientInfo.valid)
            {
                DDNetDiscoveredClientInfo clientInfo = {};

                clientInfo.pProcessName = info.clientInfo.data.clientName;
                clientInfo.pDescription = info.clientInfo.data.clientDescription;
                clientInfo.processId    = info.clientInfo.data.processId;
                clientInfo.type         = MetadataToClientType(info.metadata);
                clientInfo.id           = static_cast<DDClientId>(info.id);

                continueDiscovery = (pDiscoverInfo->pfnCallback(pDiscoverInfo->pUserdata, &clientInfo) != 0);
            }
            else
            {
                DD_PRINT(LogLevel::Warn, "Ignoring client with incomplete information. This client is likely using an older library and should update.");
            }

            return continueDiscovery;
        };
        discoverInfo.pUserdata   = &pInfo;
        discoverInfo.filter      = filter;
        discoverInfo.timeoutInMs = pInfo->timeoutInMs;

        result = DevDriverToDDResult(pMsgChannel->DiscoverClients(discoverInfo));
    }

    return result;
}
