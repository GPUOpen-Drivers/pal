/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef _SDMA24_PKT_H_
#define _SDMA24_PKT_H_

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
const unsigned int SDMA_OP_GEN_PTEPDE = 12;
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
const unsigned int SDMA_SUBOP_WRITE_LINEAR = 0;
const unsigned int SDMA_SUBOP_WRITE_TILED = 1;

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
            unsigned int reserved_1:4;
            unsigned int dst_ha:1;
            unsigned int reserved_2:1;
            unsigned int src_sw:2;
            unsigned int reserved_3:4;
            unsigned int src_ha:1;
            unsigned int reserved_4:1;
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
            unsigned int reserved_0:8;
            unsigned int dst2_sw:2;
            unsigned int reserved_1:4;
            unsigned int dst2_ha:1;
            unsigned int reserved_2:1;
            unsigned int dst1_sw:2;
            unsigned int reserved_3:4;
            unsigned int dst1_ha:1;
            unsigned int reserved_4:1;
            unsigned int src_sw:2;
            unsigned int reserved_5:4;
            unsigned int src_ha:1;
            unsigned int reserved_6:1;
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
            unsigned int reserved_0:2;
            unsigned int tmz:1;
            unsigned int reserved_1:10;
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
            unsigned int reserved_0:5;
            unsigned int src_pitch:14;
            unsigned int reserved_1:2;
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
            unsigned int reserved_0:5;
            unsigned int dst_pitch:14;
            unsigned int reserved_1:2;
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
            unsigned int reserved_1:4;
            unsigned int dst_ha:1;
            unsigned int reserved_2:1;
            unsigned int src_sw:2;
            unsigned int reserved_3:4;
            unsigned int src_ha:1;
            unsigned int reserved_4:1;
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
            unsigned int reserved_0 : 2;
            unsigned int tmz : 1;
            unsigned int reserved_1 : 12;
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
            unsigned int pitch_in_tile:11;
            unsigned int reserved_0:5;
            unsigned int height:14;
            unsigned int reserved_1:2;
        };
        unsigned int DW_3_DATA;
    } DW_3_UNION;

    union
    {
        struct
        {
            unsigned int slice_pitch:22;
            unsigned int reserved_0:10;
        };
        unsigned int DW_4_DATA;
    } DW_4_UNION;

    union
    {
        struct
        {
            unsigned int element_size:3;
            unsigned int array_mode:4;
            unsigned int reserved_0:1;
            unsigned int mit_mode:3;
            unsigned int tilesplit_size:3;
            unsigned int reserved_1:1;
            unsigned int bank_w:2;
            unsigned int reserved_2:1;
            unsigned int bank_h:2;
            unsigned int reserved_3:1;
            unsigned int num_bank:2;
            unsigned int reserved_4:1;
            unsigned int mat_aspt:2;
            unsigned int pipe_config:5;
            unsigned int reserved_5:1;
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
            unsigned int z:12;
            unsigned int reserved_0:4;
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
            unsigned int count:20;
            unsigned int reserved_0:12;
        };
        unsigned int DW_11_DATA;
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
            unsigned int reserved_0:2;
            unsigned int tmz:1;
            unsigned int reserved_1:7;
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
            unsigned int pitch_in_tile:11;
            unsigned int reserved_0:5;
            unsigned int height:14;
            unsigned int reserved_1:2;
        };
        unsigned int DW_5_DATA;
    } DW_5_UNION;

    union
    {
        struct
        {
            unsigned int slice_pitch:22;
            unsigned int reserved_0:10;
        };
        unsigned int DW_6_DATA;
    } DW_6_UNION;

    union
    {
        struct
        {
            unsigned int element_size:3;
            unsigned int array_mode:4;
            unsigned int reserved_0:1;
            unsigned int mit_mode:3;
            unsigned int tilesplit_size:3;
            unsigned int reserved_1:1;
            unsigned int bank_w:2;
            unsigned int reserved_2:1;
            unsigned int bank_h:2;
            unsigned int reserved_3:1;
            unsigned int num_bank:2;
            unsigned int reserved_4:1;
            unsigned int mat_aspt:2;
            unsigned int pipe_config:5;
            unsigned int reserved_5:1;
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
            unsigned int z:12;
            unsigned int reserved_0:20;
        };
        unsigned int DW_9_DATA;
    } DW_9_UNION;

    union
    {
        struct
        {
            unsigned int reserved_0:8;
            unsigned int dst2_sw:2;
            unsigned int reserved_1:4;
            unsigned int dst2_ha:1;
            unsigned int reserved_2:1;
            unsigned int linear_sw:2;
            unsigned int reserved_3:6;
            unsigned int tile_sw:2;
            unsigned int reserved_4:6;
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
            unsigned int count:20;
            unsigned int reserved_0:12;
        };
        unsigned int DW_14_DATA;
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
            unsigned int reserved_0:2;
            unsigned int tmz:1;
            unsigned int reserved_1:13;
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
            unsigned int src_pitch_in_tile:12;
            unsigned int reserved_1:4;
        };
        unsigned int DW_4_DATA;
    } DW_4_UNION;

    union
    {
        struct
        {
            unsigned int src_slice_pitch:22;
            unsigned int reserved_0:10;
        };
        unsigned int DW_5_DATA;
    } DW_5_UNION;

    union
    {
        struct
        {
            unsigned int src_element_size:3;
            unsigned int src_array_mode:4;
            unsigned int reserved_0:1;
            unsigned int src_mit_mode:3;
            unsigned int src_tilesplit_size:3;
            unsigned int reserved_1:1;
            unsigned int src_bank_w:2;
            unsigned int reserved_2:1;
            unsigned int src_bank_h:2;
            unsigned int reserved_3:1;
            unsigned int src_num_bank:2;
            unsigned int reserved_4:1;
            unsigned int src_mat_aspt:2;
            unsigned int src_pipe_config:5;
            unsigned int reserved_5:1;
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
            unsigned int dst_pitch_in_tile:12;
            unsigned int reserved_1:4;
        };
        unsigned int DW_10_DATA;
    } DW_10_UNION;

    union
    {
        struct
        {
            unsigned int dst_slice_pitch:22;
            unsigned int reserved_0:10;
        };
        unsigned int DW_11_DATA;
    } DW_11_UNION;

    union
    {
        struct
        {
            unsigned int reserved_0:3;
            unsigned int dst_array_mode:4;
            unsigned int reserved_1:1;
            unsigned int dst_mit_mode:3;
            unsigned int dst_tilesplit_size:3;
            unsigned int reserved_2:1;
            unsigned int dst_bank_w:2;
            unsigned int reserved_3:1;
            unsigned int dst_bank_h:2;
            unsigned int reserved_4:1;
            unsigned int dst_num_bank:2;
            unsigned int reserved_5:1;
            unsigned int dst_mat_aspt:2;
            unsigned int dst_pipe_config:5;
            unsigned int reserved_6:1;
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
            unsigned int reserved_0:2;
            unsigned int tmz:1;
            unsigned int reserved_1:12;
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
            unsigned int pitch_in_tile:12;
            unsigned int reserved_1:4;
        };
        unsigned int DW_4_DATA;
    } DW_4_UNION;

    union
    {
        struct
        {
            unsigned int slice_pitch:22;
            unsigned int reserved_0:10;
        };
        unsigned int DW_5_DATA;
    } DW_5_UNION;

    union
    {
        struct
        {
            unsigned int element_size:3;
            unsigned int array_mode:4;
            unsigned int reserved_0:1;
            unsigned int mit_mode:3;
            unsigned int tilesplit_size:3;
            unsigned int reserved_1:1;
            unsigned int bank_w:2;
            unsigned int reserved_2:1;
            unsigned int bank_h:2;
            unsigned int reserved_3:1;
            unsigned int num_bank:2;
            unsigned int reserved_4:1;
            unsigned int mat_aspt:2;
            unsigned int pipe_config:5;
            unsigned int reserved_5:1;
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
            unsigned int reserved_0:2;
            unsigned int tmz:1;
            unsigned int reserved_1:15;
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
            unsigned int struct_sw:2;
            unsigned int reserved_1:4;
            unsigned int struct_ha:1;
            unsigned int reserved_2:1;
            unsigned int linear_sw:2;
            unsigned int reserved_3:4;
            unsigned int linear_ha:1;
            unsigned int reserved_4:1;
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
            unsigned int reserved_0:2;
            unsigned int tmz:1;
            unsigned int reserved_1:13;
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
            unsigned int count:22;
            unsigned int reserved_0:2;
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
            unsigned int reserved_0:2;
            unsigned int tmz:1;
            unsigned int reserved_1:13;
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
            unsigned int pitch_in_tile:11;
            unsigned int reserved_0:5;
            unsigned int height:14;
            unsigned int reserved_1:2;
        };
        unsigned int DW_3_DATA;
    } DW_3_UNION;

    union
    {
        struct
        {
            unsigned int slice_pitch:22;
            unsigned int reserved_0:10;
        };
        unsigned int DW_4_DATA;
    } DW_4_UNION;

    union
    {
        struct
        {
            unsigned int element_size:3;
            unsigned int array_mode:4;
            unsigned int reserved_0:1;
            unsigned int mit_mode:3;
            unsigned int tilesplit_size:3;
            unsigned int reserved_1:1;
            unsigned int bank_w:2;
            unsigned int reserved_2:1;
            unsigned int bank_h:2;
            unsigned int reserved_3:1;
            unsigned int num_bank:2;
            unsigned int reserved_4:1;
            unsigned int mat_aspt:2;
            unsigned int pipe_config:5;
            unsigned int reserved_5:1;
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
            unsigned int z:12;
            unsigned int reserved_0:12;
            unsigned int sw:2;
            unsigned int reserved_1:6;
        };
        unsigned int DW_7_DATA;
    } DW_7_UNION;

    union
    {
        struct
        {
            unsigned int count:22;
            unsigned int reserved_0:10;
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
            unsigned int reserved_0:2;
            unsigned int tmz:1;
            unsigned int reserved_1:13;
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
** Definitions for SDMA_PKT_DRM_OFFSET packet
*/

typedef struct SDMA_PKT_DRM_OFFSET_TAG
{

    union
    {
        struct
        {
            unsigned int op:8;
            unsigned int sub_op:8;
            unsigned int reserved_0:15;
            unsigned int ch:1;
        };
        unsigned int DW_0_DATA;
    } HEADER_UNION;

    union
    {
        struct
        {
            unsigned int reserved_0:6;
            unsigned int offset_31_6:26;
        };
        unsigned int DW_1_DATA;
    } OFFSET_UNION;
} SDMA_PKT_DRM_OFFSET, *PSDMA_PKT_DRM_OFFSET;

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
            unsigned int addr:16;
            unsigned int reserved_0:16;
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
} SDMA_PKT_NOP, *PSDMA_PKT_NOP;

#endif // _SDMA24_PKT_H_
