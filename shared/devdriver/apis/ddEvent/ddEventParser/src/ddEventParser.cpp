/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <ddEventParser.h>
#include <ddCommon.h>

#include <eventParser.h>

using namespace DevDriver;
using namespace Event;

/// Define DDEventParser as an alias for EventParser
DD_DEFINE_HANDLE(DDEventParser, EventParser*);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Helper function used to verify if an event writer contains all necessary fields
bool ValidateWriter(const DDEventWriter& writer)
{
    return ((writer.pfnBegin != nullptr)             &&
            (writer.pfnWritePayloadChunk != nullptr) &&
            (writer.pfnEnd != nullptr));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DDApiVersion ddEventParserQueryVersion()
{
    DDApiVersion version = {};

    version.major = DD_EVENT_PARSER_API_MAJOR_VERSION;
    version.minor = DD_EVENT_PARSER_API_MINOR_VERSION;
    version.patch = DD_EVENT_PARSER_API_PATCH_VERSION;

    return version;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* ddEventParserQueryVersionString()
{
    return DD_EVENT_PARSER_API_VERSION_STRING;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddEventParserCreate(
    const DDEventParserCreateInfo* pInfo,
    DDEventParser*                 phParser)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if ((pInfo != nullptr) && ValidateWriter(pInfo->writer) && (phParser != nullptr))
    {
        EventParser* pParser = DD_NEW(EventParser, Platform::GenericAllocCb)(pInfo->writer);
        if (pParser != nullptr)
        {
            *phParser = ToHandle(pParser);

            result = DD_RESULT_SUCCESS;
        }
        else
        {
            result = DD_RESULT_COMMON_OUT_OF_HEAP_MEMORY;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ddEventParserDestroy(
    DDEventParser hParser)
{
    if (hParser != nullptr)
    {
        EventParser* pParser = FromHandle(hParser);

        DD_DELETE(pParser, Platform::GenericAllocCb);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddEventParserParse(
    DDEventParser hParser,
    const void*   pData,
    size_t        dataSize)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if ((hParser != nullptr) && ValidateBuffer(pData, dataSize))
    {
        EventParser* pParser = FromHandle(hParser);

        result = pParser->Parse(pData, dataSize);
    }

    return result;
}

// =================================================================================================
DD_RESULT ddEventParserCreateEx(DDEventParser* phParser)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    EventParser* pParser = DD_NEW(EventParser, Platform::GenericAllocCb)();
    if (pParser != nullptr)
    {
        *phParser = ToHandle(pParser);
    }
    else
    {
        result = DD_RESULT_COMMON_OUT_OF_HEAP_MEMORY;
    }

    return result;
}

// =================================================================================================
DD_EVENT_PARSER_STATE ddEventParserParseNext(DDEventParser hParser)
{
    EventParser* pParser = FromHandle(hParser);
    return pParser->Parse();
}

// =================================================================================================
void ddEventParserSetBuffer(DDEventParser hParser, const void* pBuffer, size_t size)
{
    EventParser* pParser = FromHandle(hParser);
    pParser->SetParsingBuffer(pBuffer, size);
}

// =================================================================================================
DDEventParserEventInfo ddEventParserGetEventInfo(DDEventParser hParser)
{
    EventParser* pParser = FromHandle(hParser);
    return pParser->GetEventInfo();
}

// =================================================================================================
DDEventParserDataPayload ddEventParserGetDataPayload(DDEventParser hParser)
{
    EventParser* pParser = FromHandle(hParser);
    return pParser->GetPayload();
}
