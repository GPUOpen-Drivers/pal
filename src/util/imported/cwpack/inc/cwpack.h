/*      CWPack - cwpack.h   */
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

#ifndef CWPack_H__
#define CWPack_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>


#ifndef CWP_CALL
#define CWP_CALL
#endif


/*******************************   Return Codes   *****************************/

#define CWP_RC_OK                     0
#define CWP_RC_END_OF_INPUT          -1
#define CWP_RC_BUFFER_OVERFLOW       -2
#define CWP_RC_BUFFER_UNDERFLOW      -3
#define CWP_RC_MALFORMED_INPUT       -4
#define CWP_RC_WRONG_BYTE_ORDER      -5
#define CWP_RC_ERROR_IN_HANDLER      -6
#define CWP_RC_ILLEGAL_CALL          -7
#define CWP_RC_MALLOC_ERROR          -8
#define CWP_RC_STOPPED               -9



/*******************************   P A C K   **********************************/

struct cw_pack_context;

typedef int (CWP_CALL *pack_overflow_handler)(struct cw_pack_context*, unsigned long);

typedef struct cw_pack_context {
    uint8_t*                start;
    uint8_t*                current;
    uint8_t*                end;
    bool                    be_compatible;
    int                     return_code;
    int                     err_no;          /* handlers can save error here */
    pack_overflow_handler   handle_pack_overflow;
    void*                   client_data;
} cw_pack_context;


int CWP_CALL cw_pack_context_init (cw_pack_context* pack_context, void* data, unsigned long length, pack_overflow_handler hpo, void* client_data);
void CWP_CALL cw_pack_set_compatibility (cw_pack_context* pack_context, bool be_compatible);

void CWP_CALL cw_pack_nil (cw_pack_context* pack_context);
void CWP_CALL cw_pack_true (cw_pack_context* pack_context);
void CWP_CALL cw_pack_false (cw_pack_context* pack_context);
void CWP_CALL cw_pack_boolean (cw_pack_context* pack_context, bool b);

void CWP_CALL cw_pack_signed (cw_pack_context* pack_context, int64_t i);
void CWP_CALL cw_pack_unsigned (cw_pack_context* pack_context, uint64_t i);

void CWP_CALL cw_pack_float (cw_pack_context* pack_context, float f);
void CWP_CALL cw_pack_double (cw_pack_context* pack_context, double d);
void CWP_CALL cw_pack_real (cw_pack_context* pack_context, double d);   /* Pack as float if precision isn't destroyed */

void CWP_CALL cw_pack_array_size (cw_pack_context* pack_context, uint32_t n);
void CWP_CALL cw_pack_map_size (cw_pack_context* pack_context, uint32_t n);
void CWP_CALL cw_pack_str (cw_pack_context* pack_context, const char* v, uint32_t l);
void CWP_CALL cw_pack_bin (cw_pack_context* pack_context, const void* v, uint32_t l);
void CWP_CALL cw_pack_ext (cw_pack_context* pack_context, int8_t type, const void* v, uint32_t l);

void CWP_CALL cw_pack_insert (cw_pack_context* pack_context, const void* v, uint32_t l);

/*****************************   U N P A C K   ********************************/


typedef enum
{
    CWP_ITEM_MIN_RESERVED_EXT       = -128,
    CWP_ITEM_MAX_RESERVED_EXT       = -1,
    CWP_ITEM_MIN_USER_EXT           = 0,
    CWP_ITEM_MAX_USER_EXT           = 127,
    CWP_ITEM_NIL                    = 300,
    CWP_ITEM_BOOLEAN                = 301,
    CWP_ITEM_POSITIVE_INTEGER       = 302,
    CWP_ITEM_NEGATIVE_INTEGER       = 303,
    CWP_ITEM_FLOAT                  = 304,
    CWP_ITEM_DOUBLE                 = 305,
    CWP_ITEM_STR                    = 306,
    CWP_ITEM_BIN                    = 307,
    CWP_ITEM_ARRAY                  = 308,
    CWP_ITEM_MAP                    = 309,
    CWP_ITEM_EXT                    = 310,
    CWP_NOT_AN_ITEM                 = 999,
} cwpack_item_types;


typedef struct {
    const void*     start;
    uint32_t        length;
} cwpack_blob;


typedef struct {
    uint32_t    size;
} cwpack_container;


typedef struct {
    cwpack_item_types   type;
    union
    {
        bool            boolean;
        uint64_t        u64;
        int64_t         i64;
        float           real;
        double          long_real;
        cwpack_container array;
        cwpack_container map;
        cwpack_blob     str;
        cwpack_blob     bin;
        cwpack_blob     ext;
    } as;
} cwpack_item;

struct cw_unpack_context;

typedef int (CWP_CALL *unpack_underflow_handler)(struct cw_unpack_context*, unsigned long);

typedef struct cw_unpack_context {
    cwpack_item                 item;
    const uint8_t*              start;
    const uint8_t*              current;
    const uint8_t*              end;             /* logical end of buffer */
    int                         return_code;
    int                         err_no;          /* handlers can save error here */
    unpack_underflow_handler    handle_unpack_underflow;
    void*                       client_data;
} cw_unpack_context;



int CWP_CALL cw_unpack_context_init (cw_unpack_context* unpack_context, const void* data, unsigned long length, unpack_underflow_handler huu, void* client_data);

void CWP_CALL cw_unpack_next (cw_unpack_context* unpack_context);
void CWP_CALL cw_skip_items (cw_unpack_context* unpack_context, long item_count);



#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* CWPack_H__ */
