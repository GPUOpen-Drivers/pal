/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

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
