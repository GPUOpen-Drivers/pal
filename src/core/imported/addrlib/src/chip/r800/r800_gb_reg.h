/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2007-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if !defined (__R800_GB_REG_H__)
#define __R800_GB_REG_H__

/*****************************************************************************************************************
 *
 *  r800_gb_reg.h
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

typedef union {
     unsigned int val : 32;
     GB_ADDR_CONFIG_T f;
} GB_ADDR_CONFIG;

#endif

