/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "g_palPipelineAbiMetadata.h"
#include "palMsgPackImpl.h"
#include "palHashLiteralString.h"

namespace Util
{
namespace Abi
{
namespace Metadata
{

// =====================================================================================================================
PAL_INLINE Result DeserializeEnum(
    MsgPackReader*  pReader,
    PipelineType*  pValue)
{
    Result result = pReader->Next(CWP_ITEM_STR);

    if (result == Result::Success)
    {
        const uint32 strHash = HashString(static_cast<const char*>(pReader->Get().as.str.start),
                                          pReader->Get().as.str.length);

        switch (strHash)
        {
        case HashLiteralString("VsPs"):
            *pValue = PipelineType::VsPs;
            break;
        case HashLiteralString("Gs"):
            *pValue = PipelineType::Gs;
            break;
        case HashLiteralString("Cs"):
            *pValue = PipelineType::Cs;
            break;
        case HashLiteralString("Ngg"):
            *pValue = PipelineType::Ngg;
            break;
        case HashLiteralString("Tess"):
            *pValue = PipelineType::Tess;
            break;
        case HashLiteralString("GsTess"):
            *pValue = PipelineType::GsTess;
            break;
        case HashLiteralString("NggTess"):
            *pValue = PipelineType::NggTess;
            break;
        case HashLiteralString("Mesh"):
            *pValue = PipelineType::Mesh;
            break;
        case HashLiteralString("TaskMesh"):
            *pValue = PipelineType::TaskMesh;
            break;
        default:
            result = Result::NotFound;
            break;
        }
    }

    return result;
}

// =====================================================================================================================
PAL_INLINE Result SerializeEnum(
    MsgPackWriter*  pWriter,
    PipelineType  value)
{
    Result result = Result::ErrorInvalidValue;

    switch (value)
    {
    case PipelineType::VsPs:
        result = pWriter->Pack("VsPs");
        break;
    case PipelineType::Gs:
        result = pWriter->Pack("Gs");
        break;
    case PipelineType::Cs:
        result = pWriter->Pack("Cs");
        break;
    case PipelineType::Ngg:
        result = pWriter->Pack("Ngg");
        break;
    case PipelineType::Tess:
        result = pWriter->Pack("Tess");
        break;
    case PipelineType::GsTess:
        result = pWriter->Pack("GsTess");
        break;
    case PipelineType::NggTess:
        result = pWriter->Pack("NggTess");
        break;
    case PipelineType::Mesh:
        result = pWriter->Pack("Mesh");
        break;
    case PipelineType::TaskMesh:
        result = pWriter->Pack("TaskMesh");
        break;
    default:
        break;
    }

    return result;
}

// =====================================================================================================================
PAL_INLINE Result DeserializeEnum(
    MsgPackReader*  pReader,
    ApiShaderType*  pValue)
{
    Result result = pReader->Next(CWP_ITEM_STR);

    if (result == Result::Success)
    {
        const uint32 strHash = HashString(static_cast<const char*>(pReader->Get().as.str.start),
                                          pReader->Get().as.str.length);

        switch (strHash)
        {
        case HashLiteralString(".compute"):
            *pValue = ApiShaderType::Cs;
            break;
        case HashLiteralString(".task"):
            *pValue = ApiShaderType::Task;
            break;
        case HashLiteralString(".vertex"):
            *pValue = ApiShaderType::Vs;
            break;
        case HashLiteralString(".hull"):
            *pValue = ApiShaderType::Hs;
            break;
        case HashLiteralString(".domain"):
            *pValue = ApiShaderType::Ds;
            break;
        case HashLiteralString(".geometry"):
            *pValue = ApiShaderType::Gs;
            break;
        case HashLiteralString(".mesh"):
            *pValue = ApiShaderType::Mesh;
            break;
        case HashLiteralString(".pixel"):
            *pValue = ApiShaderType::Ps;
            break;
        default:
            result = Result::NotFound;
            break;
        }
    }

    return result;
}

// =====================================================================================================================
PAL_INLINE Result SerializeEnum(
    MsgPackWriter*  pWriter,
    ApiShaderType  value)
{
    Result result = Result::ErrorInvalidValue;

    switch (value)
    {
    case ApiShaderType::Cs:
        result = pWriter->Pack(".compute");
        break;
    case ApiShaderType::Task:
        result = pWriter->Pack(".task");
        break;
    case ApiShaderType::Vs:
        result = pWriter->Pack(".vertex");
        break;
    case ApiShaderType::Hs:
        result = pWriter->Pack(".hull");
        break;
    case ApiShaderType::Ds:
        result = pWriter->Pack(".domain");
        break;
    case ApiShaderType::Gs:
        result = pWriter->Pack(".geometry");
        break;
    case ApiShaderType::Mesh:
        result = pWriter->Pack(".mesh");
        break;
    case ApiShaderType::Ps:
        result = pWriter->Pack(".pixel");
        break;
    default:
        break;
    }

    return result;
}

// =====================================================================================================================
PAL_INLINE Result DeserializeEnum(
    MsgPackReader*  pReader,
    ApiShaderSubType*  pValue)
{
    Result result = pReader->Next(CWP_ITEM_STR);

    if (result == Result::Success)
    {
        const uint32 strHash = HashString(static_cast<const char*>(pReader->Get().as.str.start),
                                          pReader->Get().as.str.length);

        switch (strHash)
        {
        case HashLiteralString("Unknown"):
            *pValue = ApiShaderSubType::Unknown;
            break;
        case HashLiteralString("Traversal"):
            *pValue = ApiShaderSubType::Traversal;
            break;
        case HashLiteralString("RayGeneration"):
            *pValue = ApiShaderSubType::RayGeneration;
            break;
        case HashLiteralString("Intersection"):
            *pValue = ApiShaderSubType::Intersection;
            break;
        case HashLiteralString("AnyHit"):
            *pValue = ApiShaderSubType::AnyHit;
            break;
        case HashLiteralString("ClosestHit"):
            *pValue = ApiShaderSubType::ClosestHit;
            break;
        case HashLiteralString("Miss"):
            *pValue = ApiShaderSubType::Miss;
            break;
        case HashLiteralString("Callable"):
            *pValue = ApiShaderSubType::Callable;
            break;
        default:
            result = Result::NotFound;
            break;
        }
    }

    return result;
}

// =====================================================================================================================
PAL_INLINE Result SerializeEnum(
    MsgPackWriter*  pWriter,
    ApiShaderSubType  value)
{
    Result result = Result::ErrorInvalidValue;

    switch (value)
    {
    case ApiShaderSubType::Unknown:
        result = pWriter->Pack("Unknown");
        break;
    case ApiShaderSubType::Traversal:
        result = pWriter->Pack("Traversal");
        break;
    case ApiShaderSubType::RayGeneration:
        result = pWriter->Pack("RayGeneration");
        break;
    case ApiShaderSubType::Intersection:
        result = pWriter->Pack("Intersection");
        break;
    case ApiShaderSubType::AnyHit:
        result = pWriter->Pack("AnyHit");
        break;
    case ApiShaderSubType::ClosestHit:
        result = pWriter->Pack("ClosestHit");
        break;
    case ApiShaderSubType::Miss:
        result = pWriter->Pack("Miss");
        break;
    case ApiShaderSubType::Callable:
        result = pWriter->Pack("Callable");
        break;
    default:
        break;
    }

    return result;
}

// =====================================================================================================================
PAL_INLINE Result DeserializeEnum(
    MsgPackReader*  pReader,
    HardwareStage*  pValue)
{
    Result result = pReader->Next(CWP_ITEM_STR);

    if (result == Result::Success)
    {
        const uint32 strHash = HashString(static_cast<const char*>(pReader->Get().as.str.start),
                                          pReader->Get().as.str.length);

        switch (strHash)
        {
        case HashLiteralString(".ls"):
            *pValue = HardwareStage::Ls;
            break;
        case HashLiteralString(".hs"):
            *pValue = HardwareStage::Hs;
            break;
        case HashLiteralString(".es"):
            *pValue = HardwareStage::Es;
            break;
        case HashLiteralString(".gs"):
            *pValue = HardwareStage::Gs;
            break;
        case HashLiteralString(".vs"):
            *pValue = HardwareStage::Vs;
            break;
        case HashLiteralString(".ps"):
            *pValue = HardwareStage::Ps;
            break;
        case HashLiteralString(".cs"):
            *pValue = HardwareStage::Cs;
            break;
        default:
            result = Result::NotFound;
            break;
        }
    }

    return result;
}

// =====================================================================================================================
PAL_INLINE Result SerializeEnum(
    MsgPackWriter*  pWriter,
    HardwareStage  value)
{
    Result result = Result::ErrorInvalidValue;

    switch (value)
    {
    case HardwareStage::Ls:
        result = pWriter->Pack(".ls");
        break;
    case HardwareStage::Hs:
        result = pWriter->Pack(".hs");
        break;
    case HardwareStage::Es:
        result = pWriter->Pack(".es");
        break;
    case HardwareStage::Gs:
        result = pWriter->Pack(".gs");
        break;
    case HardwareStage::Vs:
        result = pWriter->Pack(".vs");
        break;
    case HardwareStage::Ps:
        result = pWriter->Pack(".ps");
        break;
    case HardwareStage::Cs:
        result = pWriter->Pack(".cs");
        break;
    default:
        break;
    }

    return result;
}

// =====================================================================================================================
PAL_INLINE Result DeserializeEnum(
    MsgPackReader*  pReader,
    PipelineSymbolType*  pValue)
{
    Result result = pReader->Next(CWP_ITEM_STR);

    if (result == Result::Success)
    {
        const uint32 strHash = HashString(static_cast<const char*>(pReader->Get().as.str.start),
                                          pReader->Get().as.str.length);

        switch (strHash)
        {
        case HashLiteralString("unknown"):
            *pValue = PipelineSymbolType::Unknown;
            break;
        case HashLiteralString("_amdgpu_ls_main"):
            *pValue = PipelineSymbolType::LsMainEntry;
            break;
        case HashLiteralString("_amdgpu_hs_main"):
            *pValue = PipelineSymbolType::HsMainEntry;
            break;
        case HashLiteralString("_amdgpu_es_main"):
            *pValue = PipelineSymbolType::EsMainEntry;
            break;
        case HashLiteralString("_amdgpu_gs_main"):
            *pValue = PipelineSymbolType::GsMainEntry;
            break;
        case HashLiteralString("_amdgpu_vs_main"):
            *pValue = PipelineSymbolType::VsMainEntry;
            break;
        case HashLiteralString("_amdgpu_ps_main"):
            *pValue = PipelineSymbolType::PsMainEntry;
            break;
        case HashLiteralString("_amdgpu_cs_main"):
            *pValue = PipelineSymbolType::CsMainEntry;
            break;
        case HashLiteralString("_amdgpu_fs_main"):
            *pValue = PipelineSymbolType::FsMainEntry;
            break;
        case HashLiteralString("_amdgpu_ls_shdr_intrl_tbl"):
            *pValue = PipelineSymbolType::LsShdrIntrlTblPtr;
            break;
        case HashLiteralString("_amdgpu_hs_shdr_intrl_tbl"):
            *pValue = PipelineSymbolType::HsShdrIntrlTblPtr;
            break;
        case HashLiteralString("_amdgpu_es_shdr_intrl_tbl"):
            *pValue = PipelineSymbolType::EsShdrIntrlTblPtr;
            break;
        case HashLiteralString("_amdgpu_gs_shdr_intrl_tbl"):
            *pValue = PipelineSymbolType::GsShdrIntrlTblPtr;
            break;
        case HashLiteralString("_amdgpu_vs_shdr_intrl_tbl"):
            *pValue = PipelineSymbolType::VsShdrIntrlTblPtr;
            break;
        case HashLiteralString("_amdgpu_ps_shdr_intrl_tbl"):
            *pValue = PipelineSymbolType::PsShdrIntrlTblPtr;
            break;
        case HashLiteralString("_amdgpu_cs_shdr_intrl_tbl"):
            *pValue = PipelineSymbolType::CsShdrIntrlTblPtr;
            break;
        case HashLiteralString("_amdgpu_ls_disasm"):
            *pValue = PipelineSymbolType::LsDisassembly;
            break;
        case HashLiteralString("_amdgpu_hs_disasm"):
            *pValue = PipelineSymbolType::HsDisassembly;
            break;
        case HashLiteralString("_amdgpu_es_disasm"):
            *pValue = PipelineSymbolType::EsDisassembly;
            break;
        case HashLiteralString("_amdgpu_gs_disasm"):
            *pValue = PipelineSymbolType::GsDisassembly;
            break;
        case HashLiteralString("_amdgpu_vs_disasm"):
            *pValue = PipelineSymbolType::VsDisassembly;
            break;
        case HashLiteralString("_amdgpu_ps_disasm"):
            *pValue = PipelineSymbolType::PsDisassembly;
            break;
        case HashLiteralString("_amdgpu_cs_disasm"):
            *pValue = PipelineSymbolType::CsDisassembly;
            break;
        case HashLiteralString("_amdgpu_ls_shdr_intrl_data"):
            *pValue = PipelineSymbolType::LsShdrIntrlData;
            break;
        case HashLiteralString("_amdgpu_hs_shdr_intrl_data"):
            *pValue = PipelineSymbolType::HsShdrIntrlData;
            break;
        case HashLiteralString("_amdgpu_es_shdr_intrl_data"):
            *pValue = PipelineSymbolType::EsShdrIntrlData;
            break;
        case HashLiteralString("_amdgpu_gs_shdr_intrl_data"):
            *pValue = PipelineSymbolType::GsShdrIntrlData;
            break;
        case HashLiteralString("_amdgpu_vs_shdr_intrl_data"):
            *pValue = PipelineSymbolType::VsShdrIntrlData;
            break;
        case HashLiteralString("_amdgpu_ps_shdr_intrl_data"):
            *pValue = PipelineSymbolType::PsShdrIntrlData;
            break;
        case HashLiteralString("_amdgpu_cs_shdr_intrl_data"):
            *pValue = PipelineSymbolType::CsShdrIntrlData;
            break;
        case HashLiteralString("_amdgpu_pipeline_intrl_data"):
            *pValue = PipelineSymbolType::PipelineIntrlData;
            break;
        default:
            result = Result::NotFound;
            break;
        }
    }

    return result;
}

// =====================================================================================================================
PAL_INLINE Result SerializeEnum(
    MsgPackWriter*  pWriter,
    PipelineSymbolType  value)
{
    Result result = Result::ErrorInvalidValue;

    switch (value)
    {
    case PipelineSymbolType::Unknown:
        result = pWriter->Pack("unknown");
        break;
    case PipelineSymbolType::LsMainEntry:
        result = pWriter->Pack("_amdgpu_ls_main");
        break;
    case PipelineSymbolType::HsMainEntry:
        result = pWriter->Pack("_amdgpu_hs_main");
        break;
    case PipelineSymbolType::EsMainEntry:
        result = pWriter->Pack("_amdgpu_es_main");
        break;
    case PipelineSymbolType::GsMainEntry:
        result = pWriter->Pack("_amdgpu_gs_main");
        break;
    case PipelineSymbolType::VsMainEntry:
        result = pWriter->Pack("_amdgpu_vs_main");
        break;
    case PipelineSymbolType::PsMainEntry:
        result = pWriter->Pack("_amdgpu_ps_main");
        break;
    case PipelineSymbolType::CsMainEntry:
        result = pWriter->Pack("_amdgpu_cs_main");
        break;
    case PipelineSymbolType::FsMainEntry:
        result = pWriter->Pack("_amdgpu_fs_main");
        break;
    case PipelineSymbolType::LsShdrIntrlTblPtr:
        result = pWriter->Pack("_amdgpu_ls_shdr_intrl_tbl");
        break;
    case PipelineSymbolType::HsShdrIntrlTblPtr:
        result = pWriter->Pack("_amdgpu_hs_shdr_intrl_tbl");
        break;
    case PipelineSymbolType::EsShdrIntrlTblPtr:
        result = pWriter->Pack("_amdgpu_es_shdr_intrl_tbl");
        break;
    case PipelineSymbolType::GsShdrIntrlTblPtr:
        result = pWriter->Pack("_amdgpu_gs_shdr_intrl_tbl");
        break;
    case PipelineSymbolType::VsShdrIntrlTblPtr:
        result = pWriter->Pack("_amdgpu_vs_shdr_intrl_tbl");
        break;
    case PipelineSymbolType::PsShdrIntrlTblPtr:
        result = pWriter->Pack("_amdgpu_ps_shdr_intrl_tbl");
        break;
    case PipelineSymbolType::CsShdrIntrlTblPtr:
        result = pWriter->Pack("_amdgpu_cs_shdr_intrl_tbl");
        break;
    case PipelineSymbolType::LsDisassembly:
        result = pWriter->Pack("_amdgpu_ls_disasm");
        break;
    case PipelineSymbolType::HsDisassembly:
        result = pWriter->Pack("_amdgpu_hs_disasm");
        break;
    case PipelineSymbolType::EsDisassembly:
        result = pWriter->Pack("_amdgpu_es_disasm");
        break;
    case PipelineSymbolType::GsDisassembly:
        result = pWriter->Pack("_amdgpu_gs_disasm");
        break;
    case PipelineSymbolType::VsDisassembly:
        result = pWriter->Pack("_amdgpu_vs_disasm");
        break;
    case PipelineSymbolType::PsDisassembly:
        result = pWriter->Pack("_amdgpu_ps_disasm");
        break;
    case PipelineSymbolType::CsDisassembly:
        result = pWriter->Pack("_amdgpu_cs_disasm");
        break;
    case PipelineSymbolType::LsShdrIntrlData:
        result = pWriter->Pack("_amdgpu_ls_shdr_intrl_data");
        break;
    case PipelineSymbolType::HsShdrIntrlData:
        result = pWriter->Pack("_amdgpu_hs_shdr_intrl_data");
        break;
    case PipelineSymbolType::EsShdrIntrlData:
        result = pWriter->Pack("_amdgpu_es_shdr_intrl_data");
        break;
    case PipelineSymbolType::GsShdrIntrlData:
        result = pWriter->Pack("_amdgpu_gs_shdr_intrl_data");
        break;
    case PipelineSymbolType::VsShdrIntrlData:
        result = pWriter->Pack("_amdgpu_vs_shdr_intrl_data");
        break;
    case PipelineSymbolType::PsShdrIntrlData:
        result = pWriter->Pack("_amdgpu_ps_shdr_intrl_data");
        break;
    case PipelineSymbolType::CsShdrIntrlData:
        result = pWriter->Pack("_amdgpu_cs_shdr_intrl_data");
        break;
    case PipelineSymbolType::PipelineIntrlData:
        result = pWriter->Pack("_amdgpu_pipeline_intrl_data");
        break;
    default:
        break;
    }

    return result;
}

// =====================================================================================================================
template <typename EnumType>
Result DeserializeEnumBitflags(
    MsgPackReader*  pReader,
    uint32*         pBitflags)
{
    Result result = pReader->Next(CWP_ITEM_ARRAY);

    *pBitflags = 0;

    for (uint32 i = pReader->Get().as.array.size; ((result == Result::Success) && (i > 0)); --i)
    {
        using EnumStorage = typename std::underlying_type<EnumType>::type;
        EnumStorage curEnum = 0;

        result = DeserializeEnum(pReader, reinterpret_cast<EnumType*>(&curEnum));

        if (result == Result::Success)
        {
            PAL_ASSERT(curEnum < (sizeof(*pBitflags) * 8));
            *pBitflags |= (static_cast<EnumStorage>(1) << curEnum);
        }
    }

    return result;
}

// =====================================================================================================================
template <typename EnumType>
Result SerializeEnumBitflags(
    MsgPackWriter*  pWriter,
    uint32          bitflags)
{
    uint32 mask = bitflags;
    Result result = pWriter->DeclareArray(CountSetBits(mask));

    for (uint32 i = 0; ((result == Result::Success) && BitMaskScanForward(&i, mask)); mask &= ~(1 << i))
    {
        result = SerializeEnum(pWriter, static_cast<EnumType>(i));
    }

    return result;
}

// =====================================================================================================================
PAL_INLINE Result DeserializeShaderMetadata(
    MsgPackReader*  pReader,
    ShaderMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        result = pReader->Next(CWP_ITEM_STR);

        if (result == Result::Success)
        {
            const uint32 keyHash = HashString(static_cast<const char*>(pReader->Get().as.str.start),
                                              pReader->Get().as.str.length);

            switch (keyHash)
            {
            case HashLiteralString(ShaderMetadataKey::ApiShaderHash):
                PAL_ASSERT(pMetadata->hasEntry.apiShaderHash == 0);
                result = pReader->UnpackNext(&pMetadata->apiShaderHash);
                pMetadata->hasEntry.apiShaderHash = (result == Result::Success);
                break;

            case HashLiteralString(ShaderMetadataKey::HardwareMapping):
                PAL_ASSERT(pMetadata->hasEntry.hardwareMapping == 0);
                result = DeserializeEnumBitflags<HardwareStage>(pReader, &pMetadata->hardwareMapping);
                pMetadata->hasEntry.hardwareMapping = (result == Result::Success);
                break;

            default:
                result = pReader->Skip(1);
                break;
            }
        }
    }

    return result;
}

// =====================================================================================================================
PAL_INLINE Result DeserializeShaderMetadata(
    MsgPackReader*  pReader,
    ShaderMetadata (*pMetadata)[static_cast<uint32>(ApiShaderType::Count)])
{
    Result result = ((pReader->Type() == CWP_ITEM_MAP) && (pReader->Get().as.map.size <= ArrayLen(*pMetadata))) ?
                    Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        ApiShaderType key = ApiShaderType::Count;
        result = DeserializeEnum(pReader, &key);

        if (result == Result::Success)
        {
            result = pReader->Next();
        }

        if (result == Result::Success)
        {
            result = DeserializeShaderMetadata(
                pReader, &((*pMetadata)[static_cast<uint32>(key)]));
        }
    }

    return result;
}

// =====================================================================================================================
PAL_INLINE Result DeserializeHardwareStageMetadata(
    MsgPackReader*  pReader,
    HardwareStageMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        result = pReader->Next(CWP_ITEM_STR);

        if (result == Result::Success)
        {
            const uint32 keyHash = HashString(static_cast<const char*>(pReader->Get().as.str.start),
                                              pReader->Get().as.str.length);

            switch (keyHash)
            {
            case HashLiteralString(HardwareStageMetadataKey::EntryPoint):
                PAL_ASSERT(pMetadata->hasEntry.entryPoint == 0);
                result = DeserializeEnum(pReader, &pMetadata->entryPoint);
                pMetadata->hasEntry.entryPoint = (result == Result::Success);
                break;

            case HashLiteralString(HardwareStageMetadataKey::ScratchMemorySize):
                PAL_ASSERT(pMetadata->hasEntry.scratchMemorySize == 0);
                result = pReader->UnpackNext(&pMetadata->scratchMemorySize);
                pMetadata->hasEntry.scratchMemorySize = (result == Result::Success);
                break;

            case HashLiteralString(HardwareStageMetadataKey::LdsSize):
                PAL_ASSERT(pMetadata->hasEntry.ldsSize == 0);
                result = pReader->UnpackNext(&pMetadata->ldsSize);
                pMetadata->hasEntry.ldsSize = (result == Result::Success);
                break;

            case HashLiteralString(HardwareStageMetadataKey::PerfDataBufferSize):
                PAL_ASSERT(pMetadata->hasEntry.perfDataBufferSize == 0);
                result = pReader->UnpackNext(&pMetadata->perfDataBufferSize);
                pMetadata->hasEntry.perfDataBufferSize = (result == Result::Success);
                break;

            case HashLiteralString(HardwareStageMetadataKey::VgprCount):
                PAL_ASSERT(pMetadata->hasEntry.vgprCount == 0);
                result = pReader->UnpackNext(&pMetadata->vgprCount);
                pMetadata->hasEntry.vgprCount = (result == Result::Success);
                break;

            case HashLiteralString(HardwareStageMetadataKey::SgprCount):
                PAL_ASSERT(pMetadata->hasEntry.sgprCount == 0);
                result = pReader->UnpackNext(&pMetadata->sgprCount);
                pMetadata->hasEntry.sgprCount = (result == Result::Success);
                break;

            case HashLiteralString(HardwareStageMetadataKey::VgprLimit):
                PAL_ASSERT(pMetadata->hasEntry.vgprLimit == 0);
                result = pReader->UnpackNext(&pMetadata->vgprLimit);
                pMetadata->hasEntry.vgprLimit = (result == Result::Success);
                break;

            case HashLiteralString(HardwareStageMetadataKey::SgprLimit):
                PAL_ASSERT(pMetadata->hasEntry.sgprLimit == 0);
                result = pReader->UnpackNext(&pMetadata->sgprLimit);
                pMetadata->hasEntry.sgprLimit = (result == Result::Success);
                break;

            case HashLiteralString(HardwareStageMetadataKey::ThreadgroupDimensions):
                PAL_ASSERT(pMetadata->hasEntry.threadgroupDimensions == 0);
                result = pReader->UnpackNext(&pMetadata->threadgroupDimensions);
                pMetadata->hasEntry.threadgroupDimensions = (result == Result::Success);
                break;

            case HashLiteralString(HardwareStageMetadataKey::WavefrontSize):
                PAL_ASSERT(pMetadata->hasEntry.wavefrontSize == 0);
                result = pReader->UnpackNext(&pMetadata->wavefrontSize);
                pMetadata->hasEntry.wavefrontSize = (result == Result::Success);
                break;

            case HashLiteralString(HardwareStageMetadataKey::UsesUavs):
            {
                PAL_ASSERT(pMetadata->hasEntry.usesUavs == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.usesUavs = value;
                }
                pMetadata->hasEntry.usesUavs = (result == Result::Success);
                break;
            }

            case HashLiteralString(HardwareStageMetadataKey::UsesRovs):
            {
                PAL_ASSERT(pMetadata->hasEntry.usesRovs == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.usesRovs = value;
                }
                pMetadata->hasEntry.usesRovs = (result == Result::Success);
                break;
            }

            case HashLiteralString(HardwareStageMetadataKey::WritesUavs):
            {
                PAL_ASSERT(pMetadata->hasEntry.writesUavs == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.writesUavs = value;
                }
                pMetadata->hasEntry.writesUavs = (result == Result::Success);
                break;
            }

            case HashLiteralString(HardwareStageMetadataKey::WritesDepth):
            {
                PAL_ASSERT(pMetadata->hasEntry.writesDepth == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.writesDepth = value;
                }
                pMetadata->hasEntry.writesDepth = (result == Result::Success);
                break;
            }

            case HashLiteralString(HardwareStageMetadataKey::UsesAppendConsume):
            {
                PAL_ASSERT(pMetadata->hasEntry.usesAppendConsume == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.usesAppendConsume = value;
                }
                pMetadata->hasEntry.usesAppendConsume = (result == Result::Success);
                break;
            }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 619
            case HashLiteralString(HardwareStageMetadataKey::MaxPrimsPerWave):
                PAL_ASSERT(pMetadata->hasEntry.maxPrimsPerWave == 0);
                result = pReader->UnpackNext(&pMetadata->maxPrimsPerWave);
                pMetadata->hasEntry.maxPrimsPerWave = (result == Result::Success);
                break;

#endif
            case HashLiteralString(HardwareStageMetadataKey::UsesPrimId):
            {
                PAL_ASSERT(pMetadata->hasEntry.usesPrimId == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.usesPrimId = value;
                }
                pMetadata->hasEntry.usesPrimId = (result == Result::Success);
                break;
            }

            default:
                result = pReader->Skip(1);
                break;
            }
        }
    }

    return result;
}

// =====================================================================================================================
PAL_INLINE Result DeserializeHardwareStageMetadata(
    MsgPackReader*  pReader,
    HardwareStageMetadata (*pMetadata)[static_cast<uint32>(HardwareStage::Count)])
{
    Result result = ((pReader->Type() == CWP_ITEM_MAP) && (pReader->Get().as.map.size <= ArrayLen(*pMetadata))) ?
                    Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        HardwareStage key = HardwareStage::Count;
        result = DeserializeEnum(pReader, &key);

        if (result == Result::Success)
        {
            result = pReader->Next();
        }

        if (result == Result::Success)
        {
            result = DeserializeHardwareStageMetadata(
                pReader, &((*pMetadata)[static_cast<uint32>(key)]));
        }
    }

    return result;
}

// =====================================================================================================================
PAL_INLINE Result DeserializePipelineMetadata(
    MsgPackReader*  pReader,
    PipelineMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_ARRAY) ? Result::Success : Result::ErrorInvalidValue;

    if (result == Result::Success)
    {
        PAL_ASSERT(pReader->Get().as.array.size == 1);
        result = pReader->Next(CWP_ITEM_MAP);
    }

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        uint32 keyHash = 0;
        result = pReader->Next(CWP_ITEM_STR);

        if (result == Result::Success)
        {
            keyHash = HashString(static_cast<const char*>(pReader->Get().as.str.start),
                                 pReader->Get().as.str.length);
        }

        if (result == Result::Success)
        {
            switch (keyHash)
            {
            case HashLiteralString(PipelineMetadataKey::Name):
                PAL_ASSERT(pMetadata->hasEntry.name == 0);
                result = pReader->UnpackNext(&pMetadata->name);
                pMetadata->hasEntry.name = (result == Result::Success);
                break;

            case HashLiteralString(PipelineMetadataKey::Type):
                PAL_ASSERT(pMetadata->hasEntry.type == 0);
                result = DeserializeEnum(pReader, &pMetadata->type);
                pMetadata->hasEntry.type = (result == Result::Success);
                break;

            case HashLiteralString(PipelineMetadataKey::InternalPipelineHash):
                PAL_ASSERT(pMetadata->hasEntry.internalPipelineHash == 0);
                result = pReader->UnpackNext(&pMetadata->internalPipelineHash);
                pMetadata->hasEntry.internalPipelineHash = (result == Result::Success);
                break;

            case HashLiteralString(PipelineMetadataKey::Shaders):
                result = pReader->Next();
                if (result == Result::Success)
                {
                    result = DeserializeShaderMetadata(
                        pReader, &pMetadata->shader);
                }
                break;

            case HashLiteralString(PipelineMetadataKey::HardwareStages):
                result = pReader->Next();
                if (result == Result::Success)
                {
                    result = DeserializeHardwareStageMetadata(
                        pReader, &pMetadata->hardwareStage);
                }
                break;

            case HashLiteralString(PipelineMetadataKey::ShaderFunctions):
                PAL_ASSERT(pMetadata->hasEntry.shaderFunctions == 0);
                pMetadata->shaderFunctions = pReader->Tell();
                pMetadata->hasEntry.shaderFunctions = (result == Result::Success);
                result = pReader->Skip(1);
                break;

            case HashLiteralString(PipelineMetadataKey::Registers):
                PAL_ASSERT(pMetadata->hasEntry.registers == 0);
                pMetadata->registers = pReader->Tell();
                pMetadata->hasEntry.registers = (result == Result::Success);
                result = pReader->Skip(1);
                break;

            case HashLiteralString(PipelineMetadataKey::UserDataLimit):
                PAL_ASSERT(pMetadata->hasEntry.userDataLimit == 0);
                result = pReader->UnpackNext(&pMetadata->userDataLimit);
                pMetadata->hasEntry.userDataLimit = (result == Result::Success);
                break;

            case HashLiteralString(PipelineMetadataKey::SpillThreshold):
                PAL_ASSERT(pMetadata->hasEntry.spillThreshold == 0);
                result = pReader->UnpackNext(&pMetadata->spillThreshold);
                pMetadata->hasEntry.spillThreshold = (result == Result::Success);
                break;

            case HashLiteralString(PipelineMetadataKey::UsesViewportArrayIndex):
            {
                PAL_ASSERT(pMetadata->hasEntry.usesViewportArrayIndex == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.usesViewportArrayIndex = value;
                }
                pMetadata->hasEntry.usesViewportArrayIndex = (result == Result::Success);
                break;
            }

            case HashLiteralString(PipelineMetadataKey::EsGsLdsSize):
                PAL_ASSERT(pMetadata->hasEntry.esGsLdsSize == 0);
                result = pReader->UnpackNext(&pMetadata->esGsLdsSize);
                pMetadata->hasEntry.esGsLdsSize = (result == Result::Success);
                break;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 619
            case HashLiteralString(PipelineMetadataKey::StreamOutTableAddress):
                PAL_ASSERT(pMetadata->hasEntry.streamOutTableAddress == 0);
                result = pReader->UnpackNext(&pMetadata->streamOutTableAddress);
                pMetadata->hasEntry.streamOutTableAddress = (result == Result::Success);
                break;

            case HashLiteralString(PipelineMetadataKey::IndirectUserDataTableAddresses):
                PAL_ASSERT(pMetadata->hasEntry.indirectUserDataTableAddresses == 0);
                result = pReader->UnpackNext(&pMetadata->indirectUserDataTableAddresses);
                pMetadata->hasEntry.indirectUserDataTableAddresses = (result == Result::Success);
                break;

#endif
            case HashLiteralString(PipelineMetadataKey::NggSubgroupSize):
                PAL_ASSERT(pMetadata->hasEntry.nggSubgroupSize == 0);
                result = pReader->UnpackNext(&pMetadata->nggSubgroupSize);
                pMetadata->hasEntry.nggSubgroupSize = (result == Result::Success);
                break;

            case HashLiteralString(PipelineMetadataKey::NumInterpolants):
                PAL_ASSERT(pMetadata->hasEntry.numInterpolants == 0);
                result = pReader->UnpackNext(&pMetadata->numInterpolants);
                pMetadata->hasEntry.numInterpolants = (result == Result::Success);
                break;

            case HashLiteralString(PipelineMetadataKey::MeshScratchMemorySize):
                PAL_ASSERT(pMetadata->hasEntry.meshScratchMemorySize == 0);
                result = pReader->UnpackNext(&pMetadata->meshScratchMemorySize);
                pMetadata->hasEntry.meshScratchMemorySize = (result == Result::Success);
                break;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 619
            case HashLiteralString(PipelineMetadataKey::CalcWaveBreakSizeAtDrawTime):
            {
                PAL_ASSERT(pMetadata->hasEntry.calcWaveBreakSizeAtDrawTime == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.calcWaveBreakSizeAtDrawTime = value;
                }
                pMetadata->hasEntry.calcWaveBreakSizeAtDrawTime = (result == Result::Success);
                break;
            }

#endif
            case HashLiteralString(PipelineMetadataKey::Api):
                PAL_ASSERT(pMetadata->hasEntry.api == 0);
                result = pReader->UnpackNext(&pMetadata->api);
                pMetadata->hasEntry.api = (result == Result::Success);
                break;

            case HashLiteralString(PipelineMetadataKey::ApiCreateInfo):
                PAL_ASSERT(pMetadata->hasEntry.apiCreateInfo == 0);
                result = pReader->Next();

                if (result == Result::Success)
                {
                    result = pReader->Unpack(&pMetadata->apiCreateInfo.pBuffer,
                                             &pMetadata->apiCreateInfo.sizeInBytes);
                }
                pMetadata->hasEntry.apiCreateInfo = (result == Result::Success);
                break;

            default:
                result = pReader->Skip(1);
                break;
            }
        }
    }

    return result;
}

// =====================================================================================================================
PAL_INLINE Result DeserializePalCodeObjectMetadata(
    MsgPackReader*  pReader,
    PalCodeObjectMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        result = pReader->Next(CWP_ITEM_STR);

        if (result == Result::Success)
        {
            const uint32 keyHash = HashString(static_cast<const char*>(pReader->Get().as.str.start),
                                              pReader->Get().as.str.length);

            switch (keyHash)
            {
            case HashLiteralString(PalCodeObjectMetadataKey::Version):
                PAL_ASSERT(pMetadata->hasEntry.version == 0);
                result = pReader->UnpackNext(&pMetadata->version);
                pMetadata->hasEntry.version = (result == Result::Success);
                break;

            case HashLiteralString(PalCodeObjectMetadataKey::Pipelines):
                result = pReader->Next();
                if (result == Result::Success)
                {
                    result = DeserializePipelineMetadata(
                        pReader, &pMetadata->pipeline);
                }
                break;

            default:
                result = pReader->Skip(1);
                break;
            }
        }
    }

    return result;
}

} // Metadata

} // Abi
} // Util
