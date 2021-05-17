/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include "gpuopen.h"
#include "msgChannel.h"
#include "msgTransport.h"
#include "protocols/systemProtocols.h"
#include "protocols/typemap.h"

namespace DevDriver
{
    class IProtocolServer;

    // Server Creation Info
    // This struct extends the MessageChannelCreateInfo struct and adds information about the destination host
    // the client will connect to. It additionally allows specifying protocol servers to enable during initialization.
    // See msgChannel.h for a full list of members.
    struct ServerCreateInfo : public MessageChannelCreateInfo
    {
        HostInfo                 connectionInfo;    // Connection information describing how the Server should connect
                                                    // to the message bus.
        ProtocolFlags            servers;           // Set of boolean values indicating which servers should be created
                                                    // during initialization.
    };

    DD_STATIC_CONST uint32 kQueryStatusTimeoutInMs = 50;

    class DevDriverServer
    {
    public:
        static bool IsConnectionAvailable(const HostInfo& hostInfo, uint32 timeout = kQueryStatusTimeoutInMs);

        explicit DevDriverServer(const AllocCb& allocCb, const ServerCreateInfo& createInfo);
        ~DevDriverServer();

        Result Initialize();
        void Finalize();
        void Destroy();

#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION < GPUOPEN_DRIVER_CONTROL_CLEANUP_VERSION
        // Called by the driver to mark the end of Platform and the start of device initialization.
        // Starting with GPUOPEN_DRIVER_CONTROL_CLEANUP_VERSION the driver should call the driver control
        // functions directly.
        void StartDeviceInit();
#endif

        bool IsConnected() const;
        IMsgChannel* GetMessageChannel() const;

        DriverControlProtocol::DriverControlServer* GetDriverControlServer();
        RGPProtocol::RGPServer* GetRGPServer();
        EventProtocol::EventServer* GetEventServer();
        SettingsURIService::SettingsService* GetSettingsService();
        InfoURIService::InfoService* GetInfoService();

        bool ShouldShowOverlay();

    private:
        Result InitializeProtocols();
        void DestroyProtocols();

        Result RegisterProtocol(Protocol protocol);
        void UnregisterProtocol(Protocol protocol);
        void FinalizeProtocol(Protocol protocol);

        IMsgChannel*     m_pMsgChannel;
        AllocCb          m_allocCb;
        ServerCreateInfo m_createInfo;

        SettingsURIService::SettingsService* m_pSettingsService;

        template <Protocol protocol, class ...Args>
        inline Result RegisterProtocol(Args... args);

        template <Protocol protocol>
        inline ProtocolServerType<protocol>* GetServer();
    };

} // DevDriver
