/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
#ifndef _AMDGPU_SHARED_H_
#define _AMDGPU_SHARED_H_

typedef enum _AMDGPU_PIXEL_FORMAT
{
   AMDGPU_PIXEL_FORMAT__INVALID                 = 0x00000000,
   AMDGPU_PIXEL_FORMAT__8                       = 0x00000001,
   AMDGPU_PIXEL_FORMAT__4_4                     = 0x00000002,
   AMDGPU_PIXEL_FORMAT__3_3_2                   = 0x00000003,
   AMDGPU_PIXEL_FORMAT__RESERVED_4              = 0x00000004,
   AMDGPU_PIXEL_FORMAT__16                      = 0x00000005,
   AMDGPU_PIXEL_FORMAT__16_FLOAT                = 0x00000006,
   AMDGPU_PIXEL_FORMAT__8_8                     = 0x00000007,
   AMDGPU_PIXEL_FORMAT__5_6_5                   = 0x00000008,
   AMDGPU_PIXEL_FORMAT__6_5_5                   = 0x00000009,
   AMDGPU_PIXEL_FORMAT__1_5_5_5                 = 0x0000000a,
   AMDGPU_PIXEL_FORMAT__4_4_4_4                 = 0x0000000b,
   AMDGPU_PIXEL_FORMAT__5_5_5_1                 = 0x0000000c,
   AMDGPU_PIXEL_FORMAT__32                      = 0x0000000d,
   AMDGPU_PIXEL_FORMAT__32_FLOAT                = 0x0000000e,
   AMDGPU_PIXEL_FORMAT__16_16                   = 0x0000000f,
   AMDGPU_PIXEL_FORMAT__16_16_FLOAT             = 0x00000010,
   AMDGPU_PIXEL_FORMAT__8_24                    = 0x00000011,
   AMDGPU_PIXEL_FORMAT__8_24_FLOAT              = 0x00000012,
   AMDGPU_PIXEL_FORMAT__24_8                    = 0x00000013,
   AMDGPU_PIXEL_FORMAT__24_8_FLOAT              = 0x00000014,
   AMDGPU_PIXEL_FORMAT__10_11_11                = 0x00000015,
   AMDGPU_PIXEL_FORMAT__10_11_11_FLOAT          = 0x00000016,
   AMDGPU_PIXEL_FORMAT__11_11_10                = 0x00000017,
   AMDGPU_PIXEL_FORMAT__11_11_10_FLOAT          = 0x00000018,
   AMDGPU_PIXEL_FORMAT__2_10_10_10              = 0x00000019,
   AMDGPU_PIXEL_FORMAT__8_8_8_8                 = 0x0000001a,
   AMDGPU_PIXEL_FORMAT__10_10_10_2              = 0x0000001b,
   AMDGPU_PIXEL_FORMAT__X24_8_32_FLOAT          = 0x0000001c,
   AMDGPU_PIXEL_FORMAT__32_32                   = 0x0000001d,
   AMDGPU_PIXEL_FORMAT__32_32_FLOAT             = 0x0000001e,
   AMDGPU_PIXEL_FORMAT__16_16_16_16             = 0x0000001f,
   AMDGPU_PIXEL_FORMAT__16_16_16_16_FLOAT       = 0x00000020,
   AMDGPU_PIXEL_FORMAT__RESERVED_33             = 0x00000021,
   AMDGPU_PIXEL_FORMAT__32_32_32_32             = 0x00000022,
   AMDGPU_PIXEL_FORMAT__32_32_32_32_FLOAT       = 0x00000023,
   AMDGPU_PIXEL_FORMAT__RESERVED_36             = 0x00000024,
   AMDGPU_PIXEL_FORMAT__1                       = 0x00000025,
   AMDGPU_PIXEL_FORMAT__1_REVERSED              = 0x00000026,
   AMDGPU_PIXEL_FORMAT__GB_GR                   = 0x00000027,
   AMDGPU_PIXEL_FORMAT__BG_RG                   = 0x00000028,
   AMDGPU_PIXEL_FORMAT__32_AS_8                 = 0x00000029,
   AMDGPU_PIXEL_FORMAT__32_AS_8_8               = 0x0000002a,
   AMDGPU_PIXEL_FORMAT__5_9_9_9_SHAREDEXP       = 0x0000002b,
   AMDGPU_PIXEL_FORMAT__8_8_8                   = 0x0000002c,
   AMDGPU_PIXEL_FORMAT__16_16_16                = 0x0000002d,
   AMDGPU_PIXEL_FORMAT__16_16_16_FLOAT          = 0x0000002e,
   AMDGPU_PIXEL_FORMAT__32_32_32                = 0x0000002f,
   AMDGPU_PIXEL_FORMAT__32_32_32_FLOAT          = 0x00000030,
   AMDGPU_PIXEL_FORMAT__BC1                     = 0x00000031,
   AMDGPU_PIXEL_FORMAT__BC2                     = 0x00000032,
   AMDGPU_PIXEL_FORMAT__BC3                     = 0x00000033,
   AMDGPU_PIXEL_FORMAT__BC4                     = 0x00000034,
   AMDGPU_PIXEL_FORMAT__BC5                     = 0x00000035,
   AMDGPU_PIXEL_FORMAT__BC6                     = 0x00000036,
   AMDGPU_PIXEL_FORMAT__BC7                     = 0x00000037,
   AMDGPU_PIXEL_FORMAT__32_AS_32_32_32_32       = 0x00000038,
   AMDGPU_PIXEL_FORMAT__APC3                    = 0x00000039,
   AMDGPU_PIXEL_FORMAT__APC4                    = 0x0000003a,
   AMDGPU_PIXEL_FORMAT__APC5                    = 0x0000003b,
   AMDGPU_PIXEL_FORMAT__APC6                    = 0x0000003c,
   AMDGPU_PIXEL_FORMAT__APC7                    = 0x0000003d,
   AMDGPU_PIXEL_FORMAT__CTX1                    = 0x0000003e,
   AMDGPU_PIXEL_FORMAT__40BPP                   = 0x0000003f,
} AMDGPU_PIXEL_FORMAT;

typedef enum _AMDGPU_TILE_MODE
{
   AMDGPU_TILE_MODE__DEFAULT                    = 0,
   AMDGPU_TILE_MODE__NONE                       = 1,
   AMDGPU_TILE_MODE__MACRO                      = 2,
   AMDGPU_TILE_MODE__MICRO                      = 3,
   AMDGPU_TILE_MODE__MACRO_MICRO                = 4,
   AMDGPU_TILE_MODE__16Z                        = 5,
   AMDGPU_TILE_MODE__32Z                        = 6,
   AMDGPU_TILE_MODE__MICRO_4X4                  = 7,
   AMDGPU_TILE_MODE__MACRO_MICRO_4X4            = 8,

   // R600 TODO:
   // to remove two defines below after other components will
   // get rid of using them.
   AMDGPU_TILE_MODE__2D                         = AMDGPU_TILE_MODE__MACRO,
   AMDGPU_TILE_MODE__3D_SLICE                   = 9,

   // below R600 specific tiling modes have been placed
   AMDGPU_TILE_MODE__LINEAR_GENERAL             = 0x09,
   AMDGPU_TILE_MODE__LINEAR_ALIGNED             = AMDGPU_TILE_MODE__NONE,
   AMDGPU_TILE_MODE__1D_TILED_THIN1             = AMDGPU_TILE_MODE__MICRO,
   AMDGPU_TILE_MODE__1D_TILED_THICK             = 0x0a,
   AMDGPU_TILE_MODE__2D_TILED_THIN1             = 0x0b,
   AMDGPU_TILE_MODE__2D_TILED_THIN2             = 0x0c,
   AMDGPU_TILE_MODE__2D_TILED_THIN4             = 0x0d,
   AMDGPU_TILE_MODE__2D_TILED_THICK             = 0x0e,
   AMDGPU_TILE_MODE__2B_TILED_THIN1             = AMDGPU_TILE_MODE__MACRO_MICRO,
   AMDGPU_TILE_MODE__2B_TILED_THIN2             = 0x0f,
   AMDGPU_TILE_MODE__2B_TILED_THIN4             = 0x10,
   AMDGPU_TILE_MODE__2B_TILED_THICK             = 0x11,
   AMDGPU_TILE_MODE__3D_TILED_THIN1             = 0x12,
   AMDGPU_TILE_MODE__3D_TILED_THICK             = 0x13,
   AMDGPU_TILE_MODE__3B_TILED_THIN1             = 0x14,
   AMDGPU_TILE_MODE__3B_TILED_THICK             = 0x15,
   AMDGPU_TILE_MODE__2D_TILED_XTHICK            = 0x16,
   AMDGPU_TILE_MODE__3D_TILED_XTHICK            = 0x17,
   AMDGPU_TILE_MODE__PRT_TILED_THIN1            = 0x18,
   AMDGPU_TILE_MODE__PRT_TILED_THICK            = 0x19,
   AMDGPU_TILE_MODE__PRT_2D_TILED_THIN1         = 0x1a,
   AMDGPU_TILE_MODE__PRT_2D_TILED_THICK         = 0x1b,
   AMDGPU_TILE_MODE__PRT_3D_TILED_THIN1         = 0x1c,
   AMDGPU_TILE_MODE__PRT_3D_TILED_THICK         = 0x1d,
   AMDGPU_TILE_MODE__INVALID                    = 0xffffffff
} AMDGPU_TILE_MODE;

typedef enum _AMDGPU_SWIZZLE_MODE
{
	AMDGPU_SWIZZLE_MODE_LINEAR          = 0,
	AMDGPU_SWIZZLE_MODE_256B_S          = 1,
	AMDGPU_SWIZZLE_MODE_256B_D          = 2,
	AMDGPU_SWIZZLE_MODE_256B_R          = 3,
	AMDGPU_SWIZZLE_MODE_4KB_Z           = 4,
	AMDGPU_SWIZZLE_MODE_4KB_S           = 5,
	AMDGPU_SWIZZLE_MODE_4KB_D           = 6,
	AMDGPU_SWIZZLE_MODE_4KB_R           = 7,
	AMDGPU_SWIZZLE_MODE_64KB_Z          = 8,
	AMDGPU_SWIZZLE_MODE_64KB_S          = 9,
	AMDGPU_SWIZZLE_MODE_64KB_D          = 10,
	AMDGPU_SWIZZLE_MODE_64KB_R          = 11,
	AMDGPU_SWIZZLE_MODE_VAR_Z           = 12,
	AMDGPU_SWIZZLE_MODE_VAR_S           = 13,
	AMDGPU_SWIZZLE_MODE_VAR_D           = 14,
	AMDGPU_SWIZZLE_MODE_VAR_R           = 15,
	AMDGPU_SWIZZLE_MODE_64KB_Z_T        = 16,
	AMDGPU_SWIZZLE_MODE_64KB_S_T        = 17,
	AMDGPU_SWIZZLE_MODE_64KB_D_T        = 18,
	AMDGPU_SWIZZLE_MODE_64KB_R_T        = 19,
	AMDGPU_SWIZZLE_MODE_4KB_Z_X         = 20,
	AMDGPU_SWIZZLE_MODE_4KB_S_X         = 21,
	AMDGPU_SWIZZLE_MODE_4KB_D_X         = 22,
	AMDGPU_SWIZZLE_MODE_4KB_R_X         = 23,
	AMDGPU_SWIZZLE_MODE_64KB_Z_X        = 24,
	AMDGPU_SWIZZLE_MODE_64KB_S_X        = 25,
	AMDGPU_SWIZZLE_MODE_64KB_D_X        = 26,
	AMDGPU_SWIZZLE_MODE_64KB_R_X        = 27,
	AMDGPU_SWIZZLE_MODE_VAR_Z_X         = 28,
	AMDGPU_SWIZZLE_MODE_VAR_S_X         = 29,
	AMDGPU_SWIZZLE_MODE_VAR_D_X         = 30,
	AMDGPU_SWIZZLE_MODE_VAR_R_X         = 31,
	AMDGPU_SWIZZLE_MODE_LINEAR_GENERAL  = 32,
	AMDGPU_SWIZZLE_MODE_MAX_TYPE        = 33,

	// Used for represent block with identical size
	AMDGPU_SWIZZLE_MODE_256B            = AMDGPU_SWIZZLE_MODE_256B_S,
	AMDGPU_SWIZZLE_MODE_4KB             = AMDGPU_SWIZZLE_MODE_4KB_S,
	AMDGPU_SWIZZLE_MODE_64KB            = AMDGPU_SWIZZLE_MODE_64KB_S,
	AMDGPU_SWIZZLE_MODE_VAR             = AMDGPU_SWIZZLE_MODE_VAR_S,
} AMDGPU_SWIZZLE_MODE;

typedef enum _AMDGPU_ADDR_RESOURCE_TYPE
{
    AMDGPU_ADDR_RSRC_TEX_1D = 0,
    AMDGPU_ADDR_RSRC_TEX_2D = 1,
    AMDGPU_ADDR_RSRC_TEX_3D = 2,
    AMDGPU_ADDR_RSRC_MAX_TYPE = 3,
} AMDGPU_ADDR_RESOURCE_TYPE;

typedef enum _AMDGPU_MICRO_TILE_MODE
{
   /* Displayable tiling */
   AMDGPU_MICRO_TILE_MODE__DISPLAYABLE          = 0,

   /* Non-displayable tiling, a.k.a thin micro tiling */
   AMDGPU_MICRO_TILE_MODE__NON_DISPLAYABLE      = 1,

   /* Same as non-displayable plus depth-sample-order */
   AMDGPU_MICRO_TILE_MODE__DEPTH_SAMPLE_ORDER   = 2,

   /* Rotated displayable tiling */
   AMDGPU_MICRO_TILE_MODE__ROTATED              = 3,

   /* Thick micro-tiling, only valid for THICK and XTHICK */
   AMDGPU_MICRO_TILE_MODE__THICK                = 4,

   AMDGPU_MICRO_TILE_MODE__INVALID              =0xffffffff
} AMDGPU_MICRO_TILE_MODE;

typedef enum _AMDGPU_PIPE_CFG
{
    AMDGPU_PIPE_CFG__INVALID                    = 0,
    AMDGPU_PIPE_CFG__P2                         = 1,  /* 2 pipes */
    AMDGPU_PIPE_CFG__P4_8x16                    = 5,  /* 4 pipes */
    AMDGPU_PIPE_CFG__P4_16x16                   = 6,
    AMDGPU_PIPE_CFG__P4_16x32                   = 7,
    AMDGPU_PIPE_CFG__P4_32x32                   = 8,
    AMDGPU_PIPE_CFG__P8_16x16_8x16              = 9,  /* 8 pipes */
    AMDGPU_PIPE_CFG__P8_16x32_8x16              = 10,
    AMDGPU_PIPE_CFG__P8_32x32_8x16              = 11,
    AMDGPU_PIPE_CFG__P8_16x32_16x16             = 12,
    AMDGPU_PIPE_CFG__P8_32x32_16x16             = 13,
    AMDGPU_PIPE_CFG__P8_32x32_16x32             = 14,
    AMDGPU_PIPE_CFG__P8_32x64_32x32             = 15,
    AMDGPU_PIPE_CFG__P16_32x32_8x16             = 17, /* 16 pipes */
    AMDGPU_PIPE_CFG__P16_32x32_16x16            = 18,
    AMDGPU_PIPE_CFG__MAX                        = 19,
} AMDGPU_PIPE_CFG;

typedef struct _amdgpu_tile_cfg
{
   /* Number of banks, numerical value */
   uint32_t banks;

   /* Number of tiles in the X direction in the same bank */
   uint32_t bank_width;

   /* Number of tiles in the Y direction in the same bank */
   uint32_t bank_height;

   /* Macro tile aspect ratio. 1-1:1, 2-4:1, 4-16:1, 8-64:1 */
   uint32_t macro_aspect_ratio;

   /* Tile split size, in bytes */
   uint32_t tile_split_bytes;

   AMDGPU_PIPE_CFG pipe_config;
} amdgpu_tile_cfg;

//
/// Internal flags set for opening shared metadata path.
//
typedef union _amdgpu_shared_metadata_flags
{
    struct
    {
        uint32_t shader_fetchable:              1; ///< Main metadata is shader fetchable
        uint32_t shader_fetchable_fmask:        1; ///< In case the FMASK shader-fetchable is different from main metadata
        uint32_t has_wa_tc_compat_z_range:      1; ///< Extra per-mip uint32 reserved after fast-clear-value
        uint32_t has_eq_gpu_access:             1; ///< Metadata equation for GPU access following main metadata (DCC or HTILE)
        uint32_t has_htile_lookup_table:        1; ///< Htile look-up table for each mip and slice
        uint32_t htile_as_fmask_xor:            1; ///< Indicate htileOffset is used as Fmask Xor Setting.
        uint32_t reserved:                     26;
    };
    uint32_t all32;
} amdgpu_shared_metadata_flags;

//
/// Shared metadata info to be used for opened optimally shared image.
//
typedef struct _amdgpu_shared_metadata_info
{
    amdgpu_shared_metadata_flags flags;
    uint32_t            dcc_offset;
    uint32_t            cmask_offset;
    uint32_t            fmask_offset;
    uint32_t            htile_offset;
    uint32_t            dcc_state_offset;
    uint32_t            fast_clear_value_offset;
    uint32_t            fce_state_offset;
    uint32_t            htile_lookup_table_offset;
    uint32_t            resource_id;  ///< This id is a unique name for the cross-process shared memory used to pass extra
                                      ///< information. Currently it's composed by the image object pointer and process id.
} amdgpu_shared_metadata_info;

typedef struct _amdgpu_bo_umd_metadata
{
   uint32_t width_in_pixels;
   uint32_t height;
   uint32_t aligned_pitch_in_bytes;
   uint32_t aligned_height;

   AMDGPU_PIXEL_FORMAT      format;
   union {
            struct
            {
                   int32_t  tile_index;
                   AMDGPU_TILE_MODE         tile_mode;
                   AMDGPU_MICRO_TILE_MODE   micro_tile_mode;
                   amdgpu_tile_cfg          tile_config;
            };
            struct
            {
                    AMDGPU_SWIZZLE_MODE       swizzleMode;       ///< Swizzle Mode for Gfx9
                    AMDGPU_ADDR_RESOURCE_TYPE resourceType;      ///< Surface type
            };
    };
    uint32_t    pipeBankXor;            ///< Pipe bank Xor
    uint32_t    depth;                  ///< Image depth
    uint32_t    array_size;             ///< Array size
    union
    {
        struct
        {
            uint32_t                  mip_levels:       8;  ///< Mip levels
            AMDGPU_ADDR_RESOURCE_TYPE resource_type:    3;  ///< Resource dimensions
            uint32_t                  texture:          1;
            uint32_t                  unodered_access:  1;
            uint32_t                  render_target:    1;
            uint32_t                  depth_stencil:    1;
            uint32_t                  cubemap:          1;
            uint32_t                  optimal_shareable:1;
            uint32_t                  samples:          7;
            uint32_t                  reserved:         8;
        };
        uint32_t    all32;
    } flags;
    amdgpu_shared_metadata_info shared_metadata_info;
} amdgpu_bo_umd_metadata;

#define PRO_UMD_METADATA_OFFSET_DWORD 32
#define PRO_UMD_METADATA_SIZE (PRO_UMD_METADATA_OFFSET_DWORD * 4 + sizeof(amdgpu_bo_umd_metadata))

#endif
