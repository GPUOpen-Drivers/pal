/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <string.h>
#include <codecvt>
#include "palAssert.h"
#include "palStringUtil.h"
#include "palInlineFuncs.h"

namespace Util
{

// =====================================================================================================================
// Returns the length of a wchar_t based string.  This function is necessary when specifying the -fshort-wchar option
size_t  PalWcslen(
    const wchar_t* pWideStr)    ///< [in] wide string to query
{
    size_t      len = 0;

    if (pWideStr != nullptr)
    {
        const wchar_t*    pWchar = pWideStr;

        while (*pWchar != 0)
        {
            ++pWchar;
            ++len;
        }
    }

    return len;
}

// =====================================================================================================================
// Pal implementation for wcsrchr
extern wchar_t* PalWcsrchr(
    wchar_t *pStr,  ///< [in] wide string to scan
    wchar_t wc)     ///< [in] wide character to find
{
    wchar_t*  pCurr = nullptr;

    if (pStr != nullptr)
    {
        pCurr = const_cast<wchar_t*>(pStr) + PalWcslen(pStr);
        while (pCurr != pStr)
        {
            if (*pCurr == wc)
            {
                break;
            }
            --pCurr;
        }
        if ((pCurr == pStr) && (*pCurr != wc))
        {
            pCurr = nullptr;
        }
    }

    return pCurr;
}

// =====================================================================================================================
// Convert char string to UTF-16
bool ConvertCharStringToUtf16(
    wchar_t*      pDst,             ///< [out] dst string
    const char*   pSrc,             ///< [in] src string
    size_t        dstSizeInWords)   ///< size of the destination buffer in words
{
    std::codecvt_utf8_utf16<char16_t>   converter;
    mbstate_t                           state = {0};

    const char* pCharNext = nullptr;
    char16_t*   pUtf16Next = nullptr;

    std::codecvt_base::result retCode = converter.in(state,
                                                     pSrc,
                                                     pSrc+strlen(pSrc)+1,
                                                     pCharNext,
                                                     reinterpret_cast<char16_t*>(pDst),
                                                     reinterpret_cast<char16_t*>(pDst) + dstSizeInWords,
                                                     pUtf16Next);
    return (retCode == std::codecvt_base::ok);
}

// =====================================================================================================================
// Convert UTf-16 string to UTF-8
bool ConvertUtf16StringToUtf8(
    char*          pDst,           ///< [out] dst string
    const wchar_t* pSrc,           ///< [in] src string
    size_t         dstSizeInBytes) ///< size of the destination buffer in bytes
{
    std::codecvt_utf8_utf16<char16_t>   converter;
    mbstate_t                           state = {0};

    char*           pCharNext = nullptr;
    const char16_t* pUtf16Next = nullptr;

    std::codecvt_base::result retCode = converter.out(state,
                                                     reinterpret_cast<const char16_t*>(pSrc),
                                                     reinterpret_cast<const char16_t*>(pSrc)+PalWcslen(pSrc)+1,
                                                     pUtf16Next,
                                                     pDst,
                                                     pDst + dstSizeInBytes,
                                                     pCharNext);
    return (retCode == std::codecvt_base::ok);
}

// =====================================================================================================================
// Perform UTF-16 string copy
void CopyUtf16String(
    wchar_t*       pDst,    ///< [out] Destination string.
    const wchar_t* pSrc,    ///< [in] Source string to copy.
    size_t         dstSize) ///< Length of the destination buffer, in wchar_t's.
{
    memcpy(pDst, pSrc, Util::Min(dstSize*sizeof(char16_t), sizeof(char16_t)*(PalWcslen(pSrc)+1)));
}

// =====================================================================================================================
// This was originally going to just call Snprintf but a custom implementation ended up being easier to write.
size_t BytesToStr(
    char*       pDst,
    size_t      dstSize,
    const void* pBuffer,
    size_t      bufferSize,
    size_t      blockSize)
{
    size_t bufferOffset = 0;

    if ((pDst != nullptr) && (dstSize > 0) && (pBuffer != nullptr) && (blockSize > 0))
    {
        // Start out with a null terminator just in case we don't have space for anything else.
        *pDst = '\0';

        const uint8* pBytes = static_cast<const uint8*>(pBuffer);
        bool wroteBlock = false;

        while (bufferOffset < bufferSize)
        {
            const size_t curBlockSize = Min(blockSize, bufferSize - bufferOffset);
            const size_t blockStrLen  = 3 + curBlockSize * 2; // "0x" + 2 hex chars per byte + a space or null.

            if (blockStrLen > dstSize)
            {
                // No more space for a full block.
                break;
            }

            if (wroteBlock)
            {
                // If we wrote a block previously we should look back and replace the null with a space.
                *(pDst - 1) = ' ';
            }

            *(pDst++) = '0';
            *(pDst++) = 'x';

            // This assumes little endian.
            for (size_t remaining = curBlockSize; remaining > 0;)
            {
                const uint32 byte = pBytes[--remaining];
                const uint32 low  = byte & 0xF;
                const uint32 high = byte >> 4;

                *(pDst++) = (high > 9) ? ('A' + high - 10) : ('0' + high);
                *(pDst++) = (low  > 9) ? ('A' + low - 10)  : ('0' + low);
            }

            // Assume this is the last block we can fit in the string.
            *(pDst++)  = '\0';
            wroteBlock = true;

            dstSize      -= blockStrLen;
            pBytes       += curBlockSize;
            bufferOffset += curBlockSize;
        }
    }

    return bufferOffset;
}

} // Util
