/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

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
} DDEventParserEventInfo;

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

#endif // ! DD_EVENT_PARSER_API_HEADER
