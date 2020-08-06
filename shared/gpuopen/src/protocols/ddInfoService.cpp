/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file ddInfoService.cpp
* @brief Class definition for InfoService.
***********************************************************************************************************************
*/

#include "protocols/ddInfoService.h"
#include "msgChannel.h"
#include "ddTransferManager.h"
#include "util/hashMap.h"
#include "util/ddMetroHash.h"
#include "ddPlatform.h"

namespace DevDriver
{
namespace InfoURIService
{
// Service string constants.
static const char* kInfoServiceName = "info";

// String constants used within info responses.
static const char* kSourceVersionLabel = "version";
static const char* kSourceValueLabel   = "value";

// =====================================================================================================================
InfoService::InfoService(
    const AllocCb& allocCb)
    :
    DevDriver::IService(),
    m_registeredInfoSources(allocCb)
{
}

// =====================================================================================================================
InfoService::~InfoService()
{
}

// =====================================================================================================================
const char* InfoService::GetName() const
{
    return kInfoServiceName;
}

// =====================================================================================================================
// Handles info requests from the developer driver bus
Result InfoService::HandleRequest(
    IURIRequestContext* pContext)
{
    Result result = Result::UriInvalidParameters;

    // The delimiter used to split command and argument token strings.
    static const char* const pArgDelim = " ";

    // We can safely use a single strtok context here because HandleRequest can only be called on one thread at a time
    // (enforced by the URI server).
    char* pArgsToken = nullptr;
    const char* pCommandArg = Platform::Strtok(pContext->GetRequestArguments(), pArgDelim, &pArgsToken);

    if (pCommandArg != nullptr)
    {
        // Determine what type of command is being handled.
        if (strcmp(pCommandArg, "all") == 0)
        {
            // Retrieve all registered info sources.
            result = HandleGetAllInfoSources(pContext);
        }
        else if (strcmp(pCommandArg, "list") == 0)
        {
            // Retrieve a list of registered info sources.
            result = HandleGetInfoSourceList(pContext);
        }
        else if (strcmp(pCommandArg, "getInfo") == 0)
        {
            // Extract the first argument provided to the command. This is the name of the source to retrieve.
            const char* pSourceNameArg = Platform::Strtok(nullptr, pArgDelim, &pArgsToken);

            // Retrieve a single info source by using the source name.
            result = HandleGetInfoSourceByName(pContext, pSourceNameArg);
        }
    }

    return result;
}

// =====================================================================================================================
// Registers a new info source with the service.
Result InfoService::RegisterInfoSource(
    const InfoSource& sourceInfo)
{
    Result result = Result::Error;

    // Lock access to the registered sources map.
    Platform::LockGuard<Platform::Mutex> infoSourcesLock(m_infoSourceMutex);

    // Ensure that the incoming source name is a valid string.
    if (sourceInfo.name.AsCStr() != nullptr)
    {
        if (sourceInfo.pfnWriteCallback != nullptr)
        {
            // Verify that the source name doesn't already exist in the map of registered sources.
            const bool isAlreadyRegistered = m_registeredInfoSources.Contains(sourceInfo.name);
            DD_WARN(!isAlreadyRegistered);
            if (!isAlreadyRegistered)
            {
                // Insert the new info source parameters into the map of registered sources.
                result = m_registeredInfoSources.Insert(sourceInfo.name, sourceInfo);
                if (result != Result::Success)
                {
                    DD_WARN_ALWAYS();
                }
            }
            else
            {
                // An info source with the same was already registered.
                result = Result::InfoUriSourceAlreadyRegistered;
            }
        }
        else
        {
            // The provided info source write callback was invalid.
            result = Result::InfoUriSourceCallbackInvalid;
        }
    }
    else
    {
        // The provided source name was invalid.
        result = Result::InfoUriSourceNameInvalid;
    }

    return result;
}

// =====================================================================================================================
// Unregisters an info source from the service.
void InfoService::UnregisterInfoSource(
    const char* pName)
{
    // Lock access to the registered sources map.
    Platform::LockGuard<Platform::Mutex> infoSourcesLock(m_infoSourceMutex);

    DD_WARN(pName != nullptr);
    if (pName != nullptr)
    {
        // If the source is already registered, remove it.
        FixedString<kMaxInfoSourceNameStrLen> sourceName(pName);
        const auto iter = m_registeredInfoSources.Find(sourceName);
        if (iter != m_registeredInfoSources.End())
        {
            m_registeredInfoSources.Remove(iter);
        }
    }
}

// =====================================================================================================================
// Clears all currently registered info sources from the service.
void InfoService::ClearInfoSources()
{
    // Lock access to the registered sources map.
    Platform::LockGuard<Platform::Mutex> infoSourcesLock(m_infoSourceMutex);

    m_registeredInfoSources.Clear();
}

// =====================================================================================================================
Result InfoService::HandleGetAllInfoSources(
    IURIRequestContext* pContext)
{
    IStructuredWriter* pWriter = nullptr;
    Result result = pContext->BeginJsonResponse(&pWriter);
    if (result == Result::Success)
    {
        // Start the response as a map containing all info sources.
        pWriter->BeginMap();

        // Lock access to the registered sources map.
        Platform::LockGuard<Platform::Mutex> infoSourcesLock(m_infoSourceMutex);

        // Iterate over each registered info source and invoke the info writer callback.
        for (const auto& currentSourceIter : m_registeredInfoSources)
        {
            // Write the source's name as the key and the info source map as the value.
            pWriter->Key(currentSourceIter.value.name.AsCStr());

            // Write the info source map.
            WriteInfoSource(currentSourceIter.value, pWriter);
        }

        // End the map of info source responses.
        pWriter->EndMap();

        // End the response.
        result = pWriter->End();
    }

    return result;
}

// =====================================================================================================================
Result InfoService::HandleGetInfoSourceList(
    IURIRequestContext* pContext)
{
    Result result = Result::UriInvalidParameters;

    IStructuredWriter* pWriter = nullptr;
    result = pContext->BeginJsonResponse(&pWriter);
    if (result == Result::Success)
    {
        // Lock access to the registered info sources map.
        Platform::LockGuard<Platform::Mutex> infoSourcesLock(m_infoSourceMutex);

        pWriter->BeginList();

        for (const auto& currentInfoSource : m_registeredInfoSources)
        {
            // Write the name of each info source.
            pWriter->Value(currentInfoSource.value.name.AsCStr());
        }

        pWriter->EndList();

        result = pWriter->End();
    }

    return result;
}

// =====================================================================================================================
Result InfoService::HandleGetInfoSourceByName(
    IURIRequestContext* pContext,
    const char* pName)
{
    Result result = Result::InfoUriSourceNameInvalid;

    DD_WARN(pName != nullptr);
    if (pName != nullptr)
    {
        // Lock access to the registered info sources map.
        Platform::LockGuard<Platform::Mutex> infoSourcesLock(m_infoSourceMutex);

        // Has the source name already been added to the list of registered info sources?
        FixedString<kMaxInfoSourceNameStrLen> infoSourceName(pName);
        const auto infoSourceIter = m_registeredInfoSources.Find(infoSourceName);

        // If an info source with the given name has already been registered, write the info response.
        if (infoSourceIter != m_registeredInfoSources.End())
        {
            IStructuredWriter* pWriter = nullptr;
            result = pContext->BeginJsonResponse(&pWriter);
            if (result == Result::Success)
            {
                // Write the info.
                WriteInfoSource(infoSourceIter->value, pWriter);

                // Finish writing the response data.
                result = pWriter->End();
            }
        }
    }

    return result;
}

void InfoService::WriteInfoSource(
    const InfoSource& source,
    IStructuredWriter* pWriter)
{
    // Begin writing the info.
    pWriter->BeginMap();

    // Write the source version number.
    pWriter->KeyAndValue(kSourceVersionLabel, source.version);

    // Begin a "value" map where the info will be written.
    pWriter->KeyAndBeginMap(kSourceValueLabel);

    // Invoke the info source's callback to write the contents.
    source.pfnWriteCallback(pWriter, source.pUserdata);

    // End writing the info value.
    pWriter->EndMap();

    // End writing the info source.
    pWriter->EndMap();
}
} // InfoURIService
} // DevDriver
