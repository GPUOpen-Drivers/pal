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

const UINT_16 SW_256_S_PATIDX[] =
{
       0, // 1 pipes 1 bpe @ SW_256_S @ Navi1x
       1, // 1 pipes 2 bpe @ SW_256_S @ Navi1x
       2, // 1 pipes 4 bpe @ SW_256_S @ Navi1x
       3, // 1 pipes 8 bpe @ SW_256_S @ Navi1x
       4, // 1 pipes 16 bpe @ SW_256_S @ Navi1x
       0, // 2 pipes 1 bpe @ SW_256_S @ Navi1x
       1, // 2 pipes 2 bpe @ SW_256_S @ Navi1x
       2, // 2 pipes 4 bpe @ SW_256_S @ Navi1x
       3, // 2 pipes 8 bpe @ SW_256_S @ Navi1x
       4, // 2 pipes 16 bpe @ SW_256_S @ Navi1x
       0, // 4 pipes 1 bpe @ SW_256_S @ Navi1x
       1, // 4 pipes 2 bpe @ SW_256_S @ Navi1x
       2, // 4 pipes 4 bpe @ SW_256_S @ Navi1x
       3, // 4 pipes 8 bpe @ SW_256_S @ Navi1x
       4, // 4 pipes 16 bpe @ SW_256_S @ Navi1x
       0, // 8 pipes 1 bpe @ SW_256_S @ Navi1x
       1, // 8 pipes 2 bpe @ SW_256_S @ Navi1x
       2, // 8 pipes 4 bpe @ SW_256_S @ Navi1x
       3, // 8 pipes 8 bpe @ SW_256_S @ Navi1x
       4, // 8 pipes 16 bpe @ SW_256_S @ Navi1x
       0, // 16 pipes 1 bpe @ SW_256_S @ Navi1x
       1, // 16 pipes 2 bpe @ SW_256_S @ Navi1x
       2, // 16 pipes 4 bpe @ SW_256_S @ Navi1x
       3, // 16 pipes 8 bpe @ SW_256_S @ Navi1x
       4, // 16 pipes 16 bpe @ SW_256_S @ Navi1x
       0, // 32 pipes 1 bpe @ SW_256_S @ Navi1x
       1, // 32 pipes 2 bpe @ SW_256_S @ Navi1x
       2, // 32 pipes 4 bpe @ SW_256_S @ Navi1x
       3, // 32 pipes 8 bpe @ SW_256_S @ Navi1x
       4, // 32 pipes 16 bpe @ SW_256_S @ Navi1x
       0, // 64 pipes 1 bpe @ SW_256_S @ Navi1x
       1, // 64 pipes 2 bpe @ SW_256_S @ Navi1x
       2, // 64 pipes 4 bpe @ SW_256_S @ Navi1x
       3, // 64 pipes 8 bpe @ SW_256_S @ Navi1x
       4, // 64 pipes 16 bpe @ SW_256_S @ Navi1x
};

const UINT_16 SW_256_D_PATIDX[] =
{
      95, // 1 pipes 1 bpe @ SW_256_D @ Navi1x
       1, // 1 pipes 2 bpe @ SW_256_D @ Navi1x
       2, // 1 pipes 4 bpe @ SW_256_D @ Navi1x
      96, // 1 pipes 8 bpe @ SW_256_D @ Navi1x
      97, // 1 pipes 16 bpe @ SW_256_D @ Navi1x
      95, // 2 pipes 1 bpe @ SW_256_D @ Navi1x
       1, // 2 pipes 2 bpe @ SW_256_D @ Navi1x
       2, // 2 pipes 4 bpe @ SW_256_D @ Navi1x
      96, // 2 pipes 8 bpe @ SW_256_D @ Navi1x
      97, // 2 pipes 16 bpe @ SW_256_D @ Navi1x
      95, // 4 pipes 1 bpe @ SW_256_D @ Navi1x
       1, // 4 pipes 2 bpe @ SW_256_D @ Navi1x
       2, // 4 pipes 4 bpe @ SW_256_D @ Navi1x
      96, // 4 pipes 8 bpe @ SW_256_D @ Navi1x
      97, // 4 pipes 16 bpe @ SW_256_D @ Navi1x
      95, // 8 pipes 1 bpe @ SW_256_D @ Navi1x
       1, // 8 pipes 2 bpe @ SW_256_D @ Navi1x
       2, // 8 pipes 4 bpe @ SW_256_D @ Navi1x
      96, // 8 pipes 8 bpe @ SW_256_D @ Navi1x
      97, // 8 pipes 16 bpe @ SW_256_D @ Navi1x
      95, // 16 pipes 1 bpe @ SW_256_D @ Navi1x
       1, // 16 pipes 2 bpe @ SW_256_D @ Navi1x
       2, // 16 pipes 4 bpe @ SW_256_D @ Navi1x
      96, // 16 pipes 8 bpe @ SW_256_D @ Navi1x
      97, // 16 pipes 16 bpe @ SW_256_D @ Navi1x
      95, // 32 pipes 1 bpe @ SW_256_D @ Navi1x
       1, // 32 pipes 2 bpe @ SW_256_D @ Navi1x
       2, // 32 pipes 4 bpe @ SW_256_D @ Navi1x
      96, // 32 pipes 8 bpe @ SW_256_D @ Navi1x
      97, // 32 pipes 16 bpe @ SW_256_D @ Navi1x
      95, // 64 pipes 1 bpe @ SW_256_D @ Navi1x
       1, // 64 pipes 2 bpe @ SW_256_D @ Navi1x
       2, // 64 pipes 4 bpe @ SW_256_D @ Navi1x
      96, // 64 pipes 8 bpe @ SW_256_D @ Navi1x
      97, // 64 pipes 16 bpe @ SW_256_D @ Navi1x
};

const UINT_16 SW_4K_S_PATIDX[] =
{
       5, // 1 pipes 1 bpe @ SW_4K_S @ Navi1x
       6, // 1 pipes 2 bpe @ SW_4K_S @ Navi1x
       7, // 1 pipes 4 bpe @ SW_4K_S @ Navi1x
       8, // 1 pipes 8 bpe @ SW_4K_S @ Navi1x
       9, // 1 pipes 16 bpe @ SW_4K_S @ Navi1x
       5, // 2 pipes 1 bpe @ SW_4K_S @ Navi1x
       6, // 2 pipes 2 bpe @ SW_4K_S @ Navi1x
       7, // 2 pipes 4 bpe @ SW_4K_S @ Navi1x
       8, // 2 pipes 8 bpe @ SW_4K_S @ Navi1x
       9, // 2 pipes 16 bpe @ SW_4K_S @ Navi1x
       5, // 4 pipes 1 bpe @ SW_4K_S @ Navi1x
       6, // 4 pipes 2 bpe @ SW_4K_S @ Navi1x
       7, // 4 pipes 4 bpe @ SW_4K_S @ Navi1x
       8, // 4 pipes 8 bpe @ SW_4K_S @ Navi1x
       9, // 4 pipes 16 bpe @ SW_4K_S @ Navi1x
       5, // 8 pipes 1 bpe @ SW_4K_S @ Navi1x
       6, // 8 pipes 2 bpe @ SW_4K_S @ Navi1x
       7, // 8 pipes 4 bpe @ SW_4K_S @ Navi1x
       8, // 8 pipes 8 bpe @ SW_4K_S @ Navi1x
       9, // 8 pipes 16 bpe @ SW_4K_S @ Navi1x
       5, // 16 pipes 1 bpe @ SW_4K_S @ Navi1x
       6, // 16 pipes 2 bpe @ SW_4K_S @ Navi1x
       7, // 16 pipes 4 bpe @ SW_4K_S @ Navi1x
       8, // 16 pipes 8 bpe @ SW_4K_S @ Navi1x
       9, // 16 pipes 16 bpe @ SW_4K_S @ Navi1x
       5, // 32 pipes 1 bpe @ SW_4K_S @ Navi1x
       6, // 32 pipes 2 bpe @ SW_4K_S @ Navi1x
       7, // 32 pipes 4 bpe @ SW_4K_S @ Navi1x
       8, // 32 pipes 8 bpe @ SW_4K_S @ Navi1x
       9, // 32 pipes 16 bpe @ SW_4K_S @ Navi1x
       5, // 64 pipes 1 bpe @ SW_4K_S @ Navi1x
       6, // 64 pipes 2 bpe @ SW_4K_S @ Navi1x
       7, // 64 pipes 4 bpe @ SW_4K_S @ Navi1x
       8, // 64 pipes 8 bpe @ SW_4K_S @ Navi1x
       9, // 64 pipes 16 bpe @ SW_4K_S @ Navi1x
};

const UINT_16 SW_4K_D_PATIDX[] =
{
      98, // 1 pipes 1 bpe @ SW_4K_D @ Navi1x
       6, // 1 pipes 2 bpe @ SW_4K_D @ Navi1x
       7, // 1 pipes 4 bpe @ SW_4K_D @ Navi1x
      99, // 1 pipes 8 bpe @ SW_4K_D @ Navi1x
     100, // 1 pipes 16 bpe @ SW_4K_D @ Navi1x
      98, // 2 pipes 1 bpe @ SW_4K_D @ Navi1x
       6, // 2 pipes 2 bpe @ SW_4K_D @ Navi1x
       7, // 2 pipes 4 bpe @ SW_4K_D @ Navi1x
      99, // 2 pipes 8 bpe @ SW_4K_D @ Navi1x
     100, // 2 pipes 16 bpe @ SW_4K_D @ Navi1x
      98, // 4 pipes 1 bpe @ SW_4K_D @ Navi1x
       6, // 4 pipes 2 bpe @ SW_4K_D @ Navi1x
       7, // 4 pipes 4 bpe @ SW_4K_D @ Navi1x
      99, // 4 pipes 8 bpe @ SW_4K_D @ Navi1x
     100, // 4 pipes 16 bpe @ SW_4K_D @ Navi1x
      98, // 8 pipes 1 bpe @ SW_4K_D @ Navi1x
       6, // 8 pipes 2 bpe @ SW_4K_D @ Navi1x
       7, // 8 pipes 4 bpe @ SW_4K_D @ Navi1x
      99, // 8 pipes 8 bpe @ SW_4K_D @ Navi1x
     100, // 8 pipes 16 bpe @ SW_4K_D @ Navi1x
      98, // 16 pipes 1 bpe @ SW_4K_D @ Navi1x
       6, // 16 pipes 2 bpe @ SW_4K_D @ Navi1x
       7, // 16 pipes 4 bpe @ SW_4K_D @ Navi1x
      99, // 16 pipes 8 bpe @ SW_4K_D @ Navi1x
     100, // 16 pipes 16 bpe @ SW_4K_D @ Navi1x
      98, // 32 pipes 1 bpe @ SW_4K_D @ Navi1x
       6, // 32 pipes 2 bpe @ SW_4K_D @ Navi1x
       7, // 32 pipes 4 bpe @ SW_4K_D @ Navi1x
      99, // 32 pipes 8 bpe @ SW_4K_D @ Navi1x
     100, // 32 pipes 16 bpe @ SW_4K_D @ Navi1x
      98, // 64 pipes 1 bpe @ SW_4K_D @ Navi1x
       6, // 64 pipes 2 bpe @ SW_4K_D @ Navi1x
       7, // 64 pipes 4 bpe @ SW_4K_D @ Navi1x
      99, // 64 pipes 8 bpe @ SW_4K_D @ Navi1x
     100, // 64 pipes 16 bpe @ SW_4K_D @ Navi1x
};

const UINT_16 SW_4K_S_X_PATIDX[] =
{
       5, // 1 pipes 1 bpe @ SW_4K_S_X @ Navi1x
       6, // 1 pipes 2 bpe @ SW_4K_S_X @ Navi1x
       7, // 1 pipes 4 bpe @ SW_4K_S_X @ Navi1x
       8, // 1 pipes 8 bpe @ SW_4K_S_X @ Navi1x
       9, // 1 pipes 16 bpe @ SW_4K_S_X @ Navi1x
      10, // 2 pipes 1 bpe @ SW_4K_S_X @ Navi1x
      11, // 2 pipes 2 bpe @ SW_4K_S_X @ Navi1x
      12, // 2 pipes 4 bpe @ SW_4K_S_X @ Navi1x
      13, // 2 pipes 8 bpe @ SW_4K_S_X @ Navi1x
      14, // 2 pipes 16 bpe @ SW_4K_S_X @ Navi1x
      15, // 4 pipes 1 bpe @ SW_4K_S_X @ Navi1x
      16, // 4 pipes 2 bpe @ SW_4K_S_X @ Navi1x
      17, // 4 pipes 4 bpe @ SW_4K_S_X @ Navi1x
      18, // 4 pipes 8 bpe @ SW_4K_S_X @ Navi1x
      19, // 4 pipes 16 bpe @ SW_4K_S_X @ Navi1x
      20, // 8 pipes 1 bpe @ SW_4K_S_X @ Navi1x
      21, // 8 pipes 2 bpe @ SW_4K_S_X @ Navi1x
      22, // 8 pipes 4 bpe @ SW_4K_S_X @ Navi1x
      23, // 8 pipes 8 bpe @ SW_4K_S_X @ Navi1x
      24, // 8 pipes 16 bpe @ SW_4K_S_X @ Navi1x
      25, // 16 pipes 1 bpe @ SW_4K_S_X @ Navi1x
      26, // 16 pipes 2 bpe @ SW_4K_S_X @ Navi1x
      27, // 16 pipes 4 bpe @ SW_4K_S_X @ Navi1x
      28, // 16 pipes 8 bpe @ SW_4K_S_X @ Navi1x
      29, // 16 pipes 16 bpe @ SW_4K_S_X @ Navi1x
      25, // 32 pipes 1 bpe @ SW_4K_S_X @ Navi1x
      26, // 32 pipes 2 bpe @ SW_4K_S_X @ Navi1x
      27, // 32 pipes 4 bpe @ SW_4K_S_X @ Navi1x
      28, // 32 pipes 8 bpe @ SW_4K_S_X @ Navi1x
      29, // 32 pipes 16 bpe @ SW_4K_S_X @ Navi1x
      25, // 64 pipes 1 bpe @ SW_4K_S_X @ Navi1x
      26, // 64 pipes 2 bpe @ SW_4K_S_X @ Navi1x
      27, // 64 pipes 4 bpe @ SW_4K_S_X @ Navi1x
      28, // 64 pipes 8 bpe @ SW_4K_S_X @ Navi1x
      29, // 64 pipes 16 bpe @ SW_4K_S_X @ Navi1x
};

const UINT_16 SW_4K_D_X_PATIDX[] =
{
      98, // 1 pipes 1 bpe @ SW_4K_D_X @ Navi1x
       6, // 1 pipes 2 bpe @ SW_4K_D_X @ Navi1x
       7, // 1 pipes 4 bpe @ SW_4K_D_X @ Navi1x
      99, // 1 pipes 8 bpe @ SW_4K_D_X @ Navi1x
     100, // 1 pipes 16 bpe @ SW_4K_D_X @ Navi1x
     101, // 2 pipes 1 bpe @ SW_4K_D_X @ Navi1x
      11, // 2 pipes 2 bpe @ SW_4K_D_X @ Navi1x
      12, // 2 pipes 4 bpe @ SW_4K_D_X @ Navi1x
     102, // 2 pipes 8 bpe @ SW_4K_D_X @ Navi1x
     103, // 2 pipes 16 bpe @ SW_4K_D_X @ Navi1x
     104, // 4 pipes 1 bpe @ SW_4K_D_X @ Navi1x
      16, // 4 pipes 2 bpe @ SW_4K_D_X @ Navi1x
      17, // 4 pipes 4 bpe @ SW_4K_D_X @ Navi1x
     105, // 4 pipes 8 bpe @ SW_4K_D_X @ Navi1x
     106, // 4 pipes 16 bpe @ SW_4K_D_X @ Navi1x
     107, // 8 pipes 1 bpe @ SW_4K_D_X @ Navi1x
      21, // 8 pipes 2 bpe @ SW_4K_D_X @ Navi1x
      22, // 8 pipes 4 bpe @ SW_4K_D_X @ Navi1x
     108, // 8 pipes 8 bpe @ SW_4K_D_X @ Navi1x
     109, // 8 pipes 16 bpe @ SW_4K_D_X @ Navi1x
     110, // 16 pipes 1 bpe @ SW_4K_D_X @ Navi1x
      26, // 16 pipes 2 bpe @ SW_4K_D_X @ Navi1x
      27, // 16 pipes 4 bpe @ SW_4K_D_X @ Navi1x
     111, // 16 pipes 8 bpe @ SW_4K_D_X @ Navi1x
     112, // 16 pipes 16 bpe @ SW_4K_D_X @ Navi1x
     110, // 32 pipes 1 bpe @ SW_4K_D_X @ Navi1x
      26, // 32 pipes 2 bpe @ SW_4K_D_X @ Navi1x
      27, // 32 pipes 4 bpe @ SW_4K_D_X @ Navi1x
     111, // 32 pipes 8 bpe @ SW_4K_D_X @ Navi1x
     112, // 32 pipes 16 bpe @ SW_4K_D_X @ Navi1x
     110, // 64 pipes 1 bpe @ SW_4K_D_X @ Navi1x
      26, // 64 pipes 2 bpe @ SW_4K_D_X @ Navi1x
      27, // 64 pipes 4 bpe @ SW_4K_D_X @ Navi1x
     111, // 64 pipes 8 bpe @ SW_4K_D_X @ Navi1x
     112, // 64 pipes 16 bpe @ SW_4K_D_X @ Navi1x
};

const UINT_16 SW_4K_S3_PATIDX[] =
{
     419, // 1 pipes 1 bpe @ SW_4K_S3 @ Navi1x
     420, // 1 pipes 2 bpe @ SW_4K_S3 @ Navi1x
     421, // 1 pipes 4 bpe @ SW_4K_S3 @ Navi1x
     422, // 1 pipes 8 bpe @ SW_4K_S3 @ Navi1x
     423, // 1 pipes 16 bpe @ SW_4K_S3 @ Navi1x
     419, // 2 pipes 1 bpe @ SW_4K_S3 @ Navi1x
     420, // 2 pipes 2 bpe @ SW_4K_S3 @ Navi1x
     421, // 2 pipes 4 bpe @ SW_4K_S3 @ Navi1x
     422, // 2 pipes 8 bpe @ SW_4K_S3 @ Navi1x
     423, // 2 pipes 16 bpe @ SW_4K_S3 @ Navi1x
     419, // 4 pipes 1 bpe @ SW_4K_S3 @ Navi1x
     420, // 4 pipes 2 bpe @ SW_4K_S3 @ Navi1x
     421, // 4 pipes 4 bpe @ SW_4K_S3 @ Navi1x
     422, // 4 pipes 8 bpe @ SW_4K_S3 @ Navi1x
     423, // 4 pipes 16 bpe @ SW_4K_S3 @ Navi1x
     419, // 8 pipes 1 bpe @ SW_4K_S3 @ Navi1x
     420, // 8 pipes 2 bpe @ SW_4K_S3 @ Navi1x
     421, // 8 pipes 4 bpe @ SW_4K_S3 @ Navi1x
     422, // 8 pipes 8 bpe @ SW_4K_S3 @ Navi1x
     423, // 8 pipes 16 bpe @ SW_4K_S3 @ Navi1x
     419, // 16 pipes 1 bpe @ SW_4K_S3 @ Navi1x
     420, // 16 pipes 2 bpe @ SW_4K_S3 @ Navi1x
     421, // 16 pipes 4 bpe @ SW_4K_S3 @ Navi1x
     422, // 16 pipes 8 bpe @ SW_4K_S3 @ Navi1x
     423, // 16 pipes 16 bpe @ SW_4K_S3 @ Navi1x
     419, // 32 pipes 1 bpe @ SW_4K_S3 @ Navi1x
     420, // 32 pipes 2 bpe @ SW_4K_S3 @ Navi1x
     421, // 32 pipes 4 bpe @ SW_4K_S3 @ Navi1x
     422, // 32 pipes 8 bpe @ SW_4K_S3 @ Navi1x
     423, // 32 pipes 16 bpe @ SW_4K_S3 @ Navi1x
     419, // 64 pipes 1 bpe @ SW_4K_S3 @ Navi1x
     420, // 64 pipes 2 bpe @ SW_4K_S3 @ Navi1x
     421, // 64 pipes 4 bpe @ SW_4K_S3 @ Navi1x
     422, // 64 pipes 8 bpe @ SW_4K_S3 @ Navi1x
     423, // 64 pipes 16 bpe @ SW_4K_S3 @ Navi1x
};

const UINT_16 SW_4K_S3_X_PATIDX[] =
{
     419, // 1 pipes 1 bpe @ SW_4K_S3_X @ Navi1x
     420, // 1 pipes 2 bpe @ SW_4K_S3_X @ Navi1x
     421, // 1 pipes 4 bpe @ SW_4K_S3_X @ Navi1x
     422, // 1 pipes 8 bpe @ SW_4K_S3_X @ Navi1x
     423, // 1 pipes 16 bpe @ SW_4K_S3_X @ Navi1x
     424, // 2 pipes 1 bpe @ SW_4K_S3_X @ Navi1x
     425, // 2 pipes 2 bpe @ SW_4K_S3_X @ Navi1x
     426, // 2 pipes 4 bpe @ SW_4K_S3_X @ Navi1x
     427, // 2 pipes 8 bpe @ SW_4K_S3_X @ Navi1x
     428, // 2 pipes 16 bpe @ SW_4K_S3_X @ Navi1x
     429, // 4 pipes 1 bpe @ SW_4K_S3_X @ Navi1x
     430, // 4 pipes 2 bpe @ SW_4K_S3_X @ Navi1x
     431, // 4 pipes 4 bpe @ SW_4K_S3_X @ Navi1x
     432, // 4 pipes 8 bpe @ SW_4K_S3_X @ Navi1x
     433, // 4 pipes 16 bpe @ SW_4K_S3_X @ Navi1x
     434, // 8 pipes 1 bpe @ SW_4K_S3_X @ Navi1x
     435, // 8 pipes 2 bpe @ SW_4K_S3_X @ Navi1x
     436, // 8 pipes 4 bpe @ SW_4K_S3_X @ Navi1x
     437, // 8 pipes 8 bpe @ SW_4K_S3_X @ Navi1x
     438, // 8 pipes 16 bpe @ SW_4K_S3_X @ Navi1x
     439, // 16 pipes 1 bpe @ SW_4K_S3_X @ Navi1x
     440, // 16 pipes 2 bpe @ SW_4K_S3_X @ Navi1x
     441, // 16 pipes 4 bpe @ SW_4K_S3_X @ Navi1x
     442, // 16 pipes 8 bpe @ SW_4K_S3_X @ Navi1x
     443, // 16 pipes 16 bpe @ SW_4K_S3_X @ Navi1x
     439, // 32 pipes 1 bpe @ SW_4K_S3_X @ Navi1x
     440, // 32 pipes 2 bpe @ SW_4K_S3_X @ Navi1x
     441, // 32 pipes 4 bpe @ SW_4K_S3_X @ Navi1x
     442, // 32 pipes 8 bpe @ SW_4K_S3_X @ Navi1x
     443, // 32 pipes 16 bpe @ SW_4K_S3_X @ Navi1x
     439, // 64 pipes 1 bpe @ SW_4K_S3_X @ Navi1x
     440, // 64 pipes 2 bpe @ SW_4K_S3_X @ Navi1x
     441, // 64 pipes 4 bpe @ SW_4K_S3_X @ Navi1x
     442, // 64 pipes 8 bpe @ SW_4K_S3_X @ Navi1x
     443, // 64 pipes 16 bpe @ SW_4K_S3_X @ Navi1x
};

const UINT_16 SW_64K_S_PATIDX[] =
{
      30, // 1 pipes 1 bpe @ SW_64K_S @ Navi1x
      31, // 1 pipes 2 bpe @ SW_64K_S @ Navi1x
      32, // 1 pipes 4 bpe @ SW_64K_S @ Navi1x
      33, // 1 pipes 8 bpe @ SW_64K_S @ Navi1x
      34, // 1 pipes 16 bpe @ SW_64K_S @ Navi1x
      30, // 2 pipes 1 bpe @ SW_64K_S @ Navi1x
      31, // 2 pipes 2 bpe @ SW_64K_S @ Navi1x
      32, // 2 pipes 4 bpe @ SW_64K_S @ Navi1x
      33, // 2 pipes 8 bpe @ SW_64K_S @ Navi1x
      34, // 2 pipes 16 bpe @ SW_64K_S @ Navi1x
      30, // 4 pipes 1 bpe @ SW_64K_S @ Navi1x
      31, // 4 pipes 2 bpe @ SW_64K_S @ Navi1x
      32, // 4 pipes 4 bpe @ SW_64K_S @ Navi1x
      33, // 4 pipes 8 bpe @ SW_64K_S @ Navi1x
      34, // 4 pipes 16 bpe @ SW_64K_S @ Navi1x
      30, // 8 pipes 1 bpe @ SW_64K_S @ Navi1x
      31, // 8 pipes 2 bpe @ SW_64K_S @ Navi1x
      32, // 8 pipes 4 bpe @ SW_64K_S @ Navi1x
      33, // 8 pipes 8 bpe @ SW_64K_S @ Navi1x
      34, // 8 pipes 16 bpe @ SW_64K_S @ Navi1x
      30, // 16 pipes 1 bpe @ SW_64K_S @ Navi1x
      31, // 16 pipes 2 bpe @ SW_64K_S @ Navi1x
      32, // 16 pipes 4 bpe @ SW_64K_S @ Navi1x
      33, // 16 pipes 8 bpe @ SW_64K_S @ Navi1x
      34, // 16 pipes 16 bpe @ SW_64K_S @ Navi1x
      30, // 32 pipes 1 bpe @ SW_64K_S @ Navi1x
      31, // 32 pipes 2 bpe @ SW_64K_S @ Navi1x
      32, // 32 pipes 4 bpe @ SW_64K_S @ Navi1x
      33, // 32 pipes 8 bpe @ SW_64K_S @ Navi1x
      34, // 32 pipes 16 bpe @ SW_64K_S @ Navi1x
      30, // 64 pipes 1 bpe @ SW_64K_S @ Navi1x
      31, // 64 pipes 2 bpe @ SW_64K_S @ Navi1x
      32, // 64 pipes 4 bpe @ SW_64K_S @ Navi1x
      33, // 64 pipes 8 bpe @ SW_64K_S @ Navi1x
      34, // 64 pipes 16 bpe @ SW_64K_S @ Navi1x
};

const UINT_16 SW_64K_D_PATIDX[] =
{
     113, // 1 pipes 1 bpe @ SW_64K_D @ Navi1x
      31, // 1 pipes 2 bpe @ SW_64K_D @ Navi1x
      32, // 1 pipes 4 bpe @ SW_64K_D @ Navi1x
     114, // 1 pipes 8 bpe @ SW_64K_D @ Navi1x
     115, // 1 pipes 16 bpe @ SW_64K_D @ Navi1x
     113, // 2 pipes 1 bpe @ SW_64K_D @ Navi1x
      31, // 2 pipes 2 bpe @ SW_64K_D @ Navi1x
      32, // 2 pipes 4 bpe @ SW_64K_D @ Navi1x
     114, // 2 pipes 8 bpe @ SW_64K_D @ Navi1x
     115, // 2 pipes 16 bpe @ SW_64K_D @ Navi1x
     113, // 4 pipes 1 bpe @ SW_64K_D @ Navi1x
      31, // 4 pipes 2 bpe @ SW_64K_D @ Navi1x
      32, // 4 pipes 4 bpe @ SW_64K_D @ Navi1x
     114, // 4 pipes 8 bpe @ SW_64K_D @ Navi1x
     115, // 4 pipes 16 bpe @ SW_64K_D @ Navi1x
     113, // 8 pipes 1 bpe @ SW_64K_D @ Navi1x
      31, // 8 pipes 2 bpe @ SW_64K_D @ Navi1x
      32, // 8 pipes 4 bpe @ SW_64K_D @ Navi1x
     114, // 8 pipes 8 bpe @ SW_64K_D @ Navi1x
     115, // 8 pipes 16 bpe @ SW_64K_D @ Navi1x
     113, // 16 pipes 1 bpe @ SW_64K_D @ Navi1x
      31, // 16 pipes 2 bpe @ SW_64K_D @ Navi1x
      32, // 16 pipes 4 bpe @ SW_64K_D @ Navi1x
     114, // 16 pipes 8 bpe @ SW_64K_D @ Navi1x
     115, // 16 pipes 16 bpe @ SW_64K_D @ Navi1x
     113, // 32 pipes 1 bpe @ SW_64K_D @ Navi1x
      31, // 32 pipes 2 bpe @ SW_64K_D @ Navi1x
      32, // 32 pipes 4 bpe @ SW_64K_D @ Navi1x
     114, // 32 pipes 8 bpe @ SW_64K_D @ Navi1x
     115, // 32 pipes 16 bpe @ SW_64K_D @ Navi1x
     113, // 64 pipes 1 bpe @ SW_64K_D @ Navi1x
      31, // 64 pipes 2 bpe @ SW_64K_D @ Navi1x
      32, // 64 pipes 4 bpe @ SW_64K_D @ Navi1x
     114, // 64 pipes 8 bpe @ SW_64K_D @ Navi1x
     115, // 64 pipes 16 bpe @ SW_64K_D @ Navi1x
};

const UINT_16 SW_64K_S_T_PATIDX[] =
{
      30, // 1 pipes 1 bpe @ SW_64K_S_T @ Navi1x
      31, // 1 pipes 2 bpe @ SW_64K_S_T @ Navi1x
      32, // 1 pipes 4 bpe @ SW_64K_S_T @ Navi1x
      33, // 1 pipes 8 bpe @ SW_64K_S_T @ Navi1x
      34, // 1 pipes 16 bpe @ SW_64K_S_T @ Navi1x
      65, // 2 pipes 1 bpe @ SW_64K_S_T @ Navi1x
      66, // 2 pipes 2 bpe @ SW_64K_S_T @ Navi1x
      67, // 2 pipes 4 bpe @ SW_64K_S_T @ Navi1x
      68, // 2 pipes 8 bpe @ SW_64K_S_T @ Navi1x
      69, // 2 pipes 16 bpe @ SW_64K_S_T @ Navi1x
      70, // 4 pipes 1 bpe @ SW_64K_S_T @ Navi1x
      71, // 4 pipes 2 bpe @ SW_64K_S_T @ Navi1x
      72, // 4 pipes 4 bpe @ SW_64K_S_T @ Navi1x
      73, // 4 pipes 8 bpe @ SW_64K_S_T @ Navi1x
      74, // 4 pipes 16 bpe @ SW_64K_S_T @ Navi1x
      75, // 8 pipes 1 bpe @ SW_64K_S_T @ Navi1x
      76, // 8 pipes 2 bpe @ SW_64K_S_T @ Navi1x
      77, // 8 pipes 4 bpe @ SW_64K_S_T @ Navi1x
      78, // 8 pipes 8 bpe @ SW_64K_S_T @ Navi1x
      79, // 8 pipes 16 bpe @ SW_64K_S_T @ Navi1x
      80, // 16 pipes 1 bpe @ SW_64K_S_T @ Navi1x
      81, // 16 pipes 2 bpe @ SW_64K_S_T @ Navi1x
      82, // 16 pipes 4 bpe @ SW_64K_S_T @ Navi1x
      83, // 16 pipes 8 bpe @ SW_64K_S_T @ Navi1x
      84, // 16 pipes 16 bpe @ SW_64K_S_T @ Navi1x
      85, // 32 pipes 1 bpe @ SW_64K_S_T @ Navi1x
      86, // 32 pipes 2 bpe @ SW_64K_S_T @ Navi1x
      87, // 32 pipes 4 bpe @ SW_64K_S_T @ Navi1x
      88, // 32 pipes 8 bpe @ SW_64K_S_T @ Navi1x
      89, // 32 pipes 16 bpe @ SW_64K_S_T @ Navi1x
      90, // 64 pipes 1 bpe @ SW_64K_S_T @ Navi1x
      91, // 64 pipes 2 bpe @ SW_64K_S_T @ Navi1x
      92, // 64 pipes 4 bpe @ SW_64K_S_T @ Navi1x
      93, // 64 pipes 8 bpe @ SW_64K_S_T @ Navi1x
      94, // 64 pipes 16 bpe @ SW_64K_S_T @ Navi1x
};

const UINT_16 SW_64K_D_T_PATIDX[] =
{
     113, // 1 pipes 1 bpe @ SW_64K_D_T @ Navi1x
      31, // 1 pipes 2 bpe @ SW_64K_D_T @ Navi1x
      32, // 1 pipes 4 bpe @ SW_64K_D_T @ Navi1x
     114, // 1 pipes 8 bpe @ SW_64K_D_T @ Navi1x
     115, // 1 pipes 16 bpe @ SW_64K_D_T @ Navi1x
     134, // 2 pipes 1 bpe @ SW_64K_D_T @ Navi1x
      66, // 2 pipes 2 bpe @ SW_64K_D_T @ Navi1x
      67, // 2 pipes 4 bpe @ SW_64K_D_T @ Navi1x
     135, // 2 pipes 8 bpe @ SW_64K_D_T @ Navi1x
     136, // 2 pipes 16 bpe @ SW_64K_D_T @ Navi1x
     137, // 4 pipes 1 bpe @ SW_64K_D_T @ Navi1x
      71, // 4 pipes 2 bpe @ SW_64K_D_T @ Navi1x
      72, // 4 pipes 4 bpe @ SW_64K_D_T @ Navi1x
     138, // 4 pipes 8 bpe @ SW_64K_D_T @ Navi1x
     139, // 4 pipes 16 bpe @ SW_64K_D_T @ Navi1x
     140, // 8 pipes 1 bpe @ SW_64K_D_T @ Navi1x
      76, // 8 pipes 2 bpe @ SW_64K_D_T @ Navi1x
      77, // 8 pipes 4 bpe @ SW_64K_D_T @ Navi1x
     141, // 8 pipes 8 bpe @ SW_64K_D_T @ Navi1x
     142, // 8 pipes 16 bpe @ SW_64K_D_T @ Navi1x
     143, // 16 pipes 1 bpe @ SW_64K_D_T @ Navi1x
      81, // 16 pipes 2 bpe @ SW_64K_D_T @ Navi1x
      82, // 16 pipes 4 bpe @ SW_64K_D_T @ Navi1x
     144, // 16 pipes 8 bpe @ SW_64K_D_T @ Navi1x
     145, // 16 pipes 16 bpe @ SW_64K_D_T @ Navi1x
     146, // 32 pipes 1 bpe @ SW_64K_D_T @ Navi1x
      86, // 32 pipes 2 bpe @ SW_64K_D_T @ Navi1x
      87, // 32 pipes 4 bpe @ SW_64K_D_T @ Navi1x
     147, // 32 pipes 8 bpe @ SW_64K_D_T @ Navi1x
     148, // 32 pipes 16 bpe @ SW_64K_D_T @ Navi1x
     149, // 64 pipes 1 bpe @ SW_64K_D_T @ Navi1x
      91, // 64 pipes 2 bpe @ SW_64K_D_T @ Navi1x
      92, // 64 pipes 4 bpe @ SW_64K_D_T @ Navi1x
     150, // 64 pipes 8 bpe @ SW_64K_D_T @ Navi1x
     151, // 64 pipes 16 bpe @ SW_64K_D_T @ Navi1x
};

const UINT_16 SW_64K_S_X_PATIDX[] =
{
      30, // 1 pipes 1 bpe @ SW_64K_S_X @ Navi1x
      31, // 1 pipes 2 bpe @ SW_64K_S_X @ Navi1x
      32, // 1 pipes 4 bpe @ SW_64K_S_X @ Navi1x
      33, // 1 pipes 8 bpe @ SW_64K_S_X @ Navi1x
      34, // 1 pipes 16 bpe @ SW_64K_S_X @ Navi1x
      35, // 2 pipes 1 bpe @ SW_64K_S_X @ Navi1x
      36, // 2 pipes 2 bpe @ SW_64K_S_X @ Navi1x
      37, // 2 pipes 4 bpe @ SW_64K_S_X @ Navi1x
      38, // 2 pipes 8 bpe @ SW_64K_S_X @ Navi1x
      39, // 2 pipes 16 bpe @ SW_64K_S_X @ Navi1x
      40, // 4 pipes 1 bpe @ SW_64K_S_X @ Navi1x
      41, // 4 pipes 2 bpe @ SW_64K_S_X @ Navi1x
      42, // 4 pipes 4 bpe @ SW_64K_S_X @ Navi1x
      43, // 4 pipes 8 bpe @ SW_64K_S_X @ Navi1x
      44, // 4 pipes 16 bpe @ SW_64K_S_X @ Navi1x
      45, // 8 pipes 1 bpe @ SW_64K_S_X @ Navi1x
      46, // 8 pipes 2 bpe @ SW_64K_S_X @ Navi1x
      47, // 8 pipes 4 bpe @ SW_64K_S_X @ Navi1x
      48, // 8 pipes 8 bpe @ SW_64K_S_X @ Navi1x
      49, // 8 pipes 16 bpe @ SW_64K_S_X @ Navi1x
      50, // 16 pipes 1 bpe @ SW_64K_S_X @ Navi1x
      51, // 16 pipes 2 bpe @ SW_64K_S_X @ Navi1x
      52, // 16 pipes 4 bpe @ SW_64K_S_X @ Navi1x
      53, // 16 pipes 8 bpe @ SW_64K_S_X @ Navi1x
      54, // 16 pipes 16 bpe @ SW_64K_S_X @ Navi1x
      55, // 32 pipes 1 bpe @ SW_64K_S_X @ Navi1x
      56, // 32 pipes 2 bpe @ SW_64K_S_X @ Navi1x
      57, // 32 pipes 4 bpe @ SW_64K_S_X @ Navi1x
      58, // 32 pipes 8 bpe @ SW_64K_S_X @ Navi1x
      59, // 32 pipes 16 bpe @ SW_64K_S_X @ Navi1x
      60, // 64 pipes 1 bpe @ SW_64K_S_X @ Navi1x
      61, // 64 pipes 2 bpe @ SW_64K_S_X @ Navi1x
      62, // 64 pipes 4 bpe @ SW_64K_S_X @ Navi1x
      63, // 64 pipes 8 bpe @ SW_64K_S_X @ Navi1x
      64, // 64 pipes 16 bpe @ SW_64K_S_X @ Navi1x
};

const UINT_16 SW_64K_D_X_PATIDX[] =
{
     113, // 1 pipes 1 bpe @ SW_64K_D_X @ Navi1x
      31, // 1 pipes 2 bpe @ SW_64K_D_X @ Navi1x
      32, // 1 pipes 4 bpe @ SW_64K_D_X @ Navi1x
     114, // 1 pipes 8 bpe @ SW_64K_D_X @ Navi1x
     115, // 1 pipes 16 bpe @ SW_64K_D_X @ Navi1x
     116, // 2 pipes 1 bpe @ SW_64K_D_X @ Navi1x
      36, // 2 pipes 2 bpe @ SW_64K_D_X @ Navi1x
      37, // 2 pipes 4 bpe @ SW_64K_D_X @ Navi1x
     117, // 2 pipes 8 bpe @ SW_64K_D_X @ Navi1x
     118, // 2 pipes 16 bpe @ SW_64K_D_X @ Navi1x
     119, // 4 pipes 1 bpe @ SW_64K_D_X @ Navi1x
      41, // 4 pipes 2 bpe @ SW_64K_D_X @ Navi1x
      42, // 4 pipes 4 bpe @ SW_64K_D_X @ Navi1x
     120, // 4 pipes 8 bpe @ SW_64K_D_X @ Navi1x
     121, // 4 pipes 16 bpe @ SW_64K_D_X @ Navi1x
     122, // 8 pipes 1 bpe @ SW_64K_D_X @ Navi1x
      46, // 8 pipes 2 bpe @ SW_64K_D_X @ Navi1x
      47, // 8 pipes 4 bpe @ SW_64K_D_X @ Navi1x
     123, // 8 pipes 8 bpe @ SW_64K_D_X @ Navi1x
     124, // 8 pipes 16 bpe @ SW_64K_D_X @ Navi1x
     125, // 16 pipes 1 bpe @ SW_64K_D_X @ Navi1x
      51, // 16 pipes 2 bpe @ SW_64K_D_X @ Navi1x
      52, // 16 pipes 4 bpe @ SW_64K_D_X @ Navi1x
     126, // 16 pipes 8 bpe @ SW_64K_D_X @ Navi1x
     127, // 16 pipes 16 bpe @ SW_64K_D_X @ Navi1x
     128, // 32 pipes 1 bpe @ SW_64K_D_X @ Navi1x
      56, // 32 pipes 2 bpe @ SW_64K_D_X @ Navi1x
      57, // 32 pipes 4 bpe @ SW_64K_D_X @ Navi1x
     129, // 32 pipes 8 bpe @ SW_64K_D_X @ Navi1x
     130, // 32 pipes 16 bpe @ SW_64K_D_X @ Navi1x
     131, // 64 pipes 1 bpe @ SW_64K_D_X @ Navi1x
      61, // 64 pipes 2 bpe @ SW_64K_D_X @ Navi1x
      62, // 64 pipes 4 bpe @ SW_64K_D_X @ Navi1x
     132, // 64 pipes 8 bpe @ SW_64K_D_X @ Navi1x
     133, // 64 pipes 16 bpe @ SW_64K_D_X @ Navi1x
};

const UINT_16 SW_64K_R_X_1xaa_PATIDX[] =
{
     113, // 1 pipes 1 bpe @ SW_64K_R_X 1xaa @ Navi1x
      31, // 1 pipes 2 bpe @ SW_64K_R_X 1xaa @ Navi1x
      32, // 1 pipes 4 bpe @ SW_64K_R_X 1xaa @ Navi1x
     114, // 1 pipes 8 bpe @ SW_64K_R_X 1xaa @ Navi1x
     115, // 1 pipes 16 bpe @ SW_64K_R_X 1xaa @ Navi1x
     291, // 2 pipes 1 bpe @ SW_64K_R_X 1xaa @ Navi1x
     292, // 2 pipes 2 bpe @ SW_64K_R_X 1xaa @ Navi1x
      37, // 2 pipes 4 bpe @ SW_64K_R_X 1xaa @ Navi1x
     293, // 2 pipes 8 bpe @ SW_64K_R_X 1xaa @ Navi1x
     160, // 2 pipes 16 bpe @ SW_64K_R_X 1xaa @ Navi1x
     294, // 4 pipes 1 bpe @ SW_64K_R_X 1xaa @ Navi1x
     295, // 4 pipes 2 bpe @ SW_64K_R_X 1xaa @ Navi1x
     296, // 4 pipes 4 bpe @ SW_64K_R_X 1xaa @ Navi1x
     297, // 4 pipes 8 bpe @ SW_64K_R_X 1xaa @ Navi1x
     165, // 4 pipes 16 bpe @ SW_64K_R_X 1xaa @ Navi1x
     298, // 8 pipes 1 bpe @ SW_64K_R_X 1xaa @ Navi1x
     299, // 8 pipes 2 bpe @ SW_64K_R_X 1xaa @ Navi1x
     300, // 8 pipes 4 bpe @ SW_64K_R_X 1xaa @ Navi1x
     301, // 8 pipes 8 bpe @ SW_64K_R_X 1xaa @ Navi1x
     170, // 8 pipes 16 bpe @ SW_64K_R_X 1xaa @ Navi1x
     302, // 16 pipes 1 bpe @ SW_64K_R_X 1xaa @ Navi1x
     303, // 16 pipes 2 bpe @ SW_64K_R_X 1xaa @ Navi1x
     304, // 16 pipes 4 bpe @ SW_64K_R_X 1xaa @ Navi1x
     305, // 16 pipes 8 bpe @ SW_64K_R_X 1xaa @ Navi1x
     175, // 16 pipes 16 bpe @ SW_64K_R_X 1xaa @ Navi1x
     306, // 32 pipes 1 bpe @ SW_64K_R_X 1xaa @ Navi1x
     307, // 32 pipes 2 bpe @ SW_64K_R_X 1xaa @ Navi1x
     308, // 32 pipes 4 bpe @ SW_64K_R_X 1xaa @ Navi1x
     309, // 32 pipes 8 bpe @ SW_64K_R_X 1xaa @ Navi1x
     180, // 32 pipes 16 bpe @ SW_64K_R_X 1xaa @ Navi1x
     310, // 64 pipes 1 bpe @ SW_64K_R_X 1xaa @ Navi1x
     311, // 64 pipes 2 bpe @ SW_64K_R_X 1xaa @ Navi1x
     312, // 64 pipes 4 bpe @ SW_64K_R_X 1xaa @ Navi1x
     313, // 64 pipes 8 bpe @ SW_64K_R_X 1xaa @ Navi1x
     185, // 64 pipes 16 bpe @ SW_64K_R_X 1xaa @ Navi1x
};

const UINT_16 SW_64K_R_X_2xaa_PATIDX[] =
{
     314, // 1 pipes 1 bpe @ SW_64K_R_X 2xaa @ Navi1x
     315, // 1 pipes 2 bpe @ SW_64K_R_X 2xaa @ Navi1x
     316, // 1 pipes 4 bpe @ SW_64K_R_X 2xaa @ Navi1x
     317, // 1 pipes 8 bpe @ SW_64K_R_X 2xaa @ Navi1x
     318, // 1 pipes 16 bpe @ SW_64K_R_X 2xaa @ Navi1x
     319, // 2 pipes 1 bpe @ SW_64K_R_X 2xaa @ Navi1x
     320, // 2 pipes 2 bpe @ SW_64K_R_X 2xaa @ Navi1x
     321, // 2 pipes 4 bpe @ SW_64K_R_X 2xaa @ Navi1x
     322, // 2 pipes 8 bpe @ SW_64K_R_X 2xaa @ Navi1x
     323, // 2 pipes 16 bpe @ SW_64K_R_X 2xaa @ Navi1x
     324, // 4 pipes 1 bpe @ SW_64K_R_X 2xaa @ Navi1x
     325, // 4 pipes 2 bpe @ SW_64K_R_X 2xaa @ Navi1x
     326, // 4 pipes 4 bpe @ SW_64K_R_X 2xaa @ Navi1x
     327, // 4 pipes 8 bpe @ SW_64K_R_X 2xaa @ Navi1x
     328, // 4 pipes 16 bpe @ SW_64K_R_X 2xaa @ Navi1x
     329, // 8 pipes 1 bpe @ SW_64K_R_X 2xaa @ Navi1x
     330, // 8 pipes 2 bpe @ SW_64K_R_X 2xaa @ Navi1x
     331, // 8 pipes 4 bpe @ SW_64K_R_X 2xaa @ Navi1x
     332, // 8 pipes 8 bpe @ SW_64K_R_X 2xaa @ Navi1x
     333, // 8 pipes 16 bpe @ SW_64K_R_X 2xaa @ Navi1x
     334, // 16 pipes 1 bpe @ SW_64K_R_X 2xaa @ Navi1x
     335, // 16 pipes 2 bpe @ SW_64K_R_X 2xaa @ Navi1x
     336, // 16 pipes 4 bpe @ SW_64K_R_X 2xaa @ Navi1x
     337, // 16 pipes 8 bpe @ SW_64K_R_X 2xaa @ Navi1x
     338, // 16 pipes 16 bpe @ SW_64K_R_X 2xaa @ Navi1x
     339, // 32 pipes 1 bpe @ SW_64K_R_X 2xaa @ Navi1x
     340, // 32 pipes 2 bpe @ SW_64K_R_X 2xaa @ Navi1x
     341, // 32 pipes 4 bpe @ SW_64K_R_X 2xaa @ Navi1x
     342, // 32 pipes 8 bpe @ SW_64K_R_X 2xaa @ Navi1x
     343, // 32 pipes 16 bpe @ SW_64K_R_X 2xaa @ Navi1x
     344, // 64 pipes 1 bpe @ SW_64K_R_X 2xaa @ Navi1x
     345, // 64 pipes 2 bpe @ SW_64K_R_X 2xaa @ Navi1x
     346, // 64 pipes 4 bpe @ SW_64K_R_X 2xaa @ Navi1x
     347, // 64 pipes 8 bpe @ SW_64K_R_X 2xaa @ Navi1x
     348, // 64 pipes 16 bpe @ SW_64K_R_X 2xaa @ Navi1x
};

const UINT_16 SW_64K_R_X_4xaa_PATIDX[] =
{
     349, // 1 pipes 1 bpe @ SW_64K_R_X 4xaa @ Navi1x
     350, // 1 pipes 2 bpe @ SW_64K_R_X 4xaa @ Navi1x
     351, // 1 pipes 4 bpe @ SW_64K_R_X 4xaa @ Navi1x
     352, // 1 pipes 8 bpe @ SW_64K_R_X 4xaa @ Navi1x
     353, // 1 pipes 16 bpe @ SW_64K_R_X 4xaa @ Navi1x
     354, // 2 pipes 1 bpe @ SW_64K_R_X 4xaa @ Navi1x
     355, // 2 pipes 2 bpe @ SW_64K_R_X 4xaa @ Navi1x
     356, // 2 pipes 4 bpe @ SW_64K_R_X 4xaa @ Navi1x
     357, // 2 pipes 8 bpe @ SW_64K_R_X 4xaa @ Navi1x
     358, // 2 pipes 16 bpe @ SW_64K_R_X 4xaa @ Navi1x
     359, // 4 pipes 1 bpe @ SW_64K_R_X 4xaa @ Navi1x
     360, // 4 pipes 2 bpe @ SW_64K_R_X 4xaa @ Navi1x
     361, // 4 pipes 4 bpe @ SW_64K_R_X 4xaa @ Navi1x
     362, // 4 pipes 8 bpe @ SW_64K_R_X 4xaa @ Navi1x
     363, // 4 pipes 16 bpe @ SW_64K_R_X 4xaa @ Navi1x
     364, // 8 pipes 1 bpe @ SW_64K_R_X 4xaa @ Navi1x
     365, // 8 pipes 2 bpe @ SW_64K_R_X 4xaa @ Navi1x
     366, // 8 pipes 4 bpe @ SW_64K_R_X 4xaa @ Navi1x
     367, // 8 pipes 8 bpe @ SW_64K_R_X 4xaa @ Navi1x
     368, // 8 pipes 16 bpe @ SW_64K_R_X 4xaa @ Navi1x
     369, // 16 pipes 1 bpe @ SW_64K_R_X 4xaa @ Navi1x
     370, // 16 pipes 2 bpe @ SW_64K_R_X 4xaa @ Navi1x
     371, // 16 pipes 4 bpe @ SW_64K_R_X 4xaa @ Navi1x
     372, // 16 pipes 8 bpe @ SW_64K_R_X 4xaa @ Navi1x
     373, // 16 pipes 16 bpe @ SW_64K_R_X 4xaa @ Navi1x
     374, // 32 pipes 1 bpe @ SW_64K_R_X 4xaa @ Navi1x
     375, // 32 pipes 2 bpe @ SW_64K_R_X 4xaa @ Navi1x
     376, // 32 pipes 4 bpe @ SW_64K_R_X 4xaa @ Navi1x
     377, // 32 pipes 8 bpe @ SW_64K_R_X 4xaa @ Navi1x
     378, // 32 pipes 16 bpe @ SW_64K_R_X 4xaa @ Navi1x
     379, // 64 pipes 1 bpe @ SW_64K_R_X 4xaa @ Navi1x
     380, // 64 pipes 2 bpe @ SW_64K_R_X 4xaa @ Navi1x
     381, // 64 pipes 4 bpe @ SW_64K_R_X 4xaa @ Navi1x
     382, // 64 pipes 8 bpe @ SW_64K_R_X 4xaa @ Navi1x
     383, // 64 pipes 16 bpe @ SW_64K_R_X 4xaa @ Navi1x
};

const UINT_16 SW_64K_R_X_8xaa_PATIDX[] =
{
     384, // 1 pipes 1 bpe @ SW_64K_R_X 8xaa @ Navi1x
     385, // 1 pipes 2 bpe @ SW_64K_R_X 8xaa @ Navi1x
     386, // 1 pipes 4 bpe @ SW_64K_R_X 8xaa @ Navi1x
     387, // 1 pipes 8 bpe @ SW_64K_R_X 8xaa @ Navi1x
     388, // 1 pipes 16 bpe @ SW_64K_R_X 8xaa @ Navi1x
     389, // 2 pipes 1 bpe @ SW_64K_R_X 8xaa @ Navi1x
     390, // 2 pipes 2 bpe @ SW_64K_R_X 8xaa @ Navi1x
     391, // 2 pipes 4 bpe @ SW_64K_R_X 8xaa @ Navi1x
     392, // 2 pipes 8 bpe @ SW_64K_R_X 8xaa @ Navi1x
     393, // 2 pipes 16 bpe @ SW_64K_R_X 8xaa @ Navi1x
     394, // 4 pipes 1 bpe @ SW_64K_R_X 8xaa @ Navi1x
     395, // 4 pipes 2 bpe @ SW_64K_R_X 8xaa @ Navi1x
     396, // 4 pipes 4 bpe @ SW_64K_R_X 8xaa @ Navi1x
     397, // 4 pipes 8 bpe @ SW_64K_R_X 8xaa @ Navi1x
     398, // 4 pipes 16 bpe @ SW_64K_R_X 8xaa @ Navi1x
     399, // 8 pipes 1 bpe @ SW_64K_R_X 8xaa @ Navi1x
     400, // 8 pipes 2 bpe @ SW_64K_R_X 8xaa @ Navi1x
     401, // 8 pipes 4 bpe @ SW_64K_R_X 8xaa @ Navi1x
     402, // 8 pipes 8 bpe @ SW_64K_R_X 8xaa @ Navi1x
     403, // 8 pipes 16 bpe @ SW_64K_R_X 8xaa @ Navi1x
     404, // 16 pipes 1 bpe @ SW_64K_R_X 8xaa @ Navi1x
     405, // 16 pipes 2 bpe @ SW_64K_R_X 8xaa @ Navi1x
     406, // 16 pipes 4 bpe @ SW_64K_R_X 8xaa @ Navi1x
     407, // 16 pipes 8 bpe @ SW_64K_R_X 8xaa @ Navi1x
     408, // 16 pipes 16 bpe @ SW_64K_R_X 8xaa @ Navi1x
     409, // 32 pipes 1 bpe @ SW_64K_R_X 8xaa @ Navi1x
     410, // 32 pipes 2 bpe @ SW_64K_R_X 8xaa @ Navi1x
     411, // 32 pipes 4 bpe @ SW_64K_R_X 8xaa @ Navi1x
     412, // 32 pipes 8 bpe @ SW_64K_R_X 8xaa @ Navi1x
     413, // 32 pipes 16 bpe @ SW_64K_R_X 8xaa @ Navi1x
     414, // 64 pipes 1 bpe @ SW_64K_R_X 8xaa @ Navi1x
     415, // 64 pipes 2 bpe @ SW_64K_R_X 8xaa @ Navi1x
     416, // 64 pipes 4 bpe @ SW_64K_R_X 8xaa @ Navi1x
     417, // 64 pipes 8 bpe @ SW_64K_R_X 8xaa @ Navi1x
     418, // 64 pipes 16 bpe @ SW_64K_R_X 8xaa @ Navi1x
};

const UINT_16 SW_64K_Z_X_1xaa_PATIDX[] =
{
     152, // 1 pipes 1 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     153, // 1 pipes 2 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     154, // 1 pipes 4 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     155, // 1 pipes 8 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     115, // 1 pipes 16 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     156, // 2 pipes 1 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     157, // 2 pipes 2 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     158, // 2 pipes 4 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     159, // 2 pipes 8 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     160, // 2 pipes 16 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     161, // 4 pipes 1 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     162, // 4 pipes 2 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     163, // 4 pipes 4 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     164, // 4 pipes 8 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     165, // 4 pipes 16 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     166, // 8 pipes 1 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     167, // 8 pipes 2 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     168, // 8 pipes 4 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     169, // 8 pipes 8 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     170, // 8 pipes 16 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     171, // 16 pipes 1 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     172, // 16 pipes 2 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     173, // 16 pipes 4 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     174, // 16 pipes 8 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     175, // 16 pipes 16 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     176, // 32 pipes 1 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     177, // 32 pipes 2 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     178, // 32 pipes 4 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     179, // 32 pipes 8 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     180, // 32 pipes 16 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     181, // 64 pipes 1 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     182, // 64 pipes 2 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     183, // 64 pipes 4 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     184, // 64 pipes 8 bpe @ SW_64K_Z_X 1xaa @ Navi1x
     185, // 64 pipes 16 bpe @ SW_64K_Z_X 1xaa @ Navi1x
};

const UINT_16 SW_64K_Z_X_2xaa_PATIDX[] =
{
     186, // 1 pipes 1 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     187, // 1 pipes 2 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     188, // 1 pipes 4 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     189, // 1 pipes 8 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     190, // 1 pipes 16 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     191, // 2 pipes 1 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     192, // 2 pipes 2 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     193, // 2 pipes 4 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     194, // 2 pipes 8 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     195, // 2 pipes 16 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     196, // 4 pipes 1 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     197, // 4 pipes 2 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     198, // 4 pipes 4 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     199, // 4 pipes 8 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     200, // 4 pipes 16 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     201, // 8 pipes 1 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     202, // 8 pipes 2 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     203, // 8 pipes 4 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     204, // 8 pipes 8 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     205, // 8 pipes 16 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     206, // 16 pipes 1 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     207, // 16 pipes 2 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     208, // 16 pipes 4 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     209, // 16 pipes 8 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     210, // 16 pipes 16 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     211, // 32 pipes 1 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     212, // 32 pipes 2 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     213, // 32 pipes 4 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     214, // 32 pipes 8 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     215, // 32 pipes 16 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     216, // 64 pipes 1 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     217, // 64 pipes 2 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     218, // 64 pipes 4 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     219, // 64 pipes 8 bpe @ SW_64K_Z_X 2xaa @ Navi1x
     220, // 64 pipes 16 bpe @ SW_64K_Z_X 2xaa @ Navi1x
};

const UINT_16 SW_64K_Z_X_4xaa_PATIDX[] =
{
     221, // 1 pipes 1 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     222, // 1 pipes 2 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     223, // 1 pipes 4 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     224, // 1 pipes 8 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     225, // 1 pipes 16 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     226, // 2 pipes 1 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     227, // 2 pipes 2 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     228, // 2 pipes 4 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     229, // 2 pipes 8 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     230, // 2 pipes 16 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     231, // 4 pipes 1 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     232, // 4 pipes 2 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     233, // 4 pipes 4 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     234, // 4 pipes 8 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     235, // 4 pipes 16 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     236, // 8 pipes 1 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     237, // 8 pipes 2 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     238, // 8 pipes 4 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     239, // 8 pipes 8 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     240, // 8 pipes 16 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     241, // 16 pipes 1 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     242, // 16 pipes 2 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     243, // 16 pipes 4 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     244, // 16 pipes 8 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     245, // 16 pipes 16 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     246, // 32 pipes 1 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     247, // 32 pipes 2 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     248, // 32 pipes 4 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     249, // 32 pipes 8 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     250, // 32 pipes 16 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     251, // 64 pipes 1 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     252, // 64 pipes 2 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     253, // 64 pipes 4 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     254, // 64 pipes 8 bpe @ SW_64K_Z_X 4xaa @ Navi1x
     255, // 64 pipes 16 bpe @ SW_64K_Z_X 4xaa @ Navi1x
};

const UINT_16 SW_64K_Z_X_8xaa_PATIDX[] =
{
     256, // 1 pipes 1 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     257, // 1 pipes 2 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     258, // 1 pipes 4 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     259, // 1 pipes 8 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     260, // 1 pipes 16 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     261, // 2 pipes 1 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     262, // 2 pipes 2 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     263, // 2 pipes 4 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     264, // 2 pipes 8 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     265, // 2 pipes 16 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     266, // 4 pipes 1 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     267, // 4 pipes 2 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     268, // 4 pipes 4 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     269, // 4 pipes 8 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     270, // 4 pipes 16 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     271, // 8 pipes 1 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     272, // 8 pipes 2 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     273, // 8 pipes 4 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     274, // 8 pipes 8 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     275, // 8 pipes 16 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     276, // 16 pipes 1 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     277, // 16 pipes 2 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     278, // 16 pipes 4 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     279, // 16 pipes 8 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     280, // 16 pipes 16 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     281, // 32 pipes 1 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     282, // 32 pipes 2 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     283, // 32 pipes 4 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     284, // 32 pipes 8 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     285, // 32 pipes 16 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     286, // 64 pipes 1 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     287, // 64 pipes 2 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     288, // 64 pipes 4 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     289, // 64 pipes 8 bpe @ SW_64K_Z_X 8xaa @ Navi1x
     290, // 64 pipes 16 bpe @ SW_64K_Z_X 8xaa @ Navi1x
};

const UINT_16 SW_64K_S3_PATIDX[] =
{
     444, // 1 pipes 1 bpe @ SW_64K_S3 @ Navi1x
     445, // 1 pipes 2 bpe @ SW_64K_S3 @ Navi1x
     446, // 1 pipes 4 bpe @ SW_64K_S3 @ Navi1x
     447, // 1 pipes 8 bpe @ SW_64K_S3 @ Navi1x
     448, // 1 pipes 16 bpe @ SW_64K_S3 @ Navi1x
     444, // 2 pipes 1 bpe @ SW_64K_S3 @ Navi1x
     445, // 2 pipes 2 bpe @ SW_64K_S3 @ Navi1x
     446, // 2 pipes 4 bpe @ SW_64K_S3 @ Navi1x
     447, // 2 pipes 8 bpe @ SW_64K_S3 @ Navi1x
     448, // 2 pipes 16 bpe @ SW_64K_S3 @ Navi1x
     444, // 4 pipes 1 bpe @ SW_64K_S3 @ Navi1x
     445, // 4 pipes 2 bpe @ SW_64K_S3 @ Navi1x
     446, // 4 pipes 4 bpe @ SW_64K_S3 @ Navi1x
     447, // 4 pipes 8 bpe @ SW_64K_S3 @ Navi1x
     448, // 4 pipes 16 bpe @ SW_64K_S3 @ Navi1x
     444, // 8 pipes 1 bpe @ SW_64K_S3 @ Navi1x
     445, // 8 pipes 2 bpe @ SW_64K_S3 @ Navi1x
     446, // 8 pipes 4 bpe @ SW_64K_S3 @ Navi1x
     447, // 8 pipes 8 bpe @ SW_64K_S3 @ Navi1x
     448, // 8 pipes 16 bpe @ SW_64K_S3 @ Navi1x
     444, // 16 pipes 1 bpe @ SW_64K_S3 @ Navi1x
     445, // 16 pipes 2 bpe @ SW_64K_S3 @ Navi1x
     446, // 16 pipes 4 bpe @ SW_64K_S3 @ Navi1x
     447, // 16 pipes 8 bpe @ SW_64K_S3 @ Navi1x
     448, // 16 pipes 16 bpe @ SW_64K_S3 @ Navi1x
     444, // 32 pipes 1 bpe @ SW_64K_S3 @ Navi1x
     445, // 32 pipes 2 bpe @ SW_64K_S3 @ Navi1x
     446, // 32 pipes 4 bpe @ SW_64K_S3 @ Navi1x
     447, // 32 pipes 8 bpe @ SW_64K_S3 @ Navi1x
     448, // 32 pipes 16 bpe @ SW_64K_S3 @ Navi1x
     444, // 64 pipes 1 bpe @ SW_64K_S3 @ Navi1x
     445, // 64 pipes 2 bpe @ SW_64K_S3 @ Navi1x
     446, // 64 pipes 4 bpe @ SW_64K_S3 @ Navi1x
     447, // 64 pipes 8 bpe @ SW_64K_S3 @ Navi1x
     448, // 64 pipes 16 bpe @ SW_64K_S3 @ Navi1x
};

const UINT_16 SW_64K_S3_X_PATIDX[] =
{
     444, // 1 pipes 1 bpe @ SW_64K_S3_X @ Navi1x
     445, // 1 pipes 2 bpe @ SW_64K_S3_X @ Navi1x
     446, // 1 pipes 4 bpe @ SW_64K_S3_X @ Navi1x
     447, // 1 pipes 8 bpe @ SW_64K_S3_X @ Navi1x
     448, // 1 pipes 16 bpe @ SW_64K_S3_X @ Navi1x
     449, // 2 pipes 1 bpe @ SW_64K_S3_X @ Navi1x
     450, // 2 pipes 2 bpe @ SW_64K_S3_X @ Navi1x
     451, // 2 pipes 4 bpe @ SW_64K_S3_X @ Navi1x
     452, // 2 pipes 8 bpe @ SW_64K_S3_X @ Navi1x
     453, // 2 pipes 16 bpe @ SW_64K_S3_X @ Navi1x
     454, // 4 pipes 1 bpe @ SW_64K_S3_X @ Navi1x
     455, // 4 pipes 2 bpe @ SW_64K_S3_X @ Navi1x
     456, // 4 pipes 4 bpe @ SW_64K_S3_X @ Navi1x
     457, // 4 pipes 8 bpe @ SW_64K_S3_X @ Navi1x
     458, // 4 pipes 16 bpe @ SW_64K_S3_X @ Navi1x
     459, // 8 pipes 1 bpe @ SW_64K_S3_X @ Navi1x
     460, // 8 pipes 2 bpe @ SW_64K_S3_X @ Navi1x
     461, // 8 pipes 4 bpe @ SW_64K_S3_X @ Navi1x
     462, // 8 pipes 8 bpe @ SW_64K_S3_X @ Navi1x
     463, // 8 pipes 16 bpe @ SW_64K_S3_X @ Navi1x
     464, // 16 pipes 1 bpe @ SW_64K_S3_X @ Navi1x
     465, // 16 pipes 2 bpe @ SW_64K_S3_X @ Navi1x
     466, // 16 pipes 4 bpe @ SW_64K_S3_X @ Navi1x
     467, // 16 pipes 8 bpe @ SW_64K_S3_X @ Navi1x
     468, // 16 pipes 16 bpe @ SW_64K_S3_X @ Navi1x
     469, // 32 pipes 1 bpe @ SW_64K_S3_X @ Navi1x
     470, // 32 pipes 2 bpe @ SW_64K_S3_X @ Navi1x
     471, // 32 pipes 4 bpe @ SW_64K_S3_X @ Navi1x
     472, // 32 pipes 8 bpe @ SW_64K_S3_X @ Navi1x
     473, // 32 pipes 16 bpe @ SW_64K_S3_X @ Navi1x
     474, // 64 pipes 1 bpe @ SW_64K_S3_X @ Navi1x
     475, // 64 pipes 2 bpe @ SW_64K_S3_X @ Navi1x
     476, // 64 pipes 4 bpe @ SW_64K_S3_X @ Navi1x
     477, // 64 pipes 8 bpe @ SW_64K_S3_X @ Navi1x
     478, // 64 pipes 16 bpe @ SW_64K_S3_X @ Navi1x
};

const UINT_16 SW_64K_S3_T_PATIDX[] =
{
     444, // 1 pipes 1 bpe @ SW_64K_S3_T @ Navi1x
     445, // 1 pipes 2 bpe @ SW_64K_S3_T @ Navi1x
     446, // 1 pipes 4 bpe @ SW_64K_S3_T @ Navi1x
     447, // 1 pipes 8 bpe @ SW_64K_S3_T @ Navi1x
     448, // 1 pipes 16 bpe @ SW_64K_S3_T @ Navi1x
     449, // 2 pipes 1 bpe @ SW_64K_S3_T @ Navi1x
     450, // 2 pipes 2 bpe @ SW_64K_S3_T @ Navi1x
     451, // 2 pipes 4 bpe @ SW_64K_S3_T @ Navi1x
     452, // 2 pipes 8 bpe @ SW_64K_S3_T @ Navi1x
     453, // 2 pipes 16 bpe @ SW_64K_S3_T @ Navi1x
     454, // 4 pipes 1 bpe @ SW_64K_S3_T @ Navi1x
     455, // 4 pipes 2 bpe @ SW_64K_S3_T @ Navi1x
     456, // 4 pipes 4 bpe @ SW_64K_S3_T @ Navi1x
     457, // 4 pipes 8 bpe @ SW_64K_S3_T @ Navi1x
     458, // 4 pipes 16 bpe @ SW_64K_S3_T @ Navi1x
     479, // 8 pipes 1 bpe @ SW_64K_S3_T @ Navi1x
     480, // 8 pipes 2 bpe @ SW_64K_S3_T @ Navi1x
     481, // 8 pipes 4 bpe @ SW_64K_S3_T @ Navi1x
     482, // 8 pipes 8 bpe @ SW_64K_S3_T @ Navi1x
     483, // 8 pipes 16 bpe @ SW_64K_S3_T @ Navi1x
     484, // 16 pipes 1 bpe @ SW_64K_S3_T @ Navi1x
     485, // 16 pipes 2 bpe @ SW_64K_S3_T @ Navi1x
     486, // 16 pipes 4 bpe @ SW_64K_S3_T @ Navi1x
     487, // 16 pipes 8 bpe @ SW_64K_S3_T @ Navi1x
     488, // 16 pipes 16 bpe @ SW_64K_S3_T @ Navi1x
     489, // 32 pipes 1 bpe @ SW_64K_S3_T @ Navi1x
     490, // 32 pipes 2 bpe @ SW_64K_S3_T @ Navi1x
     491, // 32 pipes 4 bpe @ SW_64K_S3_T @ Navi1x
     492, // 32 pipes 8 bpe @ SW_64K_S3_T @ Navi1x
     493, // 32 pipes 16 bpe @ SW_64K_S3_T @ Navi1x
     494, // 64 pipes 1 bpe @ SW_64K_S3_T @ Navi1x
     495, // 64 pipes 2 bpe @ SW_64K_S3_T @ Navi1x
     496, // 64 pipes 4 bpe @ SW_64K_S3_T @ Navi1x
     497, // 64 pipes 8 bpe @ SW_64K_S3_T @ Navi1x
     498, // 64 pipes 16 bpe @ SW_64K_S3_T @ Navi1x
};

const UINT_16 SW_64K_D3_X_PATIDX[] =
{
     499, // 1 pipes 1 bpe @ SW_64K_D3_X @ Navi1x
     500, // 1 pipes 2 bpe @ SW_64K_D3_X @ Navi1x
     501, // 1 pipes 4 bpe @ SW_64K_D3_X @ Navi1x
     502, // 1 pipes 8 bpe @ SW_64K_D3_X @ Navi1x
     503, // 1 pipes 16 bpe @ SW_64K_D3_X @ Navi1x
     504, // 2 pipes 1 bpe @ SW_64K_D3_X @ Navi1x
     505, // 2 pipes 2 bpe @ SW_64K_D3_X @ Navi1x
     506, // 2 pipes 4 bpe @ SW_64K_D3_X @ Navi1x
     507, // 2 pipes 8 bpe @ SW_64K_D3_X @ Navi1x
     508, // 2 pipes 16 bpe @ SW_64K_D3_X @ Navi1x
     509, // 4 pipes 1 bpe @ SW_64K_D3_X @ Navi1x
     510, // 4 pipes 2 bpe @ SW_64K_D3_X @ Navi1x
     511, // 4 pipes 4 bpe @ SW_64K_D3_X @ Navi1x
     512, // 4 pipes 8 bpe @ SW_64K_D3_X @ Navi1x
     513, // 4 pipes 16 bpe @ SW_64K_D3_X @ Navi1x
     514, // 8 pipes 1 bpe @ SW_64K_D3_X @ Navi1x
     515, // 8 pipes 2 bpe @ SW_64K_D3_X @ Navi1x
     516, // 8 pipes 4 bpe @ SW_64K_D3_X @ Navi1x
     517, // 8 pipes 8 bpe @ SW_64K_D3_X @ Navi1x
     518, // 8 pipes 16 bpe @ SW_64K_D3_X @ Navi1x
     519, // 16 pipes 1 bpe @ SW_64K_D3_X @ Navi1x
     520, // 16 pipes 2 bpe @ SW_64K_D3_X @ Navi1x
     521, // 16 pipes 4 bpe @ SW_64K_D3_X @ Navi1x
     522, // 16 pipes 8 bpe @ SW_64K_D3_X @ Navi1x
     523, // 16 pipes 16 bpe @ SW_64K_D3_X @ Navi1x
     524, // 32 pipes 1 bpe @ SW_64K_D3_X @ Navi1x
     525, // 32 pipes 2 bpe @ SW_64K_D3_X @ Navi1x
     526, // 32 pipes 4 bpe @ SW_64K_D3_X @ Navi1x
     527, // 32 pipes 8 bpe @ SW_64K_D3_X @ Navi1x
     528, // 32 pipes 16 bpe @ SW_64K_D3_X @ Navi1x
     529, // 64 pipes 1 bpe @ SW_64K_D3_X @ Navi1x
     530, // 64 pipes 2 bpe @ SW_64K_D3_X @ Navi1x
     531, // 64 pipes 4 bpe @ SW_64K_D3_X @ Navi1x
     532, // 64 pipes 8 bpe @ SW_64K_D3_X @ Navi1x
     533, // 64 pipes 16 bpe @ SW_64K_D3_X @ Navi1x
};

const UINT_16 SW_256_S_RBPLUS_PATIDX[] =
{
       0, // 1 pipes (1 PKRs) 1 bpe @ SW_256_S @ Navi2x
       1, // 1 pipes (1 PKRs) 2 bpe @ SW_256_S @ Navi2x
       2, // 1 pipes (1 PKRs) 4 bpe @ SW_256_S @ Navi2x
       3, // 1 pipes (1 PKRs) 8 bpe @ SW_256_S @ Navi2x
       4, // 1 pipes (1 PKRs) 16 bpe @ SW_256_S @ Navi2x
       0, // 2 pipes (1-2 PKRs) 1 bpe @ SW_256_S @ Navi2x
       1, // 2 pipes (1-2 PKRs) 2 bpe @ SW_256_S @ Navi2x
       2, // 2 pipes (1-2 PKRs) 4 bpe @ SW_256_S @ Navi2x
       3, // 2 pipes (1-2 PKRs) 8 bpe @ SW_256_S @ Navi2x
       4, // 2 pipes (1-2 PKRs) 16 bpe @ SW_256_S @ Navi2x
       0, // 4 pipes (1-2 PKRs) 1 bpe @ SW_256_S @ Navi2x
       1, // 4 pipes (1-2 PKRs) 2 bpe @ SW_256_S @ Navi2x
       2, // 4 pipes (1-2 PKRs) 4 bpe @ SW_256_S @ Navi2x
       3, // 4 pipes (1-2 PKRs) 8 bpe @ SW_256_S @ Navi2x
       4, // 4 pipes (1-2 PKRs) 16 bpe @ SW_256_S @ Navi2x
       0, // 8 pipes (2 PKRs) 1 bpe @ SW_256_S @ Navi2x
       1, // 8 pipes (2 PKRs) 2 bpe @ SW_256_S @ Navi2x
       2, // 8 pipes (2 PKRs) 4 bpe @ SW_256_S @ Navi2x
       3, // 8 pipes (2 PKRs) 8 bpe @ SW_256_S @ Navi2x
       4, // 8 pipes (2 PKRs) 16 bpe @ SW_256_S @ Navi2x
       0, // 4 pipes (4 PKRs) 1 bpe @ SW_256_S @ Navi2x
       1, // 4 pipes (4 PKRs) 2 bpe @ SW_256_S @ Navi2x
       2, // 4 pipes (4 PKRs) 4 bpe @ SW_256_S @ Navi2x
       3, // 4 pipes (4 PKRs) 8 bpe @ SW_256_S @ Navi2x
       4, // 4 pipes (4 PKRs) 16 bpe @ SW_256_S @ Navi2x
       0, // 8 pipes (4 PKRs) 1 bpe @ SW_256_S @ Navi2x
       1, // 8 pipes (4 PKRs) 2 bpe @ SW_256_S @ Navi2x
       2, // 8 pipes (4 PKRs) 4 bpe @ SW_256_S @ Navi2x
       3, // 8 pipes (4 PKRs) 8 bpe @ SW_256_S @ Navi2x
       4, // 8 pipes (4 PKRs) 16 bpe @ SW_256_S @ Navi2x
       0, // 16 pipes (4 PKRs) 1 bpe @ SW_256_S @ Navi2x
       1, // 16 pipes (4 PKRs) 2 bpe @ SW_256_S @ Navi2x
       2, // 16 pipes (4 PKRs) 4 bpe @ SW_256_S @ Navi2x
       3, // 16 pipes (4 PKRs) 8 bpe @ SW_256_S @ Navi2x
       4, // 16 pipes (4 PKRs) 16 bpe @ SW_256_S @ Navi2x
       0, // 8 pipes (8 PKRs) 1 bpe @ SW_256_S @ Navi2x
       1, // 8 pipes (8 PKRs) 2 bpe @ SW_256_S @ Navi2x
       2, // 8 pipes (8 PKRs) 4 bpe @ SW_256_S @ Navi2x
       3, // 8 pipes (8 PKRs) 8 bpe @ SW_256_S @ Navi2x
       4, // 8 pipes (8 PKRs) 16 bpe @ SW_256_S @ Navi2x
       0, // 16 pipes (8 PKRs) 1 bpe @ SW_256_S @ Navi2x
       1, // 16 pipes (8 PKRs) 2 bpe @ SW_256_S @ Navi2x
       2, // 16 pipes (8 PKRs) 4 bpe @ SW_256_S @ Navi2x
       3, // 16 pipes (8 PKRs) 8 bpe @ SW_256_S @ Navi2x
       4, // 16 pipes (8 PKRs) 16 bpe @ SW_256_S @ Navi2x
       0, // 32 pipes (8 PKRs) 1 bpe @ SW_256_S @ Navi2x
       1, // 32 pipes (8 PKRs) 2 bpe @ SW_256_S @ Navi2x
       2, // 32 pipes (8 PKRs) 4 bpe @ SW_256_S @ Navi2x
       3, // 32 pipes (8 PKRs) 8 bpe @ SW_256_S @ Navi2x
       4, // 32 pipes (8 PKRs) 16 bpe @ SW_256_S @ Navi2x
       0, // 16 pipes (16 PKRs) 1 bpe @ SW_256_S @ Navi2x
       1, // 16 pipes (16 PKRs) 2 bpe @ SW_256_S @ Navi2x
       2, // 16 pipes (16 PKRs) 4 bpe @ SW_256_S @ Navi2x
       3, // 16 pipes (16 PKRs) 8 bpe @ SW_256_S @ Navi2x
       4, // 16 pipes (16 PKRs) 16 bpe @ SW_256_S @ Navi2x
       0, // 32 pipes (16 PKRs) 1 bpe @ SW_256_S @ Navi2x
       1, // 32 pipes (16 PKRs) 2 bpe @ SW_256_S @ Navi2x
       2, // 32 pipes (16 PKRs) 4 bpe @ SW_256_S @ Navi2x
       3, // 32 pipes (16 PKRs) 8 bpe @ SW_256_S @ Navi2x
       4, // 32 pipes (16 PKRs) 16 bpe @ SW_256_S @ Navi2x
       0, // 64 pipes (16 PKRs) 1 bpe @ SW_256_S @ Navi2x
       1, // 64 pipes (16 PKRs) 2 bpe @ SW_256_S @ Navi2x
       2, // 64 pipes (16 PKRs) 4 bpe @ SW_256_S @ Navi2x
       3, // 64 pipes (16 PKRs) 8 bpe @ SW_256_S @ Navi2x
       4, // 64 pipes (16 PKRs) 16 bpe @ SW_256_S @ Navi2x
       0, // 32 pipes (32 PKRs) 1 bpe @ SW_256_S @ Navi2x
       1, // 32 pipes (32 PKRs) 2 bpe @ SW_256_S @ Navi2x
       2, // 32 pipes (32 PKRs) 4 bpe @ SW_256_S @ Navi2x
       3, // 32 pipes (32 PKRs) 8 bpe @ SW_256_S @ Navi2x
       4, // 32 pipes (32 PKRs) 16 bpe @ SW_256_S @ Navi2x
       0, // 64 pipes (32 PKRs) 1 bpe @ SW_256_S @ Navi2x
       1, // 64 pipes (32 PKRs) 2 bpe @ SW_256_S @ Navi2x
       2, // 64 pipes (32 PKRs) 4 bpe @ SW_256_S @ Navi2x
       3, // 64 pipes (32 PKRs) 8 bpe @ SW_256_S @ Navi2x
       4, // 64 pipes (32 PKRs) 16 bpe @ SW_256_S @ Navi2x
};

const UINT_16 SW_256_D_RBPLUS_PATIDX[] =
{
      95, // 1 pipes (1 PKRs) 1 bpe @ SW_256_D @ Navi2x
       1, // 1 pipes (1 PKRs) 2 bpe @ SW_256_D @ Navi2x
     619, // 1 pipes (1 PKRs) 4 bpe @ SW_256_D @ Navi2x
      96, // 1 pipes (1 PKRs) 8 bpe @ SW_256_D @ Navi2x
      97, // 1 pipes (1 PKRs) 16 bpe @ SW_256_D @ Navi2x
      95, // 2 pipes (1-2 PKRs) 1 bpe @ SW_256_D @ Navi2x
       1, // 2 pipes (1-2 PKRs) 2 bpe @ SW_256_D @ Navi2x
     619, // 2 pipes (1-2 PKRs) 4 bpe @ SW_256_D @ Navi2x
      96, // 2 pipes (1-2 PKRs) 8 bpe @ SW_256_D @ Navi2x
      97, // 2 pipes (1-2 PKRs) 16 bpe @ SW_256_D @ Navi2x
      95, // 4 pipes (1-2 PKRs) 1 bpe @ SW_256_D @ Navi2x
       1, // 4 pipes (1-2 PKRs) 2 bpe @ SW_256_D @ Navi2x
     619, // 4 pipes (1-2 PKRs) 4 bpe @ SW_256_D @ Navi2x
      96, // 4 pipes (1-2 PKRs) 8 bpe @ SW_256_D @ Navi2x
      97, // 4 pipes (1-2 PKRs) 16 bpe @ SW_256_D @ Navi2x
      95, // 8 pipes (2 PKRs) 1 bpe @ SW_256_D @ Navi2x
       1, // 8 pipes (2 PKRs) 2 bpe @ SW_256_D @ Navi2x
     619, // 8 pipes (2 PKRs) 4 bpe @ SW_256_D @ Navi2x
      96, // 8 pipes (2 PKRs) 8 bpe @ SW_256_D @ Navi2x
      97, // 8 pipes (2 PKRs) 16 bpe @ SW_256_D @ Navi2x
      95, // 4 pipes (4 PKRs) 1 bpe @ SW_256_D @ Navi2x
       1, // 4 pipes (4 PKRs) 2 bpe @ SW_256_D @ Navi2x
     619, // 4 pipes (4 PKRs) 4 bpe @ SW_256_D @ Navi2x
      96, // 4 pipes (4 PKRs) 8 bpe @ SW_256_D @ Navi2x
      97, // 4 pipes (4 PKRs) 16 bpe @ SW_256_D @ Navi2x
      95, // 8 pipes (4 PKRs) 1 bpe @ SW_256_D @ Navi2x
       1, // 8 pipes (4 PKRs) 2 bpe @ SW_256_D @ Navi2x
     619, // 8 pipes (4 PKRs) 4 bpe @ SW_256_D @ Navi2x
      96, // 8 pipes (4 PKRs) 8 bpe @ SW_256_D @ Navi2x
      97, // 8 pipes (4 PKRs) 16 bpe @ SW_256_D @ Navi2x
      95, // 16 pipes (4 PKRs) 1 bpe @ SW_256_D @ Navi2x
       1, // 16 pipes (4 PKRs) 2 bpe @ SW_256_D @ Navi2x
     619, // 16 pipes (4 PKRs) 4 bpe @ SW_256_D @ Navi2x
      96, // 16 pipes (4 PKRs) 8 bpe @ SW_256_D @ Navi2x
      97, // 16 pipes (4 PKRs) 16 bpe @ SW_256_D @ Navi2x
      95, // 8 pipes (8 PKRs) 1 bpe @ SW_256_D @ Navi2x
       1, // 8 pipes (8 PKRs) 2 bpe @ SW_256_D @ Navi2x
     619, // 8 pipes (8 PKRs) 4 bpe @ SW_256_D @ Navi2x
      96, // 8 pipes (8 PKRs) 8 bpe @ SW_256_D @ Navi2x
      97, // 8 pipes (8 PKRs) 16 bpe @ SW_256_D @ Navi2x
      95, // 16 pipes (8 PKRs) 1 bpe @ SW_256_D @ Navi2x
       1, // 16 pipes (8 PKRs) 2 bpe @ SW_256_D @ Navi2x
     619, // 16 pipes (8 PKRs) 4 bpe @ SW_256_D @ Navi2x
      96, // 16 pipes (8 PKRs) 8 bpe @ SW_256_D @ Navi2x
      97, // 16 pipes (8 PKRs) 16 bpe @ SW_256_D @ Navi2x
      95, // 32 pipes (8 PKRs) 1 bpe @ SW_256_D @ Navi2x
       1, // 32 pipes (8 PKRs) 2 bpe @ SW_256_D @ Navi2x
     619, // 32 pipes (8 PKRs) 4 bpe @ SW_256_D @ Navi2x
      96, // 32 pipes (8 PKRs) 8 bpe @ SW_256_D @ Navi2x
      97, // 32 pipes (8 PKRs) 16 bpe @ SW_256_D @ Navi2x
      95, // 16 pipes (16 PKRs) 1 bpe @ SW_256_D @ Navi2x
       1, // 16 pipes (16 PKRs) 2 bpe @ SW_256_D @ Navi2x
     619, // 16 pipes (16 PKRs) 4 bpe @ SW_256_D @ Navi2x
      96, // 16 pipes (16 PKRs) 8 bpe @ SW_256_D @ Navi2x
      97, // 16 pipes (16 PKRs) 16 bpe @ SW_256_D @ Navi2x
      95, // 32 pipes (16 PKRs) 1 bpe @ SW_256_D @ Navi2x
       1, // 32 pipes (16 PKRs) 2 bpe @ SW_256_D @ Navi2x
     619, // 32 pipes (16 PKRs) 4 bpe @ SW_256_D @ Navi2x
      96, // 32 pipes (16 PKRs) 8 bpe @ SW_256_D @ Navi2x
      97, // 32 pipes (16 PKRs) 16 bpe @ SW_256_D @ Navi2x
      95, // 64 pipes (16 PKRs) 1 bpe @ SW_256_D @ Navi2x
       1, // 64 pipes (16 PKRs) 2 bpe @ SW_256_D @ Navi2x
     619, // 64 pipes (16 PKRs) 4 bpe @ SW_256_D @ Navi2x
      96, // 64 pipes (16 PKRs) 8 bpe @ SW_256_D @ Navi2x
      97, // 64 pipes (16 PKRs) 16 bpe @ SW_256_D @ Navi2x
      95, // 32 pipes (32 PKRs) 1 bpe @ SW_256_D @ Navi2x
       1, // 32 pipes (32 PKRs) 2 bpe @ SW_256_D @ Navi2x
     619, // 32 pipes (32 PKRs) 4 bpe @ SW_256_D @ Navi2x
      96, // 32 pipes (32 PKRs) 8 bpe @ SW_256_D @ Navi2x
      97, // 32 pipes (32 PKRs) 16 bpe @ SW_256_D @ Navi2x
      95, // 64 pipes (32 PKRs) 1 bpe @ SW_256_D @ Navi2x
       1, // 64 pipes (32 PKRs) 2 bpe @ SW_256_D @ Navi2x
     619, // 64 pipes (32 PKRs) 4 bpe @ SW_256_D @ Navi2x
      96, // 64 pipes (32 PKRs) 8 bpe @ SW_256_D @ Navi2x
      97, // 64 pipes (32 PKRs) 16 bpe @ SW_256_D @ Navi2x
};

const UINT_16 SW_4K_S_RBPLUS_PATIDX[] =
{
       5, // 1 pipes (1 PKRs) 1 bpe @ SW_4K_S @ Navi2x
       6, // 1 pipes (1 PKRs) 2 bpe @ SW_4K_S @ Navi2x
       7, // 1 pipes (1 PKRs) 4 bpe @ SW_4K_S @ Navi2x
       8, // 1 pipes (1 PKRs) 8 bpe @ SW_4K_S @ Navi2x
       9, // 1 pipes (1 PKRs) 16 bpe @ SW_4K_S @ Navi2x
       5, // 2 pipes (1-2 PKRs) 1 bpe @ SW_4K_S @ Navi2x
       6, // 2 pipes (1-2 PKRs) 2 bpe @ SW_4K_S @ Navi2x
       7, // 2 pipes (1-2 PKRs) 4 bpe @ SW_4K_S @ Navi2x
       8, // 2 pipes (1-2 PKRs) 8 bpe @ SW_4K_S @ Navi2x
       9, // 2 pipes (1-2 PKRs) 16 bpe @ SW_4K_S @ Navi2x
       5, // 4 pipes (1-2 PKRs) 1 bpe @ SW_4K_S @ Navi2x
       6, // 4 pipes (1-2 PKRs) 2 bpe @ SW_4K_S @ Navi2x
       7, // 4 pipes (1-2 PKRs) 4 bpe @ SW_4K_S @ Navi2x
       8, // 4 pipes (1-2 PKRs) 8 bpe @ SW_4K_S @ Navi2x
       9, // 4 pipes (1-2 PKRs) 16 bpe @ SW_4K_S @ Navi2x
       5, // 8 pipes (2 PKRs) 1 bpe @ SW_4K_S @ Navi2x
       6, // 8 pipes (2 PKRs) 2 bpe @ SW_4K_S @ Navi2x
       7, // 8 pipes (2 PKRs) 4 bpe @ SW_4K_S @ Navi2x
       8, // 8 pipes (2 PKRs) 8 bpe @ SW_4K_S @ Navi2x
       9, // 8 pipes (2 PKRs) 16 bpe @ SW_4K_S @ Navi2x
       5, // 4 pipes (4 PKRs) 1 bpe @ SW_4K_S @ Navi2x
       6, // 4 pipes (4 PKRs) 2 bpe @ SW_4K_S @ Navi2x
       7, // 4 pipes (4 PKRs) 4 bpe @ SW_4K_S @ Navi2x
       8, // 4 pipes (4 PKRs) 8 bpe @ SW_4K_S @ Navi2x
       9, // 4 pipes (4 PKRs) 16 bpe @ SW_4K_S @ Navi2x
       5, // 8 pipes (4 PKRs) 1 bpe @ SW_4K_S @ Navi2x
       6, // 8 pipes (4 PKRs) 2 bpe @ SW_4K_S @ Navi2x
       7, // 8 pipes (4 PKRs) 4 bpe @ SW_4K_S @ Navi2x
       8, // 8 pipes (4 PKRs) 8 bpe @ SW_4K_S @ Navi2x
       9, // 8 pipes (4 PKRs) 16 bpe @ SW_4K_S @ Navi2x
       5, // 16 pipes (4 PKRs) 1 bpe @ SW_4K_S @ Navi2x
       6, // 16 pipes (4 PKRs) 2 bpe @ SW_4K_S @ Navi2x
       7, // 16 pipes (4 PKRs) 4 bpe @ SW_4K_S @ Navi2x
       8, // 16 pipes (4 PKRs) 8 bpe @ SW_4K_S @ Navi2x
       9, // 16 pipes (4 PKRs) 16 bpe @ SW_4K_S @ Navi2x
       5, // 8 pipes (8 PKRs) 1 bpe @ SW_4K_S @ Navi2x
       6, // 8 pipes (8 PKRs) 2 bpe @ SW_4K_S @ Navi2x
       7, // 8 pipes (8 PKRs) 4 bpe @ SW_4K_S @ Navi2x
       8, // 8 pipes (8 PKRs) 8 bpe @ SW_4K_S @ Navi2x
       9, // 8 pipes (8 PKRs) 16 bpe @ SW_4K_S @ Navi2x
       5, // 16 pipes (8 PKRs) 1 bpe @ SW_4K_S @ Navi2x
       6, // 16 pipes (8 PKRs) 2 bpe @ SW_4K_S @ Navi2x
       7, // 16 pipes (8 PKRs) 4 bpe @ SW_4K_S @ Navi2x
       8, // 16 pipes (8 PKRs) 8 bpe @ SW_4K_S @ Navi2x
       9, // 16 pipes (8 PKRs) 16 bpe @ SW_4K_S @ Navi2x
       5, // 32 pipes (8 PKRs) 1 bpe @ SW_4K_S @ Navi2x
       6, // 32 pipes (8 PKRs) 2 bpe @ SW_4K_S @ Navi2x
       7, // 32 pipes (8 PKRs) 4 bpe @ SW_4K_S @ Navi2x
       8, // 32 pipes (8 PKRs) 8 bpe @ SW_4K_S @ Navi2x
       9, // 32 pipes (8 PKRs) 16 bpe @ SW_4K_S @ Navi2x
       5, // 16 pipes (16 PKRs) 1 bpe @ SW_4K_S @ Navi2x
       6, // 16 pipes (16 PKRs) 2 bpe @ SW_4K_S @ Navi2x
       7, // 16 pipes (16 PKRs) 4 bpe @ SW_4K_S @ Navi2x
       8, // 16 pipes (16 PKRs) 8 bpe @ SW_4K_S @ Navi2x
       9, // 16 pipes (16 PKRs) 16 bpe @ SW_4K_S @ Navi2x
       5, // 32 pipes (16 PKRs) 1 bpe @ SW_4K_S @ Navi2x
       6, // 32 pipes (16 PKRs) 2 bpe @ SW_4K_S @ Navi2x
       7, // 32 pipes (16 PKRs) 4 bpe @ SW_4K_S @ Navi2x
       8, // 32 pipes (16 PKRs) 8 bpe @ SW_4K_S @ Navi2x
       9, // 32 pipes (16 PKRs) 16 bpe @ SW_4K_S @ Navi2x
       5, // 64 pipes (16 PKRs) 1 bpe @ SW_4K_S @ Navi2x
       6, // 64 pipes (16 PKRs) 2 bpe @ SW_4K_S @ Navi2x
       7, // 64 pipes (16 PKRs) 4 bpe @ SW_4K_S @ Navi2x
       8, // 64 pipes (16 PKRs) 8 bpe @ SW_4K_S @ Navi2x
       9, // 64 pipes (16 PKRs) 16 bpe @ SW_4K_S @ Navi2x
       5, // 32 pipes (32 PKRs) 1 bpe @ SW_4K_S @ Navi2x
       6, // 32 pipes (32 PKRs) 2 bpe @ SW_4K_S @ Navi2x
       7, // 32 pipes (32 PKRs) 4 bpe @ SW_4K_S @ Navi2x
       8, // 32 pipes (32 PKRs) 8 bpe @ SW_4K_S @ Navi2x
       9, // 32 pipes (32 PKRs) 16 bpe @ SW_4K_S @ Navi2x
       5, // 64 pipes (32 PKRs) 1 bpe @ SW_4K_S @ Navi2x
       6, // 64 pipes (32 PKRs) 2 bpe @ SW_4K_S @ Navi2x
       7, // 64 pipes (32 PKRs) 4 bpe @ SW_4K_S @ Navi2x
       8, // 64 pipes (32 PKRs) 8 bpe @ SW_4K_S @ Navi2x
       9, // 64 pipes (32 PKRs) 16 bpe @ SW_4K_S @ Navi2x
};

const UINT_16 SW_4K_D_RBPLUS_PATIDX[] =
{
      98, // 1 pipes (1 PKRs) 1 bpe @ SW_4K_D @ Navi2x
       6, // 1 pipes (1 PKRs) 2 bpe @ SW_4K_D @ Navi2x
     620, // 1 pipes (1 PKRs) 4 bpe @ SW_4K_D @ Navi2x
      99, // 1 pipes (1 PKRs) 8 bpe @ SW_4K_D @ Navi2x
     100, // 1 pipes (1 PKRs) 16 bpe @ SW_4K_D @ Navi2x
      98, // 2 pipes (1-2 PKRs) 1 bpe @ SW_4K_D @ Navi2x
       6, // 2 pipes (1-2 PKRs) 2 bpe @ SW_4K_D @ Navi2x
     620, // 2 pipes (1-2 PKRs) 4 bpe @ SW_4K_D @ Navi2x
      99, // 2 pipes (1-2 PKRs) 8 bpe @ SW_4K_D @ Navi2x
     100, // 2 pipes (1-2 PKRs) 16 bpe @ SW_4K_D @ Navi2x
      98, // 4 pipes (1-2 PKRs) 1 bpe @ SW_4K_D @ Navi2x
       6, // 4 pipes (1-2 PKRs) 2 bpe @ SW_4K_D @ Navi2x
     620, // 4 pipes (1-2 PKRs) 4 bpe @ SW_4K_D @ Navi2x
      99, // 4 pipes (1-2 PKRs) 8 bpe @ SW_4K_D @ Navi2x
     100, // 4 pipes (1-2 PKRs) 16 bpe @ SW_4K_D @ Navi2x
      98, // 8 pipes (2 PKRs) 1 bpe @ SW_4K_D @ Navi2x
       6, // 8 pipes (2 PKRs) 2 bpe @ SW_4K_D @ Navi2x
     620, // 8 pipes (2 PKRs) 4 bpe @ SW_4K_D @ Navi2x
      99, // 8 pipes (2 PKRs) 8 bpe @ SW_4K_D @ Navi2x
     100, // 8 pipes (2 PKRs) 16 bpe @ SW_4K_D @ Navi2x
      98, // 4 pipes (4 PKRs) 1 bpe @ SW_4K_D @ Navi2x
       6, // 4 pipes (4 PKRs) 2 bpe @ SW_4K_D @ Navi2x
     620, // 4 pipes (4 PKRs) 4 bpe @ SW_4K_D @ Navi2x
      99, // 4 pipes (4 PKRs) 8 bpe @ SW_4K_D @ Navi2x
     100, // 4 pipes (4 PKRs) 16 bpe @ SW_4K_D @ Navi2x
      98, // 8 pipes (4 PKRs) 1 bpe @ SW_4K_D @ Navi2x
       6, // 8 pipes (4 PKRs) 2 bpe @ SW_4K_D @ Navi2x
     620, // 8 pipes (4 PKRs) 4 bpe @ SW_4K_D @ Navi2x
      99, // 8 pipes (4 PKRs) 8 bpe @ SW_4K_D @ Navi2x
     100, // 8 pipes (4 PKRs) 16 bpe @ SW_4K_D @ Navi2x
      98, // 16 pipes (4 PKRs) 1 bpe @ SW_4K_D @ Navi2x
       6, // 16 pipes (4 PKRs) 2 bpe @ SW_4K_D @ Navi2x
     620, // 16 pipes (4 PKRs) 4 bpe @ SW_4K_D @ Navi2x
      99, // 16 pipes (4 PKRs) 8 bpe @ SW_4K_D @ Navi2x
     100, // 16 pipes (4 PKRs) 16 bpe @ SW_4K_D @ Navi2x
      98, // 8 pipes (8 PKRs) 1 bpe @ SW_4K_D @ Navi2x
       6, // 8 pipes (8 PKRs) 2 bpe @ SW_4K_D @ Navi2x
     620, // 8 pipes (8 PKRs) 4 bpe @ SW_4K_D @ Navi2x
      99, // 8 pipes (8 PKRs) 8 bpe @ SW_4K_D @ Navi2x
     100, // 8 pipes (8 PKRs) 16 bpe @ SW_4K_D @ Navi2x
      98, // 16 pipes (8 PKRs) 1 bpe @ SW_4K_D @ Navi2x
       6, // 16 pipes (8 PKRs) 2 bpe @ SW_4K_D @ Navi2x
     620, // 16 pipes (8 PKRs) 4 bpe @ SW_4K_D @ Navi2x
      99, // 16 pipes (8 PKRs) 8 bpe @ SW_4K_D @ Navi2x
     100, // 16 pipes (8 PKRs) 16 bpe @ SW_4K_D @ Navi2x
      98, // 32 pipes (8 PKRs) 1 bpe @ SW_4K_D @ Navi2x
       6, // 32 pipes (8 PKRs) 2 bpe @ SW_4K_D @ Navi2x
     620, // 32 pipes (8 PKRs) 4 bpe @ SW_4K_D @ Navi2x
      99, // 32 pipes (8 PKRs) 8 bpe @ SW_4K_D @ Navi2x
     100, // 32 pipes (8 PKRs) 16 bpe @ SW_4K_D @ Navi2x
      98, // 16 pipes (16 PKRs) 1 bpe @ SW_4K_D @ Navi2x
       6, // 16 pipes (16 PKRs) 2 bpe @ SW_4K_D @ Navi2x
     620, // 16 pipes (16 PKRs) 4 bpe @ SW_4K_D @ Navi2x
      99, // 16 pipes (16 PKRs) 8 bpe @ SW_4K_D @ Navi2x
     100, // 16 pipes (16 PKRs) 16 bpe @ SW_4K_D @ Navi2x
      98, // 32 pipes (16 PKRs) 1 bpe @ SW_4K_D @ Navi2x
       6, // 32 pipes (16 PKRs) 2 bpe @ SW_4K_D @ Navi2x
     620, // 32 pipes (16 PKRs) 4 bpe @ SW_4K_D @ Navi2x
      99, // 32 pipes (16 PKRs) 8 bpe @ SW_4K_D @ Navi2x
     100, // 32 pipes (16 PKRs) 16 bpe @ SW_4K_D @ Navi2x
      98, // 64 pipes (16 PKRs) 1 bpe @ SW_4K_D @ Navi2x
       6, // 64 pipes (16 PKRs) 2 bpe @ SW_4K_D @ Navi2x
     620, // 64 pipes (16 PKRs) 4 bpe @ SW_4K_D @ Navi2x
      99, // 64 pipes (16 PKRs) 8 bpe @ SW_4K_D @ Navi2x
     100, // 64 pipes (16 PKRs) 16 bpe @ SW_4K_D @ Navi2x
      98, // 32 pipes (32 PKRs) 1 bpe @ SW_4K_D @ Navi2x
       6, // 32 pipes (32 PKRs) 2 bpe @ SW_4K_D @ Navi2x
     620, // 32 pipes (32 PKRs) 4 bpe @ SW_4K_D @ Navi2x
      99, // 32 pipes (32 PKRs) 8 bpe @ SW_4K_D @ Navi2x
     100, // 32 pipes (32 PKRs) 16 bpe @ SW_4K_D @ Navi2x
      98, // 64 pipes (32 PKRs) 1 bpe @ SW_4K_D @ Navi2x
       6, // 64 pipes (32 PKRs) 2 bpe @ SW_4K_D @ Navi2x
     620, // 64 pipes (32 PKRs) 4 bpe @ SW_4K_D @ Navi2x
      99, // 64 pipes (32 PKRs) 8 bpe @ SW_4K_D @ Navi2x
     100, // 64 pipes (32 PKRs) 16 bpe @ SW_4K_D @ Navi2x
};

const UINT_16 SW_4K_S_X_RBPLUS_PATIDX[] =
{
       5, // 1 pipes (1 PKRs) 1 bpe @ SW_4K_S_X @ Navi2x
       6, // 1 pipes (1 PKRs) 2 bpe @ SW_4K_S_X @ Navi2x
       7, // 1 pipes (1 PKRs) 4 bpe @ SW_4K_S_X @ Navi2x
       8, // 1 pipes (1 PKRs) 8 bpe @ SW_4K_S_X @ Navi2x
       9, // 1 pipes (1 PKRs) 16 bpe @ SW_4K_S_X @ Navi2x
      10, // 2 pipes (1-2 PKRs) 1 bpe @ SW_4K_S_X @ Navi2x
      11, // 2 pipes (1-2 PKRs) 2 bpe @ SW_4K_S_X @ Navi2x
      12, // 2 pipes (1-2 PKRs) 4 bpe @ SW_4K_S_X @ Navi2x
      13, // 2 pipes (1-2 PKRs) 8 bpe @ SW_4K_S_X @ Navi2x
      14, // 2 pipes (1-2 PKRs) 16 bpe @ SW_4K_S_X @ Navi2x
     534, // 4 pipes (1-2 PKRs) 1 bpe @ SW_4K_S_X @ Navi2x
     535, // 4 pipes (1-2 PKRs) 2 bpe @ SW_4K_S_X @ Navi2x
     536, // 4 pipes (1-2 PKRs) 4 bpe @ SW_4K_S_X @ Navi2x
     537, // 4 pipes (1-2 PKRs) 8 bpe @ SW_4K_S_X @ Navi2x
     538, // 4 pipes (1-2 PKRs) 16 bpe @ SW_4K_S_X @ Navi2x
     539, // 8 pipes (2 PKRs) 1 bpe @ SW_4K_S_X @ Navi2x
     540, // 8 pipes (2 PKRs) 2 bpe @ SW_4K_S_X @ Navi2x
     541, // 8 pipes (2 PKRs) 4 bpe @ SW_4K_S_X @ Navi2x
     542, // 8 pipes (2 PKRs) 8 bpe @ SW_4K_S_X @ Navi2x
     543, // 8 pipes (2 PKRs) 16 bpe @ SW_4K_S_X @ Navi2x
      15, // 4 pipes (4 PKRs) 1 bpe @ SW_4K_S_X @ Navi2x
      16, // 4 pipes (4 PKRs) 2 bpe @ SW_4K_S_X @ Navi2x
      17, // 4 pipes (4 PKRs) 4 bpe @ SW_4K_S_X @ Navi2x
      18, // 4 pipes (4 PKRs) 8 bpe @ SW_4K_S_X @ Navi2x
      19, // 4 pipes (4 PKRs) 16 bpe @ SW_4K_S_X @ Navi2x
     544, // 8 pipes (4 PKRs) 1 bpe @ SW_4K_S_X @ Navi2x
     545, // 8 pipes (4 PKRs) 2 bpe @ SW_4K_S_X @ Navi2x
     546, // 8 pipes (4 PKRs) 4 bpe @ SW_4K_S_X @ Navi2x
     547, // 8 pipes (4 PKRs) 8 bpe @ SW_4K_S_X @ Navi2x
     548, // 8 pipes (4 PKRs) 16 bpe @ SW_4K_S_X @ Navi2x
     549, // 16 pipes (4 PKRs) 1 bpe @ SW_4K_S_X @ Navi2x
     550, // 16 pipes (4 PKRs) 2 bpe @ SW_4K_S_X @ Navi2x
     551, // 16 pipes (4 PKRs) 4 bpe @ SW_4K_S_X @ Navi2x
     552, // 16 pipes (4 PKRs) 8 bpe @ SW_4K_S_X @ Navi2x
     553, // 16 pipes (4 PKRs) 16 bpe @ SW_4K_S_X @ Navi2x
      20, // 8 pipes (8 PKRs) 1 bpe @ SW_4K_S_X @ Navi2x
      21, // 8 pipes (8 PKRs) 2 bpe @ SW_4K_S_X @ Navi2x
      22, // 8 pipes (8 PKRs) 4 bpe @ SW_4K_S_X @ Navi2x
      23, // 8 pipes (8 PKRs) 8 bpe @ SW_4K_S_X @ Navi2x
      24, // 8 pipes (8 PKRs) 16 bpe @ SW_4K_S_X @ Navi2x
     554, // 16 pipes (8 PKRs) 1 bpe @ SW_4K_S_X @ Navi2x
     555, // 16 pipes (8 PKRs) 2 bpe @ SW_4K_S_X @ Navi2x
     556, // 16 pipes (8 PKRs) 4 bpe @ SW_4K_S_X @ Navi2x
     557, // 16 pipes (8 PKRs) 8 bpe @ SW_4K_S_X @ Navi2x
     558, // 16 pipes (8 PKRs) 16 bpe @ SW_4K_S_X @ Navi2x
     559, // 32 pipes (8 PKRs) 1 bpe @ SW_4K_S_X @ Navi2x
     560, // 32 pipes (8 PKRs) 2 bpe @ SW_4K_S_X @ Navi2x
     561, // 32 pipes (8 PKRs) 4 bpe @ SW_4K_S_X @ Navi2x
     562, // 32 pipes (8 PKRs) 8 bpe @ SW_4K_S_X @ Navi2x
     563, // 32 pipes (8 PKRs) 16 bpe @ SW_4K_S_X @ Navi2x
      25, // 16 pipes (16 PKRs) 1 bpe @ SW_4K_S_X @ Navi2x
      26, // 16 pipes (16 PKRs) 2 bpe @ SW_4K_S_X @ Navi2x
      27, // 16 pipes (16 PKRs) 4 bpe @ SW_4K_S_X @ Navi2x
      28, // 16 pipes (16 PKRs) 8 bpe @ SW_4K_S_X @ Navi2x
      29, // 16 pipes (16 PKRs) 16 bpe @ SW_4K_S_X @ Navi2x
     564, // 32 pipes (16 PKRs) 1 bpe @ SW_4K_S_X @ Navi2x
     565, // 32 pipes (16 PKRs) 2 bpe @ SW_4K_S_X @ Navi2x
     566, // 32 pipes (16 PKRs) 4 bpe @ SW_4K_S_X @ Navi2x
     567, // 32 pipes (16 PKRs) 8 bpe @ SW_4K_S_X @ Navi2x
     568, // 32 pipes (16 PKRs) 16 bpe @ SW_4K_S_X @ Navi2x
     569, // 64 pipes (16 PKRs) 1 bpe @ SW_4K_S_X @ Navi2x
     570, // 64 pipes (16 PKRs) 2 bpe @ SW_4K_S_X @ Navi2x
     571, // 64 pipes (16 PKRs) 4 bpe @ SW_4K_S_X @ Navi2x
     572, // 64 pipes (16 PKRs) 8 bpe @ SW_4K_S_X @ Navi2x
     573, // 64 pipes (16 PKRs) 16 bpe @ SW_4K_S_X @ Navi2x
      25, // 32 pipes (32 PKRs) 1 bpe @ SW_4K_S_X @ Navi2x
      26, // 32 pipes (32 PKRs) 2 bpe @ SW_4K_S_X @ Navi2x
      27, // 32 pipes (32 PKRs) 4 bpe @ SW_4K_S_X @ Navi2x
      28, // 32 pipes (32 PKRs) 8 bpe @ SW_4K_S_X @ Navi2x
      29, // 32 pipes (32 PKRs) 16 bpe @ SW_4K_S_X @ Navi2x
     564, // 64 pipes (32 PKRs) 1 bpe @ SW_4K_S_X @ Navi2x
     565, // 64 pipes (32 PKRs) 2 bpe @ SW_4K_S_X @ Navi2x
     566, // 64 pipes (32 PKRs) 4 bpe @ SW_4K_S_X @ Navi2x
     567, // 64 pipes (32 PKRs) 8 bpe @ SW_4K_S_X @ Navi2x
     568, // 64 pipes (32 PKRs) 16 bpe @ SW_4K_S_X @ Navi2x
};

const UINT_16 SW_4K_D_X_RBPLUS_PATIDX[] =
{
      98, // 1 pipes (1 PKRs) 1 bpe @ SW_4K_D_X @ Navi2x
       6, // 1 pipes (1 PKRs) 2 bpe @ SW_4K_D_X @ Navi2x
     620, // 1 pipes (1 PKRs) 4 bpe @ SW_4K_D_X @ Navi2x
      99, // 1 pipes (1 PKRs) 8 bpe @ SW_4K_D_X @ Navi2x
     100, // 1 pipes (1 PKRs) 16 bpe @ SW_4K_D_X @ Navi2x
     101, // 2 pipes (1-2 PKRs) 1 bpe @ SW_4K_D_X @ Navi2x
      11, // 2 pipes (1-2 PKRs) 2 bpe @ SW_4K_D_X @ Navi2x
     621, // 2 pipes (1-2 PKRs) 4 bpe @ SW_4K_D_X @ Navi2x
     102, // 2 pipes (1-2 PKRs) 8 bpe @ SW_4K_D_X @ Navi2x
     103, // 2 pipes (1-2 PKRs) 16 bpe @ SW_4K_D_X @ Navi2x
     622, // 4 pipes (1-2 PKRs) 1 bpe @ SW_4K_D_X @ Navi2x
     535, // 4 pipes (1-2 PKRs) 2 bpe @ SW_4K_D_X @ Navi2x
     623, // 4 pipes (1-2 PKRs) 4 bpe @ SW_4K_D_X @ Navi2x
     624, // 4 pipes (1-2 PKRs) 8 bpe @ SW_4K_D_X @ Navi2x
     625, // 4 pipes (1-2 PKRs) 16 bpe @ SW_4K_D_X @ Navi2x
     626, // 8 pipes (2 PKRs) 1 bpe @ SW_4K_D_X @ Navi2x
     540, // 8 pipes (2 PKRs) 2 bpe @ SW_4K_D_X @ Navi2x
     627, // 8 pipes (2 PKRs) 4 bpe @ SW_4K_D_X @ Navi2x
     628, // 8 pipes (2 PKRs) 8 bpe @ SW_4K_D_X @ Navi2x
     629, // 8 pipes (2 PKRs) 16 bpe @ SW_4K_D_X @ Navi2x
     104, // 4 pipes (4 PKRs) 1 bpe @ SW_4K_D_X @ Navi2x
      16, // 4 pipes (4 PKRs) 2 bpe @ SW_4K_D_X @ Navi2x
     630, // 4 pipes (4 PKRs) 4 bpe @ SW_4K_D_X @ Navi2x
     105, // 4 pipes (4 PKRs) 8 bpe @ SW_4K_D_X @ Navi2x
     106, // 4 pipes (4 PKRs) 16 bpe @ SW_4K_D_X @ Navi2x
     631, // 8 pipes (4 PKRs) 1 bpe @ SW_4K_D_X @ Navi2x
     545, // 8 pipes (4 PKRs) 2 bpe @ SW_4K_D_X @ Navi2x
     632, // 8 pipes (4 PKRs) 4 bpe @ SW_4K_D_X @ Navi2x
     633, // 8 pipes (4 PKRs) 8 bpe @ SW_4K_D_X @ Navi2x
     634, // 8 pipes (4 PKRs) 16 bpe @ SW_4K_D_X @ Navi2x
     635, // 16 pipes (4 PKRs) 1 bpe @ SW_4K_D_X @ Navi2x
     550, // 16 pipes (4 PKRs) 2 bpe @ SW_4K_D_X @ Navi2x
     636, // 16 pipes (4 PKRs) 4 bpe @ SW_4K_D_X @ Navi2x
     637, // 16 pipes (4 PKRs) 8 bpe @ SW_4K_D_X @ Navi2x
     638, // 16 pipes (4 PKRs) 16 bpe @ SW_4K_D_X @ Navi2x
     107, // 8 pipes (8 PKRs) 1 bpe @ SW_4K_D_X @ Navi2x
      21, // 8 pipes (8 PKRs) 2 bpe @ SW_4K_D_X @ Navi2x
     639, // 8 pipes (8 PKRs) 4 bpe @ SW_4K_D_X @ Navi2x
     108, // 8 pipes (8 PKRs) 8 bpe @ SW_4K_D_X @ Navi2x
     109, // 8 pipes (8 PKRs) 16 bpe @ SW_4K_D_X @ Navi2x
     640, // 16 pipes (8 PKRs) 1 bpe @ SW_4K_D_X @ Navi2x
     555, // 16 pipes (8 PKRs) 2 bpe @ SW_4K_D_X @ Navi2x
     641, // 16 pipes (8 PKRs) 4 bpe @ SW_4K_D_X @ Navi2x
     642, // 16 pipes (8 PKRs) 8 bpe @ SW_4K_D_X @ Navi2x
     643, // 16 pipes (8 PKRs) 16 bpe @ SW_4K_D_X @ Navi2x
     644, // 32 pipes (8 PKRs) 1 bpe @ SW_4K_D_X @ Navi2x
     560, // 32 pipes (8 PKRs) 2 bpe @ SW_4K_D_X @ Navi2x
     645, // 32 pipes (8 PKRs) 4 bpe @ SW_4K_D_X @ Navi2x
     646, // 32 pipes (8 PKRs) 8 bpe @ SW_4K_D_X @ Navi2x
     647, // 32 pipes (8 PKRs) 16 bpe @ SW_4K_D_X @ Navi2x
     110, // 16 pipes (16 PKRs) 1 bpe @ SW_4K_D_X @ Navi2x
      26, // 16 pipes (16 PKRs) 2 bpe @ SW_4K_D_X @ Navi2x
     648, // 16 pipes (16 PKRs) 4 bpe @ SW_4K_D_X @ Navi2x
     111, // 16 pipes (16 PKRs) 8 bpe @ SW_4K_D_X @ Navi2x
     112, // 16 pipes (16 PKRs) 16 bpe @ SW_4K_D_X @ Navi2x
     649, // 32 pipes (16 PKRs) 1 bpe @ SW_4K_D_X @ Navi2x
     565, // 32 pipes (16 PKRs) 2 bpe @ SW_4K_D_X @ Navi2x
     650, // 32 pipes (16 PKRs) 4 bpe @ SW_4K_D_X @ Navi2x
     651, // 32 pipes (16 PKRs) 8 bpe @ SW_4K_D_X @ Navi2x
     652, // 32 pipes (16 PKRs) 16 bpe @ SW_4K_D_X @ Navi2x
     653, // 64 pipes (16 PKRs) 1 bpe @ SW_4K_D_X @ Navi2x
     570, // 64 pipes (16 PKRs) 2 bpe @ SW_4K_D_X @ Navi2x
     654, // 64 pipes (16 PKRs) 4 bpe @ SW_4K_D_X @ Navi2x
     655, // 64 pipes (16 PKRs) 8 bpe @ SW_4K_D_X @ Navi2x
     656, // 64 pipes (16 PKRs) 16 bpe @ SW_4K_D_X @ Navi2x
     110, // 32 pipes (32 PKRs) 1 bpe @ SW_4K_D_X @ Navi2x
      26, // 32 pipes (32 PKRs) 2 bpe @ SW_4K_D_X @ Navi2x
     648, // 32 pipes (32 PKRs) 4 bpe @ SW_4K_D_X @ Navi2x
     111, // 32 pipes (32 PKRs) 8 bpe @ SW_4K_D_X @ Navi2x
     112, // 32 pipes (32 PKRs) 16 bpe @ SW_4K_D_X @ Navi2x
     649, // 64 pipes (32 PKRs) 1 bpe @ SW_4K_D_X @ Navi2x
     565, // 64 pipes (32 PKRs) 2 bpe @ SW_4K_D_X @ Navi2x
     650, // 64 pipes (32 PKRs) 4 bpe @ SW_4K_D_X @ Navi2x
     651, // 64 pipes (32 PKRs) 8 bpe @ SW_4K_D_X @ Navi2x
     652, // 64 pipes (32 PKRs) 16 bpe @ SW_4K_D_X @ Navi2x
};

const UINT_16 SW_4K_S3_RBPLUS_PATIDX[] =
{
     419, // 1 pipes (1 PKRs) 1 bpe @ SW_4K_S3 @ Navi2x
     420, // 1 pipes (1 PKRs) 2 bpe @ SW_4K_S3 @ Navi2x
     421, // 1 pipes (1 PKRs) 4 bpe @ SW_4K_S3 @ Navi2x
     422, // 1 pipes (1 PKRs) 8 bpe @ SW_4K_S3 @ Navi2x
     423, // 1 pipes (1 PKRs) 16 bpe @ SW_4K_S3 @ Navi2x
     419, // 2 pipes (1-2 PKRs) 1 bpe @ SW_4K_S3 @ Navi2x
     420, // 2 pipes (1-2 PKRs) 2 bpe @ SW_4K_S3 @ Navi2x
     421, // 2 pipes (1-2 PKRs) 4 bpe @ SW_4K_S3 @ Navi2x
     422, // 2 pipes (1-2 PKRs) 8 bpe @ SW_4K_S3 @ Navi2x
     423, // 2 pipes (1-2 PKRs) 16 bpe @ SW_4K_S3 @ Navi2x
     419, // 4 pipes (1-2 PKRs) 1 bpe @ SW_4K_S3 @ Navi2x
     420, // 4 pipes (1-2 PKRs) 2 bpe @ SW_4K_S3 @ Navi2x
     421, // 4 pipes (1-2 PKRs) 4 bpe @ SW_4K_S3 @ Navi2x
     422, // 4 pipes (1-2 PKRs) 8 bpe @ SW_4K_S3 @ Navi2x
     423, // 4 pipes (1-2 PKRs) 16 bpe @ SW_4K_S3 @ Navi2x
     419, // 8 pipes (2 PKRs) 1 bpe @ SW_4K_S3 @ Navi2x
     420, // 8 pipes (2 PKRs) 2 bpe @ SW_4K_S3 @ Navi2x
     421, // 8 pipes (2 PKRs) 4 bpe @ SW_4K_S3 @ Navi2x
     422, // 8 pipes (2 PKRs) 8 bpe @ SW_4K_S3 @ Navi2x
     423, // 8 pipes (2 PKRs) 16 bpe @ SW_4K_S3 @ Navi2x
     419, // 4 pipes (4 PKRs) 1 bpe @ SW_4K_S3 @ Navi2x
     420, // 4 pipes (4 PKRs) 2 bpe @ SW_4K_S3 @ Navi2x
     421, // 4 pipes (4 PKRs) 4 bpe @ SW_4K_S3 @ Navi2x
     422, // 4 pipes (4 PKRs) 8 bpe @ SW_4K_S3 @ Navi2x
     423, // 4 pipes (4 PKRs) 16 bpe @ SW_4K_S3 @ Navi2x
     419, // 8 pipes (4 PKRs) 1 bpe @ SW_4K_S3 @ Navi2x
     420, // 8 pipes (4 PKRs) 2 bpe @ SW_4K_S3 @ Navi2x
     421, // 8 pipes (4 PKRs) 4 bpe @ SW_4K_S3 @ Navi2x
     422, // 8 pipes (4 PKRs) 8 bpe @ SW_4K_S3 @ Navi2x
     423, // 8 pipes (4 PKRs) 16 bpe @ SW_4K_S3 @ Navi2x
     419, // 16 pipes (4 PKRs) 1 bpe @ SW_4K_S3 @ Navi2x
     420, // 16 pipes (4 PKRs) 2 bpe @ SW_4K_S3 @ Navi2x
     421, // 16 pipes (4 PKRs) 4 bpe @ SW_4K_S3 @ Navi2x
     422, // 16 pipes (4 PKRs) 8 bpe @ SW_4K_S3 @ Navi2x
     423, // 16 pipes (4 PKRs) 16 bpe @ SW_4K_S3 @ Navi2x
     419, // 8 pipes (8 PKRs) 1 bpe @ SW_4K_S3 @ Navi2x
     420, // 8 pipes (8 PKRs) 2 bpe @ SW_4K_S3 @ Navi2x
     421, // 8 pipes (8 PKRs) 4 bpe @ SW_4K_S3 @ Navi2x
     422, // 8 pipes (8 PKRs) 8 bpe @ SW_4K_S3 @ Navi2x
     423, // 8 pipes (8 PKRs) 16 bpe @ SW_4K_S3 @ Navi2x
     419, // 16 pipes (8 PKRs) 1 bpe @ SW_4K_S3 @ Navi2x
     420, // 16 pipes (8 PKRs) 2 bpe @ SW_4K_S3 @ Navi2x
     421, // 16 pipes (8 PKRs) 4 bpe @ SW_4K_S3 @ Navi2x
     422, // 16 pipes (8 PKRs) 8 bpe @ SW_4K_S3 @ Navi2x
     423, // 16 pipes (8 PKRs) 16 bpe @ SW_4K_S3 @ Navi2x
     419, // 32 pipes (8 PKRs) 1 bpe @ SW_4K_S3 @ Navi2x
     420, // 32 pipes (8 PKRs) 2 bpe @ SW_4K_S3 @ Navi2x
     421, // 32 pipes (8 PKRs) 4 bpe @ SW_4K_S3 @ Navi2x
     422, // 32 pipes (8 PKRs) 8 bpe @ SW_4K_S3 @ Navi2x
     423, // 32 pipes (8 PKRs) 16 bpe @ SW_4K_S3 @ Navi2x
     419, // 16 pipes (16 PKRs) 1 bpe @ SW_4K_S3 @ Navi2x
     420, // 16 pipes (16 PKRs) 2 bpe @ SW_4K_S3 @ Navi2x
     421, // 16 pipes (16 PKRs) 4 bpe @ SW_4K_S3 @ Navi2x
     422, // 16 pipes (16 PKRs) 8 bpe @ SW_4K_S3 @ Navi2x
     423, // 16 pipes (16 PKRs) 16 bpe @ SW_4K_S3 @ Navi2x
     419, // 32 pipes (16 PKRs) 1 bpe @ SW_4K_S3 @ Navi2x
     420, // 32 pipes (16 PKRs) 2 bpe @ SW_4K_S3 @ Navi2x
     421, // 32 pipes (16 PKRs) 4 bpe @ SW_4K_S3 @ Navi2x
     422, // 32 pipes (16 PKRs) 8 bpe @ SW_4K_S3 @ Navi2x
     423, // 32 pipes (16 PKRs) 16 bpe @ SW_4K_S3 @ Navi2x
     419, // 64 pipes (16 PKRs) 1 bpe @ SW_4K_S3 @ Navi2x
     420, // 64 pipes (16 PKRs) 2 bpe @ SW_4K_S3 @ Navi2x
     421, // 64 pipes (16 PKRs) 4 bpe @ SW_4K_S3 @ Navi2x
     422, // 64 pipes (16 PKRs) 8 bpe @ SW_4K_S3 @ Navi2x
     423, // 64 pipes (16 PKRs) 16 bpe @ SW_4K_S3 @ Navi2x
     419, // 32 pipes (32 PKRs) 1 bpe @ SW_4K_S3 @ Navi2x
     420, // 32 pipes (32 PKRs) 2 bpe @ SW_4K_S3 @ Navi2x
     421, // 32 pipes (32 PKRs) 4 bpe @ SW_4K_S3 @ Navi2x
     422, // 32 pipes (32 PKRs) 8 bpe @ SW_4K_S3 @ Navi2x
     423, // 32 pipes (32 PKRs) 16 bpe @ SW_4K_S3 @ Navi2x
     419, // 64 pipes (32 PKRs) 1 bpe @ SW_4K_S3 @ Navi2x
     420, // 64 pipes (32 PKRs) 2 bpe @ SW_4K_S3 @ Navi2x
     421, // 64 pipes (32 PKRs) 4 bpe @ SW_4K_S3 @ Navi2x
     422, // 64 pipes (32 PKRs) 8 bpe @ SW_4K_S3 @ Navi2x
     423, // 64 pipes (32 PKRs) 16 bpe @ SW_4K_S3 @ Navi2x
};

const UINT_16 SW_4K_S3_X_RBPLUS_PATIDX[] =
{
     419, // 1 pipes (1 PKRs) 1 bpe @ SW_4K_S3_X @ Navi2x
     420, // 1 pipes (1 PKRs) 2 bpe @ SW_4K_S3_X @ Navi2x
     421, // 1 pipes (1 PKRs) 4 bpe @ SW_4K_S3_X @ Navi2x
     422, // 1 pipes (1 PKRs) 8 bpe @ SW_4K_S3_X @ Navi2x
     423, // 1 pipes (1 PKRs) 16 bpe @ SW_4K_S3_X @ Navi2x
     424, // 2 pipes (1-2 PKRs) 1 bpe @ SW_4K_S3_X @ Navi2x
     425, // 2 pipes (1-2 PKRs) 2 bpe @ SW_4K_S3_X @ Navi2x
     426, // 2 pipes (1-2 PKRs) 4 bpe @ SW_4K_S3_X @ Navi2x
     427, // 2 pipes (1-2 PKRs) 8 bpe @ SW_4K_S3_X @ Navi2x
     428, // 2 pipes (1-2 PKRs) 16 bpe @ SW_4K_S3_X @ Navi2x
     429, // 4 pipes (1-2 PKRs) 1 bpe @ SW_4K_S3_X @ Navi2x
     430, // 4 pipes (1-2 PKRs) 2 bpe @ SW_4K_S3_X @ Navi2x
     431, // 4 pipes (1-2 PKRs) 4 bpe @ SW_4K_S3_X @ Navi2x
     432, // 4 pipes (1-2 PKRs) 8 bpe @ SW_4K_S3_X @ Navi2x
     433, // 4 pipes (1-2 PKRs) 16 bpe @ SW_4K_S3_X @ Navi2x
     434, // 8 pipes (2 PKRs) 1 bpe @ SW_4K_S3_X @ Navi2x
     435, // 8 pipes (2 PKRs) 2 bpe @ SW_4K_S3_X @ Navi2x
     436, // 8 pipes (2 PKRs) 4 bpe @ SW_4K_S3_X @ Navi2x
     437, // 8 pipes (2 PKRs) 8 bpe @ SW_4K_S3_X @ Navi2x
     438, // 8 pipes (2 PKRs) 16 bpe @ SW_4K_S3_X @ Navi2x
     429, // 4 pipes (4 PKRs) 1 bpe @ SW_4K_S3_X @ Navi2x
     430, // 4 pipes (4 PKRs) 2 bpe @ SW_4K_S3_X @ Navi2x
     431, // 4 pipes (4 PKRs) 4 bpe @ SW_4K_S3_X @ Navi2x
     432, // 4 pipes (4 PKRs) 8 bpe @ SW_4K_S3_X @ Navi2x
     433, // 4 pipes (4 PKRs) 16 bpe @ SW_4K_S3_X @ Navi2x
     434, // 8 pipes (4 PKRs) 1 bpe @ SW_4K_S3_X @ Navi2x
     435, // 8 pipes (4 PKRs) 2 bpe @ SW_4K_S3_X @ Navi2x
     436, // 8 pipes (4 PKRs) 4 bpe @ SW_4K_S3_X @ Navi2x
     437, // 8 pipes (4 PKRs) 8 bpe @ SW_4K_S3_X @ Navi2x
     438, // 8 pipes (4 PKRs) 16 bpe @ SW_4K_S3_X @ Navi2x
     439, // 16 pipes (4 PKRs) 1 bpe @ SW_4K_S3_X @ Navi2x
     440, // 16 pipes (4 PKRs) 2 bpe @ SW_4K_S3_X @ Navi2x
     441, // 16 pipes (4 PKRs) 4 bpe @ SW_4K_S3_X @ Navi2x
     442, // 16 pipes (4 PKRs) 8 bpe @ SW_4K_S3_X @ Navi2x
     443, // 16 pipes (4 PKRs) 16 bpe @ SW_4K_S3_X @ Navi2x
     434, // 8 pipes (8 PKRs) 1 bpe @ SW_4K_S3_X @ Navi2x
     435, // 8 pipes (8 PKRs) 2 bpe @ SW_4K_S3_X @ Navi2x
     436, // 8 pipes (8 PKRs) 4 bpe @ SW_4K_S3_X @ Navi2x
     437, // 8 pipes (8 PKRs) 8 bpe @ SW_4K_S3_X @ Navi2x
     438, // 8 pipes (8 PKRs) 16 bpe @ SW_4K_S3_X @ Navi2x
     439, // 16 pipes (8 PKRs) 1 bpe @ SW_4K_S3_X @ Navi2x
     440, // 16 pipes (8 PKRs) 2 bpe @ SW_4K_S3_X @ Navi2x
     441, // 16 pipes (8 PKRs) 4 bpe @ SW_4K_S3_X @ Navi2x
     442, // 16 pipes (8 PKRs) 8 bpe @ SW_4K_S3_X @ Navi2x
     443, // 16 pipes (8 PKRs) 16 bpe @ SW_4K_S3_X @ Navi2x
     439, // 32 pipes (8 PKRs) 1 bpe @ SW_4K_S3_X @ Navi2x
     440, // 32 pipes (8 PKRs) 2 bpe @ SW_4K_S3_X @ Navi2x
     441, // 32 pipes (8 PKRs) 4 bpe @ SW_4K_S3_X @ Navi2x
     442, // 32 pipes (8 PKRs) 8 bpe @ SW_4K_S3_X @ Navi2x
     443, // 32 pipes (8 PKRs) 16 bpe @ SW_4K_S3_X @ Navi2x
     439, // 16 pipes (16 PKRs) 1 bpe @ SW_4K_S3_X @ Navi2x
     440, // 16 pipes (16 PKRs) 2 bpe @ SW_4K_S3_X @ Navi2x
     441, // 16 pipes (16 PKRs) 4 bpe @ SW_4K_S3_X @ Navi2x
     442, // 16 pipes (16 PKRs) 8 bpe @ SW_4K_S3_X @ Navi2x
     443, // 16 pipes (16 PKRs) 16 bpe @ SW_4K_S3_X @ Navi2x
     439, // 32 pipes (16 PKRs) 1 bpe @ SW_4K_S3_X @ Navi2x
     440, // 32 pipes (16 PKRs) 2 bpe @ SW_4K_S3_X @ Navi2x
     441, // 32 pipes (16 PKRs) 4 bpe @ SW_4K_S3_X @ Navi2x
     442, // 32 pipes (16 PKRs) 8 bpe @ SW_4K_S3_X @ Navi2x
     443, // 32 pipes (16 PKRs) 16 bpe @ SW_4K_S3_X @ Navi2x
     439, // 64 pipes (16 PKRs) 1 bpe @ SW_4K_S3_X @ Navi2x
     440, // 64 pipes (16 PKRs) 2 bpe @ SW_4K_S3_X @ Navi2x
     441, // 64 pipes (16 PKRs) 4 bpe @ SW_4K_S3_X @ Navi2x
     442, // 64 pipes (16 PKRs) 8 bpe @ SW_4K_S3_X @ Navi2x
     443, // 64 pipes (16 PKRs) 16 bpe @ SW_4K_S3_X @ Navi2x
     439, // 32 pipes (32 PKRs) 1 bpe @ SW_4K_S3_X @ Navi2x
     440, // 32 pipes (32 PKRs) 2 bpe @ SW_4K_S3_X @ Navi2x
     441, // 32 pipes (32 PKRs) 4 bpe @ SW_4K_S3_X @ Navi2x
     442, // 32 pipes (32 PKRs) 8 bpe @ SW_4K_S3_X @ Navi2x
     443, // 32 pipes (32 PKRs) 16 bpe @ SW_4K_S3_X @ Navi2x
     439, // 64 pipes (32 PKRs) 1 bpe @ SW_4K_S3_X @ Navi2x
     440, // 64 pipes (32 PKRs) 2 bpe @ SW_4K_S3_X @ Navi2x
     441, // 64 pipes (32 PKRs) 4 bpe @ SW_4K_S3_X @ Navi2x
     442, // 64 pipes (32 PKRs) 8 bpe @ SW_4K_S3_X @ Navi2x
     443, // 64 pipes (32 PKRs) 16 bpe @ SW_4K_S3_X @ Navi2x
};

const UINT_16 SW_64K_S_RBPLUS_PATIDX[] =
{
      30, // 1 pipes (1 PKRs) 1 bpe @ SW_64K_S @ Navi2x
      31, // 1 pipes (1 PKRs) 2 bpe @ SW_64K_S @ Navi2x
      32, // 1 pipes (1 PKRs) 4 bpe @ SW_64K_S @ Navi2x
      33, // 1 pipes (1 PKRs) 8 bpe @ SW_64K_S @ Navi2x
      34, // 1 pipes (1 PKRs) 16 bpe @ SW_64K_S @ Navi2x
      30, // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_S @ Navi2x
      31, // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_S @ Navi2x
      32, // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_S @ Navi2x
      33, // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_S @ Navi2x
      34, // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_S @ Navi2x
      30, // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_S @ Navi2x
      31, // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_S @ Navi2x
      32, // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_S @ Navi2x
      33, // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_S @ Navi2x
      34, // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_S @ Navi2x
      30, // 8 pipes (2 PKRs) 1 bpe @ SW_64K_S @ Navi2x
      31, // 8 pipes (2 PKRs) 2 bpe @ SW_64K_S @ Navi2x
      32, // 8 pipes (2 PKRs) 4 bpe @ SW_64K_S @ Navi2x
      33, // 8 pipes (2 PKRs) 8 bpe @ SW_64K_S @ Navi2x
      34, // 8 pipes (2 PKRs) 16 bpe @ SW_64K_S @ Navi2x
      30, // 4 pipes (4 PKRs) 1 bpe @ SW_64K_S @ Navi2x
      31, // 4 pipes (4 PKRs) 2 bpe @ SW_64K_S @ Navi2x
      32, // 4 pipes (4 PKRs) 4 bpe @ SW_64K_S @ Navi2x
      33, // 4 pipes (4 PKRs) 8 bpe @ SW_64K_S @ Navi2x
      34, // 4 pipes (4 PKRs) 16 bpe @ SW_64K_S @ Navi2x
      30, // 8 pipes (4 PKRs) 1 bpe @ SW_64K_S @ Navi2x
      31, // 8 pipes (4 PKRs) 2 bpe @ SW_64K_S @ Navi2x
      32, // 8 pipes (4 PKRs) 4 bpe @ SW_64K_S @ Navi2x
      33, // 8 pipes (4 PKRs) 8 bpe @ SW_64K_S @ Navi2x
      34, // 8 pipes (4 PKRs) 16 bpe @ SW_64K_S @ Navi2x
      30, // 16 pipes (4 PKRs) 1 bpe @ SW_64K_S @ Navi2x
      31, // 16 pipes (4 PKRs) 2 bpe @ SW_64K_S @ Navi2x
      32, // 16 pipes (4 PKRs) 4 bpe @ SW_64K_S @ Navi2x
      33, // 16 pipes (4 PKRs) 8 bpe @ SW_64K_S @ Navi2x
      34, // 16 pipes (4 PKRs) 16 bpe @ SW_64K_S @ Navi2x
      30, // 8 pipes (8 PKRs) 1 bpe @ SW_64K_S @ Navi2x
      31, // 8 pipes (8 PKRs) 2 bpe @ SW_64K_S @ Navi2x
      32, // 8 pipes (8 PKRs) 4 bpe @ SW_64K_S @ Navi2x
      33, // 8 pipes (8 PKRs) 8 bpe @ SW_64K_S @ Navi2x
      34, // 8 pipes (8 PKRs) 16 bpe @ SW_64K_S @ Navi2x
      30, // 16 pipes (8 PKRs) 1 bpe @ SW_64K_S @ Navi2x
      31, // 16 pipes (8 PKRs) 2 bpe @ SW_64K_S @ Navi2x
      32, // 16 pipes (8 PKRs) 4 bpe @ SW_64K_S @ Navi2x
      33, // 16 pipes (8 PKRs) 8 bpe @ SW_64K_S @ Navi2x
      34, // 16 pipes (8 PKRs) 16 bpe @ SW_64K_S @ Navi2x
      30, // 32 pipes (8 PKRs) 1 bpe @ SW_64K_S @ Navi2x
      31, // 32 pipes (8 PKRs) 2 bpe @ SW_64K_S @ Navi2x
      32, // 32 pipes (8 PKRs) 4 bpe @ SW_64K_S @ Navi2x
      33, // 32 pipes (8 PKRs) 8 bpe @ SW_64K_S @ Navi2x
      34, // 32 pipes (8 PKRs) 16 bpe @ SW_64K_S @ Navi2x
      30, // 16 pipes (16 PKRs) 1 bpe @ SW_64K_S @ Navi2x
      31, // 16 pipes (16 PKRs) 2 bpe @ SW_64K_S @ Navi2x
      32, // 16 pipes (16 PKRs) 4 bpe @ SW_64K_S @ Navi2x
      33, // 16 pipes (16 PKRs) 8 bpe @ SW_64K_S @ Navi2x
      34, // 16 pipes (16 PKRs) 16 bpe @ SW_64K_S @ Navi2x
      30, // 32 pipes (16 PKRs) 1 bpe @ SW_64K_S @ Navi2x
      31, // 32 pipes (16 PKRs) 2 bpe @ SW_64K_S @ Navi2x
      32, // 32 pipes (16 PKRs) 4 bpe @ SW_64K_S @ Navi2x
      33, // 32 pipes (16 PKRs) 8 bpe @ SW_64K_S @ Navi2x
      34, // 32 pipes (16 PKRs) 16 bpe @ SW_64K_S @ Navi2x
      30, // 64 pipes (16 PKRs) 1 bpe @ SW_64K_S @ Navi2x
      31, // 64 pipes (16 PKRs) 2 bpe @ SW_64K_S @ Navi2x
      32, // 64 pipes (16 PKRs) 4 bpe @ SW_64K_S @ Navi2x
      33, // 64 pipes (16 PKRs) 8 bpe @ SW_64K_S @ Navi2x
      34, // 64 pipes (16 PKRs) 16 bpe @ SW_64K_S @ Navi2x
      30, // 32 pipes (32 PKRs) 1 bpe @ SW_64K_S @ Navi2x
      31, // 32 pipes (32 PKRs) 2 bpe @ SW_64K_S @ Navi2x
      32, // 32 pipes (32 PKRs) 4 bpe @ SW_64K_S @ Navi2x
      33, // 32 pipes (32 PKRs) 8 bpe @ SW_64K_S @ Navi2x
      34, // 32 pipes (32 PKRs) 16 bpe @ SW_64K_S @ Navi2x
      30, // 64 pipes (32 PKRs) 1 bpe @ SW_64K_S @ Navi2x
      31, // 64 pipes (32 PKRs) 2 bpe @ SW_64K_S @ Navi2x
      32, // 64 pipes (32 PKRs) 4 bpe @ SW_64K_S @ Navi2x
      33, // 64 pipes (32 PKRs) 8 bpe @ SW_64K_S @ Navi2x
      34, // 64 pipes (32 PKRs) 16 bpe @ SW_64K_S @ Navi2x
};

const UINT_16 SW_64K_D_RBPLUS_PATIDX[] =
{
     113, // 1 pipes (1 PKRs) 1 bpe @ SW_64K_D @ Navi2x
      31, // 1 pipes (1 PKRs) 2 bpe @ SW_64K_D @ Navi2x
     657, // 1 pipes (1 PKRs) 4 bpe @ SW_64K_D @ Navi2x
     114, // 1 pipes (1 PKRs) 8 bpe @ SW_64K_D @ Navi2x
     115, // 1 pipes (1 PKRs) 16 bpe @ SW_64K_D @ Navi2x
     113, // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_D @ Navi2x
      31, // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_D @ Navi2x
     657, // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_D @ Navi2x
     114, // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_D @ Navi2x
     115, // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_D @ Navi2x
     113, // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_D @ Navi2x
      31, // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_D @ Navi2x
     657, // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_D @ Navi2x
     114, // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_D @ Navi2x
     115, // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_D @ Navi2x
     113, // 8 pipes (2 PKRs) 1 bpe @ SW_64K_D @ Navi2x
      31, // 8 pipes (2 PKRs) 2 bpe @ SW_64K_D @ Navi2x
     657, // 8 pipes (2 PKRs) 4 bpe @ SW_64K_D @ Navi2x
     114, // 8 pipes (2 PKRs) 8 bpe @ SW_64K_D @ Navi2x
     115, // 8 pipes (2 PKRs) 16 bpe @ SW_64K_D @ Navi2x
     113, // 4 pipes (4 PKRs) 1 bpe @ SW_64K_D @ Navi2x
      31, // 4 pipes (4 PKRs) 2 bpe @ SW_64K_D @ Navi2x
     657, // 4 pipes (4 PKRs) 4 bpe @ SW_64K_D @ Navi2x
     114, // 4 pipes (4 PKRs) 8 bpe @ SW_64K_D @ Navi2x
     115, // 4 pipes (4 PKRs) 16 bpe @ SW_64K_D @ Navi2x
     113, // 8 pipes (4 PKRs) 1 bpe @ SW_64K_D @ Navi2x
      31, // 8 pipes (4 PKRs) 2 bpe @ SW_64K_D @ Navi2x
     657, // 8 pipes (4 PKRs) 4 bpe @ SW_64K_D @ Navi2x
     114, // 8 pipes (4 PKRs) 8 bpe @ SW_64K_D @ Navi2x
     115, // 8 pipes (4 PKRs) 16 bpe @ SW_64K_D @ Navi2x
     113, // 16 pipes (4 PKRs) 1 bpe @ SW_64K_D @ Navi2x
      31, // 16 pipes (4 PKRs) 2 bpe @ SW_64K_D @ Navi2x
     657, // 16 pipes (4 PKRs) 4 bpe @ SW_64K_D @ Navi2x
     114, // 16 pipes (4 PKRs) 8 bpe @ SW_64K_D @ Navi2x
     115, // 16 pipes (4 PKRs) 16 bpe @ SW_64K_D @ Navi2x
     113, // 8 pipes (8 PKRs) 1 bpe @ SW_64K_D @ Navi2x
      31, // 8 pipes (8 PKRs) 2 bpe @ SW_64K_D @ Navi2x
     657, // 8 pipes (8 PKRs) 4 bpe @ SW_64K_D @ Navi2x
     114, // 8 pipes (8 PKRs) 8 bpe @ SW_64K_D @ Navi2x
     115, // 8 pipes (8 PKRs) 16 bpe @ SW_64K_D @ Navi2x
     113, // 16 pipes (8 PKRs) 1 bpe @ SW_64K_D @ Navi2x
      31, // 16 pipes (8 PKRs) 2 bpe @ SW_64K_D @ Navi2x
     657, // 16 pipes (8 PKRs) 4 bpe @ SW_64K_D @ Navi2x
     114, // 16 pipes (8 PKRs) 8 bpe @ SW_64K_D @ Navi2x
     115, // 16 pipes (8 PKRs) 16 bpe @ SW_64K_D @ Navi2x
     113, // 32 pipes (8 PKRs) 1 bpe @ SW_64K_D @ Navi2x
      31, // 32 pipes (8 PKRs) 2 bpe @ SW_64K_D @ Navi2x
     657, // 32 pipes (8 PKRs) 4 bpe @ SW_64K_D @ Navi2x
     114, // 32 pipes (8 PKRs) 8 bpe @ SW_64K_D @ Navi2x
     115, // 32 pipes (8 PKRs) 16 bpe @ SW_64K_D @ Navi2x
     113, // 16 pipes (16 PKRs) 1 bpe @ SW_64K_D @ Navi2x
      31, // 16 pipes (16 PKRs) 2 bpe @ SW_64K_D @ Navi2x
     657, // 16 pipes (16 PKRs) 4 bpe @ SW_64K_D @ Navi2x
     114, // 16 pipes (16 PKRs) 8 bpe @ SW_64K_D @ Navi2x
     115, // 16 pipes (16 PKRs) 16 bpe @ SW_64K_D @ Navi2x
     113, // 32 pipes (16 PKRs) 1 bpe @ SW_64K_D @ Navi2x
      31, // 32 pipes (16 PKRs) 2 bpe @ SW_64K_D @ Navi2x
     657, // 32 pipes (16 PKRs) 4 bpe @ SW_64K_D @ Navi2x
     114, // 32 pipes (16 PKRs) 8 bpe @ SW_64K_D @ Navi2x
     115, // 32 pipes (16 PKRs) 16 bpe @ SW_64K_D @ Navi2x
     113, // 64 pipes (16 PKRs) 1 bpe @ SW_64K_D @ Navi2x
      31, // 64 pipes (16 PKRs) 2 bpe @ SW_64K_D @ Navi2x
     657, // 64 pipes (16 PKRs) 4 bpe @ SW_64K_D @ Navi2x
     114, // 64 pipes (16 PKRs) 8 bpe @ SW_64K_D @ Navi2x
     115, // 64 pipes (16 PKRs) 16 bpe @ SW_64K_D @ Navi2x
     113, // 32 pipes (32 PKRs) 1 bpe @ SW_64K_D @ Navi2x
      31, // 32 pipes (32 PKRs) 2 bpe @ SW_64K_D @ Navi2x
     657, // 32 pipes (32 PKRs) 4 bpe @ SW_64K_D @ Navi2x
     114, // 32 pipes (32 PKRs) 8 bpe @ SW_64K_D @ Navi2x
     115, // 32 pipes (32 PKRs) 16 bpe @ SW_64K_D @ Navi2x
     113, // 64 pipes (32 PKRs) 1 bpe @ SW_64K_D @ Navi2x
      31, // 64 pipes (32 PKRs) 2 bpe @ SW_64K_D @ Navi2x
     657, // 64 pipes (32 PKRs) 4 bpe @ SW_64K_D @ Navi2x
     114, // 64 pipes (32 PKRs) 8 bpe @ SW_64K_D @ Navi2x
     115, // 64 pipes (32 PKRs) 16 bpe @ SW_64K_D @ Navi2x
};

const UINT_16 SW_64K_S_T_RBPLUS_PATIDX[] =
{
      30, // 1 pipes (1 PKRs) 1 bpe @ SW_64K_S_T @ Navi2x
      31, // 1 pipes (1 PKRs) 2 bpe @ SW_64K_S_T @ Navi2x
      32, // 1 pipes (1 PKRs) 4 bpe @ SW_64K_S_T @ Navi2x
      33, // 1 pipes (1 PKRs) 8 bpe @ SW_64K_S_T @ Navi2x
      34, // 1 pipes (1 PKRs) 16 bpe @ SW_64K_S_T @ Navi2x
      65, // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_S_T @ Navi2x
      66, // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_S_T @ Navi2x
      67, // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_S_T @ Navi2x
      68, // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_S_T @ Navi2x
      69, // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_S_T @ Navi2x
      70, // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_S_T @ Navi2x
      71, // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_S_T @ Navi2x
      72, // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_S_T @ Navi2x
      73, // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_S_T @ Navi2x
      74, // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_S_T @ Navi2x
      75, // 8 pipes (2 PKRs) 1 bpe @ SW_64K_S_T @ Navi2x
      76, // 8 pipes (2 PKRs) 2 bpe @ SW_64K_S_T @ Navi2x
      77, // 8 pipes (2 PKRs) 4 bpe @ SW_64K_S_T @ Navi2x
      78, // 8 pipes (2 PKRs) 8 bpe @ SW_64K_S_T @ Navi2x
      79, // 8 pipes (2 PKRs) 16 bpe @ SW_64K_S_T @ Navi2x
      70, // 4 pipes (4 PKRs) 1 bpe @ SW_64K_S_T @ Navi2x
      71, // 4 pipes (4 PKRs) 2 bpe @ SW_64K_S_T @ Navi2x
      72, // 4 pipes (4 PKRs) 4 bpe @ SW_64K_S_T @ Navi2x
      73, // 4 pipes (4 PKRs) 8 bpe @ SW_64K_S_T @ Navi2x
      74, // 4 pipes (4 PKRs) 16 bpe @ SW_64K_S_T @ Navi2x
      75, // 8 pipes (4 PKRs) 1 bpe @ SW_64K_S_T @ Navi2x
      76, // 8 pipes (4 PKRs) 2 bpe @ SW_64K_S_T @ Navi2x
      77, // 8 pipes (4 PKRs) 4 bpe @ SW_64K_S_T @ Navi2x
      78, // 8 pipes (4 PKRs) 8 bpe @ SW_64K_S_T @ Navi2x
      79, // 8 pipes (4 PKRs) 16 bpe @ SW_64K_S_T @ Navi2x
      80, // 16 pipes (4 PKRs) 1 bpe @ SW_64K_S_T @ Navi2x
      81, // 16 pipes (4 PKRs) 2 bpe @ SW_64K_S_T @ Navi2x
      82, // 16 pipes (4 PKRs) 4 bpe @ SW_64K_S_T @ Navi2x
      83, // 16 pipes (4 PKRs) 8 bpe @ SW_64K_S_T @ Navi2x
      84, // 16 pipes (4 PKRs) 16 bpe @ SW_64K_S_T @ Navi2x
      75, // 8 pipes (8 PKRs) 1 bpe @ SW_64K_S_T @ Navi2x
      76, // 8 pipes (8 PKRs) 2 bpe @ SW_64K_S_T @ Navi2x
      77, // 8 pipes (8 PKRs) 4 bpe @ SW_64K_S_T @ Navi2x
      78, // 8 pipes (8 PKRs) 8 bpe @ SW_64K_S_T @ Navi2x
      79, // 8 pipes (8 PKRs) 16 bpe @ SW_64K_S_T @ Navi2x
      80, // 16 pipes (8 PKRs) 1 bpe @ SW_64K_S_T @ Navi2x
      81, // 16 pipes (8 PKRs) 2 bpe @ SW_64K_S_T @ Navi2x
      82, // 16 pipes (8 PKRs) 4 bpe @ SW_64K_S_T @ Navi2x
      83, // 16 pipes (8 PKRs) 8 bpe @ SW_64K_S_T @ Navi2x
      84, // 16 pipes (8 PKRs) 16 bpe @ SW_64K_S_T @ Navi2x
      85, // 32 pipes (8 PKRs) 1 bpe @ SW_64K_S_T @ Navi2x
      86, // 32 pipes (8 PKRs) 2 bpe @ SW_64K_S_T @ Navi2x
      87, // 32 pipes (8 PKRs) 4 bpe @ SW_64K_S_T @ Navi2x
      88, // 32 pipes (8 PKRs) 8 bpe @ SW_64K_S_T @ Navi2x
      89, // 32 pipes (8 PKRs) 16 bpe @ SW_64K_S_T @ Navi2x
      80, // 16 pipes (16 PKRs) 1 bpe @ SW_64K_S_T @ Navi2x
      81, // 16 pipes (16 PKRs) 2 bpe @ SW_64K_S_T @ Navi2x
      82, // 16 pipes (16 PKRs) 4 bpe @ SW_64K_S_T @ Navi2x
      83, // 16 pipes (16 PKRs) 8 bpe @ SW_64K_S_T @ Navi2x
      84, // 16 pipes (16 PKRs) 16 bpe @ SW_64K_S_T @ Navi2x
      85, // 32 pipes (16 PKRs) 1 bpe @ SW_64K_S_T @ Navi2x
      86, // 32 pipes (16 PKRs) 2 bpe @ SW_64K_S_T @ Navi2x
      87, // 32 pipes (16 PKRs) 4 bpe @ SW_64K_S_T @ Navi2x
      88, // 32 pipes (16 PKRs) 8 bpe @ SW_64K_S_T @ Navi2x
      89, // 32 pipes (16 PKRs) 16 bpe @ SW_64K_S_T @ Navi2x
      90, // 64 pipes (16 PKRs) 1 bpe @ SW_64K_S_T @ Navi2x
      91, // 64 pipes (16 PKRs) 2 bpe @ SW_64K_S_T @ Navi2x
      92, // 64 pipes (16 PKRs) 4 bpe @ SW_64K_S_T @ Navi2x
      93, // 64 pipes (16 PKRs) 8 bpe @ SW_64K_S_T @ Navi2x
      94, // 64 pipes (16 PKRs) 16 bpe @ SW_64K_S_T @ Navi2x
      85, // 32 pipes (32 PKRs) 1 bpe @ SW_64K_S_T @ Navi2x
      86, // 32 pipes (32 PKRs) 2 bpe @ SW_64K_S_T @ Navi2x
      87, // 32 pipes (32 PKRs) 4 bpe @ SW_64K_S_T @ Navi2x
      88, // 32 pipes (32 PKRs) 8 bpe @ SW_64K_S_T @ Navi2x
      89, // 32 pipes (32 PKRs) 16 bpe @ SW_64K_S_T @ Navi2x
      90, // 64 pipes (32 PKRs) 1 bpe @ SW_64K_S_T @ Navi2x
      91, // 64 pipes (32 PKRs) 2 bpe @ SW_64K_S_T @ Navi2x
      92, // 64 pipes (32 PKRs) 4 bpe @ SW_64K_S_T @ Navi2x
      93, // 64 pipes (32 PKRs) 8 bpe @ SW_64K_S_T @ Navi2x
      94, // 64 pipes (32 PKRs) 16 bpe @ SW_64K_S_T @ Navi2x
};

const UINT_16 SW_64K_D_T_RBPLUS_PATIDX[] =
{
     113, // 1 pipes (1 PKRs) 1 bpe @ SW_64K_D_T @ Navi2x
      31, // 1 pipes (1 PKRs) 2 bpe @ SW_64K_D_T @ Navi2x
     657, // 1 pipes (1 PKRs) 4 bpe @ SW_64K_D_T @ Navi2x
     114, // 1 pipes (1 PKRs) 8 bpe @ SW_64K_D_T @ Navi2x
     115, // 1 pipes (1 PKRs) 16 bpe @ SW_64K_D_T @ Navi2x
     134, // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_D_T @ Navi2x
      66, // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_D_T @ Navi2x
     699, // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_D_T @ Navi2x
     135, // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_D_T @ Navi2x
     136, // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_D_T @ Navi2x
     137, // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_D_T @ Navi2x
      71, // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_D_T @ Navi2x
     700, // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_D_T @ Navi2x
     138, // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_D_T @ Navi2x
     139, // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_D_T @ Navi2x
     140, // 8 pipes (2 PKRs) 1 bpe @ SW_64K_D_T @ Navi2x
      76, // 8 pipes (2 PKRs) 2 bpe @ SW_64K_D_T @ Navi2x
     701, // 8 pipes (2 PKRs) 4 bpe @ SW_64K_D_T @ Navi2x
     141, // 8 pipes (2 PKRs) 8 bpe @ SW_64K_D_T @ Navi2x
     142, // 8 pipes (2 PKRs) 16 bpe @ SW_64K_D_T @ Navi2x
     137, // 4 pipes (4 PKRs) 1 bpe @ SW_64K_D_T @ Navi2x
      71, // 4 pipes (4 PKRs) 2 bpe @ SW_64K_D_T @ Navi2x
     700, // 4 pipes (4 PKRs) 4 bpe @ SW_64K_D_T @ Navi2x
     138, // 4 pipes (4 PKRs) 8 bpe @ SW_64K_D_T @ Navi2x
     139, // 4 pipes (4 PKRs) 16 bpe @ SW_64K_D_T @ Navi2x
     140, // 8 pipes (4 PKRs) 1 bpe @ SW_64K_D_T @ Navi2x
      76, // 8 pipes (4 PKRs) 2 bpe @ SW_64K_D_T @ Navi2x
     701, // 8 pipes (4 PKRs) 4 bpe @ SW_64K_D_T @ Navi2x
     141, // 8 pipes (4 PKRs) 8 bpe @ SW_64K_D_T @ Navi2x
     142, // 8 pipes (4 PKRs) 16 bpe @ SW_64K_D_T @ Navi2x
     143, // 16 pipes (4 PKRs) 1 bpe @ SW_64K_D_T @ Navi2x
      81, // 16 pipes (4 PKRs) 2 bpe @ SW_64K_D_T @ Navi2x
     702, // 16 pipes (4 PKRs) 4 bpe @ SW_64K_D_T @ Navi2x
     144, // 16 pipes (4 PKRs) 8 bpe @ SW_64K_D_T @ Navi2x
     145, // 16 pipes (4 PKRs) 16 bpe @ SW_64K_D_T @ Navi2x
     140, // 8 pipes (8 PKRs) 1 bpe @ SW_64K_D_T @ Navi2x
      76, // 8 pipes (8 PKRs) 2 bpe @ SW_64K_D_T @ Navi2x
     701, // 8 pipes (8 PKRs) 4 bpe @ SW_64K_D_T @ Navi2x
     141, // 8 pipes (8 PKRs) 8 bpe @ SW_64K_D_T @ Navi2x
     142, // 8 pipes (8 PKRs) 16 bpe @ SW_64K_D_T @ Navi2x
     143, // 16 pipes (8 PKRs) 1 bpe @ SW_64K_D_T @ Navi2x
      81, // 16 pipes (8 PKRs) 2 bpe @ SW_64K_D_T @ Navi2x
     702, // 16 pipes (8 PKRs) 4 bpe @ SW_64K_D_T @ Navi2x
     144, // 16 pipes (8 PKRs) 8 bpe @ SW_64K_D_T @ Navi2x
     145, // 16 pipes (8 PKRs) 16 bpe @ SW_64K_D_T @ Navi2x
     146, // 32 pipes (8 PKRs) 1 bpe @ SW_64K_D_T @ Navi2x
      86, // 32 pipes (8 PKRs) 2 bpe @ SW_64K_D_T @ Navi2x
     703, // 32 pipes (8 PKRs) 4 bpe @ SW_64K_D_T @ Navi2x
     147, // 32 pipes (8 PKRs) 8 bpe @ SW_64K_D_T @ Navi2x
     148, // 32 pipes (8 PKRs) 16 bpe @ SW_64K_D_T @ Navi2x
     143, // 16 pipes (16 PKRs) 1 bpe @ SW_64K_D_T @ Navi2x
      81, // 16 pipes (16 PKRs) 2 bpe @ SW_64K_D_T @ Navi2x
     702, // 16 pipes (16 PKRs) 4 bpe @ SW_64K_D_T @ Navi2x
     144, // 16 pipes (16 PKRs) 8 bpe @ SW_64K_D_T @ Navi2x
     145, // 16 pipes (16 PKRs) 16 bpe @ SW_64K_D_T @ Navi2x
     146, // 32 pipes (16 PKRs) 1 bpe @ SW_64K_D_T @ Navi2x
      86, // 32 pipes (16 PKRs) 2 bpe @ SW_64K_D_T @ Navi2x
     703, // 32 pipes (16 PKRs) 4 bpe @ SW_64K_D_T @ Navi2x
     147, // 32 pipes (16 PKRs) 8 bpe @ SW_64K_D_T @ Navi2x
     148, // 32 pipes (16 PKRs) 16 bpe @ SW_64K_D_T @ Navi2x
     149, // 64 pipes (16 PKRs) 1 bpe @ SW_64K_D_T @ Navi2x
      91, // 64 pipes (16 PKRs) 2 bpe @ SW_64K_D_T @ Navi2x
     704, // 64 pipes (16 PKRs) 4 bpe @ SW_64K_D_T @ Navi2x
     150, // 64 pipes (16 PKRs) 8 bpe @ SW_64K_D_T @ Navi2x
     151, // 64 pipes (16 PKRs) 16 bpe @ SW_64K_D_T @ Navi2x
     146, // 32 pipes (32 PKRs) 1 bpe @ SW_64K_D_T @ Navi2x
      86, // 32 pipes (32 PKRs) 2 bpe @ SW_64K_D_T @ Navi2x
     703, // 32 pipes (32 PKRs) 4 bpe @ SW_64K_D_T @ Navi2x
     147, // 32 pipes (32 PKRs) 8 bpe @ SW_64K_D_T @ Navi2x
     148, // 32 pipes (32 PKRs) 16 bpe @ SW_64K_D_T @ Navi2x
     149, // 64 pipes (32 PKRs) 1 bpe @ SW_64K_D_T @ Navi2x
      91, // 64 pipes (32 PKRs) 2 bpe @ SW_64K_D_T @ Navi2x
     704, // 64 pipes (32 PKRs) 4 bpe @ SW_64K_D_T @ Navi2x
     150, // 64 pipes (32 PKRs) 8 bpe @ SW_64K_D_T @ Navi2x
     151, // 64 pipes (32 PKRs) 16 bpe @ SW_64K_D_T @ Navi2x
};

const UINT_16 SW_64K_S_X_RBPLUS_PATIDX[] =
{
      30, // 1 pipes (1 PKRs) 1 bpe @ SW_64K_S_X @ Navi2x
      31, // 1 pipes (1 PKRs) 2 bpe @ SW_64K_S_X @ Navi2x
      32, // 1 pipes (1 PKRs) 4 bpe @ SW_64K_S_X @ Navi2x
      33, // 1 pipes (1 PKRs) 8 bpe @ SW_64K_S_X @ Navi2x
      34, // 1 pipes (1 PKRs) 16 bpe @ SW_64K_S_X @ Navi2x
      35, // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_S_X @ Navi2x
      36, // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_S_X @ Navi2x
      37, // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_S_X @ Navi2x
      38, // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_S_X @ Navi2x
      39, // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_S_X @ Navi2x
     574, // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_S_X @ Navi2x
     575, // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_S_X @ Navi2x
     576, // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_S_X @ Navi2x
     577, // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_S_X @ Navi2x
     578, // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_S_X @ Navi2x
     579, // 8 pipes (2 PKRs) 1 bpe @ SW_64K_S_X @ Navi2x
     580, // 8 pipes (2 PKRs) 2 bpe @ SW_64K_S_X @ Navi2x
     581, // 8 pipes (2 PKRs) 4 bpe @ SW_64K_S_X @ Navi2x
     582, // 8 pipes (2 PKRs) 8 bpe @ SW_64K_S_X @ Navi2x
     583, // 8 pipes (2 PKRs) 16 bpe @ SW_64K_S_X @ Navi2x
      40, // 4 pipes (4 PKRs) 1 bpe @ SW_64K_S_X @ Navi2x
      41, // 4 pipes (4 PKRs) 2 bpe @ SW_64K_S_X @ Navi2x
      42, // 4 pipes (4 PKRs) 4 bpe @ SW_64K_S_X @ Navi2x
      43, // 4 pipes (4 PKRs) 8 bpe @ SW_64K_S_X @ Navi2x
      44, // 4 pipes (4 PKRs) 16 bpe @ SW_64K_S_X @ Navi2x
     584, // 8 pipes (4 PKRs) 1 bpe @ SW_64K_S_X @ Navi2x
     585, // 8 pipes (4 PKRs) 2 bpe @ SW_64K_S_X @ Navi2x
     586, // 8 pipes (4 PKRs) 4 bpe @ SW_64K_S_X @ Navi2x
     587, // 8 pipes (4 PKRs) 8 bpe @ SW_64K_S_X @ Navi2x
     588, // 8 pipes (4 PKRs) 16 bpe @ SW_64K_S_X @ Navi2x
     589, // 16 pipes (4 PKRs) 1 bpe @ SW_64K_S_X @ Navi2x
     590, // 16 pipes (4 PKRs) 2 bpe @ SW_64K_S_X @ Navi2x
     591, // 16 pipes (4 PKRs) 4 bpe @ SW_64K_S_X @ Navi2x
     592, // 16 pipes (4 PKRs) 8 bpe @ SW_64K_S_X @ Navi2x
     593, // 16 pipes (4 PKRs) 16 bpe @ SW_64K_S_X @ Navi2x
      45, // 8 pipes (8 PKRs) 1 bpe @ SW_64K_S_X @ Navi2x
      46, // 8 pipes (8 PKRs) 2 bpe @ SW_64K_S_X @ Navi2x
      47, // 8 pipes (8 PKRs) 4 bpe @ SW_64K_S_X @ Navi2x
      48, // 8 pipes (8 PKRs) 8 bpe @ SW_64K_S_X @ Navi2x
      49, // 8 pipes (8 PKRs) 16 bpe @ SW_64K_S_X @ Navi2x
     594, // 16 pipes (8 PKRs) 1 bpe @ SW_64K_S_X @ Navi2x
     595, // 16 pipes (8 PKRs) 2 bpe @ SW_64K_S_X @ Navi2x
     596, // 16 pipes (8 PKRs) 4 bpe @ SW_64K_S_X @ Navi2x
     597, // 16 pipes (8 PKRs) 8 bpe @ SW_64K_S_X @ Navi2x
     598, // 16 pipes (8 PKRs) 16 bpe @ SW_64K_S_X @ Navi2x
     599, // 32 pipes (8 PKRs) 1 bpe @ SW_64K_S_X @ Navi2x
     600, // 32 pipes (8 PKRs) 2 bpe @ SW_64K_S_X @ Navi2x
     601, // 32 pipes (8 PKRs) 4 bpe @ SW_64K_S_X @ Navi2x
     602, // 32 pipes (8 PKRs) 8 bpe @ SW_64K_S_X @ Navi2x
     603, // 32 pipes (8 PKRs) 16 bpe @ SW_64K_S_X @ Navi2x
      50, // 16 pipes (16 PKRs) 1 bpe @ SW_64K_S_X @ Navi2x
      51, // 16 pipes (16 PKRs) 2 bpe @ SW_64K_S_X @ Navi2x
      52, // 16 pipes (16 PKRs) 4 bpe @ SW_64K_S_X @ Navi2x
      53, // 16 pipes (16 PKRs) 8 bpe @ SW_64K_S_X @ Navi2x
      54, // 16 pipes (16 PKRs) 16 bpe @ SW_64K_S_X @ Navi2x
     604, // 32 pipes (16 PKRs) 1 bpe @ SW_64K_S_X @ Navi2x
     605, // 32 pipes (16 PKRs) 2 bpe @ SW_64K_S_X @ Navi2x
     606, // 32 pipes (16 PKRs) 4 bpe @ SW_64K_S_X @ Navi2x
     607, // 32 pipes (16 PKRs) 8 bpe @ SW_64K_S_X @ Navi2x
     608, // 32 pipes (16 PKRs) 16 bpe @ SW_64K_S_X @ Navi2x
     609, // 64 pipes (16 PKRs) 1 bpe @ SW_64K_S_X @ Navi2x
     610, // 64 pipes (16 PKRs) 2 bpe @ SW_64K_S_X @ Navi2x
     611, // 64 pipes (16 PKRs) 4 bpe @ SW_64K_S_X @ Navi2x
     612, // 64 pipes (16 PKRs) 8 bpe @ SW_64K_S_X @ Navi2x
     613, // 64 pipes (16 PKRs) 16 bpe @ SW_64K_S_X @ Navi2x
      55, // 32 pipes (32 PKRs) 1 bpe @ SW_64K_S_X @ Navi2x
      56, // 32 pipes (32 PKRs) 2 bpe @ SW_64K_S_X @ Navi2x
      57, // 32 pipes (32 PKRs) 4 bpe @ SW_64K_S_X @ Navi2x
      58, // 32 pipes (32 PKRs) 8 bpe @ SW_64K_S_X @ Navi2x
      59, // 32 pipes (32 PKRs) 16 bpe @ SW_64K_S_X @ Navi2x
     614, // 64 pipes (32 PKRs) 1 bpe @ SW_64K_S_X @ Navi2x
     615, // 64 pipes (32 PKRs) 2 bpe @ SW_64K_S_X @ Navi2x
     616, // 64 pipes (32 PKRs) 4 bpe @ SW_64K_S_X @ Navi2x
     617, // 64 pipes (32 PKRs) 8 bpe @ SW_64K_S_X @ Navi2x
     618, // 64 pipes (32 PKRs) 16 bpe @ SW_64K_S_X @ Navi2x
};

const UINT_16 SW_64K_D_X_RBPLUS_PATIDX[] =
{
     113, // 1 pipes (1 PKRs) 1 bpe @ SW_64K_D_X @ Navi2x
      31, // 1 pipes (1 PKRs) 2 bpe @ SW_64K_D_X @ Navi2x
     657, // 1 pipes (1 PKRs) 4 bpe @ SW_64K_D_X @ Navi2x
     114, // 1 pipes (1 PKRs) 8 bpe @ SW_64K_D_X @ Navi2x
     115, // 1 pipes (1 PKRs) 16 bpe @ SW_64K_D_X @ Navi2x
     116, // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_D_X @ Navi2x
      36, // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_D_X @ Navi2x
     658, // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_D_X @ Navi2x
     117, // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_D_X @ Navi2x
     118, // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_D_X @ Navi2x
     659, // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_D_X @ Navi2x
     575, // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_D_X @ Navi2x
     660, // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_D_X @ Navi2x
     661, // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_D_X @ Navi2x
     662, // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_D_X @ Navi2x
     663, // 8 pipes (2 PKRs) 1 bpe @ SW_64K_D_X @ Navi2x
     580, // 8 pipes (2 PKRs) 2 bpe @ SW_64K_D_X @ Navi2x
     664, // 8 pipes (2 PKRs) 4 bpe @ SW_64K_D_X @ Navi2x
     665, // 8 pipes (2 PKRs) 8 bpe @ SW_64K_D_X @ Navi2x
     666, // 8 pipes (2 PKRs) 16 bpe @ SW_64K_D_X @ Navi2x
     119, // 4 pipes (4 PKRs) 1 bpe @ SW_64K_D_X @ Navi2x
      41, // 4 pipes (4 PKRs) 2 bpe @ SW_64K_D_X @ Navi2x
     667, // 4 pipes (4 PKRs) 4 bpe @ SW_64K_D_X @ Navi2x
     120, // 4 pipes (4 PKRs) 8 bpe @ SW_64K_D_X @ Navi2x
     121, // 4 pipes (4 PKRs) 16 bpe @ SW_64K_D_X @ Navi2x
     668, // 8 pipes (4 PKRs) 1 bpe @ SW_64K_D_X @ Navi2x
     585, // 8 pipes (4 PKRs) 2 bpe @ SW_64K_D_X @ Navi2x
     669, // 8 pipes (4 PKRs) 4 bpe @ SW_64K_D_X @ Navi2x
     670, // 8 pipes (4 PKRs) 8 bpe @ SW_64K_D_X @ Navi2x
     671, // 8 pipes (4 PKRs) 16 bpe @ SW_64K_D_X @ Navi2x
     672, // 16 pipes (4 PKRs) 1 bpe @ SW_64K_D_X @ Navi2x
     590, // 16 pipes (4 PKRs) 2 bpe @ SW_64K_D_X @ Navi2x
     673, // 16 pipes (4 PKRs) 4 bpe @ SW_64K_D_X @ Navi2x
     674, // 16 pipes (4 PKRs) 8 bpe @ SW_64K_D_X @ Navi2x
     675, // 16 pipes (4 PKRs) 16 bpe @ SW_64K_D_X @ Navi2x
     122, // 8 pipes (8 PKRs) 1 bpe @ SW_64K_D_X @ Navi2x
      46, // 8 pipes (8 PKRs) 2 bpe @ SW_64K_D_X @ Navi2x
     676, // 8 pipes (8 PKRs) 4 bpe @ SW_64K_D_X @ Navi2x
     123, // 8 pipes (8 PKRs) 8 bpe @ SW_64K_D_X @ Navi2x
     124, // 8 pipes (8 PKRs) 16 bpe @ SW_64K_D_X @ Navi2x
     677, // 16 pipes (8 PKRs) 1 bpe @ SW_64K_D_X @ Navi2x
     595, // 16 pipes (8 PKRs) 2 bpe @ SW_64K_D_X @ Navi2x
     678, // 16 pipes (8 PKRs) 4 bpe @ SW_64K_D_X @ Navi2x
     679, // 16 pipes (8 PKRs) 8 bpe @ SW_64K_D_X @ Navi2x
     680, // 16 pipes (8 PKRs) 16 bpe @ SW_64K_D_X @ Navi2x
     681, // 32 pipes (8 PKRs) 1 bpe @ SW_64K_D_X @ Navi2x
     600, // 32 pipes (8 PKRs) 2 bpe @ SW_64K_D_X @ Navi2x
     682, // 32 pipes (8 PKRs) 4 bpe @ SW_64K_D_X @ Navi2x
     683, // 32 pipes (8 PKRs) 8 bpe @ SW_64K_D_X @ Navi2x
     684, // 32 pipes (8 PKRs) 16 bpe @ SW_64K_D_X @ Navi2x
     125, // 16 pipes (16 PKRs) 1 bpe @ SW_64K_D_X @ Navi2x
      51, // 16 pipes (16 PKRs) 2 bpe @ SW_64K_D_X @ Navi2x
     685, // 16 pipes (16 PKRs) 4 bpe @ SW_64K_D_X @ Navi2x
     126, // 16 pipes (16 PKRs) 8 bpe @ SW_64K_D_X @ Navi2x
     127, // 16 pipes (16 PKRs) 16 bpe @ SW_64K_D_X @ Navi2x
     686, // 32 pipes (16 PKRs) 1 bpe @ SW_64K_D_X @ Navi2x
     605, // 32 pipes (16 PKRs) 2 bpe @ SW_64K_D_X @ Navi2x
     687, // 32 pipes (16 PKRs) 4 bpe @ SW_64K_D_X @ Navi2x
     688, // 32 pipes (16 PKRs) 8 bpe @ SW_64K_D_X @ Navi2x
     689, // 32 pipes (16 PKRs) 16 bpe @ SW_64K_D_X @ Navi2x
     690, // 64 pipes (16 PKRs) 1 bpe @ SW_64K_D_X @ Navi2x
     610, // 64 pipes (16 PKRs) 2 bpe @ SW_64K_D_X @ Navi2x
     691, // 64 pipes (16 PKRs) 4 bpe @ SW_64K_D_X @ Navi2x
     692, // 64 pipes (16 PKRs) 8 bpe @ SW_64K_D_X @ Navi2x
     693, // 64 pipes (16 PKRs) 16 bpe @ SW_64K_D_X @ Navi2x
     128, // 32 pipes (32 PKRs) 1 bpe @ SW_64K_D_X @ Navi2x
      56, // 32 pipes (32 PKRs) 2 bpe @ SW_64K_D_X @ Navi2x
     694, // 32 pipes (32 PKRs) 4 bpe @ SW_64K_D_X @ Navi2x
     129, // 32 pipes (32 PKRs) 8 bpe @ SW_64K_D_X @ Navi2x
     130, // 32 pipes (32 PKRs) 16 bpe @ SW_64K_D_X @ Navi2x
     695, // 64 pipes (32 PKRs) 1 bpe @ SW_64K_D_X @ Navi2x
     615, // 64 pipes (32 PKRs) 2 bpe @ SW_64K_D_X @ Navi2x
     696, // 64 pipes (32 PKRs) 4 bpe @ SW_64K_D_X @ Navi2x
     697, // 64 pipes (32 PKRs) 8 bpe @ SW_64K_D_X @ Navi2x
     698, // 64 pipes (32 PKRs) 16 bpe @ SW_64K_D_X @ Navi2x
};

const UINT_16 SW_64K_R_X_1xaa_RBPLUS_PATIDX[] =
{
    1005, // 1 pipes (1 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1006, // 1 pipes (1 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1007, // 1 pipes (1 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1008, // 1 pipes (1 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ Navi2x
     709, // 1 pipes (1 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1009, // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1010, // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1011, // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1012, // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ Navi2x
     714, // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1013, // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1014, // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1015, // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1016, // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ Navi2x
     719, // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1017, // 8 pipes (2 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1018, // 8 pipes (2 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1019, // 8 pipes (2 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1020, // 8 pipes (2 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1021, // 8 pipes (2 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1022, // 4 pipes (4 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1023, // 4 pipes (4 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1024, // 4 pipes (4 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1025, // 4 pipes (4 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ Navi2x
     729, // 4 pipes (4 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1026, // 8 pipes (4 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1027, // 8 pipes (4 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1028, // 8 pipes (4 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1029, // 8 pipes (4 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ Navi2x
     734, // 8 pipes (4 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1030, // 16 pipes (4 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1031, // 16 pipes (4 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1032, // 16 pipes (4 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1033, // 16 pipes (4 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1034, // 16 pipes (4 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1035, // 8 pipes (8 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1036, // 8 pipes (8 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1037, // 8 pipes (8 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1038, // 8 pipes (8 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ Navi2x
     744, // 8 pipes (8 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1039, // 16 pipes (8 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1040, // 16 pipes (8 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1041, // 16 pipes (8 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1042, // 16 pipes (8 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ Navi2x
     749, // 16 pipes (8 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1043, // 32 pipes (8 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1044, // 32 pipes (8 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1045, // 32 pipes (8 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1046, // 32 pipes (8 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1047, // 32 pipes (8 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1048, // 16 pipes (16 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1049, // 16 pipes (16 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1050, // 16 pipes (16 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1051, // 16 pipes (16 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ Navi2x
     759, // 16 pipes (16 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1052, // 32 pipes (16 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1053, // 32 pipes (16 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1054, // 32 pipes (16 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1055, // 32 pipes (16 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ Navi2x
     764, // 32 pipes (16 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1056, // 64 pipes (16 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1057, // 64 pipes (16 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1058, // 64 pipes (16 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1059, // 64 pipes (16 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1060, // 64 pipes (16 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1061, // 32 pipes (32 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1062, // 32 pipes (32 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1063, // 32 pipes (32 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1064, // 32 pipes (32 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ Navi2x
     774, // 32 pipes (32 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1065, // 64 pipes (32 PKRs) 1 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1066, // 64 pipes (32 PKRs) 2 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1067, // 64 pipes (32 PKRs) 4 bpe @ SW_64K_R_X 1xaa @ Navi2x
    1068, // 64 pipes (32 PKRs) 8 bpe @ SW_64K_R_X 1xaa @ Navi2x
     779, // 64 pipes (32 PKRs) 16 bpe @ SW_64K_R_X 1xaa @ Navi2x
};

const UINT_16 SW_64K_R_X_2xaa_RBPLUS_PATIDX[] =
{
    1069, // 1 pipes (1 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1070, // 1 pipes (1 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1071, // 1 pipes (1 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1072, // 1 pipes (1 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1073, // 1 pipes (1 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1074, // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1075, // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1076, // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1077, // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1078, // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1079, // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1080, // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1081, // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1082, // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1083, // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1084, // 8 pipes (2 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1085, // 8 pipes (2 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1086, // 8 pipes (2 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1087, // 8 pipes (2 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1088, // 8 pipes (2 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1089, // 4 pipes (4 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1090, // 4 pipes (4 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1091, // 4 pipes (4 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1092, // 4 pipes (4 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1093, // 4 pipes (4 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1094, // 8 pipes (4 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1095, // 8 pipes (4 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1096, // 8 pipes (4 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1097, // 8 pipes (4 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1098, // 8 pipes (4 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1099, // 16 pipes (4 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1100, // 16 pipes (4 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1101, // 16 pipes (4 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1102, // 16 pipes (4 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1103, // 16 pipes (4 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1104, // 8 pipes (8 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1105, // 8 pipes (8 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1106, // 8 pipes (8 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1107, // 8 pipes (8 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1108, // 8 pipes (8 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1109, // 16 pipes (8 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1110, // 16 pipes (8 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1111, // 16 pipes (8 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1112, // 16 pipes (8 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1113, // 16 pipes (8 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1114, // 32 pipes (8 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1115, // 32 pipes (8 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1116, // 32 pipes (8 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1117, // 32 pipes (8 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1118, // 32 pipes (8 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1119, // 16 pipes (16 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1120, // 16 pipes (16 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1121, // 16 pipes (16 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1122, // 16 pipes (16 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1123, // 16 pipes (16 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1124, // 32 pipes (16 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1125, // 32 pipes (16 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1126, // 32 pipes (16 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1127, // 32 pipes (16 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1128, // 32 pipes (16 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1129, // 64 pipes (16 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1130, // 64 pipes (16 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1131, // 64 pipes (16 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1132, // 64 pipes (16 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1133, // 64 pipes (16 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1134, // 32 pipes (32 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1135, // 32 pipes (32 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1136, // 32 pipes (32 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1137, // 32 pipes (32 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1138, // 32 pipes (32 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1139, // 64 pipes (32 PKRs) 1 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1140, // 64 pipes (32 PKRs) 2 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1141, // 64 pipes (32 PKRs) 4 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1142, // 64 pipes (32 PKRs) 8 bpe @ SW_64K_R_X 2xaa @ Navi2x
    1143, // 64 pipes (32 PKRs) 16 bpe @ SW_64K_R_X 2xaa @ Navi2x
};

const UINT_16 SW_64K_R_X_4xaa_RBPLUS_PATIDX[] =
{
    1144, // 1 pipes (1 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1145, // 1 pipes (1 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1146, // 1 pipes (1 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1147, // 1 pipes (1 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1148, // 1 pipes (1 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1149, // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1150, // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1151, // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1152, // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1153, // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1154, // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1155, // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1156, // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1157, // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1158, // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1159, // 8 pipes (2 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1160, // 8 pipes (2 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1161, // 8 pipes (2 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1162, // 8 pipes (2 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1163, // 8 pipes (2 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1164, // 4 pipes (4 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1165, // 4 pipes (4 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1166, // 4 pipes (4 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1167, // 4 pipes (4 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1168, // 4 pipes (4 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1169, // 8 pipes (4 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1170, // 8 pipes (4 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1171, // 8 pipes (4 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1172, // 8 pipes (4 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1173, // 8 pipes (4 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1174, // 16 pipes (4 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1175, // 16 pipes (4 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1176, // 16 pipes (4 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1177, // 16 pipes (4 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1178, // 16 pipes (4 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1179, // 8 pipes (8 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1180, // 8 pipes (8 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1181, // 8 pipes (8 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1182, // 8 pipes (8 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1183, // 8 pipes (8 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1184, // 16 pipes (8 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1185, // 16 pipes (8 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1186, // 16 pipes (8 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1187, // 16 pipes (8 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1188, // 16 pipes (8 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1189, // 32 pipes (8 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1190, // 32 pipes (8 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1191, // 32 pipes (8 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1192, // 32 pipes (8 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1193, // 32 pipes (8 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1194, // 16 pipes (16 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1195, // 16 pipes (16 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1196, // 16 pipes (16 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1197, // 16 pipes (16 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1198, // 16 pipes (16 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1199, // 32 pipes (16 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1200, // 32 pipes (16 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1201, // 32 pipes (16 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1202, // 32 pipes (16 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1203, // 32 pipes (16 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1204, // 64 pipes (16 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1205, // 64 pipes (16 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1206, // 64 pipes (16 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1207, // 64 pipes (16 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1208, // 64 pipes (16 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1209, // 32 pipes (32 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1210, // 32 pipes (32 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1211, // 32 pipes (32 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1212, // 32 pipes (32 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1213, // 32 pipes (32 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1214, // 64 pipes (32 PKRs) 1 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1215, // 64 pipes (32 PKRs) 2 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1216, // 64 pipes (32 PKRs) 4 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1217, // 64 pipes (32 PKRs) 8 bpe @ SW_64K_R_X 4xaa @ Navi2x
    1218, // 64 pipes (32 PKRs) 16 bpe @ SW_64K_R_X 4xaa @ Navi2x
};

const UINT_16 SW_64K_R_X_8xaa_RBPLUS_PATIDX[] =
{
    1219, // 1 pipes (1 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1220, // 1 pipes (1 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1221, // 1 pipes (1 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1222, // 1 pipes (1 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1223, // 1 pipes (1 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1224, // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1225, // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1226, // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1227, // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1228, // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1229, // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1230, // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1231, // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1232, // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1233, // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1234, // 8 pipes (2 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1235, // 8 pipes (2 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1236, // 8 pipes (2 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1237, // 8 pipes (2 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1238, // 8 pipes (2 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1239, // 4 pipes (4 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1240, // 4 pipes (4 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1241, // 4 pipes (4 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1242, // 4 pipes (4 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1243, // 4 pipes (4 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1244, // 8 pipes (4 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1245, // 8 pipes (4 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1246, // 8 pipes (4 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1247, // 8 pipes (4 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1248, // 8 pipes (4 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1249, // 16 pipes (4 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1250, // 16 pipes (4 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1251, // 16 pipes (4 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1252, // 16 pipes (4 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1253, // 16 pipes (4 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1254, // 8 pipes (8 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1255, // 8 pipes (8 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1256, // 8 pipes (8 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1257, // 8 pipes (8 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1258, // 8 pipes (8 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1259, // 16 pipes (8 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1260, // 16 pipes (8 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1261, // 16 pipes (8 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1262, // 16 pipes (8 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1263, // 16 pipes (8 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1264, // 32 pipes (8 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1265, // 32 pipes (8 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1266, // 32 pipes (8 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1267, // 32 pipes (8 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1268, // 32 pipes (8 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1269, // 16 pipes (16 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1270, // 16 pipes (16 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1271, // 16 pipes (16 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1272, // 16 pipes (16 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1273, // 16 pipes (16 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1274, // 32 pipes (16 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1275, // 32 pipes (16 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1276, // 32 pipes (16 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1277, // 32 pipes (16 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1278, // 32 pipes (16 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1279, // 64 pipes (16 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1280, // 64 pipes (16 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1281, // 64 pipes (16 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1282, // 64 pipes (16 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1283, // 64 pipes (16 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1284, // 32 pipes (32 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1285, // 32 pipes (32 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1286, // 32 pipes (32 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1287, // 32 pipes (32 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1288, // 32 pipes (32 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1289, // 64 pipes (32 PKRs) 1 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1290, // 64 pipes (32 PKRs) 2 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1291, // 64 pipes (32 PKRs) 4 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1292, // 64 pipes (32 PKRs) 8 bpe @ SW_64K_R_X 8xaa @ Navi2x
    1293, // 64 pipes (32 PKRs) 16 bpe @ SW_64K_R_X 8xaa @ Navi2x
};

const UINT_16 SW_64K_Z_X_1xaa_RBPLUS_PATIDX[] =
{
     705, // 1 pipes (1 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     706, // 1 pipes (1 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     707, // 1 pipes (1 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     708, // 1 pipes (1 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     709, // 1 pipes (1 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     710, // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     711, // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     712, // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     713, // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     714, // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     715, // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     716, // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     717, // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     718, // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     719, // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     720, // 8 pipes (2 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     721, // 8 pipes (2 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     722, // 8 pipes (2 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     723, // 8 pipes (2 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     724, // 8 pipes (2 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     725, // 4 pipes (4 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     726, // 4 pipes (4 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     727, // 4 pipes (4 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     728, // 4 pipes (4 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     729, // 4 pipes (4 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     730, // 8 pipes (4 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     731, // 8 pipes (4 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     732, // 8 pipes (4 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     733, // 8 pipes (4 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     734, // 8 pipes (4 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     735, // 16 pipes (4 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     736, // 16 pipes (4 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     737, // 16 pipes (4 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     738, // 16 pipes (4 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     739, // 16 pipes (4 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     740, // 8 pipes (8 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     741, // 8 pipes (8 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     742, // 8 pipes (8 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     743, // 8 pipes (8 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     744, // 8 pipes (8 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     745, // 16 pipes (8 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     746, // 16 pipes (8 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     747, // 16 pipes (8 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     748, // 16 pipes (8 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     749, // 16 pipes (8 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     750, // 32 pipes (8 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     751, // 32 pipes (8 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     752, // 32 pipes (8 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     753, // 32 pipes (8 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     754, // 32 pipes (8 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     755, // 16 pipes (16 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     756, // 16 pipes (16 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     757, // 16 pipes (16 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     758, // 16 pipes (16 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     759, // 16 pipes (16 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     760, // 32 pipes (16 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     761, // 32 pipes (16 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     762, // 32 pipes (16 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     763, // 32 pipes (16 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     764, // 32 pipes (16 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     765, // 64 pipes (16 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     766, // 64 pipes (16 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     767, // 64 pipes (16 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     768, // 64 pipes (16 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     769, // 64 pipes (16 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     770, // 32 pipes (32 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     771, // 32 pipes (32 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     772, // 32 pipes (32 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     773, // 32 pipes (32 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     774, // 32 pipes (32 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     775, // 64 pipes (32 PKRs) 1 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     776, // 64 pipes (32 PKRs) 2 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     777, // 64 pipes (32 PKRs) 4 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     778, // 64 pipes (32 PKRs) 8 bpe @ SW_64K_Z_X 1xaa @ Navi2x
     779, // 64 pipes (32 PKRs) 16 bpe @ SW_64K_Z_X 1xaa @ Navi2x
};

const UINT_16 SW_64K_Z_X_2xaa_RBPLUS_PATIDX[] =
{
     780, // 1 pipes (1 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     781, // 1 pipes (1 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     782, // 1 pipes (1 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     783, // 1 pipes (1 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     784, // 1 pipes (1 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     785, // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     786, // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     787, // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     788, // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     789, // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     790, // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     791, // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     792, // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     793, // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     794, // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     795, // 8 pipes (2 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     796, // 8 pipes (2 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     797, // 8 pipes (2 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     798, // 8 pipes (2 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     799, // 8 pipes (2 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     800, // 4 pipes (4 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     801, // 4 pipes (4 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     802, // 4 pipes (4 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     803, // 4 pipes (4 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     804, // 4 pipes (4 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     805, // 8 pipes (4 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     806, // 8 pipes (4 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     807, // 8 pipes (4 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     808, // 8 pipes (4 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     809, // 8 pipes (4 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     810, // 16 pipes (4 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     811, // 16 pipes (4 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     812, // 16 pipes (4 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     813, // 16 pipes (4 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     814, // 16 pipes (4 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     815, // 8 pipes (8 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     816, // 8 pipes (8 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     817, // 8 pipes (8 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     818, // 8 pipes (8 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     819, // 8 pipes (8 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     820, // 16 pipes (8 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     821, // 16 pipes (8 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     822, // 16 pipes (8 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     823, // 16 pipes (8 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     824, // 16 pipes (8 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     825, // 32 pipes (8 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     826, // 32 pipes (8 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     827, // 32 pipes (8 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     828, // 32 pipes (8 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     829, // 32 pipes (8 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     830, // 16 pipes (16 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     831, // 16 pipes (16 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     832, // 16 pipes (16 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     833, // 16 pipes (16 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     834, // 16 pipes (16 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     835, // 32 pipes (16 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     836, // 32 pipes (16 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     837, // 32 pipes (16 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     838, // 32 pipes (16 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     839, // 32 pipes (16 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     840, // 64 pipes (16 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     841, // 64 pipes (16 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     842, // 64 pipes (16 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     843, // 64 pipes (16 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     844, // 64 pipes (16 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     845, // 32 pipes (32 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     846, // 32 pipes (32 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     847, // 32 pipes (32 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     848, // 32 pipes (32 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     849, // 32 pipes (32 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     850, // 64 pipes (32 PKRs) 1 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     851, // 64 pipes (32 PKRs) 2 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     852, // 64 pipes (32 PKRs) 4 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     853, // 64 pipes (32 PKRs) 8 bpe @ SW_64K_Z_X 2xaa @ Navi2x
     854, // 64 pipes (32 PKRs) 16 bpe @ SW_64K_Z_X 2xaa @ Navi2x
};

const UINT_16 SW_64K_Z_X_4xaa_RBPLUS_PATIDX[] =
{
     855, // 1 pipes (1 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     856, // 1 pipes (1 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     857, // 1 pipes (1 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     858, // 1 pipes (1 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     859, // 1 pipes (1 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     860, // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     861, // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     862, // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     863, // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     864, // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     865, // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     866, // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     867, // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     868, // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     869, // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     870, // 8 pipes (2 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     871, // 8 pipes (2 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     872, // 8 pipes (2 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     873, // 8 pipes (2 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     874, // 8 pipes (2 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     875, // 4 pipes (4 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     876, // 4 pipes (4 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     877, // 4 pipes (4 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     878, // 4 pipes (4 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     879, // 4 pipes (4 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     880, // 8 pipes (4 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     881, // 8 pipes (4 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     882, // 8 pipes (4 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     883, // 8 pipes (4 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     884, // 8 pipes (4 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     885, // 16 pipes (4 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     886, // 16 pipes (4 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     887, // 16 pipes (4 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     888, // 16 pipes (4 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     889, // 16 pipes (4 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     890, // 8 pipes (8 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     891, // 8 pipes (8 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     892, // 8 pipes (8 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     893, // 8 pipes (8 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     894, // 8 pipes (8 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     895, // 16 pipes (8 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     896, // 16 pipes (8 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     897, // 16 pipes (8 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     898, // 16 pipes (8 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     899, // 16 pipes (8 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     900, // 32 pipes (8 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     901, // 32 pipes (8 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     902, // 32 pipes (8 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     903, // 32 pipes (8 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     904, // 32 pipes (8 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     905, // 16 pipes (16 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     906, // 16 pipes (16 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     907, // 16 pipes (16 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     908, // 16 pipes (16 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     909, // 16 pipes (16 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     910, // 32 pipes (16 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     911, // 32 pipes (16 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     912, // 32 pipes (16 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     913, // 32 pipes (16 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     914, // 32 pipes (16 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     915, // 64 pipes (16 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     916, // 64 pipes (16 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     917, // 64 pipes (16 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     918, // 64 pipes (16 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     919, // 64 pipes (16 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     920, // 32 pipes (32 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     921, // 32 pipes (32 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     922, // 32 pipes (32 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     923, // 32 pipes (32 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     924, // 32 pipes (32 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     925, // 64 pipes (32 PKRs) 1 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     926, // 64 pipes (32 PKRs) 2 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     927, // 64 pipes (32 PKRs) 4 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     928, // 64 pipes (32 PKRs) 8 bpe @ SW_64K_Z_X 4xaa @ Navi2x
     929, // 64 pipes (32 PKRs) 16 bpe @ SW_64K_Z_X 4xaa @ Navi2x
};

const UINT_16 SW_64K_Z_X_8xaa_RBPLUS_PATIDX[] =
{
     930, // 1 pipes (1 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     931, // 1 pipes (1 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     932, // 1 pipes (1 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     933, // 1 pipes (1 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     934, // 1 pipes (1 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     935, // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     936, // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     937, // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     938, // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     939, // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     940, // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     941, // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     942, // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     943, // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     944, // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     945, // 8 pipes (2 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     946, // 8 pipes (2 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     947, // 8 pipes (2 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     948, // 8 pipes (2 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     949, // 8 pipes (2 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     950, // 4 pipes (4 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     951, // 4 pipes (4 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     952, // 4 pipes (4 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     953, // 4 pipes (4 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     954, // 4 pipes (4 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     955, // 8 pipes (4 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     956, // 8 pipes (4 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     957, // 8 pipes (4 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     958, // 8 pipes (4 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     959, // 8 pipes (4 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     960, // 16 pipes (4 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     961, // 16 pipes (4 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     962, // 16 pipes (4 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     963, // 16 pipes (4 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     964, // 16 pipes (4 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     965, // 8 pipes (8 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     966, // 8 pipes (8 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     967, // 8 pipes (8 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     968, // 8 pipes (8 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     969, // 8 pipes (8 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     970, // 16 pipes (8 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     971, // 16 pipes (8 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     972, // 16 pipes (8 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     973, // 16 pipes (8 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     974, // 16 pipes (8 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     975, // 32 pipes (8 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     976, // 32 pipes (8 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     977, // 32 pipes (8 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     978, // 32 pipes (8 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     979, // 32 pipes (8 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     980, // 16 pipes (16 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     981, // 16 pipes (16 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     982, // 16 pipes (16 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     983, // 16 pipes (16 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     984, // 16 pipes (16 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     985, // 32 pipes (16 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     986, // 32 pipes (16 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     987, // 32 pipes (16 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     988, // 32 pipes (16 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     989, // 32 pipes (16 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     990, // 64 pipes (16 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     991, // 64 pipes (16 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     992, // 64 pipes (16 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     993, // 64 pipes (16 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     994, // 64 pipes (16 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     995, // 32 pipes (32 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     996, // 32 pipes (32 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     997, // 32 pipes (32 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     998, // 32 pipes (32 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ Navi2x
     999, // 32 pipes (32 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ Navi2x
    1000, // 64 pipes (32 PKRs) 1 bpe @ SW_64K_Z_X 8xaa @ Navi2x
    1001, // 64 pipes (32 PKRs) 2 bpe @ SW_64K_Z_X 8xaa @ Navi2x
    1002, // 64 pipes (32 PKRs) 4 bpe @ SW_64K_Z_X 8xaa @ Navi2x
    1003, // 64 pipes (32 PKRs) 8 bpe @ SW_64K_Z_X 8xaa @ Navi2x
    1004, // 64 pipes (32 PKRs) 16 bpe @ SW_64K_Z_X 8xaa @ Navi2x
};

const UINT_16 SW_64K_S3_RBPLUS_PATIDX[] =
{
     444, // 1 pipes (1 PKRs) 1 bpe @ SW_64K_S3 @ Navi2x
     445, // 1 pipes (1 PKRs) 2 bpe @ SW_64K_S3 @ Navi2x
     446, // 1 pipes (1 PKRs) 4 bpe @ SW_64K_S3 @ Navi2x
     447, // 1 pipes (1 PKRs) 8 bpe @ SW_64K_S3 @ Navi2x
     448, // 1 pipes (1 PKRs) 16 bpe @ SW_64K_S3 @ Navi2x
     444, // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_S3 @ Navi2x
     445, // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_S3 @ Navi2x
     446, // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_S3 @ Navi2x
     447, // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_S3 @ Navi2x
     448, // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_S3 @ Navi2x
     444, // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_S3 @ Navi2x
     445, // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_S3 @ Navi2x
     446, // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_S3 @ Navi2x
     447, // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_S3 @ Navi2x
     448, // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_S3 @ Navi2x
     444, // 8 pipes (2 PKRs) 1 bpe @ SW_64K_S3 @ Navi2x
     445, // 8 pipes (2 PKRs) 2 bpe @ SW_64K_S3 @ Navi2x
     446, // 8 pipes (2 PKRs) 4 bpe @ SW_64K_S3 @ Navi2x
     447, // 8 pipes (2 PKRs) 8 bpe @ SW_64K_S3 @ Navi2x
     448, // 8 pipes (2 PKRs) 16 bpe @ SW_64K_S3 @ Navi2x
     444, // 4 pipes (4 PKRs) 1 bpe @ SW_64K_S3 @ Navi2x
     445, // 4 pipes (4 PKRs) 2 bpe @ SW_64K_S3 @ Navi2x
     446, // 4 pipes (4 PKRs) 4 bpe @ SW_64K_S3 @ Navi2x
     447, // 4 pipes (4 PKRs) 8 bpe @ SW_64K_S3 @ Navi2x
     448, // 4 pipes (4 PKRs) 16 bpe @ SW_64K_S3 @ Navi2x
     444, // 8 pipes (4 PKRs) 1 bpe @ SW_64K_S3 @ Navi2x
     445, // 8 pipes (4 PKRs) 2 bpe @ SW_64K_S3 @ Navi2x
     446, // 8 pipes (4 PKRs) 4 bpe @ SW_64K_S3 @ Navi2x
     447, // 8 pipes (4 PKRs) 8 bpe @ SW_64K_S3 @ Navi2x
     448, // 8 pipes (4 PKRs) 16 bpe @ SW_64K_S3 @ Navi2x
     444, // 16 pipes (4 PKRs) 1 bpe @ SW_64K_S3 @ Navi2x
     445, // 16 pipes (4 PKRs) 2 bpe @ SW_64K_S3 @ Navi2x
     446, // 16 pipes (4 PKRs) 4 bpe @ SW_64K_S3 @ Navi2x
     447, // 16 pipes (4 PKRs) 8 bpe @ SW_64K_S3 @ Navi2x
     448, // 16 pipes (4 PKRs) 16 bpe @ SW_64K_S3 @ Navi2x
     444, // 8 pipes (8 PKRs) 1 bpe @ SW_64K_S3 @ Navi2x
     445, // 8 pipes (8 PKRs) 2 bpe @ SW_64K_S3 @ Navi2x
     446, // 8 pipes (8 PKRs) 4 bpe @ SW_64K_S3 @ Navi2x
     447, // 8 pipes (8 PKRs) 8 bpe @ SW_64K_S3 @ Navi2x
     448, // 8 pipes (8 PKRs) 16 bpe @ SW_64K_S3 @ Navi2x
     444, // 16 pipes (8 PKRs) 1 bpe @ SW_64K_S3 @ Navi2x
     445, // 16 pipes (8 PKRs) 2 bpe @ SW_64K_S3 @ Navi2x
     446, // 16 pipes (8 PKRs) 4 bpe @ SW_64K_S3 @ Navi2x
     447, // 16 pipes (8 PKRs) 8 bpe @ SW_64K_S3 @ Navi2x
     448, // 16 pipes (8 PKRs) 16 bpe @ SW_64K_S3 @ Navi2x
     444, // 32 pipes (8 PKRs) 1 bpe @ SW_64K_S3 @ Navi2x
     445, // 32 pipes (8 PKRs) 2 bpe @ SW_64K_S3 @ Navi2x
     446, // 32 pipes (8 PKRs) 4 bpe @ SW_64K_S3 @ Navi2x
     447, // 32 pipes (8 PKRs) 8 bpe @ SW_64K_S3 @ Navi2x
     448, // 32 pipes (8 PKRs) 16 bpe @ SW_64K_S3 @ Navi2x
     444, // 16 pipes (16 PKRs) 1 bpe @ SW_64K_S3 @ Navi2x
     445, // 16 pipes (16 PKRs) 2 bpe @ SW_64K_S3 @ Navi2x
     446, // 16 pipes (16 PKRs) 4 bpe @ SW_64K_S3 @ Navi2x
     447, // 16 pipes (16 PKRs) 8 bpe @ SW_64K_S3 @ Navi2x
     448, // 16 pipes (16 PKRs) 16 bpe @ SW_64K_S3 @ Navi2x
     444, // 32 pipes (16 PKRs) 1 bpe @ SW_64K_S3 @ Navi2x
     445, // 32 pipes (16 PKRs) 2 bpe @ SW_64K_S3 @ Navi2x
     446, // 32 pipes (16 PKRs) 4 bpe @ SW_64K_S3 @ Navi2x
     447, // 32 pipes (16 PKRs) 8 bpe @ SW_64K_S3 @ Navi2x
     448, // 32 pipes (16 PKRs) 16 bpe @ SW_64K_S3 @ Navi2x
     444, // 64 pipes (16 PKRs) 1 bpe @ SW_64K_S3 @ Navi2x
     445, // 64 pipes (16 PKRs) 2 bpe @ SW_64K_S3 @ Navi2x
     446, // 64 pipes (16 PKRs) 4 bpe @ SW_64K_S3 @ Navi2x
     447, // 64 pipes (16 PKRs) 8 bpe @ SW_64K_S3 @ Navi2x
     448, // 64 pipes (16 PKRs) 16 bpe @ SW_64K_S3 @ Navi2x
     444, // 32 pipes (32 PKRs) 1 bpe @ SW_64K_S3 @ Navi2x
     445, // 32 pipes (32 PKRs) 2 bpe @ SW_64K_S3 @ Navi2x
     446, // 32 pipes (32 PKRs) 4 bpe @ SW_64K_S3 @ Navi2x
     447, // 32 pipes (32 PKRs) 8 bpe @ SW_64K_S3 @ Navi2x
     448, // 32 pipes (32 PKRs) 16 bpe @ SW_64K_S3 @ Navi2x
     444, // 64 pipes (32 PKRs) 1 bpe @ SW_64K_S3 @ Navi2x
     445, // 64 pipes (32 PKRs) 2 bpe @ SW_64K_S3 @ Navi2x
     446, // 64 pipes (32 PKRs) 4 bpe @ SW_64K_S3 @ Navi2x
     447, // 64 pipes (32 PKRs) 8 bpe @ SW_64K_S3 @ Navi2x
     448, // 64 pipes (32 PKRs) 16 bpe @ SW_64K_S3 @ Navi2x
};

const UINT_16 SW_64K_S3_X_RBPLUS_PATIDX[] =
{
     444, // 1 pipes (1 PKRs) 1 bpe @ SW_64K_S3_X @ Navi2x
     445, // 1 pipes (1 PKRs) 2 bpe @ SW_64K_S3_X @ Navi2x
     446, // 1 pipes (1 PKRs) 4 bpe @ SW_64K_S3_X @ Navi2x
     447, // 1 pipes (1 PKRs) 8 bpe @ SW_64K_S3_X @ Navi2x
     448, // 1 pipes (1 PKRs) 16 bpe @ SW_64K_S3_X @ Navi2x
     449, // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_S3_X @ Navi2x
     450, // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_S3_X @ Navi2x
     451, // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_S3_X @ Navi2x
     452, // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_S3_X @ Navi2x
     453, // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_S3_X @ Navi2x
     454, // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_S3_X @ Navi2x
     455, // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_S3_X @ Navi2x
     456, // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_S3_X @ Navi2x
     457, // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_S3_X @ Navi2x
     458, // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_S3_X @ Navi2x
     459, // 8 pipes (2 PKRs) 1 bpe @ SW_64K_S3_X @ Navi2x
     460, // 8 pipes (2 PKRs) 2 bpe @ SW_64K_S3_X @ Navi2x
     461, // 8 pipes (2 PKRs) 4 bpe @ SW_64K_S3_X @ Navi2x
     462, // 8 pipes (2 PKRs) 8 bpe @ SW_64K_S3_X @ Navi2x
     463, // 8 pipes (2 PKRs) 16 bpe @ SW_64K_S3_X @ Navi2x
     454, // 4 pipes (4 PKRs) 1 bpe @ SW_64K_S3_X @ Navi2x
     455, // 4 pipes (4 PKRs) 2 bpe @ SW_64K_S3_X @ Navi2x
     456, // 4 pipes (4 PKRs) 4 bpe @ SW_64K_S3_X @ Navi2x
     457, // 4 pipes (4 PKRs) 8 bpe @ SW_64K_S3_X @ Navi2x
     458, // 4 pipes (4 PKRs) 16 bpe @ SW_64K_S3_X @ Navi2x
     459, // 8 pipes (4 PKRs) 1 bpe @ SW_64K_S3_X @ Navi2x
     460, // 8 pipes (4 PKRs) 2 bpe @ SW_64K_S3_X @ Navi2x
     461, // 8 pipes (4 PKRs) 4 bpe @ SW_64K_S3_X @ Navi2x
     462, // 8 pipes (4 PKRs) 8 bpe @ SW_64K_S3_X @ Navi2x
     463, // 8 pipes (4 PKRs) 16 bpe @ SW_64K_S3_X @ Navi2x
     464, // 16 pipes (4 PKRs) 1 bpe @ SW_64K_S3_X @ Navi2x
     465, // 16 pipes (4 PKRs) 2 bpe @ SW_64K_S3_X @ Navi2x
     466, // 16 pipes (4 PKRs) 4 bpe @ SW_64K_S3_X @ Navi2x
     467, // 16 pipes (4 PKRs) 8 bpe @ SW_64K_S3_X @ Navi2x
     468, // 16 pipes (4 PKRs) 16 bpe @ SW_64K_S3_X @ Navi2x
     459, // 8 pipes (8 PKRs) 1 bpe @ SW_64K_S3_X @ Navi2x
     460, // 8 pipes (8 PKRs) 2 bpe @ SW_64K_S3_X @ Navi2x
     461, // 8 pipes (8 PKRs) 4 bpe @ SW_64K_S3_X @ Navi2x
     462, // 8 pipes (8 PKRs) 8 bpe @ SW_64K_S3_X @ Navi2x
     463, // 8 pipes (8 PKRs) 16 bpe @ SW_64K_S3_X @ Navi2x
     464, // 16 pipes (8 PKRs) 1 bpe @ SW_64K_S3_X @ Navi2x
     465, // 16 pipes (8 PKRs) 2 bpe @ SW_64K_S3_X @ Navi2x
     466, // 16 pipes (8 PKRs) 4 bpe @ SW_64K_S3_X @ Navi2x
     467, // 16 pipes (8 PKRs) 8 bpe @ SW_64K_S3_X @ Navi2x
     468, // 16 pipes (8 PKRs) 16 bpe @ SW_64K_S3_X @ Navi2x
     469, // 32 pipes (8 PKRs) 1 bpe @ SW_64K_S3_X @ Navi2x
     470, // 32 pipes (8 PKRs) 2 bpe @ SW_64K_S3_X @ Navi2x
     471, // 32 pipes (8 PKRs) 4 bpe @ SW_64K_S3_X @ Navi2x
     472, // 32 pipes (8 PKRs) 8 bpe @ SW_64K_S3_X @ Navi2x
     473, // 32 pipes (8 PKRs) 16 bpe @ SW_64K_S3_X @ Navi2x
     464, // 16 pipes (16 PKRs) 1 bpe @ SW_64K_S3_X @ Navi2x
     465, // 16 pipes (16 PKRs) 2 bpe @ SW_64K_S3_X @ Navi2x
     466, // 16 pipes (16 PKRs) 4 bpe @ SW_64K_S3_X @ Navi2x
     467, // 16 pipes (16 PKRs) 8 bpe @ SW_64K_S3_X @ Navi2x
     468, // 16 pipes (16 PKRs) 16 bpe @ SW_64K_S3_X @ Navi2x
     469, // 32 pipes (16 PKRs) 1 bpe @ SW_64K_S3_X @ Navi2x
     470, // 32 pipes (16 PKRs) 2 bpe @ SW_64K_S3_X @ Navi2x
     471, // 32 pipes (16 PKRs) 4 bpe @ SW_64K_S3_X @ Navi2x
     472, // 32 pipes (16 PKRs) 8 bpe @ SW_64K_S3_X @ Navi2x
     473, // 32 pipes (16 PKRs) 16 bpe @ SW_64K_S3_X @ Navi2x
     474, // 64 pipes (16 PKRs) 1 bpe @ SW_64K_S3_X @ Navi2x
     475, // 64 pipes (16 PKRs) 2 bpe @ SW_64K_S3_X @ Navi2x
     476, // 64 pipes (16 PKRs) 4 bpe @ SW_64K_S3_X @ Navi2x
     477, // 64 pipes (16 PKRs) 8 bpe @ SW_64K_S3_X @ Navi2x
     478, // 64 pipes (16 PKRs) 16 bpe @ SW_64K_S3_X @ Navi2x
     469, // 32 pipes (32 PKRs) 1 bpe @ SW_64K_S3_X @ Navi2x
     470, // 32 pipes (32 PKRs) 2 bpe @ SW_64K_S3_X @ Navi2x
     471, // 32 pipes (32 PKRs) 4 bpe @ SW_64K_S3_X @ Navi2x
     472, // 32 pipes (32 PKRs) 8 bpe @ SW_64K_S3_X @ Navi2x
     473, // 32 pipes (32 PKRs) 16 bpe @ SW_64K_S3_X @ Navi2x
     474, // 64 pipes (32 PKRs) 1 bpe @ SW_64K_S3_X @ Navi2x
     475, // 64 pipes (32 PKRs) 2 bpe @ SW_64K_S3_X @ Navi2x
     476, // 64 pipes (32 PKRs) 4 bpe @ SW_64K_S3_X @ Navi2x
     477, // 64 pipes (32 PKRs) 8 bpe @ SW_64K_S3_X @ Navi2x
     478, // 64 pipes (32 PKRs) 16 bpe @ SW_64K_S3_X @ Navi2x
};

const UINT_16 SW_64K_S3_T_RBPLUS_PATIDX[] =
{
     444, // 1 pipes (1 PKRs) 1 bpe @ SW_64K_S3_T @ Navi2x
     445, // 1 pipes (1 PKRs) 2 bpe @ SW_64K_S3_T @ Navi2x
     446, // 1 pipes (1 PKRs) 4 bpe @ SW_64K_S3_T @ Navi2x
     447, // 1 pipes (1 PKRs) 8 bpe @ SW_64K_S3_T @ Navi2x
     448, // 1 pipes (1 PKRs) 16 bpe @ SW_64K_S3_T @ Navi2x
     449, // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_S3_T @ Navi2x
     450, // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_S3_T @ Navi2x
     451, // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_S3_T @ Navi2x
     452, // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_S3_T @ Navi2x
     453, // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_S3_T @ Navi2x
     454, // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_S3_T @ Navi2x
     455, // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_S3_T @ Navi2x
     456, // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_S3_T @ Navi2x
     457, // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_S3_T @ Navi2x
     458, // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_S3_T @ Navi2x
     479, // 8 pipes (2 PKRs) 1 bpe @ SW_64K_S3_T @ Navi2x
     480, // 8 pipes (2 PKRs) 2 bpe @ SW_64K_S3_T @ Navi2x
     481, // 8 pipes (2 PKRs) 4 bpe @ SW_64K_S3_T @ Navi2x
     482, // 8 pipes (2 PKRs) 8 bpe @ SW_64K_S3_T @ Navi2x
     483, // 8 pipes (2 PKRs) 16 bpe @ SW_64K_S3_T @ Navi2x
     454, // 4 pipes (4 PKRs) 1 bpe @ SW_64K_S3_T @ Navi2x
     455, // 4 pipes (4 PKRs) 2 bpe @ SW_64K_S3_T @ Navi2x
     456, // 4 pipes (4 PKRs) 4 bpe @ SW_64K_S3_T @ Navi2x
     457, // 4 pipes (4 PKRs) 8 bpe @ SW_64K_S3_T @ Navi2x
     458, // 4 pipes (4 PKRs) 16 bpe @ SW_64K_S3_T @ Navi2x
     479, // 8 pipes (4 PKRs) 1 bpe @ SW_64K_S3_T @ Navi2x
     480, // 8 pipes (4 PKRs) 2 bpe @ SW_64K_S3_T @ Navi2x
     481, // 8 pipes (4 PKRs) 4 bpe @ SW_64K_S3_T @ Navi2x
     482, // 8 pipes (4 PKRs) 8 bpe @ SW_64K_S3_T @ Navi2x
     483, // 8 pipes (4 PKRs) 16 bpe @ SW_64K_S3_T @ Navi2x
     484, // 16 pipes (4 PKRs) 1 bpe @ SW_64K_S3_T @ Navi2x
     485, // 16 pipes (4 PKRs) 2 bpe @ SW_64K_S3_T @ Navi2x
     486, // 16 pipes (4 PKRs) 4 bpe @ SW_64K_S3_T @ Navi2x
     487, // 16 pipes (4 PKRs) 8 bpe @ SW_64K_S3_T @ Navi2x
     488, // 16 pipes (4 PKRs) 16 bpe @ SW_64K_S3_T @ Navi2x
     479, // 8 pipes (8 PKRs) 1 bpe @ SW_64K_S3_T @ Navi2x
     480, // 8 pipes (8 PKRs) 2 bpe @ SW_64K_S3_T @ Navi2x
     481, // 8 pipes (8 PKRs) 4 bpe @ SW_64K_S3_T @ Navi2x
     482, // 8 pipes (8 PKRs) 8 bpe @ SW_64K_S3_T @ Navi2x
     483, // 8 pipes (8 PKRs) 16 bpe @ SW_64K_S3_T @ Navi2x
     484, // 16 pipes (8 PKRs) 1 bpe @ SW_64K_S3_T @ Navi2x
     485, // 16 pipes (8 PKRs) 2 bpe @ SW_64K_S3_T @ Navi2x
     486, // 16 pipes (8 PKRs) 4 bpe @ SW_64K_S3_T @ Navi2x
     487, // 16 pipes (8 PKRs) 8 bpe @ SW_64K_S3_T @ Navi2x
     488, // 16 pipes (8 PKRs) 16 bpe @ SW_64K_S3_T @ Navi2x
     489, // 32 pipes (8 PKRs) 1 bpe @ SW_64K_S3_T @ Navi2x
     490, // 32 pipes (8 PKRs) 2 bpe @ SW_64K_S3_T @ Navi2x
     491, // 32 pipes (8 PKRs) 4 bpe @ SW_64K_S3_T @ Navi2x
     492, // 32 pipes (8 PKRs) 8 bpe @ SW_64K_S3_T @ Navi2x
     493, // 32 pipes (8 PKRs) 16 bpe @ SW_64K_S3_T @ Navi2x
     484, // 16 pipes (16 PKRs) 1 bpe @ SW_64K_S3_T @ Navi2x
     485, // 16 pipes (16 PKRs) 2 bpe @ SW_64K_S3_T @ Navi2x
     486, // 16 pipes (16 PKRs) 4 bpe @ SW_64K_S3_T @ Navi2x
     487, // 16 pipes (16 PKRs) 8 bpe @ SW_64K_S3_T @ Navi2x
     488, // 16 pipes (16 PKRs) 16 bpe @ SW_64K_S3_T @ Navi2x
     489, // 32 pipes (16 PKRs) 1 bpe @ SW_64K_S3_T @ Navi2x
     490, // 32 pipes (16 PKRs) 2 bpe @ SW_64K_S3_T @ Navi2x
     491, // 32 pipes (16 PKRs) 4 bpe @ SW_64K_S3_T @ Navi2x
     492, // 32 pipes (16 PKRs) 8 bpe @ SW_64K_S3_T @ Navi2x
     493, // 32 pipes (16 PKRs) 16 bpe @ SW_64K_S3_T @ Navi2x
     494, // 64 pipes (16 PKRs) 1 bpe @ SW_64K_S3_T @ Navi2x
     495, // 64 pipes (16 PKRs) 2 bpe @ SW_64K_S3_T @ Navi2x
     496, // 64 pipes (16 PKRs) 4 bpe @ SW_64K_S3_T @ Navi2x
     497, // 64 pipes (16 PKRs) 8 bpe @ SW_64K_S3_T @ Navi2x
     498, // 64 pipes (16 PKRs) 16 bpe @ SW_64K_S3_T @ Navi2x
     489, // 32 pipes (32 PKRs) 1 bpe @ SW_64K_S3_T @ Navi2x
     490, // 32 pipes (32 PKRs) 2 bpe @ SW_64K_S3_T @ Navi2x
     491, // 32 pipes (32 PKRs) 4 bpe @ SW_64K_S3_T @ Navi2x
     492, // 32 pipes (32 PKRs) 8 bpe @ SW_64K_S3_T @ Navi2x
     493, // 32 pipes (32 PKRs) 16 bpe @ SW_64K_S3_T @ Navi2x
     494, // 64 pipes (32 PKRs) 1 bpe @ SW_64K_S3_T @ Navi2x
     495, // 64 pipes (32 PKRs) 2 bpe @ SW_64K_S3_T @ Navi2x
     496, // 64 pipes (32 PKRs) 4 bpe @ SW_64K_S3_T @ Navi2x
     497, // 64 pipes (32 PKRs) 8 bpe @ SW_64K_S3_T @ Navi2x
     498, // 64 pipes (32 PKRs) 16 bpe @ SW_64K_S3_T @ Navi2x
};

const UINT_16 SW_64K_D3_X_RBPLUS_PATIDX[] =
{
     499, // 1 pipes (1 PKRs) 1 bpe @ SW_64K_D3_X @ Navi2x
     500, // 1 pipes (1 PKRs) 2 bpe @ SW_64K_D3_X @ Navi2x
     501, // 1 pipes (1 PKRs) 4 bpe @ SW_64K_D3_X @ Navi2x
     502, // 1 pipes (1 PKRs) 8 bpe @ SW_64K_D3_X @ Navi2x
     503, // 1 pipes (1 PKRs) 16 bpe @ SW_64K_D3_X @ Navi2x
    1294, // 2 pipes (1-2 PKRs) 1 bpe @ SW_64K_D3_X @ Navi2x
    1295, // 2 pipes (1-2 PKRs) 2 bpe @ SW_64K_D3_X @ Navi2x
    1296, // 2 pipes (1-2 PKRs) 4 bpe @ SW_64K_D3_X @ Navi2x
    1297, // 2 pipes (1-2 PKRs) 8 bpe @ SW_64K_D3_X @ Navi2x
    1298, // 2 pipes (1-2 PKRs) 16 bpe @ SW_64K_D3_X @ Navi2x
    1299, // 4 pipes (1-2 PKRs) 1 bpe @ SW_64K_D3_X @ Navi2x
    1300, // 4 pipes (1-2 PKRs) 2 bpe @ SW_64K_D3_X @ Navi2x
    1301, // 4 pipes (1-2 PKRs) 4 bpe @ SW_64K_D3_X @ Navi2x
    1302, // 4 pipes (1-2 PKRs) 8 bpe @ SW_64K_D3_X @ Navi2x
    1303, // 4 pipes (1-2 PKRs) 16 bpe @ SW_64K_D3_X @ Navi2x
    1304, // 8 pipes (2 PKRs) 1 bpe @ SW_64K_D3_X @ Navi2x
    1305, // 8 pipes (2 PKRs) 2 bpe @ SW_64K_D3_X @ Navi2x
    1306, // 8 pipes (2 PKRs) 4 bpe @ SW_64K_D3_X @ Navi2x
    1307, // 8 pipes (2 PKRs) 8 bpe @ SW_64K_D3_X @ Navi2x
    1308, // 8 pipes (2 PKRs) 16 bpe @ SW_64K_D3_X @ Navi2x
    1309, // 4 pipes (4 PKRs) 1 bpe @ SW_64K_D3_X @ Navi2x
    1310, // 4 pipes (4 PKRs) 2 bpe @ SW_64K_D3_X @ Navi2x
    1311, // 4 pipes (4 PKRs) 4 bpe @ SW_64K_D3_X @ Navi2x
    1312, // 4 pipes (4 PKRs) 8 bpe @ SW_64K_D3_X @ Navi2x
    1313, // 4 pipes (4 PKRs) 16 bpe @ SW_64K_D3_X @ Navi2x
    1314, // 8 pipes (4 PKRs) 1 bpe @ SW_64K_D3_X @ Navi2x
    1315, // 8 pipes (4 PKRs) 2 bpe @ SW_64K_D3_X @ Navi2x
    1316, // 8 pipes (4 PKRs) 4 bpe @ SW_64K_D3_X @ Navi2x
    1317, // 8 pipes (4 PKRs) 8 bpe @ SW_64K_D3_X @ Navi2x
    1318, // 8 pipes (4 PKRs) 16 bpe @ SW_64K_D3_X @ Navi2x
    1319, // 16 pipes (4 PKRs) 1 bpe @ SW_64K_D3_X @ Navi2x
    1320, // 16 pipes (4 PKRs) 2 bpe @ SW_64K_D3_X @ Navi2x
    1321, // 16 pipes (4 PKRs) 4 bpe @ SW_64K_D3_X @ Navi2x
    1322, // 16 pipes (4 PKRs) 8 bpe @ SW_64K_D3_X @ Navi2x
    1323, // 16 pipes (4 PKRs) 16 bpe @ SW_64K_D3_X @ Navi2x
    1324, // 8 pipes (8 PKRs) 1 bpe @ SW_64K_D3_X @ Navi2x
    1325, // 8 pipes (8 PKRs) 2 bpe @ SW_64K_D3_X @ Navi2x
    1326, // 8 pipes (8 PKRs) 4 bpe @ SW_64K_D3_X @ Navi2x
    1327, // 8 pipes (8 PKRs) 8 bpe @ SW_64K_D3_X @ Navi2x
    1328, // 8 pipes (8 PKRs) 16 bpe @ SW_64K_D3_X @ Navi2x
    1329, // 16 pipes (8 PKRs) 1 bpe @ SW_64K_D3_X @ Navi2x
    1330, // 16 pipes (8 PKRs) 2 bpe @ SW_64K_D3_X @ Navi2x
    1331, // 16 pipes (8 PKRs) 4 bpe @ SW_64K_D3_X @ Navi2x
    1332, // 16 pipes (8 PKRs) 8 bpe @ SW_64K_D3_X @ Navi2x
    1333, // 16 pipes (8 PKRs) 16 bpe @ SW_64K_D3_X @ Navi2x
    1334, // 32 pipes (8 PKRs) 1 bpe @ SW_64K_D3_X @ Navi2x
    1335, // 32 pipes (8 PKRs) 2 bpe @ SW_64K_D3_X @ Navi2x
    1336, // 32 pipes (8 PKRs) 4 bpe @ SW_64K_D3_X @ Navi2x
    1337, // 32 pipes (8 PKRs) 8 bpe @ SW_64K_D3_X @ Navi2x
    1338, // 32 pipes (8 PKRs) 16 bpe @ SW_64K_D3_X @ Navi2x
    1339, // 16 pipes (16 PKRs) 1 bpe @ SW_64K_D3_X @ Navi2x
    1340, // 16 pipes (16 PKRs) 2 bpe @ SW_64K_D3_X @ Navi2x
    1341, // 16 pipes (16 PKRs) 4 bpe @ SW_64K_D3_X @ Navi2x
    1342, // 16 pipes (16 PKRs) 8 bpe @ SW_64K_D3_X @ Navi2x
    1343, // 16 pipes (16 PKRs) 16 bpe @ SW_64K_D3_X @ Navi2x
    1344, // 32 pipes (16 PKRs) 1 bpe @ SW_64K_D3_X @ Navi2x
    1345, // 32 pipes (16 PKRs) 2 bpe @ SW_64K_D3_X @ Navi2x
    1346, // 32 pipes (16 PKRs) 4 bpe @ SW_64K_D3_X @ Navi2x
    1347, // 32 pipes (16 PKRs) 8 bpe @ SW_64K_D3_X @ Navi2x
    1348, // 32 pipes (16 PKRs) 16 bpe @ SW_64K_D3_X @ Navi2x
    1349, // 64 pipes (16 PKRs) 1 bpe @ SW_64K_D3_X @ Navi2x
    1350, // 64 pipes (16 PKRs) 2 bpe @ SW_64K_D3_X @ Navi2x
    1351, // 64 pipes (16 PKRs) 4 bpe @ SW_64K_D3_X @ Navi2x
    1352, // 64 pipes (16 PKRs) 8 bpe @ SW_64K_D3_X @ Navi2x
    1353, // 64 pipes (16 PKRs) 16 bpe @ SW_64K_D3_X @ Navi2x
    1354, // 32 pipes (32 PKRs) 1 bpe @ SW_64K_D3_X @ Navi2x
    1355, // 32 pipes (32 PKRs) 2 bpe @ SW_64K_D3_X @ Navi2x
    1356, // 32 pipes (32 PKRs) 4 bpe @ SW_64K_D3_X @ Navi2x
    1357, // 32 pipes (32 PKRs) 8 bpe @ SW_64K_D3_X @ Navi2x
    1358, // 32 pipes (32 PKRs) 16 bpe @ SW_64K_D3_X @ Navi2x
    1359, // 64 pipes (32 PKRs) 1 bpe @ SW_64K_D3_X @ Navi2x
    1360, // 64 pipes (32 PKRs) 2 bpe @ SW_64K_D3_X @ Navi2x
    1361, // 64 pipes (32 PKRs) 4 bpe @ SW_64K_D3_X @ Navi2x
    1362, // 64 pipes (32 PKRs) 8 bpe @ SW_64K_D3_X @ Navi2x
    1363, // 64 pipes (32 PKRs) 16 bpe @ SW_64K_D3_X @ Navi2x
};

const UINT_64 GFX10_SW_PATTERN[][16] =
{
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            0,             0,             0,             0,             0,             0,             0,             0,             },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            0,             0,             0,             0,             0,             0,             0,             0,             },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            0,             0,             0,             0,             0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            0,             0,             0,             0,             0,             0,             0,             0,             },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            0,             0,             0,             0,             0,             0,             0,             0,             },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4,            X4,            Y5,            X5,            0,             0,             0,             0,             },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3,            X4,            Y4,            X5,            0,             0,             0,             0,             },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3,            X3,            Y4,            X4,            0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2,            X3,            Y3,            X4,            0,             0,             0,             0,             },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2,            X2,            Y3,            X3,            0,             0,             0,             0,             },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Z0^X4^Y4,      X4,            Y5,            X5,            0,             0,             0,             0,             },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Z0^Y3^X4,      X4,            Y4,            X5,            0,             0,             0,             0,             },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Z0^X3^Y3,      X3,            Y4,            X4,            0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Z0^Y2^X3,      X3,            Y3,            X4,            0,             0,             0,             0,             },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Z0^X2^Y2,      X2,            Y3,            X3,            0,             0,             0,             0,             },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Z1^Y4^X5,      Z0^X4^Y5,      Y5,            X5,            0,             0,             0,             0,             },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Z1^Y3^X5,      Z0^X4^Y4,      Y4,            X5,            0,             0,             0,             0,             },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Z1^Y3^X4,      Z0^X3^Y4,      Y4,            X4,            0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Z1^Y2^X4,      Z0^X3^Y3,      Y3,            X4,            0,             0,             0,             0,             },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Z1^Y2^X3,      Z0^X2^Y3,      Y3,            X3,            0,             0,             0,             0,             },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Z2^Y4^X6,      Z1^X4^Y6,      Z0^X5^Y5,      X5,            0,             0,             0,             0,             },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Z2^Y3^X6,      Z1^X4^Y5,      Z0^Y4^X5,      X5,            0,             0,             0,             0,             },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Z2^Y3^X5,      Z1^X3^Y5,      Z0^X4^Y4,      X4,            0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2^Z2^X5,      Z1^X3^Y4,      Z0^Y3^X4,      X4,            0,             0,             0,             0,             },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2^Z2^X4,      Z1^X2^Y4,      Z0^X3^Y3,      X3,            0,             0,             0,             0,             },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Z3^Y4^X7,      Z2^X4^Y7,      Z1^Y5^X6,      Z0^X5^Y6,      0,             0,             0,             0,             },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3^Z3^X7,      Z2^X4^Y6,      Z1^Y4^X6,      Z0^X5^Y5,      0,             0,             0,             0,             },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3^Z3^X6,      Z2^X3^Y6,      Z1^Y4^X5,      Z0^X4^Y5,      0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2^Z3^X6,      Z2^X3^Y5,      Z1^Y3^X5,      Z0^X4^Y4,      0,             0,             0,             0,             },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2^Z3^X5,      X2^Z2^Y5,      Z1^Y3^X4,      Z0^X3^Y4,      0,             0,             0,             0,             },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4,            X4,            Y5,            X5,            Y6,            X6,            Y7,            X7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3,            X4,            Y4,            X5,            Y5,            X6,            Y6,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3,            X3,            Y4,            X4,            Y5,            X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2,            X3,            Y3,            X4,            Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2,            X2,            Y3,            X3,            Y4,            X4,            Y5,            X5,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Z0^X4^Y4,      X4,            Y5,            X5,            Y6,            X6,            Y7,            X7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Z0^Y3^X4,      X4,            Y4,            X5,            Y5,            X6,            Y6,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Z0^X3^Y3,      X3,            Y4,            X4,            Y5,            X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Z0^Y2^X3,      X3,            Y3,            X4,            Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Z0^X2^Y2,      X2,            Y3,            X3,            Y4,            X4,            Y5,            X5,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Z1^Y4^X5,      Z0^X4^Y5,      Y5,            X5,            Y6,            X6,            Y7,            X7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Z1^Y3^X5,      Z0^X4^Y4,      Y4,            X5,            Y5,            X6,            Y6,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Z1^Y3^X4,      Z0^X3^Y4,      Y4,            X4,            Y5,            X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Z1^Y2^X4,      Z0^X3^Y3,      Y3,            X4,            Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Z1^Y2^X3,      Z0^X2^Y3,      Y3,            X3,            Y4,            X4,            Y5,            X5,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Z2^Y4^X6,      Z1^X4^Y6,      Z0^X5^Y5,      X5,            Y6,            X6,            Y7,            X7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Z2^Y3^X6,      Z1^X4^Y5,      Z0^Y4^X5,      X5,            Y5,            X6,            Y6,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Z2^Y3^X5,      Z1^X3^Y5,      Z0^X4^Y4,      X4,            Y5,            X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2^Z2^X5,      Z1^X3^Y4,      Z0^Y3^X4,      X4,            Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2^Z2^X4,      Z1^X2^Y4,      Z0^X3^Y3,      X3,            Y4,            X4,            Y5,            X5,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Z3^Y4^X7,      Z2^X4^Y7,      Z1^Y5^X6,      Z0^X5^Y6,      Y6,            X6,            Y7,            X7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3^Z3^X7,      Z2^X4^Y6,      Z1^Y4^X6,      Z0^X5^Y5,      Y5,            X6,            Y6,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3^Z3^X6,      Z2^X3^Y6,      Z1^Y4^X5,      Z0^X4^Y5,      Y5,            X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2^Z3^X6,      Z2^X3^Y5,      Z1^Y3^X5,      Z0^X4^Y4,      Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2^Z3^X5,      X2^Z2^Y5,      Z1^Y3^X4,      Z0^X3^Y4,      Y4,            X4,            Y5,            X5,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^Z4^X8,      Z3^X4^Y8,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X6,            Y7,            X7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3^Z4^X8,      Z3^X4^Y7,      Z2^Y4^X7,      Z1^X5^Y6,      Z0^Y5^X6,      X6,            Y6,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3^Z4^X7,      X3^Z3^Y7,      Z2^Y4^X6,      Z1^X4^Y6,      Z0^X5^Y5,      X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2^Z4^X7,      X3^Z3^Y6,      Z2^Y3^X6,      Z1^X4^Y5,      Z0^Y4^X5,      X5,            Y5,            X6,            },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2^Z4^X6,      X2^Z3^Y6,      Z2^Y3^X5,      Z1^X3^Y5,      Z0^X4^Y4,      X4,            Y5,            X5,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^Z5^X9,      X4^Z4^Y9,      Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      Z0^X6^Y7,      Y7,            X7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3^Z5^X9,      X4^Z4^Y8,      Z3^Y4^X8,      Z2^X5^Y7,      Z1^Y5^X7,      Z0^X6^Y6,      Y6,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3^Z5^X8,      X3^Z4^Y8,      Z3^Y4^X7,      Z2^X4^Y7,      Z1^Y5^X6,      Z0^X5^Y6,      Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2^Z5^X8,      X3^Z4^Y7,      Y3^Z3^X7,      Z2^X4^Y6,      Z1^Y4^X6,      Z0^X5^Y5,      Y5,            X6,            },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2^Z5^X7,      X2^Z4^Y7,      Y3^Z3^X6,      Z2^X3^Y6,      Z1^Y4^X5,      Z0^X4^Y5,      Y5,            X5,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            X4^Y4,         X4,            Y5,            X5,            Y6,            X6,            Y7,            X7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3^X4,         X4,            Y4,            X5,            Y5,            X6,            Y6,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            X3^Y3,         X3,            Y4,            X4,            Y5,            X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2^X3,         X3,            Y3,            X4,            Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            X2^Y2,         X2,            Y3,            X3,            Y4,            X4,            Y5,            X5,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X5,         X4^Y5,         Y5,            X5,            Y6,            X6,            Y7,            X7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3^X5,         X4^Y4,         Y4,            X5,            Y5,            X6,            Y6,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3^X4,         X3^Y4,         Y4,            X4,            Y5,            X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2^X4,         X3^Y3,         Y3,            X4,            Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2^X3,         X2^Y3,         Y3,            X3,            Y4,            X4,            Y5,            X5,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X6,         X4^Y6,         X5^Y5,         X5,            Y6,            X6,            Y7,            X7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3^X6,         X4^Y5,         Y4^X5,         X5,            Y5,            X6,            Y6,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3^X5,         X3^Y5,         X4^Y4,         X4,            Y5,            X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2^X5,         X3^Y4,         Y3^X4,         X4,            Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2^X4,         X2^Y4,         X3^Y3,         X3,            Y4,            X4,            Y5,            X5,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X7,         X4^Y7,         Y5^X6,         X5^Y6,         Y6,            X6,            Y7,            X7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3^X7,         X4^Y6,         Y4^X6,         X5^Y5,         Y5,            X6,            Y6,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3^X6,         X3^Y6,         Y4^X5,         X4^Y5,         Y5,            X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2^X6,         X3^Y5,         Y3^X5,         X4^Y4,         Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2^X5,         X2^Y5,         Y3^X4,         X3^Y4,         Y4,            X4,            Y5,            X5,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4,            X4,            Y5^X7,         X5^Y7,         X6^Y6,         X6,            Y7,            X7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3,            X4,            Y4^X7,         X5^Y6,         Y5^X6,         X6,            Y6,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3,            X3,            Y4^X6,         X4^Y6,         X5^Y5,         X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2,            X3,            Y3^X6,         X4^Y5,         Y4^X5,         X5,            Y5,            X6,            },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2,            X2,            Y3^X5,         X3^Y5,         X4^Y4,         X4,            Y5,            X5,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4,            X4,            Y5,            X5,            Y6^X7,         X6^Y7,         Y7,            X7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3,            X4,            Y4,            X5,            Y5^X7,         X6^Y6,         Y6,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3,            X3,            Y4,            X4,            Y5^X6,         X5^Y6,         Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2,            X3,            Y3,            X4,            Y4^X6,         X5^Y5,         Y5,            X6,            },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2,            X2,            Y3,            X3,            Y4^X5,         X4^Y5,         Y5,            X5,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            0,             0,             0,             0,             0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            0,             0,             0,             0,             0,             0,             0,             0,             },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            0,             0,             0,             0,             0,             0,             0,             0,             },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4,            X4,            Y5,            X5,            0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2,            X3,            Y3,            X4,            0,             0,             0,             0,             },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2,            X2,            Y3,            X3,            0,             0,             0,             0,             },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Z0^X4^Y4,      X4,            Y5,            X5,            0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Z0^Y2^X3,      X3,            Y3,            X4,            0,             0,             0,             0,             },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z0^X2^Y2,      X2,            Y3,            X3,            0,             0,             0,             0,             },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Z1^Y4^X5,      Z0^X4^Y5,      Y5,            X5,            0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Z1^Y2^X4,      Z0^X3^Y3,      Y3,            X4,            0,             0,             0,             0,             },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z1^Y2^X3,      Z0^X2^Y3,      Y3,            X3,            0,             0,             0,             0,             },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Z2^Y4^X6,      Z1^X4^Y6,      Z0^X5^Y5,      X5,            0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2^Z2^X5,      Z1^X3^Y4,      Z0^Y3^X4,      X4,            0,             0,             0,             0,             },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2^Z2^X4,      Z1^X2^Y4,      Z0^X3^Y3,      X3,            0,             0,             0,             0,             },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Z3^Y4^X7,      Z2^X4^Y7,      Z1^Y5^X6,      Z0^X5^Y6,      0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2^Z3^X6,      Z2^X3^Y5,      Z1^Y3^X5,      Z0^X4^Y4,      0,             0,             0,             0,             },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2^Z3^X5,      X2^Z2^Y5,      Z1^Y3^X4,      Z0^X3^Y4,      0,             0,             0,             0,             },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4,            X4,            Y5,            X5,            Y6,            X6,            Y7,            X7,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2,            X3,            Y3,            X4,            Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2,            X2,            Y3,            X3,            Y4,            X4,            Y5,            X5,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Z0^X4^Y4,      X4,            Y5,            X5,            Y6,            X6,            Y7,            X7,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Z0^Y2^X3,      X3,            Y3,            X4,            Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z0^X2^Y2,      X2,            Y3,            X3,            Y4,            X4,            Y5,            X5,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Z1^Y4^X5,      Z0^X4^Y5,      Y5,            X5,            Y6,            X6,            Y7,            X7,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Z1^Y2^X4,      Z0^X3^Y3,      Y3,            X4,            Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z1^Y2^X3,      Z0^X2^Y3,      Y3,            X3,            Y4,            X4,            Y5,            X5,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Z2^Y4^X6,      Z1^X4^Y6,      Z0^X5^Y5,      X5,            Y6,            X6,            Y7,            X7,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2^Z2^X5,      Z1^X3^Y4,      Z0^Y3^X4,      X4,            Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2^Z2^X4,      Z1^X2^Y4,      Z0^X3^Y3,      X3,            Y4,            X4,            Y5,            X5,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Z3^Y4^X7,      Z2^X4^Y7,      Z1^Y5^X6,      Z0^X5^Y6,      Y6,            X6,            Y7,            X7,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2^Z3^X6,      Z2^X3^Y5,      Z1^Y3^X5,      Z0^X4^Y4,      Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2^Z3^X5,      X2^Z2^Y5,      Z1^Y3^X4,      Z0^X3^Y4,      Y4,            X4,            Y5,            X5,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4^Z4^X8,      Z3^X4^Y8,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X6,            Y7,            X7,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2^Z4^X7,      X3^Z3^Y6,      Z2^Y3^X6,      Z1^X4^Y5,      Z0^Y4^X5,      X5,            Y5,            X6,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2^Z4^X6,      X2^Z3^Y6,      Z2^Y3^X5,      Z1^X3^Y5,      Z0^X4^Y4,      X4,            Y5,            X5,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4^Z5^X9,      X4^Z4^Y9,      Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      Z0^X6^Y7,      Y7,            X7,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2^Z5^X8,      X3^Z4^Y7,      Y3^Z3^X7,      Z2^X4^Y6,      Z1^Y4^X6,      Z0^X5^Y5,      Y5,            X6,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2^Z5^X7,      X2^Z4^Y7,      Y3^Z3^X6,      Z2^X3^Y6,      Z1^Y4^X5,      Z0^X4^Y5,      Y5,            X5,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            X4^Y4,         X4,            Y5,            X5,            Y6,            X6,            Y7,            X7,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2^X3,         X3,            Y3,            X4,            Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            X2^Y2,         X2,            Y3,            X3,            Y4,            X4,            Y5,            X5,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4^X5,         X4^Y5,         Y5,            X5,            Y6,            X6,            Y7,            X7,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2^X4,         X3^Y3,         Y3,            X4,            Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2^X3,         X2^Y3,         Y3,            X3,            Y4,            X4,            Y5,            X5,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4^X6,         X4^Y6,         X5^Y5,         X5,            Y6,            X6,            Y7,            X7,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2^X5,         X3^Y4,         Y3^X4,         X4,            Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2^X4,         X2^Y4,         X3^Y3,         X3,            Y4,            X4,            Y5,            X5,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4^X7,         X4^Y7,         Y5^X6,         X5^Y6,         Y6,            X6,            Y7,            X7,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2^X6,         X3^Y5,         Y3^X5,         X4^Y4,         Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2^X5,         X2^Y5,         Y3^X4,         X3^Y4,         Y4,            X4,            Y5,            X5,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4,            X4,            Y5^X7,         X5^Y7,         X6^Y6,         X6,            Y7,            X7,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2,            X3,            Y3^X6,         X4^Y5,         Y4^X5,         X5,            Y5,            X6,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2,            X2,            Y3^X5,         X3^Y5,         X4^Y4,         X4,            Y5,            X5,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4,            X4,            Y5,            X5,            Y6^X7,         X6^Y7,         Y7,            X7,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2,            X3,            Y3,            X4,            Y4^X6,         X5^Y5,         Y5,            X6,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2,            X2,            Y3,            X3,            Y4^X5,         X4^Y5,         Y5,            X5,            },
    {X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y3,            Y4,            X4,            Y5,            X5,            Y6,            X6,            Y7,            X7,            },
    {0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y3,            X4,            Y4,            X5,            Y5,            X6,            Y6,            X7,            },
    {0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            Y3,            X3,            Y4,            X4,            Y5,            X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y3,            X4,            Y4,            X5,            Y5,            X6,            },
    {X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4,            Z0^X3^Y3,      X4,            Y5,            X5,            Y6,            X6,            Y7,            X7,            },
    {0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Z0^X3^Y3,      X4,            Y4,            X5,            Y5,            X6,            Y6,            X7,            },
    {0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            Z0^X3^Y3,      X3,            Y4,            X4,            Y5,            X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            X1,            Y1,            X2,            Z0^X3^Y3,      X3,            Y2,            X4,            Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z0^X3^Y3,      X2,            Y2,            X3,            Y4,            X4,            Y5,            X5,            },
    {X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4,            Z1^X3^Y3,      Z0^X4^Y4,      Y5,            X5,            Y6,            X6,            Y7,            X7,            },
    {0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Z1^X3^Y3,      Z0^X4^Y4,      Y4,            X5,            Y5,            X6,            Y6,            X7,            },
    {0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            Z1^X3^Y3,      Z0^X4^Y4,      Y3,            X4,            Y5,            X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            X1,            Y1,            X2,            Z1^X3^Y3,      Z0^X4^Y4,      Y2,            X3,            Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z1^X3^Y3,      Z0^X4^Y4,      Y2,            X2,            Y3,            X4,            Y5,            X5,            },
    {X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X5,            Y6,            X6,            Y7,            X7,            },
    {0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X4,            Y5,            X6,            Y6,            X7,            },
    {0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            Y4,            X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            X1,            Y1,            X2,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            Y2,            X4,            Y5,            X6,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X2,            Y2,            X3,            Y4,            X5,            },
    {X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      Y6,            X6,            Y7,            X7,            },
    {0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      Y4,            X6,            Y6,            X7,            },
    {0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      Y3,            X4,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            X1,            Y1,            X2,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      Y2,            X3,            Y4,            X6,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      Y2,            X2,            Y3,            X4,            },
    {X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X6,            Y7,            X7,            },
    {0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X4,            Y6,            X7,            },
    {0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X3,            Y4,            X6,            },
    {0,             0,             0,             X0,            Y0,            X1,            Y1,            X2,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      Y2,            X3,            Y4,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      Y2^X6^Y6,      X2,            Y3,            X4,            },
    {X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4,            X3^Y3^Z5,      X4^Y4^Z4,      Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      Z0^X6^Y7,      Y7,            X7,            },
    {0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            X3^Y3^Z5,      X4^Y4^Z4,      Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      Z0^X6^Y7,      Y4,            X7,            },
    {0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3^Y3^Z5,      X4^Y4^Z4,      Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      Z0^X6^Y7,      Y3,            X4,            },
    {0,             0,             0,             X0,            Y0,            X1,            Y1,            X2,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X8,      Z1^X5^Y8,      Y2^Y6^X7,      Z0^X6^Y7,      Y3,            X4,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X8,      Z0^X5^Y8,      Y2^Y6^X7,      X2^X6^Y7,      Y3,            X4,            },
    {S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y3,            Y4,            X4,            Y5,            X5,            Y6,            X6,            Y7,            },
    {0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Y3,            X3,            Y4,            X4,            Y5,            X5,            Y6,            X6,            },
    {0,             0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y3,            X3,            Y4,            X4,            Y5,            X5,            Y6,            Y2^Y7,         },
    {0,             0,             0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y3,            X3,            Y4,            X4,            Y5,            X5,            Y2^Y6,         },
    {0,             0,             0,             0,             S0,            X0,            Y0,            X1,            Y2,            X2,            Y3,            X3,            Y4,            X4,            Y5,            Y1^Y6,         },
    {S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Z0^X3^Y3,      Y4,            X4,            Y5,            X5,            Y6,            X6,            Y7,            },
    {0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Z0^X3^Y3,      X3,            Y4,            X4,            Y5,            X5,            Y6,            X6,            },
    {0,             0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Z0^X3^Y3,      X3,            Y4,            X4,            Y5,            X5,            Y6,            Y2^Y7,         },
    {0,             0,             0,             S0,            X0,            Y0,            X1,            Y1,            Z0^X3^Y3,      X2,            X3,            Y4,            X4,            Y5,            X5,            Y2^Y6,         },
    {0,             0,             0,             0,             S0,            X0,            Y0,            X1,            Z0^X3^Y3,      X2,            Y2,            X3,            Y4,            X4,            Y5,            Y1^Y6,         },
    {S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Z1^X3^Y3,      Z0^X4^Y4,      Y4,            Y5,            X5,            Y6,            X6,            Y7,            },
    {0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Z1^X3^Y3,      Z0^X4^Y4,      Y3,            X4,            Y5,            X5,            Y6,            X6,            },
    {0,             0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Z1^X3^Y3,      Z0^X4^Y4,      Y3,            X4,            Y5,            X5,            Y6,            Y2^Y7,         },
    {0,             0,             0,             S0,            X0,            Y0,            X1,            Y1,            Z1^X3^Y3,      Z0^X4^Y4,      X2,            Y3,            X4,            Y5,            X5,            Y2^Y6,         },
    {0,             0,             0,             0,             S0,            X0,            Y0,            X1,            Z1^X3^Y3,      Z0^X4^Y4,      Y2,            X2,            Y3,            X4,            Y5,            Y1^Y6,         },
    {S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      Y4,            X5,            Y6,            X6,            Y7,            },
    {0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            Y4,            X5,            Y6,            X6,            },
    {0,             0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            Y4,            X5,            Y6,            Y2^Y7,         },
    {0,             0,             0,             S0,            X0,            Y0,            X1,            Y1,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X2,            X3,            Y4,            X5,            Y2^Y6,         },
    {0,             0,             0,             0,             S0,            X0,            Y0,            X1,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X2,            Y2,            X3,            Y4,            Y1^Y6,         },
    {S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      Y4,            Y6,            X6,            Y7,            },
    {0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      Y3,            X4,            Y6,            X6,            },
    {0,             0,             S0,            X0,            Y0,            X1,            Y1,            X2,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      Y3,            X4,            Y6,            Y2^Y7,         },
    {0,             0,             0,             S0,            X0,            Y0,            X1,            Y1,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X2,            Y3,            X4,            Y2^Y6,         },
    {0,             0,             0,             0,             S0,            X0,            Y0,            X1,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X6,      Y2^X5^Y6,      Y1,            X3,            Y4,            X2^Y6,         },
    {S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      Y4,            X6,            Y7,            },
    {0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X3,            Y4,            X6,            },
    {0,             0,             S0,            X0,            Y0,            X1,            Y1,            X2,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X3,            Y4,            Y2^Y7,         },
    {0,             0,             0,             S0,            X0,            Y0,            X1,            Y1,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      Y2^X6^Y6,      Y3,            X4,            X2^Y7,         },
    {0,             0,             0,             0,             S0,            X0,            Y0,            X1,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X7,      Y2^X5^Y7,      X2^X6^Y6,      X3,            Y4,            Y1^Y7,         },
    {S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X8,      Z1^X5^Y8,      Z0^Y6^X7,      Z5^X6^Y7,      Y4,            Y7,            },
    {0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X8,      Z1^X5^Y8,      Z0^Y6^X7,      Z5^X6^Y7,      Y3,            X4,            },
    {0,             0,             S0,            X0,            Y0,            X1,            Y1,            X2,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X8,      Z1^X5^Y8,      Z0^Y6^X7,      Y2^X6^Y7,      X3,            Y4,            },
    {0,             0,             0,             S0,            X0,            Y0,            X1,            Y1,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X8,      Z0^X5^Y8,      Y2^Y6^X7,      X2^X6^Y7,      Y3,            X4,            },
    {0,             0,             0,             0,             S0,            X0,            Y0,            X1,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X8,      Y2^X5^Y8,      X2^Y6^X7,      Y1^X6^Y7,      X3,            Y4,            },
    {S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Y3,            X3,            Y4,            X4,            Y5,            X5,            Y6,            X6,            },
    {0,             S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            X3,            Y3,            X4,            Y4,            X5,            Y5,            X6,            Y2^Y6,         },
    {0,             0,             S0,            S1,            X0,            Y0,            X1,            Y1,            Y3,            X3,            Y4,            X4,            Y5,            X5,            Y2^Y6,         X2^Y7,         },
    {0,             0,             0,             S0,            S1,            X0,            Y0,            X1,            Y2,            X3,            Y3,            X4,            Y4,            X5,            Y1^Y5,         X2^Y6,         },
    {0,             0,             0,             0,             S0,            S1,            X0,            Y0,            Y2,            X2,            Y3,            X3,            Y4,            X4,            Y1^Y5,         X1^Y6,         },
    {S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Z0^X3^Y3,      X3,            Y4,            X4,            Y5,            X5,            Y6,            X6,            },
    {0,             S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Z0^X3^Y3,      X3,            X4,            Y4,            X5,            Y5,            X6,            Y2^Y6,         },
    {0,             0,             S0,            S1,            X0,            Y0,            X1,            Y1,            Z0^X3^Y3,      X3,            Y4,            X4,            Y5,            X5,            X2^Y6,         Y2^Y7,         },
    {0,             0,             0,             S0,            S1,            X0,            Y0,            X1,            Z0^X3^Y3,      X3,            Y2,            X4,            Y4,            X5,            X2^Y5,         Y1^Y6,         },
    {0,             0,             0,             0,             S0,            S1,            X0,            Y0,            Z0^X3^Y3,      X2,            Y2,            X3,            Y4,            X4,            X1^Y5,         Y1^Y6,         },
    {S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Z1^X3^Y3,      Z0^X4^Y4,      Y3,            X4,            Y5,            X5,            Y6,            X6,            },
    {0,             S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Z1^X3^Y3,      Z0^X4^Y4,      X3,            Y4,            X5,            Y5,            X6,            Y2^Y6,         },
    {0,             0,             S0,            S1,            X0,            Y0,            X1,            Y1,            Z1^X3^Y3,      Z0^X4^Y4,      Y3,            X4,            Y5,            X5,            Y2^Y6,         X2^Y7,         },
    {0,             0,             0,             S0,            S1,            X0,            Y0,            X1,            Z1^X3^Y3,      Z0^X4^Y4,      Y2,            X3,            Y4,            X5,            Y1^Y5,         X2^Y6,         },
    {0,             0,             0,             0,             S0,            S1,            X0,            Y0,            Z1^X3^Y3,      Z0^X4^Y4,      Y2,            X2,            Y3,            X4,            Y1^Y5,         X1^Y6,         },
    {S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            Y4,            X5,            Y6,            X6,            },
    {0,             S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            X4,            Y5,            X6,            Y2^Y6,         },
    {0,             0,             S0,            S1,            X0,            Y0,            X1,            Y1,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            Y4,            X5,            X2^Y6,         Y2^Y7,         },
    {0,             0,             0,             S0,            S1,            X0,            Y0,            X1,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      Y2,            X3,            Y4,            Y1^Y5,         X2^Y6,         },
    {0,             0,             0,             0,             S0,            S1,            X0,            Y0,            Z1^X3^Y3,      Z0^X4^Y4,      Y2^X5^Y5,      X2,            Y3,            X4,            X1^Y6,         Y1^Y7,         },
    {S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      Y3,            X4,            Y6,            X6,            },
    {0,             S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X3,            Y4,            X6,            Y2^Y6,         },
    {0,             0,             S0,            S1,            X0,            Y0,            X1,            Y1,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      Y3,            X4,            Y2^Y6,         X2^Y7,         },
    {0,             0,             0,             S0,            S1,            X0,            Y0,            X1,            Z2^X3^Y3,      Z1^X4^Y4,      Y2^Y5^X6,      Z0^X5^Y6,      Y3,            X4,            Y1^Y6,         X2^Y7,         },
    {0,             0,             0,             0,             S0,            S1,            X0,            Y0,            Z1^X3^Y3,      Z0^X4^Y4,      Y2^Y5^X6,      X1^X5^Y6,      Y3,            X4,            Y1^Y6,         X2^Y7,         },
    {S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      Z4^X6^Y6,      X3,            Y4,            X6,            },
    {0,             S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      Z4^X6^Y6,      X3,            Y4,            Y2^Y6,         },
    {0,             0,             S0,            S1,            X0,            Y0,            X1,            Y1,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      Y2^X6^Y6,      Y3,            X4,            X2^Y7,         },
    {0,             0,             0,             S0,            S1,            X0,            Y0,            X1,            Z2^X3^Y3,      Z1^X4^Y4,      Y2^Y5^X7,      Z0^X5^Y7,      Y1^X6^Y6,      Y3,            X4,            X2^Y7,         },
    {0,             0,             0,             0,             S0,            S1,            X0,            Y0,            Z1^X3^Y3,      Z0^X4^Y4,      Y2^Y5^X7,      X1^X5^Y7,      Y1^X6^Y6,      Y3,            X4,            X2^Y7,         },
    {S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X8,      Z0^X5^Y8,      Z5^Y6^X7,      Z4^X6^Y7,      Y3,            X4,            },
    {0,             S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X8,      Z0^X5^Y8,      Y2^Y6^X7,      Z4^X6^Y7,      Y3,            X4,            },
    {0,             0,             S0,            S1,            X0,            Y0,            X1,            Y1,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X8,      Z0^X5^Y8,      Y2^Y6^X7,      X2^X6^Y7,      Y3,            X4,            },
    {0,             0,             0,             S0,            S1,            X0,            Y0,            X1,            Z2^X3^Y3,      Z1^X4^Y4,      Y2^Y5^X8,      Z0^X5^Y8,      Y1^Y6^X7,      X2^X6^Y7,      Y3,            X4,            },
    {0,             0,             0,             0,             S0,            S1,            X0,            Y0,            Z1^X3^Y3,      Z0^X4^Y4,      Y2^Y5^X8,      X1^X5^Y8,      Y1^Y6^X7,      X2^X6^Y7,      Y3,            X4,            },
    {S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            X2,            Y3,            X3,            Y4,            X4,            Y5,            X5,            Y6,            Y2^Y7,         },
    {0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            Y3,            X3,            Y4,            X4,            Y5,            X5,            Y2^Y6,         X2^Y7,         },
    {0,             0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y3,            X3,            Y4,            X4,            Y5,            Y1^Y6,         Y2^Y7,         X2^Y8,         },
    {0,             0,             0,             S0,            S1,            S2,            X0,            Y0,            X2,            Y3,            X3,            Y4,            X4,            Y1^Y5,         X1^Y6,         Y2^Y7,         },
    {0,             0,             0,             0,             S0,            S1,            S2,            X0,            Y2,            X2,            Y3,            X3,            Y4,            Y0^Y5,         Y1^Y6,         X1^Y7,         },
    {S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            X2,            Z0^X3^Y3,      X3,            Y4,            X4,            Y5,            X5,            Y6,            Y2^Y7,         },
    {0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            Z0^X3^Y3,      X3,            Y4,            X4,            Y5,            X5,            X2^Y6,         Y2^Y7,         },
    {0,             0,             S0,            S1,            S2,            X0,            Y0,            X1,            Z0^X3^Y3,      X3,            Y4,            X4,            Y5,            Y1^Y6,         X2^Y7,         Y2^Y8,         },
    {0,             0,             0,             S0,            S1,            S2,            X0,            Y0,            Z0^X3^Y3,      Y2,            X3,            Y4,            X4,            X1^Y5,         Y1^Y6,         X2^Y7,         },
    {0,             0,             0,             0,             S0,            S1,            S2,            X0,            Z0^X3^Y3,      X2,            Y2,            X3,            Y4,            Y0^Y5,         X1^Y6,         Y1^Y7,         },
    {S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            X2,            Z1^X3^Y3,      Z0^X4^Y4,      Y3,            X4,            Y5,            X5,            Y6,            Y2^Y7,         },
    {0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            Z1^X3^Y3,      Z0^X4^Y4,      Y3,            X4,            Y5,            X5,            Y2^Y6,         X2^Y7,         },
    {0,             0,             S0,            S1,            S2,            X0,            Y0,            X1,            Z1^X3^Y3,      Z0^X4^Y4,      Y3,            X4,            Y5,            Y1^Y6,         Y2^Y7,         X2^Y8,         },
    {0,             0,             0,             S0,            S1,            S2,            X0,            Y0,            Z1^X3^Y3,      Z0^X4^Y4,      X2,            Y3,            X4,            Y1^Y5,         X1^Y6,         Y2^Y7,         },
    {0,             0,             0,             0,             S0,            S1,            S2,            X0,            Z1^X3^Y3,      Z0^X4^Y4,      X2,            Y2,            X3,            Y0^Y5,         X1^Y6,         Y1^Y7,         },
    {S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            X2,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            Y4,            X5,            Y6,            Y2^Y7,         },
    {0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            Y4,            X5,            X2^Y6,         Y2^Y7,         },
    {0,             0,             S0,            S1,            S2,            X0,            Y0,            X1,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            Y4,            Y1^Y6,         X2^Y7,         Y2^Y8,         },
    {0,             0,             0,             S0,            S1,            S2,            X0,            Y0,            Z1^X3^Y3,      Z0^X4^Y4,      Y2^X5^Y5,      Y3,            X4,            X1^Y6,         Y1^Y7,         X2^Y8,         },
    {0,             0,             0,             0,             S0,            S1,            S2,            X0,            Z1^X3^Y3,      Z0^X4^Y4,      Y0^X5^Y5,      Y2,            X3,            X1^Y6,         Y1^Y7,         X2^Y8,         },
    {S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            X2,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X6,      Z3^X5^Y6,      Y3,            X4,            Y6,            Y2^Y7,         },
    {0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X6,      Z3^X5^Y6,      Y3,            X4,            Y2^Y6,         X2^Y7,         },
    {0,             0,             S0,            S1,            S2,            X0,            Y0,            X1,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X6,      Y2^X5^Y6,      X3,            Y4,            X2^Y6,         Y1^Y7,         },
    {0,             0,             0,             S0,            S1,            S2,            X0,            Y0,            Z1^X3^Y3,      Z0^X4^Y4,      Y2^Y5^X6,      X1^X5^Y6,      Y3,            X4,            Y1^Y6,         X2^Y7,         },
    {0,             0,             0,             0,             S0,            S1,            S2,            X0,            Z1^X3^Y3,      Z0^X4^Y4,      Y0^Y5^X6,      X1^X5^Y6,      X3,            Y1^Y6,         X2^Y7,         Y2^Y8,         },
    {S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            X2,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X7,      Z4^X5^Y7,      Z3^X6^Y6,      X3,            Y4,            Y2^Y7,         },
    {0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X7,      Z3^X5^Y7,      Y2^X6^Y6,      Y3,            X4,            X2^Y7,         },
    {0,             0,             S0,            S1,            S2,            X0,            Y0,            X1,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X7,      Y2^X5^Y7,      X2^X6^Y6,      X3,            Y4,            Y1^Y7,         },
    {0,             0,             0,             S0,            S1,            S2,            X0,            Y0,            Z1^X3^Y3,      Z0^X4^Y4,      Y2^Y5^X7,      X1^X5^Y7,      Y1^X6^Y6,      Y3,            X4,            X2^Y7,         },
    {0,             0,             0,             0,             S0,            S1,            S2,            X0,            Z1^X3^Y3,      Z0^X4^Y4,      Y0^Y5^X7,      X1^X5^Y7,      Y2^X6^Y6,      X3,            X2^Y7,         Y1^Y8,         },
    {S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            X2,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X8,      Z4^X5^Y8,      Z3^Y6^X7,      Y2^X6^Y7,      X3,            Y4,            },
    {0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X8,      Z3^X5^Y8,      Y2^Y6^X7,      X2^X6^Y7,      Y3,            X4,            },
    {0,             0,             S0,            S1,            S2,            X0,            Y0,            X1,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X8,      Y2^X5^Y8,      X2^Y6^X7,      Y1^X6^Y7,      X3,            Y4,            },
    {0,             0,             0,             S0,            S1,            S2,            X0,            Y0,            Z1^X3^Y3,      Z0^X4^Y4,      Y2^Y5^X8,      X1^X5^Y8,      Y1^Y6^X7,      X2^X6^Y7,      Y3,            X4,            },
    {0,             0,             0,             0,             S0,            S1,            S2,            X0,            Z1^X3^Y3,      Z0^X4^Y4,      Y0^Y5^X8,      X1^X5^Y8,      Y2^Y6^X7,      X2^X6^Y7,      X3,            Y1^Y7,         },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y4,            Z0^X3^Y3,      X4,            Y5,            X5,            Y6,            X6,            Y7,            X7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Z0^X3^Y3,      X4,            Y4,            X5,            Y5,            X6,            Y6,            X7,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Z0^X3^Y3,      X3,            Y2,            X4,            Y4,            X5,            Y5,            X6,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y4,            Z1^X3^Y3,      Z0^X4^Y4,      Y5,            X5,            Y6,            X6,            Y7,            X7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Z1^X3^Y3,      Z0^X4^Y4,      Y4,            X5,            Y5,            X6,            Y6,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Z1^X3^Y3,      Z0^X4^Y4,      Y3,            X4,            Y5,            X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Z1^X3^Y3,      Z0^X4^Y4,      Y2,            X3,            Y4,            X5,            Y5,            X6,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y4,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X5,            Y6,            X6,            Y7,            X7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X4,            Y5,            X6,            Y6,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            Y4,            X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            Y2,            X4,            Y5,            X6,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y4,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      Y6,            X6,            Y7,            X7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      Y4,            X6,            Y6,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      Y3,            X4,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      Y2,            X3,            Y4,            X6,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y4,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X6,            Y7,            X7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X4,            Y6,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X3,            Y4,            X6,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      Y2,            X3,            Y4,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y4,            X3^Y3^Z5,      X4^Y4^Z4,      Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      Z0^X6^Y7,      Y7,            X7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            X3^Y3^Z5,      X4^Y4^Z4,      Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      Z0^X6^Y7,      Y4,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            X3^Y3^Z5,      X4^Y4^Z4,      Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      Z0^X6^Y7,      Y3,            X4,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X8,      Z1^X5^Y8,      Y2^Y6^X7,      Z0^X6^Y7,      Y3,            X4,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4,            X4,            Y5,            X5,            Y6,            X6,            Y7,            S0^Y8,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3,            X4,            Y4,            X5,            Y5,            X6,            Y6,            S0^Y7,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3,            X3,            Y4,            X4,            Y5,            X5,            Y6,            S0^Y7,         },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2,            X3,            Y3,            X4,            Y4,            X5,            Y5,            S0^Y6,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2,            X2,            Y3,            X3,            Y4,            X4,            Y5,            S0^Y6,         },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y4,            Z0^X3^Y3,      X4,            Y5,            X5,            Y6,            X6,            Y7,            S0^Y8,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Z0^X3^Y3,      X4,            Y4,            X5,            Y5,            X6,            Y6,            S0^Y7,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Z0^X3^Y3,      X3,            Y4,            X4,            Y5,            X5,            Y6,            S0^Y7,         },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Z0^X3^Y3,      X3,            Y2,            X4,            Y4,            X5,            Y5,            S0^Y6,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z0^X3^Y3,      X2,            Y2,            X3,            Y4,            X4,            Y5,            S0^Y6,         },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y4,            Z1^X3^Y3,      Z0^X4^Y4,      Y5,            X5,            Y6,            X6,            Y7,            S0^Y8,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Z1^X3^Y3,      Z0^X4^Y4,      Y4,            X5,            Y5,            X6,            Y6,            S0^Y7,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Z1^X3^Y3,      Z0^X4^Y4,      Y3,            X4,            Y5,            X5,            Y6,            S0^Y7,         },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Z1^X3^Y3,      Z0^X4^Y4,      Y2,            X3,            Y4,            X5,            Y5,            S0^Y6,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z1^X3^Y3,      Z0^X4^Y4,      Y2,            X2,            Y3,            X4,            Y5,            S0^Y6,         },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y4,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X5,            Y6,            X6,            Y7,            S0^Y8,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X4,            Y5,            X6,            Y6,            S0^Y7,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            Y4,            X5,            Y6,            S0^Y7,         },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            Y2,            X4,            Y5,            S0^Y6,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X2,            Y2,            X3,            Y4,            S0^Y6,         },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y4,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      Y6,            X6,            Y7,            S0^Y8,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      Y4,            X6,            Y6,            S0^Y7,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      Y3,            X4,            Y6,            S0^Y7,         },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      Y2,            X3,            Y4,            S0^Y6,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X6,      S0^X5^Y6,      Y2,            X2,            Y3,            X4,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y4,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X6,            Y7,            S0^Y8,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X4,            Y6,            S0^Y7,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X3,            Y4,            S0^Y7,         },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      S0^X6^Y6,      Y2,            X3,            Y4,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X7,      S0^X5^Y7,      Y2^X6^Y6,      X2,            Y3,            X4,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y4,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X8,      Z1^X5^Y8,      Z0^Y6^X7,      Z5^X6^Y7,      Y7,            S0^Y8,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X8,      Z1^X5^Y8,      Z0^Y6^X7,      Z5^X6^Y7,      Y4,            S0^Y7,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            X3^Y3^Z4,      Z3^X4^Y4,      Z2^Y5^X8,      Z1^X5^Y8,      Z0^Y6^X7,      S0^X6^Y7,      Y3,            X4,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X8,      Z0^X5^Y8,      S0^Y6^X7,      Y2^X6^Y7,      X3,            Y4,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X8,      S0^X5^Y8,      Y2^Y6^X7,      X2^X6^Y7,      Y3,            X4,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4,            X4,            Y5,            X5,            Y6,            X6,            S0^Y7,         S1^Y8,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3,            X4,            Y4,            X5,            Y5,            X6,            S0^Y6,         S1^Y7,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3,            X3,            Y4,            X4,            Y5,            X5,            S0^Y6,         S1^Y7,         },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2,            X3,            Y3,            X4,            Y4,            X5,            S0^Y5,         S1^Y6,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2,            X2,            Y3,            X3,            Y4,            X4,            S0^Y5,         S1^Y6,         },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y4,            Z0^X3^Y3,      X4,            Y5,            X5,            Y6,            X6,            S0^Y7,         S1^Y8,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Z0^X3^Y3,      X4,            Y4,            X5,            Y5,            X6,            S0^Y6,         S1^Y7,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Z0^X3^Y3,      X3,            Y4,            X4,            Y5,            X5,            S0^Y6,         S1^Y7,         },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Z0^X3^Y3,      X3,            Y2,            X4,            Y4,            X5,            S0^Y5,         S1^Y6,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z0^X3^Y3,      X2,            Y2,            X3,            Y4,            X4,            S0^Y5,         S1^Y6,         },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y4,            Z1^X3^Y3,      Z0^X4^Y4,      Y5,            X5,            Y6,            X6,            S0^Y7,         S1^Y8,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Z1^X3^Y3,      Z0^X4^Y4,      Y4,            X5,            Y5,            X6,            S0^Y6,         S1^Y7,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Z1^X3^Y3,      Z0^X4^Y4,      Y3,            X4,            Y5,            X5,            S0^Y6,         S1^Y7,         },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Z1^X3^Y3,      Z0^X4^Y4,      Y2,            X3,            Y4,            X5,            S0^Y5,         S1^Y6,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z1^X3^Y3,      Z0^X4^Y4,      Y2,            X2,            Y3,            X4,            S0^Y5,         S1^Y6,         },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y4,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X5,            Y6,            X6,            S0^Y7,         S1^Y8,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X4,            Y5,            X6,            S0^Y6,         S1^Y7,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            Y4,            X5,            S0^Y6,         S1^Y7,         },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      Y2,            X3,            Y4,            S0^Y5,         S1^Y6,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z1^X3^Y3,      Z0^X4^Y4,      S1^X5^Y5,      X2,            Y2,            X3,            Y4,            S0^Y6,         },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y4,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      Y6,            X6,            S0^Y7,         S1^Y8,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      Y4,            X6,            S0^Y6,         S1^Y7,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      Y3,            X4,            S0^Y6,         S1^Y7,         },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Z2^X3^Y3,      Z1^X4^Y4,      S1^Y5^X6,      Z0^X5^Y6,      Y2,            X3,            Y4,            S0^Y6,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z1^X3^Y3,      Z0^X4^Y4,      S1^Y5^X6,      S0^X5^Y6,      Y2,            X2,            Y3,            X4,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y4,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      Z4^X6^Y6,      X6,            S0^Y7,         S1^Y8,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      Z4^X6^Y6,      Y4,            S0^Y6,         S1^Y7,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      S1^X6^Y6,      X3,            Y4,            S0^Y7,         },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Z2^X3^Y3,      Z1^X4^Y4,      S1^Y5^X7,      Z0^X5^Y7,      S0^X6^Y6,      Y2,            X3,            Y4,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z1^X3^Y3,      Z0^X4^Y4,      S1^Y5^X7,      S0^X5^Y7,      Y2^X6^Y6,      X2,            Y3,            X4,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y4,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X8,      Z0^X5^Y8,      Z5^Y6^X7,      Z4^X6^Y7,      S0^Y7,         S1^Y8,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X8,      Z0^X5^Y8,      S1^Y6^X7,      Z4^X6^Y7,      Y4,            S0^Y7,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            X3^Y3^Z3,      Z2^X4^Y4,      Z1^Y5^X8,      Z0^X5^Y8,      S1^Y6^X7,      S0^X6^Y7,      Y3,            X4,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Z2^X3^Y3,      Z1^X4^Y4,      S1^Y5^X8,      Z0^X5^Y8,      S0^Y6^X7,      Y2^X6^Y7,      X3,            Y4,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z1^X3^Y3,      Z0^X4^Y4,      S1^Y5^X8,      S0^X5^Y8,      Y2^Y6^X7,      X2^X6^Y7,      Y3,            X4,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4,            X4,            Y5,            X5,            Y6,            S0^Y7,         S1^Y8,         S2^Y9,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3,            X4,            Y4,            X5,            Y5,            S0^Y6,         S1^Y7,         S2^Y8,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3,            X3,            Y4,            X4,            Y5,            S0^Y6,         S1^Y7,         S2^Y8,         },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2,            X3,            Y3,            X4,            Y4,            S0^Y5,         S1^Y6,         S2^Y7,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2,            X2,            Y3,            X3,            Y4,            S0^Y5,         S1^Y6,         S2^Y7,         },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y4,            Z0^X3^Y3,      X4,            Y5,            X5,            Y6,            S0^Y7,         S1^Y8,         S2^Y9,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Z0^X3^Y3,      X4,            Y4,            X5,            Y5,            S0^Y6,         S1^Y7,         S2^Y8,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Z0^X3^Y3,      X3,            Y4,            X4,            Y5,            S0^Y6,         S1^Y7,         S2^Y8,         },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Z0^X3^Y3,      X3,            Y2,            X4,            Y4,            S0^Y5,         S1^Y6,         S2^Y7,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z0^X3^Y3,      X2,            Y2,            X3,            Y4,            S0^Y5,         S1^Y6,         S2^Y7,         },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y4,            Z1^X3^Y3,      Z0^X4^Y4,      Y5,            X5,            Y6,            S0^Y7,         S1^Y8,         S2^Y9,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Z1^X3^Y3,      Z0^X4^Y4,      Y4,            X5,            Y5,            S0^Y6,         S1^Y7,         S2^Y8,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Z1^X3^Y3,      Z0^X4^Y4,      Y3,            X4,            Y5,            S0^Y6,         S1^Y7,         S2^Y8,         },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Z1^X3^Y3,      Z0^X4^Y4,      Y2,            X3,            Y4,            S0^Y5,         S1^Y6,         S2^Y7,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z1^X3^Y3,      Z0^X4^Y4,      X2,            Y2,            X3,            S0^Y5,         S1^Y6,         S2^Y7,         },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y4,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X5,            Y6,            S0^Y7,         S1^Y8,         S2^Y9,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X4,            Y5,            S0^Y6,         S1^Y7,         S2^Y8,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            Y4,            S0^Y6,         S1^Y7,         S2^Y8,         },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Z1^X3^Y3,      Z0^X4^Y4,      S2^X5^Y5,      Y2,            X3,            Y4,            S0^Y6,         S1^Y7,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z1^X3^Y3,      Z0^X4^Y4,      S2^X5^Y5,      X2,            Y2,            X3,            S0^Y6,         S1^Y7,         },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y4,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X6,      Z3^X5^Y6,      Y6,            S0^Y7,         S1^Y8,         S2^Y9,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X6,      Z3^X5^Y6,      Y4,            S0^Y6,         S1^Y7,         S2^Y8,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X6,      S2^X5^Y6,      Y3,            X4,            S0^Y6,         S1^Y7,         },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Z1^X3^Y3,      Z0^X4^Y4,      S2^Y5^X6,      S1^X5^Y6,      Y2,            X3,            Y4,            S0^Y6,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z1^X3^Y3,      Z0^X4^Y4,      S2^Y5^X6,      S1^X5^Y6,      X2,            Y2,            X3,            S0^Y6,         },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y4,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X7,      Z4^X5^Y7,      Z3^X6^Y6,      S0^Y7,         S1^Y8,         S2^Y9,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X7,      Z3^X5^Y7,      S2^X6^Y6,      Y4,            S0^Y7,         S1^Y8,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X7,      S2^X5^Y7,      S1^X6^Y6,      X3,            Y4,            S0^Y7,         },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Z1^X3^Y3,      Z0^X4^Y4,      S2^Y5^X7,      S1^X5^Y7,      S0^X6^Y6,      Y2,            X3,            Y4,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z1^X3^Y3,      Z0^X4^Y4,      S2^Y5^X7,      S1^X5^Y7,      S0^X6^Y6,      X2,            Y2,            X3,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y4,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X8,      Z4^X5^Y8,      Z3^Y6^X7,      S2^X6^Y7,      S0^Y7,         S1^Y8,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X8,      Z3^X5^Y8,      S2^Y6^X7,      S1^X6^Y7,      Y4,            S0^Y7,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Z2^X3^Y3,      Z1^X4^Y4,      Z0^Y5^X8,      S2^X5^Y8,      S1^Y6^X7,      S0^X6^Y7,      Y3,            X4,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Z1^X3^Y3,      Z0^X4^Y4,      S2^Y5^X8,      S1^X5^Y8,      S0^Y6^X7,      Y2^X6^Y7,      X3,            Y4,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z1^X3^Y3,      Z0^X4^Y4,      S2^Y5^X8,      S1^X5^Y8,      S0^Y6^X7,      X2^X6^Y7,      Y2,            X3,            },
    {X0,            X1,            Z0,            Y0,            Z1,            Y1,            X2,            Z2,            Y2,            X3,            Z3,            Y3,            0,             0,             0,             0,             },
    {0,             X0,            Z0,            Y0,            Z1,            Y1,            X1,            Z2,            Y2,            X2,            Z3,            Y3,            0,             0,             0,             0,             },
    {0,             0,             X0,            Y0,            Z0,            Y1,            X1,            Z1,            Y2,            X2,            Z2,            Y3,            0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Z0,            Y0,            X1,            Z1,            Y1,            X2,            Z2,            Y2,            0,             0,             0,             0,             },
    {0,             0,             0,             0,             Z0,            Y0,            X0,            Z1,            Y1,            X1,            Z2,            Y2,            0,             0,             0,             0,             },
    {X0,            X1,            Z0,            Y0,            Z1,            Y1,            X2,            Z2,            Y2^X3^Z3,      X3,            Z3,            Y3,            0,             0,             0,             0,             },
    {0,             X0,            Z0,            Y0,            Z1,            Y1,            X1,            Z2,            X2^Y2^Z3,      X2,            Z3,            Y3,            0,             0,             0,             0,             },
    {0,             0,             X0,            Y0,            Z0,            Y1,            X1,            Z1,            X2^Y2^Z2,      X2,            Z2,            Y3,            0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Z0,            Y0,            X1,            Z1,            Y1^X2^Z2,      X2,            Z2,            Y2,            0,             0,             0,             0,             },
    {0,             0,             0,             0,             Z0,            Y0,            X0,            Z1,            X1^Y1^Z2,      X1,            Z2,            Y2,            0,             0,             0,             0,             },
    {X0,            X1,            Z0,            Y0,            Z1,            Y1,            X2,            Z2,            Y2^X4^Z4,      X3^Y3^Z3,      Z3,            Y3,            0,             0,             0,             0,             },
    {0,             X0,            Z0,            Y0,            Z1,            Y1,            X1,            Z2,            Y2^X3^Z4,      X2^Y3^Z3,      Z3,            Y3,            0,             0,             0,             0,             },
    {0,             0,             X0,            Y0,            Z0,            Y1,            X1,            Z1,            Y2^X3^Z3,      X2^Z2^Y3,      Z2,            Y3,            0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Z0,            Y0,            X1,            Z1,            Y1^X3^Z3,      X2^Y2^Z2,      Z2,            Y2,            0,             0,             0,             0,             },
    {0,             0,             0,             0,             Z0,            Y0,            X0,            Z1,            Y1^X2^Z3,      X1^Y2^Z2,      Z2,            Y2,            0,             0,             0,             0,             },
    {X0,            X1,            Z0,            Y0,            Z1,            Y1,            X2,            Z2,            Y2^X5^Z5,      X3^Y4^Z4,      Y3^Z3^X4,      Y3,            0,             0,             0,             0,             },
    {0,             X0,            Z0,            Y0,            Z1,            Y1,            X1,            Z2,            Y2^X4^Z5,      X2^Y4^Z4,      X3^Y3^Z3,      Y3,            0,             0,             0,             0,             },
    {0,             0,             X0,            Y0,            Z0,            Y1,            X1,            Z1,            Y2^X4^Z4,      X2^Z3^Y4,      Z2^X3^Y3,      Y3,            0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Z0,            Y0,            X1,            Z1,            Y1^X4^Z4,      X2^Y3^Z3,      Y2^Z2^X3,      Y2,            0,             0,             0,             0,             },
    {0,             0,             0,             0,             Z0,            Y0,            X0,            Z1,            Y1^X3^Z4,      X1^Y3^Z3,      X2^Y2^Z2,      Y2,            0,             0,             0,             0,             },
    {X0,            X1,            Z0,            Y0,            Z1,            Y1,            X2,            Z2,            Y2^X6^Z6,      X3^Y5^Z5,      Z3^Y4^X5,      Y3^X4^Z4,      0,             0,             0,             0,             },
    {0,             X0,            Z0,            Y0,            Z1,            Y1,            X1,            Z2,            Y2^X5^Z6,      X2^Y5^Z5,      Z3^X4^Y4,      X3^Y3^Z4,      0,             0,             0,             0,             },
    {0,             0,             X0,            Y0,            Z0,            Y1,            X1,            Z1,            Y2^X5^Z5,      X2^Z4^Y5,      Z2^X4^Y4,      X3^Y3^Z3,      0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Z0,            Y0,            X1,            Z1,            Y1^X5^Z5,      X2^Y4^Z4,      Z2^Y3^X4,      Y2^X3^Z3,      0,             0,             0,             0,             },
    {0,             0,             0,             0,             Z0,            Y0,            X0,            Z1,            Y1^X4^Z5,      X1^Y4^Z4,      Z2^X3^Y3,      X2^Y2^Z3,      0,             0,             0,             0,             },
    {X0,            X1,            Z0,            Y0,            Z1,            Y1,            X2,            Z2,            Y2,            X3,            Z3,            Y3,            X4,            Z4,            Y4,            X5,            },
    {0,             X0,            Z0,            Y0,            Z1,            Y1,            X1,            Z2,            Y2,            X2,            Z3,            Y3,            X3,            Z4,            Y4,            X4,            },
    {0,             0,             X0,            Y0,            Z0,            Y1,            X1,            Z1,            Y2,            X2,            Z2,            Y3,            X3,            Z3,            Y4,            X4,            },
    {0,             0,             0,             X0,            Z0,            Y0,            X1,            Z1,            Y1,            X2,            Z2,            Y2,            X3,            Z3,            Y3,            X4,            },
    {0,             0,             0,             0,             Z0,            Y0,            X0,            Z1,            Y1,            X1,            Z2,            Y2,            X2,            Z3,            Y3,            X3,            },
    {X0,            X1,            Z0,            Y0,            Z1,            Y1,            X2,            Z2,            Y2^X3^Z3,      X3,            Z3,            Y3,            X4,            Z4,            Y4,            X5,            },
    {0,             X0,            Z0,            Y0,            Z1,            Y1,            X1,            Z2,            X2^Y2^Z3,      X2,            Z3,            Y3,            X3,            Z4,            Y4,            X4,            },
    {0,             0,             X0,            Y0,            Z0,            Y1,            X1,            Z1,            X2^Y2^Z2,      X2,            Z2,            Y3,            X3,            Z3,            Y4,            X4,            },
    {0,             0,             0,             X0,            Z0,            Y0,            X1,            Z1,            Y1^X2^Z2,      X2,            Z2,            Y2,            X3,            Z3,            Y3,            X4,            },
    {0,             0,             0,             0,             Z0,            Y0,            X0,            Z1,            X1^Y1^Z2,      X1,            Z2,            Y2,            X2,            Z3,            Y3,            X3,            },
    {X0,            X1,            Z0,            Y0,            Z1,            Y1,            X2,            Z2,            Y2^X4^Z4,      X3^Y3^Z3,      Z3,            Y3,            X4,            Z4,            Y4,            X5,            },
    {0,             X0,            Z0,            Y0,            Z1,            Y1,            X1,            Z2,            Y2^X3^Z4,      X2^Y3^Z3,      Z3,            Y3,            X3,            Z4,            Y4,            X4,            },
    {0,             0,             X0,            Y0,            Z0,            Y1,            X1,            Z1,            Y2^X3^Z3,      X2^Z2^Y3,      Z2,            Y3,            X3,            Z3,            Y4,            X4,            },
    {0,             0,             0,             X0,            Z0,            Y0,            X1,            Z1,            Y1^X3^Z3,      X2^Y2^Z2,      Z2,            Y2,            X3,            Z3,            Y3,            X4,            },
    {0,             0,             0,             0,             Z0,            Y0,            X0,            Z1,            Y1^X2^Z3,      X1^Y2^Z2,      Z2,            Y2,            X2,            Z3,            Y3,            X3,            },
    {X0,            X1,            Z0,            Y0,            Z1,            Y1,            X2,            Z2,            Y2^X5^Z5,      X3^Y4^Z4,      Y3^Z3^X4,      Y3,            X4,            Z4,            Y4,            X5,            },
    {0,             X0,            Z0,            Y0,            Z1,            Y1,            X1,            Z2,            Y2^X4^Z5,      X2^Y4^Z4,      X3^Y3^Z3,      Y3,            X3,            Z4,            Y4,            X4,            },
    {0,             0,             X0,            Y0,            Z0,            Y1,            X1,            Z1,            Y2^X4^Z4,      X2^Z3^Y4,      Z2^X3^Y3,      Y3,            X3,            Z3,            Y4,            X4,            },
    {0,             0,             0,             X0,            Z0,            Y0,            X1,            Z1,            Y1^X4^Z4,      X2^Y3^Z3,      Y2^Z2^X3,      Y2,            X3,            Z3,            Y3,            X4,            },
    {0,             0,             0,             0,             Z0,            Y0,            X0,            Z1,            Y1^X3^Z4,      X1^Y3^Z3,      X2^Y2^Z2,      Y2,            X2,            Z3,            Y3,            X3,            },
    {X0,            X1,            Z0,            Y0,            Z1,            Y1,            X2,            Z2,            Y2^X6^Z6,      X3^Y5^Z5,      Z3^Y4^X5,      Y3^X4^Z4,      X4,            Z4,            Y4,            X5,            },
    {0,             X0,            Z0,            Y0,            Z1,            Y1,            X1,            Z2,            Y2^X5^Z6,      X2^Y5^Z5,      Z3^X4^Y4,      X3^Y3^Z4,      X3,            Z4,            Y4,            X4,            },
    {0,             0,             X0,            Y0,            Z0,            Y1,            X1,            Z1,            Y2^X5^Z5,      X2^Z4^Y5,      Z2^X4^Y4,      X3^Y3^Z3,      X3,            Z3,            Y4,            X4,            },
    {0,             0,             0,             X0,            Z0,            Y0,            X1,            Z1,            Y1^X5^Z5,      X2^Y4^Z4,      Z2^Y3^X4,      Y2^X3^Z3,      X3,            Z3,            Y3,            X4,            },
    {0,             0,             0,             0,             Z0,            Y0,            X0,            Z1,            Y1^X4^Z5,      X1^Y4^Z4,      Z2^X3^Y3,      X2^Y2^Z3,      X2,            Z3,            Y3,            X3,            },
    {X0,            X1,            Z0,            Y0,            Z1,            Y1,            X2,            Z2,            Y2^X7^Z7,      X3^Y6^Z6,      Z3^Y5^X6,      Y3^X5^Z5,      X4^Y4^Z4,      Z4,            Y4,            X5,            },
    {0,             X0,            Z0,            Y0,            Z1,            Y1,            X1,            Z2,            Y2^X6^Z7,      X2^Y6^Z6,      Z3^X5^Y5,      Y3^X4^Z5,      X3^Y4^Z4,      Z4,            Y4,            X4,            },
    {0,             0,             X0,            Y0,            Z0,            Y1,            X1,            Z1,            Y2^X6^Z6,      X2^Z5^Y6,      Z2^X5^Y5,      Y3^X4^Z4,      X3^Z3^Y4,      Z3,            Y4,            X4,            },
    {0,             0,             0,             X0,            Z0,            Y0,            X1,            Z1,            Y1^X6^Z6,      X2^Y5^Z5,      Z2^Y4^X5,      Y2^X4^Z4,      X3^Y3^Z3,      Z3,            Y3,            X4,            },
    {0,             0,             0,             0,             Z0,            Y0,            X0,            Z1,            Y1^X5^Z6,      X1^Y5^Z5,      Z2^X4^Y4,      Y2^X3^Z4,      X2^Y3^Z3,      Z3,            Y3,            X3,            },
    {X0,            X1,            Z0,            Y0,            Z1,            Y1,            X2,            Z2,            Y2^X8^Z8,      X3^Y7^Z7,      Z3^Y6^X7,      Y3^X6^Z6,      X4^Y5^Z5,      Y4^Z4^X5,      Y4,            X5,            },
    {0,             X0,            Z0,            Y0,            Z1,            Y1,            X1,            Z2,            Y2^X7^Z8,      X2^Y7^Z7,      Z3^X6^Y6,      Y3^X5^Z6,      X3^Y5^Z5,      X4^Y4^Z4,      Y4,            X4,            },
    {0,             0,             X0,            Y0,            Z0,            Y1,            X1,            Z1,            Y2^X7^Z7,      X2^Z6^Y7,      Z2^X6^Y6,      Y3^X5^Z5,      X3^Z4^Y5,      Z3^X4^Y4,      Y4,            X4,            },
    {0,             0,             0,             X0,            Z0,            Y0,            X1,            Z1,            Y1^X7^Z7,      X2^Y6^Z6,      Z2^Y5^X6,      Y2^X5^Z5,      X3^Y4^Z4,      Y3^Z3^X4,      Y3,            X4,            },
    {0,             0,             0,             0,             Z0,            Y0,            X0,            Z1,            Y1^X6^Z7,      X1^Y6^Z6,      Z2^X5^Y5,      Y2^X4^Z5,      X2^Y4^Z4,      X3^Y3^Z3,      Y3,            X3,            },
    {X0,            X1,            Z0,            Y0,            Z1,            Y1,            X2,            Z2,            Y2^X5,         X3^Y4^Z4,      Y3^Z3^X4,      Y3,            X4,            Z4,            Y4,            X5,            },
    {0,             X0,            Z0,            Y0,            Z1,            Y1,            X1,            Z2,            Y2^X4,         X2^Y4^Z4,      X3^Y3^Z3,      Y3,            X3,            Z4,            Y4,            X4,            },
    {0,             0,             X0,            Y0,            Z0,            Y1,            X1,            Z1,            Y2^X4,         X2^Z3^Y4,      Z2^X3^Y3,      Y3,            X3,            Z3,            Y4,            X4,            },
    {0,             0,             0,             X0,            Z0,            Y0,            X1,            Z1,            Y1^X4,         X2^Y3^Z3,      Y2^Z2^X3,      Y2,            X3,            Z3,            Y3,            X4,            },
    {0,             0,             0,             0,             Z0,            Y0,            X0,            Z1,            Y1^X3,         X1^Y3^Z3,      X2^Y2^Z2,      Y2,            X2,            Z3,            Y3,            X3,            },
    {X0,            X1,            Z0,            Y0,            Z1,            Y1,            X2,            Z2,            Y2,            X3,            Z3^Y4^X5,      Y3^X4^Z4,      X4,            Z4,            Y4,            X5,            },
    {0,             X0,            Z0,            Y0,            Z1,            Y1,            X1,            Z2,            Y2,            X2,            Z3^X4^Y4,      X3^Y3^Z4,      X3,            Z4,            Y4,            X4,            },
    {0,             0,             X0,            Y0,            Z0,            Y1,            X1,            Z1,            Y2,            X2,            Z2^X4^Y4,      X3^Y3^Z3,      X3,            Z3,            Y4,            X4,            },
    {0,             0,             0,             X0,            Z0,            Y0,            X1,            Z1,            Y1,            X2,            Z2^Y3^X4,      Y2^X3^Z3,      X3,            Z3,            Y3,            X4,            },
    {0,             0,             0,             0,             Z0,            Y0,            X0,            Z1,            Y1,            X1,            Z2^X3^Y3,      X2^Y2^Z3,      X2,            Z3,            Y3,            X3,            },
    {X0,            X1,            Z0,            Y0,            Z1,            Y1,            X2,            Z2,            Y2,            X3,            Z3,            Y3^X5,         X4^Y4^Z4,      Z4,            Y4,            X5,            },
    {0,             X0,            Z0,            Y0,            Z1,            Y1,            X1,            Z2,            Y2,            X2,            Z3,            Y3^X4,         X3^Y4^Z4,      Z4,            Y4,            X4,            },
    {0,             0,             X0,            Y0,            Z0,            Y1,            X1,            Z1,            Y2,            X2,            Z2,            Y3^X4,         X3^Z3^Y4,      Z3,            Y4,            X4,            },
    {0,             0,             0,             X0,            Z0,            Y0,            X1,            Z1,            Y1,            X2,            Z2,            Y2^X4,         X3^Y3^Z3,      Z3,            Y3,            X4,            },
    {0,             0,             0,             0,             Z0,            Y0,            X0,            Z1,            Y1,            X1,            Z2,            Y2^X3,         X2^Y3^Z3,      Z3,            Y3,            X3,            },
    {X0,            X1,            Z0,            Y0,            Z1,            Y1,            X2,            Z2,            Y2,            X3,            Z3,            Y3,            X4,            Y4^Z4^X5,      Y4,            X5,            },
    {0,             X0,            Z0,            Y0,            Z1,            Y1,            X1,            Z2,            Y2,            X2,            Z3,            Y3,            X3,            X4^Y4^Z4,      Y4,            X4,            },
    {0,             0,             X0,            Y0,            Z0,            Y1,            X1,            Z1,            Y2,            X2,            Z2,            Y3,            X3,            Z3^X4^Y4,      Y4,            X4,            },
    {0,             0,             0,             X0,            Z0,            Y0,            X1,            Z1,            Y1,            X2,            Z2,            Y2,            X3,            Y3^Z3^X4,      Y3,            X4,            },
    {0,             0,             0,             0,             Z0,            Y0,            X0,            Z1,            Y1,            X1,            Z2,            Y2,            X2,            X3^Y3^Z3,      Y3,            X3,            },
    {X0,            X1,            Z0,            Y0,            Y1,            Z1,            X2,            Z2,            Y2,            X3,            Z3,            Y3,            X4,            Z4,            Y4,            X5,            },
    {0,             X0,            Z0,            Y0,            X1,            Z1,            Y1,            Z2,            Y2,            X2,            Z3,            Y3,            X3,            Z4,            Y4,            X4,            },
    {0,             0,             X0,            Y0,            X1,            Z0,            Y1,            Z1,            Y2,            X2,            Z2,            Y3,            X3,            Z3,            Y4,            X4,            },
    {0,             0,             0,             X0,            Y0,            Z0,            X1,            Z1,            Y1,            X2,            Z2,            Y2,            X3,            Z3,            Y3,            X4,            },
    {0,             0,             0,             0,             X0,            Z0,            Y0,            Z1,            Y1,            X1,            Z2,            Y2,            X2,            Z3,            Y3,            X3,            },
    {X0,            X1,            Z0,            Y0,            Y1,            Z1,            X2,            Z2,            X3^Y3,         X3,            Z3,            Y2,            X4,            Z4,            Y4,            X5,            },
    {0,             X0,            Z0,            Y0,            X1,            Z1,            Y1,            Z2,            X3^Y3,         X2,            Z3,            Y2,            X3,            Z4,            Y4,            X4,            },
    {0,             0,             X0,            Y0,            X1,            Z0,            Y1,            Z1,            X3^Y3,         X2,            Z2,            Y2,            X3,            Z3,            Y4,            X4,            },
    {0,             0,             0,             X0,            Y0,            Z0,            X1,            Z1,            X3^Y3,         X2,            Z2,            Y1,            X3,            Z3,            Y2,            X4,            },
    {0,             0,             0,             0,             X0,            Z0,            Y0,            Z1,            X3^Y3,         X1,            Z2,            Y1,            X2,            Z3,            Y2,            X3,            },
    {X0,            X1,            Z0,            Y0,            Y1,            Z1,            X2,            Z2,            X3^Y3,         X4^Y4,         Z3,            Y2,            X3,            Z4,            Y4,            X5,            },
    {0,             X0,            Z0,            Y0,            X1,            Z1,            Y1,            Z2,            X3^Y3,         X4^Y4,         Z3,            Y2,            X2,            Z4,            Y3,            X4,            },
    {0,             0,             X0,            Y0,            X1,            Z0,            Y1,            Z1,            X3^Y3,         X4^Y4,         Z2,            Y2,            X2,            Z3,            Y3,            X4,            },
    {0,             0,             0,             X0,            Y0,            Z0,            X1,            Z1,            X3^Y3,         X4^Y4,         Z2,            Y1,            X2,            Z3,            Y2,            X3,            },
    {0,             0,             0,             0,             X0,            Z0,            Y0,            Z1,            X3^Y3,         X1^X4^Y4,      Z2,            Y1,            X2,            Z3,            Y2,            X3,            },
    {X0,            X1,            Z0,            Y0,            Y1,            Z1,            X2,            Z2,            X3^Y3,         X4^Y4,         X5^Y5,         Z3,            Y2,            X3,            Z4,            Y4,            },
    {0,             X0,            Z0,            Y0,            X1,            Z1,            Y1,            Z2,            X3^Y3,         X4^Y4,         Z3^X5^Y5,      Y2,            X2,            Z4,            Y3,            X4,            },
    {0,             0,             X0,            Y0,            X1,            Z0,            Y1,            Z1,            X3^Y3,         X4^Y4,         Z2^X5^Y5,      Y2,            X2,            Z3,            Y3,            X4,            },
    {0,             0,             0,             X0,            Y0,            Z0,            X1,            Z1,            X3^Y3,         X4^Y4,         Z2^X5^Y5,      Y1,            X2,            Z3,            Y2,            X3,            },
    {0,             0,             0,             0,             X0,            Z0,            Y0,            Z1,            X3^Y3,         X1^X4^Y4,      Z2^X5^Y5,      Y1,            X2,            Z3,            Y2,            X3,            },
    {X0,            X1,            Z0,            Y0,            Y1,            Z1,            X2,            Z2,            X3^Y3,         X4^Y4,         Y2^Y5^X6,      X5^Y6,         Z3,            Y3,            X4,            Z4,            },
    {0,             X0,            Z0,            Y0,            X1,            Z1,            Y1,            Z2,            X3^Y3,         X4^Y4,         Z3^Y5^X6,      Y2^X5^Y6,      X2,            Z4,            Y3,            X4,            },
    {0,             0,             X0,            Y0,            X1,            Z0,            Y1,            Z1,            X3^Y3,         X4^Y4,         Z2^Y5^X6,      Y2^X5^Y6,      X2,            Z3,            Y3,            X4,            },
    {0,             0,             0,             X0,            Y0,            Z0,            X1,            Z1,            X3^Y3,         X4^Y4,         Z2^Y5^X6,      Y1^X5^Y6,      X2,            Z3,            Y2,            X3,            },
    {0,             0,             0,             0,             X0,            Z0,            Y0,            Z1,            X3^Y3,         X1^X4^Y4,      Z2^Y5^X6,      Y1^X5^Y6,      X2,            Z3,            Y2,            X3,            },
    {X0,            X1,            Z0,            Y0,            Y1,            Z1,            X2,            Z2,            X3^Y3,         X4^Y4,         Y2^Y5^X7,      X5^Y7,         Z3^X6^Y6,      Y3,            X4,            Z4,            },
    {0,             X0,            Z0,            Y0,            X1,            Z1,            Y1,            Z2,            X3^Y3,         X4^Y4,         Z3^Y5^X7,      Y2^X5^Y7,      X2^X6^Y6,      Z4,            Y3,            X4,            },
    {0,             0,             X0,            Y0,            X1,            Z0,            Y1,            Z1,            X3^Y3,         X4^Y4,         Z2^Y5^X7,      Y2^X5^Y7,      X2^X6^Y6,      Z3,            Y3,            X4,            },
    {0,             0,             0,             X0,            Y0,            Z0,            X1,            Z1,            X3^Y3,         X4^Y4,         Z2^Y5^X7,      Y1^X5^Y7,      X2^X6^Y6,      Z3,            Y2,            X3,            },
    {0,             0,             0,             0,             X0,            Z0,            Y0,            Z1,            X3^Y3,         X1^X4^Y4,      Z2^Y5^X7,      Y1^X5^Y7,      X2^X6^Y6,      Z3,            Y2,            X3,            },
    {X0,            X1,            Z0,            Y0,            Y1,            Z1,            X2,            Z2,            X3^Y3,         X4^Y4,         Y2^Y5^X8,      X5^Y8,         Z3^Y6^X7,      Z4^X6^Y7,      Y3,            X4,            },
    {0,             X0,            Z0,            Y0,            X1,            Z1,            Y1,            Z2,            X3^Y3,         X4^Y4,         Z3^Y5^X8,      Y2^X5^Y8,      X2^Y6^X7,      Z4^X6^Y7,      Y3,            X4,            },
    {0,             0,             X0,            Y0,            X1,            Z0,            Y1,            Z1,            X3^Y3,         X4^Y4,         Z2^Y5^X8,      Y2^X5^Y8,      X2^Y6^X7,      Z3^X6^Y7,      Y3,            X4,            },
    {0,             0,             0,             X0,            Y0,            Z0,            X1,            Z1,            X3^Y3,         X4^Y4,         Z2^Y5^X8,      Y1^X5^Y8,      X2^Y6^X7,      Z3^X6^Y7,      Y2,            X3,            },
    {0,             0,             0,             0,             X0,            Z0,            Y0,            Z1,            X3^Y3,         X1^X4^Y4,      Z2^Y5^X8,      Y1^X5^Y8,      X2^Y6^X7,      Z3^X6^Y7,      Y2,            X3,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X5,         Z0^X4^Y5,      Y5,            X5,            0,             0,             0,             0,             },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3^X5,         Z0^X4^Y4,      Y4,            X5,            0,             0,             0,             0,             },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3^X4,         Z0^X3^Y4,      Y4,            X4,            0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2^X4,         Z0^X3^Y3,      Y3,            X4,            0,             0,             0,             0,             },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2^X3,         Z0^X2^Y3,      Y3,            X3,            0,             0,             0,             0,             },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X6,         X4^Y6,         Z0^X5^Y5,      X5,            0,             0,             0,             0,             },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3^X6,         X4^Y5,         Z0^Y4^X5,      X5,            0,             0,             0,             0,             },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3^X5,         X3^Y5,         Z0^X4^Y4,      X4,            0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2^X5,         X3^Y4,         Z0^Y3^X4,      X4,            0,             0,             0,             0,             },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2^X4,         X2^Y4,         Z0^X3^Y3,      X3,            0,             0,             0,             0,             },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X6,         Z1^X4^Y6,      Z0^X5^Y5,      X5,            0,             0,             0,             0,             },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3^X6,         Z1^X4^Y5,      Z0^Y4^X5,      X5,            0,             0,             0,             0,             },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3^X5,         Z1^X3^Y5,      Z0^X4^Y4,      X4,            0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2^X5,         Z1^X3^Y4,      Z0^Y3^X4,      X4,            0,             0,             0,             0,             },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2^X4,         Z1^X2^Y4,      Z0^X3^Y3,      X3,            0,             0,             0,             0,             },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X7,         X4^Y7,         Z1^Y5^X6,      Z0^X5^Y6,      0,             0,             0,             0,             },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3^X7,         X4^Y6,         Z1^Y4^X6,      Z0^X5^Y5,      0,             0,             0,             0,             },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3^X6,         X3^Y6,         Z1^Y4^X5,      Z0^X4^Y5,      0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2^X6,         X3^Y5,         Z1^Y3^X5,      Z0^X4^Y4,      0,             0,             0,             0,             },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2^X5,         X2^Y5,         Z1^Y3^X4,      Z0^X3^Y4,      0,             0,             0,             0,             },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X7,         Z2^X4^Y7,      Z1^Y5^X6,      Z0^X5^Y6,      0,             0,             0,             0,             },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3^X7,         Z2^X4^Y6,      Z1^Y4^X6,      Z0^X5^Y5,      0,             0,             0,             0,             },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3^X6,         Z2^X3^Y6,      Z1^Y4^X5,      Z0^X4^Y5,      0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2^X6,         Z2^X3^Y5,      Z1^Y3^X5,      Z0^X4^Y4,      0,             0,             0,             0,             },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2^X5,         X2^Z2^Y5,      Z1^Y3^X4,      Z0^X3^Y4,      0,             0,             0,             0,             },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X7,         X4^Y7,         Z2^Y5^X6,      Z1^X5^Y6,      0,             0,             0,             0,             },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3^X7,         X4^Y6,         Z2^Y4^X6,      Z1^X5^Y5,      0,             0,             0,             0,             },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3^X6,         X3^Y6,         Z2^Y4^X5,      Z1^X4^Y5,      0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2^X6,         X3^Y5,         Z2^Y3^X5,      Z1^X4^Y4,      0,             0,             0,             0,             },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2^X5,         X2^Y5,         Z2^Y3^X4,      Z1^X3^Y4,      0,             0,             0,             0,             },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X7,         Z3^X4^Y7,      Z2^Y5^X6,      Z1^X5^Y6,      0,             0,             0,             0,             },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3^X7,         Z3^X4^Y6,      Z2^Y4^X6,      Z1^X5^Y5,      0,             0,             0,             0,             },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3^X6,         X3^Z3^Y6,      Z2^Y4^X5,      Z1^X4^Y5,      0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2^X6,         X3^Z3^Y5,      Z2^Y3^X5,      Z1^X4^Y4,      0,             0,             0,             0,             },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2^X5,         X2^Z3^Y5,      Z2^Y3^X4,      Z1^X3^Y4,      0,             0,             0,             0,             },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X7,         X4^Y7,         Z3^Y5^X6,      Z2^X5^Y6,      0,             0,             0,             0,             },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3^X7,         X4^Y6,         Z3^Y4^X6,      Z2^X5^Y5,      0,             0,             0,             0,             },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3^X6,         X3^Y6,         Z3^Y4^X5,      Z2^X4^Y5,      0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2^X6,         X3^Y5,         Y3^Z3^X5,      Z2^X4^Y4,      0,             0,             0,             0,             },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2^X5,         X2^Y5,         Y3^Z3^X4,      Z2^X3^Y4,      0,             0,             0,             0,             },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X5,         Z0^X4^Y5,      Y5,            X5,            Y6,            X6,            Y7,            X7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3^X5,         Z0^X4^Y4,      Y4,            X5,            Y5,            X6,            Y6,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3^X4,         Z0^X3^Y4,      Y4,            X4,            Y5,            X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2^X4,         Z0^X3^Y3,      Y3,            X4,            Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2^X3,         Z0^X2^Y3,      Y3,            X3,            Y4,            X4,            Y5,            X5,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X6,         X4^Y6,         Z0^X5^Y5,      X5,            Y6,            X6,            Y7,            X7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3^X6,         X4^Y5,         Z0^Y4^X5,      X5,            Y5,            X6,            Y6,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3^X5,         X3^Y5,         Z0^X4^Y4,      X4,            Y5,            X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2^X5,         X3^Y4,         Z0^Y3^X4,      X4,            Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2^X4,         X2^Y4,         Z0^X3^Y3,      X3,            Y4,            X4,            Y5,            X5,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X6,         Z1^X4^Y6,      Z0^X5^Y5,      X5,            Y6,            X6,            Y7,            X7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3^X6,         Z1^X4^Y5,      Z0^Y4^X5,      X5,            Y5,            X6,            Y6,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3^X5,         Z1^X3^Y5,      Z0^X4^Y4,      X4,            Y5,            X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2^X5,         Z1^X3^Y4,      Z0^Y3^X4,      X4,            Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2^X4,         Z1^X2^Y4,      Z0^X3^Y3,      X3,            Y4,            X4,            Y5,            X5,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X7,         X4^Y7,         Z1^Y5^X6,      Z0^X5^Y6,      Y6,            X6,            Y7,            X7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3^X7,         X4^Y6,         Z1^Y4^X6,      Z0^X5^Y5,      Y5,            X6,            Y6,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3^X6,         X3^Y6,         Z1^Y4^X5,      Z0^X4^Y5,      Y5,            X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2^X6,         X3^Y5,         Z1^Y3^X5,      Z0^X4^Y4,      Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2^X5,         X2^Y5,         Z1^Y3^X4,      Z0^X3^Y4,      Y4,            X4,            Y5,            X5,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X7,         Z2^X4^Y7,      Z1^Y5^X6,      Z0^X5^Y6,      Y6,            X6,            Y7,            X7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3^X7,         Z2^X4^Y6,      Z1^Y4^X6,      Z0^X5^Y5,      Y5,            X6,            Y6,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3^X6,         Z2^X3^Y6,      Z1^Y4^X5,      Z0^X4^Y5,      Y5,            X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2^X6,         Z2^X3^Y5,      Z1^Y3^X5,      Z0^X4^Y4,      Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2^X5,         X2^Z2^Y5,      Z1^Y3^X4,      Z0^X3^Y4,      Y4,            X4,            Y5,            X5,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X8,         X4^Y8,         Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X6,            Y7,            X7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3^X8,         X4^Y7,         Z2^Y4^X7,      Z1^X5^Y6,      Z0^Y5^X6,      X6,            Y6,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3^X7,         X3^Y7,         Z2^Y4^X6,      Z1^X4^Y6,      Z0^X5^Y5,      X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2^X7,         X3^Y6,         Z2^Y3^X6,      Z1^X4^Y5,      Z0^Y4^X5,      X5,            Y5,            X6,            },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2^X6,         X2^Y6,         Z2^Y3^X5,      Z1^X3^Y5,      Z0^X4^Y4,      X4,            Y5,            X5,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X8,         Z3^X4^Y8,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X6,            Y7,            X7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3^X8,         Z3^X4^Y7,      Z2^Y4^X7,      Z1^X5^Y6,      Z0^Y5^X6,      X6,            Y6,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3^X7,         X3^Z3^Y7,      Z2^Y4^X6,      Z1^X4^Y6,      Z0^X5^Y5,      X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2^X7,         X3^Z3^Y6,      Z2^Y3^X6,      Z1^X4^Y5,      Z0^Y4^X5,      X5,            Y5,            X6,            },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2^X6,         X2^Z3^Y6,      Z2^Y3^X5,      Z1^X3^Y5,      Z0^X4^Y4,      X4,            Y5,            X5,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X9,         X4^Y9,         Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      Z0^X6^Y7,      Y7,            X7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3^X9,         X4^Y8,         Z3^Y4^X8,      Z2^X5^Y7,      Z1^Y5^X7,      Z0^X6^Y6,      Y6,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3^X8,         X3^Y8,         Z3^Y4^X7,      Z2^X4^Y7,      Z1^Y5^X6,      Z0^X5^Y6,      Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2^X8,         X3^Y7,         Y3^Z3^X7,      Z2^X4^Y6,      Z1^Y4^X6,      Z0^X5^Y5,      Y5,            X6,            },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2^X7,         X2^Y7,         Y3^Z3^X6,      Z2^X3^Y6,      Z1^Y4^X5,      Z0^X4^Y5,      Y5,            X5,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X9,         X4^Z4^Y9,      Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      Z0^X6^Y7,      Y7,            X7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3^X9,         X4^Z4^Y8,      Z3^Y4^X8,      Z2^X5^Y7,      Z1^Y5^X7,      Z0^X6^Y6,      Y6,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            Y2,            X2,            Y3^X8,         X3^Z4^Y8,      Z3^Y4^X7,      Z2^X4^Y7,      Z1^Y5^X6,      Z0^X5^Y6,      Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            Y1,            X1,            X2,            Y2^X8,         X3^Z4^Y7,      Y3^Z3^X7,      Z2^X4^Y6,      Z1^Y4^X6,      Z0^X5^Y5,      Y5,            X6,            },
    {0,             0,             0,             0,             Y0,            Y1,            X0,            X1,            Y2^X7,         X2^Z4^Y7,      Y3^Z3^X6,      Z2^X3^Y6,      Z1^Y4^X5,      Z0^X4^Y5,      Y5,            X5,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            0,             0,             0,             0,             0,             0,             0,             0,             },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y3,            X3,            Y4,            X4,            0,             0,             0,             0,             },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Z0^X3^Y3,      X3,            Y4,            X4,            0,             0,             0,             0,             },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4^X5,         Z0^X4^Y5,      Y5,            X5,            0,             0,             0,             0,             },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y3^X4,         Z0^X3^Y4,      Y4,            X4,            0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2^X4,         Z0^X3^Y3,      Y3,            X4,            0,             0,             0,             0,             },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2^X3,         Z0^X2^Y3,      Y3,            X3,            0,             0,             0,             0,             },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4^X6,         X4^Y6,         Z0^X5^Y5,      X5,            0,             0,             0,             0,             },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y3^X5,         X3^Y5,         Z0^X4^Y4,      X4,            0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2^X5,         X3^Y4,         Z0^Y3^X4,      X4,            0,             0,             0,             0,             },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2^X4,         X2^Y4,         Z0^X3^Y3,      X3,            0,             0,             0,             0,             },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Z1^Y3^X4,      Z0^X3^Y4,      Y4,            X4,            0,             0,             0,             0,             },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4^X6,         Z1^X4^Y6,      Z0^X5^Y5,      X5,            0,             0,             0,             0,             },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y3^X5,         Z1^X3^Y5,      Z0^X4^Y4,      X4,            0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2^X5,         Z1^X3^Y4,      Z0^Y3^X4,      X4,            0,             0,             0,             0,             },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2^X4,         Z1^X2^Y4,      Z0^X3^Y3,      X3,            0,             0,             0,             0,             },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4^X7,         X4^Y7,         Z1^Y5^X6,      Z0^X5^Y6,      0,             0,             0,             0,             },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y3^X6,         X3^Y6,         Z1^Y4^X5,      Z0^X4^Y5,      0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2^X6,         X3^Y5,         Z1^Y3^X5,      Z0^X4^Y4,      0,             0,             0,             0,             },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2^X5,         X2^Y5,         Z1^Y3^X4,      Z0^X3^Y4,      0,             0,             0,             0,             },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Z2^Y3^X5,      Z1^X3^Y5,      Z0^X4^Y4,      X4,            0,             0,             0,             0,             },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4^X7,         Z2^X4^Y7,      Z1^Y5^X6,      Z0^X5^Y6,      0,             0,             0,             0,             },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y3^X6,         Z2^X3^Y6,      Z1^Y4^X5,      Z0^X4^Y5,      0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2^X6,         Z2^X3^Y5,      Z1^Y3^X5,      Z0^X4^Y4,      0,             0,             0,             0,             },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2^X5,         X2^Z2^Y5,      Z1^Y3^X4,      Z0^X3^Y4,      0,             0,             0,             0,             },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4^X7,         X4^Y7,         Z2^Y5^X6,      Z1^X5^Y6,      0,             0,             0,             0,             },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y3^X6,         X3^Y6,         Z2^Y4^X5,      Z1^X4^Y5,      0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2^X6,         X3^Y5,         Z2^Y3^X5,      Z1^X4^Y4,      0,             0,             0,             0,             },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2^X5,         X2^Y5,         Z2^Y3^X4,      Z1^X3^Y4,      0,             0,             0,             0,             },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y3^Z3^X6,      Z2^X3^Y6,      Z1^Y4^X5,      Z0^X4^Y5,      0,             0,             0,             0,             },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4^X7,         Z3^X4^Y7,      Z2^Y5^X6,      Z1^X5^Y6,      0,             0,             0,             0,             },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y3^X6,         X3^Z3^Y6,      Z2^Y4^X5,      Z1^X4^Y5,      0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2^X6,         X3^Z3^Y5,      Z2^Y3^X5,      Z1^X4^Y4,      0,             0,             0,             0,             },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2^X5,         X2^Z3^Y5,      Z2^Y3^X4,      Z1^X3^Y4,      0,             0,             0,             0,             },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4^X7,         X4^Y7,         Z3^Y5^X6,      Z2^X5^Y6,      0,             0,             0,             0,             },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y3^X6,         X3^Y6,         Z3^Y4^X5,      Z2^X4^Y5,      0,             0,             0,             0,             },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2^X6,         X3^Y5,         Y3^Z3^X5,      Z2^X4^Y4,      0,             0,             0,             0,             },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2^X5,         X2^Y5,         Y3^Z3^X4,      Z2^X3^Y4,      0,             0,             0,             0,             },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y3,            X3,            Y4,            X4,            Y5,            X5,            Y6,            X6,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Z0^X3^Y3,      X3,            Y4,            X4,            Y5,            X5,            Y6,            X6,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4^X5,         Z0^X4^Y5,      Y5,            X5,            Y6,            X6,            Y7,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y3^X4,         Z0^X3^Y4,      Y4,            X4,            Y5,            X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2^X4,         Z0^X3^Y3,      Y3,            X4,            Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2^X3,         Z0^X2^Y3,      Y3,            X3,            Y4,            X4,            Y5,            X5,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4^X6,         X4^Y6,         Z0^X5^Y5,      X5,            Y6,            X6,            Y7,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y3^X5,         X3^Y5,         Z0^X4^Y4,      X4,            Y5,            X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2^X5,         X3^Y4,         Z0^Y3^X4,      X4,            Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2^X4,         X2^Y4,         Z0^X3^Y3,      X3,            Y4,            X4,            Y5,            X5,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Z1^Y3^X4,      Z0^X3^Y4,      Y4,            X4,            Y5,            X5,            Y6,            X6,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4^X6,         Z1^X4^Y6,      Z0^X5^Y5,      X5,            Y6,            X6,            Y7,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y3^X5,         Z1^X3^Y5,      Z0^X4^Y4,      X4,            Y5,            X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2^X5,         Z1^X3^Y4,      Z0^Y3^X4,      X4,            Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2^X4,         Z1^X2^Y4,      Z0^X3^Y3,      X3,            Y4,            X4,            Y5,            X5,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4^X7,         X4^Y7,         Z1^Y5^X6,      Z0^X5^Y6,      Y6,            X6,            Y7,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y3^X6,         X3^Y6,         Z1^Y4^X5,      Z0^X4^Y5,      Y5,            X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2^X6,         X3^Y5,         Z1^Y3^X5,      Z0^X4^Y4,      Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2^X5,         X2^Y5,         Z1^Y3^X4,      Z0^X3^Y4,      Y4,            X4,            Y5,            X5,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Z2^Y3^X5,      Z1^X3^Y5,      Z0^X4^Y4,      X4,            Y5,            X5,            Y6,            X6,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4^X7,         Z2^X4^Y7,      Z1^Y5^X6,      Z0^X5^Y6,      Y6,            X6,            Y7,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y3^X6,         Z2^X3^Y6,      Z1^Y4^X5,      Z0^X4^Y5,      Y5,            X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2^X6,         Z2^X3^Y5,      Z1^Y3^X5,      Z0^X4^Y4,      Y4,            X5,            Y5,            X6,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2^X5,         X2^Z2^Y5,      Z1^Y3^X4,      Z0^X3^Y4,      Y4,            X4,            Y5,            X5,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4^X8,         X4^Y8,         Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X6,            Y7,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y3^X7,         X3^Y7,         Z2^Y4^X6,      Z1^X4^Y6,      Z0^X5^Y5,      X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2^X7,         X3^Y6,         Z2^Y3^X6,      Z1^X4^Y5,      Z0^Y4^X5,      X5,            Y5,            X6,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2^X6,         X2^Y6,         Z2^Y3^X5,      Z1^X3^Y5,      Z0^X4^Y4,      X4,            Y5,            X5,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y3^Z3^X6,      Z2^X3^Y6,      Z1^Y4^X5,      Z0^X4^Y5,      Y5,            X5,            Y6,            X6,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4^X8,         Z3^X4^Y8,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X6,            Y7,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y3^X7,         X3^Z3^Y7,      Z2^Y4^X6,      Z1^X4^Y6,      Z0^X5^Y5,      X5,            Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2^X7,         X3^Z3^Y6,      Z2^Y3^X6,      Z1^X4^Y5,      Z0^Y4^X5,      X5,            Y5,            X6,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2^X6,         X2^Z3^Y6,      Z2^Y3^X5,      Z1^X3^Y5,      Z0^X4^Y4,      X4,            Y5,            X5,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4^X9,         X4^Y9,         Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      Z0^X6^Y7,      Y7,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y3^X8,         X3^Y8,         Z3^Y4^X7,      Z2^X4^Y7,      Z1^Y5^X6,      Z0^X5^Y6,      Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2^X8,         X3^Y7,         Y3^Z3^X7,      Z2^X4^Y6,      Z1^Y4^X6,      Z0^X5^Y5,      Y5,            X6,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2^X7,         X2^Y7,         Y3^Z3^X6,      Z2^X3^Y6,      Z1^Y4^X5,      Z0^X4^Y5,      Y5,            X5,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y3^Z4^X7,      X3^Z3^Y7,      Z2^Y4^X6,      Z1^X4^Y6,      Z0^X5^Y5,      X5,            Y6,            X6,            },
    {X0,            X1,            X2,            Y1,            Y0,            Y2,            X3,            Y3,            Y4^X9,         X4^Z4^Y9,      Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      Z0^X6^Y7,      Y7,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y3^X8,         X3^Z4^Y8,      Z3^Y4^X7,      Z2^X4^Y7,      Z1^Y5^X6,      Z0^X5^Y6,      Y6,            X6,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2^X8,         X3^Z4^Y7,      Y3^Z3^X7,      Z2^X4^Y6,      Z1^Y4^X6,      Z0^X5^Y5,      Y5,            X6,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y2^X7,         X2^Z4^Y7,      Y3^Z3^X6,      Z2^X3^Y6,      Z1^Y4^X5,      Z0^X4^Y5,      Y5,            X5,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            X3^Y3,         X3,            Y4,            X4,            Y5,            X5,            Y6,            X6,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y3^X4,         X3^Y4,         Y4,            X4,            Y5,            X5,            Y6,            X6,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y3^X5,         X3^Y5,         X4^Y4,         X4,            Y5,            X5,            Y6,            X6,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y3^X6,         X3^Y6,         Y4^X5,         X4^Y5,         Y5,            X5,            Y6,            X6,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y3,            X3,            Y4^X6,         X4^Y6,         X5^Y5,         X5,            Y6,            X6,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y3,            X3,            Y4,            X4,            Y5^X6,         X5^Y6,         Y6,            X6,            },
    {X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y3,            X4,            Y4,            X5^Y10,        Y5^X10,        X6^Y9,         Y6^X9,         X7^Y8,         Y7^X8,         },
    {0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y3,            X4,            Y4^X10,        X5^Y9,         Y5^X9,         X6^Y8,         Y6^X8,         X7^Y7,         },
    {0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y3,            X4^Y9,         Y4^X9,         X5^Y8,         Y5^X8,         X6^Y7,         Y6^X7,         },
    {0,             0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y3^X9,         X4^Y8,         Y4^X8,         X5^Y7,         Y5^X7,         X6^Y6,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3^Y8,         Y3^X8,         X4^Y7,         Y4^X7,         X5^Y6,         Y5^X6,         },
    {X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y3,            Z0^X4^Y4,      Y4,            X5,            Y5^X10,        X6^Y9,         Y6^X9,         X7^Y8,         Y7^X8,         },
    {0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Z0^X4^Y4,      Y3,            Y4,            X5^Y9,         X6^Y8,         Y5^X9,         X7^Y7,         Y6^X8,         },
    {0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            Z0^X4^Y4,      X3,            Y3,            Y4^X9,         X5^Y8,         Y5^X8,         X6^Y7,         Y6^X7,         },
    {0,             0,             0,             X0,            Y0,            X1,            Y1,            X2,            Z0^X4^Y4,      Y2,            X3,            Y3^X9,         X5^Y7,         Y4^X8,         X6^Y6,         Y5^X7,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z0^X4^Y4,      X2,            Y2,            Y3^X8,         X3^Y7,         Y4^X7,         X5^Y6,         Y5^X6,         },
    {X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y3,            Y4^X5^Y5,      Z0^X4^Y4,      X5,            Y5,            X6^Y9,         Y6^X9,         X7^Y8,         Y7^X8,         },
    {0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4^X5^Y5,      Z0^X4^Y4,      Y3,            X5,            X6^Y8,         Y5^X9,         X7^Y7,         Y6^X8,         },
    {0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X5^Y5,      Z0^X4^Y4,      X3,            Y3,            X5^Y8,         Y5^X8,         X6^Y7,         Y6^X7,         },
    {0,             0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y4^X5^Y5,      Z0^X4^Y4,      Y2,            X3,            Y3^X8,         X5^Y7,         X6^Y6,         Y5^X7,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X5^Y5,      Z0^X4^Y4,      X2,            Y2,            Y3^X7,         X3^Y7,         X5^Y6,         Y5^X6,         },
    {X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y3,            Y4^X5^Y5,      Z0^X4^Y4,      X5^Y5,         Y5,            X6,            Y6^X9,         X7^Y8,         Y7^X8,         },
    {0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4^X5^Y5,      Z0^X4^Y4,      X5^Y5,         Y3,            Y5,            X6^Y8,         X7^Y7,         Y6^X8,         },
    {0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X5^Y5,      Z0^X4^Y4,      X5^Y5,         X3,            Y3,            Y5^X8,         X6^Y7,         Y6^X7,         },
    {0,             0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y4^X5^Y5,      Z0^X4^Y4,      X5^Y5,         Y2,            X3,            Y3^X8,         X6^Y6,         Y5^X7,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X5^Y5,      Z0^X4^Y4,      X5^Y5,         X2,            Y2,            Y3^X7,         X3^Y6,         Y5^X6,         },
    {X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y3,            Y4^X6^Y6,      Z1^X4^Y4,      X5,            X6,            Y6^X9,         X7^Y8,         Y7^X8,         Z0^X5^Y5,      },
    {0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4^X6^Y6,      Z1^X4^Y4,      Y3,            X5,            X6^Y8,         Y6^X8,         X7^Y7,         Z0^X5^Y5,      },
    {0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X6^Y6,      Z1^X4^Y4,      X3,            Y3,            X5^Y8,         X6^Y7,         Y6^X7,         Z0^X5^Y5,      },
    {0,             0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y4^X6^Y6,      Z1^X4^Y4,      Y2,            X3,            Y3^X7,         X5^Y7,         X6^Y6,         Z0^X5^Y5,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X6^Y6,      Z1^X4^Y4,      X2,            Y2,            X3^Y7,         Y3^X6,         X5^Y6,         Z0^X5^Y5,      },
    {X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y3,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5,            X6,            Y6^X9,         X7^Y8,         Y7^X8,         },
    {0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      Y3,            X5,            X6^Y8,         Y6^X8,         X7^Y7,         },
    {0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            Y3,            X5^Y8,         X6^Y7,         Y6^X7,         },
    {0,             0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      Y2,            X3,            Y3^X7,         X5^Y7,         X6^Y6,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X2,            Y2,            X3^Y7,         Y3^X6,         X5^Y6,         },
    {X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y3,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^Y6,         X6,            Y6,            X7^Y8,         Y7^X8,         },
    {0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^Y6,         Y3,            X6,            Y6^X8,         X7^Y7,         },
    {0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^Y6,         X3,            Y3,            X6^Y7,         Y6^X7,         },
    {0,             0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^Y6,         Y2,            X3,            Y3^X7,         X6^Y6,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^Y6,         X2,            Y2,            X3^Y6,         Y3^X6,         },
    {X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y3,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X6,            Y6,            X7^Y8,         Y7^X8,         X5^Y6,         },
    {0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      Y3,            X6,            X7^Y7,         Y6^X8,         X5^Y6,         },
    {0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X3,            Y3,            X6^Y7,         Y6^X7,         X5^Y6,         },
    {0,             0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Y2,            X3,            Y3^X7,         X6^Y6,         Z0^X5^Y6,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      X2,            Y2,            Y3^X6,         X3^Y6,         Z0^X5^Y6,      },
    {X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y3,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         X6,            Y6,            X7^Y8,         Y7^X8,         },
    {0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         Y3,            X6,            X7^Y7,         Y6^X8,         },
    {0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         X3,            Y3,            X6^Y7,         Y6^X7,         },
    {0,             0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      Y2,            X3,            Y3^X7,         X6^Y6,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X2,            Y2,            Y3^X6,         X3^Y6,         },
    {X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y3,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         X6^Y6,         Y6,            X7,            Y7^X8,         },
    {0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         X6^Y6,         Y3,            Y6,            X7^Y7,         },
    {0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         X6^Y6,         X3,            Y3,            Y6^X7,         },
    {0,             0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X6^Y6,         Y2,            X3,            Y3^X7,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X3^Y6,         X2,            Y2,            Y3^X6,         },
    {X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y3,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X6,            X7,            Y7^X8,         X6^Y6,         },
    {0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         Y3,            X6,            X7^Y7,         X6^Y6,         },
    {0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X3,            Y3,            X6^Y7,         X6^Y6,         },
    {0,             0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Y2,            X3,            Y3^X7,         Z0^X6^Y6,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X8^Y8,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      X2,            X3,            Y3^X6,         Y2^X6^Y6,      },
    {X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y3,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X6^Y6,         X6,            X7,            Y7^X8,         },
    {0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X6^Y6,         Y3,            X6,            X7^Y7,         },
    {0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X6^Y6,         X3,            Y3,            X6^Y7,         },
    {0,             0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      Y2,            X3,            Y3^X7,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X8^Y8,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      Y2^X6^Y6,      X2,            X3,            Y3^X6,         },
    {X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y3,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X6^Y6,         X6^Y8,         X7,            Y7,            },
    {0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X6^Y6,         X6^Y8,         Y3,            X7,            },
    {0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X6^Y6,         X6^Y8,         X3,            Y3,            },
    {0,             0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X3^Y8,         Y2,            Y3,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X8^Y8,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      Y2^X6^Y6,      X3^Y8,         X2,            Y3,            },
    {X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y3,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      X5^Y8,         Y6^X7,         X7,            Y7,            X6^Y7,         },
    {0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      X5^Y8,         Y6^X7,         Y3,            X7,            X6^Y7,         },
    {0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      X5^Y8,         Y6^X7,         X3,            Y3,            X6^Y7,         },
    {0,             0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y4^X9^Y9,      Z3^X4^Y4,      Z2^Y5^X8,      Z1^X5^Y8,      Y2^Y6^X7,      X3,            Y3,            Z0^X6^Y7,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X9^Y9,      Z2^X4^Y4,      Z1^Y5^X8,      Z0^X5^Y8,      Y2^Y6^X7,      X3,            Y3,            X2^X6^Y7,      },
    {X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y3,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      X5^Y8,         Y6^X7,         X6^Y7,         X7,            Y7,            },
    {0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      X5^Y8,         Y6^X7,         X6^Y7,         Y3,            X7,            },
    {0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      X5^Y8,         Y6^X7,         X6^Y7,         X3,            Y3,            },
    {0,             0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y4^X9^Y9,      Z3^X4^Y4,      Z2^Y5^X8,      Z1^X5^Y8,      Y2^Y6^X7,      Z0^X6^Y7,      X3,            Y3,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X9^Y9,      Z2^X4^Y4,      Z1^Y5^X8,      Z0^X5^Y8,      Y2^Y6^X7,      X2^X6^Y7,      X3,            Y3,            },
    {S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y3,            X4,            Y4^X10,        Y5^X9,         X5^Y9,         Y6^X8,         X6^Y8,         X7^Y7,         },
    {0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y3,            X4^Y9,         Y4^X9,         X5^Y8,         Y5^X8,         X6^Y7,         Y6^X7,         },
    {0,             0,             S0,            X0,            Y0,            X1,            Y1,            X2,            X3,            Y3,            Y4^X9,         X4^Y9,         Y5^X8,         X5^Y8,         Y6^X7,         Y2^X6^Y7,      },
    {0,             0,             0,             S0,            X0,            Y0,            X1,            Y1,            Y2,            X3,            Y3^X9,         X4^Y8,         Y4^X8,         X5^Y7,         Y5^X7,         X2^X6^Y6,      },
    {0,             0,             0,             0,             S0,            X0,            Y0,            X1,            X2,            Y2,            Y3^X8,         X3^Y8,         Y4^X7,         X4^Y7,         Y5^X6,         Y1^X5^Y6,      },
    {S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Z0^X4^Y4,      Y3,            Y4,            Y5^X9,         X5^Y9,         Y6^X8,         X6^Y8,         X7^Y7,         },
    {0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Z0^X4^Y4,      X3,            Y3,            Y4^X9,         X5^Y8,         Y5^X8,         X6^Y7,         Y6^X7,         },
    {0,             0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Z0^X4^Y4,      X3,            Y3,            Y4^X9,         Y5^X8,         X5^Y8,         Y6^X7,         Y2^X6^Y7,      },
    {0,             0,             0,             S0,            X0,            Y0,            X1,            Y1,            Z0^X4^Y4,      X2,            X3,            Y3^X9,         Y4^X8,         X5^Y7,         Y5^X7,         Y2^X6^Y6,      },
    {0,             0,             0,             0,             S0,            X0,            Y0,            X1,            Z0^X4^Y4,      X2,            Y2,            Y3^X8,         Y4^X7,         X3^Y7,         Y5^X6,         Y1^X5^Y6,      },
    {S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4^X5^Y5,      Z0^X4^Y4,      Y3,            X5,            Y5^X9,         Y6^X8,         X6^Y8,         X7^Y7,         },
    {0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X5^Y5,      Z0^X4^Y4,      X3,            Y3,            X5^Y8,         Y5^X8,         X6^Y7,         Y6^X7,         },
    {0,             0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y4^X5^Y5,      Z0^X4^Y4,      X3,            Y3,            Y5^X8,         X5^Y8,         Y6^X7,         Y2^X6^Y7,      },
    {0,             0,             0,             S0,            X0,            Y0,            X1,            Y1,            Y4^X5^Y5,      Z0^X4^Y4,      X2,            X3,            Y3^X8,         X5^Y7,         Y5^X7,         Y2^X6^Y6,      },
    {0,             0,             0,             0,             S0,            X0,            Y0,            X1,            Y4^X5^Y5,      Z0^X4^Y4,      X2,            Y2,            Y3^X7,         X3^Y7,         Y5^X6,         Y1^X5^Y6,      },
    {S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4^X5^Y5,      Z0^X4^Y4,      X5^Y5,         Y3,            X5,            Y6^X8,         X6^Y8,         X7^Y7,         },
    {0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X5^Y5,      Z0^X4^Y4,      X5^Y5,         X3,            Y3,            X5^Y8,         X6^Y7,         Y6^X7,         },
    {0,             0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y4^X5^Y5,      Z0^X4^Y4,      X5^Y5,         X3,            Y3,            X5^Y8,         Y6^X7,         Y2^X6^Y7,      },
    {0,             0,             0,             S0,            X0,            Y0,            X1,            Y1,            Y4^X5^Y5,      Z0^X4^Y4,      X5^Y5,         X2,            X3,            Y3^X7,         X5^Y7,         Y2^X6^Y6,      },
    {0,             0,             0,             0,             S0,            X0,            Y0,            X1,            Y4^X5^Y5,      Z0^X4^Y4,      X5^Y5,         X2,            Y2,            Y3^X6,         X3^Y7,         Y1^X5^Y6,      },
    {S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4^X6^Y6,      Z1^X4^Y4,      Y3,            X5,            Y6^X8,         X6^Y8,         X7^Y7,         Z0^X5^Y5,      },
    {0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X6^Y6,      Z1^X4^Y4,      X3,            Y3,            X5^Y8,         X6^Y7,         Y6^X7,         Z0^X5^Y5,      },
    {0,             0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y4^X6^Y6,      Z1^X4^Y4,      X3,            Y3,            X5^Y8,         Y6^X7,         Y2^X6^Y7,      Z0^X5^Y5,      },
    {0,             0,             0,             S0,            X0,            Y0,            X1,            Y1,            Y4^X6^Y6,      Z1^X4^Y4,      Y2,            X3,            Y3^X7,         X5^Y7,         X2^X6^Y6,      Z0^X5^Y5,      },
    {0,             0,             0,             0,             S0,            X0,            Y0,            X1,            Y4^X6^Y6,      Z1^X4^Y4,      X2,            Y2,            Y3^X6,         X3^Y7,         Y1^X5^Y6,      Z0^X5^Y5,      },
    {S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      Y3,            X5,            Y6^X8,         X6^Y8,         X7^Y7,         },
    {0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            Y3,            X5^Y8,         X6^Y7,         Y6^X7,         },
    {0,             0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            Y3,            X5^Y8,         Y6^X7,         Y2^X6^Y7,      },
    {0,             0,             0,             S0,            X0,            Y0,            X1,            Y1,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      Y2,            X3,            Y3^X7,         X5^Y7,         X2^X6^Y6,      },
    {0,             0,             0,             0,             S0,            X0,            Y0,            X1,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X2,            Y2,            Y3^X6,         X3^Y7,         Y1^X5^Y6,      },
    {S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^Y6,         Y3,            X5,            X6^Y8,         X7^Y7,         },
    {0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^Y6,         X3,            Y3,            X5^Y8,         X6^Y7,         },
    {0,             0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^Y6,         X3,            Y3,            X5^Y8,         Y2^X6^Y7,      },
    {0,             0,             0,             S0,            X0,            Y0,            X1,            Y1,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X2^X5^Y6,      Y2,            X3,            Y3^X6,         X5^Y6,         },
    {0,             0,             0,             0,             S0,            X0,            Y0,            X1,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      Y1^X5^Y6,      X2,            Y2,            Y3^X5,         X3^Y6,         },
    {S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      Y3,            X6,            Y6^X8,         X7^Y7,         X5^Y6,         },
    {0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X3,            Y3,            X6^Y7,         Y6^X7,         X5^Y6,         },
    {0,             0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X3,            Y3,            Y6^X7,         Y2^X6^Y7,      X5^Y6,         },
    {0,             0,             0,             S0,            X0,            Y0,            X1,            Y1,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      X2,            X3,            Y3^X7,         Y2^X6^Y6,      Z0^X5^Y6,      },
    {0,             0,             0,             0,             S0,            X0,            Y0,            X1,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X2,            X3,            Y3^X7,         Y2^X6^Y6,      Y1^X5^Y6,      },
    {S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         Y3,            X6,            Y6^X8,         X7^Y7,         },
    {0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         X3,            Y3,            X6^Y7,         Y6^X7,         },
    {0,             0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         X3,            Y3,            Y6^X7,         Y2^X6^Y7,      },
    {0,             0,             0,             S0,            X0,            Y0,            X1,            Y1,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X2,            X3,            Y3^X7,         Y2^X6^Y6,      },
    {0,             0,             0,             0,             S0,            X0,            Y0,            X1,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      Y1^X5^Y6,      X2,            X3,            Y3^X7,         Y2^X6^Y6,      },
    {S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         X6^Y6,         Y3,            X6,            X7^Y7,         },
    {0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         X6^Y6,         X3,            Y3,            X6^Y7,         },
    {0,             0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         X6^Y6,         X3,            Y3,            Y2^X6^Y7,      },
    {0,             0,             0,             S0,            X0,            Y0,            X1,            Y1,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      Y2^X6^Y6,      X2,            X3,            Y3^X6,         },
    {0,             0,             0,             0,             S0,            X0,            Y0,            X1,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      Y1^X5^Y6,      Y2^X6^Y6,      X2,            X3,            Y3^X6,         },
    {S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         Y3,            X6,            X7^Y7,         X6^Y6,         },
    {0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X3,            Y3,            X6^Y7,         X6^Y6,         },
    {0,             0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X3,            Y3,            Y2^X6^Y7,      X6^Y6,         },
    {0,             0,             0,             S0,            X0,            Y0,            X1,            Y1,            Y4^X8^Y8,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      X3,            Y3,            X2^X6^Y7,      Y2^X6^Y6,      },
    {0,             0,             0,             0,             S0,            X0,            Y0,            X1,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      Y1^X5^Y7,      X3,            Y3,            X2^X6^Y7,      Y2^X6^Y6,      },
    {S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X6^Y6,         Y3,            X6,            X7^Y7,         },
    {0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X6^Y6,         X3,            Y3,            X6^Y7,         },
    {0,             0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X6^Y6,         X3,            Y3,            Y2^X6^Y7,      },
    {0,             0,             0,             S0,            X0,            Y0,            X1,            Y1,            Y4^X8^Y8,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      Y2^X6^Y6,      X3,            Y3,            X2^X6^Y7,      },
    {0,             0,             0,             0,             S0,            X0,            Y0,            X1,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      Y1^X5^Y7,      Y2^X6^Y6,      X3,            Y3,            X2^X6^Y7,      },
    {S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X6^Y6,         X6^Y8,         Y3,            Y7,            },
    {0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X6^Y6,         X6^Y8,         X3,            Y3,            },
    {0,             0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X6^Y6,         Y2^X6^Y8,      X3,            Y3,            },
    {0,             0,             0,             S0,            X0,            Y0,            X1,            Y1,            Y4^X8^Y8,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      Y2^X6^Y6,      X2^X6^Y8,      X3,            Y3,            },
    {0,             0,             0,             0,             S0,            X0,            Y0,            X1,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      Y1^X5^Y7,      Y2^X6^Y6,      X2^X6^Y8,      X3,            Y3,            },
    {S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      X5^Y8,         Y6^X7,         Y3,            Y7,            X6^Y7,         },
    {0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      X5^Y8,         Y6^X7,         X3,            Y3,            X6^Y7,         },
    {0,             0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      X5^Y8,         Y6^X7,         X3,            Y3,            Y2^X6^Y7,      },
    {0,             0,             0,             S0,            X0,            Y0,            X1,            Y1,            Y4^X9^Y9,      Z2^X4^Y4,      Z1^Y5^X8,      Z0^X5^Y8,      Y2^Y6^X7,      X3,            Y3,            X2^X6^Y7,      },
    {0,             0,             0,             0,             S0,            X0,            Y0,            X1,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      Y1^X5^Y8,      Y2^Y6^X7,      X3,            Y3,            X2^X6^Y7,      },
    {S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      X5^Y8,         Y6^X7,         X6^Y7,         Y3,            Y7,            },
    {0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      X5^Y8,         Y6^X7,         X6^Y7,         X3,            Y3,            },
    {0,             0,             S0,            X0,            Y0,            X1,            Y1,            X2,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      X5^Y8,         Y6^X7,         Y2^X6^Y7,      X3,            Y3,            },
    {0,             0,             0,             S0,            X0,            Y0,            X1,            Y1,            Y4^X9^Y9,      Z2^X4^Y4,      Z1^Y5^X8,      Z0^X5^Y8,      Y2^Y6^X7,      X2^X6^Y7,      X3,            Y3,            },
    {0,             0,             0,             0,             S0,            X0,            Y0,            X1,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      Y1^X5^Y8,      Y2^Y6^X7,      X2^X6^Y7,      X3,            Y3,            },
    {S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y2,            X3,            Y3,            X4^Y9,         Y4^X9,         X5^Y8,         Y5^X8,         X6^Y7,         Y6^X7,         },
    {0,             S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            X3,            Y3,            X4^Y9,         Y4^X9,         X5^Y8,         Y5^X8,         X6^Y7,         Y2^Y6^X7,      },
    {0,             0,             S0,            S1,            X0,            Y0,            X1,            Y1,            X3,            Y3,            X4^Y9,         Y4^X9,         X5^Y8,         Y5^X8,         X2^X6^Y7,      Y2^Y6^X7,      },
    {0,             0,             0,             S0,            S1,            X0,            Y0,            X1,            Y2,            X3,            Y3^X9,         X4^Y8,         Y4^X8,         X5^Y7,         X2^X6^Y6,      Y1^Y5^X7,      },
    {0,             0,             0,             0,             S0,            S1,            X0,            Y0,            X2,            Y2,            X3^Y8,         Y3^X8,         X4^Y7,         Y4^X7,         X1^X5^Y6,      Y1^Y5^X6,      },
    {S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Z0^X4^Y4,      X3,            Y3,            Y4^X9,         X5^Y8,         Y5^X8,         X6^Y7,         Y6^X7,         },
    {0,             S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Z0^X4^Y4,      X3,            Y3,            X5^Y8,         Y4^X9,         X6^Y7,         Y5^X8,         Y2^Y6^X7,      },
    {0,             0,             S0,            S1,            X0,            Y0,            X1,            Y1,            Z0^X4^Y4,      X3,            Y3,            Y4^X9,         X5^Y8,         Y5^X8,         X2^Y6^X7,      Y2^X6^Y7,      },
    {0,             0,             0,             S0,            S1,            X0,            Y0,            X1,            Z0^X4^Y4,      Y2,            X3,            Y3^X9,         X5^Y7,         Y4^X8,         X2^Y5^X7,      Y1^X6^Y6,      },
    {0,             0,             0,             0,             S0,            S1,            X0,            Y0,            Z0^X4^Y4,      X2,            Y2,            Y3^X8,         X3^Y7,         Y4^X7,         X1^Y5^X6,      Y1^X5^Y6,      },
    {S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X5^Y5,      Z0^X4^Y4,      X3,            Y3,            X5^Y8,         Y5^X8,         X6^Y7,         Y6^X7,         },
    {0,             S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y4^X5^Y5,      Z0^X4^Y4,      X3,            Y3,            X5^Y8,         X6^Y7,         Y5^X8,         Y2^Y6^X7,      },
    {0,             0,             S0,            S1,            X0,            Y0,            X1,            Y1,            Y4^X5^Y5,      Z0^X4^Y4,      X3,            Y3,            X5^Y8,         Y5^X8,         X2^Y6^X7,      Y2^X6^Y7,      },
    {0,             0,             0,             S0,            S1,            X0,            Y0,            X1,            Y4^X5^Y5,      Z0^X4^Y4,      Y2,            X3,            Y3^X8,         X5^Y7,         X2^Y5^X7,      Y1^X6^Y6,      },
    {0,             0,             0,             0,             S0,            S1,            X0,            Y0,            Y4^X5^Y5,      Z0^X4^Y4,      X2,            Y2,            Y3^X7,         X3^Y7,         X1^Y5^X6,      Y1^X5^Y6,      },
    {S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X5^Y5,      Z0^X4^Y4,      X5^Y5,         X3,            Y3,            Y5^X8,         X6^Y7,         Y6^X7,         },
    {0,             S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y4^X5^Y5,      Z0^X4^Y4,      X5^Y5,         X3,            Y3,            X6^Y7,         Y5^X8,         Y2^Y6^X7,      },
    {0,             0,             S0,            S1,            X0,            Y0,            X1,            Y1,            Y4^X5^Y5,      Z0^X4^Y4,      X5^Y5,         X3,            Y3,            Y5^X8,         X2^Y6^X7,      Y2^X6^Y7,      },
    {0,             0,             0,             S0,            S1,            X0,            Y0,            X1,            Y4^X5^Y5,      Z0^X4^Y4,      X5^Y5,         Y2,            X3,            Y3^X8,         X2^Y5^X7,      Y1^X6^Y6,      },
    {0,             0,             0,             0,             S0,            S1,            X0,            Y0,            Y4^X5^Y5,      Z0^X4^Y4,      Y1^X5^Y5,      X2,            Y2,            Y3^X6,         X3^Y6,         X1^X5^Y5,      },
    {S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X6^Y6,      Z1^X4^Y4,      X3,            Y3,            X5^Y8,         X6^Y7,         Y6^X7,         Z0^X5^Y5,      },
    {0,             S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y4^X6^Y6,      Z1^X4^Y4,      X3,            Y3,            X5^Y8,         X6^Y7,         Y2^Y6^X7,      Z0^X5^Y5,      },
    {0,             0,             S0,            S1,            X0,            Y0,            X1,            Y1,            Y4^X6^Y6,      Z1^X4^Y4,      X3,            Y3,            X5^Y8,         X2^X6^Y7,      Y2^Y6^X7,      Z0^X5^Y5,      },
    {0,             0,             0,             S0,            S1,            X0,            Y0,            X1,            Y4^X6^Y6,      Z1^X4^Y4,      X2,            X3,            Y3^X8,         Y2^Y5^X7,      Y1^X6^Y6,      Z0^X5^Y5,      },
    {0,             0,             0,             0,             S0,            S1,            X0,            Y0,            Y4^X6^Y6,      Z0^X4^Y4,      X2,            X3,            Y3^X7,         Y2^X6^Y6,      X1^X5^Y7,      Y1^X5^Y5,      },
    {S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            Y3,            X5^Y8,         X6^Y7,         Y6^X7,         },
    {0,             S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            Y3,            X5^Y8,         X6^Y7,         Y2^Y6^X7,      },
    {0,             0,             S0,            S1,            X0,            Y0,            X1,            Y1,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            Y3,            X5^Y8,         X2^X6^Y7,      Y2^Y6^X7,      },
    {0,             0,             0,             S0,            S1,            X0,            Y0,            X1,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X2,            X3,            Y3^X8,         Y2^Y5^X7,      Y1^X6^Y6,      },
    {0,             0,             0,             0,             S0,            S1,            X0,            Y0,            Y4^X6^Y6,      Z0^X4^Y4,      Y1^X5^Y5,      X2,            X3,            Y3^X7,         Y2^X6^Y6,      X1^X5^Y7,      },
    {S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^Y6,         X3,            Y3,            X6^Y7,         Y6^X7,         },
    {0,             S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^Y6,         X3,            Y3,            X6^Y7,         Y2^Y6^X7,      },
    {0,             0,             S0,            S1,            X0,            Y0,            X1,            Y1,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^Y6,         X3,            Y3,            X2^X6^Y7,      Y2^Y6^X7,      },
    {0,             0,             0,             S0,            S1,            X0,            Y0,            X1,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      Y1^X5^Y6,      X2,            X3,            Y3^X7,         Y2^Y5^X6,      },
    {0,             0,             0,             0,             S0,            S1,            X0,            Y0,            Y4^X6^Y6,      Z0^X4^Y4,      Y1^X5^Y5,      X1^X5^Y6,      X2,            X3,            Y3^X6,         Y2^X5^Y6,      },
    {S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X3,            Y3,            X6^Y7,         Y6^X7,         X5^Y6,         },
    {0,             S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X3,            Y3,            X6^Y7,         Y2^Y6^X7,      X5^Y6,         },
    {0,             0,             S0,            S1,            X0,            Y0,            X1,            Y1,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X3,            Y3,            X2^Y6^X7,      Y2^X6^Y7,      X5^Y6,         },
    {0,             0,             0,             S0,            S1,            X0,            Y0,            X1,            Y4^X7^Y7,      Z1^X4^Y4,      Y1^Y5^X6,      X3,            Y3,            X2^Y6^X7,      Y2^X6^Y7,      Z0^X5^Y6,      },
    {0,             0,             0,             0,             S0,            S1,            X0,            Y0,            Y4^X7^Y7,      Z0^X4^Y4,      Y1^Y5^X6,      X3,            Y3,            X2^Y6^X7,      Y2^X6^Y7,      X1^X5^Y6,      },
    {S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         X3,            Y3,            X6^Y7,         Y6^X7,         },
    {0,             S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         X3,            Y3,            X6^Y7,         Y2^Y6^X7,      },
    {0,             0,             S0,            S1,            X0,            Y0,            X1,            Y1,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         X3,            Y3,            X2^Y6^X7,      Y2^X6^Y7,      },
    {0,             0,             0,             S0,            S1,            X0,            Y0,            X1,            Y4^X7^Y7,      Z1^X4^Y4,      Y1^Y5^X6,      Z0^X5^Y6,      X3,            Y3,            X2^Y6^X7,      Y2^X6^Y7,      },
    {0,             0,             0,             0,             S0,            S1,            X0,            Y0,            Y4^X7^Y7,      Z0^X4^Y4,      Y1^Y5^X6,      X1^X5^Y6,      X3,            Y3,            X2^Y6^X7,      Y2^X6^Y7,      },
    {S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         X6^Y6,         X3,            Y3,            Y6^X7,         },
    {0,             S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         X6^Y6,         X3,            Y3,            Y2^Y6^X7,      },
    {0,             0,             S0,            S1,            X0,            Y0,            X1,            Y1,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         Y2^X6^Y6,      X3,            Y3,            X2^X6^Y6,      },
    {0,             0,             0,             S0,            S1,            X0,            Y0,            X1,            Y4^X7^Y7,      Z1^X4^Y4,      Y1^Y5^X6,      Z0^X5^Y6,      Y2^X6^Y6,      X3,            Y3,            X2^X6^Y6,      },
    {0,             0,             0,             0,             S0,            S1,            X0,            Y0,            Y4^X7^Y7,      Z0^X4^Y4,      Y1^Y5^X6,      X1^X5^Y6,      Y2^X6^Y6,      X3,            Y3,            X2^X6^Y6,      },
    {S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X3,            Y3,            X6^Y7,         X6^Y6,         },
    {0,             S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X3,            Y3,            Y2^Y6^X7,      X6^Y6,         },
    {0,             0,             S0,            S1,            X0,            Y0,            X1,            Y1,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X3,            Y3,            X2^X6^Y7,      Y2^X6^Y6,      },
    {0,             0,             0,             S0,            S1,            X0,            Y0,            X1,            Y4^X8^Y8,      Z1^X4^Y4,      Y1^Y5^X7,      Z0^X5^Y7,      X3,            Y3,            X2^X6^Y7,      Y2^X6^Y6,      },
    {0,             0,             0,             0,             S0,            S1,            X0,            Y0,            Y4^X8^Y8,      Z0^X4^Y4,      Y1^Y5^X7,      X1^X5^Y7,      X3,            Y3,            X2^X6^Y7,      Y2^X6^Y6,      },
    {S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X6^Y6,         X3,            Y3,            X6^Y7,         },
    {0,             S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X6^Y6,         X3,            Y3,            Y2^Y6^X7,      },
    {0,             0,             S0,            S1,            X0,            Y0,            X1,            Y1,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         Y2^X6^Y6,      X3,            Y3,            X2^X6^Y7,      },
    {0,             0,             0,             S0,            S1,            X0,            Y0,            X1,            Y4^X8^Y8,      Z1^X4^Y4,      Y1^Y5^X7,      Z0^X5^Y7,      Y2^X6^Y6,      X3,            Y3,            X2^X6^Y7,      },
    {0,             0,             0,             0,             S0,            S1,            X0,            Y0,            Y4^X8^Y8,      Z0^X4^Y4,      Y1^Y5^X7,      X1^X5^Y7,      Y2^X6^Y6,      X3,            Y3,            X2^X6^Y7,      },
    {S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X6^Y6,         X6^Y8,         X3,            Y3,            },
    {0,             S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X6^Y6,         Y2^X6^Y8,      X3,            Y3,            },
    {0,             0,             S0,            S1,            X0,            Y0,            X1,            Y1,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         Y2^X6^Y6,      X2^X6^Y8,      X3,            Y3,            },
    {0,             0,             0,             S0,            S1,            X0,            Y0,            X1,            Y4^X8^Y8,      Z1^X4^Y4,      Y1^Y5^X7,      Z0^X5^Y7,      Y2^X6^Y6,      X2^X6^Y8,      X3,            Y3,            },
    {0,             0,             0,             0,             S0,            S1,            X0,            Y0,            Y4^X8^Y8,      Z0^X4^Y4,      Y1^Y5^X7,      X1^X5^Y7,      Y2^X6^Y6,      X2^X6^Y8,      X3,            Y3,            },
    {S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      X5^Y8,         Y6^X7,         X3,            Y3,            X6^Y7,         },
    {0,             S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      X5^Y8,         Y2^Y6^X7,      X3,            Y3,            X6^Y7,         },
    {0,             0,             S0,            S1,            X0,            Y0,            X1,            Y1,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      X5^Y8,         Y2^Y6^X7,      X3,            Y3,            X2^X6^Y7,      },
    {0,             0,             0,             S0,            S1,            X0,            Y0,            X1,            Y4^X9^Y9,      Z1^X4^Y4,      Y1^Y5^X8,      Z0^X5^Y8,      Y2^Y6^X7,      X3,            Y3,            X2^X6^Y7,      },
    {0,             0,             0,             0,             S0,            S1,            X0,            Y0,            Y4^X9^Y9,      Z0^X4^Y4,      Y1^Y5^X8,      X1^X5^Y8,      Y2^Y6^X7,      X3,            Y3,            X2^X6^Y7,      },
    {S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y2,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      X5^Y8,         Y6^X7,         X6^Y7,         X3,            Y3,            },
    {0,             S0,            S1,            X0,            Y0,            X1,            Y1,            X2,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      X5^Y8,         Y2^Y6^X7,      X6^Y7,         X3,            Y3,            },
    {0,             0,             S0,            S1,            X0,            Y0,            X1,            Y1,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      X5^Y8,         Y2^Y6^X7,      X2^X6^Y7,      X3,            Y3,            },
    {0,             0,             0,             S0,            S1,            X0,            Y0,            X1,            Y4^X9^Y9,      Z1^X4^Y4,      Y1^Y5^X8,      Z0^X5^Y8,      Y2^Y6^X7,      X2^X6^Y7,      X3,            Y3,            },
    {0,             0,             0,             0,             S0,            S1,            X0,            Y0,            Y4^X9^Y9,      Z0^X4^Y4,      Y1^Y5^X8,      X1^X5^Y8,      Y2^Y6^X7,      X2^X6^Y7,      X3,            Y3,            },
    {S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            X2,            X3,            Y3,            Y4^X9,         X4^Y9,         Y5^X8,         X5^Y8,         Y6^X7,         Y2^X6^Y7,      },
    {0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            X3,            Y3,            X4^Y9,         Y4^X9,         X5^Y8,         Y5^X8,         X2^X6^Y7,      Y2^Y6^X7,      },
    {0,             0,             S0,            S1,            S2,            X0,            Y0,            X1,            X3,            Y3,            Y4^X9,         X4^Y9,         Y5^X8,         Y1^X5^Y8,      X2^X6^Y7,      Y2^Y6^X7,      },
    {0,             0,             0,             S0,            S1,            S2,            X0,            Y0,            Y2,            X3,            Y3^X9,         X4^Y8,         Y4^X8,         X1^X5^Y7,      Y1^Y5^X7,      X2^X6^Y6,      },
    {0,             0,             0,             0,             S0,            S1,            S2,            X0,            X2,            Y2,            Y3^X8,         X3^Y8,         Y4^X7,         Y0^X4^Y7,      X1^X5^Y6,      Y1^Y5^X6,      },
    {S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            X2,            Z0^X4^Y4,      X3,            Y3,            Y4^X9,         Y5^X8,         X5^Y8,         Y6^X7,         Y2^X6^Y7,      },
    {0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            Z0^X4^Y4,      X3,            Y3,            Y4^X9,         X5^Y8,         Y5^X8,         X2^Y6^X7,      Y2^X6^Y7,      },
    {0,             0,             S0,            S1,            S2,            X0,            Y0,            X1,            Z0^X4^Y4,      X3,            Y3,            Y4^X9,         Y5^X8,         Y1^X5^Y8,      X2^X6^Y7,      Y2^Y6^X7,      },
    {0,             0,             0,             S0,            S1,            S2,            X0,            Y0,            Z0^X4^Y4,      X2,            X3,            Y3^X9,         Y4^X8,         Y1^X5^Y7,      X1^Y5^X7,      Y2^X6^Y6,      },
    {0,             0,             0,             0,             S0,            S1,            S2,            X0,            Z0^X4^Y4,      X2,            Y2,            X3^Y7,         Y3^X7,         Y0^X4^Y6,      X1^Y4^X6,      Y1^X5^Y5,      },
    {S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            X2,            Y4^X5^Y5,      Z0^X4^Y4,      X3,            Y3,            Y5^X8,         X5^Y8,         Y6^X7,         Y2^X6^Y7,      },
    {0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            Y4^X5^Y5,      Z0^X4^Y4,      X3,            Y3,            X5^Y8,         Y5^X8,         X2^Y6^X7,      Y2^X6^Y7,      },
    {0,             0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y4^X5^Y5,      Z0^X4^Y4,      X3,            Y3,            Y5^X8,         Y1^X5^Y8,      X2^X6^Y7,      Y2^Y6^X7,      },
    {0,             0,             0,             S0,            S1,            S2,            X0,            Y0,            Y4^X5^Y5,      Z0^X4^Y4,      X2,            X3,            Y3^X8,         Y1^X5^Y7,      X1^Y5^X7,      Y2^X6^Y6,      },
    {0,             0,             0,             0,             S0,            S1,            S2,            X0,            Y4^X5^Y5,      Y0^X4^Y4,      X2,            X3,            Y3^X7,         Y1^X4^Y7,      Y2^X5^Y6,      X1^Y5^X6,      },
    {S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            X2,            Y4^X5^Y5,      Z0^X4^Y4,      X5^Y5,         X3,            Y3,            X5^Y8,         Y6^X7,         Y2^X6^Y7,      },
    {0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            Y4^X5^Y5,      Z0^X4^Y4,      X5^Y5,         X3,            Y3,            X5^Y8,         X2^Y6^X7,      Y2^X6^Y7,      },
    {0,             0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y4^X5^Y5,      Z0^X4^Y4,      X5^Y5,         X3,            Y3,            Y1^X5^Y8,      X2^X6^Y7,      Y2^Y6^X7,      },
    {0,             0,             0,             S0,            S1,            S2,            X0,            Y0,            Y4^X5^Y5,      Z0^X4^Y4,      Y2^X5^Y5,      X2,            X3,            Y3^X7,         Y1^X5^Y6,      X1^Y5^X6,      },
    {0,             0,             0,             0,             S0,            S1,            S2,            X0,            Y4^X5^Y5,      Y0^X4^Y4,      X1^X5^Y5,      X2,            X3,            Y3^X6,         Y1^X4^Y6,      Y2^X5^Y5,      },
    {S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            X2,            Y4^X6^Y6,      Z1^X4^Y4,      X3,            Y3,            X5^Y8,         Y6^X7,         Y2^X6^Y7,      Z0^X5^Y5,      },
    {0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            Y4^X6^Y6,      Z1^X4^Y4,      X3,            Y3,            X5^Y8,         X2^X6^Y7,      Y2^Y6^X7,      Z0^X5^Y5,      },
    {0,             0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y4^X6^Y6,      Z1^X4^Y4,      X3,            Y3,            Y1^X5^Y8,      X2^X6^Y7,      Y2^Y6^X7,      Z0^X5^Y5,      },
    {0,             0,             0,             S0,            S1,            S2,            X0,            Y0,            Y4^X6^Y6,      Z0^X4^Y4,      X3,            Y3,            X1^X5^Y8,      Y2^Y6^X7,      X2^X6^Y7,      Y1^X5^Y5,      },
    {0,             0,             0,             0,             S0,            S1,            S2,            X0,            Y4^X6^Y6,      Y0^X4^Y4,      X3,            Y3,            X1^X5^Y8,      X2^X6^Y7,      Y2^Y6^X7,      Y1^X5^Y5,      },
    {S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            X2,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            Y3,            X5^Y8,         Y6^X7,         Y2^X6^Y7,      },
    {0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            Y3,            X5^Y8,         X2^X6^Y7,      Y2^Y6^X7,      },
    {0,             0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            Y3,            Y1^X5^Y8,      X2^X6^Y7,      Y2^Y6^X7,      },
    {0,             0,             0,             S0,            S1,            S2,            X0,            Y0,            Y4^X6^Y6,      Z0^X4^Y4,      Y1^X5^Y5,      X3,            Y3,            X1^X5^Y8,      Y2^Y6^X7,      X2^X6^Y7,      },
    {0,             0,             0,             0,             S0,            S1,            S2,            X0,            Y4^X6^Y6,      Y0^X4^Y4,      Y1^X5^Y5,      X3,            Y3,            X1^X5^Y8,      X2^X6^Y7,      Y2^Y6^X7,      },
    {S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            X2,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^Y6,         X3,            Y3,            Y6^X7,         Y2^X6^Y7,      },
    {0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^Y6,         X3,            Y3,            X2^X6^Y7,      Y2^Y6^X7,      },
    {0,             0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      Y2^X5^Y6,      X3,            Y3,            Y1^X5^Y7,      X2^X6^Y6,      },
    {0,             0,             0,             S0,            S1,            S2,            X0,            Y0,            Y4^X6^Y6,      Z0^X4^Y4,      Y1^X5^Y5,      X2^X5^Y6,      X3,            Y3,            X1^X5^Y7,      Y2^X6^Y6,      },
    {0,             0,             0,             0,             S0,            S1,            S2,            X0,            Y4^X6^Y6,      Y0^X4^Y4,      Y1^X5^Y5,      Y2^X5^Y6,      X3,            Y3,            X1^X5^Y7,      X2^X6^Y6,      },
    {S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            X2,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X3,            Y3,            Y6^X7,         Y2^X6^Y7,      X5^Y6,         },
    {0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X3,            Y3,            X2^Y6^X7,      Y2^X6^Y7,      X5^Y6,         },
    {0,             0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X3,            Y3,            X2^Y6^X7,      Y1^X6^Y7,      Y2^X5^Y6,      },
    {0,             0,             0,             S0,            S1,            S2,            X0,            Y0,            Y4^X7^Y7,      Z0^X4^Y4,      Y1^Y5^X6,      X3,            Y3,            X2^Y6^X7,      Y2^X6^Y7,      X1^X5^Y6,      },
    {0,             0,             0,             0,             S0,            S1,            S2,            X0,            Y4^X7^Y7,      Y0^X4^Y4,      Y1^Y5^X6,      X3,            Y3,            X2^Y6^X7,      Y2^X6^Y7,      X1^X5^Y6,      },
    {S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            X2,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         X3,            Y3,            Y6^X7,         Y2^X6^Y7,      },
    {0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         X3,            Y3,            X2^Y6^X7,      Y2^X6^Y7,      },
    {0,             0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      Y2^X5^Y6,      X3,            Y3,            X2^Y6^X7,      Y1^X6^Y7,      },
    {0,             0,             0,             S0,            S1,            S2,            X0,            Y0,            Y4^X7^Y7,      Z0^X4^Y4,      Y1^Y5^X6,      X1^X5^Y6,      X3,            Y3,            X2^Y6^X7,      Y2^X6^Y7,      },
    {0,             0,             0,             0,             S0,            S1,            S2,            X0,            Y4^X7^Y7,      Y0^X4^Y4,      Y1^Y5^X6,      X1^X5^Y6,      X3,            Y3,            X2^Y6^X7,      Y2^X6^Y7,      },
    {S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            X2,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         X6^Y6,         X3,            Y3,            Y2^X6^Y7,      },
    {0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X5^Y6,         Y2^X6^Y6,      X3,            Y3,            X2^X6^Y6,      },
    {0,             0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      Y2^X5^Y6,      X2^X6^Y6,      X3,            Y3,            Y1^X6^Y6,      },
    {0,             0,             0,             S0,            S1,            S2,            X0,            Y0,            Y4^X7^Y7,      Z0^X4^Y4,      Y1^Y5^X6,      X1^X5^Y6,      Y2^X6^Y6,      X3,            Y3,            X2^X6^Y6,      },
    {0,             0,             0,             0,             S0,            S1,            S2,            X0,            Y4^X7^Y7,      Y0^X4^Y4,      Y1^Y5^X6,      X1^X5^Y6,      X2^X6^Y6,      X3,            Y3,            Y2^X6^Y6,      },
    {S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            X2,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X3,            Y3,            Y2^X6^Y7,      X6^Y6,         },
    {0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X3,            Y3,            X2^X6^Y7,      Y2^X6^Y6,      },
    {0,             0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      Y2^X5^Y7,      X3,            Y3,            Y1^X6^Y7,      X2^X6^Y6,      },
    {0,             0,             0,             S0,            S1,            S2,            X0,            Y0,            Y4^X8^Y8,      Z0^X4^Y4,      Y1^Y5^X7,      X1^X5^Y7,      X3,            Y3,            X2^X6^Y7,      Y2^X6^Y6,      },
    {0,             0,             0,             0,             S0,            S1,            S2,            X0,            Y4^X8^Y8,      Y0^X4^Y4,      Y1^Y5^X7,      X1^X5^Y7,      X3,            Y3,            X2^X6^Y7,      Y2^X6^Y6,      },
    {S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            X2,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X6^Y6,         X3,            Y3,            Y2^X6^Y7,      },
    {0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         Y2^X6^Y6,      X3,            Y3,            X2^X6^Y7,      },
    {0,             0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X2^X5^Y7,      Y2^X6^Y6,      X3,            Y3,            Y1^X6^Y7,      },
    {0,             0,             0,             S0,            S1,            S2,            X0,            Y0,            Y4^X8^Y8,      Z0^X4^Y4,      Y1^Y5^X7,      X1^X5^Y7,      Y2^X6^Y6,      X3,            Y3,            X2^X6^Y7,      },
    {0,             0,             0,             0,             S0,            S1,            S2,            X0,            Y4^X8^Y8,      Y0^X4^Y4,      Y1^Y5^X7,      X1^X5^Y7,      Y2^X6^Y6,      X3,            Y3,            X2^X6^Y7,      },
    {S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            X2,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         X6^Y6,         Y2^X6^Y8,      X3,            Y3,            },
    {0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X5^Y7,         Y2^X6^Y6,      X2^X6^Y8,      X3,            Y3,            },
    {0,             0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      X2^X5^Y7,      Y2^X6^Y6,      Y1^X6^Y8,      X3,            Y3,            },
    {0,             0,             0,             S0,            S1,            S2,            X0,            Y0,            Y4^X8^Y8,      Z0^X4^Y4,      Y1^Y5^X7,      X1^X5^Y7,      Y2^X6^Y6,      X2^X6^Y8,      X3,            Y3,            },
    {0,             0,             0,             0,             S0,            S1,            S2,            X0,            Y4^X8^Y8,      Y0^X4^Y4,      Y1^Y5^X7,      X1^X5^Y7,      Y2^X6^Y6,      X2^X6^Y8,      X3,            Y3,            },
    {S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            X2,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      X5^Y8,         Y6^X7,         X3,            Y3,            Y2^X6^Y7,      },
    {0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      X5^Y8,         Y2^Y6^X7,      X3,            Y3,            X2^X6^Y7,      },
    {0,             0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      X2^X5^Y8,      Y2^Y6^X7,      X3,            Y3,            Y1^X6^Y7,      },
    {0,             0,             0,             S0,            S1,            S2,            X0,            Y0,            Y4^X9^Y9,      Z0^X4^Y4,      Y1^Y5^X8,      X1^X5^Y8,      Y2^Y6^X7,      X3,            Y3,            X2^X6^Y7,      },
    {0,             0,             0,             0,             S0,            S1,            S2,            X0,            Y4^X9^Y9,      Y0^X4^Y4,      Y1^Y5^X8,      X1^X5^Y8,      Y2^Y6^X7,      X3,            Y3,            X2^X6^Y7,      },
    {S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            X2,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      X5^Y8,         Y6^X7,         Y2^X6^Y7,      X3,            Y3,            },
    {0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y1,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      X5^Y8,         Y2^Y6^X7,      X2^X6^Y7,      X3,            Y3,            },
    {0,             0,             S0,            S1,            S2,            X0,            Y0,            X1,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      Y1^X5^Y8,      Y2^Y6^X7,      X2^X6^Y7,      X3,            Y3,            },
    {0,             0,             0,             S0,            S1,            S2,            X0,            Y0,            Y4^X9^Y9,      Z0^X4^Y4,      Y1^Y5^X8,      X1^X5^Y8,      Y2^Y6^X7,      X2^X6^Y7,      X3,            Y3,            },
    {0,             0,             0,             0,             S0,            S1,            S2,            X0,            Y4^X9^Y9,      Y0^X4^Y4,      Y1^Y5^X8,      X1^X5^Y8,      Y2^Y6^X7,      X2^X6^Y7,      X3,            Y3,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            X4,            Y4,            X5^Y10,        Y5^X10,        X6^Y9,         Y6^X9,         X7^Y8,         Y7^X8,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3,            X4,            Y4^X10,        X5^Y9,         Y5^X9,         X6^Y8,         Y6^X8,         X7^Y7,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            X3,            Y3,            X4^Y9,         Y4^X9,         X5^Y8,         Y5^X8,         X6^Y7,         Y6^X7,         },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2,            X3,            Y3^X9,         X4^Y8,         Y4^X8,         X5^Y7,         Y5^X7,         X6^Y6,         },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Z0^X4^Y4,      Y4,            X5,            Y5^X10,        X6^Y9,         Y6^X9,         X7^Y8,         Y7^X8,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Z0^X4^Y4,      Y3,            Y4,            X5^Y9,         X6^Y8,         Y5^X9,         X7^Y7,         Y6^X8,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Z0^X4^Y4,      X3,            Y3,            Y4^X9,         X5^Y8,         Y5^X8,         X6^Y7,         Y6^X7,         },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Z0^X4^Y4,      Y2,            X3,            Y3^X9,         X5^Y7,         Y4^X8,         X6^Y6,         Y5^X7,         },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X5^Y5,      Z0^X4^Y4,      X5,            Y5,            X6^Y9,         Y6^X9,         X7^Y8,         Y7^X8,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X5^Y5,      Z0^X4^Y4,      Y3,            X5,            X6^Y8,         Y5^X9,         X7^Y7,         Y6^X8,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X5^Y5,      Z0^X4^Y4,      X3,            Y3,            X5^Y8,         Y5^X8,         X6^Y7,         Y6^X7,         },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X5^Y5,      Z0^X4^Y4,      Y2,            X3,            Y3^X8,         X5^Y7,         X6^Y6,         Y5^X7,         },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X5^Y5,      Z0^X4^Y4,      X5^X6^Y6,      Y5,            X6,            Y6^X9,         X7^Y8,         Y7^X8,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X5^Y5,      Z0^X4^Y4,      X5^X6^Y6,      Y3,            Y5,            X6^Y8,         X7^Y7,         Y6^X8,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X5^Y5,      Z0^X4^Y4,      X5^X6^Y6,      X3,            Y3,            Y5^X8,         X6^Y7,         Y6^X7,         },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X5^Y5,      Z0^X4^Y4,      X5^X6^Y6,      Y2,            X3,            Y3^X8,         X6^Y6,         Y5^X7,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X5^Y5,      Z0^X4^Y4,      X5^X6^Y6,      X2,            Y2,            Y3^X7,         X3^Y6,         Y5^X6,         },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X6^Y6,      Z1^X4^Y4,      X5,            X6,            Y6^X9,         X7^Y8,         Y7^X8,         Z0^X5^Y5,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X6^Y6,      Z1^X4^Y4,      Y3,            X5,            X6^Y8,         Y6^X8,         X7^Y7,         Z0^X5^Y5,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X6^Y6,      Z1^X4^Y4,      X3,            Y3,            X5^Y8,         X6^Y7,         Y6^X7,         Z0^X5^Y5,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X6^Y6,      Z1^X4^Y4,      Y2,            X3,            Y3^X7,         X5^Y7,         X6^Y6,         Z0^X5^Y5,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5,            X6,            Y6^X9,         X7^Y8,         Y7^X8,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      Y3,            X5,            X6^Y8,         Y6^X8,         X7^Y7,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            Y3,            X5^Y8,         X6^Y7,         Y6^X7,         },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      Y2,            X3,            Y3^X7,         X5^Y7,         X6^Y6,         },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^X7^Y7,      X6,            Y6,            X7^Y8,         Y7^X8,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^X7^Y7,      Y3,            X6,            Y6^X8,         X7^Y7,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^X7^Y7,      X3,            Y3,            X6^Y7,         Y6^X7,         },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^X7^Y7,      Y2,            X3,            Y3^X7,         X6^Y6,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^X7^Y7,      X2,            Y2,            X3^Y6,         Y3^X6,         },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      X6,            Y6,            X7^Y8,         Y7^X8,         Z0^X5^Y6,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Y3,            X6,            X7^Y7,         Y6^X8,         Z0^X5^Y6,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      X3,            Y3,            X6^Y7,         Y6^X7,         Z0^X5^Y6,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Y2,            X3,            Y3^X7,         X6^Y6,         Z0^X5^Y6,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X6,            Y6,            X7^Y8,         Y7^X8,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      Y3,            X6,            X7^Y7,         Y6^X8,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X3,            Y3,            X6^Y7,         Y6^X7,         },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      Y2,            X3,            Y3^X7,         X6^Y6,         },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X6^X8^Y8,      Y6,            X7,            Y7^X8,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X6^X8^Y8,      Y3,            Y6,            X7^Y7,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X6^X8^Y8,      X3,            Y3,            Y6^X7,         },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X6^X8^Y8,      Y2,            X3,            Y3^X7,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X3^X8^Y8,      X2,            Y2,            Y3^X6,         },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      X6,            X7,            Y7^X8,         Z0^X6^Y6,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Y3,            X6,            X7^Y7,         Z0^X6^Y6,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      X3,            Y3,            X6^Y7,         Z0^X6^Y6,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Y2,            X3,            Y3^X7,         Z0^X6^Y6,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X6,            X7,            Y7^X8,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      Y3,            X6,            X7^Y7,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X3,            Y3,            X6^Y7,         },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      Y2,            X3,            Y3^X7,         },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X6^X9^Y9,      X7,            Y7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X6^X9^Y9,      Y3,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X6^X9^Y9,      X3,            Y3,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X3^X9^Y9,      Y2,            Y3,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X8^Y8,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      Y2^X6^Y6,      X3^X9^Y9,      X2,            Y3,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X9^Y9,      X4^Y4^Z4,      Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      X7,            Y7,            Z0^X6^Y7,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X9^Y9,      X4^Y4^Z4,      Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      Y3,            X7,            Z0^X6^Y7,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X9^Y9,      X4^Y4^Z4,      Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      X3,            Y3,            Z0^X6^Y7,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X9^Y9,      Z3^X4^Y4,      Z2^Y5^X8,      Z1^X5^Y8,      Y2^Y6^X7,      X3,            Y3,            Z0^X6^Y7,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X9^Y9,      X4^Y4^Z4,      Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      Z0^X6^Y7,      X7,            Y7,            },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X9^Y9,      X4^Y4^Z4,      Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      Z0^X6^Y7,      Y3,            X7,            },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X9^Y9,      X4^Y4^Z4,      Z3^Y5^X8,      Z2^X5^Y8,      Z1^Y6^X7,      Z0^X6^Y7,      X3,            Y3,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X9^Y9,      Z3^X4^Y4,      Z2^Y5^X8,      Z1^X5^Y8,      Y2^Y6^X7,      Z0^X6^Y7,      X3,            Y3,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            X4,            Y4,            Y5^X10,        X5^Y10,        Y6^X9,         X6^Y9,         Y7^X8,         S0^X7^Y8,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3,            X4,            Y4^X10,        X5^Y9,         Y5^X9,         X6^Y8,         Y6^X8,         S0^X7^Y7,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            X3,            Y3,            Y4^X9,         X4^Y9,         Y5^X8,         X5^Y8,         Y6^X7,         S0^X6^Y7,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2,            X3,            Y3^X9,         X4^Y8,         Y4^X8,         X5^Y7,         Y5^X7,         S0^X6^Y6,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            Y3^X8,         X3^Y8,         Y4^X7,         X4^Y7,         Y5^X6,         S0^X5^Y6,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Z0^X4^Y4,      Y4,            X5,            Y5^X10,        Y6^X9,         X6^Y9,         Y7^X8,         S0^X7^Y8,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Z0^X4^Y4,      Y3,            Y4,            X5^Y9,         Y5^X9,         X6^Y8,         Y6^X8,         S0^X7^Y7,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Z0^X4^Y4,      X3,            Y3,            Y4^X9,         Y5^X8,         X5^Y8,         Y6^X7,         S0^X6^Y7,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Z0^X4^Y4,      Y2,            X3,            Y3^X9,         Y4^X8,         X5^Y7,         Y5^X7,         S0^X6^Y6,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z0^X4^Y4,      X2,            Y2,            Y3^X8,         Y4^X7,         X3^Y7,         Y5^X6,         S0^X5^Y6,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X5^Y5,      Z0^X4^Y4,      X5,            Y5,            Y6^X9,         X6^Y9,         Y7^X8,         S0^X7^Y8,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X5^Y5,      Z0^X4^Y4,      Y3,            X5,            Y5^X9,         X6^Y8,         Y6^X8,         S0^X7^Y7,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X5^Y5,      Z0^X4^Y4,      X3,            Y3,            Y5^X8,         X5^Y8,         Y6^X7,         S0^X6^Y7,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X5^Y5,      Z0^X4^Y4,      Y2,            X3,            Y3^X8,         X5^Y7,         Y5^X7,         S0^X6^Y6,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X5^Y5,      Z0^X4^Y4,      X2,            Y2,            Y3^X7,         X3^Y7,         Y5^X6,         S0^X5^Y6,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X5^Y5,      Z0^X4^Y4,      X5^X6^Y6,      Y5,            X6,            Y6^X9,         Y7^X8,         S0^X7^Y8,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X5^Y5,      Z0^X4^Y4,      X5^X6^Y6,      Y3,            Y5,            X6^Y8,         Y6^X8,         S0^X7^Y7,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X5^Y5,      Z0^X4^Y4,      X5^X6^Y6,      X3,            Y3,            Y5^X8,         Y6^X7,         S0^X6^Y7,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X5^Y5,      Z0^X4^Y4,      X5^X6^Y6,      Y2,            X3,            Y3^X8,         Y5^X7,         S0^X6^Y6,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X5^Y5,      Z0^X4^Y4,      S0^X6^Y6,      X2,            Y2,            Y3^X6,         X3^Y6,         X5^Y5,         },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X6^Y6,      Z1^X4^Y4,      X5,            X6,            Y6^X9,         Y7^X8,         S0^X7^Y8,      Z0^X5^Y5,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X6^Y6,      Z1^X4^Y4,      Y3,            X5,            X6^Y8,         Y6^X8,         S0^X7^Y7,      Z0^X5^Y5,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X6^Y6,      Z1^X4^Y4,      X3,            Y3,            X5^Y8,         Y6^X7,         S0^X6^Y7,      Z0^X5^Y5,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X6^Y6,      Z1^X4^Y4,      Y2,            X3,            Y3^X7,         X5^Y7,         S0^X6^Y6,      Z0^X5^Y5,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X6^Y6,      Z1^X4^Y4,      X2,            Y2,            Y3^X6,         X3^Y7,         S0^X5^Y6,      Z0^X5^Y5,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5,            X6,            Y6^X9,         Y7^X8,         S0^X7^Y8,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      Y3,            X5,            X6^Y8,         Y6^X8,         S0^X7^Y7,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            Y3,            X5^Y8,         Y6^X7,         S0^X6^Y7,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      Y2,            X3,            Y3^X7,         X5^Y7,         S0^X6^Y6,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X2,            Y2,            Y3^X6,         X3^Y7,         S0^X5^Y6,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^X7^Y7,      X6,            Y6,            Y7^X8,         S0^X7^Y8,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^X7^Y7,      Y3,            X6,            Y6^X8,         S0^X7^Y7,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^X7^Y7,      X3,            Y3,            Y6^X7,         S0^X6^Y7,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^X7^Y7,      Y2,            X3,            Y3^X7,         S0^X6^Y6,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      S0^X7^Y7,      X2,            Y2,            Y3^X5,         X3^Y6,         },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      X6,            Y6,            Y7^X8,         S0^X7^Y8,      Z0^X5^Y6,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Y3,            X6,            Y6^X8,         S0^X7^Y7,      Z0^X5^Y6,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      X3,            Y3,            Y6^X7,         S0^X6^Y7,      Z0^X5^Y6,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Y2,            X3,            Y3^X7,         S0^X6^Y6,      Z0^X5^Y6,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X2,            Y2,            Y3^X6,         X3^Y6,         S0^X5^Y6,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X6,            Y6,            Y7^X8,         S0^X7^Y8,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      Y3,            X6,            Y6^X8,         S0^X7^Y7,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X3,            Y3,            Y6^X7,         S0^X6^Y7,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      Y2,            X3,            Y3^X7,         S0^X6^Y6,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      S0^X5^Y6,      X2,            Y2,            Y3^X6,         X3^Y6,         },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X6^X8^Y8,      Y6,            Y7,            S0^X7^Y8,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X6^X8^Y8,      Y3,            Y6,            S0^X7^Y7,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      S0^X8^Y8,      X3,            Y3,            X6^Y6,         },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      S0^X8^Y8,      Y2,            X3,            Y3^X6,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      S0^X5^Y6,      X3^X8^Y8,      X2,            Y2,            Y3^X6,         },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      X6,            Y7,            S0^X7^Y8,      Z0^X6^Y6,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Y3,            X6,            S0^X7^Y7,      Z0^X6^Y6,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      X3,            Y3,            S0^X6^Y7,      Z0^X6^Y6,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X8^Y8,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      Y2,            X3,            Y3^X6,         S0^X6^Y6,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      S0^X5^Y7,      X2,            X3,            Y3^X6,         Y2^X6^Y6,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X6,            Y7,            S0^X7^Y8,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      Y3,            X6,            S0^X7^Y7,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X3,            Y3,            S0^X6^Y7,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X8^Y8,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      S0^X6^Y6,      Y2,            X3,            Y3^X6,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      S0^X5^Y7,      Y2^X6^Y6,      X2,            X3,            Y3^X6,         },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X6^X9^Y9,      Y7,            S0^X7,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      X6^X9^Y9,      Y3,            S0^X7,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X8^Y8,      Z3^X4^Y4,      Z2^Y5^X7,      Z1^X5^Y7,      Z0^X6^Y6,      S0^X9^Y9,      X3,            Y3,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X8^Y8,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      S0^X6^Y6,      X3^X9^Y9,      Y2,            Y3,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      S0^X5^Y7,      Y2^X6^Y6,      X3^X9^Y9,      X2,            Y3,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X9^Y9,      Z3^X4^Y4,      Z2^Y5^X8,      Z1^X5^Y8,      Z0^Y6^X7,      Y7,            S0^X7,         Z4^X6^Y7,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X9^Y9,      Z3^X4^Y4,      Z2^Y5^X8,      Z1^X5^Y8,      Z0^Y6^X7,      Y3,            S0^X7,         Z4^X6^Y7,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X9^Y9,      Z3^X4^Y4,      Z2^Y5^X8,      Z1^X5^Y8,      Z0^Y6^X7,      X3,            Y3,            S0^X6^Y7,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X9^Y9,      Z2^X4^Y4,      Z1^Y5^X8,      Z0^X5^Y8,      S0^Y6^X7,      X3,            Y3,            Y2^X6^Y7,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      S0^X5^Y8,      Y2^Y6^X7,      X3,            Y3,            X2^X6^Y7,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X9^Y9,      Z3^X4^Y4,      Z2^Y5^X8,      Z1^X5^Y8,      Z0^Y6^X7,      Z4^X6^Y7,      Y7,            S0^X7,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X9^Y9,      Z3^X4^Y4,      Z2^Y5^X8,      Z1^X5^Y8,      Z0^Y6^X7,      Z4^X6^Y7,      Y3,            S0^X7,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X9^Y9,      Z3^X4^Y4,      Z2^Y5^X8,      Z1^X5^Y8,      Z0^Y6^X7,      S0^X6^Y7,      X3,            Y3,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X9^Y9,      Z2^X4^Y4,      Z1^Y5^X8,      Z0^X5^Y8,      S0^Y6^X7,      Y2^X6^Y7,      X3,            Y3,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      S0^X5^Y8,      Y2^Y6^X7,      X2^X6^Y7,      X3,            Y3,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            X4,            Y4,            X5^Y10,        Y5^X10,        X6^Y9,         Y6^X9,         S0^X7^Y8,      S1^Y7^X8,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3,            X4,            Y4^X10,        X5^Y9,         Y5^X9,         X6^Y8,         S0^Y6^X8,      S1^X7^Y7,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            X3,            Y3,            X4^Y9,         Y4^X9,         X5^Y8,         Y5^X8,         S0^X6^Y7,      S1^Y6^X7,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2,            X3,            Y3^X9,         X4^Y8,         Y4^X8,         X5^Y7,         S0^Y5^X7,      S1^X6^Y6,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            X3^Y8,         Y3^X8,         X4^Y7,         Y4^X7,         S0^X5^Y6,      S1^Y5^X6,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Z0^X4^Y4,      Y4,            X5,            Y5^X10,        X6^Y9,         Y6^X9,         S0^X7^Y8,      S1^Y7^X8,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Z0^X4^Y4,      Y3,            Y4,            X5^Y9,         X6^Y8,         Y5^X9,         S0^X7^Y7,      S1^Y6^X8,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Z0^X4^Y4,      X3,            Y3,            Y4^X9,         X5^Y8,         Y5^X8,         S0^X6^Y7,      S1^Y6^X7,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Z0^X4^Y4,      Y2,            X3,            Y3^X9,         X5^Y7,         Y4^X8,         S0^X6^Y6,      S1^Y5^X7,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z0^X4^Y4,      X2,            Y2,            Y3^X8,         X3^Y7,         Y4^X7,         S0^X5^Y6,      S1^Y5^X6,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X5^Y5,      Z0^X4^Y4,      X5,            Y5,            X6^Y9,         Y6^X9,         S0^X7^Y8,      S1^Y7^X8,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X5^Y5,      Z0^X4^Y4,      Y3,            X5,            X6^Y8,         Y5^X9,         S0^X7^Y7,      S1^Y6^X8,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X5^Y5,      Z0^X4^Y4,      X3,            Y3,            X5^Y8,         Y5^X8,         S0^X6^Y7,      S1^Y6^X7,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X5^Y5,      Z0^X4^Y4,      Y2,            X3,            Y3^X8,         X5^Y7,         S0^X6^Y6,      S1^Y5^X7,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X5^Y5,      Z0^X4^Y4,      X2,            Y2,            Y3^X7,         X3^Y7,         S0^X5^Y6,      S1^Y5^X6,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X5^Y5,      Z0^X4^Y4,      X5^X6^Y6,      Y5,            X6,            Y6^X9,         S0^X7^Y8,      S1^Y7^X8,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X5^Y5,      Z0^X4^Y4,      X5^X6^Y6,      Y3,            Y5,            X6^Y8,         S0^X7^Y7,      S1^Y6^X8,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X5^Y5,      Z0^X4^Y4,      X5^X6^Y6,      X3,            Y3,            Y5^X8,         S0^X6^Y7,      S1^Y6^X7,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X5^Y5,      Z0^X4^Y4,      X5^X6^Y6,      Y2,            X3,            Y3^X8,         S0^X6^Y6,      S1^Y5^X7,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X5^Y5,      Z0^X4^Y4,      S1^X6^Y6,      X2,            Y2,            Y3^X6,         X3^Y6,         S0^X5^Y5,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X6^Y6,      Z1^X4^Y4,      X5,            X6,            Y6^X9,         S0^X7^Y8,      S1^Y7^X8,      Z0^X5^Y5,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X6^Y6,      Z1^X4^Y4,      Y3,            X5,            X6^Y8,         S0^Y6^X8,      S1^X7^Y7,      Z0^X5^Y5,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X6^Y6,      Z1^X4^Y4,      X3,            Y3,            X5^Y8,         S0^X6^Y7,      S1^Y6^X7,      Z0^X5^Y5,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X6^Y6,      Z1^X4^Y4,      Y2,            X3,            Y3^X8,         S0^X6^Y6,      S1^Y5^X7,      Z0^X5^Y5,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X6^Y6,      Z0^X4^Y4,      X2,            Y2,            Y3^X6,         X3^Y7,         S0^X5^Y6,      S1^X5^Y5,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5,            X6,            Y6^X9,         S0^X7^Y8,      S1^Y7^X8,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      Y3,            X5,            X6^Y8,         S0^Y6^X8,      S1^X7^Y7,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            Y3,            X5^Y8,         S0^X6^Y7,      S1^Y6^X7,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      Y2,            X3,            Y3^X8,         S0^X6^Y6,      S1^Y5^X7,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X6^Y6,      Z0^X4^Y4,      S1^X5^Y5,      X2,            Y2,            Y3^X6,         X3^Y7,         S0^X5^Y6,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^X7^Y7,      X6,            Y6,            S0^X7^Y8,      S1^Y7^X8,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^X7^Y7,      Y3,            X6,            S0^Y6^X8,      S1^X7^Y7,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^X7^Y7,      X3,            Y3,            S0^X6^Y7,      S1^Y6^X7,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      S1^X7^Y7,      Y2,            X3,            Y3^X7,         S0^Y5^X6,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X6^Y6,      Z0^X4^Y4,      S1^X5^Y5,      S0^X7^Y7,      X2,            Y2,            Y3^X5,         X3^Y6,         },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      X6,            Y6,            S0^X7^Y8,      S1^Y7^X8,      Z0^X5^Y6,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Y3,            X6,            S0^X7^Y7,      S1^Y6^X8,      Z0^X5^Y6,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      X3,            Y3,            S0^X6^Y7,      S1^Y6^X7,      Z0^X5^Y6,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X7^Y7,      Z1^X4^Y4,      S1^Y5^X6,      Y2,            X3,            Y3^X7,         S0^X6^Y6,      Z0^X5^Y6,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X7^Y7,      Z0^X4^Y4,      S1^Y5^X6,      X2,            Y2,            Y3^X6,         X3^Y6,         S0^X5^Y6,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X6,            Y6,            S0^X7^Y8,      S1^Y7^X8,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      Y3,            X6,            S0^X7^Y7,      S1^Y6^X8,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X3,            Y3,            S0^X6^Y7,      S1^Y6^X7,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X7^Y7,      Z1^X4^Y4,      S1^Y5^X6,      Z0^X5^Y6,      Y2,            X3,            Y3^X7,         S0^X6^Y6,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X7^Y7,      Z0^X4^Y4,      S1^Y5^X6,      S0^X5^Y6,      X2,            Y2,            Y3^X6,         X3^Y6,         },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X6^X8^Y8,      Y6,            S0^X7,         S1^Y7^X8,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      X6^X8^Y8,      Y3,            S0^X7,         S1^Y6^X8,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X7^Y7,      Z2^X4^Y4,      Z1^Y5^X6,      Z0^X5^Y6,      S1^X8^Y8,      X3,            Y3,            S0^X6^Y6,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X7^Y7,      Z1^X4^Y4,      S1^Y5^X6,      Z0^X5^Y6,      S0^X8^Y8,      Y2,            X3,            Y3^X6,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X7^Y7,      Z0^X4^Y4,      S1^Y5^X6,      S0^X5^Y6,      X3^X8^Y8,      X2,            Y2,            Y3^X6,         },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X8^Y8,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      X6,            S0^X7,         S1^Y7^X8,      Z3^X6^Y6,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X8^Y8,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      Y3,            S0^X7,         S1^Y6^X8,      Z3^X6^Y6,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X8^Y8,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      X3,            Y3,            S0^X6^Y7,      S1^X6^Y6,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X8^Y8,      Z1^X4^Y4,      S1^Y5^X7,      Z0^X5^Y7,      Y2,            X3,            Y3^X6,         S0^X6^Y6,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X8^Y8,      Z0^X4^Y4,      S1^Y5^X7,      S0^X5^Y7,      X2,            X3,            Y3^X6,         Y2^X6^Y6,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X8^Y8,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      Z3^X6^Y6,      X6,            S0^X7,         S1^Y7^X8,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X8^Y8,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      Z3^X6^Y6,      Y3,            S0^X7,         S1^Y6^X8,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X8^Y8,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      S1^X6^Y6,      X3,            Y3,            S0^X6^Y7,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X8^Y8,      Z1^X4^Y4,      S1^Y5^X7,      Z0^X5^Y7,      S0^X6^Y6,      Y2,            X3,            Y3^X6,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X8^Y8,      Z0^X4^Y4,      S1^Y5^X7,      S0^X5^Y7,      Y2^X6^Y6,      X2,            X3,            Y3^X6,         },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X8^Y8,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      Z3^X6^Y6,      X6^X9^Y9,      S0^X7,         S1^Y7,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X8^Y8,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      Z3^X6^Y6,      S1^X9^Y9,      Y3,            S0^X7,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X8^Y8,      Z2^X4^Y4,      Z1^Y5^X7,      Z0^X5^Y7,      S1^X6^Y6,      S0^X9^Y9,      X3,            Y3,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X8^Y8,      Z1^X4^Y4,      S1^Y5^X7,      Z0^X5^Y7,      S0^X6^Y6,      X3^X9^Y9,      Y2,            Y3,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X8^Y8,      Z0^X4^Y4,      S1^Y5^X7,      S0^X5^Y7,      Y2^X6^Y6,      X3^X9^Y9,      X2,            Y3,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X9^Y9,      Z2^X4^Y4,      Z1^Y5^X8,      Z0^X5^Y8,      Z4^Y6^X7,      S0^X7,         S1^Y7,         Z3^X6^Y7,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X9^Y9,      Z2^X4^Y4,      Z1^Y5^X8,      Z0^X5^Y8,      S1^Y6^X7,      Y3,            S0^X7,         Z3^X6^Y7,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X9^Y9,      Z2^X4^Y4,      Z1^Y5^X8,      Z0^X5^Y8,      S1^Y6^X7,      X3,            Y3,            S0^X6^Y7,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X9^Y9,      Z1^X4^Y4,      S1^Y5^X8,      Z0^X5^Y8,      S0^Y6^X7,      X3,            Y3,            Y2^X6^Y7,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X9^Y9,      Z0^X4^Y4,      S1^Y5^X8,      S0^X5^Y8,      Y2^Y6^X7,      X3,            Y3,            X2^X6^Y7,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X9^Y9,      Z2^X4^Y4,      Z1^Y5^X8,      Z0^X5^Y8,      Z4^Y6^X7,      Z3^X6^Y7,      S0^X7,         S1^Y7,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X9^Y9,      Z2^X4^Y4,      Z1^Y5^X8,      Z0^X5^Y8,      S1^Y6^X7,      Z3^X6^Y7,      Y3,            S0^X7,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X9^Y9,      Z2^X4^Y4,      Z1^Y5^X8,      Z0^X5^Y8,      S1^Y6^X7,      S0^X6^Y7,      X3,            Y3,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X9^Y9,      Z1^X4^Y4,      S1^Y5^X8,      Z0^X5^Y8,      S0^Y6^X7,      Y2^X6^Y7,      X3,            Y3,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X9^Y9,      Z0^X4^Y4,      S1^Y5^X8,      S0^X5^Y8,      Y2^Y6^X7,      X2^X6^Y7,      X3,            Y3,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            X4,            Y4,            Y5^X10,        X5^Y10,        Y6^X9,         S0^X6^Y9,      S1^Y7^X8,      S2^X7^Y8,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y3,            X4,            Y4^X10,        X5^Y9,         Y5^X9,         S0^X6^Y8,      S1^Y6^X8,      S2^X7^Y7,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            X3,            Y3,            Y4^X9,         X4^Y9,         Y5^X8,         S0^X5^Y8,      S1^Y6^X7,      S2^X6^Y7,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y2,            X3,            Y3^X9,         X4^Y8,         Y4^X8,         S0^X5^Y7,      S1^Y5^X7,      S2^X6^Y6,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            X2,            Y2,            Y3^X8,         X3^Y8,         Y4^X7,         S0^X4^Y7,      S1^Y5^X6,      S2^X5^Y6,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Z0^X4^Y4,      Y4,            X5,            Y5^X10,        Y6^X9,         S0^X6^Y9,      S1^Y7^X8,      S2^X7^Y8,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Z0^X4^Y4,      Y3,            Y4,            X5^Y9,         Y5^X9,         S0^X6^Y8,      S1^Y6^X8,      S2^X7^Y7,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Z0^X4^Y4,      X3,            Y3,            Y4^X9,         Y5^X8,         S0^X5^Y8,      S1^Y6^X7,      S2^X6^Y7,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Z0^X4^Y4,      Y2,            X3,            Y3^X9,         Y4^X8,         S0^X5^Y7,      S1^Y5^X7,      S2^X6^Y6,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Z0^X4^Y4,      X2,            Y2,            X3^Y7,         Y3^X7,         S0^X4^Y6,      S1^Y4^X6,      S2^X5^Y5,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X5^Y5,      Z0^X4^Y4,      X5,            Y5,            Y6^X9,         S0^X6^Y9,      S1^Y7^X8,      S2^X7^Y8,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X5^Y5,      Z0^X4^Y4,      Y3,            X5,            Y5^X9,         S0^X6^Y8,      S1^Y6^X8,      S2^X7^Y7,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X5^Y5,      Z0^X4^Y4,      X3,            Y3,            Y5^X8,         S0^X5^Y8,      S1^Y6^X7,      S2^X6^Y7,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X5^Y5,      Z0^X4^Y4,      Y2,            X3,            Y3^X8,         S0^X5^Y7,      S1^Y5^X7,      S2^X6^Y6,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X5^Y5,      S2^X4^Y4,      X2,            Y2,            Y3^X6,         X3^Y7,         S0^X4^Y6,      S1^X5^Y5,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X5^Y5,      Z0^X4^Y4,      X5^X6^Y6,      Y5,            Y6,            S0^X6^Y9,      S1^Y7^X8,      S2^X7^Y8,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X5^Y5,      Z0^X4^Y4,      X5^X6^Y6,      Y3,            Y5,            S0^X6^Y8,      S1^Y6^X8,      S2^X7^Y7,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X5^Y5,      Z0^X4^Y4,      S2^X6^Y6,      X3,            Y3,            Y5^X7,         S0^X5^Y7,      S1^X6^Y6,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X5^Y5,      Z0^X4^Y4,      S2^X6^Y6,      Y2,            X3,            Y3^X7,         S0^X5^Y6,      S1^Y5^X6,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X5^Y5,      S2^X4^Y4,      S1^X6^Y6,      X2,            Y2,            Y3^X5,         X3^Y6,         S0^X4^Y5,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X6^Y6,      Z1^X4^Y4,      X5,            Y6,            S0^X6^Y9,      S1^Y7^X8,      S2^X7^Y8,      Z0^X5^Y5,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X6^Y6,      Z1^X4^Y4,      Y3,            X5,            S0^X6^Y8,      S1^Y6^X8,      S2^X7^Y7,      Z0^X5^Y5,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X6^Y6,      Z1^X4^Y4,      X3,            Y3,            S0^X5^Y8,      S1^Y6^X7,      S2^X6^Y7,      Z0^X5^Y5,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X6^Y6,      Z0^X4^Y4,      Y2,            X3,            Y3^X7,         S0^X5^Y7,      S1^X6^Y6,      S2^X5^Y5,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X6^Y6,      S2^X4^Y4,      X2,            Y2,            Y3^X6,         X3^Y7,         S0^X5^Y6,      S1^X5^Y5,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5,            Y6,            S0^X6^Y9,      S1^Y7^X8,      S2^X7^Y8,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      Y3,            X5,            S0^X6^Y8,      S1^Y6^X8,      S2^X7^Y7,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X3,            Y3,            S0^X5^Y8,      S1^Y6^X7,      S2^X6^Y7,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X6^Y6,      Z0^X4^Y4,      S2^X5^Y5,      Y2,            X3,            Y3^X7,         S0^X5^Y7,      S1^X6^Y6,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X6^Y6,      S2^X4^Y4,      S1^X5^Y5,      X2,            Y2,            Y3^X6,         X3^Y7,         S0^X5^Y6,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^X7^Y7,      Y6,            S0^X6,         S1^Y7^X8,      S2^X7^Y8,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      X5^X7^Y7,      Y3,            S0^X6,         S1^Y6^X8,      S2^X7^Y7,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X6^Y6,      Z1^X4^Y4,      Z0^X5^Y5,      S2^X7^Y7,      X3,            Y3,            S0^X5^Y7,      S1^X6^Y6,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X6^Y6,      Z0^X4^Y4,      S2^X5^Y5,      S1^X7^Y7,      Y2,            X3,            Y3^X6,         S0^X5^Y6,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X6^Y6,      S2^X4^Y4,      S1^X5^Y5,      S0^X7^Y7,      X2,            Y2,            Y3^X5,         X3^Y6,         },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      Y6,            S0^X6,         S1^Y7^X8,      S2^X7^Y8,      Z2^X5^Y6,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      Y3,            S0^X6,         S1^Y6^X8,      S2^X7^Y7,      Z2^X5^Y6,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      X3,            Y3,            S0^X6^Y7,      S1^Y6^X7,      S2^X5^Y6,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X7^Y7,      Z0^X4^Y4,      S2^Y5^X6,      Y2,            X3,            Y3^X7,         S0^X6^Y6,      S1^X5^Y6,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X7^Y7,      S2^X4^Y4,      S1^Y5^X6,      X2,            Y2,            Y3^X6,         X3^Y6,         S0^X5^Y6,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      Z2^X5^Y6,      Y6,            S0^X6,         S1^Y7^X8,      S2^X7^Y8,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      Z2^X5^Y6,      Y3,            S0^X6,         S1^Y6^X8,      S2^X7^Y7,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      S2^X5^Y6,      X3,            Y3,            S0^X6^Y7,      S1^Y6^X7,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X7^Y7,      Z0^X4^Y4,      S2^Y5^X6,      S1^X5^Y6,      Y2,            X3,            Y3^X7,         S0^X6^Y6,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X7^Y7,      S2^X4^Y4,      S1^Y5^X6,      S0^X5^Y6,      X2,            Y2,            Y3^X6,         X3^Y6,         },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      Z2^X5^Y6,      S2^X8^Y8,      Y6,            S0^X6,         S1^X7^Y7,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      Z2^X5^Y6,      S2^X8^Y8,      Y3,            S0^X6,         S1^Y6^X7,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X7^Y7,      Z1^X4^Y4,      Z0^Y5^X6,      S2^X5^Y6,      S1^X8^Y8,      X3,            Y3,            S0^X6^Y6,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X7^Y7,      Z0^X4^Y4,      S2^Y5^X6,      S1^X5^Y6,      S0^X8^Y8,      Y2,            X3,            Y3^X6,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X7^Y7,      S2^X4^Y4,      S1^Y5^X6,      S0^X5^Y6,      X3^X8^Y8,      X2,            Y2,            Y3^X6,         },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      Z3^X5^Y7,      S0^X6,         S1^Y7,         S2^X7^Y8,      Z2^X6^Y6,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      Z2^X5^Y7,      Y3,            S0^X6,         S1^X7^Y7,      S2^X6^Y6,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      S2^X5^Y7,      X3,            Y3,            S0^X6^Y7,      S1^X6^Y6,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X8^Y8,      Z0^X4^Y4,      S2^Y5^X7,      S1^X5^Y7,      Y2,            X3,            Y3^X6,         S0^X6^Y6,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X8^Y8,      S2^X4^Y4,      S1^Y5^X7,      S0^X5^Y7,      X2,            X3,            Y3^X6,         Y2^X6^Y6,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      Z3^X5^Y7,      Z2^X6^Y6,      S0^X6,         S1^Y7,         S2^X7^Y8,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      Z2^X5^Y7,      S2^X6^Y6,      Y3,            S0^X6,         S1^X7^Y7,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      S2^X5^Y7,      S1^X6^Y6,      X3,            Y3,            S0^X6^Y7,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X8^Y8,      Z0^X4^Y4,      S2^Y5^X7,      S1^X5^Y7,      S0^X6^Y6,      Y2,            X3,            Y3^X6,         },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X8^Y8,      S2^X4^Y4,      S1^Y5^X7,      S0^X5^Y7,      Y2^X6^Y6,      X2,            X3,            Y3^X6,         },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      Z3^X5^Y7,      Z2^X6^Y6,      S2^X9^Y9,      S0^X6,         S1^Y7,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      Z2^X5^Y7,      S2^X6^Y6,      S1^X9^Y9,      Y3,            S0^X6,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X8^Y8,      Z1^X4^Y4,      Z0^Y5^X7,      S2^X5^Y7,      S1^X6^Y6,      S0^X9^Y9,      X3,            Y3,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X8^Y8,      Z0^X4^Y4,      S2^Y5^X7,      S1^X5^Y7,      S0^X6^Y6,      X3^X9^Y9,      Y2,            Y3,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X8^Y8,      S2^X4^Y4,      S1^Y5^X7,      S0^X5^Y7,      Y2^X6^Y6,      X3^X9^Y9,      X2,            Y3,            },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      Z3^X5^Y8,      Z2^Y6^X7,      S0^X7,         S1^Y7,         S2^X6^Y7,      },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      Z2^X5^Y8,      S2^Y6^X7,      Y3,            S0^X7,         S1^X6^Y7,      },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      S2^X5^Y8,      S1^Y6^X7,      X3,            Y3,            S0^X6^Y7,      },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X9^Y9,      Z0^X4^Y4,      S2^Y5^X8,      S1^X5^Y8,      S0^Y6^X7,      X3,            Y3,            Y2^X6^Y7,      },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X9^Y9,      S2^X4^Y4,      S1^Y5^X8,      S0^X5^Y8,      Y2^Y6^X7,      X3,            Y3,            X2^X6^Y7,      },
    {X0,            X1,            X2,            X3,            Y0,            Y1,            Y2,            Y3,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      Z3^X5^Y8,      Z2^Y6^X7,      S2^X6^Y7,      S0^X7,         S1^Y7,         },
    {0,             X0,            X1,            X2,            Y0,            Y1,            Y2,            X3,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      Z2^X5^Y8,      S2^Y6^X7,      S1^X6^Y7,      Y3,            S0^X7,         },
    {0,             0,             X0,            X1,            Y0,            Y1,            X2,            Y2,            Y4^X9^Y9,      Z1^X4^Y4,      Z0^Y5^X8,      S2^X5^Y8,      S1^Y6^X7,      S0^X6^Y7,      X3,            Y3,            },
    {0,             0,             0,             X0,            Y0,            X1,            X2,            Y1,            Y4^X9^Y9,      Z0^X4^Y4,      S2^Y5^X8,      S1^X5^Y8,      S0^Y6^X7,      Y2^X6^Y7,      X3,            Y3,            },
    {0,             0,             0,             0,             X0,            Y0,            X1,            Y1,            Y4^X9^Y9,      S2^X4^Y4,      S1^Y5^X8,      S0^X5^Y8,      Y2^Y6^X7,      X2^X6^Y7,      X3,            Y3,            },
    {X0,            X1,            Z0,            Y0,            Y1,            Z1,            X2,            Z2,            X4^Y4,         Y2,            Z3,            Y3,            X3,            Z4,            Y4,            X5,            },
    {0,             X0,            Z0,            Y0,            X1,            Z1,            Y1,            Z2,            X4^Y4,         Y2,            Z3,            Y3,            X2,            Z4,            Y4,            X3,            },
    {0,             0,             X0,            Y0,            X1,            Z0,            Y1,            Z1,            X4^Y4,         Y2,            Z2,            Y3,            X2,            Z3,            Y4,            X3,            },
    {0,             0,             0,             X0,            Y0,            Z0,            X1,            Z1,            X4^Y4,         Y1,            Z2,            Y2,            X2,            Z3,            Y3,            X3,            },
    {0,             0,             0,             0,             X0,            Z0,            Y0,            Z1,            Y1^X4^Y4,      X1,            Z2,            Y2,            X2,            Z3,            Y3,            X3,            },
    {X0,            X1,            Z0,            Y0,            Y1,            Z1,            X2,            Z2,            Y4^X5^Y5,      X4^Y4,         Y2,            Z3,            Y3,            X3,            Z4,            X5,            },
    {0,             X0,            Z0,            Y0,            X1,            Z1,            Y1,            Z2,            Y4^X5^Y5,      X4^Y4,         Y2,            Z3,            Y3,            X2,            Z4,            X3,            },
    {0,             0,             X0,            Y0,            X1,            Z0,            Y1,            Z1,            Y4^X5^Y5,      X4^Y4,         Y2,            Z2,            Y3,            X2,            Z3,            X3,            },
    {0,             0,             0,             X0,            Y0,            Z0,            X1,            Z1,            Z3^Y4^X5^Y5,   X4^Y4,         Y1,            Z2,            Y2,            X2,            Y3,            X3,            },
    {0,             0,             0,             0,             X0,            Z0,            Y0,            Z1,            Z3^Y4^X5^Y5,   Y1^X4^Y4,      X1,            Z2,            Y2,            X2,            Y3,            X3,            },
    {X0,            X1,            Z0,            Y0,            Y1,            Z1,            X2,            Z2,            Y4^X5^Y5,      X4^Y4,         Z3^X5,         Y2,            Y3,            X3,            Z4,            X5,            },
    {0,             X0,            Z0,            Y0,            X1,            Z1,            Y1,            Z2,            Y4^X5^Y5,      X4^Y4,         Z3^X5,         Y2,            Y3,            X2,            Z4,            X3,            },
    {0,             0,             X0,            Y0,            X1,            Z0,            Y1,            Z1,            Y4^X5^Y5,      X4^Y4,         Z2^X5,         Y2,            Y3,            X2,            Z3,            X3,            },
    {0,             0,             0,             X0,            Y0,            Z0,            X1,            Z1,            Z3^Y4^X5^Y5,   X4^Y4,         Z2^X5,         Y1,            Y2,            X2,            Y3,            X3,            },
    {0,             0,             0,             0,             X0,            Z0,            Y0,            Z1,            Z3^Y4^X5^Y5,   Y1^X4^Y4,      Z2^X5,         X1,            Y2,            X2,            Y3,            X3,            },
    {X0,            X1,            Z0,            Y0,            Y1,            Z1,            X2,            Z2,            Y4^X6^Y6,      X4^Y4,         Y2,            Y3,            Z3,            X3,            Z4,            X5^Y5,         },
    {0,             X0,            Z0,            Y0,            X1,            Z1,            Y1,            Z2,            Y4^X6^Y6,      X4^Y4,         Z3,            Y3,            X2,            Z4,            X3,            Y2^X5^Y5,      },
    {0,             0,             X0,            Y0,            X1,            Z0,            Y1,            Z1,            Y4^X6^Y6,      X4^Y4,         Z2,            Y3,            X2,            Z3,            X3,            Y2^X5^Y5,      },
    {0,             0,             0,             X0,            Y0,            Z0,            X1,            Z1,            Z3^Y4^X6^Y6,   X4^Y4,         Z2,            Y2,            X2,            Y3,            X3,            Y1^X5^Y5,      },
    {0,             0,             0,             0,             X0,            Z0,            Y0,            Z1,            Z3^Y4^X6^Y6,   Y1^X4^Y4,      Z2,            Y2,            X2,            Y3,            X3,            X1^X5^Y5,      },
    {X0,            X1,            Z0,            Y0,            Y1,            Z1,            X2,            Z2,            Y4^X6^Y6,      X4^Y4,         X5^Y5,         Y2,            Y3,            Z3,            X3,            Z4,            },
    {0,             X0,            Z0,            Y0,            X1,            Z1,            Y1,            Z2,            Y4^X6^Y6,      X4^Y4,         Y2^X5^Y5,      Z3,            Y3,            X2,            Z4,            X3,            },
    {0,             0,             X0,            Y0,            X1,            Z0,            Y1,            Z1,            Y4^X6^Y6,      X4^Y4,         Y2^X5^Y5,      Z2,            Y3,            X2,            Z3,            X3,            },
    {0,             0,             0,             X0,            Y0,            Z0,            X1,            Z1,            Z3^Y4^X6^Y6,   X4^Y4,         Y1^X5^Y5,      Z2,            Y2,            X2,            Y3,            X3,            },
    {0,             0,             0,             0,             X0,            Z0,            Y0,            Z1,            Z3^Y4^X6^Y6,   Y1^X4^Y4,      X1^X5^Y5,      Z2,            Y2,            X2,            Y3,            X3,            },
    {X0,            X1,            Z0,            Y0,            Y1,            Z1,            X2,            Z2,            Y4^X6^Y6,      X4^Y4,         X5^Y5,         Z3^X6,         Y2,            Y3,            X3,            Z4,            },
    {0,             X0,            Z0,            Y0,            X1,            Z1,            Y1,            Z2,            Y4^X6^Y6,      X4^Y4,         Y2^X5^Y5,      Z3^X6,         Y3,            X2,            Z4,            X3,            },
    {0,             0,             X0,            Y0,            X1,            Z0,            Y1,            Z1,            Y4^X6^Y6,      X4^Y4,         Y2^X5^Y5,      Z2^X6,         Y3,            X2,            Z3,            X3,            },
    {0,             0,             0,             X0,            Y0,            Z0,            X1,            Z1,            Z3^Y4^X6^Y6,   X4^Y4,         Y1^X5^Y5,      Z2^X6,         Y2,            X2,            Y3,            X3,            },
    {0,             0,             0,             0,             X0,            Z0,            Y0,            Z1,            Z3^Y4^X6^Y6,   Y1^X4^Y4,      X1^X5^Y5,      Z2^X6,         Y2,            X2,            Y3,            X3,            },
    {X0,            X1,            Z0,            Y0,            Y1,            Z1,            X2,            Z2,            Y4^X7^Y7,      X4^Y4,         Y2^Y5^X6,      Y3,            Z3,            X3,            Z4,            X5^Y6,         },
    {0,             X0,            Z0,            Y0,            X1,            Z1,            Y1,            Z2,            Y4^X7^Y7,      X4^Y4,         Y2^Y5^X6,      Y3,            X2,            Z4,            X3,            Z3^X5^Y6,      },
    {0,             0,             X0,            Y0,            X1,            Z0,            Y1,            Z1,            Y4^X7^Y7,      X4^Y4,         Y2^Y5^X6,      Y3,            X2,            Z3,            X3,            Z2^X5^Y6,      },
    {0,             0,             0,             X0,            Y0,            Z0,            X1,            Z1,            Z3^Y4^X7^Y7,   X4^Y4,         Y1^Y5^X6,      Y2,            X2,            Y3,            X3,            Z2^X5^Y6,      },
    {0,             0,             0,             0,             X0,            Z0,            Y0,            Z1,            Z3^Y4^X7^Y7,   Y1^X4^Y4,      X1^Y5^X6,      Y2,            X2,            Y3,            X3,            Z2^X5^Y6,      },
    {X0,            X1,            Z0,            Y0,            Y1,            Z1,            X2,            Z2,            Y4^X7^Y7,      X4^Y4,         Y2^Y5^X6,      X5^Y6,         Y3,            Z3,            X3,            Z4,            },
    {0,             X0,            Z0,            Y0,            X1,            Z1,            Y1,            Z2,            Y4^X7^Y7,      X4^Y4,         Y2^Y5^X6,      Z3^X5^Y6,      Y3,            X2,            Z4,            X3,            },
    {0,             0,             X0,            Y0,            X1,            Z0,            Y1,            Z1,            Y4^X7^Y7,      X4^Y4,         Y2^Y5^X6,      Z2^X5^Y6,      Y3,            X2,            Z3,            X3,            },
    {0,             0,             0,             X0,            Y0,            Z0,            X1,            Z1,            Z3^Y4^X7^Y7,   X4^Y4,         Y1^Y5^X6,      Z2^X5^Y6,      Y2,            X2,            Y3,            X3,            },
    {0,             0,             0,             0,             X0,            Z0,            Y0,            Z1,            Z3^Y4^X7^Y7,   Y1^X4^Y4,      X1^Y5^X6,      Z2^X5^Y6,      Y2,            X2,            Y3,            X3,            },
    {X0,            X1,            Z0,            Y0,            Y1,            Z1,            X2,            Z2,            Y4^X7^Y7,      X4^Y4,         Y2^Y5^X6,      X5^Y6,         Z3^X7,         Y3,            X3,            Z4,            },
    {0,             X0,            Z0,            Y0,            X1,            Z1,            Y1,            Z2,            Y4^X7^Y7,      X4^Y4,         Y2^Y5^X6,      Y3^X5^Y6,      Z3^X7,         X2,            Z4,            X3,            },
    {0,             0,             X0,            Y0,            X1,            Z0,            Y1,            Z1,            Y4^X7^Y7,      X4^Y4,         Y2^Y5^X6,      Y3^X5^Y6,      Z2^X7,         X2,            Z3,            X3,            },
    {0,             0,             0,             X0,            Y0,            Z0,            X1,            Z1,            Z3^Y4^X7^Y7,   X4^Y4,         Y1^Y5^X6,      Y2^X5^Y6,      Z2^X7,         X2,            Y3,            X3,            },
    {0,             0,             0,             0,             X0,            Z0,            Y0,            Z1,            Z3^Y4^X7^Y7,   Y1^X4^Y4,      X1^Y5^X6,      Y2^X5^Y6,      Z2^X7,         X2,            Y3,            X3,            },
    {X0,            X1,            Z0,            Y0,            Y1,            Z1,            X2,            Z2,            Y4^X8^Y8,      X4^Y4,         Y2^Y5^X7,      X5^Y7,         Z3,            X3,            Z4,            Y3^X6^Y6,      },
    {0,             X0,            Z0,            Y0,            X1,            Z1,            Y1,            Z2,            Y4^X8^Y8,      X4^Y4,         Y2^Y5^X7,      Z3^X5^Y7,      X2,            Z4,            X3,            Y3^X6^Y6,      },
    {0,             0,             X0,            Y0,            X1,            Z0,            Y1,            Z1,            Y4^X8^Y8,      X4^Y4,         Y2^Y5^X7,      Z2^X5^Y7,      X2,            Z3,            X3,            Y3^X6^Y6,      },
    {0,             0,             0,             X0,            Y0,            Z0,            X1,            Z1,            Z3^Y4^X8^Y8,   X4^Y4,         Y1^Y5^X7,      Z2^X5^Y7,      X2,            Y3,            X3,            Y2^X6^Y6,      },
    {0,             0,             0,             0,             X0,            Z0,            Y0,            Z1,            Z3^Y4^X8^Y8,   Y1^X4^Y4,      X1^Y5^X7,      Z2^X5^Y7,      X2,            Y3,            X3,            Y2^X6^Y6,      },
    {X0,            X1,            Z0,            Y0,            Y1,            Z1,            X2,            Z2,            Y4^X8^Y8,      X4^Y4,         Y2^Y5^X7,      X5^Y7,         Y3^X6^Y6,      Z3,            X3,            Z4,            },
    {0,             X0,            Z0,            Y0,            X1,            Z1,            Y1,            Z2,            Y4^X8^Y8,      X4^Y4,         Y2^Y5^X7,      Z3^X5^Y7,      Y3^X6^Y6,      X2,            Z4,            X3,            },
    {0,             0,             X0,            Y0,            X1,            Z0,            Y1,            Z1,            Y4^X8^Y8,      X4^Y4,         Y2^Y5^X7,      Z2^X5^Y7,      Y3^X6^Y6,      X2,            Z3,            X3,            },
    {0,             0,             0,             X0,            Y0,            Z0,            X1,            Z1,            Z3^Y4^X8^Y8,   X4^Y4,         Y1^Y5^X7,      Z2^X5^Y7,      Y2^X6^Y6,      X2,            Y3,            X3,            },
    {0,             0,             0,             0,             X0,            Z0,            Y0,            Z1,            Z3^Y4^X8^Y8,   Y1^X4^Y4,      X1^Y5^X7,      Z2^X5^Y7,      Y2^X6^Y6,      X2,            Y3,            X3,            },
    {X0,            X1,            Z0,            Y0,            Y1,            Z1,            X2,            Z2,            Y4^X8^Y8,      X4^Y4,         Y2^Y5^X7,      X5^Y7,         Y3^X6^Y6,      Z3^X8,         X3,            Z4,            },
    {0,             X0,            Z0,            Y0,            X1,            Z1,            Y1,            Z2,            Y4^X8^Y8,      X4^Y4,         Y2^Y5^X7,      Y3^X5^Y7,      X2^X6^Y6,      Z3^X8,         Z4,            X3,            },
    {0,             0,             X0,            Y0,            X1,            Z0,            Y1,            Z1,            Y4^X8^Y8,      X4^Y4,         Y2^Y5^X7,      Y3^X5^Y7,      X2^X6^Y6,      Z2^X8,         Z3,            X3,            },
    {0,             0,             0,             X0,            Y0,            Z0,            X1,            Z1,            Z3^Y4^X8^Y8,   X4^Y4,         Y1^Y5^X7,      Y2^X5^Y7,      X2^X6^Y6,      Z2^X8,         Y3,            X3,            },
    {0,             0,             0,             0,             X0,            Z0,            Y0,            Z1,            Z3^Y4^X8^Y8,   Y1^X4^Y4,      X1^Y5^X7,      Y2^X5^Y7,      X2^X6^Y6,      Z2^X8,         Y3,            X3,            },
    {X0,            X1,            Z0,            Y0,            Y1,            Z1,            X2,            Z2,            Y4^X9^Y9,      X4^Y4,         Y2^Y5^X8,      X5^Y8,         Y3^Y6^X7,      X3,            Z4,            Z3^X6^Y7,      },
    {0,             X0,            Z0,            Y0,            X1,            Z1,            Y1,            Z2,            Y4^X9^Y9,      X4^Y4,         Y2^Y5^X8,      Z3^X5^Y8,      Y3^Y6^X7,      Z4,            X3,            X2^X6^Y7,      },
    {0,             0,             X0,            Y0,            X1,            Z0,            Y1,            Z1,            Y4^X9^Y9,      X4^Y4,         Y2^Y5^X8,      Z2^X5^Y8,      Y3^Y6^X7,      Z3,            X3,            X2^X6^Y7,      },
    {0,             0,             0,             X0,            Y0,            Z0,            X1,            Z1,            Z3^Y4^X9^Y9,   X4^Y4,         Y1^Y5^X8,      Z2^X5^Y8,      Y2^Y6^X7,      Y3,            X3,            X2^X6^Y7,      },
    {0,             0,             0,             0,             X0,            Z0,            Y0,            Z1,            Z3^Y4^X9^Y9,   Y1^X4^Y4,      X1^Y5^X8,      Z2^X5^Y8,      Y2^Y6^X7,      Y3,            X3,            X2^X6^Y7,      },
    {X0,            X1,            Z0,            Y0,            Y1,            Z1,            X2,            Z2,            Y4^X9^Y9,      X4^Y4,         Y2^Y5^X8,      X5^Y8,         Y3^Y6^X7,      Z3^X6^Y7,      X3,            Z4,            },
    {0,             X0,            Z0,            Y0,            X1,            Z1,            Y1,            Z2,            Y4^X9^Y9,      X4^Y4,         Y2^Y5^X8,      Z3^X5^Y8,      Y3^Y6^X7,      X2^X6^Y7,      Z4,            X3,            },
    {0,             0,             X0,            Y0,            X1,            Z0,            Y1,            Z1,            Y4^X9^Y9,      X4^Y4,         Y2^Y5^X8,      Z2^X5^Y8,      Y3^Y6^X7,      X2^X6^Y7,      Z3,            X3,            },
    {0,             0,             0,             X0,            Y0,            Z0,            X1,            Z1,            Z3^Y4^X9^Y9,   X4^Y4,         Y1^Y5^X8,      Z2^X5^Y8,      Y2^Y6^X7,      X2^X6^Y7,      Y3,            X3,            },
    {0,             0,             0,             0,             X0,            Z0,            Y0,            Z1,            Z3^Y4^X9^Y9,   Y1^X4^Y4,      X1^Y5^X8,      Z2^X5^Y8,      Y2^Y6^X7,      X2^X6^Y7,      Y3,            X3,            },
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
       0, // 1 bpe ua @ SW_64K_R_X 1xaa @ Navi2x
       1, // 2 bpe ua @ SW_64K_R_X 1xaa @ Navi2x
       2, // 4 bpe ua @ SW_64K_R_X 1xaa @ Navi2x
       3, // 8 bpe ua @ SW_64K_R_X 1xaa @ Navi2x
       4, // 16 bpe ua @ SW_64K_R_X 1xaa @ Navi2x
       0, // 1 pipes (1 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
       1, // 1 pipes (1 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
       2, // 1 pipes (1 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
       3, // 1 pipes (1 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
       4, // 1 pipes (1 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      38, // 2 pipes (1-2 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      39, // 2 pipes (1-2 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      40, // 2 pipes (1-2 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      41, // 2 pipes (1-2 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      42, // 2 pipes (1-2 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      43, // 4 pipes (1-2 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      44, // 4 pipes (1-2 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      45, // 4 pipes (1-2 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      46, // 4 pipes (1-2 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      47, // 4 pipes (1-2 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      48, // 8 pipes (2 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      49, // 8 pipes (2 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      50, // 8 pipes (2 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      51, // 8 pipes (2 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      52, // 8 pipes (2 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      53, // 4 pipes (4 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      54, // 4 pipes (4 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      55, // 4 pipes (4 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      56, // 4 pipes (4 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      57, // 4 pipes (4 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      58, // 8 pipes (4 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      59, // 8 pipes (4 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      60, // 8 pipes (4 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      61, // 8 pipes (4 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      62, // 8 pipes (4 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      63, // 16 pipes (4 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      64, // 16 pipes (4 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      65, // 16 pipes (4 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      66, // 16 pipes (4 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      67, // 16 pipes (4 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      68, // 8 pipes (8 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      69, // 8 pipes (8 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      70, // 8 pipes (8 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      71, // 8 pipes (8 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      72, // 8 pipes (8 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      73, // 16 pipes (8 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      74, // 16 pipes (8 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      75, // 16 pipes (8 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      76, // 16 pipes (8 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      77, // 16 pipes (8 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      78, // 32 pipes (8 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      79, // 32 pipes (8 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      80, // 32 pipes (8 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      81, // 32 pipes (8 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      82, // 32 pipes (8 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      83, // 16 pipes (16 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      84, // 16 pipes (16 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      85, // 16 pipes (16 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      86, // 16 pipes (16 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      87, // 16 pipes (16 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      88, // 32 pipes (16 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      89, // 32 pipes (16 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      90, // 32 pipes (16 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      91, // 32 pipes (16 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      92, // 32 pipes (16 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      93, // 64 pipes (16 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      94, // 64 pipes (16 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      95, // 64 pipes (16 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      96, // 64 pipes (16 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      97, // 64 pipes (16 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      98, // 32 pipes (32 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
      99, // 32 pipes (32 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
     100, // 32 pipes (32 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
     101, // 32 pipes (32 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
     102, // 32 pipes (32 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
     103, // 64 pipes (32 PKRs) 1 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
     104, // 64 pipes (32 PKRs) 2 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
     105, // 64 pipes (32 PKRs) 4 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
     106, // 64 pipes (32 PKRs) 8 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
     107, // 64 pipes (32 PKRs) 16 bpe pa @ SW_64K_R_X 1xaa @ Navi2x
};

const UINT_16 HTILE_64K_RBPLUS_PATIDX[] =
{
       0, // 1xaa ua @ HTILE_64K_PATIDX @ Navi2x
       0, // 2xaa ua @ HTILE_64K_PATIDX @ Navi2x
       0, // 4xaa ua @ HTILE_64K_PATIDX @ Navi2x
       0, // 8xaa ua @ HTILE_64K_PATIDX @ Navi2x
       0, // 1 pipes (1-2 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ Navi2x
       0, // 1 pipes (1-2 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ Navi2x
       0, // 1 pipes (1-2 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ Navi2x
       0, // 1 pipes (1-2 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ Navi2x
      13, // 2 pipes (1-2 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ Navi2x
      13, // 2 pipes (1-2 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ Navi2x
      13, // 2 pipes (1-2 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ Navi2x
      13, // 2 pipes (1-2 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ Navi2x
      14, // 4 pipes (1-2 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ Navi2x
      14, // 4 pipes (1-2 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ Navi2x
      14, // 4 pipes (1-2 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ Navi2x
      14, // 4 pipes (1-2 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ Navi2x
      15, // 8 pipes (1-2 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ Navi2x
      15, // 8 pipes (1-2 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ Navi2x
      15, // 8 pipes (1-2 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ Navi2x
      15, // 8 pipes (1-2 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ Navi2x
      13, // 2 pipes (4 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ Navi2x
      13, // 2 pipes (4 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ Navi2x
      13, // 2 pipes (4 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ Navi2x
      13, // 2 pipes (4 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ Navi2x
      16, // 4 pipes (4 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ Navi2x
      16, // 4 pipes (4 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ Navi2x
      16, // 4 pipes (4 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ Navi2x
      16, // 4 pipes (4 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ Navi2x
      17, // 8 pipes (4 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ Navi2x
      17, // 8 pipes (4 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ Navi2x
      17, // 8 pipes (4 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ Navi2x
      17, // 8 pipes (4 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ Navi2x
      18, // 16 pipes (4 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ Navi2x
      18, // 16 pipes (4 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ Navi2x
      18, // 16 pipes (4 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ Navi2x
      18, // 16 pipes (4 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ Navi2x
      19, // 4 pipes (8 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ Navi2x
      19, // 4 pipes (8 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ Navi2x
      19, // 4 pipes (8 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ Navi2x
      19, // 4 pipes (8 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ Navi2x
      20, // 8 pipes (8 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ Navi2x
      20, // 8 pipes (8 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ Navi2x
      20, // 8 pipes (8 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ Navi2x
      20, // 8 pipes (8 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ Navi2x
      21, // 16 pipes (8 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ Navi2x
      21, // 16 pipes (8 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ Navi2x
      21, // 16 pipes (8 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ Navi2x
      21, // 16 pipes (8 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ Navi2x
      22, // 32 pipes (8 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ Navi2x
      22, // 32 pipes (8 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ Navi2x
      22, // 32 pipes (8 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ Navi2x
      22, // 32 pipes (8 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ Navi2x
      23, // 8 pipes (16 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ Navi2x
      23, // 8 pipes (16 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ Navi2x
      23, // 8 pipes (16 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ Navi2x
      23, // 8 pipes (16 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ Navi2x
      24, // 16 pipes (16 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ Navi2x
      24, // 16 pipes (16 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ Navi2x
      24, // 16 pipes (16 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ Navi2x
      24, // 16 pipes (16 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ Navi2x
      25, // 32 pipes (16 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ Navi2x
      25, // 32 pipes (16 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ Navi2x
      25, // 32 pipes (16 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ Navi2x
      25, // 32 pipes (16 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ Navi2x
      26, // 64 pipes (16 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ Navi2x
      26, // 64 pipes (16 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ Navi2x
      26, // 64 pipes (16 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ Navi2x
      26, // 64 pipes (16 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ Navi2x
      27, // 16 pipes (32 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ Navi2x
      27, // 16 pipes (32 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ Navi2x
      27, // 16 pipes (32 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ Navi2x
      27, // 16 pipes (32 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ Navi2x
      28, // 32 pipes (32 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ Navi2x
      28, // 32 pipes (32 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ Navi2x
      28, // 32 pipes (32 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ Navi2x
      28, // 32 pipes (32 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ Navi2x
      29, // 64 pipes (32 PKRs) 1xaa pa @ HTILE_64K_PATIDX @ Navi2x
      29, // 64 pipes (32 PKRs) 2xaa pa @ HTILE_64K_PATIDX @ Navi2x
      29, // 64 pipes (32 PKRs) 4xaa pa @ HTILE_64K_PATIDX @ Navi2x
      29, // 64 pipes (32 PKRs) 8xaa pa @ HTILE_64K_PATIDX @ Navi2x
};

const UINT_16 CMASK_64K_RBPLUS_PATIDX[] =
{
       0, // 1 bpe ua @ CMASK_64K_PATIDX @ Navi2x
       0, // 2 bpe ua @ CMASK_64K_PATIDX @ Navi2x
       0, // 4 bpe ua @ CMASK_64K_PATIDX @ Navi2x
       0, // 8 bpe ua @ CMASK_64K_PATIDX @ Navi2x
       0, // 1 pipes (1-2 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ Navi2x
       0, // 1 pipes (1-2 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ Navi2x
       0, // 1 pipes (1-2 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ Navi2x
       0, // 1 pipes (1-2 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ Navi2x
       8, // 2 pipes (1-2 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ Navi2x
       8, // 2 pipes (1-2 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ Navi2x
       8, // 2 pipes (1-2 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ Navi2x
       8, // 2 pipes (1-2 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ Navi2x
       9, // 4 pipes (1-2 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ Navi2x
       9, // 4 pipes (1-2 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ Navi2x
       9, // 4 pipes (1-2 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ Navi2x
       9, // 4 pipes (1-2 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      10, // 8 pipes (1-2 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      10, // 8 pipes (1-2 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      10, // 8 pipes (1-2 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      10, // 8 pipes (1-2 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ Navi2x
       8, // 2 pipes (4 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ Navi2x
       8, // 2 pipes (4 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ Navi2x
       8, // 2 pipes (4 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ Navi2x
       8, // 2 pipes (4 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      11, // 4 pipes (4 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      11, // 4 pipes (4 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      11, // 4 pipes (4 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      11, // 4 pipes (4 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      12, // 8 pipes (4 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      12, // 8 pipes (4 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      12, // 8 pipes (4 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      12, // 8 pipes (4 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      13, // 16 pipes (4 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      13, // 16 pipes (4 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      13, // 16 pipes (4 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      13, // 16 pipes (4 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      14, // 4 pipes (8 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      14, // 4 pipes (8 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      14, // 4 pipes (8 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      14, // 4 pipes (8 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      15, // 8 pipes (8 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      15, // 8 pipes (8 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      15, // 8 pipes (8 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      16, // 8 pipes (8 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      15, // 16 pipes (8 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      15, // 16 pipes (8 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      15, // 16 pipes (8 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      17, // 16 pipes (8 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      18, // 32 pipes (8 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      18, // 32 pipes (8 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      18, // 32 pipes (8 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      19, // 32 pipes (8 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      20, // 8 pipes (16 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      20, // 8 pipes (16 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      20, // 8 pipes (16 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      21, // 8 pipes (16 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      22, // 16 pipes (16 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      22, // 16 pipes (16 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      22, // 16 pipes (16 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      23, // 16 pipes (16 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      22, // 32 pipes (16 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      22, // 32 pipes (16 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      22, // 32 pipes (16 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      24, // 32 pipes (16 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      25, // 64 pipes (16 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      25, // 64 pipes (16 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      25, // 64 pipes (16 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      26, // 64 pipes (16 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      27, // 16 pipes (32 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      27, // 16 pipes (32 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      27, // 16 pipes (32 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      28, // 16 pipes (32 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      29, // 32 pipes (32 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      29, // 32 pipes (32 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      29, // 32 pipes (32 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      30, // 32 pipes (32 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      29, // 64 pipes (32 PKRs) 1 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      29, // 64 pipes (32 PKRs) 2 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      29, // 64 pipes (32 PKRs) 4 bpe pa @ CMASK_64K_PATIDX @ Navi2x
      31, // 64 pipes (32 PKRs) 8 bpe pa @ CMASK_64K_PATIDX @ Navi2x
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
