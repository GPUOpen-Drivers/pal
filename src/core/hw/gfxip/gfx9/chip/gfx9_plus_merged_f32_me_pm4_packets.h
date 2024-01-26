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

#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
// ------------------------------- ME_ACQUIRE_MEM_pws_stage_sel_enum -------------------------------
enum ME_ACQUIRE_MEM_pws_stage_sel_enum
{
    pws_stage_sel__me_acquire_mem__pre_depth__HASPWS      =  0,
    pws_stage_sel__me_acquire_mem__pre_shader__HASPWS     =  1,
    pws_stage_sel__me_acquire_mem__pre_color__HASPWS      =  2,
    pws_stage_sel__me_acquire_mem__pre_pix_shader__HASPWS =  3,
    pws_stage_sel__me_acquire_mem__cp_pfp__HASPWS         =  4,
    pws_stage_sel__me_acquire_mem__cp_me__HASPWS          =  5,
};
#endif

#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
// ------------------------------ ME_ACQUIRE_MEM_pws_counter_sel_enum ------------------------------
enum ME_ACQUIRE_MEM_pws_counter_sel_enum
{
    pws_counter_sel__me_acquire_mem__ts_select__HASPWS =  0,
    pws_counter_sel__me_acquire_mem__ps_select__HASPWS =  1,
    pws_counter_sel__me_acquire_mem__cs_select__HASPWS =  2,
};
#endif

#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
// --------------------------------- ME_ACQUIRE_MEM_pws_ena2_enum ---------------------------------
enum ME_ACQUIRE_MEM_pws_ena2_enum
{
    pws_ena2__me_acquire_mem__pixel_wait_sync_disable__HASPWS =  0,
    pws_ena2__me_acquire_mem__pixel_wait_sync_enable__HASPWS  =  1,
};
#endif

#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
// ---------------------------------- ME_ACQUIRE_MEM_pws_ena_enum ----------------------------------
enum ME_ACQUIRE_MEM_pws_ena_enum
{
    pws_ena__me_acquire_mem__pixel_wait_sync_disable__HASPWS =  0,
    pws_ena__me_acquire_mem__pixel_wait_sync_enable__HASPWS  =  1,
};
#endif

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
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
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
            } gfx11;
        } bitfieldsB;
#endif
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t coher_size;
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
        uint32_t gcr_size;
#endif
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
            } gfx09_10;
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
            struct
            {
                uint32_t coher_size_hi : 24;
                uint32_t reserved2     :  8;
            } gfx11;
#endif
        } bitfieldsA;
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
        union
        {
            struct
            {
                uint32_t gcr_size_hi : 25;
                uint32_t reserved3   :  7;
            } gfx11;
        } bitfieldsB;
#endif
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t coher_base_lo;
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
        uint32_t gcr_base_lo;
#endif
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
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
        union
        {
            struct
            {
                uint32_t gcr_base_hi : 25;
                uint32_t reserved2   :  7;
            } gfx11;
        } bitfieldsB;
#endif
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
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
        union
        {
            struct
            {
                uint32_t                    reserved2  : 31;
                ME_ACQUIRE_MEM_pws_ena_enum pws_ena    :  1;
            } gfx11;
        } bitfieldsB;
#endif
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
} PM4_ME_ACQUIRE_MEM;

constexpr unsigned int PM4_ME_ACQUIRE_MEM_SIZEDW__CORE      = 7;
constexpr unsigned int PM4_ME_ACQUIRE_MEM_SIZEDW__GFX10PLUS = 8;

// ------------------------------- ME_ATOMIC_GDS_atom_cmp_swap_enum -------------------------------
enum ME_ATOMIC_GDS_atom_cmp_swap_enum
{
    atom_cmp_swap__me_atomic_gds__dont_repeat__CORE       =  0,
    atom_cmp_swap__me_atomic_gds__repeat_until_pass__CORE =  1,
};

// ------------------------------- ME_ATOMIC_GDS_atom_complete_enum -------------------------------
enum ME_ATOMIC_GDS_atom_complete_enum
{
    atom_complete__me_atomic_gds__dont_wait__CORE           =  0,
    atom_complete__me_atomic_gds__wait_for_completion__CORE =  1,
};

// --------------------------------- ME_ATOMIC_GDS_atom_read_enum ---------------------------------
enum ME_ATOMIC_GDS_atom_read_enum
{
    atom_read__me_atomic_gds__dont_read_preop_data__CORE =  0,
    atom_read__me_atomic_gds__read_preop_data__CORE      =  1,
};

// -------------------------------- ME_ATOMIC_GDS_atom_rd_cntl_enum --------------------------------
enum ME_ATOMIC_GDS_atom_rd_cntl_enum
{
    atom_rd_cntl__me_atomic_gds__32bits_1returnval__CORE =  0,
    atom_rd_cntl__me_atomic_gds__32bits_2returnval__CORE =  1,
    atom_rd_cntl__me_atomic_gds__64bits_1returnval__CORE =  2,
    atom_rd_cntl__me_atomic_gds__64bits_2returnval__CORE =  3,
};

// --------------------------------- ME_ATOMIC_GDS_engine_sel_enum ---------------------------------
enum ME_ATOMIC_GDS_engine_sel_enum
{
    engine_sel__me_atomic_gds__micro_engine__CORE =  0,
};

// --------------------------------------- PM4_ME_ATOMIC_GDS ---------------------------------------
typedef struct PM4_ME_ATOMIC_GDS
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
                uint32_t                         atom_op       :  8;
                uint32_t                         reserved1     :  8;
                ME_ATOMIC_GDS_atom_cmp_swap_enum atom_cmp_swap :  1;
                ME_ATOMIC_GDS_atom_complete_enum atom_complete :  1;
                ME_ATOMIC_GDS_atom_read_enum     atom_read     :  1;
                ME_ATOMIC_GDS_atom_rd_cntl_enum  atom_rd_cntl  :  2;
                uint32_t                         reserved2     :  9;
                ME_ATOMIC_GDS_engine_sel_enum    engine_sel    :  2;
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
} PM4_ME_ATOMIC_GDS;

constexpr unsigned int PM4_ME_ATOMIC_GDS_SIZEDW__CORE = 11;

// ---------------------------------- ME_ATOMIC_MEM_command_enum ----------------------------------
enum ME_ATOMIC_MEM_command_enum
{
    command__me_atomic_mem__single_pass_atomic                     =  0,
    command__me_atomic_mem__loop_until_compare_satisfied           =  1,
    command__me_atomic_mem__wait_for_write_confirmation__GFX10PLUS =  2,
    command__me_atomic_mem__send_and_continue__GFX10PLUS           =  3,
};

// -------------------------------- ME_ATOMIC_MEM_cache_policy_enum --------------------------------
enum ME_ATOMIC_MEM_cache_policy_enum
{
    cache_policy__me_atomic_mem__lru               =  0,
    cache_policy__me_atomic_mem__stream            =  1,
    cache_policy__me_atomic_mem__noa__GFX10PLUS    =  2,
    cache_policy__me_atomic_mem__bypass__GFX10PLUS =  3,
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
                uint32_t                        atomic       :  7;
                uint32_t                        reserved1    :  1;
                ME_ATOMIC_MEM_command_enum      command      :  4;
                uint32_t                        reserved2    : 13;
                ME_ATOMIC_MEM_cache_policy_enum cache_policy :  2;
                uint32_t                        reserved3    :  3;
                ME_ATOMIC_MEM_engine_sel_enum   engine_sel   :  2;
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
    cmd__me_clear_state__clear_state__HASCLEARSTATE      =  0,
    cmd__me_clear_state__push_state__HASCLEARSTATE       =  1,
    cmd__me_clear_state__pop_state__HASCLEARSTATE        =  2,
    cmd__me_clear_state__push_clear_state__HASCLEARSTATE =  3,
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
            } hasClearState;
        } bitfields;
        uint32_t u32All;
    } ordinal2;
} PM4_ME_CLEAR_STATE;

constexpr unsigned int PM4_ME_CLEAR_STATE_SIZEDW__HASCLEARSTATE = 2;

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
                uint32_t                       reserved3   : 22;
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
            struct
            {
                uint32_t reserved4   : 28;
                uint32_t load_ce_ram :  1;
                uint32_t reserved5   :  3;
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
    src_sel__me_copy_data__mem_mapped_register           =  0,
    src_sel__me_copy_data__memory__GFX09                 =  1,
    src_sel__me_copy_data__tc_l2_obsolete__GFX10PLUS     =  1,
    src_sel__me_copy_data__tc_l2                         =  2,
    src_sel__me_copy_data__gds__CORE                     =  3,
    src_sel__me_copy_data__perfcounters                  =  4,
    src_sel__me_copy_data__immediate_data                =  5,
    src_sel__me_copy_data__atomic_return_data            =  6,
    src_sel__me_copy_data__gds_atomic_return_data0__CORE =  7,
    src_sel__me_copy_data__gds_atomic_return_data1__CORE =  8,
    src_sel__me_copy_data__gpu_clock_count               =  9,
    src_sel__me_copy_data__system_clock_count            = 10,
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
    src_sel__me_copy_data__exec_ind_arg_buf__GFX11       = 12,
#endif
};

// ----------------------------------- ME_COPY_DATA_dst_sel_enum -----------------------------------
enum ME_COPY_DATA_dst_sel_enum
{
    dst_sel__me_copy_data__mem_mapped_register         =  0,
    dst_sel__me_copy_data__memory_sync_across_grbm     =  1,
    dst_sel__me_copy_data__tc_l2                       =  2,
    dst_sel__me_copy_data__gds__CORE                   =  3,
    dst_sel__me_copy_data__perfcounters                =  4,
    dst_sel__me_copy_data__memory__GFX09               =  5,
    dst_sel__me_copy_data__tc_l2_obsolete__GFX10PLUS   =  5,
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
    dst_sel__me_copy_data__exec_ind_spill_table__GFX11 =  7,
#endif
};

// ------------------------------ ME_COPY_DATA_src_cache_policy_enum ------------------------------
enum ME_COPY_DATA_src_cache_policy_enum
{
    src_cache_policy__me_copy_data__lru               =  0,
    src_cache_policy__me_copy_data__stream            =  1,
    src_cache_policy__me_copy_data__noa__GFX10PLUS    =  2,
    src_cache_policy__me_copy_data__bypass__GFX10PLUS =  3,
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

// ------------------------------ ME_COPY_DATA_dst_cache_policy_enum ------------------------------
enum ME_COPY_DATA_dst_cache_policy_enum
{
    dst_cache_policy__me_copy_data__lru               =  0,
    dst_cache_policy__me_copy_data__stream            =  1,
    dst_cache_policy__me_copy_data__noa__GFX10PLUS    =  2,
    dst_cache_policy__me_copy_data__bypass__GFX10PLUS =  3,
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
                ME_COPY_DATA_src_sel_enum          src_sel          :  4;
                uint32_t                           reserved1        :  4;
                ME_COPY_DATA_dst_sel_enum          dst_sel          :  4;
                uint32_t                           reserved2        :  1;
                ME_COPY_DATA_src_cache_policy_enum src_cache_policy :  2;
                uint32_t                           reserved3        :  1;
                ME_COPY_DATA_count_sel_enum        count_sel        :  1;
                uint32_t                           reserved4        :  3;
                ME_COPY_DATA_wr_confirm_enum       wr_confirm       :  1;
                uint32_t                           reserved5        :  4;
                ME_COPY_DATA_dst_cache_policy_enum dst_cache_policy :  2;
                uint32_t                           reserved6        :  3;
                ME_COPY_DATA_engine_sel_enum       engine_sel       :  2;
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

// ---------------------------------- ME_DMA_DATA_engine_sel_enum ----------------------------------
enum ME_DMA_DATA_engine_sel_enum
{
    engine_sel__me_dma_data__micro_engine =  0,
};

// ------------------------------- ME_DMA_DATA_src_cache_policy_enum -------------------------------
enum ME_DMA_DATA_src_cache_policy_enum
{
    src_cache_policy__me_dma_data__lru               =  0,
    src_cache_policy__me_dma_data__stream            =  1,
    src_cache_policy__me_dma_data__noa__GFX10PLUS    =  2,
    src_cache_policy__me_dma_data__bypass__GFX10PLUS =  3,
};

// ----------------------------------- ME_DMA_DATA_dst_sel_enum -----------------------------------
enum ME_DMA_DATA_dst_sel_enum
{
    dst_sel__me_dma_data__dst_addr_using_das =  0,
    dst_sel__me_dma_data__gds__CORE          =  1,
    dst_sel__me_dma_data__dst_nowhere        =  2,
    dst_sel__me_dma_data__dst_addr_using_l2  =  3,
};

// ------------------------------- ME_DMA_DATA_dst_cache_policy_enum -------------------------------
enum ME_DMA_DATA_dst_cache_policy_enum
{
    dst_cache_policy__me_dma_data__lru               =  0,
    dst_cache_policy__me_dma_data__stream            =  1,
    dst_cache_policy__me_dma_data__noa__GFX10PLUS    =  2,
    dst_cache_policy__me_dma_data__bypass__GFX10PLUS =  3,
};

// ----------------------------------- ME_DMA_DATA_src_sel_enum -----------------------------------
enum ME_DMA_DATA_src_sel_enum
{
    src_sel__me_dma_data__src_addr_using_sas =  0,
    src_sel__me_dma_data__gds__CORE          =  1,
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
                ME_DMA_DATA_engine_sel_enum       engine_sel       :  1;
                uint32_t                          reserved1        : 12;
                ME_DMA_DATA_src_cache_policy_enum src_cache_policy :  2;
                uint32_t                          reserved2        :  5;
                ME_DMA_DATA_dst_sel_enum          dst_sel          :  2;
                uint32_t                          reserved3        :  3;
                ME_DMA_DATA_dst_cache_policy_enum dst_cache_policy :  2;
                uint32_t                          reserved4        :  2;
                ME_DMA_DATA_src_sel_enum          src_sel          :  2;
                uint32_t                          cp_sync          :  1;
            };
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
            struct
            {
                uint32_t reserved5    :  1;
                uint32_t src_indirect :  1;
                uint32_t dst_indirect :  1;
                uint32_t reserved6    : 29;
            } gfx11;
#endif
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

// ------------------------- ME_DMA_DATA_FILL_MULTI_dst_cache_policy_enum -------------------------
enum ME_DMA_DATA_FILL_MULTI_dst_cache_policy_enum
{
    dst_cache_policy__me_dma_data_fill_multi__lru               =  0,
    dst_cache_policy__me_dma_data_fill_multi__stream            =  1,
    dst_cache_policy__me_dma_data_fill_multi__noa__GFX10PLUS    =  2,
    dst_cache_policy__me_dma_data_fill_multi__bypass__GFX10PLUS =  3,
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
                ME_DMA_DATA_FILL_MULTI_engine_sel_enum       engine_sel       :  1;
                uint32_t                                     reserved1        :  9;
                uint32_t                                     memlog_clear     :  1;
                uint32_t                                     reserved2        :  9;
                ME_DMA_DATA_FILL_MULTI_dst_sel_enum          dst_sel          :  2;
                uint32_t                                     reserved3        :  3;
                ME_DMA_DATA_FILL_MULTI_dst_cache_policy_enum dst_cache_policy :  2;
                uint32_t                                     reserved4        :  2;
                ME_DMA_DATA_FILL_MULTI_src_sel_enum          src_sel          :  2;
                uint32_t                                     cp_sync          :  1;
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
        uint32_t draw_initiator;
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
        union
        {
            struct
            {
                uint32_t start_indx_enable :  1;
                uint32_t reserved1         : 31;
            } gfx11;
        } bitfields;
#endif
        uint32_t u32All;
    } ordinal2;

#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
    union
    {
        union
        {
            struct
            {
                uint32_t base_vtx_loc   : 16;
                uint32_t start_indx_loc : 16;
            } gfx11;
        } bitfields;
        uint32_t u32All;
    } ordinal3;
#endif

#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
    union
    {
        union
        {
            struct
            {
                uint32_t start_inst_loc : 16;
                uint32_t reserved1      : 16;
            } gfx11;
        } bitfields;
        uint32_t u32All;
    } ordinal4;
#endif

#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
    union
    {
        uint32_t draw_initiator;
        uint32_t u32All;
    } ordinal5;
#endif
} PM4_ME_DRAW_INDEX_INDIRECT;

constexpr unsigned int PM4_ME_DRAW_INDEX_INDIRECT_SIZEDW__CORE  = 2;
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
constexpr unsigned int PM4_ME_DRAW_INDEX_INDIRECT_SIZEDW__GFX11 = 5;
#endif

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
        uint32_t draw_initiator;
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
        uint32_t draw_index;
#endif
        uint32_t u32All;
    } ordinal2;

#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
    union
    {
        uint32_t draw_initiator;
        uint32_t u32All;
    } ordinal3;
#endif
} PM4_ME_DRAW_INDEX_INDIRECT_MULTI;

constexpr unsigned int PM4_ME_DRAW_INDEX_INDIRECT_MULTI_SIZEDW__CORE  = 2;
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
constexpr unsigned int PM4_ME_DRAW_INDEX_INDIRECT_MULTI_SIZEDW__GFX11 = 3;
#endif

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
                uint32_t index_offset : 16;
                uint32_t prim_type    :  5;
                uint32_t index_count  : 11;
            };
        } bitfields;
        uint32_t u32All;
    } ordinal4;
} PM4_ME_DRAW_INDEX_MULTI_AUTO;

constexpr unsigned int PM4_ME_DRAW_INDEX_MULTI_AUTO_SIZEDW__CORE = 4;

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
        uint32_t draw_initiator;
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
        union
        {
            struct
            {
                uint32_t start_vtx_loc : 16;
                uint32_t reserved1     : 16;
            } gfx11;
        } bitfields;
#endif
        uint32_t u32All;
    } ordinal2;

#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
    union
    {
        union
        {
            struct
            {
                uint32_t start_inst_loc : 16;
                uint32_t reserved1      : 16;
            } gfx11;
        } bitfields;
        uint32_t u32All;
    } ordinal3;
#endif

#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
    union
    {
        uint32_t draw_initiator;
        uint32_t u32All;
    } ordinal4;
#endif
} PM4_ME_DRAW_INDIRECT;

constexpr unsigned int PM4_ME_DRAW_INDIRECT_SIZEDW__CORE  = 2;
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
constexpr unsigned int PM4_ME_DRAW_INDIRECT_SIZEDW__GFX11 = 4;
#endif

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
        uint32_t draw_initiator;
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
        uint32_t draw_index;
#endif
        uint32_t u32All;
    } ordinal2;

#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
    union
    {
        uint32_t draw_initiator;
        uint32_t u32All;
    } ordinal3;
#endif
} PM4_ME_DRAW_INDIRECT_MULTI;

constexpr unsigned int PM4_ME_DRAW_INDIRECT_MULTI_SIZEDW__CORE  = 2;
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
constexpr unsigned int PM4_ME_DRAW_INDIRECT_MULTI_SIZEDW__GFX11 = 3;
#endif

// -------------------------------- ME_EVENT_WRITE_event_index_enum --------------------------------
enum ME_EVENT_WRITE_event_index_enum
{
    event_index__me_event_write__other                           =  0,
    event_index__me_event_write__pixel_pipe_stat_control_or_dump =  1,
    event_index__me_event_write__sample_pipelinestat             =  2,
    event_index__me_event_write__sample_streamoutstats__GFX09_10 =  3,
    event_index__me_event_write__cs_vs_ps_partial_flush          =  4,
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
    event_index__me_event_write__sample_streamoutstats__GFX11    =  8,
    event_index__me_event_write__sample_streamoutstats1__GFX11   =  9,
    event_index__me_event_write__sample_streamoutstats2__GFX11   = 10,
    event_index__me_event_write__sample_streamoutstats3__GFX11   = 11,
#endif
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

// ----------------------------------- ME_FRAME_CONTROL_tmz_enum -----------------------------------
enum ME_FRAME_CONTROL_tmz_enum
{
    tmz__me_frame_control__tmz_off__GFX09_10 =  0,
    tmz__me_frame_control__tmz_on__GFX09_10  =  1,
};

// --------------------------------- ME_FRAME_CONTROL_command_enum ---------------------------------
enum ME_FRAME_CONTROL_command_enum
{
    command__me_frame_control__kmd_frame_begin__GFX09_10 =  0,
    command__me_frame_control__kmd_frame_end__GFX09_10   =  1,
};

// ------------------------------------- PM4_ME_FRAME_CONTROL -------------------------------------
typedef struct PM4_ME_FRAME_CONTROL
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
                ME_FRAME_CONTROL_tmz_enum     tmz        :  1;
                uint32_t                      reserved1  : 27;
                ME_FRAME_CONTROL_command_enum command    :  4;
            } gfx09_10;
        } bitfields;
        uint32_t u32All;
    } ordinal2;
} PM4_ME_FRAME_CONTROL;

constexpr unsigned int PM4_ME_FRAME_CONTROL_SIZEDW__GFX09_10 = 2;

// ------------------------------ ME_GET_LOD_STATS_cache_policy_enum ------------------------------
enum ME_GET_LOD_STATS_cache_policy_enum
{
    cache_policy__me_get_lod_stats__lru__CORE             =  0,
    cache_policy__me_get_lod_stats__stream__CORE          =  1,
    cache_policy__me_get_lod_stats__noa__GFX10COREPLUS    =  2,
    cache_policy__me_get_lod_stats__bypass__GFX10COREPLUS =  3,
};

// ------------------------------------- PM4_ME_GET_LOD_STATS -------------------------------------
typedef struct PM4_ME_GET_LOD_STATS
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        uint32_t bu_size;
        uint32_t u32All;
    } ordinal2;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1  :  6;
                uint32_t base_lo    : 26;
            } core;
        } bitfields;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t base_hi;
        uint32_t u32All;
    } ordinal4;

    union
    {
        union
        {
            struct
            {
                uint32_t                           reserved1        :  2;
                uint32_t                           interval         :  8;
                uint32_t                           reset_cnt        :  8;
                uint32_t                           reset_force      :  1;
                uint32_t                           report_and_reset :  1;
                uint32_t                           reserved2        :  8;
                ME_GET_LOD_STATS_cache_policy_enum cache_policy     :  2;
                uint32_t                           reserved3        :  2;
            } core;
        } bitfields;
        uint32_t u32All;
    } ordinal5;
} PM4_ME_GET_LOD_STATS;

constexpr unsigned int PM4_ME_GET_LOD_STATS_SIZEDW__CORE = 5;

// ---------------------------------- PM4_ME_INCREMENT_DE_COUNTER ----------------------------------
typedef struct PM4_ME_INCREMENT_DE_COUNTER
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
} PM4_ME_INCREMENT_DE_COUNTER;

constexpr unsigned int PM4_ME_INCREMENT_DE_COUNTER_SIZEDW__CORE = 2;

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
    index__me_load_sh_reg_index__direct_addr                   =  0,
    index__me_load_sh_reg_index__offset                        =  1,
#if  CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1 || CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
    index__me_load_sh_reg_index__indirect_addr__GFX103COREPLUS =  2,
#endif
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
                uint32_t reserved1   :  2;
                uint32_t mem_addr_lo : 30;
            };
            struct
            {
                uint32_t                        index      :  1; // ME_LOAD_SH_REG_INDEX_index_enum
                uint32_t                        reserved2  : 31;
            } gfx09;
            struct
            {
                uint32_t                        index      :  1; // ME_LOAD_SH_REG_INDEX_index_enum
                uint32_t                        reserved3  : 31;
            } gfx101;
#if  CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1 || CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
            struct
            {
                ME_LOAD_SH_REG_INDEX_index_enum index      :  2;
                uint32_t                        reserved4  : 30;
            } gfx103CorePlus;
#endif
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

// ------------------------------- ME_MEM_SEMAPHORE_use_mailbox_enum -------------------------------
enum ME_MEM_SEMAPHORE_use_mailbox_enum
{
    use_mailbox__me_mem_semaphore__do_not_wait_for_mailbox__GFX09_GFX10CORE =  0,
    use_mailbox__me_mem_semaphore__wait_for_mailbox__GFX09_GFX10CORE        =  1,
};

// ------------------------------- ME_MEM_SEMAPHORE_signal_type_enum -------------------------------
enum ME_MEM_SEMAPHORE_signal_type_enum
{
    signal_type__me_mem_semaphore__signal_type_increment__GFX09_GFX10CORE =  0,
    signal_type__me_mem_semaphore__signal_type_write__GFX09_GFX10CORE     =  1,
};

// --------------------------------- ME_MEM_SEMAPHORE_sem_sel_enum ---------------------------------
enum ME_MEM_SEMAPHORE_sem_sel_enum
{
    sem_sel__me_mem_semaphore__signal_semaphore__GFX09_GFX10CORE =  6,
    sem_sel__me_mem_semaphore__wait_semaphore__GFX09_GFX10CORE   =  7,
};

// ------------------------------------- PM4_ME_MEM_SEMAPHORE -------------------------------------
typedef struct PM4_ME_MEM_SEMAPHORE
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
                uint32_t                          reserved1   : 16;
                ME_MEM_SEMAPHORE_use_mailbox_enum use_mailbox :  1;
                uint32_t                          reserved2   :  3;
                ME_MEM_SEMAPHORE_signal_type_enum signal_type :  1;
                uint32_t                          reserved3   :  8;
                ME_MEM_SEMAPHORE_sem_sel_enum     sem_sel     :  3;
            } hasCe;
        } bitfields;
        uint32_t u32All;
    } ordinal4;
} PM4_ME_MEM_SEMAPHORE;

constexpr unsigned int PM4_ME_MEM_SEMAPHORE_SIZEDW__HASCE = 4;

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
    command__me_preamble_cntl__preamble_begin__HASCLEARSTATE                      =  0,
    command__me_preamble_cntl__preamble_end__HASCLEARSTATE                        =  1,
    command__me_preamble_cntl__begin_of_clear_state_initialization__HASCLEARSTATE =  2,
    command__me_preamble_cntl__end_of_clear_state_initialization__HASCLEARSTATE   =  3,
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
    command__me_preamble_cntl__user_queues_state_save__GFX11                      =  4,
    command__me_preamble_cntl__user_queues_state_restore__GFX11                   =  5,
#endif
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
            } hasClearState;
        } bitfields;
        uint32_t u32All;
    } ordinal2;
} PM4_ME_PREAMBLE_CNTL;

constexpr unsigned int PM4_ME_PREAMBLE_CNTL_SIZEDW__HASCLEARSTATE = 2;

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

// ------------------------------- ME_RELEASE_MEM_cache_policy_enum -------------------------------
enum ME_RELEASE_MEM_cache_policy_enum
{
    cache_policy__me_release_mem__lru               =  0,
    cache_policy__me_release_mem__stream            =  1,
    cache_policy__me_release_mem__noa__GFX10PLUS    =  2,
    cache_policy__me_release_mem__bypass__GFX10PLUS =  3,
};

// ---------------------------------- ME_RELEASE_MEM_dst_sel_enum ----------------------------------
enum ME_RELEASE_MEM_dst_sel_enum
{
    dst_sel__me_release_mem__memory_controller                           =  0,
    dst_sel__me_release_mem__tc_l2                                       =  1,
    dst_sel__me_release_mem__queue_write_pointer_register__NOTGFX10      =  2,
    dst_sel__me_release_mem__queue_write_pointer_poll_mask_bit__NOTGFX10 =  3,
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
    data_sel__me_release_mem__none                              =  0,
    data_sel__me_release_mem__send_32_bit_low                   =  1,
    data_sel__me_release_mem__send_64_bit_data                  =  2,
    data_sel__me_release_mem__send_gpu_clock_counter            =  3,
    data_sel__me_release_mem__send_system_clock_counter         =  4,
    data_sel__me_release_mem__store_gds_data_to_memory__CORE    =  5,
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
    data_sel__me_release_mem__send_emulated_sclk_counter__GFX11 =  6,
#endif
};

// ------------------------------- ME_RELEASE_MEM_mes_action_id_enum -------------------------------
enum ME_RELEASE_MEM_mes_action_id_enum
{
    mes_action_id__me_release_mem__no_mes_notification__GFX10COREPLUS                     =  0,
    mes_action_id__me_release_mem__interrupt_and_fence__GFX10COREPLUS                     =  1,
    mes_action_id__me_release_mem__interrupt_no_fence_then_address_payload__GFX10COREPLUS =  2,
    mes_action_id__me_release_mem__interrupt_and_address_payload__GFX10COREPLUS           =  3,
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
                uint32_t                         event_type   :  6;
                uint32_t                         reserved1    :  2;
                ME_RELEASE_MEM_event_index_enum  event_index  :  4;
                uint32_t                         reserved2    : 13;
                ME_RELEASE_MEM_cache_policy_enum cache_policy :  2;
                uint32_t                         reserved3    :  5;
            };
            struct
            {
                uint32_t reserved4           : 12;
                uint32_t tcl1_vol_action_ena :  1;
                uint32_t reserved5           :  2;
                uint32_t tc_wb_action_ena    :  1;
                uint32_t tcl1_action_ena     :  1;
                uint32_t tc_action_ena       :  1;
                uint32_t reserved6           :  1;
                uint32_t tc_nc_action_ena    :  1;
                uint32_t tc_wc_action_ena    :  1;
                uint32_t tc_md_action_ena    :  1;
                uint32_t reserved7           : 10;
            } gfx09;
            struct
            {
                uint32_t reserved8  : 28;
                uint32_t execute    :  1;
                uint32_t reserved9  :  3;
            } gfx09_10;
            struct
            {
                uint32_t reserved10 :  7;
                uint32_t wait_dma   :  1;
                uint32_t reserved11 :  4;
                uint32_t gcr_cntl   : 12;
                uint32_t reserved12 :  8;
            } gfx10;
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
            struct
            {
                uint32_t reserved13 :  7;
                uint32_t wait_sync  :  1;
                uint32_t reserved14 :  4;
                uint32_t gcr_cntl   : 13;
                uint32_t reserved15 :  3;
                uint32_t execute    :  2;
                uint32_t glk_inv    :  1;
                uint32_t pws_enable :  1;
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
                uint32_t                     reserved1  : 16;
                ME_RELEASE_MEM_dst_sel_enum  dst_sel    :  2;
                uint32_t                     reserved2  :  6;
                ME_RELEASE_MEM_int_sel_enum  int_sel    :  3;
                uint32_t                     reserved3  :  2;
                ME_RELEASE_MEM_data_sel_enum data_sel   :  3;
            };
            struct
            {
                uint32_t                          reserved4     : 20;
                uint32_t                          mes_intr_pipe :  2;
                ME_RELEASE_MEM_mes_action_id_enum mes_action_id :  2;
                uint32_t                          reserved5     :  8;
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
        union
        {
            struct
            {
                uint32_t int_ctxid  : 28;
                uint32_t reserved1  :  4;
            } gfx10Plus;
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

// --------------------------------- PM4_ME_SET_CONTEXT_REG_INDEX ---------------------------------
typedef struct PM4_ME_SET_CONTEXT_REG_INDEX
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
            } gfx09;
        } bitfields;
        uint32_t u32All;
    } ordinal2;
} PM4_ME_SET_CONTEXT_REG_INDEX;

constexpr unsigned int PM4_ME_SET_CONTEXT_REG_INDEX_SIZEDW__GFX09 = 2;

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
    index__me_set_sh_reg_index__apply_kmd_cu_and_mask =  3,
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
        } bitfields;
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

// -------------------------- ME_STRMOUT_BUFFER_UPDATE_update_memory_enum --------------------------
enum ME_STRMOUT_BUFFER_UPDATE_update_memory_enum
{
    update_memory__me_strmout_buffer_update__dont_update_memory           =  0,
    update_memory__me_strmout_buffer_update__update_memory_at_dst_address =  1,
};

// -------------------------- ME_STRMOUT_BUFFER_UPDATE_source_select_enum --------------------------
enum ME_STRMOUT_BUFFER_UPDATE_source_select_enum
{
    source_select__me_strmout_buffer_update__use_buffer_offset               =  0,
    source_select__me_strmout_buffer_update__read_strmout_buffer_filled_size =  1,
    source_select__me_strmout_buffer_update__from_src_address                =  2,
    source_select__me_strmout_buffer_update__none__GFX09_10                  =  3,
};

// ---------------------------- ME_STRMOUT_BUFFER_UPDATE_data_type_enum ----------------------------
enum ME_STRMOUT_BUFFER_UPDATE_data_type_enum
{
    data_type__me_strmout_buffer_update__dwords =  0,
    data_type__me_strmout_buffer_update__bytes  =  1,
};

// -------------------------- ME_STRMOUT_BUFFER_UPDATE_buffer_select_enum --------------------------
enum ME_STRMOUT_BUFFER_UPDATE_buffer_select_enum
{
    buffer_select__me_strmout_buffer_update__stream_out_buffer_0 =  0,
    buffer_select__me_strmout_buffer_update__stream_out_buffer_1 =  1,
    buffer_select__me_strmout_buffer_update__stream_out_buffer_2 =  2,
    buffer_select__me_strmout_buffer_update__stream_out_buffer_3 =  3,
};

// --------------------------------- PM4_ME_STRMOUT_BUFFER_UPDATE ---------------------------------
typedef struct PM4_ME_STRMOUT_BUFFER_UPDATE
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
                ME_STRMOUT_BUFFER_UPDATE_update_memory_enum update_memory :  1;
                ME_STRMOUT_BUFFER_UPDATE_source_select_enum source_select :  2;
                uint32_t                                    reserved1     :  4;
                ME_STRMOUT_BUFFER_UPDATE_data_type_enum     data_type     :  1;
                ME_STRMOUT_BUFFER_UPDATE_buffer_select_enum buffer_select :  2;
                uint32_t                                    reserved2     : 22;
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
                uint32_t reserved1      :  2;
                uint32_t dst_address_lo : 30;
            } gfx09_10;
        } bitfields;
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
        uint32_t offset;
#endif
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
        union
        {
            struct
            {
                uint32_t reserved2      :  2;
                uint32_t src_address_lo : 30;
            } gfx11;
        } bitfieldsB;
#endif
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
        union
        {
            struct
            {
                uint32_t reserved3      :  2;
                uint32_t dst_address_lo : 30;
            } gfx11;
        } bitfieldsC;
#endif
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t dst_address_hi;
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
        uint32_t src_address_hi;
#endif
        uint32_t u32All;
    } ordinal4;

    union
    {
        uint32_t offset_or_address_lo;
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
        union
        {
            struct
            {
                uint32_t reserved1          :  2;
                uint32_t control_address_lo : 30;
            } gfx11;
        } bitfields;
#endif
        uint32_t u32All;
    } ordinal5;

    union
    {
        uint32_t src_address_hi;
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
        uint32_t control_address_hi;
#endif
        uint32_t u32All;
    } ordinal6;
} PM4_ME_STRMOUT_BUFFER_UPDATE;

constexpr unsigned int PM4_ME_STRMOUT_BUFFER_UPDATE_SIZEDW__CORE = 6;

// ------------------------------------- PM4_ME_SWITCH_BUFFER -------------------------------------
typedef struct PM4_ME_SWITCH_BUFFER
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
                uint32_t tmz        :  1;
                uint32_t reserved1  : 31;
            } gfx09;
        } bitfields;
        uint32_t dummy;
        uint32_t u32All;
    } ordinal2;
} PM4_ME_SWITCH_BUFFER;

constexpr unsigned int PM4_ME_SWITCH_BUFFER_SIZEDW__HASCE = 2;

// ----------------------------------- PM4_ME_WAIT_ON_CE_COUNTER -----------------------------------
typedef struct PM4_ME_WAIT_ON_CE_COUNTER
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
                uint32_t cond_surface_sync :  1;
                uint32_t force_sync        :  1;
                uint32_t reserved1         : 30;
            } core;
        } bitfields;
        uint32_t u32All;
    } ordinal2;
} PM4_ME_WAIT_ON_CE_COUNTER;

constexpr unsigned int PM4_ME_WAIT_ON_CE_COUNTER_SIZEDW__CORE = 2;

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

// ------------------------------- ME_WAIT_REG_MEM_cache_policy_enum -------------------------------
enum ME_WAIT_REG_MEM_cache_policy_enum
{
    cache_policy__me_wait_reg_mem__lru__GFX10PLUS    =  0,
    cache_policy__me_wait_reg_mem__stream__GFX10PLUS =  1,
    cache_policy__me_wait_reg_mem__noa__GFX10PLUS    =  2,
    cache_policy__me_wait_reg_mem__bypass__GFX10PLUS =  3,
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
                ME_WAIT_REG_MEM_function_enum   function   :  3;
                uint32_t                        reserved1  :  1;
                ME_WAIT_REG_MEM_mem_space_enum  mem_space  :  2;
                ME_WAIT_REG_MEM_operation_enum  operation  :  2;
                ME_WAIT_REG_MEM_engine_sel_enum engine_sel :  2;
                uint32_t                        reserved2  : 22;
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
                uint32_t                          reserved5    : 25;
                ME_WAIT_REG_MEM_cache_policy_enum cache_policy :  2;
                uint32_t                          reserved6    :  5;
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

// ------------------------------ ME_WAIT_REG_MEM64_cache_policy_enum ------------------------------
enum ME_WAIT_REG_MEM64_cache_policy_enum
{
    cache_policy__me_wait_reg_mem64__lru__GFX10PLUS    =  0,
    cache_policy__me_wait_reg_mem64__stream__GFX10PLUS =  1,
    cache_policy__me_wait_reg_mem64__noa__GFX10PLUS    =  2,
    cache_policy__me_wait_reg_mem64__bypass__GFX10PLUS =  3,
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
                ME_WAIT_REG_MEM64_function_enum   function   :  3;
                uint32_t                          reserved1  :  1;
                ME_WAIT_REG_MEM64_mem_space_enum  mem_space  :  2;
                ME_WAIT_REG_MEM64_operation_enum  operation  :  2;
                ME_WAIT_REG_MEM64_engine_sel_enum engine_sel :  2;
                uint32_t                          reserved2  : 22;
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
                uint32_t                            reserved5    : 25;
                ME_WAIT_REG_MEM64_cache_policy_enum cache_policy :  2;
                uint32_t                            reserved6    :  5;
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
    dst_sel__me_write_data__gds__CORE               =  3,
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

// -------------------------------- ME_WRITE_DATA_cache_policy_enum --------------------------------
enum ME_WRITE_DATA_cache_policy_enum
{
    cache_policy__me_write_data__lru               =  0,
    cache_policy__me_write_data__stream            =  1,
    cache_policy__me_write_data__noa__GFX10PLUS    =  2,
    cache_policy__me_write_data__bypass__GFX10PLUS =  3,
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
                uint32_t                        reserved1    :  8;
                ME_WRITE_DATA_dst_sel_enum      dst_sel      :  4;
                uint32_t                        reserved2    :  4;
                ME_WRITE_DATA_addr_incr_enum    addr_incr    :  1;
                uint32_t                        reserved3    :  3;
                ME_WRITE_DATA_wr_confirm_enum   wr_confirm   :  1;
                uint32_t                        reserved4    :  4;
                ME_WRITE_DATA_cache_policy_enum cache_policy :  2;
                uint32_t                        reserved5    :  3;
                ME_WRITE_DATA_engine_sel_enum   engine_sel   :  2;
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
} PM4_ME_WRITE_DATA;

constexpr unsigned int PM4_ME_WRITE_DATA_SIZEDW__CORE = 4;

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
                uint32_t reserved1  : 28;
                uint32_t use_vgprs  :  1;
                uint32_t reserved2  :  3;
            } gfx10Core;
            struct
            {
                uint32_t reserved3                  : 29;
                uint32_t thread_trace_marker_enable :  1;
                uint32_t count_indirect_enable      :  1;
                uint32_t draw_index_enable          :  1;
            } gfx10CorePlus;
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
            struct
            {
                uint32_t reserved4      : 27;
                uint32_t mode1_enable   :  1;
                uint32_t xyz_dim_enable :  1;
                uint32_t reserved5      :  3;
            } gfx11;
#endif
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
            } gfx10CorePlus;
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

constexpr unsigned int PM4_ME_DISPATCH_MESH_INDIRECT_MULTI_SIZEDW__GFX10COREPLUS = 9;

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
            } gfx10CorePlus;
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t control_buf_addr_hi;
        uint32_t u32All;
    } ordinal3;
} PM4_ME_DISPATCH_TASK_STATE_INIT;

constexpr unsigned int PM4_ME_DISPATCH_TASK_STATE_INIT_SIZEDW__GFX10COREPLUS = 3;

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
            } gfx10CorePlus;
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        union
        {
            struct
            {
                uint32_t reserved1                  : 31;
                uint32_t thread_trace_marker_enable :  1;
            } gfx10CorePlus;
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
            struct
            {
                uint32_t reserved2              : 28;
                uint32_t linear_dispatch_enable :  1;
                uint32_t mode1_enable           :  1;
                uint32_t xyz_dim_enable         :  1;
                uint32_t reserved3              :  1;
            } gfx11;
#endif
        } bitfields;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t draw_initiator;
        uint32_t u32All;
    } ordinal4;
} PM4_ME_DISPATCH_TASKMESH_GFX;

constexpr unsigned int PM4_ME_DISPATCH_TASKMESH_GFX_SIZEDW__GFX10COREPLUS = 4;

// --------------------------------- PM4_ME_DRAW_INDEX_MULTI_INST ---------------------------------
typedef struct PM4_ME_DRAW_INDEX_MULTI_INST
{
    union
    {
        PM4_ME_TYPE_3_HEADER header;
        uint32_t u32All;
    } ordinal1;

    union
    {
        uint32_t instance_count;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t index_count;
        uint32_t u32All;
    } ordinal3;

    union
    {
        uint32_t draw_initiator;
        uint32_t u32All;
    } ordinal4;
} PM4_ME_DRAW_INDEX_MULTI_INST;

constexpr unsigned int PM4_ME_DRAW_INDEX_MULTI_INST_SIZEDW__GFX101        = 4;

// ------------------------------------- PM4_ME_GFX_PIPE_LOCK -------------------------------------
typedef struct PM4_ME_GFX_PIPE_LOCK
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
                uint32_t sync_ssu_cntx_mgr :  1;
                uint32_t sync_dma          :  1;
                uint32_t reserved1         : 30;
            } gfx10CorePlus;
        } bitfields;
        uint32_t u32All;
    } ordinal2;
} PM4_ME_GFX_PIPE_LOCK;

constexpr unsigned int PM4_ME_GFX_PIPE_LOCK_SIZEDW__GFX10COREPLUS = 2;

// ----------------------------- ME_LOAD_UCONFIG_REG_INDEX_index_enum -----------------------------
enum ME_LOAD_UCONFIG_REG_INDEX_index_enum
{
    index__me_load_uconfig_reg_index__direct_addr__GFX10PLUS =  0,
    index__me_load_uconfig_reg_index__offset__GFX10PLUS      =  1,
};

// -------------------------- ME_LOAD_UCONFIG_REG_INDEX_data_format_enum --------------------------
enum ME_LOAD_UCONFIG_REG_INDEX_data_format_enum
{
    data_format__me_load_uconfig_reg_index__offset_and_size__GFX10PLUS =  0,
    data_format__me_load_uconfig_reg_index__offset_and_data__GFX10PLUS =  1,
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
            } gfx10Plus;
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
            } gfx10Plus;
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
            } gfx10Plus;
        } bitfields;
        uint32_t u32All;
    } ordinal5;
} PM4_ME_LOAD_UCONFIG_REG_INDEX;

constexpr unsigned int PM4_ME_LOAD_UCONFIG_REG_INDEX_SIZEDW__GFX10PLUS = 5;

#if  CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1 || CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
// -------------------------------- ME_PERFMON_CONTROL_pmc_en_enum --------------------------------
enum ME_PERFMON_CONTROL_pmc_en_enum
{
    pmc_en__me_perfmon_control__perfmon_disable__GFX103COREPLUS =  0,
    pmc_en__me_perfmon_control__perfmon_enable__GFX103COREPLUS  =  1,
};
#endif

#if  CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1 || CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
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
} PM4_ME_PERFMON_CONTROL;

constexpr unsigned int PM4_ME_PERFMON_CONTROL_SIZEDW__GFX103COREPLUS = 3;
#endif

#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
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

constexpr unsigned int PM4_ME_DISPATCH_MESH_DIRECT_SIZEDW__GFX11 = 5;
#endif

#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
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
            } gfx11;
        } bitfields;
        uint32_t u32All;
    } ordinal2;

    union
    {
        uint32_t address_hi;
        uint32_t u32All;
    } ordinal3;
} PM4_ME_EVENT_WRITE_ZPASS;

constexpr unsigned int PM4_ME_EVENT_WRITE_ZPASS_SIZEDW__GFX11 = 3;
#endif

#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
// ---------------------------------- ME_TIMESTAMP_clock_sel_enum ----------------------------------
enum ME_TIMESTAMP_clock_sel_enum
{
    clock_sel__me_timestamp__gfx_ip_clock__GFX11 =  0,
    clock_sel__me_timestamp__soc_clock__GFX11    =  1,
};
#endif

#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
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
} PM4_ME_TIMESTAMP;

constexpr unsigned int PM4_ME_TIMESTAMP_SIZEDW__GFX11 = 6;
#endif

} // inline namespace Chip
} // namespace Gfx9
} // namespace Pal
