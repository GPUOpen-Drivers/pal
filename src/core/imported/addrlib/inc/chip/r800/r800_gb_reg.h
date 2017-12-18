#if !defined (__R800_GB_REG_H__)
#define __R800_GB_REG_H__

/*****************************************************************************************************************
 *
 *  r800_gb_reg.h
 *
 *  Register Spec Release:  Chip Spec 0.28
 *
 *  Trade secret of ATI Technologies Inc.
 *  Unpublished work, Copyright 2005 ATI Technologies Inc.
 *
 *  All rights reserved.  This notice is intended as a precaution against
 *  inadvertent publication and does not imply publication or any waiver
 *  of confidentiality.  The year included in the foregoing notice is the
 *  year of creation of the work.
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

