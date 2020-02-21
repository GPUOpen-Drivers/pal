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
    uint32_t u32All[4];

};

constexpr uint32_t SqBufRsrcTWord0BaseAddressShift                                          =  0;
constexpr uint32_t SqBufRsrcTWord1StrideShift                                               = 16;
constexpr uint32_t SqBufRsrcTWord1CacheSwizzleShift                                         = 30;
constexpr uint32_t SqBufRsrcTWord1SwizzleEnableShift                                        = 31;
constexpr uint32_t SqBufRsrcTWord2NumRecordsShift                                           =  0;
constexpr uint32_t SqBufRsrcTWord3DstSelXShift                                              =  0;
constexpr uint32_t SqBufRsrcTWord3DstSelYShift                                              =  3;
constexpr uint32_t SqBufRsrcTWord3DstSelZShift                                              =  6;
constexpr uint32_t SqBufRsrcTWord3DstSelWShift                                              =  9;
constexpr uint32_t SqBufRsrcTWord3IndexStrideShift                                          = 21;
constexpr uint32_t SqBufRsrcTWord3AddTidEnableShift                                         = 23;
constexpr uint32_t SqBufRsrcTWord3OobSelectShift                                            = 28;
constexpr uint32_t SqBufRsrcTWord3TypeShift                                                 = 30;
constexpr uint32_t MostSqBufRsrcTWord3FormatShift                                           = 12;
constexpr uint32_t MostSqBufRsrcTWord3ResourceLevelShift                                    = 24;

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
        uint64_t _reserved_206_203                                            :  4;
        uint64_t max_uncompressed_block_size                                  :  2;
        uint64_t max_compressed_block_size                                    :  2;
        uint64_t meta_pipe_aligned                                            :  1;
        uint64_t write_compress_enable                                        :  1;
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
        uint64_t                                                              : 56;
    } most;
    uint64_t u64All[4];
    uint32_t u32All[8];

};

constexpr uint32_t SqImgRsrcTWord0BaseAddressShift                                          =  0;
constexpr uint32_t SqImgRsrcTWord1WidthShift                                                = 30;
constexpr uint32_t SqImgRsrcTWord2HeightShift                                               = 14;
constexpr uint32_t SqImgRsrcTWord3DstSelXShift                                              =  0;
constexpr uint32_t SqImgRsrcTWord3DstSelYShift                                              =  3;
constexpr uint32_t SqImgRsrcTWord3DstSelZShift                                              =  6;
constexpr uint32_t SqImgRsrcTWord3DstSelWShift                                              =  9;
constexpr uint32_t SqImgRsrcTWord3BaseLevelShift                                            = 12;
constexpr uint32_t SqImgRsrcTWord3LastLevelShift                                            = 16;
constexpr uint32_t SqImgRsrcTWord3SwModeShift                                               = 20;
constexpr uint32_t SqImgRsrcTWord3BcSwizzleShift                                            = 25;
constexpr uint32_t SqImgRsrcTWord3TypeShift                                                 = 28;
constexpr uint32_t SqImgRsrcTWord4DepthShift                                                =  0;
constexpr uint32_t SqImgRsrcTWord4BaseArrayShift                                            = 16;
constexpr uint32_t SqImgRsrcTWord5PerfModShift                                              = 20;
constexpr uint32_t SqImgRsrcTWord5CornerSamplesShift                                        = 23;
constexpr uint32_t SqImgRsrcTWord6Iterate256Shift                                           = 10;
constexpr uint32_t SqImgRsrcTWord6Reserved206203Shift                                       = 11;
constexpr uint32_t SqImgRsrcTWord6MaxUncompressedBlockSizeShift                             = 15;
constexpr uint32_t SqImgRsrcTWord6MaxCompressedBlockSizeShift                               = 17;
constexpr uint32_t SqImgRsrcTWord6MetaPipeAlignedShift                                      = 19;
constexpr uint32_t SqImgRsrcTWord6WriteCompressEnableShift                                  = 20;
constexpr uint32_t SqImgRsrcTWord6CompressionEnShift                                        = 21;
constexpr uint32_t SqImgRsrcTWord6AlphaIsOnMsbShift                                         = 22;
constexpr uint32_t SqImgRsrcTWord6ColorTransformShift                                       = 23;
constexpr uint32_t SqImgRsrcTWord6MetaDataAddressShift                                      = 24;
constexpr uint32_t MostSqImgRsrcTWord1MinLodShift                                           =  8;
constexpr uint32_t MostSqImgRsrcTWord1FormatShift                                           = 20;
constexpr uint32_t MostSqImgRsrcTWord2ResourceLevelShift                                    = 31;
constexpr uint32_t MostSqImgRsrcTWord5ArrayPitchShift                                       =  0;
constexpr uint32_t MostSqImgRsrcTWord5MaxMipShift                                           =  4;
constexpr uint32_t MostSqImgRsrcTWord5MinLodWarnShift                                       =  8;
constexpr uint32_t MostSqImgRsrcTWord5LodHdwCntEnShift                                      = 25;
constexpr uint32_t MostSqImgRsrcTWord5PrtDefaultShift                                       = 26;
constexpr uint32_t MostSqImgRsrcTWord5BigPageShift                                          = 31;
constexpr uint32_t MostSqImgRsrcTWord6CounterBankIdShift                                    =  0;

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
    uint32_t u32All[4];

};

constexpr uint32_t SqImgSampDerivAdjustTWord0ClampXShift                                    =  0;
constexpr uint32_t SqImgSampDerivAdjustTWord0ClampYShift                                    =  3;
constexpr uint32_t SqImgSampDerivAdjustTWord0ClampZShift                                    =  6;
constexpr uint32_t SqImgSampDerivAdjustTWord0MaxAnisoRatioShift                             =  9;
constexpr uint32_t SqImgSampDerivAdjustTWord0DepthCompareFuncShift                          = 12;
constexpr uint32_t SqImgSampDerivAdjustTWord0ForceUnnormalizedShift                         = 15;
constexpr uint32_t SqImgSampDerivAdjustTWord0AnisoThresholdShift                            = 16;
constexpr uint32_t SqImgSampDerivAdjustTWord0McCoordTruncShift                              = 19;
constexpr uint32_t SqImgSampDerivAdjustTWord0ForceDegammaShift                              = 20;
constexpr uint32_t SqImgSampDerivAdjustTWord0AnisoBiasShift                                 = 21;
constexpr uint32_t SqImgSampDerivAdjustTWord0TruncCoordShift                                = 27;
constexpr uint32_t SqImgSampDerivAdjustTWord0DisableCubeWrapShift                           = 28;
constexpr uint32_t SqImgSampDerivAdjustTWord0FilterModeShift                                = 29;
constexpr uint32_t SqImgSampDerivAdjustTWord0SkipDegammaShift                               = 31;
constexpr uint32_t SqImgSampDerivAdjustTWord1MinLodShift                                    =  0;
constexpr uint32_t SqImgSampDerivAdjustTWord1MaxLodShift                                    = 12;
constexpr uint32_t SqImgSampDerivAdjustTWord1PerfMipShift                                   = 24;
constexpr uint32_t SqImgSampDerivAdjustTWord1PerfZShift                                     = 28;
constexpr uint32_t SqImgSampDerivAdjustTWord2BorderColorPtrShift                            =  0;
constexpr uint32_t SqImgSampDerivAdjustTWord2BorderColorTypeShift                           = 12;
constexpr uint32_t SqImgSampDerivAdjustTWord2LodBiasSecShift                                = 14;
constexpr uint32_t SqImgSampDerivAdjustTWord2XyMagFilterShift                               = 20;
constexpr uint32_t SqImgSampDerivAdjustTWord2XyMinFilterShift                               = 22;
constexpr uint32_t SqImgSampDerivAdjustTWord2ZFilterShift                                   = 24;
constexpr uint32_t SqImgSampDerivAdjustTWord2MipFilterShift                                 = 26;
constexpr uint32_t SqImgSampDerivAdjustTWord2MipPointPreclampShift                          = 28;
constexpr uint32_t SqImgSampDerivAdjustTWord2AnisoOverrideShift                             = 29;
constexpr uint32_t SqImgSampDerivAdjustTWord2BlendPrtShift                                  = 30;
constexpr uint32_t SqImgSampDerivAdjustTWord2DerivAdjustEnShift                             = 31;
constexpr uint32_t SqImgSampDerivAdjustTWord3DerivAdjustValuesShift                         =  0;

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
    uint32_t u32All[4];

};

constexpr uint32_t SqImgSampTWord0ClampXShift                                               =  0;
constexpr uint32_t SqImgSampTWord0ClampYShift                                               =  3;
constexpr uint32_t SqImgSampTWord0ClampZShift                                               =  6;
constexpr uint32_t SqImgSampTWord0MaxAnisoRatioShift                                        =  9;
constexpr uint32_t SqImgSampTWord0DepthCompareFuncShift                                     = 12;
constexpr uint32_t SqImgSampTWord0ForceUnnormalizedShift                                    = 15;
constexpr uint32_t SqImgSampTWord0AnisoThresholdShift                                       = 16;
constexpr uint32_t SqImgSampTWord0McCoordTruncShift                                         = 19;
constexpr uint32_t SqImgSampTWord0ForceDegammaShift                                         = 20;
constexpr uint32_t SqImgSampTWord0AnisoBiasShift                                            = 21;
constexpr uint32_t SqImgSampTWord0TruncCoordShift                                           = 27;
constexpr uint32_t SqImgSampTWord0DisableCubeWrapShift                                      = 28;
constexpr uint32_t SqImgSampTWord0SkipDegammaShift                                          = 31;
constexpr uint32_t SqImgSampTWord1MinLodShift                                               =  0;
constexpr uint32_t SqImgSampTWord1MaxLodShift                                               = 12;
constexpr uint32_t SqImgSampTWord1PerfMipShift                                              = 24;
constexpr uint32_t SqImgSampTWord1PerfZShift                                                = 28;
constexpr uint32_t SqImgSampTWord2LodBiasShift                                              =  0;
constexpr uint32_t SqImgSampTWord2LodBiasSecShift                                           = 14;
constexpr uint32_t SqImgSampTWord2XyMagFilterShift                                          = 20;
constexpr uint32_t SqImgSampTWord2XyMinFilterShift                                          = 22;
constexpr uint32_t SqImgSampTWord2ZFilterShift                                              = 24;
constexpr uint32_t SqImgSampTWord2MipFilterShift                                            = 26;
constexpr uint32_t SqImgSampTWord2AnisoOverrideShift                                        = 29;
constexpr uint32_t SqImgSampTWord3BorderColorTypeShift                                      = 30;
constexpr uint32_t MostSqImgSampTWord0FilterModeShift                                       = 29;
constexpr uint32_t MostSqImgSampTWord2MipPointPreclampShift                                 = 28;
constexpr uint32_t MostSqImgSampTWord2BlendPrtShift                                         = 30;
constexpr uint32_t MostSqImgSampTWord2DerivAdjustEnShift                                    = 31;
constexpr uint32_t MostSqImgSampTWord3BorderColorPtrShift                                   =  0;

