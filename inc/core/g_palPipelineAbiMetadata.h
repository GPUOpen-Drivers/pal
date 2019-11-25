/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palPipelineAbi.h"
#include "palInlineFuncs.h"
#include "palMsgPack.h"

namespace Util
{
namespace Abi
{

struct BinaryData
{
    const void*  pBuffer;
    uint32       sizeInBytes;
};

struct ShaderMetadata
{
    uint64 apiShaderHash[2];
    uint32 hardwareMapping;

    union
    {
        struct
        {
            uint8 apiShaderHash   : 1;
            uint8 hardwareMapping : 1;
            uint8 reserved        : 6;
        };
        uint8 uAll;
    } hasEntry;
};

struct HardwareStageMetadata
{
    PipelineSymbolType entryPoint;
    uint32             scratchMemorySize;
    uint32             ldsSize;
    uint32             perfDataBufferSize;
    uint32             vgprCount;
    uint32             sgprCount;
    uint32             vgprLimit;
    uint32             sgprLimit;
    uint32             threadgroupDimensions[3];
    uint32             wavefrontSize;
    uint32             maxPrimsPerWave;

    union
    {
        struct
        {
            uint8 usesUavs          : 1;
            uint8 usesRovs          : 1;
            uint8 writesUavs        : 1;
            uint8 writesDepth       : 1;
            uint8 usesAppendConsume : 1;
            uint8 reserved          : 3;
        };
        uint8 uAll;
    } flags;

    union
    {
        struct
        {
            uint16 entryPoint            : 1;
            uint16 scratchMemorySize     : 1;
            uint16 ldsSize               : 1;
            uint16 perfDataBufferSize    : 1;
            uint16 vgprCount             : 1;
            uint16 sgprCount             : 1;
            uint16 vgprLimit             : 1;
            uint16 sgprLimit             : 1;
            uint16 threadgroupDimensions : 1;
            uint16 wavefrontSize         : 1;
            uint16 usesUavs              : 1;
            uint16 usesRovs              : 1;
            uint16 writesUavs            : 1;
            uint16 writesDepth           : 1;
            uint16 usesAppendConsume     : 1;
            uint16 maxPrimsPerWave       : 1;
        };
        uint16 uAll;
    } hasEntry;
};

struct PipelineMetadata
{
    char                  name[64];
    PipelineType          type;
    uint64                internalPipelineHash[2];
    ShaderMetadata        shader[static_cast<uint32>(ApiShaderType::Count)];
    HardwareStageMetadata hardwareStage[static_cast<uint32>(HardwareStage::Count)];
    uint32                userDataLimit;
    uint32                spillThreshold;
    uint32                esGsLdsSize;
    uint32                streamOutTableAddress;
    uint32                indirectUserDataTableAddresses[3];
    uint32                numInterpolants;
    char                  api[16];
    BinaryData            apiCreateInfo;

    union
    {
        struct
        {
            uint8 usesViewportArrayIndex      : 1;
            uint8 calcWaveBreakSizeAtDrawTime : 1;
            uint8 reserved                    : 6;
        };
        uint8 uAll;
    } flags;

    union
    {
        struct
        {
            uint16 name                           : 1;
            uint16 type                           : 1;
            uint16 internalPipelineHash           : 1;
            uint16 userDataLimit                  : 1;
            uint16 spillThreshold                 : 1;
            uint16 usesViewportArrayIndex         : 1;
            uint16 esGsLdsSize                    : 1;
            uint16 streamOutTableAddress          : 1;
            uint16 indirectUserDataTableAddresses : 1;
            uint16 numInterpolants                : 1;
            uint16 placeholder0                   : 1;
            uint16 calcWaveBreakSizeAtDrawTime    : 1;
            uint16 placeholder2                   : 1;
            uint16 api                            : 1;
            uint16 apiCreateInfo                  : 1;
            uint16 reserved                       : 1;
        };
        uint16 uAll;
    } hasEntry;
};

struct PalCodeObjectMetadata
{
    uint32           version[2];
    PipelineMetadata pipeline;

    union
    {
        struct
        {
            uint8 version  : 1;
            uint8 reserved : 7;
        };
        uint8 uAll;
    } hasEntry;
};

namespace PalCodeObjectMetadataKey
{
    static constexpr char Version[]   = "amdpal.version";
    static constexpr char Pipelines[] = "amdpal.pipelines";
};

namespace PipelineMetadataKey
{
    static constexpr char Name[]                           = ".name";
    static constexpr char Type[]                           = ".type";
    static constexpr char InternalPipelineHash[]           = ".internal_pipeline_hash";
    static constexpr char Shaders[]                        = ".shaders";
    static constexpr char HardwareStages[]                 = ".hardware_stages";
    static constexpr char Registers[]                      = ".registers";
    static constexpr char UserDataLimit[]                  = ".user_data_limit";
    static constexpr char SpillThreshold[]                 = ".spill_threshold";
    static constexpr char UsesViewportArrayIndex[]         = ".uses_viewport_array_index";
    static constexpr char EsGsLdsSize[]                    = ".es_gs_lds_size";
    static constexpr char StreamOutTableAddress[]          = ".stream_out_table_address";
    static constexpr char IndirectUserDataTableAddresses[] = ".indirect_user_data_table_addresses";
    static constexpr char NumInterpolants[]                = ".num_interpolants";
    static constexpr char CalcWaveBreakSizeAtDrawTime[]    = ".calc_wave_break_size_at_draw_time";
    static constexpr char Api[]                            = ".api";
    static constexpr char ApiCreateInfo[]                  = ".api_create_info";
};

namespace HardwareStageMetadataKey
{
    static constexpr char EntryPoint[]            = ".entry_point";
    static constexpr char ScratchMemorySize[]     = ".scratch_memory_size";
    static constexpr char LdsSize[]               = ".lds_size";
    static constexpr char PerfDataBufferSize[]    = ".perf_data_buffer_size";
    static constexpr char VgprCount[]             = ".vgpr_count";
    static constexpr char SgprCount[]             = ".sgpr_count";
    static constexpr char VgprLimit[]             = ".vgpr_limit";
    static constexpr char SgprLimit[]             = ".sgpr_limit";
    static constexpr char ThreadgroupDimensions[] = ".threadgroup_dimensions";
    static constexpr char WavefrontSize[]         = ".wavefront_size";
    static constexpr char UsesUavs[]              = ".uses_uavs";
    static constexpr char UsesRovs[]              = ".uses_rovs";
    static constexpr char WritesUavs[]            = ".writes_uavs";
    static constexpr char WritesDepth[]           = ".writes_depth";
    static constexpr char UsesAppendConsume[]     = ".uses_append_consume";
    static constexpr char MaxPrimsPerWave[]       = ".max_prims_per_wave";
};

namespace ShaderMetadataKey
{
    static constexpr char ApiShaderHash[]   = ".api_shader_hash";
    static constexpr char HardwareMapping[] = ".hardware_mapping";
};

namespace Metadata
{

Result DeserializePalCodeObjectMetadata(
    MsgPackReader*  pReader,
    PalCodeObjectMetadata*  pMetadata,
    uint32*  pRegistersOffset);

Result SerializeEnum(MsgPackWriter* pWriter, PipelineType value);
Result SerializeEnum(MsgPackWriter* pWriter, ApiShaderType value);
Result SerializeEnum(MsgPackWriter* pWriter, HardwareStage value);
Result SerializeEnum(MsgPackWriter* pWriter, PipelineSymbolType value);

template <typename EnumType>
Result SerializeEnumBitflags(MsgPackWriter* pWriter, uint32 bitflags);

} // Metadata

} // Abi
} // Util
