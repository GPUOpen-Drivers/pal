/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palMetroHash.h"
#include "palPipelineAbi.h"
#include "palSparseVector.h"
#include "palLiterals.h"

#include "core/hw/gfxip/gfx9/chip/gfx9_plus_merged_offset.h"
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

#include "core/hw/gfxip/gfx9/chip/gfx10_sq_ko_reg.h"

namespace Pal
{
class Platform;

namespace Gfx9
{

// Helper struct for the 2nd dword of a DUMP_CONST_RAM_* CE packet.
union DumpConstRamOrdinal2
{
    decltype(PM4_CE_DUMP_CONST_RAM::ordinal2.bitfields)  bits;
    uint32                                               u32All;
};

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
constexpr uint32 CntxRegUsedRangeEnd  = Gfx10Plus::mmCB_COLOR7_ATTRIB3;

constexpr uint32 CntxRegUsedRangeSize = (CntxRegUsedRangeEnd - CONTEXT_SPACE_START + 1);
constexpr uint32 CntxRegCount         = (Gfx09_10::CONTEXT_SPACE_END - CONTEXT_SPACE_START + 1);

// SH reg space technically goes to 0x2FFF, but in reality there are no registers we currently write beyond the
// COMPUTE_USER_DATA_15 register.  This enum can save some memory space in situations where we shadow register state
// in the driver.
constexpr uint32 ShRegUsedRangeEnd  = Gfx10Plus::mmCOMPUTE_DISPATCH_TUNNEL;
constexpr uint32 ShRegUsedRangeSize = (ShRegUsedRangeEnd - PERSISTENT_SPACE_START + 1);
constexpr uint32 ShRegCount         = (PERSISTENT_SPACE_END - PERSISTENT_SPACE_START + 1);

// Number of registers in config register space.
constexpr uint32 ConfigRegCount = (CONFIG_SPACE_END - CONFIG_SPACE_START + 1);

// Number of registers in user-config register space.
constexpr uint32 UserConfigRegCount = (UCONFIG_SPACE_END - UCONFIG_SPACE_START + 1);

// The PERFDDEC and PERFSDEC register spaces are contiguous and hold all perfcounter related user-config registers.
// These constants aren't in the regspec so we must manually define them.
constexpr uint32 UserConfigRegPerfStart = 0xD000;
constexpr uint32 UserConfigRegPerfEnd   = 0xDFFF;

// Container used for storing registers during pipeline load.
using RegisterVector = Util::SparseVector<
    uint32,
    uint8,
    50,
    Platform,
    CONTEXT_SPACE_START,           CntxRegUsedRangeEnd,
    PERSISTENT_SPACE_START,        ShRegUsedRangeEnd,
    Gfx09::mmIA_MULTI_VGT_PARAM,   Gfx09::mmIA_MULTI_VGT_PARAM,
    Gfx10Plus::mmGE_STEREO_CNTL,   Gfx10Plus::mmGE_STEREO_CNTL,
    Gfx10Plus::mmGE_USER_VGPR_EN,  Gfx10Plus::mmGE_USER_VGPR_EN
#if PAL_BUILD_GFX11
    , Gfx11::mmVGT_GS_OUT_PRIM_TYPE, Gfx11::mmVGT_GS_OUT_PRIM_TYPE
#endif
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

// Number of SIMDs per Compute Unit
constexpr uint32 Gfx10NumSimdPerCu = 2;

// Number of SGPRs per wave
constexpr uint32 Gfx10NumSgprsPerWave = 128;

// The hardware can only support a limited number of scratch waves per CU.
constexpr uint32 MaxScratchWavesPerCu = 32;

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

constexpr Util::Abi::HardwareStage PalToAbiHwShaderStage[] =
{
    Util::Abi::HardwareStage::Hs,
    Util::Abi::HardwareStage::Gs,
    Util::Abi::HardwareStage::Vs,
    Util::Abi::HardwareStage::Ps,
    Util::Abi::HardwareStage::Cs,
};
static_assert(Util::ArrayLen(PalToAbiHwShaderStage) == uint32(HwShaderStage::Last),
              "Translation table is not sized properly!");

#if PAL_BUILD_GFX11
// Maximum number of registers that may be written with packed register pairs.
constexpr uint32 Gfx11RegPairMaxRegCount = 128;
// Maximum number of packed register pairs that may be written in a single packed register pair packet.
constexpr uint32 Gfx11MaxRegPairCount    = Gfx11RegPairMaxRegCount / 2;
// Number of graphics stages supported by packed register pairs (HS, GS, and PS).
constexpr uint32 Gfx11NumRegPairSupportedStagesGfx = 3;
// Number of compute stages supported by packed register pairs.
constexpr uint32 Gfx11NumRegPairSupportedStagesCs  = 1;
#endif

// Number of user-data registers per shader stage on the chip. PAL reserves a number of these for internal use, making
// them unusable from the client. The registers PAL reserves are:
//
// [0]  - For the global internal resource table (shader rings, offchip LDS buffers, etc.)
// [1]  - For the constant buffer table for the shader(s).
//
// This leaves registers 2-31 available for the client's use.
constexpr uint32 NumUserDataRegisters = 32;

// Starting user-data register index where the low 32 address bits of the global internal table pointer
// (shader ring SRDs, etc.) is written.
constexpr uint16 InternalTblStartReg  = 0;
// Starting user-data register indexes where the low 32 address bits of the constant buffer table pointer
// (internal CBs) for the shader(s) are written.
constexpr uint16 ConstBufTblStartReg = (InternalTblStartReg + 1);

#if PAL_BUILD_GFX11
// Maximum number of user-data entries that can be packed into packed register pairs for all supported graphics stages.
constexpr uint32 Gfx11NumUserDataGfx             = Gfx11NumRegPairSupportedStagesGfx * NumUserDataRegisters;
constexpr uint32 Gfx11MaxUserDataIndexCountGfx   = Gfx11NumUserDataGfx;
// Maximum number of graphics user-data entry packed register pairs.
constexpr uint32 Gfx11MaxPackedUserEntryCountGfx = Gfx11NumUserDataGfx / 2;
static_assert(Gfx11MaxPackedUserEntryCountGfx <= Gfx11MaxRegPairCount, "Packing too many registers!");
#endif

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

#if PAL_BUILD_GFX11
// Maximum number of user-data entries that can be packed into packed register pairs for the compute stage.
constexpr uint32 Gfx11MaxUserDataIndexCountCs   = NumUserDataRegistersCompute;
// Maximum number of compute user-data entry packed register pairs.
constexpr uint32 Gfx11MaxPackedUserEntryCountCs = NumUserDataRegistersCompute / 2;

static_assert(Gfx11MaxPackedUserEntryCountCs <= Gfx11MaxRegPairCount, "Packing too many registers!");
#endif

// HW doesn't provide enumerations for the values of the DB_DFSM_CONTROL.PUNCHOUT_MODE field.  Give
// some nice names here.
constexpr uint32 DfsmPunchoutModeAuto     = 0;
constexpr uint32 DfsmPunchoutModeForceOn  = 1;
constexpr uint32 DfsmPunchoutModeForceOff = 2;

// Number of PS input semantic registers.
constexpr uint32 MaxPsInputSemantics = 32;

// Number of VS export semantic registers.
constexpr uint32 MaxVsExportSemantics = 32;

// Cacheline size.
constexpr uint32 CacheLineBytes  = 128;
constexpr uint32 CacheLineDwords = (CacheLineBytes / sizeof(uint32));

// Number of Registers per CB slot.
constexpr uint32 CbRegsPerSlot = (mmCB_COLOR1_BASE - mmCB_COLOR0_BASE);

// Number of Registers for MSAA sample locations per 2x2 Quad.
constexpr uint32 NumSampleQuadRegs = 4;

// Gfx9 interpretation of the LDS_SIZE register field: the granularity of the value in DWORDs and the amount of bits
// to shift.
constexpr uint32 Gfx9LdsDwGranularity             = 128;
constexpr uint32 Gfx9PsExtraLdsDwGranularity      = 128;
constexpr uint32 Gfx9LdsDwGranularityShift        = 7;
constexpr uint32 Gfx9PsExtraLdsDwGranularityShift = 7;

// The WAVE_LIMIT register setting for graphics hardware stages is defined in units of this many waves per SH.
constexpr uint32 Gfx9MaxWavesPerShGraphicsUnitSize = 16;

// The maximum number of waves per SH.
constexpr uint32 Gfx9MaxWavesPerShCompute = (COMPUTE_RESOURCE_LIMITS__WAVES_PER_SH_MASK >>
                                             COMPUTE_RESOURCE_LIMITS__WAVES_PER_SH__SHIFT);
constexpr uint32 MaxGsThreadsPerSubgroup = 256;

// The value of ONCHIP that is the field of register VGT_GS_MODE
constexpr uint32 VgtGsModeOnchip = 3;

enum class GsFastLaunchMode : uint32
{
    Disabled   = 0,
    VertInLane = 1, // - Emulates threadgroups where each subgroup has 1 vert/prim and we use the primitive
                    //   amplification factor to "grow" the subgroup up to the threadgroup sizes required by the
                    //   shader.
#if PAL_BUILD_GFX11
    PrimInLane = 2  // - Uses X, Y, and Z dimensions programmed into registers to appropriately size the subgroup
                    //   explicitely.
#endif
};

// Memory alignment requirement in bytes for shader and immediate constant buffer memory.
static constexpr gpusize PrimeUtcL2MemAlignment = 4096;
static constexpr gpusize CpDmaMemAlignment      = 256;

// Highest index of the SET_BASE packet
constexpr uint32 MaxSetBaseIndex = base_index__pfp_set_base__indirect_data_base;

// Tile size is fixed on all hardware
constexpr uint32 PrtTileSize = 64 * Util::OneKibibyte;

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

constexpr PrtFeatureFlags Gfx102PlusPrtFeatures = static_cast<PrtFeatureFlags>(
    static_cast<uint32>(Gfx9PrtFeatures) |
    PrtFeaturePrtPlus);

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
    sq_buf_rsrc_t  gfx10;
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
    sq_img_rsrc_t  gfx10;
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
    sq_img_samp_t  gfx10;
};

static_assert((sizeof(Gfx9BufferSrd) == sizeof(sq_buf_rsrc_t)),
              "GFX9 and GFX10 buffer SRD definitions are not the same size!");
static_assert((sizeof(Gfx9ImageSrd) == sizeof(sq_img_rsrc_t)),
              "GFX9 and GFX10 image SRD definitions are not the same size!");
static_assert((sizeof(Gfx9SamplerSrd) == sizeof(sq_img_samp_t)),
              "GFX9 and GFX10 sampler SRD definitions are not the same size!");

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
// Maximum image array slices for GFX10 GPUs
constexpr uint32 Gfx10MaxImageArraySlices = 8192;

static_assert ((1 << (MaxImageMipLevels - 1)) == MaxImageWidth,
               "Max image dimensions don't match max mip levels!");

#if PAL_BUILD_GFX11
// GFX11 increases the max possible number of RB's to 24; round up to give us some wiggle room.
constexpr uint32 MaxNumRbs = 32;
#else
// No current ASICs have more than 16 active RBs.
constexpr uint32 MaxNumRbs = 16;
#endif

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
    uint8  mappedEntry[NumUserDataRegisters];
    uint8  userSgprCount;           // Number of valid entries in the mappedEntry array.
    uint16 firstUserSgprRegAddr;    // Address of the first user-SGPR which is mapped to user-data entries.
    // Address of the user-SGPR used for the spill table GPU virtual address for this stage.  Zero indicates that this
    // stage does not read any entries from the spill table.
    uint16 spillTableRegAddr;
};

// Special value indicating that a user-data entry is not mapped to a physical SPI register.
constexpr uint16 UserDataNotMapped = 0;

// This represents the mapping from virtualized user-data entries to physical SPI user-data registers for a
// compute shader.
struct ComputeShaderSignature
{
    // User-data entry mapping for the lone compute HW shader stage: (CS)
    UserDataEntryMap  stage;

    // Register address for the GPU virtual address pointing to the internal constant buffer containing the number
    // of thread groups launched in a Dispatch operation. Two sequential SPI user-data registers are needed to store
    // the address, this is the address of the first one.
    uint16  numWorkGroupsRegAddr;

    // Register address for the first user-data entry (+2) of the Task Shader threadgroup dimension values.
    uint16  taskDispatchDimsAddr;

    // Register address for the ring index for the task shader.
    uint16  taskRingIndexAddr;

    // Register address for the dispatch index of a multi-dispatch indirect task shader dispatch.
    uint16  dispatchIndexRegAddr;

    // Register address for passing the 32-bit GPU virtual address of a buffer storing the shader-emulated task+mesh
    // pipeline stats query.
    uint16  taskPipeStatsBufRegAddr;

    // First user-data entry which is spilled to GPU memory. A value of 'NoUserDataSpilling' indicates the pipeline
    // does not spill user-data entries to memory.
    uint16  spillThreshold;

    // The number of 'important' user-data entries for this pipeline. This effectively equates to one plus the index
    // of the highest user-data entry accessed by the pipeline.
    uint16  userDataLimit;

    // First user-data entry (+1) containing the GPU virtual address of the performance data buffer used for
    // shader-specific performance profiling. Zero indicates that the shader does not use this buffer.
    uint16  perfDataAddr;

    // Hash of CS stage user-data mapping, used to speed up pipeline binds.
    uint64  userDataHash;

    union
    {
        struct
        {
            uint16  isWave32 :  1;   // Is the pipeline running in Wave-32 mode?
            uint16  isLinear :  1;   // Is the taskMesh pipeline using linear dispatch for mesh packets?
            uint16  reserved : 14;
        };
        uint16  u16All;
    } flags;
};

// As a ComputePipeline contains only a compute shader, make these two equivalent.
typedef ComputeShaderSignature ComputePipelineSignature;

// User-data signature for an unbound compute pipeline.
extern const ComputePipelineSignature NullCsSignature;

// This represents the mapping from virtualized user-data entries to physical SPI user-data registers for an entire
// graphics pipeline.
struct GraphicsPipelineSignature
{
    // User-data entry mapping for each graphics HW shader stage: (HS, GS, VS, PS)
    UserDataEntryMap  stage[NumHwShaderStagesGfx];

    // Register address for the GPU virtual address of the vertex buffer table used by this pipeline. Zero indicates
    // that the vertex buffer table is not accessed.
    uint16  vertexBufTableRegAddr;
    // Register address for the GPU virtual address of the stream-output table used by this pipeline. Zero indicates
    // that stream-output is not used by this pipeline.
    uint16  streamOutTableRegAddr;
#if PAL_BUILD_GFX11
    // Register address for the GPU virtual address of the stream-output control buffer used by this pipeline. Zero
    // indicates that stream-output is not used by this pipeline.
    uint16  streamoutCntlBufRegAddr;
#endif
    // Register address for the GPU virtual address of the uav-export SRD table used by this pipeline. Zero indicates
    // that UAV export is not used by this pipeline.
    uint16  uavExportTableAddr;
    // Register address for the GPU virtual address of the NGG culling data constant buffer used by this pipeline.
    // Zero indicates that the NGG culling constant buffer is not used by the pipeline.
    uint16  nggCullingDataAddr;

    // Register address for the vertex ID offset of a draw. The instance ID offset is always the very next register.
    uint16  vertexOffsetRegAddr;
    // Register address for the draw index of a multi-draw. This is an optional feature of each pipeline, so it may
    // be unmapped.
    uint16  drawIndexRegAddr;
    // Register address for the X/Y/Z dimensions of a mesh shader dispatch.  Three sequential SPI user-data registers
    // are needed to store these data.
    uint16  meshDispatchDimsRegAddr;
    // Register address for the ring index for the mesh shader.
    uint16  meshRingIndexAddr;
    // Register address for passing the 32-bit GPU virtual address of a buffer storing the shader-emulated mesh
    // pipeline stats query.
    uint16  meshPipeStatsBufRegAddr;

    // Register address for dynamic numSamples and samplePatternIdx
    uint16  sampleInfoRegAddr;

    // Register address for passing the 32-bit GPU virtual address of the color export shader entry
    uint16  colorExportAddr;

    // Register address for dynamicDualSourceBlend
    uint16  dualSourceBlendInfoRegAddr;

    // First user-data entry which is spilled to GPU memory. A value of 'NoUserDataSpilling' indicates the pipeline
    // does not spill user-data entries to memory.
    uint16  spillThreshold;

    // The number of 'important' user-data entries for this pipeline. This effectively equates to one plus the index
    // of the highest user-data entry accessed by the pipeline.
    uint16  userDataLimit;

    // Register address for passing 32-bit flag which controls the outputting of generated primitives counts.
    uint16  primsNeededCntAddr;

    // Address of each shader stage's user-SGPR for view ID.  This is a compacted list, so it is not safe to assume
    // that each index of this array corresponds to the associated HW shader stage enum value.
    uint16  viewIdRegAddr[NumHwShaderStagesGfx];

    // Hash of each stages user-data mapping, used to speed up pipeline binds.
    uint64  userDataHash[NumHwShaderStagesGfx];
};

// =====================================================================================================================
// Compute a hash of the user data mapping
static uint64 ComputeUserDataHash(
    const UserDataEntryMap* pStage)
{
    uint64 hash = 0;
    Util::MetroHash64::Hash(
        reinterpret_cast<const uint8*>(pStage),
        sizeof(UserDataEntryMap),
        reinterpret_cast<uint8* const>(&hash));
    return hash;
}

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

// This flag in COMPUTE_DISPATCH_INITIATOR tells the CP to not preempt mid-dispatch when CWSR is disabled.
constexpr uint32 ComputeDispatchInitiatorDisablePartialPreemptMask = (1 << 17);

#if PAL_BUILD_GFX11
// Memory alignment and size requires for the Vertex Attribute Ring
constexpr uint32 Gfx11VertexAttributeRingAlignmentBytes     = (64 * Util::OneKibibyte);
constexpr uint32 Gfx11VertexAttributeRingMaxSizeBytes       = (16 * Util::OneMebibyte);
constexpr uint32 Gfx11PsExtraLdsDwGranularity               = 256;
constexpr uint32 Gfx11PsExtraLdsDwGranularityShift          = 8;
#endif

#if PAL_BUILD_GFX11
// Maximum number of PWS-enabled pipeline events that PWS+ supported engine (currently universal engine) can track.
constexpr uint32 MaxNumPwsSyncEvents = 64;
#endif

constexpr uint32 Gfx103UcodeVersionLoadShRegIndexIndirectAddr = 39;

// Abstract cache sync flags modeled after the hardware GCR flags.The "Glx" flags apply to the GL2, GL1, and L0 caches
// which are accessable from both graphics and compute engines.
enum SyncGlxFlags : uint8
{
    SyncGlxNone = 0x00,
    // Global caches.
    SyncGl2Inv  = 0x01, // Invalidate the GL2 cache.
    SyncGl2Wb   = 0x02, // Flush the GL2 cache.
    SyncGlmInv  = 0x04, // Invalidate the GL2 metadata cache.
    SyncGl1Inv  = 0x08, // Invalidate the GL1 cache, ignored on gfx9.
    // Shader L0 caches.
    SyncGlvInv  = 0x10, // Invalidate the L0 vector cache.
    SyncGlkInv  = 0x20, // Invalidate the L0 scalar cache.
    SyncGlkWb   = 0x40, // Flush the L0 scalar cache.
    SyncGliInv  = 0x80, // Invalidate the L0 instruction cache.

    // A helper enum which combines a GL2 flush and invalidate. Note that an equivalent for glk was not implemented
    // because it should be extremely rare for PAL to flush the glk and we don't want people to do it accidentally.
    SyncGl2WbInv = SyncGl2Wb | SyncGl2Inv,

    // Flush and invalidate all Glx caches.
    SyncGlxWbInvAll = 0xFF,
};

// We want flag combinations to retain the SyncGlxFlags type so it's harder to confuse them with SyncRbFlags.
// This is only really practical if we define some operator overloads to hide all of the casting.
constexpr SyncGlxFlags operator|(SyncGlxFlags lhs, SyncGlxFlags rhs) { return SyncGlxFlags(uint8(lhs) | uint8(rhs)); }
constexpr SyncGlxFlags operator&(SyncGlxFlags lhs, SyncGlxFlags rhs) { return SyncGlxFlags(uint8(lhs) & uint8(rhs)); }
constexpr SyncGlxFlags operator~(SyncGlxFlags val) { return SyncGlxFlags(~uint8(val)); }

constexpr SyncGlxFlags& operator|=(SyncGlxFlags& lhs, SyncGlxFlags rhs) { lhs = lhs | rhs;  return lhs; }
constexpr SyncGlxFlags& operator&=(SyncGlxFlags& lhs, SyncGlxFlags rhs) { lhs = lhs & rhs;  return lhs; }

// The same idea as the "Glx" flags but these describle the graphics render backend L0 caches.
enum SyncRbFlags : uint8
{
    SyncRbNone      = 0x00,
    SyncCbDataInv   = 0x01, // Invalidate the CB data cache (color data and DCC keys).
    SyncCbDataWb    = 0x02, // Flush the CB data cache (color data and DCC keys).
    SyncCbMetaInv   = 0x04, // Invalidate the CB metadata cache (CMask and FMask).
    SyncCbMetaWb    = 0x08, // Flush the CB metadata cache (CMask and FMask).
    SyncDbDataInv   = 0x10, // Invalidate the DB data cache (depth data and stencil data).
    SyncDbDataWb    = 0x20, // Flush the DB data cache (depth data and stencil data).
    SyncDbMetaInv   = 0x40, // Invalidate the DB metadata cache (HTile).
    SyncDbMetaWb    = 0x80, // Flush the DB metadata cache (HTile).

    // Some helper for the CB, DB, and both together (RB)
    SyncCbDataWbInv = SyncCbDataWb    | SyncCbDataInv,
    SyncCbMetaWbInv = SyncCbMetaWb    | SyncCbMetaInv,
    SyncCbWbInv     = SyncCbDataWbInv | SyncCbMetaWbInv,

    SyncDbDataWbInv = SyncDbDataWb    | SyncDbDataInv,
    SyncDbMetaWbInv = SyncDbMetaWb    | SyncDbMetaInv,
    SyncDbWbInv     = SyncDbDataWbInv | SyncDbMetaWbInv,

    SyncRbInv       = SyncCbDataInv | SyncCbMetaInv | SyncDbDataInv | SyncDbMetaInv,
    SyncRbWb        = SyncCbDataWb  | SyncCbMetaWb  | SyncDbDataWb  | SyncDbMetaWb,
    SyncRbWbInv     = 0xFF
};

// We want flag combinations to retain the SyncRbFlags type so it's harder to confuse them with SyncGlxFlags.
// This is only really practical if we define some operator overloads to hide all of the casting.
constexpr SyncRbFlags operator|(SyncRbFlags lhs, SyncRbFlags rhs) { return SyncRbFlags(uint8(lhs) | uint8(rhs)); }
constexpr SyncRbFlags operator&(SyncRbFlags lhs, SyncRbFlags rhs) { return SyncRbFlags(uint8(lhs) & uint8(rhs)); }
constexpr SyncRbFlags operator~(SyncRbFlags val) { return SyncRbFlags(~uint8(val)); }

constexpr SyncRbFlags& operator|=(SyncRbFlags& lhs, SyncRbFlags rhs) { lhs = lhs | rhs;  return lhs; }
constexpr SyncRbFlags& operator&=(SyncRbFlags& lhs, SyncRbFlags rhs) { lhs = lhs & rhs;  return lhs; }

} // Gfx9
} // Pal
