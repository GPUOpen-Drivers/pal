/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
// Make sure the necessary endian defines are there.
//
#ifndef LITTLEENDIAN_CPU
#error "LITTLEENDIAN_CPU must be defined"
#endif

#include <stdint.h>

namespace Pal
{
namespace Gfx9
{
inline namespace Chip
{
// ------------------------------------- PM4_MEC_TYPE_3_HEADER -------------------------------------
typedef union PM4_MEC_TYPE_3_HEADER
{
    struct
    {
        uint32_t reserved1 :  8;
        uint32_t opcode    :  8;
        uint32_t count     : 14;
        uint32_t type      :  2;
    };
    uint32_t u32All;
} PM4_MEC_TYPE_3_HEADER;

// -------------------------------------- PM4_MEC_ACQUIRE_MEM --------------------------------------
typedef struct PM4_MEC_ACQUIRE_MEM
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t coher_cntl : 31;
                uint32_t reserved1  :  1;
            } gfx09;
        } bitfields;
        uint32_t reserved1;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t coher_size;
        uint32_t u32All;
    } ordinal3;

    union
    {
        union
        {
            struct
            {
                uint32_t coher_size_hi :  8;
                uint32_t reserved1     : 24;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t coher_base_lo;
        uint32_t u32All;
    } ordinal5;

    union
    {
        union
        {
            struct
            {
                uint32_t coher_base_hi : 24;
                uint32_t reserved1     :  8;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal6;

    union
    {
        union
        {
            struct
            {
                uint32_t poll_interval : 16;
                uint32_t reserved1     : 16;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal7;

    union
    {
        union
        {
            struct
            {
                uint32_t gcr_cntl   : 19;
                uint32_t reserved1  : 13;
            } gfx10Plus;
        } bitfields;
        uint32_t u32All;
    } ordinal8;
} PM4_MEC_ACQUIRE_MEM;

constexpr unsigned int PM4_MEC_ACQUIRE_MEM_SIZEDW__CORE      = 7;
constexpr unsigned int PM4_MEC_ACQUIRE_MEM_SIZEDW__GFX10PLUS = 8;

// ------------------------------- MEC_ATOMIC_GDS_atom_cmp_swap_enum -------------------------------
enum MEC_ATOMIC_GDS_atom_cmp_swap_enum
{
    atom_cmp_swap__mec_atomic_gds__dont_repeat__CORE       =  0,
    atom_cmp_swap__mec_atomic_gds__repeat_until_pass__CORE =  1,
};

// ------------------------------- MEC_ATOMIC_GDS_atom_complete_enum -------------------------------
enum MEC_ATOMIC_GDS_atom_complete_enum
{
    atom_complete__mec_atomic_gds__dont_wait__CORE           =  0,
    atom_complete__mec_atomic_gds__wait_for_completion__CORE =  1,
};

// --------------------------------- MEC_ATOMIC_GDS_atom_read_enum ---------------------------------
enum MEC_ATOMIC_GDS_atom_read_enum
{
    atom_read__mec_atomic_gds__dont_read_preop_data__CORE =  0,
    atom_read__mec_atomic_gds__read_preop_data__CORE      =  1,
};

// ------------------------------- MEC_ATOMIC_GDS_atom_rd_cntl_enum -------------------------------
enum MEC_ATOMIC_GDS_atom_rd_cntl_enum
{
    atom_rd_cntl__mec_atomic_gds__32bits_1returnval__CORE =  0,
    atom_rd_cntl__mec_atomic_gds__32bits_2returnval__CORE =  1,
    atom_rd_cntl__mec_atomic_gds__64bits_1returnval__CORE =  2,
    atom_rd_cntl__mec_atomic_gds__64bits_2returnval__CORE =  3,
};

// -------------------------------------- PM4_MEC_ATOMIC_GDS --------------------------------------
typedef struct PM4_MEC_ATOMIC_GDS
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                          atom_op       :  8;
                uint32_t                          reserved1     :  8;
                MEC_ATOMIC_GDS_atom_cmp_swap_enum atom_cmp_swap :  1;
                MEC_ATOMIC_GDS_atom_complete_enum atom_complete :  1;
                MEC_ATOMIC_GDS_atom_read_enum     atom_read     :  1;
                MEC_ATOMIC_GDS_atom_rd_cntl_enum  atom_rd_cntl  :  2;
                uint32_t                          reserved2     : 11;
            } core;
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        union
        {
            struct
            {
                uint32_t auto_inc_bytes :  6;
                uint32_t reserved1      :  2;
                uint32_t dmode          :  1;
                uint32_t reserved2      : 23;
            } core;
        } bitfields;
        uint32_t u32All;
    } ordinal3;

    union
    {
        union
        {
            struct
            {
                uint32_t atom_base  : 16;
                uint32_t reserved1  : 16;
            } core;
        } bitfields;
        uint32_t u32All;
    } ordinal4;

    union
    {
        union
        {
            struct
            {
                uint32_t atom_size  : 16;
                uint32_t reserved1  : 16;
            } core;
        } bitfields;
        uint32_t u32All;
    } ordinal5;

    union
    {
        union
        {
            struct
            {
                uint32_t atom_offset0 :  8;
                uint32_t reserved1    :  8;
                uint32_t atom_offset1 :  8;
                uint32_t reserved2    :  8;
            } core;
        } bitfields;
        uint32_t u32All;
    } ordinal6;

    union
    {
        uint32_t atom_dst;
        uint32_t u32All;
    } ordinal7;

    union
    {
        uint32_t atom_src0;
        uint32_t u32All;
    } ordinal8;

    union
    {
        uint32_t atom_src0_u;
        uint32_t u32All;
    } ordinal9;

    union
    {
        uint32_t atom_src1;
        uint32_t u32All;
    } ordinal10;

    union
    {
        uint32_t atom_src1_u;
        uint32_t u32All;
    } ordinal11;
} PM4_MEC_ATOMIC_GDS;

constexpr unsigned int PM4_MEC_ATOMIC_GDS_SIZEDW__CORE = 11;

// ---------------------------------- MEC_ATOMIC_MEM_command_enum ----------------------------------
enum MEC_ATOMIC_MEM_command_enum
{
    command__mec_atomic_mem__single_pass_atomic                     =  0,
    command__mec_atomic_mem__loop_until_compare_satisfied           =  1,
    command__mec_atomic_mem__wait_for_write_confirmation__GFX10PLUS =  2,
    command__mec_atomic_mem__send_and_continue__GFX10PLUS           =  3,
};

// ------------------------------- MEC_ATOMIC_MEM_cache_policy_enum -------------------------------
enum MEC_ATOMIC_MEM_cache_policy_enum
{
    cache_policy__mec_atomic_mem__lru               =  0,
    cache_policy__mec_atomic_mem__stream            =  1,
    cache_policy__mec_atomic_mem__noa__GFX10PLUS    =  2,
    cache_policy__mec_atomic_mem__bypass__GFX10PLUS =  3,
};

// -------------------------------------- PM4_MEC_ATOMIC_MEM --------------------------------------
typedef struct PM4_MEC_ATOMIC_MEM
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                         atomic       :  7;
                uint32_t                         reserved1    :  1;
                MEC_ATOMIC_MEM_command_enum      command      :  4;
                uint32_t                         reserved2    : 13;
                MEC_ATOMIC_MEM_cache_policy_enum cache_policy :  2;
                uint32_t                         reserved3    :  5;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t addr_lo;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t addr_hi;
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t src_data_lo;
        uint32_t u32All;
    } ordinal5;

    union
    {
        uint32_t src_data_hi;
        uint32_t u32All;
    } ordinal6;

    union
    {
        uint32_t cmp_data_lo;
        uint32_t u32All;
    } ordinal7;

    union
    {
        uint32_t cmp_data_hi;
        uint32_t u32All;
    } ordinal8;

    union
    {
        union
        {
            struct
            {
                uint32_t loop_interval : 13;
                uint32_t reserved1     : 19;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal9;
} PM4_MEC_ATOMIC_MEM;

constexpr unsigned int PM4_MEC_ATOMIC_MEM_SIZEDW__CORE = 9;

// -------------------------------- MEC_COND_EXEC_cache_policy_enum --------------------------------
enum MEC_COND_EXEC_cache_policy_enum
{
    cache_policy__mec_cond_exec__lru               =  0,
    cache_policy__mec_cond_exec__stream            =  1,
    cache_policy__mec_cond_exec__noa__GFX10PLUS    =  2,
    cache_policy__mec_cond_exec__bypass__GFX10PLUS =  3,
};

// --------------------------------------- PM4_MEC_COND_EXEC ---------------------------------------
typedef struct PM4_MEC_COND_EXEC
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1  :  2;
                uint32_t addr_lo    : 30;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t addr_hi;
        uint32_t u32All;
    } ordinal3;

    union
    {
        union
        {
            struct
            {
                uint32_t                        reserved1    : 25;
                MEC_COND_EXEC_cache_policy_enum cache_policy :  2;
                uint32_t                        reserved2    :  5;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal4;

    union
    {
        union
        {
            struct
            {
                uint32_t exec_count : 14;
                uint32_t reserved1  : 18;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal5;
} PM4_MEC_COND_EXEC;

constexpr unsigned int PM4_MEC_COND_EXEC_SIZEDW__CORE = 5;

// ------------------------------ MEC_COND_INDIRECT_BUFFER_mode_enum ------------------------------
enum MEC_COND_INDIRECT_BUFFER_mode_enum
{
    mode__mec_cond_indirect_buffer__if_then      =  1,
    mode__mec_cond_indirect_buffer__if_then_else =  2,
};

// ---------------------------- MEC_COND_INDIRECT_BUFFER_function_enum ----------------------------
enum MEC_COND_INDIRECT_BUFFER_function_enum
{
    function__mec_cond_indirect_buffer__always_pass                           =  0,
    function__mec_cond_indirect_buffer__less_than_ref_value                   =  1,
    function__mec_cond_indirect_buffer__less_than_equal_to_the_ref_value      =  2,
    function__mec_cond_indirect_buffer__equal_to_the_reference_value          =  3,
    function__mec_cond_indirect_buffer__not_equal_reference_value             =  4,
    function__mec_cond_indirect_buffer__greater_than_or_equal_reference_value =  5,
    function__mec_cond_indirect_buffer__greater_than_reference_value          =  6,
};

// -------------------------- MEC_COND_INDIRECT_BUFFER_cache_policy1_enum --------------------------
enum MEC_COND_INDIRECT_BUFFER_cache_policy1_enum
{
    cache_policy1__mec_cond_indirect_buffer__lru               =  0,
    cache_policy1__mec_cond_indirect_buffer__stream            =  1,
    cache_policy1__mec_cond_indirect_buffer__noa__GFX10PLUS    =  2,
    cache_policy1__mec_cond_indirect_buffer__bypass__GFX10PLUS =  3,
};

// -------------------------- MEC_COND_INDIRECT_BUFFER_cache_policy2_enum --------------------------
enum MEC_COND_INDIRECT_BUFFER_cache_policy2_enum
{
    cache_policy2__mec_cond_indirect_buffer__lru               =  0,
    cache_policy2__mec_cond_indirect_buffer__stream            =  1,
    cache_policy2__mec_cond_indirect_buffer__noa__GFX10PLUS    =  2,
    cache_policy2__mec_cond_indirect_buffer__bypass__GFX10PLUS =  3,
};

// --------------------------------- PM4_MEC_COND_INDIRECT_BUFFER ---------------------------------
typedef struct PM4_MEC_COND_INDIRECT_BUFFER
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                MEC_COND_INDIRECT_BUFFER_mode_enum     mode       :  2;
                uint32_t                               reserved1  :  6;
                MEC_COND_INDIRECT_BUFFER_function_enum function   :  3;
                uint32_t                               reserved2  : 21;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1       :  3;
                uint32_t compare_addr_lo : 29;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t compare_addr_hi;
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t mask_lo;
        uint32_t u32All;
    } ordinal5;

    union
    {
        uint32_t mask_hi;
        uint32_t u32All;
    } ordinal6;

    union
    {
        uint32_t reference_lo;
        uint32_t u32All;
    } ordinal7;

    union
    {
        uint32_t reference_hi;
        uint32_t u32All;
    } ordinal8;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1   :  2;
                uint32_t ib_base1_lo : 30;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal9;

    union
    {
        uint32_t ib_base1_hi;
        uint32_t u32All;
    } ordinal10;

    union
    {
        union
        {
            struct
            {
                uint32_t                                    ib_size1      : 20;
                uint32_t                                    reserved1     :  8;
                MEC_COND_INDIRECT_BUFFER_cache_policy1_enum cache_policy1 :  2;
                uint32_t                                    reserved2     :  2;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal11;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1   :  2;
                uint32_t ib_base2_lo : 30;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal12;

    union
    {
        uint32_t ib_base2_hi;
        uint32_t u32All;
    } ordinal13;

    union
    {
        union
        {
            struct
            {
                uint32_t                                    ib_size2      : 20;
                uint32_t                                    reserved1     :  8;
                MEC_COND_INDIRECT_BUFFER_cache_policy2_enum cache_policy2 :  2;
                uint32_t                                    reserved2     :  2;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal14;
} PM4_MEC_COND_INDIRECT_BUFFER;

constexpr unsigned int PM4_MEC_COND_INDIRECT_BUFFER_SIZEDW__CORE = 14;

// --------------------------------- MEC_COND_WRITE_function_enum ---------------------------------
enum MEC_COND_WRITE_function_enum
{
    function__mec_cond_write__always_pass                           =  0,
    function__mec_cond_write__less_than_ref_value                   =  1,
    function__mec_cond_write__less_than_equal_to_the_ref_value      =  2,
    function__mec_cond_write__equal_to_the_reference_value          =  3,
    function__mec_cond_write__not_equal_reference_value             =  4,
    function__mec_cond_write__greater_than_or_equal_reference_value =  5,
    function__mec_cond_write__greater_than_reference_value          =  6,
};

// -------------------------------- MEC_COND_WRITE_poll_space_enum --------------------------------
enum MEC_COND_WRITE_poll_space_enum
{
    poll_space__mec_cond_write__register =  0,
    poll_space__mec_cond_write__memory   =  1,
};

// -------------------------------- MEC_COND_WRITE_write_space_enum --------------------------------
enum MEC_COND_WRITE_write_space_enum
{
    write_space__mec_cond_write__register =  0,
    write_space__mec_cond_write__memory   =  1,
    write_space__mec_cond_write__scratch  =  2,
};

// -------------------------------------- PM4_MEC_COND_WRITE --------------------------------------
typedef struct PM4_MEC_COND_WRITE
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                MEC_COND_WRITE_function_enum    function    :  3;
                uint32_t                        reserved1   :  1;
                MEC_COND_WRITE_poll_space_enum  poll_space  :  1;
                uint32_t                        reserved2   :  3;
                MEC_COND_WRITE_write_space_enum write_space :  2;
                uint32_t                        reserved3   : 22;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t poll_address_lo;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t poll_address_hi;
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t reference;
        uint32_t u32All;
    } ordinal5;

    union
    {
        uint32_t mask;
        uint32_t u32All;
    } ordinal6;

    union
    {
        uint32_t write_address_lo;
        uint32_t u32All;
    } ordinal7;

    union
    {
        uint32_t write_address_hi;
        uint32_t u32All;
    } ordinal8;

    union
    {
        uint32_t write_data;
        uint32_t u32All;
    } ordinal9;
} PM4_MEC_COND_WRITE;

constexpr unsigned int PM4_MEC_COND_WRITE_SIZEDW__CORE = 9;

// ---------------------------------- MEC_COPY_DATA_src_sel_enum ----------------------------------
enum MEC_COPY_DATA_src_sel_enum
{
    src_sel__mec_copy_data__mem_mapped_register           =  0,
    src_sel__mec_copy_data__memory__GFX09                 =  1,
    src_sel__mec_copy_data__tc_l2_obsolete__GFX10PLUS     =  1,
    src_sel__mec_copy_data__tc_l2                         =  2,
    src_sel__mec_copy_data__gds__CORE                     =  3,
    src_sel__mec_copy_data__perfcounters                  =  4,
    src_sel__mec_copy_data__immediate_data                =  5,
    src_sel__mec_copy_data__atomic_return_data            =  6,
    src_sel__mec_copy_data__gds_atomic_return_data0__CORE =  7,
    src_sel__mec_copy_data__gds_atomic_return_data1__CORE =  8,
    src_sel__mec_copy_data__gpu_clock_count               =  9,
    src_sel__mec_copy_data__system_clock_count            = 10,
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
    src_sel__mec_copy_data__exec_ind_arg_buf__GFX11       = 12,
#endif
};

// ---------------------------------- MEC_COPY_DATA_dst_sel_enum ----------------------------------
enum MEC_COPY_DATA_dst_sel_enum
{
    dst_sel__mec_copy_data__mem_mapped_register         =  0,
    dst_sel__mec_copy_data__tc_l2                       =  2,
    dst_sel__mec_copy_data__gds__CORE                   =  3,
    dst_sel__mec_copy_data__perfcounters                =  4,
    dst_sel__mec_copy_data__memory__GFX09               =  5,
    dst_sel__mec_copy_data__tc_l2_obsolete__GFX10PLUS   =  5,
    dst_sel__mec_copy_data__mem_mapped_reg_dc           =  6,
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
    dst_sel__mec_copy_data__exec_ind_spill_table__GFX11 =  7,
#endif
};

// ------------------------------ MEC_COPY_DATA_src_cache_policy_enum ------------------------------
enum MEC_COPY_DATA_src_cache_policy_enum
{
    src_cache_policy__mec_copy_data__lru               =  0,
    src_cache_policy__mec_copy_data__stream            =  1,
    src_cache_policy__mec_copy_data__noa__GFX10PLUS    =  2,
    src_cache_policy__mec_copy_data__bypass__GFX10PLUS =  3,
};

// --------------------------------- MEC_COPY_DATA_count_sel_enum ---------------------------------
enum MEC_COPY_DATA_count_sel_enum
{
    count_sel__mec_copy_data__32_bits_of_data =  0,
    count_sel__mec_copy_data__64_bits_of_data =  1,
};

// --------------------------------- MEC_COPY_DATA_wr_confirm_enum ---------------------------------
enum MEC_COPY_DATA_wr_confirm_enum
{
    wr_confirm__mec_copy_data__do_not_wait_for_confirmation =  0,
    wr_confirm__mec_copy_data__wait_for_confirmation        =  1,
};

// ------------------------------ MEC_COPY_DATA_dst_cache_policy_enum ------------------------------
enum MEC_COPY_DATA_dst_cache_policy_enum
{
    dst_cache_policy__mec_copy_data__lru               =  0,
    dst_cache_policy__mec_copy_data__stream            =  1,
    dst_cache_policy__mec_copy_data__noa__GFX10PLUS    =  2,
    dst_cache_policy__mec_copy_data__bypass__GFX10PLUS =  3,
};

// ------------------------------- MEC_COPY_DATA_pq_exe_status_enum -------------------------------
enum MEC_COPY_DATA_pq_exe_status_enum
{
    pq_exe_status__mec_copy_data__default      =  0,
    pq_exe_status__mec_copy_data__phase_update =  1,
};

// --------------------------------------- PM4_MEC_COPY_DATA ---------------------------------------
typedef struct PM4_MEC_COPY_DATA
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                MEC_COPY_DATA_src_sel_enum          src_sel          :  4;
                uint32_t                            reserved1        :  4;
                MEC_COPY_DATA_dst_sel_enum          dst_sel          :  4;
                uint32_t                            reserved2        :  1;
                MEC_COPY_DATA_src_cache_policy_enum src_cache_policy :  2;
                uint32_t                            reserved3        :  1;
                MEC_COPY_DATA_count_sel_enum        count_sel        :  1;
                uint32_t                            reserved4        :  3;
                MEC_COPY_DATA_wr_confirm_enum       wr_confirm       :  1;
                uint32_t                            reserved5        :  4;
                MEC_COPY_DATA_dst_cache_policy_enum dst_cache_policy :  2;
                uint32_t                            reserved6        :  2;
                MEC_COPY_DATA_pq_exe_status_enum    pq_exe_status    :  1;
                uint32_t                            reserved7        :  2;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        union
        {
            struct
            {
                uint32_t src_reg_offset : 18;
                uint32_t reserved1      : 14;
            };
        } bitfieldsA;
        union
        {
            struct
            {
                uint32_t reserved2       :  2;
                uint32_t src_32b_addr_lo : 30;
            };
        } bitfieldsB;
        union
        {
            struct
            {
                uint32_t reserved3       :  3;
                uint32_t src_64b_addr_lo : 29;
            };
        } bitfieldsC;
        union
        {
            struct
            {
                uint32_t src_gds_addr_lo : 16;
                uint32_t reserved4       : 16;
            } core;
        } bitfieldsD;
        uint32_t imm_data;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t src_memtc_addr_hi;
        uint32_t src_imm_data;
        uint32_t u32All;
    } ordinal4;

    union
    {
        union
        {
            struct
            {
                uint32_t dst_reg_offset : 18;
                uint32_t reserved1      : 14;
            };
        } bitfieldsA;
        union
        {
            struct
            {
                uint32_t reserved2       :  2;
                uint32_t dst_32b_addr_lo : 30;
            };
        } bitfieldsB;
        union
        {
            struct
            {
                uint32_t reserved3       :  3;
                uint32_t dst_64b_addr_lo : 29;
            };
        } bitfieldsC;
        union
        {
            struct
            {
                uint32_t dst_gds_addr_lo : 16;
                uint32_t reserved4       : 16;
            } core;
        } bitfieldsD;
        uint32_t u32All;
    } ordinal5;

    union
    {
        uint32_t dst_addr_hi;
        uint32_t u32All;
    } ordinal6;
} PM4_MEC_COPY_DATA;

constexpr unsigned int PM4_MEC_COPY_DATA_SIZEDW__CORE = 6;

// ------------------------------------ PM4_MEC_DISPATCH_DIRECT ------------------------------------
typedef struct PM4_MEC_DISPATCH_DIRECT
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        uint32_t dim_x;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t dim_y;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t dim_z;
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t dispatch_initiator;
        uint32_t u32All;
    } ordinal5;
} PM4_MEC_DISPATCH_DIRECT;

constexpr unsigned int PM4_MEC_DISPATCH_DIRECT_SIZEDW__CORE = 5;

// ----------------------------------- PM4_MEC_DISPATCH_DRAW_ACE -----------------------------------
typedef struct PM4_MEC_DISPATCH_DRAW_ACE
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t krb_loc_sgpr :  4;
                uint32_t reserved1    : 28;
            } hasCe;
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t dim_x;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t dim_y;
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t dim_z;
        uint32_t u32All;
    } ordinal5;

    union
    {
        uint32_t dispatch_initiator;
        uint32_t u32All;
    } ordinal6;
} PM4_MEC_DISPATCH_DRAW_ACE;

constexpr unsigned int PM4_MEC_DISPATCH_DRAW_ACE_SIZEDW__HASCE = 6;

// ------------------------------ PM4_MEC_DISPATCH_DRAW_PREAMBLE_ACE ------------------------------
typedef struct PM4_MEC_DISPATCH_DRAW_PREAMBLE_ACE
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t krb_size        : 10;
                uint32_t krb_free_offset : 10;
                uint32_t krb_offset      : 10;
                uint32_t reserved1       :  2;
            } hasCe;
        } bitfields;
        uint32_t u32All;
    } ordinal2;
} PM4_MEC_DISPATCH_DRAW_PREAMBLE_ACE;

constexpr unsigned int PM4_MEC_DISPATCH_DRAW_PREAMBLE_ACE_SIZEDW__HASCE = 2;

// ----------------------------------- PM4_MEC_DISPATCH_INDIRECT -----------------------------------
typedef struct PM4_MEC_DISPATCH_INDIRECT
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        uint32_t addr_lo;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t addr_hi;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t dispatch_initiator;
        uint32_t u32All;
    } ordinal4;
} PM4_MEC_DISPATCH_INDIRECT;

constexpr unsigned int PM4_MEC_DISPATCH_INDIRECT_SIZEDW__CORE = 4;

// ------------------------------ MEC_DMA_DATA_src_cache_policy_enum ------------------------------
enum MEC_DMA_DATA_src_cache_policy_enum
{
    src_cache_policy__mec_dma_data__lru               =  0,
    src_cache_policy__mec_dma_data__stream            =  1,
    src_cache_policy__mec_dma_data__noa__GFX10PLUS    =  2,
    src_cache_policy__mec_dma_data__bypass__GFX10PLUS =  3,
};

// ----------------------------------- MEC_DMA_DATA_dst_sel_enum -----------------------------------
enum MEC_DMA_DATA_dst_sel_enum
{
    dst_sel__mec_dma_data__dst_addr_using_das =  0,
    dst_sel__mec_dma_data__gds__CORE          =  1,
    dst_sel__mec_dma_data__dst_nowhere        =  2,
    dst_sel__mec_dma_data__dst_addr_using_l2  =  3,
};

// ------------------------------ MEC_DMA_DATA_dst_cache_policy_enum ------------------------------
enum MEC_DMA_DATA_dst_cache_policy_enum
{
    dst_cache_policy__mec_dma_data__lru               =  0,
    dst_cache_policy__mec_dma_data__stream            =  1,
    dst_cache_policy__mec_dma_data__noa__GFX10PLUS    =  2,
    dst_cache_policy__mec_dma_data__bypass__GFX10PLUS =  3,
};

// ----------------------------------- MEC_DMA_DATA_src_sel_enum -----------------------------------
enum MEC_DMA_DATA_src_sel_enum
{
    src_sel__mec_dma_data__src_addr_using_sas =  0,
    src_sel__mec_dma_data__gds__CORE          =  1,
    src_sel__mec_dma_data__data               =  2,
    src_sel__mec_dma_data__src_addr_using_l2  =  3,
};

// ------------------------------------- MEC_DMA_DATA_sas_enum -------------------------------------
enum MEC_DMA_DATA_sas_enum
{
    sas__mec_dma_data__memory =  0,
};

// ------------------------------------- MEC_DMA_DATA_das_enum -------------------------------------
enum MEC_DMA_DATA_das_enum
{
    das__mec_dma_data__memory =  0,
};

// ------------------------------------ MEC_DMA_DATA_saic_enum ------------------------------------
enum MEC_DMA_DATA_saic_enum
{
    saic__mec_dma_data__increment    =  0,
    saic__mec_dma_data__no_increment =  1,
};

// ------------------------------------ MEC_DMA_DATA_daic_enum ------------------------------------
enum MEC_DMA_DATA_daic_enum
{
    daic__mec_dma_data__increment    =  0,
    daic__mec_dma_data__no_increment =  1,
};

// --------------------------------------- PM4_MEC_DMA_DATA ---------------------------------------
typedef struct PM4_MEC_DMA_DATA
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                           reserved1        : 13;
                MEC_DMA_DATA_src_cache_policy_enum src_cache_policy :  2;
                uint32_t                           reserved2        :  5;
                MEC_DMA_DATA_dst_sel_enum          dst_sel          :  2;
                uint32_t                           reserved3        :  3;
                MEC_DMA_DATA_dst_cache_policy_enum dst_cache_policy :  2;
                uint32_t                           reserved4        :  2;
                MEC_DMA_DATA_src_sel_enum          src_sel          :  2;
                uint32_t                           reserved5        :  1;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t src_addr_lo_or_data;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t src_addr_hi;
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t dst_addr_lo;
        uint32_t u32All;
    } ordinal5;

    union
    {
        uint32_t dst_addr_hi;
        uint32_t u32All;
    } ordinal6;

    union
    {
        union
        {
            struct
            {
                uint32_t               byte_count : 26;
                MEC_DMA_DATA_sas_enum  sas        :  1;
                MEC_DMA_DATA_das_enum  das        :  1;
                MEC_DMA_DATA_saic_enum saic       :  1;
                MEC_DMA_DATA_daic_enum daic       :  1;
                uint32_t               raw_wait   :  1;
                uint32_t               dis_wc     :  1;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal7;
} PM4_MEC_DMA_DATA;

constexpr unsigned int PM4_MEC_DMA_DATA_SIZEDW__CORE = 7;

// ----------------------------- MEC_DMA_DATA_FILL_MULTI_dst_sel_enum -----------------------------
enum MEC_DMA_DATA_FILL_MULTI_dst_sel_enum
{
    dst_sel__mec_dma_data_fill_multi__dst_addr_using_l2__CORE =  3,
};

// ------------------------- MEC_DMA_DATA_FILL_MULTI_dst_cache_policy_enum -------------------------
enum MEC_DMA_DATA_FILL_MULTI_dst_cache_policy_enum
{
    dst_cache_policy__mec_dma_data_fill_multi__lru__CORE             =  0,
    dst_cache_policy__mec_dma_data_fill_multi__stream__CORE          =  1,
    dst_cache_policy__mec_dma_data_fill_multi__noa__GFX10COREPLUS    =  2,
    dst_cache_policy__mec_dma_data_fill_multi__bypass__GFX10COREPLUS =  3,
};

// ----------------------------- MEC_DMA_DATA_FILL_MULTI_src_sel_enum -----------------------------
enum MEC_DMA_DATA_FILL_MULTI_src_sel_enum
{
    src_sel__mec_dma_data_fill_multi__data__CORE =  2,
};

// ---------------------------------- PM4_MEC_DMA_DATA_FILL_MULTI ----------------------------------
typedef struct PM4_MEC_DMA_DATA_FILL_MULTI
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                                      reserved1        : 10;
                uint32_t                                      memlog_clear     :  1;
                uint32_t                                      reserved2        :  9;
                MEC_DMA_DATA_FILL_MULTI_dst_sel_enum          dst_sel          :  2;
                uint32_t                                      reserved3        :  3;
                MEC_DMA_DATA_FILL_MULTI_dst_cache_policy_enum dst_cache_policy :  2;
                uint32_t                                      reserved4        :  2;
                MEC_DMA_DATA_FILL_MULTI_src_sel_enum          src_sel          :  2;
                uint32_t                                      reserved5        :  1;
            } core;
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t byte_stride;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t dma_count;
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t dst_addr_lo;
        uint32_t u32All;
    } ordinal5;

    union
    {
        uint32_t dst_addr_hi;
        uint32_t u32All;
    } ordinal6;

    union
    {
        union
        {
            struct
            {
                uint32_t byte_count : 26;
                uint32_t reserved1  :  6;
            } core;
        } bitfields;
        uint32_t u32All;
    } ordinal7;
} PM4_MEC_DMA_DATA_FILL_MULTI;

constexpr unsigned int PM4_MEC_DMA_DATA_FILL_MULTI_SIZEDW__CORE = 7;

// ------------------------------- MEC_EVENT_WRITE_event_index_enum -------------------------------
enum MEC_EVENT_WRITE_event_index_enum
{
    event_index__mec_event_write__other                         =  0,
    event_index__mec_event_write__sample_pipelinestat           =  2,
    event_index__mec_event_write__cs_partial_flush              =  4,
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
    event_index__mec_event_write__sample_streamoutstats__GFX11  =  8,
    event_index__mec_event_write__sample_streamoutstats1__GFX11 =  9,
    event_index__mec_event_write__sample_streamoutstats2__GFX11 = 10,
    event_index__mec_event_write__sample_streamoutstats3__GFX11 = 11,
#endif
};

#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
// --------------------------- MEC_EVENT_WRITE_samp_plst_cntr_mode_enum ---------------------------
enum MEC_EVENT_WRITE_samp_plst_cntr_mode_enum
{
    samp_plst_cntr_mode__mec_event_write__legacy_mode__GFX11 =  0,
    samp_plst_cntr_mode__mec_event_write__mixed_mode1__GFX11 =  1,
    samp_plst_cntr_mode__mec_event_write__new_mode__GFX11    =  2,
    samp_plst_cntr_mode__mec_event_write__mixed_mode3__GFX11 =  3,
};
#endif

// -------------------------------------- PM4_MEC_EVENT_WRITE --------------------------------------
typedef struct PM4_MEC_EVENT_WRITE
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                         event_type     :  6;
                uint32_t                         reserved1      :  2;
                MEC_EVENT_WRITE_event_index_enum event_index    :  4;
                uint32_t                         reserved2      : 19;
                uint32_t                         offload_enable :  1;
            };
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
            struct
            {
                uint32_t                                 reserved3           : 29;
                MEC_EVENT_WRITE_samp_plst_cntr_mode_enum samp_plst_cntr_mode :  2;
                uint32_t                                 reserved4           :  1;
            } gfx11;
#endif
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1  :  3;
                uint32_t address_lo : 29;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t address_hi;
        uint32_t u32All;
    } ordinal4;
} PM4_MEC_EVENT_WRITE;

constexpr unsigned int PM4_MEC_EVENT_WRITE_SIZEDW__CORE = 4;

// --------------------------------------- PM4_MEC_HDP_FLUSH ---------------------------------------
typedef struct PM4_MEC_HDP_FLUSH
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        uint32_t dummy;
        uint32_t u32All;
    } ordinal2;
} PM4_MEC_HDP_FLUSH;

constexpr unsigned int PM4_MEC_HDP_FLUSH_SIZEDW__CORE = 2;

// ----------------------------- MEC_INDIRECT_BUFFER_cache_policy_enum -----------------------------
enum MEC_INDIRECT_BUFFER_cache_policy_enum
{
    cache_policy__mec_indirect_buffer__lru               =  0,
    cache_policy__mec_indirect_buffer__stream            =  1,
    cache_policy__mec_indirect_buffer__noa__GFX10PLUS    =  2,
    cache_policy__mec_indirect_buffer__bypass__GFX10PLUS =  3,
};

// ------------------------------------ PM4_MEC_INDIRECT_BUFFER ------------------------------------
typedef struct PM4_MEC_INDIRECT_BUFFER
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1  :  2;
                uint32_t ib_base_lo : 30;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t ib_base_hi;
        uint32_t u32All;
    } ordinal3;

    union
    {
        union
        {
            struct
            {
                uint32_t                              ib_size         : 20;
                uint32_t                              chain           :  1;
                uint32_t                              offload_polling :  1;
                uint32_t                              reserved1       :  1;
                uint32_t                              valid           :  1;
                uint32_t                              vmid            :  4;
                MEC_INDIRECT_BUFFER_cache_policy_enum cache_policy    :  2;
                uint32_t                              reserved2       :  1;
                uint32_t                              reserved3       :  1;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal4;
} PM4_MEC_INDIRECT_BUFFER;

constexpr unsigned int PM4_MEC_INDIRECT_BUFFER_SIZEDW__CORE = 4;

// -------------------------- MEC_INDIRECT_BUFFER_PASID_cache_policy_enum --------------------------
enum MEC_INDIRECT_BUFFER_PASID_cache_policy_enum
{
    cache_policy__mec_indirect_buffer_pasid__lru__CORE             =  0,
    cache_policy__mec_indirect_buffer_pasid__stream__CORE          =  1,
    cache_policy__mec_indirect_buffer_pasid__noa__GFX10COREPLUS    =  2,
    cache_policy__mec_indirect_buffer_pasid__bypass__GFX10COREPLUS =  3,
};

// --------------------------------- PM4_MEC_INDIRECT_BUFFER_PASID ---------------------------------
typedef struct PM4_MEC_INDIRECT_BUFFER_PASID
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1  :  2;
                uint32_t ib_base_lo : 30;
            } core;
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t ib_base_hi;
        uint32_t u32All;
    } ordinal3;

    union
    {
        union
        {
            struct
            {
                uint32_t                                    ib_size         : 20;
                uint32_t                                    chain           :  1;
                uint32_t                                    offload_polling :  1;
                uint32_t                                    reserved1       :  1;
                uint32_t                                    valid           :  1;
                uint32_t                                    reserved2       :  4;
                MEC_INDIRECT_BUFFER_PASID_cache_policy_enum cache_policy    :  2;
                uint32_t                                    reserved3       :  1;
                uint32_t                                    reserved4       :  1;
            } core;
        } bitfields;
        uint32_t u32All;
    } ordinal4;

    union
    {
        union
        {
            struct
            {
                uint32_t pasid      : 16;
                uint32_t reserved1  : 16;
            } core;
        } bitfields;
        uint32_t u32All;
    } ordinal5;
} PM4_MEC_INDIRECT_BUFFER_PASID;

constexpr unsigned int PM4_MEC_INDIRECT_BUFFER_PASID_SIZEDW__CORE = 5;

// ---------------------------- MEC_INVALIDATE_TLBS_invalidate_sel_enum ----------------------------
enum MEC_INVALIDATE_TLBS_invalidate_sel_enum
{
    invalidate_sel__mec_invalidate_tlbs__invalidate__CORE =  0,
    invalidate_sel__mec_invalidate_tlbs__use_pasid__CORE  =  1,
};

// ------------------------- MEC_INVALIDATE_TLBS_mmhub_invalidate_sel_enum -------------------------
enum MEC_INVALIDATE_TLBS_mmhub_invalidate_sel_enum
{
    mmhub_invalidate_sel__mec_invalidate_tlbs__do_not_invalidate_mmhub__CORE =  0,
    mmhub_invalidate_sel__mec_invalidate_tlbs__use_mmhub_flush_type__CORE    =  1,
    mmhub_invalidate_sel__mec_invalidate_tlbs__use_gfx_flush_type__CORE      =  2,
};

// ------------------------------------ PM4_MEC_INVALIDATE_TLBS ------------------------------------
typedef struct PM4_MEC_INVALIDATE_TLBS
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1      : 29;
                uint32_t gfx_flush_type :  3;
            };
            struct
            {
                MEC_INVALIDATE_TLBS_invalidate_sel_enum       invalidate_sel       :  3;
                MEC_INVALIDATE_TLBS_mmhub_invalidate_sel_enum mmhub_invalidate_sel :  2;
                uint32_t                                      pasid                : 16;
                uint32_t                                      reserved2            :  4;
                uint32_t                                      mmhub_flush_type     :  3;
                uint32_t                                      reserved3            :  4;
            } core;
        } bitfields;
        uint32_t u32All;
    } ordinal2;
} PM4_MEC_INVALIDATE_TLBS;

constexpr unsigned int PM4_MEC_INVALIDATE_TLBS_SIZEDW__CORE = 2;

// ------------------------------ MEC_MEM_SEMAPHORE_use_mailbox_enum ------------------------------
enum MEC_MEM_SEMAPHORE_use_mailbox_enum
{
    use_mailbox__mec_mem_semaphore__do_not_wait_for_mailbox__GFX09_GFX10CORE =  0,
    use_mailbox__mec_mem_semaphore__wait_for_mailbox__GFX09_GFX10CORE        =  1,
};

// ------------------------------ MEC_MEM_SEMAPHORE_signal_type_enum ------------------------------
enum MEC_MEM_SEMAPHORE_signal_type_enum
{
    signal_type__mec_mem_semaphore__signal_type_increment__GFX09_GFX10CORE =  0,
    signal_type__mec_mem_semaphore__signal_type_write__GFX09_GFX10CORE     =  1,
};

// -------------------------------- MEC_MEM_SEMAPHORE_sem_sel_enum --------------------------------
enum MEC_MEM_SEMAPHORE_sem_sel_enum
{
    sem_sel__mec_mem_semaphore__signal_semaphore__GFX09_GFX10CORE =  6,
    sem_sel__mec_mem_semaphore__wait_semaphore__GFX09_GFX10CORE   =  7,
};

// ------------------------------------- PM4_MEC_MEM_SEMAPHORE -------------------------------------
typedef struct PM4_MEC_MEM_SEMAPHORE
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1  :  3;
                uint32_t address_lo : 29;
            } hasCe;
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t address_hi;
        uint32_t u32All;
    } ordinal3;

    union
    {
        union
        {
            struct
            {
                uint32_t                           reserved1   : 16;
                MEC_MEM_SEMAPHORE_use_mailbox_enum use_mailbox :  1;
                uint32_t                           reserved2   :  3;
                MEC_MEM_SEMAPHORE_signal_type_enum signal_type :  1;
                uint32_t                           reserved3   :  8;
                MEC_MEM_SEMAPHORE_sem_sel_enum     sem_sel     :  3;
            } hasCe;
        } bitfields;
        uint32_t u32All;
    } ordinal4;
} PM4_MEC_MEM_SEMAPHORE;

constexpr unsigned int PM4_MEC_MEM_SEMAPHORE_SIZEDW__HASCE = 4;

// ------------------------------------------ PM4_MEC_NOP ------------------------------------------
typedef struct PM4_MEC_NOP
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;
} PM4_MEC_NOP;

constexpr unsigned int PM4_MEC_NOP_SIZEDW__CORE = 1;

// -------------------------------- MEC_PRIME_UTCL2_cache_perm_enum --------------------------------
enum MEC_PRIME_UTCL2_cache_perm_enum
{
    cache_perm__mec_prime_utcl2__read    =  0,
    cache_perm__mec_prime_utcl2__write   =  1,
    cache_perm__mec_prime_utcl2__execute =  2,
};

// -------------------------------- MEC_PRIME_UTCL2_prime_mode_enum --------------------------------
enum MEC_PRIME_UTCL2_prime_mode_enum
{
    prime_mode__mec_prime_utcl2__dont_wait_for_xack =  0,
    prime_mode__mec_prime_utcl2__wait_for_xack      =  1,
};

// -------------------------------------- PM4_MEC_PRIME_UTCL2 --------------------------------------
typedef struct PM4_MEC_PRIME_UTCL2
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                MEC_PRIME_UTCL2_cache_perm_enum cache_perm :  3;
                MEC_PRIME_UTCL2_prime_mode_enum prime_mode :  1;
                uint32_t                        reserved1  : 28;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t addr_lo;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t addr_hi;
        uint32_t u32All;
    } ordinal4;

    union
    {
        union
        {
            struct
            {
                uint32_t requested_pages : 14;
                uint32_t reserved1       : 18;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal5;
} PM4_MEC_PRIME_UTCL2;

constexpr unsigned int PM4_MEC_PRIME_UTCL2_SIZEDW__CORE = 5;

// ------------------------------- MEC_RELEASE_MEM_event_index_enum -------------------------------
enum MEC_RELEASE_MEM_event_index_enum
{
    event_index__mec_release_mem__end_of_pipe =  5,
    event_index__mec_release_mem__shader_done =  6,
};

// ------------------------------- MEC_RELEASE_MEM_cache_policy_enum -------------------------------
enum MEC_RELEASE_MEM_cache_policy_enum
{
    cache_policy__mec_release_mem__lru               =  0,
    cache_policy__mec_release_mem__stream            =  1,
    cache_policy__mec_release_mem__noa__GFX10PLUS    =  2,
    cache_policy__mec_release_mem__bypass__GFX10PLUS =  3,
};

// ------------------------------ MEC_RELEASE_MEM_pq_exe_status_enum ------------------------------
enum MEC_RELEASE_MEM_pq_exe_status_enum
{
    pq_exe_status__mec_release_mem__default__GFX09_10      =  0,
    pq_exe_status__mec_release_mem__phase_update__GFX09_10 =  1,
};

// --------------------------------- MEC_RELEASE_MEM_dst_sel_enum ---------------------------------
enum MEC_RELEASE_MEM_dst_sel_enum
{
    dst_sel__mec_release_mem__memory_controller                           =  0,
    dst_sel__mec_release_mem__tc_l2                                       =  1,
    dst_sel__mec_release_mem__queue_write_pointer_register__NOTGFX10      =  2,
    dst_sel__mec_release_mem__queue_write_pointer_poll_mask_bit__NOTGFX10 =  3,
};

// --------------------------------- MEC_RELEASE_MEM_int_sel_enum ---------------------------------
enum MEC_RELEASE_MEM_int_sel_enum
{
    int_sel__mec_release_mem__none                                                 =  0,
    int_sel__mec_release_mem__send_interrupt_only                                  =  1,
    int_sel__mec_release_mem__send_interrupt_after_write_confirm                   =  2,
    int_sel__mec_release_mem__send_data_and_write_confirm                          =  3,
    int_sel__mec_release_mem__unconditionally_send_int_ctxid                       =  4,
    int_sel__mec_release_mem__conditionally_send_int_ctxid_based_on_32_bit_compare =  5,
    int_sel__mec_release_mem__conditionally_send_int_ctxid_based_on_64_bit_compare =  6,
};

// --------------------------------- MEC_RELEASE_MEM_data_sel_enum ---------------------------------
enum MEC_RELEASE_MEM_data_sel_enum
{
    data_sel__mec_release_mem__none                              =  0,
    data_sel__mec_release_mem__send_32_bit_low                   =  1,
    data_sel__mec_release_mem__send_64_bit_data                  =  2,
    data_sel__mec_release_mem__send_gpu_clock_counter            =  3,
    data_sel__mec_release_mem__send_system_clock_counter         =  4,
    data_sel__mec_release_mem__store_gds_data_to_memory__CORE    =  5,
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
    data_sel__mec_release_mem__send_emulated_sclk_counter__GFX11 =  6,
#endif
};

// ------------------------------ MEC_RELEASE_MEM_mes_action_id_enum ------------------------------
enum MEC_RELEASE_MEM_mes_action_id_enum
{
    mes_action_id__mec_release_mem__no_mes_notification__GFX10COREPLUS                     =  0,
    mes_action_id__mec_release_mem__interrupt_and_fence__GFX10COREPLUS                     =  1,
    mes_action_id__mec_release_mem__interrupt_no_fence_then_address_payload__GFX10COREPLUS =  2,
    mes_action_id__mec_release_mem__interrupt_and_address_payload__GFX10COREPLUS           =  3,
};

// -------------------------------------- PM4_MEC_RELEASE_MEM --------------------------------------
typedef struct PM4_MEC_RELEASE_MEM
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                          event_type   :  6;
                uint32_t                          reserved1    :  2;
                MEC_RELEASE_MEM_event_index_enum  event_index  :  4;
                uint32_t                          reserved2    : 13;
                MEC_RELEASE_MEM_cache_policy_enum cache_policy :  2;
                uint32_t                          reserved3    :  5;
            };
            struct
            {
                uint32_t                           reserved4           : 12;
                uint32_t                           tcl1_vol_action_ena :  1;
                uint32_t                           reserved5           :  2;
                uint32_t                           tc_wb_action_ena    :  1;
                uint32_t                           tcl1_action_ena     :  1;
                uint32_t                           tc_action_ena       :  1;
                uint32_t                           reserved6           :  1;
                uint32_t                           tc_nc_action_ena    :  1;
                uint32_t                           tc_wc_action_ena    :  1;
                uint32_t                           tc_md_action_ena    :  1;
                uint32_t                           reserved7           :  7;
                MEC_RELEASE_MEM_pq_exe_status_enum pq_exe_status       :  1;
                uint32_t                           reserved8           :  2;
            } gfx09;
            struct
            {
                uint32_t                           reserved9     : 12;
                uint32_t                           gcr_cntl      : 12;
                uint32_t                           reserved10    :  4;
                MEC_RELEASE_MEM_pq_exe_status_enum pq_exe_status :  1;
                uint32_t                           reserved11    :  3;
            } gfx10;
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
            struct
            {
                uint32_t reserved12    : 12;
                uint32_t gcr_cntl      : 13;
                uint32_t reserved13    :  3;
                uint32_t pq_exe_status :  1;
                uint32_t reserved14    :  1;
                uint32_t glk_inv       :  1;
                uint32_t pws_enable    :  1;
            } gfx11;
#endif
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        union
        {
            struct
            {
                uint32_t                      reserved1  : 16;
                MEC_RELEASE_MEM_dst_sel_enum  dst_sel    :  2;
                uint32_t                      reserved2  :  6;
                MEC_RELEASE_MEM_int_sel_enum  int_sel    :  3;
                uint32_t                      reserved3  :  2;
                MEC_RELEASE_MEM_data_sel_enum data_sel   :  3;
            };
            struct
            {
                uint32_t                           reserved4     : 20;
                uint32_t                           mes_intr_pipe :  2;
                MEC_RELEASE_MEM_mes_action_id_enum mes_action_id :  2;
                uint32_t                           reserved5     :  8;
            } gfx10CorePlus;
        } bitfields;
        uint32_t u32All;
    } ordinal3;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1      :  2;
                uint32_t address_lo_32b : 30;
            };
        } bitfieldsA;
        union
        {
            struct
            {
                uint32_t reserved2      :  3;
                uint32_t address_lo_64b : 29;
            };
        } bitfieldsB;
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t address_hi;
        uint32_t u32All;
    } ordinal5;

    union
    {
        uint32_t data_lo;
        uint32_t cmp_data_lo;
        union
        {
            struct
            {
                uint32_t dw_offset  : 16;
                uint32_t num_dwords : 16;
            } core;
        } bitfieldsC;
        uint32_t u32All;
    } ordinal6;

    union
    {
        uint32_t data_hi;
        uint32_t cmp_data_hi;
        uint32_t u32All;
    } ordinal7;

    union
    {
        uint32_t int_ctxid;
        uint32_t u32All;
    } ordinal8;
} PM4_MEC_RELEASE_MEM;

constexpr unsigned int PM4_MEC_RELEASE_MEM_SIZEDW__CORE = 8;

// ---------------------------------------- PM4_MEC_REWIND ----------------------------------------
typedef struct PM4_MEC_REWIND
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1      : 24;
                uint32_t offload_enable :  1;
                uint32_t reserved2      :  6;
                uint32_t valid          :  1;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;
} PM4_MEC_REWIND;

constexpr unsigned int PM4_MEC_REWIND_SIZEDW__CORE = 2;

// --------------------------------- MEC_SET_CONFIG_REG_index_enum ---------------------------------
enum MEC_SET_CONFIG_REG_index_enum
{
    index__mec_set_config_reg__default__CORE     =  0,
    index__mec_set_config_reg__insert_vmid__CORE =  1,
};

// ------------------------------------ PM4_MEC_SET_CONFIG_REG ------------------------------------
typedef struct PM4_MEC_SET_CONFIG_REG
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t reg_offset : 16;
                uint32_t reserved1  : 16;
            };
            struct
            {
                uint32_t                      reserved2  : 23;
                uint32_t                      vmid_shift :  5;
                MEC_SET_CONFIG_REG_index_enum index      :  4;
            } core;
        } bitfields;
        uint32_t u32All;
    } ordinal2;
} PM4_MEC_SET_CONFIG_REG;

constexpr unsigned int PM4_MEC_SET_CONFIG_REG_SIZEDW__CORE = 2;

// ------------------------------------- PM4_MEC_SET_QUEUE_REG -------------------------------------
typedef struct PM4_MEC_SET_QUEUE_REG
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t reg_offset :  8;
                uint32_t reserved1  :  7;
                uint32_t defer_exec :  1;
                uint32_t vqid       : 10;
                uint32_t reserved2  :  6;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;
} PM4_MEC_SET_QUEUE_REG;

constexpr unsigned int PM4_MEC_SET_QUEUE_REG_SIZEDW__CORE = 2;

// ----------------------------------- MEC_SET_SH_REG_index_enum -----------------------------------
enum MEC_SET_SH_REG_index_enum
{
    index__mec_set_sh_reg__default__CORE     =  0,
    index__mec_set_sh_reg__insert_vmid__CORE =  1,
};

// -------------------------------------- PM4_MEC_SET_SH_REG --------------------------------------
typedef struct PM4_MEC_SET_SH_REG
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t reg_offset : 16;
                uint32_t reserved1  : 16;
            };
            struct
            {
                uint32_t                  reserved2  : 23;
                uint32_t                  vmid_shift :  5;
                MEC_SET_SH_REG_index_enum index      :  4;
            } core;
        } bitfields;
        uint32_t u32All;
    } ordinal2;
} PM4_MEC_SET_SH_REG;

constexpr unsigned int PM4_MEC_SET_SH_REG_SIZEDW__CORE = 2;

// -------------------------------- MEC_SET_SH_REG_INDEX_index_enum --------------------------------
enum MEC_SET_SH_REG_INDEX_index_enum
{
    index__mec_set_sh_reg_index__default               =  0,
    index__mec_set_sh_reg_index__insert_vmid__CORE     =  1,
    index__mec_set_sh_reg_index__apply_kmd_cu_and_mask =  3,
};

// ----------------------------------- PM4_MEC_SET_SH_REG_INDEX -----------------------------------
typedef struct PM4_MEC_SET_SH_REG_INDEX
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                        reg_offset : 16;
                uint32_t                        reserved1  : 12;
                MEC_SET_SH_REG_INDEX_index_enum index      :  4;
            };
            struct
            {
                uint32_t reserved2  : 23;
                uint32_t vmid_shift :  5;
                uint32_t reserved3  :  4;
            } core;
        } bitfields;
        uint32_t u32All;
    } ordinal2;
} PM4_MEC_SET_SH_REG_INDEX;

constexpr unsigned int PM4_MEC_SET_SH_REG_INDEX_SIZEDW__CORE = 2;

// ------------------------------------ PM4_MEC_SET_UCONFIG_REG ------------------------------------
typedef struct PM4_MEC_SET_UCONFIG_REG
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t reg_offset : 16;
                uint32_t reserved1  : 16;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;
} PM4_MEC_SET_UCONFIG_REG;

constexpr unsigned int PM4_MEC_SET_UCONFIG_REG_SIZEDW__CORE = 2;

// ---------------------------------- PM4_MEC_WAIT_ON_CE_COUNTER ----------------------------------
typedef struct PM4_MEC_WAIT_ON_CE_COUNTER
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t cond_acquire_mem :  1;
                uint32_t force_sync       :  1;
                uint32_t reserved1        : 30;
            } core;
        } bitfields;
        uint32_t u32All;
    } ordinal2;
} PM4_MEC_WAIT_ON_CE_COUNTER;

constexpr unsigned int PM4_MEC_WAIT_ON_CE_COUNTER_SIZEDW__CORE = 2;

// -------------------------------- MEC_WAIT_REG_MEM_function_enum --------------------------------
enum MEC_WAIT_REG_MEM_function_enum
{
    function__mec_wait_reg_mem__always_pass                           =  0,
    function__mec_wait_reg_mem__less_than_ref_value                   =  1,
    function__mec_wait_reg_mem__less_than_equal_to_the_ref_value      =  2,
    function__mec_wait_reg_mem__equal_to_the_reference_value          =  3,
    function__mec_wait_reg_mem__not_equal_reference_value             =  4,
    function__mec_wait_reg_mem__greater_than_or_equal_reference_value =  5,
    function__mec_wait_reg_mem__greater_than_reference_value          =  6,
};

// -------------------------------- MEC_WAIT_REG_MEM_mem_space_enum --------------------------------
enum MEC_WAIT_REG_MEM_mem_space_enum
{
    mem_space__mec_wait_reg_mem__register_space =  0,
    mem_space__mec_wait_reg_mem__memory_space   =  1,
};

// -------------------------------- MEC_WAIT_REG_MEM_operation_enum --------------------------------
enum MEC_WAIT_REG_MEM_operation_enum
{
    operation__mec_wait_reg_mem__wait_reg_mem         =  0,
    operation__mec_wait_reg_mem__wr_wait_wr_reg       =  1,
    operation__mec_wait_reg_mem__wait_mem_preemptable =  3,
};

// ------------------------------ MEC_WAIT_REG_MEM_cache_policy_enum ------------------------------
enum MEC_WAIT_REG_MEM_cache_policy_enum
{
    cache_policy__mec_wait_reg_mem__lru__GFX10PLUS    =  0,
    cache_policy__mec_wait_reg_mem__stream__GFX10PLUS =  1,
    cache_policy__mec_wait_reg_mem__noa__GFX10PLUS    =  2,
    cache_policy__mec_wait_reg_mem__bypass__GFX10PLUS =  3,
};

// ------------------------------------- PM4_MEC_WAIT_REG_MEM -------------------------------------
typedef struct PM4_MEC_WAIT_REG_MEM
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                MEC_WAIT_REG_MEM_function_enum  function   :  3;
                uint32_t                        reserved1  :  1;
                MEC_WAIT_REG_MEM_mem_space_enum mem_space  :  2;
                MEC_WAIT_REG_MEM_operation_enum operation  :  2;
                uint32_t                        reserved2  : 24;
            };
            struct
            {
                uint32_t reserved3     : 22;
                uint32_t mes_intr_pipe :  2;
                uint32_t mes_action    :  1;
                uint32_t reserved4     :  7;
            } gfx10CorePlus;
            struct
            {
                uint32_t                           reserved5    : 25;
                MEC_WAIT_REG_MEM_cache_policy_enum cache_policy :  2;
                uint32_t                           reserved6    :  5;
            } gfx10Plus;
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1        :  2;
                uint32_t mem_poll_addr_lo : 30;
            };
        } bitfieldsA;
        union
        {
            struct
            {
                uint32_t reg_poll_addr : 18;
                uint32_t reserved2     : 14;
            };
        } bitfieldsB;
        union
        {
            struct
            {
                uint32_t reg_write_addr1 : 18;
                uint32_t reserved3       : 14;
            };
        } bitfieldsC;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t mem_poll_addr_hi;
        union
        {
            struct
            {
                uint32_t reg_write_addr2 : 18;
                uint32_t reserved1       : 14;
            };
        } bitfieldsB;
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t reference;
        uint32_t u32All;
    } ordinal5;

    union
    {
        uint32_t mask;
        uint32_t u32All;
    } ordinal6;

    union
    {
        union
        {
            struct
            {
                uint32_t poll_interval             : 16;
                uint32_t reserved1                 : 15;
                uint32_t optimize_ace_offload_mode :  1;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal7;
} PM4_MEC_WAIT_REG_MEM;

constexpr unsigned int PM4_MEC_WAIT_REG_MEM_SIZEDW__CORE = 7;

// ------------------------------- MEC_WAIT_REG_MEM64_function_enum -------------------------------
enum MEC_WAIT_REG_MEM64_function_enum
{
    function__mec_wait_reg_mem64__always_pass                           =  0,
    function__mec_wait_reg_mem64__less_than_ref_value                   =  1,
    function__mec_wait_reg_mem64__less_than_equal_to_the_ref_value      =  2,
    function__mec_wait_reg_mem64__equal_to_the_reference_value          =  3,
    function__mec_wait_reg_mem64__not_equal_reference_value             =  4,
    function__mec_wait_reg_mem64__greater_than_or_equal_reference_value =  5,
    function__mec_wait_reg_mem64__greater_than_reference_value          =  6,
};

// ------------------------------- MEC_WAIT_REG_MEM64_mem_space_enum -------------------------------
enum MEC_WAIT_REG_MEM64_mem_space_enum
{
    mem_space__mec_wait_reg_mem64__register_space =  0,
    mem_space__mec_wait_reg_mem64__memory_space   =  1,
};

// ------------------------------- MEC_WAIT_REG_MEM64_operation_enum -------------------------------
enum MEC_WAIT_REG_MEM64_operation_enum
{
    operation__mec_wait_reg_mem64__wait_reg_mem         =  0,
    operation__mec_wait_reg_mem64__wr_wait_wr_reg       =  1,
    operation__mec_wait_reg_mem64__wait_mem_preemptable =  3,
};

// ----------------------------- MEC_WAIT_REG_MEM64_cache_policy_enum -----------------------------
enum MEC_WAIT_REG_MEM64_cache_policy_enum
{
    cache_policy__mec_wait_reg_mem64__lru__GFX10PLUS    =  0,
    cache_policy__mec_wait_reg_mem64__stream__GFX10PLUS =  1,
    cache_policy__mec_wait_reg_mem64__noa__GFX10PLUS    =  2,
    cache_policy__mec_wait_reg_mem64__bypass__GFX10PLUS =  3,
};

// ------------------------------------ PM4_MEC_WAIT_REG_MEM64 ------------------------------------
typedef struct PM4_MEC_WAIT_REG_MEM64
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                MEC_WAIT_REG_MEM64_function_enum  function   :  3;
                uint32_t                          reserved1  :  1;
                MEC_WAIT_REG_MEM64_mem_space_enum mem_space  :  2;
                MEC_WAIT_REG_MEM64_operation_enum operation  :  2;
                uint32_t                          reserved2  : 24;
            };
            struct
            {
                uint32_t reserved3     : 22;
                uint32_t mes_intr_pipe :  2;
                uint32_t mes_action    :  1;
                uint32_t reserved4     :  7;
            } gfx10CorePlus;
            struct
            {
                uint32_t                             reserved5    : 25;
                MEC_WAIT_REG_MEM64_cache_policy_enum cache_policy :  2;
                uint32_t                             reserved6    :  5;
            } gfx10Plus;
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1        :  3;
                uint32_t mem_poll_addr_lo : 29;
            };
        } bitfieldsA;
        union
        {
            struct
            {
                uint32_t reg_poll_addr : 18;
                uint32_t reserved2     : 14;
            };
        } bitfieldsB;
        union
        {
            struct
            {
                uint32_t reg_write_addr1 : 18;
                uint32_t reserved3       : 14;
            };
        } bitfieldsC;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t mem_poll_addr_hi;
        union
        {
            struct
            {
                uint32_t reg_write_addr2 : 18;
                uint32_t reserved1       : 14;
            };
        } bitfieldsB;
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t reference;
        uint32_t u32All;
    } ordinal5;

    union
    {
        uint32_t reference_hi;
        uint32_t u32All;
    } ordinal6;

    union
    {
        uint32_t mask;
        uint32_t u32All;
    } ordinal7;

    union
    {
        uint32_t mask_hi;
        uint32_t u32All;
    } ordinal8;

    union
    {
        union
        {
            struct
            {
                uint32_t poll_interval             : 16;
                uint32_t reserved1                 : 15;
                uint32_t optimize_ace_offload_mode :  1;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal9;
} PM4_MEC_WAIT_REG_MEM64;

constexpr unsigned int PM4_MEC_WAIT_REG_MEM64_SIZEDW__CORE = 9;

// ---------------------------------- MEC_WRITE_DATA_dst_sel_enum ----------------------------------
enum MEC_WRITE_DATA_dst_sel_enum
{
    dst_sel__mec_write_data__mem_mapped_register                =  0,
    dst_sel__mec_write_data__tc_l2                              =  2,
    dst_sel__mec_write_data__gds__CORE                          =  3,
    dst_sel__mec_write_data__memory                             =  5,
    dst_sel__mec_write_data__memory_mapped_adc_persistent_state =  6,
};

// --------------------------------- MEC_WRITE_DATA_addr_incr_enum ---------------------------------
enum MEC_WRITE_DATA_addr_incr_enum
{
    addr_incr__mec_write_data__increment_address        =  0,
    addr_incr__mec_write_data__do_not_increment_address =  1,
};

// -------------------------------- MEC_WRITE_DATA_wr_confirm_enum --------------------------------
enum MEC_WRITE_DATA_wr_confirm_enum
{
    wr_confirm__mec_write_data__do_not_wait_for_write_confirmation =  0,
    wr_confirm__mec_write_data__wait_for_write_confirmation        =  1,
};

// ------------------------------- MEC_WRITE_DATA_cache_policy_enum -------------------------------
enum MEC_WRITE_DATA_cache_policy_enum
{
    cache_policy__mec_write_data__lru               =  0,
    cache_policy__mec_write_data__stream            =  1,
    cache_policy__mec_write_data__noa__GFX10PLUS    =  2,
    cache_policy__mec_write_data__bypass__GFX10PLUS =  3,
};

// -------------------------------------- PM4_MEC_WRITE_DATA --------------------------------------
typedef struct PM4_MEC_WRITE_DATA
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                         reserved1    :  8;
                MEC_WRITE_DATA_dst_sel_enum      dst_sel      :  4;
                uint32_t                         reserved2    :  4;
                MEC_WRITE_DATA_addr_incr_enum    addr_incr    :  1;
                uint32_t                         reserved3    :  3;
                MEC_WRITE_DATA_wr_confirm_enum   wr_confirm   :  1;
                uint32_t                         reserved4    :  4;
                MEC_WRITE_DATA_cache_policy_enum cache_policy :  2;
                uint32_t                         reserved5    :  5;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        union
        {
            struct
            {
                uint32_t dst_mmreg_addr : 18;
                uint32_t reserved1      : 14;
            };
        } bitfieldsA;
        union
        {
            struct
            {
                uint32_t dst_gds_addr : 16;
                uint32_t reserved2    : 16;
            } core;
        } bitfieldsB;
        union
        {
            struct
            {
                uint32_t reserved4       :  2;
                uint32_t dst_mem_addr_lo : 30;
            } core;
        } bitfieldsC;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t dst_mem_addr_hi;
        uint32_t u32All;
    } ordinal4;
} PM4_MEC_WRITE_DATA;

constexpr unsigned int PM4_MEC_WRITE_DATA_SIZEDW__CORE = 4;

// ------------------------------- PM4_MEC_DISPATCH_TASK_STATE_INIT -------------------------------
typedef struct PM4_MEC_DISPATCH_TASK_STATE_INIT
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1           :  8;
                uint32_t control_buf_addr_lo : 24;
            } gfx10CorePlus;
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t control_buf_addr_hi;
        uint32_t u32All;
    } ordinal3;
} PM4_MEC_DISPATCH_TASK_STATE_INIT;

constexpr unsigned int PM4_MEC_DISPATCH_TASK_STATE_INIT_SIZEDW__GFX10COREPLUS = 3;

// ----------------------------- PM4_MEC_DISPATCH_TASKMESH_DIRECT_ACE -----------------------------
typedef struct PM4_MEC_DISPATCH_TASKMESH_DIRECT_ACE
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        uint32_t x_dim;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t y_dim;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t z_dim;
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t dispatch_initiator;
        uint32_t u32All;
    } ordinal5;

    union
    {
        union
        {
            struct
            {
                uint32_t ring_entry_loc : 16;
                uint32_t reserved1      : 16;
            } gfx10CorePlus;
        } bitfields;
        uint32_t u32All;
    } ordinal6;
} PM4_MEC_DISPATCH_TASKMESH_DIRECT_ACE;

constexpr unsigned int PM4_MEC_DISPATCH_TASKMESH_DIRECT_ACE_SIZEDW__GFX10COREPLUS = 6;

// ------------------------- PM4_MEC_DISPATCH_TASKMESH_INDIRECT_MULTI_ACE -------------------------
typedef struct PM4_MEC_DISPATCH_TASKMESH_INDIRECT_MULTI_ACE
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1    :  2;
                uint32_t data_addr_lo : 30;
            } gfx10CorePlus;
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t data_addr_hi;
        uint32_t u32All;
    } ordinal3;

    union
    {
        union
        {
            struct
            {
                uint32_t ring_entry_loc : 16;
                uint32_t reserved1      : 16;
            } gfx10CorePlus;
        } bitfields;
        uint32_t u32All;
    } ordinal4;

    union
    {
        union
        {
            struct
            {
                uint32_t thread_trace_marker_enable :  1;
                uint32_t count_indirect_enable      :  1;
                uint32_t draw_index_enable          :  1;
                uint32_t compute_xyz_dim_enable     :  1;
                uint32_t reserved1                  : 12;
                uint32_t dispatch_index_loc         : 16;
            } gfx10CorePlus;
        } bitfields;
        uint32_t u32All;
    } ordinal5;

    union
    {
        union
        {
            struct
            {
                uint32_t compute_xyz_dim_loc : 16;
                uint32_t reserved1           : 16;
            } gfx10CorePlus;
        } bitfields;
        uint32_t u32All;
    } ordinal6;

    union
    {
        uint32_t count;
        uint32_t u32All;
    } ordinal7;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1     :  2;
                uint32_t count_addr_lo : 30;
            } gfx10CorePlus;
        } bitfields;
        uint32_t u32All;
    } ordinal8;

    union
    {
        uint32_t count_addr_hi;
        uint32_t u32All;
    } ordinal9;

    union
    {
        uint32_t stride;
        uint32_t u32All;
    } ordinal10;

    union
    {
        uint32_t dispatch_initiator;
        uint32_t u32All;
    } ordinal11;
} PM4_MEC_DISPATCH_TASKMESH_INDIRECT_MULTI_ACE;

constexpr unsigned int PM4_MEC_DISPATCH_TASKMESH_INDIRECT_MULTI_ACE_SIZEDW__GFX10COREPLUS = 11;

#if  CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1 || CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
// ------------------------------- MEC_LOAD_SH_REG_INDEX_index_enum -------------------------------
enum MEC_LOAD_SH_REG_INDEX_index_enum
{
    index__mec_load_sh_reg_index__direct_addr__GFX103COREPLUS   =  0,
    index__mec_load_sh_reg_index__indirect_addr__GFX103COREPLUS =  2,
};
#endif

#if  CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1 || CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
// ---------------------------- MEC_LOAD_SH_REG_INDEX_data_format_enum ----------------------------
enum MEC_LOAD_SH_REG_INDEX_data_format_enum
{
    data_format__mec_load_sh_reg_index__offset_and_size__GFX103COREPLUS =  0,
    data_format__mec_load_sh_reg_index__offset_and_data__GFX103COREPLUS =  1,
};
#endif

#if  CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1 || CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
// ----------------------------------- PM4_MEC_LOAD_SH_REG_INDEX -----------------------------------
typedef struct PM4_MEC_LOAD_SH_REG_INDEX
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                MEC_LOAD_SH_REG_INDEX_index_enum index       :  2;
                uint32_t                         mem_addr_lo : 30;
            } gfx103CorePlus;
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t mem_addr_hi;
        uint32_t u32All;
    } ordinal3;

    union
    {
        union
        {
            struct
            {
                uint32_t                               reg_offset  : 16;
                uint32_t                               reserved1   : 15;
                MEC_LOAD_SH_REG_INDEX_data_format_enum data_format :  1;
            } gfx103CorePlus;
        } bitfields;
        uint32_t u32All;
    } ordinal4;

    union
    {
        union
        {
            struct
            {
                uint32_t num_dwords : 14;
                uint32_t reserved1  : 18;
            } gfx103CorePlus;
        } bitfields;
        uint32_t u32All;
    } ordinal5;
} PM4_MEC_LOAD_SH_REG_INDEX;

constexpr unsigned int PM4_MEC_LOAD_SH_REG_INDEX_SIZEDW__GFX103COREPLUS = 5;
#endif

#if  CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1 || CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
// -------------------------------- MEC_PERFMON_CONTROL_pmc_en_enum --------------------------------
enum MEC_PERFMON_CONTROL_pmc_en_enum
{
    pmc_en__mec_perfmon_control__perfmon_disable__GFX103COREPLUS =  0,
    pmc_en__mec_perfmon_control__perfmon_enable__GFX103COREPLUS  =  1,
};
#endif

#if  CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1 || CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
// ------------------------------------ PM4_MEC_PERFMON_CONTROL ------------------------------------
typedef struct PM4_MEC_PERFMON_CONTROL
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                        pmc_id        :  3;
                uint32_t                        reserved1     : 12;
                MEC_PERFMON_CONTROL_pmc_en_enum pmc_en        :  1;
                uint32_t                        pmc_unit_mask :  8;
                uint32_t                        reserved2     :  8;
            } gfx103CorePlus;
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        union
        {
            struct
            {
                uint32_t pmc_event  : 14;
                uint32_t reserved1  : 18;
            } gfx103CorePlus;
        } bitfields;
        uint32_t u32All;
    } ordinal3;
} PM4_MEC_PERFMON_CONTROL;

constexpr unsigned int PM4_MEC_PERFMON_CONTROL_SIZEDW__GFX103COREPLUS = 3;
#endif

#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
// --------------------------------- MEC_TIMESTAMP_clock_sel_enum ---------------------------------
enum MEC_TIMESTAMP_clock_sel_enum
{
    clock_sel__mec_timestamp__gfx_ip_clock__GFX11 =  0,
    clock_sel__mec_timestamp__soc_clock__GFX11    =  1,
};
#endif

#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
// --------------------------------------- PM4_MEC_TIMESTAMP ---------------------------------------
typedef struct PM4_MEC_TIMESTAMP
{
    union
    {
        PM4_MEC_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                     enable_bottom :  1;
                uint32_t                     enable_top    :  1;
                MEC_TIMESTAMP_clock_sel_enum clock_sel     :  1;
                uint32_t                     reserved1     : 29;
            } gfx11;
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1        :  3;
                uint32_t pipe_bot_addr_lo : 29;
            } gfx11;
        } bitfields;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t pipe_bot_addr_hi;
        uint32_t u32All;
    } ordinal4;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1        :  3;
                uint32_t pipe_top_addr_lo : 29;
            } gfx11;
        } bitfields;
        uint32_t u32All;
    } ordinal5;

    union
    {
        uint32_t pipe_top_addr_hi;
        uint32_t u32All;
    } ordinal6;
} PM4_MEC_TIMESTAMP;

constexpr unsigned int PM4_MEC_TIMESTAMP_SIZEDW__GFX11 = 6;
#endif

} // inline namespace Chip
} // namespace Gfx9
} // namespace Pal
