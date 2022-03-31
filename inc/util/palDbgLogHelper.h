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
/**
 ***********************************************************************************************************************
 * @file  palDbgLogHelper.h
 * @brief Contains definitions used by DbgLogMgr.
 ***********************************************************************************************************************
 */

#pragma once

#if PAL_ENABLE_LOGGING
#include "palUtil.h"

namespace Util
{
/// The SeverityLevel and OriginationType are used by the debug loggers to filter
/// incoming messages. For example, a file logger may allow all messages of all
/// severity and origination whereas an AMDLOG logger may only allow messages that
/// have SeverityLevel >= Critical and OriginationType >= Telemetry. Default cutoff
/// values for these will be set in the IDgbLogger class and derived loggers are
/// supposed to override the cutoffs according to their needs.

/// Specifies the severity level for each log message
enum class SeverityLevel : uint32
{
    Debug = 0,  ///< Information useful to developers for debugging.
    Info,       ///< Normal operational messages that require no action.
    Warning,    ///< Indicates that an error might occur if action is not taken.
    Error,      ///< Error conditions that are cause for concern.
    Critical,   ///< Critical conditions which indicate catastrophic failure is imminent.
    Count
};

/// Look up table for SeverityLevel. The order of entries in this table must match the order of enums in
/// SeverityLevel and this table should be updated whenever there is a change to SeverityLevel enums.
constexpr const char* SeverityLevelTable[] =
{
    "Debug",
    "Info",
    "Warning",
    "Error",
    "Critical",
};

/// Catch any mismatch between SeverityLevel and SeverityLevelTable entries
constexpr uint32 SeverityLevelTableSize = sizeof(SeverityLevelTable) / sizeof(SeverityLevelTable[0]);
static_assert(SeverityLevelTableSize == static_cast<uint32>(SeverityLevel::Count),
              "SeverityLevel and SeverityLevelTable are out of sync!");

/// Specifies the origination type (source) of each log message.
enum class OriginationType : uint32
{
    Unknown = 0,     ///< Originating from an unknown source
    DebugPrint,      ///< Originating from PAL_DPINFO, PAL_DPERROR, etc. .
    Alert,           ///< Originating from PAL_ALERT* macros.
    Assert,          ///< Originating from PAL_ASSERT* macros.
    Telemetry,       ///< Used for msgs regarding crash dump and offline analysis
    PipelineCompiler,///< Originating from pipeline compiler
    Count
};

/// Specifies the flag, or the bit position of each origination type used to turn
/// on/off this origination type. The number of enumerators here must match the
/// number of enumerators in the OriginationType enum.
/// A debug logger may be interested in logging msgs from multiple sources. Hence, these
/// can be used to create a mask of origination types to be used as a filter by the
/// debug loggers.
/// Example: If a client wants to create a debug logger to log debug prints, alerts and
/// asserts, then it should create the following bit mask:
///
/// uint32 mask = uint32(OriginationTypeFlags::OriginationTypeFlagDebugPrint) |
///               uint32(OriginationTypeFlags::OriginationTypeFlagAlert)      |
///               uint32(OriginationTypeFlags::OriginationTypeFlagAssert);
///
/// and pass this mask in the constructor of the debug logger.
enum OriginationTypeFlags : uint32
{
    OriginationTypeFlagUnknown        = (1ul << uint32(OriginationType::Unknown)),
    OriginationTypeFlagDebugPrint     = (1ul << uint32(OriginationType::DebugPrint)),
    OriginationTypeFlagAlert          = (1ul << uint32(OriginationType::Alert)),
    OriginationTypeFlagAssert         = (1ul << uint32(OriginationType::Assert)),
    OriginationTypeFlagTelemetry      = (1ul << uint32(OriginationType::Telemetry)),
    OriginationTypeFlagShaderCompiler = (1ul << uint32(OriginationType::PipelineCompiler))
};

constexpr uint32 AllOriginationTypes = uint32(OriginationTypeFlags::OriginationTypeFlagUnknown)    |
                                       uint32(OriginationTypeFlags::OriginationTypeFlagDebugPrint) |
                                       uint32(OriginationTypeFlags::OriginationTypeFlagAlert)      |
                                       uint32(OriginationTypeFlags::OriginationTypeFlagAssert)     |
                                       uint32(OriginationTypeFlags::OriginationTypeFlagTelemetry)  |
                                       uint32(OriginationTypeFlags::OriginationTypeFlagShaderCompiler);

/// Expected maximum number of characters in the client tag.
/// A client tag indicates the client that logs a message.
constexpr uint32 ClientTagSize = 8;

/// Generic debug log function called by PAL_DPF macros - variable args version.
/// Clients should use the PAL_DPF macros instead of calling this function directly.
///
/// @param [in] severity     Specifies the severity level of the log message
/// @param [in] source       Specifies the origination type (source) of the log message
/// @param [in] pClientTag   Indicates the client that logs a message. Only the first 'ClientTagSize'
///                          number of characters will be used to identify the client.
/// @param [in] pFormat      Format string for the log message.
/// @param [in] ...          Variable arguments that correspond to the format string.
extern void DbgLog(
    SeverityLevel   severity,
    OriginationType source,
    const char*     pClientTag,
    const char*     pFormat,
    ...);

/// Generic debug log function for debug prints - va_list version
/// Clients should use the PAL_DPF macros instead of calling this function directly.
///
/// @param [in] severity     Specifies the severity level of the log message
/// @param [in] source       Specifies the origination type (source) of the log message
/// @param [in] pClientTag   Indicates the client that logs a message. Only the first 'ClientTagSize'
///                          number of characters will be used to identify the client.
/// @param [in] pFormat      Format string for the log message.
/// @param [in] argList      Printf-style argument list.
extern void DbgVLog(
    SeverityLevel   severity,
    OriginationType source,
    const char*     pClientTag,
    const char*     pFormat,
    va_list         argList);

} // Util
#endif
