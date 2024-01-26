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

#include <stdint.h>

namespace Pal
{
namespace Gfx9
{
inline namespace Chip
{
union sq_buf_rsrc_t {
    struct {
        uint64_t base_address                                                 : 48;
        uint64_t stride                                                       : 14;
        uint64_t                                                              :  2;
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
        uint64_t                                                              : 62;
        uint64_t cache_swizzle                                                :  1;
        uint64_t swizzle_enable                                               :  1;
        uint64_t                                                              : 64;
    } gfx10;
#if  CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1 || CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
    struct {
        uint64_t                                                              : 64;
        uint64_t                                                              : 58;
        uint64_t llc_noalloc                                                  :  2;
        uint64_t                                                              :  4;
    } gfx103PlusExclusive;
#endif
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
    struct {
        uint64_t                                                              : 64;
        uint64_t                                                              : 44;
        uint64_t format                                                       :  6;
        uint64_t                                                              : 14;
    } gfx104Plus;
#endif
    struct {
        uint64_t                                                              : 64;
        uint64_t                                                              : 44;
        uint64_t format                                                       :  7;
        uint64_t                                                              :  5;
        uint64_t resource_level                                               :  1;
        uint64_t                                                              :  7;
    } gfx10Core;
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
    struct {
        uint64_t                                                              : 62;
        uint64_t swizzle_enable                                               :  2;
        uint64_t                                                              : 64;
    } gfx11;
#endif
    uint64_t u64All[2];
    uint32_t u32All[4];
};

constexpr uint32_t SqBufRsrcTWord0BaseAddressShift                                          =  0;
constexpr uint32_t SqBufRsrcTWord1StrideShift                                               = 16;
constexpr uint32_t SqBufRsrcTWord2NumRecordsShift                                           =  0;
constexpr uint32_t SqBufRsrcTWord3DstSelXShift                                              =  0;
constexpr uint32_t SqBufRsrcTWord3DstSelYShift                                              =  3;
constexpr uint32_t SqBufRsrcTWord3DstSelZShift                                              =  6;
constexpr uint32_t SqBufRsrcTWord3DstSelWShift                                              =  9;
constexpr uint32_t SqBufRsrcTWord3IndexStrideShift                                          = 21;
constexpr uint32_t SqBufRsrcTWord3AddTidEnableShift                                         = 23;
constexpr uint32_t SqBufRsrcTWord3OobSelectShift                                            = 28;
constexpr uint32_t SqBufRsrcTWord3TypeShift                                                 = 30;
constexpr uint32_t Gfx10SqBufRsrcTWord1CacheSwizzleShift                                    = 30;
constexpr uint32_t Gfx10SqBufRsrcTWord1SwizzleEnableShift                                   = 31;
#if  CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1 || CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
constexpr uint32_t Gfx103PlusExclusiveSqBufRsrcTWord3LlcNoallocShift                        = 26;
#endif
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
constexpr uint32_t Gfx104PlusSqBufRsrcTWord3FormatShift                                     = 12;
#endif
constexpr uint32_t Gfx10CoreSqBufRsrcTWord3FormatShift                                      = 12;
constexpr uint32_t Gfx10CoreSqBufRsrcTWord3ResourceLevelShift                               = 24;
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
constexpr uint32_t Gfx11SqBufRsrcTWord1SwizzleEnableShift                                   = 30;
#endif

#if   CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1 || CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
union sq_bvh_rsrc_t {
    struct {
        uint64_t base_address                                                 : 40;
        uint64_t                                                              : 15;
        uint64_t box_grow_value                                               :  8;
        uint64_t box_sort_en                                                  :  1;
        uint64_t size                                                         : 42;
        uint64_t                                                              : 14;
        uint64_t triangle_return_mode                                         :  1;
        uint64_t                                                              :  2;
        uint64_t big_page                                                     :  1;
        uint64_t type                                                         :  4;
    };
#if  CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1 || CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
    struct {
        uint64_t                                                              : 64;
        uint64_t                                                              : 57;
        uint64_t llc_noalloc                                                  :  2;
        uint64_t                                                              :  5;
    } gfx103PlusExclusive;
#endif
#if   CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
    struct {
        uint64_t                                                              : 53;
        uint64_t box_sorting_heuristic                                        :  2;
        uint64_t                                                              :  9;
        uint64_t                                                              : 55;
        uint64_t pointer_flags                                                :  1;
        uint64_t                                                              :  8;
    } rtIp2Plus;
#endif
    uint64_t u64All[2];
    uint32_t u32All[4];
};
#endif

#if   CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1 || CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
constexpr uint32_t SqBvhRsrcTWord0BaseAddressShift                                          =  0;
constexpr uint32_t SqBvhRsrcTWord1BoxGrowValueShift                                         = 23;
constexpr uint32_t SqBvhRsrcTWord1BoxSortEnShift                                            = 31;
constexpr uint32_t SqBvhRsrcTWord2SizeShift                                                 =  0;
constexpr uint32_t SqBvhRsrcTWord3TriangleReturnModeShift                                   = 24;
constexpr uint32_t SqBvhRsrcTWord3BigPageShift                                              = 27;
constexpr uint32_t SqBvhRsrcTWord3TypeShift                                                 = 28;
#if  CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1 || CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
constexpr uint32_t Gfx103PlusExclusiveSqBvhRsrcTWord3LlcNoallocShift                        = 25;
#endif
#if   CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
constexpr uint32_t RtIp2PlusSqBvhRsrcTWord1BoxSortingHeuristicShift                         = 21;
constexpr uint32_t RtIp2PlusSqBvhRsrcTWord3PointerFlagsShift                                = 23;
#endif
#endif

#if  CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1 || CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
union sq_img_rsrc_linked_rsrc_t {
    struct {
        uint64_t base_address                                                 : 40;
        uint64_t                                                              : 24;
        uint64_t                                                              : 32;
        uint64_t dst_sel_x                                                    :  3;
        uint64_t dst_sel_y                                                    :  3;
        uint64_t dst_sel_z                                                    :  3;
        uint64_t dst_sel_w                                                    :  3;
        uint64_t base_level                                                   :  4;
        uint64_t last_level                                                   :  4;
        uint64_t sw_mode                                                      :  5;
        uint64_t linked_resource_type                                         :  3;
        uint64_t type                                                         :  4;
        uint64_t                                                              : 32;
        uint64_t array_pitch                                                  :  4;
        uint64_t                                                              :  4;
        uint64_t depth_scale                                                  :  4;
        uint64_t height_scale                                                 :  4;
        uint64_t width_scale                                                  :  4;
        uint64_t perf_mod                                                     :  3;
        uint64_t corner_samples                                               :  1;
        uint64_t linked_resource                                              :  1;
        uint64_t                                                              :  1;
        uint64_t prt_default                                                  :  1;
        uint64_t                                                              :  5;
        uint64_t                                                              : 10;
        uint64_t iterate_256                                                  :  1;
        uint64_t                                                              :  4;
        uint64_t max_uncompressed_block_size                                  :  2;
        uint64_t max_compressed_block_size                                    :  2;
        uint64_t meta_pipe_aligned                                            :  1;
        uint64_t write_compress_enable                                        :  1;
        uint64_t compression_en                                               :  1;
        uint64_t alpha_is_on_msb                                              :  1;
        uint64_t color_transform                                              :  1;
        uint64_t meta_data_address                                            : 40;
    };
#if CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
    struct {
        uint64_t                                                              : 40;
        uint64_t min_lod                                                      : 12;
        uint64_t format                                                       :  9;
        uint64_t                                                              :  1;
        uint64_t width_lo                                                     :  2;
        uint64_t width_hi                                                     : 14;
        uint64_t height                                                       : 16;
        uint64_t                                                              :  1;
        uint64_t resource_level                                               :  1;
        uint64_t                                                              : 32;
        uint64_t depth                                                        : 16;
        uint64_t base_array                                                   : 16;
        uint64_t                                                              :  4;
        uint64_t max_mip                                                      :  4;
        uint64_t                                                              : 17;
        uint64_t lod_hdw_cnt_en                                               :  1;
        uint64_t                                                              :  5;
        uint64_t big_page                                                     :  1;
        uint64_t counter_bank_id                                              :  8;
        uint64_t llc_noalloc                                                  :  2;
        uint64_t                                                              :  1;
        uint64_t _reserved_206_203                                            :  4;
        uint64_t                                                              : 49;
    } gfx103;
#endif
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
    struct {
        uint64_t                                                              : 45;
        uint64_t llc_noalloc                                                  :  2;
        uint64_t big_page                                                     :  1;
        uint64_t max_mip                                                      :  4;
        uint64_t format                                                       :  8;
        uint64_t                                                              :  2;
        uint64_t width_lo                                                     :  2;
        uint64_t width_hi                                                     : 12;
        uint64_t                                                              :  2;
        uint64_t height                                                       : 14;
        uint64_t                                                              : 36;
        uint64_t depth                                                        : 13;
        uint64_t pitch_13                                                     :  1;
        uint64_t                                                              :  2;
        uint64_t base_array                                                   : 13;
        uint64_t                                                              : 30;
        uint64_t min_lod_lo                                                   :  5;
        uint64_t min_lod_hi                                                   :  7;
        uint64_t                                                              :  4;
        uint64_t sample_pattern_offset                                        :  4;
        uint64_t                                                              : 49;
    } gfx11;
#endif
    uint64_t u64All[4];
    uint32_t u32All[8];
};
#endif

#if  CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1 || CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
constexpr uint32_t SqImgRsrcLinkedRsrcTWord0BaseAddressShift                                =  0;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord3DstSelXShift                                    =  0;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord3DstSelYShift                                    =  3;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord3DstSelZShift                                    =  6;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord3DstSelWShift                                    =  9;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord3BaseLevelShift                                  = 12;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord3LastLevelShift                                  = 16;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord3SwModeShift                                     = 20;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord3LinkedResourceTypeShift                         = 25;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord3TypeShift                                       = 28;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord5ArrayPitchShift                                 =  0;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord5DepthScaleShift                                 =  8;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord5HeightScaleShift                                = 12;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord5WidthScaleShift                                 = 16;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord5PerfModShift                                    = 20;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord5CornerSamplesShift                              = 23;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord5LinkedResourceShift                             = 24;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord5PrtDefaultShift                                 = 26;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord6Iterate256Shift                                 = 10;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord6MaxUncompressedBlockSizeShift                   = 15;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord6MaxCompressedBlockSizeShift                     = 17;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord6MetaPipeAlignedShift                            = 19;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord6WriteCompressEnableShift                        = 20;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord6CompressionEnShift                              = 21;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord6AlphaIsOnMsbShift                               = 22;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord6ColorTransformShift                             = 23;
constexpr uint32_t SqImgRsrcLinkedRsrcTWord6MetaDataAddressShift                            = 24;
#if CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
constexpr uint32_t Gfx103SqImgRsrcLinkedRsrcTWord1MinLodShift                               =  8;
constexpr uint32_t Gfx103SqImgRsrcLinkedRsrcTWord1FormatShift                               = 20;
constexpr uint32_t Gfx103SqImgRsrcLinkedRsrcTWord1WidthShift                                = 30;
constexpr uint32_t Gfx103SqImgRsrcLinkedRsrcTWord2HeightShift                               = 14;
constexpr uint32_t Gfx103SqImgRsrcLinkedRsrcTWord2ResourceLevelShift                        = 31;
constexpr uint32_t Gfx103SqImgRsrcLinkedRsrcTWord4DepthShift                                =  0;
constexpr uint32_t Gfx103SqImgRsrcLinkedRsrcTWord4BaseArrayShift                            = 16;
constexpr uint32_t Gfx103SqImgRsrcLinkedRsrcTWord5MaxMipShift                               =  4;
constexpr uint32_t Gfx103SqImgRsrcLinkedRsrcTWord5LodHdwCntEnShift                          = 25;
constexpr uint32_t Gfx103SqImgRsrcLinkedRsrcTWord5BigPageShift                              = 31;
constexpr uint32_t Gfx103SqImgRsrcLinkedRsrcTWord6CounterBankIdShift                        =  0;
constexpr uint32_t Gfx103SqImgRsrcLinkedRsrcTWord6LlcNoallocShift                           =  8;
constexpr uint32_t Gfx103SqImgRsrcLinkedRsrcTWord6Reserved206203Shift                       = 11;
#endif
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
constexpr uint32_t Gfx11SqImgRsrcLinkedRsrcTWord1LlcNoallocShift                            = 13;
constexpr uint32_t Gfx11SqImgRsrcLinkedRsrcTWord1BigPageShift                               = 15;
constexpr uint32_t Gfx11SqImgRsrcLinkedRsrcTWord1MaxMipShift                                = 16;
constexpr uint32_t Gfx11SqImgRsrcLinkedRsrcTWord1FormatShift                                = 20;
constexpr uint32_t Gfx11SqImgRsrcLinkedRsrcTWord1WidthShift                                 = 30;
constexpr uint32_t Gfx11SqImgRsrcLinkedRsrcTWord2HeightShift                                = 14;
constexpr uint32_t Gfx11SqImgRsrcLinkedRsrcTWord4DepthShift                                 =  0;
constexpr uint32_t Gfx11SqImgRsrcLinkedRsrcTWord4Pitch13Shift                               = 13;
constexpr uint32_t Gfx11SqImgRsrcLinkedRsrcTWord4BaseArrayShift                             = 16;
constexpr uint32_t Gfx11SqImgRsrcLinkedRsrcTWord5MinLodShift                                = 27;
constexpr uint32_t Gfx11SqImgRsrcLinkedRsrcTWord6SamplePatternOffsetShift                   = 11;
#endif
#endif

union sq_img_rsrc_t {
    struct {
        uint64_t base_address                                                 : 40;
        uint64_t                                                              : 24;
        uint64_t                                                              : 32;
        uint64_t dst_sel_x                                                    :  3;
        uint64_t dst_sel_y                                                    :  3;
        uint64_t dst_sel_z                                                    :  3;
        uint64_t dst_sel_w                                                    :  3;
        uint64_t base_level                                                   :  4;
        uint64_t last_level                                                   :  4;
        uint64_t sw_mode                                                      :  5;
        uint64_t bc_swizzle                                                   :  3;
        uint64_t type                                                         :  4;
        uint64_t                                                              : 52;
        uint64_t perf_mod                                                     :  3;
        uint64_t corner_samples                                               :  1;
        uint64_t                                                              :  8;
        uint64_t                                                              : 10;
        uint64_t iterate_256                                                  :  1;
        uint64_t                                                              :  4;
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
        uint64_t                                                              : 62;
        uint64_t width_lo                                                     :  2;
        uint64_t width_hi                                                     : 14;
        uint64_t height                                                       : 16;
        uint64_t                                                              : 34;
        uint64_t depth                                                        : 16;
        uint64_t base_array                                                   : 16;
        uint64_t                                                              : 32;
        uint64_t                                                              : 11;
        uint64_t _reserved_206_203                                            :  4;
        uint64_t                                                              : 49;
    } gfx10;
#if CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
    struct {
        uint64_t                                                              : 64;
        uint64_t                                                              : 64;
        uint64_t                                                              : 64;
        uint64_t                                                              :  8;
        uint64_t llc_noalloc                                                  :  2;
        uint64_t                                                              : 54;
    } gfx103;
#endif
#if  CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1 || CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
    struct {
        uint64_t                                                              : 64;
        uint64_t                                                              : 64;
        uint64_t                                                              : 56;
        uint64_t linked_resource                                              :  1;
        uint64_t                                                              :  7;
        uint64_t                                                              : 64;
    } gfx103CorePlus;
#endif
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
    struct {
        uint64_t                                                              : 45;
        uint64_t llc_noalloc                                                  :  2;
        uint64_t big_page                                                     :  1;
        uint64_t max_mip                                                      :  4;
        uint64_t format                                                       :  8;
        uint64_t                                                              :  4;
        uint64_t                                                              : 64;
        uint64_t                                                              : 64;
        uint64_t                                                              : 64;
    } gfx104Plus;
#endif
    struct {
        uint64_t                                                              : 40;
        uint64_t min_lod                                                      : 12;
        uint64_t format                                                       :  9;
        uint64_t                                                              :  3;
        uint64_t                                                              : 31;
        uint64_t resource_level                                               :  1;
        uint64_t                                                              : 32;
        uint64_t                                                              : 36;
        uint64_t max_mip                                                      :  4;
        uint64_t                                                              : 17;
        uint64_t lod_hdw_cnt_en                                               :  1;
        uint64_t                                                              :  5;
        uint64_t big_page                                                     :  1;
        uint64_t counter_bank_id                                              :  8;
        uint64_t                                                              : 56;
    } gfx10Core;
    struct {
        uint64_t                                                              : 64;
        uint64_t                                                              : 64;
        uint64_t                                                              : 32;
        uint64_t array_pitch                                                  :  4;
        uint64_t                                                              :  4;
        uint64_t min_lod_warn                                                 : 12;
        uint64_t                                                              :  6;
        uint64_t prt_default                                                  :  1;
        uint64_t                                                              :  5;
        uint64_t                                                              : 64;
    } gfx10CorePlus;
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
    struct {
        uint64_t                                                              : 62;
        uint64_t width_lo                                                     :  2;
        uint64_t width_hi                                                     : 12;
        uint64_t                                                              :  2;
        uint64_t height                                                       : 14;
        uint64_t                                                              : 36;
        uint64_t depth                                                        : 13;
        uint64_t pitch_13                                                     :  1;
        uint64_t                                                              :  2;
        uint64_t base_array                                                   : 13;
        uint64_t                                                              : 30;
        uint64_t min_lod_lo                                                   :  5;
        uint64_t min_lod_hi                                                   :  7;
        uint64_t                                                              :  4;
        uint64_t sample_pattern_offset                                        :  4;
        uint64_t                                                              : 49;
    } gfx11;
#endif
    uint64_t u64All[4];
    uint32_t u32All[8];
};

constexpr uint32_t SqImgRsrcTWord0BaseAddressShift                                          =  0;
constexpr uint32_t SqImgRsrcTWord3DstSelXShift                                              =  0;
constexpr uint32_t SqImgRsrcTWord3DstSelYShift                                              =  3;
constexpr uint32_t SqImgRsrcTWord3DstSelZShift                                              =  6;
constexpr uint32_t SqImgRsrcTWord3DstSelWShift                                              =  9;
constexpr uint32_t SqImgRsrcTWord3BaseLevelShift                                            = 12;
constexpr uint32_t SqImgRsrcTWord3LastLevelShift                                            = 16;
constexpr uint32_t SqImgRsrcTWord3SwModeShift                                               = 20;
constexpr uint32_t SqImgRsrcTWord3BcSwizzleShift                                            = 25;
constexpr uint32_t SqImgRsrcTWord3TypeShift                                                 = 28;
constexpr uint32_t SqImgRsrcTWord5PerfModShift                                              = 20;
constexpr uint32_t SqImgRsrcTWord5CornerSamplesShift                                        = 23;
constexpr uint32_t SqImgRsrcTWord6Iterate256Shift                                           = 10;
constexpr uint32_t SqImgRsrcTWord6MaxUncompressedBlockSizeShift                             = 15;
constexpr uint32_t SqImgRsrcTWord6MaxCompressedBlockSizeShift                               = 17;
constexpr uint32_t SqImgRsrcTWord6MetaPipeAlignedShift                                      = 19;
constexpr uint32_t SqImgRsrcTWord6WriteCompressEnableShift                                  = 20;
constexpr uint32_t SqImgRsrcTWord6CompressionEnShift                                        = 21;
constexpr uint32_t SqImgRsrcTWord6AlphaIsOnMsbShift                                         = 22;
constexpr uint32_t SqImgRsrcTWord6ColorTransformShift                                       = 23;
constexpr uint32_t SqImgRsrcTWord6MetaDataAddressShift                                      = 24;
constexpr uint32_t Gfx10SqImgRsrcTWord1WidthShift                                           = 30;
constexpr uint32_t Gfx10SqImgRsrcTWord2HeightShift                                          = 14;
constexpr uint32_t Gfx10SqImgRsrcTWord4DepthShift                                           =  0;
constexpr uint32_t Gfx10SqImgRsrcTWord4BaseArrayShift                                       = 16;
constexpr uint32_t Gfx10SqImgRsrcTWord6Reserved206203Shift                                  = 11;
#if CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
constexpr uint32_t Gfx103SqImgRsrcTWord6LlcNoallocShift                                     =  8;
#endif
#if  CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1 || CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
constexpr uint32_t Gfx103CorePlusSqImgRsrcTWord5LinkedResourceShift                         = 24;
#endif
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
constexpr uint32_t Gfx104PlusSqImgRsrcTWord1LlcNoallocShift                                 = 13;
constexpr uint32_t Gfx104PlusSqImgRsrcTWord1BigPageShift                                    = 15;
constexpr uint32_t Gfx104PlusSqImgRsrcTWord1MaxMipShift                                     = 16;
constexpr uint32_t Gfx104PlusSqImgRsrcTWord1FormatShift                                     = 20;
#endif
constexpr uint32_t Gfx10CoreSqImgRsrcTWord1MinLodShift                                      =  8;
constexpr uint32_t Gfx10CoreSqImgRsrcTWord1FormatShift                                      = 20;
constexpr uint32_t Gfx10CoreSqImgRsrcTWord2ResourceLevelShift                               = 31;
constexpr uint32_t Gfx10CoreSqImgRsrcTWord5MaxMipShift                                      =  4;
constexpr uint32_t Gfx10CoreSqImgRsrcTWord5LodHdwCntEnShift                                 = 25;
constexpr uint32_t Gfx10CoreSqImgRsrcTWord5BigPageShift                                     = 31;
constexpr uint32_t Gfx10CoreSqImgRsrcTWord6CounterBankIdShift                               =  0;
constexpr uint32_t Gfx10CorePlusSqImgRsrcTWord5ArrayPitchShift                              =  0;
constexpr uint32_t Gfx10CorePlusSqImgRsrcTWord5MinLodWarnShift                              =  8;
constexpr uint32_t Gfx10CorePlusSqImgRsrcTWord5PrtDefaultShift                              = 26;
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
constexpr uint32_t Gfx11SqImgRsrcTWord1WidthShift                                           = 30;
constexpr uint32_t Gfx11SqImgRsrcTWord2HeightShift                                          = 14;
constexpr uint32_t Gfx11SqImgRsrcTWord4DepthShift                                           =  0;
constexpr uint32_t Gfx11SqImgRsrcTWord4Pitch13Shift                                         = 13;
constexpr uint32_t Gfx11SqImgRsrcTWord4BaseArrayShift                                       = 16;
constexpr uint32_t Gfx11SqImgRsrcTWord5MinLodShift                                          = 27;
constexpr uint32_t Gfx11SqImgRsrcTWord6SamplePatternOffsetShift                             = 11;
#endif

#if CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
union sq_img_samp_deriv_adjust_linked_resource_res_map_t {
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
        uint64_t linked_resource_slopes                                       : 12;
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
#endif

#if CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord0ClampXShift                =  0;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord0ClampYShift                =  3;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord0ClampZShift                =  6;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord0MaxAnisoRatioShift         =  9;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord0DepthCompareFuncShift      = 12;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord0ForceUnnormalizedShift     = 15;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord0AnisoThresholdShift        = 16;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord0McCoordTruncShift          = 19;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord0ForceDegammaShift          = 20;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord0AnisoBiasShift             = 21;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord0TruncCoordShift            = 27;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord0DisableCubeWrapShift       = 28;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord0FilterModeShift            = 29;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord0SkipDegammaShift           = 31;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord1MinLodShift                =  0;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord1MaxLodShift                = 12;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord1PerfMipShift               = 24;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord1PerfZShift                 = 28;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord2LinkedResourceSlopesShift  =  0;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord2BorderColorTypeShift       = 12;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord2LodBiasSecShift            = 14;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord2XyMagFilterShift           = 20;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord2XyMinFilterShift           = 22;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord2ZFilterShift               = 24;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord2MipFilterShift             = 26;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord2MipPointPreclampShift      = 28;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord2AnisoOverrideShift         = 29;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord2BlendPrtShift              = 30;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord2DerivAdjustEnShift         = 31;
constexpr uint32_t SqImgSampDerivAdjustLinkedResourceResMapTWord3DerivAdjustValuesShift     =  0;
#endif

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

#if  CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1 || CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
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
        uint64_t blend_prt                                                    :  1;
        uint64_t                                                              : 31;
        uint64_t border_color_type                                            :  2;
    };
#if CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
    struct {
        uint64_t                                                              : 64;
        uint64_t                                                              : 28;
        uint64_t mip_point_preclamp                                           :  1;
        uint64_t                                                              :  2;
        uint64_t deriv_adjust_en                                              :  1;
        uint64_t linked_resource_slopes                                       : 12;
        uint64_t                                                              : 16;
        uint64_t                                                              :  4;
    } gfx103;
#endif
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
    struct {
        uint64_t                                                              : 64;
        uint64_t                                                              : 50;
        uint64_t linked_resource_slopes                                       : 12;
        uint64_t                                                              :  2;
    } gfx11;
#endif
    uint64_t u64All[2];
    uint32_t u32All[4];
};
#endif

#if  CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1 || CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
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
constexpr uint32_t SqImgSampLinkedResourceResMapTWord1MaxLodShift                           = 12;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord1PerfMipShift                          = 24;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord1PerfZShift                            = 28;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord2LodBiasShift                          =  0;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord2LodBiasSecShift                       = 14;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord2XyMagFilterShift                      = 20;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord2XyMinFilterShift                      = 22;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord2ZFilterShift                          = 24;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord2MipFilterShift                        = 26;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord2AnisoOverrideShift                    = 29;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord2BlendPrtShift                         = 30;
constexpr uint32_t SqImgSampLinkedResourceResMapTWord3BorderColorTypeShift                  = 30;
#if CHIP_HDR_NAVI21|| CHIP_HDR_NAVI22|| CHIP_HDR_NAVI23|| CHIP_HDR_NAVI24|| CHIP_HDR_RAPHAEL|| CHIP_HDR_REMBRANDT
constexpr uint32_t Gfx103SqImgSampLinkedResourceResMapTWord2MipPointPreclampShift           = 28;
constexpr uint32_t Gfx103SqImgSampLinkedResourceResMapTWord2DerivAdjustEnShift              = 31;
constexpr uint32_t Gfx103SqImgSampLinkedResourceResMapTWord3LinkedResourceSlopesShift       =  0;
#endif
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
constexpr uint32_t Gfx11SqImgSampLinkedResourceResMapTWord3LinkedResourceSlopesShift        = 18;
#endif
#endif

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
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
    struct {
        uint64_t                                                              : 64;
        uint64_t                                                              : 50;
        uint64_t border_color_ptr                                             : 12;
        uint64_t                                                              :  2;
    } gfx104Plus;
#endif
    struct {
        uint64_t                                                              : 64;
        uint64_t                                                              : 28;
        uint64_t mip_point_preclamp                                           :  1;
        uint64_t                                                              :  2;
        uint64_t deriv_adjust_en                                              :  1;
        uint64_t border_color_ptr                                             : 12;
        uint64_t                                                              : 16;
        uint64_t                                                              :  4;
    } gfx10Core;
    struct {
        uint64_t                                                              : 29;
        uint64_t filter_mode                                                  :  2;
        uint64_t                                                              : 33;
        uint64_t                                                              : 30;
        uint64_t blend_prt                                                    :  1;
        uint64_t                                                              : 33;
    } gfx10CorePlus;
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
#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
constexpr uint32_t Gfx104PlusSqImgSampTWord3BorderColorPtrShift                             = 18;
#endif
constexpr uint32_t Gfx10CoreSqImgSampTWord2MipPointPreclampShift                            = 28;
constexpr uint32_t Gfx10CoreSqImgSampTWord2DerivAdjustEnShift                               = 31;
constexpr uint32_t Gfx10CoreSqImgSampTWord3BorderColorPtrShift                              =  0;
constexpr uint32_t Gfx10CorePlusSqImgSampTWord0FilterModeShift                              = 29;
constexpr uint32_t Gfx10CorePlusSqImgSampTWord2BlendPrtShift                                = 30;

#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
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
#endif

#if  CHIP_HDR_NAVI31|| CHIP_HDR_NAVI32|| CHIP_HDR_NAVI33|| CHIP_HDR_PHOENIX1
constexpr uint32_t SqWrexecExecTWord0AddrShift                                              =  0;
constexpr uint32_t SqWrexecExecTWord1FirstWaveShift                                         = 26;
constexpr uint32_t SqWrexecExecTWord1MtypeShift                                             = 28;
constexpr uint32_t SqWrexecExecTWord1MsbShift                                               = 31;
#endif

} // inline namespace Chip
} // namespace Gfx9
} // namespace Pal
