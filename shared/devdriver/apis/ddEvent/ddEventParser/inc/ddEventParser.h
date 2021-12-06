/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#ifndef DD_EVENT_PARSER_HEADER
#define DD_EVENT_PARSER_HEADER

#include <ddEventParserApi.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Attempts to create a new parser object with the provided creation information
DD_RESULT ddEventParserCreate(
    const DDEventParserCreateInfo* pInfo,     /// [in]  Create info
    DDEventParser*                 phParser); /// [out] Handle to the new parser object

/// Destroys an existing parser object
void ddEventParserDestroy(
    DDEventParser hParser); /// [in] Handle to the existing parser object

/// Parses the provided buffer of formatted event data
///
/// Returns parsed data through the DDEventWriter that was provided during the parser creation
DD_RESULT ddEventParserParse(
    DDEventParser hParser,   /// Handle to the existing parser object
    const void*   pData,     /// [in] Pointer to a buffer that contains formatted event data
    size_t        dataSize); /// Size of the buffer pointed to by pData

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ! DD_EVENT_PARSER_HEADER

