/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
namespace Gfx12
{
inline namespace Chip
{
// ------------------------------------- PM4_ME_TYPE_3_HEADER -------------------------------------
typedef union PM4_ME_TYPE_3_HEADER
{
    struct
    {
        uint32_t predicate      :  1;
        uint32_t shaderType     :  1;
        uint32_t resetFilterCam :  1;
        uint32_t reserved1      :  5;
        uint32_t opcode         :  8;
        uint32_t count          : 14;
        uint32_t type           :  2;
    };
    uint32_t u32All;
} PM4_ME_TYPE_3_HEADER;

// -------------------------------- ME_ACQUIRE_MEM_engine_sel_enum --------------------------------
enum ME_ACQUIRE_MEM_engine_sel_enum
{
    engine_sel__me_acquire_mem__micro_engine =  1,
};

// ------------------------------- ME_ACQUIRE_MEM_pws_stage_sel_enum -------------------------------
enum ME_ACQUIRE_MEM_pws_stage_sel_enum
{
    pws_stage_sel__me_acquire_mem__pre_depth      =  0,
    pws_stage_sel__me_acquire_mem__pre_shader     =  1,
    pws_stage_sel__me_acquire_mem__pre_color      =  2,
    pws_stage_sel__me_acquire_mem__pre_pix_shader =  3,
    pws_stage_sel__me_acquire_mem__cp_pfp         =  4,
    pws_stage_sel__me_acquire_mem__cp_me          =  5,
};

// ------------------------------ ME_ACQUIRE_MEM_pws_counter_sel_enum ------------------------------
enum ME_ACQUIRE_MEM_pws_counter_sel_enum
{
    pws_counter_sel__me_acquire_mem__ts_select =  0,
    pws_counter_sel__me_acquire_mem__ps_select =  1,
    pws_counter_sel__me_acquire_mem__cs_select =  2,
};

// --------------------------------- ME_ACQUIRE_MEM_pws_ena2_enum ---------------------------------
enum ME_ACQUIRE_MEM_pws_ena2_enum
{
    pws_ena2__me_acquire_mem__pixel_wait_sync_disable =  0,
    pws_ena2__me_acquire_mem__pixel_wait_sync_enable  =  1,
};

// ---------------------------------- ME_ACQUIRE_MEM_pws_ena_enum ----------------------------------
enum ME_ACQUIRE_MEM_pws_ena_enum
{
    pws_ena__me_acquire_mem__pixel_wait_sync_disable =  0,
    pws_ena__me_acquire_mem__pixel_wait_sync_enable  =  1,
};

// -------------------------------------- PM4_ME_ACQUIRE_MEM --------------------------------------
typedef struct PM4_ME_ACQUIRE_MEM
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                       coher_cntl : 31;
                ME_ACQUIRE_MEM_engine_sel_enum engine_sel :  1;
            };
        } bitfieldsA;
        union
        {
            struct
            {
                uint32_t                            reserved1       : 11;
                ME_ACQUIRE_MEM_pws_stage_sel_enum   pws_stage_sel   :  3;
                ME_ACQUIRE_MEM_pws_counter_sel_enum pws_counter_sel :  2;
                uint32_t                            reserved2       :  1;
                ME_ACQUIRE_MEM_pws_ena2_enum        pws_ena2        :  1;
                uint32_t                            pws_count       :  6;
                uint32_t                            reserved3       :  8;
            };
        } bitfieldsB;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t coher_size;
        uint32_t gcr_size;
        uint32_t u32All;
    } ordinal3;

    union
    {
        union
        {
            struct
            {
                uint32_t coher_size_hi : 24;
                uint32_t reserved1     :  8;
            };
        } bitfieldsA;
        union
        {
            struct
            {
                uint32_t gcr_size_hi : 25;
                uint32_t reserved2   :  7;
            };
        } bitfieldsB;
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t coher_base_lo;
        uint32_t gcr_base_lo;
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
        } bitfieldsA;
        union
        {
            struct
            {
                uint32_t gcr_base_hi : 25;
                uint32_t reserved2   :  7;
            };
        } bitfieldsB;
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
        } bitfieldsA;
        union
        {
            struct
            {
                uint32_t                    reserved2  : 31;
                ME_ACQUIRE_MEM_pws_ena_enum pws_ena    :  1;
            };
        } bitfieldsB;
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
            };
        } bitfields;
        uint32_t u32All;
    } ordinal8;
} PM4_ME_ACQUIRE_MEM;

constexpr unsigned int PM4_ME_ACQUIRE_MEM_SIZEDW__CORE = 8;

// ----------------------------------- ME_ATOMIC_MEM_atomic_enum -----------------------------------
enum ME_ATOMIC_MEM_atomic_enum
{
    atomic__me_atomic_mem__gl2_op_read                    =  0,
    atomic__me_atomic_mem__gl2_op_atomic_swap_rtn_32      =  7,
    atomic__me_atomic_mem__gl2_op_atomic_cmpswap_rtn_32   =  8,
    atomic__me_atomic_mem__gl2_op_atomic_add_rtn_32       = 15,
    atomic__me_atomic_mem__gl2_op_atomic_sub_rtn_32       = 16,
    atomic__me_atomic_mem__gl2_op_atomic_smin_rtn_32      = 17,
    atomic__me_atomic_mem__gl2_op_atomic_umin_rtn_32      = 18,
    atomic__me_atomic_mem__gl2_op_atomic_smax_rtn_32      = 19,
    atomic__me_atomic_mem__gl2_op_atomic_umax_rtn_32      = 20,
    atomic__me_atomic_mem__gl2_op_atomic_and_rtn_32       = 21,
    atomic__me_atomic_mem__gl2_op_atomic_or_rtn_32        = 22,
    atomic__me_atomic_mem__gl2_op_atomic_xor_rtn_32       = 23,
    atomic__me_atomic_mem__gl2_op_atomic_inc_rtn_32       = 24,
    atomic__me_atomic_mem__gl2_op_atomic_dec_rtn_32       = 25,
    atomic__me_atomic_mem__gl2_op_atomic_clamp_sub_rtn_32 = 26,
    atomic__me_atomic_mem__gl2_op_atomic_cond_sub_rtn_32  = 27,
    atomic__me_atomic_mem__gl2_op_write                   = 32,
    atomic__me_atomic_mem__gl2_op_atomic_swap_rtn_64      = 39,
    atomic__me_atomic_mem__gl2_op_atomic_cmpswap_rtn_64   = 40,
    atomic__me_atomic_mem__gl2_op_atomic_add_rtn_64       = 47,
    atomic__me_atomic_mem__gl2_op_atomic_sub_rtn_64       = 48,
    atomic__me_atomic_mem__gl2_op_atomic_smin_rtn_64      = 49,
    atomic__me_atomic_mem__gl2_op_atomic_umin_rtn_64      = 50,
    atomic__me_atomic_mem__gl2_op_atomic_smax_rtn_64      = 51,
    atomic__me_atomic_mem__gl2_op_atomic_umax_rtn_64      = 52,
    atomic__me_atomic_mem__gl2_op_atomic_and_rtn_64       = 53,
    atomic__me_atomic_mem__gl2_op_atomic_or_rtn_64        = 54,
    atomic__me_atomic_mem__gl2_op_atomic_xor_rtn_64       = 55,
    atomic__me_atomic_mem__gl2_op_atomic_inc_rtn_64       = 56,
    atomic__me_atomic_mem__gl2_op_atomic_dec_rtn_64       = 57,
    atomic__me_atomic_mem__gl2_op_atomic_swap_32          = 71,
    atomic__me_atomic_mem__gl2_op_atomic_cmpswap_32       = 72,
    atomic__me_atomic_mem__gl2_op_atomic_add_32           = 79,
    atomic__me_atomic_mem__gl2_op_atomic_sub_32           = 80,
    atomic__me_atomic_mem__gl2_op_atomic_smin_32          = 81,
    atomic__me_atomic_mem__gl2_op_atomic_umin_32          = 82,
    atomic__me_atomic_mem__gl2_op_atomic_smax_32          = 83,
    atomic__me_atomic_mem__gl2_op_atomic_umax_32          = 84,
    atomic__me_atomic_mem__gl2_op_atomic_and_32           = 85,
    atomic__me_atomic_mem__gl2_op_atomic_or_32            = 86,
    atomic__me_atomic_mem__gl2_op_atomic_xor_32           = 87,
    atomic__me_atomic_mem__gl2_op_atomic_inc_32           = 88,
    atomic__me_atomic_mem__gl2_op_atomic_dec_32           = 89,
    atomic__me_atomic_mem__gl2_op_nop_rtn0                = 91,
    atomic__me_atomic_mem__gl2_op_atomic_swap_64          = 103,
    atomic__me_atomic_mem__gl2_op_atomic_cmpswap_64       = 104,
    atomic__me_atomic_mem__gl2_op_atomic_add_64           = 111,
    atomic__me_atomic_mem__gl2_op_atomic_sub_64           = 112,
    atomic__me_atomic_mem__gl2_op_atomic_smin_64          = 113,
    atomic__me_atomic_mem__gl2_op_atomic_umin_64          = 114,
    atomic__me_atomic_mem__gl2_op_atomic_smax_64          = 115,
    atomic__me_atomic_mem__gl2_op_atomic_umax_64          = 116,
    atomic__me_atomic_mem__gl2_op_atomic_and_64           = 117,
    atomic__me_atomic_mem__gl2_op_atomic_or_64            = 118,
    atomic__me_atomic_mem__gl2_op_atomic_xor_64           = 119,
    atomic__me_atomic_mem__gl2_op_atomic_inc_64           = 120,
    atomic__me_atomic_mem__gl2_op_atomic_dec_64           = 121,
    atomic__me_atomic_mem__gl2_op_nop_ack                 = 123,
};

// ---------------------------------- ME_ATOMIC_MEM_command_enum ----------------------------------
enum ME_ATOMIC_MEM_command_enum
{
    command__me_atomic_mem__single_pass_atomic           =  0,
    command__me_atomic_mem__loop_until_compare_satisfied =  1,
    command__me_atomic_mem__wait_for_write_confirmation  =  2,
    command__me_atomic_mem__send_and_continue            =  3,
};

// ---------------------------------- ME_ATOMIC_MEM_temporal_enum ----------------------------------
enum ME_ATOMIC_MEM_temporal_enum
{
    temporal__me_atomic_mem__rt =  0,
    temporal__me_atomic_mem__nt =  1,
    temporal__me_atomic_mem__ht =  2,
    temporal__me_atomic_mem__lu =  3,
};

// --------------------------------- ME_ATOMIC_MEM_engine_sel_enum ---------------------------------
enum ME_ATOMIC_MEM_engine_sel_enum
{
    engine_sel__me_atomic_mem__micro_engine =  0,
};

// --------------------------------------- PM4_ME_ATOMIC_MEM ---------------------------------------
typedef struct PM4_ME_ATOMIC_MEM
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                ME_ATOMIC_MEM_atomic_enum     atomic     :  7;
                uint32_t                      reserved1  :  1;
                ME_ATOMIC_MEM_command_enum    command    :  4;
                uint32_t                      reserved2  : 13;
                ME_ATOMIC_MEM_temporal_enum   temporal   :  2;
                uint32_t                      reserved3  :  3;
                ME_ATOMIC_MEM_engine_sel_enum engine_sel :  2;
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
} PM4_ME_ATOMIC_MEM;

constexpr unsigned int PM4_ME_ATOMIC_MEM_SIZEDW__CORE = 9;

// ------------------------------------ ME_CLEAR_STATE_cmd_enum ------------------------------------
enum ME_CLEAR_STATE_cmd_enum
{
    cmd__me_clear_state__push_state =  1,
    cmd__me_clear_state__pop_state  =  2,
};

// -------------------------------------- PM4_ME_CLEAR_STATE --------------------------------------
typedef struct PM4_ME_CLEAR_STATE
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                ME_CLEAR_STATE_cmd_enum cmd        :  4;
                uint32_t                reserved1  : 28;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;
} PM4_ME_CLEAR_STATE;

constexpr unsigned int PM4_ME_CLEAR_STATE_SIZEDW__CORE = 2;

// ---------------------------------- ME_COND_WRITE_function_enum ----------------------------------
enum ME_COND_WRITE_function_enum
{
    function__me_cond_write__always_pass                           =  0,
    function__me_cond_write__less_than_ref_value                   =  1,
    function__me_cond_write__less_than_equal_to_the_ref_value      =  2,
    function__me_cond_write__equal_to_the_reference_value          =  3,
    function__me_cond_write__not_equal_reference_value             =  4,
    function__me_cond_write__greater_than_or_equal_reference_value =  5,
    function__me_cond_write__greater_than_reference_value          =  6,
};

// --------------------------------- ME_COND_WRITE_poll_space_enum ---------------------------------
enum ME_COND_WRITE_poll_space_enum
{
    poll_space__me_cond_write__register =  0,
    poll_space__me_cond_write__memory   =  1,
};

// -------------------------------- ME_COND_WRITE_write_space_enum --------------------------------
enum ME_COND_WRITE_write_space_enum
{
    write_space__me_cond_write__register =  0,
    write_space__me_cond_write__memory   =  1,
    write_space__me_cond_write__scratch  =  2,
};

// --------------------------------- ME_COND_WRITE_wr_confirm_enum ---------------------------------
enum ME_COND_WRITE_wr_confirm_enum
{
    wr_confirm__me_cond_write__do_not_wait_for_write_confirmation =  0,
    wr_confirm__me_cond_write__wait_for_write_confirmation        =  1,
};

// --------------------------------------- PM4_ME_COND_WRITE ---------------------------------------
typedef struct PM4_ME_COND_WRITE
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                ME_COND_WRITE_function_enum    function    :  3;
                uint32_t                       reserved1   :  1;
                ME_COND_WRITE_poll_space_enum  poll_space  :  1;
                uint32_t                       reserved2   :  3;
                ME_COND_WRITE_write_space_enum write_space :  2;
                uint32_t                       reserved3   : 10;
                ME_COND_WRITE_wr_confirm_enum  wr_confirm  :  1;
                uint32_t                       reserved4   : 11;
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
} PM4_ME_COND_WRITE;

constexpr unsigned int PM4_ME_COND_WRITE_SIZEDW__CORE = 9;

// ------------------------------------ PM4_ME_CONTEXT_CONTROL ------------------------------------
typedef struct PM4_ME_CONTEXT_CONTROL
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t load_global_config     :  1;
                uint32_t load_per_context_state :  1;
                uint32_t reserved1              : 13;
                uint32_t load_global_uconfig    :  1;
                uint32_t load_gfx_sh_regs       :  1;
                uint32_t reserved2              :  7;
                uint32_t load_cs_sh_regs        :  1;
                uint32_t reserved3              :  6;
                uint32_t update_load_enables    :  1;
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
                uint32_t shadow_global_config     :  1;
                uint32_t shadow_per_context_state :  1;
                uint32_t reserved1                : 13;
                uint32_t shadow_global_uconfig    :  1;
                uint32_t shadow_gfx_sh_regs       :  1;
                uint32_t reserved2                :  7;
                uint32_t shadow_cs_sh_regs        :  1;
                uint32_t reserved3                :  6;
                uint32_t update_shadow_enables    :  1;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal3;
} PM4_ME_CONTEXT_CONTROL;

constexpr unsigned int PM4_ME_CONTEXT_CONTROL_SIZEDW__CORE = 3;

// ------------------------------------ PM4_ME_CONTEXT_REG_RMW ------------------------------------
typedef struct PM4_ME_CONTEXT_REG_RMW
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
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

    union
    {
        uint32_t reg_mask;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t reg_data;
        uint32_t u32All;
    } ordinal4;
} PM4_ME_CONTEXT_REG_RMW;

constexpr unsigned int PM4_ME_CONTEXT_REG_RMW_SIZEDW__CORE = 4;

// ----------------------------------- ME_COPY_DATA_src_sel_enum -----------------------------------
enum ME_COPY_DATA_src_sel_enum
{
    src_sel__me_copy_data__mem_mapped_register =  0,
    src_sel__me_copy_data__tc_l2_obsolete      =  1,
    src_sel__me_copy_data__tc_l2               =  2,
    src_sel__me_copy_data__perfcounters        =  4,
    src_sel__me_copy_data__immediate_data      =  5,
    src_sel__me_copy_data__atomic_return_data  =  6,
    src_sel__me_copy_data__gpu_clock_count     =  9,
    src_sel__me_copy_data__system_clock_count  = 10,
};

// ----------------------------------- ME_COPY_DATA_dst_sel_enum -----------------------------------
enum ME_COPY_DATA_dst_sel_enum
{
    dst_sel__me_copy_data__mem_mapped_register     =  0,
    dst_sel__me_copy_data__memory_sync_across_grbm =  1,
    dst_sel__me_copy_data__tc_l2                   =  2,
    dst_sel__me_copy_data__perfcounters            =  4,
    dst_sel__me_copy_data__tc_l2_obsolete          =  5,
};

// -------------------------------- ME_COPY_DATA_src_temporal_enum --------------------------------
enum ME_COPY_DATA_src_temporal_enum
{
    src_temporal__me_copy_data__rt =  0,
    src_temporal__me_copy_data__nt =  1,
    src_temporal__me_copy_data__ht =  2,
    src_temporal__me_copy_data__lu =  3,
};

// ---------------------------------- ME_COPY_DATA_count_sel_enum ----------------------------------
enum ME_COPY_DATA_count_sel_enum
{
    count_sel__me_copy_data__32_bits_of_data =  0,
    count_sel__me_copy_data__64_bits_of_data =  1,
};

// --------------------------------- ME_COPY_DATA_wr_confirm_enum ---------------------------------
enum ME_COPY_DATA_wr_confirm_enum
{
    wr_confirm__me_copy_data__do_not_wait_for_confirmation =  0,
    wr_confirm__me_copy_data__wait_for_confirmation        =  1,
};

// ------------------------------------ ME_COPY_DATA_mode_enum ------------------------------------
enum ME_COPY_DATA_mode_enum
{
    mode__me_copy_data__PF_VF_disabled =  0,
    mode__me_copy_data__PF_VF_enabled  =  1,
};

// -------------------------------- ME_COPY_DATA_dst_temporal_enum --------------------------------
enum ME_COPY_DATA_dst_temporal_enum
{
    dst_temporal__me_copy_data__rt =  0,
    dst_temporal__me_copy_data__nt =  1,
    dst_temporal__me_copy_data__ht =  2,
    dst_temporal__me_copy_data__lu =  3,
};

// --------------------------------- ME_COPY_DATA_engine_sel_enum ---------------------------------
enum ME_COPY_DATA_engine_sel_enum
{
    engine_sel__me_copy_data__micro_engine =  0,
};

// --------------------------------------- PM4_ME_COPY_DATA ---------------------------------------
typedef struct PM4_ME_COPY_DATA
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                ME_COPY_DATA_src_sel_enum      src_sel      :  4;
                uint32_t                       reserved1    :  4;
                ME_COPY_DATA_dst_sel_enum      dst_sel      :  4;
                uint32_t                       reserved2    :  1;
                ME_COPY_DATA_src_temporal_enum src_temporal :  2;
                uint32_t                       reserved3    :  1;
                ME_COPY_DATA_count_sel_enum    count_sel    :  1;
                uint32_t                       reserved4    :  3;
                ME_COPY_DATA_wr_confirm_enum   wr_confirm   :  1;
                ME_COPY_DATA_mode_enum         mode         :  1;
                uint32_t                       reserved5    :  1;
                uint32_t                       aid_id       :  2;
                ME_COPY_DATA_dst_temporal_enum dst_temporal :  2;
                uint32_t                       reserved6    :  3;
                ME_COPY_DATA_engine_sel_enum   engine_sel   :  2;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t src_reg_offset_lo;
        union
        {
            struct
            {
                uint32_t reserved1       :  2;
                uint32_t src_32b_addr_lo : 30;
            };
        } bitfieldsB;
        union
        {
            struct
            {
                uint32_t reserved2       :  3;
                uint32_t src_64b_addr_lo : 29;
            };
        } bitfieldsC;
        uint32_t imm_data;
        uint32_t u32All;
    } ordinal3;

    union
    {
        union
        {
            struct
            {
                uint32_t src_reg_offset_hi :  6;
                uint32_t reserved1         : 26;
            };
        } bitfieldsA;
        uint32_t src_memtc_addr_hi;
        uint32_t src_imm_data;
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t dst_reg_offset_lo;
        union
        {
            struct
            {
                uint32_t reserved1       :  2;
                uint32_t dst_32b_addr_lo : 30;
            };
        } bitfieldsB;
        union
        {
            struct
            {
                uint32_t reserved2       :  3;
                uint32_t dst_64b_addr_lo : 29;
            };
        } bitfieldsC;
        uint32_t u32All;
    } ordinal5;

    union
    {
        union
        {
            struct
            {
                uint32_t dst_reg_offset_hi :  6;
                uint32_t reserved1         : 26;
            };
        } bitfieldsA;
        uint32_t dst_addr_hi;
        uint32_t u32All;
    } ordinal6;
} PM4_ME_COPY_DATA;

constexpr unsigned int PM4_ME_COPY_DATA_SIZEDW__CORE = 6;

// ------------------------------------ PM4_ME_DISPATCH_DIRECT ------------------------------------
typedef struct PM4_ME_DISPATCH_DIRECT
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
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
} PM4_ME_DISPATCH_DIRECT;

constexpr unsigned int PM4_ME_DISPATCH_DIRECT_SIZEDW__CORE = 5;

// ------------------------------ PM4_ME_DISPATCH_DIRECT_INTERLEAVED ------------------------------
typedef struct PM4_ME_DISPATCH_DIRECT_INTERLEAVED
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        uint32_t dim_z;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t prescale_dim_x;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t prescale_dim_y;
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t dim_x;
        uint32_t u32All;
    } ordinal5;

    union
    {
        uint32_t dim_y;
        uint32_t u32All;
    } ordinal6;

    union
    {
        uint32_t dispatch_initiator;
        uint32_t u32All;
    } ordinal7;
} PM4_ME_DISPATCH_DIRECT_INTERLEAVED;

constexpr unsigned int PM4_ME_DISPATCH_DIRECT_INTERLEAVED_SIZEDW__CORE = 7;

// ----------------------------------- PM4_ME_DISPATCH_INDIRECT -----------------------------------
typedef struct PM4_ME_DISPATCH_INDIRECT
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        uint32_t data_offset;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t dispatch_initiator;
        uint32_t u32All;
    } ordinal3;
} PM4_ME_DISPATCH_INDIRECT;

constexpr unsigned int PM4_ME_DISPATCH_INDIRECT_SIZEDW__CORE = 3;

// ----------------------------- PM4_ME_DISPATCH_INDIRECT_INTERLEAVED -----------------------------
typedef struct PM4_ME_DISPATCH_INDIRECT_INTERLEAVED
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t dim_z      : 16;
                uint32_t reserved1  : 16;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t prescale_dim_x;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t prescale_dim_y;
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t dim_x;
        uint32_t u32All;
    } ordinal5;

    union
    {
        union
        {
            struct
            {
                uint32_t dim_y      : 16;
                uint32_t reserved1  : 16;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal6;

    union
    {
        uint32_t dispatch_initiator;
        uint32_t u32All;
    } ordinal7;
} PM4_ME_DISPATCH_INDIRECT_INTERLEAVED;

constexpr unsigned int PM4_ME_DISPATCH_INDIRECT_INTERLEAVED_SIZEDW__CORE = 7;

// ---------------------------------- PM4_ME_DISPATCH_MESH_DIRECT ----------------------------------
typedef struct PM4_ME_DISPATCH_MESH_DIRECT
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
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
        uint32_t draw_initiator;
        uint32_t u32All;
    } ordinal5;
} PM4_ME_DISPATCH_MESH_DIRECT;

constexpr unsigned int PM4_ME_DISPATCH_MESH_DIRECT_SIZEDW__CORE = 5;

// ------------------------------ PM4_ME_DISPATCH_MESH_INDIRECT_MULTI ------------------------------
typedef struct PM4_ME_DISPATCH_MESH_INDIRECT_MULTI
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        uint32_t data_offset;
        uint32_t u32All;
    } ordinal2;

    union
    {
        union
        {
            struct
            {
                uint32_t xyz_dim_loc    : 16;
                uint32_t draw_index_loc : 16;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal3;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1                  : 27;
                uint32_t mode1_enable               :  1;
                uint32_t xyz_dim_enable             :  1;
                uint32_t thread_trace_marker_enable :  1;
                uint32_t count_indirect_enable      :  1;
                uint32_t draw_index_enable          :  1;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t count;
        uint32_t u32All;
    } ordinal5;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1     :  2;
                uint32_t count_addr_lo : 30;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal6;

    union
    {
        uint32_t count_addr_hi;
        uint32_t u32All;
    } ordinal7;

    union
    {
        uint32_t stride;
        uint32_t u32All;
    } ordinal8;

    union
    {
        uint32_t draw_initiator;
        uint32_t u32All;
    } ordinal9;
} PM4_ME_DISPATCH_MESH_INDIRECT_MULTI;

constexpr unsigned int PM4_ME_DISPATCH_MESH_INDIRECT_MULTI_SIZEDW__CORE = 9;

// -------------------------------- PM4_ME_DISPATCH_TASK_STATE_INIT --------------------------------
typedef struct PM4_ME_DISPATCH_TASK_STATE_INIT
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
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
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t control_buf_addr_hi;
        uint32_t u32All;
    } ordinal3;
} PM4_ME_DISPATCH_TASK_STATE_INIT;

constexpr unsigned int PM4_ME_DISPATCH_TASK_STATE_INIT_SIZEDW__CORE = 3;

// --------------------------------- PM4_ME_DISPATCH_TASKMESH_GFX ---------------------------------
typedef struct PM4_ME_DISPATCH_TASKMESH_GFX
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t xyz_dim_loc    : 16;
                uint32_t ring_entry_loc : 16;
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
                uint32_t reserved1                    : 27;
                uint32_t bottom_of_pipe_ts_after_draw :  1;
                uint32_t linear_dispatch_enable       :  1;
                uint32_t mode1_enable                 :  1;
                uint32_t xyz_dim_enable               :  1;
                uint32_t thread_trace_marker_enable   :  1;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t draw_initiator;
        uint32_t u32All;
    } ordinal4;
} PM4_ME_DISPATCH_TASKMESH_GFX;

constexpr unsigned int PM4_ME_DISPATCH_TASKMESH_GFX_SIZEDW__CORE = 4;

// ---------------------------------- ME_DMA_DATA_engine_sel_enum ----------------------------------
enum ME_DMA_DATA_engine_sel_enum
{
    engine_sel__me_dma_data__micro_engine =  0,
};

// --------------------------------- ME_DMA_DATA_src_temporal_enum ---------------------------------
enum ME_DMA_DATA_src_temporal_enum
{
    src_temporal__me_dma_data__rt =  0,
    src_temporal__me_dma_data__nt =  1,
    src_temporal__me_dma_data__ht =  2,
    src_temporal__me_dma_data__lu =  3,
};

// ----------------------------------- ME_DMA_DATA_dst_sel_enum -----------------------------------
enum ME_DMA_DATA_dst_sel_enum
{
    dst_sel__me_dma_data__dst_addr_using_das =  0,
    dst_sel__me_dma_data__dst_nowhere        =  2,
    dst_sel__me_dma_data__dst_addr_using_l2  =  3,
};

// --------------------------------- ME_DMA_DATA_dst_temporal_enum ---------------------------------
enum ME_DMA_DATA_dst_temporal_enum
{
    dst_temporal__me_dma_data__rt =  0,
    dst_temporal__me_dma_data__nt =  1,
    dst_temporal__me_dma_data__ht =  2,
    dst_temporal__me_dma_data__lu =  3,
};

// ----------------------------------- ME_DMA_DATA_src_sel_enum -----------------------------------
enum ME_DMA_DATA_src_sel_enum
{
    src_sel__me_dma_data__src_addr_using_sas =  0,
    src_sel__me_dma_data__data               =  2,
    src_sel__me_dma_data__src_addr_using_l2  =  3,
};

// ------------------------------------- ME_DMA_DATA_sas_enum -------------------------------------
enum ME_DMA_DATA_sas_enum
{
    sas__me_dma_data__memory   =  0,
    sas__me_dma_data__register =  1,
};

// ------------------------------------- ME_DMA_DATA_das_enum -------------------------------------
enum ME_DMA_DATA_das_enum
{
    das__me_dma_data__memory   =  0,
    das__me_dma_data__register =  1,
};

// ------------------------------------- ME_DMA_DATA_saic_enum -------------------------------------
enum ME_DMA_DATA_saic_enum
{
    saic__me_dma_data__increment    =  0,
    saic__me_dma_data__no_increment =  1,
};

// ------------------------------------- ME_DMA_DATA_daic_enum -------------------------------------
enum ME_DMA_DATA_daic_enum
{
    daic__me_dma_data__increment    =  0,
    daic__me_dma_data__no_increment =  1,
};

// ---------------------------------------- PM4_ME_DMA_DATA ----------------------------------------
typedef struct PM4_ME_DMA_DATA
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                ME_DMA_DATA_engine_sel_enum   engine_sel   :  1;
                uint32_t                      src_indirect :  1;
                uint32_t                      dst_indirect :  1;
                uint32_t                      reserved1    : 10;
                ME_DMA_DATA_src_temporal_enum src_temporal :  2;
                uint32_t                      reserved2    :  5;
                ME_DMA_DATA_dst_sel_enum      dst_sel      :  2;
                uint32_t                      reserved3    :  3;
                ME_DMA_DATA_dst_temporal_enum dst_temporal :  2;
                uint32_t                      reserved4    :  2;
                ME_DMA_DATA_src_sel_enum      src_sel      :  2;
                uint32_t                      cp_sync      :  1;
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
                uint32_t              byte_count : 26;
                ME_DMA_DATA_sas_enum  sas        :  1;
                ME_DMA_DATA_das_enum  das        :  1;
                ME_DMA_DATA_saic_enum saic       :  1;
                ME_DMA_DATA_daic_enum daic       :  1;
                uint32_t              raw_wait   :  1;
                uint32_t              dis_wc     :  1;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal7;
} PM4_ME_DMA_DATA;

constexpr unsigned int PM4_ME_DMA_DATA_SIZEDW__CORE = 7;

// ---------------------------- ME_DMA_DATA_FILL_MULTI_engine_sel_enum ----------------------------
enum ME_DMA_DATA_FILL_MULTI_engine_sel_enum
{
    engine_sel__me_dma_data_fill_multi__micro_engine    =  0,
    engine_sel__me_dma_data_fill_multi__prefetch_parser =  1,
};

// ------------------------------ ME_DMA_DATA_FILL_MULTI_dst_sel_enum ------------------------------
enum ME_DMA_DATA_FILL_MULTI_dst_sel_enum
{
    dst_sel__me_dma_data_fill_multi__dst_addr_using_l2 =  3,
};

// --------------------------- ME_DMA_DATA_FILL_MULTI_dst_temporal_enum ---------------------------
enum ME_DMA_DATA_FILL_MULTI_dst_temporal_enum
{
    dst_temporal__me_dma_data_fill_multi__rt =  0,
    dst_temporal__me_dma_data_fill_multi__nt =  1,
    dst_temporal__me_dma_data_fill_multi__ht =  2,
    dst_temporal__me_dma_data_fill_multi__lu =  3,
};

// ------------------------------ ME_DMA_DATA_FILL_MULTI_src_sel_enum ------------------------------
enum ME_DMA_DATA_FILL_MULTI_src_sel_enum
{
    src_sel__me_dma_data_fill_multi__data =  2,
};

// ---------------------------------- PM4_ME_DMA_DATA_FILL_MULTI ----------------------------------
typedef struct PM4_ME_DMA_DATA_FILL_MULTI
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                ME_DMA_DATA_FILL_MULTI_engine_sel_enum   engine_sel   :  1;
                uint32_t                                 reserved1    :  9;
                uint32_t                                 memlog_clear :  1;
                uint32_t                                 reserved2    :  9;
                ME_DMA_DATA_FILL_MULTI_dst_sel_enum      dst_sel      :  2;
                uint32_t                                 reserved3    :  3;
                ME_DMA_DATA_FILL_MULTI_dst_temporal_enum dst_temporal :  2;
                uint32_t                                 reserved4    :  2;
                ME_DMA_DATA_FILL_MULTI_src_sel_enum      src_sel      :  2;
                uint32_t                                 cp_sync      :  1;
            };
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
            };
        } bitfields;
        uint32_t u32All;
    } ordinal7;
} PM4_ME_DMA_DATA_FILL_MULTI;

constexpr unsigned int PM4_ME_DMA_DATA_FILL_MULTI_SIZEDW__CORE = 7;

// -------------------------------------- PM4_ME_DRAW_INDEX_2 --------------------------------------
typedef struct PM4_ME_DRAW_INDEX_2
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        uint32_t index_count;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t draw_initiator;
        uint32_t u32All;
    } ordinal3;
} PM4_ME_DRAW_INDEX_2;

constexpr unsigned int PM4_ME_DRAW_INDEX_2_SIZEDW__CORE = 3;

// ------------------------------------ PM4_ME_DRAW_INDEX_AUTO ------------------------------------
typedef struct PM4_ME_DRAW_INDEX_AUTO
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        uint32_t index_count;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t draw_initiator;
        uint32_t u32All;
    } ordinal3;
} PM4_ME_DRAW_INDEX_AUTO;

constexpr unsigned int PM4_ME_DRAW_INDEX_AUTO_SIZEDW__CORE = 3;

// ---------------------------------- PM4_ME_DRAW_INDEX_INDIRECT ----------------------------------
typedef struct PM4_ME_DRAW_INDEX_INDIRECT
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t start_indx_enable :  1;
                uint32_t reserved1         : 31;
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
                uint32_t base_vtx_loc   : 16;
                uint32_t start_indx_loc : 16;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal3;

    union
    {
        union
        {
            struct
            {
                uint32_t start_inst_loc : 16;
                uint32_t reserved1      : 16;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t draw_initiator;
        uint32_t u32All;
    } ordinal5;
} PM4_ME_DRAW_INDEX_INDIRECT;

constexpr unsigned int PM4_ME_DRAW_INDEX_INDIRECT_SIZEDW__CORE = 5;

// ------------------------------- PM4_ME_DRAW_INDEX_INDIRECT_MULTI -------------------------------
typedef struct PM4_ME_DRAW_INDEX_INDIRECT_MULTI
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        uint32_t draw_index;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t draw_initiator;
        uint32_t u32All;
    } ordinal3;
} PM4_ME_DRAW_INDEX_INDIRECT_MULTI;

constexpr unsigned int PM4_ME_DRAW_INDEX_INDIRECT_MULTI_SIZEDW__CORE = 3;

// --------------------------------- PM4_ME_DRAW_INDEX_MULTI_AUTO ---------------------------------
typedef struct PM4_ME_DRAW_INDEX_MULTI_AUTO
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        uint32_t prim_count;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t draw_initiator;
        uint32_t u32All;
    } ordinal3;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1         :  6;
                uint32_t num_input_cp      :  6;
                uint32_t prim_per_subgroup :  9;
                uint32_t reserved2         : 11;
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
                uint32_t index_offset : 16;
                uint32_t prim_type    :  5;
                uint32_t index_count  : 11;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal5;
} PM4_ME_DRAW_INDEX_MULTI_AUTO;

constexpr unsigned int PM4_ME_DRAW_INDEX_MULTI_AUTO_SIZEDW__CORE = 5;

// ---------------------------------- PM4_ME_DRAW_INDEX_OFFSET_2 ----------------------------------
typedef struct PM4_ME_DRAW_INDEX_OFFSET_2
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        uint32_t index_count;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t draw_initiator;
        uint32_t u32All;
    } ordinal3;
} PM4_ME_DRAW_INDEX_OFFSET_2;

constexpr unsigned int PM4_ME_DRAW_INDEX_OFFSET_2_SIZEDW__CORE = 3;

// ------------------------------------- PM4_ME_DRAW_INDIRECT -------------------------------------
typedef struct PM4_ME_DRAW_INDIRECT
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t start_vtx_loc : 16;
                uint32_t reserved1     : 16;
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
                uint32_t start_inst_loc : 16;
                uint32_t reserved1      : 16;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t draw_initiator;
        uint32_t u32All;
    } ordinal4;
} PM4_ME_DRAW_INDIRECT;

constexpr unsigned int PM4_ME_DRAW_INDIRECT_SIZEDW__CORE = 4;

// ---------------------------------- PM4_ME_DRAW_INDIRECT_MULTI ----------------------------------
typedef struct PM4_ME_DRAW_INDIRECT_MULTI
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        uint32_t draw_index;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t draw_initiator;
        uint32_t u32All;
    } ordinal3;
} PM4_ME_DRAW_INDIRECT_MULTI;

constexpr unsigned int PM4_ME_DRAW_INDIRECT_MULTI_SIZEDW__CORE = 3;

// -------------------------------- ME_EVENT_WRITE_event_index_enum --------------------------------
enum ME_EVENT_WRITE_event_index_enum
{
    event_index__me_event_write__other                           =  0,
    event_index__me_event_write__pixel_pipe_stat_control_or_dump =  1,
    event_index__me_event_write__sample_pipelinestat             =  2,
    event_index__me_event_write__cs_vs_ps_partial_flush          =  4,
};

// -------------------------------- ME_EVENT_WRITE_counter_id_enum --------------------------------
enum ME_EVENT_WRITE_counter_id_enum
{
    counter_id__me_event_write__pixel_pipe_occlusion_count_0    =  0,
    counter_id__me_event_write__pixel_pipe_occlusion_count_1    =  1,
    counter_id__me_event_write__pixel_pipe_occlusion_count_2    =  2,
    counter_id__me_event_write__pixel_pipe_occlusion_count_3    =  3,
    counter_id__me_event_write__pixel_pipe_screen_min_extents_0 =  4,
    counter_id__me_event_write__pixel_pipe_screen_max_extents_0 =  5,
    counter_id__me_event_write__pixel_pipe_screen_min_extents_1 =  6,
    counter_id__me_event_write__pixel_pipe_screen_max_extents_1 =  7,
};

// -------------------------------------- PM4_ME_EVENT_WRITE --------------------------------------
typedef struct PM4_ME_EVENT_WRITE
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                        event_type  :  6;
                uint32_t                        reserved1   :  2;
                ME_EVENT_WRITE_event_index_enum event_index :  4;
                uint32_t                        reserved2   : 20;
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
                uint32_t reserved1  :  3;
                uint32_t address_lo : 29;
            };
        } bitfieldsA;
        union
        {
            struct
            {
                uint32_t                       reserved2       :  3;
                ME_EVENT_WRITE_counter_id_enum counter_id      :  6;
                uint32_t                       stride          :  2;
                uint32_t                       instance_enable : 16;
                uint32_t                       reserved3       :  5;
            };
        } bitfieldsB;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t address_hi;
        uint32_t u32All;
    } ordinal4;
} PM4_ME_EVENT_WRITE;

constexpr unsigned int PM4_ME_EVENT_WRITE_SIZEDW__CORE = 4;

// ----------------------------------- PM4_ME_EVENT_WRITE_ZPASS -----------------------------------
typedef struct PM4_ME_EVENT_WRITE_ZPASS
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
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
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t address_hi;
        uint32_t u32All;
    } ordinal3;
} PM4_ME_EVENT_WRITE_ZPASS;

constexpr unsigned int PM4_ME_EVENT_WRITE_ZPASS_SIZEDW__CORE = 3;

// ---------------------------- ME_INVALIDATE_TLBS_invalidate_sel_enum ----------------------------
enum ME_INVALIDATE_TLBS_invalidate_sel_enum
{
    invalidate_sel__me_invalidate_tlbs__invalidate =  0,
};

// ------------------------- ME_INVALIDATE_TLBS_mmhub_invalidate_sel_enum -------------------------
enum ME_INVALIDATE_TLBS_mmhub_invalidate_sel_enum
{
    mmhub_invalidate_sel__me_invalidate_tlbs__do_not_invalidate_mmhub =  0,
    mmhub_invalidate_sel__me_invalidate_tlbs__use_mmhub_flush_type    =  1,
    mmhub_invalidate_sel__me_invalidate_tlbs__use_gfx_flush_type      =  2,
};

// ------------------------------------ PM4_ME_INVALIDATE_TLBS ------------------------------------
typedef struct PM4_ME_INVALIDATE_TLBS
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                ME_INVALIDATE_TLBS_invalidate_sel_enum       invalidate_sel       :  3;
                ME_INVALIDATE_TLBS_mmhub_invalidate_sel_enum mmhub_invalidate_sel :  2;
                uint32_t                                     reserved1            : 20;
                uint32_t                                     mmhub_flush_type     :  3;
                uint32_t                                     reserved2            :  1;
                uint32_t                                     gfx_flush_type       :  3;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;
} PM4_ME_INVALIDATE_TLBS;

constexpr unsigned int PM4_ME_INVALIDATE_TLBS_SIZEDW__CORE = 2;

// ------------------------------------ PM4_ME_LOAD_CONFIG_REG ------------------------------------
typedef struct PM4_ME_LOAD_CONFIG_REG
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1    :  2;
                uint32_t base_addr_lo : 30;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t base_addr_hi;
        uint32_t u32All;
    } ordinal3;

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
    } ordinal4;

    union
    {
        union
        {
            struct
            {
                uint32_t num_dwords : 14;
                uint32_t reserved1  : 18;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal5;
} PM4_ME_LOAD_CONFIG_REG;

constexpr unsigned int PM4_ME_LOAD_CONFIG_REG_SIZEDW__CORE = 5;

// ------------------------------------ PM4_ME_LOAD_CONTEXT_REG ------------------------------------
typedef struct PM4_ME_LOAD_CONTEXT_REG
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1    :  2;
                uint32_t base_addr_lo : 30;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t base_addr_hi;
        uint32_t u32All;
    } ordinal3;

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
    } ordinal4;

    union
    {
        union
        {
            struct
            {
                uint32_t num_dwords : 14;
                uint32_t reserved1  : 18;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal5;
} PM4_ME_LOAD_CONTEXT_REG;

constexpr unsigned int PM4_ME_LOAD_CONTEXT_REG_SIZEDW__CORE = 5;

// ----------------------------- ME_LOAD_CONTEXT_REG_INDEX_index_enum -----------------------------
enum ME_LOAD_CONTEXT_REG_INDEX_index_enum
{
    index__me_load_context_reg_index__direct_addr =  0,
    index__me_load_context_reg_index__offset      =  1,
};

// -------------------------- ME_LOAD_CONTEXT_REG_INDEX_data_format_enum --------------------------
enum ME_LOAD_CONTEXT_REG_INDEX_data_format_enum
{
    data_format__me_load_context_reg_index__offset_and_size =  0,
    data_format__me_load_context_reg_index__offset_and_data =  1,
};

// --------------------------------- PM4_ME_LOAD_CONTEXT_REG_INDEX ---------------------------------
typedef struct PM4_ME_LOAD_CONTEXT_REG_INDEX
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                ME_LOAD_CONTEXT_REG_INDEX_index_enum index       :  1;
                uint32_t                             reserved1   :  1;
                uint32_t                             mem_addr_lo : 30;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t mem_addr_hi;
        uint32_t addr_offset;
        uint32_t u32All;
    } ordinal3;

    union
    {
        union
        {
            struct
            {
                uint32_t                                   reg_offset  : 16;
                uint32_t                                   reserved1   : 15;
                ME_LOAD_CONTEXT_REG_INDEX_data_format_enum data_format :  1;
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
                uint32_t num_dwords : 14;
                uint32_t reserved1  : 18;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal5;
} PM4_ME_LOAD_CONTEXT_REG_INDEX;

constexpr unsigned int PM4_ME_LOAD_CONTEXT_REG_INDEX_SIZEDW__CORE = 5;

// -------------------------------------- PM4_ME_LOAD_SH_REG --------------------------------------
typedef struct PM4_ME_LOAD_SH_REG
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1       :  2;
                uint32_t base_address_lo : 30;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t base_address_hi;
        uint32_t u32All;
    } ordinal3;

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
    } ordinal4;

    union
    {
        union
        {
            struct
            {
                uint32_t num_dword  : 14;
                uint32_t reserved1  : 18;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal5;
} PM4_ME_LOAD_SH_REG;

constexpr unsigned int PM4_ME_LOAD_SH_REG_SIZEDW__CORE = 5;

// -------------------------------- ME_LOAD_SH_REG_INDEX_index_enum --------------------------------
enum ME_LOAD_SH_REG_INDEX_index_enum
{
    index__me_load_sh_reg_index__direct_addr   =  0,
    index__me_load_sh_reg_index__offset        =  1,
    index__me_load_sh_reg_index__indirect_addr =  2,
};

// ----------------------------- ME_LOAD_SH_REG_INDEX_data_format_enum -----------------------------
enum ME_LOAD_SH_REG_INDEX_data_format_enum
{
    data_format__me_load_sh_reg_index__offset_and_size =  0,
    data_format__me_load_sh_reg_index__offset_and_data =  1,
};

// ----------------------------------- PM4_ME_LOAD_SH_REG_INDEX -----------------------------------
typedef struct PM4_ME_LOAD_SH_REG_INDEX
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                ME_LOAD_SH_REG_INDEX_index_enum index       :  2;
                uint32_t                        mem_addr_lo : 30;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t mem_addr_hi;
        uint32_t addr_offset;
        uint32_t u32All;
    } ordinal3;

    union
    {
        union
        {
            struct
            {
                uint32_t                              reg_offset  : 16;
                uint32_t                              reserved1   : 15;
                ME_LOAD_SH_REG_INDEX_data_format_enum data_format :  1;
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
                uint32_t num_dwords : 14;
                uint32_t reserved1  : 18;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal5;
} PM4_ME_LOAD_SH_REG_INDEX;

constexpr unsigned int PM4_ME_LOAD_SH_REG_INDEX_SIZEDW__CORE = 5;

// ------------------------------------ PM4_ME_LOAD_UCONFIG_REG ------------------------------------
typedef struct PM4_ME_LOAD_UCONFIG_REG
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1       :  2;
                uint32_t base_address_lo : 30;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t base_address_hi;
        uint32_t u32All;
    } ordinal3;

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
    } ordinal4;

    union
    {
        union
        {
            struct
            {
                uint32_t num_dwords : 14;
                uint32_t reserved1  : 18;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal5;
} PM4_ME_LOAD_UCONFIG_REG;

constexpr unsigned int PM4_ME_LOAD_UCONFIG_REG_SIZEDW__CORE = 5;

// ----------------------------- ME_LOAD_UCONFIG_REG_INDEX_index_enum -----------------------------
enum ME_LOAD_UCONFIG_REG_INDEX_index_enum
{
    index__me_load_uconfig_reg_index__direct_addr =  0,
    index__me_load_uconfig_reg_index__offset      =  1,
};

// -------------------------- ME_LOAD_UCONFIG_REG_INDEX_data_format_enum --------------------------
enum ME_LOAD_UCONFIG_REG_INDEX_data_format_enum
{
    data_format__me_load_uconfig_reg_index__offset_and_size =  0,
    data_format__me_load_uconfig_reg_index__offset_and_data =  1,
};

// --------------------------------- PM4_ME_LOAD_UCONFIG_REG_INDEX ---------------------------------
typedef struct PM4_ME_LOAD_UCONFIG_REG_INDEX
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                ME_LOAD_UCONFIG_REG_INDEX_index_enum index       :  1;
                uint32_t                             reserved1   :  1;
                uint32_t                             mem_addr_lo : 30;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t mem_addr_hi;
        uint32_t addr_offset;
        uint32_t u32All;
    } ordinal3;

    union
    {
        union
        {
            struct
            {
                uint32_t                                   reg_offset  : 16;
                uint32_t                                   reserved1   : 15;
                ME_LOAD_UCONFIG_REG_INDEX_data_format_enum data_format :  1;
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
                uint32_t num_dwords : 14;
                uint32_t reserved1  : 18;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal5;
} PM4_ME_LOAD_UCONFIG_REG_INDEX;

constexpr unsigned int PM4_ME_LOAD_UCONFIG_REG_INDEX_SIZEDW__CORE = 5;

// -------------------------------- ME_PERFMON_CONTROL_pmc_en_enum --------------------------------
enum ME_PERFMON_CONTROL_pmc_en_enum
{
    pmc_en__me_perfmon_control__perfmon_disable =  0,
    pmc_en__me_perfmon_control__perfmon_enable  =  1,
};

// ------------------------------------ PM4_ME_PERFMON_CONTROL ------------------------------------
typedef struct PM4_ME_PERFMON_CONTROL
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                       pmc_id        :  3;
                uint32_t                       reserved1     : 12;
                ME_PERFMON_CONTROL_pmc_en_enum pmc_en        :  1;
                uint32_t                       pmc_unit_mask :  8;
                uint32_t                       reserved2     :  8;
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
                uint32_t pmc_event  : 14;
                uint32_t reserved1  : 18;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal3;
} PM4_ME_PERFMON_CONTROL;

constexpr unsigned int PM4_ME_PERFMON_CONTROL_SIZEDW__CORE = 3;

// -------------------------------------- PM4_ME_PFP_SYNC_ME --------------------------------------
typedef struct PM4_ME_PFP_SYNC_ME
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        uint32_t dummy_data;
        uint32_t u32All;
    } ordinal2;
} PM4_ME_PFP_SYNC_ME;

constexpr unsigned int PM4_ME_PFP_SYNC_ME_SIZEDW__CORE = 2;

// --------------------------------- ME_PREAMBLE_CNTL_command_enum ---------------------------------
enum ME_PREAMBLE_CNTL_command_enum
{
    command__me_preamble_cntl__user_queues_state_save    =  4,
    command__me_preamble_cntl__user_queues_state_restore =  5,
};

// ------------------------------------- PM4_ME_PREAMBLE_CNTL -------------------------------------
typedef struct PM4_ME_PREAMBLE_CNTL
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                      reserved1  : 28;
                ME_PREAMBLE_CNTL_command_enum command    :  4;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;
} PM4_ME_PREAMBLE_CNTL;

constexpr unsigned int PM4_ME_PREAMBLE_CNTL_SIZEDW__CORE = 2;

// -------------------------------- ME_REG_RMW_shadow_base_sel_enum --------------------------------
enum ME_REG_RMW_shadow_base_sel_enum
{
    shadow_base_sel__me_reg_rmw__no_shadow             =  0,
    shadow_base_sel__me_reg_rmw__shadow_global_uconfig =  1,
};

// ---------------------------------- ME_REG_RMW_or_mask_src_enum ----------------------------------
enum ME_REG_RMW_or_mask_src_enum
{
    or_mask_src__me_reg_rmw__immediate   =  0,
    or_mask_src__me_reg_rmw__reg_or_addr =  1,
};

// --------------------------------- ME_REG_RMW_and_mask_src_enum ---------------------------------
enum ME_REG_RMW_and_mask_src_enum
{
    and_mask_src__me_reg_rmw__immediate    =  0,
    and_mask_src__me_reg_rmw__reg_and_addr =  1,
};

// ---------------------------------------- PM4_ME_REG_RMW ----------------------------------------
typedef struct PM4_ME_REG_RMW
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                        mod_addr        : 18;
                uint32_t                        reserved1       :  6;
                ME_REG_RMW_shadow_base_sel_enum shadow_base_sel :  2;
                uint32_t                        reserved2       :  4;
                ME_REG_RMW_or_mask_src_enum     or_mask_src     :  1;
                ME_REG_RMW_and_mask_src_enum    and_mask_src    :  1;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t and_mask;
        union
        {
            struct
            {
                uint32_t and_addr   : 18;
                uint32_t reserved1  : 14;
            };
        } bitfieldsB;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t or_mask;
        union
        {
            struct
            {
                uint32_t or_addr    : 18;
                uint32_t reserved1  : 14;
            };
        } bitfieldsB;
        uint32_t u32All;
    } ordinal4;
} PM4_ME_REG_RMW;

constexpr unsigned int PM4_ME_REG_RMW_SIZEDW__CORE = 4;

// -------------------------------- ME_RELEASE_MEM_event_index_enum --------------------------------
enum ME_RELEASE_MEM_event_index_enum
{
    event_index__me_release_mem__end_of_pipe =  5,
    event_index__me_release_mem__shader_done =  6,
};

// --------------------------------- ME_RELEASE_MEM_temporal_enum ---------------------------------
enum ME_RELEASE_MEM_temporal_enum
{
    temporal__me_release_mem__rt =  0,
    temporal__me_release_mem__nt =  1,
    temporal__me_release_mem__ht =  2,
    temporal__me_release_mem__lu =  3,
};

// ---------------------------------- ME_RELEASE_MEM_dst_sel_enum ----------------------------------
enum ME_RELEASE_MEM_dst_sel_enum
{
    dst_sel__me_release_mem__memory_controller                 =  0,
    dst_sel__me_release_mem__tc_l2                             =  1,
    dst_sel__me_release_mem__queue_write_pointer_register      =  2,
    dst_sel__me_release_mem__queue_write_pointer_poll_mask_bit =  3,
};

// ------------------------------- ME_RELEASE_MEM_mes_action_id_enum -------------------------------
enum ME_RELEASE_MEM_mes_action_id_enum
{
    mes_action_id__me_release_mem__no_mes_notification                     =  0,
    mes_action_id__me_release_mem__interrupt_and_fence                     =  1,
    mes_action_id__me_release_mem__interrupt_no_fence_then_address_payload =  2,
    mes_action_id__me_release_mem__interrupt_and_address_payload           =  3,
};

// ---------------------------------- ME_RELEASE_MEM_int_sel_enum ----------------------------------
enum ME_RELEASE_MEM_int_sel_enum
{
    int_sel__me_release_mem__none                                                 =  0,
    int_sel__me_release_mem__send_interrupt_only                                  =  1,
    int_sel__me_release_mem__send_interrupt_after_write_confirm                   =  2,
    int_sel__me_release_mem__send_data_and_write_confirm                          =  3,
    int_sel__me_release_mem__unconditionally_send_int_ctxid                       =  4,
    int_sel__me_release_mem__conditionally_send_int_ctxid_based_on_32_bit_compare =  5,
    int_sel__me_release_mem__conditionally_send_int_ctxid_based_on_64_bit_compare =  6,
};

// --------------------------------- ME_RELEASE_MEM_data_sel_enum ---------------------------------
enum ME_RELEASE_MEM_data_sel_enum
{
    data_sel__me_release_mem__none                      =  0,
    data_sel__me_release_mem__send_32_bit_low           =  1,
    data_sel__me_release_mem__send_64_bit_data          =  2,
    data_sel__me_release_mem__send_gpu_clock_counter    =  3,
    data_sel__me_release_mem__send_system_clock_counter =  4,
};

// -------------------------------------- PM4_ME_RELEASE_MEM --------------------------------------
typedef struct PM4_ME_RELEASE_MEM
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                        event_type  :  6;
                uint32_t                        reserved1   :  1;
                uint32_t                        wait_sync   :  1;
                ME_RELEASE_MEM_event_index_enum event_index :  4;
                uint32_t                        gcr_cntl    : 13;
                ME_RELEASE_MEM_temporal_enum    temporal    :  2;
                uint32_t                        reserved2   :  1;
                uint32_t                        execute     :  2;
                uint32_t                        glk_inv     :  1;
                uint32_t                        pws_enable  :  1;
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
                uint32_t                          reserved1     : 16;
                ME_RELEASE_MEM_dst_sel_enum       dst_sel       :  2;
                uint32_t                          reserved2     :  2;
                uint32_t                          mes_intr_pipe :  2;
                ME_RELEASE_MEM_mes_action_id_enum mes_action_id :  2;
                ME_RELEASE_MEM_int_sel_enum       int_sel       :  3;
                uint32_t                          reserved3     :  2;
                ME_RELEASE_MEM_data_sel_enum      data_sel      :  3;
            };
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
        union
        {
            struct
            {
                uint32_t int_ctxid  : 28;
                uint32_t reserved1  :  4;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal8;
} PM4_ME_RELEASE_MEM;

constexpr unsigned int PM4_ME_RELEASE_MEM_SIZEDW__CORE = 8;

// ------------------------------------- PM4_ME_SET_CONFIG_REG -------------------------------------
typedef struct PM4_ME_SET_CONFIG_REG
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
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
} PM4_ME_SET_CONFIG_REG;

constexpr unsigned int PM4_ME_SET_CONFIG_REG_SIZEDW__CORE = 2;

// ------------------------------------ PM4_ME_SET_CONTEXT_REG ------------------------------------
typedef struct PM4_ME_SET_CONTEXT_REG
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
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
} PM4_ME_SET_CONTEXT_REG;

constexpr unsigned int PM4_ME_SET_CONTEXT_REG_SIZEDW__CORE = 2;

// --------------------------------------- PM4_ME_SET_SH_REG ---------------------------------------
typedef struct PM4_ME_SET_SH_REG
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
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
} PM4_ME_SET_SH_REG;

constexpr unsigned int PM4_ME_SET_SH_REG_SIZEDW__CORE = 2;

// -------------------------------- ME_SET_SH_REG_INDEX_index_enum --------------------------------
enum ME_SET_SH_REG_INDEX_index_enum
{
    index__me_set_sh_reg_index__compute_dispatch_interleave_shadow =  2,
    index__me_set_sh_reg_index__apply_kmd_cu_and_mask              =  3,
};

// ------------------------------------ PM4_ME_SET_SH_REG_INDEX ------------------------------------
typedef struct PM4_ME_SET_SH_REG_INDEX
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                       reg_offset : 16;
                uint32_t                       reserved1  : 12;
                ME_SET_SH_REG_INDEX_index_enum index      :  4;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;
} PM4_ME_SET_SH_REG_INDEX;

constexpr unsigned int PM4_ME_SET_SH_REG_INDEX_SIZEDW__CORE = 2;

// -------------------------------- ME_SET_SH_REG_OFFSET_index_enum --------------------------------
enum ME_SET_SH_REG_OFFSET_index_enum
{
    index__me_set_sh_reg_offset__normal_operation       =  0,
    index__me_set_sh_reg_offset__data_indirect_2dw_256b =  1,
    index__me_set_sh_reg_offset__data_indirect_1dw      =  2,
};

// ----------------------------------- PM4_ME_SET_SH_REG_OFFSET -----------------------------------
typedef struct PM4_ME_SET_SH_REG_OFFSET
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                        reg_offset : 16;
                uint32_t                        reserved1  : 14;
                ME_SET_SH_REG_OFFSET_index_enum index      :  2;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t calculated_lo;
        uint32_t u32All;
    } ordinal3;

    union
    {
        union
        {
            struct
            {
                uint32_t calculated_hi : 16;
                uint32_t driver_data   : 16;
            };
        } bitfieldsA;
        uint32_t calculated_hi;
        uint32_t u32All;
    } ordinal4;
} PM4_ME_SET_SH_REG_OFFSET;

constexpr unsigned int PM4_ME_SET_SH_REG_OFFSET_SIZEDW__CORE = 4;

// ------------------------------------ PM4_ME_SET_UCONFIG_REG ------------------------------------
typedef struct PM4_ME_SET_UCONFIG_REG
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
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
} PM4_ME_SET_UCONFIG_REG;

constexpr unsigned int PM4_ME_SET_UCONFIG_REG_SIZEDW__CORE = 2;

// --------------------------------- PM4_ME_SET_UCONFIG_REG_INDEX ---------------------------------
typedef struct PM4_ME_SET_UCONFIG_REG_INDEX
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
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
} PM4_ME_SET_UCONFIG_REG_INDEX;

constexpr unsigned int PM4_ME_SET_UCONFIG_REG_INDEX_SIZEDW__CORE = 2;

// ----------------------------- PM4_ME_UPDATE_DB_SUMMARIZER_TIMEOUTS -----------------------------
typedef struct PM4_ME_UPDATE_DB_SUMMARIZER_TIMEOUTS
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        uint32_t reg_value;
        uint32_t u32All;
    } ordinal2;
} PM4_ME_UPDATE_DB_SUMMARIZER_TIMEOUTS;

constexpr unsigned int PM4_ME_UPDATE_DB_SUMMARIZER_TIMEOUTS_SIZEDW__CORE = 2;

// ---------------------------------- ME_TIMESTAMP_clock_sel_enum ----------------------------------
enum ME_TIMESTAMP_clock_sel_enum
{
    clock_sel__me_timestamp__gfx_ip_clock =  0,
    clock_sel__me_timestamp__soc_clock    =  1,
};

// --------------------------------------- PM4_ME_TIMESTAMP ---------------------------------------
typedef struct PM4_ME_TIMESTAMP
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                    enable_bottom :  1;
                uint32_t                    enable_top    :  1;
                ME_TIMESTAMP_clock_sel_enum clock_sel     :  1;
                uint32_t                    reserved1     : 29;
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
                uint32_t reserved1        :  3;
                uint32_t pipe_bot_addr_lo : 29;
            };
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
            };
        } bitfields;
        uint32_t u32All;
    } ordinal5;

    union
    {
        uint32_t pipe_top_addr_hi;
        uint32_t u32All;
    } ordinal6;
} PM4_ME_TIMESTAMP;

constexpr unsigned int PM4_ME_TIMESTAMP_SIZEDW__CORE = 6;

// --------------------------------- ME_WAIT_REG_MEM_function_enum ---------------------------------
enum ME_WAIT_REG_MEM_function_enum
{
    function__me_wait_reg_mem__always_pass                           =  0,
    function__me_wait_reg_mem__less_than_ref_value                   =  1,
    function__me_wait_reg_mem__less_than_equal_to_the_ref_value      =  2,
    function__me_wait_reg_mem__equal_to_the_reference_value          =  3,
    function__me_wait_reg_mem__not_equal_reference_value             =  4,
    function__me_wait_reg_mem__greater_than_or_equal_reference_value =  5,
    function__me_wait_reg_mem__greater_than_reference_value          =  6,
};

// -------------------------------- ME_WAIT_REG_MEM_mem_space_enum --------------------------------
enum ME_WAIT_REG_MEM_mem_space_enum
{
    mem_space__me_wait_reg_mem__register_space =  0,
    mem_space__me_wait_reg_mem__memory_space   =  1,
};

// -------------------------------- ME_WAIT_REG_MEM_operation_enum --------------------------------
enum ME_WAIT_REG_MEM_operation_enum
{
    operation__me_wait_reg_mem__wait_reg_mem         =  0,
    operation__me_wait_reg_mem__wait_reg_mem_cond    =  2,
    operation__me_wait_reg_mem__wait_mem_preemptable =  3,
};

// -------------------------------- ME_WAIT_REG_MEM_engine_sel_enum --------------------------------
enum ME_WAIT_REG_MEM_engine_sel_enum
{
    engine_sel__me_wait_reg_mem__micro_engine =  0,
};

// --------------------------------- ME_WAIT_REG_MEM_temporal_enum ---------------------------------
enum ME_WAIT_REG_MEM_temporal_enum
{
    temporal__me_wait_reg_mem__rt =  0,
    temporal__me_wait_reg_mem__nt =  1,
    temporal__me_wait_reg_mem__ht =  2,
    temporal__me_wait_reg_mem__lu =  3,
};

// -------------------------------------- PM4_ME_WAIT_REG_MEM --------------------------------------
typedef struct PM4_ME_WAIT_REG_MEM
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                ME_WAIT_REG_MEM_function_enum   function      :  3;
                uint32_t                        reserved1     :  1;
                ME_WAIT_REG_MEM_mem_space_enum  mem_space     :  2;
                ME_WAIT_REG_MEM_operation_enum  operation     :  2;
                ME_WAIT_REG_MEM_engine_sel_enum engine_sel    :  2;
                uint32_t                        reserved2     : 12;
                uint32_t                        mes_intr_pipe :  2;
                uint32_t                        mes_action    :  1;
                ME_WAIT_REG_MEM_temporal_enum   temporal      :  2;
                uint32_t                        reserved3     :  5;
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
                uint32_t poll_interval : 16;
                uint32_t reserved1     : 16;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal7;
} PM4_ME_WAIT_REG_MEM;

constexpr unsigned int PM4_ME_WAIT_REG_MEM_SIZEDW__CORE = 7;

// -------------------------------- ME_WAIT_REG_MEM64_function_enum --------------------------------
enum ME_WAIT_REG_MEM64_function_enum
{
    function__me_wait_reg_mem64__always_pass                           =  0,
    function__me_wait_reg_mem64__less_than_ref_value                   =  1,
    function__me_wait_reg_mem64__less_than_equal_to_the_ref_value      =  2,
    function__me_wait_reg_mem64__equal_to_the_reference_value          =  3,
    function__me_wait_reg_mem64__not_equal_reference_value             =  4,
    function__me_wait_reg_mem64__greater_than_or_equal_reference_value =  5,
    function__me_wait_reg_mem64__greater_than_reference_value          =  6,
};

// ------------------------------- ME_WAIT_REG_MEM64_mem_space_enum -------------------------------
enum ME_WAIT_REG_MEM64_mem_space_enum
{
    mem_space__me_wait_reg_mem64__register_space =  0,
    mem_space__me_wait_reg_mem64__memory_space   =  1,
};

// ------------------------------- ME_WAIT_REG_MEM64_operation_enum -------------------------------
enum ME_WAIT_REG_MEM64_operation_enum
{
    operation__me_wait_reg_mem64__wait_reg_mem         =  0,
    operation__me_wait_reg_mem64__wait_reg_mem_cond    =  2,
    operation__me_wait_reg_mem64__wait_mem_preemptable =  3,
};

// ------------------------------- ME_WAIT_REG_MEM64_engine_sel_enum -------------------------------
enum ME_WAIT_REG_MEM64_engine_sel_enum
{
    engine_sel__me_wait_reg_mem64__micro_engine =  0,
};

// -------------------------------- ME_WAIT_REG_MEM64_temporal_enum --------------------------------
enum ME_WAIT_REG_MEM64_temporal_enum
{
    temporal__me_wait_reg_mem64__rt =  0,
    temporal__me_wait_reg_mem64__nt =  1,
    temporal__me_wait_reg_mem64__ht =  2,
    temporal__me_wait_reg_mem64__lu =  3,
};

// ------------------------------------- PM4_ME_WAIT_REG_MEM64 -------------------------------------
typedef struct PM4_ME_WAIT_REG_MEM64
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                ME_WAIT_REG_MEM64_function_enum   function      :  3;
                uint32_t                          reserved1     :  1;
                ME_WAIT_REG_MEM64_mem_space_enum  mem_space     :  2;
                ME_WAIT_REG_MEM64_operation_enum  operation     :  2;
                ME_WAIT_REG_MEM64_engine_sel_enum engine_sel    :  2;
                uint32_t                          reserved2     : 12;
                uint32_t                          mes_intr_pipe :  2;
                uint32_t                          mes_action    :  1;
                ME_WAIT_REG_MEM64_temporal_enum   temporal      :  2;
                uint32_t                          reserved3     :  5;
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
                uint32_t poll_interval : 16;
                uint32_t reserved1     : 16;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal9;
} PM4_ME_WAIT_REG_MEM64;

constexpr unsigned int PM4_ME_WAIT_REG_MEM64_SIZEDW__CORE = 9;

// ---------------------------------- ME_WRITE_DATA_dst_sel_enum ----------------------------------
enum ME_WRITE_DATA_dst_sel_enum
{
    dst_sel__me_write_data__mem_mapped_register     =  0,
    dst_sel__me_write_data__memory_sync_across_grbm =  1,
    dst_sel__me_write_data__tc_l2                   =  2,
    dst_sel__me_write_data__memory                  =  5,
};

// --------------------------------- ME_WRITE_DATA_addr_incr_enum ---------------------------------
enum ME_WRITE_DATA_addr_incr_enum
{
    addr_incr__me_write_data__increment_address        =  0,
    addr_incr__me_write_data__do_not_increment_address =  1,
};

// --------------------------------- ME_WRITE_DATA_wr_confirm_enum ---------------------------------
enum ME_WRITE_DATA_wr_confirm_enum
{
    wr_confirm__me_write_data__do_not_wait_for_write_confirmation =  0,
    wr_confirm__me_write_data__wait_for_write_confirmation        =  1,
};

// ------------------------------------ ME_WRITE_DATA_mode_enum ------------------------------------
enum ME_WRITE_DATA_mode_enum
{
    mode__me_write_data__PF_VF_disabled =  0,
    mode__me_write_data__PF_VF_enabled  =  1,
};

// ---------------------------------- ME_WRITE_DATA_temporal_enum ----------------------------------
enum ME_WRITE_DATA_temporal_enum
{
    temporal__me_write_data__rt =  0,
    temporal__me_write_data__nt =  1,
    temporal__me_write_data__ht =  2,
    temporal__me_write_data__lu =  3,
};

// --------------------------------- ME_WRITE_DATA_engine_sel_enum ---------------------------------
enum ME_WRITE_DATA_engine_sel_enum
{
    engine_sel__me_write_data__micro_engine =  0,
};

// --------------------------------------- PM4_ME_WRITE_DATA ---------------------------------------
typedef struct PM4_ME_WRITE_DATA
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                      reserved1  :  8;
                ME_WRITE_DATA_dst_sel_enum    dst_sel    :  4;
                uint32_t                      reserved2  :  4;
                ME_WRITE_DATA_addr_incr_enum  addr_incr  :  1;
                uint32_t                      reserved3  :  3;
                ME_WRITE_DATA_wr_confirm_enum wr_confirm :  1;
                ME_WRITE_DATA_mode_enum       mode       :  1;
                uint32_t                      reserved4  :  1;
                uint32_t                      aid_id     :  2;
                ME_WRITE_DATA_temporal_enum   temporal   :  2;
                uint32_t                      reserved5  :  3;
                ME_WRITE_DATA_engine_sel_enum engine_sel :  2;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t dst_mmreg_addr_lo;
        union
        {
            struct
            {
                uint32_t reserved1       :  2;
                uint32_t dst_mem_addr_lo : 30;
            };
        } bitfieldsB;
        uint32_t u32All;
    } ordinal3;

    union
    {
        union
        {
            struct
            {
                uint32_t dst_mmreg_addr_hi :  6;
                uint32_t reserved1         : 26;
            };
        } bitfieldsA;
        uint32_t dst_mem_addr_hi;
        uint32_t u32All;
    } ordinal4;
} PM4_ME_WRITE_DATA;

constexpr unsigned int PM4_ME_WRITE_DATA_SIZEDW__CORE = 4;

} // inline namespace Chip
} // namespace Gfx12
} // namespace Pal
