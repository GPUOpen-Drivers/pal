/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palInlineFuncs.h"
#include "palCmdBuffer.h"
#include "palSparseVector.h"

#include "core/hw/gfxip/gfx9/chip/gfx9_plus_merged_offset.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfx9/chip/gfx9_plus_merged_default.h"
#include "core/hw/gfxip/gfx9/chip/gfx9_plus_merged_enum.h"
#include "core/hw/gfxip/gfx9/chip/gfx9_plus_merged_mask.h"
#include "core/hw/gfxip/gfx9/chip/gfx9_plus_merged_shift.h"
#include "core/hw/gfxip/gfx9/chip/gfx9_plus_merged_registers.h"
#include "core/hw/gfxip/gfx9/chip/gfx9_plus_merged_typedef.h"

#include "core/hw/gfxip/gfx9/chip/gfx9_plus_merged_f32_ce_pm4_packets.h"  // constant engine
#include "core/hw/gfxip/gfx9/chip/gfx9_plus_merged_f32_mec_pm4_packets.h" // compute engine
#include "core/hw/gfxip/gfx9/chip/gfx9_plus_merged_f32_me_pm4_packets.h"  // micro-engine
#include "core/hw/gfxip/gfx9/chip/gfx9_plus_merged_f32_pfp_pm4_packets.h" // pre-fetch-parser
#include "core/hw/gfxip/gfx9/chip/gfx9_plus_merged_pm4_it_opcodes.h"

namespace Pal
{
namespace Gfx9
{

// Describes the layout of the index buffer attributes used by a INDEX_ATTRIBUTES_INDIRECT packet.
struct IndexAttribIndirect
{
    uint32  gpuVirtAddrLo;
    uint32  gpuVirtAddrHi;
    uint32  indexBufferSize;    // Index buffer size in indices, not bytes!
    uint32  indexType;
};

// Describes the layout of the index buffer attributes and any additional information for NGG fast launch indexed draws.
struct NggIndexAttrIndirect
{
    IndexAttribIndirect attributes;    // Attributes for when the pipeline is NGG.
    uint32              log2IndexSize; // Log2(sizeof(indexType)) for NGG pipelines.
};

// Describes the layout of the index buffer state data that is passed to a nested command buffer.
struct IndexBufferStateIndirect
{
    IndexAttribIndirect  attributes;    // Attributes for when the pipeline is non-NGG.
    NggIndexAttrIndirect ngg;           // Attributes for when the pipeline is NGG.
};

// Log2(sizeof(indexType)) for use by the NGG fast launch shader for indexed draws.
constexpr uint32 Log2IndexSize[] =
{
    1,  // Log2(sizeof(VGT_INDEX_16))
    2,  // Log2(sizeof(VGT_INDEX_32))
    0,  // Log2(sizeof(VGT_INDEX_8))
};

static_assert((VGT_INDEX_16 == 0) && (VGT_INDEX_32 == 1) && (VGT_INDEX_8 == 2),
    "Different VGT_INDEX_TYPE_MODE values than are expected!");

// Context reg space technically goes to 0xBFFF, but in reality there are no registers we currently write beyond
// a certain limit. This enum can save some memory space in situations where we shadow register state in the driver.
constexpr uint32 CntxRegUsedRangeEnd  =
                    Gfx09::mmCB_COLOR7_DCC_BASE_EXT;

constexpr uint32 CntxRegUsedRangeSize = (CntxRegUsedRangeEnd - CONTEXT_SPACE_START + 1);
constexpr uint32 CntxRegCount         = (CONTEXT_SPACE_END - CONTEXT_SPACE_START + 1);

// SH reg space technically goes to 0x2FFF, but in reality there are no registers we currently write beyond the
// COMPUTE_USER_DATA_15 register.  This enum can save some memory space in situations where we shadow register state
// in the driver.
constexpr uint32 ShRegUsedRangeEnd  = mmCOMPUTE_USER_DATA_15;
constexpr uint32 ShRegUsedRangeSize = (ShRegUsedRangeEnd - PERSISTENT_SPACE_START + 1);
constexpr uint32 ShRegCount         = (PERSISTENT_SPACE_END - PERSISTENT_SPACE_START + 1);

// Number of registers in config register space.
constexpr uint32 ConfigRegCount = (CONFIG_SPACE_END - CONFIG_SPACE_START + 1);

// Number of registers in user-config register space.
constexpr uint32 UserConfigRegCount = (UCONFIG_SPACE_END - UCONFIG_SPACE_START + 1);

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
    CONTEXT_SPACE_START,           CntxRegUsedRangeEnd,
    PERSISTENT_SPACE_START,        ShRegUsedRangeEnd,
    Gfx09::mmIA_MULTI_VGT_PARAM,   Gfx09::mmIA_MULTI_VGT_PARAM
    >;

// Number of SGPRs available to each wavefront.  Note that while only 104 SGPRs are available for use by a particular
// wave, each SIMD has 800 physical SGPRs so it can acommodate multiple wave even if the waves use the max available
// logical SGPRs.
// NOTE: Theoretically, we have 106 available SGPRs plus 2 for the VCC regs. However, the SPI_SHADER_PGM_RSRC1_*.SGPRS
// field is programmed in blocks of 8, making the this number ((106 + 2) & ~0x7), which is 104.
constexpr uint32 MaxSgprsAvailable = 104;

// Number of SGPRs physically present per SIMD
constexpr uint32 Gfx9PhysicalSgprsPerSimd = 800;

// Number of SIMDs per Compute Unit
constexpr uint32 Gfx9NumSimdPerCu = 4;

// The maximum number of waves per SIMD and Compute Unit.
constexpr uint32 Gfx9NumWavesPerSimd = 10;
constexpr uint32 Gfx9NumWavesPerCu   = Gfx9NumWavesPerSimd * Gfx9NumSimdPerCu;

// Number of slots(dwords) that one GPU event will need to support ReleaseAcquireInterface.
constexpr uint32 MaxSlotsPerEvent = 2;

// Enumerates the possible hardware stages which a shader can run as.  GFX9 combines several shader stages:
// 1) LS + HS have been combined into a HS stage.  Note that the registers that control this stage are still
//            called "LS", but HW considers this to be a HS stage in their docs.
// 2) ES + GS have been combined into a GS stage.  Note that the registers that control this stage are still
//            called "ES", but HW considers this to be a GS stage in their docs.
enum HwShaderStage : uint32
{
    Hs,
    Gs,
    Vs,
    Ps,
    Cs,
    Last,
};

// Number of user-data registers per shader stage on the chip. PAL reserves a number of these for internal use, making
// them unusable from the client. The registers PAL reserves are:
//
// [0]  - For the global internal resource table (shader rings, offchip LDS buffers, etc.)
// [1]  - For the constant buffer table for the shader(s).
// [29] - For the per-Draw vertex offset
// [30] - For the per-Draw instance offset
// [31] - For the ES/GS LDS size when on-chip GS is enabled
//
// This leaves registers 2-28 available for the client's use.  If user data remapping is enabled, data in slots 29-31
// will instead be packed at the end of the client's user data.
constexpr uint32 NumUserDataRegisters = 32;

// Starting user-data register index where the low 32 address bits of the global internal table pointer
// (shader ring SRD's, etc.) is written.
constexpr uint16 InternalTblStartReg  = 0;
// Starting user-data register indexes where the low 32 address bits of the constant buffer table pointer
// (internal CB's, link-constant buf's) for the shader(s) are written.
constexpr uint16 ConstBufTblStartReg = (InternalTblStartReg + 1);

// User-data register where some shaders' LDS size requirement (for on-chip GS support) is written.
constexpr uint32 EsGsLdsSizeReg     = (NumUserDataRegisters - 1);
// User data register where the API VS' per-Draw instance offset is written.
constexpr uint32 InstanceOffsetReg  = (EsGsLdsSizeReg - 1);
// User data register where the API VS' per-Draw vertex offset is written.
constexpr uint32 VertexOffsetReg    = (InstanceOffsetReg - 1);

// Copy shaders (VS stage of an GS/VS/PS pipeline), don't need client user data, and use a simple fixed layout:
//
// [0] - For the global internal resource table (shader rings, offchip LDS buffers, etc.)
// [1] - For the constant buffer table for the shader(s).
// [2] - For the ES/GS LDS size when on-chip GS is enabled.
// [3] - Streamout target table address.
constexpr uint32 EsGsLdsSizeRegCopyShader  = ConstBufTblStartReg + 1;
constexpr uint32 StreamOutTblRegCopyShader = EsGsLdsSizeRegCopyShader + 1;

// Compute still only has 16 user data registers.  Compute also uses a fixed user data layout, and does not support
// remapping.
//
// [0]     - For the global internal resource table (shader rings, offchip LDS buffers, etc.)
// [1]     - For the constant buffer table for the shader(s).
// [13]    - Spill table address.
// [14-15] - GPU address of memory holding the threadgroup dimensions of the dispatch.
//
// Slot [14-15] is only reserved for Vulkan as the corresponding feature isn't supported by other clients, and in that
// case the spill table address will be in slot [15].
constexpr uint32 NumUserDataRegistersCompute = 16;

// User-data register where compute shaders' GDS partition information is written.
constexpr uint32 GdsRangeRegCompute = (NumUserDataRegistersCompute - 1);

// First user data register where the API CS' per-Dispatch number of thread groups buffer GPU address is written.
constexpr uint32 NumThreadGroupsReg = (GdsRangeRegCompute - (sizeof(gpusize) / sizeof(uint32)));

// Unlike graphics pipelines, compute pipelines use a fixed mapping between user-data entries and physical user-data
// registers. This means that we need to always use the same register for the spill table GPU address, so reserve a
// register for that:
// NOTE: We special-case this for Vulkan and non-Vulkan clients because we don't want clients which cannot use the
// gl_numWorkGroups feature to permanently lose two user-data registers for compute.
constexpr uint32 CsSpillTableAddrReg = (NumThreadGroupsReg - 1);

// Starting user data register index where the client's graphics fast user-data 'entries' are written for shaders.
constexpr uint16 FastUserDataStartReg = (ConstBufTblStartReg + 1);

// Maximum number of fast user data 'entries' exposed to the client for vertex shaders.
constexpr uint32 MaxFastUserDataEntriesVs = (VertexOffsetReg - FastUserDataStartReg);
// Maximum number of fast user data 'entries' exposed to the client for pixel shaders.
constexpr uint32 MaxFastUserDataEntriesPs = (NumUserDataRegisters - FastUserDataStartReg);
// Maximum number of fast user data 'entries' exposed to the client for compute shader stage.
constexpr uint32 MaxFastUserDataEntriesCompute = (CsSpillTableAddrReg - FastUserDataStartReg);
// Maximum number of fast user data 'entries' exposed to the client for other shader stages.
constexpr uint32 MaxFastUserDataEntries = (EsGsLdsSizeReg - FastUserDataStartReg);

// Maximum number of fast user data 'entries' exposed to the client for each shader stage.
constexpr uint32 FastUserDataEntriesByStage[] =
{
   MaxFastUserDataEntriesCompute, // Compute
   MaxFastUserDataEntriesVs,      // Vertex
   MaxFastUserDataEntriesVs,      // Hull     - merged stage might require Api-Vs to use Vs-specific registers.
   MaxFastUserDataEntries,        // Domain
   MaxFastUserDataEntriesVs,      // Geometry - merged stage might require Api-Vs to use Vs-specific registers.
   MaxFastUserDataEntriesPs,      // Pixel
};

// HW doesn't provide enumerations for the values of the DB_DFSM_CONTROL.PUNCHOUT_MODE field.  Give
// some nice names here.
constexpr uint32 DfsmPunchoutModeEnable  = 0;
constexpr uint32 DfsmPunchoutModeDisable = 2;

// Number of PS input semantic registers.
constexpr uint32 MaxPsInputSemantics = 32;

// Number of VS export semantic registers.
constexpr uint32 MaxVsExportSemantics = 32;

// Mask of CP_ME_COHER_CNTL bits which stall based on all base addresses.
constexpr uint32 CpMeCoherCntlStallMask = CP_ME_COHER_CNTL__CB0_DEST_BASE_ENA_MASK |
                                          CP_ME_COHER_CNTL__CB1_DEST_BASE_ENA_MASK |
                                          CP_ME_COHER_CNTL__CB2_DEST_BASE_ENA_MASK |
                                          CP_ME_COHER_CNTL__CB3_DEST_BASE_ENA_MASK |
                                          CP_ME_COHER_CNTL__CB4_DEST_BASE_ENA_MASK |
                                          CP_ME_COHER_CNTL__CB5_DEST_BASE_ENA_MASK |
                                          CP_ME_COHER_CNTL__CB6_DEST_BASE_ENA_MASK |
                                          CP_ME_COHER_CNTL__CB7_DEST_BASE_ENA_MASK |
                                          CP_ME_COHER_CNTL__DB_DEST_BASE_ENA_MASK  |
                                          CP_ME_COHER_CNTL__DEST_BASE_0_ENA_MASK   |
                                          CP_ME_COHER_CNTL__DEST_BASE_1_ENA_MASK   |
                                          CP_ME_COHER_CNTL__DEST_BASE_2_ENA_MASK   |
                                          CP_ME_COHER_CNTL__DEST_BASE_3_ENA_MASK;

// Cacheline size.
constexpr uint32 CacheLineBytes  = 128;
constexpr uint32 CacheLineDwords = (CacheLineBytes / sizeof(uint32));

// Base GPU virtual address for full-range surface sync.
constexpr gpusize FullSyncBaseAddr = 0;

// Size for full-range surface sync.  This is 64-bits wide, which is much more than the number of bits actually
// available, but this value provides an easy way (+1 == 0) to determine that a full-sync is underway.
constexpr gpusize FullSyncSize = 0xFFFFFFFFFFFFFFFF;

// Number of Registers per CB slot.
constexpr uint32 CbRegsPerSlot = (mmCB_COLOR1_BASE - mmCB_COLOR0_BASE);

// Number of Registers for MSAA sample locations per 2x2 Quad.
constexpr uint32 NumSampleQuadRegs = 4;

// Gfx9 interpretation of the LDS_SIZE register field: the granularity of the value in DWORDs and the amount of bits
// to shift.
constexpr uint32 Gfx9LdsDwGranularity      = 128;
constexpr uint32 Gfx9LdsDwGranularityShift = 7;

// The maximum number of waves per SH.
constexpr uint32 Gfx9MaxWavesPerShCompute = (COMPUTE_RESOURCE_LIMITS__WAVES_PER_SH_MASK >>
                                             COMPUTE_RESOURCE_LIMITS__WAVES_PER_SH__SHIFT);
constexpr uint32 MaxGsThreadsPerSubgroup = 256;

// The value of ONCHIP that is the field of register VGT_GS_MODE
constexpr uint32 VgtGsModeOnchip = 3;

// Memory alignment requirement in bytes for shader and immediate constant buffer memory.
static constexpr gpusize PrimeUtcL2MemAlignment = 4096;
static constexpr gpusize CpDmaMemAlignment      = 256;

// Tile size is fixed at 64kb on all hardware
constexpr uint32 PrtTileSize = (64 * 1024);

// GFX9 supports the following PRT features:
constexpr PrtFeatureFlags Gfx9PrtFeatures = static_cast<PrtFeatureFlags>(
    PrtFeatureBuffer            | // - sparse buffers
    PrtFeatureImage2D           | // - sparse 2D images
    PrtFeatureImage3D           | // - sparse 3D images
    PrtFeatureShaderStatus      | // - residency status in shader instructions
    PrtFeatureShaderLodClamp    | // - LOD clamping in shader instructions
    PrtFeatureUnalignedMipSize  | // - unaligned levels outside of the miptail
    PrtFeaturePerSliceMipTail   | // - per-slice miptail (slice-major ordering)
    PrtFeatureTileAliasing      | // - tile aliasing (without metadata)
    PrtFeatureStrictNull);        // - returning zeros for unmapped tiles

// This enum defines the Shader types supported in PM4 type 3 header
enum Pm4ShaderType : uint32
{
    ShaderGraphics  = 0,
    ShaderCompute   = 1
};

// This enum defines the predicate value supported in PM4 type 3 header
enum Pm4Predicate : uint32
{
    PredDisable = 0,
    PredEnable  = 1
};

// HW enum for index stride (it is missing from gfx9_enum.h).
enum BUF_INDEX_STRIDE : uint32
{
    BUF_INDEX_STRIDE_8B  = 0,
    BUF_INDEX_STRIDE_16B = 1,
    BUF_INDEX_STRIDE_32B = 2,
    BUF_INDEX_STRIDE_64B = 3,
};

// GFX9-specific buffer resource descriptor structure
struct Gfx9BufferSrd
{
    SQ_BUF_RSRC_WORD0 word0;
    SQ_BUF_RSRC_WORD1 word1;
    SQ_BUF_RSRC_WORD2 word2;
    SQ_BUF_RSRC_WORD3 word3;
};

// Buffer resource descriptor structure
union BufferSrd
{
    Gfx9BufferSrd  gfx9;
};

// GFX9-specific image resource descriptor structure
struct Gfx9ImageSrd
{
    SQ_IMG_RSRC_WORD0 word0;
    SQ_IMG_RSRC_WORD1 word1;
    SQ_IMG_RSRC_WORD2 word2;
    SQ_IMG_RSRC_WORD3 word3;
    SQ_IMG_RSRC_WORD4 word4;
    SQ_IMG_RSRC_WORD5 word5;
    SQ_IMG_RSRC_WORD6 word6;
    SQ_IMG_RSRC_WORD7 word7;
};

// Image resource descriptor structure
union ImageSrd
{
    Gfx9ImageSrd   gfx9;
};

// GFX9-specific image sampler descriptor structure
struct Gfx9SamplerSrd
{
    SQ_IMG_SAMP_WORD0 word0;
    SQ_IMG_SAMP_WORD1 word1;
    SQ_IMG_SAMP_WORD2 word2;
    SQ_IMG_SAMP_WORD3 word3;
};

// Image sampler descriptor structure
union SamplerSrd
{
    Gfx9SamplerSrd gfx9;
};

// Maximum scissor rect value for the top-left corner.
constexpr uint32 ScissorMaxTL = 16383;
// Maximum scissor rect value for the bottom-right corner.
constexpr uint32 ScissorMaxBR = 16384;
// Minimal size for screen scissors
constexpr uint32 PaScScreenScissorMin = 0;
// Maximum size for screen scissors
constexpr uint32 PaScScreenScissorMax = 16384;

// Horizontal min screen extent.
constexpr int32 MinHorzScreenCoord = -32768;
// Horizontal max screen extent.
constexpr int32 MaxHorzScreenCoord =  32768;
// Vertical min screen extent.
constexpr int32 MinVertScreenCoord = -32768;
// Vertical max screen extent.
constexpr int32 MaxVertScreenCoord =  32768;

// Maximum image width
constexpr uint32 MaxImageWidth = 16384;
// Maximum image height
constexpr uint32 MaxImageHeight = 16384;
// Maximum image depth
constexpr uint32 MaxImageDepth = 8192;
// Maximum image mip levels. This was calculated from MaxImageWidth and MaxImageHeight
constexpr uint32 MaxImageMipLevels = 15;
// Maximum image array slices for GFX9 GPUs
constexpr uint32 Gfx9MaxImageArraySlices = 2048;

static_assert ((1 << (MaxImageMipLevels - 1)) == MaxImageWidth,
               "Max image dimensions don't match max mip levels!");

// No current ASICs have more than 16 active RBs.
constexpr uint32 MaxNumRbs = 16;

// Occlusion query data has to be 16 bytes aligned for CP access
static constexpr gpusize OcclusionQueryMemoryAlignment = 16;

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

// Number of valid hardware shader stages used in graphics pipelines.
constexpr uint32 NumHwShaderStagesGfx = (static_cast<uint32>(HwShaderStage::Ps) + 1);

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

    // First user-data entry (+1) containing the GPU virtual address of the performance data buffer used for
    // shader-specific performance profiling. Zero indicates that the shader does not use this buffer.
    uint16  perfDataAddr;

};

// User-data signature for an unbound compute pipeline.
extern const ComputePipelineSignature NullCsSignature;

// This represents the mapping from virtualized user-data entries to physical SPI user-data registers for an entire
// graphics pipeline.
struct GraphicsPipelineSignature
{
    // User-data entry mapping for each graphics HW shader stage: (HS, GS, VS, PS)
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
    // Register address for the start index offset of a draw for an NGG Fast Launch VS.
    uint16  startIndexRegAddr;
    // Register address for the Log2(sizeof(indexType)) of a draw for an NGG Fast Launch VS.
    uint16  log2IndexSizeRegAddr;

    // Register address for the IMM_LDS_ESGS_SIZE parameter for the GS stage.
    uint16  esGsLdsSizeRegAddrGs;
    // Register address for the IMM_LDS_ESGS_SIZE parameter for the VS (copy shader) stage.
    uint16  esGsLdsSizeRegAddrVs;

    // First user-data entry which is spilled to GPU memory. A value of 'NoUserDataSpilling' indicates the pipeline
    // does not spill user-data entries to memory.
    uint16  spillThreshold;

    // The number of 'important' user-data entries for this pipeline. This effectively equates to one plus the index
    // of the highest user-data entry accessed by the pipeline.
    uint16  userDataLimit;

    // Address of each shader stage's user-SGPR for view ID.  This is a compacted list, so it is not safe to assume
    // that each index of this array corresponds to the associated HW shader stage enum value.
    uint16  viewIdRegAddr[NumHwShaderStagesGfx];

    // First user-data entry (+1) containing the GPU virtual address of the performance data buffer used for
    // shader-specific performance profiling. Zero indicates that the shader does not use this buffer.
    uint16  perfDataAddr[NumHwShaderStagesGfx];

    // Hash of each stages user-data mapping, used to speed up pipeline binds.
    uint64  userDataHash[NumHwShaderStagesGfx];
};

// User-data signature for an unbound graphics pipeline.
extern const GraphicsPipelineSignature NullGfxSignature;

// Special value indicating that a pipeline or shader does not need its user-data entries to be spilled.
constexpr uint16 NoUserDataSpilling = static_cast<uint16>(0xFFFF);

// Enumerates the valid texture perf modulation values.
enum class TexPerfModulation : uint32
{
    None    = 0,
    Min     = 1,
    Default = 4,
    Max     = 7,
};

// Dword offset of mmXDMA_SLV_FLIP_PENDING.
constexpr uint32  mmXdmaSlvFlipPending = 0x348C;

} // Gfx9
} // Pal
