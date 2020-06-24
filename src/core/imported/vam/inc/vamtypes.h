/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
***************************************************************************************************
* @file  vamtypes.h
* @brief Contains the helper function and constants
***************************************************************************************************
*/
#ifndef __VAMTYPES_H__
#define __VAMTYPES_H__

#if defined(__GNUC__)
    #include <vambasictypes.h>
#if defined(VAM_CLIENT_DX12_LINUX)
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN 1
#endif
    #include <windows.h>
#endif
#else
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN 1
#endif
    #include <windows.h>
#endif

/**
***************************************************************************************************
*   Calling conventions
***************************************************************************************************
*/
#ifndef VAM_STDCALL
    #if defined(__GNUC__)
        #if defined(__amd64__) || defined(__x86_64__)
            #define VAM_STDCALL
        #else
            #define VAM_STDCALL __attribute__((stdcall))
        #endif
    #else
        #define VAM_STDCALL __stdcall
    #endif
#endif

#ifndef VAM_FASTCALL
    #if defined(__GNUC__)
        #define VAM_FASTCALL __attribute__((regparm(0)))
    #else
        #define VAM_FASTCALL __fastcall
    #endif
#endif

#if defined(__GNUC__)
    #define VAM_INLINE   inline
#else
    // win32, win64, other platforms
    #define VAM_INLINE   __inline
#endif

#define VAM_API VAM_STDCALL

/**
***************************************************************************************************
* Return codes
***************************************************************************************************
*/
typedef enum _VAM_RETURNCODE
{
    /// General Return
    VAM_OK                  = 0,
    VAM_ERROR               = 1,
    VAM_INVALIDPARAMETERS   = 2,

    /// Specific Errors
    VAM_VIRTUALADDRESSCONFLICT,
    VAM_FRAGMENTALLOCFAILED,
    VAM_OPTIONALVANOTFRAGMENTALIGNED,
    VAM_RAFTNOTEMPTY,
    VAM_SECTIONNOTEMPTY,
    VAM_OUTOFMEMORY,
    VAM_PTBALLOCFAILED

} VAM_RETURNCODE;

/**
***************************************************************************************************
* Handle definitions
***************************************************************************************************
*/
/// Required as a first parameter to nearly all interface functions
typedef VOID*   VAM_HANDLE;

/// Client handle used as first parameter in all callbacks
typedef VOID*   VAM_CLIENT_HANDLE;

/// Section handle used to identify the sections
typedef VOID*   VAM_SECTION_HANDLE;

/// Raft handle used to identify the rafts
typedef VOID*   VAM_RAFT_HANDLE;

/// Video memory allocation handle
typedef VOID*   VAM_VIDMEM_HANDLE;

/// Synchronizaiton object handle
typedef VOID*   VAM_SYNCOBJECT_HANDLE;

/// Page Table Block (PTB) allocation handle
typedef VOID*   VAM_PTB_HANDLE;

/// Opaque client object handle
typedef VOID*   VAM_CLIENT_OBJECT;

/// Allocation tracker handle
typedef VOID*   VAM_ALLOCATION_HANDLE;

/**
***************************************************************************************************
* VAM-specific definitions
***************************************************************************************************
*/
typedef unsigned long long VAM_VIRTUAL_ADDRESS;
typedef unsigned long long VAM_VA_SIZE;

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Macros for controlling linking and building on multiple systems
//
///////////////////////////////////////////////////////////////////////////////////////////////////

// If we are compiling in c++, the extern definitions need to be different
// Also, make static routines inline in C++ (eliminates error messages)
// Keep these defines only for unit test
#ifdef __cplusplus
#define VAM_EXTERN    extern "C"
#define VAM_STATIC    static inline
#else
#define VAM_EXTERN    extern
#define VAM_STATIC    static
#endif

#endif
