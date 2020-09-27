/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

//
constexpr unsigned int HEADER_AGENT_DISPATCH                    = 4;
constexpr unsigned int HEADER_BARRIER                           = 5;
constexpr unsigned int SDMA_OP_AQL_BARRIER_OR                   = 0;
constexpr unsigned int SDMA_OP_AQL_COPY                         = 0;
constexpr unsigned int SDMA_OP_ATOMIC                           = 10;
constexpr unsigned int SDMA_OP_COND_EXE                         = 9;
constexpr unsigned int SDMA_OP_CONST_FILL                       = 11;
constexpr unsigned int SDMA_OP_COPY                             = 1;
constexpr unsigned int SDMA_OP_DUMMY_TRAP                       = 32;
constexpr unsigned int SDMA_OP_FENCE                            = 5;
constexpr unsigned int SDMA_OP_GCR_REQ                          = 17;
constexpr unsigned int SDMA_OP_GPUVM_INV                        = 16;
constexpr unsigned int SDMA_OP_INDIRECT                         = 4;
constexpr unsigned int SDMA_OP_NOP                              = 0;
constexpr unsigned int SDMA_OP_POLL_REGMEM                      = 8;
constexpr unsigned int SDMA_OP_PRE_EXE                          = 15;
constexpr unsigned int SDMA_OP_SEM                              = 7;
constexpr unsigned int SDMA_OP_SRBM_WRITE                       = 14;
constexpr unsigned int SDMA_OP_TIMESTAMP                        = 13;
constexpr unsigned int SDMA_OP_TRAP                             = 6;
constexpr unsigned int SDMA_OP_WRITE                            = 2;
constexpr unsigned int SDMA_SUBOP_COPY_DIRTY_PAGE               = 7;
constexpr unsigned int SDMA_SUBOP_COPY_LINEAR                   = 0;
constexpr unsigned int SDMA_SUBOP_COPY_LINEAR_PHY               = 8;
constexpr unsigned int SDMA_SUBOP_COPY_LINEAR_SUB_WIND          = 4;
constexpr unsigned int SDMA_SUBOP_COPY_SOA                      = 3;
constexpr unsigned int SDMA_SUBOP_COPY_T2T_SUB_WIND             = 6;
constexpr unsigned int SDMA_SUBOP_COPY_TILED                    = 1;
constexpr unsigned int SDMA_SUBOP_COPY_TILED_SUB_WIND           = 5;
constexpr unsigned int SDMA_SUBOP_DATA_FILL_MULTI               = 1;
constexpr unsigned int SDMA_SUBOP_MEM_INCR                      = 1;
constexpr unsigned int SDMA_SUBOP_POLL_DBIT_WRITE_MEM           = 2;
constexpr unsigned int SDMA_SUBOP_POLL_MEM_VERIFY               = 3;
constexpr unsigned int SDMA_SUBOP_POLL_REG_WRITE_MEM            = 1;
constexpr unsigned int SDMA_SUBOP_TIMESTAMP_GET                 = 1;
constexpr unsigned int SDMA_SUBOP_TIMESTAMP_GET_GLOBAL          = 2;
constexpr unsigned int SDMA_SUBOP_TIMESTAMP_SET                 = 0;
constexpr unsigned int SDMA_SUBOP_WRITE_LINEAR                  = 0;
constexpr unsigned int SDMA_SUBOP_WRITE_TILED                   = 1;

namespace Gfx101
{
    constexpr unsigned int SDMA_SUBOP_INVALIDATION                  = 4;
} // namespace Gfx101

typedef struct SDMA_PKT_ATOMIC_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int                                :  8;
            unsigned int loop                           :  1;
            unsigned int                                :  1;
            unsigned int tmz                            :  1;
            unsigned int                                :  6;
            unsigned int atomic_op                      :  7;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int addr_31_0                      : 32;
        };
        unsigned int DW_1_DATA;
    } ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int addr_63_32                     : 32;
        };
        unsigned int DW_2_DATA;
    } ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int src_data_31_0                  : 32;
        };
        unsigned int DW_3_DATA;
    } SRC_DATA_LO_UNION;

    union
    {
        struct
        {
            unsigned int src_data_63_32                 : 32;
        };
        unsigned int DW_4_DATA;
    } SRC_DATA_HI_UNION;

    union
    {
        struct
        {
            unsigned int cmp_data_31_0                  : 32;
        };
        unsigned int DW_5_DATA;
    } CMP_DATA_LO_UNION;

    union
    {
        struct
        {
            unsigned int cmp_data_63_32                 : 32;
        };
        unsigned int DW_6_DATA;
    } CMP_DATA_HI_UNION;

    union
    {
        struct
        {
            unsigned int loop_interval                  : 13;
            unsigned int                                : 19;
        };
        unsigned int DW_7_DATA;
    } LOOP_INTERVAL_UNION;

} SDMA_PKT_ATOMIC;

typedef struct SDMA_PKT_COND_EXE_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int                                : 16;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int addr_31_0                      : 32;
        };
        unsigned int DW_1_DATA;
    } ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int addr_63_32                     : 32;
        };
        unsigned int DW_2_DATA;
    } ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int reference                      : 32;
        };
        unsigned int DW_3_DATA;
    } REFERENCE_UNION;

    union
    {
        struct
        {
            unsigned int exec_count                     : 14;
            unsigned int                                : 18;
        };
        unsigned int DW_4_DATA;
    } EXEC_COUNT_UNION;

} SDMA_PKT_COND_EXE;

typedef struct SDMA_PKT_CONSTANT_FILL_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int sw                             :  2;
            unsigned int                                : 12;
            unsigned int fillsize                       :  2;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_31_0                  : 32;
        };
        unsigned int DW_1_DATA;
    } DST_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_63_32                 : 32;
        };
        unsigned int DW_2_DATA;
    } DST_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int src_data_31_0                  : 32;
        };
        unsigned int DW_3_DATA;
    } DATA_UNION;

    union
    {
        struct
        {
            unsigned int count                          : 22;
            unsigned int                                : 10;
        };
        unsigned int DW_4_DATA;
    } COUNT_UNION;

} SDMA_PKT_CONSTANT_FILL;

typedef struct SDMA_PKT_COPY_BROADCAST_LINEAR_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int encrypt                        :  1;
            unsigned int                                :  1;
            unsigned int tmz                            :  1;
            unsigned int                                :  8;
            unsigned int broadcast                      :  1;
            unsigned int                                :  2;
            unsigned int                                :  1;
            unsigned int                                :  1;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int count                          : 22;
            unsigned int                                : 10;
        } gfx101;
        unsigned int DW_1_DATA;
    } COUNT_UNION;

    union
    {
        struct
        {
            unsigned int                                :  8;
            unsigned int dst2_sw                        :  2;
            unsigned int                                :  6;
            unsigned int dst1_sw                        :  2;
            unsigned int                                :  6;
            unsigned int src_sw                         :  2;
            unsigned int                                :  6;
        };
        unsigned int DW_2_DATA;
    } PARAMETER_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_31_0                  : 32;
        };
        unsigned int DW_3_DATA;
    } SRC_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_63_32                 : 32;
        };
        unsigned int DW_4_DATA;
    } SRC_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int dst1_addr_31_0                 : 32;
        };
        unsigned int DW_5_DATA;
    } DST1_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int dst1_addr_63_32                : 32;
        };
        unsigned int DW_6_DATA;
    } DST1_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int dst2_addr_31_0                 : 32;
        };
        unsigned int DW_7_DATA;
    } DST2_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int dst2_addr_63_32                : 32;
        };
        unsigned int DW_8_DATA;
    } DST2_ADDR_HI_UNION;

} SDMA_PKT_COPY_BROADCAST_LINEAR;

typedef struct SDMA_PKT_COPY_DIRTY_PAGE_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int                                :  2;
            unsigned int tmz                            :  1;
            unsigned int                                : 12;
            unsigned int all                            :  1;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int count                          : 22;
            unsigned int                                : 10;
        };
        unsigned int DW_1_DATA;
    } COUNT_UNION;

    union
    {
        struct
        {
            unsigned int                                :  3;
            unsigned int dst_mtype                      :  3;
            unsigned int dst_l2_policy                  :  2;
            unsigned int                                :  3;
            unsigned int src_mtype                      :  3;
            unsigned int src_l2_policy                  :  2;
            unsigned int                                :  3;
            unsigned int dst_gcc                        :  1;
            unsigned int dst_sys                        :  1;
            unsigned int                                :  1;
            unsigned int dst_snoop                      :  1;
            unsigned int dst_gpa                        :  1;
            unsigned int src_sw                         :  2;
            unsigned int                                :  2;
            unsigned int src_sys                        :  1;
            unsigned int                                :  1;
            unsigned int src_snoop                      :  1;
            unsigned int src_gpa                        :  1;
        };
        struct
        {
            unsigned int                                : 16;
            unsigned int dst_sw                         :  2;
            unsigned int                                : 14;
        } gfx101;
        unsigned int DW_2_DATA;
    } PARAMETER_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_31_0                  : 32;
        };
        unsigned int DW_3_DATA;
    } SRC_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_63_32                 : 32;
        };
        unsigned int DW_4_DATA;
    } SRC_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_31_0                  : 32;
        };
        unsigned int DW_5_DATA;
    } DST_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_63_32                 : 32;
        };
        unsigned int DW_6_DATA;
    } DST_ADDR_HI_UNION;

} SDMA_PKT_COPY_DIRTY_PAGE;

typedef struct SDMA_PKT_COPY_L2T_BROADCAST_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int encrypt                        :  1;
            unsigned int                                :  1;
            unsigned int tmz                            :  1;
            unsigned int                                :  7;
            unsigned int videocopy                      :  1;
            unsigned int broadcast                      :  1;
            unsigned int                                :  2;
            unsigned int                                :  1;
            unsigned int                                :  1;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int tiled_addr0_31_0               : 32;
        };
        unsigned int DW_1_DATA;
    } TILED_ADDR_LO_0_UNION;

    union
    {
        struct
        {
            unsigned int tiled_addr0_63_32              : 32;
        };
        unsigned int DW_2_DATA;
    } TILED_ADDR_HI_0_UNION;

    union
    {
        struct
        {
            unsigned int tiled_addr1_31_0               : 32;
        };
        unsigned int DW_3_DATA;
    } TILED_ADDR_LO_1_UNION;

    union
    {
        struct
        {
            unsigned int tiled_addr1_63_32              : 32;
        };
        unsigned int DW_4_DATA;
    } TILED_ADDR_HI_1_UNION;

    union
    {
        struct
        {
            unsigned int width                          : 14;
            unsigned int                                : 18;
        };
        unsigned int DW_5_DATA;
    } DW_5_UNION;

    union
    {
        struct
        {
            unsigned int height                         : 14;
            unsigned int                                :  2;
            unsigned int depth                          : 13;
            unsigned int                                :  3;
        };
        unsigned int DW_6_DATA;
    } DW_6_UNION;

    union
    {
        struct
        {
            unsigned int element_size                   :  3;
            unsigned int swizzle_mode                   :  5;
            unsigned int                                :  1;
            unsigned int dimension                      :  2;
            unsigned int                                : 21;
        };
        struct
        {
            unsigned int                                : 16;
            unsigned int mip_max                        :  4;
            unsigned int                                : 12;
        } gfx101;
        unsigned int DW_7_DATA;
    } DW_7_UNION;

    union
    {
        struct
        {
            unsigned int x                              : 14;
            unsigned int                                :  2;
            unsigned int y                              : 14;
            unsigned int                                :  2;
        };
        unsigned int DW_8_DATA;
    } DW_8_UNION;

    union
    {
        struct
        {
            unsigned int z                              : 13;
            unsigned int                                : 19;
        };
        unsigned int DW_9_DATA;
    } DW_9_UNION;

    union
    {
        struct
        {
            unsigned int                                :  8;
            unsigned int dst2_sw                        :  2;
            unsigned int                                :  6;
            unsigned int linear_sw                      :  2;
            unsigned int                                :  6;
            unsigned int tile_sw                        :  2;
            unsigned int                                :  6;
        };
        unsigned int DW_10_DATA;
    } DW_10_UNION;

    union
    {
        struct
        {
            unsigned int linear_addr_31_0               : 32;
        };
        unsigned int DW_11_DATA;
    } LINEAR_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int linear_addr_63_32              : 32;
        };
        unsigned int DW_12_DATA;
    } LINEAR_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int linear_pitch                   : 19;
            unsigned int                                : 13;
        };
        unsigned int DW_13_DATA;
    } LINEAR_PITCH_UNION;

    union
    {
        struct
        {
            unsigned int linear_slice_pitch             : 32;
        };
        unsigned int DW_14_DATA;
    } LINEAR_SLICE_PITCH_UNION;

    union
    {
        struct
        {
            unsigned int count                          : 22;
            unsigned int                                : 10;
        } gfx101;
        unsigned int DW_15_DATA;
    } COUNT_UNION;

} SDMA_PKT_COPY_L2T_BROADCAST;

typedef struct SDMA_PKT_COPY_LINEAR_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int encrypt                        :  1;
            unsigned int                                :  1;
            unsigned int tmz                            :  1;
            unsigned int                                :  6;
            unsigned int backwards                      :  1;
            unsigned int                                :  4;
            unsigned int                                :  1;
            unsigned int                                :  1;
        };
        struct
        {
            unsigned int                                : 27;
            unsigned int broadcast                      :  1;
            unsigned int                                :  4;
        } gfx101;
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int count                          : 22;
            unsigned int                                : 10;
        } gfx101;
        unsigned int DW_1_DATA;
    } COUNT_UNION;

    union
    {
        struct
        {
            unsigned int                                : 16;
            unsigned int dst_sw                         :  2;
            unsigned int                                :  6;
            unsigned int src_sw                         :  2;
            unsigned int                                :  6;
        };
        unsigned int DW_2_DATA;
    } PARAMETER_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_31_0                  : 32;
        };
        unsigned int DW_3_DATA;
    } SRC_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_63_32                 : 32;
        };
        unsigned int DW_4_DATA;
    } SRC_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_31_0                  : 32;
        };
        unsigned int DW_5_DATA;
    } DST_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_63_32                 : 32;
        };
        unsigned int DW_6_DATA;
    } DST_ADDR_HI_UNION;

} SDMA_PKT_COPY_LINEAR;

typedef struct SDMA_PKT_COPY_LINEAR_SUBWIN_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int                                :  2;
            unsigned int tmz                            :  1;
            unsigned int                                : 10;
            unsigned int elementsize                    :  3;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_31_0                  : 32;
        };
        unsigned int DW_1_DATA;
    } SRC_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_63_32                 : 32;
        };
        unsigned int DW_2_DATA;
    } SRC_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int src_x                          : 14;
            unsigned int                                :  2;
            unsigned int src_y                          : 14;
            unsigned int                                :  2;
        };
        unsigned int DW_3_DATA;
    } DW_3_UNION;

    union
    {
        struct
        {
            unsigned int src_z                          : 13;
            unsigned int src_pitch                      : 19;
        };
        unsigned int DW_4_DATA;
    } DW_4_UNION;

    union
    {
        struct
        {
            unsigned int src_slice_pitch                : 28;
            unsigned int                                :  4;
        };
        unsigned int DW_5_DATA;
    } DW_5_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_31_0                  : 32;
        };
        unsigned int DW_6_DATA;
    } DST_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_63_32                 : 32;
        };
        unsigned int DW_7_DATA;
    } DST_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int dst_x                          : 14;
            unsigned int                                :  2;
            unsigned int dst_y                          : 14;
            unsigned int                                :  2;
        };
        unsigned int DW_8_DATA;
    } DW_8_UNION;

    union
    {
        struct
        {
            unsigned int dst_z                          : 13;
            unsigned int dst_pitch                      : 19;
        };
        unsigned int DW_9_DATA;
    } DW_9_UNION;

    union
    {
        struct
        {
            unsigned int dst_slice_pitch                : 28;
            unsigned int                                :  4;
        };
        unsigned int DW_10_DATA;
    } DW_10_UNION;

    union
    {
        struct
        {
            unsigned int rect_x                         : 14;
            unsigned int                                :  2;
            unsigned int rect_y                         : 14;
            unsigned int                                :  2;
        };
        unsigned int DW_11_DATA;
    } DW_11_UNION;

    union
    {
        struct
        {
            unsigned int rect_z                         : 13;
            unsigned int                                :  3;
            unsigned int dst_sw                         :  2;
            unsigned int                                :  6;
            unsigned int src_sw                         :  2;
            unsigned int                                :  6;
        };
        unsigned int DW_12_DATA;
    } DW_12_UNION;

} SDMA_PKT_COPY_LINEAR_SUBWIN;

typedef struct SDMA_PKT_COPY_PHYSICAL_LINEAR_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int                                :  2;
            unsigned int tmz                            :  1;
            unsigned int                                : 13;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int count                          : 22;
            unsigned int                                :  2;
            unsigned int addr_pair_num                  :  8;
        };
        unsigned int DW_1_DATA;
    } COUNT_UNION;

    union
    {
        struct
        {
            unsigned int                                :  3;
            unsigned int dst_mtype                      :  3;
            unsigned int dst_l2_policy                  :  2;
            unsigned int                                :  3;
            unsigned int src_mtype                      :  3;
            unsigned int src_l2_policy                  :  2;
            unsigned int                                :  3;
            unsigned int dst_gcc                        :  1;
            unsigned int dst_sys                        :  1;
            unsigned int dst_log                        :  1;
            unsigned int dst_snoop                      :  1;
            unsigned int dst_gpa                        :  1;
            unsigned int src_sw                         :  2;
            unsigned int                                :  1;
            unsigned int src_gcc                        :  1;
            unsigned int src_sys                        :  1;
            unsigned int                                :  1;
            unsigned int src_snoop                      :  1;
            unsigned int src_gpa                        :  1;
        };
        struct
        {
            unsigned int                                : 16;
            unsigned int dst_sw                         :  2;
            unsigned int                                : 14;
        } gfx101;
        unsigned int DW_2_DATA;
    } PARAMETER_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_31_0                  : 32;
        };
        unsigned int DW_3_DATA;
    } SRC_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_63_32                 : 32;
        };
        unsigned int DW_4_DATA;
    } SRC_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_31_0                  : 32;
        };
        unsigned int DW_5_DATA;
    } DST_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_63_32                 : 32;
        };
        unsigned int DW_6_DATA;
    } DST_ADDR_HI_UNION;

} SDMA_PKT_COPY_PHYSICAL_LINEAR;

typedef struct SDMA_PKT_COPY_STRUCT_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int                                :  2;
            unsigned int tmz                            :  1;
            unsigned int                                : 12;
            unsigned int detile                         :  1;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int sb_addr_31_0                   : 32;
        };
        unsigned int DW_1_DATA;
    } SB_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int sb_addr_63_32                  : 32;
        };
        unsigned int DW_2_DATA;
    } SB_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int start_index                    : 32;
        };
        unsigned int DW_3_DATA;
    } START_INDEX_UNION;

    union
    {
        struct
        {
            unsigned int count                          : 32;
        };
        unsigned int DW_4_DATA;
    } COUNT_UNION;

    union
    {
        struct
        {
            unsigned int stride                         : 11;
            unsigned int                                :  5;
            unsigned int linear_sw                      :  2;
            unsigned int                                :  6;
            unsigned int struct_sw                      :  2;
            unsigned int                                :  6;
        };
        unsigned int DW_5_DATA;
    } DW_5_UNION;

    union
    {
        struct
        {
            unsigned int linear_addr_31_0               : 32;
        };
        unsigned int DW_6_DATA;
    } LINEAR_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int linear_addr_63_32              : 32;
        };
        unsigned int DW_7_DATA;
    } LINEAR_ADDR_HI_UNION;

} SDMA_PKT_COPY_STRUCT;

typedef struct SDMA_PKT_COPY_T2T_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int                                :  2;
            unsigned int tmz                            :  1;
            unsigned int dcc                            :  1;
            unsigned int                                : 11;
            unsigned int dcc_dir                        :  1;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_31_0                  : 32;
        };
        unsigned int DW_1_DATA;
    } SRC_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_63_32                 : 32;
        };
        unsigned int DW_2_DATA;
    } SRC_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int src_x                          : 14;
            unsigned int                                :  2;
            unsigned int src_y                          : 14;
            unsigned int                                :  2;
        };
        unsigned int DW_3_DATA;
    } DW_3_UNION;

    union
    {
        struct
        {
            unsigned int src_z                          : 13;
            unsigned int                                :  3;
            unsigned int src_width                      : 14;
            unsigned int                                :  2;
        };
        unsigned int DW_4_DATA;
    } DW_4_UNION;

    union
    {
        struct
        {
            unsigned int src_height                     : 14;
            unsigned int                                :  2;
            unsigned int src_depth                      : 13;
            unsigned int                                :  3;
        };
        unsigned int DW_5_DATA;
    } DW_5_UNION;

    union
    {
        struct
        {
            unsigned int src_element_size               :  3;
            unsigned int src_swizzle_mode               :  5;
            unsigned int                                :  1;
            unsigned int src_dimension                  :  2;
            unsigned int                                :  5;
            unsigned int src_mip_max                    :  4;
            unsigned int src_mip_id                     :  4;
            unsigned int                                :  8;
        };
        unsigned int DW_6_DATA;
    } DW_6_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_31_0                  : 32;
        };
        unsigned int DW_7_DATA;
    } DST_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_63_32                 : 32;
        };
        unsigned int DW_8_DATA;
    } DST_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int dst_x                          : 14;
            unsigned int                                :  2;
            unsigned int dst_y                          : 14;
            unsigned int                                :  2;
        };
        unsigned int DW_9_DATA;
    } DW_9_UNION;

    union
    {
        struct
        {
            unsigned int dst_z                          : 13;
            unsigned int                                :  3;
            unsigned int dst_width                      : 14;
            unsigned int                                :  2;
        };
        unsigned int DW_10_DATA;
    } DW_10_UNION;

    union
    {
        struct
        {
            unsigned int dst_height                     : 14;
            unsigned int                                :  2;
            unsigned int dst_depth                      : 13;
            unsigned int                                :  3;
        };
        unsigned int DW_11_DATA;
    } DW_11_UNION;

    union
    {
        struct
        {
            unsigned int dst_element_size               :  3;
            unsigned int dst_swizzle_mode               :  5;
            unsigned int                                :  1;
            unsigned int dst_dimension                  :  2;
            unsigned int                                :  5;
            unsigned int dst_mip_max                    :  4;
            unsigned int dst_mip_id                     :  4;
            unsigned int                                :  8;
        };
        unsigned int DW_12_DATA;
    } DW_12_UNION;

    union
    {
        struct
        {
            unsigned int rect_x                         : 14;
            unsigned int                                :  2;
            unsigned int rect_y                         : 14;
            unsigned int                                :  2;
        };
        unsigned int DW_13_DATA;
    } DW_13_UNION;

    union
    {
        struct
        {
            unsigned int rect_z                         : 13;
            unsigned int                                :  3;
            unsigned int dst_sw                         :  2;
            unsigned int                                :  6;
            unsigned int src_sw                         :  2;
            unsigned int                                :  6;
        };
        unsigned int DW_14_DATA;
    } DW_14_UNION;

    union
    {
        struct
        {
            unsigned int meta_addr_31_0                 : 32;
        };
        unsigned int DW_15_DATA;
    } META_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int meta_addr_63_32                : 32;
        };
        unsigned int DW_16_DATA;
    } META_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int data_format                    :  7;
            unsigned int color_transform_disable        :  1;
            unsigned int alpha_is_on_msb                :  1;
            unsigned int number_type                    :  3;
            unsigned int surface_type                   :  2;
            unsigned int                                : 10;
            unsigned int max_comp_block_size            :  2;
            unsigned int max_uncomp_block_size          :  2;
            unsigned int write_compress_enable          :  1;
            unsigned int meta_tmz                       :  1;
            unsigned int                                :  1;
            unsigned int pipe_aligned                   :  1;
        };
        unsigned int DW_17_DATA;
    } META_CONFIG_UNION;

} SDMA_PKT_COPY_T2T;

typedef struct SDMA_PKT_COPY_TILED_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int encrypt                        :  1;
            unsigned int                                :  1;
            unsigned int tmz                            :  1;
            unsigned int                                : 11;
            unsigned int                                :  1;
            unsigned int detile                         :  1;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int tiled_addr_31_0                : 32;
        };
        unsigned int DW_1_DATA;
    } TILED_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int tiled_addr_63_32               : 32;
        };
        unsigned int DW_2_DATA;
    } TILED_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int width                          : 14;
            unsigned int                                : 18;
        };
        unsigned int DW_3_DATA;
    } DW_3_UNION;

    union
    {
        struct
        {
            unsigned int height                         : 14;
            unsigned int                                :  2;
            unsigned int depth                          : 13;
            unsigned int                                :  3;
        };
        unsigned int DW_4_DATA;
    } DW_4_UNION;

    union
    {
        struct
        {
            unsigned int element_size                   :  3;
            unsigned int swizzle_mode                   :  5;
            unsigned int                                :  1;
            unsigned int dimension                      :  2;
            unsigned int                                : 21;
        };
        struct
        {
            unsigned int                                : 16;
            unsigned int mip_max                        :  4;
            unsigned int                                : 12;
        } gfx101;
        unsigned int DW_5_DATA;
    } DW_5_UNION;

    union
    {
        struct
        {
            unsigned int x                              : 14;
            unsigned int                                :  2;
            unsigned int y                              : 14;
            unsigned int                                :  2;
        };
        unsigned int DW_6_DATA;
    } DW_6_UNION;

    union
    {
        struct
        {
            unsigned int z                              : 13;
            unsigned int                                :  3;
            unsigned int linear_sw                      :  2;
            unsigned int                                :  6;
            unsigned int tile_sw                        :  2;
            unsigned int                                :  6;
        };
        struct
        {
            unsigned int                                : 20;
            unsigned int linear_cc                      :  1;
            unsigned int                                : 11;
        } gfx101;
        unsigned int DW_7_DATA;
    } DW_7_UNION;

    union
    {
        struct
        {
            unsigned int linear_addr_31_0               : 32;
        };
        unsigned int DW_8_DATA;
    } LINEAR_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int linear_addr_63_32              : 32;
        };
        unsigned int DW_9_DATA;
    } LINEAR_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int linear_pitch                   : 19;
            unsigned int                                : 13;
        };
        unsigned int DW_10_DATA;
    } LINEAR_PITCH_UNION;

    union
    {
        struct
        {
            unsigned int linear_slice_pitch             : 32;
        };
        unsigned int DW_11_DATA;
    } LINEAR_SLICE_PITCH_UNION;

    union
    {
        struct
        {
            unsigned int count                          : 22;
            unsigned int                                : 10;
        } gfx101;
        unsigned int DW_12_DATA;
    } COUNT_UNION;

} SDMA_PKT_COPY_TILED;

typedef struct SDMA_PKT_COPY_TILED_SUBWIN_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int                                :  2;
            unsigned int tmz                            :  1;
            unsigned int dcc                            :  1;
            unsigned int                                : 11;
            unsigned int detile                         :  1;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int tiled_addr_31_0                : 32;
        };
        unsigned int DW_1_DATA;
    } TILED_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int tiled_addr_63_32               : 32;
        };
        unsigned int DW_2_DATA;
    } TILED_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int tiled_x                        : 14;
            unsigned int                                :  2;
            unsigned int tiled_y                        : 14;
            unsigned int                                :  2;
        };
        unsigned int DW_3_DATA;
    } DW_3_UNION;

    union
    {
        struct
        {
            unsigned int tiled_z                        : 13;
            unsigned int                                :  3;
            unsigned int width                          : 14;
            unsigned int                                :  2;
        };
        unsigned int DW_4_DATA;
    } DW_4_UNION;

    union
    {
        struct
        {
            unsigned int height                         : 14;
            unsigned int                                :  2;
            unsigned int depth                          : 13;
            unsigned int                                :  3;
        };
        unsigned int DW_5_DATA;
    } DW_5_UNION;

    union
    {
        struct
        {
            unsigned int element_size                   :  3;
            unsigned int swizzle_mode                   :  5;
            unsigned int                                :  1;
            unsigned int dimension                      :  2;
            unsigned int                                :  5;
            unsigned int mip_max                        :  4;
            unsigned int mip_id                         :  4;
            unsigned int                                :  8;
        };
        unsigned int DW_6_DATA;
    } DW_6_UNION;

    union
    {
        struct
        {
            unsigned int linear_addr_31_0               : 32;
        };
        unsigned int DW_7_DATA;
    } LINEAR_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int linear_addr_63_32              : 32;
        };
        unsigned int DW_8_DATA;
    } LINEAR_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int linear_x                       : 14;
            unsigned int                                :  2;
            unsigned int linear_y                       : 14;
            unsigned int                                :  2;
        };
        unsigned int DW_9_DATA;
    } DW_9_UNION;

    union
    {
        struct
        {
            unsigned int linear_z                       : 13;
            unsigned int                                :  3;
            unsigned int linear_pitch                   : 14;
            unsigned int                                :  2;
        };
        unsigned int DW_10_DATA;
    } DW_10_UNION;

    union
    {
        struct
        {
            unsigned int linear_slice_pitch             : 28;
            unsigned int                                :  4;
        };
        unsigned int DW_11_DATA;
    } DW_11_UNION;

    union
    {
        struct
        {
            unsigned int rect_x                         : 14;
            unsigned int                                :  2;
            unsigned int rect_y                         : 14;
            unsigned int                                :  2;
        };
        unsigned int DW_12_DATA;
    } DW_12_UNION;

    union
    {
        struct
        {
            unsigned int rect_z                         : 13;
            unsigned int                                :  3;
            unsigned int linear_sw                      :  2;
            unsigned int                                :  6;
            unsigned int tile_sw                        :  2;
            unsigned int                                :  6;
        };
        unsigned int DW_13_DATA;
    } DW_13_UNION;

    union
    {
        struct
        {
            unsigned int meta_addr_31_0                 : 32;
        };
        unsigned int DW_14_DATA;
    } META_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int meta_addr_63_32                : 32;
        };
        unsigned int DW_15_DATA;
    } META_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int data_format                    :  7;
            unsigned int color_transform_disable        :  1;
            unsigned int alpha_is_on_msb                :  1;
            unsigned int number_type                    :  3;
            unsigned int surface_type                   :  2;
            unsigned int                                : 10;
            unsigned int max_comp_block_size            :  2;
            unsigned int max_uncomp_block_size          :  2;
            unsigned int write_compress_enable          :  1;
            unsigned int meta_tmz                       :  1;
            unsigned int                                :  1;
            unsigned int pipe_aligned                   :  1;
        };
        unsigned int DW_16_DATA;
    } META_CONFIG_UNION;

} SDMA_PKT_COPY_TILED_SUBWIN;

typedef struct SDMA_PKT_COUNTER_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int                                : 15;
            unsigned int ch                             :  1;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int cntr_31_0                      : 32;
        };
        unsigned int DW_1_DATA;
    } CNTR_0_UNION;

    union
    {
        struct
        {
            unsigned int cntr_63_32                     : 32;
        };
        unsigned int DW_2_DATA;
    } CNTR_1_UNION;

    union
    {
        struct
        {
            unsigned int cntr_95_64                     : 32;
        };
        unsigned int DW_3_DATA;
    } CNTR_2_UNION;

    union
    {
        struct
        {
            unsigned int cntr_127_96                    : 32;
        };
        unsigned int DW_4_DATA;
    } CNTR_3_UNION;

} SDMA_PKT_COUNTER;

typedef struct SDMA_PKT_DATA_FILL_MULTI_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int                                : 15;
            unsigned int memlog_clr                     :  1;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int byte_stride                    : 32;
        };
        unsigned int DW_1_DATA;
    } BYTE_STRIDE_UNION;

    union
    {
        struct
        {
            unsigned int dma_count                      : 32;
        };
        unsigned int DW_2_DATA;
    } DMA_COUNT_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_31_0                  : 32;
        };
        unsigned int DW_3_DATA;
    } DST_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_63_32                 : 32;
        };
        unsigned int DW_4_DATA;
    } DST_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int count                          : 26;
            unsigned int                                :  6;
        };
        unsigned int DW_5_DATA;
    } BYTE_COUNT_UNION;

} SDMA_PKT_DATA_FILL_MULTI;

typedef struct SDMA_PKT_DUMMY_TRAP_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int                                : 16;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int int_context                    : 28;
            unsigned int                                :  4;
        };
        unsigned int DW_1_DATA;
    } INT_CONTEXT_UNION;

} SDMA_PKT_DUMMY_TRAP;

typedef struct SDMA_PKT_FENCE_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int mtype                          :  3;
            unsigned int gcc                            :  1;
            unsigned int sys                            :  1;
            unsigned int                                :  1;
            unsigned int snp                            :  1;
            unsigned int gpa                            :  1;
            unsigned int l2_policy                      :  2;
            unsigned int                                :  6;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int addr_31_0                      : 32;
        };
        unsigned int DW_1_DATA;
    } ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int addr_63_32                     : 32;
        };
        unsigned int DW_2_DATA;
    } ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int data                           : 32;
        };
        unsigned int DW_3_DATA;
    } DATA_UNION;

} SDMA_PKT_FENCE;

typedef struct SDMA_PKT_GCR_REQ_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int                                : 16;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int                                :  7;
            unsigned int base_va_31_7                   : 25;
        };
        unsigned int DW_1_DATA;
    } PAYLOAD1_UNION;

    union
    {
        struct
        {
            unsigned int base_va_47_32                  : 16;
            unsigned int gcr_control_15_0               : 16;
        };
        unsigned int DW_2_DATA;
    } PAYLOAD2_UNION;

    union
    {
        struct
        {
            unsigned int gcr_control_18_16              :  3;
            unsigned int                                :  4;
            unsigned int limit_va_31_7                  : 25;
        };
        unsigned int DW_3_DATA;
    } PAYLOAD3_UNION;

    union
    {
        struct
        {
            unsigned int limit_va_47_32                 : 16;
            unsigned int                                :  8;
            unsigned int vmid                           :  4;
            unsigned int                                :  4;
        };
        unsigned int DW_4_DATA;
    } PAYLOAD4_UNION;

} SDMA_PKT_GCR_REQ;

typedef struct SDMA_PKT_GPUVM_INV_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int                                : 16;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int per_vmid_inv_req               : 16;
            unsigned int flush_type                     :  3;
            unsigned int l2_ptes                        :  1;
            unsigned int l2_pde0                        :  1;
            unsigned int l2_pde1                        :  1;
            unsigned int l2_pde2                        :  1;
            unsigned int l1_ptes                        :  1;
            unsigned int clr_protection_fault_status_addr :  1;
            unsigned int log_request                    :  1;
            unsigned int four_kilobytes                 :  1;
            unsigned int                                :  5;
        };
        unsigned int DW_1_DATA;
    } PAYLOAD1_UNION;

    union
    {
        struct
        {
            unsigned int s                              :  1;
            unsigned int page_va_42_12                  : 31;
        };
        unsigned int DW_2_DATA;
    } PAYLOAD2_UNION;

    union
    {
        struct
        {
            unsigned int page_va_47_43                  :  6;
            unsigned int                                : 26;
        };
        unsigned int DW_3_DATA;
    } PAYLOAD3_UNION;

} SDMA_PKT_GPUVM_INV;

typedef struct SDMA_PKT_INDIRECT_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int vmid                           :  4;
            unsigned int                                : 11;
            unsigned int priv                           :  1;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int ib_base_31_0                   : 32;
        };
        unsigned int DW_1_DATA;
    } BASE_LO_UNION;

    union
    {
        struct
        {
            unsigned int ib_base_63_32                  : 32;
        };
        unsigned int DW_2_DATA;
    } BASE_HI_UNION;

    union
    {
        struct
        {
            unsigned int ib_size                        : 20;
            unsigned int                                : 12;
        };
        unsigned int DW_3_DATA;
    } IB_SIZE_UNION;

    union
    {
        struct
        {
            unsigned int csa_addr_31_0                  : 32;
        };
        unsigned int DW_4_DATA;
    } CSA_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int csa_addr_63_32                 : 32;
        };
        unsigned int DW_5_DATA;
    } CSA_ADDR_HI_UNION;

} SDMA_PKT_INDIRECT;

typedef struct SDMA_PKT_INVALIDATION_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int                                : 16;
        } gfx101;
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int invalidatereq                  : 32;
        } gfx101;
        unsigned int DW_1_DATA;
    } INVALIDATEREQ_UNION;

    union
    {
        struct
        {
            unsigned int addressrangelo                 : 32;
        } gfx101;
        unsigned int DW_2_DATA;
    } ADDRESSRANGELO_UNION;

    union
    {
        struct
        {
            unsigned int invalidateack                  : 16;
            unsigned int addressrangehi                 :  5;
            unsigned int invalidategfxhub               :  1;
            unsigned int invalidatemmhub                :  1;
            unsigned int                                :  9;
        } gfx101;
        unsigned int DW_3_DATA;
    } ADDRESSRANGEHI_UNION;

} SDMA_PKT_INVALIDATION;

typedef struct SDMA_PKT_MEM_INCR_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int                                : 16;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int addr_31_0                      : 32;
        };
        unsigned int DW_1_DATA;
    } ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int addr_63_32                     : 32;
        };
        unsigned int DW_2_DATA;
    } ADDR_HI_UNION;

} SDMA_PKT_MEM_INCR;

typedef struct SDMA_PKT_NOP_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int count                          : 14;
            unsigned int                                :  2;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int data0                          : 32;
        };
        unsigned int DW_1_DATA;
    } DATA0_UNION;

} SDMA_PKT_NOP;

typedef struct SDMA_PKT_POLL_DBIT_WRITE_MEM_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int ea                             :  2;
            unsigned int                                : 14;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int addr_31_0                      : 32;
        };
        unsigned int DW_1_DATA;
    } DST_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int addr_63_32                     : 32;
        };
        unsigned int DW_2_DATA;
    } DST_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int                                :  4;
            unsigned int addr_31_4                      : 28;
        };
        unsigned int DW_3_DATA;
    } START_PAGE_UNION;

    union
    {
        struct
        {
            unsigned int page_num_31_0                  : 32;
        };
        unsigned int DW_4_DATA;
    } PAGE_NUM_UNION;

} SDMA_PKT_POLL_DBIT_WRITE_MEM;

typedef struct SDMA_PKT_POLL_MEM_VERIFY_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int                                : 15;
            unsigned int mode                           :  1;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int pattern                        : 32;
        };
        unsigned int DW_1_DATA;
    } PATTERN_UNION;

    union
    {
        struct
        {
            unsigned int cmp0_start_31_0                : 32;
        };
        unsigned int DW_2_DATA;
    } CMP0_ADDR_START_LO_UNION;

    union
    {
        struct
        {
            unsigned int cmp0_start_63_32               : 32;
        };
        unsigned int DW_3_DATA;
    } CMP0_ADDR_START_HI_UNION;

    union
    {
        struct
        {
            unsigned int cmp1_end_31_0                  : 32;
        } gfx101;
        unsigned int DW_4_DATA;
    } CMP0_ADDR_END_LO_UNION;

    union
    {
        struct
        {
            unsigned int cmp1_end_63_32                 : 32;
        } gfx101;
        unsigned int DW_5_DATA;
    } CMP0_ADDR_END_HI_UNION;

    union
    {
        struct
        {
            unsigned int cmp1_start_31_0                : 32;
        };
        unsigned int DW_6_DATA;
    } CMP1_ADDR_START_LO_UNION;

    union
    {
        struct
        {
            unsigned int cmp1_start_63_32               : 32;
        };
        unsigned int DW_7_DATA;
    } CMP1_ADDR_START_HI_UNION;

    union
    {
        struct
        {
            unsigned int cmp1_end_31_0                  : 32;
        };
        unsigned int DW_8_DATA;
    } CMP1_ADDR_END_LO_UNION;

    union
    {
        struct
        {
            unsigned int cmp1_end_63_32                 : 32;
        };
        unsigned int DW_9_DATA;
    } CMP1_ADDR_END_HI_UNION;

    union
    {
        struct
        {
            unsigned int rec_31_0                       : 32;
        };
        unsigned int DW_10_DATA;
    } REC_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int rec_63_32                      : 32;
        };
        unsigned int DW_11_DATA;
    } REC_ADDR_HI_UNION;

    union
    {
        unsigned int DW_12_DATA;
    } RESERVED_UNION;

} SDMA_PKT_POLL_MEM_VERIFY;

typedef struct SDMA_PKT_POLL_REGMEM_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int                                : 10;
            unsigned int hdp_flush                      :  1;
            unsigned int                                :  1;
            unsigned int func                           :  3;
            unsigned int mem_poll                       :  1;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int addr_31_0                      : 32;
        };
        unsigned int DW_1_DATA;
    } ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int addr_63_32                     : 32;
        };
        unsigned int DW_2_DATA;
    } ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int value                          : 32;
        };
        unsigned int DW_3_DATA;
    } VALUE_UNION;

    union
    {
        struct
        {
            unsigned int mask                           : 32;
        };
        unsigned int DW_4_DATA;
    } MASK_UNION;

    union
    {
        struct
        {
            unsigned int interval                       : 16;
            unsigned int retry_count                    : 12;
            unsigned int                                :  4;
        };
        unsigned int DW_5_DATA;
    } DW5_UNION;

} SDMA_PKT_POLL_REGMEM;

typedef struct SDMA_PKT_POLL_REG_WRITE_MEM_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int                                : 16;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int                                :  2;
            unsigned int addr_31_2                      : 30;
        };
        unsigned int DW_1_DATA;
    } SRC_ADDR_UNION;

    union
    {
        struct
        {
            unsigned int addr_31_0                      : 32;
        };
        unsigned int DW_2_DATA;
    } DST_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int addr_63_32                     : 32;
        };
        unsigned int DW_3_DATA;
    } DST_ADDR_HI_UNION;

} SDMA_PKT_POLL_REG_WRITE_MEM;

typedef struct SDMA_PKT_PRE_EXE_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int dev_sel                        :  8;
            unsigned int                                :  8;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int exec_count                     : 14;
            unsigned int                                : 18;
        };
        unsigned int DW_1_DATA;
    } EXEC_COUNT_UNION;

} SDMA_PKT_PRE_EXE;

typedef struct SDMA_PKT_SEMAPHORE_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int                                : 13;
            unsigned int write_one                      :  1;
            unsigned int signal                         :  1;
            unsigned int mailbox                        :  1;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int addr_31_0                      : 32;
        };
        unsigned int DW_1_DATA;
    } ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int addr_63_32                     : 32;
        };
        unsigned int DW_2_DATA;
    } ADDR_HI_UNION;

} SDMA_PKT_SEMAPHORE;

typedef struct SDMA_PKT_SRBM_WRITE_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int                                : 12;
            unsigned int byte_en                        :  4;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int addr                           : 18;
            unsigned int                                :  2;
            unsigned int apertureid                     : 12;
        };
        unsigned int DW_1_DATA;
    } ADDR_UNION;

    union
    {
        struct
        {
            unsigned int data                           : 32;
        };
        unsigned int DW_2_DATA;
    } DATA_UNION;

} SDMA_PKT_SRBM_WRITE;

typedef struct SDMA_PKT_TIMESTAMP_GET_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int                                : 16;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int                                :  3;
            unsigned int write_addr_31_3                : 29;
        };
        unsigned int DW_1_DATA;
    } WRITE_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int write_addr_63_32               : 32;
        };
        unsigned int DW_2_DATA;
    } WRITE_ADDR_HI_UNION;

} SDMA_PKT_TIMESTAMP_GET;

typedef struct SDMA_PKT_TIMESTAMP_GET_GLOBAL_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int                                : 16;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int                                :  3;
            unsigned int write_addr_31_3                : 29;
        };
        unsigned int DW_1_DATA;
    } WRITE_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int write_addr_63_32               : 32;
        };
        unsigned int DW_2_DATA;
    } WRITE_ADDR_HI_UNION;

} SDMA_PKT_TIMESTAMP_GET_GLOBAL;

typedef struct SDMA_PKT_TIMESTAMP_SET_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int                                : 16;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int init_data_31_0                 : 32;
        };
        unsigned int DW_1_DATA;
    } INIT_DATA_LO_UNION;

    union
    {
        struct
        {
            unsigned int init_data_63_32                : 32;
        };
        unsigned int DW_2_DATA;
    } INIT_DATA_HI_UNION;

} SDMA_PKT_TIMESTAMP_SET;

typedef struct SDMA_PKT_TRAP_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int                                : 16;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int int_context                    : 28;
            unsigned int                                :  4;
        };
        unsigned int DW_1_DATA;
    } INT_CONTEXT_UNION;

} SDMA_PKT_TRAP;

typedef struct SDMA_PKT_TRNG_FETCH_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int                                : 15;
            unsigned int ch                             :  1;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

} SDMA_PKT_TRNG_FETCH;

typedef struct SDMA_PKT_WRITE_INCR_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int                                : 16;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_31_0                  : 32;
        };
        unsigned int DW_1_DATA;
    } DST_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_63_32                 : 32;
        };
        unsigned int DW_2_DATA;
    } DST_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int mask_dw0                       : 32;
        };
        unsigned int DW_3_DATA;
    } MASK_DW0_UNION;

    union
    {
        struct
        {
            unsigned int mask_dw1                       : 32;
        };
        unsigned int DW_4_DATA;
    } MASK_DW1_UNION;

    union
    {
        struct
        {
            unsigned int init_dw0                       : 32;
        };
        unsigned int DW_5_DATA;
    } INIT_DW0_UNION;

    union
    {
        struct
        {
            unsigned int init_dw1                       : 32;
        };
        unsigned int DW_6_DATA;
    } INIT_DW1_UNION;

    union
    {
        struct
        {
            unsigned int incr_dw0                       : 32;
        };
        unsigned int DW_7_DATA;
    } INCR_DW0_UNION;

    union
    {
        struct
        {
            unsigned int incr_dw1                       : 32;
        };
        unsigned int DW_8_DATA;
    } INCR_DW1_UNION;

    union
    {
        struct
        {
            unsigned int count                          : 19;
            unsigned int                                : 13;
        };
        unsigned int DW_9_DATA;
    } COUNT_UNION;

} SDMA_PKT_WRITE_INCR;

typedef struct SDMA_PKT_WRITE_TILED_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int encrypt                        :  1;
            unsigned int                                :  1;
            unsigned int tmz                            :  1;
            unsigned int                                : 12;
            unsigned int                                :  1;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_31_0                  : 32;
        };
        unsigned int DW_1_DATA;
    } DST_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_63_32                 : 32;
        };
        unsigned int DW_2_DATA;
    } DST_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int width                          : 14;
            unsigned int                                : 18;
        };
        unsigned int DW_3_DATA;
    } DW_3_UNION;

    union
    {
        struct
        {
            unsigned int height                         : 14;
            unsigned int                                :  2;
            unsigned int depth                          : 13;
            unsigned int                                :  3;
        };
        unsigned int DW_4_DATA;
    } DW_4_UNION;

    union
    {
        struct
        {
            unsigned int element_size                   :  3;
            unsigned int swizzle_mode                   :  5;
            unsigned int                                :  1;
            unsigned int dimension                      :  2;
            unsigned int                                : 21;
        };
        struct
        {
            unsigned int                                : 16;
            unsigned int mip_max                        :  4;
            unsigned int                                : 12;
        } gfx101;
        unsigned int DW_5_DATA;
    } DW_5_UNION;

    union
    {
        struct
        {
            unsigned int x                              : 14;
            unsigned int                                :  2;
            unsigned int y                              : 14;
            unsigned int                                :  2;
        };
        unsigned int DW_6_DATA;
    } DW_6_UNION;

    union
    {
        struct
        {
            unsigned int z                              : 13;
            unsigned int                                : 11;
            unsigned int sw                             :  2;
            unsigned int                                :  6;
        };
        unsigned int DW_7_DATA;
    } DW_7_UNION;

    union
    {
        struct
        {
            unsigned int count                          : 20;
            unsigned int                                : 12;
        };
        unsigned int DW_8_DATA;
    } COUNT_UNION;

    union
    {
        struct
        {
            unsigned int data0                          : 32;
        };
        unsigned int DW_9_DATA;
    } DATA0_UNION;

} SDMA_PKT_WRITE_TILED;

typedef struct SDMA_PKT_WRITE_UNTILED_TAG
{
    union
    {
        struct
        {
            unsigned int op                             :  8;
            unsigned int sub_op                         :  8;
            unsigned int encrypt                        :  1;
            unsigned int                                :  1;
            unsigned int tmz                            :  1;
            unsigned int                                : 12;
            unsigned int                                :  1;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_31_0                  : 32;
        };
        unsigned int DW_1_DATA;
    } DST_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_63_32                 : 32;
        };
        unsigned int DW_2_DATA;
    } DST_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int count                          : 20;
            unsigned int                                :  4;
            unsigned int sw                             :  2;
            unsigned int                                :  6;
        };
        unsigned int DW_3_DATA;
    } DW_3_UNION;

    union
    {
        struct
        {
            unsigned int data0                          : 32;
        };
        unsigned int DW_4_DATA;
    } DATA0_UNION;

} SDMA_PKT_WRITE_UNTILED;

