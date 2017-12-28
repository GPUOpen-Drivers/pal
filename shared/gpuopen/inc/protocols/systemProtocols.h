/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  systemProtocols.h
* @brief Protocol header for all system protocols
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"

/*
***********************************************************************************************************************
* URI Protocol
***********************************************************************************************************************
*/

#define URI_PROTOCOL_MAJOR_VERSION 2
#define URI_PROTOCOL_MINOR_VERSION 0

#define URI_INTERFACE_VERSION ((URI_INTERFACE_MAJOR_VERSION << 16) | URI_INTERFACE_MINOR_VERSION)

#define URI_PROTOCOL_MINIMUM_MAJOR_VERSION 1

/*
***********************************************************************************************************************
*| Version | Change Description                                                                                       |
*| ------- | ---------------------------------------------------------------------------------------------------------|
*|  2.0    | Added support for response data formats.                                                                 |
*|  1.0    | Initial version                                                                                          |
***********************************************************************************************************************
*/

#define URI_RESPONSE_FORMATS_VERSION 2
#define URI_INITIAL_VERSION 1

namespace DevDriver
{

    namespace SystemProtocol
    {
        ///////////////////////
        // GPU Open System Protocol
        enum struct SystemMessage : MessageCode
        {
            Unknown = 0,
            ClientConnected,
            ClientDisconnected,
            Ping,
            Pong,
            QueryClientInfo,
            ClientInfo,
            Halted,
            Count,
        };
    }

    namespace SessionProtocol
    {
        ///////////////////////
        // GPU Open Session Protocol
        enum struct SessionMessage : MessageCode
        {
            Unknown = 0,
            Syn,
            SynAck,
            Fin,
            Data,
            Ack,
            Rst,
            Count
        };

        typedef uint8 SessionVersion;
        // Session protocol 2 lets session servers return session version as part of the synack
        DD_STATIC_CONST SessionVersion kSessionProtocolVersionSynAckVersion = 2;
        // Session protocol 1 lets session clients specify a max range supported as part of the syn
        DD_STATIC_CONST SessionVersion kSessionProtocolRangeVersion = 1;
        // current version is 2
        DD_STATIC_CONST SessionVersion kSessionProtocolVersion = kSessionProtocolVersionSynAckVersion;
        // not mentioned is session version 0. It only supported min version in SynAck, servers reporting it cannot
        // cleanly terminate in response to a Fin packet.

        // tripwire - this intentionally will break if the message version changes. Since that implies a breaking change, we need to address
        // to re-baseline this as version 0 and update the SynPayload struct at the same time
        static_assert(kMessageVersion == 1011, "Session packets need to be cleaned up as part of the next protocol version");

        DD_ALIGNED_STRUCT(4) SynPayload
        {
            Version         minVersion;
            Protocol        protocol;
            // pad out to 4 bytes
            SessionVersion  sessionVersion;

            // New fields read if sessionVersion != 0
            Version         maxVersion;
            // pad out to 8 bytes
            uint8           reserved[2];
        };

        DD_CHECK_SIZE(SynPayload, 8);

        //DD_ALIGNED_STRUCT(4) SynPayloadV2
        //{
        //    Protocol        protocol;
        //    SessionVersion  sessionVersion;
        //    Version         minVersion;
        //    Version         maxVersion;
        //    // pad out to 8 bytes
        //    uint8           reserved[2];
        //};

        //DD_CHECK_SIZE(SynPayloadV2, 8);

        DD_ALIGNED_STRUCT(8) SynAckPayload
        {
            Sequence            sequence;
            SessionId           initialSessionId;
            Version             version;
            SessionVersion      sessionVersion;
            uint8               reserved[1];
        };

        DD_CHECK_SIZE(SynAckPayload, 16);
    }

    namespace ClientManagementProtocol
    {

        ///////////////////////
        // GPU Open ClientManagement Protocol
        enum struct ManagementMessage : MessageCode
        {
            Unknown = 0,
            ConnectRequest,
            ConnectResponse,
            DisconnectNotification,
            DisconnectResponse,
            SetClientFlags,
            SetClientFlagsResponse,
            QueryStatus,
            QueryStatusResponse,
            KeepAlive,
            Count
        };

        DD_STATIC_CONST MessageBuffer kOutOfBandMessage =
        {
            { // header
                kBroadcastClientId,             //srcClientId
                kBroadcastClientId,             //dstClientId
                Protocol::ClientManagement,     //protocolId
                0,                              //messageId
                0,                              //windowSize
                0,                              //payloadSize
                0,                              //sessionId
                kMessageVersion                 //sequence
            },
            {} // payload
        };

        inline bool IsOutOfBandMessage(const MessageBuffer &message)
        {
            // an out of band message is denoted by both the dstClientId and srcClientId
            // being initialized to kBroadcastClientId.
            static_assert(kBroadcastClientId == 0, "Error, kBroadcastClientId is non-zero. IsOutOfBandMessage needs to be fixed");
            return ((message.header.dstClientId | message.header.srcClientId) == kBroadcastClientId);
        }

        inline bool IsValidOutOfBandMessage(const MessageBuffer &message)
        {
            // an out of band message is only valid if the sequence field is initialized with the correct version
            // and the protocolId is equal to the receiving client's Protocol::ClientManagement value
            return ((message.header.sequence == kMessageVersion) &
                    (message.header.protocolId == Protocol::ClientManagement));
        }

        DD_ALIGNED_STRUCT(4) ConnectRequestPayload
        {
            StatusFlags initialClientFlags;
            uint8       padding[2];
            Component   componentType;
            uint8       reserved[3];
        };

        DD_CHECK_SIZE(ConnectRequestPayload, 8);

        DD_ALIGNED_STRUCT(4) ConnectResponsePayload
        {
            Result      result;
            ClientId    clientId;
            // pad this out to 8 bytes for future expansion
            uint8       padding[2];
        };

        DD_CHECK_SIZE(ConnectResponsePayload, 8);

        DD_ALIGNED_STRUCT(4) SetClientFlagsPayload
        {
            StatusFlags flags;
            uint8       padding[2];
        };

        DD_CHECK_SIZE(SetClientFlagsPayload, 4);

        DD_ALIGNED_STRUCT(4) SetClientFlagsResponsePayload
        {
            Result      result;
        };

        DD_CHECK_SIZE(SetClientFlagsResponsePayload, 4);

        DD_ALIGNED_STRUCT(4) QueryStatusResponsePayload
        {
            Result      result;
            StatusFlags flags;
            uint8       reserved[2];
        };

        DD_CHECK_SIZE(QueryStatusResponsePayload, 8);
    }

    namespace TransferProtocol
    {
        ///////////////////////
        // GPU Open Transfer Protocol
        enum struct TransferMessage : MessageCode
        {
            Unknown = 0,
            TransferRequest,
            TransferDataHeader,
            TransferDataChunk,
            TransferDataSentinel,
            TransferAbort,
            Count,
        };

        // @note: We currently subtract sizeof(uint32) instead of sizeof(TransferMessage) to work around struct packing issues.
        //        The compiler pads out TransferMessage to 4 bytes when it's included in the payload struct.
        DD_STATIC_CONST Size kMaxTransferDataChunkSize = (kMaxPayloadSizeInBytes - sizeof(uint32));

        ///////////////////////
        // Transfer Types
        typedef uint32_t BlockId;
        DD_STATIC_CONST BlockId kInvalidBlockId = 0;

        ///////////////////////
        // Transfer Payloads

        DD_ALIGNED_STRUCT(4) TransferRequestPayload
        {
            BlockId blockId;
        };

        DD_CHECK_SIZE(TransferRequestPayload, 4);

        DD_ALIGNED_STRUCT(4) TransferDataHeaderPayload
        {
            Result result;
            uint32 sizeInBytes;
        };

        DD_CHECK_SIZE(TransferDataHeaderPayload, 8);

        DD_ALIGNED_STRUCT(4) TransferDataChunkPayload
        {
            uint8 data[kMaxTransferDataChunkSize];
        };

        DD_CHECK_SIZE(TransferDataChunkPayload, kMaxTransferDataChunkSize);

        DD_ALIGNED_STRUCT(4) TransferDataSentinelPayload
        {
            Result result;
        };

        DD_CHECK_SIZE(TransferDataSentinelPayload, 4);

        DD_ALIGNED_STRUCT(4) TransferPayload
        {
            TransferMessage  command;
            // pad out to 4 bytes for alignment requirements
            char        padding[3];
            union
            {
                TransferRequestPayload      transferRequest;
                TransferDataHeaderPayload   transferDataHeader;
                TransferDataChunkPayload    transferDataChunk;
                TransferDataSentinelPayload transferDataSentinel;
            };
        };

        DD_CHECK_SIZE(TransferPayload, kMaxPayloadSizeInBytes);
    }

    namespace URIProtocol
    {
        ///////////////////////
        // GPU Open URI Protocol
        enum struct URIMessage : MessageCode
        {
            Unknown = 0,
            URIRequest,
            URIResponse,
            Count,
        };

        ///////////////////////
        // URI Types
        enum struct ResponseDataFormat : uint32
        {
            Unknown = 0,
            Text,
            Binary,
            Count
        };

        ///////////////////////
        // URI Constants
        DD_STATIC_CONST uint32 kURIStringSize = 256;

        ///////////////////////
        // URI Payloads

        DD_ALIGNED_STRUCT(4) URIRequestPayload
        {
            char uriString[kURIStringSize];
        };

        DD_CHECK_SIZE(URIRequestPayload, kURIStringSize);

        DD_ALIGNED_STRUCT(4) URIResponsePayload
        {
            Result result;
            TransferProtocol::BlockId blockId;
        };

        DD_CHECK_SIZE(URIResponsePayload, 8);

        DD_ALIGNED_STRUCT(4) URIResponsePayloadV2
        {
            Result result;
            TransferProtocol::BlockId blockId;
            ResponseDataFormat format;
        };

        DD_CHECK_SIZE(URIResponsePayloadV2, 12);

        DD_ALIGNED_STRUCT(4) URIPayload
        {
            URIMessage  command;
            // pad out to 4 bytes for alignment requirements
            char        padding[3];
            union
            {
                URIRequestPayload    uriRequest;
                URIResponsePayload   uriResponse;
                URIResponsePayloadV2 uriResponseV2;
            };
        };

        DD_CHECK_SIZE(URIPayload, 260);
    }
}
