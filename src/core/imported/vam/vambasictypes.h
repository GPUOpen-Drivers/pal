/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
#ifndef _vam_basic_types_h_
#define _vam_basic_types_h_

//
// --------------------------  Autodetect settings ----------------------------
//

// We try to detect if we build for 64-bit or 32-bit OS.
// Note: We assume that we have flat address model, not segment one.
#if defined(_WIN64) || defined(__x86_64__)
#define TARGET_64_OS                    1   // We build for 64-bit target
#elif defined(_WIN32) || defined(__i386__)
#define TARGET_32_OS                    1   // We build for 32-bit target
#elif defined(__aarch64__)
#define TARGET_64_OS                    1   // We build for 64-bit target
#elif defined(__arm__)
#define TARGET_32_OS                    1   // We build for 32-bit target
#else
#error Could not detect if this 32-bit or 64-bit OS
#endif

// Different compiler has different definitions for int8, int16, int32, etc.
#if defined(LINUX)
#include <stdint.h>
#include <stddef.h>
#define DEFINE_MSWIN_DATATYPES        1 // For Linux we need to define datatypes
#elif (defined (BUILDING_CMMQSLIB) || defined(BUILDING_CMM))
#if defined(_WIN32) || defined(_WIN64)
#if !defined(_BASETSD_H_)               // It looks that standard types are not defined
#define DEFINE_MSWIN_DATATYPES        1
#endif                                  // #if !defined(_BASETSD_H_)
#endif                                  // #if defined(_WIN32) || defined(_WIN64)
#endif                                  // #if defined(LINUX)

//
// -----------------  Define standard API calling convention ------------------
//

#if defined(__AMD64__)
// @note 'stdcall' attribute (and cdecl) is not used for AMD64
#define ATIAPI_STDCALL
#else
#define ATIAPI_STDCALL  __attribute__((stdcall))
#endif

//
//
// -----------------------  Packing/alignment support. ------------------------
//

// @note: For Windows we rely on default compiler packing.
//
// @note (1) Some GGC versions of compiler could support
//           Microsoft style packing.\n
//       (2) It was found that GCC compiler could silenty ignore
//           unrecognized pragmas (e.g. #pragma pack(..)).
#define GCC_PACK_STRUCT __attribute__((packed))
#ifdef TARGET_32_OS
#define GCC_STRUCT_DEFAULT_ALIGN  __attribute__ ((aligned (8)))
#else                                   // #ifdef TARGET_32_OS
#define GCC_STRUCT_DEFAULT_ALIGN  __attribute__ ((aligned (8)))
#endif                                  // #ifdef TARGET_32_OS
#define GCC_STRUCT_ALIGN(a)  __attribute__ ((aligned (a)))

//
// ------------------  Emulate MS Windows build environment -------------------
//

#ifdef DEFINE_MSWIN_DATATYPES

typedef char                CHAR;
typedef char         *      PCHAR;
typedef wchar_t *           PWCHAR;
typedef unsigned char       UCHAR;
typedef unsigned char *     PUCHAR;
typedef void                VOID;
typedef void *              PVOID;

typedef uint16_t            USHORT;
typedef int16_t             SHORT;

typedef uint32_t            UINT;
typedef uint32_t            UINT32;
typedef uint32_t            DWORD;
typedef uint32_t       *    PDWORD;
typedef uint32_t            DWORD32;
typedef uint32_t            ULONG;
typedef uint32_t            ULONG32;
typedef uint32_t       *    PULONG;
typedef int32_t             LONG;
typedef uint64_t            DWORD64;
typedef uint64_t            UINT64;
typedef int64_t             LONGLONG;
typedef uint64_t            ULONGLONG;

typedef USHORT              *PUSHORT;
typedef SHORT               *PSHORT;
typedef LONGLONG            *PLONGLONG;
typedef void                *LPVOID;
typedef LONG                *PLONG;
typedef char                *LPSTR;

typedef float               FLOAT;

typedef int32_t             BOOL;
typedef uint8_t             BYTE;
typedef uint8_t             *PBYTE;
typedef int32_t             INT32;

typedef int32_t             BOOLEAN;

#define FALSE               0
#define TRUE                1
typedef void   *            HANDLE;

#ifdef TARGET_32_OS
typedef uint32_t            DWORD_PTR;
typedef uint32_t            ULONG_PTR;
typedef uint32_t            UINT_PTR;
#else                                       // #ifdef TARGET_32_OS
typedef uint64_t            DWORD_PTR;
typedef uint64_t            ULONG_PTR;
typedef uint64_t            UINT_PTR;
#endif                                      // #ifdef TARGET_32_OS

#define DWORD_PTR           DWORD_PTR
#define ULONG_PTR           ULONG_PTR
#define UINT_PTR            UINT_PTR

#define MAXLONGLONG         (0x7fffffffffffffffLL)

#endif                                      // #ifdef DEFINE_MSWIN_DATATYPES

#endif                                      // #ifdef _vam_basic_types_h_

