/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef DD_API_DEFS_HEADER
#define DD_API_DEFS_HEADER

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

// Exporting logic relies on __attribute__((visibility("default"))) which isn't supported by all version of gcc/clang
#if defined __GNUC__ && __GNUC__ < 4
    #error "Unsupported version of __GNUC__!"
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Per compiler definitions to control library function import/export and calling convention.
/// NOTE: Symbols can be mangled if the calling convention isn't __cdecl!
#if !defined DD_API && defined DDLIB_EXPORTS
        #define DD_API __attribute__((visibility("default")))
#endif

#ifndef DD_API
    #define DD_API
#endif

/// Macro utility functions
#define _DD_API_STRINGIFY(str) #str
#define DD_API_STRINGIFY(x) _DD_API_STRINGIFY(x)
#define DD_API_STRINGIFY_VERSION(major, minor, patch)                                                                  \
    DD_API_STRINGIFY(major) "." DD_API_STRINGIFY(minor) "." DD_API_STRINGIFY(patch)

#define DD_API_UNUSED(x) ((void)(x))

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Result enum to signal whether an operation completes successfully, does not complete, or completes with errors.
/// A guiding principle with this enumeration is that eye-balling the numbers should be readable by a human without a
/// hex editor.
///
/// Related results are grouped to gether by name and value.
///     A section (e.g. "NET") is defined by
///         - DD_RESULT_NET__START which declares the lowest valued code. It should be a multiple of 100.
///         - DD_RESULT_NET__COUNT which declares the number of codes in the section
///     This range is reserved, but not everything in this range must be used immeditely. This is done to future-proof
///     results.
///     100 is a reasonable count if you're unsure of how many your section may need.
/// Sections must reserve their first slot for UNKNOWN, so that __START is always a safe result to return if the domain
/// is known but it's not clear which result to use.
/// This makes it easy to guage the domain of a result.
///     For example, if you know
///         DD_RESULT_NET__START = INT32_C(1200),
///         DD_RESULT_NET__COUNT = INT32_C(100),
///     And seee a result code of 1210, you can be assured it's network related because it's in the NET section, even
///     without looking at the list of codes.

// Utility macro to declare new result codes
//      - `id` must be an integer literal (e.g. "1")
//      - `code` must be a prefix for a section already declared in the enum (e.g. "DD_RESULT_NET")
// The sizeof() calls are included here to make sure each section is properly declared
#define DD_R_CODE(section_name, id) (                                                                                   \
        (void)sizeof(section_name ## __START),                                                                          \
        (void)sizeof(section_name ## __COUNT),                                                                          \
        (void)sizeof(section_name ## _UNKNOWN),                                                                         \
        (((int32_t)(section_name ## __START)) + INT32_C(id))                                                            \
    )

/// Utility macro to check whether a result is part of a specific section or not
#define DD_RESULT_IS_IN_SECTION(result, section) (                                                                      \
        ((result) >= section ## __START) &&                                                                             \
        ((result) < (section ## __START + section ## __COUNT))                                                          \
    )

/// Define DD_RESULT sections
///
/// This is a separate enum from DD_RESULT to avoid warnings about not matching these when switching on DD_RESULT
/// This is an enum and not #define so that it resolves in our helper macros
enum DD_RESULT_SECTIONS
{
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Results common across components
    //
    // These results are expected to be used by most components, and exist so that components don't need to redefine
    // common results. These may be expanded in the future if enough components use a specific result.

    // Results that are expected to be used by many sections, or results too specific to warrant a dedicated section
    DD_RESULT_COMMON__START                                                     = INT32_C(10),
    DD_RESULT_COMMON__COUNT                                                     = INT32_C(990),

    // Results related to parsing data from bytes, strings, or structured data like Json and MsgPack
    DD_RESULT_PARSING__START                                                    = INT32_C(1000),
    DD_RESULT_PARSING__COUNT                                                    = INT32_C(100),

    // Results related to filesystem operations
    DD_RESULT_FS__START                                                         = INT32_C(1100),
    DD_RESULT_FS__COUNT                                                         = INT32_C(100),

    // Results related to networking (or anything that presents as a network)
    DD_RESULT_NET__START                                                        = INT32_C(1200),
    DD_RESULT_NET__COUNT                                                        = INT32_C(100),

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Component specific result codes
    //
    // These sections begin at result code 10_000

    // These sections exist for a specific component and may change more quickly than the above.
    // These sections begin at result code 10_000 to make it easier to identify them.

    // DevDriver
    DD_RESULT_DD__START                                                         = INT32_C(10000),
    DD_RESULT_DD__COUNT                                                         = INT32_C(100),

    // DevDriver section to match generic DevDriver::Result values from C++
    DD_RESULT_DD_GENERIC__START                                                 = INT32_C(10100),
    DD_RESULT_DD_GENERIC__COUNT                                                 = INT32_C(100),

    // DevDriver section to match URI-focused DevDriver::Result values from C++
    DD_RESULT_DD_URI__START                                                     = INT32_C(10200),
    DD_RESULT_DD_URI__COUNT                                                     = INT32_C(100),

    DD_RESULT_DD_RPC__START                                                     = INT32_C(10300),
    DD_RESULT_DD_RPC__COUNT                                                     = INT32_C(100),

    DD_RESULT_DD_EVENT__START                                                   = INT32_C(10400),
    DD_RESULT_DD_EVENT__COUNT                                                   = INT32_C(100),

    DD_RESULT_SETTINGS_SERVICE__START                                           = INT32_C(10500),
    DD_RESULT_SETTINGS_SERVICE__COUNT                                           = INT32_C(100),
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Future sections!
    // ADDING_SECTIONS_0
    //
    // If you need to add a section, there are a few steps. Each step will walk through inserting new code.
    // Please insert the new code *before* the tutorial comment-blocks.
    //
    // 1. Pick a prefix. This prefix should be short and unique, and it can have '_' in the name.
    //      We'll use "TUTORIAL_AMD" as an example.
    //      See the existing sections for more examples.
    //
    // 2. Add your section start and count fields here. This reserves the range for your result codes.
    //      Make sure it does not overlap with other sections.
    //      __START should be a multiple of 1000 continuing where the last section left off.
    //      Here, well use 10300 for our __START because DD_RESULT_DD_URI__START is 10200, and 100 as a default.
    //
    //          DD_RESULT_TUTORIAL_AMD__START                                   = INT32_C(10300),
    //          DD_RESULT_TUTORIAL_AMD__COUNT                                   = INT32_C(100),
    //
    // 3. Add your variants below. See blow: (Ctrl+F ADDING_SECTIONS_3)
    // ...
};

typedef enum
{
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Special or common result codes

    /// An unknown error has occurred.
    ///
    /// Use this when absolutely nothing fits. This is set to 0 to catch accidentally zeroed memory.
    DD_RESULT_UNKNOWN                                                           = INT32_C(0),

    // Some compilers use special values to mark uninitialized or freed memory.
    //
    // Avoid using these values directly. They're here to help identify bugs when looking in a debugger.
    DD_RESULT_DEBUG_UNINIT_STACK_MEMORY                                         = (int32_t)UINT32_C(0xCCCCCCCC),
    DD_RESULT_DEBUG_UNINIT_HEAP_MEMORY                                          = (int32_t)UINT32_C(0xCDCDCDCD),
    DD_RESULT_DEBUG_FREED_HEAP_MEMORY                                           = (int32_t)UINT32_C(0xDDDDDDDD),

    /// The operation completed successfully
    DD_RESULT_SUCCESS                                                           = INT32_C(1),

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Common and miscellaneous errors

    DD_RESULT_COMMON_UNKNOWN                                                    = DD_RESULT_COMMON__START,

    /// The operation is not implemented yet
    DD_RESULT_COMMON_UNIMPLEMENTED                                              = DD_R_CODE(DD_RESULT_COMMON, 1),

    /// A parameter was invalid
    DD_RESULT_COMMON_INVALID_PARAMETER                                          = DD_R_CODE(DD_RESULT_COMMON, 2),

    /// Allocating heap memory failed
    DD_RESULT_COMMON_OUT_OF_HEAP_MEMORY                                         = DD_R_CODE(DD_RESULT_COMMON, 3),

    /// The operation requires more memory, but the caller owns the allocation
    DD_RESULT_COMMON_BUFFER_TOO_SMALL                                           = DD_R_CODE(DD_RESULT_COMMON, 4),

    /// The version is not compatible
    ///
    /// This usually happens when comparing a static version (from a header) with the runtime version of a component
    /// (like the version reported from a shared object/DLL)
    DD_RESULT_COMMON_VERSION_MISMATCH                                           = DD_R_CODE(DD_RESULT_COMMON, 5),

    /// An interface could not be loaded
    ///
    /// This often happens with plugin systems and loading symbols from shared objects/DLLs.
    DD_RESULT_COMMON_INTERFACE_NOT_FOUND                                        = DD_R_CODE(DD_RESULT_COMMON, 6),

    /// The creation of an entity was attempted, but it already exists
    ///
    /// This often happens when inserting things into a list, map, or cache.
    DD_RESULT_COMMON_ALREADY_EXISTS                                             = DD_R_CODE(DD_RESULT_COMMON, 7),

    /// An entity does not exist when it was expected to
    ///
    /// This often happens when querying things from a list, map, or cache.
    DD_RESULT_COMMON_DOES_NOT_EXIST                                             = DD_R_CODE(DD_RESULT_COMMON, 8),

    /// An entity's resource is limited and that limit has been reached
    ///
    /// This may happen when a cache fills up or hits a maximum memory useage.
    DD_RESULT_COMMON_LIMIT_REACHED                                              = DD_R_CODE(DD_RESULT_COMMON, 9),

    /// The operation is not supported
    DD_RESULT_COMMON_UNSUPPORTED                                                = DD_R_CODE(DD_RESULT_COMMON, 10),

    /// The full operation completed with some partial failures. For example, when deserializing data, some
    /// fields may no longer match the current format thus are skipped, but all other fields are deserialized
    /// correctly.
    DD_RESULT_COMMON_SUCCESS_WITH_ERRORS                                        = DD_R_CODE(DD_RESULT_COMMON, 11),

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Errors related to common parsing of data

    /// An unknown parsing error has occured
    DD_RESULT_PARSING_UNKNOWN                                                   = DD_RESULT_PARSING__START,

    /// The binary format was incorrect
    DD_RESULT_PARSING_INVALID_BYTES                                             = DD_R_CODE(DD_RESULT_PARSING, 1),

    /// The text-based format was incorrect
    DD_RESULT_PARSING_INVALID_STRING                                            = DD_R_CODE(DD_RESULT_PARSING, 2),

    /// Text couldn't be parsed into Json when it should
    DD_RESULT_PARSING_INVALID_JSON                                              = DD_R_CODE(DD_RESULT_PARSING, 3),

    /// Binary data couldn't be parsed into MsgPack when it should
    DD_RESULT_PARSING_INVALID_MSGPACK                                           = DD_R_CODE(DD_RESULT_PARSING, 4),

    /// Structured input (json, msgpack, etc) has an invalid structure
    ///
    /// This may happen if text parses as json, but the resulting json is missing an expected field.
    DD_RESULT_PARSING_INVALID_STRUCTURE                                         = DD_R_CODE(DD_RESULT_PARSING, 5),

    /// The operation reached an "end of file" earlier than expected, and cannot be completed
    DD_RESULT_PARSING_UNEXPECTED_EOF                                            = DD_R_CODE(DD_RESULT_PARSING, 6),

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Errors related to filesystem I/O

    /// Any I/O error not included in this section.
    DD_RESULT_FS_UNKNOWN                                                        = DD_RESULT_FS__START,

    /// A file or folder was not found
    ///
    /// Code that generates this should log the filename somewhere to aid debugging.
    DD_RESULT_FS_NOT_FOUND                                                      = DD_R_CODE(DD_RESULT_FS, 1),

    /// The operation lacked the necessary permission to complete
    DD_RESULT_FS_PERMISSION_DENIED                                              = DD_R_CODE(DD_RESULT_FS, 2),

    /// The operation failed because a pipe was closed
    DD_RESULT_FS_BROKEN_PIPE                                                    = DD_R_CODE(DD_RESULT_FS, 3),

    /// A file or folder was not found
    ///
    /// Code that generates this should log the filename somewhere to aid debugging.
    DD_RESULT_FS_ALREADY_EXISTS                                                 = DD_R_CODE(DD_RESULT_FS, 4),

    /// The operation needs to block to complete, but it was requested not to block
    DD_RESULT_FS_WOULD_BLOCK                                                    = DD_R_CODE(DD_RESULT_FS, 5),

    /// Some data required for the operation is not valid
    ///
    /// This differs from `DD_RESULT_ERROR_INVALID_PARAMETER`. Here, the parameters are valid but the data is malformed.
    /// e.g. a valid buffer is provided with bytes that do not parse correctly.
    DD_RESULT_FS_INVALID_DATA                                                   = DD_R_CODE(DD_RESULT_FS, 6),

    /// The I/O operation's timeout expired
    DD_RESULT_FS_TIMED_OUT                                                      = DD_R_CODE(DD_RESULT_FS, 7),

    /// This operation was interrupted
    ///
    /// Interrupted operations can typically be tried again.
    DD_RESULT_FS_INTERRUPTED                                                    = DD_R_CODE(DD_RESULT_FS, 8),

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Errors related to network I/O

    /// Any I/O error not included in this section.
    DD_RESULT_NET_UNKNOWN                                                       = DD_RESULT_NET__START,

    /// The connection already exists
    DD_RESULT_NET_CONNECTION_EXISTS                                             = DD_R_CODE(DD_RESULT_NET, 1),

    /// The connection was refused
    DD_RESULT_NET_CONNECTION_REFUSED                                            = DD_R_CODE(DD_RESULT_NET, 2),

    /// The connection was reset
    DD_RESULT_NET_CONNECTION_RESET                                              = DD_R_CODE(DD_RESULT_NET, 3),

    /// The connection was aborted
    DD_RESULT_NET_CONNECTION_ABORTED                                            = DD_R_CODE(DD_RESULT_NET, 4),

    /// The operation failed because there is no connection yet
    DD_RESULT_NET_NOT_CONNECTED                                                 = DD_R_CODE(DD_RESULT_NET, 5),

    /// A socket address could not be bound because the address is already in use
    DD_RESULT_NET_ADDR_IN_USE                                                   = DD_R_CODE(DD_RESULT_NET, 6),

    /// The requested address was not available
    DD_RESULT_NET_ADDR_NOT_AVAILABLE                                            = DD_R_CODE(DD_RESULT_NET, 7),

    /// The operation needs to block to complete, but it was requested not to block
    DD_RESULT_NET_WOULD_BLOCK                                                   = DD_R_CODE(DD_RESULT_NET, 8),

    /// The I/O operation's timeout expired
    DD_RESULT_NET_TIMED_OUT                                                     = DD_R_CODE(DD_RESULT_NET, 9),

    /// This operation was interrupted
    ///
    /// Interrupted operations can typically be tried again.
    DD_RESULT_NET_INTERRUPTED                                                   = DD_R_CODE(DD_RESULT_NET, 10),

    /// This operation was invoked on a socket type that doesn't support it
    DD_RESULT_NET_SOCKET_TYPE_UNSUPPORTED                                       = DD_R_CODE(DD_RESULT_NET, 11),

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Component specific result codes

    // Errors related to the DevDriver components

    /// An unknown DevDriver error
    DD_RESULT_DD_UNKNOWN                                                        = DD_RESULT_DD__START,

    /// Connection to the message bus is not available
    DD_RESULT_DD_BUS_UNAVAILABLE                                                = DD_R_CODE(DD_RESULT_DD, 1),

    /// The data context handle is invalid
    DD_RESULT_DD_INVALID_DATA_CONTEXT                                           = DD_R_CODE(DD_RESULT_DD, 2),

    /// The client context handle is invalid
    DD_RESULT_DD_INVALID_CLIENT_CONTEXT                                         = DD_R_CODE(DD_RESULT_DD, 3),

    /// The system context handle is invalid
    DD_RESULT_DD_INVALID_SYSTEM_CONTEXT                                         = DD_R_CODE(DD_RESULT_DD, 4),

    /// An operation requires a feature that was not enabled
    DD_RESULT_DD_API_FEATURE_NOT_ENABLED                                        = DD_R_CODE(DD_RESULT_DD, 6),

    // Results ported from DevDriver verbatim

    //// Generic Result Code  ////
    DD_RESULT_DD_GENERIC_UNKNOWN                                                = DD_RESULT_DD_GENERIC__START,
    DD_RESULT_DD_GENERIC_NOT_READY                                              = DD_R_CODE(DD_RESULT_DD_GENERIC, 2),
    DD_RESULT_DD_GENERIC_VERSION_MISMATCH                                       = DD_R_CODE(DD_RESULT_DD_GENERIC, 3),
    DD_RESULT_DD_GENERIC_UNAVAILABLE                                            = DD_R_CODE(DD_RESULT_DD_GENERIC, 4),
    DD_RESULT_DD_GENERIC_REJECTED                                               = DD_R_CODE(DD_RESULT_DD_GENERIC, 5),
    DD_RESULT_DD_GENERIC_END_OF_STREAM                                          = DD_R_CODE(DD_RESULT_DD_GENERIC, 6),
    DD_RESULT_DD_GENERIC_ABORTED                                                = DD_R_CODE(DD_RESULT_DD_GENERIC, 7),
    DD_RESULT_DD_GENERIC_INSUFFICIENT_MEMORY                                    = DD_R_CODE(DD_RESULT_DD_GENERIC, 8),
    DD_RESULT_DD_GENERIC_INVALID_PARAMETER                                      = DD_R_CODE(DD_RESULT_DD_GENERIC, 9),
    DD_RESULT_DD_GENERIC_INVALID_CLIENT_ID                                      = DD_R_CODE(DD_RESULT_DD_GENERIC, 10),
    DD_RESULT_DD_GENERIC_CONNECTION_EXITS                                       = DD_R_CODE(DD_RESULT_DD_GENERIC, 11),
    DD_RESULT_DD_GENERIC_FILE_NOT_FOUND                                         = DD_R_CODE(DD_RESULT_DD_GENERIC, 12),
    DD_RESULT_DD_GENERIC_FUNTION_NOT_FOUND                                      = DD_R_CODE(DD_RESULT_DD_GENERIC, 13),
    DD_RESULT_DD_GENERIC_INTERFACE_NOT_FOUND                                    = DD_R_CODE(DD_RESULT_DD_GENERIC, 14),
    DD_RESULT_DD_GENERIC_ENTRY_EXISTS                                           = DD_R_CODE(DD_RESULT_DD_GENERIC, 15),
    DD_RESULT_DD_GENERIC_FILE_ACCESS_ERROR                                      = DD_R_CODE(DD_RESULT_DD_GENERIC, 16),
    DD_RESULT_DD_GENERIC_FILE_IO_ERROR                                          = DD_R_CODE(DD_RESULT_DD_GENERIC, 17),
    DD_RESULT_DD_GENERIC_LIMIT_REACHED                                          = DD_R_CODE(DD_RESULT_DD_GENERIC, 18),

    //// URI PROTOCOL  ////
    DD_RESULT_DD_URI_UNKNOWN                                                    = DD_RESULT_DD_URI__START,
    DD_RESULT_DD_URI_SERVICE_REGISTRATION_ERROR                                 = DD_R_CODE(DD_RESULT_DD_URI, 1),
    DD_RESULT_DD_URI_STRING_PARSE_ERROR                                         = DD_R_CODE(DD_RESULT_DD_URI, 2),
    DD_RESULT_DD_URI_INVALID_PARAMETERs                                         = DD_R_CODE(DD_RESULT_DD_URI, 3),
    DD_RESULT_DD_URI_INVALID_POST_DATA_BLOCK                                    = DD_R_CODE(DD_RESULT_DD_URI, 4),
    DD_RESULT_DD_URI_INVALID_POST_DATA_SIZE                                     = DD_R_CODE(DD_RESULT_DD_URI, 5),
    DD_RESULT_DD_URI_FAILED_TO_ACQUIRE_POST_BLOCK                               = DD_R_CODE(DD_RESULT_DD_URI, 6),
    DD_RESULT_DD_URI_FAILED_TO_OPEN_RESPONSE_BLOCK                              = DD_R_CODE(DD_RESULT_DD_URI, 7),
    DD_RESULT_DD_URI_REQUEST_FAILED                                             = DD_R_CODE(DD_RESULT_DD_URI, 8),
    DD_RESULT_DD_URI_PENDING_REQUEST_ERROR                                      = DD_R_CODE(DD_RESULT_DD_URI, 9),
    DD_RESULT_DD_URI_INVALID_CHAR                                               = DD_R_CODE(DD_RESULT_DD_URI, 10),
    DD_RESULT_DD_URI_INVALID_JSON                                               = DD_R_CODE(DD_RESULT_DD_URI, 11),

    //// RPC PROTOCOL ////
    DD_RESULT_DD_RPC_UNKNOWN                                                    = DD_RESULT_DD_RPC__START,

    /// A request was made for a service that was not found on the remote server
    DD_RESULT_DD_RPC_SERVICE_NOT_REGISTERED                                     = DD_R_CODE(DD_RESULT_DD_RPC, 1),

    /// A request was made for a function that was not found within the target service
    DD_RESULT_DD_RPC_FUNC_NOT_REGISTERED                                        = DD_R_CODE(DD_RESULT_DD_RPC, 2),

    /// The provided parameter data was rejected by the server
    ///
    /// This can happen when the client sends the wrong parameter data, or none at all when some is expected.
    DD_RESULT_DD_RPC_FUNC_PARAM_REJECTED                                        = DD_R_CODE(DD_RESULT_DD_RPC, 3),

    /// The provided parameter data is larger than the server's size limit
    DD_RESULT_DD_RPC_FUNC_PARAM_TOO_LARGE                                       = DD_R_CODE(DD_RESULT_DD_RPC, 4),

    /// The response received from the server was rejected by the client
    ///
    /// This can happen when the server sends a response that its not supposed to, or it's not the one that the
    /// client expected. On 32-bit machines, this can also happen if the response size is larger than 4GB.
    DD_RESULT_DD_RPC_FUNC_RESPONSE_REJECTED                                     = DD_R_CODE(DD_RESULT_DD_RPC, 5),

    /// The client was expecting response data from a function but none was produced
    DD_RESULT_DD_RPC_FUNC_RESPONSE_MISSING                                      = DD_R_CODE(DD_RESULT_DD_RPC, 6),

    /// The client received a response packet from the server that isn't considered valid in the current sequence
    DD_RESULT_DD_RPC_CTRL_UNEXPECTED_RESPONSE_TYPE                              = DD_R_CODE(DD_RESULT_DD_RPC, 7),

    /// The server indicated that it sent a response with an invalid size
    DD_RESULT_DD_RPC_CTRL_INVALID_RESPONSE_SIZE                                 = DD_R_CODE(DD_RESULT_DD_RPC, 8),

    /// The server indicated that it sent response data with an invalid size
    DD_RESULT_DD_RPC_CTRL_INVALID_RESPONSE_DATA_SIZE                            = DD_R_CODE(DD_RESULT_DD_RPC, 9),

    /// The server indicated how much response data it would send, but then sent a different amount
    DD_RESULT_DD_RPC_CTRL_RESPONSE_SIZE_MISMATCH                                = DD_R_CODE(DD_RESULT_DD_RPC, 10),

    /// An RPC network control packet could not be read due to data corruption
    DD_RESULT_DD_RPC_CTRL_CORRUPTED_PACKET                                      = DD_R_CODE(DD_RESULT_DD_RPC, 11),

    /// The client did not expect return data or did not provide a response writer, but the call got response
    /// data anyway.
    DD_RESULT_DD_RPC_FUNC_UNEXPECTED_RETURN_DATA                                = DD_R_CODE(DD_RESULT_DD_RPC, 12),

    //// EVENT PROTOCOL ////
    DD_RESULT_DD_EVENT_UNKNOWN                                                  = DD_RESULT_DD_EVENT__START,

    /// An application attempted to emit an event on a provider that's currently disabled
    DD_RESULT_DD_EVENT_EMIT_PROVIDER_DISABLED                                   = DD_R_CODE(DD_RESULT_DD_EVENT, 1),

    /// An application attempted to emit an event on a provider with an invalid event id
    DD_RESULT_DD_EVENT_EMIT_INVALID_EVENT_ID                                    = DD_R_CODE(DD_RESULT_DD_EVENT, 2),

    /// An application attempted to emit an event that's currently disabled
    DD_RESULT_DD_EVENT_EMIT_EVENT_DISABLED                                      = DD_R_CODE(DD_RESULT_DD_EVENT, 3),

    /// Settings Service ///
    DD_RESULT_SETTINGS_SERVICE_UNKNOWN                                          = DD_RESULT_SETTINGS_SERVICE__START,
    DD_RESULT_SETTINGS_SERVICE_INVALID_NAME                                     = DD_R_CODE(DD_RESULT_SETTINGS_SERVICE, 1),
    DD_RESULT_SETTINGS_SERVICE_INVALID_COMPONENT                                = DD_R_CODE(DD_RESULT_SETTINGS_SERVICE, 2),
    DD_RESULT_SETTINGS_SERVICE_INVALID_SETTING_DATA                             = DD_R_CODE(DD_RESULT_SETTINGS_SERVICE, 3),

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // ADDING_SECTIONS_3
    //
    // 3. The next step for adding a section is to fill in your UNKNOWN field like so:
    //
    //          DD_RESULT_TUTORIAL_AMD_UNKNOWN                                  = DD_RESULT_TUTORIAL_AMD__START,
    //
    //      This ensures that you have an UNKNOWN field in your section, so reserved values that aren't allocated can
    //      be safely mapped to your section for generic errors.
    //
    // 4. Add each result code that you need. You do not need to use your entire range, and can add to it again, later.
    //      Each value should be declared using the DD_R_CODE() utility macro declared above, with an incrementing "id".that
    //      starts at 1 (0 is UNKNOWN, remember?).
    //      We'll add three example values here so you can get the idea.
    //
    //          DD_RESULT_TUTORIAL_AMD_COMPUTER_TURNED_OFF                      = DD_R_CODE(DD_RESULT_TUTORIAL_AMD, 1),
    //          DD_RESULT_TUTORIAL_AMD_DEVICE_FELL_OFF_THE_BUS                  = DD_R_CODE(DD_RESULT_TUTORIAL_AMD, 2),
    //          DD_RESULT_TUTORIAL_AMD_COLOR_IS_NOT_RED                         = DD_R_CODE(DD_RESULT_TUTORIAL_AMD, 3),
    //
    //      ... and so on.
    // 5. Next, we'll add these new results to the ddApiResultToString() function, so that we can get real string names
    //      at runtime.
    //      On some compilers, you may see build errors now.
    //      Open ddCommon.cpp and Ctrl+F for ADDING_SECTIONS_5
    //
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
} DD_RESULT;

static_assert(sizeof(DD_RESULT) == sizeof(int32_t), "DD_RESULT isn't the same size as int32_t");

/// Default network port number
#define DD_API_DEFAULT_NETWORK_PORT 27300

/// Constant value used to represent an invalid API handle
#define DD_API_INVALID_HANDLE 0

/// Constant value used to represent an invalid client id
#define DD_API_INVALID_CLIENT_ID 0

/// Constant value used to represent an invalid protocol id
#define DD_API_INVALID_PROTOCOL_ID 0

/// Constant value used to determine how many bytes we should allocate for strings that might contain a filesystem path
#define DD_API_PATH_SIZE 256

/// Declare a new, unique handle type.
///
/// These handles are opaque and you should not make assumptions about them.
/// They may or may not be pointers in the implementation, and this changing may not be reflected
/// in the API version.
///
/// They are pointers here for basic strong-type safety.
/// C++ will not implicitly convert between pointer types, so this provides us a simple and portable
/// strongly-typed typedef. C will convert but can warn about it. Rust is laughing in repr(transparent).
#define DD_DECLARE_HANDLE(name) typedef struct name ## _t* name;

/// Standardized versioning scheme for DD APIs
typedef struct DDApiVersion
{
    uint32_t major; /// Updated with API changes that are not backwards compatible
    uint32_t minor; /// Updated with API changes that are backwards compatible
    uint32_t patch; /// Updated with internal changes that are backwards compatible
} DDApiVersion;

/// Helper function that identifies if a version structure is considered valid or not
inline int ddIsVersionValid(
    DDApiVersion version)
{
    // We consider zero initialized version structures to be invalid
    return ((version.major != 0) ||
            (version.minor != 0) ||
            (version.patch != 0));
}

/// Helper function that uses semantic versioning to determine if actualVersion version meets the compatibility
/// requirements for requiredVersion
/// This logic is derived from the Semantic Versioning 2.0.0 specification
inline int ddIsVersionCompatible(
    DDApiVersion requiredVersion,
    DDApiVersion actualVersion)
{
    // In semantic versioning, if the major revision number is 0, then the API is considered to be in the
    // "initial development" state and any change may break API compatibility at any time.
    // In this situation, we use the minor version as the major version instead since that's how the semantic
    // versioning FAQ says libraries should be versioning themselves for the initial development period.

    // Make sure we reject invalid version structures
    const uint32_t isRequiredVersionValid = ddIsVersionValid(requiredVersion);

    const uint32_t requiredMajorVersion = ((requiredVersion.major != 0) ? requiredVersion.major
                                                                        : requiredVersion.minor);

    const uint32_t isActualVersionValid = ddIsVersionValid(actualVersion);

    const uint32_t actualMajorVersion = ((actualVersion.major != 0) ? actualVersion.major
                                                                    : actualVersion.minor);

    return ((isRequiredVersionValid && isActualVersionValid) &&
            (requiredMajorVersion   == actualMajorVersion)   &&
            (requiredVersion.minor  <= actualVersion.minor)  &&
            (requiredVersion.patch  <= actualVersion.patch));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Unique ID assigned when a client connects to the developer mode message bus
typedef uint16_t DDClientId;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Operating System Process ID
typedef uint32_t DDProcessId;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Opaque handle that represents a developer driver network connection
DD_DECLARE_HANDLE(DDNetConnection);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Opaque handle to a developer driver remote procedure call server
DD_DECLARE_HANDLE(DDRpcServer);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Opaque handle to an event server
DD_DECLARE_HANDLE(DDEventServer);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Value that uniquely identifies an individual network protocol
typedef uint8_t DDProtocolId;

/// State of a connected driver
typedef enum
{
    DD_DRIVER_STATE_UNKNOWN        = 0, /// Default value
    DD_DRIVER_STATE_PLATFORMINIT   = 1, /// Paused at internal driver init
    DD_DRIVER_STATE_DEVICEINIT     = 2, /// Paused before API device Init
    DD_DRIVER_STATE_POSTDEVICEINIT = 3, /// Paused after API device Init
    DD_DRIVER_STATE_RUNNING        = 4, /// Running and executing GPU work
    DD_DRIVER_STATE_PAUSED         = 5, /// Not running, but fully initialized
    DD_DRIVER_STATE_DISCONNECTED   = 6, /// Driver has disconnected from the network

    DD_DRIVER_STATE_COUNT               /// Number of driver states
} DD_DRIVER_STATE;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Memory allocation/free callback function prototypes
typedef void*(*PFN_ddAllocCallback)(void* pUserdata, size_t size, size_t alignment, int zero);
typedef void(*PFN_ddFreeCallback)(void* pUserdata, void* pMemory);
typedef struct DDAllocCallbacks
{
    PFN_ddAllocCallback pfnAlloc;
    PFN_ddFreeCallback  pfnFree;
    void*               pUserdata;
} DDAllocCallbacks;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Log function callback data types and prototype
typedef enum
{
    DD_LOG_LEVEL_DEBUG   = 0,   /// Potentially extremely high frequency info to aid debugging
    DD_LOG_LEVEL_VERBOSE = 1,   /// High frequency information of interest
    DD_LOG_LEVEL_INFO    = 2,   /// Low frequency information of interest
    DD_LOG_LEVEL_WARN    = 3,   /// Unexpected or important behavior
    DD_LOG_LEVEL_ERROR   = 4,   /// Unexpected or incorrect behavior
    DD_LOG_LEVEL_ALWAYS  = 5,   /// Log unconditionally
    DD_LOG_LEVEL_COUNT   = 6,   /// Number of supported log levels
    DD_LOG_LEVEL_NEVER   = 0xFF /// Never logged
} DD_LOG_LEVEL;

// Encapsulates metadata about a single log event
// String fields here are valid only for the duration of the callback.
// Some fields may be optional and only available in some builds. These fields should be checked before useage.
// Absent fields are initialized to 0 or NULL.
typedef struct DDLogEvent
{
    const char*  pCategory;  /// A string that categorizes the part of the system where the event occured.

    const char*  pFilename;  /// [optional] The filename where the event was created
    const char*  pFunction;  /// [optional] The function where the event was created
    uint32_t     lineNumber; /// [optional] The line number where the event was created

    DD_LOG_LEVEL level;      /// The severity of the event logged
} DDLogEvent;

/// Queries whether the current log level will log.
/// This can be used to skip looping over datastructures when it is known up-front that nothing will be logged.
/// Reasons an event might not be logged include:
///     There are filters on the log level
///     There are filters on the category of logged events
///     There are filters on the source-code location of logged events
///     etc.
/// Returns non-zero to indicate that this log level will produce output
typedef int32_t (*PFN_ddLoggerWillLog)(
    void*             pUserdata, /// Userdata
    const DDLogEvent* pEvent     /// An event
);

/// Log an event
typedef void (*PFN_ddLoggerLog)(
    void*             pUserdata, /// Userdata
    const DDLogEvent* pEvent,    /// Event to be logged
    const char*       pMessage   /// Message to be logged alongside the event
);

/// Increment the log stack
///
/// Create a new log-scope, possibly with an associated event that should be logged atomically with the scope creation
/// A minimal implementation should NULL-check pEvent and call PFN_ddLoggerLog
typedef void (*PFN_ddLoggerPush)(
    void*             pUserdata, /// Userdata
    const DDLogEvent* pEvent,    /// [optional] An event to pair with the level push
    const char*       pMessage   /// [optional] Pair a message with the event
);

/// Decrement the log stack
///
/// End the top-most log-scope, possibly with an associated event that should be logged atomically with the scope ending
/// A minimal implementation should NULL-check pEvent and call PFN_ddLoggerLog
typedef void (*PFN_ddLoggerPop)(
    void*             pUserdata,
    const DDLogEvent* pEvent,    /// [optional] An event to pair with the level pop
    const char*       pMessage   /// [optional] Pair a message with the event
);

/// Logging functions necessary for the Apis
typedef struct DDLoggerInfo
{
    void*               pUserdata;
    PFN_ddLoggerWillLog pfnWillLog;
    PFN_ddLoggerLog     pfnLog;
    PFN_ddLoggerPush    pfnPush;
    PFN_ddLoggerPop     pfnPop;
} DDLoggerInfo;

/// Function that ddApis may use to report text back to a caller
///
/// This is especially useful when the text is expensive to query, so it can be queried once and handed off to the caller.
typedef void (*PFN_ddReceiveText)(
    void*       pUserdata, /// [in] Userdata pointer
    const char* pText);    /// [in] Pointer to a null terminated character string

/// An interface that receives a text string
typedef struct DDTextReceiver
{
    PFN_ddReceiveText pfnReceive;
    void*             pUserdata;
} DDTextReceiver;

/// Function that ddApis may use to report binary data back to a caller
///
/// This is useful when the data would otherwise have to be copied into a temporary buffer if not for the callback.
typedef void (*PFN_ddReceiveBinary)(
    void*       pUserdata, /// [in] Userdata pointer
    const void* pData,     /// [in] Pointer to a buffer that contains data
    size_t      dataSize); /// Size of the data in bytes contained in the pData buffer

/// An interface that receives a binary buffer
typedef struct DDBinaryReceiver
{
    PFN_ddReceiveBinary pfnReceive;
    void*               pUserdata;
} DDBinaryReceiver;

/// Functions that ddApis may use to stream binary data back to a caller

/// Notifies the caller that binary data will soon begin streaming via the PFN_ddByteWriterWriteBytes callback
/// This callback will only be called once per stream
/// All sizes are measured in bytes
/// If this function returns non-success, the stream will be aborted
typedef DD_RESULT (*PFN_ddByteWriterBegin)(
    void*         pUserdata,       /// [in] Userdata pointer
    const size_t* pTotalDataSize); /// [Optional]
                                   /// [in] Total Size of the stream data in bytes
                                   //<      This parameter may be unavailable if the implementation doesn't know
                                   //<      the total amount of data that will be returned through the stream.

/// Notifies the caller that new binary data is available
/// This callback may be called many times per stream
/// All sizes are measured in bytes
/// If this function returns non-success, the stream will be aborted
typedef DD_RESULT (*PFN_ddByteWriterWriteBytes)(
    void*       pUserdata, /// [in] Userdata pointer
    const void* pData,     /// [in] Pointer to a buffer that contains data
    size_t      dataSize); /// Size of the data in bytes contained in the pData buffer

/// Notifies the user that all of the data in the binary stream has been received
/// This callback will only be called once per stream
/// The PFN_ddByteWriterWriteBytes callback will not be called again for the current stream
typedef void (*PFN_ddByteWriterEnd)(
    void*     pUserdata, /// [in] Userdata pointer
    DD_RESULT result);   /// The final result of the entire transfer

/// An interface that accepts a stream of bytes
typedef struct DDByteWriter
{
    PFN_ddByteWriterBegin      pfnBegin;
    PFN_ddByteWriterWriteBytes pfnWriteBytes;
    PFN_ddByteWriterEnd        pfnEnd;
    void*                      pUserdata;
} DDByteWriter;

/// Callback that ddApis may use to provide a heartbeat for i/o operations to a caller:

typedef enum
{
    DD_IO_STATUS_BEGIN,
    DD_IO_STATUS_END,
    DD_IO_STATUS_WRITE

} DD_IO_STATUS;

/// Notifies the caller of an update for an i/o operation
/// This callback may be called many times per stream
/// All sizes are measured in bytes
/// If this function returns non-success, the stream will be aborted
typedef DD_RESULT (*PFN_ddIOWriteHeartbeat)(
    void*            pUserdata, /// [in] Userdata pointer
    DD_RESULT        result,    /// [in] The result of the current operation
    DD_IO_STATUS     status,    /// [in] The current status of the writer
    size_t           bytes);    /// [in] Estimate of the bytes, the interpretation will depend on the status above:
                                /// When status = DD_FILEIO_STATUS_BEGIN, bytes is the estimate of total bytes to write.
                                ///               DD_FILEIO_STATUS_WRITE, bytes is the last amount written.
                                ///               DD_FILEIO_STATUS_END,   bytes is 0.

/// An interface for providing a heartbeat for io operations
typedef struct DDIOHeartbeat
{
    void*                      pUserdata;
    PFN_ddIOWriteHeartbeat     pfnWriteHeartbeat;
} DDIOHeartbeat;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Shared Module Interface

/// Opaque handle that represents a module
typedef struct DDModuleContext_t* DDModuleContext;

/// Opaque handle that represents a data context which is associated with an existing module
typedef struct DDModuleDataContext_t* DDModuleDataContext;

/// Opaque handle that represents a per system instance of an existing module
typedef struct DDModuleSystemContext_t* DDModuleSystemContext;

/// Opaque handle that represents a per client instance of an existing module
typedef struct DDModuleClientContext_t* DDModuleClientContext;

/// Opaque handle that represents a command extension context
typedef struct DDModuleCommandContext_t* DDModuleCommandContext;

/// Opaque handle that represents a connection context
typedef struct DDModuleConnectionContext_t* DDModuleConnectionContext;

/// Opaque handle that represents a module interface
typedef const struct DDModuleApi_t* DDModuleApi;

/// Opaque handle that represents a module extension interface
typedef const struct DDModuleExtensionApi_t* DDModuleExtensionApi;

/// Opaque id that identifies a module extension
typedef uint64_t DDModuleExtensionId;

/// Flags structure used to communicate extra information about a module
typedef union DDModuleFlags
{
    struct DDModuleFlagsFields
    {
        uint32_t supportsSystemContexts         : 1;  /// System Context Support
        uint32_t supportsClientContexts         : 1;  /// Client Context Support
        uint32_t supportsDataContexts           : 1;  /// Data Context Support
        uint32_t supportsConnectionContexts     : 1;  /// Connection Context Support
        uint32_t reserved                       : 28; /// Reserved. Set to zero
    } fields;
    uint32_t value;                           /// An aggregate view of all flags
} DDModuleFlags;

/// Structure that describes a module
typedef struct DDModuleDescription
{
    const char*   pName;
    const char*   pDescription;
    DDApiVersion  moduleVersion;
    DDModuleFlags flags;
} DDModuleDescription;

/// Structure that contains all necessary information for interacting with a module
typedef struct DDModuleInterface
{
    DDModuleDescription description;
    DDModuleApi         pApi;
    DDApiVersion        apiVersion;
} DDModuleInterface;

/// Structure that describes a module extension
typedef struct DDModuleExtensionDescription
{
    DDModuleExtensionId id;
    const char*         pName;
    const char*         pDescription;
} DDModuleExtensionDescription;

/// Structure that contains all necessary information for interacting with a module extension
typedef struct DDModuleExtensionInterface
{
    DDModuleExtensionDescription description;
    DDModuleExtensionApi         pApi;
    DDApiVersion                 apiVersion;
} DDModuleExtensionInterface;

/// Structure that describes a module that has been loaded
typedef struct DDModuleLoadedInfo
{
    DDModuleContext     hContext;    /// Context handle for the module
    DDModuleDescription description; /// Description of the module
    const char*         pPath;       /// [Optional]
                                     /// If this module was loaded dynamically, this is a null-terminated string that
                                     /// contains the path on the filesystem where the module was loaded from.
                                     /// If this module was loaded as a built-in, then this parameter is always null.
} DDModuleLoadedInfo;

/// Structure that contains information about module probe operation
typedef struct DDModuleProbeInfo
{
    const char*  pName;        /// Name of the module
    const char*  pDescription; /// Description of the module
    DDApiVersion version;      /// Version of the module
    uint32_t     isCompatible; /// Non-zero if this module is compatible with the loader
} DDModuleProbeInfo;

/// Opaque identifier for an RPC service
typedef uint32_t DDRpcServiceId;

/// Opaque identifier for an RPC function exposed by a service
typedef uint32_t DDRpcFunctionId;

/// Macro used to set up a QueryXApi export function
#define DD_DECLARE_QUERY_API_FUNC(ApiName) \
    typedef const DD ## ApiName ## Api* (*PFN_dd ## ApiName ## QueryApi)(void); \
    extern "C" DD_API const DD ## ApiName ## Api* Query ## ApiName ## Api(void)

/// Macro used to set up a QueryXApi export function
#define DD_DEFINE_QUERY_API_FUNC(ApiName) \
    extern "C" DD_API const DD ## ApiName ## Api* Query ## ApiName ## Api(void)

#ifdef __cplusplus
} // extern "C"
#endif

#endif
