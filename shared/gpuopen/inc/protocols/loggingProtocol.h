/*
 *******************************************************************************
 *
 * Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

#include "gpuopen.h"

#define LOGGING_PROTOCOL_MAJOR_VERSION 3
#define LOGGING_PROTOCOL_MINOR_VERSION 0

#define LOGGING_INTERFACE_VERSION ((LOGGING_INTERFACE_MAJOR_VERSION << 16) | LOGGING_INTERFACE_MINOR_VERSION)

#define LOGGING_PROTOCOL_MINIMUM_MAJOR_VERSION 1

/*
***********************************************************************************************************************
*| Version | Change Description                                                                                       |
*| ------- | ---------------------------------------------------------------------------------------------------------|
*|  3.0    | Variably sized log message support                                                                       |
*|  2.0    | Refactor to simplify protocol + API semantics                                                            |
*|  1.0    | Initial version                                                                                          |
***********************************************************************************************************************
*/

#define LOGGING_LARGE_MESSAGES_VERSION 3
#define LOGGING_REFACTOR_VERSION 2
#define LOGGING_INITIAL_VERSION 1

namespace DevDriver
{

    namespace LoggingProtocol
    {
        ///////////////////////
        // Logging Constants
        DD_STATIC_CONST uint32 kLegacyLoggingPayloadSize = 152;

        // Subtract the logging header size from the max payload size to get the max size for any logging payload.
        DD_STATIC_CONST uint32 kLoggingHeaderSize = sizeof(uint64);
        DD_STATIC_CONST uint32 kMaxLoggingPayloadSize = (kMaxPayloadSizeInBytes - kLoggingHeaderSize);

        ///////////////////////
        // Logging Protocol
        enum struct LoggingMessage : uint32
        {
            Unknown = 0,
            EnableLoggingRequest,
            EnableLoggingResponse,
            DisableLogging,
            QueryCategoriesRequest,
            QueryCategoriesNumResponse,
            QueryCategoriesDataResponse,
            LogMessage,
            LogMessageSentinel,
            Count
        };

        ///////////////////////
        // Logging Types
        typedef uint64 LoggingCategory;

        // WARNING: Do not increase this without also changing the payload size.
        DD_STATIC_CONST uint32 kMaxCategoryCount = 64;
        DD_STATIC_CONST uint32 kMaxCategoryIndex = (kMaxCategoryCount - 1);
        DD_STATIC_CONST LoggingCategory kAllLoggingCategories = static_cast<LoggingCategory>(-1);

        // offset definition for the default categories.
        // we are reserving a total of four, giving us two we can use in the future
        enum DefaultCategories : LoggingCategory
        {
            kGeneralCategoryOffset,
            kSystemCategoryOffset,
            kReservedOffset1,
            kReservedOffset2,
            kReservedCategoryCount
        };

        // define categories that are available to client applications
        DD_STATIC_CONST uint32 kDefinableCategoryCount = kMaxCategoryCount - kReservedCategoryCount;
        DD_STATIC_CONST LoggingCategory kDefinableCategoryMask = ((LoggingCategory)1 << kDefinableCategoryCount) - 1;

        static_assert(kDefinableCategoryCount <= kMaxCategoryCount, "Invalid kReservedCategoryCount");
        // ensure that the available logging category mask is wholly contained inside the all category mask
        static_assert((kDefinableCategoryMask & kAllLoggingCategories) == kDefinableCategoryMask,
                      "Invalid category masks defined");

        // define the default category masks start so that the first mask is outside of the kDefinableCategoryMask
        enum BaseCategoryMasks : LoggingCategory
        {
            kGeneralCategoryMask = ((LoggingCategory)1 << (kDefinableCategoryCount + kGeneralCategoryOffset)),
            kSystemCategoryMask = ((LoggingCategory)1 << (kDefinableCategoryCount + kSystemCategoryOffset))
        };

        // test to make sure that the base logging category bitmasks are contained inside the all logging category mask
        static_assert((kAllLoggingCategories & kGeneralCategoryMask) == kGeneralCategoryMask,
                      "Invalid category masks defined");
        static_assert((kAllLoggingCategories & kSystemCategoryMask) == kSystemCategoryMask,
                      "Invalid category masks defined");

        // ensure that the base logging categories do not overlap with the available logging category mask
        static_assert((kDefinableCategoryMask & kGeneralCategoryMask) == 0, "Invalid category masks defined");
        static_assert((kDefinableCategoryMask & kSystemCategoryMask) == 0, "Invalid category masks defined");

        // A logging category is defined as both a bitmask + a name
        DD_ALIGNED_STRUCT(8) NamedLoggingCategory
        {
            LoggingCategory category;
            char            name[kMaxLoggingPayloadSize - sizeof(LoggingCategory)];
        };

        DD_CHECK_SIZE(NamedLoggingCategory, kMaxLoggingPayloadSize);

        // ensure that we cannot define more categories than we have bits more
        static_assert(kMaxCategoryCount <= 64, "kMaxCategoryCount is too big to fit inside the payload.");

        // logging filter definition
        // TODO: consider replacing this with manual bitshifting
        DD_ALIGNED_STRUCT(8) LoggingFilter
        {
            LoggingCategory        category;
            uint8                  reserved[7];
            LogLevel               priority;
        };

        DD_CHECK_SIZE(LoggingFilter, 16);

        // logging message definition. Filter is included so the client can identify the message.
        DD_ALIGNED_STRUCT(8) LogMessage
        {
            LoggingFilter filter;
            char message[kMaxLoggingPayloadSize - sizeof(LoggingFilter)];
        };

        DD_CHECK_SIZE(LogMessage, kMaxLoggingPayloadSize);

        ///////////////////////
        // Logging Payloads
        DD_ALIGNED_STRUCT(8) LoggingHeader
        {
            LoggingMessage command;
        };

        DD_CHECK_SIZE(LoggingHeader, 8);

        static_assert(sizeof(LoggingHeader) == kLoggingHeaderSize, "Logging header size mismatch!");

        DD_ALIGNED_STRUCT(8) EnableLoggingRequestPayload : public LoggingHeader
        {
            LoggingFilter filter;
        };

        DD_CHECK_SIZE(EnableLoggingRequestPayload, sizeof(LoggingHeader) + 16);

        DD_ALIGNED_STRUCT(8) EnableLoggingResponsePayload : public LoggingHeader
        {
            uint32 _padding;
            Result result;
        };

        DD_CHECK_SIZE(EnableLoggingResponsePayload, sizeof(LoggingHeader) + 8);

        DD_ALIGNED_STRUCT(8) QueryCategoriesNumResponsePayload : public LoggingHeader
        {
            uint32 _padding;
            uint32 numCategories;
        };

        DD_CHECK_SIZE(QueryCategoriesNumResponsePayload, sizeof(LoggingHeader) + 8);

        DD_ALIGNED_STRUCT(8) QueryCategoriesDataResponsePayload : public LoggingHeader
        {
            NamedLoggingCategory category;
        };

        DD_CHECK_SIZE(QueryCategoriesDataResponsePayload, sizeof(LoggingHeader) + sizeof(NamedLoggingCategory));

        DD_STATIC_CONST size_t kQueryCategoriesDataPayloadNameOffset =
            sizeof(LoggingHeader) + offsetof(NamedLoggingCategory, name);

        DD_ALIGNED_STRUCT(8) LogMessagePayload : public LoggingHeader
        {
            LogMessage message;
        };

        DD_CHECK_SIZE(LogMessagePayload, sizeof(LoggingHeader) + sizeof(LogMessage));

        DD_STATIC_CONST size_t kLogMessagePayloadMessageOffset =
            sizeof(LoggingHeader) + offsetof(LogMessage, message);

    }
}
