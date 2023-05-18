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

#include "ddURIServer.h"
#include "msgChannel.h"
#include "util/ddMetroHash.h"
#include "ddTransferManager.h"
#include "ddUriInterface.h"
#include "protocols/ddURIProtocol.h"
#include "ddURIRequestContext.h"
#include "util/vector.h"

#define URI_SERVER_MIN_VERSION URI_INITIAL_VERSION
#define URI_SERVER_MAX_VERSION URI_POST_PROTOCOL_VERSION

namespace DevDriver
{
    namespace URIProtocol
    {
        using TransferProtocol::kInvalidBlockId;

        static constexpr ResponseDataFormat UriFormatToResponseFormat(URIDataFormat format)
        {
            static_assert(static_cast<uint32>(ResponseDataFormat::Unknown) == static_cast<uint32>(URIDataFormat::Unknown),
                          "ResponseDataFormat and URIDataFormat no longer match");
            static_assert(static_cast<uint32>(ResponseDataFormat::Text) == static_cast<uint32>(URIDataFormat::Text),
                          "ResponseDataFormat and URIDataFormat no longer match");
            static_assert(static_cast<uint32>(ResponseDataFormat::Binary) == static_cast<uint32>(URIDataFormat::Binary),
                          "ResponseDataFormat and URIDataFormat no longer match");
            static_assert(static_cast<uint32>(ResponseDataFormat::Count) == static_cast<uint32>(URIDataFormat::Count),
                          "ResponseDataFormat and URIDataFormat no longer match");
            return static_cast<ResponseDataFormat>(format);
        }

        // =====================================================================================================================
        // Parses out the parameters from a request string. (Ex. service://service-args)
        bool ExtractRequestParameters(char* pRequestString, char** ppServiceName, char** ppServiceArguments)
        {
            DD_ASSERT(pRequestString != nullptr);
            DD_ASSERT(ppServiceName != nullptr);
            DD_ASSERT(ppServiceArguments != nullptr);

            bool result = false;

            // Iterate through the null terminated string until we find the ":" character or the end of the string.
            char* pCurrentChar = strstr(pRequestString, "://");

            // If we haven't reached the end of the string then we've found the ":" character.
            // Otherwise this string isn't formatted correctly.
            if (pCurrentChar != nullptr)
            {
                // Overwrite the ":" character in memory with a null byte to allow us to divide up the request string
                // in place.
                *pCurrentChar = 0;

                // Return the service name and arguments out of the modified request string.
                *ppServiceName = pRequestString;
                *ppServiceArguments = pCurrentChar + 3;

                result = true;
            }

            return result;
        }

        using TransferManager = TransferProtocol::TransferManager;

        class URIServer::URISession
        {
        public:
            explicit URISession(URIServer* pServer,
                                TransferManager* pTransferManager,
                                const SharedPointer<ISession>& pSession)
                : m_pServer(pServer)
                , m_pTransferManager(pTransferManager)
                , m_pSession(pSession)
                , m_pResponseBlock()
                , m_payload()
                , m_hasQueuedPayload(false)
                , m_context()
                , m_pendingPostRequest()
            {
            }

            ~URISession()
            {
                // Release the session's server block before destroying it.
                if (!m_pResponseBlock.IsNull())
                {
                    m_pTransferManager->CloseServerBlock(m_pResponseBlock);
                }

                ClosePendingPostRequest();
            }

            // Helper functions for working with SizedPayloadContainers and managing back-compat.

            // ========================================================================================================
            Result SendPayload(const SizedPayloadContainer& payload, uint32 timeoutInMs)
            {
                // If we're running an older transfer version, always write the fixed container size.
                // Otherwise, write the real size.
                const uint32 payloadSize =
                    (m_pSession->GetVersion() >= URI_POST_PROTOCOL_VERSION) ? payload.payloadSize : kLegacyMaxSize;

                return m_pSession->Send(payloadSize, payload.payload, timeoutInMs);
            }

            // ========================================================================================================
            Result ReceivePayload(SizedPayloadContainer* pPayload, uint32 timeoutInMs)
            {
                DD_ASSERT(pPayload != nullptr);
                return m_pSession->Receive(sizeof(pPayload->payload), pPayload->payload, &pPayload->payloadSize, timeoutInMs);
            }

            // ========================================================================================================
            void Update()
            {
                // Attempt to send the session's queued payload if it has one.
                if (m_hasQueuedPayload)
                {
                    Result result = SendPayload(m_payload, kNoWait);
                    if (result == Result::Success)
                    {
                        // We successfully sent the payload. The session can now handle new requests.
                        m_hasQueuedPayload = false;
                    }
                }

                // We can only receive new messages if we don't currently have a queued payload.
                if (!m_hasQueuedPayload)
                {
                    // Receive and handle any new requests.
                    Result result = ReceivePayload(&m_payload, kNoWait);

                    if (result == Result::Success)
                    {
                        URIHeader& header = m_payload.GetPayload<URIHeader>();
                        if (header.command == URIMessage::URIPostRequest)
                        {
                            // It's important that these have nonnull defaults.
                            char pEmptyString[] = "";
                            char* pServiceName = pEmptyString;
                            char* pServiceArguments = pEmptyString;
                            URIPostRequestPayload& payload = m_payload.GetPayload<URIPostRequestPayload>();

                            result = ExtractRequestParameters(payload.uriString, &pServiceName, &pServiceArguments) ?
                                Result::Success : Result::UriStringParseError;

                            // Then query the service to see if this is a valid request
                            if (result == Result::Success)
                            {
                                result = m_pServer->ValidatePostRequest(pServiceName, pServiceArguments, payload.dataSize);
                            }

                            if (result == Result::Success)
                            {
                                auto pBlock = m_pTransferManager->OpenServerBlock();

                                if (pBlock.IsNull())
                                {
                                    result = Result::UriFailedToOpenResponseBlock;
                                }
                                else
                                {
                                    // We should never get to this point with a valid block still stored in m_pendingPostRequest
                                    DD_ASSERT(m_pendingPostRequest.pPostDataBlock.IsNull());
                                    m_pendingPostRequest.pPostDataBlock = pBlock;
                                    m_pendingPostRequest.requestedSize = payload.dataSize;
                                    // Assemble the response payload.
                                    m_payload.CreatePayload<URIPostResponsePayload>(result, m_pendingPostRequest.pPostDataBlock->GetBlockId());
                                }
                            }

                            if (result != Result::Success)
                            {
                                // If there's an error, just send back the result and Invalid block ID
                                m_payload.CreatePayload<URIPostResponsePayload>(result, kInvalidBlockId);
                            }
                        }
                        else if (header.command == URIMessage::URIRequest)
                        {
                            PostDataInfo postInfo;
                            URIRequestPayload& payload = m_payload.GetPayload<URIRequestPayload>();
                            // Declare the post data block here so the shared pointer will live through the handling of the
                            // request
                            SharedPointer<TransferProtocol::ServerBlock> postDataBlock;

                            // Older URI clients don't know about the post data fields, fill in default values
                            if (m_pSession->GetVersion() < URI_POST_PROTOCOL_VERSION)
                            {
                                payload.blockId = kInvalidBlockId;
                                payload.dataFormat = TransferDataFormat::Unknown;
                                payload.dataSize = 0;
                            }

                            // Attempt to extract the request string.
                            char* pServiceName = nullptr;
                            char* pServiceArguments = nullptr;
                            result = ExtractRequestParameters(payload.uriString, &pServiceName, &pServiceArguments) ?
                                Result::Success : Result::UriStringParseError;

                            // Setup the context to point at any post data provided with the request
                            if ((result == Result::Success) && (payload.dataSize > 0))
                            {
                                // An invalid BlockId indicates inline data
                                if (payload.blockId == kInvalidBlockId)
                                {
                                    // Make sure there is not a pending post request
                                    if (m_pendingPostRequest.pPostDataBlock.IsNull())
                                    {
                                        // Sanity check to make sure we don't read data past the end of the packet payload if
                                        // the client passes bogus data
                                        if (payload.dataSize <= kMaxInlineDataSize)
                                        {
                                            // Request data was sent inline in a single packet, setup the context to point at
                                            // the packet offset after the request payload struct
                                            postInfo.pData  = GetInlineDataPtr(&m_payload);
                                            postInfo.size   = payload.dataSize;
                                            postInfo.format = TransferFmtToURIDataFmt(payload.dataFormat);
                                        }
                                        else
                                        {
                                            // We should never have a dataSize > 0 and an invalid BlockId
                                            result = Result::UriInvalidParameters;
                                            DD_ASSERT_ALWAYS();
                                        }
                                    }
                                    else
                                    {
                                        // Another request came in while a post request was pending, return an error and clean up
                                        result = Result::UriPendingRequestError;
                                        ClosePendingPostRequest();
                                    }
                                }
                                else
                                {
                                    // If there's a post block associated with the request, we should have the same block
                                    // stored in m_pendingPostRequest
                                    if ((m_pendingPostRequest.pPostDataBlock.IsNull() == false) &&
                                        (m_pendingPostRequest.pPostDataBlock->GetBlockId() == payload.blockId) &&
                                        (m_pendingPostRequest.pPostDataBlock->GetBlockData() != nullptr) &&
                                        (payload.dataSize == m_pendingPostRequest.pPostDataBlock->GetBlockDataSize()))
                                    {
                                        // The request data was sent via the provided blockId, setup the context to point at the block.
                                        postInfo.pData  = m_pendingPostRequest.pPostDataBlock->GetBlockData();
                                        postInfo.size   = static_cast<uint32>(m_pendingPostRequest.pPostDataBlock->GetBlockDataSize());
                                        postInfo.format = TransferFmtToURIDataFmt(payload.dataFormat);
                                    }
                                    else
                                    {
                                        result = Result::UriInvalidPostDataBlock;
                                        ClosePendingPostRequest();
                                    }
                                }
                            }

                            if (result == Result::Success)
                            {
                                m_pResponseBlock = m_pTransferManager->OpenServerBlock();

                                if (m_pResponseBlock.IsNull())
                                {
                                    result = Result::UriFailedToOpenResponseBlock;
                                }
                                else
                                {
                                    // Handle the request using the appropriate service.
                                    m_context.Begin(pServiceArguments, URIDataFormat::Unknown, m_pResponseBlock, postInfo);

                                    // We've successfully extracted the request parameters.
                                    result = m_pServer->ServiceRequest(pServiceName, &m_context);

                                    m_context.End(result);

                                    // Close the post data block, if necessary
                                    ClosePendingPostRequest();

                                    // Close the response block.
                                    m_pResponseBlock->Close();
                                }
                            }

                            // Assemble the response payload.
                            if (result == Result::Success)
                            {
                                // Assemble the response payload.

                                // We send this data back regardless of protocol version, but it will only
                                // be read in a v2 or higher session.
                                const TransferDataFormat format =
                                    UriFormatToResponseFormat(m_context.GetUriDataFormat());
                                m_payload.CreatePayload<URIResponsePayload>(result,
                                                                            m_pResponseBlock->GetBlockId(),
                                                                            format);
                            }
                            else
                            {
                                // Failed to parse request parameters.

                                // Assemble the response payload.
                                m_payload.CreatePayload<URIResponsePayload>(result);
                            }
                        }
                        else
                        {
                            // We shouldn't expect any other commands on the server side, so just assert
                            DD_ASSERT_ALWAYS();
                        }

                        // Mark the session as having a queued payload if we fail to send the response.
                        if (SendPayload(m_payload, kNoWait) != Result::Success)
                        {
                            m_hasQueuedPayload = true;
                        }
                    }
                }
            }

        private:
            URIDataFormat TransferFmtToURIDataFmt(TransferDataFormat transferFormat)
            {
                // These enum deinitions are the same (for now) and both exist to keep the public and private
                // interfaces separate.  Do a compile time check to make sure there have been no changes then
                // just cast one to the other.
                static_assert((static_cast<uint32>(URIDataFormat::Unknown) == static_cast<uint32>(TransferDataFormat::Unknown)) &&
                              (static_cast<uint32>(URIDataFormat::Binary)  == static_cast<uint32>(TransferDataFormat::Binary)) &&
                              (static_cast<uint32>(URIDataFormat::Text)    == static_cast<uint32>(TransferDataFormat::Text)) &&
                              (static_cast<uint32>(URIDataFormat::Count)   == static_cast<uint32>(TransferDataFormat::Count)),
                              "TransferDataFormat doesn't match URIDataFormat");
                return static_cast<URIDataFormat>(transferFormat);
            }

            void ClosePendingPostRequest()
            {
                    m_pTransferManager->CloseServerBlock(m_pendingPostRequest.pPostDataBlock);
                m_pendingPostRequest.requestedSize = 0;
            }

            struct PostDataRequest
            {
                SharedPointer<TransferProtocol::ServerBlock>  pPostDataBlock;
                uint32                                        requestedSize;

                PostDataRequest() :
                    pPostDataBlock(),
                    requestedSize(0)
                {
                }
            };

            // There's some subtle alignment among these members. Be careful when re-arranging these that you don't
            // trigger "padding added" warnings(errors) on 32-bit builds.
            // SizedPayloadContainer m_payload must be 8-byte aligned, and there's a balance here to be had.
            URIServer*                                   m_pServer;
            TransferManager*                             m_pTransferManager;
            SharedPointer<ISession>                      m_pSession;
            SharedPointer<TransferProtocol::ServerBlock> m_pResponseBlock;
            SizedPayloadContainer                        m_payload;
            bool                                         m_hasQueuedPayload;
            URIRequestContext                            m_context;
            PostDataRequest m_pendingPostRequest;
        };

        // =====================================================================================================================
        URIServer::URIServer(IMsgChannel* pMsgChannel)
            : BaseProtocolServer(pMsgChannel, Protocol::URI, URI_SERVER_MIN_VERSION, URI_SERVER_MAX_VERSION)
            , m_registeredServices(pMsgChannel->GetAllocCb())
            , m_internalService()
        {
            DD_ASSERT(m_pMsgChannel != nullptr);

            InternalService::ServiceInfo info       = {};
            info.pUserdata                          = this;
            info.pfnQueryRegisteredServices         = URIServer::QueryRegisteredServices;

            // These are unlikely to ever fail. If they do, it's because of
            //      1) Programmer error on our part here
            //      2) Low memory conditions
            //      But the URIServer will still start. Just without an "internal" service.
            //
            // We can't fail in a constructor anyway, so mark these as unhandled.
            DD_UNHANDLED_RESULT(m_internalService.Init(info));
            DD_UNHANDLED_RESULT(RegisterService(&m_internalService));
        }

        // =====================================================================================================================
        URIServer::~URIServer()
        {
        }

        // =====================================================================================================================
        void URIServer::Finalize()
        {
            Platform::LockGuard<Platform::Mutex> lock(m_mutex);
            BaseProtocolServer::Finalize();
        }

        // =====================================================================================================================
        bool URIServer::AcceptSession(const SharedPointer<ISession>& pSession)
        {
            DD_UNUSED(pSession);
            return true;
        }

        // =====================================================================================================================
        void URIServer::SessionEstablished(const SharedPointer<ISession>& pSession)
        {
            DD_UNUSED(pSession);

            // Allocate a server block for use by the session.
            TransferManager* pTransferManager = &m_pMsgChannel->GetTransferManager();

            // Allocate session data for the newly established session
            URISession* pSessionData = DD_NEW(URISession, m_pMsgChannel->GetAllocCb())(this, pTransferManager, pSession);

            pSession->SetUserData(pSessionData);
        }

        // =====================================================================================================================
        void URIServer::UpdateSession(const SharedPointer<ISession>& pSession)
        {
            URISession* pSessionData = reinterpret_cast<URISession*>(pSession->GetUserData());
            pSessionData->Update();
        }

        // =====================================================================================================================
        void URIServer::SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason)
        {
            DD_UNUSED(terminationReason);
            URISession *pURISession = reinterpret_cast<URISession*>(pSession->SetUserData(nullptr));

            // Free the session data
            if (pURISession != nullptr)
            {
                DD_DELETE(pURISession, m_pMsgChannel->GetAllocCb());
            }
        }

        // =====================================================================================================================
        Result URIServer::RegisterService(IService* pService)
        {
            Result result = Result::InvalidParameter;
            if (pService != nullptr)
            {
                FixedString<kMaxUriServiceNameLength> serviceName(pService->GetName());

                const uint64 hash = MetroHash::HashCStr64(serviceName.AsCStr());

                ServiceInfo info = {};
                info.pService = pService;
                info.name = serviceName;
                info.version = pService->GetVersion();

                m_mutex.Lock();
                result = m_registeredServices.Create(hash, info);
                m_mutex.Unlock();
            }

            return result;
        }

        // =====================================================================================================================
        Result URIServer::UnregisterService(IService* pService)
        {
            Result result = Result::InvalidParameter;
            if (pService != nullptr)
            {
                FixedString<kMaxUriServiceNameLength> serviceName(pService->GetName());
                const uint64 hash = MetroHash::HashCStr64(serviceName.AsCStr());

                m_mutex.Lock();
                result = m_registeredServices.Erase(hash);
                m_mutex.Unlock();
            }

            return result;
        }

        // =====================================================================================================================
        // Finds a service pointer based on the provided name. It is the caller's responsibility to take the registered services
        // mutex before calling this function.
        IService* URIServer::FindService(const char* pServiceName)
        {
            IService* pService = nullptr;

            const uint64 hash = MetroHash::HashCStr64(pServiceName);
            auto iter = m_registeredServices.Find(hash);
            if (iter != m_registeredServices.End())
            {
                pService = iter->value.pService;
            }

            return pService;
        }

        // =====================================================================================================================
        Result URIServer::ServiceRequest(const char* pServiceName, IURIRequestContext* pRequestContext)
        {
            Result result = Result::Unavailable;

            // Lock the mutex
            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            IService* pService = FindService(pServiceName);

            // Check if the requested service was successfully located.
            if (pService != nullptr)
            {
                result = pService->HandleRequest(pRequestContext);
            }

            return result;
        }

        // =====================================================================================================================
        Result URIServer::ValidatePostRequest(const char* pServiceName, char* pRequestArguments, uint32 sizeRequested)
        {
            Result result = Result::Unavailable;

            if (pServiceName != nullptr)
            {
                // Lock the mutex and look up the requested service if it's available.
                m_mutex.Lock();

                IService* pService = FindService(pServiceName);

                // Check if the requested service was successfully located.
                if (pService != nullptr)
                {
                    if (pService->QueryPostSizeLimit(pRequestArguments) >= sizeRequested)
                    {
                        result = Result::Success;
                    }
                    else
                    {
                        result = Result::UriInvalidPostDataSize;
                    }
                }

                m_mutex.Unlock();
            }

            return result;
        }

        Result URIServer::QueryRegisteredServices(void* pUserdata, Vector<const IService*>* pServices)
        {
            Result result = Result::InvalidParameter;

            if ((pUserdata != nullptr) && (pServices != nullptr))
            {
                result = Result::Success;

                auto* pThis = reinterpret_cast<URIServer*>(pUserdata);

                for (const auto& entry : pThis->m_registeredServices)
                {
                    if (pServices->PushBack(entry.value.pService) == false)
                    {
                        result = Result::InsufficientMemory;

                        break;
                    }
                }
            }

            return result;
        }
    }
} // DevDriver
