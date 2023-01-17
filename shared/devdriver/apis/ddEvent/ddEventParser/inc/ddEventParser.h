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

#ifndef DD_EVENT_PARSER_HEADER
#define DD_EVENT_PARSER_HEADER

#include <ddEventParserApi.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Attempts to create a new parser object with the provided creation information
DD_RESULT ddEventParserCreate(
    /// Create info
    const DDEventParserCreateInfo* pInfo,
    /// Handle to the new parser object
    DDEventParser*                 phParser);

/// Destroys an existing parser object
void ddEventParserDestroy(
    /// Handle to the existing parser object
    DDEventParser hParser);

/// Parses the provided buffer of formatted event data
///
/// Returns parsed data through the DDEventWriter that was provided during the parser creation
DD_RESULT ddEventParserParse(
    /// Handle to the existing parser object
    DDEventParser hParser,
    /// Pointer to a buffer that contains formatted event data
    const void*   pData,
    /// Size of the buffer pointed to by pData
    size_t        dataSize);

/// Create a new parser.
DD_RESULT ddEventParserCreateEx(DDEventParser* phParser);

/// Set the buffer to be parsed.
void ddEventParserSetBuffer(DDEventParser hParser, const void* pBuffer, size_t size);

/// Parse the buffer. To parse a buffer, users should call this function
/// repeatedly and take actions based on the value it returns.
DD_EVENT_PARSER_STATE ddEventParserParseNext(DDEventParser hParser);

/// Get the info about the event received.
DDEventParserEventInfo ddEventParserGetEventInfo(DDEventParser hParser);

/// Get the info of the parsed data payload. Callers can use the returned info
/// to copy the payload data away. Note, the returned payload info might not be
/// complete. Callers can call `ddEventParserParseNext()` repeatedly to get
/// remaining payload.
DDEventParserDataPayload ddEventParserGetDataPayload(DDEventParser hParser);

#ifdef __cplusplus
} // extern "C"
#endif

#endif

