/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef DD_EVENT_PARSER_API_HEADER
#define DD_EVENT_PARSER_API_HEADER

#include <ddApi.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Compile time version information
#define DD_EVENT_PARSER_API_MAJOR_VERSION 0
#define DD_EVENT_PARSER_API_MINOR_VERSION 1
#define DD_EVENT_PARSER_API_PATCH_VERSION 0

#define DD_EVENT_PARSER_API_VERSION_STRING DD_API_STRINGIFY_VERSION(DD_EVENT_PARSER_API_MAJOR_VERSION, \
                                                                    DD_EVENT_PARSER_API_MINOR_VERSION, \
                                                                    DD_EVENT_PARSER_API_PATCH_VERSION)

/// This enum represents the state of a `DDEventParser`.
typedef enum
{
    /// Invalid state.
    DD_EVENT_PARSER_STATE_UNKNOWN,
    /// The parser just parsed an event.
    DD_EVENT_PARSER_STATE_EVENT_RECEIVED,
    /// The parser just parsed data payload.
    DD_EVENT_PARSER_STATE_PAYLOAD_RECEIVED,
    /// The parser needs more data to parse the next event/payload.
    DD_EVENT_PARSER_STATE_NEED_MORE_DATA,
} DD_EVENT_PARSER_STATE;

/// Opaque handle to an event parser
typedef struct DDEventParser_t* DDEventParser;

// Structure that contains information about the current event being handled by the parser
typedef struct DDEventParserEventInfo
{
    uint64_t timestampFrequency; /// Frequency of the timestamp associated with this event (ticks per second)
    uint64_t timestamp;          /// Timestamp recorded when this event was emitted by the provider
    uint32_t providerId;         /// Id of the event provider that emitted this event
    uint32_t eventId;            /// Id of the event within the provider
    uint32_t eventIndex;         /// Index of the event within the provider's event stream
                                 /// This can be used to verify that all events were correctly
                                 /// captured in the data stream.
    uint64_t totalPayloadSize;   /// The total size of the data payload belonging to this event.
} DDEventParserEventInfo;

typedef struct DDEventParserDataPayload
{
    /// Pointer to the data payload.
    const void* pData;
    /// The size of the data payload. This is the size of the payload currently
    /// parsed. This might not equal the total size.
    uint64_t size;
} DDEventParserDataPayload;

/// Notifies the caller that a new event has been encountered during parsing
///
/// This callback will only be called once per event
/// All sizes are measured in bytes
/// If this function returns non-success, parsing will be aborted and any remaining data will not be considered
typedef DD_RESULT (*PFN_ddEventWriterBegin)(
    void*                         pUserdata,         /// [in] Userdata pointer
    const DDEventParserEventInfo* pEvent,            /// [in] Pointer to information about the event
    uint64_t                      totalPayloadSize); /// Total size of the associated event payload
                                                     //< This will be 0 if the event has no associated data

/// Notifies the caller that a new chunk of the current event's associated payload data is available
///
/// This callback may be called many times per event depending on how the input data is provided to the parser.
/// It may also be skipped entirely for a given event if it has no associated payload data.
/// All sizes are measured in bytes
/// If this function returns non-success, parsing will be aborted. See EventWriterBegin for more information.
typedef DD_RESULT (*PFN_ddEventWriterWritePayloadChunk)(
    void*                         pUserdata, /// [in] Userdata pointer
    const DDEventParserEventInfo* pEvent,    /// [in] Pointer to information about the event
    const void*                   pData,     /// [in] Pointer to a buffer that contains data
    uint64_t                      dataSize); /// Size of the data in bytes contained in the pData buffer

/// Notifies the caller that all of the data for the current event has been parsed
/// This callback will only be called once per event
/// If this function returns non-success, parsing will be aborted. See EventWriterBegin for more information.
typedef DD_RESULT (*PFN_ddEventWriterEnd)(
    void*                         pUserdata, /// [in] Userdata pointer
    const DDEventParserEventInfo* pEvent,    /// [in] Pointer to information about the event
    DD_RESULT                     result);   /// The final result of the current event's parsing operation

/// An interface that provides all the data associated with an individual event
///
/// The application is guaranteed to receive both a "begin" and an "end" call for every parsed event
typedef struct DDEventWriter
{
    PFN_ddEventWriterBegin             pfnBegin;
    PFN_ddEventWriterWritePayloadChunk pfnWritePayloadChunk;
    PFN_ddEventWriterEnd               pfnEnd;
    void*                              pUserdata;
} DDEventWriter;

/// Structure that contains the information required to create a parser
typedef struct DDEventParserCreateInfo
{
    DDEventWriter writer; /// Writer interface used to deliver event information the user
} DDEventParserCreateInfo;

/// Get version of the loaded library to check interface compatibility
typedef DDApiVersion (*PFN_ddEventParserQueryVersion)(
    void);

/// Get human-readable representation of the loaded library version
typedef const char* (*PFN_ddEventParserQueryVersionString)(
    void);

/// Attempts to create a new parser object with the provided creation information
typedef DD_RESULT(*PFN_ddEventParserCreate)(
    const DDEventParserCreateInfo* pInfo,     /// [in]  Create info
    DDEventParser*                 phParser); /// [out] Handle to the new parser object

/// Destroys an existing parser object
typedef void (*PFN_ddEventParserDestroy)(
    DDEventParser hParser); /// [in] Handle to the existing parser object

/// Parses the provided buffer of formatted event data
///
/// Returns parsed data through the DDEventWriter that was provided during the parser creation
typedef DD_RESULT (*PFN_ddEventParserParse)(
    DDEventParser hParser,   /// Handle to the existing parser object
    const void*   pData,     /// [in] Pointer to a buffer that contains formatted event data
    size_t        dataSize); /// Size of the buffer pointed to by pData

/// API structure
typedef struct DDEventParserApi
{
    PFN_ddEventParserQueryVersion       pfnQueryVersion;
    PFN_ddEventParserQueryVersionString pfnQueryVersionString;
    PFN_ddEventParserCreate             pfnCreateParser;
    PFN_ddEventParserDestroy            pfnDestroyParser;
    PFN_ddEventParserParse              pfnParse;
} DDEventParserApi;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
