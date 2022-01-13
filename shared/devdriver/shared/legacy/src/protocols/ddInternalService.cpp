/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <ddPlatform.h>
#include <protocols/ddInternalService.h>
#include <protocols/ddURIServer.h>

namespace DevDriver
{

size_t InternalService::kPostSizeLimit = 10 * 1024; // 10 KiB

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DevDriver::Result InternalService::HandleRequest(DevDriver::IURIRequestContext* pRequestContext)
{
    Result result = Result::Unavailable;

    // Safety note: Strtok handles nullptr by returning nullptr. We handle that below.
#if defined(DD_PLATFORM_WINDOWS_KM)
    char* pCmdName = pRequestContext->GetRequestArguments();
#else
    const char* const pArgDelim = " ";
    char* pStrtokContext = nullptr;
    char* pCmdName = Platform::Strtok(pRequestContext->GetRequestArguments(), pArgDelim, &pStrtokContext);
#endif

    if (pCmdName == nullptr)
    {
        // This happens when no command is given, and the request string looks like: "internal://".
        // Really, no command *is* a command... that we don't support.
        // We must explicitly handle this here, because it is undefined behavior to pass nullptr to strcmp.
        // We handle it by doing nothing and returning Unavailable.
    }
    else if (strcmp(pCmdName, "services") == 0)
    {
#if defined(DD_PLATFORM_WINDOWS_KM)
        // The kernel does not build with JSON support, so we will write a manual text response with JSON formatting
        // in this case
        result = WriteServicesTextResponse(pRequestContext);
#else
        result = WriteServicesJsonResponse(pRequestContext);
#endif
    }
#if !defined(DD_PLATFORM_WINDOWS_KM)
    else if (strcmp(pCmdName, "diag-echo") == 0)
    {
        IByteWriter* pWriter = nullptr;
        result = pRequestContext->BeginByteResponse(&pWriter);

        if (result == Result::Success)
        {
            // Keep track of whether we echoed the arguments yet
            bool echoedArgs = false;

            // If there are any arguments, echo them back one at a time. (Space delimited)
            {
                // Start parsing the space-delimited arguments
                const char* pArg = Platform::Strtok(nullptr, pArgDelim, &pStrtokContext);

                // Write the first one, if we have it
                if (pArg != nullptr)
                {
                    echoedArgs = true;
                    pWriter->WriteBytes(pArg, strlen(pArg));
                }

                while (true)
                {
                    // Fetch the next argument
                    pArg = Platform::Strtok(nullptr, pArgDelim, &pStrtokContext);

                    if (pArg != nullptr)
                    {
                        // And if there is a next argument, print it space-delimited
                        pWriter->Write(' ');
                        pWriter->WriteBytes(pArg, strlen(pArg));
                    }
                    else
                    {
                        // Otherwise, terminate
                        break;
                    }
                };
            }

            // If there is any post data, echo it back
            const PostDataInfo& postData = pRequestContext->GetPostData();
            if (postData.size != 0)
            {
                // Make a clear separation between the previous section and this one with a newline
                // But only if there is a previous secion.
                if (echoedArgs)
                {
                    pWriter->Write('\n');
                }

                // Then write our the post data as-is.
                pWriter->WriteBytes(postData.pData, postData.size);
            }

            // :)
            pWriter->Write('\0');

            result = pWriter->End();
        }
        else
        {
            DD_WARN_REASON("Failed to begin a ByteResponse for internal://diag-echo");
        }
    }
#endif
    else
    {
        // No other internal service commands are handled
        DD_NOT_IMPLEMENTED();
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
size_t InternalService::QueryPostSizeLimit(char* pArgs) const
{
    DD_ASSERT(pArgs != nullptr);

    size_t postSizeLimit = 0;

#if !defined(DD_PLATFORM_WINDOWS_KM)
    char* pStrtokContext = nullptr;
    char* pCmdName       = Platform::Strtok(pArgs, " ", &pStrtokContext);
    if (strcmp(pCmdName, "diag-echo") == 0)
    {
        postSizeLimit = kPostSizeLimit;
    }
#else
    DD_UNUSED(pArgs);
#endif

    return postSizeLimit;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DevDriver::Result InternalService::WriteServicesJsonResponse(DevDriver::IURIRequestContext* pRequestContext) const
{
    // This callback obtains a list of IService pointers from the URIServer and it holds onto them for the rest of
    // the function.
    // This is safe because:
    //      1) these pointers are only invalidated when services are added or removed
    //      2) no services are added or removed while executing a service's HandleRequest()
    Vector<const IService*> registeredServices(m_info.allocCb);
    DevDriver::Result result = m_info.pfnQueryRegisteredServices(m_info.pUserdata, &registeredServices);

    if (result == Result::Success)
    {
        IStructuredWriter* pWriter = nullptr;
        result = pRequestContext->BeginJsonResponse(&pWriter);

        if (result == Result::Success)
        {
            pWriter->BeginMap();
            pWriter->KeyAndBeginList("Services");
            for (const IService* pService : registeredServices)
            {
                pWriter->BeginMap();
                pWriter->KeyAndValue("Name", pService->GetName());
                pWriter->KeyAndValue("Version", pService->GetVersion());
                pWriter->EndMap();
            }

            pWriter->EndList();
            pWriter->EndMap();

            result = pWriter->End();
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DevDriver::Result InternalService::WriteServicesTextResponse(DevDriver::IURIRequestContext* pRequestContext) const
{
    // This callback obtains a list of IService pointers from the URIServer and it holds onto them for the rest of
    // the function.
    // This is safe because:
    //      1) these pointers are only invalidated when services are added or removed
    //      2) no services are added or removed while executing a service's HandleRequest()
    Vector<const IService*> registeredServices(m_info.allocCb);
    DevDriver::Result result = m_info.pfnQueryRegisteredServices(m_info.pUserdata, &registeredServices);

    if (result == Result::Success)
    {
        // The kernel does not build with JSON support, so we will write a manual text response with JSON formatting
        // in this case
        ITextWriter* pWriter = nullptr;
        result = pRequestContext->BeginTextResponse(&pWriter);

        if (result == Result::Success)
        {
            pWriter->Write("{ \"Services\": [ ");
            for (const IService* pService : registeredServices)
            {
                pWriter->Write("{ \"Name\": %s, \"Version\": %u }", pService->GetName(), pService->GetVersion());
            }

            pWriter->Write("] }");
            result = pWriter->End();
        }
    }
    return result;
}

} // namespace DevDriver
