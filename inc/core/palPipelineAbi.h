/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
/**
 ***********************************************************************************************************************
 * @file  palPipelineAbi.h
 * @brief PAL Pipeline ABI enums and structures defining the Pipeline ABI spec.
 ***********************************************************************************************************************
 */

#pragma once

#include "palUtil.h"
#include "palElf.h"
#include <cstring>

namespace Util
{
namespace Abi
{

constexpr uint64 PipelineShaderBaseAddrAlignment = 256; ///< Base address alignment for shader stage entry points on
                                                        ///  AMD GPUs.
constexpr uint64 DataMinBaseAddrAlignment        = 32;  ///< Minimum base address alignment for Data section.
constexpr uint64 RoDataMinBaseAddrAlignment      = 32;  ///< Minimum base address alignment for RoData section.

constexpr uint8  ElfOsAbiVersion = 65; ///< ELFOSABI_AMDGPU_PAL
constexpr uint8  ElfAbiVersion   = 0;  ///< ELFABIVERSION_AMDGPU_PAL

constexpr uint32 MetadataNoteType = 32; ///< NT_AMDGPU_METADATA

constexpr uint32 PipelineMetadataMajorVersion = 2;  ///< Pipeline Metadata Major Version
constexpr uint32 PipelineMetadataMinorVersion = 6;  ///< Pipeline Metadata Minor Version

constexpr uint32 PipelineMetadataBase = 0x10000000; ///< Deprecated - Pipeline Metadata base value to be OR'd with the
                                                    ///  PipelineMetadataEntry value when saving to ELF.

static constexpr char AmdGpuVendorName[] = "AMD";           ///< Vendor name string.
static constexpr char AmdGpuArchName[]   = "AMDGPU";        ///< Architecture name string.

/// AmdGpuMachineType for the EF_AMDGPU_MACH selection mask in e_flags.
enum class AmdGpuMachineType : uint8
{
    GfxNone = 0x00,  ///< EF_AMDGPU_MACH_NONE
    Gfx600  = 0x20,  ///< EF_AMDGPU_MACH_AMDGCN_GFX600
    Gfx601  = 0x21,  ///< EF_AMDGPU_MACH_AMDGCN_GFX601
    Gfx602  = 0x3a,  ///< EF_AMDGPU_MACH_AMDGCN_GFX602
    Gfx700  = 0x22,  ///< EF_AMDGPU_MACH_AMDGCN_GFX700
    Gfx701  = 0x23,  ///< EF_AMDGPU_MACH_AMDGCN_GFX701
    Gfx702  = 0x24,  ///< EF_AMDGPU_MACH_AMDGCN_GFX702
    Gfx703  = 0x25,  ///< EF_AMDGPU_MACH_AMDGCN_GFX703
    Gfx704  = 0x26,  ///< EF_AMDGPU_MACH_AMDGCN_GFX704
    Gfx705  = 0x3B,  ///< EF_AMDGPU_MACH_AMDGCN_GFX705
    Gfx800  = 0x27,  ///< EF_AMDGPU_MACH_AMDGCN_GFX800
    Gfx801  = 0x28,  ///< EF_AMDGPU_MACH_AMDGCN_GFX801
    Gfx802  = 0x29,  ///< EF_AMDGPU_MACH_AMDGCN_GFX802
    Gfx803  = 0x2a,  ///< EF_AMDGPU_MACH_AMDGCN_GFX803
    Gfx805  = 0x3c,  ///< EF_AMDGPU_MACH_AMDGCN_GFX805
    Gfx810  = 0x2b,  ///< EF_AMDGPU_MACH_AMDGCN_GFX810
    Gfx900  = 0x2c,  ///< EF_AMDGPU_MACH_AMDGCN_GFX900
    Gfx902  = 0x2d,  ///< EF_AMDGPU_MACH_AMDGCN_GFX902
    Gfx904  = 0x2e,  ///< EF_AMDGPU_MACH_AMDGCN_GFX904
    Gfx906  = 0x2f,  ///< EF_AMDGPU_MACH_AMDGCN_GFX906
    Gfx909  = 0x31,  ///< EF_AMDGPU_MACH_AMDGCN_GFX909
    Gfx90C  = 0x32,  ///< EF_AMDGPU_MACH_AMDGCN_GFX90C
    Gfx1010 = 0x33,  ///< EF_AMDGPU_MACH_AMDGCN_GFX1010
    Gfx1011 = 0x34,  ///< EF_AMDGPU_MACH_AMDGCN_GFX1011
    Gfx1012 = 0x35,  ///< EF_AMDGPU_MACH_AMDGCN_GFX1012
    Gfx1030 = 0x36,  ///< EF_AMDGPU_MACH_AMDGCN_GFX1030
    Gfx1031 = 0x37,  ///< EF_AMDGPU_MACH_AMDGCN_GFX1031
};

/// AmdGpuFeatureV4Type for the feature selection mask bits in e_flags.
enum class AmdGpuFeatureV4Type : uint8
{
    Unsupported = 0x00, ///< EF_AMDGPU_FEATURE_*_UNSUPPORTED_V4
    Any         = 0x01, ///< EF_AMDGPU_FEATURE_*_ANY_V4
    Off         = 0x02, ///< EF_AMDGPU_FEATURE_*_OFF_V4
    On          = 0x03, ///< EF_AMDGPU_FEATURE_*_ON_V4
};

/// Enumerates the steppng values for each GPU supported by PAL (and by PAL's ABI).  There are many duplicates
/// in this list, because some values are re-used across different GFXIP major/minor versions (e.g., Gfx6.0.0
/// and Gfx9.0.0 are different GPUs that happen to share a common stepping number).
enum GfxIpStepping : uint16
{
    // GFXIP 6.0.x steppings:
    GfxIpSteppingTahiti    = 0,
    GfxIpSteppingPitcairn  = 1,
    GfxIpSteppingCapeVerde = 1,
    GfxIpSteppingOland     = 2,
    GfxIpSteppingHainan    = 2,

    // GFXIP 7.0.x steppings:
    GfxIpSteppingKaveri    = 0,
    GfxIpSteppingHawaiiPro = 1,
    GfxIpSteppingHawaii    = 2,
    GfxIpSteppingKalindi   = 3,
    GfxIpSteppingBonaire   = 4,
    GfxIpSteppingGodavari  = 5,

    // GFXIP 8.0.x steppings:
    GfxIpSteppingCarrizo  = 1,
    GfxIpSteppingIceland  = 2,
    GfxIpSteppingTonga    = 2,
    GfxIpSteppingFiji     = 3,
    GfxIpSteppingPolaris  = 3,
    GfxIpSteppingTongaPro = 5,

    // GFXIP 8.1.x steppings:
    GfxIpSteppingStoney = 0,

    // GFXIP 9.0.x steppings:
    GfxIpSteppingVega10 = 0,
    GfxIpSteppingRaven  = 2,
    GfxIpSteppingVega12 = 4,
    GfxIpSteppingVega20 = 6,
    GfxIpSteppingRaven2 = 9,
    GfxIpSteppingRenoir = 12,

    // GFXIP 10.1.x steppings:
    GfxIpSteppingNavi10        = 0,
    GfxIpSteppingNavi12        = 1,
    GfxIpSteppingNavi14        = 2,

    // GFXIP 10.3.x steppings:
    GfxIpSteppingNavi21        = 0,

    // GFXIP 10.3.x steppings:
    GfxIpSteppingNavi22        = 1,

};

/// Name of the section where our pipeline binaries store the disassembly for all shader stages.
static constexpr char AmdGpuDisassemblyName[] = ".AMDGPU.disasm";

/// Name prefix of the section where our pipeline binaries store extra information e.g. LLVM IR.
static constexpr char AmdGpuCommentName[] = ".AMDGPU.comment.";

/// Name of the section where our pipeline binaries store AMDIL disassembly.
static constexpr char AmdGpuCommentAmdIlName[] = ".AMDGPU.comment.amdil";

/// Name of the section where our pipeline binaries store LLVMIR disassembly.
static constexpr char AmdGpuCommentLlvmIrName[] = ".AMDGPU.comment.llvmir";

/// String table of the Pipeline ABI symbols.
static const char* PipelineAbiSymbolNameStrings[] =
{
    "unknown",
    "_amdgpu_ls_main",
    "_amdgpu_hs_main",
    "_amdgpu_es_main",
    "_amdgpu_gs_main",
    "_amdgpu_vs_main",
    "_amdgpu_ps_main",
    "_amdgpu_cs_main",
    "_amdgpu_fs_main",
    "_amdgpu_ls_shdr_intrl_tbl",
    "_amdgpu_hs_shdr_intrl_tbl",
    "_amdgpu_es_shdr_intrl_tbl",
    "_amdgpu_gs_shdr_intrl_tbl",
    "_amdgpu_vs_shdr_intrl_tbl",
    "_amdgpu_ps_shdr_intrl_tbl",
    "_amdgpu_cs_shdr_intrl_tbl",
    "_amdgpu_ls_disasm",
    "_amdgpu_hs_disasm",
    "_amdgpu_es_disasm",
    "_amdgpu_gs_disasm",
    "_amdgpu_vs_disasm",
    "_amdgpu_ps_disasm",
    "_amdgpu_cs_disasm",
    "_amdgpu_ls_shdr_intrl_data",
    "_amdgpu_hs_shdr_intrl_data",
    "_amdgpu_es_shdr_intrl_data",
    "_amdgpu_gs_shdr_intrl_data",
    "_amdgpu_vs_shdr_intrl_data",
    "_amdgpu_ps_shdr_intrl_data",
    "_amdgpu_cs_shdr_intrl_data",
    "_amdgpu_pipeline_intrl_data",
    "_amdgpu_cs_amdil",
    "_amdgpu_task_amdil",
    "_amdgpu_vs_amdil",
    "_amdgpu_hs_amdil",
    "_amdgpu_ds_amdil",
    "_amdgpu_gs_amdil",
    "_amdgpu_mesh_amdil",
    "_amdgpu_ps_amdil",
};

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 619
/// Deprecated - String table of the Pipeline Metadata key names.
static const char* const PipelineMetadataNameStrings[] =
{
    "API_CS_HASH_DWORD0",
    "API_CS_HASH_DWORD1",
    "API_CS_HASH_DWORD2",
    "API_CS_HASH_DWORD3",

    "API_TASK_HASH_DWORD0",
    "API_TASK_HASH_DWORD1",
    "API_TASK_HASH_DWORD2",
    "API_TASK_HASH_DWORD3",

    "API_VS_HASH_DWORD0",
    "API_VS_HASH_DWORD1",
    "API_VS_HASH_DWORD2",
    "API_VS_HASH_DWORD3",

    "API_HS_HASH_DWORD0",
    "API_HS_HASH_DWORD1",
    "API_HS_HASH_DWORD2",
    "API_HS_HASH_DWORD3",

    "API_DS_HASH_DWORD0",
    "API_DS_HASH_DWORD1",
    "API_DS_HASH_DWORD2",
    "API_DS_HASH_DWORD3",

    "API_GS_HASH_DWORD0",
    "API_GS_HASH_DWORD1",
    "API_GS_HASH_DWORD2",
    "API_GS_HASH_DWORD3",

    "API_MESH_HASH_DWORD0",
    "API_MESH_HASH_DWORD1",
    "API_MESH_HASH_DWORD2",
    "API_MESH_HASH_DWORD3",

    "API_PS_HASH_DWORD0",
    "API_PS_HASH_DWORD1",
    "API_PS_HASH_DWORD2",
    "API_PS_HASH_DWORD3",

    "INTERNAL_PIPELINE_HASH_DWORD0",
    "INTERNAL_PIPELINE_HASH_DWORD1",

    "USER_DATA_LIMIT",

    "USES_SAMPLE_INFO_DEPRECATED",

    "HS_MAX_TESS_FACTOR",

    "PS_USES_UAVS",
    "PS_USES_ROVS",

    "PS_RUNS_AT_SAMPLE_RATE_DEPRECATED",

    "SPILL_THRESHOLD",

    "LS_NUM_USED_VGPRS",
    "HS_NUM_USED_VGPRS",
    "ES_NUM_USED_VGPRS",
    "GS_NUM_USED_VGPRS",
    "VS_NUM_USED_VGPRS",
    "PS_NUM_USED_VGPRS",
    "CS_NUM_USED_VGPRS",

    "LS_NUM_USED_SGPRS",
    "HS_NUM_USED_SGPRS",
    "ES_NUM_USED_SGPRS",
    "GS_NUM_USED_SGPRS",
    "VS_NUM_USED_SGPRS",
    "PS_NUM_USED_SGPRS",
    "CS_NUM_USED_SGPRS",

    "LS_NUM_AVAIL_VGPRS",
    "HS_NUM_AVAIL_VGPRS",
    "ES_NUM_AVAIL_VGPRS",
    "GS_NUM_AVAIL_VGPRS",
    "VS_NUM_AVAIL_VGPRS",
    "PS_NUM_AVAIL_VGPRS",
    "CS_NUM_AVAIL_VGPRS",

    "LS_NUM_AVAIL_SGPRS",
    "HS_NUM_AVAIL_SGPRS",
    "ES_NUM_AVAIL_SGPRS",
    "GS_NUM_AVAIL_SGPRS",
    "VS_NUM_AVAIL_SGPRS",
    "PS_NUM_AVAIL_SGPRS",
    "CS_NUM_AVAIL_SGPRS",

    "LS_LDS_BYTE_SIZE",
    "HS_LDS_BYTE_SIZE",
    "ES_LDS_BYTE_SIZE",
    "GS_LDS_BYTE_SIZE",
    "VS_LDS_BYTE_SIZE",
    "PS_LDS_BYTE_SIZE",
    "CS_LDS_BYTE_SIZE",

    "LS_SCRATCH_BYTE_SIZE",
    "HS_SCRATCH_BYTE_SIZE",
    "ES_SCRATCH_BYTE_SIZE",
    "GS_SCRATCH_BYTE_SIZE",
    "VS_SCRATCH_BYTE_SIZE",
    "PS_SCRATCH_BYTE_SIZE",
    "CS_SCRATCH_BYTE_SIZE",

    "STREAM_OUT_TABLE_ENTRY__DEPRECATED",
    "INDIRECT_TABLE_0_ENTRY__DEPRECATED",
    "INDIRECT_TABLE_1_ENTRY__DEPRECATED",
    "INDIRECT_TABLE_2_ENTRY__DEPRECATED",

    "ESGS_LDS_SIZE",
    "USES_VIEWPORT_ARRAY_INDEX",

    "PIPELINE_NAME_INDEX",

    "HW_API_SHADER_MAPPING_LO",
    "HW_API_SHADER_MAPPING_HI",

    "LS_PERFORMANCE_DATA_BUFFER_SIZE",
    "HS_PERFORMANCE_DATA_BUFFER_SIZE",
    "ES_PERFORMANCE_DATA_BUFFER_SIZE",
    "GS_PERFORMANCE_DATA_BUFFER_SIZE",
    "VS_PERFORMANCE_DATA_BUFFER_SIZE",
    "PS_PERFORMANCE_DATA_BUFFER_SIZE",
    "CS_PERFORMANCE_DATA_BUFFER_SIZE",

    "CALC_WAVE_BREAK_SIZE_AT_DRAW_TIME",

    "RESERVED1",

    "PS_WRITES_UAVS",
    "PS_WRITES_DEPTH",
    "PS_USES_APPEND_CONSUME",

    "INTERNAL_PIPELINE_HASH_DWORD2",
    "INTERNAL_PIPELINE_HASH_DWORD3",

    "CS_WAVE_FRONT_SIZE",

    "MESH_SHADER_SCRATCH_BYTE_SIZE"
};

/// The pipeline ABI note types.
enum class PipelineAbiNoteType : uint32
{
    PalMetadata    = MetadataNoteType, ///< Contains metadata needed by the PAL runtime to execute the pipeline.

    /// The following legacy note types are deprecated.

    HsaIsa          = 3,  ///< Structure defining the ISA type in the code object.  Shared with HSA code objects.
    AbiMinorVersion = 8,  ///< ABI minor version.
    LegacyMetadata  = 12, ///< Contains metadata needed by the PAL runtime to execute the pipeline.
};
#endif

/// Pipeline category.
enum PipelineType : uint32
{
    VsPs = 0,
    Gs,
    Cs,
    Ngg,
    Tess,
    GsTess,
    NggTess,
    Mesh,
    TaskMesh,
};

/// Helper enum which is used along with the @ref PipelineSymbolType and @ref PipelineMetadataType to
/// easily find a particular piece of metadata or symbol for any hardware shader stage.
/// @note: The order of these stages must match the order used for each stages' symbol type or metadata
/// type!
enum class HardwareStage : uint32
{
    Ls = 0, ///< Hardware LS stage
    Hs,     ///< Hardware hS stage
    Es,     ///< Hardware ES stage
    Gs,     ///< Hardware GS stage
    Vs,     ///< Hardware VS stage
    Ps,     ///< Hardware PS stage
    Cs,     ///< Hardware CS stage
    Count
};

/// Helper enum which is used along with the @ref GetMetadataHashForApiShader function to easily find
/// a metadata hash dword for a particular API shader type.
enum class ApiShaderType : uint32
{
    Cs = 0, ///< API compute shader
    Task,   ///< API task shader
    Vs,     ///< API vertex shader
    Hs,     ///< API hull shader
    Ds,     ///< API domain shader
    Gs,     ///< API geometry shader
    Mesh,   ///< API mesh shader
    Ps,     ///< API pixel shader
    Count
};

// Helper enum defined shader subType
enum class ApiShaderSubType : uint32
{
    Unknown = 0,
    Traversal,
    RayGeneration,
    Intersection,
    AnyHit,
    ClosestHit,
    Miss,
    Callable,
    Count
};

/// Used to represent hardware shader stage.
enum HardwareStageFlagBits : uint32
{
    HwShaderLs = (1 << uint32(HardwareStage::Ls)),
    HwShaderHs = (1 << uint32(HardwareStage::Hs)),
    HwShaderEs = (1 << uint32(HardwareStage::Es)),
    HwShaderGs = (1 << uint32(HardwareStage::Gs)),
    HwShaderVs = (1 << uint32(HardwareStage::Vs)),
    HwShaderPs = (1 << uint32(HardwareStage::Ps)),
    HwShaderCs = (1 << uint32(HardwareStage::Cs)),
};

/// Used along with the symbol name strings to identify the symbol type.
enum class PipelineSymbolType : uint32
{
    Unknown = 0,       ///< A custom symbol not defined by the Pipeline ABI.
    LsMainEntry,       ///< Hardware LS entry point.  Must be aligned to hardware requirements.
    HsMainEntry,       ///< Hardware HS entry point.  Must be aligned to hardware requirements.
    EsMainEntry,       ///< Hardware ES entry point.  Must be aligned to hardware requirements.
    GsMainEntry,       ///< Hardware GS entry point.  Must be aligned to hardware requirements.
    VsMainEntry,       ///< Hardware VS entry point.  Must be aligned to hardware requirements.
    PsMainEntry,       ///< Hardware PS entry point.  Must be aligned to hardware requirements.
    CsMainEntry,       ///< Hardware CS entry point.  Must be aligned to hardware requirements.
    FsMainEntry,       ///< Hardware FS entry point.  Must be aligned to hardware requirements.
    LsShdrIntrlTblPtr, ///< LS shader internal table pointer.  Optional.  Described in Per-Shader Internal Table.
    HsShdrIntrlTblPtr, ///< HS shader internal table pointer.  Optional.  Described in Per-Shader Internal Table.
    EsShdrIntrlTblPtr, ///< ES shader internal table pointer.  Optional.  Described in Per-Shader Internal Table.
    GsShdrIntrlTblPtr, ///< GS shader internal table pointer.  Optional.  Described in Per-Shader Internal Table.
    VsShdrIntrlTblPtr, ///< VS shader internal table pointer.  Optional.  Described in Per-Shader Internal Table.
    PsShdrIntrlTblPtr, ///< PS shader internal table pointer.  Optional.  Described in Per-Shader Internal Table.
    CsShdrIntrlTblPtr, ///< CS shader internal table pointer.  Optional.  Described in Per-Shader Internal Table.
    LsDisassembly,     ///< Hardware LS disassembly.  Optional.  Associated with the .AMDGPU.disasm section.
    HsDisassembly,     ///< Hardware HS disassembly.  Optional.  Associated with the .AMDGPU.disasm section.
    EsDisassembly,     ///< Hardware ES disassembly.  Optional.  Associated with the .AMDGPU.disasm section.
    GsDisassembly,     ///< Hardware GS disassembly.  Optional.  Associated with the .AMDGPU.disasm section.
    VsDisassembly,     ///< Hardware VS disassembly.  Optional.  Associated with the .AMDGPU.disasm section.
    PsDisassembly,     ///< Hardware PS disassembly.  Optional.  Associated with the .AMDGPU.disasm section.
    CsDisassembly,     ///< Hardware CS disassembly.  Optional.  Associated with the .AMDGPU.disasm section.
    LsShdrIntrlData,   ///< LS shader internal data pointer.  Optional.
    HsShdrIntrlData,   ///< HS shader internal data pointer.  Optional.
    EsShdrIntrlData,   ///< ES shader internal data pointer.  Optional.
    GsShdrIntrlData,   ///< GS shader internal data pointer.  Optional.
    VsShdrIntrlData,   ///< VS shader internal data pointer.  Optional.
    PsShdrIntrlData,   ///< PS shader internal data pointer.  Optional.
    CsShdrIntrlData,   ///< CS shader internal data pointer.  Optional.
    PipelineIntrlData, ///< Cross-shader internal data pointer.  Optional.
    CsAmdIl,           ///< API CS shader AMDIL disassembly.  Optional.
                       ///  Associated with the .AMDGPU.comment.amdil section.
    TaskAmdIl,         ///< API Task shader AMDIL disassembly. Optional.
                       ///  Associated with the .AMDGPU.commd.amdil section.
    VsAmdIl,           ///< API VS shader AMDIL disassembly.  Optional.
                       ///  Associated with the .AMDGPU.comment.amdil section.
    HsAmdIl,           ///< API HS shader AMDIL disassembly.  Optional.
                       ///  Associated with the .AMDGPU.comment.amdil section.
    DsAmdIl,           ///< API DS shader AMDIL disassembly.  Optional.
                       ///  Associated with the .AMDGPU.comment.amdil section.
    GsAmdIl,           ///< API GS shader AMDIL disassembly.  Optional.
                       ///  Associated with the .AMDGPU.comment.amdil section.
    MeshAmdIl,         ///< API Mesh shader AMDIL disassembly. Optional.
                       ///  Associated with the .AMDGPU.commd.amdil section.
    PsAmdIl,           ///< API PS shader AMDIL disassembly.  Optional.
                       ///  Associated with the .AMDGPU.comment.amdil section.
    Count,

    ShaderMainEntry   = LsMainEntry,        ///< Shorthand for the first shader's entry point
    ShaderIntrlTblPtr = LsShdrIntrlTblPtr,  ///< Shorthand for the first shader's internal table pointer
    ShaderDisassembly = LsDisassembly,      ///< Shorthand for the first shader's disassembly string
    ShaderIntrlData   = LsShdrIntrlData,    ///< Shorthand for the first shader's internal data pointer
    ShaderAmdIl       = CsAmdIl,            ///< Shorthand for the first shader's AMDIL disassembly string
};

static_assert(static_cast<uint32>(PipelineSymbolType::Count) == sizeof(PipelineAbiSymbolNameStrings)/sizeof(char*),
              "PipelineSymbolType enum does not match PipelineAbiSymbolNameStrings.");

static_assert(static_cast<uint32>(HardwareStage::Count) <= (sizeof(uint8) * 8),
              "A mask of HardwareStage values will no longer fit into a uint8!");

/// This packed bitfield is used to correlate the @ref ApiShaderType enum with the @ref HardwareStage enum.
union ApiHwShaderMapping
{
    uint8 apiShaders[static_cast<uint32>(ApiShaderType::Count)];

    struct
    {
        uint32 u32Lo;   ///< Low 32 bits of this structure.
        uint32 u32Hi;   ///< High 32 bits of this structure.
    };

    uint64 u64All;      ///< Flags packed as 64-bit uint.
};

static_assert((sizeof(ApiHwShaderMapping) == sizeof(uint64)),
              "ApiHwShaderMapping is different in size than expected!");

/// Helper function to get a pipeline symbol type for a specific hardware shader stage.
///
/// @param [in] symbolType Type of Pipeline Symbol to retrieve
/// @param [in] stage      Hardware shader stage of interest
///
/// @returns PipelineSymbolType enum associated with the base symbol type and hardware stage.
constexpr PipelineSymbolType GetSymbolForStage(
    PipelineSymbolType symbolType,
    HardwareStage      stage)
{
    return static_cast<PipelineSymbolType>(static_cast<uint32>(symbolType) + static_cast<uint32>(stage));
}

/// Helper function to get a pipeline symbol type for a specific API shader stage.
///
/// @param [in] symbolType Type of Pipeline Symbol to retrieve
/// @param [in] stage      API shader stage of interest
///
/// @returns PipelineSymbolType enum associated with the base symbol type and API stage.
constexpr PipelineSymbolType GetSymbolForStage(
    PipelineSymbolType symbolType,
    ApiShaderType      stage)
{
    return static_cast<PipelineSymbolType>(static_cast<uint32>(symbolType) + static_cast<uint32>(stage));
}

/// Helper function to get the symbol type when given a symbol name.
///
/// @param [in] pName The symbol name.
///
/// @returns The corresponding PipelineSymbolType.
inline PipelineSymbolType GetSymbolTypeFromName(const char* pName)
{
    PipelineSymbolType type = PipelineSymbolType::Unknown;
    for (uint32 i = 0; i < static_cast<uint32>(PipelineSymbolType::Count); i++)
    {
        if (strcmp(PipelineAbiSymbolNameStrings[i], pName) == 0)
        {
            type = static_cast<PipelineSymbolType>(i);
            break;
        }
    }

    return type;
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 619
/// Deprecated - Legacy pipeline metadata types.
enum class PipelineMetadataType : uint32
{
    ApiCsHashDword0 = 0,   ///< Dword 0 of a 128-bit hash identifying the API compute shader.
    ApiCsHashDword1,       ///< Dword 1 of a 128-bit hash identifying the API compute shader.
    ApiCsHashDword2,       ///< Dword 2 of a 128-bit hash identifying the API compute shader.
    ApiCsHashDword3,       ///< Dword 3 of a 128-bit hash identifying the API compute shader.

    ApiTaskHashDword0,     ///< Dword 0 of a 128-bit hash identifying the API task shader.
    ApiTaskHashDword1,     ///< Dword 1 of a 128-bit hash identifying the API task shader.
    ApiTaskHashDword2,     ///< Dword 2 of a 128-bit hash identifying the API task shader.
    ApiTaskHashDword3,     ///< Dword 3 of a 128-bit hash identifying the API task shader.

    ApiVsHashDword0,       ///< Dword 0 of a 128-bit hash identifying the API vertex shader.
    ApiVsHashDword1,       ///< Dword 1 of a 128-bit hash identifying the API vertex shader.
    ApiVsHashDword2,       ///< Dword 2 of a 128-bit hash identifying the API vertex shader.
    ApiVsHashDword3,       ///< Dword 3 of a 128-bit hash identifying the API vertex shader.

    ApiHsHashDword0,       ///< Dword 0 of a 128-bit hash identifying the API hull shader.
    ApiHsHashDword1,       ///< Dword 1 of a 128-bit hash identifying the API hull shader.
    ApiHsHashDword2,       ///< Dword 2 of a 128-bit hash identifying the API hull shader.
    ApiHsHashDword3,       ///< Dword 3 of a 128-bit hash identifying the API hull shader.

    ApiDsHashDword0,       ///< Dword 0 of a 128-bit hash identifying the API domain shader.
    ApiDsHashDword1,       ///< Dword 1 of a 128-bit hash identifying the API domain shader.
    ApiDsHashDword2,       ///< Dword 2 of a 128-bit hash identifying the API domain shader.
    ApiDsHashDword3,       ///< Dword 3 of a 128-bit hash identifying the API domain shader.

    ApiGsHashDword0,       ///< Dword 0 of a 128-bit hash identifying the API geometry shader.
    ApiGsHashDword1,       ///< Dword 1 of a 128-bit hash identifying the API geometry shader.
    ApiGsHashDword2,       ///< Dword 2 of a 128-bit hash identifying the API geometry shader.
    ApiGsHashDword3,       ///< Dword 3 of a 128-bit hash identifying the API geometry shader.

    ApiMeshHashDword0,     ///< Dword 0 of a 128-bit hash identifying the API mesh shader.
    ApiMeshHashDword1,     ///< Dword 1 of a 128-bit hash identifying the API mesh shader.
    ApiMeshHashDword2,     ///< Dword 2 of a 128-bit hash identifying the API mesh shader.
    ApiMeshHashDword3,     ///< Dword 3 of a 128-bit hash identifying the API mesh shader.

    ApiPsHashDword0,       ///< Dword 0 of a 128-bit hash identifying the API pixel shader.
    ApiPsHashDword1,       ///< Dword 1 of a 128-bit hash identifying the API pixel shader.
    ApiPsHashDword2,       ///< Dword 2 of a 128-bit hash identifying the API pixel shader.
    ApiPsHashDword3,       ///< Dword 3 of a 128-bit hash identifying the API pixel shader.

    InternalPipelineHashDword0, ///< Dword 0 of a 128-bit hash identifying the internal pipeline (stable portion).
    InternalPipelineHashDword1, ///< Dword 1 of a 128-bit hash identifying the internal pipeline (stable portion).

    UserDataLimit,         ///< Number of user data entries accessed by this pipeline.

    Placeholder0,          ///< This can only be removed when clients have bumped past 382 and offline ELFs
                           ///  are regenerated.  This won't be requred after moving to MessagePack.

    HsMaxTessFactor,       ///< Maximum tessellation factor declared in the pipeline's HS. 32-bit float.

    PsUsesUavs,            ///< 1 if the pipeline's pixel shader reads or writes any UAVs, otherwise 0.
    PsUsesRovs,            ///< 1 if the pipeline's pixel shader reads or writes any ROVs, otherwise 0.

    Placeholder1,          ///< Placeholder for offline compiled ELFs.

    SpillThreshold,        ///< The spill threshold.

    LsNumUsedVgprs,        ///< Number of VGPRs used by this shader
    HsNumUsedVgprs,        ///< Number of VGPRs used by this shader
    EsNumUsedVgprs,        ///< Number of VGPRs used by this shader
    GsNumUsedVgprs,        ///< Number of VGPRs used by this shader
    VsNumUsedVgprs,        ///< Number of VGPRs used by this shader
    PsNumUsedVgprs,        ///< Number of VGPRs used by this shader
    CsNumUsedVgprs,        ///< Number of VGPRs used by this shader

    LsNumUsedSgprs,        ///< Number of SGPRs used by this shader
    HsNumUsedSgprs,        ///< Number of SGPRs used by this shader
    EsNumUsedSgprs,        ///< Number of SGPRs used by this shader
    GsNumUsedSgprs,        ///< Number of SGPRs used by this shader
    VsNumUsedSgprs,        ///< Number of SGPRs used by this shader
    PsNumUsedSgprs,        ///< Number of SGPRs used by this shader
    CsNumUsedSgprs,        ///< Number of SGPRs used by this shader

    LsNumAvailableVgprs,   ///< Number of VGPR's which were available to this shader during compilation
    HsNumAvailableVgprs,   ///< Number of VGPR's which were available to this shader during compilation
    EsNumAvailableVgprs,   ///< Number of VGPR's which were available to this shader during compilation
    GsNumAvailableVgprs,   ///< Number of VGPR's which were available to this shader during compilation
    VsNumAvailableVgprs,   ///< Number of VGPR's which were available to this shader during compilation
    PsNumAvailableVgprs,   ///< Number of VGPR's which were available to this shader during compilation
    CsNumAvailableVgprs,   ///< Number of VGPR's which were available to this shader during compilation

    LsNumAvailableSgprs,   ///< Number of SGPR's which were available to this shader during compilation
    HsNumAvailableSgprs,   ///< Number of SGPR's which were available to this shader during compilation
    EsNumAvailableSgprs,   ///< Number of SGPR's which were available to this shader during compilation
    GsNumAvailableSgprs,   ///< Number of SGPR's which were available to this shader during compilation
    VsNumAvailableSgprs,   ///< Number of SGPR's which were available to this shader during compilation
    PsNumAvailableSgprs,   ///< Number of SGPR's which were available to this shader during compilation
    CsNumAvailableSgprs,   ///< Number of SGPR's which were available to this shader during compilation

    LsLdsByteSize,         ///< Amount of LDS space in bytes used by this shader.
    HsLdsByteSize,         ///< Amount of LDS space in bytes used by this shader.
    EsLdsByteSize,         ///< Amount of LDS space in bytes used by this shader.
    GsLdsByteSize,         ///< Amount of LDS space in bytes used by this shader.
    VsLdsByteSize,         ///< Amount of LDS space in bytes used by this shader.
    PsLdsByteSize,         ///< Amount of LDS space in bytes used by this shader.
    CsLdsByteSize,         ///< Amount of LDS space in bytes used by this shader.

    LsScratchByteSize,     ///< Amount of scratch memory in bytes used by this shader.
    HsScratchByteSize,     ///< Amount of scratch memory in bytes used by this shader.
    EsScratchByteSize,     ///< Amount of scratch memory in bytes used by this shader.
    GsScratchByteSize,     ///< Amount of scratch memory in bytes used by this shader.
    VsScratchByteSize,     ///< Amount of scratch memory in bytes used by this shader.
    PsScratchByteSize,     ///< Amount of scratch memory in bytes used by this shader.
    CsScratchByteSize,     ///< Amount of scratch memory in bytes used by this shader.

    StreamOutTableEntry,   ///< Index of the virtualized user-data entry which stores the 32-bit GPU virtual address
                           ///  of the stream-out SRD table.
    IndirectTableEntryLow, ///< Low range of an index of the virtualized user-data entry which stores the 32-bit GPU
                           ///  virtual address of an indirect user-data table.  Subtract IndirectTableEntryLow to
                           ///  find the index of the indirect table being described.
    /// High range of an index of the virtualized user-data entry which stores the 32bit GPU virtual address of an
    /// indirect user-data table.  Subtract IndirectTableEntryLow to find the index of the indirect table being
    /// described.
    IndirectTableEntryHigh = (IndirectTableEntryLow + 2),

    /// Amount of LDS space used internally for handling data-passing between the ES and GS shader stages.  This can
    /// be zero if the data is passed using off-chip buffers.  This value should be used to program all user-SGPR's
    /// which have been marked with @ref UserDataMapping::EsGsLdsSize (typically only the GS and VS hardware stages
    /// will ever have a user-SGPR so marked).
    EsGsLdsByteSize,

    /// Indicates whether or not the pipeline uses the viewport array index feature.  Pipelines which use this feature
    /// can render into all 16 viewports, whereas pipelines which don't use it are restricted to viewport #0.
    UsesViewportArrayIndex,

    /// Index into the string table contained in the .strtab section of the ELF where a human-readable name for this
    /// pipeline begins.  If this entry is absent from a pipeline binary, or if the .strtab section is absent, then the
    /// pipeline was not given a human-readable name.
    PipelineNameIndex,

    ApiHwShaderMappingLo,  ///< Low 32-bits of the packed bitfield which correlates a ApiShaderType to the applicable
                           ///  HardwareStages. Use @ref ApiHwShaderMapping.u32Lo to unpack value.
    ApiHwShaderMappingHi,  ///< High 32-bits of the packed bitfield which correlates a ApiShaderType to the applicable
                           ///  HardwareStages. Use @ref ApiHwShaderMapping.u32Hi to unpack value.

    LsPerformanceDataBufferSize,  ///< If present, indicates that the performance data buffer is required for this
                                  ///  shader and the size in bytes required.
    HsPerformanceDataBufferSize,  ///< If present, indicates that the performance data buffer is required for this
                                  ///  shader and the size in bytes required.
    EsPerformanceDataBufferSize,  ///< If present, indicates that the performance data buffer is required for this
                                  ///  shader and the size in bytes required.
    GsPerformanceDataBufferSize,  ///< If present, indicates that the performance data buffer is required for this
                                  ///  shader and the size in bytes required.
    VsPerformanceDataBufferSize,  ///< If present, indicates that the performance data buffer is required for this
                                  ///  shader and the size in bytes required.
    PsPerformanceDataBufferSize,  ///< If present, indicates that the performance data buffer is required for this
                                  ///  shader and the size in bytes required.
    CsPerformanceDataBufferSize,  ///< If present, indicates that the performance data buffer is required for this
                                  ///  shader and the size in bytes required.

    CalcWaveBreakSizeAtDrawTime,  ///< Deprecated.  This will be removed at a future date.

    Reserved9,                    ///< Reserved for future use.

    PsWritesUavs,                 ///< 1 if the pipeline's pixel shader writes any UAVs, otherwise 0.
    PsWritesDepth,                ///< 1 if the pipeline's pixel shader writes depth values, otherwise 0.
    PsUsesAppendConsume,          ///< 1 if the pipeline's pixel shader uses UAV append/consume operations, otherwise 0.

    InternalPipelineHashDword2,   ///< Dword 2 of a 128-bit hash identifying the internal pipeline (unique portion).
    InternalPipelineHashDword3,   ///< Dword 3 of a 128-bit hash identifying the internal pipeline (unique portion).

    CsWaveFrontSize,              ///< Wave front size.

    MeshScratchByteSize,          ///< Amount of mesh-shader scratch space needed (bytes)

    Count,

    ShaderNumUsedVgprs              = LsNumUsedVgprs,              ///< Shorthand for the first shader's used VGPR count
    ShaderNumUsedSgprs              = LsNumUsedSgprs,              ///< Shorthand for the first shader's used SGPR count
    ShaderNumAvailVgprs             = LsNumAvailableVgprs,         ///< Shorthand for the first shader's available VGPR
                                                                   ///  count
    ShaderNumAvailSgprs             = LsNumAvailableSgprs,         ///< Shorthand for the first shader's available SGPR
                                                                   ///  count
    ShaderLdsByteSize               = LsLdsByteSize,               ///< Shorthand for the first shader's used LDS size
    ShaderScratchByteSize           = LsScratchByteSize,           ///< Shorthand for the first shader's used scratch
                                                                   ///  size.
    ShaderPerformanceDataBufferSize = LsPerformanceDataBufferSize, ///< Shorthand for the first shader's performance
                                                                   ///  data buffer's size.
};

/// Deprecated - Helper function to get a pipeline metadata type for a specific hardware shader stage.
///
/// @param [in] metadataType Type of Pipeline Metadata to retrieve
/// @param [in] stage        Hardware shader stage of interest
///
/// @returns PipelineMetadataType enum associated with the base symbol type and hardware stage.
constexpr PipelineMetadataType GetMetadataForStage(
    PipelineMetadataType metadataType,
    HardwareStage        stage)
{
    return static_cast<PipelineMetadataType>(static_cast<uint32>(metadataType) + static_cast<uint32>(stage));
}

/// Deprecated - Helper function to get a pipeline metadata hash type for a specific API shader stage.
///
/// @param [in] shader        API shader stage of interest
/// @param [in] dwordSelected The selected dword [0-3]
///
/// @returns PipelineMetadataType enum for the given API shader and selected dword.
constexpr PipelineMetadataType GetMetadataHashForApiShader(
    ApiShaderType shader,
    uint32        dwordSelected)
{
    return static_cast<PipelineMetadataType>(
        (static_cast<uint32>(PipelineMetadataType::ApiCsHashDword0) + dwordSelected) +
        (static_cast<uint32>(shader) << 2));
}
#endif

/// User data entries can map to physical user data registers.  UserDataMapping describes the
/// content of the registers.
enum class UserDataMapping : uint32
{
    GlobalTable       = 0x10000000, ///< 32-bit pointer to GPU memory containing the global internal table.
    PerShaderTable    = 0x10000001, ///< 32-bit pointer to GPU memory containing the per-shader internal table.
    SpillTable        = 0x10000002, ///< 32-bit pointer to GPU memory containing the user data spill table.
    BaseVertex        = 0x10000003, ///< Vertex offset (32-bit unsigned integer). Not needed if the pipeline doesn't
                                    ///  reference the draw index in the vertex shader. Only supported by the first
                                    ///  stage in a graphics pipeline.
    BaseInstance      = 0x10000004, ///< Instance offset (32-bit unsigned integer). Only supported by the first stage in
                                    ///  a graphics pipeline.
    DrawIndex         = 0x10000005, ///< Draw index (32-bit unsigned integer). Only supported by the first stage in a
                                    ///  graphics pipeline.
    Workgroup         = 0x10000006, ///< Thread group count (32-bit unsigned integer). Low half of a 64-bit address of
                                    ///  a buffer containing the grid dimensions for a Compute dispatch operation. The
                                    ///  high half of the address is stored in the next sequential user-SGPR. Only
                                    ///  supported by compute pipelines.
    EsGsLdsSize       = 0x1000000A, ///< Indicates that PAL will program this user-SGPR to contain the amount of LDS
                                    ///  space used for the ES/GS pseudo-ring-buffer for passing data between shader
                                    ///  stages.
    ViewId            = 0x1000000B, ///< View id (32-bit unsigned integer) identifies a view of graphic
                                    ///  pipeline instancing.
    StreamOutTable    = 0x1000000C, ///< 32-bit pointer to GPU memory containing the stream out target SRD table.  This
                                    ///  can only appear for one shader stage per pipeline.
    PerShaderPerfData = 0x1000000D, ///< 32-bit pointer to GPU memory containing the per-shader performance data buffer.
    VertexBufferTable = 0x1000000F, ///< 32-bit pointer to GPU memory containing the vertex buffer SRD table.  This can
                                    ///  only appear for one shader stage per pipeline.
    UavExportTable    = 0x10000010, ///< 32-bit pointer to GPU memory containing the UAV export SRD table.  This can
                                    ///  only appear for one shader stage per pipeline (PS). These replace color targets
                                    ///  and are completely separate from any UAVs used by the shader. This is optional,
                                    ///  and only used by the PS when UAV exports are used to replace color-target
                                    ///  exports to optimize specific shaders.
    NggCullingData    = 0x10000011, ///< 64-bit pointer to GPU memory containing the hardware register data needed by
                                    ///  some NGG pipelines to perform culling.  This value contains the address of the
                                    ///  first of two consecutive registers which provide the full GPU address.
    MeshTaskDispatchDims  = 0x10000012,  ///< Offset to three consecutive registers which indicate the number of
                                         ///  threadgroups dispatched in the X, Y, and Z dimensions.
    MeshTaskRingIndex     = 0x10000013,  ///< Index offset (32-bit unsigned integer). Indicates the index into the
                                         ///  Mesh/Task shader rings for the shader to consume.
    TaskDispatchIndex     = DrawIndex,   ///< Dispatch index (32-bit unsigned integer). Only supported by the first
                                         ///  stage (task shader stage) in a hybrid graphics pipeline.
    MeshPipeStatsBuf      = 0x10000014,  ///< 32-bit GPU virtual address of a buffer storing the shader-emulated mesh
                                         ///  pipeline stats query.
    FetchShaderPtr        = 0x10000015,  ///< 64-bit pointer to GPU memory containing the fetch shader subroutine.

    /// @internal The following enum values are deprecated and only remain in the header file to avoid build errors.

    GdsRange          = 0x10000007, ///< GDS range (32-bit unsigned integer: gdsSizeInBytes | (gdsOffsetInBytes << 16)).
                                    ///  Only supported by compute pipelines.
    BaseIndex         = 0x10000008, ///< Index offset (32-bit unsigned integer). Only supported by the first stage in a
                                    ///  graphics pipeline.
    Log2IndexSize     = 0x10000009, ///< Base-2 logarithm of the size of each index buffer entry.
    IndirectTableLow  = 0x20000000, ///< Low range of 32-bit pointer to GPU memory containing the
                                    ///  address of the indirect user data table.
                                    ///  Subtract 0x20000000.
    IndirectTableHigh = 0x2FFFFFFF, ///< High range of 32-bit pointer to GPU memory containing the
                                    ///  address of the indirect user data table.
                                    ///  Subtract 0x20000000.
};

/// The ABI section type.  The Code (.text), and Data (.data)
/// sections are the main sections interacted with in the Pipeline ABI.
enum class AbiSectionType : uint32
{
    Undefined = 0, ///< An unassociated section
    Code,          ///< The code (.text) section containing executable machine code for all shader stages.
    Data,          ///< Data section
    Disassembly,   ///< Disassembly section
    AmdIl,         ///< AMDIL section
    LlvmIr         ///< LLVMIR section
};

/// These Relocation types are specific to the AMDGPU target machine architecture.
/// Relocation computation notation:
/// A   - The addend used to compute the value of the relocatable field. In Rel sections the addend
///       is obtained from the original value of the word being relocated. In Rela sections an
///       explicit field for a full-width addend is provided.
/// B   - The base address at which a shared object is loaded into memory during execution.
///       Generally, a shared object file is built with a base virtual address of 0.
///       However, the execution address of the shared object is different.
///       NOTE: As PAL does not know the base address until runtime this value
///             has to be externally provided when applying relocations.
/// G   - Represents the offset into the global offset table at which the
///       relocation entry's symbol will reside during execution.
/// GOT - Represents the address of the global offset table.
/// P   - The section offset or address of the storage unit being relocated, computed using r_offset.
/// S   - The value of the symbol whose index resides in the relocation entry.
/// Z   - The size of the symbol whose index resides in the relocation entry.
/// SEE: https://llvm.org/docs/AMDGPUUsage.html#relocation-records
/// for AMDGPU defined relocations.
enum class RelocationType : uint32
{
    None = 0,     ///< val: 0  | field: none   | calc: none
    Abs32Lo,      ///< val: 1  | field: word32 | calc: (S + A) & 0xFFFFFFFF
    Abs32Hi,      ///< val: 2  | field: word32 | calc: (S + A) >> 32
    Abs64,        ///< val: 3  | field: word64 | calc: S + A
    Rel32,        ///< val: 4  | field: word32 | calc: S + A - P
    Rel64,        ///< val: 5  | field: word64 | calc: S + A - P
    Abs32,        ///< val: 6  | field: word32 | calc: S + A
    GotPcRel,     ///< val: 7  | field: word32 | calc: G + GOT + A - P
    GotPcRel32Lo, ///< val: 8  | field: word32 | calc: (G + GOT + A - P) & 0xFFFFFFFF
    GotPcRel32Hi, ///< val: 9  | field: word32 | calc: (G + GOT + A - P) >> 32
    Rel32Lo,      ///< val: 10 | field: word32 | calc: (S + A - P) & 0xFFFFFFFF
    Rel32Hi,      ///< val: 11 | field: word32 | calc: (S + A - P) >> 32
    Rel16 = 14,   ///< val: 14 | field: word16 | calc: ((S + A - P) - 4) / 4
};

/// Contains only the relevant info for a pipeline symbol.
struct PipelineSymbolEntry
{
    PipelineSymbolType        type;
    Elf::SymbolTableEntryType entryType;
    AbiSectionType            sectionType;
    uint64                    value;
    uint64                    size;
};

/// Contains only the relevant info for a pipeline symbol.  E.g., a symbol whose name doesn't match any of the
/// predefined types in @ref PipelineSymbolType.
struct GenericSymbolEntry
{
    const char*               pName;
    Elf::SymbolTableEntryType entryType;
    AbiSectionType            sectionType;
    uint64                    value;
    uint64                    size;
};

/// The structure of the AMDGPU ELF e_flags header field.
union AmdGpuElfFlags
{
    struct
    {
        uint32 machineId      :  8;  ///< EF_AMDGPU_MACH
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 634
        uint32 xnackEnabled   :  1;  ///< EF_AMDGPU_XNACK
        uint32 reserved633    :  3;
#else
        uint32 xnackFeature   :  2;  ///< EF_AMDGPU_FEATURE_XNACK_V4
        uint32 sramEccFeature :  2;  ///< EF_AMDGPU_FEATURE_SRAMECC_V4
#endif
        uint32 reserved       : 20;
    };

    AmdGpuMachineType machineType;   ///< EF_AMDGPU_MACH

    uint32 u32All;                   ///< e_flags packed as a 32-bit unsigned integer.
};

static_assert(sizeof(AmdGpuMachineType) == 1, "AmdGpuMachineType enum underlying type is larger than expected.");

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 619
/// Deprecated - The structure of a NT_AMDGPU_HSA_ISA note.
#pragma pack (push, 1)
struct AbiAmdGpuVersionNote
{
    uint16 vendorNameSize;
    uint16 archNameSize;
    uint32 gfxipMajorVer;
    uint32 gfxipMinorVer;
    uint32 gfxipStepping;
    char vendorName[sizeof(AmdGpuVendorName)];
    char archName[sizeof(AmdGpuArchName)];
};
#pragma pack (pop)

/// Deprecated - The structure of a NT_AMDGPU_ABI_MINOR_VERSION note.
#pragma pack (push, 1)
struct AbiMinorVersionNote
{
    uint32 minorVersion;
};
#pragma pack (pop)

/// Deprecated - Holds the details required by PAL runtime to configure and launch the pipeline.
#pragma pack (push, 1)
struct PalMetadataNoteEntry
{
    uint32 key;
    uint32 value;
};
#pragma pack (pop)

/// Deprecated - Interpret the PalMetadataNoteEntry as pipeline metadata if the key >= 0x10000000.
/// The pipeline metadata specifies metadata useful to PAL that doesn't correspond directly
/// to a GFX hardware register.
#pragma pack (push, 1)
struct PipelineMetadataEntry
{
    PipelineMetadataType key;
    uint32               value;
};
#pragma pack (pop)

/// Deprecated - Interpret the PalMetadataNoteEntry as a register if the key < 0x10000000.
/// The value specifies how the GFX hardware register at the corresponding byte offset should be
/// programmed by the PAL runtime when executing the pipeline.
typedef PalMetadataNoteEntry RegisterEntry;
#endif

/// Maximum number of viewports.
constexpr uint32 MaxViewports = 16;

// Constant buffer used by the primitive shader when culling is enabled.
// Passes the currently set register state to the shader to control the culling algorithm.
struct PrimShaderCullingCb
{
    uint32 padding0;
    uint32 padding1;

    uint32 paClVteCntl;                         ///< Viewport transform control.
    uint32 paSuVtxCntl;                         ///< Controls for float to fixed vertex conversion.
    uint32 paClClipCntl;                        ///< Clip space controls.

    uint32 padding2;
    uint32 padding3;

    uint32 paSuScModeCntl;                      ///< Culling controls.

    uint32 paClGbHorzClipAdj;                   ///< Frustum horizontal adjacent culling control.
    uint32 paClGbHorzDiscAdj;                   ///< Frustum horizontal discard culling control.
    uint32 paClGbVertClipAdj;                   ///< Frustum vertical adjacent culling control.
    uint32 paClGbVertDiscAdj;                   ///< Frustum vertical discard culling control.

    uint32 padding4;

    struct Viewports
    {
        uint32 paClVportXScale;                 ///< Viewport transform scale for X.
        uint32 paClVportXOffset;                ///< Viewport transform offset for X.
        uint32 paClVportYScale;                 ///< Viewport transform scale for Y.
        uint32 paClVportYOffset;                ///< Viewport transform offset for Y.
        uint32 padding5;
        uint32 padding6;
    } viewports[MaxViewports];

    struct Scissors
    {
        uint32 padding7;
        uint32 padding8;
    } scissors[MaxViewports];

    uint32 padding9;
    uint32 padding10;
    uint32 padding11;

    uint32 enableConservativeRasterization;     ///< Conservative rasterization is enabled, disabled certain culling
                                                ///  algorithms.
};

/// Constant buffer used by primitive shader generation for per-submit register controls of culling.
struct PrimShaderPsoCb
{
    uint32 gsAddressLo;              ///< Low 32-bits of GS address used for a jump from ES.
    uint32 gsAddressHi;              ///< High 32-bits of GS address used for a jump from ES.
    uint32 paClVteCntl;              ///< Viewport transform control.
    uint32 paSuVtxCntl;              ///< Controls for float to fixed vertex conversion.
    uint32 paClClipCntl;             ///< Clip space controls.
    uint32 paScWindowOffset;         ///< Offset for vertices in screen space.
    uint32 paSuHardwareScreenOffset; ///< Offset for guardband.
    uint32 paSuScModeCntl;           ///< Culling controls.
    uint32 paClGbHorzClipAdj;        ///< Frustrum horizontal adjacent culling control.
    uint32 paClGbVertClipAdj;        ///< Frustrum vertical adjacent culling control.
    uint32 paClGbHorzDiscAdj;        ///< Frustrum horizontal discard culling control.
    uint32 paClGbVertDiscAdj;        ///< Frustrum vertical discard culling control.
    uint32 vgtPrimitiveType;         ///< Runtime handling of primitive type
};

/// Constant buffer used by primitive shader generation for per-submit register controls of viewport transform.
struct PrimShaderVportCb
{
    struct Controls
    {
        /// Viewport transform scale and offset for x, y, z components
        uint32 paClVportXscale;
        uint32 paClVportXoffset;
        uint32 paClVportYscale;
        uint32 paClVportYoffset;
        uint32 paClVportZscale;
        uint32 paClVportZoffset;
    } vportControls[MaxViewports];
};

/// Constant buffer used by primitive shader generation for per-submit register controls of bounding boxes.
struct PrimShaderScissorCb
{
    struct
    {
        /// Viewport scissor that defines a bounding box
        uint32 paScVportScissorTL;
        uint32 paScVportScissorBR;
    } scissorControls[MaxViewports];
};

/// Constant buffer used by the primitive shader generation for various render state not known until draw time
struct PrimShaderRenderCb
{
    uint32 primitiveRestartEnable;          ///< Enable resetting of a triangle strip using a special index.
    uint32 primitiveRestartIndex;           ///< Value used to determine if a primitive restart is triggered
    uint32 matchAllBits;                    ///< When comparing restart indices, this limits number of bits
    uint32 enableConservativeRasterization; ///< Conservative rasterization is enabled, triggering special logic
                                            ///  for culling.
};

/// This struct defines the expected layout in memory when 'contiguousCbs' is set
struct PrimShaderCbLayout
{
    PrimShaderPsoCb     pipelineStateCb;
    PrimShaderVportCb   viewportStateCb;
    PrimShaderScissorCb scissorStateCb;
    PrimShaderRenderCb  renderStateCb;
};

static_assert(sizeof(PrimShaderCullingCb) == sizeof(PrimShaderCbLayout),
    "Transition structure (PrimShaderCullingCb) is not the same size as original structure (PrimShaderCbLayout)!");

} //Abi
} //Pal
