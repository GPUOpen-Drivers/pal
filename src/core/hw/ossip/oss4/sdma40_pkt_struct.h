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

#ifndef _SDMA40_PKT_H_
#define _SDMA40_PKT_H_

const unsigned int SDMA_OP_NOP = 0;
const unsigned int SDMA_OP_COPY = 1;
const unsigned int SDMA_OP_WRITE = 2;
const unsigned int SDMA_OP_INDIRECT = 4;
const unsigned int SDMA_OP_FENCE = 5;
const unsigned int SDMA_OP_TRAP = 6;
const unsigned int SDMA_OP_SEM = 7;
const unsigned int SDMA_OP_POLL_REGMEM = 8;
const unsigned int SDMA_OP_COND_EXE = 9;
const unsigned int SDMA_OP_ATOMIC = 10;
const unsigned int SDMA_OP_CONST_FILL = 11;
const unsigned int SDMA_OP_PTEPDE = 12;
const unsigned int SDMA_OP_TIMESTAMP = 13;
const unsigned int SDMA_OP_SRBM_WRITE = 14;
const unsigned int SDMA_OP_PRE_EXE = 15;
const unsigned int SDMA_OP_DUMMY_TRAP = 16;
const unsigned int SDMA_SUBOP_TIMESTAMP_SET = 0;
const unsigned int SDMA_SUBOP_TIMESTAMP_GET = 1;
const unsigned int SDMA_SUBOP_TIMESTAMP_GET_GLOBAL = 2;
const unsigned int SDMA_SUBOP_COPY_LINEAR = 0;
const unsigned int SDMA_SUBOP_COPY_LINEAR_SUB_WIND = 4;
const unsigned int SDMA_SUBOP_COPY_TILED = 1;
const unsigned int SDMA_SUBOP_COPY_TILED_SUB_WIND = 5;
const unsigned int SDMA_SUBOP_COPY_T2T_SUB_WIND = 6;
const unsigned int SDMA_SUBOP_COPY_SOA = 3;
const unsigned int SDMA_SUBOP_COPY_DIRTY_PAGE = 7;
const unsigned int SDMA_SUBOP_COPY_LINEAR_PHY = 8;
const unsigned int SDMA_SUBOP_WRITE_LINEAR = 0;
const unsigned int SDMA_SUBOP_WRITE_TILED = 1;
const unsigned int SDMA_SUBOP_PTEPDE_GEN = 0;
const unsigned int SDMA_SUBOP_PTEPDE_COPY = 1;
const unsigned int SDMA_SUBOP_PTEPDE_RMW = 2;
const unsigned int SDMA_SUBOP_PTEPDE_COPY_BACKWARDS = 3;
const unsigned int SDMA_SUBOP_MEM_INCR = 1;
const unsigned int SDMA_SUBOP_DATA_FILL_MULTI = 1;
const unsigned int SDMA_SUBOP_POLL_REG_WRITE_MEM = 1;
const unsigned int SDMA_SUBOP_POLL_DBIT_WRITE_MEM = 2;
const unsigned int SDMA_SUBOP_POLL_MEM_VERIFY = 3;
const unsigned int HEADER_AGENT_DISPATCH = 4;
const unsigned int HEADER_BARRIER = 5;
const unsigned int SDMA_OP_AQL_COPY = 0;
const unsigned int SDMA_OP_AQL_BARRIER_OR = 0;

/*
** Definitions for SDMA_PKT_COPY_LINEAR packet
*/

typedef struct SDMA_PKT_COPY_LINEAR_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:2;
            unsigned int tmz:1;
            unsigned int reserved_1:8;
            unsigned int broadcast:1;
            unsigned int reserved_2:4;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int count:22;
            unsigned int reserved_0:10;
        };
        unsigned int DW_1_DATA;
    } COUNT_UNION;

    union
    {
        struct
        {
            unsigned int reserved_0:16;
            unsigned int dst_sw:2;
            unsigned int reserved_1:6;
            unsigned int src_sw:2;
            unsigned int reserved_2:6;
        };
        unsigned int DW_2_DATA;
    } PARAMETER_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_31_0:32;
        };
        unsigned int DW_3_DATA;
    } SRC_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_63_32:32;
        };
        unsigned int DW_4_DATA;
    } SRC_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_31_0:32;
        };
        unsigned int DW_5_DATA;
    } DST_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_63_32:32;
        };
        unsigned int DW_6_DATA;
    } DST_ADDR_HI_UNION;
} SDMA_PKT_COPY_LINEAR, *PSDMA_PKT_COPY_LINEAR;

/*
** Definitions for SDMA_PKT_COPY_DIRTY_PAGE packet
*/

typedef struct SDMA_PKT_COPY_DIRTY_PAGE_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:15;
            unsigned int all:1;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int count:22;
            unsigned int reserved_0:10;
        };
        unsigned int DW_1_DATA;
    } COUNT_UNION;

    union
    {
        struct
        {
            unsigned int reserved_0:16;
            unsigned int dst_sw:2;
            unsigned int reserved_1:1;
            unsigned int dst_gcc:1;
            unsigned int dst_sys:1;
            unsigned int reserved_2:1;
            unsigned int dst_snoop:1;
            unsigned int dst_gpa:1;
            unsigned int src_sw:2;
            unsigned int reserved_3:2;
            unsigned int src_sys:1;
            unsigned int reserved_4:1;
            unsigned int src_snoop:1;
            unsigned int src_gpa:1;
        };
        unsigned int DW_2_DATA;
    } PARAMETER_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_31_0:32;
        };
        unsigned int DW_3_DATA;
    } SRC_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_63_32:32;
        };
        unsigned int DW_4_DATA;
    } SRC_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_31_0:32;
        };
        unsigned int DW_5_DATA;
    } DST_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_63_32:32;
        };
        unsigned int DW_6_DATA;
    } DST_ADDR_HI_UNION;
} SDMA_PKT_COPY_DIRTY_PAGE, *PSDMA_PKT_COPY_DIRTY_PAGE;

/*
** Definitions for SDMA_PKT_COPY_PHYSICAL_LINEAR packet
*/

typedef struct SDMA_PKT_COPY_PHYSICAL_LINEAR_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:16;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int count:22;
            unsigned int reserved_0:2;
            unsigned int addr_pair_num:8;
        };
        unsigned int DW_1_DATA;
    } COUNT_UNION;

    union
    {
        struct
        {
            unsigned int reserved_0:16;
            unsigned int dst_sw:2;
            unsigned int reserved_1:1;
            unsigned int dst_gcc:1;
            unsigned int dst_sys:1;
            unsigned int dst_log:1;
            unsigned int dst_snoop:1;
            unsigned int dst_gpa:1;
            unsigned int src_sw:2;
            unsigned int reserved_2:1;
            unsigned int src_gcc:1;
            unsigned int src_sys:1;
            unsigned int reserved_3:1;
            unsigned int src_snoop:1;
            unsigned int src_gpa:1;
        };
        unsigned int DW_2_DATA;
    } PARAMETER_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_31_0:32;
        };
        unsigned int DW_3_DATA;
    } SRC_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_63_32:32;
        };
        unsigned int DW_4_DATA;
    } SRC_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_31_0:32;
        };
        unsigned int DW_5_DATA;
    } DST_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_63_32:32;
        };
        unsigned int DW_6_DATA;
    } DST_ADDR_HI_UNION;
} SDMA_PKT_COPY_PHYSICAL_LINEAR, *PSDMA_PKT_COPY_PHYSICAL_LINEAR;

/*
** Definitions for SDMA_PKT_COPY_BROADCAST_LINEAR packet
*/

typedef struct SDMA_PKT_COPY_BROADCAST_LINEAR_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:11;
            unsigned int broadcast:1;
            unsigned int reserved_1:4;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int count:22;
            unsigned int reserved_0:10;
        };
        unsigned int DW_1_DATA;
    } COUNT_UNION;

    union
    {
        struct
        {
            unsigned int reserved_0:8;
            unsigned int dst2_sw:2;
            unsigned int reserved_1:6;
            unsigned int dst1_sw:2;
            unsigned int reserved_2:6;
            unsigned int src_sw:2;
            unsigned int reserved_3:6;
        };
        unsigned int DW_2_DATA;
    } PARAMETER_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_31_0:32;
        };
        unsigned int DW_3_DATA;
    } SRC_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_63_32:32;
        };
        unsigned int DW_4_DATA;
    } SRC_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int dst1_addr_31_0:32;
        };
        unsigned int DW_5_DATA;
    } DST1_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int dst1_addr_63_32:32;
        };
        unsigned int DW_6_DATA;
    } DST1_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int dst2_addr_31_0:32;
        };
        unsigned int DW_7_DATA;
    } DST2_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int dst2_addr_63_32:32;
        };
        unsigned int DW_8_DATA;
    } DST2_ADDR_HI_UNION;
} SDMA_PKT_COPY_BROADCAST_LINEAR, *PSDMA_PKT_COPY_BROADCAST_LINEAR;

/*
** Definitions for SDMA_PKT_COPY_LINEAR_SUBWIN packet
*/

typedef struct SDMA_PKT_COPY_LINEAR_SUBWIN_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:13;
            unsigned int elementsize:3;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_31_0:32;
        };
        unsigned int DW_1_DATA;
    } SRC_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_63_32:32;
        };
        unsigned int DW_2_DATA;
    } SRC_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int src_x:14;
            unsigned int reserved_0:2;
            unsigned int src_y:14;
            unsigned int reserved_1:2;
        };
        unsigned int DW_3_DATA;
    } DW_3_UNION;

    union
    {
        struct
        {
            unsigned int src_z:11;
            unsigned int reserved_0:2;
            unsigned int src_pitch:19;
        };
        unsigned int DW_4_DATA;
    } DW_4_UNION;

    union
    {
        struct
        {
            unsigned int src_slice_pitch:28;
            unsigned int reserved_0:4;
        };
        unsigned int DW_5_DATA;
    } DW_5_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_31_0:32;
        };
        unsigned int DW_6_DATA;
    } DST_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_63_32:32;
        };
        unsigned int DW_7_DATA;
    } DST_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int dst_x:14;
            unsigned int reserved_0:2;
            unsigned int dst_y:14;
            unsigned int reserved_1:2;
        };
        unsigned int DW_8_DATA;
    } DW_8_UNION;

    union
    {
        struct
        {
            unsigned int dst_z:11;
            unsigned int reserved_0:2;
            unsigned int dst_pitch:19;
        };
        unsigned int DW_9_DATA;
    } DW_9_UNION;

    union
    {
        struct
        {
            unsigned int dst_slice_pitch:28;
            unsigned int reserved_0:4;
        };
        unsigned int DW_10_DATA;
    } DW_10_UNION;

    union
    {
        struct
        {
            unsigned int rect_x:14;
            unsigned int reserved_0:2;
            unsigned int rect_y:14;
            unsigned int reserved_1:2;
        };
        unsigned int DW_11_DATA;
    } DW_11_UNION;

    union
    {
        struct
        {
            unsigned int rect_z:11;
            unsigned int reserved_0:5;
            unsigned int dst_sw:2;
            unsigned int reserved_1:6;
            unsigned int src_sw:2;
            unsigned int reserved_2:6;
        };
        unsigned int DW_12_DATA;
    } DW_12_UNION;
} SDMA_PKT_COPY_LINEAR_SUBWIN, *PSDMA_PKT_COPY_LINEAR_SUBWIN;

/*
** Definitions for SDMA_PKT_COPY_TILED packet
*/

typedef struct SDMA_PKT_COPY_TILED_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:4;
            unsigned int mip_max:4;
            unsigned int reserved_2:7;
            unsigned int detile:1;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int tiled_addr_31_0:32;
        };
        unsigned int DW_1_DATA;
    } TILED_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int tiled_addr_63_32:32;
        };
        unsigned int DW_2_DATA;
    } TILED_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int width:14;
            unsigned int reserved_0:18;
        };
        unsigned int DW_3_DATA;
    } DW_3_UNION;

    union
    {
        struct
        {
            unsigned int height:14;
            unsigned int reserved_0:2;
            unsigned int depth:11;
            unsigned int reserved_1:5;
        };
        unsigned int DW_4_DATA;
    } DW_4_UNION;

    union
    {
        struct
        {
            unsigned int element_size:3;
            unsigned int swizzle_mode:5;
            unsigned int reserved_0:1;
            unsigned int dimension:2;
            unsigned int reserved_1:5;
            unsigned int epitch:16;
        };
        unsigned int DW_5_DATA;
    } DW_5_UNION;

    union
    {
        struct
        {
            unsigned int x:14;
            unsigned int reserved_0:2;
            unsigned int y:14;
            unsigned int reserved_1:2;
        };
        unsigned int DW_6_DATA;
    } DW_6_UNION;

    union
    {
        struct
        {
            unsigned int z:11;
            unsigned int reserved_0:5;
            unsigned int linear_sw:2;
            unsigned int reserved_1:6;
            unsigned int tile_sw:2;
            unsigned int reserved_2:6;
        };
        unsigned int DW_7_DATA;
    } DW_7_UNION;

    union
    {
        struct
        {
            unsigned int linear_addr_31_0:32;
        };
        unsigned int DW_8_DATA;
    } LINEAR_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int linear_addr_63_32:32;
        };
        unsigned int DW_9_DATA;
    } LINEAR_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int linear_pitch:19;
            unsigned int reserved_0:13;
        };
        unsigned int DW_10_DATA;
    } LINEAR_PITCH_UNION;

    union
    {
        struct
        {
            unsigned int linear_slice_pitch:32;
        };
        unsigned int DW_11_DATA;
    } LINEAR_SLICE_PITCH_UNION;

    union
    {
        struct
        {
            unsigned int count:20;
            unsigned int reserved_0:12;
        };
        unsigned int DW_12_DATA;
    } COUNT_UNION;
} SDMA_PKT_COPY_TILED, *PSDMA_PKT_COPY_TILED;

/*
** Definitions for SDMA_PKT_COPY_L2T_BROADCAST packet
*/

typedef struct SDMA_PKT_COPY_L2T_BROADCAST_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:4;
            unsigned int mip_max:4;
            unsigned int reserved_1:2;
            unsigned int videocopy:1;
            unsigned int broadcast:1;
            unsigned int reserved_2:4;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int tiled_addr0_31_0:32;
        };
        unsigned int DW_1_DATA;
    } TILED_ADDR_LO_0_UNION;

    union
    {
        struct
        {
            unsigned int tiled_addr0_63_32:32;
        };
        unsigned int DW_2_DATA;
    } TILED_ADDR_HI_0_UNION;

    union
    {
        struct
        {
            unsigned int tiled_addr1_31_0:32;
        };
        unsigned int DW_3_DATA;
    } TILED_ADDR_LO_1_UNION;

    union
    {
        struct
        {
            unsigned int tiled_addr1_63_32:32;
        };
        unsigned int DW_4_DATA;
    } TILED_ADDR_HI_1_UNION;

    union
    {
        struct
        {
            unsigned int width:14;
            unsigned int reserved_0:18;
        };
        unsigned int DW_5_DATA;
    } DW_5_UNION;

    union
    {
        struct
        {
            unsigned int height:14;
            unsigned int reserved_0:2;
            unsigned int depth:11;
            unsigned int reserved_1:5;
        };
        unsigned int DW_6_DATA;
    } DW_6_UNION;

    union
    {
        struct
        {
            unsigned int element_size:3;
            unsigned int swizzle_mode:5;
            unsigned int reserved_0:1;
            unsigned int dimension:2;
            unsigned int reserved_1:5;
            unsigned int epitch:16;
        };
        unsigned int DW_7_DATA;
    } DW_7_UNION;

    union
    {
        struct
        {
            unsigned int x:14;
            unsigned int reserved_0:2;
            unsigned int y:14;
            unsigned int reserved_1:2;
        };
        unsigned int DW_8_DATA;
    } DW_8_UNION;

    union
    {
        struct
        {
            unsigned int z:11;
            unsigned int reserved_0:21;
        };
        unsigned int DW_9_DATA;
    } DW_9_UNION;

    union
    {
        struct
        {
            unsigned int reserved_0:8;
            unsigned int dst2_sw:2;
            unsigned int reserved_1:6;
            unsigned int linear_sw:2;
            unsigned int reserved_2:6;
            unsigned int tile_sw:2;
            unsigned int reserved_3:6;
        };
        unsigned int DW_10_DATA;
    } DW_10_UNION;

    union
    {
        struct
        {
            unsigned int linear_addr_31_0:32;
        };
        unsigned int DW_11_DATA;
    } LINEAR_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int linear_addr_63_32:32;
        };
        unsigned int DW_12_DATA;
    } LINEAR_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int linear_pitch:19;
            unsigned int reserved_0:13;
        };
        unsigned int DW_13_DATA;
    } LINEAR_PITCH_UNION;

    union
    {
        struct
        {
            unsigned int linear_slice_pitch:32;
        };
        unsigned int DW_14_DATA;
    } LINEAR_SLICE_PITCH_UNION;

    union
    {
        struct
        {
            unsigned int count:20;
            unsigned int reserved_0:12;
        };
        unsigned int DW_15_DATA;
    } COUNT_UNION;
} SDMA_PKT_COPY_L2T_BROADCAST, *PSDMA_PKT_COPY_L2T_BROADCAST;

/*
** Definitions for SDMA_PKT_COPY_T2T packet
*/

typedef struct SDMA_PKT_COPY_T2T_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:4;
            unsigned int mip_max:4;
            unsigned int reserved_1:8;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_31_0:32;
        };
        unsigned int DW_1_DATA;
    } SRC_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_63_32:32;
        };
        unsigned int DW_2_DATA;
    } SRC_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int src_x:14;
            unsigned int reserved_0:2;
            unsigned int src_y:14;
            unsigned int reserved_1:2;
        };
        unsigned int DW_3_DATA;
    } DW_3_UNION;

    union
    {
        struct
        {
            unsigned int src_z:11;
            unsigned int reserved_0:5;
            unsigned int src_width:14;
            unsigned int reserved_1:2;
        };
        unsigned int DW_4_DATA;
    } DW_4_UNION;

    union
    {
        struct
        {
            unsigned int src_height:14;
            unsigned int reserved_0:2;
            unsigned int src_depth:11;
            unsigned int reserved_1:5;
        };
        unsigned int DW_5_DATA;
    } DW_5_UNION;

    union
    {
        struct
        {
            unsigned int src_element_size:3;
            unsigned int src_swizzle_mode:5;
            unsigned int reserved_0:1;
            unsigned int src_dimension:2;
            unsigned int reserved_1:5;
            unsigned int src_epitch:16;
        };
        unsigned int DW_6_DATA;
    } DW_6_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_31_0:32;
        };
        unsigned int DW_7_DATA;
    } DST_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_63_32:32;
        };
        unsigned int DW_8_DATA;
    } DST_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int dst_x:14;
            unsigned int reserved_0:2;
            unsigned int dst_y:14;
            unsigned int reserved_1:2;
        };
        unsigned int DW_9_DATA;
    } DW_9_UNION;

    union
    {
        struct
        {
            unsigned int dst_z:11;
            unsigned int reserved_0:5;
            unsigned int dst_width:14;
            unsigned int reserved_1:2;
        };
        unsigned int DW_10_DATA;
    } DW_10_UNION;

    union
    {
        struct
        {
            unsigned int dst_height:14;
            unsigned int reserved_0:2;
            unsigned int dst_depth:11;
            unsigned int reserved_1:5;
        };
        unsigned int DW_11_DATA;
    } DW_11_UNION;

    union
    {
        struct
        {
            unsigned int dst_element_size:3;
            unsigned int dst_swizzle_mode:5;
            unsigned int reserved_0:1;
            unsigned int dst_dimension:2;
            unsigned int reserved_1:5;
            unsigned int dst_epitch:16;
        };
        unsigned int DW_12_DATA;
    } DW_12_UNION;

    union
    {
        struct
        {
            unsigned int rect_x:14;
            unsigned int reserved_0:2;
            unsigned int rect_y:14;
            unsigned int reserved_1:2;
        };
        unsigned int DW_13_DATA;
    } DW_13_UNION;

    union
    {
        struct
        {
            unsigned int rect_z:11;
            unsigned int reserved_0:5;
            unsigned int dst_sw:2;
            unsigned int reserved_1:6;
            unsigned int src_sw:2;
            unsigned int reserved_2:6;
        };
        unsigned int DW_14_DATA;
    } DW_14_UNION;
} SDMA_PKT_COPY_T2T, *PSDMA_PKT_COPY_T2T;

/*
** Definitions for SDMA_PKT_COPY_TILED_SUBWIN packet
*/

typedef struct SDMA_PKT_COPY_TILED_SUBWIN_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:4;
            unsigned int mip_max:4;
            unsigned int mip_id:4;
            unsigned int reserved_1:3;
            unsigned int detile:1;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int tiled_addr_31_0:32;
        };
        unsigned int DW_1_DATA;
    } TILED_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int tiled_addr_63_32:32;
        };
        unsigned int DW_2_DATA;
    } TILED_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int tiled_x:14;
            unsigned int reserved_0:2;
            unsigned int tiled_y:14;
            unsigned int reserved_1:2;
        };
        unsigned int DW_3_DATA;
    } DW_3_UNION;

    union
    {
        struct
        {
            unsigned int tiled_z:11;
            unsigned int reserved_0:5;
            unsigned int width:14;
            unsigned int reserved_1:2;
        };
        unsigned int DW_4_DATA;
    } DW_4_UNION;

    union
    {
        struct
        {
            unsigned int height:14;
            unsigned int reserved_0:2;
            unsigned int depth:11;
            unsigned int reserved_1:5;
        };
        unsigned int DW_5_DATA;
    } DW_5_UNION;

    union
    {
        struct
        {
            unsigned int element_size:3;
            unsigned int swizzle_mode:5;
            unsigned int reserved_0:1;
            unsigned int dimension:2;
            unsigned int reserved_1:5;
            unsigned int epitch:16;
        };
        unsigned int DW_6_DATA;
    } DW_6_UNION;

    union
    {
        struct
        {
            unsigned int linear_addr_31_0:32;
        };
        unsigned int DW_7_DATA;
    } LINEAR_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int linear_addr_63_32:32;
        };
        unsigned int DW_8_DATA;
    } LINEAR_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int linear_x:14;
            unsigned int reserved_0:2;
            unsigned int linear_y:14;
            unsigned int reserved_1:2;
        };
        unsigned int DW_9_DATA;
    } DW_9_UNION;

    union
    {
        struct
        {
            unsigned int linear_z:11;
            unsigned int reserved_0:5;
            unsigned int linear_pitch:14;
            unsigned int reserved_1:2;
        };
        unsigned int DW_10_DATA;
    } DW_10_UNION;

    union
    {
        struct
        {
            unsigned int linear_slice_pitch:28;
            unsigned int reserved_0:4;
        };
        unsigned int DW_11_DATA;
    } DW_11_UNION;

    union
    {
        struct
        {
            unsigned int rect_x:14;
            unsigned int reserved_0:2;
            unsigned int rect_y:14;
            unsigned int reserved_1:2;
        };
        unsigned int DW_12_DATA;
    } DW_12_UNION;

    union
    {
        struct
        {
            unsigned int rect_z:11;
            unsigned int reserved_0:5;
            unsigned int linear_sw:2;
            unsigned int reserved_1:6;
            unsigned int tile_sw:2;
            unsigned int reserved_2:6;
        };
        unsigned int DW_13_DATA;
    } DW_13_UNION;
} SDMA_PKT_COPY_TILED_SUBWIN, *PSDMA_PKT_COPY_TILED_SUBWIN;

/*
** Definitions for SDMA_PKT_COPY_STRUCT packet
*/

typedef struct SDMA_PKT_COPY_STRUCT_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:15;
            unsigned int detile:1;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int sb_addr_31_0:32;
        };
        unsigned int DW_1_DATA;
    } SB_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int sb_addr_63_32:32;
        };
        unsigned int DW_2_DATA;
    } SB_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int start_index:32;
        };
        unsigned int DW_3_DATA;
    } START_INDEX_UNION;

    union
    {
        struct
        {
            unsigned int count:32;
        };
        unsigned int DW_4_DATA;
    } COUNT_UNION;

    union
    {
        struct
        {
            unsigned int stride:11;
            unsigned int reserved_0:5;
            unsigned int linear_sw:2;
            unsigned int reserved_1:6;
            unsigned int struct_sw:2;
            unsigned int reserved_2:6;
        };
        unsigned int DW_5_DATA;
    } DW_5_UNION;

    union
    {
        struct
        {
            unsigned int linear_addr_31_0:32;
        };
        unsigned int DW_6_DATA;
    } LINEAR_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int linear_addr_63_32:32;
        };
        unsigned int DW_7_DATA;
    } LINEAR_ADDR_HI_UNION;
} SDMA_PKT_COPY_STRUCT, *PSDMA_PKT_COPY_STRUCT;

/*
** Definitions for SDMA_PKT_WRITE_UNTILED packet
*/

typedef struct SDMA_PKT_WRITE_UNTILED_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:16;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_31_0:32;
        };
        unsigned int DW_1_DATA;
    } DST_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_63_32:32;
        };
        unsigned int DW_2_DATA;
    } DST_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int count:20;
            unsigned int reserved_0:4;
            unsigned int sw:2;
            unsigned int reserved_1:6;
        };
        unsigned int DW_3_DATA;
    } DW_3_UNION;

    union
    {
        struct
        {
            unsigned int data0:32;
        };
        unsigned int DW_4_DATA;
    } DATA0_UNION;
} SDMA_PKT_WRITE_UNTILED, *PSDMA_PKT_WRITE_UNTILED;

/*
** Definitions for SDMA_PKT_WRITE_TILED packet
*/

typedef struct SDMA_PKT_WRITE_TILED_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:4;
            unsigned int mip_max:4;
            unsigned int reserved_1:8;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_31_0:32;
        };
        unsigned int DW_1_DATA;
    } DST_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_63_32:32;
        };
        unsigned int DW_2_DATA;
    } DST_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int width:14;
            unsigned int reserved_0:18;
        };
        unsigned int DW_3_DATA;
    } DW_3_UNION;

    union
    {
        struct
        {
            unsigned int height:14;
            unsigned int reserved_0:2;
            unsigned int depth:11;
            unsigned int reserved_1:5;
        };
        unsigned int DW_4_DATA;
    } DW_4_UNION;

    union
    {
        struct
        {
            unsigned int element_size:3;
            unsigned int swizzle_mode:5;
            unsigned int reserved_0:1;
            unsigned int dimension:2;
            unsigned int reserved_1:5;
            unsigned int epitch:16;
        };
        unsigned int DW_5_DATA;
    } DW_5_UNION;

    union
    {
        struct
        {
            unsigned int x:14;
            unsigned int reserved_0:2;
            unsigned int y:14;
            unsigned int reserved_1:2;
        };
        unsigned int DW_6_DATA;
    } DW_6_UNION;

    union
    {
        struct
        {
            unsigned int z:11;
            unsigned int reserved_0:13;
            unsigned int sw:2;
            unsigned int reserved_1:6;
        };
        unsigned int DW_7_DATA;
    } DW_7_UNION;

    union
    {
        struct
        {
            unsigned int count:20;
            unsigned int reserved_0:12;
        };
        unsigned int DW_8_DATA;
    } COUNT_UNION;

    union
    {
        struct
        {
            unsigned int data0:32;
        };
        unsigned int DW_9_DATA;
    } DATA0_UNION;
} SDMA_PKT_WRITE_TILED, *PSDMA_PKT_WRITE_TILED;

/*
** Definitions for SDMA_PKT_PTEPDE_COPY packet
*/

typedef struct SDMA_PKT_PTEPDE_COPY_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:15;
            unsigned int ptepde_op:1;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_31_0:32;
        };
        unsigned int DW_1_DATA;
    } SRC_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_63_32:32;
        };
        unsigned int DW_2_DATA;
    } SRC_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_31_0:32;
        };
        unsigned int DW_3_DATA;
    } DST_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_63_32:32;
        };
        unsigned int DW_4_DATA;
    } DST_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int mask_dw0:32;
        };
        unsigned int DW_5_DATA;
    } MASK_DW0_UNION;

    union
    {
        struct
        {
            unsigned int mask_dw1:32;
        };
        unsigned int DW_6_DATA;
    } MASK_DW1_UNION;

    union
    {
        struct
        {
            unsigned int count:19;
            unsigned int reserved_0:13;
        };
        unsigned int DW_7_DATA;
    } COUNT_UNION;
} SDMA_PKT_PTEPDE_COPY, *PSDMA_PKT_PTEPDE_COPY;

/*
** Definitions for SDMA_PKT_PTEPDE_COPY_BACKWARDS packet
*/

typedef struct SDMA_PKT_PTEPDE_COPY_BACKWARDS_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:12;
            unsigned int pte_size:2;
            unsigned int direction:1;
            unsigned int ptepde_op:1;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_31_0:32;
        };
        unsigned int DW_1_DATA;
    } SRC_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_63_32:32;
        };
        unsigned int DW_2_DATA;
    } SRC_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_31_0:32;
        };
        unsigned int DW_3_DATA;
    } DST_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_63_32:32;
        };
        unsigned int DW_4_DATA;
    } DST_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int mask_first_xfer:8;
            unsigned int mask_last_xfer:8;
            unsigned int reserved_0:16;
        };
        unsigned int DW_5_DATA;
    } MASK_BIT_FOR_DW_UNION;

    union
    {
        struct
        {
            unsigned int count:17;
            unsigned int reserved_0:15;
        };
        unsigned int DW_6_DATA;
    } COUNT_IN_32B_XFER_UNION;
} SDMA_PKT_PTEPDE_COPY_BACKWARDS, *PSDMA_PKT_PTEPDE_COPY_BACKWARDS;

/*
** Definitions for SDMA_PKT_PTEPDE_RMW packet
*/

typedef struct SDMA_PKT_PTEPDE_RMW_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:3;
            unsigned int gcc:1;
            unsigned int sys:1;
            unsigned int reserved_1:1;
            unsigned int snp:1;
            unsigned int gpa:1;
            unsigned int reserved_2:8;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int addr_31_0:32;
        };
        unsigned int DW_1_DATA;
    } ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int addr_63_32:32;
        };
        unsigned int DW_2_DATA;
    } ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int mask_31_0:32;
        };
        unsigned int DW_3_DATA;
    } MASK_LO_UNION;

    union
    {
        struct
        {
            unsigned int mask_63_32:32;
        };
        unsigned int DW_4_DATA;
    } MASK_HI_UNION;

    union
    {
        struct
        {
            unsigned int value_31_0:32;
        };
        unsigned int DW_5_DATA;
    } VALUE_LO_UNION;

    union
    {
        struct
        {
            unsigned int value_63_32:32;
        };
        unsigned int DW_6_DATA;
    } VALUE_HI_UNION;

    union
    {
        struct
        {
            unsigned int num_of_pte:32;
        };
        unsigned int DW_7_DATA;
    } COUNT_UNION;
} SDMA_PKT_PTEPDE_RMW, *PSDMA_PKT_PTEPDE_RMW;

/*
** Definitions for SDMA_PKT_WRITE_INCR packet
*/

typedef struct SDMA_PKT_WRITE_INCR_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:16;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_31_0:32;
        };
        unsigned int DW_1_DATA;
    } DST_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_63_32:32;
        };
        unsigned int DW_2_DATA;
    } DST_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int mask_dw0:32;
        };
        unsigned int DW_3_DATA;
    } MASK_DW0_UNION;

    union
    {
        struct
        {
            unsigned int mask_dw1:32;
        };
        unsigned int DW_4_DATA;
    } MASK_DW1_UNION;

    union
    {
        struct
        {
            unsigned int init_dw0:32;
        };
        unsigned int DW_5_DATA;
    } INIT_DW0_UNION;

    union
    {
        struct
        {
            unsigned int init_dw1:32;
        };
        unsigned int DW_6_DATA;
    } INIT_DW1_UNION;

    union
    {
        struct
        {
            unsigned int incr_dw0:32;
        };
        unsigned int DW_7_DATA;
    } INCR_DW0_UNION;

    union
    {
        struct
        {
            unsigned int incr_dw1:32;
        };
        unsigned int DW_8_DATA;
    } INCR_DW1_UNION;

    union
    {
        struct
        {
            unsigned int count:19;
            unsigned int reserved_0:13;
        };
        unsigned int DW_9_DATA;
    } COUNT_UNION;
} SDMA_PKT_WRITE_INCR, *PSDMA_PKT_WRITE_INCR;

/*
** Definitions for SDMA_PKT_INDIRECT packet
*/

typedef struct SDMA_PKT_INDIRECT_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int vmid:4;
            unsigned int reserved_0:12;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int ib_base_31_0:32;
        };
        unsigned int DW_1_DATA;
    } BASE_LO_UNION;

    union
    {
        struct
        {
            unsigned int ib_base_63_32:32;
        };
        unsigned int DW_2_DATA;
    } BASE_HI_UNION;

    union
    {
        struct
        {
            unsigned int ib_size:20;
            unsigned int reserved_0:12;
        };
        unsigned int DW_3_DATA;
    } IB_SIZE_UNION;

    union
    {
        struct
        {
            unsigned int csa_addr_31_0:32;
        };
        unsigned int DW_4_DATA;
    } CSA_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int csa_addr_63_32:32;
        };
        unsigned int DW_5_DATA;
    } CSA_ADDR_HI_UNION;
} SDMA_PKT_INDIRECT, *PSDMA_PKT_INDIRECT;

/*
** Definitions for SDMA_PKT_SEMAPHORE packet
*/

typedef struct SDMA_PKT_SEMAPHORE_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:13;
            unsigned int write_one:1;
            unsigned int signal:1;
            unsigned int mailbox:1;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int addr_31_0:32;
        };
        unsigned int DW_1_DATA;
    } ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int addr_63_32:32;
        };
        unsigned int DW_2_DATA;
    } ADDR_HI_UNION;
} SDMA_PKT_SEMAPHORE, *PSDMA_PKT_SEMAPHORE;

/*
** Definitions for SDMA_PKT_MEM_INCR packet
*/

typedef struct SDMA_PKT_MEM_INCR_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:16;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int addr_31_0:32;
        };
        unsigned int DW_1_DATA;
    } ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int addr_63_32:32;
        };
        unsigned int DW_2_DATA;
    } ADDR_HI_UNION;
} SDMA_PKT_MEM_INCR, *PSDMA_PKT_MEM_INCR;

/*
** Definitions for SDMA_PKT_FENCE packet
*/

typedef struct SDMA_PKT_FENCE_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:16;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int addr_31_0:32;
        };
        unsigned int DW_1_DATA;
    } ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int addr_63_32:32;
        };
        unsigned int DW_2_DATA;
    } ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int data:32;
        };
        unsigned int DW_3_DATA;
    } DATA_UNION;
} SDMA_PKT_FENCE, *PSDMA_PKT_FENCE;

/*
** Definitions for SDMA_PKT_SRBM_WRITE packet
*/

typedef struct SDMA_PKT_SRBM_WRITE_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:12;
            unsigned int byte_en:4;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int addr:18;
            unsigned int reserved_0:14;
        };
        unsigned int DW_1_DATA;
    } ADDR_UNION;

    union
    {
        struct
        {
            unsigned int data:32;
        };
        unsigned int DW_2_DATA;
    } DATA_UNION;
} SDMA_PKT_SRBM_WRITE, *PSDMA_PKT_SRBM_WRITE;

/*
** Definitions for SDMA_PKT_PRE_EXE packet
*/

typedef struct SDMA_PKT_PRE_EXE_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int dev_sel:8;
            unsigned int reserved_0:8;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int exec_count:14;
            unsigned int reserved_0:18;
        };
        unsigned int DW_1_DATA;
    } EXEC_COUNT_UNION;
} SDMA_PKT_PRE_EXE, *PSDMA_PKT_PRE_EXE;

/*
** Definitions for SDMA_PKT_COND_EXE packet
*/

typedef struct SDMA_PKT_COND_EXE_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:16;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int addr_31_0:32;
        };
        unsigned int DW_1_DATA;
    } ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int addr_63_32:32;
        };
        unsigned int DW_2_DATA;
    } ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int reference:32;
        };
        unsigned int DW_3_DATA;
    } REFERENCE_UNION;

    union
    {
        struct
        {
            unsigned int exec_count:14;
            unsigned int reserved_0:18;
        };
        unsigned int DW_4_DATA;
    } EXEC_COUNT_UNION;
} SDMA_PKT_COND_EXE, *PSDMA_PKT_COND_EXE;

/*
** Definitions for SDMA_PKT_CONSTANT_FILL packet
*/

typedef struct SDMA_PKT_CONSTANT_FILL_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int sw:2;
            unsigned int reserved_0:12;
            unsigned int fillsize:2;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_31_0:32;
        };
        unsigned int DW_1_DATA;
    } DST_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_63_32:32;
        };
        unsigned int DW_2_DATA;
    } DST_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int src_data_31_0:32;
        };
        unsigned int DW_3_DATA;
    } DATA_UNION;

    union
    {
        struct
        {
            unsigned int count:22;
            unsigned int reserved_0:10;
        };
        unsigned int DW_4_DATA;
    } COUNT_UNION;
} SDMA_PKT_CONSTANT_FILL, *PSDMA_PKT_CONSTANT_FILL;

/*
** Definitions for SDMA_PKT_DATA_FILL_MULTI packet
*/

typedef struct SDMA_PKT_DATA_FILL_MULTI_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:15;
            unsigned int memlog_clr:1;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int byte_stride:32;
        };
        unsigned int DW_1_DATA;
    } BYTE_STRIDE_UNION;

    union
    {
        struct
        {
            unsigned int dma_count:32;
        };
        unsigned int DW_2_DATA;
    } DMA_COUNT_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_31_0:32;
        };
        unsigned int DW_3_DATA;
    } DST_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_63_32:32;
        };
        unsigned int DW_4_DATA;
    } DST_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int count:26;
            unsigned int reserved_0:6;
        };
        unsigned int DW_5_DATA;
    } BYTE_COUNT_UNION;
} SDMA_PKT_DATA_FILL_MULTI, *PSDMA_PKT_DATA_FILL_MULTI;

/*
** Definitions for SDMA_PKT_POLL_REGMEM packet
*/

typedef struct SDMA_PKT_POLL_REGMEM_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:10;
            unsigned int hdp_flush:1;
            unsigned int reserved_1:1;
            unsigned int func:3;
            unsigned int mem_poll:1;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int addr_31_0:32;
        };
        unsigned int DW_1_DATA;
    } ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int addr_63_32:32;
        };
        unsigned int DW_2_DATA;
    } ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int value:32;
        };
        unsigned int DW_3_DATA;
    } VALUE_UNION;

    union
    {
        struct
        {
            unsigned int mask:32;
        };
        unsigned int DW_4_DATA;
    } MASK_UNION;

    union
    {
        struct
        {
            unsigned int interval:16;
            unsigned int retry_count:12;
            unsigned int reserved_0:4;
        };
        unsigned int DW_5_DATA;
    } DW5_UNION;
} SDMA_PKT_POLL_REGMEM, *PSDMA_PKT_POLL_REGMEM;

/*
** Definitions for SDMA_PKT_POLL_REG_WRITE_MEM packet
*/

typedef struct SDMA_PKT_POLL_REG_WRITE_MEM_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:16;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int reserved_0:2;
            unsigned int addr_31_2:30;
        };
        unsigned int DW_1_DATA;
    } SRC_ADDR_UNION;

    union
    {
        struct
        {
            unsigned int addr_31_0:32;
        };
        unsigned int DW_2_DATA;
    } DST_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int addr_63_32:32;
        };
        unsigned int DW_3_DATA;
    } DST_ADDR_HI_UNION;
} SDMA_PKT_POLL_REG_WRITE_MEM, *PSDMA_PKT_POLL_REG_WRITE_MEM;

/*
** Definitions for SDMA_PKT_POLL_DBIT_WRITE_MEM packet
*/

typedef struct SDMA_PKT_POLL_DBIT_WRITE_MEM_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int ea:2;
            unsigned int reserved_0:14;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int addr_31_0:32;
        };
        unsigned int DW_1_DATA;
    } DST_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int addr_63_32:32;
        };
        unsigned int DW_2_DATA;
    } DST_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int reserved_0:4;
            unsigned int addr_31_4:28;
        };
        unsigned int DW_3_DATA;
    } START_PAGE_UNION;

    union
    {
        struct
        {
            unsigned int page_num_31_0:32;
        };
        unsigned int DW_4_DATA;
    } PAGE_NUM_UNION;
} SDMA_PKT_POLL_DBIT_WRITE_MEM, *PSDMA_PKT_POLL_DBIT_WRITE_MEM;

/*
** Definitions for SDMA_PKT_POLL_MEM_VERIFY packet
*/

typedef struct SDMA_PKT_POLL_MEM_VERIFY_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:15;
            unsigned int mode:1;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int pattern:32;
        };
        unsigned int DW_1_DATA;
    } PATTERN_UNION;

    union
    {
        struct
        {
            unsigned int cmp0_start_31_0:32;
        };
        unsigned int DW_2_DATA;
    } CMP0_ADDR_START_LO_UNION;

    union
    {
        struct
        {
            unsigned int cmp0_start_63_32:32;
        };
        unsigned int DW_3_DATA;
    } CMP0_ADDR_START_HI_UNION;

    union
    {
        struct
        {
            unsigned int cmp1_end_31_0:32;
        };
        unsigned int DW_4_DATA;
    } CMP0_ADDR_END_LO_UNION;

    union
    {
        struct
        {
            unsigned int cmp1_end_63_32:32;
        };
        unsigned int DW_5_DATA;
    } CMP0_ADDR_END_HI_UNION;

    union
    {
        struct
        {
            unsigned int cmp1_start_31_0:32;
        };
        unsigned int DW_6_DATA;
    } CMP1_ADDR_START_LO_UNION;

    union
    {
        struct
        {
            unsigned int cmp1_start_63_32:32;
        };
        unsigned int DW_7_DATA;
    } CMP1_ADDR_START_HI_UNION;

    union
    {
        struct
        {
            unsigned int cmp1_end_31_0:32;
        };
        unsigned int DW_8_DATA;
    } CMP1_ADDR_END_LO_UNION;

    union
    {
        struct
        {
            unsigned int cmp1_end_63_32:32;
        };
        unsigned int DW_9_DATA;
    } CMP1_ADDR_END_HI_UNION;

    union
    {
        struct
        {
            unsigned int rec_31_0:32;
        };
        unsigned int DW_10_DATA;
    } REC_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int rec_63_32:32;
        };
        unsigned int DW_11_DATA;
    } REC_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int reserved:32;
        };
        unsigned int DW_12_DATA;
    } RESERVED_UNION;
} SDMA_PKT_POLL_MEM_VERIFY, *PSDMA_PKT_POLL_MEM_VERIFY;

/*
** Definitions for SDMA_PKT_ATOMIC packet
*/

typedef struct SDMA_PKT_ATOMIC_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int reserved_0:8;
            unsigned int loop:1;
            unsigned int reserved_1:8;
            unsigned int atomic_op:7;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int addr_31_0:32;
        };
        unsigned int DW_1_DATA;
    } ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int addr_63_32:32;
        };
        unsigned int DW_2_DATA;
    } ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int src_data_31_0:32;
        };
        unsigned int DW_3_DATA;
    } SRC_DATA_LO_UNION;

    union
    {
        struct
        {
            unsigned int src_data_63_32:32;
        };
        unsigned int DW_4_DATA;
    } SRC_DATA_HI_UNION;

    union
    {
        struct
        {
            unsigned int cmp_data_31_0:32;
        };
        unsigned int DW_5_DATA;
    } CMP_DATA_LO_UNION;

    union
    {
        struct
        {
            unsigned int cmp_data_63_32:32;
        };
        unsigned int DW_6_DATA;
    } CMP_DATA_HI_UNION;

    union
    {
        struct
        {
            unsigned int loop_interval:13;
            unsigned int reserved_0:19;
        };
        unsigned int DW_7_DATA;
    } LOOP_INTERVAL_UNION;
} SDMA_PKT_ATOMIC, *PSDMA_PKT_ATOMIC;

/*
** Definitions for SDMA_PKT_TIMESTAMP_SET packet
*/

typedef struct SDMA_PKT_TIMESTAMP_SET_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:16;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int init_data_31_0:32;
        };
        unsigned int DW_1_DATA;
    } INIT_DATA_LO_UNION;

    union
    {
        struct
        {
            unsigned int init_data_63_32:32;
        };
        unsigned int DW_2_DATA;
    } INIT_DATA_HI_UNION;
} SDMA_PKT_TIMESTAMP_SET, *PSDMA_PKT_TIMESTAMP_SET;

/*
** Definitions for SDMA_PKT_TIMESTAMP_GET packet
*/

typedef struct SDMA_PKT_TIMESTAMP_GET_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:16;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int reserved_0:3;
            unsigned int write_addr_31_3:29;
        };
        unsigned int DW_1_DATA;
    } WRITE_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int write_addr_63_32:32;
        };
        unsigned int DW_2_DATA;
    } WRITE_ADDR_HI_UNION;
} SDMA_PKT_TIMESTAMP_GET, *PSDMA_PKT_TIMESTAMP_GET;

/*
** Definitions for SDMA_PKT_TIMESTAMP_GET_GLOBAL packet
*/

typedef struct SDMA_PKT_TIMESTAMP_GET_GLOBAL_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:16;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int reserved_0:3;
            unsigned int write_addr_31_3:29;
        };
        unsigned int DW_1_DATA;
    } WRITE_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int write_addr_63_32:32;
        };
        unsigned int DW_2_DATA;
    } WRITE_ADDR_HI_UNION;
} SDMA_PKT_TIMESTAMP_GET_GLOBAL, *PSDMA_PKT_TIMESTAMP_GET_GLOBAL;

/*
** Definitions for SDMA_PKT_TRAP packet
*/

typedef struct SDMA_PKT_TRAP_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:16;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int int_context:28;
            unsigned int reserved_0:4;
        };
        unsigned int DW_1_DATA;
    } INT_CONTEXT_UNION;
} SDMA_PKT_TRAP, *PSDMA_PKT_TRAP;

/*
** Definitions for SDMA_PKT_DUMMY_TRAP packet
*/

typedef struct SDMA_PKT_DUMMY_TRAP_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:16;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int int_context:28;
            unsigned int reserved_0:4;
        };
        unsigned int DW_1_DATA;
    } INT_CONTEXT_UNION;
} SDMA_PKT_DUMMY_TRAP, *PSDMA_PKT_DUMMY_TRAP;

/*
** Definitions for SDMA_PKT_NOP packet
*/

typedef struct SDMA_PKT_NOP_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int count:14;
            unsigned int reserved_0:2;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int data0:32;
        };
        unsigned int DW_1_DATA;
    } DATA0_UNION;
} SDMA_PKT_NOP, *PSDMA_PKT_NOP;

/*
** Definitions for SDMA_AQL_PKT_HEADER packet
*/

typedef struct SDMA_AQL_PKT_HEADER_TAG
{

    union
    {
        struct
        {
            unsigned int format:8;
            unsigned int barrier:1;
            unsigned int acquire_fence_scope:2;
            unsigned int release_fence_scope:2;
            unsigned int reserved:3;
            unsigned int op:4;
            unsigned int subop:3;
            unsigned int reserved_0:9;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;
} SDMA_AQL_PKT_HEADER, *PSDMA_AQL_PKT_HEADER;

/*
** Definitions for SDMA_AQL_PKT_COPY_LINEAR packet
*/

typedef struct SDMA_AQL_PKT_COPY_LINEAR_TAG
{

    union
    {
        struct
        {
            unsigned int format:8;
            unsigned int barrier:1;
            unsigned int acquire_fence_scope:2;
            unsigned int release_fence_scope:2;
            unsigned int reserved:3;
            unsigned int op:4;
            unsigned int subop:3;
            unsigned int reserved_0:9;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int reserved_dw1:32;
        };
        unsigned int DW_1_DATA;
    } RESERVED_DW1_UNION;

    union
    {
        struct
        {
            unsigned int return_addr_31_0:32;
        };
        unsigned int DW_2_DATA;
    } RETURN_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int return_addr_63_32:32;
        };
        unsigned int DW_3_DATA;
    } RETURN_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int count:22;
            unsigned int reserved_0:10;
        };
        unsigned int DW_4_DATA;
    } COUNT_UNION;

    union
    {
        struct
        {
            unsigned int reserved_0:16;
            unsigned int dst_sw:2;
            unsigned int reserved_1:6;
            unsigned int src_sw:2;
            unsigned int reserved_2:6;
        };
        unsigned int DW_5_DATA;
    } PARAMETER_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_31_0:32;
        };
        unsigned int DW_6_DATA;
    } SRC_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int src_addr_63_32:32;
        };
        unsigned int DW_7_DATA;
    } SRC_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_31_0:32;
        };
        unsigned int DW_8_DATA;
    } DST_ADDR_LO_UNION;

    union
    {
        struct
        {
            unsigned int dst_addr_63_32:32;
        };
        unsigned int DW_9_DATA;
    } DST_ADDR_HI_UNION;

    union
    {
        struct
        {
            unsigned int reserved_dw10:32;
        };
        unsigned int DW_10_DATA;
    } RESERVED_DW10_UNION;

    union
    {
        struct
        {
            unsigned int reserved_dw11:32;
        };
        unsigned int DW_11_DATA;
    } RESERVED_DW11_UNION;

    union
    {
        struct
        {
            unsigned int reserved_dw12:32;
        };
        unsigned int DW_12_DATA;
    } RESERVED_DW12_UNION;

    union
    {
        struct
        {
            unsigned int reserved_dw13:32;
        };
        unsigned int DW_13_DATA;
    } RESERVED_DW13_UNION;

    union
    {
        struct
        {
            unsigned int completion_signal_31_0:32;
        };
        unsigned int DW_14_DATA;
    } COMPLETION_SIGNAL_LO_UNION;

    union
    {
        struct
        {
            unsigned int completion_signal_63_32:32;
        };
        unsigned int DW_15_DATA;
    } COMPLETION_SIGNAL_HI_UNION;
} SDMA_AQL_PKT_COPY_LINEAR, *PSDMA_AQL_PKT_COPY_LINEAR;

/*
** Definitions for SDMA_AQL_PKT_BARRIER_OR packet
*/

typedef struct SDMA_AQL_PKT_BARRIER_OR_TAG
{

    union
    {
        struct
        {
            unsigned int format:8;
            unsigned int barrier:1;
            unsigned int acquire_fence_scope:2;
            unsigned int release_fence_scope:2;
            unsigned int reserved:3;
            unsigned int op:4;
            unsigned int subop:3;
            unsigned int reserved_0:9;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int reserved_dw1:32;
        };
        unsigned int DW_1_DATA;
    } RESERVED_DW1_UNION;

    union
    {
        struct
        {
            unsigned int dependent_addr_0_31_0:32;
        };
        unsigned int DW_2_DATA;
    } DEPENDENT_ADDR_0_LO_UNION;

    union
    {
        struct
        {
            unsigned int dependent_addr_0_63_32:32;
        };
        unsigned int DW_3_DATA;
    } DEPENDENT_ADDR_0_HI_UNION;

    union
    {
        struct
        {
            unsigned int dependent_addr_1_31_0:32;
        };
        unsigned int DW_4_DATA;
    } DEPENDENT_ADDR_1_LO_UNION;

    union
    {
        struct
        {
            unsigned int dependent_addr_1_63_32:32;
        };
        unsigned int DW_5_DATA;
    } DEPENDENT_ADDR_1_HI_UNION;

    union
    {
        struct
        {
            unsigned int dependent_addr_2_31_0:32;
        };
        unsigned int DW_6_DATA;
    } DEPENDENT_ADDR_2_LO_UNION;

    union
    {
        struct
        {
            unsigned int dependent_addr_2_63_32:32;
        };
        unsigned int DW_7_DATA;
    } DEPENDENT_ADDR_2_HI_UNION;

    union
    {
        struct
        {
            unsigned int dependent_addr_3_31_0:32;
        };
        unsigned int DW_8_DATA;
    } DEPENDENT_ADDR_3_LO_UNION;

    union
    {
        struct
        {
            unsigned int dependent_addr_3_63_32:32;
        };
        unsigned int DW_9_DATA;
    } DEPENDENT_ADDR_3_HI_UNION;

    union
    {
        struct
        {
            unsigned int dependent_addr_4_31_0:32;
        };
        unsigned int DW_10_DATA;
    } DEPENDENT_ADDR_4_LO_UNION;

    union
    {
        struct
        {
            unsigned int dependent_addr_4_63_32:32;
        };
        unsigned int DW_11_DATA;
    } DEPENDENT_ADDR_4_HI_UNION;

    union
    {
        struct
        {
            unsigned int reserved_dw12:32;
        };
        unsigned int DW_12_DATA;
    } RESERVED_DW12_UNION;

    union
    {
        struct
        {
            unsigned int reserved_dw13:32;
        };
        unsigned int DW_13_DATA;
    } RESERVED_DW13_UNION;

    union
    {
        struct
        {
            unsigned int completion_signal_31_0:32;
        };
        unsigned int DW_14_DATA;
    } COMPLETION_SIGNAL_LO_UNION;

    union
    {
        struct
        {
            unsigned int completion_signal_63_32:32;
        };
        unsigned int DW_15_DATA;
    } COMPLETION_SIGNAL_HI_UNION;
} SDMA_AQL_PKT_BARRIER_OR, *PSDMA_AQL_PKT_BARRIER_OR;

#endif // _SDMA40_PKT_H_
