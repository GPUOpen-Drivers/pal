/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

namespace DevDriver
{
    class IProtocolServer;
    class IProtocolClient;

    template <Protocol protocol>
    struct ProtocolServerMap
    {
        typedef IProtocolServer type;
    };

    template <Protocol protocol>
    struct ProtocolClientMap
    {
        typedef IProtocolClient type;
    };

    template <Protocol protocol>
    using ProtocolServerType = typename ProtocolServerMap<protocol>::type;

    template <Protocol protocol>
    using ProtocolClientType = typename ProtocolClientMap<protocol>::type;

    namespace DriverControlProtocol
    {
        class DriverControlServer;
        class DriverControlClient;
    }

    template <>
    struct ProtocolServerMap<Protocol::DriverControl>
    {
        typedef DriverControlProtocol::DriverControlServer type;
    };

    template <>
    struct ProtocolClientMap<Protocol::DriverControl>
    {
        typedef DriverControlProtocol::DriverControlClient type;
    };

    namespace RGPProtocol
    {
        class RGPServer;
        class RGPClient;
    }

    template <>
    struct ProtocolServerMap<Protocol::RGP>
    {
        typedef RGPProtocol::RGPServer type;
    };

    template <>
    struct ProtocolClientMap<Protocol::RGP>
    {
        typedef RGPProtocol::RGPClient type;
    };

    namespace EventProtocol
    {
        class EventServer;
        class EventClient;
    }

    template <>
    struct ProtocolServerMap<Protocol::Event>
    {
        typedef EventProtocol::EventServer type;
    };

    template <>
    struct ProtocolClientMap<Protocol::Event>
    {
        typedef EventProtocol::EventClient type;
    };

    namespace ETWProtocol
    {
        class ETWServer;
        class ETWClient;
    }

    namespace SettingsURIService
    {
        class SettingsService;
    }

    namespace InfoURIService
    {
        class InfoService;
    }

    template <>
    struct ProtocolServerMap<Protocol::ETW>
    {
        typedef ETWProtocol::ETWServer type;
    };

    template <>
    struct ProtocolClientMap<Protocol::ETW>
    {
        typedef ETWProtocol::ETWClient type;
    };

    namespace TransferProtocol
    {
        class TransferServer;
        class TransferClient;
    }

    template <>
    struct ProtocolServerMap<Protocol::Transfer>
    {
        typedef TransferProtocol::TransferServer type;
    };

    template <>
    struct ProtocolClientMap<Protocol::Transfer>
    {
        typedef TransferProtocol::TransferClient type;
    };

    namespace URIProtocol
    {
        class URIServer;
        class URIClient;
    }

    template <>
    struct ProtocolServerMap<Protocol::URI>
    {
        typedef URIProtocol::URIServer type;
    };

    template <>
    struct ProtocolClientMap<Protocol::URI>
    {
        typedef URIProtocol::URIClient type;
    };
}
