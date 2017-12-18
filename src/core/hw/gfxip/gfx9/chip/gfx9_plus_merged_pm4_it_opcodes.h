/*
 *******************************************************************************
 *
 * Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

enum IT_OpCodeType {
    IT_NOP                               = 0x10,
    IT_SET_BASE                          = 0x11,
    IT_CLEAR_STATE                       = 0x12,
    IT_INDEX_BUFFER_SIZE                 = 0x13,
    IT_DISPATCH_DIRECT                   = 0x15,
    IT_DISPATCH_INDIRECT                 = 0x16,
    IT_ATOMIC_MEM                        = 0x1E,
    IT_OCCLUSION_QUERY                   = 0x1F,
    IT_SET_PREDICATION                   = 0x20,
    IT_REG_RMW                           = 0x21,
    IT_COND_EXEC                         = 0x22,
    IT_DRAW_INDIRECT                     = 0x24,
    IT_DRAW_INDEX_INDIRECT               = 0x25,
    IT_INDEX_BASE                        = 0x26,
    IT_DRAW_INDEX_2                      = 0x27,
    IT_CONTEXT_CONTROL                   = 0x28,
    IT_DRAW_INDIRECT_MULTI               = 0x2C,
    IT_DRAW_INDEX_AUTO                   = 0x2D,
    IT_NUM_INSTANCES                     = 0x2F,
    IT_INDIRECT_BUFFER_CNST              = 0x33,
    IT_STRMOUT_BUFFER_UPDATE             = 0x34,
    IT_DRAW_INDEX_OFFSET_2               = 0x35,
    IT_WRITE_DATA                        = 0x37,
    IT_DRAW_INDEX_INDIRECT_MULTI         = 0x38,
    IT_WAIT_REG_MEM                      = 0x3C,
    IT_INDIRECT_BUFFER                   = 0x3F,
    IT_COND_INDIRECT_BUFFER              = 0x3F,
    IT_COPY_DATA                         = 0x40,
    IT_PFP_SYNC_ME                       = 0x42,
    IT_EVENT_WRITE                       = 0x46,
    IT_RELEASE_MEM                       = 0x49,
    IT_PREAMBLE_CNTL                     = 0x4A,
    IT_DMA_DATA                          = 0x50,
    IT_CONTEXT_REG_RMW                   = 0x51,
    IT_ACQUIRE_MEM                       = 0x58,
    IT_REWIND                            = 0x59,
    IT_PRIME_UTCL2                       = 0x5D,
    IT_LOAD_UCONFIG_REG                  = 0x5E,
    IT_LOAD_SH_REG                       = 0x5F,
    IT_LOAD_CONFIG_REG                   = 0x60,
    IT_LOAD_CONTEXT_REG                  = 0x61,
    IT_LOAD_SH_REG_INDEX                 = 0x63,
    IT_SET_CONTEXT_REG                   = 0x69,
    IT_SET_SH_REG                        = 0x76,
    IT_SET_SH_REG_OFFSET                 = 0x77,
    IT_SET_UCONFIG_REG                   = 0x79,
    IT_LOAD_CONST_RAM                    = 0x80,
    IT_WRITE_CONST_RAM                   = 0x81,
    IT_DUMP_CONST_RAM                    = 0x83,
    IT_INCREMENT_CE_COUNTER              = 0x84,
    IT_INCREMENT_DE_COUNTER              = 0x85,
    IT_WAIT_ON_CE_COUNTER                = 0x86,
    IT_WAIT_ON_DE_COUNTER_DIFF           = 0x88,
    IT_INDEX_ATTRIBUTES_INDIRECT         = 0x91,
    IT_WAIT_REG_MEM64                    = 0x93,
    IT_SET_SH_REG_INDEX                  = 0x9B,
    IT_DUMP_CONST_RAM_OFFSET             = 0x9E,
    IT_LOAD_CONTEXT_REG_INDEX            = 0x9F,
};

#define PM4_TYPE_0 0
#define PM4_TYPE_2 2
#define PM4_TYPE_3 3

