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
// Make sure the necessary endian defines are there.
//
#ifndef LITTLEENDIAN_CPU
#error "LITTLEENDIAN_CPU must be defined"
#endif

#include <stdint.h>

// ------------------------------------- PM4_CE_TYPE_3_HEADER -------------------------------------
typedef union PM4_CE_TYPE_3_HEADER
{
    struct
    {
        uint32_t reserved1 :  8;
        uint32_t opcode    :  8;
        uint32_t count     : 14;
        uint32_t type      :  2;
    };
    uint32_t u32All;
} PM4_CE_TYPE_3_HEADER;

// --------------------------------------- PM4_CE_COND_EXEC ---------------------------------------
typedef struct PM4_CE_COND_EXEC
{
    union
    {
        PM4_CE_TYPE_3_HEADER header;
        uint32_t ordinal1;
    };

    union
    {
        struct
        {
            uint32_t reserved1 :  2;
            uint32_t addr_lo   : 30;
        } bitfields2;
        uint32_t ordinal2;
    };

    uint32_t addr_hi;

    uint32_t reserved2;

    union
    {
        struct
        {
            uint32_t exec_count : 14;
            uint32_t reserved3  : 18;
        } bitfields5;
        uint32_t ordinal5;
    };

} PM4_CE_COND_EXEC;

// ---------------------------- CE_COND_INDIRECT_BUFFER_CONST_mode_enum ----------------------------
enum CE_COND_INDIRECT_BUFFER_CONST_mode_enum
{
    mode__ce_cond_indirect_buffer_const__if_then__HASCE      =  1,
    mode__ce_cond_indirect_buffer_const__if_then_else__HASCE =  2,
};

// -------------------------- CE_COND_INDIRECT_BUFFER_CONST_function_enum --------------------------
enum CE_COND_INDIRECT_BUFFER_CONST_function_enum
{
    function__ce_cond_indirect_buffer_const__always_pass__HASCE                           =  0,
    function__ce_cond_indirect_buffer_const__less_than_ref_value__HASCE                   =  1,
    function__ce_cond_indirect_buffer_const__less_than_equal_to_the_ref_value__HASCE      =  2,
    function__ce_cond_indirect_buffer_const__equal_to_the_reference_value__HASCE          =  3,
    function__ce_cond_indirect_buffer_const__not_equal_reference_value__HASCE             =  4,
    function__ce_cond_indirect_buffer_const__greater_than_or_equal_reference_value__HASCE =  5,
    function__ce_cond_indirect_buffer_const__greater_than_reference_value__HASCE          =  6,
};

// ----------------------- CE_COND_INDIRECT_BUFFER_CONST_cache_policy1_enum -----------------------
enum CE_COND_INDIRECT_BUFFER_CONST_cache_policy1_enum
{
    cache_policy1__ce_cond_indirect_buffer_const__lru__HASCE        =  0,
    cache_policy1__ce_cond_indirect_buffer_const__stream__HASCE     =  1,
    cache_policy1__ce_cond_indirect_buffer_const__noa__GFX10CORE    =  2,
    cache_policy1__ce_cond_indirect_buffer_const__bypass__GFX10CORE =  3,
};

// ----------------------- CE_COND_INDIRECT_BUFFER_CONST_cache_policy2_enum -----------------------
enum CE_COND_INDIRECT_BUFFER_CONST_cache_policy2_enum
{
    cache_policy2__ce_cond_indirect_buffer_const__lru__HASCE        =  0,
    cache_policy2__ce_cond_indirect_buffer_const__stream__HASCE     =  1,
    cache_policy2__ce_cond_indirect_buffer_const__noa__GFX10CORE    =  2,
    cache_policy2__ce_cond_indirect_buffer_const__bypass__GFX10CORE =  3,
};

// ------------------------------- PM4_CE_COND_INDIRECT_BUFFER_CONST -------------------------------
typedef struct PM4_CE_COND_INDIRECT_BUFFER_CONST
{
    union
    {
        PM4_CE_TYPE_3_HEADER header;
        uint32_t ordinal1;
    };

    union
    {
        struct
        {
            CE_COND_INDIRECT_BUFFER_CONST_mode_enum     mode      :  2;
            uint32_t                                    reserved1 :  6;
            CE_COND_INDIRECT_BUFFER_CONST_function_enum function  :  3;
            uint32_t                                    reserved2 : 21;
        } bitfields2;
        uint32_t ordinal2;
    };

    union
    {
        struct
        {
            uint32_t reserved3       :  3;
            uint32_t compare_addr_lo : 29;
        } bitfields3;
        uint32_t ordinal3;
    };

    uint32_t compare_addr_hi;

    uint32_t mask_lo;

    uint32_t mask_hi;

    uint32_t reference_lo;

    uint32_t reference_hi;

    union
    {
        struct
        {
            uint32_t reserved4   :  2;
            uint32_t ib_base1_lo : 30;
        } bitfields9;
        uint32_t ordinal9;
    };

    uint32_t ib_base1_hi;

    union
    {
        struct
        {
            uint32_t                                         ib_size1      : 20;
            uint32_t                                         reserved5     :  8;
            CE_COND_INDIRECT_BUFFER_CONST_cache_policy1_enum cache_policy1 :  2;
            uint32_t                                         reserved6     :  2;
        } bitfields11;
        uint32_t ordinal11;
    };

    union
    {
        struct
        {
            uint32_t reserved7   :  2;
            uint32_t ib_base2_lo : 30;
        } bitfields12;
        uint32_t ordinal12;
    };

    uint32_t ib_base2_hi;

    union
    {
        struct
        {
            uint32_t                                         ib_size2      : 20;
            uint32_t                                         reserved8     :  8;
            CE_COND_INDIRECT_BUFFER_CONST_cache_policy2_enum cache_policy2 :  2;
            uint32_t                                         reserved9     :  2;
        } bitfields14;
        uint32_t ordinal14;
    };

} PM4_CE_COND_INDIRECT_BUFFER_CONST;

// ------------------------------------ PM4_CE_CONTEXT_CONTROL ------------------------------------
typedef struct PM4_CE_CONTEXT_CONTROL
{
    union
    {
        PM4_CE_TYPE_3_HEADER header;
        uint32_t ordinal1;
    };

    union
    {
        struct
        {
            uint32_t reserved1   : 28;
            uint32_t load_ce_ram :  1;
            uint32_t reserved2   :  2;
            uint32_t load_enable :  1;
        } bitfields2;
        uint32_t ordinal2;
    };

    uint32_t reserved3;

} PM4_CE_CONTEXT_CONTROL;

// ----------------------------------- CE_COPY_DATA_src_sel_enum -----------------------------------
enum CE_COPY_DATA_src_sel_enum
{
    src_sel__ce_copy_data__mem_mapped_register__HASCE =  0,
    src_sel__ce_copy_data__memory__GFX09              =  1,
    src_sel__ce_copy_data__tc_l2_obsolete__GFX10CORE  =  1,
    src_sel__ce_copy_data__tc_l2__HASCE               =  2,
    src_sel__ce_copy_data__immediate_data__HASCE      =  5,
};

// ----------------------------------- CE_COPY_DATA_dst_sel_enum -----------------------------------
enum CE_COPY_DATA_dst_sel_enum
{
    dst_sel__ce_copy_data__mem_mapped_register__HASCE =  0,
    dst_sel__ce_copy_data__tc_l2__HASCE               =  2,
    dst_sel__ce_copy_data__memory__GFX09              =  5,
    dst_sel__ce_copy_data__tc_l2_obsolete__GFX10CORE  =  5,
};

// ------------------------------ CE_COPY_DATA_src_cache_policy_enum ------------------------------
enum CE_COPY_DATA_src_cache_policy_enum
{
    src_cache_policy__ce_copy_data__lru__HASCE        =  0,
    src_cache_policy__ce_copy_data__stream__HASCE     =  1,
    src_cache_policy__ce_copy_data__noa__GFX10CORE    =  2,
    src_cache_policy__ce_copy_data__bypass__GFX10CORE =  3,
};

// ---------------------------------- CE_COPY_DATA_count_sel_enum ----------------------------------
enum CE_COPY_DATA_count_sel_enum
{
    count_sel__ce_copy_data__32_bits_of_data__HASCE =  0,
    count_sel__ce_copy_data__64_bits_of_data__HASCE =  1,
};

// --------------------------------- CE_COPY_DATA_wr_confirm_enum ---------------------------------
enum CE_COPY_DATA_wr_confirm_enum
{
    wr_confirm__ce_copy_data__do_not_wait_for_confirmation__HASCE =  0,
    wr_confirm__ce_copy_data__wait_for_confirmation__HASCE        =  1,
};

// ------------------------------ CE_COPY_DATA_dst_cache_policy_enum ------------------------------
enum CE_COPY_DATA_dst_cache_policy_enum
{
    dst_cache_policy__ce_copy_data__lru__HASCE        =  0,
    dst_cache_policy__ce_copy_data__stream__HASCE     =  1,
    dst_cache_policy__ce_copy_data__noa__GFX10CORE    =  2,
    dst_cache_policy__ce_copy_data__bypass__GFX10CORE =  3,
};

// --------------------------------- CE_COPY_DATA_engine_sel_enum ---------------------------------
enum CE_COPY_DATA_engine_sel_enum
{
    engine_sel__ce_copy_data__constant_engine__HASCE =  2,
};

// --------------------------------------- PM4_CE_COPY_DATA ---------------------------------------
typedef struct PM4_CE_COPY_DATA
{
    union
    {
        PM4_CE_TYPE_3_HEADER header;
        uint32_t ordinal1;
    };

    union
    {
        struct
        {
            CE_COPY_DATA_src_sel_enum          src_sel          :  4;
            uint32_t                           reserved1        :  4;
            CE_COPY_DATA_dst_sel_enum          dst_sel          :  4;
            uint32_t                           reserved2        :  1;
            CE_COPY_DATA_src_cache_policy_enum src_cache_policy :  2;
            uint32_t                           reserved3        :  1;
            CE_COPY_DATA_count_sel_enum        count_sel        :  1;
            uint32_t                           reserved4        :  3;
            CE_COPY_DATA_wr_confirm_enum       wr_confirm       :  1;
            uint32_t                           reserved5        :  4;
            CE_COPY_DATA_dst_cache_policy_enum dst_cache_policy :  2;
            uint32_t                           reserved6        :  3;
            CE_COPY_DATA_engine_sel_enum       engine_sel       :  2;
        } bitfields2;
        uint32_t ordinal2;
    };

    union
    {
        struct
        {
            uint32_t src_reg_offset : 18;
            uint32_t reserved7      : 14;
        } bitfields3a;
        struct
        {
            uint32_t reserved8       :  2;
            uint32_t src_32b_addr_lo : 30;
        } bitfields3b;
        struct
        {
            uint32_t reserved9       :  3;
            uint32_t src_64b_addr_lo : 29;
        } bitfields3c;
        uint32_t imm_data;
        uint32_t ordinal3;
    };

    union
    {
        uint32_t src_memtc_addr_hi;
        uint32_t src_imm_data;
        uint32_t ordinal4;
    };

    union
    {
        struct
        {
            uint32_t dst_reg_offset : 18;
            uint32_t reserved10     : 14;
        } bitfields5a;
        struct
        {
            uint32_t reserved11      :  2;
            uint32_t dst_32b_addr_lo : 30;
        } bitfields5b;
        struct
        {
            uint32_t reserved12      :  3;
            uint32_t dst_64b_addr_lo : 29;
        } bitfields5c;
        uint32_t ordinal5;
    };

    uint32_t dst_addr_hi;

} PM4_CE_COPY_DATA;

// ------------------------------ CE_DUMP_CONST_RAM_cache_policy_enum ------------------------------
enum CE_DUMP_CONST_RAM_cache_policy_enum
{
    cache_policy__ce_dump_const_ram__lru__HASCE        =  0,
    cache_policy__ce_dump_const_ram__stream__HASCE     =  1,
    cache_policy__ce_dump_const_ram__noa__GFX10CORE    =  2,
    cache_policy__ce_dump_const_ram__bypass__GFX10CORE =  3,
};

// ------------------------------------- PM4_CE_DUMP_CONST_RAM -------------------------------------
typedef struct PM4_CE_DUMP_CONST_RAM
{
    union
    {
        PM4_CE_TYPE_3_HEADER header;
        uint32_t ordinal1;
    };

    union
    {
        struct
        {
            uint32_t                            offset       : 16;
            uint32_t                            reserved1    :  9;
            CE_DUMP_CONST_RAM_cache_policy_enum cache_policy :  2;
            uint32_t                            reserved2    :  3;
            uint32_t                            increment_ce :  1;
            uint32_t                            increment_cs :  1;
        } bitfields2;
        uint32_t ordinal2;
    };

    union
    {
        struct
        {
            uint32_t num_dw    : 15;
            uint32_t reserved3 : 17;
        } bitfields3;
        uint32_t ordinal3;
    };

    uint32_t addr_lo;

    uint32_t addr_hi;

} PM4_CE_DUMP_CONST_RAM;

// -------------------------- CE_DUMP_CONST_RAM_OFFSET_cache_policy_enum --------------------------
enum CE_DUMP_CONST_RAM_OFFSET_cache_policy_enum
{
    cache_policy__ce_dump_const_ram_offset__lru__HASCE        =  0,
    cache_policy__ce_dump_const_ram_offset__stream__HASCE     =  1,
    cache_policy__ce_dump_const_ram_offset__noa__GFX10CORE    =  2,
    cache_policy__ce_dump_const_ram_offset__bypass__GFX10CORE =  3,
};

// --------------------------------- PM4_CE_DUMP_CONST_RAM_OFFSET ---------------------------------
typedef struct PM4_CE_DUMP_CONST_RAM_OFFSET
{
    union
    {
        PM4_CE_TYPE_3_HEADER header;
        uint32_t ordinal1;
    };

    union
    {
        struct
        {
            uint32_t                                   offset       : 16;
            uint32_t                                   reserved1    :  9;
            CE_DUMP_CONST_RAM_OFFSET_cache_policy_enum cache_policy :  2;
            uint32_t                                   reserved2    :  3;
            uint32_t                                   increment_ce :  1;
            uint32_t                                   increment_cs :  1;
        } bitfields2;
        uint32_t ordinal2;
    };

    union
    {
        struct
        {
            uint32_t num_dw    : 15;
            uint32_t reserved3 : 17;
        } bitfields3;
        uint32_t ordinal3;
    };

    uint32_t addr_offset;

} PM4_CE_DUMP_CONST_RAM_OFFSET;

// ----------------------------------- CE_FRAME_CONTROL_tmz_enum -----------------------------------
enum CE_FRAME_CONTROL_tmz_enum
{
    tmz__ce_frame_control__tmz_off__HASCE =  0,
    tmz__ce_frame_control__tmz_on__HASCE  =  1,
};

// --------------------------------- CE_FRAME_CONTROL_command_enum ---------------------------------
enum CE_FRAME_CONTROL_command_enum
{
    command__ce_frame_control__kmd_frame_begin__HASCE =  0,
    command__ce_frame_control__kmd_frame_end__HASCE   =  1,
};

// ------------------------------------- PM4_CE_FRAME_CONTROL -------------------------------------
typedef struct PM4_CE_FRAME_CONTROL
{
    union
    {
        PM4_CE_TYPE_3_HEADER header;
        uint32_t ordinal1;
    };

    union
    {
        struct
        {
            CE_FRAME_CONTROL_tmz_enum     tmz       :  1;
            uint32_t                      reserved1 : 27;
            CE_FRAME_CONTROL_command_enum command   :  4;
        } bitfields2;
        uint32_t ordinal2;
    };

} PM4_CE_FRAME_CONTROL;

// ----------------------------- CE_INCREMENT_CE_COUNTER_cntrsel_enum -----------------------------
enum CE_INCREMENT_CE_COUNTER_cntrsel_enum
{
    cntrsel__ce_increment_ce_counter__invalid__HASCE                      =  0,
    cntrsel__ce_increment_ce_counter__increment_ce_counter__HASCE         =  1,
    cntrsel__ce_increment_ce_counter__increment_cs_counter__HASCE         =  2,
    cntrsel__ce_increment_ce_counter__increment_ce_and_cs_counters__HASCE =  3,
};

// ---------------------------------- PM4_CE_INCREMENT_CE_COUNTER ----------------------------------
typedef struct PM4_CE_INCREMENT_CE_COUNTER
{
    union
    {
        PM4_CE_TYPE_3_HEADER header;
        uint32_t ordinal1;
    };

    union
    {
        struct
        {
            CE_INCREMENT_CE_COUNTER_cntrsel_enum cntrsel   :  2;
            uint32_t                             reserved1 : 30;
        } bitfields2;
        uint32_t ordinal2;
    };

} PM4_CE_INCREMENT_CE_COUNTER;

// -------------------------- CE_INDIRECT_BUFFER_CONST_cache_policy_enum --------------------------
enum CE_INDIRECT_BUFFER_CONST_cache_policy_enum
{
    cache_policy__ce_indirect_buffer_const__lru__GFX10CORE    =  0,
    cache_policy__ce_indirect_buffer_const__stream__GFX10CORE =  1,
    cache_policy__ce_indirect_buffer_const__noa__GFX10CORE    =  2,
    cache_policy__ce_indirect_buffer_const__bypass__GFX10CORE =  3,
};

// --------------------------------- PM4_CE_INDIRECT_BUFFER_CONST ---------------------------------
typedef struct PM4_CE_INDIRECT_BUFFER_CONST
{
    union
    {
        PM4_CE_TYPE_3_HEADER header;
        uint32_t ordinal1;
    };

    union
    {
        struct
        {
            uint32_t reserved1  :  2;
            uint32_t ib_base_lo : 30;
        } bitfields2;
        uint32_t ordinal2;
    };

    uint32_t ib_base_hi;

    union
    {
        struct
        {
            uint32_t ib_size      : 20;
            uint32_t chain        :  1;
            uint32_t pre_ena      :  1;
            uint32_t reserved2    :  2;
            uint32_t vmid         :  4;
            uint32_t cache_policy :  2;
            uint32_t pre_resume   :  1;
            uint32_t              :  1;
        } bitfields4;
        uint32_t ordinal4;
    };

} PM4_CE_INDIRECT_BUFFER_CONST;

// ------------------------------ CE_LOAD_CONST_RAM_cache_policy_enum ------------------------------
enum CE_LOAD_CONST_RAM_cache_policy_enum
{
    cache_policy__ce_load_const_ram__lru__HASCE        =  0,
    cache_policy__ce_load_const_ram__stream__HASCE     =  1,
    cache_policy__ce_load_const_ram__noa__GFX10CORE    =  2,
    cache_policy__ce_load_const_ram__bypass__GFX10CORE =  3,
};

// ------------------------------------- PM4_CE_LOAD_CONST_RAM -------------------------------------
typedef struct PM4_CE_LOAD_CONST_RAM
{
    union
    {
        PM4_CE_TYPE_3_HEADER header;
        uint32_t ordinal1;
    };

    uint32_t addr_lo;

    uint32_t addr_hi;

    union
    {
        struct
        {
            uint32_t num_dw    : 15;
            uint32_t reserved1 : 17;
        } bitfields4;
        uint32_t ordinal4;
    };

    union
    {
        struct
        {
            uint32_t                            start_addr   : 16;
            uint32_t                            reserved2    :  9;
            CE_LOAD_CONST_RAM_cache_policy_enum cache_policy :  2;
            uint32_t                            reserved3    :  5;
        } bitfields5;
        uint32_t ordinal5;
    };

} PM4_CE_LOAD_CONST_RAM;

// ------------------------------------------ PM4_CE_NOP ------------------------------------------
typedef struct PM4_CE_NOP
{
    union
    {
        PM4_CE_TYPE_3_HEADER header;
        uint32_t ordinal1;
    };

} PM4_CE_NOP;

// -------------------------------- CE_PRIME_UTCL2_cache_perm_enum --------------------------------
enum CE_PRIME_UTCL2_cache_perm_enum
{
    cache_perm__ce_prime_utcl2__read__HASCE    =  0,
    cache_perm__ce_prime_utcl2__write__HASCE   =  1,
    cache_perm__ce_prime_utcl2__execute__HASCE =  2,
};

// -------------------------------- CE_PRIME_UTCL2_prime_mode_enum --------------------------------
enum CE_PRIME_UTCL2_prime_mode_enum
{
    prime_mode__ce_prime_utcl2__dont_wait_for_xack__HASCE =  0,
    prime_mode__ce_prime_utcl2__wait_for_xack__HASCE      =  1,
};

// -------------------------------- CE_PRIME_UTCL2_engine_sel_enum --------------------------------
enum CE_PRIME_UTCL2_engine_sel_enum
{
    engine_sel__ce_prime_utcl2__constant_engine__HASCE =  2,
};

// -------------------------------------- PM4_CE_PRIME_UTCL2 --------------------------------------
typedef struct PM4_CE_PRIME_UTCL2
{
    union
    {
        PM4_CE_TYPE_3_HEADER header;
        uint32_t ordinal1;
    };

    union
    {
        struct
        {
            CE_PRIME_UTCL2_cache_perm_enum cache_perm :  3;
            CE_PRIME_UTCL2_prime_mode_enum prime_mode :  1;
            uint32_t                       reserved1  : 26;
            CE_PRIME_UTCL2_engine_sel_enum engine_sel :  2;
        } bitfields2;
        uint32_t ordinal2;
    };

    uint32_t addr_lo;

    uint32_t addr_hi;

    union
    {
        struct
        {
            uint32_t requested_pages : 14;
            uint32_t reserved2       : 18;
        } bitfields5;
        uint32_t ordinal5;
    };

} PM4_CE_PRIME_UTCL2;

// ---------------------------------- CE_SET_BASE_base_index_enum ----------------------------------
enum CE_SET_BASE_base_index_enum
{
    base_index__ce_set_base__ce_dst_base_addr__HASCE   =  2,
    base_index__ce_set_base__ce_partition_bases__HASCE =  3,
};

// ---------------------------------------- PM4_CE_SET_BASE ----------------------------------------
typedef struct PM4_CE_SET_BASE
{
    union
    {
        PM4_CE_TYPE_3_HEADER header;
        uint32_t ordinal1;
    };

    union
    {
        struct
        {
            CE_SET_BASE_base_index_enum base_index :  4;
            uint32_t                    reserved1  : 28;
        } bitfields2;
        uint32_t ordinal2;
    };

    union
    {
        struct
        {
            uint32_t reserved2  :  3;
            uint32_t address_lo : 29;
        } bitfields3a;
        struct
        {
            uint32_t cs1_index : 16;
            uint32_t reserved3 : 16;
        } bitfields3b;
        uint32_t ordinal3;
    };

    union
    {
        uint32_t address_hi;
        struct
        {
            uint32_t cs2_index : 16;
            uint32_t reserved4 : 16;
        } bitfields4b;
        uint32_t ordinal4;
    };

} PM4_CE_SET_BASE;

// ---------------------------------- PM4_CE_SWITCH_BUFFER__GFX09 ----------------------------------
typedef struct PM4_CE_SWITCH_BUFFER__GFX09
{
    union
    {
        PM4_CE_TYPE_3_HEADER header;
        uint32_t ordinal1;
    };

    union
    {
        struct
        {
            uint32_t tmz       :  1;
            uint32_t reserved1 : 31;
        } bitfields2;
        uint32_t ordinal2;
    };

} PM4_CE_SWITCH_BUFFER__GFX09;

// -------------------------------- PM4_CE_SWITCH_BUFFER__GFX10CORE --------------------------------
typedef struct PM4_CE_SWITCH_BUFFER__GFX10CORE
{
    union
    {
        PM4_CE_TYPE_3_HEADER header;
        uint32_t ordinal1;
    };

    uint32_t dummy;

} PM4_CE_SWITCH_BUFFER__GFX10CORE;

// -------------------------------- PM4_CE_WAIT_ON_DE_COUNTER_DIFF --------------------------------
typedef struct PM4_CE_WAIT_ON_DE_COUNTER_DIFF
{
    union
    {
        PM4_CE_TYPE_3_HEADER header;
        uint32_t ordinal1;
    };

    uint32_t diff;

} PM4_CE_WAIT_ON_DE_COUNTER_DIFF;

// ------------------------------------ PM4_CE_WRITE_CONST_RAM ------------------------------------
typedef struct PM4_CE_WRITE_CONST_RAM
{
    union
    {
        PM4_CE_TYPE_3_HEADER header;
        uint32_t ordinal1;
    };

    union
    {
        struct
        {
            uint32_t offset    : 16;
            uint32_t reserved1 : 16;
        } bitfields2;
        uint32_t ordinal2;
    };

} PM4_CE_WRITE_CONST_RAM;

// ---------------------------------- CE_WRITE_DATA_dst_sel_enum ----------------------------------
enum CE_WRITE_DATA_dst_sel_enum
{
    dst_sel__ce_write_data__mem_mapped_register__HASCE    =  0,
    dst_sel__ce_write_data__memory__HASCE                 =  5,
    dst_sel__ce_write_data__preemption_meta_memory__HASCE =  8,
};

// --------------------------------- CE_WRITE_DATA_addr_incr_enum ---------------------------------
enum CE_WRITE_DATA_addr_incr_enum
{
    addr_incr__ce_write_data__increment_address__HASCE        =  0,
    addr_incr__ce_write_data__do_not_increment_address__HASCE =  1,
};

// --------------------------------- CE_WRITE_DATA_wr_confirm_enum ---------------------------------
enum CE_WRITE_DATA_wr_confirm_enum
{
    wr_confirm__ce_write_data__do_not_wait_for_write_confirmation__HASCE =  0,
    wr_confirm__ce_write_data__wait_for_write_confirmation__HASCE        =  1,
};

// -------------------------------- CE_WRITE_DATA_cache_policy_enum --------------------------------
enum CE_WRITE_DATA_cache_policy_enum
{
    cache_policy__ce_write_data__lru__HASCE        =  0,
    cache_policy__ce_write_data__stream__HASCE     =  1,
    cache_policy__ce_write_data__noa__GFX10CORE    =  2,
    cache_policy__ce_write_data__bypass__GFX10CORE =  3,
};

// --------------------------------- CE_WRITE_DATA_engine_sel_enum ---------------------------------
enum CE_WRITE_DATA_engine_sel_enum
{
    engine_sel__ce_write_data__constant_engine__HASCE =  2,
};

// --------------------------------------- PM4_CE_WRITE_DATA ---------------------------------------
typedef struct PM4_CE_WRITE_DATA
{
    union
    {
        PM4_CE_TYPE_3_HEADER header;
        uint32_t ordinal1;
    };

    union
    {
        struct
        {
            uint32_t                        reserved1    :  8;
            CE_WRITE_DATA_dst_sel_enum      dst_sel      :  4;
            uint32_t                        reserved2    :  4;
            CE_WRITE_DATA_addr_incr_enum    addr_incr    :  1;
            uint32_t                        reserved3    :  2;
            uint32_t                        resume_vf    :  1;
            CE_WRITE_DATA_wr_confirm_enum   wr_confirm   :  1;
            uint32_t                        reserved4    :  4;
            CE_WRITE_DATA_cache_policy_enum cache_policy :  2;
            uint32_t                        reserved5    :  3;
            CE_WRITE_DATA_engine_sel_enum   engine_sel   :  2;
        } bitfields2;
        uint32_t ordinal2;
    };

    union
    {
        struct
        {
            uint32_t dst_mmreg_addr : 18;
            uint32_t reserved6      : 14;
        } bitfields3a;
        struct
        {
            uint32_t reserved7       :  2;
            uint32_t dst_mem_addr_lo : 30;
        } bitfields3b;
        uint32_t ordinal3;
    };

    uint32_t dst_mem_addr_hi;

} PM4_CE_WRITE_DATA;

