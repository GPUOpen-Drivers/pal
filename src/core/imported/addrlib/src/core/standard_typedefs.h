/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2001-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

//****************************************************************************************
//
// TITLE: Standard "typedef"s
//
// DESCRIPTION: This file contains very generic "typedef"s that any library can include.
//
//              PLEASE, UPDATE THIS FILE JUDICIOUSLY.
//
//****************************************************************************************

#ifndef _STANDARD_TYPEDEFS_DEFINED_
#define _STANDARD_TYPEDEFS_DEFINED_

//----------------------------------------------------------------------------------------
// Define sized-based typedefs up to 32-bits.
//----------------------------------------------------------------------------------------
typedef signed char             int8;
typedef unsigned char           uint8;

typedef signed short            int16;
typedef unsigned short          uint16;

typedef signed int              int32;
typedef unsigned int            uint32;

//----------------------------------------------------------------------------------------
// Define 64-bit typedefs, depending on the compiler and operating system.
//----------------------------------------------------------------------------------------
#ifdef __GNUC__
typedef long long               int64;
typedef unsigned long long      uint64;

#else                                        // not __GNUC__
#error Unsupported compiler and/or operating system

#endif

//----------------------------------------------------------------------------------------
// Define other generic typedefs.
//----------------------------------------------------------------------------------------
typedef unsigned int            uint;
typedef unsigned long           ulong;

//****************************************************************************************
// End of _STANDARD_TYPEDEFS_DEFINED_
//****************************************************************************************
#endif
