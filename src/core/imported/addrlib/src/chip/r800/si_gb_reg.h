/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2007-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if !defined (__SI_GB_REG_H__)
#define __SI_GB_REG_H__

/*****************************************************************************************************************
 *
 *  si_gb_reg.h
 *
 *  Register Spec Release:  Chip Spec 0.28
 *
 *****************************************************************************************************************/

//
// Make sure the necessary endian defines are there.
//
#if defined(LITTLEENDIAN_CPU)
#elif defined(BIGENDIAN_CPU)
#else
#error "BIGENDIAN_CPU or LITTLEENDIAN_CPU must be defined"
#endif

/*
 * GB_ADDR_CONFIG struct
 */

#if     defined(LITTLEENDIAN_CPU)

     typedef struct _GB_ADDR_CONFIG_T {
          unsigned int num_pipes                      : 3;
          unsigned int                                : 1;
          unsigned int pipe_interleave_size           : 3;
          unsigned int                                : 1;
          unsigned int bank_interleave_size           : 3;
          unsigned int                                : 1;
          unsigned int num_shader_engines             : 2;
          unsigned int                                : 2;
          unsigned int shader_engine_tile_size        : 3;
          unsigned int                                : 1;
          unsigned int num_gpus                       : 3;
          unsigned int                                : 1;
          unsigned int multi_gpu_tile_size            : 2;
          unsigned int                                : 2;
          unsigned int row_size                       : 2;
          unsigned int num_lower_pipes                : 1;
          unsigned int                                : 1;
     } GB_ADDR_CONFIG_T;

#elif       defined(BIGENDIAN_CPU)

     typedef struct _GB_ADDR_CONFIG_T {
          unsigned int                                : 1;
          unsigned int num_lower_pipes                : 1;
          unsigned int row_size                       : 2;
          unsigned int                                : 2;
          unsigned int multi_gpu_tile_size            : 2;
          unsigned int                                : 1;
          unsigned int num_gpus                       : 3;
          unsigned int                                : 1;
          unsigned int shader_engine_tile_size        : 3;
          unsigned int                                : 2;
          unsigned int num_shader_engines             : 2;
          unsigned int                                : 1;
          unsigned int bank_interleave_size           : 3;
          unsigned int                                : 1;
          unsigned int pipe_interleave_size           : 3;
          unsigned int                                : 1;
          unsigned int num_pipes                      : 3;
     } GB_ADDR_CONFIG_T;

#endif

#if     defined(LITTLEENDIAN_CPU)

     typedef struct _GB_ADDR_CONFIG_N {
          unsigned int num_pipes                      : 3;
          unsigned int pipe_interleave_size           : 3;
          unsigned int max_compressed_frags           : 2;
          unsigned int bank_interleave_size           : 3;
          unsigned int                                : 1;
          unsigned int num_banks                      : 3;
          unsigned int                                : 1;
          unsigned int shader_engine_tile_size        : 3;
          unsigned int num_shader_engines             : 2;
          unsigned int num_gpus                       : 3;
          unsigned int multi_gpu_tile_size            : 2;
          unsigned int num_rb_per_se                  : 2;
          unsigned int row_size                       : 2;
          unsigned int num_lower_pipes                : 1;
          unsigned int se_enable                      : 1;
     } GB_ADDR_CONFIG_N;

#elif       defined(BIGENDIAN_CPU)

     typedef struct _GB_ADDR_CONFIG_N {
          unsigned int se_enable                      : 1;
          unsigned int num_lower_pipes                : 1;
          unsigned int row_size                       : 2;
          unsigned int num_rb_per_se                  : 2;
          unsigned int multi_gpu_tile_size            : 2;
          unsigned int num_gpus                       : 3;
          unsigned int num_shader_engines             : 2;
          unsigned int shader_engine_tile_size        : 3;
          unsigned int                                : 1;
          unsigned int num_banks                      : 3;
          unsigned int                                : 1;
          unsigned int bank_interleave_size           : 3;
          unsigned int max_compressed_frags           : 2;
          unsigned int pipe_interleave_size           : 3;
          unsigned int num_pipes                      : 3;
     } GB_ADDR_CONFIG_N;

#endif

typedef union {
     unsigned int val : 32;
     GB_ADDR_CONFIG_T f;
     GB_ADDR_CONFIG_N n;
} GB_ADDR_CONFIG;

#if       defined(LITTLEENDIAN_CPU)

     typedef struct _GB_TILE_MODE_T {
          unsigned int micro_tile_mode                : 2;
          unsigned int array_mode                     : 4;
          unsigned int pipe_config                    : 5;
          unsigned int tile_split                     : 3;
          unsigned int bank_width                     : 2;
          unsigned int bank_height                    : 2;
          unsigned int macro_tile_aspect              : 2;
          unsigned int num_banks                      : 2;
          unsigned int micro_tile_mode_new            : 3;
          unsigned int sample_split                   : 2;
          unsigned int alt_pipe_config                : 5;
     } GB_TILE_MODE_T;

     typedef struct _GB_MACROTILE_MODE_T {
          unsigned int bank_width                     : 2;
          unsigned int bank_height                    : 2;
          unsigned int macro_tile_aspect              : 2;
          unsigned int num_banks                      : 2;
          unsigned int alt_bank_height                : 2;
          unsigned int alt_macro_tile_aspect          : 2;
          unsigned int alt_num_banks                  : 2;
          unsigned int                                : 18;
     } GB_MACROTILE_MODE_T;

#elif          defined(BIGENDIAN_CPU)

     typedef struct _GB_TILE_MODE_T {
          unsigned int alt_pipe_config                : 5;
          unsigned int sample_split                   : 2;
          unsigned int micro_tile_mode_new            : 3;
          unsigned int num_banks                      : 2;
          unsigned int macro_tile_aspect              : 2;
          unsigned int bank_height                    : 2;
          unsigned int bank_width                     : 2;
          unsigned int tile_split                     : 3;
          unsigned int pipe_config                    : 5;
          unsigned int array_mode                     : 4;
          unsigned int micro_tile_mode                : 2;
     } GB_TILE_MODE_T;

     typedef struct _GB_MACROTILE_MODE_T {
          unsigned int                                : 18;
          unsigned int alt_num_banks                  : 2;
          unsigned int alt_macro_tile_aspect          : 2;
          unsigned int alt_bank_height                : 2;
          unsigned int num_banks                      : 2;
          unsigned int macro_tile_aspect              : 2;
          unsigned int bank_height                    : 2;
          unsigned int bank_width                     : 2;
     } GB_MACROTILE_MODE_T;

#endif

typedef union {
     unsigned int val : 32;
     GB_TILE_MODE_T f;
} GB_TILE_MODE;

typedef union {
     unsigned int val : 32;
     GB_MACROTILE_MODE_T f;
} GB_MACROTILE_MODE;

#endif

