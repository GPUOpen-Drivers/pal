/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palStringUtil.h
 * @brief PAL String utility collection functions.
 ***********************************************************************************************************************
 */

#pragma once

namespace Util
{

/// Returns the length of a wchar_t based string.  This function is necessary when specifying the -fshort-wchar option
/// because the standard library wcslen still interprets its argument using a 4 byte UTF-32 wide character.
///
/// @returns The length of the given string in wide characters
extern size_t PalWcslen(
    const wchar_t* pWideStr);   ///< [in] wide string to query

/// Performs a reverse string find of wide character wc.  This function is necessary when specifying the -fshort-wchar option
/// because the standard library wcsrchr still interprets its arguments using a 4 byte UTF-32 wide character.
///
/// @returns The matching character at the end of the string or nullptr if not found.
extern wchar_t* PalWcsrchr(
    wchar_t *pStr,  ///< [in] wide string to scan
    wchar_t wc);    ///< [in] wide character to find

/// When the -fshort-char compiler option is specified, wchar_t is 16 bits, but mbstowcs still treats the dest
/// as 32 bit so we provide our own implementation.
///
/// @returns Returns whether or not the conversion was successful.
extern bool ConvertCharStringToUtf16(
    wchar_t*      pDst,             ///< [out] dst string
    const char*   pSrc,             ///< [in] src string
    size_t        dstSizeInWords);  ///< size of the destination buffer in words

/// When the -fshort-char compiler option is specified, wchar_t is 16 bits, but wcstombs still treats the src
/// as 32 bit so we provide our own implementation.
///
/// @returns Returns whether or not the conversion was successful.
extern bool ConvertUtf16StringToUtf8(
    char*          pDst,            ///< [out] dst string
    const wchar_t* pSrc,            ///< [in] src string
    size_t         dstSizeInBytes); ///< size of the destination buffer in bytes

/// When the -fshort-char compiler option is specified, wchar_t is 16 bits, but wcsncpy still treats its arguments
/// as 32 bit so we provide our own implementation.
extern void CopyUtf16String(
    wchar_t*       pDst,        ///< [out] Destination string.
    const wchar_t* pSrc,        ///< [in] Source string to copy.
    size_t         dstSize);    ///< Length of the destination buffer, in wchar_t's.

} // Util

