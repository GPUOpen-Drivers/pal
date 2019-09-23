/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
/**
***********************************************************************************************************************
* @file  ddDevModeControlCmds.h
* @brief Developer mode control command definitions
***********************************************************************************************************************
*/
#pragma once

#include <gpuopen.h>
#include <ddDevModeControl.h>
#include <ddDevModeQueue.h>

namespace DevDriver
{
    ///
    /// Input and Output structures
    /// These structures are copied verbatim from messagelib and are not designed to be used directly.
    ///

    ////////////////////////////
    //// QueryStatus
    DD_NETWORK_STRUCT(QueryStatusOutput, 4)
    {
        uint32          maxMessageSize;
        uint32          maxQueueLength;
        StatusFlags     flags;
        uint8           padding[2];
    };

    DD_CHECK_SIZE(QueryStatusOutput, 12);

    DD_NETWORK_STRUCT(UpdateClientStatusInput, 4)
    {
        ClientId    clientId;
        // pad out to 4 bytes for alignment requirements
        uint8       padding[2];
        StatusFlags flags;
        uint8       reserved[2];
    };

    DD_CHECK_SIZE(UpdateClientStatusInput, 8);

    //// RegisterClient
    DD_NETWORK_STRUCT(RegisterClientInput, 8)
    {
        // Message handlers to communicate with this client
        QueueInfo       messageQueueSend;
        QueueInfo       messageQueueReceive;
        StatusFlags     initialClientFlags;
        uint8           padding[2];
        Component       component;
        // pad out to 8 byte alignment
        uint8           reserved[3];
    };

    DD_CHECK_SIZE(RegisterClientInput, 104);

    DD_NETWORK_STRUCT(RegisterClientOutput, 8)
    {
        QueueInfo       sendQueue;
        QueueInfo       receiveQueue;
        ClientId        clientId;
        // pad out to 8 bytes for alignment requirements
        uint8           padding[6];
    };

    DD_CHECK_SIZE(RegisterClientOutput, 104);

    //// UnregisterClient
    DD_NETWORK_STRUCT(UnregisterClientInput, 4)
    {
        ClientId        clientId;
        // pad out to 2 bytes for alignment requirements
        uint8           padding[2];
    };

    DD_CHECK_SIZE(UnregisterClientInput, 4);

    //// RegisterExternalClient
    DD_NETWORK_STRUCT(RegisterExternalClientInput, 4)
    {
        StatusFlags     initialClientFlags;
        uint8           reserved[2];
        ClientId        routerId;
        Component       component;
        // pad out to 8 bytes for 4 byte alignment
        uint8           padding[1];
    };

    DD_CHECK_SIZE(RegisterExternalClientInput, 8);

    DD_NETWORK_STRUCT(RegisterExternalClientOutput, 4)
    {
        ClientId        clientId;
        // pad out to 4 bytes
        uint8           padding[2];
    };

    DD_CHECK_SIZE(RegisterExternalClientOutput, 4);

    //// UnregisterExternalClient
    DD_NETWORK_STRUCT(UnregisterExternalClientInput, 4)
    {
        ClientId        clientId;
        ClientId        routerId;
    };

    DD_CHECK_SIZE(UnregisterExternalClientInput, 4);

    ////////////////////////////
    //// QueryCapabilities Escape Call

    // QueryCapabilities output
    DD_NETWORK_STRUCT(QueryCapabilitiesOutput, 4)
    {
        uint32             version;  // Supported escape call version
        DeveloperModeFlags features; // Supported features
    };

    DD_CHECK_SIZE(QueryCapabilitiesOutput, 8);

    ////////////////////////////
    //// EnableDeveloperMode Escape Call

    // EnableDeveloperMode Input
    DD_NETWORK_STRUCT(EnableDeveloperModeInput, 4)
    {
        DeveloperModeSettings settings; // Developer Mode initialization settings
    };

    DD_CHECK_SIZE(EnableDeveloperModeInput, 8);

    ////////////////////////////
    //// QueryDeveloperModeStatus Escape Call

    // QueryDeveloperModeStatus output
    DD_NETWORK_STRUCT(QueryDeveloperModeStatusOutput, 4)
    {
        DeveloperModeSettings settings; // Current settings
    };

    DD_CHECK_SIZE(QueryDeveloperModeStatusOutput, 8);

    ////////////////////////////
    //// RegisterRouter Escape Call

    // RegisterRouter Input
    DD_NETWORK_STRUCT(RegisterRouterInput, 8)
    {
        QueueInfo       sendQueue;      // Router send queue
        QueueInfo       receiveQueue;   // Router receive queue
        RouterPrefix    routingPrefix;  // The routing prefix for the router to be registered
        uint8           reserved[4];    // Pad out to 8 byte alignment
    };

    DD_CHECK_SIZE(RegisterRouterInput, 104);

    // RegisterRouter Output
    DD_NETWORK_STRUCT(RegisterRouterOutput, 8)
    {
        QueueInfo sendQueue;    // Router send queue
        QueueInfo receiveQueue; // Router receive queue
    };

    DD_CHECK_SIZE(RegisterRouterOutput, 96);

    ////////////////////////////
    //// UnregisterRouter Escape Call

    // UnregisterRouter Input
    DD_NETWORK_STRUCT(UnregisterRouterInput, 4)
    {
        RouterPrefix routingPrefix; // The routing prefix for the router to be unregistered
    };

    DD_CHECK_SIZE(UnregisterRouterInput, 4);

    ///
    /// DevMode Command structures
    /// These structures are designed to be consumed directly within DevDriver code. They act as aggregates with
    /// compile-time validation for correct usage.
    ///

    ///
    /// DevMode Command In/Out Request Types
    ///

    /// ==== QueryCapabilities =====================================================================
    DD_NETWORK_STRUCT(QueryCapabilitiesRequest, 4)
    {
        static constexpr DevModeCmd kCmd = DevModeCmd::QueryCapabilities;

        DevModeResponseHeader   header = DevModeResponseHeader::FromCmd(kCmd);
        QueryCapabilitiesOutput output;
    };

    DD_CHECK_SIZE(QueryCapabilitiesRequest, 24);

    /// ==== QueryDeveloperModeStatus ==============================================================
    DD_NETWORK_STRUCT(QueryDeveloperModeStatusRequest, 4)
    {
        static constexpr DevModeCmd kCmd = DevModeCmd::QueryDeveloperModeStatus;

        DevModeResponseHeader          header = DevModeResponseHeader::FromCmd(kCmd);
        QueryDeveloperModeStatusOutput output;
    };

    DD_CHECK_SIZE(QueryDeveloperModeStatusRequest, 24);

    /// ==== RegisterClient ========================================================================
    DD_NETWORK_STRUCT(RegisterClientRequest, 8)
    {
        static constexpr DevModeCmd kCmd = DevModeCmd::RegisterClient;

        DevModeResponseHeader header = DevModeResponseHeader::FromCmd(kCmd);
        RegisterClientInput   input;
        RegisterClientOutput  output;
    };

    DD_CHECK_SIZE(RegisterClientRequest, 224);

    /// ==== RegisterRouter ========================================================================
    DD_NETWORK_STRUCT(RegisterRouterRequest, 8)
    {
        static constexpr DevModeCmd kCmd = DevModeCmd::RegisterRouter;

        DevModeResponseHeader header = DevModeResponseHeader::FromCmd(kCmd);
        RegisterRouterInput   input;
        RegisterRouterOutput  output;
    };

    DD_CHECK_SIZE(RegisterRouterRequest, 216);

    /// ==== UnregisterRouter ======================================================================
    DD_NETWORK_STRUCT(UnregisterRouterRequest, 4)
    {
        static constexpr DevModeCmd kCmd = DevModeCmd::UnregisterRouter;

        DevModeResponseHeader header = DevModeResponseHeader::FromCmd(kCmd);
        UnregisterRouterInput input;
    };

    DD_CHECK_SIZE(UnregisterRouterRequest, 20);

    /// ==== UnregisterClient ======================================================================
    DD_NETWORK_STRUCT(UnregisterClientRequest, 4)
    {
        static constexpr DevModeCmd kCmd = DevModeCmd::UnregisterClient;

        DevModeResponseHeader header = DevModeResponseHeader::FromCmd(kCmd);
        UnregisterClientInput input;
    };

    DD_CHECK_SIZE(UnregisterClientRequest, 20);

    /// ==== EnableDeveloperMode ===================================================================
    DD_NETWORK_STRUCT(EnableDeveloperModeRequest, 4)
    {
        static constexpr DevModeCmd kCmd = DevModeCmd::EnableDeveloperMode;

        DevModeResponseHeader    header = DevModeResponseHeader::FromCmd(kCmd);
        EnableDeveloperModeInput input;
    };

    DD_CHECK_SIZE(EnableDeveloperModeRequest, 24);

    /// ==== DisableDeveloperMode ==================================================================
    DD_NETWORK_STRUCT(DisableDeveloperModeRequest, 4)
    {
        static constexpr DevModeCmd kCmd = DevModeCmd::DisableDeveloperMode;

        DevModeResponseHeader header = DevModeResponseHeader::FromCmd(kCmd);
    };

    DD_CHECK_SIZE(DisableDeveloperModeRequest, 16);
}
