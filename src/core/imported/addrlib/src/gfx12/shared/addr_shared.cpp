/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "addr_shared.h"
#include <stdio.h>

namespace GFX12_METADATA_REFERENCE_MODEL {

    void getS3Start(
        int32		 position,
        addr_params&	 p,
        int32&		 x,
        int32&		 y,
        int32&		 z)
    {
        int32 base = (position / 3) - (p.bpp_log2 / 3);
        x = base;
        y = base;
        z = base;
        if (position % 3 > 0) x++;
        if (position % 3 > 1) z++;
        if (p.bpp_log2 % 3 > 0) x--;
        if (p.bpp_log2 % 3 > 1) z--;
    }

    // Calculate the block size given the params and the block size log2
    void calcBlockSize(
        addr_params& p,
        int32		 block_size_log2,
        int32&		 width,
        int32&		 height,
        int32&		 depth)
    {
        switch (p.sw)
        {
            case SW_L:
                width  = block_size_log2 - p.bpp_log2;
                height = 0;
                depth  = 0;
                break;
            // case SW_S:
            // case SW_R:
            // case SW_Z:
            // case SW_D:
            case SW_D_2D:
                width  = (block_size_log2 >> 1) - (p.bpp_log2 >> 1) - (p.num_samples_log2 >> 1) - (p.bpp_log2 & p.num_samples_log2 & 1);
                height = (block_size_log2 >> 1) - (p.bpp_log2 >> 1) - (p.num_samples_log2 >> 1) - ((p.bpp_log2 | p.num_samples_log2) & 1);
                depth  = 0;
                break;
            //	case SW_S3:
            case SW_S_3D:
                getS3Start(block_size_log2, p, width, height, depth);
                break;
            // case SW_D3:
            // case SW_R3:
            //     block_bits = (block_size_log2 - p.bpp_log2);
            //     width  = (block_bits / 3) + (((block_bits % 3) > 0) ? 1 : 0);
            //     height = (block_bits / 3) + (((block_bits % 3) > 1) ? 1 : 0);
            //     depth  = (block_bits / 3);
            //     break;
        }
    }

    // Return W,H,D block sizes with a min of 256
    // Will return 256 for width in Linear
    void getBlockSizeSlice(
        addr_params& p,
        int32&		 width,
        int32&		 height,
        int32&		 depth)
    {
        int32 block_size_log2 = p.getSliceBlockSizeLog2();
        calcBlockSize(p, block_size_log2, width, height, depth);
    }

    // Return W,H,D block sizes
    // Will return 128 for width in Linear
    void getBlockSizePitch(addr_params& p,
                           int32& width, int32& height, int32& depth)
    {
        int32 block_size_log2 = p.getPitchBlockSizeLog2();
        calcBlockSize(p, block_size_log2, width, height, depth);
    }

    void getMipInTaleMaxSize(
        addr_params& p,
        int32&		 max_mip_in_tail_width_log2,
        int32&		 max_mip_in_tail_height_log2)
    {
        int32 block_size_log2 = p.getSliceBlockSizeLog2();

        //    int32 old_bpp = p.bpp_log2;

            //-----------------------------------------------------------------------------------------------------------------------------
            // For htile, we need to make z16 and stencil enter the mip tail at the same time as z32 would
            //-----------------------------------------------------------------------------------------------------------------------------
        //    if (p.sw_orig == SW_Z && p.bpp_log2 < 2)
        //        p.bpp_log2 = 2;

        int32 max_data_mip_in_tail_width_log2;
        int32 max_data_mip_in_tail_height_log2;

        //    int32 max_meta_mip_in_tail_width_log2;
        //    int32 max_meta_mip_in_tail_height_log2;

        int32 block_depth_log2;
        //    int32 meta_block_depth_log2;

        getBlockSizeSlice(p, max_data_mip_in_tail_width_log2, max_data_mip_in_tail_height_log2, block_depth_log2);
        //    getMetaBlockSize(p, max_meta_mip_in_tail_width_log2, max_meta_mip_in_tail_height_log2, meta_block_depth_log2);

        //    p.bpp_log2 = old_bpp;

        //    if (max_meta_mip_in_tail_width_log2 < max_data_mip_in_tail_width_log2)
        //        max_mip_in_tail_width_log2 =  max_meta_mip_in_tail_width_log2;
        //    else
        max_mip_in_tail_width_log2 = max_data_mip_in_tail_width_log2;

        //    if (max_meta_mip_in_tail_height_log2 < max_data_mip_in_tail_height_log2)
        //        max_mip_in_tail_height_log2 =  max_meta_mip_in_tail_height_log2;
        //    else
        max_mip_in_tail_height_log2 = max_data_mip_in_tail_height_log2;

        //--------------------------------------------------------------------------------------------------------------------------------------------
        // This is generalized to handle VAR block sizes
        // Since we only care about 64KB or 4KB blocks, we could simplify it to this:
        //
        // if (p.sw == some 3d mode && block_size_log2 == 12) {
        //      max_mip_in_tail_height_log2--;
        //} else {
        //      max_mip_in_tail_width_log2--;
        //}
        //--------------------------------------------------------------------------------------------------------------------------------------------

    //    if (p.sw == SW_S3 || p.sw == SW_D3 || p.sw == SW_R3) {
        if (p.sw == SW_S_3D)
        {
            switch (block_size_log2 % 3)
            {
                case 0: max_mip_in_tail_height_log2--; break;
                case 1: max_mip_in_tail_width_log2--; break;
                case 2:
                    // would decrement the depth here,
                    // if we didn't have all of the slices to begin with
                    max_mip_in_tail_width_log2--;
                    break;
            }
        }
        else
        {
#if defined(USE_VAR_MODE_FIX)
            max_mip_in_tail_width_log2--;
#else
            switch (block_size_log2 % 2)
            {
                case 0: max_mip_in_tail_width_log2--; break;
                case 1: max_mip_in_tail_height_log2--; break;
            }
#endif
        }
    }

    int32 shift_ceil(
        int32 a,
        int32 b)
    {
        // This is just ceil(a / (2^b))

        const int32 QUOT = (a >> b);
        const int32 MASK = (~(~0 << static_cast<uint32>(b)));
        const int32 ROUND_FACTOR = (((a & MASK) != 0) ? 1 : 0);

        int32 result = QUOT + ROUND_FACTOR;

        return result;
        //return (a >> b) + (((a & ~(~0 << b)) != 0) ? 1 : 0);
    }

    void getMipSize2dCompute(
        addr_params& p,
        int32			 mip,
        int32&		 width,
        int32&		 height)
    {
        width  = (p.width <= 0)  ? 1 : p.width;
        height = (p.height <= 0) ? 1 : p.height;

        width  = shift_ceil(width,  mip);
        height = shift_ceil(height, mip);
    }

    int32 addr_params::MIP_CHAIN::Get_Width(const int32 mip_id)
    {

    #if defined(CHECK_MIP_CHAIN_ARRAY_ACCESS)
        if (mip_id > MAX_POSSIBLE_MIP_LEVEL) {
            printf("\nGet_Width( mip_id: %d ) > MAX_POSSIBLE_MIP_LEVEL (%d) width: 0x%x \n\n",
                mip_id, MAX_POSSIBLE_MIP_LEVEL,
                mip_levels_array[mip_id].width);
            //assert( 0 && "mip_id <= MAX_POSSIBLE_MIP_LEVEL");
        }
    #endif

        return mip_levels_array[mip_id].width;
    }

    int32 addr_params::MIP_CHAIN::Get_Height(const int32 mip_id)
    {

    #if defined(CHECK_MIP_CHAIN_ARRAY_ACCESS)
        if (mip_id > MAX_POSSIBLE_MIP_LEVEL) {
            printf("\nGet_Height( mip_id: %d ) > MAX_POSSIBLE_MIP_LEVEL (%d)   height: 0x%x  \n\n",
                mip_id, MAX_POSSIBLE_MIP_LEVEL,
                mip_levels_array[mip_id].height);
            //assert( 0 && "mip_id <= MAX_POSSIBLE_MIP_LEVEL");
        }
    #endif

        return mip_levels_array[mip_id].height;
    }

    void getMipSize2d(
        addr_params& p,
        int32			 mip,
        int32&		 width,
        int32&		 height)
    {
        if (p.mip_chain.Get_Dirty_Bit() == true)
        {
            p.mip_chain.Init(p);
        }

        width  = p.mip_chain.Get_Width(mip);
        height = p.mip_chain.Get_Height(mip);
    }

    int32 calc_mip_in_tail(
        addr_params&	p,
        int32		mipId,
        int32		first_mip_in_tail)
    {
        // Check
        int32 mip_in_tail = mipId - first_mip_in_tail;

        if (mip_in_tail < 0) mip_in_tail = p.mip_chain.TOTAL_MIP_CHAIN_LEVELS;
        if (p.maxmip == 0)   mip_in_tail = p.mip_chain.TOTAL_MIP_CHAIN_LEVELS;

        // program as not in mip tail for <=256B size data blocks
        if (p.getSliceBlockSizeLog2() <= 8)
        {
            mip_in_tail = p.mip_chain.TOTAL_MIP_CHAIN_LEVELS;
        }
        return mip_in_tail;
    }

    void getMipOffset(
        addr_params&	p,
        int32		mip,
        // Outputs
        int64&		data_offset,
        int64&		meta_offset,
        int32&		mip_in_tail,
        int64&		data_chain_size,
        int64&		meta_chain_size)
    {
        int32 block_width_log2, block_height_log2, block_depth_log2;

        //    int32 block_size_log2 = p.getPitchBlockSizeLog2();
        int32 block_size_log2 = p.getSliceBlockSizeLog2();

        //    int32 meta_block_width_log2, meta_block_height_log2, meta_block_depth_log2;
        //    int32 meta_block_size_log2;

        getBlockSizeSlice(p,
                          block_width_log2,
                          block_height_log2,
                          block_depth_log2);

        //    meta_block_size_log2 = getMetaBlockSize(p,
        //                                            meta_block_width_log2, meta_block_height_log2, meta_block_depth_log2);

        int32 mip_width, mip_height;
        //int32 mip_depth;

        constexpr int32 TOTAL_MIP_CHAIN_LEVELS                   = addr_params::MIP_CHAIN::TOTAL_MIP_CHAIN_LEVELS;
        int32           mip_block_width[TOTAL_MIP_CHAIN_LEVELS]  = { 0 };
        int32           mip_block_height[TOTAL_MIP_CHAIN_LEVELS] = { 0 };

        //    int32 mip_meta_block_width [p.mip_chain.TOTAL_MIP_CHAIN_LEVELS] = { 0 };
        //    int32 mip_meta_block_height[p.mip_chain.TOTAL_MIP_CHAIN_LEVELS] = { 0 };

        int32 i;
        int32 first_mip_in_tail = p.maxmip;	// Set to maxmip as the default

        int32 num_mips_in_tail = getNumMipsInTail(p);

        int32 max_mip_in_tail_width_log2;
        int32 max_mip_in_tail_height_log2;

        getMipInTaleMaxSize(p,
                            max_mip_in_tail_width_log2,
                            max_mip_in_tail_height_log2);

        const int32 MAX_MIP_IN_TAIL_WIDTH_ELEMENTS  = (1 << max_mip_in_tail_width_log2);
        const int32 MAX_MIP_IN_TAIL_HEIGHT_ELEMENTS = (1 << max_mip_in_tail_height_log2);

        const int32 starting_mip_level = p.mip_chain.MAX_POSSIBLE_MIP_LEVEL;

        for (i = starting_mip_level; i >= 0; i--)
        {
            getMipSize2d(p,
                         i,
                         mip_width,
                         mip_height);  //, mip_depth);  don't use depth

            if ((mip_width <= MAX_MIP_IN_TAIL_WIDTH_ELEMENTS)
                && (mip_height <= MAX_MIP_IN_TAIL_HEIGHT_ELEMENTS)
                && (p.maxmip - i < num_mips_in_tail))
            {
                first_mip_in_tail = i;
            }

            mip_block_width[i]  = shift_ceil(mip_width, block_width_log2);
            mip_block_height[i] = shift_ceil(mip_height, block_height_log2);

            //      mip_meta_block_width[i]  = shift_ceil(mip_width, meta_block_width_log2);
            //      mip_meta_block_height[i] = shift_ceil(mip_height, meta_block_height_log2);
        }

        //if (p.maxmip == 0) {
        //    // Optimization to exit early for non-mip chains
        //    first_mip_in_tail = 1;
        //}

        mip_in_tail = calc_mip_in_tail(p, mip, first_mip_in_tail);

        //    if (mip_in_tail != 15) {
        //        bool is_x_bias = isXBias( p );
        //
        //        if (is_x_bias) {
        //            if (mip_block_height[ first_mip_in_tail] > mip_meta_block_height[ first_mip_in_tail] ) {
        //                printf("ERROR: Mip level[mip_in_tail] Data block_height 0x%x > meta_mip_block_height 0x%x \n\n\n",
        //                        mip_block_height[ first_mip_in_tail],
        //                        mip_meta_block_height[ first_mip_in_tail]
        //                       );
        //                fflush(NULL);
        //                assert(0 && "X_Bias");
        //            }
        //
        //            if (block_height_log2 >  meta_block_height_log2) {
        //                printf("ERROR: Mip level[%d] Data block_height_log2 0x%x > meta_block_height_log2 0x%x \n",
        //                       mip,
        //                       block_height_log2,
        //                       meta_block_height_log2
        //                       );
        //                fflush(NULL);
        //                //assert(0 && "X_Bias meta_block_log2");
        //        }
        //
        //        }
        //        else {
        //
        //            if ( mip_block_width[first_mip_in_tail] > mip_meta_block_width[first_mip_in_tail]) {
        //                printf("ERROR: Mip level[mip_in_tail] Data block_width 0x%x > meta_mip_block_width 0x%x   \n\n\n",
        //                            mip_block_width[ first_mip_in_tail],
        //                            mip_meta_block_width[ first_mip_in_tail]
        //                           );
        //                fflush(NULL);
        //                assert(0 && "Y_Bias");
        //            }
        //
        //            if (block_width_log2 > meta_block_width_log2) {
        //                printf("ERROR: Mip level[%d] Data block_width_log2 0x%x > meta_block_width_log2 0x%x   \n",
        //                       mip,
        //                       block_width_log2,
        //                       meta_block_width_log2
        //                      );
        //                fflush(NULL);
        //                assert(0 && "Y_Bias");
        //        }
        //
        //    }
        //
        //    }

        int64 last_mip_size = 1;
        //    int32 last_meta_mip_size = 1;

        //    if (1) { //mip_in_tail != 15) {
        //
        //        //if (p.maxmip != 0) {
        //
        //            if (  (  mip_meta_block_width [first_mip_in_tail]
        //                   * mip_meta_block_height[first_mip_in_tail]
        //                  ) > 1
        //               ) {
        //                bool is_x_bias = isXBias( p );
        //
        //                printf("ERROR: Mip level[%d]   x_bias %d    mip_in_tail %d first_in_tail(%d): mip_meta_block_width[ first: %d ] 0x%x  * mip_meta_block_height[ first: %d ] 0x%x  > 1   \n"
        //                       " MAX_MIP_IN_TAIL_WIDTH_ELEMENTS 0x%x  MAX_MIP_IN_TAIL_HEIGHT_ELEMENTS 0x%x \n",
        //                   mip,
        //                   (is_x_bias==true?1:0),
        //                       mip_in_tail,
        //                   first_mip_in_tail,
        //
        //                   first_mip_in_tail,
        //                   mip_meta_block_width [ first_mip_in_tail ],
        //
        //                   first_mip_in_tail,
        //                       mip_meta_block_height[ first_mip_in_tail ],
        //
        //                       MAX_MIP_IN_TAIL_WIDTH_ELEMENTS,
        //                       MAX_MIP_IN_TAIL_HEIGHT_ELEMENTS
        //                  );
        //            fflush(NULL);
        //
        //            assert(0 && "First_mip_In_tail > 1 metablock");
        //        }
        //    //}
        //    }

        data_offset = 0;
        meta_offset = 0;

        data_chain_size = 0;
        meta_chain_size = 0;

        for (i = first_mip_in_tail - 1; i >= -1; i--)
        {
            if (i < p.maxmip)
            {
                if (i >= mip)
                {
                    data_offset += last_mip_size;
                    //                    meta_offset += last_meta_mip_size;
                }
                data_chain_size += last_mip_size;
                //                meta_chain_size += last_meta_mip_size;
            }

            if (i >= 0)
            {
                last_mip_size = 4 * last_mip_size
                    - ((mip_block_width[i] & 1) ? mip_block_height[i] : 0)
                    - ((mip_block_height[i] & 1) ? mip_block_width[i] : 0)
                    - ((mip_block_width[i] & mip_block_height[i] & 1) ? 1 : 0);

                //                last_meta_mip_size = 4*last_meta_mip_size
                //                    - ((mip_meta_block_width [i] & 1) ? mip_meta_block_height[i] : 0)
                //                    - ((mip_meta_block_height[i] & 1) ? mip_meta_block_width [i] : 0)
                //                    - ((mip_meta_block_width[i] & mip_meta_block_height[i] & 1) ? 1 : 0);
            }
        }

        data_offset = data_offset << block_size_log2;
        //	meta_offset = meta_offset << meta_block_size_log2;

        data_chain_size = data_chain_size << block_size_log2;
        //	meta_chain_size = meta_chain_size << meta_block_size_log2;
    }

    int32 calc_byte_offset(
        addr_params& p,
        int32        mip_in_tail)
    {
        int32 byte_offset = 0;

        int32 mips_available = getNumMipsInTail(p);

        // m is mips_in_tail in reverse
        int32 m = mips_available - 1 - mip_in_tail;

        // Clamp to origin if mip_in_tail exceeds mips_available.
        // This is a convernient way to handle non-tail mips,
        // by setting mip_in_tail to a very large value
        if (m < 0) m = 0;

        if (m > 6)
        {
            // Over 2KB (16 << 7) offsets: byte offset at every power of 2 over 2KB
            byte_offset = 16 << m;
        }
        else
        {
            // Under 2KB: byte offset at every 256 B
            byte_offset = m << 8;
        }

        return byte_offset;
    }

    int32 getNumMipsInTail(addr_params& p)
    {
        int32 block_size_log2 = p.getSliceBlockSizeLog2();

        int32 effective_block_size_log2 = block_size_log2;
        // if (p.sw == SW_S3 || p.sw == SW_D3 || p.sw == SW_R3) {
        if (p.sw == SW_S_3D)
        {
            // for 3d tiling modes, we can't usee the z-term for mip-in-tail offset generation.
            // This reduces the space available in the block to use for mips within a tail.
            // So the effectvie block size is 1/3 less than what it otherwise would be (in 256B units)
            effective_block_size_log2 -= (block_size_log2 - 8) / 3;
        }

        //-----------------------------------------------------------------------------------------------------------------------------------
        // if the block size is <= 256B, then we have only 1 mip in the tail
        // if block size is <= 2KB, then we have 1 mip that takes half the block plus (block_size/2) / 256 mips in the tail
        // otherwise, we will have a mip for each power of 2 above 2KB, plus seven (that is for every 256B up to 1536 Bytes)
        //-----------------------------------------------------------------------------------------------------------------------------------
        int32 mips_in_tail = (effective_block_size_log2 <= 8) ? 1
            : ((effective_block_size_log2 <= 11) ? 1 + (1 << (effective_block_size_log2 - 9))    // 1 + (block_size_log2 - 1) - 8
                : (effective_block_size_log2 - 11) + 7);
        return mips_in_tail;
    }

    void getMicroBlockSize(
        addr_params& p,
        int32&		 width,
        int32&		 height,
        int32&		 depth)
    {
        int32 block_bits;

        switch (p.sw)
        {
            case SW_L:
                width  = (8 - p.bpp_log2);
                height = 0;
                depth  = 0;
                break;
            // case SW_S:
            // case SW_R:
            // case SW_Z:
            // case SW_D:
            // case SW_R3:
            case SW_D_2D:
                block_bits = (8 - p.bpp_log2);
                // if (p.sw == SW_Z)
                // {
                //     block_bits -= p.num_samples_log2;
                // }
                width  = (block_bits >> 1) + (block_bits & 1);
                height = (block_bits >> 1);
                depth  = 0;
                break;
                // case SW_S3:
                // case SW_D3:
            case SW_S_3D:
                block_bits = (8 - p.bpp_log2);
                depth  = (block_bits / 3) + (((block_bits % 3) > 0) ? 1 : 0);
                width  = (block_bits / 3) + (((block_bits % 3) > 1) ? 1 : 0);
                height = (block_bits / 3);
                break;
        }
    }

    void getMipOrigin(
        addr_params& p,
        int32 mip_in_tail,
        int32& mip_x,
        int32& mip_y,
        int32& mip_z)
    {
        int32 byte_offset = calc_byte_offset(p, mip_in_tail);

        // Initialize
        mip_x = 0;
        mip_y = 0;
        mip_z = 0;

    #if defined(USE_VAR_MODE_FIX)
        const int32 BLOCK_SIZE_LOG2 = p.getPitchBlockSizeLog2();
    #endif
        //  8KB  0xb0100                                               54_3210
        // x=32  0x040  >> 3 = 0010, 0001, 0000_1000   => 0x08     0b0000_1000

//V     // 16KB  0xb0100                                               54_3210
        switch (p.sw) {            // y=64  0x040  >> 3 = 0010, 0001, 0000_1000   => 0x08     0b0000_1000

            // 32KB  0xb0100                                               54_3210
//    	case SW_S3:            // x=64  0x040  >> 3 = 0010, 0001, 0000_1000   => 0x08     0b0000_1000
//	case SW_D3:
//    	case SW_R3:     //V    // 64KB  0xb1000                                               54_3210        ==============
//    	case SW_S:             // y=128 0x080  >> 3 = 0100, 0010, 0001_0000   => 0x10     0b0001_0000        ==============
//	case SW_D:
//    	case SW_R:             // 128KB  0b000?                                            54_3210
//      case SW_Z:             // ?=???  0x?  >> 3 = 1000, 0100, 0010   => 0x20      0b0010_0000

                         //V   // 256KB  0b0001                                            54_3210
                               // y=256  0x100  >> 3 = 1000, 0100, 0010   => 0x20      0b0010_0000
        case SW_D_2D:
        case SW_S_3D:
            //                                2
            //                                5       6   3   1
            //                                6       4   2   6   8   4   2   1
            //                                K       K   K   K   K   K   K   K
            //                           19  18  17  16  15  14  13  12  11  10   9   8
            // This does the following: {x5, y5, x4, y4, x3, y3, x2, y2, x1, y1, x0, y0} = byte_offset[15:8]
            //                       x0                          x1                          x2                          x3                           x4                           x5
            mip_x = ((byte_offset >> 9) & 1) | ((byte_offset >> 10) & 2) | ((byte_offset >> 11) & 4) | ((byte_offset >> 12) & 8) | ((byte_offset >> 13) & 16) | ((byte_offset >> 14) & 32);
            mip_y = ((byte_offset >> 8) & 1) | ((byte_offset >> 9) & 2) | ((byte_offset >> 10) & 4) | ((byte_offset >> 11) & 8) | ((byte_offset >> 12) & 16) | ((byte_offset >> 13) & 32);
            //                       y0                          y1                          y2                          y3                           y4                           y5

        #if defined(USE_VAR_MODE_FIX)
                // For odd block sizes swap mip_x/y, in order for it to be x-biased
                if (BLOCK_SIZE_LOG2 & 1)
                {
                    int32 temp = mip_x;
                    mip_x = mip_y;
                    mip_y = temp;

                    //-----------------------------------------------------------------------------------------------------
                    // For odd bpp, the micro block width is twice that of the height.  To compensate for this,
                    // we need divide mip_x by two, and
                    // multiply mip_y by 2, and OR in the lsb of mip_x
                    //-----------------------------------------------------------------------------------------------------
                    if (p.bpp_log2 & 1)
                    {
                        mip_y = (mip_y << 1) | (mip_x & 1);   // Preserve lsb of mip_x by pushing it into y dimension
                        mip_x = (mip_x >> 1);                 // Decrease x dimension to compensate for increase in micro block width of odd BPE
                    }
                }
        #endif
// Already initialized
//                mip_z = 0;
                break;

            case SW_L:
                mip_x = byte_offset >> 8;
// Already initialized
//                mip_y = 0;
//                mip_z = 0;
                break;
        }

        int32 u_block_width_log2, u_block_height_log2, u_block_depth_log2;

        getMicroBlockSize(p, u_block_width_log2, u_block_height_log2, u_block_depth_log2);

        mip_x = mip_x << u_block_width_log2;
        mip_y = mip_y << u_block_height_log2;
        mip_z = mip_z << u_block_depth_log2;
    }

    // Calculate the xyz offsets
    void
    getXYZoffsets(// Inputs
		  addr_params& p,
                  int32 x, int32 y, int32 z,
                  int32 mip_in_tail,
                  // Outputs
                  int32& x_offset, int32& y_offset, int32& z_offset,
                  int32& x_mip_orig, int32& y_mip_orig, int32& z_mip_orig)
    {

        getMipOrigin(p,
                     mip_in_tail,
                     x_mip_orig, y_mip_orig, z_mip_orig);

        x_offset = x + x_mip_orig;
        y_offset = y + y_mip_orig;
        z_offset = z + z_mip_orig;
    }

    // Calculate the block indexes
    // Return false if offset is beyond data block, otherwise true.
    bool
    getXYZblockIndexes(// Inputs
#ifndef ADDR_SHARED
        bool check_assert,	// Not used in SW
#endif
		       addr_params& p,
                       int32 x, int32 y, int32 z,
                       int32 mip_in_tail,
                       int32 pitch_in_elements,
                       int64 slice_in_elements,
		       // Outputs
                       int64& z_macro_block_index,
                       int64& yx_macro_block_index
        )
    {
        //--------------------------------------------------------------------------------------
        // Get block dimensions in elements
        //--------------------------------------------------------------------------------------
        int32 slice_block_width_in_elements,  slice_block_width_in_elements_log2;
        int32 slice_block_height_in_elements, slice_block_height_in_elements_log2;
        int32 slice_block_depth_in_elements,  slice_block_depth_in_elements_log2;

        getBlockSizeSlice(p,
                          slice_block_width_in_elements_log2,
                          slice_block_height_in_elements_log2,
                          slice_block_depth_in_elements_log2
            );

        // Convert block dimensions to log2 elements
        slice_block_width_in_elements  = 1 << slice_block_width_in_elements_log2;
        slice_block_height_in_elements = 1 << slice_block_height_in_elements_log2;
        slice_block_depth_in_elements  = 1 << slice_block_depth_in_elements_log2;

        int32 pitch_block_width_in_elements,  pitch_block_width_in_elements_log2;
        int32 pitch_block_height_in_elements, pitch_block_height_in_elements_log2;
        int32                                 pitch_block_depth_in_elements_log2;

        getBlockSizePitch(p,
                          pitch_block_width_in_elements_log2,
                          pitch_block_height_in_elements_log2,
                          pitch_block_depth_in_elements_log2
        );

        // Convert block dimensions to log2 elements
        pitch_block_width_in_elements  = 1 << pitch_block_width_in_elements_log2;
        pitch_block_height_in_elements = 1 << pitch_block_height_in_elements_log2;

        // Calculate the xyz offsets
        int32 x_offset;
        int32 y_offset;
        int32 z_offset;
        int32 x_mip_orig;
        int32 y_mip_orig;
        int32 z_mip_orig;
        getXYZoffsets(p,
                      x, y, z,
                      mip_in_tail,
                      x_offset, y_offset, z_offset,
                      x_mip_orig, y_mip_orig, z_mip_orig);

#ifndef ADDR_SHARED
        if (check_assert)
        {
            if (mip_in_tail != 0x11)
            {
                //-------------------------------------------------------------------------------------------------------
                // Inisde mip tail
                //-------------------------------------------------------------------------------------------------------
                if ( x_offset >= pitch_block_width_in_elements )
                {
                    printf("getZYXblockIndexes: x_offset: 0x%x within miptail is beyond data block width:  0x%x (mip_orig: x,y,z 0x%x 0x%x 0x%x)  mip_in_tail 0x%x Log2_BPE: %d \n",
                           x_offset,
                           pitch_block_width_in_elements,
                           x_mip_orig, y_mip_orig, z_mip_orig,
                           mip_in_tail,
                           p.bpp_log2
                        );
                    return false;
                }

                if (y_offset >= slice_block_height_in_elements)
                {
                    printf("getZYXblockIndexes: y_offset: 0x%x within miptail is beyond data block height:  0x%x (mip_orig: x,y,z 0x%x 0x%x 0x%x)  mip_in_tail 0x%x Log2_BPE: %d \n",
                           y_offset,
                           slice_block_height_in_elements,
                           x_mip_orig, y_mip_orig, z_mip_orig,
                           mip_in_tail,

                           p.bpp_log2
                        );
                    return false;
                }
            }
        }
#endif
        int64 pitch_in_macro_blocks = (pitch_in_elements / pitch_block_width_in_elements);
        int64 slice_in_macro_blocks = (slice_in_elements / slice_block_height_in_elements) / slice_block_width_in_elements;

        int32 x_block_units = x_offset / pitch_block_width_in_elements;
        int32 y_block_units = y_offset / pitch_block_height_in_elements;
        int32 z_block_units = z_offset / slice_block_depth_in_elements;

        // Need to separate for SW_LINEAR
        z_macro_block_index = (slice_in_macro_blocks * z_block_units);

        yx_macro_block_index = (pitch_in_macro_blocks * y_block_units)
				+ x_block_units;

        return true;
    }

}
