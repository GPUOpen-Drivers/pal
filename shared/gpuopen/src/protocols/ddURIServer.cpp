/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "ddTransferManager.h"
#include "protocols/ddURIProtocol.h"

#define URI_SERVER_MIN_MAJOR_VERSION URI_INITIAL_VERSION
#define URI_SERVER_MAX_MAJOR_VERSION URI_RESPONSE_FORMATS_VERSION

namespace DevDriver
{
    namespace URIProtocol
    {
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

        struct URISession
        {
            Version version;
            SharedPointer<TransferProtocol::ServerBlock> pBlock;
            URIPayload payload;
            bool hasQueuedPayload;

            explicit URISession()
                : version(0)
                , hasQueuedPayload(false)
            {
                memset(&payload, 0, sizeof(payload));
            }
        };

        // =====================================================================================================================
        // Parses out the parameters from a request string. (Ex. service://service-args)
        bool ExtractRequestParameters(char* pRequestString, char** ppServiceName, char** ppServiceArguments)
        {
            DD_ASSERT(pRequestString != nullptr);
            DD_ASSERT(ppServiceName != nullptr);
            DD_ASSERT(ppServiceArguments != nullptr);

            bool result = false;

            // Iterate through the null terminated string until we find "://" or the end of the string.
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

        // =====================================================================================================================
        URIServer::URIServer(IMsgChannel* pMsgChannel)
            : BaseProtocolServer(pMsgChannel, Protocol::URI, URI_SERVER_MIN_MAJOR_VERSION, URI_SERVER_MAX_MAJOR_VERSION)
            , m_registeredServices(pMsgChannel->GetAllocCb())
            , m_registeredServiceNamesCache(pMsgChannel->GetAllocCb())
        {
            DD_ASSERT(m_pMsgChannel != nullptr);
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

            // Allocate session data for the newly established session
            URISession* pSessionData = DD_NEW(URISession, m_pMsgChannel->GetAllocCb())();

            // Allocate a server block for use by the session.
            pSessionData->pBlock = m_pMsgChannel->GetTransferManager().OpenServerBlock();

            pSession->SetUserData(pSessionData);
        }

        // =====================================================================================================================
        void URIServer::UpdateSession(const SharedPointer<ISession>& pSession)
        {
            URISession* pSessionData = reinterpret_cast<URISession*>(pSession->GetUserData());

            // Attempt to send the session's queued payload if it has one.
            if (pSessionData->hasQueuedPayload)
            {
                Result result = pSession->Send(sizeof(pSessionData->payload), &pSessionData->payload, kNoWait);
                if (result == Result::Success)
                {
                    // We successfully sent the payload. The session can now handle new requests.
                    pSessionData->hasQueuedPayload = false;
                }
            }

            // We can only receive new messages if we don't currently have a queued payload.
            if (!pSessionData->hasQueuedPayload)
            {
                // Receive and handle any new requests.
                uint32 bytesReceived = 0;
                Result result = pSession->Receive(sizeof(pSessionData->payload), &pSessionData->payload, &bytesReceived, kNoWait);

                if (result == Result::Success)
                {
                    // Make sure we receive a correctly sized payload.
                    DD_ASSERT(sizeof(pSessionData->payload) == bytesReceived);

                    // Make sure the payload is a uri request since it's the only payload type we should ever receive.
                    DD_ASSERT(pSessionData->payload.command == URIMessage::URIRequest);

                    // Reset the block associated with the session so we can write new data into it.
                    pSessionData->pBlock->Reset();

                    // Attempt to extract the request string.
                    char* pRequestString = pSessionData->payload.uriRequest.uriString;
                    char* pServiceName = nullptr;
                    char* pServiceArguments = nullptr;
                    result = ExtractRequestParameters(pRequestString, &pServiceName, &pServiceArguments) ? Result::Success : Result::Error;

                    if (result == Result::Success)
                    {
                            // Handle the request using the appropriate service.
                            URIRequestContext context = {};
                            context.pRequestArguments = pServiceArguments;
                            context.pResponseBlock = pSessionData->pBlock;
                            context.responseDataFormat = URIDataFormat::Unknown;

                            result = ServiceRequest(pServiceName, &context);

                            // Close the response block.
                            pSessionData->pBlock->Close();

                            // Assemble the response payload.
                            pSessionData->payload.command = URIMessage::URIResponse;
                            // Return the block id and associate the block with the session if we successfully handled the request.
                            pSessionData->payload.uriResponse.result = result;
                            pSessionData->payload.uriResponse.blockId = ((result == Result::Success) ? pSessionData->pBlock->GetBlockId()
                                                                         : TransferProtocol::kInvalidBlockId);
                            // We send this data back regardless of protocol version, but it will only be read
                            // in a v2 session.
                            pSessionData->payload.uriResponse.format = UriFormatToResponseFormat(context.responseDataFormat);

                            // Mark the session as having a queued payload if we fail to send the response.
                            if (pSession->Send(sizeof(pSessionData->payload), &pSessionData->payload, kNoWait) != Result::Success)
                            {
                                pSessionData->hasQueuedPayload = true;
                            }
                    }
                    else
                    {
                        // Failed to parse request parameters.

                        // Assemble the response payload.
                        pSessionData->payload.command = URIMessage::URIResponse;
                        pSessionData->payload.uriResponse.result = Result::Error;
                        pSessionData->payload.uriResponse.blockId = TransferProtocol::kInvalidBlockId;
                        pSessionData->payload.uriResponse.format = ResponseDataFormat::Unknown;

                        // Mark the session as having a queued payload if we fail to send the response.
                        if (pSession->Send(sizeof(pSessionData->payload), &pSessionData->payload, kNoWait) != Result::Success)
                        {
                            pSessionData->hasQueuedPayload = true;
                        }
                    }
                }
            }
        }

        // =====================================================================================================================
        void URIServer::SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason)
        {
            DD_UNUSED(terminationReason);
            URISession *pURISession = reinterpret_cast<URISession*>(pSession->SetUserData(nullptr));

            // Free the session data
            if (pURISession != nullptr)
            {
                // Release the session's server block before destroying it.
                if (!pURISession->pBlock.IsNull())
                {
                    m_pMsgChannel->GetTransferManager().CloseServerBlock(pURISession->pBlock);
                }

                DD_DELETE(pURISession, m_pMsgChannel->GetAllocCb());
            }
        }

        // =====================================================================================================================
        Result URIServer::RegisterService(IService* pService)
        {
            m_mutex.Lock();
            Result result = m_registeredServices.PushBack(pService) ? Result::Success : Result::Error;
            m_mutex.Unlock();

            if (result == Result::Success)
            {
                m_cacheMutex.Lock();
                result = m_registeredServiceNamesCache.PushBack(pService->GetName()) ? Result::Success : Result::Error;
                m_cacheMutex.Unlock();

                // To keep the cache consistent with the registered services list,
                // undo the first insert if the second one failed.
                // This may reorder the cache, and is why we cannot rely on there being a particular ordering.
                if (result != Result::Success)
                {
                    m_mutex.Lock();
                    result = m_registeredServices.Remove(pService) ? Result::Success : Result::Error;
                    m_mutex.Unlock();
                }
            }

            return result;
        }

        // =====================================================================================================================
        Result URIServer::UnregisterService(IService* pService)
        {
            m_mutex.Lock();
            Result result = m_registeredServices.Remove(pService) ? Result::Success : Result::Error;
            m_mutex.Unlock();

            if (result == Result::Success)
            {
                m_cacheMutex.Lock();
                FixedString<kMaxUriServiceNameLength>* pServiceName = DD_NEW(FixedString<kMaxUriServiceNameLength>, m_pMsgChannel->GetAllocCb())(pService->GetName());
                result = m_registeredServiceNamesCache.Remove(*pServiceName) ? Result::Success : Result::Error;
                DD_DELETE(pServiceName, m_pMsgChannel->GetAllocCb());
                m_cacheMutex.Unlock();

                // To keep the cache consistent with the registered services list,
                // undo the first removal if the second one failed.
                // This may reorder the cache, and is why we cannot rely on there being a particular ordering.
                if (result != Result::Success)
                {
                    m_mutex.Lock();
                    result = m_registeredServices.PushBack(pService) ? Result::Success : Result::Error;
                    m_mutex.Unlock();
                }
            }

            return result;
        }

        // =====================================================================================================================
        void URIServer::GetServiceNames(Vector<FixedString<kMaxUriServiceNameLength>>& serviceNames)
        {
            Platform::LockGuard<Platform::Mutex> lock(m_cacheMutex);

            for (const FixedString<kMaxUriServiceNameLength>& serviceName : m_registeredServiceNamesCache)
            {
                serviceNames.PushBack(serviceName);
            }
        }

        // =====================================================================================================================
        IService* URIServer::FindService(const char* pServiceName)
        {
            IService* pService = nullptr;

            for (auto& pSearchServer : m_registeredServices)
            {
                if (strcmp(pSearchServer->GetName(), pServiceName) == 0)
                {
                    pService = pSearchServer;
                    break;
                }
            }

            return pService;
        }

        // =====================================================================================================================
        Result URIServer::ServiceRequest(const char* pServiceName, URIRequestContext* pRequestContext)
        {
            Result result = Result::Unavailable;

            // Lock the mutex
            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            // Look up the requested service to see if it's available.
            IService* pService = FindService(pServiceName);

            // Check if the requested service was successfully located.
            if (pService != nullptr)
            {
                result = pService->HandleRequest(pRequestContext);
            }
            return result;
        }
    }
} // DevDriver
