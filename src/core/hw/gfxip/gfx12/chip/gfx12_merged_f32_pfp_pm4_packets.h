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
// ------------------------------------- PM4_PFP_TYPE_3_HEADER -------------------------------------
typedef union PM4_PFP_TYPE_3_HEADER
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
} PM4_PFP_TYPE_3_HEADER;

// -------------------------------- PFP_ACQUIRE_MEM_engine_sel_enum --------------------------------
enum PFP_ACQUIRE_MEM_engine_sel_enum
{
    engine_sel__pfp_acquire_mem__prefetch_parser =  0,
    engine_sel__pfp_acquire_mem__micro_engine    =  1,
};

// ------------------------------ PFP_ACQUIRE_MEM_pws_stage_sel_enum ------------------------------
enum PFP_ACQUIRE_MEM_pws_stage_sel_enum
{
    pws_stage_sel__pfp_acquire_mem__pre_depth      =  0,
    pws_stage_sel__pfp_acquire_mem__pre_shader     =  1,
    pws_stage_sel__pfp_acquire_mem__pre_color      =  2,
    pws_stage_sel__pfp_acquire_mem__pre_pix_shader =  3,
    pws_stage_sel__pfp_acquire_mem__cp_pfp         =  4,
    pws_stage_sel__pfp_acquire_mem__cp_me          =  5,
};

// ----------------------------- PFP_ACQUIRE_MEM_pws_counter_sel_enum -----------------------------
enum PFP_ACQUIRE_MEM_pws_counter_sel_enum
{
    pws_counter_sel__pfp_acquire_mem__ts_select =  0,
    pws_counter_sel__pfp_acquire_mem__ps_select =  1,
    pws_counter_sel__pfp_acquire_mem__cs_select =  2,
};

// --------------------------------- PFP_ACQUIRE_MEM_pws_ena2_enum ---------------------------------
enum PFP_ACQUIRE_MEM_pws_ena2_enum
{
    pws_ena2__pfp_acquire_mem__pixel_wait_sync_disable =  0,
    pws_ena2__pfp_acquire_mem__pixel_wait_sync_enable  =  1,
};

// --------------------------------- PFP_ACQUIRE_MEM_pws_ena_enum ---------------------------------
enum PFP_ACQUIRE_MEM_pws_ena_enum
{
    pws_ena__pfp_acquire_mem__pixel_wait_sync_disable =  0,
    pws_ena__pfp_acquire_mem__pixel_wait_sync_enable  =  1,
};

// -------------------------------------- PM4_PFP_ACQUIRE_MEM --------------------------------------
typedef struct PM4_PFP_ACQUIRE_MEM
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                        coher_cntl : 31;
                PFP_ACQUIRE_MEM_engine_sel_enum engine_sel :  1;
            };
        } bitfieldsA;
        union
        {
            struct
            {
                uint32_t                             reserved1       : 11;
                PFP_ACQUIRE_MEM_pws_stage_sel_enum   pws_stage_sel   :  3;
                PFP_ACQUIRE_MEM_pws_counter_sel_enum pws_counter_sel :  2;
                uint32_t                             reserved2       :  1;
                PFP_ACQUIRE_MEM_pws_ena2_enum        pws_ena2        :  1;
                uint32_t                             pws_count       :  6;
                uint32_t                             reserved3       :  8;
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
                uint32_t                     reserved2  : 31;
                PFP_ACQUIRE_MEM_pws_ena_enum pws_ena    :  1;
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
} PM4_PFP_ACQUIRE_MEM;

constexpr unsigned int PM4_PFP_ACQUIRE_MEM_SIZEDW__CORE = 8;

// ---------------------------------- PFP_ATOMIC_MEM_atomic_enum ----------------------------------
enum PFP_ATOMIC_MEM_atomic_enum
{
    atomic__pfp_atomic_mem__gl2_op_read                    =  0,
    atomic__pfp_atomic_mem__gl2_op_atomic_swap_rtn_32      =  7,
    atomic__pfp_atomic_mem__gl2_op_atomic_cmpswap_rtn_32   =  8,
    atomic__pfp_atomic_mem__gl2_op_atomic_add_rtn_32       = 15,
    atomic__pfp_atomic_mem__gl2_op_atomic_sub_rtn_32       = 16,
    atomic__pfp_atomic_mem__gl2_op_atomic_smin_rtn_32      = 17,
    atomic__pfp_atomic_mem__gl2_op_atomic_umin_rtn_32      = 18,
    atomic__pfp_atomic_mem__gl2_op_atomic_smax_rtn_32      = 19,
    atomic__pfp_atomic_mem__gl2_op_atomic_umax_rtn_32      = 20,
    atomic__pfp_atomic_mem__gl2_op_atomic_and_rtn_32       = 21,
    atomic__pfp_atomic_mem__gl2_op_atomic_or_rtn_32        = 22,
    atomic__pfp_atomic_mem__gl2_op_atomic_xor_rtn_32       = 23,
    atomic__pfp_atomic_mem__gl2_op_atomic_inc_rtn_32       = 24,
    atomic__pfp_atomic_mem__gl2_op_atomic_dec_rtn_32       = 25,
    atomic__pfp_atomic_mem__gl2_op_atomic_clamp_sub_rtn_32 = 26,
    atomic__pfp_atomic_mem__gl2_op_atomic_cond_sub_rtn_32  = 27,
    atomic__pfp_atomic_mem__gl2_op_write                   = 32,
    atomic__pfp_atomic_mem__gl2_op_atomic_swap_rtn_64      = 39,
    atomic__pfp_atomic_mem__gl2_op_atomic_cmpswap_rtn_64   = 40,
    atomic__pfp_atomic_mem__gl2_op_atomic_add_rtn_64       = 47,
    atomic__pfp_atomic_mem__gl2_op_atomic_sub_rtn_64       = 48,
    atomic__pfp_atomic_mem__gl2_op_atomic_smin_rtn_64      = 49,
    atomic__pfp_atomic_mem__gl2_op_atomic_umin_rtn_64      = 50,
    atomic__pfp_atomic_mem__gl2_op_atomic_smax_rtn_64      = 51,
    atomic__pfp_atomic_mem__gl2_op_atomic_umax_rtn_64      = 52,
    atomic__pfp_atomic_mem__gl2_op_atomic_and_rtn_64       = 53,
    atomic__pfp_atomic_mem__gl2_op_atomic_or_rtn_64        = 54,
    atomic__pfp_atomic_mem__gl2_op_atomic_xor_rtn_64       = 55,
    atomic__pfp_atomic_mem__gl2_op_atomic_inc_rtn_64       = 56,
    atomic__pfp_atomic_mem__gl2_op_atomic_dec_rtn_64       = 57,
    atomic__pfp_atomic_mem__gl2_op_atomic_swap_32          = 71,
    atomic__pfp_atomic_mem__gl2_op_atomic_cmpswap_32       = 72,
    atomic__pfp_atomic_mem__gl2_op_atomic_add_32           = 79,
    atomic__pfp_atomic_mem__gl2_op_atomic_sub_32           = 80,
    atomic__pfp_atomic_mem__gl2_op_atomic_smin_32          = 81,
    atomic__pfp_atomic_mem__gl2_op_atomic_umin_32          = 82,
    atomic__pfp_atomic_mem__gl2_op_atomic_smax_32          = 83,
    atomic__pfp_atomic_mem__gl2_op_atomic_umax_32          = 84,
    atomic__pfp_atomic_mem__gl2_op_atomic_and_32           = 85,
    atomic__pfp_atomic_mem__gl2_op_atomic_or_32            = 86,
    atomic__pfp_atomic_mem__gl2_op_atomic_xor_32           = 87,
    atomic__pfp_atomic_mem__gl2_op_atomic_inc_32           = 88,
    atomic__pfp_atomic_mem__gl2_op_atomic_dec_32           = 89,
    atomic__pfp_atomic_mem__gl2_op_nop_rtn0                = 91,
    atomic__pfp_atomic_mem__gl2_op_atomic_swap_64          = 103,
    atomic__pfp_atomic_mem__gl2_op_atomic_cmpswap_64       = 104,
    atomic__pfp_atomic_mem__gl2_op_atomic_add_64           = 111,
    atomic__pfp_atomic_mem__gl2_op_atomic_sub_64           = 112,
    atomic__pfp_atomic_mem__gl2_op_atomic_smin_64          = 113,
    atomic__pfp_atomic_mem__gl2_op_atomic_umin_64          = 114,
    atomic__pfp_atomic_mem__gl2_op_atomic_smax_64          = 115,
    atomic__pfp_atomic_mem__gl2_op_atomic_umax_64          = 116,
    atomic__pfp_atomic_mem__gl2_op_atomic_and_64           = 117,
    atomic__pfp_atomic_mem__gl2_op_atomic_or_64            = 118,
    atomic__pfp_atomic_mem__gl2_op_atomic_xor_64           = 119,
    atomic__pfp_atomic_mem__gl2_op_atomic_inc_64           = 120,
    atomic__pfp_atomic_mem__gl2_op_atomic_dec_64           = 121,
    atomic__pfp_atomic_mem__gl2_op_nop_ack                 = 123,
};

// ---------------------------------- PFP_ATOMIC_MEM_command_enum ----------------------------------
enum PFP_ATOMIC_MEM_command_enum
{
    command__pfp_atomic_mem__single_pass_atomic           =  0,
    command__pfp_atomic_mem__loop_until_compare_satisfied =  1,
    command__pfp_atomic_mem__wait_for_write_confirmation  =  2,
    command__pfp_atomic_mem__send_and_continue            =  3,
};

// --------------------------------- PFP_ATOMIC_MEM_temporal_enum ---------------------------------
enum PFP_ATOMIC_MEM_temporal_enum
{
    temporal__pfp_atomic_mem__rt =  0,
    temporal__pfp_atomic_mem__nt =  1,
    temporal__pfp_atomic_mem__ht =  2,
    temporal__pfp_atomic_mem__lu =  3,
};

// -------------------------------- PFP_ATOMIC_MEM_engine_sel_enum --------------------------------
enum PFP_ATOMIC_MEM_engine_sel_enum
{
    engine_sel__pfp_atomic_mem__micro_engine    =  0,
    engine_sel__pfp_atomic_mem__prefetch_parser =  1,
};

// -------------------------------------- PM4_PFP_ATOMIC_MEM --------------------------------------
typedef struct PM4_PFP_ATOMIC_MEM
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                PFP_ATOMIC_MEM_atomic_enum     atomic     :  7;
                uint32_t                       reserved1  :  1;
                PFP_ATOMIC_MEM_command_enum    command    :  4;
                uint32_t                       reserved2  : 13;
                PFP_ATOMIC_MEM_temporal_enum   temporal   :  2;
                uint32_t                       reserved3  :  3;
                PFP_ATOMIC_MEM_engine_sel_enum engine_sel :  2;
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
} PM4_PFP_ATOMIC_MEM;

constexpr unsigned int PM4_PFP_ATOMIC_MEM_SIZEDW__CORE = 9;

// ------------------------------- PFP_BUILD_UNTYPED_SRD_index_enum -------------------------------
enum PFP_BUILD_UNTYPED_SRD_index_enum
{
    index__pfp_build_untyped_srd__use_addrs   =  0,
    index__pfp_build_untyped_srd__addrs_known =  1,
};

// ----------------------------------- PM4_PFP_BUILD_UNTYPED_SRD -----------------------------------
typedef struct PM4_PFP_BUILD_UNTYPED_SRD
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                PFP_BUILD_UNTYPED_SRD_index_enum index                      :  1;
                uint32_t                         vertex_offset_mode_enabled :  1;
                uint32_t                         reserved1                  :  2;
                uint32_t                         src_addr_lo                : 28;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t src_addr_hi;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t src_offset;
        uint32_t u32All;
    } ordinal4;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1   :  2;
                uint32_t dst_addr_lo : 30;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal5;

    union
    {
        uint32_t dst_addr_hi;
        uint32_t u32All;
    } ordinal6;

    union
    {
        uint32_t dst_offset;
        uint32_t u32All;
    } ordinal7;

    union
    {
        uint32_t dword3;
        uint32_t u32All;
    } ordinal8;
} PM4_PFP_BUILD_UNTYPED_SRD;

constexpr unsigned int PM4_PFP_BUILD_UNTYPED_SRD_SIZEDW__CORE = 8;

// ----------------------------------- PFP_CLEAR_STATE_cmd_enum -----------------------------------
enum PFP_CLEAR_STATE_cmd_enum
{
    cmd__pfp_clear_state__push_state =  1,
    cmd__pfp_clear_state__pop_state  =  2,
};

// -------------------------------------- PM4_PFP_CLEAR_STATE --------------------------------------
typedef struct PM4_PFP_CLEAR_STATE
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                PFP_CLEAR_STATE_cmd_enum cmd        :  4;
                uint32_t                 reserved1  : 28;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;
} PM4_PFP_CLEAR_STATE;

constexpr unsigned int PM4_PFP_CLEAR_STATE_SIZEDW__CORE = 2;

// ------------------------------------ PM4_PFP_CLEANER_SHADER ------------------------------------
typedef struct PM4_PFP_CLEANER_SHADER
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        uint32_t reserved1;
        uint32_t u32All;
    } ordinal2;
} PM4_PFP_CLEANER_SHADER;

constexpr unsigned int PM4_PFP_CLEANER_SHADER_SIZEDW__CORE = 2;

// --------------------------------------- PM4_PFP_COND_EXEC ---------------------------------------
typedef struct PM4_PFP_COND_EXEC
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
        uint32_t reserved2;
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
} PM4_PFP_COND_EXEC;

constexpr unsigned int PM4_PFP_COND_EXEC_SIZEDW__CORE = 5;

// ------------------------------ PFP_COND_INDIRECT_BUFFER_mode_enum ------------------------------
enum PFP_COND_INDIRECT_BUFFER_mode_enum
{
    mode__pfp_cond_indirect_buffer__if_then      =  1,
    mode__pfp_cond_indirect_buffer__if_then_else =  2,
};

// ---------------------------- PFP_COND_INDIRECT_BUFFER_function_enum ----------------------------
enum PFP_COND_INDIRECT_BUFFER_function_enum
{
    function__pfp_cond_indirect_buffer__always_pass                           =  0,
    function__pfp_cond_indirect_buffer__less_than_ref_value                   =  1,
    function__pfp_cond_indirect_buffer__less_than_equal_to_the_ref_value      =  2,
    function__pfp_cond_indirect_buffer__equal_to_the_reference_value          =  3,
    function__pfp_cond_indirect_buffer__not_equal_reference_value             =  4,
    function__pfp_cond_indirect_buffer__greater_than_or_equal_reference_value =  5,
    function__pfp_cond_indirect_buffer__greater_than_reference_value          =  6,
};

// ---------------------------- PFP_COND_INDIRECT_BUFFER_temporal1_enum ----------------------------
enum PFP_COND_INDIRECT_BUFFER_temporal1_enum
{
    temporal1__pfp_cond_indirect_buffer__rt =  0,
    temporal1__pfp_cond_indirect_buffer__nt =  1,
    temporal1__pfp_cond_indirect_buffer__ht =  2,
    temporal1__pfp_cond_indirect_buffer__lu =  3,
};

// ---------------------------- PFP_COND_INDIRECT_BUFFER_temporal2_enum ----------------------------
enum PFP_COND_INDIRECT_BUFFER_temporal2_enum
{
    temporal2__pfp_cond_indirect_buffer__rt =  0,
    temporal2__pfp_cond_indirect_buffer__nt =  1,
    temporal2__pfp_cond_indirect_buffer__ht =  2,
    temporal2__pfp_cond_indirect_buffer__lu =  3,
};

// --------------------------------- PM4_PFP_COND_INDIRECT_BUFFER ---------------------------------
typedef struct PM4_PFP_COND_INDIRECT_BUFFER
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                PFP_COND_INDIRECT_BUFFER_mode_enum     mode       :  2;
                uint32_t                               reserved1  :  6;
                PFP_COND_INDIRECT_BUFFER_function_enum function   :  3;
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
                uint32_t                                ib_size1   : 20;
                uint32_t                                reserved1  :  8;
                PFP_COND_INDIRECT_BUFFER_temporal1_enum temporal1  :  2;
                uint32_t                                reserved2  :  2;
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
                uint32_t                                ib_size2   : 20;
                uint32_t                                reserved1  :  8;
                PFP_COND_INDIRECT_BUFFER_temporal2_enum temporal2  :  2;
                uint32_t                                reserved2  :  2;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal14;
} PM4_PFP_COND_INDIRECT_BUFFER;

constexpr unsigned int PM4_PFP_COND_INDIRECT_BUFFER_SIZEDW__CORE = 14;

// ------------------------------------ PM4_PFP_CONTEXT_CONTROL ------------------------------------
typedef struct PM4_PFP_CONTEXT_CONTROL
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
} PM4_PFP_CONTEXT_CONTROL;

constexpr unsigned int PM4_PFP_CONTEXT_CONTROL_SIZEDW__CORE = 3;

// ------------------------------------ PM4_PFP_CONTEXT_REG_RMW ------------------------------------
typedef struct PM4_PFP_CONTEXT_REG_RMW
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
} PM4_PFP_CONTEXT_REG_RMW;

constexpr unsigned int PM4_PFP_CONTEXT_REG_RMW_SIZEDW__CORE = 4;

// ---------------------------------- PFP_COPY_DATA_src_sel_enum ----------------------------------
enum PFP_COPY_DATA_src_sel_enum
{
    src_sel__pfp_copy_data__mem_mapped_register =  0,
    src_sel__pfp_copy_data__tc_l2_obsolete      =  1,
    src_sel__pfp_copy_data__tc_l2               =  2,
    src_sel__pfp_copy_data__immediate_data      =  5,
    src_sel__pfp_copy_data__exec_ind_arg_buf    = 12,
};

// ---------------------------------- PFP_COPY_DATA_dst_sel_enum ----------------------------------
enum PFP_COPY_DATA_dst_sel_enum
{
    dst_sel__pfp_copy_data__mem_mapped_register  =  0,
    dst_sel__pfp_copy_data__tc_l2                =  2,
    dst_sel__pfp_copy_data__tc_l2_obsolete       =  5,
    dst_sel__pfp_copy_data__exec_ind_spill_table =  7,
};

// -------------------------------- PFP_COPY_DATA_src_temporal_enum --------------------------------
enum PFP_COPY_DATA_src_temporal_enum
{
    src_temporal__pfp_copy_data__rt =  0,
    src_temporal__pfp_copy_data__nt =  1,
    src_temporal__pfp_copy_data__ht =  2,
    src_temporal__pfp_copy_data__lu =  3,
};

// --------------------------------- PFP_COPY_DATA_count_sel_enum ---------------------------------
enum PFP_COPY_DATA_count_sel_enum
{
    count_sel__pfp_copy_data__32_bits_of_data =  0,
    count_sel__pfp_copy_data__64_bits_of_data =  1,
};

// --------------------------------- PFP_COPY_DATA_wr_confirm_enum ---------------------------------
enum PFP_COPY_DATA_wr_confirm_enum
{
    wr_confirm__pfp_copy_data__do_not_wait_for_confirmation =  0,
    wr_confirm__pfp_copy_data__wait_for_confirmation        =  1,
};

// ------------------------------------ PFP_COPY_DATA_mode_enum ------------------------------------
enum PFP_COPY_DATA_mode_enum
{
    mode__pfp_copy_data__PF_VF_disabled =  0,
    mode__pfp_copy_data__PF_VF_enabled  =  1,
};

// -------------------------------- PFP_COPY_DATA_dst_temporal_enum --------------------------------
enum PFP_COPY_DATA_dst_temporal_enum
{
    dst_temporal__pfp_copy_data__rt =  0,
    dst_temporal__pfp_copy_data__nt =  1,
    dst_temporal__pfp_copy_data__ht =  2,
    dst_temporal__pfp_copy_data__lu =  3,
};

// --------------------------------- PFP_COPY_DATA_engine_sel_enum ---------------------------------
enum PFP_COPY_DATA_engine_sel_enum
{
    engine_sel__pfp_copy_data__micro_engine    =  0,
    engine_sel__pfp_copy_data__prefetch_parser =  1,
};

// --------------------------------------- PM4_PFP_COPY_DATA ---------------------------------------
typedef struct PM4_PFP_COPY_DATA
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                PFP_COPY_DATA_src_sel_enum      src_sel      :  4;
                uint32_t                        reserved1    :  4;
                PFP_COPY_DATA_dst_sel_enum      dst_sel      :  4;
                uint32_t                        reserved2    :  1;
                PFP_COPY_DATA_src_temporal_enum src_temporal :  2;
                uint32_t                        reserved3    :  1;
                PFP_COPY_DATA_count_sel_enum    count_sel    :  1;
                uint32_t                        reserved4    :  3;
                PFP_COPY_DATA_wr_confirm_enum   wr_confirm   :  1;
                PFP_COPY_DATA_mode_enum         mode         :  1;
                uint32_t                        reserved5    :  1;
                uint32_t                        aid_id       :  2;
                PFP_COPY_DATA_dst_temporal_enum dst_temporal :  2;
                uint32_t                        reserved6    :  3;
                PFP_COPY_DATA_engine_sel_enum   engine_sel   :  2;
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
} PM4_PFP_COPY_DATA;

constexpr unsigned int PM4_PFP_COPY_DATA_SIZEDW__CORE = 6;

// ------------------------------------ PM4_PFP_DISPATCH_DIRECT ------------------------------------
typedef struct PM4_PFP_DISPATCH_DIRECT
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
} PM4_PFP_DISPATCH_DIRECT;

constexpr unsigned int PM4_PFP_DISPATCH_DIRECT_SIZEDW__CORE = 5;

// ------------------------------ PM4_PFP_DISPATCH_DIRECT_INTERLEAVED ------------------------------
typedef struct PM4_PFP_DISPATCH_DIRECT_INTERLEAVED
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
} PM4_PFP_DISPATCH_DIRECT_INTERLEAVED;

constexpr unsigned int PM4_PFP_DISPATCH_DIRECT_INTERLEAVED_SIZEDW__CORE = 5;

// ----------------------------------- PM4_PFP_DISPATCH_INDIRECT -----------------------------------
typedef struct PM4_PFP_DISPATCH_INDIRECT
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
} PM4_PFP_DISPATCH_INDIRECT;

constexpr unsigned int PM4_PFP_DISPATCH_INDIRECT_SIZEDW__CORE = 3;

// ----------------------------- PM4_PFP_DISPATCH_INDIRECT_INTERLEAVED -----------------------------
typedef struct PM4_PFP_DISPATCH_INDIRECT_INTERLEAVED
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
} PM4_PFP_DISPATCH_INDIRECT_INTERLEAVED;

constexpr unsigned int PM4_PFP_DISPATCH_INDIRECT_INTERLEAVED_SIZEDW__CORE = 3;

// --------------------------------- PM4_PFP_DISPATCH_MESH_DIRECT ---------------------------------
typedef struct PM4_PFP_DISPATCH_MESH_DIRECT
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
} PM4_PFP_DISPATCH_MESH_DIRECT;

constexpr unsigned int PM4_PFP_DISPATCH_MESH_DIRECT_SIZEDW__CORE = 5;

// ----------------------------- PM4_PFP_DISPATCH_MESH_INDIRECT_MULTI -----------------------------
typedef struct PM4_PFP_DISPATCH_MESH_INDIRECT_MULTI
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
} PM4_PFP_DISPATCH_MESH_INDIRECT_MULTI;

constexpr unsigned int PM4_PFP_DISPATCH_MESH_INDIRECT_MULTI_SIZEDW__CORE = 9;

// ------------------------------- PM4_PFP_DISPATCH_TASK_STATE_INIT -------------------------------
typedef struct PM4_PFP_DISPATCH_TASK_STATE_INIT
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
} PM4_PFP_DISPATCH_TASK_STATE_INIT;

constexpr unsigned int PM4_PFP_DISPATCH_TASK_STATE_INIT_SIZEDW__CORE = 3;

// --------------------------------- PM4_PFP_DISPATCH_TASKMESH_GFX ---------------------------------
typedef struct PM4_PFP_DISPATCH_TASKMESH_GFX
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
} PM4_PFP_DISPATCH_TASKMESH_GFX;

constexpr unsigned int PM4_PFP_DISPATCH_TASKMESH_GFX_SIZEDW__CORE = 4;

// --------------------------------- PFP_DMA_DATA_engine_sel_enum ---------------------------------
enum PFP_DMA_DATA_engine_sel_enum
{
    engine_sel__pfp_dma_data__micro_engine    =  0,
    engine_sel__pfp_dma_data__prefetch_parser =  1,
};

// -------------------------------- PFP_DMA_DATA_src_temporal_enum --------------------------------
enum PFP_DMA_DATA_src_temporal_enum
{
    src_temporal__pfp_dma_data__rt =  0,
    src_temporal__pfp_dma_data__nt =  1,
    src_temporal__pfp_dma_data__ht =  2,
    src_temporal__pfp_dma_data__lu =  3,
};

// ----------------------------------- PFP_DMA_DATA_dst_sel_enum -----------------------------------
enum PFP_DMA_DATA_dst_sel_enum
{
    dst_sel__pfp_dma_data__dst_addr_using_das =  0,
    dst_sel__pfp_dma_data__dst_nowhere        =  2,
    dst_sel__pfp_dma_data__dst_addr_using_l2  =  3,
};

// -------------------------------- PFP_DMA_DATA_dst_temporal_enum --------------------------------
enum PFP_DMA_DATA_dst_temporal_enum
{
    dst_temporal__pfp_dma_data__rt =  0,
    dst_temporal__pfp_dma_data__nt =  1,
    dst_temporal__pfp_dma_data__ht =  2,
    dst_temporal__pfp_dma_data__lu =  3,
};

// ----------------------------------- PFP_DMA_DATA_src_sel_enum -----------------------------------
enum PFP_DMA_DATA_src_sel_enum
{
    src_sel__pfp_dma_data__src_addr_using_sas =  0,
    src_sel__pfp_dma_data__data               =  2,
    src_sel__pfp_dma_data__src_addr_using_l2  =  3,
};

// ------------------------------------- PFP_DMA_DATA_sas_enum -------------------------------------
enum PFP_DMA_DATA_sas_enum
{
    sas__pfp_dma_data__memory   =  0,
    sas__pfp_dma_data__register =  1,
};

// ------------------------------------- PFP_DMA_DATA_das_enum -------------------------------------
enum PFP_DMA_DATA_das_enum
{
    das__pfp_dma_data__memory   =  0,
    das__pfp_dma_data__register =  1,
};

// ------------------------------------ PFP_DMA_DATA_saic_enum ------------------------------------
enum PFP_DMA_DATA_saic_enum
{
    saic__pfp_dma_data__increment    =  0,
    saic__pfp_dma_data__no_increment =  1,
};

// ------------------------------------ PFP_DMA_DATA_daic_enum ------------------------------------
enum PFP_DMA_DATA_daic_enum
{
    daic__pfp_dma_data__increment    =  0,
    daic__pfp_dma_data__no_increment =  1,
};

// --------------------------------------- PM4_PFP_DMA_DATA ---------------------------------------
typedef struct PM4_PFP_DMA_DATA
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                PFP_DMA_DATA_engine_sel_enum   engine_sel   :  1;
                uint32_t                       src_indirect :  1;
                uint32_t                       dst_indirect :  1;
                uint32_t                       reserved1    : 10;
                PFP_DMA_DATA_src_temporal_enum src_temporal :  2;
                uint32_t                       reserved2    :  5;
                PFP_DMA_DATA_dst_sel_enum      dst_sel      :  2;
                uint32_t                       reserved3    :  3;
                PFP_DMA_DATA_dst_temporal_enum dst_temporal :  2;
                uint32_t                       reserved4    :  2;
                PFP_DMA_DATA_src_sel_enum      src_sel      :  2;
                uint32_t                       cp_sync      :  1;
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
                PFP_DMA_DATA_sas_enum  sas        :  1;
                PFP_DMA_DATA_das_enum  das        :  1;
                PFP_DMA_DATA_saic_enum saic       :  1;
                PFP_DMA_DATA_daic_enum daic       :  1;
                uint32_t               raw_wait   :  1;
                uint32_t               dis_wc     :  1;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal7;
} PM4_PFP_DMA_DATA;

constexpr unsigned int PM4_PFP_DMA_DATA_SIZEDW__CORE = 7;

// ---------------------------- PFP_DMA_DATA_FILL_MULTI_engine_sel_enum ----------------------------
enum PFP_DMA_DATA_FILL_MULTI_engine_sel_enum
{
    engine_sel__pfp_dma_data_fill_multi__micro_engine    =  0,
    engine_sel__pfp_dma_data_fill_multi__prefetch_parser =  1,
};

// ----------------------------- PFP_DMA_DATA_FILL_MULTI_dst_sel_enum -----------------------------
enum PFP_DMA_DATA_FILL_MULTI_dst_sel_enum
{
    dst_sel__pfp_dma_data_fill_multi__dst_addr_using_l2 =  3,
};

// --------------------------- PFP_DMA_DATA_FILL_MULTI_dst_temporal_enum ---------------------------
enum PFP_DMA_DATA_FILL_MULTI_dst_temporal_enum
{
    dst_temporal__pfp_dma_data_fill_multi__rt =  0,
    dst_temporal__pfp_dma_data_fill_multi__nt =  1,
    dst_temporal__pfp_dma_data_fill_multi__ht =  2,
    dst_temporal__pfp_dma_data_fill_multi__lu =  3,
};

// ----------------------------- PFP_DMA_DATA_FILL_MULTI_src_sel_enum -----------------------------
enum PFP_DMA_DATA_FILL_MULTI_src_sel_enum
{
    src_sel__pfp_dma_data_fill_multi__data =  2,
};

// ---------------------------------- PM4_PFP_DMA_DATA_FILL_MULTI ----------------------------------
typedef struct PM4_PFP_DMA_DATA_FILL_MULTI
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                PFP_DMA_DATA_FILL_MULTI_engine_sel_enum   engine_sel   :  1;
                uint32_t                                  reserved1    :  9;
                uint32_t                                  memlog_clear :  1;
                uint32_t                                  reserved2    :  9;
                PFP_DMA_DATA_FILL_MULTI_dst_sel_enum      dst_sel      :  2;
                uint32_t                                  reserved3    :  3;
                PFP_DMA_DATA_FILL_MULTI_dst_temporal_enum dst_temporal :  2;
                uint32_t                                  reserved4    :  2;
                PFP_DMA_DATA_FILL_MULTI_src_sel_enum      src_sel      :  2;
                uint32_t                                  cp_sync      :  1;
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
} PM4_PFP_DMA_DATA_FILL_MULTI;

constexpr unsigned int PM4_PFP_DMA_DATA_FILL_MULTI_SIZEDW__CORE = 7;

// ------------------------------------- PM4_PFP_DRAW_INDEX_2 -------------------------------------
typedef struct PM4_PFP_DRAW_INDEX_2
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        uint32_t max_size;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t index_base_lo;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t index_base_hi;
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t index_count;
        uint32_t u32All;
    } ordinal5;

    union
    {
        uint32_t draw_initiator;
        uint32_t u32All;
    } ordinal6;
} PM4_PFP_DRAW_INDEX_2;

constexpr unsigned int PM4_PFP_DRAW_INDEX_2_SIZEDW__CORE = 6;

// ------------------------------------ PM4_PFP_DRAW_INDEX_AUTO ------------------------------------
typedef struct PM4_PFP_DRAW_INDEX_AUTO
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
} PM4_PFP_DRAW_INDEX_AUTO;

constexpr unsigned int PM4_PFP_DRAW_INDEX_AUTO_SIZEDW__CORE = 3;

// ---------------------------------- PM4_PFP_DRAW_INDEX_INDIRECT ----------------------------------
typedef struct PM4_PFP_DRAW_INDEX_INDIRECT
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
                uint32_t start_inst_loc    : 16;
                uint32_t reserved1         : 12;
                uint32_t start_indx_enable :  1;
                uint32_t reserved2         :  3;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t draw_initiator;
        uint32_t u32All;
    } ordinal5;
} PM4_PFP_DRAW_INDEX_INDIRECT;

constexpr unsigned int PM4_PFP_DRAW_INDEX_INDIRECT_SIZEDW__CORE = 5;

// ------------------------------- PM4_PFP_DRAW_INDEX_INDIRECT_MULTI -------------------------------
typedef struct PM4_PFP_DRAW_INDEX_INDIRECT_MULTI
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
        union
        {
            struct
            {
                uint32_t draw_index_loc               : 16;
                uint32_t reserved1                    :  9;
                uint32_t task_shader_mode             :  1;
                uint32_t bottom_of_pipe_ts_after_draw :  1;
                uint32_t use_vgprs                    :  1;
                uint32_t start_indx_enable            :  1;
                uint32_t thread_trace_marker_enable   :  1;
                uint32_t count_indirect_enable        :  1;
                uint32_t draw_index_enable            :  1;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal5;

    union
    {
        uint32_t count;
        uint32_t u32All;
    } ordinal6;

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
    } ordinal7;

    union
    {
        uint32_t count_addr_hi;
        uint32_t u32All;
    } ordinal8;

    union
    {
        uint32_t stride;
        uint32_t u32All;
    } ordinal9;

    union
    {
        uint32_t draw_initiator;
        uint32_t u32All;
    } ordinal10;
} PM4_PFP_DRAW_INDEX_INDIRECT_MULTI;

constexpr unsigned int PM4_PFP_DRAW_INDEX_INDIRECT_MULTI_SIZEDW__CORE = 10;

// --------------------------------- PM4_PFP_DRAW_INDEX_MULTI_AUTO ---------------------------------
typedef struct PM4_PFP_DRAW_INDEX_MULTI_AUTO
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
} PM4_PFP_DRAW_INDEX_MULTI_AUTO;

constexpr unsigned int PM4_PFP_DRAW_INDEX_MULTI_AUTO_SIZEDW__CORE = 5;

// ---------------------------------- PM4_PFP_DRAW_INDEX_OFFSET_2 ----------------------------------
typedef struct PM4_PFP_DRAW_INDEX_OFFSET_2
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        uint32_t max_size;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t index_offset;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t index_count;
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t draw_initiator;
        uint32_t u32All;
    } ordinal5;
} PM4_PFP_DRAW_INDEX_OFFSET_2;

constexpr unsigned int PM4_PFP_DRAW_INDEX_OFFSET_2_SIZEDW__CORE = 5;

// ------------------------------------- PM4_PFP_DRAW_INDIRECT -------------------------------------
typedef struct PM4_PFP_DRAW_INDIRECT
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
                uint32_t start_vtx_loc : 16;
                uint32_t reserved1     : 16;
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
} PM4_PFP_DRAW_INDIRECT;

constexpr unsigned int PM4_PFP_DRAW_INDIRECT_SIZEDW__CORE = 5;

// ---------------------------------- PM4_PFP_DRAW_INDIRECT_MULTI ----------------------------------
typedef struct PM4_PFP_DRAW_INDIRECT_MULTI
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
                uint32_t start_vtx_loc : 16;
                uint32_t reserved1     : 16;
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
        union
        {
            struct
            {
                uint32_t draw_index_loc               : 16;
                uint32_t reserved1                    : 10;
                uint32_t bottom_of_pipe_ts_after_draw :  1;
                uint32_t use_vgprs                    :  1;
                uint32_t reserved2                    :  1;
                uint32_t thread_trace_marker_enable   :  1;
                uint32_t count_indirect_enable        :  1;
                uint32_t draw_index_enable            :  1;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal5;

    union
    {
        uint32_t count;
        uint32_t u32All;
    } ordinal6;

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
    } ordinal7;

    union
    {
        uint32_t count_addr_hi;
        uint32_t u32All;
    } ordinal8;

    union
    {
        uint32_t stride;
        uint32_t u32All;
    } ordinal9;

    union
    {
        uint32_t draw_initiator;
        uint32_t u32All;
    } ordinal10;
} PM4_PFP_DRAW_INDIRECT_MULTI;

constexpr unsigned int PM4_PFP_DRAW_INDIRECT_MULTI_SIZEDW__CORE = 10;

// ------------------------------- PFP_EVENT_WRITE_event_index_enum -------------------------------
enum PFP_EVENT_WRITE_event_index_enum
{
    event_index__pfp_event_write__other                           =  0,
    event_index__pfp_event_write__pixel_pipe_stat_control_or_dump =  1,
    event_index__pfp_event_write__sample_pipelinestat             =  2,
    event_index__pfp_event_write__cs_vs_ps_partial_flush          =  4,
};

// -------------------------------- PFP_EVENT_WRITE_counter_id_enum --------------------------------
enum PFP_EVENT_WRITE_counter_id_enum
{
    counter_id__pfp_event_write__pixel_pipe_occlusion_count_0    =  0,
    counter_id__pfp_event_write__pixel_pipe_occlusion_count_1    =  1,
    counter_id__pfp_event_write__pixel_pipe_occlusion_count_2    =  2,
    counter_id__pfp_event_write__pixel_pipe_occlusion_count_3    =  3,
    counter_id__pfp_event_write__pixel_pipe_screen_min_extents_0 =  4,
    counter_id__pfp_event_write__pixel_pipe_screen_max_extents_0 =  5,
    counter_id__pfp_event_write__pixel_pipe_screen_min_extents_1 =  6,
    counter_id__pfp_event_write__pixel_pipe_screen_max_extents_1 =  7,
};

// -------------------------------------- PM4_PFP_EVENT_WRITE --------------------------------------
typedef struct PM4_PFP_EVENT_WRITE
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                         event_type  :  6;
                uint32_t                         reserved1   :  2;
                PFP_EVENT_WRITE_event_index_enum event_index :  4;
                uint32_t                         reserved2   : 20;
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
                uint32_t                        reserved2       :  3;
                PFP_EVENT_WRITE_counter_id_enum counter_id      :  6;
                uint32_t                        stride          :  2;
                uint32_t                        instance_enable : 16;
                uint32_t                        reserved3       :  5;
            };
        } bitfieldsB;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t address_hi;
        uint32_t u32All;
    } ordinal4;
} PM4_PFP_EVENT_WRITE;

constexpr unsigned int PM4_PFP_EVENT_WRITE_SIZEDW__CORE = 4;

// ----------------------------------- PM4_PFP_EVENT_WRITE_ZPASS -----------------------------------
typedef struct PM4_PFP_EVENT_WRITE_ZPASS
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
} PM4_PFP_EVENT_WRITE_ZPASS;

constexpr unsigned int PM4_PFP_EVENT_WRITE_ZPASS_SIZEDW__CORE = 3;

// ------------------------------ PFP_EXECUTE_INDIRECT_temporal_enum ------------------------------
enum PFP_EXECUTE_INDIRECT_temporal_enum
{
    temporal__pfp_execute_indirect__rt =  0,
    temporal__pfp_execute_indirect__nt =  1,
    temporal__pfp_execute_indirect__ht =  2,
    temporal__pfp_execute_indirect__lu =  3,
};

// ----------------------------------- PM4_PFP_EXECUTE_INDIRECT -----------------------------------
typedef struct PM4_PFP_EXECUTE_INDIRECT
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1   :  2;
                uint32_t cmd_base_lo : 30;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t cmd_base_hi;
        uint32_t u32All;
    } ordinal3;

    union
    {
        union
        {
            struct
            {
                uint32_t                           ib_size               : 20;
                uint32_t                           chain                 :  1;
                uint32_t                           pre_ena               :  1;
                uint32_t                           reserved1             :  1;
                uint32_t                           count_indirect_enable :  1;
                uint32_t                           vmid                  :  4;
                PFP_EXECUTE_INDIRECT_temporal_enum temporal              :  2;
                uint32_t                           pre_resume            :  1;
                uint32_t                           reserved2             :  1;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t max_count;
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
        union
        {
            struct
            {
                uint32_t reserved1    :  2;
                uint32_t data_addr_lo : 30;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal9;

    union
    {
        union
        {
            struct
            {
                uint32_t data_addr_hi       : 16;
                uint32_t spill_table_stride : 16;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal10;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1           :  2;
                uint32_t spill_table_addr_lo : 30;
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
                uint32_t spill_table_addr_hi     : 16;
                uint32_t spill_table_reg_offset0 : 16;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal12;

    union
    {
        union
        {
            struct
            {
                uint32_t spill_table_reg_offset1 : 16;
                uint32_t spill_table_reg_offset2 : 16;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal13;

    union
    {
        union
        {
            struct
            {
                uint32_t spill_table_reg_offset3    : 16;
                uint32_t spill_table_instance_count : 16;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal14;

    union
    {
        union
        {
            struct
            {
                uint32_t vb_table_reg_offset : 16;
                uint32_t vb_table_size       : 16;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal15;
} PM4_PFP_EXECUTE_INDIRECT;

constexpr unsigned int PM4_PFP_EXECUTE_INDIRECT_SIZEDW__CORE = 15;

// ---------------------------- PFP_EXECUTE_INDIRECT_V2_operation_enum ----------------------------
enum PFP_EXECUTE_INDIRECT_V2_operation_enum
{
    operation__pfp_execute_indirect_v2__draw              =  0,
    operation__pfp_execute_indirect_v2__draw_index        =  1,
    operation__pfp_execute_indirect_v2__dispatch          =  2,
    operation__pfp_execute_indirect_v2__dispatch_mesh     =  3,
    operation__pfp_execute_indirect_v2__dispatch_rays     =  4,
    operation__pfp_execute_indirect_v2__dispatch_taskmesh =  5,
};

// ---------------------- PFP_EXECUTE_INDIRECT_V2_userdata_scatter_mode_enum ----------------------
enum PFP_EXECUTE_INDIRECT_V2_userdata_scatter_mode_enum
{
    userdata_scatter_mode__pfp_execute_indirect_v2__cs_only  =  0,
    userdata_scatter_mode__pfp_execute_indirect_v2__gs_only  =  0,
    userdata_scatter_mode__pfp_execute_indirect_v2__ps_only  =  0,
    userdata_scatter_mode__pfp_execute_indirect_v2__ps_gs    =  1,
    userdata_scatter_mode__pfp_execute_indirect_v2__ps_gs_hs =  2,
};

// ---------------------------------- PM4_PFP_EXECUTE_INDIRECT_V2 ----------------------------------
typedef struct PM4_PFP_EXECUTE_INDIRECT_V2
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                                           count_indirect_enable        :  1;
                uint32_t                                           userdata_dw_count            :  5;
                uint32_t                                           command_index_enable         :  1;
                uint32_t                                           userdata_gfx_register_enable :  1;
                uint32_t                                           num_spill_regs               :  2;
                uint32_t                                           init_mem_copy_count          :  3;
                uint32_t                                           build_srd_count              :  3;
                uint32_t                                           update_mem_copy_count        :  3;
                PFP_EXECUTE_INDIRECT_V2_operation_enum             operation                    :  3;
                uint32_t                                           fetch_index_attributes       :  1;
                PFP_EXECUTE_INDIRECT_V2_userdata_scatter_mode_enum userdata_scatter_mode        :  3;
                uint32_t                                           bottom_of_pipe_ts_after_draw :  1;
                uint32_t                                           reserved1                    :  2;
                uint32_t                                           vertex_offset_mode_enabled   :  1;
                uint32_t                                           vertex_bounds_check_enable   :  1;
                uint32_t                                           thread_trace_enable          :  1;
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
                uint32_t reserved1     :  2;
                uint32_t count_addr_lo : 30;
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
                uint32_t count_addr_hi : 16;
                uint32_t reserved1     : 16;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t max_count;
        uint32_t u32All;
    } ordinal5;

    union
    {
        uint32_t stride;
        uint32_t u32All;
    } ordinal6;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1    :  2;
                uint32_t data_addr_lo : 30;
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
                uint32_t data_addr_hi            : 16;
                uint32_t index_attributes_offset : 16;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal8;

    union
    {
        union
        {
            struct
            {
                uint32_t userdata_gfx_register :  8;
                uint32_t reserved1             :  8;
                uint32_t userdata_offset       : 16;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal9;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1           :  2;
                uint32_t spill_table_addr_lo : 30;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal10;

    union
    {
        union
        {
            struct
            {
                uint32_t spill_table_addr_hi : 16;
                uint32_t reserved1           : 16;
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
                uint32_t vb_table_size      : 16;
                uint32_t spill_table_stride : 16;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal12;

    union
    {
        union
        {
            struct
            {
                uint32_t spill_graphics_reg0 :  8;
                uint32_t spill_graphics_reg1 :  8;
                uint32_t spill_graphics_reg2 :  8;
                uint32_t reserved1           :  8;
            };
        } bitfieldsA;
        union
        {
            struct
            {
                uint32_t spill_compute_reg0 : 16;
                uint32_t spill_compute_reg1 : 16;
            };
        } bitfieldsB;
        uint32_t u32All;
    } ordinal13;
} PM4_PFP_EXECUTE_INDIRECT_V2;

constexpr unsigned int PM4_PFP_EXECUTE_INDIRECT_V2_SIZEDW__CORE = 13;

// ------------------------------- PM4_PFP_INDEX_ATTRIBUTES_INDIRECT -------------------------------
typedef struct PM4_PFP_INDEX_ATTRIBUTES_INDIRECT
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t indirect_mode     :  1;
                uint32_t reserved1         :  3;
                uint32_t attribute_base_lo : 28;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t attribute_base_hi;
        uint32_t addr_offset;
        uint32_t u32All;
    } ordinal3;

    union
    {
        union
        {
            struct
            {
                uint32_t attribute_index : 16;
                uint32_t reserved1       : 16;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal4;
} PM4_PFP_INDEX_ATTRIBUTES_INDIRECT;

constexpr unsigned int PM4_PFP_INDEX_ATTRIBUTES_INDIRECT_SIZEDW__CORE = 4;

// -------------------------------------- PM4_PFP_INDEX_BASE --------------------------------------
typedef struct PM4_PFP_INDEX_BASE
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1     :  1;
                uint32_t index_base_lo : 31;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t index_base_hi;
        uint32_t u32All;
    } ordinal3;
} PM4_PFP_INDEX_BASE;

constexpr unsigned int PM4_PFP_INDEX_BASE_SIZEDW__CORE = 3;

// ----------------------------------- PM4_PFP_INDEX_BUFFER_SIZE -----------------------------------
typedef struct PM4_PFP_INDEX_BUFFER_SIZE
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        uint32_t index_buffer_size;
        uint32_t u32All;
    } ordinal2;
} PM4_PFP_INDEX_BUFFER_SIZE;

constexpr unsigned int PM4_PFP_INDEX_BUFFER_SIZE_SIZEDW__CORE = 2;

// ------------------------------- PFP_INDIRECT_BUFFER_temporal_enum -------------------------------
enum PFP_INDIRECT_BUFFER_temporal_enum
{
    temporal__pfp_indirect_buffer__rt =  0,
    temporal__pfp_indirect_buffer__nt =  1,
    temporal__pfp_indirect_buffer__ht =  2,
    temporal__pfp_indirect_buffer__lu =  3,
};

// ------------------------------------ PM4_PFP_INDIRECT_BUFFER ------------------------------------
typedef struct PM4_PFP_INDIRECT_BUFFER
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
                uint32_t                          ib_size    : 20;
                uint32_t                          chain      :  1;
                uint32_t                          pre_ena    :  1;
                uint32_t                          reserved1  :  2;
                uint32_t                          vmid       :  4;
                PFP_INDIRECT_BUFFER_temporal_enum temporal   :  2;
                uint32_t                          pre_resume :  1;
                uint32_t                          reserved2  :  1;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal4;
} PM4_PFP_INDIRECT_BUFFER;

constexpr unsigned int PM4_PFP_INDIRECT_BUFFER_SIZEDW__CORE = 4;

// ---------------------------- PFP_INVALIDATE_TLBS_invalidate_sel_enum ----------------------------
enum PFP_INVALIDATE_TLBS_invalidate_sel_enum
{
    invalidate_sel__pfp_invalidate_tlbs__invalidate =  0,
};

// ------------------------- PFP_INVALIDATE_TLBS_mmhub_invalidate_sel_enum -------------------------
enum PFP_INVALIDATE_TLBS_mmhub_invalidate_sel_enum
{
    mmhub_invalidate_sel__pfp_invalidate_tlbs__do_not_invalidate_mmhub =  0,
    mmhub_invalidate_sel__pfp_invalidate_tlbs__use_mmhub_flush_type    =  1,
    mmhub_invalidate_sel__pfp_invalidate_tlbs__use_gfx_flush_type      =  2,
};

// ------------------------------------ PM4_PFP_INVALIDATE_TLBS ------------------------------------
typedef struct PM4_PFP_INVALIDATE_TLBS
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                PFP_INVALIDATE_TLBS_invalidate_sel_enum       invalidate_sel       :  3;
                PFP_INVALIDATE_TLBS_mmhub_invalidate_sel_enum mmhub_invalidate_sel :  2;
                uint32_t                                      reserved1            : 20;
                uint32_t                                      mmhub_flush_type     :  3;
                uint32_t                                      reserved2            :  1;
                uint32_t                                      gfx_flush_type       :  3;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;
} PM4_PFP_INVALIDATE_TLBS;

constexpr unsigned int PM4_PFP_INVALIDATE_TLBS_SIZEDW__CORE = 2;

// ------------------------------- PM4_PFP_LOAD_BUFFER_FILLED_SIZES -------------------------------
typedef struct PM4_PFP_LOAD_BUFFER_FILLED_SIZES
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1             :  3;
                uint32_t streamout_ctrl_buf_lo : 29;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t streamout_ctrl_buf_hi;
        uint32_t u32All;
    } ordinal3;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1            :  2;
                uint32_t streamout_target0_lo : 30;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t streamout_target0_hi;
        uint32_t u32All;
    } ordinal5;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1            :  2;
                uint32_t streamout_target1_lo : 30;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal6;

    union
    {
        uint32_t streamout_target1_hi;
        uint32_t u32All;
    } ordinal7;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1            :  2;
                uint32_t streamout_target2_lo : 30;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal8;

    union
    {
        uint32_t streamout_target2_hi;
        uint32_t u32All;
    } ordinal9;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1            :  2;
                uint32_t streamout_target3_lo : 30;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal10;

    union
    {
        uint32_t streamout_target3_hi;
        uint32_t u32All;
    } ordinal11;
} PM4_PFP_LOAD_BUFFER_FILLED_SIZES;

constexpr unsigned int PM4_PFP_LOAD_BUFFER_FILLED_SIZES_SIZEDW__CORE = 11;

// ------------------------------------ PM4_PFP_LOAD_CONFIG_REG ------------------------------------
typedef struct PM4_PFP_LOAD_CONFIG_REG
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
} PM4_PFP_LOAD_CONFIG_REG;

constexpr unsigned int PM4_PFP_LOAD_CONFIG_REG_SIZEDW__CORE = 5;

// ----------------------------------- PM4_PFP_LOAD_CONTEXT_REG -----------------------------------
typedef struct PM4_PFP_LOAD_CONTEXT_REG
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
} PM4_PFP_LOAD_CONTEXT_REG;

constexpr unsigned int PM4_PFP_LOAD_CONTEXT_REG_SIZEDW__CORE = 5;

// ----------------------------- PFP_LOAD_CONTEXT_REG_INDEX_index_enum -----------------------------
enum PFP_LOAD_CONTEXT_REG_INDEX_index_enum
{
    index__pfp_load_context_reg_index__direct_addr =  0,
    index__pfp_load_context_reg_index__offset      =  1,
};

// -------------------------- PFP_LOAD_CONTEXT_REG_INDEX_data_format_enum --------------------------
enum PFP_LOAD_CONTEXT_REG_INDEX_data_format_enum
{
    data_format__pfp_load_context_reg_index__offset_and_size =  0,
    data_format__pfp_load_context_reg_index__offset_and_data =  1,
};

// -------------------------------- PM4_PFP_LOAD_CONTEXT_REG_INDEX --------------------------------
typedef struct PM4_PFP_LOAD_CONTEXT_REG_INDEX
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                PFP_LOAD_CONTEXT_REG_INDEX_index_enum index       :  1;
                uint32_t                              reserved1   :  1;
                uint32_t                              mem_addr_lo : 30;
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
                uint32_t                                    reg_offset  : 16;
                uint32_t                                    reserved1   : 15;
                PFP_LOAD_CONTEXT_REG_INDEX_data_format_enum data_format :  1;
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
} PM4_PFP_LOAD_CONTEXT_REG_INDEX;

constexpr unsigned int PM4_PFP_LOAD_CONTEXT_REG_INDEX_SIZEDW__CORE = 5;

// -------------------------------------- PM4_PFP_LOAD_SH_REG --------------------------------------
typedef struct PM4_PFP_LOAD_SH_REG
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
} PM4_PFP_LOAD_SH_REG;

constexpr unsigned int PM4_PFP_LOAD_SH_REG_SIZEDW__CORE = 5;

// ------------------------------- PFP_LOAD_SH_REG_INDEX_index_enum -------------------------------
enum PFP_LOAD_SH_REG_INDEX_index_enum
{
    index__pfp_load_sh_reg_index__direct_addr   =  0,
    index__pfp_load_sh_reg_index__offset        =  1,
    index__pfp_load_sh_reg_index__indirect_addr =  2,
};

// ---------------------------- PFP_LOAD_SH_REG_INDEX_data_format_enum ----------------------------
enum PFP_LOAD_SH_REG_INDEX_data_format_enum
{
    data_format__pfp_load_sh_reg_index__offset_and_size =  0,
    data_format__pfp_load_sh_reg_index__offset_and_data =  1,
};

// ----------------------------------- PM4_PFP_LOAD_SH_REG_INDEX -----------------------------------
typedef struct PM4_PFP_LOAD_SH_REG_INDEX
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                PFP_LOAD_SH_REG_INDEX_index_enum index       :  2;
                uint32_t                         mem_addr_lo : 30;
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
                uint32_t                               reg_offset  : 16;
                uint32_t                               reserved1   : 15;
                PFP_LOAD_SH_REG_INDEX_data_format_enum data_format :  1;
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
} PM4_PFP_LOAD_SH_REG_INDEX;

constexpr unsigned int PM4_PFP_LOAD_SH_REG_INDEX_SIZEDW__CORE = 5;

// ----------------------------------- PM4_PFP_LOAD_UCONFIG_REG -----------------------------------
typedef struct PM4_PFP_LOAD_UCONFIG_REG
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
} PM4_PFP_LOAD_UCONFIG_REG;

constexpr unsigned int PM4_PFP_LOAD_UCONFIG_REG_SIZEDW__CORE = 5;

// ----------------------------- PFP_LOAD_UCONFIG_REG_INDEX_index_enum -----------------------------
enum PFP_LOAD_UCONFIG_REG_INDEX_index_enum
{
    index__pfp_load_uconfig_reg_index__direct_addr =  0,
    index__pfp_load_uconfig_reg_index__offset      =  1,
};

// -------------------------- PFP_LOAD_UCONFIG_REG_INDEX_data_format_enum --------------------------
enum PFP_LOAD_UCONFIG_REG_INDEX_data_format_enum
{
    data_format__pfp_load_uconfig_reg_index__offset_and_size =  0,
    data_format__pfp_load_uconfig_reg_index__offset_and_data =  1,
};

// -------------------------------- PM4_PFP_LOAD_UCONFIG_REG_INDEX --------------------------------
typedef struct PM4_PFP_LOAD_UCONFIG_REG_INDEX
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                PFP_LOAD_UCONFIG_REG_INDEX_index_enum index       :  1;
                uint32_t                              reserved1   :  1;
                uint32_t                              mem_addr_lo : 30;
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
                uint32_t                                    reg_offset  : 16;
                uint32_t                                    reserved1   : 15;
                PFP_LOAD_UCONFIG_REG_INDEX_data_format_enum data_format :  1;
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
} PM4_PFP_LOAD_UCONFIG_REG_INDEX;

constexpr unsigned int PM4_PFP_LOAD_UCONFIG_REG_INDEX_SIZEDW__CORE = 5;

// ------------------------------------------ PM4_PFP_NOP ------------------------------------------
typedef struct PM4_PFP_NOP
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;
} PM4_PFP_NOP;

constexpr unsigned int PM4_PFP_NOP_SIZEDW__CORE = 1;

// ------------------------------------- PM4_PFP_NUM_INSTANCES -------------------------------------
typedef struct PM4_PFP_NUM_INSTANCES
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        uint32_t num_instances;
        uint32_t u32All;
    } ordinal2;
} PM4_PFP_NUM_INSTANCES;

constexpr unsigned int PM4_PFP_NUM_INSTANCES_SIZEDW__CORE = 2;

// ------------------------------------ PM4_PFP_OCCLUSION_QUERY ------------------------------------
typedef struct PM4_PFP_OCCLUSION_QUERY
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1     :  4;
                uint32_t start_addr_lo : 28;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t start_addr_hi;
        uint32_t u32All;
    } ordinal3;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1     :  2;
                uint32_t query_addr_lo : 30;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t query_addr_hi;
        uint32_t u32All;
    } ordinal5;
} PM4_PFP_OCCLUSION_QUERY;

constexpr unsigned int PM4_PFP_OCCLUSION_QUERY_SIZEDW__CORE = 5;

// -------------------------------- PFP_PERF_COUNTER_WINDOW_op_enum --------------------------------
enum PFP_PERF_COUNTER_WINDOW_op_enum
{
    op__pfp_perf_counter_window__stop_window  =  0,
    op__pfp_perf_counter_window__start_window =  7,
};

// ---------------------------------- PM4_PFP_PERF_COUNTER_WINDOW ----------------------------------
typedef struct PM4_PFP_PERF_COUNTER_WINDOW
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                PFP_PERF_COUNTER_WINDOW_op_enum op         :  3;
                uint32_t                        reserved1  : 29;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;
} PM4_PFP_PERF_COUNTER_WINDOW;

constexpr unsigned int PM4_PFP_PERF_COUNTER_WINDOW_SIZEDW__CORE = 2;

// -------------------------------------- PM4_PFP_PFP_SYNC_ME --------------------------------------
typedef struct PM4_PFP_PFP_SYNC_ME
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        uint32_t dummy_data;
        uint32_t u32All;
    } ordinal2;
} PM4_PFP_PFP_SYNC_ME;

constexpr unsigned int PM4_PFP_PFP_SYNC_ME_SIZEDW__CORE = 2;

// -------------------------------- PFP_PREAMBLE_CNTL_command_enum --------------------------------
enum PFP_PREAMBLE_CNTL_command_enum
{
    command__pfp_preamble_cntl__user_queues_state_save    =  4,
    command__pfp_preamble_cntl__user_queues_state_restore =  5,
};

// ------------------------------------- PM4_PFP_PREAMBLE_CNTL -------------------------------------
typedef struct PM4_PFP_PREAMBLE_CNTL
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                       reserved1  : 28;
                PFP_PREAMBLE_CNTL_command_enum command    :  4;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;
} PM4_PFP_PREAMBLE_CNTL;

constexpr unsigned int PM4_PFP_PREAMBLE_CNTL_SIZEDW__CORE = 2;

// --------------------------------------- PM4_PFP_PRED_EXEC ---------------------------------------
typedef struct PM4_PFP_PRED_EXEC
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t exec_count    : 14;
                uint32_t reserved1     : 10;
                uint32_t device_select :  8;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;
} PM4_PFP_PRED_EXEC;

constexpr unsigned int PM4_PFP_PRED_EXEC_SIZEDW__CORE = 2;

// -------------------------------- PFP_PRIME_UTCL2_prime_mode_enum --------------------------------
enum PFP_PRIME_UTCL2_prime_mode_enum
{
    prime_mode__pfp_prime_utcl2__dont_wait_for_xack =  0,
    prime_mode__pfp_prime_utcl2__wait_for_xack      =  1,
};

// -------------------------------- PFP_PRIME_UTCL2_engine_sel_enum --------------------------------
enum PFP_PRIME_UTCL2_engine_sel_enum
{
    engine_sel__pfp_prime_utcl2__prefetch_parser =  1,
};

// -------------------------------------- PM4_PFP_PRIME_UTCL2 --------------------------------------
typedef struct PM4_PFP_PRIME_UTCL2
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                        cache_perm :  3;
                PFP_PRIME_UTCL2_prime_mode_enum prime_mode :  1;
                uint32_t                        reserved1  : 26;
                PFP_PRIME_UTCL2_engine_sel_enum engine_sel :  2;
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
} PM4_PFP_PRIME_UTCL2;

constexpr unsigned int PM4_PFP_PRIME_UTCL2_SIZEDW__CORE = 5;

// ------------------------------- PFP_REG_RMW_shadow_base_sel_enum -------------------------------
enum PFP_REG_RMW_shadow_base_sel_enum
{
    shadow_base_sel__pfp_reg_rmw__no_shadow             =  0,
    shadow_base_sel__pfp_reg_rmw__shadow_global_uconfig =  1,
};

// --------------------------------- PFP_REG_RMW_or_mask_src_enum ---------------------------------
enum PFP_REG_RMW_or_mask_src_enum
{
    or_mask_src__pfp_reg_rmw__immediate   =  0,
    or_mask_src__pfp_reg_rmw__reg_or_addr =  1,
};

// --------------------------------- PFP_REG_RMW_and_mask_src_enum ---------------------------------
enum PFP_REG_RMW_and_mask_src_enum
{
    and_mask_src__pfp_reg_rmw__immediate    =  0,
    and_mask_src__pfp_reg_rmw__reg_and_addr =  1,
};

// ---------------------------------------- PM4_PFP_REG_RMW ----------------------------------------
typedef struct PM4_PFP_REG_RMW
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                         mod_addr        : 18;
                uint32_t                         reserved1       :  6;
                PFP_REG_RMW_shadow_base_sel_enum shadow_base_sel :  2;
                uint32_t                         reserved2       :  4;
                PFP_REG_RMW_or_mask_src_enum     or_mask_src     :  1;
                PFP_REG_RMW_and_mask_src_enum    and_mask_src    :  1;
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
} PM4_PFP_REG_RMW;

constexpr unsigned int PM4_PFP_REG_RMW_SIZEDW__CORE = 4;

// ------------------------------- PFP_RELEASE_MEM_event_index_enum -------------------------------
enum PFP_RELEASE_MEM_event_index_enum
{
    event_index__pfp_release_mem__end_of_pipe =  5,
    event_index__pfp_release_mem__shader_done =  6,
};

// --------------------------------- PFP_RELEASE_MEM_temporal_enum ---------------------------------
enum PFP_RELEASE_MEM_temporal_enum
{
    temporal__pfp_release_mem__rt =  0,
    temporal__pfp_release_mem__nt =  1,
    temporal__pfp_release_mem__ht =  2,
    temporal__pfp_release_mem__lu =  3,
};

// --------------------------------- PFP_RELEASE_MEM_dst_sel_enum ---------------------------------
enum PFP_RELEASE_MEM_dst_sel_enum
{
    dst_sel__pfp_release_mem__memory_controller                 =  0,
    dst_sel__pfp_release_mem__tc_l2                             =  1,
    dst_sel__pfp_release_mem__queue_write_pointer_register      =  2,
    dst_sel__pfp_release_mem__queue_write_pointer_poll_mask_bit =  3,
};

// ------------------------------ PFP_RELEASE_MEM_mes_action_id_enum ------------------------------
enum PFP_RELEASE_MEM_mes_action_id_enum
{
    mes_action_id__pfp_release_mem__no_mes_notification                     =  0,
    mes_action_id__pfp_release_mem__interrupt_and_fence                     =  1,
    mes_action_id__pfp_release_mem__interrupt_no_fence_then_address_payload =  2,
    mes_action_id__pfp_release_mem__interrupt_and_address_payload           =  3,
};

// --------------------------------- PFP_RELEASE_MEM_int_sel_enum ---------------------------------
enum PFP_RELEASE_MEM_int_sel_enum
{
    int_sel__pfp_release_mem__none                                                 =  0,
    int_sel__pfp_release_mem__send_interrupt_only                                  =  1,
    int_sel__pfp_release_mem__send_interrupt_after_write_confirm                   =  2,
    int_sel__pfp_release_mem__send_data_and_write_confirm                          =  3,
    int_sel__pfp_release_mem__unconditionally_send_int_ctxid                       =  4,
    int_sel__pfp_release_mem__conditionally_send_int_ctxid_based_on_32_bit_compare =  5,
    int_sel__pfp_release_mem__conditionally_send_int_ctxid_based_on_64_bit_compare =  6,
};

// --------------------------------- PFP_RELEASE_MEM_data_sel_enum ---------------------------------
enum PFP_RELEASE_MEM_data_sel_enum
{
    data_sel__pfp_release_mem__none                      =  0,
    data_sel__pfp_release_mem__send_32_bit_low           =  1,
    data_sel__pfp_release_mem__send_64_bit_data          =  2,
    data_sel__pfp_release_mem__send_gpu_clock_counter    =  3,
    data_sel__pfp_release_mem__send_system_clock_counter =  4,
};

// -------------------------------------- PM4_PFP_RELEASE_MEM --------------------------------------
typedef struct PM4_PFP_RELEASE_MEM
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                         event_type  :  6;
                uint32_t                         reserved1   :  1;
                uint32_t                         wait_sync   :  1;
                PFP_RELEASE_MEM_event_index_enum event_index :  4;
                uint32_t                         gcr_cntl    : 13;
                PFP_RELEASE_MEM_temporal_enum    temporal    :  2;
                uint32_t                         reserved2   :  1;
                uint32_t                         execute     :  2;
                uint32_t                         glk_inv     :  1;
                uint32_t                         pws_enable  :  1;
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
                uint32_t                           reserved1     : 16;
                PFP_RELEASE_MEM_dst_sel_enum       dst_sel       :  2;
                uint32_t                           reserved2     :  2;
                uint32_t                           mes_intr_pipe :  2;
                PFP_RELEASE_MEM_mes_action_id_enum mes_action_id :  2;
                PFP_RELEASE_MEM_int_sel_enum       int_sel       :  3;
                uint32_t                           reserved3     :  2;
                PFP_RELEASE_MEM_data_sel_enum      data_sel      :  3;
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
} PM4_PFP_RELEASE_MEM;

constexpr unsigned int PM4_PFP_RELEASE_MEM_SIZEDW__CORE = 8;

// ---------------------------------------- PM4_PFP_REWIND ----------------------------------------
typedef struct PM4_PFP_REWIND
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1  : 31;
                uint32_t valid      :  1;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;
} PM4_PFP_REWIND;

constexpr unsigned int PM4_PFP_REWIND_SIZEDW__CORE = 2;

// ------------------------------- PM4_PFP_SAVE_BUFFER_FILLED_SIZES -------------------------------
typedef struct PM4_PFP_SAVE_BUFFER_FILLED_SIZES
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1             :  3;
                uint32_t streamout_ctrl_buf_lo : 29;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t streamout_ctrl_buf_hi;
        uint32_t u32All;
    } ordinal3;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1            :  2;
                uint32_t streamout_target0_lo : 30;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t streamout_target0_hi;
        uint32_t u32All;
    } ordinal5;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1            :  2;
                uint32_t streamout_target1_lo : 30;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal6;

    union
    {
        uint32_t streamout_target1_hi;
        uint32_t u32All;
    } ordinal7;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1            :  2;
                uint32_t streamout_target2_lo : 30;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal8;

    union
    {
        uint32_t streamout_target2_hi;
        uint32_t u32All;
    } ordinal9;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1            :  2;
                uint32_t streamout_target3_lo : 30;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal10;

    union
    {
        uint32_t streamout_target3_hi;
        uint32_t u32All;
    } ordinal11;
} PM4_PFP_SAVE_BUFFER_FILLED_SIZES;

constexpr unsigned int PM4_PFP_SAVE_BUFFER_FILLED_SIZES_SIZEDW__CORE = 11;

// --------------------------------- PFP_SET_BASE_base_index_enum ---------------------------------
enum PFP_SET_BASE_base_index_enum
{
    base_index__pfp_set_base__display_list_patch_table_base =  0,
    base_index__pfp_set_base__patch_table_base              =  1,
    base_index__pfp_set_base__load_reg_index_base           =  4,
    base_index__pfp_set_base__indirect_data_base            =  5,
    base_index__pfp_set_base__execute_indirect_v2           =  6,
};

// --------------------------------------- PM4_PFP_SET_BASE ---------------------------------------
typedef struct PM4_PFP_SET_BASE
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                PFP_SET_BASE_base_index_enum base_index :  4;
                uint32_t                     reserved1  : 28;
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
        } bitfields;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t address_hi;
        uint32_t u32All;
    } ordinal4;
} PM4_PFP_SET_BASE;

constexpr unsigned int PM4_PFP_SET_BASE_SIZEDW__CORE = 4;

// -------------------------------- PM4_PFP_SET_BUFFER_FILLED_SIZE --------------------------------
typedef struct PM4_PFP_SET_BUFFER_FILLED_SIZE
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1             :  3;
                uint32_t streamout_ctrl_buf_lo : 29;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t streamout_ctrl_buf_hi;
        uint32_t u32All;
    } ordinal3;

    union
    {
        union
        {
            struct
            {
                uint32_t buffer_id  :  2;
                uint32_t reserved1  : 30;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t buffer_offset;
        uint32_t u32All;
    } ordinal5;
} PM4_PFP_SET_BUFFER_FILLED_SIZE;

constexpr unsigned int PM4_PFP_SET_BUFFER_FILLED_SIZE_SIZEDW__CORE = 5;

// ------------------------------------ PM4_PFP_SET_CONTEXT_REG ------------------------------------
typedef struct PM4_PFP_SET_CONTEXT_REG
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
} PM4_PFP_SET_CONTEXT_REG;

constexpr unsigned int PM4_PFP_SET_CONTEXT_REG_SIZEDW__CORE = 2;

// --------------------------------- PM4_PFP_SET_CONTEXT_REG_PAIRS ---------------------------------
typedef struct PM4_PFP_SET_CONTEXT_REG_PAIRS
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
        uint32_t reg_data;
        uint32_t u32All;
    } ordinal3;
} PM4_PFP_SET_CONTEXT_REG_PAIRS;

constexpr unsigned int PM4_PFP_SET_CONTEXT_REG_PAIRS_SIZEDW__CORE = 3;

// ------------------------------ PFP_SET_PREDICATION_pred_bool_enum ------------------------------
enum PFP_SET_PREDICATION_pred_bool_enum
{
    pred_bool__pfp_set_predication__draw_if_not_visible_or_overflow =  0,
    pred_bool__pfp_set_predication__draw_if_visible_or_no_overflow  =  1,
};

// --------------------------------- PFP_SET_PREDICATION_hint_enum ---------------------------------
enum PFP_SET_PREDICATION_hint_enum
{
    hint__pfp_set_predication__wait_until_final_zpass_written  =  0,
    hint__pfp_set_predication__draw_if_not_final_zpass_written =  1,
};

// ------------------------------- PFP_SET_PREDICATION_pred_op_enum -------------------------------
enum PFP_SET_PREDICATION_pred_op_enum
{
    pred_op__pfp_set_predication__clear_predicate         =  0,
    pred_op__pfp_set_predication__set_zpass_predicate     =  1,
    pred_op__pfp_set_predication__set_primcount_predicate =  2,
    pred_op__pfp_set_predication__DX12                    =  3,
    pred_op__pfp_set_predication__Vulkan                  =  4,
};

// ----------------------------- PFP_SET_PREDICATION_continue_bit_enum -----------------------------
enum PFP_SET_PREDICATION_continue_bit_enum
{
    continue_bit__pfp_set_predication__new_set_predication      =  0,
    continue_bit__pfp_set_predication__continue_set_predication =  1,
};

// ------------------------------------ PM4_PFP_SET_PREDICATION ------------------------------------
typedef struct PM4_PFP_SET_PREDICATION
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                              reserved1    :  8;
                PFP_SET_PREDICATION_pred_bool_enum    pred_bool    :  1;
                uint32_t                              reserved2    :  3;
                PFP_SET_PREDICATION_hint_enum         hint         :  1;
                uint32_t                              reserved3    :  3;
                PFP_SET_PREDICATION_pred_op_enum      pred_op      :  3;
                uint32_t                              reserved4    : 12;
                PFP_SET_PREDICATION_continue_bit_enum continue_bit :  1;
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
                uint32_t reserved1     :  4;
                uint32_t start_addr_lo : 28;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t start_addr_hi;
        uint32_t u32All;
    } ordinal4;
} PM4_PFP_SET_PREDICATION;

constexpr unsigned int PM4_PFP_SET_PREDICATION_SIZEDW__CORE = 4;

// -------------------------------------- PM4_PFP_SET_SH_REG --------------------------------------
typedef struct PM4_PFP_SET_SH_REG
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
} PM4_PFP_SET_SH_REG;

constexpr unsigned int PM4_PFP_SET_SH_REG_SIZEDW__CORE = 2;

// -------------------------------- PFP_SET_SH_REG_INDEX_index_enum --------------------------------
enum PFP_SET_SH_REG_INDEX_index_enum
{
    index__pfp_set_sh_reg_index__compute_dispatch_interleave_shadow =  2,
    index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask              =  3,
};

// ----------------------------------- PM4_PFP_SET_SH_REG_INDEX -----------------------------------
typedef struct PM4_PFP_SET_SH_REG_INDEX
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
                PFP_SET_SH_REG_INDEX_index_enum index      :  4;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;
} PM4_PFP_SET_SH_REG_INDEX;

constexpr unsigned int PM4_PFP_SET_SH_REG_INDEX_SIZEDW__CORE = 2;

// ------------------------------- PFP_SET_SH_REG_OFFSET_index_enum -------------------------------
enum PFP_SET_SH_REG_OFFSET_index_enum
{
    index__pfp_set_sh_reg_offset__normal_operation       =  0,
    index__pfp_set_sh_reg_offset__data_indirect_2dw_256b =  1,
    index__pfp_set_sh_reg_offset__data_indirect_1dw      =  2,
};

// ----------------------------------- PM4_PFP_SET_SH_REG_OFFSET -----------------------------------
typedef struct PM4_PFP_SET_SH_REG_OFFSET
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                         reg_offset : 16;
                uint32_t                         reserved1  : 14;
                PFP_SET_SH_REG_OFFSET_index_enum index      :  2;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t ib_offset;
        uint32_t data_offset;
        uint32_t u32All;
    } ordinal3;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1   : 16;
                uint32_t driver_data : 16;
            };
        } bitfieldsA;
        uint32_t dummy;
        uint32_t u32All;
    } ordinal4;
} PM4_PFP_SET_SH_REG_OFFSET;

constexpr unsigned int PM4_PFP_SET_SH_REG_OFFSET_SIZEDW__CORE = 4;

// ----------------------------------- PM4_PFP_SET_SH_REG_PAIRS -----------------------------------
typedef struct PM4_PFP_SET_SH_REG_PAIRS
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
        uint32_t reg_data;
        uint32_t u32All;
    } ordinal3;
} PM4_PFP_SET_SH_REG_PAIRS;

constexpr unsigned int PM4_PFP_SET_SH_REG_PAIRS_SIZEDW__CORE = 3;

// ------------------------------------ PM4_PFP_SET_UCONFIG_REG ------------------------------------
typedef struct PM4_PFP_SET_UCONFIG_REG
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
} PM4_PFP_SET_UCONFIG_REG;

constexpr unsigned int PM4_PFP_SET_UCONFIG_REG_SIZEDW__CORE = 2;

// ----------------------------- PFP_SET_UCONFIG_REG_INDEX_index_enum -----------------------------
enum PFP_SET_UCONFIG_REG_INDEX_index_enum
{
    index__pfp_set_uconfig_reg_index__default       =  0,
    index__pfp_set_uconfig_reg_index__index_type    =  2,
    index__pfp_set_uconfig_reg_index__num_instances =  3,
};

// --------------------------------- PM4_PFP_SET_UCONFIG_REG_INDEX ---------------------------------
typedef struct PM4_PFP_SET_UCONFIG_REG_INDEX
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                             reg_offset : 16;
                uint32_t                             reserved1  : 12;
                PFP_SET_UCONFIG_REG_INDEX_index_enum index      :  4;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;
} PM4_PFP_SET_UCONFIG_REG_INDEX;

constexpr unsigned int PM4_PFP_SET_UCONFIG_REG_INDEX_SIZEDW__CORE = 2;

// --------------------------------- PM4_PFP_SET_UCONFIG_REG_PAIRS ---------------------------------
typedef struct PM4_PFP_SET_UCONFIG_REG_PAIRS
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
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
        uint32_t reg_data;
        uint32_t u32All;
    } ordinal3;
} PM4_PFP_SET_UCONFIG_REG_PAIRS;

constexpr unsigned int PM4_PFP_SET_UCONFIG_REG_PAIRS_SIZEDW__CORE = 3;

// ----------------------------- PM4_PFP_UPDATE_DB_SUMMARIZER_TIMEOUTS -----------------------------
typedef struct PM4_PFP_UPDATE_DB_SUMMARIZER_TIMEOUTS
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        uint32_t reg_value;
        uint32_t u32All;
    } ordinal2;
} PM4_PFP_UPDATE_DB_SUMMARIZER_TIMEOUTS;

constexpr unsigned int PM4_PFP_UPDATE_DB_SUMMARIZER_TIMEOUTS_SIZEDW__CORE = 2;

// --------------------------------- PM4_PFP_STREAMOUT_STATS_QUERY ---------------------------------
typedef struct PM4_PFP_STREAMOUT_STATS_QUERY
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1             :  3;
                uint32_t streamout_ctrl_buf_lo : 29;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t streamout_ctrl_buf_hi;
        uint32_t u32All;
    } ordinal3;

    union
    {
        union
        {
            struct
            {
                uint32_t stream_index :  2;
                uint32_t reserved1    : 30;
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
                uint32_t reserved1        :  3;
                uint32_t streamout_dst_lo : 29;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal5;

    union
    {
        uint32_t streamout_dst_hi;
        uint32_t u32All;
    } ordinal6;
} PM4_PFP_STREAMOUT_STATS_QUERY;

constexpr unsigned int PM4_PFP_STREAMOUT_STATS_QUERY_SIZEDW__CORE = 6;

// -------------------------------- PFP_WAIT_REG_MEM_function_enum --------------------------------
enum PFP_WAIT_REG_MEM_function_enum
{
    function__pfp_wait_reg_mem__always_pass                           =  0,
    function__pfp_wait_reg_mem__less_than_ref_value                   =  1,
    function__pfp_wait_reg_mem__less_than_equal_to_the_ref_value      =  2,
    function__pfp_wait_reg_mem__equal_to_the_reference_value          =  3,
    function__pfp_wait_reg_mem__not_equal_reference_value             =  4,
    function__pfp_wait_reg_mem__greater_than_or_equal_reference_value =  5,
    function__pfp_wait_reg_mem__greater_than_reference_value          =  6,
};

// -------------------------------- PFP_WAIT_REG_MEM_mem_space_enum --------------------------------
enum PFP_WAIT_REG_MEM_mem_space_enum
{
    mem_space__pfp_wait_reg_mem__register_space =  0,
    mem_space__pfp_wait_reg_mem__memory_space   =  1,
};

// -------------------------------- PFP_WAIT_REG_MEM_operation_enum --------------------------------
enum PFP_WAIT_REG_MEM_operation_enum
{
    operation__pfp_wait_reg_mem__wait_reg_mem         =  0,
    operation__pfp_wait_reg_mem__wr_wait_wr_reg       =  1,
    operation__pfp_wait_reg_mem__wait_mem_preemptable =  3,
};

// ------------------------------- PFP_WAIT_REG_MEM_engine_sel_enum -------------------------------
enum PFP_WAIT_REG_MEM_engine_sel_enum
{
    engine_sel__pfp_wait_reg_mem__micro_engine    =  0,
    engine_sel__pfp_wait_reg_mem__prefetch_parser =  1,
};

// -------------------------------- PFP_WAIT_REG_MEM_temporal_enum --------------------------------
enum PFP_WAIT_REG_MEM_temporal_enum
{
    temporal__pfp_wait_reg_mem__rt =  0,
    temporal__pfp_wait_reg_mem__nt =  1,
    temporal__pfp_wait_reg_mem__ht =  2,
    temporal__pfp_wait_reg_mem__lu =  3,
};

// ------------------------------------- PM4_PFP_WAIT_REG_MEM -------------------------------------
typedef struct PM4_PFP_WAIT_REG_MEM
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                PFP_WAIT_REG_MEM_function_enum   function      :  3;
                uint32_t                         reserved1     :  1;
                PFP_WAIT_REG_MEM_mem_space_enum  mem_space     :  2;
                PFP_WAIT_REG_MEM_operation_enum  operation     :  2;
                PFP_WAIT_REG_MEM_engine_sel_enum engine_sel    :  2;
                uint32_t                         reserved2     : 12;
                uint32_t                         mes_intr_pipe :  2;
                uint32_t                         mes_action    :  1;
                PFP_WAIT_REG_MEM_temporal_enum   temporal      :  2;
                uint32_t                         reserved3     :  5;
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
} PM4_PFP_WAIT_REG_MEM;

constexpr unsigned int PM4_PFP_WAIT_REG_MEM_SIZEDW__CORE = 7;

// ------------------------------- PFP_WAIT_REG_MEM64_function_enum -------------------------------
enum PFP_WAIT_REG_MEM64_function_enum
{
    function__pfp_wait_reg_mem64__always_pass                           =  0,
    function__pfp_wait_reg_mem64__less_than_ref_value                   =  1,
    function__pfp_wait_reg_mem64__less_than_equal_to_the_ref_value      =  2,
    function__pfp_wait_reg_mem64__equal_to_the_reference_value          =  3,
    function__pfp_wait_reg_mem64__not_equal_reference_value             =  4,
    function__pfp_wait_reg_mem64__greater_than_or_equal_reference_value =  5,
    function__pfp_wait_reg_mem64__greater_than_reference_value          =  6,
};

// ------------------------------- PFP_WAIT_REG_MEM64_mem_space_enum -------------------------------
enum PFP_WAIT_REG_MEM64_mem_space_enum
{
    mem_space__pfp_wait_reg_mem64__register_space =  0,
    mem_space__pfp_wait_reg_mem64__memory_space   =  1,
};

// ------------------------------- PFP_WAIT_REG_MEM64_operation_enum -------------------------------
enum PFP_WAIT_REG_MEM64_operation_enum
{
    operation__pfp_wait_reg_mem64__wait_reg_mem         =  0,
    operation__pfp_wait_reg_mem64__wr_wait_wr_reg       =  1,
    operation__pfp_wait_reg_mem64__wait_mem_preemptable =  3,
};

// ------------------------------ PFP_WAIT_REG_MEM64_engine_sel_enum ------------------------------
enum PFP_WAIT_REG_MEM64_engine_sel_enum
{
    engine_sel__pfp_wait_reg_mem64__micro_engine    =  0,
    engine_sel__pfp_wait_reg_mem64__prefetch_parser =  1,
};

// ------------------------------- PFP_WAIT_REG_MEM64_temporal_enum -------------------------------
enum PFP_WAIT_REG_MEM64_temporal_enum
{
    temporal__pfp_wait_reg_mem64__rt =  0,
    temporal__pfp_wait_reg_mem64__nt =  1,
    temporal__pfp_wait_reg_mem64__ht =  2,
    temporal__pfp_wait_reg_mem64__lu =  3,
};

// ------------------------------------ PM4_PFP_WAIT_REG_MEM64 ------------------------------------
typedef struct PM4_PFP_WAIT_REG_MEM64
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                PFP_WAIT_REG_MEM64_function_enum   function      :  3;
                uint32_t                           reserved1     :  1;
                PFP_WAIT_REG_MEM64_mem_space_enum  mem_space     :  2;
                PFP_WAIT_REG_MEM64_operation_enum  operation     :  2;
                PFP_WAIT_REG_MEM64_engine_sel_enum engine_sel    :  2;
                uint32_t                           reserved2     : 12;
                uint32_t                           mes_intr_pipe :  2;
                uint32_t                           mes_action    :  1;
                PFP_WAIT_REG_MEM64_temporal_enum   temporal      :  2;
                uint32_t                           reserved3     :  5;
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
} PM4_PFP_WAIT_REG_MEM64;

constexpr unsigned int PM4_PFP_WAIT_REG_MEM64_SIZEDW__CORE = 9;

// ---------------------------------- PFP_WRITE_DATA_dst_sel_enum ----------------------------------
enum PFP_WRITE_DATA_dst_sel_enum
{
    dst_sel__pfp_write_data__mem_mapped_register =  0,
    dst_sel__pfp_write_data__tc_l2               =  2,
    dst_sel__pfp_write_data__memory              =  5,
};

// --------------------------------- PFP_WRITE_DATA_addr_incr_enum ---------------------------------
enum PFP_WRITE_DATA_addr_incr_enum
{
    addr_incr__pfp_write_data__increment_address        =  0,
    addr_incr__pfp_write_data__do_not_increment_address =  1,
};

// -------------------------------- PFP_WRITE_DATA_wr_confirm_enum --------------------------------
enum PFP_WRITE_DATA_wr_confirm_enum
{
    wr_confirm__pfp_write_data__do_not_wait_for_write_confirmation =  0,
    wr_confirm__pfp_write_data__wait_for_write_confirmation        =  1,
};

// ----------------------------------- PFP_WRITE_DATA_mode_enum -----------------------------------
enum PFP_WRITE_DATA_mode_enum
{
    mode__pfp_write_data__PF_VF_disabled =  0,
    mode__pfp_write_data__PF_VF_enabled  =  1,
};

// --------------------------------- PFP_WRITE_DATA_temporal_enum ---------------------------------
enum PFP_WRITE_DATA_temporal_enum
{
    temporal__pfp_write_data__rt =  0,
    temporal__pfp_write_data__nt =  1,
    temporal__pfp_write_data__ht =  2,
    temporal__pfp_write_data__lu =  3,
};

// -------------------------------- PFP_WRITE_DATA_engine_sel_enum --------------------------------
enum PFP_WRITE_DATA_engine_sel_enum
{
    engine_sel__pfp_write_data__micro_engine    =  0,
    engine_sel__pfp_write_data__prefetch_parser =  1,
};

// -------------------------------------- PM4_PFP_WRITE_DATA --------------------------------------
typedef struct PM4_PFP_WRITE_DATA
{
    union
    {
        PM4_PFP_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        union
        {
            struct
            {
                uint32_t                       reserved1  :  8;
                PFP_WRITE_DATA_dst_sel_enum    dst_sel    :  4;
                uint32_t                       reserved2  :  4;
                PFP_WRITE_DATA_addr_incr_enum  addr_incr  :  1;
                uint32_t                       reserved3  :  3;
                PFP_WRITE_DATA_wr_confirm_enum wr_confirm :  1;
                PFP_WRITE_DATA_mode_enum       mode       :  1;
                uint32_t                       reserved4  :  1;
                uint32_t                       aid_id     :  2;
                PFP_WRITE_DATA_temporal_enum   temporal   :  2;
                uint32_t                       reserved5  :  3;
                PFP_WRITE_DATA_engine_sel_enum engine_sel :  2;
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
} PM4_PFP_WRITE_DATA;

constexpr unsigned int PM4_PFP_WRITE_DATA_SIZEDW__CORE = 4;

} // inline namespace Chip
} // namespace Gfx12
} // namespace Pal
