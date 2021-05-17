/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#include "ddClientURIService.h"
#include "msgChannel.h"
#include "ddTransferManager.h"
#include "ddVersion.h"

namespace DevDriver
{
    // =====================================================================================================================
    ClientURIService::ClientURIService()
        : m_pMsgChannel(nullptr)
    {
    }

    // =====================================================================================================================
    ClientURIService::~ClientURIService()
    {
    }

    // =====================================================================================================================
    Result ClientURIService::HandleRequest(IURIRequestContext* pContext)
    {
        DD_ASSERT(pContext != nullptr);

        Result result = Result::Unavailable;

        // We can only handle requests if a valid message channel has been bound.
        if (m_pMsgChannel != nullptr)
        {
            if (strcmp(pContext->GetRequestArguments(), "info") == 0)
            {
                // Fetch the desired information about the client.
                const ClientId clientId = m_pMsgChannel->GetClientId();
                const ClientInfoStruct& clientInfo = m_pMsgChannel->GetClientInfo();

                // Write all the info into the response block as plain text.
                ITextWriter* pResponse = nullptr;
                result = pContext->BeginTextResponse(&pResponse);

                if (result == Result::Success)
                {
                    // Write the header
                    pResponse->Write("--- Client Information ---");

                    // Write the gpuopen library version string
                    pResponse->Write("\nClient Version String: %s", GetVersionString());

                    // Write the branch definition string
                    pResponse->Write("\nClient Branch String: %s", DD_BRANCH_STRING);

                    // Write the gpuopen library interface version
                    pResponse->Write("\nClient Available Interface Version: %u.%u", GPUOPEN_INTERFACE_MAJOR_VERSION, GPUOPEN_INTERFACE_MINOR_VERSION);

                    // Write the gpuopen client interface version
                    pResponse->Write("\nClient Supported Interface Major Version: %u", GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION);

                    // Write the client message bus version
                    pResponse->Write("\nClient Supported Message Bus Version: %u", kMessageVersion);

                    // Write the client transport type
                    pResponse->Write("\nClient Transport: %s", m_pMsgChannel->GetTransportName());

                    // Write the client id
                    pResponse->Write("\nClient Id: %u", static_cast<uint32>(clientId));

                    // Write the client type
                    const char* pClientTypeString = "Unknown";
                    switch (clientInfo.metadata.clientType)
                    {
                    case Component::Server: pClientTypeString = "Server"; break;
                    case Component::Tool: pClientTypeString = "Tool"; break;
                    case Component::Driver: pClientTypeString = "Driver"; break;
                    default: DD_WARN_ALWAYS(); break;
                    }
                    pResponse->Write("\nClient Type: %s", pClientTypeString);

                    // Write the client name
                    pResponse->Write("\nClient Name: %s", clientInfo.clientName);

                    // Write the client description
                    pResponse->Write("\nClient Description: %s", clientInfo.clientDescription);

                    // Write the client platform
                    pResponse->Write("\nClient Platform: " DD_PLATFORM_STRING " %d-bit", AMD_TARGET_ARCH_BITS);

                    // Only print protocols + status flags in debug builds for now.
#if !defined(NDEBUG)
                    {
                        IProtocolServer* pServer = m_pMsgChannel->GetProtocolServer(Protocol::Transfer);
                        if (pServer != nullptr)
                        {
                            pResponse->Write("\nClient Transfer Protocol Supported Versions: (%u -> %u)", pServer->GetMinVersion(), pServer->GetMaxVersion());
                        }
                    }

                    {
                        IProtocolServer* pServer = m_pMsgChannel->GetProtocolServer(Protocol::URI);
                        if (pServer != nullptr)
                        {
                            pResponse->Write("\nClient URI Protocol Supported Versions: (%u -> %u)", pServer->GetMinVersion(), pServer->GetMaxVersion());
                        }
                    }

                    // Write the protocols
                    pResponse->Write("\nClient Driver Control Protocol Support: %u", clientInfo.metadata.protocols.driverControl);

                    if (clientInfo.metadata.protocols.driverControl)
                    {
                        IProtocolServer* pServer = m_pMsgChannel->GetProtocolServer(Protocol::DriverControl);
                        if (pServer != nullptr)
                        {
                            pResponse->Write("\nClient Driver Control Protocol Supported Versions: (%u -> %u)", pServer->GetMinVersion(), pServer->GetMaxVersion());
                        }
                    }

                    pResponse->Write("\nClient RGP Protocol Support: %u", clientInfo.metadata.protocols.rgp);

                    if (clientInfo.metadata.protocols.rgp)
                    {
                        IProtocolServer* pServer = m_pMsgChannel->GetProtocolServer(Protocol::RGP);
                        if (pServer != nullptr)
                        {
                            pResponse->Write("\nClient RGP Protocol Supported Versions: (%u -> %u)", pServer->GetMinVersion(), pServer->GetMaxVersion());
                        }
                    }

                    pResponse->Write("\nClient ETW Protocol Support: %u", clientInfo.metadata.protocols.etw);

                    if (clientInfo.metadata.protocols.etw)
                    {
                        IProtocolServer* pServer = m_pMsgChannel->GetProtocolServer(Protocol::ETW);
                        if (pServer != nullptr)
                        {
                            pResponse->Write("\nClient ETW Protocol Supported Versions: (%u -> %u)", pServer->GetMinVersion(), pServer->GetMaxVersion());
                        }
                    }

                    // Write the status flags
                    const uint32 developerModeEnabled = ((clientInfo.metadata.status & static_cast<uint32>(ClientStatusFlags::DeveloperModeEnabled)) != 0) ? 1 : 0;
                    pResponse->Write("\nClient Developer Mode Status Flag: %u", developerModeEnabled);

                    const uint32 deviceHaltOnConnect = ((clientInfo.metadata.status & static_cast<uint32>(ClientStatusFlags::DeviceHaltOnConnect)) != 0) ? 1 : 0;
                    pResponse->Write("\nClient Device Halt On Connect Status Flag: %u", deviceHaltOnConnect);

                    const uint32 gpuCrashDumpsEnabled = ((clientInfo.metadata.status & static_cast<uint32>(ClientStatusFlags::GpuCrashDumpsEnabled)) != 0) ? 1 : 0;
                    pResponse->Write("\nClient Gpu Crash Dumps Enabled Status Flag: %u", gpuCrashDumpsEnabled);

                    const uint32 pipelineDumpsEnabled = ((clientInfo.metadata.status & static_cast<uint32>(ClientStatusFlags::PipelineDumpsEnabled)) != 0) ? 1 : 0;
                    pResponse->Write("\nClient Pipeline Dumps Enabled Status Flag: %u", pipelineDumpsEnabled);

                    const uint32 platformHaltOnConnect = ((clientInfo.metadata.status & static_cast<uint32>(ClientStatusFlags::PlatformHaltOnConnect)) != 0) ? 1 : 0;
                    pResponse->Write("\nClient Platform Halt On Connect Status Flag: %u", platformHaltOnConnect);
#endif

                    // Write the process id
                    pResponse->Write("\nClient Process Id: %u", clientInfo.processId);

                    result = pResponse->End();
                }
            }
        }

        return result;
    }
} // DevDriver
