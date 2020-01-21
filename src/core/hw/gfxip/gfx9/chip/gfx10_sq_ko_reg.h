/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

union sq_buf_rsrc_t {
    struct {
        uint64_t base_address                                                 : 48;
        uint64_t stride                                                       : 14;
        uint64_t cache_swizzle                                                :  1;
        uint64_t swizzle_enable                                               :  1;
        uint64_t num_records                                                  : 32;
        uint64_t dst_sel_x                                                    :  3;
        uint64_t dst_sel_y                                                    :  3;
        uint64_t dst_sel_z                                                    :  3;
        uint64_t dst_sel_w                                                    :  3;
        uint64_t                                                              :  9;
        uint64_t index_stride                                                 :  2;
        uint64_t add_tid_enable                                               :  1;
        uint64_t                                                              :  4;
        uint64_t oob_select                                                   :  2;
        uint64_t type                                                         :  2;
    };
    struct {
        uint64_t                                                              : 64;
        uint64_t                                                              : 44;
        uint64_t format                                                       :  7;
        uint64_t                                                              :  5;
        uint64_t resource_level                                               :  1;
        uint64_t                                                              :  7;
    } most;
    uint64_t u64All[2];

};

union sq_img_rsrc_t {
    struct {
        uint64_t base_address                                                 : 40;
        uint64_t                                                              : 22;
        uint64_t width_lo                                                     :  2;
        uint64_t width_hi                                                     : 14;
        uint64_t height                                                       : 16;
        uint64_t                                                              :  2;
        uint64_t dst_sel_x                                                    :  3;
        uint64_t dst_sel_y                                                    :  3;
        uint64_t dst_sel_z                                                    :  3;
        uint64_t dst_sel_w                                                    :  3;
        uint64_t base_level                                                   :  4;
        uint64_t last_level                                                   :  4;
        uint64_t sw_mode                                                      :  5;
        uint64_t bc_swizzle                                                   :  3;
        uint64_t type                                                         :  4;
        uint64_t depth                                                        : 16;
        uint64_t base_array                                                   : 16;
        uint64_t                                                              : 20;
        uint64_t perf_mod                                                     :  3;
        uint64_t corner_samples                                               :  1;
        uint64_t                                                              :  8;
        uint64_t                                                              : 10;
        uint64_t iterate_256                                                  :  1;
        uint64_t                                                              :  6;
        uint64_t max_compressed_block_size                                    :  2;
        uint64_t meta_pipe_aligned                                            :  1;
        uint64_t                                                              :  1;
        uint64_t compression_en                                               :  1;
        uint64_t alpha_is_on_msb                                              :  1;
        uint64_t color_transform                                              :  1;
        uint64_t meta_data_address                                            : 40;
    };
    struct {
        uint64_t                                                              : 40;
        uint64_t min_lod                                                      : 12;
        uint64_t format                                                       :  9;
        uint64_t                                                              :  3;
        uint64_t                                                              : 31;
        uint64_t resource_level                                               :  1;
        uint64_t                                                              : 32;
        uint64_t                                                              : 32;
        uint64_t array_pitch                                                  :  4;
        uint64_t max_mip                                                      :  4;
        uint64_t min_lod_warn                                                 : 12;
        uint64_t                                                              :  5;
        uint64_t lod_hdw_cnt_en                                               :  1;
        uint64_t prt_default                                                  :  1;
        uint64_t                                                              :  4;
        uint64_t big_page                                                     :  1;
        uint64_t counter_bank_id                                              :  8;
        uint64_t                                                              :  3;
        uint64_t _reserved_206_203                                            :  4;
        uint64_t max_uncompressed_block_size                                  :  2;
        uint64_t                                                              :  3;
        uint64_t write_compress_enable                                        :  1;
        uint64_t                                                              : 43;
    } most;
    uint64_t u64All[4];

};

union sq_img_samp_deriv_adjust_t {
    struct {
        uint64_t clamp_x                                                      :  3;
        uint64_t clamp_y                                                      :  3;
        uint64_t clamp_z                                                      :  3;
        uint64_t max_aniso_ratio                                              :  3;
        uint64_t depth_compare_func                                           :  3;
        uint64_t force_unnormalized                                           :  1;
        uint64_t aniso_threshold                                              :  3;
        uint64_t mc_coord_trunc                                               :  1;
        uint64_t force_degamma                                                :  1;
        uint64_t aniso_bias                                                   :  6;
        uint64_t trunc_coord                                                  :  1;
        uint64_t disable_cube_wrap                                            :  1;
        uint64_t filter_mode                                                  :  2;
        uint64_t skip_degamma                                                 :  1;
        uint64_t min_lod                                                      : 12;
        uint64_t max_lod                                                      : 12;
        uint64_t perf_mip                                                     :  4;
        uint64_t perf_z                                                       :  4;
        uint64_t border_color_ptr                                             : 12;
        uint64_t border_color_type                                            :  2;
        uint64_t lod_bias_sec                                                 :  6;
        uint64_t xy_mag_filter                                                :  2;
        uint64_t xy_min_filter                                                :  2;
        uint64_t z_filter                                                     :  2;
        uint64_t mip_filter                                                   :  2;
        uint64_t mip_point_preclamp                                           :  1;
        uint64_t aniso_override                                               :  1;
        uint64_t blend_prt                                                    :  1;
        uint64_t deriv_adjust_en                                              :  1;
        uint64_t deriv_adjust_values                                          : 32;
    };
    uint64_t u64All[2];

};

union sq_img_samp_t {
    struct {
        uint64_t clamp_x                                                      :  3;
        uint64_t clamp_y                                                      :  3;
        uint64_t clamp_z                                                      :  3;
        uint64_t max_aniso_ratio                                              :  3;
        uint64_t depth_compare_func                                           :  3;
        uint64_t force_unnormalized                                           :  1;
        uint64_t aniso_threshold                                              :  3;
        uint64_t mc_coord_trunc                                               :  1;
        uint64_t force_degamma                                                :  1;
        uint64_t aniso_bias                                                   :  6;
        uint64_t trunc_coord                                                  :  1;
        uint64_t disable_cube_wrap                                            :  1;
        uint64_t                                                              :  2;
        uint64_t skip_degamma                                                 :  1;
        uint64_t min_lod                                                      : 12;
        uint64_t max_lod                                                      : 12;
        uint64_t perf_mip                                                     :  4;
        uint64_t perf_z                                                       :  4;
        uint64_t lod_bias                                                     : 14;
        uint64_t lod_bias_sec                                                 :  6;
        uint64_t xy_mag_filter                                                :  2;
        uint64_t xy_min_filter                                                :  2;
        uint64_t z_filter                                                     :  2;
        uint64_t mip_filter                                                   :  2;
        uint64_t                                                              :  1;
        uint64_t aniso_override                                               :  1;
        uint64_t                                                              : 32;
        uint64_t border_color_type                                            :  2;
    };
    struct {
        uint64_t                                                              : 29;
        uint64_t filter_mode                                                  :  2;
        uint64_t                                                              : 33;
        uint64_t                                                              : 28;
        uint64_t mip_point_preclamp                                           :  1;
        uint64_t                                                              :  1;
        uint64_t blend_prt                                                    :  1;
        uint64_t deriv_adjust_en                                              :  1;
        uint64_t border_color_ptr                                             : 12;
        uint64_t                                                              : 20;
    } most;
    uint64_t u64All[2];

};

