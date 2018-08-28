/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#pragma once

typedef enum IT_OpCodeType {
    IT_NOP                                             = 0x00000010,
    IT_SET_BASE                                        = 0x00000011,
    IT_CLEAR_STATE                                     = 0x00000012,
    IT_INDEX_BUFFER_SIZE                               = 0x00000013,
    IT_DISPATCH_DIRECT                                 = 0x00000015,
    IT_DISPATCH_INDIRECT                               = 0x00000016,
    IT_INDIRECT_BUFFER_END                             = 0x00000017,
    IT_INDIRECT_BUFFER_CNST_END                        = 0x00000019,
    IT_ATOMIC_GDS                                      = 0x0000001d,
    IT_ATOMIC_MEM                                      = 0x0000001e,
    IT_OCCLUSION_QUERY                                 = 0x0000001f,
    IT_SET_PREDICATION                                 = 0x00000020,
    IT_REG_RMW                                         = 0x00000021,
    IT_COND_EXEC                                       = 0x00000022,
    IT_PRED_EXEC                                       = 0x00000023,
    IT_DRAW_INDIRECT                                   = 0x00000024,
    IT_DRAW_INDEX_INDIRECT                             = 0x00000025,
    IT_INDEX_BASE                                      = 0x00000026,
    IT_DRAW_INDEX_2                                    = 0x00000027,
    IT_CONTEXT_CONTROL                                 = 0x00000028,
    IT_INDEX_TYPE                                      = 0x0000002a,
    IT_DRAW_INDIRECT_MULTI                             = 0x0000002c,
    IT_DRAW_INDEX_AUTO                                 = 0x0000002d,
    IT_NUM_INSTANCES                                   = 0x0000002f,
    IT_DRAW_INDEX_MULTI_AUTO                           = 0x00000030,
    IT_INDIRECT_BUFFER_PRIV                            = 0x00000032,
    IT_INDIRECT_BUFFER_CNST                            = 0x00000033,
    IT_COND_INDIRECT_BUFFER_CNST                       = 0x00000033,
    IT_STRMOUT_BUFFER_UPDATE                           = 0x00000034,
    IT_DRAW_INDEX_OFFSET_2                             = 0x00000035,
    IT_DRAW_PREAMBLE                                   = 0x00000036,
    IT_WRITE_DATA                                      = 0x00000037,
    IT_DRAW_INDEX_INDIRECT_MULTI                       = 0x00000038,
    IT_MEM_SEMAPHORE                                   = 0x00000039,
    IT_DRAW_INDEX_MULTI_INST                           = 0x0000003a,
    IT_COPY_DW                                         = 0x0000003b,
    IT_WAIT_REG_MEM                                    = 0x0000003c,
    IT_INDIRECT_BUFFER                                 = 0x0000003f,
    IT_COND_INDIRECT_BUFFER                            = 0x0000003f,
    IT_COPY_DATA                                       = 0x00000040,
    IT_CP_DMA                                          = 0x00000041,
    IT_PFP_SYNC_ME                                     = 0x00000042,
    IT_SURFACE_SYNC                                    = 0x00000043,
    IT_ME_INITIALIZE                                   = 0x00000044,
    IT_COND_WRITE                                      = 0x00000045,
    IT_EVENT_WRITE                                     = 0x00000046,
    IT_EVENT_WRITE_EOP                                 = 0x00000047,
    IT_EVENT_WRITE_EOS                                 = 0x00000048,
    IT_RELEASE_MEM                                     = 0x00000049,
    IT_PREAMBLE_CNTL                                   = 0x0000004a,
    IT_DRAW_RESERVED0                                  = 0x0000004c,
    IT_DRAW_RESERVED1                                  = 0x0000004d,
    IT_DRAW_RESERVED2                                  = 0x0000004e,
    IT_DRAW_RESERVED3                                  = 0x0000004f,
    IT_DMA_DATA                                        = 0x00000050,
    IT_CONTEXT_REG_RMW                                 = 0x00000051,
    IT_GFX_CNTX_UPDATE                                 = 0x00000052,
    IT_BLK_CNTX_UPDATE                                 = 0x00000053,
    IT_INCR_UPDT_STATE                                 = 0x00000055,
    IT_ACQUIRE_MEM                                     = 0x00000058,
    IT_REWIND                                          = 0x00000059,
    IT_INTERRUPT                                       = 0x0000005a,
    IT_GEN_PDEPTE                                      = 0x0000005b,
    IT_INDIRECT_BUFFER_PASID                           = 0x0000005c,
    IT_PRIME_UTCL2                                     = 0x0000005d,
    IT_LOAD_UCONFIG_REG                                = 0x0000005e,
    IT_LOAD_SH_REG                                     = 0x0000005f,
    IT_LOAD_CONFIG_REG                                 = 0x00000060,
    IT_LOAD_CONTEXT_REG                                = 0x00000061,
    IT_LOAD_COMPUTE_STATE                              = 0x00000062,
    IT_LOAD_SH_REG_INDEX                               = 0x00000063,
    IT_SET_CONFIG_REG                                  = 0x00000068,
    IT_SET_CONTEXT_REG                                 = 0x00000069,
    IT_SET_CONTEXT_REG_INDEX                           = 0x0000006a,
    IT_SET_VGPR_REG_DI_MULTI                           = 0x00000071,
    IT_SET_SH_REG_DI                                   = 0x00000072,
    IT_SET_CONTEXT_REG_INDIRECT                        = 0x00000073,
    IT_SET_SH_REG_DI_MULTI                             = 0x00000074,
    IT_GFX_PIPE_LOCK                                   = 0x00000075,
    IT_SET_SH_REG                                      = 0x00000076,
    IT_SET_SH_REG_OFFSET                               = 0x00000077,
    IT_SET_QUEUE_REG                                   = 0x00000078,
    IT_SET_UCONFIG_REG                                 = 0x00000079,
    IT_SET_UCONFIG_REG_INDEX                           = 0x0000007a,
    IT_FORWARD_HEADER                                  = 0x0000007c,
    IT_SCRATCH_RAM_WRITE                               = 0x0000007d,
    IT_SCRATCH_RAM_READ                                = 0x0000007e,
    IT_LOAD_CONST_RAM                                  = 0x00000080,
    IT_WRITE_CONST_RAM                                 = 0x00000081,
    IT_DUMP_CONST_RAM                                  = 0x00000083,
    IT_INCREMENT_CE_COUNTER                            = 0x00000084,
    IT_INCREMENT_DE_COUNTER                            = 0x00000085,
    IT_WAIT_ON_CE_COUNTER                              = 0x00000086,
    IT_WAIT_ON_DE_COUNTER_DIFF                         = 0x00000088,
    IT_SWITCH_BUFFER                                   = 0x0000008b,
    IT_DISPATCH_DRAW_PREAMBLE                          = 0x0000008c,
    IT_DISPATCH_DRAW_PREAMBLE_ACE                      = 0x0000008c,
    IT_DISPATCH_DRAW                                   = 0x0000008d,
    IT_DISPATCH_DRAW_ACE                               = 0x0000008d,
    IT_DRAW_MULTI_PREAMBLE                             = 0x0000008f,
    IT_FRAME_CONTROL                                   = 0x00000090,
    IT_INDEX_ATTRIBUTES_INDIRECT                       = 0x00000091,
    IT_WAIT_REG_MEM64                                  = 0x00000093,
    IT_COND_PREEMPT                                    = 0x00000094,
    IT_HDP_FLUSH                                       = 0x00000095,
    IT_INVALIDATE_TLBS                                 = 0x00000098,
    IT_AQL_PACKET                                      = 0x00000099,
    IT_DMA_DATA_FILL_MULTI                             = 0x0000009a,
    IT_SET_SH_REG_INDEX                                = 0x0000009b,
    IT_DRAW_INDIRECT_COUNT_MULTI                       = 0x0000009c,
    IT_DRAW_INDEX_INDIRECT_COUNT_MULTI                 = 0x0000009d,
    IT_DUMP_CONST_RAM_OFFSET                           = 0x0000009e,
    IT_LOAD_CONTEXT_REG_INDEX                          = 0x0000009f,
    IT_SET_RESOURCES                                   = 0x000000a0,
    IT_MAP_PROCESS                                     = 0x000000a1,
    IT_MAP_QUEUES                                      = 0x000000a2,
    IT_UNMAP_QUEUES                                    = 0x000000a3,
    IT_QUERY_STATUS                                    = 0x000000a4,
    IT_RUN_LIST                                        = 0x000000a5,
    IT_MAP_PROCESS_VM                                  = 0x000000a6,
    IT_GET_LOD_STATS__GFX09                            = 0x0000008e,
} IT_OpCodeType;

constexpr unsigned int PM4_TYPE_0                               = 0;
constexpr unsigned int PM4_TYPE_2                               = 2;
constexpr unsigned int PM4_TYPE_3                               = 3;

