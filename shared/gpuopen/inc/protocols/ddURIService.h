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
* @file  ddURIService.h
* @brief Class declaration for URIService.
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"
#include "ddPlatform.h"
#include "protocols/systemProtocols.h"
#include "util/sharedptr.h"

namespace DevDriver
{
    namespace TransferProtocol
    {
        class LocalBlock;
    }

    namespace URIProtocol
    {
        // A struct that represents a unique URI request
        struct URIRequestContext
        {
            // Mutable arguments passed to the request
            char* pRequestArguments;

            // A local block to write the response data into.
            SharedPointer<TransferProtocol::LocalBlock> pResponseBlock;

            // The format of the data written into the response block.
            ResponseDataFormat responseDataFormat;
        };

        // Base class for URI services
        class URIService
        {
        public:
            virtual ~URIService() {}

            // Returns the name of the service
            const char* GetName() const { return m_name; }

#if !DD_VERSION_SUPPORTS(GPUOPEN_URI_RESPONSE_FORMATS_VERSION)
            // Attempts to handle a request from a client
            // Deprecated
            virtual Result HandleRequest(char*                                       pArguments,
                                         SharedPointer<TransferProtocol::LocalBlock> pBlock)
            {
                DD_NOT_IMPLEMENTED();
                return Result::Error;
            }

            // Attempts to handle a request from a client
            virtual Result HandleRequest(URIRequestContext* pContext)
            {
                DD_ASSERT(pContext != nullptr);

                const Result result = HandleRequest(pContext->pRequestArguments,
                                                    pContext->pResponseBlock);
                if (result == Result::Success)
                {
                    pContext->responseDataFormat = ResponseDataFormat::Text;
                }

                return result;
            }
#else
            // Attempts to handle a request from a client
            virtual Result HandleRequest(URIRequestContext* pContext) = 0;
#endif

        protected:
            URIService(const char* pName)
            {
                // Copy the service name into a member variable for later use.
                Platform::Strncpy(m_name, pName, sizeof(m_name));
            }

            // The name of the service
            char m_name[kURIStringSize];
        };
    }
} // DevDriver
