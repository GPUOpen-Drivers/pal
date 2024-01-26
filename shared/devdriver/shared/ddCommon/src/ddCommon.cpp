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

#include <ddCommon.h>

using namespace DevDriver;

// Static asserts for logging enums
static_assert(static_cast<uint32>(LogLevel::Debug) == static_cast<uint32>(DD_LOG_LEVEL_DEBUG),
    "DevDriver and ddTool log levels don't match.");
static_assert(static_cast<uint32>(LogLevel::Verbose) == static_cast<uint32>(DD_LOG_LEVEL_VERBOSE),
    "DevDriver and ddTool log levels don't match.");
static_assert(static_cast<uint32>(LogLevel::Info) == static_cast<uint32>(DD_LOG_LEVEL_INFO),
    "DevDriver and ddTool log levels don't match.");
static_assert(static_cast<uint32>(LogLevel::Warn) == static_cast<uint32>(DD_LOG_LEVEL_WARN),
    "DevDriver and ddTool log levels don't match.");
static_assert(static_cast<uint32>(LogLevel::Error) == static_cast<uint32>(DD_LOG_LEVEL_ERROR),
    "DevDriver and ddTool log levels don't match.");
static_assert(static_cast<uint32>(LogLevel::Always) == static_cast<uint32>(DD_LOG_LEVEL_ALWAYS),
    "DevDriver and ddTool log levels don't match.");
static_assert(static_cast<uint32>(LogLevel::Count) == static_cast<uint32>(DD_LOG_LEVEL_COUNT),
    "DevDriver and ddTool log levels don't match.");
static_assert(static_cast<uint32>(LogLevel::Never) == static_cast<uint32>(DD_LOG_LEVEL_NEVER),
    "DevDriver and ddTool log levels don't match.");

// Utility macro to protect against copy-and-paste errors or typoes in ddApiResultToString's switch statement.
#define RESULT_TO_STRING_CASE(result) case result: return # result;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* ddApiResultToString(DD_RESULT result)
{
    switch (ddApiClampResult(result))
    {
        RESULT_TO_STRING_CASE(DD_RESULT_UNKNOWN);

        RESULT_TO_STRING_CASE(DD_RESULT_DEBUG_UNINIT_STACK_MEMORY);
        RESULT_TO_STRING_CASE(DD_RESULT_DEBUG_UNINIT_HEAP_MEMORY);
        RESULT_TO_STRING_CASE(DD_RESULT_DEBUG_FREED_HEAP_MEMORY);

        RESULT_TO_STRING_CASE(DD_RESULT_SUCCESS);

        RESULT_TO_STRING_CASE(DD_RESULT_COMMON_UNKNOWN);
        RESULT_TO_STRING_CASE(DD_RESULT_COMMON_UNIMPLEMENTED);
        RESULT_TO_STRING_CASE(DD_RESULT_COMMON_INVALID_PARAMETER);
        RESULT_TO_STRING_CASE(DD_RESULT_COMMON_OUT_OF_HEAP_MEMORY);
        RESULT_TO_STRING_CASE(DD_RESULT_COMMON_BUFFER_TOO_SMALL);
        RESULT_TO_STRING_CASE(DD_RESULT_COMMON_VERSION_MISMATCH);
        RESULT_TO_STRING_CASE(DD_RESULT_COMMON_INTERFACE_NOT_FOUND);
        RESULT_TO_STRING_CASE(DD_RESULT_COMMON_ALREADY_EXISTS);
        RESULT_TO_STRING_CASE(DD_RESULT_COMMON_DOES_NOT_EXIST);
        RESULT_TO_STRING_CASE(DD_RESULT_COMMON_LIMIT_REACHED);
        RESULT_TO_STRING_CASE(DD_RESULT_COMMON_UNSUPPORTED);
        RESULT_TO_STRING_CASE(DD_RESULT_COMMON_SUCCESS_WITH_ERRORS);

        RESULT_TO_STRING_CASE(DD_RESULT_PARSING_UNKNOWN);
        RESULT_TO_STRING_CASE(DD_RESULT_PARSING_INVALID_BYTES);
        RESULT_TO_STRING_CASE(DD_RESULT_PARSING_INVALID_STRING);
        RESULT_TO_STRING_CASE(DD_RESULT_PARSING_INVALID_JSON);
        RESULT_TO_STRING_CASE(DD_RESULT_PARSING_INVALID_MSGPACK);
        RESULT_TO_STRING_CASE(DD_RESULT_PARSING_INVALID_STRUCTURE);
        RESULT_TO_STRING_CASE(DD_RESULT_PARSING_UNEXPECTED_EOF);

        RESULT_TO_STRING_CASE(DD_RESULT_FS_UNKNOWN);
        RESULT_TO_STRING_CASE(DD_RESULT_FS_NOT_FOUND);
        RESULT_TO_STRING_CASE(DD_RESULT_FS_PERMISSION_DENIED);
        RESULT_TO_STRING_CASE(DD_RESULT_FS_BROKEN_PIPE);
        RESULT_TO_STRING_CASE(DD_RESULT_FS_ALREADY_EXISTS);
        RESULT_TO_STRING_CASE(DD_RESULT_FS_WOULD_BLOCK);
        RESULT_TO_STRING_CASE(DD_RESULT_FS_INVALID_DATA);
        RESULT_TO_STRING_CASE(DD_RESULT_FS_TIMED_OUT);
        RESULT_TO_STRING_CASE(DD_RESULT_FS_INTERRUPTED);

        RESULT_TO_STRING_CASE(DD_RESULT_NET_UNKNOWN);
        RESULT_TO_STRING_CASE(DD_RESULT_NET_CONNECTION_EXISTS);
        RESULT_TO_STRING_CASE(DD_RESULT_NET_CONNECTION_REFUSED);
        RESULT_TO_STRING_CASE(DD_RESULT_NET_CONNECTION_RESET);
        RESULT_TO_STRING_CASE(DD_RESULT_NET_CONNECTION_ABORTED);
        RESULT_TO_STRING_CASE(DD_RESULT_NET_NOT_CONNECTED);
        RESULT_TO_STRING_CASE(DD_RESULT_NET_ADDR_IN_USE);
        RESULT_TO_STRING_CASE(DD_RESULT_NET_ADDR_NOT_AVAILABLE);
        RESULT_TO_STRING_CASE(DD_RESULT_NET_WOULD_BLOCK);
        RESULT_TO_STRING_CASE(DD_RESULT_NET_TIMED_OUT);
        RESULT_TO_STRING_CASE(DD_RESULT_NET_INTERRUPTED);
        RESULT_TO_STRING_CASE(DD_RESULT_NET_SOCKET_TYPE_UNSUPPORTED);

        RESULT_TO_STRING_CASE(DD_RESULT_DD_UNKNOWN);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_BUS_UNAVAILABLE);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_INVALID_DATA_CONTEXT);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_INVALID_CLIENT_CONTEXT);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_INVALID_SYSTEM_CONTEXT);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_API_FEATURE_NOT_ENABLED);

        RESULT_TO_STRING_CASE(DD_RESULT_DD_GENERIC_UNKNOWN);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_GENERIC_NOT_READY);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_GENERIC_VERSION_MISMATCH);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_GENERIC_UNAVAILABLE);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_GENERIC_REJECTED);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_GENERIC_END_OF_STREAM);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_GENERIC_ABORTED);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_GENERIC_INSUFFICIENT_MEMORY);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_GENERIC_INVALID_PARAMETER);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_GENERIC_INVALID_CLIENT_ID);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_GENERIC_CONNECTION_EXITS);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_GENERIC_FILE_NOT_FOUND);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_GENERIC_FUNTION_NOT_FOUND);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_GENERIC_INTERFACE_NOT_FOUND);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_GENERIC_ENTRY_EXISTS);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_GENERIC_FILE_ACCESS_ERROR);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_GENERIC_FILE_IO_ERROR);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_GENERIC_LIMIT_REACHED);

        RESULT_TO_STRING_CASE(DD_RESULT_DD_URI_UNKNOWN);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_URI_SERVICE_REGISTRATION_ERROR);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_URI_STRING_PARSE_ERROR);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_URI_INVALID_PARAMETERs);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_URI_INVALID_POST_DATA_BLOCK);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_URI_INVALID_POST_DATA_SIZE);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_URI_FAILED_TO_ACQUIRE_POST_BLOCK);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_URI_FAILED_TO_OPEN_RESPONSE_BLOCK);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_URI_REQUEST_FAILED);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_URI_PENDING_REQUEST_ERROR);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_URI_INVALID_CHAR);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_URI_INVALID_JSON);

        RESULT_TO_STRING_CASE(DD_RESULT_DD_RPC_UNKNOWN);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_RPC_SERVICE_NOT_REGISTERED);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_RPC_FUNC_NOT_REGISTERED);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_RPC_FUNC_PARAM_REJECTED);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_RPC_FUNC_PARAM_TOO_LARGE);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_RPC_FUNC_RESPONSE_REJECTED);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_RPC_FUNC_RESPONSE_MISSING);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_RPC_CTRL_UNEXPECTED_RESPONSE_TYPE);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_RPC_CTRL_INVALID_RESPONSE_SIZE);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_RPC_CTRL_INVALID_RESPONSE_DATA_SIZE);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_RPC_CTRL_RESPONSE_SIZE_MISMATCH);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_RPC_CTRL_CORRUPTED_PACKET);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_RPC_FUNC_UNEXPECTED_RETURN_DATA);

        RESULT_TO_STRING_CASE(DD_RESULT_DD_EVENT_UNKNOWN);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_EVENT_EMIT_PROVIDER_DISABLED);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_EVENT_EMIT_INVALID_EVENT_ID);
        RESULT_TO_STRING_CASE(DD_RESULT_DD_EVENT_EMIT_EVENT_DISABLED);

        RESULT_TO_STRING_CASE(DD_RESULT_SETTINGS_SERVICE_UNKNOWN);
        RESULT_TO_STRING_CASE(DD_RESULT_SETTINGS_SERVICE_INVALID_NAME);
        RESULT_TO_STRING_CASE(DD_RESULT_SETTINGS_SERVICE_INVALID_COMPONENT);
        RESULT_TO_STRING_CASE(DD_RESULT_SETTINGS_SERVICE_INVALID_SETTING_DATA);

        RESULT_TO_STRING_CASE(DD_RESULT_SETTINGS_UNKNOWN);
        RESULT_TO_STRING_CASE(DD_RESULT_SETTINGS_NOT_FOUND);
        RESULT_TO_STRING_CASE(DD_RESULT_SETTINGS_TYPE_MISMATCH);

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // ADDING_SECTIONS_7
        //
        // 7. Add all of the new variants of your section to this list, inside of the helper macro.
        //      Make sure you include your UNKNOWN case.
        //
        //          RESULT_TO_STRING_CASE(DD_RESULT_TUTORIAL_AMD_UNKNOWN);
        //          RESULT_TO_STRING_CASE(DD_RESULT_TUTORIAL_AMD_COMPUTER_TURNED_OFF);
        //          RESULT_TO_STRING_CASE(DD_RESULT_TUTORIAL_AMD_DEVICE_FELL_OFF_THE_BUS);
        //          RESULT_TO_STRING_CASE(DD_RESULT_TUTORIAL_AMD_COLOR_IS_NOT_RED);
        //
        //      And with that, we're done! You've added a new section! Congrats.
        //
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    return "DD_RESULT_UNKNOWN";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddApiClampResult(int32_t result)
{
    // If we already recognize the result, there's nothing that we need to do
    switch (result)
    {
        case DD_RESULT_UNKNOWN:

        case DD_RESULT_DEBUG_UNINIT_STACK_MEMORY:
        case DD_RESULT_DEBUG_UNINIT_HEAP_MEMORY:
        case DD_RESULT_DEBUG_FREED_HEAP_MEMORY:

        case DD_RESULT_SUCCESS:

        case DD_RESULT_COMMON_UNIMPLEMENTED:
        case DD_RESULT_COMMON_INVALID_PARAMETER:
        case DD_RESULT_COMMON_OUT_OF_HEAP_MEMORY:
        case DD_RESULT_COMMON_BUFFER_TOO_SMALL:
        case DD_RESULT_COMMON_VERSION_MISMATCH:
        case DD_RESULT_COMMON_INTERFACE_NOT_FOUND:
        case DD_RESULT_COMMON_ALREADY_EXISTS:
        case DD_RESULT_COMMON_DOES_NOT_EXIST:
        case DD_RESULT_COMMON_LIMIT_REACHED:
        case DD_RESULT_COMMON_UNSUPPORTED:
        case DD_RESULT_COMMON_SUCCESS_WITH_ERRORS:

        case DD_RESULT_PARSING_UNKNOWN:
        case DD_RESULT_PARSING_INVALID_BYTES:
        case DD_RESULT_PARSING_INVALID_STRING:
        case DD_RESULT_PARSING_INVALID_JSON:
        case DD_RESULT_PARSING_INVALID_MSGPACK:
        case DD_RESULT_PARSING_INVALID_STRUCTURE:
        case DD_RESULT_PARSING_UNEXPECTED_EOF:

        case DD_RESULT_FS_UNKNOWN:
        case DD_RESULT_FS_NOT_FOUND:
        case DD_RESULT_FS_PERMISSION_DENIED:
        case DD_RESULT_FS_BROKEN_PIPE:
        case DD_RESULT_FS_ALREADY_EXISTS:
        case DD_RESULT_FS_WOULD_BLOCK:
        case DD_RESULT_FS_INVALID_DATA:
        case DD_RESULT_FS_TIMED_OUT:
        case DD_RESULT_FS_INTERRUPTED:

        case DD_RESULT_NET_UNKNOWN:
        case DD_RESULT_NET_CONNECTION_EXISTS:
        case DD_RESULT_NET_CONNECTION_REFUSED:
        case DD_RESULT_NET_CONNECTION_RESET:
        case DD_RESULT_NET_CONNECTION_ABORTED:
        case DD_RESULT_NET_NOT_CONNECTED:
        case DD_RESULT_NET_ADDR_IN_USE:
        case DD_RESULT_NET_ADDR_NOT_AVAILABLE:
        case DD_RESULT_NET_WOULD_BLOCK:
        case DD_RESULT_NET_TIMED_OUT:
        case DD_RESULT_NET_INTERRUPTED:
        case DD_RESULT_NET_SOCKET_TYPE_UNSUPPORTED:

        case DD_RESULT_DD_UNKNOWN:
        case DD_RESULT_DD_BUS_UNAVAILABLE:
        case DD_RESULT_DD_INVALID_DATA_CONTEXT:
        case DD_RESULT_DD_INVALID_CLIENT_CONTEXT:
        case DD_RESULT_DD_INVALID_SYSTEM_CONTEXT:
        case DD_RESULT_DD_API_FEATURE_NOT_ENABLED:
        case DD_RESULT_DD_GENERIC_NOT_READY:
        case DD_RESULT_DD_GENERIC_VERSION_MISMATCH:
        case DD_RESULT_DD_GENERIC_UNAVAILABLE:
        case DD_RESULT_DD_GENERIC_REJECTED:
        case DD_RESULT_DD_GENERIC_END_OF_STREAM:
        case DD_RESULT_DD_GENERIC_ABORTED:
        case DD_RESULT_DD_GENERIC_INSUFFICIENT_MEMORY:
        case DD_RESULT_DD_GENERIC_INVALID_PARAMETER:
        case DD_RESULT_DD_GENERIC_INVALID_CLIENT_ID:
        case DD_RESULT_DD_GENERIC_CONNECTION_EXITS:
        case DD_RESULT_DD_GENERIC_FILE_NOT_FOUND:
        case DD_RESULT_DD_GENERIC_FUNTION_NOT_FOUND:
        case DD_RESULT_DD_GENERIC_INTERFACE_NOT_FOUND:
        case DD_RESULT_DD_GENERIC_ENTRY_EXISTS:
        case DD_RESULT_DD_GENERIC_FILE_ACCESS_ERROR:
        case DD_RESULT_DD_GENERIC_FILE_IO_ERROR:
        case DD_RESULT_DD_GENERIC_LIMIT_REACHED:

        case DD_RESULT_DD_URI_SERVICE_REGISTRATION_ERROR:
        case DD_RESULT_DD_URI_STRING_PARSE_ERROR:
        case DD_RESULT_DD_URI_INVALID_PARAMETERs:
        case DD_RESULT_DD_URI_INVALID_POST_DATA_BLOCK:
        case DD_RESULT_DD_URI_INVALID_POST_DATA_SIZE:
        case DD_RESULT_DD_URI_FAILED_TO_ACQUIRE_POST_BLOCK:
        case DD_RESULT_DD_URI_FAILED_TO_OPEN_RESPONSE_BLOCK:
        case DD_RESULT_DD_URI_REQUEST_FAILED:
        case DD_RESULT_DD_URI_PENDING_REQUEST_ERROR:
        case DD_RESULT_DD_URI_INVALID_CHAR:
        case DD_RESULT_DD_URI_INVALID_JSON:

        case DD_RESULT_DD_RPC_SERVICE_NOT_REGISTERED:
        case DD_RESULT_DD_RPC_FUNC_NOT_REGISTERED:
        case DD_RESULT_DD_RPC_FUNC_PARAM_REJECTED:
        case DD_RESULT_DD_RPC_FUNC_PARAM_TOO_LARGE:
        case DD_RESULT_DD_RPC_FUNC_RESPONSE_REJECTED:
        case DD_RESULT_DD_RPC_FUNC_RESPONSE_MISSING:
        case DD_RESULT_DD_RPC_CTRL_UNEXPECTED_RESPONSE_TYPE:
        case DD_RESULT_DD_RPC_CTRL_INVALID_RESPONSE_SIZE:
        case DD_RESULT_DD_RPC_CTRL_INVALID_RESPONSE_DATA_SIZE:
        case DD_RESULT_DD_RPC_CTRL_RESPONSE_SIZE_MISMATCH:
        case DD_RESULT_DD_RPC_CTRL_CORRUPTED_PACKET:
        case DD_RESULT_DD_RPC_FUNC_UNEXPECTED_RETURN_DATA:

        case DD_RESULT_DD_EVENT_EMIT_PROVIDER_DISABLED:
        case DD_RESULT_DD_EVENT_EMIT_INVALID_EVENT_ID:
        case DD_RESULT_DD_EVENT_EMIT_EVENT_DISABLED:

        case DD_RESULT_SETTINGS_SERVICE_INVALID_NAME:
        case DD_RESULT_SETTINGS_SERVICE_INVALID_COMPONENT:
        case DD_RESULT_SETTINGS_SERVICE_INVALID_SETTING_DATA:
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // ADDING_SECTIONS_5
        //
        // 5. Add all of the new variants of your section to this list, so that it's translated correctly.
        //      They just need to fall-through to the static_cast below.
        //      Make sure you include your UNKNOWN case.
        //
        //          case DD_RESULT_TUTORIAL_AMD_UNKNOWN:
        //          case DD_RESULT_TUTORIAL_AMD_COMPUTER_TURNED_OFF:
        //          case DD_RESULT_TUTORIAL_AMD_DEVICE_FELL_OFF_THE_BUS:
        //          case DD_RESULT_TUTORIAL_AMD_COLOR_IS_NOT_RED:
        //
        // 6. Next, we're going to add a check for unused-but-reserved values below
        //      Ctrl+F ADDING_SECTIONS_6
        //
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

            return static_cast<DD_RESULT>(result);
    }

    // At this point, we don't have have an enum for this result code.
    // Check each section and see if we're in range of something.

    if (DD_RESULT_IS_IN_SECTION(result, DD_RESULT_COMMON))
    {
        return DD_RESULT_UNKNOWN;
    }

    if (DD_RESULT_IS_IN_SECTION(result, DD_RESULT_PARSING))
    {
        return DD_RESULT_PARSING_UNKNOWN;
    }

    if (DD_RESULT_IS_IN_SECTION(result, DD_RESULT_FS))
    {
        return DD_RESULT_FS_UNKNOWN;
    }

    if (DD_RESULT_IS_IN_SECTION(result, DD_RESULT_NET))
    {
        return DD_RESULT_NET_UNKNOWN;
    }

    if (DD_RESULT_IS_IN_SECTION(result, DD_RESULT_DD_RPC))
    {
        return DD_RESULT_DD_RPC_UNKNOWN;
    }

    if (DD_RESULT_IS_IN_SECTION(result, DD_RESULT_DD_EVENT))
    {
        return DD_RESULT_DD_EVENT_UNKNOWN;
    }

    if (DD_RESULT_IS_IN_SECTION(result, DD_RESULT_SETTINGS_SERVICE))
    {
        return DD_RESULT_SETTINGS_SERVICE_UNKNOWN;
    }
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // ADDING_SECTIONS_6
    //
    // 6. Copy the above section checks. This will clamp unused-but-reserved values into the section's UNKNOWN.
    //
    //          if (DD_RESULT_IS_IN_SECTION(result, DD_RESULT_TUTORIAL_AMD))
    //          {
    //              return DD_RESULT_TUTORIAL_AMD_UNKNOWN;
    //          }
    //
    // 7. Lastly, we're going to add our new results to the String handling function above:
    //      Ctrl+F ADDING_SECTIONS_7
    //
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    if (DD_RESULT_IS_IN_SECTION(result, DD_RESULT_DD) ||
        DD_RESULT_IS_IN_SECTION(result, DD_RESULT_DD_GENERIC) ||
        DD_RESULT_IS_IN_SECTION(result, DD_RESULT_DD_URI))
    {
        return DD_RESULT_DD_UNKNOWN;
    }

    // Welp.
    DD_WARN_REASON("Unrecognized result, not part of a section");

    return DD_RESULT_UNKNOWN;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT DevDriverToDDResult(Result result)
{
    switch (result)
    {
        case Result::Success:                      return DD_RESULT_SUCCESS;
        case Result::Error:                        return DD_RESULT_UNKNOWN;
        case Result::NotReady:                     return DD_RESULT_DD_GENERIC_NOT_READY;
        case Result::VersionMismatch:              return DD_RESULT_DD_GENERIC_VERSION_MISMATCH;
        case Result::Unavailable:                  return DD_RESULT_DD_GENERIC_UNAVAILABLE;
        case Result::Rejected:                     return DD_RESULT_DD_GENERIC_REJECTED;
        case Result::EndOfStream:                  return DD_RESULT_DD_GENERIC_END_OF_STREAM;
        case Result::Aborted:                      return DD_RESULT_DD_GENERIC_ABORTED;
        case Result::InsufficientMemory:           return DD_RESULT_DD_GENERIC_INSUFFICIENT_MEMORY;
        case Result::InvalidParameter:             return DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
        case Result::InvalidClientId:              return DD_RESULT_DD_GENERIC_INVALID_CLIENT_ID;
        case Result::ConnectionExists:             return DD_RESULT_DD_GENERIC_CONNECTION_EXITS;
        case Result::FileNotFound:                 return DD_RESULT_DD_GENERIC_FILE_NOT_FOUND;
        case Result::FunctionNotFound:             return DD_RESULT_DD_GENERIC_FUNTION_NOT_FOUND;
        case Result::InterfaceNotFound:            return DD_RESULT_DD_GENERIC_INTERFACE_NOT_FOUND;
        case Result::EntryExists:                  return DD_RESULT_DD_GENERIC_ENTRY_EXISTS;
        case Result::FileAccessError:              return DD_RESULT_DD_GENERIC_FILE_ACCESS_ERROR;
        case Result::FileIoError:                  return DD_RESULT_DD_GENERIC_FILE_IO_ERROR;
        case Result::LimitReached:                 return DD_RESULT_DD_GENERIC_LIMIT_REACHED;
        case Result::MemoryOverLimit:              return DD_RESULT_DD_GENERIC_INSUFFICIENT_MEMORY;

        case Result::UriServiceRegistrationError:  return DD_RESULT_DD_URI_SERVICE_REGISTRATION_ERROR;
        case Result::UriStringParseError:          return DD_RESULT_DD_URI_STRING_PARSE_ERROR;
        case Result::UriInvalidParameters:         return DD_RESULT_DD_URI_INVALID_PARAMETERs;
        case Result::UriInvalidPostDataBlock:      return DD_RESULT_DD_URI_INVALID_POST_DATA_BLOCK;
        case Result::UriInvalidPostDataSize:       return DD_RESULT_DD_URI_INVALID_POST_DATA_SIZE;
        case Result::UriFailedToAcquirePostBlock:  return DD_RESULT_DD_URI_FAILED_TO_ACQUIRE_POST_BLOCK;
        case Result::UriFailedToOpenResponseBlock: return DD_RESULT_DD_URI_FAILED_TO_OPEN_RESPONSE_BLOCK;
        case Result::UriRequestFailed:             return DD_RESULT_DD_URI_REQUEST_FAILED;
        case Result::UriPendingRequestError:       return DD_RESULT_DD_URI_PENDING_REQUEST_ERROR;
        case Result::UriInvalidChar:               return DD_RESULT_DD_URI_INVALID_CHAR;
        case Result::UriInvalidJson:               return DD_RESULT_DD_URI_INVALID_JSON;

        // Settings URI Service
        case Result::SettingsUriInvalidComponent:
        case Result::SettingsUriInvalidSettingName:
        case Result::SettingsUriInvalidSettingValue:
        case Result::SettingsUriInvalidSettingValueSize:
        // Info URI Service
        case Result::InfoUriSourceNameInvalid:
        case Result::InfoUriSourceCallbackInvalid:
        case Result::InfoUriSourceAlreadyRegistered:
        case Result::InfoUriSourceWriteFailed:
        // Settings Service
        case Result::SettingsInvalidComponent:
        case Result::SettingsInvalidSettingName:
        case Result::SettingsInvalidSettingValue:
        case Result::SettingsInsufficientValueSize:
        case Result::SettingsInvalidSettingValueSize:
            // These Results aren't expected to be used in the Apis, so we'll map them to UNKNOWN
            // Avoid using `default` here, so that we get warnings when new Results are added but not added here.
            return DD_RESULT_UNKNOWN;
    }

    // We should never reach this - all cases should be handled in the switch() case above,
    // but we include this to silence some warnings on some compilers.
    DD_ASSERT_ALWAYS();
    return DD_RESULT_UNKNOWN;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Allocates memory via the function pointers stored in ApiAllocCallback
void* ddApiAlloc(
    void*  pUserdata, /// Userdata pointer
    size_t size,      /// Size of the allocation
    size_t alignment, /// Alignment requirement for the allocation
    bool   zero)      /// True if the allocation's memory should be zeroed
{
    DD_ASSERT(pUserdata != nullptr);
    ApiAllocCallbacks* pApiAlloc = static_cast<ApiAllocCallbacks*>(pUserdata);

    return pApiAlloc->pAllocCallback(pApiAlloc->pUserdata, size, alignment, static_cast<int>(zero));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Frees memory via the function pointers stored in ApiAllocCallback
void ddApiFree(
    void* pUserdata, /// Userdata pointer
    void* pMemory)   /// Pointer to the memory allocation
{
    DD_ASSERT(pUserdata != nullptr);
    ApiAllocCallbacks* pApiAlloc = static_cast<ApiAllocCallbacks*>(pUserdata);

    pApiAlloc->pFreeCallback(pApiAlloc->pUserdata, pMemory);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Default platform memory allocation function
void* ddApiDefaultAlloc(void* pUserdata, size_t size, size_t alignment, int zero)
{
    DD_UNUSED(pUserdata);
    return DevDriver::Platform::AllocateMemory(size, alignment, zero);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Default platform memory free function
void ddApiDefaultFree(void* pUserdata, void* pMemory)
{
    DD_UNUSED(pUserdata);
    DevDriver::Platform::FreeMemory(pMemory);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Converts a DevDriver LogLevel to a ddApi DD_LOG_LEVEL
DD_LOG_LEVEL ToDDLogLevel(
    LogLevel lvl) /// DevDriver log level
{
    return static_cast<DD_LOG_LEVEL>(static_cast<uint32>(lvl));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Converts a ddApi DD_LOG_LEVEL to a DevDriver LogLevel
LogLevel ToLogLevel(
    DD_LOG_LEVEL lvl) /// API log level
{
    return static_cast<LogLevel>(static_cast<uint32>(lvl));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Convert a `DDAllocCallbacks` structure into a DevDriver::AllocCb structure.
void ConvertAllocCallbacks(
    const DDAllocCallbacks& callbacks,
    ApiAllocCallbacks*      pApiAlloc,
    DevDriver::AllocCb*     pAllocCb)
{
    DD_ASSERT(pApiAlloc != nullptr);
    DD_ASSERT(pAllocCb != nullptr);

    // Default to internal allocation function implementations
    PFN_ddAllocCallback pfnAlloc  = ddApiDefaultAlloc;
    PFN_ddFreeCallback  pfnFree   = ddApiDefaultFree;
    void*               pUserdata = nullptr;

    // If the caller provided both allocator functions, then override our internal defaults
    if ((callbacks.pfnAlloc != nullptr) && (callbacks.pfnFree != nullptr))
    {
        pfnAlloc  = callbacks.pfnAlloc;
        pfnFree   = callbacks.pfnFree;
        pUserdata = callbacks.pUserdata;
    }

    pApiAlloc->pAllocCallback = pfnAlloc;
    pApiAlloc->pFreeCallback  = pfnFree;
    pApiAlloc->pUserdata      = pUserdata;

    pAllocCb->pfnAlloc  = &ddApiAlloc;
    pAllocCb->pfnFree   = &ddApiFree;
    pAllocCb->pUserdata = pApiAlloc;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Verify that alloc callbacks are valid and present, or missing.
// Returns a valid callback (possibly the default one) in `pCallbacks`.
DD_RESULT ValidateAlloc(
    const DDAllocCallbacks& alloc,
    DDAllocCallbacks*       pCallbacks)
{
    // This can only happen if the caller mis-uses this function.
    // That's a programmer error in here, so we can assert instead of handling it.
    DD_ASSERT(pCallbacks != nullptr);

    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if ((alloc.pfnAlloc != nullptr) && (alloc.pfnFree != nullptr))
    {
        // Valid alloc callbacks must provide all methods
        *pCallbacks = alloc;
        result      = DD_RESULT_SUCCESS;
    }
    else if ((alloc.pfnAlloc != nullptr) || (alloc.pfnFree != nullptr))
    {
        // Only supplying one of the callbacks is incorrect
        result = DD_RESULT_COMMON_INVALID_PARAMETER;
    }
    else
    {
        // Otherwise, we'll defer to our defaults
        pCallbacks->pUserdata = nullptr;
        pCallbacks->pfnAlloc  = [](void* pUserdata, size_t size, size_t alignment, int zero) -> void* {
            DD_UNUSED(pUserdata);
            return DevDriver::Platform::GenericAllocCb.Alloc(size, alignment, zero != 0);
        };
        pCallbacks->pfnFree = [](void* pUserdata, void* pMem) {
            DD_UNUSED(pUserdata);
            return DevDriver::Platform::GenericAllocCb.Free(pMem);
        };

        result = DD_RESULT_SUCCESS;
    }

    if (result == DD_RESULT_SUCCESS)
    {
        DD_ASSERT(pCallbacks->pfnAlloc != nullptr);
        DD_ASSERT(pCallbacks->pfnFree != nullptr);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Default logging function
void DefaultLog(
    void*             pUserdata,
    const DDLogEvent* pEvent,
    const char*       pMessage)
{
    // No userdata to use
    DD_API_UNUSED(pUserdata);

    // pEvent should never be NULL here.
    // It may be NULL in the Push/Pop calls, but they are supposed to NULL-check before calling Log.
    DD_ASSERT(pEvent != nullptr);

    // Sanity check that we have a real log level
    DD_ASSERT(pEvent->level < DD_LOG_LEVEL_COUNT);

    const char* pLevel = "";
    switch (pEvent->level)
    {
        case DD_LOG_LEVEL_DEBUG: pLevel = "Debug"; break;
        case DD_LOG_LEVEL_VERBOSE: pLevel = "Verbose"; break;
        case DD_LOG_LEVEL_INFO: pLevel = "Info"; break;
        case DD_LOG_LEVEL_WARN: pLevel = "Warn"; break;
        case DD_LOG_LEVEL_ERROR: pLevel = "Error"; break;
        case DD_LOG_LEVEL_ALWAYS: pLevel = "Always"; break;
        case DD_LOG_LEVEL_COUNT: pLevel = "Count"; break;
        case DD_LOG_LEVEL_NEVER: pLevel = "Never"; break;
    };

    // The source code locations are not always available, so guard against that
    if (pEvent->pFilename != nullptr)
    {
        DD_PRINT(
            DevDriver::LogLevel::Info,
            "[%s] [%s] %s:%u: %s(): %s\n",
            pLevel,
            pEvent->pCategory,
            pEvent->pFilename,
            pEvent->lineNumber,
            pEvent->pFunction,
            pMessage);
    }
    else
    {
        DD_PRINT(DevDriver::LogLevel::Info, "[%s] [%s] %s\n", pLevel, pEvent->pCategory, pMessage);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helper to validate logging callbacks
DD_RESULT ValidateLog(
    const DDLoggerInfo& logger,
    DDLoggerInfo*       pCallbacks)
{
    // This can only happen if the caller mis-uses this function.
    // That's a programmer error in here, so we can assert instead of handling it.
    DD_ASSERT(pCallbacks != nullptr);

    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if ((logger.pfnWillLog != nullptr) && (logger.pfnLog != nullptr) && (logger.pfnPush != nullptr) &&
        (logger.pfnPop != nullptr))
    {
        // Valid log callbacks must provide all methods
        *pCallbacks = logger;
        result      = DD_RESULT_SUCCESS;
    }
    else if (
        (logger.pfnWillLog != nullptr) || (logger.pfnLog != nullptr) || (logger.pfnPush != nullptr) ||
        (logger.pfnPop != nullptr))
    {
        // Only supplying one of the callbacks is incorrect
        result = DD_RESULT_COMMON_INVALID_PARAMETER;
    }
    else
    {
        // Otherwise, we'll defer to our defaults

        // No userdata
        pCallbacks->pUserdata = nullptr;

        // Log is nontrivial, so we give it a function name for easier debugging
        pCallbacks->pfnLog = DefaultLog;

        // The rest of callbacks are trivial, so we impl them inline
        pCallbacks->pfnWillLog = [](void* pUserdata, const DDLogEvent* pEvent) -> int32_t {
            DD_API_UNUSED(pUserdata);
            DD_API_UNUSED(pEvent);
            return 1;
        };
        pCallbacks->pfnPush = [](void* pUserdata, const DDLogEvent* pEvent, const char* pMessage) {
            if (pEvent != nullptr)
            {
                DefaultLog(pUserdata, pEvent, pMessage);
            }
        };
        pCallbacks->pfnPop = [](void* pUserdata, const DDLogEvent* pEvent, const char* pMessage) {
            if (pEvent != nullptr)
            {
                DefaultLog(pUserdata, pEvent, pMessage);
            }
        };

        result = DD_RESULT_SUCCESS;
    }

    if (result == DD_RESULT_SUCCESS)
    {
        DD_ASSERT(pCallbacks->pfnWillLog != nullptr);
        DD_ASSERT(pCallbacks->pfnLog != nullptr);
        DD_ASSERT(pCallbacks->pfnPush != nullptr);
        DD_ASSERT(pCallbacks->pfnPop != nullptr);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Convert a `DD_DRIVER_STATE` into a human recognizable string.
const char* ddApiDriverStateToString(DD_DRIVER_STATE state)
{
    switch (state)
    {
    case DD_DRIVER_STATE_PLATFORMINIT:   return "Platform Init";
    case DD_DRIVER_STATE_DEVICEINIT:     return "Device Init";
    case DD_DRIVER_STATE_POSTDEVICEINIT: return "Post Device Init";
    case DD_DRIVER_STATE_RUNNING:        return "Running";
    case DD_DRIVER_STATE_PAUSED:         return "Paused";
    case DD_DRIVER_STATE_DISCONNECTED:   return "Disconnected";
    case DD_DRIVER_STATE_UNKNOWN:        return "Unknown";
    case DD_DRIVER_STATE_COUNT:          break; // Count is not a valid result
    }

    DD_WARN_REASON("Invalid State");
    return "Invalid State";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ddApiIsDriverInitialized(DD_DRIVER_STATE state)
{
    return ((state == DD_DRIVER_STATE_RUNNING) || (state == DD_DRIVER_STATE_PAUSED));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
LoggerUtil::LoggerUtil(const DDLoggerInfo& info)
{
    // A valid or empty logger will succeed here. Anything incomplete is a hard error.
    const DD_RESULT result = ValidateLog(info, &m_info);
    DD_ASSERT(result == DD_RESULT_SUCCESS);
    DD_UNUSED(result);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void LoggerUtil::Printf(
    const DDLogEvent& event,

    const char*       pFmt,
    ...
)
{
    va_list args;
    va_start(args, pFmt);
    Vprintf(event, pFmt, args);
    va_end(args);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void LoggerUtil::Log(const DDLogEvent& event, const char* pMessage)
{
    m_info.pfnLog(m_info.pUserdata, &event, pMessage);
}

///////////////////////////////////////////////////////////////////////////
/// Basic Pushing and popping of scope

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void LoggerUtil::Push()
{
    m_info.pfnLog(m_info.pUserdata, nullptr, nullptr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void LoggerUtil::Pop()
{
    m_info.pfnLog(m_info.pUserdata, nullptr, nullptr);
}

///////////////////////////////////////////////////////////////////////////
/// Basic Pushing and popping of scope with an associated event

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void LoggerUtil::Push(const DDLogEvent& event, const char* pMessage)
{
    m_info.pfnLog(m_info.pUserdata, &event, pMessage);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void LoggerUtil::Pop(const DDLogEvent& event, const char* pMessage)
{
    m_info.pfnLog(m_info.pUserdata, &event, pMessage);
}

namespace DDInternal
{
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Helper method to make an Event. Don't call this directly.
    /// We can enable or disable optional fields in this
    DDLogEvent MakeEventHelper(
        DD_LOG_LEVEL level,
        const char*  pCategory,
        const char*  pFilename,
        const char*  pFunction,
        uint32_t     lineNumber
    )
    {
        // Try to keep these fields in the order that they're declared
        DDLogEvent event = {};

        event.pCategory  = pCategory;

#ifdef NDEBUG
        // Don't expose source-code info
        DD_UNUSED(pFilename);
        DD_UNUSED(pFunction);
        DD_UNUSED(lineNumber);
#else
        event.pFilename  = pFilename;
        event.pFunction  = pFunction;
        event.lineNumber = lineNumber;
#endif

        event.level = level;

        return event;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void LoggerUtil::Vprintf(
    const DDLogEvent& event,
    const char*       pFmt,
    va_list           args
)
{
    char buffer[LoggerUtil::MaxFormattedMessageLen] = {};
    Platform::Vsnprintf(buffer, sizeof(buffer), pFmt, args);

    Log(event, buffer);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Default logger that routes to DD_PRINT_FUNC
void DefaultLogCallback(
    void*            pUserdata,
   const DDLogEvent* pEvent,
   const char*       pMessage
)
{
    DD_UNUSED(pUserdata);

    const LogLevel ddLevel = ToLogLevel(pEvent->level);
    if (DD_WILL_PRINT(ddLevel))
    {
        DD_PRINT_FUNC(ddLevel, "[%s] %s", pEvent->pCategory, pMessage);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DDLoggerInfo GetApiDefaultLoggerInfo()
{
    DDLoggerInfo defaultLoggerInfo = {};

    defaultLoggerInfo.pUserdata = nullptr;

    defaultLoggerInfo.pfnWillLog = [](
        void*             pUserdata,
        const DDLogEvent* pEvent) -> int32_t
    {
        DD_UNUSED(pUserdata);
        return DD_WILL_PRINT(ToLogLevel(pEvent->level));
    };

    defaultLoggerInfo.pfnLog = &DefaultLogCallback;

    defaultLoggerInfo.pfnPush = [](
        void*             pUserdata,
        const DDLogEvent* pEvent,
        const char*       pMessage)
    {
        // Scopes are not supported by the default logger,
        // but we still need to log this event
        if (pEvent != nullptr)
        {
            DefaultLogCallback(pUserdata, pEvent, pMessage);
        }
    };

    defaultLoggerInfo.pfnPop = [](
        void*             pUserdata,
        const DDLogEvent* pEvent,
        const char*       pMessage)
    {
        // Scopes are not supported by the default logger,
        // but we still need to log this event
        if (pEvent != nullptr)
        {
            DefaultLogCallback(pUserdata, pEvent, pMessage);
        }
    };

    return defaultLoggerInfo;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FixedBufferByteWriter::FixedBufferByteWriter(
    void*  pBuffer,
    size_t bufferSize)
    : m_pBuffer(pBuffer)
    , m_bufferSize(bufferSize)
    , m_bytesWritten(0)
{
    m_writer.pUserdata = this;

    m_writer.pfnBegin = [](void* pUserdata, const size_t* pTotalDataSize) -> DD_RESULT {
        DD_API_UNUSED(pUserdata);
        DD_API_UNUSED(pTotalDataSize);

        return DD_RESULT_SUCCESS;
    };

    m_writer.pfnWriteBytes = [](void* pUserdata, const void* pData, size_t dataSize) -> DD_RESULT {
        auto* pThis = reinterpret_cast<FixedBufferByteWriter*>(pUserdata);
        const size_t bytesToWrite = Platform::Min(pThis->m_bufferSize - pThis->m_bytesWritten, dataSize);

        memcpy(reinterpret_cast<uint8_t*>(pThis->m_pBuffer) + pThis->m_bytesWritten, pData, bytesToWrite);

        pThis->m_bytesWritten += bytesToWrite;

        return (bytesToWrite == dataSize) ? DD_RESULT_SUCCESS : DD_RESULT_COMMON_BUFFER_TOO_SMALL;
    };

    m_writer.pfnEnd = [](void* pUserdata, DD_RESULT result) {
        // Nothing to do here
        DD_API_UNUSED(pUserdata);
        DD_API_UNUSED(result);
    };
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DynamicBufferByteWriter::DynamicBufferByteWriter()
    : m_buffer(Platform::GenericAllocCb)
{
    m_writer.pUserdata = this;

    m_writer.pfnBegin = [](void* pUserdata, const size_t* pTotalDataSize) -> DD_RESULT {
        DD_API_UNUSED(pUserdata);
        DD_API_UNUSED(pTotalDataSize);

        return DD_RESULT_SUCCESS;
    };

    m_writer.pfnWriteBytes = [](void* pUserdata, const void* pData, size_t dataSize) -> DD_RESULT {
        auto* pThis = reinterpret_cast<DynamicBufferByteWriter*>(pUserdata);

        const size_t oldSize = pThis->m_buffer.Grow(dataSize);
        memcpy(&pThis->m_buffer[oldSize], pData, dataSize);

        return DD_RESULT_SUCCESS;
    };

    m_writer.pfnEnd = [](void* pUserdata, DD_RESULT result) {
        // Nothing to do here
        DD_API_UNUSED(pUserdata);
        DD_API_UNUSED(result);
    };
}

// =======================================================================================
DevDriver::Vector<uint8_t> DynamicBufferByteWriter::Take()
{
    DevDriver::Vector<uint8_t> takeBuffer(Platform::GenericAllocCb);
    takeBuffer.Swap(m_buffer);

    return takeBuffer;
}
