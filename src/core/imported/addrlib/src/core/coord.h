/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2007-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

// Class used to define a coordinate bit

#ifndef __COORD_H
#define __COORD_H

namespace Addr
{
namespace V2
{
#if defined(__cplusplus)
    #if __cplusplus >= 201103L
        #define ADDR_CPP11_COMPILER TRUE
    #endif
#endif

#if defined(ADDR_CPP11_COMPILER)
enum Dim : INT_8
#else
enum Dim
#endif
{
   DIM_X,
   DIM_Y,
   DIM_Z,
   DIM_S,
   DIM_M,
   NUM_DIMS
};

class Coordinate
{
public:
    Coordinate();
    Coordinate(enum Dim dim, INT_32 n);

    VOID set(enum Dim dim, INT_32 n);
    UINT_32 ison(const UINT_32 *coords) const;
    enum Dim getdim();
    INT_8   getord();

    BOOL_32 operator==(const Coordinate& b);
    BOOL_32 operator<(const Coordinate& b);
    BOOL_32 operator>(const Coordinate& b);
    BOOL_32 operator<=(const Coordinate& b);
    BOOL_32 operator>=(const Coordinate& b);
    BOOL_32 operator!=(const Coordinate& b);
    Coordinate& operator++(INT_32);

private:
    enum Dim dim;
    INT_8 ord;
};

class CoordTerm
{
public:
    CoordTerm();
    VOID Clear();
    VOID add(Coordinate& co);
    VOID add(CoordTerm& cl);
    BOOL_32 remove(Coordinate& co);
    BOOL_32 Exists(Coordinate& co);
    VOID copyto(CoordTerm& cl);
    UINT_32 getsize();
    UINT_32 getxor(const UINT_32 *coords) const;

    VOID getsmallest(Coordinate& co);
    UINT_32 Filter(INT_8 f, Coordinate& co, UINT_32 start = 0, enum Dim axis = NUM_DIMS);
    Coordinate& operator[](UINT_32 i);
    BOOL_32 operator==(const CoordTerm& b);
    BOOL_32 operator!=(const CoordTerm& b);
    BOOL_32 exceedRange(const UINT_32 *ranges);

private:
    static const UINT_32 MaxCoords = 8;
    UINT_32 num_coords;
    Coordinate m_coord[MaxCoords];
};

class CoordEq
{
public:
    CoordEq();
    VOID remove(Coordinate& co);
    BOOL_32 Exists(Coordinate& co);
    VOID resize(UINT_32 n);
    UINT_32 getsize();
    virtual UINT_64 solve(const UINT_32 *coords) const;
    virtual VOID solveAddr(UINT_64 addr, UINT_32 sliceInM,
                           UINT_32 *coords) const;

    VOID copy(CoordEq& o, UINT_32 start = 0, UINT_32 num = 0xFFFFFFFF);
    VOID reverse(UINT_32 start = 0, UINT_32 num = 0xFFFFFFFF);
    VOID xorin(CoordEq& x, UINT_32 start = 0);
    UINT_32 Filter(INT_8 f, Coordinate& co, UINT_32 start = 0, enum Dim axis = NUM_DIMS);
    VOID shift(INT_32 amount, INT_32 start = 0);
    virtual CoordTerm& operator[](UINT_32 i);
    VOID mort2d(Coordinate& c0, Coordinate& c1, UINT_32 start = 0, UINT_32 end = 0);
    VOID mort3d(Coordinate& c0, Coordinate& c1, Coordinate& c2, UINT_32 start = 0, UINT_32 end = 0);

    BOOL_32 operator==(const CoordEq& b);
    BOOL_32 operator!=(const CoordEq& b);

private:
    static const UINT_32 MaxEqBits = 64;
    UINT_32 m_numBits;

    CoordTerm m_eq[MaxEqBits];
};

} // V2
} // Addr

#endif

