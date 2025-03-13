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

#include <stdint.h>

namespace Pal
{
namespace Gfx12
{
inline namespace Chip
{
union sq_buf_rsrc_t {
    struct {
        uint64_t base_address                                                 : 48;
        uint64_t stride                                                       : 14;
        uint64_t swizzle_enable                                               :  2;
        uint64_t num_records                                                  : 32;
        uint64_t dst_sel_x                                                    :  3;
        uint64_t dst_sel_y                                                    :  3;
        uint64_t dst_sel_z                                                    :  3;
        uint64_t dst_sel_w                                                    :  3;
        uint64_t format                                                       :  6;
        uint64_t stride_scale                                                 :  2;
        uint64_t                                                              :  1;
        uint64_t index_stride                                                 :  2;
        uint64_t add_tid_enable                                               :  1;
        uint64_t write_compress_enable                                        :  1;
        uint64_t compression_en                                               :  1;
        uint64_t compression_access_mode                                      :  2;
        uint64_t oob_select                                                   :  2;
        uint64_t type                                                         :  2;
    };
    uint64_t u64All[2];
    uint32_t u32All[4];
};

constexpr uint32_t SqBufRsrcTWord0BaseAddressShift                                          =  0;
constexpr uint32_t SqBufRsrcTWord1StrideShift                                               = 16;
constexpr uint32_t SqBufRsrcTWord1SwizzleEnableShift                                        = 30;
constexpr uint32_t SqBufRsrcTWord2NumRecordsShift                                           =  0;
constexpr uint32_t SqBufRsrcTWord3DstSelXShift                                              =  0;
constexpr uint32_t SqBufRsrcTWord3DstSelYShift                                              =  3;
constexpr uint32_t SqBufRsrcTWord3DstSelZShift                                              =  6;
constexpr uint32_t SqBufRsrcTWord3DstSelWShift                                              =  9;
constexpr uint32_t SqBufRsrcTWord3FormatShift                                               = 12;
constexpr uint32_t SqBufRsrcTWord3StrideScaleShift                                          = 18;
constexpr uint32_t SqBufRsrcTWord3IndexStrideShift                                          = 21;
constexpr uint32_t SqBufRsrcTWord3AddTidEnableShift                                         = 23;
constexpr uint32_t SqBufRsrcTWord3WriteCompressEnableShift                                  = 24;
constexpr uint32_t SqBufRsrcTWord3CompressionEnShift                                        = 25;
constexpr uint32_t SqBufRsrcTWord3CompressionAccessModeShift                                = 26;
constexpr uint32_t SqBufRsrcTWord3OobSelectShift                                            = 28;
constexpr uint32_t SqBufRsrcTWord3TypeShift                                                 = 30;

union sq_bvh_rsrc_t {
    struct {
        uint64_t base_address                                                 : 40;
        uint64_t                                                              : 12;
        uint64_t sort_triangles_first                                         :  1;
        uint64_t box_sorting_heuristic                                        :  2;
        uint64_t box_grow_value                                               :  8;
        uint64_t box_sort_en                                                  :  1;
        uint64_t size                                                         : 42;
        uint64_t                                                              :  9;
        uint64_t compressed_format_en                                         :  1;
        uint64_t box_node_64b                                                 :  1;
        uint64_t wide_sort_en                                                 :  1;
        uint64_t instance_en                                                  :  1;
        uint64_t pointer_flags                                                :  1;
        uint64_t triangle_return_mode                                         :  1;
        uint64_t                                                              :  3;
        uint64_t type                                                         :  4;
    };
    uint64_t u64All[2];
    uint32_t u32All[4];
};

constexpr uint32_t SqBvhRsrcTWord0BaseAddressShift                                          =  0;
constexpr uint32_t SqBvhRsrcTWord1SortTrianglesFirstShift                                   = 20;
constexpr uint32_t SqBvhRsrcTWord1BoxSortingHeuristicShift                                  = 21;
constexpr uint32_t SqBvhRsrcTWord1BoxGrowValueShift                                         = 23;
constexpr uint32_t SqBvhRsrcTWord1BoxSortEnShift                                            = 31;
constexpr uint32_t SqBvhRsrcTWord2SizeShift                                                 =  0;
constexpr uint32_t SqBvhRsrcTWord3CompressedFormatEnShift                                   = 19;
constexpr uint32_t SqBvhRsrcTWord3BoxNode64BShift                                           = 20;
constexpr uint32_t SqBvhRsrcTWord3WideSortEnShift                                           = 21;
constexpr uint32_t SqBvhRsrcTWord3InstanceEnShift                                           = 22;
constexpr uint32_t SqBvhRsrcTWord3PointerFlagsShift                                         = 23;
constexpr uint32_t SqBvhRsrcTWord3TriangleReturnModeShift                                   = 24;
constexpr uint32_t SqBvhRsrcTWord3TypeShift                                                 = 28;

union sq_img_rsrc_linked_rsrc_t {
    struct {
        uint64_t base_address                                                 : 40;
        uint64_t                                                              :  4;
        uint64_t max_mip                                                      :  5;
        uint64_t format                                                       :  8;
        uint64_t base_level                                                   :  5;
        uint64_t width_lo                                                     :  2;
        uint64_t width_hi                                                     : 14;
        uint64_t height                                                       : 16;
        uint64_t                                                              :  2;
        uint64_t dst_sel_x                                                    :  3;
        uint64_t dst_sel_y                                                    :  3;
        uint64_t dst_sel_z                                                    :  3;
        uint64_t dst_sel_w                                                    :  3;
        uint64_t no_edge_clamp                                                :  1;
        uint64_t                                                              :  2;
        uint64_t last_level                                                   :  5;
        uint64_t sw_mode                                                      :  5;
        uint64_t linked_resource_type                                         :  3;
        uint64_t type                                                         :  4;
        uint64_t depth                                                        : 14;
        uint64_t pitch_msb                                                    :  2;
        uint64_t base_array                                                   : 13;
        uint64_t base_array_msb                                               :  1;
        uint64_t                                                              :  6;
        uint64_t uav3d                                                        :  1;
        uint64_t depth_scale                                                  :  5;
        uint64_t height_scale                                                 :  5;
        uint64_t width_scale                                                  :  5;
        uint64_t perf_mod                                                     :  3;
        uint64_t corner_samples                                               :  1;
        uint64_t linked_resource                                              :  1;
        uint64_t                                                              :  1;
        uint64_t min_lod_lo                                                   :  6;
        uint64_t min_lod_hi                                                   :  7;
        uint64_t                                                              :  3;
        uint64_t iterate_256                                                  :  1;
        uint64_t sample_pattern_offset                                        :  4;
        uint64_t max_uncompressed_block_size                                  :  1;
        uint64_t                                                              :  1;
        uint64_t max_compressed_block_size                                    :  2;
        uint64_t                                                              :  1;
        uint64_t write_compress_enable                                        :  1;
        uint64_t compression_en                                               :  1;
        uint64_t compression_access_mode                                      :  2;
        uint64_t speculative_read                                             :  2;
        uint64_t                                                              :  6;
    };
    uint64_t u64All[4];
    uint32_t u32All[7];
};

constexpr uint32_t SqImgRsrcLinkedRsrcTWord0BaseAddressShift                                =  0;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord1MaxMipShift                                     = 12;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord1FormatShift                                     = 17;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord1BaseLevelShift                                  = 25;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord1WidthShift                                      = 30;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord2HeightShift                                     = 14;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord3DstSelXShift                                    =  0;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord3DstSelYShift                                    =  3;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord3DstSelZShift                                    =  6;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord3DstSelWShift                                    =  9;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord3NoEdgeClampShift                                = 12;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord3LastLevelShift                                  = 15;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord3SwModeShift                                     = 20;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord3LinkedResourceTypeShift                         = 25;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord3TypeShift                                       = 28;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord4DepthShift                                      =  0;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord4PitchMsbShift                                   = 14;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord4BaseArrayShift                                  = 16;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord4BaseArrayMsbShift                               = 29;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord5Uav3DShift                                      =  4;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord5DepthScaleShift                                 =  5;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord5HeightScaleShift                                = 10;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord5WidthScaleShift                                 = 15;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord5PerfModShift                                    = 20;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord5CornerSamplesShift                              = 23;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord5LinkedResourceShift                             = 24;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord5MinLodShift                                     = 26;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord6Iterate256Shift                                 = 10;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord6SamplePatternOffsetShift                        = 11;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord6MaxUncompressedBlockSizeShift                   = 15;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord6MaxCompressedBlockSizeShift                     = 17;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord6WriteCompressEnableShift                        = 20;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord6CompressionEnShift                              = 21;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord6CompressionAccessModeShift                      = 22;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord6SpeculativeReadShift                            = 24;

union sq_img_rsrc_t {
    struct {
        uint64_t base_address                                                 : 40;
        uint64_t                                                              :  4;
        uint64_t max_mip                                                      :  5;
        uint64_t format                                                       :  8;
        uint64_t base_level                                                   :  5;
        uint64_t width_lo                                                     :  2;
        uint64_t width_hi                                                     : 14;
        uint64_t height                                                       : 16;
        uint64_t                                                              :  2;
        uint64_t dst_sel_x                                                    :  3;
        uint64_t dst_sel_y                                                    :  3;
        uint64_t dst_sel_z                                                    :  3;
        uint64_t dst_sel_w                                                    :  3;
        uint64_t no_edge_clamp                                                :  1;
        uint64_t                                                              :  2;
        uint64_t last_level                                                   :  5;
        uint64_t sw_mode                                                      :  5;
        uint64_t bc_swizzle                                                   :  3;
        uint64_t type                                                         :  4;
        uint64_t depth                                                        : 14;
        uint64_t pitch_msb                                                    :  2;
        uint64_t base_array                                                   : 13;
        uint64_t base_array_msb                                               :  1;
        uint64_t                                                              :  6;
        uint64_t uav3d                                                        :  1;
        uint64_t min_lod_warn                                                 : 13;
        uint64_t                                                              :  2;
        uint64_t perf_mod                                                     :  3;
        uint64_t corner_samples                                               :  1;
        uint64_t linked_resource                                              :  1;
        uint64_t                                                              :  1;
        uint64_t min_lod_lo                                                   :  6;
        uint64_t min_lod_hi                                                   :  7;
        uint64_t                                                              :  3;
        uint64_t iterate_256                                                  :  1;
        uint64_t sample_pattern_offset                                        :  4;
        uint64_t max_uncompressed_block_size                                  :  1;
        uint64_t                                                              :  1;
        uint64_t max_compressed_block_size                                    :  2;
        uint64_t                                                              :  1;
        uint64_t write_compress_enable                                        :  1;
        uint64_t compression_en                                               :  1;
        uint64_t compression_access_mode                                      :  2;
        uint64_t speculative_read                                             :  2;
        uint64_t                                                              :  6;
    };
    uint64_t u64All[4];
    uint32_t u32All[7];
};

constexpr uint32_t SqImgRsrcTWord0BaseAddressShift                                          =  0;
constexpr uint32_t SqImgRsrcTWord1MaxMipShift                                               = 12;
constexpr uint32_t SqImgRsrcTWord1FormatShift                                               = 17;
constexpr uint32_t SqImgRsrcTWord1BaseLevelShift                                            = 25;
constexpr uint32_t SqImgRsrcTWord1WidthShift                                                = 30;
constexpr uint32_t SqImgRsrcTWord2HeightShift                                               = 14;
constexpr uint32_t SqImgRsrcTWord3DstSelXShift                                              =  0;
constexpr uint32_t SqImgRsrcTWord3DstSelYShift                                              =  3;
constexpr uint32_t SqImgRsrcTWord3DstSelZShift                                              =  6;
constexpr uint32_t SqImgRsrcTWord3DstSelWShift                                              =  9;
constexpr uint32_t SqImgRsrcTWord3NoEdgeClampShift                                          = 12;
constexpr uint32_t SqImgRsrcTWord3LastLevelShift                                            = 15;
constexpr uint32_t SqImgRsrcTWord3SwModeShift                                               = 20;
constexpr uint32_t SqImgRsrcTWord3BcSwizzleShift                                            = 25;
constexpr uint32_t SqImgRsrcTWord3TypeShift                                                 = 28;
constexpr uint32_t SqImgRsrcTWord4DepthShift                                                =  0;
constexpr uint32_t SqImgRsrcTWord4PitchMsbShift                                             = 14;
constexpr uint32_t SqImgRsrcTWord4BaseArrayShift                                            = 16;
constexpr uint32_t SqImgRsrcTWord4BaseArrayMsbShift                                         = 29;
constexpr uint32_t SqImgRsrcTWord5Uav3DShift                                                =  4;
constexpr uint32_t SqImgRsrcTWord5MinLodWarnShift                                           =  5;
constexpr uint32_t SqImgRsrcTWord5PerfModShift                                              = 20;
constexpr uint32_t SqImgRsrcTWord5CornerSamplesShift                                        = 23;
constexpr uint32_t SqImgRsrcTWord5LinkedResourceShift                                       = 24;
constexpr uint32_t SqImgRsrcTWord5MinLodShift                                               = 26;
constexpr uint32_t SqImgRsrcTWord6Iterate256Shift                                           = 10;
constexpr uint32_t SqImgRsrcTWord6SamplePatternOffsetShift                                  = 11;
constexpr uint32_t SqImgRsrcTWord6MaxUncompressedBlockSizeShift                             = 15;
constexpr uint32_t SqImgRsrcTWord6MaxCompressedBlockSizeShift                               = 17;
constexpr uint32_t SqImgRsrcTWord6WriteCompressEnableShift                                  = 20;
constexpr uint32_t SqImgRsrcTWord6CompressionEnShift                                        = 21;
constexpr uint32_t SqImgRsrcTWord6CompressionAccessModeShift                                = 22;
constexpr uint32_t SqImgRsrcTWord6SpeculativeReadShift                                      = 24;

union sq_img_samp_linked_resource_res_map_t {
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
        uint64_t min_lod                                                      : 13;
        uint64_t max_lod                                                      : 13;
        uint64_t                                                              :  2;
        uint64_t perf_z                                                       :  4;
        uint64_t lod_bias                                                     : 14;
        uint64_t lod_bias_sec                                                 :  6;
        uint64_t xy_mag_filter                                                :  2;
        uint64_t xy_min_filter                                                :  2;
        uint64_t z_filter                                                     :  2;
        uint64_t mip_filter                                                   :  2;
        uint64_t                                                              :  1;
        uint64_t aniso_override                                               :  1;
        uint64_t perf_mip                                                     :  4;
        uint64_t                                                              : 16;
        uint64_t linked_resource_slopes                                       : 12;
        uint64_t border_color_type                                            :  2;
    };
    uint64_t u64All[2];
    uint32_t u32All[4];
};

constexpr uint32_t SqImgSampLinkedResourceResMapTWord0ClampXShift                           =  0;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord0ClampYShift                           =  3;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord0ClampZShift                           =  6;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord0MaxAnisoRatioShift                    =  9;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord0DepthCompareFuncShift                 = 12;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord0ForceUnnormalizedShift                = 15;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord0AnisoThresholdShift                   = 16;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord0McCoordTruncShift                     = 19;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord0ForceDegammaShift                     = 20;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord0AnisoBiasShift                        = 21;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord0TruncCoordShift                       = 27;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord0DisableCubeWrapShift                  = 28;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord0FilterModeShift                       = 29;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord0SkipDegammaShift                      = 31;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord1MinLodShift                           =  0;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord1MaxLodShift                           = 13;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord1PerfZShift                            = 28;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord2LodBiasShift                          =  0;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord2LodBiasSecShift                       = 14;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord2XyMagFilterShift                      = 20;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord2XyMinFilterShift                      = 22;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord2ZFilterShift                          = 24;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord2MipFilterShift                        = 26;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord2AnisoOverrideShift                    = 29;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord2PerfMipShift                          = 30;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord3LinkedResourceSlopesShift             = 18;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord3BorderColorTypeShift                  = 30;

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
        uint64_t filter_mode                                                  :  2;
        uint64_t skip_degamma                                                 :  1;
        uint64_t min_lod                                                      : 13;
        uint64_t max_lod                                                      : 13;
        uint64_t                                                              :  2;
        uint64_t perf_z                                                       :  4;
        uint64_t lod_bias                                                     : 14;
        uint64_t lod_bias_sec                                                 :  6;
        uint64_t xy_mag_filter                                                :  2;
        uint64_t xy_min_filter                                                :  2;
        uint64_t z_filter                                                     :  2;
        uint64_t mip_filter                                                   :  2;
        uint64_t                                                              :  1;
        uint64_t aniso_override                                               :  1;
        uint64_t perf_mip                                                     :  4;
        uint64_t                                                              : 16;
        uint64_t border_color_ptr                                             : 12;
        uint64_t border_color_type                                            :  2;
    };
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
constexpr uint32_t SqImgSampTWord0FilterModeShift                                           = 29;
constexpr uint32_t SqImgSampTWord0SkipDegammaShift                                          = 31;
constexpr uint32_t SqImgSampTWord1MinLodShift                                               =  0;
constexpr uint32_t SqImgSampTWord1MaxLodShift                                               = 13;
constexpr uint32_t SqImgSampTWord1PerfZShift                                                = 28;
constexpr uint32_t SqImgSampTWord2LodBiasShift                                              =  0;
constexpr uint32_t SqImgSampTWord2LodBiasSecShift                                           = 14;
constexpr uint32_t SqImgSampTWord2XyMagFilterShift                                          = 20;
constexpr uint32_t SqImgSampTWord2XyMinFilterShift                                          = 22;
constexpr uint32_t SqImgSampTWord2ZFilterShift                                              = 24;
constexpr uint32_t SqImgSampTWord2MipFilterShift                                            = 26;
constexpr uint32_t SqImgSampTWord2AnisoOverrideShift                                        = 29;
constexpr uint32_t SqImgSampTWord2PerfMipShift                                              = 30;
constexpr uint32_t SqImgSampTWord3BorderColorPtrShift                                       = 18;
constexpr uint32_t SqImgSampTWord3BorderColorTypeShift                                      = 30;

union sq_wrexec_exec_t {
    struct {
        uint64_t addr                                                         : 48;
        uint64_t                                                              : 10;
        uint64_t first_wave                                                   :  1;
        uint64_t                                                              :  1;
        uint64_t mtype                                                        :  3;
        uint64_t msb                                                          :  1;
    };
    uint64_t u64All[1];
    uint32_t u32All[2];
};

constexpr uint32_t SqWrexecExecTWord0AddrShift                                              =  0;
constexpr uint32_t SqWrexecExecTWord1FirstWaveShift                                         = 26;
constexpr uint32_t SqWrexecExecTWord1MtypeShift                                             = 28;
constexpr uint32_t SqWrexecExecTWord1MsbShift                                               = 31;

} // inline namespace Chip
} // namespace Gfx12
} // namespace Pal
