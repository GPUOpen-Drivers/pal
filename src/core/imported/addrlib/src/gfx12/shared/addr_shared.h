/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
#ifndef __ADDR_SHARED_H
#define __ADDR_SHARED_H

#include <string.h>
#include <stdtypes.h>

#define ADDR_ASIC_ID_GFX_ENGINE_GFX12 0x0000000C

namespace GFX12_METADATA_REFERENCE_MODEL {

    const int32 SW_L = 0;
    const int32 SW_D_2D = 1;
    const int32 SW_S_3D = 2;

    const int32 PIPE_DIST_8X8 = 0;
    const int32 PIPE_DIST_16X16 = 1;

    class MIP_LEVEL
    {
    public:
        MIP_LEVEL(void) { width = 0; height = 0; }

        int32 width;
        int32 height;
    };

    // Forward declaration
    struct addr_params;

    void getMipSize2dCompute(addr_params& p, int32 mip, int32& width, int32& height);
    void getMipSize2d(addr_params& p, int32 mip, int32& width, int32& height);

    struct addr_params
    {
        addr_params(void)
        {
            int32 len = sizeof(*this);
            memset(static_cast<void*>(this), 0, len);
        }

#if 1
        class MIP_CHAIN
        {
        public:
            enum
            {
                TOTAL_MIP_CHAIN_LEVELS = 17,
                MAX_POSSIBLE_MIP_LEVEL = TOTAL_MIP_CHAIN_LEVELS - 1
            };

            MIP_LEVEL  mip_levels_array[TOTAL_MIP_CHAIN_LEVELS];

            MIP_CHAIN(void)
            {
                int32 len = sizeof(*this);
                memset(static_cast<void*>(this), 0, len);

                is_clean = false;
            }

            void Init(addr_params& p)
            {
                int32 w = 0;
                int32 h = 0;

                int32 mip_id;
                for (mip_id = 0; mip_id < TOTAL_MIP_CHAIN_LEVELS; mip_id++) {
                    getMipSize2dCompute(p,
                                        mip_id,
                                        w,
                                        h);

                    mip_levels_array[mip_id].width  = w;
                    mip_levels_array[mip_id].height = h;
                }

                Set_Dirty_Bit(false);
            }

            int32 Get_Width(const int32 mip_id);
            int32 Get_Height(const int32 mip_id);

            bool Get_Dirty_Bit(void) { return is_clean == false; }
            void Set_Dirty_Bit(const bool is_dirty_flag) { is_clean = !is_dirty_flag; }

        private:
            bool is_clean;
        };

        MIP_CHAIN    mip_chain;
#endif

        //===================================================================================================================
        // RB+ variables
        //===================================================================================================================
        int32 chip_engine    = ADDR_ASIC_ID_GFX_ENGINE_GFX12;
        bool  RB_Plus_Flag   = true;
        bool  Bank_Xor_Flag  = RB_Plus_Flag;
        bool  Allow_4_Terms_For_D3_Flag = RB_Plus_Flag;
        bool  Allow_Var_Flag = RB_Plus_Flag;
        bool  Var_Includes_Bank_Flag    = RB_Plus_Flag;

        int32 sw;		// For GFX11 R maps to Z
        int32 sw_orig;		// Unmodified sw type
        int32 num_pipes_log2;
        int32 bpp_log2;
        int32 num_samples_log2;

        int32 pitch_block_size_log2;
        int32 slice_block_size_log2;	// Block size that can't be < 256
        int32 pipe_interleave_log2;

        int32 xor_mode;
        bool  pipe_aligned;

        int32 max_comp_frag_log2;
        int32 surf_type;

        int32 Get_Width(void) { return width; }
        void  Set_Width(const int32 input_width) { width = input_width; }

        int32 Get_Height(void) { return height; }
        void  Set_Height(const int32 input_height) { height = input_height; }

        //private:
        int32 width;
        int32 height;

    public:
        int32 depth;
        int32 maxmip;

        int32 pipe_dist;
        int32 num_sas_log2;
        bool  msaa_bank_xor;

        int32 Get_Num_Sas_Log2(void)
        {
#ifdef ADDRESS__RB_PLUS_DEFAULTS__1
            if (RB_Plus_Flag == true)
            {
#ifdef ADDRESS__AL_GFX12__1
                return 2;       // Hard coded value ( total_num_packers / 2) for Navi4X: 5 / 2 = 2
#endif
            }
            else {
#endif
                return num_sas_log2;
#ifdef ADDRESS__RB_PLUS_DEFAULTS__1
            }
#endif
        }

        int32 getEffectiveNumPipes() { return (pipe_dist == PIPE_DIST_8X8 || Get_Num_Sas_Log2() >= num_pipes_log2 - 1) ? num_pipes_log2 : Get_Num_Sas_Log2() + 1; }
        //int32 getNumSasLog2() { return (num_pipes_log2 < 2) ? 0 : num_pipes_log2 -2; }

        int32 getMaxCompFragLog2() { return (num_samples_log2 < max_comp_frag_log2) ? num_samples_log2 : max_comp_frag_log2; }

        //    bool isRBAligned() { return (sw == SW_Z || sw == SW_R || sw == SW_D3) ? true : false; }
        bool isRBAligned() { return false; }
        int32 getPipeRotateAmount()
        {
            if (pipe_dist == PIPE_DIST_16X16 && num_pipes_log2 >= Get_Num_Sas_Log2() + 1 && num_pipes_log2 > 1)
            {
                return (num_pipes_log2 == Get_Num_Sas_Log2() + 1 && isRBAligned()) ? 1 : num_pipes_log2 - (Get_Num_Sas_Log2() + 1);
            }
            else
            {
                return 0;
            }
        }

        int32 getPitchBlockSizeLog2() const
        {
            return pitch_block_size_log2;
        }

        int32 getSliceBlockSizeLog2() const
        {
            return slice_block_size_log2;
        }
    };

    int32  shift_ceil(int32 a, int32 b);

    void getS3Start(int32 position, addr_params& p, int32& x, int32& y, int32& z);

    void getBlockSizeSlice(addr_params& p, int32& width, int32& height, int32& depth);
    void getBlockSizePitch(addr_params& p, int32& width, int32& height, int32& depth);
    void calcBlockSize(addr_params& p, int32 blockSizeLog2, int32& width, int32& height, int32& depth);

    void getMicroBlockSize(addr_params& p, int32& width, int32& height, int32& depth);

    //void getMipSize2d(addr_params& p, int mip, int32& width, int32& height);
    void getMipSize(addr_params& p, int32 mip, int32& width, int32& height, int32& depth);

    int  getNumMipsInTail(addr_params& p);
    void getMipInTaleMaxSize(addr_params& p, int32& max_mip_in_tail_width_log2, int32& max_mip_in_tail_height_log2);

    int32 calc_mip_in_tail(addr_params&	p,
                           int32	mipId,
                           int32	first_mip_in_fail);
    void getMipOffset(addr_params& p, int32 mip,
                      int64& data_offset, int64& meta_offset,
                      int32& mip_in_tail, int64& data_chain_size, int64& meta_chain_size);
    //void getMipOffsetData(addr_params& p, int32 mip, int64& data_offset,
    //                      int32& mip_in_tail, int64& data_chain_size);

    int32 calc_byte_offset(addr_params& p, int32 mip_in_tail);
    void getMipOrigin(addr_params& p, int32 mip_in_tail, int32& mip_x, int32& mip_y, int32& mip_z);

    void getXYZoffsets(addr_params& p, int32 x, int32 y, int32 z, int32 mip_in_tail,
                       int32& x_offset, int32& y_offset, int32& z_offset,
                       int32& x_mip_orig, int32& y_mip_orig, int32& z_mip_orig);

    bool getXYZblockIndexes(
#ifndef ADDR_SHARED
                            bool check_assert,	// Not used in SW
#endif
                            addr_params& p,
                            int32 x, int32 y, int32 z, int32 mip_in_tail,
                            int32 pitch_in_elements, int64 slice_in_elements,
                            int64& z_macro_block_index, int64& yx_macro_block_index);

}

#endif
