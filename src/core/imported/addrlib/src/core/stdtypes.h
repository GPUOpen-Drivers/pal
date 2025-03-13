/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2010-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef CORE_STDTYPES_H
#define CORE_STDTYPES_H
// -----------------------------------------------------------------------------

//****************************************************************************************
// Added include file "standard_typedefs.h" to contain the standard typedef's that all
// libraries can include (without the "extra" stuff this file contains). It eliminates
// other libraries (like Larry's Numbers Library or PrimLib) from having to also define
// these same typedefs.
//****************************************************************************************
#include "standard_typedefs.h"

//****************************************************************************************
// Non-standard typedefs:
//****************************************************************************************
typedef signed char *pint8;
typedef signed short *pint16;
typedef signed int *pint32;

typedef signed long long *pint64;

typedef unsigned char *puint8;
typedef unsigned short *puint16;
typedef unsigned int *puint32;

typedef unsigned long long *puint64;

typedef void * pvoid;
typedef char * pchar;
typedef const void * const_pvoid;
typedef const char * const_pchar;

//****************************************************************************************
// Windows specific definitions
//****************************************************************************************

// -----------------------------------------------------------------------------

// 64-bit integer support
#define CONST64(v) (v ## ll)
#define I64BIT0  0x1ll
#define I64MASK  0xffffffffffffffffll
#define I64S "ll"
#define I64D "lld"
#define I64U "llu"
#define I64X "llx"
#define STRTOUI64 strtoull

#ifndef MIN
#define MIN(a, b) (((a)<(b))?(a):(b))
#endif

#ifndef MAX
#define MAX(a, b) (((a)>(b))?(a):(b))
#endif

// -----------------------------------------------------------------------------

//MIDL hint for use with the midl compiler to generate Redstone RPC layer
//defined to do nothing for everyone else
#define MIDLHINT1(hint)
#define MIDLHINT2(hinta,hintb)

// -----------------------------------------------------------------------------
#include "unixapi.h"
// -----------------------------------------------------------------------------

class FloatParts {
public:
        ~FloatParts(void) { }
        FloatParts(float init) : f(init) { }

        bool    IsZero(void) const { return (parts.exponent == 0) && (parts.mantissa == 0); }
        bool    IsNan(void) const { return parts.exponent == 255; }

        bool    Sign(void) const { return parts.sign; }
        uint32  Mantissa(void) const { return parts.mantissa | ((parts.exponent == 0) ? 0 : 0x00800000); }
        int32   SignedMantissa(void) const { if(Sign()) return -(int32)Mantissa(); else return (int32)Mantissa(); }
        int             Exponent(void) const { return (int)parts.exponent - 127; }

        union {
                float f;
                struct {
#ifdef BIGENDIAN_OS
                        uint sign:1;
                        uint exponent:8;
                        uint mantissa:23;
#else
                        uint mantissa:23;
                        uint exponent:8;
                        uint sign:1;
#endif
                } parts;
        };

protected:
private:
};

// -----------------------------------------------------------------------------

#define CheckParams(high, low, sz) //Require((high<=31)&&(low>=0)&&(high>=low)&&((high-low)<sz))
#define CheckSignedParams(high, low, sz) //Require((high<=31)&&(low>=0)&&(high>low)&&((high-low)<sz))

class bits32 {
public:
    bits32(void) { }
    bits32(uint32 v) : m_data(v) { }

    uint   get_uint(int high, int low) const   { CheckParams(high, low, 32); return (uint)get_bits(high, low); }
    uint32 get_uint32(int high, int low) const { CheckParams(high, low, 32); return (uint32)get_bits(high, low); }
    uint16 get_uint16(int high, int low) const { CheckParams(high, low, 16); return (uint16)get_bits(high, low); }
    uint8  get_uint8(int high, int low) const  { CheckParams(high, low, 8);  return (uint8)get_bits(high, low); }
    bool   get_bool(int bit) const             { CheckParams(bit, bit, 1);   return get_bits(bit, bit) ? true : false; }

    int   get_int(int high, int low) const   { CheckSignedParams(high, low, 32); return (int)get_bits(high, low); }
    int32 get_int32(int high, int low) const { CheckSignedParams(high, low, 32); return (int32)get_bits(high, low); }
    int16 get_int16(int high, int low) const { CheckSignedParams(high, low, 16); return (int16)get_bits(high, low); }
    int8  get_int8(int high, int low) const  { CheckSignedParams(high, low, 8);  return (int8)get_bits(high, low); }

    void set_uint(int high, int low, uint value)     { CheckParams(high, low, 32); set_bits(high, low, (uint32)value); }
    void set_uint32(int high, int low, uint32 value) { CheckParams(high, low, 32); set_bits(high, low, (uint32)value); }
    void set_uint16(int high, int low, uint32 value) { CheckParams(high, low, 16); set_bits(high, low, (uint32)value); }
    void set_uint8(int high, int low, uint32 value)  { CheckParams(high, low, 8);  set_bits(high, low, (uint32)value); }
    void set_bool(int bit, bool value)               { CheckParams(bit, bit, 1);   set_bits(bit, bit, (uint32)value); }

    void set_int(int high, int low, int value)     { CheckSignedParams(high, low, 32); set_bits(high, low, (uint32)value); }
    void set_int32(int high, int low, int32 value) { CheckSignedParams(high, low, 32); set_bits(high, low, (uint32)value); }
    void set_int16(int high, int low, int32 value) { CheckSignedParams(high, low, 16); set_bits(high, low, (uint32)value); }
    void set_int8(int high, int low, int32 value)  { CheckSignedParams(high, low, 8);  set_bits(high, low, (uint32)value); }

private:
    uint32 m_data;

    uint32 get_bits(int high, int low) const
    {
        int sz = high - low + 1;
        uint32 m = mask(sz);
        return (m_data >> low) & m;
    }

    void set_bits(int high, int low, uint32 value)
    {
        int sz = high - low + 1;
        uint32 m = mask(sz) << low;
        m_data = (m_data & ~m) | ((value << low) & m);
    }

    static uint32 mask(int size)
    {
        if(size==32)
            return 0xFFFFFFFF;
        else
            return (1u << size) - 1;
    }
};

class bits64 {
public:
    bits64(void) { }
    bits64(uint64 v) : m_data(v) { }

    uint64 get_uint64(int high, int low) const { CheckParams(high, low, 64); return (uint64)get_bits(high, low); }
    uint32 get_uint32(int high, int low) const { CheckParams(high, low, 32); return (uint32)get_bits(high, low); }
    uint   get_uint(int high, int low) const   { CheckParams(high, low, 32); return (uint)get_bits(high, low); }
    uint16 get_uint16(int high, int low) const { CheckParams(high, low, 16); return (uint16)get_bits(high, low); }
    uint8  get_uint8(int high, int low) const  { CheckParams(high, low, 8);  return (uint8)get_bits(high, low); }
    bool   get_bool(int bit) const             { CheckParams(bit, bit, 1);   return get_bits(bit, bit) ? true : false; }

    int64 get_int64(int high, int low) const { CheckSignedParams(high, low, 64); return (int64)get_bits(high, low); }
    int32 get_int32(int high, int low) const { CheckSignedParams(high, low, 32); return (int32)get_bits(high, low); }
    int   get_int(int high, int low) const   { CheckSignedParams(high, low, 32); return (int)get_bits(high, low); }
    int16 get_int16(int high, int low) const { CheckSignedParams(high, low, 16); return (int16)get_bits(high, low); }
    int8  get_int8(int high, int low) const  { CheckSignedParams(high, low, 8);  return (int8)get_bits(high, low); }

    void set_uint64(int high, int low, uint64 value) { CheckParams(high, low, 64); set_bits(high, low, (uint64)value); }
    void set_uint32(int high, int low, uint32 value) { CheckParams(high, low, 32); set_bits(high, low, (uint64)value); }
    void set_uint(int high, int low, uint value)     { CheckParams(high, low, 32); set_bits(high, low, (uint64)value); }
    void set_uint16(int high, int low, uint32 value) { CheckParams(high, low, 16); set_bits(high, low, (uint64)value); }
    void set_uint8(int high, int low, uint32 value)  { CheckParams(high, low, 8);  set_bits(high, low, (uint64)value); }
    void set_bool(int bit, bool value)               { CheckParams(bit, bit, 1);   set_bits(bit,  bit, (uint64)value); }

    void set_int64(int high, int low, int64 value) { CheckSignedParams(high, low, 64); set_bits(high, low, (uint64)value); }
    void set_int(int high, int low, int value)     { CheckSignedParams(high, low, 32); set_bits(high, low, (uint64)value); }
    void set_int32(int high, int low, int32 value) { CheckSignedParams(high, low, 32); set_bits(high, low, (uint64)value); }
    void set_int16(int high, int low, int32 value) { CheckSignedParams(high, low, 16); set_bits(high, low, (uint64)value); }
    void set_int8(int high, int low, int32 value)  { CheckSignedParams(high, low, 8);  set_bits(high, low, (uint64)value); }

private:
    uint64 m_data;

    uint64 get_bits(int high, int low) const
    {
        int sz = high - low + 1;
        uint64 m = mask(sz);
        return (m_data >> low) & m;
    }

    void set_bits(int high, int low, uint64 value)
    {
        int sz = high - low + 1;
        uint64 m = mask(sz) << low;
        m_data = (m_data & ~m) | ((value << low) & m);
    }

    static uint64 mask(int size)
    {
        if(size==64)
            return CONST64(0xFFFFFFFFFFFF);
        else
            return ((uint64)CONST64(1) << size) - 1;
    }
};

#undef CheckParams
#undef CheckSignedParams

// -----------------------------------------------------------------------------

template<class TYPE>
inline bool extract_bit(TYPE data, uint bit)
{
    return ((data >> bit) & 1) ? true : false;
}

typedef union {
  uint32 i;
  float f;
} uintfloat32;

// -----------------------------------------------------------------------------
#endif
