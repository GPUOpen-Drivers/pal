/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2007-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
************************************************************************************************************************
* @file  gfx10SwizzlePattern.h
* @brief swizzle pattern for gfx10.
************************************************************************************************************************
*/

#ifndef __GFX10_SWIZZLE_PATTERN_H__
#define __GFX10_SWIZZLE_PATTERN_H__

namespace Addr
{
namespace V2
{
/**
************************************************************************************************************************
* @brief Bit setting for swizzle pattern
************************************************************************************************************************
*/
union ADDR_BIT_SETTING
{
    struct
    {
        UINT_16 x;
        UINT_16 y;
        UINT_16 z;
        UINT_16 s;
    };
    UINT_64 value;
};

/**
************************************************************************************************************************
* @brief Swizzle pattern information
************************************************************************************************************************
*/
struct ADDR_SW_PATINFO
{
    UINT_8  maxItemCount;
    UINT_8  nibble01Idx;
    UINT_16 nibble2Idx;
    UINT_16 nibble3Idx;
};

/**
************************************************************************************************************************
*   InitBit
*
*   @brief
*       Initialize bit setting value via a return value
************************************************************************************************************************
*/
#define InitBit(c, index) (1ull << ((c << 4) + index))

const UINT_64 X0  = InitBit(0,  0);
const UINT_64 X1  = InitBit(0,  1);
const UINT_64 X2  = InitBit(0,  2);
const UINT_64 X3  = InitBit(0,  3);
const UINT_64 X4  = InitBit(0,  4);
const UINT_64 X5  = InitBit(0,  5);
const UINT_64 X6  = InitBit(0,  6);
const UINT_64 X7  = InitBit(0,  7);
const UINT_64 X8  = InitBit(0,  8);
const UINT_64 X9  = InitBit(0,  9);
const UINT_64 X10 = InitBit(0, 10);
const UINT_64 X11 = InitBit(0, 11);

const UINT_64 Y0  = InitBit(1,  0);
const UINT_64 Y1  = InitBit(1,  1);
const UINT_64 Y2  = InitBit(1,  2);
const UINT_64 Y3  = InitBit(1,  3);
const UINT_64 Y4  = InitBit(1,  4);
const UINT_64 Y5  = InitBit(1,  5);
const UINT_64 Y6  = InitBit(1,  6);
const UINT_64 Y7  = InitBit(1,  7);
const UINT_64 Y8  = InitBit(1,  8);
const UINT_64 Y9  = InitBit(1,  9);
const UINT_64 Y10 = InitBit(1, 10);
const UINT_64 Y11 = InitBit(1, 11);

const UINT_64 Z0  = InitBit(2,  0);
const UINT_64 Z1  = InitBit(2,  1);
const UINT_64 Z2  = InitBit(2,  2);
const UINT_64 Z3  = InitBit(2,  3);
const UINT_64 Z4  = InitBit(2,  4);
const UINT_64 Z5  = InitBit(2,  5);
const UINT_64 Z6  = InitBit(2,  6);
const UINT_64 Z7  = InitBit(2,  7);
const UINT_64 Z8  = InitBit(2,  8);

const UINT_64 S0  = InitBit(3,  0);
const UINT_64 S1  = InitBit(3,  1);
const UINT_64 S2  = InitBit(3,  2);

const ADDR_SW_PATINFO SW_256_S_PATINFO[] =
{
    {   1,    0,    0,    0} , // 1 pipes 1 bpe @ SW_256_S @ Navi1x
    {   1,    1,    0,    0} , // 1 pipes 2 bpe @ SW_256_S @ Navi1x
    {   1,    2,    0,    0} , // 1 pipes 4 bpe @ SW_256_S @ Navi1x
    {   1,    3,    0,    0} , // 1 pipes 8 bpe @ SW_256_S @ Navi1x
    {   1,    4,    0,    0} , // 1 pipes 16 bpe @ SW_256_S @ Navi1x
    {   1,    0,    0,    0} , // 2 pipes 1 bpe @ SW_256_S @ Navi1x
    {   1,    1,    0,    0} , // 2 pipes 2 bpe @ SW_256_S @ Navi1x
    {   1,    2,    0,    0} , // 2 pipes 4 bpe @ SW_256_S @ Navi1x
    {   1,    3,    0,    0} , // 2 pipes 8 bpe @ SW_256_S @ Navi1x
    {   1,    4,    0,    0} , // 2 pipes 16 bpe @ SW_256_S @ Navi1x
    {   1,    0,    0,    0} , // 4 pipes 1 bpe @ SW_256_S @ Navi1x
    {   1,    1,    0,    0} , // 4 pipes 2 bpe @ SW_256_S @ Navi1x
    {   1,    2,    0,    0} , // 4 pipes 4 bpe @ SW_256_S @ Navi1x
    {   1,    3,    0,    0} , // 4 pipes 8 bpe @ SW_256_S @ Navi1x
    {   1,    4,    0,    0} , // 4 pipes 16 bpe @ SW_256_S @ Navi1x
    {   1,    0,    0,    0} , // 8 pipes 1 bpe @ SW_256_S @ Navi1x
    {   1,    1,    0,    0} , // 8 pipes 2 bpe @ SW_256_S @ Navi1x
    {   1,    2,    0,    0} , // 8 pipes 4 bpe @ SW_256_S @ Navi1x
    {   1,    3,    0,    0} , // 8 pipes 8 bpe @ SW_256_S @ Navi1x
    {   1,    4,    0,    0} , // 8 pipes 16 bpe @ SW_256_S @ Navi1x
    {   1,    0,    0,    0} , // 16 pipes 1 bpe @ SW_256_S @ Navi1x
    {   1,    1,    0,    0} , // 16 pipes 2 bpe @ SW_256_S @ Navi1x
    {   1,    2,    0,    0} , // 16 pipes 4 bpe @ SW_256_S @ Navi1x
    {   1,    3,    0,    0} , // 16 pipes 8 bpe @ SW_256_S @ Navi1x
    {   1,    4,    0,    0} , // 16 pipes 16 bpe @ SW_256_S @ Navi1x
    {   1,    0,    0,    0} , // 32 pipes 1 bpe @ SW_256_S @ Navi1x
    {   1,    1,    0,    0} , // 32 pipes 2 bpe @ SW_256_S @ Navi1x
    {   1,    2,    0,    0} , // 32 pipes 4 bpe @ SW_256_S @ Navi1x
    {   1,    3,    0,    0} , // 32 pipes 8 bpe @ SW_256_S @ Navi1x
    {   1,    4,    0,    0} , // 32 pipes 16 bpe @ SW_256_S @ Navi1x
    {   1,    0,    0,    0} , // 64 pipes 1 bpe @ SW_256_S @ Navi1x
    {   1,    1,    0,    0} , // 64 pipes 2 bpe @ SW_256_S @ Navi1x
    {   1,    2,    0,    0} , // 64 pipes 4 bpe @ SW_256_S @ Navi1x
    {   1,    3,    0,    0} , // 64 pipes 8 bpe @ SW_256_S @ Navi1x
    {   1,    4,    0,    0} , // 64 pipes 16 bpe @ SW_256_S @ Navi1x
};

const ADDR_SW_PATINFO SW_256_D_PATINFO[] =
{
    {   1,    5,    0,    0} , // 1 pipes 1 bpe @ SW_256_D @ Navi1x
    {   1,    1,    0,    0} , // 1 pipes 2 bpe @ SW_256_D @ Navi1x
    {   1,    2,    0,    0} , // 1 pipes 4 bpe @ SW_256_D @ Navi1x
    {   1,    6,    0,    0} , // 1 pipes 8 bpe @ SW_256_D @ Navi1x
    {   1,    7,    0,    0} , // 1 pipes 16 bpe @ SW_256_D @ Navi1x
    {   1,    5,    0,    0} , // 2 pipes 1 bpe @ SW_256_D @ Navi1x
    {   1,    1,    0,    0} , // 2 pipes 2 bpe @ SW_256_D @ Navi1x
    {   1,    2,    0,    0} , // 2 pipes 4 bpe @ SW_256_D @ Navi1x
    {   1,    6,    0,    0} , // 2 pipes 8 bpe @ SW_256_D @ Navi1x
    {   1,    7,    0,    0} , // 2 pipes 16 bpe @ SW_256_D @ Navi1x
    {   1,    5,    0,    0} , // 4 pipes 1 bpe @ SW_256_D @ Navi1x
    {   1,    1,    0,    0} , // 4 pipes 2 bpe @ SW_256_D @ Navi1x
    {   1,    2,    0,    0} , // 4 pipes 4 bpe @ SW_256_D @ Navi1x
    {   1,    6,    0,    0} , // 4 pipes 8 bpe @ SW_256_D @ Navi1x
    {   1,    7,    0,    0} , // 4 pipes 16 bpe @ SW_256_D @ Navi1x
    {   1,    5,    0,    0} , // 8 pipes 1 bpe @ SW_256_D @ Navi1x
    {   1,    1,    0,    0} , // 8 pipes 2 bpe @ SW_256_D @ Navi1x
    {   1,    2,    0,    0} , // 8 pipes 4 bpe @ SW_256_D @ Navi1x
    {   1,    6,    0,    0} , // 8 pipes 8 bpe @ SW_256_D @ Navi1x
    {   1,    7,    0,    0} , // 8 pipes 16 bpe @ SW_256_D @ Navi1x
    {   1,    5,    0,    0} , // 16 pipes 1 bpe @ SW_256_D @ Navi1x
    {   1,    1,    0,    0} , // 16 pipes 2 bpe @ SW_256_D @ Navi1x
    {   1,    2,    0,    0} , // 16 pipes 4 bpe @ SW_256_D @ Navi1x
    {   1,    6,    0,    0} , // 16 pipes 8 bpe @ SW_256_D @ Navi1x
    {   1,    7,    0,    0} , // 16 pipes 16 bpe @ SW_256_D @ Navi1x
    {   1,    5,    0,    0} , // 32 pipes 1 bpe @ SW_256_D @ Navi1x
    {   1,    1,    0,    0} , // 32 pipes 2 bpe @ SW_256_D @ Navi1x
    {   1,    2,    0,    0} , // 32 pipes 4 bpe @ SW_256_D @ Navi1x
    {   1,    6,    0,    0} , // 32 pipes 8 bpe @ SW_256_D @ Navi1x
    {   1,    7,    0,    0} , // 32 pipes 16 bpe @ SW_256_D @ Navi1x
    {   1,    5,    0,    0} , // 64 pipes 1 bpe @ SW_256_D @ Navi1x
    {   1,    1,    0,    0} , // 64 pipes 2 bpe @ SW_256_D @ Navi1x
    {   1,    2,    0,    0} , // 64 pipes 4 bpe @ SW_256_D @ Navi1x
    {   1,    6,    0,    0} , // 64 pipes 8 bpe @ SW_256_D @ Navi1x
    {   1,    7,    0,    0} , // 64 pipes 16 bpe @ SW_256_D @ Navi1x
};

const ADDR_SW_PATINFO SW_4K_S_PATINFO[] =
{
    {   1,    0,    1,    0} , // 1 pipes 1 bpe @ SW_4K_S @ Navi1x
    {   1,    1,    2,    0} , // 1 pipes 2 bpe @ SW_4K_S @ Navi1x
    {   1,    2,    3,    0} , // 1 pipes 4 bpe @ SW_4K_S @ Navi1x
    {   1,    3,    4,    0} , // 1 pipes 8 bpe @ SW_4K_S @ Navi1x
    {   1,    4,    5,    0} , // 1 pipes 16 bpe @ SW_4K_S @ Navi1x
    {   1,    0,    1,    0} , // 2 pipes 1 bpe @ SW_4K_S @ Navi1x
    {   1,    1,    2,    0} , // 2 pipes 2 bpe @ SW_4K_S @ Navi1x
    {   1,    2,    3,    0} , // 2 pipes 4 bpe @ SW_4K_S @ Navi1x
    {   1,    3,    4,    0} , // 2 pipes 8 bpe @ SW_4K_S @ Navi1x
    {   1,    4,    5,    0} , // 2 pipes 16 bpe @ SW_4K_S @ Navi1x
    {   1,    0,    1,    0} , // 4 pipes 1 bpe @ SW_4K_S @ Navi1x
    {   1,    1,    2,    0} , // 4 pipes 2 bpe @ SW_4K_S @ Navi1x
    {   1,    2,    3,    0} , // 4 pipes 4 bpe @ SW_4K_S @ Navi1x
    {   1,    3,    4,    0} , // 4 pipes 8 bpe @ SW_4K_S @ Navi1x
    {   1,    4,    5,    0} , // 4 pipes 16 bpe @ SW_4K_S @ Navi1x
    {   1,    0,    1,    0} , // 8 pipes 1 bpe @ SW_4K_S @ Navi1x
    {   1,    1,    2,    0} , // 8 pipes 2 bpe @ SW_4K_S @ Navi1x
    {   1,    2,    3,    0} , // 8 pipes 4 bpe @ SW_4K_S @ Navi1x
    {   1,    3,    4,    0} , // 8 pipes 8 bpe @ SW_4K_S @ Navi1x
    {   1,    4,    5,    0} , // 8 pipes 16 bpe @ SW_4K_S @ Navi1x
    {   1,    0,    1,    0} , // 16 pipes 1 bpe @ SW_4K_S @ Navi1x
    {   1,    1,    2,    0} , // 16 pipes 2 bpe @ SW_4K_S @ Navi1x
    {   1,    2,    3,    0} , // 16 pipes 4 bpe @ SW_4K_S @ Navi1x
    {   1,    3,    4,    0} , // 16 pipes 8 bpe @ SW_4K_S @ Navi1x
    {   1,    4,    5,    0} , // 16 pipes 16 bpe @ SW_4K_S @ Navi1x
    {   1,    0,    1,    0} , // 32 pipes 1 bpe @ SW_4K_S @ Navi1x
    {   1,    1,    2,    0} , // 32 pipes 2 bpe @ SW_4K_S @ Navi1x
    {   1,    2,    3,    0} , // 32 pipes 4 bpe @ SW_4K_S @ Navi1x
    {   1,    3,    4,    0} , // 32 pipes 8 bpe @ SW_4K_S @ Navi1x
    {   1,    4,    5,    0} , // 32 pipes 16 bpe @ SW_4K_S @ Navi1x
    {   1,    0,    1,    0} , // 64 pipes 1 bpe @ SW_4K_S @ Navi1x
    {   1,    1,    2,    0} , // 64 pipes 2 bpe @ SW_4K_S @ Navi1x
    {   1,    2,    3,    0} , // 64 pipes 4 bpe @ SW_4K_S @ Navi1x
    {   1,    3,    4,    0} , // 64 pipes 8 bpe @ SW_4K_S @ Navi1x
    {   1,    4,    5,    0} , // 64 pipes 16 bpe @ SW_4K_S @ Navi1x
};

const ADDR_SW_PATINFO SW_4K_D_PATINFO[] =
{
    {   1,    5,    1,    0} , // 1 pipes 1 bpe @ SW_4K_D @ Navi1x
    {   1,    1,    2,    0} , // 1 pipes 2 bpe @ SW_4K_D @ Navi1x
    {   1,    2,    3,    0} , // 1 pipes 4 bpe @ SW_4K_D @ Navi1x
    {   1,    6,    4,    0} , // 1 pipes 8 bpe @ SW_4K_D @ Navi1x
    {   1,    7,    5,    0} , // 1 pipes 16 bpe @ SW_4K_D @ Navi1x
    {   1,    5,    1,    0} , // 2 pipes 1 bpe @ SW_4K_D @ Navi1x
    {   1,    1,    2,    0} , // 2 pipes 2 bpe @ SW_4K_D @ Navi1x
    {   1,    2,    3,    0} , // 2 pipes 4 bpe @ SW_4K_D @ Navi1x
    {   1,    6,    4,    0} , // 2 pipes 8 bpe @ SW_4K_D @ Navi1x
    {   1,    7,    5,    0} , // 2 pipes 16 bpe @ SW_4K_D @ Navi1x
    {   1,    5,    1,    0} , // 4 pipes 1 bpe @ SW_4K_D @ Navi1x
    {   1,    1,    2,    0} , // 4 pipes 2 bpe @ SW_4K_D @ Navi1x
    {   1,    2,    3,    0} , // 4 pipes 4 bpe @ SW_4K_D @ Navi1x
    {   1,    6,    4,    0} , // 4 pipes 8 bpe @ SW_4K_D @ Navi1x
    {   1,    7,    5,    0} , // 4 pipes 16 bpe @ SW_4K_D @ Navi1x
    {   1,    5,    1,    0} , // 8 pipes 1 bpe @ SW_4K_D @ Navi1x
    {   1,    1,    2,    0} , // 8 pipes 2 bpe @ SW_4K_D @ Navi1x
    {   1,    2,    3,    0} , // 8 pipes 4 bpe @ SW_4K_D @ Navi1x
    {   1,    6,    4,    0} , // 8 pipes 8 bpe @ SW_4K_D @ Navi1x
    {   1,    7,    5,    0} , // 8 pipes 16 bpe @ SW_4K_D @ Navi1x
    {   1,    5,    1,    0} , // 16 pipes 1 bpe @ SW_4K_D @ Navi1x
    {   1,    1,    2,    0} , // 16 pipes 2 bpe @ SW_4K_D @ Navi1x
    {   1,    2,    3,    0} , // 16 pipes 4 bpe @ SW_4K_D @ Navi1x
    {   1,    6,    4,    0} , // 16 pipes 8 bpe @ SW_4K_D @ Navi1x
    {   1,    7,    5,    0} , // 16 pipes 16 bpe @ SW_4K_D @ Navi1x
    {   1,    5,    1,    0} , // 32 pipes 1 bpe @ SW_4K_D @ Navi1x
    {   1,    1,    2,    0} , // 32 pipes 2 bpe @ SW_4K_D @ Navi1x
    {   1,    2,    3,    0} , // 32 pipes 4 bpe @ SW_4K_D @ Navi1x
    {   1,    6,    4,    0} , // 32 pipes 8 bpe @ SW_4K_D @ Navi1x
    {   1,    7,    5,    0} , // 32 pipes 16 bpe @ SW_4K_D @ Navi1x
    {   1,    5,    1,    0} , // 64 pipes 1 bpe @ SW_4K_D @ Navi1x
    {   1,    1,    2,    0} , // 64 pipes 2 bpe @ SW_4K_D @ Navi1x
    {   1,    2,    3,    0} , // 64 pipes 4 bpe @ SW_4K_D @ Navi1x
    {   1,    6,    4,    0} , // 64 pipes 8 bpe @ SW_4K_D @ Navi1x
    {   1,    7,    5,    0} , // 64 pipes 16 bpe @ SW_4K_D @ Navi1x
};

const ADDR_SW_PATINFO SW_4K_S_X_PATINFO[] =
{
    {   1,    0,    1,    0} , // 1 pipes 1 bpe @ SW_4K_S_X @ Navi1x
    {   1,    1,    2,    0} , // 1 pipes 2 bpe @ SW_4K_S_X @ Navi1x
    {   1,    2,    3,    0} , // 1 pipes 4 bpe @ SW_4K_S_X @ Navi1x
    {   1,    3,    4,    0} , // 1 pipes 8 bpe @ SW_4K_S_X @ Navi1x
    {   1,    4,    5,    0} , // 1 pipes 16 bpe @ SW_4K_S_X @ Navi1x
    {   3,    0,    6,    0} , // 2 pipes 1 bpe @ SW_4K_S_X @ Navi1x
    {   3,    1,    7,    0} , // 2 pipes 2 bpe @ SW_4K_S_X @ Navi1x
    {   3,    2,    8,    0} , // 2 pipes 4 bpe @ SW_4K_S_X @ Navi1x
    {   3,    3,    9,    0} , // 2 pipes 8 bpe @ SW_4K_S_X @ Navi1x
    {   3,    4,   10,    0} , // 2 pipes 16 bpe @ SW_4K_S_X @ Navi1x
    {   3,    0,   11,    0} , // 4 pipes 1 bpe @ SW_4K_S_X @ Navi1x
    {   3,    1,   12,    0} , // 4 pipes 2 bpe @ SW_4K_S_X @ Navi1x
    {   3,    2,   13,    0} , // 4 pipes 4 bpe @ SW_4K_S_X @ Navi1x
    {   3,    3,   14,    0} , // 4 pipes 8 bpe @ SW_4K_S_X @ Navi1x
    {   3,    4,   15,    0} , // 4 pipes 16 bpe @ SW_4K_S_X @ Navi1x
    {   3,    0,   16,    0} , // 8 pipes 1 bpe @ SW_4K_S_X @ Navi1x
    {   3,    1,   17,    0} , // 8 pipes 2 bpe @ SW_4K_S_X @ Navi1x
    {   3,    2,   18,    0} , // 8 pipes 4 bpe @ SW_4K_S_X @ Navi1x
    {   3,    3,   19,    0} , // 8 pipes 8 bpe @ SW_4K_S_X @ Navi1x
    {   3,    4,   20,    0} , // 8 pipes 16 bpe @ SW_4K_S_X @ Navi1x
    {   3,    0,   21,    0} , // 16 pipes 1 bpe @ SW_4K_S_X @ Navi1x
    {   3,    1,   22,    0} , // 16 pipes 2 bpe @ SW_4K_S_X @ Navi1x
    {   3,    2,   23,    0} , // 16 pipes 4 bpe @ SW_4K_S_X @ Navi1x
    {   3,    3,   24,    0} , // 16 pipes 8 bpe @ SW_4K_S_X @ Navi1x
    {   3,    4,   25,    0} , // 16 pipes 16 bpe @ SW_4K_S_X @ Navi1x
    {   3,    0,   21,    0} , // 32 pipes 1 bpe @ SW_4K_S_X @ Navi1x
    {   3,    1,   22,    0} , // 32 pipes 2 bpe @ SW_4K_S_X @ Navi1x
    {   3,    2,   23,    0} , // 32 pipes 4 bpe @ SW_4K_S_X @ Navi1x
    {   3,    3,   24,    0} , // 32 pipes 8 bpe @ SW_4K_S_X @ Navi1x
    {   3,    4,   25,    0} , // 32 pipes 16 bpe @ SW_4K_S_X @ Navi1x
    {   3,    0,   21,    0} , // 64 pipes 1 bpe @ SW_4K_S_X @ Navi1x
    {   3,    1,   22,    0} , // 64 pipes 2 bpe @ SW_4K_S_X @ Navi1x
    {   3,    2,   23,    0} , // 64 pipes 4 bpe @ SW_4K_S_X @ Navi1x
    {   3,    3,   24,    0} , // 64 pipes 8 bpe @ SW_4K_S_X @ Navi1x
    {   3,    4,   25,    0} , // 64 pipes 16 bpe @ SW_4K_S_X @ Navi1x
};

const ADDR_SW_PATINFO SW_4K_D_X_PATINFO[] =
{
    {   1,    5,    1,    0} , // 1 pipes 1 bpe @ SW_4K_D_X @ Navi1x
    {   1,    1,    2,    0} , // 1 pipes 2 bpe @ SW_4K_D_X @ Navi1x
    {   1,    2,    3,    0} , // 1 pipes 4 bpe @ SW_4K_D_X @ Navi1x
    {   1,    6,    4,    0} , // 1 pipes 8 bpe @ SW_4K_D_X @ Navi1x
    {   1,    7,    5,    0} , // 1 pipes 16 bpe @ SW_4K_D_X @ Navi1x
    {   3,    5,    6,    0} , // 2 pipes 1 bpe @ SW_4K_D_X @ Navi1x
    {   3,    1,    7,    0} , // 2 pipes 2 bpe @ SW_4K_D_X @ Navi1x
    {   3,    2,    8,    0} , // 2 pipes 4 bpe @ SW_4K_D_X @ Navi1x
    {   3,    6,    9,    0} , // 2 pipes 8 bpe @ SW_4K_D_X @ Navi1x
    {   3,    7,   10,    0} , // 2 pipes 16 bpe @ SW_4K_D_X @ Navi1x
    {   3,    5,   11,    0} , // 4 pipes 1 bpe @ SW_4K_D_X @ Navi1x
    {   3,    1,   12,    0} , // 4 pipes 2 bpe @ SW_4K_D_X @ Navi1x
    {   3,    2,   13,    0} , // 4 pipes 4 bpe @ SW_4K_D_X @ Navi1x
    {   3,    6,   14,    0} , // 4 pipes 8 bpe @ SW_4K_D_X @ Navi1x
    {   3,    7,   15,    0} , // 4 pipes 16 bpe @ SW_4K_D_X @ Navi1x
    {   3,    5,   16,    0} , // 8 pipes 1 bpe @ SW_4K_D_X @ Navi1x
    {   3,    1,   17,    0} , // 8 pipes 2 bpe @ SW_4K_D_X @ Navi1x
    {   3,    2,   18,    0} , // 8 pipes 4 bpe @ SW_4K_D_X @ Navi1x
    {   3,    6,   19,    0} , // 8 pipes 8 bpe @ SW_4K_D_X @ Navi1x
    {   3,    7,   20,    0} , // 8 pipes 16 bpe @ SW_4K_D_X @ Navi1x
    {   3,    5,   21,    0} , // 16 pipes 1 bpe @ SW_4K_D_X @ Navi1x
    {   3,    1,   22,    0} , // 16 pipes 2 bpe @ SW_4K_D_X @ Navi1x
    {   3,    2,   23,    0} , // 16 pipes 4 bpe @ SW_4K_D_X @ Navi1x
    {   3,    6,   24,    0} , // 16 pipes 8 bpe @ SW_4K_D_X @ Navi1x
    {   3,    7,   25,    0} , // 16 pipes 16 bpe @ SW_4K_D_X @ Navi1x
    {   3,    5,   21,    0} , // 32 pipes 1 bpe @ SW_4K_D_X @ Navi1x
    {   3,    1,   22,    0} , // 32 pipes 2 bpe @ SW_4K_D_X @ Navi1x
    {   3,    2,   23,    0} , // 32 pipes 4 bpe @ SW_4K_D_X @ Navi1x
    {   3,    6,   24,    0} , // 32 pipes 8 bpe @ SW_4K_D_X @ Navi1x
    {   3,    7,   25,    0} , // 32 pipes 16 bpe @ SW_4K_D_X @ Navi1x
    {   3,    5,   21,    0} , // 64 pipes 1 bpe @ SW_4K_D_X @ Navi1x
    {   3,    1,   22,    0} , // 64 pipes 2 bpe @ SW_4K_D_X @ Navi1x
    {   3,    2,   23,    0} , // 64 pipes 4 bpe @ SW_4K_D_X @ Navi1x
    {   3,    6,   24,    0} , // 64 pipes 8 bpe @ SW_4K_D_X @ Navi1x
    {   3,    7,   25,    0} , // 64 pipes 16 bpe @ SW_4K_D_X @ Navi1x
};

const ADDR_SW_PATINFO SW_4K_S3_PATINFO[] =
{
    {   1,   29,  131,    0} , // 1 pipes 1 bpe @ SW_4K_S3 @ Navi1x
    {   1,   30,  132,    0} , // 1 pipes 2 bpe @ SW_4K_S3 @ Navi1x
    {   1,   31,  133,    0} , // 1 pipes 4 bpe @ SW_4K_S3 @ Navi1x
    {   1,   32,  134,    0} , // 1 pipes 8 bpe @ SW_4K_S3 @ Navi1x
    {   1,   33,  135,    0} , // 1 pipes 16 bpe @ SW_4K_S3 @ Navi1x
    {   1,   29,  131,    0} , // 2 pipes 1 bpe @ SW_4K_S3 @ Navi1x
    {   1,   30,  132,    0} , // 2 pipes 2 bpe @ SW_4K_S3 @ Navi1x
    {   1,   31,  133,    0} , // 2 pipes 4 bpe @ SW_4K_S3 @ Navi1x
    {   1,   32,  134,    0} , // 2 pipes 8 bpe @ SW_4K_S3 @ Navi1x
    {   1,   33,  135,    0} , // 2 pipes 16 bpe @ SW_4K_S3 @ Navi1x
    {   1,   29,  131,    0} , // 4 pipes 1 bpe @ SW_4K_S3 @ Navi1x
    {   1,   30,  132,    0} , // 4 pipes 2 bpe @ SW_4K_S3 @ Navi1x
    {   1,   31,  133,    0} , // 4 pipes 4 bpe @ SW_4K_S3 @ Navi1x
    {   1,   32,  134,    0} , // 4 pipes 8 bpe @ SW_4K_S3 @ Navi1x
    {   1,   33,  135,    0} , // 4 pipes 16 bpe @ SW_4K_S3 @ Navi1x
    {   1,   29,  131,    0} , // 8 pipes 1 bpe @ SW_4K_S3 @ Navi1x
    {   1,   30,  132,    0} , // 8 pipes 2 bpe @ SW_4K_S3 @ Navi1x
    {   1,   31,  133,    0} , // 8 pipes 4 bpe @ SW_4K_S3 @ Navi1x
    {   1,   32,  134,    0} , // 8 pipes 8 bpe @ SW_4K_S3 @ Navi1x
    {   1,   33,  135,    0} , // 8 pipes 16 bpe @ SW_4K_S3 @ Navi1x
    {   1,   29,  131,    0} , // 16 pipes 1 bpe @ SW_4K_S3 @ Navi1x
    {   1,   30,  132,    0} , // 16 pipes 2 bpe @ SW_4K_S3 @ Navi1x
    {   1,   31,  133,    0} , // 16 pipes 4 bpe @ SW_4K_S3 @ Navi1x
    {   1,   32,  134,    0} , // 16 pipes 8 bpe @ SW_4K_S3 @ Navi1x
    {   1,   33,  135,    0} , // 16 pipes 16 bpe @ SW_4K_S3 @ Navi1x
    {   1,   29,  131,    0} , // 32 pipes 1 bpe @ SW_4K_S3 @ Navi1x
    {   1,   30,  132,    0} , // 32 pipes 2 bpe @ SW_4K_S3 @ Navi1x
    {   1,   31,  133,    0} , // 32 pipes 4 bpe @ SW_4K_S3 @ Navi1x
    {   1,   32,  134,    0} , // 32 pipes 8 bpe @ SW_4K_S3 @ Navi1x
    {   1,   33,  135,    0} , // 32 pipes 16 bpe @ SW_4K_S3 @ Navi1x
    {   1,   29,  131,    0} , // 64 pipes 1 bpe @ SW_4K_S3 @ Navi1x
    {   1,   30,  132,    0} , // 64 pipes 2 bpe @ SW_4K_S3 @ Navi1x
    {   1,   31,  133,    0} , // 64 pipes 4 bpe @ SW_4K_S3 @ Navi1x
    {   1,   32,  134,    0} , // 64 pipes 8 bpe @ SW_4K_S3 @ Navi1x
    {   1,   33,  135,    0} , // 64 pipes 16 bpe @ SW_4K_S3 @ Navi1x
};

const ADDR_SW_PATINFO SW_4K_S3_X_PATINFO[] =
{
    {   1,   29,  131,    0} , // 1 pipes 1 bpe @ SW_4K_S3_X @ Navi1x
    {   1,   30,  132,    0} , // 1 pipes 2 bpe @ SW_4K_S3_X @ Navi1x
    {   1,   31,  133,    0} , // 1 pipes 4 bpe @ SW_4K_S3_X @ Navi1x
    {   1,   32,  134,    0} , // 1 pipes 8 bpe @ SW_4K_S3_X @ Navi1x
    {   1,   33,  135,    0} , // 1 pipes 16 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   29,  136,    0} , // 2 pipes 1 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   30,  137,    0} , // 2 pipes 2 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   31,  138,    0} , // 2 pipes 4 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   32,  139,    0} , // 2 pipes 8 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   33,  140,    0} , // 2 pipes 16 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   29,  141,    0} , // 4 pipes 1 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   30,  142,    0} , // 4 pipes 2 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   31,  143,    0} , // 4 pipes 4 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   32,  144,    0} , // 4 pipes 8 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   33,  145,    0} , // 4 pipes 16 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   29,  146,    0} , // 8 pipes 1 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   30,  147,    0} , // 8 pipes 2 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   31,  148,    0} , // 8 pipes 4 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   32,  149,    0} , // 8 pipes 8 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   33,  150,    0} , // 8 pipes 16 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   29,  151,    0} , // 16 pipes 1 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   30,  152,    0} , // 16 pipes 2 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   31,  153,    0} , // 16 pipes 4 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   32,  154,    0} , // 16 pipes 8 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   33,  155,    0} , // 16 pipes 16 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   29,  151,    0} , // 32 pipes 1 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   30,  152,    0} , // 32 pipes 2 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   31,  153,    0} , // 32 pipes 4 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   32,  154,    0} , // 32 pipes 8 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   33,  155,    0} , // 32 pipes 16 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   29,  151,    0} , // 64 pipes 1 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   30,  152,    0} , // 64 pipes 2 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   31,  153,    0} , // 64 pipes 4 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   32,  154,    0} , // 64 pipes 8 bpe @ SW_4K_S3_X @ Navi1x
    {   3,   33,  155,    0} , // 64 pipes 16 bpe @ SW_4K_S3_X @ Navi1x
};

const ADDR_SW_PATINFO SW_64K_S_PATINFO[] =
{
    {   1,    0,    1,    1} , // 1 pipes 1 bpe @ SW_64K_S @ Navi1x
    {   1,    1,    2,    2} , // 1 pipes 2 bpe @ SW_64K_S @ Navi1x
    {   1,    2,    3,    3} , // 1 pipes 4 bpe @ SW_64K_S @ Navi1x
    {   1,    3,    4,    4} , // 1 pipes 8 bpe @ SW_64K_S @ Navi1x
    {   1,    4,    5,    5} , // 1 pipes 16 bpe @ SW_64K_S @ Navi1x
    {   1,    0,    1,    1} , // 2 pipes 1 bpe @ SW_64K_S @ Navi1x
    {   1,    1,    2,    2} , // 2 pipes 2 bpe @ SW_64K_S @ Navi1x
    {   1,    2,    3,    3} , // 2 pipes 4 bpe @ SW_64K_S @ Navi1x
    {   1,    3,    4,    4} , // 2 pipes 8 bpe @ SW_64K_S @ Navi1x
    {   1,    4,    5,    5} , // 2 pipes 16 bpe @ SW_64K_S @ Navi1x
    {   1,    0,    1,    1} , // 4 pipes 1 bpe @ SW_64K_S @ Navi1x
    {   1,    1,    2,    2} , // 4 pipes 2 bpe @ SW_64K_S @ Navi1x
    {   1,    2,    3,    3} , // 4 pipes 4 bpe @ SW_64K_S @ Navi1x
    {   1,    3,    4,    4} , // 4 pipes 8 bpe @ SW_64K_S @ Navi1x
    {   1,    4,    5,    5} , // 4 pipes 16 bpe @ SW_64K_S @ Navi1x
    {   1,    0,    1,    1} , // 8 pipes 1 bpe @ SW_64K_S @ Navi1x
    {   1,    1,    2,    2} , // 8 pipes 2 bpe @ SW_64K_S @ Navi1x
    {   1,    2,    3,    3} , // 8 pipes 4 bpe @ SW_64K_S @ Navi1x
    {   1,    3,    4,    4} , // 8 pipes 8 bpe @ SW_64K_S @ Navi1x
    {   1,    4,    5,    5} , // 8 pipes 16 bpe @ SW_64K_S @ Navi1x
    {   1,    0,    1,    1} , // 16 pipes 1 bpe @ SW_64K_S @ Navi1x
    {   1,    1,    2,    2} , // 16 pipes 2 bpe @ SW_64K_S @ Navi1x
    {   1,    2,    3,    3} , // 16 pipes 4 bpe @ SW_64K_S @ Navi1x
    {   1,    3,    4,    4} , // 16 pipes 8 bpe @ SW_64K_S @ Navi1x
    {   1,    4,    5,    5} , // 16 pipes 16 bpe @ SW_64K_S @ Navi1x
    {   1,    0,    1,    1} , // 32 pipes 1 bpe @ SW_64K_S @ Navi1x
    {   1,    1,    2,    2} , // 32 pipes 2 bpe @ SW_64K_S @ Navi1x
    {   1,    2,    3,    3} , // 32 pipes 4 bpe @ SW_64K_S @ Navi1x
    {   1,    3,    4,    4} , // 32 pipes 8 bpe @ SW_64K_S @ Navi1x
    {   1,    4,    5,    5} , // 32 pipes 16 bpe @ SW_64K_S @ Navi1x
    {   1,    0,    1,    1} , // 64 pipes 1 bpe @ SW_64K_S @ Navi1x
    {   1,    1,    2,    2} , // 64 pipes 2 bpe @ SW_64K_S @ Navi1x
    {   1,    2,    3,    3} , // 64 pipes 4 bpe @ SW_64K_S @ Navi1x
    {   1,    3,    4,    4} , // 64 pipes 8 bpe @ SW_64K_S @ Navi1x
    {   1,    4,    5,    5} , // 64 pipes 16 bpe @ SW_64K_S @ Navi1x
};

const ADDR_SW_PATINFO SW_64K_D_PATINFO[] =
{
    {   1,    5,    1,    1} , // 1 pipes 1 bpe @ SW_64K_D @ Navi1x
    {   1,    1,    2,    2} , // 1 pipes 2 bpe @ SW_64K_D @ Navi1x
    {   1,    2,    3,    3} , // 1 pipes 4 bpe @ SW_64K_D @ Navi1x
    {   1,    6,    4,    4} , // 1 pipes 8 bpe @ SW_64K_D @ Navi1x
    {   1,    7,    5,    5} , // 1 pipes 16 bpe @ SW_64K_D @ Navi1x
    {   1,    5,    1,    1} , // 2 pipes 1 bpe @ SW_64K_D @ Navi1x
    {   1,    1,    2,    2} , // 2 pipes 2 bpe @ SW_64K_D @ Navi1x
    {   1,    2,    3,    3} , // 2 pipes 4 bpe @ SW_64K_D @ Navi1x
    {   1,    6,    4,    4} , // 2 pipes 8 bpe @ SW_64K_D @ Navi1x
    {   1,    7,    5,    5} , // 2 pipes 16 bpe @ SW_64K_D @ Navi1x
    {   1,    5,    1,    1} , // 4 pipes 1 bpe @ SW_64K_D @ Navi1x
    {   1,    1,    2,    2} , // 4 pipes 2 bpe @ SW_64K_D @ Navi1x
    {   1,    2,    3,    3} , // 4 pipes 4 bpe @ SW_64K_D @ Navi1x
    {   1,    6,    4,    4} , // 4 pipes 8 bpe @ SW_64K_D @ Navi1x
    {   1,    7,    5,    5} , // 4 pipes 16 bpe @ SW_64K_D @ Navi1x
    {   1,    5,    1,    1} , // 8 pipes 1 bpe @ SW_64K_D @ Navi1x
    {   1,    1,    2,    2} , // 8 pipes 2 bpe @ SW_64K_D @ Navi1x
    {   1,    2,    3,    3} , // 8 pipes 4 bpe @ SW_64K_D @ Navi1x
    {   1,    6,    4,    4} , // 8 pipes 8 bpe @ SW_64K_D @ Navi1x
    {   1,    7,    5,    5} , // 8 pipes 16 bpe @ SW_64K_D @ Navi1x
    {   1,    5,    1,    1} , // 16 pipes 1 bpe @ SW_64K_D @ Navi1x
    {   1,    1,    2,    2} , // 16 pipes 2 bpe @ SW_64K_D @ Navi1x
    {   1,    2,    3,    3} , // 16 pipes 4 bpe @ SW_64K_D @ Navi1x
    {   1,    6,    4,    4} , // 16 pipes 8 bpe @ SW_64K_D @ Navi1x
    {   1,    7,    5,    5} , // 16 pipes 16 bpe @ SW_64K_D @ Navi1x
    {   1,    5,    1,    1} , // 32 pipes 1 bpe @ SW_64K_D @ Navi1x
    {   1,    1,    2,    2} , // 32 pipes 2 bpe @ SW_64K_D @ Navi1x
    {   1,    2,    3,    3} , // 32 pipes 4 bpe @ SW_64K_D @ Navi1x
    {   1,    6,    4,    4} , // 32 pipes 8 bpe @ SW_64K_D @ Navi1x
    {   1,    7,    5,    5} , // 32 pipes 16 bpe @ SW_64K_D @ Navi1x
    {   1,    5,    1,    1} , // 64 pipes 1 bpe @ SW_64K_D @ Navi1x
    {   1,    1,    2,    2} , // 64 pipes 2 bpe @ SW_64K_D @ Navi1x
    {   1,    2,    3,    3} , // 64 pipes 4 bpe @ SW_64K_D @ Navi1x
    {   1,    6,    4,    4} , // 64 pipes 8 bpe @ SW_64K_D @ Navi1x
    {   1,    7,    5,    5} , // 64 pipes 16 bpe @ SW_64K_D @ Navi1x
};

const ADDR_SW_PATINFO SW_64K_S_T_PATINFO[] =
{
    {   1,    0,    1,    1} , // 1 pipes 1 bpe @ SW_64K_S_T @ Navi1x
    {   1,    1,    2,    2} , // 1 pipes 2 bpe @ SW_64K_S_T @ Navi1x
    {   1,    2,    3,    3} , // 1 pipes 4 bpe @ SW_64K_S_T @ Navi1x
    {   1,    3,    4,    4} , // 1 pipes 8 bpe @ SW_64K_S_T @ Navi1x
    {   1,    4,    5,    5} , // 1 pipes 16 bpe @ SW_64K_S_T @ Navi1x
    {   2,    0,   36,    1} , // 2 pipes 1 bpe @ SW_64K_S_T @ Navi1x
    {   2,    1,   37,    2} , // 2 pipes 2 bpe @ SW_64K_S_T @ Navi1x
    {   2,    2,   38,    3} , // 2 pipes 4 bpe @ SW_64K_S_T @ Navi1x
    {   2,    3,   39,    4} , // 2 pipes 8 bpe @ SW_64K_S_T @ Navi1x
    {   2,    4,   40,    5} , // 2 pipes 16 bpe @ SW_64K_S_T @ Navi1x
    {   2,    0,   41,    1} , // 4 pipes 1 bpe @ SW_64K_S_T @ Navi1x
    {   2,    1,   42,    2} , // 4 pipes 2 bpe @ SW_64K_S_T @ Navi1x
    {   2,    2,   43,    3} , // 4 pipes 4 bpe @ SW_64K_S_T @ Navi1x
    {   2,    3,   44,    4} , // 4 pipes 8 bpe @ SW_64K_S_T @ Navi1x
    {   2,    4,   45,    5} , // 4 pipes 16 bpe @ SW_64K_S_T @ Navi1x
    {   2,    0,   46,    1} , // 8 pipes 1 bpe @ SW_64K_S_T @ Navi1x
    {   2,    1,   47,    2} , // 8 pipes 2 bpe @ SW_64K_S_T @ Navi1x
    {   2,    2,   48,    3} , // 8 pipes 4 bpe @ SW_64K_S_T @ Navi1x
    {   2,    3,   49,    4} , // 8 pipes 8 bpe @ SW_64K_S_T @ Navi1x
    {   2,    4,   50,    5} , // 8 pipes 16 bpe @ SW_64K_S_T @ Navi1x
    {   2,    0,   51,    1} , // 16 pipes 1 bpe @ SW_64K_S_T @ Navi1x
    {   2,    1,   52,    2} , // 16 pipes 2 bpe @ SW_64K_S_T @ Navi1x
    {   2,    2,   53,    3} , // 16 pipes 4 bpe @ SW_64K_S_T @ Navi1x
    {   2,    3,   54,    4} , // 16 pipes 8 bpe @ SW_64K_S_T @ Navi1x
    {   2,    4,   55,    5} , // 16 pipes 16 bpe @ SW_64K_S_T @ Navi1x
    {   2,    0,   56,   16} , // 32 pipes 1 bpe @ SW_64K_S_T @ Navi1x
    {   2,    1,   57,   17} , // 32 pipes 2 bpe @ SW_64K_S_T @ Navi1x
    {   2,    2,   58,   18} , // 32 pipes 4 bpe @ SW_64K_S_T @ Navi1x
    {   2,    3,   59,   19} , // 32 pipes 8 bpe @ SW_64K_S_T @ Navi1x
    {   2,    4,   60,   20} , // 32 pipes 16 bpe @ SW_64K_S_T @ Navi1x
    {   2,    0,    1,   21} , // 64 pipes 1 bpe @ SW_64K_S_T @ Navi1x
    {   2,    1,    2,   22} , // 64 pipes 2 bpe @ SW_64K_S_T @ Navi1x
    {   2,    2,    3,   23} , // 64 pipes 4 bpe @ SW_64K_S_T @ Navi1x
    {   2,    3,    4,   24} , // 64 pipes 8 bpe @ SW_64K_S_T @ Navi1x
    {   2,    4,    5,   25} , // 64 pipes 16 bpe @ SW_64K_S_T @ Navi1x
};

const ADDR_SW_PATINFO SW_64K_D_T_PATINFO[] =
{
    {   1,    5,    1,    1} , // 1 pipes 1 bpe @ SW_64K_D_T @ Navi1x
    {   1,    1,    2,    2} , // 1 pipes 2 bpe @ SW_64K_D_T @ Navi1x
    {   1,    2,    3,    3} , // 1 pipes 4 bpe @ SW_64K_D_T @ Navi1x
    {   1,    6,    4,    4} , // 1 pipes 8 bpe @ SW_64K_D_T @ Navi1x
    {   1,    7,    5,    5} , // 1 pipes 16 bpe @ SW_64K_D_T @ Navi1x
    {   2,    5,   36,    1} , // 2 pipes 1 bpe @ SW_64K_D_T @ Navi1x
    {   2,    1,   37,    2} , // 2 pipes 2 bpe @ SW_64K_D_T @ Navi1x
    {   2,    2,   38,    3} , // 2 pipes 4 bpe @ SW_64K_D_T @ Navi1x
    {   2,    6,   39,    4} , // 2 pipes 8 bpe @ SW_64K_D_T @ Navi1x
    {   2,    7,   40,    5} , // 2 pipes 16 bpe @ SW_64K_D_T @ Navi1x
    {   2,    5,   41,    1} , // 4 pipes 1 bpe @ SW_64K_D_T @ Navi1x
    {   2,    1,   42,    2} , // 4 pipes 2 bpe @ SW_64K_D_T @ Navi1x
    {   2,    2,   43,    3} , // 4 pipes 4 bpe @ SW_64K_D_T @ Navi1x
    {   2,    6,   44,    4} , // 4 pipes 8 bpe @ SW_64K_D_T @ Navi1x
    {   2,    7,   45,    5} , // 4 pipes 16 bpe @ SW_64K_D_T @ Navi1x
    {   2,    5,   46,    1} , // 8 pipes 1 bpe @ SW_64K_D_T @ Navi1x
    {   2,    1,   47,    2} , // 8 pipes 2 bpe @ SW_64K_D_T @ Navi1x
    {   2,    2,   48,    3} , // 8 pipes 4 bpe @ SW_64K_D_T @ Navi1x
    {   2,    6,   49,    4} , // 8 pipes 8 bpe @ SW_64K_D_T @ Navi1x
    {   2,    7,   50,    5} , // 8 pipes 16 bpe @ SW_64K_D_T @ Navi1x
    {   2,    5,   51,    1} , // 16 pipes 1 bpe @ SW_64K_D_T @ Navi1x
    {   2,    1,   52,    2} , // 16 pipes 2 bpe @ SW_64K_D_T @ Navi1x
    {   2,    2,   53,    3} , // 16 pipes 4 bpe @ SW_64K_D_T @ Navi1x
    {   2,    6,   54,    4} , // 16 pipes 8 bpe @ SW_64K_D_T @ Navi1x
    {   2,    7,   55,    5} , // 16 pipes 16 bpe @ SW_64K_D_T @ Navi1x
    {   2,    5,   56,   16} , // 32 pipes 1 bpe @ SW_64K_D_T @ Navi1x
    {   2,    1,   57,   17} , // 32 pipes 2 bpe @ SW_64K_D_T @ Navi1x
    {   2,    2,   58,   18} , // 32 pipes 4 bpe @ SW_64K_D_T @ Navi1x
    {   2,    6,   59,   19} , // 32 pipes 8 bpe @ SW_64K_D_T @ Navi1x
    {   2,    7,   60,   20} , // 32 pipes 16 bpe @ SW_64K_D_T @ Navi1x
    {   2,    5,    1,   21} , // 64 pipes 1 bpe @ SW_64K_D_T @ Navi1x
    {   2,    1,    2,   22} , // 64 pipes 2 bpe @ SW_64K_D_T @ Navi1x
    {   2,    2,    3,   23} , // 64 pipes 4 bpe @ SW_64K_D_T @ Navi1x
    {   2,    6,    4,   24} , // 64 pipes 8 bpe @ SW_64K_D_T @ Navi1x
    {   2,    7,    5,   25} , // 64 pipes 16 bpe @ SW_64K_D_T @ Navi1x
};

const ADDR_SW_PATINFO SW_64K_S_X_PATINFO[] =
{
    {   1,    0,    1,    1} , // 1 pipes 1 bpe @ SW_64K_S_X @ Navi1x
    {   1,    1,    2,    2} , // 1 pipes 2 bpe @ SW_64K_S_X @ Navi1x
    {   1,    2,    3,    3} , // 1 pipes 4 bpe @ SW_64K_S_X @ Navi1x
    {   1,    3,    4,    4} , // 1 pipes 8 bpe @ SW_64K_S_X @ Navi1x
    {   1,    4,    5,    5} , // 1 pipes 16 bpe @ SW_64K_S_X @ Navi1x
    {   3,    0,    6,    1} , // 2 pipes 1 bpe @ SW_64K_S_X @ Navi1x
    {   3,    1,    7,    2} , // 2 pipes 2 bpe @ SW_64K_S_X @ Navi1x
    {   3,    2,    8,    3} , // 2 pipes 4 bpe @ SW_64K_S_X @ Navi1x
    {   3,    3,    9,    4} , // 2 pipes 8 bpe @ SW_64K_S_X @ Navi1x
    {   3,    4,   10,    5} , // 2 pipes 16 bpe @ SW_64K_S_X @ Navi1x
    {   3,    0,   11,    1} , // 4 pipes 1 bpe @ SW_64K_S_X @ Navi1x
    {   3,    1,   12,    2} , // 4 pipes 2 bpe @ SW_64K_S_X @ Navi1x
    {   3,    2,   13,    3} , // 4 pipes 4 bpe @ SW_64K_S_X @ Navi1x
    {   3,    3,   14,    4} , // 4 pipes 8 bpe @ SW_64K_S_X @ Navi1x
    {   3,    4,   15,    5} , // 4 pipes 16 bpe @ SW_64K_S_X @ Navi1x
    {   3,    0,   16,    1} , // 8 pipes 1 bpe @ SW_64K_S_X @ Navi1x
    {   3,    1,   17,    2} , // 8 pipes 2 bpe @ SW_64K_S_X @ Navi1x
    {   3,    2,   18,    3} , // 8 pipes 4 bpe @ SW_64K_S_X @ Navi1x
    {   3,    3,   19,    4} , // 8 pipes 8 bpe @ SW_64K_S_X @ Navi1x
    {   3,    4,   20,    5} , // 8 pipes 16 bpe @ SW_64K_S_X @ Navi1x
    {   3,    0,   21,    1} , // 16 pipes 1 bpe @ SW_64K_S_X @ Navi1x
    {   3,    1,   22,    2} , // 16 pipes 2 bpe @ SW_64K_S_X @ Navi1x
    {   3,    2,   23,    3} , // 16 pipes 4 bpe @ SW_64K_S_X @ Navi1x
    {   3,    3,   24,    4} , // 16 pipes 8 bpe @ SW_64K_S_X @ Navi1x
    {   3,    4,   25,    5} , // 16 pipes 16 bpe @ SW_64K_S_X @ Navi1x
    {   3,    0,   26,    6} , // 32 pipes 1 bpe @ SW_64K_S_X @ Navi1x
    {   3,    1,   27,    7} , // 32 pipes 2 bpe @ SW_64K_S_X @ Navi1x
    {   3,    2,   28,    8} , // 32 pipes 4 bpe @ SW_64K_S_X @ Navi1x
    {   3,    3,   29,    9} , // 32 pipes 8 bpe @ SW_64K_S_X @ Navi1x
    {   3,    4,   30,   10} , // 32 pipes 16 bpe @ SW_64K_S_X @ Navi1x
    {   3,    0,   31,   11} , // 64 pipes 1 bpe @ SW_64K_S_X @ Navi1x
    {   3,    1,   32,   12} , // 64 pipes 2 bpe @ SW_64K_S_X @ Navi1x
    {   3,    2,   33,   13} , // 64 pipes 4 bpe @ SW_64K_S_X @ Navi1x
    {   3,    3,   34,   14} , // 64 pipes 8 bpe @ SW_64K_S_X @ Navi1x
    {   3,    4,   35,   15} , // 64 pipes 16 bpe @ SW_64K_S_X @ Navi1x
};

const ADDR_SW_PATINFO SW_64K_D_X_PATINFO[] =
{
    {   1,    5,    1,    1} , // 1 pipes 1 bpe @ SW_64K_D_X @ Navi1x
    {   1,    1,    2,    2} , // 1 pipes 2 bpe @ SW_64K_D_X @ Navi1x
    {   1,    2,    3,    3} , // 1 pipes 4 bpe @ SW_64K_D_X @ Navi1x
    {   1,    6,    4,    4} , // 1 pipes 8 bpe @ SW_64K_D_X @ Navi1x
    {   1,    7,    5,    5} , // 1 pipes 16 bpe @ SW_64K_D_X @ Navi1x
    {   3,    5,    6,    1} , // 2 pipes 1 bpe @ SW_64K_D_X @ Navi1x
    {   3,    1,    7,    2} , // 2 pipes 2 bpe @ SW_64K_D_X @ Navi1x
    {   3,    2,    8,    3} , // 2 pipes 4 bpe @ SW_64K_D_X @ Navi1x
    {   3,    6,    9,    4} , // 2 pipes 8 bpe @ SW_64K_D_X @ Navi1x
    {   3,    7,   10,    5} , // 2 pipes 16 bpe @ SW_64K_D_X @ Navi1x
    {   3,    5,   11,    1} , // 4 pipes 1 bpe @ SW_64K_D_X @ Navi1x
    {   3,    1,   12,    2} , // 4 pipes 2 bpe @ SW_64K_D_X @ Navi1x
    {   3,    2,   13,    3} , // 4 pipes 4 bpe @ SW_64K_D_X @ Navi1x
    {   3,    6,   14,    4} , // 4 pipes 8 bpe @ SW_64K_D_X @ Navi1x
    {   3,    7,   15,    5} , // 4 pipes 16 bpe @ SW_64K_D_X @ Navi1x
    {   3,    5,   16,    1} , // 8 pipes 1 bpe @ SW_64K_D_X @ Navi1x
    {   3,    1,   17,    2} , // 8 pipes 2 bpe @ SW_64K_D_X @ Navi1x
    {   3,    2,   18,    3} , // 8 pipes 4 bpe @ SW_64K_D_X @ Navi1x
    {   3,    6,   19,    4} , // 8 pipes 8 bpe @ SW_64K_D_X @ Navi1x
    {   3,    7,   20,    5} , // 8 pipes 16 bpe @ SW_64K_D_X @ Navi1x
    {   3,    5,   21,    1} , // 16 pipes 1 bpe @ SW_64K_D_X @ Navi1x
    {   3,    1,   22,    2} , // 16 pipes 2 bpe @ SW_64K_D_X @ Navi1x
    {   3,    2,   23,    3} , // 16 pipes 4 bpe @ SW_64K_D_X @ Navi1x
    {   3,    6,   24,    4} , // 16 pipes 8 bpe @ SW_64K_D_X @ Navi1x
    {   3,    7,   25,    5} , // 16 pipes 16 bpe @ SW_64K_D_X @ Navi1x
    {   3,    5,   26,    6} , // 32 pipes 1 bpe @ SW_64K_D_X @ Navi1x
    {   3,    1,   27,    7} , // 32 pipes 2 bpe @ SW_64K_D_X @ Navi1x
    {   3,    2,   28,    8} , // 32 pipes 4 bpe @ SW_64K_D_X @ Navi1x
    {   3,    6,   29,    9} , // 32 pipes 8 bpe @ SW_64K_D_X @ Navi1x
    {   3,    7,   30,   10} , // 32 pipes 16 bpe @ SW_64K_D_X @ Navi1x
    {   3,    5,   31,   11} , // 64 pipes 1 bpe @ SW_64K_D_X @ Navi1x
    {   3,    1,   32,   12} , // 64 pipes 2 bpe @ SW_64K_D_X @ Navi1x
    {   3,    2,   33,   13} , // 64 pipes 4 bpe @ SW_64K_D_X @ Navi1x
    {   3,    6,   34,   14} , // 64 pipes 8 bpe @ SW_64K_D_X @ Navi1x
    {   3,    7,   35,   15} , // 64 pipes 16 bpe @ SW_64K_D_X @ Navi1x
};

const ADDR_SW_PATINFO SW_64K_R_X_1xaa_PATINFO[] =
{
    {   1,    5,    1,    1} , // 1 pipes 1 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   1,    1,    2,    2} , // 1 pipes 2 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   1,    2,    3,    3} , // 1 pipes 4 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   1,    6,    4,    4} , // 1 pipes 8 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   1,    7,    5,    5} , // 1 pipes 16 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,   28,   61,    1} , // 2 pipes 1 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,    1,   62,    2} , // 2 pipes 2 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,    2,    8,    3} , // 2 pipes 4 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,    6,   63,    4} , // 2 pipes 8 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,    7,   64,    5} , // 2 pipes 16 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,   28,   65,    1} , // 4 pipes 1 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,    1,   66,    2} , // 4 pipes 2 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,    2,   67,    3} , // 4 pipes 4 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,    6,   68,    4} , // 4 pipes 8 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,    7,   69,   26} , // 4 pipes 16 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,   28,   70,    1} , // 8 pipes 1 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,    1,   71,    2} , // 8 pipes 2 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,    2,   72,   27} , // 8 pipes 4 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,    6,   72,   28} , // 8 pipes 8 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,    7,   73,   29} , // 8 pipes 16 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,   28,   74,    1} , // 16 pipes 1 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,    1,   74,   30} , // 16 pipes 2 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,    2,   74,   31} , // 16 pipes 4 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,    6,   74,   32} , // 16 pipes 8 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,    7,   74,   33} , // 16 pipes 16 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,   28,   75,    6} , // 32 pipes 1 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,    1,   75,   34} , // 32 pipes 2 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,    2,   75,   35} , // 32 pipes 4 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,    6,   75,   36} , // 32 pipes 8 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,    7,   76,   37} , // 32 pipes 16 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,   28,   77,   11} , // 64 pipes 1 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,    1,   77,   38} , // 64 pipes 2 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,    2,   77,   39} , // 64 pipes 4 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,    6,   78,   40} , // 64 pipes 8 bpe @ SW_64K_R_X 1xaa @ Navi1x
    {   3,    7,   79,   41} , // 64 pipes 16 bpe @ SW_64K_R_X 1xaa @ Navi1x
};

const ADDR_SW_PATINFO SW_64K_R_X_2xaa_PATINFO[] =
{
    {   2,    5,    1,   99} , // 1 pipes 1 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   2,    1,    2,  100} , // 1 pipes 2 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   2,    2,    3,  101} , // 1 pipes 4 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   2,    6,    4,  102} , // 1 pipes 8 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   2,    7,    5,  103} , // 1 pipes 16 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,   28,   61,   99} , // 2 pipes 1 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,    1,   62,  100} , // 2 pipes 2 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,    2,    8,  101} , // 2 pipes 4 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,    6,   63,  102} , // 2 pipes 8 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,    7,   64,  103} , // 2 pipes 16 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,   28,   65,   99} , // 4 pipes 1 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,    1,   66,  100} , // 4 pipes 2 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,    2,   67,  101} , // 4 pipes 4 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,    6,   68,  102} , // 4 pipes 8 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,    7,   69,  104} , // 4 pipes 16 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,   28,   70,   99} , // 8 pipes 1 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,    1,   71,  100} , // 8 pipes 2 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,    2,   72,  105} , // 8 pipes 4 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,    6,   72,  106} , // 8 pipes 8 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,    7,   73,  107} , // 8 pipes 16 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,   28,   74,   99} , // 16 pipes 1 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,    1,   74,  108} , // 16 pipes 2 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,    2,   74,  109} , // 16 pipes 4 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,    6,   74,  107} , // 16 pipes 8 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,    7,  113,   33} , // 16 pipes 16 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,   28,   75,  110} , // 32 pipes 1 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,    1,   75,  111} , // 32 pipes 2 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,    2,   75,  112} , // 32 pipes 4 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,    6,   76,  113} , // 32 pipes 8 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,    7,  114,   37} , // 32 pipes 16 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,   28,   78,  114} , // 64 pipes 1 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,    1,   78,  115} , // 64 pipes 2 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,    2,   78,  116} , // 64 pipes 4 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,    6,   79,  117} , // 64 pipes 8 bpe @ SW_64K_R_X 2xaa @ Navi1x
    {   3,    7,  115,   41} , // 64 pipes 16 bpe @ SW_64K_R_X 2xaa @ Navi1x
};

const ADDR_SW_PATINFO SW_64K_R_X_4xaa_PATINFO[] =
{
    {   2,    5,    1,  118} , // 1 pipes 1 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   2,    1,    2,  119} , // 1 pipes 2 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   2,    2,    3,  120} , // 1 pipes 4 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   2,    6,    4,  121} , // 1 pipes 8 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   2,    7,    5,  122} , // 1 pipes 16 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,   28,   61,  118} , // 2 pipes 1 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,    1,   62,  119} , // 2 pipes 2 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,    2,    8,  120} , // 2 pipes 4 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,    6,   63,  121} , // 2 pipes 8 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,    7,   64,  122} , // 2 pipes 16 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,   28,   65,  118} , // 4 pipes 1 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,    1,   66,  119} , // 4 pipes 2 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,    2,   67,  120} , // 4 pipes 4 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,    6,   68,  121} , // 4 pipes 8 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,    7,   69,  123} , // 4 pipes 16 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,   28,   70,  118} , // 8 pipes 1 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,    1,   71,  119} , // 8 pipes 2 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,    2,   72,  124} , // 8 pipes 4 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,    6,   93,  125} , // 8 pipes 8 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,    7,  116,  107} , // 8 pipes 16 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,   28,   74,  118} , // 16 pipes 1 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,    1,   74,  126} , // 16 pipes 2 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,    2,   74,  127} , // 16 pipes 4 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,    6,  117,  107} , // 16 pipes 8 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,    7,  118,   33} , // 16 pipes 16 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,   28,   76,  128} , // 32 pipes 1 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,    1,   76,  129} , // 32 pipes 2 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,    2,   76,  130} , // 32 pipes 4 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,    6,  119,  113} , // 32 pipes 8 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,    7,  120,   37} , // 32 pipes 16 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,   28,   79,  131} , // 64 pipes 1 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,    1,   79,  132} , // 64 pipes 2 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,    2,   79,  133} , // 64 pipes 4 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,    6,  121,  117} , // 64 pipes 8 bpe @ SW_64K_R_X 4xaa @ Navi1x
    {   3,    7,  122,   41} , // 64 pipes 16 bpe @ SW_64K_R_X 4xaa @ Navi1x
};

const ADDR_SW_PATINFO SW_64K_R_X_8xaa_PATINFO[] =
{
    {   2,    5,    1,  134} , // 1 pipes 1 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   2,    1,    2,  135} , // 1 pipes 2 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   2,    2,    3,  135} , // 1 pipes 4 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   2,    6,    4,  136} , // 1 pipes 8 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   2,    7,    5,  136} , // 1 pipes 16 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,   28,   61,  134} , // 2 pipes 1 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,    1,   62,  135} , // 2 pipes 2 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,    2,    8,  135} , // 2 pipes 4 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,    6,   63,  136} , // 2 pipes 8 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,    7,   64,  136} , // 2 pipes 16 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,   28,   65,  134} , // 4 pipes 1 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,    1,   66,  135} , // 4 pipes 2 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,    2,   67,  135} , // 4 pipes 4 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,    6,   68,  136} , // 4 pipes 8 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,    7,  102,  137} , // 4 pipes 16 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,   28,   70,  134} , // 8 pipes 1 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,    1,   71,  135} , // 8 pipes 2 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,    2,   72,  138} , // 8 pipes 4 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,    6,  123,  139} , // 8 pipes 8 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,    7,  124,  140} , // 8 pipes 16 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,   28,  105,  134} , // 16 pipes 1 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,    1,  105,  138} , // 16 pipes 2 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,    2,  125,  127} , // 16 pipes 4 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,    6,  126,  107} , // 16 pipes 8 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,    7,  126,  141} , // 16 pipes 16 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,   28,  107,  142} , // 32 pipes 1 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,    1,  108,  143} , // 32 pipes 2 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,    2,  127,  130} , // 32 pipes 4 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,    6,  128,  113} , // 32 pipes 8 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,    7,  128,  144} , // 32 pipes 16 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,   28,  110,  145} , // 64 pipes 1 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,    1,  111,  146} , // 64 pipes 2 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,    2,  129,  133} , // 64 pipes 4 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,    6,  130,  117} , // 64 pipes 8 bpe @ SW_64K_R_X 8xaa @ Navi1x
    {   3,    7,  130,  147} , // 64 pipes 16 bpe @ SW_64K_R_X 8xaa @ Navi1x
};

const ADDR_SW_PATINFO SW_64K_Z_X_1xaa_PATINFO[] =
{
    {   1,    8,    1,    1} , // 1 pipes 1 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   1,    9,    2,    2} , // 1 pipes 2 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   1,   10,    3,    3} , // 1 pipes 4 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   1,   11,    4,    4} , // 1 pipes 8 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   1,    7,    5,    5} , // 1 pipes 16 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,   12,   61,    1} , // 2 pipes 1 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,    9,   62,    2} , // 2 pipes 2 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,   10,    8,    3} , // 2 pipes 4 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,   11,   63,    4} , // 2 pipes 8 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,    7,   64,    5} , // 2 pipes 16 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,   12,   65,    1} , // 4 pipes 1 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,    9,   66,    2} , // 4 pipes 2 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,   10,   67,    3} , // 4 pipes 4 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,   11,   68,    4} , // 4 pipes 8 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,    7,   69,   26} , // 4 pipes 16 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,   12,   70,    1} , // 8 pipes 1 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,    9,   71,    2} , // 8 pipes 2 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,   10,   72,   27} , // 8 pipes 4 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,   11,   72,   28} , // 8 pipes 8 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,    7,   73,   29} , // 8 pipes 16 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,   12,   74,    1} , // 16 pipes 1 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,    9,   74,   30} , // 16 pipes 2 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,   10,   74,   31} , // 16 pipes 4 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,   11,   74,   32} , // 16 pipes 8 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,    7,   74,   33} , // 16 pipes 16 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,   12,   75,    6} , // 32 pipes 1 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,    9,   75,   34} , // 32 pipes 2 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,   10,   75,   35} , // 32 pipes 4 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,   11,   75,   36} , // 32 pipes 8 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,    7,   76,   37} , // 32 pipes 16 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,   12,   77,   11} , // 64 pipes 1 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,    9,   77,   38} , // 64 pipes 2 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,   10,   77,   39} , // 64 pipes 4 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,   11,   78,   40} , // 64 pipes 8 bpe @ SW_64K_Z_X 1xaa @ Navi1x
    {   3,    7,   79,   41} , // 64 pipes 16 bpe @ SW_64K_Z_X 1xaa @ Navi1x
};

const ADDR_SW_PATINFO SW_64K_Z_X_2xaa_PATINFO[] =
{
    {   1,   13,   80,   42} , // 1 pipes 1 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   1,   14,    3,    3} , // 1 pipes 2 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   2,   15,    3,   43} , // 1 pipes 4 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   2,   16,   81,   44} , // 1 pipes 8 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   2,   17,    5,   45} , // 1 pipes 16 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   13,   82,   42} , // 2 pipes 1 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   14,    8,    3} , // 2 pipes 2 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   15,    8,   43} , // 2 pipes 4 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   16,   83,   44} , // 2 pipes 8 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   17,   64,   45} , // 2 pipes 16 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   13,   84,   42} , // 4 pipes 1 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   14,   67,    3} , // 4 pipes 2 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   15,   67,   43} , // 4 pipes 4 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   16,   85,   44} , // 4 pipes 8 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   17,   69,   46} , // 4 pipes 16 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   13,   86,   42} , // 8 pipes 1 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   14,   72,   27} , // 8 pipes 2 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   15,   72,   47} , // 8 pipes 4 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   16,   73,   48} , // 8 pipes 8 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   17,   73,   49} , // 8 pipes 16 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   13,   74,   50} , // 16 pipes 1 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   14,   74,   31} , // 16 pipes 2 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   15,   74,   51} , // 16 pipes 4 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   16,   74,   52} , // 16 pipes 8 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   17,   87,   53} , // 16 pipes 16 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   13,   75,   54} , // 32 pipes 1 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   14,   75,   35} , // 32 pipes 2 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   15,   75,   55} , // 32 pipes 4 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   16,   76,   56} , // 32 pipes 8 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   17,   88,   57} , // 32 pipes 16 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   13,   78,   58} , // 64 pipes 1 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   14,   78,   59} , // 64 pipes 2 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   15,   78,   60} , // 64 pipes 4 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   16,   79,   41} , // 64 pipes 8 bpe @ SW_64K_Z_X 2xaa @ Navi1x
    {   3,   17,   89,   61} , // 64 pipes 16 bpe @ SW_64K_Z_X 2xaa @ Navi1x
};

const ADDR_SW_PATINFO SW_64K_Z_X_4xaa_PATINFO[] =
{
    {   1,   18,    3,    3} , // 1 pipes 1 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   2,   19,   90,   62} , // 1 pipes 2 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   2,   20,    3,   63} , // 1 pipes 4 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   2,   21,    4,   64} , // 1 pipes 8 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   2,   22,    5,   65} , // 1 pipes 16 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   18,    8,    3} , // 2 pipes 1 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   19,   91,   62} , // 2 pipes 2 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   20,    8,   66} , // 2 pipes 4 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   21,   63,   67} , // 2 pipes 8 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   22,   64,   68} , // 2 pipes 16 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   18,   67,    3} , // 4 pipes 1 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   19,   92,   62} , // 4 pipes 2 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   20,   67,   63} , // 4 pipes 4 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   21,   68,   64} , // 4 pipes 8 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   22,   69,   69} , // 4 pipes 16 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   18,   72,   27} , // 8 pipes 1 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   19,   72,   70} , // 8 pipes 2 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   20,   72,   71} , // 8 pipes 4 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   21,   93,   72} , // 8 pipes 8 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   22,   94,   73} , // 8 pipes 16 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   18,   74,   31} , // 16 pipes 1 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   19,   74,   74} , // 16 pipes 2 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   20,   74,   75} , // 16 pipes 4 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   21,   95,   76} , // 16 pipes 8 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   22,   96,   76} , // 16 pipes 16 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   18,   76,   77} , // 32 pipes 1 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   19,   76,   78} , // 32 pipes 2 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   20,   76,   56} , // 32 pipes 4 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   21,   97,   79} , // 32 pipes 8 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   22,   98,   79} , // 32 pipes 16 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   18,   79,   80} , // 64 pipes 1 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   19,   79,   81} , // 64 pipes 2 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   20,   79,   41} , // 64 pipes 4 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   21,   99,   82} , // 64 pipes 8 bpe @ SW_64K_Z_X 4xaa @ Navi1x
    {   3,   22,  100,   82} , // 64 pipes 16 bpe @ SW_64K_Z_X 4xaa @ Navi1x
};

const ADDR_SW_PATINFO SW_64K_Z_X_8xaa_PATINFO[] =
{
    {   2,   23,    3,   43} , // 1 pipes 1 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   2,   24,    3,   63} , // 1 pipes 2 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   2,   25,    3,   83} , // 1 pipes 4 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   2,   26,   81,   84} , // 1 pipes 8 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   2,   27,    5,   85} , // 1 pipes 16 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   23,    8,   43} , // 2 pipes 1 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   24,    8,   66} , // 2 pipes 2 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   25,    8,   86} , // 2 pipes 4 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   26,  101,   87} , // 2 pipes 8 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   27,   64,   88} , // 2 pipes 16 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   23,   67,   43} , // 4 pipes 1 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   24,   67,   63} , // 4 pipes 2 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   25,   67,   83} , // 4 pipes 4 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   26,   85,   84} , // 4 pipes 8 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   27,  102,   89} , // 4 pipes 16 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   23,   72,   47} , // 8 pipes 1 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   24,   72,   71} , // 8 pipes 2 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   25,   72,   90} , // 8 pipes 4 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   26,  103,   91} , // 8 pipes 8 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   27,  104,   92} , // 8 pipes 16 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   23,  105,   51} , // 16 pipes 1 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   24,  105,   75} , // 16 pipes 2 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   25,   87,   93} , // 16 pipes 4 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   26,   96,   76} , // 16 pipes 8 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   27,  106,   94} , // 16 pipes 16 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   23,  107,   95} , // 32 pipes 1 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   24,  108,   56} , // 32 pipes 2 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   25,   88,   57} , // 32 pipes 4 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   26,   98,   79} , // 32 pipes 8 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   27,  109,   96} , // 32 pipes 16 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   23,  110,   97} , // 64 pipes 1 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   24,  111,   41} , // 64 pipes 2 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   25,   89,   61} , // 64 pipes 4 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   26,  100,   82} , // 64 pipes 8 bpe @ SW_64K_Z_X 8xaa @ Navi1x
    {   3,   27,  112,   98} , // 64 pipes 16 bpe @ SW_64K_Z_X 8xaa @ Navi1x
};

const ADDR_SW_PATINFO SW_64K_S3_PATINFO[] =
{
    {   1,   29,  131,  148} , // 1 pipes 1 bpe @ SW_64K_S3 @ Navi1x
    {   1,   30,  132,  149} , // 1 pipes 2 bpe @ SW_64K_S3 @ Navi1x
    {   1,   31,  133,  150} , // 1 pipes 4 bpe @ SW_64K_S3 @ Navi1x
    {   1,   32,  134,  151} , // 1 pipes 8 bpe @ SW_64K_S3 @ Navi1x
    {   1,   33,  135,  152} , // 1 pipes 16 bpe @ SW_64K_S3 @ Navi1x
    {   1,   29,  131,  148} , // 2 pipes 1 bpe @ SW_64K_S3 @ Navi1x
    {   1,   30,  132,  149} , // 2 pipes 2 bpe @ SW_64K_S3 @ Navi1x
    {   1,   31,  133,  150} , // 2 pipes 4 bpe @ SW_64K_S3 @ Navi1x
    {   1,   32,  134,  151} , // 2 pipes 8 bpe @ SW_64K_S3 @ Navi1x
    {   1,   33,  135,  152} , // 2 pipes 16 bpe @ SW_64K_S3 @ Navi1x
    {   1,   29,  131,  148} , // 4 pipes 1 bpe @ SW_64K_S3 @ Navi1x
    {   1,   30,  132,  149} , // 4 pipes 2 bpe @ SW_64K_S3 @ Navi1x
    {   1,   31,  133,  150} , // 4 pipes 4 bpe @ SW_64K_S3 @ Navi1x
    {   1,   32,  134,  151} , // 4 pipes 8 bpe @ SW_64K_S3 @ Navi1x
    {   1,   33,  135,  152} , // 4 pipes 16 bpe @ SW_64K_S3 @ Navi1x
    {   1,   29,  131,  148} , // 8 pipes 1 bpe @ SW_64K_S3 @ Navi1x
    {   1,   30,  132,  149} , // 8 pipes 2 bpe @ SW_64K_S3 @ Navi1x
    {   1,   31,  133,  150} , // 8 pipes 4 bpe @ SW_64K_S3 @ Navi1x
    {   1,   32,  134,  151} , // 8 pipes 8 bpe @ SW_64K_S3 @ Navi1x
    {   1,   33,  135,  152} , // 8 pipes 16 bpe @ SW_64K_S3 @ Navi1x
    {   1,   29,  131,  148} , // 16 pipes 1 bpe @ SW_64K_S3 @ Navi1x
    {   1,   30,  132,  149} , // 16 pipes 2 bpe @ SW_64K_S3 @ Navi1x
    {   1,   31,  133,  150} , // 16 pipes 4 bpe @ SW_64K_S3 @ Navi1x
    {   1,   32,  134,  151} , // 16 pipes 8 bpe @ SW_64K_S3 @ Navi1x
    {   1,   33,  135,  152} , // 16 pipes 16 bpe @ SW_64K_S3 @ Navi1x
    {   1,   29,  131,  148} , // 32 pipes 1 bpe @ SW_64K_S3 @ Navi1x
    {   1,   30,  132,  149} , // 32 pipes 2 bpe @ SW_64K_S3 @ Navi1x
    {   1,   31,  133,  150} , // 32 pipes 4 bpe @ SW_64K_S3 @ Navi1x
    {   1,   32,  134,  151} , // 32 pipes 8 bpe @ SW_64K_S3 @ Navi1x
    {   1,   33,  135,  152} , // 32 pipes 16 bpe @ SW_64K_S3 @ Navi1x
    {   1,   29,  131,  148} , // 64 pipes 1 bpe @ SW_64K_S3 @ Navi1x
    {   1,   30,  132,  149} , // 64 pipes 2 bpe @ SW_64K_S3 @ Navi1x
    {   1,   31,  133,  150} , // 64 pipes 4 bpe @ SW_64K_S3 @ Navi1x
    {   1,   32,  134,  151} , // 64 pipes 8 bpe @ SW_64K_S3 @ Navi1x
    {   1,   33,  135,  152} , // 64 pipes 16 bpe @ SW_64K_S3 @ Navi1x
};

const ADDR_SW_PATINFO SW_64K_S3_X_PATINFO[] =
{
    {   1,   29,  131,  148} , // 1 pipes 1 bpe @ SW_64K_S3_X @ Navi1x
    {   1,   30,  132,  149} , // 1 pipes 2 bpe @ SW_64K_S3_X @ Navi1x
    {   1,   31,  133,  150} , // 1 pipes 4 bpe @ SW_64K_S3_X @ Navi1x
    {   1,   32,  134,  151} , // 1 pipes 8 bpe @ SW_64K_S3_X @ Navi1x
    {   1,   33,  135,  152} , // 1 pipes 16 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   29,  136,  148} , // 2 pipes 1 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   30,  137,  149} , // 2 pipes 2 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   31,  138,  150} , // 2 pipes 4 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   32,  139,  151} , // 2 pipes 8 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   33,  140,  152} , // 2 pipes 16 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   29,  141,  148} , // 4 pipes 1 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   30,  142,  149} , // 4 pipes 2 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   31,  143,  150} , // 4 pipes 4 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   32,  144,  151} , // 4 pipes 8 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   33,  145,  152} , // 4 pipes 16 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   29,  146,  148} , // 8 pipes 1 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   30,  147,  149} , // 8 pipes 2 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   31,  148,  150} , // 8 pipes 4 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   32,  149,  151} , // 8 pipes 8 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   33,  150,  152} , // 8 pipes 16 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   29,  151,  148} , // 16 pipes 1 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   30,  152,  149} , // 16 pipes 2 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   31,  153,  150} , // 16 pipes 4 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   32,  154,  151} , // 16 pipes 8 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   33,  155,  152} , // 16 pipes 16 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   29,  156,  153} , // 32 pipes 1 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   30,  157,  154} , // 32 pipes 2 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   31,  158,  155} , // 32 pipes 4 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   32,  159,  156} , // 32 pipes 8 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   33,  160,  157} , // 32 pipes 16 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   29,  161,  158} , // 64 pipes 1 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   30,  162,  159} , // 64 pipes 2 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   31,  163,  160} , // 64 pipes 4 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   32,  164,  161} , // 64 pipes 8 bpe @ SW_64K_S3_X @ Navi1x
    {   3,   33,  165,  162} , // 64 pipes 16 bpe @ SW_64K_S3_X @ Navi1x
};

const ADDR_SW_PATINFO SW_64K_S3_T_PATINFO[] =
{
    {   1,   29,  131,  148} , // 1 pipes 1 bpe @ SW_64K_S3_T @ Navi1x
    {   1,   30,  132,  149} , // 1 pipes 2 bpe @ SW_64K_S3_T @ Navi1x
    {   1,   31,  133,  150} , // 1 pipes 4 bpe @ SW_64K_S3_T @ Navi1x
    {   1,   32,  134,  151} , // 1 pipes 8 bpe @ SW_64K_S3_T @ Navi1x
    {   1,   33,  135,  152} , // 1 pipes 16 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   29,  136,  148} , // 2 pipes 1 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   30,  137,  149} , // 2 pipes 2 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   31,  138,  150} , // 2 pipes 4 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   32,  139,  151} , // 2 pipes 8 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   33,  140,  152} , // 2 pipes 16 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   29,  141,  148} , // 4 pipes 1 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   30,  142,  149} , // 4 pipes 2 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   31,  143,  150} , // 4 pipes 4 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   32,  144,  151} , // 4 pipes 8 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   33,  145,  152} , // 4 pipes 16 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   29,  166,  148} , // 8 pipes 1 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   30,  167,  149} , // 8 pipes 2 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   31,  168,  150} , // 8 pipes 4 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   32,  169,  151} , // 8 pipes 8 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   33,  170,  152} , // 8 pipes 16 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   29,  171,  148} , // 16 pipes 1 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   30,  172,  149} , // 16 pipes 2 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   31,  173,  150} , // 16 pipes 4 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   32,  174,  151} , // 16 pipes 8 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   33,  175,  152} , // 16 pipes 16 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   29,  176,  153} , // 32 pipes 1 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   30,  177,  154} , // 32 pipes 2 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   31,  178,  155} , // 32 pipes 4 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   32,  179,  156} , // 32 pipes 8 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   33,  180,  157} , // 32 pipes 16 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   29,  131,  163} , // 64 pipes 1 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   30,  132,  164} , // 64 pipes 2 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   31,  133,  165} , // 64 pipes 4 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   32,  134,  166} , // 64 pipes 8 bpe @ SW_64K_S3_T @ Navi1x
    {   3,   33,  135,  167} , // 64 pipes 16 bpe @ SW_64K_S3_T @ Navi1x
};

const ADDR_SW_PATINFO SW_64K_D3_X_PATINFO[] =
{
    {   1,   34,  131,  148} , // 1 pipes 1 bpe @ SW_64K_D3_X @ Navi1x
    {   1,   35,  132,  149} , // 1 pipes 2 bpe @ SW_64K_D3_X @ Navi1x
    {   1,   36,  133,  150} , // 1 pipes 4 bpe @ SW_64K_D3_X @ Navi1x
    {   1,   37,  134,  151} , // 1 pipes 8 bpe @ SW_64K_D3_X @ Navi1x
    {   1,   38,  135,  152} , // 1 pipes 16 bpe @ SW_64K_D3_X @ Navi1x
    {   2,   34,  181,  148} , // 2 pipes 1 bpe @ SW_64K_D3_X @ Navi1x
    {   2,   35,  182,  149} , // 2 pipes 2 bpe @ SW_64K_D3_X @ Navi1x
    {   2,   36,  183,  150} , // 2 pipes 4 bpe @ SW_64K_D3_X @ Navi1x
    {   2,   37,  184,  168} , // 2 pipes 8 bpe @ SW_64K_D3_X @ Navi1x
    {   2,   38,  185,  169} , // 2 pipes 16 bpe @ SW_64K_D3_X @ Navi1x
    {   2,   34,  186,  170} , // 4 pipes 1 bpe @ SW_64K_D3_X @ Navi1x
    {   2,   35,  186,  171} , // 4 pipes 2 bpe @ SW_64K_D3_X @ Navi1x
    {   2,   36,  187,  172} , // 4 pipes 4 bpe @ SW_64K_D3_X @ Navi1x
    {   2,   37,  188,  169} , // 4 pipes 8 bpe @ SW_64K_D3_X @ Navi1x
    {   3,   38,  189,  169} , // 4 pipes 16 bpe @ SW_64K_D3_X @ Navi1x
    {   2,   34,  190,  173} , // 8 pipes 1 bpe @ SW_64K_D3_X @ Navi1x
    {   3,   35,  191,  171} , // 8 pipes 2 bpe @ SW_64K_D3_X @ Navi1x
    {   3,   36,  192,  172} , // 8 pipes 4 bpe @ SW_64K_D3_X @ Navi1x
    {   3,   37,  193,  169} , // 8 pipes 8 bpe @ SW_64K_D3_X @ Navi1x
    {   3,   38,  194,  169} , // 8 pipes 16 bpe @ SW_64K_D3_X @ Navi1x
    {   3,   34,  195,  174} , // 16 pipes 1 bpe @ SW_64K_D3_X @ Navi1x
    {   3,   35,  196,  171} , // 16 pipes 2 bpe @ SW_64K_D3_X @ Navi1x
    {   3,   36,  197,  172} , // 16 pipes 4 bpe @ SW_64K_D3_X @ Navi1x
    {   3,   37,  198,  169} , // 16 pipes 8 bpe @ SW_64K_D3_X @ Navi1x
    {   3,   38,  199,  169} , // 16 pipes 16 bpe @ SW_64K_D3_X @ Navi1x
    {   3,   34,  200,  175} , // 32 pipes 1 bpe @ SW_64K_D3_X @ Navi1x
    {   3,   35,  201,  176} , // 32 pipes 2 bpe @ SW_64K_D3_X @ Navi1x
    {   3,   36,  202,  177} , // 32 pipes 4 bpe @ SW_64K_D3_X @ Navi1x
    {   3,   37,  203,  178} , // 32 pipes 8 bpe @ SW_64K_D3_X @ Navi1x
    {   3,   38,  204,  178} , // 32 pipes 16 bpe @ SW_64K_D3_X @ Navi1x
    {   3,   34,  205,  179} , // 64 pipes 1 bpe @ SW_64K_D3_X @ Navi1x
    {   3,   35,  206,  180} , // 64 pipes 2 bpe @ SW_64K_D3_X @ Navi1x
    {   3,   36,  207,  181} , // 64 pipes 4 bpe @ SW_64K_D3_X @ Navi1x
    {   3,   37,  208,  182} , // 64 pipes 8 bpe @ SW_64K_D3_X @ Navi1x
    {   3,   38,  209,  182} , // 64 pipes 16 bpe @ SW_64K_D3_X @ Navi1x
};

const ADDR_SW_PATINFO SW_256_S_RBPLUS_PATINFO[] =
{
    {   1,    0,    0,    0} , // 1 pipes (1 PKRs) 1 bpe @ SW_256_S @ RbPlus
    {   1,    1,    0,    0} , // 1 pipes (1 PKRs) 2 bpe @ SW_256_S @ RbPlus
    {   1,    2,    0,    0} , // 1 pipes (1 PKRs) 4 bpe @ SW_256_S @ RbPlus
    {   1,    3,    0,    0} , // 1 pipes (1 PKRs) 8 bpe @ SW_256_S @ RbPlus
    {   1,    4,    0,    0} , // 1 pipes (1 PKRs) 16 bpe @ SW_256_S @ RbPlus
    {   1,    0,    0,    0} , // 2 pipes (1-2 PKRs) 1 bpe @ SW_256_S @ RbPlus
    {   1,    1,    0,    0} , // 2 pipes (1-2 PKRs) 2 bpe @ SW_256_S @ RbPlus
    {   1,    2,    0,    0} , // 2 pipes (1-2 PKRs) 4 bpe @ SW_256_S @ RbPlus
    {   1,    3,    0,    0} , // 2 pipes (1-2 PKRs) 8 bpe @ SW_256_S @ RbPlus
    {   1,    4,    0,    0} , // 2 pipes (1-2 PKRs) 16 bpe @ SW_256_S @ RbPlus
    {   1,    0,    0,    0} , // 4 pipes (1-2 PKRs) 1 bpe @ SW_256_S @ RbPlus
    {   1,    1,    0,    0} , // 4 pipes (1-2 PKRs) 2 bpe @ SW_256_S @ RbPlus
    {   1,    2,    0,    0} , // 4 pipes (1-2 PKRs) 4 bpe @ SW_256_S @ RbPlus
    {   1,    3,    0,    0} , // 4 pipes (1-2 PKRs) 8 bpe @ SW_256_S @ RbPlus
    {   1,    4,    0,    0} , // 4 pipes (1-2 PKRs) 16 bpe @ SW_256_S @ RbPlus
    {   1,    0,    0,    0} , // 8 pipes (2 PKRs) 1 bpe @ SW_256_S @ RbPlus
    {   1,    1,    0,    0} , // 8 pipes (2 PKRs) 2 bpe @ SW_256_S @ RbPlus
    {   1,    2,    0,    0} , // 8 pipes (2 PKRs) 4 bpe @ SW_256_S @ RbPlus
    {   1,    3,    0,    0} , // 8 pipes (2 PKRs) 8 bpe @ SW_256_S @ RbPlus
    {   1,    4,    0,    0} , // 8 pipes (2 PKRs) 16 bpe @ SW_256_S @ RbPlus
    {   1,    0,    0,    0} , // 4 pipes (4 PKRs) 1 bpe @ SW_256_S @ RbPlus
    {   1,    1,    0,    0} , // 4 pipes (4 PKRs) 2 bpe @ SW_256_S @ RbPlus
    {   1,    2,    0,    0} , // 4 pipes (4 PKRs) 4 bpe @ SW_256_S @ RbPlus
    {   1,    3,    0,    0} , // 4 pipes (4 PKRs) 8 bpe @ SW_256_S @ RbPlus
    {   1,    4,    0,    0} , // 4 pipes (4 PKRs) 16 bpe @ SW_256_S @ RbPlus
    {   1,    0,    0,    0} , // 8 pipes (4 PKRs) 1 bpe @ SW_256_S @ RbPlus
    {   1,    1,    0,    0} , // 8 pipes (4 PKRs) 2 bpe @ SW_256_S @ RbPlus
    {   1,    2,    0,    0} , // 8 pipes (4 PKRs) 4 bpe @ SW_256_S @ RbPlus
    {   1,    3,    0,    0} , // 8 pipes (4 PKRs) 8 bpe @ SW_256_S @ RbPlus
    {   1,    4,    0,    0} , // 8 pipes (4 PKRs) 16 bpe @ SW_256_S @ RbPlus
    {   1,    0,    0,    0} , // 16 pipes (4 PKRs) 1 bpe @ SW_256_S @ RbPlus
    {   1,    1,    0,    0} , // 16 pipes (4 PKRs) 2 bpe @ SW_256_S @ RbPlus
    {   1,    2,    0,    0} , // 16 pipes (4 PKRs) 4 bpe @ SW_256_S @ RbPlus
    {   1,    3,    0,    0} , // 16 pipes (4 PKRs) 8 bpe @ SW_256_S @ RbPlus
    {   1,    4,    0,    0} , // 16 pipes (4 PKRs) 16 bpe @ SW_256_S @ RbPlus
    {   1,    0,    0,    0} , // 8 pipes (8 PKRs) 1 bpe @ SW_256_S @ RbPlus
    {   1,    1,    0,    0} , // 8 pipes (8 PKRs) 2 bpe @ SW_256_S @ RbPlus
    {   1,    2,    0,    0} , // 8 pipes (8 PKRs) 4 bpe @ SW_256_S @ RbPlus
    {   1,    3,    0,    0} , // 8 pipes (8 PKRs) 8 bpe @ SW_256_S @ RbPlus
    {   1,    4,    0,    0} , // 8 pipes (8 PKRs) 16 bpe @ SW_256_S @ RbPlus
    {   1,    0,    0,    0} , // 16 pipes (8 PKRs) 1 bpe @ SW_256_S @ RbPlus
    {   1,    1,    0,    0} , // 16 pipes (8 PKRs) 2 bpe @ SW_256_S @ RbPlus
    {   1,    2,    0,    0} , // 16 pipes (8 PKRs) 4 bpe @ SW_256_S @ RbPlus
    {   1,    3,    0,    0} , // 16 pipes (8 PKRs) 8 bpe @ SW_256_S @ RbPlus
    {   1,    4,    0,    0} , // 16 pipes (8 PKRs) 16 bpe @ SW_256_S @ RbPlus
    {   1,    0,    0,    0} , // 32 pipes (8 PKRs) 1 bpe @ SW_256_S @ RbPlus
    {   1,    1,    0,    0} , // 32 pipes (8 PKRs) 2 bpe @ SW_256_S @ RbPlus
    {   1,    2,    0,    0} , // 32 pipes (8 PKRs) 4 bpe @ SW_256_S @ RbPlus
    {   1,    3,    0,    0} , // 32 pipes (8 PKRs) 8 bpe @ SW_256_S @ RbPlus
    {   1,    4,    0,    0} , // 32 pipes (8 PKRs) 16 bpe @ SW_256_S @ RbPlus
    {   1,    0,    0,    0} , // 16 pipes (16 PKRs) 1 bpe @ SW_256_S @ RbPlus
    {   1,    1,    0,    0} , // 16 pipes (16 PKRs) 2 bpe @ SW_256_S @ RbPlus
    {   1,    2,    0,    0} , // 16 pipes (16 PKRs) 4 bpe @ SW_256_S @ RbPlus
    {   1,    3,    0,    0} , // 16 pipes (16 PKRs) 8 bpe @ SW_256_S @ RbPlus
    {   1,    4,    0,    0} , // 16 pipes (16 PKRs) 16 bpe @ SW_256_S @ RbPlus
    {   1,    0,    0,    0} , // 32 pipes (16 PKRs) 1 bpe @ SW_256_S @ RbPlus
    {   1,    1,    0,    0} , // 32 pipes (16 PKRs) 2 bpe @ SW_256_S @ RbPlus
    {   1,    2,    0,    0} , // 32 pipes (16 PKRs) 4 bpe @ SW_256_S @ RbPlus
    {   1,    3,    0,    0} , // 32 pipes (16 PKRs) 8 bpe @ SW_256_S @ RbPlus
    {   1,    4,    0,    0} , // 32 pipes (16 PKRs) 16 bpe @ SW_256_S @ RbPlus
    {   1,    0,    0,    0} , // 64 pipes (16 PKRs) 1 bpe @ SW_256_S @ RbPlus
    {   1,    1,    0,    0} , // 64 pipes (16 PKRs) 2 bpe @ SW_256_S @ RbPlus
    {   1,    2,    0,    0} , // 64 pipes (16 PKRs) 4 bpe @ SW_256_S @ RbPlus
    {   1,    3,    0,    0} , // 64 pipes (16 PKRs) 8 bpe @ SW_256_S @ RbPlus
    {   1,    4,    0,    0} , // 64 pipes (16 PKRs) 16 bpe @ SW_256_S @ RbPlus
    {   1,    0,    0,    0} , // 32 pipes (32 PKRs) 1 bpe @ SW_256_S @ RbPlus
    {   1,    1,    0,    0} , // 32 pipes (32 PKRs) 2 bpe @ SW_256_S @ RbPlus
    {   1,    2,    0,    0} , // 32 pipes (32 PKRs) 4 bpe @ SW_256_S @ RbPlus
    {   1,    3,    0,    0} , // 32 pipes (32 PKRs) 8 bpe @ SW_256_S @ RbPlus
    {   1,    4,    0,    0} , // 32 pipes (32 PKRs) 16 bpe @ SW_256_S @ RbPlus
    {   1,    0,    0,    0} , // 64 pipes (32 PKRs) 1 bpe @ SW_256_S @ RbPlus
    {   1,    1,    0,    0} , // 64 pipes (32 PKRs) 2 bpe @ SW_256_S @ RbPlus
    {   1,    2,    0,    0} , // 64 pipes (32 PKRs) 4 bpe @ SW_256_S @ RbPlus
    {   1,    3,    0,    0} , // 64 pipes (32 PKRs) 8 bpe @ SW_256_S @ RbPlus
    {   1,    4,    0,    0} , // 64 pipes (32 PKRs) 16 bpe @ SW_256_S @ RbPlus
};

const ADDR_SW_PATINFO SW_256_D_RBPLUS_PATINFO[] =
{
    {   1,    5,    0,    0} , // 1 pipes (1 PKRs) 1 bpe @ SW_256_D @ RbPlus
    {   1,    1,    0,    0} , // 1 pipes (1 PKRs) 2 bpe @ SW_256_D @ RbPlus
    {   1,   39,    0,    0} , // 1 pipes (1 PKRs) 4 bpe @ SW_256_D @ RbPlus
    {   1,    6,    0,    0} , // 1 pipes (1 PKRs) 8 bpe @ SW_256_D @ RbPlus
    {   1,    7,    0,    0} , // 1 pipes (1 PKRs) 16 bpe @ SW_256_D @ RbPlus
    {   1,    5,    0,    0} , // 2 pipes (1-2 PKRs) 1 bpe @ SW_256_D @ RbPlus
    {   1,    1,    0,    0} , // 2 pipes (1-2 PKRs) 2 bpe @ SW_256_D @ RbPlus
    {   1,   39,    0,    0} , // 2 pipes (1-2 PKRs) 4 bpe @ SW_256_D @ RbPlus
    {   1,    6,    0,    0} , // 2 pipes (1-2 PKRs) 8 bpe @ SW_256_D @ RbPlus
    {   1,    7,    0,    0} , // 2 pipes (1-2 PKRs) 16 bpe @ SW_256_D @ RbPlus
    {   1,    5,    0,    0} , // 4 pipes (1-2 PKRs) 1 bpe @ SW_256_D @ RbPlus
    {   1,    1,    0,    0} , // 4 pipes (1-2 PKRs) 2 bpe @ SW_256_D @ RbPlus
    {   1,   39,    0,    0} , // 4 pipes (1-2 PKRs) 4 bpe @ SW_256_D @ RbPlus
    {   1,    6,    0,    0} , // 4 pipes (1-2 PKRs) 8 bpe @ SW_256_D @ RbPlus
    {   1,    7,    0,    0} , // 4 pipes (1-2 PKRs) 16 bpe @ SW_256_D @ RbPlus
    {   1,    5,    0,    0} , // 8 pipes (2 PKRs) 1 bpe @ SW_256_D @ RbPlus
    {   1,    1,    0,    0} , // 8 pipes (2 PKRs) 2 bpe @ SW_256_D @ RbPlus
    {   1,   39,    0,    0} , // 8 pipes (2 PKRs) 4 bpe @ SW_256_D @ RbPlus
    {   1,    6,    0,    0} , // 8 pipes (2 PKRs) 8 bpe @ SW_256_D @ RbPlus
    {   1,    7,    0,    0} , // 8 pipes (2 PKRs) 16 bpe @ SW_256_D @ RbPlus
    {   1,    5,    0,    0} , // 4 pipes (4 PKRs) 1 bpe @ SW_256_D @ RbPlus
    {   1,    1,    0,    0} , // 4 pipes (4 PKRs) 2 bpe @ SW_256_D @ RbPlus
    {   1,   39,    0,    0} , // 4 pipes (4 PKRs) 4 bpe @ SW_256_D @ RbPlus
    {   1,    6,    0,    0} , // 4 pipes (4 PKRs) 8 bpe @ SW_256_D @ RbPlus
    {   1,    7,    0,    0} , // 4 pipes (4 PKRs) 16 bpe @ SW_256_D @ RbPlus
    {   1,    5,    0,    0} , // 8 pipes (4 PKRs) 1 bpe @ SW_256_D @ RbPlus
    {   1,    1,    0,    0} , // 8 pipes (4 PKRs) 2 bpe @ SW_256_D @ RbPlus
    {   1,   39,    0,    0} , // 8 pipes (4 PKRs) 4 bpe @ SW_256_D @ RbPlus
    {   1,    6,    0,    0} , // 8 pipes (4 PKRs) 8 bpe @ SW_256_D @ RbPlus
    {   1,    7,    0,    0} , // 8 pipes (4 PKRs) 16 bpe @ SW_256_D @ RbPlus
    {   1,    5,    0,    0} , // 16 pipes (4 PKRs) 1 bpe @ SW_256_D @ RbPlus
    {   1,    1,    0,    0} , // 16 pipes (4 PKRs) 2 bpe @ SW_256_D @ RbPlus
    {   1,   39,    0,    0} , // 16 pipes (4 PKRs) 4 bpe @ SW_256_D @ RbPlus
    {   1,    6,    0,    0} , // 16 pipes (4 PKRs) 8 bpe @ SW_256_D @ RbPlus
    {   1,    7,    0,    0} , // 16 pipes (4 PKRs) 16 bpe @ SW_256_D @ RbPlus
    {   1,    5,    0,    0} , // 8 pipes (8 PKRs) 1 bpe @ SW_256_D @ RbPlus
    {   1,    1,    0,    0} , // 8 pipes (8 PKRs) 2 bpe @ SW_256_D @ RbPlus
    {   1,   39,    0,    0} , // 8 pipes (8 PKRs) 4 bpe @ SW_256_D @ RbPlus
    {   1,    6,    0,    0} , // 8 pipes (8 PKRs) 8 bpe @ SW_256_D @ RbPlus
    {   1,    7,    0,    0} , // 8 pipes (8 PKRs) 16 bpe @ SW_256_D @ RbPlus
    {   1,    5,    0,    0} , // 16 pipes (8 PKRs) 1 bpe @ SW_256_D @ RbPlus
    {   1,    1,    0,    0} , // 16 pipes (8 PKRs) 2 bpe @ SW_256_D @ RbPlus
    {   1,   39,    0,    0} , // 16 pipes (8 PKRs) 4 bpe @ SW_256_D @ RbPlus
    {   1,    6,    0,    0} , // 16 pipes (8 PKRs) 8 bpe @ SW_256_D @ RbPlus
    {   1,    7,    0,    0} , // 16 pipes (8 PKRs) 16 bpe @ SW_256_D @ RbPlus
    {   1,    5,    0,    0} , // 32 pipes (8 PKRs) 1 bpe @ SW_256_D @ RbPlus
    {   1,    1,    0,    0} , // 32 pipes (8 PKRs) 2 bpe @ SW_256_D @ RbPlus
    {   1,   39,    0,    0} , // 32 pipes (8 PKRs) 4 bpe @ SW_256_D @ RbPlus
    {   1,    6,    0,    0} , // 32 pipes (8 PKRs) 8 bpe @ SW_256_D @ RbPlus
    {   1,    7,    0,    0} , // 32 pipes (8 PKRs) 16 bpe @ SW_256_D @ RbPlus
    {   1,    5,    0,    0} , // 16 pipes (16 PKRs) 1 bpe @ SW_256_D @ RbPlus
    {   1,    1,    0,    0} , // 16 pipes (16 PKRs) 2 bpe @ SW_256_D @ RbPlus
    {   1,   39,    0,    0} , // 16 pipes (16 PKRs) 4 bpe @ SW_256_D @ RbPlus
    {   1,    6,    0,    0} , // 16 pipes (16 PKRs) 8 bpe @ SW_256_D @ RbPlus
    {   1,    7,    0,    0} , // 16 pipes (16 PKRs) 16 bpe @ SW_256_D @ RbPlus
    {   1,    5,    0,    0} , // 32 pipes (16 PKRs) 1 bpe @ SW_256_D @ RbPlus
    {   1,    1,    0,    0} , // 32 pipes (16 PKRs) 2 bpe @ SW_256_D @ RbPlus
    {   1,   39,    0,    0} , // 32 pipes (16 PKRs) 4 bpe @ SW_256_D @ RbPlus
    {   1,    6,    0,    0} , // 32 pipes (16 PKRs) 8 bpe @ SW_256_D @ RbPlus
    {   1,    7,    0,    0} , // 32 pipes (16 PKRs) 16 bpe @ SW_256_D @ RbPlus
    {   1,    5,    0,    0} , // 64 pipes (16 PKRs) 1 bpe @ SW_256_D @ RbPlus
    {   1,    1,    0,    0} , // 64 pipes (16 PKRs) 2 bpe @ SW_256_D @ RbPlus
    {   1,   39,    0,    0} , // 64 pipes (16 PKRs) 4 bpe @ SW_256_D @ RbPlus
    {   1,    6,    0,    0} , // 64 pipes (16 PKRs) 8 bpe @ SW_256_D @ RbPlus
    {   1,    7,    0,    0} , // 64 pipes (16 PKRs) 16 bpe @ SW_256_D @ RbPlus
    {   1,    5,    0,    0} , // 32 pipes (32 PKRs) 1 bpe @ SW_256_D @ RbPlus
    {   1,    1,    0,    0} , // 32 pipes (32 PKRs) 2 bpe @ SW_256_D @ RbPlus
    {   1,   39,    0,    0} , // 32 pipes (32 PKRs) 4 bpe @ SW_256_D @ RbPlus
    {   1,    6,    0,    0} , // 32 pipes (32 PKRs) 8 bpe @ SW_256_D @ RbPlus
    {   1,    7,    0,    0} , // 32 pipes (32 PKRs) 16 bpe @ SW_256_D @ RbPlus
    {   1,    5,    0,    0} , // 64 pipes (32 PKRs) 1 bpe @ SW_256_D @ RbPlus
    {   1,    1,    0,    0} , // 64 pipes (32 PKRs) 2 bpe @ SW_256_D @ RbPlus
    {   1,   39,    0,    0} , // 64 pipes (32 PKRs) 4 bpe @ SW_256_D @ RbPlus
    {   1,    6,    0,    0} , // 64 pipes (32 PKRs) 8 bpe @ SW_256_D @ RbPlus
    {   1,    7,    0,    0} , // 64 pipes (32 PKRs) 16 bpe @ SW_256_D @ RbPlus
};

const ADDR_SW_PATINFO SW_4K_S_RBPLUS_PATINFO[] =
{
    {   1,    0,    1,    0} , // 1 pipes (1 PKRs) 1 bpe @ SW_4K_S @ RbPlus
    {   1,    1,    2,    0} , // 1 pipes (1 PKRs) 2 bpe @ SW_4K_S @ RbPlus
    {   1,    2,    3,    0} , // 1 pipes (1 PKRs) 4 bpe @ SW_4K_S @ RbPlus
    {   1,    3,    4,    0} , // 1 pipes (1 PKRs) 8 bpe @ SW_4K_S @ RbPlus
    {   1,    4,    5,    0} , // 1 pipes (1 PKRs) 16 bpe @ SW_4K_S @ RbPlus
    {   1,    0,    1,    0} , // 2 pipes (1-2 PKRs) 1 bpe @ SW_4K_S @ RbPlus
    {   1,    1,    2,    0} , // 2 pipes (1-2 PKRs) 2 bpe @ SW_4K_S @ RbPlus
    {   1,    2,    3,    0} , // 2 pipes (1-2 PKRs) 4 bpe @ SW_4K_S @ RbPlus
    {   1,    3,    4,    0} , // 2 pipes (1-2 PKRs) 8 bpe @ SW_4K_S @ RbPlus
    {   1,    4,    5,    0} , // 2 pipes (1-2 PKRs) 16 bpe @ SW_4K_S @ RbPlus
    {   1,    0,    1,    0} , // 4 pipes (1-2 PKRs) 1 bpe @ SW_4K_S @ RbPlus
    {   1,    1,    2,    0} , // 4 pipes (1-2 PKRs) 2 bpe @ SW_4K_S @ RbPlus
    {   1,    2,    3,    0} , // 4 pipes (1-2 PKRs) 4 bpe @ SW_4K_S @ RbPlus
    {   1,    3,    4,    0} , // 4 pipes (1-2 PKRs) 8 bpe @ SW_4K_S @ RbPlus
    {   1,    4,    5,    0} , // 4 pipes (1-2 PKRs) 16 bpe @ SW_4K_S @ RbPlus
    {   1,    0,    1,    0} , // 8 pipes (2 PKRs) 1 bpe @ SW_4K_S @ RbPlus
    {   1,    1,    2,    0} , // 8 pipes (2 PKRs) 2 bpe @ SW_4K_S @ RbPlus
    {   1,    2,    3,    0} , // 8 pipes (2 PKRs) 4 bpe @ SW_4K_S @ RbPlus
    {   1,    3,    4,    0} , // 8 pipes (2 PKRs) 8 bpe @ SW_4K_S @ RbPlus
    {   1,    4,    5,    0} , // 8 pipes (2 PKRs) 16 bpe @ SW_4K_S @ RbPlus
    {   1,    0,    1,    0} , // 4 pipes (4 PKRs) 1 bpe @ SW_4K_S @ RbPlus
    {   1,    1,    2,    0} , // 4 pipes (4 PKRs) 2 bpe @ SW_4K_S @ RbPlus
    {   1,    2,    3,    0} , // 4 pipes (4 PKRs) 4 bpe @ SW_4K_S @ RbPlus
    {   1,    3,    4,    0} , // 4 pipes (4 PKRs) 8 bpe @ SW_4K_S @ RbPlus
    {   1,    4,    5,    0} , // 4 pipes (4 PKRs) 16 bpe @ SW_4K_S @ RbPlus
    {   1,    0,    1,    0} , // 8 pipes (4 PKRs) 1 bpe @ SW_4K_S @ RbPlus
    {   1,    1,    2,    0} , // 8 pipes (4 PKRs) 2 bpe @ SW_4K_S @ RbPlus
    {   1,    2,    3,    0} , // 8 pipes (4 PKRs) 4 bpe @ SW_4K_S @ RbPlus
    {   1,    3,    4,    0} , // 8 pipes (4 PKRs) 8 bpe @ SW_4K_S @ RbPlus
    {   1,    4,    5,    0} , // 8 pipes (4 PKRs) 16 bpe @ SW_4K_S @ RbPlus
    {   1,    0,    1,    0} , // 16 pipes (4 PKRs) 1 bpe @ SW_4K_S @ RbPlus
    {   1,    1,    2,    0} , // 16 pipes (4 PKRs) 2 bpe @ SW_4K_S @ RbPlus
    {   1,    2,    3,    0} , // 16 pipes (4 PKRs) 4 bpe @ SW_4K_S @ RbPlus
    {   1,    3,    4,    0} , // 16 pipes (4 PKRs) 8 bpe @ SW_4K_S @ RbPlus
    {   1,    4,    5,    0} , // 16 pipes (4 PKRs) 16 bpe @ SW_4K_S @ RbPlus
    {   1,    0,    1,    0} , // 8 pipes (8 PKRs) 1 bpe @ SW_4K_S @ RbPlus
    {   1,    1,    2,    0} , // 8 pipes (8 PKRs) 2 bpe @ SW_4K_S @ RbPlus
    {   1,    2,    3,    0} , // 8 pipes (8 PKRs) 4 bpe @ SW_4K_S @ RbPlus
    {   1,    3,    4,    0} , // 8 pipes (8 PKRs) 8 bpe @ SW_4K_S @ RbPlus
    {   1,    4,    5,    0} , // 8 pipes (8 PKRs) 16 bpe @ SW_4K_S @ RbPlus
    {   1,    0,    1,    0} , // 16 pipes (8 PKRs) 1 bpe @ SW_4K_S @ RbPlus
    {   1,    1,    2,    0} , // 16 pipes (8 PKRs) 2 bpe @ SW_4K_S @ RbPlus
    {   1,    2,    3,    0} , // 16 pipes (8 PKRs) 4 bpe @ SW_4K_S @ RbPlus
    {   1,    3,    4,    0} , // 16 pipes (8 PKRs) 8 bpe @ SW_4K_S @ RbPlus
    {   1,    4,    5,    0} , // 16 pipes (8 PKRs) 16 bpe @ SW_4K_S @ RbPlus
    {   1,    0,    1,    0} , // 32 pipes (8 PKRs) 1 bpe @ SW_4K_S @ RbPlus
    {   1,    1,    2,    0} , // 32 pipes (8 PKRs) 2 bpe @ SW_4K_S @ RbPlus
    {   1,    2,    3,    0} , // 32 pipes (8 PKRs) 4 bpe @ SW_4K_S @ RbPlus
    {   1,    3,    4,    0} , // 32 pipes (8 PKRs) 8 bpe @ SW_4K_S @ RbPlus
    {   1,    4,    5,    0} , // 32 pipes (8 PKRs) 16 bpe @ SW_4K_S @ RbPlus
    {   1,    0,    1,    0} , // 16 pipes (16 PKRs) 1 bpe @ SW_4K_S @ RbPlus
    {   1,    1,    2,    0} , // 16 pipes (16 PKRs) 2 bpe @ SW_4K_S @ RbPlus
    {   1,    2,    3,    0} , // 16 pipes (16 PKRs) 4 bpe @ SW_4K_S @ RbPlus
    {   1,    3,    4,    0} , // 16 pipes (16 PKRs) 8 bpe @ SW_4K_S @ RbPlus
    {   1,    4,    5,    0} , // 16 pipes (16 PKRs) 16 bpe @ SW_4K_S @ RbPlus
    {   1,    0,    1,    0} , // 32 pipes (16 PKRs) 1 bpe @ SW_4K_S @ RbPlus
    {   1,    1,    2,    0} , // 32 pipes (16 PKRs) 2 bpe @ SW_4K_S @ RbPlus
    {   1,    2,    3,    0} , // 32 pipes (16 PKRs) 4 bpe @ SW_4K_S @ RbPlus
    {   1,    3,    4,    0} , // 32 pipes (16 PKRs) 8 bpe @ SW_4K_S @ RbPlus
    {   1,    4,    5,    0} , // 32 pipes (16 PKRs) 16 bpe @ SW_4K_S @ RbPlus
    {   1,    0,    1,    0} , // 64 pipes (16 PKRs) 1 bpe @ SW_4K_S @ RbPlus
    {   1,    1,    2,    0} , // 64 pipes (16 PKRs) 2 bpe @ SW_4K_S @ RbPlus
    {   1,    2,    3,    0} , // 64 pipes (16 PKRs) 4 bpe @ SW_4K_S @ RbPlus
    {   1,    3,    4,    0} , // 64 pipes (16 PKRs) 8 bpe @ SW_4K_S @ RbPlus
    {   1,    4,    5,    0} , // 64 pipes (16 PKRs) 16 bpe @ SW_4K_S @ RbPlus
    {   1,    0,    1,    0} , // 32 pipes (32 PKRs) 1 bpe @ SW_4K_S @ RbPlus
    {   1,    1,    2,    0} , // 32 pipes (32 PKRs) 2 bpe @ SW_4K_S @ RbPlus
    {   1,    2,    3,    0} , // 32 pipes (32 PKRs) 4 bpe @ SW_4K_S @ RbPlus
    {   1,    3,    4,    0} , // 32 pipes (32 PKRs) 8 bpe @ SW_4K_S @ RbPlus
    {   1,    4,    5,    0} , // 32 pipes (32 PKRs) 16 bpe @ SW_4K_S @ RbPlus
    {   1,    0,    1,    0} , // 64 pipes (32 PKRs) 1 bpe @ SW_4K_S @ RbPlus
    {   1,    1,    2,    0} , // 64 pipes (32 PKRs) 2 bpe @ SW_4K_S @ RbPlus
    {   1,    2,    3,    0} , // 64 pipes (32 PKRs) 4 bpe @ SW_4K_S @ RbPlus
    {   1,    3,    4,    0} , // 64 pipes (32 PKRs) 8 bpe @ SW_4K_S @ RbPlus
    {   1,    4,    5,    0} , // 64 pipes (32 PKRs) 16 bpe @ SW_4K_S @ RbPlus
};

const ADDR_SW_PATINFO SW_4K_D_RBPLUS_PATINFO[] =
{
    {   1,    5,    1,    0} , // 1 pipes (1 PKRs) 1 bpe @ SW_4K_D @ RbPlus
    {   1,    1,    2,    0} , // 1 pipes (1 PKRs) 2 bpe @ SW_4K_D @ RbPlus
    {   1,   39,    3,    0} , // 1 pipes (1 PKRs) 4 bpe @ SW_4K_D @ RbPlus
    {   1,    6,    4,    0} , // 1 pipes (1 PKRs) 8 bpe @ SW_4K_D @ RbPlus
    {   1,    7,    5,    0} , // 1 pipes (1 PKRs) 16 bpe @ SW_4K_D @ RbPlus
    {   1,    5,    1,    0} , // 2 pipes (1-2 PKRs) 1 bpe @ SW_4K_D @ RbPlus
    {   1,    1,    2,    0} , // 2 pipes (1-2 PKRs) 2 bpe @ SW_4K_D @ RbPlus
    {   1,   39,    3,    0} , // 2 pipes (1-2 PKRs) 4 bpe @ SW_4K_D @ RbPlus
    {   1,    6,    4,    0} , // 2 pipes (1-2 PKRs) 8 bpe @ SW_4K_D @ RbPlus
    {   1,    7,    5,    0} , // 2 pipes (1-2 PKRs) 16 bpe @ SW_4K_D @ RbPlus
    {   1,    5,    1,    0} , // 4 pipes (1-2 PKRs) 1 bpe @ SW_4K_D @ RbPlus
    {   1,    1,    2,    0} , // 4 pipes (1-2 PKRs) 2 bpe @ SW_4K_D @ RbPlus
    {   1,   39,    3,    0} , // 4 pipes (1-2 PKRs) 4 bpe @ SW_4K_D @ RbPlus
    {   1,    6,    4,    0} , // 4 pipes (1-2 PKRs) 8 bpe @ SW_4K_D @ RbPlus
    {   1,    7,    5,    0} , // 4 pipes (1-2 PKRs) 16 bpe @ SW_4K_D @ RbPlus
    {   1,    5,    1,    0} , // 8 pipes (2 PKRs) 1 bpe @ SW_4K_D @ RbPlus
    {   1,    1,    2,    0} , // 8 pipes (2 PKRs) 2 bpe @ SW_4K_D @ RbPlus
    {   1,   39,    3,    0} , // 8 pipes (2 PKRs) 4 bpe @ SW_4K_D @ RbPlus
    {   1,    6,    4,    0} , // 8 pipes (2 PKRs) 8 bpe @ SW_4K_D @ RbPlus
    {   1,    7,    5,    0} , // 8 pipes (2 PKRs) 16 bpe @ SW_4K_D @ RbPlus
    {   1,    5,    1,    0} , // 4 pipes (4 PKRs) 1 bpe @ SW_4K_D @ RbPlus
    {   1,    1,    2,    0} , // 4 pipes (4 PKRs) 2 bpe @ SW_4K_D @ RbPlus
    {   1,   39,    3,    0} , // 4 pipes (4 PKRs) 4 bpe @ SW_4K_D @ RbPlus
    {   1,    6,    4,    0} , // 4 pipes (4 PKRs) 8 bpe @ SW_4K_D @ RbPlus
    {   1,    7,    5,    0} , // 4 pipes (4 PKRs) 16 bpe @ SW_4K_D @ RbPlus
    {   1,    5,    1,    0} , // 8 pipes (4 PKRs) 1 bpe @ SW_4K_D @ RbPlus
    {   1,    1,    2,    0} , // 8 pipes (4 PKRs) 2 bpe @ SW_4K_D @ RbPlus
    {   1,   39,    3,    0} , // 8 pipes (4 PKRs) 4 bpe @ SW_4K_D @ RbPlus
    {   1,    6,    4,    0} , // 8 pipes (4 PKRs) 8 bpe @ SW_4K_D @ RbPlus
    {   1,    7,    5,    0} , // 8 pipes (4 PKRs) 16 bpe @ SW_4K_D @ RbPlus
    {   1,    5,    1,    0} , // 16 pipes (4 PKRs) 1 bpe @ SW_4K_D @ RbPlus
    {   1,    1,    2,    0} , // 16 pipes (4 PKRs) 2 bpe @ SW_4K_D @ RbPlus
    {   1,   39,    3,    0} , // 16 pipes (4 PKRs) 4 bpe @ SW_4K_D @ RbPlus
    {   1,    6,    4,    0} , // 16 pipes (4 PKRs) 8 bpe @ SW_4K_D @ RbPlus
    {   1,    7,    5,    0} , // 16 pipes (4 PKRs) 16 bpe @ SW_4K_D @ RbPlus
    {   1,    5,    1,    0} , // 8 pipes (8 PKRs) 1 bpe @ SW_4K_D @ RbPlus
    {   1,    1,    2,    0} , // 8 pipes (8 PKRs) 2 bpe @ SW_4K_D @ RbPlus
    {   1,   39,    3,    0} , // 8 pipes (8 PKRs) 4 bpe @ SW_4K_D @ RbPlus
    {   1,    6,    4,    0} , // 8 pipes (8 PKRs) 8 bpe @ SW_4K_D @ RbPlus
    {   1,    7,    5,    0} , // 8 pipes (8 PKRs) 16 bpe @ SW_4K_D @ RbPlus
    {   1,    5,    1,    0} , // 16 pipes (8 PKRs) 1 bpe @ SW_4K_D @ RbPlus
    {   1,    1,    2,    0} , // 16 pipes (8 PKRs) 2 bpe @ SW_4K_D @ RbPlus
    {   1,   39,    3,    0} , // 16 pipes (8 PKRs) 4 bpe @ SW_4K_D @ RbPlus
    {   1,    6,    4,    0} , // 16 pipes (8 PKRs) 8 bpe @ SW_4K_D @ RbPlus
    {   1,    7,    5,    0} , // 16 pipes (8 PKRs) 16 bpe @ SW_4K_D @ RbPlus
    {   1,    5,    1,    0} , // 32 pipes (8 PKRs) 1 bpe @ SW_4K_D @ RbPlus
    {   1,    1,    2,    0} , // 32 pipes (8 PKRs) 2 bpe @ SW_4K_D @ RbPlus
    {   1,   39,    3,    0} , // 32 pipes (8 PKRs) 4 bpe @ SW_4K_D @ RbPlus
    {   1,    6,    4,    0} , // 32 pipes (8 PKRs) 8 bpe @ SW_4K_D @ RbPlus
    {   1,    7,    5,    0} , // 32 pipes (8 PKRs) 16 bpe @ SW_4K_D @ RbPlus
    {   1,    5,    1,    0} , // 16 pipes (16 PKRs) 1 bpe @ SW_4K_D @ RbPlus
    {   1,    1,    2,    0} , // 16 pipes (16 PKRs) 2 bpe @ SW_4K_D @ RbPlus
    {   1,   39,    3,    0} , // 16 pipes (16 PKRs) 4 bpe @ SW_4K_D @ RbPlus
    {   1,    6,    4,    0} , // 16 pipes (16 PKRs) 8 bpe @ SW_4K_D @ RbPlus
    {   1,    7,    5,    0} , // 16 pipes (16 PKRs) 16 bpe @ SW_4K_D @ RbPlus
    {   1,    5,    1,    0} , // 32 pipes (16 PKRs) 1 bpe @ SW_4K_D @ RbPlus
    {   1,    1,    2,    0} , // 32 pipes (16 PKRs) 2 bpe @ SW_4K_D @ RbPlus
    {   1,   39,    3,    0} , // 32 pipes (16 PKRs) 4 bpe @ SW_4K_D @ RbPlus
    {   1,    6,    4,    0} , // 32 pipes (16 PKRs) 8 bpe @ SW_4K_D @ RbPlus
    {   1,    7,    5,    0} , // 32 pipes (16 PKRs) 16 bpe @ SW_4K_D @ RbPlus
    {   1,    5,    1,    0} , // 64 pipes (16 PKRs) 1 bpe @ SW_4K_D @ RbPlus
    {   1,    1,    2,    0} , // 64 pipes (16 PKRs) 2 bpe @ SW_4K_D @ RbPlus
    {   1,   39,    3,    0} , // 64 pipes (16 PKRs) 4 bpe @ SW_4K_D @ RbPlus
    {   1,    6,    4,    0} , // 64 pipes (16 PKRs) 8 bpe @ SW_4K_D @ RbPlus
    {   1,    7,    5,    0} , // 64 pipes (16 PKRs) 16 bpe @ SW_4K_D @ RbPlus
    {   1,    5,    1,    0} , // 32 pipes (32 PKRs) 1 bpe @ SW_4K_D @ RbPlus
    {   1,    1,    2,    0} , // 32 pipes (32 PKRs) 2 bpe @ SW_4K_D @ RbPlus
    {   1,   39,    3,    0} , // 32 pipes (32 PKRs) 4 bpe @ SW_4K_D @ RbPlus
    {   1,    6,    4,    0} , // 32 pipes (32 PKRs) 8 bpe @ SW_4K_D @ RbPlus
    {   1,    7,    5,    0} , // 32 pipes (32 PKRs) 16 bpe @ SW_4K_D @ RbPlus
    {   1,    5,    1,    0} , // 64 pipes (32 PKRs) 1 bpe @ SW_4K_D @ RbPlus
    {   1,    1,    2,    0} , // 64 pipes (32 PKRs) 2 bpe @ SW_4K_D @ RbPlus
    {   1,   39,    3,    0} , // 64 pipes (32 PKRs) 4 bpe @ SW_4K_D @ RbPlus
    {   1,    6,    4,    0} , // 64 pipes (32 PKRs) 8 bpe @ SW_4K_D @ RbPlus
    {   1,    7,    5,    0} , // 64 pipes (32 PKRs) 16 bpe @ SW_4K_D @ RbPlus
};

const ADDR_SW_PATINFO SW_4K_S_X_RBPLUS_PATINFO[] =
{
    {   1,    0,    1,    0} , // 1 pipes (1 PKRs) 1 bpe @ SW_4K_S_X @ RbPlus
    {   1,    1,    2,    0} , // 1 pipes (1 PKRs) 2 bpe @ SW_4K_S_X @ RbPlus
    {   1,    2,    3,    0} , // 1 pipes (1 PKRs) 4 bpe @ SW_4K_S_X @ RbPlus
    {   1,    3,    4,    0} , // 1 pipes (1 PKRs) 8 bpe @ SW_4K_S_X @ RbPlus
    {   1,    4,    5,    0} , // 1 pipes (1 PKRs) 16 bpe @ SW_4K_S_X @ RbPlus
    {   3,    0,    6,    0} , // 2 pipes (1-2 PKRs) 1 bpe @ SW_4K_S_X @ RbPlus
    {   3,    1,    7,    0} , // 2 pipes (1-2 PKRs) 2 bpe @ SW_4K_S_X @ RbPlus
    {   3,    2,    8,    0} , // 2 pipes (1-2 PKRs) 4 bpe @ SW_4K_S_X @ RbPlus
    {   3,    3,    9,    0} , // 2 pipes (1-2 PKRs) 8 bpe @ SW_4K_S_X @ RbPlus
    {   3,    4,   10,    0} , // 2 pipes (1-2 PKRs) 16 bpe @ SW_4K_S_X @ RbPlus
    {   3,    0,  210,    0} , // 4 pipes (1-2 PKRs) 1 bpe @ SW_4K_S_X @ RbPlus
    {   3,    1,  211,    0} , // 4 pipes (1-2 PKRs) 2 bpe @ SW_4K_S_X @ RbPlus
    {   3,    2,  212,    0} , // 4 pipes (1-2 PKRs) 4 bpe @ SW_4K_S_X @ RbPlus
    {   3,    3,  213,    0} , // 4 pipes (1-2 PKRs) 8 bpe @ SW_4K_S_X @ RbPlus
    {   3,    4,  214,    0} , // 4 pipes (1-2 PKRs) 16 bpe @ SW_4K_S_X @ RbPlus
    {   3,    0,  215,    0} , // 8 pipes (2 PKRs) 1 bpe @ SW_4K_S_X @ RbPlus
    {   3,    1,  216,    0} , // 8 pipes (2 PKRs) 2 bpe @ SW_4K_S_X @ RbPlus
    {   3,    2,  217,    0} , // 8 pipes (2 PKRs) 4 bpe @ SW_4K_S_X @ RbPlus
    {   3,    3,  218,    0} , // 8 pipes (2 PKRs) 8 bpe @ SW_4K_S_X @ RbPlus
    {   3,    4,  219,    0} , // 8 pipes (2 PKRs) 16 bpe @ SW_4K_S_X @ RbPlus
    {   3,    0,   11,    0} , // 4 pipes (4 PKRs) 1 bpe @ SW_4K_S_X @ RbPlus
    {   3,    1,   12,    0} , // 4 pipes (4 PKRs) 2 bpe @ SW_4K_S_X @ RbPlus
    {   3,    2,   13,    0} , // 4 pipes (4 PKRs) 4 bpe @ SW_4K_S_X @ RbPlus
    {   3,    3,   14,    0} , // 4 pipes (4 PKRs) 8 bpe @ SW_4K_S_X @ RbPlus
    {   3,    4,   15,    0} , // 4 pipes (4 PKRs) 16 bpe @ SW_4K_S_X @ RbPlus
    {   3,    0,  220,    0} , // 8 pipes (4 PKRs) 1 bpe @ SW_4K_S_X @ RbPlus
    {   3,    1,  221,    0} , // 8 pipes (4 PKRs) 2 bpe @ SW_4K_S_X @ RbPlus
    {   3,    2,  222,    0} , // 8 pipes (4 PKRs) 4 bpe @ SW_4K_S_X @ RbPlus
    {   3,    3,  223,    0} , // 8 pipes (4 PKRs) 8 bpe @ SW_4K_S_X @ RbPlus
    {   3,    4,  224,    0} , // 8 pipes (4 PKRs) 16 bpe @ SW_4K_S_X @ RbPlus
    {   3,    0,  225,    0} , // 16 pipes (4 PKRs) 1 bpe @ SW_4K_S_X @ RbPlus
    {   3,    1,  226,    0} , // 16 pipes (4 PKRs) 2 bpe @ SW_4K_S_X @ RbPlus
    {   3,    2,  227,    0} , // 16 pipes (4 PKRs) 4 bpe @ SW_4K_S_X @ RbPlus
    {   3,    3,  228,    0} , // 16 pipes (4 PKRs) 8 bpe @ SW_4K_S_X @ RbPlus
    {   3,    4,  229,    0} , // 16 pipes (4 PKRs) 16 bpe @ SW_4K_S_X @ RbPlus
    {   3,    0,   16,    0} , // 8 pipes (8 PKRs) 1 bpe @ SW_4K_S_X @ RbPlus
    {   3,    1,   17,    0} , // 8 pipes (8 PKRs) 2 bpe @ SW_4K_S_X @ RbPlus
    {   3,    2,   18,    0} , // 8 pipes (8 PKRs) 4 bpe @ SW_4K_S_X @ RbPlus
    {   3,    3,   19,    0} , // 8 pipes (8 PKRs) 8 bpe @ SW_4K_S_X @ RbPlus
    {   3,    4,   20,    0} , // 8 pipes (8 PKRs) 16 bpe @ SW_4K_S_X @ RbPlus
    {   3,    0,  230,    0} , // 16 pipes (8 PKRs) 1 bpe @ SW_4K_S_X @ RbPlus
    {   3,    1,  231,    0} , // 16 pipes (8 PKRs) 2 bpe @ SW_4K_S_X @ RbPlus
    {   3,    2,  232,    0} , // 16 pipes (8 PKRs) 4 bpe @ SW_4K_S_X @ RbPlus
    {   3,    3,  233,    0} , // 16 pipes (8 PKRs) 8 bpe @ SW_4K_S_X @ RbPlus
    {   3,    4,  234,    0} , // 16 pipes (8 PKRs) 16 bpe @ SW_4K_S_X @ RbPlus
    {   3,    0,  235,    0} , // 32 pipes (8 PKRs) 1 bpe @ SW_4K_S_X @ RbPlus
    {   3,    1,  236,    0} , // 32 pipes (8 PKRs) 2 bpe @ SW_4K_S_X @ RbPlus
    {   3,    2,  237,    0} , // 32 pipes (8 PKRs) 4 bpe @ SW_4K_S_X @ RbPlus
    {   3,    3,  238,    0} , // 32 pipes (8 PKRs) 8 bpe @ SW_4K_S_X @ RbPlus
    {   3,    4,  239,    0} , // 32 pipes (8 PKRs) 16 bpe @ SW_4K_S_X @ RbPlus
    {   3,    0,   21,    0} , // 16 pipes (16 PKRs) 1 bpe @ SW_4K_S_X @ RbPlus
    {   3,    1,   22,    0} , // 16 pipes (16 PKRs) 2 bpe @ SW_4K_S_X @ RbPlus
    {   3,    2,   23,    0} , // 16 pipes (16 PKRs) 4 bpe @ SW_4K_S_X @ RbPlus
    {   3,    3,   24,    0} , // 16 pipes (16 PKRs) 8 bpe @ SW_4K_S_X @ RbPlus
    {   3,    4,   25,    0} , // 16 pipes (16 PKRs) 16 bpe @ SW_4K_S_X @ RbPlus
    {   3,    0,  240,    0} , // 32 pipes (16 PKRs) 1 bpe @ SW_4K_S_X @ RbPlus
    {   3,    1,  241,    0} , // 32 pipes (16 PKRs) 2 bpe @ SW_4K_S_X @ RbPlus
    {   3,    2,  242,    0} , // 32 pipes (16 PKRs) 4 bpe @ SW_4K_S_X @ RbPlus
    {   3,    3,  243,    0} , // 32 pipes (16 PKRs) 8 bpe @ SW_4K_S_X @ RbPlus
    {   3,    4,  244,    0} , // 32 pipes (16 PKRs) 16 bpe @ SW_4K_S_X @ RbPlus
    {   3,    0,  245,    0} , // 64 pipes (16 PKRs) 1 bpe @ SW_4K_S_X @ RbPlus
    {   3,    1,  246,    0} , // 64 pipes (16 PKRs) 2 bpe @ SW_4K_S_X @ RbPlus
    {   3,    2,  247,    0} , // 64 pipes (16 PKRs) 4 bpe @ SW_4K_S_X @ RbPlus
    {   3,    3,  248,    0} , // 64 pipes (16 PKRs) 8 bpe @ SW_4K_S_X @ RbPlus
    {   3,    4,  249,    0} , // 64 pipes (16 PKRs) 16 bpe @ SW_4K_S_X @ RbPlus
    {   3,    0,   21,    0} , // 32 pipes (32 PKRs) 1 bpe @ SW_4K_S_X @ RbPlus
    {   3,    1,   22,    0} , // 32 pipes (32 PKRs) 2 bpe @ SW_4K_S_X @ RbPlus
    {   3,    2,   23,    0} , // 32 pipes (32 PKRs) 4 bpe @ SW_4K_S_X @ RbPlus
    {   3,    3,   24,    0} , // 32 pipes (32 PKRs) 8 bpe @ SW_4K_S_X @ RbPlus
    {   3,    4,   25,    0} , // 32 pipes (32 PKRs) 16 bpe @ SW_4K_S_X @ RbPlus
    {   3,    0,  240,    0} , // 64 pipes (32 PKRs) 1 bpe @ SW_4K_S_X @ RbPlus
    {   3,    1,  241,    0} , // 64 pipes (32 PKRs) 2 bpe @ SW_4K_S_X @ RbPlus
    {   3,    2,  242,    0} , // 64 pipes (32 PKRs) 4 bpe @ SW_4K_S_X @ RbPlus
    {   3,    3,  243,    0} , // 64 pipes (32 PKRs) 8 bpe @ SW_4K_S_X @ RbPlus
    {   3,    4,  244,    0} , // 64 pipes (32 PKRs) 16 bpe @ SW_4K_S_X @ RbPlus
};

const ADDR_SW_PATINFO SW_4K_D_X_RBPLUS_PATINFO[] =
{
    {   1,    5,    1,    0} , // 1 pipes (1 PKRs) 1 bpe @ SW_4K_D_X @ RbPlus
    {   1,    1,    2,    0} , // 1 pipes (1 PKRs) 2 bpe @ SW_4K_D_X @ RbPlus
    {   1,   39,    3,    0} , // 1 pipes (1 PKRs) 4 bpe @ SW_4K_D_X @ RbPlus
    {   1,    6,    4,    0} , // 1 pipes (1 PKRs) 8 bpe @ SW_4K_D_X @ RbPlus
    {   1,    7,    5,    0} , // 1 pipes (1 PKRs) 16 bpe @ SW_4K_D_X @ RbPlus
    {   3,    5,    6,    0} , // 2 pipes (1-2 PKRs) 1 bpe @ SW_4K_D_X @ RbPlus
    {   3,    1,    7,    0} , // 2 pipes (1-2 PKRs) 2 bpe @ SW_4K_D_X @ RbPlus
    {   3,   39,    8,    0} , // 2 pipes (1-2 PKRs) 4 bpe @ SW_4K_D_X @ RbPlus
    {   3,    6,    9,    0} , // 2 pipes (1-2 PKRs) 8 bpe @ SW_4K_D_X @ RbPlus
    {   3,    7,   10,    0} , // 2 pipes (1-2 PKRs) 16 bpe @ SW_4K_D_X @ RbPlus
    {   3,    5,  210,    0} , // 4 pipes (1-2 PKRs) 1 bpe @ SW_4K_D_X @ RbPlus
    {   3,    1,  211,    0} , // 4 pipes (1-2 PKRs) 2 bpe @ SW_4K_D_X @ RbPlus
    {   3,   39,  212,    0} , // 4 pipes (1-2 PKRs) 4 bpe @ SW_4K_D_X @ RbPlus
    {   3,    6,  213,    0} , // 4 pipes (1-2 PKRs) 8 bpe @ SW_4K_D_X @ RbPlus
    {   3,    7,  214,    0} , // 4 pipes (1-2 PKRs) 16 bpe @ SW_4K_D_X @ RbPlus
    {   3,    5,  215,    0} , // 8 pipes (2 PKRs) 1 bpe @ SW_4K_D_X @ RbPlus
    {   3,    1,  216,    0} , // 8 pipes (2 PKRs) 2 bpe @ SW_4K_D_X @ RbPlus
    {   3,   39,  217,    0} , // 8 pipes (2 PKRs) 4 bpe @ SW_4K_D_X @ RbPlus
    {   3,    6,  218,    0} , // 8 pipes (2 PKRs) 8 bpe @ SW_4K_D_X @ RbPlus
    {   3,    7,  219,    0} , // 8 pipes (2 PKRs) 16 bpe @ SW_4K_D_X @ RbPlus
    {   3,    5,   11,    0} , // 4 pipes (4 PKRs) 1 bpe @ SW_4K_D_X @ RbPlus
    {   3,    1,   12,    0} , // 4 pipes (4 PKRs) 2 bpe @ SW_4K_D_X @ RbPlus
    {   3,   39,   13,    0} , // 4 pipes (4 PKRs) 4 bpe @ SW_4K_D_X @ RbPlus
    {   3,    6,   14,    0} , // 4 pipes (4 PKRs) 8 bpe @ SW_4K_D_X @ RbPlus
    {   3,    7,   15,    0} , // 4 pipes (4 PKRs) 16 bpe @ SW_4K_D_X @ RbPlus
    {   3,    5,  220,    0} , // 8 pipes (4 PKRs) 1 bpe @ SW_4K_D_X @ RbPlus
    {   3,    1,  221,    0} , // 8 pipes (4 PKRs) 2 bpe @ SW_4K_D_X @ RbPlus
    {   3,   39,  222,    0} , // 8 pipes (4 PKRs) 4 bpe @ SW_4K_D_X @ RbPlus
    {   3,    6,  223,    0} , // 8 pipes (4 PKRs) 8 bpe @ SW_4K_D_X @ RbPlus
    {   3,    7,  224,    0} , // 8 pipes (4 PKRs) 16 bpe @ SW_4K_D_X @ RbPlus
    {   3,    5,  225,    0} , // 16 pipes (4 PKRs) 1 bpe @ SW_4K_D_X @ RbPlus
    {   3,    1,  226,    0} , // 16 pipes (4 PKRs) 2 bpe @ SW_4K_D_X @ RbPlus
    {   3,   39,  227,    0} , // 16 pipes (4 PKRs) 4 bpe @ SW_4K_D_X @ RbPlus
    {   3,    6,  228,    0} , // 16 pipes (4 PKRs) 8 bpe @ SW_4K_D_X @ RbPlus
    {   3,    7,  229,    0} , // 16 pipes (4 PKRs) 16 bpe @ SW_4K_D_X @ RbPlus
    {   3,    5,   16,    0} , // 8 pipes (8 PKRs) 1 bpe @ SW_4K_D_X @ RbPlus
    {   3,    1,   17,    0} , // 8 pipes (8 PKRs) 2 bpe @ SW_4K_D_X @ RbPlus
    {   3,   39,   18,    0} , // 8 pipes (8 PKRs) 4 bpe @ SW_4K_D_X @ RbPlus
    {   3,    6,   19,    0} , // 8 pipes (8 PKRs) 8 bpe @ SW_4K_D_X @ RbPlus
    {   3,    7,   20,    0} , // 8 pipes (8 PKRs) 16 bpe @ SW_4K_D_X @ RbPlus
    {   3,    5,  230,    0} , // 16 pipes (8 PKRs) 1 bpe @ SW_4K_D_X @ RbPlus
    {   3,    1,  231,    0} , // 16 pipes (8 PKRs) 2 bpe @ SW_4K_D_X @ RbPlus
    {   3,   39,  232,    0} , // 16 pipes (8 PKRs) 4 bpe @ SW_4K_D_X @ RbPlus
    {   3,    6,  233,    0} , // 16 pipes (8 PKRs) 8 bpe @ SW_4K_D_X @ RbPlus
    {   3,    7,  234,    0} , // 16 pipes (8 PKRs) 16 bpe @ SW_4K_D_X @ RbPlus
    {   3,    5,  235,    0} , // 32 pipes (8 PKRs) 1 bpe @ SW_4K_D_X @ RbPlus
    {   3,    1,  236,    0} , // 32 pipes (8 PKRs) 2 bpe @ SW_4K_D_X @ RbPlus
    {   3,   39,  237,    0} , // 32 pipes (8 PKRs) 4 bpe @ SW_4K_D_X @ RbPlus
    {   3,    6,  238,    0} , // 32 pipes (8 PKRs) 8 bpe @ SW_4K_D_X @ RbPlus
    {   3,    7,  239,    0} , // 32 pipes (8 PKRs) 16 bpe @ SW_4K_D_X @ RbPlus
    {   3,    5,   21,    0} , // 16 pipes (16 PKRs) 1 bpe @ SW_4K_D_X @ RbPlus
    {   3,    1,   22,    0} , // 16 pipes (16 PKRs) 2 bpe @ SW_4K_D_X @ RbPlus
    {   3,   39,   23,    0} , // 16 pipes (16 PKRs) 4 bpe @ SW_4K_D_X @ RbPlus
    {   3,    6,   24,    0} , // 16 pipes (16 PKRs) 8 bpe @ SW_4K_D_X @ RbPlus
    {   3,    7,   25,    0} , // 16 pipes (16 PKRs) 16 bpe @ SW_4K_D_X @ RbPlus
    {   3,    5,  240,    0} , // 32 pipes (16 PKRs) 1 bpe @ SW_4K_D_X @ RbPlus
    {   3,    1,  241,    0} , // 32 pipes (16 PKRs) 2 bpe @ SW_4K_D_X @ RbPlus
    {   3,   39,  242,    0} , // 32 pipes (16 PKRs) 4 bpe @ SW_4K_D_X @ RbPlus
    {   3,    6,  243,    0} , // 32 pipes (16 PKRs) 8 bpe @ SW_4K_D_X @ RbPlus
    {   3,    7,  244,    0} , // 32 pipes (16 PKRs) 16 bpe @ SW_4K_D_X @ RbPlus
    {   3,    5,  245,    0} , // 64 pipes (16 PKRs) 1 bpe @ SW_4K_D_X @ RbPlus
    {   3,    1,  246,    0} , // 64 pipes (16 PKRs) 2 bpe @ SW_4K_D_X @ RbPlus
    {   3,   39,  247,    0} , // 64 pipes (16 PKRs) 4 bpe @ SW_4K_D_X @ RbPlus
    {   3,    6,  248,    0} , // 64 pipes (16 PKRs) 8 bpe @ SW_4K_D_X @ RbPlus
    {   3,    7,  249,    0} , // 64 pipes (16 PKRs) 16 bpe @ SW_4K_D_X @ RbPlus
    {   3,    5,   21,    0} , // 32 pipes (32 PKRs) 1 bpe @ SW_4K_D_X @ RbPlus
    {   3,    1,   22,    0} , // 32 pipes (32 PKRs) 2 bpe @ SW_4K_D_X @ RbPlus
    {   3,   39,   23,    0} , // 32 pipes (32 PKRs) 4 bpe @ SW_4K_D_X @ RbPlus
    {   3,    6,   24,    0} , // 32 pipes (32 PKRs) 8 bpe @ SW_4K_D_X @ RbPlus
    {   3,    7,   25,    0} , // 32 pipes (32 PKRs) 16 bpe @ SW_4K_D_X @ RbPlus
    {   3,    5,  240,    0} , // 64 pipes (32 PKRs) 1 bpe @ SW_4K_D_X @ RbPlus
    {   3,    1,  241,    0} , // 64 pipes (32 PKRs) 2 bpe @ SW_4K_D_X @ RbPlus
    {   3,   39,  242,    0} , // 64 pipes (32 PKRs) 4 bpe @ SW_4K_D_X @ RbPlus
    {   3,    6,  243,    0} , // 64 pipes (32 PKRs) 8 bpe @ SW_4K_D_X @ RbPlus
    {   3,    7,  244,    0} , // 64 pipes (32 PKRs) 16 bpe @ SW_4K_D_X @ RbPlus
};

const ADDR_SW_PATINFO SW_4K_S3_RBPLUS_PATINFO[] =
{
    {   1,   29,  131,    0} , // 1 pipes (1 PKRs) 1 bpe @ SW_4K_S3 @ RbPlus
    {   1,   30,  132,    0} , // 1 pipes (1 PKRs) 2 bpe @ SW_4K_S3 @ RbPlus
    {   1,   31,  133,    0} , // 1 pipes (1 PKRs) 4 bpe @ SW_4K_S3 @ RbPlus
    {   1,   32,  134,    0} , // 1 pipes (1 PKRs) 8 bpe @ SW_4K_S3 @ RbPlus
    {   1,   33,  135,    0} , // 1 pipes (1 PKRs) 16 bpe @ SW_4K_S3 @ RbPlus
    {   1,   29,  131,    0} , // 2 pipes (1-2 PKRs) 1 bpe @ SW_4K_S3 @ RbPlus
    {   1,   30,  132,    0} , // 2 pipes (1-2 PKRs) 2 bpe @ SW_4K_S3 @ RbPlus
    {   1,   31,  133,    0} , // 2 pipes (1-2 PKRs) 4 bpe @ SW_4K_S3 @ RbPlus
    {   1,   32,  134,    0} , // 2 pipes (1-2 PKRs) 8 bpe @ SW_4K_S3 @ RbPlus
    {   1,   33,  135,    0} , // 2 pipes (1-2 PKRs) 16 bpe @ SW_4K_S3 @ RbPlus
    {   1,   29,  131,    0} , // 4 pipes (1-2 PKRs) 1 bpe @ SW_4K_S3 @ RbPlus
    {   1,   30,  132,    0} , // 4 pipes (1-2 PKRs) 2 bpe @ SW_4K_S3 @ RbPlus
    {   1,   31,  133,    0} , // 4 pipes (1-2 PKRs) 4 bpe @ SW_4K_S3 @ RbPlus
    {   1,   32,  134,    0} , // 4 pipes (1-2 PKRs) 8 bpe @ SW_4K_S3 @ RbPlus
    {   1,   33,  135,    0} , // 4 pipes (1-2 PKRs) 16 bpe @ SW_4K_S3 @ RbPlus
    {   1,   29,  131,    0} , // 8 pipes (2 PKRs) 1 bpe @ SW_4K_S3 @ RbPlus
    {   1,   30,  132,    0} , // 8 pipes (2 PKRs) 2 bpe @ SW_4K_S3 @ RbPlus
    {   1,   31,  133,    0} , // 8 pipes (2 PKRs) 4 bpe @ SW_4K_S3 @ RbPlus
    {   1,   32,  134,    0} , // 8 pipes (2 PKRs) 8 bpe @ SW_4K_S3 @ RbPlus
    {   1,   33,  135,    0} , // 8 pipes (2 PKRs) 16 bpe @ SW_4K_S3 @ RbPlus
    {   1,   29,  131,    0} , // 4 pipes (4 PKRs) 1 bpe @ SW_4K_S3 @ RbPlus
    {   1,   30,  132,    0} , // 4 pipes (4 PKRs) 2 bpe @ SW_4K_S3 @ RbPlus
    {   1,   31,  133,    0} , // 4 pipes (4 PKRs) 4 bpe @ SW_4K_S3 @ RbPlus
    {   1,   32,  134,    0} , // 4 pipes (4 PKRs) 8 bpe @ SW_4K_S3 @ RbPlus
    {   1,   33,  135,    0} , // 4 pipes (4 PKRs) 16 bpe @ SW_4K_S3 @ RbPlus
    {   1,   29,  131,    0} , // 8 pipes (4 PKRs) 1 bpe @ SW_4K_S3 @ RbPlus
    {   1,   30,  132,    0} , // 8 pipes (4 PKRs) 2 bpe @ SW_4K_S3 @ RbPlus
    {   1,   31,  133,    0} , // 8 pipes (4 PKRs) 4 bpe @ SW_4K_S3 @ RbPlus
    {   1,   32,  134,    0} , // 8 pipes (4 PKRs) 8 bpe @ SW_4K_S3 @ RbPlus
    {   1,   33,  135,    0} , // 8 pipes (4 PKRs) 16 bpe @ SW_4K_S3 @ RbPlus
    {   1,   29,  131,    0} , // 16 pipes (4 PKRs) 1 bpe @ SW_4K_S3 @ RbPlus
    {   1,   30,  132,    0} , // 16 pipes (4 PKRs) 2 bpe @ SW_4K_S3 @ RbPlus
    {   1,   31,  133,    0} , // 16 pipes (4 PKRs) 4 bpe @ SW_4K_S3 @ RbPlus
    {   1,   32,  134,    0} , // 16 pipes (4 PKRs) 8 bpe @ SW_4K_S3 @ RbPlus
    {   1,   33,  135,    0} , // 16 pipes (4 PKRs) 16 bpe @ SW_4K_S3 @ RbPlus
    {   1,   29,  131,    0} , // 8 pipes (8 PKRs) 1 bpe @ SW_4K_S3 @ RbPlus
    {   1,   30,  132,    0} , // 8 pipes (8 PKRs) 2 bpe @ SW_4K_S3 @ RbPlus
    {   1,   31,  133,    0} , // 8 pipes (8 PKRs) 4 bpe @ SW_4K_S3 @ RbPlus
    {   1,   32,  134,    0} , // 8 pipes (8 PKRs) 8 bpe @ SW_4K_S3 @ RbPlus
    {   1,   33,  135,    0} , // 8 pipes (8 PKRs) 16 bpe @ SW_4K_S3 @ RbPlus
    {   1,   29,  131,    0} , // 16 pipes (8 PKRs) 1 bpe @ SW_4K_S3 @ RbPlus
    {   1,   30,  132,    0} , // 16 pipes (8 PKRs) 2 bpe @ SW_4K_S3 @ RbPlus
    {   1,   31,  133,    0} , // 16 pipes (8 PKRs) 4 bpe @ SW_4K_S3 @ RbPlus
    {   1,   32,  134,    0} , // 16 pipes (8 PKRs) 8 bpe @ SW_4K_S3 @ RbPlus
    {   1,   33,  135,    0} , // 16 pipes (8 PKRs) 16 bpe @ SW_4K_S3 @ RbPlus
    {   1,   29,  131,    0} , // 32 pipes (8 PKRs) 1 bpe @ SW_4K_S3 @ RbPlus
    {   1,   30,  132,    0} , // 32 pipes (8 PKRs) 2 bpe @ SW_4K_S3 @ RbPlus
    {   1,   31,  133,    0} , // 32 pipes (8 PKRs) 4 bpe @ SW_4K_S3 @ RbPlus
    {   1,   32,  134,    0} , // 32 pipes (8 PKRs) 8 bpe @ SW_4K_S3 @ RbPlus
    {   1,   33,  135,    0} , // 32 pipes (8 PKRs) 16 bpe @ SW_4K_S3 @ RbPlus
    {   1,   29,  131,    0} , // 16 pipes (16 PKRs) 1 bpe @ SW_4K_S3 @ RbPlus
    {   1,   30,  132,    0} , // 16 pipes (16 PKRs) 2 bpe @ SW_4K_S3 @ RbPlus
    {   1,   31,  133,    0} , // 16 pipes (16 PKRs) 4 bpe @ SW_4K_S3 @ RbPlus
    {   1,   32,  134,    0} , // 16 pipes (16 PKRs) 8 bpe @ SW_4K_S3 @ RbPlus
    {   1,   33,  135,    0} , // 16 pipes (16 PKRs) 16 bpe @ SW_4K_S3 @ RbPlus
    {   1,   29,  131,    0} , // 32 pipes (16 PKRs) 1 bpe @ SW_4K_S3 @ RbPlus
    {   1,   30,  132,    0} , // 32 pipes (16 PKRs) 2 bpe @ SW_4K_S3 @ RbPlus
    {   1,   31,  133,    0} , // 32 pipes (16 PKRs) 4 bpe @ SW_4K_S3 @ RbPlus
    {   1,   32,  134,    0} , // 32 pipes (16 PKRs) 8 bpe @ SW_4K_S3 @ RbPlus
    {   1,   33,  135,    0} , // 32 pipes (16 PKRs) 16 bpe @ SW_4K_S3 @ RbPlus
    {   1,   29,  131,    0} , // 64 pipes (16 PKRs) 1 bpe @ SW_4K_S3 @ RbPlus
    {   1,   30,  132,    0} , // 64 pipes (16 PKRs) 2 bpe @ SW_4K_S3 @ RbPlus
    {   1,   31,  133,    0} , // 64 pipes (16 PKRs) 4 bpe @ SW_4K_S3 @ RbPlus
    {   1,   32,  134,    0} , // 64 pipes (16 PKRs) 8 bpe @ SW_4K_S3 @ RbPlus
    {   1,   33,  135,    0} , // 64 pipes (16 PKRs) 16 bpe @ SW_4K_S3 @ RbPlus
    {   1,   29,  131,    0} , // 32 pipes (32 PKRs) 1 bpe @ SW_4K_S3 @ RbPlus
    {   1,   30,  132,    0} , // 32 pipes (32 PKRs) 2 bpe @ SW_4K_S3 @ RbPlus
    {   1,   31,  133,    0} , // 32 pipes (32 PKRs) 4 bpe @ SW_4K_S3 @ RbPlus
    {   1,   32,  134,    0} , // 32 pipes (32 PKRs) 8 bpe @ SW_4K_S3 @ RbPlus
    {   1,   33,  135,    0} , // 32 pipes (32 PKRs) 16 bpe @ SW_4K_S3 @ RbPlus
    {   1,   29,  131,    0} , // 64 pipes (32 PKRs) 1 bpe @ SW_4K_S3 @ RbPlus
    {   1,   30,  132,    0} , // 64 pipes (32 PKRs) 2 bpe @ SW_4K_S3 @ RbPlus
    {   1,   31,  133,    0} , // 64 pipes (32 PKRs) 4 bpe @ SW_4K_S3 @ RbPlus
    {   1,   32,  134,    0} , // 64 pipes (32 PKRs) 8 bpe @ SW_4K_S3 @ RbPlus
    {   1,   33,  135,    0} , // 64 pipes (32 PKRs) 16 bpe @ SW_4K_S3 @ RbPlus
};

const ADDR_SW_PATINFO SW_4K_S3_X_RBPLUS_PATINFO[] =
{
    {   1,   29,  131,    0} , // 1 pipes (1 PKRs) 1 bpe @ SW_4K_S3_X @ RbPlus
    {   1,   30,  132,    0} , // 1 pipes (1 PKRs) 2 bpe @ SW_4K_S3_X @ RbPlus
    {   1,   31,  133,    0} , // 1 pipes (1 PKRs) 4 bpe @ SW_4K_S3_X @ RbPlus
    {   1,   32,  134,    0} , // 1 pipes (1 PKRs) 8 bpe @ SW_4K_S3_X @ RbPlus
    {   1,   33,  135,    0} , // 1 pipes (1 PKRs) 16 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   29,  136,    0} , // 2 pipes (1-2 PKRs) 1 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   30,  137,    0} , // 2 pipes (1-2 PKRs) 2 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   31,  138,    0} , // 2 pipes (1-2 PKRs) 4 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   32,  139,    0} , // 2 pipes (1-2 PKRs) 8 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   33,  140,    0} , // 2 pipes (1-2 PKRs) 16 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   29,  141,    0} , // 4 pipes (1-2 PKRs) 1 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   30,  142,    0} , // 4 pipes (1-2 PKRs) 2 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   31,  143,    0} , // 4 pipes (1-2 PKRs) 4 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   32,  144,    0} , // 4 pipes (1-2 PKRs) 8 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   33,  145,    0} , // 4 pipes (1-2 PKRs) 16 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   29,  146,    0} , // 8 pipes (2 PKRs) 1 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   30,  147,    0} , // 8 pipes (2 PKRs) 2 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   31,  148,    0} , // 8 pipes (2 PKRs) 4 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   32,  149,    0} , // 8 pipes (2 PKRs) 8 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   33,  150,    0} , // 8 pipes (2 PKRs) 16 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   29,  141,    0} , // 4 pipes (4 PKRs) 1 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   30,  142,    0} , // 4 pipes (4 PKRs) 2 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   31,  143,    0} , // 4 pipes (4 PKRs) 4 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   32,  144,    0} , // 4 pipes (4 PKRs) 8 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   33,  145,    0} , // 4 pipes (4 PKRs) 16 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   29,  146,    0} , // 8 pipes (4 PKRs) 1 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   30,  147,    0} , // 8 pipes (4 PKRs) 2 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   31,  148,    0} , // 8 pipes (4 PKRs) 4 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   32,  149,    0} , // 8 pipes (4 PKRs) 8 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   33,  150,    0} , // 8 pipes (4 PKRs) 16 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   29,  151,    0} , // 16 pipes (4 PKRs) 1 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   30,  152,    0} , // 16 pipes (4 PKRs) 2 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   31,  153,    0} , // 16 pipes (4 PKRs) 4 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   32,  154,    0} , // 16 pipes (4 PKRs) 8 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   33,  155,    0} , // 16 pipes (4 PKRs) 16 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   29,  146,    0} , // 8 pipes (8 PKRs) 1 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   30,  147,    0} , // 8 pipes (8 PKRs) 2 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   31,  148,    0} , // 8 pipes (8 PKRs) 4 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   32,  149,    0} , // 8 pipes (8 PKRs) 8 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   33,  150,    0} , // 8 pipes (8 PKRs) 16 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   29,  151,    0} , // 16 pipes (8 PKRs) 1 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   30,  152,    0} , // 16 pipes (8 PKRs) 2 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   31,  153,    0} , // 16 pipes (8 PKRs) 4 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   32,  154,    0} , // 16 pipes (8 PKRs) 8 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   33,  155,    0} , // 16 pipes (8 PKRs) 16 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   29,  151,    0} , // 32 pipes (8 PKRs) 1 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   30,  152,    0} , // 32 pipes (8 PKRs) 2 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   31,  153,    0} , // 32 pipes (8 PKRs) 4 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   32,  154,    0} , // 32 pipes (8 PKRs) 8 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   33,  155,    0} , // 32 pipes (8 PKRs) 16 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   29,  151,    0} , // 16 pipes (16 PKRs) 1 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   30,  152,    0} , // 16 pipes (16 PKRs) 2 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   31,  153,    0} , // 16 pipes (16 PKRs) 4 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   32,  154,    0} , // 16 pipes (16 PKRs) 8 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   33,  155,    0} , // 16 pipes (16 PKRs) 16 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   29,  151,    0} , // 32 pipes (16 PKRs) 1 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   30,  152,    0} , // 32 pipes (16 PKRs) 2 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   31,  153,    0} , // 32 pipes (16 PKRs) 4 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   32,  154,    0} , // 32 pipes (16 PKRs) 8 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   33,  155,    0} , // 32 pipes (16 PKRs) 16 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   29,  151,    0} , // 64 pipes (16 PKRs) 1 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   30,  152,    0} , // 64 pipes (16 PKRs) 2 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   31,  153,    0} , // 64 pipes (16 PKRs) 4 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   32,  154,    0} , // 64 pipes (16 PKRs) 8 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   33,  155,    0} , // 64 pipes (16 PKRs) 16 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   29,  151,    0} , // 32 pipes (32 PKRs) 1 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   30,  152,    0} , // 32 pipes (32 PKRs) 2 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   31,  153,    0} , // 32 pipes (32 PKRs) 4 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   32,  154,    0} , // 32 pipes (32 PKRs) 8 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   33,  155,    0} , // 32 pipes (32 PKRs) 16 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   29,  151,    0} , // 64 pipes (32 PKRs) 1 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   30,  152,    0} , // 64 pipes (32 PKRs) 2 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   31,  153,    0} , // 64 pipes (32 PKRs) 4 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   32,  154,    0} , // 64 pipes (32 PKRs) 8 bpe @ SW_4K_S3_X @ RbPlus
    {   3,   33,  155,    0} , // 64 pipes (32 PKRs) 16 bpe @ SW_4K_S3_X @ RbPlus
};

const ADDR_SW_PATINFO SW_64K_S_RBPLUS_PATINFO[] =
{
    {   1,    0,    1,    1} , // 1 pipes (1 PKRs) 1 bpe @ SW_64K_S @ RbPlus
    {   1,    1,    2,    2} , // 1 pipes (1 PKRs) 2 bpe @ SW_64K_S @ RbPlus
    {   1,    2,    3,    3} , // 1 pipes (1 PKRs) 4 bpe @ SW_64K_S @ RbPlus
    {   1,    3,    4,    4} , // 1 pipes (1 PKRs) 8 bpe @ SW_64K_S @ RbPlus
    {   1,    4,    5,    5} , // 1 pipes (1 PKRs) 16 bpe @ SW_64K_S @ RbPlus
    {   1,    0,    1,    1} , // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_S @ RbPlus
    {   1,    1,    2,    2} , // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_S @ RbPlus
    {   1,    2,    3,    3} , // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_S @ RbPlus
    {   1,    3,    4,    4} , // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_S @ RbPlus
    {   1,    4,    5,    5} , // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_S @ RbPlus
    {   1,    0,    1,    1} , // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_S @ RbPlus
    {   1,    1,    2,    2} , // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_S @ RbPlus
    {   1,    2,    3,    3} , // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_S @ RbPlus
    {   1,    3,    4,    4} , // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_S @ RbPlus
    {   1,    4,    5,    5} , // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_S @ RbPlus
    {   1,    0,    1,    1} , // 8 pipes (2 PKRs) 1 bpe @ SW_64K_S @ RbPlus
    {   1,    1,    2,    2} , // 8 pipes (2 PKRs) 2 bpe @ SW_64K_S @ RbPlus
    {   1,    2,    3,    3} , // 8 pipes (2 PKRs) 4 bpe @ SW_64K_S @ RbPlus
    {   1,    3,    4,    4} , // 8 pipes (2 PKRs) 8 bpe @ SW_64K_S @ RbPlus
    {   1,    4,    5,    5} , // 8 pipes (2 PKRs) 16 bpe @ SW_64K_S @ RbPlus
    {   1,    0,    1,    1} , // 4 pipes (4 PKRs) 1 bpe @ SW_64K_S @ RbPlus
    {   1,    1,    2,    2} , // 4 pipes (4 PKRs) 2 bpe @ SW_64K_S @ RbPlus
    {   1,    2,    3,    3} , // 4 pipes (4 PKRs) 4 bpe @ SW_64K_S @ RbPlus
    {   1,    3,    4,    4} , // 4 pipes (4 PKRs) 8 bpe @ SW_64K_S @ RbPlus
    {   1,    4,    5,    5} , // 4 pipes (4 PKRs) 16 bpe @ SW_64K_S @ RbPlus
    {   1,    0,    1,    1} , // 8 pipes (4 PKRs) 1 bpe @ SW_64K_S @ RbPlus
    {   1,    1,    2,    2} , // 8 pipes (4 PKRs) 2 bpe @ SW_64K_S @ RbPlus
    {   1,    2,    3,    3} , // 8 pipes (4 PKRs) 4 bpe @ SW_64K_S @ RbPlus
    {   1,    3,    4,    4} , // 8 pipes (4 PKRs) 8 bpe @ SW_64K_S @ RbPlus
    {   1,    4,    5,    5} , // 8 pipes (4 PKRs) 16 bpe @ SW_64K_S @ RbPlus
    {   1,    0,    1,    1} , // 16 pipes (4 PKRs) 1 bpe @ SW_64K_S @ RbPlus
    {   1,    1,    2,    2} , // 16 pipes (4 PKRs) 2 bpe @ SW_64K_S @ RbPlus
    {   1,    2,    3,    3} , // 16 pipes (4 PKRs) 4 bpe @ SW_64K_S @ RbPlus
    {   1,    3,    4,    4} , // 16 pipes (4 PKRs) 8 bpe @ SW_64K_S @ RbPlus
    {   1,    4,    5,    5} , // 16 pipes (4 PKRs) 16 bpe @ SW_64K_S @ RbPlus
    {   1,    0,    1,    1} , // 8 pipes (8 PKRs) 1 bpe @ SW_64K_S @ RbPlus
    {   1,    1,    2,    2} , // 8 pipes (8 PKRs) 2 bpe @ SW_64K_S @ RbPlus
    {   1,    2,    3,    3} , // 8 pipes (8 PKRs) 4 bpe @ SW_64K_S @ RbPlus
    {   1,    3,    4,    4} , // 8 pipes (8 PKRs) 8 bpe @ SW_64K_S @ RbPlus
    {   1,    4,    5,    5} , // 8 pipes (8 PKRs) 16 bpe @ SW_64K_S @ RbPlus
    {   1,    0,    1,    1} , // 16 pipes (8 PKRs) 1 bpe @ SW_64K_S @ RbPlus
    {   1,    1,    2,    2} , // 16 pipes (8 PKRs) 2 bpe @ SW_64K_S @ RbPlus
    {   1,    2,    3,    3} , // 16 pipes (8 PKRs) 4 bpe @ SW_64K_S @ RbPlus
    {   1,    3,    4,    4} , // 16 pipes (8 PKRs) 8 bpe @ SW_64K_S @ RbPlus
    {   1,    4,    5,    5} , // 16 pipes (8 PKRs) 16 bpe @ SW_64K_S @ RbPlus
    {   1,    0,    1,    1} , // 32 pipes (8 PKRs) 1 bpe @ SW_64K_S @ RbPlus
    {   1,    1,    2,    2} , // 32 pipes (8 PKRs) 2 bpe @ SW_64K_S @ RbPlus
    {   1,    2,    3,    3} , // 32 pipes (8 PKRs) 4 bpe @ SW_64K_S @ RbPlus
    {   1,    3,    4,    4} , // 32 pipes (8 PKRs) 8 bpe @ SW_64K_S @ RbPlus
    {   1,    4,    5,    5} , // 32 pipes (8 PKRs) 16 bpe @ SW_64K_S @ RbPlus
    {   1,    0,    1,    1} , // 16 pipes (16 PKRs) 1 bpe @ SW_64K_S @ RbPlus
    {   1,    1,    2,    2} , // 16 pipes (16 PKRs) 2 bpe @ SW_64K_S @ RbPlus
    {   1,    2,    3,    3} , // 16 pipes (16 PKRs) 4 bpe @ SW_64K_S @ RbPlus
    {   1,    3,    4,    4} , // 16 pipes (16 PKRs) 8 bpe @ SW_64K_S @ RbPlus
    {   1,    4,    5,    5} , // 16 pipes (16 PKRs) 16 bpe @ SW_64K_S @ RbPlus
    {   1,    0,    1,    1} , // 32 pipes (16 PKRs) 1 bpe @ SW_64K_S @ RbPlus
    {   1,    1,    2,    2} , // 32 pipes (16 PKRs) 2 bpe @ SW_64K_S @ RbPlus
    {   1,    2,    3,    3} , // 32 pipes (16 PKRs) 4 bpe @ SW_64K_S @ RbPlus
    {   1,    3,    4,    4} , // 32 pipes (16 PKRs) 8 bpe @ SW_64K_S @ RbPlus
    {   1,    4,    5,    5} , // 32 pipes (16 PKRs) 16 bpe @ SW_64K_S @ RbPlus
    {   1,    0,    1,    1} , // 64 pipes (16 PKRs) 1 bpe @ SW_64K_S @ RbPlus
    {   1,    1,    2,    2} , // 64 pipes (16 PKRs) 2 bpe @ SW_64K_S @ RbPlus
    {   1,    2,    3,    3} , // 64 pipes (16 PKRs) 4 bpe @ SW_64K_S @ RbPlus
    {   1,    3,    4,    4} , // 64 pipes (16 PKRs) 8 bpe @ SW_64K_S @ RbPlus
    {   1,    4,    5,    5} , // 64 pipes (16 PKRs) 16 bpe @ SW_64K_S @ RbPlus
    {   1,    0,    1,    1} , // 32 pipes (32 PKRs) 1 bpe @ SW_64K_S @ RbPlus
    {   1,    1,    2,    2} , // 32 pipes (32 PKRs) 2 bpe @ SW_64K_S @ RbPlus
    {   1,    2,    3,    3} , // 32 pipes (32 PKRs) 4 bpe @ SW_64K_S @ RbPlus
    {   1,    3,    4,    4} , // 32 pipes (32 PKRs) 8 bpe @ SW_64K_S @ RbPlus
    {   1,    4,    5,    5} , // 32 pipes (32 PKRs) 16 bpe @ SW_64K_S @ RbPlus
    {   1,    0,    1,    1} , // 64 pipes (32 PKRs) 1 bpe @ SW_64K_S @ RbPlus
    {   1,    1,    2,    2} , // 64 pipes (32 PKRs) 2 bpe @ SW_64K_S @ RbPlus
    {   1,    2,    3,    3} , // 64 pipes (32 PKRs) 4 bpe @ SW_64K_S @ RbPlus
    {   1,    3,    4,    4} , // 64 pipes (32 PKRs) 8 bpe @ SW_64K_S @ RbPlus
    {   1,    4,    5,    5} , // 64 pipes (32 PKRs) 16 bpe @ SW_64K_S @ RbPlus
};

const ADDR_SW_PATINFO SW_64K_D_RBPLUS_PATINFO[] =
{
    {   1,    5,    1,    1} , // 1 pipes (1 PKRs) 1 bpe @ SW_64K_D @ RbPlus
    {   1,    1,    2,    2} , // 1 pipes (1 PKRs) 2 bpe @ SW_64K_D @ RbPlus
    {   1,   39,    3,    3} , // 1 pipes (1 PKRs) 4 bpe @ SW_64K_D @ RbPlus
    {   1,    6,    4,    4} , // 1 pipes (1 PKRs) 8 bpe @ SW_64K_D @ RbPlus
    {   1,    7,    5,    5} , // 1 pipes (1 PKRs) 16 bpe @ SW_64K_D @ RbPlus
    {   1,    5,    1,    1} , // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_D @ RbPlus
    {   1,    1,    2,    2} , // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_D @ RbPlus
    {   1,   39,    3,    3} , // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_D @ RbPlus
    {   1,    6,    4,    4} , // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_D @ RbPlus
    {   1,    7,    5,    5} , // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_D @ RbPlus
    {   1,    5,    1,    1} , // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_D @ RbPlus
    {   1,    1,    2,    2} , // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_D @ RbPlus
    {   1,   39,    3,    3} , // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_D @ RbPlus
    {   1,    6,    4,    4} , // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_D @ RbPlus
    {   1,    7,    5,    5} , // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_D @ RbPlus
    {   1,    5,    1,    1} , // 8 pipes (2 PKRs) 1 bpe @ SW_64K_D @ RbPlus
    {   1,    1,    2,    2} , // 8 pipes (2 PKRs) 2 bpe @ SW_64K_D @ RbPlus
    {   1,   39,    3,    3} , // 8 pipes (2 PKRs) 4 bpe @ SW_64K_D @ RbPlus
    {   1,    6,    4,    4} , // 8 pipes (2 PKRs) 8 bpe @ SW_64K_D @ RbPlus
    {   1,    7,    5,    5} , // 8 pipes (2 PKRs) 16 bpe @ SW_64K_D @ RbPlus
    {   1,    5,    1,    1} , // 4 pipes (4 PKRs) 1 bpe @ SW_64K_D @ RbPlus
    {   1,    1,    2,    2} , // 4 pipes (4 PKRs) 2 bpe @ SW_64K_D @ RbPlus
    {   1,   39,    3,    3} , // 4 pipes (4 PKRs) 4 bpe @ SW_64K_D @ RbPlus
    {   1,    6,    4,    4} , // 4 pipes (4 PKRs) 8 bpe @ SW_64K_D @ RbPlus
    {   1,    7,    5,    5} , // 4 pipes (4 PKRs) 16 bpe @ SW_64K_D @ RbPlus
    {   1,    5,    1,    1} , // 8 pipes (4 PKRs) 1 bpe @ SW_64K_D @ RbPlus
    {   1,    1,    2,    2} , // 8 pipes (4 PKRs) 2 bpe @ SW_64K_D @ RbPlus
    {   1,   39,    3,    3} , // 8 pipes (4 PKRs) 4 bpe @ SW_64K_D @ RbPlus
    {   1,    6,    4,    4} , // 8 pipes (4 PKRs) 8 bpe @ SW_64K_D @ RbPlus
    {   1,    7,    5,    5} , // 8 pipes (4 PKRs) 16 bpe @ SW_64K_D @ RbPlus
    {   1,    5,    1,    1} , // 16 pipes (4 PKRs) 1 bpe @ SW_64K_D @ RbPlus
    {   1,    1,    2,    2} , // 16 pipes (4 PKRs) 2 bpe @ SW_64K_D @ RbPlus
    {   1,   39,    3,    3} , // 16 pipes (4 PKRs) 4 bpe @ SW_64K_D @ RbPlus
    {   1,    6,    4,    4} , // 16 pipes (4 PKRs) 8 bpe @ SW_64K_D @ RbPlus
    {   1,    7,    5,    5} , // 16 pipes (4 PKRs) 16 bpe @ SW_64K_D @ RbPlus
    {   1,    5,    1,    1} , // 8 pipes (8 PKRs) 1 bpe @ SW_64K_D @ RbPlus
    {   1,    1,    2,    2} , // 8 pipes (8 PKRs) 2 bpe @ SW_64K_D @ RbPlus
    {   1,   39,    3,    3} , // 8 pipes (8 PKRs) 4 bpe @ SW_64K_D @ RbPlus
    {   1,    6,    4,    4} , // 8 pipes (8 PKRs) 8 bpe @ SW_64K_D @ RbPlus
    {   1,    7,    5,    5} , // 8 pipes (8 PKRs) 16 bpe @ SW_64K_D @ RbPlus
    {   1,    5,    1,    1} , // 16 pipes (8 PKRs) 1 bpe @ SW_64K_D @ RbPlus
    {   1,    1,    2,    2} , // 16 pipes (8 PKRs) 2 bpe @ SW_64K_D @ RbPlus
    {   1,   39,    3,    3} , // 16 pipes (8 PKRs) 4 bpe @ SW_64K_D @ RbPlus
    {   1,    6,    4,    4} , // 16 pipes (8 PKRs) 8 bpe @ SW_64K_D @ RbPlus
    {   1,    7,    5,    5} , // 16 pipes (8 PKRs) 16 bpe @ SW_64K_D @ RbPlus
    {   1,    5,    1,    1} , // 32 pipes (8 PKRs) 1 bpe @ SW_64K_D @ RbPlus
    {   1,    1,    2,    2} , // 32 pipes (8 PKRs) 2 bpe @ SW_64K_D @ RbPlus
    {   1,   39,    3,    3} , // 32 pipes (8 PKRs) 4 bpe @ SW_64K_D @ RbPlus
    {   1,    6,    4,    4} , // 32 pipes (8 PKRs) 8 bpe @ SW_64K_D @ RbPlus
    {   1,    7,    5,    5} , // 32 pipes (8 PKRs) 16 bpe @ SW_64K_D @ RbPlus
    {   1,    5,    1,    1} , // 16 pipes (16 PKRs) 1 bpe @ SW_64K_D @ RbPlus
    {   1,    1,    2,    2} , // 16 pipes (16 PKRs) 2 bpe @ SW_64K_D @ RbPlus
    {   1,   39,    3,    3} , // 16 pipes (16 PKRs) 4 bpe @ SW_64K_D @ RbPlus
    {   1,    6,    4,    4} , // 16 pipes (16 PKRs) 8 bpe @ SW_64K_D @ RbPlus
    {   1,    7,    5,    5} , // 16 pipes (16 PKRs) 16 bpe @ SW_64K_D @ RbPlus
    {   1,    5,    1,    1} , // 32 pipes (16 PKRs) 1 bpe @ SW_64K_D @ RbPlus
    {   1,    1,    2,    2} , // 32 pipes (16 PKRs) 2 bpe @ SW_64K_D @ RbPlus
    {   1,   39,    3,    3} , // 32 pipes (16 PKRs) 4 bpe @ SW_64K_D @ RbPlus
    {   1,    6,    4,    4} , // 32 pipes (16 PKRs) 8 bpe @ SW_64K_D @ RbPlus
    {   1,    7,    5,    5} , // 32 pipes (16 PKRs) 16 bpe @ SW_64K_D @ RbPlus
    {   1,    5,    1,    1} , // 64 pipes (16 PKRs) 1 bpe @ SW_64K_D @ RbPlus
    {   1,    1,    2,    2} , // 64 pipes (16 PKRs) 2 bpe @ SW_64K_D @ RbPlus
    {   1,   39,    3,    3} , // 64 pipes (16 PKRs) 4 bpe @ SW_64K_D @ RbPlus
    {   1,    6,    4,    4} , // 64 pipes (16 PKRs) 8 bpe @ SW_64K_D @ RbPlus
    {   1,    7,    5,    5} , // 64 pipes (16 PKRs) 16 bpe @ SW_64K_D @ RbPlus
    {   1,    5,    1,    1} , // 32 pipes (32 PKRs) 1 bpe @ SW_64K_D @ RbPlus
    {   1,    1,    2,    2} , // 32 pipes (32 PKRs) 2 bpe @ SW_64K_D @ RbPlus
    {   1,   39,    3,    3} , // 32 pipes (32 PKRs) 4 bpe @ SW_64K_D @ RbPlus
    {   1,    6,    4,    4} , // 32 pipes (32 PKRs) 8 bpe @ SW_64K_D @ RbPlus
    {   1,    7,    5,    5} , // 32 pipes (32 PKRs) 16 bpe @ SW_64K_D @ RbPlus
    {   1,    5,    1,    1} , // 64 pipes (32 PKRs) 1 bpe @ SW_64K_D @ RbPlus
    {   1,    1,    2,    2} , // 64 pipes (32 PKRs) 2 bpe @ SW_64K_D @ RbPlus
    {   1,   39,    3,    3} , // 64 pipes (32 PKRs) 4 bpe @ SW_64K_D @ RbPlus
    {   1,    6,    4,    4} , // 64 pipes (32 PKRs) 8 bpe @ SW_64K_D @ RbPlus
    {   1,    7,    5,    5} , // 64 pipes (32 PKRs) 16 bpe @ SW_64K_D @ RbPlus
};

const ADDR_SW_PATINFO SW_64K_S_T_RBPLUS_PATINFO[] =
{
    {   1,    0,    1,    1} , // 1 pipes (1 PKRs) 1 bpe @ SW_64K_S_T @ RbPlus
    {   1,    1,    2,    2} , // 1 pipes (1 PKRs) 2 bpe @ SW_64K_S_T @ RbPlus
    {   1,    2,    3,    3} , // 1 pipes (1 PKRs) 4 bpe @ SW_64K_S_T @ RbPlus
    {   1,    3,    4,    4} , // 1 pipes (1 PKRs) 8 bpe @ SW_64K_S_T @ RbPlus
    {   1,    4,    5,    5} , // 1 pipes (1 PKRs) 16 bpe @ SW_64K_S_T @ RbPlus
    {   2,    0,   36,    1} , // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_S_T @ RbPlus
    {   2,    1,   37,    2} , // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_S_T @ RbPlus
    {   2,    2,   38,    3} , // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_S_T @ RbPlus
    {   2,    3,   39,    4} , // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_S_T @ RbPlus
    {   2,    4,   40,    5} , // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_S_T @ RbPlus
    {   2,    0,   41,    1} , // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_S_T @ RbPlus
    {   2,    1,   42,    2} , // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_S_T @ RbPlus
    {   2,    2,   43,    3} , // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_S_T @ RbPlus
    {   2,    3,   44,    4} , // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_S_T @ RbPlus
    {   2,    4,   45,    5} , // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_S_T @ RbPlus
    {   2,    0,   46,    1} , // 8 pipes (2 PKRs) 1 bpe @ SW_64K_S_T @ RbPlus
    {   2,    1,   47,    2} , // 8 pipes (2 PKRs) 2 bpe @ SW_64K_S_T @ RbPlus
    {   2,    2,   48,    3} , // 8 pipes (2 PKRs) 4 bpe @ SW_64K_S_T @ RbPlus
    {   2,    3,   49,    4} , // 8 pipes (2 PKRs) 8 bpe @ SW_64K_S_T @ RbPlus
    {   2,    4,   50,    5} , // 8 pipes (2 PKRs) 16 bpe @ SW_64K_S_T @ RbPlus
    {   2,    0,   41,    1} , // 4 pipes (4 PKRs) 1 bpe @ SW_64K_S_T @ RbPlus
    {   2,    1,   42,    2} , // 4 pipes (4 PKRs) 2 bpe @ SW_64K_S_T @ RbPlus
    {   2,    2,   43,    3} , // 4 pipes (4 PKRs) 4 bpe @ SW_64K_S_T @ RbPlus
    {   2,    3,   44,    4} , // 4 pipes (4 PKRs) 8 bpe @ SW_64K_S_T @ RbPlus
    {   2,    4,   45,    5} , // 4 pipes (4 PKRs) 16 bpe @ SW_64K_S_T @ RbPlus
    {   2,    0,   46,    1} , // 8 pipes (4 PKRs) 1 bpe @ SW_64K_S_T @ RbPlus
    {   2,    1,   47,    2} , // 8 pipes (4 PKRs) 2 bpe @ SW_64K_S_T @ RbPlus
    {   2,    2,   48,    3} , // 8 pipes (4 PKRs) 4 bpe @ SW_64K_S_T @ RbPlus
    {   2,    3,   49,    4} , // 8 pipes (4 PKRs) 8 bpe @ SW_64K_S_T @ RbPlus
    {   2,    4,   50,    5} , // 8 pipes (4 PKRs) 16 bpe @ SW_64K_S_T @ RbPlus
    {   2,    0,   51,    1} , // 16 pipes (4 PKRs) 1 bpe @ SW_64K_S_T @ RbPlus
    {   2,    1,   52,    2} , // 16 pipes (4 PKRs) 2 bpe @ SW_64K_S_T @ RbPlus
    {   2,    2,   53,    3} , // 16 pipes (4 PKRs) 4 bpe @ SW_64K_S_T @ RbPlus
    {   2,    3,   54,    4} , // 16 pipes (4 PKRs) 8 bpe @ SW_64K_S_T @ RbPlus
    {   2,    4,   55,    5} , // 16 pipes (4 PKRs) 16 bpe @ SW_64K_S_T @ RbPlus
    {   2,    0,   46,    1} , // 8 pipes (8 PKRs) 1 bpe @ SW_64K_S_T @ RbPlus
    {   2,    1,   47,    2} , // 8 pipes (8 PKRs) 2 bpe @ SW_64K_S_T @ RbPlus
    {   2,    2,   48,    3} , // 8 pipes (8 PKRs) 4 bpe @ SW_64K_S_T @ RbPlus
    {   2,    3,   49,    4} , // 8 pipes (8 PKRs) 8 bpe @ SW_64K_S_T @ RbPlus
    {   2,    4,   50,    5} , // 8 pipes (8 PKRs) 16 bpe @ SW_64K_S_T @ RbPlus
    {   2,    0,   51,    1} , // 16 pipes (8 PKRs) 1 bpe @ SW_64K_S_T @ RbPlus
    {   2,    1,   52,    2} , // 16 pipes (8 PKRs) 2 bpe @ SW_64K_S_T @ RbPlus
    {   2,    2,   53,    3} , // 16 pipes (8 PKRs) 4 bpe @ SW_64K_S_T @ RbPlus
    {   2,    3,   54,    4} , // 16 pipes (8 PKRs) 8 bpe @ SW_64K_S_T @ RbPlus
    {   2,    4,   55,    5} , // 16 pipes (8 PKRs) 16 bpe @ SW_64K_S_T @ RbPlus
    {   2,    0,   56,   16} , // 32 pipes (8 PKRs) 1 bpe @ SW_64K_S_T @ RbPlus
    {   2,    1,   57,   17} , // 32 pipes (8 PKRs) 2 bpe @ SW_64K_S_T @ RbPlus
    {   2,    2,   58,   18} , // 32 pipes (8 PKRs) 4 bpe @ SW_64K_S_T @ RbPlus
    {   2,    3,   59,   19} , // 32 pipes (8 PKRs) 8 bpe @ SW_64K_S_T @ RbPlus
    {   2,    4,   60,   20} , // 32 pipes (8 PKRs) 16 bpe @ SW_64K_S_T @ RbPlus
    {   2,    0,   51,    1} , // 16 pipes (16 PKRs) 1 bpe @ SW_64K_S_T @ RbPlus
    {   2,    1,   52,    2} , // 16 pipes (16 PKRs) 2 bpe @ SW_64K_S_T @ RbPlus
    {   2,    2,   53,    3} , // 16 pipes (16 PKRs) 4 bpe @ SW_64K_S_T @ RbPlus
    {   2,    3,   54,    4} , // 16 pipes (16 PKRs) 8 bpe @ SW_64K_S_T @ RbPlus
    {   2,    4,   55,    5} , // 16 pipes (16 PKRs) 16 bpe @ SW_64K_S_T @ RbPlus
    {   2,    0,   56,   16} , // 32 pipes (16 PKRs) 1 bpe @ SW_64K_S_T @ RbPlus
    {   2,    1,   57,   17} , // 32 pipes (16 PKRs) 2 bpe @ SW_64K_S_T @ RbPlus
    {   2,    2,   58,   18} , // 32 pipes (16 PKRs) 4 bpe @ SW_64K_S_T @ RbPlus
    {   2,    3,   59,   19} , // 32 pipes (16 PKRs) 8 bpe @ SW_64K_S_T @ RbPlus
    {   2,    4,   60,   20} , // 32 pipes (16 PKRs) 16 bpe @ SW_64K_S_T @ RbPlus
    {   2,    0,    1,   21} , // 64 pipes (16 PKRs) 1 bpe @ SW_64K_S_T @ RbPlus
    {   2,    1,    2,   22} , // 64 pipes (16 PKRs) 2 bpe @ SW_64K_S_T @ RbPlus
    {   2,    2,    3,   23} , // 64 pipes (16 PKRs) 4 bpe @ SW_64K_S_T @ RbPlus
    {   2,    3,    4,   24} , // 64 pipes (16 PKRs) 8 bpe @ SW_64K_S_T @ RbPlus
    {   2,    4,    5,   25} , // 64 pipes (16 PKRs) 16 bpe @ SW_64K_S_T @ RbPlus
    {   2,    0,   56,   16} , // 32 pipes (32 PKRs) 1 bpe @ SW_64K_S_T @ RbPlus
    {   2,    1,   57,   17} , // 32 pipes (32 PKRs) 2 bpe @ SW_64K_S_T @ RbPlus
    {   2,    2,   58,   18} , // 32 pipes (32 PKRs) 4 bpe @ SW_64K_S_T @ RbPlus
    {   2,    3,   59,   19} , // 32 pipes (32 PKRs) 8 bpe @ SW_64K_S_T @ RbPlus
    {   2,    4,   60,   20} , // 32 pipes (32 PKRs) 16 bpe @ SW_64K_S_T @ RbPlus
    {   2,    0,    1,   21} , // 64 pipes (32 PKRs) 1 bpe @ SW_64K_S_T @ RbPlus
    {   2,    1,    2,   22} , // 64 pipes (32 PKRs) 2 bpe @ SW_64K_S_T @ RbPlus
    {   2,    2,    3,   23} , // 64 pipes (32 PKRs) 4 bpe @ SW_64K_S_T @ RbPlus
    {   2,    3,    4,   24} , // 64 pipes (32 PKRs) 8 bpe @ SW_64K_S_T @ RbPlus
    {   2,    4,    5,   25} , // 64 pipes (32 PKRs) 16 bpe @ SW_64K_S_T @ RbPlus
};

const ADDR_SW_PATINFO SW_64K_D_T_RBPLUS_PATINFO[] =
{
    {   1,    5,    1,    1} , // 1 pipes (1 PKRs) 1 bpe @ SW_64K_D_T @ RbPlus
    {   1,    1,    2,    2} , // 1 pipes (1 PKRs) 2 bpe @ SW_64K_D_T @ RbPlus
    {   1,   39,    3,    3} , // 1 pipes (1 PKRs) 4 bpe @ SW_64K_D_T @ RbPlus
    {   1,    6,    4,    4} , // 1 pipes (1 PKRs) 8 bpe @ SW_64K_D_T @ RbPlus
    {   1,    7,    5,    5} , // 1 pipes (1 PKRs) 16 bpe @ SW_64K_D_T @ RbPlus
    {   2,    5,   36,    1} , // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_D_T @ RbPlus
    {   2,    1,   37,    2} , // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_D_T @ RbPlus
    {   2,   39,   38,    3} , // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_D_T @ RbPlus
    {   2,    6,   39,    4} , // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_D_T @ RbPlus
    {   2,    7,   40,    5} , // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_D_T @ RbPlus
    {   2,    5,   41,    1} , // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_D_T @ RbPlus
    {   2,    1,   42,    2} , // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_D_T @ RbPlus
    {   2,   39,   43,    3} , // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_D_T @ RbPlus
    {   2,    6,   44,    4} , // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_D_T @ RbPlus
    {   2,    7,   45,    5} , // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_D_T @ RbPlus
    {   2,    5,   46,    1} , // 8 pipes (2 PKRs) 1 bpe @ SW_64K_D_T @ RbPlus
    {   2,    1,   47,    2} , // 8 pipes (2 PKRs) 2 bpe @ SW_64K_D_T @ RbPlus
    {   2,   39,   48,    3} , // 8 pipes (2 PKRs) 4 bpe @ SW_64K_D_T @ RbPlus
    {   2,    6,   49,    4} , // 8 pipes (2 PKRs) 8 bpe @ SW_64K_D_T @ RbPlus
    {   2,    7,   50,    5} , // 8 pipes (2 PKRs) 16 bpe @ SW_64K_D_T @ RbPlus
    {   2,    5,   41,    1} , // 4 pipes (4 PKRs) 1 bpe @ SW_64K_D_T @ RbPlus
    {   2,    1,   42,    2} , // 4 pipes (4 PKRs) 2 bpe @ SW_64K_D_T @ RbPlus
    {   2,   39,   43,    3} , // 4 pipes (4 PKRs) 4 bpe @ SW_64K_D_T @ RbPlus
    {   2,    6,   44,    4} , // 4 pipes (4 PKRs) 8 bpe @ SW_64K_D_T @ RbPlus
    {   2,    7,   45,    5} , // 4 pipes (4 PKRs) 16 bpe @ SW_64K_D_T @ RbPlus
    {   2,    5,   46,    1} , // 8 pipes (4 PKRs) 1 bpe @ SW_64K_D_T @ RbPlus
    {   2,    1,   47,    2} , // 8 pipes (4 PKRs) 2 bpe @ SW_64K_D_T @ RbPlus
    {   2,   39,   48,    3} , // 8 pipes (4 PKRs) 4 bpe @ SW_64K_D_T @ RbPlus
    {   2,    6,   49,    4} , // 8 pipes (4 PKRs) 8 bpe @ SW_64K_D_T @ RbPlus
    {   2,    7,   50,    5} , // 8 pipes (4 PKRs) 16 bpe @ SW_64K_D_T @ RbPlus
    {   2,    5,   51,    1} , // 16 pipes (4 PKRs) 1 bpe @ SW_64K_D_T @ RbPlus
    {   2,    1,   52,    2} , // 16 pipes (4 PKRs) 2 bpe @ SW_64K_D_T @ RbPlus
    {   2,   39,   53,    3} , // 16 pipes (4 PKRs) 4 bpe @ SW_64K_D_T @ RbPlus
    {   2,    6,   54,    4} , // 16 pipes (4 PKRs) 8 bpe @ SW_64K_D_T @ RbPlus
    {   2,    7,   55,    5} , // 16 pipes (4 PKRs) 16 bpe @ SW_64K_D_T @ RbPlus
    {   2,    5,   46,    1} , // 8 pipes (8 PKRs) 1 bpe @ SW_64K_D_T @ RbPlus
    {   2,    1,   47,    2} , // 8 pipes (8 PKRs) 2 bpe @ SW_64K_D_T @ RbPlus
    {   2,   39,   48,    3} , // 8 pipes (8 PKRs) 4 bpe @ SW_64K_D_T @ RbPlus
    {   2,    6,   49,    4} , // 8 pipes (8 PKRs) 8 bpe @ SW_64K_D_T @ RbPlus
    {   2,    7,   50,    5} , // 8 pipes (8 PKRs) 16 bpe @ SW_64K_D_T @ RbPlus
    {   2,    5,   51,    1} , // 16 pipes (8 PKRs) 1 bpe @ SW_64K_D_T @ RbPlus
    {   2,    1,   52,    2} , // 16 pipes (8 PKRs) 2 bpe @ SW_64K_D_T @ RbPlus
    {   2,   39,   53,    3} , // 16 pipes (8 PKRs) 4 bpe @ SW_64K_D_T @ RbPlus
    {   2,    6,   54,    4} , // 16 pipes (8 PKRs) 8 bpe @ SW_64K_D_T @ RbPlus
    {   2,    7,   55,    5} , // 16 pipes (8 PKRs) 16 bpe @ SW_64K_D_T @ RbPlus
    {   2,    5,   56,   16} , // 32 pipes (8 PKRs) 1 bpe @ SW_64K_D_T @ RbPlus
    {   2,    1,   57,   17} , // 32 pipes (8 PKRs) 2 bpe @ SW_64K_D_T @ RbPlus
    {   2,   39,   58,   18} , // 32 pipes (8 PKRs) 4 bpe @ SW_64K_D_T @ RbPlus
    {   2,    6,   59,   19} , // 32 pipes (8 PKRs) 8 bpe @ SW_64K_D_T @ RbPlus
    {   2,    7,   60,   20} , // 32 pipes (8 PKRs) 16 bpe @ SW_64K_D_T @ RbPlus
    {   2,    5,   51,    1} , // 16 pipes (16 PKRs) 1 bpe @ SW_64K_D_T @ RbPlus
    {   2,    1,   52,    2} , // 16 pipes (16 PKRs) 2 bpe @ SW_64K_D_T @ RbPlus
    {   2,   39,   53,    3} , // 16 pipes (16 PKRs) 4 bpe @ SW_64K_D_T @ RbPlus
    {   2,    6,   54,    4} , // 16 pipes (16 PKRs) 8 bpe @ SW_64K_D_T @ RbPlus
    {   2,    7,   55,    5} , // 16 pipes (16 PKRs) 16 bpe @ SW_64K_D_T @ RbPlus
    {   2,    5,   56,   16} , // 32 pipes (16 PKRs) 1 bpe @ SW_64K_D_T @ RbPlus
    {   2,    1,   57,   17} , // 32 pipes (16 PKRs) 2 bpe @ SW_64K_D_T @ RbPlus
    {   2,   39,   58,   18} , // 32 pipes (16 PKRs) 4 bpe @ SW_64K_D_T @ RbPlus
    {   2,    6,   59,   19} , // 32 pipes (16 PKRs) 8 bpe @ SW_64K_D_T @ RbPlus
    {   2,    7,   60,   20} , // 32 pipes (16 PKRs) 16 bpe @ SW_64K_D_T @ RbPlus
    {   2,    5,    1,   21} , // 64 pipes (16 PKRs) 1 bpe @ SW_64K_D_T @ RbPlus
    {   2,    1,    2,   22} , // 64 pipes (16 PKRs) 2 bpe @ SW_64K_D_T @ RbPlus
    {   2,   39,    3,   23} , // 64 pipes (16 PKRs) 4 bpe @ SW_64K_D_T @ RbPlus
    {   2,    6,    4,   24} , // 64 pipes (16 PKRs) 8 bpe @ SW_64K_D_T @ RbPlus
    {   2,    7,    5,   25} , // 64 pipes (16 PKRs) 16 bpe @ SW_64K_D_T @ RbPlus
    {   2,    5,   56,   16} , // 32 pipes (32 PKRs) 1 bpe @ SW_64K_D_T @ RbPlus
    {   2,    1,   57,   17} , // 32 pipes (32 PKRs) 2 bpe @ SW_64K_D_T @ RbPlus
    {   2,   39,   58,   18} , // 32 pipes (32 PKRs) 4 bpe @ SW_64K_D_T @ RbPlus
    {   2,    6,   59,   19} , // 32 pipes (32 PKRs) 8 bpe @ SW_64K_D_T @ RbPlus
    {   2,    7,   60,   20} , // 32 pipes (32 PKRs) 16 bpe @ SW_64K_D_T @ RbPlus
    {   2,    5,    1,   21} , // 64 pipes (32 PKRs) 1 bpe @ SW_64K_D_T @ RbPlus
    {   2,    1,    2,   22} , // 64 pipes (32 PKRs) 2 bpe @ SW_64K_D_T @ RbPlus
    {   2,   39,    3,   23} , // 64 pipes (32 PKRs) 4 bpe @ SW_64K_D_T @ RbPlus
    {   2,    6,    4,   24} , // 64 pipes (32 PKRs) 8 bpe @ SW_64K_D_T @ RbPlus
    {   2,    7,    5,   25} , // 64 pipes (32 PKRs) 16 bpe @ SW_64K_D_T @ RbPlus
};

const ADDR_SW_PATINFO SW_64K_S_X_RBPLUS_PATINFO[] =
{
    {   1,    0,    1,    1} , // 1 pipes (1 PKRs) 1 bpe @ SW_64K_S_X @ RbPlus
    {   1,    1,    2,    2} , // 1 pipes (1 PKRs) 2 bpe @ SW_64K_S_X @ RbPlus
    {   1,    2,    3,    3} , // 1 pipes (1 PKRs) 4 bpe @ SW_64K_S_X @ RbPlus
    {   1,    3,    4,    4} , // 1 pipes (1 PKRs) 8 bpe @ SW_64K_S_X @ RbPlus
    {   1,    4,    5,    5} , // 1 pipes (1 PKRs) 16 bpe @ SW_64K_S_X @ RbPlus
    {   3,    0,    6,    1} , // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_S_X @ RbPlus
    {   3,    1,    7,    2} , // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_S_X @ RbPlus
    {   3,    2,    8,    3} , // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_S_X @ RbPlus
    {   3,    3,    9,    4} , // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_S_X @ RbPlus
    {   3,    4,   10,    5} , // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_S_X @ RbPlus
    {   3,    0,  210,    1} , // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_S_X @ RbPlus
    {   3,    1,  211,    2} , // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_S_X @ RbPlus
    {   3,    2,  212,    3} , // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_S_X @ RbPlus
    {   3,    3,  213,    4} , // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_S_X @ RbPlus
    {   3,    4,  214,    5} , // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_S_X @ RbPlus
    {   3,    0,  215,    1} , // 8 pipes (2 PKRs) 1 bpe @ SW_64K_S_X @ RbPlus
    {   3,    1,  216,    2} , // 8 pipes (2 PKRs) 2 bpe @ SW_64K_S_X @ RbPlus
    {   3,    2,  217,    3} , // 8 pipes (2 PKRs) 4 bpe @ SW_64K_S_X @ RbPlus
    {   3,    3,  218,    4} , // 8 pipes (2 PKRs) 8 bpe @ SW_64K_S_X @ RbPlus
    {   3,    4,  219,    5} , // 8 pipes (2 PKRs) 16 bpe @ SW_64K_S_X @ RbPlus
    {   3,    0,   11,    1} , // 4 pipes (4 PKRs) 1 bpe @ SW_64K_S_X @ RbPlus
    {   3,    1,   12,    2} , // 4 pipes (4 PKRs) 2 bpe @ SW_64K_S_X @ RbPlus
    {   3,    2,   13,    3} , // 4 pipes (4 PKRs) 4 bpe @ SW_64K_S_X @ RbPlus
    {   3,    3,   14,    4} , // 4 pipes (4 PKRs) 8 bpe @ SW_64K_S_X @ RbPlus
    {   3,    4,   15,    5} , // 4 pipes (4 PKRs) 16 bpe @ SW_64K_S_X @ RbPlus
    {   3,    0,  220,    1} , // 8 pipes (4 PKRs) 1 bpe @ SW_64K_S_X @ RbPlus
    {   3,    1,  221,    2} , // 8 pipes (4 PKRs) 2 bpe @ SW_64K_S_X @ RbPlus
    {   3,    2,  222,    3} , // 8 pipes (4 PKRs) 4 bpe @ SW_64K_S_X @ RbPlus
    {   3,    3,  223,    4} , // 8 pipes (4 PKRs) 8 bpe @ SW_64K_S_X @ RbPlus
    {   3,    4,  224,    5} , // 8 pipes (4 PKRs) 16 bpe @ SW_64K_S_X @ RbPlus
    {   3,    0,  225,    1} , // 16 pipes (4 PKRs) 1 bpe @ SW_64K_S_X @ RbPlus
    {   3,    1,  226,    2} , // 16 pipes (4 PKRs) 2 bpe @ SW_64K_S_X @ RbPlus
    {   3,    2,  227,    3} , // 16 pipes (4 PKRs) 4 bpe @ SW_64K_S_X @ RbPlus
    {   3,    3,  228,    4} , // 16 pipes (4 PKRs) 8 bpe @ SW_64K_S_X @ RbPlus
    {   3,    4,  229,    5} , // 16 pipes (4 PKRs) 16 bpe @ SW_64K_S_X @ RbPlus
    {   3,    0,   16,    1} , // 8 pipes (8 PKRs) 1 bpe @ SW_64K_S_X @ RbPlus
    {   3,    1,   17,    2} , // 8 pipes (8 PKRs) 2 bpe @ SW_64K_S_X @ RbPlus
    {   3,    2,   18,    3} , // 8 pipes (8 PKRs) 4 bpe @ SW_64K_S_X @ RbPlus
    {   3,    3,   19,    4} , // 8 pipes (8 PKRs) 8 bpe @ SW_64K_S_X @ RbPlus
    {   3,    4,   20,    5} , // 8 pipes (8 PKRs) 16 bpe @ SW_64K_S_X @ RbPlus
    {   3,    0,  230,    1} , // 16 pipes (8 PKRs) 1 bpe @ SW_64K_S_X @ RbPlus
    {   3,    1,  231,    2} , // 16 pipes (8 PKRs) 2 bpe @ SW_64K_S_X @ RbPlus
    {   3,    2,  232,    3} , // 16 pipes (8 PKRs) 4 bpe @ SW_64K_S_X @ RbPlus
    {   3,    3,  233,    4} , // 16 pipes (8 PKRs) 8 bpe @ SW_64K_S_X @ RbPlus
    {   3,    4,  234,    5} , // 16 pipes (8 PKRs) 16 bpe @ SW_64K_S_X @ RbPlus
    {   3,    0,  250,    6} , // 32 pipes (8 PKRs) 1 bpe @ SW_64K_S_X @ RbPlus
    {   3,    1,  251,    7} , // 32 pipes (8 PKRs) 2 bpe @ SW_64K_S_X @ RbPlus
    {   3,    2,  252,    8} , // 32 pipes (8 PKRs) 4 bpe @ SW_64K_S_X @ RbPlus
    {   3,    3,  253,    9} , // 32 pipes (8 PKRs) 8 bpe @ SW_64K_S_X @ RbPlus
    {   3,    4,  254,   10} , // 32 pipes (8 PKRs) 16 bpe @ SW_64K_S_X @ RbPlus
    {   3,    0,   21,    1} , // 16 pipes (16 PKRs) 1 bpe @ SW_64K_S_X @ RbPlus
    {   3,    1,   22,    2} , // 16 pipes (16 PKRs) 2 bpe @ SW_64K_S_X @ RbPlus
    {   3,    2,   23,    3} , // 16 pipes (16 PKRs) 4 bpe @ SW_64K_S_X @ RbPlus
    {   3,    3,   24,    4} , // 16 pipes (16 PKRs) 8 bpe @ SW_64K_S_X @ RbPlus
    {   3,    4,   25,    5} , // 16 pipes (16 PKRs) 16 bpe @ SW_64K_S_X @ RbPlus
    {   3,    0,  255,    6} , // 32 pipes (16 PKRs) 1 bpe @ SW_64K_S_X @ RbPlus
    {   3,    1,  256,    7} , // 32 pipes (16 PKRs) 2 bpe @ SW_64K_S_X @ RbPlus
    {   3,    2,  257,    8} , // 32 pipes (16 PKRs) 4 bpe @ SW_64K_S_X @ RbPlus
    {   3,    3,  258,    9} , // 32 pipes (16 PKRs) 8 bpe @ SW_64K_S_X @ RbPlus
    {   3,    4,  259,   10} , // 32 pipes (16 PKRs) 16 bpe @ SW_64K_S_X @ RbPlus
    {   3,    0,  260,   11} , // 64 pipes (16 PKRs) 1 bpe @ SW_64K_S_X @ RbPlus
    {   3,    1,  261,   12} , // 64 pipes (16 PKRs) 2 bpe @ SW_64K_S_X @ RbPlus
    {   3,    2,  262,   13} , // 64 pipes (16 PKRs) 4 bpe @ SW_64K_S_X @ RbPlus
    {   3,    3,  263,   14} , // 64 pipes (16 PKRs) 8 bpe @ SW_64K_S_X @ RbPlus
    {   3,    4,  264,   15} , // 64 pipes (16 PKRs) 16 bpe @ SW_64K_S_X @ RbPlus
    {   3,    0,   26,    6} , // 32 pipes (32 PKRs) 1 bpe @ SW_64K_S_X @ RbPlus
    {   3,    1,   27,    7} , // 32 pipes (32 PKRs) 2 bpe @ SW_64K_S_X @ RbPlus
    {   3,    2,   28,    8} , // 32 pipes (32 PKRs) 4 bpe @ SW_64K_S_X @ RbPlus
    {   3,    3,   29,    9} , // 32 pipes (32 PKRs) 8 bpe @ SW_64K_S_X @ RbPlus
    {   3,    4,   30,   10} , // 32 pipes (32 PKRs) 16 bpe @ SW_64K_S_X @ RbPlus
    {   3,    0,  265,   11} , // 64 pipes (32 PKRs) 1 bpe @ SW_64K_S_X @ RbPlus
    {   3,    1,  266,   12} , // 64 pipes (32 PKRs) 2 bpe @ SW_64K_S_X @ RbPlus
    {   3,    2,  267,   13} , // 64 pipes (32 PKRs) 4 bpe @ SW_64K_S_X @ RbPlus
    {   3,    3,  268,   14} , // 64 pipes (32 PKRs) 8 bpe @ SW_64K_S_X @ RbPlus
    {   3,    4,  269,   15} , // 64 pipes (32 PKRs) 16 bpe @ SW_64K_S_X @ RbPlus
};

const ADDR_SW_PATINFO SW_64K_D_X_RBPLUS_PATINFO[] =
{
    {   1,    5,    1,    1} , // 1 pipes (1 PKRs) 1 bpe @ SW_64K_D_X @ RbPlus
    {   1,    1,    2,    2} , // 1 pipes (1 PKRs) 2 bpe @ SW_64K_D_X @ RbPlus
    {   1,   39,    3,    3} , // 1 pipes (1 PKRs) 4 bpe @ SW_64K_D_X @ RbPlus
    {   1,    6,    4,    4} , // 1 pipes (1 PKRs) 8 bpe @ SW_64K_D_X @ RbPlus
    {   1,    7,    5,    5} , // 1 pipes (1 PKRs) 16 bpe @ SW_64K_D_X @ RbPlus
    {   3,    5,    6,    1} , // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_D_X @ RbPlus
    {   3,    1,    7,    2} , // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_D_X @ RbPlus
    {   3,   39,    8,    3} , // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_D_X @ RbPlus
    {   3,    6,    9,    4} , // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_D_X @ RbPlus
    {   3,    7,   10,    5} , // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_D_X @ RbPlus
    {   3,    5,  210,    1} , // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_D_X @ RbPlus
    {   3,    1,  211,    2} , // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_D_X @ RbPlus
    {   3,   39,  212,    3} , // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_D_X @ RbPlus
    {   3,    6,  213,    4} , // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_D_X @ RbPlus
    {   3,    7,  214,    5} , // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_D_X @ RbPlus
    {   3,    5,  215,    1} , // 8 pipes (2 PKRs) 1 bpe @ SW_64K_D_X @ RbPlus
    {   3,    1,  216,    2} , // 8 pipes (2 PKRs) 2 bpe @ SW_64K_D_X @ RbPlus
    {   3,   39,  217,    3} , // 8 pipes (2 PKRs) 4 bpe @ SW_64K_D_X @ RbPlus
    {   3,    6,  218,    4} , // 8 pipes (2 PKRs) 8 bpe @ SW_64K_D_X @ RbPlus
    {   3,    7,  219,    5} , // 8 pipes (2 PKRs) 16 bpe @ SW_64K_D_X @ RbPlus
    {   3,    5,   11,    1} , // 4 pipes (4 PKRs) 1 bpe @ SW_64K_D_X @ RbPlus
    {   3,    1,   12,    2} , // 4 pipes (4 PKRs) 2 bpe @ SW_64K_D_X @ RbPlus
    {   3,   39,   13,    3} , // 4 pipes (4 PKRs) 4 bpe @ SW_64K_D_X @ RbPlus
    {   3,    6,   14,    4} , // 4 pipes (4 PKRs) 8 bpe @ SW_64K_D_X @ RbPlus
    {   3,    7,   15,    5} , // 4 pipes (4 PKRs) 16 bpe @ SW_64K_D_X @ RbPlus
    {   3,    5,  220,    1} , // 8 pipes (4 PKRs) 1 bpe @ SW_64K_D_X @ RbPlus
    {   3,    1,  221,    2} , // 8 pipes (4 PKRs) 2 bpe @ SW_64K_D_X @ RbPlus
    {   3,   39,  222,    3} , // 8 pipes (4 PKRs) 4 bpe @ SW_64K_D_X @ RbPlus
    {   3,    6,  223,    4} , // 8 pipes (4 PKRs) 8 bpe @ SW_64K_D_X @ RbPlus
    {   3,    7,  224,    5} , // 8 pipes (4 PKRs) 16 bpe @ SW_64K_D_X @ RbPlus
    {   3,    5,  225,    1} , // 16 pipes (4 PKRs) 1 bpe @ SW_64K_D_X @ RbPlus
    {   3,    1,  226,    2} , // 16 pipes (4 PKRs) 2 bpe @ SW_64K_D_X @ RbPlus
    {   3,   39,  227,    3} , // 16 pipes (4 PKRs) 4 bpe @ SW_64K_D_X @ RbPlus
    {   3,    6,  228,    4} , // 16 pipes (4 PKRs) 8 bpe @ SW_64K_D_X @ RbPlus
    {   3,    7,  229,    5} , // 16 pipes (4 PKRs) 16 bpe @ SW_64K_D_X @ RbPlus
    {   3,    5,   16,    1} , // 8 pipes (8 PKRs) 1 bpe @ SW_64K_D_X @ RbPlus
    {   3,    1,   17,    2} , // 8 pipes (8 PKRs) 2 bpe @ SW_64K_D_X @ RbPlus
    {   3,   39,   18,    3} , // 8 pipes (8 PKRs) 4 bpe @ SW_64K_D_X @ RbPlus
    {   3,    6,   19,    4} , // 8 pipes (8 PKRs) 8 bpe @ SW_64K_D_X @ RbPlus
    {   3,    7,   20,    5} , // 8 pipes (8 PKRs) 16 bpe @ SW_64K_D_X @ RbPlus
    {   3,    5,  230,    1} , // 16 pipes (8 PKRs) 1 bpe @ SW_64K_D_X @ RbPlus
    {   3,    1,  231,    2} , // 16 pipes (8 PKRs) 2 bpe @ SW_64K_D_X @ RbPlus
    {   3,   39,  232,    3} , // 16 pipes (8 PKRs) 4 bpe @ SW_64K_D_X @ RbPlus
    {   3,    6,  233,    4} , // 16 pipes (8 PKRs) 8 bpe @ SW_64K_D_X @ RbPlus
    {   3,    7,  234,    5} , // 16 pipes (8 PKRs) 16 bpe @ SW_64K_D_X @ RbPlus
    {   3,    5,  250,    6} , // 32 pipes (8 PKRs) 1 bpe @ SW_64K_D_X @ RbPlus
    {   3,    1,  251,    7} , // 32 pipes (8 PKRs) 2 bpe @ SW_64K_D_X @ RbPlus
    {   3,   39,  252,    8} , // 32 pipes (8 PKRs) 4 bpe @ SW_64K_D_X @ RbPlus
    {   3,    6,  253,    9} , // 32 pipes (8 PKRs) 8 bpe @ SW_64K_D_X @ RbPlus
    {   3,    7,  254,   10} , // 32 pipes (8 PKRs) 16 bpe @ SW_64K_D_X @ RbPlus
    {   3,    5,   21,    1} , // 16 pipes (16 PKRs) 1 bpe @ SW_64K_D_X @ RbPlus
    {   3,    1,   22,    2} , // 16 pipes (16 PKRs) 2 bpe @ SW_64K_D_X @ RbPlus
    {   3,   39,   23,    3} , // 16 pipes (16 PKRs) 4 bpe @ SW_64K_D_X @ RbPlus
    {   3,    6,   24,    4} , // 16 pipes (16 PKRs) 8 bpe @ SW_64K_D_X @ RbPlus
    {   3,    7,   25,    5} , // 16 pipes (16 PKRs) 16 bpe @ SW_64K_D_X @ RbPlus
    {   3,    5,  255,    6} , // 32 pipes (16 PKRs) 1 bpe @ SW_64K_D_X @ RbPlus
    {   3,    1,  256,    7} , // 32 pipes (16 PKRs) 2 bpe @ SW_64K_D_X @ RbPlus
    {   3,   39,  257,    8} , // 32 pipes (16 PKRs) 4 bpe @ SW_64K_D_X @ RbPlus
    {   3,    6,  258,    9} , // 32 pipes (16 PKRs) 8 bpe @ SW_64K_D_X @ RbPlus
    {   3,    7,  259,   10} , // 32 pipes (16 PKRs) 16 bpe @ SW_64K_D_X @ RbPlus
    {   3,    5,  260,   11} , // 64 pipes (16 PKRs) 1 bpe @ SW_64K_D_X @ RbPlus
    {   3,    1,  261,   12} , // 64 pipes (16 PKRs) 2 bpe @ SW_64K_D_X @ RbPlus
    {   3,   39,  262,   13} , // 64 pipes (16 PKRs) 4 bpe @ SW_64K_D_X @ RbPlus
    {   3,    6,  263,   14} , // 64 pipes (16 PKRs) 8 bpe @ SW_64K_D_X @ RbPlus
    {   3,    7,  264,   15} , // 64 pipes (16 PKRs) 16 bpe @ SW_64K_D_X @ RbPlus
    {   3,    5,   26,    6} , // 32 pipes (32 PKRs) 1 bpe @ SW_64K_D_X @ RbPlus
    {   3,    1,   27,    7} , // 32 pipes (32 PKRs) 2 bpe @ SW_64K_D_X @ RbPlus
    {   3,   39,   28,    8} , // 32 pipes (32 PKRs) 4 bpe @ SW_64K_D_X @ RbPlus
    {   3,    6,   29,    9} , // 32 pipes (32 PKRs) 8 bpe @ SW_64K_D_X @ RbPlus
    {   3,    7,   30,   10} , // 32 pipes (32 PKRs) 16 bpe @ SW_64K_D_X @ RbPlus
    {   3,    5,  265,   11} , // 64 pipes (32 PKRs) 1 bpe @ SW_64K_D_X @ RbPlus
    {   3,    1,  266,   12} , // 64 pipes (32 PKRs) 2 bpe @ SW_64K_D_X @ RbPlus
    {   3,   39,  267,   13} , // 64 pipes (32 PKRs) 4 bpe @ SW_64K_D_X @ RbPlus
    {   3,    6,  268,   14} , // 64 pipes (32 PKRs) 8 bpe @ SW_64K_D_X @ RbPlus
    {   3,    7,  269,   15} , // 64 pipes (32 PKRs) 16 bpe @ SW_64K_D_X @ RbPlus
};

const ADDR_SW_PATINFO SW_64K_R_X_1xaa_RBPLUS_PATINFO[] =
{
    {   2,    0,  270,  183} , // 1 pipes (1 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   2,    1,  271,  184} , // 1 pipes (1 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   2,   39,  272,  185} , // 1 pipes (1 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   2,    6,  273,  186} , // 1 pipes (1 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   2,    7,  274,  187} , // 1 pipes (1 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    0,  275,  183} , // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    1,  276,  188} , // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,   39,  277,  185} , // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    6,  278,  189} , // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    7,  279,  190} , // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    0,  280,  183} , // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    1,  281,  188} , // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,   39,  282,  185} , // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    6,  283,  191} , // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    7,  284,  192} , // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    0,  359,  193} , // 8 pipes (2 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    1,  360,  194} , // 8 pipes (2 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,   39,  361,  195} , // 8 pipes (2 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    6,  362,  196} , // 8 pipes (2 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    7,  363,  197} , // 8 pipes (2 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    0,  290,  198} , // 4 pipes (4 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    1,  291,  199} , // 4 pipes (4 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,   39,  292,  200} , // 4 pipes (4 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    6,  293,  201} , // 4 pipes (4 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    7,  294,  202} , // 4 pipes (4 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    0,  295,  193} , // 8 pipes (4 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    1,  296,  203} , // 8 pipes (4 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,   39,  297,  204} , // 8 pipes (4 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    6,  298,  205} , // 8 pipes (4 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    7,  299,  206} , // 8 pipes (4 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    0,  364,  207} , // 16 pipes (4 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    1,  364,  208} , // 16 pipes (4 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,   39,  364,  209} , // 16 pipes (4 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    6,  364,  210} , // 16 pipes (4 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    7,  364,  211} , // 16 pipes (4 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    0,  365,  353} , // 8 pipes (8 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    1,  366,  354} , // 8 pipes (8 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,   39,  367,  355} , // 8 pipes (8 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    6,  304,  215} , // 8 pipes (8 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    7,  305,  216} , // 8 pipes (8 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    0,  307,  207} , // 16 pipes (8 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    1,  307,  217} , // 16 pipes (8 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,   39,  307,  209} , // 16 pipes (8 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    6,  307,  210} , // 16 pipes (8 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    7,  307,  218} , // 16 pipes (8 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    0,  307,  356} , // 32 pipes (8 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    1,  307,  357} , // 32 pipes (8 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,   39,  307,  358} , // 32 pipes (8 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    6,  307,  359} , // 32 pipes (8 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    7,  307,  360} , // 32 pipes (8 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    0,  309,  361} , // 16 pipes (16 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    1,  309,  362} , // 16 pipes (16 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,   39,  309,  363} , // 16 pipes (16 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    6,  309,  227} , // 16 pipes (16 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    7,  310,  228} , // 16 pipes (16 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    0,  309,  364} , // 32 pipes (16 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    1,  309,  365} , // 32 pipes (16 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,   39,  309,  366} , // 32 pipes (16 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    6,  309,  232} , // 32 pipes (16 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    7,  310,  233} , // 32 pipes (16 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    0,  309,  367} , // 64 pipes (16 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    1,  309,  368} , // 64 pipes (16 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,   39,  309,  369} , // 64 pipes (16 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    6,  309,  370} , // 64 pipes (16 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    7,  310,  371} , // 64 pipes (16 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    0,  368,  372} , // 32 pipes (32 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    1,  368,  373} , // 32 pipes (32 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,   39,  368,  374} , // 32 pipes (32 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    6,  312,  242} , // 32 pipes (32 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    7,  313,  243} , // 32 pipes (32 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    0,  368,  375} , // 64 pipes (32 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    1,  368,  376} , // 64 pipes (32 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,   39,  368,  377} , // 64 pipes (32 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    6,  312,  247} , // 64 pipes (32 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ RbPlus
    {   3,    7,  313,  248} , // 64 pipes (32 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ RbPlus
};

const ADDR_SW_PATINFO SW_64K_R_X_2xaa_RBPLUS_PATINFO[] =
{
    {   3,    0,  369,  378} , // 1 pipes (1 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    1,  271,  379} , // 1 pipes (1 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,   39,  315,  380} , // 1 pipes (1 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    6,  273,  381} , // 1 pipes (1 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    7,  316,  382} , // 1 pipes (1 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    0,  275,  378} , // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    1,  276,  379} , // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,   39,  277,  380} , // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    6,  278,  381} , // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    7,  279,  383} , // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    0,  280,  378} , // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    1,  281,  379} , // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,   39,  282,  380} , // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    6,  283,  384} , // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    7,  284,  385} , // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    0,  359,  386} , // 8 pipes (2 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    1,  360,  387} , // 8 pipes (2 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,   39,  361,  388} , // 8 pipes (2 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    6,  362,  389} , // 8 pipes (2 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    7,  370,  390} , // 8 pipes (2 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    0,  290,  391} , // 4 pipes (4 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    1,  291,  392} , // 4 pipes (4 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,   39,  292,  393} , // 4 pipes (4 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    6,  293,  394} , // 4 pipes (4 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    7,  294,  395} , // 4 pipes (4 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    0,  295,  386} , // 8 pipes (4 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    1,  296,  396} , // 8 pipes (4 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,   39,  297,  397} , // 8 pipes (4 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    6,  298,  398} , // 8 pipes (4 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    7,  299,  399} , // 8 pipes (4 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    0,  364,  400} , // 16 pipes (4 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    1,  364,  401} , // 16 pipes (4 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,   39,  364,  402} , // 16 pipes (4 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    6,  364,  403} , // 16 pipes (4 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    7,  371,  271} , // 16 pipes (4 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    0,  365,  404} , // 8 pipes (8 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    1,  366,  405} , // 8 pipes (8 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,   39,  367,  406} , // 8 pipes (8 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    6,  304,  407} , // 8 pipes (8 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    7,  322,  408} , // 8 pipes (8 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    0,  307,  400} , // 16 pipes (8 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    1,  307,  401} , // 16 pipes (8 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,   39,  307,  402} , // 16 pipes (8 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    6,  307,  403} , // 16 pipes (8 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    7,  372,  218} , // 16 pipes (8 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    0,  307,  409} , // 32 pipes (8 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    1,  307,  410} , // 32 pipes (8 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,   39,  307,  411} , // 32 pipes (8 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    6,  307,  412} , // 32 pipes (8 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    7,  372,  360} , // 32 pipes (8 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    0,  309,  413} , // 16 pipes (16 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    1,  309,  414} , // 16 pipes (16 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,   39,  309,  415} , // 16 pipes (16 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    6,  310,  416} , // 16 pipes (16 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    7,  373,  228} , // 16 pipes (16 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    0,  309,  417} , // 32 pipes (16 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    1,  309,  418} , // 32 pipes (16 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,   39,  309,  419} , // 32 pipes (16 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    6,  310,  420} , // 32 pipes (16 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    7,  373,  233} , // 32 pipes (16 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    0,  309,  421} , // 64 pipes (16 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    1,  309,  422} , // 64 pipes (16 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,   39,  309,  423} , // 64 pipes (16 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    6,  310,  424} , // 64 pipes (16 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    7,  373,  371} , // 64 pipes (16 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    0,  312,  425} , // 32 pipes (32 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    1,  312,  426} , // 32 pipes (32 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,   39,  312,  427} , // 32 pipes (32 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    6,  313,  428} , // 32 pipes (32 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    7,  374,  243} , // 32 pipes (32 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    0,  312,  429} , // 64 pipes (32 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    1,  312,  430} , // 64 pipes (32 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,   39,  312,  431} , // 64 pipes (32 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    6,  313,  432} , // 64 pipes (32 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ RbPlus
    {   3,    7,  374,  248} , // 64 pipes (32 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ RbPlus
};

const ADDR_SW_PATINFO SW_64K_R_X_4xaa_RBPLUS_PATINFO[] =
{
    {   3,    0,  270,  433} , // 1 pipes (1 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    1,  271,  434} , // 1 pipes (1 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,   39,  272,  435} , // 1 pipes (1 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    6,  273,  436} , // 1 pipes (1 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    7,  274,  437} , // 1 pipes (1 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    0,  275,  433} , // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    1,  276,  438} , // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,   39,  277,  435} , // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    6,  278,  439} , // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    7,  279,  440} , // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    0,  280,  433} , // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    1,  281,  438} , // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,   39,  282,  435} , // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    6,  283,  441} , // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    7,  284,  442} , // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    0,  359,  443} , // 8 pipes (2 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    1,  360,  444} , // 8 pipes (2 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,   39,  361,  445} , // 8 pipes (2 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    6,  362,  446} , // 8 pipes (2 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    7,  375,  447} , // 8 pipes (2 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    0,  290,  448} , // 4 pipes (4 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    1,  291,  449} , // 4 pipes (4 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,   39,  292,  450} , // 4 pipes (4 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    6,  293,  451} , // 4 pipes (4 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    7,  376,  452} , // 4 pipes (4 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    0,  295,  443} , // 8 pipes (4 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    1,  296,  453} , // 8 pipes (4 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,   39,  297,  454} , // 8 pipes (4 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    6,  298,  446} , // 8 pipes (4 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    7,  377,  399} , // 8 pipes (4 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    0,  364,  455} , // 16 pipes (4 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    1,  364,  456} , // 16 pipes (4 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,   39,  364,  457} , // 16 pipes (4 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    6,  378,  458} , // 16 pipes (4 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    7,  379,  271} , // 16 pipes (4 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    0,  365,  459} , // 8 pipes (8 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    1,  366,  460} , // 8 pipes (8 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,   39,  367,  461} , // 8 pipes (8 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    6,  380,  407} , // 8 pipes (8 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    7,  381,  408} , // 8 pipes (8 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    0,  307,  455} , // 16 pipes (8 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    1,  307,  462} , // 16 pipes (8 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,   39,  307,  457} , // 16 pipes (8 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    6,  382,  403} , // 16 pipes (8 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    7,  383,  218} , // 16 pipes (8 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    0,  307,  463} , // 32 pipes (8 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    1,  307,  464} , // 32 pipes (8 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,   39,  307,  465} , // 32 pipes (8 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    6,  382,  412} , // 32 pipes (8 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    7,  383,  360} , // 32 pipes (8 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    0,  310,  466} , // 16 pipes (16 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    1,  310,  467} , // 16 pipes (16 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,   39,  310,  468} , // 16 pipes (16 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    6,  384,  416} , // 16 pipes (16 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    7,  385,  228} , // 16 pipes (16 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    0,  310,  469} , // 32 pipes (16 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    1,  310,  470} , // 32 pipes (16 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,   39,  310,  471} , // 32 pipes (16 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    6,  384,  420} , // 32 pipes (16 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    7,  385,  233} , // 32 pipes (16 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    0,  310,  472} , // 64 pipes (16 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    1,  310,  473} , // 64 pipes (16 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,   39,  310,  474} , // 64 pipes (16 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    6,  384,  424} , // 64 pipes (16 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    7,  385,  371} , // 64 pipes (16 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    0,  313,  475} , // 32 pipes (32 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    1,  313,  476} , // 32 pipes (32 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,   39,  313,  477} , // 32 pipes (32 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    6,  386,  428} , // 32 pipes (32 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    7,  387,  243} , // 32 pipes (32 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    0,  313,  478} , // 64 pipes (32 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    1,  313,  479} , // 64 pipes (32 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,   39,  313,  480} , // 64 pipes (32 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    6,  386,  432} , // 64 pipes (32 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ RbPlus
    {   3,    7,  387,  248} , // 64 pipes (32 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ RbPlus
};

const ADDR_SW_PATINFO SW_64K_R_X_8xaa_RBPLUS_PATINFO[] =
{
    {   3,    0,  369,  481} , // 1 pipes (1 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    1,  271,  482} , // 1 pipes (1 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,   39,  315,  483} , // 1 pipes (1 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    6,  273,  484} , // 1 pipes (1 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    7,  316,  485} , // 1 pipes (1 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    0,  275,  481} , // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    1,  276,  482} , // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,   39,  277,  483} , // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    6,  278,  484} , // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    7,  340,  486} , // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    0,  280,  481} , // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    1,  281,  482} , // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,   39,  282,  483} , // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    6,  283,  487} , // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    7,  388,  488} , // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    0,  359,  489} , // 8 pipes (2 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    1,  360,  490} , // 8 pipes (2 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,   39,  389,  491} , // 8 pipes (2 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    6,  390,  492} , // 8 pipes (2 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    7,  391,  493} , // 8 pipes (2 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    0,  392,  494} , // 4 pipes (4 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    1,  291,  495} , // 4 pipes (4 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,   39,  292,  496} , // 4 pipes (4 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    6,  393,  497} , // 4 pipes (4 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    7,  394,  452} , // 4 pipes (4 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    0,  295,  489} , // 8 pipes (4 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    1,  296,  498} , // 8 pipes (4 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,   39,  297,  499} , // 8 pipes (4 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    6,  395,  500} , // 8 pipes (4 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    7,  396,  399} , // 8 pipes (4 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    0,  364,  501} , // 16 pipes (4 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    1,  364,  502} , // 16 pipes (4 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,   39,  397,  503} , // 16 pipes (4 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    6,  398,  504} , // 16 pipes (4 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    7,  399,  271} , // 16 pipes (4 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    0,  400,  505} , // 8 pipes (8 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    1,  302,  506} , // 8 pipes (8 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,   39,  303,  507} , // 8 pipes (8 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    6,  401,  508} , // 8 pipes (8 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    7,  402,  408} , // 8 pipes (8 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    0,  403,  501} , // 16 pipes (8 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    1,  403,  502} , // 16 pipes (8 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,   39,  404,  457} , // 16 pipes (8 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    6,  405,  403} , // 16 pipes (8 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    7,  406,  218} , // 16 pipes (8 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    0,  403,  509} , // 32 pipes (8 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    1,  403,  510} , // 32 pipes (8 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,   39,  404,  465} , // 32 pipes (8 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    6,  405,  412} , // 32 pipes (8 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    7,  406,  360} , // 32 pipes (8 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    0,  407,  511} , // 16 pipes (16 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    1,  408,  512} , // 16 pipes (16 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,   39,  409,  468} , // 16 pipes (16 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    6,  410,  416} , // 16 pipes (16 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    7,  411,  228} , // 16 pipes (16 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    0,  407,  513} , // 32 pipes (16 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    1,  408,  514} , // 32 pipes (16 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,   39,  409,  471} , // 32 pipes (16 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    6,  410,  420} , // 32 pipes (16 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    7,  411,  233} , // 32 pipes (16 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    0,  407,  515} , // 64 pipes (16 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    1,  408,  516} , // 64 pipes (16 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,   39,  409,  474} , // 64 pipes (16 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    6,  410,  424} , // 64 pipes (16 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    7,  411,  371} , // 64 pipes (16 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    0,  412,  517} , // 32 pipes (32 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    1,  413,  518} , // 32 pipes (32 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,   39,  414,  477} , // 32 pipes (32 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    6,  415,  428} , // 32 pipes (32 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    7,  416,  243} , // 32 pipes (32 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    0,  412,  519} , // 64 pipes (32 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    1,  413,  520} , // 64 pipes (32 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,   39,  414,  480} , // 64 pipes (32 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    6,  415,  432} , // 64 pipes (32 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ RbPlus
    {   3,    7,  416,  248} , // 64 pipes (32 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ RbPlus
};

const ADDR_SW_PATINFO SW_64K_Z_X_1xaa_RBPLUS_PATINFO[] =
{
    {   2,    8,  270,  183} , // 1 pipes (1 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   2,    9,  271,  184} , // 1 pipes (1 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   2,   10,  272,  185} , // 1 pipes (1 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   2,   11,  273,  186} , // 1 pipes (1 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   2,    7,  274,  187} , // 1 pipes (1 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    8,  275,  183} , // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    9,  276,  188} , // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,   10,  277,  185} , // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,   11,  278,  189} , // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    7,  279,  190} , // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    8,  280,  183} , // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    9,  281,  188} , // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,   10,  282,  185} , // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,   11,  283,  191} , // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    7,  284,  192} , // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    8,  285,  193} , // 8 pipes (2 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    9,  286,  194} , // 8 pipes (2 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,   10,  287,  195} , // 8 pipes (2 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,   11,  288,  196} , // 8 pipes (2 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    7,  289,  197} , // 8 pipes (2 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    8,  290,  198} , // 4 pipes (4 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    9,  291,  199} , // 4 pipes (4 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,   10,  292,  200} , // 4 pipes (4 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,   11,  293,  201} , // 4 pipes (4 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    7,  294,  202} , // 4 pipes (4 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    8,  295,  193} , // 8 pipes (4 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    9,  296,  203} , // 8 pipes (4 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,   10,  297,  204} , // 8 pipes (4 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,   11,  298,  205} , // 8 pipes (4 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    7,  299,  206} , // 8 pipes (4 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    8,  300,  207} , // 16 pipes (4 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    9,  300,  208} , // 16 pipes (4 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,   10,  300,  209} , // 16 pipes (4 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,   11,  300,  210} , // 16 pipes (4 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    7,  300,  211} , // 16 pipes (4 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    8,  301,  212} , // 8 pipes (8 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    9,  302,  213} , // 8 pipes (8 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,   10,  303,  214} , // 8 pipes (8 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,   11,  304,  215} , // 8 pipes (8 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    7,  305,  216} , // 8 pipes (8 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    8,  306,  207} , // 16 pipes (8 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    9,  306,  217} , // 16 pipes (8 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,   10,  306,  209} , // 16 pipes (8 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,   11,  307,  210} , // 16 pipes (8 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    7,  307,  218} , // 16 pipes (8 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    8,  306,  219} , // 32 pipes (8 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    9,  306,  220} , // 32 pipes (8 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,   10,  306,  221} , // 32 pipes (8 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,   11,  307,  222} , // 32 pipes (8 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    7,  307,  223} , // 32 pipes (8 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    8,  308,  224} , // 16 pipes (16 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    9,  308,  225} , // 16 pipes (16 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,   10,  308,  226} , // 16 pipes (16 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,   11,  309,  227} , // 16 pipes (16 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    7,  310,  228} , // 16 pipes (16 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    8,  308,  229} , // 32 pipes (16 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    9,  308,  230} , // 32 pipes (16 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,   10,  308,  231} , // 32 pipes (16 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,   11,  309,  232} , // 32 pipes (16 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    7,  310,  233} , // 32 pipes (16 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    8,  308,  234} , // 64 pipes (16 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    9,  308,  235} , // 64 pipes (16 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,   10,  308,  236} , // 64 pipes (16 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,   11,  309,  237} , // 64 pipes (16 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    7,  310,  238} , // 64 pipes (16 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    8,  311,  239} , // 32 pipes (32 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    9,  311,  240} , // 32 pipes (32 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,   10,  311,  241} , // 32 pipes (32 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,   11,  312,  242} , // 32 pipes (32 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    7,  313,  243} , // 32 pipes (32 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    8,  311,  244} , // 64 pipes (32 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    9,  311,  245} , // 64 pipes (32 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,   10,  311,  246} , // 64 pipes (32 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,   11,  312,  247} , // 64 pipes (32 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ RbPlus
    {   3,    7,  313,  248} , // 64 pipes (32 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ RbPlus
};

const ADDR_SW_PATINFO SW_64K_Z_X_2xaa_RBPLUS_PATINFO[] =
{
    {   2,   13,  314,  249} , // 1 pipes (1 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   2,   14,  272,  185} , // 1 pipes (1 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   15,  315,  250} , // 1 pipes (1 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   16,  273,  251} , // 1 pipes (1 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   17,  316,  252} , // 1 pipes (1 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   13,  317,  249} , // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   14,  277,  185} , // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   15,  277,  250} , // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   16,  318,  253} , // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   17,  279,  254} , // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   13,  281,  255} , // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   14,  282,  185} , // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   15,  282,  250} , // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   16,  319,  256} , // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   17,  284,  257} , // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   13,  286,  258} , // 8 pipes (2 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   14,  287,  204} , // 8 pipes (2 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   15,  287,  259} , // 8 pipes (2 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   16,  289,  260} , // 8 pipes (2 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   17,  289,  261} , // 8 pipes (2 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   13,  291,  262} , // 4 pipes (4 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   14,  292,  200} , // 4 pipes (4 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   15,  292,  263} , // 4 pipes (4 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   16,  293,  264} , // 4 pipes (4 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   17,  294,  265} , // 4 pipes (4 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   13,  296,  258} , // 8 pipes (4 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   14,  297,  204} , // 8 pipes (4 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   15,  297,  259} , // 8 pipes (4 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   16,  298,  266} , // 8 pipes (4 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   17,  299,  261} , // 8 pipes (4 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   13,  300,  267} , // 16 pipes (4 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   14,  300,  268} , // 16 pipes (4 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   15,  300,  269} , // 16 pipes (4 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   16,  320,  270} , // 16 pipes (4 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   17,  321,  271} , // 16 pipes (4 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   13,  302,  272} , // 8 pipes (8 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   14,  303,  214} , // 8 pipes (8 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   15,  303,  273} , // 8 pipes (8 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   16,  305,  274} , // 8 pipes (8 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   17,  322,  275} , // 8 pipes (8 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   13,  306,  208} , // 16 pipes (8 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   14,  306,  209} , // 16 pipes (8 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   15,  306,  276} , // 16 pipes (8 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   16,  307,  277} , // 16 pipes (8 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   17,  323,  277} , // 16 pipes (8 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   13,  306,  230} , // 32 pipes (8 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   14,  306,  231} , // 32 pipes (8 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   15,  306,  278} , // 32 pipes (8 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   16,  307,  233} , // 32 pipes (8 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   17,  323,  233} , // 32 pipes (8 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   13,  308,  225} , // 16 pipes (16 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   14,  308,  226} , // 16 pipes (16 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   15,  308,  279} , // 16 pipes (16 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   16,  310,  280} , // 16 pipes (16 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   17,  324,  280} , // 16 pipes (16 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   13,  308,  230} , // 32 pipes (16 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   14,  308,  231} , // 32 pipes (16 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   15,  308,  278} , // 32 pipes (16 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   16,  310,  281} , // 32 pipes (16 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   17,  324,  281} , // 32 pipes (16 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   13,  308,  282} , // 64 pipes (16 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   14,  308,  236} , // 64 pipes (16 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   15,  308,  283} , // 64 pipes (16 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   16,  310,  284} , // 64 pipes (16 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   17,  324,  284} , // 64 pipes (16 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   13,  311,  285} , // 32 pipes (32 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   14,  311,  241} , // 32 pipes (32 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   15,  311,  286} , // 32 pipes (32 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   16,  313,  243} , // 32 pipes (32 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   17,  325,  243} , // 32 pipes (32 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   13,  311,  287} , // 64 pipes (32 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   14,  311,  246} , // 64 pipes (32 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   15,  311,  288} , // 64 pipes (32 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   16,  313,  248} , // 64 pipes (32 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ RbPlus
    {   3,   17,  325,  248} , // 64 pipes (32 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ RbPlus
};

const ADDR_SW_PATINFO SW_64K_Z_X_4xaa_RBPLUS_PATINFO[] =
{
    {   2,   18,  272,  185} , // 1 pipes (1 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   19,  272,  289} , // 1 pipes (1 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   20,  272,  290} , // 1 pipes (1 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   21,  273,  291} , // 1 pipes (1 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   22,  274,  292} , // 1 pipes (1 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   18,  277,  185} , // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   19,  326,  293} , // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   20,  277,  294} , // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   21,  278,  295} , // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   22,  279,  296} , // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   18,  282,  185} , // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   19,  282,  297} , // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   20,  282,  294} , // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   21,  283,  298} , // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   22,  284,  299} , // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   18,  287,  195} , // 8 pipes (2 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   19,  287,  300} , // 8 pipes (2 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   20,  287,  301} , // 8 pipes (2 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   21,  288,  302} , // 8 pipes (2 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   22,  327,  303} , // 8 pipes (2 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   18,  292,  200} , // 4 pipes (4 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   19,  292,  304} , // 4 pipes (4 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   20,  292,  305} , // 4 pipes (4 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   21,  328,  306} , // 4 pipes (4 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   22,  329,  307} , // 4 pipes (4 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   18,  297,  204} , // 8 pipes (4 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   19,  297,  308} , // 8 pipes (4 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   20,  297,  309} , // 8 pipes (4 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   21,  299,  310} , // 8 pipes (4 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   22,  330,  311} , // 8 pipes (4 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   18,  300,  209} , // 16 pipes (4 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   19,  300,  312} , // 16 pipes (4 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   20,  300,  313} , // 16 pipes (4 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   21,  321,  314} , // 16 pipes (4 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   22,  331,  315} , // 16 pipes (4 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   18,  303,  214} , // 8 pipes (8 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   19,  303,  316} , // 8 pipes (8 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   20,  303,  317} , // 8 pipes (8 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   21,  332,  318} , // 8 pipes (8 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   22,  333,  319} , // 8 pipes (8 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   18,  306,  209} , // 16 pipes (8 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   19,  306,  312} , // 16 pipes (8 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   20,  306,  320} , // 16 pipes (8 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   21,  334,  320} , // 16 pipes (8 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   22,  335,  320} , // 16 pipes (8 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   18,  306,  221} , // 32 pipes (8 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   19,  306,  321} , // 32 pipes (8 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   20,  306,  322} , // 32 pipes (8 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   21,  334,  322} , // 32 pipes (8 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   22,  335,  322} , // 32 pipes (8 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   18,  308,  226} , // 16 pipes (16 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   19,  308,  323} , // 16 pipes (16 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   20,  308,  280} , // 16 pipes (16 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   21,  336,  280} , // 16 pipes (16 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   22,  337,  280} , // 16 pipes (16 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   18,  308,  231} , // 32 pipes (16 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   19,  308,  321} , // 32 pipes (16 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   20,  308,  281} , // 32 pipes (16 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   21,  336,  281} , // 32 pipes (16 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   22,  337,  281} , // 32 pipes (16 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   18,  308,  236} , // 64 pipes (16 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   19,  308,  283} , // 64 pipes (16 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   20,  308,  284} , // 64 pipes (16 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   21,  336,  284} , // 64 pipes (16 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   22,  337,  284} , // 64 pipes (16 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   18,  311,  241} , // 32 pipes (32 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   19,  311,  324} , // 32 pipes (32 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   20,  311,  243} , // 32 pipes (32 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   21,  338,  243} , // 32 pipes (32 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   22,  339,  243} , // 32 pipes (32 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   18,  311,  246} , // 64 pipes (32 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   19,  311,  325} , // 64 pipes (32 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   20,  311,  248} , // 64 pipes (32 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   21,  338,  248} , // 64 pipes (32 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ RbPlus
    {   3,   22,  339,  248} , // 64 pipes (32 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ RbPlus
};

const ADDR_SW_PATINFO SW_64K_Z_X_8xaa_RBPLUS_PATINFO[] =
{
    {   3,   23,  315,  250} , // 1 pipes (1 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   24,  272,  290} , // 1 pipes (1 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   25,  315,  326} , // 1 pipes (1 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   26,  273,  327} , // 1 pipes (1 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   27,  316,  328} , // 1 pipes (1 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   23,  277,  250} , // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   24,  277,  294} , // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   25,  277,  326} , // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   26,  318,  329} , // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   27,  340,  330} , // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   23,  282,  250} , // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   24,  282,  294} , // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   25,  282,  326} , // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   26,  319,  331} , // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   27,  341,  332} , // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   23,  287,  259} , // 8 pipes (2 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   24,  287,  333} , // 8 pipes (2 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   25,  287,  334} , // 8 pipes (2 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   26,  342,  335} , // 8 pipes (2 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   27,  343,  336} , // 8 pipes (2 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   23,  292,  263} , // 4 pipes (4 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   24,  292,  305} , // 4 pipes (4 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   25,  292,  337} , // 4 pipes (4 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   26,  344,  338} , // 4 pipes (4 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   27,  345,  339} , // 4 pipes (4 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   23,  297,  259} , // 8 pipes (4 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   24,  297,  309} , // 8 pipes (4 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   25,  297,  334} , // 8 pipes (4 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   26,  346,  340} , // 8 pipes (4 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   27,  347,  341} , // 8 pipes (4 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   23,  300,  276} , // 16 pipes (4 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   24,  300,  313} , // 16 pipes (4 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   25,  348,  342} , // 16 pipes (4 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   26,  349,  343} , // 16 pipes (4 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   27,  350,  344} , // 16 pipes (4 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   23,  303,  273} , // 8 pipes (8 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   24,  303,  317} , // 8 pipes (8 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   25,  303,  345} , // 8 pipes (8 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   26,  333,  319} , // 8 pipes (8 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   27,  351,  319} , // 8 pipes (8 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   23,  306,  276} , // 16 pipes (8 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   24,  306,  320} , // 16 pipes (8 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   25,  352,  346} , // 16 pipes (8 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   26,  335,  320} , // 16 pipes (8 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   27,  353,  320} , // 16 pipes (8 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   23,  306,  278} , // 32 pipes (8 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   24,  306,  322} , // 32 pipes (8 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   25,  352,  347} , // 32 pipes (8 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   26,  335,  322} , // 32 pipes (8 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   27,  353,  348} , // 32 pipes (8 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   23,  308,  279} , // 16 pipes (16 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   24,  308,  280} , // 16 pipes (16 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   25,  354,  349} , // 16 pipes (16 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   26,  337,  280} , // 16 pipes (16 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   27,  355,  280} , // 16 pipes (16 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   23,  308,  278} , // 32 pipes (16 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   24,  308,  281} , // 32 pipes (16 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   25,  356,  350} , // 32 pipes (16 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   26,  337,  281} , // 32 pipes (16 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   27,  355,  281} , // 32 pipes (16 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   23,  308,  283} , // 64 pipes (16 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   24,  308,  284} , // 64 pipes (16 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   25,  356,  351} , // 64 pipes (16 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   26,  337,  284} , // 64 pipes (16 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   27,  355,  284} , // 64 pipes (16 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   23,  311,  286} , // 32 pipes (32 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   24,  311,  243} , // 32 pipes (32 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   25,  357,  352} , // 32 pipes (32 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   26,  339,  243} , // 32 pipes (32 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   27,  358,  243} , // 32 pipes (32 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   23,  311,  288} , // 64 pipes (32 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   24,  311,  248} , // 64 pipes (32 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   25,  325,  248} , // 64 pipes (32 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   26,  339,  248} , // 64 pipes (32 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ RbPlus
    {   3,   27,  358,  248} , // 64 pipes (32 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ RbPlus
};

const ADDR_SW_PATINFO SW_64K_S3_RBPLUS_PATINFO[] =
{
    {   1,   29,  131,  148} , // 1 pipes (1 PKRs) 1 bpe @ SW_64K_S3 @ RbPlus
    {   1,   30,  132,  149} , // 1 pipes (1 PKRs) 2 bpe @ SW_64K_S3 @ RbPlus
    {   1,   31,  133,  150} , // 1 pipes (1 PKRs) 4 bpe @ SW_64K_S3 @ RbPlus
    {   1,   32,  134,  151} , // 1 pipes (1 PKRs) 8 bpe @ SW_64K_S3 @ RbPlus
    {   1,   33,  135,  152} , // 1 pipes (1 PKRs) 16 bpe @ SW_64K_S3 @ RbPlus
    {   1,   29,  131,  148} , // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_S3 @ RbPlus
    {   1,   30,  132,  149} , // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_S3 @ RbPlus
    {   1,   31,  133,  150} , // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_S3 @ RbPlus
    {   1,   32,  134,  151} , // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_S3 @ RbPlus
    {   1,   33,  135,  152} , // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_S3 @ RbPlus
    {   1,   29,  131,  148} , // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_S3 @ RbPlus
    {   1,   30,  132,  149} , // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_S3 @ RbPlus
    {   1,   31,  133,  150} , // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_S3 @ RbPlus
    {   1,   32,  134,  151} , // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_S3 @ RbPlus
    {   1,   33,  135,  152} , // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_S3 @ RbPlus
    {   1,   29,  131,  148} , // 8 pipes (2 PKRs) 1 bpe @ SW_64K_S3 @ RbPlus
    {   1,   30,  132,  149} , // 8 pipes (2 PKRs) 2 bpe @ SW_64K_S3 @ RbPlus
    {   1,   31,  133,  150} , // 8 pipes (2 PKRs) 4 bpe @ SW_64K_S3 @ RbPlus
    {   1,   32,  134,  151} , // 8 pipes (2 PKRs) 8 bpe @ SW_64K_S3 @ RbPlus
    {   1,   33,  135,  152} , // 8 pipes (2 PKRs) 16 bpe @ SW_64K_S3 @ RbPlus
    {   1,   29,  131,  148} , // 4 pipes (4 PKRs) 1 bpe @ SW_64K_S3 @ RbPlus
    {   1,   30,  132,  149} , // 4 pipes (4 PKRs) 2 bpe @ SW_64K_S3 @ RbPlus
    {   1,   31,  133,  150} , // 4 pipes (4 PKRs) 4 bpe @ SW_64K_S3 @ RbPlus
    {   1,   32,  134,  151} , // 4 pipes (4 PKRs) 8 bpe @ SW_64K_S3 @ RbPlus
    {   1,   33,  135,  152} , // 4 pipes (4 PKRs) 16 bpe @ SW_64K_S3 @ RbPlus
    {   1,   29,  131,  148} , // 8 pipes (4 PKRs) 1 bpe @ SW_64K_S3 @ RbPlus
    {   1,   30,  132,  149} , // 8 pipes (4 PKRs) 2 bpe @ SW_64K_S3 @ RbPlus
    {   1,   31,  133,  150} , // 8 pipes (4 PKRs) 4 bpe @ SW_64K_S3 @ RbPlus
    {   1,   32,  134,  151} , // 8 pipes (4 PKRs) 8 bpe @ SW_64K_S3 @ RbPlus
    {   1,   33,  135,  152} , // 8 pipes (4 PKRs) 16 bpe @ SW_64K_S3 @ RbPlus
    {   1,   29,  131,  148} , // 16 pipes (4 PKRs) 1 bpe @ SW_64K_S3 @ RbPlus
    {   1,   30,  132,  149} , // 16 pipes (4 PKRs) 2 bpe @ SW_64K_S3 @ RbPlus
    {   1,   31,  133,  150} , // 16 pipes (4 PKRs) 4 bpe @ SW_64K_S3 @ RbPlus
    {   1,   32,  134,  151} , // 16 pipes (4 PKRs) 8 bpe @ SW_64K_S3 @ RbPlus
    {   1,   33,  135,  152} , // 16 pipes (4 PKRs) 16 bpe @ SW_64K_S3 @ RbPlus
    {   1,   29,  131,  148} , // 8 pipes (8 PKRs) 1 bpe @ SW_64K_S3 @ RbPlus
    {   1,   30,  132,  149} , // 8 pipes (8 PKRs) 2 bpe @ SW_64K_S3 @ RbPlus
    {   1,   31,  133,  150} , // 8 pipes (8 PKRs) 4 bpe @ SW_64K_S3 @ RbPlus
    {   1,   32,  134,  151} , // 8 pipes (8 PKRs) 8 bpe @ SW_64K_S3 @ RbPlus
    {   1,   33,  135,  152} , // 8 pipes (8 PKRs) 16 bpe @ SW_64K_S3 @ RbPlus
    {   1,   29,  131,  148} , // 16 pipes (8 PKRs) 1 bpe @ SW_64K_S3 @ RbPlus
    {   1,   30,  132,  149} , // 16 pipes (8 PKRs) 2 bpe @ SW_64K_S3 @ RbPlus
    {   1,   31,  133,  150} , // 16 pipes (8 PKRs) 4 bpe @ SW_64K_S3 @ RbPlus
    {   1,   32,  134,  151} , // 16 pipes (8 PKRs) 8 bpe @ SW_64K_S3 @ RbPlus
    {   1,   33,  135,  152} , // 16 pipes (8 PKRs) 16 bpe @ SW_64K_S3 @ RbPlus
    {   1,   29,  131,  148} , // 32 pipes (8 PKRs) 1 bpe @ SW_64K_S3 @ RbPlus
    {   1,   30,  132,  149} , // 32 pipes (8 PKRs) 2 bpe @ SW_64K_S3 @ RbPlus
    {   1,   31,  133,  150} , // 32 pipes (8 PKRs) 4 bpe @ SW_64K_S3 @ RbPlus
    {   1,   32,  134,  151} , // 32 pipes (8 PKRs) 8 bpe @ SW_64K_S3 @ RbPlus
    {   1,   33,  135,  152} , // 32 pipes (8 PKRs) 16 bpe @ SW_64K_S3 @ RbPlus
    {   1,   29,  131,  148} , // 16 pipes (16 PKRs) 1 bpe @ SW_64K_S3 @ RbPlus
    {   1,   30,  132,  149} , // 16 pipes (16 PKRs) 2 bpe @ SW_64K_S3 @ RbPlus
    {   1,   31,  133,  150} , // 16 pipes (16 PKRs) 4 bpe @ SW_64K_S3 @ RbPlus
    {   1,   32,  134,  151} , // 16 pipes (16 PKRs) 8 bpe @ SW_64K_S3 @ RbPlus
    {   1,   33,  135,  152} , // 16 pipes (16 PKRs) 16 bpe @ SW_64K_S3 @ RbPlus
    {   1,   29,  131,  148} , // 32 pipes (16 PKRs) 1 bpe @ SW_64K_S3 @ RbPlus
    {   1,   30,  132,  149} , // 32 pipes (16 PKRs) 2 bpe @ SW_64K_S3 @ RbPlus
    {   1,   31,  133,  150} , // 32 pipes (16 PKRs) 4 bpe @ SW_64K_S3 @ RbPlus
    {   1,   32,  134,  151} , // 32 pipes (16 PKRs) 8 bpe @ SW_64K_S3 @ RbPlus
    {   1,   33,  135,  152} , // 32 pipes (16 PKRs) 16 bpe @ SW_64K_S3 @ RbPlus
    {   1,   29,  131,  148} , // 64 pipes (16 PKRs) 1 bpe @ SW_64K_S3 @ RbPlus
    {   1,   30,  132,  149} , // 64 pipes (16 PKRs) 2 bpe @ SW_64K_S3 @ RbPlus
    {   1,   31,  133,  150} , // 64 pipes (16 PKRs) 4 bpe @ SW_64K_S3 @ RbPlus
    {   1,   32,  134,  151} , // 64 pipes (16 PKRs) 8 bpe @ SW_64K_S3 @ RbPlus
    {   1,   33,  135,  152} , // 64 pipes (16 PKRs) 16 bpe @ SW_64K_S3 @ RbPlus
    {   1,   29,  131,  148} , // 32 pipes (32 PKRs) 1 bpe @ SW_64K_S3 @ RbPlus
    {   1,   30,  132,  149} , // 32 pipes (32 PKRs) 2 bpe @ SW_64K_S3 @ RbPlus
    {   1,   31,  133,  150} , // 32 pipes (32 PKRs) 4 bpe @ SW_64K_S3 @ RbPlus
    {   1,   32,  134,  151} , // 32 pipes (32 PKRs) 8 bpe @ SW_64K_S3 @ RbPlus
    {   1,   33,  135,  152} , // 32 pipes (32 PKRs) 16 bpe @ SW_64K_S3 @ RbPlus
    {   1,   29,  131,  148} , // 64 pipes (32 PKRs) 1 bpe @ SW_64K_S3 @ RbPlus
    {   1,   30,  132,  149} , // 64 pipes (32 PKRs) 2 bpe @ SW_64K_S3 @ RbPlus
    {   1,   31,  133,  150} , // 64 pipes (32 PKRs) 4 bpe @ SW_64K_S3 @ RbPlus
    {   1,   32,  134,  151} , // 64 pipes (32 PKRs) 8 bpe @ SW_64K_S3 @ RbPlus
    {   1,   33,  135,  152} , // 64 pipes (32 PKRs) 16 bpe @ SW_64K_S3 @ RbPlus
};

const ADDR_SW_PATINFO SW_64K_S3_X_RBPLUS_PATINFO[] =
{
    {   1,   29,  131,  148} , // 1 pipes (1 PKRs) 1 bpe @ SW_64K_S3_X @ RbPlus
    {   1,   30,  132,  149} , // 1 pipes (1 PKRs) 2 bpe @ SW_64K_S3_X @ RbPlus
    {   1,   31,  133,  150} , // 1 pipes (1 PKRs) 4 bpe @ SW_64K_S3_X @ RbPlus
    {   1,   32,  134,  151} , // 1 pipes (1 PKRs) 8 bpe @ SW_64K_S3_X @ RbPlus
    {   1,   33,  135,  152} , // 1 pipes (1 PKRs) 16 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   29,  136,  148} , // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   30,  137,  149} , // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   31,  138,  150} , // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   32,  139,  151} , // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   33,  140,  152} , // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   29,  141,  148} , // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   30,  142,  149} , // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   31,  143,  150} , // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   32,  144,  151} , // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   33,  145,  152} , // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   29,  146,  148} , // 8 pipes (2 PKRs) 1 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   30,  147,  149} , // 8 pipes (2 PKRs) 2 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   31,  148,  150} , // 8 pipes (2 PKRs) 4 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   32,  149,  151} , // 8 pipes (2 PKRs) 8 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   33,  150,  152} , // 8 pipes (2 PKRs) 16 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   29,  141,  148} , // 4 pipes (4 PKRs) 1 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   30,  142,  149} , // 4 pipes (4 PKRs) 2 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   31,  143,  150} , // 4 pipes (4 PKRs) 4 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   32,  144,  151} , // 4 pipes (4 PKRs) 8 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   33,  145,  152} , // 4 pipes (4 PKRs) 16 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   29,  146,  148} , // 8 pipes (4 PKRs) 1 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   30,  147,  149} , // 8 pipes (4 PKRs) 2 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   31,  148,  150} , // 8 pipes (4 PKRs) 4 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   32,  149,  151} , // 8 pipes (4 PKRs) 8 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   33,  150,  152} , // 8 pipes (4 PKRs) 16 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   29,  151,  148} , // 16 pipes (4 PKRs) 1 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   30,  152,  149} , // 16 pipes (4 PKRs) 2 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   31,  153,  150} , // 16 pipes (4 PKRs) 4 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   32,  154,  151} , // 16 pipes (4 PKRs) 8 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   33,  155,  152} , // 16 pipes (4 PKRs) 16 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   29,  146,  148} , // 8 pipes (8 PKRs) 1 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   30,  147,  149} , // 8 pipes (8 PKRs) 2 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   31,  148,  150} , // 8 pipes (8 PKRs) 4 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   32,  149,  151} , // 8 pipes (8 PKRs) 8 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   33,  150,  152} , // 8 pipes (8 PKRs) 16 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   29,  151,  148} , // 16 pipes (8 PKRs) 1 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   30,  152,  149} , // 16 pipes (8 PKRs) 2 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   31,  153,  150} , // 16 pipes (8 PKRs) 4 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   32,  154,  151} , // 16 pipes (8 PKRs) 8 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   33,  155,  152} , // 16 pipes (8 PKRs) 16 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   29,  156,  153} , // 32 pipes (8 PKRs) 1 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   30,  157,  154} , // 32 pipes (8 PKRs) 2 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   31,  158,  155} , // 32 pipes (8 PKRs) 4 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   32,  159,  156} , // 32 pipes (8 PKRs) 8 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   33,  160,  157} , // 32 pipes (8 PKRs) 16 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   29,  151,  148} , // 16 pipes (16 PKRs) 1 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   30,  152,  149} , // 16 pipes (16 PKRs) 2 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   31,  153,  150} , // 16 pipes (16 PKRs) 4 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   32,  154,  151} , // 16 pipes (16 PKRs) 8 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   33,  155,  152} , // 16 pipes (16 PKRs) 16 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   29,  156,  153} , // 32 pipes (16 PKRs) 1 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   30,  157,  154} , // 32 pipes (16 PKRs) 2 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   31,  158,  155} , // 32 pipes (16 PKRs) 4 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   32,  159,  156} , // 32 pipes (16 PKRs) 8 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   33,  160,  157} , // 32 pipes (16 PKRs) 16 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   29,  161,  158} , // 64 pipes (16 PKRs) 1 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   30,  162,  159} , // 64 pipes (16 PKRs) 2 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   31,  163,  160} , // 64 pipes (16 PKRs) 4 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   32,  164,  161} , // 64 pipes (16 PKRs) 8 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   33,  165,  162} , // 64 pipes (16 PKRs) 16 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   29,  156,  153} , // 32 pipes (32 PKRs) 1 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   30,  157,  154} , // 32 pipes (32 PKRs) 2 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   31,  158,  155} , // 32 pipes (32 PKRs) 4 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   32,  159,  156} , // 32 pipes (32 PKRs) 8 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   33,  160,  157} , // 32 pipes (32 PKRs) 16 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   29,  161,  158} , // 64 pipes (32 PKRs) 1 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   30,  162,  159} , // 64 pipes (32 PKRs) 2 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   31,  163,  160} , // 64 pipes (32 PKRs) 4 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   32,  164,  161} , // 64 pipes (32 PKRs) 8 bpe @ SW_64K_S3_X @ RbPlus
    {   3,   33,  165,  162} , // 64 pipes (32 PKRs) 16 bpe @ SW_64K_S3_X @ RbPlus
};

const ADDR_SW_PATINFO SW_64K_S3_T_RBPLUS_PATINFO[] =
{
    {   1,   29,  131,  148} , // 1 pipes (1 PKRs) 1 bpe @ SW_64K_S3_T @ RbPlus
    {   1,   30,  132,  149} , // 1 pipes (1 PKRs) 2 bpe @ SW_64K_S3_T @ RbPlus
    {   1,   31,  133,  150} , // 1 pipes (1 PKRs) 4 bpe @ SW_64K_S3_T @ RbPlus
    {   1,   32,  134,  151} , // 1 pipes (1 PKRs) 8 bpe @ SW_64K_S3_T @ RbPlus
    {   1,   33,  135,  152} , // 1 pipes (1 PKRs) 16 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   29,  136,  148} , // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   30,  137,  149} , // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   31,  138,  150} , // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   32,  139,  151} , // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   33,  140,  152} , // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   29,  141,  148} , // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   30,  142,  149} , // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   31,  143,  150} , // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   32,  144,  151} , // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   33,  145,  152} , // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   29,  166,  148} , // 8 pipes (2 PKRs) 1 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   30,  167,  149} , // 8 pipes (2 PKRs) 2 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   31,  168,  150} , // 8 pipes (2 PKRs) 4 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   32,  169,  151} , // 8 pipes (2 PKRs) 8 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   33,  170,  152} , // 8 pipes (2 PKRs) 16 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   29,  141,  148} , // 4 pipes (4 PKRs) 1 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   30,  142,  149} , // 4 pipes (4 PKRs) 2 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   31,  143,  150} , // 4 pipes (4 PKRs) 4 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   32,  144,  151} , // 4 pipes (4 PKRs) 8 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   33,  145,  152} , // 4 pipes (4 PKRs) 16 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   29,  166,  148} , // 8 pipes (4 PKRs) 1 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   30,  167,  149} , // 8 pipes (4 PKRs) 2 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   31,  168,  150} , // 8 pipes (4 PKRs) 4 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   32,  169,  151} , // 8 pipes (4 PKRs) 8 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   33,  170,  152} , // 8 pipes (4 PKRs) 16 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   29,  171,  148} , // 16 pipes (4 PKRs) 1 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   30,  172,  149} , // 16 pipes (4 PKRs) 2 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   31,  173,  150} , // 16 pipes (4 PKRs) 4 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   32,  174,  151} , // 16 pipes (4 PKRs) 8 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   33,  175,  152} , // 16 pipes (4 PKRs) 16 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   29,  166,  148} , // 8 pipes (8 PKRs) 1 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   30,  167,  149} , // 8 pipes (8 PKRs) 2 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   31,  168,  150} , // 8 pipes (8 PKRs) 4 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   32,  169,  151} , // 8 pipes (8 PKRs) 8 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   33,  170,  152} , // 8 pipes (8 PKRs) 16 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   29,  171,  148} , // 16 pipes (8 PKRs) 1 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   30,  172,  149} , // 16 pipes (8 PKRs) 2 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   31,  173,  150} , // 16 pipes (8 PKRs) 4 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   32,  174,  151} , // 16 pipes (8 PKRs) 8 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   33,  175,  152} , // 16 pipes (8 PKRs) 16 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   29,  176,  153} , // 32 pipes (8 PKRs) 1 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   30,  177,  154} , // 32 pipes (8 PKRs) 2 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   31,  178,  155} , // 32 pipes (8 PKRs) 4 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   32,  179,  156} , // 32 pipes (8 PKRs) 8 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   33,  180,  157} , // 32 pipes (8 PKRs) 16 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   29,  171,  148} , // 16 pipes (16 PKRs) 1 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   30,  172,  149} , // 16 pipes (16 PKRs) 2 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   31,  173,  150} , // 16 pipes (16 PKRs) 4 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   32,  174,  151} , // 16 pipes (16 PKRs) 8 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   33,  175,  152} , // 16 pipes (16 PKRs) 16 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   29,  176,  153} , // 32 pipes (16 PKRs) 1 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   30,  177,  154} , // 32 pipes (16 PKRs) 2 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   31,  178,  155} , // 32 pipes (16 PKRs) 4 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   32,  179,  156} , // 32 pipes (16 PKRs) 8 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   33,  180,  157} , // 32 pipes (16 PKRs) 16 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   29,  131,  163} , // 64 pipes (16 PKRs) 1 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   30,  132,  164} , // 64 pipes (16 PKRs) 2 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   31,  133,  165} , // 64 pipes (16 PKRs) 4 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   32,  134,  166} , // 64 pipes (16 PKRs) 8 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   33,  135,  167} , // 64 pipes (16 PKRs) 16 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   29,  176,  153} , // 32 pipes (32 PKRs) 1 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   30,  177,  154} , // 32 pipes (32 PKRs) 2 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   31,  178,  155} , // 32 pipes (32 PKRs) 4 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   32,  179,  156} , // 32 pipes (32 PKRs) 8 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   33,  180,  157} , // 32 pipes (32 PKRs) 16 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   29,  131,  163} , // 64 pipes (32 PKRs) 1 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   30,  132,  164} , // 64 pipes (32 PKRs) 2 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   31,  133,  165} , // 64 pipes (32 PKRs) 4 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   32,  134,  166} , // 64 pipes (32 PKRs) 8 bpe @ SW_64K_S3_T @ RbPlus
    {   3,   33,  135,  167} , // 64 pipes (32 PKRs) 16 bpe @ SW_64K_S3_T @ RbPlus
};

const ADDR_SW_PATINFO SW_64K_D3_X_RBPLUS_PATINFO[] =
{
    {   1,   34,  131,  148} , // 1 pipes (1 PKRs) 1 bpe @ SW_64K_D3_X @ RbPlus
    {   1,   35,  132,  149} , // 1 pipes (1 PKRs) 2 bpe @ SW_64K_D3_X @ RbPlus
    {   1,   36,  133,  150} , // 1 pipes (1 PKRs) 4 bpe @ SW_64K_D3_X @ RbPlus
    {   1,   37,  134,  151} , // 1 pipes (1 PKRs) 8 bpe @ SW_64K_D3_X @ RbPlus
    {   1,   38,  135,  152} , // 1 pipes (1 PKRs) 16 bpe @ SW_64K_D3_X @ RbPlus
    {   2,   34,  417,  170} , // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_D3_X @ RbPlus
    {   2,   35,  417,  521} , // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_D3_X @ RbPlus
    {   2,   36,  418,  522} , // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_D3_X @ RbPlus
    {   2,   37,  419,  152} , // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   38,  420,  152} , // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   34,  421,  523} , // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   35,  421,  524} , // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   36,  422,  525} , // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_D3_X @ RbPlus
    {   4,   37,  423,  526} , // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_D3_X @ RbPlus
    {   4,   38,  424,  526} , // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   34,  425,  523} , // 8 pipes (2 PKRs) 1 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   35,  425,  524} , // 8 pipes (2 PKRs) 2 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   36,  426,  525} , // 8 pipes (2 PKRs) 4 bpe @ SW_64K_D3_X @ RbPlus
    {   4,   37,  427,  526} , // 8 pipes (2 PKRs) 8 bpe @ SW_64K_D3_X @ RbPlus
    {   4,   38,  428,  526} , // 8 pipes (2 PKRs) 16 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   34,  429,  527} , // 4 pipes (4 PKRs) 1 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   35,  430,  528} , // 4 pipes (4 PKRs) 2 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   36,  431,  529} , // 4 pipes (4 PKRs) 4 bpe @ SW_64K_D3_X @ RbPlus
    {   4,   37,  432,  530} , // 4 pipes (4 PKRs) 8 bpe @ SW_64K_D3_X @ RbPlus
    {   4,   38,  433,  531} , // 4 pipes (4 PKRs) 16 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   34,  434,  532} , // 8 pipes (4 PKRs) 1 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   35,  435,  524} , // 8 pipes (4 PKRs) 2 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   36,  436,  525} , // 8 pipes (4 PKRs) 4 bpe @ SW_64K_D3_X @ RbPlus
    {   4,   37,  437,  526} , // 8 pipes (4 PKRs) 8 bpe @ SW_64K_D3_X @ RbPlus
    {   4,   38,  438,  526} , // 8 pipes (4 PKRs) 16 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   34,  439,  533} , // 16 pipes (4 PKRs) 1 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   35,  440,  524} , // 16 pipes (4 PKRs) 2 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   36,  441,  525} , // 16 pipes (4 PKRs) 4 bpe @ SW_64K_D3_X @ RbPlus
    {   4,   37,  442,  526} , // 16 pipes (4 PKRs) 8 bpe @ SW_64K_D3_X @ RbPlus
    {   4,   38,  443,  526} , // 16 pipes (4 PKRs) 16 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   34,  444,  534} , // 8 pipes (8 PKRs) 1 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   35,  444,  535} , // 8 pipes (8 PKRs) 2 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   36,  444,  536} , // 8 pipes (8 PKRs) 4 bpe @ SW_64K_D3_X @ RbPlus
    {   4,   37,  445,  537} , // 8 pipes (8 PKRs) 8 bpe @ SW_64K_D3_X @ RbPlus
    {   4,   38,  446,  537} , // 8 pipes (8 PKRs) 16 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   34,  447,  532} , // 16 pipes (8 PKRs) 1 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   35,  448,  524} , // 16 pipes (8 PKRs) 2 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   36,  449,  525} , // 16 pipes (8 PKRs) 4 bpe @ SW_64K_D3_X @ RbPlus
    {   4,   37,  450,  526} , // 16 pipes (8 PKRs) 8 bpe @ SW_64K_D3_X @ RbPlus
    {   4,   38,  451,  526} , // 16 pipes (8 PKRs) 16 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   34,  447,  538} , // 32 pipes (8 PKRs) 1 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   35,  452,  539} , // 32 pipes (8 PKRs) 2 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   36,  452,  540} , // 32 pipes (8 PKRs) 4 bpe @ SW_64K_D3_X @ RbPlus
    {   4,   37,  453,  541} , // 32 pipes (8 PKRs) 8 bpe @ SW_64K_D3_X @ RbPlus
    {   4,   38,  454,  541} , // 32 pipes (8 PKRs) 16 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   34,  455,  542} , // 16 pipes (16 PKRs) 1 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   35,  456,  543} , // 16 pipes (16 PKRs) 2 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   36,  457,  544} , // 16 pipes (16 PKRs) 4 bpe @ SW_64K_D3_X @ RbPlus
    {   4,   37,  458,  545} , // 16 pipes (16 PKRs) 8 bpe @ SW_64K_D3_X @ RbPlus
    {   4,   38,  459,  545} , // 16 pipes (16 PKRs) 16 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   34,  455,  546} , // 32 pipes (16 PKRs) 1 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   35,  456,  547} , // 32 pipes (16 PKRs) 2 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   36,  457,  548} , // 32 pipes (16 PKRs) 4 bpe @ SW_64K_D3_X @ RbPlus
    {   4,   37,  458,  549} , // 32 pipes (16 PKRs) 8 bpe @ SW_64K_D3_X @ RbPlus
    {   4,   38,  459,  549} , // 32 pipes (16 PKRs) 16 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   34,  455,  550} , // 64 pipes (16 PKRs) 1 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   35,  460,  551} , // 64 pipes (16 PKRs) 2 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   36,  460,  552} , // 64 pipes (16 PKRs) 4 bpe @ SW_64K_D3_X @ RbPlus
    {   4,   37,  461,  553} , // 64 pipes (16 PKRs) 8 bpe @ SW_64K_D3_X @ RbPlus
    {   4,   38,  462,  553} , // 64 pipes (16 PKRs) 16 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   34,  463,  554} , // 32 pipes (32 PKRs) 1 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   35,  464,  555} , // 32 pipes (32 PKRs) 2 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   36,  465,  556} , // 32 pipes (32 PKRs) 4 bpe @ SW_64K_D3_X @ RbPlus
    {   4,   37,  466,  557} , // 32 pipes (32 PKRs) 8 bpe @ SW_64K_D3_X @ RbPlus
    {   4,   38,  467,  557} , // 32 pipes (32 PKRs) 16 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   34,  463,  558} , // 64 pipes (32 PKRs) 1 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   35,  464,  559} , // 64 pipes (32 PKRs) 2 bpe @ SW_64K_D3_X @ RbPlus
    {   3,   36,  465,  560} , // 64 pipes (32 PKRs) 4 bpe @ SW_64K_D3_X @ RbPlus
    {   4,   37,  466,  561} , // 64 pipes (32 PKRs) 8 bpe @ SW_64K_D3_X @ RbPlus
    {   4,   38,  467,  561} , // 64 pipes (32 PKRs) 16 bpe @ SW_64K_D3_X @ RbPlus
};

const UINT_64 GFX10_SW_PATTERN_NIBBLE01[][8] =
{
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            }, // 0
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            }, // 1
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            }, // 2
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            }, // 3
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            }, // 4
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            }, // 5
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            }, // 6
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            }, // 7
    {X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y3,            }, // 8
    {0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            }, // 9
    {0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            }, // 10
    {0,             0,             0,             X0,            Y0,            X1,            Y1,            X2,            }, // 11
    {X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4,            }, // 12
    {S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            }, // 13
    {0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            }, // 14
    {0,             0,             S0,            X0,            Y0,            X1,            Y1,            X2,            }, // 15
    {0,             0,             0,             S0,            X0,            Y0,            X1,            Y1,            }, // 16
    {0,             0,             0,             0,             S0,            X0,            Y0,            X1,            }, // 17
    {S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y2,            }, // 18
    {0,             S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            }, // 19
    {0,             0,             S0,            S1,            X0,            Y0,            X1,            Y1,            }, // 20
    {0,             0,             0,             S0,            S1,            X0,            Y0,            X1,            }, // 21
    {0,             0,             0,             0,             S0,            S1,            X0,            Y0,            }, // 22
    {S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            X2,            }, // 23
    {0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            }, // 24
    {0,             0,             S0,            S1,            S2,            X0,            Y0,            X1,            }, // 25
    {0,             0,             0,             S0,            S1,            S2,            X0,            Y0,            }, // 26
    {0,             0,             0,             0,             S0,            S1,            S2,            X0,            }, // 27
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y4,            }, // 28
    {X0,            X1,            Z0,            Y0,            Z1,            Y1,            X2,            Z2,            }, // 29
    {0,             X0,            Z0,            Y0,            Z1,            Y1,            X1,            Z2,            }, // 30
    {0,             0,             X0,            Y0,            Z0,            Y1,            X1,            Z1,            }, // 31
    {0,             0,             0,             X0,            Z0,            Y0,            X1,            Z1,            }, // 32
    {0,             0,             0,             0,             Z0,            Y0,            X0,            Z1,            }, // 33
    {X0,            X1,            Z0,            Y0,            Y1,            Z1,            X2,            Z2,            }, // 34
    {0,             X0,            Z0,            Y0,            X1,            Z1,            Y1,            Z2,            }, // 35
    {0,             0,             X0,            Y0,            X1,            Z0,            Y1,            Z1,            }, // 36
    {0,             0,             0,             X0,            Y0,            Z0,            X1,            Z1,            }, // 37
    {0,             0,             0,             0,             X0,            Z0,            Y0,            Z1,            }, // 38
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            }, // 39
};

const UINT_64 GFX10_SW_PATTERN_NIBBLE2[][4] =
{
    {}, // 0
    {Y4,            X4,            Y5,            X5,            }, // 1
    {Y3,            X4,            Y4,            X5,            }, // 2
    {Y3,            X3,            Y4,            X4,            }, // 3
    {Y2,            X3,            Y3,            X4,            }, // 4
    {Y2,            X2,            Y3,            X3,            }, // 5
    {Z0^X4^Y4,      X4,            Y5,            X5,            }, // 6
    {Z0^Y3^X4,      X4,            Y4,            X5,            }, // 7
    {Z0^X3^Y3,      X3,            Y4,            X4,            }, // 8
    {Z0^Y2^X3,      X3,            Y3,            X4,            }, // 9
    {Z0^X2^Y2,      X2,            Y3,            X3,            }, // 10
    {Z1^Y4^X5,      Z0^X4^Y5,      Y5,            X5,            }, // 11
    {Z1^Y3^X5,      Z0^X4^Y4,      Y4,            X5,            }, // 12
    {Z1^Y3^X4,      Z0^X3^Y4,      Y4,            X4,            }, // 13
    {Z1^Y2^X4,      Z0^X3^Y3,      Y3,            X4,            }, // 14
    {Z1^Y2^X3,      Z0^X2^Y3,      Y3,            X3,            }, // 15
    {Z2^Y4^X6,      Z1^X4^Y6,      Z0^X5^Y5,      X5,            }, // 16
    {Z2^Y3^X6,      Z1^X4^Y5,      Z0^Y4^X5,      X5,            }, // 17
    {Z2^Y3^X5,      Z1^X3^Y5,      Z0^X4^Y4,      X4,            }, // 18
    {Y2^Z2^X5,      Z1^X3^Y4,      Z0^Y3^X4,      X4,            }, // 19
    {Y2^Z2^X4,      Z1^X2^Y4,      Z0^X3^Y3,      X3,            }, // 20
    {Z3^Y4^X7,      Z2^X4^Y7,      Z1^Y5^X6,      Z0^X5^Y6,      }, // 21
    {Y3^Z3^X7,      Z2^X4^Y6,      Z1^Y4^X6,      Z0^X5^Y5,      }, // 22
    {Y3^Z3^X6,      Z2^X3^Y6,      Z1^Y4^X5,      Z0^X4^Y5,      }, // 23
    {Y2^Z3^X6,      Z2^X3^Y5,      Z1^Y3^X5,      Z0^X4^Y4,      }, // 24
    {Y2^Z3^X5,      X2^Z2^Y5,      Z1^Y3^X4,      Z0^X3^Y4,      }, // 25
    {Y4^Z4^X8,      Z3^X4^Y8,      Z2^Y5^X7,      Z1^X5^Y7,      }, // 26
    {Y3^Z4^X8,      Z3^X4^Y7,      Z2^Y4^X7,      Z1^X5^Y6,      }, // 27
    {Y3^Z4^X7,      X3^Z3^Y7,      Z2^Y4^X6,      Z1^X4^Y6,      }, // 28
    {Y2^Z4^X7,      X3^Z3^Y6,      Z2^Y3^X6,      Z1^X4^Y5,      }, // 29
    {Y2^Z4^X6,      X2^Z3^Y6,      Z2^Y3^X5,      Z1^X3^Y5,      }, // 30
    {Y4^Z5^X9,      X4^Z4^Y9,      Z3^Y5^X8,      Z2^X5^Y8,      }, // 31
    {Y3^Z5^X9,      X4^Z4^Y8,      Z3^Y4^X8,      Z2^X5^Y7,      }, // 32
    {Y3^Z5^X8,      X3^Z4^Y8,      Z3^Y4^X7,      Z2^X4^Y7,      }, // 33
    {Y2^Z5^X8,      X3^Z4^Y7,      Y3^Z3^X7,      Z2^X4^Y6,      }, // 34
    {Y2^Z5^X7,      X2^Z4^Y7,      Y3^Z3^X6,      Z2^X3^Y6,      }, // 35
    {X4^Y4,         X4,            Y5,            X5,            }, // 36
    {Y3^X4,         X4,            Y4,            X5,            }, // 37
    {X3^Y3,         X3,            Y4,            X4,            }, // 38
    {Y2^X3,         X3,            Y3,            X4,            }, // 39
    {X2^Y2,         X2,            Y3,            X3,            }, // 40
    {Y4^X5,         X4^Y5,         Y5,            X5,            }, // 41
    {Y3^X5,         X4^Y4,         Y4,            X5,            }, // 42
    {Y3^X4,         X3^Y4,         Y4,            X4,            }, // 43
    {Y2^X4,         X3^Y3,         Y3,            X4,            }, // 44
    {Y2^X3,         X2^Y3,         Y3,            X3,            }, // 45
    {Y4^X6,         X4^Y6,         X5^Y5,         X5,            }, // 46
    {Y3^X6,         X4^Y5,         Y4^X5,         X5,            }, // 47
    {Y3^X5,         X3^Y5,         X4^Y4,         X4,            }, // 48
    {Y2^X5,         X3^Y4,         Y3^X4,         X4,            }, // 49
    {Y2^X4,         X2^Y4,         X3^Y3,         X3,            }, // 50
    {Y4^X7,         X4^Y7,         Y5^X6,         X5^Y6,         }, // 51
    {Y3^X7,         X4^Y6,         Y4^X6,         X5^Y5,         }, // 52
    {Y3^X6,         X3^Y6,         Y4^X5,         X4^Y5,         }, // 53
    {Y2^X6,         X3^Y5,         Y3^X5,         X4^Y4,         }, // 54
    {Y2^X5,         X2^Y5,         Y3^X4,         X3^Y4,         }, // 55
    {Y4,            X4,            Y5^X7,         X5^Y7,         }, // 56
    {Y3,            X4,            Y4^X7,         X5^Y6,         }, // 57
    {Y3,            X3,            Y4^X6,         X4^Y6,         }, // 58
    {Y2,            X3,            Y3^X6,         X4^Y5,         }, // 59
    {Y2,            X2,            Y3^X5,         X3^Y5,         }, // 60
    {Z0^X3^Y3,      X4,            Y5,            X5,            }, // 61
    {Z0^X3^Y3,      X4,            Y4,            X5,            }, // 62
    {Z0^X3^Y3,      X3,            Y2,            X4,            }, // 63
    {Z0^X3^Y3,      X2,            Y2,            X3,            }, // 64
    {Z1^X3^Y3,      Z0^X4^Y4,      Y5,            X5,            }, // 65
    {Z1^X3^Y3,      Z0^X4^Y4,      Y4,            X5,            }, // 66
    {Z1^X3^Y3,      Z0^X4^Y4,      Y3,            X4,            }, // 67
    {Z1^X3^Y3,      Z0^X4^Y4,      Y2,            X3,            }, // 68
    {Z1^X3^Y3,      Z0^X4^Y4,      Y2,            X2,            }, // 69
    {Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X5,            }, // 70
    {Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X4,            }, // 71
    {Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            }, // 72
    {Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X2,            }, // 73
    {X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      }, // 74
    {X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      }, // 75
    {X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      }, // 76
    {X3^Y3^Z5,      X4^Y4^Z4,      Z3^Y5^X8,      Z2^X5^Y8,      }, // 77
    {X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X8,      Z1^X5^Y8,      }, // 78
    {X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X8,      Z0^X5^Y8,      }, // 79
    {Y3,            Y4,            X4,            Y5,            }, // 80
    {X2,            Y3,            X3,            Y4,            }, // 81
    {Z0^X3^Y3,      Y4,            X4,            Y5,            }, // 82
    {Z0^X3^Y3,      X2,            X3,            Y4,            }, // 83
    {Z1^X3^Y3,      Z0^X4^Y4,      Y4,            Y5,            }, // 84
    {Z1^X3^Y3,      Z0^X4^Y4,      X2,            Y3,            }, // 85
    {Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      Y4,            }, // 86
    {Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X6,      Y2^X5^Y6,      }, // 87
    {Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X7,      Y2^X5^Y7,      }, // 88
    {Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X8,      Y2^X5^Y8,      }, // 89
    {X3,            Y3,            X4,            Y4,            }, // 90
    {Z0^X3^Y3,      X3,            X4,            Y4,            }, // 91
    {Z1^X3^Y3,      Z0^X4^Y4,      X3,            Y4,            }, // 92
    {Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      Y2,            }, // 93
    {Z1^X3^Y3,      Z0^X4^Y4,      Y2^X5^Y5,      X2,            }, // 94
    {Z2^X3^Y3,      Z1^X4^Y4,      Y2^Y5^X6,      Z0^X5^Y6,      }, // 95
    {Z1^X3^Y3,      Z0^X4^Y4,      Y2^Y5^X6,      X1^X5^Y6,      }, // 96
    {Z2^X3^Y3,      Z1^X4^Y4,      Y2^Y5^X7,      Z0^X5^Y7,      }, // 97
    {Z1^X3^Y3,      Z0^X4^Y4,      Y2^Y5^X7,      X1^X5^Y7,      }, // 98
    {Z2^X3^Y3,      Z1^X4^Y4,      Y2^Y5^X8,      Z0^X5^Y8,      }, // 99
    {Z1^X3^Y3,      Z0^X4^Y4,      Y2^Y5^X8,      X1^X5^Y8,      }, // 100
    {Z0^X3^Y3,      Y2,            X3,            Y4,            }, // 101
    {Z1^X3^Y3,      Z0^X4^Y4,      X2,            Y2,            }, // 102
    {Z1^X3^Y3,      Z0^X4^Y4,      Y2^X5^Y5,      Y3,            }, // 103
    {Z1^X3^Y3,      Z0^X4^Y4,      Y0^X5^Y5,      Y2,            }, // 104
    {Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X6,      Z3^X5^Y6,      }, // 105
    {Z1^X3^Y3,      Z0^X4^Y4,      Y0^Y5^X6,      X1^X5^Y6,      }, // 106
    {Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X7,      Z4^X5^Y7,      }, // 107
    {Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X7,      Z3^X5^Y7,      }, // 108
    {Z1^X3^Y3,      Z0^X4^Y4,      Y0^Y5^X7,      X1^X5^Y7,      }, // 109
    {Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X8,      Z4^X5^Y8,      }, // 110
    {Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X8,      Z3^X5^Y8,      }, // 111
    {Z1^X3^Y3,      Z0^X4^Y4,      Y0^Y5^X8,      X1^X5^Y8,      }, // 112
    {Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X6,      S0^X5^Y6,      }, // 113
    {Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X7,      S0^X5^Y7,      }, // 114
    {Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X8,      S0^X5^Y8,      }, // 115
    {Z1^X3^Y3,      Z0^X4^Y4,      S1^X5^Y5,      X2,            }, // 116
    {Z2^X3^Y3,      Z1^X4^Y4,      S1^Y5^X6,      Z0^X5^Y6,      }, // 117
    {Z1^X3^Y3,      Z0^X4^Y4,      S1^Y5^X6,      S0^X5^Y6,      }, // 118
    {Z2^X3^Y3,      Z1^X4^Y4,      S1^Y5^X7,      Z0^X5^Y7,      }, // 119
    {Z1^X3^Y3,      Z0^X4^Y4,      S1^Y5^X7,      S0^X5^Y7,      }, // 120
    {Z2^X3^Y3,      Z1^X4^Y4,      S1^Y5^X8,      Z0^X5^Y8,      }, // 121
    {Z1^X3^Y3,      Z0^X4^Y4,      S1^Y5^X8,      S0^X5^Y8,      }, // 122
    {Z1^X3^Y3,      Z0^X4^Y4,      S2^X5^Y5,      Y2,            }, // 123
    {Z1^X3^Y3,      Z0^X4^Y4,      S2^X5^Y5,      X2,            }, // 124
    {Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X6,      S2^X5^Y6,      }, // 125
    {Z1^X3^Y3,      Z0^X4^Y4,      S2^Y5^X6,      S1^X5^Y6,      }, // 126
    {Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X7,      S2^X5^Y7,      }, // 127
    {Z1^X3^Y3,      Z0^X4^Y4,      S2^Y5^X7,      S1^X5^Y7,      }, // 128
    {Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X8,      S2^X5^Y8,      }, // 129
    {Z1^X3^Y3,      Z0^X4^Y4,      S2^Y5^X8,      S1^X5^Y8,      }, // 130
    {Y2,            X3,            Z3,            Y3,            }, // 131
    {Y2,            X2,            Z3,            Y3,            }, // 132
    {Y2,            X2,            Z2,            Y3,            }, // 133
    {Y1,            X2,            Z2,            Y2,            }, // 134
    {Y1,            X1,            Z2,            Y2,            }, // 135
    {Y2^X3^Z3,      X3,            Z3,            Y3,            }, // 136
    {X2^Y2^Z3,      X2,            Z3,            Y3,            }, // 137
    {X2^Y2^Z2,      X2,            Z2,            Y3,            }, // 138
    {Y1^X2^Z2,      X2,            Z2,            Y2,            }, // 139
    {X1^Y1^Z2,      X1,            Z2,            Y2,            }, // 140
    {Y2^X4^Z4,      X3^Y3^Z3,      Z3,            Y3,            }, // 141
    {Y2^X3^Z4,      X2^Y3^Z3,      Z3,            Y3,            }, // 142
    {Y2^X3^Z3,      X2^Z2^Y3,      Z2,            Y3,            }, // 143
    {Y1^X3^Z3,      X2^Y2^Z2,      Z2,            Y2,            }, // 144
    {Y1^X2^Z3,      X1^Y2^Z2,      Z2,            Y2,            }, // 145
    {Y2^X5^Z5,      X3^Y4^Z4,      Y3^Z3^X4,      Y3,            }, // 146
    {Y2^X4^Z5,      X2^Y4^Z4,      X3^Y3^Z3,      Y3,            }, // 147
    {Y2^X4^Z4,      X2^Z3^Y4,      Z2^X3^Y3,      Y3,            }, // 148
    {Y1^X4^Z4,      X2^Y3^Z3,      Y2^Z2^X3,      Y2,            }, // 149
    {Y1^X3^Z4,      X1^Y3^Z3,      X2^Y2^Z2,      Y2,            }, // 150
    {Y2^X6^Z6,      X3^Y5^Z5,      Z3^Y4^X5,      Y3^X4^Z4,      }, // 151
    {Y2^X5^Z6,      X2^Y5^Z5,      Z3^X4^Y4,      X3^Y3^Z4,      }, // 152
    {Y2^X5^Z5,      X2^Z4^Y5,      Z2^X4^Y4,      X3^Y3^Z3,      }, // 153
    {Y1^X5^Z5,      X2^Y4^Z4,      Z2^Y3^X4,      Y2^X3^Z3,      }, // 154
    {Y1^X4^Z5,      X1^Y4^Z4,      Z2^X3^Y3,      X2^Y2^Z3,      }, // 155
    {Y2^X7^Z7,      X3^Y6^Z6,      Z3^Y5^X6,      Y3^X5^Z5,      }, // 156
    {Y2^X6^Z7,      X2^Y6^Z6,      Z3^X5^Y5,      Y3^X4^Z5,      }, // 157
    {Y2^X6^Z6,      X2^Z5^Y6,      Z2^X5^Y5,      Y3^X4^Z4,      }, // 158
    {Y1^X6^Z6,      X2^Y5^Z5,      Z2^Y4^X5,      Y2^X4^Z4,      }, // 159
    {Y1^X5^Z6,      X1^Y5^Z5,      Z2^X4^Y4,      Y2^X3^Z4,      }, // 160
    {Y2^X8^Z8,      X3^Y7^Z7,      Z3^Y6^X7,      Y3^X6^Z6,      }, // 161
    {Y2^X7^Z8,      X2^Y7^Z7,      Z3^X6^Y6,      Y3^X5^Z6,      }, // 162
    {Y2^X7^Z7,      X2^Z6^Y7,      Z2^X6^Y6,      Y3^X5^Z5,      }, // 163
    {Y1^X7^Z7,      X2^Y6^Z6,      Z2^Y5^X6,      Y2^X5^Z5,      }, // 164
    {Y1^X6^Z7,      X1^Y6^Z6,      Z2^X5^Y5,      Y2^X4^Z5,      }, // 165
    {Y2^X5,         X3^Y4^Z4,      Y3^Z3^X4,      Y3,            }, // 166
    {Y2^X4,         X2^Y4^Z4,      X3^Y3^Z3,      Y3,            }, // 167
    {Y2^X4,         X2^Z3^Y4,      Z2^X3^Y3,      Y3,            }, // 168
    {Y1^X4,         X2^Y3^Z3,      Y2^Z2^X3,      Y2,            }, // 169
    {Y1^X3,         X1^Y3^Z3,      X2^Y2^Z2,      Y2,            }, // 170
    {Y2,            X3,            Z3^Y4^X5,      Y3^X4^Z4,      }, // 171
    {Y2,            X2,            Z3^X4^Y4,      X3^Y3^Z4,      }, // 172
    {Y2,            X2,            Z2^X4^Y4,      X3^Y3^Z3,      }, // 173
    {Y1,            X2,            Z2^Y3^X4,      Y2^X3^Z3,      }, // 174
    {Y1,            X1,            Z2^X3^Y3,      X2^Y2^Z3,      }, // 175
    {Y2,            X3,            Z3,            Y3^X5,         }, // 176
    {Y2,            X2,            Z3,            Y3^X4,         }, // 177
    {Y2,            X2,            Z2,            Y3^X4,         }, // 178
    {Y1,            X2,            Z2,            Y2^X4,         }, // 179
    {Y1,            X1,            Z2,            Y2^X3,         }, // 180
    {X3^Y3,         X3,            Z3,            Y2,            }, // 181
    {X3^Y3,         X2,            Z3,            Y2,            }, // 182
    {X3^Y3,         X2,            Z2,            Y2,            }, // 183
    {X3^Y3,         X2,            Z2,            Y1,            }, // 184
    {X3^Y3,         X1,            Z2,            Y1,            }, // 185
    {X3^Y3,         X4^Y4,         Z3,            Y2,            }, // 186
    {X3^Y3,         X4^Y4,         Z2,            Y2,            }, // 187
    {X3^Y3,         X4^Y4,         Z2,            Y1,            }, // 188
    {X3^Y3,         X1^X4^Y4,      Z2,            Y1,            }, // 189
    {X3^Y3,         X4^Y4,         X5^Y5,         Z3,            }, // 190
    {X3^Y3,         X4^Y4,         Z3^X5^Y5,      Y2,            }, // 191
    {X3^Y3,         X4^Y4,         Z2^X5^Y5,      Y2,            }, // 192
    {X3^Y3,         X4^Y4,         Z2^X5^Y5,      Y1,            }, // 193
    {X3^Y3,         X1^X4^Y4,      Z2^X5^Y5,      Y1,            }, // 194
    {X3^Y3,         X4^Y4,         Y2^Y5^X6,      X5^Y6,         }, // 195
    {X3^Y3,         X4^Y4,         Z3^Y5^X6,      Y2^X5^Y6,      }, // 196
    {X3^Y3,         X4^Y4,         Z2^Y5^X6,      Y2^X5^Y6,      }, // 197
    {X3^Y3,         X4^Y4,         Z2^Y5^X6,      Y1^X5^Y6,      }, // 198
    {X3^Y3,         X1^X4^Y4,      Z2^Y5^X6,      Y1^X5^Y6,      }, // 199
    {X3^Y3,         X4^Y4,         Y2^Y5^X7,      X5^Y7,         }, // 200
    {X3^Y3,         X4^Y4,         Z3^Y5^X7,      Y2^X5^Y7,      }, // 201
    {X3^Y3,         X4^Y4,         Z2^Y5^X7,      Y2^X5^Y7,      }, // 202
    {X3^Y3,         X4^Y4,         Z2^Y5^X7,      Y1^X5^Y7,      }, // 203
    {X3^Y3,         X1^X4^Y4,      Z2^Y5^X7,      Y1^X5^Y7,      }, // 204
    {X3^Y3,         X4^Y4,         Y2^Y5^X8,      X5^Y8,         }, // 205
    {X3^Y3,         X4^Y4,         Z3^Y5^X8,      Y2^X5^Y8,      }, // 206
    {X3^Y3,         X4^Y4,         Z2^Y5^X8,      Y2^X5^Y8,      }, // 207
    {X3^Y3,         X4^Y4,         Z2^Y5^X8,      Y1^X5^Y8,      }, // 208
    {X3^Y3,         X1^X4^Y4,      Z2^Y5^X8,      Y1^X5^Y8,      }, // 209
    {Y4^X5,         Z0^X4^Y5,      Y5,            X5,            }, // 210
    {Y3^X5,         Z0^X4^Y4,      Y4,            X5,            }, // 211
    {Y3^X4,         Z0^X3^Y4,      Y4,            X4,            }, // 212
    {Y2^X4,         Z0^X3^Y3,      Y3,            X4,            }, // 213
    {Y2^X3,         Z0^X2^Y3,      Y3,            X3,            }, // 214
    {Y4^X6,         X4^Y6,         Z0^X5^Y5,      X5,            }, // 215
    {Y3^X6,         X4^Y5,         Z0^Y4^X5,      X5,            }, // 216
    {Y3^X5,         X3^Y5,         Z0^X4^Y4,      X4,            }, // 217
    {Y2^X5,         X3^Y4,         Z0^Y3^X4,      X4,            }, // 218
    {Y2^X4,         X2^Y4,         Z0^X3^Y3,      X3,            }, // 219
    {Y4^X6,         Z1^X4^Y6,      Z0^X5^Y5,      X5,            }, // 220
    {Y3^X6,         Z1^X4^Y5,      Z0^Y4^X5,      X5,            }, // 221
    {Y3^X5,         Z1^X3^Y5,      Z0^X4^Y4,      X4,            }, // 222
    {Y2^X5,         Z1^X3^Y4,      Z0^Y3^X4,      X4,            }, // 223
    {Y2^X4,         Z1^X2^Y4,      Z0^X3^Y3,      X3,            }, // 224
    {Y4^X7,         X4^Y7,         Z1^Y5^X6,      Z0^X5^Y6,      }, // 225
    {Y3^X7,         X4^Y6,         Z1^Y4^X6,      Z0^X5^Y5,      }, // 226
    {Y3^X6,         X3^Y6,         Z1^Y4^X5,      Z0^X4^Y5,      }, // 227
    {Y2^X6,         X3^Y5,         Z1^Y3^X5,      Z0^X4^Y4,      }, // 228
    {Y2^X5,         X2^Y5,         Z1^Y3^X4,      Z0^X3^Y4,      }, // 229
    {Y4^X7,         Z2^X4^Y7,      Z1^Y5^X6,      Z0^X5^Y6,      }, // 230
    {Y3^X7,         Z2^X4^Y6,      Z1^Y4^X6,      Z0^X5^Y5,      }, // 231
    {Y3^X6,         Z2^X3^Y6,      Z1^Y4^X5,      Z0^X4^Y5,      }, // 232
    {Y2^X6,         Z2^X3^Y5,      Z1^Y3^X5,      Z0^X4^Y4,      }, // 233
    {Y2^X5,         X2^Z2^Y5,      Z1^Y3^X4,      Z0^X3^Y4,      }, // 234
    {Y4^X7,         X4^Y7,         Z2^Y5^X6,      Z1^X5^Y6,      }, // 235
    {Y3^X7,         X4^Y6,         Z2^Y4^X6,      Z1^X5^Y5,      }, // 236
    {Y3^X6,         X3^Y6,         Z2^Y4^X5,      Z1^X4^Y5,      }, // 237
    {Y2^X6,         X3^Y5,         Z2^Y3^X5,      Z1^X4^Y4,      }, // 238
    {Y2^X5,         X2^Y5,         Z2^Y3^X4,      Z1^X3^Y4,      }, // 239
    {Y4^X7,         Z3^X4^Y7,      Z2^Y5^X6,      Z1^X5^Y6,      }, // 240
    {Y3^X7,         Z3^X4^Y6,      Z2^Y4^X6,      Z1^X5^Y5,      }, // 241
    {Y3^X6,         X3^Z3^Y6,      Z2^Y4^X5,      Z1^X4^Y5,      }, // 242
    {Y2^X6,         X3^Z3^Y5,      Z2^Y3^X5,      Z1^X4^Y4,      }, // 243
    {Y2^X5,         X2^Z3^Y5,      Z2^Y3^X4,      Z1^X3^Y4,      }, // 244
    {Y4^X7,         X4^Y7,         Z3^Y5^X6,      Z2^X5^Y6,      }, // 245
    {Y3^X7,         X4^Y6,         Z3^Y4^X6,      Z2^X5^Y5,      }, // 246
    {Y3^X6,         X3^Y6,         Z3^Y4^X5,      Z2^X4^Y5,      }, // 247
    {Y2^X6,         X3^Y5,         Y3^Z3^X5,      Z2^X4^Y4,      }, // 248
    {Y2^X5,         X2^Y5,         Y3^Z3^X4,      Z2^X3^Y4,      }, // 249
    {Y4^X8,         X4^Y8,         Z2^Y5^X7,      Z1^X5^Y7,      }, // 250
    {Y3^X8,         X4^Y7,         Z2^Y4^X7,      Z1^X5^Y6,      }, // 251
    {Y3^X7,         X3^Y7,         Z2^Y4^X6,      Z1^X4^Y6,      }, // 252
    {Y2^X7,         X3^Y6,         Z2^Y3^X6,      Z1^X4^Y5,      }, // 253
    {Y2^X6,         X2^Y6,         Z2^Y3^X5,      Z1^X3^Y5,      }, // 254
    {Y4^X8,         Z3^X4^Y8,      Z2^Y5^X7,      Z1^X5^Y7,      }, // 255
    {Y3^X8,         Z3^X4^Y7,      Z2^Y4^X7,      Z1^X5^Y6,      }, // 256
    {Y3^X7,         X3^Z3^Y7,      Z2^Y4^X6,      Z1^X4^Y6,      }, // 257
    {Y2^X7,         X3^Z3^Y6,      Z2^Y3^X6,      Z1^X4^Y5,      }, // 258
    {Y2^X6,         X2^Z3^Y6,      Z2^Y3^X5,      Z1^X3^Y5,      }, // 259
    {Y4^X9,         X4^Y9,         Z3^Y5^X8,      Z2^X5^Y8,      }, // 260
    {Y3^X9,         X4^Y8,         Z3^Y4^X8,      Z2^X5^Y7,      }, // 261
    {Y3^X8,         X3^Y8,         Z3^Y4^X7,      Z2^X4^Y7,      }, // 262
    {Y2^X8,         X3^Y7,         Y3^Z3^X7,      Z2^X4^Y6,      }, // 263
    {Y2^X7,         X2^Y7,         Y3^Z3^X6,      Z2^X3^Y6,      }, // 264
    {Y4^X9,         X4^Z4^Y9,      Z3^Y5^X8,      Z2^X5^Y8,      }, // 265
    {Y3^X9,         X4^Z4^Y8,      Z3^Y4^X8,      Z2^X5^Y7,      }, // 266
    {Y3^X8,         X3^Z4^Y8,      Z3^Y4^X7,      Z2^X4^Y7,      }, // 267
    {Y2^X8,         X3^Z4^Y7,      Y3^Z3^X7,      Z2^X4^Y6,      }, // 268
    {Y2^X7,         X2^Z4^Y7,      Y3^Z3^X6,      Z2^X3^Y6,      }, // 269
    {X4,            Y4,            X5^Y10,        Y5^X10,        }, // 270
    {Y3,            X4,            Y4^X10,        X5^Y9,         }, // 271
    {X3,            Y3,            X4^Y9,         Y4^X9,         }, // 272
    {Y2,            X3,            Y3^X9,         X4^Y8,         }, // 273
    {X2,            Y2,            X3^Y8,         Y3^X8,         }, // 274
    {Z0^X4^Y4,      Y4,            X5,            Y5^X10,        }, // 275
    {Z0^X4^Y4,      Y3,            Y4,            X5^Y9,         }, // 276
    {Z0^X4^Y4,      X3,            Y3,            Y4^X9,         }, // 277
    {Z0^X4^Y4,      Y2,            X3,            Y3^X9,         }, // 278
    {Z0^X4^Y4,      X2,            Y2,            Y3^X8,         }, // 279
    {Y4^X5^Y5,      Z0^X4^Y4,      X5,            Y5,            }, // 280
    {Y4^X5^Y5,      Z0^X4^Y4,      Y3,            X5,            }, // 281
    {Y4^X5^Y5,      Z0^X4^Y4,      X3,            Y3,            }, // 282
    {Y4^X5^Y5,      Z0^X4^Y4,      Y2,            X3,            }, // 283
    {Y4^X5^Y5,      Z0^X4^Y4,      X2,            Y2,            }, // 284
    {Y4^X5^Y5,      Z0^X4^Y4,      X5^Y5,         Y5,            }, // 285
    {Y4^X5^Y5,      Z0^X4^Y4,      X5^Y5,         Y3,            }, // 286
    {Y4^X5^Y5,      Z0^X4^Y4,      X5^Y5,         X3,            }, // 287
    {Y4^X5^Y5,      Z0^X4^Y4,      X5^Y5,         Y2,            }, // 288
    {Y4^X5^Y5,      Z0^X4^Y4,      X5^Y5,         X2,            }, // 289
    {Y4^X6^Y6,      Z1^X4^Y4,      X5,            X6,            }, // 290
    {Y4^X6^Y6,      Z1^X4^Y4,      Y3,            X5,            }, // 291
    {Y4^X6^Y6,      Z1^X4^Y4,      X3,            Y3,            }, // 292
    {Y4^X6^Y6,      Z1^X4^Y4,      Y2,            X3,            }, // 293
    {Y4^X6^Y6,      Z1^X4^Y4,      X2,            Y2,            }, // 294
    {Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5,            }, // 295
    {Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      Y3,            }, // 296
    {Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            }, // 297
    {Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      Y2,            }, // 298
    {Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X2,            }, // 299
    {Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^Y6,         }, // 300
    {Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X6,            }, // 301
    {Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      Y3,            }, // 302
    {Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X3,            }, // 303
    {Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Y2,            }, // 304
    {Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      X2,            }, // 305
    {Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         }, // 306
    {Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      }, // 307
    {Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         }, // 308
    {Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      }, // 309
    {Y4^X8^Y8,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      }, // 310
    {Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      X5^Y8,         }, // 311
    {Y4^X9^Y9,      Z3^X4^Y4,      Z2^Y5^X8,      Z1^X5^Y8,      }, // 312
    {Y4^X9^Y9,      Z2^X4^Y4,      Z1^Y5^X8,      Z0^X5^Y8,      }, // 313
    {Y3,            X4,            Y4^X10,        Y5^X9,         }, // 314
    {X3,            Y3,            Y4^X9,         X4^Y9,         }, // 315
    {X2,            Y2,            Y3^X8,         X3^Y8,         }, // 316
    {Z0^X4^Y4,      Y3,            Y4,            Y5^X9,         }, // 317
    {Z0^X4^Y4,      X2,            X3,            Y3^X9,         }, // 318
    {Y4^X5^Y5,      Z0^X4^Y4,      X2,            X3,            }, // 319
    {Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X2^X5^Y6,      }, // 320
    {Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      Y1^X5^Y6,      }, // 321
    {Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X2,            }, // 322
    {Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      Y1^X5^Y6,      }, // 323
    {Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      Y1^X5^Y7,      }, // 324
    {Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      Y1^X5^Y8,      }, // 325
    {Z0^X4^Y4,      X3,            Y3,            X5^Y8,         }, // 326
    {Y4^X5^Y5,      Z0^X4^Y4,      Y1^X5^Y5,      X2,            }, // 327
    {Y4^X6^Y6,      Z1^X4^Y4,      X2,            X3,            }, // 328
    {Y4^X6^Y6,      Z0^X4^Y4,      X2,            X3,            }, // 329
    {Y4^X6^Y6,      Z0^X4^Y4,      Y1^X5^Y5,      X2,            }, // 330
    {Y4^X6^Y6,      Z0^X4^Y4,      Y1^X5^Y5,      X1^X5^Y6,      }, // 331
    {Y4^X7^Y7,      Z1^X4^Y4,      Y1^Y5^X6,      X3,            }, // 332
    {Y4^X7^Y7,      Z0^X4^Y4,      Y1^Y5^X6,      X3,            }, // 333
    {Y4^X7^Y7,      Z1^X4^Y4,      Y1^Y5^X6,      Z0^X5^Y6,      }, // 334
    {Y4^X7^Y7,      Z0^X4^Y4,      Y1^Y5^X6,      X1^X5^Y6,      }, // 335
    {Y4^X8^Y8,      Z1^X4^Y4,      Y1^Y5^X7,      Z0^X5^Y7,      }, // 336
    {Y4^X8^Y8,      Z0^X4^Y4,      Y1^Y5^X7,      X1^X5^Y7,      }, // 337
    {Y4^X9^Y9,      Z1^X4^Y4,      Y1^Y5^X8,      Z0^X5^Y8,      }, // 338
    {Y4^X9^Y9,      Z0^X4^Y4,      Y1^Y5^X8,      X1^X5^Y8,      }, // 339
    {Z0^X4^Y4,      X2,            Y2,            X3^Y7,         }, // 340
    {Y4^X5^Y5,      Y0^X4^Y4,      X2,            X3,            }, // 341
    {Y4^X5^Y5,      Z0^X4^Y4,      Y2^X5^Y5,      X2,            }, // 342
    {Y4^X5^Y5,      Y0^X4^Y4,      X1^X5^Y5,      X2,            }, // 343
    {Y4^X6^Y6,      Z0^X4^Y4,      X3,            Y3,            }, // 344
    {Y4^X6^Y6,      Y0^X4^Y4,      X3,            Y3,            }, // 345
    {Y4^X6^Y6,      Z0^X4^Y4,      Y1^X5^Y5,      X3,            }, // 346
    {Y4^X6^Y6,      Y0^X4^Y4,      Y1^X5^Y5,      X3,            }, // 347
    {Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      Y2^X5^Y6,      }, // 348
    {Y4^X6^Y6,      Z0^X4^Y4,      Y1^X5^Y5,      X2^X5^Y6,      }, // 349
    {Y4^X6^Y6,      Y0^X4^Y4,      Y1^X5^Y5,      Y2^X5^Y6,      }, // 350
    {Y4^X7^Y7,      Y0^X4^Y4,      Y1^Y5^X6,      X3,            }, // 351
    {Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      Y2^X5^Y6,      }, // 352
    {Y4^X7^Y7,      Y0^X4^Y4,      Y1^Y5^X6,      X1^X5^Y6,      }, // 353
    {Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      Y2^X5^Y7,      }, // 354
    {Y4^X8^Y8,      Y0^X4^Y4,      Y1^Y5^X7,      X1^X5^Y7,      }, // 355
    {Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X2^X5^Y7,      }, // 356
    {Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      X2^X5^Y8,      }, // 357
    {Y4^X9^Y9,      Y0^X4^Y4,      Y1^Y5^X8,      X1^X5^Y8,      }, // 358
    {Y4^X5^Y5,      Z0^X4^Y4,      X5^X6^Y6,      Y5,            }, // 359
    {Y4^X5^Y5,      Z0^X4^Y4,      X5^X6^Y6,      Y3,            }, // 360
    {Y4^X5^Y5,      Z0^X4^Y4,      X5^X6^Y6,      X3,            }, // 361
    {Y4^X5^Y5,      Z0^X4^Y4,      X5^X6^Y6,      Y2,            }, // 362
    {Y4^X5^Y5,      Z0^X4^Y4,      X5^X6^Y6,      X2,            }, // 363
    {Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^X7^Y7,      }, // 364
    {Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      X6,            }, // 365
    {Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Y3,            }, // 366
    {Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      X3,            }, // 367
    {Y4^X9^Y9,      X4^Y4^Z4,      Z3^Y5^X8,      Z2^X5^Y8,      }, // 368
    {X4,            Y4,            Y5^X10,        X5^Y10,        }, // 369
    {Y4^X5^Y5,      Z0^X4^Y4,      S0^X6^Y6,      X2,            }, // 370
    {Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      S0^X7^Y7,      }, // 371
    {Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      S0^X5^Y6,      }, // 372
    {Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      S0^X5^Y7,      }, // 373
    {Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      S0^X5^Y8,      }, // 374
    {Y4^X5^Y5,      Z0^X4^Y4,      S1^X6^Y6,      X2,            }, // 375
    {Y4^X6^Y6,      Z0^X4^Y4,      X2,            Y2,            }, // 376
    {Y4^X6^Y6,      Z0^X4^Y4,      S1^X5^Y5,      X2,            }, // 377
    {Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      S1^X7^Y7,      }, // 378
    {Y4^X6^Y6,      Z0^X4^Y4,      S1^X5^Y5,      S0^X7^Y7,      }, // 379
    {Y4^X7^Y7,      Z1^X4^Y4,      S1^Y5^X6,      Y2,            }, // 380
    {Y4^X7^Y7,      Z0^X4^Y4,      S1^Y5^X6,      X2,            }, // 381
    {Y4^X7^Y7,      Z1^X4^Y4,      S1^Y5^X6,      Z0^X5^Y6,      }, // 382
    {Y4^X7^Y7,      Z0^X4^Y4,      S1^Y5^X6,      S0^X5^Y6,      }, // 383
    {Y4^X8^Y8,      Z1^X4^Y4,      S1^Y5^X7,      Z0^X5^Y7,      }, // 384
    {Y4^X8^Y8,      Z0^X4^Y4,      S1^Y5^X7,      S0^X5^Y7,      }, // 385
    {Y4^X9^Y9,      Z1^X4^Y4,      S1^Y5^X8,      Z0^X5^Y8,      }, // 386
    {Y4^X9^Y9,      Z0^X4^Y4,      S1^Y5^X8,      S0^X5^Y8,      }, // 387
    {Y4^X5^Y5,      S2^X4^Y4,      X2,            Y2,            }, // 388
    {Y4^X5^Y5,      Z0^X4^Y4,      S2^X6^Y6,      X3,            }, // 389
    {Y4^X5^Y5,      Z0^X4^Y4,      S2^X6^Y6,      Y2,            }, // 390
    {Y4^X5^Y5,      S2^X4^Y4,      S1^X6^Y6,      X2,            }, // 391
    {Y4^X6^Y6,      Z1^X4^Y4,      X5,            Y6,            }, // 392
    {Y4^X6^Y6,      Z0^X4^Y4,      Y2,            X3,            }, // 393
    {Y4^X6^Y6,      S2^X4^Y4,      X2,            Y2,            }, // 394
    {Y4^X6^Y6,      Z0^X4^Y4,      S2^X5^Y5,      Y2,            }, // 395
    {Y4^X6^Y6,      S2^X4^Y4,      S1^X5^Y5,      X2,            }, // 396
    {Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      S2^X7^Y7,      }, // 397
    {Y4^X6^Y6,      Z0^X4^Y4,      S2^X5^Y5,      S1^X7^Y7,      }, // 398
    {Y4^X6^Y6,      S2^X4^Y4,      S1^X5^Y5,      S0^X7^Y7,      }, // 399
    {Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      Y6,            }, // 400
    {Y4^X7^Y7,      Z0^X4^Y4,      S2^Y5^X6,      Y2,            }, // 401
    {Y4^X7^Y7,      S2^X4^Y4,      S1^Y5^X6,      X2,            }, // 402
    {Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      Z2^X5^Y6,      }, // 403
    {Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      S2^X5^Y6,      }, // 404
    {Y4^X7^Y7,      Z0^X4^Y4,      S2^Y5^X6,      S1^X5^Y6,      }, // 405
    {Y4^X7^Y7,      S2^X4^Y4,      S1^Y5^X6,      S0^X5^Y6,      }, // 406
    {Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      Z3^X5^Y7,      }, // 407
    {Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      Z2^X5^Y7,      }, // 408
    {Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      S2^X5^Y7,      }, // 409
    {Y4^X8^Y8,      Z0^X4^Y4,      S2^Y5^X7,      S1^X5^Y7,      }, // 410
    {Y4^X8^Y8,      S2^X4^Y4,      S1^Y5^X7,      S0^X5^Y7,      }, // 411
    {Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      Z3^X5^Y8,      }, // 412
    {Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      Z2^X5^Y8,      }, // 413
    {Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      S2^X5^Y8,      }, // 414
    {Y4^X9^Y9,      Z0^X4^Y4,      S2^Y5^X8,      S1^X5^Y8,      }, // 415
    {Y4^X9^Y9,      S2^X4^Y4,      S1^Y5^X8,      S0^X5^Y8,      }, // 416
    {X4^Y4,         Y2,            Z3,            Y3,            }, // 417
    {X4^Y4,         Y2,            Z2,            Y3,            }, // 418
    {X4^Y4,         Y1,            Z2,            Y2,            }, // 419
    {Y1^X4^Y4,      X1,            Z2,            Y2,            }, // 420
    {Y4^X5^Y5,      X4^Y4,         Y2,            Z3,            }, // 421
    {Y4^X5^Y5,      X4^Y4,         Y2,            Z2,            }, // 422
    {Z3^Y4^X5^Y5,   X4^Y4,         Y1,            Z2,            }, // 423
    {Z3^Y4^X5^Y5,   Y1^X4^Y4,      X1,            Z2,            }, // 424
    {Y4^X5^Y5,      X4^Y4,         Z3^X5,         Y2,            }, // 425
    {Y4^X5^Y5,      X4^Y4,         Z2^X5,         Y2,            }, // 426
    {Z3^Y4^X5^Y5,   X4^Y4,         Z2^X5,         Y1,            }, // 427
    {Z3^Y4^X5^Y5,   Y1^X4^Y4,      Z2^X5,         X1,            }, // 428
    {Y4^X6^Y6,      X4^Y4,         Y2,            Y3,            }, // 429
    {Y4^X6^Y6,      X4^Y4,         Z3,            Y3,            }, // 430
    {Y4^X6^Y6,      X4^Y4,         Z2,            Y3,            }, // 431
    {Z3^Y4^X6^Y6,   X4^Y4,         Z2,            Y2,            }, // 432
    {Z3^Y4^X6^Y6,   Y1^X4^Y4,      Z2,            Y2,            }, // 433
    {Y4^X6^Y6,      X4^Y4,         X5^Y5,         Y2,            }, // 434
    {Y4^X6^Y6,      X4^Y4,         Y2^X5^Y5,      Z3,            }, // 435
    {Y4^X6^Y6,      X4^Y4,         Y2^X5^Y5,      Z2,            }, // 436
    {Z3^Y4^X6^Y6,   X4^Y4,         Y1^X5^Y5,      Z2,            }, // 437
    {Z3^Y4^X6^Y6,   Y1^X4^Y4,      X1^X5^Y5,      Z2,            }, // 438
    {Y4^X6^Y6,      X4^Y4,         X5^Y5,         Z3^X6,         }, // 439
    {Y4^X6^Y6,      X4^Y4,         Y2^X5^Y5,      Z3^X6,         }, // 440
    {Y4^X6^Y6,      X4^Y4,         Y2^X5^Y5,      Z2^X6,         }, // 441
    {Z3^Y4^X6^Y6,   X4^Y4,         Y1^X5^Y5,      Z2^X6,         }, // 442
    {Z3^Y4^X6^Y6,   Y1^X4^Y4,      X1^X5^Y5,      Z2^X6,         }, // 443
    {Y4^X7^Y7,      X4^Y4,         Y2^Y5^X6,      Y3,            }, // 444
    {Z3^Y4^X7^Y7,   X4^Y4,         Y1^Y5^X6,      Y2,            }, // 445
    {Z3^Y4^X7^Y7,   Y1^X4^Y4,      X1^Y5^X6,      Y2,            }, // 446
    {Y4^X7^Y7,      X4^Y4,         Y2^Y5^X6,      X5^Y6,         }, // 447
    {Y4^X7^Y7,      X4^Y4,         Y2^Y5^X6,      Z3^X5^Y6,      }, // 448
    {Y4^X7^Y7,      X4^Y4,         Y2^Y5^X6,      Z2^X5^Y6,      }, // 449
    {Z3^Y4^X7^Y7,   X4^Y4,         Y1^Y5^X6,      Z2^X5^Y6,      }, // 450
    {Z3^Y4^X7^Y7,   Y1^X4^Y4,      X1^Y5^X6,      Z2^X5^Y6,      }, // 451
    {Y4^X7^Y7,      X4^Y4,         Y2^Y5^X6,      Y3^X5^Y6,      }, // 452
    {Z3^Y4^X7^Y7,   X4^Y4,         Y1^Y5^X6,      Y2^X5^Y6,      }, // 453
    {Z3^Y4^X7^Y7,   Y1^X4^Y4,      X1^Y5^X6,      Y2^X5^Y6,      }, // 454
    {Y4^X8^Y8,      X4^Y4,         Y2^Y5^X7,      X5^Y7,         }, // 455
    {Y4^X8^Y8,      X4^Y4,         Y2^Y5^X7,      Z3^X5^Y7,      }, // 456
    {Y4^X8^Y8,      X4^Y4,         Y2^Y5^X7,      Z2^X5^Y7,      }, // 457
    {Z3^Y4^X8^Y8,   X4^Y4,         Y1^Y5^X7,      Z2^X5^Y7,      }, // 458
    {Z3^Y4^X8^Y8,   Y1^X4^Y4,      X1^Y5^X7,      Z2^X5^Y7,      }, // 459
    {Y4^X8^Y8,      X4^Y4,         Y2^Y5^X7,      Y3^X5^Y7,      }, // 460
    {Z3^Y4^X8^Y8,   X4^Y4,         Y1^Y5^X7,      Y2^X5^Y7,      }, // 461
    {Z3^Y4^X8^Y8,   Y1^X4^Y4,      X1^Y5^X7,      Y2^X5^Y7,      }, // 462
    {Y4^X9^Y9,      X4^Y4,         Y2^Y5^X8,      X5^Y8,         }, // 463
    {Y4^X9^Y9,      X4^Y4,         Y2^Y5^X8,      Z3^X5^Y8,      }, // 464
    {Y4^X9^Y9,      X4^Y4,         Y2^Y5^X8,      Z2^X5^Y8,      }, // 465
    {Z3^Y4^X9^Y9,   X4^Y4,         Y1^Y5^X8,      Z2^X5^Y8,      }, // 466
    {Z3^Y4^X9^Y9,   Y1^X4^Y4,      X1^Y5^X8,      Z2^X5^Y8,      }, // 467
};

const UINT_64 GFX10_SW_PATTERN_NIBBLE3[][4] =
{
    {}, // 0
    {Y6,            X6,            Y7,            X7,            }, // 1
    {Y5,            X6,            Y6,            X7,            }, // 2
    {Y5,            X5,            Y6,            X6,            }, // 3
    {Y4,            X5,            Y5,            X6,            }, // 4
    {Y4,            X4,            Y5,            X5,            }, // 5
    {Z0^X6^Y6,      X6,            Y7,            X7,            }, // 6
    {Z0^Y5^X6,      X6,            Y6,            X7,            }, // 7
    {Z0^X5^Y5,      X5,            Y6,            X6,            }, // 8
    {Z0^Y4^X5,      X5,            Y5,            X6,            }, // 9
    {Z0^X4^Y4,      X4,            Y5,            X5,            }, // 10
    {Z1^Y6^X7,      Z0^X6^Y7,      Y7,            X7,            }, // 11
    {Z1^Y5^X7,      Z0^X6^Y6,      Y6,            X7,            }, // 12
    {Z1^Y5^X6,      Z0^X5^Y6,      Y6,            X6,            }, // 13
    {Z1^Y4^X6,      Z0^X5^Y5,      Y5,            X6,            }, // 14
    {Z1^Y4^X5,      Z0^X4^Y5,      Y5,            X5,            }, // 15
    {X6^Y6,         X6,            Y7,            X7,            }, // 16
    {Y5^X6,         X6,            Y6,            X7,            }, // 17
    {X5^Y5,         X5,            Y6,            X6,            }, // 18
    {Y4^X5,         X5,            Y5,            X6,            }, // 19
    {X4^Y4,         X4,            Y5,            X5,            }, // 20
    {Y6^X7,         X6^Y7,         Y7,            X7,            }, // 21
    {Y5^X7,         X6^Y6,         Y6,            X7,            }, // 22
    {Y5^X6,         X5^Y6,         Y6,            X6,            }, // 23
    {Y4^X6,         X5^Y5,         Y5,            X6,            }, // 24
    {Y4^X5,         X4^Y5,         Y5,            X5,            }, // 25
    {Y3,            X4,            Y5,            X5,            }, // 26
    {Y4,            X5,            Y6,            X6,            }, // 27
    {Y2,            X4,            Y5,            X6,            }, // 28
    {Y2,            X3,            Y4,            X5,            }, // 29
    {Y4,            X6,            Y6,            X7,            }, // 30
    {Y3,            X4,            Y6,            X6,            }, // 31
    {Y2,            X3,            Y4,            X6,            }, // 32
    {Y2,            X2,            Y3,            X4,            }, // 33
    {Z0^X6^Y6,      X4,            Y6,            X7,            }, // 34
    {Z0^X6^Y6,      X3,            Y4,            X6,            }, // 35
    {Z0^X6^Y6,      Y2,            X3,            Y4,            }, // 36
    {Y2^X6^Y6,      X2,            Y3,            X4,            }, // 37
    {Z1^Y6^X7,      Z0^X6^Y7,      Y4,            X7,            }, // 38
    {Z1^Y6^X7,      Z0^X6^Y7,      Y3,            X4,            }, // 39
    {Y2^Y6^X7,      Z0^X6^Y7,      Y3,            X4,            }, // 40
    {Y2^Y6^X7,      X2^X6^Y7,      Y3,            X4,            }, // 41
    {X5,            Y6,            X6,            Y7,            }, // 42
    {Y5,            X5,            Y6,            Y2^Y7,         }, // 43
    {X4,            Y5,            X5,            Y2^Y6,         }, // 44
    {Y4,            X4,            Y5,            Y1^Y6,         }, // 45
    {Y3,            X4,            Y5,            Y1^Y6,         }, // 46
    {Y4,            X5,            Y6,            Y2^Y7,         }, // 47
    {X3,            Y4,            X5,            Y2^Y6,         }, // 48
    {Y2,            X3,            Y4,            Y1^Y6,         }, // 49
    {Y4,            Y6,            X6,            Y7,            }, // 50
    {Y3,            X4,            Y6,            Y2^Y7,         }, // 51
    {X2,            Y3,            X4,            Y2^Y6,         }, // 52
    {Y1,            X3,            Y4,            X2^Y6,         }, // 53
    {Z0^X6^Y6,      Y4,            X6,            Y7,            }, // 54
    {Z0^X6^Y6,      X3,            Y4,            Y2^Y7,         }, // 55
    {Y2^X6^Y6,      Y3,            X4,            X2^Y7,         }, // 56
    {X2^X6^Y6,      X3,            Y4,            Y1^Y7,         }, // 57
    {Z0^Y6^X7,      Z5^X6^Y7,      Y4,            Y7,            }, // 58
    {Z0^Y6^X7,      Z5^X6^Y7,      Y3,            X4,            }, // 59
    {Z0^Y6^X7,      Y2^X6^Y7,      X3,            Y4,            }, // 60
    {X2^Y6^X7,      Y1^X6^Y7,      X3,            Y4,            }, // 61
    {X5,            Y5,            X6,            Y2^Y6,         }, // 62
    {Y5,            X5,            Y2^Y6,         X2^Y7,         }, // 63
    {Y4,            X5,            Y1^Y5,         X2^Y6,         }, // 64
    {Y4,            X4,            Y1^Y5,         X1^Y6,         }, // 65
    {Y5,            X5,            X2^Y6,         Y2^Y7,         }, // 66
    {Y4,            X5,            X2^Y5,         Y1^Y6,         }, // 67
    {Y4,            X4,            X1^Y5,         Y1^Y6,         }, // 68
    {Y3,            X4,            Y1^Y5,         X1^Y6,         }, // 69
    {X4,            Y5,            X6,            Y2^Y6,         }, // 70
    {Y4,            X5,            X2^Y6,         Y2^Y7,         }, // 71
    {X3,            Y4,            Y1^Y5,         X2^Y6,         }, // 72
    {Y3,            X4,            X1^Y6,         Y1^Y7,         }, // 73
    {X3,            Y4,            X6,            Y2^Y6,         }, // 74
    {Y3,            X4,            Y2^Y6,         X2^Y7,         }, // 75
    {Y3,            X4,            Y1^Y6,         X2^Y7,         }, // 76
    {Z4^X6^Y6,      X3,            Y4,            X6,            }, // 77
    {Z4^X6^Y6,      X3,            Y4,            Y2^Y6,         }, // 78
    {Y1^X6^Y6,      Y3,            X4,            X2^Y7,         }, // 79
    {Z5^Y6^X7,      Z4^X6^Y7,      Y3,            X4,            }, // 80
    {Y2^Y6^X7,      Z4^X6^Y7,      Y3,            X4,            }, // 81
    {Y1^Y6^X7,      X2^X6^Y7,      Y3,            X4,            }, // 82
    {Y5,            Y1^Y6,         Y2^Y7,         X2^Y8,         }, // 83
    {X4,            Y1^Y5,         X1^Y6,         Y2^Y7,         }, // 84
    {Y4,            Y0^Y5,         Y1^Y6,         X1^Y7,         }, // 85
    {Y5,            Y1^Y6,         X2^Y7,         Y2^Y8,         }, // 86
    {X4,            X1^Y5,         Y1^Y6,         X2^Y7,         }, // 87
    {Y4,            Y0^Y5,         X1^Y6,         Y1^Y7,         }, // 88
    {X3,            Y0^Y5,         X1^Y6,         Y1^Y7,         }, // 89
    {Y4,            Y1^Y6,         X2^Y7,         Y2^Y8,         }, // 90
    {X4,            X1^Y6,         Y1^Y7,         X2^Y8,         }, // 91
    {X3,            X1^Y6,         Y1^Y7,         X2^Y8,         }, // 92
    {X3,            Y4,            X2^Y6,         Y1^Y7,         }, // 93
    {X3,            Y1^Y6,         X2^Y7,         Y2^Y8,         }, // 94
    {Z3^X6^Y6,      X3,            Y4,            Y2^Y7,         }, // 95
    {Y2^X6^Y6,      X3,            X2^Y7,         Y1^Y8,         }, // 96
    {Z3^Y6^X7,      Y2^X6^Y7,      X3,            Y4,            }, // 97
    {Y2^Y6^X7,      X2^X6^Y7,      X3,            Y1^Y7,         }, // 98
    {Y6,            X6,            Y7,            S0^Y8,         }, // 99
    {Y5,            X6,            Y6,            S0^Y7,         }, // 100
    {Y5,            X5,            Y6,            S0^Y7,         }, // 101
    {Y4,            X5,            Y5,            S0^Y6,         }, // 102
    {Y4,            X4,            Y5,            S0^Y6,         }, // 103
    {Y3,            X4,            Y5,            S0^Y6,         }, // 104
    {Y4,            X5,            Y6,            S0^Y7,         }, // 105
    {Y2,            X4,            Y5,            S0^Y6,         }, // 106
    {Y2,            X3,            Y4,            S0^Y6,         }, // 107
    {Y4,            X6,            Y6,            S0^Y7,         }, // 108
    {Y3,            X4,            Y6,            S0^Y7,         }, // 109
    {Z0^X6^Y6,      X6,            Y7,            S0^Y8,         }, // 110
    {Z0^X6^Y6,      X4,            Y6,            S0^Y7,         }, // 111
    {Z0^X6^Y6,      X3,            Y4,            S0^Y7,         }, // 112
    {S0^X6^Y6,      Y2,            X3,            Y4,            }, // 113
    {Z0^Y6^X7,      Z5^X6^Y7,      Y7,            S0^Y8,         }, // 114
    {Z0^Y6^X7,      Z5^X6^Y7,      Y4,            S0^Y7,         }, // 115
    {Z0^Y6^X7,      S0^X6^Y7,      Y3,            X4,            }, // 116
    {S0^Y6^X7,      Y2^X6^Y7,      X3,            Y4,            }, // 117
    {Y6,            X6,            S0^Y7,         S1^Y8,         }, // 118
    {Y5,            X6,            S0^Y6,         S1^Y7,         }, // 119
    {Y5,            X5,            S0^Y6,         S1^Y7,         }, // 120
    {Y4,            X5,            S0^Y5,         S1^Y6,         }, // 121
    {Y4,            X4,            S0^Y5,         S1^Y6,         }, // 122
    {Y3,            X4,            S0^Y5,         S1^Y6,         }, // 123
    {Y4,            X5,            S0^Y6,         S1^Y7,         }, // 124
    {X3,            Y4,            S0^Y5,         S1^Y6,         }, // 125
    {Y4,            X6,            S0^Y6,         S1^Y7,         }, // 126
    {Y3,            X4,            S0^Y6,         S1^Y7,         }, // 127
    {Z4^X6^Y6,      X6,            S0^Y7,         S1^Y8,         }, // 128
    {Z4^X6^Y6,      Y4,            S0^Y6,         S1^Y7,         }, // 129
    {S1^X6^Y6,      X3,            Y4,            S0^Y7,         }, // 130
    {Z5^Y6^X7,      Z4^X6^Y7,      S0^Y7,         S1^Y8,         }, // 131
    {S1^Y6^X7,      Z4^X6^Y7,      Y4,            S0^Y7,         }, // 132
    {S1^Y6^X7,      S0^X6^Y7,      Y3,            X4,            }, // 133
    {Y6,            S0^Y7,         S1^Y8,         S2^Y9,         }, // 134
    {Y5,            S0^Y6,         S1^Y7,         S2^Y8,         }, // 135
    {Y4,            S0^Y5,         S1^Y6,         S2^Y7,         }, // 136
    {X3,            S0^Y5,         S1^Y6,         S2^Y7,         }, // 137
    {Y4,            S0^Y6,         S1^Y7,         S2^Y8,         }, // 138
    {X3,            Y4,            S0^Y6,         S1^Y7,         }, // 139
    {Y2,            X3,            S0^Y6,         S1^Y7,         }, // 140
    {X2,            Y2,            X3,            S0^Y6,         }, // 141
    {Z3^X6^Y6,      S0^Y7,         S1^Y8,         S2^Y9,         }, // 142
    {S2^X6^Y6,      Y4,            S0^Y7,         S1^Y8,         }, // 143
    {S0^X6^Y6,      X2,            Y2,            X3,            }, // 144
    {Z3^Y6^X7,      S2^X6^Y7,      S0^Y7,         S1^Y8,         }, // 145
    {S2^Y6^X7,      S1^X6^Y7,      Y4,            S0^Y7,         }, // 146
    {S0^Y6^X7,      X2^X6^Y7,      Y2,            X3,            }, // 147
    {X4,            Z4,            Y4,            X5,            }, // 148
    {X3,            Z4,            Y4,            X4,            }, // 149
    {X3,            Z3,            Y4,            X4,            }, // 150
    {X3,            Z3,            Y3,            X4,            }, // 151
    {X2,            Z3,            Y3,            X3,            }, // 152
    {X4^Y4^Z4,      Z4,            Y4,            X5,            }, // 153
    {X3^Y4^Z4,      Z4,            Y4,            X4,            }, // 154
    {X3^Z3^Y4,      Z3,            Y4,            X4,            }, // 155
    {X3^Y3^Z3,      Z3,            Y3,            X4,            }, // 156
    {X2^Y3^Z3,      Z3,            Y3,            X3,            }, // 157
    {X4^Y5^Z5,      Y4^Z4^X5,      Y4,            X5,            }, // 158
    {X3^Y5^Z5,      X4^Y4^Z4,      Y4,            X4,            }, // 159
    {X3^Z4^Y5,      Z3^X4^Y4,      Y4,            X4,            }, // 160
    {X3^Y4^Z4,      Y3^Z3^X4,      Y3,            X4,            }, // 161
    {X2^Y4^Z4,      X3^Y3^Z3,      Y3,            X3,            }, // 162
    {X4,            Y4^Z4^X5,      Y4,            X5,            }, // 163
    {X3,            X4^Y4^Z4,      Y4,            X4,            }, // 164
    {X3,            Z3^X4^Y4,      Y4,            X4,            }, // 165
    {X3,            Y3^Z3^X4,      Y3,            X4,            }, // 166
    {X2,            X3^Y3^Z3,      Y3,            X3,            }, // 167
    {X3,            Z3,            Y2,            X4,            }, // 168
    {X2,            Z3,            Y2,            X3,            }, // 169
    {X3,            Z4,            Y4,            X5,            }, // 170
    {X2,            Z4,            Y3,            X4,            }, // 171
    {X2,            Z3,            Y3,            X4,            }, // 172
    {Y2,            X3,            Z4,            Y4,            }, // 173
    {Z3,            Y3,            X4,            Z4,            }, // 174
    {Z3^X6^Y6,      Y3,            X4,            Z4,            }, // 175
    {X2^X6^Y6,      Z4,            Y3,            X4,            }, // 176
    {X2^X6^Y6,      Z3,            Y3,            X4,            }, // 177
    {X2^X6^Y6,      Z3,            Y2,            X3,            }, // 178
    {Z3^Y6^X7,      Z4^X6^Y7,      Y3,            X4,            }, // 179
    {X2^Y6^X7,      Z4^X6^Y7,      Y3,            X4,            }, // 180
    {X2^Y6^X7,      Z3^X6^Y7,      Y3,            X4,            }, // 181
    {X2^Y6^X7,      Z3^X6^Y7,      Y2,            X3,            }, // 182
    {X6^Y9,         Y6^X9,         X7^Y8,         Y7^X8,         }, // 183
    {Y5^X9,         X6^Y8,         Y6^X8,         X7^Y7,         }, // 184
    {X5^Y8,         Y5^X8,         X6^Y7,         Y6^X7,         }, // 185
    {Y4^X8,         X5^Y7,         Y5^X7,         X6^Y6,         }, // 186
    {X4^Y7,         Y4^X7,         X5^Y6,         Y5^X6,         }, // 187
    {X6^Y8,         Y5^X9,         X7^Y7,         Y6^X8,         }, // 188
    {X5^Y7,         Y4^X8,         X6^Y6,         Y5^X7,         }, // 189
    {X3^Y7,         Y4^X7,         X5^Y6,         Y5^X6,         }, // 190
    {Y3^X8,         X5^Y7,         X6^Y6,         Y5^X7,         }, // 191
    {Y3^X7,         X3^Y7,         X5^Y6,         Y5^X6,         }, // 192
    {X6,            Y6^X9,         X7^Y8,         Y7^X8,         }, // 193
    {Y5,            X6^Y8,         X7^Y7,         Y6^X8,         }, // 194
    {Y3,            Y5^X8,         X6^Y7,         Y6^X7,         }, // 195
    {X3,            Y3^X8,         X6^Y6,         Y5^X7,         }, // 196
    {Y2,            Y3^X7,         X3^Y6,         Y5^X6,         }, // 197
    {Y6^X9,         X7^Y8,         Y7^X8,         Z0^X5^Y5,      }, // 198
    {X6^Y8,         Y6^X8,         X7^Y7,         Z0^X5^Y5,      }, // 199
    {X5^Y8,         X6^Y7,         Y6^X7,         Z0^X5^Y5,      }, // 200
    {Y3^X7,         X5^Y7,         X6^Y6,         Z0^X5^Y5,      }, // 201
    {X3^Y7,         Y3^X6,         X5^Y6,         Z0^X5^Y5,      }, // 202
    {X5,            X6^Y8,         Y6^X8,         X7^Y7,         }, // 203
    {Y3,            X5^Y8,         X6^Y7,         Y6^X7,         }, // 204
    {X3,            Y3^X7,         X5^Y7,         X6^Y6,         }, // 205
    {Y2,            X3^Y7,         Y3^X6,         X5^Y6,         }, // 206
    {X6,            Y6,            X7^Y8,         Y7^X8,         }, // 207
    {Y3,            X6,            Y6^X8,         X7^Y7,         }, // 208
    {X3,            Y3,            X6^Y7,         Y6^X7,         }, // 209
    {Y2,            X3,            Y3^X7,         X6^Y6,         }, // 210
    {X2,            Y2,            X3^Y6,         Y3^X6,         }, // 211
    {Y6,            X7^Y8,         Y7^X8,         X5^Y6,         }, // 212
    {X6,            X7^Y7,         Y6^X8,         X5^Y6,         }, // 213
    {Y3,            X6^Y7,         Y6^X7,         X5^Y6,         }, // 214
    {X3,            Y3^X7,         X6^Y6,         Z0^X5^Y6,      }, // 215
    {Y2,            Y3^X6,         X3^Y6,         Z0^X5^Y6,      }, // 216
    {Y3,            X6,            X7^Y7,         Y6^X8,         }, // 217
    {X2,            Y2,            Y3^X6,         X3^Y6,         }, // 218
    {X6^Y6,         Y6,            X7,            Y7^X8,         }, // 219
    {X6^Y6,         Y3,            Y6,            X7^Y7,         }, // 220
    {X6^Y6,         X3,            Y3,            Y6^X7,         }, // 221
    {X6^Y6,         Y2,            X3,            Y3^X7,         }, // 222
    {X3^Y6,         X2,            Y2,            Y3^X6,         }, // 223
    {X6,            X7,            Y7^X8,         X6^Y6,         }, // 224
    {Y3,            X6,            X7^Y7,         X6^Y6,         }, // 225
    {X3,            Y3,            X6^Y7,         X6^Y6,         }, // 226
    {Y2,            X3,            Y3^X7,         Z0^X6^Y6,      }, // 227
    {X2,            X3,            Y3^X6,         Y2^X6^Y6,      }, // 228
    {X6^Y6,         X6,            X7,            Y7^X8,         }, // 229
    {X6^Y6,         Y3,            X6,            X7^Y7,         }, // 230
    {X6^Y6,         X3,            Y3,            X6^Y7,         }, // 231
    {Z0^X6^Y6,      Y2,            X3,            Y3^X7,         }, // 232
    {Y2^X6^Y6,      X2,            X3,            Y3^X6,         }, // 233
    {X6^Y6,         X6^Y8,         X7,            Y7,            }, // 234
    {X6^Y6,         X6^Y8,         Y3,            X7,            }, // 235
    {X6^Y6,         X6^Y8,         X3,            Y3,            }, // 236
    {Z0^X6^Y6,      X3^Y8,         Y2,            Y3,            }, // 237
    {Y2^X6^Y6,      X3^Y8,         X2,            Y3,            }, // 238
    {Y6^X7,         X7,            Y7,            X6^Y7,         }, // 239
    {Y6^X7,         Y3,            X7,            X6^Y7,         }, // 240
    {Y6^X7,         X3,            Y3,            X6^Y7,         }, // 241
    {Y2^Y6^X7,      X3,            Y3,            Z0^X6^Y7,      }, // 242
    {Y2^Y6^X7,      X3,            Y3,            X2^X6^Y7,      }, // 243
    {Y6^X7,         X6^Y7,         X7,            Y7,            }, // 244
    {Y6^X7,         X6^Y7,         Y3,            X7,            }, // 245
    {Y6^X7,         X6^Y7,         X3,            Y3,            }, // 246
    {Y2^Y6^X7,      Z0^X6^Y7,      X3,            Y3,            }, // 247
    {Y2^Y6^X7,      X2^X6^Y7,      X3,            Y3,            }, // 248
    {X5^Y9,         Y6^X8,         X6^Y8,         X7^Y7,         }, // 249
    {Y5^X8,         X5^Y8,         Y6^X7,         Y2^X6^Y7,      }, // 250
    {Y4^X8,         X5^Y7,         Y5^X7,         X2^X6^Y6,      }, // 251
    {Y4^X7,         X4^Y7,         Y5^X6,         Y1^X5^Y6,      }, // 252
    {Y4^X8,         X5^Y7,         Y5^X7,         Y2^X6^Y6,      }, // 253
    {Y4^X7,         X3^Y7,         Y5^X6,         Y1^X5^Y6,      }, // 254
    {Y5^X9,         Y6^X8,         X6^Y8,         X7^Y7,         }, // 255
    {Y3^X8,         X5^Y7,         Y5^X7,         Y2^X6^Y6,      }, // 256
    {Y3^X7,         X3^Y7,         Y5^X6,         Y1^X5^Y6,      }, // 257
    {X5,            Y6^X8,         X6^Y8,         X7^Y7,         }, // 258
    {Y3,            X5^Y8,         Y6^X7,         Y2^X6^Y7,      }, // 259
    {X3,            Y3^X7,         X5^Y7,         Y2^X6^Y6,      }, // 260
    {Y2,            Y3^X6,         X3^Y7,         Y1^X5^Y6,      }, // 261
    {Y6^X8,         X6^Y8,         X7^Y7,         Z0^X5^Y5,      }, // 262
    {X5^Y8,         Y6^X7,         Y2^X6^Y7,      Z0^X5^Y5,      }, // 263
    {Y3^X7,         X5^Y7,         X2^X6^Y6,      Z0^X5^Y5,      }, // 264
    {Y3^X6,         X3^Y7,         Y1^X5^Y6,      Z0^X5^Y5,      }, // 265
    {X3,            Y3^X7,         X5^Y7,         X2^X6^Y6,      }, // 266
    {Y3,            X5,            X6^Y8,         X7^Y7,         }, // 267
    {X3,            Y3,            X5^Y8,         X6^Y7,         }, // 268
    {X3,            Y3,            X5^Y8,         Y2^X6^Y7,      }, // 269
    {Y2,            X3,            Y3^X6,         X5^Y6,         }, // 270
    {X2,            Y2,            Y3^X5,         X3^Y6,         }, // 271
    {X6,            Y6^X8,         X7^Y7,         X5^Y6,         }, // 272
    {Y3,            Y6^X7,         Y2^X6^Y7,      X5^Y6,         }, // 273
    {X3,            Y3^X7,         Y2^X6^Y6,      Z0^X5^Y6,      }, // 274
    {X3,            Y3^X7,         Y2^X6^Y6,      Y1^X5^Y6,      }, // 275
    {X3,            Y3,            Y6^X7,         Y2^X6^Y7,      }, // 276
    {X2,            X3,            Y3^X7,         Y2^X6^Y6,      }, // 277
    {X6^Y6,         X3,            Y3,            Y2^X6^Y7,      }, // 278
    {X3,            Y3,            Y2^X6^Y7,      X6^Y6,         }, // 279
    {X3,            Y3,            X2^X6^Y7,      Y2^X6^Y6,      }, // 280
    {Y2^X6^Y6,      X3,            Y3,            X2^X6^Y7,      }, // 281
    {X6^Y6,         X6^Y8,         Y3,            Y7,            }, // 282
    {X6^Y6,         Y2^X6^Y8,      X3,            Y3,            }, // 283
    {Y2^X6^Y6,      X2^X6^Y8,      X3,            Y3,            }, // 284
    {Y6^X7,         Y3,            Y7,            X6^Y7,         }, // 285
    {Y6^X7,         X3,            Y3,            Y2^X6^Y7,      }, // 286
    {Y6^X7,         X6^Y7,         Y3,            Y7,            }, // 287
    {Y6^X7,         Y2^X6^Y7,      X3,            Y3,            }, // 288
    {X5^Y8,         Y5^X8,         X6^Y7,         Y2^Y6^X7,      }, // 289
    {X5^Y8,         Y5^X8,         X2^X6^Y7,      Y2^Y6^X7,      }, // 290
    {Y4^X8,         X5^Y7,         X2^X6^Y6,      Y1^Y5^X7,      }, // 291
    {X4^Y7,         Y4^X7,         X1^X5^Y6,      Y1^Y5^X6,      }, // 292
    {Y4^X9,         X6^Y7,         Y5^X8,         Y2^Y6^X7,      }, // 293
    {X5^Y8,         Y5^X8,         X2^Y6^X7,      Y2^X6^Y7,      }, // 294
    {X5^Y7,         Y4^X8,         X2^Y5^X7,      Y1^X6^Y6,      }, // 295
    {X3^Y7,         Y4^X7,         X1^Y5^X6,      Y1^X5^Y6,      }, // 296
    {X5^Y8,         X6^Y7,         Y5^X8,         Y2^Y6^X7,      }, // 297
    {Y3^X8,         X5^Y7,         X2^Y5^X7,      Y1^X6^Y6,      }, // 298
    {Y3^X7,         X3^Y7,         X1^Y5^X6,      Y1^X5^Y6,      }, // 299
    {Y3,            X6^Y7,         Y5^X8,         Y2^Y6^X7,      }, // 300
    {Y3,            Y5^X8,         X2^Y6^X7,      Y2^X6^Y7,      }, // 301
    {X3,            Y3^X8,         X2^Y5^X7,      Y1^X6^Y6,      }, // 302
    {Y2,            Y3^X6,         X3^Y6,         X1^X5^Y5,      }, // 303
    {X5^Y8,         X6^Y7,         Y2^Y6^X7,      Z0^X5^Y5,      }, // 304
    {X5^Y8,         X2^X6^Y7,      Y2^Y6^X7,      Z0^X5^Y5,      }, // 305
    {Y3^X8,         Y2^Y5^X7,      Y1^X6^Y6,      Z0^X5^Y5,      }, // 306
    {Y3^X7,         Y2^X6^Y6,      X1^X5^Y7,      Y1^X5^Y5,      }, // 307
    {Y3,            X5^Y8,         X6^Y7,         Y2^Y6^X7,      }, // 308
    {Y3,            X5^Y8,         X2^X6^Y7,      Y2^Y6^X7,      }, // 309
    {X3,            Y3^X8,         Y2^Y5^X7,      Y1^X6^Y6,      }, // 310
    {X3,            Y3^X7,         Y2^X6^Y6,      X1^X5^Y7,      }, // 311
    {X3,            Y3,            X6^Y7,         Y2^Y6^X7,      }, // 312
    {X3,            Y3,            X2^X6^Y7,      Y2^Y6^X7,      }, // 313
    {X2,            X3,            Y3^X7,         Y2^Y5^X6,      }, // 314
    {X2,            X3,            Y3^X6,         Y2^X5^Y6,      }, // 315
    {Y3,            X6^Y7,         Y2^Y6^X7,      X5^Y6,         }, // 316
    {Y3,            X2^Y6^X7,      Y2^X6^Y7,      X5^Y6,         }, // 317
    {Y3,            X2^Y6^X7,      Y2^X6^Y7,      Z0^X5^Y6,      }, // 318
    {Y3,            X2^Y6^X7,      Y2^X6^Y7,      X1^X5^Y6,      }, // 319
    {X3,            Y3,            X2^Y6^X7,      Y2^X6^Y7,      }, // 320
    {X6^Y6,         X3,            Y3,            Y2^Y6^X7,      }, // 321
    {Y2^X6^Y6,      X3,            Y3,            X2^X6^Y6,      }, // 322
    {X3,            Y3,            Y2^Y6^X7,      X6^Y6,         }, // 323
    {Y2^Y6^X7,      X3,            Y3,            X6^Y7,         }, // 324
    {Y2^Y6^X7,      X6^Y7,         X3,            Y3,            }, // 325
    {Y5^X8,         Y1^X5^Y8,      X2^X6^Y7,      Y2^Y6^X7,      }, // 326
    {Y4^X8,         X1^X5^Y7,      Y1^Y5^X7,      X2^X6^Y6,      }, // 327
    {Y4^X7,         Y0^X4^Y7,      X1^X5^Y6,      Y1^Y5^X6,      }, // 328
    {Y4^X8,         Y1^X5^Y7,      X1^Y5^X7,      Y2^X6^Y6,      }, // 329
    {Y3^X7,         Y0^X4^Y6,      X1^Y4^X6,      Y1^X5^Y5,      }, // 330
    {Y3^X8,         Y1^X5^Y7,      X1^Y5^X7,      Y2^X6^Y6,      }, // 331
    {Y3^X7,         Y1^X4^Y7,      Y2^X5^Y6,      X1^Y5^X6,      }, // 332
    {Y3,            X5^Y8,         X2^Y6^X7,      Y2^X6^Y7,      }, // 333
    {Y3,            Y1^X5^Y8,      X2^X6^Y7,      Y2^Y6^X7,      }, // 334
    {X3,            Y3^X7,         Y1^X5^Y6,      X1^Y5^X6,      }, // 335
    {X3,            Y3^X6,         Y1^X4^Y6,      Y2^X5^Y5,      }, // 336
    {Y1^X5^Y8,      X2^X6^Y7,      Y2^Y6^X7,      Z0^X5^Y5,      }, // 337
    {X1^X5^Y8,      Y2^Y6^X7,      X2^X6^Y7,      Y1^X5^Y5,      }, // 338
    {X1^X5^Y8,      X2^X6^Y7,      Y2^Y6^X7,      Y1^X5^Y5,      }, // 339
    {Y3,            X1^X5^Y8,      Y2^Y6^X7,      X2^X6^Y7,      }, // 340
    {Y3,            X1^X5^Y8,      X2^X6^Y7,      Y2^Y6^X7,      }, // 341
    {X3,            Y3,            Y1^X5^Y7,      X2^X6^Y6,      }, // 342
    {X3,            Y3,            X1^X5^Y7,      Y2^X6^Y6,      }, // 343
    {X3,            Y3,            X1^X5^Y7,      X2^X6^Y6,      }, // 344
    {Y3,            X2^Y6^X7,      Y1^X6^Y7,      Y2^X5^Y6,      }, // 345
    {X3,            Y3,            X2^Y6^X7,      Y1^X6^Y7,      }, // 346
    {X2^X6^Y6,      X3,            Y3,            Y1^X6^Y6,      }, // 347
    {X2^X6^Y6,      X3,            Y3,            Y2^X6^Y6,      }, // 348
    {X3,            Y3,            Y1^X6^Y7,      X2^X6^Y6,      }, // 349
    {Y2^X6^Y6,      X3,            Y3,            Y1^X6^Y7,      }, // 350
    {Y2^X6^Y6,      Y1^X6^Y8,      X3,            Y3,            }, // 351
    {Y2^Y6^X7,      X3,            Y3,            Y1^X6^Y7,      }, // 352
    {Y6,            X7^Y8,         Y7^X8,         Z0^X5^Y6,      }, // 353
    {X6,            X7^Y7,         Y6^X8,         Z0^X5^Y6,      }, // 354
    {Y3,            X6^Y7,         Y6^X7,         Z0^X5^Y6,      }, // 355
    {X6^X8^Y8,      Y6,            X7,            Y7^X8,         }, // 356
    {X6^X8^Y8,      Y3,            Y6,            X7^Y7,         }, // 357
    {X6^X8^Y8,      X3,            Y3,            Y6^X7,         }, // 358
    {X6^X8^Y8,      Y2,            X3,            Y3^X7,         }, // 359
    {X3^X8^Y8,      X2,            Y2,            Y3^X6,         }, // 360
    {X6,            X7,            Y7^X8,         Z0^X6^Y6,      }, // 361
    {Y3,            X6,            X7^Y7,         Z0^X6^Y6,      }, // 362
    {X3,            Y3,            X6^Y7,         Z0^X6^Y6,      }, // 363
    {Z0^X6^Y6,      X6,            X7,            Y7^X8,         }, // 364
    {Z0^X6^Y6,      Y3,            X6,            X7^Y7,         }, // 365
    {Z0^X6^Y6,      X3,            Y3,            X6^Y7,         }, // 366
    {Z0^X6^Y6,      X6^X9^Y9,      X7,            Y7,            }, // 367
    {Z0^X6^Y6,      X6^X9^Y9,      Y3,            X7,            }, // 368
    {Z0^X6^Y6,      X6^X9^Y9,      X3,            Y3,            }, // 369
    {Z0^X6^Y6,      X3^X9^Y9,      Y2,            Y3,            }, // 370
    {Y2^X6^Y6,      X3^X9^Y9,      X2,            Y3,            }, // 371
    {Z1^Y6^X7,      X7,            Y7,            Z0^X6^Y7,      }, // 372
    {Z1^Y6^X7,      Y3,            X7,            Z0^X6^Y7,      }, // 373
    {Z1^Y6^X7,      X3,            Y3,            Z0^X6^Y7,      }, // 374
    {Z1^Y6^X7,      Z0^X6^Y7,      X7,            Y7,            }, // 375
    {Z1^Y6^X7,      Z0^X6^Y7,      Y3,            X7,            }, // 376
    {Z1^Y6^X7,      Z0^X6^Y7,      X3,            Y3,            }, // 377
    {Y6^X9,         X6^Y9,         Y7^X8,         S0^X7^Y8,      }, // 378
    {Y5^X9,         X6^Y8,         Y6^X8,         S0^X7^Y7,      }, // 379
    {Y5^X8,         X5^Y8,         Y6^X7,         S0^X6^Y7,      }, // 380
    {Y4^X8,         X5^Y7,         Y5^X7,         S0^X6^Y6,      }, // 381
    {Y4^X7,         X4^Y7,         Y5^X6,         S0^X5^Y6,      }, // 382
    {Y4^X7,         X3^Y7,         Y5^X6,         S0^X5^Y6,      }, // 383
    {Y3^X8,         X5^Y7,         Y5^X7,         S0^X6^Y6,      }, // 384
    {Y3^X7,         X3^Y7,         Y5^X6,         S0^X5^Y6,      }, // 385
    {X6,            Y6^X9,         Y7^X8,         S0^X7^Y8,      }, // 386
    {Y5,            X6^Y8,         Y6^X8,         S0^X7^Y7,      }, // 387
    {Y3,            Y5^X8,         Y6^X7,         S0^X6^Y7,      }, // 388
    {X3,            Y3^X8,         Y5^X7,         S0^X6^Y6,      }, // 389
    {Y2,            Y3^X6,         X3^Y6,         X5^Y5,         }, // 390
    {Y6^X9,         Y7^X8,         S0^X7^Y8,      Z0^X5^Y5,      }, // 391
    {X6^Y8,         Y6^X8,         S0^X7^Y7,      Z0^X5^Y5,      }, // 392
    {X5^Y8,         Y6^X7,         S0^X6^Y7,      Z0^X5^Y5,      }, // 393
    {Y3^X7,         X5^Y7,         S0^X6^Y6,      Z0^X5^Y5,      }, // 394
    {Y3^X6,         X3^Y7,         S0^X5^Y6,      Z0^X5^Y5,      }, // 395
    {X5,            X6^Y8,         Y6^X8,         S0^X7^Y7,      }, // 396
    {Y3,            X5^Y8,         Y6^X7,         S0^X6^Y7,      }, // 397
    {X3,            Y3^X7,         X5^Y7,         S0^X6^Y6,      }, // 398
    {Y2,            Y3^X6,         X3^Y7,         S0^X5^Y6,      }, // 399
    {X6,            Y6,            Y7^X8,         S0^X7^Y8,      }, // 400
    {Y3,            X6,            Y6^X8,         S0^X7^Y7,      }, // 401
    {X3,            Y3,            Y6^X7,         S0^X6^Y7,      }, // 402
    {Y2,            X3,            Y3^X7,         S0^X6^Y6,      }, // 403
    {Y6,            Y7^X8,         S0^X7^Y8,      Z0^X5^Y6,      }, // 404
    {X6,            Y6^X8,         S0^X7^Y7,      Z0^X5^Y6,      }, // 405
    {Y3,            Y6^X7,         S0^X6^Y7,      Z0^X5^Y6,      }, // 406
    {X3,            Y3^X7,         S0^X6^Y6,      Z0^X5^Y6,      }, // 407
    {Y2,            Y3^X6,         X3^Y6,         S0^X5^Y6,      }, // 408
    {X6^X8^Y8,      Y6,            Y7,            S0^X7^Y8,      }, // 409
    {X6^X8^Y8,      Y3,            Y6,            S0^X7^Y7,      }, // 410
    {S0^X8^Y8,      X3,            Y3,            X6^Y6,         }, // 411
    {S0^X8^Y8,      Y2,            X3,            Y3^X6,         }, // 412
    {X6,            Y7,            S0^X7^Y8,      Z0^X6^Y6,      }, // 413
    {Y3,            X6,            S0^X7^Y7,      Z0^X6^Y6,      }, // 414
    {X3,            Y3,            S0^X6^Y7,      Z0^X6^Y6,      }, // 415
    {Y2,            X3,            Y3^X6,         S0^X6^Y6,      }, // 416
    {Z0^X6^Y6,      X6,            Y7,            S0^X7^Y8,      }, // 417
    {Z0^X6^Y6,      Y3,            X6,            S0^X7^Y7,      }, // 418
    {Z0^X6^Y6,      X3,            Y3,            S0^X6^Y7,      }, // 419
    {S0^X6^Y6,      Y2,            X3,            Y3^X6,         }, // 420
    {Z0^X6^Y6,      X6^X9^Y9,      Y7,            S0^X7,         }, // 421
    {Z0^X6^Y6,      X6^X9^Y9,      Y3,            S0^X7,         }, // 422
    {Z0^X6^Y6,      S0^X9^Y9,      X3,            Y3,            }, // 423
    {S0^X6^Y6,      X3^X9^Y9,      Y2,            Y3,            }, // 424
    {Z0^Y6^X7,      Y7,            S0^X7,         Z4^X6^Y7,      }, // 425
    {Z0^Y6^X7,      Y3,            S0^X7,         Z4^X6^Y7,      }, // 426
    {Z0^Y6^X7,      X3,            Y3,            S0^X6^Y7,      }, // 427
    {S0^Y6^X7,      X3,            Y3,            Y2^X6^Y7,      }, // 428
    {Z0^Y6^X7,      Z4^X6^Y7,      Y7,            S0^X7,         }, // 429
    {Z0^Y6^X7,      Z4^X6^Y7,      Y3,            S0^X7,         }, // 430
    {Z0^Y6^X7,      S0^X6^Y7,      X3,            Y3,            }, // 431
    {S0^Y6^X7,      Y2^X6^Y7,      X3,            Y3,            }, // 432
    {X6^Y9,         Y6^X9,         S0^X7^Y8,      S1^Y7^X8,      }, // 433
    {Y5^X9,         X6^Y8,         S0^Y6^X8,      S1^X7^Y7,      }, // 434
    {X5^Y8,         Y5^X8,         S0^X6^Y7,      S1^Y6^X7,      }, // 435
    {Y4^X8,         X5^Y7,         S0^Y5^X7,      S1^X6^Y6,      }, // 436
    {X4^Y7,         Y4^X7,         S0^X5^Y6,      S1^Y5^X6,      }, // 437
    {X6^Y8,         Y5^X9,         S0^X7^Y7,      S1^Y6^X8,      }, // 438
    {X5^Y7,         Y4^X8,         S0^X6^Y6,      S1^Y5^X7,      }, // 439
    {X3^Y7,         Y4^X7,         S0^X5^Y6,      S1^Y5^X6,      }, // 440
    {Y3^X8,         X5^Y7,         S0^X6^Y6,      S1^Y5^X7,      }, // 441
    {Y3^X7,         X3^Y7,         S0^X5^Y6,      S1^Y5^X6,      }, // 442
    {X6,            Y6^X9,         S0^X7^Y8,      S1^Y7^X8,      }, // 443
    {Y5,            X6^Y8,         S0^X7^Y7,      S1^Y6^X8,      }, // 444
    {Y3,            Y5^X8,         S0^X6^Y7,      S1^Y6^X7,      }, // 445
    {X3,            Y3^X8,         S0^X6^Y6,      S1^Y5^X7,      }, // 446
    {Y2,            Y3^X6,         X3^Y6,         S0^X5^Y5,      }, // 447
    {Y6^X9,         S0^X7^Y8,      S1^Y7^X8,      Z0^X5^Y5,      }, // 448
    {X6^Y8,         S0^Y6^X8,      S1^X7^Y7,      Z0^X5^Y5,      }, // 449
    {X5^Y8,         S0^X6^Y7,      S1^Y6^X7,      Z0^X5^Y5,      }, // 450
    {Y3^X8,         S0^X6^Y6,      S1^Y5^X7,      Z0^X5^Y5,      }, // 451
    {Y3^X6,         X3^Y7,         S0^X5^Y6,      S1^X5^Y5,      }, // 452
    {X5,            X6^Y8,         S0^Y6^X8,      S1^X7^Y7,      }, // 453
    {Y3,            X5^Y8,         S0^X6^Y7,      S1^Y6^X7,      }, // 454
    {X6,            Y6,            S0^X7^Y8,      S1^Y7^X8,      }, // 455
    {Y3,            X6,            S0^Y6^X8,      S1^X7^Y7,      }, // 456
    {X3,            Y3,            S0^X6^Y7,      S1^Y6^X7,      }, // 457
    {Y2,            X3,            Y3^X7,         S0^Y5^X6,      }, // 458
    {Y6,            S0^X7^Y8,      S1^Y7^X8,      Z0^X5^Y6,      }, // 459
    {X6,            S0^X7^Y7,      S1^Y6^X8,      Z0^X5^Y6,      }, // 460
    {Y3,            S0^X6^Y7,      S1^Y6^X7,      Z0^X5^Y6,      }, // 461
    {Y3,            X6,            S0^X7^Y7,      S1^Y6^X8,      }, // 462
    {X6^X8^Y8,      Y6,            S0^X7,         S1^Y7^X8,      }, // 463
    {X6^X8^Y8,      Y3,            S0^X7,         S1^Y6^X8,      }, // 464
    {S1^X8^Y8,      X3,            Y3,            S0^X6^Y6,      }, // 465
    {X6,            S0^X7,         S1^Y7^X8,      Z3^X6^Y6,      }, // 466
    {Y3,            S0^X7,         S1^Y6^X8,      Z3^X6^Y6,      }, // 467
    {X3,            Y3,            S0^X6^Y7,      S1^X6^Y6,      }, // 468
    {Z3^X6^Y6,      X6,            S0^X7,         S1^Y7^X8,      }, // 469
    {Z3^X6^Y6,      Y3,            S0^X7,         S1^Y6^X8,      }, // 470
    {S1^X6^Y6,      X3,            Y3,            S0^X6^Y7,      }, // 471
    {Z3^X6^Y6,      X6^X9^Y9,      S0^X7,         S1^Y7,         }, // 472
    {Z3^X6^Y6,      S1^X9^Y9,      Y3,            S0^X7,         }, // 473
    {S1^X6^Y6,      S0^X9^Y9,      X3,            Y3,            }, // 474
    {Z4^Y6^X7,      S0^X7,         S1^Y7,         Z3^X6^Y7,      }, // 475
    {S1^Y6^X7,      Y3,            S0^X7,         Z3^X6^Y7,      }, // 476
    {S1^Y6^X7,      X3,            Y3,            S0^X6^Y7,      }, // 477
    {Z4^Y6^X7,      Z3^X6^Y7,      S0^X7,         S1^Y7,         }, // 478
    {S1^Y6^X7,      Z3^X6^Y7,      Y3,            S0^X7,         }, // 479
    {S1^Y6^X7,      S0^X6^Y7,      X3,            Y3,            }, // 480
    {Y6^X9,         S0^X6^Y9,      S1^Y7^X8,      S2^X7^Y8,      }, // 481
    {Y5^X9,         S0^X6^Y8,      S1^Y6^X8,      S2^X7^Y7,      }, // 482
    {Y5^X8,         S0^X5^Y8,      S1^Y6^X7,      S2^X6^Y7,      }, // 483
    {Y4^X8,         S0^X5^Y7,      S1^Y5^X7,      S2^X6^Y6,      }, // 484
    {Y4^X7,         S0^X4^Y7,      S1^Y5^X6,      S2^X5^Y6,      }, // 485
    {Y3^X7,         S0^X4^Y6,      S1^Y4^X6,      S2^X5^Y5,      }, // 486
    {Y3^X8,         S0^X5^Y7,      S1^Y5^X7,      S2^X6^Y6,      }, // 487
    {Y3^X6,         X3^Y7,         S0^X4^Y6,      S1^X5^Y5,      }, // 488
    {Y6,            S0^X6^Y9,      S1^Y7^X8,      S2^X7^Y8,      }, // 489
    {Y5,            S0^X6^Y8,      S1^Y6^X8,      S2^X7^Y7,      }, // 490
    {Y3,            Y5^X7,         S0^X5^Y7,      S1^X6^Y6,      }, // 491
    {X3,            Y3^X7,         S0^X5^Y6,      S1^Y5^X6,      }, // 492
    {Y2,            Y3^X5,         X3^Y6,         S0^X4^Y5,      }, // 493
    {S0^X6^Y9,      S1^Y7^X8,      S2^X7^Y8,      Z0^X5^Y5,      }, // 494
    {S0^X6^Y8,      S1^Y6^X8,      S2^X7^Y7,      Z0^X5^Y5,      }, // 495
    {S0^X5^Y8,      S1^Y6^X7,      S2^X6^Y7,      Z0^X5^Y5,      }, // 496
    {Y3^X7,         S0^X5^Y7,      S1^X6^Y6,      S2^X5^Y5,      }, // 497
    {X5,            S0^X6^Y8,      S1^Y6^X8,      S2^X7^Y7,      }, // 498
    {Y3,            S0^X5^Y8,      S1^Y6^X7,      S2^X6^Y7,      }, // 499
    {X3,            Y3^X7,         S0^X5^Y7,      S1^X6^Y6,      }, // 500
    {Y6,            S0^X6,         S1^Y7^X8,      S2^X7^Y8,      }, // 501
    {Y3,            S0^X6,         S1^Y6^X8,      S2^X7^Y7,      }, // 502
    {X3,            Y3,            S0^X5^Y7,      S1^X6^Y6,      }, // 503
    {Y2,            X3,            Y3^X6,         S0^X5^Y6,      }, // 504
    {S0^X6,         S1^Y7^X8,      S2^X7^Y8,      Z2^X5^Y6,      }, // 505
    {S0^X6,         S1^Y6^X8,      S2^X7^Y7,      Z2^X5^Y6,      }, // 506
    {Y3,            S0^X6^Y7,      S1^Y6^X7,      S2^X5^Y6,      }, // 507
    {X3,            Y3^X7,         S0^X6^Y6,      S1^X5^Y6,      }, // 508
    {S2^X8^Y8,      Y6,            S0^X6,         S1^X7^Y7,      }, // 509
    {S2^X8^Y8,      Y3,            S0^X6,         S1^Y6^X7,      }, // 510
    {S0^X6,         S1^Y7,         S2^X7^Y8,      Z2^X6^Y6,      }, // 511
    {Y3,            S0^X6,         S1^X7^Y7,      S2^X6^Y6,      }, // 512
    {Z2^X6^Y6,      S0^X6,         S1^Y7,         S2^X7^Y8,      }, // 513
    {S2^X6^Y6,      Y3,            S0^X6,         S1^X7^Y7,      }, // 514
    {Z2^X6^Y6,      S2^X9^Y9,      S0^X6,         S1^Y7,         }, // 515
    {S2^X6^Y6,      S1^X9^Y9,      Y3,            S0^X6,         }, // 516
    {Z2^Y6^X7,      S0^X7,         S1^Y7,         S2^X6^Y7,      }, // 517
    {S2^Y6^X7,      Y3,            S0^X7,         S1^X6^Y7,      }, // 518
    {Z2^Y6^X7,      S2^X6^Y7,      S0^X7,         S1^Y7,         }, // 519
    {S2^Y6^X7,      S1^X6^Y7,      Y3,            S0^X7,         }, // 520
    {X2,            Z4,            Y4,            X3,            }, // 521
    {X2,            Z3,            Y4,            X3,            }, // 522
    {Y3,            X3,            Z4,            X5,            }, // 523
    {Y3,            X2,            Z4,            X3,            }, // 524
    {Y3,            X2,            Z3,            X3,            }, // 525
    {Y2,            X2,            Y3,            X3,            }, // 526
    {Z3,            X3,            Z4,            X5^Y5,         }, // 527
    {X2,            Z4,            X3,            Y2^X5^Y5,      }, // 528
    {X2,            Z3,            X3,            Y2^X5^Y5,      }, // 529
    {X2,            Y3,            X3,            Y1^X5^Y5,      }, // 530
    {X2,            Y3,            X3,            X1^X5^Y5,      }, // 531
    {Y3,            Z3,            X3,            Z4,            }, // 532
    {Y2,            Y3,            X3,            Z4,            }, // 533
    {Z3,            X3,            Z4,            X5^Y6,         }, // 534
    {X2,            Z4,            X3,            Z3^X5^Y6,      }, // 535
    {X2,            Z3,            X3,            Z2^X5^Y6,      }, // 536
    {X2,            Y3,            X3,            Z2^X5^Y6,      }, // 537
    {Z3^X7,         Y3,            X3,            Z4,            }, // 538
    {Z3^X7,         X2,            Z4,            X3,            }, // 539
    {Z2^X7,         X2,            Z3,            X3,            }, // 540
    {Z2^X7,         X2,            Y3,            X3,            }, // 541
    {Z3,            X3,            Z4,            Y3^X6^Y6,      }, // 542
    {X2,            Z4,            X3,            Y3^X6^Y6,      }, // 543
    {X2,            Z3,            X3,            Y3^X6^Y6,      }, // 544
    {X2,            Y3,            X3,            Y2^X6^Y6,      }, // 545
    {Y3^X6^Y6,      Z3,            X3,            Z4,            }, // 546
    {Y3^X6^Y6,      X2,            Z4,            X3,            }, // 547
    {Y3^X6^Y6,      X2,            Z3,            X3,            }, // 548
    {Y2^X6^Y6,      X2,            Y3,            X3,            }, // 549
    {Y3^X6^Y6,      Z3^X8,         X3,            Z4,            }, // 550
    {X2^X6^Y6,      Z3^X8,         Z4,            X3,            }, // 551
    {X2^X6^Y6,      Z2^X8,         Z3,            X3,            }, // 552
    {X2^X6^Y6,      Z2^X8,         Y3,            X3,            }, // 553
    {Y3^Y6^X7,      X3,            Z4,            Z3^X6^Y7,      }, // 554
    {Y3^Y6^X7,      Z4,            X3,            X2^X6^Y7,      }, // 555
    {Y3^Y6^X7,      Z3,            X3,            X2^X6^Y7,      }, // 556
    {Y2^Y6^X7,      Y3,            X3,            X2^X6^Y7,      }, // 557
    {Y3^Y6^X7,      Z3^X6^Y7,      X3,            Z4,            }, // 558
    {Y3^Y6^X7,      X2^X6^Y7,      Z4,            X3,            }, // 559
    {Y3^Y6^X7,      X2^X6^Y7,      Z3,            X3,            }, // 560
    {Y2^Y6^X7,      X2^X6^Y7,      Y3,            X3,            }, // 561
};

const UINT_16 DCC_64K_R_X_PATIDX[] =
{
       0, // 1 pipes 1 bpe ua @ SW_64K_R_X 1xaa @ Navi1x
       1, // 1 pipes 2 bpe ua @ SW_64K_R_X 1xaa @ Navi1x
       2, // 1 pipes 4 bpe ua @ SW_64K_R_X 1xaa @ Navi1x
       3, // 1 pipes 8 bpe ua @ SW_64K_R_X 1xaa @ Navi1x
       4, // 1 pipes 16 bpe ua @ SW_64K_R_X 1xaa @ Navi1x
       5, // 2 pipes 1 bpe ua @ SW_64K_R_X 1xaa @ Navi1x
       6, // 2 pipes 2 bpe ua @ SW_64K_R_X 1xaa @ Navi1x
       2, // 2 pipes 4 bpe ua @ SW_64K_R_X 1xaa @ Navi1x
       3, // 2 pipes 8 bpe ua @ SW_64K_R_X 1xaa @ Navi1x
       4, // 2 pipes 16 bpe ua @ SW_64K_R_X 1xaa @ Navi1x
       7, // 4+ pipes 1 bpe ua @ SW_64K_R_X 1xaa @ Navi1x
       6, // 4+ pipes 2 bpe ua @ SW_64K_R_X 1xaa @ Navi1x
       2, // 4+ pipes 4 bpe ua @ SW_64K_R_X 1xaa @ Navi1x
       3, // 4+ pipes 8 bpe ua @ SW_64K_R_X 1xaa @ Navi1x
       4, // 4+ pipes 16 bpe ua @ SW_64K_R_X 1xaa @ Navi1x
       0, // 1 pipes 1 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
       1, // 1 pipes 2 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
       2, // 1 pipes 4 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
       3, // 1 pipes 8 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
       4, // 1 pipes 16 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
       8, // 2 pipes 1 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
       9, // 2 pipes 2 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
      10, // 2 pipes 4 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
      11, // 2 pipes 8 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
      12, // 2 pipes 16 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
      13, // 4 pipes 1 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
      14, // 4 pipes 2 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
      15, // 4 pipes 4 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
      16, // 4 pipes 8 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
      17, // 4 pipes 16 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
      18, // 8 pipes 1 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
      19, // 8 pipes 2 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
      20, // 8 pipes 4 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
      21, // 8 pipes 8 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
      22, // 8 pipes 16 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
      23, // 16 pipes 1 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
      24, // 16 pipes 2 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
      25, // 16 pipes 4 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
      26, // 16 pipes 8 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
      27, // 16 pipes 16 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
      28, // 32 pipes 1 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
      29, // 32 pipes 2 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
      30, // 32 pipes 4 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
      31, // 32 pipes 8 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
      32, // 32 pipes 16 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
      33, // 64 pipes 1 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
      34, // 64 pipes 2 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
      35, // 64 pipes 4 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
      36, // 64 pipes 8 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
      37, // 64 pipes 16 bpe pa @ SW_64K_R_X 1xaa @ Navi1x
};

const UINT_16 HTILE_64K_PATIDX[] =
{
       0, // 1xaa ua @ HTILE_64K_PATIDX @ Navi1x
       0, // 2xaa ua @ HTILE_64K_PATIDX @ Navi1x
       0, // 4xaa ua @ HTILE_64K_PATIDX @ Navi1x
       0, // 8xaa ua @ HTILE_64K_PATIDX @ Navi1x
       0, // 1 pipes 1xaa pa @ HTILE_64K_PATIDX @ Navi1x
       0, // 1 pipes 2xaa pa @ HTILE_64K_PATIDX @ Navi1x
       0, // 1 pipes 4xaa pa @ HTILE_64K_PATIDX @ Navi1x
       0, // 1 pipes 8xaa pa @ HTILE_64K_PATIDX @ Navi1x
       1, // 2 pipes 1xaa pa @ HTILE_64K_PATIDX @ Navi1x
       1, // 2 pipes 2xaa pa @ HTILE_64K_PATIDX @ Navi1x
       1, // 2 pipes 4xaa pa @ HTILE_64K_PATIDX @ Navi1x
       1, // 2 pipes 8xaa pa @ HTILE_64K_PATIDX @ Navi1x
       2, // 4 pipes 1xaa pa @ HTILE_64K_PATIDX @ Navi1x
       2, // 4 pipes 2xaa pa @ HTILE_64K_PATIDX @ Navi1x
       2, // 4 pipes 4xaa pa @ HTILE_64K_PATIDX @ Navi1x
       2, // 4 pipes 8xaa pa @ HTILE_64K_PATIDX @ Navi1x
       3, // 8 pipes 1xaa pa @ HTILE_64K_PATIDX @ Navi1x
       3, // 8 pipes 2xaa pa @ HTILE_64K_PATIDX @ Navi1x
       3, // 8 pipes 4xaa pa @ HTILE_64K_PATIDX @ Navi1x
       3, // 8 pipes 8xaa pa @ HTILE_64K_PATIDX @ Navi1x
       4, // 16 pipes 1xaa pa @ HTILE_64K_PATIDX @ Navi1x
       4, // 16 pipes 2xaa pa @ HTILE_64K_PATIDX @ Navi1x
       4, // 16 pipes 4xaa pa @ HTILE_64K_PATIDX @ Navi1x
       5, // 16 pipes 8xaa pa @ HTILE_64K_PATIDX @ Navi1x
       6, // 32 pipes 1xaa pa @ HTILE_64K_PATIDX @ Navi1x
       6, // 32 pipes 2xaa pa @ HTILE_64K_PATIDX @ Navi1x
       7, // 32 pipes 4xaa pa @ HTILE_64K_PATIDX @ Navi1x
       8, // 32 pipes 8xaa pa @ HTILE_64K_PATIDX @ Navi1x
       9, // 64 pipes 1xaa pa @ HTILE_64K_PATIDX @ Navi1x
      10, // 64 pipes 2xaa pa @ HTILE_64K_PATIDX @ Navi1x
      11, // 64 pipes 4xaa pa @ HTILE_64K_PATIDX @ Navi1x
      12, // 64 pipes 8xaa pa @ HTILE_64K_PATIDX @ Navi1x
};

const UINT_16 CMASK_64K_PATIDX[] =
{
       0, // 1 bpe ua @ CMASK_64K_PATIDX @ Navi1x
       0, // 2 bpe ua @ CMASK_64K_PATIDX @ Navi1x
       0, // 4 bpe ua @ CMASK_64K_PATIDX @ Navi1x
       0, // 8 bpe ua @ CMASK_64K_PATIDX @ Navi1x
       0, // 1 pipes 1 bpe pa @ CMASK_64K_PATIDX @ Navi1x
       0, // 1 pipes 2 bpe pa @ CMASK_64K_PATIDX @ Navi1x
       0, // 1 pipes 4 bpe pa @ CMASK_64K_PATIDX @ Navi1x
       0, // 1 pipes 8 bpe pa @ CMASK_64K_PATIDX @ Navi1x
       1, // 2 pipes 1 bpe pa @ CMASK_64K_PATIDX @ Navi1x
       1, // 2 pipes 2 bpe pa @ CMASK_64K_PATIDX @ Navi1x
       1, // 2 pipes 4 bpe pa @ CMASK_64K_PATIDX @ Navi1x
       1, // 2 pipes 8 bpe pa @ CMASK_64K_PATIDX @ Navi1x
       2, // 4 pipes 1 bpe pa @ CMASK_64K_PATIDX @ Navi1x
       2, // 4 pipes 2 bpe pa @ CMASK_64K_PATIDX @ Navi1x
       2, // 4 pipes 4 bpe pa @ CMASK_64K_PATIDX @ Navi1x
       2, // 4 pipes 8 bpe pa @ CMASK_64K_PATIDX @ Navi1x
       3, // 8 pipes 1 bpe pa @ CMASK_64K_PATIDX @ Navi1x
       3, // 8 pipes 2 bpe pa @ CMASK_64K_PATIDX @ Navi1x
       3, // 8 pipes 4 bpe pa @ CMASK_64K_PATIDX @ Navi1x
       3, // 8 pipes 8 bpe pa @ CMASK_64K_PATIDX @ Navi1x
       4, // 16 pipes 1 bpe pa @ CMASK_64K_PATIDX @ Navi1x
       4, // 16 pipes 2 bpe pa @ CMASK_64K_PATIDX @ Navi1x
       4, // 16 pipes 4 bpe pa @ CMASK_64K_PATIDX @ Navi1x
       4, // 16 pipes 8 bpe pa @ CMASK_64K_PATIDX @ Navi1x
       5, // 32 pipes 1 bpe pa @ CMASK_64K_PATIDX @ Navi1x
       5, // 32 pipes 2 bpe pa @ CMASK_64K_PATIDX @ Navi1x
       5, // 32 pipes 4 bpe pa @ CMASK_64K_PATIDX @ Navi1x
       5, // 32 pipes 8 bpe pa @ CMASK_64K_PATIDX @ Navi1x
       6, // 64 pipes 1 bpe pa @ CMASK_64K_PATIDX @ Navi1x
       6, // 64 pipes 2 bpe pa @ CMASK_64K_PATIDX @ Navi1x
       6, // 64 pipes 4 bpe pa @ CMASK_64K_PATIDX @ Navi1x
       7, // 64 pipes 8 bpe pa @ CMASK_64K_PATIDX @ Navi1x
};

const UINT_16 DCC_64K_R_X_RBPLUS_PATIDX[] =
{
       0, // 1 bpe ua @ SW_64K_R_X 1xaa @ RbPlus
       1, // 2 bpe ua @ SW_64K_R_X 1xaa @ RbPlus
       2, // 4 bpe ua @ SW_64K_R_X 1xaa @ RbPlus
       3, // 8 bpe ua @ SW_64K_R_X 1xaa @ RbPlus
       4, // 16 bpe ua @ SW_64K_R_X 1xaa @ RbPlus
       0, // 1 pipes (1 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
       1, // 1 pipes (1 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
       2, // 1 pipes (1 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
       3, // 1 pipes (1 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
       4, // 1 pipes (1 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      38, // 2 pipes (1-2 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      39, // 2 pipes (1-2 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      40, // 2 pipes (1-2 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      41, // 2 pipes (1-2 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      42, // 2 pipes (1-2 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      43, // 4 pipes (1-2 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      44, // 4 pipes (1-2 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      45, // 4 pipes (1-2 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      46, // 4 pipes (1-2 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      47, // 4 pipes (1-2 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      48, // 8 pipes (2 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      49, // 8 pipes (2 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      50, // 8 pipes (2 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      51, // 8 pipes (2 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      52, // 8 pipes (2 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      53, // 4 pipes (4 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      54, // 4 pipes (4 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      55, // 4 pipes (4 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      56, // 4 pipes (4 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      57, // 4 pipes (4 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      58, // 8 pipes (4 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      59, // 8 pipes (4 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      60, // 8 pipes (4 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      61, // 8 pipes (4 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      62, // 8 pipes (4 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      63, // 16 pipes (4 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      64, // 16 pipes (4 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      65, // 16 pipes (4 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      66, // 16 pipes (4 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      67, // 16 pipes (4 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      68, // 8 pipes (8 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      69, // 8 pipes (8 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      70, // 8 pipes (8 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      71, // 8 pipes (8 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      72, // 8 pipes (8 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      73, // 16 pipes (8 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      74, // 16 pipes (8 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      75, // 16 pipes (8 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      76, // 16 pipes (8 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      77, // 16 pipes (8 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      78, // 32 pipes (8 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      79, // 32 pipes (8 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      80, // 32 pipes (8 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      81, // 32 pipes (8 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      82, // 32 pipes (8 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      83, // 16 pipes (16 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      84, // 16 pipes (16 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      85, // 16 pipes (16 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      86, // 16 pipes (16 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      87, // 16 pipes (16 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      88, // 32 pipes (16 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      89, // 32 pipes (16 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      90, // 32 pipes (16 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      91, // 32 pipes (16 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      92, // 32 pipes (16 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      93, // 64 pipes (16 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      94, // 64 pipes (16 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      95, // 64 pipes (16 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      96, // 64 pipes (16 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      97, // 64 pipes (16 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      98, // 32 pipes (32 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
      99, // 32 pipes (32 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
     100, // 32 pipes (32 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
     101, // 32 pipes (32 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
     102, // 32 pipes (32 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
     103, // 64 pipes (32 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
     104, // 64 pipes (32 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
     105, // 64 pipes (32 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
     106, // 64 pipes (32 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
     107, // 64 pipes (32 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ RbPlus
};

const UINT_16 HTILE_64K_RBPLUS_PATIDX[] =
{
       0, // 1xaa ua @ HTILE_64K_PATIDX @ RbPlus
       0, // 2xaa ua @ HTILE_64K_PATIDX @ RbPlus
       0, // 4xaa ua @ HTILE_64K_PATIDX @ RbPlus
       0, // 8xaa ua @ HTILE_64K_PATIDX @ RbPlus
       0, // 1 pipes (1-2 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ RbPlus
       0, // 1 pipes (1-2 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ RbPlus
       0, // 1 pipes (1-2 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ RbPlus
       0, // 1 pipes (1-2 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ RbPlus
      13, // 2 pipes (1-2 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ RbPlus
      13, // 2 pipes (1-2 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ RbPlus
      13, // 2 pipes (1-2 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ RbPlus
      13, // 2 pipes (1-2 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ RbPlus
      14, // 4 pipes (1-2 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ RbPlus
      14, // 4 pipes (1-2 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ RbPlus
      14, // 4 pipes (1-2 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ RbPlus
      14, // 4 pipes (1-2 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ RbPlus
      15, // 8 pipes (1-2 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ RbPlus
      15, // 8 pipes (1-2 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ RbPlus
      15, // 8 pipes (1-2 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ RbPlus
      15, // 8 pipes (1-2 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ RbPlus
      13, // 2 pipes (4 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ RbPlus
      13, // 2 pipes (4 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ RbPlus
      13, // 2 pipes (4 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ RbPlus
      13, // 2 pipes (4 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ RbPlus
      16, // 4 pipes (4 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ RbPlus
      16, // 4 pipes (4 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ RbPlus
      16, // 4 pipes (4 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ RbPlus
      16, // 4 pipes (4 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ RbPlus
      17, // 8 pipes (4 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ RbPlus
      17, // 8 pipes (4 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ RbPlus
      17, // 8 pipes (4 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ RbPlus
      17, // 8 pipes (4 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ RbPlus
      18, // 16 pipes (4 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ RbPlus
      18, // 16 pipes (4 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ RbPlus
      18, // 16 pipes (4 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ RbPlus
      18, // 16 pipes (4 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ RbPlus
      19, // 4 pipes (8 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ RbPlus
      19, // 4 pipes (8 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ RbPlus
      19, // 4 pipes (8 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ RbPlus
      19, // 4 pipes (8 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ RbPlus
      20, // 8 pipes (8 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ RbPlus
      20, // 8 pipes (8 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ RbPlus
      20, // 8 pipes (8 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ RbPlus
      20, // 8 pipes (8 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ RbPlus
      21, // 16 pipes (8 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ RbPlus
      21, // 16 pipes (8 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ RbPlus
      21, // 16 pipes (8 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ RbPlus
      21, // 16 pipes (8 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ RbPlus
      22, // 32 pipes (8 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ RbPlus
      22, // 32 pipes (8 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ RbPlus
      22, // 32 pipes (8 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ RbPlus
      22, // 32 pipes (8 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ RbPlus
      23, // 8 pipes (16 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ RbPlus
      23, // 8 pipes (16 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ RbPlus
      23, // 8 pipes (16 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ RbPlus
      23, // 8 pipes (16 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ RbPlus
      24, // 16 pipes (16 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ RbPlus
      24, // 16 pipes (16 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ RbPlus
      24, // 16 pipes (16 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ RbPlus
      24, // 16 pipes (16 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ RbPlus
      25, // 32 pipes (16 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ RbPlus
      25, // 32 pipes (16 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ RbPlus
      25, // 32 pipes (16 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ RbPlus
      25, // 32 pipes (16 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ RbPlus
      26, // 64 pipes (16 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ RbPlus
      26, // 64 pipes (16 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ RbPlus
      26, // 64 pipes (16 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ RbPlus
      26, // 64 pipes (16 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ RbPlus
      27, // 16 pipes (32 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ RbPlus
      27, // 16 pipes (32 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ RbPlus
      27, // 16 pipes (32 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ RbPlus
      27, // 16 pipes (32 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ RbPlus
      28, // 32 pipes (32 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ RbPlus
      28, // 32 pipes (32 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ RbPlus
      28, // 32 pipes (32 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ RbPlus
      28, // 32 pipes (32 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ RbPlus
      29, // 64 pipes (32 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ RbPlus
      29, // 64 pipes (32 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ RbPlus
      29, // 64 pipes (32 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ RbPlus
      29, // 64 pipes (32 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ RbPlus
};

const UINT_16 CMASK_64K_RBPLUS_PATIDX[] =
{
       0, // 1 bpe ua @ CMASK_64K_PATIDX @ RbPlus
       0, // 2 bpe ua @ CMASK_64K_PATIDX @ RbPlus
       0, // 4 bpe ua @ CMASK_64K_PATIDX @ RbPlus
       0, // 8 bpe ua @ CMASK_64K_PATIDX @ RbPlus
       0, // 1 pipes (1-2 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ RbPlus
       0, // 1 pipes (1-2 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ RbPlus
       0, // 1 pipes (1-2 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ RbPlus
       0, // 1 pipes (1-2 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ RbPlus
       8, // 2 pipes (1-2 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ RbPlus
       8, // 2 pipes (1-2 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ RbPlus
       8, // 2 pipes (1-2 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ RbPlus
       8, // 2 pipes (1-2 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ RbPlus
       9, // 4 pipes (1-2 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ RbPlus
       9, // 4 pipes (1-2 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ RbPlus
       9, // 4 pipes (1-2 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ RbPlus
       9, // 4 pipes (1-2 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      10, // 8 pipes (1-2 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      10, // 8 pipes (1-2 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      10, // 8 pipes (1-2 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      10, // 8 pipes (1-2 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ RbPlus
       8, // 2 pipes (4 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ RbPlus
       8, // 2 pipes (4 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ RbPlus
       8, // 2 pipes (4 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ RbPlus
       8, // 2 pipes (4 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      11, // 4 pipes (4 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      11, // 4 pipes (4 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      11, // 4 pipes (4 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      11, // 4 pipes (4 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      12, // 8 pipes (4 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      12, // 8 pipes (4 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      12, // 8 pipes (4 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      12, // 8 pipes (4 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      13, // 16 pipes (4 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      13, // 16 pipes (4 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      13, // 16 pipes (4 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      13, // 16 pipes (4 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      14, // 4 pipes (8 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      14, // 4 pipes (8 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      14, // 4 pipes (8 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      14, // 4 pipes (8 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      15, // 8 pipes (8 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      15, // 8 pipes (8 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      15, // 8 pipes (8 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      16, // 8 pipes (8 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      15, // 16 pipes (8 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      15, // 16 pipes (8 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      15, // 16 pipes (8 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      17, // 16 pipes (8 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      18, // 32 pipes (8 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      18, // 32 pipes (8 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      18, // 32 pipes (8 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      19, // 32 pipes (8 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      20, // 8 pipes (16 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      20, // 8 pipes (16 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      20, // 8 pipes (16 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      21, // 8 pipes (16 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      22, // 16 pipes (16 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      22, // 16 pipes (16 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      22, // 16 pipes (16 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      23, // 16 pipes (16 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      22, // 32 pipes (16 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      22, // 32 pipes (16 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      22, // 32 pipes (16 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      24, // 32 pipes (16 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      25, // 64 pipes (16 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      25, // 64 pipes (16 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      25, // 64 pipes (16 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      26, // 64 pipes (16 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      27, // 16 pipes (32 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      27, // 16 pipes (32 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      27, // 16 pipes (32 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      28, // 16 pipes (32 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      29, // 32 pipes (32 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      29, // 32 pipes (32 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      29, // 32 pipes (32 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      30, // 32 pipes (32 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      29, // 64 pipes (32 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      29, // 64 pipes (32 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      29, // 64 pipes (32 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ RbPlus
      31, // 64 pipes (32 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ RbPlus
};

const UINT_64 DCC_64K_R_X_SW_PATTERN[][17] =
{
    {0,             X4,            Y4,            X5,            Y5,            X6,            Y6,            X7,            Y7,            X8,            Y8,            X9,            Y9,            0,             0,             0,             0,             },
    {0,             Y3,            X4,            Y4,            X5,            Y5,            X6,            Y6,            X7,            Y7,            X8,            Y8,            X9,            0,             0,             0,             0,             },
    {0,             X3,            Y3,            X4,            Y4,            X5,            Y5,            X6,            Y6,            X7,            Y7,            X8,            Y8,            0,             0,             0,             0,             },
    {0,             Y2,            X3,            Y3,            X4,            Y4,            X5,            Y5,            X6,            Y6,            X7,            Y7,            X8,            0,             0,             0,             0,             },
    {0,             X2,            Y2,            X3,            Y3,            X4,            Y4,            X5,            Y5,            X6,            Y6,            X7,            Y7,            0,             0,             0,             0,             },
    {0,             X3^Y3,         X4,            X5,            Y5,            X6,            Y6,            X7,            Y7,            X8,            Y8,            X9,            Y9,            0,             0,             0,             0,             },
    {0,             X3^Y3,         X4,            Y4,            X5,            Y5,            X6,            Y6,            X7,            Y7,            X8,            Y8,            X9,            0,             0,             0,             0,             },
    {0,             X3^Y3,         X4^Y4,         X5,            Y5,            X6,            Y6,            X7,            Y7,            X8,            Y8,            X9,            Y9,            0,             0,             0,             0,             },
    {0,             X4,            X5,            Y5,            X6,            Y6,            X7,            Y7,            X8,            Z0^X3^Y3,      Y8,            X9,            Y9,            0,             0,             0,             0,             },
    {0,             Y4,            X4,            X5,            Y5,            X6,            Y6,            X7,            Y7,            Z0^X3^Y3,      X8,            Y8,            X9,            0,             0,             0,             0,             },
    {0,             X3,            Y4,            X4,            X5,            Y5,            X6,            Y6,            X7,            Z0^X3^Y3,      Y7,            X8,            Y8,            0,             0,             0,             0,             },
    {0,             Y2,            X3,            Y4,            X4,            X5,            Y5,            X6,            Y6,            Z0^X3^Y3,      X7,            Y7,            X8,            0,             0,             0,             0,             },
    {0,             X2,            Y2,            X3,            Y4,            X4,            X5,            Y5,            X6,            Z0^X3^Y3,      Y6,            X7,            Y7,            0,             0,             0,             0,             },
    {0,             X5,            Y5,            X6,            Y6,            X7,            Y7,            X8,            Y8,            Z1^X3^Y3,      Z0^X4^Y4,      X9,            Y9,            0,             0,             0,             0,             },
    {0,             Y4,            X5,            Y5,            X6,            Y6,            X7,            Y7,            X8,            Z1^X3^Y3,      Z0^X4^Y4,      Y8,            X9,            0,             0,             0,             0,             },
    {0,             X3,            Y4,            X5,            Y5,            X6,            Y6,            X7,            Y7,            Z1^X3^Y3,      Z0^X4^Y4,      X8,            Y8,            0,             0,             0,             0,             },
    {0,             Y2,            X3,            Y4,            X5,            Y5,            X6,            Y6,            X7,            Z1^X3^Y3,      Z0^X4^Y4,      Y7,            X8,            0,             0,             0,             0,             },
    {0,             X2,            Y2,            X3,            Y4,            X5,            Y5,            X6,            Y6,            Z1^X3^Y3,      Z0^X4^Y4,      X7,            Y7,            0,             0,             0,             0,             },
    {0,             Y5,            X6,            Y6,            X7,            Y7,            X8,            Y8,            X9,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      Y9,            0,             0,             0,             0,             },
    {0,             Y4,            Y5,            X6,            Y6,            X7,            Y7,            X8,            Y8,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X9,            0,             0,             0,             0,             },
    {0,             X3,            Y4,            Y5,            X6,            Y6,            X7,            Y7,            X8,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      Y8,            0,             0,             0,             0,             },
    {0,             Y2,            X3,            Y4,            Y5,            X6,            Y6,            X7,            Y7,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X8,            0,             0,             0,             0,             },
    {0,             X2,            Y2,            X3,            Y4,            Y5,            X6,            Y6,            X7,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      Y7,            0,             0,             0,             0,             },
    {0,             X6,            Y6,            X7,            Y7,            X8,            Y8,            X9,            Y9,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      0,             0,             0,             0,             },
    {0,             Y4,            X6,            Y6,            X7,            Y7,            X8,            Y8,            X9,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      0,             0,             0,             0,             },
    {0,             X3,            Y4,            X6,            Y6,            X7,            Y7,            X8,            Y8,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      0,             0,             0,             0,             },
    {0,             Y2,            X3,            Y4,            X6,            Y6,            X7,            Y7,            X8,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      0,             0,             0,             0,             },
    {0,             X2,            Y2,            X3,            Y4,            X6,            Y6,            X7,            Y7,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      0,             0,             0,             0,             },
    {0,             Y6,            X7,            Y7,            X8,            Y8,            X9,            Y9,            X10,           X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      0,             0,             0,             },
    {0,             Y4,            Y6,            X7,            Y7,            X8,            Y8,            X9,            Y9,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      0,             0,             0,             },
    {0,             X3,            Y4,            Y6,            X7,            Y7,            X8,            Y8,            X9,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      0,             0,             0,             },
    {0,             Y2,            X3,            Y4,            Y6,            X7,            Y7,            X8,            Y8,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      0,             0,             0,             },
    {0,             X2,            X3,            Y4,            Y6,            X7,            Y7,            Y2,            X8,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      Y2^X6^Y6,      0,             0,             0,             },
    {0,             X7,            Y7,            X8,            Y8,            X9,            Y9,            X10,           Y10,           X3^Y3^Z5,      X4^Y4^Z4,      Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      Z0^X6^Y7,      0,             0,             },
    {0,             Y4,            X7,            Y7,            X8,            Y8,            X9,            Y9,            X10,           X3^Y3^Z5,      X4^Y4^Z4,      Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      Z0^X6^Y7,      0,             0,             },
    {0,             X3,            Y4,            X7,            Y7,            X8,            Y8,            X9,            Y9,            X3^Y3^Z5,      X4^Y4^Z4,      Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      Z0^X6^Y7,      0,             0,             },
    {0,             X3,            Y4,            X7,            Y7,            X8,            Y8,            Y2,            X9,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X8,      Z1^X5^Y8,      Y2^Y6^X7,      Z0^X6^Y7,      0,             0,             },
    {0,             X3,            Y4,            X7,            Y7,            X8,            Y8,            X2,            Y2,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X8,      Z0^X5^Y8,      Y2^Y6^X7,      X2^X6^Y7,      0,             0,             },
    {0,             Y4,            X5,            Y5,            X6,            Y6,            X7,            Y7,            X8,            Z0^X4^Y4,      Y8,            X9,            Y9,            0,             0,             0,             0,             },
    {0,             Y3,            Y4,            X5,            Y5,            X6,            Y6,            X7,            Y7,            Z0^X4^Y4,      X8,            Y8,            X9,            0,             0,             0,             0,             },
    {0,             X3,            Y3,            Y4,            X5,            Y5,            X6,            Y6,            X7,            Z0^X4^Y4,      Y7,            X8,            Y8,            0,             0,             0,             0,             },
    {0,             Y2,            X3,            Y3,            Y4,            X5,            Y5,            X6,            Y6,            Z0^X4^Y4,      X7,            Y7,            X8,            0,             0,             0,             0,             },
    {0,             X2,            Y2,            X3,            Y3,            Y4,            X5,            Y5,            X6,            Z0^X4^Y4,      Y6,            X7,            Y7,            0,             0,             0,             0,             },
    {0,             X5,            Y5,            X6,            Y6,            X7,            Y7,            X8,            Y8,            Y4^X5^Y5,      Z0^X4^Y4,      X9,            Y9,            0,             0,             0,             0,             },
    {0,             Y3,            X5,            Y5,            X6,            Y6,            X7,            Y7,            X8,            Y4^X5^Y5,      Z0^X4^Y4,      Y8,            X9,            0,             0,             0,             0,             },
    {0,             X3,            Y3,            X5,            Y5,            X6,            Y6,            X7,            Y7,            Y4^X5^Y5,      Z0^X4^Y4,      X8,            Y8,            0,             0,             0,             0,             },
    {0,             Y2,            X3,            Y3,            X5,            Y5,            X6,            Y6,            X7,            Y4^X5^Y5,      Z0^X4^Y4,      Y7,            X8,            0,             0,             0,             0,             },
    {0,             X2,            Y2,            X3,            Y3,            X5,            Y5,            X6,            Y6,            Y4^X5^Y5,      Z0^X4^Y4,      X7,            Y7,            0,             0,             0,             0,             },
    {0,             Y5,            X6,            Y6,            X7,            Y7,            X8,            Y8,            X9,            Y4^X5^Y5,      Z0^X4^Y4,      X5^X6^Y6,      Y9,            0,             0,             0,             0,             },
    {0,             Y3,            Y5,            X6,            Y6,            X7,            Y7,            X8,            Y8,            Y4^X5^Y5,      Z0^X4^Y4,      X5^X6^Y6,      X9,            0,             0,             0,             0,             },
    {0,             X3,            Y3,            Y5,            X6,            Y6,            X7,            Y7,            X8,            Y4^X5^Y5,      Z0^X4^Y4,      X5^X6^Y6,      Y8,            0,             0,             0,             0,             },
    {0,             Y2,            X3,            Y3,            Y5,            X6,            Y6,            X7,            Y7,            Y4^X5^Y5,      Z0^X4^Y4,      X5^X6^Y6,      X8,            0,             0,             0,             0,             },
    {0,             X2,            Y2,            X3,            Y3,            Y5,            X6,            Y6,            X7,            Y4^X5^Y5,      Z0^X4^Y4,      X5^X6^Y6,      Y7,            0,             0,             0,             0,             },
    {0,             X5,            X6,            Y6,            X7,            Y7,            X8,            Y8,            X9,            Y4^X6^Y6,      Z1^X4^Y4,      X5^Y5,         Y9,            0,             0,             0,             0,             },
    {0,             Y3,            X5,            X6,            Y6,            X7,            Y7,            X8,            Y8,            Y4^X6^Y6,      Z1^X4^Y4,      X5^Y5,         X9,            0,             0,             0,             0,             },
    {0,             X3,            Y3,            X5,            X6,            Y6,            X7,            Y7,            X8,            Y4^X6^Y6,      Z1^X4^Y4,      X5^Y5,         Y8,            0,             0,             0,             0,             },
    {0,             Y2,            X3,            Y3,            X5,            X6,            Y6,            X7,            Y7,            Y4^X6^Y6,      Z1^X4^Y4,      X5^Y5,         X8,            0,             0,             0,             0,             },
    {0,             X2,            Y2,            X3,            Y3,            X5,            X6,            Y6,            X7,            Y4^X6^Y6,      Z1^X4^Y4,      X5^Y5,         Y7,            0,             0,             0,             0,             },
    {0,             X5,            X6,            Y6,            X7,            Y7,            X8,            Y8,            X9,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      Y9,            0,             0,             0,             0,             },
    {0,             Y3,            X5,            X6,            Y6,            X7,            Y7,            X8,            Y8,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X9,            0,             0,             0,             0,             },
    {0,             X3,            Y3,            X5,            X6,            Y6,            X7,            Y7,            X8,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      Y8,            0,             0,             0,             0,             },
    {0,             Y2,            X3,            Y3,            X5,            X6,            Y6,            X7,            Y7,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X8,            0,             0,             0,             0,             },
    {0,             X2,            Y2,            X3,            Y3,            X5,            X6,            Y6,            X7,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      Y7,            0,             0,             0,             0,             },
    {0,             X6,            Y6,            X7,            Y7,            X8,            Y8,            X9,            Y9,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^X7^Y7,      0,             0,             0,             0,             },
    {0,             Y3,            X6,            Y6,            X7,            Y7,            X8,            Y8,            X9,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^X7^Y7,      0,             0,             0,             0,             },
    {0,             X3,            Y3,            X6,            Y6,            X7,            Y7,            X8,            Y8,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^X7^Y7,      0,             0,             0,             0,             },
    {0,             Y2,            X3,            Y3,            X6,            Y6,            X7,            Y7,            X8,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^X7^Y7,      0,             0,             0,             0,             },
    {0,             X2,            Y2,            X3,            Y3,            X6,            Y6,            X7,            Y7,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^X7^Y7,      0,             0,             0,             0,             },
    {0,             X6,            Y6,            X7,            Y7,            X8,            Y8,            X9,            Y9,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      X5^Y6,         0,             0,             0,             0,             },
    {0,             Y3,            X6,            Y6,            X7,            Y7,            X8,            Y8,            X9,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      X5^Y6,         0,             0,             0,             0,             },
    {0,             X3,            Y3,            X6,            Y6,            X7,            Y7,            X8,            Y8,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      X5^Y6,         0,             0,             0,             0,             },
    {0,             Y2,            X3,            Y3,            X6,            Y6,            X7,            Y7,            X8,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      X5^Y6,         0,             0,             0,             0,             },
    {0,             X2,            Y2,            X3,            Y3,            X6,            Y6,            X7,            Y7,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      X5^Y6,         0,             0,             0,             0,             },
    {0,             X6,            Y6,            X7,            Y7,            X8,            Y8,            X9,            Y9,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      0,             0,             0,             0,             },
    {0,             Y3,            X6,            Y6,            X7,            Y7,            X8,            Y8,            X9,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      0,             0,             0,             0,             },
    {0,             X3,            Y3,            X6,            Y6,            X7,            Y7,            X8,            Y8,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      0,             0,             0,             0,             },
    {0,             Y2,            X3,            Y3,            X6,            Y6,            X7,            Y7,            X8,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      0,             0,             0,             0,             },
    {0,             X2,            Y2,            X3,            Y3,            X6,            Y6,            X7,            Y7,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      0,             0,             0,             0,             },
    {0,             Y6,            X7,            Y7,            X8,            Y8,            X9,            Y9,            X10,           Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X6^X8^Y8,      0,             0,             0,             },
    {0,             Y3,            Y6,            X7,            Y7,            X8,            Y8,            X9,            Y9,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X6^X8^Y8,      0,             0,             0,             },
    {0,             X3,            Y3,            Y6,            X7,            Y7,            X8,            Y8,            X9,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X6^X8^Y8,      0,             0,             0,             },
    {0,             Y2,            X3,            Y3,            Y6,            X7,            Y7,            X8,            Y8,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X6^X8^Y8,      0,             0,             0,             },
    {0,             X2,            Y2,            Y3,            X6,            Y6,            X7,            Y7,            X8,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X3^X8^Y8,      0,             0,             0,             },
    {0,             X6,            X7,            Y7,            X8,            Y8,            X9,            Y9,            X10,           Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      X6^Y6,         0,             0,             0,             },
    {0,             Y3,            X6,            X7,            Y7,            X8,            Y8,            X9,            Y9,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      X6^Y6,         0,             0,             0,             },
    {0,             X3,            Y3,            X6,            X7,            Y7,            X8,            Y8,            X9,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      X6^Y6,         0,             0,             0,             },
    {0,             Y2,            X3,            Y3,            X6,            X7,            Y7,            X8,            Y8,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      X6^Y6,         0,             0,             0,             },
    {0,             X2,            X3,            Y3,            X6,            X7,            Y7,            Y2,            X8,            Y4^X8^Y8,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      X6^Y6,         0,             0,             0,             },
    {0,             X6,            X7,            Y7,            X8,            Y8,            X9,            Y9,            X10,           Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      0,             0,             0,             },
    {0,             Y3,            X6,            X7,            Y7,            X8,            Y8,            X9,            Y9,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      0,             0,             0,             },
    {0,             X3,            Y3,            X6,            X7,            Y7,            X8,            Y8,            X9,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      0,             0,             0,             },
    {0,             Y2,            X3,            Y3,            X6,            X7,            Y7,            X8,            Y8,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      0,             0,             0,             },
    {0,             X2,            X3,            Y3,            X6,            X7,            Y7,            Y2,            X8,            Y4^X8^Y8,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      Y2^X6^Y6,      0,             0,             0,             },
    {0,             X7,            Y7,            X8,            Y8,            X9,            Y9,            X10,           Y10,           Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X6^X9^Y9,      0,             0,             },
    {0,             Y3,            X7,            Y7,            X8,            Y8,            X9,            Y9,            X10,           Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X6^X9^Y9,      0,             0,             },
    {0,             X3,            Y3,            X7,            Y7,            X8,            Y8,            X9,            Y9,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X6^X9^Y9,      0,             0,             },
    {0,             Y2,            Y3,            X6,            X7,            Y7,            X8,            Y8,            X9,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X3^X9^Y9,      0,             0,             },
    {0,             X2,            Y3,            X6,            X7,            Y7,            X8,            Y2,            Y8,            Y4^X8^Y8,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      Y2^X6^Y6,      X3^X9^Y9,      0,             0,             },
    {0,             X7,            Y7,            X8,            Y8,            X9,            Y9,            X10,           Y10,           Y4^X9^Y9,      X4^Y4^Z4,      Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      X6^Y7,         0,             0,             },
    {0,             Y3,            X7,            Y7,            X8,            Y8,            X9,            Y9,            X10,           Y4^X9^Y9,      X4^Y4^Z4,      Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      X6^Y7,         0,             0,             },
    {0,             X3,            Y3,            X7,            Y7,            X8,            Y8,            X9,            Y9,            Y4^X9^Y9,      X4^Y4^Z4,      Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      X6^Y7,         0,             0,             },
    {0,             X3,            Y3,            X7,            Y7,            X8,            Y8,            Y2,            X9,            Y4^X9^Y9,      Z3^X4^Y4,      Z2^Y5^X8,      Z1^X5^Y8,      Y2^Y6^X7,      X6^Y7,         0,             0,             },
    {0,             X3,            Y3,            X7,            Y7,            X8,            Y8,            X2,            Y2,            Y4^X9^Y9,      Z2^X4^Y4,      Z1^Y5^X8,      Z0^X5^Y8,      Y2^Y6^X7,      X6^Y7,         0,             0,             },
    {0,             X7,            Y7,            X8,            Y8,            X9,            Y9,            X10,           Y10,           Y4^X9^Y9,      X4^Y4^Z4,      Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      Z0^X6^Y7,      0,             0,             },
    {0,             Y3,            X7,            Y7,            X8,            Y8,            X9,            Y9,            X10,           Y4^X9^Y9,      X4^Y4^Z4,      Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      Z0^X6^Y7,      0,             0,             },
    {0,             X3,            Y3,            X7,            Y7,            X8,            Y8,            X9,            Y9,            Y4^X9^Y9,      X4^Y4^Z4,      Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      Z0^X6^Y7,      0,             0,             },
    {0,             X3,            Y3,            X7,            Y7,            X8,            Y8,            Y2,            X9,            Y4^X9^Y9,      Z3^X4^Y4,      Z2^Y5^X8,      Z1^X5^Y8,      Y2^Y6^X7,      Z0^X6^Y7,      0,             0,             },
    {0,             X3,            Y3,            X7,            Y7,            X8,            Y8,            X2,            Y2,            Y4^X9^Y9,      Z2^X4^Y4,      Z1^Y5^X8,      Z0^X5^Y8,      Y2^Y6^X7,      X2^X6^Y7,      0,             0,             },
};

const UINT_64 HTILE_64K_SW_PATTERN[][18] =
{
    {0,             0,             0,             X3,            Y3,            X4,            Y4,            X5,            Y5,            X6,            Y6,            X7,            Y7,            0,             0,             0,             0,             0,             },
    {0,             0,             0,             X3,            Y4,            X4,            X5,            Y5,            X6,            Z0^X3^Y3,      Y6,            X7,            Y7,            0,             0,             0,             0,             0,             },
    {0,             0,             0,             X3,            Y4,            X5,            Y5,            X6,            Y6,            Z1^X3^Y3,      Z0^X4^Y4,      X7,            Y7,            X8,            0,             0,             0,             0,             },
    {0,             0,             0,             X3,            Y4,            Y5,            X6,            Y6,            X7,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      Y7,            X8,            Y8,            0,             0,             0,             },
    {0,             0,             0,             X3,            Y4,            X6,            Y6,            X7,            Y7,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X8,            Y8,            X9,            0,             0,             },
    {0,             0,             0,             X3,            Y4,            X6,            Y6,            X7,            Y7,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         X8,            Y8,            X9,            0,             0,             },
    {0,             0,             0,             X3,            Y4,            Y6,            X7,            Y7,            X8,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      Y8,            X9,            Y9,            0,             },
    {0,             0,             0,             X3,            Y4,            Y6,            X7,            Y7,            X8,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      X6^Y6,         Y8,            X9,            Y9,            0,             },
    {0,             0,             0,             X3,            Y4,            Y6,            X7,            Y7,            X8,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X6^Y6,         Y8,            X9,            Y9,            0,             },
    {0,             0,             0,             X3,            Y4,            X7,            Y7,            X8,            Y8,            X3^Y3^Z5,      X4^Y4^Z4,      Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      Z0^X6^Y7,      X9,            Y9,            X10,           },
    {0,             0,             0,             X3,            Y4,            X7,            Y7,            X8,            Y8,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X8,      Z1^X5^Y8,      Z0^Y6^X7,      X6^Y7,         X9,            Y9,            X10,           },
    {0,             0,             0,             X3,            Y4,            X7,            Y7,            X8,            Y8,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X8,      Z0^X5^Y8,      Y6^X7,         X6^Y7,         X9,            Y9,            X10,           },
    {0,             0,             0,             X3,            Y4,            X7,            Y7,            X8,            Y8,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X8,      X5^Y8,         Y6^X7,         X6^Y7,         X9,            Y9,            X10,           },
    {0,             0,             0,             X3,            Y3,            Y4,            X5,            Y5,            X6,            Z0^X4^Y4,      Y6,            X7,            Y7,            0,             0,             0,             0,             0,             },
    {0,             0,             0,             X3,            Y3,            X5,            Y5,            X6,            Y6,            Y4^X5^Y5,      Z0^X4^Y4,      X7,            Y7,            X8,            0,             0,             0,             0,             },
    {0,             0,             0,             X3,            Y3,            Y5,            X6,            Y6,            X7,            Y4^X5^Y5,      Z0^X4^Y4,      X5^Y5,         Y7,            X8,            Y8,            0,             0,             0,             },
    {0,             0,             0,             X3,            Y3,            X5,            X6,            Y6,            X7,            Y4^X6^Y6,      Z1^X4^Y4,      Y7,            X8,            Y8,            X5^Y5,         0,             0,             0,             },
    {0,             0,             0,             X3,            Y3,            X5,            X6,            Y6,            X7,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      Y7,            X8,            Y8,            0,             0,             0,             },
    {0,             0,             0,             X3,            Y3,            X6,            Y6,            X7,            Y7,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^Y6,         X8,            Y8,            X9,            0,             0,             },
    {0,             0,             0,             X3,            Y3,            Y4,            X5,            X6,            Y6,            Z1^X4^Y4,      Z0^X5^Y5,      X7,            Y7,            X8,            0,             0,             0,             0,             },
    {0,             0,             0,             X3,            Y3,            X6,            Y6,            X7,            Y7,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X8,            Y8,            X9,            X5^Y6,         0,             0,             },
    {0,             0,             0,             X3,            Y3,            X6,            Y6,            X7,            Y7,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         X8,            Y8,            X9,            0,             0,             },
    {0,             0,             0,             X3,            Y3,            Y6,            X7,            Y7,            X8,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         X6^Y6,         Y8,            X9,            Y9,            0,             },
    {0,             0,             0,             X3,            Y3,            Y4,            X6,            Y6,            X7,            Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         Y7,            X8,            Y8,            0,             0,             0,             },
    {0,             0,             0,             X3,            Y3,            X6,            X7,            Y7,            X8,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         Y8,            X9,            Y9,            X6^Y6,         0,             },
    {0,             0,             0,             X3,            Y3,            X6,            X7,            Y7,            X8,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X6^Y6,         Y8,            X9,            Y9,            0,             },
    {0,             0,             0,             X3,            Y3,            X7,            Y7,            X8,            Y8,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X6^Y6,         X6^Y8,         X9,            Y9,            X10,           },
    {0,             0,             0,             X3,            Y3,            Y4,            X6,            X7,            Y7,            Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X6^Y6,         X8,            Y8,            X9,            0,             0,             },
    {0,             0,             0,             X3,            Y3,            X7,            Y7,            X8,            Y8,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      X5^Y8,         Y6^X7,         X9,            Y9,            X10,           X6^Y7,         },
    {0,             0,             0,             X3,            Y3,            X7,            Y7,            X8,            Y8,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      X5^Y8,         Y6^X7,         X6^Y7,         X9,            Y9,            X10,           },
};

const UINT_64 CMASK_64K_SW_PATTERN[][17] =
{
    {X3,            Y3,            X4,            Y4,            X5,            Y5,            X6,            Y6,            X7,            Y7,            X8,            Y8,            X9,            0,             0,             0,             0,             },
    {X3,            Y4,            X4,            X5,            Y5,            X6,            Y6,            X7,            Y7,            Z0^X3^Y3,      X8,            Y8,            X9,            0,             0,             0,             0,             },
    {X3,            Y4,            X5,            Y5,            X6,            Y6,            X7,            Y7,            X8,            Z1^X3^Y3,      Z0^X4^Y4,      Y8,            X9,            0,             0,             0,             0,             },
    {X3,            Y4,            Y5,            X6,            Y6,            X7,            Y7,            X8,            Y8,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X9,            0,             0,             0,             0,             },
    {X3,            Y4,            X6,            Y6,            X7,            Y7,            X8,            Y8,            X9,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      0,             0,             0,             0,             },
    {X3,            Y4,            Y6,            X7,            Y7,            X8,            Y8,            X9,            Y9,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      0,             0,             0,             },
    {X3,            Y4,            X7,            Y7,            X8,            Y8,            X9,            Y9,            X10,           X3^Y3^Z5,      X4^Y4^Z4,      Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      Z0^X6^Y7,      0,             0,             },
    {X3,            Y4,            X7,            Y7,            X8,            Y8,            X9,            Y9,            X10,           X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X8,      Z1^X5^Y8,      Y6^X7,         Z0^X6^Y7,      0,             0,             },
    {X3,            Y3,            Y4,            X5,            Y5,            X6,            Y6,            X7,            Y7,            Z0^X4^Y4,      X8,            Y8,            X9,            0,             0,             0,             0,             },
    {X3,            Y3,            X5,            Y5,            X6,            Y6,            X7,            Y7,            X8,            Y4^X5^Y5,      Z0^X4^Y4,      Y8,            X9,            0,             0,             0,             0,             },
    {X3,            Y3,            Y5,            X6,            Y6,            X7,            Y7,            X8,            Y8,            Y4^X5^Y5,      Z0^X4^Y4,      X5^Y5,         X9,            0,             0,             0,             0,             },
    {X3,            Y3,            X5,            X6,            Y6,            X7,            Y7,            X8,            Y8,            Y4^X6^Y6,      Z1^X4^Y4,      X5^Y5,         X9,            0,             0,             0,             0,             },
    {X3,            Y3,            X5,            X6,            Y6,            X7,            Y7,            X8,            Y8,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X9,            0,             0,             0,             0,             },
    {X3,            Y3,            X6,            Y6,            X7,            Y7,            X8,            Y8,            X9,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^Y6,         0,             0,             0,             0,             },
    {X3,            Y3,            Y4,            X5,            X6,            Y6,            X7,            Y7,            X8,            Z1^X4^Y4,      Z0^X5^Y5,      Y8,            X9,            0,             0,             0,             0,             },
    {X3,            Y3,            X6,            Y6,            X7,            Y7,            X8,            Y8,            X9,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         0,             0,             0,             0,             },
    {X3,            Y3,            X6,            Y6,            X7,            Y7,            X8,            Y8,            X9,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      X5^Y6,         0,             0,             0,             0,             },
    {X3,            Y3,            X6,            Y6,            X7,            Y7,            X8,            Y8,            X9,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      0,             0,             0,             0,             },
    {X3,            Y3,            Y6,            X7,            Y7,            X8,            Y8,            X9,            Y9,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         X6^Y6,         0,             0,             0,             },
    {X3,            Y3,            Y6,            X7,            Y7,            X8,            Y8,            X9,            Y9,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X6^Y6,         0,             0,             0,             },
    {X3,            Y3,            Y4,            X6,            Y6,            X7,            Y7,            X8,            Y8,            Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         X9,            0,             0,             0,             0,             },
    {X3,            Y3,            Y4,            X6,            Y6,            X7,            Y7,            X8,            Y8,            Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X9,            0,             0,             0,             0,             },
    {X3,            Y3,            X6,            X7,            Y7,            X8,            Y8,            X9,            Y9,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X6^Y6,         0,             0,             0,             },
    {X3,            Y3,            X6,            X7,            Y7,            X8,            Y8,            X9,            Y9,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      X6^Y6,         0,             0,             0,             },
    {X3,            Y3,            X6,            X7,            Y7,            X8,            Y8,            X9,            Y9,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      0,             0,             0,             },
    {X3,            Y3,            X7,            Y7,            X8,            Y8,            X9,            Y9,            X10,           Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X6^Y6,         X6^Y8,         0,             0,             },
    {X3,            Y3,            X6,            X7,            Y7,            X8,            X9,            Y9,            X10,           Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X3^Y8,         0,             0,             },
    {X3,            Y3,            Y4,            X6,            X7,            Y7,            X8,            Y8,            X9,            Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X6^Y6,         0,             0,             0,             0,             },
    {X3,            Y3,            Y4,            X6,            X7,            Y7,            X8,            Y8,            X9,            Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      0,             0,             0,             0,             },
    {X3,            Y3,            X7,            Y7,            X8,            Y8,            X9,            Y9,            X10,           Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      X5^Y8,         Y6^X7,         X6^Y7,         0,             0,             },
    {X3,            Y3,            X7,            Y7,            X8,            Y8,            X9,            Y9,            X10,           Y4^X9^Y9,      Z3^X4^Y4,      Z2^Y5^X8,      Z1^X5^Y8,      Y6^X7,         X6^Y7,         0,             0,             },
    {X3,            Y3,            X7,            Y7,            X8,            Y8,            X9,            Y9,            X10,           Y4^X9^Y9,      Z3^X4^Y4,      Z2^Y5^X8,      Z1^X5^Y8,      Y6^X7,         Z0^X6^Y7,      0,             0,             },
};

} // V2
} // Addr

#endif
