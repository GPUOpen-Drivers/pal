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

} // Util
