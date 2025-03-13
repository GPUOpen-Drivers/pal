/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palDevice.h"
#include "palLiterals.h"
#include "palPipelineAbi.h"
#include "core/hw/gfxip/gfx12/chip/gfx12_merged_offset.h"
#include "core/hw/gfxip/gfx12/chip/gfx12_merged_default.h"
#include "core/hw/gfxip/gfx12/chip/gfx12_merged_enum.h"
#include "core/hw/gfxip/gfx12/chip/gfx12_merged_mask.h"
#include "core/hw/gfxip/gfx12/chip/gfx12_merged_shift.h"
#include "core/hw/gfxip/gfx12/chip/gfx12_sq_ko_reg.h"
#include "core/hw/gfxip/gfx12/chip/gfx12_merged_registers.h"
#include "core/hw/gfxip/gfx12/chip/gfx12_merged_pm4_it_opcodes.h"
#include "core/hw/gfxip/gfx12/chip/gfx12_merged_f32_mec_pm4_packets.h"
#include "core/hw/gfxip/gfx12/chip/gfx12_merged_f32_me_pm4_packets.h"
#include "core/hw/gfxip/gfx12/chip/gfx12_merged_f32_pfp_pm4_packets.h"
#include "core/hw/gfxip/gfx12/chip/gfx12_merged_typedef.h"

namespace Pal
{
namespace Gfx12
{

constexpr uint32 UConfigRangeEnd  = 0xC7FF;
constexpr uint32 UConfigPerfStart = 0xD000;
constexpr uint32 UconfigPerfEnd   = 0xDFFF;

static_assert(sizeof(sq_buf_rsrc_t) == 4 * sizeof(uint32), "Buffer SRD is not expected size!");
static_assert(sizeof(sq_img_rsrc_t) == 8 * sizeof(uint32), "Image SRD is not expected size!");
static_assert(sizeof(sq_img_samp_t) == 4 * sizeof(uint32), "Sampler SRD is not expected size!");
static_assert(sizeof(sq_bvh_rsrc_t) == 4 * sizeof(uint32), "BVH SRD is not expected size!");

constexpr sq_buf_rsrc_t NullBufferView =
{
    .u32All =
        {
            0u,
            0u,
            0u,
            0u | (SQ_RSRC_BUF << SqBufRsrcTWord3TypeShift),
        },
};
constexpr sq_img_rsrc_t NullImageView  =
{
    .u32All =
        {
            0u,
            0u,
            0u,
            0u | (SQ_RSRC_IMG_2D_ARRAY << SqImgRsrcTWord3TypeShift),
            0u,
            0u,
            0u,
        },
};
constexpr sq_img_samp_t NullSampler    = {};

// HW enum for index stride (it is missing from gfx12_enum.h).
enum BUF_INDEX_STRIDE : uint32
{
    BUF_INDEX_STRIDE_8B  = 0,
    BUF_INDEX_STRIDE_16B = 1,
    BUF_INDEX_STRIDE_32B = 2,
    BUF_INDEX_STRIDE_64B = 3,
};

// Enumerates the valid texture perf modulation values.
enum class TexPerfModulation : uint32
{
    None    = 0,
    Min     = 1,
    Default = 4,
    Max     = 7,
};

// RB Compression Mode settings.
enum class RbCompressionMode : uint32
{
    Default = 0,
    Bypass = 1,
    CompressWriteDisable = 2,
    ReadBypassWriteDisable = 3,
};

// Size of buffer descriptor structure.
constexpr uint32 DwordsPerBufferSrd = Util::NumBytesToNumDwords(sizeof(sq_buf_rsrc_t));

// Number of SGPRs available to each wavefront (see GFX12 Shader Programming Guide)
constexpr uint32 MaxSgprsAvailable = 106;

// Maximum scissor size.
constexpr uint32 MaxScissorSize = 32768;

// Maximum image width
constexpr uint32 MaxImageWidth  = 32768;
// Maximum image height
constexpr uint32 MaxImageHeight = 32768;
// Maximum image depth
constexpr uint32 MaxImageDepth  = 16384;
// Maximum image mip levels. This was calculated from MaxImageWidth and MaxImageHeight
constexpr uint32 MaxImageMipLevels = 16;

static_assert ((1 << (MaxImageMipLevels - 1)) == MaxImageWidth,
    "Max image dimensions don't match max mip levels!");

// Horizontal min screen extent.
constexpr int32 MinHorzScreenCoord = -32768;
// Horizontal max screen extent.
constexpr int32 MaxHorzScreenCoord = 32768;
// Vertical min screen extent.
constexpr int32 MinVertScreenCoord = -32768;
// Vertical max screen extent.
constexpr int32 MaxVertScreenCoord = 32768;

// Maximum image array slices
constexpr uint32 MaxImageArraySlices = 8192;

// The Max rectangle number that is allowed for clip rects.
constexpr uint32 MaxClipRects = 0x00000004;

// Gfx12 interpretation of the LDS_SIZE register field: the granularity of the value in DWORDs and the amount of bits
// to shift.
constexpr uint32 LdsDwGranularity = 128;
constexpr uint32 LdsDwGranularityShift = 7;

// Granularity, in bytes, of the PS extraLdsSize field.
constexpr uint32 ExtraLdsSizeGranularity = 0x00000400;

// Scratch ring size granularity is 64 dwords.
constexpr uint32 ScratchWaveSizeGranularityShift = 0x00000006;

// Scratch ring max wave size in DWs (16M - 64 DWs)
constexpr size_t MaxScratchWaveSizeInDwords = (SPI_TMPRING_SIZE__WAVESIZE_MASK >> SPI_TMPRING_SIZE__WAVESIZE__SHIFT) <<
                                              ScratchWaveSizeGranularityShift;

static_assert(MaxScratchWaveSizeInDwords ==
               ((COMPUTE_TMPRING_SIZE__WAVESIZE_MASK >> COMPUTE_TMPRING_SIZE__WAVESIZE__SHIFT) <<
                ScratchWaveSizeGranularityShift),
              "SPI, COMPUTE MaxScratchWaveSize do not match!");

// Geometry export rings (primitive and position) base is in units of 64KB.
constexpr uint32 GeometryExportRingShift = 0x00000010;

// Geometry export rings (primitive and position) memory size per SE is in units of 32 bytes.
constexpr uint32 GeometryExportRingMemSizeShift = 0x00000005;

constexpr uint32 MaxGePosRingPos    = 32764;
constexpr uint32 MaxGePrimRingPrims = 16368;

constexpr uint32 MaxNumRbs = 36;

// Number of PS input semantic registers.
constexpr uint32 MaxPsInputSemantics = 32;

// Tile size is fixed at 64kb on all hardware
constexpr uint32 PrtTileSize = (64 * 1024);

constexpr PrtFeatureFlags PrtFeatures = static_cast<PrtFeatureFlags>(
    PrtFeatureBuffer            | // - sparse buffers
    PrtFeatureImage2D           | // - sparse 2D images
    PrtFeatureImage3D           | // - sparse 3D images
    PrtFeatureShaderStatus      | // - residency status in shader instructions
    PrtFeatureShaderLodClamp    | // - LOD clamping in shader instructions
    PrtFeatureUnalignedMipSize  | // - unaligned levels outside of the miptail
    PrtFeaturePerSliceMipTail   | // - per-slice miptail (slice-major ordering)
    PrtFeatureTileAliasing      | // - tile aliasing (without metadata)
    PrtFeatureStrictNull        | // - returning zeros for unmapped tiles
    PrtFeaturePrtPlus);

// The hardware can only support a limited number of scratch waves per CU.
constexpr uint32 MaxScratchWavesPerCu = 32;

// The Streamout Control Buffer has the following layout:
// - 4 Dwords: Buffer offsets
// - 16 Dwords: Prims needed/written 0/1/2/3
// - 4 Dwords: Dwords written 0/1/2/3
// - 1 Dword:  ordered_ID
constexpr uint32 SoCtrlBufSize = 25 * sizeof(uint32);

// The Streamout Control Buffer must adhere to a QWORD alignment.
constexpr uint32 SoCtrlBufAlignShift = 3;

// Streamout targets must adhere to a DWORD alignment.
constexpr uint32 SoTargetAlignShift = 2;

// Query pool addresses must adhere to a QWORD alignment.
constexpr uint32 QueryPoolAlignShift = 3;

// Number of Registers for MSAA sample locations per 2x2 Quad.
constexpr uint32 NumSampleQuadRegs = 4;

// Number of user-data registers per shader stage on the chip. PAL reserves a number of these for internal use, making
// them unusable from the client. The registers PAL reserves are:
//
// [0]  - For the global internal resource table (shader rings, offchip LDS buffers, etc.)
// [1]  - For the constant buffer table for the shader(s).
//
// This leaves registers 2-31 available for the client's use.
constexpr uint32 NumUserDataRegisters = 32;

// On MEC/Compute we only have 16 registers available to use.
constexpr uint32 NumUserDataRegistersAce = 16;

// Starting user-data register index where the low 32 address bits of the global internal table pointer
// (shader ring SRDs, etc.) is written.
constexpr uint16 InternalTblStartReg  = 0;
// Starting user-data register indexes where the low 32 address bits of the constant buffer table pointer
// (internal CBs) for the shader(s) are written.
constexpr uint16 ConstBufTblStartReg = (InternalTblStartReg + 1);

// 1MB of ATM memory per SE.
constexpr uint32  VertexAttributeRingAlignmentBytes = (64 * Util::OneKibibyte);
constexpr uint32  VertexAttributeRingMaxSizeBytes   = (16 * Util::OneMebibyte);

// Spill table stride is one slot of the global spill buffer per one draw, which is used to store VB srd table and
// spilled user data registers (CP copies into from argument buffer). To avoid cache coherency issue,
//
// e.g. the first draw is launched and loads the spill buffer slot into GL2 and K$ with cache line size but CP doesn't
// update VB srd or user data register into the second spill buffer slot yet - note that on gx12 CP write bypasses GL2;
// if each slot is not cache line size aligned, there will be cache coherency issue since second draw may hit the cache
// to read stale data and not load refresh data from mall/memory.
//
// require alignment to be max value of K$ cache line size (64B) and GL2 cache line size (256B).
constexpr uint32 EiSpillTblStrideAlignmentBytes  = 256;
constexpr uint32 EiSpillTblStrideAlignmentDwords = Util::NumBytesToNumDwords(EiSpillTblStrideAlignmentBytes);

// This enum defines the Shader types supported in PM4 type 3 header
enum Pm4ShaderType : uint32
{
    ShaderGraphics = 0,
    ShaderCompute = 1
};

// This enum defines the predicate value supported in PM4 type 3 header
enum Pm4Predicate : uint32
{
    PredDisable = 0,
    PredEnable = 1
};

constexpr uint32 StartingUserDataOffset[] =
{
    UINT32_MAX,
    mmSPI_SHADER_USER_DATA_HS_0,
    UINT32_MAX,
    mmSPI_SHADER_USER_DATA_GS_0,
    UINT32_MAX,
    mmSPI_SHADER_USER_DATA_PS_0,
    mmCOMPUTE_USER_DATA_0,
};
static_assert(Util::ArrayLen(StartingUserDataOffset) == uint32(Util::Abi::HardwareStage::Count),
              "Array does not match expected length!");

// Special value indicating that a user-data entry is not mapped to a physical SPI register.
constexpr uint32 UserDataNotMapped = 0;

// Number of hw shader stages for graphics: counts for hw HS / GS / PS
constexpr uint32 NumHwShaderStagesGfx = 3;

// The maximum number of waves per SH.
constexpr uint32 Gfx12MaxWavesPerShCompute = (COMPUTE_RESOURCE_LIMITS__WAVES_PER_SH_MASK >>
                                              COMPUTE_RESOURCE_LIMITS__WAVES_PER_SH__SHIFT);
// Abstract cache sync flags modeled after the hardware GCR flags.The "Glx" flags apply to the GL2, GL1, and L0 caches
// which are accessible from both graphics and compute engines.
enum SyncGlxFlags : uint8
{
    SyncGlxNone = 0x00,
    // Global caches.
    SyncGl2Inv  = 0x01, // Invalidate the GL2 cache.
    SyncGl2Wb   = 0x02, // Flush the GL2 cache.
    // Shader L0 caches.
    SyncGlvInv  = 0x04, // Invalidate the L0 vector cache.
    SyncGlkInv  = 0x08, // Invalidate the L0 scalar cache.
    SyncGliInv  = 0x10, // Invalidate the L0 instruction cache.

    // A helper enum which combines a GL2 flush and invalidate. Note that an equivalent for glk was not implemented
    // because it should be extremely rare for PAL to flush the glk and we don't want people to do it accidentally.
    SyncGl2WbInv = SyncGl2Wb | SyncGl2Inv,

    // Flush and invalidate all Glx caches.
    SyncGlxWbInvAll = 0x1F,
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

// Define ordered HW acquire points.
//
// PAL's AcquirePoint only exposes a subset of HW's PWS acquire points. Below acquire points are dropped,
// - PRE_SHADER
//     Very close to ME, use ME instead; P/CS_PARTIAL_FLUSH has lighter CP overhead than RELEASE_MEM(P/CS_DONE) +
//     ACQIURE_MEM(PRE_SHADER); the PWS packet pair may stress event FIFOs if the number is large, e.g. 2000+ per
//     frame in TimeSpy.
// - PRE_PIX_SHADER
//     Very close to PreDepth, use PreDepth instead.
//
//  Note that PRE_SHADER and PRE_PIX_SHADER are still broken in HW that they can't fence future events:
//  SPI lets events leak past PWS wait events. This will break our barrier logic because we require that logically
//  sequential barriers like (ColorTarget -> PsRead) and (PsRead -> CsWrite) form a chain of sequential execution.
//  SPI lets PS_DONE events leak past its shader wait points so these barriers would malfunction.
//
// - PRE_COLOR
//  Not used because it is broken in HW.
enum AcquirePoint : uint8
{
    AcquirePointPfp,
    AcquirePointMe,
    AcquirePointPreDepth,
    AcquirePointEop,
    AcquirePointCount
};

// Memory alignment requirement in bytes for shader and immediate constant buffer memory.
static constexpr gpusize PrimeUtcL2MemAlignment = 4096;

// The maximum amount of data that may be compressed into one block
enum MaxUncompressSize : uint32
{
    MaxUncompressSize128B = 0,
    MaxUncompressSize256B = 1,
};

// MAX_UNCOMPRESSED_BLOCK_SIZE must be >= MAX_COMPRESSED_BLOCK_SIZE
constexpr MaxUncompressSize DefaultMaxUncompressedSize = MaxUncompressSize256B;

// Temporal Hint Field for Load/Read Operations
enum class MemoryLoadTemporalHint : uint8
{
    Rt   = 0, // regular temporal (default) for both near and far caches
    Nt   = 1, // non-temporal (re-use not expected) for both near and far caches
    Ht   = 2, // High-priority temporal (precedence over RT) for both near and far caches
    Lu   = 3, // Last-use (non-temporal AND discard dirty if it hits)
    NtRt = 4, // non-temporal for near cache(s) and regular for far caches
    RtNt = 5, // regular for near cache(s) and non-temporal for far caches
    NtHt = 6, // non-temporal for near cache(s) and high-priority temporal for far caches
};

// Temporal Hint Field for Store/Write Operations
enum class MemoryStoreTemporalHint : uint8
{
    Rt   = 0, // regular temporal(default) for both nearand far caches(default wr - rinse)
    Nt   = 1, // non - temporal(re - use not expected) for both nearand far caches
    Ht   = 2, // High - priority temporal(precedence over RT) for both nearand far caches(default wr - rinse)
    Wb   = 3, // Same as "HT", but also overrides wr - rinse in far cache where it forces to stay dirty in cache
    NtRt = 4, // non - temporal for near cache(s) and regular for far caches
    RtNt = 5, // regular for near cache(s) and non - temporal for far caches
    NtHt = 6, // non - temporal for near cache(s) and HT for far caches
    NtWb = 7, // non - temporal for near cache(s) and WB for far cache
};

} // Gfx12
} // Pal
