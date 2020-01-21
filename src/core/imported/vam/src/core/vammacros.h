/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2009-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  vammacros.h
* @brief Contains macro definitions used in the VAM project.
***************************************************************************************************
*/

#ifndef __VAMMACROS_H__
#define __VAMMACROS_H__

// True and False boolean values
#ifndef FALSE
    #define FALSE   0
#endif
#ifndef TRUE
    #define TRUE    1
#endif

// Common macros
#define ROUND_UP(value, boundary)       ((value) == 0 ? (boundary) : (((value) + ((boundary)-1)) & (~((boundary)-1))))
#define ROUND_DOWN(value, boundary)     ((value) & (~((boundary)-1)))
#define IS_ALIGNED(value, alignment)    (!((value) & ((alignment) - 1)))
#define POW2(value)                     ((value) && !((value) & ((value)-1)))     // TRUE if value is power of 2

#define VAM_PAGE_SIZE                   4096
#define GLOBAL_ALLOC_ALGMT_SIZE         VAM_PAGE_SIZE
#define SUB_ALLOC_ALGMT_SIZE            256
#define PTE_SIZE_IN_BYTES               8

#if VAM_DEBUG
    #if defined(__GNUC__)
        #include <signal.h>
        #define VAM_DBG_BREAK()         { raise(SIGTRAP); }
    #else
        #define VAM_DBG_BREAK()         { __debugbreak(); }
    #endif
#else
    #define VAM_DBG_BREAK()
#endif

#if defined(__GNUC__)
    #define VAM_ANALYSIS_ASSUME(expr)   ((void)0)
#else
    #define VAM_ANALYSIS_ASSUME(expr)   __analysis_assume(expr)
#endif

#define VAM_ASSERT(__expr)                  \
{                                           \
    VAM_ANALYSIS_ASSUME(__expr);            \
    if ( !((__expr) ? TRUE : FALSE))        \
    {                                       \
        VAM_DBG_BREAK();                    \
    }                                       \
}

#define VAM_ASSERT_ALWAYS()             VAM_DBG_BREAK()

#endif // __VAMMACROS_H__
