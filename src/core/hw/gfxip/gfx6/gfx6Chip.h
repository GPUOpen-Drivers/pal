/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "pal.h"
#include "palAssert.h"
#include "palDevice.h"
#include "palSparseVector.h"

#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfx6/chip/si_ci_vi_merged_enum.h"
#include "core/hw/gfxip/gfx6/chip/si_ci_vi_merged_mask.h"
#include "core/hw/gfxip/gfx6/chip/si_ci_vi_merged_shift.h"
#include "core/hw/gfxip/gfx6/chip/si_ci_vi_merged_offset.h"
#include "core/hw/gfxip/gfx6/chip/si_ci_vi_merged_registers.h"
#include "core/hw/gfxip/gfx6/chip/si_ci_vi_merged_typedef.h"
#include "core/hw/gfxip/gfx6/chip/si_ci_vi_merged_pm4_it_opcodes.h"
#include "core/hw/gfxip/gfx6/chip/si_ci_vi_merged_pm4defs.h"

// Put newly added registers definitions here to avoid getting lost when HW chip headers get regenerated,
// Registers here can be simply removed once they are in place in HW chip header files over the time
#define mmPA_SU_SMALL_PRIM_FILTER_CNTL__VI               0xA20C

union PA_SU_SMALL_PRIM_FILTER_CNTL__VI {
    struct {
        unsigned int                      SMALL_PRIM_FILTER_ENABLE : 1;
        unsigned int                      TRIANGLE_FILTER_DISABLE  : 1;
        unsigned int                      LINE_FILTER_DISABLE      : 1;
        unsigned int                      POINT_FILTER_DISABLE     : 1;
        unsigned int                      RECTANGLE_FILTER_DISABLE : 1;
        unsigned int : 27;
    } bitfields, bits;
    unsigned int u32All;
    signed int   i32All;
    float        f32All;
};

typedef union PA_SU_SMALL_PRIM_FILTER_CNTL__VI  regPA_SU_SMALL_PRIM_FILTER_CNTL__VI;

namespace Pal
{

// This is for HW and SC to use undefined 27:30 4 bits in WORD4,
union Gfx6ImageSrdWord4
{
    struct
    {
        uint32 DEPTH            : 13;  // Same as SQ_IMG_RSRC_WORD4
        uint32 PITCH            : 14;  // Same as SQ_IMG_RSRC_WORD4
        uint32 samplePatternIdx : 4;   // Unused bits in SRD hijacked to select the sample pattern
                                       // palette to be read on any samplepos instructions.
        uint32 reserved         : 1;
    } bitfields, bits;
    uint32 u32All;
    int32  i32All;
    float  f32All;
};

namespace Gfx6
{

// Describes the layout of the index buffer attributes used by a INDEXATTRIBUTESINDIRECT packet.
struct IndexAttribIndirect
{
    uint32  gpuVirtAddrLo;
    uint32  gpuVirtAddrHi;
    uint32  indexBufferSize;    // Index buffer size in indices, not bytes!
    uint32  indexType;
};

// Width/height of a tile in pixels
constexpr uint32 TileWidth = 8;

// Number of tile pixels
constexpr uint32 TilePixels = 64;

// Context reg space technically goes to 0xAFFF (SI) or 0xBFFF (CI), but in reality there are no registers we currently
// write beyond 0xA38E. This enum can save some memory space in situations where we shadow register state in the driver.
constexpr uint32 CntxRegUsedRangeEnd  = 0xA38E;
constexpr uint32 CntxRegUsedRangeSize = (CntxRegUsedRangeEnd - CONTEXT_SPACE_START + 1);
constexpr uint32 CntxRegCountGfx6     = (CONTEXT_SPACE_END__SI - CONTEXT_SPACE_START + 1);
constexpr uint32 CntxRegCountGfx7     = (CONTEXT_SPACE_END__CI__VI - CONTEXT_SPACE_START + 1);

// SH reg space technically goes to 0x2FFF, but in reality there are no registers we currently write beyond 0x2E4F. This
// enum can save some memory space in situations where we shadow register state in the driver.
constexpr uint32 ShRegUsedRangeEnd  = 0x2E4F;
constexpr uint32 ShRegUsedRangeSize = (ShRegUsedRangeEnd - PERSISTENT_SPACE_START + 1);
constexpr uint32 ShRegCount         = (PERSISTENT_SPACE_END - PERSISTENT_SPACE_START + 1);

// Number of registers in config register space.
constexpr uint32 ConfigRegCountGfx6 = (CONFIG_SPACE_END__SI - CONFIG_SPACE_START + 1);
constexpr uint32 ConfigRegCountGfx7 = (CONFIG_SPACE_END__CI__VI - CONFIG_SPACE_START + 1);

// Number of registers in user-config register space.
constexpr uint32 UserConfigRegCount = (UCONFIG_SPACE_END__CI__VI - UCONFIG_SPACE_START__CI__VI + 1);

// Defines a range of registers to be loaded from state-shadow memory into state registers.
struct RegisterRange
{
    uint32 regOffset;   // Offset to the first register to load. Relative to the base address of the register type.
                        // E.g., PERSISTENT_SPACE_START for SH registers, etc.
    uint32 regCount;    // Number of registers to load.
};

// Container used for storing registers during pipeline load.
using RegisterVector = Util::SparseVector<
    uint32,
    uint8,
    50,
    Platform,
    CONTEXT_SPACE_START,    CntxRegUsedRangeEnd,
    PERSISTENT_SPACE_START, ShRegUsedRangeEnd>;

// Number of user-data registers per shader stage on the chip. PAL reserves a number of these for internal use, making
// them unusable from the client. The registers PAL reserves are:
//
// [0]  - For the global internal resource table (shader rings, offchip LDS buffers, etc.)
// [1]  - For the constant buffer table (internal constant buffers, etc.)
// [15] - For the ES/GS LDS size when on-chip GS is enabled
//
// This leaves registers [2,14] available for the client's use.
constexpr uint32 NumUserDataRegisters = 16;

// Starting user-data register index where the low 32 address bits of the global internal table pointer
// (shader ring SRDs, etc.) is written.
constexpr uint32 InternalTblStartReg = 0;
// Starting user-data register index where the low 32 address bits of the constant buffer table pointer
// (internal CBs) is written.
constexpr uint32 ConstBufTblStartReg = (InternalTblStartReg + 1);

// Starting user data register index where the client's fast user-data 'entries' are written.
constexpr uint32 FastUserDataStartReg = (ConstBufTblStartReg + 1);

// Number of PS input semantic registers.
constexpr uint32 MaxPsInputSemantics = 32;

// Number of VS export semantic registers.
constexpr uint32 MaxVsExportSemantics = 32;

// Number of SGPRs available to each wavefront.
// NOTE: Theoretically, we have 106 available SGPRs plus 2 for the VCC regs. However, the SPI_SHADER_PGM_RSRC1_*.SGPRS
// field is programmed in blocks of 8, making the this number ((106 + 2) & ~0x7), which is 104.
constexpr uint32 MaxSgprsAvailable = 104;

// Number of SGPRs available on HW with bug.  This creates a fixed pool of physical SGPR ranges such that a
// wave will never wrap around the end of the SGPR file - it either fits completely or not at all.  The chosen value of
// 96 SGPRs allows up to 8 waves per SIMD.  This range has to account for the fact that 16 additional physical SGPRs
// will be allocated when a trap handler is present.
constexpr uint32 MaxSgprsAvailableWithSpiBug            = 96;
constexpr uint32 MaxSgprsAvailableWithSpiBugTrapPresent = MaxSgprsAvailableWithSpiBug - 16;

// Gfx6 and some Gfx7 hardware are affected by an issue which can cause a GPU hang when any compute
// shader having more than 256 threads-per-group is running on either the graphics engine or the async compute engines.
// This is the number of threads-per-group limit for shaders which won't potentially trigger the bug.
constexpr uint32 ThreadsPerGroupForRegAllocFragmentationBug = 256;

// Number of SIMDs per Compute Unit
constexpr uint32 NumSimdPerCu = 4;

// The maximum number of waves per SIMD and Compute Unit.
constexpr uint32 NumWavesPerSimd = 10;
constexpr uint32 NumWavesPerCu   = NumWavesPerSimd * NumSimdPerCu;

// The hardware can only support a limited number of scratch waves per CU.
constexpr uint32 MaxScratchWavesPerCu = 32;

// The value of ONCHIP that is the field of register VGT_GS_MODE
constexpr uint32 VgtGsModeOnchip = 3;

// Highest index of the SET_BASE packet
constexpr uint32 MaxSetBaseIndex = BASE_INDEX_INDIRECT_DATA;

// Maximum image width
constexpr uint32 MaxImageWidth = 16384;

// Maximum image height
constexpr uint32 MaxImageHeight = 16384;

// Maximum image depth
constexpr uint32 MaxImageDepth = 8192;

// Maximum image mip levels. This was calculated from MaxImageWidth and MaxImageHeight
constexpr uint32 MaxImageMipLevels = 15;

// Maximum image array slices
constexpr uint32 MaxImageArraySlices = 2048;

// No current ASICs have more than 16 active RBs.
constexpr uint32 MaxNumRbs = 16;

// Tile size is fixed at 64kb on all hardware
constexpr uint32 PrtTileSize = (64 * 1024);

// GFX6 supports the following PRT features:
constexpr PrtFeatureFlags Gfx6PrtFeatures = static_cast<PrtFeatureFlags>(
    PrtFeatureBuffer            | // - sparse buffers
    PrtFeatureImage2D           | // - sparse 2D images
    PrtFeatureShaderStatus      | // - residency status in shader instructions
    PrtFeatureShaderLodClamp);    // - LOD clamping in shader instructions

// GFX7 supports the following PRT features:
constexpr PrtFeatureFlags Gfx7PrtFeatures = static_cast<PrtFeatureFlags>(
    Gfx6PrtFeatures                | // - all features supported by GFX6
    PrtFeatureUnalignedMipSize     | // - unaligned levels outside of the miptail
    PrtFeatureTileAliasing         | // - tile aliasing (without metadata)
    PrtFeatureStrictNull           | // - returning zeros for unmapped tiles
    PrtFeatureNonStandardImage3D);   // - limited support for sparse 3D images

// GFX8 supports the same PRT features as GFX7.
constexpr PrtFeatureFlags Gfx8PrtFeatures = Gfx7PrtFeatures;

// Buffer resource descriptor structure
struct BufferSrd
{
    SQ_BUF_RSRC_WORD0 word0;
    SQ_BUF_RSRC_WORD1 word1;
    SQ_BUF_RSRC_WORD2 word2;
    SQ_BUF_RSRC_WORD3 word3;
};

// Image resource descriptor structure
struct ImageSrd
{
    SQ_IMG_RSRC_WORD0 word0;
    SQ_IMG_RSRC_WORD1 word1;
    SQ_IMG_RSRC_WORD2 word2;
    SQ_IMG_RSRC_WORD3 word3;
    Gfx6ImageSrdWord4 word4;
    SQ_IMG_RSRC_WORD5 word5;
    SQ_IMG_RSRC_WORD6 word6;
    SQ_IMG_RSRC_WORD7 word7;
};

// Image sampler descriptor structure
struct SamplerSrd
{
    SQ_IMG_SAMP_WORD0 word0;
    SQ_IMG_SAMP_WORD1 word1;
    SQ_IMG_SAMP_WORD2 word2;
    SQ_IMG_SAMP_WORD3 word3;
};

// Horizontal min screen extent.
constexpr int32 MinHorzScreenCoord = -32768;
// Horizontal max screen extent.
constexpr int32 MaxHorzScreenCoord =  32768;
// Vertical min screen extent.
constexpr int32 MinVertScreenCoord = -32768;
// Vertical max screen extent.
constexpr int32 MaxVertScreenCoord =  32768;

// Maximum scissor rect value for the top-left corner.
constexpr uint32 ScissorMaxTL = 16383;
// Maximum scissor rect value for the bottom-right corner.
constexpr uint32 ScissorMaxBR = 16384;
// Minimal size for screen scissors
constexpr uint32 PaScScreenScissorMin = 0;
// Maximum size for screen scissors
constexpr uint32 PaScScreenScissorMax = 16384;

// Mask of CP_COHER_CNTL bits which perform a flush/inval of the L1 texture caches.
constexpr uint32 CpCoherCntlL1TexCacheMask = CP_COHER_CNTL__TCL1_ACTION_ENA_MASK;

// Mask of CP_COHER_CNTL bits which perform a flush/inval of all texture caches.
constexpr uint32 CpCoherCntlTexCacheMask = CP_COHER_CNTL__TC_ACTION_ENA_MASK   |
                                           CP_COHER_CNTL__TCL1_ACTION_ENA_MASK |
                                           CP_COHER_CNTL__SH_KCACHE_ACTION_ENA_MASK;

// Mask of CP_COHER_CNTL bits which perform a flush/inval of the CB cache and base addresses.
constexpr uint32 CpCoherCntlCbFlushMask = CP_COHER_CNTL__CB0_DEST_BASE_ENA_MASK |
                                          CP_COHER_CNTL__CB1_DEST_BASE_ENA_MASK |
                                          CP_COHER_CNTL__CB2_DEST_BASE_ENA_MASK |
                                          CP_COHER_CNTL__CB3_DEST_BASE_ENA_MASK |
                                          CP_COHER_CNTL__CB4_DEST_BASE_ENA_MASK |
                                          CP_COHER_CNTL__CB5_DEST_BASE_ENA_MASK |
                                          CP_COHER_CNTL__CB6_DEST_BASE_ENA_MASK |
                                          CP_COHER_CNTL__CB7_DEST_BASE_ENA_MASK |
                                          CP_COHER_CNTL__CB_ACTION_ENA_MASK;

// Mask of CP_COHER_CNTL bits which perform a flush/inval of the DB cache and depth base address.
constexpr uint32 CpCoherCntlDbDepthFlushMask = CP_COHER_CNTL__DB_DEST_BASE_ENA_MASK |
                                               CP_COHER_CNTL__DB_ACTION_ENA_MASK;

// Mask of CP_COHER_CNTL bits which perform a flush/inval of the DB cache and stencil base address.
constexpr uint32 CpCoherCntlDbStencilFlushMask = CP_COHER_CNTL__DEST_BASE_0_ENA_MASK |
                                                 CP_COHER_CNTL__DB_ACTION_ENA_MASK;

// Mask of CP_COHER_CNTL bits which perform a flush/inval of the DB cache and htile base address.
constexpr uint32 CpCoherCntlDbHtileFlushMask = CP_COHER_CNTL__DEST_BASE_1_ENA_MASK |
                                               CP_COHER_CNTL__DB_ACTION_ENA_MASK;

// On the compute engine setting the CB or DB related sync bits doesn't make a whole lot of sense. Setup a mask here of
// all the bits that we can safely set on compute.
constexpr uint32 CpCoherCntlComputeValidMask =  static_cast<uint32>(~(CP_COHER_CNTL__CB0_DEST_BASE_ENA_MASK |
                                                                      CP_COHER_CNTL__CB1_DEST_BASE_ENA_MASK |
                                                                      CP_COHER_CNTL__CB2_DEST_BASE_ENA_MASK |
                                                                      CP_COHER_CNTL__CB3_DEST_BASE_ENA_MASK |
                                                                      CP_COHER_CNTL__CB4_DEST_BASE_ENA_MASK |
                                                                      CP_COHER_CNTL__CB5_DEST_BASE_ENA_MASK |
                                                                      CP_COHER_CNTL__CB6_DEST_BASE_ENA_MASK |
                                                                      CP_COHER_CNTL__CB7_DEST_BASE_ENA_MASK |
                                                                      CP_COHER_CNTL__DB_DEST_BASE_ENA_MASK  |
                                                                      CP_COHER_CNTL__CB_ACTION_ENA_MASK     |
                                                                      CP_COHER_CNTL__DB_ACTION_ENA_MASK));

constexpr uint32 CpCoherCntlStallMask = CP_COHER_CNTL__CB0_DEST_BASE_ENA_MASK |
                                        CP_COHER_CNTL__CB1_DEST_BASE_ENA_MASK |
                                        CP_COHER_CNTL__CB2_DEST_BASE_ENA_MASK |
                                        CP_COHER_CNTL__CB3_DEST_BASE_ENA_MASK |
                                        CP_COHER_CNTL__CB4_DEST_BASE_ENA_MASK |
                                        CP_COHER_CNTL__CB5_DEST_BASE_ENA_MASK |
                                        CP_COHER_CNTL__CB6_DEST_BASE_ENA_MASK |
                                        CP_COHER_CNTL__CB7_DEST_BASE_ENA_MASK |
                                        CP_COHER_CNTL__DB_DEST_BASE_ENA_MASK  |
                                        CP_COHER_CNTL__DEST_BASE_0_ENA_MASK   |
                                        CP_COHER_CNTL__DEST_BASE_1_ENA_MASK   |
                                        CP_COHER_CNTL__DEST_BASE_2_ENA_MASK   |
                                        CP_COHER_CNTL__DEST_BASE_3_ENA_MASK;

// Cacheline size.
constexpr uint32 CacheLineBytes  = 64;
constexpr uint32 CacheLineDwords = (CacheLineBytes / sizeof(uint32));

// Base GPU virtual address for full-range surface sync.
constexpr gpusize FullSyncBaseAddr = 0;

// Size for full-range surface sync.  This is 64-bits wide, which is much more than the number of bits actually
// available, but this value provides an easy way (+1 == 0) to determine that a full-sync is underway.
constexpr gpusize FullSyncSize     = 0xFFFFFFFFFFFFFFFF;

// Maximum number of color render targets
constexpr uint32 MaxCbSlots = 8;

// Number of Registers per CB slot.
constexpr uint32 CbRegsPerSlot = (mmCB_COLOR1_BASE - mmCB_COLOR0_BASE);

// Number of Registers for MSAA sample locations per 2x2 Quad.
constexpr uint32 NumSampleQuadRegs = 4;

// GFXIP 6 and GFXIP 7+ have different interpretation of the LDS_SIZE register field: the granularity of the value in
// DWORDs and the amount of bits to shift are both different.
constexpr uint32 Gfx6LdsDwGranularity      = 64;
constexpr uint32 Gfx6LdsDwGranularityShift = 6;
constexpr uint32 Gfx7LdsDwGranularity      = 128;
constexpr uint32 Gfx7LdsDwGranularityShift = 7;

// Max size of primitives per subgroup for adjacency primitives or when GS instancing is used. This restriction is
// applicable only when onchip GS is used.
constexpr uint32 OnChipGsMaxPrimPerSubgrp = 128;

// Enumerates the valid texture perf modulation values.
enum class TexPerfModulation : uint32
{
    None    = 0,
    Min     = 1,
    Default = 4,
    Max     = 7,
};

// Shift the 64-bit wide address by 8 to get 256 byte-aligned address, and return the low DWORD of that shifted
// address.
//
// The maximum number of address bits which GFXIP 6+ supports is 48. Some parts are limited to 40 bits.
// For CI and above, the maximum number of address bits is 64bits.
PAL_INLINE uint32 Get256BAddrLo(
    gpusize virtAddr)
{
    PAL_ASSERT((virtAddr & 0xff) == 0);
    return static_cast<uint32>(virtAddr >> 8);
}

// Shift the 64-bit wide address by 8 to get 256 byte-aligned address, and return the high DWORD of that shifted
// address.
//
// The maximum number of address bits which GFXIP 6+ supports is 48. Some parts are limited to 40 bits.
// For CI and above, the maximum number of address bits is 64bits.
PAL_INLINE uint32 Get256BAddrHi(
    gpusize virtAddr)
{
    PAL_ASSERT((virtAddr & 0xFF) == 0);
    return static_cast<uint32>(virtAddr >> 40);
}

// HW enum for element size (it is missing from si_enum.h).
enum BUF_ELEMENT_SIZE : uint32
{
    BUF_ELEMENT_SIZE_2B  = 0,
    BUF_ELEMENT_SIZE_4B  = 1,
    BUF_ELEMENT_SIZE_8B  = 2,
    BUF_ELEMENT_SIZE_16B = 3,
};

// HW enum for index stride (it is missing from si_enum.h).
enum BUF_INDEX_STRIDE : uint32
{
    BUF_INDEX_STRIDE_8B  = 0,
    BUF_INDEX_STRIDE_16B = 1,
    BUF_INDEX_STRIDE_32B = 2,
    BUF_INDEX_STRIDE_64B = 3,
};

// Defines the structure of the 64-bit data reported by each RB for z-pass data
union OcclusionQueryResult
{
    uint64 data;

    struct
    {
        uint64 zPassData : 63;
        uint64 valid     : 1;
    } bits;
};

static_assert(sizeof(OcclusionQueryResult) == sizeof(uint64), "OcclusionQueryResult is the wrong size.");

// Defines the structure of a begin / end pair of data.
struct OcclusionQueryResultPair
{
    OcclusionQueryResult  begin;
    OcclusionQueryResult  end;
};

static_assert(sizeof(OcclusionQueryResultPair) == 16, "OcclusionQueryResultPair is the wrong size.");

// Enumerates the possible hardware stages which a shader can run as.
enum class HwShaderStage : uint32
{
    Ls = 0,
    Hs,
    Es,
    Gs,
    Vs,
    Ps,
    Cs,
};

// Number of valid hardware shader stages used in graphics pipelines.
constexpr uint32 NumHwShaderStagesGfx = (static_cast<uint32>(HwShaderStage::Ps) + 1);

// Base SPI user-data register addresses for client user-data entries per hardware shader stage.
constexpr uint16 FirstUserDataRegAddr[] =
{
    (mmSPI_SHADER_USER_DATA_LS_0 + FastUserDataStartReg), // Ls
    (mmSPI_SHADER_USER_DATA_HS_0 + FastUserDataStartReg), // Hs
    (mmSPI_SHADER_USER_DATA_ES_0 + FastUserDataStartReg), // Es
    (mmSPI_SHADER_USER_DATA_GS_0 + FastUserDataStartReg), // Gs
    (mmSPI_SHADER_USER_DATA_VS_0 + FastUserDataStartReg), // Vs
    (mmSPI_SHADER_USER_DATA_PS_0 + FastUserDataStartReg), // Ps
    (mmCOMPUTE_USER_DATA_0       + FastUserDataStartReg), // Cs
};
static_assert(Util::ArrayLen(FirstUserDataRegAddr) == NumHwShaderStagesGfx + 1,
              "FirstUserDataRegAddr[] array has the wrong number of elements!");

// This represents the mapping from virtualized user-data entries to physical SPI user-data registers for a single HW
// shader stage.
struct UserDataEntryMap
{
    // Each element of this array is the entry ID which is mapped to the user-SGPR associated with that array element.
    // The only elements in this array which are valid are ones whose index is less than userSgprCount.
    uint8  mappedEntry[NumUserDataRegisters - FastUserDataStartReg];
    uint8  userSgprCount;           // Number of valid entries in the mappedEntry array.
    uint16 firstUserSgprRegAddr;    // Address of the first user-SGPR which is mapped to user-data entries.
    // Address of the user-SGPR used for the spill table GPU virtual address for this stage.  Zero indicates that this
    // stage does not read any entries from the spill table.
    uint16 spillTableRegAddr;
};

// Special value indicating that a user-data entry is not mapped to a physical SPI register.
constexpr uint16 UserDataNotMapped = 0;

// This represents the mapping from virtualized user-data entries to physical SPI user-data registers for an entire
// graphics pipeline.
struct ComputePipelineSignature
{
    // User-data entry mapping for the lone compute HW shader stage: (CS)
    UserDataEntryMap  stage;

    // Register address for the GPU virtual address pointing to the internal constant buffer containing the number
    // of thread groups launched in a Dispatch operation. Two sequential SPI user-data registers are needed to store
    // the address, this is the address of the first one.
    uint16  numWorkGroupsRegAddr;

    // First user-data entry which is spilled to GPU memory. A value of 'NoUserDataSpilling' indicates the pipeline
    // does not spill user-data entries to memory.
    uint16  spillThreshold;

    // The number of 'important' user-data entries for this pipeline. This effectively equates to one plus the index
    // of the highest user-data entry accessed by the pipeline.
    uint16  userDataLimit;

    // Hash of CS stage user-data mapping, used to speed up pipeline binds.
    uint64  userDataHash;
};

// User-data signature for an unbound compute pipeline.
extern const ComputePipelineSignature NullCsSignature;

// This represents the mapping from virtualized user-data entries to physical SPI user-data registers for an entire
// graphics pipeline.
struct GraphicsPipelineSignature
{
    // User-data entry mapping for each graphics HW shader stage: (LS, HS, ES, GS, VS, PS)
    UserDataEntryMap  stage[NumHwShaderStagesGfx];

    // Register address for the GPU virtual address of the vertex buffer table used by this pipeline. Zero
    // indicates that the vertex buffer table is not accessed.
    uint16  vertexBufTableRegAddr;
    // Register address for the GPU virtual address of the stream-output table used by this pipeline. Zero
    // indicates that stream-output is not used by this pipeline.
    uint16  streamOutTableRegAddr;

    // Register address for the vertex ID offset of a draw. The instance ID offset is always the very next register.
    uint16  vertexOffsetRegAddr;
    // Register address for the draw index of a multi-draw. This is an optional feature of each pipeline, so it may
    // be unmapped.
    uint16  drawIndexRegAddr;

    // First user-data entry which is spilled to GPU memory. A value of 'NoUserDataSpilling' indicates the pipeline
    // does not spill user-data entries to memory.
    uint16  spillThreshold;

    // The number of 'important' user-data entries for this pipeline. This effectively equates to one plus the index
    // of the highest user-data entry accessed by the pipeline.
    uint16  userDataLimit;

    // Address of each shader stage's user-SGPR for view ID.  This is a compacted list, so it is not safe to assume
    // that each index of this array corresponds to the associated HW shader stage enum value.
    uint16  viewIdRegAddr[NumHwShaderStagesGfx];

    // Hash of each stages user-data mapping, used to speed up pipeline binds.
    uint64  userDataHash[NumHwShaderStagesGfx];
};

// User-data signature for an unbound graphics pipeline.
extern const GraphicsPipelineSignature NullGfxSignature;

// Special value indicating that a pipeline or shader does not need its user-data entries to be spilled.
constexpr uint16 NoUserDataSpilling = static_cast<uint16>(0xFFFF);

} // Gfx6
} // Pal
