/*      CWPack - cwpack_defines.h   */
/*
 The MIT License (MIT)

 Copyright (c) 2017 Claes Wihlborg

 Permission is hereby granted, free of charge, to any person obtaining a copy of this
 software and associated documentation files (the "Software"), to deal in the Software
 without restriction, including without limitation the rights to use, copy, modify,
 merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 persons to whom the Software is furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


#ifndef cwpack_defines_h
#define cwpack_defines_h



/*************************   A L I G N M E N T   ******************************/

/*
 * Sometime the processor demands that integer access is to an even memory address.
 * In that case define FORCE_ALIGNMENT
 */

/* #define FORCE_ALIGNMENT */


/*************************   C   S Y S T E M   L I B R A R Y   ****************/

/*
 * The packer uses "memcpy" to move blobs. If you dont want to load C system library
 * for just that, define FORCE_NO_LIBRARY and CWPack will use an internal "memcpy"
 */

/* #define FORCE_NO_LIBRARY */



/*************************   B Y T E   O R D E R   ****************************/

/*
 * The pack/unpack routines are written in three versions: for big endian, for
 * little endian and insensitive to byte order. As you can get some speed gain
 * if the byte order is known, we try that when we can certainly detect it.
 * Define COMPILE_FOR_BIG_ENDIAN or COMPILE_FOR_LITTLE_ENDIAN if you know.
 */

#if !defined(FORCE_ALIGNMENT) && !defined(COMPILE_FOR_BIG_ENDIAN) && !defined(COMPILE_FOR_LITTLE_ENDIAN)
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && defined(__ORDER_BIG_ENDIAN__)

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define COMPILE_FOR_BIG_ENDIAN
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define COMPILE_FOR_LITTLE_ENDIAN
#endif

#elif defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && defined(__BIG_ENDIAN)

#if __BYTE_ORDER == __BIG_ENDIAN
#define COMPILE_FOR_BIG_ENDIAN
#elif __BYTE_ORDER == __LITTLE_ENDIAN
#define COMPILE_FOR_LITTLE_ENDIAN
#endif

#elif defined(__BIG_ENDIAN__)
#define COMPILE_FOR_BIG_ENDIAN

#elif defined(__LITTLE_ENDIAN__)
#define COMPILE_FOR_LITTLE_ENDIAN

#elif defined(__i386__) || defined(__x86_64__)
#define COMPILE_FOR_LITTLE_ENDIAN

#endif
#endif

//#undef COMPILE_FOR_LITTLE_ENDIAN


/*******************************   P A C K   **********************************/



#define PACK_ERROR(error_code)                          \
{                                                       \
    pack_context->return_code = error_code;             \
    return;                                             \
}



#ifdef COMPILE_FOR_BIG_ENDIAN

#define cw_store16(x)  *(uint16_t*)p = *(uint16_t*)&x;
#define cw_store32(x)  *(uint32_t*)p = *(uint32_t*)&x;
#define cw_store64(x)  *(uint64_t*)p = *(uint64_t*)&x;

#else    /* Byte order little endian or undetermined */

#ifdef COMPILE_FOR_LITTLE_ENDIAN

#define cw_store16(d)                                       \
    *(uint16_t*)p = (uint16_t)((((d) >> 8) & 0x0ff) | (d) << 8)

#define cw_store32(x)                                       \
    *(uint32_t*)p =                                         \
        ((uint32_t)((((uint32_t)(x)) >> 24) |               \
        (((uint32_t)(x) & 0x00ff0000) >>  8) |              \
        (((uint32_t)(x) & 0x0000ff00) <<  8) |              \
        (((uint32_t)(x)) << 24)));                          \

#define cw_store64(x)                                       \
    *(uint64_t*)p =                                         \
        ((uint64_t)(                                        \
        (((((uint64_t)(x)) >> 40) |                         \
        (((uint64_t)(x)) << 24)) & 0x0000ff000000ff00ULL) | \
        (((((uint64_t)(x)) >> 24) |                         \
        (((uint64_t)(x)) << 40)) & 0x00ff000000ff0000ULL) | \
        (((uint64_t)(x) & 0x000000ff00000000ULL) >>  8) |   \
        (((uint64_t)(x) & 0x00000000ff000000ULL) <<  8) |   \
        (((uint64_t)(x)) >> 56) |                           \
        (((uint64_t)(x)) << 56)));                          \

#else   /* Byte order undetermined */

#define cw_store16(d)           \
    *p = (uint8_t)(d >> 8);     \
    p[1] = (uint8_t)d;

#define cw_store32(d)           \
    *p = (uint8_t)(d >> 24);    \
    p[1] = (uint8_t)(d >> 16);  \
    p[2] = (uint8_t)(d >> 8);   \
    p[3] = (uint8_t)d;

#define cw_store64(z)           \
    *p = (uint8_t)(z >> 56);    \
    p[1] = (uint8_t)(z >> 48);  \
    p[2] = (uint8_t)(z >> 40);  \
    p[3] = (uint8_t)(z >> 32);  \
    p[4] = (uint8_t)(z >> 24);  \
    p[5] = (uint8_t)(z >> 16);  \
    p[6] = (uint8_t)(z >> 8);   \
    p[7] = (uint8_t)z;
#endif
#endif



#define cw_pack_reserve_space(more)                                                         \
{                                                                                           \
    p = pack_context->current;                                                              \
    uint8_t* nyp = p + more;                                                                \
    if (nyp > pack_context->end)                                                            \
    {                                                                                       \
        if (!pack_context->handle_pack_overflow)                                            \
            PACK_ERROR(CWP_RC_BUFFER_OVERFLOW)                                              \
        int rc = pack_context->handle_pack_overflow (pack_context, (unsigned long)(more));  \
        if (rc)                                                                             \
            PACK_ERROR(rc)                                                                  \
        p = pack_context->current;                                                          \
        nyp = p + more;                                                                     \
    }                                                                                       \
    pack_context->current = nyp;                                                            \
}


#define tryMove0(t)                                     \
{                                                       \
    uint8_t *p;                                         \
    cw_pack_reserve_space(1)                            \
    *p = (uint8_t)(t);                                  \
    return;                                             \
}

#define tryMove1(t,d)                                   \
{                                                       \
    uint8_t *p;                                         \
    cw_pack_reserve_space(2)                            \
    *p++ = (uint8_t)t;                                  \
    *p = (uint8_t)d;                                    \
    return;                                             \
}

#define tryMove2(t,d)                                   \
{                                                       \
    uint8_t *p;                                         \
    cw_pack_reserve_space(3)                            \
    *p++ = (uint8_t)t;                                  \
    cw_store16(d);                                      \
    return;                                             \
}

#define tryMove4(t,d)                                   \
{                                                       \
    uint8_t *p;                                         \
    cw_pack_reserve_space(5)                            \
    *p++ = (uint8_t)t;                                  \
    cw_store32(d);                                      \
    return;                                             \
}

#define tryMove8(t,d)                                   \
{                                                       \
    uint8_t *p;                                         \
    cw_pack_reserve_space(9)                            \
    *p++ = (uint8_t)t;                                  \
    cw_store64(d);                                      \
    return;                                             \
}




/*******************************   U N P A C K   **********************************/



#define UNPACK_ERROR(error_code)                        \
{                                                       \
    unpack_context->item.type = CWP_NOT_AN_ITEM;        \
    unpack_context->return_code = error_code;           \
    return;                                             \
}



#ifdef COMPILE_FOR_BIG_ENDIAN

#define cw_load16(ptr)  tmpu16 = *(uint16_t*)ptr;
#define cw_load32(ptr)  tmpu32 = *(uint32_t*)ptr;
#define cw_load64(ptr)  tmpu64 = *(uint64_t*)ptr;

#else    /* Byte order little endian or undetermined */

#ifdef COMPILE_FOR_LITTLE_ENDIAN

#define cw_load16(ptr)                                          \
    tmpu16 = *(uint16_t*)ptr;                                   \
    tmpu16 = (uint16_t)((tmpu16<<8) | (tmpu16>>8))

#define cw_load32(ptr)                                          \
    tmpu32 = *(uint32_t*)ptr;                                   \
    tmpu32 = (tmpu32<<24) | ((tmpu32 & 0xff00)<<8) |            \
            ((tmpu32 & 0xff0000)>>8) | (tmpu32>>24)

#define cw_load64(ptr)                                          \
    tmpu64 = *((uint64_t*)ptr);                                 \
    tmpu64 =                   (                                \
        (((tmpu64 >> 40) |                                      \
        (tmpu64 << 24)) & 0x0000ff000000ff00ULL) |              \
        (((tmpu64 >> 24) |                                      \
        (tmpu64 << 40)) & 0x00ff000000ff0000ULL) |              \
        ((tmpu64 & 0x000000ff00000000ULL) >>  8) |              \
        ((tmpu64 & 0x00000000ff000000ULL) <<  8) |              \
        (tmpu64 >> 56) |                                        \
        (tmpu64 << 56)                              )

#else /* Byte order undetermined */

#define cw_load16(ptr)                                          \
    tmpu16 = (uint16_t)((*ptr++) << 8);                         \
    tmpu16 |= (uint16_t)(*ptr++)

#define cw_load32(ptr)                                          \
    tmpu32 = (uint32_t)(*ptr++ << 24);                          \
    tmpu32 |= (uint32_t)(*ptr++ << 16);                         \
    tmpu32 |= (uint32_t)(*ptr++ << 8);                          \
    tmpu32 |= (uint32_t)(*ptr++)

#define cw_load64(ptr)                                          \
    tmpu64 = ((uint64_t)*ptr++) << 56;                          \
    tmpu64 |= ((uint64_t)*ptr++) << 48;                         \
    tmpu64 |= ((uint64_t)*ptr++) << 40;                         \
    tmpu64 |= ((uint64_t)*ptr++) << 32;                         \
    tmpu64 |= ((uint64_t)*ptr++) << 24;                         \
    tmpu64 |= ((uint64_t)*ptr++) << 16;                         \
    tmpu64 |= ((uint64_t)*ptr++) << 8;                          \
    tmpu64 |= (uint64_t)*ptr++

#endif
#endif



#define cw_unpack_assert_space(more)                                                                \
{                                                                                                   \
    p = unpack_context->current;                                                                    \
    const uint8_t* nyp = p + more;                                                                  \
    if (nyp > unpack_context->end)                                                                  \
    {                                                                                               \
        if (!unpack_context->handle_unpack_underflow)                                               \
            UNPACK_ERROR(buffer_end_return_code)                                                    \
        int rc = unpack_context->handle_unpack_underflow (unpack_context, (unsigned long)(more));   \
        if (rc != CWP_RC_OK)                                                                        \
        {                                                                                           \
            if (rc != CWP_RC_END_OF_INPUT)                                                          \
                UNPACK_ERROR(rc)                                                                    \
            else                                                                                    \
                UNPACK_ERROR(buffer_end_return_code)                                                \
        }                                                                                           \
        p = unpack_context->current;                                                                \
        nyp = p + more;                                                                             \
    }                                                                                               \
    unpack_context->current = nyp;                                                                  \
}


#define cw_unpack_assert_blob(blob)                                         \
    cw_unpack_assert_space(unpack_context->item.as.blob.length);            \
    unpack_context->item.as.blob.start = p;                                 \
    return;


#define getDDItem(typ,var,val)                                              \
    unpack_context->item.type = typ;                                        \
    unpack_context->item.as.var = val;

#define getDDItem1(typ,var,cast)                                            \
    unpack_context->item.type = typ;                                        \
    cw_unpack_assert_space(1);                                              \
    unpack_context->item.as.var = (cast)*p;

#define getDDItem2(typ,var,cast)                                            \
    unpack_context->item.type = typ;                                        \
    cw_unpack_assert_space(2);                                              \
    cw_load16(p);                                                           \
    unpack_context->item.as.var = (cast)tmpu16;

#define getDDItem4(typ,var,cast)                                            \
    unpack_context->item.type = typ;                                        \
    cw_unpack_assert_space(4);                                              \
    cw_load32(p);                                                           \
    unpack_context->item.as.var = (cast)tmpu32;

#define getDDItem8(typ)                                                     \
    unpack_context->item.type = typ;                                        \
    cw_unpack_assert_space(8);                                              \
    cw_load64(p);                                                           \
    unpack_context->item.as.u64 = tmpu64;

#define getDDItemFix(len)                                                   \
    cw_unpack_assert_space(1);                                              \
    unpack_context->item.type = *(int8_t*)p;                                \
    unpack_context->item.as.ext.length = len;                               \
    cw_unpack_assert_blob(ext);




#endif /* cwpack_defines_h */
