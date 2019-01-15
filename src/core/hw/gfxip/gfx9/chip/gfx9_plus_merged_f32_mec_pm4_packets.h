/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

//--------------------TYPE_3_HEADER--------------------
typedef union PM4_MEC_TYPE_3_HEADER
{
    struct
    {
        uint32_t                              reserved1 : 8;
        uint32_t                                 opcode : 8;
        uint32_t                                  count : 14;
        uint32_t                                   type : 2;
    };
    uint32_t                                     u32All;

} PM4_MEC_TYPE_3_HEADER, *PPM4_MEC_TYPE_3_HEADER;

//--------------------ACQUIRE_MEM--------------------

//--------------------ACQUIRE_MEM__GFX09--------------------
typedef struct PM4_MEC_ACQUIRE_MEM__GFX09
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    union
    {
        struct
        {
            uint32_t                         coher_cntl : 31;
            uint32_t                          reserved1 : 1;
        } bitfields2;
        uint32_t                               ordinal2;
    };

    uint32_t                                 coher_size;

    union
    {
        struct
        {
            uint32_t                      coher_size_hi : 8;
            uint32_t                          reserved1 : 24;
        } bitfields4;
        uint32_t                               ordinal4;
    };

    uint32_t                              coher_base_lo;

    union
    {
        struct
        {
            uint32_t                      coher_base_hi : 24;
            uint32_t                          reserved1 : 8;
        } bitfields6;
        uint32_t                               ordinal6;
    };

    union
    {
        struct
        {
            uint32_t                      poll_interval : 16;
            uint32_t                          reserved1 : 16;
        } bitfields7;
        uint32_t                               ordinal7;
    };

} PM4MEC_ACQUIRE_MEM__GFX09, *PPM4MEC_ACQUIRE_MEM__GFX09;

//--------------------ATOMIC_GDS--------------------
enum MEC_ATOMIC_GDS_atom_cmp_swap_enum {
    atom_cmp_swap__mec_atomic_gds__dont_repeat                             =  0,
    atom_cmp_swap__mec_atomic_gds__repeat_until_pass                       =  1,
};

enum MEC_ATOMIC_GDS_atom_complete_enum {
    atom_complete__mec_atomic_gds__dont_wait                               =  0,
    atom_complete__mec_atomic_gds__wait_for_completion                     =  1,
};

enum MEC_ATOMIC_GDS_atom_rd_cntl_enum {
    atom_rd_cntl__mec_atomic_gds__32bits_1returnval                        =  0,
    atom_rd_cntl__mec_atomic_gds__32bits_2returnval                        =  1,
    atom_rd_cntl__mec_atomic_gds__64bits_1returnval                        =  2,
    atom_rd_cntl__mec_atomic_gds__64bits_2returnval                        =  3,
};

enum MEC_ATOMIC_GDS_atom_read_enum {
    atom_read__mec_atomic_gds__dont_read_preop_data                        =  0,
    atom_read__mec_atomic_gds__read_preop_data                             =  1,
};

typedef struct PM4_MEC_ATOMIC_GDS
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    union
    {
        struct
        {
            uint32_t                            atom_op : 7;
            uint32_t                          reserved1 : 9;
            MEC_ATOMIC_GDS_atom_cmp_swap_enum atom_cmp_swap : 1;
            MEC_ATOMIC_GDS_atom_complete_enum atom_complete : 1;
            MEC_ATOMIC_GDS_atom_read_enum     atom_read : 1;
            MEC_ATOMIC_GDS_atom_rd_cntl_enum atom_rd_cntl : 2;
            uint32_t                          reserved2 : 11;
        } bitfields2;
        uint32_t                               ordinal2;
    };

    union
    {
        struct
        {
            uint32_t                     auto_inc_bytes : 6;
            uint32_t                          reserved1 : 2;
            uint32_t                              dmode : 1;
            uint32_t                          reserved2 : 23;
        } bitfields3;
        uint32_t                               ordinal3;
    };

    union
    {
        struct
        {
            uint32_t                          atom_base : 16;
            uint32_t                          reserved1 : 16;
        } bitfields4;
        uint32_t                               ordinal4;
    };

    union
    {
        struct
        {
            uint32_t                          atom_size : 16;
            uint32_t                          reserved1 : 16;
        } bitfields5;
        uint32_t                               ordinal5;
    };

    union
    {
        struct
        {
            uint32_t                       atom_offset0 : 8;
            uint32_t                          reserved1 : 8;
            uint32_t                       atom_offset1 : 8;
            uint32_t                          reserved2 : 8;
        } bitfields6;
        uint32_t                               ordinal6;
    };

    uint32_t                                   atom_dst;

    uint32_t                                  atom_src0;

    uint32_t                                atom_src0_u;

    uint32_t                                  atom_src1;

    uint32_t                                atom_src1_u;

} PM4MEC_ATOMIC_GDS, *PPM4MEC_ATOMIC_GDS;

//--------------------ATOMIC_MEM--------------------
enum MEC_ATOMIC_MEM_cache_policy_enum {
    cache_policy__mec_atomic_mem__lru                                      =  0,
    cache_policy__mec_atomic_mem__stream                                   =  1,
};

enum MEC_ATOMIC_MEM_command_enum {
    command__mec_atomic_mem__single_pass_atomic                            =  0,
    command__mec_atomic_mem__loop_until_compare_satisfied                  =  1,
};

typedef struct PM4_MEC_ATOMIC_MEM
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    union
    {
        struct
        {
            uint32_t                             atomic : 7;
            uint32_t                          reserved1 : 1;
            MEC_ATOMIC_MEM_command_enum         command : 4;
            uint32_t                          reserved2 : 13;
            MEC_ATOMIC_MEM_cache_policy_enum cache_policy : 2;
            uint32_t                          reserved3 : 5;
        } bitfields2;
        uint32_t                               ordinal2;
    };

    uint32_t                                    addr_lo;

    uint32_t                                    addr_hi;

    uint32_t                                src_data_lo;

    uint32_t                                src_data_hi;

    uint32_t                                cmp_data_lo;

    uint32_t                                cmp_data_hi;

    union
    {
        struct
        {
            uint32_t                      loop_interval : 13;
            uint32_t                          reserved1 : 19;
        } bitfields9;
        uint32_t                               ordinal9;
    };

} PM4MEC_ATOMIC_MEM, *PPM4MEC_ATOMIC_MEM;

//--------------------COND_EXEC--------------------
enum MEC_COND_EXEC_cache_policy_enum {
    cache_policy__mec_cond_exec__lru                                       =  0,
    cache_policy__mec_cond_exec__stream                                    =  1,
};

typedef struct PM4_MEC_COND_EXEC
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    union
    {
        struct
        {
            uint32_t                          reserved1 : 2;
            uint32_t                            addr_lo : 30;
        } bitfields2;
        uint32_t                               ordinal2;
    };

    uint32_t                                    addr_hi;

    union
    {
        struct
        {
            uint32_t                          reserved1 : 25;
            MEC_COND_EXEC_cache_policy_enum cache_policy : 2;
            uint32_t                          reserved2 : 5;
        } bitfields4;
        uint32_t                               ordinal4;
    };

    union
    {
        struct
        {
            uint32_t                         exec_count : 14;
            uint32_t                          reserved1 : 18;
        } bitfields5;
        uint32_t                               ordinal5;
    };

} PM4MEC_COND_EXEC, *PPM4MEC_COND_EXEC;

//--------------------COND_INDIRECT_BUFFER--------------------
enum MEC_COND_INDIRECT_BUFFER_cache_policy1_enum {
    cache_policy1__mec_cond_indirect_buffer__lru                           =  0,
    cache_policy1__mec_cond_indirect_buffer__stream                        =  1,
};

enum MEC_COND_INDIRECT_BUFFER_cache_policy2_enum {
    cache_policy2__mec_cond_indirect_buffer__lru                           =  0,
    cache_policy2__mec_cond_indirect_buffer__stream                        =  1,
};

enum MEC_COND_INDIRECT_BUFFER_function_enum {
    function__mec_cond_indirect_buffer__always_pass                        =  0,
    function__mec_cond_indirect_buffer__less_than_ref_value                =  1,
    function__mec_cond_indirect_buffer__less_than_equal_to_the_ref_value   =  2,
    function__mec_cond_indirect_buffer__equal_to_the_reference_value       =  3,
    function__mec_cond_indirect_buffer__not_equal_reference_value          =  4,
    function__mec_cond_indirect_buffer__greater_than_or_equal_reference_value =  5,
    function__mec_cond_indirect_buffer__greater_than_reference_value       =  6,
};

enum MEC_COND_INDIRECT_BUFFER_mode_enum {
    mode__mec_cond_indirect_buffer__if_then                                =  1,
    mode__mec_cond_indirect_buffer__if_then_else                           =  2,
};

typedef struct PM4_MEC_COND_INDIRECT_BUFFER
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    union
    {
        struct
        {
            MEC_COND_INDIRECT_BUFFER_mode_enum     mode : 2;
            uint32_t                          reserved1 : 6;
            MEC_COND_INDIRECT_BUFFER_function_enum function : 3;
            uint32_t                          reserved2 : 21;
        } bitfields2;
        uint32_t                               ordinal2;
    };

    union
    {
        struct
        {
            uint32_t                          reserved1 : 3;
            uint32_t                    compare_addr_lo : 29;
        } bitfields3;
        uint32_t                               ordinal3;
    };

    uint32_t                            compare_addr_hi;

    uint32_t                                    mask_lo;

    uint32_t                                    mask_hi;

    uint32_t                               reference_lo;

    uint32_t                               reference_hi;

    union
    {
        struct
        {
            uint32_t                          reserved1 : 2;
            uint32_t                        ib_base1_lo : 30;
        } bitfields9;
        uint32_t                               ordinal9;
    };

    uint32_t                                ib_base1_hi;

    union
    {
        struct
        {
            uint32_t                           ib_size1 : 20;
            uint32_t                          reserved1 : 8;
            MEC_COND_INDIRECT_BUFFER_cache_policy1_enum cache_policy1 : 2;
            uint32_t                          reserved2 : 2;
        } bitfields11;
        uint32_t                              ordinal11;
    };

    union
    {
        struct
        {
            uint32_t                          reserved1 : 2;
            uint32_t                        ib_base2_lo : 30;
        } bitfields12;
        uint32_t                              ordinal12;
    };

    uint32_t                                ib_base2_hi;

    union
    {
        struct
        {
            uint32_t                           ib_size2 : 20;
            uint32_t                          reserved1 : 8;
            MEC_COND_INDIRECT_BUFFER_cache_policy2_enum cache_policy2 : 2;
            uint32_t                          reserved2 : 2;
        } bitfields14;
        uint32_t                              ordinal14;
    };

} PM4MEC_COND_INDIRECT_BUFFER, *PPM4MEC_COND_INDIRECT_BUFFER;

//--------------------COND_WRITE--------------------
enum MEC_COND_WRITE_function_enum {
    function__mec_cond_write__always_pass                                  =  0,
    function__mec_cond_write__less_than_ref_value                          =  1,
    function__mec_cond_write__less_than_equal_to_the_ref_value             =  2,
    function__mec_cond_write__equal_to_the_reference_value                 =  3,
    function__mec_cond_write__not_equal_reference_value                    =  4,
    function__mec_cond_write__greater_than_or_equal_reference_value        =  5,
    function__mec_cond_write__greater_than_reference_value                 =  6,
};

enum MEC_COND_WRITE_poll_space_enum {
    poll_space__mec_cond_write__register                                   =  0,
    poll_space__mec_cond_write__memory                                     =  1,
};

enum MEC_COND_WRITE_write_space_enum {
    write_space__mec_cond_write__register                                  =  0,
    write_space__mec_cond_write__memory                                    =  1,
    write_space__mec_cond_write__scratch                                   =  2,
};

typedef struct PM4_MEC_COND_WRITE
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    union
    {
        struct
        {
            MEC_COND_WRITE_function_enum       function : 3;
            uint32_t                          reserved1 : 1;
            MEC_COND_WRITE_poll_space_enum   poll_space : 1;
            uint32_t                          reserved2 : 3;
            MEC_COND_WRITE_write_space_enum write_space : 2;
            uint32_t                          reserved3 : 22;
        } bitfields2;
        uint32_t                               ordinal2;
    };

    uint32_t                            poll_address_lo;

    uint32_t                            poll_address_hi;

    uint32_t                                  reference;

    uint32_t                                       mask;

    uint32_t                           write_address_lo;

    uint32_t                           write_address_hi;

    uint32_t                                 write_data;

} PM4MEC_COND_WRITE, *PPM4MEC_COND_WRITE;

//--------------------COPY_DATA--------------------
enum MEC_COPY_DATA_count_sel_enum {
    count_sel__mec_copy_data__32_bits_of_data                              =  0,
    count_sel__mec_copy_data__64_bits_of_data                              =  1,
};

enum MEC_COPY_DATA_dst_cache_policy_enum {
    dst_cache_policy__mec_copy_data__lru                                   =  0,
    dst_cache_policy__mec_copy_data__stream                                =  1,
};

enum MEC_COPY_DATA_dst_sel_enum {
    dst_sel__mec_copy_data__mem_mapped_register                            =  0,
    dst_sel__mec_copy_data__tc_l2                                          =  2,
    dst_sel__mec_copy_data__gds                                            =  3,
    dst_sel__mec_copy_data__perfcounters                                   =  4,
    dst_sel__mec_copy_data__memory__GFX09                                  =  5,
    dst_sel__mec_copy_data__mem_mapped_reg_dc                              =  6,
};

enum MEC_COPY_DATA_pq_exe_status_enum {
    pq_exe_status__mec_copy_data__default                                  =  0,
    pq_exe_status__mec_copy_data__phase_update                             =  1,
};

enum MEC_COPY_DATA_src_cache_policy_enum {
    src_cache_policy__mec_copy_data__lru                                   =  0,
    src_cache_policy__mec_copy_data__stream                                =  1,
};

enum MEC_COPY_DATA_src_sel_enum {
    src_sel__mec_copy_data__mem_mapped_register                            =  0,
    src_sel__mec_copy_data__memory__GFX09                                  =  1,
    src_sel__mec_copy_data__tc_l2                                          =  2,
    src_sel__mec_copy_data__gds                                            =  3,
    src_sel__mec_copy_data__perfcounters                                   =  4,
    src_sel__mec_copy_data__immediate_data                                 =  5,
    src_sel__mec_copy_data__atomic_return_data                             =  6,
    src_sel__mec_copy_data__gds_atomic_return_data0                        =  7,
    src_sel__mec_copy_data__gds_atomic_return_data1                        =  8,
    src_sel__mec_copy_data__gpu_clock_count                                =  9,
    src_sel__mec_copy_data__system_clock_count                             = 10,
};

enum MEC_COPY_DATA_wr_confirm_enum {
    wr_confirm__mec_copy_data__do_not_wait_for_confirmation                =  0,
    wr_confirm__mec_copy_data__wait_for_confirmation                       =  1,
};

typedef struct PM4_MEC_COPY_DATA
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    union
    {
        struct
        {
            MEC_COPY_DATA_src_sel_enum          src_sel : 4;
            uint32_t                          reserved1 : 4;
            MEC_COPY_DATA_dst_sel_enum          dst_sel : 4;
            uint32_t                          reserved2 : 1;
            MEC_COPY_DATA_src_cache_policy_enum src_cache_policy : 2;
            uint32_t                          reserved3 : 1;
            MEC_COPY_DATA_count_sel_enum      count_sel : 1;
            uint32_t                          reserved4 : 3;
            MEC_COPY_DATA_wr_confirm_enum    wr_confirm : 1;
            uint32_t                          reserved5 : 4;
            MEC_COPY_DATA_dst_cache_policy_enum dst_cache_policy : 2;
            uint32_t                          reserved6 : 2;
            MEC_COPY_DATA_pq_exe_status_enum pq_exe_status : 1;
            uint32_t                          reserved7 : 2;
        } bitfields2;
        uint32_t                               ordinal2;
    };

    union
    {
        struct
        {
            uint32_t                     src_reg_offset : 18;
            uint32_t                          reserved1 : 14;
        } bitfields3a;
        struct
        {
            uint32_t                          reserved1 : 2;
            uint32_t                    src_32b_addr_lo : 30;
        } bitfields3b;
        struct
        {
            uint32_t                          reserved1 : 3;
            uint32_t                    src_64b_addr_lo : 29;
        } bitfields3c;
        struct
        {
            uint32_t                    src_gds_addr_lo : 16;
            uint32_t                          reserved1 : 16;
        } bitfields3d;
        uint32_t                               imm_data;
        uint32_t                               ordinal3;
    };

    union
    {
        uint32_t                      src_memtc_addr_hi;
        uint32_t                           src_imm_data;
        uint32_t                               ordinal4;
    };

    union
    {
        struct
        {
            uint32_t                     dst_reg_offset : 18;
            uint32_t                          reserved1 : 14;
        } bitfields5a;
        struct
        {
            uint32_t                          reserved1 : 2;
            uint32_t                    dst_32b_addr_lo : 30;
        } bitfields5b;
        struct
        {
            uint32_t                          reserved1 : 3;
            uint32_t                    dst_64b_addr_lo : 29;
        } bitfields5c;
        struct
        {
            uint32_t                    dst_gds_addr_lo : 16;
            uint32_t                          reserved1 : 16;
        } bitfields5d;
        uint32_t                               ordinal5;
    };

    uint32_t                                dst_addr_hi;

} PM4MEC_COPY_DATA, *PPM4MEC_COPY_DATA;

//--------------------DISPATCH_DIRECT--------------------
typedef struct PM4_MEC_DISPATCH_DIRECT
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    uint32_t                                      dim_x;

    uint32_t                                      dim_y;

    uint32_t                                      dim_z;

    uint32_t                         dispatch_initiator;

} PM4MEC_DISPATCH_DIRECT, *PPM4MEC_DISPATCH_DIRECT;

//--------------------DISPATCH_DRAW_ACE--------------------
typedef struct PM4_MEC_DISPATCH_DRAW_ACE
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    union
    {
        struct
        {
            uint32_t                       krb_loc_sgpr : 4;
            uint32_t                          reserved1 : 28;
        } bitfields2;
        uint32_t                               ordinal2;
    };

    uint32_t                                      dim_x;

    uint32_t                                      dim_y;

    uint32_t                                      dim_z;

    uint32_t                         dispatch_initiator;

} PM4MEC_DISPATCH_DRAW_ACE, *PPM4MEC_DISPATCH_DRAW_ACE;

//--------------------DISPATCH_DRAW_PREAMBLE_ACE--------------------
typedef struct PM4_MEC_DISPATCH_DRAW_PREAMBLE_ACE
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    union
    {
        struct
        {
            uint32_t                           krb_size : 10;
            uint32_t                    krb_free_offset : 10;
            uint32_t                         krb_offset : 10;
            uint32_t                          reserved1 : 2;
        } bitfields2;
        uint32_t                               ordinal2;
    };

} PM4MEC_DISPATCH_DRAW_PREAMBLE_ACE, *PPM4MEC_DISPATCH_DRAW_PREAMBLE_ACE;

//--------------------DISPATCH_INDIRECT--------------------
typedef struct PM4_MEC_DISPATCH_INDIRECT
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    uint32_t                                    addr_lo;

    uint32_t                                    addr_hi;

    uint32_t                         dispatch_initiator;

} PM4MEC_DISPATCH_INDIRECT, *PPM4MEC_DISPATCH_INDIRECT;

//--------------------DMA_DATA--------------------
enum MEC_DMA_DATA_daic_enum {
    daic__mec_dma_data__increment                                          =  0,
    daic__mec_dma_data__no_increment                                       =  1,
};

enum MEC_DMA_DATA_das_enum {
    das__mec_dma_data__memory                                              =  0,
};

enum MEC_DMA_DATA_dst_cache_policy_enum {
    dst_cache_policy__mec_dma_data__lru                                    =  0,
    dst_cache_policy__mec_dma_data__stream                                 =  1,
};

enum MEC_DMA_DATA_dst_sel_enum {
    dst_sel__mec_dma_data__dst_addr_using_das                              =  0,
    dst_sel__mec_dma_data__gds                                             =  1,
    dst_sel__mec_dma_data__dst_nowhere                                     =  2,
    dst_sel__mec_dma_data__dst_addr_using_l2                               =  3,
};

enum MEC_DMA_DATA_saic_enum {
    saic__mec_dma_data__increment                                          =  0,
    saic__mec_dma_data__no_increment                                       =  1,
};

enum MEC_DMA_DATA_sas_enum {
    sas__mec_dma_data__memory                                              =  0,
};

enum MEC_DMA_DATA_src_cache_policy_enum {
    src_cache_policy__mec_dma_data__lru                                    =  0,
    src_cache_policy__mec_dma_data__stream                                 =  1,
};

enum MEC_DMA_DATA_src_sel_enum {
    src_sel__mec_dma_data__src_addr_using_sas                              =  0,
    src_sel__mec_dma_data__gds                                             =  1,
    src_sel__mec_dma_data__data                                            =  2,
    src_sel__mec_dma_data__src_addr_using_l2                               =  3,
};

typedef struct PM4_MEC_DMA_DATA
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    union
    {
        struct
        {
            uint32_t                          reserved1 : 13;
            MEC_DMA_DATA_src_cache_policy_enum src_cache_policy : 2;
            uint32_t                          reserved2 : 5;
            MEC_DMA_DATA_dst_sel_enum           dst_sel : 2;
            uint32_t                          reserved3 : 3;
            MEC_DMA_DATA_dst_cache_policy_enum dst_cache_policy : 2;
            uint32_t                          reserved4 : 2;
            MEC_DMA_DATA_src_sel_enum           src_sel : 2;
            uint32_t                          reserved5 : 1;
        } bitfields2;
        uint32_t                               ordinal2;
    };

    uint32_t                        src_addr_lo_or_data;

    uint32_t                                src_addr_hi;

    uint32_t                                dst_addr_lo;

    uint32_t                                dst_addr_hi;

    union
    {
        struct
        {
            uint32_t                         byte_count : 26;
            MEC_DMA_DATA_sas_enum                   sas : 1;
            MEC_DMA_DATA_das_enum                   das : 1;
            MEC_DMA_DATA_saic_enum                 saic : 1;
            MEC_DMA_DATA_daic_enum                 daic : 1;
            uint32_t                           raw_wait : 1;
            uint32_t                             dis_wc : 1;
        } bitfields7;
        uint32_t                               ordinal7;
    };

    uint32_t                                  reserved6;

} PM4MEC_DMA_DATA, *PPM4MEC_DMA_DATA;

//--------------------DMA_DATA_FILL_MULTI--------------------
enum MEC_DMA_DATA_FILL_MULTI_dst_cache_policy_enum {
    dst_cache_policy__mec_dma_data_fill_multi__lru                         =  0,
    dst_cache_policy__mec_dma_data_fill_multi__stream                      =  1,
};

enum MEC_DMA_DATA_FILL_MULTI_dst_sel_enum {
    dst_sel__mec_dma_data_fill_multi__dst_addr_using_l2                    =  3,
};

enum MEC_DMA_DATA_FILL_MULTI_src_sel_enum {
    src_sel__mec_dma_data_fill_multi__data                                 =  2,
};

typedef struct PM4_MEC_DMA_DATA_FILL_MULTI
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    union
    {
        struct
        {
            uint32_t                          reserved1 : 10;
            uint32_t                       memlog_clear : 1;
            uint32_t                          reserved2 : 9;
            MEC_DMA_DATA_FILL_MULTI_dst_sel_enum dst_sel : 2;
            uint32_t                          reserved3 : 3;
            MEC_DMA_DATA_FILL_MULTI_dst_cache_policy_enum dst_cache_policy : 2;
            uint32_t                          reserved4 : 2;
            MEC_DMA_DATA_FILL_MULTI_src_sel_enum src_sel : 2;
            uint32_t                          reserved5 : 1;
        } bitfields2;
        uint32_t                               ordinal2;
    };

    uint32_t                                byte_stride;

    uint32_t                                  dma_count;

    uint32_t                                dst_addr_lo;

    uint32_t                                dst_addr_hi;

    union
    {
        struct
        {
            uint32_t                         byte_count : 26;
            uint32_t                          reserved1 : 6;
        } bitfields7;
        uint32_t                               ordinal7;
    };

} PM4MEC_DMA_DATA_FILL_MULTI, *PPM4MEC_DMA_DATA_FILL_MULTI;

//--------------------EVENT_WRITE--------------------
enum MEC_EVENT_WRITE_event_index_enum {
    event_index__mec_event_write__other                                    =  0,
    event_index__mec_event_write__sample_pipelinestats                     =  2,
    event_index__mec_event_write__cs_partial_flush                         =  4,
};

typedef struct PM4_MEC_EVENT_WRITE
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    union
    {
        struct
        {
            uint32_t                         event_type : 6;
            uint32_t                          reserved1 : 2;
            MEC_EVENT_WRITE_event_index_enum event_index : 4;
            uint32_t                          reserved2 : 19;
            uint32_t                     offload_enable : 1;
        } bitfields2;
        uint32_t                               ordinal2;
    };

    union
    {
        struct
        {
            uint32_t                          reserved1 : 3;
            uint32_t                         address_lo : 29;
        } bitfields3;
        uint32_t                               ordinal3;
    };

    uint32_t                                 address_hi;

} PM4MEC_EVENT_WRITE, *PPM4MEC_EVENT_WRITE;

//--------------------HDP_FLUSH--------------------
typedef struct PM4_MEC_HDP_FLUSH
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    uint32_t                                      dummy;

} PM4MEC_HDP_FLUSH, *PPM4MEC_HDP_FLUSH;

//--------------------INDIRECT_BUFFER--------------------
enum MEC_INDIRECT_BUFFER_cache_policy_enum {
    cache_policy__mec_indirect_buffer__lru                                 =  0,
    cache_policy__mec_indirect_buffer__stream                              =  1,
};

typedef struct PM4_MEC_INDIRECT_BUFFER
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    union
    {
        struct
        {
            uint32_t                          reserved1 : 2;
            uint32_t                         ib_base_lo : 30;
        } bitfields2;
        uint32_t                               ordinal2;
    };

    uint32_t                                 ib_base_hi;

    union
    {
        struct
        {
            uint32_t                            ib_size : 20;
            uint32_t                              chain : 1;
            uint32_t                    offload_polling : 1;
            uint32_t                          reserved1 : 1;
            uint32_t                              valid : 1;
            uint32_t                               vmid : 4;
            MEC_INDIRECT_BUFFER_cache_policy_enum cache_policy : 2;
            uint32_t                          reserved2 : 1;
            uint32_t                          reserved3 : 1;
        } bitfields4;
        uint32_t                               ordinal4;
    };

} PM4MEC_INDIRECT_BUFFER, *PPM4MEC_INDIRECT_BUFFER;

//--------------------INDIRECT_BUFFER_PASID--------------------
enum MEC_INDIRECT_BUFFER_PASID_cache_policy_enum {
    cache_policy__mec_indirect_buffer_pasid__lru                           =  0,
    cache_policy__mec_indirect_buffer_pasid__stream                        =  1,
};

typedef struct PM4_MEC_INDIRECT_BUFFER_PASID
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    union
    {
        struct
        {
            uint32_t                          reserved1 : 2;
            uint32_t                         ib_base_lo : 30;
        } bitfields2;
        uint32_t                               ordinal2;
    };

    uint32_t                                 ib_base_hi;

    union
    {
        struct
        {
            uint32_t                            ib_size : 20;
            uint32_t                              chain : 1;
            uint32_t                    offload_polling : 1;
            uint32_t                          reserved1 : 1;
            uint32_t                              valid : 1;
            uint32_t                          reserved2 : 4;
            MEC_INDIRECT_BUFFER_PASID_cache_policy_enum cache_policy : 2;
            uint32_t                          reserved3 : 1;
            uint32_t                          reserved4 : 1;
        } bitfields4;
        uint32_t                               ordinal4;
    };

    union
    {
        struct
        {
            uint32_t                              pasid : 16;
            uint32_t                          reserved1 : 16;
        } bitfields5;
        uint32_t                               ordinal5;
    };

} PM4MEC_INDIRECT_BUFFER_PASID, *PPM4MEC_INDIRECT_BUFFER_PASID;

//--------------------INVALIDATE_TLBS--------------------
enum MEC_INVALIDATE_TLBS_invalidate_sel_enum {
    invalidate_sel__mec_invalidate_tlbs__invalidate                        =  0,
    invalidate_sel__mec_invalidate_tlbs__use_pasid                         =  1,
};

enum MEC_INVALIDATE_TLBS_mmhub_invalidate_sel_enum {
    mmhub_invalidate_sel__mec_invalidate_tlbs__do_not_invalidate_mmhub     =  0,
    mmhub_invalidate_sel__mec_invalidate_tlbs__use_mmhub_flush_type        =  1,
    mmhub_invalidate_sel__mec_invalidate_tlbs__use_gfx_flush_type          =  2,
};

typedef struct PM4_MEC_INVALIDATE_TLBS
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    union
    {
        struct
        {
            MEC_INVALIDATE_TLBS_invalidate_sel_enum invalidate_sel : 3;
            MEC_INVALIDATE_TLBS_mmhub_invalidate_sel_enum mmhub_invalidate_sel : 2;
            uint32_t                              pasid : 16;
            uint32_t                          reserved1 : 4;
            uint32_t                   mmhub_flush_type : 3;
            uint32_t                          reserved2 : 1;
            uint32_t                     gfx_flush_type : 3;
        } bitfields2;
        uint32_t                               ordinal2;
    };

} PM4MEC_INVALIDATE_TLBS, *PPM4MEC_INVALIDATE_TLBS;

//--------------------MEM_SEMAPHORE--------------------
enum MEC_MEM_SEMAPHORE_sem_sel_enum {
    sem_sel__mec_mem_semaphore__signal_semaphore                           =  6,
    sem_sel__mec_mem_semaphore__wait_semaphore                             =  7,
};

enum MEC_MEM_SEMAPHORE_signal_type_enum {
    signal_type__mec_mem_semaphore__signal_type_increment                  =  0,
    signal_type__mec_mem_semaphore__signal_type_write                      =  1,
};

enum MEC_MEM_SEMAPHORE_use_mailbox_enum {
    use_mailbox__mec_mem_semaphore__do_not_wait_for_mailbox                =  0,
    use_mailbox__mec_mem_semaphore__wait_for_mailbox                       =  1,
};

typedef struct PM4_MEC_MEM_SEMAPHORE
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    union
    {
        struct
        {
            uint32_t                          reserved1 : 3;
            uint32_t                         address_lo : 29;
        } bitfields2;
        uint32_t                               ordinal2;
    };

    uint32_t                                 address_hi;

    union
    {
        struct
        {
            uint32_t                          reserved1 : 16;
            MEC_MEM_SEMAPHORE_use_mailbox_enum use_mailbox : 1;
            uint32_t                          reserved2 : 3;
            MEC_MEM_SEMAPHORE_signal_type_enum signal_type : 1;
            uint32_t                          reserved3 : 8;
            MEC_MEM_SEMAPHORE_sem_sel_enum      sem_sel : 3;
        } bitfields4;
        uint32_t                               ordinal4;
    };

} PM4MEC_MEM_SEMAPHORE, *PPM4MEC_MEM_SEMAPHORE;

//--------------------NOP--------------------
typedef struct PM4_MEC_NOP
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

} PM4MEC_NOP, *PPM4MEC_NOP;

//--------------------PRIME_UTCL2--------------------
enum MEC_PRIME_UTCL2_cache_perm_enum {
    cache_perm__mec_prime_utcl2__read                                      =  0,
    cache_perm__mec_prime_utcl2__write                                     =  1,
    cache_perm__mec_prime_utcl2__execute                                   =  2,
};

enum MEC_PRIME_UTCL2_prime_mode_enum {
    prime_mode__mec_prime_utcl2__dont_wait_for_xack                        =  0,
    prime_mode__mec_prime_utcl2__wait_for_xack                             =  1,
};

typedef struct PM4_MEC_PRIME_UTCL2
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    union
    {
        struct
        {
            MEC_PRIME_UTCL2_cache_perm_enum  cache_perm : 3;
            MEC_PRIME_UTCL2_prime_mode_enum  prime_mode : 1;
            uint32_t                          reserved1 : 28;
        } bitfields2;
        uint32_t                               ordinal2;
    };

    uint32_t                                    addr_lo;

    uint32_t                                    addr_hi;

    union
    {
        struct
        {
            uint32_t                    requested_pages : 14;
            uint32_t                          reserved1 : 18;
        } bitfields5;
        uint32_t                               ordinal5;
    };

} PM4MEC_PRIME_UTCL2, *PPM4MEC_PRIME_UTCL2;

//--------------------RELEASE_MEM--------------------
enum MEC_RELEASE_MEM_cache_policy_enum {
    cache_policy__mec_release_mem__lru                                     =  0,
    cache_policy__mec_release_mem__stream                                  =  1,
};

enum MEC_RELEASE_MEM_data_sel_enum {
    data_sel__mec_release_mem__none                                        =  0,
    data_sel__mec_release_mem__send_32_bit_low                             =  1,
    data_sel__mec_release_mem__send_64_bit_data                            =  2,
    data_sel__mec_release_mem__send_gpu_clock_counter                      =  3,
    data_sel__mec_release_mem__send_cp_perfcounter_hi_lo                   =  4,
    data_sel__mec_release_mem__store_gds_data_to_memory                    =  5,
};

enum MEC_RELEASE_MEM_dst_sel_enum {
    dst_sel__mec_release_mem__memory_controller                            =  0,
    dst_sel__mec_release_mem__tc_l2                                        =  1,
    dst_sel__mec_release_mem__queue_write_pointer_register                 =  2,
    dst_sel__mec_release_mem__queue_write_pointer_poll_mask_bit            =  3,
};

enum MEC_RELEASE_MEM_event_index_enum {
    event_index__mec_release_mem__end_of_pipe                              =  5,
    event_index__mec_release_mem__shader_done                              =  6,
};

enum MEC_RELEASE_MEM_int_sel_enum {
    int_sel__mec_release_mem__none                                         =  0,
    int_sel__mec_release_mem__send_interrupt_only                          =  1,
    int_sel__mec_release_mem__send_interrupt_after_write_confirm           =  2,
    int_sel__mec_release_mem__send_data_after_write_confirm                =  3,
    int_sel__mec_release_mem__unconditionally_send_int_ctxid               =  4,
    int_sel__mec_release_mem__conditionally_send_int_ctxid_based_on_32_bit_compare =  5,
    int_sel__mec_release_mem__conditionally_send_int_ctxid_based_on_64_bit_compare =  6,
};

enum MEC_RELEASE_MEM_pq_exe_status_enum {
    pq_exe_status__mec_release_mem__default                                =  0,
    pq_exe_status__mec_release_mem__phase_update                           =  1,
};

//--------------------RELEASE_MEM__GFX09--------------------
typedef struct PM4_MEC_RELEASE_MEM__GFX09
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    union
    {
        struct
        {
            uint32_t                         event_type : 6;
            uint32_t                          reserved1 : 2;
            MEC_RELEASE_MEM_event_index_enum event_index : 4;
            uint32_t                tcl1_vol_action_ena : 1;
            uint32_t                  tc_vol_action_ena : 1;
            uint32_t                          reserved2 : 1;
            uint32_t                   tc_wb_action_ena : 1;
            uint32_t                    tcl1_action_ena : 1;
            uint32_t                      tc_action_ena : 1;
            uint32_t                          reserved3 : 1;
            uint32_t                   tc_nc_action_ena : 1;
            uint32_t                   tc_wc_action_ena : 1;
            uint32_t                   tc_md_action_ena : 1;
            uint32_t                          reserved4 : 3;
            MEC_RELEASE_MEM_cache_policy_enum cache_policy : 2;
            uint32_t                          reserved5 : 2;
            MEC_RELEASE_MEM_pq_exe_status_enum pq_exe_status : 1;
            uint32_t                          reserved6 : 2;
        } bitfields2;
        uint32_t                               ordinal2;
    };

    union
    {
        struct
        {
            uint32_t                          reserved1 : 16;
            MEC_RELEASE_MEM_dst_sel_enum        dst_sel : 2;
            uint32_t                          reserved2 : 6;
            MEC_RELEASE_MEM_int_sel_enum        int_sel : 3;
            uint32_t                          reserved3 : 2;
            MEC_RELEASE_MEM_data_sel_enum      data_sel : 3;
        } bitfields3;
        uint32_t                               ordinal3;
    };

    union
    {
        struct
        {
            uint32_t                          reserved1 : 2;
            uint32_t                     address_lo_32b : 30;
        } bitfields4a;
        struct
        {
            uint32_t                          reserved1 : 3;
            uint32_t                     address_lo_64b : 29;
        } bitfields4b;
        uint32_t                             reserved12;
        uint32_t                               ordinal4;
    };

    union
    {
        uint32_t                             address_hi;
        uint32_t                             reserved13;
        uint32_t                               ordinal5;
    };

    union
    {
        struct
        {
            uint32_t                          dw_offset : 16;
            uint32_t                         num_dwords : 16;
        } bitfields6;
        uint32_t                                data_lo;
        uint32_t                            cmp_data_lo;
        uint32_t                             reserved14;
        uint32_t                               ordinal6;
    };

    union
    {
        uint32_t                                data_hi;
        uint32_t                            cmp_data_hi;
        uint32_t                             reserved15;
        uint32_t                             reserved16;
        uint32_t                               ordinal7;
    };

    uint32_t                                  int_ctxid;

} PM4MEC_RELEASE_MEM__GFX09, *PPM4MEC_RELEASE_MEM__GFX09;

//--------------------REWIND--------------------
typedef struct PM4_MEC_REWIND
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    union
    {
        struct
        {
            uint32_t                          reserved1 : 24;
            uint32_t                     offload_enable : 1;
            uint32_t                          reserved2 : 6;
            uint32_t                              valid : 1;
        } bitfields2;
        uint32_t                               ordinal2;
    };

} PM4MEC_REWIND, *PPM4MEC_REWIND;

//--------------------SET_CONFIG_REG--------------------
enum MEC_SET_CONFIG_REG_index_enum {
    index__mec_set_config_reg__default                                     =  0,
    index__mec_set_config_reg__insert_vmid                                 =  1,
};

typedef struct PM4_MEC_SET_CONFIG_REG
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    union
    {
        struct
        {
            uint32_t                         reg_offset : 16;
            uint32_t                          reserved1 : 7;
            uint32_t                         vmid_shift : 5;
            MEC_SET_CONFIG_REG_index_enum         index : 4;
        } bitfields2;
        uint32_t                               ordinal2;
    };

} PM4MEC_SET_CONFIG_REG, *PPM4MEC_SET_CONFIG_REG;

//--------------------SET_QUEUE_REG--------------------
typedef struct PM4_MEC_SET_QUEUE_REG
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    union
    {
        struct
        {
            uint32_t                         reg_offset : 8;
            uint32_t                          reserved1 : 7;
            uint32_t                         defer_exec : 1;
            uint32_t                               vqid : 10;
            uint32_t                          reserved2 : 6;
        } bitfields2;
        uint32_t                               ordinal2;
    };

} PM4MEC_SET_QUEUE_REG, *PPM4MEC_SET_QUEUE_REG;

//--------------------SET_SH_REG--------------------
enum MEC_SET_SH_REG_index_enum {
    index__mec_set_sh_reg__default                                         =  0,
    index__mec_set_sh_reg__insert_vmid                                     =  1,
};

typedef struct PM4_MEC_SET_SH_REG
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    union
    {
        struct
        {
            uint32_t                         reg_offset : 16;
            uint32_t                          reserved1 : 7;
            uint32_t                         vmid_shift : 5;
            MEC_SET_SH_REG_index_enum             index : 4;
        } bitfields2;
        uint32_t                               ordinal2;
    };

} PM4MEC_SET_SH_REG, *PPM4MEC_SET_SH_REG;

//--------------------SET_SH_REG_INDEX--------------------
enum MEC_SET_SH_REG_INDEX_index_enum {
    index__mec_set_sh_reg_index__default                                   =  0,
    index__mec_set_sh_reg_index__insert_vmid__GFX09                        =  1,
    index__mec_set_sh_reg_index__apply_kmd_cu_and_mask                     =  3,
};

typedef struct PM4_MEC_SET_SH_REG_INDEX
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    union
    {
        struct
        {
            uint32_t                         reg_offset : 16;
            uint32_t                          reserved1 : 7;
            uint32_t                  vmid_shift__GFX09 : 5;
        } bitfields2;
        uint32_t                               ordinal2;
    };

} PM4MEC_SET_SH_REG_INDEX, *PPM4MEC_SET_SH_REG_INDEX;

//--------------------SET_UCONFIG_REG--------------------
typedef struct PM4_MEC_SET_UCONFIG_REG
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    union
    {
        struct
        {
            uint32_t                         reg_offset : 16;
            uint32_t                          reserved1 : 16;
        } bitfields2;
        uint32_t                               ordinal2;
    };

} PM4MEC_SET_UCONFIG_REG, *PPM4MEC_SET_UCONFIG_REG;

//--------------------WAIT_ON_CE_COUNTER--------------------
typedef struct PM4_MEC_WAIT_ON_CE_COUNTER
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    union
    {
        struct
        {
            uint32_t                   cond_acquire_mem : 1;
            uint32_t                         force_sync : 1;
            uint32_t                          reserved1 : 25;
            uint32_t                       mem_volatile : 1;
            uint32_t                          reserved2 : 4;
        } bitfields2;
        uint32_t                               ordinal2;
    };

} PM4MEC_WAIT_ON_CE_COUNTER, *PPM4MEC_WAIT_ON_CE_COUNTER;

//--------------------WAIT_REG_MEM--------------------
enum MEC_WAIT_REG_MEM_cache_policy_enum {
};

enum MEC_WAIT_REG_MEM_function_enum {
    function__mec_wait_reg_mem__always_pass                                =  0,
    function__mec_wait_reg_mem__less_than_ref_value                        =  1,
    function__mec_wait_reg_mem__less_than_equal_to_the_ref_value           =  2,
    function__mec_wait_reg_mem__equal_to_the_reference_value               =  3,
    function__mec_wait_reg_mem__not_equal_reference_value                  =  4,
    function__mec_wait_reg_mem__greater_than_or_equal_reference_value      =  5,
    function__mec_wait_reg_mem__greater_than_reference_value               =  6,
};

enum MEC_WAIT_REG_MEM_mem_space_enum {
    mem_space__mec_wait_reg_mem__register_space                            =  0,
    mem_space__mec_wait_reg_mem__memory_space                              =  1,
};

enum MEC_WAIT_REG_MEM_operation_enum {
    operation__mec_wait_reg_mem__wait_reg_mem                              =  0,
    operation__mec_wait_reg_mem__wr_wait_wr_reg                            =  1,
    operation__mec_wait_reg_mem__wait_mem_preemptable                      =  3,
};

typedef struct PM4_MEC_WAIT_REG_MEM
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    union
    {
        struct
        {
            MEC_WAIT_REG_MEM_function_enum     function : 3;
            uint32_t                          reserved1 : 1;
            MEC_WAIT_REG_MEM_mem_space_enum   mem_space : 2;
            MEC_WAIT_REG_MEM_operation_enum   operation : 2;
            uint32_t                          reserved2 : 17;
        } bitfields2;
        uint32_t                               ordinal2;
    };

    union
    {
        struct
        {
            uint32_t                          reserved1 : 2;
            uint32_t                   mem_poll_addr_lo : 30;
        } bitfields3a;
        struct
        {
            uint32_t                      reg_poll_addr : 18;
            uint32_t                          reserved1 : 14;
        } bitfields3b;
        struct
        {
            uint32_t                    reg_write_addr1 : 18;
            uint32_t                          reserved1 : 14;
        } bitfields3c;
        uint32_t                               ordinal3;
    };

    union
    {
        struct
        {
            uint32_t                    reg_write_addr2 : 18;
            uint32_t                          reserved1 : 14;
        } bitfields4;
        uint32_t                       mem_poll_addr_hi;
        uint32_t                               ordinal4;
    };

    uint32_t                                  reference;

    uint32_t                                       mask;

    union
    {
        struct
        {
            uint32_t                      poll_interval : 16;
            uint32_t                          reserved1 : 16;
        } bitfields7;
        uint32_t                               ordinal7;
    };

} PM4MEC_WAIT_REG_MEM, *PPM4MEC_WAIT_REG_MEM;

//--------------------WAIT_REG_MEM64--------------------
enum MEC_WAIT_REG_MEM64_cache_policy_enum {
};

enum MEC_WAIT_REG_MEM64_function_enum {
    function__mec_wait_reg_mem64__always_pass                              =  0,
    function__mec_wait_reg_mem64__less_than_ref_value                      =  1,
    function__mec_wait_reg_mem64__less_than_equal_to_the_ref_value         =  2,
    function__mec_wait_reg_mem64__equal_to_the_reference_value             =  3,
    function__mec_wait_reg_mem64__not_equal_reference_value                =  4,
    function__mec_wait_reg_mem64__greater_than_or_equal_reference_value    =  5,
    function__mec_wait_reg_mem64__greater_than_reference_value             =  6,
};

enum MEC_WAIT_REG_MEM64_mem_space_enum {
    mem_space__mec_wait_reg_mem64__register_space                          =  0,
    mem_space__mec_wait_reg_mem64__memory_space                            =  1,
};

enum MEC_WAIT_REG_MEM64_operation_enum {
    operation__mec_wait_reg_mem64__wait_reg_mem                            =  0,
    operation__mec_wait_reg_mem64__wr_wait_wr_reg                          =  1,
    operation__mec_wait_reg_mem64__wait_mem_preemptable                    =  3,
};

typedef struct PM4_MEC_WAIT_REG_MEM64
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    union
    {
        struct
        {
            MEC_WAIT_REG_MEM64_function_enum   function : 3;
            uint32_t                          reserved1 : 1;
            MEC_WAIT_REG_MEM64_mem_space_enum mem_space : 2;
            MEC_WAIT_REG_MEM64_operation_enum operation : 2;
            uint32_t                          reserved2 : 17;
        } bitfields2;
        uint32_t                               ordinal2;
    };

    union
    {
        struct
        {
            uint32_t                          reserved1 : 3;
            uint32_t                   mem_poll_addr_lo : 29;
        } bitfields3a;
        struct
        {
            uint32_t                      reg_poll_addr : 18;
            uint32_t                          reserved1 : 14;
        } bitfields3b;
        struct
        {
            uint32_t                    reg_write_addr1 : 18;
            uint32_t                          reserved1 : 14;
        } bitfields3c;
        uint32_t                               ordinal3;
    };

    union
    {
        struct
        {
            uint32_t                    reg_write_addr2 : 18;
            uint32_t                          reserved1 : 14;
        } bitfields4;
        uint32_t                       mem_poll_addr_hi;
        uint32_t                               ordinal4;
    };

    uint32_t                                  reference;

    uint32_t                               reference_hi;

    uint32_t                                       mask;

    uint32_t                                    mask_hi;

    union
    {
        struct
        {
            uint32_t                      poll_interval : 16;
            uint32_t                          reserved1 : 16;
        } bitfields9;
        uint32_t                               ordinal9;
    };

} PM4MEC_WAIT_REG_MEM64, *PPM4MEC_WAIT_REG_MEM64;

//--------------------WRITE_DATA--------------------
enum MEC_WRITE_DATA_addr_incr_enum {
    addr_incr__mec_write_data__increment_address                           =  0,
    addr_incr__mec_write_data__do_not_increment_address                    =  1,
};

enum MEC_WRITE_DATA_cache_policy_enum {
    cache_policy__mec_write_data__lru                                      =  0,
    cache_policy__mec_write_data__stream                                   =  1,
};

enum MEC_WRITE_DATA_dst_sel_enum {
    dst_sel__mec_write_data__mem_mapped_register                           =  0,
    dst_sel__mec_write_data__tc_l2                                         =  2,
    dst_sel__mec_write_data__gds                                           =  3,
    dst_sel__mec_write_data__memory                                        =  5,
    dst_sel__mec_write_data__memory_mapped_adc_persistent_state            =  6,
};

enum MEC_WRITE_DATA_wr_confirm_enum {
    wr_confirm__mec_write_data__do_not_wait_for_write_confirmation         =  0,
    wr_confirm__mec_write_data__wait_for_write_confirmation                =  1,
};

typedef struct PM4_MEC_WRITE_DATA
{
    union
    {
        PM4_MEC_TYPE_3_HEADER                    header;
        uint32_t                               ordinal1;
    };

    union
    {
        struct
        {
            uint32_t                          reserved1 : 8;
            MEC_WRITE_DATA_dst_sel_enum         dst_sel : 4;
            uint32_t                          reserved2 : 4;
            MEC_WRITE_DATA_addr_incr_enum     addr_incr : 1;
            uint32_t                          reserved3 : 2;
            uint32_t                          resume_vf : 1;
            MEC_WRITE_DATA_wr_confirm_enum   wr_confirm : 1;
            uint32_t                          reserved4 : 4;
            MEC_WRITE_DATA_cache_policy_enum cache_policy : 2;
            uint32_t                          reserved5 : 5;
        } bitfields2;
        uint32_t                               ordinal2;
    };

    union
    {
        struct
        {
            uint32_t                     dst_mmreg_addr : 18;
            uint32_t                          reserved1 : 14;
        } bitfields3a;
        struct
        {
            uint32_t                       dst_gds_addr : 16;
            uint32_t                          reserved1 : 16;
        } bitfields3b;
        struct
        {
            uint32_t                          reserved1 : 2;
            uint32_t                    dst_mem_addr_lo : 30;
        } bitfields3c;
        uint32_t                               ordinal3;
    };

    uint32_t                            dst_mem_addr_hi;

} PM4MEC_WRITE_DATA, *PPM4MEC_WRITE_DATA;

